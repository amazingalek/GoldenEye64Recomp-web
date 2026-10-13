#include "patches.h"

// The shader survey: lets a script show any menu and start any stage by itself, to record the shaders each one draws
// for the page's prebuilt list (src/rt64_shaders.txt, made by tools/record_shaders.sh) without anyone playing. It does
// nothing unless the script asks.
//
// The script finds this struct in RDRAM by its two magic words and writes a MENU into `menu`, with a stage in `stage`
// first if the menu needs one (-1 is Dam); `loaded` counts the requests taken, so it knows one was. Once a frame the
// game calls permit_stderr from bossMainloop, on its own thread, so a request is taken here, between frames. The menu
// is switched to as the frontend does on a button, with folder 1, Agent and the stage's solo briefing set up behind
// it, so MENU_RUN_STAGE starts the stage as the menus do. A stage started that way plays, but ending it crashes (the
// mission report reads what the menus before it set up), and so does switching to the difficulty, the briefing or a
// report: the script reaches those through mission select, as a player does.

extern u32 stderr_permitted;
void frontChangeMenu(s32 menu, s32 reload);

struct ShaderSurvey {
    u32 magic[2];
    volatile u32 loaded;
    volatile s32 menu;
    volatile s32 stage;
};

struct ShaderSurvey shader_survey = { { 0x53484452, 0x53525659 }, 0, -1, -1 }; // "SHDR" "SRVY"

RECOMP_PATCH void permit_stderr(u32 flag) {
    stderr_permitted = flag;

    if (shader_survey.menu >= 0) {
        s32 menu = shader_survey.menu;
        shader_survey.menu = -1;
        fileSetCurrentFolder(0);
        set_selected_difficulty(DIFFICULTY_AGENT);
        set_solo_and_ptr_briefing((shader_survey.stage >= 0) ? shader_survey.stage : LEVELID_DAM);
        frontChangeMenu(menu, 1);
        shader_survey.loaded++;
    }
}
