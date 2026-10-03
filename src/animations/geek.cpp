// "Icone geek": small pixel-art loops - a Space Invader, Pac-Man with a
// ghost, a terminal typing commands, a rocket among the stars, a coffee
// cup, a floppy disk, a Game Boy, Matrix rain, the hacker emblem, Wi-Fi.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "gfx.h"
#include "font_micro.h"
#include "settings.h"
#include "sysinfo.h"
#include "timekeeping.h"
#include "ui.h"

#include <vector>

namespace {

// Pictures are drawn with the shared ui::icon().
using ui::icon;

class GeekAnimation : public Animation {
 public:
  const char *group() const override { return "Icone geek"; }
};

// ---------------------------------------------------------------------------
// The classic crab invader, legs going, drifting side to side.
class InvaderIcon : public GeekAnimation {
 public:
  const char *id() const override { return "invader"; }
  const char *name() const override { return "Alieno"; }
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
    icon(DRIFT[tick_ % 4], 4, CRAB[tick_ % 2], 8);
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
    icon(x, 5, PAC[(tick_ / 2) % 2], 5);
    icon(x - 8, 5, GHOST[(tick_ / 3) % 2], 5);
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
    icon(5, 2 + bob, ROCKET, 8);
    icon(5, 10 + bob, FLAME[(tick_ / 2) % 2], 2);
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
    icon(3, 8, CUP, 7);
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
// A 3.5" floppy disk: the metal shutter slides open over the disk and
// back, while lines get "written" on the label.
class FloppyIcon : public GeekAnimation {
 public:
  const char *id() const override { return "floppy"; }
  const char *name() const override { return "Floppy"; }
  uint16_t frameMs() const override { return 120; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    // The case, x1-14 and y1-14, with the write-protect corner cut.
    for (int i = 1; i <= 14; i++) {
      display.setLevel(i, 1, i < 14 ? 200 : 0);
      display.setLevel(i, 14, 200);
      display.setLevel(1, i, 200);
      display.setLevel(14, i, i > 1 ? 200 : 0);
    }
    display.setLevel(13, 2, 200);
    // The shutter (x4-10, y1-5), its window sliding right and back.
    const int phase = tick_ % 40;
    const int slide = phase < 10 ? 0 : phase < 16 ? phase - 10 : phase < 30 ? 6 : max(0, 36 - phase);
    for (int y = 1; y <= 5; y++) {
      for (int x = 4; x <= 10; x++) display.setLevel(x, y, 150);
    }
    for (int y = 2; y <= 4; y++) {
      for (int x = 0; x < 2; x++) display.setLevel(5 + x + min(slide, 4), y, slide >= 4 ? 40 : 0);  // the disk shows through
    }
    // The label (x3-12, y8-13) and the lines written on it, one by one.
    for (int y = 8; y <= 13; y++) {
      for (int x = 3; x <= 12; x++) display.setLevel(x, y, 45);
    }
    const int written = (tick_ / 6) % 5;
    for (int l = 0; l < min(written, 3); l++) {
      const int len = (l == 2) ? 5 : 8;
      for (int x = 4; x < 4 + len; x++) display.setLevel(x, 9 + l * 2, 220);
    }
  }

 private:
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// A Game Boy: the grey brick with a little Tetris game on its screen, the
// d-pad and the A/B buttons.
class GameBoyIcon : public GeekAnimation {
 public:
  const char *id() const override { return "gameboy"; }
  const char *name() const override { return "Game Boy"; }
  uint16_t frameMs() const override { return 180; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    // Body x3-12, y0-15 (rounded bottom-right corner).
    for (int y = 0; y < ROWS; y++) {
      for (int x = 3; x <= 12; x++) {
        const bool edge = x == 3 || x == 12 || y == 0 || y == 15;
        if (edge && !(x == 12 && y == 15)) display.setLevel(x, y, 150);
      }
    }
    display.setLevel(11, 14, 150);
    // Screen x5-10, y2-7: a piece falling onto a stack.
    for (int y = 2; y <= 7; y++) {
      for (int x = 5; x <= 10; x++) display.setLevel(x, y, 30);
    }
    for (int x = 5; x <= 10; x++) display.setLevel(x, 7, x == 8 ? 30 : 230);  // the stack, a gap to fill
    const int fall = tick_ % 7;  // rows 2-6, then it lands and a new one comes
    const int py = 2 + min(fall, 4);
    display.setLevel(8, py - 1, 255);
    display.setLevel(8, py, 255);
    display.setLevel(7, py - 1, 255);
    // D-pad and the A/B buttons.
    display.setLevel(5, 10, 230);
    display.setLevel(4, 11, 230);
    display.setLevel(5, 11, 230);
    display.setLevel(6, 11, 230);
    display.setLevel(5, 12, 230);
    const bool press = (tick_ / 3) % 4 == 0;
    display.setLevel(10, 10, press ? 90 : 230);
    display.setLevel(9, 11, 230);
    // Speaker grille.
    display.setLevel(10, 13, 70);
    display.setLevel(9, 14, 70);
  }

