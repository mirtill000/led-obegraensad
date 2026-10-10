#include "sound.h"

#include <math.h>
#include <time.h>

#include "constants.h"
#include "modes.h"
#include "settings.h"
#include "timekeeping.h"

#if __has_include(<ESP_I2S.h>)
#include <ESP_I2S.h>
#define SOUND_I2S 1
#endif

namespace sound {

namespace {

enum Wave : uint8_t { SQUARE, TRIANGLE, SINE, NOISE, REST };

// One step of a sound: a tone gliding from f0 to f1 Hz in `ms`, at `vol`
// (0-255); `ring` lets it fade out like a bell instead of holding.
struct Step {
  Wave wave;
  uint16_t f0, f1, ms;
  uint8_t vol;
  bool ring;
};
const Step END = {REST, 0, 0, 0, 0, false};

// The sounds. Short and soft: they come out of a lamp.
const Step CLICK_S[] = {{SQUARE, 1800, 1800, 12, 90, false}, END};
const Step BLIP_S[] = {{SQUARE, 880, 1320, 45, 140, false}, END};
const Step POINT_S[] = {{SQUARE, 988, 988, 60, 150, false}, {SQUARE, 1319, 1319, 120, 150, true}, END};
const Step JUMP_S[] = {{SQUARE, 330, 990, 120, 130, false}, END};
const Step HIT_S[] = {{NOISE, 1200, 300, 160, 190, false}, {SQUARE, 220, 110, 160, 150, false}, END};
const Step LOSE_S[] = {{SQUARE, 523, 494, 180, 150, false}, {SQUARE, 466, 440, 180, 150, false}, {SQUARE, 415, 392, 180, 150, false}, {TRIANGLE, 370, 300, 500, 200, true}, END};
const Step WIN_S[] = {{SQUARE, 523, 523, 90, 150, false}, {SQUARE, 659, 659, 90, 150, false}, {SQUARE, 784, 784, 90, 150, false}, {SQUARE, 1047, 1047, 300, 150, true}, END};
const Step LINE_S[] = {{TRIANGLE, 660, 1320, 160, 200, false}, {TRIANGLE, 1320, 1320, 160, 160, true}, END};
const Step EAT_S[] = {{NOISE, 900, 600, 50, 150, false}, {REST, 0, 0, 60, 0, false}, {NOISE, 900, 600, 50, 150, false}, END};
const Step MEOW_S[] = {{TRIANGLE, 520, 820, 160, 200, false}, {TRIANGLE, 820, 600, 260, 200, false}, {TRIANGLE, 600, 420, 160, 140, true}, END};
const Step PURR_S[] = {{NOISE, 60, 60, 450, 120, false}, {REST, 0, 0, 120, 0, false}, {NOISE, 55, 55, 450, 110, false}, {REST, 0, 0, 120, 0, false}, {NOISE, 60, 60, 450, 100, false}, END};
const Step ROAR_S[] = {{NOISE, 400, 150, 500, 220, false}, {SQUARE, 110, 70, 300, 120, true}, END};
const Step FLAME_S[] = {{NOISE, 3000, 900, 600, 160, true}, END};
const Step CHEEP_S[] = {{SINE, 1600, 2400, 70, 160, false}, {REST, 0, 0, 40, 0, false}, {SINE, 1800, 2600, 70, 160, false}, END};
const Step NO_S[] = {{SQUARE, 200, 180, 120, 140, false}, {REST, 0, 0, 40, 0, false}, {SQUARE, 180, 160, 160, 140, false}, END};
const Step WATER_S[] = {{SINE, 500, 900, 60, 140, false}, {SINE, 600, 1000, 60, 140, false}, {SINE, 450, 850, 60, 140, false}, {NOISE, 4000, 4000, 600, 50, true}, END};
const Step SNIP_S[] = {{NOISE, 6000, 6000, 30, 180, false}, {REST, 0, 0, 80, 0, false}, {NOISE, 6000, 6000, 30, 180, false}, END};
const Step SPARKLE_S[] = {{SINE, 2093, 2093, 70, 120, true}, {SINE, 2637, 2637, 70, 120, true}, {SINE, 3136, 3136, 70, 120, true}, {SINE, 4186, 4186, 300, 120, true}, END};
const Step NOTIFY_S[] = {{SINE, 1319, 1319, 140, 200, true}, {SINE, 1760, 1760, 500, 200, true}, END};
const Step CHIME_S[] = {{SINE, 659, 659, 900, 210, true}, {SINE, 523, 523, 1600, 210, true}, END};
const Step BIRDS_S[] = {{SINE, 2800, 3600, 80, 110, false}, {SINE, 3400, 2600, 90, 110, false}, {REST, 0, 0, 160, 0, false}, {SINE, 3000, 4000, 60, 100, false}, {SINE, 3000, 4000, 60, 100, false}, {REST, 0, 0, 500, 0, false}, {SINE, 2200, 2900, 120, 90, false}, {SINE, 2900, 2400, 140, 90, true}, {REST, 0, 0, 700, 0, false}, {SINE, 3500, 2800, 70, 100, false}, {SINE, 3500, 2800, 70, 100, false}, {SINE, 3500, 2800, 70, 100, false}, END};
const Step TEST_S[] = {{TRIANGLE, 523, 523, 150, 200, false}, {TRIANGLE, 659, 659, 150, 200, false}, {TRIANGLE, 784, 784, 150, 200, false}, {TRIANGLE, 1047, 1047, 400, 200, true}, END};
const Step *const SOUNDS[COUNT] = {CLICK_S, BLIP_S, POINT_S, JUMP_S, HIT_S, LOSE_S, WIN_S, LINE_S, EAT_S, MEOW_S, PURR_S, ROAR_S, FLAME_S, CHEEP_S, NO_S, WATER_S, SNIP_S, SPARKLE_S, NOTIFY_S, CHIME_S, BIRDS_S, TEST_S};

const char *const NAMES[COUNT] = {"click", "blip", "point", "jump", "hit", "lose", "win", "line", "eat", "meow", "purr",
                                  "roar", "flame", "cheep", "no", "water", "snip", "sparkle", "notify", "chime", "birds", "test"};

// A voice: where it is in its sound.
struct Voice {
  const Step *step = nullptr;  // nullptr = silent
  uint32_t at = 0;             // samples into the step
  uint32_t phase = 0;
  uint32_t started = 0;        // for "the oldest"
  uint16_t noise = 0xACE1;
  int16_t held = 0;            // noise: the sample held until the next period
};

const int VOICES = 2;
Voice voices[VOICES];
#ifdef SOUND_I2S
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
#define LOCK() portENTER_CRITICAL(&lock)
#define UNLOCK() portEXIT_CRITICAL(&lock)
#else
#define LOCK()
#define UNLOCK()
#endif
volatile Id queued[4];
volatile uint8_t queuedCount = 0;
uint32_t playCount = 0;
float amplitude = 0;  // from soundVol
bool running = false;

const int16_t *sineTable() {
  static int16_t table[256];
  static bool made = false;
  if (!made) {
    for (int i = 0; i < 256; i++) table[i] = (int16_t)lroundf(32767 * sinf(i * 2 * (float)M_PI / 256));
    made = true;
  }
  return table;
}

float volumeAmplitude() {
  const float v = settings.soundVol / 100.0f;
  return 14000 * v * v;  // perceived loudness grows with the square; kept below clipping
}

void startQueued() {
  LOCK();
  const uint8_t n = queuedCount;
  Id ids[4];
  for (uint8_t i = 0; i < n; i++) ids[i] = queued[i];
  queuedCount = 0;
  UNLOCK();
  for (uint8_t i = 0; i < n; i++) {
    Voice *v = &voices[0];
    for (Voice &c : voices) {
      if (!c.step) {
        v = &c;
        break;
      }
      if (c.started < v->started) v = &c;
    }
    *v = Voice();
    v->step = SOUNDS[ids[i]];
    v->started = ++playCount;
  }
}

int16_t sample(Voice &v) {
  const Step &s = *v.step;
  const uint32_t len = (uint32_t)s.ms * RATE / 1000;
  if (v.at >= len) {
    v.step++;
    v.at = 0;
    if (!v.step->ms) {
      v.step = nullptr;
      return 0;
    }
    return sample(v);
  }
  const float k = (float)v.at / len;
  v.at++;
  if (s.wave == REST) return 0;
  const float f = s.f0 + (s.f1 - s.f0) * k;
  const uint32_t inc = (uint32_t)(f * 4294967296.0f / RATE);
  const uint32_t before = v.phase;
  v.phase += inc;
  int32_t x;
  switch (s.wave) {
    case SQUARE: x = v.phase < 0x80000000u ? 20000 : -20000; break;
    case TRIANGLE: {
      const int32_t p = (int32_t)(v.phase >> 16);  // 0..65535
      x = p < 32768 ? -32767 + p * 2 : 32767 - (p - 32768) * 2;
      break;
    }
    case SINE: x = sineTable()[v.phase >> 24]; break;
    default:  // noise, a new random value every period (pitched noise)
      if (v.phase < before) {
        v.noise = (v.noise >> 1) ^ (-(v.noise & 1) & 0xB400u);
        v.held = (int16_t)(v.noise - 32768);
      }
      x = v.held * 2 / 3;
      break;
  }
  // Soft edges (no clicks): 3 ms in; out over the last 8 ms, or over the
  // whole step for a ringing one.
  const uint32_t attack = RATE * 3 / 1000, release = RATE * 8 / 1000;
  float env = 1;
  if (v.at < attack) env = (float)v.at / attack;
  if (s.ring) env *= (1 - k) * (1 - k);
  else if (len - v.at < release) env *= (float)(len - v.at) / release;
  return (int16_t)(x * env * s.vol / 255);
}

bool allowed(bool anyTime) {
  if (!settings.soundOn || settings.soundVol == 0) return false;
  return anyTime || !isNight();
}

#ifdef SOUND_I2S
I2SClass i2s;
TaskHandle_t task = nullptr;

void audioTask(void *) {
  static int16_t mono[256];
  static int16_t stereo[512];
  bool quiet = true;
  for (;;) {
    if (quiet) ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));  // asleep until something plays
    fill(mono, 256);
    bool any = false;
    for (int i = 0; i < 256; i++) {
      stereo[i * 2] = stereo[i * 2 + 1] = mono[i];  // the amplifier mixes left and right
      any |= mono[i] != 0;
    }
    i2s.write((const uint8_t *)stereo, sizeof(stereo));  // waits while the DMA is full
    bool playing = queuedCount > 0;
    for (const Voice &v : voices) playing |= v.step != nullptr;
    quiet = !playing && !any;
  }
}
#endif

}  // namespace

