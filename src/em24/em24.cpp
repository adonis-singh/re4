// em24 module (D:/Bio4/Prog/em24.cpp): a small enemy that either waits in a box and jumps out at the
// player (routine 1/0, work flag bit2), or wanders freely (1/2) and coils up when the player comes
// near (1/1, 1/3). Dies to any damage volume of kind 1/4/5/7 and to most weapons; a random weapon
// item drops at death (em24_R0_Die).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "em24.h"
#include "emhit.h"
#include "emwep.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "quake.h"
#include "pad.h"
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



typedef void (*Em24Func)(cEm24*);

static void em24_R0_Init(cEm24* em);
static void em24_R0_Move(cEm24* em);
static void em24_R1_BoxWait(cEm24* em);
static void em24_R1_CoilWait(cEm24* em);
static void em24_R1_Free(cEm24* em);
static void em24_R1_Coil(cEm24* em);
static void em24_R0_Damage(cEm24* em);
static void em24_R0_Die(cEm24* em);




// Module entry (SN loader): registers Em24Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em24 prolog Ok\n");
    EmInitFunc = Em24Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm24 class in the manager's work.
void Em24Init(cEm* em)
{
    new (em) cEm24();
}

// Per-frame damage check (cEm24::move): an explosion / fire volume (kind 1/4/5/7) or any weapon hit
// except 0x14 / 0x16 / flash 0x17 / 0x2A / the mine 0xE kills the snake at once (hit SE 8, blood 0x1C,
// dmType 0x80 = no more damage, R0_Die).
void em24DmCk(cEm24* em)
{
    Em24Work* w = EM24_WK(em);
    int wep;

    if (em->hp > 0 && !EmDeadCk(em)) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            goto die;
        }
    }
    if (em->dmg.m_Flag) {
        wep = em->dmg.m_Wep;
        em->dmg.m_Flag = 0;
        if (wep == 0x14 || wep == 0x16 || wep == 0x17 || wep == 0x2A || wep == 0xE) {
            return;
        }
        SndCall(8, 8, &em->pos, em->id, 0, em);
        if (w->Be_flg & 0x20) {
            EmDmBloodSet2(em, 0x1C, 2, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x1C, 0, 0, 0, 0);
        }
    die:
        em->hp = 0;
        em->dmg.m_Timer = 0x80;
        em->setRno(3, 0, 0, 0);
    }
}

Em24Func Em24_R0_move_tbl[4] = {
    em24_R0_Init,
    em24_R0_Move,
    em24_R0_Damage,
    em24_R0_Die,
};

static Em24Func Em24_R1_move_tbl[4] = {
    em24_R1_BoxWait,
    em24_R1_CoilWait,
    em24_R1_Free,
    em24_R1_Coil,
};

// Jump attack (em24AtkCk): range, type, damage, ...
static EmAtkInfo em24_atk_tbl[1] = {
    { 500.0f, PL_DM_AUTO, 100, 4, 0xA, 0 },
};

// Per-frame update: damage check, the R0 table (Init / Move / Damage / Die), then the collision and
// scenario check (checkAir in box mode / while jumping, else snapped to the floor), the slope tilt
// (em24SlopeMove) and the in-water splash effects (Be_flg bit5).
void cEm24::move()
{
    Em24Work* w = EM24_WK(this);
    f32 spd;

    em24DmCk(this);
    w->Be_flg &= ~4;
    Em24_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em24SlopeMove(this);
    partsWorldCalc();
    w->Be_flg &= ~0x20;
    if (w->Be_flg & 0x10) {
        return;
    }
    spd = VEC_DISTXZ(&pos_old, &pos);
    EmAtCheck(this);
    atari.move();
    if (hp > 0) {
        if (w->Be_flg & 4) {
            SatMgr.checkAir(this, 0);
        } else {
            f32 wh;

            pos.y = SatMgr.getFloor(&pos, 0, 600.0f, 100000.0f, 0);
            if (GetWaterHeight(&pos, &wh)) {
                if (pos.y < wh) {
                    w->Be_flg |= 0x20;
                    pos.y = wh;
                    if (hp > 0) {
                        if (w->Water_eff_wait) {
                            w->Water_eff_wait--;
                        } else {
                            w->Water_eff_wait = 2;
                            EstSet(this, -1, 0, 0, EFF_EM24, 4, 0, ESP_CORE_KIND_NONE, this, 0);
                        }
                    }
                }
            }
            SatMgr.checkAir(this, 0);
        }
    }
    if (VEC_DISTXZ(&pos, &pos_old) < spd * 0.5f) {
        w->HoseiCnt++;
    } else {
        w->HoseiCnt = 0;
    }
}

