#include "modes/creature.h"

#include <Preferences.h>
#include <math.h>

#include "timekeeping.h"

static const uint32_t MAX_CATCH_UP_MIN = 3 * 24 * 60;

float creatureRandom() { return (esp_random() & 0xFFFF) / 65535.0f; }

Creature *Creature::find(const String &id) {
  for (uint8_t i = 0; i < CREATURE_COUNT; i++) {
    if (id == CREATURES[i]->id()) return CREATURES[i];
  }
  return nullptr;
}

void Creature::load() {
  if (loaded_) return;
  loaded_ = true;
  fresh();
  Preferences prefs;
  prefs.begin("obegransad", true);
  const size_t n = stateSize();
  uint8_t *saved = (uint8_t *)malloc(n);
  if (saved && prefs.getBytesLength(id()) == n && prefs.getBytes(id(), saved, n) == n &&
      ((CreatureClock *)saved)->version == version()) {
    memcpy(state(), saved, n);
  }
  free(saved);
  prefs.end();
  clock().version = version();
}

void Creature::save() {
  Preferences prefs;
  prefs.begin("obegransad", false);
  prefs.putBytes(id(), state(), stateSize());
  prefs.end();
  dirty_ = false;
  lastSaveMs_ = millis();
}

void Creature::tick() {
  load();
  const uint32_t ms = millis();
  if (lastTickMs_ && ms - lastTickMs_ < 1000) return;
  lastTickMs_ = ms;
  watch();
  struct tm tm;
  if (localTime(tm)) {
    const time_t now = time(nullptr);
    CreatureClock &c = clock();
    if (!c.lastEpoch || now < (time_t)c.lastEpoch) c.lastEpoch = now;  // first time with a clock
    uint32_t minutes = (now - c.lastEpoch) / 60;
    if (minutes > MAX_CATCH_UP_MIN) {
      c.lastEpoch = now - MAX_CATCH_UP_MIN * 60;
      minutes = MAX_CATCH_UP_MIN;
    }
    for (uint32_t i = 0; i < minutes; i++) {
      c.lastEpoch += 60;
      const time_t at = c.lastEpoch;
      struct tm local;
      localtime_r(&at, &local);
      liveMinute(&local);
      dirty_ = true;
    }
  } else if (!clock().lastEpoch) {
    // Never had a clock: minutes of uptime, no night.
    pendingMs_ += 1000;
    if (pendingMs_ >= 60000) {
      pendingMs_ = 0;
      liveMinute(nullptr);
      dirty_ = true;
    }
  }
  // Hourly (care is saved at once): every flash write stalls the panel's
  // refresh for a moment, a blink that shows on a still picture.
  if (dirty_ && ms - lastSaveMs_ > 60 * 60000UL) save();
}

bool Creature::input(char key) {
  load();
  if (!strchr(keys()->keys, key)) return false;
  if (care(key)) save();
  return true;
}

String Creature::status() const {
  Info i = const_cast<Creature *>(this)->info();
  return i.title + " · " + i.mood;
}

void Creature::reset() {
  load();
  const uint32_t epoch = clock().lastEpoch;
  fresh();
  clock().version = version();
  clock().lastEpoch = epoch;
  save();
  start();
}

// ---------------------------------------------------------------------------

void Canvas::fill(float v) {
  for (auto &row : px) {
    for (float &p : row) p = v;
  }
}

void Canvas::put(float x, float y, float v) {
  const int x0 = (int)floorf(x), y0 = (int)floorf(y);
  const float fx = x - x0, fy = y - y0;
  auto add = [&](int px_, int py_, float a) {
    if (in(px_, py_)) px[py_][px_] = min(1.0f, px[py_][px_] + a);
  };
  add(x0, y0, v * (1 - fx) * (1 - fy));
  add(x0 + 1, y0, v * fx * (1 - fy));
  add(x0, y0 + 1, v * (1 - fx) * fy);
  add(x0 + 1, y0 + 1, v * fx * fy);
}

void Canvas::sprite(const Sprite &s, int x, int y, int frame, float v, bool flip, float (*mark)(char, void *),
                    void *context) {
  for (int r = 0; r < s.h; r++) {
    for (int c = 0; c < s.w; c++) {
      const char ch = sprites::at(s, frame, flip ? s.w - 1 - c : c, r);
      if (ch == '.' || ch == ' ') continue;
      float l;
      if (isalpha((uint8_t)ch) && mark) {
        l = mark(ch, context);
        if (l < 0) continue;
      } else {
        l = sprites::charLevel(ch, 255) / 255.0f * v;
        if (ch == '+') l = 1;
      }
      set(x + c, y + r, l);
    }
  }
}

void Canvas::show() {
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) display.setLevel(x, y, ui::tone(px[y][x]));
  }
  display.render();
}
