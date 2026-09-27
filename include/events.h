#pragma once

#include <Arduino.h>

// A short log of what happened to the lamp - starts and why, updates,
// rollbacks, restored settings, WiFi lost for a while - kept in flash
// (/events.log, the last 30) for the diagnostics page. Call from loop()
// only (it writes to the file system).
void eventsBegin();  // after the file system is mounted
void eventsLoop();   // logs the start once the clock is set; confirms a new firmware
void logEvent(const String &text);
String eventsJson();  // [[epoch, "text"], ...], newest first

// Why the lamp last started, in words ("watchdog (blocco)", ...).
const char *resetReasonText();

// A new firmware waits for this before it is kept (see events.cpp).
bool firmwarePendingVerify();
