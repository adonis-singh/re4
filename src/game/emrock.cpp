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

extern "C" {
int MotionMove(cModel* m, int a);
void EffectEspgenDelete(int Core_flg, int Core_kind, cModel* m);
int EmAtkHitCk(void* info, Vec* pPos, Vec* pPosOld, int flag);   // em_sub.cpp
}
void MotionSetCore(cModel* m, void* w, void* data, int seq, int hokan, int flags, int frame);   // motion.cpp (C++ linkage)
// cGameSave::save is `save(void*)` by name but the original reads a second argument (-1 here);
// ABI-identical redeclaration (dvd.h ReadCheckInfo).
int GameSaveSave(cGameSave* g, void* data, int mode) asm("save__9cGameSavePv");

// setYarareCube(0, 400, 800, 400) with the float arguments' moves issued before the `li r4, 0`
// (atari_init.h: GCC emits the argument moves in declaration order).
void setYarareCubeF(cEmRock* em, f32 x, f32 y, f32 z, Vec* size) asm("setYarareCube__7cEmRockP3Vecfff");

// Head of a key-frame motion data block (motion.h MotionData; motion.h's one-argument MotionMove
// prototype keeps it out of the em units).
struct RockMotData {
    u16 maxFrame;   // 0x00
};

// lockParts = 0 through an int parameter: the zero becomes an SImode pseudo shared with the
// later `= 0` stores (emmine SetMine).
static inline void LockPartsSet(cEm* em, int no)
{
    em->lockParts = no;
}

// Struct-member view of pPL (the pGS trick): the load stays below the preceding atari flag store.
struct PlayerPtr {
    cPlayer* p;
};
#define PLS (((PlayerPtr*) &pPL)->p)

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
static EmAtkInfo emRockAtk = { 1500.0f, 8, 9999, 0, 10, 0 };

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
        atariInitF(&em->atari, 0.0f, -(em->scale.y * 1200.0f) * 0.5f, 0.0f, em->scale.x * 1200.0f * 0.5f,
                   em->scale.x * 1200.0f * 0.5f, em->scale.x * 1200.0f * 0.5f, em->scale.y * 1200.0f * 0.5f, 0, 0x2000, 10);
    } else {
        atariInitF(&em->atari, 0.0f, 2000.0f, 0.0f, 2700.0f, 2700.0f, 2700.0f, 2000.0f, 0, 0x2000, 10);
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
    LockPartsSet(em, zero);
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
    em->pMotion = (void*) 0;
    w->plMot[2] = (void*) 0;
    w->plMot[3] = (void*) 0;
    w->plMot[4] = (void*) 0;
    w->plMot[5] = (void*) 0;
    w->plMot[6] = (void*) 0;
    w->plMot[7] = (void*) 0;
    w->plMot[8] = (void*) 0;
    w->plMot[9] = (void*) 0;
    w->plMot[10] = (void*) 0;
    w->plMot[11] = (void*) 0;
    w->mot1 = (void*) 0;
    w->mot0 = (void*) 0;
    w->mot2 = (void*) 0;
    w->mot3 = (void*) 0;
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
void cEmRock::beginEvent()
{
}

// Rocks take no weapon damage: the registered hit is simply cleared.
void emRockDmCk(cEmRock* em)
{
    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
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
void emRock_R0_Init(cEmRock* em)
{
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 Lost, 2 Parent, 3 Fall, 4 Throw, 5 Throw2, 6 Roll,
// 7 Drop, 8 Drop2).
void emRock_R0_Move(cEmRock* em)
{
    EmRock_R1_move_tbl[em->r_no_1](em);
}

// Rno1 == 0: resting rock; plays its motion or rebuilds the matrices; the type 1 boulder starts
// rolling (Rno1 6) when emRockRollStartCk fires (player crosses the trigger).
void emRock_R1_Set(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    if (em->pMotion) {
        MotionMove(em, 0);
    } else {
        RotMatrix(em->mat, &em->ang);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
    }
    em->partsWorldCalc();
    switch (em->r_no_2) {
    case 0:
        em->r_no_2++;
        break;
    case 1:
        if (w->Roll_flag == 0 && em->type == 1) {
            if (emRockRollStartCk(em)) {
                w->Roll_flag = 1;
                em->flag |= 1;
                em->r_no_0 = 1;
                em->r_no_1 = 6;
                em->r_no_2 = 0;
                em->r_no_3 = 0;
            }
        }
        break;
    }
}

// Rno1 == 1: hides the rock, drops ACTIVE and its effects, destroys the work 30 frames later.
void emRock_R1_Lost(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->hp = 0;
        em->be_flag &= ~2;
        em->clearStatus(EM_STATUS_ACTIVE);
        EffectEspgenDelete(0, w->espKind, em);
        em->r_no_2++;
        w->Timer = 30;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            EmMgr.destroy(em);
        }
        break;
    }
}

// Rno1 == 2: held: follows parts `oya_parts` of pEm_oya (rotation re-normalised unless Be_flg
// bit0) and plays its own motion when it has one; lost when the holder vanishes.
void emRock_R1_Parent(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cEm* parent = w->pEm_oya;
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;

    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    if (parent && parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->oya_parts)->mat, em->mat, m);
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
        PSMTXCopy(m, em->mat);
    }
    if (em->pMotion) {
        em->motFlags2 |= 0x40000000;
        MotionMove(em, 0);
    } else {
        em->partsMatCalc();
    }
    em->partsWorldCalc();
}

