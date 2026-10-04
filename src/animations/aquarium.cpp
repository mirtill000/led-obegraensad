// "Acquario": a small tank - fish swimming as a school (each keeps away
// from the others, swims along with its neighbours and towards the middle
// of the group, and turns before the glass), water weeds swaying, bubbles
// rising, sand. "Dai da mangiare" (the page's button, or A) drops a few
// flakes: they sink slowly and the fish dart for them.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "ui.h"

namespace {

const int FISH = 5, FLAKES = 8, BUBBLES = 6;
const float TOP = 1.0f, FLOOR = 12.5f;  // where the fish may swim
const float STEP = 0.04f;               // seconds per step

float frand() { return (esp_random() & 0xFFFF) / 65535.0f; }
float clamp01(float v) { return constrain(v, 0.0f, 1.0f); }

}  // namespace

class AquariumAnimation : public Animation {
 public:
  const char *id() const override { return "aquarium"; }
  const char *name() const override { return "Acquario"; }
  const char *group() const override { return "Atmosfere"; }
  uint16_t frameMs() const override { return 40; }
  bool fixedStep() const override { return true; }
  const char *pokeName() const override { return "Dai da mangiare"; }

  void start() override {
    for (Fish &f : fish_) {
      f.x = 2 + frand() * 12;
      f.y = 3 + frand() * 8;
      f.vx = (frand() - 0.5f) * 2;
      f.vy = (frand() - 0.5f) * 0.6f;
      f.phase = frand() * 6;
    }
    for (Flake &k : flakes_) k.alive = false;
    for (Bubble &b : bubbles_) b.alive = false;
    t_ = 0;
  }

  void poke() override {
    int dropped = 0;
    for (Flake &k : flakes_) {
      if (k.alive || dropped >= 5) continue;
      k = {true, 3 + frand() * 10, -frand() * 2, 0};
      dropped++;
    }
  }

  void frame(uint32_t) override {
    t_ += STEP;
    swim();
    sink();
    rise();
    draw();
  }

 private:
  struct Fish {
    float x, y, vx, vy, phase;
    float size;  // 0 small .. 1 big (set by draw order)
  } fish_[FISH];
  struct Flake {
    bool alive;
    float x, y, rest;  // rest: seconds on the sand
  } flakes_[FLAKES];
  struct Bubble {
    bool alive;
    float x, y, phase;
  } bubbles_[BUBBLES];
  float t_ = 0, nextBubble_ = 0;
  float water_[ROWS][COLS];

  void swim() {
    for (int i = 0; i < FISH; i++) {
      Fish &f = fish_[i];
      float ax = 0, ay = 0, cx = 0, cy = 0, avx = 0, avy = 0;
      int n = 0;
      for (int j = 0; j < FISH; j++) {
        if (j == i) continue;
        const float dx = fish_[j].x - f.x, dy = fish_[j].y - f.y, d2 = dx * dx + dy * dy;
        if (d2 < 12) {  // too close: move apart (more across than along)
          ax -= dx / (d2 + 0.1f) * 1.5f;
          ay -= dy / (d2 + 0.1f) * 3.0f;
        }
        if (d2 < 36) {
          cx += fish_[j].x;
          cy += fish_[j].y;
          avx += fish_[j].vx;
          avy += fish_[j].vy;
          n++;
        }
      }
      if (n) {
        ax += (cx / n - f.x) * 0.05f + (avx / n - f.vx) * 0.25f;  // towards the group, along with it
        ay += (cy / n - f.y) * 0.03f + (avy / n - f.vy) * 0.25f;
      }
      // Food: the nearest flake pulls hard.
      const Flake *food = nullptr;
      float best = 1e9f;
      for (const Flake &k : flakes_) {
        if (!k.alive || k.y < 0) continue;
        const float d2 = (k.x - f.x) * (k.x - f.x) + (k.y - f.y) * (k.y - f.y);
        if (d2 < best) best = d2, food = &k;
      }
      if (food) {
        const float d = sqrtf(best) + 0.01f;
        ax += (food->x - f.x) / d * 3;
        ay += (food->y - f.y) / d * 3;
      }
      // Wander, and turn before the glass, the surface and the sand.
      ax += sinf(t_ * 0.7f + f.phase * 3) * 0.6f;
      ay += cosf(t_ * 0.5f + f.phase * 5) * 0.25f;
      if (f.x < 2) ax += (2 - f.x) * 2;
      if (f.x > 14) ax -= (f.x - 14) * 2;
      if (f.y < TOP + 1) ay += (TOP + 1 - f.y) * 2;
      if (f.y > FLOOR - 1) ay -= (f.y - FLOOR + 1) * 2;
      f.vx += ax * STEP;
      f.vy += ay * STEP;
      // Cruise at 1.5 - 3 px/s (faster after food), mostly level.
      const float maxV = food ? 4.5f : 2.6f;
      const float v = sqrtf(f.vx * f.vx + f.vy * f.vy);
      if (v > maxV) f.vx *= maxV / v, f.vy *= maxV / v;
      if (v < 1.2f && v > 0.01f) f.vx *= 1.2f / v, f.vy *= 1.2f / v;
      f.vy *= 0.97f;
      f.x = constrain(f.x + f.vx * STEP, 0.5f, 15.5f);
      f.y = constrain(f.y + f.vy * STEP, TOP, FLOOR);
      // Eat what is at the mouth.
      for (Flake &k : flakes_) {
        if (k.alive && fabsf(k.x - (f.x + (f.vx > 0 ? 1 : -1))) < 0.9f && fabsf(k.y - f.y) < 0.9f) k.alive = false;
      }
    }
  }

