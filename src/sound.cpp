#include "sound.h"

#include <time.h>

#include "ble.h"
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

synth::Synth player;  // the sounds themselves: sound_synth.h
#ifdef SOUND_I2S
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
#define LOCK() portENTER_CRITICAL(&lock)
#define UNLOCK() portEXIT_CRITICAL(&lock)
#else
#define LOCK()
#define UNLOCK()
#endif
// play() runs in loop(), the synthesizer in the audio task: sounds to
// start are handed over here.
volatile Id queued[4];
volatile uint8_t queuedCount = 0;
float amplitude = 0;  // from soundVol
bool running = false;

void startQueued() {
  LOCK();
  const uint8_t n = queuedCount;
  Id ids[4];
  for (uint8_t i = 0; i < n; i++) ids[i] = queued[i];
  queuedCount = 0;
  UNLOCK();
  for (uint8_t i = 0; i < n; i++) player.start(ids[i]);
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
    quiet = !queuedCount && !player.playing() && !any;
  }
}
#endif

}  // namespace

void fill(int16_t *out, size_t n) {
  if (queuedCount) startQueued();
  player.fill(out, n, amplitude);
}

void begin() { apply(); }

void apply() {
  amplitude = amplitudeFor(settings.soundVol);
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
  if (id >= COUNT || !allowed(anyTime)) return;
  bleSound(NAMES[id], settings.soundVol);  // a connected Cardputer plays it too
  if (!running) return;
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

bool find(const String &name, Id &id) { return findSound(name.c_str(), id); }

const char *name(Id id) { return id < COUNT ? NAMES[id] : ""; }

}  // namespace sound
