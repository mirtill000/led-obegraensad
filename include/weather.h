#pragma once

#include <Arduino.h>

// Weather from Open-Meteo (free, no API key) for settings.latitude/longitude:
// current conditions, the next 12 hours and today's sunrise and sunset.
// Fetched in the background by the network task (see net.h), every 15
// minutes or on request.
struct Weather {
  bool valid = false;
  float temperature = 0;  // degrees Celsius
  int code = 0;           // WMO weather code
  bool isDay = true;
  uint32_t fetchedAt = 0;  // millis() of the last successful fetch

  // Next hours, starting with the current one.
  static const int HOURS = 12;
  uint8_t hours = 0;           // entries filled in
  uint8_t firstHour = 0;       // hour of day (0-23) of entry 0
  float hourlyTemp[HOURS] = {};
  uint8_t hourlyRain[HOURS] = {};  // precipitation probability, %

  // Today's sunrise and sunset, minutes after local midnight; -1 unknown.
  int16_t sunrise = -1;
  int16_t sunset = -1;

  // Today's range and chance of rain.
  bool hasDaily = false;
  float todayMin = 0, todayMax = 0;
  uint8_t todayRain = 0;  // highest precipitation probability of the day, %
  int todayCode = -1;     // WMO code for the day as a whole; -1 unknown
};

// A consistent copy of the latest data (safe to call from any task).
Weather weatherNow();
// Asks the network task to fetch now (e.g. after the location changed).
void requestWeatherUpdate();
// True if rain is likely (>= 60%) within the next 2 hours but it isn't
// raining yet.
bool rainSoon(const Weather &w);

// Called by the network task: fetches when the data is stale or requested.
void weatherTick();
// Parses an Open-Meteo response into `out`; false if malformed.
bool parseWeather(const String &json, Weather &out);
