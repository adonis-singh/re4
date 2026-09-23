// game/emrock.cpp: rolling rock enemy (cEmRock): boulders that hang on a parent, fall, get
// thrown, roll after the player (with the escape event) or drop on him.
//
// Byte-identical (DOL sweep 23b). emRockDropCamMove sets `up` before `len` (sched1's 32-entry
// pending-memory flush otherwise lands on the up.x store).
//
// Camera tails: `Camera* cam = &emRockCam;` is declared BEFORE the `cp`/`ca` pointers. cse rewrites
// `&emRockCam` from the OLDEST related constant (`emRockCam+K`) whose class still holds a register
// (use_related_value walks the ring from the base symbol): with `ca = &emRockCam.param.at` declared
// first, `cam` came out `ca - 176`; the original has `cp - 164` (PushCamMove: the `pos = p` block
// copy's address pseudo) or a fresh `lis/addi` (EscapeCamMove2/DropDieCamMove: the PosToPos/PSVECAdd
// argument registers were clobbered by the calls), i.e. `cam` was computed before `ca` existed.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "emrock.h"
#include "emhit.h"
#include "at_mod.h"
#include "esp.h"
#include "snd.h"
#include "quake.h"
#include "pad.h"
#include "main.h"
#include "act_btn.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cockpit.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "route_ck.h"
#include "game.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "motion.h"
#include "est.h"
#include "em_sub.h"
#include "em2b.h"

// Head of a key-frame motion data block (motion.h MotionData).
struct RockMotData {
    u16 maxFrame;   // 0x00
};


typedef void (*EmRockFunc)(cEmRock*);

extern "C" void emRock_R0_Move(cEmRock* em);
extern "C" void emRock_R1_Lost(cEmRock* em);

EmRockFunc EmRock_R0_move_tbl[4] = {
    emRock_R0_Init,
    emRock_R0_Move,
    0,
    0,
};

static EmRockFunc EmRock_R1_move_tbl[9] = {
    emRock_R1_Set,
    emRock_R1_Lost,
    emRock_R1_Parent,
    emRock_R1_Fall,
    emRock_R1_Throw,
    emRock_R1_Throw2,
    emRock_R1_Roll,
    emRock_R1_Drop,
    emRock_R1_Drop2,
};

// Attack parameters of a falling / thrown rock without its own (setFall / setThrow); range = radius.
static EmAtkInfo emRockAtk = { 1500.0f, PL_DM_AUTO, 9999, 0, 10, 0 };

// Event camera of the escape / drop scenes (CamCtrl.x250 points at it while they run).
static Camera emRockCam = { 0 };

// Creates a rolling rock enemy (id 0x4A, at the back of the pool) from a model / TPL at pos / rot.
// type 0 the boulder El Gigante / room events throw, 1 the big (scale 4.2) rolling boulder of
// the chase rooms (starts rolling on its own, Roll), 3 the room 11E / 300 event rocks (no atari,
// radius 2000). 1000 hp, unlockable, its own Core_kind for the trail effects. Starts in Rno1 0
// Set. NULL on failure.
cEmRock* SetRock(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type)
{
    cEmRock* em;
    EmRockWork* w;

    em = (cEmRock*) EmMgr.createBack(0x4A);
    if (em == 0) {
        return 0;
    }
    w = EMROCK_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetRock() ModelInit failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    switch (em->type) {
    case 0:
        break;
    case 1:
        em->scale.x = 4.2f;
        em->scale.y = 4.2f;
        em->scale.z = 4.2f;
        break;
    }
    if (em->type != 3) {
        em->atari.init(0.0f, -(em->scale.y * 1200.0f) * 0.5f, 0.0f, em->scale.x * 1200.0f * 0.5f,
                   em->scale.x * 1200.0f * 0.5f, em->scale.x * 1200.0f * 0.5f, em->scale.y * 1200.0f * 0.5f, 0, 0x2000, 10);
    } else {
        em->atari.init(0.0f, 2000.0f, 0.0f, 2700.0f, 2700.0f, 2700.0f, 2000.0f, 0, 0x2000, 10);
    }
    em->hp = 1000;
    em->hp_max = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        if (type != 1 && type != 3) {
            em->LightInfo.init2(0, 1, &ofs, &size, 0x10);
        } else {
            em->LightInfo.init2(0, 1, &ofs, &size, 8);
        }
    }
    // COMPILER-DIFF: #13 (dying-store shape): the original's zero pseudo (REG_EQUIV 0, never
    // allocated, reloaded per label region as `li r30, 0`) does not die at `seAlways[2] = 0`, so the
    // store block comes out in source order; ours allocates the pseudo and would hoist that dying
    // store to the block top. The volatile use after the block keeps our pseudo live past it; the
    // second block's literal zeros are the fresh post-label `li r30, 0` of the original.
    int zero;
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->be_flag &= ~0x01000000;
    em->atari.setPriority(PRI_LV3);
    em->atari.clrFlag100();
    em->be_flag &= ~0x10;
    w->alwaysWait = 4;
    w->seid_throw = zero;
    w->Be_flg = zero;
    w->x24 = zero;
    w->pEm_oya = (cEm*) zero;
    w->pEm_old = zero;
    w->pAtk = (EmAtkInfo*) zero;
    w->xA1 = zero;
    w->se8C = zero;
    w->seFall[0] = 0xFF;
    w->seFall[1] = 0xFF;
    w->seFall[2] = zero;
    w->seFall[3] = zero;
    w->se8D[0] = 0xFF;
    w->se8D[1] = 0xFF;
    w->se8D[2] = zero;
    w->se97[0] = 0xFF;
    w->se97[1] = 0xFF;
    w->se97[2] = zero;
    w->se90[0] = 0xFF;
    w->se90[1] = 0xFF;
    w->se90[2] = zero;
    w->seAlways[0] = 0xFF;
    w->seAlways[1] = 0xFF;
    w->seAlways[2] = zero;
    w->effFall[0] = 0xFF;
    w->effFall[1] = 0xFF;
    w->eff9E[0] = 0xFF;
    w->eff9E[1] = 0xFF;
    w->eff9C[0] = 0xFF;
    w->eff9C[1] = 0xFF;
    asm volatile("" : : "r"(zero)); // COMPILER-DIFF: #13
    if (em->type != 3) {
        w->Radius = em->scale.x * 600.0f;
    } else {
        w->Radius = 2000.0f;
    }
    w->Roll_flag = 0;
    w->Gravity = 20.0f;
    w->rollWait = 0;
    em->Motion.pMot = (MotionData*) 0;
    w->Mot_tbl[2] = (void*) 0;
    w->Mot_tbl[3] = (void*) 0;
    w->Mot_tbl[4] = (void*) 0;
    w->Mot_tbl[5] = (void*) 0;
    w->Mot_tbl[6] = (void*) 0;
    w->Mot_tbl[7] = (void*) 0;
    w->Mot_tbl[8] = (void*) 0;
    w->Mot_tbl[9] = (void*) 0;
    w->Mot_tbl[10] = (void*) 0;
    w->Mot_tbl[11] = (void*) 0;
    w->Mot_drop = (void*) 0;
    w->Mot_wait = (void*) 0;
    w->Mot_pldie = (void*) 0;
    w->Mot_subdie = (void*) 0;
    w->pSat = (cSat*) 0;
    w->espKind = EspPullCoreKind();
    em->setStatus(EM_STATUS_ACTIVE);
    em->flag &= ~1;
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    emRock_R0_Move(em);
    return em;
}

// Event start hook: nothing to do for rocks.
void cEmRock::beginEvent(u32 flag)
{
}

// Rocks take no weapon damage: the registered hit is simply cleared.
void emRockDmCk(cEmRock* pEm)
{
    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    pEm->dmg.m_Flag = 0;
}

// Per-frame: hit clear, the Rno0 routine, then the model-vs-player atari, mirroring the parent's
// visibility / fade while hanging on it (Be_flg bit1 forces hidden), and the room 11E collision
// piece.
void cEmRock::move()
{
    EmRockWork* w = EMROCK_WK(this);

    emRockDmCk(this);
    EmRock_R0_move_tbl[r_no_0](this);
    if ((be_flag & 0x201) != 1) {
        return;
    }
    EmAtCheck(this);
    atari.move();
    if (w->pEm_oya) {
        invisible_factor = w->pEm_oya->invisible_factor;
        invisible_factor2 = w->pEm_oya->invisible_factor2;
        if (w->pEm_oya->be_flag & 2) {
            be_flag |= 2;
        } else {
            be_flag &= ~2;
        }
    }
    if (w->Be_flg & 2) {
        be_flag &= ~2;
    }
    emRockSatSet(this);
}

