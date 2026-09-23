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
  prefs.end();
}

void saveSettings() {
  prefs.begin("obegransad", false);
  prefs.putString("mode", settings.mode);
  prefs.putString("text", settings.text);
  prefs.putUChar("brightness", settings.brightness);
  prefs.putUShort("speed", settings.speedMs);
  prefs.end();
}
