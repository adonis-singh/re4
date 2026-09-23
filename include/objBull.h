#ifndef OBJBULL_H
#define OBJBULL_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Room-script view of the bulldozer (game/objBull.cpp defines the class with its virtuals; the
// rooms only call the out-of-line members, so no vtable is emitted here).
class cObjBull : public cObjUnion {
public:
    void setMotion(void** pMot);
    int ckBullRide(Vec* pPos, u8* pParts_no, Vec* pOffset);
    int ckGoal();
    void setRide();
    int ckBreak1st();
    int ckBreak2nd();
    int ckBreak3rd();
    int ckBreak4th();
    int ckLift();
    int ckTruckGo();
    int ckLiftWait();
    void setSubBullDrive();
    void setSubBullFinger();
    void setSubBullLookBack();
    int getMoveFrameToLift();
    int getMoveFrameRtn();
    void setAdjustMode(u8 mode, void (*func)(cObj*));
    void setBreakTruck();
};

cObj* SetBull(void* bin, void* tpl, Vec* pos, Vec* rot, u32 type);

#endif
