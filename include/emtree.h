#ifndef EMTREE_H
#define EMTREE_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"

// One rope node of the falling tree (emTree_R1_Fall): three point masses joined by distance
// constraints; the model matrix is rebuilt from them every frame.
struct EmTreeNode {
    Vec pos;      // 0x00
    Vec old;      // 0x0C  position before this frame's move
    Vec spd;      // 0x18
    f32 len;      // 0x24  rest distance to the next node
    int onFloor;  // 0x28  1 when the node was clamped to the floor this frame
};

// Work of the tree enemy (game/emtree.cpp), overlaid on cEm from 0x3E0.
struct EmTreeWork {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: setParent flag (no matrix normalisation), bit1: hidden
    int Timer;            // 0x004 (0x3E4)
    int Timer2;           // 0x008 (0x3E8)  emTree_R1_Shot: frames before the tree is lost
    u8 pad_C[0x18 - 0xC];
    int fallTimer;        // 0x018 (0x3F8)  emTree_R1_Parent: frames until setFall (30 when the player survived)
    cModel* pParent;      // 0x01C (0x3FC)  model the tree follows (setParent)
    int pEm_old;              // 0x020 (0x400)
    int oya_parts;          // 0x024 (0x404)
    u32 sndId;            // 0x028 (0x408)  handle of the looping sound (seAlways)
    Vec pt[3];            // 0x02C (0x40C)  node speeds kept between frames (setFall randomises them)
    Vec spd;              // 0x050 (0x430)  throw / shot speed
    u8 seFall[3];         // 0x05C (0x43C)  blk, no, id of the landing sound (0xFF = none)
    u8 landed;            // 0x05F (0x43F)  landing sound / effect done
    u8 pad_60;
    u8 seHit[3];          // 0x061 (0x441)  hit the player
    u8 se64[3];           // 0x064 (0x444)
    u8 seAlways[3];       // 0x067 (0x447)  looping sound while thrown / shot
    u8 seAlwaysWait;      // 0x06A (0x44A)  frames between its restarts (4)
    u8 seWall[3];         // 0x06B (0x44B)  hit the scenario
    u8 effFall[2];        // 0x06E (0x44E)  EstSet id / type when the fall ends (0xFF = none)
    u8 effHit[2];         // 0x070 (0x450)  EmPlBloodSet2 arguments when the player is hit
    u8 eff72[2];          // 0x072 (0x452)
    u8 estNo;             // 0x074 (0x454)  effect number deleted when the tree lands (50)
    u8 caught;            // 0x075 (0x455)  setCatch / ckCatch
    u8 pad_76[2];
    EmAtkInfo* pAtk;      // 0x078 (0x458)  attack info used against the player (emTreeAtk by default)
};

#define EMTREE_WK(em) ((EmTreeWork*) (((cEmTree*) (em))->free))

// Tree enemy: a felled tree trunk that hangs on a parent's parts (setParent), falls as a rope of
// three nodes (setFall) or is thrown / shot at the player.
class cEmTree : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMTREE_WK)
    virtual void beginEvent();
    virtual void move();

    void setParent(cModel* parent, int partsNo, int flag);
    void clearParent();
    void setFall();
    void setThrow(Vec* spd, EmAtkInfo* atk);
    void setShot(Vec* spd, EmAtkInfo* atk);
    int ckCatch();        // 1 while not caught
    void setCatch();
    void setLost();
};

extern EmAtkInfo emTreeAtk;

extern "C" {
cEmTree* SetTree(void* bin, void* tpl, Vec* pos, Vec* rot);
void emTreeDmCk(cEmTree* em);
void emTree_R0_Init(cEmTree* em);
void emTree_R1_Set(cEmTree* em);
void emTree_R1_LostWait(cEmTree* em);
void emTree_R1_Lost(cEmTree* em);
void emTree_R1_Parent(cEmTree* em);
void emTree_R1_Fall(cEmTree* em);
void emTree_R1_Throw(cEmTree* em);
void emTree_R1_Shot(cEmTree* em);
}

#endif
