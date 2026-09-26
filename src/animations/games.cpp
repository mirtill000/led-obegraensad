// Games that play themselves: Tetris and Snake.
#include "animation.h"
#include "display.h"
#include "settings.h"

// ---------------------------------------------------------------------------
// Tetris: a 10x16 well in the middle of the panel. In demo mode, for every
// new piece the computer tries each rotation and column and picks the
// placement with the best score (low stack, few holes, even surface,
// cleared lines), then moves the piece there in view as it falls. Otherwise
// the player moves (L/R), rotates (U) and drops (D or A) it.
// ---------------------------------------------------------------------------
class TetrisAnimation : public Animation {
 public:
  const char *id() const override { return "tetris"; }
  const char *name() const override { return "Tetris"; }
  const char *group() const override { return "Giochi"; }
  uint16_t frameMs() const override { return 60; }
  bool isGame() const override { return true; }
  void setDemo(bool demo) override { demo_ = demo; }

  void input(char key) override {
    if (phase_ != DROPPING) return;
    if (key == 'L' || key == 'R') {
      const int dx = key == 'L' ? -1 : 1;
      if (fits(board_, piece_, rotation_, x_ + dx, y_)) x_ += dx;
    } else if (key == 'U') {
      // Rotate, nudging sideways if the piece is against a wall or block.
      const int r = (rotation_ + 1) % 4;
      for (int kick : {0, -1, 1, -2, 2}) {
        if (fits(board_, piece_, r, x_ + kick, y_)) {
          rotation_ = r;
          x_ += kick;
          break;
        }
      }
    } else if (key == 'D' || key == 'A') {
      while (fits(board_, piece_, rotation_, x_, y_ + 1)) y_++;
      dropNow_ = true;
    }
  }

  void start() override {
    W = settings.vertical ? MAXW : 10;
    LEFT = (COLS - W) / 2;
    memset(board_, 0, sizeof(board_));
    phase_ = DROPPING;
    spawn();
  }

