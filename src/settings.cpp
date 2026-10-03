#include "settings.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <math.h>

#include "animation.h"
#include "constants.h"
#include "modes.h"

Settings settings;

static Preferences prefs;

// Speed levels, kept parsed here and saved as "id:level,id:level".
struct SpeedEntry {
  char id[16];
  uint8_t level;
};
static SpeedEntry speeds[12];
static uint8_t speedCount = 0;

static void parseSpeeds(const String &s) {
  speedCount = 0;
  int start = 0;
  while (start < (int)s.length() && speedCount < 12) {
    int end = s.indexOf(',', start);
    if (end < 0) end = s.length();
    const String item = s.substring(start, end);
    const int colon = item.indexOf(':');
    if (colon > 0 && colon < 16) {
      SpeedEntry &e = speeds[speedCount++];
      strlcpy(e.id, item.substring(0, colon).c_str(), sizeof(e.id));
      e.level = constrain(item.substring(colon + 1).toInt(), 1, 9);
    }
    start = end + 1;
  }
}

static String formatSpeeds() {
  String s;
  for (uint8_t i = 0; i < speedCount; i++) {
    if (i) s += ',';
    s += String(speeds[i].id) + ':' + speeds[i].level;
  }
  return s;
}

uint8_t speedLevel(const char *modeId) {
  for (uint8_t i = 0; i < speedCount; i++) {
    if (strcmp(speeds[i].id, modeId) == 0) return speeds[i].level;
  }
  return SPEED_DEFAULT;
}

void setSpeedLevel(const char *modeId, uint8_t level) {
  level = constrain(level, 1, 9);
  for (uint8_t i = 0; i < speedCount; i++) {
    if (strcmp(speeds[i].id, modeId) == 0) {
      speeds[i].level = level;
      return;
    }
  }
  if (speedCount < 12) {
    strlcpy(speeds[speedCount].id, modeId, sizeof(speeds[speedCount].id));
    speeds[speedCount++].level = level;
  }
}

uint32_t scaledInterval(const char *modeId, uint32_t baseMs) {
  // Each level step is a factor of sqrt(2): level 1 = x4 slower, 9 = x4 faster.
  static const float FACTORS[10] = {0, 4.0f, 2.83f, 2.0f, 1.41f, 1.0f, 0.71f, 0.5f, 0.35f, 0.25f};
  const uint32_t ms = lroundf(baseMs * FACTORS[speedLevel(modeId)]);
  return ms > 0 ? ms : 1;
}

bool demoMode(const char *gameId) {
  // Is gameId one of the comma-separated ids in demoOff? (Called on every
  // frame of a game: no String copies.)
  const char *list = settings.demoOff.c_str();
  const size_t n = strlen(gameId);
  for (const char *p = list; *p;) {
    const char *end = strchr(p, ',');
    const size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len == n && strncmp(p, gameId, n) == 0) return false;
    if (!end) break;
    p = end + 1;
  }
  return true;
}

void setDemoMode(const char *gameId, bool demo) {
  String list = String(',') + settings.demoOff + ',';
  list.replace(String(',') + gameId + ',', ",");
  if (!demo) list += String(gameId) + ',';
  // Back to "a,b" without the surrounding commas.
  while (list.startsWith(",")) list.remove(0, 1);
  while (list.endsWith(",")) list.remove(list.length() - 1);
  settings.demoOff = list;
}

// Removes the entries "id:N" from comma-separated lists (a playlist, or the
// lists inside the time slots "HHMM|B|a:1,b:2;...").
static void dropFromLists(String &lists, const char *id) {
  const String key = String(id) + ":";
  for (int i = lists.indexOf(key); i >= 0; i = lists.indexOf(key, i)) {
    if (i > 0 && lists[i - 1] != ',' && lists[i - 1] != '|') {  // part of a longer id
      i += key.length();
      continue;
    }
    int end = i;
    while (end < (int)lists.length() && lists[end] != ',' && lists[end] != ';') end++;
    if (end < (int)lists.length() && lists[end] == ',') end++;             // "x:1," -> ""
    else if (i > 0 && lists[i - 1] == ',') i--;                          // ",x:1" at the end
    lists.remove(i, end - i);
  }
}

