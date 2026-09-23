#include "animation.h"

// Each animation's instance is defined next to its class, in this folder.
extern Animation *const rainAnimation;
extern Animation *const fireAnimation;
extern Animation *const starsAnimation;
extern Animation *const wavesAnimation;
extern Animation *const breathAnimation;
extern Animation *const tetrisAnimation;
extern Animation *const snakeAnimation;
extern Animation *const analogClockAnimation;
extern Animation *const binaryClockAnimation;
extern Animation *const wordClockAnimation;
extern Animation *const cubeAnimation;
extern Animation *const tunnelAnimation;
extern Animation *const plasmaAnimation;
extern Animation *const metaballsAnimation;
extern Animation *const mandelbrotAnimation;

// Menu order; the page groups them by Animation::group().
Animation *const ANIMATIONS[] = {
    rainAnimation,
    fireAnimation,
    starsAnimation,
    wavesAnimation,
    breathAnimation,
    tetrisAnimation,
    snakeAnimation,
    analogClockAnimation,
    binaryClockAnimation,
    wordClockAnimation,
    cubeAnimation,
    tunnelAnimation,
    plasmaAnimation,
    metaballsAnimation,
    mandelbrotAnimation,
};
const uint8_t ANIMATION_COUNT = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);

Animation *findAnimation(const String &id) {
  for (Animation *a : ANIMATIONS) {
    if (id == a->id()) return a;
  }
  return nullptr;
}
