#include "modes/pomodoro_mode.h"

#include "display.h"
#include "settings.h"

uint32_t PomodoroMode::periodMs() const {
  return (onBreak_ ? settings.pomodoroBreak : settings.pomodoroWork) * 60000UL;
}

uint32_t PomodoroMode::remainingMs() const {
  const uint32_t elapsed = elapsedMs_ + (running_ ? millis() - resumedAt_ : 0);
  return elapsed >= periodMs() ? 0 : periodMs() - elapsed;
}

void PomodoroMode::resume() {
  if (running_) return;
  running_ = true;
  resumedAt_ = millis();
}

void PomodoroMode::pause() {
  if (!running_) return;
  elapsedMs_ += millis() - resumedAt_;
  running_ = false;
}

void PomodoroMode::reset() {
  running_ = false;
  onBreak_ = false;
  elapsedMs_ = 0;
  flashUntil_ = 0;
}

void PomodoroMode::update(uint32_t now) {
  if (running_ && remainingMs() == 0) {
    // Period over: flash, then the next one starts by itself.
    onBreak_ = !onBreak_;
    elapsedMs_ = 0;
    resumedAt_ = now;
    flashUntil_ = now + 2400;
  }
  if (now - lastDraw_ < 100) return;
  lastDraw_ = now;

  display.clear();
  if ((int32_t)(flashUntil_ - now) > 0) {
    const bool on = ((flashUntil_ - now) / 400) % 2;
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) display.setLevel(x, y, on ? 255 : 0);
    }
    display.render();
    return;
  }

  // Time left as lit pixels, draining from the last one: work fills from
  // the top, a break from the bottom and dimmer.
  const uint32_t left = remainingMs();
  const float filled = (float)left / periodMs() * TOTAL_PIXELS;
  for (int i = 0; i < TOTAL_PIXELS; i++) {
    const float f = filled - i;
    if (f <= 0) break;
    const int x = i % COLS, y = onBreak_ ? ROWS - 1 - i / COLS : i / COLS;
    display.setLevel(x, y, (onBreak_ ? 22 : 40) * fminf(f, 1));
  }

  // Minutes left (rounded up), or seconds in the last minute; blinking while
  // paused.
  const bool blinkOff = !running_ && (now / 500) % 2 && elapsedMs_ > 0;
  if (!blinkOff) {
    const uint32_t seconds = (left + 999) / 1000;
    const String s = seconds >= 60 ? String((seconds + 59) / 60) : String(seconds) + "s";
    const int w = Display::textWidth(s.c_str(), 0, s.length()) - 1;
    const int x0 = (COLS - w) / 2, y0 = 5;
    // Clear a box behind the digits so they read over the fill.
    for (int y = y0 - 1; y < y0 + 7; y++) {
      for (int x = x0 - 1; x <= x0 + w; x++) display.setLevel(x, y, 0);
    }
    display.drawText(x0, y0, s.c_str(), 0, s.length());
  }
  display.render();
}
