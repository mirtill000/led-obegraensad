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
extern Animation *const sonicAnimation;
extern Animation *const invaderIconAnimation;
extern Animation *const pacmanIconAnimation;
extern Animation *const terminalIconAnimation;
extern Animation *const rocketIconAnimation;
extern Animation *const coffeeIconAnimation;
extern Animation *const floppyIconAnimation;
extern Animation *const gameBoyIconAnimation;
extern Animation *const matrixIconAnimation;
extern Animation *const gliderIconAnimation;
extern Animation *const wifiIconAnimation;
extern Animation *const binaryClockAnimation;
extern Animation *const wordClockAnimation;
extern Animation *const englishWordClockAnimation;
extern Animation *const flipClockAnimation;
extern Animation *const sandClockAnimation;
extern Animation *const cubeAnimation;
extern Animation *const solidCubeAnimation;
extern Animation *const tunnelAnimation;
extern Animation *const sunSphereAnimation;
extern Animation *const voxelAnimation;
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
    sonicAnimation,
    invaderIconAnimation,
    pacmanIconAnimation,
    terminalIconAnimation,
    rocketIconAnimation,
    coffeeIconAnimation,
    floppyIconAnimation,
    gameBoyIconAnimation,
    matrixIconAnimation,
    gliderIconAnimation,
    wifiIconAnimation,
    binaryClockAnimation,
    wordClockAnimation,
    englishWordClockAnimation,
    flipClockAnimation,
    sandClockAnimation,
    cubeAnimation,
    solidCubeAnimation,
    tunnelAnimation,
    sunSphereAnimation,
    voxelAnimation,
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

const char *styleId(GameStyle style) {
  switch (style) {
    case GameStyle::Crisp: return "crisp";
    case GameStyle::Shaded: return "shaded";
    default: return "selectable";
  }
}
