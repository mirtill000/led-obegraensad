#include "web.h"

#include <Update.h>
#include <WiFi.h>
#include <esp_system.h>
#include <WebServer.h>
#include <mbedtls/base64.h>

#include "animation.h"
#include "ble.h"
#include "build_info.h"
#include "commands.h"
#include "display.h"
#include "modes.h"
#include "modes/ambient_mode.h"
#include "gallery.h"
#include "modes/countdown_mode.h"
#include "modes/forecast_mode.h"
#include "modes/gallery_mode.h"
#include "modes/hourglass_mode.h"
#include "modes/notify_mode.h"
#include "modes/quotes_mode.h"
#include "modes/sunrise_mode.h"
#include "moon.h"
#include "pager.h"
#include "webinfo.h"
#include "settings.h"
#include "timekeeping.h"
#include "weather.h"

static WebServer server(80);
static WiFiServer events(81);  // live updates and game keys (see liveLoop())

// The control page: web/page.html with web/style.css and web/app.js inlined,
// gzip-compressed by scripts/webpage.py at build time (include/webpage.h).
#include "webpage.h"

// ---------------------------------------------------------------------------
// JSON state

static String jsonString(const String &s) {
  String out = "\"";
  for (unsigned i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;  // UTF-8 bytes pass through unchanged
    }
  }
  return out + "\"";
}

static String jsonBool(bool b) { return b ? "true" : "false"; }

