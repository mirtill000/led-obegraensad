// Calm background animations: digital rain, fire, stars, waves.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "gfx.h"

static float random01() { return (esp_random() & 0xFFFF) / 65535.0f; }

// "Matrix" rain: a falling drop in each column, its tail fading out.
class RainAnimation : public Animation {
 public:
  const char *id() const override { return "rain"; }
  const char *name() const override { return "Pioggia digitale"; }
  const char *group() const override { return "Atmosfere"; }
  uint16_t frameMs() const override { return 60; }
  void start() override {
    for (Drop &d : drops_) respawn(d, 20);
  }
  void frame(uint32_t) override {
    display.clear();
    for (int x = 0; x < COLS; x++) {
      Drop &d = drops_[x];
      d.y += d.speed;
      if (d.y - d.length > ROWS) respawn(d, 10);
      const int head = (int)d.y;
      for (int i = 0; i < d.length; i++) {
        const float f = 1.0f - (float)i / d.length;
        display.setLevel(x, head - i, (uint8_t)(20 + 235 * f * f));
      }
    }
  }

 private:
  struct Drop {
    float y, speed;
    uint8_t length;
  } drops_[COLS];
  static void respawn(Drop &d, float spread) {
    d = {-random01() * spread, 0.25f + random01() * 0.5f, (uint8_t)(3 + esp_random() % 5)};
  }
};

// Classic "doom fire": heat rises from the bottom row and cools on the way
// up; brightness follows the heat.
class FireAnimation : public Animation {
 public:
  const char *id() const override { return "fire"; }
  const char *name() const override { return "Fuoco"; }
  const char *group() const override { return "Atmosfere"; }
  uint16_t frameMs() const override { return 70; }
  void start() override { memset(heat_, 0, sizeof(heat_)); }
  void frame(uint32_t) override {
    for (int x = 0; x < COLS; x++) heat_[ROWS - 1][x] = 150 + esp_random() % 106;
    for (int y = 0; y < ROWS - 1; y++) {
      for (int x = 0; x < COLS; x++) {
        const int from = x + (int)(esp_random() % 3) - 1;
        const int cooled = heat_[y + 1][(from + COLS) % COLS] - (int)(esp_random() % 32);
        heat_[y][x] = cooled > 0 ? cooled : 0;
      }
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const int l = (heat_[y][x] - 50) * 255 / 170;
        display.setLevel(x, y, l < 0 ? 0 : l > 255 ? 255 : l);
      }
    }
  }

 private:
  uint8_t heat_[ROWS][COLS];
};

// Stars that fade in, shine for a while and fade out again.
class StarsAnimation : public Animation {
 public:
  const char *id() const override { return "stars"; }
  const char *name() const override { return "Stelle"; }
  const char *group() const override { return "Atmosfere"; }
  uint16_t frameMs() const override { return 100; }
  void start() override {
    memset(life_, 0, sizeof(life_));
    memset(span_, 0, sizeof(span_));
  }
  void frame(uint32_t) override {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        if (life_[y][x] > 0) life_[y][x]--;
      }
    }
    if (esp_random() % 3 == 0) {
      const int y = esp_random() % ROWS, x = esp_random() % COLS;
      if (life_[y][x] == 0) life_[y][x] = span_[y][x] = 20 + esp_random() % 40;
    }
    display.clear();
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const int life = life_[y][x];
        if (life == 0) continue;
        const int age = span_[y][x] - life;
        const int fade = min(min(age, life), 8);  // 8 frames in, 8 frames out
        const int twinkle = (esp_random() % 5 == 0) ? 60 : 0;
        display.setLevel(x, y, max(0, fade * 255 / 8 - twinkle));
      }
    }
  }

 private:
  uint8_t life_[ROWS][COLS];  // frames left
  uint8_t span_[ROWS][COLS];  // total frames, to fade in and out
};

// Soft moving blobs from overlapping sine waves.
class WavesAnimation : public Animation {
 public:
  const char *id() const override { return "waves"; }
  const char *name() const override { return "Onde"; }
  const char *group() const override { return "Atmosfere"; }
  uint16_t frameMs() const override { return 60; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const float v = sinf(x * 0.45f + t) + sinf(y * 0.35f - t * 1.3f) + sinf((x + y) * 0.25f + t * 0.7f);
        display.setLevel(x, y, gfx::level((v + 0.4f) / 2.4f));  // v is in -3..3
      }
    }
  }
};

static RainAnimation rain;
extern Animation *const rainAnimation = &rain;
static FireAnimation fire;
extern Animation *const fireAnimation = &fire;
static StarsAnimation stars;
extern Animation *const starsAnimation = &stars;
static WavesAnimation waves;
extern Animation *const wavesAnimation = &waves;
