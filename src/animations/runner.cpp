// "Dino": the endless runner of Chrome's offline page. The dinosaur runs
// on the ground line; cacti and pterodactyls come at it faster and faster.
// ↑ or the main button jumps, ↓ ducks. In demo mode the computer simulates
// running on, jumping and ducking and takes the first choice that keeps it
// alive. Drawn in the games' style (softGames(): a fainter ground).
#include <math.h>

#include "animations/arcade_game.h"
#include "display.h"
#include "settings.h"

namespace {

const int GROUND = ROWS - 1;  // the ground line
const int DINO_X = 1;
const float GRAVITY = 0.12f;
const float JUMP_SPEED = -1.35f;  // about 7 rows high
const uint32_t DUCK_MS = 350;     // one press ducks this long (hold = repeat)

// Sprites, top row first ('#' lit). The dino stands on the row above the
// ground; its feet alternate while running.
const char *const DINO_RUN[2][7] = {
    {"...###", "...#.#", "...###", "#.###.", "####..", ".###..", ".#..#."},
    {"...###", "...#.#", "...###", "#.###.", "####..", ".###..", "..##.."},
};
const char *const DINO_DUCK[2][4] = {
    {"....##", "######", ".####.", ".#..#."},
    {"....##", "######", ".####.", "..##.."},
};
const char *const CACTUS[3][5] = {
    {".#.", "##.", ".##", ".#.", ".#."},       // 3 wide, 5 tall
    {"#", "#", "#", "#"},                      // 1 wide, 4 tall
    {"#.#", "###", ".#.", ".#.", ".#."},      // 3 wide, 5 tall
};
const int CACTUS_ROWS[3] = {5, 4, 5};
const char *const BIRD[2][3] = {
    {"#....", ".###.", "...##"},
    {".....", ".####", "#..#."},
};

struct Obstacle {
  float x;
  int kind;   // 0-2 cactus, 3 bird
  int y;      // top row
  bool used;
};

}  // namespace

class RunnerGame : public ArcadeGame {
 public:
  const char *id() const override { return "dino"; }
  const char *name() const override { return "Dino"; }
  uint16_t frameMs() const override { return 35; }

  void start() override {
    for (Obstacle &o : obstacles_) o.used = false;
    y_ = 0;
    vy_ = 0;
    speed_ = 0.45f;
    distance_ = 0;
    duckUntil_ = 0;
    tick_ = 0;
    nextGap_ = 12;
  }

  void input(char key) override {
    if (key == 'U' || key == 'A') jump();
    if (key == 'D') duckUntil_ = tick_ + DUCK_MS / 35;
  }

 protected:
  void tick(uint32_t) override {
    tick_++;
    if (demo_) autopilot();
    State s = state();
    advance(s);
    load(s);
    spawn();
    if (collides(s, ducking())) {
      draw();
      gameOver((int)distance_ / 4);
      return;
    }
    // Faster over time, up to about twice the start speed.
    speed_ = min(0.9f, 0.45f + distance_ / 6000.0f);
    draw();
  }

 private:
  static const int MAX_OBSTACLES = 4;

  // What the physics touches, so the autopilot can look ahead.
  struct State {
    float y, vy;  // height above the ground (0 = on it), vertical speed
    float distance;
    Obstacle obstacles[MAX_OBSTACLES];
  };

  State state() const {
    State s{y_, vy_, distance_, {}};
    for (int i = 0; i < MAX_OBSTACLES; i++) s.obstacles[i] = obstacles_[i];
    return s;
  }
  void load(const State &s) {
    y_ = s.y;
    vy_ = s.vy;
    distance_ = s.distance;
    for (int i = 0; i < MAX_OBSTACLES; i++) obstacles_[i] = s.obstacles[i];
  }

  bool ducking() const { return tick_ < duckUntil_ && y_ == 0; }
  static void startJump(State &s) {
    if (s.y > 0) return;
    s.vy = JUMP_SPEED;
    s.y = 0.01f;  // off the ground, so advance() moves it
  }
  void jump() {
    State s = state();
    startJump(s);
    load(s);
  }

  void advance(State &s) const {
    if (s.y > 0 || s.vy < 0) {
      s.y -= s.vy;
      s.vy += GRAVITY;
      if (s.y <= 0) s.y = s.vy = 0;
    }
    s.distance += speed_;
    for (Obstacle &o : s.obstacles) {
      if (!o.used) continue;
      o.x -= speed_;
      if (o.x < -6) o.used = false;
    }
  }

  // The dino's sprite rows and top-left corner for a state.
  void dinoSprite(const State &s, bool duck, const char *const **rows, int &count, int &top) const {
    const int run = (int)(s.distance / 2) % 2;
    if (duck) {
      *rows = DINO_DUCK[run];
      count = 4;
    } else {
      *rows = DINO_RUN[run];
      count = 7;
    }
    top = GROUND - count - (int)lroundf(s.y);
  }