static String stateJson() {
  String json;
  json.reserve(8192);  // one allocation: the state is about 6 kB
  json = "{\"mode\":" + jsonString(settings.mode) + ",\"active\":" + jsonString(currentMode()->id());
  json += ",\"night\":" + jsonBool(isNight()) + ",\"playlistPos\":" + String(playlistPosition());

  auto modeJson = [](const Mode *m) {
    String j = "{\"id\":" + jsonString(m->id()) + ",\"name\":" + jsonString(m->name());
    j += ",\"action\":" + (m->actionName() ? jsonString(m->actionName()) : String("null"));
    return j + ",\"hasSpeed\":" + jsonBool(m->hasSpeed()) + ",\"speed\":" + String(speedLevel(m->id())) + "}";
  };
  json += ",\"modes\":[";
  bool first = true;
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    if (MODES[i]->hidden()) continue;
    if (!first) json += ',';
    first = false;
    json += modeJson(MODES[i]);
  }
  json += "],\"activeMode\":" + modeJson(currentMode());

  json += ",\"text\":" + jsonString(settings.text) + ",\"textFont\":" + jsonString(settings.textFont);
  json += ",\"textPos\":" + jsonString(settings.textPosition);
  json += ",\"brightness\":" + String(settings.brightness) + ",\"vertical\":" + jsonBool(settings.vertical);
  json += ",\"transition\":" + jsonString(settings.transition) + ",\"gameStyle\":" + jsonString(settings.gameStyle);
  json += ",\"lat\":" + String(settings.latitude, 4) + ",\"lon\":" + String(settings.longitude, 4);
  json += ",\"city\":" + jsonString(settings.city) + ",\"tzName\":" + jsonString(settings.timezoneName);

  json += ",\"ambient\":" + jsonString(settings.ambient) + ",\"animations\":[";
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    if (i) json += ',';
    json += "{\"id\":" + jsonString(ANIMATIONS[i]->id()) + ",\"name\":" + jsonString(ANIMATIONS[i]->name()) +
            ",\"group\":" + jsonString(ANIMATIONS[i]->group()) + ",\"game\":" + jsonBool(ANIMATIONS[i]->isGame()) + "}";
  }
  json += "],\"games\":" + jsonString(settings.game);
  // The animation or game on the panel (Animazioni or Giochi mode).
  const bool player = strcmp(currentMode()->id(), "ambient") == 0 || strcmp(currentMode()->id(), "games") == 0;
  const AmbientMode *ambient = player ? static_cast<const AmbientMode *>(currentMode()) : nullptr;
  const Animation *playing = ambient ? ambient->playing() : nullptr;
  json += ",\"animation\":" + (playing ? jsonString(playing->id()) : String("null"));

  // The game on the panel and its demo mode.
  const char *game = currentMode()->gameId();
  if (game) {
    const bool forced = ambient && ambient->demoForced();
    const char *gameName = playing ? playing->name() : currentMode()->name();
    json += ",\"game\":{\"id\":" + jsonString(game) + ",\"name\":" + jsonString(gameName) +
            ",\"demo\":" + jsonBool(forced || demoMode(game)) + ",\"forced\":" + jsonBool(forced) + "}";
  } else {
    json += ",\"game\":null";
  }

  json += ",\"forecast\":" + jsonString(ForecastMode::summary());
  const WebInfo info = webInfoNow();
  json += ",\"web\":{\"word\":" + jsonBool(settings.infoWord) + ",\"history\":" + jsonBool(settings.infoHistory) +
          ",\"calendar\":" + jsonBool(settings.infoCalendar) + ",\"url\":" + jsonString(settings.icalUrl) +
          ",\"pos\":" + jsonString(settings.webPosition) + ",\"wordText\":" + jsonString(info.word) +
          ",\"event\":" + jsonString(info.event) + ",\"historyStatus\":" + jsonString(info.historyStatus) +
          ",\"calendarStatus\":" + jsonString(info.calendarStatus) + "}";
  json += ",\"countdown\":{\"label\":" + jsonString(settings.countdownLabel) + ",\"date\":" +
          jsonString(settings.countdownDate) + ",\"time\":" + jsonString(settings.countdownTime) +
          ",\"sentence\":" + jsonString(CountdownMode::sentence()) + "}";
  json += ",\"hourglass\":{\"minutes\":" + String(settings.hourglassMinutes) + ",\"left\":" +
          String(HourglassMode::secondsLeft()) + ",\"running\":" + jsonBool(HourglassMode::running()) + "}";
  json += ",\"ble\":{\"on\":" + jsonBool(settings.bleOn) + ",\"pin\":" + String(settings.blePin) +
          ",\"connected\":" + jsonBool(bleConnected()) + "}";
  json += ",\"notifyNight\":" + jsonBool(settings.notifyNight) + ",\"notifyPending\":" + String(NotifyMode::pending());
  json += ",\"alarm\":{\"on\":" + jsonBool(settings.alarmOn) + ",\"time\":" + String(settings.alarmTime) +
          ",\"days\":" + String(settings.alarmDays) + ",\"ramp\":" + String(settings.alarmRamp) +
          ",\"hold\":" + String(settings.alarmHold) + "}";
  const float phase = moonPhase(time(nullptr));
  json += ",\"moon\":{\"name\":" + jsonString(moonPhaseName(phase)) + ",\"lit\":" +
          String((int)lroundf(moonIllumination(phase) * 100)) + "}";
  json += ",\"version\":" + jsonString(String(FIRMWARE_COMMIT) + " del " + FIRMWARE_BUILT);
  json += ",\"galleryCurrent\":" + jsonString(galleryMode().currentId());
  json += ",\"demoStyle\":" + jsonString(settings.demoStyle);
  json += ",\"galleryShow\":" + jsonString(settings.galleryShow) + ",\"nightSun\":" + jsonBool(settings.nightSun);

  json += ",\"playlistOn\":" + jsonBool(settings.playlistOn) + ",\"playlist\":" + jsonString(settings.playlist);
  json += ",\"scenesOn\":" + jsonBool(settings.scenesOn) + ",\"scenes\":" + jsonString(settings.scenes) +
          ",\"scene\":" + String(activeScene());
  json += ",\"nightOn\":" + jsonBool(settings.nightOn) + ",\"nightStart\":" + String(settings.nightStart);
  json += ",\"nightEnd\":" + String(settings.nightEnd) + ",\"nightMode\":" + jsonString(settings.nightMode);
  json += ",\"nightBrightness\":" + String(settings.nightBrightness);

  struct tm t;
  if (localTime(t)) {
    char hhmm[6];
    strftime(hhmm, sizeof(hhmm), "%H:%M", &t);
    json += ",\"time\":" + jsonString(hhmm);
  } else {
    json += ",\"time\":null";
  }
  const Weather weather = weatherNow();
  if (weather.valid) {
    json += ",\"weather\":{\"temp\":" + String(weather.temperature, 1) + ",\"code\":" + String(weather.code);
    json += ",\"rainSoon\":" + jsonBool(rainSoon(weather));
    json += ",\"sunrise\":" + String(weather.sunrise) + ",\"sunset\":" + String(weather.sunset) + ",\"hours\":[";
    for (int i = 0; i < weather.hours; i++) {
      if (i) json += ',';
      json += "[" + String((weather.firstHour + i) % 24) + "," + String(weather.hourlyTemp[i], 1) + "," +
              String(weather.hourlyRain[i]) + "]";
    }
    // Daily forecast as on the lamp: [year, month, day, code, min, max, rain %].
    json += "],\"days\":[";
    for (int i = 0; i < weather.days; i++) {
      if (i) json += ',';
      json += "[" + String(weather.dayYear[i]) + "," + String(weather.dayMonth[i]) + "," + String(weather.dayOfMonth[i]) + "," +
              String(weather.dayCode[i]) + "," + String(weather.dayMin[i], 1) + "," + String(weather.dayMax[i], 1) + "," +
              String(weather.dayRain[i]) + "]";
    }
    json += "]}";
  } else {
    json += ",\"weather\":null";
  }
  json += "}";
  return json;
}

static void sendState() { server.send(200, "application/json", stateJson()); }

// ---------------------------------------------------------------------------
// Handlers: each changes settings, saves them and answers with the state.

static void badRequest(const char *message) { server.send(400, "text/plain", message); }

// Runs a remote command (remote_protocol.h), the same as Bluetooth's, and
// answers with the state, or 400 and why not.
static void command(const String &cmd) {
  if (const char *error = runCommand(cmd)) return badRequest(error);
  sendState();
}

// POST /api/cmd c=<command>: any remote command.
static void handleCommand() { command(server.arg("c")); }

static void handleMode() { command("m " + server.arg("id")); }

static void handleAction() { command("x"); }

static void handleInput() {
  if (const char *error = runCommand("k " + server.arg("key"))) return badRequest(error);
  server.send(204);
}

static void handleDemo() { command("d " + String(server.arg("on") == "1" ? "1" : "0") + " " + server.arg("id")); }

static void handleSpeed() { command("s " + server.arg("level") + " " + server.arg("id")); }

