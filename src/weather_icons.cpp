#include "weather_icons.h"

const Sprite &iconFor(int code, bool isDay) {
  if (code == 0) return isDay ? spr::WEATHER_SUN : spr::WEATHER_MOON;
  if (code <= 2) return isDay ? spr::WEATHER_PARTLY : spr::WEATHER_CLOUD;
  if (code == 3) return spr::WEATHER_CLOUD;
  if (code == 45 || code == 48) return spr::WEATHER_FOG;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return spr::WEATHER_SNOW;
  if (code >= 95) return spr::WEATHER_STORM;
  if (code >= 51) return spr::WEATHER_RAIN;
  return spr::WEATHER_CLOUD;
}
