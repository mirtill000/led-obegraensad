#include "modes/board.h"

#include <Preferences.h>
#include <string.h>

#include "display.h"

namespace board {

static uint8_t canvas[ROWS * COLS];
static bool loaded = false, dirty = false;
static uint32_t changeVersion = 0, lastChange = 0;
static int cursorX = 7, cursorY = 7;
static uint32_t cursorUntil = 0;  // the cursor shows for a while after a key
static const uint32_t CURSOR_MS = 6000, SAVE_AFTER_MS = 4000;

static void load() {
  if (loaded) return;
  loaded = true;
  Preferences prefs;
  prefs.begin("obegransad", true);
  if (prefs.getBytesLength("canvas") == sizeof(canvas)) prefs.getBytes("canvas", canvas, sizeof(canvas));
  prefs.end();
}

static void changed() {
  dirty = true;
  lastChange = millis();
  changeVersion++;
}

void paint(int x, int y, uint8_t level) {
  load();
  if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
  if (canvas[y * COLS + x] == level) return;
  canvas[y * COLS + x] = level;
  changed();
}

void clear() {
  load();
  memset(canvas, 0, sizeof(canvas));
  changed();
}

const uint8_t *pixels() {
  load();
  return canvas;
}

uint32_t version() { return changeVersion; }

bool input(char key) {
  load();
  switch (key) {
    case 'L': cursorX = (cursorX + COLS - 1) % COLS; break;
    case 'R': cursorX = (cursorX + 1) % COLS; break;
    case 'U': cursorY = (cursorY + ROWS - 1) % ROWS; break;
    case 'D': cursorY = (cursorY + 1) % ROWS; break;
    case 'A': {
      uint8_t &p = canvas[cursorY * COLS + cursorX];
      p = p ? 0 : 255;
      changed();
      break;
    }
    default: return false;
  }
  cursorUntil = millis() + CURSOR_MS;
  return true;
}

void draw(uint32_t now) {
  load();
  display.clear();
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) display.setLevel(x, y, canvas[y * COLS + x]);
  }
  if ((int32_t)(cursorUntil - now) > 0 && (now / 250) % 2) {
    // Blinking: dim over a lit pixel, bright over a dark one.
    display.setLevel(cursorX, cursorY, canvas[cursorY * COLS + cursorX] > 128 ? 40 : 200);
  }
  display.render();
}

void saveIfChanged() {
  if (!dirty || millis() - lastChange < SAVE_AFTER_MS) return;
  Preferences prefs;
  prefs.begin("obegransad", false);
  prefs.putBytes("canvas", canvas, sizeof(canvas));
  prefs.end();
  dirty = false;
}

}  // namespace board