static void handleText() {
  // One line only: '|' would split the text in two.
  String text = server.arg("text");
  text.replace('|', ' ');
  text.trim();
  if (text.length() > 200) text = text.substring(0, 200);
  settings.text = text;
  setMode("text");  // show the new text straight away
  saveSettings();
  sendState();
}

static void handleQuotes() {
  String quotes = server.arg("quotes");
  quotes.replace("\r", "");
  quotes.trim();
  if (quotes.length() > QUOTES_MAX) return badRequest(("Troppo testo: al massimo " + String(QUOTES_MAX) + " caratteri").c_str());
  settings.quotes = quotes;
  if (!saveQuotes()) return server.send(500, "text/plain", "Non riesco a salvare le frasi nella memoria della lampada");
  if (strcmp(currentMode()->id(), "quotes") == 0) restartMode();
  sendState();
}

// The quotes list being used (the user's, or the built-in one) and how
// many there are.
static void sendQuotes() {
  // The textarea holds only the quotes added on the page; the built-in ones
  // are always shown too.
  server.send(200, "application/json",
              "{\"custom\":" + jsonBool(settings.quotes.length() > 0) + ",\"builtIn\":" +
                  String(QuotesMode::builtInCount()) + ",\"count\":" + String(QuotesMode::count()) +
                  ",\"quotes\":" + jsonString(settings.quotes) + "}");
}

static const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "accensione";
    case ESP_RST_SW: return "riavvio (aggiornamento o comando)";
    case ESP_RST_PANIC: return "errore del firmware";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog (blocco)";
    case ESP_RST_BROWNOUT: return "calo di tensione";
    case ESP_RST_DEEPSLEEP: return "risveglio";
    default: return "altro";
  }
}

// Diagnostics: system, network, data sources and the LED refresh.
static volatile uint32_t loopRounds = 0, loopMaxUs = 0, loopSince = 0;

void noteLoopTime(uint32_t us) {
  loopRounds = loopRounds + 1;
  if (us > loopMaxUs) loopMaxUs = us;
}

static void handleDiag() {
  const Weather w = weatherNow();
  const WebInfo info = webInfoNow();
  const Display::RefreshStats r = Display::refreshStats();
  String json = "{\"uptime\":" + String(millis() / 1000) + ",\"reset\":" + jsonString(resetReason());
  json += ",\"heap\":" + String(ESP.getFreeHeap()) + ",\"minHeap\":" + String(ESP.getMinFreeHeap());
  json += ",\"psram\":" + String(ESP.getFreePsram()) + ",\"chipTemp\":" + String(temperatureRead(), 1);
  json += ",\"version\":" + jsonString(String(FIRMWARE_COMMIT) + " del " + FIRMWARE_BUILT);
  json += ",\"ssid\":" + jsonString(WiFi.SSID()) + ",\"rssi\":" + String(WiFi.RSSI());
  json += ",\"ip\":" + jsonString(WiFi.localIP().toString()) + ",\"live\":" + String(liveClients());
  json += ",\"weather\":" + jsonString(weatherStatus());
  json += ",\"weatherAge\":" + String(w.valid ? (long)((millis() - w.fetchedAt) / 1000) : -1L);
  json += ",\"history\":" + jsonString(settings.infoHistory ? info.historyStatus : String(""));
  json += ",\"calendar\":" + jsonString(settings.infoCalendar ? info.calendarStatus : String(""));
  json += ",\"refresh\":{\"hw\":" + jsonBool(r.hardwareTimer) + ",\"planes\":" + String(r.planes);
  json += ",\"missed\":" + String(r.missed) + ",\"avg\":" + String(r.avgLatencyUs) + ",\"max\":" + String(r.maxLatencyUs);
  json += ",\"cycleUs\":" + String(r.cycleUs) + "}";
  const uint32_t secs = max<uint32_t>(1, (millis() - loopSince) / 1000);
  json += ",\"loop\":{\"perSec\":" + String(loopRounds / secs) + ",\"maxMs\":" + String(loopMaxUs / 1000.0f, 1) + "}}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// What the panel shows right now, for the page's preview: 256 levels as
// hex, row by row from the top-left (as seen on the lamp).
static void frameHex(char *out) {
  static const char HEX_DIGITS[] = "0123456789abcdef";
  int n = 0;
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      const uint8_t v = display.shownLevel(x, y);
      out[n++] = HEX_DIGITS[v >> 4];
      out[n++] = HEX_DIGITS[v & 15];
    }
  }
  out[n] = 0;
}

