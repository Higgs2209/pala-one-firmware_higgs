#include "src/hal/ota.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "src/config.h"

// Built-in Mozilla root bundle embedded in libmbedtls.a by the IDF.
// setCACertBundle requires both start pointer and byte length (arduino-esp32 >= 3.0.4).
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

static constexpr const char* kOtaBaseUrl =
    "https://paullagier.github.io/pala-one-firmware/";

// Board and language tokens resolved at compile time from build flags.
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

static WiFiClientSecure& secureClient() {
  static WiFiClientSecure client;
  client.setCACertBundle(x509_crt_imported_bundle_bin_start,
                         x509_crt_imported_bundle_bin_end -
                         x509_crt_imported_bundle_bin_start);
  return client;
}

bool OTA::isUpdateServerReachable() {
  WiFiClientSecure& client = secureClient();
  HTTPClient http;
  http.begin(client, kOtaBaseUrl);
  http.setTimeout(5000);
  int code = http.sendRequest("HEAD");
  http.end();
  return code > 0 && code < 500;
}

OtaCheckResult OTA::checkAvailable(const String& channel) {
  OtaCheckResult result;

  String url = String(kOtaBaseUrl) + channel + "/manifest-"
               + kBoardToken + "-" + kLangToken + ".json";

  WiFiClientSecure& client = secureClient();
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
