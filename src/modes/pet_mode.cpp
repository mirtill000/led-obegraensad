#include "modes/pet_mode.h"

#include <Preferences.h>
#include <math.h>
#include <time.h>

#include "display.h"
#include "sound.h"
#include "sprite_atlas.h"
#include "timekeeping.h"

// ---------------------------------------------------------------------------
// The pet's life, simulated a minute at a time.

namespace {

enum Stage : uint8_t { EGG, FROG };

// Saved as one NVS blob; `version` guards against an older layout.
struct Life {
  uint8_t version = 1;
  float food = 80, joy = 80, energy = 90;  // 0-100
  uint8_t poops = 0;
  bool sick = false, napping = false, ate = false;
  uint16_t neglect = 0;    // minutes in a row of hunger, dirt or sadness
  uint16_t poopTimer = 0;  // minutes since the last dropping
  uint32_t ageMin = 0;     // minutes lived (the egg included)
  uint32_t lastEpoch = 0;  // wall clock of the last simulated minute, 0 = unknown
  char name[16] = "Pixel";
};

const uint32_t HATCH_MIN = 5;
const uint32_t MAX_CATCH_UP_MIN = 3 * 24 * 60;
const uint16_t SICK_AFTER_MIN = 120;
const uint16_t POOP_EVERY_MIN = 180;
const uint8_t MAX_POOPS = 4;

Life life;
bool loaded = false, dirty = false;
uint32_t lastTickMs = 0, lastSaveMs = 0, pendingMs = 0;

Stage stage() {
  return life.ageMin < HATCH_MIN ? EGG : FROG;
}

void load() {
  if (loaded) return;
  loaded = true;
  Preferences prefs;
  prefs.begin("obegransad", true);
  Life saved;
  if (prefs.getBytesLength("pet") == sizeof(Life) && prefs.getBytes("pet", &saved, sizeof(Life)) == sizeof(Life) &&
      saved.version == 1) {
    saved.name[sizeof(saved.name) - 1] = 0;
    life = saved;
  }
  prefs.end();
}

void save() {
  Preferences prefs;
  prefs.begin("obegransad", false);
  prefs.putBytes("pet", &life, sizeof(Life));
  prefs.end();
  dirty = false;
  lastSaveMs = millis();
}

bool nightHour(int hour) { return hour >= 22 || hour < 7; }

bool asleepAt(int hour) { return stage() != EGG && (life.napping || (hour >= 0 && nightHour(hour))); }

// `hour` is the local hour of that minute, or -1 if the clock isn't set.
void liveOneMinute(int hour) {
  life.ageMin++;
  if (stage() == EGG) return;
  const bool asleep = asleepAt(hour);
  life.food -= asleep ? 3.0f / 60 : 8.0f / 60;
  life.joy -= (asleep ? 1.0f : 6.0f) / 60 + life.poops * 2.0f / 60 + (life.sick ? 4.0f / 60 : 0);
  life.energy += asleep ? (life.napping ? 30.0f : 14.0f) / 60 : -5.0f / 60;
  life.food = constrain(life.food, 0.0f, 100.0f);
  life.joy = constrain(life.joy, 0.0f, 100.0f);
  life.energy = constrain(life.energy, 0.0f, 100.0f);
  // A nap when exhausted, until rested.
  if (!life.napping && life.energy < 8) life.napping = true;
  if (life.napping && life.energy >= 50) life.napping = false;
  if (!asleep && life.ate && ++life.poopTimer >= POOP_EVERY_MIN) {
    life.poopTimer = 0;
    life.ate = false;
    if (life.poops < MAX_POOPS) life.poops++;
  }
  const bool neglected = life.food < 5 || life.joy < 5 || life.poops >= 3;
  if (neglected) life.neglect++;
  else life.neglect = life.neglect > 2 ? life.neglect - 2 : 0;
  if (life.neglect >= SICK_AFTER_MIN) {
    life.sick = true;
    life.neglect = 0;
  }
  dirty = true;
}

int localHour(time_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  return tm.tm_hour;
}

bool clockHour(int &hour) {
  struct tm t;
  if (!localTime(t)) return false;
  hour = t.tm_hour;
  return true;
}

bool asleepNow() {
  int hour = -1;
  clockHour(hour);
  return asleepAt(hour);
}

// ---------------------------------------------------------------------------
// Drawing. The pet is a soft, fuzzy plush (after the felt dolls): its
// outline is a little dimmer and a faint fur shimmers just outside it.
// Its sprites are in the atlas (pet.egg, pet.frog...); the frog's marks:
// 'w' bright (its eye bumps), 'e' eye (dark; body when closed), 'm' mouth
// (dark; lit while chewing). Its rows 5-6 swap for the sad face.

const int FROG_MOUTH_ROW = 5;

const int GROUND = ROWS - 1;  // the floor row; the pet stands on the row above


enum Anim : uint8_t { NONE, FEED, PLAY, CLEAN, CURE, LOVE, REFUSE };
Anim anim = NONE;
uint32_t animStart = 0;
uint8_t animPoops = 0;  // droppings being swept away
const uint32_t ANIM_MS[] = {0, 2200, 2400, 1200, 1800, 1600, 800};

float petX = 4;
int targetX = 4;
uint32_t lastStep = 0, lastDraw = 0;

const Sprite &sprite() { return stage() == EGG ? spr::PET_EGG : spr::PET_FROG; }

// The sprite's character at (c, r), with the sad mouth swapped in.
char cell(const Sprite &s, int c, int r, bool happy) {
  if (&s == &spr::PET_FROG && !happy && (r == FROG_MOUTH_ROW || r == FROG_MOUTH_ROW + 1)) {
    r = r == FROG_MOUTH_ROW ? FROG_MOUTH_ROW + 1 : FROG_MOUTH_ROW;
  }
  return sprites::at(s, 0, c, r);
}

void drawPet(const Sprite &s, int x, int y, uint8_t level, bool eyesOpen, bool happy, bool chewing, uint32_t now) {
  for (int r = -1; r <= s.h; r++) {
    for (int c = -1; c <= s.w; c++) {
      const char ch = cell(s, c, r, happy);
      const bool edge = cell(s, c - 1, r, happy) == '.' || cell(s, c + 1, r, happy) == '.' ||
                        cell(s, c, r - 1, happy) == '.' || cell(s, c, r + 1, happy) == '.';
      const int px = x + c, py = y + r;
      if (ch == '.') {
        // The fur: a faint shimmer around the body (not over anything else).
        const bool nextToBody = cell(s, c - 1, r, happy) != '.' || cell(s, c + 1, r, happy) != '.' ||
                                cell(s, c, r - 1, happy) != '.';
        if (nextToBody && r < s.h && display.getLevel(px, py) == 0) {
          const uint32_t wave = (uint32_t)(px * 3 + py * 5) + now / 300;
          display.setLevel(px, py, (uint8_t)(level * (wave % 3 == 0 ? 0.22f : 0.12f)));
        }
        continue;
      }
      uint8_t l = 0;
      switch (ch) {
        case 'w': l = (uint8_t)min(255, level * 5 / 4); break;
        case 'e': l = eyesOpen ? 0 : level; break;
        case 'm': l = chewing ? level : 0; break;  // chewing: the mouth opens and closes
        default: l = (uint8_t)max(0, sprites::charLevel(ch, level)); break;
      }
      if (edge && (ch == '#' || ch == 'w')) l = l * 7 / 10;  // a soft felt outline
      display.setLevel(px, py, l);
    }
  }
}

void draw(uint32_t now) {
  display.clear();
  for (int x = 0; x < COLS; x++) display.setLevel(x, GROUND, 25);

  const Sprite &s = sprite();
  const bool asleep = asleepNow();
  const uint32_t t = anim != NONE ? now - animStart : 0;
  if (anim != NONE && t >= ANIM_MS[anim]) anim = NONE;

  // Droppings on the right, swept away by the broom line when cleaning.
  const int poops = anim == CLEAN ? animPoops : life.poops;
  const int sweep = anim == CLEAN ? (int)(t * (COLS + 2) / ANIM_MS[CLEAN]) - 1 : -1;
  for (int i = 0; i < poops; i++) {
    const int px = COLS - 3 - i * 4;
    if (px > sweep) sprites::draw(spr::PET_POOP, px, GROUND - 3, 0, 110);
  }
  if (anim == CLEAN) {
    for (int y = 0; y < GROUND; y++) display.setLevel(sweep, y, 200);
  }

  int x = (int)lroundf(petX), y = GROUND - s.h;
  if (stage() == EGG) {
    // It wobbles, more and more as it's about to hatch.
    const uint32_t period = life.ageMin + 1 >= HATCH_MIN ? 150 : 900;
    x = (COLS - s.w) / 2 + ((now / period) % 4 == 1 ? 1 : (now / period) % 4 == 3 ? -1 : 0);
    drawPet(s, x, y, 200, true, true, false, now);
    display.render();
    return;
  }
  if (anim == PLAY) y -= (int)lroundf(2.5f * fabsf(sinf(t * 3 * (float)M_PI / ANIM_MS[PLAY])));
  if (anim == REFUSE) x += (t / 100) % 2 ? 1 : -1;
  if (anim == NONE && !asleep && life.joy > 70 && (now / 700) % 6 == 0) y -= 1;  // a happy hop

  const bool happy = life.joy >= 45 && !life.sick && life.food >= 20;
  const bool blink = (now % 4000) < 150;
  const bool chewing = anim == FEED && t > 800 && (t / 200) % 2;
  uint8_t level = life.sick ? 110 : 200;
  if (asleep) level = 70;
  drawPet(s, x, y, level, !asleep && !blink, happy, chewing, now);

  switch (anim) {
    case FEED: {
      // The apple drops next to the mouth, then is eaten row by row.
      const int ax = x + s.w + 1 <= COLS - 5 ? x + s.w + 1 : x - 6;
      const int restY = GROUND - 5;
      const int ay = t < 800 ? -5 + (int)((restY + 5) * t / 800) : restY;
      const int eaten = t < 800 ? 0 : (int)((t - 800) * 6 / 1400);
      if (eaten < 5) sprites::drawFrom(spr::PET_APPLE, ax, ay, eaten, 0, 230);
      break;
    }
    case PLAY: {
      // A ball bouncing over the pet, from one side to the other.
      const float p = (float)t / ANIM_MS[PLAY];
      const int bx = (int)(p * (COLS - 2));
      const int by = GROUND - 2 - (int)lroundf(9 * fabsf(sinf(p * 3 * (float)M_PI)));
      for (int i = 0; i < 4; i++) display.setLevel(bx + i % 2, by + i / 2, 255);
      break;
    }
    case CURE:
      if ((t / 300) % 2 == 0) sprites::draw(spr::PET_CROSS, x + s.w / 2 - 2, max(0, y - 6));
      break;
    case LOVE: {
      const int hy = y - 5 - (int)(t * 8 / ANIM_MS[LOVE]);
      sprites::draw(spr::PET_HEART, x + s.w / 2 - 2, hy);
      break;
    }
    default: break;
  }

  if (asleep) {
    // A "z" floating up from its head.
    const int phase = (now / 600) % 4;
    sprites::draw(spr::PET_ZED, min(x + s.w, COLS - 4), max(0, y - 3 - phase), 0, 90 + 40 * phase);
  } else if (anim == NONE && (now / 1000) % 2) {
    // What it needs, blinking in the corner.
    if (life.sick) sprites::draw(spr::PET_CROSS, 0, 0, 0, 200);
    else if (life.food < 25) sprites::draw(spr::PET_APPLE, 0, 0, 0, 200);
    else if (life.joy < 25) sprites::draw(spr::PET_NOTE, 0, 0, 0, 200);
  }
  display.render();
}

void walk(uint32_t now) {
  if (now - lastStep < 380) return;
  lastStep = now;
  if (anim != NONE || asleepNow() || stage() == EGG) return;
  const int maxX = COLS - sprite().w;
  if ((int)petX == targetX) {
    if (esp_random() % 4 == 0) targetX = esp_random() % (maxX + 1);
    return;
  }
  petX += targetX > petX ? 1 : -1;
  petX = constrain(petX, 0.0f, (float)maxX);
}

}  // namespace

