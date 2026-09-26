#pragma once

#include <stdint.h>

// Control page on port 80: pick a mode, edit the scrolling text, set
// brightness and speed. Call webBegin() once WiFi is up, webLoop() from
// loop().
void webBegin();
void webLoop();

// Pages connected for live updates (Server-Sent Events on port 81).
int liveClients();

// Called by loop() with how long its last round took, for the diagnostics
// (rounds per second and the longest one: a long one froze the panel).
void noteLoopTime(uint32_t us);
