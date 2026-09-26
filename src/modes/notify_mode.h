#pragma once

#include "modes.h"
#include "pager.h"

// Notifications from the phone (GET/POST /api/notify, e.g. from an iOS
// Shortcut): an icon drops in and moves for a couple of seconds, then the
// text is shown as still pages (Pager), then the lamp goes back to what it
// was showing. Up to QUEUE notifications wait their turn. Hidden: modes.cpp
// switches to it while one is pending (only the sunrise alarm wins).
class NotifyMode : public Mode {
 public:
  static const int QUEUE = 4;

  const char *id() const override { return "notify"; }
  const char *name() const override { return "Notifica"; }
  bool hidden() const override { return true; }
  bool hasSpeed() const override { return false; }
  void start() override;
  void update(uint32_t now) override;

  // Queues a notification (UTF-8 text, icon id from ICONS or ""); the
  // oldest waiting one is dropped when the queue is full. False if both
  // are empty or the icon is unknown.
  static bool push(const String &text, const String &icon);
  static int pending();
  static void clear();

 private:
  void begin(uint32_t now);
  void drawIcon(uint32_t t);

  enum Phase : uint8_t { ICON, TEXT, DONE } phase_ = DONE;
  uint32_t since_ = 0;
  Pager pager_;
};