 private:
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// Matrix digital rain: drops of glyphs falling down every column at their
// own pace, a bright head and a fading trail that flickers like changing
// characters.
class MatrixIcon : public GeekAnimation {
 public:
  const char *id() const override { return "matrix"; }
  const char *name() const override { return "Matrix"; }
  uint16_t frameMs() const override { return 60; }
  void start() override {
    for (Drop &d : drops_) reset(d, true);
  }
  void frame(uint32_t) override {
    display.clear();
    for (int x = 0; x < COLS; x++) {
      Drop &d = drops_[x];
      d.y += d.speed;
      if (d.y - d.length > ROWS) reset(d, false);
      const int head = (int)d.y;
      for (int k = 0; k <= d.length; k++) {
        const int y = head - k;
        if (y < 0 || y >= ROWS) continue;
        if (k > 0 && (esp_random() % 9) == 0) continue;  // a glyph changing
        const uint8_t level = k == 0 ? 255 : (uint8_t)(170 * (d.length - k + 1) / (d.length + 1));
        display.setLevel(x, y, level);
      }
    }
  }

 private:
  struct Drop {
    float y, speed;
    int length;
  };
  static void reset(Drop &d, bool anywhere) {
    d.speed = 0.25f + (esp_random() % 100) / 100.0f * 0.55f;
    d.length = 4 + esp_random() % 7;
    d.y = anywhere ? (float)(esp_random() % (ROWS + 10)) - 10 : -(float)(esp_random() % 12);
  }
  Drop drops_[COLS];
};

// ---------------------------------------------------------------------------
// The hacker emblem: a glider from the Game of Life in a 3x3 grid, going
// through its four generations.
class GliderIcon : public GeekAnimation {
 public:
  const char *id() const override { return "glider"; }
  const char *name() const override { return "Emblema hacker"; }
  uint16_t frameMs() const override { return 600; }
  void frame(uint32_t) override {
    static const char *const PHASES[4][3] = {
        {".#.", "..#", "###"}, {"#.#", ".##", ".#."}, {"..#", "#.#", ".##"}, {"#..", ".##", "##."}};
    tick_++;
    display.clear();
    for (int i = 0; i < COLS; i++) {
      for (int line = 0; line < 16; line += 5) {
        display.setLevel(line, i, 40);
        display.setLevel(i, line, 40);
      }
    }
    const char *const *cells = PHASES[tick_ % 4];
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 3; c++) {
        if (cells[r][c] != '#') continue;
        for (int y = 0; y < 4; y++) {
          for (int x = 0; x < 4; x++) display.setLevel(1 + c * 5 + x, 1 + r * 5 + y, 230);
        }
      }
    }
  }

 private:
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// The Wi-Fi sign "connecting": the dot, then the three arcs lighting one by
// one, then all of them flashing.
class WifiIcon : public GeekAnimation {
 public:
  const char *id() const override { return "wifi"; }
  const char *name() const override { return "Wi-Fi"; }
  uint16_t frameMs() const override { return 250; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    const int step = tick_ % 9;  // 0 dot, 1-3 arcs, 4-8 full (blinking once)
    const int arcs = step < 4 ? step : (step == 6 ? 0 : 3);
    const float cx = 7.5f, cy = 13.0f;
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const float dx = x - cx, dy = cy - y;
        const float r = sqrtf(dx * dx + dy * dy);
        if (r < 1.3f) {
          display.setLevel(x, y, 255);  // the dot
          continue;
        }
        if (dy <= 0 || fabsf(dx) > dy * 1.05f) continue;  // a 90-degree wedge, pointing up
        for (int a = 1; a <= arcs; a++) {
          const float d = fabsf(r - (0.4f + a * 3.3f));
          if (d < 0.8f) display.setLevel(x, y, gfx::level(1.0f - d * 0.9f));
        }
      }
    }
  }

 private:
  uint32_t tick_ = 0;
};

InvaderIcon invader;
PacManIcon pacIcon;
TerminalIcon terminal;
RocketIcon rocket;
CoffeeIcon coffee;
FloppyIcon floppy;
GameBoyIcon gameBoy;
MatrixIcon matrix;
GliderIcon glider;
WifiIcon wifi;

}  // namespace

extern Animation *const invaderIconAnimation = &invader;
extern Animation *const pacmanIconAnimation = &pacIcon;
extern Animation *const terminalIconAnimation = &terminal;
extern Animation *const rocketIconAnimation = &rocket;
extern Animation *const coffeeIconAnimation = &coffee;
extern Animation *const floppyIconAnimation = &floppy;
extern Animation *const gameBoyIconAnimation = &gameBoy;
extern Animation *const matrixIconAnimation = &matrix;
extern Animation *const gliderIconAnimation = &glider;
extern Animation *const wifiIconAnimation = &wifi;
