// game/emwep.cpp: weapon enemy (cEmWep): the weapons the enemies hold (setParent), drop (setFall,
// a three-node rope), throw (axes, scythes, dynamite, grenades) or shoot (arrows, rockets) at the
// player, with the player's escape routines of the grenade.
//

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "atari_init.h"
#include "emwep.h"
#include "at_mod.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "quake.h"
#include "pad.h"
#include "main.h"
#include "act_btn.h"
#include "cam_ctrl.h"
#include "sce_at.h"
#include "obj.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);
int EmAtkHitCk(void* info, Vec* pPos, Vec* pPosOld, int flag);                                        // em_sub.cpp
YARARE_INFO* EmAtkLineHitCkSub(Vec* pPos, Vec* pPos2, Vec* hit, Vec* nrm);                            // em_sub.cpp
void EmAtkSetDamageSub(YARARE_INFO* part, EmAtkInfo* info, Vec* pPos, Vec* pPos2);                     // em_sub.cpp
YARARE_INFO* emLineAtCk(cEm* em, Vec* pPos, Vec* pPos2, f32 len, int flag);                           // em_sub.cpp
int CheckInWater(cModel* m, int parts_no);                                                          // em_sub.cpp
void GameAddPoint(int no);                                                                   // game.cpp
static void emWep_R1_Parent(cEmWep* em);
// The original is a `static plemEscape` (emBar.cpp has a global one); the name carries the split's
// address suffix in sym_map.
#define plemEscape plemEscape_80017688
static void plemEscape(cPlayer* pl);
}
void MotionSetCore(cModel* m, void* w, void* data, int seq, int hokan, int flags, int frame);   // motion.cpp (C++ linkage)
cObj* SetObj01(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* spd, f32 grav, f32 rad, int life, int flags);   // obj01.cpp
void Obj01SetEst(cObj* obj, int no0, int prm0, u32 type, int no1, int prm1, int no2, int prm2, int no3, int prm3);


// setYarareCube(0, x, y, z) with the float arguments' moves issued before the `li r4, 0`
// (atari_init.h: GCC emits the argument moves in declaration order).
void setYarareCubeF(cEmWep* em, f32 x, f32 y, f32 z, Vec* size) asm("setYarareCube__6cEmWepP3Vecfff");

// One rope node of the falling weapon (emWep_R1_Fall): three point masses joined by distance
// constraints; the model matrix is rebuilt from them every frame.
struct EmWepNode {
    Vec pos;      // 0x00
    Vec old;      // 0x0C
    Vec spd;      // 0x18
    f32 len;      // 0x24
    int onFloor;  // 0x28
};

// lockParts = 0 through an int parameter: the zero becomes an SImode pseudo shared with the
// later `= 0` stores (emrock SetRock).
static inline void LockPartsSet(cEm* em, int no)
{
    em->lockParts = no;
}

typedef void (*EmWepFunc)(cEmWep*);

EmWepFunc EmWep_R0_move_tbl[4] = {
    emWep_R0_Init,
    emWep_R0_Move,
    0,
    0,
};

EmWepFunc EmWep_R1_move_tbl[13] = {
    emWep_R1_Set,
    emWep_R1_LostWait,
    emWep_R1_Lost,
    emWep_R1_Parent,
    emWep_R1_Fall,
    emWep_R1_Throw,
    emWep_R1_Shot,
    emWep_R1_ShotArrow,
    emWep_R1_Rocket,
    emWep_R1_BombThrow,
    emWep_R1_ThrowScythe,
    emWep_R1_FlashThrow,
    emWep_R1_GrenadeThrow,
};

EmAtkInfo emWepAtk = { 200.0f, 8, 400, 0, 10, 0 };

// Cloth chain of the whip-like weapons (setCloth): parts per link and the neighbour tables.
u8 emWepClothP[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
u8 emWepClothUp[10] = { 0xFF, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static u8 emWepClothDp[10] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 0xFF };
f32 emWepClothMax[10] = { 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f };
CLOTH_AT_SET emWepAt[3] = {
    { 0, 4, 4, 1.0f, 200.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 3, 3, 0.5f, 250.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 2, 2, 1.0f, 250.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};

// Creates a weapon enemy (id 0x42, at the back of the pool) from a model / TPL at pos / rot: the
// hand weapons, projectiles and thrown items of the enemies. type 1 marks a weapon that vanishes
// when hidden (Set). No hp (not shootable unless setYarare), a small solid atari, all SE / effect
// ids cleared, Core_kind 50 for its effects. Starts in Rno1 0 Set. NULL on failure.
cEmWep* SetWeapon(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type)
{
    cEmWep* em;
    EmWepWork* w;

    em = (cEmWep*) EmMgr.createBack(0x42);
    if (em == 0) {
        return 0;
    }
    w = EMWEP_WK(em);
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
    em->type = type;
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 150.0f, 150.0f, 150.0f, 300.0f, 1, 0x2000, 10);
    em->hp = 0;
    em->hp_max = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        if (em->type == 1) {
            em->LightInfo.init2(0, 1, &ofs, &size, 0x20);
        } else {
            em->LightInfo.init2(0, 1, &ofs, &size, 2);
        }
    }
    LockPartsSet(em, 0);
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->be_flag &= ~0x01000000;
    em->atari.setPriority(PRI_LV3);
    em->atari.throughOn();
    em->be_flag &= ~0x10;
    w->At_no = -1;
    w->seThrow[3] = 4;
    w->grav = 20.0f;
    w->seFall[0] = 0xFF;
    w->seFall[1] = 0xFF;
    w->seHit[0] = 0xFF;
    w->seHit[1] = 0xFF;
    w->seHitWall[0] = 0xFF;
    w->seHitWall[1] = 0xFF;
    w->seDamage[0] = 0xFF;
    w->seDamage[1] = 0xFF;
    w->seThrow[0] = 0xFF;
    w->seThrow[1] = 0xFF;
    w->Be_flg = 0;
    w->timer4 = 0;
    w->Water_ck = 0;
    w->pEm_oya = 0;
    w->pEm_old = 0;
    w->pAtk = 0;
    w->fall_type = 0;
    w->seFall[2] = 0;
    w->seFall[3] = 0;
    w->seHit[2] = 0;
    w->seHitWall[2] = 0;
    w->seDamage[2] = 0;
    w->seThrow[2] = 0;
    w->alwaysTimer = 0;
    w->seid_throw = 0;
    w->effFall[0] = 0xFF;
    w->espKind = 50;
    w->effFall[1] = 0xFF;
    w->effDamage[0] = 0xFF;
    w->effDamage[1] = 0xFF;
    w->effHit[0] = 0xFF;
    w->effHit[1] = 0xFF;
    w->eff_id_always2[0] = 0xFF;
    w->eff_id_always2[1] = 0xFF;
    w->effAlways[0] = 0xFF;
    w->effAlways[1] = 0xFF;
    w->always2_parts = 0xFF;
    w->effAlwaysWait = 0;
    w->effAlwaysTimer = 0;
    w->always2_offset.x = 0.0f;
    w->always2_offset.y = 0.0f;
    w->Mot_escape = 0;
    w->motBackjump = 0;
    w->motFront = 0;
    w->motEscape2 = 0;
    w->always2_offset.z = 0.0f;
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
    emWep_R0_Move(em);
    return em;
}

// Event start: an unowned type 0 weapon lying around is removed.
void cEmWep::beginEvent()
{
    if (EMWEP_WK(this)->pEm_oya == 0 && type == 0) {
        EmMgr.destroy(this);
    }
}

