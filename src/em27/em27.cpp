// em27 module (D:/Bio4/Prog/em27.cpp): the lake fish. Swims towards a target point (the player while
// chaseTimer runs, else its home or a random point), with wait / walk / dash / bank / turn / jump
// routines, three damage reactions and a die routine that floats the body up to the surface.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em27.h"
#include "emhit.h"
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
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"



typedef void (*Em27Func)(cEm27*);

static void em27_R0_Init(cEm27* em);
static void em27_R0_Move(cEm27* em);
static void em27_R1_Wait(cEm27* em);
static void em27_R1_Walk(cEm27* em);
static void em27_R1_Dash(cEm27* em);
static void em27_R1_Bank(cEm27* em);
static void em27_R1_Turn180(cEm27* em);
static void em27_R1_Jump(cEm27* em);
static void em27_R0_Damage(cEm27* em);
static void em27_R1_Dm_Normal(cEm27* em);
static void em27_R1_Dm_Big(cEm27* em);
static void em27_R1_Dm_Air(cEm27* em);
static void em27_R0_Die(cEm27* em);
static void em27_R1_Die_Normal(cEm27* em);





// Module entry (SN loader): registers Em27Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em27 prolog Ok\n");
    EmInitFunc = Em27Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm27 class in the manager's work.
void Em27Init(cEm* em)
{
    new (em) cEm27();
}

// Per-frame damage check (cEm27::move): a weapon hit (not 0x14 / 0x16 / flash 0x17 / 0x2A) takes
// 999..1000 for guns / knife / TMP (one hit of the 1000 hp), 9999 for explosives, magnum, rifles and
// the mine, or 0 for the harpoon kinds 0x19 / 0x1F / 0x20 (they kill through their own code); a kill
// goes to Dm_Air (R2 2), a survivor to Dm_Normal / Dm_Big (a far shotgun hit prefers the small one).
void em27DmCk(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    int wep;
    int dmg;
    // COMPILER-DIFF: #13 (int shape): the EstSet stack zero is a function-scope constant with one
    // use in another block, so sched1 sees the store as a leaf and update_equiv_regs moves the
    // `li` next to it (r0, like the original's rematerialised reload).
    int zero;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
    zero = 0;
    if (wep == 0x14 || wep == 0x16 || wep == 0x17 || wep == 0x2A) {
        return;
    }
    switch (wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 7:
    case 8:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1A:
    case 0x1B:
    case 0x21:
        dmg = (Rnd() & 1) + 999;
        break;
    case 0x19:
    case 0x1F:
    case 0x20:
        return;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x2C:
    case 0x2D:
    default:
        dmg = 9999;
        break;
    }
    LifeDownSet2(em, dmg, 0, 0);
    if (em->hp > 0) {
        if (em->pos.y > w->Water_h + 300.0f) {
            EmDmBloodSet2(em, 0x1F, 6, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x1F, 7, 0, 0, 0);
            EstSet(em, -1, 0, 0, EFF_EM27, 8, 0, ESP_CORE_KIND_NONE, em, (void*) zero);
        }
    }
    em->invisible_factor = 1.0f;
    if (em->hp <= 0) {
        EmSetDie(em);
        if (em->pos.y > w->Water_h) {
            em->setRno(2, 2, 0, 0);
        } else if (Rnd() & 1) {
            em->setRno(2, 0, 0, 0);
        } else {
            em->setRno(2, 1, 0, 0);
        }
    } else {
        // Two identical arms behind a dmWep == 0x21 test: jump2 cross-jumps the whole first arm
        // into the second and only the dead `lbz dmWep; cmpwi 0x21` survives (jump2 runs after
        // flow2). The first arm must be written else-first (`!(a > b)`) so that after its
        // sub-arms are merged it reads `ble E2; b T2`, identical to the second arm's head.
        if (em->dmg.m_Wep == 0x21) {
            if (!(em->pos.y > w->Water_h)) {
                em->setRno(2, 0, 0, 0);
            } else {
                em->setRno(2, 2, 0, 0);
            }
        } else {
            if (em->pos.y > w->Water_h) {
                em->setRno(2, 2, 0, 0);
            } else {
                em->setRno(2, 0, 0, 0);
            }
        }
    }
}

