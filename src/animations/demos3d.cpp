// 3D effects rendered per pixel, with 2x2 supersampling where edges matter:
// a demoscene tunnel, a solid shaded cube, a sphere lit by the real sun and
// a voxel landscape to fly over.
#include <math.h>
#include <time.h>

#include "animation.h"
#include "display.h"
#include "moon.h"
#include "settings.h"
#include "timekeeping.h"

namespace {

uint8_t toLevel(float v) { return (uint8_t)(constrain(v, 0.0f, 1.0f) * 255); }

// Rotates (x, y, z) around X, then Y.
void rotate(float &x, float &y, float &z, float ax, float ay) {
  float t = y * cosf(ax) - z * sinf(ax);
  z = y * sinf(ax) + z * cosf(ax);
  y = t;
  t = x * cosf(ay) + z * sinf(ay);
  z = -x * sinf(ay) + z * cosf(ay);
  x = t;
}

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
// Solid cube: a ray per sub-pixel against the rotating cube (slab test in
// the cube's own frame), faces shaded by how squarely they face the light.
class SolidCubeAnimation : public Animation {
 public:
  const char *id() const override { return "solidcube"; }
  const char *name() const override { return "Cubo solido"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 40; }
  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    const float ax = t * 0.8f, ay = t * 0.55f;
    // Towards the light (upper left, in front), in world space; y is down.
    const float lx = -0.5f, ly = -0.6f, lz = -0.62f;
    // Camera at z = -4.2 looking along +z; the cube (side 2) spins at the origin.
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        float sum = 0;
        for (int s = 0; s < 4; s++) {
          float dx = (x + 0.25f + 0.5f * (s & 1) - 8) / 21.0f, dy = (y + 0.25f + 0.5f * (s >> 1) - 8) / 21.0f, dz = 1;
          // Into the cube's frame: undo its rotation (Y then X).
          float px = 0, py = 0, pz = -4.2f;
          inverse(px, py, pz, ax, ay);
          inverse(dx, dy, dz, ax, ay);
          float tNear = -1e9f, tFar = 1e9f;
          int axis = -1;
          float sign = 0;
          const float p[3] = {px, py, pz}, d[3] = {dx, dy, dz};
          bool miss = false;
          for (int a = 0; a < 3 && !miss; a++) {
            if (fabsf(d[a]) < 1e-6f) {
              if (fabsf(p[a]) > 1) miss = true;
              continue;
            }
            float t1 = (-1 - p[a]) / d[a], t2 = (1 - p[a]) / d[a];
            float s1 = -1;
            if (t1 > t2) {
              const float tmp = t1;
              t1 = t2;
              t2 = tmp;
              s1 = 1;
            }
            if (t1 > tNear) {
              tNear = t1;
              axis = a;
              sign = s1;
            }
            tFar = min(tFar, t2);
            if (tNear > tFar) miss = true;
          }
          if (miss || axis < 0 || tFar < 0) continue;
          // The face normal, back to world space.
          float nx = axis == 0 ? sign : 0, ny = axis == 1 ? sign : 0, nz = axis == 2 ? sign : 0;
          forward(nx, ny, nz, ax, ay);
          const float diffuse = max(0.0f, nx * lx + ny * ly + nz * lz);
          sum += 0.12f + 0.88f * diffuse;
        }
        display.setLevel(x, y, toLevel(sum / 4));
      }
    }
  }

 private:
  // Cube frame -> world: around X by ax, then Y by ay.
  static void forward(float &x, float &y, float &z, float ax, float ay) { rotate(x, y, z, ax, ay); }
  // World -> cube frame: Y by -ay, then X by -ax.
  static void inverse(float &x, float &y, float &z, float ax, float ay) {
    float t = x * cosf(-ay) + z * sinf(-ay);
    z = -x * sinf(-ay) + z * cosf(-ay);
    x = t;
    t = y * cosf(-ax) - z * sinf(-ax);
    z = y * sinf(-ax) + z * cosf(-ax);
    y = t;
  }
};