// Rno1 == 3: dropped straight down with `Gravity`, sliding along the scenery (EatMgr.adjust with
// Radius): a scenery contact ends it with the dust est 1/8 (hidden, Lost); hits the player through
// pAtk (emRockAtkCk); spins with the travelled distance; gives up after 60 frames.
void emRock_R1_Fall(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (em->r_no_2) {
    case 0:
        w->Timer2 = 60;
        em->r_no_2++;
    case 1:
        if (w->Timer2) {
            w->Timer2--;
            break;
        }
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->ang);
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        EstSet(0, -1, &em->pos, 0, 1, 8, 0, 0, 0, 0);
        break;
    }
    w->spd.y -= w->Gravity;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->pos_old, &em->pos, w->Radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        em->be_flag &= ~2;
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->ang);
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        EstSet(0, -1, &em->pos, 0, 1, 8, 0, 0, 0, 0);
        return;
    }
    if (w->pAtk) {
        emRockAtkCk(em, w->pAtk, 0, w->Radius);
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&em->pos, &em->pos_old, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = SQRTF((em->pos.x - em->pos_old.x) * (em->pos.x - em->pos_old.x) +
                    (em->pos.y - em->pos_old.y) * (em->pos.y - em->pos_old.y) +
                    (em->pos.z - em->pos_old.z) * (em->pos.z - em->pos_old.z));
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
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    emRockAtkScrCk(em);
}

// Rno1 == 4: the thrown boulder: flies with gravity, loops the whoosh SE, bounces off the scenery
// (speed reflected x 0.99; a hard landing plays seFall / effFall and shakes the camera), hits the
// player through pAtk, and stops (dust est, Lost) after 60 frames or when it comes to rest.
void emRock_R1_Throw(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (em->r_no_2) {
    case 0:
        w->Timer = 0;
        w->Timer2 = 60;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->alwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->seid_throw = SndCall(w->seAlways[0], w->seAlways[1], &em->pos, w->seAlways[2], 0, em);
            }
        }
        if (w->Timer2) {
            w->Timer2--;
            break;
        }
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->ang);
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        EstSet(0, -1, &em->pos, 0, 1, 8, 0, 0, 0, 0);
        break;
    }
    w->spd.y -= w->Gravity;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->pos_old, &em->pos, w->Radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        f32 spd;

        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.99f);
        if (nrm.y < 0.5f) {
            em->be_flag &= ~2;
            em->pos.x = em->mat[0][3];
            em->pos.y = em->mat[1][3];
            em->pos.z = em->mat[2][3];
            Matrix2AxisAngle(em->mat, &em->ang);
            em->r_no_0 = 1;
            em->r_no_1 = 1;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            EstSet(0, -1, &em->pos, 0, 1, 8, 0, 0, 0, 0);
            return;
        }
        if (w->spd.y > 50.0f) {
            if (w->seFall[0] != 0xFF && w->seFall[1] != 0xFF) {
                SndCall(w->seFall[0], w->seFall[1], &em->pos, w->seFall[2], 0, em);
            }
            if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                Vec fp;

                fp = em->pos;
                fp.y = EatMgr.getFloor(&fp, 600.0f, 100000.0f, 0, 0);
                EstSet(0, -1, &fp, 0, w->effFall[0], w->effFall[1], 0, 0, 0, 0);
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
        }
    }
    if (w->pAtk) {
        emRockAtkCk(em, w->pAtk, 1, w->Radius);
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&em->pos, &em->pos_old, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = SQRTF((em->pos.x - em->pos_old.x) * (em->pos.x - em->pos_old.x) +
                    (em->pos.y - em->pos_old.y) * (em->pos.y - em->pos_old.y) +
                    (em->pos.z - em->pos_old.z) * (em->pos.z - em->pos_old.z));
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
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    emRockAtkScrCk(em);
}

// Rno1 == 5: the event boulder thrown at the player (room 202 / 214): same flight, but the first
// scenery contact after 180 frames ends it with a crash SE and the big break est (1/1 when the
// player still has more than 500 life, else 1/2).
void emRock_R1_Throw2(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cAtariInfo* at;
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (em->r_no_2) {
    case 0:
        w->Timer = 0;
        w->Timer2 = 180;
        at = &em->atari;
        at->m_flag &= ~0x300;
        SndCall(6, 0x49, &em->pos, 0, 0, em);
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->alwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->seid_throw = SndCall(w->seAlways[0], w->seAlways[1], &em->pos, w->seAlways[2], 0, em);
            }
        }
        if (w->Timer2) {
            w->Timer2--;
            break;
        }
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->ang);
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        break;
    }
    w->spd.y -= w->Gravity;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->pos_old, &em->pos, w->Radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        Vec p;

        RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        if ((s16) pG->pl_life > 500) {
            w->pAtk->flag |= 4;
        } else {
            w->pAtk->flag &= ~4;
        }
        p = em->pos;
        p.y += 1000.0f;
        PlWepHitCheck2(0, &p, &p, 0x12, 3, 5000.0f);
        EffectEspgenDelete(0, w->espKind, em);
        if (nrm.y > 0.7f) {
            EstSet(0, -1, &em->pos, 0, 1, 1, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &em->pos, 0, 1, 2, 0, 0, 0, 0);
        }
        SndCall(6, 0x4A, &em->pos, 0, 0, em);
        em->be_flag &= ~2;
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        return;
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&em->pos, &em->pos_old, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = SQRTF((em->pos.x - em->pos_old.x) * (em->pos.x - em->pos_old.x) +
                    (em->pos.y - em->pos_old.y) * (em->pos.y - em->pos_old.y) +
                    (em->pos.z - em->pos_old.z) * (em->pos.z - em->pos_old.z));
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
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    emRockAtkScrCk(em);
}

