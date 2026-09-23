// game/emmine.cpp: mine / arrow enemy (cEmMine): the mine thrower's mines (homing when the
// weapon level is high enough) and the crossbow arrows. They fly, stick to the scenario or an
// enemy, beep and explode (mines) or fall down as a three-node rope (arrows).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "emmine.h"
#include "emhit.h"
#include "esp.h"
#include "snd.h"
#include "player.h"
#include "pl_wep.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "db_log.h"
#include "est.h"
#include "em_sub.h"

// Rope node of the falling arrow (emMine_R1_Fall).
struct MineNode {
    Vec pos;          // 0x00
    Vec old;       // 0x0C
    Vec spd;          // 0x18
    f32 len;          // 0x24  rest distance to the next node
    int reflect;      // 0x28
};


typedef void (*EmMineFunc)(cEmMine*);

EmMineFunc EmMine_R0_move_tbl[4] = {
    emMine_R0_Init,
    emMine_R0_Move,
    0,
    0,
};

static EmMineFunc EmMine_R1_move_tbl[9] = {
    emMine_R1_Shot,
    emMine_R1_ShotArrow,
    emMine_R1_Set,
    emMine_R1_SetWater,
    emMine_R1_Parent,
    emMine_R1_BombWait,
    emMine_R1_BombWait2,
    emMine_R1_Fall,
    emMine_R1_Lost,
};

// Creates a mine / arrow enemy (id 0x4F) from a model / TPL at `pos` flying with speed `spd`
// (NULL: a random forward throw). type 0 mine, 1 homing mine (picks its target at once), 2
// crossbow arrow; a firepower level above 2 forces em->type to 1 whatever was asked. Records the weapon
// level for the blast radius, a Core_kind for the trail effects and the default explosion est
// 0x36 / SE. Starts in Rno1 0 Shot (mine) or 1 ShotArrow. NULL on failure.
cEmMine* SetMine(void* bin, void* tpl, Vec* pos, Vec* spd, int type)
{
    cEmMine* em;
    EmMineWork* w;
    cAtariInfo* at;
    Vec v;
    f32 len;

    em = (cEmMine*) EmMgr.createBack(0x4F);
    if (em == 0) {
        return 0;
    }
    w = EMMINE_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    em->pos_old = em->pos;
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetWeapon() ModelInit failed.");
        EmMgr.destroy(em);
        return 0;
    }
    w->Lv = pG->weapon_lv_power;
    em->type = type;
    if (w->Lv > 2) {
        em->type = 1;
    }
    YarareInit(em, 0.0f, 0.0f, -100.0f, 300.0f, 10.0f, 1, YAT_FLAG_ON);
    at = &em->atari;
    at->init(0.0f, 0.0f, 0.0f, 150.0f, 150.0f, 150.0f, 300.0f, 1, 0x2000, 10);
    em->hp = 1;
    em->hp_max = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 4);
    }
    LockPartsSet(em, 0);
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->be_flag &= ~0x01000000;
    at->setPriority(PRI_LV3);
    at->m_flag &= ~0x300;
    em->be_flag &= ~0x10;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    w->Be_flg = 0;
    w->Norm.x = 0.0f;
    w->Norm.y = 1.0f;
    w->Norm.z = 0.0f;
    w->Norm_ck = 0;
    w->pEm_oya = 0;
    w->pEm_homing = 0;
    w->Homing_wait = 3;
    w->EffKindId = EspPullCoreKind();
    w->Bomb_eff = 0;
    w->Bomb_est = 0x36;
    w->Bomb_seid = 1;
    w->Bomb_seno = 0x14;
    if (spd) {
        v = *spd;
        if (em->type == 1) {
            PSVECScale(&v, &v, 0.5f);
        }
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
    }
    w->Spd.x = v.x;
    w->Spd.y = v.y;
    w->Spd.z = v.z;
    len = SQRTF(v.x * v.x + v.z * v.z);
    em->ang.x = -atan2f(v.y, len);
    em->ang.y = atan2f(v.x, v.z);
    em->ang.z = 0.0f;
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (em->type == 1) {
        cEm* target = pPL->Wep->m_pWep->m_SightEm;

        if (target) {
            w->pEm_homing = target;
        } else {
            emMineSearchEm(em, 1);
        }
    }
    if (type == 2) {
        em->hp = 0;
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    } else {
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    }
    emMine_R0_Move(em);
    return em;
}

// Event start: mines and arrows in flight are removed.
void cEmMine::beginEvent(u32 flag)
{
    EmMgr.destroy(this);
}

// A weapon hit (not knife / grenades) detonates the mine at once.
void emMineDmCk(cEmMine* pEm)
{
    u8 wep;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    wep = pEm->dmg.m_Wep;
    pEm->dmg.m_Flag = 0;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0x2A) {
        return;
    }
    pEm->setStatus(EM_STATUS_LOCKOFF);
    pEm->hp = 0;
    pEm->setBomb();
}

// Per-frame: hit check, then the Rno0 routine (0 Init -> Lost, 1 Move).
void cEmMine::move()
{
    emMineDmCk(this);
    EmMine_R0_move_tbl[r_no_0](this);
}

