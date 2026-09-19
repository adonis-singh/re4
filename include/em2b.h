#ifndef EM2B_H
#define EM2B_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "camera.h"
#include "model.h"
#include "pendulum.h"
#include "obj.h"

class cCtrl;
class cEmTree;
class cObjYagura;
class TexRenderMng;
class cEmRock;

// Enemy head object of the parasite (game/obj16.cpp; em10.h declares the same class, which this
// module cannot include).
class cObj16 : public cObj {
public:
    int ckAtkEnable();
    void setDamage();
    void setAtk(u8 a);
    void clearLostWait();
    void setMotData(void* a, void* b, void* c, void* d, void* e, void* f, void* g, void* h, void* i, void* j, void* k);
    void setLostWait(int a);
    void setBurn();
    void setPlDmgMot(void* m, int a);
    void setDieEff();
    void setCritical();
    int ckAtkHit();
    void setScale(Vec* s);
};

// One entry of the room's EMI data (pG->pRoomEmi): kind 3 = a village house the giant can break
// (room 119 sets pG->flags_174 bits per house), kind 4 = a rock spot, kind 0xD = a catch spot.
struct Em2bEmi {
    u8 kind;              // 0x00
    u8 no;                // 0x01
    u8 state;             // 0x02  house: 0 intact, 1 hit, 3 broken
    u8 pad_3;
    Vec pos;              // 0x04
    f32 rot;              // 0x10  kind 0xD (catch spot): y angle
    u8 pad_14[0x40 - 0x14];
};

struct Em2bEmiTbl {
    int num;              // 0x00
    int x4;
    Em2bEmi e[1];         // 0x08
};

// Work of the em2b enemy (em2b module, D:/Bio4/Prog/em2b.cpp): the giant, overlaid on cEm from
// 0x3E0. Field names are work-relative offsets; the comment gives the cEm offset.
struct Em2bWork {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: route found, bit2: targets the partner, bit3: damage / die routine, bit4: routine running,
                          //                bit7: targets the friend (0x26C), bit9: parasite out, bit13: parasite hit
    int Timer;            // 0x004 (0x3E4)
    int Timer2;           // 0x008 (0x3E8)
    int mode;             // 0x00C (0x3EC)
    int TmpU32b;              // 0x010 (0x3F0)
    Vec posSave;          // 0x014 (0x3F4)  pos of the previous frame (GetTree / TreeAtk move the tree by the delta)
    YARARE_INFO hit[10];    // 0x020 (0x400)  extra hit boxes (YarareAdd in em2b_R0_Init); hit[9] is the parasite (parts 0x3F)
    f32 routeAng;         // 0x228 (0x608)
    f32 routeAngAbs;      // 0x22C (0x60C)
    u8 pad_230[8];
    f32 targetAng;        // 0x238 (0x618)
    f32 targetAngAbs;     // 0x23C (0x61C)
    f32 targetDist;       // 0x240 (0x620)  squared
    Vec routePos;         // 0x244 (0x624)
    u8 pad_250[0xC];
    Vec targetPos;        // 0x25C (0x63C)
    cEm* pTarget;         // 0x268 (0x648)
    cEm* pFriend;         // 0x26C (0x64C)
    Vec moveVec;          // 0x270 (0x650)  remaining move towards the tree / rock (a tenth per frame)
    cCtrl* pCtrlGroup;       // 0x27C (0x65C)  GetCtrlCtrl12()
    cModelInfo* pHead;    // 0x280 (0x660)
    TexRenderMng* pMgr;   // 0x284 (0x664)  Ctrl12GetTexRenderEm2b (room 224)
    u8 Tex_buf[8];       // 0x288 (0x668)  cModelInfo::setTexBlendTbl table (em2b_R1_HoleAtk)
    u8 pad_290[0x18];
    f32 Blend;         // 0x2A8 (0x688)
    int blendCnt;         // 0x2AC (0x68C)
    int blendSeq;         // 0x2B0 (0x690)
    MotionWorkSub blendMot;  // 0x2B4 (0x694)  second motion work (cModel::motBlend)
    void* blendM0;        // 0x384 (0x764)
    void* blendM1;        // 0x388 (0x768)
    void* blendM2;        // 0x38C (0x76C)
    int blendA;           // 0x390 (0x770)
    int blendB;           // 0x394 (0x774)
    int blendC;           // 0x398 (0x778)
    int blendD;           // 0x39C (0x77C)
    f32 Neck_dir_y;          // 0x3A0 (0x780)
    PenCloth Cloth;       // 0x3A4 (0x784)  chain cloth (em2bClothSet, type 1)
    PenCloth rope[2];     // 0x404 (0x7E4)  short rope / chain pendulums (em2bShortRopeSet, em2bChainSet)
    cObj* pChain;        // 0x4C4 (0x8A4)  chain object (PS2 pChain before pChain2/pChain3)
    cObj* pChain2;        // 0x4C8 (0x8A8)
    cObj* pChain3;        // 0x4CC (0x8AC)
    cObj16* pParasite;    // 0x4D0 (0x8B0)
    cObj16* pTen[10]; // 0x4D4 (0x8B4)  face tentacles of the parasite (em2bSetTentacle)
    cObjYagura* pYagura;  // 0x4FC (0x8DC)  the tower shaken by the base attack (em2bYaguraSearch)
    cEmTree* pTree;       // 0x500 (0x8E0)  tree held
    cEmTree* pTreeBrk;   // 0x504 (0x8E4)
    cEm* pTreeTarget;      // 0x508 (0x8E8)
    cEmRock* pRock;       // 0x50C (0x8EC)  rock held
    Em2bEmi* pHouse;      // 0x510 (0x8F0)  house being broken (em2bPlInHouseCk, kind 3)
    Em2bEmi* pGoto;       // 0x514 (0x8F4)  rock spot walked to (em2bSearchRockCk, kind 4)
    Camera Cam;           // 0x518 (0x8F8)  event camera of the catch / escape scenes
    int Total_damage;         // 0x610 (0x9F0)
    int Rock_wait;         // 0x614 (0x9F4)
    int Atk_wait;         // 0x618 (0x9F8)
    int Dash_wait;         // 0x61C (0x9FC)
    int Punch_wait;         // 0x620 (0xA00)
    int Tree_brk_wait;         // 0x624 (0xA04)
    int No_go_sub_timer;         // 0x628 (0xA08)
    int Catch_power;         // 0x62C (0xA0C)
    int Parasite_damage;           // 0x630 (0xA10)
    u32 HoseiCnt;         // 0x634 (0xA14)
    int dmGuard;          // 0x638 (0xA18)
    int Dog_wait;             // 0x63C (0xA1C)
    int Event_wait;             // 0x640 (0xA20)
    f32 scaleRate;        // 0x644 (0xA24)
    u8 espKind;           // 0x648 (0xA28)
    u8 Atk_ck;            // 0x649 (0xA29)
    u8 variant;           // 0x64A (0xA2A)
    u8 Button_mode;            // 0x64B (0xA2B)  parasite attack button: 1 = 0x40000, 0 = 0x80000 (Key.trg)
    u8 espKind2;          // 0x64C (0xA2C)
    u8 Debug_atk_rtn;          // 0x64D (0xA2D)
};


