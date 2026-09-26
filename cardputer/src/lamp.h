#pragma once

#include <Arduino.h>

#include <vector>

// The Bluetooth link to the lamp: finds it, pairs with the PIN shown on the
// lamp's web page (remembered afterwards), keeps the lamp's state and
// picture up to date and sends it commands. Call loop() often; connecting
// blocks it for a few seconds at a time.
namespace lamp {

enum class Status : uint8_t {
  NeedPin,     // no PIN yet (or it was wrong): ask for it, then setPin()
  Searching,   // scanning for the lamp
  Connecting,  // found it, connecting and pairing
  Ready,       // connected: commands go through
};

struct Item {
  char kind;  // 'M' mode, 'G' game, 'A' animation
  String id, name;
};

void begin();
void loop();
Status status();
// Last problem, for the screen ("" if none).
const String &message();

// Pairing PIN (6 digits); 0 = unknown.
uint32_t pin();
void setPin(uint32_t pin);
// Forgets the lamp: PIN and pairing, back to NeedPin.
void forget();

// Sends one command (see ../../src/ble.cpp); false if not connected.
bool send(const String &command);

// What the lamp reported last.
struct State {
  String mode, modeName, button, game, gameName, time;
  bool demo = true, demoForced = false;
  int brightness = 255;
};
State state();  // a thread-safe copy (read from the UI task)
// Copies the panel (256 levels 0-15, row by row) into `out`; the return
// value changes with each new picture.
uint32_t frameSnapshot(uint8_t out[256]);
uint32_t frameVersion();
std::vector<Item> catalog();  // a thread-safe copy

}  // namespace lamp
