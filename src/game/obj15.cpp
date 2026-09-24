// game/obj15: object id 0x15, the mounted gatling gun (D:/Bio4/Prog/obj15.cpp): a turret that an
// enemy rides (setRide); it turns towards `pTarget` (the player) limited by Rot_max, spins up (30
// frames) and fires every third frame (obj15GunHitck: line hit against the player with
// Obj15_atk_info_tbl damage, or a wall spark), 40 rounds per reload; three cEmHit boxes take
// weapon damage and break it (R1 1) unless Break_mode says otherwise; an optional pEat collision
// follows it.
#include "atari.h"
#include "light.h"
#include "obj.h"
#include "esp.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pad.h"
#include "quake.h"
#include "pl_wep.h"
#include "player.h"
#include "obj15.h"


extern "C" {
void obj15_R1_Set(cObjGatling* obj);
void obj15_R1_Break(cObjGatling* obj);
void obj15BarrelMove(cObjGatling* obj);
void obj15MatCalc(cObjGatling* obj);
int obj15GunHitck(cObjGatling* obj);
void obj15DmCk(cObjGatling* obj);
}

void (*Obj15_R1_move_tbl[2])(cObjGatling*) = { obj15_R1_Set, obj15_R1_Break };
EmAtkInfo Obj15_atk_info_tbl = { 100.0f, PL_DM_AUTO, 600, 0, 10, 0 };

// Creates the gatling at pos/rot with its three hit bodies, 40 rounds and no rider.
cObjGatling* SetObjGatling(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    GatlingWork* w;
    obj = ObjMgr.create(cObjMgr::ID_GATLING);
    if (obj == 0) {
        return 0;
    }
    w = GATLING_WK((cObjGatling*) obj);
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj15() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    w = GATLING_WK((cObjGatling*) obj);
    obj->atari.off();
    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    w->pEm = 0;
    w->Break_mode = 0;
    w->pTarget = 0;
    w->Se_on = 0;
    w->Fire_num = 40;
    w->Seid = 0;
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    obj->pos_old = obj->pos;
    if (rot) {
        obj->ang = *rot;
    } else {
        obj->ang.x = 0.0f;
        obj->ang.y = 0.0f;
        obj->ang.z = 0.0f;
    }
    w->St_dir = obj->ang.y;
    w->Rot_max = 3.1415927f;
    {
        Vec hpos;
        Vec hrot;

        hpos.x = 0.0f;
        hpos.y = 0.0f;
        hpos.z = 0.0f;
        hrot.x = 0.0f;
        hrot.y = 0.0f;
        hrot.z = 0.0f;
        w->pHit[0] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &hpos, &hrot, 1);
        if (w->pHit[0]) {
            w->pHit[0]->setParent(obj, 0, 0);
            YarareInitCube(w->pHit[0], 430.0f, 0.0f, 520.0f, 300.0f, 1600.0f, 50.0f, 1, YAT_FLAG_ON);
        }
        w->pHit[1] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &hpos, &hrot, 1);
        if (w->pHit[1]) {
            w->pHit[1]->setParent(obj, 0, 0);
            YarareInitCube(w->pHit[1], -430.0f, 0.0f, 520.0f, 300.0f, 1600.0f, 50.0f, 1, YAT_FLAG_ON);
        }
        w->pHit[2] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &hpos, &hrot, 1);
        if (w->pHit[2]) {
            w->pHit[2]->setParent(obj, 0, 0);
            YarareInitCube(w->pHit[2], 0.0f, 0.0f, 0.0f, 300.0f, 1600.0f, 300.0f, 1, YAT_FLAG_ON);
        }
    }
    w->pEat = 0;
    obj->r_no_0 = 1;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    return (cObjGatling*) obj;
}

// Per-frame: forgets a dead/removed rider, damage check, R1 routine, moves the eat collision with
// the gun, target timeout.
void cObjGatling::move()
{
    GatlingWork* w = GATLING_WK(this);

    if (w->pEm) {
        if (!w->pEm->isAlive()) {
            w->pEm = 0;
        }
        if (w->pEm) {
            if (w->pEm->hp <= 0) {
                w->pEm = 0;
            }
        }
    }
    obj15DmCk(this);
    Obj15_R1_move_tbl[r_no_1](this);
    if (w->pEat) {
        if (be_flag & 2) {
            w->pEat->setEnable();
            w->pEat->setCoord(&pos, &ang);
        } else {
            w->pEat->setDisable();
        }
    }
    if (w->Heli_lock_timer) {
        w->Heli_lock_timer--;
        if (w->Heli_lock_timer == 0) {
            w->pTarget = 0;
        }
    }
}

