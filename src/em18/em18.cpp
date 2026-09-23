// em18 module (D:/Bio4/Prog/em18.cpp): the merchant. Waits, offers the trade action button when
// the player stands in front of him (em18ActEvtSetTrade), plays the trade motion and opens the
// shop sub screen; dies to a damage volume of kind 1/7 or to any weapon but the special ones.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "em18.h"
#include "em10.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "foot_shadow.h"
#include "act_btn.h"
#include "sscrn.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include <dolphin/os.h>
#include "em_mod.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em18,52,4");


typedef void (*Em18Func)(cEm18*);

static void em18_R0_Init(cEm18* em);
static void em18_R0_Move(cEm18* em);
static void em18_R1_Wait(cEm18* em);
static void em18_R1_Trade(cEm18* em);
static void em18TradeAction(cEm18* em);
static void em18_R0_Damage(cEm18* em);
static void em18_R1_Dm_Normal(cEm18* em);
static void em18_R0_Die(cEm18* em);
static void em18_R1_Die_Normal(cEm18* em);


// Module entry (SN loader): registers Em18Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em18 prolog Ok\n");
    EmInitFunc = Em18Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm18 class in the manager's work.
void Em18Init(cEm* em)
{
    new (em) cEm18();
}

// Per-frame damage check (cEm18::move, r_no_0 != 0): an explosion / fire damage volume (kind 1 / 7) or
// any weapon hit except the mine (0xE), 0x14, 0x16 and the flash grenade (0x17) kills the merchant at
// once (hp 0, blood, death SE 4, EmSetDie, Die_Normal); the knife sets dmType 0x11.
void em18DmCk(cEm18* em)
{
    u8 wep;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_ENV_FIRE:
            em->hp = 0;
            EmSetDie(em);
            EmRoutineSet(em, 3, 0, 0, 0);
            return;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    em->dmg.m_Timer = 1;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    wep = em->dmg.m_Wep;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    em->hp = 0;
    em18BloodSet(em);
    SndCall(8, 4, &em->pos, em->id, 0, 0);
    EmSetDie(em);
    EmRoutineSet(em, 3, 0, 0, 0);
}

