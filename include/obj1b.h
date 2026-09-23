#ifndef OBJ1B_H
#define OBJ1B_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Spear work (game/obj1b.cpp `cObjSpear`): thrown (R1_Throw), stuck in a parts of the enemy it hit
// (R1_Parent), then falling as a three-point rope (R1_Fall) and fading out (R1_LostWait / Lost).
struct SpearWork {
    u32 flags;            // 0x00  bit0: keep the parent parts matrix as it is (no axis normalize)
    int timer;            // 0x04  LostWait: frames before the fade (120); Throw: frames between the flight SEs
    int timer2;           // 0x08  Throw: flight frames left (60)
    u8 pad_C[0xC];
    cModel* parent;       // 0x18
    int partsNo;          // 0x1C
    Vec spd[3];           // 0x20  rope point speeds (R1_Fall)
    Vec throwSpd;         // 0x44
    int x50;              // 0x50
    int parentTimer;      // 0x54  frames until the spear falls off its parent (1800)
    int estTimer;         // 0x58  frames of the stuck-in-boss effect (600, every 2nd frame)
    u8 seBlk;             // 0x5C  landing SE (0xFF = none)
    u8 seNo;              // 0x5D
    u8 seId;              // 0x5E
    u8 sePlayed;          // 0x5F
    u8 type;              // 0x60  rope offsets table row (R1_Fall)
    u8 se2Blk;            // 0x61
    u8 se2No;             // 0x62
    u8 se2Id;             // 0x63
    u8 se3Blk;            // 0x64
    u8 se3No;             // 0x65
    u8 se3Id;             // 0x66
    u8 throwSeBlk;        // 0x67  flight SE (0xFF = none)
    u8 throwSeNo;         // 0x68
    u8 throwSeId;         // 0x69
    u8 estNo;             // 0x6A  landing effect (0xFF = none)
    u8 estPrm;            // 0x6B
    u8 x6C;               // 0x6C
    u8 x6D;               // 0x6D
    u8 x6E;               // 0x6E
    u8 x6F;               // 0x6F
    u8 espId;             // 0x70  effect owner id deleted on landing (0x32)
};

// Spear (obj 0x1B): thrown by an enemy (R1_Throw), sticks into the enemy it hits (R1_Parent:
// follows a parts of the target), falls off as a three-point rope (R1_Fall) and fades out (Lost).
class cObjSpear : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  SpearWork

    virtual void move();
    virtual void beginEvent(u32 mode);
    virtual ~cObjSpear() {}
    void setParent(cModel* parent, int partsNo, int noNormalize);
    void setFall(u8 type, Vec* dir);
    void setThrow(Vec* dir);
    void setLost();
};

#define SPEAR_WK(o) ((SpearWork*) (o)->free)

#endif
