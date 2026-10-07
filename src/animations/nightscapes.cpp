// Night scenes, quiet and slow, drawn as shades (ui::tone) on a float
// canvas with sub-pixel points so things glide:
//  - Faro: a lighthouse on a cliff, its beam sweeping through the mist and
//    flashing when it faces you, waves breaking below;
//  - Lucciole: fireflies over the grass, each blinking at its own pace,
//    falling into step with each other little by little, then scattering;
//  - Aurora boreale: curtains of light waving over dark mountains,
//    mirrored in a lake;
//  - Pioggia sul vetro: drops on a window pane, growing, sliding down in
//    fits and starts, leaving trails, city lights blurred behind;
//  - Falò: a campfire, its flames licking up from the logs, sparks rising;
//  - Stelle cadenti: a deep sky with the Milky Way and now and then a
//    shooting star (a shower of them during the Perseids, 9-14 August);
//  - Stelle circolari: the sky turning round the pole star, the stars
//    drawing arcs as in a long exposure, over the trees;
//  - Treno nella notte: a train crossing the countryside, its windows lit,
//    under the moon;
//  - Respiro: one soft wave of light breathing in (4 s) and out (6 s).
#include <math.h>
#include <time.h>

#include "animation.h"
#include "display.h"
#include "timekeeping.h"
#include "ui.h"

namespace {

const float PI_F = (float)M_PI;

float clamp01(float v) { return constrain(v, 0.0f, 1.0f); }
float smooth(float a, float b, float x) {
  const float k = clamp01((x - a) / (b - a));
  return k * k * (3 - 2 * k);
}
float hash01(int32_t a, int32_t b) {
  uint32_t h = (uint32_t)a * 0x8da6b343u ^ (uint32_t)b * 0xd8163841u;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return (h >> 8) / 16777216.0f;
}
float rnd() { return (esp_random() & 0xFFFF) / 65535.0f; }

// A frame of brightnesses (0..1), shown through ui::tone().
struct Canvas {
  float px[ROWS][COLS];
  void fill(float v) {
    for (auto &row : px) {
      for (float &p : row) p = v;
    }
  }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < COLS && y < ROWS; }
  void set(int x, int y, float v) {
    if (in(x, y)) px[y][x] = v;
  }
  void lift(int x, int y, float v) {  // at least v
    if (in(x, y)) px[y][x] = max(px[y][x], v);
  }
  void add(int x, int y, float v) {
    if (in(x, y)) px[y][x] = min(1.0f, px[y][x] + v);
  }
  // A point at (x, y), shared among the four pixels around it.
  void put(float x, float y, float v) {
    const int x0 = (int)floorf(x), y0 = (int)floorf(y);
    const float fx = x - x0, fy = y - y0;
    add(x0, y0, v * (1 - fx) * (1 - fy));
    add(x0 + 1, y0, v * fx * (1 - fy));
    add(x0, y0 + 1, v * (1 - fx) * fy);
    add(x0 + 1, y0 + 1, v * fx * fy);
  }
  // A soft glow of radius r around (x, y).
  void glow(float x, float y, float r, float v) {
    for (int py = (int)(y - r - 1); py <= (int)(y + r + 1); py++) {
      for (int px_ = (int)(x - r - 1); px_ <= (int)(x + r + 1); px_++) {
        const float d = hypotf(px_ + 0.5f - x, py + 0.5f - y);
        if (d < r) add(px_, py, v * (1 - d / r) * (1 - d / r));
      }
    }
  }
  void show() {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) display.setLevel(x, y, ui::tone(px[y][x]));
    }
  }
};

// A few stars twinkling, fixed, in rows top..bottom-1.
void stars(Canvas &c, float t, int bottom, float amount, int seed) {
  for (int y = 0; y < bottom; y++) {
    for (int x = 0; x < COLS; x++) {
      const float r = hash01(x * 7 + seed, y);
      if (r < 0.07f) c.lift(x, y, amount * (0.12f + 0.08f * sinf(t * (1 + r * 25) + r * 90)));
    }
  }
}

class Nightscape : public Animation {
 public:
  const char *group() const override { return "Atmosfere"; }
};