// Blood effect of the killing hit (EmDmBloodSet2 kind 0x15): the big splash for a near shotgun hit,
// explosives, magnum and rifles; none for the hand weapons.
void em18BloodSet(cEm18* em)
{
    YARARE_INFO* part = em->dmg.m_pDamageYarare;

    switch (em->dmg.m_Wep) {
    case 7:
    case 8:
        if (part->len < 36000000.0f) {
            EmDmBloodSet2(em, 0x15, 1, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x15, 0, 0, 0, 0);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        EmDmBloodSet2(em, 0x15, 0, 0, 0, 0);
        break;
    case 9:
    case 0xA:
        EmDmBloodSet2(em, 0x15, 0, 0, 0, 0);
        break;
    case 0xB:
    case 0xC:
    case 0x10:
        EmDmBloodSet2(em, 0x15, 0, 0, 0, 0);
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
        EmDmBloodSet2(em, 0x15, 1, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    default:
        break;
    }
}

Em18Func Em18_R0_move_tbl[4] = {
    em18_R0_Init,
    em18_R0_Move,
    em18_R0_Damage,
    em18_R0_Die,
};

static Em18Func Em18_R1_move_tbl[2] = {
    em18_R1_Wait,
    em18_R1_Trade,
};

static Em18Func Em18_R2_move_tbl[1] = {
    em18_R1_Dm_Normal,
};

static Em18Func Em18_R3_move_tbl[1] = {
    em18_R1_Die_Normal,
};

// Parts index remap of the mirrored motions (cModel::motFlip).
static u16 em18_flip_tbl[80] = {
    0, 1, 2, 3, 4, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 5, 6, 7, 8, 9,
    0xA, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
};

// Per-frame update: damage check, clears the per-frame Be_flg bits, the R0 routine table (Init / Move /
// Damage / Die), then the neck tracking, collision and scenario check and the robe cloth (Em18ClothMove).
void cEm18::move()
{
    Em18Work* w = EM18_WK(this);

    if (r_no_0) {
        em18DmCk(this);
    }
    w->Be_flg &= ~0x1F;
    Em18_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em18NeckMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    Em18ClothMove(this, &w->Cloth);
}

// R0 == 0: creation. Builds the model (body ARC 5/6, the robe ARC 0xE and the extra part ARC 7, hands,
// hidden coat / goods parts), light, collision, hit boxes (head 5 + hit[0..8]), loads the effect data,
// sets up the cloth and starts Wait; the merchant is not "active" (Ashley never runs from him).
static void em18_R0_Init(cEm18* em)
{
    Em18Work* w = EM18_WK(em);
    cModelInfo* info;
    void* tpl;
    void* tplE;
    int one;

    if (em->modelInit(ARC(5), ARC(6)) == 0) {
        pLog->err(0, 0, "em18() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    tpl = ARC(8);
    tplE = ARC(0xF);
    w->pRobe = ModInfoMgr.create(ARC(0xE), tplE);
    if (w->pRobe) {
        em->addModel(w->pRobe);
        w->pRobe->be_flag |= 0x20;
    }
    info = ModInfoMgr.create(ARC(7), tpl);
    if (info) {
        em->addModel(info);
    }
    w->pRHand = 0;
    w->pLHand = 0;
    em18HandSet(em);
    w->pCloth = 0;
    w->pGoods = 0;
    em18ClothPartsSet(em, 0);
    em18GoodsPartsSet(em, 0);
    em->pFsdTbl = &Em10_fs_tbl;
    em->Motion.flip = em18_flip_tbl;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->atari.init(0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 1800.0f, 1, 0x2000, 10);
    em->atari.m_flag |= 8;
    em->litArea.on(1);
    one = 1;
    em->setStatus(one);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 150.0f, 120.0f, 5, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[0], 0.0f, -30.0f, 0.0f, 200.0f, 300.0f, 2, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], -20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x14, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[2], 20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x18, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[3], -300.0f, 0.0f, 0.0f, 100.0f, 400.0f, 9, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 100.0f, 400.0f, 0xF, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[5], -20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x13, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[6], 20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x17, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 120.0f, 300.0f, 8, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 120.0f, 300.0f, 0xE, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), EFF_EM18, 0);
    Em18ClothSet(em, &w->Cloth, 0);
    w->Be_flg = 0;
    w->Neck_dir_y = 0.0f;
    EmRoutineSet(em, one, 0, 0, 0);
    MotionSetCore(em, MOTION(em), ARC(0x14), 0, 0, 1, 0);
    MotionMove(em, 0);
    em->clearStatus(EM_STATUS_ACTIVE);
    em18_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em18_R1_move_tbl: Wait, Trade).
static void em18_R0_Move(cEm18* em)
{
    Em18_R1_move_tbl[em->r_no_1](em);
}

// R1 == 0 Wait: the idle motion ARC 0x14 with the neck following the player (Be_flg bit4) and the
// trade action button offered (em18ActEvtSetTrade).
static void em18_R1_Wait(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
    em18ActEvtSetTrade(em);
}

// R1 == 1 Trade: opens the coat (motion 0x15, greeting voice 9, cloth shown at frame 36, goods at 41)
// turning to the player, waits for the shop sub screen (SubScreenOpen SS_OPEN_SHOP) to close, closes
// the coat again (0x17, farewell voice 7) and returns to Wait; damage is held meanwhile.
static void em18_R1_Trade(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 10, 1, 0);
        w->Seid_voice = SndCall(8, 9, &em->pos, em->id, 0, 0);
        KeyStop(0xEFCF0000);
        em->r_no_2++;
    case 1:
        em->dmg.m_Timer = 2;
        if (em->Motion.Seq_frame > 35.7f && em->Motion.Seq_frame < 36.3f) {
            em18ClothPartsSet(em, 1);
            SndCall(8, 0xA, &em->pos, em->id, 0, 0);
        }
        if (em->Motion.Seq_frame > 40.7f && em->Motion.Seq_frame < 41.3f) {
            em18GoodsPartsSet(em, 1);
        }
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        if (SubScreenOpen(SS_OPEN_SHOP, 0)) {
            em->r_no_2++;
        }
        break;
    case 3:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 10, 1, 0);
        SndCall(8, 0xA, &em->pos, em->id, 0, 0);
        w->Seid_voice = SndCall(8, 7, &em->pos, em->id, 0, 0);
        pG->Stop_flg &= 0x7FFFFFFF;
        em->r_no_2++;
    case 4:
        if (em->Motion.Seq_frame > 33.7f && em->Motion.Seq_frame < 34.3f) {
            em18ClothPartsSet(em, 0);
        }
        if (em->Motion.Seq_frame > 26.7f && em->Motion.Seq_frame < 27.3f) {
            em18GoodsPartsSet(em, 0);
        }
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