// ---------------------------------------------------------------------------
// Sunlit sphere: a slowly turning globe with faint meridians and
// parallels, lit from where the sun really is in your sky (settings'
// latitude and longitude), as if you looked north: sunrise lights its right
// side, noon its front and top, sunset its left; at night only a thin rim
// of twilight and a few stars. Without the clock the sun circles quickly.
class SunSphereAnimation : public Animation {
 public:
  const char *id() const override { return "sunsphere"; }
  const char *name() const override { return "Sfera al sole"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 60; }
  void frame(uint32_t now) override {
    float az, el;
    struct tm tm;
    if (localTime(tm)) {
      sunPosition(time(nullptr), settings.latitude, settings.longitude, az, el);
    } else {
      az = fmodf(now / 4000.0f, 2 * (float)M_PI);  // a day in 25 seconds
      el = 0.8f * sinf(az - (float)M_PI / 2);
    }
    // Light direction in view space: x right (east), y up, z towards the viewer (south).
    const float lx = cosf(el) * sinf(az), ly = sinf(el), lz = -cosf(el) * cosf(az);
    const float spin = now / 6000.0f;
    const float R = 6.6f;
    display.clear();
    if (el < 0) {
      // Night: a few stars around the globe, twinkling.
      static const uint8_t STARS[][2] = {{1, 1}, {14, 2}, {2, 13}, {13, 14}, {0, 7}, {15, 9}};
      for (int i = 0; i < 6; i++) {
        display.setLevel(STARS[i][0], STARS[i][1], (uint8_t)(40 + 30 * sinf(now / 700.0f + i * 1.7f)));
      }
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        float sum = 0;
        for (int s = 0; s < 4; s++) {
          const float sx = (x + 0.25f + 0.5f * (s & 1) - 8) / R, sy = -(y + 0.25f + 0.5f * (s >> 1) - 8) / R;
          const float d2 = sx * sx + sy * sy;
          if (d2 > 1) continue;
          const float sz = sqrtf(1 - d2);  // the visible hemisphere faces +z
          const float light = sx * lx + sy * ly + sz * lz;
          // Soft terminator; a trace of twilight past it.
          float shade = light > 0 ? 0.15f + 0.85f * light : max(0.0f, 0.15f + light * 0.6f);
          if (el < 0) shade = 0.06f + shade * 0.35f;  // night: just the outline
          // Meridians and parallels on the turning globe.
          const float lon = atan2f(sx, sz) + spin, lat = asinf(sy);
          const float grid = min(fabsf(sinf(lon * 3)), fabsf(sinf(lat * 3)));
          if (grid < 0.12f) shade *= 0.55f;
          sum += shade;
        }
        if (sum > 0) display.setLevel(x, y, toLevel(sum / 4));
      }
    }
  }
};

// ---------------------------------------------------------------------------
// Voxel landscape (the Comanche way): a heightmap of hills and valleys,
// drawn column by column front to back, each ray raising the "horizon"
// it may still paint above. Brighter peaks, fog far away, a gentle bank
// as the flight curves.
class VoxelAnimation : public Animation {
 public:
  const char *id() const override { return "voxel"; }
  const char *name() const override { return "Volo sulle colline"; }
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 30; }
  void start() override { camH_ = -1; }
  void frame(uint32_t now) override {
    // A slow, gliding flight: a gentle speed, wide lazy turns, and a
    // camera height that eases towards the hills ahead instead of
    // following every bump below.
    const float t = now / 1000.0f;
    const float heading = 0.35f * sinf(t * 0.06f);
    const float camX = 30 * sinf(t * 0.021f) + 8 * sinf(t * 0.047f), camY = t * 2.4f;
    float ahead = 0;
    for (float d = 0; d <= 20; d += 2.5f) {
      ahead = max(ahead, height(camX + sinf(heading) * d, camY + cosf(heading) * d));
    }
    const float target = ahead + 5.5f;
    camH_ = camH_ < 0 ? target : camH_ + (target - camH_) * 0.02f;
    camH_ = max(camH_, height(camX, camY) + 3.0f);  // never skim the grass
    const float horizon = 3.0f;
    display.clear();
    for (int x = 0; x < COLS; x++) {
      // Sky: a pale gradient.
      float column[ROWS];
      for (int y = 0; y < ROWS; y++) column[y] = max(0.0f, 0.12f - y * 0.02f);
      const float angle = heading + (x - 7.5f) * 0.06f;
      const float sa = sinf(angle), ca = cosf(angle);
      float top = ROWS;  // lowest painted edge so far, with its fraction
      float lastH = height(camX, camY);
      for (float dist = 1.2f; dist < 70 && top > 0; dist += 0.25f + dist * 0.02f) {
        const float h = height(camX + sa * dist, camY + ca * dist);
        const float screen = max(0.0f, (camH_ - h) / dist * 14.0f + horizon);
        if (screen >= top) {
          lastH = h;
          continue;
        }
        const float fog = 1 - dist / 80;
        // Higher is brighter, and slopes rising towards you catch the light.
        const float rise = constrain((h - lastH) * 0.5f, -0.3f, 0.4f);
        lastH = h;
        const float light = constrain((0.2f + 0.8f * sqrtf(h / 16.0f) + rise) * fog, 0.0f, 1.0f);
        // Fill from the new edge down to the old one; the pixel the edge
        // falls in gets its share, so ridges glide instead of jumping by rows.
        const int from = (int)screen, to = min(ROWS, (int)ceilf(top));
        for (int y = from; y < to; y++) {
          const float cover = min((float)y + 1, top) - max((float)y, screen);  // 0..1 of this row
          column[y] = column[y] * (1 - cover) + light * cover;
        }
        top = screen;
      }
      for (int y = 0; y < ROWS; y++) display.setLevel(x, y, toLevel(column[y]));
    }
  }

 private:
  // Rolling hills: a few sine waves at odd angles, peaks sharpened.
  static float height(float x, float y) {
    const float a = sinf(x * 0.11f) * cosf(y * 0.09f);
    const float b = sinf((x + y) * 0.05f + 1.3f);
    const float c = sinf(x * 0.23f - y * 0.17f) * 0.35f;
    const float h = (a + b + c + 2.35f) / 4.7f;  // 0..1
    return h * h * 16;
  }

  float camH_ = -1;
};

static TunnelAnimation tunnel;
static SolidCubeAnimation solidCube;
static SunSphereAnimation sunSphere;
static VoxelAnimation voxel;
extern Animation *const tunnelAnimation = &tunnel;
extern Animation *const solidCubeAnimation = &solidCube;
extern Animation *const sunSphereAnimation = &sunSphere;
extern Animation *const voxelAnimation = &voxel;