// The weapon was shot (setYarare weapons, not by knife / grenades): a carried / falling weapon is
// knocked out of the hand (setFall away from the shooter) with the damage SE / est; a thrown
// dynamite (Rno1 9) explodes early (blast est, SE, damage on the thrower); a flying grenade
// (Rno1 0xC) is shot down: point bonus, explosion at its position, Status_flg[1] 0x20000; any
// scenario attribute tied to the weapon (At_no) is destroyed.
void emWepDmCk(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    u8 wep;
    u8 stat;
    int one;
    Vec p;
    Mtx m;
    Vec v;
    Vec r;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
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
    em->setStatus(EM_STATUS_LOCKOFF);
    em->hp = 0;
    switch (em->r_no_1) {
    case 3:
    case 0xA:
    default:
        PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &em->dmg.m_PosFrom));
        v.x = fRand1_1() * 200.0f;
        v.y = fRand0_1() * 50.0f + 50.0f;
        v.z = fRand0_1() * 100.0f + -250.0f;
        PSMTXMultVecSR(m, &v, &v);
        em->setFall(0, &v, 20.0f);
        if (w->seDamage[0] != 0xFF) {
            SndCall(w->seDamage[0], w->seDamage[1], &em->pos, w->seDamage[2], 0, em);
        }
        if (w->effDamage[0] != 0xFF && w->effDamage[1] != 0xFF) {
            EstSet(0, -1, &em->pos, &em->ang, w->effDamage[0], w->effDamage[1], 0, 0, 0, 0);
        }
        StaFlagOn(pG, STA_CRITICAL);
        GameAddPoint(9);
        break;
    case 7:
        StaFlagOn(pG, STA_CRITICAL);
        GameAddPoint(9);
        emWepArrowBomb(em);
        break;
    case 8:
        StaFlagOn(pG, STA_CRITICAL);
        GameAddPoint(9);
        emWepRocketBobm(em);
        break;
    case 9:
        stat = 1;
        StaFlagOn(pG, STA_CRITICAL);
        GameAddPoint(9);
        r.x = 0.0f;
        r.y = GetXZAngle(&em->pos, &pG->Cam.param.pos);
        r.z = 0.0f;
        EstSet(0, -1, &em->pos, &r, 0x10, 0x42, 0, 0, 0, 0);
        if (w->pEm_old) {
            SndCall(8, 0x96, &em->pos, w->pEm_old->id, 0, em);
        }
        StaFlagOn(pG, STA_PL_FIRE);
        p = em->pos;
        p.y += 800.0f;
        PlWepHitCheck2(0, &p, &p, 0x13, 3, 5000.0f);
        StaFlagOn(pG, STA_SE_BURST);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
        pG->bell_stat = stat;
        em->setLost();
        break;
    case 0xC:
        StaFlagOn(pG, STA_CRITICAL);
        GameAddPoint(9);
        EstSet(0, -1, &em->pos, 0, 0, 0xD, 0, 0, 0, 0);
        EstSet(0, -1, &em->pos, 0, 0, 0x1A, 0, 0, 0, 0);
        one = 1;
        SndCall(one, 0x14, &em->pos, 0, 0, em);
        StaFlagOn(pG, STA_PL_FIRE);
        p = em->pos;
        p.y += 800.0f;
        PlWepHitCheck2(0, &p, &p, 0x13, 3, 5000.0f);
        StaFlagOn(pG, STA_SE_BURST);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
        pG->bell_stat = one;
        em->setLost();
        break;
    case 0:
    case 2:
        if (w->effDamage[0] != 0xFF && w->effDamage[1] != 0xFF) {
            EstSet(0, -1, &em->pos, &em->ang, w->effDamage[0], w->effDamage[1], 0, 0, 0, 0);
        }
        em->r_no_0 = 1;
        em->r_no_1 = 2;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        if (w->seDamage[0] != 0xFF) {
            SndCall(w->seDamage[0], w->seDamage[1], &em->pos, w->seDamage[2], 0, em);
        }
        if (w->At_no != -1) {
            SceAtDestroy(w->At_no);
            w->At_no = -1;
        }
        break;
    }
}

// Per-frame: hit check, the Rno0 routine, then while live: the cloth chain, mirroring the
// holder's visibility / fade (Be_flg bit1 hides), the periodic "always" est and SE, and
// destruction when the holder's work vanished.
void cEmWep::move()
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;

    emWepDmCk(this);
    EmWep_R0_move_tbl[r_no_0](this);
    if ((be_flag & 0x201) != 1) {
        return;
    }
    moveCloth();
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
    if (be_flag & 2) {
        if (w->effAlways[0] != 0xFF && w->effAlways[1] != 0xFF && w->effAlwaysTimer != 0) {
            w->effAlwaysTimer--;
            if ((s16) w->effAlwaysTimer == 0) {
                PSMTXMultVec(getPartsPtr(w->always2_parts)->mat, &w->always2_offset, &v);
                EstSet(0, -1, &v, 0, w->effAlways[0], w->effAlways[1], 0, 0, 0, 0);
                w->effAlwaysTimer = w->effAlwaysWait;
            }
        }
        if (w->alwaysTimer) {
            w->alwaysTimer--;
            if (w->alwaysTimer == 0) {
                cModel* p = getPartsPtr(0);

                w->alwaysTimer = w->alwaysWait;
                SndCall(w->seAlways[0], w->seAlways[1], &p->world, w->seAlways[2], 0, this);
            }
        }
    }
    if (w->pEm_oya && (w->pEm_oya->be_flag & 0x201) != 1) {
        EmMgr.destroy(this);
    }
}

// Rno0 == 0: resets to the Set state.
void emWep_R0_Init(cEmWep* em)
{
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 LostWait, 2 Lost, 3 Parent, 4 Fall, 5 Throw, 6 Shot,
// 7 ShotArrow, 8 Rocket, 9 BombThrow, 0xA ThrowScythe, 0xB FlashThrow, 0xC GrenadeThrow).
void emWep_R0_Move(cEmWep* em)
{
    EmWep_R1_move_tbl[em->r_no_1](em);
}

// Rno1 == 0: a free weapon lying in the room; plays its motion or rebuilds the matrices (type 1
// freezes its matrix after 3 frames and is lost as soon as it is hidden).
void emWep_R1_Set(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);

    switch (em->r_no_2) {
    case 0:
        w->Timer = 3;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        }
        if (em->pMotion) {
            MotionMove(em, 0);
        } else {
            if (em->type != 1 || w->Timer != 0) {
                RotMatrix(em->mat, &em->ang);
                TransMatrix(em->mat, &em->pos);
                ScaleMatrix(em->mat, &em->scale);
            }
            em->partsMatCalc();
        }
        break;
    }
    em->partsWorldCalc();
    if (em->type == 1 && !(em->be_flag & 2)) {
        em->hp = 0;
        em->r_no_0 = 1;
        em->r_no_1 = 2;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    }
}

