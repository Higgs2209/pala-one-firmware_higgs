#include "src/ui/screens/update_screen.h"

#include "src/config.h"
#include "src/hal/display.h"
#include "src/hal/input.h"
#include "src/hal/ota.h"
#include "src/hal/wifi.h"
#include "src/hal/wifi_provisioning.h"
#include "src/state.h"
#include "src/ui/font.h"
#include "src/ui/screens/library_screen.h"
#include "src/ui/widgets.h"

static constexpr uint32_t kStaTimeoutMs  = 5000;
static constexpr const char* kKeyOtaChannel = "cfg_ota_channel";

// ----------------------------------------------------------------------------
//  NVS helpers
// ----------------------------------------------------------------------------
void UpdateScreen::loadChannel() {
  stableChan_ = (prefs.getString(kKeyOtaChannel, "stable") == "stable");
}

void UpdateScreen::saveChannel() {
  prefs.putString(kKeyOtaChannel, stableChan_ ? "stable" : "dev");
}

// ----------------------------------------------------------------------------
//  Exit
// ----------------------------------------------------------------------------
void UpdateScreen::exitToLibrary() {
  wifiEnd();
  WifiProvisioning::notifyUploadSession(false);
  nextScreen = &g_libraryScreen;
}

// ----------------------------------------------------------------------------
//  Lifecycle
// ----------------------------------------------------------------------------
void UpdateScreen::onEnter() {
  loadChannel();
  focusItem_ = 0;
  WifiProvisioning::notifyUploadSession(true);

  if (wifiStaBegin()) {
    phase_      = Phase::Connecting;
    staStartMs_ = millis();
  } else {
    phase_ = Phase::ConnFailed;
    WifiProvisioning::notifyUploadSession(false);
  }
  draw();
}

void UpdateScreen::onIdleTick() {
  if (phase_ != Phase::Connecting) return;

  WifiStaResult r = wifiStaPoll(net_);
  if (r == WifiStaResult::Connected) {
    phase_ = Phase::Idle;
    draw();
    return;
  }
  if (r == WifiStaResult::Failed ||
      (uint32_t)(millis() - staStartMs_) > kStaTimeoutMs) {
    wifiEnd();
    WifiProvisioning::notifyUploadSession(false);
    phase_ = Phase::ConnFailed;
    draw();
  }
}

// ----------------------------------------------------------------------------
//  Drawing
// ----------------------------------------------------------------------------
void UpdateScreen::draw() {
  prepareMenuFrame();
  Font::useBody();
  int y = drawSectionHeader(D_UPDATE_HEADER);

  if (phase_ == Phase::Connecting) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_CONNECTING);
    display.update();
    return;
  }

  if (phase_ == Phase::ConnFailed) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_CONN_FAILED);
    display.update();
    return;
  }

  if (phase_ == Phase::Checking) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_CHECKING);
    display.update();
    return;
  }

  // Phase::Idle, ServerFail, ServerOk — full UI with cursor
  Font::useBody();
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(D_UPDATE_VERSION_PREFIX FW_VERSION);
  y += 16;

  // Channel checkboxes — label of focused item is bold
  int cx = MARGIN_X;

  Font::useBody();
  u8g2.setCursor(cx, y);
  u8g2.print(stableChan_ ? "[x] " : "[ ] ");
  cx += u8g2.getUTF8Width("[x] ");
  if (focusItem_ == 0) Font::useBold(); else Font::useBody();
  u8g2.setCursor(cx, y);
  u8g2.print(D_UPDATE_CHAN_STABLE);
  cx += u8g2.getUTF8Width(D_UPDATE_CHAN_STABLE);
  Font::useBody();
  cx += u8g2.getUTF8Width("   ");

  u8g2.setCursor(cx, y);
  u8g2.print(!stableChan_ ? "[x] " : "[ ] ");
  cx += u8g2.getUTF8Width("[x] ");
  if (focusItem_ == 1) Font::useBold(); else Font::useBody();
  u8g2.setCursor(cx, y);
  u8g2.print(D_UPDATE_CHAN_DEV);
  y += 16;

  if (!stableChan_) {
    Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_DEV_WARNING);
    y += 14;
  }
  y += 6;

  // Probe result line
  if (phase_ == Phase::ServerFail) {
    Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_SERVER_FAIL);
    y += 14;
  } else if (phase_ == Phase::ServerOk) {
    Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_SERVER_OK);
    y += 14;
  }

  if (focusItem_ == 2) Font::useBold(); else Font::useBody();
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(D_UPDATE_BTN_CHECK);

  display.update();
}

// ----------------------------------------------------------------------------
//  Input
// ----------------------------------------------------------------------------
void UpdateScreen::onButton(const ButtonEvent& e) {
  if (!e.any()) return;

  if (e.kind == ButtonEvent::Triple) {
    exitToLibrary();
    return;
  }

  // Cursor navigation and activation — available when full UI is visible
  if (phase_ == Phase::Idle || phase_ == Phase::ServerFail ||
      phase_ == Phase::ServerOk) {

    if (e.kind == ButtonEvent::Short) {
      focusItem_ = (focusItem_ + 1) % 3;
      draw();
      return;
    }

    if (e.kind == ButtonEvent::Double) {
      if (focusItem_ == 0) {
        stableChan_ = true;
        saveChannel();
        draw();
      } else if (focusItem_ == 1) {
        stableChan_ = false;
        saveChannel();
        draw();
      } else {
        phase_ = Phase::Checking;
        draw();
        phase_ = OTA::isUpdateServerReachable() ? Phase::ServerOk : Phase::ServerFail;
        clearButtonQueue();  // discard any presses that queued during the blocking probe
        draw();
      }
      return;
    }
  }
}
