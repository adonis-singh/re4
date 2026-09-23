// game/esp06.cpp: effect id 0x06, a sprite riding an effect path (EspGetPathAddr owner Work8[0],
// id Work8[1]). Dist advances by PathSpeed (prm 0xCC, +- xD4 random, accelerated by 0xD0 / 10)
// each frame; at the path end Flg (Work8[2]) bit0 loops (pausing StopFrame + random frames),
// bit1 parks at the end, else the sprite dies. Vec1 (degrees) / Vec0 (scale in 10ths) build
// PathMat, a transform applied to the path; Vec2 gives a start fraction along it.

#include "atari.h"
#include "light.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp06Work {
    u8 PathId;    // 0x00 (gen->Work8[1])
    u8 Flg;     // 0x01 bit0: loop, bit1: stop at the end, bit2: stopped, bit7: has matrix (gen->Work8[2])
    u16 pathId;   // 0x02 (gen->Work8[0])
    u16 seg;      // 0x04 current path segment (PathGetPos reads/writes a halfword)
    u8 pad_6[2];
    void* pPath;   // 0x08
    f32 Dist;     // 0x0C distance along the path
    Vec LocalPos;      // 0x10 base position
    Mtx PathMat;      // 0x1C rotation / scale applied to the path
    f32 PathSpeed;      // 0x4C
    f32 PathAccele;      // 0x50
    u8 StopFrame;  // 0x54 frames to wait at a loop restart (gen->WorkSp8[0])
    u8 StopFrameRnd;   // 0x55 random addition to waitBase (gen->WorkSp8[1])
    u8 wait;      // 0x56
};

// Path follower: moves the sprite along an effect path (loops / stops / dies at the end).
class cEsp06 : public cEsp {
public:
    Esp06Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
int Esp06GetPathPos(cEsp06* esp);
void esp06_CommonMove(cEsp06* esp);
void esp06_Move00(cEsp06* esp);
void esp06_Move01(cEsp06* esp);
}

static void (*Esp06MoveTbl[])(cEsp06*) = { esp06_Move00, esp06_Move01 };

// EspCreateTbl[0x06] factory.
cEsp* Esp06_Create()
{
    return new cEsp06;
}

// Samples the path at Dist into m_Pos (weighted paths attached to a model use PathGetPosEm).
// Returns 0 when Dist is past either end.
int Esp06GetPathPos(cEsp06* pEsp)
{
    Esp06Work* w = &pEsp->m_Free;
    int ret;

    if (PathHasWeight(w->pPath)) {
        if (pEsp->m_pMod != NULL) {
            ret = PathGetPosEm(w->pPath, pEsp->m_pMod, w->Dist, &w->seg, &pEsp->m_Pos);
        } else {
            ret = PathGetPos(w->pPath, w->Dist, &w->seg, &pEsp->m_Pos);
        }
    } else {
        ret = PathGetPos(w->pPath, w->Dist, &w->seg, &pEsp->m_Pos);
    }
    return ret;
}

// The full per-frame update: on release from the parent bakes the parent matrix into LocalPos,
// speeds, angles and PathMat; integrates LocalPos (the path origin) with the base speed, applies
// scale / colour / life / animation, advances Dist (or counts down `wait`), handles the path end
// (loop / stop / die) and sets m_Pos = PathMat * path point + LocalPos.
void esp06_CommonMove(cEsp06* pEsp)
{
    Esp06Work* w = &pEsp->m_Free;
    Mtx m;

    if (pEsp->parent != pEffParentWorld && pEsp->m_Release_time != 0xFF && pEsp->m_Release_time <= pEsp->m_Life_time) {
        PSMTXMultVecSR(pEsp->parent->mat, &w->LocalPos, &w->LocalPos);
        PSMTXMultVecSR(pEsp->parent->mat, &pEsp->m_Speed, &pEsp->m_Speed);
        PSMTXMultVecSR(pEsp->parent->mat, &pEsp->m_Speed_plus, &pEsp->m_Speed_plus);
        if (pEsp->m_Tool_flg & 1) {
            RotMatrix(m, &pEsp->m_Ang);
            PSMTXConcat(pEsp->parent->mat, m, m);
            Matrix2AxisAngle(m, &pEsp->m_Ang);
        }
        PSMTXConcat(pEsp->parent->mat, w->PathMat, w->PathMat);
        w->Flg |= 0x80;
        pEsp->parent = pEffParentWorld;
    }
    if (pEsp->m_Pos_start_cnt <= pEsp->m_Life_time) {
        PSVECAdd(&w->LocalPos, &pEsp->m_Speed, &w->LocalPos);
        w->PathSpeed += w->PathAccele;
        PSVECAdd(&pEsp->m_Speed, &pEsp->m_Speed_plus, &pEsp->m_Speed);
        PSVECScale(&pEsp->m_Speed, &pEsp->m_Speed, pEsp->m_D_speed);
    }
    if (pEsp->m_Size_start_cnt <= pEsp->m_Life_time) {
        pEsp->m_Size_mul += pEsp->m_Size_plus;
        pEsp->m_Size_plus *= pEsp->m_D_size_plus;
        if (pEsp->m_Size_mul <= 0.0f) {
            PushEsp(pEsp);
            return;
        }
    }
    PSVECAdd(&pEsp->m_Ang, &pEsp->m_Ang_plus, &pEsp->m_Ang);
    if (pEsp->ColorUpdate()) {
        if (pEsp->m_Life_max != 0 && pEsp->m_Life_max <= pEsp->m_Life_time) {
            PushEsp(pEsp);
            return;
        }
        pEsp->m_Life_time++;
        if (!pEsp->AnmMove()) {
            PushEsp(pEsp);
            return;
        }
        {
            if (w->wait == 0) {
                w->Dist += w->PathSpeed;
            } else {
                w->wait--;
            }
            if (!Esp06GetPathPos(pEsp)) {
                if (w->Flg & 1) {
                    if (w->PathSpeed > 0.0f) {
                        w->Dist -= PathGetLength(w->pPath);
                    } else {
                        w->Dist += PathGetLength(w->pPath);
                    }
                    Esp06GetPathPos(pEsp);
                    w->wait = w->StopFrame;
                    if (w->StopFrameRnd != 0) {
                        w->StopFrame += (u32)Rnd() % w->StopFrameRnd;
                    }
                } else if (w->Flg & 2) {
                    if (w->PathSpeed > 0.0f) {
                        w->Dist = PathGetLength(w->pPath) - 0.1f;
                    } else {
                        w->Dist = 0.0f;
                    }
                    Esp06GetPathPos(pEsp);
                    w->Flg |= 4;
                } else {
                    PushEsp(pEsp);
                    return;
                }
            }
            if (w->Flg & 0x80) {
                PSMTXMultVec(w->PathMat, &pEsp->m_Pos, &pEsp->m_Pos);
            }
            PSVECAdd(&pEsp->m_Pos, &w->LocalPos, &pEsp->m_Pos);
        }
    }
}