// R0 == 0: creation. Builds the model (ARC 5/6) at a random 1.1..1.25 scale, no lock-on / no Ashley
// help, collision and hit boxes, and the start routine by cEm::set: 0 BoxWait (the snake in the box,
// Be_flg bit2), else Free (2).
static void em24_R0_Init(cEm24* em)
{
    Em24Work* w = EM24_WK(em);
    cAtariInfo* at;
    f32 scale;
    int zero;
    int two;

    if (em->modelInit(ARC(5), ARC(6)) == 0) {
        pLog->err(0, 0, "em24() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    scale = (f32) (int) Rnd() * 0.15f * (1.0f / 256.0f) + 1.1f;
    em->scale.x = scale;
    em->scale.y = scale;
    em->scale.z = scale;
    zero = 0;
    two = 2;
    EspDataLoad((u32) ARC(4), EFF_EM24, 0);
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 1000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    at = &em->atari;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    at->init(0.0f, -50.0f, 0.0f, 350.0f, 150.0f, 150.0f, 100.0f, 1, 0x2000, 10);
    at->offOba();
    em->setStatus(EM_STATUS_LOCKOFF);
    em->be_flag &= ~0x10;
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 3, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 6, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 8, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 0xA, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 0xC, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    w->Be_flg = zero;
    w->HoseiCnt = zero;
    w->Water_eff_wait = two;
    w->slopeRot.x = 0.0f;
    w->slopeRot.y = 0.0f;
    w->slopeRot.z = 0.0f;
    switch (em->set) {
    default:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0, 5, 0);
        MotionMove(em, 0);
        em->setRno(1, two, zero, zero);
        break;
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0, 5, 0);
        MotionMove(em, 0);
        em->setRno(1, 0, 0, 0);
        break;
    }
    em24_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em24_R1_move_tbl: BoxWait, CoilWait, Free, Coil).
static void em24_R0_Move(cEm24* em)
{
    Em24_R1_move_tbl[em->r_no_1](em);
}

// R1 == 0 BoxWait: coiled inside the box (ARC 0x17); when the box opens with the player close it
// springs at him (ARC 0x11, spd arc, the bite em24AtkCk at the head part 5), lands on the floor and
// goes Free (2).
static void em24_R1_BoxWait(cEm24* em)
{
    Em24Work* w = EM24_WK(em);

    w->Be_flg |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 5, 0);
        em->atari.off();
        w->Timer = 45;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, PI);
        MotionMove(em, 0);
        if (!(em->flag & 1)) {
            em->dmg.m_Timer = 2;
            break;
        }
        if (w->Timer) {
            w->Timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x11), ARC(0x18), 3, 1, 0);
        w->spd.x = 0.0f;
        w->spd.y = -100.0f;
        w->spd.z = 200.0f;
        w->motEnd = 0;
        w->Atk_ck = 0;
        SndCall(8, 4, &em->pos, em->id, 0, em);
        EstSet(em, -1, 0, 0, EFF_EM24, 5, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 3: {
        int two = 2;

        em->dmg.m_Timer = two;
        if (em->Motion.Seq_old.Free & 1) {
            cParts* p = em->getPartsPtr(5);

            em24AtkCk(em, &p->world, &p->world_old, 0);
        }
        if (!(em->Motion.Seq_old.Free & 0x80)) {
            cAtariInfo* at = &em->atari;
            Vec v;
            f32 fl;

            at->m_flag |= 0x100;
            PSMTXMultVecSR(em->mat, &w->spd, &v);
            PSVECAdd(&em->pos, &v, &em->pos);
            w->spd.y -= 20.0f;
            fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                w->spd.y = 0.0f;
                if (MotionMove(em, 0)) {
                    w->motEnd = 1;
                }
                if (w->motEnd) {
                    at->m_flag |= 0x100;
                    em->setRno(1, two, 0, 0);
                    break;
                }
            }
        }
        if (MotionMove(em, 0)) {
            w->motEnd = 1;
        }
        break;
    }
    }
}

