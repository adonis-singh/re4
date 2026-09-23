// game/emtree.cpp: tree enemy (cEmTree): a felled trunk that hangs on a parent's parts, falls as a
// three-node rope, or is thrown / shot at the player.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emtree.h"
#include "at_mod.h"
#include "player.h"
#include "esp.h"
#include "snd.h"
#include "quake.h"
#include "pad.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "motion.h"
#include "est.h"
#include "em_sub.h"

extern "C" {
static void emTree_R0_Move(cEmTree* em);
}

typedef void (*EmTreeFunc)(cEmTree*);

EmTreeFunc EmTree_R0_move_tbl[4] = {
    emTree_R0_Init,
    emTree_R0_Move,
    0,
    0,
};

EmTreeFunc EmTree_R1_move_tbl[7] = {
    emTree_R1_Set,
    emTree_R1_LostWait,
    emTree_R1_Lost,
    emTree_R1_Parent,
    emTree_R1_Fall,
    emTree_R1_Throw,
    emTree_R1_Shot,
};

EmAtkInfo emTreeAtk = { 200.0f, PL_DM_AUTO, 400, 0, 10, 0 };

// Creates a tree enemy (id 0x49, at the back of the pool) from a model / TPL at pos / rot: the
// trunk El Gigante (r119) tears out and throws. Hit boxes, a solid atari, unlockable, SE / effect
// ids cleared, Core_kind 50 for its effects. Starts in Rno1 0 Set. NULL on failure.
cEmTree* SetTree(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cEmTree* em;
    EmTreeWork* w;

    em = (cEmTree*) EmMgr.createBack(0x49);
    if (em == 0) {
        return 0;
    }
    w = EMTREE_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetTree() ModelInit failed.");
        EmMgr.destroy(em);
        return 0;
    }
    YarareInit(em, 0.0f, 0.0f, 0.0f, 250.0f, 10000.0f, 1, YAT_FLAG_ON);
    int parts = 0;
    f32 zero = 0.0f;
    f32 h = 5000.0f;
    f32 r = 200.0f;
    em->atari.init(zero, h, zero, r, r, r, h, parts, 2, parts);
    em->hp_max = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 20000.0f, 20000.0f, 20000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->be_flag &= ~0x01000000;
    em->atari.setPriority(PRI_LV3);
    em->atari.offSca();
    w->Be_flg = 0;
    em->be_flag &= ~0x10;
    w->Fall_wait = 0;
    w->pParent = 0;
    w->pEm_old = 0;
    w->pAtk = 0;
    w->caught = 0;
    w->seFall[0] = 0xFF;
    w->seFall[1] = 0xFF;
    w->seFall[2] = 0;
    w->landed = 0;
    w->seHit[0] = 0xFF;
    w->seHit[1] = 0xFF;
    w->seHit[2] = 0;
    w->seWall[0] = 0xFF;
    w->seWall[1] = 0xFF;
    w->seWall[2] = 0;
    w->se64[0] = 0xFF;
    w->se64[1] = 0xFF;
    w->se64[2] = 0;
    w->seAlways[0] = 0xFF;
    w->seAlways[1] = 0xFF;
    w->seAlways[2] = 0;
    w->seAlwaysWait = 4;
    w->effFall[0] = 0xFF;
    w->effFall[1] = 0xFF;
    w->eff72[0] = 0xFF;
    w->eff72[1] = 0xFF;
    w->seid_throw = 0;
    w->effHit[0] = 0xFF;
    w->effHit[1] = 0xFF;
    em->Motion.pMot = 0;
    w->estNo = 50;
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    emTree_R0_Move(em);
    return em;
}

// Event start hook: nothing to do for trees.
void cEmTree::beginEvent(u32 flag)
{
}

// A weapon hit only spawns the wood-splinter est (owner 1, est 12) at the hit.
void emTreeDmCk(cEmTree* pEm)
{
    u8 wep;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    wep = pEm->dmg.m_Wep;
    pEm->dmg.m_Flag = 0;
    if (wep == 0x10) {
        pEm->dmg.m_Timer = 0x11;
    }
    EmDmBloodSet2(pEm, 1, 12, 0, 0, 0);
}

