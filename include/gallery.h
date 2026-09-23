#pragma once

#include <Arduino.h>

#include <vector>

// Drawings and animations made on the page (pixel editor or imported
// images), saved in flash (LittleFS, the "spiffs" partition). Each frame is
// 256 bytes: one brightness level per pixel, row by row.
struct Drawing {
  String id;
  String name;
  uint16_t frameMs = 200;
  std::vector<uint8_t> frames;  // 256 bytes per frame (listings: the first only)
  uint8_t count = 0;            // frames in the file
  uint8_t frameCount() const { return count; }
};

static const uint8_t GALLERY_MAX_FRAMES = 32;
static const uint8_t GALLERY_MAX_DRAWINGS = 60;

bool galleryBegin();
// Every drawing, oldest first, with just its first frame (for thumbnails).
std::vector<Drawing> galleryList();
bool galleryLoad(const String &id, Drawing &out);
// Saves d.frames (new if d.id is empty: an id is assigned); returns success.
bool gallerySave(Drawing &d);
bool galleryDelete(const String &id);