// Rno0 == 0: first frame; runs the common update and moves to Rno0 1.
void esp06_Move00(cEsp06* esp)
{
    esp06_CommonMove(esp);
    esp->m_Rno0 = 1;
}

// Rno0 == 1: steady state, the common update.
void esp06_Move01(cEsp06* esp)
{
    esp06_CommonMove(esp);
}

// Dispatches on m_Rno0 through Esp06MoveTbl.
void cEsp06::move()
{
    Esp06MoveTbl[m_Rno0](this);
}

// Resolves the path (fails when missing), reads speed / acceleration / wait parameters, forces
// the speed sign to match prm 0xCC, builds PathMat from Vec1 rotation and Vec0 scale, and picks
// the start distance from Vec2 (percent + random percent, wrapped) or the far end for a negative
// speed.
int cEsp06::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp06Work* w = &m_Free;
    f32 t;

    w->pathId = pSeq->Work8[0];
    w->PathId = pSeq->Work8[1];
    w->Flg = pSeq->Work8[2];
    w->PathSpeed = (f32)(s32)pSeq->prm.w.xCC;
    w->PathAccele = (f32)(s32)pSeq->prm.w.xD0 * 0.1f;
    w->PathSpeed += (f32)(s32)pSeq->xD4 * fRandSeed1_1(pRand_seed);
    w->StopFrame = pSeq->WorkSp8[0];
    w->StopFrameRnd = pSeq->WorkSp8[1];
    if ((f32)(s32)pSeq->prm.w.xCC != 0.0f) {
        if ((f32)(s32)pSeq->prm.w.xCC > 0.0f) {
            if (w->PathSpeed < 0.0f) {
                w->PathSpeed = -w->PathSpeed;
            }
        } else {
            if (w->PathSpeed > 0.0f) {
                w->PathSpeed = -w->PathSpeed;
            }
        }
    }
    w->pPath = EspGetPathAddr(w->pathId, w->PathId);
    if (w->pPath == NULL) {
        return 0;
    }
    w->LocalPos = m_Pos;
    if (pSeq->Vec1.x != 0.0f || pSeq->Vec1.y != 0.0f || pSeq->Vec1.z != 0.0f) {
        Vec r;

        r = pSeq->Vec1;
        w->Flg |= 0x80;
        PSVECScale(&r, &r, 0.017453292f);
        RotMatrix(w->PathMat, &r);
    } else {
        PSMTXIdentity(w->PathMat);
    }
    if (pSeq->Vec0.x != 0.0f || pSeq->Vec0.y != 0.0f || pSeq->Vec0.z != 0.0f) {
        Vec s;
        Mtx sm;

        s = pSeq->Vec0;
        w->Flg |= 0x80;
        PSVECScale(&s, &s, 0.1f);
        s.x += 1.0f;
        s.y += 1.0f;
        s.z += 1.0f;
        PSMTXScale(sm, s.x, s.y, s.z);
        PSMTXConcat(w->PathMat, sm, w->PathMat);
    }
    if (pSeq->Vec2.x != 0.0f || pSeq->Vec2.y != 0.0f) {
        t = pSeq->Vec2.x * 0.01f;
        t += pSeq->Vec2.y * 0.01f * fRandSeed0_1(pRand_seed);
        if (t > 1.0f) {
            t -= (f32)(u32)t;
        }
        if (t < 0.0f) {
            t += (f32)(u32)(-t) + 1.0f;
        }
        w->Dist = t * PathGetLength(w->pPath);
    } else if (w->PathSpeed < 0.0f) {
        w->Dist = PathGetLength(w->pPath);
    }
    return 1;
}
