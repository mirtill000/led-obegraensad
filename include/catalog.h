#pragma once

#include <Arduino.h>

#include <vector>

// The catalog (E1): everything the lamp can show, each with one id that
// the playlist and its time slots, the alarm, the notifications, the page,
// the API, Bluetooth and the Cardputer all use the same way:
//   "clock", "pet", ...   a mode, as its id
//   "a/voxel"               an animation (shown by the Animazioni mode)
//   "g/doom"              a game (Giochi; in demo when nobody picked it)
//   "d/<id>"              a drawing of the gallery (Disegni)
struct Scene {
  String id;     // as above
  String name;   // "Volo sul mare", "Il mio gatto"
  String group;  // "Modalità", the animation's group, "Giochi", "Disegni"
  char kind;     // 'M' mode, 'A' animation, 'G' game, 'D' drawing
};

// Every scene: modes (not tools nor hidden ones), animations by group,
// games, drawings.
std::vector<Scene> catalog();
// What shows a scene: the mode's id and what it picks inside it ("" for a
// plain mode). False if the id is not a scene. Drawings are not looked up
// (cheap enough for every second): validScene() does that.
bool sceneTarget(const String &id, String &modeId, String &pick);
bool validScene(const String &id);
// Whether a (valid) scene can be shown now: a seasonal animation only
// around its days. The playlist skips the others; showing one is refused.
bool sceneAvailable(const String &id);
String sceneName(const String &id);
// The catalog as JSON for the page: [{id, name, group, kind}].
String catalogJson();
