#ifndef OBJGONDOLA_H
#define OBJGONDOLA_H

#include "types.h"
#include "vec.h"
#include "obj.h"
#include "em.h"
#include "motion.h"

// Room-script view of the cable car (game/objGondola.cpp defines the class with its virtuals; the
// rooms only call the out-of-line members, so no vtable is emitted here). em10.h carries the em10
// library's own partial view of the same class: include one or the other.
class cObjGondola : public cObjUnion {
public:
    void setMoveMotion(void* mot, int frame);
    int ckRide();
    void setRideEm(cEm* em);
    void setGetOffEm(cEm* em);
    void setDamage();
    void setBreak();
    void setRidePL();
    void setGetOffPL();
    void setSubMotion(MotionWork* work, void* mot, void* breakMot);
    void setVib();
};

cObj* SetGondola(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
