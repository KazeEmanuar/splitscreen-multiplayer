#include <ultra64.h>

#include "sm64.h"
#include "display.h"
#include "game.h"
#include "level_update.h"
#include "camera.h"
#include "print.h"
#include "ingame_menu.h"
#include "hud.h"
#include "segment2.h"
#include "area.h"
#include "save_file.h"
#include "print.h"

/* @file hud.c
 * This file implements HUD rendering and power meter animations.
 * That includes stars, lives, coins, camera status, power meter, timer
 * cannon reticle, and the unused keys.
 **/

struct PowerMeterHUD {
    s8 animation;
    s16 x;
    s16 y;
    f32 unused;
};

struct UnusedStruct803314F0 {
    u32 unused1;
    u16 unused2;
    u16 unused3;
};

struct CameraHUD {
    s16 status;
};

// Stores health segmented value defined by numHealthWedges
// When the HUD is rendered this value is 8, full health.
static s16 sPowerMeterStoredHealth[2];

static struct PowerMeterHUD sPowerMeterHUD[2] = { {
                                                      POWER_METER_HIDDEN,
                                                      26,
                                                      100,
                                                      1.0,
                                                  },
                                                  {
                                                      POWER_METER_HIDDEN,
                                                      26,
                                                      100,
                                                      1.0,
                                                  } };

// Power Meter timer that keeps counting when it's visible.
// Gets reset when the health is filled and stops counting
// when the power meter is hidden.
s32 sPowerMeterVisibleTimer[2] = { 0, 0 };

static struct UnusedStruct803314F0 unused803314F0 = { 0x00000000, 0x000A, 0x0000 };

static struct CameraHUD sCameraHUD = { CAM_STATUS_NONE };

/**
 * Renders a rgba16 16x16 glyph texture from a table list.
 */
void render_hud_tex_lut(s32 x, s32 y, u8 *texture) {
    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, texture);
    gSPDisplayList(gDisplayListHead++, &dl_hud_img_load_tex_block);
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 15) << 2, (y + 15) << 2,
                        G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10);
}

/**
 * Renders a rgba16 8x8 glyph texture from a table list.
 */
void render_hud_small_tex_lut(s32 x, s32 y, u8 *texture) {
    gDPSetTile(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0,
               (G_TX_NOMIRROR | G_TX_WRAP), G_TX_NOMASK, G_TX_NOLOD, (G_TX_NOMIRROR | G_TX_WRAP),
               G_TX_NOMASK, G_TX_NOLOD);
    gDPTileSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 2, 0, G_TX_RENDERTILE, 0,
               (G_TX_NOMIRROR | G_TX_CLAMP), 3, G_TX_NOLOD, (G_TX_NOMIRROR | G_TX_CLAMP), 3,
               G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0, 28, 28);
    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, texture);
    gDPLoadSync(gDisplayListHead++);
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 63, 1024);
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 7) << 2, (y + 7) << 2, G_TX_RENDERTILE,
                        0, 0, 4 << 10, 1 << 10);
}

/**
 * Renders power meter health segment texture using a table list.
 */
void render_power_meter_health_segment(s16 numHealthWedges) {
    u8 *(*healthLUT)[];

    healthLUT = segmented_to_virtual(&power_meter_health_segments_lut);

    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
                       (*healthLUT)[numHealthWedges - 1]);
    gDPLoadSync(gDisplayListHead++);
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 1023, 256);
    gSP1Triangle(gDisplayListHead++, 0, 1, 2, 0);
    gSP1Triangle(gDisplayListHead++, 0, 2, 3, 0);
}

/**
 * Renders power meter display lists.
 * That includes the "POWER" base and the colored health segment textures.
 */
void render_dl_power_meter(s16 numHealthWedges, int playerID) {
    Mtx *mtx;

    mtx = alloc_display_list(sizeof(Mtx));

    if (mtx == NULL) {
        return;
    }

    guTranslate(mtx, (f32) sPowerMeterHUD[playerID].x,
                (f32) sPowerMeterHUD[playerID].y / (1 + playerID) - playerID * 19, 0);

    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx++),
              (G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH));
    gSPDisplayList(gDisplayListHead++, &dl_power_meter_base);

    if (numHealthWedges != 0) {
        gSPDisplayList(gDisplayListHead++, &dl_power_meter_health_segments_begin);
        render_power_meter_health_segment(numHealthWedges);
        gSPDisplayList(gDisplayListHead++, &dl_power_meter_health_segments_end);
    }

    gSPPopMatrix(gDisplayListHead++, 0);
}

