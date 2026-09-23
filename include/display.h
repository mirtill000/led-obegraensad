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
  void setPixel(int x, int y, bool on);
  // Draws glyph `c` with its top-left corner at (x, y); returns its width.
  int drawChar(int x, int y, char c);
  void render();

  // Blocking: scrolls `text` once from off-screen right to off-screen left.
  // A '|' in `text` splits it into two lines that scroll together.
  void scrollTextOnce(const char *text, uint16_t frameDelayMs);

 private:
  // Width in pixels of text[start, end).
  static int textWidth(const char *text, int start, int end);
  // Draws text[start, end) with its left edge at x.
  void drawText(int x, int y, const char *text, int start, int end);

  uint8_t frame_[TOTAL_PIXELS] = {0};
};

extern Display display;
