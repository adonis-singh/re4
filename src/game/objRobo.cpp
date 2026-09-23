// game/objRobo: object id 0x37, the giant Salazar statue of room r4-2 (D:/Bio4/Prog/objRobo.cpp).
// R0 routines: waits on the gondola (its hands are switches the player shoots, TaskSwitchFront/
// Back), walks the passage, smashes the door, then chases the player over the bridge whose plates
// give way behind it (R0WalkBridge, Room_flg bits 0x12..0x1D); its feet carry whoever stands on
// them (SatMove) and crush the player when they come down near him (WalkHitCk).
#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "flag_rsf.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "scroll.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "player.h"
#include "motion.h"
#include "objRobo.h"
#include <string.h>




// Reference read of a .sdata float: an unflagged MEM that stays below the preceding `w->fallX` store.


f32 RoboFallSpdX = -50.0f;
f32 RoboFallSpdY = -100.0f;
f32 lbl_80313FD8 = -10.0f;   // unreferenced fall speed (Bio4.sym has no name for it)
f32 BridgeStartX = -63000.0f;
static f32 RoboHitRadius = 6000.0f;
static f32 posysub = 500.0f;

// Hit box table: model parts and the YarareInit cylinder (x, y, z offset, radius, height).
struct RoboHitTbl {
    int parts;   // RoboPartsNoEnum
    f32 x;
    f32 y;
    f32 z;
    f32 w;
    f32 h;
};

// Creates the statue (id 0x37) at pos/rot with a 0x98-byte extra work, tall light volume; R0 0.
cObjRobo* SetObjRobo(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    RoboWork* w;

    obj = ObjMgr.create(cObjMgr::ID_ROBO);
    if (obj == 0) {
        return 0;
    }
    w = &((cObjRobo*) obj)->robo;
    memset(w, 0, sizeof(RoboWork));
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
#line 94 "D:/Bio4/Prog/objRobo.cpp"
    obj->Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 0xD);
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 10000.0f, 5000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    obj->sub2B4.clrFlags(0xFCFF);
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    obj->pos_old = obj->pos;
    if (rot) {
        obj->ang = *rot;
    } else {
        obj->ang.x = 0.0f;
        obj->ang.y = 0.0f;
        obj->ang.z = 0.0f;
    }
    RotMatrix(obj->l_mat, &obj->ang);
    TransMatrix(obj->l_mat, &obj->pos);
    ScaleMatrix(obj->l_mat, &obj->scale);
    PSMTXCopy(obj->l_mat, obj->mat);
    obj->partsMatCalc();
    obj->partsWorldCalc();
    w->r_no_0 = 0;
    w->r_no_1 = 0;
    return (cObjRobo*) obj;
}

// Dispatches RoboWork::r_no_0 through R0Tbl (0 Init, 1 WaitGondola, 2 WalkPassage, 3 WaitDoor,
// 4 WalkBridge, 5 WaitBreak, 6 WaitDie, 7 Event).
void cObjRobo::move()
{
    static void (*R0Tbl[])(cObjRobo*) = {
        R0Init,       R0WaitGondola, R0WalkPassage, R0WaitDoor,
        R0WalkBridge, R0WaitBreak,   R0WaitDie,     R0Event,
    };

    R0Tbl[robo.r_no_0](this);
}

// Event start: the statue keeps moving during the event in the Event routine.
void cObjRobo::SetBeginEvent(u32 a)
{
    RoboWork* w = &robo;

    setNoSuspend(1);
    w->r_no_0 = 7;
    w->r_no_1 = 0;
}

// Event end: normal suspend behaviour again (the room sets the next routine).
void cObjRobo::SetEndEvent(u32 a)
{
    setNoSuspend(0);
}

