#include "src/ui/icons.h"

#include "src/config.h"
#include "src/state.h"              // prefs
#include "src/hal/battery.h"        // batteryTopRightLeftEdge
#include "src/hal/display.h"        // gfx
#include "src/hal/wifi.h"           // wifiActive
#include "src/ui/sleep.h"           // Sleep::inhibited

namespace Icons {

static bool s_wifiIcon  = true;
static bool s_sleepIcon = true;
static constexpr const char* kKeyWifiIcon  = "cfg_ico_wifi";
static constexpr const char* kKeySleepIcon = "cfg_ico_sleep";

void loadSettings() {
  s_wifiIcon  = prefs.getBool(kKeyWifiIcon,  true);
  s_sleepIcon = prefs.getBool(kKeySleepIcon, true);
}

bool wifiIconEnabled()  { return s_wifiIcon; }
bool sleepIconEnabled() { return s_sleepIcon; }

void setWifiIconEnabled(bool val) {
  s_wifiIcon = val;
  prefs.putBool(kKeyWifiIcon, val);
}

void setSleepIconEnabled(bool val) {
  s_sleepIcon = val;
  prefs.putBool(kKeySleepIcon, val);
}

// ============================================================================
//  Glyphs
//
//  XBM: LSB-first within each byte, every row padded to a whole byte.
//  `drawXBitmap` paints only the set bits, leaving clear bits untouched.
// ============================================================================

static const int kIconH   = 9;   // matches the battery so the band is uniform
static const int kIconGap = 3;
static const int kTrayY   = 2;   // matches drawBatteryTopRight's yIcon
static const int kMaxIcons = 4;

// 11x9. Three arcs of decreasing width separated by blank rows — the gaps
// are what make it legible at this size — over a 3x2 base dot.
static const int kWifiW = 11;
static const unsigned char kWifiBits[] PROGMEM = {
  0xFC, 0x01,   // ..#######..
  0x02, 0x02,   // .#.......#.
  0x01, 0x04,   // #.........#
  0x00, 0x00,
  0xF8, 0x00,   // ...#####...
  0x04, 0x01,   // ..#.....#..
  0x00, 0x00,
  0x70, 0x00,   // ....###....
  0x70, 0x00,   // ....###....
};

// 9x9. Crossed-out moon: a crescent opening to the right, struck through by
// a horizontal line. The blank row above the strike is deliberate — it keeps
// the strike from merging into the upper horn, which is what makes the shape
// still read as a moon rather than a solid blob at this size.
//
// Rows are two bytes because XBM pads to whole bytes, so bits 1-7 of the
// second byte are unused padding, not drawable columns.
static const int kNoSleepW = 9;
static const unsigned char kNoSleepBits[] PROGMEM = {
  0x00, 0x00,   // .........
  0xF0, 0x00,   // ....####.   upper horn
  0x38, 0x00,   // ...###...
  0x1C, 0x00,   // ..###....
  0x00, 0x00,   // .........   separating channel
  0xFF, 0x01,   // #########   strike
  0x1C, 0x00,   // ..###....
  0x38, 0x00,   // ...###...
  0xF0, 0x00,   // ....####.   lower horn
};

static void drawWifiGlyph(int x, int y) {
  gfx.drawXBitmap(x, y, kWifiBits, kWifiW, kIconH, 1);
}

static void drawNoSleepGlyph(int x, int y) {
  gfx.drawXBitmap(x, y, kNoSleepBits, kNoSleepW, kIconH, 1);
}

// ============================================================================
//  Tray
// ============================================================================

// Which icons want to be on screen right now, as a bitmask. Kept separate
// from the drawing so `trayStateChanged` costs nothing to evaluate.
static const int kMaskWifi  = 1 << 0;
static const int kMaskSleep = 1 << 1;

static int activeMask() {
  int mask = 0;
  if (s_wifiIcon && wifiActive()) mask |= kMaskWifi;
  if (s_sleepIcon && ENABLE_DEEP_SLEEP && Sleep::inhibited()) mask |= kMaskSleep;
  return mask;
}

int trayRightEdge(bool batteryDrawn) {
  return batteryDrawn ? batteryTopRightLeftEdge() - kIconGap
                      : SCREEN_W - MARGIN_X;
}

int drawStatusTray(int rightEdge) {
  struct Item {
    void (*draw)(int x, int y);
    int  w;
  };

  Item items[kMaxIcons] = {};
  int n = 0;
  const int mask = activeMask();
  if (mask & kMaskWifi)  items[n++] = { drawWifiGlyph,    kWifiW };
  if (mask & kMaskSleep) items[n++] = { drawNoSleepGlyph, kNoSleepW };

  int x = rightEdge;
  for (int i = 0; i < n; i++) {
    x -= items[i].w;
    // Self-clear so the tray is safe for callers that draw a header without
    // clearing the canvas first — the Pala apps API exposes drawHeader and
    // clearScreen as independent calls.
    gfx.fillRect(x, kTrayY, items[i].w, kIconH, 0);
    items[i].draw(x, kTrayY);
    x -= kIconGap;
  }

  return (n > 0) ? x + kIconGap : rightEdge;
}

bool trayStateChanged() {
  static int s_lastMask = 0;
  const int mask = activeMask();
  if (mask == s_lastMask) return false;
  s_lastMask = mask;
  return true;
}

}  // namespace Icons
