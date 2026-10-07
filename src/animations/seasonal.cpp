// "Ricorrenze": animations for special days (see occasions.h) - snow,
// a Christmas tree, fireworks, hearts, a Halloween pumpkin
// and a birthday cake with the greeting from the calendar.
#include <math.h>
#include <string.h>

#include "animation.h"
#include "display.h"
#include "sprite_atlas.h"
#include "gfx.h"
#include "occasions.h"
#include "scroller.h"
#include "timekeeping.h"
#include "ui.h"
#include "webinfo.h"

namespace {

float rnd() { return (esp_random() % 10000) / 10000.0f; }

class Seasonal : public Animation {
 public:
  const char *group() const override { return "Ricorrenze"; }
  bool fixedStep() const override { return true; }  // particles and flickers
  // Only around its days (occasions.h); without the clock, not at all.
  bool available() const override {
    struct tm t;
    return localTime(t) && inSeason(id(), t, webInfoNow().birthday.length() > 0);
  }
};


// ---------------------------------------------------------------------------
// A snowy night: a crescent moon and a few stars, a far white ridge, two
// pines and a cottage with its window lit and smoke from the chimney.
// The snow falls in three depths - far flakes small, dim and slow, near
// ones bright and quick - blown by gusts of wind, and settles on the
// ground in drifts that grow and slowly sink back.
class SnowAnimation : public Seasonal {
 public:
  const char *id() const override { return "snow"; }
  const char *name() const override { return "Neve"; }
  uint16_t frameMs() const override { return 50; }
  void start() override {
    for (float &d : drift_) d = 0.3f + rnd() * 0.4f;
    for (int i = 0; i < FLAKES; i++) reset(flakes_[i], i % 3, true);
    t_ = 0;
  }
  void frame(uint32_t) override {
    const float dt = frameMs() / 1000.0f;
    t_ += dt;
    // The wind: slow gusts, now one way now the other.
    const float wind = 0.9f * sinf(t_ * 0.13f) + 0.5f * sinf(t_ * 0.41f + 1);

    float px[ROWS][COLS] = {};
    // Sky: stars twinkling, the moon, a pale ridge far away.
    static const uint8_t STARS[][2] = {{6, 1}, {11, 0}, {14, 3}, {9, 4}, {4, 5}};
    for (int i = 0; i < 5; i++) px[STARS[i][1]][STARS[i][0]] = 0.1f + 0.08f * sinf(t_ * (1.3f + i * 0.4f) + i);
    static const uint8_t MOON[][2] = {{2, 1}, {3, 1}, {1, 2}, {1, 3}, {2, 4}, {3, 4}};
    for (const auto &m : MOON) px[m[1]][m[0]] = 0.45f;
    for (int x = 0; x < COLS; x++) {
      const float ridge = 10.2f + 0.8f * sinf(x * 0.5f + 0.7f) + 0.4f * sinf(x * 1.3f);
      for (int y = (int)ridge; y < ROWS; y++) px[y][x] = y == (int)ridge ? 0.14f : 0.07f;
    }
    // Far snow behind everything.
    for (Flake &f : flakes_) {
      if (f.layer == 0) fall(f, wind, dt, px);
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) display.setLevel(x, y, ui::tone(px[y][x]));
    }

    // The pines and the cottage, its window glowing like a fire inside.
    window_ = (uint8_t)(150 + 50 * sinf(t_ * 7) * sinf(t_ * 2.3f) + (esp_random() % 20));
    // ('#' is drawn at the level given: dark needles and walls.)
    sprites::draw(spr::SEASON_PINE, 1, 6, 0, 45, parts, this);
    sprites::draw(spr::SEASON_COTTAGE, 6, 7, 0, 55, parts, this);
    // Smoke from the chimney (column 12, row 7), bent by the wind.
    for (int p = 0; p < 3; p++) {
      const float age = fmodf(t_ * 0.35f + p / 3.0f, 1.0f);
      blend(12 + wind * age * 2 + sinf(age * 7 + p) * 0.6f, 6.5f - age * 6, 0.3f * (1 - age));
    }

