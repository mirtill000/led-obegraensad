// "Donkey Kong" on 16x16: four floors joined by ladders, Donkey Kong at
// the top left, Pauline at the top right, Mario starting at the bottom and
// climbing up to her: ← → walk, ↑ ↓ climb, the main button jumps (over a
// barrel or a fire for points). Three levels, in turn, faster each time
// round:
//   1 Barili  - Kong throws barrels that roll down in a zigzag (sometimes
//               down a ladder)
//   2 Fiamme  - no barrels, but fires (the first from the oil drum at
//               the bottom, the others on the middle floors) wander the floors, climbing the ladders and
//               drifting towards Mario
//   3 Tutto   - barrels and fires together, new ladders
// 3 lives. In demo mode Mario heads for the nearest ladder up, jumps what
// comes at him, waits on the ladder (head still below the floor) while
// something passes the top and backs off from what comes down it.
//
// The characters are small sprites in gray levels (softGames(); in the
// "nitida" style only the brighter parts light up): Mario 2x3 with cap,
// face and walking legs, facing where he goes; Kong 5x3, arms up when he
// throws; Pauline 2x3.
//
//   row 0-2  Kong (x0-4)                          Pauline (x14-15)
//   row 3    floor 3 ----------------------------------------------
//   row 7    floor 2            (ladders: dotted, with a gap in the
//   row 11   floor 1             floor above them)
//   row 15   floor 0
#include "animations/arcade_game.h"
#include "display.h"
#include "pager.h"
#include "settings.h"

namespace {

const int FLOORS = 4;
const int FLOOR_ROW[FLOORS] = {15, 11, 7, 3};
const int CLIMB_STEPS = 4;  // rows from one floor's feet row to the next
const int PAULINE_X = 14;
const int KONG_X = 0;
const int MARIO_STEP = 3;   // ticks per Mario step
const int JUMP_TICKS = 12;  // time in the air
const int MAX_BARRELS = 8, MAX_FIRES = 4;
const int INTRO_FRAMES = 45;

struct Level {
  int8_t ladders[FLOORS - 1][2];  // x of the ladders from floor i up to i + 1 (-1: none)
  int8_t rollDir[FLOORS];         // barrels' direction on each floor
  bool barrels;
  uint8_t fires;  // how many fires the oil drum lets out (0: no drum)
};

const Level LEVELS[3] = {
    {{{13, -1}, {2, -1}, {10, -1}}, {-1, 1, -1, 1}, true, 0},
    {{{12, -1}, {3, -1}, {12, -1}}, {-1, 1, -1, 1}, false, 4},
    {{{8, 13}, {3, 11}, {12, -1}}, {-1, 1, -1, 1}, true, 2},
};

struct Barrel {
  bool used;
  int x, y;      // the barrel's pixel
  int floor;     // floor it rolls on (while rolling)
  bool falling;  // dropping to the floor below
  bool jumped;   // Mario already scored for it
};

struct Fire {
  bool used;
  int x, floor, climb;  // like Mario: climb 0 = on the floor
  int dir;              // -1 / 1 along the floor
  int climbDir;         // 1 up, -1 down, 0 walking
  bool jumped;
  int feet() const { return FLOOR_ROW[floor] - 1 - climb; }
};

// Sprites: gray levels, row by row, left to right (0 = nothing).
const uint8_t KONG[2][3][5] = {
    {{0, 200, 200, 200, 0}, {255, 110, 50, 110, 255}, {255, 255, 0, 255, 255}},    // standing
    {{255, 200, 200, 200, 255}, {0, 255, 50, 255, 0}, {255, 255, 0, 255, 255}},    // arms up: throwing
};
const uint8_t PAULINE[3][2] = {{200, 255}, {255, 150}, {255, 255}};

}  // namespace

class KongGame : public ArcadeGame {
 public:
  const char *id() const override { return "kong"; }
  const char *name() const override { return "Donkey Kong"; }
  const GameControls *controls() const override {
    static const GameControls c = {"LRUDA", {nullptr, nullptr, nullptr, nullptr, "Salta"}, true,
                                   "← → per camminare, ↑ ↓ per le scale, Salta per scavalcare i barili. Tastiera: frecce e spazio."};
    return &c;
  }
  uint16_t frameMs() const override { return 40; }

