// Flights over voxel landscapes, the Comanche way: one ray per column,
// walked front to back over a heightmap; every sample that rises above
// what the column has painted so far fills the rows up to it. One engine
// (Flight) does the rays, the sub-pixel ridges, the gliding camera and the
// fog; each flight only says what the ground looks like and how it is lit:
//  - Volo sulle colline: rolling hills, brighter with height;
//  - Volo sul mare: moving waves, the real sun (or moon) glittering on them;
//  - Nel canyon: along a winding river between layered rock walls;
//  - Città di notte: blocks of buildings, lit windows, red beacons;
//  - Sopra le nuvole: snowy peaks and a drifting cloud layer to dive into.
#include <math.h>
#include <time.h>

#include "animation.h"
#include "display.h"
#include "moon.h"
#include "settings.h"
#include "timekeeping.h"
#include "ui.h"

namespace {

float clamp01(float v) { return constrain(v, 0.0f, 1.0f); }
float smooth(float a, float b, float x) {
  const float k = clamp01((x - a) / (b - a));
  return k * k * (3 - 2 * k);
}
float mix(float a, float b, float k) { return a + (b - a) * k; }
// An angle folded into -pi..pi.
float wrapAngle(float a) {
  a = fmodf(a + (float)M_PI, 2 * (float)M_PI);
  return a < 0 ? a + (float)M_PI : a - (float)M_PI;
}
// 0..1, the same for the same three numbers.
float random01(int32_t a, int32_t b, int32_t c) {
  uint32_t h = (uint32_t)a * 0x8da6b343u ^ (uint32_t)b * 0xd8163841u ^ (uint32_t)c * 0xcb1ab31fu;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return (h >> 8) / 16777216.0f;
}

const float SCALE = 14.0f;     // screen rows per unit of height at distance 1
const float FAN = 0.06f;       // radians between two columns
const float FAR = 70.0f;       // how far the rays go
const float INF = 1e9f;        // the distance of the sky

// A star field fixed to the world (by the column's direction).
float stars(float angle, int y, float t) {
  const int32_t col = (int32_t)floorf(angle / FAN);
  const float r = random01(col, y, 7);
  if (r > 0.07f) return 0;
  return 0.18f + 0.12f * sinf(t * 1.7f + r * 90);
}

}  // namespace

class Flight : public Animation {
 public:
  const char *group() const override { return "3D e demo"; }
  uint16_t frameMs() const override { return 30; }
  void start() override { camH_ = -1; }
  void frame(uint32_t now) override {
    View v;
    v.t = now / 1000.0f;
    path(v);
    const float target = altitude(v);
    // Eases by ease() every 30 ms of the clock, however often frames come.
    const float dt = camH_ < 0 ? 0 : constrain((now - lastNow_) / 30.0f, 0.0f, 10.0f);
    lastNow_ = now;
    camH_ = camH_ < 0 ? target : camH_ + (target - camH_) * (1 - powf(1 - ease(), dt));
    camH_ = max(camH_, height(v.x, v.y, v.t) + clearance());  // never skim the ground
    v.h = camH_;
    v.horizon = horizonRow(v.t);
    display.clear();
    const int rays = raysPerColumn();
    for (int x = 0; x < COLS; x++) {
      // Sharp-edged worlds (buildings, ridges) cast several rays per column
      // and average them, so edges slide sideways instead of popping.
      float sum[ROWS] = {0};
      for (int r = 0; r < rays; r++) {
        float column[ROWS];
        castRay(v, v.heading + (x - 7.5f + (r + 0.5f) / rays - 0.5f) * FAN, column);
        for (int y = 0; y < ROWS; y++) sum[y] += column[y];
      }
      for (int y = 0; y < ROWS; y++) display.setLevel(x, y, ui::tone(sum[y] / rays));
    }
  }

 protected:
  struct View {
    float t;          // seconds
    float x, y;       // the camera on the map
    float heading;    // where it looks: radians from +y (north) towards +x (east)
    float h;          // its altitude
    float horizon;    // the screen row of the horizon
  };
  struct Hit {
    float x, y, h;    // the ground the ray met
    float rise;       // -0.3..0.4, how much it climbs towards the viewer
    float dist, angle;
  };