/**
 * Power meter animation called when there's less than 8 health segments
 * Checks its timer to later change into deemphasizing mode.
 */
void animate_power_meter_emphasized(int playerID) {
    s16 hudDisplayFlags;
    hudDisplayFlags = gHudDisplay.flags;

    if (!(hudDisplayFlags & HUD_DISPLAY_FLAG_EMPHASIZE_POWER)) {
        if (sPowerMeterVisibleTimer[playerID] == 45.0) {
            sPowerMeterHUD[playerID].animation = POWER_METER_DEEMPHASIZING;
        }
    } else {
        sPowerMeterVisibleTimer[playerID] = 0;
    }
}

/**
 * Power meter animation called after emphasized mode.
 * Moves power meter y pos speed until it's at 200 to be visible.
 */
static void animate_power_meter_deemphasizing(int playerID) {
    s16 speed = 5;

    if (sPowerMeterHUD[playerID].y >= 181) {
        speed = 3;
    }

    if (sPowerMeterHUD[playerID].y >= 191) {
        speed = 2;
    }

    if (sPowerMeterHUD[playerID].y >= 196) {
        speed = 1;
    }

    sPowerMeterHUD[playerID].y += speed;

    if (sPowerMeterHUD[playerID].y >= 201) {
        sPowerMeterHUD[playerID].y = 200;
        sPowerMeterHUD[playerID].animation = POWER_METER_VISIBLE;
    }
}

/**
 * Power meter animation called when there's 8 health segments.
 * Moves power meter y pos quickly until it's at 301 to be hidden.
 */
static void animate_power_meter_hiding(int playerID) {
    sPowerMeterHUD[playerID].y += 20;
    if (sPowerMeterHUD[playerID].y >= 301) {
        sPowerMeterHUD[playerID].animation = POWER_METER_HIDDEN;
        sPowerMeterVisibleTimer[playerID] = 0;
    }
}

/**
 * Handles power meter actions depending of the health segments values.
 */
void handle_power_meter_actions(s16 numHealthWedges, int playerID) {
    // Show power meter if health is not full, less than 8
    if (numHealthWedges < 8 && sPowerMeterStoredHealth[playerID] == 8
        && sPowerMeterHUD[playerID].animation == POWER_METER_HIDDEN) {
        sPowerMeterHUD[playerID].animation = POWER_METER_EMPHASIZED;
        sPowerMeterHUD[playerID].y = 166;
    }

    // Show power meter if health is full, has 8
    if (numHealthWedges == 8 && sPowerMeterStoredHealth[playerID] == 7) {
        sPowerMeterVisibleTimer[playerID] = 0;
    }

    // After health is full, hide power meter
    if (numHealthWedges == 8 && sPowerMeterVisibleTimer[playerID] > 45.0) {
        sPowerMeterHUD[playerID].animation = POWER_METER_HIDING;
    }

    // Update to match health value
    sPowerMeterStoredHealth[playerID] = numHealthWedges;

    // If mario is swimming, keep showing power meter
    if (gPlayerStatusForCamera[playerID].action & ACT_FLAG_SWIMMING) {
        if (sPowerMeterHUD[playerID].animation == POWER_METER_HIDDEN
            || sPowerMeterHUD[playerID].animation == POWER_METER_EMPHASIZED) {
            sPowerMeterHUD[playerID].animation = POWER_METER_DEEMPHASIZING;
            sPowerMeterHUD[playerID].y = 166;
        }
        sPowerMeterVisibleTimer[playerID] = 0;
    }
}

/**
 * Renders the power meter that shows when Mario is in underwater
 * or has taken damage and has less than 8 health segments.
 * And calls a power meter animation function depending of the value defined.
 */
