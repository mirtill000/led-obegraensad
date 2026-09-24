#pragma once

#include "modes.h"
#include "scroller.h"

// "Dal web": word of the day, an "on this day" event and the next calendar
// event (see webinfo.h), scrolling one after another at the height set in
// settings.webPosition. Until the first data arrives it shows the shared
// "waiting" dots (see ui.h).
class WebMode : public Mode {
 public:
  const char *id() const override { return "web"; }
  const char *name() const override { return "Dal web"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossima"; }
  void action() override { showNext(); }

 private:
  void showNext();

  Scroller scroller_;
  bool waiting_ = false;  // sources on but nothing downloaded yet
  uint32_t lastWait_ = 0;
  uint32_t waitSince_ = 0;
  uint8_t slot_ = 0;      // which source comes next
  uint8_t history_ = 0;   // which "on this day" event comes next
  int row_ = -1;
};
