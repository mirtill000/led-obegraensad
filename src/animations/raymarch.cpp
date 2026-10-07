// Raymarched renders: scenes described by signed distance functions (how
// far a point is from the nearest surface) and drawn by walking each ray
// in steps as long as that distance. One engine (Raymarch) does the rays
// (one per pixel corner, shared), the normals and a soft shadow; each scene
// only says its shape, its light and where the camera is:
//  - Metaball 3D: three spheres that melt into each other as they orbit.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "ui.h"

namespace {

struct V3 {
  float x, y, z;
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float len(V3 a) { return sqrtf(dot(a, a)); }
inline V3 norm(V3 a) { return a * (1 / len(a)); }
inline float clamp01(float v) { return constrain(v, 0.0f, 1.0f); }
inline float mixf(float a, float b, float k) { return a + (b - a) * k; }
// Rotation around Y, then X.
inline V3 rotY(V3 p, float a) { return {p.x * cosf(a) + p.z * sinf(a), p.y, -p.x * sinf(a) + p.z * cosf(a)}; }
inline V3 rotX(V3 p, float a) { return {p.x, p.y * cosf(a) - p.z * sinf(a), p.y * sinf(a) + p.z * cosf(a)}; }
// Smooth minimum: two shapes blending into one where they meet.
inline float smin(float a, float b, float k) {
  const float h = clamp01(0.5f + 0.5f * (b - a) / k);
  return mixf(b, a, h) - k * h * (1 - h);
}

}  // namespace

class Raymarch : public Animation {
 public:
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    t_ = now / 1000.0f;
    setup(t_);
    // One ray through every pixel corner (17 x 17); a pixel is the mean of
    // its four corners - anti-aliased edges for a quarter of the rays of
    // 2x2 samples per pixel.
    float corner[ROWS + 1][COLS + 1];
    for (int y = 0; y <= ROWS; y++) {
      for (int x = 0; x <= COLS; x++) corner[y][x] = shadeRay((x - 8) / 8.0f, -(y - 8) / 8.0f);
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const float sum = corner[y][x] + corner[y][x + 1] + corner[y + 1][x] + corner[y + 1][x + 1];
        display.setLevel(x, y, ui::tone(sum / 4));
      }
    }
  }

 protected:
  struct Hit {
    V3 p, n;     // where, and the surface's normal
    float dist;  // along the ray
    int id;      // which part of the scene (from sdf())
  };
  // Called once per frame: camera, moving parts.
  virtual void setup(float t) = 0;
  // Distance from p to the nearest surface; *id says which one.
  virtual float sdf(V3 p, int *id) const = 0;
  // The brightness of a hit (0..1).
  virtual float light(const Hit &h, V3 dir) const = 0;
  // Where a ray hits nothing.
  virtual float background(V3 dir, float u, float v) const { return 0; }
  virtual float farPlane() const { return 20; }
  virtual int maxSteps() const { return 40; }

  // Soft shadow towards `l` from p: 1 lit, 0 in full shadow (the nearer a
  // ray passes to something, the darker).
  float shadow(V3 p, V3 l, float maxDist) const {
    float k = 1, d = 0.05f;
    for (int i = 0; i < 20 && d < maxDist; i++) {
      int id;
      const float h = sdf(p + l * d, &id);
      if (h < 0.002f) return 0;
      k = min(k, 8 * h / d);
      d += constrain(h, 0.03f, 0.5f);
    }
    return clamp01(k);
  }
  V3 normalAt(V3 p) const {
    int id;
    const float e = 0.01f;
    return norm({sdf({p.x + e, p.y, p.z}, &id) - sdf({p.x - e, p.y, p.z}, &id),
                 sdf({p.x, p.y + e, p.z}, &id) - sdf({p.x, p.y - e, p.z}, &id),
                 sdf({p.x, p.y, p.z + e}, &id) - sdf({p.x, p.y, p.z - e}, &id)});
  }

  V3 eye_ = {0, 0, -4}, fwd_ = {0, 0, 1}, right_ = {1, 0, 0}, up_ = {0, 1, 0};
  float zoom_ = 1.3f;
  float t_ = 0;

  void lookAt(V3 eye, V3 target) {
    eye_ = eye;
    fwd_ = norm(target - eye);
    right_ = norm({fwd_.z, 0, -fwd_.x});
    up_ = {fwd_.y * right_.z - fwd_.z * right_.y, fwd_.z * right_.x - fwd_.x * right_.z,
           fwd_.x * right_.y - fwd_.y * right_.x};  // fwd x right
  }

 private:
  float shadeRay(float u, float v) const {
    const V3 dir = norm(fwd_ * zoom_ + right_ * u + up_ * v);
    float d = 0;
    for (int i = 0; i < maxSteps() && d < farPlane(); i++) {
      int id = 0;
      const V3 p = eye_ + dir * d;
      const float h = sdf(p, &id);
      if (h < 0.003f * (1 + d)) {
        Hit hit{p, normalAt(p), d, id};
        return light(hit, dir);
      }
      d += h;
    }
    return background(dir, u, v);
  }
};

// ---------------------------------------------------------------------------
class MetaballRender : public Raymarch {
 public:
  const char *id() const override { return "metaball3d"; }
  const char *name() const override { return "Metaball 3D"; }

 protected:
  V3 c_[3];
  void setup(float t) override {
    for (int i = 0; i < 3; i++) {
      const float a = t * (0.6f + 0.17f * i) + i * 2.1f;
      c_[i] = {1.1f * cosf(a), 0.7f * sinf(a * 1.3f + i), 0.9f * sinf(a)};
    }
    lookAt({0, 0.4f, -3.3f}, {0, 0, 0});
    zoom_ = 1.5f;
  }
  float sdf(V3 p, int *id) const override {
    *id = 0;
    float d = len(p - c_[0]) - 0.75f;
    d = smin(d, len(p - c_[1]) - 0.6f, 0.6f);
    return smin(d, len(p - c_[2]) - 0.5f, 0.6f);
  }
  float light(const Hit &h, V3 dir) const override {
    const V3 l = norm({0.6f, 0.7f, -0.5f});
    const float diff = max(0.0f, dot(h.n, l));
    const float rim = powf(1 - max(0.0f, -dot(dir, h.n)), 3);  // a glow along the edges
    const V3 r = dir - h.n * (2 * dot(dir, h.n));
    return 0.1f + 0.55f * diff + 0.35f * rim + 0.35f * powf(max(0.0f, dot(r, l)), 20);
  }
};

static MetaballRender metaball3d;
extern Animation *const metaball3dAnimation = &metaball3d;