Em27Func Em27_R0_move_tbl[4] = {
    em27_R0_Init,
    em27_R0_Move,
    em27_R0_Damage,
    em27_R0_Die,
};

static Em27Func Em27_R1_move_tbl[6] = {
    em27_R1_Wait,
    em27_R1_Walk,
    em27_R1_Dash,
    em27_R1_Bank,
    em27_R1_Turn180,
    em27_R1_Jump,
};

static Em27Func Em27_R2_move_tbl[3] = {
    em27_R1_Dm_Normal,
    em27_R1_Dm_Big,
    em27_R1_Dm_Air,
};

static Em27Func Em27_R3_move_tbl[1] = {
    em27_R1_Die_Normal,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em27_flip_tbl[24] = {
    0, 1, 2, 3, 4, 5, 6, 8, 7, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0, 0, 0,
};

// Per-frame update: room 10B hides the fish while the lake boss fight is on (Status_flg[1] bit21);
// damage check, the dash / escape timers, the water surface height (GetWaterHeight unless the room
// set it), the swim target: away from the player while Esc_timer runs (started when he comes within
// 500 units, on the room alert, or at random), else home or a random point within 10000; then the R0
// table, the fin scale relax, collision, the push-apart (em27ObaHitCk) and the airborne scenario check.
void cEm27::move()
{
    Em27Work* w = EM27_WK(this);
    Vec v;
    f32 wh;

    if (pG->room_id == 0x10B && (StaFlagChk(pG, STA_PL_BOAT)) && hp > 0) {
        be_flag &= ~2;
        be_flag &= ~0x20;
        return;
    }
    if (r_no_0 != 0) {
        em27DmCk(this);
    }
    if (w->Dash_wait) {
        w->Dash_wait--;
    }
    if (w->Esc_timer) {
        w->Esc_timer--;
    }
    w->Be_flg &= ~0xD8;
    if (!(w->Be_flg & 0x100)) {
        if (GetWaterHeight(&pos, &wh)) {
            w->Water_h = wh - 300.0f;
        }
    }
    if (l_pl < 250000.0f) {
        w->Esc_timer = Rnd() % 60 + 60;
    }
    if (StaFlagChk(pG, STA_PL_FIRE)) {
        w->Esc_timer = Rnd() % 60 + 60;
    }
    if ((u8) pG->Frame_cnt == emset_no % 0x100) {
        if (Rnd() & 1) {
            w->Esc_timer = Rnd() % 60 + 60;
        }
    }
    if (w->Esc_timer) {
        PSVECSubtract(&pos, &pPL->pos, &v);
#line 379 "D:/Bio4/Prog/em27.cpp"
        VECNormalize(&v, &v);
        PSVECScale(&v, &v, 10000.0f);
        PSVECAdd(&pPL->pos, &v, &w->target);
    } else if (w->Go_timer) {
        w->Go_timer = 0;
    } else {
        w->Go_timer = Rnd() % 120 + 120;
        if (Rnd() & 1) {
            w->target = w->St_pos;
        } else {
            v.x = fRand1_1() * 10000.0f;
            v.y = 0.0f;
            v.z = fRand1_1() * 10000.0f;
            PSMTXMultVec(mat, &v, &w->target);
        }
    }
    Em27_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em27ScaleReset(this);
    partsWorldCalc();
    EmAtCheck(this);
    em27ObaHitCk(this);
    atari.move();
    SatMgr.checkAir(this, 0);
    em27WaterEffSet(this);
}

// R0 == 0: creation. Builds the model (ARC 5/6, mirrored parts), hp 1000, no Ashley help / lock-on,
// collision, hit boxes, home = Start_pos, the room's ctrl11 / ctrl12, Dash_wait 210..360, and Wait.
static void em27_R0_Init(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    cAtariInfo* at;
    Vec* pos;
    f32 scale;
    f32 wh;
    int zero;

    em->ot_type = 0;
    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em27() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    em->be_flag &= ~0x10;
    em->Motion.flip = em27_flip_tbl;
    em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 300.0f, 300.0f, 300.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    zero = 0;
    em->setTarget(zero, 0.0f, 0.0f, 0.0f);
    scale = fRand0_1() * 0.5f + 1.0f;
    if (em->type == 1) {
        scale = 3.0f;
    }
    at = &em->atari;
    pos = &em->pos;
    em->scale.x = scale;
    em->scale.y = scale;
    em->scale.z = scale;
    at->init(0.0f, 0.0f, 0.0f, 250.0f, 100.0f, 100.0f, 100.0f, 1, 0x2800, 10);
    em->atari.m_flag &= 0xFDFF;
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    YarareInit(em, 0.0f, 0.0f, -100.0f, 100.0f, 250.0f, 5, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    EspDataLoad((u32) ARC(6), EFF_EM27, 0);
    w->Be_flg = zero;
    w->Dash_wait = Rnd() % 150 + 210;
    w->Esc_timer = zero;
    w->Spd.x = 0.0f;
    w->Spd.y = 0.0f;
    w->Spd.z = 0.0f;
    w->Spd_t.x = 0.0f;
    w->Spd_t.y = 0.0f;
    w->Spd_t.z = 0.0f;
    w->Go_timer = zero;
    w->St_pos = *pos;
    w->Water_h = 400.0f;
    if (GetWaterHeight(pos, &wh)) {
        w->Water_h = wh;
    }
    if (em->pos.y > w->Water_h) {
        em->pos.y = w->Water_h;
    }
    w->Start_pos = *pos;
    w->Start_ang = em->ang;
    w->pCtrlPlAvoid = GetCtrlCtrl11();
    w->pCtrlGroup = GetCtrlCtrl12();
    em->setStatus(EM_STATUS_LOCKOFF);
    at->offOba();
    em->setRno(1, zero, zero, zero);
    em->ang.y = fRand1_1() * PI;
    MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
    MotionMove(em, 0);
    em27_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em27_R1_move_tbl: Wait, Walk, Dash, Bank, Turn180, Jump).
static void em27_R0_Move(cEm27* em)
{
    Em27_R1_move_tbl[em->r_no_1](em);
}

// R1 == 0 Wait: hovers (one of two idle motions) for Timer frames, then Jump (5, one in eight when
// clear of the boat), Dash (2) when Dash_wait ran out, Walk (1) or Turn180 (4).
static void em27_R1_Wait(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xE), 0, 0, 5, 0);
        }
        w->Timer = (Rnd() & 0x3C) + 60;
        w->Spd_t.x = 0.0f;
        w->Spd_t.y = 0.0f;
        w->Spd_t.z = 0.0f;
        w->Spd_t.z = fRand1_1() * 10.0f + 10.0f;
        em->r_no_2++;
    case 1:
        em27SetSPeed(em, 0.1f);
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
        } else if ((Rnd() & 7) == 0 && em27JumpCk(em)) {
            em->setRno(1, 5, 0, 0);
        } else if (w->Dash_wait == 0) {
            em->setRno(1, 2, 0, 0);
        } else if (Rnd() & 3) {
            em->setRno(1, 1, 0, 0);
        } else {
            em->setRno(1, 4, 0, 0);
        }
        break;
    }
}

