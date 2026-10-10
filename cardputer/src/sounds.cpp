#include "sounds.h"

#include <M5Cardputer.h>
#include <freertos/queue.h>
#include <string.h>

#include "../../include/sound_synth.h"

namespace sounds {

namespace {

struct Msg {
  sound::Id id;
  uint8_t volume;
};
QueueHandle_t queue = nullptr;

// The speaker plays chunks queued one after the other (at most two per
// channel): three buffers in turn, so the one being filled is never one
// still queued or playing.
const int CHANNEL = 0;
const size_t CHUNK = 512;  // 32 ms
int16_t buffers[3][CHUNK];

void audioTask(void *) {
  sound::synth::Synth player;
  float amplitude = 0;
  int next = 0;
  for (;;) {
    Msg m;
    // Waiting for a sound; while one plays, only a glance at the queue.
    if (xQueueReceive(queue, &m, player.playing() ? 0 : portMAX_DELAY) == pdTRUE) {
      player.start(m.id);
      amplitude = sound::amplitudeFor(m.volume);
    }
    if (!player.playing()) continue;
    if (M5.Speaker.isPlaying(CHANNEL) >= 2) {  // two chunks waiting already
      vTaskDelay(pdMS_TO_TICKS(4));
      continue;
    }
    player.fill(buffers[next], CHUNK, amplitude);
    M5.Speaker.playRaw(buffers[next], CHUNK, sound::RATE, false, 1, CHANNEL, false);
    next = (next + 1) % 3;
  }
}

}  // namespace

void begin() {
  queue = xQueueCreate(4, sizeof(Msg));
  M5.Speaker.begin();
  M5.Speaker.setVolume(200);  // the lamp's volume scales the samples themselves
  // Core 1 with the screen, below it in priority: core 0 has the Bluetooth.
  xTaskCreatePinnedToCore(audioTask, "sound", 4096, nullptr, 1, nullptr, 1);
}

void received(const char *message, unsigned length) {
  if (!queue) return;
  char text[32];
  const unsigned n = length < sizeof(text) - 1 ? length : sizeof(text) - 1;
  memcpy(text, message, n);
  text[n] = 0;
  char *space = strchr(text, ' ');
  int volume = 50;
  if (space) {
    *space = 0;
    volume = atoi(space + 1);
  }
  Msg m;
  if (!sound::findSound(text, m.id)) return;  // a sound this remote doesn't know yet
  m.volume = (uint8_t)(volume < 0 ? 0 : volume > 100 ? 100 : volume);
  xQueueSend(queue, &m, 0);
}

}  // namespace sounds
