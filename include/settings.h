#pragma once

#include <Arduino.h>

#include "display.h"

// User settings, kept in flash (NVS) so they survive a power cycle.
struct Settings {
  String mode;          // mode picked by the user, see modes.h
  String text;          // scrolling text (UTF-8)
  String textFont;      // font of all scrolling text: "small" (font A, the default), "big" or "mini"
  // How the scrolling text is shown: at a height - "top", "middle",
  // "bottom" or "random" (a different one at every pass) - or "pages" (still
  // pages of three lines, see Pager). settings.webPosition works the same.
  String textPosition;
  uint8_t brightness;   // 1-255
  bool vertical;        // how the lamp hangs: vertical or horizontal
  String transition;    // between modes: "fade", "wipe" or "none"

  // Weather location and time zone.
  float latitude;
  float longitude;
  String city;          // label only, e.g. "Milano"
  String timezone;      // POSIX TZ string used by the clock
  String timezoneName;  // IANA name (e.g. "Europe/Rome"), for the page

  String ambient;       // animation for the ambient mode, or "auto"
  String game;          // game for the Giochi mode, or "auto"

  // "Dal web" mode: which sources to show, the iCal link, and the height.
  bool infoWord;
  bool infoHistory;
  bool infoCalendar;
  String icalUrl;
  String webPosition;
  String quotes;        // one quote per line; empty = built-in list (see loadQuotes)
  String galleryShow;   // drawing shown by the "Disegni" mode, or "all"
  String demoStyle;     // "Demo" mode: "auto", "rows3", "pages" or "rows2"
  String gameStyle;     // all games: "soft" (shades of gray) or "crisp" (LEDs on/off)

  // Playlist: modes shown in turn, "id:minutes,id:minutes,...".
  bool playlistOn;
  String playlist;
  // Time slots: up to 4 "HHMM|brightness|id:min,..." separated by ';',
  // each with its own playlist and brightness (0 = the Display setting).
  bool scenesOn;
  String scenes;

  // Night: from nightStart to nightEnd (minutes after midnight) the lamp is
  // off ("off"), shows only stars ("stars") or is dimmed ("dim").
  // Games whose demo mode is switched off (comma-separated ids): there the
  // player controls the game from the page instead of the computer.
  String demoOff;

  // Timers: sunrise alarm.
  String formula;  // the Formule mode's expression (see formula.h)
  uint8_t hourglassMinutes;  // the sand timer's time (1-120)
  bool notifyNight;
  bool bleOn;        // Bluetooth remote control
  uint32_t blePin;   // its 6-digit pairing PIN          // show phone notifications during the night too
  bool alarmOn;
  uint16_t alarmTime;    // minutes after midnight
  uint8_t alarmDays;     // bit 0 = Monday ... bit 6 = Sunday
  uint8_t alarmRamp;     // minutes of sunrise before the alarm
  uint8_t alarmHold;     // minutes it stays bright after

  bool nightOn;
  bool nightSun;        // from sunset to sunrise instead of nightStart/End
  uint16_t nightStart;
  uint16_t nightEnd;
  String nightMode;
  uint8_t nightBrightness;

  // Brightness that follows the sun (see sunBrightness()): from
  // `brightness` with the sun high down to autoMin after dusk.
  bool autoBright;
  uint8_t autoMin;
};

extern Settings settings;

void loadSettings();
// Version of the saved settings' layout: loadSettings() migrates older ones.
static const uint8_t SETTINGS_VERSION = 3;
// Writes the settings that changed since they were last loaded or saved
// (each NVS write wears the flash: only the differences go).
void saveSettings();

// ---------------------------------------------------------------------------
// Every setting is described once, in SETTING_DEFS (settings.cpp): its name
// (in the page's JSON and in POST /api/settings), its NVS key, type, limits,
// default and what changing it affects. Loading, saving, validating, the
// page's state and the backup all go through this table.
enum class SettingType : uint8_t { Bool, U8, U16, U32, Float, Text };