// R1 == 1 Walk: swims towards target (em27SetSPeed, turning PI/32 per frame); after each motion loop
// may Jump (5) when far from the target, Bank (3), Dash (2), or return to Wait (0).
static void em27_R1_Walk(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(8), 0, 5, 5, 0);
            w->Spd_t.z = fRand1_1() * 25.0f + 30.0f;
        } else {
            MotionSetCore(em, MOTION(em), ARC(9), 0, 5, 5, 0);
            w->Spd_t.z = fRand1_1() * 25.0f + 60.0f;
        }
        w->Spd_t.x = 0.0f;
        w->Spd_t.y = fRand1_1() * 10.0f;
        w->Timer = (Rnd() & 0x3C) + 60;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->target, em->ang.y, PI / 32.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em27SetSPeed(em, 0.1f);
        if (MotionMove(em, 0) && (Rnd() & 3) == 0) {
            if (w->L_go > 9000000.0f && (Rnd() & 3) == 0 && em27JumpCk(em)) {
                em->setRno(1, 5, 0, 0);
            } else {
                em->setRno(1, 3, 0, 0);
            }
        } else if (w->Dash_wait == 0) {
            em->setRno(1, 2, 0, 0);
        } else if (w->Timer) {
            w->Timer--;
        } else {
            em->setRno(1, 0, 0, 0);
        }
        break;
    }
}

