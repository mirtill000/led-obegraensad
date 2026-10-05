// Raymarched renders: scenes described by signed distance functions (how
// far a point is from the nearest surface) and drawn by walking each ray
// in steps as long as that distance. One engine (Raymarch) does the rays
// (one per pixel corner, shared), the normals and a soft shadow; each scene
// only says its shape, its light and where the camera is:
//  - Metaball 3D: three spheres that melt into each other as they orbit;
//  - Colonne infinite: a hall of pillars without end, flown through;
//  - Pianeta con anelli: a planet with its rings, lit from the side, the
//    rings' shadow on it and its shadow on the rings, among stars.
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
float hash01(int a, int b) {
  uint32_t h = (uint32_t)a * 0x8da6b343u ^ (uint32_t)b * 0xd8163841u;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  return (h >> 8 & 0xFFFF) / 65535.0f;
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

// ---------------------------------------------------------------------------
class PillarsRender : public Raymarch {
 public:
  const char *id() const override { return "pillars"; }
  const char *name() const override { return "Colonne infinite"; }

 protected:
  float farPlane() const override { return 18; }
  int maxSteps() const override { return 48; }
  void setup(float t) override {
    const float z = t * 0.8f;
    const float x = 2 + 0.9f * sinf(t * 0.3f);  // weaving between the rows
    lookAt({x, 0.2f + 0.3f * sinf(t * 0.21f), z}, {x + 1.4f * sinf(t * 0.17f), 0, z + 4});
  }
  float sdf(V3 p, int *id) const override {
    // Pillars every 4 units both ways (the space repeated), floor and ceiling.
    const float cx = p.x - 4 * floorf(p.x / 4 + 0.5f), cz = p.z - 4 * floorf(p.z / 4 + 0.5f);
    const float pillar = sqrtf(cx * cx + cz * cz) - 0.45f;
    const float slab = 1.6f - fabsf(p.y);
    *id = pillar < slab ? 0 : 1;
    return min(pillar, slab);
  }
  float light(const Hit &h, V3 dir) const override {
    const float fog = clamp01(1 - h.dist / 16);
    if (h.id == 1) {
      // Floor and ceiling: tiles drawn by the lines between them.
      const float gx = fabsf(h.p.x - 2 * floorf(h.p.x / 2 + 0.5f)), gz = fabsf(h.p.z - 2 * floorf(h.p.z / 2 + 0.5f));
      const float line = min(gx, gz) < 0.06f ? 0.25f : 0;
      return (0.06f + line) * fog * fog;
    }
    // Pillars lit from the camera, with rings around them.
    const float face = max(0.0f, -dot(dir, h.n));
    const float ring = fabsf(fmodf(h.p.y + 4, 0.8f) - 0.4f) < 0.05f ? -0.2f : 0;
    return (0.1f + 0.75f * face * face + ring) * fog;
  }
};

// ---------------------------------------------------------------------------
class PlanetRender : public Raymarch {
 public:
  const char *id() const override { return "planet"; }
  const char *name() const override { return "Pianeta con anelli"; }

 protected:
  float spin_ = 0;
  void setup(float t) override {
    spin_ = t * 0.25f;
    const float a = 0.35f + 0.25f * sinf(t * 0.07f);
    lookAt({4.8f * sinf(t * 0.05f), 4.8f * sinf(a), -4.8f * cosf(t * 0.05f) * cosf(a)}, {0, 0, 0});
    zoom_ = 1.5f;
  }
  // The planet tilted 25 degrees; the rings a thin flat torus around it.
  static V3 tilt(V3 p) { return rotX(p, 0.44f); }
  float sdf(V3 p, int *id) const override {
    const float planet = len(p) - 1.0f;
    const V3 q = tilt(p);
    const float r = sqrtf(q.x * q.x + q.z * q.z);
    const float band = max(fabsf(r - 1.75f) - 0.45f, fabsf(q.y) - 0.015f);  // 1.3 .. 2.2 wide, paper thin
    *id = planet < band ? 0 : 1;
    return min(planet, band);
  }
  float light(const Hit &h, V3 dir) const override {
    const V3 l = norm({1, 0.25f, -0.3f});  // the sun, from the side
    if (h.id == 0) {
      // Bands of cloud across the planet, turning; the rings' shadow.
      const V3 q = tilt(h.p);
      const float lat = q.y, lon = atan2f(q.z, q.x) + spin_;
      const float bands = 0.75f + 0.25f * sinf(lat * 9 + 0.6f * sinf(lon * 2));
      const float diff = max(0.0f, dot(h.n, l)) * shadow(h.p + h.n * 0.02f, l, 4);
      return (0.04f + 0.85f * diff) * bands;
    }
    // The rings: grooves, lit on both faces, dark where the planet hides the sun.
    const V3 q = tilt(h.p);
    const float r = sqrtf(q.x * q.x + q.z * q.z);
    const float grooves = 0.55f + 0.45f * sinf(r * 22) * sinf(r * 7);
    const float gap = fabsf(r - 1.85f) < 0.07f ? 0.2f : 1;  // a division
    const float lit = shadow(h.p + l * 0.03f, l, 4);
    return (0.06f + 0.6f * lit) * grooves * gap;
  }
  float background(V3 dir, float u, float v) const override {
    // Fixed stars, twinkling a little.
    const int sx = (int)floorf((atan2f(dir.x, dir.z) + 4) * 9), sy = (int)floorf((dir.y + 2) * 9);
    const float r = hash01(sx, sy);
    return r < 0.05f ? 0.25f + 0.15f * sinf(t_ * 2 + r * 100) : 0;
  }
};

static MetaballRender metaball3d;
static PillarsRender pillars;
static PlanetRender planet;
extern Animation *const metaball3dAnimation = &metaball3d;
extern Animation *const pillarsAnimation = &pillars;
extern Animation *const planetAnimation = &planet;
