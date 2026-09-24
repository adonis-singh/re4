// game/emshield.cpp: shield enemy (cEmShield): a wooden shield carried by an enemy that loses its
// planks when shot and falls to the ground as a three-node rope.
//
// Byte-identical.

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "emshield.h"
#include "at_mod.h"
#include "player.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "motion.h"
#include "em_sub.h"
#include "est.h"

typedef void (*EmShieldFunc)(cEmShield*);

static EmShieldFunc EmShield_R0_move_tbl[4] = {
    emShield_R0_Init,
    emShield_R0_Move,
    0,
    0,
};

static EmShieldFunc EmShield_R1_move_tbl[5] = {
    emShield_R1_Set,
    emShield_R1_LostWait,
    emShield_R1_Lost,
    emShield_R1_Parent,
    emShield_R1_Fall,
};

// Creates a shield enemy (id 0x50, at the back of the pool) from a model / TPL: a body hit cube
// plus 9 plank hit boxes (parts 2..10), 1000 hp, unlockable and ignored by Ashley, SE / effect
// ids cleared, random plank hit count. Starts in Rno1 0 Set. NULL on failure.
cEmShield* SetShield(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cEmShield* em;
    EmShieldWork* w;

    em = (cEmShield*) EmMgr.createBack(0x50);
    if (em == 0) {
        return 0;
    }
    w = EMSHIELD_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetWeapon() ModelInit failed.");
        EmMgr.destroy(em);
        return 0;
    }
    int no = 0;
    YarareInitCube(em, 0.0f, -50.0f, -100.0f, 200.0f, 100.0f, 300.0f, no, no);
    YarareAddCube(em, &w->hit[0], 0.0f, 0.0f, -70.0f, 100.0f, 90.0f, 300.0f, 2, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[1], 0.0f, 0.0f, 100.0f, 150.0f, 90.0f, 300.0f, 3, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 100.0f, 90.0f, 300.0f, 4, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 100.0f, 90.0f, 300.0f, 5, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 150.0f, 90.0f, 200.0f, 6, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[5], 0.0f, 0.0f, -75.0f, 100.0f, 90.0f, 300.0f, 7, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 150.0f, 90.0f, 250.0f, 8, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[7], 0.0f, 0.0f, 0.0f, 150.0f, 90.0f, 250.0f, 9, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[8], -50.0f, 0.0f, 0.0f, 150.0f, 90.0f, 250.0f, 10, YAT_FLAG_ON);
    em->atari.init(0.0f, 0.0f, 0.0f, 150.0f, 150.0f, 150.0f, 300.0f, 1, 0x2000, 10);
    em->hp_max = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->setTarget(0, 0.0f, 0.0f, 0.0f);
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->be_flag &= ~0x01000000;
    em->atari.setPriority(PRI_LV3);
    em->atari.off();
    w->Be_flg = 0;
    em->be_flag &= ~0x10;
    w->Fall_wait = 0;
    w->inWater = 0;
    w->Break_num = 0;
    w->pParent = 0;
    w->pOldParent = 0;
    w->xA8 = 0;
    w->x38 = -1;
    w->Parts_hp = (Rnd() % 3) + 2;
    w->seAlwaysWait = 4;
    w->Gravity = 20.0f;
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
    w->se8F[0] = 0xFF;
    w->se8F[1] = 0xFF;
    w->se8F[2] = 0;
    w->seAlways[0] = 0xFF;
    w->seAlways[1] = 0xFF;
    w->seAlways[2] = 0;
    w->effFall[0] = 0xFF;
    w->effFall[1] = 0xFF;
    w->eff9D[0] = 0xFF;
    w->eff9D[1] = 0xFF;
    w->eff9B[0] = 0xFF;
    w->eff9B[1] = 0xFF;
    w->effWater[0] = 0xFF;
    w->effWater[1] = 0xFF;
    w->effAlways[0] = 0xFF;
    w->effAlways[1] = 0xFF;
    w->always2_parts = 0xFF;
    w->x34 = 0;
    w->always2_wait = 0;
    w->always2_timer = 0;
    w->always2_offset.x = 0.0f;
    w->always2_offset.y = 0.0f;
    w->always2_offset.z = 0.0f;
    w->estNo = 50;
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    emShield_R0_Move(em);
    return em;
}

