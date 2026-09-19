// em26 module (D:/Bio4/Prog/em26.cpp): a large stationary enemy that waits, roars and bites
// (em26AtkCk against parts 4) when hurt enough; damage volumes of kind 1/4/5/7 kill it at once.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em26.h"
#include "em10.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "snd.h"
#include "quake.h"
#include "pad.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

typedef void (*Em26Func)(cEm26*);

static void em26_R0_Init(cEm26* em);
static void em26_R0_Move(cEm26* em);
static void em26_R1_Wait(cEm26* em);
static void em26_R1_Atk(cEm26* em);
static void em26_R0_Damage(cEm26* em);
static void em26_R1_Dm_Small(cEm26* em);
static void em26_R0_Die(cEm26* em);
static void em26_R1_Die_Normal(cEm26* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->r_no_0 = r0;
    em->r_no_1 = r1;
    em->r_no_2 = r2;
    em->r_no_3 = r3;
}

// Module entry (SN loader): registers Em26Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em26 prolog Ok\n");
    EmInitFunc = Em26Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm26 class in the manager's work.
void Em26Init(cEm* em)
{
    new (em) cEm26();
}

// Per-frame damage check (cEm26::move): an explosion / fire volume kills the cow at once (flag bit5,
// Die_Normal). A weapon hit (all but 0x14 / 0x16 / flash 0x17 / 0x2A / mine 0xE) takes GetWepDmVal
// off hp (dmgTotal accumulates it for the bite), blood by weapon kind (big for explosives and a near
// shotgun hit); a kill goes to Die_Normal, a hit to the head (5) or tail (0x18), or one in five
// elsewhere, to Dm_Small.
void em26DmCk(cEm26* em)
{
    Em26Work* w = EM26_WK(em);
    YARARE_INFO* part;
    int near;
    int dmg;
    u8 wep;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            em->hp = 0;
            EmSetDie(em);
            w->flags |= 0x20;
            EmRoutineSet(em, 3, 0, 0, 0);
            return;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    part = em->dmg.m_pDamageYarare;
    near = 0;
    if (part->rad < 36000000.0f) {
        near = 1;
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
    if (wep == 0x2A) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    em->dmg.m_Timer = 1;
    if (wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    {
        u32 no = em->dmg.m_Wep;

        dmg = 100;
        if (no <= 0x2D) {
            dmg = GetWepDmVal(em, no, near);
        }
    }
    LifeDownSet(em, dmg, 0);
    w->dmgTotal += dmg;
    switch (em->dmg.m_Wep) {
    case 0x2B:
        EmDmBloodSet2(em, 0x1E, 0, 0, 0, 0);
        break;
    case 0x10:
        EmDmBloodSet2(em, 0x1E, 6, 0, 0, 0);
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        EmDmBloodSet2(em, 0x1E, 5, 0, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            Camera* cam = &pG->Cam;
            cModel* p = em->getPartsPtr(0);

            if ((cam->param.pos.x - p->world.x) * (cam->param.pos.x - p->world.x)
                    + (cam->param.pos.y - p->world.y) * (cam->param.pos.y - p->world.y)
                    + (cam->param.pos.z - p->world.z) * (cam->param.pos.z - p->world.z) < 4000000.0f) {
                EmDmBloodSet2(em, 0x1E, 7, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x1E, 1, 0, 0, 0);
            }
        } else {
            EmDmBloodSet2(em, 0x1E, 0, 0, 0, 0);
        }
        break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x26:
    case 0x28:
        EmDmBloodSet2(em, 0x1E, 0, 0, 0, 0);
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        EmDmBloodSet2(em, 0x1E, 1, 0, 0, 0);
        break;
    }
    SndCall(8, 0xA, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        EmRoutineSet(em, 3, 0, 0, 0);
    } else if (part->partsNo == 5 || part->partsNo == 0x18 || Rnd() % 5 == 0) {
        EmRoutineSet(em, 2, 0, 0, 0);
    }
}

Em26Func Em26_R0_move_tbl[4] = {
    em26_R0_Init,
    em26_R0_Move,
    em26_R0_Damage,
    em26_R0_Die,
};

static Em26Func Em26_R1_move_tbl[2] = {
    em26_R1_Wait,
    em26_R1_Atk,
};

static Em26Func Em26_R2_move_tbl[1] = {
    em26_R1_Dm_Small,
};

static Em26Func Em26_R3_move_tbl[1] = {
    em26_R1_Die_Normal,
};

// Parts index remap of the mirrored motions (cModel::motFlip).
static u16 em26_flip_tbl[32] = {
    0, 1, 2, 3, 4, 5, 0xA, 0xB, 0xC, 0xD, 6, 7, 8, 9, 0xE, 0x13,
    0x14, 0x15, 0x16, 0xF, 0x10, 0x11, 0x12, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0,
};

// Bite attack (em26AtkCk): range, type, damage, ...
static EmAtkInfo em26_atk_info = { 600.0f, 8, 0x12C, 4, 0xA, 0 };

// Per-frame update: damage check, clears the per-frame flags, the R0 table (Init / Move / Damage /
// Die), then the collision and scenario check and the breath SE.
void cEm26::move()
{
    Em26Work* w = EM26_WK(this);

    if (r_no_0) {
        em26DmCk(this);
    }
    w->flags &= ~0xF;
    if (r_no_0) {
        getPartsPtr(1)->scale.x = 1.0f;
        getPartsPtr(2)->scale.x = 1.0f;
    }
    Em26_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
}

// R0 == 0: creation. Builds the model of type 0 / 1 (ARC 5/6 or 6/7), mirrors half of the cows (flag
// bit4 -> the 0x41 motion flag), hp from the list (1000 default), IK / lock-on off, hit boxes on the
// head 5 and tail 0x18, the room's ctrl11 / ctrl12, and starts Wait with the idle motion 8.
static void em26_R0_Init(cEm26* em)
{
    Em26Work* w = EM26_WK(em);
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em26() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(4), ARC(6)) == 0) {
            pLog->err(0, 0, "em26() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    }
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->pXFlip = em26_flip_tbl;
    if (Rnd() & 1) {
        w->flags |= 0x10;
    } else {
        w->flags &= ~0x10;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 3, &ofs, &size, 2);
    }
    em->lockParts = 5;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    if (pGS->stage_no == 0 && pGS->room_no == 4) {
        if (em->hp < 0) {
            em->hp = 1000;
        }
    }
    AtariInit(&em->atari, 0.0f, 750.0f, 650.0f, 350.0f, 1250.0f, 1250.0f, 750.0f, 0, 2, 0);   // COMPILER-DIFF: #1
    zero = 0;
    em->setStatus(EM_STATUS_IK_OFF);
    em->setStatus(EM_STATUS_LOCKOFF);
    em->atari.m_flag &= ~0x100;
    em->atari.m_flag |= 0x10;
    YarareInit(em, 0.0f, -150.0f, -150.0f, 500.0f, 1200.0f, 2, 5);
    YarareAdd(em, &w->hit[0], 0.0f, -50.0f, -100.0f, 300.0f, 350.0f, 5, 5);
    YarareAdd(em, &w->hit[1], 0.0f, 0.0f, -200.0f, 100.0f, 200.0f, 0x18, 5);
    EspDataLoad((u32) ARC(7), 0x1E, 0);
    w->flags = zero;
    w->sndId = zero;
    w->breathTimer = Rnd() % 60 + 30;
    w->estTimer = Rnd() % 20 + 10;
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    w->x194 = zero;
    em->setStatus(EM_STATUS_ACTIVE);
    EmRoutineSet(em, 1, zero, zero, zero);
    if (w->flags & 0x10) {
        MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 0x41, 0);
    } else {
        MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 1, 0);
    }
    MotionMoveF(em, 0);
    em26_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em26_R1_move_tbl: Wait, Atk).
