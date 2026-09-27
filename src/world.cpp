#include "world.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include "settings.h"
#include "timekeeping.h"

static WorldInfo latest;
static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
static volatile uint32_t wantedAt = 0;
static volatile bool everWanted = false;

WorldInfo worldInfoNow() {
  xSemaphoreTake(lock, portMAX_DELAY);
  WorldInfo copy = latest;
  xSemaphoreGive(lock);
  return copy;
}

void worldWanted() {
  wantedAt = millis();
  everWanted = true;
}

// ---------------------------------------------------------------------------

const char *aqiBand(int aqi) {
  if (aqi < 20) return "buona";
  if (aqi < 40) return "discreta";
  if (aqi < 60) return "moderata";
  if (aqi < 80) return "scadente";
  if (aqi < 100) return "molto scadente";
  return "pessima";
}

float distanceKm(float lat1, float lon1, float lat2, float lon2, float *bearing) {
  const double r = M_PI / 180;
  const double p1 = lat1 * r, p2 = lat2 * r, dl = (lon2 - lon1) * r;
  const double a = sin((p2 - p1) / 2) * sin((p2 - p1) / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
  if (bearing) {
    double b = atan2(sin(dl) * cos(p2), cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl)) / r;
    *bearing = fmod(b + 360, 360);
  }
  return 6371.0 * 2 * atan2(sqrt(a), sqrt(1 - a));
}

const char *compassName(float bearing) {
  static const char *const NAMES[] = {"nord", "nord-est", "est", "sud-est", "sud", "sud-ovest", "ovest", "nord-ovest"};
  return NAMES[(int)floorf(fmodf(bearing + 22.5f, 360) / 45) % 8];
}

// The text after "key": up to the next , } or ] (numbers), or the string.
static String valueAfter(const String &json, const char *key, int from = 0) {
  const String k = String("\"") + key + "\":";
  int i = json.indexOf(k, from);
  if (i < 0) return "";
  i += k.length();
  while (i < (int)json.length() && json[i] == ' ') i++;
  if (i < (int)json.length() && json[i] == '"') {
    String out;
    for (i++; i < (int)json.length() && json[i] != '"'; i++) {
      if (json[i] == '\\' && i + 1 < (int)json.length()) i++;
      out += json[i];
    }
    return out;
  }
  int end = i;
  while (end < (int)json.length() && json[end] != ',' && json[end] != '}' && json[end] != ']') end++;
  return json.substring(i, end);
}

bool parseAir(const String &json, int &aqi, float &pm25) {
  // {"current_units":{...,"european_aqi":"EAQI"},"current":{...,"european_aqi":18,"pm2_5":4.1}}
  const int cur = json.indexOf("\"current\":");
  if (cur < 0) return false;
  const String a = valueAfter(json, "european_aqi", cur), p = valueAfter(json, "pm2_5", cur);
  if (!a.length() || a == "null") return false;
  aqi = (int)lroundf(a.toFloat());
  pm25 = p.length() && p != "null" ? p.toFloat() : -1;
  return true;
}

bool parseIss(const String &json, float &lat, float &lon) {
  const String a = valueAfter(json, "latitude"), o = valueAfter(json, "longitude");
  if (!a.length() || !o.length()) return false;
  lat = a.toFloat();
  lon = o.toFloat();
  return true;
}

// ---------------------------------------------------------------------------

static bool get(const String &url, String &body, int &code) {
  WiFiClientSecure client;
  client.setInsecure();  // public/read-only data
  HTTPClient http;
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("OBEGRANSAD-lamp/1.0 (ESP32)");
  if (!http.begin(client, url)) return false;
  code = http.GET();
  if (code == HTTP_CODE_OK) body = http.getString();
  http.end();
  return code == HTTP_CODE_OK;
}

void worldTick() {
  static uint32_t airAt = 0, issAt = 0;
  static bool airDone = false;
  const uint32_t now = millis();
  if (!everWanted || now - wantedAt > 15 * 60000UL) return;  // nobody is looking
  if (WiFi.status() != WL_CONNECTED) return;

  if (!issAt || now - issAt >= 20000) {
    issAt = now;
    String body;
    int code = 0;
    float lat, lon;
    const bool ok = get("https://api.wheretheiss.at/v1/satellites/25544", body, code) && parseIss(body, lat, lon);
    xSemaphoreTake(lock, portMAX_DELAY);
    if (ok) {
      if (latest.issOk) {
        // Keep a trail of where it has been.
        memmove(latest.trailLat + 1, latest.trailLat, sizeof(float) * (WorldInfo::TRAIL - 1));
        memmove(latest.trailLon + 1, latest.trailLon, sizeof(float) * (WorldInfo::TRAIL - 1));
        latest.trailLat[0] = latest.issLat;
        latest.trailLon[0] = latest.issLon;
        if (latest.trailCount < WorldInfo::TRAIL) latest.trailCount++;
      }
      latest.issLat = lat;
      latest.issLon = lon;
      latest.issOk = true;
      latest.issStatus = "ok";
    } else {
      latest.issStatus = String("non raggiungibile (") + code + ")";
    }
    xSemaphoreGive(lock);
  }

  if (!airDone || now - airAt >= 30 * 60000UL) {
    airAt = now;
    airDone = true;
    char url[200];
    snprintf(url, sizeof(url),
             "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f&current=european_aqi,pm2_5",
             settings.latitude, settings.longitude);
    String body;
    int code = 0, aqi;
    float pm25;
    const bool ok = get(url, body, code) && parseAir(body, aqi, pm25);
    xSemaphoreTake(lock, portMAX_DELAY);
    if (ok) {
      latest.airOk = true;
      latest.aqi = aqi;
      latest.pm25 = pm25;
      latest.airStatus = "ok";
    } else {
      latest.airStatus = String("non disponibile (") + code + ")";
    }
    xSemaphoreGive(lock);
  }
}
