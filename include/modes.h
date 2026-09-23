#pragma once

#include <Arduino.h>

#include "settings.h"

// A "mode" is one of the things the lamp can show (scrolling text, Game of
// Life, ...). Exactly one is shown at a time; its update() is called on
// every loop() and must return quickly - no delay() - so the web server
// stays responsive. Keep your own timers with `now` (millis()).
class Mode {
 public:
  virtual const char *id() const = 0;    // stable, used in URLs and NVS
  virtual const char *name() const = 0;  // shown on the web page
  virtual void start() {}                // called when the mode is shown
  virtual void update(uint32_t now) = 0;

  // Optional command shown as a button on the web page while this mode is
  // shown (e.g. "next quote"); nullptr for none.
  virtual const char *actionName() const { return nullptr; }
  virtual void action() {}

  // Whether the page offers a speed slider for this mode.
  virtual bool hasSpeed() const { return true; }

  // Game controls from the page: 'L', 'R', 'U', 'D' (arrows) or 'A' (the
  // main button: jump / drop). Returns false if nothing is listening.
  virtual bool input(char) { return false; }
  // The game being shown and whether it is in demo mode, or nullptr.
  virtual const char *gameId() const { return nullptr; }

 protected:
  // `baseMs` adjusted for this mode's speed setting.
  uint32_t interval(uint32_t baseMs) const { return scaledInterval(id(), baseMs); }
};

// To add a mode: implement Mode in src/modes/, then list it in MODES in
// src/modes.cpp. The web page picks it up automatically.
extern Mode *const MODES[];
extern const uint8_t MODE_COUNT;

// What is shown is decided in this order: the night schedule (lamp off or
// stars only), then the playlist if it is on, then the mode the user
// picked (settings.mode).
Mode *currentMode();
bool isNight();
// Position in the playlist of the item being shown, or -1.
int playlistPosition();

// The user picks a mode (no-op if unknown); this stops the playlist.
bool setMode(const String &id);
void nextMode();
// Restarts the playlist from its first item (after it changed or was
// switched on).
void restartPlaylist();
// Re-applies settings that affect what is shown (night schedule, playlist,
// brightness) right away instead of within a second.
void refreshModes();
void updateMode();
// Restarts the shown mode, e.g. after its settings changed.
void restartMode();

bool validModeId(const String &id);
