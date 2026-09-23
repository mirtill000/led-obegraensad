#include "timekeeping.h"

#include <Arduino.h>

#include "constants.h"

void startTimeSync() { configTzTime(TIMEZONE, "pool.ntp.org", "time.google.com"); }

bool localTime(struct tm &out) { return getLocalTime(&out, 0); }