// Rno1 == 1: a dropped weapon at rest; after 90 frames (1 for type 1), or at once off screen,
// fades out and goes to Lost.
void emWep_R1_LostWait(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec scr;
    Vec pos;

    switch (em->r_no_2) {
    case 0:
        em->setStatus(EM_STATUS_LOCKOFF);
        if (pG->room_id == 0x30F) {
            w->Timer = 1;
        } else {
            w->Timer = 90;
        }
        em->r_no_2++;
    case 1:
        if (w->Timer == 0) {
            em->invisible_factor -= 0.1f;
            if (em->invisible_factor <= 0.0f) {
                em->invisible_factor = 0.0f;
                em->r_no_0 = 1;
                em->r_no_1 = 2;
                em->r_no_2 = 0;
                em->r_no_3 = 0;
                break;
            }
        } else {
            w->Timer--;
        }
        pos = em->pos;
        GetScreenPos(&pos, &scr);
        if (scr.z > 1.0f) {
            em->r_no_0 = 1;
            em->r_no_1 = 2;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// Rno1 == 2: removes the weapon: hidden, its effects (Core_kind espKind) deleted, work destroyed.
void emWep_R1_Lost(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->hp = 0;
        em->be_flag &= ~2;
        em->be_flag &= ~0x20;
        em->setStatus(EM_STATUS_LOCKOFF);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        em->r_no_2++;
        EmMgr.destroy(em);
        break;
    }
}

// Rno1 == 3: held by pEm_oya: follows its parts (setParentMatCalc) and drops after timer4 frames
// (setWaitDrop).
static void emWep_R1_Parent(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);

    em->setParentMatCalc(0);
    if (w->timer4) {
        w->timer4--;
        if (w->timer4 == 0) {
            em->setFall(0, 0, 20.0f);
        }
    }
}

// Rno1 == 4: the dropped weapon tumbles as a 3-node rope (node offsets by fall_type: long,
// pole, small, flat): gravity `grav`, 30 relaxation passes, floor contact with the landing SE /
// est and random bounce damping, matrix rebuilt from the nodes; rests (LostWait) when the node
// speeds are small; a water entry plays the splash est / SE once.
void emWep_R1_Fall(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec ofs[4][3] = {
        { { 0.0f, 0.0f, 600.0f }, { 0.0f, 0.0f, -600.0f }, { 300.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 1500.0f }, { 0.0f, 0.0f, 0.0f }, { 300.0f, 0.0f, 1300.0f } },
        { { -140.0f, 60.0f, 140.0f }, { -140.0f, 60.0f, -140.0f }, { 200.0f, 60.0f, 0.0f } },
        { { -140.0f, 30.0f, 140.0f }, { -140.0f, 30.0f, -140.0f }, { 200.0f, 30.0f, 0.0f } },
    };
    EmWepNode node[3];
    // one pointer shared by every node loop (emtree emTree_R1_Fall): the later mentions keep the
    // k-body loop's giv from being marked replaceable, loop.c emits its final value `&node[2]`
    // after that loop and cse2 makes the last loop's bound a copy of it (`mr r25, r0`)
    EmWepNode* n;
    EmWepNode* nx;
    Vec b;
    Vec c;
    Vec a;
    Vec tmp;
    f32 floor;
    u32 i;
    u32 k;
    f32 mag;
    f32 d;

    em->hp = 0;
    em->setStatus(EM_STATUS_LOCKOFF);
    floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 50.0f;
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.x = w->pt[i].x;
        n->spd.y = w->pt[i].y;
        n->spd.z = w->pt[i].z;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        PSMTXMultVec(em->mat, &ofs[w->fall_type][i], &n->pos);
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
        n->spd.y -= w->grav;
        PSVECAdd(&n->pos, &n->spd, &n->pos);
        n->onFloor = 0;
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
                n->onFloor = 1;
            }
            if (nx->pos.y < floor) {
                nx->pos.y = floor;
                nx->onFloor = 1;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        // dead in this loop (flow deletes it) but its `i == 2` compare is what makes loop.c put the
        // loop bound in the preheader instead of rematerialising `&node[2]` at the bottom
        if (i == 2) {
            nx = node;
        } else {
            nx = &node[i + 1];
        }
        if (n->onFloor) {
            if (w->seFall[3] == 0 && n->spd.y < -50.0f) {
                w->seFall[3] = 1;
                if (w->seFall[0] != 0xFF && w->Water_ck == 0) {
                    SndCall(w->seFall[0], w->seFall[1], &em->pos, w->seFall[2], 0, em);
                }
                if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                    EstSet((int) em, -1, 0, 0, w->effFall[0], w->effFall[1], 0, 0, (u32) em, 0);
                }
            }
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            switch (w->fall_type) {
            default:
                n->spd.x *= fRand0_1() * 0.2f + 0.5f;
                n->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
                n->spd.z *= fRand0_1() * 0.2f + 0.5f;
                break;
            case 2:
            case 3:
                n->spd.x *= fRand0_1() * 0.2f + 0.4f;
                n->spd.y *= -(fRand0_1() * 0.1f + 0.3f);
                n->spd.z *= fRand0_1() * 0.2f + 0.4f;
                break;
            }
            if (n->spd.y <= w->grav) {
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
#line 906 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&b, &b);
#line 907 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&c, &c);
#line 908 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&a, &a);
    em->mat[0][0] = b.x;
    em->mat[1][0] = b.y;
    em->mat[2][0] = b.z;
    em->mat[0][1] = c.x;
    em->mat[1][1] = c.y;
    em->mat[2][1] = c.z;
    em->mat[0][2] = a.x;
    em->mat[1][2] = a.y;
    em->mat[2][2] = a.z;
    PSVECScale(&ofs[w->fall_type][0], &tmp, -1.0f);
    TransMatrix(em->mat, &node[0].pos);
    PSMTXMultVec(em->mat, &tmp, &tmp);
    TransMatrix(em->mat, &tmp);
    em->pos = tmp;
    mag = node[0].spd.x * node[0].spd.x + node[0].spd.y * node[0].spd.y + node[0].spd.z * node[0].spd.z
        + node[1].spd.x * node[1].spd.x + node[1].spd.y * node[1].spd.y + node[1].spd.z * node[1].spd.z
        + node[2].spd.x * node[2].spd.x + node[2].spd.y * node[2].spd.y + node[2].spd.z * node[2].spd.z;
    if (mag < 25.0f) {
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->ang);
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    }
    em->partsWorldCalc();
    if (w->Water_ck == 0 && CheckInWater(em, 0)) {
        if (w->eff_id_always2[0] != 0xFF && w->eff_id_always2[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->eff_id_always2[0], w->eff_id_always2[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->Water_ck = 1;
    }
}

// Rno1 == 5: a thrown hand weapon (axe, sickle, hatchet): flies with gravity spinning end over
// end, looping seThrow; a scenery hit drops it (seHitWall), a player hit (EmAtkHitCk with pAtk)
// deals damage with blood, SE, quake and drops it; water entry splashes.
void emWep_R1_Throw(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Mtx m;
    Vec up;
    Vec fwd;
    f32 ang;
    cCtrl* c;

    switch (em->r_no_2) {
    case 0:
        w->Timer = 0;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->Water_ck == 0) {
                w->seid_throw = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
            }
        }
        break;
    }
    w->spd.y -= w->grav;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (EatMgr.hitCheck(&em->pos_old, &em->pos, 0, 0, 0, 0)) {
        em->setFall(0, 0, 20.0f);
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->Water_ck == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->seid_throw, 0);
    } else if (w->pAtk) {
        if (EmAtkHitCk(w->pAtk, &em->pos, &em->pos_old, 0)) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
                EmPlBloodSet2(em, &em->pos, 1, w->effHit[0], w->effHit[1]);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
            }
            em->setFall(0, 0, 20.0f);
            c = GetCtrlCtrl12();
            Ctrl12Set(c, CTRL12_ID_EM10_ATK, 0x1E);
            Ctrl12Set(c, CTRL12_ID_EM10_THROW, 0x78);
        }
    }
    PSVECSubtract(&em->pos, &em->pos_old, &d);
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
#line 1068 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&fwd, &fwd);
    ang = acosf(PSVECDotProduct(&up, &fwd));
    if (ang > 0.01f && ang < 3.1315927f) {
        PSVECCrossProduct(&up, &fwd, &up);
        PSMTXRotAxisRad(m, &up, 0.62831855f);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->Water_ck == 0 && CheckInWater(em, 0)) {
        if (w->eff_id_always2[0] != 0xFF && w->eff_id_always2[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->eff_id_always2[0], w->eff_id_always2[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->Water_ck = 1;
    }
}

// Rno1 == 0xA: the thrown scythe (Garrador / zealot): like Throw but spinning flat about its
// vertical axis; a head hit decapitates the player (emWepPlHeadLost).
void emWep_R1_ThrowScythe(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Mtx m;
    cCtrl* c;

    switch (em->r_no_2) {
    case 0:
        w->Timer = 0;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->Water_ck == 0) {
                w->seid_throw = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
            }
        }
        break;
    }
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (EatMgr.hitCheck(&em->pos_old, &em->pos, 0, 0, 0, 0)) {
        em->setFall(0, 0, 20.0f);
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->Water_ck == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->seid_throw, 0);
    } else if (w->pAtk) {
        if (EmAtkHitCk(w->pAtk, &em->pos, &em->pos_old, 0)) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            EmPlBloodSet2(em, &em->pos, 1, 0x10, 7);
            if ((s16) pG->pl_life <= 0) {
                emWepPlHeadLost();
            }
            c = GetCtrlCtrl12();
            Ctrl12Set(c, CTRL12_ID_EM10_ATK, 0x1E);
            Ctrl12Set(c, CTRL12_ID_EM10_THROW, 0x78);
        }
    }
    PSVECSubtract(&em->pos, &em->pos_old, &d);
    PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    PSMTXRotRad(m, 'y', -0.62831855f);
    PSMTXConcat(em->mat, m, em->mat);
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->Water_ck == 0 && CheckInWater(em, 0)) {
        if (w->eff_id_always2[0] != 0xFF && w->eff_id_always2[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->eff_id_always2[0], w->eff_id_always2[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->Water_ck = 1;
    }
}

// Rno1 == 6: a straight projectile (thrown knife / bolt) for at most 90 frames: a scenery hit
// (attribute 0x404000) stops it in place (rests, then drops), a player or partner hit
// (EmAtkLineHitCk / Sub) deals the pAtk damage; when the hit part is flagged 0x4000 the weapon
// stays stuck in that parts for timer4 frames (setParent) before dropping.
void emWep_R1_Shot(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec hit;
    Vec hitPos;
    Vec nrm;
    Mtx inv;
    YARARE_INFO* part;
    int no;
    f32 len;
    cCtrl* c;

    switch (em->r_no_2) {
    case 0:
        PSVECSubtract(&em->pos, &w->spd, &em->pos_old);
        w->Timer = 0;
        w->Timer2 = 90;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->Water_ck == 0) {
                w->seid_throw = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
            }
        }
        if (w->Timer2) {
            w->Timer2--;
        } else {
            em->r_no_0 = 1;
            em->r_no_1 = 2;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            return;
        }
        break;
    case 2:
        w->pEm_old = 0;
        w->timer4 = 60;
        em->hp = 0;
        em->setStatus(EM_STATUS_LOCKOFF);
        em->r_no_2++;
    case 3:
        em->partsWorldCalc();
        if (w->timer4) {
            w->timer4--;
        } else {
            em->setFall(0, 0, 20.0f);
            SndStop(w->seid_throw, 0);
        }
        return;
    }
    w->spd.y -= 0.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (emWepShotHitVaseCk(&em->pos_old, &em->pos) || emWepShotHitWindowCk(&em->pos_old, &em->pos)) {
        em->setFall(0, 0, 20.0f);
        return;
    }
    if (EatMgr.hitCheck(&em->pos_old, &em->pos, &hit, 0, 0, 0x404000)) {
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->Water_ck == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        em->hp = 0;
        SndStop(w->seid_throw, 0);
        em->pos = hit;
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        em->r_no_2 = 2;
        return;
    }
    if (w->pAtk) {
        part = (YARARE_INFO*) EmAtkLineHitCk(&em->pos_old, &em->pos, &hitPos, &nrm, 0);
        if (part) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
                EmPlBloodSet2(em, &em->pos, 1, w->effHit[0], w->effHit[1]);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
            }
            EmAtkSetDamagePL((cEm*) part, w->pAtk, &em->pos_old, &em->pos);
            if ((part->flags & 0x4000) == 0) {
                em->setFall(0, 0, 20.0f);
                return;
            }
            no = 0;
            if (part->partsNo != 0) {
                no = part->partsNo - 1;
            }
            PSMTXInverse(pPL->getPartsPtr(no)->mat, inv);
            PSMTXMultVec(inv, &part->pos, &em->pos);
            len = SQRTF(em->pos.x * em->pos.x + em->pos.z * em->pos.z);
            em->ang.x = -atan2f(-em->pos.y, len);
            em->ang.y = atan2f(-em->pos.x, -em->pos.z);
            em->ang.z = 0.0f;
            if ((s16) pGS->pl_life <= 0) {
                w->timer4 = 0;
            } else {
                w->timer4 = 30;
            }
            em->setParent(pPL, no, 0);
            em->hp = 0;
            emWep_R1_Parent(em);
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            c = GetCtrlCtrl12();
            Ctrl12Set(c, CTRL12_ID_EM10_ATK, 0x1E);
            Ctrl12Set(c, CTRL12_ID_EM10_THROW, 0x78);
            return;
        }
        part = EmAtkLineHitCkSub(&em->pos_old, &em->pos, &hitPos, &nrm);
        if (part) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
                EmPlBloodSet2(em, &em->pos, 1, w->effHit[0], w->effHit[1]);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
            }
            EmAtkSetDamageSub(part, w->pAtk, &em->pos_old, &em->pos);
            if ((part->flags & 0x4000) == 0) {
                em->setFall(0, 0, 20.0f);
                return;
            }
            no = 0;
            if (part->partsNo != 0) {
                no = part->partsNo - 1;
            }
            PSMTXInverse(pSUB->getPartsPtr(no)->mat, inv);
            PSMTXMultVec(inv, &part->pos, &em->pos);
            len = SQRTF(em->pos.x * em->pos.x + em->pos.z * em->pos.z);
            em->ang.x = -atan2f(-em->pos.y, len);
            em->ang.y = atan2f(-em->pos.x, -em->pos.z);
            em->ang.z = 0.0f;
            if ((s16) pGS->ashley_life <= 0) {
                w->timer4 = 0;
            } else {
                w->timer4 = 30;
            }
            em->setParent(pSUB, no, 0);
            em->hp = 0;
            emWep_R1_Parent(em);
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            return;
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->Water_ck == 0 && CheckInWater(em, 0)) {
        if (w->eff_id_always2[0] != 0xFF && w->eff_id_always2[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->eff_id_always2[0], w->eff_id_always2[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->Water_ck = 1;
    }
}

// Rno1 == 7: the crossbow's explosive arrow: trail est 0x2F/7, flies straight; a scenery hit
// (after the first 3 frames) plants it (est 0x2F/8) with a 63 frame beeping fuse, then
// emWepArrowBomb; a body hit sticks and explodes the same way.
void emWep_R1_ShotArrow(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec hit;
    Vec nrm;
    YARARE_INFO* part;

    switch (em->r_no_2) {
    case 0:
        EstSet((int) em, -1, 0, 0, 0x2F, 7, 0x800, w->espKind, (u32) em, 0);
        w->Timer = 0;
        w->Timer2 = 90;
        w->Timer3 = 3;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->Water_ck == 0) {
                w->seid_throw = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
            }
        }
        if (w->Timer2) {
            w->Timer2--;
        } else {
            em->r_no_0 = 1;
            em->r_no_1 = 2;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            return;
        }
        break;
    case 2:
        w->pEm_old = 0;
        em->setStatus(EM_STATUS_LOCKOFF);
        w->Bomb_wait = 63;
        w->Timer2 = 15;
        w->Timer = 0;
        EstSet((int) em, -1, 0, 0, 0x2F, 8, 0x800, w->espKind, (u32) em, 0);
        em->r_no_2++;
    case 3:
        em->partsWorldCalc();
        if (w->Bomb_wait == 0) {
            emWepArrowBomb(em);
            return;
        }
        w->Bomb_wait--;
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->Timer2;
            if (w->Timer2 > 3) {
                w->Timer2 -= 2;
            }
            SndCall(8, 0x14, &em->pos, 0x39, 0, em);
        }
        return;
    }
    w->spd.y -= 0.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (w->Timer3) {
        w->Timer3--;
    } else if (EatMgr.hitCheck(&em->pos_old, &em->pos, &hit, 0, 0, 0x404000)) {
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->Water_ck == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->seid_throw, 0);
        em->pos = hit;
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
        em->r_no_2 = 2;
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        return;
    }
    if (w->pAtk) {
        part = (YARARE_INFO*) EmAtkLineHitCk(&em->pos_old, &em->pos, &hit, &nrm, 0);
        if (part) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
                EmPlBloodSet2(em, &em->pos, 1, w->effHit[0], w->effHit[1]);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
            }
            EmAtkSetDamagePL((cEm*) part, w->pAtk, &em->pos_old, &em->pos);
            // `mr r3,part` is the LAST argument move in the original (part does not die there).
            asm("" : "=m"(hit) : "r"(part));  // COMPILER-DIFF: #13 (keep-alive)
        } else {
            part = EmAtkLineHitCkSub(&em->pos_old, &em->pos, &hit, &nrm);
            if (part == 0) {
                goto fly;
            }
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
            }
            SndStop(w->seid_throw, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
                EmPlBloodSet2(em, &em->pos, 1, w->effHit[0], w->effHit[1]);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
            }
            EmAtkSetDamageSub(part, w->pAtk, &em->pos_old, &em->pos);
            asm("" : "=m"(hit) : "r"(part));  // COMPILER-DIFF: #13 (keep-alive)
        }
        emWepArrowBomb(em);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        return;
    }
