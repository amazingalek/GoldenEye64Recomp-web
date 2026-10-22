#include "patches.h"

// The frames RT64 interpolates between two of the game's, across a change of the world's origin (@recomp).
//
// GoldenEye draws its world relative to an origin, the position of the room the camera is in: each frame
// bondviewUpdateCameraMatrices sets it (bondviewUpdateCurrentRoomPosition) before the camera's and the rooms' matrices
// are made from it, and the props' are made relative to it too. So on the frame the camera crosses into another room,
// every room's, prop's and the camera's matrix jumps by the distance between the two rooms' positions. RT64 interpolates
// an untagged camera always and snaps an untagged matrix that jumps (its teleport check), so the frames it drew between
// those two drew the whole world shifted by part of that jump, at every room seam of every level: through a door, the
// room beyond gone to the clear colour for a frame.
//
// Each frame's display list starts with the two matrix groups RT64 gives untagged matrices, the model matrices' and the
// camera's, with an id that changes when the origin does: a frame's matrices are paired with the frame before's only
// with the same id, so the frame the origin moves is shown as it is, and every other frame is interpolated as before.

#define ORIGIN_GROUP_ID 0x47450000 // "GE", and the origin's count in the low half

void getRoomPositionScaledByIndex(s32 room, coord3d* pos);
void setPlayerRoomField(s32 room);

static Gfx* origin_groups; // where this frame's display list has its groups
static u32 origin_count;

static Gfx* origin_write_groups(Gfx* gdl) {
    u32 id = ORIGIN_GROUP_ID | (origin_count & 0xFFFF);

    // As RT64 has an untagged model matrix: decomposed, each component interpolated as its own checks say, paired by
    // its draw calls.
    gEXMatrixGroup(gdl++, id, G_EX_INTERPOLATE_DECOMPOSE, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_AUTO,
                   G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO,
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_AUTO, G_EX_EDIT_NONE, G_EX_ASPECT_AUTO,
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    // As it has an untagged camera: not decomposed, every component interpolated.
    gEXMatrixGroup(gdl++, id, G_EX_INTERPOLATE_SIMPLE, G_EX_NOPUSH, G_MTX_PROJECTION, G_EX_COMPONENT_INTERPOLATE,
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,
                   G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_AUTO,
                   G_EX_EDIT_NONE, G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    return gdl;
}

// Called by bossMainloop at the start of each frame's display list, after gEXEnable. The origin moves later in the
// frame, as the view is drawn, so the groups' id is written again then.
Gfx* originTagFrame(Gfx* gdl) {
    origin_groups = gdl;
    return origin_write_groups(gdl);
}

RECOMP_PATCH void bondviewUpdateCurrentRoomPosition(s32 arg0) {
    // curRoomIndex is the room the player's origin was last set from.
    if ((arg0 != g_CurrentPlayer->curRoomIndex) && (origin_groups != NULL)) {
        origin_count++;
        origin_write_groups(origin_groups);
    }

    getRoomPositionScaledByIndex(arg0, &g_CurrentPlayer->current_model_pos);
    g_CurrentPlayer->current_room_pos.f[0] = g_CurrentPlayer->current_model_pos.f[0] * get_room_data_float1();
    g_CurrentPlayer->current_room_pos.f[1] = g_CurrentPlayer->current_model_pos.f[1] * get_room_data_float1();
    g_CurrentPlayer->current_room_pos.f[2] = g_CurrentPlayer->current_model_pos.f[2] * get_room_data_float1();
    setPlayerRoomField(arg0);
}
