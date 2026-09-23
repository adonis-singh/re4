// game/esp14.cpp: effect id 0x14, a vertical light shaft sprite. Each frame the beam is turned to
// face the camera about the vertical axis and its length is stretched with the horizontal camera
// distance (Work8[0] x 0.05 + 0.25 per unit), damped by the eighth power of the view elevation,
// and clipped to an x/z box (Vec0 half extents around -Vec1) so it never leaves its room.

#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

struct Esp14Work {
    f32 Mul;  // 0x00 sizeY per unit of camera distance
    f32 Base_y;    // 0x04 base sizeY
    Vec Rimiter;       // 0x08 clip box half extents (x, z)
    Vec Rimiter_ofs;       // 0x14 clip box center (x, z)
};

// Light shaft sprite: a vertical beam whose length depends on the camera view and is clipped
// to a box around the effect.
class cEsp14 : public cEsp {
public:
    Esp14Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

// EspCreateTbl[0x14] factory.
cEsp* Esp14_Create()
{
    return new cEsp14;
}

// Texture animation only (no base motion); sets m_Ang to face the camera, m_Size_base_y = len *
// Mul * (1 - |sin elevation|)^8 + Base_y, shortens it where the beam end would cross the clip
// box, and marks the effect as never Z-culled (huge m_Radius, m_Flg bit1).
void cEsp14::move()
{
    Esp14Work* w = &m_Free;
    Vec d;
    Vec cross;
    Vec camDir;
    f32 len;
    f32 rate;
    f32 sy, cy;
    f32 min;
    f32 t, t2;
    f32 nx, nz;
    f32 lim0, lim1, lim2, lim3;

    if (!AnmMove()) {
        PushEsp(this);
    } else {
        PSVECSubtract(&pG->Camera.param.at, &pG->Camera.param.pos, &camDir);
        PSVECSubtract(&m_Pos, &pG->Camera.param.pos, &d);
        PSVECCrossProduct(&d, &pG->Camera.Up, &cross);
        m_Ang.x = PI / 2.0f;
        m_Ang.y = atan2f(cross.z, -cross.x);
        len = SQRTF(d.x * d.x + d.z * d.z);
        rate = d.y / PSVECMag(&d);
        if (rate < 0.0f) {
            rate = -rate;
        }
        rate = 1.0f - rate;
        rate = rate * rate;
        rate = rate * rate;
        rate = rate * rate;
        m_Size_base_y = len * w->Mul * rate + w->Base_y;
        if (m_Size_base_y < w->Base_y) {
            m_Size_base_y = m_Size_base_x;
        }
        if (w->Rimiter.x != 0.0f || w->Rimiter.z != 0.0f) {
            min = 1.0f;
            nx = -w->Rimiter_ofs.x;
            nz = -w->Rimiter_ofs.z;
            lim0 = nx + w->Rimiter.x;
            lim1 = nx - w->Rimiter.x;
            lim3 = nz + w->Rimiter.z;
            lim2 = nz - w->Rimiter.z;
            sy = sinf(m_Ang.y) * m_Size_base_y;
            cy = cosf(m_Ang.y) * m_Size_base_y;
            if (sy > lim0) {
                t = lim0 / sy;
                if (t < min) {
                    min = t;
                }
            }
            if (sy < lim1) {
                t = lim1 / sy;
                if (t < min) {
                    min = t;
                }
            }
            if (cy > lim3) {
                t2 = lim3 / cy;
                if (t2 < min) {
                    min = t2;
                }
            }
            if (cy < lim2) {
                t2 = lim2 / cy;
                if (t2 < min) {
                    min = t2;
                }
            }
            if (min != 1.0f) {
                cy = cy * min;
                sy = sy * min;
                m_Size_base_y = SQRTF(sy * sy + cy * cy);
            }
        }
        m_Radius = 100000000.0f;
        m_Flg |= 2;
    }
}

// Length factor from Work8[0], clip box from Vec0 (extents) and Vec1 (centre offset); fails when
// the box is inconsistent or has a y component. Sets Tool_flg bit0.
int cEsp14::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp14Work* w = &m_Free;

    w->Mul = (f32)(s8)pSeq->Work8[0] * 0.05f + 0.25f;
    w->Base_y = m_Size_base_y;
    w->Rimiter = pSeq->Vec0;
    w->Rimiter_ofs = pSeq->Vec1;
    if (fabsf(pSeq->Vec0.x) < fabsf(pSeq->Vec1.x) || fabsf(pSeq->Vec0.z) < fabsf(pSeq->Vec1.z) ||
        fabsf(pSeq->Vec0.y) != 0.0f || fabsf(pSeq->Vec1.y) != 0.0f) {
        pLog->err(0, 0, "ESP14 : Vec0 or Vec1 Invalid Paramater.");
        return 0;
    }
    m_Tool_flg |= 1;
    return 1;
}
