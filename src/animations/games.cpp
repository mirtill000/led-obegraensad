// Games that play themselves: Tetris and Snake.
#include "animation.h"
#include "display.h"

// ---------------------------------------------------------------------------
// Tetris: a 10x16 well in the middle of the panel. For every new piece the
// computer tries each rotation and column and picks the placement with the
// best score (low stack, few holes, even surface, cleared lines), then
// moves the piece there in view as it falls.
// ---------------------------------------------------------------------------
class TetrisAnimation : public Animation {
 public:
  const char *id() const override { return "tetris"; }
  const char *name() const override { return "Tetris"; }
  const char *group() const override { return "Giochi"; }
  uint16_t frameMs() const override { return 60; }

  void start() override {
    memset(board_, 0, sizeof(board_));
    phase_ = DROPPING;
    spawn();
  }

  void frame(uint32_t) override {
    tick_++;
    switch (phase_) {
      case DROPPING: fall(); break;
      case CLEARING:
        if (++phaseFrames_ >= 8) {
          removeFullRows();
          spawn();
        }
        break;
      case GAME_OVER:
        if (++phaseFrames_ >= 30) start();
        break;
    }
    draw();
  }

 private:
  static const int W = 10, H = ROWS, LEFT = 3;  // well at columns 3-12
  enum Phase { DROPPING, CLEARING, GAME_OVER };
  struct Cell {
    int8_t x, y;
  };

  // The 7 tetrominoes, 4 cells each in a 4x4 box (rotation 0).
  static const Cell *shape(int piece) {
    static const Cell SHAPES[7][4] = {
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},  // I
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},  // O
        {{1, 0}, {0, 1}, {1, 1}, {2, 1}},  // T
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}},  // S
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}},  // Z
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},  // J
        {{2, 0}, {0, 1}, {1, 1}, {2, 1}},  // L
    };
    return SHAPES[piece];
  }

  // Cells of `piece` in `rotation`, shifted so the top-left is at (0, 0).
  static void cells(int piece, int rotation, Cell out[4]) {
    int minX = 9, minY = 9;
    for (int i = 0; i < 4; i++) {
      int x = shape(piece)[i].x, y = shape(piece)[i].y;
      for (int r = 0; r < rotation; r++) {  // rotate clockwise in the 4x4 box
        const int t = x;
        x = 3 - y;
        y = t;
      }
      out[i] = {(int8_t)x, (int8_t)y};
      minX = min(minX, x);
      minY = min(minY, y);
    }
    for (int i = 0; i < 4; i++) {
      out[i].x -= minX;
      out[i].y -= minY;
    }
  }

  static bool fits(const uint8_t b[H][W], int piece, int rotation, int px, int py) {
    Cell c[4];
    cells(piece, rotation, c);
    for (const Cell &k : c) {
      const int x = px + k.x, y = py + k.y;
      if (x < 0 || x >= W || y >= H) return false;
      if (y >= 0 && b[y][x]) return false;
    }
    return true;
  }

  // Score of the board after dropping `piece` at (rotation, column); false
  // if it doesn't fit at all.
  static bool evaluate(const uint8_t b[H][W], int piece, int rotation, int px, float &score) {
    if (!fits(b, piece, rotation, px, 0)) return false;
    int py = 0;
    while (fits(b, piece, rotation, px, py + 1)) py++;
    uint8_t t[H][W];
    memcpy(t, b, sizeof(t));
    Cell c[4];
    cells(piece, rotation, c);
    for (const Cell &k : c) t[py + k.y][px + k.x] = 1;

    int lines = 0;
    for (int y = 0; y < H; y++) {
      bool full = true;
      for (int x = 0; x < W; x++) full &= t[y][x] != 0;
      if (full) {
        lines++;
        for (int yy = y; yy > 0; yy--) memcpy(t[yy], t[yy - 1], W);
        memset(t[0], 0, W);
      }
    }
    int heights[W], total = 0, holes = 0, bumps = 0;
    for (int x = 0; x < W; x++) {
      int y = 0;
      while (y < H && !t[y][x]) y++;
      heights[x] = H - y;
      total += heights[x];
      for (; y < H; y++) holes += !t[y][x];
      if (x) bumps += abs(heights[x] - heights[x - 1]);
    }
    // Weights from the well-known "near perfect" Tetris AI by Yiyuan Lee.
    score = -0.510066f * total + 0.760666f * lines - 0.35663f * holes - 0.184483f * bumps;
    return true;
  }

  void spawn() {
    piece_ = esp_random() % 7;
    rotation_ = 0;
    x_ = 3;
    y_ = 0;
    phaseFrames_ = 0;
    if (!fits(board_, piece_, rotation_, x_, y_)) {
      phase_ = GAME_OVER;
      return;
    }
    phase_ = DROPPING;
    float best = -1e9f;
    targetRotation_ = 0;
    targetX_ = x_;
    for (int r = 0; r < 4; r++) {
      for (int x = -1; x < W; x++) {
        float s;
        if (evaluate(board_, piece_, r, x, s) && s > best) {
          best = s;
          targetRotation_ = r;
          targetX_ = x;
        }
      }
    }
  }

  void fall() {
    // Steer towards the planned rotation and column, then drop.
    if (rotation_ != targetRotation_ && fits(board_, piece_, (rotation_ + 1) % 4, x_, y_)) {
      rotation_ = (rotation_ + 1) % 4;
    } else if (x_ != targetX_ && fits(board_, piece_, rotation_, x_ + (targetX_ > x_ ? 1 : -1), y_)) {
      x_ += targetX_ > x_ ? 1 : -1;
    }
    const bool placed = rotation_ == targetRotation_ && x_ == targetX_;
    if (!placed && tick_ % 4 != 0) return;  // slow fall while still moving
    if (fits(board_, piece_, rotation_, x_, y_ + 1)) {
      y_++;
      return;
    }
    // Lock the piece.
    Cell c[4];
    cells(piece_, rotation_, c);
    for (const Cell &k : c) {
      if (y_ + k.y >= 0) board_[y_ + k.y][x_ + k.x] = 1;
    }
    bool anyFull = false;
    for (int y = 0; y < H; y++) anyFull |= rowFull(y);
    if (anyFull) {
      phase_ = CLEARING;
      phaseFrames_ = 0;
    } else {
      spawn();
    }
  }

  bool rowFull(int y) const {
    for (int x = 0; x < W; x++) {
      if (!board_[y][x]) return false;
    }
    return true;
  }

  void removeFullRows() {
    for (int y = 0; y < H; y++) {
      if (!rowFull(y)) continue;
      for (int yy = y; yy > 0; yy--) memcpy(board_[yy], board_[yy - 1], W);
      memset(board_[0], 0, W);
    }
  }

  void draw() {
    display.clear();
    for (int y = 0; y < H; y++) {
      display.setLevel(LEFT - 1, y, 30);  // walls
      display.setLevel(LEFT + W, y, 30);
      const bool flashing = phase_ == CLEARING && rowFull(y);
      for (int x = 0; x < W; x++) {
        if (!board_[y][x]) continue;
        uint8_t l = 150;
        if (flashing) l = (phaseFrames_ % 2) ? 255 : 40;
        if (phase_ == GAME_OVER) l = 150 * (30 - phaseFrames_) / 30;  // fade out
        display.setLevel(LEFT + x, y, l);
      }
    }
    if (phase_ == DROPPING) {
      Cell c[4];
      cells(piece_, rotation_, c);
      for (const Cell &k : c) display.setLevel(LEFT + x_ + k.x, y_ + k.y, 255);
    }
  }

  uint8_t board_[H][W];
  Phase phase_ = DROPPING;
  int piece_ = 0, rotation_ = 0, x_ = 0, y_ = 0;
  int targetRotation_ = 0, targetX_ = 0;
  uint32_t tick_ = 0;
  int phaseFrames_ = 0;
};

