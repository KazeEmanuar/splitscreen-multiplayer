#include <ultra64.h>

#include "sm64.h"
#include "behavior_data.h"
#include "mario_actions_automatic.h"
#include "audio/external.h"
#include "area.h"
#include "mario.h"
#include "mario_step.h"
#include "engine/math_util.h"
#include "memory.h"
#include "engine/graph_node.h"
#include "save_file.h"
#include "engine/surface_collision.h"
#include "interaction.h"
#include "camera.h"
#include "object_helpers.h"

#define POLE_NONE 0
#define POLE_TOUCHED_FLOOR 1
#define POLE_FELL_OFF 2

#define HANG_NONE 0
#define HANG_HIT_CEIL_OR_OOB 1
#define HANG_LEFT_CEIL 2

void add_tree_leaf_particles(struct MarioState *m) {
    f32 leafHeight;

    if (m->usedObj->behavior == segmented_to_virtual(bhvTree)) {
        // make leaf effect spawn higher on the Shifting Sand Land palm tree
        if (gCurrLevelNum == LEVEL_SSL) {
            leafHeight = 250.0f;
        } else {
            leafHeight = 100.0f;
        }
        if (m->pos[1] - m->floorHeight > leafHeight) {
            m->particleFlags |= PARTICLE_LEAVES;
        }
    }
}

void play_climbing_sounds(struct MarioState *m, s32 b) {
    s32 isOnTree = (m->usedObj->behavior == segmented_to_virtual(bhvTree));

    if (b == 1) {
        if (is_anim_past_frame(m, 1) != 0) {
            play_sound(isOnTree ? SOUND_ACTION_CLIMB_UP_TREE : SOUND_ACTION_CLIMB_UP_POLE,
                       m->marioObj->soundOrigin);
        }
    } else {
        play_sound(isOnTree ? SOUND_MOVING_SLIDE_DOWN_TREE : SOUND_MOVING_SLIDE_DOWN_POLE,
                   m->marioObj->soundOrigin);
    }
}

s32 set_pole_position(struct MarioState *m, f32 offsetY) {
    UNUSED s32 unused1;
    UNUSED s32 unused2;
    UNUSED s32 unused3;
    struct Surface *floor;
    struct Surface *ceil;
    f32 floorHeight;
    f32 ceilHeight;
    s32 collided;
    s32 result = POLE_NONE;
    f32 poleTop = m->usedObj->hitboxHeight - 100.0f;
    struct Object *marioObj = m->marioObj;

    if (marioObj->oMarioPolePos > poleTop) {
        marioObj->oMarioPolePos = poleTop;
    }

    m->pos[0] = m->usedObj->oPosX;
    m->pos[2] = m->usedObj->oPosZ;
    m->pos[1] = m->usedObj->oPosY + marioObj->oMarioPolePos + offsetY;

    collided = f32_find_wall_collision(&m->pos[0], &m->pos[1], &m->pos[2], 60.0f, 50.0f);
    collided |= f32_find_wall_collision(&m->pos[0], &m->pos[1], &m->pos[2], 30.0f, 24.0f);

    ceilHeight = vec3f_find_ceil(m->pos, m->pos[1], &ceil);
    if (m->pos[1] > ceilHeight - 160.0f) {
        m->pos[1] = ceilHeight - 160.0f;
        marioObj->oMarioPolePos = m->pos[1] - m->usedObj->oPosY;
    }

    floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &floor);
    if (m->pos[1] < floorHeight) {
        m->pos[1] = floorHeight;
        set_mario_action(m, ACT_IDLE, 0);
        result = POLE_TOUCHED_FLOOR;
    } else if (marioObj->oMarioPolePos < -m->usedObj->hitboxDownOffset) {
        m->pos[1] = m->usedObj->oPosY - m->usedObj->hitboxDownOffset;
        set_mario_action(m, ACT_FREEFALL, 0);
        result = POLE_FELL_OFF;
    } else if (collided) {
        if (m->pos[1] > floorHeight + 20.0f) {
            m->forwardVel = -2.0f;
            set_mario_action(m, ACT_SOFT_BONK, 0);
            result = POLE_FELL_OFF;
        } else {
            set_mario_action(m, ACT_IDLE, 0);
            result = POLE_TOUCHED_FLOOR;
        }
    }

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, m->usedObj->oMoveAnglePitch, m->faceAngle[1],
              m->usedObj->oMoveAngleRoll);

    return result;
}

