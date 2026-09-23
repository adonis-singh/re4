// em2e module (D:/Bio4/Prog/em2e.cpp): a small crawling enemy that wanders (wait / walk / turn),
// or in wall mode (work flag bit1) walks along the surface normal found by the scenario check
// (em2eSetWallMatrix). Dies to a nearby player when pG->flags_5010 bit31 is set or to any damage.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "em2e.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em_mod.h"


typedef void (*Em2eFunc)(cEm2e*);

static void em2e_R0_Init(cEm2e* em);
static void em2e_R0_Move(cEm2e* em);
static void em2e_R1_Wait(cEm2e* em);
static void em2e_R1_Walk(cEm2e* em);
static void em2e_R1_Turn(cEm2e* em);
static void em2e_R1_W_Wait(cEm2e* em);
static void em2e_R1_W_Walk(cEm2e* em);
static void em2e_R1_W_Turn(cEm2e* em);
static void em2e_R0_Damage(cEm2e* em);
static void em2e_R0_Die(cEm2e* em);
static void em2e_R1_Die_Normal(cEm2e* em);



// Module entry (SN loader): registers Em2eInit as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    EmInitFunc = Em2eInit;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm2e class in the manager's work.
void Em2eInit(cEm* em)
{
    new (em) cEm2e();
}

// Per-frame damage check (cEm2e::move): a floor spider is squashed (hp 0, splat effect, Die_Normal)
// when the player comes within 400 units while the alert flag Status_flg[1] bit31 is set; any weapon
// hit kills it (the wall variant with its own effect).
void em2eDmCk(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    if (em->hp > 0 && (StaFlagChk(pG, STA_PL_SE_FOOT)) && !(w->flags & 2)) {
        if ((pPL->pos.x - em->pos.x) * (pPL->pos.x - em->pos.x) + (pPL->pos.y - em->pos.y) * (pPL->pos.y - em->pos.y)
            + (pPL->pos.z - em->pos.z) * (pPL->pos.z - em->pos.z) < 160000.0f) {
            em->hp = 0;
            EstSet(em, -1, 0, 0, EFF_EM2E, 0, 0, ESP_CORE_KIND_NONE, em, 0);
            SndCall(8, 4, &em->pos, em->id, 0, em);
            EmSetDie(em);
            EmRoutineSet(em, 3, 0, 0, 0);
        }
    }
    if (em->dmg.m_Flag) {
        em->dmg.m_Flag = 0;
        em->hp = 0;
        if (w->flags & 2) {
            Vec rot;

            rot.x = 0.0f;
            rot.y = atan2f(w->nrm.x, w->nrm.z);
            rot.z = 0.0f;
            EstSet(0, -1, &em->pos, &rot, EFF_EM2E, 1, 0, ESP_CORE_KIND_NONE, 0, 0);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM2E, 0, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        SndCall(8, 4, &em->pos, em->id, 0, em);
        EmSetDie(em);
        EmRoutineSet(em, 3, 0, 0, 0);
    }
}

Em2eFunc Em2e_R0_move_tbl[5] = {
    em2e_R0_Init,
    em2e_R0_Move,
    em2e_R0_Damage,
    em2e_R0_Die,
    (Em2eFunc) Em_R0_Scenario,
};

static Em2eFunc Em2e_R1_move_tbl[6] = {
    em2e_R1_Wait,
    em2e_R1_Walk,
    em2e_R1_Turn,
    em2e_R1_W_Wait,
    em2e_R1_W_Walk,
    em2e_R1_W_Turn,
};

static Em2eFunc Em2e_R3_move_tbl[1] = {
    em2e_R1_Die_Normal,
};

// Per-frame update: damage check, the R0 table (Init / Move / Damage / Die / Scenario), then the
// collision and, for a floor spider, the scenario check.
void cEm2e::move()
{
    Em2eWork* w = EM2E_WK(this);

    em2eDmCk(this);
    Em2e_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    if (hp > 0) {
        EmAtCheck(this);
        atari.move();
        if (!(w->flags & 2)) {
            SatMgr.check(this, 0);
        }
    }
}

// R0 == 0: creation. Builds the model of type 0 / 1 (ARC 4 + 5 / 6), lock-on off, no Ashley help,
// hp 1, effect data, a small light, collision and hit box; set 0 starts Wait on the floor, other
// sets snap the spider onto the wall behind it (scenario probe -> pos / nrm) and start W_Wait (3).
static void em2e_R0_Init(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);
    cAtariInfo* at;
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em2e 00() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(4), ARC(6)) == 0) {
            pLog->err(0, 0, "em2e 01() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    }
    em->setStatus(EM_STATUS_LOCKOFF);
    zero = 0;
    at = &em->atari;
    em->be_flag &= ~0x01000000;
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    EspDataLoad((u32) ARC(7), EFF_EM2E, 0);
    em->hp = 1;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 500.0f, 500.0f, 500.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    at->init(0.0f, 0.0f, 0.0f, 150.0f, 150.0f, 150.0f, 300.0f, 1, 0x2000, 10);
    AtariOff(at, 0xFDFF);
    em->be_flag &= ~0x10;
    YarareInit(em, 0.0f, 0.0f, 0.0f, 100.0f, 50.0f, 1, YAT_FLAG_ON);
    w->flags = zero;
    w->footAng = 0.0f;
    w->nrm.y = 1.0f;
    w->nrm.x = 0.0f;
    w->nrm.z = 0.0f;
    switch (em->set) {
    case 0:
    default:
        EmRoutineSet(em, 1, zero, zero, zero);
        break;
    case 1: {
        Mtx m;
        Vec a;
        Vec b;
        Vec hit;
        Vec nrm;

        PSMTXRotRad(m, 'y', em->ang.y);
        TransMatrix(m, &em->pos);
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 0.0f;
        b.x = 0.0f;
        b.y = 0.0f;
        b.z = 2000.0f;
        PSMTXMultVec(m, &a, &a);
        PSMTXMultVec(m, &b, &b);
        if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0)) {
            em->pos = hit;
            w->nrm = nrm;
        }
        AtariOff(at, 0xFEFF);
        EmRoutineSet(em, 1, 3, zero, zero);
        break;
    }
    }
    em2e_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em2e_R1_move_tbl: Wait, Walk, Turn, W_Wait, W_Walk, W_Turn).
