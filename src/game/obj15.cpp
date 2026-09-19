// game/obj15: object id 0x15, the mounted gatling gun (D:/Bio4/Prog/obj15.cpp): a turret that an
// enemy rides (setRide); it turns towards `target` (the player) limited by maxRot, spins up (30
// frames) and fires every third frame (obj15GunHitck: line hit against the player with
// Obj15_atk_info_tbl damage, or a wall spark), 40 rounds per reload; three cEmHit boxes take
// weapon damage and break it (R1 1) unless breakMode says otherwise; an optional eat collision
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

// Mounted gatling gun: aims at `target` (the player unless an enemy rides it), fires every third
// frame once spun up, takes weapon damage on three cEmHit boxes and breaks (R1_Break).
class cObjGatling : public cObj {
public:
    virtual void move();
    virtual ~cObjGatling() {}

    void setRide(cEm* em);
    void setFire();
    void stopFire();
    int ckReload();
    void setReload();
    void setEat(void* data, int type);
    void setMaxRot(f32 r);
    int ckBreak();
    void setBreakMode(u8 mode);
    void setBreak();
};

extern "C" {
void obj15_R1_Set(cObjGatling* obj);
void obj15_R1_Break(cObjGatling* obj);
void obj15BarrelMove(cObjGatling* obj);
void obj15MatCalc(cObjGatling* obj);
int obj15GunHitck(cObjGatling* obj);
void obj15DmCk(cObjGatling* obj);
void EspSetGatling(Vec pos, Vec dir);
}

void (*Obj15_R1_move_tbl[2])(cObjGatling*) = { obj15_R1_Set, obj15_R1_Break };
EmAtkInfo Obj15_atk_info_tbl = { 100.0f, 8, 600, 0, 10, 0 };

// Creates the gatling at pos/rot with its three hit bodies, 40 rounds and no rider.
cObj* SetObjGatling(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    GatlingWork* w;
    obj = ObjMgr.create(0x15);
    if (obj == 0) {
        return 0;
    }
    w = &obj->gatling;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj15() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    w = &obj->gatling;
    obj->sub2B4.atari.throughOn();
    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    w->ride = 0;
    w->breakMode = 0;
    w->target = 0;
    w->seOn = 0;
    w->ammo = 40;
    w->seHandle = 0;
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
    FSet(w->rotY, obj->ang.y);
    FSet(w->maxRot, 3.1415927f);
    {
        Vec hpos;
        Vec hrot;

        hpos.x = 0.0f;
        hpos.y = 0.0f;
        hpos.z = 0.0f;
        hrot.x = 0.0f;
        hrot.y = 0.0f;
        hrot.z = 0.0f;
        w->hit[0] = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &hpos, &hrot, 1);
        if (w->hit[0]) {
            w->hit[0]->setParent(obj, 0, 0);
            YarareInitCube(w->hit[0], 430.0f, 0.0f, 520.0f, 300.0f, 1600.0f, 50.0f, 1, 1);
        }
        w->hit[1] = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &hpos, &hrot, 1);
        if (w->hit[1]) {
            w->hit[1]->setParent(obj, 0, 0);
            YarareInitCube(w->hit[1], -430.0f, 0.0f, 520.0f, 300.0f, 1600.0f, 50.0f, 1, 1);
        }
        w->hit[2] = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &hpos, &hrot, 1);
        if (w->hit[2]) {
            w->hit[2]->setParent(obj, 0, 0);
            YarareInitCube(w->hit[2], 0.0f, 0.0f, 0.0f, 300.0f, 1600.0f, 300.0f, 1, 1);
        }
    }
    w->eat = 0;
    obj->r_no_0 = 1;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    return obj;
}

