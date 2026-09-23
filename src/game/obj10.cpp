// game/obj10: object id 10, the player's thrown weapon item cWepItem (D:/Bio4/Prog/obj10.cpp):
// grenades and the like thrown by the player, with the obj01 flight model (gravity, spin, bounce,
// water) but its own landing sounds, a self-damage check on the explosion (hitCkPl) and no
// underwater/flash variants; deleted when an event starts.
#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "obj10.h"
#include "esp.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pl_wep.h"
#include "player.h"
#include "motion.h"

extern "C" {
int obj10AddSpeed(cWepItem* obj);
int effWaterCheck(cModel* obj);
}

// r_no_0 dispatch: 0 flying, 1 exploded.
void cWepItem::move()
{
    static void (cWepItem::*funcTbl[2])() = { &cWepItem::move00, &cWepItem::move01 };

    (this->*funcTbl[r_no_0])();
}

// Rno0 == 0: fuse countdown and detonation by eff_action (1 hand grenade + player self-damage, 2
// flash, 3 incendiary), pending motion, hold-on-parts / release, flight (obj10AddSpeed) and spin.
void cWepItem::move00()
{
    WepItemWork* w = WEPITEM_WK(this);
    int life = w->timer;
    f32 wh;

    if (life) {
        if (w->eff_action != 2) {
            w->timer = life - 1;
        }
    } else {
        if (w->eff != -1 && w->est != -1) {
            switch (w->eff_action) {
            case 1:
                StaFlagOn(pG, STA_PL_FIRE);
                if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
                    EstSet(0, -1, &pos, 0, w->eff4, (u8) w->est4, 0, ESP_CORE_KIND_NONE, 0, 0);
                    AddWaterPower(pos, 1.0f);
                    SndCall(1, 0x17, &pos, 0, 0, 0);
                } else {
                    EstSet(0, -1, &pos, 0, w->eff, (u8) w->est, 0, ESP_CORE_KIND_NONE, 0, 0);
                    SndCall(1, 0x14, &pos, 0, 0, 0);
                }
                PlWepHitCheck2(0, &pos, &pos, 0x13, 0, 6000.0f);
                hitCkPl();
                StaFlagOn(pG, STA_SE_BURST);
                pG->SeInfo.pos = pos;
                pG->SeInfo.type = 1;
                ObjMgr.destroy(this);
                return;
            case 2:
                StaFlagOn(pG, STA_PL_FIRE);
                EstSet(0, -1, &pos, 0, w->eff, (u8) w->est, 0, ESP_CORE_KIND_NONE, 0, 0);
                EstSet(0, -1, &pos, 0, w->eff2, (u8) w->est2, 0, ESP_CORE_KIND_NONE, 0, 0);
                SndCall(1, 0x15, &pos, 0, 0, 0);
                SndCall(1, 0x16, &pos, 0, 0, 0);
                dmgSet(1);
                r_no_0 = 1;
                return;
            case 0:
            default:
                break;
            }
        } else {
            invisible_factor -= 0.2f;
            if (invisible_factor <= 0.0f) {
                ObjMgr.destroy(this);
            }
            return;
        }
        ObjMgr.destroy(this);
        return;
    }
    if (w->be_flag & 1) {
        MotionSetCore(this, &Motion, w->pMot, 0, 0, w->motPrm, 0);
        w->be_flag = (w->be_flag & ~1) | 2;
    }
    if (w->be_flag & 2) {
        MotionMove(this, 0);
    }
    if (w->hold) {
        if (w->release_timer) {
            w->release_timer--;
            if (w->release_timer == 0) {
                cModel* parts;
                Vec hit;
                Vec dir;

                parts = GetPartsAddr(w->hold->pParts, 0);
                if (SatMgr.hitCheck(&parts->world, &pos, &hit, 0, 0, 0)) {
                    PSVECSubtract(&parts->world, &hit, &dir);
#line 167 "D:/Bio4/Prog/obj10.cpp"
                    VECNormalize(&dir, &dir);
                    PSVECScale(&dir, &dir, w->r);
                    PSVECAdd(&hit, &dir, &dir);
                    pos = dir;
                    TransMatrix(mat, &pos);
                }
                pos_old = pos;
                w->hold = 0;
            }
        }
        if (w->hold == 0) {
            if (obj10AddSpeed(this)) {
                ObjMgr.destroy(this);
                return;
            }
        }
    } else if (obj10AddSpeed(this)) {
        ObjMgr.destroy(this);
        return;
    }
    if (w->be_flag & 8) {
        if (w->hold == 0) {
            cModel* parts = GetPartsAddr(pParts, 0);
            if (parts) {
                PSVECAdd(&parts->ang, &w->rot_spd, &parts->ang);
                parts->ang.x = LIMIT_ANGLE(parts->ang.x);
                parts->ang.y = LIMIT_ANGLE(parts->ang.y);
                parts->ang.z = LIMIT_ANGLE(parts->ang.z);
                RotMatrix(parts->l_mat, &parts->ang);
                TransMatrix(parts->l_mat, &parts->pos);
                ScaleMatrix(parts->l_mat, &parts->scale);
            }
        }
    }
    if (w->hold) {
        if ((w->hold->be_flag & 0x201) != 1) {
            w->hold = 0;
        }
    }
    if (w->hold) {
        cModel* parts = w->hold->getPartsPtr(w->parts_no);
        RotMatrix(mat, &w->ang);
        TransMatrix(mat, &w->offset);
        ScaleMatrix(mat, &scale);
        PSMTXMultVec(parts->mat, &w->offset, &pos);
        PSMTXConcat(parts->mat, mat, mat);
        TransMatrix(mat, &pos);
        invisible_factor = w->hold->invisible_factor;
        invisible_factor2 = w->hold->invisible_factor2;
    } else {
        RotMatrix(l_mat, &ang);
        TransMatrix(l_mat, &pos);
        ScaleMatrix(l_mat, &scale);
        PSMTXCopy(l_mat, mat);
        invisible_factor = 1.0f;
        invisible_factor2 = 1.0f;
    }
    partsMatCalc();
    partsWorldCalc();
}