s32 act_holding_pole(struct MarioState *m) {
    struct Object *marioObj = m->marioObj;

#ifdef VERSION_JP
    if (m->input & INPUT_A_PRESSED) {
        add_tree_leaf_particles(m);
        m->faceAngle[1] += 0x8000;
        return set_mario_action(m, ACT_WALL_KICK_AIR, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        add_tree_leaf_particles(m);
        m->forwardVel = -2.0f;
        return set_mario_action(m, ACT_SOFT_BONK, 0);
    }
#else
    if ((m->input & INPUT_Z_PRESSED) || m->health < 0x100) {
        add_tree_leaf_particles(m);
        m->forwardVel = -2.0f;
        return set_mario_action(m, ACT_SOFT_BONK, 0);
    }

    if (m->input & INPUT_A_PRESSED) {
        add_tree_leaf_particles(m);
        m->faceAngle[1] += 0x8000;
        return set_mario_action(m, ACT_WALL_KICK_AIR, 0);
    }
#endif

    if (m->controller->stickY > 16.0f) {
        f32 poleTop = m->usedObj->hitboxHeight - 100.0f;
        void *poleBehavior = virtual_to_segmented(0x13, m->usedObj->behavior);

        if (marioObj->oMarioPolePos < poleTop - 0.4f) {
            return set_mario_action(m, ACT_CLIMBING_POLE, 0);
        }

        if (poleBehavior != bhvGiantPole && m->controller->stickY > 50.0f) {
            return set_mario_action(m, ACT_TOP_OF_POLE_TRANSITION, 0);
        }
    }

    if (m->controller->stickY < -16.0f) {
        marioObj->oMarioPoleYawVel -= m->controller->stickY * 2;
        if (marioObj->oMarioPoleYawVel > 0x1000) {
            marioObj->oMarioPoleYawVel = 0x1000;
        }

        m->faceAngle[1] += marioObj->oMarioPoleYawVel;
        marioObj->oMarioPolePos -= marioObj->oMarioPoleYawVel / 0x100;

        if (m->usedObj->behavior == segmented_to_virtual(bhvTree)) {
            //! The Shifting Sand Land palm tree check is done climbing up in
            // add_tree_leaf_particles, but not here, when climbing down.
            if (m->pos[1] - m->floorHeight > 100.0f) {
                m->particleFlags |= PARTICLE_LEAVES;
            }
        }
        play_climbing_sounds(m, 2);
        func_80320A4C(1, marioObj->oMarioPoleYawVel / 0x100 * 2);
    } else {
        marioObj->oMarioPoleYawVel = 0;
        m->faceAngle[1] -= m->controller->stickX * 16.0f;
    }

    if (set_pole_position(m, 0.0f) == POLE_NONE) {
        set_mario_animation(m, MARIO_ANIM_IDLE_ON_POLE);
    }

    return FALSE;
}

s32 act_climbing_pole(struct MarioState *m) {
    s32 sp24;
    struct Object *marioObj = m->marioObj;
    s16 cameraAngle = m->thisPlayerCamera->trueYaw;

#ifndef VERSION_JP
    if (m->health < 0x100) {
        add_tree_leaf_particles(m);
        m->forwardVel = -2.0f;
        return set_mario_action(m, ACT_SOFT_BONK, 0);
    }
#endif

    if (m->input & INPUT_A_PRESSED) {
        add_tree_leaf_particles(m);
        m->faceAngle[1] += 0x8000;
        return set_mario_action(m, ACT_WALL_KICK_AIR, 0);
    }

    if (m->controller->stickY < 8.0f) {
        return set_mario_action(m, ACT_HOLDING_POLE, 0);
    }

    marioObj->oMarioPolePos += m->controller->stickY / 8.0f;
    marioObj->oMarioPoleYawVel = 0;
    m->faceAngle[1] = cameraAngle - approach_s32((s16)(cameraAngle - m->faceAngle[1]), 0, 0x400, 0x400);

    if (set_pole_position(m, 0.0f) == POLE_NONE) {
        sp24 = m->controller->stickY / 4.0f * 0x10000;
        set_mario_anim_with_accel(m, MARIO_ANIM_CLIMB_UP_POLE, sp24);
        add_tree_leaf_particles(m);
        play_climbing_sounds(m, 1);
    }

    return FALSE;
}

s32 act_grab_pole_slow(struct MarioState *m) {
    play_sound_if_no_flag(m, SOUND_MARIO_WHOA, MARIO_MARIO_SOUND_PLAYED);

    if (set_pole_position(m, 0.0f) == POLE_NONE) {
        set_mario_animation(m, MARIO_ANIM_GRAB_POLE_SHORT);
        if (is_anim_at_end(m)) {
            set_mario_action(m, ACT_HOLDING_POLE, 0);
        }
        add_tree_leaf_particles(m);
    }

    return FALSE;
}

s32 act_grab_pole_fast(struct MarioState *m) {
    struct Object *marioObj = m->marioObj;

    play_sound_if_no_flag(m, SOUND_MARIO_WHOA, MARIO_MARIO_SOUND_PLAYED);
    m->faceAngle[1] += marioObj->oMarioPoleYawVel;
    marioObj->oMarioPoleYawVel = marioObj->oMarioPoleYawVel * 8 / 10;

    if (set_pole_position(m, 0.0f) == POLE_NONE) {
        if (marioObj->oMarioPoleYawVel > 0x800) {
            set_mario_animation(m, MARIO_ANIM_GRAB_POLE_SWING_PART1);
        } else {
            set_mario_animation(m, MARIO_ANIM_GRAB_POLE_SWING_PART2);
            if (is_anim_at_end(m) != 0) {
                marioObj->oMarioPoleYawVel = 0;
                set_mario_action(m, ACT_HOLDING_POLE, 0);
            }
        }
        add_tree_leaf_particles(m);
    }

    return FALSE;
}

s32 act_top_of_pole_transition(struct MarioState *m) {
    struct Object *marioObj = m->marioObj;

    marioObj->oMarioPoleYawVel = 0;
    if (m->actionArg == 0) {
        set_mario_animation(m, MARIO_ANIM_START_HANDSTAND);
        if (is_anim_at_end(m)) {
            return set_mario_action(m, ACT_TOP_OF_POLE, 0);
        }
    } else {
        set_mario_animation(m, MARIO_ANIM_RETURN_FROM_HANDSTAND);
        if (m->marioObj->header.gfx.unk38.animFrame == 0) {
            return set_mario_action(m, ACT_HOLDING_POLE, 0);
        }
    }

    set_pole_position(m, return_mario_anim_y_translation(m));
    return FALSE;
}

s32 act_top_of_pole(struct MarioState *m) {
    UNUSED struct Object *marioObj = m->marioObj;

    if (m->input & INPUT_A_PRESSED) {
        return set_mario_action(m, ACT_TOP_OF_POLE_JUMP, 0);
    }
    if (m->controller->stickY < -16.0f) {
        return set_mario_action(m, ACT_TOP_OF_POLE_TRANSITION, 1);
    }

    m->faceAngle[1] -= m->controller->stickX * 16.0f;

    set_mario_animation(m, MARIO_ANIM_HANDSTAND_IDLE);
    set_pole_position(m, return_mario_anim_y_translation(m));
    return FALSE;
}

s32 perform_hanging_step(struct MarioState *m, Vec3f nextPos) {
    UNUSED s32 unused;
    struct Surface *ceil;
    struct Surface *floor;
    f32 ceilHeight;
    f32 floorHeight;
    f32 ceilOffset;

    m->wall = resolve_and_return_wall_collisions(nextPos, 50.0f, 50.0f);
    floorHeight = find_floor(nextPos[0], nextPos[1], nextPos[2], &floor);
    ceilHeight = vec3f_find_ceil(nextPos, floorHeight, &ceil);

    if (floor == NULL) {
        return HANG_HIT_CEIL_OR_OOB;
    }
    if (ceil == NULL) {
        return HANG_LEFT_CEIL;
    }
    if (ceilHeight - floorHeight <= 160.0f) {
        return HANG_HIT_CEIL_OR_OOB;
    }
    if (ceil->type != SURFACE_HANGABLE) {
        return HANG_LEFT_CEIL;
    }

    ceilOffset = ceilHeight - (nextPos[1] + 160.0f);
    if (ceilOffset < -30.0f) {
        return HANG_HIT_CEIL_OR_OOB;
    }
    if (ceilOffset > 30.0f) {
        return HANG_LEFT_CEIL;
    }

    nextPos[1] = m->ceilHeight - 160.0f;
    vec3f_copy(m->pos, nextPos);

    m->floor = floor;
    m->floorHeight = floorHeight;
    m->ceil = ceil;
    m->ceilHeight = ceilHeight;

    return HANG_NONE;
}

s32 update_hang_moving(struct MarioState *m) {
    s32 stepResult;
    Vec3f nextPos;
    f32 maxSpeed = 4.0f;

    m->forwardVel += 1.0f;
    if (m->forwardVel > maxSpeed) {
        m->forwardVel = maxSpeed;
    }

    m->faceAngle[1] =
        m->intendedYaw - approach_s32((s16)(m->intendedYaw - m->faceAngle[1]), 0, 0x800, 0x800);

    m->slideYaw = m->faceAngle[1];
    m->slideVelX = m->forwardVel * sins(m->faceAngle[1]);
    m->slideVelZ = m->forwardVel * coss(m->faceAngle[1]);

    m->vel[0] = m->slideVelX;
    m->vel[1] = 0.0f;
    m->vel[2] = m->slideVelZ;

    nextPos[0] = m->pos[0] - m->ceil->normal.y * m->vel[0];
    nextPos[2] = m->pos[2] - m->ceil->normal.y * m->vel[2];
    nextPos[1] = m->pos[1];

    stepResult = perform_hanging_step(m, nextPos);

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);
    return stepResult;
}