// Rno0 == 0: an uninitialised mine is discarded (Lost).
void emMine_R0_Init(cEmMine* pEm)
{
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 8;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Shot, 1 ShotArrow, 2 Set, 3 SetWater, 4 Parent, 5 BombWait,
// 6 BombWait2, 7 Fall, 8 Lost).
void emMine_R0_Move(cEmMine* pEm)
{
    EmMine_R1_move_tbl[pEm->r_no_1](pEm);
}

// Rno1 == 0: the mine in flight: starts the trail est 0x38, explodes after 210 frames, homes on
// its target every frame (type 1, retargeting every Homing_wait frames), moves by Spd; hitting an
// enemy sticks the mine to it (emMineHitCk -> Parent), hitting the scenery sticks it there (Set,
// with the surface effect and the explosion est / SE chosen by the surface's AtEffInfo; attribute
// 0x40 surfaces detonate at once), landing in water sinks it (SetWater with a splash).
void emMine_R1_Shot(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);
    Vec d;
    Vec hit;
    f32 wh;
    f32 wh2;
    f32 len;
    int attr;
    AtEffInfo* info;
    AtEffInfo* wi;  // the water blocks' own pointer: its zero (not the HitCk result's) feeds their EstSet stack zeros

    switch (pEm->r_no_2) {
    case 0:
        EstSet(pEm, -1, 0, 0, EFF_CORE, 0x38, 0, w->EffKindId, pEm, 0);
        w->Bomb_wait = 210;
        pEm->r_no_2++;
    case 1:
        if (w->Bomb_wait == 0) {
            pEm->setBomb();
            return;
        }
        w->Bomb_wait--;
        break;
    }
    if (pEm->type == 1) {
        if (w->Homing_wait != 0) {
            w->Homing_wait--;
        } else {
            emMineSearchEm(pEm, 0);
            emMineHomingEm(pEm);
        }
    }
    PSVECAdd(&pEm->pos, &w->Spd, &pEm->pos);
    if (emMineHitCk(pEm) != 0) {
        goto DELETE_EFFECT;
    }
    {
        w->Norm_ck = 0;
        attr = EatMgr.hitCheck(&pEm->pos_old, &pEm->pos, &hit, &w->Norm, 0, 0x404000);
        if (attr) {
            if (attr & 0x40) {
                pEm->setBomb();
                return;
            }
            w->Norm_ck = 1;
            pEm->pos = hit;
            info = EatMgr.getEffInfo(EatGetEffectType(attr));
            if (info) {
                if (info->flag & 1) {
                    if (!(info->eff0[0] == 0xD2 && info->eff0[1] == 1)) {
                        EstSet(0, -1, &pEm->pos, 0, info->eff0[0], (u8) info->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
                    }
                }
                w->Bomb_eff = (u8) info->eff6[0];
                w->Bomb_est = (u8) info->eff6[1];
                if (info->flag & 1) {
                    w->Bomb_seid = 1;
                    w->Norm_ck = 0;
                    w->Bomb_seno = 0x17;
                    SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                    pEm->r_no_0 = 1;
                    pEm->r_no_1 = 3;
                    pEm->r_no_2 = 0;
                    pEm->r_no_3 = 0;
                } else {
                    w->Bomb_seid = 1;
                    w->Bomb_seno = 0x14;
                    SndCall(2, 0x14, &pEm->pos, 0, 0, pEm);
                    pEm->r_no_0 = 1;
                    pEm->r_no_1 = 2;
                    pEm->r_no_2 = 0;
                    pEm->r_no_3 = 0;
                }
            } else {
                if (GetWaterHeight(&pEm->pos, &wh) && pEm->pos.y <= wh) {
                    pEm->pos.y = wh;
                    wi = EatMgr.getEffInfo(EAT_ET_WATER);
                    if (wi) {
                        if (!(wi->eff0[0] == 0xD2 && wi->eff0[1] == 1)) {
                            EstSet(0, -1, &pEm->pos, 0, wi->eff0[0], (u8) wi->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
                        }
                        w->Bomb_eff = (u8) wi->eff6[0];
                        w->Bomb_est = (u8) wi->eff6[1];
                        SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                        AddWaterPower(pEm->pos, 0.5f);
                    } else {
                        EstSet(0, -1, &pEm->pos, 0, EFF_CORE, 0x3A, 0, ESP_CORE_KIND_NONE, 0, 0);
                        w->Bomb_eff = 0;
                        w->Bomb_est = 0x39;
                        SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                        AddWaterPower(pEm->pos, 0.5f);
                        w->Bomb_eff = 0;
                        w->Bomb_est = 0x39;
                    }
                    w->Bomb_seid = 1;
                    w->Bomb_seno = 0x17;
                    EffectEspDelete(0, w->EffKindId, pEm, 0);
                    EffectEspgenDelete(0, w->EffKindId, pEm);
                    EffectEfmDelete(0, w->EffKindId, pEm);
                    pEm->r_no_0 = 1;
                    pEm->r_no_1 = 3;
                    pEm->r_no_2 = 0;
                    pEm->r_no_3 = 0;
                    return;
                }
                w->Bomb_est = 0x36;
                w->Bomb_seno = 0x14;
                w->Bomb_eff = 0;
                w->Bomb_seid = 1;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 2;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
                SndCall(2, 0x14, &pEm->pos, 0, 0, pEm);
                // Tail written out (jump2 cross-jumps it into DELETE_EFFECT): the SndCall block then
                // runs through the three Effect*Delete calls, so its r7/r8 argument moves collect an
                // anti-dependence from every later call and issue first (r7, r8, r5, r4, r6, r3).
                EffectEspDelete(0, w->EffKindId, pEm, 0);
                EffectEspgenDelete(0, w->EffKindId, pEm);
                EffectEfmDelete(0, w->EffKindId, pEm);
                return;
            }
        DELETE_EFFECT:
            EffectEspDelete(0, w->EffKindId, pEm, 0);
            EffectEspgenDelete(0, w->EffKindId, pEm);
            EffectEfmDelete(0, w->EffKindId, pEm);
            return;
        }
        if (GetWaterHeight(&pEm->pos, &wh2) && pEm->pos.y <= wh2) {
            pEm->pos.y = wh2;
            wi = EatMgr.getEffInfo(EAT_ET_WATER);
            if (wi) {
                if (!(wi->eff0[0] == 0xD2 && wi->eff0[1] == 1)) {
                    EstSet(0, -1, &pEm->pos, 0, wi->eff0[0], (u8) wi->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
                }
                w->Bomb_eff = (u8) wi->eff6[0];
                w->Bomb_est = (u8) wi->eff6[1];
                SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                AddWaterPower(pEm->pos, 0.5f);
            } else {
                EstSet(0, -1, &pEm->pos, 0, EFF_CORE, 0x3A, 0, ESP_CORE_KIND_NONE, 0, 0);
                w->Bomb_eff = 0;
                w->Bomb_est = 0x39;
                SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                AddWaterPower(pEm->pos, 0.5f);
                w->Bomb_eff = 0;
                w->Bomb_est = 0x39;
            }
            w->Bomb_seid = 1;
            w->Bomb_seno = 0x17;
            EffectEspDelete(0, w->EffKindId, pEm, 0);
            EffectEspgenDelete(0, w->EffKindId, pEm);
            EffectEfmDelete(0, w->EffKindId, pEm);
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 3;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            return;
        }
        PSVECSubtract(&pEm->pos, &pEm->pos_old, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        pEm->ang.x = -atan2f(d.y, len);
        pEm->ang.y = atan2f(d.x, d.z);
        pEm->ang.z = 0.0f;
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        pEm->partsWorldCalc();
    }
}

// Rno1 == 1: the arrow in flight (trail est 0x4C, lost after 210 frames): same hit handling as
// the mine but it sticks silently and never explodes.
void emMine_R1_ShotArrow(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);
    Vec d;
    Vec hit;
    f32 wh;
    f32 wh2;
    f32 len;
    int attr;
    AtEffInfo* info;

    switch (pEm->r_no_2) {
    case 0:
        EstSet(pEm, -1, 0, 0, EFF_CORE, 0x4C, 0, w->EffKindId, pEm, 0);
        w->Bomb_wait = 210;
        pEm->r_no_2++;
    case 1:
        if (w->Bomb_wait == 0) {
            pEm->setLost();
            return;
        }
        w->Bomb_wait--;
        break;
    }
    PSVECAdd(&pEm->pos, &w->Spd, &pEm->pos);
    if (emMineHitCk(pEm) != 0) {
        goto DELETE_EFFECT;
    }
    {
        w->Norm_ck = 0;
        attr = EatMgr.hitCheck(&pEm->pos_old, &pEm->pos, &hit, &w->Norm, 0, 0x404000);
        if (attr) {
            if (attr & 0x40) {
                pEm->setFall();
                return;
            }
            w->Norm_ck = 1;
            pEm->pos = hit;
            info = EatMgr.getEffInfo(EatGetEffectType(attr));
            if (info) {
                if (info->flag & 1) {
                    EstSet(0, -1, &pEm->pos, 0, info->eff0[0], (u8) info->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
                }
                w->Bomb_eff = (u8) info->eff6[0];
                w->Bomb_est = (u8) info->eff6[1];
                if (info->flag & 1) {
                    w->Bomb_seid = 1;
                    w->Norm_ck = 0;
                    w->Bomb_seno = 0x17;
                    SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                    EffectEspDelete(0, w->EffKindId, pEm, 0);
                    EffectEspgenDelete(0, w->EffKindId, pEm);
                    EffectEfmDelete(0, w->EffKindId, pEm);
                    pEm->setLost();
                    return;
                }
                w->Bomb_seid = 1;
                w->Bomb_seno = 0x14;
                SndCall(1, 0x50, &pEm->pos, 0, 0, pEm);
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 2;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
                return;
            }
            if (GetWaterHeight(&pEm->pos, &wh) && pEm->pos.y <= wh) {
                pEm->pos.y = wh;
                // Block-scoped water pointers (one single-set pseudo per block: each ranks below `em`
                // in global-alloc, so em keeps r29 and both take r28; one two-set `wi` outranks em).
                AtEffInfo* wi = EatMgr.getEffInfo(EAT_ET_WATER);
                if (wi) {
                    EstSet(0, -1, &pEm->pos, 0, wi->eff0[0], (u8) wi->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
                    w->Bomb_eff = (u8) wi->eff6[0];
                    w->Bomb_est = (u8) wi->eff6[1];
                    SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                    AddWaterPower(pEm->pos, 0.5f);
                } else {
                    EstSet(0, -1, &pEm->pos, 0, EFF_CORE, 0x3A, 0, ESP_CORE_KIND_NONE, 0, 0);
                    w->Bomb_eff = 0;
                    w->Bomb_est = 0x39;
                    SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                    AddWaterPower(pEm->pos, 0.5f);
                    w->Bomb_eff = 0;
                    w->Bomb_est = 0x39;
                }
                w->Bomb_seid = 1;
                w->Bomb_seno = 0x17;
                EffectEspDelete(0, w->EffKindId, pEm, 0);
                EffectEspgenDelete(0, w->EffKindId, pEm);
                EffectEfmDelete(0, w->EffKindId, pEm);
                pEm->setLost();
                return;
            }
            w->Bomb_est = 0x36;
            w->Bomb_seno = 0x14;
            w->Bomb_eff = 0;
            w->Bomb_seid = 1;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 2;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            SndCall(1, 0x50, &pEm->pos, 0, 0, pEm);
            EffectEspDelete(0, w->EffKindId, pEm, 0); // tail written out, see emMine_R1_Shot
            EffectEspgenDelete(0, w->EffKindId, pEm);
            EffectEfmDelete(0, w->EffKindId, pEm);
            return;
        DELETE_EFFECT:
            EffectEspDelete(0, w->EffKindId, pEm, 0);
            EffectEspgenDelete(0, w->EffKindId, pEm);
            EffectEfmDelete(0, w->EffKindId, pEm);
            return;
        }
        if (GetWaterHeight(&pEm->pos, &wh2) && pEm->pos.y <= wh2) {
            pEm->pos.y = wh2;
            AtEffInfo* wi = EatMgr.getEffInfo(EAT_ET_WATER);
            if (wi) {
                EstSet(0, -1, &pEm->pos, 0, wi->eff0[0], (u8) wi->eff0[1], 0, ESP_CORE_KIND_NONE, 0, 0);
                w->Bomb_eff = (u8) wi->eff6[0];
                w->Bomb_est = (u8) wi->eff6[1];
                SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                AddWaterPower(pEm->pos, 0.5f);
            } else {
                EstSet(0, -1, &pEm->pos, 0, EFF_CORE, 0x3A, 0, ESP_CORE_KIND_NONE, 0, 0);
                w->Bomb_eff = 0;
                w->Bomb_est = 0x39;
                SndCall(5, 0x24, &pEm->pos, 0, 0, pEm);
                AddWaterPower(pEm->pos, 0.5f);
                w->Bomb_eff = 0;
                w->Bomb_est = 0x39;
            }
            w->Bomb_seid = 1;
            w->Bomb_seno = 0x17;
            EffectEspDelete(0, w->EffKindId, pEm, 0);
            EffectEspgenDelete(0, w->EffKindId, pEm);
            EffectEfmDelete(0, w->EffKindId, pEm);
            pEm->setLost();
            return;
        }
        PSVECSubtract(&pEm->pos, &pEm->pos_old, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        pEm->ang.x = -atan2f(d.y, len);
        pEm->ang.y = atan2f(d.x, d.z);
        pEm->ang.z = 0.0f;
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        pEm->partsWorldCalc();
    }
}

// Picks the homing target: the live, visible, targetable enemy (ids 0x10..0x3F minus animals /
// vehicles) closest to the flight direction (dot product above 0; mode 1 allows up to 135
// degrees off, mode 0 also requires 15000 units) with a clear line of sight. Keeps the current
// target while it lives.
void emMineSearchEm(cEmMine* pEm, int mode)
{
    EmMineWork* w = EMMINE_WK(pEm);
    Vec dir;
    Vec v;
    f32 best;
    f32 dot;
    u32 i;

    if (w->pEm_homing) {
        if (w->pEm_homing->hp > 0) {
            return;
        }
        w->pEm_homing = 0;
    }
#line 829 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&w->Spd, &dir);
    if (mode) {
        best = -0.7f;
    } else {
        best = 0.0f;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);
        cParts* parts;

        if (!e->isAlive()) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x3F) {
            continue;
        }
        if (!(e->be_flag & 2)) {
            continue;
        }
        switch (e->id) {
        case 0x21:
        case 0x24:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2E:
        case 0x3B:
        case 0x3D:
            continue;
        }
        if (mode == 0) {
            if ((pEm->pos.x - e->pos.x) * (pEm->pos.x - e->pos.x) + (pEm->pos.y - e->pos.y) * (pEm->pos.y - e->pos.y) +
                    (pEm->pos.z - e->pos.z) * (pEm->pos.z - e->pos.z) >
                225000000.0f) {
                continue;
            }
        }
        parts = e->getPartsPtr(0);
        PSVECSubtract(&parts->world, &pEm->pos, &v);
        if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
            w->pEm_homing = e;
            return;
        }
#line 877 "D:/Bio4/Prog/emmine.cpp"
        VECNormalize(&v, &v);
        dot = PSVECDotProduct(&dir, &v);
        if (dot < 0.0f) {
            continue;
        }
        if (dot < best) {
            continue;
        }
        if (EatMgr.hitCheck(&pEm->pos, &parts->world, 0, 0, 0, 0x404000) != 0) {
            continue;
        }
        w->pEm_homing = e;
        best = dot;
    }
}

// Turns Spd toward the homing target's parts 0 (by a fixed step about the perpendicular axis)
// while the target is visible, unobstructed, more than 100 units away and within 145 degrees.
void emMineHomingEm(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);
    Vec axis;
    Mtx m;
    Vec dir;
    Vec to;
    cParts* parts;
    f32 ang;

    if (w->pEm_homing == 0) {
        return;
    }
    parts = w->pEm_homing->getPartsPtr(0);
    if (EatMgr.hitCheck(&pEm->pos, &parts->world, 0, 0, 0, 0x404000)) {
        w->pEm_homing = 0;
        return;
    }
    if (!(w->pEm_homing->be_flag & 2)) {
        w->pEm_homing = 0;
        return;
    }
    if ((parts->world.x - pEm->pos.x) * (parts->world.x - pEm->pos.x) +
            (parts->world.y - pEm->pos.y) * (parts->world.y - pEm->pos.y) +
            (parts->world.z - pEm->pos.z) * (parts->world.z - pEm->pos.z) <
        10000.0f) {
        return;
    }
#line 932 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&w->Spd, &dir);
    PSVECSubtract(&parts->world, &pEm->pos, &to);
#line 936 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&to, &to);
    ang = acosf(PSVECDotProduct(&dir, &to));
    if (ang < 0.0017453292f) {
        return;
    }
    if (ang > 2.5307274f) {
        return;
    }
    ang = Muku2(0.0f, ang, PI / 16.0f);
    PSVECCrossProduct(&dir, &to, &axis);
    PSMTXRotAxisRad(m, &axis, ang);
    PSMTXMultVecSR(m, &w->Spd, &w->Spd);
    if (pG->debug_mode == 8) {
        Draw_line3d(&pEm->pos, &parts->world, 0xFFFFFFFF, 0);
    }
}

// Rno1 == 2: stuck to the scenery: mines beep (est 0x37 + SE 5 at the mine's nose) at a
// shrinking interval (17 -> 5 frames) and explode when Bomb_wait runs out; arrows fall instead.
void emMine_R1_Set(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        if (pEm->type == 2) {
            pEm->hp = 0;
        } else {
            pEm->hp = 1;
        }
        w->Bomb_wait = 150;
        w->Timer = 1;
        w->Timer2 = 17;
        pEm->r_no_2++;
        break;
    case 1:
        if (pEm->type != 2 && w->Timer != 0) {
            w->Timer--;
            if (w->Timer == 0) {
                Vec p;

                w->Timer = w->Timer2;
                w->Timer2--;
                if (w->Timer2 <= 4) {
                    w->Timer2 = 5;
                }
                EstSet(pEm, -1, 0, 0, EFF_CORE, 0x37, 0, ESP_CORE_KIND_NONE, pEm, 0);
                p.x = 0.0f;
                p.y = 0.0f;
                p.z = -250.0f;
                PSMTXMultVec(pEm->mat, &p, &p);
                SndCall(1, 5, &p, 0, 0, pEm);
            }
        }
        if (w->Bomb_wait != 0) {
            w->Bomb_wait--;
        } else {
            if (pEm->type == 2) {
                pEm->setFall();
            } else {
                pEm->setBomb();
            }
            return;
        }
        break;
    }
    RotMatrix(pEm->mat, &pEm->ang);
    TransMatrix(pEm->mat, &pEm->pos);
    ScaleMatrix(pEm->mat, &pEm->scale);
    pEm->partsMatCalc();
    pEm->partsWorldCalc();
}

// Rno1 == 3: sunk in water: hidden, then explodes (mine) after 150 frames.
void emMine_R1_SetWater(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        if (pEm->type == 2) {
            pEm->hp = 0;
        } else {
            pEm->hp = 1;
        }
        w->Bomb_wait = 150;
        pEm->be_flag &= ~2;
        pEm->r_no_2++;
        break;
    case 1:
        if (w->Bomb_wait == 0) {
            pEm->setBomb();
            return;
        }
        w->Bomb_wait--;
        break;
    }
    RotMatrix(pEm->mat, &pEm->ang);
    TransMatrix(pEm->mat, &pEm->pos);
    ScaleMatrix(pEm->mat, &pEm->scale);
    pEm->partsMatCalc();
    pEm->partsWorldCalc();
}