// R1 == 1 CoilWait: coiled idle (ARC 0xF) until the player comes near, uncoils (0x12) and goes Free (2).
static void em24_R1_CoilWait(cEm24* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em->l_pl < 9000000.0f) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 0, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
}

// R1 == 2 Free: slithers about (ARC 7 / 8), turning towards a new random Target_dir every Timer2
// frames, pauses (9 / 0xA); coils up (Coil 3) when the player comes close.
static void em24_R1_Free(cEm24* em)
{
    Em24Work* w = EM24_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(7), 0, 3, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(8), 0, 3, 5, 0);
        }
        w->motEnd = Rnd() % 3;
        w->Target_dir = GetXZAngle(&pPL->pos, &em->pos);
        w->Target_dir += fRand1_1() * (PI / 2.0f);
        w->Target_dir = LIMIT_ANGLE(w->Target_dir);
        w->Timer = Rnd() % 3 + 3;
        w->Timer2 = Rnd() % 30 + 30;
        w->HoseiCnt = 0;
        em->r_no_2++;
    case 1:
        if (w->Timer2) {
            w->Timer2--;
        } else {
            w->Timer2 = Rnd() % 15 + 15;
            w->Target_dir += fRand1_1() * (PI / 4.0f);
            w->Target_dir = LIMIT_ANGLE(w->Target_dir);
        }
        em->ang.y += Muku2(em->ang.y, w->Target_dir, PI / 128.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            if (w->HoseiCnt > 1) {
                em->r_no_2++;
            }
        }
        break;
    case 2:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(9), 0, 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xA), 0, 3, 1, 0);
        }
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    }
}

// R1 == 3 Coil: coils up (ARC 0xE), stays coiled facing the player (0xF), uncoils (0x12) and goes
// back to Free (2) when he moves away.
static void em24_R1_Coil(cEm24* em)
{
    Em24Work* w = EM24_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE), 0, 3, 5, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 3, 5, 0);
        w->Timer = Rnd() % 90 + 90;
        em->r_no_2++;
    case 3:
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, PI / 64.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 3, 5, 0);
        em->r_no_2++;
    case 5:
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, PI / 64.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
}

// R0 == 2: the snake has no damage reaction; straight back to Free (R1 2).
static void em24_R0_Damage(cEm24* em)
{
    em->setRno(1, 2, 0, 0);
}

