// game/obj1c: object id 0x1C, the floating island / raft of the lake (D:/Bio4/Prog/obj1c.cpp):
// drifts back to its home position (50 units/frame outside a 50-unit radius), is pushed away
// (300 units/frame, decaying) and plays a crash motion when Del Lago hits it (setCrash /
// setCrashBig; ckCrash reports the 15-frame crash window to the room), spawns water effects
// every 30 frames, and is hidden with its effects during Status_flg[1] 0x80000.
#include "atari.h"
#include "light.h"
#include "obj.h"
#include "obj1c.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "motion.h"
#include "est.h"

extern "C" {
void obj1c_R1_Set(cObj1c* obj);
void obj1c_R1_Crash(cObj1c* obj);
void obj1c_R1_CrashBig(cObj1c* obj);
void obj1cSpdMove(cObj1c* obj);
}

void (*Obj1c_R1_move_tbl[3])(cObj1c*) = { obj1c_R1_Set, obj1c_R1_Crash, obj1c_R1_CrashBig };

// Creates the island at pos/rot (home = pos) with its own effect owner kind.
cObj* SetFloatIsland(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    IslandWork* w;

    obj = ObjMgr.create(cObjMgr::ID_FLOATISLAND);
    if (obj == 0) {
        return 0;
    }
    w = ISLAND_WK((cObj1c*) obj);
    if (pos) {
        obj->pos = *pos;
    }
    if (rot) {
        obj->ang = *rot;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj1c() modelInit() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 3000.0f, 3000.0f, 0.0f };

    obj->atari.off();
    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    w->Be_flg = 0;
    w->Eff_wait = (u8) ((u32) Rnd() % 30);
    w->Eff_wait2 = 0;
    w->motIdle = 0;
    w->motCrash = 0;
    w->St_pos = obj->pos;
    w->Spd.x = 0.0f;
    w->Spd.y = 0.0f;
    w->Spd.z = 0.0f;
    w->EffKindId = EspPullCoreKind();
    obj->r_no_1 = 0;
    obj->r_no_0 = 1;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    RotMatrix(obj->mat, &obj->ang);
    TransMatrix(obj->mat, &obj->pos);
    ScaleMatrix(obj->mat, &obj->scale);
    obj->partsMatCalc();
    obj->partsWorldCalc();
    return obj;
}

// Per-frame: timers, R1 routine, and hide/show with effect deletion by Status_flg[1] 0x80000.
void cObj1c::move()
{
    IslandWork* w = ISLAND_WK(this);
    u32 f;

    be_flag &= ~0x4000;
    if (w->Crash_wait) {
        w->Crash_wait--;
    }
    if (w->Eff_wait2) {
        w->Eff_wait2--;
    }
    Obj1c_R1_move_tbl[r_no_1](this);
    f = be_flag;
    if ((f & 0x201) == 1) {
        if (StaFlagChk(pG, STA_PL_SWIM_CAMERA)) {
            be_flag = f & ~2;
            EffectEspDelete(0, w->EffKindId, this, 0);
            EffectEspgenDelete(0, w->EffKindId, this);
            EffectEfmDelete(0, w->EffKindId, this);
        } else {
            be_flag = f | 2;
        }
    }
}