static void handleFrame() {
  char out[TOTAL_PIXELS * 2 + 1];
  frameHex(out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", out);
}

static void handleSettings() {
  if (server.hasArg("brightness")) settings.brightness = constrain(server.arg("brightness").toInt(), 1, 255);
  if (server.hasArg("textFont")) {
    const String font = server.arg("textFont");
    if (font != "small" && font != "big" && font != "mini" && font != "tiny") return badRequest("Font sconosciuto");
    settings.textFont = font;
    Display::setScrollFont(fontForSettings());
    restartMode();  // scrolling widths depend on the font
  }
  if (server.hasArg("gameStyle")) {
    const String style = server.arg("gameStyle");
    if (style != "soft" && style != "crisp") return badRequest("Stile sconosciuto");
    settings.gameStyle = style;
  }
  if (server.hasArg("transition")) {
    const String style = server.arg("transition");
    if (style != "fade" && style != "wipe" && style != "none") return badRequest("Passaggio sconosciuto");
    settings.transition = style;
    display.setTransition(transitionForSettings());
  }
  if (server.hasArg("vertical")) {
    settings.vertical = server.arg("vertical") == "1";
    display.setRotation(rotationForSettings());
    Display::setVerticalText(settings.vertical);
    restartMode();  // redraw straight away in the new orientation
  }
  if (server.hasArg("textPos")) {
    const String pos = server.arg("textPos");
    if (pos != "random" && pos != "top" && pos != "middle" && pos != "bottom" && pos != "pages") return badRequest("Altezza non valida");
    settings.textPosition = pos;
    if (strcmp(currentMode()->id(), "text") == 0) restartMode();  // show it at the new height now
  }
  if (server.hasArg("demoStyle")) {
    const String style = server.arg("demoStyle");
    if (style != "auto" && style != "rows3" && style != "pages" && style != "rows2") return badRequest("Stile sconosciuto");
    settings.demoStyle = style;
    if (strcmp(currentMode()->id(), "demo") == 0) restartMode();  // a new quote in the new style
  }
  if (server.hasArg("notifyNight")) settings.notifyNight = server.arg("notifyNight") == "1";
  if (server.hasArg("ambient")) {
    const String id = server.arg("ambient");
    const Animation *a = findAnimation(id);
    if (id != "auto" && !(a && !a->isGame())) return badRequest("Animazione sconosciuta");
    settings.ambient = id;
    setMode("ambient");
  }
  if (server.hasArg("games")) {
    const String id = server.arg("games");
    const Animation *a = findAnimation(id);
    if (id != "auto" && !(a && a->isGame())) return badRequest("Gioco sconosciuto");
    settings.game = id;
    setMode("games");
  }
  saveSettings();
  refreshModes();
  sendState();
}

static void handleLocation() {
  const float lat = server.arg("lat").toFloat(), lon = server.arg("lon").toFloat();
  if (!server.hasArg("lat") || !server.hasArg("lon") || fabsf(lat) > 90 || fabsf(lon) > 180) {
    return badRequest("Coordinate non valide");
  }
  settings.latitude = lat;
  settings.longitude = lon;
  String city = server.arg("city");
  city.trim();
  settings.city = city.length() ? city.substring(0, 60) : String("?");
  saveSettings();
  requestWeatherUpdate();
  sendState();
}

static void handleTimezone() {
  const String tz = server.arg("tz"), name = server.arg("tzName");
  if (tz.length() == 0 || tz.length() > 60 || name.length() > 60) return badRequest("Fuso orario non valido");
  settings.timezone = tz;
  settings.timezoneName = name;
  applyTimezone();
  saveSettings();
  refreshModes();
  sendState();
}

// Keeps only well-formed "mode:minutes" items (at most 12); returns how
// many there are.
static int cleanPlaylist(const String &items, String &clean) {
  clean = "";
  int start = 0, count = 0;
  while (start < (int)items.length() && count < 12) {
    int end = items.indexOf(',', start);
    if (end < 0) end = items.length();
    const String item = items.substring(start, end);
    const int colon = item.indexOf(':');
    const long minutes = colon > 0 ? item.substring(colon + 1).toInt() : 0;
    if (colon > 0 && validModeId(item.substring(0, colon)) && minutes >= 1 && minutes <= 240) {
      if (clean.length()) clean += ',';
      clean += item.substring(0, colon) + ':' + String(minutes);
      count++;
    }
    start = end + 1;
  }
  return count;
}

static void handlePlaylist() {
  String clean;
  const int count = cleanPlaylist(server.arg("items"), clean);
  // Time slots: "HHMM|brightness|items" separated by ';', at most 4, each
  // with at least one valid item.
  String scenes;
  int sceneCount = 0;
  const String raw = server.arg("scenes");
  int start = 0;
  while (start < (int)raw.length() && sceneCount < MAX_SCENES) {
    int end = raw.indexOf(';', start);
    if (end < 0) end = raw.length();
    const String scene = raw.substring(start, end);
    start = end + 1;
    const int a = scene.indexOf('|'), b = scene.indexOf('|', a + 1);
    if (a != 4 || b < 0) continue;
    const String hhmm = scene.substring(0, 4);
    if (hhmm.substring(0, 2).toInt() > 23 || hhmm.substring(2).toInt() > 59) continue;
    String items;
    if (cleanPlaylist(scene.substring(b + 1), items) == 0) continue;
    if (scenes.length()) scenes += ';';
    scenes += hhmm + '|' + String(constrain(scene.substring(a + 1, b).toInt(), 0, 255)) + '|' + items;
    sceneCount++;
  }
  if (server.hasArg("scenes")) {
    settings.scenes = scenes;
    settings.scenesOn = server.arg("scenesOn") == "1" && sceneCount > 0;
  }
  settings.playlist = clean;
  settings.playlistOn = server.arg("on") == "1" && (count > 0 || settings.scenesOn);
  saveSettings();
  restartPlaylist();
  sendState();
}

static void handleNight() {
  const String mode = server.arg("mode");
  if (mode != "off" && mode != "stars" && mode != "dim") return badRequest("Modalità notte sconosciuta");
  settings.nightOn = server.arg("on") == "1";
  settings.nightSun = server.arg("sun") == "1";
  settings.nightStart = constrain(server.arg("start").toInt(), 0, 1439);
  settings.nightEnd = constrain(server.arg("end").toInt(), 0, 1439);
  settings.nightMode = mode;
  settings.nightBrightness = constrain(server.arg("brightness").toInt(), 1, 255);
  saveSettings();
  refreshModes();
  sendState();
}

static void handleWeb() {
  const String pos = server.arg("pos"), url = server.arg("url");
  if (pos != "random" && pos != "top" && pos != "middle" && pos != "bottom" && pos != "pages") return badRequest("Altezza non valida");
  if (url.length() > 500) return badRequest("Link troppo lungo");
  settings.infoWord = server.arg("word") == "1";
  settings.infoHistory = server.arg("history") == "1";
  settings.infoCalendar = server.arg("calendar") == "1";
  settings.icalUrl = url;
  settings.webPosition = pos;
  saveSettings();
  requestWebInfoUpdate();
  if (strcmp(currentMode()->id(), "web") == 0) restartMode();
  sendState();
}

static void handleCountdown() {
  const String date = server.arg("date"), time = server.arg("time");
  if (date.length() && date.length() != 10) return badRequest("Data non valida");
  if (time.length() != 5) return badRequest("Ora non valida");
  String label = server.arg("label");
  label.trim();
  settings.countdownLabel = label.length() ? label.substring(0, 40) : String("L'evento");
  settings.countdownDate = date;
  settings.countdownTime = time;
  saveSettings();
  if (strcmp(currentMode()->id(), "countdown") == 0) restartMode();
  sendState();
}

static void handleHourglass() {
  const int minutes = server.arg("minutes").toInt();
  if (minutes < 1 || minutes > 120) return badRequest("Durata non valida");
  const bool changed = minutes != settings.hourglassMinutes;
  settings.hourglassMinutes = minutes;
  if (server.arg("start") == "1") {
    if (strcmp(currentMode()->id(), "hourglass") == 0) restartMode();
    else setMode("hourglass");
  } else if (changed && strcmp(currentMode()->id(), "hourglass") == 0) {
    restartMode();
  }
  saveSettings();
  sendState();
}

// Value of "key" in a flat JSON object (strings only), for callers that
// POST JSON (IFTTT webhooks, some automation apps).
static String jsonField(const String &body, const char *key) {
  const int k = body.indexOf(String("\"") + key + "\"");
  if (k < 0) return "";
  int i = body.indexOf(':', k);
  if (i < 0) return "";
  i = body.indexOf('"', i);
  if (i < 0) return "";
  String out;
  for (i++; i < (int)body.length() && body[i] != '"'; i++) {
    char c = body[i];
    if (c == '\\' && i + 1 < (int)body.length()) {
      c = body[++i];
      if (c == 'n') c = ' ';
    }
    out += c;
  }
  return out;
}

// Notification from the phone: GET or POST with text and icon (form or
// JSON). Answers {"ok":true,"queued":n}, or ok:false at night.
static void handleNotify() {
  String text = server.arg("text"), icon = server.arg("icon");
  if (!server.hasArg("text") && !server.hasArg("icon") && server.hasArg("plain")) {
    text = jsonField(server.arg("plain"), "text");
    icon = jsonField(server.arg("plain"), "icon");
  }
  icon.trim();
  icon.toLowerCase();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (isNight() && !settings.notifyNight) {
    server.send(200, "application/json", "{\"ok\":false,\"reason\":\"night\"}");
    return;
  }
  if (!NotifyMode::push(text, icon)) return badRequest("Serve un testo o un'icona conosciuta (bell, mail, check, alert, heart, phone, home, star)");
  server.send(200, "application/json", "{\"ok\":true,\"queued\":" + String(NotifyMode::pending()) + "}");
}

// Bluetooth: on/off, or forget the paired remotes (new PIN). Both restart
// the lamp, since the BLE stack is set up once at boot.
static void handleBle() {
  if (server.hasArg("forget")) bleForgetRemotes();
  if (server.hasArg("on")) settings.bleOn = server.arg("on") == "1";
  saveSettings();
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", "{}");
  delay(500);  // let the answer reach the browser
  ESP.restart();
}

static void handleAlarm() {
  const String cmd = server.arg("cmd");
  if (cmd == "stop") {
    SunriseMode::dismiss();
  } else if (cmd == "test") {
    SunriseMode::test();
  } else {
    settings.alarmOn = server.arg("on") == "1";
    settings.alarmTime = constrain(server.arg("time").toInt(), 0, 1439);
    settings.alarmDays = server.arg("days").toInt() & 0x7F;
    settings.alarmRamp = constrain(server.arg("ramp").toInt(), 5, 60);
    settings.alarmHold = constrain(server.arg("hold").toInt(), 1, 120);
    saveSettings();
  }
  refreshModes();
  sendState();
}

// --- Gallery ---------------------------------------------------------------

static String base64(const uint8_t *data, size_t length) {
  size_t outLength = 0;
  mbedtls_base64_encode(nullptr, 0, &outLength, data, length);
  String out;
  out.reserve(outLength);
  std::vector<unsigned char> buf(outLength + 1);
  mbedtls_base64_encode(buf.data(), buf.size(), &outLength, data, length);
  buf[outLength] = 0;
  out = (const char *)buf.data();
  return out;
}

static bool unbase64(const String &text, std::vector<uint8_t> &out) {
  size_t length = 0;
  mbedtls_base64_decode(nullptr, 0, &length, (const unsigned char *)text.c_str(), text.length());
  out.resize(length);
  return mbedtls_base64_decode(out.data(), out.size(), &length, (const unsigned char *)text.c_str(), text.length()) == 0 &&
         (out.resize(length), true);
}

static void handleGalleryList() {
  String json = "[";
  bool first = true;
  for (const Drawing &d : galleryList()) {
    if (!first) json += ',';
    first = false;
    json += "{\"id\":" + jsonString(d.id) + ",\"name\":" + jsonString(d.name) + ",\"frames\":" + String(d.frameCount()) +
            ",\"frameMs\":" + String(d.frameMs) + ",\"thumb\":\"" + base64(d.frames.data(), 256) + "\"}";
  }
  server.send(200, "application/json", json + "]");
}

static void handleGalleryItem() {
  Drawing d;
  if (!galleryLoad(server.arg("id"), d)) return badRequest("Disegno non trovato");
  server.send(200, "application/json",
              "{\"id\":" + jsonString(d.id) + ",\"name\":" + jsonString(d.name) + ",\"frameMs\":" + String(d.frameMs) +
                  ",\"data\":\"" + base64(d.frames.data(), d.frames.size()) + "\"}");
}

static bool readFrames(std::vector<uint8_t> &frames, uint16_t &frameMs) {
  if (!unbase64(server.arg("data"), frames) || frames.empty() || frames.size() % 256 ||
      frames.size() / 256 > GALLERY_MAX_FRAMES) {
    return false;
  }
  frameMs = constrain(server.arg("frameMs").toInt(), 30, 2000);
  return true;
}

static void handleGallerySave() {
  Drawing d;
  if (!readFrames(d.frames, d.frameMs)) return badRequest("Disegno non valido");
  d.id = server.arg("id");
  d.name = server.arg("name");
  if (!gallerySave(d)) return badRequest("Impossibile salvare (galleria piena?)");
  server.send(200, "application/json", "{\"id\":" + jsonString(d.id) + "}");
}

static void handleGalleryDelete() {
  const String id = server.arg("id");
  galleryDelete(id);
  if (settings.galleryShow == id) {
    settings.galleryShow = "all";
    saveSettings();
  }
  if (strcmp(currentMode()->id(), "gallery") == 0) restartMode();
  server.send(204);
}

static void handleGalleryShow() {
  settings.galleryShow = server.arg("id") == "all" ? String("all") : server.arg("id");
  setMode("gallery");
  saveSettings();
  sendState();
}

// The editor's drawing in progress, shown live.
static void handleDraw() {
  std::vector<uint8_t> frames;
  uint16_t frameMs;
  if (!readFrames(frames, frameMs)) return badRequest("Disegno non valido");
  if (strcmp(currentMode()->id(), "gallery") != 0) setMode("gallery");
  galleryMode().showDraft(frames.data(), frames.size() / 256, frameMs);
  server.send(204);
}

// --- Firmware update (OTA) -----------------------------------------------------
// The new image goes into the spare app partition; only if it verifies does
// the lamp boot from it, so a bad upload leaves the running firmware alone.

static String updateError;

// The panel during an update: the percentage in the middle and a progress
// bar at the bottom (rows 13-14), a bright "packet" blinking at its end.
static void showUpdateProgress(size_t done, size_t total) {
  display.clear();
  const int percent = total ? min<int>(100, done * 100 / total) : 0;
  const String label = String(percent) + "%";
  Pager::drawText((COLS - Pager::textWidth(label)) / 2, 5, label);
  const float filled = total ? (float)done / total * COLS : 0;
  const int head = (int)filled;
  for (int x = 0; x < COLS; x++) {
    const uint8_t level = x < head ? 150 : 20;
    display.setLevel(x, 13, level);
    display.setLevel(x, 14, level);
  }
  if (head < COLS) {  // the packet on its way, blinking
    const uint8_t level = (millis() / 120) % 2 ? 255 : 90;
    display.setLevel(head, 13, level);
    display.setLevel(head, 14, level);
  }
  display.render();
}

// The end of an update: "OK" and a full bar before the restart, or "ERR"
// blinking before going back to the mode on show.
static void showUpdateResult(bool ok) {
  for (int blink = 0; blink < (ok ? 1 : 3); blink++) {
    display.clear();
    const char *label = ok ? "OK" : "ERR";
    Pager::drawText((COLS - Pager::textWidth(label)) / 2, 5, label);
    for (int x = 0; x < COLS; x++) {
      display.setLevel(x, 13, ok ? 255 : (x % 2 ? 255 : 0));
      display.setLevel(x, 14, ok ? 255 : (x % 2 ? 0 : 255));
    }
    display.render();
    delay(ok ? 0 : 300);
    if (!ok) {
      display.clear();
      display.render();
      delay(200);
    }
  }
}

static void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  feedLoopWDT();  // an upload takes longer than the watchdog allows one loop()
  const size_t total = server.clientContentLength();
  switch (up.status) {
    case UPLOAD_FILE_START:
      updateError = "";
      if (!up.filename.endsWith(".bin") || up.filename.indexOf("factory") >= 0) {
        updateError = "serve il file firmware.bin";
      } else if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        updateError = Update.errorString();
      }
      showUpdateProgress(0, total);
      break;
    case UPLOAD_FILE_WRITE:
      if (updateError.length() == 0 && Update.write(up.buf, up.currentSize) != up.currentSize) {
        updateError = Update.errorString();  // e.g. "Wrong Magic Byte": not an ESP32 firmware
      }
      showUpdateProgress(up.totalSize + up.currentSize, total);
      break;
    case UPLOAD_FILE_END:
      if (updateError.length() == 0 && !Update.end(true)) updateError = Update.errorString();
      break;
    case UPLOAD_FILE_ABORTED:
      Update.abort();
      updateError = "caricamento interrotto";
      break;
  }
}

