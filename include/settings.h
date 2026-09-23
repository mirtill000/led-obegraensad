#pragma once

#include <Arduino.h>

// User settings, kept in flash (NVS) so they survive a power cycle.
struct Settings {
  String mode;          // id of the active mode, see modes.h
  String text;          // scrolling text; '|' splits it into two lines
  uint8_t brightness;   // 1-255
  uint16_t speedMs;     // delay between scroll steps
};

extern Settings settings;

void loadSettings();
void saveSettings();
