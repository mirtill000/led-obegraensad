#include "modes/hourglass_mode.h"

#include <math.h>

#include "display.h"
#include "settings.h"

// The glass, row by row: the columns inside it (LO-HI), symmetric top to
// bottom; the neck is rows 7-8, x7-8. Rows 0 and 15 are the wooden caps.
//
//   row 1-2 x2-13 | 3 x3-12 | 4 x4-11 | 5 x5-10 | 6 x6-9 | 7-8 x7-8 (neck)
static const int8_t LO[ROWS] = {99, 2, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 2, 99};
static const int NECK_TOP = 7;  // grains enter this row only when let through
static const int GRAINS = 44;
static const uint32_t STEP_MS = 40;
static const uint32_t PULSE_MS = 6000;  // the sand pulses this long at the end

static bool sand[ROWS][COLS];
static uint32_t lastStep = 0, lastRelease = 0, doneAt = 0;
static bool done = false;

static bool inside(int x, int y) { return y > 0 && y < ROWS - 1 && x >= LO[y] && x <= COLS - 1 - LO[y]; }
static bool freeCell(int x, int y) { return inside(x, y) && !sand[y][x]; }

static uint32_t releaseMs() { return settings.hourglassMinutes * 60000UL / GRAINS; }

static int grainsOnTop() {
  int n = 0;
  for (int y = 1; y < NECK_TOP; y++) {
    for (int x = 0; x < COLS; x++) n += sand[y][x];
  }
  return n;
}

// One step of the falling-sand automaton, from the bottom up: a grain
// falls if the cell below is free, else slides down-left or down-right
// (in random order). It never enters the neck on its own. True if anything
// moved.
static bool fall() {
  bool moved = false;
  for (int y = ROWS - 3; y >= 1; y--) {
    const bool leftFirst = esp_random() & 1;
    for (int i = 0; i < COLS; i++) {
      const int x = leftFirst ? i : COLS - 1 - i;
      if (!sand[y][x]) continue;
      if (y + 1 == NECK_TOP) continue;  // waits for the neck
      int nx = -1;
      if (freeCell(x, y + 1)) {
        nx = x;
      } else {
        const int a = (esp_random() & 1) ? -1 : 1;
        if (freeCell(x + a, y + 1)) nx = x + a;
        else if (freeCell(x - a, y + 1)) nx = x - a;
      }
      if (nx < 0) continue;
      sand[y][x] = false;
      sand[y + 1][nx] = true;
      moved = true;
    }
  }
  return moved;
}

// Lets one grain from the bottom of the top bulb into the neck: the one
// right above it if there is one, so the crater opens in the middle.
static bool release() {
  const int y = NECK_TOP - 1;
  const int mid = (esp_random() & 1) ? 7 : 8, other = 15 - mid;
  const int order[4] = {mid, other, mid - (mid == 7 ? 1 : -1), other - (other == 7 ? 1 : -1)};  // 7 8 6 9
  for (int x : order) {
    if (!sand[y][x]) continue;
    for (int nx = x - 1; nx <= x + 1; nx++) {
      if (freeCell(nx, NECK_TOP)) {
        sand[y][x] = false;
        sand[NECK_TOP][nx] = true;
        return true;
      }
    }
  }
  return false;
}

static void draw(uint32_t now) {
  display.clear();
  for (int x = 1; x < COLS - 1; x++) {
    display.setLevel(x, 0, 90);
    display.setLevel(x, ROWS - 1, 90);
  }
  for (int y = 1; y < ROWS - 1; y++) {
    display.setLevel(LO[y] - 1, y, 45);
    display.setLevel(COLS - LO[y], y, 45);
  }
  uint8_t level = 255;
  if (done && now - doneAt < PULSE_MS) {
    level = 90 + (uint8_t)(165 * (0.5f + 0.5f * cosf((now - doneAt) * 2 * (float)M_PI / 1000)));
  }
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (sand[y][x]) display.setLevel(x, y, level);
    }
  }
  display.render();
}

void HourglassMode::start() {
  memset(sand, 0, sizeof(sand));
  // Fill the top bulb from the neck up; the last row partly, centred.
  int left = GRAINS;
  for (int y = NECK_TOP - 1; y >= 1 && left > 0; y--) {
    const int width = COLS - 2 * LO[y];
    const int n = min(width, left);
    const int from = LO[y] + (width - n) / 2;
    for (int x = from; x < from + n; x++) sand[y][x] = true;
    left -= n;
  }
  lastStep = lastRelease = millis();
  done = false;
  display.beginTransition();
  draw(lastStep);
}

void HourglassMode::action() {
  // Turn it over: the glass is symmetric, so just mirror the sand.
  for (int y = 0; y < ROWS / 2; y++) {
    for (int x = 0; x < COLS; x++) {
      const bool t = sand[y][x];
      sand[y][x] = sand[ROWS - 1 - y][x];
      sand[ROWS - 1 - y][x] = t;
    }
  }
  lastRelease = millis();
  done = false;
  display.beginTransition();
  draw(lastRelease);
}

void HourglassMode::update(uint32_t now) {
  if (now - lastStep < STEP_MS) return;
  lastStep = now;
  if (!done && now - lastRelease >= releaseMs()) {
    if (release() || grainsOnTop() == 0) lastRelease = now;
  }
  const bool moved = fall();
  // Run out: nothing on top and the last grain has come to rest.
  if (!done && !moved && grainsOnTop() == 0) {
    bool neck = false;
    for (int x = 0; x < COLS; x++) neck |= sand[NECK_TOP][x];
    if (!neck) {
      done = true;
      doneAt = now;
    }
  }
  draw(now);
}

uint32_t HourglassMode::secondsLeft() {
  if (done) return 0;
  const uint32_t since = millis() - lastRelease;
  const uint32_t each = releaseMs();
  const int top = grainsOnTop();
  if (top == 0) return 0;
  return ((uint64_t)top * each - min(since, each)) / 1000;
}

bool HourglassMode::running() { return !done; }
