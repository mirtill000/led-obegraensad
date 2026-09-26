#pragma once

#include <Arduino.h>

// Bluetooth LE remote control (see src/ble.cpp and cardputer/): a GATT
// service to drive the lamp from a paired device - modes, games (keys and
// demo), text, notifications, brightness - with the panel's picture and a
// short state pushed back. Pairing needs the 6-digit PIN shown on the page.
void bleBegin();  // once, after WiFi (does nothing if settings.bleOn is off)
void bleLoop();   // from loop(): runs the commands received, sends updates

bool bleConnected();
// Forgets every paired remote and picks a new PIN (takes effect on restart).
void bleForgetRemotes();

// UUIDs (shared with cardputer/src/protocol.h).
#define BLE_SERVICE_UUID "8f3e0000-5c1a-4a6b-9b8e-0b5e6a1d0bea"
#define BLE_COMMAND_UUID "8f3e0001-5c1a-4a6b-9b8e-0b5e6a1d0bea"  // write: one command
#define BLE_STATE_UUID "8f3e0002-5c1a-4a6b-9b8e-0b5e6a1d0bea"    // read/notify: short JSON
#define BLE_FRAME_UUID "8f3e0003-5c1a-4a6b-9b8e-0b5e6a1d0bea"    // read/notify: 128 bytes
#define BLE_CATALOG_UUID "8f3e0004-5c1a-4a6b-9b8e-0b5e6a1d0bea"  // read: modes, games, animations