  void start() override {
    lives_ = 3;
    points_ = 0;
    round_ = 0;
    newRound();
  }

  void input(char key) override {
    if (dying_ || won_ || intro_) return;
    if (key == 'L' || key == 'R') walk(key == 'L' ? -1 : 1);
    if (key == 'U') climb(1);
    if (key == 'D') climb(-1);
    if (key == 'A') jump();
  }

 protected:
  void tick(uint32_t) override {
    tick_++;
    if (intro_) {  // "LIV 2" before each level
      intro_--;
      return drawIntro();
    }
    if (dying_) {
      if (--dying_ == 0) {
        if (--lives_ <= 0) return gameOver(points_);
        placeMario();
        for (Barrel &b : barrels_) b.used = false;
        for (Fire &f : fires_) f.used = false;
        nextFire_ = tick_ + 30;
      }
      return draw();
    }
    if (won_) {
      if (--won_ == 0) {
        round_++;
        newRound();
      }
      return draw();
    }

    if (demo_ && tick_ % MARIO_STEP == 0) autopilot();
    if (jump_ > 0) jump_--;
    if (walkAnim_ > 0) walkAnim_--;
    moveBarrels();
    throwBarrel();
    moveFires();
    releaseFire();
    if (hit()) {
      dying_ = 40;
    } else if (floor_ == FLOORS - 1 && climb_ == 0 && x_ >= PAULINE_X - 2) {
      points_ += 500;
      won_ = 40;
    }
    draw();
  }

 private:
  const Level &level() const { return LEVELS[round_ % 3]; }
  int speedUp() const { return round_ / 3; }  // one step faster every full cycle

  void newRound() {
    for (Barrel &b : barrels_) b.used = false;
    for (Fire &f : fires_) f.used = false;
    placeMario();
    nextThrow_ = tick_ + INTRO_FRAMES + 30;
    nextFire_ = tick_ + INTRO_FRAMES + 15;
    firesOut_ = 0;
    intro_ = INTRO_FRAMES;
  }

  void placeMario() {
    floor_ = 0;
    x_ = level().fires ? 4 : 1;  // clear of the oil drum
    climb_ = 0;
    jump_ = 0;
    facing_ = 1;
  }

  // Mario's feet row (his body is the two rows above).
  int feet() const { return FLOOR_ROW[floor_] - 1 - climb_ - (jump_ > 0 ? 1 : 0); }
  // The column in front of him, where his sprite's cap brim and leg go.
  int front() const { return x_ + facing_; }

  bool ladderAt(int floor, int x) const {
    if (floor < 0 || floor >= FLOORS - 1) return false;
    for (int8_t lx : level().ladders[floor]) {
      if (lx == x) return true;
    }
    return false;
  }
  bool onLadderBottom() const { return ladderAt(floor_, x_); }
  bool onLadderTop() const { return floor_ > 0 && ladderAt(floor_ - 1, x_); }

  void walk(int dir) {
    if (climb_ > 0) return;  // on a ladder
    facing_ = dir;
    x_ = constrain(x_ + dir, 0, COLS - 1);
    walkAnim_ = 6;
  }

  void climb(int dir) {
    if (jump_ > 0) return;
    if (dir > 0 && (climb_ > 0 || onLadderBottom())) {
      if (++climb_ == CLIMB_STEPS) {
        floor_++;
        climb_ = 0;
      }
    } else if (dir < 0) {
      if (climb_ > 0) {
        climb_--;
      } else if (onLadderTop()) {
        floor_--;
        climb_ = CLIMB_STEPS - 1;
      }
    }
  }

  void jump() {
    if (climb_ == 0 && jump_ == 0) jump_ = JUMP_TICKS;
  }

  // --- barrels ----------------------------------------------------------------

