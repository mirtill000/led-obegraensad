#pragma once

#include "sprite_atlas.h"

// Animated weather icons, 6x7 sprites of the atlas (weather.*), shared by
// "Orologio" and "Previsioni": draw them at sprites::frameAt(icon, now).

// WMO weather code -> icon (https://open-meteo.com/en/docs). The umbrella
// (spr::WEATHER_UMBRELLA) is shown in turn with it when rain is on its way.
const Sprite &iconFor(int code, bool isDay);
