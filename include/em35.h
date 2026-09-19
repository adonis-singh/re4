#ifndef EM35_H
#define EM35_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "camera.h"
#include "pl_cloth.h"

class cObj;
class cModelInfo;
class cEm35;

// One beam of the room's beam graph (em35_beam_tbl, 0x24 bytes): the links to the neighbouring
// beams (0xFF = none) and the two end points.
struct Em35Beam {
    u16 type;             // 0x00  0: runs along X, 1: runs along Z (em35BeamDirCk)
    u8 up;                // 0x02  beam above (em35BeamUpCk)
    u8 down;              // 0x03  beam below (em35BeamDownCk)
    u8 side[2];           // 0x04  side beams (em35BeamSideCk)
    u8 front;             // 0x06  next beam ahead / behind (em35BeamFrontCk / em35BeamBackCk)
    u8 back;              // 0x07
    u8 pad_8[4];
    Vec a;                // 0x0C  end points
    Vec b;                // 0x18
};

// Work of the em35 enemy (em35 module, D:/Bio4/Prog/em35.cpp), overlaid on cEm from 0x3E0. cModel::type 0
// is the whole enemy, type 1 the upper body that climbs the beams (em35_R1_U_*), type 2 the divided legs.
struct Em35Work {
    u32 flags;            // 0x000 (0x3E0)  bit0: route to the player found, bit3: damage / die routine, bit4: routine running (neck follows),
                          //                bit5: upper body in the air / crawling (cloth off), bit6: divide (cloth off), bit7: attacking (no damage switch)
    int timer;            // 0x004 (0x3E4)
    int timer2;           // 0x008 (0x3E8)
    int atkTimer;         // 0x00C (0x3EC)  frames the attack keeps flags bit7
    int walkType;         // 0x010 (0x3F0)  Rnd() % 3 walk variant / AtkDouble second hit
    f32 jumpAng;          // 0x014 (0x3F4)  Atk2F / U_JumpToBeam: yaw the jump turns to
    Vec jumpSpd;          // 0x018 (0x3F8)  U_OverStep / U_JumpToBeam: movement left
    YARARE_INFO hit[26];    // 0x024 (0x404)  extra hit boxes (YarareAdd)
    u8 pad_56C[0x63C - 0x56C];
    f32 routeAng;         // 0x63C (0xA1C)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x640 (0xA20)
    f32 subAng;           // 0x644 (0xA24)
    f32 subAngAbs;        // 0x648 (0xA28)
    f32 targetAng;        // 0x64C (0xA2C)  copy of the chosen target's angle / distance
    f32 targetAngAbs;     // 0x650 (0xA30)
    f32 targetDist;       // 0x654 (0xA34)
    Vec routePos;         // 0x658 (0xA38)  RouteCkToPos result towards the player
    Vec subRoutePos;      // 0x664 (0xA44)
    Vec targetPos;        // 0x670 (0xA50)  chosen target position
    cEm* pTarget;         // 0x67C (0xA5C)  pPL
    Camera cam;           // 0x680 (0xA60)  event camera (em35EscapeCamMove / em35StampCamMove)
    f32 neckAng;          // 0x778 (0xB58)  smoothed neck yaw (em35NeckMove)
    PlCloth cloth1;       // 0x77C (0xB5C)  em35ClothSet / em35ClothMove (type 1 tail)
    PlCloth cloth2;       // 0x7DC (0xBBC)  em35ClothSet2 / ClothSet3 (the hanging skin)
    f32 scaleAng;         // 0x83C (0xC1C)  em35ScaleMove phase
    cModelInfo* pInfo;    // 0x840 (0xC20)  extra model (type 0)
    cObj* pWeak[4];       // 0x844 (0xC24)  weak point objects (em35WeakInit, obj00)
    int atkWait;          // 0x854 (0xC34)  frames until the next attack
    int lockWait;         // 0x858 (0xC38)  frames the attack-double / hook are held off
    int weakDmg;          // 0x85C (0xC3C)  damage on the weak parts since the last down
    int dmgCnt;           // 0x860 (0xC40)  damage since the last reaction
    int dieTimer;         // 0x864 (0xC44)  frames since the damage manager killed the enemy
    u32 sndId;            // 0x868 (0xC48)  damage voice handle (SndStop)
    f32 blendRate;        // 0x86C (0xC4C)  em35BlendMotSet weight (-255..255)
    int blendA;           // 0x870 (0xC50)  interpolation frames left
    u32 blendB;           // 0x874 (0xC54)  frame counter of the blended motion
    MotionWorkSub blendMot;  // 0x878 (0xC58)  the blend motion work (cModel::blendMot)
    s16 effTimer;         // 0x948 (0xD28)  frames until the next upper body effect
    u16 seTimer;          // 0x94A (0xD2A)  frames until the next voice
    u8 atkHit;            // 0x94C (0xD2C)  the attack hit the player
    u8 atkHit2;           // 0x94D (0xD2D)  the player escaped the attack (em35AtkEscapeAction)
    u8 beamNo;            // 0x94E (0xD2E)  beam the upper body stands on (em35GetBeamNo), 0xFF = none
    u8 beamType;          // 0x94F (0xD2F)  its Em35Beam::type
    u8 espKind;           // 0x950 (0xD30)  EspPullCoreKind at creation
};

#define EM35_WK(em) ((Em35Work*) (((cEm35*) (em))->free))

class cEm35 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM35_WK)
    virtual void move();
    virtual void setDiePose();
    virtual void setUpperStart();
};

void Em35Init(cEm* em);
void em35DmCk(cEm35* em);
void em35DmCkUpper(cEm35* em);
void em35EscapeCamMove(cEm35* em);
void em35StampCamMove(cEm35* em);
void em35RouteCk(cEm35* em);
void em35NeckMove(cEm35* em);
int em35AtkCk(cEm35* em, u32 no, int parts);
int em35PlFallCk(cEm35* em);
int em35WeakDmCk(cEm35* em);
int em35CatchCk(cEm35* em);
void em35CatchPosSet(cEm35* em);
int em35bPlRunCk(cEm35* em);
void em35ClothSet(cEm35* em);
void em35ClothMove(cEm35* em);
void em35ClothSet2(cEm35* em);
void em35ClothMove2(cEm35* em);
void em35ClothSet3(cEm35* em);
void em35ClothMove3(cEm35* em);
void em35NextRtnSetUpper(cEm35* em);
void em35NextRtnSetUpper2(cEm35* em);
int em35LockCk(cEm35* em);
int em35GetBeamNo(Vec* pos, int type);
f32 em35GetBeamDis(Vec* pos, int no, int side, f32 ang);
int em35BeamFrontCk(cEm35* em, int no);
int em35BeamBackCk(cEm35* em, int no);
int em35BeamFrontDobuleCk(cEm35* em, int no);
int em35BeamUpCk(cEm35* em, int no);
int em35BeamDownCk(cEm35* em, int no);
int em35BeamFrontUpCk(cEm35* em, int no);
int em35BeamFrontDownCk(cEm35* em, int no);
int em35BeamSideStepCk(cEm35* em, int side);
int em35BeamSideCk(int no, int side, f32 ang);
int em35BeamDirCk(int no, f32 ang);
int em35SetDmVal(cEm35* em);
void em35BlendMotSet(cEm35* em, void* m0, void* m1, void* m2, void* m3, int a, int b, int kind);
void em35ScaleMove(cEm35* em);
int em35BigStepCk(cEm35* em);
void em35WeakInit(cEm35* em);
void em35WeakMove(cEm35* em);

#endif