  static void obstacleSprite(const Obstacle &o, uint32_t tick, const char *const **rows, int &count) {
    if (o.kind == 3) {
      *rows = BIRD[(tick / 6) % 2];
      count = 3;
    } else {
      *rows = CACTUS[o.kind];
      count = CACTUS_ROWS[o.kind];
    }
  }

  // Pixel-perfect: any lit pixel of the dino on a lit pixel of an obstacle.
  bool collides(const State &s, bool duck) const {
    const char *const *dino;
    int dRows, dTop;
    dinoSprite(s, duck, &dino, dRows, dTop);
    for (const Obstacle &o : s.obstacles) {
      if (!o.used) continue;
      const char *const *rows;
      int count;
      obstacleSprite(o, tick_, &rows, count);
      const int ox = (int)lroundf(o.x);
      for (int r = 0; r < count; r++) {
        for (int c = 0; rows[r][c]; c++) {
          if (rows[r][c] != '#') continue;
          const int x = ox + c - DINO_X, y = o.y + r - dTop;
          if (y < 0 || y >= dRows || x < 0 || x >= (int)strlen(dino[y])) continue;
          if (dino[y][x] == '#') return true;
        }
      }
    }
    return false;
  }

  void spawn() {
    float last = -100;
    for (const Obstacle &o : obstacles_) {
      if (o.used) last = max(last, o.x);
    }
    if (last > COLS - nextGap_) return;
    for (Obstacle &o : obstacles_) {
      if (o.used) continue;
      o.used = true;
      o.x = COLS;
      // Birds only once the dino is up to speed; at three heights.
      const bool bird = distance_ > 400 && esp_random() % 4 == 0;
      if (bird) {
        static const int HEIGHTS[3] = {GROUND - 3, GROUND - 7, GROUND - 10};  // jump over / duck / run under
        o.kind = 3;
        o.y = HEIGHTS[esp_random() % 3];
      } else {
        o.kind = esp_random() % 3;
        o.y = GROUND - CACTUS_ROWS[o.kind];
      }
      nextGap_ = 11 + esp_random() % 10 + (int)(speed_ * 8);
      return;
    }
  }

  // Frames survived (up to `horizon`) with this plan: jump now, duck for a
  // while, or just run.
  enum Plan { RUN, JUMP, DUCK };
  int survive(Plan plan, int horizon) const {
    State s = state();
    if (plan == JUMP) startJump(s);
    for (int f = 0; f < horizon; f++) {
      const bool duck = plan == DUCK && s.y == 0;
      advance(s);
      if (collides(s, duck)) return f;
    }
    return horizon;
  }

  // Keeps running while that is safe; otherwise ducks or jumps as soon as
  // doing so clears everything in sight - not earlier, or the jump would
  // come down on the obstacle - and only when a hit is imminent takes
  // whichever choice lasts longest.
  void autopilot() {
    if (y_ > 0) return;  // in the air: nothing to decide
    const int horizon = 40;
    const int run = survive(RUN, horizon);
    if (run == horizon) return;
    const int duck = survive(DUCK, horizon), leap = survive(JUMP, horizon);
    if (duck == horizon) {
      duckUntil_ = tick_ + 3;
    } else if (leap == horizon) {
      jump();
    } else if (run <= 2) {
      if (leap >= duck && leap > run) jump();
      else if (duck > run) duckUntil_ = tick_ + 3;
    }
  }

  void draw() {
    display.clear();
    // Ground with a few gaps that scroll by.
    const int shift = (int)distance_;
    const uint8_t ground = softGames() ? 90 : 255;  // "sfumata": a fainter ground
    for (int x = 0; x < COLS; x++) display.setLevel(x, GROUND, (x + shift) % 9 != 0 ? ground : 0);
    for (const Obstacle &o : obstacles_) {
      if (!o.used) continue;
      const char *const *rows;
      int count;
      obstacleSprite(o, tick_, &rows, count);
      const int ox = (int)lroundf(o.x);
      for (int r = 0; r < count; r++) {
        for (int c = 0; rows[r][c]; c++) {
          if (rows[r][c] == '#') display.setPixel(ox + c, o.y + r, true);
        }
      }
    }
    const char *const *dino;
    int count, top;
    dinoSprite(state(), ducking(), &dino, count, top);
    for (int r = 0; r < count; r++) {
      for (int c = 0; dino[r][c]; c++) {
        if (dino[r][c] == '#') display.setPixel(DINO_X + c, top + r, true);
      }
    }
  }

  Obstacle obstacles_[MAX_OBSTACLES];
  float y_ = 0, vy_ = 0, speed_ = 0.45f, distance_ = 0;
  uint32_t tick_ = 0, duckUntil_ = 0;
  int nextGap_ = 12;
};

static RunnerGame runner;
extern Animation *const runnerAnimation = &runner;