// R1 == 2 Dash: the fast swim towards target (faster turn), Dash_wait 210..270, back to Walk (1) when
// the motion ends.
static void em27_R1_Dash(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 5, 5, 0);
        w->Timer = Rnd() % 6 + 7;
        w->Dash_wait = Rnd() % 60 + 210;
        w->Spd_t.x = 0.0f;
        w->Spd_t.y = fRand1_1() * 15.0f;
        w->Spd_t.z = 130.0f;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->target, em->ang.y, PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em27SetSPeed(em, 0.5f);
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
        } else {
            em->setRno(1, 1, 0, 0);
            w->Dash_wait = Rnd() % 150 + 210;
        }
        break;
    }
}

// R1 == 3 Bank: a banking turn (mirrored by the target side) towards target, then Walk (1).
static void em27_R1_Bank(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->r_no_2) {
    case 0: {
        Vec v;

        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 5, 5, 0);
        w->Spd_t.z = fRand1_1() * 25.0f + 100.0f;
        w->Spd_t.x = 0.0f;
        w->Spd_t.y = fRand1_1() * 10.0f;
        v = em->pos;
        v.y = w->Water_h + 300.0f;
        EstSet(0, -1, &v, &em->ang, EFF_EM27, 0xF, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(8, 0, &em->pos, em->id, 0, em);
        em->r_no_2++;
    }
    case 1:
        em->ang.y += Muku(&em->pos, &w->target, em->ang.y, PI / 32.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em27SetSPeed(em, 0.1f);
        if (MotionMove(em, 0)) {
            em->setRno(1, 1, 0, 0);
        }
        break;
    }
}

// R1 == 4 Turn180: turns around (random side / motion variant), then Dash (2) or Walk (1).
static void em27_R1_Turn180(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->r_no_2) {
    case 0: {
        void* m;
        int flag;

        if (Rnd() & 1) {
            m = ARC(0xB);
        } else {
            m = ARC(0xC);
        }
        if (Rnd() & 1) {
            flag = 1;
        } else {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), m, 0, 5, flag, 0);
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (w->Dash_wait == 0) {
                em->setRno(1, 2, 0, 0);
            } else {
                em->setRno(1, 1, 0, 0);
            }
        }
        break;
    }
}

// R1 == 5 Jump: leaps out of the water (one of two jump motions, mirrored at random) with the
// surface splash (em27WaterEffSet), then Dash (2) or Walk (1).
static void em27_R1_Jump(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->r_no_2) {
    case 0: {
        void* m0;
        void* m1;
        int flag;

        switch (Rnd() & 1) {
        case 0:
        default:
            m0 = ARC(0xF);
            m1 = ARC(0x1D);
            break;
        case 1:
            m0 = ARC(0x10);
            m1 = ARC(0x1E);
            break;
        }
        if (Rnd() & 1) {
            flag = 0;
        } else {
            flag = 0x40;
        }
        MotionSetCore(em, MOTION(em), m0, m1, 5, flag, 0);
        em->r_no_2++;
    }
    case 1:
        if (em27MotionMoveScale(em)) {
            if (w->Dash_wait == 0) {
                em->setRno(1, 2, 0, 0);
            } else {
                em->setRno(1, 1, 0, 0);
            }
        }
        break;
    }
}

