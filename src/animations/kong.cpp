// "Donkey Kong" on 16x16: four floors joined by ladders, Donkey Kong at
// the top left throwing barrels that roll down the floors in a zigzag (and
// sometimes down a ladder), Pauline at the top right. Mario starts at the
// bottom left and climbs up to her: ← → walk, ↑ ↓ climb, the main button
// jumps (over a barrel for points). 3 lives; each rescue makes the next
// round faster. In demo mode Mario heads for the next ladder, jumps the
// barrels coming at him, waits on the ladder (head still below the floor)
// while a barrel passes the top, and gets out of the way of barrels coming
// down a ladder. Drawn in the games' style (softGames(): dimmer floors and
// ladders).
//
//   row 0-2  Donkey Kong (x0-3)                 Pauline (x14, rows 1-2)
//   row 3    floor 3 ----------------------------------------------
//   row 7    floor 2        ladders: 0-1 at x13, 1-2 at x2, 2-3 at x10
//   row 11   floor 1        (dotted, with a gap in the floor above)
//   row 15   floor 0
#include "animations/arcade_game.h"
#include "display.h"
#include "settings.h"

namespace {

const int FLOORS = 4;
const int FLOOR_ROW[FLOORS] = {15, 11, 7, 3};
const int LADDER_X[FLOORS - 1] = {13, 2, 10};  // from floor i up to i + 1
const int ROLL_DIR[FLOORS] = {-1, 1, -1, 1};   // barrels' direction on each floor
const int CLIMB_STEPS = 4;                     // rows from one floor's feet row to the next
const int PAULINE_X = 14;
const int MARIO_STEP = 3;   // ticks per Mario step
const int JUMP_TICKS = 12;  // time in the air
const int MAX_BARRELS = 8;

struct Barrel {
  bool used;
  int x, y;       // the barrel's pixel
  int floor;      // floor it rolls on (while rolling)
  bool falling;   // dropping to the floor below
  bool jumped;    // Mario already scored for it
};

}  // namespace

class KongGame : public ArcadeGame {
 public:
  const char *id() const override { return "kong"; }
  const char *name() const override { return "Donkey Kong"; }
  uint16_t frameMs() const override { return 40; }

  void start() override {
    lives_ = 3;
    points_ = 0;
    round_ = 0;
    newRound();
  }

  void input(char key) override {
    if (dying_ || won_) return;
    if (key == 'L' || key == 'R') walk(key == 'L' ? -1 : 1);
    if (key == 'U') climb(1);
    if (key == 'D') climb(-1);
    if (key == 'A') jump();
  }

