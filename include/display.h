#pragma once

#include <Arduino.h>
#include "constants.h"

// Drives the OBEGRÄNSAD 16x16 panel: a single 256-bit shift-register chain
// (no row/column multiplexing), pushed out over hardware SPI.
//
// Drawing happens in a frame buffer of per-pixel levels (0 = off, 255 = full)
// and render() publishes it. With GRAYSCALE a timer keeps re-sending the
// frame as bit planes in the background to make the in-between levels; the
// registers hold their state, so without it a frame is only sent when it
// changes.
class Display {
 public:
  void begin();
  void clear();
  void setPixel(int x, int y, bool on) { setLevel(x, y, on ? 255 : 0); }
  bool getPixel(int x, int y) const { return getLevel(x, y) > 0; }
  // Brightness of one pixel, 0-255 (perceptual: 128 looks about half as
  // bright as 255). Without GRAYSCALE any level above 0 is fully on.
  void setLevel(int x, int y, uint8_t level);
  uint8_t getLevel(int x, int y) const;
  // Draws glyph `c` with its top-left corner at (x, y); returns its width.
  int drawChar(int x, int y, char c);
  void render();

  // Global brightness 1-255, done by PWM on the panel's output-enable pin.
  void setBrightness(uint8_t brightness);
  // Clockwise rotation of the image: 0, 90, 180 or 270. Takes effect from
  // the next frame drawn.
  void setRotation(uint16_t degrees) { rotation_ = degrees; }

  // A '|' in scrolling text splits it into two lines stacked on top of each
  // other that scroll together; without it one line scrolls through the
  // middle of the panel.
  //
  // Scroll length in pixels: offsets -COLS .. scrollWidth(text) - 1 take the
  // text from off-screen right to off-screen left.
  static int scrollWidth(const char *text);
  // Top row for one line of text at a position setting: "top", "middle",
  // "bottom", or "random" - a different height from `previous` each time
  // (at least 2 rows away).
  static int textRow(const String &position, int previous);
  // Clears, draws `text` scrolled left by `offset` pixels and renders. A
  // single line has its top at row `y` (0 = top edge, ROWS - FONT_HEIGHT
  // = bottom edge), or is centred when `y` is negative.
  void drawScrollFrame(const char *text, int offset, int y = -1);
  // Blocking: scrolls `text` once from off-screen right to off-screen left.
  void scrollTextOnce(const char *text, uint16_t frameDelayMs);

  // Converts UTF-8 text (as typed on the web page) to the font's
  // single-byte characters: Italian accented lowercase letters keep their
  // accent, accented capitals lose it, curly quotes become ', anything
  // else outside ASCII becomes a space.
  static String fontText(const String &utf8);

  // Width in pixels of text[start, end) (including trailing spacing).
  static int textWidth(const char *text, int start, int end);
  // Draws text[start, end) with its left edge at x.
  void drawText(int x, int y, const char *text, int start, int end);
  // Draws `rows` of a bitmap `width` pixels wide (bit 15 = leftmost).
  void drawBitmap(int x, int y, const uint16_t *bitmap, int width, int rows);

 private:
  // Maps logical (x, y) to an index into frame_, applying flips and the
  // rotation; returns -1 when off-screen.
  int frameIndex(int x, int y) const;

  uint8_t frame_[TOTAL_PIXELS] = {0};
  uint16_t rotation_ = ROTATION_HORIZONTAL;
};

extern Display display;
