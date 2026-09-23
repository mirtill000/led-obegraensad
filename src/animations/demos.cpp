// "Demoscene" effects, rendered per pixel in grayscale.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "gfx.h"

// ---------------------------------------------------------------------------
// Wireframe cube rotating in 3D, with perspective; nearer edges are brighter.
class CubeAnimation : public Animation {
 public:
  const char *id() const override { return "cube"; }
  const char *name() const override { return "Cubo 3D"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    const float ax = t * 0.9f, ay = t * 0.6f, az = t * 0.3f;
    float px[8], py[8], pz[8];
    for (int i = 0; i < 8; i++) {
      float x = (i & 1) ? 1 : -1, y = (i & 2) ? 1 : -1, z = (i & 4) ? 1 : -1;
      float t1 = y * cosf(ax) - z * sinf(ax);  // rotate around X
      z = y * sinf(ax) + z * cosf(ax);
      y = t1;
      t1 = x * cosf(ay) + z * sinf(ay);  // around Y
      z = -x * sinf(ay) + z * cosf(ay);
      x = t1;
      t1 = x * cosf(az) - y * sinf(az);  // around Z
      y = x * sinf(az) + y * cosf(az);
      x = t1;
      const float scale = 11.0f / (z + 4.2f);  // perspective
      px[i] = 7.5f + x * scale;
      py[i] = 7.5f + y * scale;
      pz[i] = z;
    }
    display.clear();
    for (int a = 0; a < 8; a++) {
      for (int bit = 1; bit < 8; bit <<= 1) {
        const int b = a | bit;
        if (b == a) continue;  // each edge once: from the vertex without `bit`
        const float depth = (pz[a] + pz[b]) / 2;  // -1.7 (near) .. 1.7 (far)
        gfx::line(px[a], py[a], px[b], py[b], 0.95f - (depth + 1.7f) * 0.2f);
      }
    }
  }
};

// ---------------------------------------------------------------------------
// Classic plasma: sums of sine waves turned into soft bands.
class PlasmaAnimation : public Animation {
 public:
  const char *id() const override { return "plasma"; }
  const char *name() const override { return "Plasma"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    const float cx = 7.5f + 5 * sinf(t * 0.6f), cy = 7.5f + 5 * cosf(t * 0.45f);
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        float v = sinf(x * 0.5f + t) + sinf((y * 0.4f + t) * 1.1f) + sinf((x + y) * 0.3f + t * 0.8f);
        v += sinf(sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) * 0.7f - t * 1.5f);
        display.setLevel(x, y, gfx::level((sinf(v * PI * 0.5f) + 1) / 2));
      }
    }
  }
};

// ---------------------------------------------------------------------------
// Metaballs: three soft blobs that merge when they get close.
class MetaballsAnimation : public Animation {
 public:
  const char *id() const override { return "metaballs"; }
  const char *name() const override { return "Metaball"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    const float bx[3] = {7.5f + 5.5f * sinf(t * 0.8f), 7.5f + 5.0f * sinf(t * 0.53f + 2), 7.5f + 4.5f * cosf(t * 0.67f)};
    const float by[3] = {7.5f + 5.0f * cosf(t * 0.6f), 7.5f + 5.5f * cosf(t * 0.9f + 1), 7.5f + 5.0f * sinf(t * 0.41f + 4)};
    const float r2[3] = {7.0f, 5.5f, 4.5f};
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        float field = 0;
        for (int i = 0; i < 3; i++) {
          const float dx = x - bx[i], dy = y - by[i];
          field += r2[i] / (dx * dx + dy * dy + 0.5f);
        }
        // Soft edge around the surface at field = 1, dim glow outside.
        const float edge = (field - 0.75f) / 0.5f;
        display.setLevel(x, y, gfx::level(edge >= 0 ? 0.25f + 0.75f * fminf(edge, 1) : 0.25f * field / 0.75f));
      }
    }
  }
};

// ---------------------------------------------------------------------------
// Endless zoom into the Mandelbrot set's "seahorse valley".
class MandelbrotAnimation : public Animation {
 public:
  const char *id() const override { return "mandelbrot"; }
  const char *name() const override { return "Frattale di Mandelbrot"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 60; }
  void start() override { scale_ = 3.0f; }
  void frame(uint32_t) override {
    static const float CX = -0.743643887f, CY = 0.131825904f;
    scale_ *= 0.97f;
    if (scale_ < 3e-4f) scale_ = 3.0f;  // float precision runs out: start over
    const int maxIter = 40 + (int)(-logf(scale_ / 3) * 6);
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const float cr = CX + (x - 7.5f) / COLS * scale_, ci = CY + (y - 7.5f) / ROWS * scale_;
        float zr = 0, zi = 0;
        int i = 0;
        while (i < maxIter && zr * zr + zi * zi < 4) {
          const float t = zr * zr - zi * zi + cr;
          zi = 2 * zr * zi + ci;
          zr = t;
          i++;
        }
        // Inside the set: dark; outside: bands by escape time.
        display.setLevel(x, y, i == maxIter ? 0 : gfx::level(0.15f + 0.85f * ((i % 12) / 11.0f)));
      }
    }
  }

 private:
  float scale_ = 3.0f;
};

static CubeAnimation cube;
extern Animation *const cubeAnimation = &cube;
static PlasmaAnimation plasma;
extern Animation *const plasmaAnimation = &plasma;
static MetaballsAnimation metaballs;
extern Animation *const metaballsAnimation = &metaballs;
static MandelbrotAnimation mandelbrot;
extern Animation *const mandelbrotAnimation = &mandelbrot;