// ---------------------------------------------------------------------------
class LighthouseAnimation : public Nightscape {
 public:
  const char *id() const override { return "lighthouse"; }
  const char *name() const override { return "Faro"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    Canvas c;
    c.fill(0);
    stars(c, t, 7, 1, 3);
    // The beam turns once every 8 s: towards you it flashes, to the side
    // it is a cone of light through the mist, longer the more sideways.
    const float a = t * 2 * PI_F / 8;
    const float side = cosf(a), facing = sinf(a);
    const float lx = 3.5f, ly = 4.5f;  // the lamp
    if (fabsf(side) > 0.05f && facing < 0.6f) {
      const float dir = side > 0 ? 1 : -1, len = 15 * fabsf(side);
      for (int x = 0; x < COLS; x++) {
        const float d = (x + 0.5f - lx) * dir;
        if (d < 0.5f || d > len) continue;
        const float half = 0.5f + d * 0.13f;
        for (int y = 0; y < 12; y++) {
          const float off = fabsf(y + 0.5f - ly) / half;
          if (off > 1) continue;
          const float mist = 0.75f + 0.25f * sinf(x * 1.3f + y * 2.1f + t * 0.8f);
          c.add(x, y, (1 - off) * (1 - d / (len + 2)) * 0.5f * mist);
        }
      }
    }
    // The tower: striped, on the cliff; the lantern on top.
    for (int y = 5; y < 12; y++) {
      const float stripe = (y / 2) % 2 ? 0.22f : 0.1f;
      c.set(3, y, stripe);
      c.set(4, y, stripe * 0.8f);
    }
    c.set(3, 4, 0.5f);
    c.set(4, 4, 0.5f);
    c.set(3, 3, 0.15f);
    c.set(4, 3, 0.15f);
    c.glow(lx, ly - 0.5f, 1.5f + 2.5f * max(0.0f, facing), 0.3f + 0.7f * max(0.0f, facing));
    // The cliff, and the sea with waves running in.
    static const uint8_t CLIFF[COLS] = {11, 11, 11, 12, 12, 12, 13, 14, 16, 16, 16, 16, 16, 16, 16, 16};
    for (int x = 0; x < COLS; x++) {
      for (int y = CLIFF[x]; y < ROWS; y++) c.set(x, y, y == CLIFF[x] ? 0.08f : 0.02f);
    }
    for (int y = 13; y < ROWS; y++) {
      for (int x = 7; x < COLS; x++) {
        if (CLIFF[x] <= y) continue;
        const float wave = sinf(x * 0.9f + t * 1.6f + y * 2) * sinf(x * 0.35f - t * 0.7f);
        float v = 0.05f + (wave > 0.75f ? 0.25f : 0);
        if (side > 0.3f && facing < 0.6f) v += 0.12f * side;  // the beam on the water
        c.set(x, y, v);
      }
    }
    // Spray where the waves hit the cliff.
    const float spray = max(0.0f, sinf(t * 1.6f));
    c.put(6.5f, 12.5f - spray * 1.5f, 0.4f * spray);
    c.show();
  }
};

// ---------------------------------------------------------------------------
class FirefliesAnimation : public Nightscape {
 public:
  const char *id() const override { return "fireflies"; }
  const char *name() const override { return "Lucciole"; }
  uint16_t frameMs() const override { return 40; }
  bool fixedStep() const override { return true; }
  void start() override {
    for (Fly &f : flies_) f = {rnd() * 6.3f, 0.8f + rnd() * 0.5f, rnd() * 100, rnd() * 100};
    t_ = 0;
  }
  void frame(uint32_t) override {
    const float dt = frameMs() / 1000.0f;
    t_ += dt;
    // Each one blinks on its own clock, nudged by the others' (they fall
    // into step); every minute or so the spell breaks and they scatter.
    const float pull = fmodf(t_, 70) < 50 ? 0.35f : 0;
    float sx = 0, sy = 0;
    for (const Fly &f : flies_) {
      sx += cosf(f.phase);
      sy += sinf(f.phase);
    }
    const float mean = atan2f(sy, sx);
    for (Fly &f : flies_) {
      f.phase += (f.rate * 2.2f + pull * sinf(mean - f.phase)) * dt;
      if (fmodf(t_, 70) < dt) f.phase = rnd() * 6.3f;  // scatter
    }
    Canvas c;
    c.fill(0);
    stars(c, t_, 6, 0.6f, 11);
    // The grass: blades of different heights, swaying a little.
    for (int x = 0; x < COLS; x++) {
      const int h = 2 + (int)(hash01(x, 1) * 3);
      for (int k = 0; k < h; k++) {
        const float sway = sinf(t_ * 0.9f + x) * k * 0.15f;
        c.put(x + sway, ROWS - 1 - k, 0.09f);
      }
    }
    for (Fly &f : flies_) {
      const float x = 8 + 6.5f * sinf(t_ * 0.11f + f.sx) + 1.2f * sinf(t_ * 0.53f + f.sy);
      const float y = 8 + 3.5f * sinf(t_ * 0.09f + f.sy) + 1.0f * cosf(t_ * 0.47f + f.sx);
      const float s = sinf(f.phase);
      const float b = s > 0 ? powf(s, 4) : 0;
      if (b > 0.02f) {
        c.glow(x, y, 1.8f, 0.25f * b);
        c.put(x - 0.5f, y - 0.5f, 0.9f * b);
      }
    }
    c.show();
  }

