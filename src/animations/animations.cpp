#include "animation.h"

// Each animation's instance is defined next to its class, in this folder.
extern Animation *const rainAnimation;
extern Animation *const fireAnimation;
extern Animation *const starsAnimation;
extern Animation *const wavesAnimation;
extern Animation *const marioAnimation;
extern Animation *const tetrisAnimation;
extern Animation *const snakeAnimation;
extern Animation *const pongAnimation;
extern Animation *const breakoutAnimation;
extern Animation *const flappyAnimation;
extern Animation *const invadersAnimation;
extern Animation *const maze3dAnimation;
extern Animation *const runnerAnimation;
extern Animation *const kongAnimation;
extern Animation *const doomAnimation;
extern Animation *const invaderIconAnimation;
extern Animation *const pacmanIconAnimation;
extern Animation *const terminalIconAnimation;
extern Animation *const heartIconAnimation;
extern Animation *const rocketIconAnimation;
extern Animation *const coffeeIconAnimation;
extern Animation *const batteryIconAnimation;
extern Animation *const binaryClockAnimation;
extern Animation *const wordClockAnimation;
extern Animation *const cubeAnimation;
extern Animation *const plasmaAnimation;
extern Animation *const metaballsAnimation;
extern Animation *const mandelbrotAnimation;

// Menu order; the page groups them by Animation::group().
Animation *const ANIMATIONS[] = {
    rainAnimation,
    fireAnimation,
    starsAnimation,
    wavesAnimation,
    marioAnimation,
    tetrisAnimation,
    snakeAnimation,
    pongAnimation,
    breakoutAnimation,
    flappyAnimation,
    invadersAnimation,
    maze3dAnimation,
    runnerAnimation,
    kongAnimation,
    doomAnimation,
    invaderIconAnimation,
    pacmanIconAnimation,
    terminalIconAnimation,
    heartIconAnimation,
    rocketIconAnimation,
    coffeeIconAnimation,
    batteryIconAnimation,
    binaryClockAnimation,
    wordClockAnimation,
    cubeAnimation,
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
