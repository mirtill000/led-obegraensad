#pragma once

#include <time.h>

#include "display.h"
#include "sprite.h"
#include "modes.h"
#include "ui.h"

// What the Tamagotchi-style modes (Bonsai, Gatto, Draghetto) share: a life
// simulated a minute at a time - also while another mode is shown or the
// lamp is off, catching up to three days when it comes back - saved as one
// NVS blob (so it is in the settings backup), care given with the game keys
// (page buttons, arrows, the Cardputer), and what the page shows about it.
//
// The saved state starts with a CreatureClock; the rest is up to the mode.
struct CreatureClock {
  uint8_t version;
  uint32_t lastEpoch;  // wall clock of the last simulated minute, 0 = unknown
};

class Creature : public Mode {
 public:
  bool hasSpeed() const override { return false; }
  const GameControls *controls() const override { return keys(); }
  bool input(char key) override;
  String status() const override;

  struct Stat {
    const char *label;
    uint8_t value, low;  // 0-100; below `low` it is a need
  };
  struct Info {
    String title, mood;  // "Pixel · cucciolo, 2 giorni", "Ha fame"
    Stat stats[5];
    uint8_t count = 0;
    void add(const char *label, float value, uint8_t low) {
      if (count < 5) stats[count++] = {label, (uint8_t)constrain(value, 0.0f, 100.0f), low};
    }
  };
  virtual Info info() = 0;
  virtual const GameControls *keys() const = 0;
  // A name of its own (the page offers to change it), "" for none.
  virtual String petName() { return String(); }
  virtual void rename(const String &) {}
  // Starting again: the button's label and the question before it.
  virtual const char *resetName() const = 0;
  virtual const char *resetQuestion() const = 0;
  void reset();

  // Brings it up to date (from loop, shown or not).
  void tick();
  static Creature *find(const String &id);

 protected:
  // The saved state: its bytes, starting with a CreatureClock.
  virtual uint8_t *state() = 0;
  virtual size_t stateSize() const = 0;
  virtual uint8_t version() const = 0;
  virtual void fresh() = 0;  // a new life (the state's defaults)
  // One minute of its life; `t` is that minute's local time, or nullptr if
  // the clock isn't set.
  virtual void liveMinute(const struct tm *t) = 0;
  // Every second, shown or not (to notice things as they happen).
  virtual void watch() {}
  // A care key; false if it refuses (it is asleep, it isn't hungry...).
  virtual bool care(char key) = 0;

  CreatureClock &clock() { return *(CreatureClock *)state(); }
  void load();
  void save();
  void changed() { dirty_ = true; }

 private:
  bool loaded_ = false, dirty_ = false;
  uint32_t lastTickMs_ = 0, lastSaveMs_ = 0, pendingMs_ = 0;
};

// The creatures, for the loop and the page (listed in modes.cpp).
extern Creature *const CREATURES[];
extern const uint8_t CREATURE_COUNT;
extern Creature *const bonsaiCreature;  // bonsai_mode.cpp
extern Creature *const catCreature;     // cat_mode.cpp
extern Creature *const dragonCreature;  // dragon_mode.cpp

// A frame of brightnesses (0..1) for the creatures, shown through ui::tone().
struct Canvas {
  float px[ROWS][COLS];
  void fill(float v);
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < COLS && y < ROWS; }
  void set(int x, int y, float v) {
    if (in(x, y)) px[y][x] = v;
  }
  void lift(int x, int y, float v) {
    if (in(x, y)) px[y][x] = max(px[y][x], v);
  }
  float get(int x, int y) const { return in(x, y) ? px[y][x] : 0; }
  // A sub-pixel point shared among the four pixels around it.
  void put(float x, float y, float v);
  // A sprite: its shades scaled to `v` (marks via `mark`: a level 0..1, or
  // < 0 for transparent; nullptr draws them as '#'). `flip` mirrors it.
  void sprite(const Sprite &s, int x, int y, int frame, float v, bool flip = false,
              float (*mark)(char, void *) = nullptr, void *context = nullptr);
  void show();
};

float creatureRandom();  // 0..1
