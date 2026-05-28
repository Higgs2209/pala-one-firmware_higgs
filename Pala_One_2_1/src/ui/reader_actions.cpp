#include "src/ui/reader_actions.h"

#include "src/state.h"  // prefs

namespace Gestures {

static constexpr const char* kKeyLong      = "cfg_btnL";
static constexpr const char* kKeyExtraLong = "cfg_btnXL";
static constexpr const char* kKeyClickHold = "cfg_btnCH";

// Defaults chosen to make the device useful out of the box:
//   long      = bookmark — the most common action while reading
//   extralong = lock     — a deliberate "I'm putting it down" gesture
//   clickhold = menu     — easy chord, doesn't fight short-click paging
static ButtonAction s_long      = ACTION_BOOKMARK;
static ButtonAction s_extraLong = ACTION_LOCK;
static ButtonAction s_clickHold = ACTION_MENU;

static ButtonAction clamp(int v) {
  if (v < ACTION_NONE || v > ACTION_MENU) return ACTION_NONE;
  return (ButtonAction)v;
}

// cppcheck-suppress unusedFunction
void loadSettings() {
  s_long      = clamp(prefs.getInt(kKeyLong,      ACTION_BOOKMARK));
  s_extraLong = clamp(prefs.getInt(kKeyExtraLong, ACTION_LOCK));
  s_clickHold = clamp(prefs.getInt(kKeyClickHold, ACTION_MENU));
}

// cppcheck-suppress unusedFunction
ButtonAction actionLong()      { return s_long; }
// cppcheck-suppress unusedFunction
ButtonAction actionExtraLong() { return s_extraLong; }
// cppcheck-suppress unusedFunction
ButtonAction actionClickHold() { return s_clickHold; }

static void persist(const char* key, ButtonAction& dest, ButtonAction value) {
  ButtonAction v = clamp(value);
  if (v == dest) return;
  dest = v;
  prefs.putInt(key, (int)v);
}

// cppcheck-suppress unusedFunction
void setActionLong(ButtonAction a)      { persist(kKeyLong,      s_long,      a); }
// cppcheck-suppress unusedFunction
void setActionExtraLong(ButtonAction a) { persist(kKeyExtraLong, s_extraLong, a); }
// cppcheck-suppress unusedFunction
void setActionClickHold(ButtonAction a) { persist(kKeyClickHold, s_clickHold, a); }

// cppcheck-suppress unusedFunction
ButtonAction actionFor(ButtonEvent::Kind kind) {
  switch (kind) {
    case ButtonEvent::Long:      return s_long;
    case ButtonEvent::VeryLong:  return s_extraLong;
    case ButtonEvent::ClickHold: return s_clickHold;
    default:                     return ACTION_NONE;
  }
}

}  // namespace Gestures