static void em26_R0_Move(cEm26* em)
{
    Em26_R1_move_tbl[em->r_no_1](em);
}

// R1 == 0 Wait: chews (ARC 8), now and then moos (ARC 9, voice 4) with the breath effect every
// 90..120 frames; once it took more than 500 damage since the last attack and the player stands
// within 1000 units of its head it bites (Atk 1).
static void em26_R1_Wait(cEm26* em)
{
    Em26Work* w = EM26_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->flags & 0x10) {
            MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 0x45, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 5, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0) && (Rnd() & 3) == 0) {
            em->r_no_2++;
        } else {
            em26BreathSe(em);
        }
        break;
    case 2:
        if (w->flags & 0x10) {
            MotionSetCore(em, MOTION(em), ARC(9), 0, 0, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(9), 0, 0, 1, 0);
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 4, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    }
    if (w->estTimer) {
        w->estTimer--;
    } else {
        w->estTimer = Rnd() % 30 + 90;
        EstSet((int) em, -1, 0, 0, 0x1E, 3, 0, 0, (u32) em, 0);
    }
    if (w->dmgTotal > 500) {
        cModel* p = em->getPartsPtr(4);

        if ((p->world.x - pPL->pos.x) * (p->world.x - pPL->pos.x)
                + (p->world.z - pPL->pos.z) * (p->world.z - pPL->pos.z) < 1000000.0f
            && fabsf(em->pos.y - pPL->pos.y) < 500.0f) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
    }
}

// R1 == 1 Atk: the head butt / bite to the side the player is on (ARC 0xE / 0xF), em26AtkCk on the
// hit frames, then back to Wait with dmgTotal reset.
static void em26_R1_Atk(cEm26* em)
{
    Em26Work* w = EM26_WK(em);

    switch (em->r_no_2) {
    case 0: {
        f32 ang = Muku(&em->pos, &pPL->pos, em->ang.y, PI);
        int mode = 1;

        if (w->flags & 0x10) {
            mode = 0x41;
        }
        if (ang < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0xE), (int) ARC(0x16), 0, mode, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xF), (int) ARC(0x17), 0, mode, 0);
        }
        w->atkHit = 0;
        em->r_no_2++;
    }
    case 1:
        if (em->seFlags28B & 1) {
            em26AtkCk(em);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
    if (w->estTimer) {
        w->estTimer--;
    } else {
        w->estTimer = Rnd() % 20 + 10;
        EstSet((int) em, -1, 0, 0, 0x1E, 3, 0, 0, (u32) em, 0);
    }
}

