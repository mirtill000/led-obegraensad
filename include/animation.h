#pragma once

#include <Arduino.h>

// One animation of the "Animazioni" mode. frame() draws a whole frame into
// the display buffer (the mode clears nothing and calls render() after);
// it is called every frameMs(), scaled by the mode's speed setting.
class Animation {
 public:
  virtual const char *id() const = 0;     // stable, used in URLs and NVS
  virtual const char *name() const = 0;   // shown on the web page
  virtual const char *group() const = 0;  // heading in the page's menu
  virtual uint16_t frameMs() const = 0;
  virtual bool needsTime() const { return false; }  // skipped in "auto" until the clock syncs
  virtual void start() {}
  virtual void frame(uint32_t now) = 0;
};

// All animations, in menu order (src/animations/animations.cpp).
extern Animation *const ANIMATIONS[];
extern const uint8_t ANIMATION_COUNT;
Animation *findAnimation(const String &id);