// Rno1 == 4: stuck to enemy `pEm_oya` parts `oya_parts`: follows the parts matrix, beeps like
// Set, and explodes (mine) / falls (arrow) when the enemy dies, is hidden, or the timer runs out;
// a vanished enemy loses the mine.
void emMine_R1_Parent(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);
    cEm* parent = w->pEm_oya;

    if (!parent->isAlive()) {
        w->pEm_oya = 0;
        parent = 0;
    }
    if (parent == 0) {
        pEm->setLost();
    }
    switch (pEm->r_no_2) {
    case 0:
        if (pEm->type == 2) {
            pEm->hp = 0;
        } else {
            pEm->hp = 1;
        }
        w->Bomb_wait = 150;
        w->Timer = 1;
        w->Timer2 = 17;
        pEm->r_no_2++;
        break;
    case 1:
        if (w->Timer != 0 && pEm->type != 2) {
            w->Timer--;
            if (w->Timer == 0) {
                Vec p;

                w->Timer = w->Timer2;
                w->Timer2--;
                if (w->Timer2 <= 4) {
                    w->Timer2 = 5;
                }
                EstSet(pEm, -1, 0, 0, EFF_CORE, 0x37, 0, ESP_CORE_KIND_NONE, pEm, 0);
                p.x = 0.0f;
                p.y = 0.0f;
                p.z = -250.0f;
                PSMTXMultVec(pEm->mat, &p, &p);
                SndCall(1, 5, &p, 0, 0, pEm);
            }
        }
        if (!(parent->be_flag & 2)) {
            w->Bomb_wait = 0;
        }
        if (w->Bomb_wait == 0) {
            if (pEm->type != 2) {
                pEm->setBomb();
                return;
            }
            pEm->setFall();
            return;
        }
        w->Bomb_wait--;
        break;
    }
    RotMatrix(pEm->mat, &pEm->ang);
    TransMatrix(pEm->mat, &pEm->pos);
    ScaleMatrix(pEm->mat, &pEm->scale);
    if (parent) {
        if (parent->hp <= 0) {
            if (pEm->type != 2) {
                pEm->setBomb();
                return;
            }
            if (w->Bomb_wait > 30) {
                w->Bomb_wait = 30;
            }
        }
        if (parent->pList) {
            Mtx m;
            Vec v0;
            Vec v1;
            Vec v2;

            PSMTXConcat(parent->getPartsPtr(w->oya_parts)->mat, pEm->mat, m);
            v0.x = m[0][0];
            v0.y = m[1][0];
            v0.z = m[2][0];
            v1.x = m[0][1];
            v1.y = m[1][1];
            v1.z = m[2][1];
            v2.x = m[0][2];
            v2.y = m[1][2];
            v2.z = m[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 1161 "D:/Bio4/Prog/emmine.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 1163 "D:/Bio4/Prog/emmine.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 1165 "D:/Bio4/Prog/emmine.cpp"
            VECNormalize(&v2, &v2);
            m[0][0] = v0.x;
            m[1][0] = v0.y;
            m[2][0] = v0.z;
            m[0][1] = v1.x;
            m[1][1] = v1.y;
            m[2][1] = v1.z;
            m[0][2] = v2.x;
            m[1][2] = v2.y;
            m[2][2] = v2.z;
            PSMTXCopy(m, pEm->mat);
        }
    }
    pEm->partsMatCalc();
    pEm->partsWorldCalc();
}

