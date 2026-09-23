#include "modes/life_mode.h"

#include "display.h"

static const uint32_t STEP_MS = 250;
static const uint32_t SEED_PAUSE_MS = 1000;  // show the starting cells before evolving

// Small starting patterns that take a long time to settle. On this 16x16
// torus they live for about 40-150 generations.
struct Seed {
  uint8_t width, height;
  const char *rows;  // '#' = alive, row after row
};
static const Seed SEEDS[] = {
    {3, 3, ".##" "##." ".#."},                      // R-pentomino
    {7, 3, ".#....." "...#..." "##..###"},          // acorn
    {4, 3, "#.##" "###." ".#.."},                   // B-heptomino
    {3, 3, "###" "#.#" "#.#"},                      // pi-heptomino
    {8, 3, "......#." "##......" ".#...###"},       // diehard
    {3, 5, "###" "..." ".#." ".#." ".#."},          // thunderbird
};
static const uint16_t MAX_GENERATIONS = 1000;

void LifeMode::start() {
  seed();
  draw();
  lastStep_ = millis() + SEED_PAUSE_MS;
}

void LifeMode::update(uint32_t now) {
  if ((int32_t)(now - lastStep_) < (int32_t)interval(STEP_MS)) return;
  lastStep_ = now;

  step();
  const uint32_t h = hash();
  bool stuck = generation_ >= MAX_GENERATIONS;
  for (uint32_t past : history_) stuck |= (past == h);
  if (stuck) {
    seed();
    lastStep_ = now + SEED_PAUSE_MS;
  } else {
    memmove(history_ + 1, history_, sizeof(history_) - sizeof(history_[0]));
    history_[0] = h;
  }
  draw();
}

// Empty board plus one random seed pattern, randomly rotated and mirrored,
// in the middle.
void LifeMode::seed() {
  memset(cells_, 0, sizeof(cells_));
  const Seed &s = SEEDS[esp_random() % (sizeof(SEEDS) / sizeof(SEEDS[0]))];
  const int turns = esp_random() % 4;
  const bool mirror = esp_random() & 1;
  const int w = (turns % 2) ? s.height : s.width;
  const int h = (turns % 2) ? s.width : s.height;
  for (int y = 0; y < s.height; y++) {
    for (int x = 0; x < s.width; x++) {
      if (s.rows[y * s.width + x] != '#') continue;
      int rx = mirror ? s.width - 1 - x : x, ry = y;
      for (int t = 0; t < turns; t++) {  // rotate 90 degrees clockwise
        const int nx = (t % 2 ? s.width : s.height) - 1 - ry;
        ry = rx;
        rx = nx;
      }
      cells_[(ROWS - h) / 2 + ry][(COLS - w) / 2 + rx] = true;
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
