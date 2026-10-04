// "Finestra sul cielo": the sky outside right now, as if you looked south
// through a window. The sun and the moon (with its phase) where they really
// are (moon.cpp, from the lamp's latitude and longitude), the light of the
// hour (day, the glow of sunrise and sunset, night with stars), clouds as
// thick as the weather says, and its rain, snow, fog or lightning
// (weather.cpp). Everything moves on the animation's clock; where things
// are comes from the real one.
#include <math.h>
#include <time.h>

#include "animation.h"
#include "display.h"
#include "moon.h"
#include "settings.h"
#include "timekeeping.h"
#include "ui.h"
#include "weather.h"

namespace {

const float PI_F = (float)M_PI;
const int HORIZON = 13;          // the row the ground starts on
const float PX_PER_RAD = 6.5f;   // 16 columns ~ 140 degrees of sky

float clamp01(float v) { return constrain(v, 0.0f, 1.0f); }
float smooth(float a, float b, float x) {
  const float k = clamp01((x - a) / (b - a));
  return k * k * (3 - 2 * k);
}
float wrapAngle(float a) {
  a = fmodf(a + PI_F, 2 * PI_F);
  return a < 0 ? a + PI_F : a - PI_F;
}
float hash01(int32_t a, int32_t b) {
  uint32_t h = (uint32_t)a * 0x8da6b343u ^ (uint32_t)b * 0xd8163841u;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return (h >> 8) / 16777216.0f;
}

// What the weather code means for the window.
struct Conditions {
  float cover = 0;   // 0 clear .. 1 overcast
  float rain = 0;    // drops: 0 none .. 1 heavy
  float snow = 0;    // flakes
  bool fog = false, storm = false;
};

Conditions conditionsFor(const Weather &w) {
  Conditions c;
  if (!w.valid) return c;
  const int k = w.code;
  if (k == 1) c.cover = 0.2f;
  else if (k == 2) c.cover = 0.45f;
  else if (k == 3) c.cover = 0.8f;
  else if (k == 45 || k == 48) c.cover = 0.6f, c.fog = true;
  else if (k >= 51 && k <= 57) c.cover = 0.8f, c.rain = 0.3f;        // drizzle
  else if (k >= 61 && k <= 67) c.cover = 0.9f, c.rain = k >= 65 ? 1.0f : k >= 63 ? 0.7f : 0.45f;
  else if (k >= 71 && k <= 77) c.cover = 0.85f, c.snow = k >= 75 ? 1.0f : 0.6f;
  else if (k >= 80 && k <= 82) c.cover = 0.75f, c.rain = k == 82 ? 1.0f : 0.6f;  // showers
  else if (k == 85 || k == 86) c.cover = 0.8f, c.snow = 0.8f;
  else if (k >= 95) c.cover = 0.95f, c.rain = 0.8f, c.storm = true;
  return c;
}

}  // namespace

class SkyWindowAnimation : public Animation {
 public:
  const char *id() const override { return "sky"; }
  const char *name() const override { return "Finestra sul cielo"; }
  const char *group() const override { return "Atmosfere"; }
  uint16_t frameMs() const override { return 50; }
  bool needsTime() const override { return true; }