// Event start: a shield nobody carries is destroyed.
void cEmShield::beginEvent(u32 flag)
{
    if (EMSHIELD_WK(this)->pParent == 0) {
        EmMgr.destroy(this);
    }
}

// Damage: every few hits a plank (hit box parts 2..10) breaks off; the fourth plank, an explosion
// or a heavy weapon destroys the shield.
// Byte-identical. Shape notes: the compare tree needs the default-labelled members 0..4, 0xB, 0xE,
// 0x10, 0x11, 0x14, 0x1B, 0x1D, 0x26, 0x27, 0x2B (tools/research/casetree.py: [0,4] and [16,17] only add
// balance weight, their compares are jump-threaded away); the two plank bodies (default arm / B arm)
// stay separate copies only because they use different pointer variables (`parts0` = the top
// getPartsPtr(0) variable, also the breakAll/D one, so it crosses calls and is callee-saved r31;
// `parts` in B, `parts2` in the C continuation), the C arm falls through into D with `goto plank`
// for its own body laid out after D, and `!(rad < K)` gives the plain `bge`. The D arm's SndCall
// goes through a do-while(0) + void-returning alias: the loop notes give `&parts0->worldPos` a 5th
// weighted ref (global-alloc priority above `w`: r26/r25) and the void result keeps `li r3,8`
// ahead of the other argument `li`s inside the notes (u32 SndCall issues it last there).
void SndCallV(u16, u16, Vec*, int, int, cUnit*) asm("SndCall__FUsUsP3VeciiP5cUnit");
// Weapon hit reaction (see the note above): plays the shield hit SE on the carrier, and by weapon
// class either counts hits toward knocking off the hit plank (est 0x10/0x61, or 0x63 for parts 5,
// the plank is scaled to 0 and its hit box disabled), breaks the whole shield on the fourth plank
// or a heavy / explosive weapon (est 0x10/0x62, hp 0, Rno1 2 Lost), or (shotguns) decides by hit
// distance; a body hit only spawns blood.
void emShieldDmCk(cEmShield* pEm)
{
    EmShieldWork* w = EMSHIELD_WK(pEm);
    Vec p;
    Vec r;
    u8 wep;
    YARARE_INFO* part;
    cParts* parts;
    cParts* parts0;
    cParts* parts2;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    wep = pEm->dmg.m_Wep;
    pEm->dmg.m_Flag = 0;
    pEm->dmg.m_Timer = 1;
    part = pEm->dmg.m_pDamageYarare;
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
    parts0 = pEm->getPartsPtr(0);
    if (w->pParent) {
        SndCall(8, 0xAC, &parts0->world, w->pParent->id, 0, pEm);
    }
    switch (pEm->dmg.m_Wep) {
    default:
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (part->parts_no == 0) {
            goto blood;
        }
        w->Parts_hp--;
        if (w->Parts_hp > 0) {
            goto blood;
        }
        w->Break_num++;
        w->Parts_hp = (Rnd() % 3) + 2;
        if (w->Break_num > 3) {
            goto breakAll;
        }
        parts0 = pEm->getPartsPtr(part->parts_no - 1);
        p = parts0->world;
        Matrix2AxisAngle(parts0->mat, &r);
        if (part->parts_no == 5) {
            EstSet(0, -1, &p, &r, EFF_EM10, 0x63, 0, ESP_CORE_KIND_NONE, 0, 0);
        } else {
            EstSet(0, -1, &p, &r, EFF_EM10, 0x61, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        if (w->pParent) {
            SndCall(8, 0xAD, &parts0->world, w->pParent->id, 0, pEm);
        }
        parts0->scale.x = 0.0f;
        parts0->scale.y = 0.0f;
        parts0->scale.z = 0.0f;
        part->flag &= ~1;
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xF:
    case 0x28:
    case 0x2C:
        if (part->parts_no == 0) {
            goto blood;
        }
        w->Break_num++;
        w->Parts_hp = (Rnd() % 3) + 2;
        if (w->Break_num > 3) {
        breakAll:
            parts0 = pEm->getPartsPtr(0);
            p = parts0->world;
            Matrix2AxisAngle(parts0->mat, &r);
            EstSet(0, -1, &p, &r, EFF_EM10, 0x62, 0, ESP_CORE_KIND_NONE, 0, 0);
            if (w->pParent) {
                SndCall(8, 0xAE, &parts0->world, w->pParent->id, 0, pEm);
            }
            pEm->hp = 0;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 2;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            break;
        }
        parts = pEm->getPartsPtr(part->parts_no - 1);
        p = parts->world;
        Matrix2AxisAngle(parts->mat, &r);
        if (part->parts_no == 5) {
            EstSet(0, -1, &p, &r, EFF_EM10, 0x63, 0, ESP_CORE_KIND_NONE, 0, 0);
        } else {
            EstSet(0, -1, &p, &r, EFF_EM10, 0x61, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        if (w->pParent) {
            SndCall(8, 0xAD, &parts->world, w->pParent->id, 0, pEm);
        }
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        part->flag &= ~1;
        break;
    blood:
        EmDmBloodSet2(pEm, 0x10, 0x60, 0, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (part->len > 64000000.0f) {
            break;
        }
        if (w->Break_num <= 3 && !(part->len < 12250000.0f)) {
            goto plank;
        }
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x1C:
    case 0x29:
    case 0x2D:
        parts0 = pEm->getPartsPtr(0);
        p = parts0->world;
        Matrix2AxisAngle(parts0->mat, &r);
        EstSet(0, -1, &p, &r, EFF_EM10, 0x62, 0, ESP_CORE_KIND_NONE, 0, 0);
        pEm->hp = 0;
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 2;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
        if (w->pParent) {
            do {
                SndCallV(8, 0xAE, &parts0->world, w->pParent->id, 0, pEm);
            } while (0);
        }
        break;
    plank:
        if (part->parts_no == 0) {
            break;
        }
        parts2 = pEm->getPartsPtr(part->parts_no - 1);
        p = parts2->world;
        Matrix2AxisAngle(parts2->mat, &r);
        if (part->parts_no == 5) {
            EstSet(0, -1, &p, &r, EFF_EM10, 0x63, 0, ESP_CORE_KIND_NONE, 0, 0);
        } else {
            EstSet(0, -1, &p, &r, EFF_EM10, 0x61, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        w->Parts_hp -= 3;
        if (w->Parts_hp > 0) {
            break;
        }
        w->Break_num++;
        parts2->scale.x = 0.0f;
        parts2->scale.y = 0.0f;
        parts2->scale.z = 0.0f;
        part->flag &= ~1;
        w->Parts_hp = (Rnd() % 3) + 2;
        if (w->pParent) {
            SndCall(8, 0xAD, &pEm->getPartsPtr(0)->world, w->pParent->id, 0, pEm);
        }
        break;
    }
}

// Per-frame: damage check and the Rno0 routine, then mirrors the carrier's visibility and fade
// factors, fires the periodic "always" est on its parts, dies with the carrier's hp and is
// destroyed when the carrier work is gone.
void cEmShield::move()
{
    EmShieldWork* w = EMSHIELD_WK(this);
    Vec p;

    emShieldDmCk(this);
    EmShield_R0_move_tbl[r_no_0](this);
    if (isAlive()) {
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
        if ((be_flag & 2) && w->effAlways[0] != 0xFF && w->effAlways[1] != 0xFF && w->always2_timer != 0) {
            s16 t = --w->always2_timer;

            if (t == 0) {
                PSMTXMultVec(getPartsPtr(w->always2_parts)->mat, &w->always2_offset, &p);
                EstSet(0, -1, &p, 0, w->effAlways[0], w->effAlways[1], 0, ESP_CORE_KIND_NONE, 0, 0);
                w->always2_timer = w->always2_wait;
            }
        }
        if (w->pParent) {
            if (((cEm*) w->pParent)->hp <= 0) {
                hp = 0;
            }
            if (w->pParent && !w->pParent->isAlive()) {
                EmMgr.destroy(this);
            }
        }
    }
}

// Rno0 == 0: resets to the Set state.
void emShield_R0_Init(cEmShield* pEm)
{
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 0;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 LostWait, 2 Lost, 3 Parent, 4 Fall).
void emShield_R0_Move(cEmShield* pEm)
{
    EmShield_R1_move_tbl[pEm->r_no_1](pEm);
}

// Rno1 == 0: free-standing shield; plays its motion or rebuilds the matrices from pos / ang.
void emShield_R1_Set(cEmShield* pEm)
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

// Rno1 == 1: a dropped shield at rest; after 90 frames fades out (invisible_factor -0.1 / frame),
// or at once when it leaves the screen, then Rno1 2.
void emShield_R1_LostWait(cEmShield* pEm)
{
    EmShieldWork* w = EMSHIELD_WK(pEm);
    Vec scr;
    Vec pos;

    switch (pEm->r_no_2) {
    case 0:
        pEm->setStatus(EM_STATUS_LOCKOFF);
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

// Rno1 == 2: removes the shield: hides it, deletes its effects (Core_kind estNo) and destroys the
// work.
void emShield_R1_Lost(cEmShield* pEm)
{
    EmShieldWork* w = EMSHIELD_WK(pEm);

    if (pEm->r_no_2 == 0) {
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        pEm->be_flag &= ~0x20;
        pEm->setStatus(EM_STATUS_LOCKOFF);
        EffectEspDelete(0, w->estNo, pEm, 0);
        EffectEspgenDelete(0, w->estNo, pEm);
        EffectEfmDelete(0, w->estNo, pEm);
        pEm->r_no_2++;
        EmMgr.destroy(pEm);
    }
}

// Rno1 == 3: carried: follows parts `partsNo` of the carrier (rotation re-normalised unless
// Be_flg bit0), plays its own motion when it has one, and counts Fall_wait down to setFall.
void emShield_R1_Parent(cEmShield* pEm)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmShieldWork* w = EMSHIELD_WK(pEm);
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
#line 762 "D:/Bio4/Prog/emshield.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 764 "D:/Bio4/Prog/emshield.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 766 "D:/Bio4/Prog/emshield.cpp"
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
            pEm->setFall(0, 20.0f);
        }
    }
}

// Rno1 == 4: the dropped shield tumbles as a 3-node rope: gravity, 30 relaxation passes of the
// edge constraints, floor contact with the landing SE / est and effect deletion, random bounce
// damping; the matrix is rebuilt from the three nodes. Comes to rest (Rno1 1) when the node
// speeds are small; entering water spawns the water est and SE once.
void emShield_R1_Fall(cEmShield* pEm)
{
    EmShieldWork* w = EMSHIELD_WK(pEm);
    Vec pt[3] = {
        { -200.0f, 30.0f, 500.0f },
        { -200.0f, 30.0f, -800.0f },
        { 500.0f, 30.0f, 0.0f },
    };
    EmTreeNode node[3];
    Vec b;
    Vec c;
    Vec a;
    Vec tmp;
    EmTreeNode* n;    // shared by every node loop (emtree emTree_R1_Fall: the giv final-value copy)
    EmTreeNode* nx;
    f32 floor;
    u32 i;
    u32 k;
    f32 mag;
    f32 d;
    cParts* parts0;

    pEm->hp = 0;
    pEm->setStatus(EM_STATUS_LOCKOFF);
    floor = EatMgr.getFloor(&pEm->pos, 0, 600.0f, 100000.0f, 0) + 80.0f;
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
        n->spd.y -= w->Gravity;
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
                if (w->inWater == 0 && w->pOldParent) {
                    parts0 = pEm->getPartsPtr(0);
                    SndCall(8, 0xAF, &parts0->world, w->pOldParent->id, 0, pEm);
                }
                if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                    EstSet(pEm, -1, 0, 0, w->effFall[0], w->effFall[1], 0, ESP_CORE_KIND_NONE, pEm, 0);
                }
            }
            EffectEspDelete(0, w->estNo, pEm, 0);
            EffectEspgenDelete(0, w->estNo, pEm);
            EffectEfmDelete(0, w->estNo, pEm);
            n->spd.x *= fRand0_1() * 0.2f + 0.5f;
            n->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
            n->spd.z *= fRand0_1() * 0.2f + 0.5f;
            if (n->spd.y <= w->Gravity) {
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
    PSVECCrossProduct(&a, &b, &c);
    PSVECCrossProduct(&c, &a, &b);
#line 973 "D:/Bio4/Prog/emshield.cpp"
    VECNormalize(&b, &b);
#line 974 "D:/Bio4/Prog/emshield.cpp"
    VECNormalize(&c, &c);
#line 975 "D:/Bio4/Prog/emshield.cpp"
    VECNormalize(&a, &a);
    pEm->mat[0][0] = b.x;
    pEm->mat[1][0] = b.y;
    pEm->mat[2][0] = b.z;
    pEm->mat[0][1] = c.x;
    pEm->mat[1][1] = c.y;
    pEm->mat[2][1] = c.z;
    pEm->mat[0][2] = a.x;
    pEm->mat[1][2] = a.y;
    pEm->mat[2][2] = a.z;
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
    if (w->inWater == 0) {
        parts0 = pEm->getPartsPtr(0);
        if (CheckInWater(pEm, 0)) {
            if (w->effWater[0] != 0xFF && w->effWater[1] != 0xFF) {
                EstSet(0, -1, &pEm->pos, 0, w->effWater[0], w->effWater[1], 0, ESP_CORE_KIND_NONE, 0, 0);
            }
            SndCall(6, 0x17, &parts0->world, 0, 0, pEm);
            w->inWater = 1;
        }
    }
}

// Hands the shield to `parent` parts `partsNo` (Rno1 3); flag skips the matrix normalisation.
void cEmShield::setParent(cModel* parent, int partsNo, int flag)
{
    EmShieldWork* w = EMSHIELD_WK(this);

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
}

// Drops the shield (Rno1 4): remembers the carrier as pOldParent, gives the three rope nodes the
// initial speed `spd` (rotated +-90 degrees for nodes 1 / 2) or random speeds, stops the motion.
// Drops the shield: node speeds from `spd` (node 0 as is, nodes 1 / 2 rotated +-90 degrees around Y)
// or random when NULL.
void cEmShield::setFall(Vec* pSpd, f32 gravity)
{
    EmShieldWork* w = EMSHIELD_WK(this);
    Mtx m;
    Vec v;
    u32 i;

    Motion.pMot = 0;
    for (i = 0; i < 3; i++) {
        if (pSpd) {
            f32 ang;

            switch (i) {
            case 0:
            default:
                w->pt[i].x = pSpd->x;
                w->pt[i].y = pSpd->y;
                w->pt[i].z = pSpd->z;
                break;
            case 1:
                if (pSpd->x == 0.0f && pSpd->z == 0.0f) {
                    ang = 0.0f;
                } else {
                    ang = atan2f(pSpd->x, pSpd->z);
                }
                PSMTXRotRad(m, 'y', ang + 1.5707964f);
                PSMTXMultVec(m, pSpd, &v);
                w->pt[i].x = v.x;
                w->pt[i].y = v.y;
                w->pt[i].z = v.z;
                break;
            case 2:
                if (pSpd->x == 0.0f && pSpd->z == 0.0f) {
                    ang = 0.0f;
                } else {
                    ang = atan2f(pSpd->x, pSpd->z);
                }
                PSMTXRotRad(m, 'y', ang - 1.5707964f);
                PSMTXMultVec(m, pSpd, &v);
                w->pt[i].x = v.x;
                w->pt[i].y = v.y;
                w->pt[i].z = v.z;
                break;
            }
        } else {
            w->pt[i].x = fRand1_1() * 20.0f;
            w->pt[i].y = fRand1_1() * 20.0f;
            w->pt[i].z = fRand1_1() * 20.0f;
        }
    }
    w->pOldParent = w->pParent;
    w->pParent = 0;
    hp = 0;
    w->Gravity = gravity;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &this->ang);
    r_no_0 = 1;
    r_no_1 = 4;
    r_no_2 = 0;
    r_no_3 = 0;
}
