// game/obj01: object id 1, thrown grenade / bottle (D:/Bio4/Prog/obj01.cpp): hand, incendiary and
// flash grenades and enemy-thrown objects. Flies under gravity with a spin, bounces off the
// scenario (EatMgr), splashes into water, and when `life` runs out spawns the effects set by
// Obj01SetEst and the damage (DmgMgr) of its eff_action type; can be held on a model's parts
// until release_timer expires.
#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "obj01.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pl_wep.h"
#include "motion.h"

extern "C" {
int obj01AddSpeed(cObj01* obj);
}

// r_no_0 dispatch: 0 flying/held, 1 exploded (fading out).
void cObj01::move()
{
    static void (cObj01::*funcTbl[2])() = { &cObj01::move00, &cObj01::move01 };

    (this->*funcTbl[r_no_0])();
}

// Rno0 == 0: counts `life` down (not for type 2, which detonates on impact) and on 0 detonates by
// eff_action: 1 hand grenade (blast effect or water burst, PlWepHitCheck2 radius 6000, rings the
// bell), 2 flash grenade (two effects, flash damage 4/5), 3 incendiary (fire effect attached to
// the object), 0/4 nothing; then Rno0 = 1. Plays the pending motion; while held (w->pEm) follows the parts
// and releases after release_timer (snapping out of the wall); otherwise obj01AddSpeed moves it
// (destroyed when it says so) and the spin is applied to parts 0.
void cObj01::move00()
{
    Obj01Work* w = OBJ01_WK(this);
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
                if (w->eff_action == 2) {
                    dmgSet(4);
                } else {
                    dmgSet(5);
                }
                r_no_0 = 1;
                return;
            case 3:
                StaFlagOn(pG, STA_PL_FIRE);
                EstSet(0, -1, &pos, 0, w->eff, (u8) w->est, 0, ESP_CORE_KIND_NONE, 0, 0);
                EstSet(this, -1, 0, 0, w->eff2, (u8) w->est2, 0, ESP_CORE_KIND_NONE, this, 0);
                SndCall(6, 0, &pos, 0, 0, 0);
                if (w->eff_action == 2) {
                    dmgSet(4);
                } else {
                    dmgSet(5);
                }
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
    if (w->pEm) {
        if (w->release_timer) {
            w->release_timer--;
            if (w->release_timer == 0) {
                cModel* parts;
                Vec hit;
                Vec dir;

                parts = GetPartsAddr(w->pEm->pParts, 0);
                if (SatMgr.hitCheck(&parts->world, &pos, &hit, 0, 0, 0)) {
                    PSVECSubtract(&parts->world, &hit, &dir);
#line 172 "D:/Bio4/Prog/obj01.cpp"
                    VECNormalize(&dir, &dir);
                    PSVECScale(&dir, &dir, w->r);
                    PSVECAdd(&hit, &dir, &dir);
                    pos = dir;
                    TransMatrix(mat, &pos);
                }
                pos_old = pos;
                w->pEm = 0;
            }
        }
        if (w->pEm == 0) {
            if (obj01AddSpeed(this)) {
                ObjMgr.destroy(this);
                return;
            }
        }
    } else if (obj01AddSpeed(this)) {
        ObjMgr.destroy(this);
        return;
    }
    if (w->be_flag & 8) {
        if (w->pEm == 0) {
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
    if (w->pEm) {
        if ((w->pEm->be_flag & 0x201) != 1) {
            w->pEm = 0;
        }
    }
    if (w->pEm) {
        cModel* parts = w->pEm->getPartsPtr(w->parts_no);
        RotMatrix(mat, &w->ang);
        TransMatrix(mat, &w->offset);
        ScaleMatrix(mat, &scale);
        PSMTXMultVec(parts->mat, &w->offset, &pos);
        PSMTXConcat(parts->mat, mat, mat);
        TransMatrix(mat, &pos);
        invisible_factor = w->pEm->invisible_factor;
        invisible_factor2 = w->pEm->invisible_factor2;
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

// Rno0 == 1: after-explosion: a sound every 30 frames, destroyed after the 6th.
void cObj01::move01()
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

// Registers the explosion damage volume: 2/8 fire (radius 3000 / 15000), 4/5 flash (1500).
void cObj01::dmgSet(int type)
{
    switch ((u32) type) {
    case 8:
        DmgMgr.set(DMG_TYPE_GRENADE_BLAST, 2, &pos, 3000.0f, 3000.0f);
        DmgMgr.set(DMG_TYPE_GRENADE, 2, &pos, 15000.0f, 3000.0f);
        break;
    case 4:
        DmgMgr.set(DMG_TYPE_FLAME, 90, &pos, 1500.0f, 3000.0f);
        break;
    case 5:
        DmgMgr.set(DMG_TYPE_LAMP, 90, &pos, 1500.0f, 3000.0f);
        break;
    }
}

// Physics step: gravity, move; with be_flag bit 2 checks water (splash effect, drown -> life 0;
// returns 1 to destroy for non-exploding types) and the scenario (reflect the speed at 20%, damp
// the spin; on a floor hit slower than 50 stop with the landing sound / effect). Returns 1 when
// the object should be destroyed.
int obj01AddSpeed(cObj01* pObj)
{
    Obj01Work* w = OBJ01_WK(pObj);
    f32 wh;
    Vec ref;
    Vec nrm;
    f32 len;

    w->spd.y -= w->gravity;
    PSVECAdd(&pObj->pos, &w->spd, &pObj->pos);
    if (!(w->be_flag & 4)) {
        return 0;
    }
    if (GetWaterHeight(&pObj->pos, &wh) && pObj->pos.y <= wh) {
        pObj->pos.y = wh;
        if (!(w->flag & 8)) {
            EstSet(0, -1, &pObj->pos, 0, w->eff3, (u8) w->est3, 0, ESP_CORE_KIND_NONE, 0, 0);
            w->flag |= 8;
            AddWaterPower(pObj->pos, 0.5f);
            if (pObj->type != 1) {
                SndCall(6, 0x64, &pObj->pos, 0, 0, 0);
            } else {
                SndCall(2, 4, &pObj->pos, 0, 0, 0);
            }
        }
        w->timer = 0;
        switch (w->eff_action) {
        case 2:
        case 3:
            break;
        default:
            return 0;
        }
        return 1;
    }
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &pObj->pos_old, &pObj->pos, w->r * 0.5f, 0x2001, 0);
    if (nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f) {
        return 0;
    }
    len = RootSumSquare3(&w->spd);
    C_VECReflect(&w->spd, &nrm, &ref);
    PSVECScale(&ref, &w->spd, len * 0.2f);
    PSVECScale(&w->rot_spd, &w->rot_spd, -0.8f);
    if (nrm.y > 0.9f) {
        switch (w->eff_action) {
        case 0:
            break;
        case 1:
            if (w->spd.y > 50.0f) {
                if (w->Bound_se_ck == 0) {
                    w->Bound_se_ck = 3;
                    SndCall(5, 6, &pObj->pos, 0, 0, 0);
                }
            }
            break;
        case 2:
            pObj->dmgSet(4);
            EstSet(0, -1, &pObj->pos, 0, w->eff, (u8) w->est, 0, ESP_CORE_KIND_NONE, 0, 0);
            EstSet(0, -1, &pObj->pos, 0, w->eff2, (u8) w->est2, 0, ESP_CORE_KIND_NONE, 0, 0);
            if (w->eff_action == 3) {
                SndCall(6, 0, &pObj->pos, 0, 0, 0);
            } else {
                SndCall(1, 0x15, &pObj->pos, 0, 0, 0);
                SndCall(1, 0x16, &pObj->pos, 0, 0, 0);
            }
            pObj->r_no_0 = 1;
            pObj->be_flag &= ~2;
            return 0;
        case 3:
            pObj->dmgSet(4);
            EstSet(0, -1, &pObj->pos, 0, w->eff, (u8) w->est, 0, ESP_CORE_KIND_NONE, 0, 0);
            EstSet(pObj, -1, 0, 0, w->eff2, (u8) w->est2, 0, ESP_CORE_KIND_NONE, pObj, 0);
            if (w->eff_action == 3) {
                SndCall(6, 0, &pObj->pos, 0, 0, 0);
            } else {
                SndCall(1, 0x15, &pObj->pos, 0, 0, 0);
                SndCall(1, 0x16, &pObj->pos, 0, 0, 0);
            }
            pObj->r_no_0 = 1;
            pObj->be_flag &= ~2;
            return 0;
        case 4:
            if (w->Bound_se_ck == 0) {
                w->Bound_se_ck = 1;
                SndCall(5, 5, &pObj->pos, 0, 0, 0);
            }
            break;
        }
    }
    return 0;
}

// Creates a thrown object at pos/rot with speed spd, gravity grav per frame, radius rad and fuse
// `life` frames; flags: 1 scenario collision, 2 random tumble, 4 slow forward tumble, 0x10 fixed
// tumble.
cObj* SetObj01(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* spd, f32 grav, f32 rad, int life, int flags)
{
    cObj* obj;
    Obj01Work* w;

    obj = ObjMgr.create(cObjMgr::ID_MAGAZINE);
    if (obj == 0) {
        return 0;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    obj->atari.throughOn();
    obj->LightInfo.init2(0, 1, &p0, &p1, 4);
    w = OBJ01_WK((cObj01*) obj);
    obj->pos = *pos;
    obj->pos_old = *pos;
    obj->ang = *rot;
    w->spd = *spd;
    w->gravity = grav;
    w->r = rad;
    w->timer = life;
    w->pEm = 0;
    w->eff = -1;
    w->est = -1;
    w->eff2 = -1;
    w->est2 = -1;
    w->eff_action = 0;
    w->release_timer = 0;
    w->Bound_se_ck = 0;
    if (flags & 1) {
        w->be_flag |= 4;
    }
    if (flags & 2) {
        w->be_flag |= 8;
        w->rot_spd.x = fRand0_1() * 0.19634955f + 0.09817477f;
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
    if (flags & 0x10) {
        w->be_flag |= 8;
        w->rot_spd.x = -0.2617994f;
        w->rot_spd.y = 0.0f;
        w->rot_spd.z = 0.0f;
    }
    return obj;
}

// Sets the detonation type (eff_action) and its four est (owner, id) pairs: burst, secondary,
// water splash on landing, water explosion.
void Obj01SetEst(cObj* pObj, u32 eff, u32 est, u32 action, u32 eff2, u32 est2, u32 eff3, u32 est3, u32 eff4, u32 est4)
{
    Obj01Work* w;

    if (pObj == 0) {
        return;
    }
    w = OBJ01_WK((cObj01*) pObj);
    w->eff = eff;
    w->est = est;
    w->eff2 = eff2;
    w->est2 = est2;
    w->est3 = est3;
    w->eff3 = eff3;
    w->eff4 = eff4;
    w->est4 = est4;
    w->eff_action = action;
}
