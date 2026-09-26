#ifndef OBJ13_H
#define OBJ13_H

#include "types.h"
#include "obj.h"

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

// Ladder work (game/obj13.cpp `cObjLadder`): a ladder the player / partner climbs (plobjLadderClimb),
// kicks down (plobjLadderDown) and stands up again (plobjLadderReset).
struct LadderWork {
    u32 be_flag;            // 0x00  bit0 motions set, bit1 off (setOff), bit2 partner climbing, bit3 transOld
    int Status;           // 0x04  0 standing, 1 downed, 2 falling, 3 falling (timer done), 4 fall / reset motion
    class cSat* pSat;      // 0x08
    u32 Ladder_num;        // 0x0C  rungs (setLadderInfo; converted unsigned)
    u8 pad_10[4];
    Vec St_pos;          // 0x14  position at SetLadder (R1_Set restores it)
    f32 St_dir;         // 0x20
    int Climb_wait;       // 0x24  frames the action button stays off after a climb (setClimb: 90)
    int Reset_wait;     // 0x28  setResetReserve: 60
    int Down_wait;        // 0x2C  setDown: 17 frames until status 3
    class cObjObaModel* pHosei;  // 0x30  second ladder object sharing the collision flags
    int Cam_no;           // 0x34  setCamera: camera cut of the climb (-1: the ladder cameras)
    void* mot_tbl[20];        // 0x38  setMotion table (player / partner climb, down, reset motions)
    u8 Etc_no;             // 0x88  etc model number (GetEtcFlgPtr)
};

// Ladder (obj 0x13): the player and the partner climb it (plobjLadderClimb / subobjLadderClimb),
// the player kicks it down (plobjLadderDown) and puts it up again (plobjLadderReset); the ladder
// falls with a damage area (R1_Fall) and breaks the windows it lands on (breakWindow).
class cObjLadder : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  LadderWork

    virtual void move();
    virtual ~cObjLadder() {}
    int getStatus();
    int getType();
    int ckClimb();
    void setClimb();
    int getLadderNum();
    void setLadderInfo(int num, u8 type);
    void setStand();
    void setDowned();
    void setDown(void* mot, void* seq);
    void setDown2();
    int ckReset();
    void setReset(int mode);
    void setTransOld();
    void getTransOld();
    void setOff();
    void setOn();
    void setResetReserve();
    void setMotion(void** tbl);
    void breakWindow();
    void setCamera(int no);
};

#define LADDER_WK(o) ((LadderWork*) (o)->free)

#endif
