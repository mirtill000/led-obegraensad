// "Corsa": a top-down road race. Three lanes, the road scrolling down
// under your car at the bottom; slower traffic comes down the lanes and
// the road gets faster and faster. ← → change lane (the car slides
// across). The score is the distance covered. In demo mode the computer
// heads for the lane with the most free road ahead, moving one lane at a
// time and only when the lane next to it is clear. Drawn in the games'
// style (softGames(): dimmer road markings and traffic).
//
//   x0 grass  x1 edge  x2-4 lane 0  x5 dashes  x6-8 lane 1  x9 dashes
//   x10-12 lane 2  x13 edge  x14-15 grass        your car: rows 11-14
#include "animations/arcade_game.h"
#include "display.h"
#include "settings.h"

namespace {

const int LANES = 3;
const int LANE_X[LANES] = {2, 6, 10};  // left column of each lane
const int CAR_W = 3, CAR_H = 4;
const int PLAYER_Y = 11;
const int MAX_CARS = 6;
const float START_SPEED = 0.22f;  // rows per tick
const float MAX_SPEED = 0.62f;
const float TRAFFIC = 0.45f;      // traffic's own speed, as a share of yours

const char *const PLAYER[CAR_H] = {".#.", "###", ".#.", "###"};
const char *const OTHER[CAR_H] = {"###", ".#.", "###", ".#."};

struct Car {
  bool used;
  int lane;
  float y;  // top row
};

}  // namespace

class RaceGame : public ArcadeGame {
 public:
  const char *id() const override { return "race"; }
  const char *name() const override { return "Corsa"; }
  uint16_t frameMs() const override { return 40; }

  void start() override {
    for (Car &c : cars_) c.used = false;
    lane_ = 1;
    x_ = LANE_X[lane_];
    speed_ = START_SPEED;
    road_ = 0;
    crash_ = 0;
    nextCar_ = 20;
  }

  void input(char key) override {
    if (crash_) return;
    if (key == 'L' && lane_ > 0) lane_--;
    if (key == 'R' && lane_ < LANES - 1) lane_++;
  }

 protected:
  void tick(uint32_t) override {
    if (crash_) {
      if (--crash_ == 0) return gameOver((int)(road_ / 10));
      return draw();
    }
    if (demo_) autopilot();
    // Slide towards the chosen lane, a pixel per tick.
    if (x_ < LANE_X[lane_]) x_++;
    if (x_ > LANE_X[lane_]) x_--;

    road_ += speed_;
    speed_ = min(MAX_SPEED, speed_ + 0.00012f);
    const float fall = speed_ * (1 - TRAFFIC);  // traffic comes down this fast
    for (Car &c : cars_) {
      if (!c.used) continue;
      c.y += fall;
      if (c.y >= ROWS) c.used = false;
    }
    spawn();
    if (hit()) crash_ = 30;
    draw();
  }

 private:
  // Does your car, at column x, touch a car `ticks` from now (traffic keeps
  // its speed)?
  bool hitAt(int x, int ticks) const {
    const float fall = speed_ * (1 - TRAFFIC);
    for (const Car &c : cars_) {
      if (!c.used) continue;
      const int cx = LANE_X[c.lane], cy = (int)floorf(c.y + fall * ticks);
      if (cx < x + CAR_W && x < cx + CAR_W && cy < PLAYER_Y + CAR_H && PLAYER_Y < cy + CAR_H) return true;
    }
    return false;
  }

  void spawn() {
    if (--nextCar_ > 0) return;
    const int lane = esp_random() % LANES;
    // Never close all three lanes: a new car needs the other two lanes
    // free near the top, room behind the last car in its own lane and
    // enough rows from the cars in the lanes beside it to slip between.
    int blocked = 0;
    for (int l = 0; l < LANES; l++) {
      if (l == lane) continue;
      for (const Car &c : cars_) {
        if (c.used && c.lane == l && c.y < CAR_H + 5) {
          blocked++;
          break;
        }
      }
    }
    for (const Car &c : cars_) {
      if (c.used && c.lane == lane && c.y < CAR_H + 2) blocked = LANES;
      // Staggered cars in the lanes next door, too close to slip between.
      if (c.used && abs(c.lane - lane) == 1 && c.y < CAR_H + 3) blocked = LANES;
    }
    if (blocked >= LANES - 1) {
      nextCar_ = 3;
      return;
    }
    for (Car &c : cars_) {
      if (c.used) continue;
      c = {true, lane, (float)-CAR_H};
      break;
    }
    // Denser traffic as the speed goes up.
    const int base = max(8, 26 - (int)((speed_ - START_SPEED) * 40));
    nextCar_ = base / 2 + esp_random() % base;
  }

