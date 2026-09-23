// game/obj02: object id 2, scripted map object cObjScr (D:/Bio4/Prog/obj02.cpp): the room "SMD"
// scroll models (doors, gates, machinery) with a per-type mover (type 1 rotate, 2 swing) selected
// by `type`, an optional motion and a room callback.
#include "atari.h"
#include "obj.h"
#include "obj02.h"
#include "math_sub.h"
#include "motion.h"


struct ObjScrSwingWork {
    f32 phaseZ;   // 0x00
    f32 ampZ;     // 0x04
    f32 freqZ;    // 0x08
    f32 time;     // 0x0C
    f32 phaseX;   // 0x10
    f32 ampX;     // 0x14
    f32 freqX;    // 0x18
    f32 phaseY;   // 0x1C
    f32 ampY;     // 0x20
    f32 freqY;    // 0x24
    Vec baseRot;  // 0x28
};

struct ObjScrRotWork {
    Vec rotSpd;   // 0x00
    u8 flag;      // 0x0C bit0: rotate the first parts instead of the object
};

// New scroll object: no callback.
cObjScr::cObjScr()
{
    attr = 0;
    callBack = 0;
}

// Per-frame: type mover, motion, matrix update, callback, light volume.
void cObjScr::move()
{
    static void (cObjScr::*funcTbl[16])() = {
        &cObjScr::moveNormal, &cObjScr::moveRotate, &cObjScr::moveSwingRot, &cObjScr::moveNormal,
        &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal,
        &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal,
        &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal,
    };

    (this->*funcTbl[type])();
    if (Motion.pMot) {
        MotionMove(this, 0);
    } else {
        matUpdate();
    }
    if (callBack) {
        callBack(this);
    }
    LightInfo.updateMatrix(this);
}

// Type 0 and unused types: nothing.
void cObjScr::moveNormal()
{
}

// Type 1: adds rotSpd to the object angle (flag bit 0: to parts 0 instead).
void cObjScr::moveRotate()
{
    ObjScrRotWork* w = (ObjScrRotWork*)free;

    if (w->flag & 1) {
        PSVECAdd(&pParts->ang, &w->rotSpd, &pParts->ang);
    } else {
        PSVECAdd(&ang, &w->rotSpd, &ang);
    }
}

// Type 2: sinusoidal swing of the three angles around the start angles (amp * sin(freq * t + phase)).
void cObjScr::moveSwingRot()
{
    ObjScrSwingWork* w = (ObjScrSwingWork*)free;

    if (r_no_0 == 0) {
        w->baseRot.x = ang.x;
        w->baseRot.y = ang.y;
        w->baseRot.z = ang.z;
        r_no_0 = 1;
    }
    w->time += 1.0f;
    ang.x = w->baseRot.x + w->ampX * sinf(w->freqX * w->time + w->phaseX);
    ang.y = w->baseRot.y + w->ampY * sinf(w->freqY * w->time + w->phaseY);
    ang.z = w->baseRot.z + w->ampZ * sinf(w->freqZ * w->time + w->phaseZ);
}

// Sets a y-axis swing: amplitude (radians), period (in 1/10000 s -> frequency), phase.
// Never called: the original linker dead-stripped the body (unit in STRIP_UNUSED) and kept its
// pool [1.0, 10000.0, 2pi] right after moveSwingRot's; the body is a guess with that pool.
void cObjScr::SetSwingRot(f32 amp, f32 period, f32 phase)
{
    ObjScrSwingWork* w = (ObjScrSwingWork*)free;
    f32 f = 1.0f / period;

    f *= 10000.0f;
    w->ampY = amp;
    w->freqY = f * 6.2831855f;
    w->phaseY = phase;
}

// Installs the room's per-frame callback.
void cObjScr::SetCallBack(void (*func)(cObj*))
{
    callBack = func;
}