void update_hang_stationary(struct MarioState *m) {
    m->forwardVel = 0.0f;
    m->slideVelX = 0.0f;
    m->slideVelZ = 0.0f;

    m->pos[1] = m->ceilHeight - 160.0f;
    vec3f_copy(m->vel, gVec3fZero);
    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
}

s32 act_start_hanging(struct MarioState *m) {
    m->actionTimer++;

    if ((m->input & INPUT_NONZERO_ANALOG) && m->actionTimer >= 31) {
        return set_mario_action(m, ACT_HANGING, 0);
    }

    if (!(m->input & INPUT_A_DOWN)) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        return set_mario_action(m, ACT_GROUND_POUND, 0);
    }

    //! Crash if mario's referenced ceiling is NULL (same for other hanging actions)
    if (m->ceil->type != SURFACE_HANGABLE) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    set_mario_animation(m, MARIO_ANIM_HANG_ON_CEILING);
    play_sound_if_no_flag(m, SOUND_ACTION_HANGING_STEP, MARIO_ACTION_SOUND_PLAYED);
    update_hang_stationary(m);

    if (is_anim_at_end(m)) {
        set_mario_action(m, ACT_HANGING, 0);
    }

    return FALSE;
}

s32 act_hanging(struct MarioState *m) {
    if (m->input & INPUT_NONZERO_ANALOG) {
        return set_mario_action(m, ACT_HANG_MOVING, m->actionArg);
    }

    if (!(m->input & INPUT_A_DOWN)) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        return set_mario_action(m, ACT_GROUND_POUND, 0);
    }

    if (m->ceil->type != SURFACE_HANGABLE) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->actionArg & 1) {
        set_mario_animation(m, MARIO_ANIM_HANDSTAND_LEFT);
    } else {
        set_mario_animation(m, MARIO_ANIM_HANDSTAND_RIGHT);
    }

    update_hang_stationary(m);

    return FALSE;
}

