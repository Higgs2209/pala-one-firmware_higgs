#ifndef PALA_HAL_OTA_H
#define PALA_HAL_OTA_H

class OTA {
public:
  // HTTPS HEAD probe to the firmware distribution root.
  // Returns true on any non-5xx response, false on DNS/TLS/timeout/5xx.
  // Blocks for up to 5 s. Call only when Wi-Fi STA is connected.
  static bool isUpdateServerReachable();
};

#endif  // PALA_HAL_OTA_H
