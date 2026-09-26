// "Icone geek": small pixel-art loops - a Space Invader, Pac-Man with a
// ghost, a terminal typing commands, a loading spinner, an 8-bit heart, a
// rocket among the stars, a coffee cup, a charging battery.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "gfx.h"
#include "pager.h"

namespace {

// Draws `rows` ('#' lit) with the top-left corner at (x, y).
void sprite(int x, int y, const char *const *rows, int count, uint8_t level = 255) {
  for (int r = 0; r < count; r++) {
    for (int c = 0; rows[r][c]; c++) {
      if (rows[r][c] == '#') display.setLevel(x + c, y + r, level);
    }
  }
}

class GeekAnimation : public Animation {
 public:
  const char *group() const override { return "Icone geek"; }
};

// ---------------------------------------------------------------------------
// The classic crab invader, legs going, drifting side to side.
class InvaderIcon : public GeekAnimation {
 public:
  const char *id() const override { return "invader"; }
  const char *name() const override { return "Space Invader"; }
  uint16_t frameMs() const override { return 500; }
  void frame(uint32_t) override {
    static const char *const CRAB[2][8] = {
        {"..#.....#..", "...#...#...", "..#######..", ".##.###.##.", "###########", "#.#######.#", "#.#.....#.#",
         "...##.##..."},
        {"..#.....#..", "#..#...#..#", "#.#######.#", "###.###.###", "###########", ".#########.", "..#.....#..",
         ".#.......#."},
    };
    static const int DRIFT[4] = {2, 3, 2, 1};
    tick_++;
    display.clear();
    sprite(DRIFT[tick_ % 4], 4, CRAB[tick_ % 2], 8);
  }

 private:
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// Pac-Man chomping along a row of dots with a ghost on his tail.
class PacManIcon : public GeekAnimation {
 public:
  const char *id() const override { return "pacicon"; }
  const char *name() const override { return "Pac-Man"; }
  uint16_t frameMs() const override { return 110; }
  void frame(uint32_t) override {
    static const char *const PAC[2][5] = {{".###.", "####.", "###..", "####.", ".###."},
                                          {".###.", "#####", "#####", "#####", ".###."}};
    static const char *const GHOST[2][5] = {{".###.", "#####", "#.#.#", "#####", "#.#.#"},
                                            {".###.", "#####", "#.#.#", "#####", ".#.#."}};
    tick_++;
    const int span = COLS + 20;
    const int x = (int)(tick_ % span) - 6;
    display.clear();
    for (int dx = 1; dx < COLS; dx += 3) {
      if (dx > x + 2) display.setPixel(dx, 7, true);  // not eaten yet
    }
    sprite(x, 5, PAC[(tick_ / 2) % 2], 5);
    sprite(x - 8, 5, GHOST[(tick_ / 3) % 2], 5);
  }

 private:
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// A terminal: a prompt, a command typed a letter at a time, a blinking
// cursor, then "OK" and the next one.
class TerminalIcon : public GeekAnimation {
 public:
  const char *id() const override { return "terminal"; }
  const char *name() const override { return "Terminale"; }
  uint16_t frameMs() const override { return 100; }
  void frame(uint32_t) override {
    static const char *const COMMANDS[] = {"LS", "GIT", "SSH", "VIM", "CD", "TOP", "PING"};
    static const char *const PROMPT[4] = {"#..", ".#.", "#..", "..."};
    tick_++;
    const String cmd = COMMANDS[command_ % (sizeof(COMMANDS) / sizeof(COMMANDS[0]))];
    const uint32_t t = tick_ - since_;
    const unsigned typed = min<uint32_t>(cmd.length(), t > 6 ? (t - 6) / 3 : 0);
    const bool done = typed == cmd.length() && t > 6 + cmd.length() * 3 + 8;
    display.clear();
    for (int x = 0; x < COLS; x++) display.setPixel(x, 0, true);  // title bar
    display.setPixel(1, 1, true);
    display.setPixel(3, 1, true);
    display.setPixel(5, 1, true);
    sprite(0, 5, PROMPT, 4);
    const String shown = cmd.substring(0, typed);
    Pager::drawText(4, 5, shown);
    const int cursorX = 4 + (typed ? Pager::textWidth(shown) + 1 : 0);
    if (done) Pager::drawText(0, 11, "OK");
    if ((tick_ / 4) % 2 && cursorX < COLS) {
      for (int y = 5; y < 9; y++) display.setPixel(cursorX, y, true);
    }
    if (done && t > 6 + cmd.length() * 3 + 26) {
      command_++;
      since_ = tick_;
    }
  }

 private:
  uint32_t tick_ = 0, since_ = 0;
  uint8_t command_ = 0;
};

// ---------------------------------------------------------------------------
// The loading spinner: twelve dots in a ring, a bright head with a fading
// tail going round.
class SpinnerIcon : public GeekAnimation {
 public:
  const char *id() const override { return "spinner"; }
  const char *name() const override { return "Caricamento"; }
  uint16_t frameMs() const override { return 80; }
  void frame(uint32_t) override {
    static const uint8_t TAIL[5] = {255, 170, 110, 60, 30};
    tick_++;
    display.clear();
    for (int i = 0; i < 12; i++) {
      const float a = i * (float)M_PI / 6;
      const int x = (int)lroundf(7.5f + 5.5f * sinf(a) - 0.5f), y = (int)lroundf(7.5f - 5.5f * cosf(a) - 0.5f);
      const int behind = ((int)(tick_ % 12) - i + 12) % 12;
      display.setLevel(x, y, behind < 5 ? TAIL[behind] : 12);
      display.setLevel(x + 1, y, behind < 5 ? TAIL[behind] : 12);
      display.setLevel(x, y + 1, behind < 5 ? TAIL[behind] : 12);
      display.setLevel(x + 1, y + 1, behind < 5 ? TAIL[behind] : 12);
    }
  }

