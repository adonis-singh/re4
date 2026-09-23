// em28 module (D:/Bio4/Prog/em28.cpp): the chicken (cEmMgr::idName 0x28 "CHICKEN"; the crow is em23).
// Waits, pecks and lays a random egg item (ids 8 / 9 / 0xA) from its wait routine, walks, and flutters
// off (dash / jump routines) when the player comes near, shoots, or the bell rings (em28EscapeCk); dies
// on the ground (R1_Die_Normal) or falling out of the air (R1_Die_Air). Routines: R0 Init / Move /
// Damage / Die, R1 Wait / Walk / Dash / Jump, R2 Dm_Small, R3 Die_Normal / Die_Air.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em28.h"
#include "emhit.h"
#include "emwep.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "sce_at.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include <dolphin/os.h>
#include "em_mod.h"



typedef void (*Em28Func)(cEm28*);

static void em28_R0_Init(cEm28* em);
static void em28_R0_Move(cEm28* em);
static void em28_R1_Wait(cEm28* em);
static void em28_R1_Walk(cEm28* em);
static void em28_R1_Dash(cEm28* em);
static void em28_R1_Jump(cEm28* em);
static void em28_R0_Damage(cEm28* em);
static void em28_R1_Dm_Small(cEm28* em);
static void em28_R0_Die(cEm28* em);
static void em28_R1_Die_Normal(cEm28* em);
static void em28_R1_Die_Air(cEm28* em);


// Module entry (SN loader): registers Em28Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em28 prolog Ok\n");
    EmInitFunc = Em28Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm28 class in the manager's work.
void Em28Init(cEm* em)
{
    new (em) cEm28();
}

// Per-frame damage check (cEm28::move): an explosion / fire volume kills the chicken (flag bit6,
// Die_Air when airborne else Die_Normal). A weapon hit: 0x16 / flash 0x17 / 0x2A only startle it
// (Dash / Jump), the mine (0xE) leaves it at 1 hp, anything else kills it with the blood effect
// by weapon kind (big for explosives / magnum / rifles / near shotgun).
void em28DmCk(cEm28* em)
{
    Em28Work* w = EM28_WK(em);
    u8 wep;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            em->hp = 0;
            EmSetDie(em);
            w->flags |= 0x40;
            if (w->flags & 0x10) {
                em->setRno(3, 1, 0, 0);
            } else {
                em->setRno(3, 0, 0, 0);
            }
            return;
        }
    }
    if (em->dmg.m_Flag) {
        wep = em->dmg.m_Wep;
        em->dmg.m_Flag = 0;
        switch (wep) {
        case 0x16:
        case 0x17:
        case 0x2A:
            if (!(w->flags & 0x10)) {
                if (Rnd() & 1) {
                    em->setRno(1, 3, 0, 0);
                } else {
                    em->setRno(1, 2, 0, 0);
                }
            }
            return;
        }
        em->hp = 0;
        SndCall(8, 0, &em->pos, em->id, 0, em);
        switch (em->dmg.m_Wep) {
        case 0:
        case 0xb:
        case 0xc:
        case 0x10:
        case 0x11:
        case 0x14:
        case 0x15:
        case 0x1b:
        case 0x1d:
        case 0x26:
        case 0x27:
        case 0x2b:
            EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
            EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            break;
        case 5:
        case 6:
        case 9:
        case 0xa:
        case 0xd:
        case 0x12:
        case 0x13:
        case 0x28:
        case 0x29:
            EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
            EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            break;
        case 0x2c:
        default:
            EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
            EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (em->l_pl < 16000000.0f) {
                EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
                EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
                EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            }
            break;
        case 0xE:
            EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
            EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            em->hp = 1;
            if (!(w->flags & 0x10)) {
                // Allocation lever (loop notes, no code): the doubled routine refs rank the HI `1`
                // (hp = 1 / xFC) above the HI zero in global-alloc (one r30, zero r29).
                do { em->setRno(1, (Rnd() & 1) ? 3 : 2, 0, 0); } while (0);
            }
            return;
        }
        if (em->hp <= 0) {
            EmSetDie(em);
            if (w->flags & 0x10) {
                em->setRno(3, 1, 0, 0);
            } else {
                em->setRno(3, 0, 0, 0);
            }
        } else if (!(w->flags & 0x10)) {
            if (Rnd() & 1) {
                em->setRno(1, 3, 0, 0);
            } else {
                em->setRno(1, 2, 0, 0);
            }
        }
    }
}

