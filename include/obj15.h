#ifndef OBJ15_H
#define OBJ15_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// game/obj15.cpp: creates the gatling object from the room archive model (r209 GatlingAppear).

// Gatling gun work (game/obj15.cpp `cObjGatling`): a mounted gun the player (or `pEm`) fires
// at `pTarget`; three cEmHit hit boxes take the damage, `pEat` is its effect collision piece.
struct GatlingWork {
    u32 Be_flag;            // 0x00
    int Timer;              // 0x04  frames of barrel spin-down after the break
    u8 pad_8[2];
    s16 Fire_timer;         // 0x0A  frames since firing started (shots every 3rd frame after 30)
    u8 Fire_ready;          // 0x0C  setFire: start firing
    u8 Fire_go;             // 0x0D
    u8 Fire_num;            // 0x0E  shots left (setReload: 40)
    u8 Break_mode;          // 0x0F  0: weapon damage 0xD / 0x12 breaks it
    f32 St_dir;             // 0x10  base yaw
    f32 Rot_max;            // 0x14  yaw step limit (pi)
    int Heli_lock_timer;    // 0x18  frames until `pTarget` reverts to the player
    u8 Se_on;               // 0x1C  spin SE playing
    u8 pad_1D[3];
    u32 Seid;               // 0x20
    class cSat* pEat;       // 0x24
    class cEmHit* pHit[3];  // 0x28
    u8 pad_34[8];          // PS2's FREE_OBJ15 has pEm/pTarget here instead (0x34/0x38); GC's
                            // compiled layout leaves these 8 bytes unused and places them at 0x3C/0x40
    class cEm* pEm;         // 0x3C  enemy riding the gun
    class cModel* pTarget;  // 0x40  aimed-at model (player when NULL)
};

// Mounted gatling gun: aims at `pTarget` (the player unless an enemy rides it), fires every third
// frame once spun up, takes weapon damage on three cEmHit boxes and breaks (R1_Break).
class cObjGatling : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  GatlingWork

    virtual void move();
    virtual ~cObjGatling() {}
    void setRide(cEm* pEm);
    void setFire();
    void stopFire();
    int ckReload();
    void setReload();
    void setEat(void* data, int type);
    void setMaxRot(f32 rot_max);
    int ckBreak();
    void setBreakMode(u8 mode);
    void setBreak();
};

#define GATLING_WK(o) ((GatlingWork*) (o)->free)

cObjGatling* SetObjGatling(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