// ---------------------------------------------------------------------------
// The table. Names are what the page uses; NVS keys are kept from older
// firmware so saved settings carry over.

#define STR_(x) #x
#define STR(x) STR_(x)
#define F_(member) (void *)&settings.member

static const char *checkMode(String &v) { return validModeId(v) ? nullptr : "Modalità sconosciuta"; }
static const char *checkAnimation(String &v) {
  const Animation *a = findAnimation(v);
  return v == "auto" || (a && !a->isGame() && !a->isClockFace()) ? nullptr : "Animazione sconosciuta";
}
static const char *checkGame(String &v) {
  const Animation *a = findAnimation(v);
  return v == "auto" || (a && a->isGame()) ? nullptr : "Gioco sconosciuto";
}
static const char *checkPlaylist(String &v) {
  String clean;
  cleanPlaylist(v, clean);
  v = clean;
  return nullptr;
}
static const char *checkScenes(String &v) {
  String clean;
  cleanScenes(v, clean);
  v = clean;
  return nullptr;
}
static const char *checkCity(String &v) {
  v.trim();
  if (!v.length()) v = "?";
  return nullptr;
}

using T = SettingType;
static const uint8_t SW = SET_SHOW | SET_WEB;  // shown and changeable from the page
const SettingDef SETTING_DEFS[] = {
    // name           NVS key        type     field                    min  max   default        choices
    {"mode", "mode", T::Text, F_(mode), 0, 20, "text", nullptr, SET_SHOW, 0, nullptr, checkMode},
    {"text", "text", T::Text, F_(text), 0, 200, MESSAGE, nullptr, SW, FX_RESTART, "text", nullptr},
    {"textFont", "scrollFont", T::Text, F_(textFont), 0, 0, "small", "small|big|mini|tiny", SW, FX_FONT | FX_RESTART, nullptr, nullptr},
    {"textPos", "textPos", T::Text, F_(textPosition), 0, 0, "random", "random|top|middle|bottom|pages", SW, FX_RESTART, "text", nullptr},
    {"brightness", "brightness", T::U8, F_(brightness), 1, 255, "255", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"vertical", "vertical", T::Bool, F_(vertical), 0, 1, "0", nullptr, SW, FX_ROTATION | FX_RESTART, nullptr, nullptr},
    {"transition", "transition", T::Text, F_(transition), 0, 0, "fade", "fade|wipe|none", SW, FX_TRANSITION, nullptr, nullptr},
    {"clockStyle", "clockStyle", T::Text, F_(clockStyle), 0, 0, "weather", "weather|binary|words|wordsen", SW, FX_RESTART, "clock", nullptr},
    {"occasions", "occasions", T::Bool, F_(occasions), 0, 1, "1", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"autoBright", "autoBright", T::Bool, F_(autoBright), 0, 1, "0", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"autoMin", "autoMin", T::U8, F_(autoMin), 1, 255, "25", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"lat", "lat", T::Float, F_(latitude), -90, 90, STR(DEFAULT_LATITUDE), nullptr, SW, FX_WEATHER | FX_MODES, nullptr, nullptr},
    {"lon", "lon", T::Float, F_(longitude), -180, 180, STR(DEFAULT_LONGITUDE), nullptr, SW, FX_WEATHER | FX_MODES, nullptr, nullptr},
    {"city", "city", T::Text, F_(city), 0, 60, DEFAULT_CITY, nullptr, SW, 0, nullptr, checkCity},
    {"tz", "tz", T::Text, F_(timezone), 1, 60, TIMEZONE, nullptr, SW, FX_TIMEZONE | FX_MODES, nullptr, nullptr},
    {"tzName", "tzName", T::Text, F_(timezoneName), 0, 60, TIMEZONE_NAME, nullptr, SW, 0, nullptr, nullptr},
    {"ambient", "ambient", T::Text, F_(ambient), 0, 20, "auto", nullptr, SW, FX_SHOW, "ambient", checkAnimation},
    {"games", "game", T::Text, F_(game), 0, 20, "auto", nullptr, SW, FX_SHOW, "games", checkGame},
    {"infoWord", "infoWord", T::Bool, F_(infoWord), 0, 1, "1", nullptr, SW, FX_WEB | FX_RESTART, "web", nullptr},
    {"infoHistory", "infoHistory", T::Bool, F_(infoHistory), 0, 1, "1", nullptr, SW, FX_WEB | FX_RESTART, "web", nullptr},
    {"infoCalendar", "infoCal", T::Bool, F_(infoCalendar), 0, 1, "0", nullptr, SW, FX_WEB | FX_RESTART, "web", nullptr},
    {"icalUrl", "icalUrl", T::Text, F_(icalUrl), 0, 500, "", nullptr, SW, FX_WEB, nullptr, nullptr},
    {"webPos", "webPos", T::Text, F_(webPosition), 0, 0, "random", "random|top|middle|bottom|pages", SW, FX_RESTART, "web", nullptr},
    {"galleryShow", "galleryShow", T::Text, F_(galleryShow), 0, 40, "all", nullptr, SET_SHOW, 0, nullptr, nullptr},
    {"demoStyle", "demoStyle", T::Text, F_(demoStyle), 0, 0, "auto", "auto|rows3|pages|rows2", SW, FX_RESTART, "demo", nullptr},
    {"gameStyle", "gameStyle", T::Text, F_(gameStyle), 0, 0, "soft", "soft|crisp", SW, 0, nullptr, nullptr},
    {"playlistOn", "plOn", T::Bool, F_(playlistOn), 0, 1, "0", nullptr, SW, FX_PLAYLIST | FX_MODES, nullptr, nullptr},
    {"playlist", "playlist", T::Text, F_(playlist), 0, 400, "clock:10,quotes:3,ambient:5,games:5", nullptr, SW, FX_PLAYLIST, nullptr, checkPlaylist},
    {"scenesOn", "scenesOn", T::Bool, F_(scenesOn), 0, 1, "0", nullptr, SW, FX_PLAYLIST | FX_MODES, nullptr, nullptr},
    {"scenes", "scenes", T::Text, F_(scenes), 0, 1000,
     "0700|200|clock:10,forecast:1,quotes:3;1300|255|clock:10,web:3,ambient:10,games:5;"
     "1900|120|quotes:3,ambient:10,clock:5;2300|25|clock:30",
     nullptr, SW, FX_PLAYLIST, nullptr, checkScenes},
    {"demoOff", "demoOff", T::Text, F_(demoOff), 0, 300, "", nullptr, 0, 0, nullptr, nullptr},
    {"formula", "formula", T::Text, F_(formula), 0, 300, "sin(t-hypot(x-7.5,y-7.5))", nullptr, SET_SHOW, 0, nullptr, nullptr},
    {"hgMin", "hgMin", T::U8, F_(hourglassMinutes), 1, 120, "5", nullptr, SW, FX_RESTART, "hourglass", nullptr},
    {"notifyNight", "notifyNight", T::Bool, F_(notifyNight), 0, 1, "0", nullptr, SW, 0, nullptr, nullptr},
    {"bleOn", "bleOn", T::Bool, F_(bleOn), 0, 1, "1", nullptr, SW, FX_REBOOT, nullptr, nullptr},
    {"blePin", "blePin", T::U32, F_(blePin), 0, 999999, "0", nullptr, SET_SHOW, 0, nullptr, nullptr},
    {"alarmOn", "alarmOn", T::Bool, F_(alarmOn), 0, 1, "0", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"alarmTime", "alarmTime", T::U16, F_(alarmTime), 0, 1439, "420", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"alarmDays", "alarmDays", T::U8, F_(alarmDays), 0, 127, "31", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"alarmRamp", "alarmRamp", T::U8, F_(alarmRamp), 5, 60, "20", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"alarmHold", "alarmHold", T::U8, F_(alarmHold), 1, 120, "30", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"nightOn", "nightOn", T::Bool, F_(nightOn), 0, 1, "0", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"nightSun", "nightSun", T::Bool, F_(nightSun), 0, 1, "0", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"nightStart", "nightStart", T::U16, F_(nightStart), 0, 1439, "1380", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"nightEnd", "nightEnd", T::U16, F_(nightEnd), 0, 1439, "420", nullptr, SW, FX_MODES, nullptr, nullptr},
    {"nightMode", "nightMode", T::Text, F_(nightMode), 0, 0, "stars", "off|stars|dim", SW, FX_MODES, nullptr, nullptr},
    {"nightBrightness", "nightBright", T::U8, F_(nightBrightness), 1, 255, "20", nullptr, SW, FX_MODES, nullptr, nullptr},
};
const uint8_t SETTING_COUNT = sizeof(SETTING_DEFS) / sizeof(SETTING_DEFS[0]);