// Rno1 == 6: the chase boulder: waits 75 frames (starting the player's escape routine
// plemRockEscape and the rumble SE), then follows the EMI route (type 6 points) with gravity 10,
// bouncing on the floor with dust, accelerating along the route; reaching the end (or losing the
// route) breaks it (SE, est 1/0x1F, Lost).
void emRock_R1_Roll(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    Vec d;
    f32 len;
    f32 ang;

    switch (em->r_no_2) {
    case 0:
        KeyStop(0xEFCF0000ULL);
        if (emRockSetRollRoute(em) == 0) {
            em->pos.x = em->mat[0][3];
            em->pos.y = em->mat[1][3];
            em->pos.z = em->mat[2][3];
            Matrix2AxisAngle(em->mat, &em->ang);
            em->r_no_0 = 1;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
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
        emRockPushCk(em, 0);
        em->atari.m_flag &= ~0x200;
        PLS->ang.y = em->ang.y;
        SetPlDamage(em, (void (*)(cPlayer*)) plemRockEscape);
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
        em->r_no_2++;
    case 1:
        if (w->Roll_wait) {
            w->Roll_wait--;
            if (w->Roll_wait == 0) {
                w->sndId2 = SndCall(6, 5, &em->pos, 0, 0, em);
            }
            return;
        }
        if (emRockSetRollSpd(em)) {
            SndStop(w->sndId2, 0);
            SndCall(6, 6, &em->pos, 0, 0, em);
            EstSet(0, -1, &em->pos, 0, 1, 0x1F, 0, 0, 0, 0);
            em->be_flag &= ~2;
            em->r_no_0 = 1;
            em->r_no_1 = 1;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            return;
        }
    default:
        w->spd.y -= 10.0f;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (w->rollWait) {
            w->rollWait--;
        } else {
            f32 floor;

            floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + w->Radius;
            if (em->pos.y < floor) {
                em->pos.y = floor;
                w->spd.y *= -0.5f;
                if (w->spd.y > 50.0f) {
                    Vec fp;

                    fp = em->pos;
                    fp.y -= w->Radius;
                    EstSet(0, -1, &fp, 0, 0xC8, 0, 0, 0, 0, 0);
                    SndCall(6, 7, &em->pos, 0, 0, em);
                    if (w->First_bound == 0) {
                        w->First_bound = 1;
                        w->spd.x = 0.0f;
                        w->spd.z = 0.0f;
                    }
                }
            }
        }
        emRockRollHitCk(em);
        emRockRunDownCk(em);
        {
            Mtx m;
            Vec up;
            Vec axis;

            PSVECSubtract(&em->pos, &em->pos_old, &d);
            PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
            len = SQRTF((em->pos.x - em->pos_old.x) * (em->pos.x - em->pos_old.x) +
                        (em->pos.y - em->pos_old.y) * (em->pos.y - em->pos_old.y) +
                        (em->pos.z - em->pos_old.z) * (em->pos.z - em->pos_old.z));
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
                PSMTXConcat(m, em->mat, em->mat);
            }
        }
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
        emRockAtkScrCk(em);
        break;
    }
}

// Rno1 == 7: the ceiling rock of setDropMot: plays the loosening motions (mot0 / mot1) with dust
// and creak SEs, then drops on the player: plemDropFind makes him notice it, and it either kills
// him (plemDropDie) or the escape succeeds; ends in Lost.
void emRock_R1_Drop(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.m_flag &= ~0x200;
        MotionSetCore(em, &em->pMotion, w->mot0, 0, 0, 1, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (!(em->flag & 1)) {
            break;
        }
        em->r_no_2++;
    case 2:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 1, 4, 0, w->espKind, (u32) em, 0);
        SndCall(6, 8, &em->pos, 0, 0, em);
        w->Timer = 37;
        em->r_no_2++;
    case 3:
        if (w->Timer) {
            w->Timer--;
            emRockDropHitCk(em);
            emRockDropHitCkSub(em);
            if (emRockDropHitCkEm2b(em)) {
                em->be_flag &= ~2;
                em->atari.m_flag &= ~0x200;
                em->hp = 0;
                em->r_no_0 = 1;
                em->r_no_1 = 1;
                em->r_no_2 = 0;
                em->r_no_3 = 0;
                EffectEspgenDelete(0, w->espKind, em);
                EstSet(0, -1, &em->getPartsPtr(0)->world, 0, 1, 1, 0, 0, 0, 0);
                SndCall(6, 7, &em->pos, 0, 0, em);
                break;
            }
            if (w->Timer == 0) {
                SndCall(6, 7, &em->pos, 0, 0, em);
                em->atari.m_flag |= 0x200;
            }
        }
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 4:
        break;
    }
    em->partsWorldCalc();
}

