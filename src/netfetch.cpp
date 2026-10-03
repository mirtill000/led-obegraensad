#include "netfetch.h"

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
  if (seconds < 60) return "adesso";
  if (seconds < 3600) return String(seconds / 60) + " min fa";
  if (seconds < 86400) return String(seconds / 3600) + " h fa";
  return String(seconds / 86400) + " g fa";
}

String Source::status() const {
  Guard g;
  if (off_) return "";
  const uint32_t now = millis();
  String s;
  if (failures_) {
    s = lastCode_ > 0 ? "errore " + String(lastCode_) : String("non raggiungibile");
    const uint32_t wait = retryMs() - min(retryMs(), now - lastTry_);
    s += " · riprovo tra " + (wait < 60000 ? String("poco") : String((wait + 59999) / 60000) + " min");
    if (everOk_) s += " · ultimo dato di " + ago((now - lastOk_) / 1000);
  } else if (everOk_) {
    s = "aggiornato " + ago((now - lastOk_) / 1000);
  } else if (cached_) {
    s = cachedAge_ >= 0 ? "dalla memoria (di " + ago(cachedAge_ + now / 1000) + ")" : String("dalla memoria");
  } else {
    return "in attesa";
  }
  if (detail_.length()) s += " · " + detail_;
  return s;
}

// ---------------------------------------------------------------------------
// The cache: "<epoch seconds>\n<text>" (epoch 0 if the clock wasn't set).

static String cachePath(const char *name) { return String("/cache/") + name; }

void Source::saveCache(const String &text) {
  if (!cacheName_) return;
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