 private:
  struct Fly {
    float phase, rate, sx, sy;
  } flies_[12];
  float t_ = 0;
};

// ---------------------------------------------------------------------------
class AuroraAnimation : public Nightscape {
 public:
  const char *id() const override { return "aurora"; }
  const char *name() const override { return "Aurora boreale"; }
  uint16_t frameMs() const override { return 50; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    Canvas c;
    c.fill(0);
    stars(c, t, 10, 0.7f, 21);
    const int LAKE = 12;
    // The curtains: a bright lower hem waving, light rising and fading
    // above it, rays shimmering along.
    float sky[LAKE][COLS] = {};
    for (int x = 0; x < COLS; x++) {
      const float hem = 6 + 1.4f * sinf(x * 0.45f + t * 0.35f) + 0.5f * sinf(x * 0.9f - t * 0.5f);
      const float rays = 0.65f + 0.35f * sinf(x * 1.3f - t * 0.9f);
      const float strength = 0.6f + 0.4f * sinf(t * 0.21f + x * 0.2f);
      for (int y = 0; y < LAKE; y++) {
        const float above = hem - (y + 0.5f);
        if (above < -0.6f) continue;
        const float v = above < 0 ? (1 + above / 0.6f) : expf(-above / 2.6f);
        sky[y][x] = 0.75f * v * rays * strength;
      }
    }
    for (int y = 0; y < LAKE; y++) {
      for (int x = 0; x < COLS; x++) c.lift(x, y, sky[y][x]);
    }
    // Mountains against it, black.
    for (int x = 0; x < COLS; x++) {
      const int top = 9 + (int)(2.2f * fabsf(sinf(x * 0.38f + 0.6f)) - 0.6f);
      for (int y = top; y < LAKE; y++) c.set(x, y, 0);
    }
    // The lake: the curtains' light in streaks, shimmering, fainter deeper.
    for (int x = 0; x < COLS; x++) {
      float light = 0;
      for (int y = 0; y < LAKE; y++) light = max(light, sky[y][x]);
      for (int y = LAKE; y < ROWS; y++) {
        const float ripple = 0.5f + 0.5f * sinf(y * 2.5f + x * 0.7f + t * 1.5f);
        c.set(x, y, light * ripple * (0.55f - 0.08f * (y - LAKE)));
      }
    }
    c.show();
  }
};

