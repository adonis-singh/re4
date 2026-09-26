#ifndef OBJ1C_H
#define OBJ1C_H

#include "types.h"
#include "vec.h"
#include "obj.h"

cObj* SetFloatIsland(void* bin, void* tpl, Vec* pos, Vec* rot);

// Floating island work (game/obj1c.cpp): drifts back toward its home position, plays crash
// motions and spawns effects while the player is on it.
struct IslandWork {
    u32 Be_flg;             // 0x00
    u8 pad_4[8];            // PS2's FREE_OBJ1C has Timer/Timer2 here, unused by our code
    int Crash_wait;         // 0x0C  frames since setCrashBig (ckCrash)
    int Eff_wait;           // 0x10  frames until the next idle effect
    int Eff_wait2;          // 0x14  frames the crash effect is suppressed
    Vec Spd;                // 0x18  push speed (setCrashBig)
    Vec St_pos;             // 0x24  position it drifts back to
    u8 EffKindId;           // 0x30  effect kind (EspPullCoreKind)
    u8 pad_31[3];
    void* motIdle;          // 0x34  motions: idle / crash, and their big-scale (>= 1.5) variants
    void* motCrash;         // 0x38
    void* motIdleBig;       // 0x3C
    void* motCrashBig;      // 0x40
};

// Floating island (the lake raft): drifts back to its home position, gets pushed and plays a
// crash motion when hit, spawns water effects while alive.
class cObj1c : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  IslandWork

    virtual void move();
    void setMotion(void* idle, void* crash, void* idleBig, void* crashBig);
    void setCrash();
    void setCrashBig(Vec* from);
    int ckCrash();
};

#define ISLAND_WK(o) ((IslandWork*) (o)->free)

#endif