// ---------------------------------------------------------------------------

void PetMode::tickClock() {
  load();
  const uint32_t ms = millis();
  if (ms - lastTickMs < 1000 && lastTickMs) return;
  lastTickMs = ms;
  struct tm tm;
  if (localTime(tm)) {
    const time_t now = time(nullptr);
    if (!life.lastEpoch || now < (time_t)life.lastEpoch) life.lastEpoch = now;  // first time with a clock
    uint32_t minutes = (now - life.lastEpoch) / 60;
    if (minutes > MAX_CATCH_UP_MIN) {
      life.lastEpoch = now - MAX_CATCH_UP_MIN * 60;
      minutes = MAX_CATCH_UP_MIN;
    }
    for (uint32_t i = 0; i < minutes; i++) {
      life.lastEpoch += 60;
      liveOneMinute(localHour(life.lastEpoch));
    }
  } else if (!life.lastEpoch) {
    // Never had a clock: count minutes of uptime, no night. (With a saved
    // time the catch-up above covers this wait once the clock is set.)
    pendingMs += 1000;
    if (pendingMs >= 60000) {
      pendingMs = 0;
      liveOneMinute(-1);
    }
  }
  if (dirty && ms - lastSaveMs > 15 * 60000UL) save();
}

void PetMode::start() {
  load();
  anim = NONE;
  const int maxX = COLS - sprite().w;
  petX = targetX = maxX / 2;
  display.beginTransition();
  draw(millis());
}

