#pragma once

#include <time.h>

// Moon phase from the date alone (mean synodic month from a known new
// moon): 0 = new, 0.25 = first quarter, 0.5 = full, 0.75 = last quarter.
float moonPhase(time_t when);
// Italian name of the phase ("luna piena", ...).
const char *moonPhaseName(float phase);
// Lit fraction of the disc, 0-1.
float moonIllumination(float phase);
