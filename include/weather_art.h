#pragma once

#include <Arduino.h>

// The weather as a little shaded, moving picture (instead of the 1-bit
// icons): a sun with its rays turning, the moon in its real phase, clouds
// lit from above, rain and snow falling out of them, lightning, drifting
// fog. Drawn anti-aliased into the box (x, y, w, h) of the panel, never
// darkening what is already there; `level` scales it (0-255).
void drawWeatherArt(int code, bool isDay, float moonPhase, int x, int y, int w, int h, uint32_t now,
                    uint8_t level = 255);
