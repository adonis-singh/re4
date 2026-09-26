#include "types.h"
#include "vec.h"
#include "model.h"
#include "math_sub.h"
#include "rnd.h"
#include "db_log.h"
#include "cSceObj.h"

// Scenario object mover (D:/Bio4/Prog/cSceObj.cpp), shared by the st2_0/st2_3/st4_0 stage RELs.
// The original REL link dead-stripped the members no room of the module calls (STRIP_UNUSED).

// Move obj and the attached models by d.
inline void cSceObj::addPos(Vec* d)
{
    u32 i;

    if (flags & 2) {
        return;
    }
    if (obj == NULL) {
        return;
    }
    PSVECAdd(&obj->pos, d, &obj->pos);
    obj->setPos(&obj->pos);
    for (i = 0; i < 4; i++) {
        if (sub[i]) {
            PSVECAdd(&sub[i]->pos, d, &sub[i]->pos);
            sub[i]->setPos(&sub[i]->pos);
        }
    }
}

// Rotate obj (or its parts parent) by d and carry the attached models around it.
inline void cSceObj::addRot(Vec* d)
{
    Vec out;
    Vec r;
    u32 i;

    if (flags & 1) {
        return;
    }
    if (obj == NULL) {
        return;
    }
    if (flags & 4) {
        r = obj->pList->ang;
    } else {
        r = obj->ang;
    }
    PSVECAdd(&r, d, &out);
    out.x = LIMIT_ANGLE(out.x);
    out.y = LIMIT_ANGLE(out.y);
    out.z = LIMIT_ANGLE(out.z);
    if (flags & 4) {
        obj->pList->ang = out;
    } else {
        obj->ang = out;
    }
    obj->matUpdate();
    for (i = 0; i < 4; i++) {
        if (sub[i]) {
            Mtx m;
            Vec d2;

            if (flags & 4) {
                MTX_COPY(obj->pList->mat, m);
            } else {
                MTX_COPY(obj->mat, m);
            }
            PSVECSubtract(&sub[i]->pos, &obj->pos, &d2);
            PSMTXMultVec(m, &d2, &sub[i]->pos);
        }
    }
}

// Place obj (and attached models) at an absolute position via addPos of the difference.
inline void cSceObj::setPosTo(Vec* target)
{
    Vec d;

    if (obj) {
        PSVECSubtract(target, &obj->pos, &d);
        addPos(&d);
    }
}

// Place obj (or, with flags bit 2, its parts parent) at an absolute rotation via addRot of the difference.
inline void cSceObj::setRotTo(Vec* target)
{
    Vec d;

    if (obj) {
        if (flags & 4) {
            PSVECSubtract(target, &obj->pList->ang, &d);
        } else {
            PSVECSubtract(target, &obj->ang, &d);
        }
        addRot(&d);
    }
}

// Put obj at target position/rotation tp/tr (one scratch Vec for both parts).
inline void cSceObj::moveTo(Vec* tp, Vec* tr)
{
    Vec d;

    if (obj) {
        PSVECSubtract(tp, &obj->pos, &d);
        addPos(&d);
    }
    {
        cModel* o = obj;

        if (o) {
            if (flags & 4) {
                PSVECSubtract(tr, &o->pList->ang, &d);
            } else {
                PSVECSubtract(tr, &o->ang, &d);
            }
            addRot(&d);
        }
    }
}

// Per-frame step, called by the room's task each frame: dispatches on `mode` (0 move1 accel/const/decel,
// 1 move2 no-op, 2 move3 gravity drop). Returns 1 while moving, 0 once the destination is reached.
int cSceObj::move()
{
    if (obj == NULL) {
        return 0;
    }
    {
        int (cSceObj::*tbl[3])() = { &cSceObj::move1, &cSceObj::move2, &cSceObj::move3 };

        return (this->*tbl[mode])();
    }
}

