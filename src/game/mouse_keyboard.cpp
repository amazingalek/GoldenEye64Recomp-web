// Keyboard and mouse as the PC ports play GoldenEye (the README compares them with the Perfect Dark port): the game's
// side is patches/mouse.c, and this is the program's. RecompFrontend binds keys and a pad's buttons to the N64's
// buttons, and gives a game the mouse's motion; the rest is here, read from SDL:
// - the mouse's buttons on the pad: left fires (Z), right aims (R: the crosshair, and the zoom of the weapons that
//   zoom, while the mouse still turns Bond), and in the game's menus, which the patches say are up, goes back (B);
// - the actions GoldenEye has no N64 button for, which the patches perform: reload (R), crouch (Left Ctrl or C, a
//   toggle), next and previous weapon (the wheel, down and up);
// - the motion, gathered from every event until the patches take it, since the frontend keeps only what came since
//   its last poll and the game reads its controls at its own rate;
// - the keyboard's default bindings, the Mouse Sensitivity option, on at 50%, and Invert Mouse Y, off.
// None of it reaches the game while the frontend's menus have the input, or while the pointer isn't locked.

#include <algorithm>
#include <chrono>
#include <mutex>

#include "SDL.h"

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "recompinput/recompinput.h"
#include "recompinput/input_state.h"
#include "recompinput/input_mapping.h"
#include "recompinput/profiles.h"
#include "recompui/config.h"
#include "librecomp/config.hpp"
#include "goldeneye.h"

namespace {
    // recomp_get_pc_actions' bits, as patches/mouse.c has them.
    constexpr uint32_t action_reload = 0x1;
    constexpr uint32_t action_crouch = 0x2;
    constexpr uint32_t action_next_weapon = 0x4;
    constexpr uint32_t action_prev_weapon = 0x8;

    // At most this many of one action wait for the game, so a spun wheel doesn't keep changing weapon for seconds.
    constexpr int max_pending = 2;
    // Motion and presses the game hasn't taken for this long are dropped: they were made while Bond wasn't the game's
    // to move (a cutscene, the watch, a pause), and would turn him all at once, or reload, after it.
    constexpr auto stale_motion = std::chrono::milliseconds(100);

    std::mutex pending_mutex;
    float pending_dx = 0.0f;
    float pending_dy = 0.0f;
    std::chrono::steady_clock::time_point last_take{};
    std::chrono::steady_clock::time_point last_actions_take{};
    // When a menu last showed its cursor: one did a tick ago at most while it is up, so for as long as motion stays
    // fresh after it.
    std::chrono::steady_clock::time_point last_menu{};
    int pending_actions[4] = {};

    bool game_has_mouse() {
        return !recompinput::game_input_disabled() && SDL_GetRelativeMouseMode() == SDL_TRUE;
    }

    void queue_action(int index) {
        std::lock_guard lock{ pending_mutex };
        pending_actions[index] = std::min(pending_actions[index] + 1, max_pending);
    }

    int watch_events(void*, SDL_Event* event) {
        switch (event->type) {
        case SDL_MOUSEMOTION:
            if (game_has_mouse()) {
                std::lock_guard lock{ pending_mutex };
                pending_dx += event->motion.xrel;
                pending_dy += event->motion.yrel;
            }
            break;
        case SDL_MOUSEWHEEL:
            if (game_has_mouse() && event->wheel.y != 0) {
                bool up = (event->wheel.y > 0) != (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED);
                queue_action(up ? 3 : 2);
            }
            break;
        case SDL_KEYDOWN:
            if (event->key.repeat == 0 && !recompinput::game_input_disabled()) {
                switch (event->key.keysym.scancode) {
                case SDL_SCANCODE_R:
                    queue_action(0);
                    break;
                case SDL_SCANCODE_LCTRL:
                case SDL_SCANCODE_C:
                    queue_action(1);
                    break;
                default:
                    break;
                }
            }
            break;
        }
        return 0;
    }

    // Invert Mouse Y's id and values.
    const std::string invert_y_option = "mouse_invert_y";
    enum class InvertY {
        Off,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> invert_y_options = {
        { InvertY::Off, "Off" },
        { InvertY::On, "On" },
    };

    float mouse_sensitivity() {
        return (float)recompui::config::general::get_mouse_sensitivity() / 100.0f;
    }

