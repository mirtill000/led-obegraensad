#include "catalog.h"

#include "animation.h"
#include "gallery.h"
#include "modes.h"

namespace {

// "a/sea" -> the animation (of the right kind), or nullptr.
const Animation *animationOf(const String &id, bool game) {
  const Animation *a = findAnimation(id.substring(2));
  return a && !a->isClockFace() && a->isGame() == game ? a : nullptr;
}

String jsonText(const String &s) {
  String out = "\"";
  for (unsigned i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"' || c == '\\') out += '\\';
    if ((uint8_t)c >= 0x20) out += c;
  }
  return out + "\"";
}

}  // namespace

std::vector<Scene> catalog() {
  std::vector<Scene> out;
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    const Mode *m = MODES[i];
    if (!m->hidden() && !m->tool()) out.push_back({m->id(), m->name(), "Modalità", 'M'});
  }
  for (int games = 0; games < 2; games++) {  // the animations, then the games
    for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
      const Animation *a = ANIMATIONS[i];
      if (a->isClockFace() || a->isGame() != (games == 1)) continue;  // clock faces: styles of the Orologio
      if (games) out.push_back({String("g/") + a->id(), a->name(), "Giochi", 'G'});
      else out.push_back({String("a/") + a->id(), a->name(), a->group(), 'A'});
    }
  }
  for (const Drawing &d : galleryList()) out.push_back({"d/" + d.id, d.name, "Disegni", 'D'});
  return out;
}

bool sceneTarget(const String &id, String &modeId, String &pick) {
  pick = "";
  if (id.startsWith("a/") || id.startsWith("g/")) {
    const bool game = id[0] == 'g';
    if (!animationOf(id, game)) return false;
    modeId = game ? "games" : "ambient";
    pick = id.substring(2);
    return true;
  }
  if (id.startsWith("d/")) {
    if (id.length() < 3) return false;
    modeId = "gallery";
    pick = id.substring(2);
    return true;
  }
  if (!validModeId(id)) return false;
  modeId = id;
  return true;
}

bool validScene(const String &id) {
  String mode, pick;
  if (!sceneTarget(id, mode, pick)) return false;
  return !id.startsWith("d/") || galleryHas(pick);
}

String sceneName(const String &id) {
  if (id.startsWith("a/") || id.startsWith("g/")) {
    const Animation *a = animationOf(id, id[0] == 'g');
    return a ? a->name() : id;
  }
  if (id.startsWith("d/")) {
    Drawing d;
    return galleryLoad(id.substring(2), d) ? d.name : id;
  }
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    if (id == MODES[i]->id()) return MODES[i]->name();
  }
  return id;
}

String catalogJson() {
  String j = "[";
  for (const Scene &s : catalog()) {
    if (j.length() > 1) j += ',';
    j += "{\"id\":" + jsonText(s.id) + ",\"name\":" + jsonText(s.name) + ",\"group\":" + jsonText(s.group) +
         ",\"kind\":\"" + s.kind + "\"}";
  }
  return j + "]";
}
