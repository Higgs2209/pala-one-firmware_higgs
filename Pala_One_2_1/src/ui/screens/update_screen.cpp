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

static constexpr uint32_t    kStaTimeoutMs  = 5000;
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
  remoteVersion_ = "";
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

  // ---- Single-line transient states ----------------------------------------
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

  // ---- Full UI (Idle, ServerFail, UpToDate, UpdateAvailable) ---------------

  // Clamp focus to valid range for the current phase
  int maxItems = (phase_ == Phase::UpdateAvailable) ? 4 : 3;
  if (focusItem_ >= maxItems) focusItem_ = 0;

  // Version line
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

  // Dev warning
  if (!stableChan_) {
    Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_DEV_WARNING);
    y += 14;
  }
  y += 6;

  // Status line
  Font::useBody();
  if (phase_ == Phase::ServerFail) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_SERVER_FAIL);
    y += 14;
  } else if (phase_ == Phase::UpToDate) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_UP_TO_DATE);
    y += 14;
  } else if (phase_ == Phase::UpdateAvailable) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_AVAILABLE_PREFIX);
    u8g2.print(remoteVersion_.c_str());
    y += 14;
  }
  y += 2;

  // Check button
  if (focusItem_ == 2) Font::useBold(); else Font::useBody();
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(D_UPDATE_BTN_CHECK);
  y += 14;

  // Install button — only when an update is available
  if (phase_ == Phase::UpdateAvailable) {
    if (focusItem_ == 3) Font::useBold(); else Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_BTN_INSTALL);
  }

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

  // Full UI phases only
  if (phase_ != Phase::Idle && phase_ != Phase::ServerFail &&
      phase_ != Phase::UpToDate && phase_ != Phase::UpdateAvailable) return;

  if (e.kind == ButtonEvent::Short) {
    int maxItems = (phase_ == Phase::UpdateAvailable) ? 4 : 3;
    focusItem_   = (focusItem_ + 1) % maxItems;
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
    } else if (focusItem_ == 2) {
      // Check: probe + manifest fetch in one blocking sequence
      phase_ = Phase::Checking;
      draw();
      if (OTA::isUpdateServerReachable()) {
        OtaCheckResult r = OTA::checkAvailable(stableChan_ ? "stable" : "dev");
        if (r.updateAvailable) {
          remoteVersion_ = r.remoteVersion;
          phase_         = Phase::UpdateAvailable;
        } else if (r.remoteVersion.length() > 0) {
          phase_ = Phase::UpToDate;
        } else {
          phase_ = Phase::ServerFail;  // manifest fetch failed
        }
      } else {
        phase_ = Phase::ServerFail;
      }
      clearButtonQueue();
      draw();
    } else if (focusItem_ == 3) {
      // Install — download + flash, implemented in next step
    }
    return;
  }
}