static void handleUpdateDone() {
  if (updateError.length() || Update.hasError()) {
    if (Update.isRunning()) Update.abort();
    server.send(400, "text/plain", "Aggiornamento non riuscito: " + updateError);
    showUpdateResult(false);
    restartMode();  // back to what was on the panel
    return;
  }
  showUpdateResult(true);
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "ok");
  delay(500);  // let the answer reach the browser
  ESP.restart();
}

void webBegin() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html; charset=utf-8", (const char *)WEBPAGE_GZ, WEBPAGE_GZ_LEN);
  });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/frame", HTTP_GET, handleFrame);
  server.on("/api/diag", HTTP_GET, handleDiag);
  server.on("/api/diag/reset", HTTP_POST, [] {
    Display::resetRefreshStats();
    loopRounds = loopMaxUs = 0;
    loopSince = millis();
    server.send(204);
  });
  server.on("/api/cmd", HTTP_POST, handleCommand);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/speed", HTTP_POST, handleSpeed);
  server.on("/api/input", HTTP_POST, handleInput);
  server.on("/api/demo", HTTP_POST, handleDemo);
  server.on("/api/text", HTTP_POST, handleText);
  server.on("/api/quotes", HTTP_POST, handleQuotes);
  server.on("/api/quotes", HTTP_GET, sendQuotes);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/location", HTTP_POST, handleLocation);
  server.on("/api/timezone", HTTP_POST, handleTimezone);
  server.on("/api/playlist", HTTP_POST, handlePlaylist);
  server.on("/api/night", HTTP_POST, handleNight);
  server.on("/api/web", HTTP_POST, handleWeb);
  server.on("/api/countdown", HTTP_POST, handleCountdown);
  server.on("/api/hourglass", HTTP_POST, handleHourglass);
  server.on("/api/notify", HTTP_ANY, handleNotify);
  server.on("/api/ble", HTTP_POST, handleBle);
  server.on("/api/alarm", HTTP_POST, handleAlarm);
  server.on("/api/gallery", HTTP_GET, handleGalleryList);
  server.on("/api/gallery/item", HTTP_GET, handleGalleryItem);
  server.on("/api/gallery/save", HTTP_POST, handleGallerySave);
  server.on("/api/gallery/delete", HTTP_POST, handleGalleryDelete);
  server.on("/api/gallery/show", HTTP_POST, handleGalleryShow);
  server.on("/api/draw", HTTP_POST, handleDraw);
  server.on("/api/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
  events.begin();
}