// Rno1 == 0 (working): Rno2 0 idle until a rider fires, 1 turning towards the target (turn rate by
// distance, faster during the first 14 frames) and firing when spun up, 2 a 30-frame pause when
// out of ammo / rider gone / fire stopped, then back to 0. Fires only while the player is alive.
void obj15_R1_Set(cObjGatling* pObj)
{
    GatlingWork* w = GATLING_WK(pObj);
    f32 dist;
    f32 lim;
    f32 ang;

    if (w->pTarget == 0) {
        w->pTarget = pPL;
    }
    switch (pObj->r_no_2) {
    case 0:
        w->Fire_timer = 0;
        w->Fire_go = 0;
        if (w->pEm == 0) {
            break;
        }
        if (w->Fire_ready == 0) {
            break;
        }
        {
            Vec tpos;

            pObj->getPartsPtr(2);
            tpos = w->pTarget->pos;
            w->Fire_ready = 0;
            w->Fire_go = 1;
            w->Fire_timer = 0;
            tpos.y += 2000.0f;
        }
        pObj->r_no_2++;
    case 1:
        dist = VEC_DISTXZ(&pObj->pos, &w->pTarget->pos);
        if (dist < 5000.0f) {
            dist = 5000.0f;
        }
        dist *= 0.0002f;
        if (w->Fire_timer <= 14) {
            lim = 1.0f / dist * 0.03926991f;
        } else {
            lim = 1.0f / dist * 0.019634955f;
        }
        ang = LIMIT_ANGLE(Muku(&pObj->pos, &w->pTarget->pos, pObj->ang.y, lim) + pObj->ang.y);
        pObj->ang.y = w->St_dir + Muku2(w->St_dir, ang, w->Rot_max);
        pObj->ang.y = LIMIT_ANGLE(pObj->ang.y);
        obj15BarrelMove(pObj);
        if (w->Fire_num == 0 || w->pEm == 0 || w->Fire_go == 0) {
            pObj->r_no_2++;
        }
        break;
    case 2:
        w->Timer = 30;
        pObj->r_no_2++;
    case 3:
        if (w->Timer) {
            w->Timer--;
            obj15BarrelMove(pObj);
            pObj->r_no_2 = 0;
        }
        break;
    }
    obj15MatCalc(pObj);
    if (w->Fire_go) {
        if ((s16) pG->pl_life > 0) {
            w->Fire_timer++;
            if (w->Fire_timer > 30) {
                if (w->Fire_timer % 3 == 0) {
                    if (w->Fire_num) {
                        w->Fire_num--;
                        if (obj15GunHitck(pObj)) {
                            if (w->Fire_num > 10) {
                                w->Fire_num = 10;
                            }
                        }
                    }
                }
            }
        }
    }
}


