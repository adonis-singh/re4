#ifndef OBJPILLAR_H
#define OBJPILLAR_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Falling pillar work (game/objPillar.cpp `cObjPillar`).
struct PillarWork {
    u32 Be_flg;            // 0x00  bit0: set (ckSet), cleared by setBreak / setThrow / setFall
    int Timer;            // 0x04  frames before the fade out
    int TmpU32;              // 0x08  Rnd() & 1: action button type 3 / 4
    void* Mot;       // 0x0C  setMotion
    void* Mot_catch;      // 0x10  setThrow: lift, throw
    void* Mot_throw;      // 0x14
    void* Mot_escape;      // 0x18  R0_Escape
    void* Mot_fall;       // 0x1C  setFall: fall, land
    void* Mot_landing;       // 0x20
    void* Mot_pl_escape;          // 0x24  player escape motion (plemEscape MotionSetCore)
    void* Seq_pl_escape;         // 0x28  its 4th argument (PS2 u32 Seq_pl_escape)
    Vec St_pos;          // 0x2C  position at R0_Set (attack line end, plemEscape2 heading)
    Vec Break_pos;           // 0x38  setBreak position (plemEscape heading)
    Vec Spd;              // 0x44  throw / fall speed
    u32 Seid;         // 0x50  SndCall handle of the rolling SE
    class cSat* pEat;      // 0x54  effect collision piece (objPillarEatSet)
    u8 Act_ck;           // 0x58  1: the player escaped / was hit (no more action button)
};

// Falling pillar (obj 0x1F): breaks (setBreak) or is thrown (setThrow) at the player, who can
// escape with the action button; the escape / die sequences run as player damage routines.
class cObjPillar : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  PillarWork

    virtual void move();
    virtual ~cObjPillar() {}
    void setMotion(void* mot);
    int ckSet();
    void setBreak(Vec* pos, void* mot, void* pl_seq);
    void setThrow(void* mot0, void* mot1, void* motEscape, void* plMot, void* pl_seq);
    void setFall(void* mot0, void* mot1);
};

#define PILLAR_WK(o) ((PillarWork*) (o)->free)

#endif