  void sink() {
    for (Flake &k : flakes_) {
      if (!k.alive) continue;
      if (k.y < 14) {
        k.y += 0.9f * STEP;
        k.x += sinf(t_ * 2 + k.x) * 0.3f * STEP;
      } else if ((k.rest += STEP) > 8) {
        k.alive = false;  // gone into the sand
      }
    }
  }

  void rise() {
    if (t_ >= nextBubble_) {
      nextBubble_ = t_ + 0.5f + frand() * 1.2f;
      for (Bubble &b : bubbles_) {
        if (b.alive) continue;
        b = {true, 11.5f + frand(), 13.5f, frand() * 6};
        break;
      }
    }
    for (Bubble &b : bubbles_) {
      if (!b.alive) continue;
      b.y -= 2.2f * STEP;
      b.x += sinf(t_ * 5 + b.phase) * 0.6f * STEP;
      if (b.y < 0) b.alive = false;
    }
  }

  // A point shared between the four pixels around it.
  void put(float x, float y, float level) {
    const int x0 = (int)floorf(x), y0 = (int)floorf(y);
    const float fx = x - x0, fy = y - y0;
    const float w[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
    for (int i = 0; i < 4; i++) {
      const int px = x0 + (i & 1), py = y0 + (i >> 1);
      if (px < 0 || py < 0 || px >= COLS || py >= ROWS) continue;
      water_[py][px] = min(1.0f, water_[py][px] + level * w[i]);
    }
  }

  void draw() {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) water_[y][x] = 0;
    }
    // The surface shimmering, the sand with its ripples.
    for (int x = 0; x < COLS; x++) water_[0][x] = 0.11f + 0.04f * sinf(x * 0.9f + t_ * 1.5f);
    for (int y = 14; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) water_[y][x] = 0.12f + 0.06f * sinf(x * 1.7f + y * 2.3f);
    }
    // Weeds: stalks rooted in the sand, swaying more towards the tip.
    static const float WEEDS[3][2] = {{2, 7}, {8.5f, 5}, {13.5f, 8}};  // x, height
    for (const auto &w : WEEDS) {
      for (int k = 0; k < (int)w[1]; k++) {
        const float sway = sinf(t_ * 1.1f + w[0] + k * 0.45f) * k * 0.13f;
        put(w[0] + sway, 13.5f - k, 0.32f - k * 0.02f);
      }
    }
    for (const Bubble &b : bubbles_) {
      if (b.alive) put(b.x, b.y, 0.35f);
    }
    for (const Flake &k : flakes_) {
      if (k.alive && k.y >= 0) put(k.x, k.y, 0.6f);
    }
    // Fish: a bright head, the body, a tail that wags; facing their way,
    // tilted a little as they climb or dive.
    for (const Fish &f : fish_) {
      const float dir = f.vx >= 0 ? 1 : -1, tilt = constrain(f.vy / (fabsf(f.vx) + 0.5f), -0.5f, 0.5f);
      const float wag = sinf(t_ * 12 + f.phase * 5) * 0.45f;
      put(f.x + dir * 1.0f, f.y - tilt, 0.95f);
      put(f.x, f.y, 0.6f);
      put(f.x - dir * 1.0f, f.y + tilt * 0.5f + wag, 0.35f);
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) display.setLevel(x, y, ui::tone(water_[y][x]));
    }
  }
};

static AquariumAnimation aquarium;
extern Animation *const aquariumAnimation = &aquarium;
