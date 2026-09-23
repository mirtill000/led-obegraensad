#pragma once

#include <time.h>

// Starts syncing the clock over NTP (needs an internet connection).
void startTimeSync();
// Fills `out` with the local time; false until the first NTP sync.
bool localTime(struct tm &out);