    void set_keyboard(recompinput::GameInput input, std::initializer_list<SDL_Scancode> keys) {
        std::vector<recompinput::InputField> fields;
        for (SDL_Scancode key : keys) {
            fields.push_back(recompinput::InputField::keyboard(key));
        }
        recompinput::set_default_mapping_for_keyboard(input, fields);
    }
}

// The N64's buttons as a pad reports them.
constexpr uint16_t n64_button_b = 0x4000;
constexpr uint16_t n64_button_z = 0x2000;
constexpr uint16_t n64_button_r = 0x0010;

void goldeneye::init_mouse_keyboard() {
    using recompinput::GameInput;
    // As the PC ports have it: WASD walks and strafes, the arrows turn and look, the mouse looks and its buttons fire
    // (also F) and aim (also Left Shift); Space and the wheel change weapon, E uses, Return or Tab is Start.
    set_keyboard(GameInput::Y_AXIS_POS, { SDL_SCANCODE_W });
    set_keyboard(GameInput::Y_AXIS_NEG, { SDL_SCANCODE_S });
    set_keyboard(GameInput::C_LEFT, { SDL_SCANCODE_A });
    set_keyboard(GameInput::C_RIGHT, { SDL_SCANCODE_D });
    set_keyboard(GameInput::X_AXIS_NEG, { SDL_SCANCODE_LEFT });
    set_keyboard(GameInput::X_AXIS_POS, { SDL_SCANCODE_RIGHT });
    set_keyboard(GameInput::C_UP, { SDL_SCANCODE_UP });
    set_keyboard(GameInput::C_DOWN, { SDL_SCANCODE_DOWN });
    set_keyboard(GameInput::Z, { SDL_SCANCODE_F });
    set_keyboard(GameInput::R, { SDL_SCANCODE_LSHIFT });
    set_keyboard(GameInput::A, { SDL_SCANCODE_SPACE, SDL_SCANCODE_Q });
    set_keyboard(GameInput::B, { SDL_SCANCODE_E });
    set_keyboard(GameInput::L, { SDL_SCANCODE_X });
    set_keyboard(GameInput::START, { SDL_SCANCODE_RETURN, SDL_SCANCODE_TAB });

    SDL_AddEventWatch(watch_events, nullptr);
}

void goldeneye::add_mouse_option(recomp::config::Config& general) {
    // The frontend's own option, by its id: the frontend locks the pointer while it is above zero.
    general.add_percent_number_option(
        recompui::config::general::options::mouse_sensitivity,
        "Mouse Sensitivity",
        "How far the mouse turns Bond, aiming or not. At 50% a count turns him 0.055 degrees, as the "
        "Perfect Dark PC port's default does. <b>Setting this to zero turns mouse look off</b> and frees the pointer."
        "<br />"
        "<br />"
        "Left click fires and right click aims: it shows the crosshair and zooms the weapons that zoom, and shots go to the "
        "middle of the screen. The wheel changes weapon, R reloads and Left Ctrl or C crouches. In the game's menus "
        "left click selects and right click goes back.",
        50.0
    );
    general.add_enum_option(
        invert_y_option,
        "Invert Mouse Y",
        "Moving the mouse up looks down. The menus' cursor follows the mouse either way.",
        invert_y_options,
        InvertY::Off
    );
}

bool goldeneye::get_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out) {
    bool ret = recompinput::profiles::get_n64_input(controller_num, buttons_out, x_out, y_out);
    if (controller_num == 0 && game_has_mouse()) {
        uint32_t mouse_buttons = SDL_GetMouseState(nullptr, nullptr);
        if (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
            *buttons_out |= n64_button_z;
        }
        if (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            bool in_menu;
            {
                std::lock_guard lock{ pending_mutex };
                in_menu = std::chrono::steady_clock::now() - last_menu < stale_motion;
            }
            *buttons_out |= in_menu ? n64_button_b : n64_button_r;
        }
    }
    return ret;
}

extern "C" void recomp_take_mouse_look(uint8_t* rdram, recomp_context* ctx) {
    float* dx_out = _arg<0, float*>(rdram, ctx);
    float* dy_out = _arg<1, float*>(rdram, ctx);
    float dx = 0.0f;
    float dy = 0.0f;
    {
        std::lock_guard lock{ pending_mutex };
        auto now = std::chrono::steady_clock::now();
        if (now - last_take < stale_motion) {
            dx = pending_dx;
            dy = pending_dy;
        }
        pending_dx = 0.0f;
        pending_dy = 0.0f;
        last_take = now;
    }
    float sensitivity = mouse_sensitivity();
    *dx_out = dx * sensitivity;
    *dy_out = dy * sensitivity;
}

extern "C" void recomp_get_pc_actions(uint8_t* rdram, recomp_context* ctx) {
    uint32_t actions = 0;
    std::lock_guard lock{ pending_mutex };
    auto now = std::chrono::steady_clock::now();
    bool stale = now - last_actions_take >= stale_motion;
    last_actions_take = now;
    constexpr uint32_t bits[4] = { action_reload, action_crouch, action_next_weapon, action_prev_weapon };
    for (int i = 0; i < 4; i++) {
        if (stale) {
            pending_actions[i] = 0;
        }
        else if (pending_actions[i] > 0) {
            pending_actions[i]--;
            actions |= bits[i];
        }
    }
    _return(ctx, actions);
}

extern "C" void recomp_mouse_look_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return<int32_t>(ctx, mouse_sensitivity() > 0.0f ? 1 : 0);
}

extern "C" void recomp_mouse_menu_shown(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pending_mutex };
    last_menu = std::chrono::steady_clock::now();
}

extern "C" void recomp_mouse_invert_y(uint8_t* rdram, recomp_context* ctx) {
    auto value = recompui::config::get_general_config().get_option_value(invert_y_option);
    _return<int32_t>(ctx, static_cast<InvertY>(std::get<uint32_t>(value)) == InvertY::On ? 1 : 0);
}