// Rno1 == 1 (broken): once spawns the explosion (est 1/0xD) and does the break work.
void obj15_R1_Break(cObjGatling* pObj)
{
    GatlingWork* w = GATLING_WK(pObj);
    u32 i;

    if (pObj->r_no_2 == 0) {
        EstSet(0, -1, &pObj->pos, &pObj->ang, EFF_ROOM, 0xD, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndStop(w->Seid, 0);
        for (i = 0; i < 3; i++) {
                if (w->pHit[i]) {
                        w->pHit[i]->hp = 0;
                        w->pHit[i] = 0;
                }
        }
        pObj->be_flag &= ~2;
        if (w->pEat) {
                w->pEat->setDisable();
        }
        pObj->r_no_2++;
    }
}


// Aims the barrel pitch at the target (+1400 y) with 10% easing and spins the barrel parts while
// firing (with the spin sound); stops the sound when not firing.
void obj15BarrelMove(cObjGatling* pObj)
{
    GatlingWork* w = GATLING_WK(pObj);
    Vec tpos;
    cCoord* parts;

    if (w->pTarget == 0) {
        w->pTarget = pPL;
    }
    tpos = w->pTarget->pos;
    parts = w->pEm;
    tpos.y += 1400.0f;
    if (parts) {
        Vec d;
        f32 len;
        f32 ang;

        parts = pObj->getPartsPtr(2);
        PSVECSubtract(&tpos, &parts->world, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        ang = -atan2f(d.y, len);
        parts->ang.x = parts->ang.x * 0.9f + ang * 0.1f;
        if (w->Fire_go && (s16) pG->pl_life > 0) {
            parts = pObj->getPartsPtr(3);
            parts->ang.z += 0.20943952f;
            parts->ang.z = LIMIT_ANGLE(parts->ang.z);
            if (w->Se_on == 0) {
                w->Se_on = 1;
                w->Seid = SndCall(6, 0x24, &pObj->pos, 0, 0, 0);
            }
        } else {
            if (w->Se_on) {
                SndStop(w->Seid, 0);
                SndCall(6, 0x25, &pObj->pos, 0, 0, 0);
            }
            w->Se_on = 0;
        }
    } else {
        if (w->Se_on) {
            SndStop(w->Seid, 0);
            SndCall(6, 0x25, &pObj->pos, 0, 0, 0);
        }
        w->Se_on = 0;
    }
}

// Rebuilds the gun matrix and parts.
void obj15MatCalc(cObjGatling* pObj)
{
    GatlingWork* w = GATLING_WK(pObj);

    if (w->pTarget == 0) {
        w->pTarget = pPL;
    }
    pObj->matCalc();
    if (pObj->Motion.pMot == 0) {
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// One shot: muzzle effect and sound, a line from the muzzle 50000 units ahead with +-5000 random
// spread: on the player it applies Obj15_atk_info_tbl damage, blood, vibration and quake and
// returns 1; otherwise a spark/hit effect and tracer where it hits the scenario, returns 0.
int obj15GunHitck(cObjGatling* pObj)
{
    Vec ofs;
    Vec mzl;
    EmAtkInfo info;
    Vec hit;
    Vec dir;
    cEm* em;
    cParts* parts;
    u32 attr;

    EstSet(pObj, -1, 0, 0, EFF_ROOM, 0x1F, 0, ESP_CORE_KIND_NONE, pObj, 0);
    SndCall(6, 9, &pObj->pos, 0, 0, 0);
    ofs.x = 0.0f;
    ofs.y = 0.0f;
    ofs.z = 1000.0f;
    mzl.x = fRand1_1() * 5000.0f;
    mzl.y = fRand1_1() * 5000.0f;
    mzl.z = 50000.0f;
    parts = pObj->getPartsPtr(2);
    PSMTXMultVec(parts->mat, &ofs, &ofs);
    PSMTXMultVec(parts->mat, &mzl, &mzl);
    PlWepHitCheck2(0, &ofs, &mzl, 0xC, 3, 6000.0f);
    em = EmAtkLineHitCk(&ofs, &mzl, &hit, &dir, &attr);
    if (em == 0) {
        int eff = 0;

        if (EatGetEffectType(attr)) {
            eff = 1;
        }
        if (G_ROOM_ID == 0x320) {
            eff = 1;
        }
        if (eff) {
            Vec sc;
            Vec erot;
            Vec d;
            f32 len;

            len = SQRTF(dir.x * dir.x + dir.z * dir.z);
            erot.x = -atan2f(dir.y, len);
            erot.y = atan2f(dir.x, dir.z);
            erot.z = 0.0f;
            PSVECScale(&dir, &sc, 30.0f);
            PSVECAdd(&hit, &sc, &hit);
            EstSet(0, -1, &hit, &erot, EFF_ROOM, 0x1E, 0, ESP_CORE_KIND_NONE, 0, 0);
            PSVECSubtract(&hit, &ofs, &d);
            EspSetGatling(ofs, d);
            SndCall(6, 0xA, &hit, 0, 0, 0);
        }
        return 0;
    }
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
    SndCall(6, 0x15, &pPL->pos, 0, 0, 0);
    QuakeExec(0, 0, 5, 22.0f, 2);
    EmPlBloodSet2(pObj, &pObj->pos, 1, 1, 0x1C);
    info = Obj15_atk_info_tbl;
    EmAtkSetDamagePL(em, &info, &ofs, &mzl);
    return 1;
}

// Debug: straight shot line (dead-stripped).
// Never called (dead-stripped by the original linker, STRIP_UNUSED): its constant pool survives
// in .rodata (0.0, 150.0, 1000.0, 50000.0, 1.0).
static void obj15GunHitckDbg(cObjGatling* obj)
{
    Vec ofs;
    Vec mzl;
    cParts* parts;

    ofs.x = 0.0f;
    ofs.y = 150.0f;
    ofs.z = 1000.0f;
    mzl.x = 0.0f;
    mzl.y = 0.0f;
    mzl.z = 50000.0f;
    parts = obj->getPartsPtr(2);
    PSMTXMultVec(parts->mat, &ofs, &ofs);
    PSMTXMultVec(parts->mat, &mzl, &mzl);
    PlWepHitCheck2(0, &ofs, &mzl, 0xC, 3, 1.0f);
}

// Sets the enemy operating the gun.
void cObjGatling::setRide(cEm* pEm)
{
    GATLING_WK(this)->pEm = pEm;
}

// Rider request: start firing.
void cObjGatling::setFire()
{
    GATLING_WK(this)->Fire_ready = 1;
}

// Rider request: stop firing.
void cObjGatling::stopFire()
{
    GATLING_WK(this)->Fire_go = 0;
}

// 1 when the gun is empty.
int cObjGatling::ckReload()
{
    return GATLING_WK(this)->Fire_num == 0;
}

// Refills 40 rounds.
void cObjGatling::setReload()
{
    GATLING_WK(this)->Fire_num = 40;
}

// Weapon hits on the three hit bodies (only when breakable, stat 0x0101): spark effects; Break_mode
// 0 breaks the gun (R1 1) with a sound.
void obj15DmCk(cObjGatling* pObj)
{
    GatlingWork* w = GATLING_WK(pObj);
    u32 i;

    if (pObj->r_no_0 == 1 && pObj->r_no_1 == 1) {
        return;
    }
    for (i = 0; i < 3; i++) {
        if (w->pHit[i]) {
            switch ((u32) w->pHit[i]->ckDmgWeapon()) {
            default:
                EmDmBloodSet2(w->pHit[i], 1, 0x1D, 0, 0, 0);
                break;
            case 0:
                break;
            case 0xD:
            case 0x12:
                if (w->Break_mode == 0) {
                    pObj->r_no_0 = 1;
                    pObj->r_no_1 = 1;
                    pObj->r_no_2 = 0;
                    pObj->r_no_3 = 0;
                    return;
                }
                SndCall(6, 0x16, &pObj->pos, 0, 0, 0);
                EmDmBloodSet2(w->pHit[i], 1, 0x1D, 0, 0, 0);
                break;
            }
        }
    }
}

// Creates the eat collision that follows the gun.
void cObjGatling::setEat(void* data, int type)
{
    GatlingWork* w = GATLING_WK(this);

    w->pEat = EatMgr.create(data, 0, &pos, &ang, type);
}

// Max yaw away from the rest angle (radians).
void cObjGatling::setMaxRot(f32 rot_max)
{
    GATLING_WK(this)->Rot_max = rot_max;
}

// 1 when the gun is flagged breakable/broken (stat high half 0x0101).
int cObjGatling::ckBreak()
{
    return (r_no_0 == 1 && r_no_1 == 1);
}

// 0 = weapon hits break it, else only setBreak does.
void cObjGatling::setBreakMode(u8 mode)
{
    GATLING_WK(this)->Break_mode = mode;
}

// Breaks the gun from the room script (break work, R1 1 without the explosion effect).
void cObjGatling::setBreak()
{
    GatlingWork* w = GATLING_WK(this);
    u32 i;

    if (r_no_0 == 1 && r_no_1 == 1) {
        return;
    }
    SndStop(w->Seid, 0);
    for (i = 0; i < 3; i++) {
        if (w->pHit[i]) {
            w->pHit[i]->hp = 0;
            w->pHit[i] = 0;
        }
    }
    be_flag &= ~2;
    if (w->pEat) {
        w->pEat->setDisable();
    }
    r_no_0 = 1;
    r_no_1 = 1;
    r_no_2 = 1;
    r_no_3 = 0;
}
