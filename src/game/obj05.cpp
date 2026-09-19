// game/obj05: object id 5, the scattering effect model Efm05 (D:/Bio4/Prog/obj05.cpp): a
// multi-parts model whose parts burst away from `center` one by one as the burst radius grows
// (rangeStep per frame), each flying with its own speed/spin (stored in the parts), bouncing off
// the scenario/floor (flags bit 1) and settling flat (flags bit 3); the body scales/fades like Efm04.
#include "atari.h"
#include "obj.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"

extern "C" {
void Efm05RotMatrix(cObj* obj, Mtx m);
}

// Effect model with loose parts (Efm05): the model scales and fades like obj04 while each parts
// bursts away from `center` once it comes within `range`, flying with its own speed / rotation
// speed (kept in the parts' cModel at 0x128) and bouncing off the scenario / floor.
class cObj05 : public cObj {
public:
    virtual void move();
};

// Per-frame Efm05 update: body scale/colour/life envelopes; parts within the growing radius start
// flying (efmStat 1) with speed pow/range along the radial direction plus random spread; flying
// parts move in parent space with damping/gravity, bounce, and stop (efmStat 2) below speed 15.
void cObj05::move()
{
    Efm05Work* w = &efm05;
    Mtx inv;
    Vec d;
    Vec old;
    Vec v;
    Vec ref;
    Vec hitPos;
    Vec nrm;
    u32 attr;
    f32 range;
    f32 pow;
    f32 rnd;
    f32 rotAmp;
    f32 amp;
    f32 len;
    int hit;
    u32 i;
    cModel* p;

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
        w->a = (f32) w->alpha0 * ratio;
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

    range = (f32) ((w->frame + 1) * (w->rangeStep * 2 + 1)) + 1.0f;
    pow = (f32) (int) w->pow * 4096.0f / range;
    rnd = (f32) (int) w->rnd / 32.0f;
    rotAmp = (f32) (int) w->rotAmp * 0.005f;

    for (i = 0, p = pParts; i < nParts; i++, p = p->pParts) {
        if (p->efmStat == 0) {
            PSVECAdd(&pos, &w->center, &d);
            PSVECSubtract(&p->world, &d, &d);
            if (PSVECMag(&d) < range || w->rangeStep == 0xFF) {
                p->efmStat = 1;
                if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
                    d.y = 1.0f;
                }
#line 162 "D:/Bio4/Prog/obj05.cpp"
                VECNormalize(&d, &p->efmSpd);
                amp = pow * rnd;
                PSVECScale(&p->efmSpd, &p->efmSpd, pow);
                p->efmSpd.x += amp * fRandSeed1_1(&w->seed);
                p->efmSpd.y += amp * fRandSeed1_1(&w->seed);
                p->efmSpd.z += amp * fRandSeed1_1(&w->seed);
                p->efmRotSpd.x = rotAmp * fRandSeed1_1(&w->seed);
                p->efmRotSpd.y = rotAmp * fRandSeed1_1(&w->seed);
                p->efmRotSpd.z = rotAmp * fRandSeed1_1(&w->seed);
            }
        }
        if (p->efmStat == 1) {
            PSMTXInverse(p->pParent->mat, inv);
            PSMTXMultVecSR(inv, &p->efmSpd, &v);
            PSVECAdd(&p->pos, &v, &p->pos);
            PSVECScale(&p->efmSpd, &p->efmSpd, w->spdDamp);
            p->efmSpd.y += w->grav;
            PSVECAdd(&p->ang, &p->efmRotSpd, &p->ang);
            p->ang.x = LIMIT_ANGLE(p->ang.x);
            p->ang.y = LIMIT_ANGLE(p->ang.y);
            p->ang.z = LIMIT_ANGLE(p->ang.z);
            old = p->world;
            PSMTXMultVec(p->pParent->mat, &p->pos, &p->world);
            hit = 0;
            if (w->flags & 2) {
                if (SatMgr.hitCheck(&old, &p->world, &hitPos, &nrm, 0, 0)) {
                    p->world = hitPos;
                    hit = 1;
                    PSVECAdd(&nrm, &p->world, &p->world);
                    len = RootSumSquare3(&p->efmSpd);
                    nrm.x = -nrm.x;
                    nrm.y = -nrm.y;
                    nrm.z = -nrm.z;
                    C_VECReflect(&p->efmSpd, &nrm, &ref);
                    PSVECScale(&ref, &p->efmSpd, len * w->bounceXZ);
                    PSVECScale(&p->efmRotSpd, &p->efmRotSpd, -0.8f);
                }
            } else if (w->flags & 1) {
                f32 floor = EatMgr.getFloor(&p->world, 600.0f, 100000.0f, &attr, 0);
                f32 ofs = (f32) w->groundOfs;

                if (DbgFlagChk(pG, DBG_IN_ESP_TOOL) && !DbgFlagChk(pG, DBG_ESPTOOL_ONSCR)) {
                    floor = 0.0f;
                }
                if (p->world.y - ofs < floor) {
                    FSet(p->efmSpd.x, p->efmSpd.x * w->bounceXZ);
                    FSet(p->efmSpd.y, p->efmSpd.y * -w->bounceY);
                    FSet(p->efmSpd.z, p->efmSpd.z * w->bounceXZ);
                    FSet(p->world.y, floor + ofs);
                    hit = 1;
                    PSVECScale(&p->efmRotSpd, &p->efmRotSpd, 0.8f);
                    if (w->flags & 8) {
                        f32 ry;

                        p->ang.x = LIMIT_ANGLE(p->ang.x);
                        p->ang.y = LIMIT_ANGLE(p->ang.y);
                        p->ang.z = LIMIT_ANGLE(p->ang.z);
                        p->ang.x += PI / 2;
                        PSVECScale(&p->efmRotSpd, &p->efmRotSpd, -0.9f);
                        ry = p->ang.y;
                        PSVECScale(&p->ang, &p->ang, 0.55f);
                        p->ang.y = ry;
                        p->ang.x -= PI / 2;
                    }
                }
            }
            if (hit) {
                PSMTXMultVec(inv, &p->world, &p->pos);
                if (PSVECMag(&p->efmSpd) < 15.0f) {
                    if ((w->flags & 8) && fabsf(p->ang.x + PI / 2) > 0.4f) {
                        f32 ry;

                        p->ang.x = LIMIT_ANGLE(p->ang.x);
                        p->ang.y = LIMIT_ANGLE(p->ang.y);
                        p->ang.z = LIMIT_ANGLE(p->ang.z);
                        p->ang.x += PI / 2;
                        PSVECScale(&p->efmRotSpd, &p->efmRotSpd, -0.7f);
                        ry = p->ang.y;
                        PSVECScale(&p->ang, &p->ang, 0.8f);
                        p->ang.y = ry;
                        p->ang.x -= PI / 2;
                    } else {
                        p->efmStat = 2;
                    }
                }
            }
        }
    }
    matUpdate();
}

// Re-orient the model by `m`: the position and the burst centre are transformed, the rotation
// is composed with it.
void Efm05RotMatrix(cObj* obj, Mtx m)
{
    Mtx tmp;

    PSMTXMultVec(m, &obj->pos, &obj->pos);
    RotMatrix(tmp, &obj->ang);
    PSMTXConcat(m, tmp, tmp);
    Matrix2AxisAngle(tmp, &obj->ang);
    tmp[0][3] = 0.0f;
    tmp[1][3] = 0.0f;
    tmp[2][3] = 0.0f;
    PSMTXMultVec(tmp, &obj->efm05.center, &obj->efm05.center);
}
