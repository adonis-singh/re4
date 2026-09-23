#ifndef OBJ13_H
#define OBJ13_H

#include "types.h"
#include "obj.h"

// Room-script view of the ladder object (game/obj13.cpp defines the full class with its virtuals;
// this declares only the out-of-line members the rooms call on a getRoomEtcLadder() result, so no
// vtable is emitted here).
class cObjLadder : public cObjUnion {
public:
    int getStatus();
    void setStand();
    void setCamera(int cam_no);
    void setOff();
    void setOn();
    void setMotion(void** pMot);   // r400 setLadderMotion: the 20-entry motion table
    void setDowned();             // r402 R402ExecEvent01Main: the ladder falls into place
};

// game/obj13.cpp: shows / hides the ladders of the running event (r101 Evt_R101S30_Func).
extern "C" void LadderEventTrans(int on);

class cEm;
extern "C" {
// Creates ladder `no` from the etc model files (EtcModel.cpp).
cObj* SetLadder(void* bin, void* tpl, Vec* pos, Vec* rot, int no);
// 1 when a ladder is within reach of `pos` (emwindow.cpp).
int LadderNearCk(Vec* pPos);
// Partner ladder climb checks (pl_npc.cpp).
int SubLadderClimbCk(cEm* pEm);
int SubLadderClimbCk2(cEm* pEm);
}

#endif
