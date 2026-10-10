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
#define REMOTE_CATALOG_UUID "8f3e0004-5c1a-4a6b-9b8e-0b5e6a1d0bea"  // read: the scene catalog
#define REMOTE_SETTINGS_UUID "8f3e0005-5c1a-4a6b-9b8e-0b5e6a1d0bea" // read/notify: the settings a remote may change
#define REMOTE_SOUND_UUID "8f3e0006-5c1a-4a6b-9b8e-0b5e6a1d0bea"    // notify: "<sound> <volume 0-100>" to play

// The same data reaches the page over WiFi (Server-Sent Events on port 81,
// GET /events) in the same formats (src/live.cpp):
//   frame  256 levels 0-15, two pixels per byte, high nibble first, row by
//          row (Bluetooth: 128 bytes; SSE and /api/frame: 256 hex digits)
//   now    the short state, JSON (Bluetooth: the state characteristic)
//   state  (SSE only) all of /api/state; board (SSE only) the Game of Life's
//          drawing board
//
// Commands: a letter, a space, the argument. The same strings go over
// Bluetooth (command characteristic), to POST /api/cmd (field "c") and are
// used by the web page's own buttons.
//   k <L|R|U|D|A>        game key (lower case: player 2)
//   v <scene>            show anything from the catalog: a mode ("clock"),
//                        an animation ("a/voxel"), a game ("g/doom"), a
//                        drawing ("d/<id>") - see the catalog below
//   m <mode id>          show a mode
//   g <game id|auto>     play a game            a <anim id|auto>  an animation
//   d <0|1> [game id]    demo off/on (the game on show if no id)
//   x                    the mode's button      n                 next mode
//   b <1-255>            brightness
//   t <text>             show this text         p <icon>|<text>   notification
//                        (the icon may also be an animation scene, "a/fireworks")
//   s <1-9> [mode id]    speed (of the mode on show if no id)
//   w <x> <y> <0-255>    paint a pixel of the Game of Life's board (shows it)
//   w c                  clear the board      w l   set it going
//   u <sound>            play a sound (sound.h: meow, test, ...)
//   o <name> <value>     change a setting (names and limits: the settings
//                        characteristic, one per line "name, kind B/C/N,
//                        value, label, choices|..., choice names|..., min,
//                        max" separated by tabs)
#define REMOTE_KEYS "LRUDA"
// The second player of a two-player game (Tron) sends the same keys in
// lower case: "k l" is player 2 turning left.
#define REMOTE_KEYS_P2 "lruda"
