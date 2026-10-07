// Saturn, drawn flat (no raymarching) so it reads at 16x16: each pixel is
// the mean of 3x3 samples of a picture made of a lit disc, the rings as an
// ellipse (the half in front over the planet, the half behind hidden by
// it) and a few stars:
//  - Saturno: the classic picture, the rings rocking slowly, their shadow
//    across the planet's bands;
//  - Saturno e la luna: half lit by the sun, a moon going round it,
//    behind the planet and then in front;
//  - Saturno da vicino: a close-up, the planet in a corner and the rings
//    across the sky, their grooves turning.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "ui.h"

namespace {

const uint8_t STARS[][2] = {{1, 1}, {13, 2}, {3, 12}, {14, 13}, {9, 0}, {0, 7}, {15, 9}, {6, 14}};

float stars(float x, float y, float t) {
  for (int k = 0; k < 8; k++) {
    if ((int)x == STARS[k][0] && (int)y == STARS[k][1]) return 0.15f + 0.08f * sinf(t * 2 + k);
  }
  return 0;
}

class Saturn : public Animation {
 public:
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 50; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    setup(t);
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        float sum = 0;
        for (int i = 0; i < 3; i++) {
          for (int j = 0; j < 3; j++) sum += at(x + (i + 0.5f) / 3, y + (j + 0.5f) / 3, t);
        }
        display.setLevel(x, y, ui::tone(sum / 9));
      }
    }
  }

 protected:
  virtual void setup(float t) {}
  // The picture's brightness (0..1) at (x, y), in pixels.
  virtual float at(float x, float y, float t) const = 0;
};

// ---------------------------------------------------------------------------
class SaturnClassic : public Saturn {
 public:
  const char *id() const override { return "planet"; }
  const char *name() const override { return "Saturno"; }

 protected:
  float tilt_ = 0.32f;
  void setup(float t) override { tilt_ = 0.32f + 0.12f * sinf(t * 0.5f); }
  float at(float x, float y, float t) const override {
    const float cx = 7.5f, cy = 7.8f, R = 3.7f;
    const float dx = x - cx, dy = y - cy;
    const float rr = hypotf(dx, dy / tilt_);  // radius in the rings' plane
    const bool planet = dx * dx + dy * dy < R * R;
    const bool front = dy > 0;  // the lower half of the ellipse is nearer
    if (rr > 5 && rr < 7.3f && (front || !planet)) return rr < 6.3f ? 0.85f : 0.5f;  // two rings
    if (rr > 4.4f && rr <= 5 && !planet) return 0;  // the gap next to the planet
    if (planet) {
      const float band = 0.55f + 0.2f * sinf(dy * 1.6f + 0.6f);
      const float z = sqrtf(max(0.0f, R * R - dx * dx - dy * dy));
      const float light = 0.35f + 0.65f * max(0.0f, (-dx * 0.5f - dy * 0.4f + z) / R);
      const float shadow = fabsf(dy + 0.9f * tilt_) < 0.45f ? 0.5f : 1;  // the rings' shadow
      return 0.55f * band * light * shadow;
    }
    return stars(x, y, t);
  }
};

// ---------------------------------------------------------------------------
class SaturnMoon : public Saturn {
 public:
  const char *id() const override { return "saturnmoon"; }
  const char *name() const override { return "Saturno e la luna"; }

 protected:
  float mx_ = 0, my_ = 0, mz_ = 0;  // the moon; mz_ > 0 in front
  void setup(float t) override {
    const float a = t * 0.9f;
    mx_ = 7.5f + 6.3f * cosf(a);
    my_ = 8 + 6.3f * sinf(a) * 0.3f;
    mz_ = sinf(a);
  }
  float at(float x, float y, float t) const override {
    const float cx = 7.5f, cy = 8, R = 3.4f, tilt = 0.3f;
    const float dx = x - cx, dy = y - cy;
    const float rr = hypotf(dx, dy / tilt);
    const bool planet = dx * dx + dy * dy < R * R;
    const bool moon = (x - mx_) * (x - mx_) + (y - my_) * (y - my_) < 1.1f * 1.1f;
    if (moon && (mz_ > 0 || !planet)) return 0.9f;
    if (rr > 4.6f && rr < 6.6f && (dy > 0 || !planet)) return 0.55f;
    if (planet) {
      const float z = sqrtf(max(0.0f, R * R - dx * dx - dy * dy));
      const float lit = max(0.0f, (dx * 0.9f + z * 0.45f) / R);  // the sun on the right
      const float band = 0.7f + 0.3f * sinf(dy * 1.7f);
      return 0.04f + 0.85f * lit * band;
    }
    return stars(x, y, t);
  }
};

// ---------------------------------------------------------------------------
class SaturnClose : public Saturn {
 public:
  const char *id() const override { return "saturnclose"; }
  const char *name() const override { return "Saturno da vicino"; }

 protected:
  float rot_ = 0.55f;
  void setup(float t) override { rot_ = 0.55f + 0.05f * sinf(t * 0.3f); }
  float at(float x, float y, float t) const override {
    const float cx = 3.5f, cy = 13.5f, R = 6.5f, tilt = 0.32f;
    const float dx = x - cx, dy = y - cy;
    const float u = dx * cosf(rot_) - dy * sinf(rot_), v = dx * sinf(rot_) + dy * cosf(rot_);
    const float rr = hypotf(u, v / tilt);
    const bool planet = dx * dx + dy * dy < R * R;
    if (rr > 8 && rr < 11.5f && (v > 0 || !planet)) return 0.55f * (0.6f + 0.4f * sinf(rr * 3.2f - t * 0.6f));
    if (planet) {
      const float z = sqrtf(max(0.0f, R * R - dx * dx - dy * dy));
      const float lit = max(0.0f, (dx * 0.5f - dy * 0.5f + z * 0.6f) / R);
      const float band = 0.7f + 0.3f * sinf((dy * 0.9f - dx * 0.4f) * 1.4f + t * 0.2f);
      return 0.04f + 0.8f * lit * band;
    }
    return stars(x, y, t);
  }
};

SaturnClassic saturn;
SaturnMoon saturnMoon;
SaturnClose saturnClose;

}  // namespace

extern Animation *const planetAnimation = &saturn;
extern Animation *const saturnMoonAnimation = &saturnMoon;
extern Animation *const saturnCloseAnimation = &saturnClose;