fly:
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->Water_ck == 0 && CheckInWater(em, 0)) {
        if (w->eff_id_always2[0] != 0xFF && w->eff_id_always2[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->eff_id_always2[0], w->eff_id_always2[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->Water_ck = 1;
    }
}

// Rno1 == 8: the RPG rocket: accelerates to 30 units / frame along its heading (looping the
// engine SE), explodes on the scenery (after 2 frames), within 500 units of the player's or the
// partner's chest, or after 500 frames.
void emWep_R1_Rocket(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Mtx m;
    Vec hit;
    cModel* p;

    switch (em->r_no_2) {
    case 0:
        w->Timer = 0;
        w->Timer2 = 500;
        w->Timer3 = 2;
        w->Roll = 0.0f;
        w->rocketSpd = 0.0f;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->Water_ck == 0) {
                w->seid_throw = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
            }
        }
        w->rocketSpd += 3.0f;
        if (w->rocketSpd > 30.0f) {
            w->rocketSpd = 30.0f;
        }
        if (w->Timer2) {
            w->Timer2--;
        } else {
            em->r_no_0 = 1;
            em->r_no_1 = 2;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            return;
        }
        break;
    }
    w->spd.y -= 0.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (w->Timer3 == 0) {
        if (EatMgr.hitCheck(&em->pos_old, &em->pos, &hit, 0, 0, 0x4000)) {
            emWepRocketBobm(em);
            return;
        }
    } else {
        w->Timer3--;
    }
    p = pPL->getPartsPtr(2);
    if ((em->pos.x - p->world.x) * (em->pos.x - p->world.x) + (em->pos.y - p->world.y) * (em->pos.y - p->world.y)
            + (em->pos.z - p->world.z) * (em->pos.z - p->world.z) < 250000.0f) {
        emWepRocketBobm(em);
        return;
    }
    if (pSUB) {
        p = pSUB->getPartsPtr(2);
        if ((em->pos.x - p->world.x) * (em->pos.x - p->world.x) + (em->pos.y - p->world.y) * (em->pos.y - p->world.y)
                + (em->pos.z - p->world.z) * (em->pos.z - p->world.z) < 250000.0f) {
            emWepRocketBobm(em);
            return;
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
}

// Rocket explosion: blast est (0x10/0x48, or 0x41 on a flat floor hit) at the rocket, the
// thrower's explosion SE, 5000 radius damage (PlWepHitCheck2 type 0x13), then Lost.
void emWepRocketBobm(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Camera* cam = &pG->Cam;
    cModel* p;
    Vec r;
    Vec pos;
    f32 len;

    SndStop(w->seid_throw, 0);
    EffectEspDelete(0, w->espKind, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind, (int) em);
    EffectEfmDelete(0, w->espKind, (int) em);
    p = em->getPartsPtr(0);
    len = (cam->param.pos.x - p->world.x) * (cam->param.pos.x - p->world.x)
        + (cam->param.pos.y - p->world.y) * (cam->param.pos.y - p->world.y)
        + (cam->param.pos.z - p->world.z) * (cam->param.pos.z - p->world.z);
    r.x = 0.0f;
    r.y = GetXZAngle(&p->world, &cam->param.pos);
    r.z = 0.0f;
    if (len < 16000000.0f) {
        EstSet(0, -1, &em->pos, &r, 0x10, 0x48, 0, 0, 0, 0);
    } else {
        EstSet(0, -1, &em->pos, &r, 0x10, 0x41, 0, 0, 0, 0);
    }
    em->hp = 0;
    if (w->pEm_old) {
        SndCall(8, 0x96, &em->pos, w->pEm_old->id, 0, em);
    }
    StaFlagOn(pG, STA_PL_FIRE);
    pos = em->pos_old;
    pos.y += 1200.0f;
    PlWepHitCheck2(0, &pos, &pos, 0x13, 3, 5000.0f);
    StaFlagOn(pG, STA_SE_BURST);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos_old, sizeof(Vec));
    pG->bell_stat = 1;
    em->setLost();
}

// Explosive arrow detonation: blast ests 0/0xD + 0/0x1A, SE, 5000 radius damage, then Lost.
void emWepArrowBomb(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec pos;

    SndStop(w->seid_throw, 0);
    EffectEspDelete(0, w->espKind, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind, (int) em);
    EffectEfmDelete(0, w->espKind, (int) em);
    EstSet(0, -1, &em->pos, 0, 0, 0xD, 0, 0, 0, 0);
    EstSet(0, -1, &em->pos, 0, 0, 0x1A, 0, 0, 0, 0);
    em->hp = 0;
    SndCall(8, 0x15, &em->pos, 0x39, 0, em);
    StaFlagOn(pG, STA_PL_FIRE);
    pos = em->pos;
    pos.y += 1200.0f;
    PlWepHitCheck2(0, &pos, &pos, 0x13, 3, 5000.0f);
    StaFlagOn(pG, STA_SE_BURST);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
    pG->bell_stat = 1;
    em->setLost();
}

// Rno1 == 9: thrown dynamite: tumbles with gravity, ticking SE every 6 frames, bounces off the
// scenery (EatMgr.adjust, first bounce SE), explodes when Bomb_wait (the fuse) runs out (blast
// est, SE, 5000 radius damage, Lost); landing in water drowns it (splash, Fall).
void emWep_R1_BombThrow(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Vec nrm;
    f32 spd;
    f32 ang;
    f32 len;
    f32 rad;

    switch (em->r_no_2) {
    case 0:
        w->bounce = 1;
        w->Timer = 0;
        w->Timer2 = 0;
        em->r_no_2++;
    case 1:
        if (w->Timer2 % 6 == 0 && w->pEm_old) {
            SndCall(8, 0x95, &em->pos, w->pEm_old->id, 0, em);
        }
        w->Timer2++;
        break;
    }
    if (w->Bomb_wait) {
        w->Bomb_wait--;
    }
    if (w->Bomb_wait == 0) {
        GlobalWork* g = pG;
        Camera* cam = &g->Cam;
        cModel* p;
        Vec r;
        Vec pos;
        f32 dist;

        p = em->getPartsPtr(0);
        dist = (cam->param.pos.x - p->world.x) * (cam->param.pos.x - p->world.x)
            + (cam->param.pos.y - p->world.y) * (cam->param.pos.y - p->world.y)
            + (cam->param.pos.z - p->world.z) * (cam->param.pos.z - p->world.z);
        r.x = 0.0f;
        r.y = GetXZAngle(&p->world, &cam->param.pos);
        r.z = 0.0f;
        if (dist < 16000000.0f) {
            EstSet(0, -1, &em->pos, &r, 0x10, 0x48, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &em->pos, &r, 0x10, 0x41, 0, 0, 0, 0);
        }
        em->hp = 0;
        if (w->pEm_old) {
            SndCall(8, 0x96, &em->pos, w->pEm_old->id, 0, em);
        }
        StaFlagOn(pG, STA_PL_FIRE);
        pos = em->pos;
        pos.y += 1200.0f;
        PlWepHitCheck2(0, &pos, &pos, 0x13, 3, 5000.0f);
        StaFlagOn(pG, STA_SE_BURST);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
        pG->bell_stat = 1;
        em->setLost();
        return;
    }
    w->spd.y -= 15.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->pos_old, &em->pos, 100.0f, 0x2001, 0x4000);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.5f);
        if (w->bounce) {
            w->bounce = 0;
            SndCall(5, 6, &em->pos, 0, 0, em);
        }
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->Water_ck == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->seid_throw, 0);
    }
    {
        Vec up;
        Mtx m;
        Vec fwd;

        PSVECSubtract(&em->pos, &em->pos_old, &d);
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
#line 2096 "D:/Bio4/Prog/emwep.cpp"
        VECNormalize(&fwd, &fwd);
        ang = acosf(PSVECDotProduct(&up, &fwd));
        if (ang > 0.01f && ang < 3.1315927f) {
            len = SQRTF(w->spd.x * w->spd.x + w->spd.y * w->spd.y + w->spd.z * w->spd.z);
            if (len > 200.0f) {
                len = 200.0f;
            }
            rad = len * 0.005f * 0.62831855f;
            PSVECCrossProduct(&up, &fwd, &up);
            PSMTXRotAxisRad(m, &up, rad);
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->Water_ck == 0 && CheckInWater(em, 0)) {
        if (w->eff_id_always2[0] != 0xFF && w->eff_id_always2[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->eff_id_always2[0], w->eff_id_always2[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->Water_ck = 1;
        em->setFall(0, 0, 20.0f);
    }
}

// Rno1 == 0xB: a thrown flash grenade: tumbles and bounces like the dynamite; when the fuse ends
// spawns the flash ests (0x2F/5 + 6), the SE and blinds the player (a white-out) when he is
// alive, then Lost.
void emWep_R1_FlashThrow(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Vec nrm;
    Mtx m;
    Vec up;
    Vec fwd;
    f32 spd;
    f32 ang;
    f32 len;
    f32 rad;
    int dead;

    switch (em->r_no_2) {
    case 0:
        em->hp = 0;
        w->Timer = 0;
        w->Timer2 = 0;
        w->bounce = 1;
        em->r_no_2++;
    case 1:
        w->Timer2++;
        break;
    }
    if (w->Bomb_wait) {
        w->Bomb_wait--;
    }
    if (w->Bomb_wait == 0) {
        EstSet(0, -1, &em->pos, 0, 0x2F, 5, 0, 0, 0, 0);
        EstSet(0, -1, 0, 0, 0x2F, 6, 0, 0, 0, 0);
        SndCall(1, 0x13, &em->pos, 0, 0, 0);
        if ((s16) pG->pl_life > 0) {
            dead = 1;
            if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
                dead = 0;
            }
            if (dead == 0) {
                PlSetDamage(9, 0, 0);
            }
        }
        em->setLost();
        return;
    }
    w->spd.y -= 15.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->pos_old, &em->pos, 100.0f, 0x2001, 0x4000);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.5f);
        if (w->bounce) {
            w->bounce = 0;
            SndCall(5, 6, &em->pos, 0, 0, em);
        }
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->Water_ck == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->seid_throw, 0);
    }
    PSVECSubtract(&em->pos, &em->pos_old, &d);
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
#line 2232 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&fwd, &fwd);
    ang = acosf(PSVECDotProduct(&up, &fwd));
    if (ang > 0.01f && ang < 3.1315927f) {
        len = SQRTF(w->spd.x * w->spd.x + w->spd.y * w->spd.y + w->spd.z * w->spd.z);
        if (len > 200.0f) {
            len = 200.0f;
        }
        rad = len * 0.005f * 0.62831855f;
        PSVECCrossProduct(&up, &fwd, &up);
        PSMTXRotAxisRad(m, &up, rad);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
}

