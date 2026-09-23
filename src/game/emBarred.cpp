// game/emBarred.cpp: barred gate enemy (cEmBarred): iron gates that rise for the player when he
// stands near, drop back shut, and can be shot open (type 6).
//
// SetEmBarred: the `em->hpMax = em->hp = 1000` pair is written in every YarareInitCube arm (the
// original's source shape): with the store after each call no arm ends in a CALL_INSN, so flow's
// `(use (const_int 0))` nop never blocks the fall-through cross-jump and jump2 merges every arm into
// case 4's `lfs f6` + call + store tail (then 8/9 into the default arm's 140.0 `lfs f6`). A single
// `hp = 1000` after the switch left case 4's call block ending in the call (69 words). The atari
// init interleave is COMPILER-DIFF #1 (atari_init.h AtariInit).
// emBarredEatSet only differs in two `lis 0x8023` words the split object carries without a
// relocation (their `lfs` sits in another block); the linked bytes are identical.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "emBarred.h"
#include "emhit.h"
#include "etc_model.h"
#include "at_mod.h"
#include "player.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "pl_npc.h"


typedef void (*EmBarredFunc)(cEmBarred*);

EmBarredFunc EmBarred_R1_move_tbl[4] = {
    emBarred_R1_Set,
    emBarred_R1_Open,
    emBarred_R1_Close,
    emBarred_R1_Break,
};


