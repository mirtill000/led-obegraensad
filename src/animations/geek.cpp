// "Icone geek": small pixel-art loops - a Space Invader, Pac-Man with a
// ghost, a terminal typing commands, an 8-bit heart, a rocket among the
// stars, a coffee cup, a charging battery.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "gfx.h"
#include "font_micro.h"
#include "settings.h"
#include "sysinfo.h"
#include "timekeeping.h"

#include <vector>

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
// A terminal in the 3x3 Micro font, four lines on screen: a command typed a
// letter at a time after the prompt, then its output, then the next
// command, the lines scrolling up. The answers are the lamp's own: its files,
// time and uptime, address, free flash and memory, chip temperature, date.
class TerminalIcon : public GeekAnimation {
 public:
  const char *id() const override { return "terminal"; }
  const char *name() const override { return "Terminale"; }
  uint16_t frameMs() const override { return 90; }
  void start() override {
    lines_.clear();
    command_ = esp_random() % COMMAND_COUNT;
    newPrompt();
  }
  void frame(uint32_t) override {
    tick_++;
    const uint32_t t = tick_ - since_;
    switch (phase_) {
      case WAIT:  // a blinking cursor on the new prompt
        if (t >= 14) {
          phase_ = TYPE;
          since_ = tick_;
        }
        break;
      case TYPE: {
        const String cmd = COMMANDS[command_];
        const unsigned typed = min<unsigned>(cmd.length(), t / 3);
        lines_.back().text = cmd.substring(0, typed);
        if (typed == cmd.length() && t >= cmd.length() * 3 + 5) {
          for (const String &l : wrap(output(cmd))) pending_.push_back(l);
          phase_ = PRINT;
          since_ = tick_;
        }
        break;
      }
      case PRINT:  // one line of output every few frames
        if (t % 3 == 0) {
          if (pending_.empty()) {
            command_ = (command_ + 1) % COMMAND_COUNT;
            newPrompt();
          } else {
            push({pending_.front(), false});
            pending_.erase(pending_.begin());
          }
        }
        break;
    }
    draw();
  }

 private:
  struct Line {
    String text;
    bool prompt;
  };
  enum Phase : uint8_t { WAIT, TYPE, PRINT };
  static const int VISIBLE = 4, PROMPT_W = 3;  // '>' and a gap
  static constexpr const char *COMMANDS[] = {"LS", "W", "IP", "DF", "TOP", "PWD", "CAL"};
  static const int COMMAND_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

  static int width(const String &s) {
    int w = 0;
    for (unsigned i = 0; i < s.length(); i++) w += findMicroGlyph(s[i])->width + 1;
    return w ? w - 1 : 0;
  }
  static void text(int x, int y, const String &s, uint8_t level) {
    for (unsigned i = 0; i < s.length(); i++) {
      const MicroGlyph *g = findMicroGlyph(s[i]);
      for (int r = 0; r < MICRO_HEIGHT; r++) {
        for (int c = 0; c < g->width; c++) {
          if (g->rows[r] & (0x80 >> c)) display.setLevel(x + c, y + r, level);
        }
      }
      x += g->width + 1;
    }
  }

  // Output lines: split at '\n', then wrapped at the panel's edge like a
  // terminal does (mid-word).
  static std::vector<String> wrap(const String &out) {
    std::vector<String> lines;
    int start = 0;
    while (start <= (int)out.length()) {
      int end = out.indexOf('\n', start);
      if (end < 0) end = out.length();
      String rest = out.substring(start, end);
      do {
        unsigned n = rest.length();
        while (n > 1 && width(rest.substring(0, n)) > COLS) n--;
        lines.push_back(rest.substring(0, n));
        rest = rest.substring(n);
      } while (rest.length());
      start = end + 1;
    }
    return lines;
  }

  static String uptime() {
    const uint32_t s = millis() / 1000, d = s / 86400, h = s / 3600 % 24, m = s / 60 % 60;
    if (d) return String(d) + "D" + h + "H";
    if (h) return String(h) + "H" + (m < 10 ? "0" : "") + m;
    return String(m) + "M";
  }