// ---------------------------------------------------------------------------
// Snake on the whole 16x16 panel. It takes the shortest path to the food,
// but only if afterwards it could still reach its own tail (so it doesn't
// trap itself); otherwise it follows its tail until the way is safe.
// ---------------------------------------------------------------------------
class SnakeAnimation : public Animation {
 public:
  const char *id() const override { return "snake"; }
  const char *name() const override { return "Snake"; }
  const char *group() const override { return "Giochi"; }
  uint16_t frameMs() const override { return 110; }

  void start() override {
    length_ = 3;
    for (int i = 0; i < length_; i++) body_[i] = cell(8 - i, 8);  // head first
    over_ = 0;
    placeFood();
  }

  void frame(uint32_t now) override {
    if (over_) {
      if (++over_ > 25) start();
    } else {
      step();
    }
    draw(now);
  }

 private:
  static const int N = COLS * ROWS;
  static uint8_t cell(int x, int y) { return y * COLS + x; }

  // Cells the snake occupies, minus its tail (which moves away this step).
  void occupancy(const uint8_t *body, int length, bool blocked[N]) const {
    memset(blocked, 0, N);
    for (int i = 0; i < length - 1; i++) blocked[body[i]] = true;
  }

  // Breadth-first search from `from` to `to`; fills `next` with the first
  // step and returns the path length, or -1 if unreachable.
  static int bfs(uint8_t from, uint8_t to, const bool blocked[N], uint8_t &next) {
    static int16_t prev[N];
    static uint8_t queue[N];
    for (int i = 0; i < N; i++) prev[i] = -1;
    int head = 0, tail = 0;
    queue[tail++] = from;
    prev[from] = from;
    while (head < tail) {
      const uint8_t c = queue[head++];
      if (c == to) break;
      const int x = c % COLS, y = c / COLS;
      const int nx[4] = {x + 1, x - 1, x, x}, ny[4] = {y, y, y + 1, y - 1};
      for (int d = 0; d < 4; d++) {
        if (nx[d] < 0 || nx[d] >= COLS || ny[d] < 0 || ny[d] >= ROWS) continue;
        const uint8_t n = cell(nx[d], ny[d]);
        if (prev[n] >= 0 || (blocked[n] && n != to)) continue;
        prev[n] = c;
        queue[tail++] = n;
      }
    }
    if (prev[to] < 0) return -1;
    int len = 0;
    uint8_t c = to;
    while (prev[c] != from) {
      c = prev[c];
      len++;
    }
    next = c;
    return len + 1;
  }