// R0 == 2: damage (flag bit3), runs Em26_R2_move_tbl (Dm_Small).
static void em26_R0_Damage(cEm26* em)
{
    Em26Work* w = EM26_WK(em);

    w->flags |= 8;
    Em26_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Small: the flinch by hit zone (head ARC 0xA, tail 0xC, body 0xB / 0x10) with the
// pain voice 8, then back to Wait.
static void em26_R1_Dm_Small(cEm26* em)
{
    Em26Work* w = EM26_WK(em);

    switch (em->r_no_2) {
    case 0: {
        YARARE_INFO* part = em->dmg.m_pDamageYarare;
        int mode = 1;
        int kind;

        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 8, &em->pos, em->id, 0, em);
        kind = part->partsNo == 5;
        if (part->partsNo == 0x18) {
            kind = 2;
        }
        if (w->flags & 0x10) {
            mode = 0x41;
        }
        switch ((u32) kind) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, mode, 0);
            break;
        case 1:
            if (Rnd() & 1) {
                MotionSetCore(em, MOTION(em), ARC(0xB), 0, 0, mode, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x10), 0, 0, mode, 0);
            }
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, mode, 0);
            break;
        }
        em->r_no_2++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

// R0 == 3: death (flag bit3), runs Em26_R3_move_tbl (Die_Normal).
static void em26_R0_Die(cEm26* em)
{
    Em26Work* w = EM26_WK(em);

    w->flags |= 8;
    Em26_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Normal: item drop (ITEMSET), the collapse motion ARC 0xD (blend by list slot)
// with the death voice, the body thud effect / SE on landing; the corpse stays.
static void em26_R1_Die_Normal(cEm26* em)
{
    Em26Work* w = EM26_WK(em);

    switch (em->r_no_2) {
    case 0: {
        void* seq;
        int mode;

        em->clearStatus(EM_STATUS_ACTIVE);
        em->setStatus(EM_STATUS_ITEMSET);
        EmSetDropItem(em);
        EmSetDie(em);
        switch (em->emset_no % 5) {
        case 0:
        default:
            seq = ARC(0x11);
            break;
        case 1:
            seq = ARC(0x12);
            break;
        case 2:
            seq = ARC(0x13);
            break;
        case 3:
            seq = ARC(0x14);
            break;
        case 4:
            seq = ARC(0x15);
            break;
        }
        mode = 1;
        if (w->flags & 0x10) {
            mode = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) seq, 0, mode, 0);
        em->atari.m_flag &= ~0x200;
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 8, &em->pos, em->id, 0, em);
        EstSet((int) em, -1, 0, 0, 0x1E, 2, 0, 0, (u32) em, 0);
        em->r_no_2++;
    }
    case 1:
        if (w->flags & 0x20) {
            cModelInfo* info;

            for (info = em->pModelInfo; info; info = info->pList) {
                if (info->color[0] > 0x20) {
                    info->color[0] -= 0x20;
                }
                info->color[2] = info->color[1] = info->color[0];
            }
        }
        if (MotionMoveF(em, 0)) {
            cModel* p = em->getPartsPtr(2);

            EstSet(0, -1, &p->world, &em->ang, 0x1E, 4, 0, 0, 0, 0);
            em->r_no_2++;
        }
        break;
    }
}

// Breath SE through the room's ctrl11 every breathTimer frames while alive.
void em26BreathSe(cEm26* em)
{
    Em26Work* w = EM26_WK(em);

    if (w->breathTimer) {
        w->breathTimer--;
    } else {
        w->breathTimer = Rnd() % 60 + 30;
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 6, &em->pos, em->id, 0, em);
    }
}

// Bite hit test on the attack frames: the head part 4's sweep against the player (em26_atk_info: 600
// range, 300 damage), once per attack (atkHit); a hit adds blood and vibration. 1 = hit.
int em26AtkCk(cEm26* em)
{
    Em26Work* w = EM26_WK(em);
    // Unreferenced constants the original keeps between em26_R1_Atk's pool and this one.
    static const f32 em26_atk_ofs[5] = { -400.0f, 0.0f, 2000.0f, -500.0f, 400.0f };

    if (w->atkHit) {
        return 0;
    }
    {
        EmAtkInfo* atk = &em26_atk_info;
        cModel* p = em->getPartsPtr(4);
        int hit = EmAtkHitCk(atk, &p->world, &p->world_old, 0);

        if (hit) {
            if (hit & 1) {
                EmPlBloodSet2(em, &p->world_old, 1, 0x1E, 8);
                w->atkHit = 1;
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            return 1;
        }
    }
    return 0;
}
