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

void sunPosition(time_t when, float latitude, float longitude, float &azimuth, float &elevation) {
  const double rad = M_PI / 180;
  const double n = when / 86400.0 + 2440587.5 - 2451545.0;  // days since J2000
  const double L = fmod(280.460 + 0.9856474 * n, 360);
  const double g = fmod(357.528 + 0.9856003 * n, 360) * rad;
  const double lambda = (L + 1.915 * sin(g) + 0.020 * sin(2 * g)) * rad;
  const double eps = (23.439 - 0.0000004 * n) * rad;
  const double ra = atan2(cos(eps) * sin(lambda), cos(lambda));
  const double dec = asin(sin(eps) * sin(lambda));
  const double gmst = fmod(280.46061837 + 360.98564736629 * n, 360);
  const double H = (gmst + longitude) * rad - ra;  // hour angle
  const double lat = latitude * rad;
  elevation = asin(sin(lat) * sin(dec) + cos(lat) * cos(dec) * cos(H));
  double az = atan2(-sin(H) * cos(dec), sin(dec) * cos(lat) - cos(dec) * sin(lat) * cos(H));
  if (az < 0) az += 2 * M_PI;
  azimuth = az;
}