  static String output(const String &cmd) {
    struct tm t;
    const bool clock = localTime(t);
    if (cmd == "LS") return sysinfo::files(3);
    if (cmd == "W") {
      char hhmm[6];
      if (clock) strftime(hhmm, sizeof(hhmm), "%H:%M", &t);
      return (clock ? String(hhmm) + "\n" : String()) + "UP\n" + uptime();
    }
    if (cmd == "IP") return sysinfo::ip();
    if (cmd == "DF") return sysinfo::freeFlash();
    if (cmd == "TOP") return "MEM\n" + sysinfo::freeHeap() + "\n" + sysinfo::chipTemp();
    if (cmd == "PWD") return "/";
    if (cmd == "CAL") {
      if (!clock) return "?";
      static const char *const DAYS[7] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
      return String(DAYS[t.tm_wday]) + "\n" + t.tm_mday + "/" + (t.tm_mon + 1);
    }
    return "?";
  }

  void push(const Line &l) {
    lines_.push_back(l);
    if ((int)lines_.size() > VISIBLE) lines_.erase(lines_.begin());
  }
  void newPrompt() {
    push({"", true});
    phase_ = WAIT;
    since_ = tick_;
  }

  void draw() {
    display.clear();
    for (size_t i = 0; i < lines_.size(); i++) {
      const int y = i * (MICRO_HEIGHT + 1);
      const Line &l = lines_[i];
      if (l.prompt) {
        text(0, y, ">", 255);
        text(PROMPT_W, y, l.text, 255);
      } else {
        text(0, y, l.text, 150);
      }
    }
    // Cursor after the prompt line being typed (blinking while waiting).
    const Line &last = lines_.back();
    if (last.prompt && phase_ != PRINT && (phase_ == TYPE || (tick_ / 4) % 2)) {
      const int x = PROMPT_W + (last.text.length() ? width(last.text) + 1 : 0);
      const int y = (lines_.size() - 1) * (MICRO_HEIGHT + 1);
      for (int r = 0; r < MICRO_HEIGHT; r++) display.setLevel(x, y + r, 200);
    }
  }

  std::vector<Line> lines_;
  std::vector<String> pending_;
  Phase phase_ = WAIT;
  uint32_t tick_ = 0, since_ = 0;
  uint8_t command_ = 0;
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
// A battery charging bar by bar, then flashing full: lying down with the
// lamp horizontal, standing up (terminal on top) with it vertical.
class BatteryIcon : public GeekAnimation {
 public:
  const char *id() const override { return "battery"; }
  const char *name() const override { return "Batteria"; }
  uint16_t frameMs() const override { return 400; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    const bool up = settings.vertical;
    // Drawn lying down (outline x0-14, rows 4-11, terminal at x15); standing
    // up it is the same picture turned a quarter, terminal at the top.
    auto px = [up](int x, int y) {
      if (up) display.setPixel(y, COLS - 1 - x, true);
      else display.setPixel(x, y, true);
    };
    for (int x = 0; x <= 14; x++) {
      px(x, 4);
      px(x, 11);
    }
    for (int y = 4; y <= 11; y++) {
      px(0, y);
      px(14, y);
    }
    for (int y = 6; y <= 9; y++) px(15, y);
    // Charge: 0-3 bars (3 pixels wide, a pixel apart and from the outline),
    // then two flashes when full.
    const int step = tick_ % 8;
    const int bars = step < 4 ? step : ((step - 4) % 2 == 0 ? 3 : 0);
    for (int b = 0; b < bars; b++) {
      for (int x = 2 + b * 4; x < 5 + b * 4; x++) {
        for (int y = 6; y <= 9; y++) px(x, y);
      }
    }
  }

 private:
  uint32_t tick_ = 0;
};

InvaderIcon invader;
PacManIcon pacIcon;
TerminalIcon terminal;
HeartIcon heart;
RocketIcon rocket;
CoffeeIcon coffee;
BatteryIcon battery;

}  // namespace

extern Animation *const invaderIconAnimation = &invader;
extern Animation *const pacmanIconAnimation = &pacIcon;
extern Animation *const terminalIconAnimation = &terminal;
extern Animation *const heartIconAnimation = &heart;
extern Animation *const rocketIconAnimation = &rocket;
extern Animation *const coffeeIconAnimation = &coffee;
extern Animation *const batteryIconAnimation = &battery;