s32 act_hang_moving(struct MarioState *m) {
    if (!(m->input & INPUT_A_DOWN)) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        return set_mario_action(m, ACT_GROUND_POUND, 0);
    }

    if (m->ceil->type != SURFACE_HANGABLE) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->actionArg & 1) {
        set_mario_animation(m, MARIO_ANIM_MOVE_ON_WIRE_NET_RIGHT);
    } else {
        set_mario_animation(m, MARIO_ANIM_MOVE_ON_WIRE_NET_LEFT);
    }

    if (m->marioObj->header.gfx.unk38.animFrame == 12) {
        play_sound(SOUND_ACTION_HANGING_STEP, m->marioObj->soundOrigin);
    }

    if (is_anim_past_end(m)) {
        m->actionArg ^= 1;
        if (m->input & INPUT_UNKNOWN_5) {
            return set_mario_action(m, ACT_HANGING, m->actionArg);
        }
    }

    if (update_hang_moving(m) == HANG_LEFT_CEIL) {
        set_mario_action(m, ACT_FREEFALL, 0);
    }

    return FALSE;
}

s32 let_go_of_ledge(struct MarioState *m) {
    f32 floorHeight;
    struct Surface *floor;

    m->vel[1] = 0.0f;
    m->forwardVel = -8.0f;
    m->pos[0] -= 60.0f * sins(m->faceAngle[1]);
    m->pos[2] -= 60.0f * coss(m->faceAngle[1]);

    floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &floor);
    if (floorHeight < m->pos[1] - 100.0f) {
        m->pos[1] -= 100.0f;
    } else {
        m->pos[1] = floorHeight;
    }

    return set_mario_action(m, ACT_SOFT_BONK, 0);
}

void func_8025F0DC(struct MarioState *m) {
    set_mario_animation(m, MARIO_ANIM_IDLE_HEAD_LEFT);
    m->pos[0] += 14.0f * sins(m->faceAngle[1]);
    m->pos[2] += 14.0f * coss(m->faceAngle[1]);
    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
}