  // Where the camera is and where it looks, at v.t (sets x, y, heading).
  virtual void path(View &v) = 0;
  virtual float height(float x, float y, float t) const = 0;
  // The brightness (0..1, before fog) of the ground hit, for screen row y
  // whose ray meets it at height rowH (facades, rock strata...).
  virtual float shade(const Hit &hit, float rowH, int y, const View &v) const = 0;
  // The brightness the distance fades to (and the sky just above the horizon).
  virtual float haze() const { return 0.12f; }
  // How much the outline of a hill against what is behind it lights up.
  virtual float crest() const { return 0.45f; }
  // How much darker each hill gets below its ridge (0 none).
  virtual float falloff() const { return 0.0f; }
  // The sky: dark, lighter towards the horizon.
  virtual float sky(int y, float, const View &v) const {
    return haze() * clamp01(1 - (v.horizon - y - 0.5f) / 4);
  }
  // After a column is drawn: depth[y] is how far the ground of each row is
  // (INF for sky). For see-through layers drawn in front of the ground.
  virtual void overlay(float *, const float *, float, float, const View &) const {}
  // The altitude the camera eases towards: above the highest ground ahead.
  virtual float altitude(const View &v) const {
    float ahead = 0;
    for (float d = 0; d <= 20; d += 2.5f) {
      ahead = max(ahead, height(v.x + sinf(v.heading) * d, v.y + cosf(v.heading) * d, v.t));
    }
    return ahead + 5.5f;
  }
  virtual float clearance() const { return 3.0f; }  // the least height above the ground below
  virtual float ease() const { return 0.02f; }      // per 30 ms, towards altitude()
  virtual float horizonRow(float) const { return 3.0f; }
  virtual int raysPerColumn() const { return 1; }

 private:
  // One ray: fills column[] with the ground (or sky) it sees.
  void castRay(const View &v, float angle, float *column) {
    const float sa = sinf(angle), ca = cosf(angle);
    float depth[ROWS];
    int8_t hill[ROWS];  // which hill (counted front to back) each row shows, -1 sky
    for (int y = 0; y < ROWS; y++) {
      column[y] = sky(y, angle, v);
      depth[y] = INF;
      hill[y] = -1;
    }
    int8_t hills = 0;
    float top = ROWS;  // the lowest edge painted so far, with its fraction
    float edgeDist = 0;  // how far the ground just below `top` is
    float lastH = height(v.x, v.y, v.t);
    for (float dist = 1.2f; dist < FAR && top > 0; dist += 0.25f + dist * 0.02f) {
      Hit hit;
      hit.x = v.x + sa * dist;
      hit.y = v.y + ca * dist;
      hit.h = height(hit.x, hit.y, v.t);
      const float screen = max(0.0f, (v.h - hit.h) / dist * SCALE + v.horizon);
      if (screen >= top) {
        lastH = hit.h;
        continue;
      }
      // Positive: a slope rising towards you (it catches the light).
      hit.rise = constrain((hit.h - lastH) * 0.5f, -0.3f, 0.4f);
      hit.dist = dist;
      hit.angle = angle;
      lastH = hit.h;
      // Far away the ground melts into the haze of the horizon (not into
      // black: the panel can't show the shades just above it).
      const float fog = 1 - dist / (FAR + 10);
      // A hill now seen behind a nearer one: the nearer one's ridge lights up.
      if (top < ROWS && dist > edgeDist * 1.15f + 0.6f) {
        lightCrest(column, top, edgeDist);
        if (hills < 100) hills++;
      }
      // Fill from the new edge down to the old one; the row the edge
      // falls in gets its share, so ridges glide instead of jumping rows.
      const int from = (int)screen, to = min(ROWS, (int)ceilf(top));
      for (int y = from; y < to; y++) {
        const float cover = min((float)y + 1, top) - max((float)y, screen);  // 0..1 of this row
        const float rowH = v.h - (y + 0.5f - v.horizon) * dist / SCALE;    // where its ray meets
        column[y] = mix(column[y], mix(haze(), clamp01(shade(hit, rowH, y, v)), fog), cover);
        if (cover > 0.5f) {
          depth[y] = dist;
          hill[y] = hills;
        }
      }
      top = screen;
      edgeDist = dist;
    }
    // Each hill darker further down from its ridge (lit from above), so one
    // stands out against the next.
    if (falloff() > 0) {
      int ridge = ROWS;
      for (int y = 0; y < ROWS; y++) {
        if (hill[y] < 0) continue;
        if (y == 0 || hill[y] != hill[y - 1]) ridge = y;
        column[y] *= 1 - falloff() * (1 - expf(-(y - ridge) / 2.5f));
      }
    }
    if (top > 0 && top < ROWS) lightCrest(column, top, edgeDist);  // the skyline
    overlay(column, depth, sa, ca, v);
  }

