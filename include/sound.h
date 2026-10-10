#pragma once

#include <Arduino.h>

// Sounds, through a MAX98357A I2S amplifier and a small loudspeaker (see
// PIN_I2S_* in constants.h and the README). A tiny synthesizer in a task of
// its own - two voices of square, triangle, sine or noise with pitch
// glides and soft edges - so the panel and the web page never wait on it.
// Off unless settings.soundOn; silent at night (the night schedule)
// except for the alarm; settings.soundVol is the volume.
namespace sound {

enum Id : uint8_t {
  CLICK,    // a key
  BLIP,     // something hit (a brick, an invader)
  POINT,    // a point scored
  JUMP,     // a jump
  HIT,      // a life lost
  LOSE,     // game over
  WIN,      // a round won, a level cleared
  LINE,     // Tetris lines
  EAT,      // chomp
  MEOW,     // the cat
  PURR,     // the cat, stroked
  ROAR,     // the dragon
  FLAME,    // the dragon's fire
  CHEEP,    // the frog / the pet, happy
  NO,       // refused
  WATER,    // watering
  SNIP,     // pruning
  SPARKLE,  // fertiliser, magic
  NOTIFY,   // a notification
  CHIME,    // the hour
  BIRDS,    // the alarm
  TEST,     // a little tune, to try the speaker
  COUNT
};

// Starts the audio task (from setup()); does nothing if sound is off.
void begin();
// After soundOn / soundVol changed.
void apply();
// Plays a sound (on a free voice, or instead of the oldest). `anyTime`
// also at night (the alarm). Returns at once.
void play(Id id, bool anyTime = false);
// The hourly chime and other timed sounds (from loop()).
void loop();
// By name ("meow"), for the command "s <name>"; false if unknown.
bool find(const String &name, Id &id);
const char *name(Id id);

// The next `n` mono samples (16 bit) - what the task sends to the
// amplifier; also for tests on a computer.
void fill(int16_t *out, size_t n);
static const uint32_t RATE = 16000;  // samples per second

}  // namespace sound
