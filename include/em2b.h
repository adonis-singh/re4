#ifndef EM2B_H
#define EM2B_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "camera.h"
#include "model.h"
#include "pendulum.h"
#include "obj.h"
#include "obj16.h"

// Room_flg bits of room 1-19 (the quarry: the huts the rocks and the giant destroy, the dog), from
// the PS2 symbols; RmfFlagChk(pG, n).
enum R119_FLAG {
    RMF_R119_DESTROY_KOYA_A = 0,
    RMF_R119_DESTROY_KOYA_B = 1,
    RMF_R119_DESTROY_KOYA_C = 2,
    RMF_R119_DESTROY_YANE_A = 3,
    RMF_R119_DESTROY_YANE_B = 4,
    RMF_R119_DESTROY_YANE_C = 5,
    RMF_R119_DOG_APPEAR = 6,
    RMF_R119_PARASIET = 7,
};

class cCtrl;
class cEmTree;
class cObjYagura;
class TexRenderMng;
class cEmRock;

// Work of the em2b enemy (em2b module, D:/Bio4/Prog/em2b.cpp): the giant, overlaid on cEm from
// 0x3E0. Field names are work-relative offsets; the comment gives the cEm offset.
struct Em2bWork {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: route found, bit2: targets the partner, bit3: damage / die routine, bit4: routine running,
                          //                bit7: targets the friend (0x26C), bit9: parasite out, bit13: parasite hit
    int Timer;            // 0x004 (0x3E4)
    int Timer2;           // 0x008 (0x3E8)
    int TmpU32;             // 0x00C (0x3EC)
    int TmpU32b;              // 0x010 (0x3F0)
    Vec TmpV;          // 0x014 (0x3F4)  pos of the previous frame (GetTree / TreeAtk move the tree by the delta)
    YARARE_INFO hit[10];    // 0x020 (0x400)  extra hit boxes (YarareAdd in em2b_R0_Init); hit[9] is the parasite (parts 0x3F)
    f32 Pl_dir;         // 0x228 (0x608)
    f32 Pl_rot;      // 0x22C (0x60C)
    u8 pad_230[8];
    f32 Go_dir;        // 0x238 (0x618)
    f32 Go_rot;     // 0x23C (0x61C)
    f32 L_go;       // 0x240 (0x620)  squared
    Vec Pl_pos;         // 0x244 (0x624)
    u8 pad_250[0xC];
    Vec Go_pos;        // 0x25C (0x63C)
    cEm* pEm;         // 0x268 (0x648)
    cEm* pFriend;         // 0x26C (0x64C)
    Vec moveVec;          // 0x270 (0x650)  remaining move towards the tree / rock (a tenth per frame)
    cCtrl* pCtrlGroup;       // 0x27C (0x65C)  GetCtrlCtrl12()
    cModelInfo* pHead;    // 0x280 (0x660)
    TexRenderMng* pMgr;   // 0x284 (0x664)  Ctrl12GetTexRenderEm2b (room 224)
    u8 Tex_buf[8];       // 0x288 (0x668)  cModelInfo::setTexBlendTbl table (em2b_R1_HoleAtk)
    u8 pad_290[0x18];
    f32 Blend;         // 0x2A8 (0x688)
    int Hokan;         // 0x2AC (0x68C)
    int Frame;         // 0x2B0 (0x690)
    MotionWorkSub Sub_mot;  // 0x2B4 (0x694)  second motion work (cModel::Motion.blend)
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
    struct EmiEntry* pHouse;   // 0x510 (0x8F0)  EMI entry (embarrel.h) of the house being broken (em2bPlInHouseCk, type 3; state: 0 intact, 1 hit, 3 broken)  (PS2 EMINFO_WK* pHouse)
    struct EmiEntry* pGoto;    // 0x514 (0x8F4)  rock spot walked to (em2bSearchRockCk, type 4); type 0xD = a catch spot (rotY)
    CAMERA Cam;           // 0x518 (0x8F8)  event camera of the catch / escape scenes
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
    int Go_dog_timer;          // 0x638 (0xA18)  frames left heading for the dog (900 when pFriend is set) (PS2 Go_dog_timer; was `dmGuard`)
    int Dog_wait;             // 0x63C (0xA1C)
    int Event_wait;             // 0x640 (0xA20)
    f32 Compress_y;        // 0x644 (0xA24)  y scale (1 -> 0.1 while dying) (PS2 Compress_y)
    u8 espKind;           // 0x648 (0xA28)
    u8 Atk_ck;            // 0x649 (0xA29)
    u8 Ft_axis;           // 0x64A (0xA2A)  selects the effect set (PS2 Ft_axis)
    u8 Button_mode;            // 0x64B (0xA2B)  parasite attack button: 1 = 0x40000, 0 = 0x80000 (Key.trg)
    u8 Eff;          // 0x64C (0xA2C)  effect kind for EstSet / EmDmBloodSet2 (PS2 Eff)
    u8 Debug_atk_rtn;          // 0x64D (0xA2D)
};


#define EM2B_WK(em) ((Em2bWork*) (((cEm2b*) (em))->free))
#define EM2B_BLEND_MOT(w) ((MotionWork*) &(w)->Sub_mot)

class cEm2b : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2B_WK)
    virtual ~cEm2b();
    virtual void move();
    virtual void setNoSuspend(int onoff);
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
void em2bBlendMotSet(cEm2b* em, void* m0, void* m1, void* m2, int a, int b, int c, int d);
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
