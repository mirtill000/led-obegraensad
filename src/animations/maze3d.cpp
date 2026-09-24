// "Labirinto 3D": a first-person maze drawn by raycasting, one ray per
// column, with walls shaded by distance through a fixed dot pattern (LEDs
// only on or off, so nothing flickers).
#include <math.h>
#include <string.h>

#include "animation.h"
#include "display.h"

// ---------------------------------------------------------------------------
// The maze is a 13x13 grid (6x6 rooms and the walls between them), carved
// by a random depth-first search. The exit is a glowing block at the dead
// end farthest from the start: walk into it to win.
//
// Moves are on the grid - a step forward or back, a quarter turn - and are
// animated over a few frames. The player uses ↑ ↓ to walk, ← → to turn and
// the main button to show the map; in demo mode the computer explores by
// keeping its right hand on the wall, which always finds the exit.
//
// The map (1 pixel per cell) is shown for a moment at the start of every
// maze: walls dim, the exit bright, the player blinking.
// ---------------------------------------------------------------------------
class Maze3dAnimation : public Animation {
 public:
  const char *id() const override { return "maze"; }
  const char *name() const override { return "Labirinto 3D"; }
  const char *group() const override { return "Giochi"; }
  uint16_t frameMs() const override { return 40; }
  bool isGame() const override { return true; }
  void setDemo(bool demo) override { demo_ = demo; }

  void input(char key) override { queued_ = key; }

  void start() override {
    generate();
    phase_ = MAP;
    phaseFrames_ = 0;
    queued_ = 0;
  }

  void frame(uint32_t) override {
    tick_++;
    switch (phase_) {
      case MAP:
        drawMap();
        if (++phaseFrames_ >= MAP_FRAMES) {
          phase_ = WALK;
          queued_ = 0;
        }
        return;
      case WON:
        // The view dissolves into light, then a new maze.
        render();
        for (int i = 0; i < phaseFrames_ * 12 && i < 256; i++) {
          const int x = i % COLS, y = i / COLS;
          display.setLevel(x, y, 255);
        }
        if (++phaseFrames_ >= 30) start();
        return;
      case WALK:
        break;
    }

    if (move_ == NONE) nextMove();
    advanceMove();
    render();
  }

 private:
  static const int N = 13;  // grid size (odd: rooms at odd coordinates)
  static const int MAP_FRAMES = 60;
  static const int STEP_FRAMES = 6;
  static const int TURN_FRAMES = 5;
  enum Cell : uint8_t { OPEN = 0, WALL = 1, EXIT = 2 };
  enum Phase : uint8_t { MAP, WALK, WON };
  enum Move : uint8_t { NONE, FORWARD, BACK, LEFT, RIGHT };

  // Directions 0-3: east, south, west, north (grid y grows southwards).
  static int dx(int d) { return d == 0 ? 1 : d == 2 ? -1 : 0; }
  static int dy(int d) { return d == 1 ? 1 : d == 3 ? -1 : 0; }

  uint8_t cellAt(int x, int y) const { return x < 0 || y < 0 || x >= N || y >= N ? WALL : grid_[y][x]; }

