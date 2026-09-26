#ifndef EM22_H
#define EM22_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"
#include "camera.h"

class cCtrl;
class cEm22;

// Work of the em22 enemy (em22 module, D:/Bio4/Prog/em22.cpp), overlaid on cEm from 0x3E0.
struct Em22Work {
    u32 flags;            // 0x000 (0x3E0)  bit0: the player is in sight (Em22RouteCk), bit1: waking up, bit2: neck follows the player,
                          //                bit3: jumping (no damage switch), bit5: growing a parasite, bit7: in the air (checkAir),
                          //                bit9: parasite attack, bit10: killed by the damage manager
    int timer;            // 0x004 (0x3E4)
    int timer2;           // 0x008 (0x3E8)
    f32 delta;            // 0x00C (0x3EC)  em22_R1_Turn: angle left to turn; em22_R1_Jump: height left to climb
    int camType;          // 0x010 (0x3F0)  em22GetCamType result of the catch scenes
    f32 routeAng;         // 0x014 (0x3F4)  Muku towards the route point to the player
    f32 routeAngAbs;      // 0x018 (0x3F8)
    u8 pad_1C[8];
    f32 targetAng;        // 0x024 (0x404)  the same towards the current target (player, goto point or escape point)
    f32 targetAngAbs;     // 0x028 (0x408)
    f32 targetDist2;      // 0x02C (0x40C)
    f32 plDist;           // 0x030 (0x410)  RouteCkPosToPosDis to the player
    Vec plRoutePos;       // 0x034 (0x414)  RouteCkToPos result towards the player
    u8 pad_40[0xC];
    Vec routePos;         // 0x04C (0x42C)  route point the routines run to
    cEm* pTarget;         // 0x058 (0x438)
    Vec blowSpd;          // 0x05C (0x43C)  em22_R1_Dm_Blow: speed of the blown-away fall

    int gotoOn;           // 0x068 (0x448)  cEm22::setGoto: run to gotoPos (em22_R1_Goto)
    Vec gotoPos;          // 0x06C (0x44C)
    CAMERA cam;           // 0x078 (0x458)  event camera of the catch scenes (em22CamMove)
    YARARE_INFO hit[5];     // 0x170 (0x550)  extra hit boxes (em22YarareInit)
    class cObj16* pPara[5];     // 0x274 (0x654)  parasites on the back (em22SetParasite)
    class cObj16* pParaAtk[3];  // 0x288 (0x668)  parasites of the attack (em22SetParasiteAtk)
    cCtrl* pCtrl12;       // 0x294 (0x674)  GetCtrlCtrl12()
    cCtrl* pCtrl11;       // 0x298 (0x678)  GetCtrlCtrl11()
    f32 neckX;            // 0x29C (0x67C)  em22NeckMove: head pitch
    f32 neckY;            // 0x2A0 (0x680)  head yaw
    int plDeadWait;       // 0x2A4 (0x684)  frames since the player died (no attack)
    int stuckTimer;       // 0x2A8 (0x688)  slow turn while stuck
    int escTimer;         // 0x2AC (0x68C)  escaping (Em22RouteCk uses RouteCkEscEm)
    int stuckCnt;         // 0x2B0 (0x690)  frames in a row the enemy barely moved
    int paraWait;         // 0x2B4 (0x694)  frames until a parasite may grow (em22SetParasiteCk)
    int lockCnt;          // 0x2B8 (0x698)  frames the player has aimed at the enemy (em22LockCk)
    u32 sndId[3];         // 0x2BC (0x69C)  catch scene SE handles (SndStop)
    int Seid_foot;             // 0x2C8 (0x6A8)  em22FootSeControl Ctrl11SetSe handle, SndStop on death
    int slaverTimer;      // 0x2CC (0x6AC)  em22SlaverSet
    int x2D0;             // 0x2D0 (0x6B0)
    int voiceTimer;       // 0x2D4 (0x6B4)  frames until the next growl while a parasite is out
    f32 tilt;             // 0x2D8 (0x6B8)  em22DirMatrix: body roll of the run
    f32 scale;            // 0x2DC (0x6BC)  em22ScaleCompress
    u8 espKind;           // 0x2E0 (0x6C0)  EspPullCoreKind at creation
};

#define EM22_WK(em) ((Em22Work*) (((cEm22*) (em))->free))

class cEm22 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM22_WK)
    virtual void move();
    virtual void setGoto(Vec* pos, int on);
};

void Em22Init(cEm* em);
void em22DmCk(cEm22* em);
void Em22RouteCk(cEm22* em);
void em22DirMatrix(cEm22* em, f32 dir);
void em22NeckMove(cEm22* em);
void em22YarareInit(cEm22* em);
void em22BloodSet(cEm22* em);
int em22SetDmVal(cEm22* em);
int em22GetCamType(cEm22* em);
void em22CamMove(cEm22* em, int type);
int em22SetParasiteCk(cEm22* em);
void em22SetParasite(cEm22* em);
void em22SetParasiteAtk(cEm22* em);
void em22ParaSetMotWait(cEm22* em);
void em22ParaSetMotAtk(cEm22* em);
void em22ParaSetMotAtkHit(cEm22* em);
void em22AtkParaClearCk(cEm22* em);
void em22OpenBack(cEm22* em);
int em22LockCk(cEm22* em);
void em22ScaleCompress(cEm22* em);
void em22FootSeControl(cEm22* em);
int em22JumpCk(cEm22* em);
int em22PlRunCk(cEm22* em);
int em22PlRunCk2(cEm22* em);
void em22FootEff(cEm22* em);
void em22ParaAtkHitPosSet(cEm22* em);
void em22SlaverSet(cEm22* em, int run);
int em22GotoCk(cEm22* em);
int em22ScreenInCk(cEm22* em);
void em22DoorOpenCk(cEm22* em);

#endif
