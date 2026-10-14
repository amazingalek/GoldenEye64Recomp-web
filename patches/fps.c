#include "patches.h"

#define MAIN_LOOP_TICK_INTERVAL 0x5eb61U

#if 0
RECOMP_PATCH s32 get_debug_testingmanpos_flag(void) {
    return 1;
}

RECOMP_PATCH s8* bgDebPrintROOMID(s32 roomId) {
    s8* buffer = "01234567890123456789";

    sprintf_recomp(buffer, "printVar %d", printVar); // format and insert the value

    return buffer;
}
#endif

// The main loop's waits for the next tick poll the count. On the console a higher-priority thread (the scheduler, the
// audio thread) preempts the loop at each interrupt; the recomp runs one game thread at a time and switches only at an
// OS call, so a wait that polled without one held them back for all of it: the sound ran dry while it lasted. Each poll
// yields instead (@recomp).
#if 1
RECOMP_PATCH void waitForNextFrame(void) __attribute__((optnone)) // maybe WaitForTick
{
    u32 nextFrameTime; // next frame time?

    while (1) {
        nextFrameTime = ((osGetCount_recomp() - copy_of_osgetcount_value_1) + MAIN_LOOP_TICK_INTERVAL) /
                        ((MAIN_LOOP_TICK_INTERVAL * 2) + 1); // current time + 1/5
        if (nextFrameTime >= frameDelay) {
            break;
        }
        recomp_yield_self_1ms();
    }

    frameDelay = 1;
    updateFrameCounters(nextFrameTime);
}
#endif

#if 1
void waitForNextFrame2(void) // @theboy181 - Improtant for proper fps
{
    u32 nextFrameTime;

    // As long as the frame before (frameDelay is its ticks), so one slow frame holds every one after it to that length,
    // which makes this wait as long as a frame can be: it yields as waitForNextFrame's does.
    while (1) {
        nextFrameTime = ((osGetCount_recomp() - copy_of_osgetcount_value_1) + MAIN_LOOP_TICK_INTERVAL) /
                        ((MAIN_LOOP_TICK_INTERVAL * 2) + 1);
        if (nextFrameTime >= frameDelay) {
            break;
        }
        recomp_yield_self_1ms();
    }

    frameDelay = speedgraphframes;
    updateFrameCounters(nextFrameTime);
}
#endif