void render_hud_power_meter(int playerID) {
    s16 shownHealthWedges = gMarioStates[playerID].health > 0 ? gMarioStates[playerID].health >> 8 : 0;
    ;

    if (sPowerMeterHUD[playerID].animation != POWER_METER_HIDING) {
        handle_power_meter_actions(shownHealthWedges, playerID);
    }

    if (sPowerMeterHUD[playerID].animation == POWER_METER_HIDDEN) {
        return;
    }

    switch (sPowerMeterHUD[playerID].animation) {
        case POWER_METER_EMPHASIZED:
            animate_power_meter_emphasized(playerID);
            break;
        case POWER_METER_DEEMPHASIZING:
            animate_power_meter_deemphasizing(playerID);
            break;
        case POWER_METER_HIDING:
            animate_power_meter_hiding(playerID);
            break;
        default:
            break;
    }

    render_dl_power_meter(shownHealthWedges, playerID);

    sPowerMeterVisibleTimer[playerID] += 1;
}

#ifdef VERSION_JP
#define HUD_TOP_Y 210
#else
#define HUD_TOP_Y 206 + 16 - BORDER_HEIGHT * 2
#endif

/**
 * Renders the amount of lives Mario has.
 */
void render_hud_mario_lives(void) {
    int i;
    int c = HUD_TOP_Y;
    for (i = 0; i < activePlayers; i++) {
        if (i == 0) {
            print_text(4, c / (i + 1) - 8 * i, ","); // 'Mario Head' glyph
        } else {
            print_text(4, c / (i + 1) - 8 * i, "/"); // 'Mario Head' glyph
        }
        print_text(20, c / (i + 1) - 8 * i, "*"); // 'X' glyph
        print_text_fmt_int(36, c / (i + 1) - 8 * i, "%d", gMarioStates[i].numLives);
    }
}

/**
 * Renders the amount of coins collected.
 */
void render_hud_coins(void) {
    int c = HUD_TOP_Y;
    print_text(252, c / 2 - 8, "+"); // 'Mario Head' glyph
    print_text(268, c / 2 - 8, "*"); // 'X' glyph
    print_text_fmt_int(282, c / 2 - 8, "%d", gMarioStates[0].numCoins + gMarioStates[1].numCoins);
}

#ifdef VERSION_JP
#define HUD_STARS_X 247
#else
#define HUD_STARS_X 242
#endif

/**
 * Renders the amount of stars collected.
 * Disables "X" glyph when Mario has 100 stars or more.
 */
void render_hud_stars(void) {
    if (gHudFlash == 1 && gGlobalTimer & 0x08) {
        return;
    }

    print_text(252, HUD_TOP_Y, "-"); // 'Mario Head' glyph
    print_text(268, HUD_TOP_Y, "*"); // 'X' glyph
    print_text_fmt_int(282, HUD_TOP_Y, "%d", gHudDisplay.stars);
}

/**
 * Unused function that renders the amount of keys collected.
 * Leftover function from the beta version of the game.
 */
void render_hud_keys(void) {
    s16 i;

    for (i = 0; i < gHudDisplay.keys; i++) {
        print_text((i * 16) + 220, 142, "/"); // unused glyph - beta key
    }
}

/**
 * Renders the timer when Mario start sliding in PSS.
 */
void render_hud_timer(void) {
    u8 *(*hudLUT)[58];
    u16 timerValFrames;
    u16 timerMins;
    u16 timerSecs;
    u16 timerFracSecs;

    hudLUT = segmented_to_virtual(&main_hud_lut);
    timerValFrames = gHudDisplay.timer;
#ifdef VERSION_EU
    switch (eu_get_language()) {
        case LANGUAGE_ENGLISH:
            print_text(170, 185, "TIME");
            break;
        case LANGUAGE_FRENCH:
            print_text(165, 185, "TEMPS");
            break;
        case LANGUAGE_GERMAN:
            print_text(170, 185, "ZEIT");
            break;
    }
#endif
    timerMins = timerValFrames / (30 * 60);
    timerSecs = (timerValFrames - (timerMins * 1800)) / 30;

    timerFracSecs = ((timerValFrames - (timerMins * 1800) - (timerSecs * 30)) & 0xFFFF) / 3;
#ifndef VERSION_EU
    print_text(170, 185, "TIME");
#endif
    print_text_fmt_int(229, 185, "%0d", timerMins);
    print_text_fmt_int(249, 185, "%02d", timerSecs);
    print_text_fmt_int(283, 185, "%d", timerFracSecs);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    render_hud_tex_lut(239, 32, (*hudLUT)[GLYPH_APOSTROPHE]);
    render_hud_tex_lut(274, 32, (*hudLUT)[GLYPH_DOUBLE_QUOTE]);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
}

