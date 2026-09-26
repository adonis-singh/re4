// game/quake: camera shake — up to 16 QuakeEntry requests (QuakeExec: delay, duration, amplitude,
// axes) scheduled each frame; the strongest active one shakes pG->Camera by a pseudo-random offset
// in camera space (QuakeMain). Rooms and effects request quakes for explosions, footsteps of the
// giants and the like.
#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "quake.h"
#include <string.h>


QuakeWork Quake;
static Vec QuakeOfsOld[2];

// Once per frame (game loop): advances the quake entries and, while one is active, shakes the camera.
void QuakeMove()
{
    QuakeScheduler();
    if (Quake.active) {
        QuakeMain();
    }
}

// Clears the 16 quake entries (room init).
void QuakeInit()
{
    int i;
    QuakeEntry* e = Quake.ent;

    for (i = 0; i < 16; i++, e++) {
        e->Be_flg = 0;
        e->No = 0;
        e->Delay = 0;
        e->Time = 0;
        e->Scale = 0.0f;
        e->Axis = 0;
    }
    Quake.rnd_idx = 0;
}

// Starts a camera shake: after `delay` frames, `time` frames of amplitude `power` (units) on the
// axes in `axis` (bit0 x, bit1 y, bit2 z); `id` names it for QuakeKill. Silently dropped when all
// 16 entries are busy.
void QuakeExec(u8 id, u16 delay, s16 time, f32 power, u8 axis)
{
    int i;
    QuakeEntry* e = Quake.ent;

    for (i = 0; i < 16; i++, e++) {
        if (!(e->Be_flg & 1)) {
            e->Be_flg = 1;
            e->No = id;
            e->Delay = delay;
            e->Time = time;
            e->Scale = power;
            e->Axis = axis;
            break;
        }
    }
}

// Stops every entry started with `id`.
static void QuakeKill(u8 id)
{
    int i;
    QuakeEntry* e = Quake.ent;

    for (i = 0; i < 16; i++, e++) {
        if ((e->Be_flg & 1) && e->No == id) {
            e->Be_flg = 0;
            e->No = 0;
            e->Delay = 0;
            e->Time = 0;
            e->Scale = 0.0f;
        }
    }
}

// Per frame: counts the delays / times down, frees finished entries, and sets Quake.active /
// power / axis from the strongest running entry (axes of weaker ones are or-ed in only when they
// raise the power).
void QuakeScheduler()
{
    int i;
    QuakeEntry* e;

    Quake.active = 0;
    Quake.axis = 0;
    Quake.power = 0.0f;
    e = Quake.ent;
    for (i = 0; i < 16; i++, e++) {
        if (e->Be_flg & 1) {
            if (e->Delay != 0) {
                e->Delay--;
            } else if (e->Time == 0) {
                e->Be_flg = 0;
                e->No = 0;
                e->Delay = 0;
                e->Time = 0;
                e->Scale = 0.0f;
                e->Axis = 0;
            } else {
                Quake.active = 1;
                if (Quake.power < e->Scale) {
                    Quake.power = e->Scale;
                    Quake.axis |= e->Axis;
                }
                e->Time--;
            }
        }
    }
}

// Applies the shake: a pseudo-random offset (table rnd_tbl x power) per enabled axis, in camera
// space, added to both the camera position and target (pG->Camera), then the up vector is redone.
void QuakeMain()
{
    static s8 rnd_tbl[16] = {0, -1, 1, 2, -1, 0, 1, -1, 1, -1, 0, 1, -1, -2, 0, 1};
    GlobalWork* g = pG;
    CAMERA* cam = &g->Camera;
    Vec ofs = {0.0f, 0.0f, 0.0f};

    if (Quake.axis & 1) {
        Quake.rnd_idx = (Quake.rnd_idx + 1) & 0xF;
        ofs.x = (f32) rnd_tbl[Quake.rnd_idx] * Quake.power;
    }
    if (Quake.axis & 2) {
        Quake.rnd_idx = (Quake.rnd_idx + 1) & 0xF;
        ofs.y = (f32) rnd_tbl[Quake.rnd_idx] * Quake.power;
    }
    if (Quake.axis & 4) {
        Quake.rnd_idx = (Quake.rnd_idx + 1) & 0xF;
        ofs.z = (f32) rnd_tbl[Quake.rnd_idx] * Quake.power;
    }
    PSMTXMultVecSR(cam->mat, &ofs, &ofs);
    PSVECAdd(&g->Camera.param.pos, &ofs, &g->Camera.param.pos);
    PSVECAdd(&g->Camera.param.at, &ofs, &g->Camera.param.at);
    CameraSetOrientationUp(cam);
}
