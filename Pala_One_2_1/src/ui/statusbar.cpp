#include "src/ui/statusbar.h"

#include "src/config.h"   // STATUS_H
#include "src/state.h"    // prefs
#include "src/ui/font.h"  // Font::invalidateLayoutCache

namespace Statusbar {

static constexpr const char* kKey = "cfg_statusbar";

static Mode s_mode = Full;

static Mode clamp(int v) {
  if (v < Full || v > Hidden) return Full;
  return (Mode)v;
}

void loadSettings() {
  s_mode = clamp(prefs.getInt(kKey, Full));
}

Mode mode() { return s_mode; }

int reserveH() {
  switch (s_mode) {
    case Hidden:  return 0;
    case Minimal: return 1;
    case Full:
    default:      return STATUS_H;
  }
}

void setMode(Mode m) {
  Mode nm = clamp(m);
  if (nm == s_mode) return;
  s_mode = nm;
  prefs.putInt(kKey, (int)s_mode);
  Font::invalidateLayoutCache();
}

void cycleMode() {
  Mode next = (Mode)((int)s_mode + 1);
  if (next > Hidden) next = Full;
  setMode(next);
}

}  // namespace Statusbar
