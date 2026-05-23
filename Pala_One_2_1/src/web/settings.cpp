#include "src/web/settings.h"

#include "src/config.h"
#include "src/state.h"
#include "src/ui/font.h"
#include "src/ui/sleep.h"
#include "src/web/chrome.h"

static void handleSettings() {
  int curFont = Font::currentBodySize();
  String sel8  = (curFont == 8)  ? " selected" : "";
  String sel10 = (curFont == 10) ? " selected" : "";
  String sel12 = (curFont == 12) ? " selected" : "";
  String sel14 = (curFont == 14) ? " selected" : "";

  int curSleep = Sleep::idleTimeoutSecs();
  String ss30   = (curSleep == 30)   ? " selected" : "";
  String ss60   = (curSleep == 60)   ? " selected" : "";
  String ss120  = (curSleep == 120)  ? " selected" : "";
  String ss300  = (curSleep == 300)  ? " selected" : "";
  String ss600  = (curSleep == 600)  ? " selected" : "";
  String ss1800 = (curSleep == 1800) ? " selected" : "";

  int curGap = Font::currentLineGap();
  String lg0 = (curGap == 0) ? " selected" : "";
  String lg1 = (curGap == 1) ? " selected" : "";
  String lg2 = (curGap == 2) ? " selected" : "";
  String lg3 = (curGap == 3) ? " selected" : "";

  String out = webPageStart(
    D_WEB_SETTINGS_TITLE,
    D_WEB_SETTINGS_SUBTITLE_PREFIX FW_BUILD D_WEB_SETTINGS_SUBTITLE_SUFFIX,
    "<a href='/'>" D_WEB_SETTINGS_BACK_NAV "</a>"
  );
  out.reserve(out.length() + 4000);

  out +=
    "<div class='card'><h2>" D_WEB_READING_HEADING "</h2>"
    "<form method='POST' action='/settings' accept-charset='UTF-8'>"
    "<div class='grid cols-2'>"
    "<div><label for='font'>" D_WEB_FONT_SIZE_LABEL "</label><select id='font' name='font'>"
    "<option value='8'";  out += sel8;  out += ">" D_WEB_FONT_SIZE_8  "</option>"
    "<option value='10'"; out += sel10; out += ">" D_WEB_FONT_SIZE_10 "</option>"
    "<option value='12'"; out += sel12; out += ">" D_WEB_FONT_SIZE_12 "</option>"
    "<option value='14'"; out += sel14; out += ">" D_WEB_FONT_SIZE_14 "</option>"
    "</select><div class='hint'>" D_WEB_FONT_SIZE_HINT "</div></div>"
    "<div><label for='sleep'>" D_WEB_SLEEP_AFTER_LABEL "</label><select id='sleep' name='sleep'>"
    "<option value='30'";   out += ss30;   out += ">" D_WEB_SLEEP_30S "</option>"
    "<option value='60'";   out += ss60;   out += ">" D_WEB_SLEEP_1M  "</option>"
    "<option value='120'";  out += ss120;  out += ">" D_WEB_SLEEP_2M  "</option>"
    "<option value='300'";  out += ss300;  out += ">" D_WEB_SLEEP_5M  "</option>"
    "<option value='600'";  out += ss600;  out += ">" D_WEB_SLEEP_10M "</option>"
    "<option value='1800'"; out += ss1800; out += ">" D_WEB_SLEEP_30M "</option>"
    "</select><div class='hint'>" D_WEB_SLEEP_HINT "</div></div>"
    "<div><label for='lgap'>" D_WEB_LINE_SPACING_LABEL "</label><select id='lgap' name='lgap'>"
    "<option value='0'"; out += lg0; out += ">" D_WEB_LINE_SPACING_0 "</option>"
    "<option value='1'"; out += lg1; out += ">" D_WEB_LINE_SPACING_1 "</option>"
    "<option value='2'"; out += lg2; out += ">" D_WEB_LINE_SPACING_2 "</option>"
    "<option value='3'"; out += lg3; out += ">" D_WEB_LINE_SPACING_3 "</option>"
    "</select><div class='hint'>" D_WEB_LINE_SPACING_HINT "</div></div>"
    "</div>"
    "<div class='actions' style='margin-top:24px'><button type='submit'>" D_WEB_SAVE_SETTINGS_BUTTON "</button><span class='muted'>" D_WEB_SETTINGS_NO_EXTRAS "</span></div>"
    "</form></div>"
    "<div class='card'><h2>" D_WEB_SCREENSAVER_HEADING "</h2>"
    "<p class='muted'>" D_WEB_SCREENSAVER_CARD_DESC "</p>"
    "<div class='actions' style='margin-top:8px'>"
    "<a class='btn' href='/screensavers'>" D_WEB_SCREENSAVER_EDITOR_LINK "</a>"
    "<span class='muted'>" D_WEB_SCREENSAVER_EDITOR_HINT "</span>"
    "</div></div>";

  out += webPageEnd();
  server.send(200, "text/html; charset=utf-8", out);
}

static void handleSettingsPost() {
  // Font::setBodySize, Font::setLineGap, Sleep::setIdleTimeout all clamp /
  // validate internally — no inline guards needed here.
  if (server.hasArg("font")) {
    int fs = server.arg("font").toInt();
    if (fs != Font::currentBodySize()) Font::setBodySize(fs);
  }
  if (server.hasArg("sleep")) {
    int ss = server.arg("sleep").toInt();
    if (ss != Sleep::idleTimeoutSecs()) Sleep::setIdleTimeout(ss);
  }
  if (server.hasArg("lgap")) {
    int lg = server.arg("lgap").toInt();
    if (lg != Font::currentLineGap()) Font::setLineGap(lg);
  }
  // On-disk page caches are layout-stamped and self-invalidate on load, so
  // a font/lineGap change needs no cross-cutting cleanup here.
  server.sendHeader("Location", "/settings");
  server.send(302, "text/plain", "");
}

static void handleDeleteSleepImg() {
  if (FS.exists("/sleep.bin")) FS.remove("/sleep.bin");
  server.sendHeader("Location", "/settings");
  server.send(302, "text/plain", "");
}

void registerSettingsRoutes() {
  server.on("/settings",  HTTP_GET,  handleSettings);
  server.on("/settings",  HTTP_POST, handleSettingsPost);
  server.on("/del-sleep", HTTP_POST, handleDeleteSleepImg);
}