// Accelerate / constant speed / decelerate move from srcPos/srcRot to dstPos/dstRot.
int cSceObj::move1()
{
    Vec vp;
    Vec vr;

    cnt++;
    getVibrationValue(&vp, &vr);
    switch (step) {
    case 0: {
        Vec pos;
        Vec tot;
        Vec rot;
        Vec totR;
        Vec rem;
        Vec accP;
        Vec decP;
        Vec remR;
        Vec accRt;
        Vec decRt;
        f32 s;
        f32 sum;
        f32 k;

        s = accR + decR;
        if (s > 1.0f) {
            accR = accR / s;
            decR = decR / s;
        }
        accFrame = (u32) ((f32) frame * accR + 0.9f);
        decFrame = (u32) ((f32) frame * decR + 0.9f);
        cstFrame = frame - (accFrame + decFrame);
        sum = accR + decR;
        k = 1.0f - sum;
        k = k / (sum * 0.5f + k);
        PSVECSubtract(&dstPos, &srcPos, &tot);
        PSVECScale(&tot, &pos, k);
        PSVECSubtract(&dstRot, &srcRot, &totR);
        PSVECScale(&totR, &rot, k);
        if (sum > 0.0f) {
            PSVECSubtract(&tot, &pos, &rem);
            PSVECScale(&rem, &accP, accR / sum);
            PSVECSubtract(&rem, &accP, &decP);
        } else {
            accP.x = 0.0f;
            accP.y = 0.0f;
            accP.z = 0.0f;
            decP.x = 0.0f;
            decP.y = 0.0f;
            decP.z = 0.0f;
        }
        if (sum > 0.0f) {
            PSVECSubtract(&totR, &rot, &remR);
            PSVECScale(&remR, &accRt, accR / sum);
            PSVECSubtract(&remR, &accRt, &decRt);
        } else {
            accRt.x = 0.0f;
            accRt.y = 0.0f;
            accRt.z = 0.0f;
            decRt.x = 0.0f;
            decRt.y = 0.0f;
            decRt.z = 0.0f;
        }
        if (accFrame != 0) {
            PSVECScale(&accP, &accPos, 2.0f / (f32) (accFrame * accFrame));
            PSVECScale(&accRt, &accRot, 2.0f / (f32) (accFrame * accFrame));
        } else {
            accPos.x = 0.0f;
            accPos.y = 0.0f;
            accPos.z = 0.0f;
            accRot.x = 0.0f;
            accRot.y = 0.0f;
            accRot.z = 0.0f;
        }
        if (decFrame != 0) {
            PSVECScale(&decP, &decPos, -2.0f / (f32) (decFrame * decFrame));
            PSVECScale(&decRt, &decRot, -2.0f / (f32) (decFrame * decFrame));
        } else {
            decPos.x = 0.0f;
            decPos.y = 0.0f;
            decPos.z = 0.0f;
            decRot.x = 0.0f;
            decRot.y = 0.0f;
            decRot.z = 0.0f;
        }
        if (cstFrame != 0) {
            PSVECScale(&pos, &spd, 1.0f / (f32) cstFrame);
            PSVECScale(&rot, &rotSpd, 1.0f / (f32) cstFrame);
        } else {
            PSVECScale(&accPos, &spd, (f32) accFrame);
            PSVECScale(&accRot, &rotSpd, (f32) accFrame);
        }
        cnt = 1;
        step++;
    }
    case 1: {
        Vec pos;
        Vec rot;

        if (cnt <= accFrame) {
            PSVECScale(&accPos, &pos, (f32) (cnt * cnt) * 0.5f);
            PSVECScale(&accRot, &rot, (f32) (cnt * cnt) * 0.5f);
            PSVECAdd(&pos, &vp, &pos);
            PSVECAdd(&rot, &vr, &rot);
            setStartPos();
            addPos(&pos);
            addRot(&rot);
            return 1;
        }
        step++;
    }
    case 2:
        step++;
    case 3: {
        Vec pos;
        Vec rot;
        Vec tmp;

        if (cnt <= accFrame + cstFrame) {
            PSVECScale(&accPos, &pos, (f32) (accFrame * accFrame) * 0.5f);
            PSVECScale(&accRot, &rot, (f32) (accFrame * accFrame) * 0.5f);
            PSVECScale(&spd, &tmp, (f32) (cnt - accFrame));
            PSVECAdd(&pos, &tmp, &pos);
            PSVECScale(&rotSpd, &tmp, (f32) (cnt - accFrame));
            PSVECAdd(&rot, &tmp, &rot);
            PSVECAdd(&pos, &vp, &pos);
            PSVECAdd(&rot, &vr, &rot);
            setStartPos();
            addPos(&pos);
            addRot(&rot);
            return 1;
        }
        step++;
    }
    case 4:
        step++;
    case 5: {
        Vec pos;
        Vec rot;
        Vec tmp;
        Vec tmp2;
        f32 t;
        f32 t2;

        if (cnt <= accFrame + cstFrame + decFrame) {
            PSVECScale(&accPos, &pos, (f32) (accFrame * accFrame) * 0.5f);
            PSVECScale(&accRot, &rot, (f32) (accFrame * accFrame) * 0.5f);
            t = (f32) (cnt - (accFrame + cstFrame));
            PSVECScale(&spd, &tmp, (f32) cstFrame);
            PSVECAdd(&pos, &tmp, &pos);
            PSVECScale(&spd, &tmp2, t);
            t2 = t * t * 0.5f;
            PSVECScale(&decPos, &tmp, t2);
            PSVECAdd(&pos, &tmp2, &pos);
            PSVECAdd(&pos, &tmp, &pos);
            PSVECScale(&rotSpd, &tmp, (f32) cstFrame);
            PSVECAdd(&rot, &tmp, &rot);
            PSVECScale(&rotSpd, &tmp2, t);
            PSVECScale(&decRot, &tmp, t2);
            PSVECAdd(&rot, &tmp2, &rot);
            PSVECAdd(&rot, &tmp, &rot);
            PSVECAdd(&pos, &vp, &pos);
            PSVECAdd(&rot, &vr, &rot);
            setStartPos();
            addPos(&pos);
            addRot(&rot);
            return 1;
        }
        step++;
    }
    case 6:
        moveTo(&dstPos, &dstRot);
        step++;
    case 7:
        return 0;
    default:
        return 1;
    }
}