// ---------------------------------------------------------------------------
// Port 81: live updates and game keys, served without ever blocking loop().
//
//   GET /events     Server-Sent Events: a "frame" event when the panel
//                   changes (at most every 150 ms) and a "state" event when
//                   /api/state changes (checked every 2 s).
//   GET /input?k=L  a game key (L R U D A), answered 204 on a kept-alive
//                   connection, so held keys don't open a TCP connection
//                   each.
//
// Everything goes out with non-blocking send(): what the socket can't take
// yet waits in the connection's own buffer, and while it waits no new frame
// is queued (the next one sent is simply the latest). A connection that
// takes nothing for STALL_MS (a phone gone to sleep, out of range) is
// closed. The port-80 WebServer instead writes with blocking calls that can
// hold loop() for seconds when a client stops reading, which froze the
// panel mid-game.

#include <lwip/sockets.h>

static const int MAX_LIVE = 5;
static const uint32_t FRAME_EVERY_MS = 150, STATE_EVERY_MS = 2000, PING_EVERY_MS = 15000;
static const uint32_t STALL_MS = 5000, IDLE_MS = 15000, REQUEST_MS = 3000;
static const size_t MAX_BACKLOG = 16384;

struct LiveClient {
  WiFiClient client;
  bool sse = false;         // answered /events: now only receives events
  String in, out;           // request being read; bytes not sent yet
  uint32_t since = 0, lastActive = 0, lastProgress = 0;
  uint32_t lastFrame = 0, lastState = 0;  // hashes of what it last got
  bool closeAfter = false;  // close once `out` is sent
};
static LiveClient live[MAX_LIVE];

