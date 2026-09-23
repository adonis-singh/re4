#ifndef OBJGONDOLA_H
#define OBJGONDOLA_H

#include "types.h"
#include "vec.h"
#include "obj.h"
#include "em.h"
#include "motion.h"

cObj* SetGondola(void* bin, void* tpl, Vec* pos, Vec* rot);

// Gondola work (game/objGondola.cpp `cObjGondola`): a cable car the player / partner / up to
// five enemies ride; five scenario collision quads follow it.
struct GondolaWork {
    u32 Be_flg;              // 0x00
    int Timer;            // 0x04  break: frames before the sub motion starts
    u8 Ride_pl;            // 0x08  player is on board (ckRide)
    u8 pad_9[3];
    int Ride_sub;          // 0x0C  partner is on board
    int Act_wait;              // 0x10  counts down every frame
    Vec Spd;              // 0x14
    class cEm* pEm[5]; // 0x20
    class cSat* pSat[5];   // 0x34
    class cSat* pEat[5];  // 0x48
    struct MotionWork* subWork;  // 0x5C  sub (vibration / break) motion work (setSubMotion)
    void* Sub_mot1;         // 0x60  vibration motion (setVib)
    void* Sub_mot2;       // 0x64  break motion (R0_Break)
};

// Cable car (gondola): carries the player, the partner and up to five enemies along its motion,
// with five collision quads following the car; the break routine hands the camera over.
class cObjGondola : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  GondolaWork

    virtual void move();
    virtual ~cObjGondola() {}
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

#define GONDOLA_WK(o) ((GondolaWork*) (o)->free)

#endif
