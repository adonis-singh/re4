#ifndef EM25_H
#define EM25_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cObj16;

// Work of the em25 enemy (em25 module, D:/Bio4/Prog/em25.cpp): the parasite that bursts out of a
// host's head (mode 1, attached to a parent enemy's parts: P_ routines) or crawls on the floor
// (mode 0). Overlaid on cEm from 0x3E0; the comment gives the cEm offset.
struct Em25Work {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: route to the player found, bit3: damage routine, bit4: parasite objects set (em25SetParasite),
                          //                bit5: parent head turned away (em25_R1_P_Wait / em25OnParent); the low bits (0x2F) are cleared every frame
    int Timer;            // 0x004 (0x3E4)
    int Timer2;       // 0x008 (0x3E8)  em25_R1_Bite: frames the catch motion blends
    u32 sndId;            // 0x00C (0x3EC)  bite SE handle (SndStop)
    YARARE_INFO hit[3];     // 0x010 (0x3F0)  extra hit boxes (YarareAdd in em25_R0_Init)
    u8 pad_AC[0x218 - 0xAC];
    f32 routeAng;         // 0x218 (0x5F8)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x21C (0x5FC)
    u8 pad_220[8];
    f32 targetAng;        // 0x228 (0x608)  copy of the route angle / distance
    f32 targetAngAbs;     // 0x22C (0x60C)
    f32 targetDist;       // 0x230 (0x610)
    Vec routePos;         // 0x234 (0x614)  RouteCkToPos result towards the player
    u8 pad_240[0xC];
    Vec targetPos;        // 0x24C (0x62C)
    cEm* pEm;         // 0x258 (0x638)  pPL
    cEm* pEm_oya;         // 0x25C (0x63C)  host enemy (setParent), NULL on the floor
    int parentParts;      // 0x260 (0x640)  host parts the parasite is attached to
    u8 pad_264[0x378 - 0x264];
    cObj16* pPara[3];     // 0x378 (0x758)  the three tentacle objects (em25SetParasite, type 7)
    u32 hitCnt;           // 0x384 (0x764)  damage counter 0..3
    int Alive_timer;        // 0x388 (0x768)  frames until the parasite dies by itself (900)
    f32 Compress_y;           // 0x38C (0x76C)  em25ScaleCompress: y scale of the parts (the die routines shrink it)
    int Mode;             // 0x390 (0x770)  1 while attached to a parent
    int Atk_wait;        // 0x394 (0x774)
    int Fire_timer;          // 0x398 (0x778)  DmgMgr hit guard timer
    u8 EffKindId;           // 0x39C (0x77C)  EspPullCoreKind at creation
    u8 pad_39D[3];
    int Se_breath_wait;          // 0x3A0 (0x780)  frames until the next crawl SE
    int Eff_wait1;             // 0x3A4 (0x784)
    int estTimer;         // 0x3A8 (0x788)  frames until the next attached effect
    u8 dead;              // 0x3AC (0x78C)  ckDie
    u8 atkHit;            // 0x3AD (0x78D)  the attack already hit (em25AtkCk)
    u8 Atk_enable;         // 0x3AE (0x78E)  ckAtkEnable
};

#define EM25_WK(em) ((Em25Work*) (((cEm25*) (em))->free))

class cEm25 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM25_WK)
    virtual void move();
    virtual int ckParent();          // 1 while no parent is set
    virtual void setParent(cEm* parent, int parts, Vec* pos, Vec* rot);
    virtual void setWait();
    virtual void setAtk();
    virtual int ckAtkHit();
    virtual void setPoison();
    virtual int ckAtkEnd();
    virtual void setDie();
    virtual int ckDie();
    virtual void setGoOut(int flag);
    virtual void setHide();
    virtual int ckHide();
    virtual void setBirth(Vec* pos, f32 ang);
    virtual int ckAtkEnable();
    virtual void setDamage();
    virtual int ckLock();
};

typedef void (*Em25Func)(cEm25*);

void Em25Init(cEm* em);
void em25DmCk(cEm25* em);
void em25OnParent(cEm25* em);
int em25AtkCk(cEm25* em, int no, int parts);
int em25CatchCk(cEm25* em);
void em25ScaleCompress(cEm25* em);
void em25RouteCk(cEm25* em);
int em25SetDmVal(cEm25* em);
void em25SetParasite(cEm25* em);
void em25ClearParasite(cEm25* em);
void em25BloodSet(cEm25* em);
void em25PlHeadLost();
void em25SetPoison(cEm25* em);

#endif