void fill(int16_t *out, size_t n) {
  if (queuedCount) startQueued();
  const float amp = amplitude / 32767.0f;
  for (size_t i = 0; i < n; i++) {
    int32_t mix = 0;
    for (Voice &v : voices) {
      if (v.step) mix += sample(v);
    }
    out[i] = (int16_t)constrain((int32_t)(mix * amp), -32767, 32767);
  }
}

void begin() { apply(); }

void apply() {
  amplitude = volumeAmplitude();
#ifdef SOUND_I2S
  if (!settings.soundOn || running) return;
  i2s.setPins(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN);
  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) return;
  running = true;
  // Core 0, next to the network: the panel's refresh has core 1.
  xTaskCreatePinnedToCore(audioTask, "audio", 3072, nullptr, 2, &task, 0);
#else
  running = settings.soundOn;
#endif
}

void play(Id id, bool anyTime) {
  if (id >= COUNT || !running || !allowed(anyTime)) return;
  LOCK();
  if (queuedCount < 4) queued[queuedCount++] = id;
  UNLOCK();
#ifdef SOUND_I2S
  if (task) xTaskNotifyGive(task);
#endif
}

void loop() {
  // The hour: a soft "ding-dong" (by day; settings.soundChime).
  static int lastHour = -1;
  struct tm t;
  if (!settings.soundChime || !localTime(t)) return;
  if (t.tm_min == 0 && t.tm_hour != lastHour) {
    if (lastHour >= 0) play(CHIME);
    lastHour = t.tm_hour;
  } else if (t.tm_min != 0) {
    lastHour = t.tm_hour;
  }
}

bool find(const String &name, Id &id) {
  for (uint8_t i = 0; i < COUNT; i++) {
    if (name == NAMES[i]) {
      id = (Id)i;
      return true;
    }
  }
  return false;
}

const char *name(Id id) { return id < COUNT ? NAMES[id] : ""; }

}  // namespace sound