void PetMode::update(uint32_t now) {
  if (now - lastDraw < 50) return;
  lastDraw = now;
  walk(now);
  if ((int)petX > COLS - (int)sprite().w) petX = COLS - sprite().w;  // it grew
  static Stage shown = stage();
  if (stage() != shown) {
    shown = stage();
    display.beginPageTransition(900);  // the egg hatches
  }
  draw(now);
}

bool PetMode::input(char key) {
  load();
  if (stage() == EGG) {
    anim = REFUSE;
    animStart = millis();
    sound::play(sound::NO);
    return true;
  }
  if (anim != NONE && millis() - animStart < ANIM_MS[anim]) return true;  // one thing at a time
  Anim next = REFUSE;
  if (life.napping && key != 'U') {
    // Woken from a nap: a grumpy shake, then it listens. (At night it
    // sleeps on.)
    life.napping = false;
    life.joy = max(0.0f, life.joy - 3);
  }
  const bool asleep = asleepNow();
  switch (key) {
    case 'L':  // pappa
      if (!asleep && life.food <= 90) {
        life.food = min(100.0f, life.food + 30);
        life.ate = true;
        next = FEED;
      }
      break;
    case 'R':  // gioca
      if (!asleep && life.energy >= 15) {
        life.joy = min(100.0f, life.joy + 25);
        life.energy = max(0.0f, life.energy - 10);
        life.food = max(0.0f, life.food - 4);
        next = PLAY;
      }
      break;
    case 'U':  // pulisci (also while it sleeps)
      if (life.poops) {
        animPoops = life.poops;
        life.poops = 0;
        life.joy = min(100.0f, life.joy + 5);
        next = CLEAN;
      }
      break;
    case 'D':  // medicina
      if (life.sick) {
        life.sick = false;
        life.neglect = 0;
        life.joy = max(0.0f, life.joy - 5);  // it's bitter
        next = CURE;
      }
      break;
    case 'A':  // coccole
      if (!asleep) {
        life.joy = min(100.0f, life.joy + 8);
        next = LOVE;
      }
      break;
    default: return false;
  }
  anim = next;
  animStart = millis();
  static const sound::Id SOUND[] = {sound::CLICK, sound::EAT, sound::CHEEP, sound::WATER, sound::SPARKLE, sound::CHEEP, sound::NO};
  sound::play(SOUND[next]);
  if (next != REFUSE) save();
  return true;
}