// Rno1 == 0xC: a thrown hand grenade: tumbles and bounces; explodes (blast, SE, 5000 radius
// damage) when the fuse ends. In its last 24 frames, with the player within 6000 units, offers
// the escape action button 0x25 (emWepEscapeAction) once.
void emWep_R1_GrenadeThrow(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Vec nrm;
    f32 spd;
    f32 ang;
    f32 len;
    f32 rad;

    switch (em->r_no_2) {
    case 0:
        w->bounce = 1;
        w->Timer = 0;
        w->Timer2 = 0;
        w->Act_ck = 0;
        em->r_no_2++;
    case 1:
        w->Timer2++;
        break;
    }
    if (w->Bomb_wait) {
        w->Bomb_wait--;
    }
    if (w->Bomb_wait == 0) {
        Vec pos;

        EstSet(0, -1, &em->pos, 0, 0, 0xD, 0, 0, 0, 0);
        EstSet(0, -1, &em->pos, 0, 0, 0x1A, 0, 0, 0, 0);
        em->hp = 0;
        SndCall(1, 0x14, &em->pos, 0, 0, em);
        StaFlagOn(pG, STA_PL_FIRE);
        pos = em->pos;
        pos.y += 1200.0f;
        PlWepHitCheck2(0, &pos, &pos, 0x13, 3, 5000.0f);
        StaFlagOn(pG, STA_SE_BURST);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
        pG->bell_stat = 1;
        em->setLost();
        return;
    }
    if (w->Bomb_wait <= 0x18 && em->plDist2 < 36000000.0f && w->Act_ck == 0) {
        ActBtn.set(0x25, 0xB, (int) emWepEscapeAction, (int) em, 1, 3, 0, 0);
    }
    w->spd.y -= 15.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->pos_old, &em->pos, 100.0f, 0x2001, 0x4000);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.5f);
        if (w->bounce) {
            w->bounce = 0;
            SndCall(5, 6, &em->pos, 0, 0, em);
        }
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->Water_ck == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->seid_throw, 0);
    }
    {
        Mtx m;
        Vec up;
        Vec fwd;

        PSVECSubtract(&em->pos, &em->pos_old, &d);
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
#line 2366 "D:/Bio4/Prog/emwep.cpp"
        VECNormalize(&fwd, &fwd);
        ang = acosf(PSVECDotProduct(&up, &fwd));
        if (ang > 0.01f && ang < 3.1315927f) {
            len = SQRTF(w->spd.x * w->spd.x + w->spd.y * w->spd.y + w->spd.z * w->spd.z);
            if (len > 200.0f) {
                len = 200.0f;
            }
            rad = len * 0.005f * 0.62831855f;
            PSVECCrossProduct(&up, &fwd, &up);
            PSMTXRotAxisRad(m, &up, rad);
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
}

// Action button of the grenade: the player dodges according to where the grenade lies.
void emWepEscapeAction(cEmWep* em)
{
    f32 ang;
    f32 a;

    EMWEP_WK(em)->Act_ck = 1;
    ang = Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.1415927f);
    a = fabsf(ang);
    if (a < 0.7853982f) {
        SetPlDamage(em, plemBackjump);
    } else if (a < 2.3561945f) {
        SetPlDamage(em, plemFrontEscape);
    } else if (ang > 0.0f) {
        SetPlDamage(em, plemEscape);
    } else {
        SetPlDamage(em, plemEscape);
        pPL->r_no_3 = 1;
        GameAddPoint(9);
    }
}