// Rno1 == 8: the setDropMot2 variant with the action-button escape: after the loosening motion
// a 31 frame window offers button 0x25 (randomly variant 3 or 4); no press kills the player
// (plemDropDie), a press plays the escape motion (plemDropEscape).
void emRock_R1_Drop2(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->r_no_2++;
        em->atari.m_flag &= ~0x200;
    case 1:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0);
        MotionMove(em, 0);
        if (!(em->flag & 1)) {
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        KeyStop(0xEFCF0000ULL);
        em->r_no_2++;
    case 2:
        w->Timer = 25;
        emRockPushCk(em, 50);
        FSet(pPL->ang.y, -0.1f);
        FSet(pPL->pos.x, -4924.0f);
        FSet(pPL->pos.y, -11950.0f);
        FSet(pPL->pos.z, -14770.0f);
        pPL->setPos(&pPL->pos);
        pPL->dmg.m_Timer = 2;
        SetPlDamage(em, (void (*)(cPlayer*)) plemDropFind);
        em->r_no_2++;
    case 3:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0);
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
            break;
        }
        em->r_no_2++;
        break;
    case 4:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0);
        EstSet(0, -1, 0, 0, 1, 5, 0, 0, 0, 0);
        SndCall(6, 4, &em->pos, 0, 0, em);
        w->Timer = 31;
        w->rnd = Rnd() & 1;
        w->Act_ck = 0;
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            SndCall(6, 5, &em->pos, 0, 0, em);
            em->be_flag &= ~2;
            em->r_no_0 = 1;
            em->r_no_1 = 1;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
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
                    SetPlDamage(em, (void (*)(cPlayer*)) plemDropDie);
                    pPL->r_no_3 = 1;
                    break;
                }
            }
            if (w->Act_ck == 0) {
                switch (w->rnd) {
                case 0:
                default:
                    ActBtn.set(0x25, 5, (int) plemDropEscAction, (int) em, 0x42, 3, 0, 0);
                    break;
                case 1:
                    ActBtn.set(0x25, 5, (int) plemDropEscAction, (int) em, 0x42, 4, 0, 0);
                    break;
                }
            }
        }
        break;
    }
    em->partsWorldCalc();
}

// Action button callback of Drop2: marks the escape and starts the player's escape damage routine.
void plemDropEscAction(cEmRock* em)
{
    EMROCK_WK(em)->Act_ck = 1;
    pPL->dmg.m_Timer = 2;
    SetPlDamage(em, (void (*)(cPlayer*)) plemDropEscape);
}

