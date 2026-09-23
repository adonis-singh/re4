#ifndef EM_H
#define EM_H

#include "types.h"
#include "cManager.h"
#include "model.h"
#include "atariInfo.h"
#include "main_mem.h"

// The player classes derive from cEm, so the player-only fields the pl_* units touch live in
// cEm too (they all sit below 0xDE0).
// YARARE_INFO::flags bits (PS2 YARARE_ATARI_FLAG): the Yarare* `flag` argument.
enum YARARE_ATARI_FLAG {
    YAT_FLAG_ON = 1,
    YAT_FLAG_X_AXIS = 2,
    YAT_FLAG_Z_AXIS = 4,
    YAT_FLAG_CUBE = 8,
    YAT_FLAG_HANDGUN_MUSHI = 16,
    YAT_FLAG_THROUGH = 32,
    YAT_FLAG_NO_MARK = 64,
    YAT_FLAG_NO_SCR_BOMB_CK = 128,
    YAT_FLAG_DMPOS = 16384
};

// Hit box ("yarare") / damage part info (cEm+0x33C for the player; GetWepTargetList returns
// pointers to these per target), 0x34 bytes; extra boxes are chained through `next` (at_mod.cpp
// YarareAdd / YarareAddCube).
struct YARARE_INFO {
    Vec offset;              // 0x00  box centre offset from the model / parts (yarareInit0 x, y, z)
    Vec cross;              // 0x0C  hit position in the parts (obj1b: the spear sticks here)
    f32 radius;            // 0x18  box half width (PS2 radius)
    f32 height;           // 0x1C
    f32 extent;            // 0x20  cube depth (YarareInitCube / YarareAddCube set it with flags bit3)
    u16 flag;            // 0x24  YARARE_ATARI_FLAG bits: CUBE, THROUGH (the hit sets cDmgInfo bit5 too, pl_wep PlWepHitCheck2)
    s16 parts_no;          // 0x26  parts the effect is placed at (0 = the model itself), 1-based
    f32 len;              // 0x28  squared distance hit point -> line start (em_sub emLineAtCk / emBoxAtCk)
    f32 c_dis;             // 0x2C  squared distance of the hit from the aim line (em_sub GetWepTargetList sorts on it)
    YARARE_INFO* pList;      // 0x30  next hit box of the model (YarareAdd)
};

// Damage info at cEm+0x324 (game/em.cpp), 0x18 bytes. set(0, 10, kind, pos, rad, part) registers a hit.
class cDmgInfo {
public:
    u8 m_Flag;
    u8 m_Timer;
    u8 m_Wep;      // 0x02  set() kind
    u8 m_Padding03;
    Vec m_PosFrom;              // 0x04  hit position
    f32 m_Dist;              // 0x10
    YARARE_INFO* m_pDamageYarare;      // 0x14  hit part

    cDmgInfo();
    void set(int flag, int timer, u8 wep, Vec* pos, f32 rad, YARARE_INFO* part);
    void set(int flag, int timer);   // stores the two bytes at 0/1 (pl_sub: set(0, 10), set(0, 0x80))
    void clear();
    void move();              // counts x1 down; clears stat when it reaches 0
};

// Room water effect table registered at cEm::pRoomEff (pl_sub PlRegistRoomEff): 3 entries of
// {u32 id; u8 pad[3]; u8 type;} used as EstSet(..., id, type, ...) for the ripple / splash effects.
struct PlRoomEff {
    u32 id;
    u8 pad_4[3];
    u8 type;
};

// Blend motion work (0xD0 bytes): a MotionWork (model.h) without the trailing blend/flip/blendTbl
// pointers. cEm::m_SubMot (0x42C) and cMot3::work are one; MotionWork::blend points at it.
struct PlArc;      // global.h
struct EmiEntry;   // embarrel.h
class cSubChar;    // pl_npc.h
class cPlayer;     // player.h
class cLight;      // light.h

enum EM_STATUS {
    EM_STATUS_ATTACKING = 0,
    EM_STATUS_LOCKOFF = 1,
    EM_STATUS_MIST_ON = 2,
    EM_STATUS_IK_OFF = 3,
    EM_STATUS_ALERT = 4,
    EM_STATUS_ACTIVE = 5,
    EM_STATUS_DOGCK = 6,
    EM_STATUS_DOGATK = 7,
    EM_STATUS_ITEMSET = 8,
    EM_STATUS_LOOK_ME = 9,
    EM_STATUS_DONT_FIRE = 10,
    EM_STATUS_ASHLEY_NO_HELP = 11
};