// Rno0 == 0: resets to the Set state.
void emRock_R0_Init(cEmRock* pEm)
{
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 0;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 Lost, 2 Parent, 3 Fall, 4 Throw, 5 Throw2, 6 Roll,
// 7 Drop, 8 Drop2).
void emRock_R0_Move(cEmRock* pEm)
{
    EmRock_R1_move_tbl[pEm->r_no_1](pEm);
}

// Rno1 == 0: resting rock; plays its motion or rebuilds the matrices; the type 1 boulder starts
// rolling (Rno1 6) when emRockRollStartCk fires (player crosses the trigger).
void emRock_R1_Set(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);

    if (pEm->Motion.pMot) {
        MotionMove(pEm, 0);
    } else {
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        ScaleMatrix(pEm->mat, &pEm->scale);
        pEm->partsMatCalc();
    }
    pEm->partsWorldCalc();
    switch (pEm->r_no_2) {
    case 0:
        pEm->r_no_2++;
        break;
    case 1:
        if (w->Roll_flag == 0 && pEm->type == 1) {
            if (emRockRollStartCk(pEm)) {
                w->Roll_flag = 1;
                pEm->flag |= 1;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 6;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            }
        }
        break;
    }
}

// Rno1 == 1: hides the rock, drops ACTIVE and its effects, destroys the work 30 frames later.
void emRock_R1_Lost(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        pEm->clearStatus(EM_STATUS_ACTIVE);
        EffectEspgenDelete(0, w->espKind, pEm);
        pEm->r_no_2++;
        w->Timer = 30;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            EmMgr.destroy(pEm);
        }
        break;
    }
}

// Rno1 == 2: held: follows parts `oya_parts` of pEm_oya (rotation re-normalised unless Be_flg
// bit0) and plays its own motion when it has one; lost when the holder vanishes.
void emRock_R1_Parent(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    cEm* parent = w->pEm_oya;
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;

    RotMatrix(pEm->mat, &pEm->ang);
    TransMatrix(pEm->mat, &pEm->pos);
    ScaleMatrix(pEm->mat, &pEm->scale);
    if (parent && parent->pList) {
        PSMTXConcat(parent->getPartsPtr(w->oya_parts)->mat, pEm->mat, m);
        if (!(w->Be_flg & 1)) {
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
#line 547 "D:/Bio4/Prog/emrock.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 549 "D:/Bio4/Prog/emrock.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 551 "D:/Bio4/Prog/emrock.cpp"
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
        }
        PSMTXCopy(m, pEm->mat);
    }
    if (pEm->Motion.pMot) {
        pEm->Motion.Mot_flag |= 0x40000000;
        MotionMove(pEm, 0);
    } else {
        pEm->partsMatCalc();
    }
    pEm->partsWorldCalc();
}

// Rno1 == 3: dropped straight down with `Gravity`, sliding along the scenery (EatMgr.adjust with
// Radius): a scenery contact ends it with the dust est 1/8 (hidden, Lost); hits the player through
// pAtk (emRockAtkCk); spins with the travelled distance; gives up after 60 frames.
void emRock_R1_Fall(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (pEm->r_no_2) {
    case 0:
        w->Timer2 = 60;
        pEm->r_no_2++;
    case 1:
        if (w->Timer2) {
            w->Timer2--;
            break;
        }
        pEm->pos.x = pEm->mat[0][3];
        pEm->pos.y = pEm->mat[1][3];
        pEm->pos.z = pEm->mat[2][3];
        Matrix2AxisAngle(pEm->mat, &pEm->ang);
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 1;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
        EstSet(0, -1, &pEm->pos, 0, EFF_ROOM, 8, 0, ESP_CORE_KIND_NONE, 0, 0);
        break;
    }
    w->spd.y -= w->Gravity;
    PSVECAdd(&pEm->pos, &w->spd, &pEm->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &pEm->pos_old, &pEm->pos, w->Radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        pEm->be_flag &= ~2;
        pEm->pos.x = pEm->mat[0][3];
        pEm->pos.y = pEm->mat[1][3];
        pEm->pos.z = pEm->mat[2][3];
        Matrix2AxisAngle(pEm->mat, &pEm->ang);
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 1;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
        EstSet(0, -1, &pEm->pos, 0, EFF_ROOM, 8, 0, ESP_CORE_KIND_NONE, 0, 0);
        return;
    }
    if (w->pAtk) {
        emRockAtkCk(pEm, w->pAtk, 0, w->Radius);
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&pEm->pos, &pEm->pos_old, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = VEC_DIST(&pEm->pos, &pEm->pos_old);
        if (len > 500.0f) {
            len = 500.0f;
        }
        ang = len * 0.002f * 0.62831855f;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        axis.x = 0.0f;
        axis.y = 0.0f;
        axis.z = 1.0f;
        PSMTXMultVecSR(m, &axis, &axis);
        if (axis.x == 0.0f) {
            axis.y = 0.0f;
        }
#line 654 "D:/Bio4/Prog/emrock.cpp"
        VECNormalize(&axis, &axis);
        len = acosf(PSVECDotProduct(&up, &axis));
        if (len > 0.01f && len < 3.1315927f) {
            PSVECCrossProduct(&up, &axis, &up);
            PSMTXRotAxisRad(m, &up, ang);
            PSMTXConcat(m, pEm->mat, pEm->mat);
        }
    }
    TransMatrix(pEm->mat, &pEm->pos);
    pEm->partsWorldCalc();
    emRockAtkScrCk(pEm);
}

// Rno1 == 4: the thrown boulder: flies with gravity, loops the whoosh SE, bounces off the scenery
// (speed reflected x 0.99; a hard landing plays seFall / effFall and shakes the camera), hits the
// player through pAtk, and stops (dust est, Lost) after 60 frames or when it comes to rest.
void emRock_R1_Throw(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (pEm->r_no_2) {
    case 0:
        w->Timer = 0;
        w->Timer2 = 60;
        pEm->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->alwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->seid_throw = SndCall(w->seAlways[0], w->seAlways[1], &pEm->pos, w->seAlways[2], 0, pEm);
            }
        }
        if (w->Timer2) {
            w->Timer2--;
            break;
        }
        pEm->pos.x = pEm->mat[0][3];
        pEm->pos.y = pEm->mat[1][3];
        pEm->pos.z = pEm->mat[2][3];
        Matrix2AxisAngle(pEm->mat, &pEm->ang);
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 1;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
        EstSet(0, -1, &pEm->pos, 0, EFF_ROOM, 8, 0, ESP_CORE_KIND_NONE, 0, 0);
        break;
    }
    w->spd.y -= w->Gravity;
    PSVECAdd(&pEm->pos, &w->spd, &pEm->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &pEm->pos_old, &pEm->pos, w->Radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        f32 spd;

        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.99f);
        if (nrm.y < 0.5f) {
            pEm->be_flag &= ~2;
            pEm->pos.x = pEm->mat[0][3];
            pEm->pos.y = pEm->mat[1][3];
            pEm->pos.z = pEm->mat[2][3];
            Matrix2AxisAngle(pEm->mat, &pEm->ang);
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 1;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            EstSet(0, -1, &pEm->pos, 0, EFF_ROOM, 8, 0, ESP_CORE_KIND_NONE, 0, 0);
            return;
        }
        if (w->spd.y > 50.0f) {
            if (w->seFall[0] != 0xFF && w->seFall[1] != 0xFF) {
                SndCall(w->seFall[0], w->seFall[1], &pEm->pos, w->seFall[2], 0, pEm);
            }
            if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                Vec fp;

                fp = pEm->pos;
                fp.y = EatMgr.getFloor(&fp, 0, 600.0f, 100000.0f, 0);
                EstSet(0, -1, &fp, 0, w->effFall[0], w->effFall[1], 0, ESP_CORE_KIND_NONE, 0, 0);
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
        }
    }
    if (w->pAtk) {
        emRockAtkCk(pEm, w->pAtk, 1, w->Radius);
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&pEm->pos, &pEm->pos_old, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = VEC_DIST(&pEm->pos, &pEm->pos_old);
        if (len > 500.0f) {
            len = 500.0f;
        }
        ang = len * 0.002f * 0.62831855f;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        axis.x = 0.0f;
        axis.y = 0.0f;
        axis.z = 1.0f;
        PSMTXMultVecSR(m, &axis, &axis);
        if (axis.x == 0.0f) {
            axis.y = 0.0f;
        }
#line 800 "D:/Bio4/Prog/emrock.cpp"
        VECNormalize(&axis, &axis);
        len = acosf(PSVECDotProduct(&up, &axis));
        if (len > 0.01f && len < 3.1315927f) {
            PSVECCrossProduct(&up, &axis, &up);
            PSMTXRotAxisRad(m, &up, ang);
            PSMTXConcat(m, pEm->mat, pEm->mat);
        }
    }
    TransMatrix(pEm->mat, &pEm->pos);
    pEm->partsWorldCalc();
    emRockAtkScrCk(pEm);
}

