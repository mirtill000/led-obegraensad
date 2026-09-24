#pragma once

#include <Arduino.h>

// Animated weather icons, 6x7 pixels per frame (bit 15 = leftmost column),
// shared by "Orologio e meteo" and "Previsioni".
struct AnimatedIcon {
  uint16_t frameMs;
  uint8_t frameCount;
  const uint16_t (*frames)[7];
};

// Shown in turn with the weather icon when rain is on its way.
extern const AnimatedIcon ICON_UMBRELLA;

// WMO weather code -> icon (https://open-meteo.com/en/docs).
const AnimatedIcon &iconFor(int code, bool isDay);