// Mode 1: no movement (always "still running").
int cSceObj::move2()
{
    return 1;
}

// Gravity drop with a bounce on the destination height.
int cSceObj::move3()
{
    Vec d;

    cnt++;
    switch (step) {
    case 0:
        if (reverse == 1) {
            PSVECScale(&accPos, &accPos, -1.0f);
        }
        cnt = 1;
        step++;
    case 1:
        PSVECAdd(&spd, &accPos, &spd);
        d = spd;
        if (flags & 0x10) {
            if (accPos.y > 0.0f) {
                if (obj->pos.y + spd.y < dstPos.y) {
                    addPos(&spd);
                    return 1;
                }
            } else {
                if (obj->pos.y + spd.y > dstPos.y) {
                    addPos(&spd);
                    return 1;
                }
            }
            d.y -= obj->pos.y + spd.y - dstPos.y;
            addPos(&d);
            spd.y = spd.y * -bounce;
            if (fabsf(spd.y) > fabsf(accPos.y)) {
                return 1;
            }
        }
        step++;
    case 2:
        if (flags & 0x10) {
            PSVECSubtract(&obj->pos, &srcPos, &d);
            d.y = dstPos.y - srcPos.y;
        }
        setStartPos();
        addPos(&d);
        step++;
    case 3:
        return 0;
    default:
        return 1;
    }
}

// Bind a scroll object and start a position-only move1 of `dp` over nFrame frames (acc/dec in percent of
// the duration); records the object's current pos/ang as base and marks it be_flag 0x20 (moved by script).
void cSceObj::initMove1_pos(cModel* o, u32 nFrame, Vec* dp, f32 acc, f32 dec)
{
    if (o) {
        obj = o;
        basePos = o->pos;
        baseRot = o->ang;
        obj->be_flag |= 0x20;
    }
    setMove1_pos(nFrame, dp, acc, dec);
}

// Restart a position-only move1 (dp relative to basePos, rotation kept at baseRot) on the bound object.
void cSceObj::setMove1_pos(u32 nFrame, Vec* dp, f32 acc, f32 dec)
{
    setMove1_all(nFrame, dp, &baseRot, acc, dec, 1);
}

// Bind a scroll object and start a rotation-only move1 of `dr` radians over nFrame frames; flg bit 2
// rotates the parts parent instead of the object itself.
void cSceObj::initMove1_ang(cModel* o, u32 nFrame, Vec* dr, f32 acc, f32 dec, int flg)
{
    flg |= 2;
    if (o) {
        obj = o;
        basePos = o->pos;
        if (flg & 4) {
            baseRot = o->pList->ang;
        } else {
            baseRot = o->ang;
        }
        obj->be_flag |= 0x20;
    }
    setMove1_ang(nFrame, dr, acc, dec, flg);
}

// Restart a rotation-only move1 (dr relative to baseRot) on the bound object.
void cSceObj::setMove1_ang(u32 nFrame, Vec* dr, f32 acc, f32 dec, int flg)
{
    setMove1_all(nFrame, &basePos, dr, acc, dec, flg | 2);
}

// Both position and angle from `o` (st3_3 r320/r332 use it; the other modules' links dropped it).
void cSceObj::initMove1_all(cModel* o, u32 nFrame, Vec* dp, Vec* dr, f32 acc, f32 dec, int flg)
{
    if (o) {
        obj = o;
        basePos = o->pos;
        if (flg & 4) {
            baseRot = o->pList->ang;
        } else {
            baseRot = o->ang;
        }
        obj->be_flag |= 0x20;
    }
    setMove1_all(nFrame, dp, dr, acc, dec, flg);
}


// The 0.01 load is issued after the two Vec copies: a constant-pool load (RTX_UNCHANGING_P) never
// depends on stores, so the constant is a function-local `static const f32` (emitted where the pool
// would be) read through a const reference (a MEM with neither the struct nor the scalar flag, which
// true_dependence keeps below the struct stores through `this`).
void cSceObj::setMove1_all(u32 nFrame, Vec* dp, Vec* dr, f32 acc, f32 dec, int flg)
{
    static const f32 rate = 0.01f;
    flags = flg;
    step = mode = 0;
    frame = nFrame;
    dPos = *dp;
    dRot = *dr;
    f32 r = *(const f32*) &rate;
    accR0 = acc * r;
    decR0 = dec * r;
    decR = decR0;
    accR = accR0;
    setSrcDstPos();
    setStartPos();
}