// Player damage routine: runs away from the grenade.
static void plemEscape(cPlayer* pl)
{
    EmWepWork* w = EMWEP_WK(pl->pEmCatch);

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 2;
    switch (pl->r_no_2) {
    case 0:
        if (pl->r_no_3) {
            MotionSetCore(pl, &pl->pMotion, w->Mot_escape, (int) w->motEscape2, 3, 0x41, 0);
        } else {
            MotionSetCore(pl, &pl->pMotion, w->Mot_escape, (int) w->motEscape2, 3, 1, 0);
        }
        SndCall(1, 0x48, &pl->pos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->world, 0, 0, pl);
        pl->m_Work0 = 50;
        pl->m_Work1 = 15;
        pl->r_no_2++;
    case 1:
        emWepEscapeCamMove((cEmWep*)pl->pEmCatch);
        if (pl->m_Work1 && w->pEm_old) {
            pl->ang.y += Muku(&pl->pos, &w->pEm_old->pos, pl->ang.y, 0.19634955f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        MotionMove(pl, 0);
        if (pl->frame > 11.7f && pl->frame < 12.3f) {
            EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Player damage routine: back jump away from the grenade.
void plemBackjump(cPlayer* pl)
{
    EmWepWork* w = EMWEP_WK(pl->pEmCatch);

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 0x1E;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, w->motBackjump, 0, 3, 1, 5);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->world, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->world, 0, 0, pl);
        GameAddPoint(11);
        pl->m_Work0 = 35;
        pl->m_Work1 = 0;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else if (Key.on & 0x1F) {
            pl->m_Work1 = 1;
        }
        if (pl->frame > 10.7f && pl->frame < 11.3f) {
            SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 21.7f && pl->frame < 22.3f) {
            SndCall(5, 0x14, &pl->pos, 0, 0, pl);
        }
        if ((pl->frame > 36.7f && pl->frame < 37.3f) || (pl->frame > 49.7f && pl->frame < 50.3f)) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if ((pl->frame > 37.7f && pl->frame < 38.3f) || (pl->frame > 50.7f && pl->frame < 51.3f)) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0) || pl->m_Work1) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Player damage routine: dive forward over the grenade.
void plemFrontEscape(cPlayer* pl)
{
    EmWepWork* w = EMWEP_WK(pl->pEmCatch);

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 0x1E;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, w->motFront, 0, 3, 1, 5);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->world, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->world, 0, 0, pl);
        GameAddPoint(11);
        pl->m_Work0 = 35;
        pl->m_Work1 = 0;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else if (Key.on & 0x1F) {
            pl->m_Work1 = 1;
        }
        if (pl->frame > 21.7f && pl->frame < 22.3f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 34.7f && pl->frame < 35.3f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0) || pl->m_Work1) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Camera of the grenade escape: behind the player, pulled in front of the scenery.
void emWepEscapeCamMove(cEmWep* em)
{
    GlobalWork* g = pG;
    EmWepWork* w = EMWEP_WK(em);
    Vec p0;
    Vec p1;
    Vec hit;
    Vec d;
    f32 len;

    // Store through a cast pointer (no MEM_IN_STRUCT_P): the store may alias the `pPL` load below,
    // which keeps `lwz pPL` after it and ranks the `w` chain above the constant-pool `lis`es.
    *(f32*) (u8*) &w->Cam.param.fovy = g->Cam.param.fovy;
    p0.x = -376.0f;
    p0.y = 575.0f;
    p0.z = -1831.0f;
    p1.x = -244.0f;
    p1.y = 809.0f;
    p1.z = 52.6f;
    PSMTXMultVec(pPL->mat, &p0, &p0);
    PSMTXMultVec(pPL->mat, &p1, &p1);
    PosToPos(&g->Cam.param.at, &p1, &w->Cam.param.at, 1.0f);
    PosToPos(&g->Cam.param.pos, &p0, &w->Cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->Cam.param.at, &w->Cam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &w->Cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 2682 "D:/Bio4/Prog/emwep.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->Cam.param.at, &d, &w->Cam.param.pos);
    }
    len = (w->Cam.param.pos.x - w->Cam.param.at.x) * (w->Cam.param.pos.x - w->Cam.param.at.x)
        + (w->Cam.param.pos.y - w->Cam.param.at.y) * (w->Cam.param.pos.y - w->Cam.param.at.y)
        + (w->Cam.param.pos.z - w->Cam.param.at.z) * (w->Cam.param.pos.z - w->Cam.param.at.z);
    w->Cam.up.x = 0.0f;
    w->Cam.up.y = 1.0f;
    w->Cam.up.z = 0.0f;
    w->Cam.dist = SQRTF(len);
    CameraSetOrientationUp(&w->Cam);
    CamCtrl.m_pExtraCamera = (s32) &w->Cam;
}

