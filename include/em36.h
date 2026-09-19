#ifndef EM36_H
#define EM36_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cEm36;
class cObj;
class cModelInfo;

// One regenerating limb entry of the em36 work (0xC bytes): the parts the limb is made of.
struct Em36Limb {
    cObj* pObj;           // 0x00  the cObj00 weak point (em36WeakInit)
    s16 hp;               // 0x04
    int hit;              // 0x08  index into Em36Work::hit
};

class cObj16;

// The three tentacle objects growing out of a lost limb (em36RegeneTenMove), 0xC bytes.
struct Em36Ten {
    cObj16* obj[3];
};

// Work of the em36 enemy (em36 module, D:/Bio4/Prog/em36.cpp), overlaid on cEm from 0x3E0.
struct Em36Work {
    u32 flags;            // 0x000 (0x3E0)
    int timer;            // 0x004 (0x3E4)
    int timer2;           // 0x008 (0x3E8)
    int seTimer;          // 0x00C (0x3EC)  em36_R1_Die_*: frames of the death voice
    f32 turnAng;          // 0x010 (0x3F0)  em36_R1_Turn target yaw
    int x014;             // 0x014 (0x3F4)
    int x018;             // 0x018 (0x3F8)
    int x01C;             // 0x01C (0x3FC)
    int x020;             // 0x020 (0x400)
    int x024;             // 0x024 (0x404)
    YARARE_INFO hit[30];    // 0x028 (0x408)  hit boxes (YarareAdd)
    f32 routeAng;         // 0x640 (0xA20)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x644 (0xA24)
    f32 subAng;           // 0x648 (0xA28)  towards the partner
    f32 subAngAbs;        // 0x64C (0xA2C)
    f32 subDist2;         // 0x650 (0xA30)  squared XZ distance to the partner
    f32 targetAng;        // 0x654 (0xA34)  copy of the chosen target's angle / distance
    f32 targetAngAbs;     // 0x658 (0xA38)
    f32 targetDist;       // 0x65C (0xA3C)
    f32 plRouteDis;       // 0x660 (0xA40)  RouteCkPosToPosDis to the player / partner
    f32 subRouteDis;      // 0x664 (0xA44)
    Vec routePos;         // 0x668 (0xA48)
    Vec subRoutePos;      // 0x674 (0xA54)
    Vec targetPos;        // 0x680 (0xA60)
    cEm* pTarget;         // 0x68C (0xA6C)
    Em36Limb limb[5];     // 0x690 (0xA70)
    Em36Ten ten[7];       // 0x6CC (0xAAC)
    cModelInfo* pParts[7];  // 0x720 (0xB00)  the limb models (em36PartsSet)
    u32 flags2;           // 0x73C (0xB1C)
    int atkTimer[7];      // 0x740 (0xB20)
    Vec slopeRot;         // 0x75C (0xB3C)  em36SlopeMove: the body tilt (x) the matrix is rotated by
    s16 slopeTimer;       // 0x768 (0xB48)
    u8 pad_76A[2];
    f32 slopeLift;        // 0x76C (0xB4C)  em36SlopeMove: the vertical offset
    f32 jumpAng;          // 0x770 (0xB50)  em36_R1_JumpDown yaw target
    Vec fanceVec;         // 0x774 (0xB54)  em36_R1_FanceOver movement left
    f32 x780;             // 0x780 (0xB60)
    f32 scale;            // 0x784 (0xB64)
    f32 spine[14];        // 0x788 (0xB68)
    int wait;             // 0x7C0 (0xBA0)
    int findWait;         // 0x7C4 (0xBA4)
    int seWait;           // 0x7C8 (0xBA8)
    int dieTimer;         // 0x7CC (0xBAC)
    u32 stuckCnt;         // 0x7D0 (0xBB0)
    u32 sndId;            // 0x7D4 (0xBB4)  catch voice handle (SndStop)
    u16 voiceTimer;       // 0x7D8 (0xBB8)  frames until the next breath voice (em36BreathSe)
    u16 tenSeTimer;       // 0x7DA (0xBBA)  em36RegeneTenMove: frames until the next tentacle voice
    s16 lostTimer;        // 0x7DC (0xBBC)  frames since a limb was lost (em36LostParts)
    u8 frameCnt;          // 0x7DE (0xBBE)
    u8 espKind[4];        // 0x7DF (0xBBF)  EspPullCoreKind results 0x41..0x44
    u8 atkHit;            // 0x7E3 (0xBC3)  the attack hit the player
};

#define EM36_WK(em) ((Em36Work*) (((cEm36*) (em))->free))

class cEm36 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM36_WK)
    virtual void move();
    virtual void setR307Appear();
    virtual int ckFindPL();
    virtual void setFindPL();
};

void Em36Init(cEm* em);
void em36DmCk(cEm36* em);
void em36RouteCk(cEm36* em);
void em36NeckMove(cEm36* em);
int em36AtkCk(cEm36* em, u32 no, int parts);
int em36AtkCk2(cEm36* em, int no, Vec* pos, Vec* oldPos);
int em36GetDmPosType(cEm36* em);
int em36LostParts(cEm36* em);
int em36RegeneCk(cEm36* em);
void em36RegeneCk2(cEm36* em);
void em36YarareCk(cEm36* em);
int em36CatchCk(cEm36* em);
int em36BiteCk(cEm36* em);
int em36LongCatchCk(cEm36* em);
int em36BetweenHitCk(cEm36* em);
void em36WeakInit(cEm36* em);
void em36WeakMove(cEm36* em);
void em36SlopeMove(cEm36* em);
int em36SetDmVal(cEm36* em);
void em36SetHitMark(cEm36* em, int big);
void em36DoorOpenCk(cEm36* em);
int em36AtkRtnCk(cEm36* em);
void em36PartsSet(cEm36* em, int no, int on);
void em36RegeneTenMove(cEm36* em);
void em36RegeneTenClear(cEm36* em, int no);
void em36BloodSet(cEm36* em);
int em36FindCk(cEm36* em);
int em36JumpDownCk(cEm36* em);
int em36FanceOverCk(cEm36* em);
void em36BreathSe(cEm36* em);
void em36BreathSeStopCk(cEm36* em);
void em36VoiceSet(cEm* em, int no, int kind);
void em36SpineScaleMove(cEm36* em);
int em36CrashCk(cEm36* em);
void em36ScaleCompress(cEm36* em);

#endif