  // Would the snake still reach its tail after walking to the food?
  bool safeAfterEating() const {
    uint8_t body[N];
    int length = length_;
    memcpy(body, body_, length);
    bool blocked[N];
    for (int guard = 0; guard < N; guard++) {
      occupancy(body, length, blocked);
      uint8_t next;
      if (bfs(body[0], food_, blocked, next) < 0) return false;
      const bool eats = next == food_;
      if (eats) length++;
      memmove(body + 1, body, length - 1);
      body[0] = next;
      if (eats) break;
    }
    occupancy(body, length, blocked);
    uint8_t next;
    return length >= N || bfs(body[0], body[length - 1], blocked, next) >= 0;
  }

  // Free cells reachable from `from` (flood fill), to pick the roomiest move.
  static int space(uint8_t from, const bool blocked[N]) {
    static bool seen[N];
    static uint8_t queue[N];
    memset(seen, 0, sizeof(seen));
    int head = 0, tail = 0, count = 0;
    queue[tail++] = from;
    seen[from] = true;
    while (head < tail) {
      const uint8_t c = queue[head++];
      count++;
      const int x = c % COLS, y = c / COLS;
      const int nx[4] = {x + 1, x - 1, x, x}, ny[4] = {y, y, y + 1, y - 1};
      for (int d = 0; d < 4; d++) {
        if (nx[d] < 0 || nx[d] >= COLS || ny[d] < 0 || ny[d] >= ROWS) continue;
        const uint8_t n = cell(nx[d], ny[d]);
        if (seen[n] || blocked[n]) continue;
        seen[n] = true;
        queue[tail++] = n;
      }
    }
    return count;
  }

  void step() {
    bool blocked[N];
    occupancy(body_, length_, blocked);
    uint8_t next;
    bool found = bfs(body_[0], food_, blocked, next) >= 0 && safeAfterEating();
    if (!found) found = bfs(body_[0], body_[length_ - 1], blocked, next) >= 0 && next != body_[length_ - 1];
    if (!found) {
      // Last resort: the neighbour with the most room.
      int best = 0;
      const int x = body_[0] % COLS, y = body_[0] / COLS;
      const int nx[4] = {x + 1, x - 1, x, x}, ny[4] = {y, y, y + 1, y - 1};
      for (int d = 0; d < 4; d++) {
        if (nx[d] < 0 || nx[d] >= COLS || ny[d] < 0 || ny[d] >= ROWS) continue;
        const uint8_t n = cell(nx[d], ny[d]);
        if (blocked[n]) continue;
        const int room = space(n, blocked);
        if (room > best) {
          best = room;
          next = n;
          found = true;
        }
      }
    }
    if (!found) {
      over_ = 1;
      return;
    }
    const bool eats = next == food_;
    if (eats) length_++;
    memmove(body_ + 1, body_, length_ - 1);
    body_[0] = next;
    if (eats) {
      if (length_ >= N) {
        over_ = 1;  // the whole panel is snake: well played
        return;
      }
      placeFood();
    }
  }

  void placeFood() {
    bool taken[N] = {false};
    for (int i = 0; i < length_; i++) taken[body_[i]] = true;
    do {
      food_ = esp_random() % N;
    } while (taken[food_]);
  }

  void draw(uint32_t now) {
    display.clear();
    for (int i = length_ - 1; i >= 0; i--) {
      // Head brightest, fading towards the tail.
      uint8_t l = i == 0 ? 255 : 200 - 140 * i / max(1, length_ - 1);
      if (over_) l = l * (25 - min(over_, 25)) / 25;
      display.setLevel(body_[i] % COLS, body_[i] / COLS, l);
    }
    if (!over_) display.setLevel(food_ % COLS, food_ / COLS, (now / 250) % 2 ? 255 : 90);
  }

  uint8_t body_[N];
  int length_ = 3;
  uint8_t food_ = 0;
  int over_ = 0;  // frames since game over, 0 while playing
};

static TetrisAnimation tetris;
extern Animation *const tetrisAnimation = &tetris;
static SnakeAnimation snake;
extern Animation *const snakeAnimation = &snake;