// Rno0 == 1: after-explosion sound every 30 frames, destroyed after the 6th.
void cWepItem::move01()
{
    r_no_2++;
    if (r_no_2 > 30) {
        r_no_2 = 0;
        SndCall(1, 0x16, &pos, 0, 0, 0);
        r_no_3++;
        if (r_no_3 > 5) {
            ObjMgr.destroy(this);
        }
    }
}

// Explosion damage volumes: 2/8 fire (3000 / 15000), 1 flash (1500, 180 frames).
void cWepItem::dmgSet(int dmtype)
{
    switch (dmtype) {
    case 8:
        DmgMgr.set(DMG_TYPE_GRENADE_BLAST, 2, &pos, 3000.0f, 3000.0f);
        DmgMgr.set(DMG_TYPE_GRENADE, 2, &pos, 15000.0f, 3000.0f);
        break;
    case 1:
        DmgMgr.set(DMG_TYPE_FIRE, 180, &pos, 1500.0f, 3000.0f);
        break;
    }
}

// Hand grenade self-damage: the player within 2000 units takes damage type 7, 10000.
void cWepItem::hitCkPl()
{
    if (GetDistance(&pos, &pPL->pos) < 4000000.0f) {
        PlSetDamage(PL_DM_BACK, 10000, 0);
    }
}

// Event start: the thrown item is removed.
void cWepItem::beginEvent(u32 flag)
{
    ObjMgr.destroy(this);
}