// Per-frame: forgets a dead/removed rider, damage check, R1 routine, moves the eat collision with
// the gun, target timeout.
void cObjGatling::move()
{
    GatlingWork* w = &gatling;

    if (w->ride) {
        if ((w->ride->be_flag & 0x201) != 1) {
            w->ride = 0;
        }
        if (w->ride) {
            if (w->ride->hp <= 0) {
                w->ride = 0;
            }
        }
    }
    obj15DmCk(this);
    Obj15_R1_move_tbl[r_no_1](this);
    if (w->eat) {
        if (be_flag & 2) {
            w->eat->m_Flag |= 4;
            w->eat->setCoord(&pos, &ang);
        } else {
            w->eat->m_Flag &= ~4;
        }
    }
    if (w->targetTimer) {
        w->targetTimer--;
        if (w->targetTimer == 0) {
            w->target = 0;
        }
    }
}

// Rno1 == 0 (working): Rno2 0 idle until a rider fires, 1 turning towards the target (turn rate by
// distance, faster during the first 14 frames) and firing when spun up, 2 a 30-frame pause when
// out of ammo / rider gone / fire stopped, then back to 0. Fires only while the player is alive.
void obj15_R1_Set(cObjGatling* obj)
{
    GatlingWork* w = &obj->gatling;
    f32 dist;
    f32 lim;
    f32 ang;

    if (w->target == 0) {
        w->target = pPL;
    }
    switch (obj->r_no_2) {
    case 0:
        w->cnt = 0;
        w->firing = 0;
        if (w->ride == 0) {
            break;
        }
        if (w->fire == 0) {
            break;
        }
        {
            Vec tpos;

            obj->getPartsPtr(2);
            tpos = w->target->pos;
            w->fire = 0;
            w->firing = 1;
            w->cnt = 0;
            tpos.y += 2000.0f;
        }
        obj->r_no_2++;
    case 1:
        dist = SQRTF((obj->pos.x - w->target->pos.x) * (obj->pos.x - w->target->pos.x) +
                     (obj->pos.z - w->target->pos.z) * (obj->pos.z - w->target->pos.z));
        if (dist < 5000.0f) {
            dist = 5000.0f;
        }
        dist *= 0.0002f;
        if (w->cnt <= 14) {
            lim = 1.0f / dist * 0.03926991f;
        } else {
            lim = 1.0f / dist * 0.019634955f;
        }
        ang = LIMIT_ANGLE(Muku(&obj->pos, &w->target->pos, obj->ang.y, lim) + obj->ang.y);
        obj->ang.y = w->rotY + Muku2(w->rotY, ang, w->maxRot);
        obj->ang.y = LIMIT_ANGLE(obj->ang.y);
        obj15BarrelMove(obj);
        if (w->ammo == 0 || w->ride == 0 || w->firing == 0) {
            obj->r_no_2++;
        }
        break;
    case 2:
        w->breakTimer = 30;
        obj->r_no_2++;
    case 3:
        if (w->breakTimer) {
            w->breakTimer--;
            obj15BarrelMove(obj);
            obj->r_no_2 = 0;
        }
        break;
    }
    obj15MatCalc(obj);
    if (w->firing) {
        if ((s16) pG->pl_life > 0) {
            w->cnt++;
            if (w->cnt > 30) {
                if (w->cnt % 3 == 0) {
                    if (w->ammo) {
                        w->ammo--;
                        if (obj15GunHitck(obj)) {
                            if (w->ammo > 10) {
                                w->ammo = 10;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Shared break work: stops the spin sound, kills the hit bodies, hides the gun, releases the eat.
static inline void obj15BreakCommon(cObjGatling* obj)
{
    GatlingWork* w = &obj->gatling;
    u32 i;

    SndStop(w->seHandle, 0);
    for (i = 0; i < 3; i++) {
        if (w->hit[i]) {
            w->hit[i]->hp = 0;
            w->hit[i] = 0;
        }
    }
    obj->be_flag &= ~2;
    if (w->eat) {
        w->eat->m_Flag &= ~4;
    }
}

// Rno1 == 1 (broken): once spawns the explosion (est 1/0xD) and does the break work.
void obj15_R1_Break(cObjGatling* obj)
{
    GatlingWork* w = &obj->gatling;
    u32 i;

    if (obj->r_no_2 == 0) {
        EstSet(0, -1, &obj->pos, &obj->ang, 1, 0xD, 0, 0, 0, 0);
        SndStop(w->seHandle, 0);
        for (i = 0; i < 3; i++) {
                if (w->hit[i]) {
                        w->hit[i]->hp = 0;
                        w->hit[i] = 0;
                }
        }
        obj->be_flag &= ~2;
        if (w->eat) {
                w->eat->m_Flag &= ~4;
        }
        obj->r_no_2++;
    }
}

// Stops the spin-up loop and plays the spin-down sound.
static inline void obj15SeStop(cObjGatling* obj)
{
    GatlingWork* w = &obj->gatling;

    if (w->seOn) {
        SndStop(w->seHandle, 0);
        SndCall(6, 0x25, &obj->pos, 0, 0, 0);
    }
    w->seOn = 0;
}

// Aims the barrel pitch at the target (+1400 y) with 10% easing and spins the barrel parts while
// firing (with the spin sound); stops the sound when not firing.
void obj15BarrelMove(cObjGatling* obj)
{
    GatlingWork* w = &obj->gatling;
    Vec tpos;
    cModel* parts;

    if (w->target == 0) {
        w->target = pPL;
    }
    tpos = w->target->pos;
    parts = w->ride;
    tpos.y += 1400.0f;
    if (parts) {
        Vec d;
        f32 len;
        f32 ang;

        parts = obj->getPartsPtr(2);
        PSVECSubtract(&tpos, &parts->world, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        ang = -atan2f(d.y, len);
        parts->ang.x = parts->ang.x * 0.9f + ang * 0.1f;
        if (w->firing && (s16) pG->pl_life > 0) {
            parts = obj->getPartsPtr(3);
            parts->ang.z += 0.20943952f;
            parts->ang.z = LIMIT_ANGLE(parts->ang.z);
            if (w->seOn == 0) {
                w->seOn = 1;
                w->seHandle = SndCall(6, 0x24, &obj->pos, 0, 0, 0);
            }
        } else {
            if (w->seOn) {
                SndStop(w->seHandle, 0);
                SndCall(6, 0x25, &obj->pos, 0, 0, 0);
            }
            w->seOn = 0;
        }
    } else {
        if (w->seOn) {
            SndStop(w->seHandle, 0);
            SndCall(6, 0x25, &obj->pos, 0, 0, 0);
        }
        w->seOn = 0;
    }
}

// Rebuilds the gun matrix and parts.
void obj15MatCalc(cObjGatling* obj)
{
    GatlingWork* w = &obj->gatling;

    if (w->target == 0) {
        w->target = pPL;
    }
    RotMatrix(obj->l_mat, &obj->ang);
    TransMatrix(obj->l_mat, &obj->pos);
    ScaleMatrix(obj->l_mat, &obj->scale);
    PSMTXCopy(obj->l_mat, obj->mat);
    if (obj->pMotion == 0) {
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

// One shot: muzzle effect and sound, a line from the muzzle 50000 units ahead with +-5000 random
// spread: on the player it applies Obj15_atk_info_tbl damage, blood, vibration and quake and
// returns 1; otherwise a spark/hit effect and tracer where it hits the scenario, returns 0.
int obj15GunHitck(cObjGatling* obj)
{
    Vec ofs;
    Vec mzl;
    EmAtkInfo info;
    Vec hit;
    Vec dir;
    cEm* em;
    cModel* parts;
    u32 attr;

    EstSet((int) obj, -1, 0, 0, 1, 0x1F, 0, 0, (u32) obj, 0);
    SndCall(6, 9, &obj->pos, 0, 0, 0);
    ofs.x = 0.0f;
    ofs.y = 0.0f;
    ofs.z = 1000.0f;
    mzl.x = fRand1_1() * 5000.0f;
    mzl.y = fRand1_1() * 5000.0f;
    mzl.z = 50000.0f;
    parts = obj->getPartsPtr(2);
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
            EstSet(0, -1, &hit, &erot, 1, 0x1E, 0, 0, 0, 0);
            PSVECSubtract(&hit, &ofs, &d);
            EspSetGatling(ofs, d);
            SndCall(6, 0xA, &hit, 0, 0, 0);
        }
        return 0;
    }
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    SndCall(6, 0x15, &pPL->pos, 0, 0, 0);
    QuakeExec(0, 0, 5, 22.0f, 2);
    EmPlBloodSet2(obj, &obj->pos, 1, 1, 0x1C);
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
    cModel* parts;

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
void cObjGatling::setRide(cEm* em)
{
    gatling.ride = em;
}

// Rider request: start firing.
void cObjGatling::setFire()
{
    gatling.fire = 1;
}

// Rider request: stop firing.
void cObjGatling::stopFire()
{
    gatling.firing = 0;
}

// 1 when the gun is empty.
int cObjGatling::ckReload()
{
    return gatling.ammo == 0;
}

// Refills 40 rounds.
void cObjGatling::setReload()
{
    gatling.ammo = 40;
}

// Weapon hits on the three hit bodies (only when breakable, stat 0x0101): spark effects; breakMode
// 0 breaks the gun (R1 1) with a sound.
void obj15DmCk(cObjGatling* obj)
{
    GatlingWork* w = &obj->gatling;
    u32 i;

    if (obj->r_no_0 == 1 && obj->r_no_1 == 1) {
        return;
    }
    for (i = 0; i < 3; i++) {
        if (w->hit[i]) {
            switch ((u32) w->hit[i]->ckDmgWeapon()) {
            default:
                EmDmBloodSet2(w->hit[i], 1, 0x1D, 0, 0, 0);
                break;
            case 0:
                break;
            case 0xD:
            case 0x12:
                if (w->breakMode == 0) {
                    obj->r_no_0 = 1;
                    obj->r_no_1 = 1;
                    obj->r_no_2 = 0;
                    obj->r_no_3 = 0;
                    return;
                }
                SndCall(6, 0x16, &obj->pos, 0, 0, 0);
                EmDmBloodSet2(w->hit[i], 1, 0x1D, 0, 0, 0);
                break;
            }
        }
    }
}

// Creates the eat collision that follows the gun.
void cObjGatling::setEat(void* data, int type)
{
    GatlingWork* w = &gatling;

    w->eat = EatMgr.create(data, 0, &pos, &ang, type);
}

// Max yaw away from the rest angle (radians).
void cObjGatling::setMaxRot(f32 r)
{
    gatling.maxRot = r;
}

// 1 when the gun is flagged breakable/broken (stat high half 0x0101).
int cObjGatling::ckBreak()
{
    return (r_no_0 == 1 && r_no_1 == 1);
}

// 0 = weapon hits break it, else only setBreak does.
void cObjGatling::setBreakMode(u8 mode)
{
    gatling.breakMode = mode;
}

// Breaks the gun from the room script (break work, R1 1 without the explosion effect).
void cObjGatling::setBreak()
{
    GatlingWork* w = &gatling;
    u32 i;

    if (r_no_0 == 1 && r_no_1 == 1) {
        return;
    }
    SndStop(w->seHandle, 0);
    for (i = 0; i < 3; i++) {
        if (w->hit[i]) {
            w->hit[i]->hp = 0;
            w->hit[i] = 0;
        }
    }
    be_flag &= ~2;
    if (w->eat) {
        w->eat->m_Flag &= ~4;
    }
    r_no_0 = 1;
    r_no_1 = 1;
    r_no_2 = 1;
    r_no_3 = 0;
}
