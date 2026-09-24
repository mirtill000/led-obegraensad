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

int headerPeriod(const String &text) { return miniWidth(text) + HEADER_GAP; }

void header(const String &text, int offset, const String &next) {
  mini(-offset, 0, text);
  mini(headerPeriod(text) - offset, 0, next);
}

void headerLoop(const String &text, uint32_t now) {
  header(text, (now / HEADER_STEP_MS) % headerPeriod(text), text);
}

void waiting(uint32_t now, int y) {
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