// Puts the weapon in `parent`'s parts `partsNo_` (Rno1 3); flag skips the matrix normalisation.
void cEmWep::setParent(cEm* parent, int partsNo_, int flag)
{
    EmWepWork* w = EMWEP_WK(this);

    w->pEm_oya = parent;
    w->oya_parts = partsNo_;
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

// Drops the weapon: it falls as a three-node rope (emWep_R1_Fall) with `spd` as the initial
// speed of the nodes (random when NULL).
void cEmWep::setFall(int type_, Vec* spd, f32 grav)
{
    EmWepWork* w = EMWEP_WK(this);
    Mtx m;
    Vec v;
    f32 ang;
    u32 i;

    pMotion = 0;
    for (i = 0; i < 3; i++) {
        if (spd) {
            switch (i) {
            case 0:
            default:
                w->pt[i].x = spd->x;
                w->pt[i].y = spd->y;
                w->pt[i].z = spd->z;
                break;
            case 1:
                if (spd->x == 0.0f && spd->z == 0.0f) {
                    ang = 0.0f;
                } else {
                    ang = atan2f(spd->x, spd->z);
                }
                PSMTXRotRad(m, 'y', ang + 1.5707964f);
                PSMTXMultVec(m, spd, &v);
                w->pt[i].x = v.x;
                w->pt[i].y = v.y;
                w->pt[i].z = v.z;
                break;
            case 2:
                if (spd->x == 0.0f && spd->z == 0.0f) {
                    ang = 0.0f;
                } else {
                    ang = atan2f(spd->x, spd->z);
                }
                PSMTXRotRad(m, 'y', ang - 1.5707964f);
                PSMTXMultVec(m, spd, &v);
                w->pt[i].x = v.x;
                w->pt[i].y = v.y;
                w->pt[i].z = v.z;
                break;
            }
        } else {
            w->pt[i].x = fRand1_1() * 10.0f;
            w->pt[i].y = fRand1_1() * 10.0f + 50.0f;
            w->pt[i].z = fRand1_1() * 10.0f;
        }
    }
    w->fall_type = type_;
    w->pEm_oya = 0;
    w->pEm_old = 0;
    hp = 0;
    w->grav = grav;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &this->ang);
    r_no_0 = 1;
    r_no_1 = 4;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Throws the weapon with speed `spd` (a random forward throw in the parent's frame when NULL).
void cEmWep::setThrow(Vec* spd, f32 grav, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;
    Mtx m;
    register f64 hd asm("fr1"); // COMPILER-DIFF: #8

    // COMPILER-DIFF: #8 -- the original ranks `fmr f30,f1` (grav) after `mr r26,r5; addi w`, i.e. as
    // if f1 did not die at the copy; the DFmode read of f1 keeps it live past the copy (see
    // emshield setFall / docs/matching.md #8).
    asm("" : "=m"(hp) : "f"(hd));
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
    w->grav = grav;
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
        w->pEm_old = w->pEm_oya;
    }
    w->pEm_oya = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    r_no_0 = 1;
    r_no_1 = 5;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Scythe throw: flies straight (no gravity) spinning about its axis (emWep_R1_ThrowScythe).
void cEmWep::setThrowScythe(Vec* spd, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;

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
    w->grav = 15.0f;
    ang.x = 0.0f;
    ang.y = atan2f(v.x, v.z);
    ang.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    pos_old = pos;
    if (w->pEm_oya) {
        w->pEm_old = w->pEm_oya;
    }
    w->pEm_oya = 0;
    hp = 1;
    setYarareCubeF(this, 1500.0f, 1500.0f, 1500.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    r_no_0 = 1;
    r_no_1 = 0xA;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Shoots the weapon along `spd` (emWep_R1_Shot): it sticks into the player on a hit.
void cEmWep::setShot(Vec* spd, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;
    Mtx m;
    f32 len;

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
    len = SQRTF(v.x * v.x + v.z * v.z);
    ang.x = -atan2f(v.y, len);
    ang.y = atan2f(v.x, v.z);
    ang.z = 0.0f;
    w->grav = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &ang);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    pos_old = pos;
    if (w->pEm_oya) {
        w->pEm_old = w->pEm_oya;
    }
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    r_no_0 = 1;
    r_no_1 = 6;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Shoots an (explosive) arrow (emWep_R1_ShotArrow).
void cEmWep::setShotArrow(Vec* spd, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;
    Mtx m;
    f32 len;

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
    len = SQRTF(v.x * v.x + v.z * v.z);
    ang.x = -atan2f(v.y, len);
    ang.y = atan2f(v.x, v.z);
    ang.z = 0.0f;
    w->grav = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &ang);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    pos_old = pos;
    w->pEm_old = w->pEm_oya;
    w->pEm_oya = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    r_no_0 = 1;
    r_no_1 = 7;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Fires the weapon as a rocket (emWep_R1_Rocket) for `owner`.
void cEmWep::setRocket(cEm* owner, Vec* spd, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;
    Mtx m;
    f32 len;

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
    len = SQRTF(v.x * v.x + v.z * v.z);
    ang.x = -atan2f(v.y, len);
    ang.y = atan2f(v.x, v.z);
    ang.z = 0.0f;
    w->grav = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &ang);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    pos_old = pos;
    w->pEm_old = owner;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    r_no_0 = 1;
    r_no_1 = 8;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Throws the weapon as dynamite with a `fuse` frame fuse (emWep_R1_BombThrow).
void cEmWep::setBombThrow(Vec* spd, int fuse)
{
    EmWepWork* w = EMWEP_WK(this);
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
    w->grav = 15.0f;
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
        w->pEm_old = w->pEm_oya;
    }
    w->pEm_oya = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    w->Bomb_wait = fuse;
    w->pAtk = 0;
    r_no_0 = 1;
    r_no_1 = 9;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Throws the weapon as a flash grenade (emWep_R1_FlashThrow).
void cEmWep::setFlashThrow(Vec* spd, int fuse)
{
    EmWepWork* w = EMWEP_WK(this);
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
    w->grav = 15.0f;
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
        w->pEm_old = w->pEm_oya;
    }
    w->pEm_oya = 0;
    hp = 0;
    w->Bomb_wait = fuse;
    w->pAtk = 0;
    r_no_0 = 1;
    r_no_1 = 0xB;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Throws the weapon as a hand grenade (emWep_R1_GrenadeThrow) with the player's escape motions.
void cEmWep::setGrenadeThrow(Vec* spd, int fuse, void* motEscape, void* motEscape2, void* motBackjump, void* motFront)
{
    EmWepWork* w = EMWEP_WK(this);
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
    w->grav = 15.0f;
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
        w->pEm_old = w->pEm_oya;
    }
    w->pEm_oya = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    w->motFront = motFront;
    w->Bomb_wait = fuse;
    w->Mot_escape = motEscape;
    w->motEscape2 = motEscape2;
    w->motBackjump = motBackjump;
    w->pAtk = 0;
    r_no_0 = 1;
    r_no_1 = 0xC;
    r_no_2 = 0;
    r_no_3 = 0;
}

// SE played when the falling weapon lands (0xFF = none).
void cEmWep::setSeFall(u8 blk, u8 no, u8 vol)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seFall[0] = blk;
    w->seFall[1] = no;
    w->seFall[2] = vol;
    w->seFall[3] = 0;
}

// SE played when the weapon is shot out of the hand.
void cEmWep::setSeDamage(u8 blk, u8 no, u8 vol)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seDamage[0] = blk;
    w->seDamage[1] = no;
    w->seDamage[2] = vol;
}

// SE played when the thrown weapon hits the player.
void cEmWep::setSeHit(u8 blk, u8 no, u8 vol)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seHit[0] = blk;
    w->seHit[1] = no;
    w->seHit[2] = vol;
}

// SE played when the thrown weapon hits the scenery.
void cEmWep::setSeHitWall(u8 blk, u8 no, u8 vol)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seHitWall[0] = blk;
    w->seHitWall[1] = no;
    w->seHitWall[2] = vol;
}

// Flying SE restarted every `wait` frames while thrown / shot.
void cEmWep::setSeThrow(u8 blk, u8 no, u8 vol, u8 wait)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seThrow[0] = blk;
    w->seThrow[1] = no;
    w->seThrow[2] = vol;
    w->seThrow[3] = wait;
}

// SE played every `wait` frames at parts 0 while the weapon is visible (chainsaw idle).
void cEmWep::setSeAlways(u8 blk, u8 no, u8 vol, u8 wait)
{
    EmWepWork* w = EMWEP_WK(this);

    w->alwaysTimer = wait;
    w->seAlways[0] = blk;
    w->seAlways[1] = no;
    w->seAlways[2] = vol;
    w->alwaysWait = wait;
}

// Est spawned when the fall ends.
void cEmWep::setEffFall(u8 id, u8 type_)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effFall[0] = id;
    w->effFall[1] = type_;
}

// Est spawned when the weapon is shot out of the hand.
void cEmWep::setEffDamage(u8 id, u8 type_)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effDamage[0] = id;
    w->effDamage[1] = type_;
}

// Blood est arguments when the thrown weapon hits the player.
void cEmWep::setEffHit(u8 id, u8 type_)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effHit[0] = id;
    w->effHit[1] = type_;
}

// Est spawned when the weapon enters water.
void cEmWep::setEffWater(u8 id, u8 type_)
{
    EmWepWork* w = EMWEP_WK(this);

    w->eff_id_always2[0] = id;
    w->eff_id_always2[1] = type_;
}

// Attaches a continuous est (torch flame, chainsaw smoke) to the weapon under its Core_kind.
void cEmWep::setEffAlways(int id, int type_)
{
    EstSet((int) this, -1, 0, 0, id, type_, 0x800, EMWEP_WK(this)->espKind, (u32) this, 0);
}

// A repeating est spawned every `wait` frames at `ofs` in parts `parts` while visible.
void cEmWep::setEffAlways2(u8 id, u8 type_, u8 parts, Vec* ofs, u16 wait)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effAlways[0] = id;
    w->effAlways[1] = type_;
    w->always2_parts = parts;
    w->effAlwaysWait = wait;
    w->effAlwaysTimer = 1;
    w->always2_offset = *ofs;
}

// Makes the weapon shootable: a cylinder hit box (offset `size` or the origin) of width w /
// height h, hp 1.
void cEmWep::setYarare(Vec* size, f32 w, f32 h)
{
    if (size) {
        YarareInit(this, size->x, size->y, size->z, w, h, 0, 1);
    } else {
        YarareInit(this, 0.0f, 0.0f, 0.0f, w, h, 0, 1);
    }
    hp = 1;
}

// Makes the weapon shootable with a box hit box (offset `size` or 400 below the origin).
void cEmWep::setYarareCube(Vec* size, f32 x, f32 y, f32 z)
{
    if (size) {
        YarareInitCube(this, size->x, size->y, size->z, x, y, z, 0, 1);
    } else {
        YarareInitCube(this, 0.0f, -400.0f, 0.0f, x, y, z, 0, 1);
    }
    hp = 1;
}

