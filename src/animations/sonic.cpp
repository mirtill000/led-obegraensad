// "Sonic": a side-scroller in the style of Sonic the Hedgehog. Sonic runs
// faster and faster over rolling Green Hill ground (the checkerboard soil),
// collecting rings; jumping curls him into a ball that smashes the
// motobugs. Spikes and enemies hit while not in a ball cost all the rings
// (with none left, a life); pits cost a life straight away; springs throw
// him high. → speeds up, ← brakes, the main button (or ↑) jumps. 3 lives;
// the score is distance, rings and enemies. In demo mode the computer
// simulates running on and jumping at each moment ahead, and jumps at the
// first moment that clears everything. Drawn in the games' style (softGames(): shaded
// soil, glinting rings).
#include <math.h>
#include <string.h>

#include "animations/arcade_game.h"
#include "display.h"
#include "settings.h"

namespace {

const int RING = 64;         // world columns kept (ring buffer)
const int AHEAD = 40;        // generated this far past Sonic
const int SONIC_SX = 4;      // Sonic's screen column
const int PIT = ROWS;        // ground height meaning "no ground"
const int MAX_BUGS = 6;
const float GRAVITY = 0.1f;
const float JUMP_SPEED = -1.15f, SPRING_SPEED = -1.7f, BOUNCE_SPEED = -0.9f;
const float MAX_SPEED = 0.85f;

struct Column {
  int8_t h;       // top row of the ground (PIT = pit)
  int8_t ring;    // row of a ring, or -1
  bool spike, spring;
};

struct Bug {
  bool alive;
  float x;    // world column of its left edge
  float dir;  // -1 or 1
};

// Everything the physics touches, so the autopilot can copy it and look
// ahead.
struct State {
  Column cols[RING];
  int32_t generatedTo;
  float x, y, vx, vy;  // Sonic: world column, feet row, speeds
  bool onGround, spinning;
  int invincible;
  int rings, kills;
  Bug bugs[MAX_BUGS];
  bool hurt, dead;  // this frame: lost the rings / a life
};

Column &col(State &s, int32_t x) { return s.cols[((x % RING) + RING) % RING]; }
const Column &col(const State &s, int32_t x) { return s.cols[((x % RING) + RING) % RING]; }
int groundAt(const State &s, float x) { return col(s, (int32_t)floorf(x + 1)).h; }  // under Sonic's middle

}  // namespace

class SonicGame : public ArcadeGame {
 public:
  const char *id() const override { return "sonic"; }
  const char *name() const override { return "Sonic"; }
  uint16_t frameMs() const override { return 35; }

  void start() override {
    memset(&s_, 0, sizeof(s_));
    lives_ = 3;
    tick_ = 0;
    segment_ = 0;
    lastH_ = 12;
    cooldown_ = 12;
    s_.generatedTo = -8;
    generate(s_, 8 + AHEAD);
    placeSonic();
    dying_ = 0;
    pendingFrom_ = -1;
    ringsTotal_ = 0;
    rightUntil_ = leftUntil_ = 0;
  }

  void input(char key) override {
    if (key == 'R') rightUntil_ = tick_ + 4;
    if (key == 'L') leftUntil_ = tick_ + 4;
    if (key == 'A' || key == 'U') jump(s_);
  }

 protected:
  void tick(uint32_t) override {
    tick_++;
    if (dying_) {
      // Sonic pops up and falls off the screen, then the next life.
      dieY_ += dieVy_;
      dieVy_ += GRAVITY;
      if (--dying_ == 0) {
        if (--lives_ <= 0) return gameOver(score());
        placeSonic();
      }
      return draw();
    }
    Plan plan = RUN;
    if (demo_) plan = autopilot();
    const bool right = demo_ ? true : tick_ < rightUntil_;
    const bool left = demo_ ? false : tick_ < leftUntil_;
    if (demo_ && plan == JUMP) jump(s_);
    const int ringsBefore = s_.rings;
    step(s_, right, left);
    if (s_.rings > ringsBefore) ringsTotal_ += s_.rings - ringsBefore;
    if (s_.dead) {
      dying_ = 40;
      dieY_ = s_.y - 3;
      dieVy_ = -1.2f;
    }
    generate(s_, (int32_t)s_.x + AHEAD);
    moveBugsSpawn();
    draw();
  }

 private:
  enum Plan { RUN, JUMP };

  int score() const { return (int)(s_.x / 4) + ringsTotal_ * 10 + s_.kills * 100; }

