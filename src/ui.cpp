#include "ui.h"

#include <WiFi.h>

#include "display.h"
#include "font_mini.h"

namespace ui {

int mini(int x, int y, const String &text, uint8_t level) {
  for (unsigned i = 0; i < text.length(); i++) {
    const MiniGlyph *g = findMiniGlyph(text[i]);
    if (x + g->width >= 0 && x < COLS) {
      for (int r = 0; r < MINI_HEIGHT; r++) {
        for (int c = 0; c < g->width; c++) {
          if (g->rows[r] & (0x80 >> c)) display.setLevel(x + c, y + r, level);
        }
      }
    }
    x += g->width + 1;
  }
  return x;
}

int miniWidth(const String &text) {
  int w = 0;
  for (unsigned i = 0; i < text.length(); i++) w += findMiniGlyph(text[i])->width + 1;
  return max(0, w - 1);
}

void textHeaderLoop(const String &text, uint32_t now) {
  // The compact letters, like all scrolling text.
  const String t = Display::fontText(text);
  const int width = Display::textWidthIn(TextFont::Compact, t.c_str(), 0, t.length());
  const int period = width + HEADER_GAP;
  const int offset = (now / HEADER_STEP_MS) % period;
  display.drawTextIn(TextFont::Compact, -offset, 0, t.c_str(), 0, t.length());
  display.drawTextIn(TextFont::Compact, period - offset, 0, t.c_str(), 0, t.length());
}

void icon(int x, int y, const char *const *rows, int count, uint8_t level) {
  for (int r = 0; r < count; r++) {
    for (int c = 0; rows[r][c]; c++) {
      const char ch = rows[r][c];
      if (ch == '#') display.setLevel(x + c, y + r, level);
      else if (ch == '+') display.setLevel(x + c, y + r, LEVEL_FULL);
      else if (ch == ':') display.setLevel(x + c, y + r, level / 3);
    }
  }
}

void label(const String &text, int y, uint8_t level, uint32_t t) {
  const String plain = Display::fontText(text);
  String s;
  for (unsigned k = 0; k < plain.length(); k++) s += (char)toupper((uint8_t)plain[k]);  // capitals only
  const int w = Display::textWidthIn(TextFont::Tiny, s.c_str(), 0, s.length());
  if (w <= COLS + 1) {
    display.drawTextIn(TextFont::Tiny, (COLS + 1 - w) / 2, y, s.c_str(), 0, s.length());
  } else {
    const int period = w + 6;  // a gap before it comes round again
    const int offset = (int)(t / 90) % period;
    display.drawTextIn(TextFont::Tiny, -offset, y, s.c_str(), 0, s.length());
    display.drawTextIn(TextFont::Tiny, period - offset, y, s.c_str(), 0, s.length());
  }
  // The font draws at full brightness: bring its rows down to `level`.
  for (int r = y; r < y + Display::fontHeightOf(TextFont::Tiny); r++) {
    for (int x = 0; x < COLS; x++) {
      if (display.getLevel(x, r)) display.setLevel(x, r, level);
    }
  }
}

void value(const String &text, int y, uint8_t level) {
  const int w = Display::textWidthIn(TextFont::Short, text.c_str(), 0, text.length());
  display.drawTextIn(TextFont::Short, (COLS + 1 - w) / 2, y, text.c_str(), 0, text.length());
  if (level == LEVEL_FULL) return;
  for (int r = y; r < y + Display::fontHeightOf(TextFont::Short); r++) {
    for (int x = 0; x < COLS; x++) {
      if (display.getLevel(x, r)) display.setLevel(x, r, level);
    }
  }
}

void card(const String &title, const String &reading, const String &caption, uint32_t t) {
  label(title, 0, LEVEL_DIM, t);
  value(reading, 5);
  label(caption, 12, LEVEL_DIM + 50, t);
}

void waiting(uint32_t now) {
  const int y = WAITING_ROW;
  if (WiFi.status() != WL_CONNECTED) {
    // Fan of arcs, 7x5, blinking once a second.
    static const char *WIFI[] = {".#####.", "#.....#", "..###..", ".#...#.", "...#..."};
    if ((now / 500) % 2) return;
    for (int r = 0; r < 5; r++) {
      for (int c = 0; c < 7; c++) {
        if (WIFI[r][c] == '#') display.setLevel(5 + c, y - 2 + r, 255);
      }
    }
    return;
  }
  // ".", "..", "...", then a pause with none, all at full brightness.
  const int lit = (now / 400) % 4;
  for (int i = 0; i < lit; i++) display.setLevel(5 + i * 3, y, 255);
}

}  // namespace ui