// What a change touches besides the saved value (applied by the caller,
// see applySettingEffects() in web.cpp).
enum SettingEffect : uint16_t {
  FX_FONT = 1 << 0,        // the scrolling font
  FX_ROTATION = 1 << 1,    // the orientation
  FX_TRANSITION = 1 << 2,  // the transition style
  FX_MODES = 1 << 3,       // what is shown (night, alarm, brightness, ...)
  FX_RESTART = 1 << 4,     // restart def.mode if on show (any mode if none)
  FX_SHOW = 1 << 5,        // show def.mode
  FX_WEB = 1 << 6,         // fetch the web info again
  FX_WEATHER = 1 << 7,     // fetch the weather again
  FX_TIMEZONE = 1 << 8,    // switch the clock's time zone
  FX_PLAYLIST = 1 << 9,    // start the playlist over
  FX_REBOOT = 1 << 10,     // only takes effect after a restart
};

enum SettingFlag : uint8_t {
  SET_SHOW = 1,  // in the page's state ("settings")
  SET_WEB = 2,   // the page may change it with POST /api/settings
};

struct SettingDef {
  const char *name;     // in JSON and POST /api/settings
  const char *nvsKey;   // in NVS (kept from older firmware)
  SettingType type;
  void *field;          // in `settings`
  int32_t min, max;     // numbers; for text, max = the longest allowed
  const char *def;      // default, as text
  const char *choices;  // text: the allowed values "a|b|c", or nullptr
  uint8_t flags;
  uint16_t effects;
  const char *mode;     // for FX_RESTART / FX_SHOW
  // Extra check that may also tidy the value (e.g. a playlist); nullptr
  // if the limits are enough. Returns an Italian reason if it is refused.
  const char *(*clean)(String &value);
};
extern const SettingDef SETTING_DEFS[];
extern const uint8_t SETTING_COUNT;
const SettingDef *findSetting(const String &name);
// The value as text ("1"/"0" for booleans) and as a JSON literal.
String settingText(const SettingDef &def);
String settingJson(const SettingDef &def);
// Checks `value` against the definition (limits, choices, clean()); sets it
// if `apply`. Returns nullptr or the reason it was refused.
const char *setSetting(const SettingDef &def, const String &value, bool apply = true);
// {"name":value,...} of the settings marked SET_SHOW.
String settingsJson();

// Playlist and time slots in their tidy form; the number of valid items
// (0 = nothing usable).
int cleanPlaylist(const String &items, String &clean);
int cleanScenes(const String &scenes, String &clean);

// settings.quotes lives in its own file (/quotes.txt in LittleFS): the
// list can be longer than NVS allows for a string. loadQuotes() needs the
// file system mounted (galleryBegin()); saveQuotes() writes just that file.
static const size_t QUOTES_MAX = 16000;  // bytes
void loadQuotes();
bool saveQuotes();

// Font for scrolling text from settings.textFont.
TextFont fontForSettings();
// Whether games draw with shades of gray (settings.gameStyle "soft") or
// with LEDs only fully on or off ("crisp").
inline bool softGames() { return settings.gameStyle != "crisp"; }

// Brightness now: settings.brightness, or with autoBright on, between
// autoMin and it following the sun's height where the lamp is.
uint8_t sunBrightness();

// Transition style from settings.transition.
Transition transitionForSettings();
// Display rotation for the current orientation setting.
uint16_t rotationForSettings();

// Demo mode of a game ("mario", "tetris", "snake"): true (the default) =
// it plays by itself and ignores input.
bool demoMode(const char *gameId);
void setDemoMode(const char *gameId, bool demo);

// Per-mode speed, 1 (slowest) to 9 (fastest); 5 is each mode's default.
static const uint8_t SPEED_DEFAULT = 5;
uint8_t speedLevel(const char *modeId);
void setSpeedLevel(const char *modeId, uint8_t level);
// Scales a mode's base interval by its speed level: x4 slower at 1, x4
// faster at 9.
uint32_t scaledInterval(const char *modeId, uint32_t baseMs);
