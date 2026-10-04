// The atlas of every picture the lamp draws, in the one sprite format of
// sprite.h, and the retouches made on the page.
#include "sprite.h"

#include <LittleFS.h>

#include <vector>

#include "display.h"

namespace sprites {

namespace {

struct Retouch {
  const Sprite *sprite;
  std::vector<String> rows;
};
std::vector<Retouch> retouches;
bool loaded = false;

String pathOf(const Sprite &s) { return String("/sprites/") + s.name; }

bool isSpriteChar(char c) {
  return c == '.' || (c >= '0' && c <= '9') || c == '#' || c == ':' || c == '+' || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z');
}

// h * frames rows of exactly w characters, or empty if `text` doesn't fit.
std::vector<String> parseRows(const Sprite &s, const String &text) {
  std::vector<String> rows;
  int start = 0;
  while (start <= (int)text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String row = text.substring(start, end);
    row.trim();
    if (row.length()) {
      for (unsigned i = 0; i < row.length(); i++) {
        if (!isSpriteChar(row[i])) row.setCharAt(i, '.');
      }
      rows.push_back(row);
    }
    start = end + 1;
  }
  if (rows.size() != (size_t)s.h * s.frames) return {};
  for (const String &r : rows) {
    if (r.length() != s.w) return {};
  }
  return rows;
}

void load() {
  if (loaded) return;
  loaded = true;
  for (uint16_t i = 0; i < ATLAS_COUNT; i++) {
    const Sprite &s = *ATLAS[i];
    if (!LittleFS.exists(pathOf(s))) continue;
    File f = LittleFS.open(pathOf(s), "r");
    if (!f) continue;
    const std::vector<String> rows = parseRows(s, f.readString());
    f.close();
    if (!rows.empty()) retouches.push_back({&s, rows});
  }
}

const Retouch *retouchOf(const Sprite &s) {
  load();
  for (const Retouch &r : retouches) {
    if (r.sprite == &s) return &r;
  }
  return nullptr;
}

const char *rowOf(const Sprite &s, int frame, int r) {
  const Retouch *t = retouches.empty() && loaded ? nullptr : retouchOf(s);
  const int i = frame * s.h + r;
  return t ? t->rows[i].c_str() : s.rows[i];
}

// What a character draws: a level, or -1 for nothing.
int levelOf(char ch, uint8_t level, MarkFn mark, void *context) {
  if (ch >= '0' && ch <= '9') return level * (ch - '0') / 9;
  switch (ch) {
    case '#': return level;
    case ':': return level / 3;
    case '+': return 255;
    default: break;
  }
  if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) return mark ? mark(ch, level, context) : level;
  return -1;
}

}  // namespace

void draw(const Sprite &s, int x, int y, int frame, uint8_t level, MarkFn mark, void *context) {
  frame = s.frames ? ((frame % s.frames) + s.frames) % s.frames : 0;
  for (int r = 0; r < s.h; r++) {
    const char *row = rowOf(s, frame, r);
    for (int c = 0; c < s.w && row[c]; c++) {
      const int l = levelOf(row[c], level, mark, context);
      if (l >= 0) display.setLevel(x + c, y + r, (uint8_t)l);
    }
  }
}

void drawFrom(const Sprite &s, int x, int y, int fromRow, int frame, uint8_t level) {
  frame = s.frames ? ((frame % s.frames) + s.frames) % s.frames : 0;
  for (int r = max(0, fromRow); r < s.h; r++) {
    const char *row = rowOf(s, frame, r);
    for (int c = 0; c < s.w && row[c]; c++) {
      const int l = levelOf(row[c], level, nullptr, nullptr);
      if (l >= 0) display.setLevel(x + c, y + r, (uint8_t)l);
    }
  }
}

int shade(const Sprite &s, int frame, int c, int r, uint8_t level) {
  return levelOf(at(s, frame, c, r), level, nullptr, nullptr);
}

int charLevel(char ch, uint8_t level) { return levelOf(ch, level, nullptr, nullptr); }

int frameAt(const Sprite &s, uint32_t now) { return s.frameMs && s.frames > 1 ? (now / s.frameMs) % s.frames : 0; }

char at(const Sprite &s, int frame, int c, int r) {
  if (c < 0 || r < 0 || c >= s.w || r >= s.h || frame < 0 || frame >= s.frames) return '.';
  const char ch = rowOf(s, frame, r)[c];
  return ch == ' ' ? '.' : ch;
}

void drawRows(int x, int y, const char *const *rows, int count, uint8_t level) {
  for (int r = 0; r < count; r++) {
    for (int c = 0; rows[r][c]; c++) {
      const int l = levelOf(rows[r][c], level, nullptr, nullptr);
      if (l >= 0) display.setLevel(x + c, y + r, (uint8_t)l);
    }
  }
}

const Sprite *find(const String &name) {
  for (uint16_t i = 0; i < ATLAS_COUNT; i++) {
    if (name == ATLAS[i]->name) return ATLAS[i];
  }
  return nullptr;
}

bool retouch(const Sprite &s, const String &text) {
  const std::vector<String> rows = parseRows(s, text);
  if (rows.empty()) return false;
  load();
  if (!LittleFS.exists("/sprites")) LittleFS.mkdir("/sprites");
  File f = LittleFS.open(pathOf(s), "w");
  if (!f) return false;
  for (const String &r : rows) {
    f.print(r);
    f.print('\n');
  }
  f.close();
  for (Retouch &r : retouches) {
    if (r.sprite == &s) {
      r.rows = rows;
      return true;
    }
  }
  retouches.push_back({&s, rows});
  return true;
}

void restore(const Sprite &s) {
  load();
  LittleFS.remove(pathOf(s));
  for (size_t i = 0; i < retouches.size(); i++) {
    if (retouches[i].sprite == &s) {
      retouches.erase(retouches.begin() + i);
      return;
    }
  }
}

bool retouched(const Sprite &s) { return retouchOf(s) != nullptr; }

String atlasJson() {
  String j = "[";
  for (uint16_t i = 0; i < ATLAS_COUNT; i++) {
    const Sprite &s = *ATLAS[i];
    if (i) j += ',';
    j += String("{\"name\":\"") + s.name + "\",\"w\":" + s.w + ",\"h\":" + s.h + ",\"frames\":" + s.frames +
         ",\"ms\":" + s.frameMs + ",\"retouched\":" + (retouched(s) ? "true" : "false") + ",\"rows\":[";
    for (int r = 0; r < s.h * s.frames; r++) {
      if (r) j += ',';
      j += '"';
      j += rowOf(s, r / s.h, r % s.h);
      j += '"';
    }
    j += "]}";
  }
  return j + "]";
}

}  // namespace sprites
