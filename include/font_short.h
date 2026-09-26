#pragma once

#include "font_compact.h"

// Short variant of the text font: the same look as "Attuale" (font A) but 6
// rows tall instead of 8, used for scrolling text and pages when the lamp
// hangs vertically. Capitals, ascenders and x-height letters already fit in
// rows 0-5 of the compact font, so they are reused as they are (only rows
// 0-5 are drawn); the letters whose tail dips into rows 6-7 - g j p q y and
// the comma and semicolon - are redrawn here with the tail folded up so it
// sits on the baseline (like the small clocks' fonts), keeping lowercase.
#define SHORT_HEIGHT 6

static const Glyph SHORT_GLYPHS[] = {
    {'g', 3, {0x00, 0x60, 0xA0, 0x60, 0x20, 0xC0, 0x00, 0x00}},
    {'p', 3, {0x00, 0xC0, 0xA0, 0xC0, 0x80, 0x80, 0x00, 0x00}},
    {'q', 3, {0x00, 0x60, 0xA0, 0x60, 0x20, 0x20, 0x00, 0x00}},
    {'y', 3, {0x00, 0xA0, 0xA0, 0x60, 0x20, 0xC0, 0x00, 0x00}},
    {'j', 2, {0x40, 0x00, 0x40, 0x40, 0x40, 0x80, 0x00, 0x00}},
    {',', 2, {0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00}},
    {';', 2, {0x00, 0x40, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00}},
};

// The 6-row glyph for `c`: a folded descender where one exists, otherwise
// the top six rows of the compact glyph.
inline const Glyph *findShortGlyph(char c) {
  for (const Glyph &g : SHORT_GLYPHS) {
    if (g.c == c) return &g;
  }
  return findCompactGlyph(c);
}
