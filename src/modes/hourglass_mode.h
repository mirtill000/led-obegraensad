#pragma once

#include "modes.h"

// "Clessidra": a sand timer. About fifty grains of sand, each a pixel,
// with real falling-sand physics: they pile up, slide down the slopes and
// leave a crater in the top bulb. The neck lets one grain through at a
// time, at the pace that empties the top in settings.hourglassMinutes.
// "Gira" turns it over (the sand that's already down goes back up, so the
// time left is what has run so far, like a real one); at the end the sand
// pulses for a few seconds.
class HourglassMode : public Mode {
 public:
  const char *id() const override { return "hourglass"; }
  const char *name() const override { return "Clessidra"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Gira"; }
  void action() override;
  bool hasSpeed() const override { return false; }

  // Seconds of sand left in the top bulb (0 when it has run out), and
  // whether it is still running.
  String status() const override {
    const uint32_t s = secondsLeft();
    return running() ? "Resta " + String(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + String(s % 60) : String("Tempo scaduto");
  }
  static uint32_t secondsLeft();
  static bool running();
};
