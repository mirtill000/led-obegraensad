#pragma once

#include <Arduino.h>

#include "display.h"

#include <vector>

// A text shown as still pages in the font picked on the page (Display's
// scroll font): 3 lines per page in the Tiny font, 2 in Attuale (its compact
// letters) or Mini; each line up to 16 pixels, words kept whole where they
// fit and split by Italian syllables where they don't (CON / SAPE / VO /
// LEZ / ZA), every line centred and the block centred vertically. The Big
// font can't make pages (two letters each), so with it the text scrolls
// instead. Used by Frase dell'ora, Testo scorrevole and Dal web (as
// pages), the notifications and the Demo mode.
// Time each page stays, before the speed setting scales it.
static const uint32_t PAGE_MS = 2500;

class Pager {
 public:
  // `text` is UTF-8; draws the first page (with the mode transition).
  void start(const String &text);
  // The same in a given font instead of the page's choice.
  void start(const String &text, TextFont font);
  // Moves to the next page every `pageMs`; returns true once the last page
  // has had its time (the text is over; call start() again for another).
  bool update(uint32_t now, uint32_t pageMs);

  // Width of Tiny-font text, with one blank column between glyphs.
  static int textWidth(const String &text);
  // Draws Tiny-font text (already in the font's characters) at (x, y).
  static void drawText(int x, int y, const String &text);

 private:
  void draw();

  TextFont font_ = TextFont::Tiny;
  std::vector<String> lines_;
  size_t page_ = 0;
  uint32_t lastStep_ = 0;
  // Big font: the text scrolls.
  String text_;
  int offset_ = 0, width_ = 0;
};
