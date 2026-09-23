#include "modes/sunrise_mode.h"

#include <math.h>

#include "display.h"
#include "gfx.h"
#include "settings.h"
#include "timekeeping.h"

static const int WEEK = 7 * 24 * 60;
static int32_t dismissedWindow = -1;  // week-minute when the dismissed alarm rings
static uint32_t testStart = 0;
static bool testing = false;

float SunriseMode::alarmProgress() {
  if (testing) {
    const float t = (millis() - testStart) / 1000.0f;
    if (t < 60) return t / 60;
    if (t < 80) return 1;
    testing = false;
  }
  struct tm t;
  if (!settings.alarmOn || !localTime(t)) return -1;
  // Minutes since Monday 00:00; an alarm window may cross midnight or the
  // end of the week, so compare modulo a week.
  const int weekday = (t.tm_wday + 6) % 7;  // Monday = 0
  const int now = weekday * 1440 + t.tm_hour * 60 + t.tm_min;
  const float nowF = now + t.tm_sec / 60.0f;
  for (int d = 0; d < 7; d++) {
    if (!(settings.alarmDays & (1 << d))) continue;
    const int ring = d * 1440 + settings.alarmTime;
    const float sinceRing = fmodf(nowF - ring + WEEK * 2, WEEK);  // minutes after the ring, 0..WEEK
    const float before = WEEK - sinceRing;                        // minutes before it
    if (dismissedWindow == ring) {
      if (sinceRing < settings.alarmHold || before <= settings.alarmRamp) continue;
      dismissedWindow = -1;  // that alarm is over
    }
    if (sinceRing < settings.alarmHold) return 1;
    if (before <= settings.alarmRamp) return 1 - before / fmaxf(1, settings.alarmRamp);
  }
  return -1;
}

uint8_t SunriseMode::brightness(float progress) {
  const float b = 1 + 254 * progress * progress;  // slow at first, like dawn
  return (uint8_t)fminf(255, b);
}

void SunriseMode::dismiss() {
  testing = false;
  struct tm t;
  if (!localTime(t)) return;
  const int weekday = (t.tm_wday + 6) % 7;
  const int now = weekday * 1440 + t.tm_hour * 60 + t.tm_min;
  // Remember which alarm was stopped: the one whose window we are in.
  for (int d = 0; d < 7; d++) {
    const int ring = d * 1440 + settings.alarmTime;
    const int since = ((now - ring) % WEEK + WEEK) % WEEK;
    if (since < settings.alarmHold || WEEK - since <= settings.alarmRamp) dismissedWindow = ring;
  }
}

void SunriseMode::test() {
  testing = true;
  testStart = millis();
}

void SunriseMode::update(uint32_t now) {
  static uint32_t lastDraw = 0;
  if (now - lastDraw < 50) return;
  lastDraw = now;
  const float p = fmaxf(0, alarmProgress());

  display.clear();
  // Sky: a glow near the horizon that grows with the sunrise.
  for (int y = 0; y < ROWS; y++) {
    const float glow = p * fmaxf(0, (y - 4) / 11.0f);
    for (int x = 0; x < COLS; x++) gfx::plot(x, y, glow * 0.35f);
  }
  // Sun: rising from below the bottom edge to the middle, growing.
  const float cy = 19 - 11 * p, r = 3 + 2.5f * p;
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      const float d = sqrtf((x - 7.5f) * (x - 7.5f) + (y - cy) * (y - cy));
      gfx::plot(x, y, fminf(1, r - d + 0.5f));
    }
  }
  // Once it's time: pulsing rays.
  if (p >= 1) {
    const float pulse = 0.5f + 0.5f * sinf(now / 300.0f);
    for (int i = 0; i < 8; i++) {
      const float a = i * PI / 4 + now / 4000.0f;
      gfx::line(7.5f + cosf(a) * (r + 1), cy + sinf(a) * (r + 1), 7.5f + cosf(a) * (r + 3), cy + sinf(a) * (r + 3),
                0.4f + 0.6f * pulse);
    }
  }
  display.render();
}
