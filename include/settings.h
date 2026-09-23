#pragma once

#include <Arduino.h>

// User settings, kept in flash (NVS) so they survive a power cycle.
struct Settings {
  String mode;          // mode picked by the user, see modes.h
  String text;          // scrolling text (UTF-8)
  // Height of the scrolling text and of the hourly quote: "top", "middle",
  // "bottom" or "random" (a different height at every pass).
  String textPosition;
  String quotesPosition;
  uint8_t brightness;   // 1-255
  bool vertical;        // how the lamp hangs: vertical or horizontal

  // Weather location and time zone.
  float latitude;
  float longitude;
  String city;          // label only, e.g. "Milano"
  String timezone;      // POSIX TZ string used by the clock
  String timezoneName;  // IANA name (e.g. "Europe/Rome"), for the page

  String ambient;       // animation for the ambient mode, or "auto"

  // "Dal web" mode: which sources to show, the iCal link, and the height.
  bool infoWord;
  bool infoHistory;
  bool infoCalendar;
  String icalUrl;
  String webPosition;
  String quotes;        // one quote per line; empty = built-in list
  String galleryShow;   // drawing shown by the "Disegni" mode, or "all"

  // Playlist: modes shown in turn, "id:minutes,id:minutes,...".
  bool playlistOn;
  String playlist;

  // Night: from nightStart to nightEnd (minutes after midnight) the lamp is
  // off ("off"), shows only stars ("stars") or is dimmed ("dim").
  // Games whose demo mode is switched off (comma-separated ids): there the
  // player controls the game from the page instead of the computer.
  String demoOff;

  // Timers: Pomodoro lengths (minutes), countdown target, sunrise alarm.
  uint8_t pomodoroWork;
  uint8_t pomodoroBreak;
  String countdownLabel;
  String countdownDate;  // "YYYY-MM-DD"
  String countdownTime;  // "HH:MM"
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
};

extern Settings settings;

void loadSettings();
void saveSettings();

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