// Offers the "trade" action button when the alive merchant is in front of the player inside the talk
// box (type 1 = the far-standing variant uses a 1000..2500 box beyond 1500 units).
void em18ActEvtSetTrade(cEm18* em)
{
    Mtx inv;
    Vec lp;
    // COMPILER-DIFF: #5. The original has one `fabsf(lp.y) > 700` check after the type if/else and its
    // interblock scheduler copied it into the first arm (`fabs f13, f0` with the 700 still in f12); ours
    // forms no region there, so the check is written in both arms through one shared `ay`: a pseudo set
    // in two blocks is global-allocated, so local-alloc cannot tie the fabs result to its dying input.
    f32 ay;

    if (em->hp <= 0) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI)) > 0.7853982f) {
        return;
    }
    PSMTXInverse(pPL->mat, inv);
    PSMTXMultVec(inv, &em->pos, &lp);
    if (em->type != 1) {
        if (lp.z > 1200.0f || lp.z < 0.0f) {
            return;
        }
        if (lp.x > 700.0f || lp.x < -700.0f) {
            return;
        }
        ay = fabsf(lp.y);
        if (ay > 700.0f) {
            return;
        }
    } else {
        if (em->l_pl > 2250000.0f) {
            if (lp.z > 2500.0f || lp.z < 1000.0f) {
                return;
            }
            if (lp.x > 700.0f || lp.x < -700.0f) {
                return;
            }
        }
        ay = fabsf(lp.y);
        if (ay > 700.0f) {
            return;
        }
    }
    ActBtn.set(ACT_TALK, 2, (void*) em18TradeAction, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
}

// Action button callback: the first time starts the Trade routine (Be_flg bit5) with the player's
// damage held 30 frames; afterwards (and always for type 1) opens the shop sub screen directly.
static void em18TradeAction(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    if (em->type != 1) {
        if (w->Be_flg & 0x20) {
            SubScreenOpen(SS_OPEN_SHOP, 0);
        } else {
            w->Be_flg |= 0x20;
            EmRoutineSet(em, 1, 1, 0, 0);
            pPL->dmg.set(0, 30);
        }
    } else {
        SubScreenOpen(SS_OPEN_SHOP, 0);
    }
}

// R0 == 2: damage (Be_flg bit3), runs Em18_R2_move_tbl (Dm_Normal).
static void em18_R0_Damage(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    w->Be_flg |= 8;
    Em18_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Normal: plays the idle motion once with the neck tracking on, back to Wait; the
// merchant has no real flinch (every hit kills him).
static void em18_R1_Dm_Normal(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 10);
        }
        break;
    }
}