// R0 0: idle motion; creates the two foot SAT/EAT collisions and the body EAT (room archive
// entries 5 / 0x12), two dummy scroll objects carrying the hand scenario areas 0x26/0x27, the hand
// switch scenario callbacks (areas 3/4 -> TaskSwitchBack/Front), the 14 cEmHit damage boxes
// (RoboHitTbl), and picks R0 1 (gondola wait) or 7 by room flag 9.
void cObjRobo::R0Init(cObjRobo* pObj)
{
    RoboWork* w = &pObj->robo;
    cObj* smd;
    cEmHit* hit;

    MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x42), 0, 0, 4, 0);
    Vec pos = { 0.0f, 0.0f, 0.0f };
    Vec rot = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 2; i++) {
        w->pSat[i] = 0;
        w->pEat[i] = 0;
    }
    w->pSat[0] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
    w->pSat[1] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 3);
    w->pEat[0] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 6);
    w->pEat[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 7);
    w->pEatBody = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pObj->pos, &rot, 2);
    for (int i = 0; i < 2; i++) {
        Vec pos2 = { 0.0f, 0.0f, 0.0f };
        Vec rot2 = { 0.0f, 0.0f, 0.0f };

        smd = SetObjSmd((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                        &pos2, &rot2, 0x10, 1);
        w->smd[i] = smd;
        if (smd) {
            if (i == 0) {
                SceAtSetParent(SCEAT_ITEMPARENT_L, smd, 0);
            } else {
                SceAtSetParent(SCEAT_ITEMPARENT_R, smd, 0);
            }
            smd->be_flag &= ~2;
        }
    }
    SceAtDataSet_exec(SCEAT_EXEC_BACK, SCE_LEVEL10, 0, (TaskFunc) TaskSwitchBack, pObj, 1);
    SceAtDataSet_exec(SCEAT_EXEC_FRONT, SCE_LEVEL10, 0, (TaskFunc) TaskSwitchFront, pObj, 1);
    if (RsfCheck(pG->room_id, 9)) {
        w->r_no_1 = 0;
        w->r_no_0 = 1;
    } else {
        w->r_no_1 = 0;
        w->r_no_0 = 7;
    }
    {
    int i;
    RoboHitTbl tbl[HitNoMax] = {
        { RoboPartsNoBody, 0.0f, 0.0f, 0.0f, 2900.0f, 3500.0f },
        { RoboPartsNoRShoulder, 0.0f, 0.0f, 0.0f, 1300.0f, 0.0f },
        { RoboPartsNoRUArm, 0.0f, 1000.0f, 0.0f, 1200.0f, -2500.0f },
        { RoboPartsNoRDArm, 0.0f, 0.0f, 0.0f, 1200.0f, -3000.0f },
        { RoboPartsNoLShoulder, 0.0f, 0.0f, 0.0f, 1300.0f, 0.0f },
        { RoboPartsNoLUArm, 0.0f, 1000.0f, 0.0f, 1200.0f, -2500.0f },
        { RoboPartsNoLDArm, 0.0f, 0.0f, 0.0f, 1200.0f, -3000.0f },
        { RoboPartsNoRULeg, 0.0f, 0.0f, 0.0f, 1800.0f, 0.0f },
        { RoboPartsNoRDLeg, 0.0f, 0.0f, -500.0f, 1300.0f, 3500.0f },
        { RoboPartsNoLULeg, 0.0f, 0.0f, 0.0f, 1800.0f, 0.0f },
        { RoboPartsNoLDLeg, 0.0f, 0.0f, -500.0f, 1300.0f, 3500.0f },
        { RoboPartsNoMouth, 0.0f, 2000.0f, -1500.0f, 2000.0f, 0.0f },
        { RoboPartsNoSwitchBL, 0.0f, 100.0f, -300.0f, 1000.0f, 0.0f },
        { RoboPartsNoSwitchF, 0.0f, 0.0f, 200.0f, 1000.0f, 0.0f },
    };

    for (i = 0; i < HitNoMax; i++) {
        w->pEmHitTbl[i] = 0;
        hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), 0, 0, 1);
        if (hit) {
            hit->setParent(pObj, tbl[i].parts, 0);
            if (tbl[i].parts == 0x15 || tbl[i].parts == 0x16) {
                YarareInit(hit, tbl[i].x, tbl[i].y, tbl[i].z, tbl[i].w, tbl[i].h, 0, YAT_FLAG_ON);
            } else {
                YarareInit(hit, tbl[i].x, tbl[i].y, tbl[i].z, tbl[i].w, tbl[i].h, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
            }
            w->pEmHitTbl[i] = hit;
        }
    }
    }
}