// Creates a barred gate enemy (id 0x4E) from a model / TPL at pos / rot unless room etc flag
// `flagNo` bit0 says it was destroyed. type 1..9 selects the gate size (atari cylinder, hit
// boxes, Height / Width of the lifting frame): 5 / 6 / 8 / 9 are the automatic gates that rise
// when someone comes near (start closed, Status 2), the others start open (Status 1) and are
// scripted; type 6 also has shootable bars (Rno1 3 Break). 1000 hp. NULL on failure.
cEmBarred* SetEmBarred(void* bin, void* tpl, Vec* pos, Vec* rot, int flagNo, int type)
{
    cEmBarred* em;
    EmBarredWork* w;
    u16* flg;
    cModel* parts;
    u32 i;

    flg = GetEtcFlgPtr(flagNo, pG->room_id);
    if (flg != 0 && (*flg & 1)) {
        return 0;
    }
    em = (cEmBarred*) EmMgr.create(0x4E);
    if (em == 0) {
        return 0;
    }
    w = EMBARRED_WK(em);
    w->Etc_no = flagNo;
    em->type = type;
    if (type == 6) {
        em->ot_type = 1;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetEmBarred() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 3000.0f, 3000.0f, 3000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    switch (em->type) {
    case 1:
    case 5:
    case 6:
    default:
        AtariInit(&em->atari, 0.0f, 1200.0f, 0.0f, 800.0f, 120.0f, 120.0f, 1200.0f, 0, 2, 0);
        break;
    case 8:
        AtariInit(&em->atari, 0.0f, 1300.0f, 0.0f, 850.0f, 120.0f, 120.0f, 1300.0f, 0, 2, 0);
        break;
    case 9:
        AtariInit(&em->atari, 0.0f, 1550.0f, 0.0f, 750.0f, 120.0f, 120.0f, 1550.0f, 0, 2, 0);
        break;
    case 2:
        AtariInit(&em->atari, 0.0f, 1300.0f, 0.0f, 800.0f, 120.0f, 120.0f, 1300.0f, 0, 2, 0);
        break;
    case 3:
        AtariInit(&em->atari, 0.0f, 2400.0f, 0.0f, 4150.0f, 230.0f, 230.0f, 2400.0f, 0, 2, 0);
        break;
    case 4:
        AtariInit(&em->atari, 0.0f, 1500.0f, 0.0f, 1500.0f, 230.0f, 230.0f, 1500.0f, 0, 2, 0);
        break;
    case 7:
        AtariInit(&em->atari, 0.0f, 1750.0f, 0.0f, 1950.0f, 230.0f, 230.0f, 1750.0f, 0, 2, 0);
        break;
    }
    em->atari.clrFlag100();
    em->atari.setPriority(PRI_LV3);
    em->setNoSuspend(1);
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    switch (em->type) {
    case 1:
    case 5:
    default:
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 800.0f, 2400.0f, 140.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        em->hp_max = em->hp = 1000;
        break;
    case 8:
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 850.0f, 2600.0f, 140.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        em->hp_max = em->hp = 1000;
        break;
    case 9:
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 750.0f, 3100.0f, 140.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        em->hp_max = em->hp = 1000;
        break;
    case 6:
        YarareInitCube(em, 0.0f, 260.0f, 0.0f, 490.0f, 1880.0f, 140.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        YarareAddCube(em, &w->hit[0], -490.0f, 0.0f, 0.0f, 80.0f, 2400.0f, 140.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        YarareAddCube(em, &w->hit[1], 490.0f, 0.0f, 0.0f, 80.0f, 2400.0f, 140.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        YarareAddCube(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 650.0f, 260.0f, 140.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        YarareAddCube(em, &w->hit[3], 0.0f, 2040.0f, 0.0f, 650.0f, 260.0f, 140.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        em->hp_max = em->hp = 1000;
        break;
    case 2:
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 800.0f, 2700.0f, 120.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        em->hp_max = em->hp = 1000;
        break;
    case 3:
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 4150.0f, 4800.0f, 250.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        em->hp_max = em->hp = 1000;
        break;
    case 4:
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 1650.0f, 3150.0f, 250.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        em->hp_max = em->hp = 1000;
        break;
    case 0:
    case 7:
        em->hp_max = em->hp = 1000;
        break;
    }
    if (pos) {
        em->pos = *pos;
    } else {
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
    }
    em->pos_old = em->pos;
    if (rot) {
        em->ang = *rot;
    }
    w->pos0 = em->pos;
    switch (em->type) {
    case 0:
    case 1:
    case 5:
    case 6:
    default:
        w->Height = 2300.0f;
        break;
    case 8:
        w->Height = 2600.0f;
        break;
    case 9:
        w->Height = 3100.0f;
        break;
    case 2:
        w->Height = 2600.0f;
        break;
    case 3:
        w->Height = 3800.0f;
        break;
    case 4:
        w->Height = 3000.0f;
        break;
    case 7:
        w->Height = 3500.0f;
        break;
    }
    switch (em->type) {
    case 0:
        w->Width = 675.0f;
        break;
    case 1:
        w->Width = 675.0f;
        break;
    case 8:
        w->Width = 875.0f;
        break;
    case 9:
        w->Width = 875.0f;
        break;
    case 2:
        w->Width = 750.0f;
        break;
    case 3:
        w->Width = 4150.0f;
        break;
    case 4:
        w->Width = 1500.0f;
        break;
    case 7:
        w->Width = 2000.0f;
        break;
    case 5:
        w->Width = 675.0f;
        break;
    case 6:
        w->Width = 675.0f;
        break;
    default:
        w->Width = 675.0f;
        break;
    }
    w->pEat = 0;
    for (i = 0; i < 4; i++) {
        w->pEatFrame[i] = 0;
    }
    w->Eff_id = 0xFF;
    if (flg && (*flg & 2)) {
        parts = em->getPartsPtr(1);
        em->hitInfo.flag &= ~1;
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
    }
    w->Status = 1;
    w->Lock_mode = 0;
    w->Open_flag = 1;
    w->pBarred = 0;
    if (em->type == 5 || em->type == 6 || em->type == 8 || em->type == 9) {
        w->Status = 2;
        w->Open_flag = 0;
    }
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    emBarredEatSet(em);
    return em;
}

// Weapon hit reaction: on the type 6 gate a hit on the bar hit box (hitInfo) knocks the bar out
// (est 3 of Eff_id facing the shooter, parts 1 hidden, SE, etc flag bit1); otherwise plays the
// spark est (1 near / 0 far) by weapon class.
void emBarredDmCk(cEmBarred* pEm)
{
    EmBarredWork* w = EMBARRED_WK(pEm);
    YARARE_INFO* part;
    cModel* parts;
    u16* flg;
    Vec v;
    f32 ang;
    int near;
    u8 wep;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    part = pEm->dmg.m_pDamageYarare;
    pEm->dmg.m_Flag = 0;
    near = 0;
    if (part->len < 36000000.0f) {
        near = 1;
    }
    wep = pEm->dmg.m_Wep;
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
    if (wep == 0xE) {
        return;
    }
    pEm->dmg.m_Timer = 1;
    if (wep == 0x10) {
        pEm->dmg.m_Timer = 0x11;
    }
    if (pEm->type == 6 && part == &pEm->hitInfo && w->Eff_id != 0xFF) {
        ang = Muku(&pEm->pos, &pEm->dmg.m_PosFrom, pEm->ang.y, PI);
        if (fabsf(ang) < PI / 2) {
            ang = pEm->ang.y;
        } else {
            ang = pEm->ang.y + PI;
        }
        v.y = LIMIT_ANGLE(ang);
        v.x = 0.0f;
        v.z = 0.0f;
        parts = pEm->getPartsPtr(1);
        EstSet(0, -1, &pEm->pos, &v, w->Eff_id, 3, 0, ESP_CORE_KIND_NONE, 0, 0);
        part->flag &= ~1;
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        SndCall(6, 0x3B, &pEm->pos, 0, 0, pEm);
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 2;
        }
    } else {
        switch (pEm->dmg.m_Wep) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 0xB:
        case 0xC:
        case 0x10:
        case 0x11:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x2B:
            if (w->Eff_id != 0xFF) {
                EmDmBloodSet2(pEm, w->Eff_id, 0, 0, 0, 0);
            }
            break;
        case 7:
        case 8:
        case 0x21:
            if (w->Eff_id != 0xFF) {
                if (near) {
                    EmDmBloodSet2(pEm, w->Eff_id, 1, 0, 0, 0);
                } else {
                    EmDmBloodSet2(pEm, w->Eff_id, 0, 0, 0, 0);
                }
            }
            break;
        case 5:
        case 6:
        case 9:
        case 0xA:
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x28:
        case 0x29:
        case 0x2C:
        case 0x2D:
            if (w->Eff_id != 0xFF) {
                EmDmBloodSet2(pEm, w->Eff_id, 0, 0, 0, 0);
            }
            break;
        case 0x14:
        case 0x15:
        default:
            break;
        }
    }
}

// Per-frame: hit check, the Rno1 routine (0 Set, 1 Open, 2 Close, 3 Break), the model-vs-player
// atari and the effect collision quads of the frame / bars.
void cEmBarred::move()
{
    emBarredDmCk(this);
    EmBarred_R1_move_tbl[r_no_1](this);
    EmAtCheck(this);
    atari.move();
    emBarredEatSet(this);
}

// Script entry: starts raising the gate (Rno1 1) unless already open / opening / broken; mode
// (Rno3) 1 = silent (the paired gate of setDouble).
void cEmBarred::setOpen(int mode)
{
    EmBarredWork* w = EMBARRED_WK(this);

    if (w->Status == 1) {
        return;
    }
    if (r_no_1 == 1) {
        return;
    }
    if (r_no_1 == 3) {
        return;
    }
    w->Status = 0;
    w->Open_flag = 1;
    r_no_0 = 1;
    r_no_1 = 1;
    r_no_2 = 0;
    r_no_3 = mode;
}

// Script entry: starts lowering the gate (Rno1 2) unless already closed / closing / broken.
void cEmBarred::setClose(int mode)
{
    EmBarredWork* w = EMBARRED_WK(this);

    if (w->Status == 2) {
        return;
    }
    if (r_no_1 == 2) {
        return;
    }
    if (r_no_1 == 3) {
        return;
    }
    w->Status = 0;
    w->Open_flag = 0;
    r_no_0 = 1;
    r_no_1 = 2;
    r_no_2 = 0;
    r_no_3 = mode;
}

// Script entry: snaps the gate to fully open (2500 above pos0) without animation.
void cEmBarred::setOpened()
{
    EmBarredWork* w = EMBARRED_WK(this);

    if (r_no_1 != 3) {
        w->Status = 1;
        w->Open_flag = 1;
        pos.y = w->pos0.y + 2500.0f;
        SndStop(w->Seid, 0);
        r_no_0 = 1;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 0;
    }
}

// Script entry: snaps the gate shut at pos0.
void cEmBarred::setClosed()
{
    EmBarredWork* w = EMBARRED_WK(this);

    if (r_no_1 != 3) {
        w->Status = 2;
        w->Open_flag = 0;
        pos = w->pos0;
        SndStop(w->Seid, 0);
        r_no_0 = 1;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 0;
    }
}

// Lock_mode != 0 disables the automatic open / close of the proximity gates.
void cEmBarred::setLockMode(u8 mode)
{
    EMBARRED_WK(this)->Lock_mode = mode;
}

// Gate status: 0 moving, 1 open, 2 closed.
int cEmBarred::ckStatus()
{
    return EMBARRED_WK(this)->Status;
}

// 1 when the gate is open or opening (Open_flag).
int cEmBarred::ckOpen()
{
    if (EMBARRED_WK(this)->Open_flag) {
        return 1;
    }
    return 0;
}

// Rno1 == 0: resting gate. The proximity types (5 / 6 / 8 / 9) open when the player or a live
// character comes near (emBarredNearCk) and close again 30 frames after everyone left, unless
// locked or setNoClose; a setDouble partner is driven along.
void emBarred_R1_Set(cEmBarred* pEm)
{
    EmBarredWork* w = EMBARRED_WK(pEm);

    pEm->matUpdate();
    if (pEm->r_no_2 == 0) {
        w->Timer = 0;
        pEm->r_no_2++;
    }
    if (pEm->type == 5 || pEm->type == 6 || pEm->type == 8 || pEm->type == 9) {
        if (emBarredNearCk(pEm)) {
            w->Timer = 0;
            if (w->Status != 1 && w->Lock_mode == 0) {
                w->Open_flag = 1;
                w->Status = 0;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 1;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
                if (w->pBarred) {
                    w->pBarred->setOpen(1);
                }
            }
        } else if ((!(w->be_flag & 1))) {
            w->Timer++;
            if (w->Status == 1 && w->Timer > 30 && w->Lock_mode == 0) {
                w->Status = 0;
                w->Open_flag = 0;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 2;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
                if (w->pBarred) {
                    w->pBarred->setClose(1);
                }
            }
        }
    }
}

// Rno1 == 1: raises the gate: chain SE (unless Rno3 1), then pos.y climbs 100 / frame (50 for
// type 4; types 5 / 6 slide sideways instead) to pos0.y + Height, a 5 frame rattle, then Status 1.
void emBarred_R1_Open(cEmBarred* pEm)
{
    EmBarredWork* w = EMBARRED_WK(pEm);
    Vec v;
    f32 d;

    w->Status = 0;
    w->Open_flag = 1;
    switch (pEm->r_no_2) {
    case 0:
        SndStop(w->Seid, 0);
        if (pEm->r_no_3 == 0) {
            switch (pEm->type) {
            case 5:
            case 6:
            case 8:
            case 9:
                w->Seid = SndCall(6, 0xF, &pEm->pos, 0, 0, pEm);
                break;
            default:
                w->Seid = SndCall(6, 0x24, &pEm->pos, 0, 0, pEm);
                break;
            }
        }
        pEm->r_no_2++;
    case 1:
        switch (pEm->type) {
        case 0xA:
        default:
            if (pEm->type == 4) {
                pEm->pos.y += 50.0f;
            } else {
                pEm->pos.y += 100.0f;
            }
            if (pEm->pos.y > w->pos0.y + w->Height) {
                pEm->pos.y = w->pos0.y + w->Height;
                pEm->r_no_2++;
            }
            break;
        case 5:
        case 6:
            v.x = 100.0f;
            v.y = 0.0f;
            v.z = 0.0f;
            PSMTXMultVecSR(pEm->mat, &v, &v);
            PSVECAdd(&pEm->pos, &v, &pEm->pos);
            d = (pEm->pos.x - w->pos0.x) * (pEm->pos.x - w->pos0.x) + (pEm->pos.y - w->pos0.y) * (pEm->pos.y - w->pos0.y) + (pEm->pos.z - w->pos0.z) * (pEm->pos.z - w->pos0.z);
            if (d > 1690000.0f) {
                w->Status = 1;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 0;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            }
            break;
        case 8:
            v.x = 100.0f;
            v.y = 0.0f;
            v.z = 0.0f;
            PSMTXMultVecSR(pEm->mat, &v, &v);
            PSVECAdd(&pEm->pos, &v, &pEm->pos);
            d = (pEm->pos.x - w->pos0.x) * (pEm->pos.x - w->pos0.x) + (pEm->pos.y - w->pos0.y) * (pEm->pos.y - w->pos0.y) + (pEm->pos.z - w->pos0.z) * (pEm->pos.z - w->pos0.z);
            if (d > 2890000.0f) {
                w->Status = 1;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 0;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            }
            break;
        case 9:
            v.x = 100.0f;
            v.y = 0.0f;
            v.z = 0.0f;
            PSMTXMultVecSR(pEm->mat, &v, &v);
            PSVECAdd(&pEm->pos, &v, &pEm->pos);
            d = (pEm->pos.x - w->pos0.x) * (pEm->pos.x - w->pos0.x) + (pEm->pos.y - w->pos0.y) * (pEm->pos.y - w->pos0.y) + (pEm->pos.z - w->pos0.z) * (pEm->pos.z - w->pos0.z);
            if (d > 2890000.0f) {
                w->Status = 1;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 0;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            }
            break;
        }
        break;
    case 2:
        if (pEm->r_no_3 == 0) {
            SndStop(w->Seid, 0);
            w->Seid = SndCall(6, 0x25, &pEm->pos, 0, 0, pEm);
        }
        w->Timer = 5;
        pEm->r_no_2++;
    case 3:
        pEm->pos.x = fRand1_1() * 20.0f + w->pos0.x;
        pEm->pos.y = fRand0_1() * 20.0f + (w->pos0.y + w->Height);
        pEm->pos.z = fRand1_1() * 20.0f + w->pos0.z;
        if (w->Timer != 0) {
            w->Timer--;
        } else {
            pEm->pos = w->pos0;
            pEm->pos.y = w->pos0.y + w->Height;
            w->Status = 1;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
        }
        break;
    }
    pEm->matUpdate();
}

// Rno1 == 2: drops the gate with gravity (spd -10 / -15 per frame) to pos0.y; anyone under it
// (emBarredUnderCk) is hit; slam SE, a short rattle, then Status 2.
void emBarred_R1_Close(cEmBarred* pEm)
{
    EmBarredWork* w = EMBARRED_WK(pEm);
    Vec d;

    w->Status = 0;
    w->Open_flag = 0;
    switch (pEm->r_no_2) {
    case 0:
        if (pEm->type == 4) {
            w->TmpF = -10.0f;
        } else {
            w->TmpF = -50.0f;
        }
        SndStop(w->Seid, 0);
        if (pEm->r_no_3 == 0) {
            switch (pEm->type) {
            case 5:
            case 6:
            case 8:
            case 9:
                w->Seid = SndCall(6, 0xF, &pEm->pos, 0, 0, pEm);
                break;
            default:
                w->Seid = SndCall(6, 0x24, &pEm->pos, 0, 0, pEm);
                break;
            }
        }
        pEm->r_no_2++;
    case 1:
        switch (pEm->type) {
        default:
            pEm->pos.y += w->TmpF;
            pEm->pos.y += w->TmpF;
            if (pEm->type == 4) {
                w->TmpF -= 10.0f;
            } else {
                w->TmpF -= 15.0f;
            }
            if (pEm->pos.y < w->pos0.y) {
                pEm->pos.y = w->pos0.y;
                pEm->r_no_2++;
            }
            break;
        case 5:
        case 6:
        case 8:
        case 9:
            PSVECSubtract(&w->pos0, &pEm->pos, &d);
            if (d.x * d.x + d.y * d.y + d.z * d.z <= 10000.0f) {
                pEm->pos = w->pos0;
                pEm->r_no_2++;
            } else {
#line 996 "D:/Bio4/Prog/emBarred.cpp"
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, 100.0f);
                PSVECAdd(&pEm->pos, &d, &pEm->pos);
            }
            break;
        }
        if (emBarredUnderCk(pEm)) {
            w->Status = 0;
            w->Open_flag = 1;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 1;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            if (w->pBarred) {
                w->pBarred->setOpen(1);
            }
        }
        break;
    case 2:
        switch (pEm->type) {
        case 5:
        case 6:
        case 8:
        case 9:
            break;
        default:
            if (pEm->r_no_3 == 0) {
                SndStop(w->Seid, 0);
                w->Seid = SndCall(6, 0x27, &pEm->pos, 0, 0, pEm);
            }
            break;
        }
        switch (pEm->type) {
        case 5:
        case 6:
        case 8:
        case 9:
            break;
        default:
            if (w->Eff_id != 0xFF) {
                EstSet(pEm, -1, 0, 0, w->Eff_id, 2, 0, ESP_CORE_KIND_NONE, pEm, 0);
            }
            break;
        }
        w->Timer = 5;
        pEm->r_no_2++;
    case 3:
        pEm->pos.x = fRand1_1() * 20.0f + w->pos0.x;
        pEm->pos.y = fRand0_1() * 20.0f + w->pos0.y;
        pEm->pos.z = fRand1_1() * 20.0f + w->pos0.z;
        if (w->Timer != 0) {
            w->Timer--;
        } else {
            pEm->pos = w->pos0;
            w->Status = 2;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
        }
        break;
    }
    pEm->matUpdate();
    if (w->Lock_mode == 0) {
        if (pEm->type == 5 || pEm->type == 6 || pEm->type == 8 || pEm->type == 9) {
            if (emBarredNearCk(pEm)) {
                if (w->Status != 1) {
                    w->Status = 0;
                    w->Open_flag = 1;
                    pEm->r_no_0 = 1;
                    pEm->r_no_1 = 1;
                    pEm->r_no_2 = 0;
                    pEm->r_no_3 = 0;
                    if (w->pBarred) {
                        w->pBarred->setOpen(1);
                    }
                }
            }
        }
    }
}

// Rno1 == 3: destroyed (setBreak): hides the gate, clears ACTIVE, lets everyone through and sets
// bit0 of the etc flag.
void emBarred_R1_Break(cEmBarred* pEm)
{
    EmBarredWork* w = EMBARRED_WK(pEm);
    u16* flg;

    if (pEm->r_no_2 == 0) {
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        pEm->clearStatus(EM_STATUS_ACTIVE);
        pEm->atari.throughOn();
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        pEm->r_no_2++;
    }
}

// Keeps the gate's effect collision in place: a quad for the bars (pEat, follows the lifting
// position) and four for the fixed frame posts / lintel (pEatFrame), sized by type; a broken
// gate only deactivates them and lets the player through.
void emBarredEatSet(cEmBarred* pEm)
{
    EmBarredWork* w = EMBARRED_WK(pEm);
    Vec poly[4];
    u16* flg;
    f32 hx;
    f32 hz;
    f32 hy;
    f32 h;
    f32 y;
    int attr;

    if (pEm->hp <= 0) {
        pEm->atari.throughOn();
        if (w->pEat) {
            w->pEat->setDisable();
        }
        if (w->pEatFrame[0]) {
            w->pEatFrame[0]->setDisable();
        }
        if (w->pEatFrame[1]) {
            w->pEatFrame[1]->setDisable();
        }
        if (w->pEatFrame[2]) {
            w->pEatFrame[2]->setDisable();
        }
        if (w->pEatFrame[3]) {
            w->pEatFrame[3]->setDisable();
        }
    }
    attr = 0;
    switch (pEm->type) {
    case 1:
    case 5:
    default:
        hx = 675.0f;
        hy = 60.0f;
        hz = 2350.0f;
        break;
    case 6:
        hx = 675.0f;
        hy = 60.0f;
        hz = 2350.0f;
        break;
    case 8:
        hx = 875.0f;
        hy = 60.0f;
        hz = 2650.0f;
        break;
    case 9:
        hx = 875.0f;
        hy = 60.0f;
        hz = 2650.0f;
        break;
    case 2:
        hx = 750.0f;
        hy = 60.0f;
        hz = 2600.0f;
        break;
    case 3:
        hx = 4150.0f;
        hy = 125.0f;
        hz = 4800.0f;
        break;
    case 4:
        hx = 1500.0f;
        hy = 125.0f;
        hz = 3000.0f;
        break;
    case 7:
        hx = 2000.0f;
        hy = 115.0f;
        hz = 3500.0f;
        attr = 0x404000;
        break;
    case 0:
        hx = 675.0f;
        hy = 60.0f;
        hz = 2350.0f;
        attr = 0x404000;
        break;
    }
    if (w->pEat == 0) {
        y = 0.0f;
        h = hz;
        if (pEm->type == 6) {
            y = 260.0f;
            h = hz - y;
        }
        poly[0].x = -hx;
        poly[0].y = y;
        poly[0].z = -hy;
        poly[1].x = hx;
        poly[1].y = y;
        poly[1].z = -hy;
        poly[2].x = hx;
        poly[2].y = y;
        poly[2].z = hy;
        poly[3].x = -hx;
        poly[3].y = y;
        poly[3].z = hy;
        w->pEat = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
    } else {
        w->pEat->setEnable();
        w->pEat->setCoord(&pEm->pos, &pEm->ang);
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg && (*flg & 2)) {
            w->pEat->setDisable();
        }
    }
    if (pEm->type == 6) {
        h = hz;
        if (w->pEatFrame[0] == 0) {
            poly[0].x = -hx;
            poly[0].y = 0.0f;
            poly[0].z = -hy;
            poly[1].x = -hx + 160.0f;
            poly[1].y = 0.0f;
            poly[1].z = -hy;
            poly[2].x = -hx + 160.0f;
            poly[2].y = 0.0f;
            poly[2].z = hy;
            poly[3].x = -hx;
            poly[3].y = 0.0f;
            poly[3].z = hy;
            w->pEatFrame[0] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
        } else {
            w->pEatFrame[0]->setEnable();
            w->pEatFrame[0]->setCoord(&pEm->pos, &pEm->ang);
        }
        if (w->pEatFrame[1] == 0) {
            poly[0].x = hx - 160.0f;
            poly[0].y = 0.0f;
            poly[0].z = -hy;
            poly[1].x = hx;
            poly[1].y = 0.0f;
            poly[1].z = -hy;
            poly[2].x = hx;
            poly[2].y = 0.0f;
            poly[2].z = hy;
            poly[3].x = hx - 160.0f;
            poly[3].y = 0.0f;
            poly[3].z = hy;
            w->pEatFrame[1] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
        } else {
            w->pEatFrame[1]->setEnable();
            w->pEatFrame[1]->setCoord(&pEm->pos, &pEm->ang);
        }
        if (w->pEatFrame[2] == 0) {
            poly[0].x = -hx;
            poly[0].y = 0.0f;
            poly[0].z = -hy;
            poly[1].x = hx;
            poly[1].y = 0.0f;
            poly[1].z = -hy;
            poly[2].x = hx;
            poly[2].y = 0.0f;
            poly[2].z = hy;
            poly[3].x = -hx;
            poly[3].y = 0.0f;
            poly[3].z = hy;
            w->pEatFrame[2] = EatMgr.create(&pEm->pos, &pEm->ang, poly, 260.0f, attr, 0);
        } else {
            w->pEatFrame[2]->setEnable();
            w->pEatFrame[2]->setCoord(&pEm->pos, &pEm->ang);
        }
        if (w->pEatFrame[3] == 0) {
            poly[0].x = -hx;
            poly[0].y = hz - 260.0f;
            poly[0].z = -hy;
            poly[1].x = hx;
            poly[1].y = hz - 260.0f;
            poly[1].z = -hy;
            poly[2].x = hx;
            poly[2].y = hz - 260.0f;
            poly[2].z = hy;
            poly[3].x = -hx;
            poly[3].y = hz - 260.0f;
            poly[3].z = hy;
            w->pEatFrame[3] = EatMgr.create(&pEm->pos, &pEm->ang, poly, 260.0f, attr, 0);
        } else {
            w->pEatFrame[3]->setEnable();
            w->pEatFrame[3]->setCoord(&pEm->pos, &pEm->ang);
        }
    }
}

// Proximity test of the automatic gates: 1 when the player or any live character (id <= 0x3F) is
// within 2500 units of pos0 (3500 while open, hysteresis); 0 for other types or when locked.
int emBarredNearCk(cEmBarred* pEm)
{
    EmBarredWork* w = EMBARRED_WK(pEm);
    f32 r2;
    f32 d;
    u32 i;

    switch (pEm->type) {
    case 5:
    case 6:
    case 8:
    case 9:
        break;
    default:
        return 0;
    }
    if (w->Lock_mode != 0) {
        return 0;
    }
    if (w->Status == 1) {
        r2 = 12250000.0f;
    } else {
        r2 = 6250000.0f;
    }
    d = (w->pos0.x - pPL->pos.x) * (w->pos0.x - pPL->pos.x) + (w->pos0.y - pPL->pos.y) * (w->pos0.y - pPL->pos.y) + (w->pos0.z - pPL->pos.z) * (w->pos0.z - pPL->pos.z);
    if (d < r2) {
        return 1;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id > 0x3F) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == pEm) {
            continue;
        }
        d = (w->pos0.x - e->pos.x) * (w->pos0.x - e->pos.x) + (w->pos0.y - e->pos.y) * (w->pos0.y - e->pos.y) + (w->pos0.z - e->pos.z) * (w->pos0.z - e->pos.z);
        if (d < r2) {
            return 1;
        }
    }
    return 0;
}

