// game/esp10.cpp: effect id 0x10, a ground decal. At spawn the sprite is detached from its parent
// and snapped 65 units + Vec0.y above the floor found by a collision ray (600 up / 100000 down);
// Work8[3] selects the extra rule: 1 = discard when the point is inside the room's effect area,
// 2 = never below the water surface.

#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"

// Floor decal: dropped onto the ground (or the water surface) when created.
class cEsp10 : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" int EffAreaCheckInRoom(Vec* pos);

// EspCreateTbl[0x10] factory.
cEsp* Esp10_Create()
{
    return new cEsp10;
}

// Standard sprite update; released when the animation ends.
void cEsp10::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

// Floor height under `pos`: casts a ray from pos.y + up down to pos.y - down against the scenery
// collision (SatMgr.hitCheck2, mask 0x40) and returns the hit y and its attribute in *attr;
// -100000 when nothing is hit (or Debug_flg[1] 0x10000000 disables the probe: returns 0).
f32 getFloor_attr(Vec* pos, u32* attr, int x, f32 up, f32 down)
{
    Vec top;
    Vec bottom;
    Vec hit;
    int r;

    if (DbgFlagChk(pG, DBG_FLAT_FLOOR)) {
        return 0.0f;
    }
    top.x = pos->x;
    top.y = pos->y + up;
    top.z = pos->z;
    bottom.x = pos->x;
    bottom.y = pos->y - down;
    bottom.z = pos->z;
    r = SatMgr.hitCheck2(&top, &bottom, &hit, attr, 0x40, x);
    if (r != 0 && !(r & 4)) {
        return hit.y;
    }
    return -100000.0f;
}

// Moves the effect into world space, sets m_Pos.y to floor + 65 + Vec0.y (0 in the effect tool),
// then applies the Work8[3] rule (0 none, 1 in-room check, 2 water clamp); other values fail.
int cEsp10::SetFreeWork(EspGenWork* gen, u32* seed)
{
    u32 attr;
    f32 h;

    if (parent != pEffParentWorld) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    FSet(m_Pos.y, getFloor_attr(&m_Pos, &attr, 0, 600.0f, 100000.0f) + 65.0f + gen->Vec0.y);
    if (DbgFlagChk(pG, DBG_IN_ESP_TOOL) && !DbgFlagChk(pG, DBG_ESPTOOL_ONSCR)) {
        m_Pos.y = 0.0f;
    }
    switch ((s8)gen->Work8[3]) {
    case 0:
        break;
    case 1:
        if (EffAreaCheckInRoom(&m_Pos) == 1) {
            PushEsp(this);
        }
        break;
    case 2:
        if (GetWaterHeight(&m_Pos, &h)) {
            if (m_Pos.y < h + gen->Vec0.y) {
                m_Pos.y = h + gen->Vec0.y;
            }
        }
        break;
    default:
        pLog->err(0, 0, "ESP10 : Type[%d] Invalid Trans.", (s8)gen->Work8[3]);
        return 0;
    }
    return 1;
}