  void throwBarrel() {
    if (!level().barrels || tick_ < nextThrow_) return;
    for (Barrel &b : barrels_) {
      if (b.used) continue;
      b = {true, KONG_X + 5, FLOOR_ROW[FLOORS - 1] - 1, FLOORS - 1, false, false};
      throwing_ = 8;
      break;
    }
    const int spread = max(30, 90 - (round_ + speedUp() * 3) * 6);
    nextThrow_ = tick_ + spread / 2 + esp_random() % spread;
  }

  int barrelStep() const { return max(2, 4 - speedUp()); }

  void moveBarrels() {
    if (throwing_ > 0) throwing_--;
    if (tick_ % barrelStep() != 0) return;
    for (Barrel &b : barrels_) {
      if (!b.used) continue;
      if (b.falling) {
        b.y++;
        if (b.y == FLOOR_ROW[b.floor] - 1) b.falling = false;  // landed on the floor below
        continue;
      }
      // Now and then a barrel takes a ladder down instead of rolling on.
      if (ladderAt(b.floor - 1, b.x) && esp_random() % 4 == 0) {
        b.floor--;
        b.falling = true;
        b.y++;
        continue;
      }
      const int next = b.x + level().rollDir[b.floor];
      if (next < 0 || next >= COLS) {  // off the end of the floor
        if (b.floor == 0) {
          b.used = false;  // gone (into the oil drum)
        } else {
          b.floor--;
          b.falling = true;
          b.y++;
        }
        continue;
      }
      b.x = next;
      if (!b.jumped && jump_ > 0 && b.x == x_ && b.y == feet() + 1) {
        b.jumped = true;
        points_ += 100;
      }
    }
  }

  // --- fires ------------------------------------------------------------------

  void releaseFire() {
    if (!level().fires || firesOut_ >= level().fires || tick_ < nextFire_) return;
    for (Fire &f : fires_) {
      if (f.used) continue;
      if (firesOut_ == 0) {
        f = {true, 2, 0, 0, 1, 0, false};  // the first one out of the drum
      } else {
        // The others turn up on the middle floors, away from Mario.
        const int floor = 1 + esp_random() % 2;
        const int x = x_ < COLS / 2 ? COLS - 3 : 2;
        f = {true, x, floor, 0, x < COLS / 2 ? 1 : -1, 0, false};
      }
      firesOut_++;
      break;
    }
    nextFire_ = tick_ + 40 + esp_random() % 40;
  }

  int fireStep() const { return max(3, 5 - speedUp()); }

  // Fires wander: along the floor (turning at the ends or on a whim), up a
  // ladder they meet now and then, down one sometimes.
  void moveFires() {
    if (tick_ % fireStep() != 0) return;
    for (Fire &f : fires_) {
      if (!f.used) continue;
      if (f.climbDir > 0) {
        if (++f.climb == CLIMB_STEPS) {
          f.floor++;
          f.climb = 0;
          f.climbDir = 0;
        }
        continue;
      }
      if (f.climbDir < 0) {
        if (f.climb > 0) {
          if (--f.climb == 0) f.climbDir = 0;
        } else {
          f.floor--;
          f.climb = CLIMB_STEPS - 1;
        }
        continue;
      }
      // Up the ladder here (not to the top floor: that's Pauline's), or down one.
      if (ladderAt(f.floor, f.x) && f.floor < FLOORS - 2 && esp_random() % 3 == 0) {
        f.climbDir = 1;
        f.climb = 1;
        continue;
      }
      if (f.floor > 0 && ladderAt(f.floor - 1, f.x) && esp_random() % 5 == 0) {
        f.climbDir = -1;
        f.floor--;
        f.climb = CLIMB_STEPS - 1;
        continue;
      }
      // On Mario's floor they tend to drift towards him, as in the arcade.
      if (f.floor == floor_ && climb_ == 0 && f.x != x_ && esp_random() % 3 == 0) f.dir = f.x < x_ ? 1 : -1;
      else if (esp_random() % 10 == 0) f.dir = -f.dir;
      int next = f.x + f.dir;
      if (next < 1 || next >= COLS) {  // turn at the ends (and at the drum)
        f.dir = -f.dir;
        next = f.x + f.dir;
      }
      f.x = next;
      if (!f.jumped && jump_ > 0 && climb_ == 0 && f.floor == floor_ && f.climb == 0 && f.x == x_) {
        f.jumped = true;
        points_ += 100;
      }
    }
  }

