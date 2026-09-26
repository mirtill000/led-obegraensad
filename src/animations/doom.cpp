// "Doom": a first-person shooter on 16x16. The level is drawn by raycasting
// like Labirinto 3D (walls shaded by distance), the imps are sprites that
// stand in it (hidden behind walls, bigger as they come closer), and they
// throw fireballs you can see coming. You have a gun at the bottom of the
// view and a health bar on the bottom row.
//
// ↑ ↓ walk, ← → turn, the main button shoots (the shot hits the nearest imp
// in the middle of the view). Kill every imp to reach the next level, with
// more of them and some health back; at 0 health it's game over (score:
// 100 per imp, 500 per level). In demo mode the computer turns towards the
// nearest imp it can see and shoots, or walks along the shortest path to
// the nearest one.
#include <math.h>
#include <string.h>

#include "animations/arcade_game.h"
#include "display.h"

namespace {

const int N = 16;
const char *const MAP[N] = {
    "################",
    "#......#.......#",
    "#......#.......#",
    "#..##..#..##...#",
    "#..##.....##...#",
    "#......#.......#",
    "###.####.#######",
    "#.......#......#",
    "#.......#......#",
    "#..#....#..##..#",
    "#..#...........#",
    "#..#....#......#",
    "####.####..#...#",
    "#.......#..#...#",
    "#.......#......#",
    "################",
};
const float START_X = 1.5f, START_Y = 14.5f;

const int VIEW_ROWS = ROWS - 1;  // the bottom row is the health bar
const float HORIZON = VIEW_ROWS / 2.0f;
const int MAX_IMPS = 8, MAX_BALLS = 6;
const float RADIUS = 0.22f;  // how close anything gets to a wall

// The imp, 5x6: horns, eyes, arms out, legs.
const char *const IMP[6] = {"#...#", ".###.", "#.#.#", "#####", ".###.", ".#.#."};
const char *const IMP_DEAD[2] = {"#.##.", "#####"};

struct Imp {
  bool alive;
  float x, y;
  int hurt;    // frames of the hit flash
  int dying;   // frames of the fall
  int reload;  // frames until it can throw again
  bool awake;  // has seen you
};

struct Ball {
  bool used;
  float x, y, vx, vy;
};

bool wall(float x, float y) {
  const int cx = (int)x, cy = (int)y;
  return cx < 0 || cy < 0 || cx >= N || cy >= N || MAP[cy][cx] == '#';
}

// Nothing solid on the straight line between two points.
bool sight(float ax, float ay, float bx, float by) {
  const float dx = bx - ax, dy = by - ay;
  const int steps = (int)(sqrtf(dx * dx + dy * dy) * 8) + 1;
  for (int i = 1; i < steps; i++) {
    if (wall(ax + dx * i / steps, ay + dy * i / steps)) return false;
  }
  return true;
}

float wrapAngle(float a) {
  while (a > (float)M_PI) a -= 2 * (float)M_PI;
  while (a < -(float)M_PI) a += 2 * (float)M_PI;
  return a;
}

}  // namespace

class DoomGame : public ArcadeGame {
 public:
  const char *id() const override { return "doom"; }
  const char *name() const override { return "Doom"; }
  uint16_t frameMs() const override { return 50; }

  void start() override {
    health_ = 100;
    kills_ = 0;
    level_ = 0;
    newLevel();
  }

  void input(char key) override {
    if (dead_ || demo_) return;
    if (key == 'L') angle_ -= 0.2f;
    if (key == 'R') angle_ += 0.2f;
    if (key == 'U') walk(0.3f);
    if (key == 'D') walk(-0.3f);
    if (key == 'A') shoot();
  }

