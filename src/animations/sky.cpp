// "Finestra sul cielo": the sky outside right now, seen through a window
// (its frame and sill drawn, so it reads as one) looking south over the
// roofs. The sun (with its rays) and the moon (with its phase) where they
// really are (moon.cpp, from the lamp's latitude and longitude), the light
// of the hour (day, the glow of sunrise and sunset, night with stars and a
// few lit windows), puffy clouds as many as the weather says, and its rain,
// snow, fog or lightning (weather.cpp). Everything moves on the animation's
// clock; where things are comes from the real one.
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
const int HORIZON = 11;          // the row the roofs start on
const float PX_PER_RAD = 5.5f;   // the 14 columns inside the frame ~ 145 degrees of sky
// The roofs outside: the top row of each column inside the frame (1-14).
const uint8_t ROOFS[COLS] = {0, 12, 11, 11, 12, 10, 10, 10, 12, 11, 11, 12, 12, 10, 11, 0};

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
    const float day = smooth(-0.1f, 0.2f, sunEl);                    // 0 night .. 1 day
    const float twilight = clamp01(1 - fabsf(sunEl + 0.03f) / 0.2f);  // around sunrise and sunset
    const float sunX = 7.5f + wrapAngle(sunAz - PI_F) * PX_PER_RAD;
    const float sunY = HORIZON - sunEl * PX_PER_RAD;

    // The sky: dim by day (lighter low down, greyer when overcast), the
    // glow on the sun's side at sunrise and sunset, stars at night.
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        float v = day * (0.07f + 0.05f * y / HORIZON + 0.08f * smooth(0.6f, 1, c.cover));
        const float dx = (x + 0.5f - sunX) / 5, low = (HORIZON - y - 0.5f) / 3;
        v += twilight * 0.55f * expf(-dx * dx - low) * (1 - 0.6f * c.cover);
        if (day < 0.6f && y < 8) {
          const float r = hash01(x, y);
          if (r < 0.07f) v += (1 - day / 0.6f) * (1 - c.cover) * (0.18f + 0.1f * sinf(t * (1.5f + r * 20) + r * 70));
        }
        sky[y][x] = v;
      }
    }

    // The sun: a disc with short rays turning slowly (when the sky is
    // clear enough to see them).
    if (sunEl > -0.06f) {
      const float veil = 1 - 0.65f * c.cover;
      disc(sky, sunX, sunY, 1.7f, veil, -1);
      if (c.cover < 0.6f) {
        for (int k = 0; k < 8; k++) {
          const float a = k * PI_F / 4 + t * 0.3f;
          const float r = 3.0f + 0.3f * sinf(t * 2 + k);
          put(sky, sunX + cosf(a) * r - 0.5f, sunY + sinf(a) * r - 0.5f, 0.55f * veil * (k % 2 ? 0.7f : 1));
        }
      }
    }
    // The moon, its lit part from the phase (its dark part just showing);
    // it trails the sun by its phase: about where the sun was that fraction
    // of a lunar day ago.
    if (clock && day < 0.8f) {
      const float phase = moonPhase(when);
      float moonAz, moonEl;
      sunPosition(when - (time_t)(phase * 89400), settings.latitude, settings.longitude, moonAz, moonEl);
      if (moonEl > 0) {
        const float mx = 7.5f + wrapAngle(moonAz - PI_F) * PX_PER_RAD, my = HORIZON - moonEl * PX_PER_RAD;
        disc(sky, mx, my, 2.3f, 0.09f, -1);                      // the whole disc, faint
        disc(sky, mx, my, 2.3f, 0.75f * (1 - 0.7f * c.cover), phase);  // its lit part
      }
    }

    // Clouds: puffs of three rounded lumps with a flat base, lighter on
    // top, drifting with the wind - as many as the cover says.
    const int puffs = c.cover > 0 ? 1 + (int)(c.cover * 4.5f) : 0;
    float light = (0.12f + 0.33f * day + 0.1f * twilight) * (1 - 0.35f * c.rain);
    const bool flash = c.storm && fmodf(t, 6.0f) < 0.15f;  // lightning
    if (flash) light = 0.8f;
    for (int i = 0; i < puffs; i++) {
      const float span = 24;
      const float cx = fmodf(hash01(i, 5) * span + t * (0.35f + 0.1f * i), span) - 4;
      const float cy = 2.5f + i * 1.6f + hash01(i, 6);
      puff(sky, cx, cy, 0.9f + 0.25f * hash01(i, 7), light);
    }
    if (flash) bolt(sky, (int)(t / 6) % 3);

    // Fog: slow bands low over the roofs.
    if (c.fog) {
      for (int y = 5; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
          const float band = 0.5f + 0.5f * sinf(y * 1.3f + x * 0.15f + t * 0.4f);
          sky[y][x] = sky[y][x] * 0.5f + (0.1f + 0.12f * day) * (0.6f + 0.4f * band) * smooth(4, 10, y);
        }
      }
    }

    // The roofs: dark, their edges just showing, a few windows lit at night.
    for (int x = 1; x < COLS - 1; x++) {
      for (int y = ROOFS[x]; y < ROWS; y++) {
        const bool edge = y == ROOFS[x];
        sky[y][x] = edge ? (c.snow > 0 ? 0.3f : 0.07f + 0.05f * day) : 0.02f;
      }
    }
    if (day < 0.5f) {
      static const uint8_t LIT[][2] = {{3, 13}, {6, 12}, {10, 13}, {13, 12}};
      for (int i = 0; i < 4; i++) {
        if (hash01(i, (int32_t)(t / 40)) < 0.6f) sky[LIT[i][1]][LIT[i][0]] = 0.35f;  // they change now and then
      }
    }

    // Rain: streaks slanting with the wind; snow: drifting flakes. In
    // front of everything (the window is open on the weather).
    if (c.rain > 0) {
      const int drops = 4 + (int)(c.rain * 14);
      for (int i = 0; i < drops; i++) {
        const float speed = 11 + 4 * hash01(i, 1);
        const float y = fmodf(t * speed + hash01(i, 2) * 20, 20) - 3;
        const float x = 1 + fmodf(hash01(i, 3) * 20 + y * 0.35f, 14);
        for (int k = 0; k < 2; k++) put(sky, x - k * 0.35f, y - k, k ? 0.3f : 0.65f);
      }
    }
    if (c.snow > 0) {
      const int flakes = 6 + (int)(c.snow * 12);
      for (int i = 0; i < flakes; i++) {
        const float speed = 1.4f + hash01(i, 1);
        const float y = fmodf(t * speed + hash01(i, 2) * 18, 18) - 1;
        const float x = 1 + fmodf(hash01(i, 3) * 14 + sinf(t * 0.8f + i) * 1.2f + 14, 13);
        put(sky, x, y, 0.8f);
      }
    }

    // The window: its frame, and the sill.
    for (int i = 0; i < ROWS; i++) {
      sky[i][0] = sky[i][COLS - 1] = 0.13f;
      sky[0][i] = 0.13f;
    }
    for (int x = 0; x < COLS; x++) sky[ROWS - 1][x] = 0.22f;

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
  // A cloud centred at (cx, cy), `scale` big: three lumps over a flat
  // base, lighter on top; it hides what is behind it.
  static void puff(float sky[ROWS][COLS], float cx, float cy, float scale, float light) {
    static const float LUMPS[3][3] = {{-1.9f, 0.5f, 1.5f}, {0, -0.3f, 2.1f}, {1.9f, 0.6f, 1.4f}};  // dx, dy, radius
    for (int y = (int)(cy - 3 * scale); y <= (int)(cy + 2 * scale); y++) {
      for (int x = (int)(cx - 4 * scale); x <= (int)(cx + 4 * scale); x++) {
        if (x < 0 || y < 0 || x >= COLS || y >= HORIZON) continue;
        float cover = 0;
        for (int s = 0; s < 4; s++) {
          const float px = x + 0.25f + 0.5f * (s & 1), py = y + 0.25f + 0.5f * (s >> 1);
          if (py > cy + 1.6f * scale) continue;  // the flat base
          for (const auto &l : LUMPS) {
            const float dx = px - cx - l[0] * scale, dy = py - cy - l[1] * scale;
            if (dx * dx + dy * dy < l[2] * l[2] * scale * scale) {
              cover += 0.25f;
              break;
            }
          }
        }
        if (cover <= 0) continue;
        const float shade = light * (1.15f - 0.35f * clamp01((y - cy + 2 * scale) / (3.5f * scale)));
        sky[y][x] = sky[y][x] * (1 - cover) + shade * cover;
      }
    }
  }
  // A lightning bolt from the clouds to the roofs, zigzagging.
  static void bolt(float sky[ROWS][COLS], int which) {
    int x = 5 + which * 3;
    for (int y = 4; y < HORIZON; y++) {
      sky[y][x] = 1.0f;
      x += (y + which) % 3 == 0 ? 1 : (y % 2 ? -1 : 0);
      x = constrain(x, 1, COLS - 2);
    }
  }
  // A point at (x, y), shared between the pixels around it.
  static void put(float sky[ROWS][COLS], float x, float y, float level) {
    const int x0 = (int)floorf(x), y0 = (int)floorf(y);
    const float fx = x - x0, fy = y - y0;
    const float w[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
    for (int i = 0; i < 4; i++) {
      const int px = x0 + (i & 1), py = y0 + (i >> 1);
      if (py < 0 || py >= ROWS || px < 0 || px >= COLS) continue;
      sky[py][px] = min(1.0f, sky[py][px] + level * w[i]);
    }
  }
};

static SkyWindowAnimation skyWindow;
extern Animation *const skyAnimation = &skyWindow;
