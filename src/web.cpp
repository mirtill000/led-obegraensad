#include "web.h"

#include <Update.h>
#include <WiFi.h>
#include <esp_system.h>
#include <WebServer.h>
#include <mbedtls/base64.h>

#include "animation.h"
#include "backup.h"
#include "ble.h"
#include "build_info.h"
#include "commands.h"
#include "remote_protocol.h"
#include "events.h"
#include "display.h"
#include "modes.h"
#include "modes/ambient_mode.h"
#include "gallery.h"
#include "modes/forecast_mode.h"
#include "modes/gallery_mode.h"
#include "modes/board.h"
#include "modes/formula_mode.h"
#include "modes/hourglass_mode.h"
#include "modes/pet_mode.h"
#include "modes/notify_mode.h"
#include "modes/quotes_mode.h"
#include "modes/sunrise_mode.h"
#include "moon.h"
#include "pager.h"
#include "webinfo.h"
#include "world.h"
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

// A game's pad for the page: {"keys":"LRA","labels":[...5, null = default],"repeat":..,"hint":..}.
static String controlsJson(const GameControls *c) {
  if (!c) return "null";
  String j = "{\"keys\":" + jsonString(c->keys) + ",\"labels\":[";
  for (int i = 0; i < 5; i++) j += String(i ? "," : "") + (c->labels[i] ? jsonString(c->labels[i]) : String("null"));
  return j + "],\"repeat\":" + jsonBool(c->repeat) + ",\"hint\":" + jsonString(c->hint) +
         ",\"players\":" + String(c->players) + "}";
}

static String stateJson() {
  String json;
  json.reserve(8192);  // one allocation: the state is about 6 kB
  json = "{\"settings\":" + settingsJson() + ",\"active\":" + jsonString(currentMode()->id());
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

  json += ",\"brightnessNow\":" + String(sunBrightness());
  json += ",\"animations\":[";
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    if (i) json += ',';
    json += "{\"id\":" + jsonString(ANIMATIONS[i]->id()) + ",\"name\":" + jsonString(ANIMATIONS[i]->name()) +
            ",\"group\":" + jsonString(ANIMATIONS[i]->group()) + ",\"game\":" + jsonBool(ANIMATIONS[i]->isGame());
    if (ANIMATIONS[i]->isGame()) json += ",\"style\":" + jsonString(styleId(ANIMATIONS[i]->style()));
    json += "}";
  }
  json += "]";
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
            ",\"demo\":" + jsonBool(forced || demoMode(game)) + ",\"forced\":" + jsonBool(forced) +
            ",\"pad\":" + controlsJson(findAnimation(game) ? findAnimation(game)->controls() : nullptr) + "}";
  } else {
    json += ",\"game\":null";
  }

  json += ",\"forecast\":" + jsonString(ForecastMode::summary());
  const WebInfo info = webInfoNow();
  json += ",\"web\":{\"wordText\":" + jsonString(info.word) +
          ",\"event\":" + jsonString(info.event) + ",\"historyStatus\":" + jsonString(info.historyStatus) +
          ",\"calendarStatus\":" + jsonString(info.calendarStatus) + "}";
  json += ",\"hourglass\":{\"left\":" +
          String(HourglassMode::secondsLeft()) + ",\"running\":" + jsonBool(HourglassMode::running()) + "}";
  const WorldInfo world = worldInfoNow();
  json += ",\"world\":{\"air\":" + (world.airOk ? String(world.aqi) : String("null")) +
          ",\"airBand\":" + jsonString(world.airOk ? aqiBand(world.aqi) : "") +
          ",\"iss\":" + (world.issOk ? String((long)distanceKm(settings.latitude, settings.longitude, world.issLat, world.issLon)) : String("null")) +
          ",\"status\":" + jsonString("aria " + (world.airStatus.length() ? world.airStatus : String("in attesa")) + " · ISS " +
                                        (world.issStatus.length() ? world.issStatus : String("in attesa"))) + "}";
  const PetMode::Status pet = PetMode::status();
  json += ",\"pet\":{\"name\":" + jsonString(pet.name) + ",\"stage\":" + jsonString(pet.stage) +
          ",\"mood\":" + jsonString(pet.mood) + ",\"food\":" + String(pet.food) + ",\"joy\":" + String(pet.joy) +
          ",\"energy\":" + String(pet.energy) + ",\"poops\":" + String(pet.poops) + ",\"sick\":" + jsonBool(pet.sick) +
          ",\"asleep\":" + jsonBool(pet.asleep) + ",\"hours\":" + String(pet.ageHours) +
          ",\"pad\":" + controlsJson(PetMode::keys()) + "}";
  json += ",\"ble\":{\"connected\":" + jsonBool(bleConnected()) + "}";
  json += ",\"notifyPending\":" + String(NotifyMode::pending());
  const float phase = moonPhase(time(nullptr));
  json += ",\"moon\":{\"name\":" + jsonString(moonPhaseName(phase)) + ",\"lit\":" +
          String((int)lroundf(moonIllumination(phase) * 100)) + "}";
  json += ",\"version\":" + jsonString(String(FIRMWARE_COMMIT) + " del " + FIRMWARE_BUILT);
  json += ",\"galleryCurrent\":" + jsonString(galleryMode().currentId());
  json += ",\"scene\":" + String(activeScene());

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
  String json = "{\"uptime\":" + String(millis() / 1000) + ",\"reset\":" + jsonString(resetReasonText());
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
  json += ",\"loop\":{\"perSec\":" + String(loopRounds / secs) + ",\"maxMs\":" + String(loopMaxUs / 1000.0f, 1) + "}";
  json += ",\"ble\":{\"on\":" + jsonBool(settings.bleOn) + ",\"connected\":" + jsonBool(bleConnected()) + "}";
  json += ",\"trial\":" + jsonBool(firmwarePendingVerify());
  json += ",\"events\":" + eventsJson() + "}";
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