const GameControls *PetMode::keys() {
  static const GameControls c = {"LRUDA", {"Pappa", "Gioca", "Pulisci", "Medicina", "Coccole"}, false,
                                 "Tastiera: ← pappa, → gioca, ↑ pulisci, ↓ medicina, spazio coccole."};
  return &c;
}

String PetMode::status() const {
  const Status s = PetMode::info();
  return s.name + " · " + s.mood;
}

PetMode::Status PetMode::info() {
  load();
  Status s;
  s.name = life.name;
  static const char *STAGES[] = {"Uovo", "Ragazzo"};
  s.stage = STAGES[stage()];
  s.food = (uint8_t)life.food;
  s.joy = (uint8_t)life.joy;
  s.energy = (uint8_t)life.energy;
  s.poops = life.poops;
  s.sick = life.sick;
  s.asleep = asleepNow();
  s.ageHours = life.ageMin / 60;
  if (stage() == EGG) s.mood = "Sta per nascere";
  else if (s.sick) s.mood = "È malato: dagli la medicina";
  else if (s.asleep) s.mood = "Dorme";
  else if (s.food < 25) s.mood = "Ha fame";
  else if (s.poops >= 2) s.mood = "Vuole un po' di pulizia";
  else if (s.joy < 25) s.mood = "È triste: gioca con lui";
  else if (s.energy < 20) s.mood = "È stanco";
  else if (s.joy > 70) s.mood = "È felice";
  else s.mood = "Sta bene";
  return s;
}

void PetMode::rename(const String &name) {
  load();
  String n = name;
  n.trim();
  if (!n.length()) return;
  strncpy(life.name, n.c_str(), sizeof(life.name) - 1);
  life.name[sizeof(life.name) - 1] = 0;
  save();
}

void PetMode::reset() {
  load();
  Life fresh;
  memcpy(fresh.name, life.name, sizeof(fresh.name));
  fresh.lastEpoch = life.lastEpoch;
  life = fresh;
  anim = NONE;
  save();
}