// Player damage routine of the drop: notices the rock, then the escape / death routine takes over.
void plemDropFind(cPlayer* pl)
{
    EmRockWork* w = EMROCK_WK(pl->pEmCatch);

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 2;
    switch (pl->r_no_2) {
    case 0:
        pl->m_Work0 = 25;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
            MotionSetCore(pl, &pl->pMotion, w->mot5, 0, 3, 1, 0);
            emRockPushCamMove((cEmRock*)pl->pEmCatch);
        } else {
            emRockDropCamMove((cEmRock*)pl->pEmCatch);
        }
        if (MotionMove(pl, 0)) {
            SpfFlagOff(pG, SPF_KEY);
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Player damage routine: the player dives out of the way of the dropping rock.
void plemDropEscape(cPlayer* pl)
{
    EmRockWork* w = EMROCK_WK(pl->pEmCatch);

    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, w->mot4, 0, 3, 1, 0);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->world, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->world, 0, 0, pl);
        pPL->dmg.m_Timer = 0x1E;
        pl->r_no_2++;
    case 1:
        if (pl->frame > 20.7f && pl->frame < 21.3f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 33.7f && pl->frame < 34.3f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0)) {
            SpfFlagOff(pG, SPF_KEY);
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// The rolling rock runs the player over: 1 when it hit him this frame.
int emRockRollHitCk(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
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
    if ((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z) >
        w->Radius * w->Radius) {
        return 0;
    }
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
    QuakeExec(0, 0, 5, 22.0f, 2);
    pG->pl_life = 0;
    PlSetDamage(8, 0, 0);
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
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
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
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
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
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
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
void cEmRock::setSeFall(u8 blk, u8 no, u8 vol)
{
    EmRockWork* w = EMROCK_WK(this);

    w->seFall[0] = blk;
    w->seFall[1] = no;
    w->seFall[2] = vol;
    w->seFall[3] = 0;
}

// Est spawned at the floor when the thrown rock lands hard (0xFF = none).
void cEmRock::setEffFall(u8 id, u8 type)
{
    EmRockWork* w = EMROCK_WK(this);

    w->effFall[0] = id;
    w->effFall[1] = type;
}

// Attaches a continuous est (trail / glow) to the rock under its Core_kind.
void cEmRock::setEffAlways(int id, int type)
{
    EstSet((int) this, -1, 0, 0, id, type, 0, EMROCK_WK(this)->espKind, (u32) this, 0);
}

// Gives the rock a hit box (offset `size` or 400 below the origin) of x / y / z so it can be
// shot (hp 1, e.g. the r300 rock that must be broken).
void cEmRock::setYarareCube(Vec* size, f32 x, f32 y, f32 z)
{
    if (size) {
        YarareInitCube(this, size->x, size->y, size->z, x, y, z, 0, 1);
    } else {
        YarareInitCube(this, 0.0f, -400.0f, 0.0f, x, y, z, 0, 1);
    }
    hp = 1;
}

// on == 0 keeps the rock hidden (Be_flg bit1), on != 0 shows it.
void cEmRock::setTransMode(int on)
{
    EmRockWork* w = EMROCK_WK(this);

    if (on) {
        w->Be_flg &= ~2;
    } else {
        w->Be_flg |= 2;
    }
}

// Scenario event triggers (EMI type 3) the flying rock passes over: sets the pG->flags_174 event
// bits selected by the entry's sub type (room 119 fires a second bit while the trigger is fresh).
void emRockAtkScrCk(cEmRock* em)
{
    int i;

    if (pG->pEmi == 0) {
        return;
    }
    for (i = 0; i < *(int*) pG->pEmi; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (((u8*) pG->pEmi)[o] != 3) {
            continue;
        }
        if (e->state == 3) {
            continue;
        }
        if ((e->pos.x - em->pos.x) * (e->pos.x - em->pos.x) + (e->pos.z - em->pos.z) * (e->pos.z - em->pos.z) >
            9000000.0f) {
            continue;
        }
        if (pG->stage_no == 1 && pG->room_no == 0x19) {
            if (e->state == 0) {
                switch (e->sub) {
                case 0:
                    BitOn(pG->Room_flg[0], 0x80000000);
                    BitOn(pG->Room_flg[0], 0x10000000);
                    break;
                case 1:
                    BitOn(pG->Room_flg[0], 0x40000000);
                    BitOn(pG->Room_flg[0], 0x08000000);
                    break;
                case 2:
                    BitOn(pG->Room_flg[0], 0x20000000);
                    BitOn(pG->Room_flg[0], 0x04000000);
                    break;
                }
            } else {
                switch (e->sub) {
                case 0:
                    pG->Room_flg[0] |= 0x80000000;
                    break;
                case 1:
                    pG->Room_flg[0] |= 0x40000000;
                    break;
                case 2:
                    pG->Room_flg[0] |= 0x20000000;
                    break;
                }
            }
        }
        e->state = 3;
    }
}

// First EMI route point (type 6): 1 when found (routeIdx / pRoute set).
int emRockSetRollRoute(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    u8* emi;
    int i;
    int idx;

    emi = (u8*) pG->pEmi;
    if (emi == 0) {
        return 0;
    }
    idx = -1;
    for (i = 0; i < *(int*) pG->pEmi; i++) {
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

        w->pRoute = (EmiEntry*) ((u8*) pGS->pEmi + o);
    }
    return 1;
}

// Steers the rolling speed towards the current route point, advancing to the next one within
// 500 units; 1 when the route ends (the rock stops).
int emRockSetRollSpd(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
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
    spd = (e->pos.x - em->pos.x) * (e->pos.x - em->pos.x) + (e->pos.z - em->pos.z) * (e->pos.z - em->pos.z);
    if (spd < 250000.0f) {
        idx = -1;
        for (i = w->Rock_route + 1; i < *(int*) pG->pEmi; i++) {
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

            e = (EmiEntry*) ((u8*) pGS->pEmi + o);
        }
        w->pRoute = e;
    }
    PSVECSubtract(&e->pos, &em->pos, &dir);
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
int emRockRollStartCk(cEmRock* em)
{
    EmiData* emi;
    int dead;
    int i;
    GlobalWork* g;

    emi = (EmiData*) pG->pEmi;
    g = pGS;  // the struct-view read is a second pG pseudo (`mr r11,r9`) that the pl_life test reads
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
void plemRockEscape(cPlayer* pl)
{
    EmRockWork* w = EMROCK_WK(pl->pEmCatch);
    void* mot;
    void* mot2;
    Vec v;
    int lim;
    int n;
    int flag;

    pl->subArc = pl->pEmCatch->subArc;
    mot2 = w->plMot[3];
    mot = w->plMot[2];
    switch (pl->r_no_2) {
    case 0:
        pl->m_Work0 = 85;
        Cckpt.lifeMeterDisp(0);
        pl->Wep->setTrans(0, 0);
        switch (pG->room_no) {
        case 4:
        default:
            FSet(pPL->pos.x, 57947.0f);
            FSet(pPL->pos.y, 3273.0f);
            FSet(pPL->pos.z, -27900.0f);
            pl->ang.y = 1.67f;
            break;
        case 6:
            FSet(pPL->pos.x, 28428.0f);
            FSet(pPL->pos.y, -5465.0f);
            FSet(pPL->pos.z, 2765.0f);
            pl->ang.y = -1.99f;
            break;
        case 0xA:
            FSet(pPL->pos.x, -38340.0f);
            FSet(pPL->pos.y, 5111.0f);
            FSet(pPL->pos.z, 68313.0f);
            pl->ang.y = -1.86f;
            break;
        }
        pl->r_no_2++;
    case 1:
        pl->dmg.m_Timer = 0x1E;
        if (pl->m_Work0) {
            pl->m_Work0--;
            emRockPushCamMove((cEmRock*)pl->pEmCatch);
            MotionSetCore(pl, &pl->pMotion, w->plMot[0], (int) w->plMot[1], 0, 1, 0);
            pl->ang.y += Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, 3.1415927f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
            MotionMove(pl, 0);
        } else {
            emRockPushCamMove2((cEmRock*)pl->pEmCatch);
            ActBtn.set(0x18, 5, 0, 0, 2, 2, 0, 0);
            if (MotionMove(pl, 0)) {
                pl->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->pMotion, mot, (int) mot2, 10, 5, 0);
        pl->m_Work0 = 0;
        pl->m_Work1 = 0;
        pl->m_Work2 = 0;
        pl->m_Work3 = plemRockSetEscapeRoute();
        pl->m_Work4 = 0;
        pl->m_Work5 = 0;
        pl->m_Work6 = 0;
        pl->m_Work7 = Rnd() & 1;
        pl->m_Fwork0 = 0.1f;
        w->Act_ck = 0;
        pl->r_no_2++;
    case 3:
        plemRockEscapeCamMove(pl, pl->m_Fwork0);
        pl->m_Fwork0 += 0.05f;
        if (pl->m_Fwork0 > 1.0f) {
            pl->m_Fwork0 = 1.0f;
        }
        lim = 8;
        if (pG->Game_level <= 2) {
            lim = 12;
        }
        if (pG->Game_level > 7) {
            lim = 5;
        }
        pl->m_Work4++;
        if (pl->m_Work4 > lim) {
            pl->m_Work4 = lim;
            pl->m_Work0 -= 5;
            if ((int) pl->m_Work0 < 0) {
                pl->m_Work0 = 0;
            }
        }
        n = (int) pl->m_Work0 / 20;
        if (n > 7) {
            n = 7;
        }
        if (n != pl->m_Work1) {
            f32 ratio;
            f32 f;
            u32 cnt;
            u32 fr;

            pl->m_Work1 = n;
            switch (n) {
            case 0:
            default:
                mot2 = w->plMot[3];
                break;
            case 1:
                mot2 = w->plMot[4];
                break;
            case 2:
                mot2 = w->plMot[5];
                break;
            case 3:
                mot2 = w->plMot[6];
                break;
            case 4:
                mot2 = w->plMot[7];
                break;
            case 5:
                mot2 = w->plMot[8];
                break;
            case 6:
                mot2 = w->plMot[9];
                break;
            case 7:
                mot2 = w->plMot[10];
                break;
            }
            ratio = pl->frame / (f32) pl->frameMax;
            cnt = ((RockMotData*) mot2)->maxFrame;
            f = (f32) cnt * ratio;
            fr = (u32) f + 1;
            if (fr >= cnt) {
                fr = 0;
            }
            MotionSetCore(pl, &pl->pMotion, mot, (int) mot2, pl->motHokanCnt, 5, (u16) fr);
        }
        if (Key.trg & 0x80000) {
            pl->m_Work0 += pl->m_Work4;
            pl->m_Work4 = 0;
            if ((int) pl->m_Work0 > 0x9F) {
                pl->m_Work0 = 0x9F;
            }
        }
        if (pl->m_Work3 != -1) {
            u32 o = pl->m_Work3 * 0x40 + 8;
            EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

            RouteCkToPos(pl, &e->pos, &v, 0, 0);
            pl->ang.y += Muku(&pl->pos, &v, pl->ang.y, 0.024543693f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        MotionMove(pl, 0);
        if (pl->m_Work6 == 0) {
            if (plemRockEscapeCk(pl)) {
                pl->m_Work6 = 1;
            }
        }
        if (pl->m_Work6 && w->Act_ck == 0) {
            if (pl->m_Work7) {
                ActBtn.set(0x25, 5, (int) plemRockEscAction, (int) pl, 0x42, 3, 0, 0);
            } else {
                ActBtn.set(0x25, 5, (int) plemRockEscAction, (int) pl, 0x42, 4, 0, 0);
            }
        } else {
            ActBtn.set(0x18, 5, 0, 0, 2, 2, 0, 0);
        }
        break;
    case 4:
        mot = w->plMot[11];
        flag = 1;
        if (pl->m_Work5) {
            flag = 0x41;
        }
        MotionSetCore(pl, &pl->pMotion, mot, 0, 3, flag, 0);
        pl->m_Work0 = 20;
        SndCall(1, 0x48, &pl->getPartsPtr(4)->world, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->world, 0, 0, pl);
        pl->r_no_2++;
    case 5:
        if (pl->m_Work0) {
            pl->m_Work0--;
            plemRockEscapeCamMove(pl, 1.0f);
        } else {
            plemRockEscapeCamMove2(pl, pl->m_Work5);
        }
        pl->dmg.m_Timer = 0x78;
        if (pl->frame > 11.7f && pl->frame < 12.3f) {
            EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0)) {
            SpfFlagOff(pG, SPF_KEY);
            Cckpt.lifeMeterDisp(1);
            pl->Wep->setTrans(1, 0);
            GameSaveSave(&GameSave, pSaveData, -1);
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Stores the 16 player motions of the boulder chase escape (plemRockEscape steps).
void cEmRock::setPlMotion(void** mot)
{
    EmRockWork* w = EMROCK_WK(this);

    w->plMot[0] = *mot++;
    w->plMot[1] = *mot++;
    w->plMot[2] = *mot++;
    w->plMot[3] = *mot++;
    w->plMot[4] = *mot++;
    w->plMot[5] = *mot++;
    w->plMot[6] = *mot++;
    w->plMot[7] = *mot++;
    w->plMot[8] = *mot++;
    w->plMot[9] = *mot++;
    w->plMot[10] = *mot++;
    w->plMot[11] = *mot++;
    w->plMot[12] = *mot++;
    w->plMot[13] = *mot++;
    w->plMot[14] = *mot++;
    w->plMot[15] = *mot++;
}

// Uniform scale and the matching collision radius (s x 600).
void cEmRock::setScale(f32 s)
{
    scale.z = s;
    scale.y = s;
    scale.x = s;
    EMROCK_WK(this)->Radius = s * 600.0f;
}

// Last EMI route point (type 6): the player runs towards it. -1 when there is none.
int plemRockSetEscapeRoute()
{
    EmiData* emi;
    int i;

    emi = (EmiData*) pG->pEmi;
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
int plemRockEscapeCk(cPlayer* pl)
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
    for (i = 0; i < *(int*) pG->pEmi; i++) {
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
    if ((pl->pos.x - e->pos.x) * (pl->pos.x - e->pos.x) + (pl->pos.z - e->pos.z) * (pl->pos.z - e->pos.z) > 9000000.0f) {
        return 0;
    }
    pl->m_Work5 = e->sub;
    return 1;
}

// Action button callback of the boulder chase: marks the press and advances the escape step.
void plemRockEscAction(cEmRock* em)
{
    EMROCK_WK(em)->Act_ck = 1;
    em->r_no_2++;
}

// Camera behind the running player, blended from the current camera by `rate`, shaken a little
// and pulled in front of the scenery.
void plemRockEscapeCamMove(cPlayer* pl, f32 rate)
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

    FSet(cam->param.fovy, 27.0f);
    PSMTXMultVec(pl->mat, &emRock_campos, &p0);
    PSMTXMultVec(pl->mat, &emRock_target, &p1);
    PosToPos(&g->Cam.param.at, &p1, &emRockCam.param.at, rate);
    PosToPos(&g->Cam.param.pos, &p0, &emRockCam.param.pos, rate);
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
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Camera of the side jump at the goal (`side` = the goal's sub type).
void plemRockEscapeCamMove2(cPlayer* pl, int side)
{
    static Vec emRock_campos = { -30.0f, 490.0f, -1424.0f };
    static Vec emRock_target = { 397.0f, 1301.0f, 1408.0f };
    Vec p0;
    Vec p1;
    Vec r;
    f32 len;
    Camera* gcam = &pG->Cam;

    emRockCam.param.fovy = 50.0f;
    if (side) {
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
    PSMTXMultVec(pl->mat, &emRock_campos, &p0);
    PSMTXMultVec(pl->mat, &emRock_target, &p1);
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
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Camera of the player crushed by the dropping rock: looks at him from the rock's side.
void plemRockDropDieCamMove(cEmRock* em)
{
    Vec p;
    f32 len;
    cModel* parts;
    Camera* gcam = &pG->Cam;

    emRockCam.param.fovy = 50.0f;
    if (Muku(&em->pos, &pPL->pos, em->ang.y, 3.1415927f) < 0.0f) {
        p.x = -10000.0f;
        p.y = 5000.0f;
        p.z = 0.0f;
    } else {
        p.x = 10000.0f;
        p.y = 5000.0f;
        p.z = 0.0f;
    }
    PSMTXMultVec(em->mat, &p, &p);
    parts = pPL->getPartsPtr(0);
    PosToPos(&gcam->param.at, &parts->world, &emRockCam.param.at, 0.1f);
    PosToPos(&gcam->param.pos, &p, &emRockCam.param.pos, 0.1f);
    {
        Camera* cam = &emRockCam;
        Vec* cp = &emRockCam.param.pos;
        Vec* ca = &emRockCam.param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Camera of the rock being pushed loose (per room), looking at the rock.
void emRockPushCamMove(cEmRock* em)
{
    Vec p;
    f32 len;
    cModel* parts;
    Camera* gcam = &pG->Cam;

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
    parts = em->getPartsPtr(0);
    PosToPos(&gcam->param.at, &parts->world, &emRockCam.param.at, 1.0f);
    emRockCam.param.pos = p;
    {
        Camera* cam = &emRockCam;
        Vec* cp = &emRockCam.param.pos;
        Vec* ca = &emRockCam.param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Fixed camera of the rock starting to roll (per room).
void emRockPushCamMove2(cEmRock* em)
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
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
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
    cam->up.x = 0.0f;
    cam->up.y = 1.0f;
    cam->up.z = 0.0f;
    len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
    cam->dist = SQRTF(len);
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// Enemies (ids 0x10..0x20) within 1.5 radii of the rock are knocked down (routine 3/4).
void emRockRunDownCk(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cModel* p = em->getPartsPtr(0);
    cEm* e;
    Vec v;
    f32 len;
    f32 r;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
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
        if (e == em) {
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
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->se8D[0] != 0xFF && w->se8D[1] != 0xFF) {
                SndCall(w->se8D[0], w->se8D[1], &em->pos, w->se8D[2], 0, em);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (type) {
                PlSetDamage(8, 0, 0);
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
void emRockPushCk(cEmRock* em, int frame)
{
    EmRockWork* w = EMROCK_WK(em);
    cEm* e;
    u32 n;
    u32 i;

    n = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
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
            MotionSetCore(e, &e->pMotion, w->plMot[13], 0, 0, 1, (u16) frame);
            break;
        case 1:
            MotionSetCore(e, &e->pMotion, w->plMot[14], 0, 0, 1, (u16) frame);
            break;
        case 2:
            MotionSetCore(e, &e->pMotion, w->plMot[15], 0, 0, 1, (u16) frame);
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

    w->mot0 = a;
    w->mot1 = b;
    w->mot2 = c;
    w->mot3 = d;
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

    w->mot1 = a;
    w->mot2 = b;
    w->mot4 = c;
    w->mot5 = d;
    w->plMot[13] = e;
    w->plMot[14] = f;
    w->plMot[15] = g;
    r_no_0 = 1;
    r_no_1 = 8;
    r_no_2 = 0;
    r_no_3 = 0;
}

// The dropping rock reached the player (radius + 1000): starts the death routine. 1 on a hit.
int emRockDropHitCk(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cModel* p;
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
    if (w->mot2 == 0) {
        return 0;
    }
    p = em->getPartsPtr(0);
    len = (p->world.x - pPL->pos.x) * (p->world.x - pPL->pos.x) + (p->world.y - pPL->pos.y) * (p->world.y - pPL->pos.y) +
          (p->world.z - pPL->pos.z) * (p->world.z - pPL->pos.z);
    r = w->Radius + 1000.0f;
    if (len > r * r) {
        return 0;
    }
    SetPlDamage(em, plemDropDie);
    return 1;
}

// Same for the sub character.
int emRockDropHitCkSub(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cModel* p;
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
    if (w->mot3 == 0) {
        return 0;
    }
    p = em->getPartsPtr(0);
    len = (p->world.x - pSUB->pos.x) * (p->world.x - pSUB->pos.x) + (p->world.y - pSUB->pos.y) * (p->world.y - pSUB->pos.y) +
          (p->world.z - pSUB->pos.z) * (p->world.z - pSUB->pos.z);
    r = w->Radius + 1000.0f;
    if (len > r * r) {
        return 0;
    }
    SetSubDamage(em, (void*) subemDropDie);
    return 1;
}

// The dropping rock hit an em2b (parts 2 within radius + 2000): knocks it down unless flagged. 1 on a hit.
int emRockDropHitCkEm2b(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cModel* p = em->getPartsPtr(0);
    cEm* e;
    cModel* q;
    f32 len;
    f32 r;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2B) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
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
void plemDropDie(cPlayer* pl)
{
    EmRockWork* w = EMROCK_WK(pl->pEmCatch);

    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, w->mot2, 0, 3, 1, 0);
        pG->pl_life = 0;
        PlSetDamageSe(0xD);
        pl->r_no_2++;
    case 1:
        if (pl->r_no_3 == 0) {
            plemRockDropDieCamMove((cEmRock*)pl->pEmCatch);
        } else {
            emRockDropCamMove((cEmRock*)pl->pEmCatch);
        }
        MotionMove(pl, 0);
        break;
    }
    pl->subArc = pl->subArc2;
}

// Sub character damage routine: crushed by the dropping rock.
void subemDropDie()
{
    cEm* sub = pSUB;
    EmRockWork* w = EMROCK_WK(sub->pEmCatch);

    sub->subArc = sub->pEmCatch->subArc;
    switch (sub->r_no_2) {
    case 0:
        MotionSetCore(sub, &sub->pMotion, w->mot3, 0, 3, 1, 0);
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

    EstSet(0, -1, &getPartsPtr(0)->world, 0, 1, 2, 0, 0, 0, 0);
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
void emRockSatClear(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    if (pG->room_id != 0x11E) {
        return;
    }
    if (em->type != 3) {
        return;
    }
    if (w->pSat == 0) {
        return;
    }
    w->pSat->m_Flag &= ~4;
}

// Room 11E type 3 rocks: keeps a scenario collision piece at the rock's parts 0 while visible
// (the boulders the player must climb around).
void emRockSatSet(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    Vec pos;
    Vec rot;

    if (pG->room_id != 0x11E) {
        return;
    }
    if (em->type != 3) {
        return;
    }
    emRockSatClear(em);
    if (!(em->be_flag & 2)) {
        return;
    }
    pos = em->getPartsPtr(0)->world;
    pos.y -= 2800.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    if (w->pSat) {
        w->pSat->m_Flag |= 4;
        w->pSat->setCoord(&pos, &rot);
    } else {
        w->pSat = EatMgr.create((void*) (((u32*) pG->pRoom)[5] + (u32) pG->pRoom), 0, &pos, &rot, 1);
    }
}