void func_8025F188(struct MarioState *m) {
    f32 sp4;

    if (m->actionTimer < 14) {
        sp4 = m->actionTimer;
    } else {
        sp4 = 14.0f;
    }
    m->statusForCamera->pos[0] = m->pos[0] + sp4 * sins(m->faceAngle[1]);
    m->statusForCamera->pos[2] = m->pos[2] + sp4 * coss(m->faceAngle[1]);
    m->statusForCamera->pos[1] = m->pos[1];
    m->actionTimer++;
    m->flags |= MARIO_UNKNOWN_25;
}

void update_ledge_climb(struct MarioState *m, s32 animation, u32 endAction) {
    stop_and_set_height_to_floor(m);

    set_mario_animation(m, animation);
    if (is_anim_at_end(m)) {
        set_mario_action(m, endAction, 0);
        if (endAction == ACT_IDLE) {
            func_8025F0DC(m);
        }
    }
}

s32 act_ledge_grab(struct MarioState *m) {
    f32 heightAboveFloor;
    s16 intendedDYaw = m->intendedYaw - m->faceAngle[1];
    s32 hasSpaceForMario = (m->ceilHeight - m->floorHeight >= 160.0f);

    if (m->actionTimer < 10) {
        m->actionTimer++;
    }

    if (m->floor->normal.y < 0.9063078f) {
        return let_go_of_ledge(m); // KAZEFIX removing this  fixes letting go of ledges automatically
    }

    if (m->input & (INPUT_Z_PRESSED | INPUT_OFF_FLOOR)) {
        return let_go_of_ledge(m);
    }

    if ((m->input & INPUT_A_PRESSED) && hasSpaceForMario) {
        return set_mario_action(m, ACT_LEDGE_CLIMB_FAST, 0);
    }

    if (m->input & INPUT_UNKNOWN_10) {
        if (m->marioObj->oInteractStatus & INT_STATUS_MARIO_UNK1) {
            m->hurtCounter += (m->flags & MARIO_CAP_ON_HEAD) ? 12 : 18;
        }
        return let_go_of_ledge(m);
    }
#ifdef VERSION_EU
    // On PAL, you can't slow climb up ledges while holding A.
    if (m->actionTimer == 10 && (m->input & INPUT_NONZERO_ANALOG) && !(m->input & INPUT_A_DOWN))
#else
    if (m->actionTimer == 10 && (m->input & INPUT_NONZERO_ANALOG))
#endif
    {
        if (intendedDYaw >= -0x4000 && intendedDYaw <= 0x4000) {
            if (hasSpaceForMario) {
                return set_mario_action(m, ACT_LEDGE_CLIMB_SLOW_1, 0);
            }
        } else {
            return let_go_of_ledge(m);
        }
    }

    heightAboveFloor = m->pos[1] - find_floor_height_relative_polar(m, -0x8000, 30.0f);
    if (hasSpaceForMario && heightAboveFloor < 100.0f) {
        return set_mario_action(m, ACT_LEDGE_CLIMB_FAST, 0);
    }

    if (m->actionArg == 0) {
        play_sound_if_no_flag(m, SOUND_MARIO_WHOA, MARIO_MARIO_SOUND_PLAYED);
    }

    stop_and_set_height_to_floor(m);
    set_mario_animation(m, MARIO_ANIM_IDLE_ON_LEDGE);

    return FALSE;
}

s32 act_ledge_climb_slow(struct MarioState *m) {
    if (m->input & INPUT_OFF_FLOOR) {
        return let_go_of_ledge(m);
    }

    if (m->actionTimer >= 28
        && (m->input
            & (INPUT_NONZERO_ANALOG | INPUT_A_PRESSED | INPUT_OFF_FLOOR | INPUT_ABOVE_SLIDE))) {
        func_8025F0DC(m);
        return check_common_action_exits(m);
    }

    if (m->actionTimer == 10) {
        play_sound_if_no_flag(m, SOUND_MARIO_EEUH, MARIO_MARIO_SOUND_PLAYED);
    }

    update_ledge_climb(m, MARIO_ANIM_SLOW_LEDGE_GRAB, ACT_IDLE);

    func_8025F188(m);
    if (m->marioObj->header.gfx.unk38.animFrame == 17) {
        m->action = ACT_LEDGE_CLIMB_SLOW_2;
    }

    return FALSE;
}

s32 act_ledge_climb_down(struct MarioState *m) {
    if (m->input & INPUT_OFF_FLOOR) {
        return let_go_of_ledge(m);
    }

    play_sound_if_no_flag(m, SOUND_MARIO_WHOA, MARIO_MARIO_SOUND_PLAYED);

    update_ledge_climb(m, MARIO_ANIM_CLIMB_DOWN_LEDGE, ACT_LEDGE_GRAB);
    m->actionArg = 1;

    return FALSE;
}

