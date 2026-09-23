// game/esp07.cpp: effect id 0x07, a bouncing particle (debris, shells, drops). Every frame it
// tests the floor (HitType 0: height probed once and cached, 1: probed every frame) or the
// walls (HitType 2, a ray along the speed); on contact it plays SE `SeType`, then per EstCall:
// 0 bounce with damping RefRate (Vec0 x 0.1), 1 spawn the floor / wall est and die, 2 spawn and
// bounce, 3 die. A bounce below 10 units/frame stops the particle for good.

#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

struct Esp07Work {
    Vec RefRate;    // 0x00 x: horizontal damping, y: vertical damping (gen->Vec0.x.. * 0.1)
    u8 GndEstOwner;      // 0x0C est on floor hit (gen->Work8[0])
    u8 GndEstNo;     // 0x0D (gen->Work8[1])
    u8 WallEstOwner;     // 0x0E est on wall hit (gen->Work8[2])
    u8 WallEstNo;    // 0x0F (gen->Work8[3])
    u32 HitType;   // 0x10 0: floor (cached), 1: floor, 2: wall (gen->WorkSp8[0])
    u32 EstCall;   // 0x14 0: bounce, 1: est + die, 2: est + bounce, 3: die (gen->WorkSp8[1])
    u32 Flg;     // 0x18 bit0: stopped, bit1: floor height cached
    f32 GndHeight;    // 0x1C
    u8 SeType;     // 0x20 (gen->WorkSp8[2])
};

// Bouncing particle: checks the floor (or walls) every frame, bounces / spawns an est / dies.
class cEsp07 : public cEsp {
public:
    Esp07Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
void Esp07_ChkGnd(cEsp07* esp, f32 floorY);
void Esp07_HitGndLight(cEsp07* esp);
void Esp07_HitGnd(cEsp07* esp);
void Esp07_HitWall(cEsp07* esp);
}

// EspCreateTbl[0x07] factory.
cEsp* Esp07_Create()
{
    return new cEsp07;
}

// Floor contact test against `floorY` using half the sprite height: on contact snaps the sprite
// onto the floor, plays the SE (SeType 3 always, others within 8000 of the camera), spawns the
// ground est facing the travel direction (EstCall 1/2), releases (EstCall 1/3) or reflects the
// speed (x/z by RefRate.x, y by -RefRate.y) and freezes the particle when it is nearly at rest.
void Esp07_ChkGnd(cEsp07* pEsp, f32 height)
{
    Esp07Work* w = &pEsp->m_Free;
    f32 half;

    half = pEsp->m_Size_base_y * 0.5f * pEsp->m_Size_mul;
    if (DbgFlagChk(pG, DBG_IN_ESP_TOOL) && !DbgFlagChk(pG, DBG_ESPTOOL_ONSCR)) {
        height = 0.0f;
    }
    if (pEsp->m_Pos.y - half < height) {
        pEsp->m_Pos.y = height + half;
        if (w->SeType != 0) {
            Vec d;
            f32 dist;

            PSVECSubtract(&pG->Camera.param.pos, &pEsp->m_Pos, &d);
            dist = PSVECMag(&d);
            if (w->SeType == 3 || dist < 8000.0f) {
                EspCallSeType(w->SeType, &pEsp->m_Pos);
            }
        }
        if (w->EstCall == 1 || w->EstCall == 2) {
            Vec rot;
            Vec p;

            if (pEsp->m_Speed.x == 0.0f && pEsp->m_Speed.z == 0.0f) {
                rot.x = rot.y = rot.z = 0.0f;
            } else {
                rot.x = rot.z = 0.0f;
                rot.y = atan2f(pEsp->m_Speed.x, pEsp->m_Speed.z);
            }
            p = pEsp->m_Pos;
            p.y = height + 65.0f;
            EstSet(0, -1, &p, &rot, w->GndEstOwner, w->GndEstNo, pEsp->info.Core_flg, pEsp->info.Core_kind, pEsp->info.Core_pEm, NULL);
            if (w->EstCall != 2) {
                PushEsp(pEsp);
                return;
            }
        }
        if (w->EstCall == 3) {
            PushEsp(pEsp);
            return;
        }
        pEsp->m_Speed.x = pEsp->m_Speed.x * w->RefRate.x;
        pEsp->m_Speed.y = pEsp->m_Speed.y * -w->RefRate.y;
        pEsp->m_Speed.z = pEsp->m_Speed.z * w->RefRate.x;
        if (PSVECMag(&pEsp->m_Speed) < 10.0f) {
            w->Flg = 1;
            PSVECScale(&pEsp->m_Speed, &pEsp->m_Speed, 0.0f);
            PSVECScale(&pEsp->m_Speed_plus, &pEsp->m_Speed_plus, 0.0f);
        }
    }
}

// HitType 0: probes the floor height once (Flg bit1 caches it) and runs Esp07_ChkGnd.
void Esp07_HitGndLight(cEsp07* pEsp)
{
    Esp07Work* w = &pEsp->m_Free;
    u32 attr;

    if (!(w->Flg & 2)) {
        w->Flg |= 2;
        w->GndHeight = SatMgr.getFloor(&pEsp->m_Pos, &attr, 600.0f, 100000.0f, 0);
    }
    Esp07_ChkGnd(pEsp, w->GndHeight);
}

