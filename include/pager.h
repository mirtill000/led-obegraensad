#pragma once

#include <Arduino.h>

#include <vector>

// A text shown as still pages of up to three lines in the 4-row Tiny font
// (font_tiny.h): each line up to 16 pixels, words kept whole where they fit
// and split by Italian syllables where they don't (CON / SAPE / VO / LEZ /
// ZA), every line centred and the block centred vertically. Used by "Frase dell'ora" and the Demo mode.
// Time each page stays, before the speed setting scales it.
static const uint32_t PAGE_MS = 2500;

class Pager {
 public:
  // `text` is UTF-8; draws the first page (with the mode transition).
  void start(const String &text);
  // Moves to the next page every `pageMs`; returns true once the last page
  // has had its time (the text is over; call start() again for another).
  bool update(uint32_t now, uint32_t pageMs);

  // Width of Tiny-font text, with one blank column between glyphs.
  static int textWidth(const String &text);
  // Draws Tiny-font text (already in the font's characters) at (x, y).
  static void drawText(int x, int y, const String &text);

 private:
  void draw();

  std::vector<String> lines_;
  size_t page_ = 0;
  uint32_t lastStep_ = 0;
};