s32 act_ledge_climb_fast(struct MarioState *m) {
    if (m->input & INPUT_OFF_FLOOR) {
        return let_go_of_ledge(m);
    }

    play_sound_if_no_flag(m, SOUND_MARIO_UH2, MARIO_MARIO_SOUND_PLAYED);

    update_ledge_climb(m, MARIO_ANIM_FAST_LEDGE_GRAB, ACT_IDLE);

    if (m->marioObj->header.gfx.unk38.animFrame == 8) {
        play_mario_landing_sound(m, SOUND_ACTION_TERRAIN_LANDING);
    }
    func_8025F188(m);

    return FALSE;
}

s32 act_grabbed(struct MarioState *m) {
    if (m->marioObj->oInteractStatus & INT_STATUS_MARIO_UNK2) {
        s32 thrown = (m->marioObj->oInteractStatus & INT_STATUS_MARIO_UNK6) == 0;

        m->faceAngle[1] = m->usedObj->oMoveAngleYaw;
        vec3f_copy(m->pos, m->marioObj->header.gfx.pos);

        return set_mario_action(m, (m->forwardVel >= 0.0f) ? ACT_THROWN_FORWARD : ACT_THROWN_BACKWARD,
                                thrown);
    }

    set_mario_animation(m, MARIO_ANIM_BEING_GRABBED);
    return FALSE;
}

s32 act_in_cannon(struct MarioState *m) {
    struct Object *marioObj = m->marioObj;
    s16 startFacePitch = m->faceAngle[0];
    s16 startFaceYaw = m->faceAngle[1];

    switch (m->actionState) {
        case 0:
            m->marioObj->header.gfx.node.flags &= ~0x0001;
            m->usedObj->oInteractStatus = INT_STATUS_INTERACTED;

            m->statusForCamera->unk1C[1] = 1;
            m->statusForCamera->usedObj = m->usedObj;

            vec3f_set(m->vel, 0.0f, 0.0f, 0.0f);

            m->pos[0] = m->usedObj->oPosX;
            m->pos[1] = m->usedObj->oPosY + 350.0f;
            m->pos[2] = m->usedObj->oPosZ;

            m->forwardVel = 0.0f;

            m->actionState = 1;
            break;

        case 1:
            if (m->usedObj->oAction == 1) {
                m->faceAngle[0] = m->usedObj->oMoveAnglePitch;
                m->faceAngle[1] = m->usedObj->oMoveAngleYaw;

                marioObj->oMarioCannonObjectYaw = m->usedObj->oMoveAngleYaw;
                marioObj->oMarioCannonInputYaw = 0;

                m->actionState = 2;
            }
            break;

        case 2:
            m->faceAngle[0] -= (s16)(m->controller->stickY * 10.0f);
            marioObj->oMarioCannonInputYaw -= (s16)(m->controller->stickX * 10.0f);

            if (m->faceAngle[0] > 0x38E3) {
                m->faceAngle[0] = 0x38E3;
            }
            if (m->faceAngle[0] < 0) {
                m->faceAngle[0] = 0;
            }

            if (marioObj->oMarioCannonInputYaw > 0x4000) {
                marioObj->oMarioCannonInputYaw = 0x4000;
            }
            if (marioObj->oMarioCannonInputYaw < -0x4000) {
                marioObj->oMarioCannonInputYaw = -0x4000;
            }

            m->faceAngle[1] = marioObj->oMarioCannonObjectYaw + marioObj->oMarioCannonInputYaw;
            if (m->input & INPUT_A_PRESSED) {
                m->forwardVel = 100.0f * coss(m->faceAngle[0]);

                m->vel[1] = 100.0f * sins(m->faceAngle[0]);

                m->pos[0] += 120.0f * coss(m->faceAngle[0]) * sins(m->faceAngle[1]);
                m->pos[1] += 120.0f * sins(m->faceAngle[0]);
                m->pos[2] += 120.0f * coss(m->faceAngle[0]) * coss(m->faceAngle[1]);

                play_sound(SOUND_ACTION_UNKNOWN456, m->marioObj->soundOrigin);
                play_sound(SOUND_OBJ_POUNDING_CANNON, m->marioObj->soundOrigin);

                m->marioObj->header.gfx.node.flags |= 0x0001;

                set_mario_action(m, ACT_SHOT_FROM_CANNON, 0);
                m->usedObj->oAction = 2;
                return FALSE;
            } else {
                if (m->faceAngle[0] != startFacePitch || m->faceAngle[1] != startFaceYaw) {
                    play_sound(SOUND_MOVING_AIM_CANNON, m->marioObj->soundOrigin);
                }
            }
    }

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);
    set_mario_animation(m, MARIO_ANIM_DIVE);

    return FALSE;
}