// Bind a scroll object and start a move3 gravity drop: initial velocity v, gravity grav per frame,
// destination height h relative to base, bounce factor bnc.
void cSceObj::initMove3_y(cModel* o, Vec* v, f32 grav, f32 h, f32 bnc)
{
    if (o) {
        obj = o;
        basePos = o->pos;
        baseRot = o->ang;
        obj->be_flag |= 0x20;
    }
    setMove3_y(v, grav, h, bnc);
}

// Set up the move3 parameters on the bound object (mode 2, flags 0x10 bounce) and put it at the start.
void cSceObj::setMove3_y(Vec* v, f32 grav, f32 h, f32 bnc)
{
    mode = 2;
    step = 0;
    flags = 0x10;
    dPos.x = 0.0f;
    dPos.y = h;
    dPos.z = 0.0f;
    accPos.x = 0.0f;
    accPos.y = grav;
    accPos.z = 0.0f;
    spd = *v;
    bounce = bnc;
    setSrcDstPos();
    setStartPos();
}

// Random offsets along the move direction: 10 units / 1 degree scaled by the phase amplitude.
void cSceObj::getVibrationValue(Vec* vp, Vec* vr)
{
    if (flags & 8) {
        *vp = dPos;
        *vr = dRot;
        if (vp->x != 0.0f || vp->y != 0.0f || vp->z != 0.0f) {
#line 375 "D:/Bio4/Prog/cSceObj.cpp"
            VECNormalize(vp, vp);
        }
        if (vr->x != 0.0f || vr->y != 0.0f || vr->z != 0.0f) {
#line 380 "D:/Bio4/Prog/cSceObj.cpp"
            VECNormalize(vr, vr);
        }
        PSVECScale(vp, vp, 10.0f);
        PSVECScale(vr, vr, PI / 180.0f);
        if (cnt <= vibStart) {
            PSVECScale(vp, vp, fRand1_1() * vibAmp0);
            PSVECScale(vr, vr, fRand1_1() * vibAmp0);
        } else if (cnt > frame - vibEnd) {
            PSVECScale(vp, vp, fRand1_1() * vibAmp2);
            PSVECScale(vr, vr, fRand1_1() * vibAmp2);
        } else {
            PSVECScale(vp, vp, fRand1_1() * vibAmp1);
            PSVECScale(vr, vr, fRand1_1() * vibAmp1);
        }
    } else {
        vp->x = 0.0f;
        vp->y = 0.0f;
        vp->z = 0.0f;
        vr->x = 0.0f;
        vr->y = 0.0f;
        vr->z = 0.0f;
    }
}

// Compute srcPos/Rot and dstPos/Rot from base + delta; `reverse` swaps them (run the move backwards).
void cSceObj::setSrcDstPos()
{
    if (reverse == 1) {
        PSVECAdd(&basePos, &dPos, &srcPos);
        PSVECAdd(&baseRot, &dRot, &srcRot);
        dstPos = basePos;
        dstRot = baseRot;
    } else {
        srcPos = basePos;
        srcRot = baseRot;
        PSVECAdd(&basePos, &dPos, &dstPos);
        PSVECAdd(&baseRot, &dRot, &dstRot);
    }
}

// Snap the object to the move's start pose.
void cSceObj::setStartPos()
{
    setPosTo(&srcPos);
    setRotTo(&srcRot);
}

// Snap the object to the move's end pose (used to skip a move, e.g. when a flag says it already happened).
void cSceObj::setEndPos()
{
    setPosTo(&dstPos);
    setRotTo(&dstRot);
}

// Snap to the start pose and rewind the mover (step 0) so move() replays it.
void cSceObj::setStart()
{
    setStartPos();
    step = 0;
}

// Choose direction (rev = 1: end -> start), swapping the accel/decel ratios, then rewind to the new start pose.
void cSceObj::setReverse(int rev)
{
    if (rev == 1) {
        reverse = 1;
    } else {
        reverse = 0;
    }
    step = 0;
    if (rev == 1) {
        accR = decR0;
        decR = accR0;
    } else {
        accR = accR0;
        decR = decR0;
    }
    setSrcDstPos();
    setStartPos();
}

// Enable position/rotation shake (flags bit 3) between frames start..end of the move with the three
// amplitude coefficients used by getVibrationValue.
void cSceObj::setVibration(u16 start, u16 end, f32 amp0, f32 amp1, f32 amp2)
{
    flags |= 8;
    vibStart = start;
    vibEnd = end;
    vibAmp0 = amp0;
    vibAmp1 = amp1;
    vibAmp2 = amp2;
}
