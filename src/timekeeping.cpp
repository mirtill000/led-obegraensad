#include "timekeeping.h"

#include <Arduino.h>

#include "settings.h"

void startTimeSync() { configTzTime(settings.timezone.c_str(), "pool.ntp.org", "time.google.com"); }

void applyTimezone() {
  setenv("TZ", settings.timezone.c_str(), 1);
  tzset();
}

bool localTime(struct tm &out) { return getLocalTime(&out, 0); }