// Physics step as obj01AddSpeed, with the water landing skipped for weapons 0xB/0xC and the
// player's landing sounds; returns 1 when the object should be destroyed.
int obj10AddSpeed(cWepItem* pObj)
{
    WepItemWork* w = WEPITEM_WK(pObj);
    f32 wh;
    Vec ref;
    Vec nrm;
    f32 len;

    w->spd.y -= w->gravity;
    PSVECAdd(&pObj->pos, &w->spd, &pObj->pos);
    if (!(w->be_flag & 4)) {
        return 0;
    }
    if (GetWaterHeight(&pObj->pos, &wh) && pObj->pos.y <= wh && !(pG->weapon_no == 0xB || pG->weapon_no == 0xC)) {
        pObj->pos.y = wh;
        if (!(w->flag & 8)) {
            EstSet(0, -1, &pObj->pos, 0, w->eff3, (u8) w->est3, 0, ESP_CORE_KIND_NONE, 0, 0);
            w->flag |= 8;
            AddWaterPower(pObj->pos, 0.5f);
            switch (pObj->type) {
            default:
                SndCall(6, 0x64, &pObj->pos, 0, 0, 0);
                break;
            case 1:
                SndCall(2, 0xA, &pObj->pos, 0, 0, 0);
                break;
            case 0x63:
                break;
            }
        }
        w->timer = 0;
        return w->eff_action == 2;
    }
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    if (!((pObj->type == 1 || pObj->type == 0x63) && (pG->weapon_no == 0xB || pG->weapon_no == 0xC))) {
        EatMgr.adjust(&nrm, &pObj->pos_old, &pObj->pos, w->r * 0.5f, 0x2001, 0);
    }
    if (nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f) {
        return 0;
    }
    len = RootSumSquare3(&w->spd);
    C_VECReflect(&w->spd, &nrm, &ref);
    PSVECScale(&ref, &w->spd, len * 0.5f);
    PSVECScale(&w->rot_spd, &w->rot_spd, -0.8f);
    if (nrm.y > 0.9f) {
        switch (w->eff_action) {
        case 1:
            if (w->spd.y > 50.0f) {
                if (w->Bound_se_ck) {
                    w->Bound_se_ck--;
                    SndCall(5, 6, &pObj->pos, 0, 0, 0);
                }
            }
            break;
        case 2:
            pObj->dmgSet(1);
            EstSet(0, -1, &pObj->pos, 0, w->eff, (u8) w->est, 0, ESP_CORE_KIND_NONE, 0, 0);
            EstSet(0, -1, &pObj->pos, 0, w->eff2, (u8) w->est2, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(1, 0x15, &pObj->pos, 0, 0, 0);
            SndCall(1, 0x16, &pObj->pos, 0, 0, 0);
            pObj->r_no_0 = 1;
            pObj->be_flag &= ~2;
            return 0;
        case 0:
        default:
            break;
        }
    }
    switch (pObj->type) {
    case 1:
        if (effWaterCheck(pObj)) {
            SndCall(2, 0xA, &pObj->pos, 0, 0, 0);
            return 1;
        }
        w->se_count++;
        if (w->se_count <= 3) {
            SndCall(2, 0xF, &pObj->pos, 0, 0, 0);
        }
        break;
    case 2:
        w->se_count++;
        if (w->se_count <= 2) {
            SndCall(2, 8, &pObj->pos, 0, 0, 0);
        }
        break;
    case 0x63:
        break;
    }
    return 0;
}

// 1 when the floor 500 units below the object is a water-type eat surface (effect type 2).
int effWaterCheck(cModel* pObj)
{
    static const Vec spd = { 0.0f, -500.0f, 0.0f };
    Vec hit;
    u32 attr;

    PSVECAdd(&spd, &pObj->pos, &hit);
    attr = EatMgr.hitCheck(&pObj->pos, &hit, 0, 0, 0, 0);
    if (attr & 0x1000000) {
        return EatGetEffectType(attr) == 2;
    }
    return 0;
}

// Creates the thrown item (back of the object pool) at pos/rot with speed, gravity, radius, fuse
// and the obj01 tumble flags.
cObj* SetObj10(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* spd, f32 grav, f32 rad, int life, int flags)
{
    cObj* obj;
    WepItemWork* w;

    obj = ObjMgr.createBack(cObjMgr::ID_WEP_ITEM);
    if (obj == 0) {
        return 0;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    obj->sub2B4.atari.throughOn();
    obj->sub2B4.atari.m_flag |= 0x400;
    obj->LightInfo.init2(0, 1, &p0, &p1, 4);
    w = WEPITEM_WK((cWepItem*) obj);
    obj->pos = *pos;
    obj->pos_old = *pos;
    obj->ang = *rot;
    w->spd = *spd;
    w->gravity = grav;
    w->r = rad;
    w->timer = life;
    w->hold = 0;
    w->eff = -1;
    w->est = -1;
    w->eff2 = -1;
    w->est2 = -1;
    w->eff_action = 0;
    w->release_timer = 0;
    w->Bound_se_ck = 3;
    if (flags & 1) {
        w->be_flag |= 4;
    }
    if (flags & 2) {
        w->be_flag |= 8;
        w->rot_spd.x = fRand0_1() * 0.19634955f + 0.39269908f;
        w->rot_spd.y = 0.0f;
        w->rot_spd.z = fRand0_1() * 0.09817477f + 0.09817477f;
        if (Rnd() & 1) {
            w->rot_spd.x = -w->rot_spd.x;
        }
        if (Rnd() & 1) {
            w->rot_spd.z = -w->rot_spd.z;
        }
    }
    if (flags & 4) {
        w->be_flag |= 8;
        w->rot_spd.x = -(fRand0_1() * 0.049087387f + 0.19634955f);
        w->rot_spd.y = 0.0f;
        w->rot_spd.z = 0.0f;
    }
    return obj;
}

// Sets the detonation type and its four est pairs (as Obj01SetEst).
void Obj10SetEst(cObj* obj, int no0, int prm0, u32 type, int no1, int prm1, int no2, int prm2, int no3, int prm3)
{
    WepItemWork* w;

    if (obj == 0) {
        return;
    }
    w = WEPITEM_WK((cWepItem*) obj);
    w->eff = no0;
    w->est = prm0;
    w->eff2 = no1;
    w->est2 = prm1;
    w->est3 = prm2;
    w->eff3 = no2;
    w->eff4 = no3;
    w->est4 = prm3;
    w->eff_action = type;
}
// __END__
