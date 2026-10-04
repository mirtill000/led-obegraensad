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
  char kind;  // 'M' mode, 'G' game, 'A' animation, 'D' drawing
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
  String keys, actionKey;  // the game's keys (of LRUDA) and what A does
  String labels;           // "L|R|U|D|A" names, "" = the plain arrow
  int players = 1;         // 2: a two-player game (Tron)
  String status;           // one line about what the mode shows
  String label(int i) const {  // 0-4, in LRUDA order
    int start = 0;
    for (int k = 0; k < i; k++) {
      start = labels.indexOf('|', start) + 1;
      if (start <= 0) return "";
    }
    const int end = labels.indexOf('|', start);
    return labels.substring(start, end < 0 ? labels.length() : end);
  }
  bool demo = true, demoForced = false;
  int brightness = 255;
};
State state();  // a thread-safe copy (read from the UI task)
// Copies the panel (256 levels 0-15, row by row) into `out`; the return
// value changes with each new picture.
uint32_t frameSnapshot(uint8_t out[256]);
uint32_t frameVersion();
std::vector<Item> catalog();  // a thread-safe copy

// The lamp's settings a remote may change (its "settings" characteristic;
// empty with an older lamp firmware). Change one with send("o name value").
struct Setting {
  String name, value, label;
  char kind;                          // 'B' on/off, 'C' choice, 'N' number
  std::vector<String> choices, names;  // 'C': values and their names
  long min, max;                      // 'N'
  String shown() const;               // the value as the lamp's page names it
};
std::vector<Setting> settings();  // a thread-safe copy

}  // namespace lamp