 protected:
  void tick(uint32_t) override {
    tick_++;
    if (dead_) {
      if (--dead_ == 0) return gameOver(kills_ * 100 + level_ * 500);
      return draw();
    }
    if (cleared_) {  // level done: a moment of light, then the next one
      if (--cleared_ == 0) {
        level_++;
        health_ = min(100, health_ + 30);
        newLevel();
      }
      return draw();
    }
    if (demo_) autopilot();
    if (reload_ > 0) reload_--;
    if (flash_ > 0) flash_--;
    if (hurt_ > 0) hurt_--;
    moveImps();
    moveBalls();
    if (health_ <= 0) {
      health_ = 0;
      dead_ = 40;
    } else {
      bool any = false;
      for (const Imp &m : imps_) any |= m.alive;
      if (!any) cleared_ = 30;
    }
    draw();
  }

 private:
  void newLevel() {
    x_ = START_X;
    y_ = START_Y;
    angle_ = -(float)M_PI / 2;  // facing north, up the corridor
    for (Ball &b : balls_) b.used = false;
    for (Imp &m : imps_) m = {};
    const int count = min(MAX_IMPS, 3 + level_);
    for (int i = 0; i < count; i++) {
      // A random floor cell away from the start and from the other imps.
      for (int tries = 0; tries < 200; tries++) {
        const int cx = 1 + esp_random() % (N - 2), cy = 1 + esp_random() % (N - 2);
        if (MAP[cy][cx] == '#' || hypotf(cx + 0.5f - x_, cy + 0.5f - y_) < 6) continue;
        bool taken = false;
        for (const Imp &m : imps_) taken |= m.alive && (int)m.x == cx && (int)m.y == cy;
        if (taken) continue;
        imps_[i] = {true, cx + 0.5f, cy + 0.5f, 0, 0, 30 + (int)(esp_random() % 60), false};
        break;
      }
    }
    reload_ = flash_ = hurt_ = 0;
  }

  void walk(float step) {
    const float nx = x_ + cosf(angle_) * step, ny = y_ + sinf(angle_) * step;
    // Slide along walls: try each axis on its own.
    if (!wall(nx + copysignf(RADIUS, nx - x_), y_)) x_ = nx;
    if (!wall(x_, ny + copysignf(RADIUS, ny - y_))) y_ = ny;
  }