// Est id used for the spark / bar-break effects.
void cEmBarred::setEff(u8 eff_id)
{
    EMBARRED_WK(this)->Eff_id = eff_id;
}

// Script / explosion entry: blows the gate out toward `target` (est 3 of Eff_id) and goes to Break.
void cEmBarred::setBreak(Vec* pPos)
{
    EmBarredWork* w = EMBARRED_WK(this);
    Vec v;
    f32 ang;

    if (hp > 0) {
        ang = Muku(&pos, pPos, this->ang.y, PI);
        if (fabsf(ang) < PI / 2) {
            ang = this->ang.y;
        } else {
            ang = this->ang.y + PI;
        }
        v.y = LIMIT_ANGLE(ang);
        v.x = 0.0f;
        v.z = 0.0f;
        EstSet(0, -1, &pos, &v, w->Eff_id, 3, 0, ESP_CORE_KIND_NONE, 0, 0);
        atari.throughOn();
        hp = 0;
        r_no_0 = 1;
        r_no_1 = 3;
        r_no_2 = 0;
        r_no_3 = 0;
    }
}

// The proximity gate stays open once opened (be_flag bit0).
void cEmBarred::setNoClose()
{
    EMBARRED_WK(this)->be_flag |= 1;
}

// Pairs two gates so the proximity logic of one opens / closes the other.
void cEmBarred::setDouble(cEmBarred* other)
{
    EmBarredWork* w = EMBARRED_WK(this);

    if (other) {
        w->pBarred = other;
        EMBARRED_WK(other)->pBarred = this;
    }
}

