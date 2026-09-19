// game/esp43.cpp: effect id 0x43, a delayed sprite gated on the effect tool. Outside the tool the
// sprite is held at life 0 until `started` is set; once running it spawns est Work8[0] (owner 1)
// on the frame the colour fade-in ends, then behaves like a plain sprite.

#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"

struct Esp43Work {
    u8 EstNo;    // 0x00 est to spawn when the effect starts (0xFF = none)
    u8 pad_1[3];
    u32 started; // 0x04
};

Vec g_pos;

// Effect that waits for the cutscene flag before starting and spawns an est on its first frame.
class cEsp43 : public cEsp {
public:
    Esp43Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

// EspCreateTbl[0x43] factory.
cEsp* Esp43_Create()
{
    return new cEsp43;
}

// Dead-stripped from the DOL (its 0.0f pool word survives at .rodata+0x54, STRIP_UNUSED): a debug
// override that moves the effect to g_pos when it is set.
static void Esp43_SetPos(cEsp* esp)
{
    if (g_pos.x != 0.0f || g_pos.y != 0.0f || g_pos.z != 0.0f) {
        esp->m_Pos = g_pos;
    }
}

// While not started: holds m_Life_time at 0 (m_Col_start_cnt forced to 1). In the effect tool
// (Debug_flg[1] 0x00800000) it starts at once. Once started: fires EstSet(owner 1, EstNo) when
// m_Life_time reaches m_Col_start_cnt, then the base update; released when the animation ends.
void cEsp43::move()
{
    Esp43Work* w = &m_Free;
    u32 on = 1;

    if (DbgFlagChk(pG, DBG_IN_ESP_TOOL)) {
        if (w->started == 0) {
            w->started = on;
        }
    }
    if (w->started == 0) {
        m_Col_start_cnt = on;
        m_Life_time = 0;
    } else {
        if (m_Col_start_cnt == m_Life_time && w->EstNo != 0xff) {
            EstSet(0, -1, &m_Pos, &m_Ang, 1, w->EstNo, info.Core_flg, info.Core_kind, info.Core_pEm, 0);
        }
        if (!CommonMove()) {
            return;
        }
    }
    if (!AnmMove()) {
        PushEsp(this);
    }
}

// Remembers the est id (Work8[0], 0xFF = none) to spawn on start.
int cEsp43::SetFreeWork(EspGenWork* gen, u32* seed)
{
    m_Free.EstNo = gen->Work8[0];
    return 1;
}
