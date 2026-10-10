#include "weather_art.h"

#include <math.h>

#include "display.h"
#include "ui.h"

namespace {

enum Kind : uint8_t { SUN, MOON, PARTLY_SUN, PARTLY_MOON, CLOUD, FOG, RAIN, SNOW, STORM };

Kind kindOf(int code, bool isDay) {
  if (code == 0) return isDay ? SUN : MOON;
  if (code <= 2) return isDay ? PARTLY_SUN : PARTLY_MOON;
  if (code == 45 || code == 48) return FOG;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return SNOW;
  if (code >= 95) return STORM;
  if (code >= 51) return RAIN;
  return CLOUD;
}

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
// 1 inside a circle, 0 outside, a soft edge half a pixel wide.
float disc(float px, float py, float cx, float cy, float r) { return clamp01(r - hypotf(px - cx, py - cy) + 0.5f); }
float hash01(int a, int b) {
  uint32_t h = (uint32_t)a * 0x8da6b343u ^ (uint32_t)b * 0xd8163841u;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return (h >> 8) / 16777216.0f;
}

struct Scene {
  Kind kind;
  float w, h, t, phase;
};

// How much of a cloud (centre cx, cy, size s) covers the point.
float cloudCover(float px, float py, float cx, float cy, float s) {
  float c = disc(px, py, cx - 0.32f * s, cy + 0.08f * s, 0.30f * s);
  c = fmaxf(c, disc(px, py, cx + 0.02f * s, cy - 0.12f * s, 0.40f * s));
  c = fmaxf(c, disc(px, py, cx + 0.36f * s, cy + 0.10f * s, 0.28f * s));
  // A flat base.
  const float base = cy + 0.34f * s;
  if (px > cx - 0.55f * s && px < cx + 0.6f * s && py > cy && py < base) c = fmaxf(c, clamp01(base - py + 0.5f));
  return c;
}

// A cloud's light at the point: a bright rim along its top edge (lit from
// above), a soft grey body getting darker underneath; `dark` dims it all
// (rain and storm clouds). *cover: how much of the point it hides.
float cloud(float px, float py, float cx, float cy, float s, float dark, float *cover) {
  const float c = cloudCover(px, py, cx, cy, s);
  *cover = c;
  if (c <= 0) return 0;
  const float above = cloudCover(px, py - 1.0f, cx, cy, s);
  const float rim = c * (1 - above);  // the top edge
  const float shade = clamp01(0.5f + (cy - py) / (0.9f * s));  // top 1, bottom 0
  const float body = c * (0.12f + 0.38f * shade);
  return fmaxf(body, rim * 0.95f) * (1 - dark);
}

float sun(float px, float py, float cx, float cy, float r, float t) {
  float v = disc(px, py, cx, cy, r);
  const float d = hypotf(px - cx, py - cy);
  // Rays: eight, turning slowly, breathing.
  if (d > r + 0.6f && d < r * 2.1f + 0.8f) {
    const float a = atan2f(py - cy, px - cx) - t * 0.4f;
    const float ray = powf(fabsf(cosf(a * 4)), 12);
    v = fmaxf(v, ray * (0.55f + 0.2f * sinf(t * 2)) * clamp01((r * 2.1f + 0.8f - d) / 1.2f));
  }
  return v;
}

float moon(float px, float py, float cx, float cy, float r, float phase) {
  const float in = disc(px, py, cx, cy, r);
  if (in <= 0) return 0;
  // The lit part: right of the terminator while waxing, left while waning.
  const float k = cosf(phase * 2 * (float)M_PI);  // 1 new, -1 full
  const float dx = (px - cx) / r, dy = (py - cy) / r;
  const float half = sqrtf(fmaxf(0, 1 - dy * dy));
  const float edge = k * half;  // the terminator's x
  const bool waxing = phase < 0.5f;
  const float lit = waxing ? clamp01((dx - edge) * r + 0.5f) : clamp01((-dx - edge) * r + 0.5f);
  return in * (0.06f + 0.94f * lit);
}

float pixel(const Scene &s, float px, float py) {
  const float W = s.w, H = s.h, t = s.t, S = fminf(W, H);
  float cover = 0;
  switch (s.kind) {
    case SUN: return sun(px, py, W / 2, H / 2, S * 0.24f, t);
    case MOON: return moon(px, py, W / 2, H / 2, S * 0.36f, s.phase);
    case PARTLY_SUN:
    case PARTLY_MOON: {
      // The cloud in front, low left; the sun (or moon) behind it, high right.
      const float c = cloud(px, py, W * 0.42f, H * 0.62f, S * 0.75f, 0.0f, &cover);
      const float back = s.kind == PARTLY_SUN ? sun(px, py, W * 0.66f, H * 0.32f, S * 0.2f, t)
                                              : moon(px, py, W * 0.66f, H * 0.32f, S * 0.24f, s.phase);
      return c + back * (1 - cover);
    }
    case CLOUD: {
      // Two clouds, the far one dimmer, drifting slowly.
      const float drift = sinf(t * 0.3f) * 0.6f;
      const float front = cloud(px, py, W * 0.45f + drift, H * 0.55f, S * 0.8f, 0.0f, &cover);
      float c2;
      const float back = cloud(px, py, W * 0.72f - drift, H * 0.32f, S * 0.5f, 0.3f, &c2);
      return front + back * 0.6f * (1 - cover);
    }
    case FOG: {
      // Bands drifting at their own speeds.
      float v = 0;
      for (int i = 0; i < 3; i++) {
        const float row = H * (0.25f + 0.25f * i);
        const float band = clamp01(1 - fabsf(py - row) / 0.9f);
        const float wave = 0.55f + 0.45f * sinf(px * 0.9f + t * (0.6f + 0.3f * i) + i * 2);
        v = fmaxf(v, band * wave * 0.6f);
      }
      return v;
    }
    case RAIN:
    case SNOW:
    case STORM: {
      const float c = cloud(px, py, W * 0.48f, H * 0.32f, S * 0.75f, s.kind == STORM ? 0.35f : 0.15f, &cover);
      float v = c;
      const float top = H * 0.5f;  // below the cloud
      if (py > top) {
        for (int i = 0; i < 5; i++) {
          const float x0 = W * (0.15f + 0.17f * i) + hash01(i, 3);
          const float span = H - top + 2;
          if (s.kind == SNOW) {
            const float y = top + fmodf(t * 2.2f + hash01(i, 1) * span, span) - 1;
            const float x = x0 + sinf(t * 1.5f + i) * 0.7f;
            v = fmaxf(v, 0.8f * disc(px, py, x, y, 0.55f));
          } else {
            const float y = top + fmodf(t * 6 + hash01(i, 1) * span, span) - 1;
            // A streak: bright head, a fading tail above it.
            const float along = y - py;
            if (fabsf(px - (x0 - along * 0.15f)) < 0.5f && along > -0.5f && along < 1.8f) {
              v = fmaxf(v, 0.95f * (1 - along / 2.2f));
            }
          }
        }
      }
      if (s.kind == STORM) {
        // Now and then a bolt, flashing twice.
        const float cycle = fmodf(t, 3.2f);
        if (cycle < 0.35f && (cycle < 0.12f || cycle > 0.22f)) {
          static const float BOLT[][2] = {{0.55f, 0.5f}, {0.42f, 0.68f}, {0.56f, 0.72f}, {0.44f, 0.95f}};
          for (int k = 0; k < 3; k++) {
            const float ax = BOLT[k][0] * W, ay = BOLT[k][1] * H, bx = BOLT[k + 1][0] * W, by = BOLT[k + 1][1] * H;
            const float vx = bx - ax, vy = by - ay;
            const float u = clamp01(((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy));
            const float d = hypotf(px - (ax + vx * u), py - (ay + vy * u));
            v = fmaxf(v, clamp01(1 - d * 1.4f));
          }
        }
      }
      return v;
    }
  }
  return 0;
}

}  // namespace

void drawWeatherArt(int code, bool isDay, float moonPhase, int x, int y, int w, int h, uint32_t now, uint8_t level) {
  const Scene s = {kindOf(code, isDay), (float)w, (float)h, now / 1000.0f, moonPhase};
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      // 2x2 samples per pixel: soft edges.
      float v = 0;
      for (int k = 0; k < 4; k++) v += pixel(s, i + 0.25f + 0.5f * (k & 1), j + 0.25f + 0.5f * (k >> 1));
      const uint8_t l = (uint8_t)(ui::tone(fminf(1, v / 4)) * level / 255);
      if (l > display.getLevel(x + i, y + j)) display.setLevel(x + i, y + j, l);
    }
  }
}