// R0 1: the statue idles on the gondola (motion 0x62 with its sequence; foot stomp sounds on the
// motion events), moves the player and the Ganados standing on its feet with them (SatMove), the
// shot hand boxes (hit 12/13) start the hand switch tasks, and every hit box shows sparks when shot.
void cObjRobo::R0WaitGondola(cObjRobo* pObj)
{
    RoboWork* w = &pObj->robo;
    cPlayer* pl = pPL;
    Vec ft[2];
    Vec p;
    Vec d;
    int i;
    cModel* parts;
    cEmHit* hit;

    switch (w->r_no_1) {
    case 0:
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x62), ROOM_ARC_PTR(pG->pRoom, 0x67), 0, 4, 0);
        w->r_no_1++;
    case 1:
        if (pObj->Motion.Seq_old.Free & 1) {
            SndCall(6, 2, &pObj->getPartsPtr(RoboPartsNoLHand)->world, 0, 0, 0);
        }
        if (pObj->Motion.Seq_old.Free & 2) {
            SndCall(6, 1, &pObj->getPartsPtr(RoboPartsNoRHand)->world, 0, 0, 0);
        }
        if (pObj->Motion.Seq_old.Free & 4) {
            SndCall(6, 4, &pObj->getPartsPtr(RoboPartsNoLHand)->world, 0, 0, 0);
        }
        if (pObj->Motion.Seq_old.Free & 8) {
            SndCall(6, 3, &pObj->getPartsPtr(RoboPartsNoRHand)->world, 0, 0, 0);
        }
        for (i = 0; i < 2; i++) {
            parts = pObj->getPartsPtr(i == 0 ? 10 : 5);
            ft[i].x = 0.0f;
            ft[i].y = 100.0f;
            ft[i].z = 0.0f;
            PSMTXMultVec(parts->mat, &ft[i], &ft[i]);
        }
        MotionMove(pObj, 0);
        pObj->partsWorldCalc();
        if (pl->stat & 0x80) {
            pl->setPos(&pl->m_VecWork1);
        }
        for (i = 0; i < 2; i++) {
            pObj->SatMove(pObj, &ft[i], i);
        }
        if (w->pEmHitTbl[HitNoSwitchF] && w->pEmHitTbl[HitNoSwitchF]->ckStatus() == 1 && !RmfFlagChk(pG, RMF_BOBO_SWITCH_EXEC_FRONT)) {
            SceExec(0x12, (TaskFunc) TaskSwitchFront, (int) pObj, 0, SCE_PRIO_DEF_2, 0);
        }
        if (w->pEmHitTbl[HitNoSwitchBR] && w->pEmHitTbl[HitNoSwitchBR]->ckStatus() == 1 && !RmfFlagChk(pG, RMF_BOBO_SWITCH_EXEC_BACK)) {
            SceExec(0x12, (TaskFunc) TaskSwitchBack, (int) pObj, 0, SCE_PRIO_DEF_2, 0);
        }
        for (i = 0; i < HitNoMax; i++) {
            hit = w->pEmHitTbl[i];
            if (hit && hit->ckStatus() == 1) {
                EmDmBloodSet2(hit, 1, 0xF, 0, 0, 2);
                EmGetDmPos(hit, &p, &d);
            }
        }
        break;
    }
}

