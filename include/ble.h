#pragma once

#include <Arduino.h>

// Bluetooth LE remote control (see src/ble.cpp and cardputer/): a GATT
// service to drive the lamp from a paired device - modes, games (keys and
// demo), text, notifications, brightness - with the panel's picture and a
// short state pushed back. Pairing needs the 6-digit PIN shown on the page.
void bleBegin();  // once, after WiFi (does nothing if settings.bleOn is off)
void bleLoop();   // from loop(): runs the commands received, sends updates

bool bleConnected();
// The scene catalog changed (a drawing saved or deleted): remotes read the
// new one the next time they connect.
void bleCatalogChanged();
// Forgets every paired remote and picks a new PIN (takes effect on restart).
void bleForgetRemotes();

// UUIDs and commands: include/remote_protocol.h (shared with cardputer/).
#include "remote_protocol.h"
