#ifndef OBJ15_H
#define OBJ15_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Mounted gatling gun: aims at `target` (the player unless an enemy rides it), fires every third
// frame once spun up, takes weapon damage on three cEmHit boxes and breaks (R1_Break).
class cObjGatling : public cObjUnion {
public:
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

// game/obj15.cpp: creates the gatling object from the room archive model (r209 GatlingAppear).
cObjGatling* SetObjGatling(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
