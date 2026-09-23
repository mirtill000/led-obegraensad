#include "moon.h"

#include <math.h>

float moonPhase(time_t when) {
  static const double SYNODIC = 29.530588853;       // days
  static const double NEW_MOON_2000 = 947182440.0;  // 2000-01-06 18:14 UTC
  double days = (when - NEW_MOON_2000) / 86400.0;
  double phase = fmod(days / SYNODIC, 1.0);
  if (phase < 0) phase += 1;
  return (float)phase;
}

float moonIllumination(float phase) { return (1 - cosf(phase * 2 * (float)M_PI)) / 2; }

const char *moonPhaseName(float phase) {
  static const char *const NAMES[] = {"luna nuova",     "luna crescente", "primo quarto", "gibbosa crescente",
                                      "luna piena",     "gibbosa calante", "ultimo quarto", "luna calante"};
  return NAMES[(int)floorf(phase * 8 + 0.5f) % 8];
}
