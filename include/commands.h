#pragma once

#include <Arduino.h>

// Runs one remote command (see remote_protocol.h), from Bluetooth, the web
// API or the page. Returns nullptr when done, else why not (in Italian, for
// the page). Must be called from loop(), like everything that changes modes.
const char *runCommand(const String &command);

struct SettingDef;
// Applies what changed settings touch (see SettingEffect), once for all of
// them: the page's POST /api/settings and the command "o".
void applySettingEffects(const SettingDef *const *changed, int count);
