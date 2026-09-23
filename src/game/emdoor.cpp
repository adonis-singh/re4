// game/emdoor.cpp: door enemy (cEmDoor): the room doors the player opens or kicks open, with
// locks, chains, breakable panes and the effect collision pieces around them.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "emdoor.h"
#include "emhit.h"
#include "emwep.h"
#include "emrack.h"
#include "em_set.h"
#include "etc_model.h"
#include "at_mod.h"
#include "act_btn.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"
#include "main.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "motion.h"
#include "em_sub.h"
#include "obj12.h"


typedef void (*EmDoorFunc)(cEmDoor*);


static void emDoor_R1_Open2(cEmDoor* em);

// Parts index remap for the flipped motions (MotionWork::flip): identity.
static u16 emDoor_xflip_tbl[20] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
};

EmDoorFunc EmDoor_R0_move_tbl[5] = {
    emDoor_R0_Init,
    emDoor_R0_Move,
    0,
    0,
    (EmDoorFunc) Em_R0_Scenario,
};

static EmDoorFunc EmDoor_R1_move_tbl[10] = {
    emDoor_R1_Set,
    emDoor_R1_Open,
    emDoor_R1_Open2,
    emDoor_R1_Close,
    emDoor_R1_Break,
    emDoor_R1_Shock,
    emDoor_R1_OpenLock,
    emDoor_R1_CloseLock,
    emDoor_R1_Down,
    emDoor_R1_Downed,
};