// R0 == 2: damage (Be_flg bit3), runs Em27_R2_move_tbl (Dm_Normal, Dm_Big, Dm_Air).
static void em27_R0_Damage(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    w->Be_flg |= 8;
    Em27_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Normal: the small flinch turned to the hit direction (two variants), then Dash (2);
// a dead fish goes to Die_Normal on the motion's key frame.
static void em27_R1_Dm_Normal(cEm27* em)
{
    switch (em->r_no_2) {
    case 0: {
        int flag;

        em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
        if (Rnd() & 1) {
            flag = 0;
            em->r_no_3 = flag;
        } else {
            em->r_no_3 = 1;
            flag = 0x40;
        }
        MotionSetCore(em, MOTION(em), ARC(0x11), ARC(0x1F), 3, flag, 0);
        em->r_no_2++;
    }
    case 1:
        if (em27MotionMoveScale(em)) {
            em->setRno(1, 2, 0, 0);
        }
        if ((em->Motion.Seq_old.Free & 4) && em->hp <= 0) {
            em->r_no_0 = 3;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
        }
        break;
    }
}

// R0 2 / R1 == 1 Dm_Big: the big thrash (mirrored by r_no_3) turned to the hit direction, then Die_Normal.
static void em27_R1_Dm_Big(cEm27* em)
{
    switch (em->r_no_2) {
    case 0: {
        int flag;

        em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
        em->r_no_3 = Rnd() & 1;
        if (em->r_no_3) {
            flag = 0x40;
        } else {
            flag = 0;
        }
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 3, flag, 0);
        em->r_no_2++;
    }
    case 1:
        if (em27MotionMoveScale(em)) {
            em->r_no_0 = 3;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
        }
        break;
    }
}

