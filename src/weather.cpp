#include "weather.h"

#include <string.h>

#include <WiFi.h>

#include "netfetch.h"
#include "settings.h"

static Weather latest;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
// Every 15 minutes; the last forecast is kept in flash for after a restart.
static Source source("Meteo", 15 * 60 * 1000, "weather");

Weather weatherNow() {
  portENTER_CRITICAL(&lock);
  Weather copy = latest;
  portEXIT_CRITICAL(&lock);
  return copy;
}

void requestWeatherUpdate() { source.request(); }

bool rainSoon(const Weather &w) {
  if (!w.valid || w.code >= 51) return false;  // no data, or already raining/snowing
  for (int i = 0; i < 3 && i < w.hours; i++) {
    if (w.hourlyRain[i] >= 60) return true;
  }
  return false;
}

// Finds `"key":` after `from` and returns the index just past it, or -1.
static int valueStart(const String &json, const char *key, int from) {
  const String needle = String('"') + key + "\":";
  const int i = json.indexOf(needle, from);
  return i < 0 ? -1 : i + needle.length();
}

// Reads a JSON array of numbers starting at `at` ('['); returns the count.
static int readNumbers(const String &json, int at, float *out, int max) {
  if (at < 0 || at >= (int)json.length() || json[at] != '[') return 0;
  int n = 0, i = at + 1;
  while (n < max && i < (int)json.length() && json[i] != ']') {
    out[n++] = json.substring(i).toFloat();  // toFloat stops at the comma; null reads as 0
    while (i < (int)json.length() && json[i] != ',' && json[i] != ']') i++;
    if (json[i] == ',') i++;
  }
  return n;
}

// "2026-09-23T07:12" at `at` (the opening quote) -> minutes after midnight.
static int readClock(const String &json, int at) {
  const int t = json.indexOf('T', at);
  if (t < 0 || t - at > 12) return -1;
  return json.substring(t + 1, t + 3).toInt() * 60 + json.substring(t + 4, t + 6).toInt();
}

bool parseWeather(const String &json, Weather &out) {
  // Skip the "*_units" blocks, which repeat the keys with unit strings.
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

  const int hourly = valueStart(json, "hourly", 0);
  if (hourly >= 0) {
    const int times = valueStart(json, "time", hourly);
    if (times >= 0) out.firstHour = readClock(json, times + 1) / 60;
    float rain[Weather::HOURS];
    const int temps = readNumbers(json, valueStart(json, "temperature_2m", hourly), out.hourlyTemp, Weather::HOURS);
    const int rains = readNumbers(json, valueStart(json, "precipitation_probability", hourly), rain, Weather::HOURS);
    out.hours = min(temps, rains);
    for (int i = 0; i < out.hours; i++) out.hourlyRain[i] = (uint8_t)constrain((int)rain[i], 0, 100);
  }
  const int daily = valueStart(json, "daily", 0);
  if (daily >= 0) {
    const int rise = valueStart(json, "sunrise", daily), set = valueStart(json, "sunset", daily);
    if (rise >= 0) out.sunrise = readClock(json, rise + 1);
    if (set >= 0) out.sunset = readClock(json, set + 1);
    float lo[Weather::DAYS], hi[Weather::DAYS], rain[Weather::DAYS], code[Weather::DAYS];
    const int n = min(readNumbers(json, valueStart(json, "temperature_2m_min", daily), lo, Weather::DAYS),
                      readNumbers(json, valueStart(json, "temperature_2m_max", daily), hi, Weather::DAYS));
    const int rains = readNumbers(json, valueStart(json, "precipitation_probability_max", daily), rain, Weather::DAYS);
    const int codes = readNumbers(json, valueStart(json, "weather_code", daily), code, Weather::DAYS);
    // "time":["2026-09-24","2026-09-25",...]
    int at = valueStart(json, "time", daily);
    for (int i = 0; i < n; i++) {
      out.dayMin[i] = lo[i];
      out.dayMax[i] = hi[i];
      out.dayRain[i] = i < rains ? (uint8_t)constrain((int)rain[i], 0, 100) : 0;
      out.dayCode[i] = i < codes ? (int16_t)code[i] : -1;
      at = at < 0 ? -1 : json.indexOf('"', at);
      if (at >= 0 && at + 11 < (int)json.length() && json[at + 5] == '-' && json[at + 8] == '-') {
        out.dayYear[i] = json.substring(at + 1, at + 5).toInt();
        out.dayMonth[i] = json.substring(at + 6, at + 8).toInt();
        out.dayOfMonth[i] = json.substring(at + 9, at + 11).toInt();
        at += 12;  // past the closing quote
      } else {
        at = -1;
      }
    }
    out.days = n;
  }
  return true;
}

static void store(const Weather &fresh) {
  portENTER_CRITICAL(&lock);
  latest = fresh;
  portEXIT_CRITICAL(&lock);
}

void weatherTick() {
  // Right after a restart: the forecast saved last time, until a new one.
  static bool cacheTried = false;
  if (!cacheTried) {
    cacheTried = true;
    String body;
    long age;
    Weather cached;
    if (source.loadCache(body, age) && parseWeather(body, cached)) {
      cached.fetchedAt = millis();
      store(cached);
    }
  }
  if (!source.due(millis()) || WiFi.status() != WL_CONNECTED) return;
  const String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(settings.latitude, 4) +
                     "&longitude=" + String(settings.longitude, 4) +
                     "&current=temperature_2m,weather_code,is_day"
                     "&hourly=temperature_2m,precipitation_probability&forecast_hours=12"
                     "&daily=weather_code,sunrise,sunset,temperature_2m_min,temperature_2m_max,precipitation_probability_max"
                     "&forecast_days=4&timezone=auto";
  String body;
  int code;
  Weather fresh;
  if (!httpsGet(url, body, code)) return source.failed(code);
  if (!parseWeather(body, fresh)) return source.failed(0);
  fresh.fetchedAt = millis();
  store(fresh);
  source.succeeded();
  source.saveCache(body);
}

String weatherStatus() { return source.status(); }
