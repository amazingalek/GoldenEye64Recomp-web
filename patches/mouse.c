#include "patches.h"

// Keyboard and mouse as the PC ports play it (the README compares them with the Perfect Dark port). The mouse looks in
// both of GoldenEye's aim modes: outside aim mode it turns Bond and tilts his view by an angle a count, as Perfect
// Dark's port does (0.055 degrees a count at the default 50%, finer as the view zooms); in aim mode it moves the
// crosshair across the screen, and the view turns while the crosshair is past 70% of the way to an edge. The keys and
// the wheel's actions beyond the N64's buttons (reload, crouch, next and previous weapon) come from the program
// (src/game/mouse_keyboard.cpp), which also puts the mouse's buttons on the pad (fire, aim). Invert Mouse Y flips the
// mouse's up and down for looking and aiming.
//
// In the front end's folders (file, mission and difficulty select) the mouse moves the menus' own cursor as the stick
// does, and its left click selects, since the menus take Z as they take A.
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
#define PLAYER_INSIGHTAIMMODE PLAYER_FIELD(s32, 0x124)
#define PLAYER_OUTSIDE_WATCH_MENU PLAYER_FIELD(s32, 0x1CC)
#define PLAYER_PAUSING PLAYER_FIELD(s32, 0x200)

#define CROUCH_SQUAT 0
#define CROUCH_STAND 2

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
s32 recomp_mouse_invert_y(void);

s32 getCurrentPlayerWeaponId(s32 hand);
void* get_ptr_item_statistics(s32 item);
void caclulate_gun_crosshair_position_rotation(f32 x, f32 y, f32 crosshair_speed, f32 aim_lock_speed);
f32 getPlayer_c_perspfovy(void);
void currentPlayerAdjustCrouchPos(s32 delta);
void attempt_reload_item_in_hand(s32 hand);
void advance_through_inventory(void);
void backstep_through_inventory(void);
s8 joyGetStickX(s8 contpadnum);
s8 joyGetStickY(s8 contpadnum);
f32 getPlayer_c_screenleft(void);
f32 getPlayer_c_screenwidth(void);
f32 getPlayer_c_screentop(void);
f32 getPlayer_c_screenheight(void);
extern f32 g_GlobalTimerDelta;
extern f32 cursor_h_pos;
extern f32 cursor_v_pos;

// Degrees a count outside aim mode, and the crosshair's travel a count in aim mode (1 is the screen's edge), at 100%.
#define LOOK_DEGREES_PER_COUNT 0.11f
#define AIM_TRAVEL_PER_COUNT 0.0049f
// In aim mode the view turns while the crosshair is past this, up to EDGE_TURN_DEGREES a second at the edge.
#define EDGE_BOUNDARY 0.7f
#define EDGE_TURN_DEGREES 120.0f
// The crosshair follows the mouse at once, not with the weapon's damping (which smooths a stick's moves).
#define MOUSE_CROSSHAIR_SPEED 0.01f
// The menu cursor's travel a count at 100%, in the menus' units (the screen is 440 across): at the default 50% a count
// moves it about as far across a full-screen page as the desktop's pointer goes.
#define MENU_UNITS_PER_COUNT 0.5f
// The menu cursor as the game moves it with the stick: past a dead zone of 5, up to 70 counts more, 0.075 units a tick
// each and half a unit besides, kept 20 units inside the screen.
#define MENU_STICK_DEAD_ZONE 5
#define MENU_STICK_MAX 70
#define MENU_STICK_UNITS 0.075f
#define MENU_STICK_MIN_UNITS 0.5f
#define MENU_CURSOR_MARGIN 20.0f

// Where the mouse has put the crosshair in aim mode, x right and y down as the game has them, and whether it has moved it since aim mode
// began: until it has, a pad aims as it always did.
static f32 aim_x;
static f32 aim_y;
static s32 aim_moved;

// Whether the crouch key has Bond squatting, and the player it was pressed for.
static s32 key_crouch;
static void* key_crouch_player;

