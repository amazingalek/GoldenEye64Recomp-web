// In-app ROM conversion: the ROM picker takes a retail GoldenEye 007 NTSC-U dump, which is what the launcher stores
// (its hash is the game entry's), and when the game starts this, the entry's on_init_callback, converts it into the
// TLBFREE ROM this recompilation needs, using an embedded GEP1 delta patch (no Nintendo assets are shipped; the patch
// only rearranges/augments the user's own dump), and reads the boot segment in again from it.
//
// GEP1 format (little-endian):
//   magic   4 bytes  "GEP1"
//   u32     source size
//   u32     target size
//   records until END:
//     0x00  COPY: u32 src_off, u32 len   (copy from source ROM)
//     0x01  DATA: u32 len, <len bytes>   (literal bytes from the patch)
//     0xFF  END

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <span>
#include <vector>

#include "librecomp/game.hpp"
#include "ultramodern/ultramodern.hpp"
#include "zelda_config.h"
#include "xxHash/xxh3.h"

#include "ge_rom_patch.h"

// XXH3-64 of the retail NTSC-U ROM (big-endian .z64 byte order).
// librecomp has already normalized byteswapped (.v64/.n64) dumps before we're called.
static constexpr uint64_t GE_RETAIL_US_HASH = 0x639ece0bc88c6e4aULL;

gpr get_entrypoint_address();

static uint32_t read_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool apply_gep1(const uint8_t* patch, size_t patch_size, const std::vector<uint8_t>& src, std::vector<uint8_t>& out) {
    if (patch_size < 13 || patch[0] != 'G' || patch[1] != 'E' || patch[2] != 'P' || patch[3] != '1') {
        return false;
    }
    uint32_t src_size = read_u32le(patch + 4);
    uint32_t tgt_size = read_u32le(patch + 8);
    if (src_size != src.size()) {
        return false;
    }
    out.clear();
    out.reserve(tgt_size);

    size_t o = 12;
    while (true) {
        if (o >= patch_size) {
            return false; // ran off the end without an END record
        }
        uint8_t op = patch[o++];
        if (op == 0x00) { // COPY
            if (o + 8 > patch_size) return false;
            uint32_t off = read_u32le(patch + o);
            uint32_t len = read_u32le(patch + o + 4);
            o += 8;
            if ((uint64_t)off + len > src.size()) return false;
            out.insert(out.end(), src.begin() + off, src.begin() + off + len);
        }
        else if (op == 0x01) { // DATA
            if (o + 4 > patch_size) return false;
            uint32_t len = read_u32le(patch + o);
            o += 4;
            if (o + len > patch_size) return false;
            out.insert(out.end(), patch + o, patch + o + len);
            o += len;
        }
        else if (op == 0xFF) { // END
            break;
        }
        else {
            return false;
        }
    }
    return out.size() == tgt_size;
}

void zelda64::convert_rom(uint8_t* rdram, recomp_context* ctx) {
    std::span<const uint8_t> rom = recomp::get_rom();
    if (XXH3_64bits(rom.data(), rom.size()) != GE_RETAIL_US_HASH) {
        // Already the TLBFREE ROM (one stored by hand).
        return;
    }

    printf("Retail NTSC-U GoldenEye ROM detected; converting to the TLBFREE ROM...\n");
    std::vector<uint8_t> retail{ rom.begin(), rom.end() };
    std::vector<uint8_t> converted;
    if (!apply_gep1(reinterpret_cast<const uint8_t*>(ge_rom_patch), sizeof(ge_rom_patch), retail, converted)) {
        ultramodern::error_handling::message_box("Embedded ROM patch failed to apply.");
        return;
    }
    recomp::set_rom_contents(std::move(converted));
    // As librecomp's own start does, from the ROM the game now reads: the boot segment at the entrypoint.
    recomp::do_rom_read(rdram, get_entrypoint_address(), 0x10001000, 0x100000);
}