// ---------------------------------------------------------------------------
class RainGlassAnimation : public Nightscape {
 public:
  const char *id() const override { return "rainglass"; }
  const char *name() const override { return "Pioggia sul vetro"; }
  uint16_t frameMs() const override { return 50; }
  bool fixedStep() const override { return true; }
  void start() override {
    for (Drop &d : drops_) d = {rnd() * COLS, rnd() * ROWS, 0.2f + rnd() * 0.4f, 0, 0};
    for (auto &row : trail_) {
      for (float &v : row) v = 0;
    }
    t_ = 0;
  }
  void frame(uint32_t) override {
    const float dt = frameMs() / 1000.0f;
    t_ += dt;
    Canvas c;
    // Behind the glass: city lights, out of focus, slowly changing.
    c.fill(0);
    static const float LIGHTS[][3] = {{3, 4, 2.5f}, {10, 3, 3}, {13, 9, 2.2f}, {6, 10, 2.8f}, {1, 12, 2}};
    for (int i = 0; i < 5; i++) {
      const float b = 0.4f + 0.12f * sinf(t_ * (0.2f + i * 0.07f) + i);
      c.glow(LIGHTS[i][0], LIGHTS[i][1], LIGHTS[i][2], b);
    }
    // Trails dry up slowly.
    for (auto &row : trail_) {
      for (float &v : row) v = max(0.0f, v - dt * 0.05f);
    }
    for (Drop &d : drops_) {
      // Drops grow as rain lands on them; past a size they slide, in fits
      // and starts, picking up the drops in their way.
      d.size = min(1.0f, d.size + dt * (0.02f + 0.05f * rnd()));
      if (d.size > 0.7f) {
        if (d.speed <= 0 || rnd() < 0.04f) d.speed = rnd() < 0.3f ? 0 : 2 + rnd() * 5;  // stick and slip
        d.y += d.speed * dt;
        d.x += sinf(d.y * 1.7f + d.wobble) * 0.25f * dt;
        if ((int)d.x >= 0 && (int)d.x < COLS && (int)d.y >= 0 && (int)d.y < ROWS) trail_[(int)d.y][(int)d.x] = 0.14f;
        for (Drop &o : drops_) {
          if (&o != &d && o.size <= 0.7f && fabsf(o.x - d.x) < 0.8f && fabsf(o.y - d.y) < 0.8f) {
            d.size = 1;
            o = {rnd() * COLS, rnd() * ROWS, 0.1f, 0, rnd() * 6};
          }
        }
        if (d.y > ROWS + 1) d = {rnd() * COLS, rnd() * 4, 0.1f, 0, rnd() * 6};
      }
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) c.add(x, y, trail_[y][x]);
    }
    for (const Drop &d : drops_) c.put(d.x, d.y, 0.25f + 0.55f * d.size);
    c.show();
  }

 private:
  struct Drop {
    float x, y, size, speed, wobble;
  } drops_[10];
  float trail_[ROWS][COLS];
  float t_ = 0;
};

// ---------------------------------------------------------------------------
class CampfireAnimation : public Nightscape {
 public:
  const char *id() const override { return "campfire"; }
  const char *name() const override { return "Falò"; }
  uint16_t frameMs() const override { return 60; }
  bool fixedStep() const override { return true; }
  void start() override {
    memset(heat_, 0, sizeof(heat_));
    for (Spark &s : sparks_) s.life = 0;
    t_ = 0;
  }
  void frame(uint32_t) override {
    const float dt = frameMs() / 1000.0f;
    t_ += dt;
    // Flames: heat rises from the logs and cools, kept to a cone.
    for (int x = 4; x < 12; x++) heat_[12][x] = 0.7f + 0.3f * rnd();
    for (int y = 0; y < 12; y++) {
      for (int x = 0; x < COLS; x++) {
        const int from = constrain(x + (int)(esp_random() % 3) - 1, 0, COLS - 1);
        // Narrower the higher: tongues of flame, not a blob.
        const float cone = clamp01(1.3f - fabsf(x + 0.5f - 8) / (0.8f + y * 0.3f));
        heat_[y][x] = max(0.0f, heat_[y + 1][from] - 0.03f - 0.07f * rnd()) * cone;
      }
    }
    Canvas c;
    c.fill(0);
    stars(c, t_, 5, 0.6f, 31);
    for (int y = 0; y < 13; y++) {
      for (int x = 0; x < COLS; x++) c.lift(x, y, heat_[y][x] * heat_[y][x] * 1.2f);
    }
    // Sparks: a few float up out of the fire, drifting, fading.
    for (Spark &s : sparks_) {
      if (s.life <= 0 && rnd() < 0.04f) s = {6.5f + rnd() * 3, 8, (rnd() - 0.5f) * 1.2f, -3 - rnd() * 2, 1};
      if (s.life <= 0) continue;
      s.x += s.vx * dt + sinf(t_ * 4 + s.y) * 0.03f;
      s.y += s.vy * dt;
      s.life -= dt * 0.6f;
      c.put(s.x, s.y, 0.8f * s.life);
    }
    // The logs, crossed, glowing; the ground lit by the fire.
    const float ember = 0.3f + 0.15f * sinf(t_ * 3) + 0.1f * rnd();
    for (int k = 0; k < 8; k++) {
      c.lift(4 + k, 13 - k / 4, k % 3 ? 0.18f : ember);
      c.lift(11 - k, 13 - k / 4, k % 3 ? 0.15f : ember);
    }
    for (int x = 0; x < COLS; x++) {
      const float d = fabsf(x + 0.5f - 8) / 8;
      c.set(x, 14, (0.18f + 0.06f * sinf(t_ * 5 + x)) * (1 - d));
      c.set(x, 15, 0.08f * (1 - d));
    }
    c.show();
  }

