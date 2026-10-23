// The settings: RecompFrontend's stock General, Controls, Graphics and Sound tabs, with the keyboard's bindings and
// the Mouse Sensitivity option of mouse_keyboard.cpp. The options GoldenRecomp had of its own (music and effect
// volumes, camera inversion) aren't here: the natives that read them (recomp_api.cpp) answer with the original game's
// behaviour, and the Sound tab's main volume scales the mix.

#include <filesystem>

#include "recompui/config.h"
#include "util/file.h"
#include "goldeneye.h"

void goldeneye::init_config() {
    std::filesystem::path app_dir = recompui::file::get_app_folder_path();
    if (!app_dir.empty()) {
        std::filesystem::create_directories(app_dir);
    }

    goldeneye::init_mouse_keyboard();

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = true;
    recompui::config::create_general_tab(general_options);
    goldeneye::add_mouse_option(recompui::config::get_general_config());
    recompui::config::create_controls_tab();
    recompui::config::create_graphics_tab();
    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();
    recompui::config::finalize();
    goldeneye::mouse_option_loaded();
}
