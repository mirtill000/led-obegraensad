#pragma once

#include <Arduino.h>

// Current weather from Open-Meteo (free, no API key), for
// settings.latitude/longitude.
struct Weather {
  bool valid = false;
  float temperature = 0;  // degrees Celsius
  int code = 0;           // WMO weather code
  bool isDay = true;
  uint32_t fetchedAt = 0;  // millis() of the last successful fetch
};

extern Weather weather;

// Fetches if the data is missing or older than the refresh interval, or if
// `force` is set. Blocks for up to a few seconds while fetching; needs the
// lamp to be on a WiFi network with internet.
void updateWeather(bool force = false);

// Parses an Open-Meteo "current" response into `out`; false if malformed.
bool parseWeather(const String &json, Weather &out);