  // --- collisions and the demo ------------------------------------------------

  // The hit box is Mario's own column (the cap brim and the leg in front are
  // drawn but don't count, as in the arcade).
  bool touches(int x, int y) const {
    const int f = feet();
    return x == x_ && (y == f || y == f - 1);
  }

  bool hit() const {
    for (const Barrel &b : barrels_) {
      if (b.used && touches(b.x, b.y)) return true;
    }
    for (const Fire &f : fires_) {
      if (f.used && touches(f.x, f.feet())) return true;
    }
    return false;
  }

  // Something on `floor` (rolling barrel or walking fire) that could reach
  // column x within `steps`.
  bool dangerComing(int floor, int x, int steps) const {
    for (const Barrel &b : barrels_) {
      if (!b.used || b.floor != floor) continue;
      const int d = (x - b.x) * level().rollDir[floor];  // > 0: heading for x
      // Rolling towards x, or about to land close by and roll on.
      if (d >= -1 && d <= steps + (b.falling ? 1 : 0)) return true;
    }
    for (const Fire &f : fires_) {
      if (!f.used || f.floor != floor || f.climb != 0) continue;
      if (abs(f.x - x) <= steps / 2 + 1) return true;  // fires wander both ways
    }
    return false;
  }

  // Something coming down column x (a falling barrel, a fire on the ladder)
  // above row y.
  bool dropping(int x, int y) const {
    for (const Barrel &b : barrels_) {
      if (b.used && b.falling && b.x == x && b.y < y) return true;
    }
    for (const Fire &f : fires_) {
      if (f.used && f.climbDir != 0 && f.x == x && f.feet() < y) return true;
    }
    return false;
  }

  // The ladder up from Mario's floor nearest to him (or Pauline on top).
  int target() const {
    if (floor_ == FLOORS - 1) return PAULINE_X - 2;
    int best = -1;
    for (int8_t lx : level().ladders[floor_]) {
      if (lx >= 0 && (best < 0 || abs(lx - x_) < abs(best - x_))) best = lx;
    }
    return best;
  }

  void autopilot() {
    if (jump_ > 0) return;
    // Something coming down the ladder onto Mario: back down, or step aside.
    if (dropping(x_, feet())) {
      if (climb_ > 0) {
        climb(-1);
      } else {
        const int side = x_ > 0 && !dangerComing(floor_, x_ - 1, 2) ? -1 : 1;
        walk(side);
      }
      return;
    }
    if (climb_ > 0) {
      // Two rungs from the top the head is still below the floor above:
      // wait there until nothing is about to pass the top of the ladder.
      if (climb_ < 2 || !dangerComing(floor_ + 1, x_, 3)) climb(1);
      return;
    }
    // Something about to reach Mario on his floor: jump it.
    if (dangerComing(floor_, x_, 2)) {
      jump();
      return;
    }
    const int goal = target();
    if (x_ != goal) {
      walk(goal > x_ ? 1 : -1);
    } else {
      climb(1);
    }
  }

  // --- drawing ----------------------------------------------------------------

  // A sprite pixel: its gray level in the "sfumata" style; in the "nitida"
  // one only the brighter parts light up, fully.
  void px(int x, int y, uint8_t level) {
    if (!level) return;
    if (softGames()) display.setLevel(x, y, level);
    else if (level >= 140) display.setPixel(x, y, true);
  }

  void drawMario() {
    const int f = feet();
    const bool onLadder = climb_ > 0;
    const int step = (tick_ / 4) % 2;
    uint8_t cap[2], body[2], legs[2];
    if (onLadder) {  // from behind, arms and legs going
      cap[0] = cap[1] = 200;
      body[0] = body[1] = 255;
      legs[0] = step ? 255 : 0;
      legs[1] = step ? 0 : 255;
    } else {
      cap[0] = 255;                 // cap
      cap[1] = 200;                 // its brim, in front
      body[0] = 170;                // face
      body[1] = 110;                // nose / hand
      if (jump_ > 0) {
        legs[0] = legs[1] = 255;    // legs apart
      } else if (walkAnim_ > 0) {
        legs[0] = step ? 255 : 0;
        legs[1] = step ? 0 : 255;
      } else {
        legs[0] = 255;
        legs[1] = 0;
      }
    }
    // Column 0 is Mario's own (x), column 1 the one he faces.
    const int cols[2] = {x_, front()};
    for (int i = 0; i < 2; i++) {
      px(cols[i], f - 2, cap[i]);
      px(cols[i], f - 1, body[i]);
      px(cols[i], f, legs[i]);
    }
  }