  // Ridges are what a 16x16 panel can show best: a band one row tall just
  // below `top` (the outline of the ground at distance `dist`) brightens.
  void lightCrest(float *column, float top, float dist) {
    const float amount = crest() * (1 - dist / (FAR + 10));
    for (int y = (int)top; y < min(ROWS, (int)top + 2); y++) {
      const float k = clamp01(min((float)y + 1, top + 1) - max((float)y, top));
      column[y] = min(1.0f, column[y] + amount * k);
    }
  }

  float camH_ = -1;
  uint32_t lastNow_ = 0;
};

// ---------------------------------------------------------------------------
// Volo sulle colline: a slow glide over rolling hills; higher is brighter.
class HillsFlight : public Flight {
 public:
  const char *id() const override { return "voxel"; }
  const char *name() const override { return "Volo sulle colline"; }

 protected:
  void path(View &v) override {
    v.heading = 0.35f * sinf(v.t * 0.06f);
    v.x = 30 * sinf(v.t * 0.021f) + 8 * sinf(v.t * 0.047f);
    v.y = v.t * 2.4f;
  }
  // A few sine waves at odd angles, peaks sharpened.
  float height(float x, float y, float) const override {
    const float a = sinf(x * 0.11f) * cosf(y * 0.09f);
    const float b = sinf((x + y) * 0.05f + 1.3f);
    const float c = sinf(x * 0.23f - y * 0.17f) * 0.35f;
    const float h = (a + b + c + 2.35f) / 4.7f;  // 0..1
    return h * h * 16;
  }
  float shade(const Hit &hit, float, int, const View &) const override {
    return 0.08f + 0.35f * hit.h / 16 + hit.rise * 0.6f;
  }
  // Night-like: black sky, dark hills, their ridges drawn in light.
  float haze() const override { return 0.0f; }
  float crest() const override { return 0.85f; }
  // Low between the hills, so they stand against the sky one behind another.
  float altitude(const View &v) const override { return Flight::altitude(v) - 3.0f; }
  float horizonRow(float) const override { return 6.0f; }
  float falloff() const override { return 0.45f; }
};

// ---------------------------------------------------------------------------
// Volo sul mare: low over moving waves, towards the real sun - or, at
// night, the moon (its place worked out from the phase): a glittering
// path on the water and a glow on the horizon. Foam on the highest crests.
// Without the clock: a low morning sun.
class SeaFlight : public Flight {
 public:
  const char *id() const override { return "sea"; }
  const char *name() const override { return "Volo sul mare"; }
  void start() override {
    Flight::start();
    aim_ = INF;
  }
  void frame(uint32_t now) override {
    findLight(now);
    // Turn slowly towards the light (a full circle in the dark, gently).
    const float target = lit_ ? lightAz_ : now / 1000.0f * 0.01f;
    if (aim_ == INF) aim_ = target;
    const float dt = constrain((now - lastNow_) / 30.0f, 0.0f, 10.0f);  // 1% every 30 ms of the clock
    lastNow_ = now;
    aim_ += wrapAngle(target - aim_) * (1 - powf(0.99f, dt));
    Flight::frame(now);
  }

 protected:
  void path(View &v) override {
    v.heading = aim_ + 0.35f * sinf(v.t * 0.035f);
    v.x = 5 * sinf(v.t * 0.03f);
    v.y = v.t * 1.8f;
  }
  // A long swell and two smaller trains of waves, all running towards -y.
  float height(float x, float y, float t) const override {
    const float swell = sinf(y * 0.16f + x * 0.03f + t * 1.1f);
    const float chop = 0.55f * sinf(x * 0.31f + y * 0.22f + t * 1.7f) + 0.35f * sinf(-x * 0.43f + y * 0.5f + t * 2.3f);
    return (swell + chop + 1.9f) * 0.85f;  // 0..3.2
  }
  float altitude(const View &v) const override { return 2.2f + 0.5f * height(v.x, v.y, v.t); }  // rides the swell
  float clearance() const override { return 1.2f; }
  float ease() const override { return 0.05f; }
  float horizonRow(float t) const override { return 4.0f + 0.3f * sinf(t * 0.6f); }  // a gentle roll
  float haze() const override { return 0.1f; }
  float crest() const override { return 0.3f; }  // the lines of the waves

