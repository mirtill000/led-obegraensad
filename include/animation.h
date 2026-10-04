#pragma once

#include <Arduino.h>

// One animation of the "Animazioni" mode. frame() draws a whole frame into
// the display buffer (the mode clears nothing and calls render() after).
//
// Time, the same for all (E6): `now` is the animation's own clock in ms,
// which the speed setting runs faster or slower; frames come every
// frameMs() of real time whatever the speed. So an animation should depend
// on `now` only, never on how many frames it has drawn:
//  - continuous ones (plasma, flights, the cube) compute everything from it;
//  - ones that move in steps (particles, cellular fire, blinking icons)
//    say fixedStep(): frame() is then called once per frameMs() of that
//    clock - several times in a row when it is behind, or when the speed
//    is high - so they keep the same pace however the lamp is doing.
// Games are steps too: they are called every frameMs() scaled by the speed.
// How a game is played, told to the web page (its pad) and to the Cardputer
// (state "c"/"ca") so neither keeps its own list.
struct GameControls {
  const char *keys;       // the keys it uses, out of "LRUDA"
  const char *labels[5];  // pad labels for L R U D A; nullptr = the plain arrow / "Salta"
  bool repeat;            // held arrows repeat (paddles, walking)
  const char *hint;       // one line under the pad
  uint8_t players = 1;    // 2: a second player sends the keys in lower case
};

// Graphics of a game: always on/off LEDs, always shaded, or following the
// "Grafica dei giochi" setting (softGames()).
enum class GameStyle : uint8_t { Crisp, Shaded, Selectable };

class Animation {
 public:
  virtual const char *id() const = 0;     // stable, used in URLs and NVS
  virtual const char *name() const = 0;   // shown on the web page
  virtual const char *group() const = 0;  // heading in the page's menu
  virtual uint16_t frameMs() const = 0;
  virtual bool needsTime() const { return false; }  // skipped in "auto" until the clock syncs
  virtual bool fixedStep() const { return false; }  // moves one step per frame (see above)
  virtual void start() {}
  virtual void frame(uint32_t now) = 0;

  // Games only: in demo mode they play by themselves; otherwise they take
  // input() from the page ('L', 'R', 'U', 'D' arrows, 'A' main button).
  virtual bool isGame() const { return false; }
  virtual void setDemo(bool) {}
  virtual void input(char) {}
  virtual const GameControls *controls() const { return nullptr; }
  virtual GameStyle style() const { return GameStyle::Selectable; }
  // Clock faces (binary, in words...) are styles of the Orologio mode, not
  // animations of their own: they don't appear among the Animazioni.
  virtual bool isClockFace() const { return false; }
};

// "crisp", "shaded" or "selectable", for the page.
const char *styleId(GameStyle style);

// All animations, in menu order (src/animations/animations.cpp).
extern Animation *const ANIMATIONS[];
extern const uint8_t ANIMATION_COUNT;
Animation *findAnimation(const String &id);