static void em2e_R0_Move(cEm2e* em)
{
    Em2e_R1_move_tbl[em->r_no_1](em);
}

// R1 == 0 Wait: sits 60..150 frames, then Walk (1) or Turn (2) at random; rebuilds the matrix.
static void em2e_R1_Wait(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    switch (em->r_no_2) {
    case 0:
        w->timer = Rnd() % 90 + 60;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else if (Rnd() & 1) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// R1 == 1 Walk: crawls forward 10 units per frame for 30..90 frames with the leg swing
// (em2eFootMove), then Wait or Turn.
static void em2e_R1_Walk(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    switch (em->r_no_2) {
    case 0:
        w->timer = Rnd() % 60 + 30;
        em->r_no_2++;
    case 1: {
        Vec spd;

        spd.x = 0.0f;
        spd.y = 0.0f;
        spd.z = 10.0f;
        PSMTXMultVecSR(em->mat, &spd, &spd);
        PSVECAdd(&em->pos, &spd, &em->pos);
        em2eFootMove(em);
        if (w->timer) {
            w->timer--;
        } else if (Rnd() & 3) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// R1 == 2 Turn: turns PI/64 per frame (turnDir side) for 15..60 frames, then Wait or Walk.
static void em2e_R1_Turn(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    switch (em->r_no_2) {
    case 0:
        w->timer = Rnd() % 45 + 15;
        w->turnDir = Rnd() & 1;
        em->r_no_2++;
    case 1:
        if (w->turnDir) {
            em->ang.y += PI / 64.0f;
        } else {
            em->ang.y -= PI / 64.0f;
        }
        em2eFootMove(em);
        if (w->timer) {
            w->timer--;
        } else if (Rnd() & 3) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// R1 == 3 W_Wait: the wall spider (flag bit1) sits 60..150 frames on its surface (em2eSetWallMatrix),
// then W_Turn (5).
static void em2e_R1_W_Wait(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    w->flags |= 2;
    switch (em->r_no_2) {
    case 0:
        w->timer = Rnd() % 90 + 60;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    em2eSetWallMatrix(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// R1 == 4 W_Walk: crawls 15 frames along the wall following the surface normal, then W_Wait or W_Turn.
static void em2e_R1_W_Walk(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    w->flags |= 2;
    switch (em->r_no_2) {
    case 0:
        w->timer = 15;
        em->r_no_2++;
    case 1: {
        Vec spd;

        spd.x = 0.0f;
        spd.y = 0.0f;
        spd.z = 10.0f;
        PSMTXMultVecSR(em->mat, &spd, &spd);
        PSVECAdd(&em->pos, &spd, &em->pos);
        em2eFootMove(em);
        if (w->timer) {
            w->timer--;
        } else if (Rnd() & 3) {
            EmRoutineSet(em, 1, 3, 0, 0);
        } else {
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    }
    em2eSetWallMatrix(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// R1 == 5 W_Turn: turns 15 frames about the surface normal (direction from the list slot), then W_Walk (4).
static void em2e_R1_W_Turn(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    w->flags |= 2;
    switch (em->r_no_2) {
    case 0:
        w->timer = 15;
        w->turnDir = em->emset_no & 1;
        em->r_no_2++;
    case 1: {
        Mtx m;

        PSMTXRotRad(m, 'y', w->turnDir ? PI / 64.0f : -PI / 64.0f);
        PSMTXConcat(em->mat, m, em->mat);
        TransMatrix(em->mat, &em->pos);
        em2eFootMove(em);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
    }
    em2eSetWallMatrix(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// R0 == 2: the spider has no damage reaction; runs the die table.
static void em2e_R0_Damage(cEm2e* em)
{
    Em2e_R3_move_tbl[em->r_no_1](em);
}

// R0 == 3: death, runs Em2e_R3_move_tbl (Die_Normal).
static void em2e_R0_Die(cEm2e* em)
{
    Em2e_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Normal: the squashed spider: collision off and invisible, nothing more.
static void em2e_R1_Die_Normal(cEm2e* em)
{
    if (em->r_no_2 == 0) {
        AtariOff(&em->atari, 0xFCFF);
        em->be_flag &= ~2;
        em->r_no_2++;
    }
}

// Swings the leg parts 2 / 3 back and forth (footAng +-PI/16, PI/64 per frame, flag bit0 = direction).
void em2eFootMove(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);
    cParts* p2 = em->getPartsPtr(2);
    cParts* p3 = em->getPartsPtr(3);

    p2->ang.y = w->footAng;
    p3->ang.y = -w->footAng;
    if (w->flags & 1) {
        w->footAng += PI / 64.0f;
        if (w->footAng >= PI / 16.0f) {
            w->footAng = PI / 16.0f;
            w->flags &= ~1;
        }
    } else {
        w->footAng -= PI / 64.0f;
        if (w->footAng <= -PI / 16.0f) {
            w->footAng = -PI / 16.0f;
            w->flags |= 1;
        }
    }
}

// Wall mode: re-finds the surface under the spider (probe along nrm), snaps to it and rotates the
// model matrix so its up axis matches the surface normal.
void em2eSetWallMatrix(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);
    Vec nrm;
    Vec hit;
    Vec a;
    Vec b;
    Vec up;
    Vec axis;
    Mtx m;
    f32 ang;

    PSVECScale(&w->nrm, &a, 500.0f);
    PSVECScale(&w->nrm, &b, -2000.0f);
    PSVECAdd(&em->pos, &a, &a);
    PSVECAdd(&em->pos, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0)) {
        em->pos = hit;
        w->nrm = nrm;
    }
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    PSMTXMultVecSR(em->mat, &up, &up);
#line 694 "D:/Bio4/Prog/em2e.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->nrm, &axis);
    ang = acosf(PSVECDotProduct(&up, &w->nrm));
    if (ang > 0.001f) {
        PSMTXRotAxisRad(m, &axis, ang);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
}
