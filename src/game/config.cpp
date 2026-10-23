// The settings, on RecompFrontend's config: its General, Controls, Graphics and Sound tabs, saved as general.json,
// controls.json, graphics.json and sound.json in the app folder (recompui::file::get_app_folder_path: %LOCALAPPDATA%,
// ~/.config or ~/Library/Application Support, then GoldenRecomp; APP_FOLDER_PATH or a portable.txt beside the program
// override it). This game's own options are added to its tabs in the order the menus have always had them:
// - General: Rumble Strength, Joystick Deadzone and Background Input (RecompFrontend's), then Mouse Sensitivity and
//   Invert Mouse Y (mouse.cpp reads them).
// - Sound: Main Volume (RecompFrontend's), then Music, Voice and Sound Effects volumes.
// - Graphics and Controls are RecompFrontend's; the Controls tab lists the Reload and Crouch inputs too.

#include <filesystem>

#include "recompui/config.h"
#include "util/file.h"
#include "zelda_config.h"
#include "zelda_sound.h"

namespace {
    namespace sound_options {
        const std::string bgm_volume = "bgm_volume";
        const std::string voice_volume = "voice_volume";
        const std::string sfx_volume = "sfx_volume";
    }

    const std::string mouse_invert_y = "mouse_invert_y";
    enum class InvertY {
        Off,
        On,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> invert_y_options = {
        { InvertY::Off, "Off" },
        { InvertY::On, "On" },
    };

    int get_sound_volume(const std::string& option_id) {
        return static_cast<int>(std::get<double>(recompui::config::get_sound_config().get_option_value(option_id)));
    }
}

void zelda64::init_config() {
    std::filesystem::path app_dir = recompui::file::get_app_folder_path();
    if (!app_dir.empty()) {
        std::filesystem::create_directories(app_dir);
    }
    bool first_run = !std::filesystem::exists(app_dir / (recompui::config::general::id + ".json"));

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    // Added below with this game's description and default.
    general_options.has_mouse_sensitivity = false;
    recomp::config::Config& general = recompui::config::create_general_tab(general_options);
    // RecompFrontend locks the pointer to the game while this, its own option, is above zero.
    general.add_percent_number_option(
        recompui::config::general::options::mouse_sensitivity,
        "Mouse Sensitivity",
        "How far the mouse turns Bond, aiming or not. At 50% a count turns him 0.055 degrees, as the Perfect Dark PC "
        "port's default does. <b>Setting this to zero turns mouse look off</b> and frees the pointer."
        "<br />"
        "<br />"
        "Left click fires and right click aims: it shows the crosshair and zooms the weapons that zoom, and Bond moves "
        "as he does without it. The game's Auto-Aim and Look Ahead are off while the mouse plays. The wheel changes "
        "weapon, or zooms the sniper rifle while aiming. In the game's menus left click selects and right click goes "
        "back. In the tank A and D turn it and the mouse turns its turret.",
        50.0
    );
    general.add_enum_option(
        mouse_invert_y,
        "Invert Mouse Y",
        "Moving the mouse up looks down. The menus' cursor follows the mouse either way.",
        invert_y_options,
        InvertY::Off
    );

    recompui::config::create_controls_tab();
    recompui::config::create_graphics_tab();

    recomp::config::Config& sound = recompui::config::create_sound_tab();
    sound.add_percent_number_option(sound_options::bgm_volume, "Music Volume", "Controls the overall volume of music.", 100.0);
    sound.add_percent_number_option(sound_options::voice_volume, "Voice Volume", "Controls the overall volume of voices.", 100.0);
    sound.add_percent_number_option(sound_options::sfx_volume, "Sound Effects Volume", "Controls the overall volume of sound effects.", 100.0);

    recompui::config::finalize();

    // Rumble starts at 50%, as it always has here, where RecompFrontend's own default is 25%. The tabs made since
    // have moved the general tab's config, so it is looked up again.
    if (first_run) {
        recomp::config::Config& general_config = recompui::config::get_general_config();
        general_config.set_option_value(recompui::config::general::options::rumble_strength, 50.0);
        general_config.save_config();
    }
}

bool zelda64::get_mouse_invert_y() {
    auto value = recompui::config::get_general_config().get_option_value(mouse_invert_y);
    return static_cast<InvertY>(std::get<uint32_t>(value)) == InvertY::On;
}

int zelda64::get_main_volume() {
    return static_cast<int>(recompui::config::sound::get_main_volume());
}

int zelda64::get_bgm_volume() {
    return get_sound_volume(sound_options::bgm_volume);
}

int zelda64::get_sfx_volume() {
    return get_sound_volume(sound_options::sfx_volume);
}

int zelda64::get_voice_volume() {
    return get_sound_volume(sound_options::voice_volume);
}
