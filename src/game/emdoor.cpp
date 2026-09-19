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
#include "cmath.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);
void EtcSetAddAmb(cModel* m, int kind);   // EtcModel.cpp
void Em_R0_Scenario(cEm* em);          // em_sub.cpp
}
void MotionSetCore(cModel* m, void* w, void* data, int seq, int hokan, int flags, int frame);   // motion.cpp (C++ linkage)

// Hanging object (game/obj12.cpp): the locks and the chain hang on the door as cObj12 models.
class cObj12 : public cObj {
public:
    void setFall(Vec* spd, u8 type);
    void setFallSe(u8 blk, u8 no, u8 id);
};
cObj* SetObj12(void* bin, void* tpl, Vec* pos, Vec* rot);

typedef void (*EmDoorFunc)(cEmDoor*);

// Store through a scalar reference: the following pPL read is not shared with the one before it.
static inline void U32Set(u32& d, u32 v) { d = v; }

// Struct-member view of pPL: the load stays below a preceding store through the player pointer.
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

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
    em->pXFlip = emDoor_xflip_tbl;
    EtcSetAddAmb(em, 2);
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
void emDoorDmCkWood(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    YARARE_INFO* part;
    u8 wep;
    Vec v;
    Vec out;
    int kind;

    if (em->hp > 0) {
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
            em->setBreak(&out);
            return;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
    part = em->dmg.m_pDamageYarare;
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
    if (w->Be_flg & 1) {
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        return;
    }
    if (em->dmg.m_Wep == 0x10) {
        if (part != &w->hit[12] && part != &w->hit[11] && part != &w->hit[13] && part != &w->hit[14] && part != &w->hit[15]) {
            em->dmg.m_Timer = 0;
            return;
        }
    }
    switch (em->dmg.m_Wep) {
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
            emDoorSetDmgLock_L(em, 0);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorSetDmgLock_R(em, 0);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (part->partsNo != 0) {
            if (em->plDist2 < 9000000.0f && Rnd() % 5 == 0) {
                w->Door_hp = 0.0f;
            } else {
                w->Door_hp -= 1.0f;
            }
        }
        emDoorSetDmgDoor(em);
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
                emDoorSetDmgLock_L(em, 0);
            } else {
                emDoorSetDmgLock_L(em, 1);
            }
            return;
        }
        if (part == &w->hit[11]) {
            if (w->Be_flg & 2) {
                w->Lock_L_hp -= 4;
                emDoorSetDmgLock_R(em, 0);
            } else {
                emDoorSetDmgLock_R(em, 1);
            }
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (part->partsNo != 0) {
            w->Door_hp = 0.0f;
        }
        emDoorSetDmgDoor(em);
        break;
    case 7:
    case 8:
    case 0x21:
        if (part == &w->hit[12]) {
            if (em->plDist2 < 25000000.0f) {
                if (w->Be_flg & 2) {
                    w->Lock_L_hp -= 4;
                } else {
                    w->Lock_L_hp = 0;
                }
            }
            emDoorSetDmgLock_L(em, 0);
            return;
        }
        if (part == &w->hit[11]) {
            if (em->plDist2 < 25000000.0f) {
                if (w->Be_flg & 2) {
                    w->Lock_R_hp -= 4;
                } else {
                    w->Lock_R_hp = 0;
                }
            }
            emDoorSetDmgLock_R(em, 0);
            return;
        }
        if (part->partsNo != 0 && part->rad < 64000000.0f) {
            w->Door_hp = 0.0f;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        emDoorSetDmgDoor(em);
        break;
    case 0xE:
        break;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        emDoorSetDmgLock_L(em, 1);
        emDoorSetDmgLock_R(em, 1);
        emDoorSetDmgChain(em, 0);
        emDoorSetDmgChain(em, 1);
        emDoorSetDmgChain(em, 2);
        emDoorSetBrkDoor(em, &em->dmg.m_PosFrom);
        break;
    }
    if (emDoorBrkCk(em)) {
        emDoorSetDmgLock_L(em, 1);
        emDoorSetDmgLock_R(em, 1);
        emDoorSetDmgChain(em, 0);
        emDoorSetDmgChain(em, 1);
        emDoorSetDmgChain(em, 2);
        EstSet((int) em, -1, 0, 0, w->Eff_id, 6, 0, 0, (u32) em, 0);
        SndCall(6, 0x37, &em->pos, 0, 0, em);
        em->r_no_0 = 1;
        em->r_no_1 = 4;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
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
    if (em->plDist2 < 25000000.0f) {
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
    if (em->plDist2 < 25000000.0f) {
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
void emDoorDmCkIron(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    YARARE_INFO* part;
    u8 wep;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
    part = em->dmg.m_pDamageYarare;
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
        if (part != &EMDOOR_WK(em)->hit[12] && part != &EMDOOR_WK(em)->hit[11] && part != &EMDOOR_WK(em)->hit[13] &&
            part != &EMDOOR_WK(em)->hit[14] && part != &EMDOOR_WK(em)->hit[15]) {
            em->dmg.m_Timer = 0;
            return;
        }
    }
    switch (em->dmg.m_Wep) {
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
            emDoorSetDmgLock_L(em, 0);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorSetDmgLock_R(em, 0);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        return;
    case 9:
    case 0xA:
    case 0x28:
        if (part == &w->hit[12]) {
            emDoorLockHitL(em, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitR(em, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        return;
    case 5:
    case 6:
    case 7:
    case 8:
    case 0xF:
    case 0x21:
    case 0x2C:
        if (part == &w->hit[12]) {
            emDoorLockHitNearL(em, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitNearR(em, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 3, 0, 0, 0);
        return;
    case 0xE:
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        emDoorBreakLocks(em);
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        }
        break;
    }
}

// Damage check of the type 4 iron door: like the iron door but its panes (hit boxes 3..10) break
// on any bullet hit.
void emDoorDmCkIron2(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    YARARE_INFO* part;
    u8 wep;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
    part = em->dmg.m_pDamageYarare;
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
        if (part != &EMDOOR_WK(em)->hit[12] && part != &EMDOOR_WK(em)->hit[11] && part != &EMDOOR_WK(em)->hit[13] &&
            part != &EMDOOR_WK(em)->hit[14] && part != &EMDOOR_WK(em)->hit[15]) {
            em->dmg.m_Timer = 0;
            return;
        }
    }
    switch (em->dmg.m_Wep) {
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
            emDoorSetDmgLock_L(em, 0);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorSetDmgLock_R(em, 0);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (part->partsNo != 0) {
            w->Door_hp = 0.0f;
            emDoorSetDmgDoor(em);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        return;
    case 9:
    case 0xA:
    case 0x28:
        if (part == &w->hit[12]) {
            emDoorLockHitL(em, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitR(em, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (part->partsNo != 0) {
            w->Door_hp = 0.0f;
            emDoorSetDmgDoor(em);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        return;
    case 5:
    case 6:
    case 7:
    case 8:
    case 0xF:
    case 0x21:
    case 0x2C:
        if (part == &w->hit[12]) {
            emDoorLockHitNearL(em, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitNearR(em, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (part->partsNo != 0) {
            w->Door_hp = 0.0f;
            emDoorSetDmgDoor(em);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 3, 0, 0, 0);
        return;
    case 0xE:
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        emDoorBreakLocks(em);
        if (part->partsNo != 0) {
            w->Door_hp = 0.0f;
            emDoorSetDmgDoor(em);
            return;
        }
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        }
        break;
    }
}

// Damage check of the type 2 falling door: locks / chains as the iron door; the door itself
// only sparks (it is opened by kicking, not shooting).
void emDoorDmCkIronDown(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    YARARE_INFO* part;
    u8 wep;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
    part = em->dmg.m_pDamageYarare;
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
        if (part != &EMDOOR_WK(em)->hit[12] && part != &EMDOOR_WK(em)->hit[11] && part != &EMDOOR_WK(em)->hit[13] &&
            part != &EMDOOR_WK(em)->hit[14] && part != &EMDOOR_WK(em)->hit[15]) {
            em->dmg.m_Timer = 0;
            return;
        }
    }
    switch (em->dmg.m_Wep) {
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
            emDoorSetDmgLock_L(em, 0);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorSetDmgLock_R(em, 0);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        return;
    case 9:
    case 0xA:
    case 0x28:
        if (part == &w->hit[12]) {
            emDoorLockHitL(em, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitR(em, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        return;
    case 5:
    case 6:
    case 7:
    case 8:
    case 0xF:
    case 0x21:
    case 0x2C:
        if (part == &w->hit[12]) {
            emDoorLockHitNearL(em, w);
            return;
        }
        if (part == &w->hit[11]) {
            emDoorLockHitNearR(em, w);
            return;
        }
        if (part == &w->hit[13]) {
            emDoorChainHit(em, 0);
            return;
        }
        if (part == &w->hit[14]) {
            emDoorChainHit(em, 1);
            return;
        }
        if (part == &w->hit[15]) {
            emDoorChainHit(em, 2);
            return;
        }
        if (w->Eff_id == 0xFF) {
            return;
        }
        EmDmBloodSet2(em, w->Eff_id, 3, 0, 0, 0);
        return;
    case 0xE:
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        emDoorBreakLocks(em);
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        }
        break;
    }
}

// A hit box no longer takes hits.
static inline void emDoorHitOff(YARARE_INFO* hit)
{
    hit->flags &= ~1;
}

// Damages the left lock (mode 0: one hit, 4 for strong locks; mode 1: destroy): rattle est / SE,
// and when its hp is gone drops the lock object (cObj12 setFall) with the break SE and sets etc
// flag bit2.
void emDoorSetDmgLock_L(cEmDoor* em, int mode)
{
    EmDoorWork* w = EMDOOR_WK(em);
    EmListData* d = EM_LIST(em->emset_no);
    YARARE_INFO* hit;
    u16* flg;
    Vec v;

    if (w->pLockL == 0) {
        return;
    }
    hit = &w->hit[12];
    switch (mode) {
    case 0:
    default:
        if (w->Be_flg & 2) {
            w->Lock_L_hp -= 1;
        } else {
            w->Lock_L_hp = 0;
        }
        SndCall(6, 0x52, &em->pos, 0, 0, em);
        EmDmBloodSet2(em, 0xC9, 0, 0, 0, 0);
        EstSet((int) w->pLockL, -1, 0, 0, 0xC9, 1, 0, 0, (u32) w->pLockL, 0);
        if (w->Lock_L_hp <= 0) {
            SndCall(6, 0x53, &em->pos, 0, 0, em);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 20.0f;
            PSMTXMultVecSR(w->pLockL->mat, &v, &v);
            w->pLockL->setFall(&v, 4);
            w->pLockL->setFallSe(6, 0x3C, 0);
            d->flag &= ~0x80000000;
            em->flag &= ~0x80000000;
            em->setStatus(EM_STATUS_LOCKOFF);
            w->pLockL = 0;
            emDoorHitOff(hit);
        }
        w->Lock_L_bend = PI / 4;
        break;
    case 1:
        w->Lock_L_hp = 0;
        SndCall(6, 0x53, &em->pos, 0, 0, em);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 20.0f;
        PSMTXMultVecSR(w->pLockL->mat, &v, &v);
        w->pLockL->setFall(&v, 4);
        w->pLockL->setFallSe(6, 0x3C, 0);
        SndCall(6, 0x52, &em->pos, 0, 0, em);
        EmDmBloodSet2(em, 0xC9, 0, 0, 0, 0);
        EstSet((int) w->pLockL, -1, 0, 0, 0xC9, 1, 0, 0, (u32) w->pLockL, 0);
        d->flag &= ~0x80000000;
        em->flag &= ~0x80000000;
        em->setStatus(EM_STATUS_LOCKOFF);
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
void emDoorSetDmgLock_R(cEmDoor* em, int mode)
{
    EmDoorWork* w = EMDOOR_WK(em);
    EmListData* d = EM_LIST(em->emset_no);
    YARARE_INFO* hit;
    u16* flg;
    Vec v;

    if (w->pLockR == 0) {
        return;
    }
    hit = &w->hit[11];
    switch (mode) {
    case 0:
    default:
        if (w->Be_flg & 2) {
            w->Lock_R_hp -= 1;
        } else {
            w->Lock_R_hp = 0;
        }
        SndCall(6, 0x52, &em->pos, 0, 0, em);
        EmDmBloodSet2(em, 0xC9, 0, 0, 0, 0);
        EstSet((int) w->pLockR, -1, 0, 0, 0xC9, 1, 0, 0, (u32) w->pLockR, 0);
        if (w->Lock_R_hp <= 0) {
            SndCall(6, 0x53, &em->pos, 0, 0, em);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 20.0f;
            PSMTXMultVecSR(w->pLockR->mat, &v, &v);
            w->pLockR->setFall(&v, 4);
            w->pLockR->setFallSe(6, 0x3C, 0);
            d->flag &= ~0x40000000;
            em->flag &= ~0x40000000;
            em->setStatus(EM_STATUS_LOCKOFF);
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
        SndCall(6, 0x53, &em->pos, 0, 0, em);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 20.0f;
        PSMTXMultVecSR(w->pLockR->mat, &v, &v);
        w->pLockR->setFall(&v, 4);
        w->pLockR->setFallSe(6, 0x3C, 0);
        SndCall(6, 0x52, &em->pos, 0, 0, em);
        EmDmBloodSet2(em, 0xC9, 0, 0, 0, 0);
        EstSet((int) w->pLockR, -1, 0, 0, 0xC9, 1, 0, 0, (u32) w->pLockR, 0);
        d->flag &= ~0x40000000;
        em->flag &= ~0x40000000;
        em->setStatus(EM_STATUS_LOCKOFF);
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
void emDoorSetDmgChain(cEmDoor* em, u32 no)
{
    EmDoorWork* w = EMDOOR_WK(em);
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
        EstSet(0, -1, &parts->world, &w->pChain->ang, 0xCB, 1, 0, 0, 0, 0);
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        SndCall(6, 0x16, &em->pos, 0, 0, em);
        if (flg) {
            *flg |= 8;
        }
        break;
    case 1:
        emDoorHitOff(&w->hit[14]);
        w->Chain_hp[no] = 0;
        parts = w->pChain->getPartsPtr(2);
        EstSet(0, -1, &parts->world, &w->pChain->ang, 0xCB, 2, 0, 0, 0, 0);
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        SndCall(6, 0x16, &em->pos, 0, 0, em);
        if (flg) {
            *flg |= 0x10;
        }
        break;
    case 2:
        emDoorHitOff(&w->hit[15]);
        w->Chain_hp[no] = 0;
        parts = w->pChain->getPartsPtr(3);
        EstSet(0, -1, &parts->world, &w->pChain->ang, 0xCB, 3, 0, 0, 0, 0);
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        SndCall(6, 0x16, &em->pos, 0, 0, em);
        if (flg) {
            *flg |= 0x20;
        }
        break;
    }
}

// A pane (parts of the hit box) was shot: when Door_hp is used up hides the pane, spawns the
// splinter est facing the shooter, plays the SE and sets the pane's flag bit (0x8000 >> pane);
// otherwise just the hit SE. Four broken panes break the whole door.
void emDoorSetDmgDoor(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    YARARE_INFO* part = em->dmg.m_pDamageYarare;
    EmListData* d = EM_LIST(em->emset_no);
    cModel* parts;
    u16* flg;
    Vec v;
    Vec rot;
    Vec* wpos;
    f32 ang;
    f32 dif;
    u32 bit;
    u32 i;

    if (part->partsNo != 0) {
        parts = em->getPartsPtr(part->partsNo - 1);
        if (w->Door_hp <= 0.0f) {
            w->Door_hp = (Rnd() & 1) + 1;
            parts->scale.x = 0.0f;
            parts->scale.y = 0.0f;
            parts->scale.z = 0.0f;
            part->flags &= ~1;
            if (em->type == 4) {
                if (w->Eff_id != 0xFF) {
                    wpos = &em->getPartsPtr(part->partsNo - 1)->world;
                    v.x = 0.0f;
                    v.y = 0.0f;
                    v.z = 1.0f;
                    PSMTXMultVecSR(em->mat, &v, &v);
                    rot.x = 0.0f;
                    rot.y = atan2f(v.x, v.z);
                    rot.z = 0.0f;
                    ang = GetXZAngle(wpos, &em->dmg.m_PosFrom);
                    dif = Muku2(rot.y, ang, PI);
                    if (fabsf(dif) > PI / 2) {
                        rot.y += PI;
                        rot.y = LIMIT_ANGLE(rot.y);
                    }
                    EstSet(0, -1, wpos, &rot, w->Eff_id, 0, 0, 0, 0, 0);
                    SndCall(6, 0x3B, &em->pos, 0, 0, em);
                    flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
                    if (flg) {
                        *flg |= 8;
                    }
                }
            } else {
                if (w->Eff_id != 0xFF) {
                    EmDmBloodSet2(em, w->Eff_id, 0, 0, 0, 0);
                }
                SndCall(6, 0x36, &em->pos, 0, 0, em);
            }
            bit = 0x8000;
            for (i = 3; i <= 10; i++) {
                if (part == &w->hit[i]) {
                    em->flag |= bit;
                    d->flag |= bit;
                    break;
                }
                bit >>= 1;
            }
            return;
        }
    }
    if (w->Eff_id != 0xFF) {
        switch (em->dmg.m_Wep) {
        default:
            EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            EmDmBloodSet2(em, w->Eff_id, 3, 0, 0, 0);
            break;
        }
    }
}

// Breaks the door from a shot at `pos` (explosive / heavy weapon): types 2 / 3 just open toward
// it, others spawn the break est (5 from the front, 4 from behind) with the SE and go to Break.
void emDoorSetBrkDoor(cEmDoor* em, Vec* pos)
{
    EmDoorWork* w = EMDOOR_WK(em);
    f32 ang;

    if (em->hp <= 0) {
        return;
    }
    switch (em->type) {
    case 2:
    case 3:
        em->setOpen(pos, 0, 0, 0);
        return;
    }
    if (w->Eff_id != 0xFF) {
        ang = Muku(&em->pos, pos, em->ang.y, PI);
        if (fabsf(ang) < PI / 2) {
            EstSet((int) em, -1, 0, 0, w->Eff_id, 5, 0, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, w->Eff_id, 4, 0, 0, (u32) em, 0);
        }
    }
    SndCall(6, 0x37, &em->pos, 0, 0, em);
    em->r_no_0 = 1;
    em->r_no_1 = 4;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
}

// 1 when more than 3 panes are broken (the door should break).
int emDoorBrkCk(cEmDoor* em)
{
    u32 bit;
    int cnt;
    int i;

    if (em->hp <= 0) {
        return 0;
    }
    bit = 0x8000;
    cnt = 0;
    for (i = 0; i < 8; i++) {
        if (em->flag & bit) {
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
void emDoor_R0_Init(cEmDoor* em)
{
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 Open, 2 Open2, 3 Close, 4 Break, 5 Shock, 6 OpenLock,
// 7 CloseLock, 8 Down, 9 Downed).
void emDoor_R0_Move(cEmDoor* em)
{
    EmDoor_R1_move_tbl[em->r_no_1](em);
}

// Rno1 == 0: resting (closed or swung open) door; rebuilds the matrix once (or every frame while
// panes are broken), and an open door swings shut again (Close) when nobody stands in it.
void emDoor_R1_Set(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);

    switch (em->r_no_2) {
    case 0:
        emDoorMatUpdate(em);
        em->r_no_2++;
        SndStop(w->Seid_open, 0);
        break;
    case 1:
        if (em->flag & 0xFFC0) {
            emDoorMatUpdate(em);
        }
        break;
    }
    if (emDoorDoorAutoCloseCk(em) == 0) {
        if (!(em->flag & 0xFFC0)) {
            em->be_flag |= 0x4000;
        }
    }
}

// Rno1 == 1: kicked open: flags the door open, swings ang.y toward +-90 degrees from base_dir
// (direction from Open_pos), hits whoever stands behind it once (Rno3: PlWepHitCheck3 type
// 0x18, 400 radius), bounces back a little and settles (flag 0x10000000 = fully open).
void emDoor_R1_Open(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    f32 ang;
    f32 d;
    Vec v;
    s16 hp;

    switch (em->r_no_2) {
    case 0:
        em->flag |= 0x20000000;
        w->kickCnt = 1;
        w->Timer = 0x14;
        w->Open_timer = 0x96;
        emDoorSetDmgLock_L(em, 1);
        emDoorSetDmgLock_R(em, 1);
        emDoorSetDmgChain(em, 0);
        emDoorSetDmgChain(em, 1);
        emDoorSetDmgChain(em, 2);
        em->hp = 1000;
        ang = fabsf(Muku(&em->pos, &w->Open_pos, w->base_dir, PI));
        if (ang > PI / 2) {
            w->Open_flag = 0;
        } else {
            w->Open_flag = 1;
        }
        emDoorDropWeapon(em);
        em->r_no_2++;
    case 1:
        if (w->Open_flag) {
            ang = LIMIT_ANGLE(w->base_dir - PI / 2);
            d = Muku2(em->ang.y, ang, PI / 6);
        } else {
            ang = LIMIT_ANGLE(w->base_dir + PI / 2);
            d = Muku2(em->ang.y, ang, PI / 7);
        }
        em->ang.y += d;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        d = fabsf(d);
        if (d < PI / 1024) {
            em->ang.y = ang;
            em->r_no_2++;
        }
        if (w->kickCnt != 0) {
            w->kickCnt--;
            em->dmg.m_Timer = 2;
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
            if (em->r_no_3 != 0) {
                hp = 0;
                if (pSUB) {
                    hp = pSUB->hp;
                    pSUB->hp = 0;
                }
                if (PlWepHitCheck3(&v, 0x18, 10, 400.0f)) {
                    SndCall(1, 0xF, &v, 0, 0, em);
                }
                if (pSUB) {
                    pSUB->hp = hp;
                }
            }
        }
        break;
    case 2:
        w->Timer = 2;
        em->r_no_2++;
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
            em->ang.y = ang + d;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        } else {
            em->ang.y = ang;
            em->r_no_0 = 1;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            em->flag |= 0x10000000;
        }
        break;
    }
    emDoorMatUpdate(em);
}

// Rno1 == 2: opened by hand: breaks any lock / chain, plays the door's open motion (player
// archive motion 0x1F, mirrored per Rno3 side) with the creak SE at frame 15 (unless the paired
// door plays it), drops hanging weapons nearby, then rests fully open.
static void emDoor_R1_Open2(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->flag |= 0x20000000;
        emDoorSetDmgLock_L(em, 1);
        emDoorSetDmgLock_R(em, 1);
        emDoorSetDmgChain(em, 0);
        emDoorSetDmgChain(em, 1);
        emDoorSetDmgChain(em, 2);
        em->hp = 1000;
        switch (em->r_no_3) {
        case 1:
            MotionSetCore(em, &em->pMotion, PL_ARC_PTR(pG->pPlayer, 0x1F), 0, 0, 0x41, 0);
            break;
        case 0:
        case 2:
        default:
            MotionSetCore(em, &em->pMotion, PL_ARC_PTR(pG->pPlayer, 0x1F), 0, 0, 1, 0);
            break;
        case 3:
            MotionSetCore(em, &em->pMotion, PL_ARC_PTR(pG->pPlayer, 0x1F), 0, 0, 0x41, 0);
            break;
        }
        emDoorDropWeapon(em);
        em->r_no_2++;
    case 1:
        if (em->frame > 14.7f && em->frame < 15.3f) {
            if (w->Se_cancel == 0) {
                switch (em->type) {
                case 0:
                case 3:
                default:
                    w->Seid_open = SndCall(1, 0x42, &em->pos, 0, 0, em);
                    break;
                case 1:
                case 2:
                case 4:
                case 5:
                case 7:
                    w->Seid_open = SndCall(1, 0x41, &em->pos, 0, 0, em);
                    break;
                }
            }
            w->Se_cancel = 0;
        }
        if (MotionMove(em, 0)) {
            em->flag |= 0x30000000;
            em->r_no_0 = 1;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
        }
        break;
    }
    em->partsWorldCalc();
}

// Rno1 == 8: the door is kicked off its hinges (types 2 / 3, or down_ck): breaks locks / chains,
// records the fall direction in the etc flag (bit6 / bit7), spawns the fall est (0xA / 7), then
// rotates about x with growing speed to +-90 degrees, hitting whoever stands behind (type 0x14),
// lands with the crash est (0xC / 0xB) and rests as Downed.
void emDoor_R1_Down(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    u16* flg;
    f32 ang;
    f32 h;
    f32 r;
    int water;
    Vec v;
    s16 hp;

    switch (em->r_no_2) {
    case 0:
        em->flag |= 0x20000000;
        w->kickCnt = 1;
        w->Timer = 0x14;
        w->Open_timer = 0x96;
        emDoorSetDmgLock_L(em, 1);
        emDoorSetDmgLock_R(em, 1);
        emDoorSetDmgChain(em, 0);
        emDoorSetDmgChain(em, 1);
        emDoorSetDmgChain(em, 2);
        ang = Muku(&em->pos, &w->Open_pos, w->base_dir, PI);
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
        if (GetWaterHeight(&em->pos, &h) && em->pos.y < h) {
            water = 1;
        }
        switch (em->type) {
        case 2:
        case 3:
            if (water) {
                EstSet((int) em, -1, 0, 0, w->Eff_id, 0xA, 0, 0, (u32) em, 0);
                SndCall(6, 3, &em->pos, 0, 0, em);
            } else {
                EstSet((int) em, -1, 0, 0, w->Eff_id, 7, 0, 0, (u32) em, 0);
            }
            break;
        }
        w->Spd = 0.0f;
        emDoorDropWeapon(em);
        em->hp = 0;
        if (w->Open_flag) {
            w->Spd = -(7.0f * (PI / 180.0f));
        } else {
            w->Spd = 7.0f * (PI / 180.0f);
        }
        em->pos.y += 40.0f;
        em->r_no_2++;
    case 1:
        if (w->Open_flag) {
            w->Spd -= PI / 180.0f;
            em->ang.x += w->Spd;
            if (em->ang.x < -PI / 2) {
                em->ang.x = -PI / 2;
                em->r_no_2++;
            }
        } else {
            w->Spd += PI / 180.0f;
            em->ang.x += w->Spd;
            if (em->ang.x > PI / 2) {
                em->ang.x = PI / 2;
                em->r_no_2++;
            }
        }
        if (w->kickCnt != 0) {
            w->kickCnt--;
            em->dmg.m_Timer = 2;
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
            if (em->r_no_3 != 0) {
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
        if (GetWaterHeight(&em->pos, &h) && em->pos.y < h) {
            water = 1;
        }
        switch (em->type) {
        case 2:
        case 3:
            if (water) {
                if (w->Open_flag) {
                    EstSet((int) em, -1, 0, 0, w->Eff_id, 0xC, 0, 0, (u32) em, 0);
                    SndCall(6, 3, &em->pos, 0, 0, em);
                } else {
                    EstSet((int) em, -1, 0, 0, w->Eff_id, 0xB, 0, 0, (u32) em, 0);
                    SndCall(6, 3, &em->pos, 0, 0, em);
                }
            } else {
                if (w->Open_flag) {
                    EstSet((int) em, -1, 0, 0, w->Eff_id, 9, 0, 0, (u32) em, 0);
                } else {
                    EstSet((int) em, -1, 0, 0, w->Eff_id, 8, 0, 0, (u32) em, 0);
                }
            }
            break;
        }
        em->r_no_2++;
    case 3:
        if (w->Open_flag) {
            em->ang.x = -PI / 2;
        } else {
            em->ang.x = PI / 2;
        }
        if (w->Timer != 0) {
            w->Timer--;
            r = fRand0_1() * (PI / 128) + PI / 128;
            if (pG->Frame_cnt & 1) {
                r = -r;
            }
            em->ang.x += r;
        } else {
            em->flag |= 0x10000000;
            em->r_no_2++;
        }
        break;
    case 4:
        break;
    }
    emDoorMatUpdate(em);
}

// Rno1 == 9: the fallen door lying flat (ang.x +-90 degrees by Open_flag), hp 0.
void emDoor_R1_Downed(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);

    if (em->r_no_2 == 0) {
        em->hp = 0;
        if (w->Open_flag) {
            em->ang.x = -PI / 2;
        } else {
            em->ang.x = PI / 2;
        }
        em->pos.y += 40.0f;
        em->flag |= 0x30000000;
        em->r_no_2++;
    }
    emDoorMatUpdate(em);
}

// Rno1 == 3: swings back to base_dir over 20 frames, then Set.
void emDoor_R1_Close(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->flag &= ~0x30000000;
        w->Timer = 0x14;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->base_dir, PI) * 0.3f;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (w->Timer != 0) {
            w->Timer--;
        } else {
            em->ang.y = w->base_dir;
            em->r_no_0 = 1;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
        }
        break;
    }
    emDoorMatUpdate(em);
}

// Rno1 == 4: destroyed: hides the door, hp 0, etc flag bit0; then stays as a hit-box-only work.
void emDoor_R1_Break(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    u16* flg;

    if (em->r_no_2 == 0) {
        em->hp = 0;
        em->be_flag &= ~2;
        em->clearStatus(EM_STATUS_ACTIVE);
        emDoorSatClear(em);
        em->flag |= 0x30000000;
        flg = GetEtcFlgPtr(w->Etc_no, pGS->room_id);
        if (flg) {
            *flg |= 1;
        }
        if (w->Key_flag != 0x36) {
            u32* tbl = pG->Key_flg;

            tbl[w->Key_flag >> 5] |= 0x80000000 >> (w->Key_flag & 0x1F);
        }
        em->r_no_2++;
    }
}

// Rno1 == 5: kicked but not opened (locked / blocked): takes 25 / 50 hp (never below 1) and
// rattles about base_dir for 7 frames.
void emDoor_R1_Shock(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    f32 r;

    switch (em->r_no_2) {
    case 0:
        w->Timer = 7;
        if (em->ckObj() == 0) {
            em->hp -= 25;
        } else {
            em->hp -= 50;
        }
        if (em->hp <= 0) {
            em->hp = 1;
        }
        em->r_no_2++;
    case 1:
        if (w->Timer != 0) {
            w->Timer--;
            r = fRand0_1() * (PI / 128) + PI / 128;
            if (pG->Frame_cnt & 1) {
                r = -r;
            }
            em->ang.y = w->base_dir + r;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        } else {
            em->ang.y = w->base_dir;
            em->r_no_0 = 1;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
        }
        break;
    }
    emDoorMatUpdate(em);
}

// Rno1 == 6: script-opened door: snaps to +-90 degrees (Rno3 side), then waits until setNormal
// clears the lock bit before returning to Set.
void emDoor_R1_OpenLock(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    f32 ang;
    int unlock;

    switch (em->r_no_2) {
    case 0:
        em->flag |= 0x30000000;
        w->Timer = 0x14;
        if (em->r_no_3) {
            w->Open_flag = 1;
            ang = w->base_dir;
            ang -= PI / 2;
        } else {
            w->Open_flag = 0;
            ang = w->base_dir;
            ang += PI / 2;
        }
        em->ang.y = ang;
        em->r_no_2++;
    case 1:
        unlock = !(w->Be_flg & 1);
        if (unlock) {
            em->r_no_0 = 1;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
        }
        break;
    }
    emDoorMatUpdate(em);
}

// Rno1 == 7: script-closed door: swings back to base_dir over 20 frames, then waits for setNormal.
void emDoor_R1_CloseLock(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    int unlock;

    switch (em->r_no_2) {
    case 0:
        em->flag &= ~0x30000000;
        w->Timer = 0x14;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->base_dir, PI) * 0.3f;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (w->Timer != 0) {
            w->Timer--;
        } else {
            em->ang.y = w->base_dir;
            unlock = !(w->Be_flg & 1);
            if (unlock) {
                em->r_no_0 = 1;
                em->r_no_1 = 0;
                em->r_no_2 = 0;
                em->r_no_3 = 0;
            }
        }
        break;
    }
    emDoorMatUpdate(em);
}

// Applies the lock / chain bend angles (set by hits and kicks) to the hanging objects' parts and
// decays them by 0.7 per frame.
void emDoorLockBendMove(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
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
void emDoorSatSet(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    Vec poly[4];
    f32 h;
    int attr;

    emDoorSatClear(em);
    if (em->hp <= 0) {
        return;
    }
    em->atari.setFlag200();
    switch (em->type) {
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
        switch (em->type) {
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
        w->pSat[1] = EatMgr.create(&em->pos, &em->ang, poly, attr, 0, h);
    } else {
        w->pSat[1]->m_Flag |= 4;
        w->pSat[1]->setCoord(&em->pos, &em->ang);
    }
    if (!(em->flag & 0x7500)) {
        if (w->pSat[2] == 0) {
            switch (em->type) {
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
            w->pSat[2] = EatMgr.create(&em->pos, &em->ang, poly, attr, 0, h);
        } else {
            w->pSat[2]->m_Flag |= 4;
            w->pSat[2]->setCoord(&em->pos, &em->ang);
        }
    }
    if (!(em->flag & 0x8AC0)) {
        if (w->pSat[3] == 0) {
            switch (em->type) {
            case 4:
                SAT_POLY(-w->Width * 2.0f + 320.0f, -320.0f, 1300.0f);
                h = 580.0f;
                w->pSat[3] = EatMgr.create(&em->pos, &em->ang, poly, attr, 0, h);
                break;
            case 5:
                SAT_POLY(-w->Width * 2.0f + 150.0f, -150.0f, 1350.0f);
                h = 500.0f;
                w->pSat[3] = EatMgr.create(&em->pos, &em->ang, poly, 0x404000, 0, h);
                break;
            default:
                SAT_POLY(-w->Width * 2.0f, 0.0f, 1400.0f);
                h = w->Height - 1400.0f;
                w->pSat[3] = EatMgr.create(&em->pos, &em->ang, poly, attr, 0, h);
                break;
            }
        } else {
            w->pSat[3]->m_Flag |= 4;
            w->pSat[3]->setCoord(&em->pos, &em->ang);
        }
    }
    if (w->pSat[4] == 0) {
        switch (em->type) {
        case 4:
            SAT_POLY(-w->Width * 2.0f, -w->Width * 2.0f + 320.0f, 1300.0f);
            h = 580.0f;
            w->pSat[4] = EatMgr.create(&em->pos, &em->ang, poly, attr, 0, h);
            break;
        case 5:
            SAT_POLY(-w->Width * 2.0f, -w->Width * 2.0f + 150.0f, 1300.0f);
            h = 580.0f;
            w->pSat[4] = EatMgr.create(&em->pos, &em->ang, poly, attr, 0, h);
            break;
        }
    } else {
        w->pSat[4]->m_Flag |= 4;
        w->pSat[4]->setCoord(&em->pos, &em->ang);
    }
    if (w->pSat[5] == 0) {
        switch (em->type) {
        case 4:
            SAT_POLY(-320.0f, 0.0f, 1300.0f);
            h = 580.0f;
            w->pSat[5] = EatMgr.create(&em->pos, &em->ang, poly, attr, 0, h);
            break;
        case 5:
            SAT_POLY(-150.0f, 0.0f, 1300.0f);
            h = 580.0f;
            w->pSat[5] = EatMgr.create(&em->pos, &em->ang, poly, attr, 0, h);
            break;
        }
    } else {
        w->pSat[5]->m_Flag |= 4;
        w->pSat[5]->setCoord(&em->pos, &em->ang);
    }
}

// Deactivates every effect collision panel of the door.
void emDoorSatClear(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);

    em->atari.clrFlag200();
    if (w->pSat[1]) {
        w->pSat[1]->m_Flag &= ~4;
    }
    if (w->pSat[2]) {
        w->pSat[2]->m_Flag &= ~4;
    }
    if (w->pSat[3]) {
        w->pSat[3]->m_Flag &= ~4;
    }
    if (w->pSat[4]) {
        w->pSat[4]->m_Flag &= ~4;
    }
    if (w->pSat[5]) {
        w->pSat[5]->m_Flag &= ~4;
    }
}

// Positions the left / right lock and the chain objects on the door (fixed door-space offsets).
void emDoorLockMove(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    Vec v;
    Vec rot;

    if (w->pLockL) {
        v.x = -1160.0f;
        v.y = 1050.0f;
        v.z = 80.0f;
        PSMTXMultVec(em->mat, &v, &v);
        w->pLockL->pos = v;
        w->pLockL->ang = em->ang;
    }
    if (w->pLockR) {
        rot = em->ang;
        rot.y += PI;
        rot.y = LIMIT_ANGLE(rot.y);
        v.x = -1160.0f;
        v.y = 1050.0f;
        v.z = -80.0f;
        PSMTXMultVec(em->mat, &v, &v);
        w->pLockR->pos = v;
        w->pLockR->ang = rot;
    }
    if (w->pChain) {
        rot = em->ang;
        v.x = -650.0f;
        v.y = 1550.0f;
        v.z = 30.0f;
        PSMTXMultVec(em->mat, &v, &v);
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
int emDoorDoorAutoCloseCk(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    Vec v;
    f32 x0;
    f32 lim0;
    f32 x1;
    f32 lim1;
    f32 x2;
    f32 lim2;
    int ok;
    u32 i;

    if (!(em->flag & 0x20000000)) {
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
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

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
        if (e == em) {
            continue;
        }
        PSMTXMultVec(w->base_im, &e->pos, &v);
        NEAR_CK(x2, lim2);
        if (ok) {
            return 0;
        }
    }
    if (em->ckObj() == 0) {
        return 0;
    }
    em->r_no_0 = 1;
    em->r_no_1 = 3;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    em->flag &= ~0x20000000;
    return 1;
}

// Hit boxes by type: the whole leaf, the top board and both side posts (sizes per type); the
// falling door (2) and type 7 have none.
void emDoorYarareInit(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    u16 flags;

    if (em->type == 2) {
        return;
    }
    if (em->type == 7) {
        return;
    }
    if (em->type == 0) {
        flags = 0x21;
    } else {
        flags = 0x41;
    }
    switch (em->type) {
    default:
        YarareInitCube(em, -w->Width, 0.0f, 0.0f, w->Width, w->Height, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[0], -w->Width, w->Height - 300.0f, 0.0f, w->Width, 300.0f, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[1], -100.0f, 0.0f, 0.0f, 100.0f, w->Height, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[2], -(w->Width * 2.0f - 100.0f), 0.0f, 0.0f, 100.0f, w->Height, 55.0f, 0, flags);
        break;
    case 4:
        YarareInitCube(em, -w->Width, 0.0f, 0.0f, w->Width, w->Height - 1000.0f, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[0], -w->Width, w->Height - 370.0f, 0.0f, w->Width, 370.0f, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[1], -170.0f, 0.0f, 0.0f, 170.0f, w->Height, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[2], -(w->Width * 2.0f - 170.0f), 0.0f, 0.0f, 170.0f, w->Height, 55.0f, 0, flags);
        break;
    case 5:
        YarareInitCube(em, -w->Width, 0.0f, 0.0f, w->Width, 1350.0f, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[0], -w->Width, w->Height - 600.0f, 0.0f, w->Width, 600.0f, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[1], -75.0f, 0.0f, 0.0f, 75.0f, w->Height, 55.0f, 0, flags);
        YarareAddCube(em, &w->hit[2], -(w->Width * 2.0f - 75.0f), 0.0f, 0.0f, 75.0f, w->Height, 55.0f, 0, flags);
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
        w->pLockL = (cObj12*) SetObj12(bin, tpl, &v, &ang);
        if (w->pLockL) {
            w->pLockL->LightInfo.EnableMask = 0x10;
            w->pLockL->setNoSuspend(1);
        }
        YarareAddCube(this, &w->hit[12], -1150.0f, 800.0f, 60.0f, 150.0f, 350.0f, 100.0f, 0, 1);
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
        w->pLockR = (cObj12*) SetObj12(bin, tpl, &v, &r);
        if (w->pLockR) {
            w->pLockR->LightInfo.EnableMask = 4;
            w->pLockR->setNoSuspend(1);
        }
        YarareAddCube(this, &w->hit[11], -1150.0f, 800.0f, -60.0f, 150.0f, 350.0f, 100.0f, 0, 1);
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
    w->pChain = (cObj12*) SetObj12(bin, tpl, &v, &ang);
    if (w->pChain == 0) {
        return;
    }
    w->pChain->LightInfo.EnableMask = 0x10;
    w->pChain->setNoSuspend(1);
    if (flg) {
        if (!(*flg & 8)) {
            YarareAddCube(this, &w->hit[13], -650.0f, 1350.0f, 50.0f, 650.0f, 350.0f, 100.0f, 0, 1);
            w->Chain_hp[0] = 2;
        } else {
            cModel* parts = w->pChain->getPartsPtr(1);

            parts->scale.x = 0.0f;
            parts->scale.y = 0.0f;
            parts->scale.z = 0.0f;
        }
        if (!(*flg & 0x10)) {
            YarareAddCube(this, &w->hit[14], -650.0f, 1000.0f, 50.0f, 650.0f, 350.0f, 100.0f, 0, 1);
            w->Chain_hp[1] = 2;
        } else {
            cModel* parts = w->pChain->getPartsPtr(2);

            parts->scale.x = 0.0f;
            parts->scale.y = 0.0f;
            parts->scale.z = 0.0f;
        }
        if (!(*flg & 0x20)) {
            YarareAddCube(this, &w->hit[15], -650.0f, 700.0f, 50.0f, 650.0f, 300.0f, 100.0f, 0, 1);
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
void cEmDoor::setEff(u8 eff)
{
    EMDOOR_WK(this)->Eff_id = eff;
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
            w->hit[3].flags &= ~1;
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
int cEmDoor::ckKick(Vec* pos)
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
void cEmDoor::setOpen(Vec* pos, int mode, int se_off, int down_ck)
{
    EmDoorWork* w = EMDOOR_WK(this);

    if (hp <= 0) {
        return;
    }
    w->Open_pos = *pos;
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
    if (pSUB && pSUB->plDist2 < 25000000.0f) {
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
void cEmDoor::setShock(int mode, Vec* pos, int se_off)
{
    EmDoorWork* w = EMDOOR_WK(this);
    f32 ang;
    u32 i;
    u32 no;

    if (hp <= 0) {
        return;
    }
    ang = fabsf(Muku(&this->pos, pos, w->base_dir, PI));
    if (ang < PI / 2) {
        if (w->pLockL) {
            EstSet((int) w->pLockL, -1, 0, 0, 0xC9, 1, 0, 0, (u32) w->pLockL, 0);
            w->Lock_L_bend = -PI / 2;
        }
    } else {
        if (w->pLockR) {
            EstSet((int) w->pLockR, -1, 0, 0, 0xC9, 1, 0, 0, (u32) w->pLockR, 0);
            w->Lock_R_bend = -PI / 2;
        }
    }
    if (w->pChain) {
        for (i = 0; i <= 2; i++) {
            if (w->Chain_hp[i] > 0) {
                switch (i) {
                case 0:
                    EstSet((int) w->pChain, -1, 0, 0, 0xCB, 4, 0, 0, (u32) w->pChain, 0);
                    break;
                case 1:
                    EstSet((int) w->pChain, -1, 0, 0, 0xCB, 5, 0, 0, (u32) w->pChain, 0);
                    break;
                case 2:
                    EstSet((int) w->pChain, -1, 0, 0, 0xCB, 6, 0, 0, (u32) w->pChain, 0);
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
            EstSet((int) w->pLockL, -1, 0, 0, 0xC9, 1, 0, 0, (u32) w->pLockL, 0);
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
            EstSet((int) w->pLockR, -1, 0, 0, 0xC9, 1, 0, 0, (u32) w->pLockR, 0);
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
void cEmDoor::setBreak(Vec* pos)
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
        setOpen(pos, 0, 0, 0);
        return;
    }
    zero = 0;
    EstSet((int) this, -1, 0, 0, w->Eff_id, 6, 0, 0, (u32) this, (void*) zero);
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
void emDoorActEvtCk(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    f32 ang;
    f32 lim;
    Vec v;

    if (em->flag & 0x20000000) {
        return;
    }
    if (em->hp <= 0) {
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
        ActBtn.set(0x10, 5, (int) emDoorAction2, (int) em, 0, 1, 0, 0);
    } else {
        ActBtn.set(0x10, 5, (int) emDoorAction, (int) em, 0, 1, 0, 0);
    }
}

// Action button callback: a locked / blocked / falling-type door is kicked (plemDoorKick), an
// openable one opened by hand (plemDoorOpen).
void emDoorAction(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    int kick;

    kick = 0;
    if (em->ckOpen()) {
        kick = 1;
    }
    if (w->pDoor && w->pDoor->ckOpen() > 1) {
        kick = 1;
    }
    switch (em->type) {
    case 2:
        kick = 1;
        break;
    case 3:
        kick = 1;
        break;
    }
    if (kick) {
        SetPlDamage(em, plemDoorKick);
    } else {
        SetPlDamage(em, plemDoorOpen);
    }
}

// Same for the partner half of a double door (flags Status_flg[1] 0x20000000, a noise).
void emDoorAction2(cEmDoor* em)
{
    EmDoorWork* w = EMDOOR_WK(em);
    int kick;

    kick = 0;
    if (em->ckOpen()) {
        kick = 1;
    }
    if (w->pDoor && w->pDoor->ckOpen() > 1) {
        kick = 1;
    }
    switch (em->type) {
    case 2:
        kick = 1;
        break;
    case 3:
        kick = 1;
        break;
    }
    if (kick) {
        SetPlDamage(em, plemDoorKick);
        pPL->r_no_3 = 1;
    } else {
        SetPlDamage(em, plemDoorOpen);
        pPL->r_no_3 = 1;
    }
}

// The bell position marks where the door was kicked / opened (pG->bell_pos).
static inline void emDoorBellSet(Vec* pos)
{
    StaFlagOn(pG, STA_SE_BURST);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), pos, sizeof(Vec));
    pG->bell_stat = 0;
}

// Player damage routine of the kick: a kickable door plays the kick-open motion (0x1C) and
// setOpen at frame 14, otherwise the blocked-kick motion (0x1D) with setShock at frames 14 / 17;
// the paired door follows. Ends the damage state when the motion finishes.
void plemDoorKick(cPlayer* pl)
{
    cEmDoor* door = (cEmDoor*) pl->pEmCatch;
    cEmDoor* door2 = (cEmDoor*) pPL->pEmCatch;
    EmDoorWork* w = EMDOOR_WK(door2);
    int frame;
    Vec v;

    pl->subArc = door2->subArc;
    if (pl->r_no_2 == 0 || pl->r_no_2 == 4) {
        if (door->ckKick(&pPLS->pos) && (w->pDoor == 0 || w->pDoor->ckKick(&pPL->pos))) {
            if (pl->r_no_2 == 0) {
                pl->r_no_2 = 2;
            }
            if (pl->r_no_2 == 4) {
                pl->r_no_2 = 6;
            }
        }
    }
    frame = 0;
    if (pl->r_no_2 == 4) {
        pl->r_no_2 = 0;
        frame = 6;
    }
    if (pl->r_no_2 == 6) {
        pl->r_no_2 = 2;
        frame = 6;
    }
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlayer, 0x1D), 0, 5, 1, frame);
        pl->r_no_2++;
    case 1:
        if (pl->frame > 13.7f && pl->frame < 14.3f) {
            door->setShock(1, &pl->pos, 0);
            if (pl->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setShock(1, &pl->pos, 1);
            }
            emDoorBellSet(&pl->pos);
        }
        if (pl->frame > 16.7f && pl->frame < 17.3f) {
            door->setShock(2, &pl->pos, 0);
            if (pl->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setShock(2, &pl->pos, 1);
            }
        }
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlayer, 0x1C), 0, 5, 1, frame);
        pl->r_no_2++;
    case 3:
        if (pl->frame > 13.7f && pl->frame < 14.3f) {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -500.0f;
            PSMTXMultVec(pl->mat, &v, &v);
            door->setOpen(&v, 1, 0, 0);
            if (pl->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setOpen(&v, 1, 0, 0);
                w->pDoor->setSeCancel();
            }
            emDoorBellSet(&pl->pos);
        }
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Player damage routine of opening by hand: motion 0x1E with setOpen2 (side by which half the
// player stands on; the partner door mirrors it), then walks the player through and ends the
// damage state.
void plemDoorOpen(cPlayer* pl)
{
    cEmDoor* door = (cEmDoor*) pl->pEmCatch;
    cEmDoor* door2 = (cEmDoor*) pPL->pEmCatch;
    EmDoorWork* w = EMDOOR_WK(door2);
    f32 d;
    u8 flag;
    Vec v;

    pl->subArc = door2->subArc;
    switch (pl->r_no_2) {
    case 0:
        d = Muku2(pl->ang.y, door->ang.y, PI);
        if (fabsf(d) > PI / 2) {
            v.x = -638.54f;
            v.y = 0.0f;
            v.z = 440.4f;
            PSMTXMultVec(pl->pEmCatch->mat, &v, &v);
            PSVECSubtract(&v, &pPL->pos, &pl->m_VecWork0);
            pl->m_VecWork0.y = 0.0f;
            pl->m_Fwork0 = pl->pEmCatch->ang.y + PI;
            FSet(pl->m_Fwork0, LIMIT_ANGLE(pl->m_Fwork0));
            MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlayer, 0x1E), 0, 5, 1, 0);
            door->setOpen2(0);
            if (pl->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setOpen2(1);
                w->pDoor->setSeCancel();
            }
        } else {
            v.x = -638.54f;
            v.y = 0.0f;
            v.z = -440.4f;
            PSMTXMultVec(((cEmDoor*) pl->pEmCatch)->mat, &v, &v);
            PSVECSubtract(&v, &pPL->pos, &pl->m_VecWork0);
            pl->m_VecWork0.y = 0.0f;
            FSet(pl->m_Fwork0, ((cEmDoor*) pl->pEmCatch)->ang.y);
            MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlayer, 0x1E), 0, 5, 1, 0);
            door->setOpen2(1);
            if (pl->r_no_3 && w->pDoor && !(w->pDoor->flag & 0x10000000)) {
                w->pDoor->setOpen2(0);
                w->pDoor->setSeCancel();
            }
        }
        pl->dmg.set(0, 0x26);
        pl->atari.clrFlag200();
        pl->m_Work0 = 0x2D;
        pl->r_no_2++;
    case 1:
        pl->ang.y += Muku2(pl->ang.y, pl->m_Fwork0, PI / 16);
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        if (pl->r_no_3 == 0) {
            PSVECScale(&pl->m_VecWork0, &v, 0.1f);
            PSVECAdd(&pl->pos, &v, &pl->pos);
            PSVECSubtract(&pl->m_VecWork0, &v, &pl->m_VecWork0);
        }
        if (MotionMove(pl, 0)) {
            pl->atari.setFlag200();
            EndPlDamage();
            pl->dmg.set(0, 10);
        } else if (pl->m_Work0 != 0) {
            pl->m_Work0--;
            if (Key.trg & 0x400) {
                flag = pl->r_no_3;
                SetPlDamage(pl->pEmCatch, plemDoorKick);
                pl->r_no_3 = flag;
                pl->r_no_2 = 4;
                door->r_no_0 = 1;
                door->r_no_1 = 0;
                door->r_no_2 = 0;
                door->r_no_3 = 0;
                if (pl->r_no_3 && w->pDoor) {
                    w->pDoor->r_no_0 = 1;
                    w->pDoor->r_no_1 = 0;
                    w->pDoor->r_no_2 = 0;
                    w->pDoor->r_no_3 = 0;
                }
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
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

    for (i = 0; i < EmMgr.nArray; i++) {
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
void cEmDoor::setCloseLock(int a)
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
void cEmDoor::setDowned(int dir)
{
    EmDoorWork* w = EMDOOR_WK(this);
    u16* flg;

    w->Open_flag = dir;
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

    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* em = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
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
void SubOpenDoorSet(cEmDoor* door)
{
    SetSubDamage(door, (void*) subDoorKick);
}

// Partner damage routine: kick motion (0x2B) with setShock, or the kick-open motion (0x2A) with
// setOpen when the door can be kicked open; ends when the motion finishes.
void subDoorKick()
{
    cSubChar* sub = pSUB;
    cEmDoor* door = (cEmDoor*) sub->pEmCatch;

    if (sub->r_no_2 == 0) {
        if (door->ckKick(&sub->pos)) {
            sub->r_no_2 = 2;
        }
    }
    switch (sub->r_no_2) {
    case 0:
        MotionSetCore(sub, &sub->pMotion, PL_ARC_PTR(sub->subArc, 0x2B), 0, 5, 1, 0);
        sub->subHideMode = 0xE;
        sub->r_no_2++;
    case 1:
        if (sub->subHideMode != 0) {
            sub->subHideMode--;
            if (sub->subHideMode == 0) {
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
        MotionSetCore(sub, &sub->pMotion, PL_ARC_PTR(sub->subArc, 0x2A), 0, 5, 1, 0);
        sub->subHideMode = 0xE;
        sub->r_no_2++;
    case 3:
        if (sub->subHideMode != 0) {
            sub->subHideMode--;
            if (sub->subHideMode == 0) {
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
void emDoorDropWeapon(cEmDoor* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEmWep* e = (cEmWep*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x42) {
            continue;
        }
        {
            f32 dx = e->pos.x - em->pos.x;
            f32 dy = e->pos.y - em->pos.y;
            f32 dz = e->pos.z - em->pos.z;

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
