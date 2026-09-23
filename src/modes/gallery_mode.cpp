#include "modes/gallery_mode.h"

#include "display.h"
#include "settings.h"

static const uint32_t EACH_MS = 20000;        // per drawing, showing them all
static const uint32_t DRAFT_MS = 2 * 60000;   // live draft after the last edit

void GalleryMode::load(const String &id) {
  if (!galleryLoad(id, current_)) current_ = Drawing();
  frame_ = 0;
  frameSince_ = shownSince_ = millis();
}

void GalleryMode::start() {
  draft_ = false;
  ids_.clear();
  if (settings.galleryShow == "all") {
    for (const Drawing &d : galleryList()) ids_.push_back(d.id);
    index_ = 0;
    load(ids_.empty() ? String("") : ids_[0]);
  } else {
    load(settings.galleryShow);
  }
}

void GalleryMode::next() {
  draft_ = false;
  if (ids_.empty()) {
    for (const Drawing &d : galleryList()) ids_.push_back(d.id);
    index_ = 0;
  } else {
    index_ = (index_ + 1) % ids_.size();
  }
  if (!ids_.empty()) load(ids_[index_]);
}

void GalleryMode::showDraft(const uint8_t *frames, size_t count, uint16_t frameMs) {
  current_ = Drawing();
  current_.frames.assign(frames, frames + count * 256);
  current_.count = count;
  current_.frameMs = frameMs;
  if (!draft_) frame_ = 0;
  if (frame_ >= current_.frameCount()) frame_ = 0;
  draft_ = true;
  draftUntil_ = millis() + DRAFT_MS;
}

void GalleryMode::update(uint32_t now) {
  if (draft_ && (int32_t)(now - draftUntil_) > 0) start();  // editor left: back to the gallery
  if (!draft_ && settings.galleryShow == "all" && ids_.size() > 1 && now - shownSince_ >= EACH_MS) next();

  const uint8_t frames = current_.frameCount();
  if (frames > 1 && now - frameSince_ >= current_.frameMs) {
    frameSince_ = now;
    frame_ = (frame_ + 1) % frames;
  }
  static uint32_t lastDraw = 0;
  if (now - lastDraw < 40) return;
  lastDraw = now;

  display.clear();
  if (frames == 0) {
    // Nothing to show yet: a small pencil, inviting to draw on the page.
    static const char *PENCIL[] = {"......##", ".....#.#", "....#.#.", "...#.#..", "..#.#...", ".##.....", ".#......", "#......."};
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        if (PENCIL[y][x] == '#') display.setLevel(4 + x, 4 + y, 120);
      }
    }
  } else {
    const uint8_t *px = current_.frames.data() + frame_ * 256;
    for (int i = 0; i < 256; i++) display.setLevel(i % COLS, i / COLS, px[i]);
  }
  display.render();
}