// R0 2 / R1 == 2 Dm_Air: shot into a leap out of the water: the jump (mirrored by r_no_3), the fall
// back through the surface with the splash, then Die_Normal when dead or Dash (2).
static void em27_R1_Dm_Air(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    Mtx m;
    Vec v;
    int flag;

    switch (em->r_no_2) {
    case 0:
        em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
        em->r_no_3 = Rnd() & 1;
        if (em->r_no_3) {
            flag = 0x41;
        } else {
            flag = 1;
        }
        MotionSetCore(em, MOTION(em), ARC(0x13), 0, 3, flag, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        if (em->r_no_3) {
            flag = 0x41;
        } else {
            flag = 1;
        }
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 3, flag, 0);
        w->Spd.x = 0.0f;
        w->Spd.y = -100.0f;
        w->Spd.z = 0.0f;
        em->r_no_2++;
    case 3:
        w->Spd.y -= 5.0f;
        PSMTXRotRad(m, 'y', em->ang.y);
        PSMTXMultVecSR(m, &w->Spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        MotionMove(em, 0);
        if (em->pos.y < w->Water_h) {
            em->r_no_2++;
        }
        break;
    case 4:
        em->r_no_2++;
    case 5:
        w->Spd.y += 20.0f;
        PSMTXRotRad(m, 'y', em->ang.y);
        PSMTXMultVecSR(m, &w->Spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        if (em->pos.y < w->Water_h - 300.0f) {
            em->pos.y = w->Water_h - 300.0f;
            w->Spd.y = 0.0f;
        }
        MotionMove(em, 0);
        if (w->Spd.y > 0.0f) {
            if (em->hp <= 0) {
                em->r_no_0 = 3;
                em->r_no_1 = 0;
                em->r_no_2 = 0;
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 6:
        if (em->r_no_3) {
            flag = 0x41;
        } else {
            flag = 1;
        }
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, flag, 0);
        em->r_no_2++;
    case 7:
        if (MotionMove(em, 0)) {
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
}

// R0 == 3: death (Be_flg bit3), runs Em27_R3_move_tbl (Die_Normal).
static void em27_R0_Die(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    w->Be_flg |= 8;
    Em27_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Normal: the death thrash (ARC 0x15 / 0x1C by Die_type), counts the kill in the
// room's ctrl12 EM27_DIE counter, the item drop, then the body floats up to the surface (upDown) and
// bobs there (0x1B / 0x1C) with splashes every 5..65 frames.
static void em27_R1_Die_Normal(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    int flag;
    int no;

    w->Be_flg |= 0x80;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            flag = 0x41;
        } else {
            flag = 1;
        }
        w->Die_type = Rnd() & 1;
        if (w->Die_type) {
            MotionSetCore(em, MOTION(em), ARC(0x15), 0, 10, flag, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1C), 0, 15, flag, 0);
        }
        Ctrl12CntAdd(w->pCtrlGroup, CTRL12_ID_CNT_EM27_DIE, 1);
        em->atari.m_flag &= 0xFCFF;
        w->Timer = 120;
        w->Spd.y = fRand1_1() * PI;
        w->Timer2 = 0;
        switch (em->type) {
        case 0:
        default:
            no = SceAtCreateItemAt(&em->pos, 0x95, 0, -1, -1, 0, -1);
            break;
        case 1:
            no = SceAtCreateItemAt(&em->pos, 0x97, 0, -1, -1, 0, -1);
            break;
        }
        SceAtSetItemModel(no, em);
        em->r_no_2++;
    case 1:
        w->Spd.y += PI / 32.0f;
        w->Spd.y = LIMIT_ANGLE(w->Spd.y);
        if (w->Timer2) {
            em->pos.y += sinf(w->Spd.y) * 3.0f;
        } else {
            em->pos.y += 5.0f;
            if (em->pos.y > w->Water_h + 150.0f) {
                em->pos.y = w->Water_h + 150.0f;
                w->Timer2 = 1;
            }
        }
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        w->Timer = Rnd() % 60 + 60;
        w->Timer2 = 0;
        w->Spd.y = fRand1_1() * PI;
        em->r_no_2++;
    case 3:
        w->Spd.y += PI / 32.0f;
        w->Spd.y = LIMIT_ANGLE(w->Spd.y);
        em->pos.y = SINF(w->Spd.y) * 3.0f + em->pos.y;
        if (em->pos.y > w->Water_h + 150.0f) {
            em->pos.y = em->pos.y * 0.9f + (w->Water_h + 150.0f) * 0.1f;
        }
        if (w->Timer2) {
            w->Timer2--;
        } else {
            cParts* p = em->getPartsPtr(4);
            Vec v;

            flag = 1;
            v.x = p->world.x;
            v.y = w->Water_h + 300.0f;
            v.z = p->world.z;
            EstSet(0, -1, &v, &em->ang, EFF_EM27, 5, 0, ESP_CORE_KIND_NONE, 0, 0);
            w->Timer2 = Rnd() % 60 + 5;
            if (em->r_no_3) {
                flag = 0x41;
            }
            if (w->Die_type) {
                MotionSetCore(em, MOTION(em), ARC(0x1B), 0, 10, flag, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x1C), 0, 10, flag, 0);
            }
        }
        MotionMove(em, 0);
        break;
    }
}

// Blends the current speed Spd towards the wanted Spd_t by `rate`, moves the fish along its heading
// and keeps it between the surface (Water_h) and 300 below.
void em27SetSPeed(cEm27* em, f32 rate)
{
    Em27Work* w = EM27_WK(em);
    Mtx m;
    Vec v;

    w->Spd.x = w->Spd.x * (1.0f - rate) + w->Spd_t.x * rate;
    w->Spd.y = w->Spd.y * (1.0f - rate) + w->Spd_t.y * rate;
    w->Spd.z = w->Spd.z * (1.0f - rate) + w->Spd_t.z * rate;
    PSMTXRotRad(m, 'y', em->ang.y);
    PSMTXMultVecSR(m, &w->Spd, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    if (em->pos.y < w->Water_h - 300.0f) {
        em->pos.y = w->Water_h - 300.0f;
    }
    if (em->pos.y > w->Water_h) {
        em->pos.y = w->Water_h;
    }
}

// Relaxes the fin parts 1 / 3 back to scale 1 (10% per frame) unless the routine holds them (Be_flg bit4).
void em27ScaleReset(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    cParts* p;

    if (w->Be_flg & 0x10) {
        return;
    }
    p = em->getPartsPtr(1);
    p->scale.x = p->scale.x * 0.9f + 0.1f;
    p->scale.y = p->scale.y * 0.9f + 0.1f;
    p->scale.z = p->scale.z * 0.9f + 0.1f;
    p = em->getPartsPtr(3);
    p->scale.x = p->scale.x * 0.9f + 0.1f;
    p->scale.y = p->scale.y * 0.9f + 0.1f;
    p->scale.z = p->scale.z * 0.9f + 0.1f;
}

// Pushes the fish out of other alive enemies (within 200 units: moved to 0.9 of the distance + 20) and
// out of the player's collision radius + 100, unless Be_flg bit6 (no push).
void em27ObaHitCk(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    Vec d;
    f32 dist;
    f32 r;
    u32 i;

    if (w->Be_flg & 0x40) {
        return;
    }
    if (!(em->be_flag & 2)) {
        return;
    }
    if (em->hp <= 0) {
        return;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.at(i);

        {
            int dead = !(e->be_flag & 1);

            if (dead) {
                continue;
            }
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->pList == 0) {
            continue;
        }
        PSVECSubtract(&em->pos, &e->pos, &d);
        dist = d.x * d.x + d.y * d.y + d.z * d.z;
        if (dist > 40000.0f) {
            continue;
        }
        if (dist <= 0.0f) {
            continue;
        }
        dist = SQRTF(dist) * 0.9f + 20.0f;
#line 1315 "D:/Bio4/Prog/em27.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, dist);
        PSVECAdd(&e->pos, &d, &em->pos);
    }
    PSVECSubtract(&em->pos, &pPL->pos, &d);
    d.y = 0.0f;
    dist = d.x * d.x + d.z * d.z;
    r = pPL->atari.m_radius2 + 100.0f;
    if (dist > r * r) {
        return;
    }
    if (dist <= 0.0f) {
        return;
    }
#line 1327 "D:/Bio4/Prog/em27.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, r);
    em->pos.x = pPL->pos.x + d.x;
    em->pos.z = pPL->pos.z + d.z;
    PartsWorldPosCalc(em);
}

// MotionMove with the motion's translation divided by the model scale (so a scaled fish swims at the
// authored speed); returns the motion-ended flag.
int em27MotionMoveScale(cEm27* em)
{
    Vec spd;
    Vec rot;
    Vec inv;
    int ret;
    cParts* p;

    inv.x = 1.0f / em->scale.x;
    inv.y = 1.0f / em->scale.y;
    inv.z = 1.0f / em->scale.z;
    MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
    spd.x *= inv.x;
    spd.y *= inv.y;
    spd.z *= inv.z;
    MotionAddSpeed(em, MOTION(em), &spd, &rot);
    ret = MotionMove(em, 0);
    p = em->getPartsPtr(0);
    p->pos.y *= inv.y;
    p->l_mat[1][3] *= inv.y;
    return ret;
}

// Surface crossing effects: when the root part passes the surface (Water_h + 300) upwards the
// leap splash / jump SE, downwards the dive splash / SE, both pushing the water (AddWaterPower);
// off while dying (Be_flg bit7).
void em27WaterEffSet(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    Vec v;
    f32 h;
    cParts* p;

    if (w->Be_flg & 0x80) {
        return;
    }
    if (!(em->be_flag & 2)) {
        return;
    }
    h = w->Water_h + 300.0f;
    v = em->pos;
    v.y = h;
    p = em->getPartsPtr(0);
    if (p->world.y > h && p->world_old.y < h) {
        AddWaterPower(em->pos, -0.5f);
        EstSet(0, -1, &v, &em->ang, EFF_EM27, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
        EstSet(em, -1, 0, 0, EFF_EM27, 4, 0, ESP_CORE_KIND_NONE, em, 0);
        SndCall(8, (Rnd() & 3) | 4, &em->pos, em->id, 0, em);
    }
    if (p->world.y < h && p->world_old.y > h) {
        AddWaterPower(em->pos, 0.5f);
        EstSet(0, -1, &v, &em->ang, EFF_EM27, 1, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(8, (Rnd() & 1) + 7, &em->pos, em->id, 0, em);
    }
}

// 1 when no boat (enemy id 0xF) is within 2000 units: the fish may leap.
int em27JumpCk(cEm27* em)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (e->isAlive() && e->id == 0xF) {
            if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z)
                < 4000000.0f) {
                return 0;
            }
        }
    }
    return 1;
}

// Room script: fixes the surface height (Be_flg bit8 stops the GetWaterHeight lookup).
void cEm27::setWaterHeight(f32 h)
{
    Em27Work* w = EM27_WK(this);

    w->Water_h = h;
    w->Be_flg |= 0x100;
}
