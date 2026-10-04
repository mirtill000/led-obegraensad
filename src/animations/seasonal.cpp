// "Ricorrenze": animations for special days (see occasions.h) - snow,
// a Christmas tree, fireworks, hearts, an Easter egg, a Halloween pumpkin
// and a birthday cake with the greeting from the calendar.
#include <math.h>
#include <string.h>

#include "animation.h"
#include "display.h"
#include "sprite_atlas.h"
#include "gfx.h"
#include "occasions.h"
#include "scroller.h"
#include "ui.h"
#include "webinfo.h"

namespace {

float rnd() { return (esp_random() % 10000) / 10000.0f; }

class Seasonal : public Animation {
 public:
  const char *group() const override { return "Ricorrenze"; }
  bool fixedStep() const override { return true; }  // particles and flickers
};


// ---------------------------------------------------------------------------
// Snow falling past a small pine, settling into drifts that slowly melt.
class SnowAnimation : public Seasonal {
 public:
  const char *id() const override { return "snow"; }
  const char *name() const override { return "Neve"; }
  uint16_t frameMs() const override { return 60; }
  void start() override {
    memset(pile_, 0, sizeof(pile_));
    for (Flake &f : flakes_) reset(f, true);
  }
  void frame(uint32_t now) override {
    display.clear();
    sprites::draw(spr::SEASON_PINE, 10, ROWS - 1 - 6, 0, 45);
    for (Flake &f : flakes_) {
      f.y += f.speed;
      f.x += sinf(now / 700.0f + f.phase) * 0.06f;
      const int x = ((int)lroundf(f.x) % COLS + COLS) % COLS;
      if (f.y >= ROWS - 1 - pile_[x]) {
        if (pile_[x] < 4) pile_[x]++;
        reset(f, false);
        continue;
      }
      display.setLevel(x, (int)f.y, f.speed > 0.2f ? 230 : 120);  // nearer flakes brighter
    }
    if (now - lastMelt_ > 2500) {  // drifts sink slowly, so it never fills up
      lastMelt_ = now;
      const int x = esp_random() % COLS;
      pile_[x] = max(0, pile_[x] - 1);
    }
    for (int x = 0; x < COLS; x++) {
      for (int h = 0; h < pile_[x]; h++) display.setLevel(x, ROWS - 1 - h, 160);
    }
  }

