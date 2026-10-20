#include "patches.h"

// Keyboard and mouse as the PC ports play it (the README compares them with the Perfect Dark port). The mouse turns Bond
// and tilts his view by an angle a count, as Perfect Dark's port does (0.055 degrees a count at the default 50%, finer
// as the view zooms), aiming or not, and leaves the crosshair where the stick puts it, at the centre without one, as
// Perfect Dark's port's Locked mouse aim and goldeneye-pc-port's Centred aim have it.
//
// Right click aims without the game's aim mode (R): it shows the crosshair and zooms the weapons that zoom, the wheel
// zooms the sniper rifle and the camera further, and nothing else changes. Aim mode would also stop Bond: the stick
// (W and S) moves its crosshair instead of walking him, C-left and C-right (A and D) lean instead of strafing, and
// C-up and C-down crouch. So the game reads Bond's controls as outside aim mode, and the two things right click wants
// are put back where the game decides them each tick: the crosshair after the game hides it, the zoom where it picks
// the view's. R itself (Left Shift, a pad's R) is still the game's aim mode.
//
// The keys and the wheel's actions beyond the N64's buttons (reload, crouch, next and previous weapon) come from the
// program (src/game/mouse_keyboard.cpp), which also puts left click on the pad (fire) and says whether right click is
// held. Invert Mouse Y flips the mouse's up and down for looking and aiming.
//
// In the front end's folders (file, mission and difficulty select) the mouse moves the menus' own cursor as the stick
// does, its left click selects, since the menus take Z as they take A, and its right click goes back, as B does.
//
// The game calls one of two functions each tick it reads Bond's controls, with the crosshair's input: sub_GAME_7F067F58
// outside aim mode and sub_GAME_7F067FBC in it. Both are patched here to turn the view by the mouse; each passes the
// game's own arguments on as the original does, with the weapon's crosshair and aim speeds.

// The current player's fields this writes, at the offsets the recompiled code reads them at (structs.h's comments
// don't all match its layout).
#define PLAYER_FIELD(type, offset) (*(type*)((u8*)g_CurrentPlayer + (offset)))
#define PLAYER_CROUCHPOS PLAYER_FIELD(s32, 0x9C)
#define PLAYER_LOOKAHEAD PLAYER_FIELD(s32, 0x118)
#define PLAYER_VV_THETA PLAYER_FIELD(f32, 0x148)
#define PLAYER_VV_VERTA PLAYER_FIELD(f32, 0x158)
#define PLAYER_INSIGHTAIMMODE PLAYER_FIELD(s32, 0x124)
#define PLAYER_WATCH_ANIMATION_STATE PLAYER_FIELD(s32, 0x1C8)
#define PLAYER_OUTSIDE_WATCH_MENU PLAYER_FIELD(s32, 0x1CC)
#define PLAYER_PAUSING PLAYER_FIELD(s32, 0x200)
#define PLAYER_ZOOMINFOVY PLAYER_FIELD(f32, 0x11C4)

// The watch's zoom out of it, the view without a weapon's zoom.
#define DEFAULT_FOVY 60.0f
// How far one notch of the wheel zooms the sniper rifle or the camera while right click aims: as C-up or C-down held for
// 2.5 ticks in aim mode, a quarter of the view, so about ten notches cover the scope's 60 to 7 degrees.
#define WHEEL_ZOOM 2.5f

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
void recomp_mouse_menu_shown(void);
s32 recomp_mouse_aim_held(void);

s32 getCurrentPlayerWeaponId(s32 hand);
void* get_ptr_item_statistics(s32 item);
void caclulate_gun_crosshair_position_rotation(f32 x, f32 y, f32 crosshair_speed, f32 aim_lock_speed);
f32 getPlayer_c_perspfovy(void);
s32 get_item_in_hand_or_watch_menu(s32 hand);
f32 get_item_in_hand_zoom(void);
void camera_sniper_zoom_in(f32 zoom);
void camera_sniper_zoom_out(f32 zoom);
f32 bondviewGetWatchZoomFovy(void);
void trigger_watch_zoom(f32 fovy, f32 time);
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

// Degrees a count at 100%.
#define LOOK_DEGREES_PER_COUNT 0.11f
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

// Whether the crouch key has Bond squatting, and the player it was pressed for.
static s32 key_crouch;
static void* key_crouch_player;

