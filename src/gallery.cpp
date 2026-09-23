#include "gallery.h"

#include <LittleFS.h>
#include <Preferences.h>

#include "gallery_examples.h"

// File layout: "PX1", frame count, frameMs (2 bytes, little endian), name
// length, name (UTF-8), then the frames.
static const char *DIR = "/gallery";
static bool mounted = false;

// The example drawings go in once, on the first start with this feature;
// deleting them later doesn't bring them back.
static void addExamples() {
  Preferences prefs;
  prefs.begin("obegransad", false);
  if (!prefs.getBool("galExamples", false)) {
    for (const ExampleDrawing &e : EXAMPLES) {
      Drawing d;
      d.name = e.name;
      d.frameMs = e.frameMs;
      d.frames.assign(e.data, e.data + e.frames * 256);
      gallerySave(d);  // ids keep increasing, so the list stays in this order
    }
    prefs.putBool("galExamples", true);
  }
  prefs.end();
}

bool galleryBegin() {
  mounted = LittleFS.begin(true);  // formats the partition on first use
  if (mounted && !LittleFS.exists(DIR)) LittleFS.mkdir(DIR);
  if (mounted) addExamples();
  return mounted;
}

static String pathOf(const String &id) { return String(DIR) + "/" + id + ".px"; }

// Ids are short and made of letters and digits only (they end up in paths).
static bool validId(const String &id) {
  if (id.length() == 0 || id.length() > 12) return false;
  for (char c : id) {
    if (!isalnum((unsigned char)c)) return false;
  }
  return true;
}

// Reads a drawing with all its frames, or just the first (`allFrames` false).
static bool readFile(const String &id, Drawing &out, bool allFrames) {
  File f = LittleFS.open(pathOf(id), "r");
  if (!f) return false;
  uint8_t header[6];
  if (f.read(header, 6) != 6 || memcmp(header, "PX1", 3) != 0) return false;
  const uint8_t frames = header[3];
  out.id = id;
  out.frameMs = header[4] | (header[5] << 8);
  const int nameLength = f.read();
  if (nameLength < 0) return false;
  char name[256];
  if (f.read((uint8_t *)name, nameLength) != (size_t)nameLength) return false;
  name[nameLength] = 0;
  out.name = name;
  out.count = frames;
  out.frames.assign((allFrames ? frames : min<uint8_t>(frames, 1)) * 256, 0);
  if (f.read(out.frames.data(), out.frames.size()) != out.frames.size()) return false;
  f.close();
  return true;
}

std::vector<Drawing> galleryList() {
  std::vector<Drawing> list;
  if (!mounted) return list;
  File dir = LittleFS.open(DIR);
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String name = f.name();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    f.close();
    if (!name.endsWith(".px")) continue;
    Drawing d;
    if (readFile(name.substring(0, name.length() - 3), d, false)) list.push_back(d);
  }
  // Oldest first: ids grow with time.
  std::sort(list.begin(), list.end(), [](const Drawing &a, const Drawing &b) {
    return a.id.length() != b.id.length() ? a.id.length() < b.id.length() : a.id < b.id;
  });
  return list;
}

bool galleryLoad(const String &id, Drawing &out) { return mounted && validId(id) && readFile(id, out, true); }

bool gallerySave(Drawing &d) {
  if (!mounted || d.frames.empty() || d.frames.size() % 256 || d.frames.size() / 256 > GALLERY_MAX_FRAMES) return false;
  d.count = d.frames.size() / 256;
  if (d.id.length() == 0) {
    if (galleryList().size() >= GALLERY_MAX_DRAWINGS) return false;
    // New id: seconds since 2020 in base 36, unique enough for a lamp.
    uint32_t n = time(nullptr) > 1577836800 ? time(nullptr) - 1577836800 : millis();
    do {
      String id;
      for (uint32_t v = n; v; v /= 36) id = "0123456789abcdefghijklmnopqrstuvwxyz"[v % 36] + id;
      d.id = id.length() ? id : String("0");
      n++;
    } while (LittleFS.exists(pathOf(d.id)));
  }
  if (!validId(d.id)) return false;
  File f = LittleFS.open(pathOf(d.id), "w");
  if (!f) return false;
  const String name = d.name.substring(0, 60);
  const uint8_t header[6] = {'P', 'X', '1', d.frameCount(), (uint8_t)(d.frameMs & 0xFF), (uint8_t)(d.frameMs >> 8)};
  f.write(header, 6);
  f.write((uint8_t)name.length());
  f.write((const uint8_t *)name.c_str(), name.length());
  const bool ok = f.write(d.frames.data(), d.frames.size()) == d.frames.size();
  f.close();
  return ok;
}

bool galleryDelete(const String &id) { return mounted && validId(id) && LittleFS.remove(pathOf(id)); }
