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

    if (w->parent) {
        if ((w->parent->be_flag & 0x201) != 1) {
            ObjMgr.destroy(this);
            return;
        }
        if (w->parent->guid != w->parentSerial) {
            ObjMgr.destroy(this);
            return;
        }
    }
    li = &LightInfo;
    if ((li->Flag & 3) == 2) {
        li->updateMatrix(this);
    }
    if (w->flags & 8) {
        MotionMove(this, 0);
    }
    if (w->parentWorld != pEffParentWorld) {
        if (w->rotFrame != 0xFF && w->rotFrame <= w->frame) {
            Efm04RotMatrix(this, w->parentWorld->mat);
            w->parentWorld = pEffParentWorld;
        }
        if (w->parentWorld != pEffParentWorld && w->parent) {
            if (!(w->parent->be_flag & 2)) {
                be_flag &= ~2;
            } else {
                be_flag |= 2;
            }
        }
    }
    if (w->moveStart <= w->frame) {
        pos_old = pos;
        PSVECAdd(&pos, &speed, &pos);
        PSVECAdd(&speed, &w->acc, &speed);
        PSVECScale(&speed, &speed, w->spdDamp);
    }
    if (w->scaleStart <= w->frame) {
        w->scale += w->scaleSpd;
        w->scaleSpd *= w->scaleDamp;
        if (w->scale <= 0.0f) {
            ObjMgr.destroy(this);
            return;
        }
    }
    PSVECAdd(&ang, &w->rotSpd, &ang);
    if (w->fadeStart < w->frame) {
        if (w->fadeStart + w->fadeLen <= w->frame) {
            w->r *= w->rMul;
            w->g *= w->gMul;
            w->b *= w->bMul;
            w->a *= w->aMul;
            if (w->r > 255.0f) {
                w->r = 255.0f;
            }
            if (w->g > 255.0f) {
                w->g = 255.0f;
            }
            if (w->b > 255.0f) {
                w->b = 255.0f;
            }
            if (w->a > 255.0f) {
                w->a = 255.0f;
            }
            if (w->a < 4.0f) {
                ObjMgr.destroy(this);
                return;
            }
        }
    } else if (w->fadeStart != 0) {
        f32 ratio = (f32) w->frame / (f32) w->fadeStart;
        w->a = (f32) w->a0 * ratio;
    }
    if (ot_type != 2) {
        if (w->a < 250.0f) {
            ot_type = 1;
        } else {
            ot_type = 0;
        }
    }
    if (w->life != 0 && w->life <= w->frame) {
        ObjMgr.destroy(this);
        return;
    }
    w->frame++;
    pModelInfo->color[0] = (u8) w->r;
    pModelInfo->color[1] = (u8) w->g;
    pModelInfo->color[2] = (u8) w->b;
    pModelInfo->color[3] = 0xFF;
    invisible_factor = w->a * (1.0f / 255.0f);
    scale.y = w->scaleY * w->scale;
    scale.z = scale.x = w->scaleXZ * w->scale;
    if (!(w->stopped & 1)) {
        hit = 0;
        if (w->flags & 2) {
            if (SatMgr.hitCheck(&pos_old, &pos, &hitPos, &nrm, 0, 0)) {
                pos = hitPos;
                hit = 1;
                PSVECAdd(&nrm, &pos, &pos);
                len = RootSumSquare3(&speed);
                neg.x = -nrm.x;
                neg.y = -nrm.y;
                neg.z = -nrm.z;
                C_VECReflect(&speed, &neg, &ref);
                PSVECScale(&ref, &speed, len * w->bounce.y);
                PSVECScale(&w->rotSpd, &w->rotSpd, -0.8f);
            }
        } else if (w->flags & 1) {
            f32 floor = EatMgr.getFloor(&pos, &attr, 600.0f, 100000.0f, 0);
            f32 ofs = w->groundOfs;

            if (DbgFlagChk(pG, DBG_TEST_MODE)) {
                if (!DbgFlagChk(pG, DBG_ESPTOOL_ONSCR)) {
                    floor = 0.0f;
                }
            }
            if (pos.y - ofs < floor) {
                speed.x = speed.x * w->bounce.x;
                speed.y = speed.y * -w->bounce.y;
                speed.z = speed.z * w->bounce.x;
                pos.y = floor + ofs;
                hit = 1;
                PSVECScale(&w->rotSpd, &w->rotSpd, obj04_gnd_ratio);
            }
        }
        if (hit) {
            if (PSVECMag(&speed) < 15.0f) {
                PSVECScale(&speed, &speed, 0.0f);
                PSVECScale(&w->acc, &w->acc, 0.0f);
                w->stopped = 1;
            }
        }
    }
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    if (w->parentWorld != pEffParentWorld) {
        PSMTXConcat(w->parentWorld->mat, mat, mat);
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
    PSMTXMultVecSR(pMat, &EFM04_WK((cObj04*) pObj)->acc, &EFM04_WK((cObj04*) pObj)->acc);
    RotMatrix(tmp, &pObj->ang);
    PSMTXConcat(pMat, tmp, tmp);
    Matrix2AxisAngle(tmp, &pObj->ang);
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