// Per-frame: damage check, the Rno0 routine, then the model-vs-player atari and mirroring of the
// parent's visibility / fade while attached; Be_flg bit1 hides the tree.
void cEmTree::move()
{
    EmTreeWork* w = EMTREE_WK(this);

    Motion.Mot_flag &= ~0x40000000;
    emTreeDmCk(this);
    EmTree_R0_move_tbl[r_no_0](this);
    if (isAlive()) {
        EmAtCheck(this);
        atari.move();
        if (w->pParent) {
            invisible_factor = w->pParent->invisible_factor;
            invisible_factor2 = w->pParent->invisible_factor2;
            if (w->pParent->be_flag & 2) {
                be_flag |= 2;
            } else {
                be_flag &= ~2;
            }
        }
        if (w->Be_flg & 2) {
            be_flag &= ~2;
        }
    }
}

// Rno0 == 0: resets to the Set state.
void emTree_R0_Init(cEmTree* pEm)
{
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 0;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 LostWait, 2 Lost, 3 Parent, 4 Fall, 5 Throw, 6 Shot).
static void emTree_R0_Move(cEmTree* pEm)
{
    EmTree_R1_move_tbl[pEm->r_no_1](pEm);
}

// Rno1 == 0: a standing tree; plays its motion or rebuilds the matrices from pos / ang.
void emTree_R1_Set(cEmTree* pEm)
{
    if (pEm->Motion.pMot) {
        MotionMove(pEm, 0);
    } else {
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        ScaleMatrix(pEm->mat, &pEm->scale);
        pEm->partsMatCalc();
    }
    pEm->partsWorldCalc();
}

// Rno1 == 1: a fallen trunk at rest; after 90 frames (or at once when off screen) fades out and
// goes to Lost.
void emTree_R1_LostWait(cEmTree* pEm)
{
    EmTreeWork* w = EMTREE_WK(pEm);
    Vec scr;
    Vec pos;

    switch (pEm->r_no_2) {
    case 0:
        w->Timer = 90;
        pEm->r_no_2++;
    case 1:
        if (w->Timer == 0) {
            pEm->invisible_factor -= 0.1f;
            if (pEm->invisible_factor <= 0.0f) {
                pEm->invisible_factor = 0.0f;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 2;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
                break;
            }
        } else {
            w->Timer--;
        }
        pos = pEm->pos;
        GetScreenPos(&pos, &scr);
        if (scr.z > 1.0f) {
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 2;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
        }
        break;
    }
    RotMatrix(pEm->mat, &pEm->ang);
    TransMatrix(pEm->mat, &pEm->pos);
    ScaleMatrix(pEm->mat, &pEm->scale);
    pEm->partsMatCalc();
    pEm->partsWorldCalc();
}

// Rno1 == 2: hides the tree, drops its collision and destroys the work 30 frames later.
void emTree_R1_Lost(cEmTree* pEm)
{
    EmTreeWork* w = EMTREE_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        pEm->hp = 0;
        pEm->atari.m_flag &= ~0x200;
        pEm->be_flag &= ~2;
        w->Timer = 30;
        pEm->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            EmMgr.destroy(pEm);
        }
        break;
    }
}

// Rno1 == 3: carried: follows parts `oya_parts` of pParent (rotation re-normalised unless
// Be_flg bit0), plays its motion when it has one, and counts Fall_wait down to setFall (a tree
// stuck in the player).
void emTree_R1_Parent(cEmTree* pEm)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmTreeWork* w = EMTREE_WK(pEm);
    cModel* parent = w->pParent;

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
#line 471 "D:/Bio4/Prog/emtree.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 473 "D:/Bio4/Prog/emtree.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 475 "D:/Bio4/Prog/emtree.cpp"
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
    if (w->Fall_wait) {
        w->Fall_wait--;
        if (w->Fall_wait == 0) {
            pEm->setFall();
        }
    }
}

