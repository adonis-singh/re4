// game/esp0b.cpp: effect id 0x0B, a sprite jittering in the camera plane. Each frame a new random
// offset of up to prm.x sideways and prm.y up (camera axes) plus prm.z toward the camera replaces
// the previous one. Core_flg 0x8000 effects (moving during pauses) apply the jitter only inside
// the trans function so the stored position stays clean.

#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp0bWork {
    Vec ofs;  // 0x00 offset applied to the position this frame
    Vec BasePos;  // 0x0C x: sideways jitter, y: vertical jitter, z: distance toward the camera
};

// Camera-relative jitter: every frame the sprite is moved by a random offset in the camera's
// side/up plane (and toward the camera by prm.z). With info bit 0x8000 set the offset is
// applied only while drawing.
class cEsp0b : public cEsp {
public:
    Esp0bWork m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

// EspCreateTbl[0x0B] factory.
cEsp* Esp0b_Create()
{
    return new cEsp0b;
}

// Removes last frame's offset, runs the base update/animation, then computes the new camera-plane
// offset (in parent space when attached) and adds it to m_Pos. Skipped for Core_flg 0x8000.
void cEsp0b::move()
{
    Esp0bWork* w = &m_Free;
    Vec look;
    Vec up;
    Vec side;
    Vec tmp;
    Vec wpos;
    Mtx inv;
    CAMERA* cam;

    if (!(info.Core_flg & 0x8000)) {
        PSVECSubtract(&m_Pos, &w->ofs, &m_Pos);
    }
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else if (!(info.Core_flg & 0x8000)) {
            cam = &pG->Camera;
            if (parent != pEffParentWorld) {
                PSMTXMultVec(parent->mat, &m_Pos, &wpos);
            } else {
                wpos = m_Pos;
            }
            PSVECSubtract(&wpos, &cam->param.pos, &look);
            if (look.x == 0.0f && look.y == 0.0f && look.z == 0.0f) {
                pLog->warn(0, 0, "Esp0b : look vec is ZERO");
                look.x = look.y = look.z = 0.0f;
            } else {
#line 101 "D:/Bio4/Prog/esp0b.cpp"
                VECNormalize(&look, &look);
            }
            CameraGetUpVec(cam, &up);
            PSVECCrossProduct(&look, &up, &side);
            PSVECScale(&look, &w->ofs, -w->BasePos.z);
            PSVECScale(&side, &tmp, w->BasePos.x * fRand1_1());
            PSVECAdd(&w->ofs, &tmp, &w->ofs);
            PSVECScale(&up, &tmp, w->BasePos.y * fRand1_1());
            PSVECAdd(&w->ofs, &tmp, &w->ofs);
            if (parent != pEffParentWorld) {
                PSMTXInverse(parent->mat, inv);
                PSMTXMultVecSR(inv, &w->ofs, &w->ofs);
            }
            PSVECAdd(&m_Pos, &w->ofs, &m_Pos);
        }
    }
}

// EspTransTbl[0x0B]: for Core_flg 0x8000 effects computes the jitter here, draws with the offset
// applied and removes it again; otherwise a plain EspCommonTrans.
extern "C" void Esp0b_Trans(cEsp0b* esp)
{
    Vec look;
    Vec up;
    Vec side;
    Vec tmp;
    Vec wpos;
    Mtx inv;
    CAMERA* cam;

    if (esp->info.Core_flg & 0x8000) {
        Esp0bWork* w = &esp->m_Free;
        cam = &pG->Camera;
        if (esp->parent != pEffParentWorld) {
            PSMTXMultVec(esp->parent->mat, &esp->m_Pos, &wpos);
        } else {
            wpos = esp->m_Pos;
        }
        PSVECSubtract(&wpos, &cam->param.pos, &look);
        if (look.x == 0.0f && look.y == 0.0f && look.z == 0.0f) {
            pLog->warn(0, 0, "Esp0b : look vec is ZERO");
            look.x = look.y = look.z = 0.0f;
        } else {
#line 159 "D:/Bio4/Prog/esp0b.cpp"
            VECNormalize(&look, &look);
        }
        CameraGetUpVec(cam, &up);
        PSVECCrossProduct(&look, &up, &side);
        PSVECScale(&look, &w->ofs, -w->BasePos.z);
        PSVECScale(&side, &tmp, w->BasePos.x * fRand1_1());
        PSVECAdd(&w->ofs, &tmp, &w->ofs);
        PSVECScale(&up, &tmp, w->BasePos.y * fRand1_1());
        PSVECAdd(&w->ofs, &tmp, &w->ofs);
        if (esp->parent != pEffParentWorld) {
            PSMTXInverse(esp->parent->mat, inv);
            PSMTXMultVecSR(inv, &w->ofs, &w->ofs);
        }
        PSVECAdd(&esp->m_Pos, &w->ofs, &esp->m_Pos);
        EspCommonTrans(esp);
        PSVECSubtract(&esp->m_Pos, &w->ofs, &esp->m_Pos);
    } else {
        EspCommonTrans(esp);
    }
}

// Jitter amplitudes from Vec0; Work8[0..1] must be 0 (reported, not fatal).
int cEsp0b::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    m_Free.BasePos = pSeq->Vec0;
    if (pSeq->Work8[0] != 0) {
        pLog->err(0, 0, "ESP : 'ESP15' WK0 not 0!! ");
    }
    if (pSeq->Work8[1] != 0) {
        pLog->err(0, 0, "ESP : 'ESP15' WK1 not 0!! ");
    }
    return 1;
}