// R0 2: walks the passage (walk motion 0x25) with the dust effect and step hit checks until x <=
// -60000, where it is clamped.
void cObjRobo::R0WalkPassage(cObjRobo* pObj)
{
    RoboWork* w = &pObj->robo;
    Vec v;

    switch (w->r_no_1) {
    case 0:
        EstSet(pObj, -1, 0, 0, EFF_ROOM, 7, 1, ESP_CORE_KIND_ROOM01, 0, 0);
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), 0x3C, 5, 0);
        w->r_no_1++;
    case 1:
        pObj->WalkSequence(pObj, 1);
        break;
    }
    if (pObj->pos.x <= -60000.0f) {
        v.x = -60000.0f;
        v.y = pObj->pos.y;
        v.z = pObj->pos.z;
        pObj->setPos(&v);
    }
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
}

// R0 3: walks up to the door (x -55598), then the door-smash motion 0x5C with its effect and sounds
// (Room_flg[0] 0x10000 = door broken).
void cObjRobo::R0WaitDoor(cObjRobo* pObj)
{
    RoboWork* w = &pObj->robo;
    Vec v;

    switch (w->r_no_1) {
    case 0:
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), 0x3C, 5, 0);
        w->r_no_1++;
    case 1:
        pObj->WalkSequence(pObj, 0);
        if (pObj->pos.x <= -55597.8984375f) {
            int t = 0;

            EffectEspDelete(1, ESP_CORE_KIND_ROOM01, 0, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_ROOM01, 0);
            EffectEfmDelete(1, ESP_CORE_KIND_ROOM01, 0);
            RmfFlagOn(pG, RMF_BOBO_DOOR_PUNCH);
            MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x5C), ROOM_ARC_PTR(pG->pRoom, 0x65), 0x3C, 4, 0);
            v.x = -55597.8984375f;
            v.y = pObj->pos.y;
            v.z = pObj->pos.z;
            pObj->setPos(&v);
            w->SndTimer = t;
            w->r_no_1++;
        }
        break;
    case 2:
        if (pObj->Motion.Seq_old.Free & 1) {
            EstSet(pObj, -1, 0, 0, EFF_ROOM, 0xC, 1, ESP_CORE_KIND_ROOM02, 0, 0);
            w->SndTimer = 0;
        }
        w->SndTimer++;
        if (w->SndTimer == 10) {
            SndCall(6, 0x10, &pObj->pos, 0, 0, 0);
        }
        if (w->SndTimer == 30) {
            SndCall(6, 7, &pObj->pos, 0, 0, 0);
        }
        break;
    }
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
}

