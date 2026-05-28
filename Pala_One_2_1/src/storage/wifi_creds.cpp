#include "src/storage/wifi_creds.h"

#include "src/state.h"  // prefs

namespace WifiCreds {

static constexpr const char* kKeySsid = "wifi_ssid";
static constexpr const char* kKeyPass = "wifi_pass";

// cppcheck-suppress unusedFunction
bool has() {
  return prefs.getString(kKeySsid, "").length() > 0;
}

// cppcheck-suppress unusedFunction
String ssid() { return prefs.getString(kKeySsid, ""); }
// cppcheck-suppress unusedFunction
String pass() { return prefs.getString(kKeyPass, ""); }

// cppcheck-suppress unusedFunction
void save(const String& ssid, const String& pass) {
  prefs.putString(kKeySsid, ssid);
  prefs.putString(kKeyPass, pass);
}

// cppcheck-suppress unusedFunction
void clear() {
  prefs.remove(kKeySsid);
  prefs.remove(kKeyPass);
}

}  // namespace WifiCreds