  void generate() {
    memset(grid_, WALL, sizeof(grid_));
    // Depth-first carving from the start room with an explicit stack.
    static int stackX[N * N], stackY[N * N];
    int top = 0;
    stackX[0] = 1;
    stackY[0] = 1;
    grid_[1][1] = OPEN;
    while (top >= 0) {
      const int x = stackX[top], y = stackY[top];
      int dirs[4], n = 0;
      for (int d = 0; d < 4; d++) {
        const int nx = x + 2 * dx(d), ny = y + 2 * dy(d);
        if (nx > 0 && ny > 0 && nx < N - 1 && ny < N - 1 && grid_[ny][nx] == WALL) dirs[n++] = d;
      }
      if (n == 0) {
        top--;
        continue;
      }
      const int d = dirs[esp_random() % n];
      grid_[y + dy(d)][x + dx(d)] = OPEN;
      grid_[y + 2 * dy(d)][x + 2 * dx(d)] = OPEN;
      top++;
      stackX[top] = x + 2 * dx(d);
      stackY[top] = y + 2 * dy(d);
    }

    // Exit: the room farthest from the start (breadth-first distances).
    static int dist[N][N];
    memset(dist, -1, sizeof(dist));
    static int queueX[N * N], queueY[N * N];
    int head = 0, tail = 0;
    queueX[tail] = 1;
    queueY[tail++] = 1;
    dist[1][1] = 0;
    int farX = 1, farY = 1;
    while (head < tail) {
      const int x = queueX[head], y = queueY[head++];
      if (dist[y][x] > dist[farY][farX]) {
        farX = x;
        farY = y;
      }
      for (int d = 0; d < 4; d++) {
        const int nx = x + dx(d), ny = y + dy(d);
        if (grid_[ny][nx] == OPEN && dist[ny][nx] < 0) {
          dist[ny][nx] = dist[y][x] + 1;
          queueX[tail] = nx;
          queueY[tail++] = ny;
        }
      }
    }
    grid_[farY][farX] = EXIT;

    cellX_ = 1;
    cellY_ = 1;
    // Face an open corridor.
    dir_ = 0;
    while (cellAt(cellX_ + dx(dir_), cellY_ + dy(dir_)) == WALL) dir_ = (dir_ + 1) % 4;
    posX_ = cellX_ + 0.5f;
    posY_ = cellY_ + 0.5f;
    angle_ = dir_ * (float)M_PI / 2;
    move_ = NONE;
  }

  bool open(int d) const { return cellAt(cellX_ + dx(d), cellY_ + dy(d)) != WALL; }

  // Picks the next move: the player's key, or the right-hand rule.
  void nextMove() {
    Move m = NONE;
    if (demo_) {
      const int right = (dir_ + 1) % 4;
      if (lastTurnedRight_ && open(dir_)) m = FORWARD;  // finish a right turn by stepping
      else if (open(right)) m = RIGHT;
      else if (open(dir_)) m = FORWARD;
      else m = LEFT;
      lastTurnedRight_ = m == RIGHT;
    } else {
      switch (queued_) {
        case 'U': m = FORWARD; break;
        case 'D': m = BACK; break;
        case 'L': m = LEFT; break;
        case 'R': m = RIGHT; break;
        case 'A':
          phase_ = MAP;
          phaseFrames_ = MAP_FRAMES / 2;  // a shorter look
          break;
      }
      queued_ = 0;
    }
    if (m == FORWARD || m == BACK) {
      const int d = m == FORWARD ? dir_ : (dir_ + 2) % 4;
      const uint8_t target = cellAt(cellX_ + dx(d), cellY_ + dy(d));
      if (target == EXIT) {
        phase_ = WON;
        phaseFrames_ = 0;
        return;
      }
      if (target == WALL) m = NONE;  // bump: nothing happens
      else {
        fromX_ = posX_;
        fromY_ = posY_;
        cellX_ += dx(d);
        cellY_ += dy(d);
      }
    } else if (m == LEFT || m == RIGHT) {
      fromAngle_ = angle_;
      dir_ = (dir_ + (m == RIGHT ? 1 : 3)) % 4;
    }
    move_ = m;
    moveFrames_ = 0;
  }

  void advanceMove() {
    if (move_ == NONE) return;
    moveFrames_++;
    if (move_ == FORWARD || move_ == BACK) {
      const float t = min(1.0f, moveFrames_ / (float)STEP_FRAMES);
      posX_ = fromX_ + (cellX_ + 0.5f - fromX_) * t;
      posY_ = fromY_ + (cellY_ + 0.5f - fromY_) * t;
      if (t >= 1) move_ = NONE;
    } else {
      const float t = min(1.0f, moveFrames_ / (float)TURN_FRAMES);
      const float delta = move_ == RIGHT ? (float)M_PI / 2 : -(float)M_PI / 2;
      angle_ = fromAngle_ + delta * t;
      if (t >= 1) {
        angle_ = dir_ * (float)M_PI / 2;
        move_ = NONE;
      }
    }
  }

