#include <ultra64.h>

#include "sm64.h"
#include "platform_displacement.h"
#include "engine/math_util.h"
#include "object_helpers.h"
#include "object_helpers2.h"
#include "mario.h"
#include "engine/behavior_script.h"
#include "level_update.h"
#include "engine/surface_collision.h"
#include "object_list_processor.h"

u16 D_8032FEC0 = 0;

u32 unused_8032FEC4[4] = { 0 };

struct Object *gMarioPlatform[2] = { NULL, NULL };

/**
 * Determine if mario is standing on a platform object, meaning that he is
 * within 4 units of the floor. Set his referenced platform object accordingly.
 */
void update_mario_platform(void) {
    struct Surface *floor;
    UNUSED u32 unused;
    f32 marioX;
    f32 marioY;
    f32 marioZ;
    f32 floorHeight;
    u32 awayFromFloor;
    int i;
    for (i = 0; i < activePlayers; i++) {
        if (gMarioObject != NULL) {
            marioX = gMarioStates[i].marioObj->oPosX;
            marioY = gMarioStates[i].marioObj->oPosY;
            marioZ = gMarioStates[i].marioObj->oPosZ;
            floorHeight = find_floor(marioX, marioY, marioZ, &floor);

            if (absf(marioY - floorHeight) < 4.0f) {
                awayFromFloor = 0;
            } else {
                awayFromFloor = 1;
            }

            switch (awayFromFloor) {
                case 1:
                    gMarioPlatform[i] = NULL;
                    gMarioStates[i].marioObj->platform = NULL;
                    break;

                case 0:
                    if (floor != NULL && floor->object != NULL) {
                        gMarioPlatform[i] = floor->object;
                        gMarioStates[i].marioObj->platform = floor->object;
                    } else {
                        gMarioPlatform[i] = NULL;
                        gMarioStates[i].marioObj->platform = NULL;
                    }
                    break;
            }
        }
    }
}

/**
 * Get mario's position and store it in x, y, and z.
 */
void get_mario_pos(f32 *x, f32 *y, f32 *z, int ID) {
    *x = gMarioStates[ID].pos[0];
    *y = gMarioStates[ID].pos[1];
    *z = gMarioStates[ID].pos[2];
}

/**
 * Set mario's position.
 */
void set_mario_pos(f32 x, f32 y, f32 z, int ID) {
    gMarioStates[ID].pos[0] = x;
    gMarioStates[ID].pos[1] = y;
    gMarioStates[ID].pos[2] = z;
}

/**
 * Apply one frame of platform rotation to mario or an object using the given
 * platform. If isMario is 0, use gCurrentObject.
 */
void apply_platform_displacement(u32 isMario, struct Object *platform) {
    f32 x;
    f32 y;
    f32 z;
    f32 platformPosX;
    f32 platformPosY;
    f32 platformPosZ;
    Vec3f currentObjectOffset;
    Vec3f relativeOffset;
    Vec3f newObjectOffset;
    Vec3s rotation;
    UNUSED s16 unused1;
    UNUSED s16 unused2;
    UNUSED s16 unused3;
    f32 displaceMatrix[4][4];

    rotation[0] = platform->oAngleVelPitch;
    rotation[1] = platform->oAngleVelYaw;
    rotation[2] = platform->oAngleVelRoll;

    if (isMario) {
        D_8032FEC0 = 0;
        get_mario_pos(&x, &y, &z, isMario-1);
    } else {
        x = gCurrentObject->oPosX;
        y = gCurrentObject->oPosY;
        z = gCurrentObject->oPosZ;
    }

    x += platform->oVelX;
    z += platform->oVelZ;

    if (rotation[0] != 0 || rotation[1] != 0 || rotation[2] != 0) {
        unused1 = rotation[0];
        unused2 = rotation[2];
        unused3 = platform->oFaceAngleYaw;

        if (isMario) {
            gMarioStates[isMario-1].faceAngle[1] += rotation[1];
        }

        platformPosX = platform->oPosX;
        platformPosY = platform->oPosY;
        platformPosZ = platform->oPosZ;

        currentObjectOffset[0] = x - platformPosX;
        currentObjectOffset[1] = y - platformPosY;
        currentObjectOffset[2] = z - platformPosZ;

        rotation[0] = platform->oFaceAnglePitch - platform->oAngleVelPitch;
        rotation[1] = platform->oFaceAngleYaw - platform->oAngleVelYaw;
        rotation[2] = platform->oFaceAngleRoll - platform->oAngleVelRoll;

        mtxf_rotate_zxy_and_translate(displaceMatrix, currentObjectOffset, rotation);
        linear_mtxf_transpose_mul_vec3f(displaceMatrix, relativeOffset, currentObjectOffset);

        rotation[0] = platform->oFaceAnglePitch;
        rotation[1] = platform->oFaceAngleYaw;
        rotation[2] = platform->oFaceAngleRoll;

        mtxf_rotate_zxy_and_translate(displaceMatrix, currentObjectOffset, rotation);
        linear_mtxf_mul_vec3f(displaceMatrix, newObjectOffset, relativeOffset);

        x = platformPosX + newObjectOffset[0];
        y = platformPosY + newObjectOffset[1];
        z = platformPosZ + newObjectOffset[2];
    }

    if (isMario) {
        gMarioStates[isMario-1].platformDisplacement[0] = x - gMarioStates[isMario-1].pos[0];
        gMarioStates[isMario-1].platformDisplacement[1] = y - gMarioStates[isMario-1].pos[1];
        gMarioStates[isMario-1].platformDisplacement[2] = z - gMarioStates[isMario-1].pos[2];

        set_mario_pos(x, y, z, isMario-1);

    } else {
        gCurrentObject->oPosX = x;
        gCurrentObject->oPosY = y;
        gCurrentObject->oPosZ = z;
    }
}

/**
 * If mario's platform is not null, apply platform displacement.
 */
// KAZEFIX sets platform displacement to 0 now so it wont be stored from prior platforms
void apply_mario_platform_displacement(void) {
    struct Object *platform;
    int i;
    for (i = 0; i < activePlayers; i++) {
        platform = gMarioPlatform[i];
        if (gMarioStates[i].pos[1] == gMarioStates[i].floorHeight) {
            gMarioStates[i].platformDisplacement[0] = 0.0f;
            gMarioStates[i].platformDisplacement[1] = 0.0f;
            gMarioStates[i].platformDisplacement[2] = 0.0f;
        }
        if (!(gTimeStopState & TIME_STOP_ACTIVE) && gMarioObject != NULL && platform != NULL) {
            apply_platform_displacement(1+i, platform);
        }
    }
}

#ifndef VERSION_JP
/**
 * Set mario's platform to NULL.
 */
void clear_mario_platform(void) {
    gMarioPlatform[0] = NULL;
    gMarioPlatform[1] = NULL;
}
#endif