Em28Func Em28_R0_move_tbl[4] = {
    em28_R0_Init,
    em28_R0_Move,
    em28_R0_Damage,
    em28_R0_Die,
};

static Em28Func Em28_R1_move_tbl[4] = {
    em28_R1_Wait,
    em28_R1_Walk,
    em28_R1_Dash,
    em28_R1_Jump,
};

static Em28Func Em28_R2_move_tbl[1] = {
    em28_R1_Dm_Small,
};

static Em28Func Em28_R3_move_tbl[2] = {
    em28_R1_Die_Normal,
    em28_R1_Die_Air,
};

// Parts remap of the mirrored motions (cModel::motFlip).
static u16 em28_flip_tbl[36] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x09, 0x0A, 0x05, 0x06, 0x07, 0x0B, 0x0F, 0x10, 0x11, 0x0C,
    0x0D, 0x0E, 0x12, 0x13, 0x14, 0x15, 0x16, 0x0F, 0x10, 0x11, 0x12, 0x17, 0x18, 0x19, 0x1A, 0x1B,
    0x1C, 0x1D, 0x1E, 0x00,
};
asm(".section .data\n\t.balign 8\n\t.text");

// Per-frame update: damage check, clears the per-frame flags, the R0 table (Init / Move / Damage /
// Die), then collision and the scenario check (checkAir while airborne, flag bit4).
void cEm28::move()
{
    Em28Work* w = EM28_WK(this);
    f32 spd;

    if (r_no_0 != 0) {
        em28DmCk(this);
    }
    w->flags &= ~0x1F;
    if (w->escapeWait) {
        w->escapeWait--;
    }
    Em28_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    spd = VEC_DISTXZ(&pos_old, &pos);
    EmAtCheck(this);
    atari.move();
    if (w->flags & 0x10) {
        SatMgr.checkAir(this, 0);
    } else {
        SatMgr.check(this, 0);
    }
    if (VEC_DISTXZ(&pos, &pos_old) < spd * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
}

// R0 == 0: creation. Builds the model of type 0 / 1, IK / lock-on off, no Ashley help, collision and
// hit box, the room's ctrl11 / ctrl12, and starts Wait.
static void em28_R0_Init(cEm28* em)
{
    Em28Work* w = EM28_WK(em);
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em28() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(4), ARC(6)) == 0) {
            pLog->err(0, 0, "em28() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    }
    em->be_flag &= ~0x10;
    em->setStatus(EM_STATUS_IK_OFF);
    zero = 0;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->Motion.flip = em28_flip_tbl;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 500.0f, 500.0f, 0.0f };

        em->LightInfo.init2(0, 3, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->atari.init(0.0f, 0.0f, 0.0f, 300.0f, 200.0f, 200.0f, 500.0f, 3, 0x2000, 10);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    YarareInit(em, 0.0f, 0.0f, -130.0f, 200.0f, 100.0f, 3, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    EspDataLoad((u32) ARC(0xB), EFF_EM28, 0);
    w->flags = zero;
    w->escapeWait = zero;
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    w->x17C = zero;
    em->setStatus(EM_STATUS_ACTIVE);
    em->setRno(1, zero, zero, zero);
    MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 5, 0);
    MotionMove(em, 0);
    em28_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em28_R1_move_tbl: Wait, Walk, Dash, Jump).
static void em28_R0_Move(cEm28* em)
{
    Em28_R1_move_tbl[em->r_no_1](em);
}

// Take off when the floor under the chicken drops away (its perch broke).
static inline void em28FloorCk(cEm28* em)
{
    if ((pG->Frame_cnt & 3) == (em->emset_no & 3)) {
        if (SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0) < em->pos.y - 250.0f) {
            em->setRno(1, 3, 0, 0);
        }
    }
}