s32 act_bubbled(struct MarioState *m) {
    struct Object *marioObj = m->marioObj;
    f32 mag;
    f32 mag2;
    struct Surface *ceil;
    struct Surface *floor;
    f32 ceilHeight;
    f32 floorHeight;
    f32 ceilOffset;
    int i;
    m->thisPlayerCamera->cutscene = 0;


    if (m->bubble == NULL) {
        m->bubble = spawn_object(m->marioObj, MODEL_BUBBLE, bhvBubbleJail);
        m->vel[1] = 0;
    }
    set_mario_animation(m, MARIO_ANIM_SLEEP_IDLE);
    mag = m->controller->stickMag / 25.f;
    m->vel[0] += mag * sins(m->intendedYaw);
    m->vel[2] += mag * coss(m->intendedYaw);
    if (m->controller->buttonDown & A_BUTTON) {
        m->vel[1] += 2.f;
    }
    if (m->controller->buttonDown & B_BUTTON) {
        m->vel[1] -= 2.f;
    }
    m->pos[0] += m->vel[0];
    m->pos[1] += m->vel[1];
    m->pos[2] += m->vel[2];
    mag2 = sqrtf(m->vel[0] * m->vel[0] + m->vel[2] * m->vel[2]) * 10;
    m->angleVel[1] = approach_s16_symmetric(m->angleVel[1], atan2s(m->vel[2], m->vel[0]), (int) mag2);
    m->thisPlayerCamera->pos[1] = (m->pos[1] + 200.f);
    for (i = 0; i < 3; i++) {
        if (m->vel[i] > 40.f) {
            m->vel[i] = 40.f;
        }
        if (m->vel[i] < -40.f) {
            m->vel[i] = -40.f;
        }
    }

    m->vel[0] = approach_f32(m->vel[0], 0, 1, 1);
    m->vel[1] = approach_f32(m->vel[1], 0, 1, 1);
    m->vel[2] = approach_f32(m->vel[2], 0, 1, 1);

    m->wall = resolve_and_return_wall_collisions(m->pos, 10.0f, 120.0f);
    floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &floor) + 20.f;
    if (m->pos[1] < floorHeight) {
        m->pos[1] = floorHeight;
        m->vel[1] = 0;
    }
    ceilHeight = vec3f_find_ceil(m->pos, floorHeight, &ceil) - 200.f;
    if (m->pos[1] > (ceilHeight)) {
        m->pos[1] = ceilHeight;
        m->vel[1] = 0;
    }

    vec3f_copy(&m->marioObj->oPosX, m->pos);
    vec3f_copy(&m->marioObj->header.gfx.pos, &m->marioObj->oPosX);
    m->marioObj->header.gfx.angle[1] = m->angleVel[1];
    m->marioObj->header.gfx.angle[0] = 0;
    m->marioObj->header.gfx.angle[2] = 0;
    m->marioObj->oFaceAnglePitch = 0;
    m->marioObj->oFaceAngleYaw = m->angleVel[1];
    m->marioObj->oMoveAngleYaw = m->angleVel[1];
    m->marioObj->oFaceAngleRoll = 0;
    vec3f_copy(&m->bubble->oPosX, &m->marioObj->oPosX);
    m->marioObj->header.gfx.pos[0] += 35.f * sins(m->marioObj->header.gfx.angle[1]);
    m->marioObj->header.gfx.pos[2] += 35.f * coss(m->marioObj->header.gfx.angle[1]);
    m->flags |= MARIO_VANISH_CAP;
    if (m->numLives < 0) {
        m->marioObj->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
    }
    // undo action state if touched other mario
    if ((dist_between_objects(obj_nearest_object_with_behavior(segmented_to_virtual(bhvMario)),
                              m->marioObj)
         < 200.f)
        && (m->numLives != -1)) {
        set_mario_action(m, ACT_FREEFALL, 0);
        play_sound(SOUND_OBJ_DEFAULT_DEATH, gDefaultSoundArgs);
        m->bubble->activeFlags = 0;
        m->bubble = NULL;
        m->invincTimer = 0x20;
        m->flags &= ~MARIO_VANISH_CAP;
    }

    return FALSE;
}

