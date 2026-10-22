// Keyboard and mouse, the program's side: patches/mouse.c is the game's, and says what it all does in the game.
// input.cpp gathers the mouse's motion and wheel, and locks the pointer to the game while Mouse Sensitivity is above
// zero; this adds what the patches ask for:
// - the mouse's buttons: left click fires (Z on the pad); right click aims, which the patches ask about, and in the
//   game's menus, which the patches say are up, goes back (B on the pad);
// - the actions GoldenEye has no N64 button for, which the patches perform: the Reload and Crouch inputs (R, and Left
//   Ctrl or C, by default), and the wheel for the next and previous weapon;
// - whether the mouse plays: the pointer locked to the game, and the mouse used more recently than a controller;
// - in the tank, which the patches say Bond drives, C-left and C-right (A and D) as the stick's left and right while
//   the mouse plays, so that they turn it;
// - Invert Mouse Y.

#include <algorithm>
#include <chrono>
#include <mutex>

#include "SDL.h"

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "recomp_input.h"
#include "zelda_config.h"

namespace {
// recomp_get_mouse_actions' bits, as patches/mouse.c has them.
constexpr uint32_t action_reload = 0x1;
constexpr uint32_t action_crouch = 0x2;
constexpr uint32_t action_next_weapon = 0x4;
constexpr uint32_t action_prev_weapon = 0x8;

// The N64's buttons as the pad reports them.
constexpr uint16_t n64_button_b = 0x4000;
constexpr uint16_t n64_button_z = 0x2000;
constexpr uint16_t n64_button_c_left = 0x0002;
constexpr uint16_t n64_button_c_right = 0x0001;

// At most this many notches of the wheel wait for the game, so a spun wheel doesn't keep changing weapon for seconds.
constexpr int max_pending_wheel = 2;
// Presses the game hasn't taken for this long were made while Bond wasn't the game's to move (a cutscene, the
// watch, a pause), and are dropped rather than performed after it. A menu that showed its cursor this recently is up.
constexpr auto stale_input = std::chrono::milliseconds(100);

std::mutex mouse_mutex;
std::chrono::steady_clock::time_point last_actions_take{};
std::chrono::steady_clock::time_point last_menu{};
int pending_wheel = 0;
bool reload_was_held = false;
bool crouch_was_held = false;
// Whether Bond drives the tank outside aim mode, as the patches say each tick he can move, until a menu shows.
bool driving_tank = false;

bool game_has_mouse() {
    return !recomp::game_input_disabled() && SDL_GetRelativeMouseMode() == SDL_TRUE;
}

bool mouse_playing() {
    return game_has_mouse() && recomp::mouse_in_use();
}

bool mouse_button_held(int button) {
    return game_has_mouse() && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(button)) != 0;
}

bool menu_shown() {
    std::lock_guard lock{ mouse_mutex };
    return std::chrono::steady_clock::now() - last_menu < stale_input;
}

bool input_held(recomp::GameInput input) {
    if (recomp::game_input_disabled()) {
        return false;
    }
    for (recomp::InputDevice device : { recomp::InputDevice::Keyboard, recomp::InputDevice::Controller }) {
        for (size_t i = 0; i < recomp::bindings_per_input; i++) {
            if (recomp::get_input_digital(recomp::get_input_binding(input, i, device))) {
                return true;
            }
        }
    }
    return false;
}
} // namespace

void recomp::apply_mouse_input(uint16_t* buttons, float* x) {
    if (!game_has_mouse()) {
        return;
    }
    if (mouse_button_held(SDL_BUTTON_LEFT)) {
        *buttons |= n64_button_z;
    }
    if (mouse_button_held(SDL_BUTTON_RIGHT) && menu_shown()) {
        *buttons |= n64_button_b;
    }
    // The tank turns with the stick's left and right; the C-buttons would only swing its turret, which the mouse does.
    bool tank;
    {
        std::lock_guard lock{ mouse_mutex };
        tank = driving_tank;
    }
    if (tank && recomp::mouse_in_use()) {
        if (*buttons & n64_button_c_left) {
            *x -= 1.0f;
        }
        if (*buttons & n64_button_c_right) {
            *x += 1.0f;
        }
        *x = std::clamp(*x, -1.0f, 1.0f);
        *buttons &= ~(n64_button_c_left | n64_button_c_right);
    }
}

// This tick's presses: the Reload and Crouch inputs as they go down, and a notch of the wheel.
extern "C" void recomp_get_mouse_actions(uint8_t* rdram, recomp_context* ctx) {
    bool reload = input_held(recomp::GameInput::RELOAD);
    bool crouch = input_held(recomp::GameInput::CROUCH);
    int wheel = recomp::take_mouse_wheel();
    if (!game_has_mouse()) {
        wheel = 0;
    }

    uint32_t actions = 0;
    {
        std::lock_guard lock{ mouse_mutex };
        auto now = std::chrono::steady_clock::now();
        bool stale = now - last_actions_take >= stale_input;
        last_actions_take = now;
        if (stale) {
            pending_wheel = 0;
        } else {
            if (reload && !reload_was_held) {
                actions |= action_reload;
            }
            if (crouch && !crouch_was_held) {
                actions |= action_crouch;
            }
            pending_wheel = std::clamp(pending_wheel + wheel, -max_pending_wheel, max_pending_wheel);
            if (pending_wheel > 0) {
                pending_wheel--;
                actions |= action_prev_weapon;
            } else if (pending_wheel < 0) {
                pending_wheel++;
                actions |= action_next_weapon;
            }
        }
        reload_was_held = reload;
        crouch_was_held = crouch;
    }
    _return(ctx, actions);
}

extern "C" void recomp_get_mouse_invert_y(uint8_t* rdram, recomp_context* ctx) {
    zelda64::CameraInvertMode mode = zelda64::get_mouse_invert_mode();
    _return<s32>(ctx, mode == zelda64::CameraInvertMode::InvertY || mode == zelda64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_mouse_playing(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, mouse_playing());
}

extern "C" void recomp_mouse_aim_held(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, mouse_button_held(SDL_BUTTON_RIGHT) && !menu_shown());
}

extern "C" void recomp_mouse_menu_shown(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ mouse_mutex };
    last_menu = std::chrono::steady_clock::now();
    driving_tank = false;
}

extern "C" void recomp_mouse_driving_tank(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ mouse_mutex };
    driving_tank = _arg<0, s32>(rdram, ctx) != 0;
}