// Whether right click aims: held, with Bond the player's to move (not behind the watch, nor his arm raising it).
static s32 mouse_aiming(void) {
    return recomp_mouse_aim_held() && PLAYER_OUTSIDE_WATCH_MENU && !PLAYER_PAUSING;
}

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
    // While right click aims the sniper rifle or the camera, the wheel zooms it (up in, down out), as C-up and C-down do
    // in aim mode; otherwise it changes weapon.
    s32 item = get_item_in_hand_or_watch_menu(GUNRIGHT);
    if ((item == ITEM_SNIPERRIFLE || item == ITEM_CAMERA) && mouse_aiming()) {
        if (actions & PC_ACTION_PREV_WEAPON) {
            camera_sniper_zoom_in(WHEEL_ZOOM);
        }
        if (actions & PC_ACTION_NEXT_WEAPON) {
            camera_sniper_zoom_out(WHEEL_ZOOM);
        }
        return;
    }
    if (actions & PC_ACTION_NEXT_WEAPON) {
        advance_through_inventory();
    }
    if (actions & PC_ACTION_PREV_WEAPON) {
        backstep_through_inventory();
    }
}

// Takes this tick's motion and actions, turns the view by the motion and performs the actions. The game reads Bond's
// controls while the watch is up and while his arm raises or lowers it too: what comes then is dropped, or the mouse
// would turn him behind the watch.
static void take_mouse(void) {
    u32 actions = recomp_get_pc_actions();
    f32 dx, dy;
    recomp_take_mouse_look(&dx, &dy);
    if (!PLAYER_OUTSIDE_WATCH_MENU || PLAYER_PAUSING) {
        actions = 0;
    } else if (dx != 0.0f || dy != 0.0f) {
        f32 scale = LOOK_DEGREES_PER_COUNT * zoom_scale();
        turn_view(dx * scale, (recomp_mouse_invert_y() ? dy : -dy) * scale);
    }
    apply_pc_actions(actions);
}

// Outside aim mode: x and y are the crosshair's sway, speed its damping. The game calls this last of all Bond's
// controls, after it has hidden the crosshair outside aim mode, so right click shows it again here.
RECOMP_PATCH void sub_GAME_7F067F58(f32 x, f32 y, f32 speed) {
    void* stats = get_ptr_item_statistics(getCurrentPlayerWeaponId(GUNRIGHT));
    take_mouse();
    if (mouse_aiming()) {
        gunSetSightVisible(GUNSIGHTREASON_NOTAIMING, TRUE);
    }

    f32 aim_lock_speed = ITEM_AIM_LOCK_SPEED(stats);
    if (aim_lock_speed < speed) {
        aim_lock_speed = speed;
    }
    caclulate_gun_crosshair_position_rotation(x, y, speed, aim_lock_speed);
}

// In aim mode: x and y are the stick's, up to 0.65 each way.
RECOMP_PATCH void sub_GAME_7F067FBC(f32 x, f32 y) {
    void* stats = get_ptr_item_statistics(getCurrentPlayerWeaponId(GUNRIGHT));
    take_mouse();
    caclulate_gun_crosshair_position_rotation(x, y, ITEM_CROSSHAIR_SPEED(stats), ITEM_AIM_LOCK_SPEED(stats));
}

// Starts the view's zoom towards fovy, as the original does. Each tick Bond's controls ask for the weapon's zoom in aim
// mode and for the default view outside it, with the watch down: while right click aims, the default becomes the
// weapon's zoom, as aim mode has it. The watch asks for the default too as it starts to rise, which stays as it is.
RECOMP_PATCH void bondviewTriggerWatchZoom(f32 fovy) {
    if (fovy == DEFAULT_FOVY && PLAYER_WATCH_ANIMATION_STATE == 0 && mouse_aiming()) {
        fovy = get_item_in_hand_zoom();
        if (fovy <= 0.0f) {
            fovy = DEFAULT_FOVY;
        }
    }
    if (bondviewGetWatchZoomFovy() != fovy) {
        if (fovy < PLAYER_ZOOMINFOVY) {
            trigger_watch_zoom(fovy, ((PLAYER_ZOOMINFOVY - fovy) * 15.0f) / 30.0f);
            return;
        }
        trigger_watch_zoom(fovy, ((fovy - PLAYER_ZOOMINFOVY) * 15.0f) / 30.0f);
    }
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
// too. Every menu with a way back shows it, so this also tells the program a menu is up, where right click is B.
RECOMP_PATCH void frontUpdateControlStickPosition(void) {
    f32 dx, dy;
    recomp_mouse_menu_shown();
    recomp_take_mouse_look(&dx, &dy);

    f32 left = getPlayer_c_screenleft();
    f32 top = getPlayer_c_screentop();
    cursor_h_pos = clampf(cursor_h_pos + menu_stick_travel(joyGetStickX(0)) + dx * MENU_UNITS_PER_COUNT,
                          left + MENU_CURSOR_MARGIN, left + getPlayer_c_screenwidth() - MENU_CURSOR_MARGIN);
    cursor_v_pos = clampf(cursor_v_pos + menu_stick_travel(-joyGetStickY(0)) + dy * MENU_UNITS_PER_COUNT,
                          top + MENU_CURSOR_MARGIN, top + getPlayer_c_screenheight() - MENU_CURSOR_MARGIN);
}