// Remember where the bell (this chicken) was disturbed.
static inline void em28BellSet(cEm28* em)
{
    if (!StaFlagChk(pG, STA_SE_BURST)) {
        StaFlagOn(pG, STA_SE_BURST);
        pG->SeInfo.pos = em->pos;
        pG->SeInfo.type = 0;
    }
}

// R1 == 0 Wait: idles and pecks (random idle variants); once (flag bit5) it may lay an egg: a
// cEmWep egg model registered as a pickup item (SceAtCreateItemAt id 8 white / 9 brown / 0xA gold,
// 1 in 8 attempts); then Walk (1); flees (em28EscapeCk) or takes off when its perch breaks.
static void em28_R1_Wait(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0x1E, 5, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (!(em->flag & 0x80000000)) {
                if (Rnd() & 1) {
                    if (Rnd() & 3) {
                        em->r_no_2++;
                    } else if ((Rnd() & 3) == 0 && !(w->flags & 0x20)) {
                        em->r_no_2 = 6;
                    } else {
                        em->r_no_2 = 4;
                    }
                }
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 0xA, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (Rnd() & 1) {
                em->r_no_2 = 0;
            } else {
                em->setRno(1, 1, 0, 0);
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 0xA, 1, 0);
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            if (Rnd() & 1) {
                em->r_no_2 = 0;
            } else {
                em->setRno(1, 1, 0, 0);
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x19), 0, 0xA, 1, 0);
        w->timer = 15;
        em->r_no_2++;
    case 7:
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                Vec pos;
                cEmWep* wep;
                // One `no` for the three arms: a 3-set pseudo with 9 refs over ~15 insns outranks
                // em in global-alloc and takes r31 (a block-local `no` is local-alloc'd and can
                // never get r31, the frame-pointer register), which pushes em to r30 and lets
                // &pos reuse r31.
                int no;

                pos = em->pos;
                if (Rnd() & 7) {
                    if ((u8) (Rnd() % 3)) {
                        wep = SetWeapon(ARC(7), ARC(8), &pos, &em->ang, 1);
                        if (wep) {
                            no = SceAtCreateItemAt(&pos, 8, 0, -1, -1, 0, -1);
                            SceAtSetItemModel(no, wep);
                            wep->setAtNo(no);
                        }
                    } else {
                        wep = SetWeapon(ARC(7), ARC(9), &pos, &em->ang, 1);
                        if (wep) {
                            no = SceAtCreateItemAt(&pos, 9, 0, -1, -1, 0, -1);
                            SceAtSetItemModel(no, wep);
                            wep->setAtNo(no);
                        }
                    }
                } else {
                    wep = SetWeapon(ARC(7), ARC(0xA), &pos, &em->ang, 1);
                    if (wep) {
                        no = SceAtCreateItemAt(&pos, 0xA, 0, -1, -1, 0, -1);
                        SceAtSetItemModel(no, wep);
                        wep->setAtNo(no);
                    }
                }
                w->flags |= 0x20;
                if (wep) {
                    wep->setYarare(100.0f, 10.0f, 0);
                    wep->setEffDamage(0x20, 1);
                    wep->setSeDamage(1, 6, em->id);
                }
            }
        }
        if (MotionMove(em, 0)) {
            em->setRno(1, 1, 0, 0);
        }
        break;
    }
    em28FloorCk(em);
    em28EscapeCk(em);
}

// R1 == 1 Walk: struts (two walk variants) 3..7 steps turning slowly to targetAng, then back to Wait;
// escape checks meanwhile.
static void em28_R1_Walk(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0xE), 0, 0xA, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0xA, 5, 0);
        }
        w->targetAng = fRand1_1() * PI;
        w->timer = (u8) (Rnd() % 5) + 3;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->targetAng, PI / 128.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (w->stuckCnt > 30) {
            w->timer = 0;
        }
        if (MotionMove(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                em->setRno(1, 0, 0, 0);
            }
        }
        break;
    }
    em28FloorCk(em);
    em28EscapeCk(em);
}

