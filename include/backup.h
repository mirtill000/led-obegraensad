#pragma once

#include <Arduino.h>

// All the lamp's settings as one JSON file, to download and restore: every
// key of its NVS namespace (so settings added later are included without
// touching this code) plus the added quotes. Drawings (the gallery) are not
// included.
String settingsBackup();
// Restores a backup made by settingsBackup(): replaces all settings and the
// quotes. Returns nullptr on success (the caller restarts the lamp), else
// why not.
const char *restoreSettings(const String &json);
