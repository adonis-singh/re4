// game/obj04: object id 4, the effect model Efm04 (D:/Bio4/Prog/obj04.cpp): a model spawned by an
// effect record (esp_efm.cpp EfmSetObj04) that flies with speed/acceleration/damping, spins,
// scales and fades over its life, follows its parent parts until rotFrame, and bounces off the
// scenario and floor (flags bit 1) until it comes to rest.
#include "atari.h"
#include "obj.h"
#include "obj04.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "motion.h"

extern "C" {
void Efm04RotMatrix(cObj* obj, Mtx m);
}

// Per-frame Efm04 update: dies with its parent (pointer + serial), detaches from the parent at
// rotFrame, position/speed/scale/rotation/colour envelopes (fadeStart / fadeLen / life like the
// esp sprites), floor + scenario bounces with bounceXZ/bounceY and stops below speed 15.
void cObj04::move()
{
    Efm04Work* w = EFM04_WK(this);
    cLightInfo* li;
    Vec ref;
    Vec hitPos;
    Vec neg;
    Vec nrm;
    u32 attr;
    f32 len;
    int hit;
    static f32 obj04_gnd_ratio = 0.0f;

    if (w->pMod) {
        if (!w->pMod->isAlive()) {
            ObjMgr.destroy(this);
            return;
        }
        if (w->pMod->guid != w->Guid_pMod) {
            ObjMgr.destroy(this);
            return;
        }
    }
    li = &LightInfo;
    if (li->getType() == 2) {
        li->updateMatrix(this);
    }
    if (w->Tool_flg & 8) {
        MotionMove(this, 0);
    }
    if (w->pParts != pEffParentWorld) {
        if (w->Release_time != 0xFF && w->Release_time <= w->Life_time) {
            Efm04RotMatrix(this, w->pParts->mat);
            w->pParts = pEffParentWorld;
        }
        if (w->pParts != pEffParentWorld && w->pMod) {
            if (!(w->pMod->be_flag & 2)) {
                be_flag &= ~2;
            } else {
                be_flag |= 2;
            }
        }
    }
    if (w->Pos_start_cnt <= w->Life_time) {
        pos_old = pos;
        PSVECAdd(&pos, &speed, &pos);
        PSVECAdd(&speed, &w->Speed_plus, &speed);
        PSVECScale(&speed, &speed, w->D_speed);
    }
    if (w->Size_start_cnt <= w->Life_time) {
        w->Size_mul += w->Size_plus;
        w->Size_plus *= w->D_size_plus;
        if (w->Size_mul <= 0.0f) {
            ObjMgr.destroy(this);
            return;
        }
    }
    PSVECAdd(&ang, &w->Ang_plus, &ang);
    if (w->Col_max_cnt < w->Life_time) {
        if (w->Col_max_cnt + w->Col_start_cnt <= w->Life_time) {
            w->Col_r *= w->Col_d_r;
            w->Col_g *= w->Col_d_g;
            w->Col_b *= w->Col_d_b;
            w->Col_a *= w->Col_d_a;
            if (w->Col_r > 255.0f) {
                w->Col_r = 255.0f;
            }
            if (w->Col_g > 255.0f) {
                w->Col_g = 255.0f;
            }
            if (w->Col_b > 255.0f) {
                w->Col_b = 255.0f;
            }
            if (w->Col_a > 255.0f) {
                w->Col_a = 255.0f;
            }
            if (w->Col_a < 4.0f) {
                ObjMgr.destroy(this);
                return;
            }
        }
    } else if (w->Col_max_cnt != 0) {
        f32 ratio = (f32) w->Life_time / (f32) w->Col_max_cnt;
        w->Col_a = (f32) w->Col_start_a * ratio;
    }
    if (ot_type != 2) {
        if (w->Col_a < 250.0f) {
            ot_type = 1;
        } else {
            ot_type = 0;
        }
    }
    if (w->Life_max != 0 && w->Life_max <= w->Life_time) {
        ObjMgr.destroy(this);
        return;
    }
    w->Life_time++;
    pModelInfo->color[0] = (u8) w->Col_r;
    pModelInfo->color[1] = (u8) w->Col_g;
    pModelInfo->color[2] = (u8) w->Col_b;
    pModelInfo->color[3] = 0xFF;
    invisible_factor = w->Col_a * (1.0f / 255.0f);
    scale.y = w->Size_base_y * w->Size_mul;
    scale.z = scale.x = w->Size_base_x * w->Size_mul;
    if (!(w->Flg & 1)) {
        hit = 0;
        if (w->Tool_flg & 2) {
            if (SatMgr.hitCheck(&pos_old, &pos, &hitPos, &nrm, 0, 0)) {
                pos = hitPos;
                hit = 1;
                PSVECAdd(&nrm, &pos, &pos);
                len = RootSumSquare3(&speed);
                neg.x = -nrm.x;
                neg.y = -nrm.y;
                neg.z = -nrm.z;
                C_VECReflect(&speed, &neg, &ref);
                PSVECScale(&ref, &speed, len * w->RefRate.y);
                PSVECScale(&w->Ang_plus, &w->Ang_plus, -0.8f);
            }
        } else if (w->Tool_flg & 1) {
            f32 floor = EatMgr.getFloor(&pos, &attr, 600.0f, 100000.0f, 0);
            f32 ofs = w->Pt_hit_size;

            if (DbgFlagChk(pG, DBG_TEST_MODE)) {
                if (!DbgFlagChk(pG, DBG_ESPTOOL_ONSCR)) {
                    floor = 0.0f;
                }
            }
            if (pos.y - ofs < floor) {
                speed.x = speed.x * w->RefRate.x;
                speed.y = speed.y * -w->RefRate.y;
                speed.z = speed.z * w->RefRate.x;
                pos.y = floor + ofs;
                hit = 1;
                PSVECScale(&w->Ang_plus, &w->Ang_plus, obj04_gnd_ratio);
            }
        }
        if (hit) {
            if (PSVECMag(&speed) < 15.0f) {
                PSVECScale(&speed, &speed, 0.0f);
                PSVECScale(&w->Speed_plus, &w->Speed_plus, 0.0f);
                w->Flg = 1;
            }
        }
    }
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    if (w->pParts != pEffParentWorld) {
        PSMTXConcat(w->pParts->mat, mat, mat);
    }
    partsWorldCalc();
}

// Re-orient the model by `m`: position, speed and acceleration are transformed, the rotation
// is composed with it.
void Efm04RotMatrix(cObj* pObj, Mtx pMat)
{
    Mtx tmp;

    PSMTXMultVec(pMat, &pObj->pos, &pObj->pos);
    PSMTXMultVecSR(pMat, &pObj->speed, &pObj->speed);
    PSMTXMultVecSR(pMat, &EFM04_WK((cObj04*) pObj)->Speed_plus, &EFM04_WK((cObj04*) pObj)->Speed_plus);
    RotMatrix(tmp, &pObj->ang);
    PSMTXConcat(pMat, tmp, tmp);
    Matrix2AxisAngle(tmp, &pObj->ang);
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