 private:
  struct Flake {
    float x, y, speed, phase;
  };
  static void reset(Flake &f, bool anywhere) {
    f.x = rnd() * COLS;
    f.y = anywhere ? rnd() * ROWS : -rnd() * 4;
    f.speed = 0.08f + rnd() * 0.2f;
    f.phase = rnd() * 6.28f;
  }
  Flake flakes_[22];
  int pile_[COLS];
  uint32_t lastMelt_ = 0;
};

// ---------------------------------------------------------------------------
// A Christmas tree: dim branches, lights twinkling one by one, a star
// pulsing on top.
class XmasTreeAnimation : public Seasonal {
 public:
  const char *id() const override { return "xmastree"; }
  const char *name() const override { return "Albero di Natale"; }
  uint16_t frameMs() const override { return 80; }
  void frame(uint32_t now) override {
    display.clear();
    // Three tiers, wider and wider, and the trunk.
    static const int8_t HALF[ROWS] = {-1, -1, 0, 1, 2, 1, 2, 3, 4, 2, 3, 4, 5, 6, -1, -1};
    for (int y = 0; y < ROWS; y++) {
      if (HALF[y] < 0) continue;
      for (int x = 7 - HALF[y]; x <= 8 + HALF[y]; x++) display.setLevel(x, y, 50);
    }
    for (int y = 14; y < ROWS; y++) {
      display.setLevel(7, y, 70);
      display.setLevel(8, y, 70);
    }
    // The star.
    const uint8_t star = 150 + (uint8_t)(105 * (0.5f + 0.5f * sinf(now / 300.0f)));
    display.setLevel(7, 0, star);
    display.setLevel(8, 0, star);
    display.setLevel(7, 1, star / 2);
    display.setLevel(8, 1, star / 2);
    // Lights: each blinks at its own pace.
    static const uint8_t LIGHTS[][2] = {{8, 3}, {6, 4}, {9, 5}, {5, 7}, {10, 7}, {7, 8}, {4, 10},
                                        {9, 10}, {11, 11}, {6, 12}, {3, 13}, {9, 13}, {12, 13}};
    for (unsigned i = 0; i < sizeof(LIGHTS) / sizeof(LIGHTS[0]); i++) {
      const float s = sinf(now / (260.0f + i * 37) + i * 1.9f);
      display.setLevel(LIGHTS[i][0], LIGHTS[i][1], s > 0.2f ? 255 : 110);
    }
  }
};

// ---------------------------------------------------------------------------
// Fireworks: rockets climb with a spark trail and burst into a ring of
// sparks that fall and fade.
class FireworksAnimation : public Seasonal {
 public:
  const char *id() const override { return "fireworks"; }
  const char *name() const override { return "Fuochi d'artificio"; }
  uint16_t frameMs() const override { return 40; }
  void start() override {
    for (Spark &s : sparks_) s.life = 0;
    for (Rocket &r : rockets_) r.alive = false;
  }
  void frame(uint32_t) override {
    display.clear();
    if (esp_random() % 18 == 0) launch();
    for (Rocket &r : rockets_) {
      if (!r.alive) continue;
      r.y += r.vy;
      r.vy += 0.012f;
      display.setLevel((int)r.x, (int)r.y, 255);
      display.setLevel((int)r.x, (int)r.y + 1, 90);
      if (r.vy >= -0.05f || r.y < 3) burst(r);
    }
    for (Spark &s : sparks_) {
      if (s.life <= 0) continue;
      s.x += s.vx;
      s.y += s.vy;
      s.vy += 0.015f;  // gravity
      s.vx *= 0.97f;
      s.life -= 0.022f;
      if (s.x < 0 || s.x >= COLS || s.y >= ROWS) {
        s.life = 0;
        continue;
      }
      if (s.y >= 0) gfx::plot((int)s.x, (int)s.y, s.life);
    }
  }

 private:
  struct Rocket {
    float x, y, vy;
    bool alive;
  };
  struct Spark {
    float x, y, vx, vy, life;
  };
  void launch() {
    for (Rocket &r : rockets_) {
      if (r.alive) continue;
      r = {2.0f + rnd() * 12, (float)ROWS, -0.42f - rnd() * 0.18f, true};
      return;
    }
  }
  void burst(Rocket &r) {
    r.alive = false;
    const int n = 14 + esp_random() % 8;
    const float speed = 0.22f + rnd() * 0.12f;
    for (int i = 0, made = 0; i < (int)(sizeof(sparks_) / sizeof(sparks_[0])) && made < n; i++) {
      if (sparks_[i].life > 0) continue;
      const float a = made * 6.2832f / n;
      sparks_[i] = {r.x, r.y, cosf(a) * speed, sinf(a) * speed, 1.0f};
      made++;
    }
  }
  Rocket rockets_[3] = {};
  Spark sparks_[60] = {};
};

// ---------------------------------------------------------------------------
// Hearts floating up, swaying, a little dimmer the farther they are.
class HeartsAnimation : public Seasonal {
 public:
  const char *id() const override { return "hearts"; }
  const char *name() const override { return "Cuori"; }
  uint16_t frameMs() const override { return 60; }
  void start() override {
    for (Heart &h : hearts_) reset(h, true);
  }
  void frame(uint32_t now) override {
    display.clear();
    for (Heart &h : hearts_) {
      h.y -= h.speed;
      if (h.y < -5) reset(h, false);
      const int x = (int)lroundf(h.x + sinf(now / 600.0f + h.phase) * 1.2f);
      sprites::draw(spr::SEASON_HEART, x, (int)h.y, 0, h.level);
    }
  }

