#ifndef OBJ15_H
#define OBJ15_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// game/obj15.cpp: creates the gatling object from the room archive model (r209 GatlingAppear).

// Gatling gun work (game/obj15.cpp `cObjGatling`): a mounted gun the player (or `ride`) fires
// at `target`; three cEmHit hit boxes take the damage, `eat` is its effect collision piece.
struct GatlingWork {
    u32 x00;              // 0x00
    int breakTimer;       // 0x04  frames of barrel spin-down after the break
    u8 pad_8[2];
    s16 cnt;              // 0x0A  frames since firing started (shots every 3rd frame after 30)
    u8 fire;              // 0x0C  setFire: start firing
    u8 firing;            // 0x0D
    u8 ammo;              // 0x0E  shots left (setReload: 40)
    u8 breakMode;         // 0x0F  0: weapon damage 0xD / 0x12 breaks it
    f32 rotY;             // 0x10  base yaw
    f32 maxRot;           // 0x14  yaw step limit (pi)
    int targetTimer;      // 0x18  frames until `target` reverts to the player
    u8 seOn;              // 0x1C  spin SE playing
    u8 pad_1D[3];
    u32 seHandle;         // 0x20
    class cSat* eat;      // 0x24
    class cEmHit* hit[3]; // 0x28
    u8 pad_34[8];
    class cEm* ride;      // 0x3C  enemy riding the gun
    class cModel* target; // 0x40  aimed-at model (player when NULL)
};

// Mounted gatling gun: aims at `target` (the player unless an enemy rides it), fires every third
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