  // --- the world -------------------------------------------------------------

  // Extends the terrain up to world column `to`: rolling hills (one row up
  // or down at a time), and now and then a pit, spikes, a spring, a row or
  // arc of rings, a motobug - never two hazards close together.
  void generate(State &s, int32_t to) {
    while (s.generatedTo < to) {
      const int32_t x = ++s.generatedTo;
      Column &c = col(s, x);
      c = {(int8_t)lastH_, -1, false, false};
      // Rings of an arc or a spring's reward, laid over the following columns.
      if (pendingFrom_ >= 0 && x >= pendingFrom_) {
        c.ring = pendingRings_[x - pendingFrom_];
        if (x == pendingFrom_ + 4) pendingFrom_ = -1;
      }
      if (cooldown_ > 0) {
        cooldown_--;
        continue;
      }
      const uint32_t r = esp_random() % 100;
      if (x > 30 && r < 6) {  // a pit, 2-3 columns, a ring over it to jump for
        const int w = 2 + esp_random() % 2;
        for (int k = 0; k < w; k++) col(s, x + k) = {(int8_t)PIT, -1, false, false};
        col(s, x + w / 2).ring = lastH_ - 5;
        s.generatedTo += w - 1;
        cooldown_ = 6;
        continue;
      } else if (x > 20 && r < 13) {  // spikes, 1-2 columns
        c.spike = true;
        if (esp_random() % 2) {
          col(s, ++s.generatedTo) = {(int8_t)lastH_, -1, true, false};
        }
        cooldown_ = 6;
      } else if (r < 17) {  // a spring, and rings up high where it throws him
        c.spring = true;
        for (int k = 0; k < 5; k++) pendingRings_[k] = lastH_ - 8;
        pendingFrom_ = x + 3;
        cooldown_ = 8;
      } else if (r < 32) {  // rings: a line or a little arc
        static const int8_t ARC[5] = {2, 3, 4, 3, 2};
        const bool arc = esp_random() % 2;
        for (int k = 0; k < 5; k++) pendingRings_[k] = lastH_ - (arc ? ARC[k] : 2);
        pendingFrom_ = x + 1;
        cooldown_ = 6;
      } else if (x > 16 && r < 42) {
        spawnBug(s, x);
        cooldown_ = 7;
      } else if (r < 70 && ++segment_ % 2) {  // hills
        const int h = lastH_ + (esp_random() % 2 ? 1 : -1);
        lastH_ = constrain(h, 9, 13);
        c.h = lastH_;
      }
    }
  }

  void spawnBug(State &s, int32_t x) {
    for (Bug &b : s.bugs) {
      if (b.alive) continue;
      b = {true, (float)x + 2, -1};
      return;
    }
  }

  void moveBugsSpawn() {
    // Enemies left behind the screen are dropped.
    for (Bug &b : s_.bugs) {
      if (b.alive && b.x < s_.x - SONIC_SX - 4) b.alive = false;
    }
  }

  void placeSonic() {
    s_.x = floorf(s_.x);
    // Onto solid ground with a clear stretch ahead (no pit or spikes for 8
    // columns), so he has a run-up.
    for (int tries = 0; tries < AHEAD - 12; tries++) {
      bool clear = true;
      for (int k = 0; k < 10 && clear; k++) {
        const Column &c = col(s_, (int32_t)s_.x + k);
        clear = c.h != PIT && !c.spike;
      }
      if (clear) break;
      s_.x += 1;
    }
    s_.y = groundAt(s_, s_.x) - 1;
    s_.vx = s_.vy = 0;
    s_.onGround = true;
    s_.spinning = false;
    s_.invincible = 40;
    s_.dead = s_.hurt = false;
    for (Bug &b : s_.bugs) {
      if (b.alive && b.x - s_.x > -4 && b.x - s_.x < 14) b.alive = false;
    }
  }

  // --- physics (shared by the game and the autopilot's look-ahead) -----------

  static void jump(State &s) {
    if (!s.onGround) return;
    s.vy = JUMP_SPEED;
    s.onGround = false;
    s.spinning = true;
  }

  static void hurt(State &s) {
    if (s.invincible > 0) return;
    if (s.rings > 0) {
      s.rings = 0;
      s.invincible = 45;
      s.hurt = true;
    } else {
      s.dead = true;
    }
  }

