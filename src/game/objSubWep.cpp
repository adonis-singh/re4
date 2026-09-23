#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "objSubWep.h"
#include "esp.h"
#include "est.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pl_wep.h"
#include "player.h"

// Player sub weapons: the thrown hand grenade (cObjGrenade), incendiary grenade (cObjGreFire),
// flash grenade (cObjGreLight) and the egg (cObjEgg) share cSubWep's flight, bounce and water
// handling; each supplies its explosion.

class cObjGrenade : public cSubWep {
public:
    cObjGrenade();
    virtual ~cObjGrenade() {}
    virtual void explode();
    virtual void waterExplode();
};

class cObjGreFire : public cSubWep {
public:
    cObjGreFire();
    virtual ~cObjGreFire() {}
    virtual void explode();
    virtual void waterExplode();
};

class cObjGreLight : public cSubWep {
public:
    cObjGreLight();
    virtual ~cObjGreLight() {}
    virtual void explode();
    virtual void waterExplode();
};

class cObjEgg : public cSubWep {
public:
    cObjEgg();
    virtual ~cObjEgg() {}
    virtual void explode();
    virtual void waterExplode();
};

extern "C" {
void setThrowSpeed(Vec* spd, f32 power);
}

// r_no_0 0: flying / bouncing (moveNormal), 1: sunk in water, waiting to blow (moveWater).
void cSubWep::move()
{
    static void (cSubWep::*funcTbl[2])() = { &cSubWep::moveNormal, &cSubWep::moveWater };

    (this->*funcTbl[r_no_0])();
}

// Flight frame: counts SUBWEP_WK(this)->life down (a timed grenade, type 0, explodes when it reaches 0;
// the others are only destroyed), moves by the speed / bounces (addSpeed) and spins parts 0 by
// rotSpd.
void cSubWep::moveNormal()
{

    if (SUBWEP_WK(this)->life >= 0) {
        if (SUBWEP_WK(this)->life > 0) {
            SUBWEP_WK(this)->life--;
        } else {
            if (type == 0) {
                scrAdjust();
                explode();
            }
            ObjMgr.destroy(this);
            return;
        }
    }
    addSpeed();
    {
        cModel* parts = getPartsPtr(0);
        if (parts) {
            PSVECAdd(&parts->ang, &SUBWEP_WK(this)->rotSpd, &parts->ang);
            parts->ang.x = LIMIT_ANGLE(parts->ang.x);
            parts->ang.y = LIMIT_ANGLE(parts->ang.y);
            parts->ang.z = LIMIT_ANGLE(parts->ang.z);
            RotMatrix(parts->l_mat, &parts->ang);
            TransMatrix(parts->l_mat, &parts->pos);
            ScaleMatrix(parts->l_mat, &parts->scale);
        }
    }
    matUpdate();
}

