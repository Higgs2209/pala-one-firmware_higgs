#ifndef PALA_HAL_OTA_H
#define PALA_HAL_OTA_H

#include <Arduino.h>

struct OtaCheckResult {
  bool   updateAvailable = false;
  String remoteVersion;
};

class OTA {
public:
  // HTTPS HEAD probe to the firmware distribution root.
  // Returns true on any non-5xx response, false on DNS/TLS/timeout/5xx.
  // Blocks for up to 5 s. Call only when Wi-Fi STA is connected.
  static bool isUpdateServerReachable();

  // Fetches the manifest for the given channel and compares its version
  // against the running FW_VERSION. Board and language tokens are resolved
  // from build-time defines. Blocks for up to 5 s.
  static OtaCheckResult checkAvailable(const String& channel);
};

#endif  // PALA_HAL_OTA_H
