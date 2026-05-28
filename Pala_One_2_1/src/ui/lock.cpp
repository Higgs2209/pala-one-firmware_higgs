#include "src/ui/lock.h"

#include "src/state.h"   // prefs

namespace Lock {

static constexpr const char* kKey = "cfg_locked";

static bool s_locked = false;

// cppcheck-suppress unusedFunction
void loadSettings() {
  s_locked = prefs.getBool(kKey, false);
}

// cppcheck-suppress unusedFunction
bool isLocked() { return s_locked; }

// cppcheck-suppress unusedFunction
void engage() {
  if (s_locked) return;
  s_locked = true;
  prefs.putBool(kKey, true);
}

// cppcheck-suppress unusedFunction
void disengage() {
  if (!s_locked) return;
  s_locked = false;
  prefs.putBool(kKey, false);
}

// cppcheck-suppress unusedFunction
bool isUnlockGesture(const ButtonEvent& ev) {
  return ev.kind == ButtonEvent::Long
      || ev.kind == ButtonEvent::VeryLong
      || ev.kind == ButtonEvent::ClickHold;
}

}  // namespace Lock