// What a changed setting touches (see SettingEffect), once for a whole
// request.
static void applySettingEffects(const SettingDef *const *changed, int count) {
  uint16_t fx = 0;
  for (int i = 0; i < count; i++) fx |= changed[i]->effects;
  if (fx & FX_FONT) Display::setScrollFont(fontForSettings());
  if (fx & FX_ROTATION) {
    display.setRotation(rotationForSettings());
    Display::setVerticalText(settings.vertical);
  }
  if (fx & FX_TRANSITION) display.setTransition(transitionForSettings());
  if (fx & FX_TIMEZONE) applyTimezone();
  if (fx & FX_WEATHER) requestWeatherUpdate();
  if (fx & FX_WEB) requestWebInfoUpdate();
  if (fx & FX_PLAYLIST) restartPlaylist();
  bool restarted = false;
  for (int i = 0; i < count; i++) {
    const SettingDef &d = *changed[i];
    if (d.effects & FX_SHOW) {
      setMode(d.mode);
      restarted = true;
    } else if ((d.effects & FX_RESTART) && !restarted && (!d.mode || strcmp(currentMode()->id(), d.mode) == 0)) {
      restartMode();
      restarted = true;
    }
  }
  if (fx & (FX_MODES | FX_PLAYLIST)) refreshModes();
}

// POST /api/settings name=value&...: any of the settings the page may
// change (SETTING_DEFS). All are checked first: one refused value changes
// nothing (400 and the reason, with the setting's name).
static void handleSettings() {
  const SettingDef *changed[48];
  int count = 0;
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < server.args() && count < 48; i++) {
      const SettingDef *d = findSetting(server.argName(i));
      if (!d) continue;  // e.g. "plain": other fields of the request
      if (!(d->flags & SET_WEB)) return badRequest("Impostazione non modificabile");
      if (const char *reason = setSetting(*d, server.arg(i), pass == 1)) {
        return badRequest((String(reason) + " (" + d->name + ")").c_str());
      }
      if (pass == 1) changed[count++] = d;
    }
  }
  saveSettings();
  applySettingEffects(changed, count);
  bool reboot = false;
  for (int i = 0; i < count; i++) reboot |= (changed[i]->effects & FX_REBOOT) != 0;
  if (reboot) {
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", "{}");
    delay(500);  // let the answer reach the browser
    ESP.restart();
  }
  sendState();
}



// Keeps only well-formed "mode:minutes" items (at most 12); returns how
// many there are.




// The sand timer starts over (its time is the setting hgMin).
static void handleHourglass() {
  if (strcmp(currentMode()->id(), "hourglass") == 0) restartMode();
  else setMode("hourglass");
  saveSettings();
  sendState();
}

