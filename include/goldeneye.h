#ifndef __GOLDENEYE_H__
#define __GOLDENEYE_H__

#include <string_view>

#include "recomp.h"

namespace goldeneye {
    inline constexpr std::u8string_view program_id = u8"GoldenRecomp";
    inline constexpr std::string_view program_name = "GoldenEye 007: Recompiled";

    void register_overlays();
    void register_patches();
    void init_config();

    // The game entry's on_init_callback: turns the stored retail ROM into the TLBFREE ROM the code was recompiled
    // from (rom_conversion.cpp).
    void convert_rom(uint8_t* rdram, recomp_context* ctx);
}

#endif
