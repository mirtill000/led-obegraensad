// "Tron": two light cycles leave a wall behind them on the 16x16 arena;
// whoever hits a wall, a trail or the panel's edge loses the round, first
// to 3 rounds wins the match. Player 1 drives with the keys (L R U D),
// player 2 with the same keys in lower case - from a second phone set to
// "Giocatore 2" or from the Cardputer. A cycle nobody drives is driven by
// the lamp; in demo mode both are. The computer steers towards the side
// with the most room (a flood fill of the free cells), keeps going straight
// when it can, and dodges a head-on crash.
#include <string.h>

#include "animations/arcade_game.h"
#include "display.h"
#include "settings.h"

namespace {

const int WIN_ROUNDS = 3;
const uint32_t READY_MS = 900;    // heads blinking before the start
const uint32_t CRASH_MS = 1200;   // the crash flashing after a round
const uint32_t STEP_MS = 150, MIN_STEP_MS = 85;

struct Cycle {
  int x, y, dx, dy, nextDx, nextDy;
  bool alive, human;
  int score;
};

}  // namespace

class TronGame : public ArcadeGame {
 public:
  const char *id() const override { return "tron"; }
  const char *name() const override { return "Tron"; }
  uint16_t frameMs() const override { return 20; }
  const GameControls *controls() const override {
    static const GameControls c = {"LRUD", {nullptr, nullptr, nullptr, nullptr, nullptr}, false,
                                   "Le frecce girano il tuo ciclo. Un secondo giocatore sceglie «Giocatore 2» "
                                   "(o usa il Cardputer); senza di lui guida la lampada.",
                                   2};
    return &c;
  }

  void start() override {
    cycles_[0].score = cycles_[1].score = 0;
    cycles_[1].human = false;
    newRound(millis());
  }

  void input(char key) override {
    const int p = (key >= 'a' && key <= 'z') ? 1 : 0;
    const char k = p ? key - 'a' + 'A' : key;
    Cycle &c = cycles_[p];
    int dx = 0, dy = 0;
    if (k == 'L') dx = -1;
    else if (k == 'R') dx = 1;
    else if (k == 'U') dy = -1;
    else if (k == 'D') dy = 1;
    else return;
    if (demo_) return;
    if (p == 1) c.human = true;  // player 2 joined
    if (dx == -c.dx && dy == -c.dy) return;  // no U-turn into your own wall
    c.nextDx = dx;
    c.nextDy = dy;
  }

 protected:
  void tick(uint32_t now) override {
    switch (phase_) {
      case READY:
        if (now - phaseStart_ >= READY_MS) {
          phase_ = RUN;
          lastStep_ = now;
        }
        break;
      case RUN:
        if (now - lastStep_ >= stepMs()) {
          lastStep_ = now;
          step();
          if (!cycles_[0].alive || !cycles_[1].alive) {
            if (!cycles_[0].alive && cycles_[1].alive) cycles_[1].score++;
            if (!cycles_[1].alive && cycles_[0].alive) cycles_[0].score++;
            phase_ = CRASH;
            phaseStart_ = now;
            sfx(sound::HIT);
          }
        }
        break;
      case CRASH:
        if (now - phaseStart_ >= CRASH_MS) {
          const int a = cycles_[0].score, b = cycles_[1].score;
          if (a >= WIN_ROUNDS || b >= WIN_ROUNDS) {
            gameOver(String("Vince ") + (a > b ? "1" : "2") + ": " + a + "-" + b);
            return;
          }
          newRound(now);
        }
        break;
    }
    draw(now);
  }

 private:
  enum Phase : uint8_t { READY, RUN, CRASH };

  bool driven(int p) const { return !demo_ && (p == 0 || cycles_[p].human); }

  uint32_t stepMs() const {
    const uint32_t faster = steps_ / 12 * 6;  // a little faster as the arena fills
    return max(MIN_STEP_MS, STEP_MS - faster);
  }

  void newRound(uint32_t now) {
    memset(owner_, 0, sizeof(owner_));
    cycles_[0].x = 2;
    cycles_[0].y = ROWS / 2;
    cycles_[0].dx = cycles_[0].nextDx = 1;
    cycles_[0].dy = cycles_[0].nextDy = 0;
    cycles_[1].x = COLS - 3;
    cycles_[1].y = ROWS / 2 - 1;
    cycles_[1].dx = cycles_[1].nextDx = -1;
    cycles_[1].dy = cycles_[1].nextDy = 0;
    for (int p = 0; p < 2; p++) {
      cycles_[p].alive = true;
      owner_[cycles_[p].y][cycles_[p].x] = p + 1;
    }
    steps_ = 0;
    phase_ = READY;
    phaseStart_ = now;
  }

  bool free(int x, int y) const { return x >= 0 && x < COLS && y >= 0 && y < ROWS && !owner_[y][x]; }

