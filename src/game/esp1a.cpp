// game/esp1a.cpp: effect id 0x1A, a camera-plane jitter sprite (like esp0b) that is spawned along
// the axis of a model part: the +y axis of parts Work8[0] gives the direction, R_pos.x / .y the
// min / max distance along it and R_pos.z the radius of a random disc around it. The sprite is
// detached into world space at spawn; Vec0 gives the per-frame jitter amplitudes.

#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp1aWork {
    Vec Move_vec;  // 0x00 camera-relative jitter applied this frame
    Vec Dist;  // 0x0C x: sideways jitter, y: vertical jitter, z: distance toward the camera
};

// Jittering sprite spawned in a random cone around a model part (esp0b-style camera jitter).
class cEsp1a : public cEsp {
public:
    Esp1aWork m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" void get_angle(Vec* v, f32* rx, f32* ry);

// EspCreateTbl[0x1A] factory.
cEsp* Esp1a_Create()
{
    return new cEsp1a;
}

// Removes last frame's jitter, runs the base update/animation, then adds a new random offset of
// Dist.x sideways / Dist.y up in the camera plane and Dist.z toward the camera.
void cEsp1a::move()
{
    Esp1aWork* w = &m_Free;
    Vec look;
    Vec up;
    Vec side;
    Vec tmp;
    Vec wpos;
    Mtx inv;
    Camera* cam;

    PSVECSubtract(&m_Pos, &w->Move_vec, &m_Pos);
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            cam = &pG->Camera;
            if (parent != pEffParentWorld) {
                PSMTXMultVec(parent->mat, &m_Pos, &wpos);
            } else {
                wpos = m_Pos;
            }
            PSVECSubtract(&wpos, &cam->param.pos, &look);
#line 84 "D:/Bio4/Prog/esp1a.cpp"
            VECNormalize(&look, &look);
            CameraGetUpVec(cam, &up);
            PSVECCrossProduct(&look, &up, &side);
            PSVECScale(&look, &w->Move_vec, -w->Dist.z);
            PSVECScale(&side, &tmp, w->Dist.x * fRand1_1());
            PSVECAdd(&w->Move_vec, &tmp, &w->Move_vec);
            PSVECScale(&up, &tmp, w->Dist.y * fRand1_1());
            PSVECAdd(&w->Move_vec, &tmp, &w->Move_vec);
            if (parent != pEffParentWorld) {
                PSMTXInverse(parent->mat, inv);
                PSMTXMultVecSR(inv, &w->Move_vec, &w->Move_vec);
            }
            PSVECAdd(&m_Pos, &w->Move_vec, &m_Pos);
        }
    }
}

// Rotation angles (around x then y) that turn +z onto `v`.
void get_angle(Vec* vec, f32* ang_x, f32* ang_y)
{
    Vec t;
    Mtx m;

    if (vec->x == 0.0f && vec->z == 0.0f) {
        *ang_y = 0.0f;
    } else {
        *ang_y = atan2f(vec->x, vec->z);
    }
    PSMTXRotRad(m, 'y', -*ang_y);
    PSMTXMultVec(m, vec, &t);
    if (t.y == 0.0f && t.z == 0.0f) {
        *ang_x = 0.0f;
    } else {
        *ang_x = -atan2f(t.y, t.z);
    }
}

// Requires a parent model: places m_Pos at gen->Pos + random disc offset (radius R_pos.z) +
// a random fraction (R_pos.x..R_pos.y) of the parts' y axis, rotates speed / acceleration by the
// model's angles, then detaches into world space. Fails without a parent or a bad parts number.
int cEsp1a::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp1aWork* w = &m_Free;

    if (parent != pEffParentWorld && (m_Release_time == 0xFF || m_Release_time <= m_Life_time)) {
        cParts* parts;

        if ((s8)pSeq->Work8[0] >= m_pMod->nParts) {
            pLog->err(0, 0, "ESP1a : Wk0 PartsNo > %d ", m_pMod->nParts);
            return 0;
        }
        parts = m_pMod->getPartsPtr((s8)pSeq->Work8[0]);
        m_Pos = *(Vec*)&pSeq->Pos.x;
        {
            Vec dir = { 0.0f, 0.01f, 0.0f };
            Vec sc;
            Mtx inv;
            Vec rot;
            Mtx rm;
            Vec off;
            Mtx m2;
            f32 a;
            f32 len;
            f32 lo;
            f32 hi;
            f32 t;

            PSMTXMultVec(parts->mat, &dir, &dir);
            PSMTXInverse(parent->mat, inv);
            PSMTXMultVec(inv, &dir, &dir);
            get_angle(&dir, &rot.x, &rot.y);
            rot.z = 0.0f;
            RotMatrix(rm, &rot);
            a = fRandSeed0_1(pRand_seed) * 2.0f * PI;
            off.x = SINF(a) * pSeq->R_pos.z * fRandSeed0_1(pRand_seed);
            off.y = COSF(a) * pSeq->R_pos.z * fRandSeed0_1(pRand_seed);
            off.z = 0.0f;
            PSMTXMultVec(rm, &off, &off);
            PSVECAdd(&m_Pos, &off, &m_Pos);
            len = PSVECMag(&dir);
            lo = pSeq->R_pos.x / len;
            hi = pSeq->R_pos.y / len;
            t = 1.0f - lo + hi;
            PSVECScale(&dir, &sc, fRandSeed0_1(pRand_seed) * t + lo);
            PSVECAdd(&m_Pos, &sc, &m_Pos);
            PSMTXMultVec(parent->mat, &m_Pos, &m_Pos);
            parts = m_pMod->getPartsPtr(m_Parts_no);
            PSMTXIdentity(m2);
            low_RotMatrix(m2, &m_pMod->ang);
            PSMTXMultVecSR(m2, &m_Speed, &m_Speed);
            PSMTXMultVecSR(m2, &m_Speed_plus, &m_Speed_plus);
            parent = pEffParentWorld;
            m_pMod = NULL;
        }
    } else {
        pLog->err(0, 0, "ESP1a : no parent!!");
        return 0;
    }
    w->Dist = *(Vec*)&pSeq->Vec0.x;
    return 1;
}