  float shade(const Hit &hit, float, int y, const View &v) const override {
    const float ambient = lit_ && isSun_ ? 0.55f + 0.45f * clamp01(lightEl_ * 3) : 0.4f;
    float s = (0.08f + 0.12f * hit.h / 3.2f + max(0.0f, hit.rise) * 0.5f) * ambient;
    s += smooth(2.6f, 3.1f, hit.h) * 0.5f * ambient;  // foam
    if (lit_) {
      // The glittering path: around the light's direction and around the
      // row where its reflection falls, broken up by the waves.
      const float d = wrapAngle(hit.angle - lightAz_);
      const float width = 0.07f + 0.1f * clamp01(lightEl_);
      const float reflectRow = v.horizon + tanf(max(lightEl_, 0.0f)) * SCALE;
      const float rows = (y + 0.5f - reflectRow) / 4.5f;
      const float sparkle = 0.35f + 0.65f * clamp01(0.5f + 0.5f * sinf(hit.x * 2.3f + hit.y * 1.9f + v.t * 5));
      s += lightPower_ * expf(-d * d / (width * width) - rows * rows) * sparkle * (0.6f + 0.6f * smooth(3, 30, hit.dist));
    }
    return s;
  }
  float sky(int y, float angle, const View &v) const override {
    const float above = v.horizon - (y + 0.5f);  // rows above the horizon
    // Dark, with a band of light along the horizon (by day) or stars.
    float s = lit_ && isSun_ ? 0.22f * clamp01(1 - above / 3) : stars(angle, y, v.t);
    if (lit_) {
      const float d = wrapAngle(angle - lightAz_);
      s += lightPower_ * 0.3f * expf(-d * d / 0.1f) * clamp01(1 - above / 4);  // glow on the horizon
      // The disc itself, when it is low enough to be on screen.
      const float row = v.horizon - tanf(lightEl_) * SCALE;
      s += lightPower_ * clamp01(1 - fabsf(d) / FAN) * clamp01(1.2f - fabsf(y + 0.5f - row));
    }
    return s;
  }

 private:
  void findLight(uint32_t now) {
    struct tm tm;
    if (!localTime(tm)) {
      lit_ = isSun_ = true;
      lightAz_ = 1.6f;  // east
      lightEl_ = 0.12f;
      lightPower_ = 0.9f;
      return;
    }
    const time_t when = time(nullptr);
    float az, el;
    sunPosition(when, settings.latitude, settings.longitude, az, el);
    if (el > -0.02f) {
      lit_ = isSun_ = true;
      lightAz_ = az;
      lightEl_ = el;
      lightPower_ = 0.9f;
      return;
    }
    // The moon trails the sun by its phase: about where the sun was that
    // fraction of a lunar day ago (24 h 50 min).
    const float phase = moonPhase(when);
    sunPosition(when - (time_t)(phase * 89400), settings.latitude, settings.longitude, az, el);
    isSun_ = false;
    lit_ = el > 0;
    lightAz_ = az;
    lightEl_ = el;
    lightPower_ = 0.25f + 0.5f * moonIllumination(phase);
  }

  float aim_ = INF;
  uint32_t lastNow_ = 0;
  bool lit_ = false, isSun_ = false;
  float lightAz_ = 0, lightEl_ = 0, lightPower_ = 0;
};

// ---------------------------------------------------------------------------
// Nel canyon: following a winding river, low, between rock walls with
// layers in them; the walls ahead (and the ones facing east) catch the
// light, a slit of sky above, the water shimmering below.
class CanyonFlight : public Flight {
 public:
  const char *id() const override { return "canyon"; }
  const char *name() const override { return "Nel canyon"; }

 protected:
  static float river(float y) { return 22 * sinf(y * 0.021f) + 9 * sinf(y * 0.053f + 1); }
  void path(View &v) override {
    v.y = v.t * 2.2f;
    v.x = river(v.y);
    v.heading = atan2f(river(v.y + 8) - v.x, 8);  // look along the bend ahead
  }
  float height(float x, float y, float) const override {
    const float d = fabsf(x - river(y));
    const float wall = smooth(2.2f, 9.0f, d);
    const float rough = sinf(x * 0.6f + y * 0.25f) * 0.6f + sinf(y * 0.9f - x * 0.2f) * 0.4f;
    return wall * (15 + 1.5f * rough);
  }
  float altitude(const View &v) const override { return 4.5f + 0.8f * sinf(v.t * 0.1f); }
  float clearance() const override { return 1.5f; }
  float horizonRow(float) const override { return 4.0f; }