// Rno1 == 4: the trunk tumbles as a 3-node rope (top, base, side point): gravity, 30 relaxation
// passes, floor contact with the landing SE / est and effect deletion, random bounce damping; the
// matrix is rebuilt from the nodes and the tree comes to rest (Rno1 1) when the node speeds are
// small.
void emTree_R1_Fall(cEmTree* pEm)
{
    EmTreeWork* w = EMTREE_WK(pEm);
    Vec pt[3] = {
        { 0.0f, 7000.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 500.0f, 3500.0f, 0.0f },
    };
    EmTreeNode node[3];
    EmTreeNode* n;
    EmTreeNode* nx;
    Vec b;
    Vec a;
    Vec c;
    Vec tmp;
    f32 floor;
    u32 i;
    u32 k;
    f32 mag;
    f32 d;

    pEm->hp = 0;
    floor = EatMgr.getFloor(&pEm->pos, 0, 600.0f, 100000.0f, 0) + 300.0f;
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.x = w->pt[i].x;
        n->spd.y = w->pt[i].y;
        n->spd.z = w->pt[i].z;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        PSMTXMultVec(pEm->mat, &pt[i], &n->pos);
        n->old = n->pos;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        if (i == 2) {
            nx = node;
        } else {
            nx = &node[i + 1];
        }
        n->len = GetDistance3(&n->pos, &nx->pos);
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.y -= 20.0f;
        PSVECAdd(&n->pos, &n->spd, &n->pos);
        n->reflect = 0;
    }
    for (k = 0; k < 30; k++) {
        for (i = 0; i < 3; i++) {
            n = &node[i];
            if (i == 2) {
                nx = node;
            } else {
                nx = &node[i + 1];
            }
            PSVECSubtract(&nx->pos, &n->pos, &tmp);
            mag = PSVECMag(&tmp);
            d = (n->len - mag) * 0.5f;
            PSVECScale(&tmp, &tmp, (1.0f / mag) * d);
            PSVECAdd(&nx->pos, &tmp, &nx->pos);
            PSVECSubtract(&n->pos, &tmp, &n->pos);
            if (n->pos.y < floor) {
                n->pos.y = floor;
                n->reflect = 1;
            }
            if (nx->pos.y < floor) {
                nx->pos.y = floor;
                nx->reflect = 1;
            }
        }
    }
    // `n` is one function-scope pointer shared by all the loops: the later mentions keep the
    // inner loop's giv from being marked replaceable by record_giv, so loop.c emits its final value
    // (`addi r0, node, 0x58` after the inner loop, inside the k loop) and cse2 turns this loop's
    // bound into a copy of it (`mr r22, r0`). Block-scoped `n`s give one `addi r22` here.
    for (i = 0; i < 3; i++) {
        n = &node[i];
        if (i == 2) {
            nx = node;
        } else {
            nx = &node[i + 1];
        }
        if (n->reflect) {
            if (w->landed == 0 && n->spd.y < -50.0f) {
                w->landed = 1;
                if (w->seFall[0] != 0xFF) {
                    SndCall(w->seFall[0], w->seFall[1], &pEm->pos, w->seFall[2], 0, pEm);
                }
                if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                    EstSet(pEm, -1, 0, 0, w->effFall[0], w->effFall[1], 0, ESP_CORE_KIND_NONE, pEm, 0);
                    pEm->be_flag &= ~2;
                    pEm->r_no_0 = 1;
                    pEm->r_no_1 = 2;
                    pEm->r_no_2 = 0;
                    pEm->r_no_3 = 0;
                    return;
                }
            }
            EffectEspDelete(0, w->estNo, pEm, 0);
            EffectEspgenDelete(0, w->estNo, pEm);
            EffectEfmDelete(0, w->estNo, pEm);
            n->spd.x *= fRand0_1() * 0.2f + 0.5f;
            n->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
            n->spd.z *= fRand0_1() * 0.2f + 0.5f;
            if (n->spd.y <= 20.0f) {
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
        w->pt[i].x = n->spd.x;
        w->pt[i].y = n->spd.y;
        w->pt[i].z = n->spd.z;
    }
    PSVECSubtract(&node[0].pos, &node[1].pos, &a);
    PSVECSubtract(&node[2].pos, &node[1].pos, &b);
    PSVECCrossProduct(&b, &a, &c);
    PSVECCrossProduct(&a, &c, &b);
#line 685 "D:/Bio4/Prog/emtree.cpp"
    VECNormalize(&b, &b);
#line 686 "D:/Bio4/Prog/emtree.cpp"
    VECNormalize(&a, &a);
#line 687 "D:/Bio4/Prog/emtree.cpp"
    VECNormalize(&c, &c);
    pEm->mat[0][0] = b.x;
    pEm->mat[1][0] = b.y;
    pEm->mat[2][0] = b.z;
    pEm->mat[0][1] = a.x;
    pEm->mat[1][1] = a.y;
    pEm->mat[2][1] = a.z;
    pEm->mat[0][2] = c.x;
    pEm->mat[1][2] = c.y;
    pEm->mat[2][2] = c.z;
    PSVECScale(&pt[0], &tmp, -1.0f);
    TransMatrix(pEm->mat, &node[0].pos);
    PSMTXMultVec(pEm->mat, &tmp, &tmp);
    TransMatrix(pEm->mat, &tmp);
    pEm->pos = tmp;
    mag = node[0].spd.x * node[0].spd.x + node[0].spd.y * node[0].spd.y + node[0].spd.z * node[0].spd.z
        + node[1].spd.x * node[1].spd.x + node[1].spd.y * node[1].spd.y + node[1].spd.z * node[1].spd.z
        + node[2].spd.x * node[2].spd.x + node[2].spd.y * node[2].spd.y + node[2].spd.z * node[2].spd.z;
    if (mag < 25.0f) {
        pEm->pos.x = pEm->mat[0][3];
        pEm->pos.y = pEm->mat[1][3];
        pEm->pos.z = pEm->mat[2][3];
        Matrix2AxisAngle(pEm->mat, &pEm->ang);
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 1;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
    }
    pEm->partsWorldCalc();
}