// R1 == 2 Dash: runs away flapping (mirrored by r_no_3) for 3..5 loops with random turns every
// turnTimer frames, then Wait (0).
static void em28_R1_Dash(cEm28* em)
{
    Em28Work* w = EM28_WK(em);
    int st = em->r_no_2;

    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x10), ARC(0x1B), 3, 5, 0);
        w->targetAng = GetXZAngle(&pPL->pos, &em->pos);
        w->targetAng += fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = (u8) (Rnd() % 3) + 3;
        w->turnTimer = (u8) (Rnd() % 30) + 30;
        EstSet(0, -1, &em->getPartsPtr(0)->world, 0, EFF_EM28, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(8, 2, &em->pos, em->id, 0, em);
        em28BellSet(em);
        em->r_no_3 = Rnd() & 1;
        em->r_no_2++;
    case 1:
        if (w->turnTimer) {
            w->turnTimer--;
        } else {
            w->turnTimer = (u8) (Rnd() % 15) + 15;
            w->targetAng += fRand1_1() * (PI / 4.0f);
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        em->ang.y += Muku2(em->ang.y, w->targetAng, PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (w->stuckCnt > 3) {
            if (em->r_no_3) {
                w->targetAng += PI / 5.0f;
            } else {
                w->targetAng -= PI / 5.0f;
            }
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        if (MotionMove(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                em->setRno(1, 0, 0, 0);
            }
        }
        break;
    }
    em28FloorCk(em);
    em28EscapeCk(em);
}

// R1 == 3 Jump: flutters up into the air (spd, flag bit4 airborne) heading targetAng, glides 4..7
// loops, lands on the floor found below and goes to Dash (2).
static void em28_R1_Jump(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x12), ARC(0x1C), 3, 1, 0);
        w->targetAng = GetXZAngle(&pPL->pos, &em->pos);
        w->targetAng += fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = (Rnd() & 3) + 4;
        em->r_no_3 = Rnd() & 1;
        SndCall(8, 2, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->targetAng, PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (w->stuckCnt > 3) {
            if (em->r_no_3) {
                w->targetAng += PI / 16.0f;
            } else {
                w->targetAng -= PI / 16.0f;
            }
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x13), ARC(0x1D), 3, 4, 0);
        w->spd.x = 0.0f;
        w->spd.y = fRand0_1() * 100.0f + 150.0f;
        w->spd.z = fRand0_1() * 50.0f + 150.0f;
        EstSet(0, -1, &em->getPartsPtr(0)->world, 0, EFF_EM28, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
        em28BellSet(em);
        em->r_no_2++;
    case 3: {
        Vec v;

        w->flags |= 0x10;
        em->ang.y += Muku2(em->ang.y, w->targetAng, PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (w->stuckCnt > 3) {
            w->targetAng += PI / 32.0f;
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        w->spd.y -= 15.0f;
        PSMTXMultVecSR(em->mat, &w->spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        if (w->spd.y < 0.0f) {
            f32 fl;

            v = em->pos;
            v.y = em->pos_old.y;
            fl = SatMgr.getFloor(&v, 0, 600.0f, 100000.0f, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                w->spd.y = 0.0f;
                em->r_no_2++;
                MotionMove(em, 0);
                break;
            }
        }
        MotionMove(em, 0);
        break;
    }
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x14), ARC(0x1E), 3, 1, 0);
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
    em28EscapeCk(em);
}

// R0 == 2: damage (flag bit3), runs Em28_R2_move_tbl (Dm_Small).
static void em28_R0_Damage(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    w->flags |= 8;
    Em28_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Small: the startled flap, then Wait (0).
static void em28_R1_Dm_Small(cEm28* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x11), 0, 0, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->setRno(1, 0, 0, 0);
        }
        break;
    }
}

// R0 == 3: death (flag bit3), runs Em28_R3_move_tbl (Die_Normal, Die_Air).
static void em28_R0_Die(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    w->flags |= 8;
    Em28_R3_move_tbl[em->r_no_1](em);
}