  float shade(const Hit &hit, float rowH, int, const View &v) const override {
    if (hit.h < 0.4f) {  // the river
      return 0.1f + 0.2f * (0.5f + 0.5f * sinf(hit.y * 1.3f + hit.x - v.t * 4));
    }
    const float east = (height(hit.x + 0.5f, hit.y, 0) - height(hit.x - 0.5f, hit.y, 0)) * 0.15f;
    const float strata = 0.07f * sinf(rowH * 2.6f);
    return 0.1f + 0.15f * hit.h / 16 + hit.rise * 0.7f - constrain(east, -0.15f, 0.15f) + strata;
  }
  float haze() const override { return 0.12f; }
  float crest() const override { return 0.6f; }
  float falloff() const override { return 0.4f; }
  float sky(int y, float, const View &) const override { return max(0.15f, 0.55f - y * 0.06f); }
};

// ---------------------------------------------------------------------------
// Città di notte: down an avenue between blocks of whole floors, dark
// facades with lit windows that now and then turn on or off, a red (well,
// blinking) beacon on the towers. Every so often the flight climbs over
// the roofs and comes back down.
class CityFlight : public Flight {
 public:
  const char *id() const override { return "city"; }
  const char *name() const override { return "Città di notte"; }

 protected:
  static constexpr float BLOCK = 6.0f, STREET = 2.4f, FLOOR = 1.2f;
  void path(View &v) override {
    v.heading = 0.12f * sinf(v.t * 0.045f);
    v.x = STREET / 2;
    v.y = v.t * 2.0f;
  }
  static float building(int32_t bx, int32_t by) {
    const float r = random01(bx, by, 1);
    if (r < 0.08f) return 0;  // a square
    const float h = 2.5f + 6 * r * r + (r > 0.85f ? (r - 0.85f) * 50 : 0);  // a few towers
    return floorf(h / FLOOR) * FLOOR;
  }
  float height(float x, float y, float) const override {
    const float fx = x - floorf(x / BLOCK) * BLOCK, fy = y - floorf(y / BLOCK) * BLOCK;
    if (fx < STREET || fy < STREET) return 0;
    return building((int32_t)floorf(x / BLOCK), (int32_t)floorf(y / BLOCK));
  }
  float altitude(const View &v) const override { return 3.0f + 12 * smooth(0.6f, 1, sinf(v.t * 0.04f)); }
  float clearance() const override { return 1.0f; }
  float horizonRow(float) const override { return 6.0f; }
  int raysPerColumn() const override { return 3; }

  float shade(const Hit &hit, float rowH, int, const View &v) const override {
    if (hit.h <= 0) return 0.04f;  // the street
    const int32_t bx = (int32_t)floorf(hit.x / BLOCK), by = (int32_t)floorf(hit.y / BLOCK);
    // The beacon: a corner of a tower's roof, blinking.
    if (hit.h > 9 && rowH > hit.h - 0.8f) {
      const float fx = hit.x - bx * BLOCK, fy = hit.y - by * BLOCK;
      if (fx > BLOCK - 1.5f && fy > BLOCK - 1.5f) {
        return fmodf(v.t + random01(bx, by, 2) * 3, 1.8f) < 0.35f ? 1.0f : 0.15f;
      }
    }
    if (rowH > hit.h - 0.3f) return 0.3f;   // the roof line, so blocks stand out
    if (hit.rise < 0.05f) return 0.08f;     // a roof seen from above
    // A facade: windows by floor and by bay; far away they blur into a glow.
    const int32_t storey = (int32_t)floorf(rowH / FLOOR);
    const float across = (hit.x + hit.y) * 1.1f;
    const int32_t bay = (int32_t)floorf(across);
    const float inFloor = rowH / FLOOR - storey;
    const float id = random01(bx * 131 + bay, by, storey);
    const float period = 20 + id * 40;  // each window keeps its state a while
    const bool on = random01(bay, storey, (int32_t)floorf(v.t / period + id * 7)) < 0.45f;
    // Soft-edged panes, so they slide across the pixels instead of popping.
    const float inBay = across - bay;
    const float pane = smooth(0.1f, 0.35f, inFloor) * smooth(0.95f, 0.7f, inFloor) * smooth(0.05f, 0.3f, inBay) *
                       smooth(0.95f, 0.7f, inBay);
    const float window = mix(0.1f, on ? 0.8f : 0.06f, pane);
    return mix(window, 0.26f, smooth(5, 14, hit.dist));
  }
  float sky(int y, float angle, const View &v) const override {
    const float glow = 0.08f * clamp01(1 - (v.horizon - y - 0.5f) / 4);  // the city's light
    return 0.01f + glow + stars(angle, y, v.t) * 0.6f;
  }
};