 private:
  float heat_[ROWS][COLS];
  struct Spark {
    float x, y, vx, vy, life;
  } sparks_[6];
  float t_ = 0;
};

// ---------------------------------------------------------------------------
class ShootingStarsAnimation : public Nightscape {
 public:
  const char *id() const override { return "meteors"; }
  const char *name() const override { return "Stelle cadenti"; }
  uint16_t frameMs() const override { return 30; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    Canvas c;
    c.fill(0);
    // The Milky Way: a faint band across, grainy.
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const float d = fabsf((x - y * 0.8f - 2) / 3.2f);
        if (d < 1 && hash01(x, y + 50) < 0.3f * (1 - d)) c.lift(x, y, 0.1f);
      }
    }
    stars(c, t, ROWS, 1.2f, 41);
    // Shooting stars: one every few seconds - every second or so during the
    // Perseids (9-14 August), all of them coming from the same corner.
    struct tm tm;
    const bool perseids = localTime(tm) && tm.tm_mon == 7 && tm.tm_mday >= 9 && tm.tm_mday <= 14;
    const float every = perseids ? 1.3f : 5.0f;
    for (int back = 0; back < 2; back++) {
      const int k = (int)(t / every) - back;
      const float age = t - k * every - hash01(k, 1) * every * 0.4f;
      const float life = 0.8f;
      if (age < 0 || age > life) continue;
      const float sx = 2 + hash01(k, 2) * 12, sy = hash01(k, 3) * 5;
      const float dx = perseids ? -0.8f : (hash01(k, 4) < 0.5f ? -1 : 1) * 0.8f, dy = 0.6f;
      const float run = age * 18;
      const float fade = 1 - age / life;
      for (int i = 0; i < 8; i++) {
        const float back_ = run - i * 0.8f;
        if (back_ < 0) continue;
        c.put(sx + dx * back_, sy + dy * back_, (1 - i / 8.0f) * fade);
      }
    }
    // A treeline.
    for (int x = 0; x < COLS; x++) {
      const int top = 14 - (int)(hash01(x, 9) * 2.5f);
      for (int y = top; y < ROWS; y++) c.set(x, y, 0);
    }
    c.show();
  }
};

// ---------------------------------------------------------------------------
class StarTrailsAnimation : public Nightscape {
 public:
  const char *id() const override { return "startrails"; }
  const char *name() const override { return "Stelle circolari"; }
  uint16_t frameMs() const override { return 50; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    Canvas c;
    c.fill(0);
    // A long exposure: the arcs grow for 40 s, stay a moment, fade, again.
    const float cycle = fmodf(t, 50);
    const float arc = min(cycle, 40.0f) * 0.07f;  // radians of arc drawn so far
    const float fade = cycle < 44 ? 1 : 1 - (cycle - 44) / 6;
    const float px = 8, py = 5;  // the pole star
    const float turn = t * 0.07f;
    // Each pixel: on which circle round the pole it lies and where along
    // it; lit if a star has swept past there - brightest where it is now.
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const float r = hypotf(x + 0.5f - px, y + 0.5f - py), a = atan2f(y + 0.5f - py, x + 0.5f - px);
        float v = 0;
        for (int i = 0; i < 14; i++) {
          const float rs = 2 + hash01(i, 1) * 11;
          const float near = 1 - fabsf(r - rs) / 0.6f;
          if (near <= 0) continue;
          float behind = fmodf(hash01(i, 2) * 2 * PI_F + turn - a + 8 * PI_F, 2 * PI_F);  // how far behind the star
          if (behind > PI_F * 1.9f) behind -= 2 * PI_F;  // just ahead of it
          if (behind < -0.15f || behind > arc + 0.15f) continue;
          const float head = behind < 0.3f ? 1 : 0.55f;
          v = max(v, near * head * (0.3f + 0.4f * hash01(i, 3)) * fade);
        }
        c.set(x, y, v);
      }
    }
    c.glow(px, py, 1.1f, 0.6f);
    // Trees against the sky.
    for (int x = 0; x < COLS; x++) {
      const float tree = 2.5f + 2.5f * fabsf(sinf(x * 1.1f + 0.4f)) * (x % 4 == 1 ? 1.4f : 0.7f);
      for (int y = (int)(ROWS - tree); y < ROWS; y++) c.set(x, y, 0);
    }
    c.show();
  }
};