// R0 == 3: death. The death motion (ARC 0x15 in the box / 0x13), the burst effect, then drops a
// random weapon item (cEmWep + SceAtCreateItemAt: id 8 mostly, 9 or 0xA rarer) and fades out
// (invisible_factor -0.05, Be_flg bit4), left invisible (be_flag 0x4000).
static void em24_R0_Die(cEm24* em)
{
    Em24Work* w = EM24_WK(em);

    switch (em->r_no_1) {
    case 0:
        em->atari.offSca();
        if (w->Be_flg & 0x20) {
            em->r_no_3 = 1;
        } else {
            em->r_no_3 = 0;
        }
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x15), 0, 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM24, 7, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x13), 0, 3, 1, 0);
        }
        EmSetDie(em);
        em->r_no_1++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_1++;
        }
        break;
    case 2: {
        Vec pos;
        void* bin;
        void* tpl;
        int id;
        cEmWep* wep;

        w->Timer = 60;
        if (em->r_no_3) {
            EstSet(em, -1, 0, 0, EFF_EM24, 3, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM24, 1, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        pos = em->pos;
        switch (Rnd() & 0xF) {
        default:
            id = 8;
            bin = ARC(0x19);
            tpl = ARC(0x1A);
            break;
        case 0xC:
        case 0xD:
        case 0xE:
            id = 9;
            bin = ARC(0x19);
            tpl = ARC(0x1B);
            break;
        case 0xF:
            id = 0xA;
            bin = ARC(0x19);
            tpl = ARC(0x1C);
            break;
        }
        wep = SetWeapon(bin, tpl, &pos, &em->ang, 1);
        if (wep) {
            int no = SceAtCreateItemAt(&pos, id, 0, -1, -1, 0, -1);

            SceAtSetItemModel(no, wep);
            wep->setAtNo(no);
            wep->setYarare(100.0f, 10.0f, 0);
            wep->setEffDamage(0x1C, 0x1F);
            wep->setSeDamage(8, 0xC, em->id);
        }
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        em->r_no_1++;
    }
    case 3:
        em->pos.y -= 2.0f;
        if (w->Timer) {
            em->invisible_factor -= 0.05f;
            if (em->invisible_factor < 0.0f) {
                em->invisible_factor = 0.0f;
                em->be_flag &= ~2;
                em->be_flag |= 0x4000;
                em->r_no_1++;
                w->Be_flg |= 0x10;
                break;
            }
        }
        MotionMove(em, 0);
        break;
    case 4:
        break;
    }
}

// Bite hit test of the box jump: the capsule a -> b against the player / partner with
// em24_atk_tbl[no] (once per attack, Atk_ck); a player hit adds blood, quake and vibration. 1 = hit.
int em24AtkCk(cEm24* em, Vec* a, Vec* b, int no)
{
    Em24Work* w = EM24_WK(em);

    if (w->Atk_ck) {
        return 0;
    }
    {
        int hit = EmAtkHitCk(&em24_atk_tbl[no], a, b, 0);

        if (hit) {
            w->Atk_ck = 1;
            if (hit & 1) {
                EmPlBloodSet2(em, a, 1, 0x1C, 6);
                QuakeExec(0, 0, 5, 22.0f, 2);
                VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
                return 1;
            }
        }
    }
    return 0;
}

// Tilts the model to the floor slope measured by two floor probes ahead / behind (slopeRot smoothed),
// while alive.
void em24SlopeMove(cEm24* em)
{
    Em24Work* w = EM24_WK(em);
    Vec a;
    Vec b;
    Mtx m;
    f32 wh;
    f32 fa;
    f32 fb;
    f32 len;

    if (em->hp <= 0) {
        return;
    }
    a.x = 0.0f;
    a.y = 1000.0f;
    a.z = 200.0f;
    b.x = 0.0f;
    b.y = 1000.0f;
    b.z = -200.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    fa = SatMgr.getFloor(&a, 0, 600.0f, 100000.0f, 0);
    if (GetWaterHeight(&a, &wh)) {
        if (fa < wh) {
            fa = wh;
        }
    }
    fb = SatMgr.getFloor(&b, 0, 600.0f, 100000.0f, 0);
    if (GetWaterHeight(&b, &wh)) {
        if (fb < wh) {
            fb = wh;
        }
    }
    if (fa == -100000.0f) {
        fa = em->pos.y;
    }
    if (fb == -100000.0f) {
        fb = em->pos.y;
    }
    fa -= fb;
    if (fa > 400.0f) {
        fa = 400.0f;
    }
    if (fa < -400.0f) {
        fa = -400.0f;
    }
    len = VEC_DISTXZ(&a, &b);
    w->slopeRot.x = w->slopeRot.x * 0.95f + -atan2f(fa, len) * 0.05f;
    RotMatrix(m, &w->slopeRot);
    PSMTXConcat(em->mat, m, em->mat);
    TransMatrix(em->mat, &em->pos);
}