// Rno1 == 5: the event boulder thrown at the player (room 202 / 214): same flight, but the first
// scenery contact after 180 frames ends it with a crash SE and the big break est (1/1 when the
// player still has more than 500 life, else 1/2).
void emRock_R1_Throw2(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    cAtariInfo* at;
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (pEm->r_no_2) {
    case 0:
        w->Timer = 0;
        w->Timer2 = 180;
        at = &pEm->atari;
        at->m_flag &= ~0x300;
        SndCall(6, 0x49, &pEm->pos, 0, 0, pEm);
        pEm->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->alwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->seid_throw = SndCall(w->seAlways[0], w->seAlways[1], &pEm->pos, w->seAlways[2], 0, pEm);
            }
        }
        if (w->Timer2) {
            w->Timer2--;
            break;
        }
        pEm->pos.x = pEm->mat[0][3];
        pEm->pos.y = pEm->mat[1][3];
        pEm->pos.z = pEm->mat[2][3];
        Matrix2AxisAngle(pEm->mat, &pEm->ang);
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 1;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
        break;
    }
    w->spd.y -= w->Gravity;
    PSVECAdd(&pEm->pos, &w->spd, &pEm->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &pEm->pos_old, &pEm->pos, w->Radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        Vec p;

        RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        if ((s16) pG->pl_life > 500) {
            w->pAtk->flag |= 4;
        } else {
            w->pAtk->flag &= ~4;
        }
        p = pEm->pos;
        p.y += 1000.0f;
        PlWepHitCheck2(0, &p, &p, 0x12, 3, 5000.0f);
        EffectEspgenDelete(0, w->espKind, pEm);
        if (nrm.y > 0.7f) {
            EstSet(0, -1, &pEm->pos, 0, EFF_ROOM, 1, 0, ESP_CORE_KIND_NONE, 0, 0);
        } else {
            EstSet(0, -1, &pEm->pos, 0, EFF_ROOM, 2, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        SndCall(6, 0x4A, &pEm->pos, 0, 0, pEm);
        pEm->be_flag &= ~2;
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 1;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
        return;
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&pEm->pos, &pEm->pos_old, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = VEC_DIST(&pEm->pos, &pEm->pos_old);
        if (len > 500.0f) {
            len = 500.0f;
        }
        ang = len * 0.002f * 0.62831855f;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        axis.x = 0.0f;
        axis.y = 0.0f;
        axis.z = 1.0f;
        PSMTXMultVecSR(m, &axis, &axis);
        if (axis.x == 0.0f) {
            axis.y = 0.0f;
        }
#line 936 "D:/Bio4/Prog/emrock.cpp"
        VECNormalize(&axis, &axis);
        len = acosf(PSVECDotProduct(&up, &axis));
        if (len > 0.01f && len < 3.1315927f) {
            PSVECCrossProduct(&up, &axis, &up);
            PSMTXRotAxisRad(m, &up, ang);
            PSMTXConcat(m, pEm->mat, pEm->mat);
        }
    }
    TransMatrix(pEm->mat, &pEm->pos);
    pEm->partsWorldCalc();
    emRockAtkScrCk(pEm);
}

// Rno1 == 6: the chase boulder: waits 75 frames (starting the player's escape routine
// plemRockEscape and the rumble SE), then follows the EMI route (type 6 points) with gravity 10,
// bouncing on the floor with dust, accelerating along the route; reaching the end (or losing the
// route) breaks it (SE, est 1/0x1F, Lost).
void emRock_R1_Roll(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    Vec d;
    f32 len;
    f32 ang;

    switch (pEm->r_no_2) {
    case 0:
        KeyStop(0xEFCF0000ULL);
        if (emRockSetRollRoute(pEm) == 0) {
            pEm->pos.x = pEm->mat[0][3];
            pEm->pos.y = pEm->mat[1][3];
            pEm->pos.z = pEm->mat[2][3];
            Matrix2AxisAngle(pEm->mat, &pEm->ang);
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            return;
        }
        switch (pG->room_no) {
        case 4:
            SndStrReq(1, 0x3E, 0x80000003, 0, 0, 0.0f);
            break;
        case 6:
            SndStrReq(1, 0x3C, 0x80000003, 0, 0, 0.0f);
            break;
        case 0xA:
            SndStrReq(1, 0x3D, 0x80000003, 0, 0, 0.0f);
            break;
        }
        emRockPushCk(pEm, 0);
        pEm->atari.m_flag &= ~0x200;
        pPL->ang.y = pEm->ang.y;
        SetPlDamage(pEm, plemRockEscape);
        w->Roll_wait = 75;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        if (pG->stage_no == 1 && pG->room_no == 4) {
            w->First_bound = 1;
            w->rollWait = 0;
        } else {
            w->First_bound = 0;
            w->rollWait = 25;
        }
        pEm->r_no_2++;
    case 1:
        if (w->Roll_wait) {
            w->Roll_wait--;
            if (w->Roll_wait == 0) {
                w->Seid = SndCall(6, 5, &pEm->pos, 0, 0, pEm);
            }
            return;
        }
        if (emRockSetRollSpd(pEm)) {
            SndStop(w->Seid, 0);
            SndCall(6, 6, &pEm->pos, 0, 0, pEm);
            EstSet(0, -1, &pEm->pos, 0, EFF_ROOM, 0x1F, 0, ESP_CORE_KIND_NONE, 0, 0);
            pEm->be_flag &= ~2;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 1;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            return;
        }
    default:
        w->spd.y -= 10.0f;
        PSVECAdd(&pEm->pos, &w->spd, &pEm->pos);
        if (w->rollWait) {
            w->rollWait--;
        } else {
            f32 floor;

            floor = EatMgr.getFloor(&pEm->pos, 0, 600.0f, 100000.0f, 0) + w->Radius;
            if (pEm->pos.y < floor) {
                pEm->pos.y = floor;
                w->spd.y *= -0.5f;
                if (w->spd.y > 50.0f) {
                    Vec fp;

                    fp = pEm->pos;
                    fp.y -= w->Radius;
                    EstSet(0, -1, &fp, 0, EFF_OBM1F, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
                    SndCall(6, 7, &pEm->pos, 0, 0, pEm);
                    if (w->First_bound == 0) {
                        w->First_bound = 1;
                        w->spd.x = 0.0f;
                        w->spd.z = 0.0f;
                    }
                }
            }
        }
        emRockRollHitCk(pEm);
        emRockRunDownCk(pEm);
        {
            Mtx m;
            Vec up;
            Vec axis;

            PSVECSubtract(&pEm->pos, &pEm->pos_old, &d);
            PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
            len = VEC_DIST(&pEm->pos, &pEm->pos_old);
            if (len > 500.0f) {
                len = 500.0f;
            }
            ang = len * 0.002f * 0.31415927f;
            up.x = 0.0f;
            up.y = 1.0f;
            up.z = 0.0f;
            axis.x = 0.0f;
            axis.y = 0.0f;
            axis.z = 1.0f;
            PSMTXMultVecSR(m, &axis, &axis);
            if (axis.x == 0.0f) {
                axis.y = 0.0f;
            }
#line 1093 "D:/Bio4/Prog/emrock.cpp"
            VECNormalize(&axis, &axis);
            len = acosf(PSVECDotProduct(&up, &axis));
            if (len > 0.01f && len < 3.1315927f) {
                PSVECCrossProduct(&up, &axis, &up);
                PSMTXRotAxisRad(m, &up, ang);
                PSMTXConcat(m, pEm->mat, pEm->mat);
            }
        }
        TransMatrix(pEm->mat, &pEm->pos);
        pEm->partsWorldCalc();
        emRockAtkScrCk(pEm);
        break;
    }
}

// Rno1 == 7: the ceiling rock of setDropMot: plays the loosening motions (mot0 / mot1) with dust
// and creak SEs, then drops on the player: plemDropFind makes him notice it, and it either kills
// him (plemDropDie) or the escape succeeds; ends in Lost.
void emRock_R1_Drop(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        pEm->atari.m_flag &= ~0x200;
        MotionSetCore(pEm, &pEm->Motion, w->Mot_wait, 0, 0, 1, 0);
        pEm->r_no_2++;
    case 1:
        MotionMove(pEm, 0);
        if (!(pEm->flag & 1)) {
            break;
        }
        pEm->r_no_2++;
    case 2:
        MotionSetCore(pEm, &pEm->Motion, w->Mot_drop, 0, 0, 1, 0);
        EstSet(pEm, -1, 0, 0, EFF_ROOM, 4, 0, w->espKind, pEm, 0);
        SndCall(6, 8, &pEm->pos, 0, 0, pEm);
        w->Timer = 37;
        pEm->r_no_2++;
    case 3:
        if (w->Timer) {
            w->Timer--;
            emRockDropHitCk(pEm);
            emRockDropHitCkSub(pEm);
            if (emRockDropHitCkEm2b(pEm)) {
                pEm->be_flag &= ~2;
                pEm->atari.m_flag &= ~0x200;
                pEm->hp = 0;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 1;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
                EffectEspgenDelete(0, w->espKind, pEm);
                EstSet(0, -1, &pEm->getPartsPtr(0)->world, 0, EFF_ROOM, 1, 0, ESP_CORE_KIND_NONE, 0, 0);
                SndCall(6, 7, &pEm->pos, 0, 0, pEm);
                break;
            }
            if (w->Timer == 0) {
                SndCall(6, 7, &pEm->pos, 0, 0, pEm);
                pEm->atari.m_flag |= 0x200;
            }
        }
        if (MotionMove(pEm, 0)) {
            pEm->r_no_2++;
        }
        break;
    case 4:
        break;
    }
    pEm->partsWorldCalc();
}

// Rno1 == 8: the setDropMot2 variant with the action-button escape: after the loosening motion
// a 31 frame window offers button 0x25 (randomly variant 3 or 4); no press kills the player
// (plemDropDie), a press plays the escape motion (plemDropEscape).
void emRock_R1_Drop2(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        pEm->r_no_2++;
        pEm->atari.m_flag &= ~0x200;
    case 1:
        MotionSetCore(pEm, &pEm->Motion, w->Mot_drop, 0, 0, 1, 0);
        MotionMove(pEm, 0);
        if (!(pEm->flag & 1)) {
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        KeyStop(0xEFCF0000ULL);
        pEm->r_no_2++;
    case 2:
        w->Timer = 25;
        emRockPushCk(pEm, 50);
        pPL->ang.y = -0.1f;
        pPL->pos.x = -4924.0f;
        pPL->pos.y = -11950.0f;
        pPL->pos.z = -14770.0f;
        pPL->setPos(&pPL->pos);
        pPL->dmg.m_Timer = 2;
        SetPlDamage(pEm, plemDropFind);
        pEm->r_no_2++;
    case 3:
        MotionSetCore(pEm, &pEm->Motion, w->Mot_drop, 0, 0, 1, 0);
        MotionMove(pEm, 0);
        if (w->Timer) {
            w->Timer--;
            break;
        }
        pEm->r_no_2++;
        break;
    case 4:
        MotionSetCore(pEm, &pEm->Motion, w->Mot_drop, 0, 0, 1, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 5, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(6, 4, &pEm->pos, 0, 0, pEm);
        w->Timer = 31;
        w->TmpU32 = Rnd() & 1;
        w->Act_ck = 0;
        pEm->r_no_2++;
    case 5:
        if (MotionMove(pEm, 0)) {
            SndCall(6, 5, &pEm->pos, 0, 0, pEm);
            pEm->be_flag &= ~2;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 1;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            break;
        }
        if (w->Timer) {
            w->Timer--;
            if (w->Timer == 0) {
                if (w->Act_ck != 0) {
                    break;
                }
                if ((s16) pG->pl_life > 0) {
                    w->Act_ck = 1;
                    SetPlDamage(pEm, plemDropDie);
                    pPL->r_no_3 = 1;
                    break;
                }
            }
            if (w->Act_ck == 0) {
                switch (w->TmpU32) {
                case 0:
                default:
                    ActBtn.set(ACT_GUARD, 5, (void*) plemDropEscAction, pEm, ACTCTR_ENFORCE_EXEC | ACTCTR_EXACT_KEY, DISP_L_R, ACT_FUNC_NORMAL, 0);
                    break;
                case 1:
                    ActBtn.set(ACT_GUARD, 5, (void*) plemDropEscAction, pEm, ACTCTR_ENFORCE_EXEC | ACTCTR_EXACT_KEY, DISP_A_B, ACT_FUNC_NORMAL, 0);
                    break;
                }
            }
        }
        break;
    }
    pEm->partsWorldCalc();
}

// Action button callback of Drop2: marks the escape and starts the player's escape damage routine.
void plemDropEscAction(cEmRock* ptr)
{
    EMROCK_WK(ptr)->Act_ck = 1;
    pPL->dmg.m_Timer = 2;
    SetPlDamage(ptr, plemDropEscape);
}

// Player damage routine of the drop: notices the rock, then the escape / death routine takes over.
void plemDropFind(cPlayer* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm->pEmCatch);

    pEm->subArc = pEm->pEmCatch->subArc;
    pEm->dmg.m_Timer = 2;
    switch (pEm->r_no_2) {
    case 0:
        pEm->m_Work0 = 25;
        pEm->r_no_2++;
    case 1:
        if (pEm->m_Work0) {
            pEm->m_Work0--;
            MotionSetCore(pEm, &pEm->Motion, w->Mot_plfind, 0, 3, 1, 0);
            emRockPushCamMove((cEmRock*)pEm->pEmCatch);
        } else {
            emRockDropCamMove((cEmRock*)pEm->pEmCatch);
        }
        if (MotionMove(pEm, 0)) {
            SpfFlagOff(pG, SPF_KEY);
            EndPlDamage();
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Player damage routine: the player dives out of the way of the dropping rock.
void plemDropEscape(cPlayer* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm->pEmCatch);

    pEm->subArc = pEm->pEmCatch->subArc;
    switch (pEm->r_no_2) {
    case 0:
        MotionSetCore(pEm, &pEm->Motion, w->Mot_plesc, 0, 3, 1, 0);
        EstSet(pEm, -1, 0, 0, EFF_PL00, 0x14, 0, ESP_CORE_KIND_NONE, pEm, 0);
        SndCall(1, 0x43, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        SndCall(1, 0x44, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        pPL->dmg.m_Timer = 0x1E;
        pEm->r_no_2++;
    case 1:
        if (pEm->Motion.Seq_frame > 20.7f && pEm->Motion.Seq_frame < 21.3f) {
            SndCall(5, 2, &pEm->pos, 0, 0, pEm);
        }
        if (pEm->Motion.Seq_frame > 33.7f && pEm->Motion.Seq_frame < 34.3f) {
            SndCall(5, 3, &pEm->pos, 0, 0, pEm);
        }
        if (MotionMove(pEm, 0)) {
            SpfFlagOff(pG, SPF_KEY);
            EndPlDamage();
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// The rolling rock runs the player over: 1 when it hit him this frame.
int emRockRollHitCk(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    int dead;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    if ((pEm->pos.x - pPL->pos.x) * (pEm->pos.x - pPL->pos.x) + (pEm->pos.z - pPL->pos.z) * (pEm->pos.z - pPL->pos.z) >
        w->Radius * w->Radius) {
        return 0;
    }
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
    QuakeExec(0, 0, 5, 22.0f, 2);
    pG->pl_life = 0;
    PlSetDamage(PL_DM_AUTO, 0, 0);
    return 1;
}

// Hangs the rock on parts `partsNo_` of `parent` (Rno1 2); flag skips the matrix normalisation.
// Clears the holder's atari flag 0x200.
void cEmRock::setParent(cEm* parent, int partsNo_, int flag)
{
    EmRockWork* w = EMROCK_WK(this);

    w->oya_parts = partsNo_;
    w->pEm_oya = parent;
    if (flag) {
        w->Be_flg |= 1;
    } else {
        w->Be_flg &= ~1;
    }
    r_no_0 = 1;
    r_no_1 = 2;
    r_no_2 = 0;
    r_no_3 = 0;
    parent->atari.m_flag &= ~0x200;
}

// Drops the rock off its parent: it falls straight down (emRock_R1_Fall) with `atk` as its attack.
void cEmRock::setFall(EmAtkInfo* atk)
{
    EmRockWork* w = EMROCK_WK(this);
    Mtx m;

    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &ang);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    pos_old = pos;
    if (w->pEm_oya) {
        w->pEm_old = (u32) w->pEm_oya;
    }
    w->pEm_oya = 0;
    hp = 1;
    setYarareCube(400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emRockAtk;
        emRockAtk.range = w->Radius;
    }
    r_no_0 = 1;
    r_no_1 = 3;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Throws the rock with speed `spd` (a random forward throw in the parent's frame when NULL).
void cEmRock::setThrow(Vec* spd, EmAtkInfo* atk)
{
    EmRockWork* w = EMROCK_WK(this);
    Vec v;
    Mtx m;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pEm_oya) {
            PSMTXMultVecSR(w->pEm_oya->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    ang.x = 0.0f;
    ang.y = atan2f(v.x, v.z);
    ang.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &ang);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    pos_old = pos;
    if (w->pEm_oya) {
        w->pEm_old = (u32) w->pEm_oya;
    }
    w->pEm_oya = 0;
    hp = 1;
    setYarareCube(400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emRockAtk;
        emRockAtk.range = w->Radius;
    }
    r_no_0 = 1;
    r_no_1 = 4;
    r_no_2 = 0;
    r_no_3 = 0;
}

// setThrow variant that breaks on the first scenario hit (emRock_R1_Throw2).
void cEmRock::setThrow2(Vec* spd, EmAtkInfo* atk)
{
    EmRockWork* w = EMROCK_WK(this);
    Vec v;
    Mtx m;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pEm_oya) {
            PSMTXMultVecSR(w->pEm_oya->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    ang.x = 0.0f;
    ang.y = atan2f(v.x, v.z);
    ang.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &ang);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    pos_old = pos;
    if (w->pEm_oya) {
        w->pEm_old = (u32) w->pEm_oya;
    }
    w->pEm_oya = 0;
    hp = 1;
    setYarareCube(400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emRockAtk;
        emRockAtk.range = w->Radius;
    }
    r_no_0 = 1;
    r_no_1 = 5;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Dead-stripped in the original (STRIP_UNUSED): only its constant pool (one 0.0f) survives between
// setThrow2's pool and setYarareCube's.
static void emRockSpdClear(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
}

// SE (block / number / volume) played when the thrown rock lands hard (0xFF = none).
void cEmRock::setSeFall(u8 se_id, u8 se_no, u8 em_id)
{
    EmRockWork* w = EMROCK_WK(this);

    w->seFall[0] = se_id;
    w->seFall[1] = se_no;
    w->seFall[2] = em_id;
    w->seFall[3] = 0;
}

// Est spawned at the floor when the thrown rock lands hard (0xFF = none).
void cEmRock::setEffFall(u8 eff_id, u8 est_id)
{
    EmRockWork* w = EMROCK_WK(this);

    w->effFall[0] = eff_id;
    w->effFall[1] = est_id;
}

// Attaches a continuous est (trail / glow) to the rock under its Core_kind.
void cEmRock::setEffAlways(u8 eff_id, u8 est_id)
{
    EstSet(this, -1, 0, 0, eff_id, est_id, 0, EMROCK_WK(this)->espKind, this, 0);
}

// Gives the rock a hit box (offset `size` or 400 below the origin) of x / y / z so it can be
// shot (hp 1, e.g. the r300 rock that must be broken).
void cEmRock::setYarareCube(f32 w, f32 h, f32 d, Vec* pOfs)
{
    if (pOfs) {
        YarareInitCube(this, pOfs->x, pOfs->y, pOfs->z, w, h, d, 0, YAT_FLAG_ON);
    } else {
        YarareInitCube(this, 0.0f, -400.0f, 0.0f, w, h, d, 0, YAT_FLAG_ON);
    }
    hp = 1;
}

// on == 0 keeps the rock hidden (Be_flg bit1), on != 0 shows it.
void cEmRock::setTransMode(int mode)
{
    EmRockWork* w = EMROCK_WK(this);

    if (mode) {
        w->Be_flg &= ~2;
    } else {
        w->Be_flg |= 2;
    }
}

// Scenario event triggers (EMI type 3) the flying rock passes over: sets the pG->flags_174 event
// bits selected by the entry's sub type (room 119 fires a second bit while the trigger is fresh).
void emRockAtkScrCk(cEmRock* pEm)
{
    int i;

    if (pG->pEmi == 0) {
        return;
    }
    for (i = 0; i < pG->pEmi->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (((u8*) pG->pEmi)[o] != 3) {
            continue;
        }
        if (e->state == 3) {
            continue;
        }
        if ((e->pos.x - pEm->pos.x) * (e->pos.x - pEm->pos.x) + (e->pos.z - pEm->pos.z) * (e->pos.z - pEm->pos.z) >
            9000000.0f) {
            continue;
        }
        if (pG->stage_no == 1 && pG->room_no == 0x19) {
            if (e->state == 0) {
                switch (e->sub) {
                case 0:
                    RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_A);
                    RmfFlagOn(pG, RMF_R119_DESTROY_YANE_A);
                    break;
                case 1:
                    RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_B);
                    RmfFlagOn(pG, RMF_R119_DESTROY_YANE_B);
                    break;
                case 2:
                    RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_C);
                    RmfFlagOn(pG, RMF_R119_DESTROY_YANE_C);
                    break;
                }
            } else {
                switch (e->sub) {
                case 0:
                    RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_A);
                    break;
                case 1:
                    RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_B);
                    break;
                case 2:
                    RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_C);
                    break;
                }
            }
        }
        e->state = 3;
    }
}

// First EMI route point (type 6): 1 when found (routeIdx / pRoute set).
int emRockSetRollRoute(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    u8* emi;
    int i;
    int idx;

    emi = (u8*) pG->pEmi;
    if (emi == 0) {
        return 0;
    }
    idx = -1;
    for (i = 0; i < pG->pEmi->n; i++) {
        u32 o = i * 0x40 + 8;

        if (((u8*) pG->pEmi)[o] == 6) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        return 0;
    }
    w->Rock_route = idx;
    {
        u32 o = idx * 0x40 + 8;

        w->pRoute = (EmiEntry*) ((u8*) pG->pEmi + o);
    }
    return 1;
}

// Steers the rolling speed towards the current route point, advancing to the next one within
// 500 units; 1 when the route ends (the rock stops).
int emRockSetRollSpd(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    u8* emi;
    EmiEntry* e;
    int idx;
    int i;
    f32 spd;
    f32 add;
    Vec dir;

    emi = (u8*) pG->pEmi;
    if (emi == 0) {
        return 1;
    }
    e = w->pRoute;
    spd = (e->pos.x - pEm->pos.x) * (e->pos.x - pEm->pos.x) + (e->pos.z - pEm->pos.z) * (e->pos.z - pEm->pos.z);
    if (spd < 250000.0f) {
        idx = -1;
        for (i = w->Rock_route + 1; i < pG->pEmi->n; i++) {
            u32 o = i * 0x40 + 8;

            if (((u8*) pG->pEmi)[o] == 6) {
                idx = i;
                break;
            }
        }
        if (idx == -1) {
            return 1;
        }
        w->Rock_route = idx;
        {
            u32 o = idx * 0x40 + 8;

            e = (EmiEntry*) ((u8*) pG->pEmi + o);
        }
        w->pRoute = e;
    }
    PSVECSubtract(&e->pos, &pEm->pos, &dir);
    dir.y = 0.0f;
#line 2170 "D:/Bio4/Prog/emrock.cpp"
    VECNormalize(&dir, &dir);
    spd = SQRTF(w->spd.x * w->spd.x + w->spd.z * w->spd.z);
    if (w->First_bound) {
        add = 1.3f;
        if (pG->Game_level <= 2) {
            add = 1.27f;
        }
    } else {
        add = 3.0f;
    }
    spd += add;
    if (spd < 50.0f) {
        spd = 50.0f;
    }
    if (spd > 500.0f) {
        spd = 500.0f;
    }
    PSVECScale(&dir, &dir, spd);
    w->spd.x = dir.x;
    w->spd.z = dir.z;
    return 0;
}

// The player stepped into a roll start trigger (EMI type 8, 3000 units).
int emRockRollStartCk(cEmRock* pEm)
{
    EmiData* emi;
    int dead;
    int i;
    GlobalWork* g;

    emi = pG->pEmi;
    g = *(GlobalWork**) &pG;  // a second pG pseudo (`mr r11,r9`) that the pl_life test reads
    if (emi == 0) {
        return 0;
    }
    if ((s16) g->pl_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type == 8) {
            if ((e->pos.x - pPL->pos.x) * (e->pos.x - pPL->pos.x) + (e->pos.y - pPL->pos.y) * (e->pos.y - pPL->pos.y) +
                    (e->pos.z - pPL->pos.z) * (e->pos.z - pPL->pos.z) >
                9000000.0f) {
                return 0;
            }
            return 1;
        }
    }
    return 0;
}

// Player damage routine of the rolling rock: the player turns, runs along the EMI route with the
// button-mash speed motions, and jumps to the side (or gets caught) at the goal.
void plemRockEscape(cPlayer* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm->pEmCatch);
    void* mot;
    void* mot2;
    Vec v;
    int lim;
    int n;
    int flag;

    pEm->subArc = pEm->pEmCatch->subArc;
    mot2 = w->Mot_tbl[3];
    mot = w->Mot_tbl[2];
    switch (pEm->r_no_2) {
    case 0:
        pEm->m_Work0 = 85;
        Cckpt.lifeMeterDisp(0);
        pEm->Wep->setTrans(0, 0);
        switch (pG->room_no) {
        case 4:
        default:
            pPL->pos.x = 57947.0f;
            pPL->pos.y = 3273.0f;
            pPL->pos.z = -27900.0f;
            pEm->ang.y = 1.67f;
            break;
        case 6:
            pPL->pos.x = 28428.0f;
            pPL->pos.y = -5465.0f;
            pPL->pos.z = 2765.0f;
            pEm->ang.y = -1.99f;
            break;
        case 0xA:
            pPL->pos.x = -38340.0f;
            pPL->pos.y = 5111.0f;
            pPL->pos.z = 68313.0f;
            pEm->ang.y = -1.86f;
            break;
        }
        pEm->r_no_2++;
    case 1:
        pEm->dmg.m_Timer = 0x1E;
        if (pEm->m_Work0) {
            pEm->m_Work0--;
            emRockPushCamMove((cEmRock*)pEm->pEmCatch);
            MotionSetCore(pEm, &pEm->Motion, w->Mot_tbl[0], w->Mot_tbl[1], 0, 1, 0);
            pEm->ang.y += Muku(&pEm->pos, &pEm->pEmCatch->pos, pEm->ang.y, 3.1415927f);
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
            MotionMove(pEm, 0);
        } else {
            emRockPushCamMove2((cEmRock*)pEm->pEmCatch);
            ActBtn.set(ACT_SPRINT, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
            if (MotionMove(pEm, 0)) {
                pEm->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(pEm, &pEm->Motion, mot, mot2, 10, 5, 0);
        pEm->m_Work0 = 0;
        pEm->m_Work1 = 0;
        pEm->m_Work2 = 0;
        pEm->m_Work3 = plemRockSetEscapeRoute();
        pEm->m_Work4 = 0;
        pEm->m_Work5 = 0;
        pEm->m_Work6 = 0;
        pEm->m_Work7 = Rnd() & 1;
        pEm->m_Fwork0 = 0.1f;
        w->Act_ck = 0;
        pEm->r_no_2++;
    case 3:
        plemRockEscapeCamMove(pEm, pEm->m_Fwork0);
        pEm->m_Fwork0 += 0.05f;
        if (pEm->m_Fwork0 > 1.0f) {
            pEm->m_Fwork0 = 1.0f;
        }
        lim = 8;
        if (pG->Game_level <= 2) {
            lim = 12;
        }
        if (pG->Game_level > 7) {
            lim = 5;
        }
        pEm->m_Work4++;
        if (pEm->m_Work4 > lim) {
            pEm->m_Work4 = lim;
            pEm->m_Work0 -= 5;
            if ((int) pEm->m_Work0 < 0) {
                pEm->m_Work0 = 0;
            }
        }
        n = (int) pEm->m_Work0 / 20;
        if (n > 7) {
            n = 7;
        }
        if (n != pEm->m_Work1) {
            f32 ratio;
            f32 f;
            u32 cnt;
            u32 fr;

            pEm->m_Work1 = n;
            switch (n) {
            case 0:
            default:
                mot2 = w->Mot_tbl[3];
                break;
            case 1:
                mot2 = w->Mot_tbl[4];
                break;
            case 2:
                mot2 = w->Mot_tbl[5];
                break;
            case 3:
                mot2 = w->Mot_tbl[6];
                break;
            case 4:
                mot2 = w->Mot_tbl[7];
                break;
            case 5:
                mot2 = w->Mot_tbl[8];
                break;
            case 6:
                mot2 = w->Mot_tbl[9];
                break;
            case 7:
                mot2 = w->Mot_tbl[10];
                break;
            }
            ratio = pEm->Motion.Seq_frame / (f32) pEm->Motion.Seq_frame_num;
            cnt = ((RockMotData*) mot2)->maxFrame;
            f = (f32) cnt * ratio;
            fr = (u32) f + 1;
            if (fr >= cnt) {
                fr = 0;
            }
            MotionSetCore(pEm, &pEm->Motion, mot, mot2, pEm->Motion.Hokan_cnt, 5, (u16) fr);
        }
        if (Key.trg & 0x80000) {
            pEm->m_Work0 += pEm->m_Work4;
            pEm->m_Work4 = 0;
            if ((int) pEm->m_Work0 > 0x9F) {
                pEm->m_Work0 = 0x9F;
            }
        }
        if (pEm->m_Work3 != -1) {
            u32 o = pEm->m_Work3 * 0x40 + 8;
            EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

            RouteCkToPos(pEm, &e->pos, &v, 0, 0);
            pEm->ang.y += Muku(&pEm->pos, &v, pEm->ang.y, 0.024543693f);
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        }
        MotionMove(pEm, 0);
        if (pEm->m_Work6 == 0) {
            if (plemRockEscapeCk(pEm)) {
                pEm->m_Work6 = 1;
            }
        }
        if (pEm->m_Work6 && w->Act_ck == 0) {
            if (pEm->m_Work7) {
                ActBtn.set(ACT_GUARD, 5, (void*) plemRockEscAction, pEm, ACTCTR_ENFORCE_EXEC | ACTCTR_EXACT_KEY, DISP_L_R, ACT_FUNC_NORMAL, 0);
            } else {
                ActBtn.set(ACT_GUARD, 5, (void*) plemRockEscAction, pEm, ACTCTR_ENFORCE_EXEC | ACTCTR_EXACT_KEY, DISP_A_B, ACT_FUNC_NORMAL, 0);
            }
        } else {
            ActBtn.set(ACT_SPRINT, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
        }
        break;
    case 4:
        mot = w->Mot_tbl[11];
        flag = 1;
        if (pEm->m_Work5) {
            flag = 0x41;
        }
        MotionSetCore(pEm, &pEm->Motion, mot, 0, 3, flag, 0);
        pEm->m_Work0 = 20;
        SndCall(1, 0x48, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        SndCall(1, 0x11, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        pEm->r_no_2++;
    case 5:
        if (pEm->m_Work0) {
            pEm->m_Work0--;
            plemRockEscapeCamMove(pEm, 1.0f);
        } else {
            plemRockEscapeCamMove2(pEm, pEm->m_Work5);
        }
        pEm->dmg.m_Timer = 0x78;
        if (pEm->Motion.Seq_frame > 11.7f && pEm->Motion.Seq_frame < 12.3f) {
            EstSet(0, -1, &pEm->pos, 0, EFF_PL00, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(5, 5, &pEm->pos, 0, 0, pEm);
        }
        if (MotionMove(pEm, 0)) {
            SpfFlagOff(pG, SPF_KEY);
            Cckpt.lifeMeterDisp(1);
            pEm->Wep->setTrans(1, 0);
            GameSave.save(pSaveData, -1);
            EndPlDamage();
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Stores the 16 player motions of the boulder chase escape (plemRockEscape steps).
void cEmRock::setPlMotion(void** pTbl)
{
    EmRockWork* w = EMROCK_WK(this);

    w->Mot_tbl[0] = *pTbl++;
    w->Mot_tbl[1] = *pTbl++;
    w->Mot_tbl[2] = *pTbl++;
    w->Mot_tbl[3] = *pTbl++;
    w->Mot_tbl[4] = *pTbl++;
    w->Mot_tbl[5] = *pTbl++;
    w->Mot_tbl[6] = *pTbl++;
    w->Mot_tbl[7] = *pTbl++;
    w->Mot_tbl[8] = *pTbl++;
    w->Mot_tbl[9] = *pTbl++;
    w->Mot_tbl[10] = *pTbl++;
    w->Mot_tbl[11] = *pTbl++;
    w->Mot_tbl[12] = *pTbl++;
    w->Mot_tbl[13] = *pTbl++;
    w->Mot_tbl[14] = *pTbl++;
    w->Mot_tbl[15] = *pTbl++;
}

// Uniform scale and the matching collision radius (s x 600).
void cEmRock::setScale(f32 mag)
{
    scale.z = mag;
    scale.y = mag;
    scale.x = mag;
    EMROCK_WK(this)->Radius = mag * 600.0f;
}

// Last EMI route point (type 6): the player runs towards it. -1 when there is none.
int plemRockSetEscapeRoute()
{
    EmiData* emi;
    int i;

    emi = pG->pEmi;
    if (emi == 0) {
        return -1;
    }
    for (i = emi->n - 1; i >= 0; i--) {
        if (emi->entry[i].type == 6) {
            return i;
        }
    }
    return -1;
}

// The player reached the escape goal (EMI type 7, 3000 units): its sub type goes to x3F4.
int plemRockEscapeCk(cPlayer* pEm)
{
    u8* emi;
    EmiEntry* e;
    int i;
    int idx;

    emi = (u8*) pG->pEmi;
    if (emi == 0) {
        return 0;
    }
    idx = -1;
    for (i = 0; i < pG->pEmi->n; i++) {
        u32 o = i * 0x40 + 8;

        if (((u8*) pG->pEmi)[o] == 7) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        return 0;
    }
    {
        u32 o = idx * 0x40 + 8;

        e = (EmiEntry*) ((u8*) pG->pEmi + o);
    }
    if ((pEm->pos.x - e->pos.x) * (pEm->pos.x - e->pos.x) + (pEm->pos.z - e->pos.z) * (pEm->pos.z - e->pos.z) > 9000000.0f) {
        return 0;
    }
    pEm->m_Work5 = e->sub;
    return 1;
}

// Action button callback of the boulder chase: marks the press and advances the escape step.
void plemRockEscAction(cEmRock* ptr)
{
    EMROCK_WK(ptr)->Act_ck = 1;
    ptr->r_no_2++;
}

// Camera behind the running player, blended from the current camera by `rate`, shaken a little
// and pulled in front of the scenery.
void plemRockEscapeCamMove(cPlayer* pEm, f32 rate)
{
    static Vec emRock_campos = { 500.0f, 200.0f, 3000.0f };
    static Vec emRock_target = { 250.0f, 1500.0f, 0.0f };
    Vec p0;
    Vec p1;
    Vec r;
    Vec hit;
    Vec d;
    f32 len;
    GlobalWork* g = pG;
    Camera* cam = &emRockCam;

    cam->param.fovy = 27.0f;
    PSMTXMultVec(pEm->mat, &emRock_campos, &p0);
    PSMTXMultVec(pEm->mat, &emRock_target, &p1);
    PosToPos(&g->Camera.param.at, &p1, &emRockCam.param.at, rate);
    PosToPos(&g->Camera.param.pos, &p0, &emRockCam.param.pos, rate);
    r.x = fRand1_1() * 10.0f;
    r.y = fRand1_1() * 10.0f;
    r.z = fRand1_1() * 10.0f;
    PSVECAdd(&emRockCam.param.pos, &r, &emRockCam.param.pos);
    PSVECAdd(&emRockCam.param.at, &r, &emRockCam.param.at);
    if (EatMgr.hitCheck(&emRockCam.param.at, &emRockCam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &emRockCam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 2738 "D:/Bio4/Prog/emrock.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&emRockCam.param.at, &d, &emRockCam.param.pos);
    }
    {
        Camera* cam = &emRockCam;
        Vec* cp = &cam->param.pos;
        Vec* ca = &cam->param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Camera of the side jump at the goal (`side` = the goal's sub type).
void plemRockEscapeCamMove2(cPlayer* pEm, int mode)
{
    static Vec emRock_campos = { -30.0f, 490.0f, -1424.0f };
    static Vec emRock_target = { 397.0f, 1301.0f, 1408.0f };
    Vec p0;
    Vec p1;
    Vec r;
    f32 len;
    Camera* gcam = &pG->Camera;

    emRockCam.param.fovy = 50.0f;
    if (mode) {
        emRock_campos.x = 30.0f;
        emRock_target.x = -397.0f;
    } else {
        emRock_campos.x = -30.0f;
        emRock_target.x = 397.0f;
    }
    if (pG->stage_no == 1 && pG->room_no == 6) {
        emRock_campos.y = 690.0f;
    } else {
        emRock_campos.y = 490.0f;
    }
    PSMTXMultVec(pEm->mat, &emRock_campos, &p0);
    PSMTXMultVec(pEm->mat, &emRock_target, &p1);
    PosToPos(&gcam->param.at, &p1, &emRockCam.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &p0, &emRockCam.param.pos, 1.0f);
    r.x = fRand1_1() * 10.0f;
    r.y = fRand1_1() * 10.0f;
    r.z = fRand1_1() * 10.0f;
    PSVECAdd(&emRockCam.param.pos, &r, &emRockCam.param.pos);
    PSVECAdd(&emRockCam.param.at, &r, &emRockCam.param.at);
    {
        Camera* cam = &emRockCam;
        Vec* cp = &emRockCam.param.pos;
        Vec* ca = &emRockCam.param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Camera of the player crushed by the dropping rock: looks at him from the rock's side.
void plemRockDropDieCamMove(cEmRock* pEm)
{
    Vec p;
    f32 len;
    cParts* parts;
    Camera* gcam = &pG->Camera;

    emRockCam.param.fovy = 50.0f;
    if (Muku(&pEm->pos, &pPL->pos, pEm->ang.y, 3.1415927f) < 0.0f) {
        p.x = -10000.0f;
        p.y = 5000.0f;
        p.z = 0.0f;
    } else {
        p.x = 10000.0f;
        p.y = 5000.0f;
        p.z = 0.0f;
    }
    PSMTXMultVec(pEm->mat, &p, &p);
    parts = pPL->getPartsPtr(0);
    PosToPos(&gcam->param.at, &parts->world, &emRockCam.param.at, 0.1f);
    PosToPos(&gcam->param.pos, &p, &emRockCam.param.pos, 0.1f);
    {
        Camera* cam = &emRockCam;
        Vec* cp = &emRockCam.param.pos;
        Vec* ca = &emRockCam.param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Camera of the rock being pushed loose (per room), looking at the rock.
void emRockPushCamMove(cEmRock* pEm)
{
    Vec p;
    f32 len;
    cParts* parts;
    Camera* gcam = &pG->Camera;

    emRockCam.param.fovy = 50.0f;
    switch (pG->room_no) {
    case 4:
    default:
        p.x = 83070.0f;
        p.y = 11064.0f;
        p.z = -29906.0f;
        break;
    case 6:
        p.x = 15434.0f;
        p.y = 4482.0f;
        p.z = -7368.0f;
        break;
    case 0xA:
        p.x = -51287.0f;
        p.y = 15308.0f;
        p.z = 65768.0f;
        break;
    case 0:
        p.x = -6801.0f;
        p.y = -1833.0f;
        p.z = -9193.0f;
        break;
    }
    parts = pEm->getPartsPtr(0);
    PosToPos(&gcam->param.at, &parts->world, &emRockCam.param.at, 1.0f);
    emRockCam.param.pos = p;
    {
        Camera* cam = &emRockCam;
        Vec* cp = &emRockCam.param.pos;
        Vec* ca = &emRockCam.param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Fixed camera of the rock starting to roll (per room).
void emRockPushCamMove2(cEmRock* pEm)
{
    Vec p0;
    Vec p1;
    f32 len;

    emRockCam.param.fovy = 27.0f;
    switch (pG->room_no) {
    case 4:
    default:
        p0.x = 53244.0f;
        p0.y = 3116.0f;
        p0.z = -28110.0f;
        p1.x = 58819.0f;
        p1.y = 4385.0f;
        p1.z = -28363.0f;
        break;
    case 6:
        p0.x = 32002.0f;
        p0.y = -6025.0f;
        p0.z = 4672.0f;
        p1.x = 27490.0f;
        p1.y = -3769.0f;
        p1.z = 2875.0f;
        break;
    case 0xA:
        p0.x = -34645.0f;
        p0.y = 4086.0f;
        p0.z = 67755.0f;
        p1.x = -39140.0f;
        p1.y = 6867.0f;
        p1.z = 69202.0f;
        break;
    }
    emRockCam.param.pos = p0;
    emRockCam.param.at = p1;
    {
        Camera* cam = &emRockCam;
        Vec* cp = &cam->param.pos;
        Vec* ca = &cam->param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Fixed camera of the drop scene.
void emRockDropCamMove(cEmRock* em)
{
    Vec p0;
    Vec p1;
    f32 len;
    Camera* cam = &emRockCam;
    Vec* cp = &cam->param.pos;
    Vec* ca = &cam->param.at;

    cam->param.fovy = 50.0f;
    p0.x = -5217.81f;
    p0.y = -12316.48f;
    p0.z = -16037.2f;
    p1.x = -5256.36f;
    p1.y = -10495.61f;
    p1.z = -15051.18f;
    cam->param.pos = p0;
    cam->param.at = p1;
    // `up` is set BEFORE `len`: with the up stores after the six len loads, the up.x store is the
    // 34th memory insn of the block and sched1 flushes its pending lists there (haifa's 32-entry
    // limit), which pins the 1.0/0.0 stores behind it and swaps the three pool highs (r27..r29).
    cam->Up.x = 0.0f;
    cam->Up.y = 1.0f;
    cam->Up.z = 0.0f;
    len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
    cam->Distance = SQRTF(len);
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// Enemies (ids 0x10..0x20) within 1.5 radii of the rock are knocked down (routine 3/4).
void emRockRunDownCk(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    cParts* p = pEm->getPartsPtr(0);
    cEm* e;
    Vec v;
    f32 len;
    f32 r;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        e = EmMgr.fastAt(i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == pEm) {
            continue;
        }
        v = e->pos;
        v.y += 1000.0f;
        r = w->Radius * 1.5f;
        len = (p->world.x - v.x) * (p->world.x - v.x) + (p->world.y - v.y) * (p->world.y - v.y) +
              (p->world.z - v.z) * (p->world.z - v.z);
        if (len < r * r) {
            e->hp = 0;
            e->r_no_0 = 3;
            e->r_no_1 = 4;
            e->r_no_2 = 0;
            e->r_no_3 = 0;
        }
    }
}

// Flying rock against the player (`atk` with the rock's radius as range): 1 on a hit.
int emRockAtkCk(cEmRock* em, EmAtkInfo* atk, int type, f32 r)
{
    EmRockWork* w = EMROCK_WK(em);
    EmAtkInfo a;

    if (atk) {
        a = *atk;
        a.range = w->Radius;
        if (EmAtkHitCk(&a, &em->pos, &em->pos_old, 1)) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            if (w->se8D[0] != 0xFF && w->se8D[1] != 0xFF) {
                SndCall(w->se8D[0], w->se8D[1], &em->pos, w->se8D[2], 0, em);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (type) {
                PlSetDamage(PL_DM_AUTO, 0, 0);
            }
            if (w->eff9C[0] != 0xFF && w->eff9C[1] != 0xFF) {
                EmPlBloodSet2(em, &em->pos, 1, w->eff9C[0], w->eff9C[1]);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
            }
            return 1;
        }
    }
    return 0;
}

// Starts the push motions (plMot[13..15], round robin) on the enemies pushing the rock.
void emRockPushCk(cEmRock* pEm, int frame)
{
    EmRockWork* w = EMROCK_WK(pEm);
    cEm* e;
    u32 n;
    u32 i;

    n = 0;
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        e = EmMgr.fastAt(i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->set != 0x1C) {
            continue;
        }
        switch (n) {
        case 0:
        default:
            MotionSetCore(e, &e->Motion, w->Mot_tbl[13], 0, 0, 1, (u16) frame);
            break;
        case 1:
            MotionSetCore(e, &e->Motion, w->Mot_tbl[14], 0, 0, 1, (u16) frame);
            break;
        case 2:
            MotionSetCore(e, &e->Motion, w->Mot_tbl[15], 0, 0, 1, (u16) frame);
            break;
        }
        n++;
        if (n > 2) {
            n = 0;
        }
        e->flag |= 1;
    }
}

// Ceiling drop setup (Rno1 7): loosening motions a / b, player death motion c, partner death
// motion d.
void cEmRock::setDropMot(void* a, void* b, void* c, void* d)
{
    EmRockWork* w = EMROCK_WK(this);

    w->Mot_wait = a;
    w->Mot_drop = b;
    w->Mot_pldie = c;
    w->Mot_subdie = d;
    r_no_0 = 1;
    r_no_1 = 7;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Ceiling drop with escape (Rno1 8): loosening motion a, player death b, escape c, notice d, and
// three escape player motions e / f / g.
void cEmRock::setDropMot2(void* a, void* b, void* c, void* d, void* e, void* f, void* g)
{
    EmRockWork* w = EMROCK_WK(this);

    w->Mot_drop = a;
    w->Mot_pldie = b;
    w->Mot_plesc = c;
    w->Mot_plfind = d;
    w->Mot_tbl[13] = e;
    w->Mot_tbl[14] = f;
    w->Mot_tbl[15] = g;
    r_no_0 = 1;
    r_no_1 = 8;
    r_no_2 = 0;
    r_no_3 = 0;
}

// The dropping rock reached the player (radius + 1000): starts the death routine. 1 on a hit.
int emRockDropHitCk(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    cParts* p;
    int dead;
    f32 len;
    f32 r;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    if (w->Mot_pldie == 0) {
        return 0;
    }
    p = pEm->getPartsPtr(0);
    len = (p->world.x - pPL->pos.x) * (p->world.x - pPL->pos.x) + (p->world.y - pPL->pos.y) * (p->world.y - pPL->pos.y) +
          (p->world.z - pPL->pos.z) * (p->world.z - pPL->pos.z);
    r = w->Radius + 1000.0f;
    if (len > r * r) {
        return 0;
    }
    SetPlDamage(pEm, plemDropDie);
    return 1;
}

// Same for the sub character.
int emRockDropHitCkSub(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    cParts* p;
    int dead;
    f32 len;
    f32 r;

    if (pSUB == 0) {
        return 0;
    }
    if ((s16) pG->ashley_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!pSUB->dmg.m_Flag && !pSUB->dmg.m_Timer) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    if (w->Mot_subdie == 0) {
        return 0;
    }
    p = pEm->getPartsPtr(0);
    len = (p->world.x - pSUB->pos.x) * (p->world.x - pSUB->pos.x) + (p->world.y - pSUB->pos.y) * (p->world.y - pSUB->pos.y) +
          (p->world.z - pSUB->pos.z) * (p->world.z - pSUB->pos.z);
    r = w->Radius + 1000.0f;
    if (len > r * r) {
        return 0;
    }
    SetSubDamage(pEm, subemDropDie);
    return 1;
}

// The dropping rock hit an em2b (parts 2 within radius + 2000): knocks it down unless flagged. 1 on a hit.
int emRockDropHitCkEm2b(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    cParts* p = pEm->getPartsPtr(0);
    cEm* e;
    cParts* q;
    f32 len;
    f32 r;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        e = EmMgr.fastAt(i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2B) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == pEm) {
            continue;
        }
        q = e->getPartsPtr(2);
        len = (q->world.x - p->world.x) * (q->world.x - p->world.x) +
              (q->world.y - p->world.y) * (q->world.y - p->world.y) +
              (q->world.z - p->world.z) * (q->world.z - p->world.z);
        r = w->Radius + 2000.0f;
        if (len < r * r) {
            if (!(e->flag & 8)) {
                e->r_no_0 = 2;
                e->r_no_1 = 4;
                e->r_no_2 = 0;
                e->r_no_3 = 0;
            }
            return 1;
        }
    }
    return 0;
}

// Player damage routine: crushed by the dropping rock.
void plemDropDie(cPlayer* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm->pEmCatch);

    pEm->subArc = pEm->pEmCatch->subArc;
    switch (pEm->r_no_2) {
    case 0:
        MotionSetCore(pEm, &pEm->Motion, w->Mot_pldie, 0, 3, 1, 0);
        pG->pl_life = 0;
        PlSetDamageSe(0xD);
        pEm->r_no_2++;
    case 1:
        if (pEm->r_no_3 == 0) {
            plemRockDropDieCamMove((cEmRock*)pEm->pEmCatch);
        } else {
            emRockDropCamMove((cEmRock*)pEm->pEmCatch);
        }
        MotionMove(pEm, 0);
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Sub character damage routine: crushed by the dropping rock.
void subemDropDie()
{
    cEm* sub = pSUB;
    EmRockWork* w = EMROCK_WK(sub->pEmCatch);

    sub->subArc = sub->pEmCatch->subArc;
    switch (sub->r_no_2) {
    case 0:
        MotionSetCore(sub, &sub->Motion, w->Mot_subdie, 0, 3, 1, 0);
        pG->ashley_life = 0;
        sub->r_no_2++;
    case 1:
        MotionMove(sub, 0);
        break;
    }
    sub->subArc = sub->subArc2;
}

// Room 11E: the rock breaks (effect, sound) and stops.
void cEmRock::setBreakR11E()
{
    EmRockWork* w = EMROCK_WK(this);

    EstSet(0, -1, &getPartsPtr(0)->world, 0, EFF_ROOM, 2, 0, ESP_CORE_KIND_NONE, 0, 0);
    SndCall(6, 7, &pos, 0, 0, this);
    hp = 0;
    be_flag &= ~2;
    atari.m_flag &= ~0x200;
    r_no_0 = 1;
    r_no_1 = 1;
    r_no_2 = 0;
    r_no_3 = 0;
    EffectEspgenDelete(0, w->espKind, this);
}

// Room 11E type 3 rocks: deactivates the rock's scenario collision piece.
void emRockSatClear(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);

    if (pG->room_id != 0x11E) {
        return;
    }
    if (pEm->type != 3) {
        return;
    }
    if (w->pSat == 0) {
        return;
    }
    w->pSat->setDisable();
}

// Room 11E type 3 rocks: keeps a scenario collision piece at the rock's parts 0 while visible
// (the boulders the player must climb around).
void emRockSatSet(cEmRock* pEm)
{
    EmRockWork* w = EMROCK_WK(pEm);
    Vec pos;
    Vec rot;

    if (pG->room_id != 0x11E) {
        return;
    }
    if (pEm->type != 3) {
        return;
    }
    emRockSatClear(pEm);
    if (!(pEm->be_flag & 2)) {
        return;
    }
    pos = pEm->getPartsPtr(0)->world;
    pos.y -= 2800.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    if (w->pSat) {
        w->pSat->setEnable();
        w->pSat->setCoord(&pos, &rot);
    } else {
        w->pSat = EatMgr.create((void*) (((u32*) pG->pRoom)[5] + (u32) pG->pRoom), 0, &pos, &rot, 1);
    }
}