// ---------------------------------------------------------------------------
class NightTrainAnimation : public Nightscape {
 public:
  const char *id() const override { return "train"; }
  const char *name() const override { return "Treno nella notte"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    Canvas c;
    c.fill(0);
    stars(c, t, 5, 0.6f, 51);
    c.glow(12.5f, 2.5f, 1.8f, 0.6f);  // the moon
    c.set(12, 2, 0.7f);
    c.set(13, 2, 0.7f);
    // Far hills, then the railway: poles and the rails.
    for (int x = 0; x < COLS; x++) {
      const int top = 7 + (int)(1.2f * sinf(x * 0.5f) + 0.8f);
      for (int y = top; y < ROWS; y++) c.set(x, y, y == top ? 0.07f : 0);
    }
    for (int x = 3; x < COLS; x += 9) {
      for (int y = 5; y < 13; y++) c.set(x, y, 0.1f);
      c.set(x - 1, 5, 0.1f);
      c.set(x + 1, 5, 0.1f);
    }
    for (int x = 0; x < COLS; x++) c.set(x, 13, 0.1f);
    // The train: every 20 s, from left to right, 45 columns long - the
    // engine with its headlight, then carriages with lit windows.
    const float pass = fmodf(t, 20);
    const float head = pass * 7 - 2;  // the front, in columns
    if (head - 45 < COLS) {
      for (int x = 0; x < COLS; x++) {
        const float back = head - (x + 0.5f);  // how far behind the front
        if (back < 0 || back > 45) continue;
        const bool gap = back > 6 && fmodf(back - 6, 13) < 1;  // between carriages
        if (gap) {
          c.set(x, 11, 0.05f);
          continue;
        }
        for (int y = 9; y < 13; y++) c.set(x, y, 0.07f);
        const bool engine = back <= 6;
        const bool window = !engine && fmodf(back - 6, 13) > 2 && fmodf(back - 6, 3) < 2;
        if (window) {
          const float flick = 0.55f + 0.1f * sinf(t * 7 + x);
          c.set(x, 10, flick);
          c.set(x, 13, 0.2f);  // its light on the ballast
        }
        if (engine && back > 4) c.set(x, 9, 0.12f);  // the cab
      }
      // The headlight and its beam ahead.
      c.set((int)head, 11, 1.0f);
      for (int k = 1; k < 5; k++) c.add((int)head + k, 11, 0.4f / k);
    }
    c.show();
  }
};

// ---------------------------------------------------------------------------
class BreathAnimation : public Nightscape {
 public:
  const char *id() const override { return "breath"; }
  const char *name() const override { return "Respiro"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    // 4 s breathing in, 6 s breathing out, eased like a breath.
    const float p = fmodf(now / 1000.0f, 10);
    const float k = p < 4 ? smooth(0, 4, p) : 1 - smooth(4, 10, p);
    const float r = 1.5f + 5.5f * k;
    Canvas c;
    c.fill(0);
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const float d = hypotf(x + 0.5f - 8, y + 0.5f - 8);
        // A soft disc, brightest just inside its edge, like a ripple.
        const float inside = clamp01(r - d + 0.5f);
        const float rim = expf(-(d - r) * (d - r) / 1.2f);
        c.set(x, y, (0.04f + 0.12f * k) * inside + (0.15f + 0.3f * k) * rim);
      }
    }
    c.show();
  }
};

LighthouseAnimation lighthouse;
FirefliesAnimation fireflies;
AuroraAnimation aurora;
RainGlassAnimation rainGlass;
CampfireAnimation campfire;
ShootingStarsAnimation meteors;
StarTrailsAnimation starTrails;
NightTrainAnimation nightTrain;
BreathAnimation breath;

}  // namespace

extern Animation *const lighthouseAnimation = &lighthouse;
extern Animation *const firefliesAnimation = &fireflies;
extern Animation *const auroraAnimation = &aurora;
extern Animation *const rainGlassAnimation = &rainGlass;
extern Animation *const campfireAnimation = &campfire;
extern Animation *const meteorsAnimation = &meteors;
extern Animation *const starTrailsAnimation = &starTrails;
extern Animation *const nightTrainAnimation = &nightTrain;
extern Animation *const breathAnimation = &breath;
