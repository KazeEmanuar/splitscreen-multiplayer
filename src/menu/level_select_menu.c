#include <ultra64.h>

#include "sm64.h"
#include "audio/external.h"
#include "game/game.h"
#include "game/main.h"
#include "game/memory.h"
#include "game/area.h"
#include "game/save_file.h"
#include "game/level_update.h"
#include "game/sound_init.h"
#include "game/print.h"
#include "game/display.h"
#include "seq_ids.h"
#include "engine/math_util.h"

#define PRESS_START_DEMO_TIMER 800

static char gLevelSelect_StageNamesText[64][16] = {
    "",
    "",
    "",
    "TERESA OBAKE",
    "YYAMA1 % YSLD1",
    "SELECT ROOM",
    "HORROR DUNGEON",
    "SABAKU % PYRMD",
    "BATTLE FIELD",
    "YUKIYAMA2",
    "POOL KAI",
    "WTDG % TINBOTU",
    "BIG WORLD",
    "CLOCK TOWER",
    "RAINBOW CRUISE",
    "MAIN MAP",
    "EXT1 YOKO SCRL",
    "EXT7 HORI MINI",
    "EXT2 TIKA LAVA",
    "EXT9 SUISOU",
    "EXT3 HEAVEN",
    "FIREB1 % INVLC",
    "WATER LAND",
    "MOUNTAIN",
    "ENDING",
    "URANIWA",
    "EXT4 MINI SLID",
    "IN THE FALL",
    "EXT6 MARIO FLY",
    "KUPPA1",
    "EXT8 BLUE SKY",
    "",
    "KUPPA2",
    "KUPPA3",
    "",
    "DONKEY % SLID2",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

static u16 gDemoCountdown = 0;
#ifndef VERSION_JP
static s16 D_U_801A7C34 = 1;
static s16 gameOverNotPlayed = 1;
#endif

// run the demo timer on the PRESS START screen.
// this function will return a non-0 timer once
// the demo starts, signaling to the subsystem that
// the demo needs to be ran.

// don't shift this function from being the first function in the segment.
// the level scripts assume this function is the first, so it cant be moved.
int run_press_start_demo_timer(s32 timer) {
    gCurrDemoInput = NULL;

    if (timer == 0) {
        if (!gPlayer1Controller->buttonDown && !gPlayer1Controller->stickMag) {
            if ((++gDemoCountdown) == PRESS_START_DEMO_TIMER) {
            }
        } else { // activity was detected, so reset the demo countdown.
            gDemoCountdown = 0;
        }
    }
    return timer;
}

// input loop for the level select menu. updates the selected stage
// count if an input was received. signals the stage to be started
// or the level select to be exited if start or the quit combo is
// pressed.
//also allow player 2 inputs
s16 level_select_input_loop(void) {
}

int func_8016F3CC(void) {
    s32 sp1C = 0;

#ifndef VERSION_JP
    if (D_U_801A7C34 == 1) {
        if (gGlobalTimer < 0x81) {
            play_sound(SOUND_MARIO_HELLO, gDefaultSoundArgs);
        } else {
            play_sound(SOUND_MARIO_PRESS_START_TO_PLAY, gDefaultSoundArgs);
        }
        D_U_801A7C34 = 0;
    }
#endif
    print_intro_text();

    if ((gPlayer1Controller->buttonPressed | gPlayer2Controller->buttonPressed )& START_BUTTON) {
#ifdef VERSION_JP
        play_sound(SOUND_MENU_STAR_SOUND, gDefaultSoundArgs);
        sp1C = 100 + gDebugLevelSelect;
#else
        play_sound(SOUND_MENU_STAR_SOUND, gDefaultSoundArgs);
        sp1C = 100 + gDebugLevelSelect;
        D_U_801A7C34 = 1;
#endif
    }
    return run_press_start_demo_timer(sp1C);
}

int func_8016F444(void) {
    s32 sp1C = 0;

#ifndef VERSION_JP
    if (gameOverNotPlayed == 1) {
        play_sound(SOUND_MARIO_GAME_OVER, gDefaultSoundArgs);
        gameOverNotPlayed = 0;
    }
#endif

    print_intro_text();

    if ((gPlayer1Controller->buttonPressed | gPlayer2Controller->buttonPressed ) & START_BUTTON) {
        play_sound(SOUND_MENU_STAR_SOUND, gDefaultSoundArgs);
        sp1C = 100 + gDebugLevelSelect;
#ifndef VERSION_JP
        gameOverNotPlayed = 1;
#endif
    }
    return run_press_start_demo_timer(sp1C);
}

int func_8016F4BC(void) {
    set_background_music(0, SEQ_SOUND_PLAYER, 0);
    play_sound(SOUND_MENU_COIN_ITS_A_ME_MARIO, gDefaultSoundArgs);
    return 1;
}

s32 LevelProc_8016F508(s16 arg1, UNUSED s32 arg2) {
    s32 retVar;

    switch (arg1) {
        case 0:
            retVar = func_8016F4BC();
            break;
        case 1:
            retVar = func_8016F3CC();
            break;
        case 2:
            retVar = func_8016F444();
            break;
        case 3:
            retVar = level_select_input_loop();
            break; // useless break needed to match
    }
    return retVar;
}