// R0 4: the bridge chase from BridgeStartX: walks with step hit checks until it reaches the first
// bridge plate, then falls through (motion 0x63, sound at frames 10 and 90) while the fall point
// advances 50/frame over the six plates: each plate's flag (0x12..0x17) is set, its effect swapped
// (est 3..8 -> 0x17..0x1C) and 15 frames later the plate-gone flag (0x18..0x1D).
void cObjRobo::R0WalkBridge(cObjRobo* pObj)
{
    RoboWork* w = &pObj->robo;
    u32 smdNo[6] = { 0x43, 0x44, 0x4F, 0x50, 0x51, 0x52 };
    u32 flagNo[6] = { 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 };
    u32 flagNo2[6] = { 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D };
    u32 estNo[6] = { 3, 4, 5, 6, 7, 8 };
    u32 estNo2[6] = { 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C };
    Vec v;
    cObj* smd;
    int i;
    u32 f;
    int* hp;

    switch (w->r_no_1) {
    case 0:
        v.x = BridgeStartX;
        v.y = pObj->pos.y;
        {
            // `&v` as a pointer local after the x/y stores: the z store goes through the pointer
            // (`stfs f0,8(r11)`) and setPos gets `mr r4,r11` instead of a fresh `addi r4,r1,136`.
            Vec* pv = &v;
            pv->z = -16560.0f;
            pObj->setPos(pv);
        }
        EstSet(pObj, -1, 0, 0, EFF_ROOM, 7, 1, ESP_CORE_KIND_ROOM01, 0, 0);
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), 0, 5, 0);
        for (i = 0; i < 6; i++) {
            w->BridgeTimer[i] = 0;
        }
        w->r_no_1++;
    case 1:
        pObj->WalkSequence(pObj, 1);
        smd = SmdGetObjPtr(smdNo[0]);
        if (smd == 0) {
            break;
        }
        if (pObj->pos.x < smd->pos.x) {
            w->FallTimer = 0;
            w->r_no_1++;
        }
        break;
    case 2:
        w->FallTimer++;
        if (w->FallTimer <= 9) {
            pObj->WalkSequence(pObj, 1);
        }
        if (w->FallTimer == 10) {
            SndCall(6, 0xA, &pObj->pos, 0, 0, 0);
            EffectEspDelete(1, ESP_CORE_KIND_ROOM01, 0, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_ROOM01, 0);
            EffectEfmDelete(1, ESP_CORE_KIND_ROOM01, 0);
            MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x63), 0, 0xA, 1, 0);
            EstSet(pObj, -1, 0, 0, EFF_ROOM, 0x20, 1, ESP_CORE_KIND_NONE, 0, 0);
            w->BridgeFallPos = pObj->pos.x;
            w->FallSpdY = RoboFallSpdY;
        }
        if (w->FallTimer == 90) {
            SndCall(6, 9, &pObj->pos, 0, 0, 0);
        }
        // `hp` is a plain pointer (`*hp` aliases the scalar pG, so the second flag test reloads it)
        // incremented before `i` (its `addi` leads the latch); the first flag test reads the word into
        // the user variable `f`, so cse1 cannot thread its taken branch past the second test and the
        // 0x80000000 constants stay per block (a threaded label would make them single-use movables
        // that loop.c combines and hoists).
        for (i = 0, hp = w->BridgeTimer; i < 6; hp++, i++) {
            smd = SmdGetObjPtr(smdNo[i]);
            if (smd) {
                w->BridgeFallPos += RoboFallSpdX;
                if (w->BridgeFallPos < smd->pos.x) {
                    f = eventFlags()[flagNo[i] >> 5];
                    if (!(f & (0x80000000 >> (flagNo[i] & 31)))) {
                        eventFlags()[flagNo[i] >> 5] |= 0x80000000 >> (flagNo[i] & 31);
                        EffectEspDelete(0x2001, (u8) estNo[i], 0, 0);
                        EffectEspgenDelete(0x2001, (u8) estNo[i], 0);
                        EffectEfmDelete(0x2001, (u8) estNo[i], 0);
                        EstSet(0, -1, 0, 0, EFF_ROOM, (u8) estNo2[i], 1, ESP_CORE_KIND_NONE, 0, 0);
                    }
                }
            }
            if (eventFlags()[flagNo[i] >> 5] & (0x80000000 >> (flagNo[i] & 31))) {
                (*hp)++;
                if (*hp > 14) {
                    eventFlags()[flagNo2[i] >> 5] |= 0x80000000 >> (flagNo2[i] & 31);
                }
            }
        }
        break;
    }
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
}

// R0 5: broken: plays the idle motion 0x3C.
void cObjRobo::R0WaitBreak(cObjRobo* pObj)
{
    RoboWork* w = &pObj->robo;

    if (w->r_no_1 == 0) {
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x3C), 0, 0, 4, 0);
        w->r_no_1++;
    }
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
}

// R0 6: dying: plays the idle motion 0x3C.
void cObjRobo::R0WaitDie(cObjRobo* pObj)
{
    RoboWork* w = &pObj->robo;

    if (w->r_no_1 == 0) {
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x3C), 0, 0, 4, 0);
        w->r_no_1++;
    }
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
}

// R0 7: during an event: only advances the motion set by the event.
void cObjRobo::R0Event(cObjRobo* pObj)
{
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
}

