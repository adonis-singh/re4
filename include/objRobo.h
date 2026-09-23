#ifndef OBJROBO_H
#define OBJROBO_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Room_flg bits of room 2-26 (the statue chase), from the PS2 symbols; RmfFlagChk(pG, n).
enum R226_FLAG {
    RMF_BOBO_SWITCH_EXEC_FRONT = 0,
    RMF_BOBO_SWITCH_EXEC_BACK = 1,
    RMF_PILLAR_ESCAPE_ON = 2,
    RMF_PILLAR_ESCAPE_ING = 3,
    RMF_PILLAR_ESCAPE_LAST = 4,
    RMF_PILLAR_SET_6 = 5,
    RMF_PILLAR_SET_7 = 6,
    RMF_PILLAR_SET_8 = 7,
    RMF_PILLAR_SET_9 = 8,
    RMF_PILLAR_SET_10 = 9,
    RMF_PILLAR_SET_11 = 10,
    RMF_PILLAR_SET_12 = 11,
    RMF_PILLAR_SET_13 = 12,
    RMF_BOBO_DOOR_PUNCH = 15,
    RMF_BOBO_SWITCH_FRONT = 16,
    RMF_BOBO_SWITCH_BACK = 17,
    RMF_BRIDGE_ST_00 = 18,
    RMF_BRIDGE_ST_01 = 19,
    RMF_BRIDGE_ST_02 = 20,
    RMF_BRIDGE_ST_03 = 21,
    RMF_BRIDGE_ST_04 = 22,
    RMF_BRIDGE_ST_05 = 23,
    RMF_BRIDGE_DIE_00 = 24,
    RMF_BRIDGE_DIE_01 = 25,
    RMF_BRIDGE_DIE_02 = 26,
    RMF_BRIDGE_DIE_03 = 27,
    RMF_BRIDGE_DIE_04 = 28,
    RMF_BRIDGE_DIE_05 = 29,
    RMF_BRIDGE_ON_AVOID = 30,
    RMF_BRIDGE_ON_SAFE = 31,
    RMF_PLAYER_DIE_PASSAGE_SET = 32,
    RMF_PLAYER_DIE_BRIDGE_SET = 33,
    RMF_PLAYER_DIE_ING = 34,
    RMF_BGM_ON = 35,
    RMF_ROBO_DOOR_BREAK = 36,
    RMF_FLAG_EMRESET00 = 64,
    RMF_BRIDGE_ON_00 = 65,
    RMF_BRIDGE_ON_01 = 66,
    RMF_BRIDGE_ON_02 = 67,
    RMF_BRIDGE_ON_03 = 68,
    RMF_BRIDGE_ON_04 = 69,
    RMF_BRIDGE_ON_05 = 70,
    RMF_EMSET_00 = 71,
    RMF_EMSET_01 = 72,
};

// Area (sce_at) numbers of room 226 (PS2 SCE_AT_NO): SceAtDataSet_exec / SceAtSetEnable `no` in r226 and objRobo.
enum SCE_AT_NO {
    SCEAT_EXEC_DOOR_TO_R225 = 0,
    SCEAT_DOOR_TO_R227 = 1,
    SCEAT_LADDER_L = 2,
    SCEAT_EXEC_BACK = 3,
    SCEAT_EXEC_FRONT = 4,
    SCEAT_EXEC_ROBO_WALK_PASSAGE_START = 5,
    SCEAT_SCRAT_DOOR = 6,
    SCEAT_EXEC_DOOR_OPEN = 7,
    SCEAT_LADDER_R = 8,
    SCEAT_FIELD_INFO_L_3F = 9,
    SCEAT_FIELD_INFO_R_3F = 10,
    SCEAT_FIELD_INFO_R_2F = 11,
    SCEAT_FIELD_INFO_L_2F = 12,
    SCEAT_FIELD_INFO_CL_2F = 13,
    SCEAT_FIELD_INFO_CR_2F = 14,
    SCEAT_EXEC_ROBO_WALK_BRIDGE_START = 15,
    SCEAT_EXEC_ROBO_WALK_BRIDGE_END = 16,
    SCEAT_SCRAT_PILLAR_DOWN = 17,
    SCEAT_EXEC_PASSAGE_SWITCH00 = 18,
    SCEAT_EXEC_PASSAGE_SWITCH01 = 19,
    SCEAT_SCRAT_PASSAGE00 = 20,
    SCEAT_SCRAT_PASSAGE01 = 21,
    SCEAT_SCRAT_BRIDGE_BREAK = 22,
    SCEAT_EXEC_ROBO_START = 23,
    SCEAT_EXEC_ROBO_WATCH = 24,
    SCEAT_FLAG_EMRESTET = 25,
    SCEAT_FLAG_BRIDGE00 = 26,
    SCEAT_FLAG_BRIDGE01 = 27,
    SCEAT_FLAG_BRIDGE02 = 28,
    SCEAT_FLAG_BRIDGE03 = 29,
    SCEAT_FLAG_BRIDGE04 = 30,
    SCEAT_FLAG_BRIDGE05 = 31,
    SCEAT_FLAG_BRIDGE_AVOID = 32,
    SCEAT_FLAG_BRIDGE_SAFE = 33,
    SCEAT_EXEC_TOWER_LOOK = 34,
    SCEAT_SCRAT_FENCE = 35,
    SCEAT_EXEC_CONTINUE_POINT = 36,
    SCEAT_SAVE = 37,
    SCEAT_ITEMPARENT_L = 38,
    SCEAT_ITEMPARENT_R = 39
};

