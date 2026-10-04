#include "live.h"

#include <string.h>

#include "animation.h"
#include "catalog.h"
#include "display.h"
#include "modes.h"
#include "modes/ambient_mode.h"
#include "settings.h"
#include "timekeeping.h"

void packedFrame(uint8_t out[LIVE_FRAME_BYTES]) {
  for (int i = 0; i < LIVE_FRAME_BYTES; i++) {
    const int x = (2 * i) % COLS, y = (2 * i) / COLS;
    out[i] = (display.shownLevel(x, y) >> 4) << 4 | display.shownLevel(x + 1, y) >> 4;
  }
}

void packedFrameHex(char out[LIVE_FRAME_BYTES * 2 + 1]) {
  static const char DIGITS[] = "0123456789abcdef";
  uint8_t frame[LIVE_FRAME_BYTES];
  packedFrame(frame);
  for (int i = 0; i < LIVE_FRAME_BYTES; i++) {
    out[2 * i] = DIGITS[frame[i] >> 4];
    out[2 * i + 1] = DIGITS[frame[i] & 15];
  }
  out[LIVE_FRAME_BYTES * 2] = 0;
}

String catalogText() {
  // The scene catalog (catalog.h), one line each: kind, the id without its
  // "a/", "g/", "d/" (a remote rebuilds the scene id from the kind), name.
  String out;
  for (const Scene &s : catalog()) {
    out += String(s.kind) + "\t" + (s.kind == 'M' ? s.id : s.id.substring(2)) + "\t" + s.name + "\n";
  }
  return out;
}

static String quoted(const char *s) {
  String out = "\"";
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    out += *s;
  }
  return out + "\"";
}

// "c": the keys, "ca": what A does, "cl": all five labels ("" = the plain
// arrow), separated by '|' (e.g. "Pappa|Gioca|Pulisci|Medicina|Coccole").
static String controlsFields(const GameControls *c) {
  String labels;
  for (int i = 0; i < 5; i++) labels += String(i ? "|" : "") + (c->labels[i] ? c->labels[i] : "");
  return ",\"c\":" + quoted(c->keys) + ",\"ca\":" + quoted(c->labels[4] ? c->labels[4] : "Salta") +
         ",\"cl\":" + quoted(labels.c_str()) + ",\"n\":" + String(c->players);
}

String summaryJson() {
  Mode *m = currentMode();
  String j = "{\"m\":" + quoted(m->id()) + ",\"mn\":" + quoted(m->name());
  j += ",\"x\":" + quoted(m->actionName() ? m->actionName() : "");
  const char *game = m->gameId();
  if (game) {
    const bool player = strcmp(m->id(), "ambient") == 0 || strcmp(m->id(), "games") == 0;
    const AmbientMode *a = player ? static_cast<const AmbientMode *>(m) : nullptr;
    const bool forced = a && a->demoForced();
    j += ",\"g\":" + quoted(game) + ",\"gn\":" + quoted(a && a->playing() ? a->playing()->name() : m->name());
    j += String(",\"d\":") + (forced || demoMode(game) ? 1 : 0) + ",\"f\":" + (forced ? 1 : 0);
    const Animation *g = findAnimation(game);
    const GameControls *c = g ? g->controls() : nullptr;
    if (c) j += controlsFields(c);
  } else if (m->controls()) {
    j += controlsFields(m->controls());  // a mode that takes keys (the pet)
  }
  const String line = m->status();
  if (line.length()) j += ",\"s\":" + quoted(line.c_str());
  j += ",\"b\":" + String(settings.brightness);
  struct tm t;
  if (localTime(t)) {
    char hhmm[6];
    strftime(hhmm, sizeof(hhmm), "%H:%M", &t);
    j += ",\"t\":\"" + String(hhmm) + "\"";
  }
  return j + "}";
}