// Local matrix from pos / rot / scale and the parts after it (the door has no motion most of the time).
static inline void emDoorMatUpdate(cEmDoor* em)
{
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// pG->Key_flg bit of key `no`.
static inline u32 emDoorKeyCk(u32 no)
{
    u32* tbl = pG->Key_flg;

    return tbl[no >> 5] & (0x80000000 >> (no & 0x1F));
}

// Creates a door enemy (id 0x41) from a model / TPL at pos / rot. type 0 / 3 wooden doors with
// breakable panes, 1 / 5 / 7 iron doors, 2 an iron door that falls flat when kicked, 4 an iron
// door with panes, 6 a tall (4400) iron door stored as type 1. 1300 x 2300 atari, hit boxes by
// type, 1000 hp. Room etc flag `flagNo` restores a broken door (bit0 -> Break) or a fallen one
// (bits 6 / 7 -> Downed with its direction). Starts closed in Rno1 0 Set. NULL on failure.
cEmDoor* SetDoor(void* bin, void* tpl, Vec* pos, Vec* rot, int type, int flagNo)
{
    cEmDoor* em;
    EmDoorWork* w;
    u16* flg;
    Vec v;
    int zero;
    f32 ry;

    em = (cEmDoor*) EmMgr.create(0x41);
    if (em == 0) {
        return 0;
    }
    w = EMDOOR_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetDoor() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    if (type != 6) {
        w->Height = 2300.0f;
        w->Width = 650.0f;
    } else {
        w->Height = 4400.0f;
        w->Width = 650.0f;
        em->type = 1;
    }
    em->Motion.flip = emDoor_xflip_tbl;
    EtcSetAddAmb(em, ETC_AMB_DOOR);
    zero = 0;
    w->Eff_id = 0xFF;
    AtariInit(&em->atari, -w->Width, w->Height * 0.5f, 0.0f, w->Width + 50.0f, 150.0f, 150.0f, w->Height * 0.5f + 50.0f, zero, 2, zero);
    em->atari.setPriority(PRI_LV3);
    em->atari.clrFlag100();
    em->setStatus(EM_STATUS_ACTIVE);
    w->pSat[0] = 0;
    w->pSat[3] = 0;
    w->pSat[2] = 0;
    w->pSat[1] = 0;
    w->pSat[4] = 0;
    w->pSat[5] = 0;
    emDoorYarareInit(em);
    em->hp_max = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 3000.0f, 3000.0f, 3000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->Lock_L_bend = 0.0f;
    w->Lock_R_bend = 0.0f;
    w->Chain_bend = 0.0f;
    w->Lock_L_hp = 0;
    w->Lock_R_hp = 0;
    w->Chain_hp[0] = 0;
    w->Chain_hp[1] = 0;
    w->Chain_hp[2] = 0;
    w->pLockL = 0;
    w->pLockR = 0;
    w->pChain = 0;
    w->Se_cancel = 0;
    w->Be_flg = 0;
    w->Key_flag = 0x36;
    w->rnd = Rnd() % 5;
    w->Door_hp = (Rnd() & 1) + 1;
    w->Open_timer = 0;
    w->pDoor = 0;
    w->Seid_open = 0;
    ry = em->ang.y;
    w->base_dir = ry;
    PSMTXRotRad(w->base_mat, 'y', ry);
    TransMatrix(w->base_mat, &em->pos);
    v.x = -w->Width;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(w->base_mat, &v, &v);
    TransMatrix(w->base_mat, &v);
    PSMTXInverse(w->base_mat, w->base_im);
    w->Etc_no = flagNo;
    flg = GetEtcFlgPtr(flagNo, pG->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->r_no_0 = 1;
        em->r_no_1 = 4;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        em->clearStatus(EM_STATUS_ACTIVE);
        if (flg && (*flg & 0xC0)) {
            if (*flg & 0x40) {
                w->Open_flag = 0;
            } else {
                w->Open_flag = 1;
            }
            em->r_no_0 = 1;
            em->r_no_1 = 9;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
        }
    } else {
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        em->setStatus(EM_STATUS_ACTIVE);
    }
    if (em->hp > 0) {
        emDoorSatSet(em);
    }
    return em;
}

// Chain `no` of a wooden door was hit.
static inline void emDoorChainHit(cEmDoor* em, u32 no)
{
    SndCall(6, 0x15, &em->pos, 0, 0, em);
    EmDmBloodSet2(em, 0xCB, 0, 0, 0, 0);
    emDoorSetDmgChain(em, no);
}

// Damage check of the wooden doors: a damage volume hit at the door centre breaks it; a weapon
// hit (not knife / grenades) on a lock / chain hit box damages that lock or chain (heavy weapons
// harder, shotguns only from close), on a pane counts down Door_hp and breaks the pane
// (emDoorSetDmgDoor), and explosives / magnum / rifle break the whole door and its locks.
void emDoorDmCkWood(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    YARARE_INFO* part;
    u8 wep;
    Vec v;
    Vec out;
    int kind;

    if (pEm->hp > 0) {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(w->base_mat, &v, &v);
        kind = DmgMgr.hitCheck(&v, &out);
        switch (kind) {
        case 1:
        case 4:
        case 5:
        case 7:
            pEm->setBreak(&out);
            return;
        }
    }
    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    wep = pEm->dmg.m_Wep;
    pEm->dmg.m_Flag = 0;
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
    pEm->dmg.m_Timer = 1;
    if (wep == 0x10) {
        pEm->dmg.m_Timer = 0x11;
    }
    if (w->Be_flg & 1) {
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        return;
    }
    if (pEm->dmg.m_Wep == 0x10) {
        if (part != &w->hit[12] && part != &w->hit[11] && part != &w->hit[13] && part != &w->hit[14] && part != &w->hit[15]) {
            pEm->dmg.m_Timer = 0;
            return;
        }
    }
    switch (pEm->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (part == &w->hit[12]) {
            emDoorSetDmgLock_L(pEm, 0);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorSetDmgLock_R(pEm, 0);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (part->parts_no != 0) {
            if (pEm->l_pl < 9000000.0f && Rnd() % 5 == 0) {
                w->Door_hp = 0.0f;
            } else {
                w->Door_hp -= 1.0f;
            }
        }
        emDoorSetDmgDoor(pEm);
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xF:
    case 0x28:
    case 0x2C:
        if (part == &w->hit[12]) {
            if (w->Be_flg & 2) {
                w->Lock_L_hp -= 4;
                emDoorSetDmgLock_L(pEm, 0);
            } else {
                emDoorSetDmgLock_L(pEm, 1);
            }
            return;
        }
        if (part == &w->hit[11]) {
            if (w->Be_flg & 2) {
                w->Lock_L_hp -= 4;
                emDoorSetDmgLock_R(pEm, 0);
            } else {
                emDoorSetDmgLock_R(pEm, 1);
            }
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (part->parts_no != 0) {
            w->Door_hp = 0.0f;
        }
        emDoorSetDmgDoor(pEm);
        break;
    case 7:
    case 8:
    case 0x21:
        if (part == &w->hit[12]) {
            if (pEm->l_pl < 25000000.0f) {
                if (w->Be_flg & 2) {
                    w->Lock_L_hp -= 4;
                } else {
                    w->Lock_L_hp = 0;
                }
            }
            emDoorSetDmgLock_L(pEm, 0);
            return;
        }
        if (part == &w->hit[11]) {
            if (pEm->l_pl < 25000000.0f) {
                if (w->Be_flg & 2) {
                    w->Lock_R_hp -= 4;
                } else {
                    w->Lock_R_hp = 0;
                }
            }
            emDoorSetDmgLock_R(pEm, 0);
            return;
        }
        if (part->parts_no != 0 && part->len < 64000000.0f) {
            w->Door_hp = 0.0f;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        emDoorSetDmgDoor(pEm);
        break;
    case 0xE:
        break;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        emDoorSetDmgLock_L(pEm, 1);
        emDoorSetDmgLock_R(pEm, 1);
        emDoorSetDmgChain(pEm, 0);
        emDoorSetDmgChain(pEm, 1);
        emDoorSetDmgChain(pEm, 2);
        emDoorSetBrkDoor(pEm, &pEm->dmg.m_PosFrom);
        break;
    }
    if (emDoorBrkCk(pEm)) {
        emDoorSetDmgLock_L(pEm, 1);
        emDoorSetDmgLock_R(pEm, 1);
        emDoorSetDmgChain(pEm, 0);
        emDoorSetDmgChain(pEm, 1);
        emDoorSetDmgChain(pEm, 2);
        EstSet(pEm, -1, 0, 0, w->Eff_id, 6, 0, ESP_CORE_KIND_NONE, pEm, 0);
        SndCall(6, 0x37, &pEm->pos, 0, 0, pEm);
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 4;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
    }
}

// Lock hit by a strong weapon: the lock hp drops by 4 in the strong mode, otherwise it breaks.
static inline void emDoorLockHitL(cEmDoor* em, EmDoorWork* w)
{
    if (w->Be_flg & 2) {
        w->Lock_L_hp -= 4;
        emDoorSetDmgLock_L(em, 0);
    } else {
        emDoorSetDmgLock_L(em, 1);
    }
}

// Right lock hit by a heavy weapon: strong locks lose 4 hp, normal ones break.
static inline void emDoorLockHitR(cEmDoor* em, EmDoorWork* w)
{
    if (w->Be_flg & 2) {
        w->Lock_R_hp -= 4;
        emDoorSetDmgLock_R(em, 0);
    } else {
        emDoorSetDmgLock_R(em, 1);
    }
}

// Lock hit by a shotgun: only from close by.
static inline void emDoorLockHitNearL(cEmDoor* em, EmDoorWork* w)
{
    if (em->l_pl < 25000000.0f) {
        if (w->Be_flg & 2) {
            w->Lock_L_hp -= 4;
        } else {
            w->Lock_L_hp = 0;
        }
    }
    emDoorSetDmgLock_L(em, 0);
}

// Right lock hit by a shotgun: damaged only within 5000 units.
static inline void emDoorLockHitNearR(cEmDoor* em, EmDoorWork* w)
{
    if (em->l_pl < 25000000.0f) {
        if (w->Be_flg & 2) {
            w->Lock_R_hp -= 4;
        } else {
            w->Lock_R_hp = 0;
        }
    }
    emDoorSetDmgLock_R(em, 0);
}

// Breaks the locks and the chain (or the one the shot came from).
static inline void emDoorBreakLocks(cEmDoor* em)
{
    f32 ang;

    switch (em->type) {
    default:
        ang = Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
        if (fabsf(ang) < PI / 2) {
            emDoorSetDmgLock_L(em, 1);
        } else {
            emDoorSetDmgLock_R(em, 1);
        }
        break;
    case 3:
    case 7:
        emDoorSetDmgLock_L(em, 1);
        emDoorSetDmgLock_R(em, 1);
        break;
    }
    emDoorSetDmgChain(em, 0);
    emDoorSetDmgChain(em, 1);
    emDoorSetDmgChain(em, 2);
}

// Damage check of the iron doors (types 1 / 5 / 7): only the locks and chains take damage,
// everything else sparks (est 2 of Eff_id); explosives break every lock and chain.
void emDoorDmCkIron(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    YARARE_INFO* part;
    u8 wep;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    wep = pEm->dmg.m_Wep;
    pEm->dmg.m_Flag = 0;
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
    pEm->dmg.m_Timer = 1;
    if (wep == 0x10) {
        pEm->dmg.m_Timer = 0x11;
        if (part != &EMDOOR_WK(pEm)->hit[12] && part != &EMDOOR_WK(pEm)->hit[11] && part != &EMDOOR_WK(pEm)->hit[13] &&
            part != &EMDOOR_WK(pEm)->hit[14] && part != &EMDOOR_WK(pEm)->hit[15]) {
            pEm->dmg.m_Timer = 0;
            return;
        }
    }
    switch (pEm->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (part == &w->hit[12]) {
            emDoorSetDmgLock_L(pEm, 0);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorSetDmgLock_R(pEm, 0);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        return;
    case 9:
    case 0xA:
    case 0x28:
        if (part == &w->hit[12]) {
            emDoorLockHitL(pEm, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitR(pEm, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        return;
    case 5:
    case 6:
    case 7:
    case 8:
    case 0xF:
    case 0x21:
    case 0x2C:
        if (part == &w->hit[12]) {
            emDoorLockHitNearL(pEm, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitNearR(pEm, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 3, 0, 0, 0);
        return;
    case 0xE:
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        emDoorBreakLocks(pEm);
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        }
        break;
    }
}

// Damage check of the type 4 iron door: like the iron door but its panes (hit boxes 3..10) break
// on any bullet hit.
void emDoorDmCkIron2(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    YARARE_INFO* part;
    u8 wep;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    wep = pEm->dmg.m_Wep;
    pEm->dmg.m_Flag = 0;
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
    pEm->dmg.m_Timer = 1;
    if (wep == 0x10) {
        pEm->dmg.m_Timer = 0x11;
        if (part != &EMDOOR_WK(pEm)->hit[12] && part != &EMDOOR_WK(pEm)->hit[11] && part != &EMDOOR_WK(pEm)->hit[13] &&
            part != &EMDOOR_WK(pEm)->hit[14] && part != &EMDOOR_WK(pEm)->hit[15]) {
            pEm->dmg.m_Timer = 0;
            return;
        }
    }
    switch (pEm->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (part == &w->hit[12]) {
            emDoorSetDmgLock_L(pEm, 0);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorSetDmgLock_R(pEm, 0);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (part->parts_no != 0) {
            w->Door_hp = 0.0f;
            emDoorSetDmgDoor(pEm);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        return;
    case 9:
    case 0xA:
    case 0x28:
        if (part == &w->hit[12]) {
            emDoorLockHitL(pEm, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitR(pEm, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (part->parts_no != 0) {
            w->Door_hp = 0.0f;
            emDoorSetDmgDoor(pEm);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        return;
    case 5:
    case 6:
    case 7:
    case 8:
    case 0xF:
    case 0x21:
    case 0x2C:
        if (part == &w->hit[12]) {
            emDoorLockHitNearL(pEm, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitNearR(pEm, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (part->parts_no != 0) {
            w->Door_hp = 0.0f;
            emDoorSetDmgDoor(pEm);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 3, 0, 0, 0);
        return;
    case 0xE:
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        emDoorBreakLocks(pEm);
        if (part->parts_no != 0) {
            w->Door_hp = 0.0f;
            emDoorSetDmgDoor(pEm);
            return;
        }
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        }
        break;
    }
}

// Damage check of the type 2 falling door: locks / chains as the iron door; the door itself
// only sparks (it is opened by kicking, not shooting).
void emDoorDmCkIronDown(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    YARARE_INFO* part;
    u8 wep;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    wep = pEm->dmg.m_Wep;
    pEm->dmg.m_Flag = 0;
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
    pEm->dmg.m_Timer = 1;
    if (wep == 0x10) {
        pEm->dmg.m_Timer = 0x11;
        if (part != &EMDOOR_WK(pEm)->hit[12] && part != &EMDOOR_WK(pEm)->hit[11] && part != &EMDOOR_WK(pEm)->hit[13] &&
            part != &EMDOOR_WK(pEm)->hit[14] && part != &EMDOOR_WK(pEm)->hit[15]) {
            pEm->dmg.m_Timer = 0;
            return;
        }
    }
    switch (pEm->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (part == &w->hit[12]) {
            emDoorSetDmgLock_L(pEm, 0);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorSetDmgLock_R(pEm, 0);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        return;
    case 9:
    case 0xA:
    case 0x28:
        if (part == &w->hit[12]) {
            emDoorLockHitL(pEm, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitR(pEm, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        return;
    case 5:
    case 6:
    case 7:
    case 8:
    case 0xF:
    case 0x21:
    case 0x2C:
        if (part == &w->hit[12]) {
            emDoorLockHitNearL(pEm, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitNearR(pEm, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(pEm, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(pEm, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(pEm, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(pEm, w->Eff_id, 3, 0, 0, 0);
        return;
    case 0xE:
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        emDoorBreakLocks(pEm);
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
        }
        break;
    }
}

// A hit box no longer takes hits.
static inline void emDoorHitOff(YARARE_INFO* hit)
{
    hit->flag &= ~1;
}

// Damages the left lock (mode 0: one hit, 4 for strong locks; mode 1: destroy): rattle est / SE,
// and when its hp is gone drops the lock object (cObj12 setFall) with the break SE and sets etc
// flag bit2.
void emDoorSetDmgLock_L(cEmDoor* pEm, int type)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    EmListData* d = &pG->Em_list[pEm->emset_no];
    YARARE_INFO* hit;
    u16* flg;
    Vec v;

    if (w->pLockL == 0) {
        return;
    }
    hit = &w->hit[12];
    switch (type) {
    case 0:
    default:
        if (w->Be_flg & 2) {
            w->Lock_L_hp -= 1;
        } else {
            w->Lock_L_hp = 0;
        }
        SndCall(6, 0x52, &pEm->pos, 0, 0, pEm);
        EmDmBloodSet2(pEm, 0xC9, 0, 0, 0, 0);
        EstSet(w->pLockL, -1, 0, 0, EFF_OBM2B, 1, 0, ESP_CORE_KIND_NONE, w->pLockL, 0);
        if (w->Lock_L_hp <= 0) {
            SndCall(6, 0x53, &pEm->pos, 0, 0, pEm);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 20.0f;
            PSMTXMultVecSR(w->pLockL->mat, &v, &v);
            w->pLockL->setFall(&v, 4);
            w->pLockL->setFallSe(6, 0x3C, 0);
            d->flag &= ~0x80000000;
            pEm->flag &= ~0x80000000;
            pEm->setStatus(EM_STATUS_LOCKOFF);
            w->pLockL = 0;
            emDoorHitOff(hit);
        }
        w->Lock_L_bend = PI / 4;
        break;
    case 1:
        w->Lock_L_hp = 0;
        SndCall(6, 0x53, &pEm->pos, 0, 0, pEm);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 20.0f;
        PSMTXMultVecSR(w->pLockL->mat, &v, &v);
        w->pLockL->setFall(&v, 4);
        w->pLockL->setFallSe(6, 0x3C, 0);
        SndCall(6, 0x52, &pEm->pos, 0, 0, pEm);
        EmDmBloodSet2(pEm, 0xC9, 0, 0, 0, 0);
        EstSet(w->pLockL, -1, 0, 0, EFF_OBM2B, 1, 0, ESP_CORE_KIND_NONE, w->pLockL, 0);
        d->flag &= ~0x80000000;
        pEm->flag &= ~0x80000000;
        pEm->setStatus(EM_STATUS_LOCKOFF);
        w->pLockL = 0;
        emDoorHitOff(hit);
        break;
    }
    if (w->Lock_L_hp <= 0) {
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 4;
        }
    }
}

// Same for the right lock (etc flag bit1).
void emDoorSetDmgLock_R(cEmDoor* pEm, int type)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    EmListData* d = &pG->Em_list[pEm->emset_no];
    YARARE_INFO* hit;
    u16* flg;
    Vec v;

    if (w->pLockR == 0) {
        return;
    }
    hit = &w->hit[11];
    switch (type) {
    case 0:
    default:
        if (w->Be_flg & 2) {
            w->Lock_R_hp -= 1;
        } else {
            w->Lock_R_hp = 0;
        }
        SndCall(6, 0x52, &pEm->pos, 0, 0, pEm);
        EmDmBloodSet2(pEm, 0xC9, 0, 0, 0, 0);
        EstSet(w->pLockR, -1, 0, 0, EFF_OBM2B, 1, 0, ESP_CORE_KIND_NONE, w->pLockR, 0);
        if (w->Lock_R_hp <= 0) {
            SndCall(6, 0x53, &pEm->pos, 0, 0, pEm);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 20.0f;
            PSMTXMultVecSR(w->pLockR->mat, &v, &v);
            w->pLockR->setFall(&v, 4);
            w->pLockR->setFallSe(6, 0x3C, 0);
            d->flag &= ~0x40000000;
            pEm->flag &= ~0x40000000;
            pEm->setStatus(EM_STATUS_LOCKOFF);
            flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
            if (flg) {
                *flg |= 4;
            }
            w->pLockR = 0;
            emDoorHitOff(hit);
        }
        w->Lock_R_bend = PI / 4;
        break;
    case 1:
        w->Lock_R_hp = 0;
        SndCall(6, 0x53, &pEm->pos, 0, 0, pEm);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 20.0f;
        PSMTXMultVecSR(w->pLockR->mat, &v, &v);
        w->pLockR->setFall(&v, 4);
        w->pLockR->setFallSe(6, 0x3C, 0);
        SndCall(6, 0x52, &pEm->pos, 0, 0, pEm);
        EmDmBloodSet2(pEm, 0xC9, 0, 0, 0, 0);
        EstSet(w->pLockR, -1, 0, 0, EFF_OBM2B, 1, 0, ESP_CORE_KIND_NONE, w->pLockR, 0);
        d->flag &= ~0x40000000;
        pEm->flag &= ~0x40000000;
        pEm->setStatus(EM_STATUS_LOCKOFF);
        w->pLockR = 0;
        emDoorHitOff(hit);
        break;
    }
    if (w->Lock_R_hp <= 0) {
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 2;
        }
    }
}

// Breaks chain link `no` (0..2): disables its hit box, hides the chain parts, spawns the chain
// break est (0xCB / no + 1) with the SE and sets etc flag bit 3 + no.
void emDoorSetDmgChain(cEmDoor* pEm, u32 no)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    cModel* parts;
    u16* flg;

    if (w->pChain == 0) {
        return;
    }
    if (w->Chain_hp[no] <= 0) {
        return;
    }
    flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
    switch (no) {
    case 0:
        emDoorHitOff(&w->hit[13]);
        w->Chain_hp[no] = 0;
        parts = w->pChain->getPartsPtr(1);
        EstSet(0, -1, &parts->world, &w->pChain->ang, EFF_OBM4C, 1, 0, ESP_CORE_KIND_NONE, 0, 0);
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        SndCall(6, 0x16, &pEm->pos, 0, 0, pEm);
        if (flg) {
            *flg |= 8;
        }
        break;
    case 1:
        emDoorHitOff(&w->hit[14]);
        w->Chain_hp[no] = 0;
        parts = w->pChain->getPartsPtr(2);
        EstSet(0, -1, &parts->world, &w->pChain->ang, EFF_OBM4C, 2, 0, ESP_CORE_KIND_NONE, 0, 0);
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        SndCall(6, 0x16, &pEm->pos, 0, 0, pEm);
        if (flg) {
            *flg |= 0x10;
        }
        break;
    case 2:
        emDoorHitOff(&w->hit[15]);
        w->Chain_hp[no] = 0;
        parts = w->pChain->getPartsPtr(3);
        EstSet(0, -1, &parts->world, &w->pChain->ang, EFF_OBM4C, 3, 0, ESP_CORE_KIND_NONE, 0, 0);
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        SndCall(6, 0x16, &pEm->pos, 0, 0, pEm);
        if (flg) {
            *flg |= 0x20;
        }
        break;
    }
}

// A pane (parts of the hit box) was shot: when Door_hp is used up hides the pane, spawns the
// splinter est facing the shooter, plays the SE and sets the pane's flag bit (0x8000 >> pane);
// otherwise just the hit SE. Four broken panes break the whole door.
void emDoorSetDmgDoor(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    YARARE_INFO* part = pEm->dmg.m_pDamageYarare;
    EmListData* d = &pG->Em_list[pEm->emset_no];
    cModel* parts;
    u16* flg;
    Vec v;
    Vec rot;
    Vec* wpos;
    f32 ang;
    f32 dif;
    u32 bit;
    u32 i;

    if (part->parts_no != 0) {
        parts = pEm->getPartsPtr(part->parts_no - 1);
        if (w->Door_hp <= 0.0f) {
            w->Door_hp = (Rnd() & 1) + 1;
            parts->scale.x = 0.0f;
            parts->scale.y = 0.0f;
            parts->scale.z = 0.0f;
            part->flag &= ~1;
            if (pEm->type == 4) {
                if (w->Eff_id != 0xFF) {
                    wpos = &pEm->getPartsPtr(part->parts_no - 1)->world;
                    v.x = 0.0f;
                    v.y = 0.0f;
                    v.z = 1.0f;
                    PSMTXMultVecSR(pEm->mat, &v, &v);
                    rot.x = 0.0f;
                    rot.y = atan2f(v.x, v.z);
                    rot.z = 0.0f;
                    ang = GetXZAngle(wpos, &pEm->dmg.m_PosFrom);
                    dif = Muku2(rot.y, ang, PI);
                    if (fabsf(dif) > PI / 2) {
                        rot.y += PI;
                        rot.y = LIMIT_ANGLE(rot.y);
                    }
                    EstSet(0, -1, wpos, &rot, w->Eff_id, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
                    SndCall(6, 0x3B, &pEm->pos, 0, 0, pEm);
                    flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
                    if (flg) {
                        *flg |= 8;
                    }
                }
            } else {
                if (w->Eff_id != 0xFF) {
                    EmDmBloodSet2(pEm, w->Eff_id, 0, 0, 0, 0);
                }
                SndCall(6, 0x36, &pEm->pos, 0, 0, pEm);
            }
            bit = 0x8000;
            for (i = 3; i <= 10; i++) {
                if (part == &w->hit[i]) {
                    pEm->flag |= bit;
                    d->flag |= bit;
                    break;
                }
                bit >>= 1;
            }
            return;
        }
    }
    if (w->Eff_id != 0xFF) {
        switch (pEm->dmg.m_Wep) {
        default:
            EmDmBloodSet2(pEm, w->Eff_id, 2, 0, 0, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            EmDmBloodSet2(pEm, w->Eff_id, 3, 0, 0, 0);
            break;
        }
    }
}

// Breaks the door from a shot at `pos` (explosive / heavy weapon): types 2 / 3 just open toward
// it, others spawn the break est (5 from the front, 4 from behind) with the SE and go to Break.
void emDoorSetBrkDoor(cEmDoor* pEm, Vec* pPos)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    f32 ang;

    if (pEm->hp <= 0) {
        return;
    }
    switch (pEm->type) {
    case 2:
    case 3:
        pEm->setOpen(pPos, 0, 0, 0);
        return;
    }
    if (w->Eff_id != 0xFF) {
        ang = Muku(&pEm->pos, pPos, pEm->ang.y, PI);
        if (fabsf(ang) < PI / 2) {
            EstSet(pEm, -1, 0, 0, w->Eff_id, 5, 0, ESP_CORE_KIND_NONE, pEm, 0);
        } else {
            EstSet(pEm, -1, 0, 0, w->Eff_id, 4, 0, ESP_CORE_KIND_NONE, pEm, 0);
        }
    }
    SndCall(6, 0x37, &pEm->pos, 0, 0, pEm);
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 4;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
}

// 1 when more than 3 panes are broken (the door should break).
int emDoorBrkCk(cEmDoor* pEm)
{
    u32 bit;
    int cnt;
    int i;

    if (pEm->hp <= 0) {
        return 0;
    }
    bit = 0x8000;
    cnt = 0;
    for (i = 0; i < 8; i++) {
        if (pEm->flag & bit) {
            cnt++;
        }
        bit >>= 1;
    }
    return (u32) cnt > 3;
}

// Per-frame: the type's damage check, forget a broken partner door, the action button check, the
// Rno0 routine (0 Init, 1 Move, 4 scenario), the model-vs-player atari, lock / chain bending
// and placement while intact, and the effect collision panels.
void cEmDoor::move()
{
    EmDoorWork* w = EMDOOR_WK(this);

    switch (type) {
    case 4:
        emDoorDmCkIron2(this);
        break;
    case 1:
    case 5:
        emDoorDmCkIron(this);
        break;
    case 2:
        emDoorDmCkIronDown(this);
        break;
    case 0:
    case 3:
    default:
        emDoorDmCkWood(this);
        break;
    case 7:
        emDoorDmCkIron(this);
        break;
    }
    if (w->pDoor && w->pDoor->hp <= 0) {
        w->pDoor = 0;
    }
    emDoorActEvtCk(this);
    be_flag &= ~0x4000;
    EmDoor_R0_move_tbl[r_no_0](this);
    EmAtCheck(this);
    atari.move();
    if (hp > 0) {
        emDoorLockBendMove(this);
        emDoorLockMove(this);
    }
    emDoorSatSet(this);
}

// Rno0 == 0: resets to the Set state.
void emDoor_R0_Init(cEmDoor* pEm)
{
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 0;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 Open, 2 Open2, 3 Close, 4 Break, 5 Shock, 6 OpenLock,
// 7 CloseLock, 8 Down, 9 Downed).
void emDoor_R0_Move(cEmDoor* pEm)
{
    EmDoor_R1_move_tbl[pEm->r_no_1](pEm);
}

// Rno1 == 0: resting (closed or swung open) door; rebuilds the matrix once (or every frame while
// panes are broken), and an open door swings shut again (Close) when nobody stands in it.
void emDoor_R1_Set(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        emDoorMatUpdate(pEm);
        pEm->r_no_2++;
        SndStop(w->Seid_open, 0);
        break;
    case 1:
        if (pEm->flag & 0xFFC0) {
            emDoorMatUpdate(pEm);
        }
        break;
    }
    if (emDoorDoorAutoCloseCk(pEm) == 0) {
        if (!(pEm->flag & 0xFFC0)) {
            pEm->be_flag |= 0x4000;
        }
    }
}

// Rno1 == 1: kicked open: flags the door open, swings ang.y toward +-90 degrees from base_dir
// (direction from Open_pos), hits whoever stands behind it once (Rno3: PlWepHitCheck3 type
// 0x18, 400 radius), bounces back a little and settles (flag 0x10000000 = fully open).
void emDoor_R1_Open(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    f32 ang;
    f32 d;
    Vec v;
    s16 hp;

    switch (pEm->r_no_2) {
    case 0:
        pEm->flag |= 0x20000000;
        w->Timer2 = 1;
        w->Timer = 0x14;
        w->Open_timer = 0x96;
        emDoorSetDmgLock_L(pEm, 1);
        emDoorSetDmgLock_R(pEm, 1);
        emDoorSetDmgChain(pEm, 0);
        emDoorSetDmgChain(pEm, 1);
        emDoorSetDmgChain(pEm, 2);
        pEm->hp = 1000;
        ang = fabsf(Muku(&pEm->pos, &w->Open_pos, w->base_dir, PI));
        if (ang > PI / 2) {
            w->Open_flag = 0;
        } else {
            w->Open_flag = 1;
        }
        emDoorDropWeapon(pEm);
        pEm->r_no_2++;
    case 1:
        if (w->Open_flag) {
            ang = LIMIT_ANGLE(w->base_dir - PI / 2);
            d = Muku2(pEm->ang.y, ang, PI / 6);
        } else {
            ang = LIMIT_ANGLE(w->base_dir + PI / 2);
            d = Muku2(pEm->ang.y, ang, PI / 7);
        }
        pEm->ang.y += d;
        pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        d = fabsf(d);
        if (d < PI / 1024) {
            pEm->ang.y = ang;
            pEm->r_no_2++;
        }
        if (w->Timer2 != 0) {
            w->Timer2--;
            pEm->dmg.m_Timer = 2;
            if (w->Open_flag) {
                v.x = 0.0f;
                v.y = w->Width;
                v.z = -600.0f;
            } else {
                v.x = 0.0f;
                v.y = w->Width;
                v.z = 600.0f;
            }
            PSMTXMultVec(w->base_mat, &v, &v);
            if (pEm->r_no_3 != 0) {
                hp = 0;
                if (pSUB) {
                    hp = pSUB->hp;
                    pSUB->hp = 0;
                }
                if (PlWepHitCheck3(&v, 0x18, 10, 400.0f)) {
                    SndCall(1, 0xF, &v, 0, 0, pEm);
                }
                if (pSUB) {
                    pSUB->hp = hp;
                }
            }
        }
        break;
    case 2:
        w->Timer = 2;
        pEm->r_no_2++;
    case 3:
        if (w->Open_flag) {
            ang = w->base_dir - PI / 2;
        } else {
            ang = w->base_dir + PI / 2;
        }
        if (w->Timer != 0) {
            w->Timer--;
            d = fRand0_1() * (PI / 128) + PI / 128;
            if (pG->Frame_cnt & 1) {
                d = -d;
            }
            pEm->ang.y = ang + d;
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        } else {
            pEm->ang.y = ang;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            pEm->flag |= 0x10000000;
        }
        break;
    }
    emDoorMatUpdate(pEm);
}

// Rno1 == 2: opened by hand: breaks any lock / chain, plays the door's open motion (player
// archive motion 0x1F, mirrored per Rno3 side) with the creak SE at frame 15 (unless the paired
// door plays it), drops hanging weapons nearby, then rests fully open.
static void emDoor_R1_Open2(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        pEm->flag |= 0x20000000;
        emDoorSetDmgLock_L(pEm, 1);
        emDoorSetDmgLock_R(pEm, 1);
        emDoorSetDmgChain(pEm, 0);
        emDoorSetDmgChain(pEm, 1);
        emDoorSetDmgChain(pEm, 2);
        pEm->hp = 1000;
        switch (pEm->r_no_3) {
        case 1:
            MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x1F), 0, 0, 0x41, 0);
            break;
        case 0:
        case 2:
        default:
            MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x1F), 0, 0, 1, 0);
            break;
        case 3:
            MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x1F), 0, 0, 0x41, 0);
            break;
        }
        emDoorDropWeapon(pEm);
        pEm->r_no_2++;
    case 1:
        if (pEm->Motion.Seq_frame > 14.7f && pEm->Motion.Seq_frame < 15.3f) {
            if (w->Se_cancel == 0) {
                switch (pEm->type) {
                case 0:
                case 3:
                default:
                    w->Seid_open = SndCall(1, 0x42, &pEm->pos, 0, 0, pEm);
                    break;
                case 1:
                case 2:
                case 4:
                case 5:
                case 7:
                    w->Seid_open = SndCall(1, 0x41, &pEm->pos, 0, 0, pEm);
                    break;
                }
            }
            w->Se_cancel = 0;
        }
        if (MotionMove(pEm, 0)) {
            pEm->flag |= 0x30000000;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
        }
        break;
    }
    pEm->partsWorldCalc();
}

// Rno1 == 8: the door is kicked off its hinges (types 2 / 3, or down_ck): breaks locks / chains,
// records the fall direction in the etc flag (bit6 / bit7), spawns the fall est (0xA / 7), then
// rotates about x with growing speed to +-90 degrees, hitting whoever stands behind (type 0x14),
// lands with the crash est (0xC / 0xB) and rests as Downed.
void emDoor_R1_Down(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    u16* flg;
    f32 ang;
    f32 h;
    f32 r;
    int water;
    Vec v;
    s16 hp;

    switch (pEm->r_no_2) {
    case 0:
        pEm->flag |= 0x20000000;
        w->Timer2 = 1;
        w->Timer = 0x14;
        w->Open_timer = 0x96;
        emDoorSetDmgLock_L(pEm, 1);
        emDoorSetDmgLock_R(pEm, 1);
        emDoorSetDmgChain(pEm, 0);
        emDoorSetDmgChain(pEm, 1);
        emDoorSetDmgChain(pEm, 2);
        ang = Muku(&pEm->pos, &w->Open_pos, w->base_dir, PI);
        if (fabsf(ang) > PI / 2) {
            w->Open_flag = 0;
        } else {
            w->Open_flag = 1;
        }
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 1;
            if (w->Open_flag == 0) {
                *flg |= 0x40;
            } else {
                *flg |= 0x80;
            }
        }
        water = 0;
        if (GetWaterHeight(&pEm->pos, &h) && pEm->pos.y < h) {
            water = 1;
        }
        switch (pEm->type) {
        case 2:
        case 3:
            if (water) {
                EstSet(pEm, -1, 0, 0, w->Eff_id, 0xA, 0, ESP_CORE_KIND_NONE, pEm, 0);
                SndCall(6, 3, &pEm->pos, 0, 0, pEm);
            } else {
                EstSet(pEm, -1, 0, 0, w->Eff_id, 7, 0, ESP_CORE_KIND_NONE, pEm, 0);
            }
            break;
        }
        w->Spd = 0.0f;
        emDoorDropWeapon(pEm);
        pEm->hp = 0;
        if (w->Open_flag) {
            w->Spd = -(7.0f * (PI / 180.0f));
        } else {
            w->Spd = 7.0f * (PI / 180.0f);
        }
        pEm->pos.y += 40.0f;
        pEm->r_no_2++;
    case 1:
        if (w->Open_flag) {
            w->Spd -= PI / 180.0f;
            pEm->ang.x += w->Spd;
            if (pEm->ang.x < -PI / 2) {
                pEm->ang.x = -PI / 2;
                pEm->r_no_2++;
            }
        } else {
            w->Spd += PI / 180.0f;
            pEm->ang.x += w->Spd;
            if (pEm->ang.x > PI / 2) {
                pEm->ang.x = PI / 2;
                pEm->r_no_2++;
            }
        }
        if (w->Timer2 != 0) {
            w->Timer2--;
            pEm->dmg.m_Timer = 2;
            if (w->Open_flag) {
                v.x = 0.0f;
                v.y = w->Width;
                v.z = -600.0f;
            } else {
                v.x = 0.0f;
                v.y = w->Width;
                v.z = 600.0f;
            }
            PSMTXMultVec(w->base_mat, &v, &v);
            if (pEm->r_no_3 != 0) {
                hp = 0;
                if (pSUB) {
                    hp = pSUB->hp;
                    pSUB->hp = 0;
                }
                if (PlWepHitCheck3(&v, 0x14, 10, 400.0f)) {
                    SndCall(1, 0xF, &pPL->pos, 0, 0, pPL);
                }
                if (pSUB) {
                    pSUB->hp = hp;
                }
            }
        }
        break;
    case 2:
        w->Timer = 2;
        water = 0;
        if (GetWaterHeight(&pEm->pos, &h) && pEm->pos.y < h) {
            water = 1;
        }
        switch (pEm->type) {
        case 2:
        case 3:
            if (water) {
                if (w->Open_flag) {
                    EstSet(pEm, -1, 0, 0, w->Eff_id, 0xC, 0, ESP_CORE_KIND_NONE, pEm, 0);
                    SndCall(6, 3, &pEm->pos, 0, 0, pEm);
                } else {
                    EstSet(pEm, -1, 0, 0, w->Eff_id, 0xB, 0, ESP_CORE_KIND_NONE, pEm, 0);
                    SndCall(6, 3, &pEm->pos, 0, 0, pEm);
                }
            } else {
                if (w->Open_flag) {
                    EstSet(pEm, -1, 0, 0, w->Eff_id, 9, 0, ESP_CORE_KIND_NONE, pEm, 0);
                } else {
                    EstSet(pEm, -1, 0, 0, w->Eff_id, 8, 0, ESP_CORE_KIND_NONE, pEm, 0);
                }
            }
            break;
        }
        pEm->r_no_2++;
    case 3:
        if (w->Open_flag) {
            pEm->ang.x = -PI / 2;
        } else {
            pEm->ang.x = PI / 2;
        }
        if (w->Timer != 0) {
            w->Timer--;
            r = fRand0_1() * (PI / 128) + PI / 128;
            if (pG->Frame_cnt & 1) {
                r = -r;
            }
            pEm->ang.x += r;
        } else {
            pEm->flag |= 0x10000000;
            pEm->r_no_2++;
        }
        break;
    case 4:
        break;
    }
    emDoorMatUpdate(pEm);
}

// Rno1 == 9: the fallen door lying flat (ang.x +-90 degrees by Open_flag), hp 0.
void emDoor_R1_Downed(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);

    if (pEm->r_no_2 == 0) {
        pEm->hp = 0;
        if (w->Open_flag) {
            pEm->ang.x = -PI / 2;
        } else {
            pEm->ang.x = PI / 2;
        }
        pEm->pos.y += 40.0f;
        pEm->flag |= 0x30000000;
        pEm->r_no_2++;
    }
    emDoorMatUpdate(pEm);
}

// Rno1 == 3: swings back to base_dir over 20 frames, then Set.
void emDoor_R1_Close(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);

    switch (pEm->r_no_2) {
    case 0:
        pEm->flag &= ~0x30000000;
        w->Timer = 0x14;
        pEm->r_no_2++;
    case 1:
        pEm->ang.y += Muku2(pEm->ang.y, w->base_dir, PI) * 0.3f;
        pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        if (w->Timer != 0) {
            w->Timer--;
        } else {
            pEm->ang.y = w->base_dir;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
        }
        break;
    }
    emDoorMatUpdate(pEm);
}

// Rno1 == 4: destroyed: hides the door, hp 0, etc flag bit0; then stays as a hit-box-only work.
void emDoor_R1_Break(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    u16* flg;

    if (pEm->r_no_2 == 0) {
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        pEm->clearStatus(EM_STATUS_ACTIVE);
        emDoorSatClear(pEm);
        pEm->flag |= 0x30000000;
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        if (w->Key_flag != 0x36) {
            u32* tbl = pG->Key_flg;

            FlagOn(tbl, w->Key_flag);
        }
        pEm->r_no_2++;
    }
}

// Rno1 == 5: kicked but not opened (locked / blocked): takes 25 / 50 hp (never below 1) and
// rattles about base_dir for 7 frames.
void emDoor_R1_Shock(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    f32 r;

    switch (pEm->r_no_2) {
    case 0:
        w->Timer = 7;
        if (pEm->ckObj() == 0) {
            pEm->hp -= 25;
        } else {
            pEm->hp -= 50;
        }
        if (pEm->hp <= 0) {
            pEm->hp = 1;
        }
        pEm->r_no_2++;
    case 1:
        if (w->Timer != 0) {
            w->Timer--;
            r = fRand0_1() * (PI / 128) + PI / 128;
            if (pG->Frame_cnt & 1) {
                r = -r;
            }
            pEm->ang.y = w->base_dir + r;
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        } else {
            pEm->ang.y = w->base_dir;
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
        }
        break;
    }
    emDoorMatUpdate(pEm);
}

// Rno1 == 6: script-opened door: snaps to +-90 degrees (Rno3 side), then waits until setNormal
// clears the lock bit before returning to Set.
void emDoor_R1_OpenLock(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    f32 ang;
    int unlock;

    switch (pEm->r_no_2) {
    case 0:
        pEm->flag |= 0x30000000;
        w->Timer = 0x14;
        if (pEm->r_no_3) {
            w->Open_flag = 1;
            ang = w->base_dir;
            ang -= PI / 2;
        } else {
            w->Open_flag = 0;
            ang = w->base_dir;
            ang += PI / 2;
        }
        pEm->ang.y = ang;
        pEm->r_no_2++;
    case 1:
        unlock = !(w->Be_flg & 1);
        if (unlock) {
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 0;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
        }
        break;
    }
    emDoorMatUpdate(pEm);
}

// Rno1 == 7: script-closed door: swings back to base_dir over 20 frames, then waits for setNormal.
void emDoor_R1_CloseLock(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    int unlock;

    switch (pEm->r_no_2) {
    case 0:
        pEm->flag &= ~0x30000000;
        w->Timer = 0x14;
        pEm->r_no_2++;
    case 1:
        pEm->ang.y += Muku2(pEm->ang.y, w->base_dir, PI) * 0.3f;
        pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        if (w->Timer != 0) {
            w->Timer--;
        } else {
            pEm->ang.y = w->base_dir;
            unlock = !(w->Be_flg & 1);
            if (unlock) {
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 0;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            }
        }
        break;
    }
    emDoorMatUpdate(pEm);
}

// Applies the lock / chain bend angles (set by hits and kicks) to the hanging objects' parts and
// decays them by 0.7 per frame.
void emDoorLockBendMove(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    cModel* parts;

    if (w->pLockL) {
        parts = w->pLockL->getPartsPtr(1);
        parts->ang.x = w->Lock_L_bend;
        w->Lock_L_bend *= 0.7f;
    }
    if (w->pLockR) {
        parts = w->pLockR->getPartsPtr(1);
        parts->ang.x = w->Lock_R_bend;
        w->Lock_R_bend *= 0.7f;
    }
    if (w->pChain) {
        parts = w->pChain->getPartsPtr(1);
        parts->ang.x = w->Chain_bend;
        parts = w->pChain->getPartsPtr(2);
        parts->ang.x = w->Chain_bend;
        parts = w->pChain->getPartsPtr(3);
        parts->ang.x = w->Chain_bend;
        w->Chain_bend *= 0.7f;
    }
}

// Four corners of an effect collision panel: x0..x1 along the door, at height y, 50 deep.
#define SAT_POLY(X0, X1, Y)      \
    poly[0].x = X0;              \
    poly[0].y = Y;               \
    poly[0].z = -50.0f;          \
    poly[1].x = X1;              \
    poly[1].y = Y;               \
    poly[1].z = -50.0f;          \
    poly[2].x = X1;              \
    poly[2].y = Y;               \
    poly[2].z = 50.0f;           \
    poly[3].x = X0;              \
    poly[3].y = Y;               \
    poly[3].z = 50.0f

// Keeps the intact door's effect collision panels in place: [1] the door leaf (attribute and
// height by type), [2] the panel above it (unless the upper panes are broken), [3..5] the pane
// panels of types 4 / 5 (skipped once those panes are gone).
void emDoorSatSet(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    Vec poly[4];
    f32 h;
    int attr;

    emDoorSatClear(pEm);
    if (pEm->hp <= 0) {
        return;
    }
    pEm->atari.setFlag200();
    switch (pEm->type) {
    case 1:
    case 4:
    case 5:
        attr = 0;
        break;
    case 2:
        attr = 0x404000;
        break;
    case 7:
        attr = 0x400000;
        break;
    default:
        attr = 0x400000;
        break;
    }
    if (w->pSat[1] == 0) {
        switch (pEm->type) {
        case 4:
            SAT_POLY(-w->Width * 2.0f, 0.0f, 0.0f);
            h = 1300.0f;
            break;
        case 5:
            SAT_POLY(-w->Width * 2.0f, 0.0f, 0.0f);
            h = 1350.0f;
            break;
        default:
            SAT_POLY(-w->Width * 2.0f, 0.0f, 0.0f);
            h = 800.0f;
            break;
        }
        w->pSat[1] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
    } else {
        w->pSat[1]->setEnable();
        w->pSat[1]->setCoord(&pEm->pos, &pEm->ang);
    }
    if (!(pEm->flag & 0x7500)) {
        if (w->pSat[2] == 0) {
            switch (pEm->type) {
            case 4:
                SAT_POLY(-w->Width * 2.0f, 0.0f, 1880.0f);
                h = w->Height - 1880.0f;
                break;
            case 5:
                SAT_POLY(-w->Width * 2.0f, 0.0f, 1800.0f);
                h = w->Height - 1800.0f;
                break;
            default:
                SAT_POLY(-w->Width * 2.0f, 0.0f, 800.0f);
                h = 600.0f;
                break;
            }
            w->pSat[2] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
        } else {
            w->pSat[2]->setEnable();
            w->pSat[2]->setCoord(&pEm->pos, &pEm->ang);
        }
    }
    if (!(pEm->flag & 0x8AC0)) {
        if (w->pSat[3] == 0) {
            switch (pEm->type) {
            case 4:
                SAT_POLY(-w->Width * 2.0f + 320.0f, -320.0f, 1300.0f);
                h = 580.0f;
                w->pSat[3] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
                break;
            case 5:
                SAT_POLY(-w->Width * 2.0f + 150.0f, -150.0f, 1350.0f);
                h = 500.0f;
                w->pSat[3] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, 0x404000, 0);
                break;
            default:
                SAT_POLY(-w->Width * 2.0f, 0.0f, 1400.0f);
                h = w->Height - 1400.0f;
                w->pSat[3] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
                break;
            }
        } else {
            w->pSat[3]->setEnable();
            w->pSat[3]->setCoord(&pEm->pos, &pEm->ang);
        }
    }
    if (w->pSat[4] == 0) {
        switch (pEm->type) {
        case 4:
            SAT_POLY(-w->Width * 2.0f, -w->Width * 2.0f + 320.0f, 1300.0f);
            h = 580.0f;
            w->pSat[4] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
            break;
        case 5:
            SAT_POLY(-w->Width * 2.0f, -w->Width * 2.0f + 150.0f, 1300.0f);
            h = 580.0f;
            w->pSat[4] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
            break;
        }
    } else {
        w->pSat[4]->setEnable();
        w->pSat[4]->setCoord(&pEm->pos, &pEm->ang);
    }
    if (w->pSat[5] == 0) {
        switch (pEm->type) {
        case 4:
            SAT_POLY(-320.0f, 0.0f, 1300.0f);
            h = 580.0f;
            w->pSat[5] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
            break;
        case 5:
            SAT_POLY(-150.0f, 0.0f, 1300.0f);
            h = 580.0f;
            w->pSat[5] = EatMgr.create(&pEm->pos, &pEm->ang, poly, h, attr, 0);
            break;
        }
    } else {
        w->pSat[5]->setEnable();
        w->pSat[5]->setCoord(&pEm->pos, &pEm->ang);
    }
}

// Deactivates every effect collision panel of the door.
void emDoorSatClear(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);

    pEm->atari.clrFlag200();
    if (w->pSat[1]) {
        w->pSat[1]->setDisable();
    }
    if (w->pSat[2]) {
        w->pSat[2]->setDisable();
    }
    if (w->pSat[3]) {
        w->pSat[3]->setDisable();
    }
    if (w->pSat[4]) {
        w->pSat[4]->setDisable();
    }
    if (w->pSat[5]) {
        w->pSat[5]->setDisable();
    }
}

// Positions the left / right lock and the chain objects on the door (fixed door-space offsets).
void emDoorLockMove(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    Vec v;
    Vec rot;

    if (w->pLockL) {
        v.x = -1160.0f;
        v.y = 1050.0f;
        v.z = 80.0f;
        PSMTXMultVec(pEm->mat, &v, &v);
        w->pLockL->pos = v;
        w->pLockL->ang = pEm->ang;
    }
    if (w->pLockR) {
        rot = pEm->ang;
        rot.y += PI;
        rot.y = LIMIT_ANGLE(rot.y);
        v.x = -1160.0f;
        v.y = 1050.0f;
        v.z = -80.0f;
        PSMTXMultVec(pEm->mat, &v, &v);
        w->pLockR->pos = v;
        w->pLockR->ang = rot;
    }
    if (w->pChain) {
        rot = pEm->ang;
        v.x = -650.0f;
        v.y = 1550.0f;
        v.z = 30.0f;
        PSMTXMultVec(pEm->mat, &v, &v);
        w->pChain->pos = v;
        w->pChain->ang = rot;
    }
}

// `v` (in the closed door's space) is inside the door's passage box.
#define NEAR_CK(X, LIM)                                    \
    ok = 1;                                                \
    if (v.z > 1500.0f) {                                   \
        ok = 0;                                            \
    }                                                      \
    if (v.z < -1500.0f) {                                  \
        ok = 0;                                            \
    }                                                      \
    if (v.y > 500.0f) {                                    \
        ok = 0;                                            \
    }                                                      \
    if (v.y < -500.0f) {                                   \
        ok = 0;                                            \
    }                                                      \
    X = v.x;                                               \
    if (X > w->Width + 150.0f) {                           \
        ok = 0;                                            \
    }                                                      \
    if (w->pDoor) {                                        \
        if (X < -(w->Width * 3.0f + 150.0f)) {             \
            ok = 0;                                        \
        }                                                  \
    } else {                                               \
        if (X < -(w->Width + 150.0f)) {                    \
            ok = 0;                                        \
        }                                                  \
    }

// Open door auto-close: 1 (and Close started) when neither the player, the partner nor any live
// character stands within the door's swing area (in base_mat space) and no object blocks it;
// 0 while something is in the way or the door is not open.
int emDoorDoorAutoCloseCk(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    Vec v;
    f32 x0;
    f32 lim0;
    f32 x1;
    f32 lim1;
    f32 x2;
    f32 lim2;
    int ok;
    u32 i;

    if (!(pEm->flag & 0x20000000)) {
        return 0;
    }
    PSMTXMultVec(w->base_im, &pPL->pos, &v);
    NEAR_CK(x0, lim0);
    if (ok) {
        return 0;
    }
    if (pSUB) {
        PSMTXMultVec(w->base_im, &pSUB->pos, &v);
        NEAR_CK(x1, lim1);
        if (ok) {
            return 0;
        }
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->checkStatus(EM_STATUS_ACTIVE) == 0) {
            continue;
        }
        switch (e->id) {
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
            continue;
        }
        if (e == pEm) {
            continue;
        }
        PSMTXMultVec(w->base_im, &e->pos, &v);
        NEAR_CK(x2, lim2);
        if (ok) {
            return 0;
        }
    }
    if (pEm->ckObj() == 0) {
        return 0;
    }
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 3;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
    pEm->flag &= ~0x20000000;
    return 1;
}

// Hit boxes by type: the whole leaf, the top board and both side posts (sizes per type); the
// falling door (2) and type 7 have none.
void emDoorYarareInit(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    u16 flags;

    if (pEm->type == 2) {
        return;
    }
    if (pEm->type == 7) {
        return;
    }
    if (pEm->type == 0) {
        flags = 0x21;
    } else {
        flags = 0x41;
    }
    switch (pEm->type) {
    default:
        YarareInitCube(pEm, -w->Width, 0.0f, 0.0f, w->Width, w->Height, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[0], -w->Width, w->Height - 300.0f, 0.0f, w->Width, 300.0f, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[1], -100.0f, 0.0f, 0.0f, 100.0f, w->Height, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[2], -(w->Width * 2.0f - 100.0f), 0.0f, 0.0f, 100.0f, w->Height, 55.0f, 0, flags);
        break;
    case 4:
        YarareInitCube(pEm, -w->Width, 0.0f, 0.0f, w->Width, w->Height - 1000.0f, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[0], -w->Width, w->Height - 370.0f, 0.0f, w->Width, 370.0f, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[1], -170.0f, 0.0f, 0.0f, 170.0f, w->Height, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[2], -(w->Width * 2.0f - 170.0f), 0.0f, 0.0f, 170.0f, w->Height, 55.0f, 0, flags);
        break;
    case 5:
        YarareInitCube(pEm, -w->Width, 0.0f, 0.0f, w->Width, 1350.0f, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[0], -w->Width, w->Height - 600.0f, 0.0f, w->Width, 600.0f, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[1], -75.0f, 0.0f, 0.0f, 75.0f, w->Height, 55.0f, 0, flags);
        YarareAddCube(pEm, &w->hit[2], -(w->Width * 2.0f - 75.0f), 0.0f, 0.0f, 75.0f, w->Height, 55.0f, 0, flags);
        break;
    }
}

// Hangs a lock object (cObj12) on the left (side 1) or right (side 0) of the door with its hit
// box and 3..4 hp (strong: 15 hp, Be_flg bit1); skipped when the etc flag says it is already
// broken.
void cEmDoor::setLock(void* bin, void* tpl, int side, int strong)
{
    EmDoorWork* w = EMDOOR_WK(this);
    u16* flg;
    Mtx m;
    Vec v;
    Vec r;

    flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
    if (hp <= 0) {
        return;
    }
    if (side) {
        if (flg && (*flg & 4)) {
            return;
        }
        lockOfs.x = -1160.0f;
        lockOfs.y = 1050.0f;
        lockOfs.z = 150.0f;
        flag |= 0x80000000;
        lockParts = 0;
        clearStatus(EM_STATUS_LOCKOFF);
        v.x = -1160.0f;
        v.y = 1050.0f;
        v.z = 80.0f;
        RotMatrix(m, &ang);
        TransMatrix(m, &pos);
        ScaleMatrix(m, &scale);
        PSMTXMultVec(m, &v, &v);
        w->pLockL = SetObj12(bin, tpl, &v, &ang);
        if (w->pLockL) {
            w->pLockL->LightInfo.EnableMask = 0x10;
            w->pLockL->setNoSuspend(1);
        }
        YarareAddCube(this, &w->hit[12], -1150.0f, 800.0f, 60.0f, 150.0f, 350.0f, 100.0f, 0, YAT_FLAG_ON);
        w->Lock_L_hp = (Rnd() & 1) + 3;
        if (strong) {
            w->Lock_L_hp = 0xF;
            w->Be_flg |= 2;
        }
    } else {
        if (flg && (*flg & 2)) {
            return;
        }
        lockOfs.x = -1160.0f;
        lockOfs.y = 1050.0f;
        lockOfs.z = -150.0f;
        flag |= 0x40000000;
        lockParts = 0;
        clearStatus(EM_STATUS_LOCKOFF);
        v.x = -1160.0f;
        v.y = 1050.0f;
        v.z = -80.0f;
        RotMatrix(m, &ang);
        TransMatrix(m, &pos);
        ScaleMatrix(m, &scale);
        PSMTXMultVec(m, &v, &v);
        r = ang;
        r.y += PI;
        r.y = LIMIT_ANGLE(r.y);
        w->pLockR = SetObj12(bin, tpl, &v, &r);
        if (w->pLockR) {
            w->pLockR->LightInfo.EnableMask = 4;
            w->pLockR->setNoSuspend(1);
        }
        YarareAddCube(this, &w->hit[11], -1150.0f, 800.0f, -60.0f, 150.0f, 350.0f, 100.0f, 0, YAT_FLAG_ON);
        w->Lock_R_hp = (Rnd() & 1) + 3;
        if (strong) {
            w->Lock_R_hp = 0xF;
            w->Be_flg |= 2;
        }
    }
}

// 1 while a lock still holds (left or right hp > 0).
int cEmDoor::ckLock()
{
    EmDoorWork* w = EMDOOR_WK(this);

    if (w->pLockL) {
        return 1;
    }
    if (w->pLockR) {
        return 1;
    }
    return 0;
}

// Hangs the chain object with its three links (hit boxes 13..15, 2 hp each); links already
// broken in the etc flag start hidden.
void cEmDoor::setChain(void* bin, void* tpl)
{
    EmDoorWork* w = EMDOOR_WK(this);
    u16* flg;
    Mtx m;
    Vec v;

    if (hp <= 0) {
        return;
    }
    flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
    v.x = -650.0f;
    v.y = 1550.0f;
    v.z = 30.0f;
    RotMatrix(m, &ang);
    TransMatrix(m, &pos);
    ScaleMatrix(m, &scale);
    PSMTXMultVec(m, &v, &v);
    w->pChain = SetObj12(bin, tpl, &v, &ang);
    if (w->pChain == 0) {
        return;
    }
    w->pChain->LightInfo.EnableMask = 0x10;
    w->pChain->setNoSuspend(1);
    if (flg) {
        if (!(*flg & 8)) {
            YarareAddCube(this, &w->hit[13], -650.0f, 1350.0f, 50.0f, 650.0f, 350.0f, 100.0f, 0, YAT_FLAG_ON);
            w->Chain_hp[0] = 2;
        } else {
            cModel* parts = w->pChain->getPartsPtr(1);

            parts->scale.x = 0.0f;
            parts->scale.y = 0.0f;
            parts->scale.z = 0.0f;
        }
        if (!(*flg & 0x10)) {
            YarareAddCube(this, &w->hit[14], -650.0f, 1000.0f, 50.0f, 650.0f, 350.0f, 100.0f, 0, YAT_FLAG_ON);
            w->Chain_hp[1] = 2;
        } else {
            cModel* parts = w->pChain->getPartsPtr(2);

            parts->scale.x = 0.0f;
            parts->scale.y = 0.0f;
            parts->scale.z = 0.0f;
        }
        if (!(*flg & 0x20)) {
            YarareAddCube(this, &w->hit[15], -650.0f, 700.0f, 50.0f, 650.0f, 300.0f, 100.0f, 0, YAT_FLAG_ON);
            w->Chain_hp[2] = 2;
        } else {
            cModel* parts = w->pChain->getPartsPtr(3);

            parts->scale.x = 0.0f;
            parts->scale.y = 0.0f;
            parts->scale.z = 0.0f;
        }
    }
}

// Effect owner id used for the door's splinter / spark / break ests.
void cEmDoor::setEff(u8 eff_id)
{
    EMDOOR_WK(this)->Eff_id = eff_id;
}

// Pane `no` (parts no - 1, hit box hit[no + 1]) of a wooden door: a hit box while whole, hidden when broken.
#define PANE_SET(no)                                                                          \
    if (!(flag & bit)) {                                                                 \
        YarareAddCube(this, &w->hit[(no) + 1], 0.0f, -300.0f, 0.0f, 300.0f, 600.0f, 65.0f, no, flags); \
    } else {                                                                                  \
        parts = getPartsPtr((no) - 1);                                                        \
        parts->scale.x = 0.0f;                                                                \
        parts->scale.y = 0.0f;                                                                \
        parts->scale.z = 0.0f;                                                                \
    }

// Adds the pane hit boxes (parts 2..9 -> hit[3..10]) for the wooden and type 4 doors; panes
// already broken in the flag word are hidden and disabled.
void cEmDoor::setYarare()
{
    EmDoorWork* w = EMDOOR_WK(this);
    u16 flags;
    u32 bit;
    u16* flg;
    cModel* parts;

    if (type == 0 || type == 4) {
        flags = 0x21;
    } else {
        flags = 0x41;
    }
    if (type != 4) {
        hitInfo.height = 500.0f;
        bit = 0x8000;
        PANE_SET(2);
        bit >>= 1;
        PANE_SET(3);
        bit >>= 1;
        PANE_SET(4);
        bit >>= 1;
        PANE_SET(5);
        bit >>= 1;
        PANE_SET(6);
        bit >>= 1;
        PANE_SET(7);
        bit >>= 1;
        PANE_SET(8);
        bit >>= 1;
        PANE_SET(9);
    } else {
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        parts = getPartsPtr(1);
        YarareAddCube(this, &w->hit[3], 0.0f, -350.0f, 0.0f, 350.0f, 700.0f, 65.0f, 2, flags);
        parts->scale.x = 1.0f;
        parts->scale.y = 1.0f;
        parts->scale.z = 1.0f;
        if (flg && (*flg & 8)) {
            w->hit[3].flag &= ~1;
            parts->scale.x = 0.0f;
            parts->scale.y = 0.0f;
            parts->scale.z = 0.0f;
            flag |= 0xFFC0;
        }
    }
}

// Can the door be opened? 0 yes, 1 already open / broken, 2 an object stands in it, 3 locked
// (script lock, a lock / chain still holds, or the key is missing).
u32 cEmDoor::ckOpen()
{
    EmDoorWork* w = EMDOOR_WK(this);

    if (hp <= 0) {
        return 1;
    }
    if (w->Be_flg & 1) {
        return 3;
    }
    if (flag & 0x20000000) {
        return 1;
    }
    if (w->Lock_L_hp > 0) {
        return 3;
    }
    if (w->Lock_R_hp > 0) {
        return 3;
    }
    if (w->Chain_hp[0] > 0) {
        return 3;
    }
    if (w->Chain_hp[1] > 0) {
        return 3;
    }
    if (w->Chain_hp[2] > 0) {
        return 3;
    }
    if (ckObj() == 0) {
        return 2;
    }
    if (w->Key_flag != 0x36 && emDoorKeyCk(w->Key_flag) == 0) {
        return 3;
    }
    return 0;
}

// 1 when a kick from `pos` would open the door: key owned, at most one lock hp and one chain hp
// left.
int cEmDoor::ckKick(Vec* pPos)
{
    EmDoorWork* w = EMDOOR_WK(this);
    Mtx m;
    Vec v;

    if (w->Key_flag != 0x36 && emDoorKeyCk(w->Key_flag) == 0) {
        return 0;
    }
    if (w->Lock_L_hp > 1) {
        return 0;
    }
    if (w->Lock_R_hp > 1) {
        return 0;
    }
    if (w->Chain_hp[0] + w->Chain_hp[1] + w->Chain_hp[2] > 1) {
        return 0;
    }
    PSMTXRotRad(m, 'y', w->base_dir);
    TransMatrix(m, &this->pos);
    v.x = -1000.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(m, &v, &v);
    return 1;
}

// Kicks the door open away from `pos`: types 2 / 3 (or down_ck) fall (Down), others swing (Open);
// mode 1 = the paired-door / silent variant (Rno3); se_off skips the open SE.
void cEmDoor::setOpen(Vec* pPos, int mode, int se_off, int down_ck)
{
    EmDoorWork* w = EMDOOR_WK(this);

    if (hp <= 0) {
        return;
    }
    w->Open_pos = *pPos;
    switch (type) {
    case 2:
    case 3:
        if (mode) {
            r_no_0 = 1;
            r_no_1 = 8;
            r_no_2 = 0;
            r_no_3 = 1;
        } else {
            r_no_0 = 1;
            r_no_1 = 8;
            r_no_2 = 0;
            r_no_3 = 0;
        }
        break;
    default:
        if (down_ck) {
        if (mode) {
            r_no_0 = 1;
            r_no_1 = 8;
            r_no_2 = 0;
            r_no_3 = 1;
        } else {
            r_no_0 = 1;
            r_no_1 = 8;
            r_no_2 = 0;
            r_no_3 = 0;
        }
        } else {
            if (mode) {
                r_no_0 = 1;
                r_no_1 = 1;
                r_no_2 = 0;
                r_no_3 = 1;
            } else {
                r_no_0 = 1;
                r_no_1 = 1;
                r_no_2 = 0;
                r_no_3 = 0;
            }
        }
        break;
    }
    flag |= 0x20000000;
    emDoorSetDmgLock_L(this, 1);
    emDoorSetDmgLock_R(this, 1);
    emDoorSetDmgChain(this, 0);
    emDoorSetDmgChain(this, 1);
    emDoorSetDmgChain(this, 2);
    if (se_off == 0) {
        switch (type) {
        case 0:
        default:
            if (mode) {
                SndCall(1, 0x1E, &this->pos, 0, 0, this);
            } else {
                SndCall(1, 0x19, &this->pos, 0, 0, this);
            }
            break;
        case 1:
        case 4:
        case 5:
        case 7:
            if (mode) {
                SndCall(1, 0x20, &this->pos, 0, 0, this);
            } else {
                SndCall(1, 0x1C, &this->pos, 0, 0, this);
            }
            break;
        case 2:
            if (mode) {
                SndCall(6, 0x17, &this->pos, 0, 0, this);
            } else {
                SndCall(1, 0x1C, &this->pos, 0, 0, this);
            }
            break;
        case 3:
            if (mode) {
                SndCall(6, 0x2C, &this->pos, 0, 0, this);
            } else {
                SndCall(6, 0x2C, &this->pos, 0, 0, this);
            }
            break;
        }
    }
}

// Opens the door by hand with the motion (Open2); type 0..3 selects the side / mirrored motion.
void cEmDoor::setOpen2(int type)
{
    int near;

    if (hp <= 0) {
        return;
    }
    near = 0;
    if (pSUB && pSUB->l_pl < 25000000.0f) {
        near = 1;
    }
    if (near) {
        if (type) {
            r_no_0 = 1;
            r_no_1 = 2;
            r_no_2 = 0;
            r_no_3 = 3;
        } else {
            r_no_0 = 1;
            r_no_1 = 2;
            r_no_2 = 0;
            r_no_3 = 2;
        }
    } else {
        if (type) {
            r_no_0 = 1;
            r_no_1 = 2;
            r_no_2 = 0;
            r_no_3 = 1;
        } else {
            r_no_0 = 1;
            r_no_1 = 2;
            r_no_2 = 0;
            r_no_3 = 0;
        }
    }
    flag |= 0x20000000;
    emDoorSetDmgLock_L(this, 1);
    emDoorSetDmgLock_R(this, 1);
    emDoorSetDmgChain(this, 0);
    emDoorSetDmgChain(this, 1);
    emDoorSetDmgChain(this, 2);
}

// A kick from `pos` that does not open the door: bends the lock on that side and the chain
// (rattle ests / SE), mode 1 then plays the kick SE and the Shock rattle; mode 2 only bends.
void cEmDoor::setShock(int mode, Vec* pPos, int se_off)
{
    EmDoorWork* w = EMDOOR_WK(this);
    f32 ang;
    u32 i;
    u32 no;

    if (hp <= 0) {
        return;
    }
    ang = fabsf(Muku(&this->pos, pPos, w->base_dir, PI));
    if (ang < PI / 2) {
        if (w->pLockL) {
            EstSet(w->pLockL, -1, 0, 0, EFF_OBM2B, 1, 0, ESP_CORE_KIND_NONE, w->pLockL, 0);
            w->Lock_L_bend = -PI / 2;
        }
    } else {
        if (w->pLockR) {
            EstSet(w->pLockR, -1, 0, 0, EFF_OBM2B, 1, 0, ESP_CORE_KIND_NONE, w->pLockR, 0);
            w->Lock_R_bend = -PI / 2;
        }
    }
    if (w->pChain) {
        for (i = 0; i <= 2; i++) {
            if (w->Chain_hp[i] > 0) {
                switch (i) {
                case 0:
                    EstSet(w->pChain, -1, 0, 0, EFF_OBM4C, 4, 0, ESP_CORE_KIND_NONE, w->pChain, 0);
                    break;
                case 1:
                    EstSet(w->pChain, -1, 0, 0, EFF_OBM4C, 5, 0, ESP_CORE_KIND_NONE, w->pChain, 0);
                    break;
                case 2:
                    EstSet(w->pChain, -1, 0, 0, EFF_OBM4C, 6, 0, ESP_CORE_KIND_NONE, w->pChain, 0);
                    break;
                }
                break;
            }
        }
        w->Chain_bend = -PI / 2;
        if (mode == 2) {
            return;
        }
        SndCall(6, 0x15, &this->pos, 0, 0, this);
    }
    if (mode == 2) {
        return;
    }
    if (ang < PI / 2) {
        if (w->pLockL) {
            if (w->Lock_L_hp > 1) {
                w->Lock_L_hp--;
                if ((Rnd() & 1) && !(w->Be_flg & 2) && w->Lock_L_hp > 1) {
                    w->Lock_L_hp--;
                }
            }
            EstSet(w->pLockL, -1, 0, 0, EFF_OBM2B, 1, 0, ESP_CORE_KIND_NONE, w->pLockL, 0);
            w->Lock_L_bend = -PI / 2;
        }
    } else {
        if (w->pLockR) {
            if (w->Lock_R_hp > 1) {
                w->Lock_R_hp--;
                if ((Rnd() & 1) && !(w->Be_flg & 2) && w->Lock_R_hp > 1) {
                    w->Lock_R_hp--;
                }
            }
            EstSet(w->pLockR, -1, 0, 0, EFF_OBM2B, 1, 0, ESP_CORE_KIND_NONE, w->pLockR, 0);
            w->Lock_R_bend = -PI / 2;
        }
    }
    if (w->pChain) {
        for (no = 0; no <= 2; no++) {
            if (w->Chain_hp[no] > 0) {
                w->Chain_hp[no]--;
                if (w->Chain_hp[no] <= 0) {
                    w->Chain_hp[no] = 1;
                    emDoorSetDmgChain(this, no);
                }
                break;
            }
        }
    }
    if (se_off == 0) {
        switch (type) {
        case 0:
        default:
            if (mode) {
                SndCall(1, 0x1D, &this->pos, 0, 0, this);
            } else {
                SndCall(1, 0x18, &this->pos, 0, 0, this);
            }
            break;
        case 1:
        case 2:
        case 4:
        case 5:
        case 7:
            if (mode) {
                SndCall(1, 0x1F, &this->pos, 0, 0, this);
            } else {
                SndCall(1, 0x1B, &this->pos, 0, 0, this);
            }
            break;
        case 3:
            if (mode) {
                SndCall(6, 0x2B, &this->pos, 0, 0, this);
            } else {
                SndCall(6, 0x2B, &this->pos, 0, 0, this);
            }
            break;
        }
        if (w->pLockR || w->pLockL) {
            SndCall(6, 0x5D, &this->pos, 0, 0, this);
        }
    }
    r_no_0 = 1;
    r_no_1 = 5;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Destroys the door from `pos` (explosion): break est toward it and Break.
void cEmDoor::setBreak(Vec* pPos)
{
    EmDoorWork* w = EMDOOR_WK(this);
    int zero;

    if (hp <= 0) {
        return;
    }
    emDoorSetDmgLock_L(this, 1);
    emDoorSetDmgLock_R(this, 1);
    emDoorSetDmgChain(this, 0);
    emDoorSetDmgChain(this, 1);
    emDoorSetDmgChain(this, 2);
    switch (type) {
    case 2:
    case 3:
        setOpen(pPos, 0, 0, 0);
        return;
    }
    zero = 0;
    EstSet(this, -1, 0, 0, w->Eff_id, 6, 0, ESP_CORE_KIND_NONE, this, (void*) zero);
    SndCall(6, 0x37, &this->pos, 0, 0, this);
    hp = zero;
    r_no_0 = 1;
    r_no_1 = 4;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Offers action button 0x10 (open / kick) when the player stands in front of the closed door,
// roughly facing it, with the key (if any), within the door's reach box; a double door offers
// the partner's action when the player stands on its half.
void emDoorActEvtCk(cEmDoor* pEm)
{
    EmDoorWork* w = EMDOOR_WK(pEm);
    f32 ang;
    f32 lim;
    Vec v;

    if (pEm->flag & 0x20000000) {
        return;
    }
    if (pEm->hp <= 0) {
        return;
    }
    if (w->Be_flg & 1) {
        return;
    }
    if (w->Key_flag != 0x36 && emDoorKeyCk(w->Key_flag) == 0) {
        return;
    }
    ang = fabsf(Muku2(w->base_dir, pPL->ang.y, PI));
    if (ang > PI / 4) {
        if (ang < PI * 3 / 4) {
            return;
        }
    }
    PSMTXMultVec(w->base_im, &pPL->pos, &v);
    if (ang < PI / 2) {
        if (v.z > 0.0f) {
            return;
        }
        if (v.z < -800.0f) {
            return;
        }
        if (v.x > w->Width) {
            return;
        }
        if (w->pDoor) {
            lim = -(w->Width + 250.0f);
        } else {
            lim = -w->Width;
        }
    } else {
        if (v.z < 0.0f) {
            return;
        }
        if (v.z > 800.0f) {
            return;
        }
        if (w->pDoor) {
            if (v.x > w->Width + 250.0f) {
                return;
            }
        } else {
            if (v.x > w->Width) {
                return;
            }
        }
        lim = -w->Width;
    }
    if (v.x < lim) {
        return;
    }
    if (v.y > 500.0f) {
        return;
    }
    if (v.y < -500.0f) {
        return;
    }
    if (w->pDoor && v.x < -250.0f && w->pDoor->ckOpen() == 0) {
        ActBtn.set(ACT_OPEN, 5, (void*) emDoorAction2, pEm, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    } else {
        ActBtn.set(ACT_OPEN, 5, (void*) emDoorAction, pEm, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
}

// Action button callback: a locked / blocked / falling-type door is kicked (plemDoorKick), an
// openable one opened by hand (plemDoorOpen).
void emDoorAction(cEmDoor* ptr)
{
    EmDoorWork* w = EMDOOR_WK(ptr);
    int kick;

    kick = 0;
    if (ptr->ckOpen()) {
        kick = 1;
    }
    if (w->pDoor && w->pDoor->ckOpen() > 1) {
        kick = 1;
    }
    switch (ptr->type) {
    case 2:
        kick = 1;
        break;
    case 3:
        kick = 1;
        break;
    }
    if (kick) {
        SetPlDamage(ptr, plemDoorKick);
    } else {
        SetPlDamage(ptr, plemDoorOpen);
    }
}

// Same for the partner half of a double door (flags Status_flg[1] 0x20000000, a noise).
void emDoorAction2(cEmDoor* ptr)
{
    EmDoorWork* w = EMDOOR_WK(ptr);
    int kick;

    kick = 0;
    if (ptr->ckOpen()) {
        kick = 1;
    }
    if (w->pDoor && w->pDoor->ckOpen() > 1) {
        kick = 1;
    }
    switch (ptr->type) {
    case 2:
        kick = 1;
        break;
    case 3:
        kick = 1;
        break;
    }
    if (kick) {
        SetPlDamage(ptr, plemDoorKick);
        pPL->r_no_3 = 1;
    } else {
        SetPlDamage(ptr, plemDoorOpen);
        pPL->r_no_3 = 1;
    }
}

// The bell position marks where the door was kicked / opened (pG->SeInfo.pos).
static inline void emDoorBellSet(Vec* pos)
{
    StaFlagOn(pG, STA_SE_BURST);
    pG->SeInfo.pos = *pos;
    pG->SeInfo.type = 0;
}

// Player damage routine of the kick: a kickable door plays the kick-open motion (0x1C) and
// setOpen at frame 14, otherwise the blocked-kick motion (0x1D) with setShock at frames 14 / 17;
// the paired door follows. Ends the damage state when the motion finishes.
void plemDoorKick(cPlayer* pEm)
{
    cEmDoor* door = (cEmDoor*) pEm->pEmCatch;
    cEmDoor* door2 = (cEmDoor*) pPL->pEmCatch;
    EmDoorWork* w = EMDOOR_WK(door2);
    int frame;
    Vec v;

    pEm->subArc = door2->subArc;
    if (pEm->r_no_2 == 0 || pEm->r_no_2 == 4) {
        if (door->ckKick(&pPL->pos) && (w->pDoor == 0 || w->pDoor->ckKick(&pPL->pos))) {
            if (pEm->r_no_2 == 0) {
                pEm->r_no_2 = 2;
            }
            if (pEm->r_no_2 == 4) {
                pEm->r_no_2 = 6;
            }
        }
    }
    frame = 0;
    if (pEm->r_no_2 == 4) {
        pEm->r_no_2 = 0;
        frame = 6;
    }
    if (pEm->r_no_2 == 6) {
        pEm->r_no_2 = 2;
        frame = 6;
    }
    switch (pEm->r_no_2) {
    case 0:
        MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x1D), 0, 5, 1, frame);
        pEm->r_no_2++;
    case 1:
        if (pEm->Motion.Seq_frame > 13.7f && pEm->Motion.Seq_frame < 14.3f) {
            door->setShock(1, &pEm->pos, 0);
            if (pEm->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setShock(1, &pEm->pos, 1);
            }
            emDoorBellSet(&pEm->pos);
        }
        if (pEm->Motion.Seq_frame > 16.7f && pEm->Motion.Seq_frame < 17.3f) {
            door->setShock(2, &pEm->pos, 0);
            if (pEm->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setShock(2, &pEm->pos, 1);
            }
        }
        if (MotionMove(pEm, 0)) {
            EndPlDamage();
        }
        break;
    case 2:
        MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x1C), 0, 5, 1, frame);
        pEm->r_no_2++;
    case 3:
        if (pEm->Motion.Seq_frame > 13.7f && pEm->Motion.Seq_frame < 14.3f) {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -500.0f;
            PSMTXMultVec(pEm->mat, &v, &v);
            door->setOpen(&v, 1, 0, 0);
            if (pEm->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setOpen(&v, 1, 0, 0);
                w->pDoor->setSeCancel();
            }
            emDoorBellSet(&pEm->pos);
        }
        if (MotionMove(pEm, 0)) {
            EndPlDamage();
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Player damage routine of opening by hand: motion 0x1E with setOpen2 (side by which half the
// player stands on; the partner door mirrors it), then walks the player through and ends the
// damage state.
void plemDoorOpen(cPlayer* pEm)
{
    cEmDoor* door = (cEmDoor*) pEm->pEmCatch;
    cEmDoor* door2 = (cEmDoor*) pPL->pEmCatch;
    EmDoorWork* w = EMDOOR_WK(door2);
    f32 d;
    u8 flag;
    Vec v;

    pEm->subArc = door2->subArc;
    switch (pEm->r_no_2) {
    case 0:
        d = Muku2(pEm->ang.y, door->ang.y, PI);
        if (fabsf(d) > PI / 2) {
            v.x = -638.54f;
            v.y = 0.0f;
            v.z = 440.4f;
            PSMTXMultVec(pEm->pEmCatch->mat, &v, &v);
            PSVECSubtract(&v, &pPL->pos, &pEm->m_VecWork0);
            pEm->m_VecWork0.y = 0.0f;
            pEm->m_Fwork0 = pEm->pEmCatch->ang.y + PI;
            pEm->m_Fwork0 = LIMIT_ANGLE(pEm->m_Fwork0);
            MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x1E), 0, 5, 1, 0);
            door->setOpen2(0);
            if (pEm->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setOpen2(1);
                w->pDoor->setSeCancel();
            }
        } else {
            v.x = -638.54f;
            v.y = 0.0f;
            v.z = -440.4f;
            PSMTXMultVec(((cEmDoor*) pEm->pEmCatch)->mat, &v, &v);
            PSVECSubtract(&v, &pPL->pos, &pEm->m_VecWork0);
            pEm->m_VecWork0.y = 0.0f;
            pEm->m_Fwork0 = ((cEmDoor*) pEm->pEmCatch)->ang.y;
            MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x1E), 0, 5, 1, 0);
            door->setOpen2(1);
            if (pEm->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setOpen2(0);
                w->pDoor->setSeCancel();
            }
        }
        pEm->dmg.set(0, 0x26);
        pEm->atari.clrFlag200();
        pEm->m_Work0 = 0x2D;
        pEm->r_no_2++;
    case 1:
        pEm->ang.y += Muku2(pEm->ang.y, pEm->m_Fwork0, PI / 16);
        pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        if (pEm->r_no_3 == 0) {
            PSVECScale(&pEm->m_VecWork0, &v, 0.1f);
            PSVECAdd(&pEm->pos, &v, &pEm->pos);
            PSVECSubtract(&pEm->m_VecWork0, &v, &pEm->m_VecWork0);
        }
        if (MotionMove(pEm, 0)) {
            pEm->atari.setFlag200();
            EndPlDamage();
            pEm->dmg.set(0, 10);
        } else if (pEm->m_Work0 != 0) {
            pEm->m_Work0--;
            if (Key.trg & 0x400) {
                flag = pEm->r_no_3;
                SetPlDamage(pEm->pEmCatch, plemDoorKick);
                pEm->r_no_3 = flag;
                pEm->r_no_2 = 4;
                door->r_no_0 = 1;
                door->r_no_1 = 0;
                door->r_no_2 = 0;
                door->r_no_3 = 0;
                if (pEm->r_no_3 && w->pDoor) {
                    w->pDoor->r_no_0 = 1;
                    w->pDoor->r_no_1 = 0;
                    w->pDoor->r_no_2 = 0;
                    w->pDoor->r_no_3 = 0;
                }
            }
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Corner `v` of an object (in the closed door's space) is inside the door's swing box.
#define OBJ_BOX_CK()                                                                                              \
    if (v.x < width && v.x > -width && v.z < 600.0f && v.z > -600.0f && v.y < 1000.0f && v.y > -1000.0f) { \
        return 0;                                                                                                 \
    }

// Object corner into the closed door's space.
#define emDoorObjToDoor(m, inv, v) \
    PSMTXMultVec(m, v, v);         \
    PSMTXMultVec(inv, v, v)

// 0 when a rack object (id 0x45) stands inside the door's swing area, else 1.
int cEmDoor::ckObj()
{
    Vec v;
    EmDoorWork* w = EMDOOR_WK(this);
    f32 width = w->Width;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        // loop.c hoists `&EmMgr` only if threshold*savings*lifetime >= the 387-insn loop: the
        // pointer local and the two statements put luids between the high/lo_sum and their uses
        // (high life 6, lo_sum life 2 + the matched bottom-test lo_sum); the dead `rw = 0` is the
        // 396th insn that gives gcse the 199-bucket table in which mat's PRE pseudo precedes inv's.
        cEmMgr* m = &EmMgr;
        u32 ofs = m->size * i;
        cEm* e = (cEm*) ((u8*) m->pArray + ofs);
        FREE_EMRACK* rw = 0;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x45) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        {
            f32 dx = pos.x - e->pos.x;
            f32 dz = pos.z - e->pos.z;

            if (dx * dx + dz * dz > 16000000.0f) {
                continue;
            }
        }
        rw = EMRACK_WK(e);
        v.x = rw->Size_x;
        v.y = 0.0f;
        v.z = rw->Size_z;
        emDoorObjToDoor(e->mat, w->base_im, &v);
        OBJ_BOX_CK();
        v.x = rw->Size_x;
        v.y = 0.0f;
        v.z = -rw->Size_z;
        emDoorObjToDoor(e->mat, w->base_im, &v);
        OBJ_BOX_CK();
        v.x = -rw->Size_x;
        v.y = 0.0f;
        v.z = rw->Size_z;
        emDoorObjToDoor(e->mat, w->base_im, &v);
        OBJ_BOX_CK();
        v.x = -rw->Size_x;
        v.y = 0.0f;
        v.z = -rw->Size_z;
        emDoorObjToDoor(e->mat, w->base_im, &v);
        OBJ_BOX_CK();
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = rw->Size_z;
        emDoorObjToDoor(e->mat, w->base_im, &v);
        OBJ_BOX_CK();
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -rw->Size_z;
        emDoorObjToDoor(e->mat, w->base_im, &v);
        OBJ_BOX_CK();
        v.x = rw->Size_x;
        v.y = 0.0f;
        v.z = 0.0f;
        emDoorObjToDoor(e->mat, w->base_im, &v);
        OBJ_BOX_CK();
        v.x = -rw->Size_x;
        v.y = 0.0f;
        v.z = 0.0f;
        emDoorObjToDoor(e->mat, w->base_im, &v);
        OBJ_BOX_CK();
    }
    return 1;
}

// Script: opens the door to the `type` side and locks it open (Be_flg bit0) until setNormal.
void cEmDoor::setOpenLock(int type)
{
    EmDoorWork* w = EMDOOR_WK(this);

    r_no_0 = 1;
    r_no_1 = 6;
    r_no_2 = 0;
    r_no_3 = type;
    flag |= 0x20000000;
    w->Be_flg |= 1;
}

// Script: closes the door and locks it closed until setNormal.
void cEmDoor::setCloseLock()
{
    EmDoorWork* w = EMDOOR_WK(this);

    r_no_0 = 1;
    r_no_1 = 7;
    r_no_2 = 0;
    r_no_3 = 0;
    flag &= ~0x20000000;
    w->Be_flg |= 1;
}

// Script: snaps the door shut at once.
void cEmDoor::setClose()
{
    EmDoorWork* w = EMDOOR_WK(this);

    ang.y = w->base_dir;
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
    emDoorMatUpdate(this);
}

// Script: lays the door flat in direction `dir` and records it in the etc flag.
void cEmDoor::setDowned(int type)
{
    EmDoorWork* w = EMDOOR_WK(this);
    u16* flg;

    w->Open_flag = type;
    flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
    if (flg) {
        *flg |= 1;
        if (w->Open_flag == 0) {
            *flg |= 0x40;
        } else {
            *flg |= 0x80;
        }
    }
    r_no_0 = 1;
    r_no_1 = 9;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Script: releases the script lock (Be_flg bit0).
void cEmDoor::setNormal()
{
    EMDOOR_WK(this)->Be_flg &= ~1;
}

// The pG->Key_flg key bit required to open the door (0x36 = none).
void cEmDoor::setKey(int no)
{
    EMDOOR_WK(this)->Key_flag = no;
}

// The intact door `m` (an NPC / partner) stands in front of and faces, within its reach box;
// NULL when none.
cEmDoor* DoorOpenCk(cModel* m)
{
    Vec v;
    f32 ang;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEmDoor* em = (cEmDoor*) EmMgr.fastAt(i);
        EmDoorWork* w;

        if ((em->be_flag & 0x201) != 1) {
            continue;
        }
        if (em->id != 0x41) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        {
            f32 dx = m->pos.x - em->pos.x;
            f32 dy = m->pos.y - em->pos.y;
            f32 dz = m->pos.z - em->pos.z;

            if (dx * dx + dy * dy + dz * dz > 4000000.0f) {
                continue;
            }
        }
        w = EMDOOR_WK(em);
        ang = fabsf(Muku2(w->base_dir, m->ang.y, PI));
        if (ang > PI / 4) {
            if (ang < PI * 3 / 4) {
                continue;
            }
        }
        PSMTXMultVec(w->base_im, &m->pos, &v);
        if (ang < PI / 2) {
            if (v.z > 0.0f) {
                continue;
            }
            if (v.z < -800.0f) {
                continue;
            }
        } else {
            if (v.z < 0.0f) {
                continue;
            }
            if (v.z > 800.0f) {
                continue;
            }
        }
        if (v.x > w->Width) {
            continue;
        }
        if (v.x < -w->Width) {
            continue;
        }
        if (v.y > 500.0f) {
            continue;
        }
        if (v.y < -500.0f) {
            continue;
        }
        switch (em->ckOpen()) {
        case 0:
            return em;
        case 1:
            break;
        case 2:
            return em;
        case 3:
            return em;
        default:
            return em;
        }
    }
    return 0;
}

// Makes the partner open / kick `door` (subDoorKick as her damage routine).
void SubOpenDoorSet(cEmDoor* pDoor)
{
    SetSubDamage(pDoor, subDoorKick);
}

// Partner damage routine: kick motion (0x2B) with setShock, or the kick-open motion (0x2A) with
// setOpen when the door can be kicked open; ends when the motion finishes.
void subDoorKick()
{
    cSubChar* sub = SUB_CHAR();
    cEmDoor* door = (cEmDoor*) sub->pEmCatch;

    if (sub->r_no_2 == 0) {
        if (door->ckKick(&sub->pos)) {
            sub->r_no_2 = 2;
        }
    }
    switch (sub->r_no_2) {
    case 0:
        MotionSetCore(sub, &sub->Motion, PL_ARC_PTR(sub->subArc, 0x2B), 0, 5, 1, 0);
        sub->m_Work0 = 0xE;
        sub->r_no_2++;
    case 1:
        if (sub->m_Work0 != 0) {
            sub->m_Work0--;
            if (sub->m_Work0 == 0) {
                door->setShock(1, &sub->pos, 0);
                emDoorBellSet(&sub->pos);
            }
        }
        if (MotionMove(sub, 0)) {
            EndSubDamage();
            sub->dmg.set(0, 0x1E);
        }
        break;
    case 2:
        MotionSetCore(sub, &sub->Motion, PL_ARC_PTR(sub->subArc, 0x2A), 0, 5, 1, 0);
        sub->m_Work0 = 0xE;
        sub->r_no_2++;
    case 3:
        if (sub->m_Work0 != 0) {
            sub->m_Work0--;
            if (sub->m_Work0 == 0) {
                door->setOpen(&sub->pos, 1, 0, 0);
                emDoorBellSet(&sub->pos);
            }
        }
        if (MotionMove(sub, 0)) {
            EndSubDamage();
        }
        break;
    }
}

// Drops every hanging weapon object (id 0x42) within 5000 units of the door (opening a door with
// weapons hooked on it).
void emDoorDropWeapon(cEmDoor* pEm)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEmWep* e = (cEmWep*) EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x42) {
            continue;
        }
        {
            f32 dx = e->pos.x - pEm->pos.x;
            f32 dy = e->pos.y - pEm->pos.y;
            f32 dz = e->pos.z - pEm->pos.z;

            if (dx * dx + dy * dy + dz * dz > 25000000.0f) {
                continue;
            }
        }
        e->setWaitDrop();
    }
}

// Pairs two halves of a double door so kicks / opens are mirrored.
void cEmDoor::setDoor(cEmDoor* other)
{
    EmDoorWork* w = EMDOOR_WK(this);

    if (other) {
        w->pDoor = other;
        EMDOOR_WK(other)->pDoor = this;
    }
}

// The next open SE is skipped (the paired door plays it).
void cEmDoor::setSeCancel()
{
    EMDOOR_WK(this)->Se_cancel = 1;
}