    // Nearer snow in front, then the ground: white, its drifts on top.
    for (Flake &f : flakes_) {
      if (f.layer > 0) fall(f, wind, dt, nullptr);
    }
    for (int x = 0; x < COLS; x++) {
      drift_[x] = max(0.2f, drift_[x] - dt * 0.012f);  // settling, so it never fills up
      const float top = ROWS - 1 - drift_[x];
      for (int y = ROWS - 1; y >= (int)floorf(top); y--) {
        const float cover = constrain(y + 1 - top, 0.0f, 1.0f);
        const uint8_t snow = ui::tone(0.3f + 0.08f * sinf(x * 1.7f + y));
        const uint8_t was = display.getLevel(x, y);
        display.setLevel(x, y, (uint8_t)(was + (snow - was) * cover));
      }
    }
  }

 private:
  static const int FLAKES = 21;
  struct Flake {
    float x, y, speed, phase;
    uint8_t layer;  // 0 far, 1 middle, 2 near
  } flakes_[FLAKES];
  float drift_[COLS];
  float t_ = 0;
  uint8_t window_ = 180;

  static void reset(Flake &f, uint8_t layer, bool anywhere) {
    static const float SPEED[3] = {1.0f, 1.8f, 3.0f};  // rows per second
    f.layer = layer;
    f.x = rnd() * COLS;
    f.y = anywhere ? rnd() * ROWS : -rnd() * 3;
    f.speed = SPEED[layer] * (0.8f + 0.4f * rnd());
    f.phase = rnd() * 6.28f;
  }
  // Moves a flake on and draws it (into `px`, or over the panel); near
  // ones land on the drifts and pile up.
  void fall(Flake &f, float wind, float dt, float (*px)[COLS]) {
    static const float LIGHT[3] = {0.16f, 0.4f, 0.85f};
    const float depth = 0.4f + 0.3f * f.layer;  // the far ones drift less
    f.y += f.speed * dt;
    f.x += (wind * depth + 0.5f * sinf(t_ * 1.7f + f.phase)) * dt;
    f.x = fmodf(f.x + COLS, COLS);
    const int col = (int)f.x;
    if (f.layer > 0 && f.y >= ROWS - 1 - drift_[col]) {
      drift_[col] = min(1.8f, drift_[col] + (f.layer == 2 ? 0.1f : 0.05f));
      reset(f, f.layer, false);
      return;
    }
    if (f.y > ROWS) reset(f, f.layer, false);
    if (px) {
      const int x = (int)lroundf(f.x) % COLS, y = (int)lroundf(f.y);
      if (y >= 0 && y < ROWS) px[y][x] = max(px[y][x], LIGHT[0]);
    } else {
      blend(f.x, f.y, LIGHT[f.layer]);
    }
  }
  // A soft point over what is drawn: shared by the four pixels around it.
  static void blend(float x, float y, float v) {
    const int x0 = (int)floorf(x), y0 = (int)floorf(y);
    const float fx = x - x0, fy = y - y0;
    const float w[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
    for (int i = 0; i < 4; i++) {
      const int px = ((x0 + (i & 1)) % COLS + COLS) % COLS, py = y0 + (i >> 1);
      if (py < 0 || py >= ROWS || w[i] < 0.15f) continue;
      const uint8_t l = ui::tone(v * w[i]);
      if (l > display.getLevel(px, py)) display.setLevel(px, py, l);
    }
  }
  static int parts(char mark, uint8_t level, void *self) {
    switch (mark) {
      case 's': return 110;                                          // snow on branches and roof
      case 'w': return static_cast<SnowAnimation *>(self)->window_;  // the lit window
      case 'd': return 40;                                           // the door
      case 'c': return 70;                                           // the chimney
      default: return level;
    }
  }
};

// ---------------------------------------------------------------------------
// A Christmas tree on a snowy night: its tiers shaded, snow on the tips of
// the branches, a garland of lights with a wave of light running down it
// (each light glowing on the needles around it), the star on top pulsing
// and now and then sparkling, presents underneath, a few flakes falling.
class XmasTreeAnimation : public Seasonal {
 public:
  const char *id() const override { return "xmastree"; }
  const char *name() const override { return "Albero di Natale"; }
  uint16_t frameMs() const override { return 50; }
  void start() override {
    for (Flake &f : flakes_) f = {rnd() * COLS, rnd() * ROWS, 0.8f + rnd() * 0.8f, rnd() * 6};
    t_ = 0;
  }
  void frame(uint32_t) override {
    const float dt = frameMs() / 1000.0f;
    t_ += dt;
    float px[ROWS][COLS] = {};
    // The tree: three tiers, each wider going down; their lowest row is
    // the snowy tips of the branches, their sides a little lighter.
    static const int8_t HALF[ROWS] = {-1, -1, 0, 1, 2, 1, 2, 3, 4, 2, 3, 4, 5, 6, -1, -1};
    for (int y = 0; y < ROWS; y++) {
      if (HALF[y] < 0) continue;
      const bool tips = y + 1 < ROWS && HALF[y + 1] <= HALF[y];
      for (int x = 7 - HALF[y]; x <= 8 + HALF[y]; x++) {
        const bool side = x == 7 - HALF[y] || x == 8 + HALF[y];
        px[y][x] = tips ? (side ? 0.22f : 0.3f) : side ? 0.16f : 0.1f + 0.03f * ((x * 7 + y * 3) % 3);
      }
    }
    for (int y = 13; y < ROWS; y++) px[y][7] = px[y][8] = 0.12f;  // the trunk
    for (int x = 0; x < COLS; x++) px[ROWS - 1][x] = 0.28f + 0.05f * sinf(x * 1.9f);  // snow on the ground

    // The garland: a wave of light runs down it; each light lights the
    // needles around it.
    static const uint8_t LIGHTS[][2] = {{7, 2},  {8, 4},  {6, 4},  {9, 6},  {7, 7},  {5, 8},  {10, 8},
                                        {8, 10}, {5, 10}, {11, 11}, {7, 12}, {3, 12}, {12, 12}, {9, 13}};
    const int count = sizeof(LIGHTS) / sizeof(LIGHTS[0]);
    for (int i = 0; i < count; i++) {
      const float wave = 0.5f + 0.5f * cosf(t_ * 2.6f - i * 0.75f);
      const float b = 0.3f + 0.7f * wave * wave;
      const int x = LIGHTS[i][0], y = LIGHTS[i][1];
      px[y][x] = max(px[y][x], b);
      static const int8_t AROUND[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (const auto &d : AROUND) {
        const int nx = x + d[0], ny = y + d[1];
        if (nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS && px[ny][nx] > 0) px[ny][nx] = max(px[ny][nx], 0.1f + 0.25f * b);
      }
    }
    // The star: pulsing, and every few seconds a sparkle of four rays.
    const float star = 0.65f + 0.35f * sinf(t_ * 2.2f);
    px[0][7] = px[0][8] = star;
    px[1][7] = px[1][8] = star * 0.7f;
    const float spark = fmodf(t_, 4.0f);
    if (spark < 0.6f) {
      const float k = sinf(spark / 0.6f * (float)M_PI);
      px[0][6] = px[0][9] = max(px[0][6], 0.5f * k);
      px[1][5] = px[1][10] = max(px[1][5], 0.35f * k);
      px[2][6] = px[2][9] = max(px[2][6], 0.3f * k);
    }
    // A few flakes, slow, swaying.
    for (Flake &f : flakes_) {
      f.y += f.speed * dt;
      f.x += 0.3f * sinf(t_ + f.phase) * dt;
      if (f.y > ROWS - 1) f = {rnd() * COLS, -1, 0.8f + rnd() * 0.8f, rnd() * 6};
      const int x = ((int)lroundf(f.x) % COLS + COLS) % COLS, y = (int)lroundf(f.y);
      if (y >= 0 && y < ROWS) px[y][x] = max(px[y][x], 0.32f);
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) display.setLevel(x, y, ui::tone(px[y][x]));
    }
    // The presents, in front of the lowest branches.
    sprites::draw(spr::SEASON_GIFTS, 1, 12, 0, 70, ribbon, nullptr);
  }