 protected:
  void tick(uint32_t) override {
    tick_++;
    if (dying_) {
      if (--dying_ == 0) {
        if (--lives_ <= 0) return gameOver(points_);
        placeMario();
        for (Barrel &b : barrels_) b.used = false;
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
    moveBarrels();
    throwBarrel();
    if (hit()) {
      dying_ = 40;
    } else if (floor_ == FLOORS - 1 && climb_ == 0 && x_ >= PAULINE_X - 2) {
      points_ += 500;
      won_ = 40;
    }
    draw();
  }

 private:
  void newRound() {
    for (Barrel &b : barrels_) b.used = false;
    placeMario();
    nextThrow_ = tick_ + 30;
  }

  void placeMario() {
    floor_ = 0;
    x_ = 1;
    climb_ = 0;
    jump_ = 0;
  }

  // Mario's feet row (his head is the row above).
  int feet() const { return FLOOR_ROW[floor_] - 1 - climb_ - (jump_ > 0 ? 1 : 0); }
  bool onLadderBottom() const { return floor_ < FLOORS - 1 && x_ == LADDER_X[floor_]; }
  bool onLadderTop() const { return floor_ > 0 && x_ == LADDER_X[floor_ - 1]; }

  void walk(int dir) {
    if (climb_ > 0) return;  // on a ladder
    x_ = constrain(x_ + dir, 0, COLS - 1);
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

  void throwBarrel() {
    if (tick_ < nextThrow_) return;
    for (Barrel &b : barrels_) {
      if (b.used) continue;
      b = {true, 4, FLOOR_ROW[FLOORS - 1] - 1, FLOORS - 1, false, false};
      throwing_ = 8;
      break;
    }
    const int spread = max(30, 90 - round_ * 12);
    nextThrow_ = tick_ + spread / 2 + esp_random() % spread;
  }

  int barrelStep() const { return max(2, 4 - round_ / 2); }

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
      // Now and then a barrel takes the ladder down instead of rolling on.
      if (b.floor > 0 && b.x == LADDER_X[b.floor - 1] && esp_random() % 4 == 0) {
        b.floor--;
        b.falling = true;
        b.y++;
        continue;
      }
      const int next = b.x + ROLL_DIR[b.floor];
      if (next < 0 || next >= COLS) {  // off the end of the floor
        if (b.floor == 0) {
          b.used = false;  // gone
        } else {
          b.floor--;
          b.falling = true;
          b.y++;
        }
        continue;
      }
      b.x = next;
      // Jumped over: the barrel passes right under Mario in the air.
      if (!b.jumped && jump_ > 0 && b.x == x_ && b.y == feet() + 1) {
        b.jumped = true;
        points_ += 100;
      }
    }
  }

  bool hit() const {
    const int f = feet();
    for (const Barrel &b : barrels_) {
      if (b.used && b.x == x_ && (b.y == f || b.y == f - 1)) return true;
    }
    return false;
  }

  // A barrel rolling on `floor` that will reach column x within `steps`.
  bool barrelComing(int floor, int x, int steps) const {
    for (const Barrel &b : barrels_) {
      if (!b.used || b.falling || b.floor != floor) continue;
      const int d = (x - b.x) * ROLL_DIR[floor];  // > 0: it is heading for x
      if (d >= 0 && d <= steps) return true;
    }
    return false;
  }

  // A barrel dropping down column x (a ladder or a floor's end) above y.
  bool barrelFalling(int x, int y) const {
    for (const Barrel &b : barrels_) {
      if (b.used && b.falling && b.x == x && b.y < y) return true;
    }
    return false;
  }

  void autopilot() {
    if (jump_ > 0) return;
    // A barrel coming down the ladder onto Mario: back down, or step aside.
    if (barrelFalling(x_, feet())) {
      if (climb_ > 0) {
        climb(-1);
      } else {
        // Step to the side no rolling barrel is about to reach.
        const int side = x_ > 0 && !barrelComing(floor_, x_ - 1, 2) ? -1 : 1;
        walk(side);
      }
      return;
    }
    if (climb_ > 0) {
      // Two rungs from the top the head is still below the floor above:
      // wait there until no barrel is about to pass the top of the ladder.
      if (climb_ < 2 || !barrelComing(floor_ + 1, x_, 3)) climb(1);
      return;
    }
    // A barrel about to reach Mario on his floor: jump it.
    if (barrelComing(floor_, x_, 2)) {
      jump();
      return;
    }
    const int target = floor_ == FLOORS - 1 ? PAULINE_X - 2 : LADDER_X[floor_];
    if (x_ != target) {
      walk(target > x_ ? 1 : -1);
    } else {
      climb(1);
    }
  }

  void draw() {
    display.clear();
    const bool flash = won_ > 0 && (won_ / 4) % 2;
    // "Sfumata" (softGames()): floors and ladders dimmer than the figures.
    const uint8_t floorLevel = softGames() && !flash ? 110 : 255, ladderLevel = softGames() ? 70 : 255;
    // Floors, with a gap where a ladder comes up through them.
    for (int f = 0; f < FLOORS; f++) {
      for (int x = 0; x < COLS; x++) {
        const bool gap = f > 0 && x == LADDER_X[f - 1];
        display.setLevel(x, FLOOR_ROW[f], !gap || flash ? floorLevel : 0);
      }
    }
    // Ladders: dotted, the rungs just below the floor above and just above
    // the floor below.
    for (int f = 0; f < FLOORS - 1; f++) {
      display.setLevel(LADDER_X[f], FLOOR_ROW[f + 1] + 1, ladderLevel);
      display.setLevel(LADDER_X[f], FLOOR_ROW[f] - 1, ladderLevel);
    }
    // Donkey Kong: arms up while throwing.
    static const char *const KONG[2][3] = {{".##.", "####", "#..#"}, {"#..#", "####", ".##."}};
    const char *const *k = KONG[throwing_ > 0];
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 4; c++) {
        if (k[r][c] == '#') display.setPixel(c, r, true);
      }
    }
    // Pauline.
    display.setPixel(PAULINE_X, 1, true);
    display.setPixel(PAULINE_X, 2, true);
    for (const Barrel &b : barrels_) {
      if (b.used) display.setPixel(b.x, b.y, true);
    }
    // Mario (blinking while he loses a life); spare lives as dots on the
    // top row, between Donkey Kong and Pauline.
    if (!dying_ || (dying_ / 4) % 2) {
      display.setPixel(x_, feet(), true);
      display.setPixel(x_, feet() - 1, true);
    }
    for (int i = 0; i < lives_ - 1; i++) display.setPixel(7 + i * 2, 0, true);
  }

  Barrel barrels_[MAX_BARRELS] = {};
  int floor_ = 0, x_ = 1, climb_ = 0, jump_ = 0;
  int lives_ = 3, points_ = 0, round_ = 0;
  int dying_ = 0, won_ = 0, throwing_ = 0;
  uint32_t tick_ = 0, nextThrow_ = 0;
};

static KongGame kong;
extern Animation *const kongAnimation = &kong;
