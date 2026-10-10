#include "modes.h"
#include "moon.h"
#include "occasions.h"

#include "catalog.h"
#include "display.h"
#include "modes/ambient_mode.h"
#include "modes/clock_mode.h"
#include "modes/creature.h"
#include "modes/hourglass_mode.h"
#include "modes/notify_mode.h"
#include "modes/demo_mode.h"
#include "modes/forecast_mode.h"
#include "modes/formula_mode.h"
#include "modes/gallery_mode.h"
#include "modes/life_mode.h"
#include "modes/off_mode.h"
#include "modes/pet_mode.h"
#include "modes/quotes_mode.h"
#include "modes/sunrise_mode.h"
#include "modes/text_mode.h"
#include "modes/web_mode.h"
#include "modes/world_mode.h"
#include "settings.h"
#include "timekeeping.h"
#include "weather.h"

static TextMode textMode;
static QuotesMode quotesMode;
static ClockMode clockMode;
static ForecastMode forecastMode;
static WebMode webMode;
static WorldMode worldMode;
static LifeMode lifeMode;
static AmbientMode ambientMode(false);
static AmbientMode gamesMode(true);
static GalleryMode galleryModeInstance;
static HourglassMode hourglassMode;
static PetMode petMode;
static FormulaMode formulaMode;
static NotifyMode notifyMode;
static DemoMode demoModeInstance;
static SunriseMode sunriseMode;
static OffMode offMode;

Mode *const MODES[] = {&textMode, &quotesMode, &clockMode, &forecastMode, &webMode, &worldMode, &lifeMode, &ambientMode, &gamesMode, &galleryModeInstance, &formulaMode,
                        &hourglassMode, &petMode, bonsaiCreature, catCreature, dragonCreature, &demoModeInstance, &offMode, &sunriseMode, &notifyMode};
const uint8_t MODE_COUNT = sizeof(MODES) / sizeof(MODES[0]);
Creature *const CREATURES[] = {bonsaiCreature, catCreature, dragonCreature};
const uint8_t CREATURE_COUNT = sizeof(CREATURES) / sizeof(CREATURES[0]);

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

uint8_t sunBrightness() {
  if (!settings.autoBright) return settings.brightness;
  struct tm t;
  if (!localTime(t)) return settings.brightness;
  float azimuth, elevation;
  sunPosition(time(nullptr), settings.latitude, settings.longitude, azimuth, elevation);
  // Lowest from civil dusk (sun 6 degrees below the horizon), highest with
  // the sun 15 degrees up, a smooth curve in between: it changes by a
  // level now and then, which nobody notices.
  float f = constrain((elevation * 57.2958f + 6) / 21.0f, 0.0f, 1.0f);
  f = f * f * (3 - 2 * f);
  const uint8_t low = min(settings.autoMin, settings.brightness);
  return low + (uint8_t)lroundf((settings.brightness - low) * f);
}
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

