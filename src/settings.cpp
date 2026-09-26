#include "settings.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <math.h>

#include "animation.h"
#include "constants.h"

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

void loadSettings() {
  prefs.begin("obegransad", true);
  settings.mode = prefs.getString("mode", "text");
  settings.text = prefs.getString("text", MESSAGE);
  settings.textFont = prefs.getString("scrollFont", "small");
  settings.textPosition = prefs.getString("textPos", "random");
  settings.brightness = prefs.getUChar("brightness", 255);
  settings.vertical = prefs.getBool("vertical", false);
  settings.transition = prefs.getString("transition", "fade");
  settings.latitude = prefs.getFloat("lat", DEFAULT_LATITUDE);
  settings.longitude = prefs.getFloat("lon", DEFAULT_LONGITUDE);
  settings.city = prefs.getString("city", DEFAULT_CITY);
  settings.timezone = prefs.getString("tz", TIMEZONE);
  settings.timezoneName = prefs.getString("tzName", TIMEZONE_NAME);
  settings.ambient = prefs.getString("ambient", "auto");
  settings.game = prefs.getString("game", "auto");
  settings.infoWord = prefs.getBool("infoWord", true);
  settings.infoHistory = prefs.getBool("infoHistory", true);
  settings.infoCalendar = prefs.getBool("infoCal", false);
  settings.icalUrl = prefs.getString("icalUrl", "");
  settings.webPosition = prefs.getString("webPos", "random");
  settings.quotes = prefs.getString("quotes", "");  // from before /quotes.txt
  settings.galleryShow = prefs.getString("galleryShow", "all");
  settings.demoStyle = prefs.getString("demoStyle", "auto");
  settings.gameStyle = prefs.getString("gameStyle", "soft");
  settings.playlistOn = prefs.getBool("plOn", false);
  settings.playlist = prefs.getString("playlist", "clock:10,quotes:3,ambient:5,games:5");
  settings.scenesOn = prefs.getBool("scenesOn", false);
  settings.scenes = prefs.getString("scenes",
                                    "0700|200|clock:10,forecast:1,quotes:3;"
                                    "1300|255|clock:10,web:3,ambient:10,games:5;"
                                    "1900|120|quotes:3,ambient:10,clock:5;"
                                    "2300|25|clock:30");
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
  settings.demoOff = prefs.getString("demoOff", "");
  settings.countdownLabel = prefs.getString("cdLabel", "Vacanze");
  settings.countdownDate = prefs.getString("cdDate", "");
  settings.countdownTime = prefs.getString("cdTime", "00:00");
  settings.hourglassMinutes = constrain(prefs.getUChar("hgMin", 5), 1, 120);
  settings.notifyNight = prefs.getBool("notifyNight", false);
  settings.bleOn = prefs.getBool("bleOn", true);
  settings.blePin = prefs.getUInt("blePin", 0);
  const bool newPin = settings.blePin < 100000 || settings.blePin > 999999;
  if (newPin) settings.blePin = 100000 + esp_random() % 900000;  // first boot: a random PIN, kept
  settings.alarmOn = prefs.getBool("alarmOn", false);
  settings.alarmTime = prefs.getUShort("alarmTime", 7 * 60);
  settings.alarmDays = prefs.getUChar("alarmDays", 0x1F);  // Monday-Friday
  settings.alarmRamp = prefs.getUChar("alarmRamp", 20);
  settings.alarmHold = prefs.getUChar("alarmHold", 30);
  settings.nightOn = prefs.getBool("nightOn", false);
  settings.nightSun = prefs.getBool("nightSun", false);
  settings.nightStart = prefs.getUShort("nightStart", 23 * 60);
  settings.nightEnd = prefs.getUShort("nightEnd", 7 * 60);
  settings.nightMode = prefs.getString("nightMode", "stars");
  settings.nightBrightness = prefs.getUChar("nightBright", 20);
  parseSpeeds(prefs.getString("speeds", ""));
  prefs.end();
  if (newPin) saveSettings();
}

void saveSettings() {
  prefs.begin("obegransad", false);
  prefs.putString("mode", settings.mode);
  prefs.putString("text", settings.text);
  prefs.putString("scrollFont", settings.textFont);
  prefs.putString("textPos", settings.textPosition);
  prefs.putUChar("brightness", settings.brightness);
  prefs.putBool("vertical", settings.vertical);
  prefs.putString("transition", settings.transition);
  prefs.putFloat("lat", settings.latitude);
  prefs.putFloat("lon", settings.longitude);
  prefs.putString("city", settings.city);
  prefs.putString("tz", settings.timezone);
  prefs.putString("tzName", settings.timezoneName);
  prefs.putString("ambient", settings.ambient);
  prefs.putString("game", settings.game);
  prefs.putBool("infoWord", settings.infoWord);
  prefs.putBool("infoHistory", settings.infoHistory);
  prefs.putBool("infoCal", settings.infoCalendar);
  prefs.putString("icalUrl", settings.icalUrl);
  prefs.putString("webPos", settings.webPosition);
  prefs.putString("galleryShow", settings.galleryShow);
  prefs.putString("demoStyle", settings.demoStyle);
  prefs.putString("gameStyle", settings.gameStyle);
  prefs.putBool("plOn", settings.playlistOn);
  prefs.putString("playlist", settings.playlist);
  prefs.putBool("scenesOn", settings.scenesOn);
  prefs.putString("scenes", settings.scenes);
  prefs.putString("demoOff", settings.demoOff);
  prefs.putString("cdLabel", settings.countdownLabel);
  prefs.putString("cdDate", settings.countdownDate);
  prefs.putString("cdTime", settings.countdownTime);
  prefs.putUChar("hgMin", settings.hourglassMinutes);
  prefs.putBool("notifyNight", settings.notifyNight);
  prefs.putBool("bleOn", settings.bleOn);
  prefs.putUInt("blePin", settings.blePin);
  prefs.putBool("alarmOn", settings.alarmOn);
  prefs.putUShort("alarmTime", settings.alarmTime);
  prefs.putUChar("alarmDays", settings.alarmDays);
  prefs.putUChar("alarmRamp", settings.alarmRamp);
  prefs.putUChar("alarmHold", settings.alarmHold);
  prefs.putBool("nightOn", settings.nightOn);
  prefs.putBool("nightSun", settings.nightSun);
  prefs.putUShort("nightStart", settings.nightStart);
  prefs.putUShort("nightEnd", settings.nightEnd);
  prefs.putString("nightMode", settings.nightMode);
  prefs.putUChar("nightBright", settings.nightBrightness);
  prefs.putString("speeds", formatSpeeds());
  prefs.end();
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