// The Gioco della vita's drawing board: GET gives its pixels (512 hex
// digits, row by row) and a version that changes with every stroke; POST
// /api/paint takes a batch of strokes p="x,y,level;x,y,level;..." (the page
// sends one every ~60 ms while you draw) and shows the board. Clear and
// "Fai vivere" are the commands "w c" and "w l".
static void handleCanvas() {
  static const char DIGITS[] = "0123456789abcdef";
  const uint8_t *px = board::pixels();
  String hex;
  hex.reserve(ROWS * COLS * 2);
  for (int i = 0; i < ROWS * COLS; i++) {
    hex += DIGITS[px[i] >> 4];
    hex += DIGITS[px[i] & 15];
  }
  server.send(200, "application/json", "{\"v\":" + String(board::version()) + ",\"px\":\"" + hex + "\"}");
}

static void handlePaint() {
  const String p = server.arg("p");
  int start = 0, painted = 0;
  while (start < (int)p.length() && painted < 256) {
    int end = p.indexOf(';', start);
    if (end < 0) end = p.length();
    int x, y, level;
    if (sscanf(p.substring(start, end).c_str(), "%d,%d,%d", &x, &y, &level) == 3) {
      board::paint(x, y, constrain(level, 0, 255));
      painted++;
    }
    start = end + 1;
  }
  if (strcmp(currentMode()->id(), "life") != 0) {
    setMode("life");
    saveSettings();
  }
  handleCanvas();
}

// A new formula for the Formule mode (f=...): 400 with the reason if it
// doesn't parse; otherwise saved and shown.
static void handleFormula() {
  String error;
  if (!FormulaMode::setFormula(server.arg("f"), error)) return badRequest(error.c_str());
  setMode("formula");
  saveSettings();
  sendState();
}

// The pet's name (name=) or a new egg (reset=1); care goes through /api/cmd.
static void handlePet() {
  if (server.hasArg("name")) {
    if (!server.arg("name").length()) return badRequest("Serve un nome");
    PetMode::rename(server.arg("name"));
  }
  if (server.arg("reset") == "1") {
    PetMode::reset();
    logEvent("Animaletto: nuovo uovo");
  }
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
// Forgets the paired remotes and draws a new PIN; restarts (switching
// Bluetooth on or off is the setting bleOn).
static void handleBle() {
  bleForgetRemotes();
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", "{}");
  delay(500);  // let the answer reach the browser
  ESP.restart();
}

// Backup: all settings as a file to download, and back.
static void handleBackup() {
  server.sendHeader("Content-Disposition", "attachment; filename=\"obegransad-backup.json\"");
  server.send(200, "application/json", settingsBackup());
}

static void handleRestore() {
  if (const char *error = restoreSettings(server.arg("plain"))) return badRequest(error);
  logEvent("Impostazioni ripristinate da un backup");
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "ok");
  delay(500);  // let the answer reach the browser
  ESP.restart();
}

// The alarm's buttons: stop it, or a one-minute test (its settings go
// through /api/settings).
static void handleAlarm() {
  const String cmd = server.arg("cmd");
  if (cmd == "stop") SunriseMode::dismiss();
  else if (cmd == "test") SunriseMode::test();
  else return badRequest("Comando sconosciuto");
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
    logEvent("Aggiornamento rifiutato: " + updateError);
    showUpdateResult(false);
    restartMode();  // back to what was on the panel
    return;
  }
  logEvent("Aggiornamento ricevuto: riavvio con il firmware nuovo");
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
  server.on("/api/hourglass", HTTP_POST, handleHourglass);
  server.on("/api/pet", HTTP_POST, handlePet);
  server.on("/api/formula", HTTP_POST, handleFormula);
  server.on("/api/canvas", HTTP_GET, handleCanvas);
  server.on("/api/paint", HTTP_POST, handlePaint);
  server.on("/api/notify", HTTP_ANY, handleNotify);
  server.on("/api/ble", HTTP_POST, handleBle);
  server.on("/api/backup", HTTP_GET, handleBackup);
  server.on("/api/restore", HTTP_POST, handleRestore);
  server.on("/api/alarm", HTTP_POST, handleAlarm);
  server.on("/api/gallery", HTTP_GET, handleGalleryList);
  server.on("/api/gallery/item", HTTP_GET, handleGalleryItem);
  server.on("/api/gallery/save", HTTP_POST, handleGallerySave);
  server.on("/api/gallery/delete", HTTP_POST, handleGalleryDelete);
  server.on("/api/gallery/show", HTTP_POST, handleGalleryShow);
  server.on("/api/draw", HTTP_POST, handleDraw);
  server.on("/api/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([] { server.send(404, "text/plain", "Pagina non trovata"); });
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
  if (path.startsWith("/input?k=") && path.length() == 10 && (strchr(REMOTE_KEYS, path[9]) || strchr(REMOTE_KEYS_P2, path[9]))) {
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