// Model parts of the statue (PS2 RoboPartsNoEnum): getPartsPtr, the hit box table's parts column.
enum RoboPartsNoEnum {
    RoboPartsNoBody = 0,
    RoboPartsNoRShoulder = 1,
    RoboPartsNoRUArm = 2,
    RoboPartsNoRDArm = 3,
    RoboPartsNoRHandSub = 4,
    RoboPartsNoRHand = 5,
    RoboPartsNoLShoulder = 6,
    RoboPartsNoLUArm = 7,
    RoboPartsNoLDArm = 8,
    RoboPartsNoLHandSub = 9,
    RoboPartsNoLHand = 10,
    RoboPartsNoRULeg = 11,
    RoboPartsNoRDLeg = 12,
    RoboPartsNoRFoot = 13,
    RoboPartsNoLULeg = 14,
    RoboPartsNoLDLeg = 15,
    RoboPartsNoLFoot = 16,
    RoboPartsNoREye = 17,
    RoboPartsNoLEye = 18,
    RoboPartsNoMouth = 19,
    RoboPartsNoSwitchBR = 20,
    RoboPartsNoSwitchBL = 21,
    RoboPartsNoSwitchF = 22,
    RoboPartsNoMax = 23
};

// Rows of the hit box table / pEmHitTbl (PS2 HitNoEnum).
enum HitNoEnum {
    HitNoBody = 0,
    HitNoRShoulder = 1,
    HitNoRUArm = 2,
    HitNoRDArm = 3,
    HitNoLShoulder = 4,
    HitNoLUArm = 5,
    HitNoLDArm = 6,
    HitNoRULeg = 7,
    HitNoRDLeg = 8,
    HitNoLULeg = 9,
    HitNoLDLeg = 10,
    HitNoMouth = 11,
    HitNoSwitchBR = 12,
    HitNoSwitchF = 13,
    HitNoMax = 14
};

// Giant robot statue work (game/objRobo.cpp `cObjRobo`): the Salazar statue that walks after the
// player over the bridge; two scenario / effect collision pieces per side, 14 hit boxes.
struct RoboWork {
    s8 r_no_0;           // 0x00  R0Tbl index
    s8 r_no_1;              // 0x01
    u8 pad_2[6];
    int pillar;           // 0x08  r226: index of the bridge pillar being pushed over (playerPillarDownCk)
    class cSat* pSat[2];   // 0x0C  scenario pieces (front / back)
    class cSat* pEat[2];   // 0x14  effect pieces
    class cEmHit* pEmHitTbl[14];  // 0x1C
    class cSat* pEatBody;     // 0x54  effect piece at the model position
    cObj* smd[2];         // 0x58  scroll objects following the feet (SetObjSmd)
    f32 FallSpdY;         // 0x60
    int FallTimer;              // 0x64
    int BridgeTimer[6];        // 0x68  frames each bridge piece has been hit
    f32 BridgeFallPos;            // 0x80
    int SndTimer;            // 0x84
};

// Giant statue (Salazar's robot) of room 4-2: waits on the gondola, walks the passage, waits at
// the door, then chases the player over the bridge, breaking its pieces one by one.
class cObjRobo : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  RoboWork

    virtual void move();
    void SetBeginEvent(u32 a);
    void SetEndEvent(u32 a);
    static void R0Init(cObjRobo* pObj);
    static void R0WaitGondola(cObjRobo* pObj);
    static void R0WalkPassage(cObjRobo* pObj);
    static void R0WaitDoor(cObjRobo* pObj);
    static void R0WalkBridge(cObjRobo* pObj);
    static void R0WaitBreak(cObjRobo* pObj);
    static void R0WaitDie(cObjRobo* pObj);
    static void R0Event(cObjRobo* pObj);
    void WalkSequence(cObjRobo* pObj, int hitCheckFlag);
    static void TaskSwitchFront(cObjRobo* pObj);
    static void TaskSwitchBack(cObjRobo* robo);
    int WalkHitCk(cObjRobo* pObj);
    void SatMove(cObjRobo* pObj, Vec* pPosOld, int armNo);
    int SatMoveSub(cModel* pMod, Vec* pPosCenter, Vec* d);
};

#define ROBO_WK(o) ((RoboWork*) (o)->free)

cObjRobo* SetObjRobo(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
