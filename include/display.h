#pragma once

#include <Arduino.h>
#include "constants.h"

// Drives the OBEGRÄNSAD 16x16 panel: a single 256-bit shift-register chain
// (no row/column multiplexing), pushed out over hardware SPI. Because the
// registers hold their state, a frame only needs to be re-sent when it
// changes - there is no continuous refresh loop.
class Display {
 public:
  void begin();
  void clear();
  void setPixel(uint8_t x, uint8_t y, bool on);
  void drawChar(int x, int y, char c);
  void render();

  // Blocking: scrolls `text` once from off-screen right to off-screen left.
  void scrollTextOnce(const char *text, uint16_t frameDelayMs);

 private:
  uint8_t frame_[TOTAL_PIXELS] = {0};
};

extern Display display;