// on == 0 keeps the weapon hidden (Be_flg bit1), on != 0 shows it.
void cEmWep::setTransMode(int on)
{
    EmWepWork* w = EMWEP_WK(this);

    if (on) {
        w->Be_flg &= ~2;
        be_flag |= 2;
    } else {
        w->Be_flg |= 2;
    }
}

// Scenario attribute (SceAt) destroyed together with the weapon (-1 = none).
void cEmWep::setAtNo(int no)
{
    EMWEP_WK(this)->At_no = no;
}

// Hides the weapon and goes to Lost.
void cEmWep::setLost()
{
    r_no_0 = 1;
    be_flag &= ~2;
    r_no_1 = 2;
    hp = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Drops a held weapon that has a pending drop timer (doors opening, holder staggered).
void cEmWep::setWaitDrop()
{
    if (EMWEP_WK(this)->timer4) {
        setFall(0, 0, 20.0f);
    }
}

// The scythe took the player's head off: the head becomes an obj01 (blood effects on both).
void emWepPlHeadLost()
{
    Vec p0;
    Vec p1;
    cModel* p;
    cObj* obj;

    if (pSys->eff_country == 0) {
        PlSetDamageSe(0xD);
        EstSet((int) pPL, -1, 0, 0, 0x10, 0x57, 0, 0, (u32) pPL, 0);
        return;
    }
    pPL->setHead(0);
    p = pPL->getPartsPtr(3);
    p0.x = 0.0f;
    p0.y = 68.0f;
    p0.z = 28.0f;
    p1.x = 0.0f;
    p1.y = 50.0f;
    p1.z = -25.0f;
    PSMTXMultVec(p->mat, &p0, &p0);
    PSMTXMultVecSR(pPL->mat, &p1, &p1);
    obj = SetObj01(PL_ARC_PTR(pG->pPlayer, 0xC), PL_ARC_PTR(pG->pPlayer, 7), &p0, &pPL->ang, &p1, 10.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->LightInfo.EnableMask = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, 0, -1);
    }
    EstSet((int) obj, -1, 0, 0, 0x10, 0x46, 0, 0, (u32) obj, 0);
    EstSet((int) pPL, -1, 0, 0, 0x10, 0x45, 0, 0, (u32) pPL, 0);
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
}

// The shot weapon (a-b) against the vase enemies (id 0x43, types 6/7) in front of the player:
// registers the damage on the hit one. 1 on a hit.
int emWepShotHitVaseCk(Vec* pPos, Vec* pPos2)
{
    Mtx m;
    Vec hit;
    Vec hitPos;
    Vec d;
    cEm* hitEm;
    YARARE_INFO* hitPart;
    f32 len;
    u32 i;
    int r;

    r = EatMgr.hitCheck(pPos, pPos2, &hit, 0, 0, 0x404000);
    hitEm = 0;
    hitPart = 0;
    if (r == 0) {
        hit = *pPos2;
    }
    len = (pPos->x - hit.x) * (pPos->x - hit.x) + (pPos->y - hit.y) * (pPos->y - hit.y) + (pPos->z - hit.z) * (pPos->z - hit.z);
    PSVECSubtract(pPos2, pPos, &d);
    if (d.x == d.z) {
        PSMTXIdentity(m);
    } else {
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    }
    TransMatrix(m, pPos);
    PSMTXInverse(m, m);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        YARARE_INFO* part;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->id != 0x43) {
            continue;
        }
        switch (e->type) {
        case 6:
        case 7:
            break;
        default:
            continue;
        }
        PSMTXMultVec(m, &e->getPartsPtr(0)->world, &d);
        if (d.z < -10000.0f) {
            continue;
        }
        if (d.x > 10000.0f) {
            continue;
        }
        if (d.x < -10000.0f) {
            continue;
        }
        if (Front_check(pPL, e, 1.5707964f) == 0) {
            continue;
        }
        part = emLineAtCk(e, pPos, pPos2, len, 0);
        if (part) {
            part->flags |= 0x4000;
            hitEm = e;
            hitPart = part;
            hit = hitPos;
        }
    }
    if (hitEm) {
        hitEm->dmg.set(0, 10, 0x18, pPos, hitPart->rad, hitPart);
    }
    return hitEm != 0;
}

// Same for the window enemies (id 0x46).
int emWepShotHitWindowCk(Vec* pPos, Vec* pPos2)
{
    Mtx m;
    Vec hit;
    Vec hitPos;
    Vec d;
    cEm* hitEm;
    YARARE_INFO* hitPart;
    f32 len;
    u32 i;
    int r;

    r = EatMgr.hitCheck(pPos, pPos2, &hit, 0, 0, 0x404000);
    hitEm = 0;
    hitPart = 0;
    if (r == 0) {
        hit = *pPos2;
    }
    len = (pPos->x - hit.x) * (pPos->x - hit.x) + (pPos->y - hit.y) * (pPos->y - hit.y) + (pPos->z - hit.z) * (pPos->z - hit.z);
    PSVECSubtract(pPos2, pPos, &d);
    if (d.x == d.z) {
        PSMTXIdentity(m);
    } else {
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    }
    TransMatrix(m, pPos);
    PSMTXInverse(m, m);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        YARARE_INFO* part = 0;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->id != 0x46) {
            continue;
        }
        PSMTXMultVec(m, &e->getPartsPtr(0)->world, &d);
        if (d.z < -10000.0f) {
            continue;
        }
        if (d.x > 10000.0f) {
            continue;
        }
        if (d.x < -10000.0f) {
            continue;
        }
        if (Front_check(pPL, e, 1.5707964f) == 0) {
            continue;
        }
        part = emLineAtCk(e, pPos, pPos2, len, 0);
        if (part) {
            part->flags |= 0x4000;
            hitEm = e;
            hitPart = part;
            hit = hitPos;
        }
    }
    if (hitEm) {
        hitEm->dmg.set(0, 10, 0x18, pPos, hitPart->rad, hitPart);
    }
    return hitEm != 0;
}

// Chain weapons (whips): the parts hang as a pendulum cloth from `owner`'s collision volumes.
void cEmWep::setCloth(cModel* owner)
{
    EmWepWork* w = EMWEP_WK(this);

    w->Cloth.Num = 10;
    w->Cloth.pLeft = 0;
    w->Cloth.pRight = 0;
    w->Cloth.pUpLeft = 0;
    w->Cloth.pUpRight = 0;
    w->Cloth.pWindSin = 0;
    w->Cloth.pWindRate = 0;
    w->Cloth.pGravity = 0;
    w->Cloth.pRate = 0;
    w->Cloth.Bundle_num = 0;
    w->Cloth.Flag = 0;
    w->Cloth.pCloth = emWepClothP;
    w->Cloth.pParent = emWepClothUp;
    w->Cloth.pChild = emWepClothDp;
    w->Cloth.pMax = emWepClothMax;
    w->Cloth.pAtset = emWepAt;
    w->Cloth.pEm_at = owner;
    w->Cloth.At_num = 3;
    w->Cloth.Gravity = 25.0f;
    w->Cloth.Rate = 0.6f;
    w->Cloth.WindSin = 0.0f;
    w->Cloth.Stretchy = 1.0f;
    w->Cloth.Move_rate = 0.0f;
    w->Cloth.pPtbl = 0;
    PenClothSet(this, &w->Cloth, 100.0f);
    w->Be_flg |= 4;
}

// Per-frame chain simulation (setCloth weapons: flails / chains): PenClothMove2 on the 10-link
// chain, then the parts' local matrices are rebuilt from the simulated world matrices; a
// vanished collision target is forgotten.
void cEmWep::moveCloth()
{
    EmWepWork* w = EMWEP_WK(this);
    cModel* p;
    Mtx inv;

    if (w->Be_flg & 4) {
        if (w->Cloth.pEm_at && (w->Cloth.pEm_at->be_flag & 0x201) != 1) {
            w->Cloth.pEm_at = 0;
        }
        PenClothMove2(this, &w->Cloth);
        for (p = getPartsPtr(1); p; p = p->pParts) {
            PSMTXInverse(p->pParent->mat, inv);
            PSMTXConcat(inv, p->mat, p->l_mat);
        }
    }
}

// Matrix of a weapon hanging on its parent's parts (emWep_R1_Parent; objTrolley calls it with
// noMotion = 1 to skip the motion update).
void cEmWep::setParentMatCalc(int noMotion)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmWepWork* w = EMWEP_WK(this);
    cEm* parent = w->pEm_oya;

    if (parent == 0) {
        return;
    }
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    if (parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->oya_parts)->mat, mat, m);
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
#line 4302 "D:/Bio4/Prog/emwep.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 4304 "D:/Bio4/Prog/emwep.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 4306 "D:/Bio4/Prog/emwep.cpp"
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
        PSMTXCopy(m, mat);
    }
    if (noMotion == 0) {
        if (pMotion) {
            motFlags2 |= 0x40000000;
            MotionMove(this, 0);
        } else {
            partsMatCalc();
        }
    }
    partsWorldCalc();
}
