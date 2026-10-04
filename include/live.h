#pragma once

#include <Arduino.h>

#include "constants.h"

// What every live client gets, in the same format whatever the link: the
// page (Server-Sent Events on port 81, see web.cpp) and the Cardputer
// (Bluetooth notifications, see ble.cpp). Described in remote_protocol.h.

// The panel as seen: 256 levels 0-15, two pixels per byte (high nibble
// first), row by row from the top-left.
static const int LIVE_FRAME_BYTES = TOTAL_PIXELS / 2;
void packedFrame(uint8_t out[LIVE_FRAME_BYTES]);
// The same as 256 hex digits (for text channels: SSE, /api/frame).
void packedFrameHex(char out[LIVE_FRAME_BYTES * 2 + 1]);

// The short state: {"m":mode,"mn":name,"x":button,"g":game,"gn":game name,
// "d":demo,"f":demo forced,"c":keys,"ca":what A does,"cl":labels,"n":players,
// "b":brightness,"t":"HH:MM"}.
String summaryJson();

// The scene catalog (catalog.h) for remotes: lines "M|A|G|D <tab> id <tab>
// name" - modes, animations, games, drawings; the id without its "a/",
// "g/" or "d/".
String catalogText();
