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
  bool getPixel(int x, int y) const;
  // Draws glyph `c` with its top-left corner at (x, y); returns its width.
  int drawChar(int x, int y, char c);
  void render();

  // Global brightness 1-255, done by PWM on the panel's output-enable pin.
  void setBrightness(uint8_t brightness);

  // A '|' in scrolling text splits it into two lines stacked on top of each
  // other that scroll together; without it one line scrolls through the
  // middle of the panel.
  //
  // Scroll length in pixels: offsets -COLS .. scrollWidth(text) - 1 take the
  // text from off-screen right to off-screen left.
  static int scrollWidth(const char *text);
  // Clears, draws `text` scrolled left by `offset` pixels and renders.
  void drawScrollFrame(const char *text, int offset);
  // Blocking: scrolls `text` once from off-screen right to off-screen left.
  void scrollTextOnce(const char *text, uint16_t frameDelayMs);

 private:
  // Width in pixels of text[start, end).
  static int textWidth(const char *text, int start, int end);
  // Draws text[start, end) with its left edge at x.
  void drawText(int x, int y, const char *text, int start, int end);
  // Maps logical (x, y) to an index into frame_, applying flips and
  // ROTATION; returns -1 when off-screen.
  static int frameIndex(int x, int y);

  uint8_t frame_[TOTAL_PIXELS] = {0};
};

extern Display display;
