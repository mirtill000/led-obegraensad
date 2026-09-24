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
// Font for scrolling text (everything drawn through drawScrollFrame):
//  Small - the proportional 8-row font (a-z, A-Z)
//  Big   - the same font doubled with EPX/Scale2x: 16 rows, the whole panel
//  Mini  - 5-row capitals, about 4 letters at a time
//  Compact - Small with letters one pixel narrower (font_compact.h); only
//            "Testo scorrevole" and "Dal web" ask for it, see Scroller
enum class TextFont : uint8_t { Small, Big, Mini, Compact };

// How the panel goes from one mode to the next (see beginTransition()).
//  None - straight cut
//  Fade - cross-fade from the old image to the new one
//  Wipe - the new image sweeps in from the left, with a soft edge
enum class Transition : uint8_t { None, Fade, Wipe };

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

  // Blends from what is on the panel now to the frames rendered next, in
  // the style set by setTransition(). Call it just before a new mode or
  // animation starts drawing; tick() keeps the blend moving even if the
  // new mode renders only once.
  void setTransition(Transition style) { transition_ = style; }
  void beginTransition();
  void tick(uint32_t now);

  // Level of logical pixel (x, y) as last sent to the panel (after any
  // transition), for the page's live preview.
  uint8_t shownLevel(int x, int y) const;

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
  // `compact`: the Small font becomes Compact (other fonts are unchanged).
  static int scrollWidth(const char *text, bool compact = false);
  static void setScrollFont(TextFont font);
  static TextFont scrollFont();
  // Rows of a line of scrolling text in the current font.
  static int scrollFontHeight();
  // Top row for one line of text at a position setting: "top", "middle",
  // "bottom", or "random" - a different height from `previous` each time
  // (at least 2 rows away).
  static int textRow(const String &position, int previous);
  // Clears, draws `text` scrolled left by `offset` pixels and renders. A
  // single line has its top at row `y` (0 = top edge, ROWS - FONT_HEIGHT
  // = bottom edge), or is centred when `y` is negative.
  void drawScrollFrame(const char *text, int offset, int y = -1, bool compact = false);
  // Blocking: scrolls `text` once from off-screen right to off-screen left.
  void scrollTextOnce(const char *text, uint16_t frameDelayMs);

  // Converts UTF-8 text (as typed on the web page) to the font's
  // single-byte characters: accented letters become the plain letter and
  // an apostrophe ("perché" -> "perche'", "È" -> "E'"), as a one-pixel
  // accent is hard to see; curly quotes become ', dashes (– —) become -, anything
  // else outside ASCII becomes a space.
  static String fontText(const String &utf8);

  // Width in pixels of text[start, end) (including trailing spacing).
  static int textWidth(const char *text, int start, int end);
  // Draws text[start, end) with its left edge at x.
  void drawText(int x, int y, const char *text, int start, int end);
  // Draws `rows` of a bitmap `width` pixels wide (bit 15 = leftmost).
  void drawBitmap(int x, int y, const uint16_t *bitmap, int width, int rows);

 private:
  // Text in any font (drawText/textWidth use the small one).
  static int textWidthIn(TextFont font, const char *text, int start, int end);
  void drawTextIn(TextFont font, int x, int y, const char *text, int start, int end);

  // Maps logical (x, y) to an index into frame_, applying flips and the
  // rotation; returns -1 when off-screen.
  int frameIndex(int x, int y) const;

  // Blends from_ into target_ (the last rendered frame_) into shown_ and
  // sends shown_ to the panel.
  void output();

  uint8_t frame_[TOTAL_PIXELS] = {0};   // being drawn
  uint8_t target_[TOTAL_PIXELS] = {0};  // last render()ed
  uint8_t from_[TOTAL_PIXELS] = {0};    // on the panel when a transition began
  uint8_t shown_[TOTAL_PIXELS] = {0};   // on the panel now
  Transition transition_ = Transition::Fade;
  bool blending_ = false;
  uint32_t blendStart_ = 0;
  uint32_t lastBlend_ = 0;
  uint16_t rotation_ = ROTATION_HORIZONTAL;
};

extern Display display;