// Enables the "someone is under the closing gate" test (be_flag bit1).
void cEmBarred::setUnderCk()
{
    EMBARRED_WK(this)->be_flag |= 2;
}

// 1 when the player, the partner or a live visible character stands in the gate's slot (within
// Width x 200 of pos0 at floor level) while the gate is less than 2200 up; only with setUnderCk
// and not for the proximity types.
int emBarredUnderCk(cEmBarred* pEm)
{
    EmBarredWork* w = EMBARRED_WK(pEm);
    Mtx m;
    Vec v;
    u32 i;

    if (!(w->be_flag & 2)) {
        return 0;
    }
    switch (pEm->type) {
    case 5:
    case 6:
    case 8:
    case 9:
        return 0;
    }
    if (pEm->pos.y - w->pos0.y > 2200.0f) {
        return 0;
    }
    PSMTXRotRad(m, 'y', pEm->ang.y);
    TransMatrix(m, &w->pos0);
    PSMTXInverse(m, m);
    PSMTXMultVec(m, &pPL->pos, &v);
    if (v.x > -w->Width && v.x < w->Width && v.y > -100.0f && v.y < 100.0f && v.z > -200.0f && v.z < 200.0f) {
        return 1;
    }
    if (pSUB) {
        PSMTXMultVec(m, &pSUB->pos, &v);
        if (v.x > -w->Width && v.x < w->Width && v.y > -100.0f && v.y < 100.0f && v.z > -200.0f && v.z < 2100.0f) {
            return 1;
        }
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id > 0x3F) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == pEm) {
            continue;
        }
        if (!(e->be_flag & 2)) {
            continue;
        }
        PSMTXMultVec(m, &e->pos, &v);
        if (v.x > -w->Width && v.x < w->Width && v.y > -100.0f && v.y < 100.0f && v.z > -200.0f && v.z < 200.0f) {
            return 1;
        }
    }
    return 0;
}