static f32 clampf(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// The view's zoom: a count turns less the narrower the view, so a zoomed scope moves as far across the screen.
static f32 zoom_scale(void) {
    return clampf(getPlayer_c_perspfovy() / 60.0f, 0.05f, 2.0f);
}

// Turns the view right and up by these angles (the game's theta grows to the right, as the stick turns Bond, and its
// verta upward).
static void turn_view(f32 degrees_right, f32 degrees_up) {
    f32 theta = PLAYER_VV_THETA + degrees_right;
    while (theta >= 360.0f) {
        theta -= 360.0f;
    }
    while (theta < 0.0f) {
        theta += 360.0f;
    }
    PLAYER_VV_THETA = theta;
    PLAYER_VV_VERTA = clampf(PLAYER_VV_VERTA + degrees_up, -90.0f, 90.0f);
}

static void apply_pc_actions(u32 actions) {
    if (actions & PC_ACTION_RELOAD) {
        attempt_reload_item_in_hand(GUNRIGHT);
        attempt_reload_item_in_hand(GUNLEFT);
    }
    // The crouch key toggles a squat, as the PC ports' does. Outside aim mode the game leaves a crouch as it is, so
    // anything else that stands Bond up (a new level, the tank) ends the key's; in aim mode the game stands him back
    // up every tick C-up isn't held, unless C-down is, so while the key's squat lasts it is put back each tick.
    if (key_crouch && (key_crouch_player != g_CurrentPlayer ||
                       (!PLAYER_INSIGHTAIMMODE && PLAYER_CROUCHPOS != CROUCH_SQUAT))) {
        key_crouch = FALSE;
    }
    if (actions & PC_ACTION_CROUCH) {
        key_crouch = !key_crouch;
        key_crouch_player = g_CurrentPlayer;
        if (!key_crouch) {
            currentPlayerAdjustCrouchPos(CROUCH_STAND - CROUCH_SQUAT);
        }
    }
    if (key_crouch && PLAYER_CROUCHPOS != CROUCH_SQUAT) {
        currentPlayerAdjustCrouchPos(CROUCH_SQUAT - CROUCH_STAND);
    }
    if (actions & PC_ACTION_NEXT_WEAPON) {
        advance_through_inventory();
    }
    if (actions & PC_ACTION_PREV_WEAPON) {
        backstep_through_inventory();
    }
}

// Takes this tick's motion and actions, and performs the actions. The game reads Bond's controls while the watch is up
// and while his arm raises or lowers it too: what comes then is dropped, or the mouse would turn him behind the watch.
static void take_mouse(f32* dx, f32* dy) {
    u32 actions = recomp_get_pc_actions();
    recomp_take_mouse_look(dx, dy);
    if (recomp_mouse_invert_y()) {
        *dy = -*dy;
    }
    if (!PLAYER_OUTSIDE_WATCH_MENU || PLAYER_PAUSING) {
        actions = 0;
        *dx = 0.0f;
        *dy = 0.0f;
    }
    apply_pc_actions(actions);
}

// Outside aim mode: x and y are the crosshair's sway, speed its damping.
RECOMP_PATCH void sub_GAME_7F067F58(f32 x, f32 y, f32 speed) {
    void* stats = get_ptr_item_statistics(getCurrentPlayerWeaponId(GUNRIGHT));
    f32 dx, dy;

    take_mouse(&dx, &dy);
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

    take_mouse(&dx, &dy);
    if (dx != 0.0f || dy != 0.0f) {
        f32 travel = AIM_TRAVEL_PER_COUNT * zoom_scale();
        aim_x = clampf(aim_x + dx * travel, -1.0f, 1.0f);
        aim_y = clampf(aim_y + dy * travel, -1.0f, 1.0f);
        aim_moved = TRUE;
    }

    if (aim_moved) {
        x = clampf(aim_x + x, -1.0f, 1.0f);
        y = clampf(aim_y + y, -1.0f, 1.0f);
        crosshair_speed = MOUSE_CROSSHAIR_SPEED;

        // Past the boundary the view turns toward that edge, faster the nearer the crosshair is to it (y is down).
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
            turn_y = -(y - EDGE_BOUNDARY) * turn;
        } else if (y < -EDGE_BOUNDARY) {
            turn_y = -(y + EDGE_BOUNDARY) * turn;
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

// How far the stick moves the menu cursor this tick along one axis.
static f32 menu_stick_travel(s32 v) {
    if (v > MENU_STICK_DEAD_ZONE) {
        v -= MENU_STICK_DEAD_ZONE;
    } else if (v < -MENU_STICK_DEAD_ZONE) {
        v += MENU_STICK_DEAD_ZONE;
    } else {
        return 0.0f;
    }
    if (v > MENU_STICK_MAX) {
        v = MENU_STICK_MAX;
    } else if (v < -MENU_STICK_MAX) {
        v = -MENU_STICK_MAX;
    }
    return ((f32)v * MENU_STICK_UNITS + (v > 0 ? MENU_STICK_MIN_UNITS : -MENU_STICK_MIN_UNITS)) * g_GlobalTimerDelta;
}

// The menus' cursor, each tick a menu shows it: the stick moves it (the stick's y up, the cursor's down), and the mouse
// too.
RECOMP_PATCH void frontUpdateControlStickPosition(void) {
    f32 dx, dy;
    recomp_take_mouse_look(&dx, &dy);

    f32 left = getPlayer_c_screenleft();
    f32 top = getPlayer_c_screentop();
    cursor_h_pos = clampf(cursor_h_pos + menu_stick_travel(joyGetStickX(0)) + dx * MENU_UNITS_PER_COUNT,
                          left + MENU_CURSOR_MARGIN, left + getPlayer_c_screenwidth() - MENU_CURSOR_MARGIN);
    cursor_v_pos = clampf(cursor_v_pos + menu_stick_travel(-joyGetStickY(0)) + dy * MENU_UNITS_PER_COUNT,
                          top + MENU_CURSOR_MARGIN, top + getPlayer_c_screenheight() - MENU_CURSOR_MARGIN);
}
