#pragma once

// The lamp's sounds, out of the Cardputer's speaker: the lamp sends a
// sound's name and its volume over Bluetooth ("meow 50", see the "sound"
// characteristic in ../../src/ble.cpp) and the same synthesizer as the
// lamp's (../../include/sound_synth.h) plays it here. Whether there are
// sounds at all, and how loud, is the lamp's setting (Suoni, Volume).
namespace sounds {

void begin();  // after M5Cardputer.begin(): starts the audio task
// From the Bluetooth task: the lamp's message, queued (never blocks).
void received(const char *message, unsigned length);

}  // namespace sounds
