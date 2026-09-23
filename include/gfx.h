#pragma once

#include <math.h>

#include "display.h"

// Small drawing helpers on top of Display's grayscale levels.
namespace gfx {

inline uint8_t level(float v) { return v <= 0 ? 0 : v >= 1 ? 255 : (uint8_t)(v * 255 + 0.5f); }

// Lights (x, y) at least as bright as `v` (0-1): overlapping shapes add up
// to the brightest one instead of overwriting each other.
inline void plot(int x, int y, float v) {
  const uint8_t l = level(v);
  if (l > display.getLevel(x, y)) display.setLevel(x, y, l);
}

// Antialiased line (Xiaolin Wu), brightness `v` 0-1.
inline void line(float x0, float y0, float x1, float y1, float v = 1) {
  const bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
  if (steep) {
    float t = x0; x0 = y0; y0 = t;
    t = x1; x1 = y1; y1 = t;
  }
  if (x0 > x1) {
    float t = x0; x0 = x1; x1 = t;
    t = y0; y0 = y1; y1 = t;
  }
  const float dx = x1 - x0, dy = y1 - y0;
  const float gradient = dx == 0 ? 1 : dy / dx;
  const int xStart = (int)roundf(x0), xEnd = (int)roundf(x1);
  float y = y0 + gradient * (xStart - x0);
  for (int x = xStart; x <= xEnd; x++, y += gradient) {
    const int yi = (int)floorf(y);
    const float f = y - yi;
    if (steep) {
      plot(yi, x, v * (1 - f));
      plot(yi + 1, x, v * f);
    } else {
      plot(x, yi, v * (1 - f));
      plot(x, yi + 1, v * f);
    }
  }
}

}  // namespace gfx