  void frame(uint32_t) override {
    if (W != (settings.vertical ? MAXW : 10)) start();  // the lamp was turned
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
  // The well: 10 columns (3-12) with the lamp horizontal, 14 (1-14, the
  // whole width inside the walls) when it hangs vertically.
  static const int H = ROWS, MAXW = 14;
  int W = 10, LEFT = 3;
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

  bool fits(const uint8_t b[H][MAXW], int piece, int rotation, int px, int py) const {
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
  bool evaluate(const uint8_t b[H][MAXW], int piece, int rotation, int px, float &score) const {
    if (!fits(b, piece, rotation, px, 0)) return false;
    int py = 0;
    while (fits(b, piece, rotation, px, py + 1)) py++;
    uint8_t t[H][MAXW];
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
    int heights[MAXW], total = 0, holes = 0, bumps = 0;
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
    x_ = W / 2 - 2;
    y_ = 0;
    phaseFrames_ = 0;
    if (!fits(board_, piece_, rotation_, x_, y_)) {
      phase_ = GAME_OVER;
      return;
    }
    phase_ = DROPPING;
    dropNow_ = false;
    float best = -1e9f;
    targetRotation_ = 0;
    targetX_ = x_;
    if (!demo_) return;  // the player steers
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
    if (demo_) {
      // Steer towards the planned rotation and column, then drop.
      if (rotation_ != targetRotation_ && fits(board_, piece_, (rotation_ + 1) % 4, x_, y_)) {
        rotation_ = (rotation_ + 1) % 4;
      } else if (x_ != targetX_ && fits(board_, piece_, rotation_, x_ + (targetX_ > x_ ? 1 : -1), y_)) {
        x_ += targetX_ > x_ ? 1 : -1;
      }
      const bool placed = rotation_ == targetRotation_ && x_ == targetX_;
      if (!placed && tick_ % 4 != 0) return;  // slow fall while still moving
    } else if (!dropNow_ && tick_ % 5 != 0) {
      return;  // player: one row every 5 frames
    }
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

  // "Nitida": every LED fully on or off. "Sfumata" (softGames()): settled
  // blocks dimmer than the falling piece, faint walls, a fade at game over.
  void draw() {
    if (softGames()) return drawSoft();
    display.clear();
    for (int y = 0; y < H; y++) {
      display.setPixel(LEFT - 1, y, true);  // walls
      display.setPixel(LEFT + W, y, true);
      const bool flashing = phase_ == CLEARING && rowFull(y);
      // Game over: the rows go dark one by one from the top.
      const bool gone = phase_ == GAME_OVER && y < phaseFrames_ * H / 30;
      for (int x = 0; x < W; x++) {
        if (!board_[y][x] || gone) continue;
        if (flashing && phaseFrames_ % 2 == 0) continue;  // full rows blink
        display.setPixel(LEFT + x, y, true);
      }
    }
    if (phase_ == DROPPING) {
      Cell c[4];
      cells(piece_, rotation_, c);
      for (const Cell &k : c) display.setLevel(LEFT + x_ + k.x, y_ + k.y, 255);
    }
  }

  void drawSoft() {
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

  uint8_t board_[H][MAXW];
  Phase phase_ = DROPPING;
  int piece_ = 0, rotation_ = 0, x_ = 0, y_ = 0;
  int targetRotation_ = 0, targetX_ = 0;
  bool demo_ = true;
  bool dropNow_ = false;  // hard drop pressed: lock without waiting
  uint32_t tick_ = 0;
  int phaseFrames_ = 0;
};

// ---------------------------------------------------------------------------
// Snake on the whole 16x16 panel. In demo mode it takes the shortest path
// to the food, but only if afterwards it could still reach its own tail (so
// it doesn't trap itself); otherwise it follows its tail until the way is
// safe. Otherwise the player steers it with the arrows.
// ---------------------------------------------------------------------------
class SnakeAnimation : public Animation {
 public:
  const char *id() const override { return "snake"; }
  const char *name() const override { return "Snake"; }
  const char *group() const override { return "Giochi"; }
  uint16_t frameMs() const override { return 110; }
  bool isGame() const override { return true; }
  void setDemo(bool demo) override { demo_ = demo; }

  void input(char key) override {
    int dx = 0, dy = 0;
    if (key == 'L') dx = -1;
    if (key == 'R') dx = 1;
    if (key == 'U') dy = -1;
    if (key == 'D') dy = 1;
    if ((dx || dy) && !(dx == -dx_ && dy == -dy_)) {  // no U-turns
      nextDx_ = dx;
      nextDy_ = dy;
    }
  }

  void start() override {
    length_ = 3;
    for (int i = 0; i < length_; i++) body_[i] = cell(8 - i, 8);  // head first
    dx_ = nextDx_ = 1;
    dy_ = nextDy_ = 0;
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

  // Player: one cell in the chosen direction; hitting a wall or itself ends
  // the game.
  bool playerMove(const bool blocked[N], uint8_t &next) {
    if (!(nextDx_ == -dx_ && nextDy_ == -dy_)) {
      dx_ = nextDx_;
      dy_ = nextDy_;
    }
    const int x = body_[0] % COLS + dx_, y = body_[0] / COLS + dy_;
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return false;
    next = cell(x, y);
    // The tail moves away this step, unless the snake grows into it.
    return !blocked[next] && !(next == body_[length_ - 1] && next == food_);
  }

  void step() {
    bool blocked[N];
    occupancy(body_, length_, blocked);
    uint8_t next;
    if (!demo_) {
      if (!playerMove(blocked, next)) {
        over_ = 1;
        return;
      }
      advance(next);
      return;
    }
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
    // Remember the direction, so the player can take over mid-game.
    dx_ = nextDx_ = (int)(next % COLS) - (int)(body_[0] % COLS);
    dy_ = nextDy_ = (int)(next / COLS) - (int)(body_[0] / COLS);
    advance(next);
  }

  void advance(uint8_t next) {
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

  // "Nitida": LEDs only fully on or off; at game over the snake shortens
  // from the tail. "Sfumata": the body fades towards the tail, the food
  // pulses, and the snake fades out at game over.
  void draw(uint32_t now) {
    if (softGames()) {
      display.clear();
      for (int i = length_ - 1; i >= 0; i--) {
        uint8_t l = i == 0 ? 255 : 200 - 140 * i / max(1, length_ - 1);
        if (over_) l = l * (25 - min(over_, 25)) / 25;
        display.setLevel(body_[i] % COLS, body_[i] / COLS, l);
      }
      if (!over_) display.setLevel(food_ % COLS, food_ / COLS, (now / 250) % 2 ? 255 : 90);
      return;
    }
    display.clear();
    const int shown = over_ ? length_ * (25 - min(over_, 25)) / 25 : length_;
    for (int i = 0; i < shown; i++) display.setPixel(body_[i] % COLS, body_[i] / COLS, true);
    if (!over_) display.setPixel(food_ % COLS, food_ / COLS, true);
  }

  uint8_t body_[N];
  int length_ = 3;
  bool demo_ = true;
  int dx_ = 1, dy_ = 0;          // current direction
  int nextDx_ = 1, nextDy_ = 0;  // requested by the player for the next step
  uint8_t food_ = 0;
  int over_ = 0;  // frames since game over, 0 while playing
};

static TetrisAnimation tetris;
extern Animation *const tetrisAnimation = &tetris;
static SnakeAnimation snake;
extern Animation *const snakeAnimation = &snake;
