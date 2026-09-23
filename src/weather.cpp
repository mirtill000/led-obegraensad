#include "weather.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "settings.h"

Weather weather;

static const uint32_t REFRESH_MS = 15 * 60 * 1000;
static const uint32_t RETRY_MS = 60 * 1000;
static uint32_t lastAttempt = 0;
static bool attempted = false;

// Finds `"key":` after `from` and returns the index just past it, or -1.
static int valueStart(const String &json, const char *key, int from) {
  const String needle = String('"') + key + "\":";
  const int i = json.indexOf(needle, from);
  return i < 0 ? -1 : i + needle.length();
}

bool parseWeather(const String &json, Weather &out) {
  // Skip "current_units", which repeats the same keys with unit strings.
  const int current = valueStart(json, "current", 0);
  if (current < 0) return false;
  const int t = valueStart(json, "temperature_2m", current);
  const int c = valueStart(json, "weather_code", current);
  const int d = valueStart(json, "is_day", current);
  if (t < 0 || c < 0 || d < 0) return false;
  out.temperature = json.substring(t).toFloat();
  out.code = json.substring(c).toInt();
  out.isDay = json.substring(d).toInt() != 0;
  out.valid = true;
  return true;
}

void updateWeather(bool force) {
  const uint32_t now = millis();
  if (!force && attempted) {
    if (weather.valid && now - weather.fetchedAt < REFRESH_MS) return;
    if (now - lastAttempt < RETRY_MS) return;
  }
  attempted = true;
  lastAttempt = now;
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();  // public weather data: no need to pin certificates
  HTTPClient http;
  http.setTimeout(5000);
  const String url = String("https://api.open-meteo.com/v1/forecast?latitude=") +
                     String(settings.latitude, 4) + "&longitude=" + String(settings.longitude, 4) +
                     "&current=temperature_2m,weather_code,is_day&timezone=auto";
  if (!http.begin(client, url)) return;
  if (http.GET() == HTTP_CODE_OK) {
    Weather fresh;
    if (parseWeather(http.getString(), fresh)) {
      fresh.fetchedAt = now;
      weather = fresh;
    }
  }
  http.end();
}