  void step(State &s, bool right, bool left) const {
    s.hurt = s.dead = false;
    if (s.invincible > 0) s.invincible--;
    // Run: speed up holding →, brake with ←, else roll to a stop slowly.
    if (right) s.vx += s.onGround ? 0.025f : 0.012f;
    else if (left) s.vx -= 0.05f;
    else s.vx -= 0.008f;
    if (s.onGround) {  // slopes: slower uphill, faster downhill
      const int here = groundAt(s, s.x), ahead = groundAt(s, s.x + 1);
      if (ahead != PIT && here != PIT) s.vx += (ahead - here) * 0.012f;
    }
    s.vx = constrain(s.vx, 0.0f, MAX_SPEED);
    s.x += s.vx;

    const int g = groundAt(s, s.x);
    if (s.onGround) {
      if (g == PIT) {
        s.onGround = false;  // ran off into a pit
      } else {
        s.y = g - 1;
        if (col(s, (int32_t)floorf(s.x + 1)).spring) {
          s.vy = SPRING_SPEED;
          s.onGround = false;
          s.spinning = true;
        }
      }
    }
    if (!s.onGround) {
      s.vy += GRAVITY;
      s.y += s.vy;
      if (s.vy > 0 && g != PIT && s.y >= g - 1) {
        s.y = g - 1;
        s.vy = 0;
        s.onGround = true;
        s.spinning = false;
        if (col(s, (int32_t)floorf(s.x + 1)).spring) {
          s.vy = SPRING_SPEED;
          s.onGround = false;
          s.spinning = true;
        }
      }
      if (s.y > ROWS + 3) {
        s.dead = true;  // fell into a pit
        return;
      }
    }

    const int sx = (int)floorf(s.x);
    const int feet = (int)lroundf(s.y);
    // Spikes under his feet.
    if (s.onGround && (col(s, sx + 1).spike || col(s, sx + 2).spike)) hurt(s);
    // Rings he passes through.
    for (int k = 0; k < 3; k++) {
      Column &c = col(s, sx + k);
      if (c.ring >= 0 && c.ring >= feet - 3 && c.ring <= feet) {
        c.ring = -1;
        s.rings++;
      }
    }
    // Motobugs: they trundle along the ground and turn at pits; a ball
    // (jumping) destroys them and bounces off, anything else is a hit.
    for (Bug &b : s.bugs) {
      if (!b.alive) continue;
      const float nx = b.x + b.dir * 0.03f;
      if (col(s, (int32_t)floorf(nx + (b.dir > 0 ? 2 : 0))).h == PIT) b.dir = -b.dir;
      else b.x = nx;
      const int bx = (int)floorf(b.x), by = col(s, bx + 1).h - 1;
      const bool overlapX = bx < sx + 3 && sx < bx + 3;
      const bool overlapY = feet >= by - 1 && feet - 3 <= by;
      if (!overlapX || !overlapY) continue;
      if (s.spinning) {
        b.alive = false;
        s.kills++;
        s.vy = BOUNCE_SPEED;
        s.onGround = false;
      } else {
        hurt(s);
      }
    }
  }

  // --- demo ------------------------------------------------------------------

  // Frames survived (up to `horizon`) running on and jumping after `jumpAt`
  // frames (-1: never). Once a jump has landed the plan counts as a
  // success: the next obstacle gets its own decision.
  int survive(int jumpAt, int horizon) const {
    State s = s_;
    s.invincible = 0;  // judge hazards as if he could be hit: blinking only postpones it
    bool jumped = false;
    for (int f = 0; f < horizon; f++) {
      if (f >= jumpAt && jumpAt >= 0 && !jumped && s.onGround) {
        jump(s);
        jumped = true;
      }
      step(s, true, false);
      if (s.dead || s.hurt) return f;
      if (jumped && s.onGround) return horizon;
    }
    return horizon;
  }

  // Keeps running while that is safe; otherwise finds the soonest moment to
  // jump that clears everything (jumping now only if waiting won't do - so
  // he doesn't come down on the obstacle), and with no way through, the
  // plan that lasts longest.
  Plan autopilot() const {
    const int horizon = 34;
    if (!s_.onGround) return RUN;
    if (survive(-1, horizon) == horizon) {
      // Safe: jump for rings just above when that is safe too.
      for (int k = 2; k < 5; k++) {
        const Column &c = col(s_, (int32_t)s_.x + k);
        if (c.ring >= 0 && c.ring < s_.y - 3 && survive(0, horizon) == horizon) return JUMP;
      }
      return RUN;
    }
    int best = 0, bestFrames = -1;
    for (int k = 0; k < horizon; k += 1) {
      const int f = survive(k, horizon);
      if (f == horizon) return k == 0 ? JUMP : RUN;  // a later jump works: keep running
      if (f > bestFrames) {
        bestFrames = f;
        best = k;
      }
    }
    return best == 0 ? JUMP : RUN;
  }

