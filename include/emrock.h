#ifndef EMROCK_H
#define EMROCK_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "embarrel.h"

class cPlayer;
struct EmAtkInfo;

// Work of the rolling rock enemy (game/emrock.cpp), overlaid on cEm from 0x3E0.
struct EmRockWork {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: setParent flag (no matrix normalisation), bit1: transparent mode
    int Timer;            // 0x004 (0x3E4)
    int Timer2;           // 0x008 (0x3E8)  Fall / Throw: frames before the rock stops
    int rnd;              // 0x00C (0x3EC)  Drop2: Rnd() & 1 (action button variant)
    f32 Radius;           // 0x010 (0x3F0)  collision radius (scale.x * 265 or 450)
    f32 Gravity;             // 0x014 (0x3F4)  gravity per frame
    u8 pad_18[0xC];
    u32 x24;              // 0x024 (0x404)
    int rollWait;         // 0x028 (0x408)  Roll: frames before the floor check starts
    cEm* pEm_oya;         // 0x02C (0x40C)  model the rock hangs on (setParent)
    u32 pEm_old;              // 0x030 (0x410)  pParent at the time of setFall / setThrow
    int oya_parts;          // 0x034 (0x414)  parts of pParent
    u32 seid_throw;            // 0x038 (0x418)  SndCall handle of the always sound
    int Roll_wait;           // 0x03C (0x41C)  Roll: start delay
    void* mot0;           // 0x040 (0x420)  Drop motions (setDropMot)
    void* mot1;           // 0x044 (0x424)
    void* mot2;           // 0x048 (0x428)  player death motion (plemDropDie)
    void* mot3;           // 0x04C (0x42C)  sub character death motion (subemDropDie)
    void* mot4;           // 0x050 (0x430)  setDropMot2: player escape motion (plemDropEscape)
    void* mot5;           // 0x054 (0x434)  player find motion (plemDropFind)
    u8 pad_58[0x7C - 0x58];
    Vec spd;              // 0x07C (0x45C)
    u8 seFall[4];         // 0x088 (0x468)  SndCall blk / no / vol of the landing (setSeFall), 0xFF = none
    u8 se8C;              // 0x08C (0x46C)
    u8 se8D[3];           // 0x08D (0x46D)  SndCall of the player hit (emRockAtkCk), 0xFF = none
    u8 se90[3];           // 0x090 (0x470)
    u8 seAlways[3];       // 0x093 (0x473)  SndCall of the flying sound, 0xFF = none
    u8 alwaysWait;        // 0x096 (0x476)  frames between the always sound calls (4)
    u8 se97[3];           // 0x097 (0x477)
    u8 effFall[2];        // 0x09A (0x47A)  EstSet id / type when the rock lands (setEffFall), 0xFF = none
    u8 eff9C[2];          // 0x09C (0x47C)  EmPlBloodSet2 arguments when the player is hit, 0xFF = none
    u8 eff9E[2];          // 0x09E (0x47E)
    u8 espKind;           // 0x0A0 (0x480)  EspPullCoreKind at creation
    u8 xA1;               // 0x0A1 (0x481)
    u8 pad_A2[2];
    int Rock_route;         // 0x0A4 (0x484)  current EMI route point (type 6) of the rolling rock
    EmiEntry* pRoute;     // 0x0A8 (0x488)
    u8 Roll_flag;           // 0x0AC (0x48C)  Set: the roll started
    u8 First_bound;               // 0x0AD (0x48D)  Roll: room 104 flag
    u8 Act_ck;               // 0x0AE (0x48E)  Drop2 / escape: the player escaped / died
    u8 pad_AF;
    u32 sndId2;           // 0x0B0 (0x490)  Roll: rolling sound handle
    void* plMot[16];      // 0x0B4 (0x494)  player motions of the roll escape (setPlMotion)
    struct EmAtkInfo* pAtk;  // 0x0F4 (0x4D4)  attack parameters of the flying rock (emRockAtkCk)
    u8 pad_F8[0x1FC - 0xF8];
    class cSat* pSat;     // 0x1FC (0x5DC)  scenario piece of the room 11E rock (emRockSatSet)
};

#define EMROCK_WK(em) ((EmRockWork*) (((cEmRock*) (em))->free))

// Rolling rock enemy (game/emrock.cpp): the boulders that chase the player, hang on a parent
// model, fall, get thrown by El Gigante or drop on the player.
class cEmRock : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMROCK_WK)
    virtual void beginEvent();
    virtual void move();

    void setParent(cEm* parent, int partsNo, int flag);
    void setFall(EmAtkInfo* atk);
    void setThrow(Vec* spd, EmAtkInfo* atk);
    void setThrow2(Vec* spd, EmAtkInfo* atk);
    void setSeFall(u8 blk, u8 no, u8 vol);
    void setEffFall(u8 id, u8 type);
    void setEffAlways(int id, int type);
    void setYarareCube(Vec* size, f32 x, f32 y, f32 z);
    void setTransMode(int on);
    void setPlMotion(void** mot);
    void setScale(f32 s);
    void setDropMot(void* a, void* b, void* c, void* d);
    void setDropMot2(void* a, void* b, void* c, void* d, void* e, void* f, void* g);
    void setBreakR11E();
};

cEmRock* SetRock(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type);

extern "C" {
void emRockDmCk(cEmRock* em);
void emRock_R0_Init(cEmRock* em);
void emRock_R1_Set(cEmRock* em);
void emRock_R1_Parent(cEmRock* em);
void emRock_R1_Fall(cEmRock* em);
void emRock_R1_Throw(cEmRock* em);
void emRock_R1_Throw2(cEmRock* em);
void emRock_R1_Roll(cEmRock* em);
void emRock_R1_Drop(cEmRock* em);
void emRock_R1_Drop2(cEmRock* em);
void plemDropEscAction(cEmRock* em);
void plemDropFind(cPlayer* pl);
void plemDropEscape(cPlayer* pl);
int emRockRollHitCk(cEmRock* em);
void emRockAtkScrCk(cEmRock* em);
int emRockSetRollRoute(cEmRock* em);
int emRockSetRollSpd(cEmRock* em);
int emRockRollStartCk(cEmRock* em);
void plemRockEscape(cPlayer* pl);
int plemRockSetEscapeRoute();
int plemRockEscapeCk(cPlayer* pl);
void plemRockEscAction(cEmRock* em);
void plemRockEscapeCamMove(cPlayer* pl, f32 rate);
void plemRockEscapeCamMove2(cPlayer* pl, int side);
void plemRockDropDieCamMove(cEmRock* em);
void emRockPushCamMove(cEmRock* em);
void emRockPushCamMove2(cEmRock* em);
void emRockDropCamMove(cEmRock* em);
void emRockRunDownCk(cEmRock* em);
int emRockAtkCk(cEmRock* em, struct EmAtkInfo* atk, int type, f32 r);
void emRockPushCk(cEmRock* em, int frame);
int emRockDropHitCk(cEmRock* em);
int emRockDropHitCkSub(cEmRock* em);
int emRockDropHitCkEm2b(cEmRock* em);
void plemDropDie(cPlayer* pl);
void subemDropDie();
void emRockSatClear(cEmRock* em);
void emRockSatSet(cEmRock* em);
}

#endif
