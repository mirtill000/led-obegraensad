#include "modes.h"

#include "modes/ambient_mode.h"
#include "modes/clock_mode.h"
#include "modes/life_mode.h"
#include "modes/off_mode.h"
#include "modes/quotes_mode.h"
#include "modes/text_mode.h"
#include "settings.h"

static TextMode textMode;
static QuotesMode quotesMode;
static ClockMode clockMode;
static LifeMode lifeMode;
static AmbientMode ambientMode;
static OffMode offMode;

Mode *const MODES[] = {&textMode, &quotesMode, &clockMode, &lifeMode, &ambientMode, &offMode};
const uint8_t MODE_COUNT = sizeof(MODES) / sizeof(MODES[0]);

static uint8_t current = 0;

Mode *currentMode() { return MODES[current]; }

bool setMode(const String &id) {
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    if (id == MODES[i]->id()) {
      current = i;
      settings.mode = id;
      restartMode();
      return true;
    }
  }
  return false;
}

void nextMode() { setMode(MODES[(current + 1) % MODE_COUNT]->id()); }

void updateMode() { MODES[current]->update(millis()); }

void restartMode() { MODES[current]->start(); }