// Under water: when life runs out plays the water-surface effect chosen at entry (effNo/effPrm;
// 0xD2/1 = none) plus the 0x28 splash when nothing is above the surface, then waterExplode().
void cSubWep::moveWater()
{

    SUBWEP_WK(this)->life--;
    if (SUBWEP_WK(this)->life > 0) {
        return;
    }
    if (!(SUBWEP_WK(this)->effNo == 0xD2 && SUBWEP_WK(this)->effPrm == 1)) {
        EstSet(0, -1, &pos, 0, SUBWEP_WK(this)->effNo, SUBWEP_WK(this)->effPrm, 0, ESP_CORE_KIND_NONE, 0, 0);
        if (SUBWEP_WK(this)->effNo == 0 && SUBWEP_WK(this)->effPrm == 0x15) {
            Vec a;
            Vec b;
            Vec nrm;

            a.x = pos.x;
            a.y = pos.y - 300.0f;
            a.z = pos.z;
            b.x = pos.x;
            b.y = pos.y + 300.0f;
            b.z = pos.z;
            if (EatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) == 0 || nrm.y > 0.9f) {
                EstSet(0, -1, &pos, 0, EFF_CORE, 0x28, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
    }
    AddWaterPower(pos, 1.0f);
    waterExplode();
    ObjMgr.destroy(this);
}

// Before exploding, pushes `pos` 400 units away from any wall within 400 units in the four
// horizontal directions (measured 300 above the grenade), so the blast is not inside a wall.
void cSubWep::scrAdjust()
{
    Vec p;
    Vec a;
    Vec hit;
    Vec nrm;
    Vec n;
    const f32 ofs = 400.0f;

    p.x = 0.0f;
    p.y = 300.0f;
    p.z = 0.0f;
    PSMTXMultVec(mat, &p, &p);
    a.x = p.x + ofs;
    a.y = p.y;
    a.z = p.z;
    if (EatMgr.hitCheck(&p, &a, &hit, &n, 0, 0)) {
        nrm.x = n.x;
        nrm.y = 0.0f;
        nrm.z = n.z;
        if (!(nrm.x == 0.0f && nrm.z == 0.0f)) {
#line 170 "D:/Bio4/Prog/objSubWep.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, ofs);
            PSVECAdd(&hit, &nrm, &pos);
        }
    }
    a.x = p.x - ofs;
    a.y = p.y;
    a.z = p.z;
    if (EatMgr.hitCheck(&p, &a, &hit, &n, 0, 0)) {
        nrm.x = n.x;
        nrm.y = 0.0f;
        nrm.z = n.z;
        if (!(nrm.x == 0.0f && nrm.z == 0.0f)) {
#line 180 "D:/Bio4/Prog/objSubWep.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, ofs);
            PSVECAdd(&hit, &nrm, &pos);
        }
    }
    a.x = p.x;
    a.y = p.y;
    a.z = p.z + ofs;
    if (EatMgr.hitCheck(&p, &a, &hit, &n, 0, 0)) {
        nrm.x = n.x;
        nrm.y = 0.0f;
        nrm.z = n.z;
        if (!(nrm.x == 0.0f && nrm.z == 0.0f)) {
#line 190 "D:/Bio4/Prog/objSubWep.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, ofs);
            PSVECAdd(&hit, &nrm, &pos);
        }
    }
    a.x = p.x;
    a.y = p.y;
    a.z = p.z - ofs;
    if (EatMgr.hitCheck(&p, &a, &hit, &n, 0, 0)) {
        nrm.x = n.x;
        nrm.y = 0.0f;
        nrm.z = n.z;
        if (!(nrm.x == 0.0f && nrm.z == 0.0f)) {
#line 200 "D:/Bio4/Prog/objSubWep.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, ofs);
            PSVECAdd(&hit, &nrm, &pos);
        }
    }
}

// Damage area for the explosion: kind 1 (incendiary) = DmgMgr type 1 for 75 frames, radius 2500 x
// 1500; kind 8 = types 2 and 8 (unused here).
void cSubWep::dmgSet(int dmtype)
{
    switch (dmtype) {
    case 8:
        DmgMgr.set(DMG_TYPE_GRENADE_BLAST, 2, &pos, 3000.0f, 3000.0f);
        DmgMgr.set(DMG_TYPE_GRENADE, 2, &pos, 15000.0f, 3000.0f);
        break;
    case 1:
        DmgMgr.set(DMG_TYPE_FIRE, 75, &pos, 2500.0f, 1500.0f);
        break;
    }
}

// One frame of ballistics: gravity (grav / frame), vehicle adjust, then water (GetWaterHeight:
// water effect / SE, eggs (type >= 3) vanish, a hand grenade sinks to r_no_0 1, fire / flash blow
// at once) or the effect collision (EatMgr.adjust with radius rad/2): a surface whose AtEffInfo
// flag bit0 is set counts as water, everything else bounces.
void cSubWep::addSpeed()
{
    Vec old;
    Vec hit;
    Vec nrm;
    f32 wh;
    AtEffInfo* info;

    VehicleAdjust(&pos);
    old = pos;
    SUBWEP_WK(this)->spd.y -= SUBWEP_WK(this)->grav;
    PSVECAdd(&pos, &SUBWEP_WK(this)->spd, &pos);
    EatMgr.hitCheck(&old, &pos, &hit, 0, 0, 0x4000);
    if (GetWaterHeight(&pos, &wh) && pos.y <= wh && hit.y < wh) {
        pos.y = wh + 20.0f;
        info = EatMgr.getEffInfo(EAT_ET_WATER);
        if (info == 0) {
            pLog->err(0, 0, "GRENADE CANT FOUND WATER INFORMATION");
            pLog->err(0, 0, "  PLEASE SET EatMgr.registEffInfo()");
            return;
        }
        SUBWEP_WK(this)->attr = info->flag;
        switch (type) {
        case 0:
        default:
            SUBWEP_WK(this)->effNo = info->eff13[0];
            SUBWEP_WK(this)->effPrm = info->eff13[1];
            break;
        case 1:
            SUBWEP_WK(this)->effNo = info->eff16[0];
            SUBWEP_WK(this)->effPrm = info->eff16[1];
            break;
        case 2:
            SUBWEP_WK(this)->effNo = info->eff17[0];
            SUBWEP_WK(this)->effPrm = info->eff17[1];
            break;
        case 3:
            SUBWEP_WK(this)->effNo = info->eff17[0];
            SUBWEP_WK(this)->effPrm = info->eff17[1];
            break;
        case 4:
            SUBWEP_WK(this)->effNo = info->eff17[0];
            SUBWEP_WK(this)->effPrm = info->eff17[1];
            break;
        case 5:
            SUBWEP_WK(this)->effNo = info->eff17[0];
            SUBWEP_WK(this)->effPrm = info->eff17[1];
            break;
        }
        if (type > 2) {
            if (info->eff0[0] != 0xD2) {
                EstSet(0, -1, &pos, 0, info->eff0[0], (u8) info->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
            } else if (info->eff0[1] != 1) {
                EstSet(0, -1, &pos, 0, EFF_CORE, 0x3A, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
            SndCall(5, 0x24, &pos, 0, 0, 0);
            AddWaterPower(pos, 0.5f);
            ObjMgr.destroy(this);
        } else if (type == 0) {
            if (info->eff0[0] != 0xD2) {
                EstSet(0, -1, &pos, 0, info->eff0[0], (u8) info->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
            } else if (info->eff0[1] != 1) {
                EstSet(0, -1, &pos, 0, EFF_CORE, 0x3A, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
            SndCall(5, 0x24, &pos, 0, 0, 0);
            AddWaterPower(pos, 0.5f);
            r_no_0 = 1;
            be_flag &= ~2;
        } else {
            SUBWEP_WK(this)->life = 1;
            moveWater();
        }
        return;
    }
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &pos_old, &pos, SUBWEP_WK(this)->rad * 0.5f, 0x2001, 0x4000);
    if (nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f) {
        return;
    }
    info = EatMgr.getEffInfo(getEffectType());
    if (info) {
        SUBWEP_WK(this)->attr = info->flag | 0x80000000;
        switch (type) {
        case 0:
        default:
            SUBWEP_WK(this)->effNo = info->eff13[0];
            SUBWEP_WK(this)->effPrm = info->eff13[1];
            break;
        case 1:
            SUBWEP_WK(this)->effNo = info->eff16[0];
            SUBWEP_WK(this)->effPrm = info->eff16[1];
            break;
        case 2:
            SUBWEP_WK(this)->effNo = info->eff17[0];
            SUBWEP_WK(this)->effPrm = info->eff17[1];
            break;
        }
    } else {
        SUBWEP_WK(this)->attr = 0;
    }
    if (SUBWEP_WK(this)->attr & 1) {
        if (type > 2) {
            if (info->eff0[0] != 0xD2) {
                EstSet(0, -1, &pos, 0, info->eff0[0], (u8) info->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
            } else if (info->eff0[1] != 1) {
                EstSet(0, -1, &pos, 0, EFF_CORE, 0x3A, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
            SndCall(5, 0x24, &pos, 0, 0, 0);
            AddWaterPower(pos, 0.5f);
            ObjMgr.destroy(this);
        } else if (type == 0) {
            if (info->eff0[0] != 0xD2) {
                EstSet(0, -1, &pos, 0, info->eff0[0], (u8) info->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
            } else if (info->eff0[1] != 1) {
                EstSet(0, -1, &pos, 0, EFF_CORE, 0x3A, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
            SndCall(5, 0x24, &pos, 0, 0, 0);
            r_no_0 = 1;
            be_flag &= ~2;
        } else {
            SUBWEP_WK(this)->life = 1;
            moveWater();
        }
    } else {
        bounce(&nrm);
    }
}

// Reflects the speed on the hit normal at half the speed, reverses the spin; on a floor
// (nrm.y > 0.7) the fire / flash grenades (flags bit0) explode, the others play a bounce SE (4
// max); on a wall the egg (flags bit1) breaks with flags 0x10 set (wall splat effect).
void cSubWep::bounce(Vec* norm)
{
    Vec ref;
    f32 len;
    const f32 lim = 0.7f;
    const f32 minSpd = 50.0f;
    const f32 rate = 0.5f;
    const f32 rotRate = -0.8f;

    len = RootSumSquare3(&SUBWEP_WK(this)->spd);
    C_VECReflect(&SUBWEP_WK(this)->spd, norm, &ref);
    PSVECScale(&ref, &SUBWEP_WK(this)->spd, len * rate);
    if (norm->y > 0.0f && norm->y < lim) {
        if (SUBWEP_WK(this)->spd.y < minSpd) {
            SUBWEP_WK(this)->spd.y = minSpd;
        }
    }
    PSVECScale(&SUBWEP_WK(this)->rotSpd, &SUBWEP_WK(this)->rotSpd, rotRate);
    if (norm->y > lim) {
        if (fabsf(SUBWEP_WK(this)->spd.y) > 10.0f) {
            if (SUBWEP_WK(this)->flags & 1) {
                scrAdjust();
                explode();
                ObjMgr.destroy(this);
            } else if (SUBWEP_WK(this)->seCnt0 <= 3) {
                SndCall(5, 6, &pos, 0, 0, 0);
                SUBWEP_WK(this)->seCnt0++;
            }
        }
    } else if ((SUBWEP_WK(this)->flags & 2) || (type == 1 && norm->y > lim)) {
        SUBWEP_WK(this)->flags |= 0x10;
        explode();
        ObjMgr.destroy(this);
    } else if (SUBWEP_WK(this)->seCnt1 <= 3) {
        SndCall(1, 0x21, &pos, 0, 0, 0);
        SUBWEP_WK(this)->seCnt1++;
    }
}

// Effect surface type (EAT_ET_*) 3000 units ahead along the speed, 0 when nothing is hit.
int cSubWep::getEffectType()
{
    Vec d;
    u32 attr;

#line 484 "D:/Bio4/Prog/objSubWep.cpp"
    VECNormalize(&SUBWEP_WK(this)->spd, &d);
    PSVECScale(&d, &d, 3000.0f);
    PSVECAdd(&d, &pos, &d);
    attr = EatMgr.hitCheck(&pos, &d, 0, 0, 0, 0);
    if (attr & 0x1000000) {
        return EatGetEffectType(attr);
    }
    return 0;
}

// Defaults: pass-through collision, 1000-unit light, gravity 20, radius 50, random spin.
cSubWep::cSubWep()
{
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    sub2B4.atari.throughOn();
    LightInfo.init2(0, 1, &p0, &p1, 4);
    SUBWEP_WK(this)->seCnt0 = 0;
    SUBWEP_WK(this)->seCnt1 = 0;
    SUBWEP_WK(this)->flags = 0;
    SUBWEP_WK(this)->grav = 20.0f;
    SUBWEP_WK(this)->rad = 50.0f;
    SUBWEP_WK(this)->life = 10;
    SUBWEP_WK(this)->x7C = 3;
    SUBWEP_WK(this)->rotSpd.x = fRand0_1() * 0.19634955f + 0.09817477f;
    SUBWEP_WK(this)->rotSpd.y = 0.0f;
    SUBWEP_WK(this)->rotSpd.z = fRand0_1() * 0.09817477f + 0.09817477f;
    if (Rnd() & 1) {
        SUBWEP_WK(this)->rotSpd.x = -SUBWEP_WK(this)->rotSpd.x;
    }
    if (Rnd() & 1) {
        SUBWEP_WK(this)->rotSpd.z = -SUBWEP_WK(this)->rotSpd.z;
    }
}

// Loads the model for `type` (0 hand grenade, 1 incendiary, 2 flash, 3-5 eggs at half scale) from
// the player archive, starts it at the player's hand (parts 10, pulled back 500 from a wall
// between the body and the hand), throw speed from `power` (-1..1 stick tilt), life 45 frames for
// the hand grenade / 300 for the rest. Returns 0 when the model failed (object destroyed).
int cSubWep::init(Vec* angS, f32 rx)
{
    Vec p;
    Vec d;
    cModel* parts;
    cModel* parts2;
    void* bin;
    void* tpl;

    switch (type) {
    case 0:
    default:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6B);
        break;
    case 1:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6D);
        break;
    case 2:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6F);
        break;
    case 3:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x7E);
        break;
    case 4:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x7F);
        break;
    case 5:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x80);
        break;
    }
    if (modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(this);
        return 0;
    }
    if (type >= 3 && type <= 5) {
        scale.x = 0.5f;
        scale.y = 0.5f;
        scale.z = 0.5f;
    }
    parts = pPL->getPartsPtr(0);
    parts2 = pPL->getPartsPtr(10);
    if (EatMgr.hitCheck(&parts->world, &parts2->world, &p, &d, 0, 0) & 0x1000000) {
        PSVECScale(&d, &d, 500.0f);
        PSVECAdd(&p, &d, &p);
    }
    setPos(&p);
    this->ang = *angS;
    setThrowSpeed(&SUBWEP_WK(this)->spd, rx);
    switch (type) {
    case 0:
        SUBWEP_WK(this)->life = 45;
        break;
    case 1:
        SUBWEP_WK(this)->life = 300;
        break;
    case 2:
        SUBWEP_WK(this)->life = 300;
        break;
    case 3:
        SUBWEP_WK(this)->life = 300;
        break;
    case 4:
        SUBWEP_WK(this)->life = 300;
        break;
    case 5:
        SUBWEP_WK(this)->life = 300;
        break;
    }
    return 1;
}

// Throw velocity in world space: base speed (grenade 283 forward / 30 up; eggs 500 / 5), scaled up
// for a forward tilt (power > 0.1), down for a back tilt, pitched by -power * 45 degrees with a
// small random sideways component, plus the hand's own motion this frame.
void setThrowSpeed(Vec* spd, f32 rx)
{
    static const Vec speedGre = { 0.0f, 30.000002f, 283.5f };
    static const Vec speedEgg = { 0.0f, 5.0f, 500.0f };
    static Vec h_ang = { -0.2617994f, 0.0f, 0.0f };
    Vec v;
    Vec ang;
    Vec d;
    cModel* parts;

    switch (pG->weapon_no) {
    default:
        v = speedGre;
        break;
    case 0x19:
    case 0x1F:
    case 0x20:
        v = speedEgg;
        break;
    }
    if (rx > 0.1f) {
        PSVECScale(&v, &v, rx + 1.0f);
    } else if (rx < -0.2f) {
        PSVECScale(&v, &v, rx * 0.4f + 1.0f);
    } else {
        RotVector(&v, &h_ang, &v);
    }
    v.x = fRand1_1() * 15.0f;
    ang.z = 0.0f;
    ang.y = 0.0f;
    ang.x = rx * -0.7853982f;
    RotVector(&v, &ang, &v);
    PSMTXMultVecSR(pPL->mat, &v, spd);
    parts = pPL->getPartsPtr(0);
    PSVECSubtract(&parts->world, &parts->world_old2, &d);
    PSVECAdd(spd, &d, spd);
}

// A thrown sub weapon is dropped when an event starts.
void cSubWep::beginEvent(u32 flag)
{
    ObjMgr.destroy(this);
}

// Hand grenade: no special flags (explodes by its timer).
cObjGrenade::cObjGrenade()
{
}

// Hand grenade blast: water bomb on water, else the surface effect remembered from the last hit
// (or the default 0x0D blast with scorch 0x1A on a floor), blast SE; damage 0x13 in a 6000 radius
// (PlWepHitCheck2), Status_flg[0] 0x800000 = an explosion this frame, and the bell noise.
void cObjGrenade::explode()
{
    f32 wh;
    int no;
    int prm;
    const f32 up = 1000.0f;
    const f32 down = 3000.0f;

    if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
        EspSetWaterBomb(&pos);
        AddWaterPower(pos, 1.0f);
        SndCall(1, 0x17, &pos, 0, 0, 0);
    } else {
        if (SUBWEP_WK(this)->effNo == 0xD2 && SUBWEP_WK(this)->effPrm == 1) {
            return;
        }
        if (SUBWEP_WK(this)->attr < 0 && SUBWEP_WK(this)->effNo != 0xD2) {
            no = (u8) SUBWEP_WK(this)->effNo;
            prm = SUBWEP_WK(this)->effPrm;
        } else {
            Vec a;
            Vec b;
            Vec nrm;
            u32 attr;

            a.x = pos.x;
            a.y = pos.y + up;
            a.z = pos.z;
            b.x = pos.x;
            b.y = pos.y - down;
            b.z = pos.z;
            attr = EatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0);
            if (nrm.y > 0.9f && !(attr & 0x40)) {
                EstSet(0, -1, &pos, 0, EFF_CORE, 0x1A, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
            no = 0;
            prm = 0xD;
        }
        EstSet(0, -1, &pos, 0, no, prm, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(1, 0x14, &pos, 0, 0, 0);
    }
    StaFlagOn(pG, STA_PL_FIRE);
    PlWepHitCheck2(0, &pos, &pos, 0x13, 0, 6000.0f);
    StaFlagOn(pG, STA_SE_BURST);
    pG->SeInfo.pos = pos;
    pG->SeInfo.type = 1;
}

// Under-water blast: the same 6000-radius damage, water SE and bell noise, no effect.
void cObjGrenade::waterExplode()
{
    StaFlagOn(pG, STA_PL_FIRE);
    PlWepHitCheck2(0, &pos, &pos, 0x13, 0, 6000.0f);
    SndCall(1, 0x17, &pos, 0, 0, 0);
    StaFlagOn(pG, STA_SE_BURST);
    pG->SeInfo.pos = pos;
    pG->SeInfo.type = 1;
}

// Incendiary: explodes on the first floor hit (flags bit0).
cObjGreFire::cObjGreFire()
{
    SUBWEP_WK(this)->flags |= 1;
}

// Incendiary burst: surface effect (default 0x0B fire, plus 0x26 burning floor when within 200 of
// the floor), fire SE, DmgMgr fire area for 75 frames; no direct hit check.
void cObjGreFire::explode()
{
    f32 wh;
    int no;
    int prm;
    const f32 up = 1000.0f;
    const f32 down = 3000.0f;

    if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
        EspSetWaterBomb(&pos);
        AddWaterPower(pos, 1.0f);
        SndCall(1, 0x17, &pos, 0, 0, 0);
    } else {
        if (SUBWEP_WK(this)->effNo == 0xD2 && SUBWEP_WK(this)->effPrm == 1) {
            return;
        }
        if (SUBWEP_WK(this)->attr < 0 && SUBWEP_WK(this)->effNo != 0xD2) {
            no = (u8) SUBWEP_WK(this)->effNo;
            prm = SUBWEP_WK(this)->effPrm;
        } else {
            Vec a;
            Vec b;
            Vec nrm;
            u32 attr;

            a.x = pos.x;
            a.y = pos.y + up;
            a.z = pos.z;
            b.x = pos.x;
            b.y = pos.y - down;
            b.z = pos.z;
            attr = EatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0);
            if (nrm.y > 0.9f && !(attr & 0x40)) {
                if (pos.y - SatMgr.getFloor(&pos, 0, 600.0f, 100000.0f, 0) < 200.0f) {
                    EstSet(0, -1, &pos, 0, EFF_CORE, 0x26, 0, ESP_CORE_KIND_NONE, 0, 0);
                }
            }
            no = 0;
            prm = 0xB;
        }
        EstSet(0, -1, &pos, 0, no, prm, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(1, 0x22, &pos, 0, 0, 0);
        dmgSet(1);
    }
    StaFlagOn(pG, STA_PL_FIRE);
    StaFlagOn(pG, STA_SE_BURST);
    pG->SeInfo.pos = pos;
    pG->SeInfo.type = 1;
}

// Fizzles under water (SE only).
void cObjGreFire::waterExplode()
{
    SndCall(1, 0x23, &pos, 0, 0, 0);
}

// Flash grenade: explodes on the first floor hit (flags bit0).
cObjGreLight::cObjGreLight()
{
    SUBWEP_WK(this)->flags |= 1;
}

// Flash: screen flash 0x3F plus the 0x0C effect, flash SE, and the 0x17 (flash) hit check in a
// 15000 radius — kills Plagas heads, stuns everyone looking.
void cObjGreLight::explode()
{
    f32 wh;
    int no;
    int prm;

    if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
        EspSetWaterBomb(&pos);
        AddWaterPower(pos, 1.0f);
        SndCall(1, 0x17, &pos, 0, 0, 0);
    } else {
        if (SUBWEP_WK(this)->effNo == 0xD2 && SUBWEP_WK(this)->effPrm == 1) {
            return;
        }
        if (SUBWEP_WK(this)->attr < 0 && SUBWEP_WK(this)->effNo != 0xD2) {
            no = (u8) SUBWEP_WK(this)->effNo;
            prm = SUBWEP_WK(this)->effPrm;
        } else {
            EstSet(0, -1, 0, 0, EFF_CORE, 0x3F, 0, ESP_CORE_KIND_NONE, 0, 0);
            no = 0;
            prm = 0xC;
        }
        EstSet(0, -1, &pos, 0, no, prm, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(1, 0x13, &pos, 0, 0, 0);
    }
    StaFlagOn(pG, STA_PL_FIRE);
    PlWepHitCheck2(0, &pos, &pos, 0x17, 0, 15000.0f);
    StaFlagOn(pG, STA_SE_BURST);
    pG->SeInfo.pos = pos;
    pG->SeInfo.type = 1;
}

// Fizzles under water (SE only).
void cObjGreLight::waterExplode()
{
    SndCall(1, 0x24, &pos, 0, 0, 0);
}

// Egg: breaks on the first floor or wall hit (flags bits 0 and 1).
cObjEgg::cObjEgg()
{
    SUBWEP_WK(this)->flags |= 3;
}

// Egg splat: effect 0x42 (0x43 on a wall), SE, and the 0x19 (egg) hit check in a 2000 radius.
void cObjEgg::explode()
{
    f32 wh;

    if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
        AddWaterPower(pos, 1.0f);
    } else {
        if (SUBWEP_WK(this)->flags & 0x10) {
            EstSet(0, -1, &pos, 0, EFF_CORE, 0x43, 0, ESP_CORE_KIND_NONE, 0, 0);
        } else {
            EstSet(0, -1, &pos, 0, EFF_CORE, 0x42, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        SndCall(1, 6, &pos, 0, 0, 0);
    }
    StaFlagOn(pG, STA_PL_FIRE);
    PlWepHitCheck2(0, &pos, &pos, 0x19, 0, 2000.0f);
    StaFlagOn(pG, STA_SE_BURST);
    pG->SeInfo.pos = pos;
    pG->SeInfo.type = 1;
}

// Nothing: an egg just sinks.
void cObjEgg::waterExplode()
{
}