 private:
  struct Heart {
    float x, y, speed, phase;
    uint8_t level;
  };
  static void reset(Heart &h, bool anywhere) {
    h.x = rnd() * 12;
    h.y = anywhere ? rnd() * ROWS : ROWS + rnd() * 6;
    h.speed = 0.05f + rnd() * 0.08f;
    h.phase = rnd() * 6.28f;
    h.level = h.speed > 0.09f ? 255 : 110;
  }
  Heart hearts_[4];
};

// ---------------------------------------------------------------------------
// An Easter egg with a zigzag band and dots, rocking from side to side.
class EasterEggAnimation : public Seasonal {
 public:
  const char *id() const override { return "easter"; }
  const char *name() const override { return "Uovo di Pasqua"; }
  uint16_t frameMs() const override { return 120; }
  void frame(uint32_t now) override {
    display.clear();
    const int rock = (int)lroundf(sinf(now / 450.0f) * 1.2f);
    sprites::draw(spr::SEASON_EGG, 2 + rock, 2, 0, 70);
  }
};

// ---------------------------------------------------------------------------
// A Halloween pumpkin, its carved face lit by a flickering candle.
class PumpkinAnimation : public Seasonal {
 public:
  const char *id() const override { return "pumpkin"; }
  const char *name() const override { return "Zucca di Halloween"; }
  uint16_t frameMs() const override { return 70; }
  void frame(uint32_t) override {
    display.clear();
    sprites::draw(spr::SEASON_PUMPKIN, 1, 3, 0, 60);
    flicker_ = constrain(flicker_ + (int)(esp_random() % 61) - 30, 150, 255);
    sprites::draw(spr::SEASON_PUMPKIN_FACE, 1, 3, 0, (uint8_t)flicker_);
  }

 private:
  int flicker_ = 220;
};

// ---------------------------------------------------------------------------
// A birthday cake with flickering candles; every few seconds the greeting
// from the calendar ("Buon compleanno, Anna!") scrolls by.
class CakeAnimation : public Seasonal {
 public:
  const char *id() const override { return "cake"; }
  const char *name() const override { return "Torta di compleanno"; }
  uint16_t frameMs() const override { return 70; }
  void start() override {
    since_ = millis();
    scrolling_ = false;
  }
  void frame(uint32_t now) override {
    if (scrolling_) {
      if (scroller_.update(now, 0)) {
        scrolling_ = false;
        since_ = now;
      }
      return;
    }
    if (now - since_ > 5000) {
      const String greeting = webInfoNow().birthday;
      scroller_.start(greeting.length() ? greeting : String("Buon compleanno!"));
      scroller_.setRow(-1);
      scrolling_ = true;
      return;
    }
    display.clear();
    sprites::draw(spr::SEASON_CAKE, 2, 10, 0, 120);
    for (int c = 0; c < 3; c++) {
      const int x = 4 + c * 4;
      for (int y = 6; y < 10; y++) display.setLevel(x, y, 200);
      const bool tall = (now / (90 + c * 23)) % 3;
      display.setLevel(x, 5, 255);
      if (tall) display.setLevel(x, 4, 150);
    }
  }

 private:
  Scroller scroller_;
  uint32_t since_ = 0;
  bool scrolling_ = false;
};

SnowAnimation snow;
XmasTreeAnimation xmasTree;
FireworksAnimation fireworks;
HeartsAnimation hearts;
EasterEggAnimation easterEgg;
PumpkinAnimation pumpkin;
CakeAnimation cake;

}  // namespace

extern Animation *const snowAnimation = &snow;
extern Animation *const xmasTreeAnimation = &xmasTree;
extern Animation *const fireworksAnimation = &fireworks;
extern Animation *const heartsAnimation = &hearts;
extern Animation *const easterEggAnimation = &easterEgg;
extern Animation *const pumpkinAnimation = &pumpkin;
extern Animation *const cakeAnimation = &cake;