  bool hit() const {
    for (const Car &c : cars_) {
      if (!c.used) continue;
      const int cx = LANE_X[c.lane], cy = (int)floorf(c.y);
      if (cx < x_ + CAR_W && x_ < cx + CAR_W && cy < PLAYER_Y + CAR_H && PLAYER_Y < cy + CAR_H) return true;
    }
    return false;
  }

  // Looks ahead in steps of a lane change (4 ticks): which lanes can the
  // car be in at each step without touching anything on the way? Then takes
  // the first move of a route that lasts the whole horizon - staying put
  // if it can, else towards the lane with the most room.
  void autopilot() {
    if (x_ != LANE_X[lane_]) return;  // still sliding
    const int STEP = LANE_X[1] - LANE_X[0], STEPS = 12;
    // first[l]: the move (-1, 0, 1) that starts a safe route ending in l.
    int first[LANES], next[LANES];
    for (int l = 0; l < LANES; l++) first[l] = l == lane_ ? 0 : 9;  // 9: unreachable
    for (int k = 0; k < STEPS; k++) {
      for (int l = 0; l < LANES; l++) next[l] = 9;
      for (int from = 0; from < LANES; from++) {
        if (first[from] == 9) continue;
        for (int d = -1; d <= 1; d++) {
          const int to = from + d;
          if (to < 0 || to >= LANES || next[to] != 9) continue;
          bool ok = true;
          for (int t = 1; t <= STEP && ok; t++) {
            ok = !hitAt(LANE_X[from] + d * t, k * STEP + t);
          }
          if (ok) next[to] = k == 0 ? d : first[from];
        }
      }
      bool any = false;
      for (int l = 0; l < LANES; l++) {
        first[l] = next[l];
        any |= first[l] != 9;
      }
      if (!any) return;  // no way out: keep going and hope
    }
    int move = 9;
    for (int l = 0; l < LANES; l++) {
      if (first[l] == 0) move = 0;
    }
    if (move == 9) {
      for (int l = 0; l < LANES; l++) {
        if (first[l] != 9) move = first[l];
      }
    }
    if (move != 9) lane_ += move;
  }

  void draw() {
    display.clear();
    const bool soft = softGames();
    const int scroll = (int)road_;
    // Grass: sparse dots going by at road speed.
    for (int y = 0; y < ROWS; y++) {
      const int w = y - scroll;
      if (((w % 7) + 7) % 7 == 0) display.setLevel(0, y, soft ? 50 : 255);
      if (((w % 5) + 5) % 5 == 0) display.setLevel(15, y, soft ? 50 : 255);
      if ((((w + 3) % 9) + 9) % 9 == 0) display.setLevel(14, y, soft ? 35 : 255);
    }
    // Edges (red-and-white kerbs: alternate levels) and lane dashes.
    for (int y = 0; y < ROWS; y++) {
      const int w = ((y - scroll) % 4 + 4) % 4;
      const uint8_t edge = soft ? (w < 2 ? 140 : 60) : 255;
      display.setLevel(1, y, edge);
      display.setLevel(13, y, edge);
      if (w < 2) {
        display.setLevel(5, y, soft ? 70 : 255);
        display.setLevel(9, y, soft ? 70 : 255);
      }
    }
    for (const Car &c : cars_) {
      if (c.used) car(LANE_X[c.lane], (int)floorf(c.y), OTHER, soft ? 170 : 255);
    }
    // Your car, blinking after a crash.
    if (!crash_ || (crash_ / 3) % 2) car(x_, PLAYER_Y, PLAYER, 255);
  }

  static void car(int x, int y, const char *const *rows, uint8_t level) {
    for (int r = 0; r < CAR_H; r++) {
      for (int c = 0; c < CAR_W; c++) {
        // Clear the car's box first so markings don't show through it.
        display.setLevel(x + c, y + r, rows[r][c] == '#' ? level : 0);
      }
    }
  }

  Car cars_[MAX_CARS] = {};
  int lane_ = 1, x_ = 6, crash_ = 0, nextCar_ = 0;
  float speed_ = START_SPEED, road_ = 0;
};

static RaceGame race;
extern Animation *const raceAnimation = &race;