// ---------------------------------------------------------------------------
// Sopra le nuvole: sharp ridges with snow above the snow line, a layer of
// clouds drifting with the wind between the peaks; now and then the flight
// sinks through it (all goes white) and comes out again.
class CloudsFlight : public Flight {
 public:
  const char *id() const override { return "clouds"; }
  const char *name() const override { return "Sopra le nuvole"; }

 protected:
  static constexpr float CLOUDS = 9.0f;  // the height of the layer
  void path(View &v) override {
    v.heading = 0.4f * sinf(v.t * 0.05f);
    v.x = 25 * sinf(v.t * 0.018f);
    v.y = v.t * 2.6f;
  }
  // Ridged waves: 1 - |sin| makes crests (rounded off just at the tip, so
  // the rays don't step over them).
  static float ridge(float v) { return 1.05f - sqrtf(sinf(v) * sinf(v) + 0.01f); }
  float height(float x, float y, float) const override {
    const float a = ridge(x * 0.08f + sinf(y * 0.04f) * 1.5f);
    const float b = ridge(y * 0.07f - x * 0.03f);
    const float c = ridge((x + y) * 0.15f);
    const float h = a * 0.55f + b * 0.3f + c * 0.15f;
    return 1 + h * h * 18;
  }
  float density(float x, float y, float t) const {
    const float n = sinf(x * 0.12f + t * 0.35f) * sinf(y * 0.1f + 0.7f) + 0.6f * sinf(x * 0.27f - y * 0.2f + t * 0.5f);
    return smooth(-0.1f, 0.7f, n);
  }
  // Mostly above the clouds; every couple of minutes it dives through them.
  float altitude(const View &v) const override {
    float ahead = 0;
    for (float d = 0; d <= 12; d += 2) {
      ahead = max(ahead, height(v.x + sinf(v.heading) * d, v.y + cosf(v.heading) * d, v.t));
    }
    return max(ahead + 2.5f, CLOUDS + 3.0f + 4.5f * sinf(v.t * 0.05f));
  }
  float clearance() const override { return 2.0f; }
  int raysPerColumn() const override { return 2; }

  float shade(const Hit &hit, float, int, const View &) const override {
    const float east = (height(hit.x + 0.5f, hit.y, 0) - height(hit.x - 0.5f, hit.y, 0)) * 0.2f;
    const float rock = 0.08f + 0.15f * hit.h / 19 + hit.rise * 0.6f - constrain(east, -0.12f, 0.12f);
    return mix(rock, 0.75f + hit.rise * 0.4f, smooth(11.5f, 12.5f, hit.h));  // snow
  }
  float haze() const override { return 0.1f; }
  float crest() const override { return 0.5f; }

  void overlay(float *column, const float *depth, float sa, float ca, const View &v) const override {
    for (int y = 0; y < ROWS; y++) {
      // Where this row's ray crosses the layer, if in front of the ground.
      const float below = y + 0.5f - v.horizon;
      float d = INF;
      if (v.h > CLOUDS && below > 0) d = (v.h - CLOUDS) * SCALE / below;
      if (v.h < CLOUDS && below < 0) d = (CLOUDS - v.h) * SCALE / -below;
      if (d >= depth[y] || d >= FAR) continue;
      // Fades in as it gets in front of the ground, so peaks don't pop through.
      const float front = depth[y] == INF ? 1 : clamp01((depth[y] - d) / 6);
      const float k = density(v.x + sa * d, v.y + ca * d, v.t) * (1 - d / FAR) * front;
      column[y] = mix(column[y], 0.3f + 0.15f * (1 - d / FAR), k * 0.8f);
    }
    // Inside the layer: everything fades to white.
    const float inside = density(v.x, v.y, v.t) * clamp01(1 - fabsf(v.h - CLOUDS) / 1.5f);
    for (int y = 0; y < ROWS; y++) column[y] = mix(column[y], 0.45f, inside * 0.9f);
  }
};

static HillsFlight hills;
static SeaFlight sea;
static CanyonFlight canyon;
static CityFlight city;
static CloudsFlight clouds;
extern Animation *const voxelAnimation = &hills;
extern Animation *const seaAnimation = &sea;
extern Animation *const canyonAnimation = &canyon;
extern Animation *const cityAnimation = &city;
extern Animation *const cloudsAnimation = &clouds;
