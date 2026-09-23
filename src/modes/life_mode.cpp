#include "modes/life_mode.h"

#include "display.h"

static const uint32_t STEP_MS = 250;
static const uint16_t MAX_GENERATIONS = 1000;

void LifeMode::start() {
  seed();
  draw();
  lastStep_ = millis();
}

void LifeMode::update(uint32_t now) {
  if (now - lastStep_ < STEP_MS) return;
  lastStep_ = now;

  step();
  const uint32_t h = hash();
  bool stuck = generation_ >= MAX_GENERATIONS;
  for (uint32_t past : history_) stuck |= (past == h);
  if (stuck) {
    seed();
  } else {
    memmove(history_ + 1, history_, sizeof(history_) - sizeof(history_[0]));
    history_[0] = h;
  }
  draw();
}

void LifeMode::seed() {
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      cells_[y][x] = (esp_random() % 100) < 35;
    }
  }
  memset(previous_, 0, sizeof(previous_));
  memset(history_, 0, sizeof(history_));
  generation_ = 0;
}

void LifeMode::step() {
  bool next[ROWS][COLS];
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      int n = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          if ((dx || dy) && cells_[(y + dy + ROWS) % ROWS][(x + dx + COLS) % COLS]) n++;
        }
      }
      next[y][x] = n == 3 || (n == 2 && cells_[y][x]);
    }
  }
  memcpy(previous_, cells_, sizeof(cells_));
  memcpy(cells_, next, sizeof(cells_));
  generation_++;
}

void LifeMode::draw() {
  display.clear();
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      display.setLevel(x, y, cells_[y][x] ? 255 : previous_[y][x] ? 30 : 0);
    }
  }
  display.render();
}

// FNV-1a over the grid; an empty grid hashes to a fixed value, so a dead
// board is also caught as "same as last generation".
uint32_t LifeMode::hash() const {
  uint32_t h = 2166136261u;
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      h = (h ^ cells_[y][x]) * 16777619u;
    }
  }
  return h;
}