  // One ray per column (DDA through the grid). LEDs are only fully on or
  // off (in-between levels are made by fast switching, which can flicker),
  // so walls are shaded with a fixed dot pattern (ordered dithering): the
  // closer the wall, the more of its LEDs are lit; sides facing north/south
  // are a bit sparser so corners read. Their top and bottom edges are fully
  // lit, and so is a column wherever the view passes from one block face
  // to another. The exit is a solid block.
  void render() {
    display.clear();
    static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    const float dirX = cosf(angle_), dirY = sinf(angle_);
    const float planeX = -dirY * 0.66f, planeY = dirX * 0.66f;  // ~66° field of view
    const float horizon = ROWS / 2.0f;
    int lastFace = -1;

    for (int x = 0; x < COLS; x++) {
      const float camera = 2 * (x + 0.5f) / COLS - 1;
      const float rayX = dirX + planeX * camera, rayY = dirY + planeY * camera;
      int mapX = (int)posX_, mapY = (int)posY_;
      const float deltaX = rayX == 0 ? 1e30f : fabsf(1 / rayX);
      const float deltaY = rayY == 0 ? 1e30f : fabsf(1 / rayY);
      const int stepX = rayX < 0 ? -1 : 1, stepY = rayY < 0 ? -1 : 1;
      float sideX = rayX < 0 ? (posX_ - mapX) * deltaX : (mapX + 1 - posX_) * deltaX;
      float sideY = rayY < 0 ? (posY_ - mapY) * deltaY : (mapY + 1 - posY_) * deltaY;
      bool ySide = false;
      uint8_t hit = WALL;
      for (int guard = 0; guard < 2 * N; guard++) {
        if (sideX < sideY) {
          sideX += deltaX;
          mapX += stepX;
          ySide = false;
        } else {
          sideY += deltaY;
          mapY += stepY;
          ySide = true;
        }
        hit = cellAt(mapX, mapY);
        if (hit != OPEN) break;
      }
      const float dist = max(0.05f, ySide ? sideY - deltaY : sideX - deltaX);

      float shade = 1.0f / (1.0f + dist * 0.45f);
      if (ySide) shade *= 0.72f;
      const int density = (int)(shade * 16);  // lit LEDs out of 16

      // Wall span (rounded to whole pixels).
      const float height = ROWS / dist;
      const int top = (int)lroundf(horizon - height / 2), bottom = (int)lroundf(horizon + height / 2) - 1;
      const int face = (mapY * N + mapX) * 2 + ySide;
      const bool edge = face != lastFace;  // a new block face starts here
      lastFace = face;
      for (int y = max(0, top); y <= min(ROWS - 1, bottom); y++) {
        const bool on = hit == EXIT || edge || y == top || y == bottom || BAYER[y % 4][x % 4] < density;
        display.setPixel(x, y, on);
      }
    }
  }

  // The whole maze, 1 pixel per cell, centred.
  void drawMap() {
    display.clear();
    const int ox = (COLS - N) / 2, oy = (ROWS - N) / 2;
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        if (grid_[y][x] == WALL) display.setPixel(ox + x, oy + y, true);
      }
    }
    // With the walls lit, the exit and the player show as slow blinks, in
    // turn.
    const bool phase = (tick_ / 6) % 2;
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        if (grid_[y][x] == EXIT) display.setPixel(ox + x, oy + y, phase);
      }
    }
    display.setPixel(ox + cellX_, oy + cellY_, !phase);
  }

  uint8_t grid_[N][N];
  int cellX_ = 1, cellY_ = 1, dir_ = 0;
  float posX_ = 1.5f, posY_ = 1.5f, angle_ = 0;
  float fromX_ = 0, fromY_ = 0, fromAngle_ = 0;
  Move move_ = NONE;
  int moveFrames_ = 0;
  Phase phase_ = MAP;
  int phaseFrames_ = 0;
  uint32_t tick_ = 0;
  char queued_ = 0;
  bool demo_ = true;
  bool lastTurnedRight_ = false;
};

static Maze3dAnimation maze3d;
extern Animation *const maze3dAnimation = &maze3d;
