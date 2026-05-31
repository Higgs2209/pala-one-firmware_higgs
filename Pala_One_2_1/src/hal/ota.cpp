#include "src/hal/ota.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Built-in Mozilla root bundle embedded in libmbedtls.a by the IDF.
// The asm labels are the linker-generated symbols for the binary section;
// setCACertBundle requires both start pointer and byte length (arduino-esp32 >= 3.0.4).
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

static constexpr const char* kOtaBaseUrl =
    "https://paullagier.github.io/pala-one-firmware/";

bool OTA::isUpdateServerReachable() {
  WiFiClientSecure client;
  client.setCACertBundle(x509_crt_imported_bundle_bin_start,
                         x509_crt_imported_bundle_bin_end -
                         x509_crt_imported_bundle_bin_start);
  HTTPClient http;
  http.begin(client, kOtaBaseUrl);
  http.setTimeout(5000);
  int code = http.sendRequest("HEAD");
  http.end();
  return code > 0 && code < 500;
}