// One frame of walking: applies the motion's root movement, step sounds on motion events 4/8, and
// on the foot-down events 1/2 the dust effect plus (hitCk) the crush check on the player.
// One walk frame: keep the statue on its line, play the step SEs / effects of the motion events
// and (hitCk) check whether a foot caught the player.
void cObjRobo::WalkSequence(cObjRobo* pObj, int hitCheckFlag)
{
    Vec v;
    cModel* parts;

    v.x = pObj->pos.x;
    v.y = pObj->pos.y;
    v.z = -16560.0f;
    pObj->setPos(&v);
    v.x = 0.0f;
    v.y = -PI / 2;
    v.z = 0.0f;
    pObj->setAng(&v);
    if (pObj->Motion.Seq_old.Free & 4) {
        parts = pObj->getPartsPtr(RoboPartsNoRFoot);
        if (parts) {
            SndCall(6, 7, &parts->world, 0, 0, 0);
        }
    }
    if (pObj->Motion.Seq_old.Free & 8) {
        parts = pObj->getPartsPtr(RoboPartsNoLFoot);
        if (parts) {
            SndCall(6, 7, &parts->world, 0, 0, 0);
        }
    }
    if (pObj->Motion.Seq_old.Free & 1) {
        if (hitCheckFlag == 1) {
            pObj->WalkHitCk(pObj);
        }
        EstSet(pObj, -1, 0, 0, EFF_ROOM, 8, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        parts = pObj->getPartsPtr(RoboPartsNoRFoot);
        if (parts) {
            SndCall(6, 8, &parts->world, 0, 0, 0);
        }
    }
    if (pObj->Motion.Seq_old.Free & 2) {
        if (hitCheckFlag == 1) {
            pObj->WalkHitCk(pObj);
        }
        EstSet(pObj, -1, 0, 0, EFF_ROOM, 9, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        parts = pObj->getPartsPtr(RoboPartsNoLFoot);
        if (parts) {
            SndCall(6, 8, &parts->world, 0, 0, 0);
        }
    }
}

// Scenario task (front hand switch shot / area 4): rotates the right hand parts (0x16) closed over
// 15 frames, plays the hand motion 0x61, then opens it and returns to idle; Room_flg[0]
// 0x80000000 while running, 0x8000 = hand closed.
// Scenario task: the front arm swings down (or back up) over 15 frames.
// Loop shapes (both tasks): the down arm sets `range = to` in the for-init (a preheader copy, LUID
// between `j = 0` and gcse's `&robo->Motion` insertion: `fmr` before `lfd`/`addi`), the up arm
// computes `range2 = from - to` inside the loop (a loop.c movable after the insertion); no `base`
// copy (the offsets are `from`/`to` directly), so max (2 sets, x4 length) outranks the two ranges.
void cObjRobo::TaskSwitchFront(cObjRobo* pObj)
{
    cModel* parts;
    int i;
    int j;
    register f32 to asm("fr28");  // COMPILER-DIFF: #17 (FPR value pin): a hard-register `to` keeps the for-init copy `range = to` out of gcse's copy propagation
    to = -1.483529806137085f;
    f32 from = 0.0f;
    f32 max;
    f32 range;
    f32 range2;

    i = 15;
    parts = pObj->getPartsPtr(RoboPartsNoSwitchF);
    if (RmfFlagChk(pG, RMF_BOBO_SWITCH_EXEC_FRONT)) {
        return;
    }
    RmfFlagOn(pG, RMF_BOBO_SWITCH_EXEC_FRONT);
    SceAtSetEnable(SCEAT_EXEC_FRONT, 0);
    if (parts) {
        SndCall(6, 5, &parts->pos, 0, 0, 0);
    }
    if (!RmfFlagChk(pG, RMF_BOBO_SWITCH_FRONT)) {
        RmfFlagOn(pG, RMF_BOBO_SWITCH_FRONT);
        for (j = 0, range = to; j < i; j++) {
            max = (f32) i;
            parts->ang.y = range * (f32) j / max + from;
            SceSleep(1);
        }
        parts->ang.y = to;
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x61), ROOM_ARC_PTR(pG->pRoom, 0x66), 0xF0, 4, 0);
    } else {
        RmfFlagOff(pG, RMF_BOBO_SWITCH_FRONT);
        for (j = 0; j < i; j++) {
            max = (f32) i;
            range2 = from - to;
            parts->ang.y = range2 * (f32) j / max + to;
            SceSleep(1);
        }
        parts->ang.y = from;
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x62), ROOM_ARC_PTR(pG->pRoom, 0x67), 0xF0, 4, 0);
    }
    RmfFlagOff(pG, RMF_BOBO_SWITCH_BACK);
    pObj->getPartsPtr(RoboPartsNoSwitchBL)->ang.x = 0.0f;
    SceSleep(1);
    RmfFlagOff(pG, RMF_BOBO_SWITCH_EXEC_FRONT);
    SceAtSetEnable(SCEAT_EXEC_FRONT, 1);
}