  void drawIntro() {
    display.clear();
    Pager::drawText((COLS - Pager::textWidth("LIV")) / 2, 3, "LIV");
    const String n(round_ % 3 + 1);
    Pager::drawText((COLS - Pager::textWidth(n)) / 2, 9, n);
  }

  void draw() {
    display.clear();
    const bool flash = won_ > 0 && (won_ / 4) % 2;
    const bool soft = softGames();
    const uint8_t floorLevel = soft && !flash ? 110 : 255, ladderLevel = soft ? 70 : 255;
    // Floors, with a gap where a ladder comes up through them.
    for (int f = 0; f < FLOORS; f++) {
      for (int x = 0; x < COLS; x++) {
        const bool gap = f > 0 && ladderAt(f - 1, x);
        display.setLevel(x, FLOOR_ROW[f], !gap || flash ? floorLevel : 0);
      }
    }
    // Ladders: rungs just below the floor above and just above the floor below.
    for (int f = 0; f < FLOORS - 1; f++) {
      for (int8_t lx : level().ladders[f]) {
        if (lx < 0) continue;
        display.setLevel(lx, FLOOR_ROW[f + 1] + 1, ladderLevel);
        display.setLevel(lx, FLOOR_ROW[f] - 1, ladderLevel);
      }
    }
    // The oil drum, a flame flickering on top.
    if (level().fires) {
      for (int x = 0; x < 2; x++) {
        px(x, FLOOR_ROW[0] - 1, 160);
        px(x, FLOOR_ROW[0] - 2, 160);
      }
      px((tick_ / 3) % 2, FLOOR_ROW[0] - 3, (tick_ / 2) % 2 ? 255 : 140);
    }
    // Donkey Kong, arms up while throwing.
    const uint8_t(*k)[5] = KONG[throwing_ > 0];
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 5; c++) px(KONG_X + c, r, k[r][c]);
    }
    // Pauline; a heart over her when Mario gets there.
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 2; c++) px(PAULINE_X + c, r, PAULINE[r][c]);
    }
    if (won_ && (won_ / 3) % 2) px(PAULINE_X - 2, 0, 255);
    // Barrels roll: bright and dim in turn as they go.
    for (const Barrel &b : barrels_) {
      if (b.used) px(b.x, b.y, (b.x + b.y) % 2 ? 255 : 170);
    }
    // Fires flicker.
    for (const Fire &f : fires_) {
      if (!f.used) continue;
      px(f.x, f.feet(), (tick_ + f.x) % 3 ? 255 : 150);
      if (soft && (tick_ / 2) % 2) display.setLevel(f.x, f.feet() - 1, 60);  // the flame's tip
    }
    // Mario (blinking while he loses a life); spare lives as dots on the
    // top row, between Kong and Pauline.
    if (!dying_ || (dying_ / 4) % 2) drawMario();
    for (int i = 0; i < lives_ - 1; i++) display.setPixel(7 + i * 2, 0, true);
  }

  Barrel barrels_[MAX_BARRELS] = {};
  Fire fires_[MAX_FIRES] = {};
  int floor_ = 0, x_ = 1, climb_ = 0, jump_ = 0, facing_ = 1, walkAnim_ = 0;
  int lives_ = 3, points_ = 0, round_ = 0, firesOut_ = 0;
  int dying_ = 0, won_ = 0, throwing_ = 0, intro_ = 0;
  uint32_t tick_ = 0, nextThrow_ = 0, nextFire_ = 0;
};

static KongGame kong;
extern Animation *const kongAnimation = &kong;
