#pragma once

#include <Arduino.h>

// One way to get things from the web, for everything the lamp downloads
// (weather, Wikipedia, the calendar, air quality, the Space Station):
//  - one HTTPS client (httpsGet), same timeouts and user agent;
//  - one rule for when to fetch: after a success, every `everyMs`; after a
//    failure, again in 1 minute, then 2, 4, ... up to `everyMs`;
//  - one status for the page and the diagnostics: "aggiornato 3 min fa",
//    "errore 503 · riprovo tra 4 min", "dalla memoria (di 2 h fa)";
//  - the last good answer kept in flash (LittleFS /cache/<name>), so after a
//    restart the lamp shows it before the network is back.
// Sources are used from the network task; status() is safe from any task.
class Source {
 public:
  Source(const char *label, uint32_t everyMs, const char *cacheName = nullptr);

  bool due(uint32_t now) const;  // time to fetch?
  void request();                // fetch at the next chance
  void succeeded(const String &detail = String());  // detail: e.g. "12 eventi"
  void failed(int code);         // HTTP code, or <= 0: no connection / bad answer
  void off();                    // switched off: no status, not due

  String status() const;
  bool ok() const;               // the last attempt worked (or a cached result)

  // The last good answer in flash. load() also says how old it is (seconds,
  // -1 if unknown: the clock wasn't set when it was saved).
  void saveCache(const String &text);
  bool loadCache(String &text, long &ageSeconds);

  const char *label;

 private:
  const uint32_t everyMs_;
  const char *cacheName_;
  uint32_t lastTry_ = 0, lastOk_ = 0;
  uint8_t failures_ = 0;
  int lastCode_ = 0;
  bool tried_ = false, everOk_ = false, requested_ = true, off_ = false, cached_ = false;
  long cachedAge_ = -1;
  String detail_;
  uint32_t retryMs() const;
};

// GET `url` over HTTPS: the body streamed to `sink` or kept in `body`.
// `code` is the HTTP status (or a negative HTTPClient error).
bool httpsGet(const String &url, Stream &sink, int &code);
bool httpsGet(const String &url, String &body, int &code);

// Every source with its status, for the diagnostics:
// [{"name":"Meteo","status":"aggiornato 3 min fa","ok":true},...]
String sourcesJson();