// Playlist item `n` of the playlist in use ("scene:minutes,...", scenes as
// in catalog.h); false if there is no such (valid) item. `pick` is what the
// mode shows inside it (an animation, a game, a drawing; "" for none).
static bool playlistItem(int n, int &mode, uint32_t &minutes, String &pick) {
  const String list = currentPlaylist();
  int start = 0;
  for (int i = 0; start <= (int)list.length(); i++) {
    int end = list.indexOf(',', start);
    if (end < 0) end = list.length();
    if (i == n) {
      const String item = list.substring(start, end);
      const int colon = item.lastIndexOf(':');
      if (colon < 0) return false;
      String modeId;
      if (!sceneTarget(item.substring(0, colon), modeId, pick)) return false;
      if (!sceneAvailable(item.substring(0, colon))) return false;  // out of season: skipped
      mode = indexOf(modeId);
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

static bool chooseScene(const String &scene);

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

  String pick;  // inside the mode: what the playlist item picks
  if (settings.playlistOn) {
    int mode;
    uint32_t minutes;
    if (playlistPos < 0 || !playlistItem(playlistPos, mode, minutes, pick)) {
      playlistPos = 0;
      playlistSince = now;
    } else if (now - playlistSince >= minutes * 60000) {
      playlistPos++;
      playlistSince = now;
    }
    // The next item that can be shown (skipping the ones out of season),
    // wrapping around.
    bool found = false;
    for (int tries = 0; tries < 24 && !found; tries++) {
      found = playlistItem(playlistPos, mode, minutes, pick);
      if (!found) playlistPos = tries < MAX_PLAYLIST_ITEMS ? playlistPos + 1 : 0;
      if (playlistPos >= MAX_PLAYLIST_ITEMS) playlistPos = 0;
    }
    if (found) wanted = mode;
    else pick = "";
  }

  const char *override = nullptr;
  if (night && settings.nightMode == "off") wanted = indexOf("off");
  if (night && settings.nightMode == "stars") {
    wanted = indexOf("ambient");
    override = "stars";
  }
  // A special day (occasions.h): its animation for the first minute of
  // every hour, then back to what was on.
  if (!override && settings.occasions && !night && settings.mode != "games") {  // not in the middle of a game
    struct tm t;
    if (localTime(t) && t.tm_min == 0) {
      const Occasion o = occasionNow();
      if (o.animation) {
        wanted = indexOf("ambient");
        override = o.animation;
      }
    }
  }
  const bool overrideChanged = ambientMode.setOverride(override);

  uint8_t brightness = sunBrightness();
  if (scene >= 0) {
    const int sceneBrightness = sceneField(scene, 1).toInt();
    if (sceneBrightness > 0) brightness = min(255, sceneBrightness);
  }
  if (night && settings.nightMode == "dim") brightness = settings.nightBrightness;

  // Notifications from the phone come next (at night only if allowed).
  if (night && !settings.notifyNight) NotifyMode::clear();
  if (NotifyMode::pending()) wanted = indexOf("notify");

  // The sunrise alarm wins over everything, and sets its own brightness.
  // When it is over, the scene chosen for after it (settings.alarmScene)
  // takes over, as if picked on the page.
  const float sunrise = SunriseMode::alarmProgress();
  static bool alarmWasOn = false;
  if (sunrise >= 0) {
    wanted = indexOf("sunrise");
    brightness = SunriseMode::brightness(sunrise);
  } else if (alarmWasOn) {
    alarmWasOn = false;
    if (settings.alarmScene.length() && chooseScene(settings.alarmScene)) {
      saveSettings();
      return evaluate(now);
    }
  }
  alarmWasOn = sunrise >= 0;
  if (brightness != appliedBrightness) {
    display.setBrightness(brightness);
    appliedBrightness = brightness;
  }

  // The pick goes to the mode that will show it; any other mode gets none.
  bool pickChanged = false;
  for (uint8_t i = 0; i < MODE_COUNT; i++) pickChanged |= MODES[i]->setPick(i == wanted ? pick : String()) && i == wanted;

  if (!started || wanted != current || pickChanged || (overrideChanged && wanted == indexOf("ambient"))) {
    // A notification interrupts the mode on show; afterwards that mode
    // carries on where it was (its picture put back) instead of starting
    // over.
    static int resumeTo = -1;
    static uint8_t saved[ROWS][COLS];
    const int notify = indexOf("notify");
    const bool resume = started && current == notify && wanted == resumeTo && !overrideChanged;
    if (wanted == notify && started && current != notify) {
      resumeTo = current;
      for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) saved[y][x] = display.getLevel(x, y);
      }
    } else if (wanted != notify) {
      resumeTo = -1;
    }
    current = wanted;
    started = true;
    display.beginTransition();
    if (resume) {
      for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) display.setLevel(x, y, saved[y][x]);
      }
      display.render();
    } else {
      MODES[current]->start();
    }
  }
}

// A scene as the page's choice: its mode, and inside it the animation,
// game or drawing (settings.ambient / game / galleryShow); playlist off.
// Doesn't save nor switch.
static bool chooseScene(const String &scene) {
  String modeId, pick;
  if (!validScene(scene) || !sceneAvailable(scene) || !sceneTarget(scene, modeId, pick)) return false;
  if (modeId == "ambient") settings.ambient = pick;
  if (modeId == "games") settings.game = pick;
  if (modeId == "gallery") settings.galleryShow = pick;
  settings.mode = modeId;
  settings.playlistOn = false;
  return true;
}

bool showScene(const String &scene) {
  if (!chooseScene(scene)) return false;
  started = false;  // restart even if it is already shown
  evaluate(millis());
  return true;
}

String currentScene() {
  const Mode *m = MODES[current];
  const String id = m->id();
  if (id == "ambient" || id == "games") {
    const Animation *a = static_cast<const AmbientMode *>(m)->playing();
    if (a) return String(a->isGame() ? "g/" : "a/") + a->id();
  }
  if (id == "gallery" && galleryModeInstance.currentId().length()) return "d/" + galleryModeInstance.currentId();
  return id;
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
  PetMode::tickClock();  // the pet lives on while other modes are shown
  for (uint8_t i = 0; i < CREATURE_COUNT; i++) CREATURES[i]->tick();  // and so do the others
  // A notification arriving or ending is picked up at once.
  const bool notifyChanged = (NotifyMode::pending() > 0) != (strcmp(MODES[current]->id(), "notify") == 0);
  if (!started || now - lastCheck >= (notifyChanged ? 100u : 1000u)) {
    lastCheck = now;
    evaluate(now);
  }
  MODES[current]->update(now);
  display.tick(now);
}

void restartMode() { MODES[current]->start(); }
