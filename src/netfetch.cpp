#include "netfetch.h"

#include "texts.h"

#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <time.h>

static const uint32_t FIRST_RETRY_MS = 60 * 1000;
static const int MAX_SOURCES = 8;
static Source *sources[MAX_SOURCES];
static int sourceCount = 0;
static SemaphoreHandle_t lock = xSemaphoreCreateMutex();

struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};

Source::Source(const char *l, uint32_t everyMs, const char *cacheName) : label(l), everyMs_(everyMs), cacheName_(cacheName) {
  if (sourceCount < MAX_SOURCES) sources[sourceCount++] = this;
}

uint32_t Source::retryMs() const {
  uint32_t ms = FIRST_RETRY_MS;
  for (int i = 1; i < failures_ && ms < everyMs_; i++) ms *= 2;
  return min(ms, everyMs_);
}

bool Source::due(uint32_t now) const {
  Guard g;
  if (off_) return false;
  if (requested_ || !tried_) return true;
  if (failures_) return now - lastTry_ >= retryMs();
  return now - lastOk_ >= everyMs_;
}

void Source::request() {
  Guard g;
  requested_ = true;
  off_ = false;
}

void Source::succeeded(const String &detail) {
  Guard g;
  tried_ = everOk_ = true;
  requested_ = off_ = cached_ = false;
  lastTry_ = lastOk_ = millis();
  failures_ = 0;
  lastCode_ = 200;
  detail_ = detail;
}

void Source::failed(int code) {
  Guard g;
  tried_ = true;
  requested_ = false;
  lastTry_ = millis();
  if (failures_ < 30) failures_++;
  lastCode_ = code;
}

void Source::off() {
  Guard g;
  off_ = true;
  requested_ = false;
}

bool Source::ok() const {
  Guard g;
  return !failures_ && (everOk_ || cached_);
}

static String ago(uint32_t seconds) {
  if (seconds < 60) return txt::AGO_NOW;
  if (seconds < 3600) return String(seconds / 60) + txt::AGO_MIN;
  if (seconds < 86400) return String(seconds / 3600) + txt::AGO_HOURS;
  return String(seconds / 86400) + txt::AGO_DAYS;
}

String Source::status() const {
  Guard g;
  if (off_) return "";
  const uint32_t now = millis();
  String s;
  if (failures_) {
    s = lastCode_ > 0 ? String(txt::FETCH_ERROR) + " " + lastCode_ : String(txt::FETCH_UNREACHABLE);
    const uint32_t wait = retryMs() - min(retryMs(), now - lastTry_);
    s += String(" · ") + txt::FETCH_RETRY + " " + (wait < 60000 ? String(txt::FETCH_RETRY_SOON) : String((wait + 59999) / 60000) + " min");
    if (everOk_) s += String(" · ") + txt::FETCH_LAST + " " + ago((now - lastOk_) / 1000);
  } else if (everOk_) {
    s = String(txt::FETCH_UPDATED) + " " + ago((now - lastOk_) / 1000);
  } else if (cached_) {
    s = cachedAge_ >= 0 ? String(txt::FETCH_CACHED) + " (di " + ago(cachedAge_ + now / 1000) + ")" : String(txt::FETCH_CACHED);
  } else {
    return txt::FETCH_WAITING;
  }
  if (detail_.length()) s += " · " + detail_;
  return s;
}

// ---------------------------------------------------------------------------
// The cache: "<epoch seconds>\n<text>" (epoch 0 if the clock wasn't set).

static String cachePath(const char *name) { return String("/cache/") + name; }

void Source::saveCache(const String &text) {
  if (!cacheName_) return;
  // At most every 3 hours (and once after boot): the cache only has to give
  // something to show right after a restart, and every write to the flash
  // stalls the panel's refresh for a moment (a visible blink).
  const uint32_t ms = millis();
  if (cacheSavedAt_ && ms - cacheSavedAt_ < 3 * 3600 * 1000UL) return;
  cacheSavedAt_ = ms ? ms : 1;
  if (!LittleFS.exists("/cache")) LittleFS.mkdir("/cache");
  File f = LittleFS.open(cachePath(cacheName_), "w");
  if (!f) return;
  const time_t now = time(nullptr);
  f.print(String(now > 1600000000 ? (long)now : 0L));
  f.print('\n');
  f.print(text);
  f.close();
}

bool Source::loadCache(String &text, long &ageSeconds) {
  ageSeconds = -1;
  if (!cacheName_) return false;
  File f = LittleFS.open(cachePath(cacheName_), "r");
  if (!f) return false;
  const long saved = f.readStringUntil('\n').toInt();
  text = f.readString();
  f.close();
  const time_t now = time(nullptr);
  if (saved > 0 && now > 1600000000 && now >= saved) ageSeconds = now - saved;
  Guard g;
  cached_ = text.length() > 0;
  cachedAge_ = ageSeconds >= 0 ? ageSeconds - (long)(millis() / 1000) : -1;  // status() adds the uptime back
  return cached_;
}

// ---------------------------------------------------------------------------

static bool begin(HTTPClient &http, WiFiClientSecure &client, const String &url) {
  client.setInsecure();  // public, read-only data: no certificates to pin
  http.useHTTP10(true);  // no chunked encoding: simpler streaming
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("OBEGRANSAD-lamp/1.0 (ESP32)");
  return http.begin(client, url);
}

bool httpsGet(const String &url, Stream &sink, int &code) {
  WiFiClientSecure client;
  HTTPClient http;
  code = -1;
  if (!begin(http, client, url)) return false;
  code = http.GET();
  if (code == HTTP_CODE_OK) http.writeToStream(&sink);
  http.end();
  return code == HTTP_CODE_OK;
}

bool httpsGet(const String &url, String &body, int &code) {
  WiFiClientSecure client;
  HTTPClient http;
  code = -1;
  if (!begin(http, client, url)) return false;
  code = http.GET();
  if (code == HTTP_CODE_OK) body = http.getString();
  http.end();
  return code == HTTP_CODE_OK;
}

String sourcesJson() {
  String j = "[";
  for (int i = 0; i < sourceCount; i++) {
    const String st = sources[i]->status();
    if (!st.length()) continue;  // switched off
    if (j.length() > 1) j += ',';
    j += String("{\"name\":\"") + sources[i]->label + "\",\"status\":\"" + st + "\",\"ok\":" +
         (sources[i]->ok() ? "true" : "false") + "}";
  }
  return j + "]";
}
