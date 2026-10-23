#ifndef __GOLDENEYE_H__
#define __GOLDENEYE_H__

#include <string_view>

#include "recomp.h"

namespace recomp::config {
    class Config;
}

namespace goldeneye {
    inline constexpr std::u8string_view program_id = u8"GoldenRecomp";
    inline constexpr std::string_view program_name = "GoldenEye 007: Recompiled";

    void register_overlays();
    void register_patches();
    void init_config();

    // Keyboard and mouse (mouse_keyboard.cpp): the keyboard's default bindings and the events it watches, before the
    // config is loaded; the Mouse Sensitivity option, in the General tab; the pad with the mouse's buttons on it.
    void init_mouse_keyboard();
    void add_mouse_option(recomp::config::Config& general);
    void mouse_option_loaded();
    bool get_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out);

    // The game entry's on_init_callback: turns the stored retail ROM into the TLBFREE ROM the code was recompiled
    // from (rom_conversion.cpp).
    void convert_rom(uint8_t* rdram, recomp_context* ctx);
}

#endif
