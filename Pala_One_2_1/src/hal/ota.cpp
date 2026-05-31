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
  client.setCACertBundle(x509_crt_imported_bundle_bin_start,
                         x509_crt_imported_bundle_bin_end -
                         x509_crt_imported_bundle_bin_start);
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

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();

  if (err) return result;

  result.remoteVersion   = doc["version"].as<String>();
  result.updateAvailable = result.remoteVersion.length() > 0 &&
                           result.remoteVersion != String(FW_VERSION);
  return result;
}

// ----------------------------------------------------------------------------

OtaDownloadResult OTA::download(const String& channel, OtaProgressFn progressCb) {
  OtaDownloadResult result;

  String url = String(kOtaBaseUrl) + channel + "/firmware-"
               + kBoardToken + "-" + kLangToken + ".bin";

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

  int contentLength = http.getSize();
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

  WiFiClient* stream = http.getStreamPtr();
  static uint8_t buf[4096];  // static: keeps off the loopTask stack
  int written   = 0;
  int remaining = contentLength;  // -1 = unknown (chunked / no Content-Length)
  int lastPct   = -1;

  while (http.connected() || stream->available() > 0) {
    int avail = stream->available();
    if (avail > 0) {
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
        if (remaining == 0) break;
      }
      if (progressCb && contentLength > 0) {
        int pct = (int)((int64_t)written * 100 / contentLength);
        if (pct != lastPct) {
          lastPct = pct;
          progressCb(pct);
        }
      }
    } else {
      delay(1);
    }
  }

  http.end();

  if (esp_ota_end(handle) != ESP_OK) return result;
  if (esp_ota_set_boot_partition(target) != ESP_OK) return result;

  result.success = true;
  return result;
}