  void frame(uint32_t now) override {
    const float t = now / 1000.0f;
    float sky[ROWS][COLS];
    const time_t when = time(nullptr);
    struct tm tm;
    const bool clock = localTime(tm);
    float sunAz = PI_F, sunEl = -0.5f;  // without the clock: a calm night
    if (clock) sunPosition(when, settings.latitude, settings.longitude, sunAz, sunEl);
    const Conditions c = conditionsFor(weatherNow());
    const float day = smooth(-0.1f, 0.2f, sunEl);             // 0 night .. 1 day
    const float twilight = clamp01(1 - fabsf(sunEl + 0.03f) / 0.2f);  // around sunrise and sunset
    const float sunX = 7.5f + wrapAngle(sunAz - PI_F) * PX_PER_RAD;
    const float sunY = HORIZON - sunEl * PX_PER_RAD;

    // The sky: a dim gradient by day (lighter low down), the glow on the
    // sun's side at sunrise and sunset, stars at night.
    for (int y = 0; y < ROWS; y++) {  // (below the horizon too: the hills leave gaps)
      for (int x = 0; x < COLS; x++) {
        float v = day * (0.09f + 0.07f * y / HORIZON) * (1 - 0.4f * c.cover);
        const float dx = (x + 0.5f - sunX) / 6, low = (HORIZON - y - 0.5f) / 3.5f;
        v += twilight * 0.55f * expf(-dx * dx - low) * (1 - 0.6f * c.cover);
        if (day < 0.6f) {
          const float r = hash01(x, y);
          if (r < 0.09f) v += (1 - day / 0.6f) * (0.16f + 0.1f * sinf(t * (1.5f + r * 20) + r * 70));
        }
        sky[y][x] = v;
      }
    }
    // The ground: dark, the outline of the hills lit a little (white if it
    // is snowing).
    for (int x = 0; x < COLS; x++) {
      const int top = HORIZON + (sinf(x * 0.7f + 1) + sinf(x * 0.31f) > 0.9f ? 0 : 1);
      for (int y = HORIZON; y < ROWS; y++) {
        sky[y][x] = y < top ? sky[y][x] : y > top ? 0 : c.snow > 0 ? 0.2f + 0.15f * day : 0.08f + 0.06f * day;
      }
    }

    // The sun: a soft disc, its light veiled by the clouds.
    if (sunEl > -0.06f) disc(sky, sunX, sunY, 1.5f, 1.0f * (1 - 0.7f * c.cover), -1);
    // The moon: its lit part from the phase (the moon trails the sun by
    // its phase: about where the sun was that fraction of a lunar day ago).
    if (clock && day < 0.8f) {
      const float phase = moonPhase(when);
      float moonAz, moonEl;
      sunPosition(when - (time_t)(phase * 89400), settings.latitude, settings.longitude, moonAz, moonEl);
      if (moonEl > 0) {
        disc(sky, 7.5f + wrapAngle(moonAz - PI_F) * PX_PER_RAD, HORIZON - moonEl * PX_PER_RAD, 2.2f,
             0.75f * (1 - 0.8f * c.cover), phase);
      }
    }

    // Clouds, drifting with the wind: as many as the cover says.
    if (c.cover > 0) {
      const float cloudLight = (0.1f + 0.3f * day + 0.1f * twilight) * (1 - 0.45f * c.rain);
      for (int y = 0; y < HORIZON; y++) {
        for (int x = 0; x < COLS; x++) {
          const float u = x + t * 0.5f, v = y * 1.6f;
          const float n = 0.5f + 0.3f * sinf(u * 0.35f + sinf(v * 0.4f + t * 0.1f)) * cosf(v * 0.3f - u * 0.1f) +
                          0.2f * sinf(u * 0.9f - v * 0.7f + t * 0.3f);
          const float k = smooth(1.05f - c.cover, 1.25f - c.cover, n) * smooth(-1, 3, y);
          // Lightning: the clouds flash now and then.
          float light = cloudLight;
          if (c.storm && hash01((int32_t)(t * 8), 3) < 0.012f) light = 0.9f;
          sky[y][x] = sky[y][x] * (1 - k) + light * (0.75f + 0.25f * n) * k;
        }
      }
    }
    // Fog: slow bands low over the ground.
    if (c.fog) {
      for (int y = 4; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
          const float band = 0.5f + 0.5f * sinf(y * 1.3f + x * 0.15f + t * 0.4f);
          sky[y][x] = sky[y][x] * 0.4f + (0.1f + 0.15f * day) * (0.6f + 0.4f * band) * smooth(3, 10, y);
        }
      }
    }
    // Rain: streaks slanting with the wind; snow: drifting flakes.
    if (c.rain > 0) {
      const int drops = 4 + (int)(c.rain * 18);
      for (int i = 0; i < drops; i++) {
        const float speed = 10 + 4 * hash01(i, 1);
        const float y = fmodf(t * speed + hash01(i, 2) * 20, 20) - 3;
        const float x = fmodf(hash01(i, 3) * 20 + y * 0.25f, 16);
        for (int k = 0; k < 2; k++) put(sky, x - k * 0.25f, y - k, k ? 0.3f : 0.65f);
      }
    }
    if (c.snow > 0) {
      const int flakes = 6 + (int)(c.snow * 16);
      for (int i = 0; i < flakes; i++) {
        const float speed = 1.4f + hash01(i, 1);
        const float y = fmodf(t * speed + hash01(i, 2) * 18, 18) - 1;
        const float x = fmodf(hash01(i, 3) * 16 + sinf(t * 0.8f + i) * 1.2f + 16, 16);
        put(sky, x, y, 0.8f);
      }
    }

    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) display.setLevel(x, y, ui::tone(sky[y][x]));
    }
  }

 private:
  // A disc of radius r at (cx, cy), `level` bright, sampled 2x2 per pixel;
  // with a moon `phase` (0 new .. 0.5 full .. 1; -1 for the sun) only its
  // lit part.
  static void disc(float sky[ROWS][COLS], float cx, float cy, float r, float level, float phase) {
    const float k = cosf(phase * 2 * PI_F);
    for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++) {
      for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
        if (x < 0 || y < 0 || x >= COLS || y >= HORIZON) continue;
        float lit = 0;
        for (int s = 0; s < 4; s++) {
          const float u = (x + 0.25f + 0.5f * (s & 1) - cx) / r, v = (y + 0.25f + 0.5f * (s >> 1) - cy) / r;
          if (u * u + v * v > 1) continue;
          if (phase >= 0) {
            const float edge = k * sqrtf(1 - v * v);  // the terminator
            if (phase < 0.5f ? u < edge : u > -edge) continue;
          }
          lit += 0.25f;
        }
        if (lit > 0) sky[y][x] = max(sky[y][x], level * lit);
      }
    }
  }
  // A point at (x, y), shared between the pixels around it.
  static void put(float sky[ROWS][COLS], float x, float y, float level) {
    const int x0 = (int)floorf(x), y0 = (int)floorf(y);
    const float fx = x - x0, fy = y - y0;
    const float w[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
    for (int i = 0; i < 4; i++) {
      const int px = (x0 + (i & 1)) % COLS, py = y0 + (i >> 1);
      if (py < 0 || py >= ROWS) continue;
      sky[py][px] = min(1.0f, sky[py][px] + level * w[i]);
    }
  }
};

static SkyWindowAnimation skyWindow;
extern Animation *const skyAnimation = &skyWindow;
