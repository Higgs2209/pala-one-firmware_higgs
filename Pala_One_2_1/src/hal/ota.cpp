#include "src/hal/ota.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>

#include "src/config.h"

// Built-in Mozilla root bundle embedded in libmbedtls.a by the IDF.
// setCACertBundle requires both start pointer and byte length (arduino-esp32 >= 3.0.4).
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

static constexpr const char* kOtaBaseUrl =
    "https://paullagier.github.io/pala-one-firmware/";

// How long to wait for the next data chunk before aborting the download.
// Distinct from http.setTimeout() which covers initial connection + each
// individual read() call, not the total silence between reads in our loop.
static constexpr uint32_t kDownloadStallMs = 30000;

#if defined(DISPLAY_V1_2)
  static constexpr const char* kBoardToken = "v1_2";
#else
  static constexpr const char* kBoardToken = "v1_1";
#endif

#if defined(LANG_ES_LA)
  static constexpr const char* kLangToken = "es";
#else
  static constexpr const char* kLangToken = "en";
#endif

// ----------------------------------------------------------------------------

static void configureClient(WiFiClientSecure& client) {
  // Use uintptr_t arithmetic instead of pointer subtraction: the two symbols
  // are linker-generated section markers guaranteed to be contiguous, but
  // cppcheck cannot verify that and would flag a comparePointers error.
  size_t len = (size_t)((uintptr_t)x509_crt_imported_bundle_bin_end
                       - (uintptr_t)x509_crt_imported_bundle_bin_start);
  client.setCACertBundle(x509_crt_imported_bundle_bin_start, len);
}

// ----------------------------------------------------------------------------

bool OTA::isUpdateServerReachable() {
  WiFiClientSecure client;
  configureClient(client);
  HTTPClient http;
  http.begin(client, kOtaBaseUrl);
  http.setTimeout(5000);
  int code = http.sendRequest("HEAD");
  http.end();
  return code > 0 && code < 500;
}

// ----------------------------------------------------------------------------

OtaCheckResult OTA::checkAvailable(const String& channel) {
  OtaCheckResult result;

  String url = String(kOtaBaseUrl) + channel + "/manifest-"
               + kBoardToken + "-" + kLangToken + ".json";

  WiFiClientSecure client;
  configureClient(client);
  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(5000);

  if (http.GET() != HTTP_CODE_OK) {
    http.end();
    return result;
  }

  // Stream-parse directly — avoids allocating the full body as a String.
  // ArduinoJson reads only what it needs; extra bytes are left in the buffer
  // and discarded when http.end() closes the connection.
  WiFiClient* stream = http.getStreamPtr();
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, *stream);
  http.end();

  if (err) return result;

  const char* ver = doc["version"];
  if (!ver || ver[0] == '\0') return result;

  result.remoteVersion   = ver;
  result.updateAvailable = result.remoteVersion != String(FW_VERSION);
  return result;
}

// ----------------------------------------------------------------------------

OtaDownloadResult OTA::download(const String& channel, OtaProgressFn progressCb) {
  OtaDownloadResult result;

  String url = String(kOtaBaseUrl) + channel + "/firmware-"
               + kBoardToken + "-" + kLangToken + ".bin";

  // Resolve the idle OTA partition — fails if the partition table has no OTA
  // slots (e.g. device still on the old factory layout).
  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  if (!target) return result;

  WiFiClientSecure client;
  configureClient(client);
  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(30000);

  if (http.GET() != HTTP_CODE_OK) {
    http.end();
    return result;
  }

  int contentLength = http.getSize();  // -1 = chunked / unknown

  // Reject before writing a single byte if the reported size already exceeds
  // the partition. Unknown size (-1) passes here and is caught by esp_ota_end
  // if the image turns out to be too large.
  if (contentLength > 0 && (size_t)contentLength > target->size) {
    http.end();
    result.partitionTooSmall = true;
    return result;
  }

  esp_ota_handle_t handle;
  if (esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK) {
    http.end();
    return result;
  }

  WiFiClient* stream  = http.getStreamPtr();
  // Static buffer keeps 4 KB off the loopTask stack. Safe: download() is
  // never called re-entrantly — it blocks the main loop for its duration.
  static uint8_t buf[4096];
  int  written     = 0;
  int  remaining   = contentLength;
  int  lastPct     = -1;
  uint32_t lastDataMs = millis();

  while (http.connected() || stream->available() > 0) {
    int avail = stream->available();

    if (avail > 0) {
      lastDataMs = millis();  // reset stall timer on every arriving byte
      int toRead = min(avail, (int)sizeof(buf));
      if (remaining > 0) toRead = min(toRead, remaining);
      int len = stream->readBytes(buf, toRead);
      if (len <= 0) break;

      if (esp_ota_write(handle, buf, len) != ESP_OK) {
        esp_ota_abort(handle);
        http.end();
        return result;
      }
      written += len;
      if (remaining > 0) {
        remaining -= len;
        if (remaining == 0) break;  // received all declared bytes
      }
      if (progressCb && contentLength > 0) {
        int pct = (int)((int64_t)written * 100 / contentLength);
        if (pct != lastPct) {
          lastPct = pct;
          progressCb(pct);
        }
      }
    } else {
      // No bytes available yet — check for a hung connection.
      if ((uint32_t)(millis() - lastDataMs) > kDownloadStallMs) {
        esp_ota_abort(handle);
        http.end();
        return result;
      }
      delay(1);
    }
  }

  http.end();

  // esp_ota_end validates the image (descriptor, SHA-256 if secure boot is
  // enabled). A truncated or corrupted binary will be rejected here. The
  // handle is always freed by esp_ota_end regardless of the return value,
  // so no separate abort is needed on failure.
  if (esp_ota_end(handle) != ESP_OK) return result;

  // Only mark the new partition as bootable after a clean validation.
  // If this call fails the device stays on the current firmware.
  if (esp_ota_set_boot_partition(target) != ESP_OK) return result;

  result.success = true;
  return result;
}
