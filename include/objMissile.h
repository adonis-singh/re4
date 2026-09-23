#ifndef OBJMISSILE_H
#define OBJMISSILE_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Helicopter missile work (game/objMissile.cpp `cObjMissile`): hangs from a parts of the
// helicopter (setParent), then flies toward `target` (setFire) and explodes (objMissileBomb).
struct MissileWork {
    u32 Be_flg;              // 0x00
    int Timer;            // 0x04  fire wait / flight frames
    int Timer2;          // 0x08  frames before the hit checks start
    cModel* parent;       // 0x0C
    int oya_parts;          // 0x10
    int scale_mode;      // 0x14  keep the parent parts matrix as it is
    Vec Target;           // 0x18
    class cEmHit* pHit;    // 0x24
    u8 Target_ok;         // 0x28
    u8 pad_29[3];
    Vec Spd;              // 0x2C
};

// Helicopter missile: follows a parts of the helicopter (R0_Parent), waits (R0_FireWait), flies
// toward its target and explodes on the scenario / an enemy (R0_Fire, objMissileBomb).
class cObjMissile : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  MissileWork

    virtual void move();
    virtual ~cObjMissile() {}
    void setParent(cModel* parent, int partsNo, int noNormalize);
    void setFire(Vec* target);
};

#define MISSILE_WK(o) ((MissileWork*) (o)->free)

#endif