// Character work (game/em.cpp), sizeof 0x3E0: the cModel (0x320, which carries the motion work,
// the cAtariInfo, pFsdTbl and the light area) plus the fields below. The derived classes own
// the bytes from 0x3E0 up: cPlayer and cSubChar name theirs, the enemies overlay a work struct on
// the free area cEmFree declares.
class cEm : public cModel {
public:
    s16 hp;               // 0x320
    s16 hp_max;            // 0x322
    cDmgInfo dmg;     // 0x324  (obj08: dmg.set on a hit target)
    YARARE_INFO hitInfo;    // 0x33C .. 0x370  (obj08: the player's hit part for the damage effect)
    f32 l_pl;             // 0x370  squared distance to the player (db_work prints its sqrt as "L PL") (PS2 l_pl)
    f32 l_sub;             // 0x374  (em_set: 1e16 at creation)  squared distance to the partner (em30/em34/em38: closer than l_pl -> target it) (PS2 l_sub)
    PlArc* subArc;          // 0x378  cSubChar: motion archive the routines index (pl_npc.cpp)
    PlArc* subArc2;         // 0x37C  cSubChar: the archive restored after a damage routine
    Vec lockOfs;          // 0x380  lock-on point offset in the lockParts' matrix (pl_wep)
    u8 lockParts;         // 0x38C  parts the lock-on point follows (pl_wep; AutoTrack uses the low 3 bits)
    u8 set;              // 0x38D  (db_cam "set=")
    u8 pad_38E[2];
    void (*pScenario)(cEm*);  // 0x390  em_sub EmScenario: called with the enemy when set
    u8 pad_394[4];
    u8 emset_no;           // 0x398
    u8 pad_399[3];
    union {
        struct {
            u8 x39C;
            u8 x39D;      // 0x39D  (obj16: the type 1 head is drawn at half scale while set)
            u8 pad_39E[0x3A8 - 0x39E];
        };
        Vec Catch_pos_adj;  // 0x39C  em_sub EmCatchPLSet: offset the caught model keeps to the catcher (PS2 Catch_pos_adj)
    };
    Vec Catch_at_adj;     // 0x3A8  pos at the end of the catcher's frame; EmCatchMotionMove moves the catcher by pos - Catch_at_adj (objTrolleySetAdjust adds the car movement to it) (PS2 Catch_at_adj)
    f32 Catch_dir;        // 0x3B4  em_sub EmCatchPLSet: rot.y left to turn (EmCatchMotionMove eats it) (PS2 Catch_dir)
    cEm* pEmCatch;         // 0x3B8  what damaged this one (pl_sub SetPlDamage/SetSubDamage
                          //         first argument); object enemies store a cObj of their own
    u8 RckStat;           // 0x3BC  route_ck: bit0 = RckNear valid this frame (RouteCk clears it)
    s8 RckMy;             // 0x3BD  route_ck: way point the enemy heads to (-1 = none)
    s8 RckTo;             // 0x3BE  route_ck: way point nearest to the target
    s8 RckNear;           // 0x3BF  route_ck: way point nearest to the enemy
    u8 pad_3C0[4];
    u32 status;           // 0x3C4  setStatus / clearStatus / checkStatus bits (bit0 = in battle, bit1, bit11)
    u32 flag;        // 0x3C8  (db_cam "Flag=")  (PS2 cEm::flag; EM_LIST.flag)
    f32 Guard_r;             // 0x3CC  (em_set: list entry s16 x1A * 1000)  guard radius (em10: L_guard vs Guard_r) (PS2 Guard_r)
    u8 Character;              // 0x3D0  (em_set: list entry byte 0xB)  (PS2 Character)
    u8 itemFlag;          // 0x3D1  setItem 5th argument (setNoItem: 0)
    u8 pad_3D2[4];
    u16 Item_id;           // 0x3D6  setItem a (setNoItem: 0xFFFF)
    u16 Item_num;          // 0x3D8  setItem b
    u16 Item_flg;          // 0x3DA  setItem c
    u16 Auto_item_flg;          // 0x3DC  setItem d
    u8 pad_3DE[2];

    cEm();
    virtual ~cEm() {}
    virtual void move();
    virtual void setItem(u16 item_id, u16 num, u16 item_flg, u16 auto_item_flg, u8 item_eff);  // 0x3D6.. item drop (0x3D1 flag)
    virtual void setNoItem();
    virtual int checkThrow();
    void setStatus(int id);     // status |= 1 << bit
    void clearStatus(int id);
    int checkStatus(int stat);
    int initWork();              // be_flag = 0x21, x12E = 0 (the constructor)
};

