#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "constants.h"
#include "display.h"
#include "gallery.h"
#include "modes.h"
#include "net.h"
#include "settings.h"
#include "timekeeping.h"
#include "web.h"

static const uint32_t WATCHDOG_S = 20;

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h: copy include/secrets.example.h and fill in your WiFi network"
#endif

// Joins the home network, retrying until it succeeds; meanwhile the panel
// shows that it is waiting for WiFi. Returns the lamp's IP address.
static String startWifi() {
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);  // also rejoins by itself if WiFi drops later
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to %s\n", WIFI_SSID);

  uint32_t lastAttempt = millis();
  while (WiFi.status() != WL_CONNECTED) {
    display.scrollTextOnce("wifi...", 60);
    if (WiFi.status() != WL_CONNECTED && millis() - lastAttempt >= WIFI_RETRY_MS) {
      Serial.println("Still not connected, retrying");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastAttempt = millis();
    }
  }
  startTimeSync();
  return WiFi.localIP().toString();
}

// Optional push button to GND on PIN_BUTTON: each press switches to the
// next mode. Without a button the pin just stays high (internal pull-up),
// and everything is controlled from the web page.
static void checkButton() {
  static bool lastState = HIGH;
  static uint32_t lastChange = 0;
  const bool state = digitalRead(PIN_BUTTON);
  if (state != lastState && millis() - lastChange > 50) {
    lastChange = millis();
    lastState = state;
    if (state == LOW) {
      nextMode();
      saveSettings();
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  loadSettings();
  galleryBegin();
  loadQuotes();
  display.begin();
  display.setBrightness(settings.brightness);
  display.setRotation(rotationForSettings());
  display.setTransition(transitionForSettings());
  Display::setScrollFont(fontForSettings());
  applyTimezone();

  const String ip = startWifi();
  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  webBegin();
  startNetTask();
  Serial.printf("Control page: http://%s  (http://%s.local)\n", ip.c_str(), HOSTNAME);

  // Show where to find the control page, then start the saved mode.
  display.scrollTextOnce(ip.c_str(), 60);
  refreshModes();

  // Watchdog: if loop() ever gets stuck for WATCHDOG_S, restart instead of
  // staying frozen (the diagnostics then show "watchdog" as the last
  // restart's reason).
  const esp_task_wdt_config_t wdt = {.timeout_ms = WATCHDOG_S * 1000, .idle_core_mask = 1, .trigger_panic = true};
  esp_task_wdt_reconfigure(&wdt);
  enableLoopWDT();
}

void loop() {
  const uint32_t start = micros();
  webLoop();
  checkButton();
  updateMode();
  noteLoopTime(micros() - start);
}