// Rno1 == 0: drift, a water effect (est 1/0) every 30 frames while visible, idle motion.
void obj1c_R1_Set(cObj1c* pObj)
{
    IslandWork* w = ISLAND_WK(pObj);

    obj1cSpdMove(pObj);
    if (w->Eff_wait) {
        w->Eff_wait--;
    } else {
        w->Eff_wait = 30;
        if (pObj->be_flag & 2) {
            EstSet(pObj, -1, 0, 0, EFF_ROOM, 0, 0, w->EffKindId, pObj, 0);
        }
    }
    if (pObj->Motion.pMot) {
        MotionMove(pObj, 0);
    } else {
        RotMatrix(pObj->mat, &pObj->ang);
        TransMatrix(pObj->mat, &pObj->pos);
        ScaleMatrix(pObj->mat, &pObj->scale);
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// Rno1 == 1: crash motion, then back to the idle motion (big variant when scale >= 1.5).
void obj1c_R1_Crash(cObj1c* pObj)
{
    IslandWork* w = ISLAND_WK(pObj);

    obj1cSpdMove(pObj);
    if (pObj->Motion.pMot) {
        if (MotionMove(pObj, 0)) {
            if (w->motIdle) {
                if (pObj->scale.x >= 1.5f) {
                    MotionSetCore(pObj, &pObj->Motion, w->motIdleBig, 0, 0, 5, 0);
                } else {
                    MotionSetCore(pObj, &pObj->Motion, w->motIdle, 0, 0, 5, 0);
                }
                pObj->r_no_0 = 1;
                pObj->r_no_1 = 0;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            }
        }
    } else {
        RotMatrix(pObj->mat, &pObj->ang);
        TransMatrix(pObj->mat, &pObj->pos);
        ScaleMatrix(pObj->mat, &pObj->scale);
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// Rno1 == 2: same as Crash (the big crash entry).
void obj1c_R1_CrashBig(cObj1c* pObj)
{
    IslandWork* w = ISLAND_WK(pObj);

    obj1cSpdMove(pObj);
    if (pObj->Motion.pMot) {
        if (MotionMove(pObj, 0)) {
            if (w->motIdle) {
                if (pObj->scale.x >= 1.5f) {
                    MotionSetCore(pObj, &pObj->Motion, w->motIdleBig, 0, 0, 5, 0);
                } else {
                    MotionSetCore(pObj, &pObj->Motion, w->motIdle, 0, 0, 5, 0);
                }
                pObj->r_no_0 = 1;
                pObj->r_no_1 = 0;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            }
        }
    } else {
        RotMatrix(pObj->mat, &pObj->ang);
        TransMatrix(pObj->mat, &pObj->pos);
        ScaleMatrix(pObj->mat, &pObj->scale);
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// Installs the idle/crash motions (normal and big-scale variants) and starts the idle.
void cObj1c::setMotion(void* idle, void* crash, void* idleBig, void* crashBig)
{
    IslandWork* w = ISLAND_WK(this);

    w->motIdle = idle;
    w->motCrash = crash;
    w->motIdleBig = idleBig;
    w->motCrashBig = crashBig;
    if (scale.x >= 1.5f) {
        MotionSetCore(this, &Motion, idleBig, 0, 0, 5, 0);
    } else {
        MotionSetCore(this, &Motion, idle, 0, 0, 5, 0);
    }
}

// Plays the crash motion and the splash effect (15-frame effect cooldown).
void cObj1c::setCrash()
{
    IslandWork* w = ISLAND_WK(this);

    if (w->motCrash) {
        if (scale.x >= 1.5f) {
            MotionSetCore(this, &Motion, w->motCrashBig, 0, 0, 1, 0);
        } else {
            MotionSetCore(this, &Motion, w->motCrash, 0, 0, 1, 0);
        }
        r_no_0 = 1;
        r_no_2 = 0;
        r_no_1 = 1;
        r_no_3 = 0;
    }
    if (w->Eff_wait2 == 0) {
        w->Eff_wait2 = 15;
        EstSet(this, -1, 0, 0, EFF_ROOM, 1, 0, w->EffKindId, this, 0);
    }
}

// Big hit: pushes the island away from `from` at 300 units/frame, crash motion + splash, and opens
// the 15-frame crash window (ckCrash).
void cObj1c::setCrashBig(Vec* pPos)
{
    IslandWork* w = ISLAND_WK(this);
    Vec dir;

    PSVECSubtract(&pos, pPos, &dir);
    dir.y = 0.0f;
    if (dir.x == 0.0f && dir.z == 0.0f) {
        dir.z = 1.0f;
    } else {
#line 345 "D:/Bio4/Prog/obj1c.cpp"
        VECNormalize(&dir, &dir);
    }
    PSVECScale(&dir, &w->Spd, 300.0f);
    if (w->motCrash) {
        if (scale.x >= 1.5f) {
            MotionSetCore(this, &Motion, w->motCrashBig, 0, 0, 1, 0);
        } else {
            MotionSetCore(this, &Motion, w->motCrash, 0, 0, 1, 0);
        }
        r_no_0 = 1;
        r_no_1 = 2;
        r_no_2 = 0;
        r_no_3 = 0;
    }
    if (w->Eff_wait2 == 0) {
        w->Eff_wait2 = 15;
        EstSet(this, -1, 0, 0, EFF_ROOM, 1, 0, w->EffKindId, this, 0);
    }
    w->Crash_wait = 15;
}

// 1 during the 15 frames after a big crash (the room throws the player off).
int cObj1c::ckCrash()
{
    if (ISLAND_WK(this)->Crash_wait) {
        return 1;
    }
    return 0;
}

// Drift: with no push speed moves 50/frame towards home (outside 50 units); a push speed moves the
// island and decays by 10% per frame until below 50.
void obj1cSpdMove(cObj1c* pObj)
{
    IslandWork* w = ISLAND_WK(pObj);
    Vec d;

    if (w->Spd.x == 0.0f || w->Spd.z == 0.0f) {
        PSVECSubtract(&w->St_pos, &pObj->pos, &d);
        if (d.x * d.x + d.z * d.z > 2500.0f) {
#line 408 "D:/Bio4/Prog/obj1c.cpp"
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, 50.0f);
            PSVECAdd(&pObj->pos, &d, &pObj->pos);
        }
    } else {
        if (w->Spd.x * w->Spd.x + w->Spd.z * w->Spd.z < 2500.0f) {
            w->Spd.x = 0.0f;
            w->Spd.z = 0.0f;
        } else {
            PSVECAdd(&pObj->pos, &w->Spd, &pObj->pos);
            PSVECScale(&w->Spd, &w->Spd, 0.9f);
        }
    }
}