// R0 == 3: death (Be_flg bit3), runs Em18_R3_move_tbl (Die_Normal).
static void em18_R0_Die(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    w->Be_flg |= 8;
    Em18_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Normal: the death motion ARC 0x18 with the death voice 8 and the body-fall SEs at
// frames 35 / 66, then inactive with the collision off; the corpse stays.
static void em18_R1_Die_Normal(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    switch (em->r_no_2) {
    case 2:
    default:
        break;
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 3, 1, 0);
        SndStop(w->Seid_voice, 0);
        SndCall(8, 8, &em->pos, em->id, 0, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->clearStatus(EM_STATUS_ACTIVE);
            em->atari.m_flag &= ~0x300;
            em->r_no_2++;
        } else {
            if (em->Motion.Seq_frame > 34.7f && em->Motion.Seq_frame < 35.3f) {
                SndCall(8, 5, &em->pos, em->id, 0, 0);
            }
            if (em->Motion.Seq_frame > 65.7f && em->Motion.Seq_frame < 66.3f) {
                SndCall(8, 6, &em->pos, em->id, 0, 0);
            }
        }
        break;
    }
}

// Neck tracking: while Be_flg bit4 is set turns the head part 3 towards the player within 60 deg
// (Neck_dir_y smoothed 10% per frame), else eases back to the motion.
void em18NeckMove(cEm18* em)
{
    Em18Work* w = EM18_WK(em);
    cParts* p;
    Vec v;

    p = em->getPartsPtr(4);
    {
        cParts* h = pPL->getPartsPtr(4);

        v.x = 0.0f;
        v.y = 250.0f;
        v.z = 0.0f;
        PSMTXMultVec(h->mat, &v, &v);
    }
    if (w->Be_flg & 0x10) {
        w->Neck_dir_y = w->Neck_dir_y * 0.9f + Muku(&em->pos, &pPL->pos, em->ang.y, 1.0471976f) * 0.1f;
    } else {
        w->Neck_dir_y = w->Neck_dir_y * 0.9f;
    }
    p = em->getPartsPtr(3);
    ((cParts*) p)->motParts.flags |= 0x40000000;
    ((cParts*) p)->inv_offset.x = 0.0f;
    ((cParts*) p)->inv_offset.y = w->Neck_dir_y;
    ((cParts*) p)->inv_offset.z = 0.0f;
}

// Swaps the coat model (pCloth): `on` 1 the open coat ARC 0xB, 0 the closed coat ARC 0xC.
void em18ClothPartsSet(cEm18* em, int on)
{
    Em18Work* w = EM18_WK(em);
    void* tpl = ARC(0xF);
    void* bin;
    cModelInfo* info;

    switch (on) {
    case 0:
    default:
        bin = ARC(0xC);
        break;
    case 1:
        bin = ARC(0xB);
        break;
    }
    info = ModInfoMgr.create(bin, tpl);
    if (info) {
        if (w->pCloth) {
            em->swapModelInfo(w->pCloth->model_addr, info);
        } else {
            em->addModel(info);
        }
        info->be_flag |= 0x20;
        w->pCloth = info;
    }
}

// Shows (1) / hides (0) the goods model ARC 0xD hanging inside the coat (pGoods; created on first use).
void em18GoodsPartsSet(cEm18* em, int on)
{
    Em18Work* w = EM18_WK(em);

    if (w->pGoods == 0) {
        cModelInfo* info = ModInfoMgr.create(ARC(0xD), ARC(6));

        if (info) {
            em->addModel(info);
        }
        w->pGoods = info;
    }
    if (on == 0) {
        w->pGoods->be_flag &= ~8;
    } else {
        w->pGoods->be_flag |= 8;
    }
}

// Creates / swaps the two hand models (ARC 0x12 / 0x13) into pRHand / pLHand.
void em18HandSet(cEm18* em)
{
    Em18Work* w = EM18_WK(em);
    void* binL = ARC(0x12);
    void* binR = ARC(0x13);
    cModelInfo* info;

    info = ModInfoMgr.create(binL, ARC(6));
    if (info) {
        if (w->pRHand) {
            em->swapModelInfo(w->pRHand->model_addr, info);
        } else {
            em->addModel(info);
        }
        w->pRHand = info;
    }
    info = ModInfoMgr.create(binR, ARC(6));
    if (info) {
        if (w->pLHand) {
            em->swapModelInfo(w->pLHand->model_addr, info);
        } else {
            em->addModel(info);
        }
        w->pLHand = info;
    }
}
