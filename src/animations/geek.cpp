// "Icone geek": small pixel-art loops - a Space Invader, Pac-Man with a
// ghost, a terminal typing commands, a rocket among the stars, a coffee
// cup, a floppy disk, a Game Boy, Matrix rain, the hacker emblem, Wi-Fi,
// a skull, a light bulb, a little house, a burger, an old computer, a
// bird, a robot and a die.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "sprite_atlas.h"
#include "gfx.h"
#include "font_micro.h"
#include "settings.h"
#include "sysinfo.h"
#include "timekeeping.h"
#include "ui.h"

#include <vector>

namespace {

// Pictures come from the sprite atlas (sprite_atlas.h).

class GeekAnimation : public Animation {
 public:
  const char *group() const override { return "Icone geek"; }
  bool fixedStep() const override { return true; }  // they count their frames
};

// ---------------------------------------------------------------------------
// The classic crab invader, legs going, drifting side to side.
class InvaderIcon : public GeekAnimation {
 public:
  const char *id() const override { return "invader"; }
  const char *name() const override { return "Alieno"; }
  uint16_t frameMs() const override { return 500; }
  void frame(uint32_t) override {
    static const int DRIFT[4] = {2, 3, 2, 1};
    tick_++;
    display.clear();
    sprites::draw(spr::GEEK_INVADER, DRIFT[tick_ % 4], 4, tick_ % 2);
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
    tick_++;
    const int span = COLS + 20;
    const int x = (int)(tick_ % span) - 6;
    display.clear();
    for (int dx = 1; dx < COLS; dx += 3) {
      if (dx > x + 2) display.setPixel(dx, 7, true);  // not eaten yet
    }
    sprites::draw(spr::GEEK_PACMAN, x, 5, (tick_ / 2) % 2);
    sprites::draw(spr::GEEK_GHOST, x - 8, 5, (tick_ / 3) % 2);
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
    sprites::draw(spr::GEEK_ROCKET, 5, 2 + bob);
    sprites::draw(spr::GEEK_FLAME, 5, 10 + bob, (tick_ / 2) % 2);
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
    tick_++;
    display.clear();
    sprites::draw(spr::GEEK_CUP, 3, 8);
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
    tick_++;
    display.clear();
    for (int i = 0; i < COLS; i++) {
      for (int line = 0; line < 16; line += 5) {
        display.setLevel(line, i, 40);
        display.setLevel(i, line, 40);
      }
    }
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 3; c++) {
        if (sprites::shade(spr::GEEK_GLIDER, tick_ % 4, c, r) <= 0) continue;  // a cell of the glider, 4x4
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

// ---------------------------------------------------------------------------
// A skull that chatters its teeth now and then, its eyes glowing.
class SkullIcon : public GeekAnimation {
 public:
  const char *id() const override { return "skull"; }
  const char *name() const override { return "Teschio"; }
  uint16_t frameMs() const override { return 90; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    const uint32_t c = tick_ % 50;
    const bool chatter = c < 14;
    const int jaw = chatter ? (tick_ / 2) % 2 : 0;
    const int y = 2 + (chatter ? 0 : (tick_ / 12) % 2);
    glow_ = (uint8_t)(70 + 185 * fabsf(sinf(tick_ * 0.12f)));
    sprites::draw(spr::GEEK_SKULL, 2, y, jaw, 255, eyes, this);
  }

 private:
  static int eyes(char mark, uint8_t, void *self) { return mark == 'e' ? static_cast<SkullIcon *>(self)->glow_ : 255; }
  uint32_t tick_ = 0;
  uint8_t glow_ = 0;
};

// ---------------------------------------------------------------------------
// A light bulb switching on: the filament flickers, the glass fills with
// light, rays shine around it; after a while it goes out again.
class BulbIcon : public GeekAnimation {
 public:
  const char *id() const override { return "bulb"; }
  const char *name() const override { return "Lampadina"; }
  uint16_t frameMs() const override { return 60; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    const uint32_t c = tick_ % 130;
    float on = 0;  // 0 off .. 1 fully lit
    if (c >= 20 && c < 36) on = esp_random() % 3 ? 0.9f : 0.1f;  // flickering on
    else if (c >= 36 && c < 115) on = 1;
    else if (c >= 115) on = 1 - (c - 115) / 15.0f;                // fading out
    light_ = on;
    sprites::draw(spr::GEEK_BULB, 2, 1, 0, 255, parts, this);
    if (on > 0.5f) {
      // Rays: eight short strokes around the glass, breathing.
      static const int8_t RAYS[8][2] = {{7, -1}, {12, 1}, {14, 5}, {12, 9}, {2, 9}, {0, 5}, {2, 1}, {-1, -1}};
      for (int i = 0; i < 7; i++) {
        const float b = on * (0.35f + 0.25f * sinf(tick_ * 0.3f + i));
        gfx::plot(RAYS[i][0], RAYS[i][1] + 1, b);
      }
    }
  }

 private:
  static int parts(char mark, uint8_t, void *self) {
    const float on = static_cast<BulbIcon *>(self)->light_;
    if (mark == 'f') return on > 0 ? gfx::level(0.25f + 0.75f * on) : 40;  // the filament
    if (mark == 'g') return on > 0.05f ? gfx::level(0.45f * on) : -1;       // the glass
    return 255;
  }
  uint32_t tick_ = 0;
  float light_ = 0;
};

// ---------------------------------------------------------------------------
// A little house at dusk: smoke curls from the chimney, the windows light
// up one after the other, then go dark again.
class HouseIcon : public GeekAnimation {
 public:
  const char *id() const override { return "house"; }
  const char *name() const override { return "Casetta"; }
  uint16_t frameMs() const override { return 120; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    const uint32_t c = tick_ % 100;
    lit_ = c < 15 ? 0 : c < 30 ? 1 : c < 85 ? 2 : c < 92 ? 1 : 0;  // windows on
    sprites::draw(spr::GEEK_HOUSE, 1, 3, 0, 255, parts, this);
    // Smoke: three puffs rising from the chimney (x 11-12, top row 3).
    for (int p = 0; p < 3; p++) {
      const float age = fmodf(tick_ * 0.08f + p / 3.0f, 1.0f);
      const float x = 11.5f + sinf(age * 6 + p) * 0.8f + age * 1.5f, y = 3 - age * 4;
      if (y >= 0) gfx::plot((int)lroundf(x), (int)lroundf(y), 0.45f * (1 - age));
    }
  }

 private:
  static int parts(char mark, uint8_t, void *self) {
    const int lit = static_cast<HouseIcon *>(self)->lit_;
    if (mark == 'w') return lit ? 255 : 45;  // (both windows go on together after the first)
    if (mark == 'd') return 90;              // the door
    return 255;
  }
  uint32_t tick_ = 0;
  int lit_ = 0;
};

// ---------------------------------------------------------------------------
// A burger whose top bun hops up and lands again, sesame seeds glinting.
class BurgerIcon : public GeekAnimation {
 public:
  const char *id() const override { return "burger"; }
  const char *name() const override { return "Hamburger"; }
  uint16_t frameMs() const override { return 70; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    const uint32_t c = tick_ % 40;
    const int hop = c < 12 ? (int)lroundf(3 * sinf(c / 12.0f * (float)M_PI)) : 0;
    sprites::draw(spr::GEEK_BURGER_BOTTOM, 1, 9);
    sprites::draw(spr::GEEK_BURGER_TOP, 1, 5 - hop, 0, 255, seeds, this);
  }

 private:
  static int seeds(char, uint8_t, void *self) {
    return (static_cast<BurgerIcon *>(self)->tick_ / 3) % 4 ? 70 : 0;  // a seed glints dark-light
  }
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// An old computer showing its screensaver: a ball bouncing round the
// screen, leaving a fading trail.
class ComputerIcon : public GeekAnimation {
 public:
  const char *id() const override { return "computer"; }
  const char *name() const override { return "Computer"; }
  uint16_t frameMs() const override { return 50; }
  void start() override {
    x_ = 3;
    y_ = 4;
    vx_ = 0.45f;
    vy_ = 0.3f;
    for (auto &t : trail_) t[0] = t[1] = -1;
  }
  void frame(uint32_t) override {
    // The screen's inside: columns 2-13, rows 3-9.
    x_ += vx_;
    y_ += vy_;
    if (x_ < 2 || x_ > 13) vx_ = -vx_, x_ = constrain(x_, 2.0f, 13.0f);
    if (y_ < 3 || y_ > 9) vy_ = -vy_, y_ = constrain(y_, 3.0f, 9.0f);
    for (int i = TRAIL - 1; i > 0; i--) {
      trail_[i][0] = trail_[i - 1][0];
      trail_[i][1] = trail_[i - 1][1];
    }
    trail_[0][0] = (int)lroundf(x_);
    trail_[0][1] = (int)lroundf(y_);
    display.clear();
    sprites::draw(spr::GEEK_MONITOR, 1, 2);
    for (int i = TRAIL - 1; i >= 0; i--) {
      if (trail_[i][0] >= 0) gfx::plot(trail_[i][0], trail_[i][1], i ? 0.5f - i * 0.08f : 1.0f);
    }
  }

 private:
  static const int TRAIL = 6;
  float x_ = 3, y_ = 4, vx_ = 0.45f, vy_ = 0.3f;
  int trail_[TRAIL][2];
};

// ---------------------------------------------------------------------------
// A little bird hopping along the ground, flicking its tail, stopping to
// peck.
class BirdIcon : public GeekAnimation {
 public:
  const char *id() const override { return "bird"; }
  const char *name() const override { return "Uccellino"; }
  uint16_t frameMs() const override { return 80; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    for (int x = 0; x < COLS; x += 2) display.setLevel(x, 14, 40);  // the ground
    const uint32_t c = tick_ % 24;
    int y = 7, frame = 0;
    if (c < 8) {  // a hop forward
      y -= (int)lroundf(2.5f * sinf(c / 8.0f * (float)M_PI));
      frame = 1;
      x_ += 0.5f;
    } else if (c >= 14 && c < 20) {
      y += (c / 2) % 2;  // pecking
    }
    if (x_ > COLS) x_ = -9;
    sprites::draw(spr::GEEK_BIRD, (int)x_, y, frame, 255, eye, this);
  }

 private:
  static int eye(char mark, uint8_t, void *self) {
    if (mark != 'e') return 255;
    return static_cast<BirdIcon *>(self)->tick_ % 30 < 2 ? 255 : 0;  // blinks now and then
  }
  uint32_t tick_ = 0;
  float x_ = 2;
};

// ---------------------------------------------------------------------------
// A robot pacing back and forth, its antenna blinking and its eyes
// scanning left and right.
class RobotIcon : public GeekAnimation {
 public:
  const char *id() const override { return "robot"; }
  const char *name() const override { return "Robot"; }
  uint16_t frameMs() const override { return 140; }
  void frame(uint32_t) override {
    tick_++;
    display.clear();
    const int span = 6, p = tick_ % (2 * span);
    const int x = 1 + (p < span ? p : 2 * span - p);
    sprites::draw(spr::GEEK_ROBOT, x, 1, tick_ % 2, 220, parts, this);
  }

 private:
  static int parts(char mark, uint8_t level, void *self) {
    const uint32_t t = static_cast<RobotIcon *>(self)->tick_;
    if (mark == 'a') return t % 4 < 2 ? 255 : 30;  // the antenna light
    if (mark == 'e') return 255;
    return level;
  }
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// A die rolling: it rattles, its faces flashing by slower and slower, then
// it settles on a number for a moment.
class DiceIcon : public GeekAnimation {
 public:
  const char *id() const override { return "dice"; }
  const char *name() const override { return "Dado"; }
  uint16_t frameMs() const override { return 60; }
  void frame(uint32_t) override {
    tick_++;
    const uint32_t c = tick_ % 70;
    int dx = 0, dy = 0;
    if (c < 35) {
      // Rolling: a new face more and more rarely, a shake each time.
      const uint32_t every = 1 + c / 7;
      if (c % every == 0) {
        face_ = (face_ + 1 + esp_random() % 5) % 6;
        dx = (int)(esp_random() % 3) - 1;
        dy = (int)(esp_random() % 3) - 1;
      }
    }
    display.clear();
    const int x0 = 2 + dx, y0 = 2 + dy;
    sprites::draw(spr::GEEK_DIE, x0, y0);
    // Pips, 2x2, on a 3x3 grid inside the die.
    static const uint16_t FACES[6] = {0x010, 0x101, 0x111, 0x145, 0x155, 0x16D};  // bit 8 = top left .. bit 0 = bottom right
    for (int i = 0; i < 9; i++) {
      if (!(FACES[face_] & (0x100 >> i))) continue;
      const int px = x0 + 2 + (i % 3) * 3, py = y0 + 2 + (i / 3) * 3;
      for (int k = 0; k < 4; k++) display.setLevel(px + k % 2, py + k / 2, 255);
    }
  }

 private:
  uint32_t tick_ = 0;
  uint8_t face_ = 0;
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
SkullIcon skull;
BulbIcon bulb;
HouseIcon house;
BurgerIcon burger;
ComputerIcon computer;
BirdIcon bird;
RobotIcon robot;
DiceIcon dice;

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
extern Animation *const skullIconAnimation = &skull;
extern Animation *const bulbIconAnimation = &bulb;
extern Animation *const houseIconAnimation = &house;
extern Animation *const burgerIconAnimation = &burger;
extern Animation *const computerIconAnimation = &computer;
extern Animation *const birdIconAnimation = &bird;
extern Animation *const robotIconAnimation = &robot;
extern Animation *const diceIconAnimation = &dice;
