#ifndef PL14_H
#define PL14_H

// pl14 module (D:/Bio4/Prog/pl14.cpp): Luis (`cSubLuis`, the pSUB of the cabin fight). The
// character keeps a routine machine (cRoutine, motion state per routine number), an action machine
// (cAction, what he wants to do) and an analysis (cAnalysis, targets / player state) as members,
// plus his own hit boxes and the thrown item object (cObjLuisItem, ObjMgr id 0x1E).

#include "types.h"
#include "model.h"
#include "em.h"
#include "player.h"
#include "obj.h"

// Room_flg bits of room 1-1C (the besieged cabin), from the PS2 symbols; RmfFlagChk(pG, n).
enum R11C_FLAG {
    RMF_DEBUG_00 = 0,
    RMF_BESIEGEDING = 1,
    RMF_LUIS_ANGRY = 2,
    RMF_ROUTE_EVT_CANCEL = 3,
    RMF_GATE_OPEN = 64,
    RMF_EMSET_2F = 65,
};

class cSubLuis;

// Spoken line: the sound and the subtitle it shows.
class cVoice {
public:
    u8 on;                // 0x00  a line is playing
    int time;            // 0x04  frames left
    u32 seId;            // 0x08  SndCall id, 0xF0F0F0F0 = none

    cVoice();
    void set(int mesNo, u16 seNo, int time);
    void move();
};

// Routine machine: owner->xFC is the routine number, xFD its step. Higher priority routines
// (damage, die) interrupt lower ones.
class cRoutine {
public:
    cSubLuis* owner;      // 0x000
    cMot3 mot3;           // 0x004 .. 0x0EC  three-way blend (aim up / level / down)
    f32 rate;             // 0x0EC  mot3 rate (aim elevation)
    int type;              // 0x0F0  (PS2 cRoutine::type, unused on GC)
    int intLevel;             // 0x0F4  priority of the running routine
    u8 intStack[3];          // 0x0F8  routine interrupted per priority (0xFF = none)
    u8 padFB;
    u32 flag;            // 0x0FC  bit0: the routine ended (eor)
    cVoice voice;         // 0x100 .. 0x10C
    u8 shotCnt;           // 0x10C  shots of the current burst
    u8 pad10D[3];
    int work[4];          // 0x110  [0] damage: motion variant / turn: direction, [1] damage: voice type / turn: frames (PS2 work[4])
    f32 dist;             // 0x120  walk / run: arrival distance
    u8 pad124[0xC];
    Vec pos;           // 0x130  walk / run target
    cEm* pTarget;         // 0x13C  enemy aimed at
    int m_ShootDown;              // 0x140  frames the dead target was kept

    void init(cSubLuis* o);
    int move();
    void moveFootwork();
    void moveDamage();
    void moveDie();
    void moveEvent();
    void moveWalk();
    void moveRun();
    void moveWepReady();
    void moveWepSet();
    void moveWepFire();
    void moveWepDown();
    void moveThrowItem();
    void setItem();
    void moveDown();
    void moveUp();
    void moveBlast();
    void moveAvoid();
    void moveTurn();
    void moveTurn180();
    int set(int no);
    void end();
    int eor();
    void shot();
};

// Target analysis.
class cAnalysis {
public:
    cSubLuis* owner;      // 0x00
    int iem;              // 0x04  EmMgr index the round-robin isTarget scan is at
    f32 plDist;           // 0x08  route distance to the player
    int time;              // 0x0C  frames
    s8 grenadeTimer;            // 0x10  frames the player has aimed a grenade at him (bit7 = handled)
    cEm* pEmNear;         // 0x14  nearest target
    f32 pEmNearDist;       // 0x18  its squared distance
    s8 flags;             // 0x1C  bit1 aimed at by the player, bit2 down, bit3 periodic, bit4 grenade, bit5 spoke, bit6 damaged, bit7 rack
    u8 pad1D[3];

    void init(cSubLuis* o);
    void move();
    int aimCheck();
};

