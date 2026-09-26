#include "modes.h"

#include "display.h"
#include "modes/ambient_mode.h"
#include "modes/clock_mode.h"
#include "modes/countdown_mode.h"
#include "modes/hourglass_mode.h"
#include "modes/demo_mode.h"
#include "modes/forecast_mode.h"
#include "modes/gallery_mode.h"
#include "modes/life_mode.h"
#include "modes/off_mode.h"
#include "modes/quotes_mode.h"
#include "modes/sunrise_mode.h"
#include "modes/text_mode.h"
#include "modes/web_mode.h"
#include "settings.h"
#include "timekeeping.h"
#include "weather.h"

static TextMode textMode;
static QuotesMode quotesMode;
static ClockMode clockMode;
static ForecastMode forecastMode;
static WebMode webMode;
static LifeMode lifeMode;
static AmbientMode ambientMode;
static GalleryMode galleryModeInstance;
static CountdownMode countdownMode;
static HourglassMode hourglassMode;
static DemoMode demoModeInstance;
static SunriseMode sunriseMode;
static OffMode offMode;

Mode *const MODES[] = {&textMode, &quotesMode, &clockMode, &forecastMode, &webMode, &lifeMode, &ambientMode, &galleryModeInstance,
                        &countdownMode, &hourglassMode, &demoModeInstance, &offMode, &sunriseMode};
const uint8_t MODE_COUNT = sizeof(MODES) / sizeof(MODES[0]);

static uint8_t current = 0;       // index of the mode being shown
static bool started = false;      // current has been start()ed
static bool night = false;
static int playlistPos = -1;
static uint32_t playlistSince = 0;
static uint32_t lastCheck = 0;
static uint8_t appliedBrightness = 0;

static int indexOf(const String &id) {
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    if (id == MODES[i]->id()) return i;
  }
  return -1;
}

bool validModeId(const String &id) { return indexOf(id) >= 0 && !MODES[indexOf(id)]->hidden(); }

Mode *currentMode() { return MODES[current]; }
GalleryMode &galleryMode() { return galleryModeInstance; }
bool isNight() { return night; }
int playlistPosition() { return settings.playlistOn ? playlistPos : -1; }

// --- time slots ("scene") --------------------------------------------------
// settings.scenes: up to MAX_SCENES slots "HHMM|brightness|id:min,id:min",
// separated by ';'. With settings.scenesOn the playlist is the one of the
// slot that started last (the last slot of the day carries on past
// midnight), at that slot's brightness (0 = the Display setting).

static int sceneCount() {
  if (settings.scenes.length() == 0) return 0;
  int n = 1;
  for (unsigned i = 0; i < settings.scenes.length(); i++) n += settings.scenes[i] == ';';
  return min(n, MAX_SCENES);
}

// Field `field` (0 start, 1 brightness, 2 items) of slot `n`.
static String sceneField(int n, int field) {
  int start = 0;
  for (int i = 0; i < n; i++) start = settings.scenes.indexOf(';', start) + 1;
  int end = settings.scenes.indexOf(';', start);
  if (end < 0) end = settings.scenes.length();
  const String scene = settings.scenes.substring(start, end);
  const int a = scene.indexOf('|'), b = scene.indexOf('|', a + 1);
  if (a < 0 || b < 0) return "";
  return field == 0 ? scene.substring(0, a) : field == 1 ? scene.substring(a + 1, b) : scene.substring(b + 1);
}

int activeScene() {
  struct tm t;
  if (!settings.playlistOn || !settings.scenesOn || !localTime(t)) return -1;
  const int now = t.tm_hour * 60 + t.tm_min;
  const int count = sceneCount();
  int best = -1, bestStart = -1, latest = -1, latestStart = -1;
  for (int i = 0; i < count; i++) {
    const String hhmm = sceneField(i, 0);
    const int start = hhmm.substring(0, 2).toInt() * 60 + hhmm.substring(2, 4).toInt();
    if (start <= now && start > bestStart) best = i, bestStart = start;
    if (start > latestStart) latest = i, latestStart = start;
  }
  return best >= 0 ? best : latest;  // before the first slot: yesterday's last
}

// The playlist in use: the active slot's, or settings.playlist.
static String currentPlaylist() {
  const int scene = activeScene();
  return scene >= 0 ? sceneField(scene, 2) : settings.playlist;
}

