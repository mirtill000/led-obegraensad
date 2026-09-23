#include "settings.h"

#include <Preferences.h>

#include "constants.h"

Settings settings;

static Preferences prefs;

void loadSettings() {
  prefs.begin("obegransad", true);
  settings.mode = prefs.getString("mode", "text");
  settings.text = prefs.getString("text", MESSAGE);
  settings.brightness = prefs.getUChar("brightness", 255);
  settings.speedMs = prefs.getUShort("speed", SCROLL_DELAY_MS);
  settings.latitude = prefs.getFloat("lat", DEFAULT_LATITUDE);
  settings.longitude = prefs.getFloat("lon", DEFAULT_LONGITUDE);
  settings.ambient = prefs.getString("ambient", "auto");
  prefs.end();
}

void saveSettings() {
  prefs.begin("obegransad", false);
  prefs.putString("mode", settings.mode);
  prefs.putString("text", settings.text);
  prefs.putUChar("brightness", settings.brightness);
  prefs.putUShort("speed", settings.speedMs);
  prefs.putFloat("lat", settings.latitude);
  prefs.putFloat("lon", settings.longitude);
  prefs.putString("ambient", settings.ambient);
  prefs.end();
}