// Rno1 == 5: the thrown trunk flies with gravity 15 spinning end over end (36 degrees / frame
// about the axis perpendicular to its path), looping the whoosh SE; hitting the scenery or the
// player (EmAtkHitCk with pAtk: damage, vibration, quake, blood) makes it fall.
void emTree_R1_Throw(cEmTree* pEm)
{
    EmTreeWork* w = EMTREE_WK(pEm);
    Vec d;
    Mtx m;
    Vec up;
    Vec fwd;
    f32 ang;

    switch (pEm->r_no_2) {
    case 0:
        w->Timer = 0;
        pEm->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->seAlwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->seid_throw = SndCall(w->seAlways[0], w->seAlways[1], &pEm->pos, w->seAlways[2], 0, pEm);
            }
        }
        break;
    }
    w->spd.y -= 15.0f;
    PSVECAdd(&pEm->pos, &w->spd, &pEm->pos);
    if (EatMgr.hitCheck(&pEm->pos_old, &pEm->pos, 0, 0, 0, 0)) {
        pEm->setFall();
        if (w->seWall[0] != 0xFF && w->seWall[1] != 0xFF) {
            SndCall(w->seWall[0], w->seWall[1], &pEm->pos, w->seWall[2], 0, pEm);
        }
        SndStop(w->seid_throw, 0);
    } else if (w->pAtk) {
        if (EmAtkHitCk(w->pAtk, &pEm->pos, &pEm->pos_old, 1)) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &pEm->pos, w->seHit[2], 0, pEm);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
                EmPlBloodSet2(pEm, &pEm->pos, 1, w->effHit[0], w->effHit[1]);
            } else {
                EmPlBloodSet2(pEm, &pEm->pos, 1, 0xFF, 0xFF);
            }
            pEm->setFall();
        }
    }
    PSVECSubtract(&pEm->pos, &pEm->pos_old, &d);
    PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    fwd.x = 0.0f;
    fwd.y = 0.0f;
    fwd.z = 1.0f;
    PSMTXMultVecSR(m, &fwd, &fwd);
    if (fwd.x == 0.0f) {
        fwd.y = 0.0f;
    }