 private:
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// An 8-bit heart beating: lub-dub, pause.
class HeartIcon : public GeekAnimation {
 public:
  const char *id() const override { return "heart"; }
  const char *name() const override { return "Cuore 8-bit"; }
  uint16_t frameMs() const override { return 150; }
  void frame(uint32_t) override {
    static const char *const BIG[10] = {"..###.###..", ".#########.", "###########", "###########", "###########",
                                        ".#########.", "..#######..", "...#####...", "....###....", ".....#....."};
    static const char *const SMALL[6] = {".##.##.", "#######", "#######", ".#####.", "..###..", "...#..."};
    static const bool BEAT[8] = {true, false, true, false, false, false, false, false};
    tick_++;
    display.clear();
    if (BEAT[tick_ % 8]) sprite(2, 3, BIG, 10);
    else sprite(4, 5, SMALL, 6);
  }

 private:
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// A rocket flying up through a field of stars, flame flickering.
class RocketIcon : public GeekAnimation {
 public:
  const char *id() const override { return "rocket"; }
  const char *name() const override { return "Razzo"; }
  uint16_t frameMs() const override { return 70; }
  void start() override {
    for (Star &s : stars_) {
      s.x = esp_random() % COLS;
      s.y = esp_random() % ROWS;
      s.speed = 0.2f + (esp_random() % 100) / 100.0f * 0.6f;
    }
  }
  void frame(uint32_t) override {
    static const char *const ROCKET[8] = {"..#..", ".###.", ".###.", ".#.#.", ".###.", ".###.", "#####", "#.#.#"};
    static const char *const FLAME[2][2] = {{".#.#.", "..#.."}, {"..#..", ".#.#."}};
    tick_++;
    display.clear();
    for (Star &s : stars_) {
      s.y += s.speed;
      if (s.y >= ROWS) {
        s.y -= ROWS;
        s.x = esp_random() % COLS;
      }
      display.setLevel((int)s.x, (int)s.y, s.speed > 0.5f ? 200 : 70);  // nearer stars brighter
    }
    const int bob = (tick_ / 6) % 2;
    // Clear the rocket's outline so stars don't show through it.
    for (int r = 0; r < 10; r++) {
      for (int c = 0; c < 5; c++) display.setLevel(5 + c, 2 + bob + r, 0);
    }
    sprite(5, 2 + bob, ROCKET, 8);
    sprite(5, 10 + bob, FLAME[(tick_ / 2) % 2], 2);
  }

 private:
  struct Star {
    float x, y, speed;
  };
  Star stars_[10];
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// A cup of coffee, steam curling up from it.
class CoffeeIcon : public GeekAnimation {
 public:
  const char *id() const override { return "coffee"; }
  const char *name() const override { return "Caffè"; }
  uint16_t frameMs() const override { return 90; }
  void frame(uint32_t) override {
    static const char *const CUP[7] = {"########.", "#######.#", "#######.#", "########.", ".######..", "..####...",
                                       "#########"};
    tick_++;
    display.clear();
    sprite(3, 8, CUP, 7);
    // Three wisps: each a column swaying with a sine, fading as it rises.
    for (int w = 0; w < 3; w++) {
      for (int y = 1; y < 7; y++) {
        const float phase = tick_ * 0.25f - y * 0.9f + w * 2.1f;
        const int x = 4 + w * 2 + (int)lroundf(sinf(phase) * 0.8f);
        gfx::plot(x, y, 0.25f + 0.12f * y);
      }
    }
  }

 private:
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// A battery charging bar by bar, then flashing full.
class BatteryIcon : public GeekAnimation {
 public:
  const char *id() const override { return "battery"; }
  const char *name() const override { return "Batteria"; }
  uint16_t frameMs() const override { return 400; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    // Outline: x0-14, rows 4-11, with the tip at x15.
    for (int x = 0; x <= 14; x++) {
      display.setPixel(x, 4, true);
      display.setPixel(x, 11, true);
    }
    for (int y = 4; y <= 11; y++) {
      display.setPixel(0, y, true);
      display.setPixel(14, y, true);
    }
    for (int y = 6; y <= 9; y++) display.setPixel(15, y, true);
    // Charge: 0-3 bars (3 pixels wide, a pixel apart and from the outline),
    // then two flashes when full.
    const int step = tick_ % 8;
    const int bars = step < 4 ? step : ((step - 4) % 2 == 0 ? 3 : 0);
    for (int b = 0; b < bars; b++) {
      for (int x = 2 + b * 4; x < 5 + b * 4; x++) {
        for (int y = 6; y <= 9; y++) display.setPixel(x, y, true);
      }
    }
  }

 private:
  uint32_t tick_ = 0;
};

InvaderIcon invader;
PacManIcon pacIcon;
TerminalIcon terminal;
SpinnerIcon spinner;
HeartIcon heart;
RocketIcon rocket;
CoffeeIcon coffee;
BatteryIcon battery;

}  // namespace

extern Animation *const invaderIconAnimation = &invader;
extern Animation *const pacmanIconAnimation = &pacIcon;
extern Animation *const terminalIconAnimation = &terminal;
extern Animation *const spinnerIconAnimation = &spinner;
extern Animation *const heartIconAnimation = &heart;
extern Animation *const rocketIconAnimation = &rocket;
extern Animation *const coffeeIconAnimation = &coffee;
extern Animation *const batteryIconAnimation = &battery;
