#pragma once

// The remote-control protocol, shared by the lamp (src/commands.cpp,
// src/ble.cpp, src/web.cpp) and the Cardputer remote (cardputer/): one
// definition, so the two can't drift apart. Plain #defines, no Arduino
// dependencies, so any project can include it.
//
// Bluetooth LE service and characteristics:
#define REMOTE_SERVICE_UUID "8f3e0000-5c1a-4a6b-9b8e-0b5e6a1d0bea"
#define REMOTE_COMMAND_UUID "8f3e0001-5c1a-4a6b-9b8e-0b5e6a1d0bea"  // write: one command
#define REMOTE_STATE_UUID "8f3e0002-5c1a-4a6b-9b8e-0b5e6a1d0bea"    // read/notify: short JSON
#define REMOTE_FRAME_UUID "8f3e0003-5c1a-4a6b-9b8e-0b5e6a1d0bea"    // read/notify: 128 bytes
#define REMOTE_CATALOG_UUID "8f3e0004-5c1a-4a6b-9b8e-0b5e6a1d0bea"  // read: modes, games, animations

// Commands: a letter, a space, the argument. The same strings go over
// Bluetooth (command characteristic), to POST /api/cmd (field "c") and are
// used by the web page's own buttons.
//   k <L|R|U|D|A>        game key
//   m <mode id>          show a mode
//   g <game id|auto>     play a game            a <anim id|auto>  an animation
//   d <0|1> [game id]    demo off/on (the game on show if no id)
//   x                    the mode's button      n                 next mode
//   b <1-255>            brightness
//   t <text>             show this text         p <icon>|<text>   notification
//   s <1-9> [mode id]    speed (of the mode on show if no id)
//   w <x> <y> <0-255>    paint a pixel of the Lavagna (shows it)
//   w c                  clear the Lavagna    w l   Game of Life from it
#define REMOTE_KEYS "LRUDA"