  // The imp the crosshair is on (in the middle two columns, in front of
  // the wall there), or -1.
  int target() {
    const float dirX = cosf(angle_), dirY = sinf(angle_);
    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < MAX_IMPS; i++) {
      const Imp &m = imps_[i];
      if (!m.alive) continue;
      const float sx = m.x - x_, sy = m.y - y_;
      const float depth = sx * dirX + sy * dirY;
      if (depth <= 0.2f || depth >= bestDist) continue;
      const float side = -sx * dirY + sy * dirX;  // sideways offset
      if (fabsf(side) > 0.35f + depth * 0.04f) continue;
      if (!sight(x_, y_, m.x, m.y)) continue;
      best = i;
      bestDist = depth;
    }
    return best;
  }

  void shoot() {
    if (reload_ > 0) return;
    reload_ = 8;
    flash_ = 3;
    const int i = target();
    if (i < 0) return;
    Imp &m = imps_[i];
    m.alive = false;
    m.dying = 10;
    kills_++;
  }

  void moveImps() {
    for (Imp &m : imps_) {
      if (m.dying > 0) m.dying--;
      if (!m.alive) continue;
      if (m.hurt > 0) m.hurt--;
      const float dx = x_ - m.x, dy = y_ - m.y, dist = hypotf(dx, dy);
      const bool sees = dist < 9 && sight(m.x, m.y, x_, y_);
      if (sees && !m.awake) {
        m.awake = true;
        m.reload = 8 + esp_random() % 20;  // a first throw soon after it sees you
      }
      if (!m.awake) continue;
      // Come closer (but not too close), and throw a fireball now and then.
      if (sees && dist > 2.2f) {
        const float speed = 0.035f + level_ * 0.004f;
        const float nx = m.x + dx / dist * speed, ny = m.y + dy / dist * speed;
        if (!wall(nx, m.y)) m.x = nx;
        if (!wall(m.x, ny)) m.y = ny;
      }
      if (--m.reload <= 0) {
        if (sees) {
          throwBall(m, dx / dist, dy / dist);
          m.reload = max(20, 60 - level_ * 5) + esp_random() % 30;
        } else {
          m.reload = 5;  // throws as soon as it sees you again
        }
      }
    }
  }

  void throwBall(const Imp &m, float ux, float uy) {
    for (Ball &b : balls_) {
      if (b.used) continue;
      const float speed = 0.16f;
      b = {true, m.x + ux * 0.3f, m.y + uy * 0.3f, ux * speed, uy * speed};
      return;
    }
  }

  void moveBalls() {
    for (Ball &b : balls_) {
      if (!b.used) continue;
      b.x += b.vx;
      b.y += b.vy;
      if (wall(b.x, b.y)) {
        b.used = false;
      } else if (hypotf(b.x - x_, b.y - y_) < 0.35f) {
        b.used = false;
        health_ -= 15;
        hurt_ = 4;
      }
    }
  }

  // Next cell on a shortest path from the player's cell to the nearest imp
  // (breadth-first search on the grid); false if none can be reached.
  bool pathStep(int &nx, int &ny) const {
    static int8_t fromX[N][N], fromY[N][N];
    static uint8_t qx[N * N], qy[N * N];
    memset(fromX, -1, sizeof(fromX));
    const int sx = (int)x_, sy = (int)y_;
    int head = 0, tail = 0;
    qx[tail] = sx;
    qy[tail++] = sy;
    fromX[sy][sx] = sx;
    fromY[sy][sx] = sy;
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    while (head < tail) {
      const int cx = qx[head], cy = qy[head++];
      bool imp = false;
      for (const Imp &m : imps_) imp |= m.alive && (int)m.x == cx && (int)m.y == cy;
      if (imp && (cx != sx || cy != sy)) {
        // Walk back to the cell next to the start.
        int x = cx, y = cy;
        while (fromX[y][x] != sx || fromY[y][x] != sy) {
          const int px = fromX[y][x], py = fromY[y][x];
          x = px;
          y = py;
        }
        nx = x;
        ny = y;
        return true;
      }
      for (int d = 0; d < 4; d++) {
        const int x = cx + DX[d], y = cy + DY[d];
        if (MAP[y][x] == '#' || fromX[y][x] >= 0) continue;
        fromX[y][x] = cx;
        fromY[y][x] = cy;
        qx[tail] = x;
        qy[tail++] = y;
      }
    }
    return false;
  }

  // Turns towards `want` at a limited speed; true once facing it.
  bool turnTo(float want) {
    const float diff = wrapAngle(want - angle_);
    const float turn = constrain(diff, -0.1f, 0.1f);
    angle_ += turn;
    return fabsf(diff) < 0.2f;
  }

  void autopilot() {
    // The nearest imp in sight: face it and shoot.
    const Imp *seen = nullptr;
    float seenDist = 1e9f;
    for (const Imp &m : imps_) {
      if (!m.alive) continue;
      const float d = hypotf(m.x - x_, m.y - y_);
      if (d < seenDist && sight(x_, y_, m.x, m.y)) {
        seen = &m;
        seenDist = d;
      }
    }
    if (seen) {
      // Not a perfect shot: it needs a moment on target, so it can lose.
      turnTo(atan2f(seen->y - y_, seen->x - x_));
      if (target() >= 0 && esp_random() % 6 == 0) shoot();
      return;
    }
    // Otherwise head for the nearest one along the corridors.
    int cx, cy;
    if (!pathStep(cx, cy)) return;
    if (turnTo(atan2f(cy + 0.5f - y_, cx + 0.5f - x_))) walk(0.11f);
  }

  // --- drawing ---------------------------------------------------------------

  // Billboard `rows` (w x h pattern) standing on the floor at (sx, sy),
  // `size` blocks tall, behind walls closer than it.
  void sprite(float sx, float sy, const char *const *rows, int pw, int ph, float size, float lift, uint8_t level,
              const float *depth) {
    const float dirX = cosf(angle_), dirY = sinf(angle_);
    const float planeX = -dirY * 0.66f, planeY = dirX * 0.66f;
    const float rx = sx - x_, ry = sy - y_;
    const float inv = 1.0f / (planeX * dirY - dirX * planeY);
    const float tx = inv * (dirY * rx - dirX * ry), ty = inv * (-planeY * rx + planeX * ry);
    if (ty <= 0.15f) return;
    const float centre = COLS / 2.0f * (1 + tx / ty);
    const float h = ROWS / ty * size, w = h * pw / ph;
    const float floorY = HORIZON + ROWS / ty / 2 - lift * ROWS / ty;
    const float left = centre - w / 2, top = floorY - h;
    for (int x = max(0, (int)floorf(left)); x < COLS && x < left + w; x++) {
      if (ty >= depth[x]) continue;
      const int u = min(pw - 1, (int)((x + 0.5f - left) / w * pw));
      for (int y = max(0, (int)floorf(top)); y < VIEW_ROWS && y < floorY; y++) {
        const int v = min(ph - 1, (int)((y + 0.5f - top) / h * ph));
        if (v >= 0 && u >= 0 && rows[v][u] == '#') display.setLevel(x, y, level);
      }
    }
  }

  void draw() {
    display.clear();
    const float dirX = cosf(angle_), dirY = sinf(angle_);
    const float planeX = -dirY * 0.66f, planeY = dirX * 0.66f;
    float depth[COLS];

    for (int y = (int)HORIZON + 1; y < VIEW_ROWS; y++) {  // a faint floor
      for (int x = 0; x < COLS; x++) display.setLevel(x, y, 6 + (y - HORIZON) * 3);
    }
    for (int x = 0; x < COLS; x++) {
      const float camera = 2 * (x + 0.5f) / COLS - 1;
      const float rayX = dirX + planeX * camera, rayY = dirY + planeY * camera;
      int mapX = (int)x_, mapY = (int)y_;
      const float deltaX = rayX == 0 ? 1e30f : fabsf(1 / rayX);
      const float deltaY = rayY == 0 ? 1e30f : fabsf(1 / rayY);
      const int stepX = rayX < 0 ? -1 : 1, stepY = rayY < 0 ? -1 : 1;
      float sideX = rayX < 0 ? (x_ - mapX) * deltaX : (mapX + 1 - x_) * deltaX;
      float sideY = rayY < 0 ? (y_ - mapY) * deltaY : (mapY + 1 - y_) * deltaY;
      bool ySide = false;
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
        if (wall(mapX + 0.5f, mapY + 0.5f)) break;
      }
      const float dist = max(0.05f, ySide ? sideY - deltaY : sideX - deltaX);
      depth[x] = dist;
      float wallU = ySide ? x_ + dist * rayX : y_ + dist * rayY;
      wallU -= floorf(wallU);
      // Walls stay below the imps' full brightness so these stand out.
      float shade = 170.0f / (1.0f + dist * 0.4f);
      if (ySide) shade *= 0.7f;
      if (wallU < 0.06f || wallU > 0.94f) shade *= 0.55f;
      const uint8_t level = (uint8_t)constrain(shade, 10.0f, 170.0f);
      const float height = ROWS / dist;
      const float top = HORIZON - height / 2, bottom = HORIZON + height / 2;
      for (int y = max(0, (int)floorf(top)); y < VIEW_ROWS && y < bottom; y++) {
        const float cover = min(bottom, y + 1.0f) - max(top, (float)y);
        if (cover <= 0) continue;
        const uint8_t under = display.getLevel(x, y);
        display.setLevel(x, y, (uint8_t)(under + (level - under) * min(1.0f, cover)));
      }
    }

    // Sprites, farthest first, so nearer ones cover them.
    struct Item {
      float d;
      int kind, index;  // 0 imp, 1 ball
    } items[MAX_IMPS + MAX_BALLS];
    int n = 0;
    for (int i = 0; i < MAX_IMPS; i++) {
      const Imp &m = imps_[i];
      if (m.alive || m.dying > 0 || m.x > 0) items[n++] = {hypotf(m.x - x_, m.y - y_), 0, i};
    }
    for (int i = 0; i < MAX_BALLS; i++) {
      if (balls_[i].used) items[n++] = {hypotf(balls_[i].x - x_, balls_[i].y - y_), 1, i};
    }
    for (int i = 1; i < n; i++) {  // insertion sort, far to near
      for (int j = i; j > 0 && items[j].d > items[j - 1].d; j--) {
        const Item t = items[j];
        items[j] = items[j - 1];
        items[j - 1] = t;
      }
    }
    for (int i = 0; i < n; i++) {
      if (items[i].kind == 1) {
        static const char *const BALL[2] = {"##", "##"};
        const Ball &b = balls_[items[i].index];
        sprite(b.x, b.y, BALL, 2, 2, 0.18f, 0.3f, (tick_ / 2) % 2 ? 255 : 200, depth);
        continue;
      }
      const Imp &m = imps_[items[i].index];
      if (m.alive) {
        sprite(m.x, m.y, IMP, 5, 6, 0.8f, 0, m.hurt % 2 ? 90 : 255, depth);
      } else if (m.dying > 0) {
        sprite(m.x, m.y, IMP, 5, 6, 0.8f * m.dying / 10, 0, (m.dying / 2) % 2 ? 255 : 120, depth);
      } else {
        sprite(m.x, m.y, IMP_DEAD, 5, 2, 0.15f, 0, 60, depth);  // what's left of it
      }
    }

    // The gun, and its flash when it fires.
    static const char *const GUN[3] = {".##.", ".##.", "####"};
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 4; c++) {
        if (GUN[r][c] == '#') display.setLevel(6 + c, VIEW_ROWS - 3 + r, 200);
      }
    }
    if (flash_ > 0) {
      display.setLevel(7, VIEW_ROWS - 4, 255);
      display.setLevel(8, VIEW_ROWS - 4, 255);
      if (flash_ > 1) {
        display.setLevel(6, VIEW_ROWS - 5, 180);
        display.setLevel(9, VIEW_ROWS - 5, 180);
      }
    }
    // Hit: the edges of the view light up.
    if (hurt_ > 0 || (dead_ && (dead_ / 3) % 2)) {
      for (int i = 0; i < COLS; i++) {
        display.setLevel(i, 0, 255);
        display.setLevel(i, VIEW_ROWS - 1, 255);
        display.setLevel(0, i < VIEW_ROWS ? i : 0, 255);
        display.setLevel(COLS - 1, i < VIEW_ROWS ? i : 0, 255);
      }
    }
    // Level cleared: the view fills with light.
    if (cleared_) {
      const int lit = (30 - cleared_) * 10;
      for (int i = 0; i < lit && i < COLS * VIEW_ROWS; i++) display.setLevel(i % COLS, i / COLS, 255);
    }
    // Health bar, and a dot per level cleared at its right end.
    const int bar = (health_ * COLS + 99) / 100;
    for (int x = 0; x < COLS; x++) display.setLevel(x, ROWS - 1, x < bar ? (health_ > 25 || (tick_ / 4) % 2 ? 150 : 40) : 0);
  }

  Imp imps_[MAX_IMPS] = {};
  Ball balls_[MAX_BALLS] = {};
  float x_ = START_X, y_ = START_Y, angle_ = 0;
  int health_ = 100, kills_ = 0, level_ = 0;
  int reload_ = 0, flash_ = 0, hurt_ = 0, dead_ = 0, cleared_ = 0;
  uint32_t tick_ = 0;
};

static DoomGame doom;
extern Animation *const doomAnimation = &doom;
