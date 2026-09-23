#include "modes.h"

#include "display.h"
#include "modes/ambient_mode.h"
#include "modes/clock_mode.h"
#include "modes/life_mode.h"
#include "modes/mario_mode.h"
#include "modes/off_mode.h"
#include "modes/quotes_mode.h"
#include "modes/text_mode.h"
#include "settings.h"
#include "timekeeping.h"

static TextMode textMode;
static QuotesMode quotesMode;
static ClockMode clockMode;
static LifeMode lifeMode;
static MarioMode marioMode;
static AmbientMode ambientMode;
static OffMode offMode;

Mode *const MODES[] = {&textMode, &quotesMode, &clockMode, &lifeMode, &marioMode, &ambientMode, &offMode};
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

bool validModeId(const String &id) { return indexOf(id) >= 0; }

Mode *currentMode() { return MODES[current]; }
bool isNight() { return night; }
int playlistPosition() { return settings.playlistOn ? playlistPos : -1; }

// Playlist item `n` of settings.playlist ("id:minutes,..."); false if there
// is no such (valid) item.
static bool playlistItem(int n, int &mode, uint32_t &minutes) {
  int start = 0;
  for (int i = 0; start <= (int)settings.playlist.length(); i++) {
    int end = settings.playlist.indexOf(',', start);
    if (end < 0) end = settings.playlist.length();
    if (i == n) {
      const String item = settings.playlist.substring(start, end);
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
  const uint16_t from = settings.nightStart, to = settings.nightEnd;
  return from <= to ? (now >= from && now < to) : (now >= from || now < to);  // may cross midnight
}

// Decides what to show now and switches to it if needed.
static void evaluate(uint32_t now) {
  night = inNightWindow();

  int wanted = indexOf(settings.mode);
  if (wanted < 0) wanted = 0;

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

  const uint8_t brightness = (night && settings.nightMode == "dim") ? settings.nightBrightness : settings.brightness;
  if (brightness != appliedBrightness) {
    display.setBrightness(brightness);
    appliedBrightness = brightness;
  }

  if (!started || wanted != current || (overrideChanged && wanted == indexOf("ambient"))) {
    current = wanted;
    started = true;
    MODES[current]->start();
  }
}

bool setMode(const String &id) {
  if (indexOf(id) < 0) return false;
  settings.mode = id;
  settings.playlistOn = false;
  started = false;  // restart even if it is already shown
  evaluate(millis());
  return true;
}

void nextMode() { setMode(MODES[(indexOf(settings.mode) + 1) % MODE_COUNT]->id()); }

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
}

void restartMode() { MODES[current]->start(); }
