// 3D effects rendered per pixel, supersampled where edges matter:
// a demoscene tunnel and a solid shaded cube.
// The voxel flights are in flights.cpp.
#include <math.h>

#include "animation.h"
#include "display.h"
#include "ui.h"

namespace {

uint8_t toLevel(float v) { return (uint8_t)(constrain(v, 0.0f, 1.0f) * 255); }

}  // namespace

// ---------------------------------------------------------------------------
// Tunnel: every pixel looks up a tiled texture by its angle and inverse
// distance from a wandering centre, so you fly down a twisting pipe; the
// far end fades into the dark.
class TunnelAnimation : public Animation {
 public:
  const char *id() const override { return "tunnel"; }
  const char *name() const override { return "Tunnel"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    const float cx = 7.5f + 3.0f * sinf(t * 0.7f), cy = 7.5f + 3.0f * cosf(t * 0.53f);
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        float sum = 0;
        for (int s = 0; s < 4; s++) {
          const float dx = x + 0.25f + 0.5f * (s & 1) - cx, dy = y + 0.25f + 0.5f * (s >> 1) - cy;
          const float r = sqrtf(dx * dx + dy * dy) + 0.3f;
          const float u = 7.0f / r + t * 2.5f;                           // depth, towards you
          const float v = atan2f(dy, dx) / (float)M_PI * 3 + t * 0.4f;  // around, slowly turning
          // Soft tiles (a smooth checker), which stays readable at 16x16.
          const float tile = sinf(u * (float)M_PI) * sinf(v * (float)M_PI);
          const float fog = constrain((r - 0.8f) / 7.0f, 0.0f, 1.0f);  // the far end is dark
          sum += (0.55f + 0.45f * tile) * fog;
        }
        display.setLevel(x, y, toLevel(sum / 4));
      }
    }
  }
};

// ---------------------------------------------------------------------------
// Solid cube: 16 rays per pixel against the rotating cube (slab test in the
// cube's own frame), faces shaded by how squarely they face the light. The
// rotation is worked out once per frame, which leaves time for a high frame
// rate and smooth, anti-aliased edges; the spin speeds up and slows down
// gently instead of tumbling at a fixed rate.
class SolidCubeAnimation : public Animation {
 public:
  const char *id() const override { return "solidcube"; }
  const char *name() const override { return "Cubo solido"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 20; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    // Angles whose speed drifts slowly (their derivatives stay smooth).
    const float ax = t * 0.45f + 0.6f * sinf(t * 0.13f), ay = t * 0.32f + 0.8f * sinf(t * 0.09f + 1);
    // R = rotation around Y by ay after X by ax (cube frame -> world).
    const float cx = cosf(ax), sx = sinf(ax), cy = cosf(ay), sy = sinf(ay);
    const float R[3][3] = {{cy, sy * sx, sy * cx}, {0, cx, -sx}, {-sy, cy * sx, cy * cx}};
    // Towards the light (upper left, in front), in world space; y is down.
    const float L[3] = {-0.5f, -0.6f, -0.62f};
    // How bright each face (+x, -x, +y, -y, +z, -z) is this frame.
    float faceLight[6];
    for (int a = 0; a < 3; a++) {
      const float d = R[0][a] * L[0] + R[1][a] * L[1] + R[2][a] * L[2];  // normal (column a) . light
      faceLight[a * 2] = 0.15f + 0.85f * max(0.0f, d);
      faceLight[a * 2 + 1] = 0.15f + 0.85f * max(0.0f, -d);
    }
    // The camera at z = -5 looking along +z (the cube just fits the panel),
    // in the cube's frame (R^T).
    const float o[3] = {R[2][0] * -5.0f, R[2][1] * -5.0f, R[2][2] * -5.0f};
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        float sum = 0;
        for (int s = 0; s < 16; s++) {
          const float wx = (x + 0.125f + 0.25f * (s & 3) - 8) / 17.0f, wy = (y + 0.125f + 0.25f * (s >> 2) - 8) / 17.0f;
          const float d[3] = {R[0][0] * wx + R[1][0] * wy + R[2][0], R[0][1] * wx + R[1][1] * wy + R[2][1],
                              R[0][2] * wx + R[1][2] * wy + R[2][2]};
          float tNear = -1e9f, tFar = 1e9f;
          int face = -1;
          bool miss = false;
          for (int a = 0; a < 3 && !miss; a++) {
            if (fabsf(d[a]) < 1e-6f) {
              if (fabsf(o[a]) > 1) miss = true;
              continue;
            }
            float t1 = (-1 - o[a]) / d[a], t2 = (1 - o[a]) / d[a];
            int f = a * 2 + 1;  // entering through the -a face
            if (t1 > t2) {
              const float tmp = t1;
              t1 = t2;
              t2 = tmp;
              f = a * 2;
            }
            if (t1 > tNear) {
              tNear = t1;
              face = f;
            }
            tFar = min(tFar, t2);
            if (tNear > tFar) miss = true;
          }
          if (!miss && face >= 0 && tFar > 0) sum += faceLight[face];
        }
        display.setLevel(x, y, ui::tone(sum / 16));
      }
    }
  }
};

static TunnelAnimation tunnel;
static SolidCubeAnimation solidCube;
extern Animation *const tunnelAnimation = &tunnel;
extern Animation *const solidCubeAnimation = &solidCube;