// Playlist item `n` of the playlist in use ("id:minutes,..."); false if
// there is no such (valid) item.
static bool playlistItem(int n, int &mode, uint32_t &minutes) {
  const String list = currentPlaylist();
  int start = 0;
  for (int i = 0; start <= (int)list.length(); i++) {
    int end = list.indexOf(',', start);
    if (end < 0) end = list.length();
    if (i == n) {
      const String item = list.substring(start, end);
      const int colon = item.indexOf(':');
      if (colon < 0) return false;
      mode = indexOf(item.substring(0, colon));
      minutes = item.substring(colon + 1).toInt();
      return mode >= 0 && minutes > 0;
    }
    start = end + 1;
  }
  return false;
}

static bool inNightWindow() {
  struct tm t;
  if (!settings.nightOn || !localTime(t)) return false;
  const uint16_t now = t.tm_hour * 60 + t.tm_min;
  uint16_t from = settings.nightStart, to = settings.nightEnd;
  if (settings.nightSun) {
    // Sunset to sunrise; the fixed times stand in until they are known.
    const Weather w = weatherNow();
    if (w.sunrise >= 0 && w.sunset >= 0) {
      from = w.sunset;
      to = w.sunrise;
    }
  }
  return from <= to ? (now >= from && now < to) : (now >= from || now < to);  // may cross midnight
}

// Decides what to show now and switches to it if needed.
static void evaluate(uint32_t now) {
  night = inNightWindow();

  int wanted = indexOf(settings.mode);
  if (wanted < 0) wanted = 0;

  // A new time slot starts its playlist from the top.
  static int lastScene = -1;
  const int scene = activeScene();
  if (scene != lastScene) {
    lastScene = scene;
    playlistPos = -1;
  }

  if (settings.playlistOn) {
    int mode;
    uint32_t minutes;
    if (playlistPos < 0 || !playlistItem(playlistPos, mode, minutes)) {
      playlistPos = 0;
      playlistSince = now;
    } else if (now - playlistSince >= minutes * 60000) {
      playlistPos++;
      playlistSince = now;
    }
    if (!playlistItem(playlistPos, mode, minutes)) playlistPos = 0;  // wrap around
    if (playlistItem(playlistPos, mode, minutes)) wanted = mode;
  }

  const char *override = nullptr;
  if (night && settings.nightMode == "off") wanted = indexOf("off");
  if (night && settings.nightMode == "stars") {
    wanted = indexOf("ambient");
    override = "stars";
  }
  const bool overrideChanged = ambientMode.setOverride(override);

  uint8_t brightness = settings.brightness;
  if (scene >= 0) {
    const int sceneBrightness = sceneField(scene, 1).toInt();
    if (sceneBrightness > 0) brightness = min(255, sceneBrightness);
  }
  if (night && settings.nightMode == "dim") brightness = settings.nightBrightness;

  // The sunrise alarm wins over everything, and sets its own brightness.
  const float sunrise = SunriseMode::alarmProgress();
  if (sunrise >= 0) {
    wanted = indexOf("sunrise");
    brightness = SunriseMode::brightness(sunrise);
  }
  if (brightness != appliedBrightness) {
    display.setBrightness(brightness);
    appliedBrightness = brightness;
  }

  if (!started || wanted != current || (overrideChanged && wanted == indexOf("ambient"))) {
    current = wanted;
    started = true;
    display.beginTransition();
    MODES[current]->start();
  }
}

bool setMode(const String &id) {
  if (!validModeId(id)) return false;
  settings.mode = id;
  settings.playlistOn = false;
  started = false;  // restart even if it is already shown
  evaluate(millis());
  return true;
}

void nextMode() {
  int i = indexOf(settings.mode);
  do {
    i = (i + 1) % MODE_COUNT;
  } while (MODES[i]->hidden());
  setMode(MODES[i]->id());
}

void restartPlaylist() {
  playlistPos = -1;
  started = false;
  evaluate(millis());
}

void refreshModes() { evaluate(millis()); }

void updateMode() {
  const uint32_t now = millis();
  if (!started || now - lastCheck >= 1000) {
    lastCheck = now;
    evaluate(now);
  }
  MODES[current]->update(now);
  display.tick(now);
}

void restartMode() { MODES[current]->start(); }
