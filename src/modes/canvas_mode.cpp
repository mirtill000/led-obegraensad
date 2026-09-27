#include "modes/canvas_mode.h"

#include <Preferences.h>
#include <string.h>

#include "display.h"
#include "modes/life_mode.h"
#include "settings.h"

static uint8_t canvas[ROWS * COLS];
static bool loaded = false, dirty = false;
static uint32_t changeVersion = 0, lastChange = 0, lastDraw = 0;
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

static void draw(uint32_t now) {
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

void CanvasMode::start() {
  load();
  display.beginTransition();
  draw(millis());
}

void CanvasMode::update(uint32_t now) {
  if (now - lastDraw < 50) return;
  lastDraw = now;
  saveIfChanged();
  draw(now);
}

bool CanvasMode::input(char key) {
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

const GameControls *CanvasMode::controls() const {
  static const GameControls c = {"LRUDA", {nullptr, nullptr, nullptr, nullptr, "Punto"}, true,
                                 "Le frecce muovono il cursore, «Punto» (spazio) accende o spegne il pixel sotto."};
  return &c;
}

void CanvasMode::paint(int x, int y, uint8_t level) {
  load();
  if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
  if (canvas[y * COLS + x] == level) return;
  canvas[y * COLS + x] = level;
  changed();
}

void CanvasMode::clear() {
  load();
  memset(canvas, 0, sizeof(canvas));
  changed();
}

const uint8_t *CanvasMode::pixels() {
  load();
  return canvas;
}

uint32_t CanvasMode::version() { return changeVersion; }

void CanvasMode::toLife() {
  load();
  bool cells[ROWS][COLS];
  for (int i = 0; i < ROWS * COLS; i++) cells[i / COLS][i % COLS] = canvas[i] > 0;
  LifeMode::seedWith(cells);
  saveIfChanged();
  setMode("life");  // starts it, from the drawing
  saveSettings();
}

void CanvasMode::saveIfChanged() {
  if (!dirty || millis() - lastChange < SAVE_AFTER_MS) return;
  Preferences prefs;
  prefs.begin("obegransad", false);
  prefs.putBytes("canvas", canvas, sizeof(canvas));
  prefs.end();
  dirty = false;
}
