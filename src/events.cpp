#include "events.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <esp_ota_ops.h>

#include <vector>

#include "build_info.h"

static const char *LOG_FILE = "/events.log";
static const size_t KEEP = 30;

struct Event {
  uint32_t when;  // epoch seconds, or 0 if the clock wasn't set yet
  String text;
};
static std::vector<Event> events;
static bool started = false, pending = false;
static String bootText;

// --- rollback of a firmware that doesn't start ---------------------------------
//
// The bootloader can go back to the previous firmware on its own; the
// Arduino core would mark every new one as good straight away. Instead the
// lamp confirms a new firmware only once it has run for CONFIRM_MS with
// WiFi up: if it crashes or hangs before (the loop watchdog restarts it),
// the bootloader starts the previous firmware again.
static const uint32_t CONFIRM_MS = 60000;

extern "C" bool verifyRollbackLater() { return true; }  // we decide, in eventsLoop()

bool firmwarePendingVerify() {
  esp_ota_img_states_t state;
  return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
         state == ESP_OTA_IMG_PENDING_VERIFY;
}

// --- the log -----------------------------------------------------------------------

static void save() {
  File f = LittleFS.open(LOG_FILE, "w");
  if (!f) return;
  for (const Event &e : events) f.printf("%lu|%s\n", (unsigned long)e.when, e.text.c_str());
  f.close();
}

static uint32_t now() {
  const time_t t = time(nullptr);
  return t > 1600000000 ? (uint32_t)t : 0;  // before the clock is set: unknown
}

void logEvent(const String &text) {
  String line = text;
  line.replace("\n", " ");
  line.replace("|", "/");
  events.push_back({now(), line.substring(0, 120)});
  while (events.size() > KEEP) events.erase(events.begin());
  save();
}

const char *resetReasonText() {
  const esp_reset_reason_t r = esp_reset_reason();
  switch (r) {
    case ESP_RST_POWERON: return "accensione";
    case ESP_RST_SW: return "riavvio (aggiornamento o comando)";
    case ESP_RST_PANIC: return "errore del firmware";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog (blocco)";
    case ESP_RST_BROWNOUT: return "calo di tensione";
    default: return "altro";
  }
}

void eventsBegin() {
  File f = LittleFS.open(LOG_FILE, "r");
  while (f && f.available()) {
    const String line = f.readStringUntil('\n');
    const int bar = line.indexOf('|');
    if (bar > 0) events.push_back({(uint32_t)line.substring(0, bar).toInt(), line.substring(bar + 1)});
  }
  if (f) f.close();
  while (events.size() > KEEP) events.erase(events.begin());

  pending = firmwarePendingVerify();
  bootText = String("Avvio: ") + resetReasonText() + " - firmware " + FIRMWARE_COMMIT;
  if (pending) bootText += " (nuovo, in prova)";
  // A firmware that didn't start and was replaced by the previous one.
  const esp_partition_t *bad = esp_ota_get_last_invalid_partition();
  if (bad) {
    const String mark = String("rollback@") + bad->address;
    bool seen = false;
    for (const Event &e : events) seen |= e.text.endsWith(mark);
    if (!seen) bootText += " - il firmware nuovo non partiva: tornato al precedente " + mark;
  }
}

void eventsLoop() {
  const uint32_t ms = millis();
  // The start is logged once the clock is set (so it has a date), or after
  // a minute without.
  if (!started && (now() || ms > 60000)) {
    started = true;
    logEvent(bootText);
  }
  if (pending && ms > CONFIRM_MS && WiFi.status() == WL_CONNECTED) {
    pending = false;
    esp_ota_mark_app_valid_cancel_rollback();
    logEvent("Firmware nuovo confermato dopo un minuto senza problemi");
  }
  // WiFi lost for more than half a minute, and back.
  static uint32_t lostAt = 0;
  static bool reported = false;
  if (WiFi.status() != WL_CONNECTED) {
    if (!lostAt) lostAt = ms;
    if (!reported && ms - lostAt > 30000) {
      reported = true;
      logEvent("Wi-Fi perso");
    }
  } else {
    if (reported) logEvent("Wi-Fi ritrovato dopo " + String((ms - lostAt) / 1000) + " s");
    lostAt = 0;
    reported = false;
  }
}

String eventsJson() {
  String json = "[";
  for (int i = (int)events.size() - 1; i >= 0; i--) {
    String t = events[i].text;
    t.replace("\\", "\\\\");
    t.replace("\"", "\\\"");
    json += String(i == (int)events.size() - 1 ? "" : ",") + "[" + events[i].when + ",\"" + t + "\"]";
  }
  return json + "]";
}
