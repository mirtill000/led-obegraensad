// The atlas: every picture the lamp draws, in the format of sprite.h.
// Retouches made on the page are kept apart (LittleFS) and win over these.
#include "sprite_atlas.h"

namespace spr {

static const char *const NOTIFY_BELL_ROWS[] = {
    "....#....", "...###...", "..#####..", "..#####..", "..#####..", ".#######.", ".#######.", "#########", ".........", "...###...",
};
const Sprite NOTIFY_BELL = {"notify.bell", 9, 10, 1, 0, NOTIFY_BELL_ROWS};

static const char *const NOTIFY_MAIL_ROWS[] = {
    "############", "##........##", "#.#......#.#", "#..#....#..#", "#...#..#...#", "#....##....#", "#..........#", "############",
};
const Sprite NOTIFY_MAIL = {"notify.mail", 12, 8, 1, 0, NOTIFY_MAIL_ROWS};

static const char *const NOTIFY_CHECK_ROWS[] = {
    "..........#", ".........##", "........##.", "#......##..", "##....##...", ".##..##....", "..####.....", "...##......",
};
const Sprite NOTIFY_CHECK = {"notify.check", 11, 8, 1, 0, NOTIFY_CHECK_ROWS};

static const char *const NOTIFY_ALERT_ROWS[] = {
    ".....#.....", "....###....", "....#.#....", "...##.##...", "...##.##...", "..###.###..", "..#######..", ".####.####.", "###########",
};
const Sprite NOTIFY_ALERT = {"notify.alert", 11, 9, 1, 0, NOTIFY_ALERT_ROWS};

static const char *const NOTIFY_HEART_ROWS[] = {
    ".##...##.", "####.####", "#########", "#########", ".#######.", "..#####..", "...###...", "....#....",
};
const Sprite NOTIFY_HEART = {"notify.heart", 9, 8, 1, 0, NOTIFY_HEART_ROWS};

static const char *const NOTIFY_PHONE_ROWS[] = {
    "#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#######", "#..#..#", "#######",
};
const Sprite NOTIFY_PHONE = {"notify.phone", 7, 11, 1, 0, NOTIFY_PHONE_ROWS};

static const char *const NOTIFY_HOME_ROWS[] = {
    ".....#.....", "....#.#....", "...#...#...", "..#.....#..", ".#.......#.", "###########", ".#.......#.", ".#..###..#.", ".#..#.#..#.", ".####.####.",
};
const Sprite NOTIFY_HOME = {"notify.home", 11, 10, 1, 0, NOTIFY_HOME_ROWS};

static const char *const NOTIFY_STAR_ROWS[] = {
    ".....#.....", ".....#.....", "....###....", "###########", ".#########.", "..#######..", "..#######..", ".####.####.", ".###...###.", ".#.......#.",
};
const Sprite NOTIFY_STAR = {"notify.star", 11, 10, 1, 0, NOTIFY_STAR_ROWS};

// a felt egg; ':' felt spots
static const char *const PET_EGG_ROWS[] = {
    "..##..", ".####.", ".#:##.", "######", "##:###", "######", ".####.", "..##..",
};
const Sprite PET_EGG = {"pet.egg", 6, 8, 1, 0, PET_EGG_ROWS};

// marks: w eye bumps (bright), e eyes, m mouth (rows 5-6 swap when sad)
static const char *const PET_FROG_ROWS[] = {
    ".www.www.", ".wew.wew.", ".www#www.", "#########", "#########", "##m###m##", "###mmm###", ".#######.",
};
const Sprite PET_FROG = {"pet.frog", 9, 8, 1, 0, PET_FROG_ROWS};

static const char *const PET_APPLE_ROWS[] = {
    "..#..", ".###.", "#####", "#####", ".###.",
};
const Sprite PET_APPLE = {"pet.apple", 5, 5, 1, 0, PET_APPLE_ROWS};

static const char *const PET_NOTE_ROWS[] = {
    "..##.", "..#.#", "..#..", "###..", "##...",
};
const Sprite PET_NOTE = {"pet.note", 5, 5, 1, 0, PET_NOTE_ROWS};

static const char *const PET_CROSS_ROWS[] = {
    "..#..", "..#..", "#####", "..#..", "..#..",
};
const Sprite PET_CROSS = {"pet.cross", 5, 5, 1, 0, PET_CROSS_ROWS};

static const char *const PET_HEART_ROWS[] = {
    ".#.#.", "#####", ".###.", "..#..",
};
const Sprite PET_HEART = {"pet.heart", 5, 4, 1, 0, PET_HEART_ROWS};

static const char *const PET_POOP_ROWS[] = {
    ".#.", "##.", "###",
};
const Sprite PET_POOP = {"pet.poop", 3, 3, 1, 0, PET_POOP_ROWS};

static const char *const PET_ZED_ROWS[] = {
    "####", "..#.", ".#..", "####",
};
const Sprite PET_ZED = {"pet.zed", 4, 4, 1, 0, PET_ZED_ROWS};

static const char *const GEEK_INVADER_ROWS[] = {
    "..#.....#..", "...#...#...", "..#######..", ".##.###.##.", "###########", "#.#######.#", "#.#.....#.#", "...##.##...",
    "..#.....#..", "#..#...#..#", "#.#######.#", "###.###.###", "###########", ".#########.", "..#.....#..", ".#.......#.",
};
const Sprite GEEK_INVADER = {"geek.invader", 11, 8, 2, 0, GEEK_INVADER_ROWS};

static const char *const GEEK_PACMAN_ROWS[] = {
    ".###.", "####.", "###..", "####.", ".###.",
    ".###.", "#####", "#####", "#####", ".###.",
};
const Sprite GEEK_PACMAN = {"geek.pacman", 5, 5, 2, 0, GEEK_PACMAN_ROWS};

static const char *const GEEK_GHOST_ROWS[] = {
    ".###.", "#####", "#.#.#", "#####", "#.#.#",
    ".###.", "#####", "#.#.#", "#####", ".#.#.",
};
const Sprite GEEK_GHOST = {"geek.ghost", 5, 5, 2, 0, GEEK_GHOST_ROWS};

static const char *const GEEK_ROCKET_ROWS[] = {
    "..#..", ".###.", ".###.", ".#.#.", ".###.", ".###.", "#####", "#.#.#",
};
const Sprite GEEK_ROCKET = {"geek.rocket", 5, 8, 1, 0, GEEK_ROCKET_ROWS};

static const char *const GEEK_FLAME_ROWS[] = {
    ".#.#.", "..#..",
    "..#..", ".#.#.",
};
const Sprite GEEK_FLAME = {"geek.flame", 5, 2, 2, 0, GEEK_FLAME_ROWS};

static const char *const GEEK_CUP_ROWS[] = {
    "########.", "#######.#", "#######.#", "########.", ".######..", "..####...", "#########",
};
const Sprite GEEK_CUP = {"geek.cup", 9, 7, 1, 0, GEEK_CUP_ROWS};

static const char *const GEEK_GLIDER_ROWS[] = {
    ".#.", "..#", "###",
    "#.#", ".##", ".#.",
    "..#", "#.#", ".##",
    "#..", ".##", "##.",
};
const Sprite GEEK_GLIDER = {"geek.glider", 3, 3, 4, 0, GEEK_GLIDER_ROWS};

// Marks of the new geek icons: e eyes (skull, bird, robot), g glass and f
// filament (bulb), w windows and d door (house), s sesame (burger), a the
// robot's antenna.
static const char *const GEEK_SKULL_ROWS[] = {
    "...######...", ".##########.", "############", "##...##...##", "##.e.##.e.##", "##...##...##", "#####..#####", ".####..####.", "..########..", "..#.#..#.#..", "..########..", "............",
    "...######...", ".##########.", "############", "##...##...##", "##.e.##.e.##", "##...##...##", "#####..#####", ".####..####.", "..########..", "..#.#..#.#..", "............", "..########..",
};
const Sprite GEEK_SKULL = {"geek.skull", 12, 12, 2, 0, GEEK_SKULL_ROWS};

static const char *const GEEK_BULB_ROWS[] = {
    "...#####...", "..#ggggg#..", ".#ggggggg#.", "#ggggggggg#", "#ggfgggfgg#", "#gggfgfggg#", "#ggggfgggg#", ".#gggfggg#.", "..#ggfgg#..", "...#ggg#...", "...:::::...", "...#####...", "...:::::...", "....###....",
};
const Sprite GEEK_BULB = {"geek.bulb", 11, 14, 1, 0, GEEK_BULB_ROWS};

static const char *const GEEK_HOUSE_ROWS[] = {
    "..........##..", "......#...##..", ".....###..##..", "....#####.##..", "...#########..", "..###########.", ".#############", "..#.........#.", "..#.ww...ww.#.", "..#.ww...ww.#.", "..#....d....#.", "..#...ddd...#.", "..###########.",
};
const Sprite GEEK_HOUSE = {"geek.house", 14, 13, 1, 0, GEEK_HOUSE_ROWS};

static const char *const GEEK_BURGER_TOP_ROWS[] = {
    "....######....", "..##########..", ".###s##s###s#.", "##############",
};
const Sprite GEEK_BURGER_TOP = {"geek.burger-top", 14, 4, 1, 0, GEEK_BURGER_TOP_ROWS};

static const char *const GEEK_BURGER_BOTTOM_ROWS[] = {
    ".:.:.:.:.:.:.:", "##############", "##############", ".::::::::::::.", "##############", ".############.",
};
const Sprite GEEK_BURGER_BOTTOM = {"geek.burger-bottom", 14, 6, 1, 0, GEEK_BURGER_BOTTOM_ROWS};

static const char *const GEEK_MONITOR_ROWS[] = {
    "##############", "#............#", "#............#", "#............#", "#............#", "#............#", "#............#", "#............#", "##############", "......##......", "....######....",
};
const Sprite GEEK_MONITOR = {"geek.monitor", 14, 11, 1, 0, GEEK_MONITOR_ROWS};

static const char *const GEEK_BIRD_ROWS[] = {
    "....###..", "...#####.", "...##e##:", "#.######.", "########.", ".######..", "...#.#...",
    "....###..", "...#####.", "#..##e##:", "########.", ".#######.", "..#####..", "...#.#...",
};
const Sprite GEEK_BIRD = {"geek.bird", 9, 7, 2, 0, GEEK_BIRD_ROWS};

static const char *const GEEK_ROBOT_ROWS[] = {
    "....a....", "....#....", ".#######.", ".#e###e#.", ".#######.", ".##...##.", ".#######.", "...###...", "#########", "#.#####.#", "#.##:##.#", "..#####..", "..#...#..", "..#...#..",
    "....a....", "....#....", ".#######.", ".#e###e#.", ".#######.", ".##...##.", ".#######.", "...###...", "#########", "#.#####.#", "#.##:##.#", "..#####..", "..#..#...", ".#....#..",
};
const Sprite GEEK_ROBOT = {"geek.robot", 9, 14, 2, 0, GEEK_ROBOT_ROWS};

static const char *const GEEK_DIE_ROWS[] = {
    ".##########.", "#..........#", "#..........#", "#..........#", "#..........#", "#..........#", "#..........#", "#..........#", "#..........#", "#..........#", "#..........#", ".##########.",
};
const Sprite GEEK_DIE = {"geek.die", 12, 12, 1, 0, GEEK_DIE_ROWS};

static const char *const SEASON_PINE_ROWS[] = {
    "..#..", ".###.", "..#..", ".###.", "#####", "..#..",
};
const Sprite SEASON_PINE = {"season.pine", 5, 6, 1, 0, SEASON_PINE_ROWS};

static const char *const SEASON_HEART_ROWS[] = {
    ".#.#.", "#####", "#####", ".###.", "..#..",
};
const Sprite SEASON_HEART = {"season.heart", 5, 5, 1, 0, SEASON_HEART_ROWS};

static const char *const SEASON_PUMPKIN_ROWS[] = {
    "......##......", "......#.......", "..##########..", ".############.", "##############", "##############", "##############", "##############", "##############", ".############.", "..##########..",
};
const Sprite SEASON_PUMPKIN = {"season.pumpkin", 14, 11, 1, 0, SEASON_PUMPKIN_ROWS};

static const char *const SEASON_PUMPKIN_FACE_ROWS[] = {
    "..............", "..............", "..............", "..............", "...#......#...", "..###....###..", "..............", "...#.#..#.#...", "....#.##.#....", "..............", "..............",
};
const Sprite SEASON_PUMPKIN_FACE = {"season.pumpkin-face", 14, 11, 1, 0, SEASON_PUMPKIN_FACE_ROWS};

static const char *const SEASON_CAKE_ROWS[] = {
    "############", "#:#:##:#:##:", "############", "############", "#+#+#+#+#+#+", "############",
};
const Sprite SEASON_CAKE = {"season.cake", 12, 6, 1, 0, SEASON_CAKE_ROWS};

static const char *const GALLERY_PENCIL_ROWS[] = {
    "......##", ".....#.#", "....#.#.", "...#.#..", "..#.#...", ".##.....", ".#......", "#.......",
};
const Sprite GALLERY_PENCIL = {"gallery.pencil", 8, 8, 1, 0, GALLERY_PENCIL_ROWS};

static const char *const RUNNER_DINO_ROWS[] = {
    "...###", "...#.#", "...###", "#.###.", "####.#", ".###..", ".#..#.",
    "...###", "...#.#", "...###", "#.###.", "####.#", ".###..", "..##..",
};
const Sprite RUNNER_DINO = {"runner.dino", 6, 7, 2, 0, RUNNER_DINO_ROWS};

static const char *const RUNNER_DUCK_ROWS[] = {
    "....##", "######", ".#####", ".#..#.",
    "....##", "######", ".#####", "..##..",
};
const Sprite RUNNER_DUCK = {"runner.duck", 6, 4, 2, 0, RUNNER_DUCK_ROWS};

static const char *const RUNNER_CACTUS1_ROWS[] = {
    ".#.", "##.", ".##", ".#.", ".#.",
};
const Sprite RUNNER_CACTUS1 = {"runner.cactus1", 3, 5, 1, 0, RUNNER_CACTUS1_ROWS};

static const char *const RUNNER_CACTUS2_ROWS[] = {
    "#", "#", "#", "#",
};
const Sprite RUNNER_CACTUS2 = {"runner.cactus2", 1, 4, 1, 0, RUNNER_CACTUS2_ROWS};

static const char *const RUNNER_CACTUS3_ROWS[] = {
    "#.#", "###", ".#.", ".#.", ".#.",
};
const Sprite RUNNER_CACTUS3 = {"runner.cactus3", 3, 5, 1, 0, RUNNER_CACTUS3_ROWS};

static const char *const RUNNER_BIRD_ROWS[] = {
    "#....", ".###.", "...##",
    ".....", ".####", "#..#.",
};
const Sprite RUNNER_BIRD = {"runner.bird", 5, 3, 2, 0, RUNNER_BIRD_ROWS};

static const char *const SONIC_RUN_ROWS[] = {
    "##.", ".##", ".#.", "#.#",
    "##.", ".##", ".#.", ".#.",
};
const Sprite SONIC_RUN = {"sonic.run", 3, 4, 2, 0, SONIC_RUN_ROWS};

static const char *const SONIC_BALL_ROWS[] = {
    ".#.", "###", ".#.",
    "#.#", ".#.", "#.#",
};
const Sprite SONIC_BALL = {"sonic.ball", 3, 3, 2, 0, SONIC_BALL_ROWS};

static const char *const SONIC_BUG_ROWS[] = {
    "###", "#.#",
};
const Sprite SONIC_BUG = {"sonic.motobug", 3, 2, 1, 0, SONIC_BUG_ROWS};

static const char *const DOOM_IMP_ROWS[] = {
    "#...#", ".###.", "#.#.#", "#####", ".###.", ".#.#.",
};
const Sprite DOOM_IMP = {"doom.imp", 5, 6, 1, 0, DOOM_IMP_ROWS};

static const char *const DOOM_IMP_DEAD_ROWS[] = {
    "#.##.", "#####",
};
const Sprite DOOM_IMP_DEAD = {"doom.imp-dead", 5, 2, 1, 0, DOOM_IMP_DEAD_ROWS};

static const char *const DOOM_FIREBALL_ROWS[] = {
    "##", "##",
};
const Sprite DOOM_FIREBALL = {"doom.fireball", 2, 2, 1, 0, DOOM_FIREBALL_ROWS};

static const char *const DOOM_GUN_ROWS[] = {
    ".##.", ".##.", "####",
};
const Sprite DOOM_GUN = {"doom.gun", 4, 3, 1, 0, DOOM_GUN_ROWS};

// frames: run, run, jump; marks: C cap, O overalls, S skin, B shoes, M hair/eye/moustache
static const char *const MARIO_ROWS[] = {
    ".CCC.", "CCCCC", "MSSMS", "SSMMM", ".OOO.", "OO.OO", "B...B",
    ".CCC.", "CCCCC", "MSSMS", "SSMMM", ".OOO.", ".OOO.", ".BB..",
    "SCCC.", "CCCCC", "MSSMS", "SSMMM", "OOOOS", "OO.OO", "B..B.",
};
const Sprite MARIO = {"mario.mario", 5, 7, 3, 0, MARIO_ROWS};

static const char *const MARIO_GOOMBA_ROWS[] = {
    "###", "#..",
    "###", "..#",
};
const Sprite MARIO_GOOMBA = {"mario.goomba", 3, 2, 2, 0, MARIO_GOOMBA_ROWS};

static const char *const MARIO_CLOUD_ROWS[] = {
    ".##.", "####",
};
const Sprite MARIO_CLOUD = {"mario.cloud", 4, 2, 1, 0, MARIO_CLOUD_ROWS};

static const char *const WEATHER_SUN_ROWS[] = {
    "......", "..##..", ".####.", ".####.", "..##..", "......", "......",
    "#....#", "..##..", ".####.", ".####.", "..##..", "#....#", "......",
    ".#..#.", "#.##.#", ".####.", ".####.", "#.##.#", ".#..#.", "......",
    "#....#", "..##..", ".####.", ".####.", "..##..", "#....#", "......",
};
const Sprite WEATHER_SUN = {"weather.sun", 6, 7, 4, 400, WEATHER_SUN_ROWS};

static const char *const WEATHER_MOON_ROWS[] = {
    "..##..", ".#....", "#.....", "#.....", ".#....", "..##..", "......",
    "..##.#", ".#....", "#.....", "#...#.", ".#....", "..##..", "......",
    "..##..", ".#..#.", "#.....", "#.....", ".#...#", "..##..", "......",
};
const Sprite WEATHER_MOON = {"weather.moon", 6, 7, 3, 700, WEATHER_MOON_ROWS};

static const char *const WEATHER_PARTLY_ROWS[] = {
    "#.#...", ".#....", "#.##..", "..####", ".#####", "......", "......",
    ".#....", "###...", ".###..", "..####", ".#####", "......", "......",
};
const Sprite WEATHER_PARTLY = {"weather.partly", 6, 7, 2, 600, WEATHER_PARTLY_ROWS};

static const char *const WEATHER_CLOUD_ROWS[] = {
    "......", "..##..", ".####.", "######", ".####.", "......", "......",
    "......", "...##.", "..####", ".#####", "..####", "......", "......",
    "......", "..##..", ".####.", "######", ".####.", "......", "......",
    "......", ".##...", "####..", "#####.", "####..", "......", "......",
};
const Sprite WEATHER_CLOUD = {"weather.cloud", 6, 7, 4, 700, WEATHER_CLOUD_ROWS};

static const char *const WEATHER_FOG_ROWS[] = {
    "......", "#####.", "......", ".#####", "......", "#####.", "......",
    "......", ".#####", "......", "#####.", "......", ".#####", "......",
};
const Sprite WEATHER_FOG = {"weather.fog", 6, 7, 2, 800, WEATHER_FOG_ROWS};

static const char *const WEATHER_RAIN_ROWS[] = {
    "..##..", ".####.", "######", "......", "#...#.", "..#...", "....#.",
    "..##..", ".####.", "######", "#...#.", "..#...", "....#.", "#.....",
    "..##..", ".####.", "######", "..#...", "....#.", "#.....", "..#...",
    "..##..", ".####.", "######", "....#.", "#.....", "..#...", "#...#.",
};
const Sprite WEATHER_RAIN = {"weather.rain", 6, 7, 4, 150, WEATHER_RAIN_ROWS};

static const char *const WEATHER_SNOW_ROWS[] = {
    "..##..", ".####.", "######", "......", ".#..#.", "......", "...#..",
    "..##..", ".####.", "######", ".#....", "....#.", "..#...", "......",
    "..##..", ".####.", "######", "....#.", "......", ".#..#.", "......",
    "..##..", ".####.", "######", "......", "...#..", "......", ".#..#.",
};
const Sprite WEATHER_SNOW = {"weather.snow", 6, 7, 4, 350, WEATHER_SNOW_ROWS};

static const char *const WEATHER_STORM_ROWS[] = {
    "..##..", ".####.", "######", "...#..", "..#...", "...#..", "..#...",
    "..##..", ".####.", "######", "......", "......", "......", "......",
    "..##..", ".####.", "######", "...#..", "..#...", "...#..", "..#...",
    "..##..", ".####.", "######", "......", "......", "......", "......",
    "..##..", ".####.", "######", "......", "......", "......", "......",
    "..##..", ".####.", "######", "......", "......", "......", "......",
};
const Sprite WEATHER_STORM = {"weather.storm", 6, 7, 6, 150, WEATHER_STORM_ROWS};

static const char *const WEATHER_UMBRELLA_ROWS[] = {
    "..##..", ".####.", "######", "...#..", "...#..", ".#.#..", "..#...",
    "..##..", ".####.", "######", "#..#..", "...#.#", ".#.#..", "..#...",
    "..##..", ".####.", "######", "...#.#", "#..#..", ".#.#.#", "..#...",
};
const Sprite WEATHER_UMBRELLA = {"weather.umbrella", 6, 7, 3, 300, WEATHER_UMBRELLA_ROWS};

// The world, 16 x 8 (22.5 degrees a cell), from 180 W and from the pole.
static const char *const WORLD_MAP_ROWS[] = {
    "....#.#.....#...",  // 79 N
    ".####...#######.",  // 56 N
    "..###..#######..",  // 34 N
    "....#..###.##...",  // 11 N
    "....##..##...##.",  // 11 S
    "....##..#....##.",  // 34 S
    ".....#..........",  // 56 S
    "################",  // Antarctica
};
const Sprite WORLD_MAP = {"world.map", 16, 8, 1, 0, WORLD_MAP_ROWS};

}  // namespace spr