// What was last read from or written to NVS, per setting, as text: what
// saveSettings() compares against. A key missing from NVS is "\x01".
static String stored[sizeof(SETTING_DEFS) / sizeof(SETTING_DEFS[0])];
static String storedSpeeds = "\x01";
static uint8_t storedVersion = 0;

const SettingDef *findSetting(const String &name) {
  for (const SettingDef &d : SETTING_DEFS) {
    if (name == d.name) return &d;
  }
  return nullptr;
}

String settingText(const SettingDef &d) {
  switch (d.type) {
    case T::Bool: return *(bool *)d.field ? "1" : "0";
    case T::U8: return String(*(uint8_t *)d.field);
    case T::U16: return String(*(uint16_t *)d.field);
    case T::U32: return String(*(uint32_t *)d.field);
    case T::Float: return String(*(float *)d.field, 4);
    default: return *(String *)d.field;
  }
}

String settingJson(const SettingDef &d) {
  if (d.type == T::Bool) return *(bool *)d.field ? "true" : "false";
  if (d.type != T::Text) return settingText(d);
  const String &v = *(String *)d.field;
  String out = "\"";
  for (unsigned i = 0; i < v.length(); i++) {
    const char c = v[i];
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n') out += "\\n";
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out + "\"";
}

const char *setSetting(const SettingDef &d, const String &input, bool apply) {
  String v = input;
  if (d.type == T::Text) {
    if (d.choices) {
      // One of "a|b|c".
      const String all = String('|') + d.choices + '|';
      if (v.indexOf('|') >= 0 || all.indexOf(String('|') + v + '|') < 0) return "Valore non ammesso";
    } else if (d.max && (int32_t)v.length() > d.max) {
      return "Testo troppo lungo";
    } else if ((int32_t)v.length() < d.min) {
      return "Valore mancante";
    }
  } else if (d.type == T::Bool) {
    if (v != "0" && v != "1" && v != "true" && v != "false") return "Valore non valido";
  } else {
    char *end;
    const double n = strtod(v.c_str(), &end);
    if (end == v.c_str() || *end || !isfinite(n)) return "Numero non valido";
    if (n < d.min || n > d.max) return "Valore fuori dai limiti";
  }
  if (d.clean) {
    if (const char *reason = d.clean(v)) return reason;
  }
  if (!apply) return nullptr;
  switch (d.type) {
    case T::Bool: *(bool *)d.field = v == "1" || v == "true"; break;
    case T::U8: *(uint8_t *)d.field = v.toInt(); break;
    case T::U16: *(uint16_t *)d.field = v.toInt(); break;
    case T::U32: *(uint32_t *)d.field = strtoul(v.c_str(), nullptr, 10); break;
    case T::Float: *(float *)d.field = v.toFloat(); break;
    default: *(String *)d.field = v;
  }
  return nullptr;
}

String settingsJson() {
  String j = "{";
  for (const SettingDef &d : SETTING_DEFS) {
    if (!(d.flags & SET_SHOW)) continue;
    if (j.length() > 1) j += ',';
    j += String('"') + d.name + "\":" + settingJson(d);
  }
  return j + "}";
}

// Reads one setting from NVS (its default if missing or out of range).
static void loadSetting(const SettingDef &d, String &raw) {
  raw = "\x01";
  if (prefs.isKey(d.nvsKey)) {
    switch (d.type) {
      case T::Bool: raw = prefs.getBool(d.nvsKey, false) ? "1" : "0"; break;
      case T::U8: raw = String(prefs.getUChar(d.nvsKey, 0)); break;
      case T::U16: raw = String(prefs.getUShort(d.nvsKey, 0)); break;
      case T::U32: raw = String(prefs.getUInt(d.nvsKey, 0)); break;
      case T::Float: raw = String(prefs.getFloat(d.nvsKey, 0), 4); break;
      default: raw = prefs.getString(d.nvsKey, "");
    }
  }
  // Saved text is taken as it is (a playlist naming a mode that no longer
  // exists is tidied by the migrations, not dropped here); numbers out of
  // range fall back to the default.
  if (d.type == T::Text) {
    *(String *)d.field = raw == "\x01" ? String(d.def) : raw;
  } else if (raw == "\x01" || setSetting(d, raw) != nullptr) {
    // The default (numbers written like "45.4642f" in constants.h).
    const double v = strtod(d.def, nullptr);
    switch (d.type) {
      case T::Bool: *(bool *)d.field = v != 0; break;
      case T::U8: *(uint8_t *)d.field = v; break;
      case T::U16: *(uint16_t *)d.field = v; break;
      case T::U32: *(uint32_t *)d.field = v; break;
      default: *(float *)d.field = v;
    }
  }
}

static void storeSetting(const SettingDef &d) {
  switch (d.type) {
    case T::Bool: prefs.putBool(d.nvsKey, *(bool *)d.field); break;
    case T::U8: prefs.putUChar(d.nvsKey, *(uint8_t *)d.field); break;
    case T::U16: prefs.putUShort(d.nvsKey, *(uint16_t *)d.field); break;
    case T::U32: prefs.putUInt(d.nvsKey, *(uint32_t *)d.field); break;
    case T::Float: prefs.putFloat(d.nvsKey, *(float *)d.field); break;
    default: prefs.putString(d.nvsKey, *(String *)d.field);
  }
}

int cleanPlaylist(const String &items, String &clean) {
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

// Time slots: "HHMM|brightness|items" separated by ';', at most 4, each
// with at least one valid item.
int cleanScenes(const String &raw, String &clean) {
  clean = "";
  int count = 0, start = 0;
  while (start < (int)raw.length() && count < MAX_SCENES) {
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
    if (clean.length()) clean += ';';
    clean += hhmm + '|' + String(constrain(scene.substring(a + 1, b).toInt(), 0, 255)) + '|' + items;
    count++;
  }
  return count;
}

void loadSettings() {
  prefs.begin("obegransad", true);
  for (uint8_t i = 0; i < SETTING_COUNT; i++) loadSetting(SETTING_DEFS[i], stored[i]);
  if (settings.quotes.length() == 0) settings.quotes = prefs.getString("quotes", "");  // from before /quotes.txt
  storedSpeeds = prefs.isKey("speeds") ? prefs.getString("speeds", "") : String("\x01");
  parseSpeeds(storedSpeeds == "\x01" ? String() : storedSpeeds);

  // Migrations, by settings version (saved as "cfgVer"): each step brings
  // settings saved by an older firmware up to date, once.
  const uint8_t version = prefs.getUChar("cfgVer", 0);
  storedVersion = version;
  prefs.end();
  // 2: new defaults reach lamps still on the old ones (Giochi joined the
  // default playlist and the 13:00 time slot).
  if (version < 2) {
    if (settings.playlist == "clock:10,quotes:3,ambient:5") settings.playlist = "clock:10,quotes:3,ambient:5,games:5";
    settings.scenes.replace("1300|255|clock:10,web:3,ambient:10;", "1300|255|clock:10,web:3,ambient:10,games:5;");
  }
  // 3: the Lavagna joined the Gioco della vita; the countdown is gone.
  if (version < 3) {
    if (settings.mode == "canvas") settings.mode = "life";
    if (settings.mode == "countdown") settings.mode = "clock";
    settings.playlist.replace("canvas:", "life:");
    settings.scenes.replace("canvas:", "life:");
    dropFromLists(settings.playlist, "countdown");
    dropFromLists(settings.scenes, "countdown");
    // A list left empty shows the clock instead.
    if (!settings.playlist.length()) settings.playlist = "clock:10";
    settings.scenes.replace("|;", "|clock:10;");
    if (settings.scenes.endsWith("|")) settings.scenes += "clock:10";
  }
  // 4: the clock faces left the Animazioni for the Orologio's styles.
  if (version < 4) {
    const Animation *face = findAnimation(settings.ambient);
    if (face && face->isClockFace()) {
      settings.clockStyle = settings.ambient;
      settings.ambient = "auto";
      if (settings.mode == "ambient") settings.mode = "clock";
    }
  }
  // Super Mario used to be a mode of its own; it is now one of the games.
  if (settings.mode == "mario") {
    settings.mode = "games";
    settings.game = "mario";
  }
  settings.playlist.replace("mario:", "games:");
  // The games used to be among the animations; they have a mode of their own.
  const Animation *picked = findAnimation(settings.ambient);
  if (picked && picked->isGame()) {
    settings.game = settings.ambient;
    settings.ambient = "auto";
    if (settings.mode == "ambient") settings.mode = "games";
  }
  if (!validModeId(settings.mode)) settings.mode = "clock";  // a mode removed since
  if (settings.blePin < 100000 || settings.blePin > 999999) {
    settings.blePin = 100000 + esp_random() % 900000;  // first boot: a random PIN, kept
  }
  saveSettings();  // the migrations' changes and the new PIN, if any
}

void saveSettings() {
  bool open = false;
  auto begin = [&] {
    if (!open) prefs.begin("obegransad", false);
    open = true;
  };
  if (storedVersion != SETTINGS_VERSION) {
    begin();
    prefs.putUChar("cfgVer", SETTINGS_VERSION);
    storedVersion = SETTINGS_VERSION;
  }
  for (uint8_t i = 0; i < SETTING_COUNT; i++) {
    const String now = settingText(SETTING_DEFS[i]);
    if (now == stored[i]) continue;
    begin();
    storeSetting(SETTING_DEFS[i]);
    stored[i] = now;
  }
  const String speedsNow = formatSpeeds();
  if (speedsNow != storedSpeeds) {
    begin();
    prefs.putString("speeds", speedsNow);
    storedSpeeds = speedsNow;
  }
  if (open) prefs.end();
}

TextFont fontForSettings() {
  if (settings.textFont == "big") return TextFont::Big;
  if (settings.textFont == "mini") return TextFont::Mini;
  if (settings.textFont == "tiny") return TextFont::Tiny;
  return TextFont::Small;
}

Transition transitionForSettings() {
  if (settings.transition == "wipe") return Transition::Wipe;
  if (settings.transition == "none") return Transition::None;
  return Transition::Fade;
}

uint16_t rotationForSettings() { return settings.vertical ? ROTATION_VERTICAL : ROTATION_HORIZONTAL; }

static const char *QUOTES_FILE = "/quotes.txt";

void loadQuotes() {
  File f = LittleFS.open(QUOTES_FILE, "r");
  if (!f) return;  // none saved: keep the old NVS copy, if any
  settings.quotes = f.readString();
  f.close();
}

bool saveQuotes() {
  bool ok;
  if (settings.quotes.length() == 0) {
    ok = !LittleFS.exists(QUOTES_FILE) || LittleFS.remove(QUOTES_FILE);  // back to the built-in list
  } else {
    File f = LittleFS.open(QUOTES_FILE, "w");
    ok = f && f.print(settings.quotes) == settings.quotes.length();
    if (f) f.close();
  }
  if (ok) {
    prefs.begin("obegransad", false);
    prefs.remove("quotes");  // superseded by the file
    prefs.end();
  }
  return ok;
}
