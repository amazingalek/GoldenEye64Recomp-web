#include "patches.h"

// Keyboard and mouse as the PC ports play it (the README compares them with the Perfect Dark port). The mouse looks in
// both of GoldenEye's aim modes: outside aim mode it turns Bond and tilts his view by an angle a count, as Perfect
// Dark's port does (0.055 degrees a count at the default 50%, finer as the view zooms); in aim mode it moves the
// crosshair across the screen, and the view turns while the crosshair is past 70% of the way to an edge. The keys and
// the wheel's actions beyond the N64's buttons (reload, crouch, next and previous weapon) come from the program
// (src/game/mouse_keyboard.cpp), which also puts the mouse's buttons on the pad (fire, aim).
//
// The game calls one of two functions each tick it reads Bond's controls, with the crosshair's input: sub_GAME_7F067F58
// outside aim mode and sub_GAME_7F067FBC in it. Both are patched here; each passes the game's own arguments on as the
// original does, with the weapon's crosshair and aim speeds, unless the mouse has moved the crosshair.

// The current player's fields this writes, at the offsets the recompiled code reads them at (structs.h's comments
// don't all match its layout).
#define PLAYER_FIELD(type, offset) (*(type*)((u8*)g_CurrentPlayer + (offset)))
#define PLAYER_CROUCHPOS PLAYER_FIELD(s32, 0x9C)
#define PLAYER_LOOKAHEAD PLAYER_FIELD(s32, 0x118)
#define PLAYER_VV_THETA PLAYER_FIELD(f32, 0x148)
#define PLAYER_VV_VERTA PLAYER_FIELD(f32, 0x158)

// An item's statistics: the crosshair's damping and the aim lock's speed.
#define ITEM_CROSSHAIR_SPEED(stats) (*(f32*)((u8*)(stats) + 0x38))
#define ITEM_AIM_LOCK_SPEED(stats) (*(f32*)((u8*)(stats) + 0x3C))

#define GUNRIGHT 0
#define GUNLEFT 1

// recomp_get_pc_actions' bits, one each press.
#define PC_ACTION_RELOAD 0x1
#define PC_ACTION_CROUCH 0x2
#define PC_ACTION_NEXT_WEAPON 0x4
#define PC_ACTION_PREV_WEAPON 0x8

void recomp_take_mouse_look(f32* dx, f32* dy);
u32 recomp_get_pc_actions(void);
s32 recomp_mouse_look_enabled(void);

s32 getCurrentPlayerWeaponId(s32 hand);
void* get_ptr_item_statistics(s32 item);
void caclulate_gun_crosshair_position_rotation(f32 x, f32 y, f32 crosshair_speed, f32 aim_lock_speed);
f32 getPlayer_c_perspfovy(void);
void currentPlayerAdjustCrouchPos(s32 delta);
void attempt_reload_item_in_hand(s32 hand);
void advance_through_inventory(void);
void backstep_through_inventory(void);
extern f32 g_GlobalTimerDelta;

// Degrees a count outside aim mode, and the crosshair's travel a count in aim mode (1 is the screen's edge), at 100%.
#define LOOK_DEGREES_PER_COUNT 0.11f
#define AIM_TRAVEL_PER_COUNT 0.0049f
// In aim mode the view turns while the crosshair is past this, up to EDGE_TURN_DEGREES a second at the edge.
#define EDGE_BOUNDARY 0.7f
#define EDGE_TURN_DEGREES 120.0f
// The crosshair follows the mouse at once, not with the weapon's damping (which smooths a stick's moves).
#define MOUSE_CROSSHAIR_SPEED 0.01f

// Where the mouse has put the crosshair in aim mode, x right and y up, and whether it has moved it since aim mode
// began: until it has, a pad aims as it always did.
static f32 aim_x;
static f32 aim_y;
static s32 aim_moved;