namespace sprites {

const Sprite *const ATLAS[] = {
    &spr::NOTIFY_BELL,
    &spr::NOTIFY_MAIL,
    &spr::NOTIFY_CHECK,
    &spr::NOTIFY_ALERT,
    &spr::NOTIFY_HEART,
    &spr::NOTIFY_PHONE,
    &spr::NOTIFY_HOME,
    &spr::NOTIFY_STAR,
    &spr::PET_EGG,
    &spr::PET_FROG,
    &spr::PET_APPLE,
    &spr::PET_NOTE,
    &spr::PET_CROSS,
    &spr::PET_HEART,
    &spr::PET_POOP,
    &spr::PET_ZED,
    &spr::GEEK_INVADER,
    &spr::GEEK_PACMAN,
    &spr::GEEK_GHOST,
    &spr::GEEK_ROCKET,
    &spr::GEEK_FLAME,
    &spr::GEEK_CUP,
    &spr::GEEK_GLIDER,
    &spr::GEEK_SKULL,
    &spr::GEEK_BULB,
    &spr::GEEK_HOUSE,
    &spr::GEEK_BURGER_TOP,
    &spr::GEEK_BURGER_BOTTOM,
    &spr::GEEK_MONITOR,
    &spr::GEEK_BIRD,
    &spr::GEEK_ROBOT,
    &spr::GEEK_DIE,
    &spr::SEASON_PINE,
    &spr::SEASON_HEART,
    &spr::SEASON_PUMPKIN,
    &spr::SEASON_PUMPKIN_FACE,
    &spr::SEASON_CAKE,
    &spr::GALLERY_PENCIL,
    &spr::RUNNER_DINO,
    &spr::RUNNER_DUCK,
    &spr::RUNNER_CACTUS1,
    &spr::RUNNER_CACTUS2,
    &spr::RUNNER_CACTUS3,
    &spr::RUNNER_BIRD,
    &spr::SONIC_RUN,
    &spr::SONIC_BALL,
    &spr::SONIC_BUG,
    &spr::DOOM_IMP,
    &spr::DOOM_IMP_DEAD,
    &spr::DOOM_FIREBALL,
    &spr::DOOM_GUN,
    &spr::MARIO,
    &spr::MARIO_GOOMBA,
    &spr::MARIO_CLOUD,
    &spr::WEATHER_SUN,
    &spr::WEATHER_MOON,
    &spr::WEATHER_PARTLY,
    &spr::WEATHER_CLOUD,
    &spr::WEATHER_FOG,
    &spr::WEATHER_RAIN,
    &spr::WEATHER_SNOW,
    &spr::WEATHER_STORM,
    &spr::WEATHER_UMBRELLA,
    &spr::WORLD_MAP,
};
const uint16_t ATLAS_COUNT = sizeof(ATLAS) / sizeof(ATLAS[0]);

}  // namespace sprites