// Scenario task (back hand switch / area 3): the same for the left hand parts (0x15) with motion
// 0x22; flags 0x40000000 / 0x4000.
// Scenario task: the back arm.
void cObjRobo::TaskSwitchBack(cObjRobo* pObj)
{
    cModel* parts;
    int i;
    int j;
    register f32 to asm("fr28");  // COMPILER-DIFF: #17 (FPR value pin): a hard-register `to` keeps the for-init copy `range = to` out of gcse's copy propagation
    to = -1.483529806137085f;
    f32 from = 0.0f;
    f32 max;
    f32 range;
    f32 range2;

    i = 15;
    parts = pObj->getPartsPtr(RoboPartsNoSwitchBL);
    if (RmfFlagChk(pG, RMF_BOBO_SWITCH_EXEC_BACK)) {
        return;
    }
    RmfFlagOn(pG, RMF_BOBO_SWITCH_EXEC_BACK);
    SceAtSetEnable(SCEAT_EXEC_BACK, 0);
    if (parts) {
        SndCall(6, 5, &parts->pos, 0, 0, 0);
    }
    if (!RmfFlagChk(pG, RMF_BOBO_SWITCH_BACK)) {
        RmfFlagOn(pG, RMF_BOBO_SWITCH_BACK);
        for (j = 0, range = to; j < i; j++) {
            max = (f32) i;
            parts->ang.x = range * (f32) j / max + from;
            SceSleep(1);
        }
        parts->ang.x = to;
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x45), 0xF0, 4, 0);
    } else {
        RmfFlagOff(pG, RMF_BOBO_SWITCH_BACK);
        for (j = 0; j < i; j++) {
            max = (f32) i;
            range2 = from - to;
            parts->ang.x = range2 * (f32) j / max + to;
            SceSleep(1);
        }
        parts->ang.x = from;
        MotionSetCore(pObj, &pObj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x62), ROOM_ARC_PTR(pG->pRoom, 0x67), 0xF0, 4, 0);
    }
    RmfFlagOff(pG, RMF_BOBO_SWITCH_FRONT);
    pObj->getPartsPtr(RoboPartsNoSwitchF)->ang.y = 0.0f;
    SceSleep(1);
    RmfFlagOff(pG, RMF_BOBO_SWITCH_EXEC_BACK);
    SceAtSetEnable(SCEAT_EXEC_BACK, 1);
}

// (Unused) v * 100 + 10 degrees to radians.
// Dead-stripped in the original: only their constant pool / string survive in .rodata.
static f32 roboDegToRad(f32 v)
{
    f32 x = v * 100.0f;

    x += 10.0f;
    return x * (PI / 180.0f);
}

// (Unused) error message for a missing scenario area.
static void roboSceAtCk()
{
    pLog->err(0, 0, "move : SceAt no create");
}

