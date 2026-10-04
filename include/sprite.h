#pragma once

#include <Arduino.h>

// One format for every picture the lamp draws - icons, characters, the
// pet, the weather, the notifications - kept in one atlas (sprites.cpp) so
// the page can list them, show them and let you retouch them.
//
// A sprite is rows of characters, top row first; an animated one has its
// frames one after the other (h rows each). The characters:
//   '.' ' '  transparent: what is below stays
//   '0'-'9'  a shade of the level it is drawn at: '9' all of it, '1' a
//            ninth, '0' off (an opaque black pixel)
//   '#'      the level it is drawn at (as '9')
//   ':'      a third of it (as '3')
//   '+'      always full light, whatever the level
//   letters  marks: what they mean is up to the code drawing the sprite
//            (the pet's eyes and mouth...); without one, they draw as '#'
struct Sprite {
  const char *name;          // "group.name", e.g. "pet.frog", "notify.bell"
  uint8_t w, h, frames;
  uint16_t frameMs;          // animated by time; 0 = the code picks the frame
  const char *const *rows;   // h * frames rows of w characters
};

namespace sprites {

// What a mark (a letter) becomes: a level 0-255, or -1 for transparent.
using MarkFn = int (*)(char mark, uint8_t level, void *context);

// Draws `frame` of `s` with its top-left corner at (x, y). Pixels outside
// the panel are skipped. Any retouched version from the page is used.
void draw(const Sprite &s, int x, int y, int frame = 0, uint8_t level = 255, MarkFn mark = nullptr,
          void *context = nullptr);
// Only rows fromRow.. of `frame`, each where it would be anyway (a sprite
// being eaten from the top, sliding out of view...).
void drawFrom(const Sprite &s, int x, int y, int fromRow, int frame = 0, uint8_t level = 255);
// The frame an animated sprite shows at `now` (ms).
int frameAt(const Sprite &s, uint32_t now);
// The character at column c, row r of `frame` ('.' outside the sprite),
// retouches included: for code that looks at a sprite's shape.
char at(const Sprite &s, int frame, int c, int r);
// What the pixel at column c, row r of `frame` draws at `level`: a level,
// or -1 if it is transparent (marks count as lit) - for collisions and
// for code that scales a sprite.
int shade(const Sprite &s, int frame, int c, int r, uint8_t level = 255);
// What one sprite character draws at `level` (-1 transparent, marks lit).
int charLevel(char ch, uint8_t level);
// Rows given directly (count rows of any width), same characters.
void drawRows(int x, int y, const char *const *rows, int count, uint8_t level = 255);

// The atlas: every sprite, by name.
extern const Sprite *const ATLAS[];
extern const uint16_t ATLAS_COUNT;
const Sprite *find(const String &name);

// Retouches from the page, kept in LittleFS (/sprites/<name>). `rows` is
// h * frames lines of w characters, separated by '\n'; anything not a
// sprite character becomes '.'. Returns false if the size doesn't match.
bool retouch(const Sprite &s, const String &rows);
void restore(const Sprite &s);  // back to the built-in picture
bool retouched(const Sprite &s);
// The atlas as JSON for the page: [{name, w, h, frames, ms, rows: [...], retouched}].
String atlasJson();

}  // namespace sprites