#define EM2B_WK(em) ((Em2bWork*) (((cEm2b*) (em))->free))
#define EM2B_BLEND_MOT(w) ((MotionWork*) &(w)->blendMot)

class cEm2b : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2B_WK)
    virtual ~cEm2b();
    virtual void move();
    virtual void setNoSuspend(int on);
    virtual void setPos(Vec* pos, f32 rot);
    virtual void setEventDie();
    virtual int ckParasite();
    virtual int ckR224Drop();
    virtual int ckSit();
};

typedef void (*Em2bFunc)(cEm2b*);

void Em2bInit(cEm* em);
void em2bDmCk(cEm2b* em);
void em2bPlFallCK(cEm2b* em);
void em2bParasiteAtkCamMove(cEm2b* em);
void em2bRouteCk(cEm2b* em);
void em2bNeckMove(cEm2b* em);
void em2bBlendMotSet(cEm2b* em, void* m0, void* m1, void* m2, int a, int b, int c, u16 d);
void em2bClothSet(cEm2b* em);
void em2bClothMove(cEm2b* em);
int em2bAtkCk(cEm2b* em, Vec* a, Vec* b, int no);
void em2bEscapeCamMove(cEm2b* em);
void em2bFootSe(cEm2b* em);
void em2bFtChgCk(cEm2b* em);
void em2bQuakeSet(Vec* pos);
void em2bShortRopeSet(cEm2b* em);
void em2bChainSet(cEm2b* em);
int em2bPlInHouseCk(cEm2b* em);
int em2bSearchTree(cEm2b* em);
int em2bGetTreeCk(cEm2b* em);
int em2bSearchRockCk(cEm2b* em);
int em2bGetRockCk(cEm2b* em);
int em2bTreeAtkCk(cEm2b* em);
int em2bTreeAtkScrCk(cEm2b* em);
void em2bDashScrCk(cEm2b* em, Vec* pos, f32 rad);
void em2bR11eScrBrkCk(cEm2b* em);
void em2bR11eScrBrkCk2(cEm2b* em, Vec* pos, f32 rad);
void em2bPlBlowAtkScrCk(cPlayer* pl);
void em2bBlowCamMove(cEm2b* em, f32 rate);
void em2bStampCamMove(cEm2b* em);
void plBlendMotSet(cPlayer* pl, void* m0, void* m1, int a, int b);
int em2bStaggerCk(cEm2b* em, Vec* pos);
int em2bSearchDog(cEm2b* em);
int em2bAtkRtnCk(cEm2b* em);
int em2bAtkRtnCkDebug(cEm2b* em);
void em2bNextRtnSet(cEm2b* em);
int em2bInScreenCk(cEm2b* em);
int em2bPlDashEscapeCk(cEm2b* em);
int em2bPlRunCk(cEm2b* em);
int em2bPressPlCk(cEm2b* em);
int em2bPressSubCk(cEm2b* em);
void em2bCatchPosSet(cEm2b* em);
void em2bSetTentacle(cEm2b* em, int on);
int em2bSetDmVal(cEm2b* em);
int em2bStayCk(cEm2b* em);
void em2bObaHitCk(cEm2b* em);
void em2bScaleCompress(cEm2b* em);
int em2bFriendCk(cEm2b* em);
void em2bYaguraSearch(cEm2b* em);
void em2bTexrenderInit(cEm2b* em);

#endif