// HitType 1: probes the floor under the sprite every frame (600 up / 100000 down) and checks it.
void Esp07_HitGnd(cEsp07* pEsp)
{
    u32 attr;

    Esp07_ChkGnd(pEsp, SatMgr.getFloor(&pEsp->m_Pos, &attr, 600.0f, 100000.0f, 0));
}

// HitType 2: casts pos -> pos + speed against the scenery; on a hit moves onto the surface,
// plays the SE, spawns the ground est (normal.y > 0.98) or the wall est, then releases or
// reflects the speed about the normal scaled by RefRate.y and reverses most of the spin.
void Esp07_HitWall(cEsp07* pEsp)
{
    Esp07Work* w = &pEsp->m_Free;
    Vec refl;
    Vec hit;
    Vec n2;
    Vec next;
    Vec nrm;
    Vec rot;
    f32 mag;

    PSVECAdd(&pEsp->m_Pos, &pEsp->m_Speed, &next);
    if (SatMgr.hitCheck(&pEsp->m_Pos, &next, &hit, &nrm, 0, 0)) {
        pEsp->m_Pos = hit;
        PSVECAdd(&nrm, &pEsp->m_Pos, &pEsp->m_Pos);
        if (w->SeType != 0) {
            EspCallSeType(w->SeType, &pEsp->m_Pos);
        }
        if (w->EstCall == 1 || w->EstCall == 2) {
            if (pEsp->m_Speed.x == 0.0f && pEsp->m_Speed.z == 0.0f) {
                rot.x = rot.y = rot.z = 0.0f;
            } else {
                rot.x = rot.z = 0.0f;
                rot.y = atan2f(pEsp->m_Speed.x, pEsp->m_Speed.z);
            }
            if (nrm.y > 0.98f) {
                EstSet(0, -1, &pEsp->m_Pos, &rot, w->GndEstOwner, w->GndEstNo, pEsp->info.Core_flg, pEsp->info.Core_kind, pEsp->info.Core_pEm, NULL);
            } else {
                EstSet(0, -1, &pEsp->m_Pos, &rot, w->WallEstOwner, w->WallEstNo, pEsp->info.Core_flg, pEsp->info.Core_kind, pEsp->info.Core_pEm, NULL);
            }
            if (w->EstCall != 2) {
                PushEsp(pEsp);
                return;
            }
        }
        if (w->EstCall == 3) {
            PushEsp(pEsp);
            return;
        }
        mag = RootSumSquare3(&pEsp->m_Speed);
        n2.x = -nrm.x;
        n2.y = -nrm.y;
        n2.z = -nrm.z;
        C_VECReflect(&pEsp->m_Speed, &n2, &refl);
        PSVECScale(&refl, &pEsp->m_Speed, mag * w->RefRate.y);
        PSVECScale(&pEsp->m_Ang_plus, &pEsp->m_Ang_plus, -0.8f);
    }
}

// Base update, then the HitType collision handler unless the particle has stopped (Flg bit0);
// released when the animation ends.
void cEsp07::move()
{
    Esp07Work* w = &m_Free;

    if (CommonMove()) {
        if (!(w->Flg & 1)) {
            switch (w->HitType) {
            case 0:
                Esp07_HitGndLight(this);
                break;
            case 1:
                Esp07_HitGnd(this);
                break;
            case 2:
                Esp07_HitWall(this);
                break;
            default:
                pLog->err(0, 0, "ESP_07 : HitType[%x] is invalid.", w->HitType);
                PushEsp(this);
                return;
            }
        }
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

// Damping from Vec0 x 0.1, est owner/id pairs from Work8[0..3], HitType / EstCall / SeType from
// WorkSp8[0..2] (range-checked, else fails); a zero life becomes 0x80 frames.
int cEsp07::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp07Work* w = &m_Free;

    w->RefRate = pSeq->Vec0;
    PSVECScale(&w->RefRate, &w->RefRate, 0.1f);
    w->GndEstOwner = pSeq->Work8[0];
    w->GndEstNo = pSeq->Work8[1];
    w->WallEstOwner = pSeq->Work8[2];
    w->WallEstNo = pSeq->Work8[3];
    w->HitType = pSeq->WorkSp8[0];
    w->EstCall = pSeq->WorkSp8[1];
    w->SeType = pSeq->WorkSp8[2];
    if (w->HitType > 2) {
        pLog->err(0, 0, "ESP_07 : HitType[%x] is invalid.", w->HitType);
        return 0;
    }
    if (w->EstCall > 3) {
        pLog->err(0, 0, "ESP_07 : EstCall[%x] is invalid.", w->EstCall);
        return 0;
    }
    if (w->SeType > 3) {
        pLog->err(0, 0, "ESP_07 : SeType[%d] is invalid.", w->SeType);
        return 0;
    }
    if (m_Life_max == 0) {
        m_Life_max = 0x80;
    }
    return 1;
}
