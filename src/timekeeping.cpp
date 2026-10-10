#include "timekeeping.h"

#include <Arduino.h>

#include "settings.h"

void startTimeSync() { configTzTime(settings.timezone.c_str(), "pool.ntp.org", "time.google.com"); }

void applyTimezone() {
  setenv("TZ", settings.timezone.c_str(), 1);
  tzset();
}

// Not getLocalTime(&out, 0): with a zero timeout it checks the clock only
// if millis() hasn't ticked since it started - when it has (now and then,
// at random) it says "no time" without even looking. Every caller then
// took the clock as unset for that moment: the clock face blanked for a
// frame, the time slot's playlist and the night schedule let go for a
// second, the sun-following brightness jumped to full - the lamp's image
// going on and off "a intermittenza".
bool localTime(struct tm &out) {
  const time_t now = time(nullptr);
  if (now < 1483228800) return false;  // before 2017: not synced yet
  localtime_r(&now, &out);
  return true;
}