// A foot came down: when the living player is within RoboHitRadius (6000) of the statue, raises
// Room_flg[1] 0x80000000 (the room kills him) and returns 1.
// The player is under a foot: flag the death.
int cObjRobo::WalkHitCk(cObjRobo* pObj)
{
    int dead;

    if ((s16) pG->pl_life > 0) {
        dead = 1;
        if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
            dead = 0;
        }
        if (dead == 0) {
            if (sqrtf((pObj->pos.x - pPL->pos.x) * (pObj->pos.x - pPL->pos.x) +
                      (pObj->pos.z - pPL->pos.z) * (pObj->pos.z - pPL->pos.z)) > RoboHitRadius) {
                return 0;
            }
            RmfFlagOn(pG, RMF_PLAYER_DIE_PASSAGE_SET);
            return 1;
        }
    }
    return 0;
}

// (Unused) helper: a * 1000, -3000 when zero.
static f32 roboDead1(f32 a)
{
    f32 r = a * 1000.0f;

    if (r == 0.0f) {
        r = -3000.0f;
    }
    return r;
}

// (Unused) helper: a - 1000 when positive, else a * 1000.
static f32 roboDead2(f32 a)
{
    if (a > 0.0f) {
        return a + -1000.0f;
    }
    return a * 1000.0f;
}

// Moves the foot collision `side` (0 right / 1 left) to the foot position (2000 in -x) and carries
// the player (unless stat 0x100; the camera quake offset follows) and the Ganados standing
// on it by the foot's displacement; also moves the hand-area dummy object.
// Move the collision pieces of one side to the foot at `pos` and push the player / enemies
// standing on it along.
void cObjRobo::SatMove(cObjRobo* pObj, Vec* pPosOld, int armNo)
{
    RoboWork* w = &pObj->robo;
    cPlayer* pl = pPL;
    Vec a = { 0.0f, 0.0f, 0.0f };
    Vec b = { 0.0f, 0.0f, 0.0f };
    Vec c;
    Vec d;
    cModel* parts;
    int partsNo;
    u32 i;
    cEm* em;

    parts = pObj->getPartsPtr(armNo == 0 ? 10 : 5);
    a.x = pPosOld->x - 2000.0f;
    a.y = pPosOld->y;
    a.z = pPosOld->z;
    c.x = 0.0f;
    c.y = 100.0f;
    c.z = 0.0f;
    PSMTXMultVec(parts->mat, &c, &c);
    PSVECSubtract(&c, pPosOld, &d);
    if (!(pl->stat & 0x100)) {
        if (SatMoveSub(pl, &a, &d) == 1) {
            pG->quake_ofs = d;
        }
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        em = EmMgr.fastAt(i);
        if ((em->be_flag & 0x201) == 1 && em->id > 0xF && em->id <= 0x20) {
            SatMoveSub(em, &a, &d);
        }
    }
    if (w->pSat[armNo]) {
        w->pSat[armNo]->setCoord(&a, &b);
    }
    if (w->pEat[armNo]) {
        w->pEat[armNo]->setCoord(&a, &b);
    }
    if (w->smd[armNo]) {
        w->smd[armNo]->setPos(&a);
    }
}

// `em` stands within 1000 of the foot at `pos`: move it by `d`. Returns 1 when it did.
int cObjRobo::SatMoveSub(cModel* pMod, Vec* pPosCenter, Vec* pVecMov)
{
    Vec t;

    if (pPosCenter->x - 1000.0f <= pMod->pos.x && pPosCenter->x + 1000.0f >= pMod->pos.x && pPosCenter->z - 1000.0f <= pMod->pos.z &&
        pPosCenter->z + 1000.0f >= pMod->pos.z && __builtin_fabsf(pPosCenter->y - pMod->pos.y) <= posysub) {
        PSVECAdd(&pMod->pos, pVecMov, &t);
        pMod->setPos(&t);
        return 1;
    }
    return 0;
}
