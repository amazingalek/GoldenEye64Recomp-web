#ifndef __ZELDA_SOUND_H__
#define __ZELDA_SOUND_H__

// The Sound tab's volumes, 0-100 (config.cpp). The main volume is RecompFrontend's own option.
namespace zelda64 {
    int get_main_volume();
    int get_bgm_volume();
    int get_sfx_volume();
    int get_voice_volume();
}

#endif
