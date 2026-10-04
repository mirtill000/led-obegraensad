#pragma once

#include "animation.h"
#include "modes.h"

// "Animaletto": a Tamagotchi-style pet living on the panel. It hatches from
// an egg into a little frog ("ragazzo"), gets hungry and bored, sleeps
// at night (22-7), leaves droppings and falls ill if neglected. Time runs
// even while another mode is shown or the lamp is off: when it comes back
// the pet catches up on the hours it missed (up to three days). It never
// dies; a neglected pet is just sick and sad until you look after it.
//
// Care comes as game keys (page buttons, arrows, the Cardputer):
//   L pappa, R gioca, U pulisci, D medicina, A coccole
// The state is saved in NVS (key "pet", so it is in the settings backup).
class PetMode : public Mode {
 public:
  const char *id() const override { return "pet"; }
  const char *name() const override { return "Animaletto"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Dai la pappa"; }
  void action() override { input('L'); }
  bool hasSpeed() const override { return false; }
  bool input(char key) override;
  const GameControls *controls() const override { return keys(); }
  String status() const override;
  static const GameControls *keys();

  struct Status {
    String name, stage, mood;  // e.g. "Pixel", "Piccolo", "Ha fame"
    uint8_t food, joy, energy;  // 0-100
    uint8_t poops;
    bool sick, asleep;
    uint32_t ageHours;
  };
  static Status info();
  static void rename(const String &name);
  static void reset();  // a new egg
  // Brings the pet up to date (called every minute from loop, shown or not).
  static void tickClock();
};