// Fade every part while dying (work flag bit6).
static inline void em28DieFade(cEm28* em, Em28Work* w)
{
    if (w->flags & 0x40) {
        cModelInfo* info;

        for (info = em->pModelInfo; info; info = info->pList) {
            if (info->color[0] > 0x20) {
                info->color[0] -= 0x20;
            }
            info->color[2] = info->color[1] = info->color[0];
        }
    }
}

// R0 3 / R1 == 0 Die_Normal: dies on the ground (one of three death motions, one turned to the hit
// direction), drops the item (ITEMSET) and fades the parts out (em28DieFade).
static void em28_R1_Die_Normal(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->clearStatus(EM_STATUS_ACTIVE);
        switch ((u8) (Rnd() % 3)) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
            break;
        case 1:
            em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 3, 1, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0x1A), 0, 3, 1, 0);
            break;
        }
        w->timer = 14;
        em->atari.m_flag &= ~0x200;
        em->r_no_2++;
    case 1:
        em28DieFade(em, w);
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                SndCall(8, 1, &em->getPartsPtr(0)->world, em->id, 0, em);
            }
        }
        if (MotionMove(em, 0)) {
            em->setStatus(EM_STATUS_ITEMSET);
            EmSetDropItem(em);
            em->r_no_2++;
        }
        break;
    }
}

// R0 3 / R1 == 1 Die_Air: killed in the air: tumbles down (spd, flag bit4) to the floor, then the
// item drop and the fade.
static void em28_R1_Die_Air(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 3, 1, 0);
        em->r_no_2++;
    case 1: {
        Vec v;

        em28DieFade(em, w);
        w->flags |= 0x10;
        w->spd.y -= 15.0f;
        PSMTXMultVecSR(em->mat, &w->spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        if (w->spd.y < 0.0f) {
            f32 fl;

            v = em->pos;
            v.y = em->pos_old.y;
            fl = SatMgr.getFloor(&v, 0, 600.0f, 100000.0f, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                w->spd.y = 0.0f;
                em->r_no_2++;
                MotionMove(em, 0);
                break;
            }
        }
        MotionMove(em, 0);
        break;
    }
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 3, 1, 0);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->atari.m_flag &= ~0x200;
        SndCall(8, 1, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 3:
        em28DieFade(em, w);
        if (MotionMove(em, 0)) {
            em->setStatus(EM_STATUS_ITEMSET);
            EmSetDropItem(em);
            em->r_no_2++;
        }
        break;
    }
}

// Escape when the chicken stands too close to the player, the bell rings or the player shoots.
int em28EscapeCk(cEm28* em)
{
    Em28Work* w = EM28_WK(em);
    f32 d;

    if (w->flags & 0x10) {
        return 0;
    }
    if (w->escapeWait != 0) {
        return 0;
    }
    int esc = 0;
    if (StaFlagChk(pG, STA_SE_BURST)) {
        f32 r;

        // three identical arms + the override after the switch: the arm sets are dead (the
        // compare skeleton stays) and the block-local `r` is loaded at the use (em3cFindCk idiom)
        switch (pG->SeInfo.type) {
        case 0:
            r = 15000.0f;
            break;
        case 1:
            r = 15000.0f;
            break;
        default:
            r = 15000.0f;
            break;
        }
        r = 15000.0f;
        if ((em->pos.x - pG->SeInfo.pos.x) * (em->pos.x - pG->SeInfo.pos.x) + (em->pos.y - pG->SeInfo.pos.y) * (em->pos.y - pG->SeInfo.pos.y)
            + (em->pos.z - pG->SeInfo.pos.z) * (em->pos.z - pG->SeInfo.pos.z) < r * r) {
            esc = 1;
        }
    }
    d = em->l_pl;
    if (StaFlagChk(pG, STA_PL_FIRE)) {
        if (d < 100000000.0f) {
            esc = 1;
        }
    }
    if (d < 4000000.0f) {
        esc = 1;
    }
    if (!esc) {
        return 0;
    }
    if (Rnd() & 3) {
        em->setRno(1, 2, 0, 0);
    } else {
        em->setRno(1, 3, 0, 0);
    }
    w->escapeWait = (u8) (Rnd() % 15) + 15;
    return 1;
}