/**
 * Sets HUD status camera value depending of the actions
 * defined in update_camera_status.
 */
void set_hud_camera_status(s16 status) {
    sCameraHUD.status = status;
}

/**
 * Renders camera HUD glyphs using a table list, depending of
 * the camera status called, a defined glyph is rendered.
 */
void render_hud_camera_status(void) {
    u8 *(*cameraLUT)[6];
    s32 x;
    s32 y;
    int i;
    // gMarioStates[0].thisPlayerCamera->hudStatus;
    for (i = 0; i < activePlayers; i++) {
        if (gMarioStates[i].thisPlayerCamera == NULL) {
            return;
        }
        cameraLUT = segmented_to_virtual(&main_hud_camera_lut);
        x = 282;
        y = 88 + 8 - BORDER_HEIGHT + 120 * i;

        if (gMarioStates[i].thisPlayerCamera->hudStatus == CAM_STATUS_NONE) {
            return;
        }

        gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
        render_hud_tex_lut(x, y, (*cameraLUT)[GLYPH_CAM_CAMERA]);

        switch (gMarioStates[i].thisPlayerCamera->hudStatus & CAM_STATUS_MODE_GROUP) {
            case CAM_STATUS_MARIO:
                if (i == 0) {
                    render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_MARIO_HEAD]);
                } else {
                    render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_LUIGI_HEAD]);
                }
                break;
            case CAM_STATUS_LAKITU:
                render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_LAKITU_HEAD]);
                break;
            case CAM_STATUS_FIXED:
                render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_FIXED]);
                break;
        }

        switch (gMarioStates[i].thisPlayerCamera->hudStatus & CAM_STATUS_C_MODE_GROUP) {
            case CAM_STATUS_C_DOWN:
                render_hud_small_tex_lut(x + 4, y + 16, (*cameraLUT)[GLYPH_CAM_ARROW_DOWN]);
                break;
            case CAM_STATUS_C_UP:
                render_hud_small_tex_lut(x + 4, y - 8, (*cameraLUT)[GLYPH_CAM_ARROW_UP]);
                break;
        }
    }
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
}

/**
 * Render HUD strings using hudDisplayFlags with it's render functions,
 * excluding the cannon reticle which detects a camera preset for it.
 */
void render_hud(void) {
    s16 hudDisplayFlags;
#ifdef VERSION_EU
    Mtx *mtx;
#endif

    hudDisplayFlags = gHudDisplay.flags;

    if (hudDisplayFlags == HUD_DISPLAY_NONE) {
        sPowerMeterHUD[0].animation = POWER_METER_HIDDEN;
        sPowerMeterHUD[1].animation = POWER_METER_HIDDEN;
        sPowerMeterStoredHealth[0] = 8;
        sPowerMeterStoredHealth[1] = 8;
        sPowerMeterVisibleTimer[0] = 0;
        sPowerMeterVisibleTimer[1] = 0;
    } else {
#ifdef VERSION_EU
        // basically create_dl_ortho_matrix but guOrtho screen width is different
        mtx = alloc_display_list(sizeof(*mtx));
        if (mtx == NULL) {
            return;
        }
        create_dl_identity_matrix();
        guOrtho(mtx, -16.0f, 336.0f, 0, 240.0f, -10.0f, 10.0f, 1.0f);
        gMoveWd(gDisplayListHead++, 0xE, 0, 0xFFFF);
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx), 1);

#else
        create_dl_ortho_matrix();
#endif

        if (gCurrentArea != NULL && gCurrentArea->camera->currPreset == CAMERA_PRESET_INSIDE_CANNON) {
            render_hud_cannon_reticle();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_LIVES) {
            render_hud_mario_lives();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_COIN_COUNT) {
            render_hud_coins();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_STAR_COUNT) {
            render_hud_stars();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_KEYS) {
            render_hud_keys();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_CAMERA_AND_POWER) {
            render_hud_power_meter(0);
            render_hud_power_meter(1);
            render_hud_camera_status();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_TIMER) {
            render_hud_timer();
        }
    }
}
