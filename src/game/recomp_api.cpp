#include <cmath>

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "recompinput/recompinput.h"
#include "recompui/recompui.h"
#include "recompui/renderer.h"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"
#include "librecomp/game.hpp"
#include "librecomp/addresses.hpp"

extern "C" void boot_osPiRawStartDma(uint8_t* rdram, recomp_context* ctx) {
    uint32_t direction = ctx->r4;
    uint32_t device_address = ctx->r5;
    gpr rdram_address = ctx->r6;
    uint32_t size = ctx->r7;

    assert(direction == 0); // Only reads

    // Complete the DMA synchronously (the game immediately waits until it's done anyways)
    recomp::do_rom_read(rdram, rdram_address, device_address + recomp::rom_base, size);
}

extern "C" void osDpGetCounters_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Empty
}

extern "C" void osPiReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Empty
}

extern "C" void osPfsInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Empty
}

// __f_to_ll_recomp and __ll_lshift_recomp are librecomp's (math_routines.cpp).

// TODO: Validate this
extern "C" void __ll_to_d_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Properly sign-extend high and zero-extend low
    int64_t input = ((int64_t) (ctx->r4) << 32) | (uint32_t) ctx->r5;

    double result = (double) input;
    ctx->f0.d = result;
}

extern "C" void recomp_update_inputs(uint8_t* rdram, recomp_context* ctx) {
    recompinput::poll_inputs();
}

// The patches' audio loops yield through this: ultramodern's yield_self_1ms takes rdram alone, and WebAssembly traps
// on a call whose signature doesn't match the callee.
extern "C" void yield_self_1ms(uint8_t* rdram);
extern "C" void recomp_yield_self_1ms(uint8_t* rdram, recomp_context* ctx) {
    yield_self_1ms(rdram);
}

extern "C" void rmonPrintf_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Empty
}

extern "C" void __ll_rshift_recomp(uint8_t* rdram, recomp_context* ctx) {
    int64_t a = (ctx->r4 << 32) | ((ctx->r5 << 0) & 0xFFFFFFFFu);
    int64_t b = (ctx->r6 << 32) | ((ctx->r7 << 0) & 0xFFFFFFFFu);
    int64_t ret = a >> b;

    ctx->r2 = (int32_t) (ret >> 32);
    ctx->r3 = (int32_t) (ret >> 0);
}

extern "C" void recomp_puts(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    for (u32 i = 0; i < length; i++) {
        fputc(MEM_B(i, (gpr) cur_str), stdout);
    }
}

extern "C" void recomp_exit(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::quit();
}

extern "C" void recomp_get_gyro_deltas(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    recompinput::get_gyro_deltas(0, x_out, y_out);
}

extern "C" void recomp_get_mouse_deltas(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    recompinput::get_mouse_deltas(x_out, y_out);
}

extern "C" void recomp_powf(uint8_t* rdram, recomp_context* ctx) {
    float a = _arg<0, float>(rdram, ctx);
    float b = ctx->f14.fl; //_arg<1, float>(rdram, ctx);

    _return(ctx, std::pow(a, b));
}

extern "C" void recomp_get_target_framerate(uint8_t* rdram, recomp_context* ctx) {
    int frame_divisor = _arg<0, u32>(rdram, ctx);

    _return(ctx, ultramodern::get_target_framerate(60 / frame_divisor));
}

extern "C" void recomp_get_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::renderer::GraphicsConfig graphics_config = ultramodern::renderer::get_graphics_config();
    float original = _arg<0, float>(rdram, ctx);
    int width, height;
    recompui::get_window_size(width, height);

    switch (graphics_config.ar_option) {
        case ultramodern::renderer::AspectRatio::Original:
        default:
            _return(ctx, original);
            return;
        case ultramodern::renderer::AspectRatio::Expand:
            _return(ctx, std::max(static_cast<float>(width) / height, original));
            return;
    }
}

extern "C" void recomp_get_targeting_mode(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, 0);
}

extern "C" void recomp_get_bgm_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, 1.0f);
}

extern "C" void recomp_get_sfx_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, 1.0f);
}

extern "C" void recomp_get_voice_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, 1.0f);
}

extern "C" void recomp_get_low_health_beeps_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, 1u);
}

extern "C" void recomp_time_us(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<u32>(
                     std::chrono::duration_cast<std::chrono::microseconds>(ultramodern::time_since_start()).count()));
}

extern "C" void recomp_autosave_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, 0);
}

extern "C" void recomp_load_overlays(uint8_t* rdram, recomp_context* ctx) {
    u32 rom = _arg<0, u32>(rdram, ctx);
    PTR(void) ram = _arg<1, PTR(void)>(rdram, ctx);
    u32 size = _arg<2, u32>(rdram, ctx);

    load_overlays(rom, ram, size);
}

extern "C" void recomp_high_precision_fb_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<s32>(recompui::renderer::RT64HighPrecisionFBEnabled()));
}

extern "C" void recomp_get_resolution_scale(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, ultramodern::get_resolution_scale());
}

extern "C" void recomp_get_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    *x_out = 0;
    *y_out = 0;
}

extern "C" void recomp_get_analog_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    *x_out = 0;
    *y_out = 0;
}

extern "C" void recomp_analog_cam_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, 0);
}

extern "C" void recomp_get_camera_inputs(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    // TODO expose this in the menu
    constexpr float radial_deadzone = 0.05f;

    float x, y;

    recompinput::get_right_analog(0, &x, &y);

    float magnitude = sqrtf(x * x + y * y);

    if (magnitude < radial_deadzone) {
        *x_out = 0.0f;
        *y_out = 0.0f;
    } else {
        float x_normalized = x / magnitude;
        float y_normalized = y / magnitude;

        *x_out = x_normalized * ((magnitude - radial_deadzone) / (1 - radial_deadzone));
        *y_out = y_normalized * ((magnitude - radial_deadzone) / (1 - radial_deadzone));
    }
}

extern "C" void recomp_set_right_analog_suppressed(uint8_t* rdram, recomp_context* ctx) {
    s32 suppressed = _arg<0, s32>(rdram, ctx);

    recompinput::set_right_analog_suppressed(suppressed);
}