s32 act_tornado_twirling(struct MarioState *m) {
    struct Surface *floor;
    Vec3f nextPos;
    f32 sinAngleVel;
    f32 cosAngleVel;
    f32 floorHeight;
    struct Object *marioObj = m->marioObj;
    struct Object *usedObj = m->usedObj;
    s16 prevTwirlYaw = m->twirlYaw;

    f32 dx = (m->pos[0] - usedObj->oPosX) * 0.95f;
    f32 dz = (m->pos[2] - usedObj->oPosZ) * 0.95f;

    if (m->vel[1] < 60.0f) {
        m->vel[1] += 1.0f;
    }

    if ((marioObj->oMarioTornadoPosY += m->vel[1]) < 0.0f) {
        marioObj->oMarioTornadoPosY = 0.0f;
    }
    if (marioObj->oMarioTornadoPosY > usedObj->hitboxHeight) {
        if (m->vel[1] < 20.0f) {
            m->vel[1] = 20.0f;
        }
        return set_mario_action(m, ACT_TWIRLING, 1);
    }

    if (m->angleVel[1] < 0x3000) {
        m->angleVel[1] += 0x100;
    }

    if (marioObj->oMarioTornadoYawVel < 0x1000) {
        marioObj->oMarioTornadoYawVel += 0x100;
    }

    m->twirlYaw += m->angleVel[1];

    sinAngleVel = sins(marioObj->oMarioTornadoYawVel);
    cosAngleVel = coss(marioObj->oMarioTornadoYawVel);

    nextPos[0] = usedObj->oPosX + dx * cosAngleVel + dz * sinAngleVel;
    nextPos[2] = usedObj->oPosZ - dx * sinAngleVel + dz * cosAngleVel;
    nextPos[1] = usedObj->oPosY + marioObj->oMarioTornadoPosY;

    f32_find_wall_collision(&nextPos[0], &nextPos[1], &nextPos[2], 60.0f, 50.0f);

    floorHeight = find_floor(nextPos[0], nextPos[1], nextPos[2], &floor);
    if (floor != NULL) {
        m->floor = floor;
        m->floorHeight = floorHeight;
        vec3f_copy(m->pos, nextPos);
    } else {
        if (nextPos[1] >= m->floorHeight) {
            m->pos[1] = nextPos[1];
        } else {
            m->pos[1] = m->floorHeight;
        }
    }

    m->actionTimer++;

    set_mario_animation(m, (m->actionArg == 0) ? MARIO_ANIM_START_TWIRL : MARIO_ANIM_TWIRL);

    if (is_anim_past_end(m)) {
        m->actionArg = 1;
    }

    // Play sound on angle overflow
    if (prevTwirlYaw > m->twirlYaw) {
        play_sound(SOUND_ACTION_SWISH2_2, m->marioObj->soundOrigin);
    }

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1] + m->twirlYaw, 0);

    return FALSE;
}

s32 check_common_automatic_cancels(struct MarioState *m) {
    if (m->pos[1] < m->waterLevel - 100) {
        return set_water_plunge_action(m);
    }

    return FALSE;
}

s32 mario_execute_automatic_action(struct MarioState *m) {
    s32 cancel;

    if (check_common_automatic_cancels(m)) {
        return TRUE;
    }

    m->quicksandDepth = 0.0f;

    /* clang-format off */
    switch (m->action) {
        case ACT_HOLDING_POLE:           cancel = act_holding_pole(m);           break;
        case ACT_GRAB_POLE_SLOW:         cancel = act_grab_pole_slow(m);         break;
        case ACT_GRAB_POLE_FAST:         cancel = act_grab_pole_fast(m);         break;
        case ACT_CLIMBING_POLE:          cancel = act_climbing_pole(m);          break;
        case ACT_TOP_OF_POLE_TRANSITION: cancel = act_top_of_pole_transition(m); break;
        case ACT_TOP_OF_POLE:            cancel = act_top_of_pole(m);            break;
        case ACT_START_HANGING:          cancel = act_start_hanging(m);          break;
        case ACT_HANGING:                cancel = act_hanging(m);                break;
        case ACT_HANG_MOVING:            cancel = act_hang_moving(m);            break;
        case ACT_LEDGE_GRAB:             cancel = act_ledge_grab(m);             break;
        case ACT_LEDGE_CLIMB_SLOW_1:     cancel = act_ledge_climb_slow(m);       break;
        case ACT_LEDGE_CLIMB_SLOW_2:     cancel = act_ledge_climb_slow(m);       break;
        case ACT_LEDGE_CLIMB_DOWN:       cancel = act_ledge_climb_down(m);       break;
        case ACT_LEDGE_CLIMB_FAST:       cancel = act_ledge_climb_fast(m);       break;
        case ACT_GRABBED:                cancel = act_grabbed(m);                break;
        case ACT_IN_CANNON:              cancel = act_in_cannon(m);              break;
        case ACT_TORNADO_TWIRLING:       cancel = act_tornado_twirling(m);       break;
        case ACT_BUBBLED:                cancel = act_bubbled(m);              break;
    }
    /* clang-format on */

    return cancel;
}