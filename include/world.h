#pragma once

#include <Arduino.h>

// News from the world for the "Mondo" mode, fetched by the network task
// while that mode is on show (and for 15 minutes after):
//  - air quality where the lamp is (Open-Meteo, European AQI and PM2.5,
//    every 30 minutes)
//  - where the International Space Station is (wheretheiss.at, every
//    20 seconds), with its last positions for a trail
struct WorldInfo {
  bool airOk = false;
  int aqi = -1;
  float pm25 = -1;
  bool issOk = false;
  float issLat = 0, issLon = 0;
  static const int TRAIL = 12;
  float trailLat[TRAIL], trailLon[TRAIL];
  uint8_t trailCount = 0;
};

WorldInfo worldInfoNow();
// The mode says it is on show (starts/keeps the downloads going).
void worldWanted();
// Called by the network task.
void worldTick();
// For the page (see netfetch.h).
String airStatus();
String issStatus();

// Helpers, exposed for tests.
// Air quality band name for a European AQI value ("buona" ... "pessima").
const char *aqiBand(int aqi);
// Great-circle distance in km and initial bearing in degrees from (lat1, lon1).
float distanceKm(float lat1, float lon1, float lat2, float lon2, float *bearing = nullptr);
// "nord-est" etc. for a bearing in degrees.
const char *compassName(float bearing);
// Parsers of the two responses.
bool parseAir(const String &json, int &aqi, float &pm25);
bool parseIss(const String &json, float &lat, float &lon);
