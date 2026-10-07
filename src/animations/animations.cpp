#include "animation.h"

// Each animation's instance is defined next to its class, in this folder.
extern Animation *const rainAnimation;
extern Animation *const fireAnimation;
extern Animation *const starsAnimation;
extern Animation *const wavesAnimation;
extern Animation *const skyAnimation;
extern Animation *const aquariumAnimation;
extern Animation *const lighthouseAnimation;
extern Animation *const firefliesAnimation;
extern Animation *const auroraAnimation;
extern Animation *const rainGlassAnimation;
extern Animation *const campfireAnimation;
extern Animation *const meteorsAnimation;
extern Animation *const starTrailsAnimation;
extern Animation *const nightTrainAnimation;
extern Animation *const breathAnimation;
extern Animation *const marioAnimation;
extern Animation *const tetrisAnimation;
extern Animation *const snakeAnimation;
extern Animation *const tronAnimation;
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
extern Animation *const skullIconAnimation;
extern Animation *const houseIconAnimation;
extern Animation *const burgerIconAnimation;
extern Animation *const birdIconAnimation;
extern Animation *const diceIconAnimation;
extern Animation *const binaryClockAnimation;
extern Animation *const wordClockAnimation;
extern Animation *const englishWordClockAnimation;
extern Animation *const cubeAnimation;
extern Animation *const solidCubeAnimation;
extern Animation *const tunnelAnimation;
extern Animation *const metaball3dAnimation;
extern Animation *const planetAnimation;
extern Animation *const saturnMoonAnimation;
extern Animation *const saturnCloseAnimation;
extern Animation *const voxelAnimation;
extern Animation *const cityAnimation;
extern Animation *const cloudsAnimation;
extern Animation *const plasmaAnimation;
extern Animation *const metaballsAnimation;
extern Animation *const mandelbrotAnimation;
extern Animation *const snowAnimation;
extern Animation *const xmasTreeAnimation;
extern Animation *const fireworksAnimation;
extern Animation *const heartsAnimation;
extern Animation *const pumpkinAnimation;
extern Animation *const cakeAnimation;

// Menu order; the page groups them by Animation::group().
Animation *const ANIMATIONS[] = {
    rainAnimation,
    fireAnimation,
    starsAnimation,
    wavesAnimation,
    skyAnimation,
    aquariumAnimation,
    lighthouseAnimation,
    firefliesAnimation,
    auroraAnimation,
    rainGlassAnimation,
    campfireAnimation,
    meteorsAnimation,
    starTrailsAnimation,
    nightTrainAnimation,
    breathAnimation,
    marioAnimation,
    tetrisAnimation,
    snakeAnimation,
    tronAnimation,
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
    skullIconAnimation,
    houseIconAnimation,
    burgerIconAnimation,
    birdIconAnimation,
    diceIconAnimation,
    binaryClockAnimation,
    wordClockAnimation,
    englishWordClockAnimation,
    cubeAnimation,
    solidCubeAnimation,
    tunnelAnimation,
    metaball3dAnimation,
    planetAnimation,
    saturnMoonAnimation,
    saturnCloseAnimation,
    voxelAnimation,
    cityAnimation,
    cloudsAnimation,
    plasmaAnimation,
    metaballsAnimation,
    mandelbrotAnimation,
    snowAnimation,
    xmasTreeAnimation,
    fireworksAnimation,
    heartsAnimation,
    pumpkinAnimation,
    cakeAnimation,
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
