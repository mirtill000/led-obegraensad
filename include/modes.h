#pragma once

#include <Arduino.h>

// A "mode" is one of the things the lamp can show (scrolling text, Game of
// Life, ...). Exactly one is active at a time; its update() is called on
// every loop() and must return quickly - no delay() - so the web server
// stays responsive. Keep your own timers with `now` (millis()).
class Mode {
 public:
  virtual const char *id() const = 0;    // stable, used in URLs and NVS
  virtual const char *name() const = 0;  // shown on the web page
  virtual void start() {}                // called when the mode is selected
  virtual void update(uint32_t now) = 0;
};

// To add a mode: implement Mode in src/modes/, then list it in MODES in
// src/modes.cpp. The web page picks it up automatically.
extern Mode *const MODES[];
extern const uint8_t MODE_COUNT;

Mode *currentMode();
// Switches to the mode with this id (no-op if unknown); returns success.
bool setMode(const String &id);
void nextMode();
void updateMode();
// Restarts the active mode, e.g. after its settings changed.
void restartMode();