static f32 clampf(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// The view's zoom: a count turns less the narrower the view, so a zoomed scope moves as far across the screen.
static f32 zoom_scale(void) {
    return clampf(getPlayer_c_perspfovy() / 60.0f, 0.05f, 2.0f);
}

static void turn_view(f32 degrees_x, f32 degrees_y) {
    f32 theta = PLAYER_VV_THETA + degrees_x;
    while (theta >= 360.0f) {
        theta -= 360.0f;
    }
    while (theta < 0.0f) {
        theta += 360.0f;
    }
    PLAYER_VV_THETA = theta;
    PLAYER_VV_VERTA = clampf(PLAYER_VV_VERTA + degrees_y, -90.0f, 90.0f);
}

static void apply_pc_actions(void) {
    u32 actions = recomp_get_pc_actions();
    if (actions & PC_ACTION_RELOAD) {
        attempt_reload_item_in_hand(GUNRIGHT);
        attempt_reload_item_in_hand(GUNLEFT);
    }
    if (actions & PC_ACTION_CROUCH) {
        // The game's own crouch: down to a squat, or up from any crouch to standing.
        currentPlayerAdjustCrouchPos(PLAYER_CROUCHPOS == 2 ? -2 : 2);
    }
    if (actions & PC_ACTION_NEXT_WEAPON) {
        advance_through_inventory();
    }
    if (actions & PC_ACTION_PREV_WEAPON) {
        backstep_through_inventory();
    }
}

// Outside aim mode: x and y are the crosshair's sway, speed its damping.
RECOMP_PATCH void sub_GAME_7F067F58(f32 x, f32 y, f32 speed) {
    void* stats = get_ptr_item_statistics(getCurrentPlayerWeaponId(GUNRIGHT));
    f32 dx, dy;

    apply_pc_actions();
    recomp_take_mouse_look(&dx, &dy);
    if (dx != 0.0f || dy != 0.0f) {
        f32 scale = LOOK_DEGREES_PER_COUNT * zoom_scale();
        turn_view(dx * scale, -dy * scale);
    }
    aim_x = 0.0f;
    aim_y = 0.0f;
    aim_moved = FALSE;

    f32 aim_lock_speed = ITEM_AIM_LOCK_SPEED(stats);
    if (aim_lock_speed < speed) {
        aim_lock_speed = speed;
    }
    caclulate_gun_crosshair_position_rotation(x, y, speed, aim_lock_speed);
}

// In aim mode: x and y are the stick's, up to 0.65 each way.
RECOMP_PATCH void sub_GAME_7F067FBC(f32 x, f32 y) {
    void* stats = get_ptr_item_statistics(getCurrentPlayerWeaponId(GUNRIGHT));
    f32 crosshair_speed = ITEM_CROSSHAIR_SPEED(stats);
    f32 dx, dy;

    apply_pc_actions();
    recomp_take_mouse_look(&dx, &dy);
    if (dx != 0.0f || dy != 0.0f) {
        f32 travel = AIM_TRAVEL_PER_COUNT * zoom_scale();
        aim_x = clampf(aim_x + dx * travel, -1.0f, 1.0f);
        aim_y = clampf(aim_y - dy * travel, -1.0f, 1.0f);
        aim_moved = TRUE;
    }

    if (aim_moved) {
        x = clampf(aim_x + x, -1.0f, 1.0f);
        y = clampf(aim_y + y, -1.0f, 1.0f);
        crosshair_speed = MOUSE_CROSSHAIR_SPEED;

        // Past the boundary the view turns toward that edge, faster the nearer the crosshair is to it.
        f32 seconds = g_GlobalTimerDelta / 60.0f;
        f32 turn = EDGE_TURN_DEGREES * zoom_scale() * seconds / (1.0f - EDGE_BOUNDARY);
        f32 turn_x = 0.0f;
        f32 turn_y = 0.0f;
        if (x > EDGE_BOUNDARY) {
            turn_x = (x - EDGE_BOUNDARY) * turn;
        } else if (x < -EDGE_BOUNDARY) {
            turn_x = (x + EDGE_BOUNDARY) * turn;
        }
        if (y > EDGE_BOUNDARY) {
            turn_y = (y - EDGE_BOUNDARY) * turn;
        } else if (y < -EDGE_BOUNDARY) {
            turn_y = (y + EDGE_BOUNDARY) * turn;
        }
        if (turn_x != 0.0f || turn_y != 0.0f) {
            turn_view(turn_x, turn_y);
        }
    }

    caclulate_gun_crosshair_position_rotation(x, y, crosshair_speed, ITEM_AIM_LOCK_SPEED(stats));
}

// The look-ahead option centres the view up and down while Bond walks, which would undo the mouse's: off while the
// mouse looks, the saved option as it was.
RECOMP_PATCH void currentPlayerSetLookAheadSetting(s32 setting) {
    PLAYER_LOOKAHEAD = recomp_mouse_look_enabled() ? 0 : setting;
}
