#pragma once

#include "modes.h"

// Wake-up light: from settings.alarmRamp minutes before the alarm a sun
// rises from the bottom of the panel and the brightness slowly goes up;
// at the alarm time it is full and stays for settings.alarmHold minutes,
// rays pulsing. Shown by the mode selection (it wins over everything else)
// while alarmProgress() >= 0; not listed on the page as a mode.
class SunriseMode : public Mode {
 public:
  const char *id() const override { return "sunrise"; }
  const char *name() const override { return "Sveglia"; }
  void update(uint32_t now) override;
  const char *actionName() const override { return "Spegni la sveglia"; }
  void action() override { dismiss(); }
  bool hasSpeed() const override { return false; }
  bool hidden() const override { return true; }

  // -1 when no alarm is running; otherwise 0-1 while the sun rises, and
  // exactly 1 after the alarm time until the hold time is over.
  static float alarmProgress();
  // Brightness for the current progress (1-255).
  static uint8_t brightness(float progress);
  // Stops the alarm that is running (until the next one).
  static void dismiss();
  // One-minute preview of the sunrise, then 20 s at full light.
  static void test();
};