#line 825 "D:/Bio4/Prog/emtree.cpp"
    VECNormalize(&fwd, &fwd);
    ang = acosf(PSVECDotProduct(&up, &fwd));
    if (ang > 0.01f && ang < 3.1315927f) {
        PSVECCrossProduct(&up, &fwd, &up);
        PSMTXRotAxisRad(m, &up, 0.62831855f);
        PSMTXConcat(m, pEm->mat, pEm->mat);
    }
    TransMatrix(pEm->mat, &pEm->pos);
    pEm->partsWorldCalc();
}

// Rno1 == 6: the trunk launched straight (no gravity) for at most 90 frames; a scenery hit stops
// it in place (Rno2 2: rests 60 frames then falls), a player hit deals the pAtk damage and either
// makes it fall or, when the hit part is flagged 0x4000, impales the player: the tree is parented
// to that parts and drops after 30 frames (at once when the player is dead).
void emTree_R1_Shot(cEmTree* pEm)
{
    EmTreeWork* w = EMTREE_WK(pEm);
    Vec hit;
    Vec hitPos;
    Vec nrm;
    Mtx inv;
    YARARE_INFO* part;
    int no;
    f32 len;

    switch (pEm->r_no_2) {
    case 0:
        w->Timer = 0;
        w->Timer2 = 90;
        pEm->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->seAlwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->seid_throw = SndCall(w->seAlways[0], w->seAlways[1], &pEm->pos, w->seAlways[2], 0, pEm);
            }
        }
        if (w->Timer2) {
            w->Timer2--;
        } else {
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 2;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            return;
        }
        break;
    case 2:
        w->pEm_old = 0;
        w->Timer = 60;
        pEm->hp = 0;
        pEm->r_no_2++;
    case 3:
        pEm->partsWorldCalc();
        if (w->Timer) {
            w->Timer--;
        } else {
            pEm->setFall();
            SndStop(w->seid_throw, 0);
        }
        return;
    }
    w->spd.y -= 0.0f;
    PSVECAdd(&pEm->pos, &w->spd, &pEm->pos);
    if (EatMgr.hitCheck(&pEm->pos_old, &pEm->pos, &hit, 0, 0, 0)) {
        if (w->seWall[0] != 0xFF && w->seWall[1] != 0xFF) {
            SndCall(w->seWall[0], w->seWall[1], &pEm->pos, w->seWall[2], 0, pEm);
        }
        SndStop(w->seid_throw, 0);
        pEm->pos = hit;
        TransMatrix(pEm->mat, &pEm->pos);
        pEm->partsWorldCalc();
        pEm->r_no_2 = 2;
    } else if (w->pAtk && (part = (YARARE_INFO*) EmAtkLineHitCk(&pEm->pos_old, &pEm->pos, &hitPos, &nrm, 0)) != 0) {
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
            SndCall(w->seHit[0], w->seHit[1], &pEm->pos, w->seHit[2], 0, pEm);
        }
        SndStop(w->seid_throw, 0);
        QuakeExec(0, 0, 5, 22.0f, 2);
        if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
            EmPlBloodSet2(pEm, &pEm->pos, 1, w->effHit[0], w->effHit[1]);
        } else {
            EmPlBloodSet2(pEm, &pEm->pos, 1, 0xFF, 0xFF);
        }
        EmAtkSetDamagePL((cEm*) part, w->pAtk, &pEm->pos_old, &pEm->pos);
        if ((part->flag & YAT_FLAG_DMPOS) == 0) {
            pEm->setFall();
        } else {
            no = 0;
            if (part->parts_no != 0) {
                no = part->parts_no - 1;
            }
            PSMTXInverse(pPL->getPartsPtr(no)->mat, inv);
            PSMTXMultVec(inv, &part->cross, &pEm->pos);
            len = SQRTF(pEm->pos.x * pEm->pos.x + pEm->pos.z * pEm->pos.z);
            pEm->ang.x = -atan2f(-pEm->pos.y, len);
            pEm->ang.y = atan2f(-pEm->pos.x, -pEm->pos.z);
            pEm->ang.z = 0.0f;
            if ((s16) pG->pl_life <= 0) {
                w->Fall_wait = 0;
            } else {
                w->Fall_wait = 30;
            }
            pEm->setParent(pPL, no, 0);
            emTree_R1_Parent(pEm);
        }
    } else {
        TransMatrix(pEm->mat, &pEm->pos);
        pEm->partsWorldCalc();
    }
}

