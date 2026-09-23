#pragma once

#include "modes.h"

// Pomodoro timer: work and break periods in turn (settings.pomodoroWork /
// pomodoroBreak minutes). The panel drains as time passes, with the minutes
// left on top (seconds in the last minute); it flashes when a period ends
// and the next one starts by itself. Started, paused and reset from the
// page.
class PomodoroMode : public Mode {
 public:
  const char *id() const override { return "pomodoro"; }
  const char *name() const override { return "Pomodoro"; }
  void start() override { lastDraw_ = 0; }
  void update(uint32_t now) override;
  const char *actionName() const override { return running_ ? "Pausa" : "Avvia"; }
  void action() override { running_ ? pause() : resume(); }
  bool hasSpeed() const override { return false; }

  void resume();
  void pause();
  void reset();
  bool running() const { return running_; }
  bool onBreak() const { return onBreak_; }
  uint32_t remainingMs() const;

 private:
  uint32_t periodMs() const;

  bool running_ = false;
  bool onBreak_ = false;
  uint32_t elapsedMs_ = 0;  // in the current period, while paused
  uint32_t resumedAt_ = 0;  // millis() when last started
  uint32_t flashUntil_ = 0;
  uint32_t lastDraw_ = 0;
};

PomodoroMode &pomodoroMode();