// Rno1 == 5: 4 frame fuse for a mine stuck to the scenery; then moves the blast 1000 units out
// along the surface normal and detonates (setBomb without Norm_ck).
void emMine_R1_BombWait(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);
    Vec v;

    switch (pEm->r_no_2) {
    case 0:
        w->Timer = 4;
        pEm->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            break;
        }
        pEm->hp = 0;
        pEm->pos.x = pEm->mat[0][3];
        pEm->pos.y = pEm->mat[1][3];
        pEm->pos.z = pEm->mat[2][3];
        if (w->Norm_ck) {
            PSVECScale(&w->Norm, &v, 1000.0f);
            PSVECAdd(&pEm->pos, &v, &pEm->pos);
            TransMatrix(pEm->mat, &pEm->pos);
        }
        w->Norm_ck = 0;
        pEm->setBomb();
        break;
    }
}

// Rno1 == 6: 2 frames after the explosion effect: applies the blast damage (PlWepHitCheck2 type
// 0x13) with radius 2000 / 4000 / 6000 by firepower level at the mine's nose, then Lost.
void emMine_R1_BombWait2(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);
    Vec p;
    f32 r;

    switch (pEm->r_no_2) {
    case 0:
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        w->Timer = 2;
        pEm->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            break;
        }
        pEm->hp = 0;
        p.x = 0.0f;
        p.y = 0.0f;
        p.z = -250.0f;
        PSMTXMultVec(pEm->mat, &p, &p);
        switch (w->Lv) {
        case 0:
            r = 2000.0f;
            break;
        case 1:
            r = 4000.0f;
            break;
        default:
            r = 6000.0f;
            break;
        }
        PlWepHitCheck2(0, &p, &p, 0x13, 0, r);
        pEm->setLost();
        break;
    }
}