// One work per enemy: the manager's stride is the largest enemy class, cEm plus its free area.
#define EM_WORK_SIZE 0xDE0

// The routine bytes written all at once, in the single stw the original emits at some sites (four
// separate byte stores do not reproduce it). A macro, not an inline: an inline defined here would
// create entities in every unit that includes em.h, which renumbers their static locals.
#define EmRoutineSetW(em, r0, r1, r2, r3) \
    (*(u32*) &(em)->r_no_0 = ((u32) (r0) << 24) | ((u32) (r1) << 16) | ((u32) (r2) << 8) | (u32) (r3))

// The four routine numbers of an enemy written through a helper: the stores stay in this order and the
// arguments keep the registers of the call, which is not the case when they are assigned inline.
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3) { em->r_no_0 = r0; em->r_no_1 = r1; em->r_no_2 = r2; em->r_no_3 = r3; }

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int EmDeadCk(cEm* em) { return em->dmg.m_Flag || em->dmg.m_Timer; }

// Enemy manager (game/em.cpp). The construct id selects the class: 0 player, 1..0xE / others a
// read-table enemy (EmInitFunc), 0x40.. the object enemies (cEmObj, cEmDoor, ...), 0xFF a plain cEm.
class cEmMgr : public cManager<cEm> {
public:
    u32 Guid;              // 0x34  next cModel::serial (construct)

    static const char* idName[96];   // debug names per construct id

    cEmMgr();
    // no user destructor: the synthesized one (and cManager<cEm>'s) land after the other inlines
    virtual void* memAlloc(u32 size) { return MemAlloc(size, 1); }
    virtual void memFree(void* p) { MemFree(p); }
    virtual void memClear(cEm* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual void destroy(cEm* pEm);   // em.cpp overrides the cManager one (pl_sub SubCharCtrl / PlDataRelease)
    virtual int construct(cEm* pSat, u32 room_no);

    int arrayAlloc(u32 workNum);        // cManager<cEm>::arrayAlloc + pPL = pSUB = 0; returns 1
    void move();                  // dieCheck, RouteCk, emMove for every alive work (or only pSUB when stopped)
    // first alive enemy with model id `id`, searching from `start->next` (or the list head)
    cEm* getEmPtr(int id, cEm* pEm);
    int isBattle();               // 1 when any alive enemy has status bit0
    void destroyAll();            // killEm on every alive work (id != 0)
};

extern cEmMgr EmMgr;
extern cPlayer* pPL;
extern cEm* pSUB;  // partner character (a cSubChar)

// Pushable rack/crate enemy (game/emrack.cpp); only what pl_push calls.
class cEmRack : public cEm {
    // PS2 keeps the size of the free area in this static; the GC build allocates no storage for it
    // (a definition here would add a 4-byte common symbol the original DOL does not have).
    static u32 classFreeSize;
public:
    u8 free[0xD60 - 0x3E0];   // 0x3E0  the rack's own work (emrack.h FREE_EMRACK)
    Mtx baseMat;          // 0xD60  push range matrix (setRange: rot * trans of the rack)
    Mtx baseInvMat;       // 0xD90  its inverse (adjustRange transforms the position into range space)
    f32 rackRange[4];     // 0xDC0  push limits (adjustRange dir 0: [1], 1: -[2], 2: [0], 3: -[3])
    u8 rackFlags;         // 0xDD0  bit4 (0x10) range set; SetRack initialises it to 0xF

    virtual void move();   // key function: keeps the vtable in emrack.o (cEmMgr::construct stores it)

    void setBreak(Vec* pPos);
    void setDown(Vec* pos);
    void setShock();
    void setEff(u8 eff_id);
    void setRange(f32 x_low, f32 x_up, f32 y_low, f32 y_up);
    int adjustRange(u8 dir);
};

// Motion / model data `no` of the archive of work `w` (cEm::subArc: an enemy's own archive, a
// partner's, or the one a player damage routine animates from).
#define EM_ARC(w, no) PL_ARC_PTR((w)->subArc, no)
// The same for the enemy module's own archive; the enemy is the local `em`.
#define ARC(no) PL_ARC_PTR(em->subArc, no)

extern "C" {
void emMove(cEm* pEm);        // per-frame update of one alive work: distance to the player, damage info, move()
void battleCheck(cEm* pEm);
void killEm(cEm* pEm);
}

#endif