 private:
  struct Flake {
    float x, y, speed, phase;
  } flakes_[9];
  float t_ = 0;
  static int ribbon(char mark, uint8_t level, void *) { return mark == 'r' ? 190 : level; }
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
// Halloween night: a full moon and a few stars, a bat flapping across
// now and then, and the pumpkin - round, with its ribs - its carved face
// lit by a candle inside that flickers and gutters now and then (the skin
// brightens and darkens a little with it).
class PumpkinAnimation : public Seasonal {
 public:
  const char *id() const override { return "pumpkin"; }
  const char *name() const override { return "Zucca di Halloween"; }
  uint16_t frameMs() const override { return 60; }
  void start() override { t_ = 0; }
  void frame(uint32_t) override {
    t_ += frameMs() / 1000.0f;
    float px[ROWS][COLS] = {};
    // Stars, and the moon with a darker sea on it.
    static const uint8_t STARS[][2] = {{1, 1}, {5, 0}, {8, 2}, {3, 3}};
    for (int i = 0; i < 4; i++) px[STARS[i][1]][STARS[i][0]] = 0.1f + 0.08f * sinf(t_ * (1.1f + i * 0.5f) + i * 2);
    for (int y = 0; y < 5; y++) {
      for (int x = 10; x < COLS; x++) {
        const float dx = x + 0.5f - 13, dy = y + 0.5f - 2.2f, d = sqrtf(dx * dx + dy * dy);
        if (d < 2.3f) px[y][x] = (0.5f - ((x == 12 && y == 2) || (x == 13 && y == 1) ? 0.15f : 0)) * min(1.0f, (2.3f - d) * 1.5f);
      }
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) display.setLevel(x, y, ui::tone(px[y][x]));
    }
    // A bat crossing every 9 seconds, flapping, bobbing.
    const float b = fmodf(t_, 9.0f);
    if (b < 5) {
      const int x = (int)lroundf(-5 + b / 5 * 21), y = (int)lroundf(1.5f + 1.2f * sinf(b * 3));
      sprites::draw(spr::SEASON_BAT, x, y, (int)(t_ * 8) % 2, 110);
    }
    // The candle: a restless flame, guttering low now and then.
    flame_ += ((float)(esp_random() % 1000) / 1000 - 0.5f) * 0.25f;
    flame_ += (0.85f - flame_) * 0.15f;
    if (esp_random() % 120 == 0) flame_ = 0.35f;  // a draught
    flame_ = constrain(flame_, 0.3f, 1.0f);
    // The skin dim, the carving bright: the candle is all the light.
    sprites::draw(spr::SEASON_PUMPKIN, 1, 5, 0, (uint8_t)(80 + 20 * flame_));
    const uint8_t face = (uint8_t)(255 * flame_);
    sprites::draw(spr::SEASON_PUMPKIN_FACE, 1, 5, 0, face);
  }

 private:
  float t_ = 0, flame_ = 0.85f;
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
PumpkinAnimation pumpkin;
CakeAnimation cake;

}  // namespace

extern Animation *const snowAnimation = &snow;
extern Animation *const xmasTreeAnimation = &xmasTree;
extern Animation *const fireworksAnimation = &fireworks;
extern Animation *const heartsAnimation = &hearts;
extern Animation *const pumpkinAnimation = &pumpkin;
extern Animation *const cakeAnimation = &cake;