// Rno1 == 7: the arrow drops as a 3-node rope (gravity `grav`, floor contact 50 above the effect
// collision floor, water splash once), matrix rebuilt from the nodes; lost when it comes to rest.
void emMine_R1_Fall(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);
    Vec ofs[4] = { { 0.0f, 0.0f, 600.0f }, { 0.0f, 0.0f, -600.0f }, { 300.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    MineNode node[3];
    Vec e1;
    Vec nrm;
    Vec e0;
    Vec d;
    MineNode* n;
    MineNode* nn;
    f32 floor;
    u32 i;
    u32 k;
    f32 mag;
    f32 dd;

    pEm->hp = 0;
    pEm->setStatus(EM_STATUS_LOCKOFF);
    floor = EatMgr.getFloor(&pEm->pos, 0, 600.0f, 100000.0f, 0) + 50.0f;
    // COMPILER-DIFF: candidate (gcse PRE pseudo numbering): two dead sets (deleted by flow) take the
    // expression table from 235 to 237 buckets, so `w+48`/`fp+100` (13389) hash below `fp+144` (13433)
    // and the three PRE'd addresses get the original's spill-slot order (256/260/264).
    i = 5; i = 6;
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.x = w->Fall_spd[i].x;
        n->spd.y = w->Fall_spd[i].y;
        n->spd.z = w->Fall_spd[i].z;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        PSMTXMultVec(pEm->mat, &ofs[i], &n->pos);
        n->old = n->pos;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        if (i == 2) {
            nn = &node[0];
        } else {
            nn = &node[i + 1];
        }
        n->len = GetDistance3(&n->pos, &nn->pos);
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.y -= w->Gravity;
        PSVECAdd(&n->pos, &n->spd, &n->pos);
        n->reflect = 0;
    }
    for (k = 0; k < 30; k++) {
        for (i = 0; i < 3; i++) {
            n = &node[i];
            if (i == 2) {
                nn = &node[0];
            } else {
                nn = &node[i + 1];
            }
            PSVECSubtract(&nn->pos, &n->pos, &d);
            // `mag` is assigned again after the loops (the speed test), so the call result is not
            // tied to it (`fmr f12, f1`); `dd` keeps the (len - mag) * 0.5 chain in f1 (emtree).
            mag = PSVECMag(&d);
            dd = (n->len - mag) * 0.5f;
            PSVECScale(&d, &d, (1.0f / mag) * dd);
            PSVECAdd(&nn->pos, &d, &nn->pos);
            PSVECSubtract(&n->pos, &d, &n->pos);
            if (n->pos.y < floor) {
                n->pos.y = floor;
                n->reflect = 1;
            }
            if (nn->pos.y < floor) {
                nn->pos.y = floor;
                nn->reflect = 1;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        if (n->reflect) {
            n->spd.x *= fRand0_1() * 0.2f + 0.5f;
            n->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
            n->spd.z *= fRand0_1() * 0.2f + 0.5f;
            if (n->spd.y <= w->Gravity) {
                if (n->spd.y > 0.0f) {
                    n->spd.y = 0.0f;
                }
            }
        } else {
            PSVECSubtract(&n->pos, &n->old, &n->spd);
        }
        PSVECScale(&n->spd, &n->spd, 0.999f);
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        w->Fall_spd[i].x = n->spd.x;
        w->Fall_spd[i].y = n->spd.y;
        w->Fall_spd[i].z = n->spd.z;
    }
    PSVECSubtract(&node[0].pos, &node[1].pos, &e0);
    PSVECSubtract(&node[2].pos, &node[1].pos, &e1);
    PSVECCrossProduct(&e0, &e1, &nrm);
    PSVECCrossProduct(&nrm, &e0, &e1);
#line 1442 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&e1, &e1);
#line 1443 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&nrm, &nrm);
#line 1444 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&e0, &e0);
    pEm->mat[0][0] = e1.x;
    pEm->mat[1][0] = e1.y;
    pEm->mat[2][0] = e1.z;
    pEm->mat[0][1] = nrm.x;
    pEm->mat[1][1] = nrm.y;
    pEm->mat[2][1] = nrm.z;
    pEm->mat[0][2] = e0.x;
    pEm->mat[1][2] = e0.y;
    pEm->mat[2][2] = e0.z;
    PSVECScale(&ofs[0], &d, -1.0f);
    TransMatrix(pEm->mat, &node[0].pos);
    PSMTXMultVec(pEm->mat, &d, &d);
    TransMatrix(pEm->mat, &d);
    pEm->pos = d;
    mag = node[0].spd.x * node[0].spd.x + node[0].spd.y * node[0].spd.y + node[0].spd.z * node[0].spd.z
        + node[1].spd.x * node[1].spd.x + node[1].spd.y * node[1].spd.y + node[1].spd.z * node[1].spd.z
        + node[2].spd.x * node[2].spd.x + node[2].spd.y * node[2].spd.y + node[2].spd.z * node[2].spd.z;
    if (mag < 25.0f) {
        pEm->pos.x = pEm->mat[0][3];
        pEm->pos.y = pEm->mat[1][3];
        pEm->pos.z = pEm->mat[2][3];
        Matrix2AxisAngle(pEm->mat, &pEm->ang);
        pEm->setLost();
    }
    pEm->partsWorldCalc();
    if (w->Water_ck == 0) {
        if (CheckInWater(pEm, 0)) {
            SndCall(6, 0x17, &pEm->pos, 0, 0, pEm);
            w->Water_ck = 1;
        }
    }
}

// Rno1 == 8: removes the work: hidden, trail effects deleted, destroyed.
void emMine_R1_Lost(cEmMine* pEm)
{
    EmMineWork* w = EMMINE_WK(pEm);

    if (pEm->r_no_2 == 0) {
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        EffectEspDelete(0, w->EffKindId, pEm, 0);
        EffectEspgenDelete(0, w->EffKindId, pEm);
        EffectEfmDelete(0, w->EffKindId, pEm);
        pEm->r_no_2++;
        EmMgr.destroy(pEm);
    }
}

// Sticks the mine / arrow to `parent` parts `partsNo_` (Rno1 4).
void cEmMine::setParent(cEm* parent, int partsNo_)
{
    EmMineWork* w = EMMINE_WK(this);

    w->pEm_oya = parent;
    w->oya_parts = partsNo_;
    r_no_0 = 1;
    r_no_1 = 4;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Hides the mine and goes to Lost.
void cEmMine::setLost()
{
    hp = 0;
    be_flag &= ~2;
    r_no_0 = 1;
    r_no_1 = 8;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Detonates: a mine stuck to a surface first goes through BombWait (Rno1 5); otherwise spawns
// the explosion est / SE at the nose, splashes water, deletes the trail, flags the explosion
// (Status_flg[0] 0x800000, Status_flg[1] 0x20000000) and stores the blast position as the noise
// source (SeInfo.pos / SeInfo.type, alerts enemies), then BombWait2 for the damage.
void cEmMine::setBomb()
{
    EmMineWork* w = EMMINE_WK(this);
    Vec p;
    int hit;

    hp = 0;
    hit = w->Norm_ck;
    if (hit) {
        r_no_0 = 1;
        r_no_1 = 5;
        r_no_2 = 0;
        r_no_3 = 0;
        return;
    }
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = -250.0f;
    PSMTXMultVec(mat, &p, &p);
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    EstSet(0, -1, &pos, 0, w->Bomb_eff, w->Bomb_est, 0, ESP_CORE_KIND_NONE, 0, 0);
    SndCall(w->Bomb_seid, w->Bomb_seno, &p, 0, 0, this);
    AddWaterPower(pos, 1.0f);
    EffectEspDelete(0, w->EffKindId, this, 0);
    EffectEspgenDelete(0, w->EffKindId, this);
    EffectEfmDelete(0, w->EffKindId, this);
    StaFlagOn(pG, STA_PL_FIRE);
    StaFlagOn(pG, STA_SE_BURST);
    pG->SeInfo.pos = p;
    pG->SeInfo.type = 1;
    setLost();
    // COMPILER-DIFF: candidate #12 (cse wider-mode zero fold): the original stores the known-zero
    // `hit` register into xFE (ours folds it to the HImode zero pseudo); the launder keeps `hit` as
    // the store source, the codeless keep-alive after the stores keeps `hit`/`this` from dying at
    // the xFE store (weight -1 would issue it first; the target has it last, source order).
    asm("" : "+r"(hit));
    r_no_3 = 0;
    r_no_0 = 1;
    r_no_1 = 6;
    r_no_2 = hit;
    asm("" : "=m"(hp) : "r"(hit));
}

// Starts the arrow's fall (Rno1 7): random upward node speeds, gravity 15, detached.
void cEmMine::setFall()
{
    EmMineWork* w = EMMINE_WK(this);
    u32 i;

    Motion.pMot = 0;
    for (i = 0; i < 3; i++) {
        w->Fall_spd[i].x = fRand1_1() * 10.0f;
        w->Fall_spd[i].y = fRand1_1() * 10.0f + 50.0f;
        w->Fall_spd[i].z = fRand1_1() * 10.0f;
    }
    w->pEm_oya = 0;
    w->pEm_old = 0;
    hp = 0;
    w->Gravity = 15.0f;
    w->Water_ck = 0;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &ang);
    r_no_0 = 1;
    r_no_1 = 7;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Enemy hit test along the last move (GetWepTargetList2 with weapon type 0xE mine / 0x1C
// arrow): registers the hit on the enemy's damage info, embeds the projectile in the hit part
// (aimed 50 units back along the hit direction; objects use the flight direction), doubles its
// scale, plays the stick SE (0x50 on objects, 0x54 for arrows on flesh) and parents it. 1 when
// something was hit.
int emMineHitCk(cEmMine* pEm)
{
    Vec hit;
    Vec nrm;
    Mtx inv;
    Vec dir;
    Vec a;
    Vec b;
    WepTarget list;
    u32 attr;
    cEm* hitEm;
    YARARE_INFO* part;
    int type;
    int partsNo;
    f32 len;

    type = 0xE;
    if (pEm->type == 2) {
        type = 0x1C;
    }
    if (GetWepTargetList2(&pEm->pos_old, &pEm->pos, &list, 1, &hit, &nrm, &attr, type, 0) != 0) {
        hitEm = list.em;
        part = list.part;
        hitEm->dmg.set(0, 2, type, &pEm->pos_old, part->len, part);
        if (part->flag & YAT_FLAG_DMPOS) {
            partsNo = 0;
            if (part->parts_no != 0) {
                partsNo = part->parts_no - 1;
            }
            PSMTXInverse(hitEm->getPartsPtr(partsNo)->mat, inv);
            PSMTXMultVec(inv, &part->cross, &pEm->pos);
#line 1723 "D:/Bio4/Prog/emmine.cpp"
            VECNormalize(&pEm->pos, &dir);
            PSVECScale(&dir, &dir, -50.0f);
            PSVECAdd(&pEm->pos, &dir, &pEm->pos);
            switch (hitEm->id) {
            default:
                len = SQRTF(pEm->pos.x * pEm->pos.x + pEm->pos.z * pEm->pos.z);
                pEm->ang.x = -atan2f(-pEm->pos.y, len);
                pEm->ang.y = atan2f(-pEm->pos.x, -pEm->pos.z);
                pEm->ang.z = 0.0f;
                break;
            case 0x40:
            case 0x41:
            case 0x43:
            case 0x44:
            case 0x45:
            case 0x46:
            case 0x47:
            case 0x48:
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
            case 0x4D:
            case 0x4E:
            case 0x50:
            case 0x51:
                a.x = 0.0f;
                a.y = 0.0f;
                a.z = 0.0f;
                b.x = 0.0f;
                b.y = 0.0f;
                b.z = 1.0f;
                PSMTXMultVec(pEm->mat, &a, &a);
                PSMTXMultVec(pEm->mat, &b, &b);
                PSMTXMultVec(inv, &a, &a);
                PSMTXMultVec(inv, &b, &b);
                PSVECSubtract(&b, &a, &dir);
                len = SQRTF(dir.x * dir.x + dir.z * dir.z);
                pEm->ang.x = -atan2f(dir.y, len);
                pEm->ang.y = atan2f(dir.x, dir.z);
                pEm->ang.z = 0.0f;
                break;
            }
        } else {
            partsNo = 0;
            pEm->pos.x = 0.0f;
            pEm->pos.y = 0.0f;
            pEm->pos.z = 0.0f;
            pEm->ang.x = 0.0f;
            pEm->ang.y = 0.0f;
            pEm->ang.z = 0.0f;
        }
        pEm->scale.x = 2.0f;
        pEm->scale.y = 2.0f;
        pEm->scale.z = 2.0f;
        switch (hitEm->id) {
        default:
            if (pEm->type == 2) {
                SndCall(1, 0x54, &pEm->pos, 0, 0, pEm);
            }
            break;
        case 0x2A:
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51:
            SndCall(1, 0x50, &pEm->pos, 0, 0, pEm);
            break;
        }
        pEm->setParent(hitEm, partsNo);
        emMine_R1_Parent(pEm);
        return 1;
    }
    return 0;
}