static uint32_t hashOf(const char *s, size_t n) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (size_t i = 0; i < n; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
  return h;
}

static void drop(LiveClient &c) {
  c.client.stop();
  c.sse = c.closeAfter = false;
  c.in = String();
  c.out = String();
}

int liveClients() {
  int n = 0;
  for (LiveClient &c : live) n += c.sse && c.client.connected();
  return n;
}

// Sends what the socket takes now; false if the connection is gone.
static bool flush(LiveClient &c, uint32_t now) {
  if (!c.out.length()) {
    c.lastProgress = now;
    return true;
  }
  const int sent = ::send(c.client.fd(), c.out.c_str(), c.out.length(), MSG_DONTWAIT);
  if (sent > 0) {
    c.out.remove(0, sent);
    c.lastProgress = now;
    return true;
  }
  if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return false;
  return now - c.lastProgress < STALL_MS;
}

// False if the client is too far behind to take more (flush() times it out).
static bool queue(LiveClient &c, const String &text) {
  if (c.out.length() + text.length() > MAX_BACKLOG) return false;
  c.out += text;
  return true;
}

static bool queueEvent(LiveClient &c, const char *name, const char *data, size_t length) {
  String e;
  e.reserve(length + 24);
  e = "event: ";
  e += name;
  e += "\ndata: ";
  e.concat(data, length);
  e += "\n\n";
  return queue(c, e);
}