  // --- drawing ---------------------------------------------------------------

  void draw() {
    display.clear();
    const bool soft = softGames();
    const int32_t cam = (int32_t)floorf(s_.x) - SONIC_SX;
    for (int sx = 0; sx < COLS; sx++) {
      const int32_t wx = cam + sx;
      const Column &c = col(s_, wx);
      if (c.h < PIT) {
        display.setLevel(sx, c.h, soft ? 200 : 255);  // the grass edge
        for (int y = c.h + 1; y < ROWS; y++) {
          const bool light = (((wx >> 1) + ((y - c.h) >> 1)) & 1) == 0;  // Green Hill checkerboard
          if (soft) display.setLevel(sx, y, light ? 90 : 35);
          else if (light) display.setPixel(sx, y, true);
        }
        if (c.spike) {
          display.setLevel(sx, c.h - 1, 255);
          if (wx % 2 == 0) display.setLevel(sx, c.h - 2, soft ? 160 : 255);
        }
        if (c.spring) {
          display.setLevel(sx, c.h - 1, soft ? 180 : 255);
          if ((tick_ / 3) % 2) display.setLevel(sx, c.h - 2, soft ? 110 : 255);
        }
      }
      if (c.ring >= 0) {
        const bool glint = ((tick_ / 3) + wx) % 4 == 0;
        if (soft) display.setLevel(sx, c.ring, glint ? 255 : 150);
        else if (!glint || (tick_ / 2) % 2) display.setPixel(sx, c.ring, true);
      }
    }
    // Motobugs: a body and two wheels.
    for (const Bug &b : s_.bugs) {
      if (!b.alive) continue;
      const int bx = (int)floorf(b.x) - cam, by = col(s_, (int32_t)floorf(b.x) + 1).h - 1;
      static const char *const BUG[2] = {"###", "#.#"};
      for (int r = 0; r < 2; r++) {
        for (int k = 0; k < 3; k++) {
          if (BUG[r][k] == '#') display.setLevel(bx + k, by - 1 + r, 255);
        }
      }
    }
    // Sonic: running (legs going) or curled in a spinning ball; blinking
    // while invincible; flying off the screen when he loses a life.
    static const char *const RUN_FRAMES[2][4] = {{"##.", ".##", ".#.", "#.#"}, {"##.", ".##", ".#.", ".#."}};
    static const char *const BALL[2][3] = {{".#.", "###", ".#."}, {"#.#", ".#.", "#.#"}};
    const bool show = dying_ || s_.invincible == 0 || (tick_ / 2) % 2;
    if (show) {
      const int top = dying_ ? (int)lroundf(dieY_) : (int)lroundf(s_.y) - 3;
      if (s_.spinning && !dying_) {
        const char *const *b = BALL[(tick_ / 2) % 2];
        for (int r = 0; r < 3; r++) {
          for (int k = 0; k < 3; k++) {
            if (b[r][k] == '#') display.setLevel(SONIC_SX + k, top + 1 + r, 255);
          }
        }
      } else {
        const char *const *f = RUN_FRAMES[s_.vx > 0.05f ? (tick_ / 3) % 2 : 0];
        for (int r = 0; r < 4; r++) {
          for (int k = 0; k < 3; k++) {
            if (f[r][k] == '#') display.setLevel(SONIC_SX + k, top + r, 255);
          }
        }
      }
    }
    // Rings collected along the top row (up to 16), lives in the corner.
    for (int i = 0; i < s_.rings && i < COLS - 3; i++) display.setLevel(i, 0, soft ? 120 : 255);
    for (int i = 0; i < lives_ - 1; i++) display.setLevel(COLS - 1 - i * 2, 0, 255);
  }

  State s_;
  int lives_ = 3, dying_ = 0, segment_ = 0, lastH_ = 12, cooldown_ = 0;
  int8_t pendingRings_[5] = {};
  int32_t pendingFrom_ = -1;
  int ringsTotal_ = 0;
  float dieY_ = 0, dieVy_ = 0;
  uint32_t tick_ = 0, rightUntil_ = 0, leftUntil_ = 0;
};

static SonicGame sonic;
extern Animation *const sonicAnimation = &sonic;
