#pragma once

#include <time.h>

// Starts syncing the clock over NTP (needs an internet connection), in the
// time zone from settings.
void startTimeSync();
// Switches to settings.timezone right away (after it changed on the page).
void applyTimezone();
// Fills `out` with the local time; false until the first NTP sync.
bool localTime(struct tm &out);