// Attaches the tree to parts `partsNo` of `parent` (Rno1 3); flag skips the matrix normalisation.
// Clears the parent's atari flag 0x200.
void cEmTree::setParent(cModel* parent, int partsNo, int flag)
{
    EmTreeWork* w = EMTREE_WK(this);

    w->pParent = parent;
    w->oya_parts = partsNo;
    if (flag) {
        w->Be_flg |= 1;
    } else {
        w->Be_flg &= ~1;
    }
    r_no_0 = 1;
    r_no_1 = 3;
    r_no_2 = 0;
    r_no_3 = 0;
    ((cEm*) parent)->atari.m_flag &= ~0x200;
}

// Detaches the tree and returns it to the Set state.
void cEmTree::clearParent()
{
    EmTreeWork* w = EMTREE_WK(this);

    w->pParent = 0;
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Drops the trunk (Rno1 4): random upward node speeds, detached, hp 0, pose taken from the
// current matrix.
void cEmTree::setFall()
{
    EmTreeWork* w = EMTREE_WK(this);
    cParts* parts;
    u32 i;

    Motion.pMot = 0;
    for (i = 0; i < 3; i++) {
        w->pt[i].x = fRand1_1() * 10.0f;
        w->pt[i].y = fRand1_1() * 10.0f + 50.0f;
        w->pt[i].z = fRand1_1() * 10.0f;
    }
    w->pParent = 0;
    w->pEm_old = 0;
    hp = 0;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &ang);
    parts = getPartsPtr(0);
    parts->ang.x = 0.0f;
    parts->ang.y = 0.0f;
    parts->ang.z = 0.0f;
    RotMatrix(parts->l_mat, &parts->ang);
    TransMatrix(parts->l_mat, &parts->pos);
    r_no_0 = 1;
    r_no_1 = 4;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Never called in the DOL: the linker dropped the bodies and kept the constant pools
// ({10, 20, 75, 350, 0, PI/2} twice after setFall's), see STRIP_UNUSED.
void cEmTree::setThrow(Vec* spd, EmAtkInfo* atk)
{
    EmTreeWork* w = EMTREE_WK(this);
    Vec v;
    Mtx m;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        }
    }
    PSMTXMultVecSR(mat, &v, &v);
    w->spd = v;
    ang.x = 0.0f;
    ang.y = atan2f(v.x, v.z);
    ang.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &ang);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(m, mat, mat);
    TransMatrix(mat, &pos);
    pos_old = pos;
    w->pParent = 0;
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emTreeAtk;
    }
    r_no_0 = 1;
    r_no_1 = 5;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Launches the trunk straight at the player (Rno1 6) with speed `spd` (default: forward + a
// little up in the parent's frame) and attack info `atk` (default emTreeAtk: 200 range, 400
// damage); the trunk is laid horizontal along its path.
void cEmTree::setShot(Vec* spd, EmAtkInfo* atk)
{
    EmTreeWork* w = EMTREE_WK(this);
    Vec v;
    Mtx m;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        }
    }
    PSMTXMultVecSR(mat, &v, &v);
    w->spd = v;
    ang.x = 0.0f;
    ang.y = atan2f(v.x, v.z);
    ang.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &ang);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(m, mat, mat);
    TransMatrix(mat, &pos);
    pos_old = pos;
    w->pParent = 0;
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emTreeAtk;
    }
    r_no_0 = 1;
    r_no_1 = 6;
    r_no_2 = 0;
    r_no_3 = 0;
}

// 1 while the tree has not been caught yet (setCatch not called).
int cEmTree::ckCatch()
{
    return EMTREE_WK(this)->caught == 0;
}

// Marks the tree as caught (El Gigante grabbed it).
void cEmTree::setCatch()
{
    EMTREE_WK(this)->caught = 1;
}

// Script entry: hides the tree and removes it (Rno1 2).
void cEmTree::setLost()
{
    be_flag &= ~2;
    atari.m_flag &= ~0x200;
    hp = 0;
    r_no_0 = 1;
    r_no_1 = 2;
    r_no_2 = 0;
    r_no_3 = 0;
}
