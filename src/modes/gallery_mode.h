#pragma once

#include "gallery.h"
#include "modes.h"

// Drawings and animations from the page's pixel editor: the one chosen
// (settings.galleryShow) or all of them in turn. While someone draws on the
// page, the drawing in progress is shown live instead.
class GalleryMode : public Mode {
 public:
  const char *id() const override { return "gallery"; }
  const char *name() const override { return "Disegni"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossimo disegno"; }
  void action() override { next(); }
  bool hasSpeed() const override { return false; }

  // The editor's current drawing (frames of 256 bytes), shown for a while.
  void showDraft(const uint8_t *frames, size_t count, uint16_t frameMs);

 private:
  void next();
  void load(const String &id);

  Drawing current_;
  std::vector<String> ids_;  // for "all"
  size_t index_ = 0;
  uint32_t shownSince_ = 0;
  uint32_t frameSince_ = 0;
  uint8_t frame_ = 0;
  bool draft_ = false;
  uint32_t draftUntil_ = 0;
};

GalleryMode &galleryMode();