// A complete request in c.in: answer it.
static void answer(LiveClient &c, uint32_t now) {
  const int sp = c.in.indexOf(' '), sp2 = c.in.indexOf(' ', sp + 1);
  const String path = sp > 0 && sp2 > sp ? c.in.substring(sp + 1, sp2) : String();
  c.in = String();
  c.lastActive = now;
  if (path.startsWith("/events")) {
    queue(c, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\n"
             "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\nretry: 3000\n\n");
    c.sse = true;
    c.lastFrame = c.lastState = 0;  // send both right away
    return;
  }
  if (path.startsWith("/input?k=") && path.length() == 10 && strchr("LRUDA", path[9])) {
    runCommand(String("k ") + path[9]);
    queue(c, "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nCache-Control: no-store\r\n"
             "Connection: keep-alive\r\nContent-Length: 0\r\n\r\n");
    return;
  }
  queue(c, "HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
  c.closeAfter = true;
}

static void liveLoop() {
  const uint32_t now = millis();
  // New connection: a free slot, else the one idle longest (never a page
  // receiving events, if there is another choice).
  if (events.hasClient()) {
    LiveClient *slot = nullptr;
    for (LiveClient &c : live) {
      if (!c.client.connected()) {
        slot = &c;
        break;
      }
      if (!slot || (slot->sse && !c.sse) || (slot->sse == c.sse && c.lastActive < slot->lastActive)) slot = &c;
    }
    drop(*slot);
    slot->client = events.accept();
    slot->client.setNoDelay(true);
    // Notice a phone that vanished within about half a minute.
    int on = 1, idle = 10, interval = 5, count = 3;
    const int fd = slot->client.fd();
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
    slot->since = slot->lastActive = slot->lastProgress = now;
  }

  bool anySse = false;
  for (LiveClient &c : live) {
    if (!c.client.connected()) {
      if (c.out.length() || c.in.length()) drop(c);
      continue;
    }
    // Requests (a page receiving events sends nothing more; drain it).
    int n = c.client.available();
    while (n-- > 0) {
      const int ch = c.client.read();
      if (ch < 0) break;
      if (c.sse) continue;
      if (c.in.length() < 512) c.in += (char)ch;
      if (c.in.endsWith("\r\n\r\n")) answer(c, now);
    }
    if (!c.sse && c.in.length() && now - c.lastActive > REQUEST_MS && !c.in.endsWith("\r\n\r\n")) c.in = String();
    if (!flush(c, now) || (c.closeAfter && !c.out.length()) || (!c.sse && now - c.lastActive > IDLE_MS)) {
      drop(c);
      continue;
    }
    anySse |= c.sse;
  }
  if (!anySse) return;

  static uint32_t lastFrameAt = 0, lastStateAt = 0, lastPingAt = 0;
  if (now - lastFrameAt >= FRAME_EVERY_MS) {
    lastFrameAt = now;
    char frame[TOTAL_PIXELS * 2 + 1];
    frameHex(frame);
    const uint32_t h = hashOf(frame, TOTAL_PIXELS * 2);
    for (LiveClient &c : live) {
      if (!c.sse || c.lastFrame == h || c.out.length()) continue;  // busy: it gets a later frame
      if (queueEvent(c, "frame", frame, TOTAL_PIXELS * 2)) c.lastFrame = h;
    }
  }
  bool stateDue = now - lastStateAt >= STATE_EVERY_MS;
  for (LiveClient &c : live) stateDue |= c.sse && c.lastState == 0;  // just connected
  if (stateDue) {
    lastStateAt = now;
    const String json = stateJson();
    const uint32_t h = hashOf(json.c_str(), json.length());
    for (LiveClient &c : live) {
      if (c.sse && c.lastState != h && queueEvent(c, "state", json.c_str(), json.length())) c.lastState = h;
    }
  }
  if (now - lastPingAt >= PING_EVERY_MS) {  // keeps idle connections (and proxies) alive
    lastPingAt = now;
    for (LiveClient &c : live) {
      if (c.sse && !c.out.length()) queue(c, ": ping\n\n");
    }
  }
  for (LiveClient &c : live) {
    if (c.sse && c.out.length() && !flush(c, now)) drop(c);
  }
}

void webLoop() {
  server.handleClient();
  liveLoop();
}
