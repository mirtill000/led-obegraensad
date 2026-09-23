#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "constants.h"
#include "display.h"
#include "modes.h"
#include "settings.h"
#include "timekeeping.h"
#include "web.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

// Joins the home network, or opens the lamp's own access point if that
// fails. Returns the address the control page is reachable at.
static String startWifi() {
  WiFi.setHostname(HOSTNAME);
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("Connecting to %s", WIFI_SSID);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.setAutoReconnect(true);
      startTimeSync();
      return WiFi.localIP().toString();
    }
    Serial.println("WiFi connection failed, starting access point");
    WiFi.disconnect(true);
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  return WiFi.softAPIP().toString();
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
  display.begin();
  display.setBrightness(settings.brightness);

  const String ip = startWifi();
  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  webBegin();
  Serial.printf("Control page: http://%s  (http://%s.local)\n", ip.c_str(), HOSTNAME);

  // Show where to find the control page, then start the saved mode.
  display.scrollTextOnce(ip.c_str(), 60);
  if (!setMode(settings.mode)) setMode(MODES[0]->id());
}

void loop() {
  webLoop();
  checkButton();
  updateMode();
}