  // Free cells reachable from (x, y), with `blockedX/Y` also taken.
  int room(int x, int y, int blockedX, int blockedY) const {
    if (!free(x, y)) return 0;
    static uint8_t queue[ROWS * COLS];
    bool seen[ROWS][COLS];
    memset(seen, 0, sizeof(seen));
    if (blockedX >= 0) seen[blockedY][blockedX] = true;
    int head = 0, tail = 0, count = 0;
    queue[tail++] = y * COLS + x;
    seen[y][x] = true;
    while (head < tail) {
      const int cx = queue[head] % COLS, cy = queue[head] / COLS;
      head++;
      count++;
      static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
      for (int d = 0; d < 4; d++) {
        const int nx = cx + DX[d], ny = cy + DY[d];
        if (!free(nx, ny) || seen[ny][nx]) continue;
        seen[ny][nx] = true;
        queue[tail++] = ny * COLS + nx;
      }
    }
    return count;
  }

  // The computer's choice: of straight, left and right, the move into the
  // most room; going straight wins ties, a cell the other head could take
  // next is avoided unless there's no other way.
  void steer(int p) {
    Cycle &me = cycles_[p];
    const Cycle &other = cycles_[1 - p];
    const int dirs[3][2] = {{me.dx, me.dy}, {me.dy, -me.dx}, {-me.dy, me.dx}};
    float best = -1;
    for (int i = 0; i < 3; i++) {
      const int nx = me.x + dirs[i][0], ny = me.y + dirs[i][1];
      if (!free(nx, ny)) continue;
      float score = room(nx, ny, -1, -1);
      if (i == 0) score += 1.5f;                        // prefer straight
      const int dist = abs(nx - other.x) + abs(ny - other.y);
      if (dist == 1) score -= 40;                       // head-on risk
      score += (esp_random() % 100) / 100.0f * 2.0f;    // a little character
      if (score > best) {
        best = score;
        me.nextDx = dirs[i][0];
        me.nextDy = dirs[i][1];
      }
    }
  }

  void step() {
    steps_++;
    for (int p = 0; p < 2; p++) {
      if (!driven(p)) steer(p);
    }
    int tx[2], ty[2];
    for (int p = 0; p < 2; p++) {
      Cycle &c = cycles_[p];
      c.dx = c.nextDx;
      c.dy = c.nextDy;
      tx[p] = c.x + c.dx;
      ty[p] = c.y + c.dy;
    }
    for (int p = 0; p < 2; p++) {
      if (!free(tx[p], ty[p])) cycles_[p].alive = false;
    }
    if (tx[0] == tx[1] && ty[0] == ty[1]) cycles_[0].alive = cycles_[1].alive = false;  // head-on
    for (int p = 0; p < 2; p++) {
      Cycle &c = cycles_[p];
      if (!c.alive) continue;
      c.x = tx[p];
      c.y = ty[p];
      owner_[c.y][c.x] = p + 1;
    }
    crashX_[0] = tx[0];
    crashY_[0] = ty[0];
    crashX_[1] = tx[1];
    crashY_[1] = ty[1];
  }

  void draw(uint32_t now) {
    display.clear();
    const bool soft = softGames();
    // Player 1 bright, player 2 dimmer ("Nitida": both full, player 2's
    // head blinking).
    const uint8_t trail[2] = {soft ? (uint8_t)170 : (uint8_t)255, soft ? (uint8_t)55 : (uint8_t)255};
    const bool crashed = phase_ == CRASH, flash = (now / 120) % 2;
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const int o = owner_[y][x];
        if (!o) continue;
        uint8_t level = trail[o - 1];
        // The loser's wall fades during the crash.
        if (crashed && !cycles_[o - 1].alive) level = level * (CRASH_MS - min(CRASH_MS, now - phaseStart_)) / CRASH_MS;
        display.setLevel(x, y, level);
      }
    }
    for (int p = 0; p < 2; p++) {
      const Cycle &c = cycles_[p];
      bool on = true;
      if (phase_ == READY) on = (now / 150) % 2;
      else if (p == 1 && !soft) on = (now / 200) % 2;
      if (on) display.setLevel(c.x, c.y, p == 0 || !soft ? 255 : 150);
      if (crashed && !c.alive && flash) display.setLevel(constrain(crashX_[p], 0, COLS - 1), constrain(crashY_[p], 0, ROWS - 1), 255);
    }
  }

  Cycle cycles_[2] = {};
  uint8_t owner_[ROWS][COLS];
  Phase phase_ = READY;
  uint32_t phaseStart_ = 0, lastStep_ = 0;
  int steps_ = 0;
  int crashX_[2] = {0, 0}, crashY_[2] = {0, 0};
};

static TronGame tron;
extern Animation *const tronAnimation = &tron;