// Action machine: mode 0 wait, 1 attack, 2 chase the player, 3 go upstairs, 4 attack the player,
// 5 damage, 6 die, 7 give the item, 8 down, 9 up, 0xA avoid, 0xB room 11C opening, 0xC escape the rack.
class cAction {
public:
    cSubLuis* owner;      // 0x00
    int type;             // 0x04
    int rno0;              // 0x08  mode move() dispatches on
    u8 rno1;              // 0x0C
    u8 rno2;               // 0x0D
    u8 rno3;                // 0x0E
    u8 padF;
    int timer;            // 0x10
    u8 pad14[0xC];
    u8 flags;             // 0x20  bit0 / bit1: 11C opening started / done

    void init(cSubLuis* o);
    void move(cAnalysis* an, cRoutine* rt);
    void moveAttack(cAnalysis* an, cRoutine* rt);
    void moveGo2F(cAnalysis* an, cRoutine* rt);
    void moveAttackPl(cAnalysis* an, cRoutine* rt);
    void moveGiveItem(cAnalysis* an, cRoutine* rt);
    void moveDown(cAnalysis* an, cRoutine* rt);
    void moveUp(cAnalysis* an, cRoutine* rt);
    void moveAvoid(cAnalysis* an, cRoutine* rt);
    void move11cBegin(cAnalysis* an, cRoutine* rt);
    void moveEscRack(cAnalysis* an, cRoutine* rt);
    void moveChasePl(cAnalysis* an, cRoutine* rt);
    void set(int mode);
    int chasePlAreaCheck();
};

class cObjLuisItem;

class cSubLuis : public cEm {
public:
    cSubLuis* pEm;    // 0x3E0  the model the routines animate (itself)
    float dist;           // 0x3E4
    Vec distPos;          // 0x3E8
    float distMargin;     // 0x3F4
    int m_Work0;          // 0x3F8
    cRoutine routine;     // 0x3FC .. 0x540
    cAction action;       // 0x540 .. 0x564
    cAnalysis analysis;   // 0x564 .. 0x584
    void (*m_pFunc)();     // 0x584  routine 4 (event): the scenario's function
    cObj* pWep;           // 0x588  his gun (ObjMgr id 0xB, equipWeapon)
    s8 flags;             // 0x58C  bit0 damaged, bit1 dead, bit2 upstairs, bit3 neck set this frame, bit6 damage from an enemy
    u8 pad58D[3];
    int thankCtr;              // 0x590  frames of the damage reaction voice

private:
    YARARE_INFO m_Yarare[10];    // 0x594 .. 0x79C
    u8 pad79C[4];
    cModelInfo* pFace;    // 0x7A0
    u16 m_LeonHp;          // 0x7A4  player life the last worry line was spoken at
    u8 m_PlAtack;            // 0x7A6  hits left before he goes down
    u8 m_okTime;         // 0x7A7
    cEm* pRackWk[3];         // 0x7A8  the room's racks (getRoomEtcRack)

public:
    f32 neckY;          // 0x7B4

    cSubLuis();
    virtual ~cSubLuis();
    virtual void move();
    virtual void endDamage();
    void init();
    void modelSet();
    void think();
    int rackCheck();
    void seqSeCtrl();
    int damageCheck();
    void equipWeapon();
    void moveEye();
    void neckSet(f32 ang, f32 limit);
    void neckMove();
};

struct LuisItemWork {
    int timer;            // 0x00 (0x328)
    Vec spd;              // 0x04 (0x32C)
    Vec acc;              // 0x10 (0x338)
};

class cObjLuisItem : public cObjUnion {
public:
    virtual void move();
    void init(Vec* pos, f32 rotY);
};

int doorHitCheck(Vec* a, Vec* b);
int stairCheck(cModel* m);
int sameFloorCheck(cModel* a, cModel* b);
int greThrowCheck();
int isTarget(cSubLuis* luis, cEm* em);
void luisItemInit(cObj* obj);

#endif
