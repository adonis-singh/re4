#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "objTrolley.h"
#include "em.h"
#include "emwep.h"
#include "global.h"
#include "math_sub.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "snd.h"
#include "esp.h"
#include "rnd.h"
#include "main.h"
#include "player.h"
#include "pl_sub.h"
#include "motion.h"

// The work area of the enemy thrown off the car: the trolley only uses the yaw at 0x9BC.
struct TrolleyEmWork {
    u8 pad_3E0[0x9BC - 0x3E0];
    f32 yaw;              // 0x9BC
};
#define TROLLEY_EM_WK(em) ((TrolleyEmWork*) ((u8*) (em) + 0x3E0))


// Room module enemy (the em1x classes): a cEm with the module's own virtuals; the trolley only
// calls the one the cars' break throws them with (vtable slot 31).
class cEmRoom : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMROOM_WK)
    virtual void v09();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void v14();
    virtual void v15();
    virtual void v16();
    virtual void v17();
    virtual void v18();
    virtual void v19();
    virtual void v20();
    virtual void v21();
    virtual void v22();
    virtual void v23();
    virtual void v24();
    virtual void v25();
    virtual void v26();
    virtual void v27();
    virtual void v28();
    virtual void v29();
    virtual void v30();
    virtual void setTrolleyLost();
};

extern "C" {
void objTrolley_R0_Set(cObjTrolley* obj);
void objTrolley_R0_Move(cObjTrolley* obj);
void objTrolley_R0_Break(cObjTrolley* obj);
void objTrolleySatSet(cObjTrolley* obj);
void objTrolleyEscapeAction(cObjTrolley* obj);
void plobjTrolleyEscape(cPlayer* pl);
void plobjTrolleyDie(cPlayer* pl);
void objTrolleyPushMtx(cObjTrolley* obj);
int objTrolleyGetTrolleyNo(cObjTrolley* obj, Vec* pos);
int objTrolleyGetTrolleyNo2(cObjTrolley* obj, Vec* pos);
void objTrolleyGetAdjust(cObjTrolley* obj);
void objTrolleySetAdjust(cObjTrolley* obj, cEm* em);
void objTrolleyMoveAdjustPL(cObjTrolley* obj);
void objTrolleyMoveAdjustEM(cObjTrolley* obj);
void objTrolleyHitCk(cObjTrolley* obj);
void objTrolleyFallEM(cObjTrolley* obj);
void objTrolleyLostEM(cObjTrolley* obj);
}
static void objTrolleySatClear(cObjTrolley* obj);

void (*ObjTrolley_R0_move_tbl[3])(cObjTrolley*) = {
    objTrolley_R0_Set, objTrolley_R0_Move, objTrolley_R0_Break,
};

Mtx Trolley_MatOld[3];      // car matrices of the previous frame (objTrolleyPushMtx)
Vec Trolley_vec[3];         // car movement of this frame (objTrolleyGetAdjust)
f32 Trolley_dir[3];         // car turn of this frame
u8 Trolley_parts_tbl[3] = { 0, 4, 8 };

// Creates the mine trolley (ObjMgr id 0x3B; room 3-x mine cart ride) at pos / rot, no motions
// yet (setMotion), and builds its scenario / effect collision pieces. Returns 0 on failure.
cObj* SetTrolley(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    TrolleyWork* w;
    int i;
    void** p;

    obj = ObjMgr.create(cObjMgr::ID_TROLLEY);
    if (obj == 0) {
        return 0;
    }
    w = TROLLEY_WK((cObjTrolley*) obj);
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->atari.throughOn();
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
    for (i = 0; i < 5; i++) {
        w->pSat[i] = 0;
        w->pEat[i] = 0;
    }
    w->Ride_pl = 0;
    p = w->Mot_tbl;
    for (i = 0; i < 9; i++) {
        *p++ = 0;
    }
    obj->r_no_0 = 0;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    objTrolleySatSet((cObjTrolley*) obj);
    return obj;
}

// Per frame: clears the pieces' active bit (set again by the routine) and runs r_no_0 (0 Set,
// 1 Move, 2 Break).
void cObjTrolley::move()
{
    objTrolleySatClear(this);
    ObjTrolley_R0_move_tbl[r_no_0](this);
}

// r_no_0 == 0: waits at the start on the first frame of the run motion; when the scenario calls
// setStart (Be_flg bit0) the player rides (Ride_pl, Status_flg[0] 0x20) and the run begins.
void objTrolley_R0_Set(cObjTrolley* pObj)
{
    TrolleyWork* w = TROLLEY_WK(pObj);

    if (w->Mot_tbl[0]) {
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[0], 0, 0, 0x8001, 0);
        MotionMove(pObj, 0);
    } else {
        pObj->matUpdate();
    }
    pObj->partsWorldCalc();
    objTrolleyPushMtx(pObj);
    objTrolleySatSet(pObj);
    if (w->Be_flg & 1) {
        StaFlagOn(pG, STA_RIDE_GONDOLA);
        w->Ride_pl = 1;
        pObj->r_no_0 = 1;
        pObj->r_no_1 = 0;
        pObj->r_no_2 = 0;
        pObj->r_no_3 = 0;
    }
}

// r_no_0 == 1, the ride: r_no_2 0/1 first run motion (mot[0]) to its end -> Be_flg bit2 "stopped";
// 3 holds until set2ndStart (bit1); 4/5 second run (mot[1]): after frame 2250 the camera flag
// Status_flg[2] 0x08000000, 2300 the crash effect, 2865 the jump-off action button (random type
// 3 / 4 -> objTrolleyEscapeAction); the motion ending with the player aboard is death
// (plobjTrolleyDie). Every frame the riders are carried along and the front of the car hits enemies.
void objTrolley_R0_Move(cObjTrolley* pObj)
{
    TrolleyWork* w = TROLLEY_WK(pObj);

    objTrolleyPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[0], 0, 0, 0x8001, 0);
        SndCall(6, 0, 0, 0, 0, 0);
        SndCall(6, 1, 0, 0, 0, 0);
        pObj->r_no_2++;
    case 1:
        if (MotionMove(pObj, 0)) {
            SndCall(6, 2, 0, 0, 0, 0);
            SndCall(6, 3, 0, 0, 0, 0);
            w->Be_flg |= 4;
            pObj->r_no_2++;
        }
        break;
    case 2:
        pObj->r_no_2++;
    case 3:
        MotionMove(pObj, 0);
        if (w->Be_flg & 2) {
            pObj->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[1], 0, 0, 0x8001, 0);
        SndCall(6, 0, 0, 0, 0, 0);
        SndCall(6, 1, 0, 0, 0, 0);
        w->Timer = 20;
        pObj->r_no_3 = Rnd() & 1;
        pObj->r_no_2++;
    case 5:
        if (MotionMove(pObj, 0) && w->Ride_pl) {
            w->Ride_pl = 0;
            SetPlDamage((cEm*) pObj, plobjTrolleyDie);
            pObj->r_no_0 = 2;
            pObj->r_no_1 = 0;
            pObj->r_no_2 = 0;
            pObj->r_no_3 = 0;
        } else {
            if (pObj->Motion.Seq_frame > 2250.0f) {
                StaFlagOn(pG, STA_NO_FENCE);
            }
            if (pObj->Motion.Seq_frame > 2300.0f) {
                EstSet(pObj, -1, 0, 0, EFF_ROOM, 0x13, 0, ESP_CORE_KIND_NONE, pObj, 0);
            }
            if (pObj->Motion.Seq_frame > 2865.0f) {
                if (pObj->r_no_3) {
                    ActBtn.set(ACT_JUMP_DOWN, 0xB, (void*) objTrolleyEscapeAction, pObj, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
                } else {
                    ActBtn.set(ACT_JUMP_DOWN, 0xB, (void*) objTrolleyEscapeAction, pObj, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);
                }
            }
        }
        break;
    }
    pObj->partsWorldCalc();
    objTrolleyGetAdjust(pObj);
    if (w->Ride_pl) {
        objTrolleyMoveAdjustPL(pObj);
    }
    objTrolleyMoveAdjustEM(pObj);
    objTrolleySatSet(pObj);
    objTrolleyHitCk(pObj);
}

// r_no_0 == 2, the crash: the cars are put at the crash point and play the break motion (mot[2]
// when the player escaped, mot[3] when he died), the enemies aboard fall and, when the motion
// ends, are told setTrolleyLost and the cars vanish.
void objTrolley_R0_Break(cObjTrolley* pObj)
{
    TrolleyWork* w = TROLLEY_WK(pObj);

    objTrolleyPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        pObj->pos.x = -201000.0f;
        pObj->pos.y = -52400.0f;
        pObj->pos.z = 78000.0f;
        pObj->ang.y = 0.0f;
        if (pObj->r_no_3) {
            MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[2], 0, 0, 0x8001, 0);
        } else {
            MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[3], 0, 0, 0x8001, 0);
        }
        SndCall(6, 2, 0, 0, 0, 0);
        SndCall(6, 3, 0, 0, 0, 0);
        objTrolleyFallEM(pObj);
        pObj->r_no_2++;
    case 1:
        if (MotionMove(pObj, 0)) {
            objTrolleyLostEM(pObj);
            pObj->be_flag &= ~2;
        }
        break;
    }
    pObj->partsWorldCalc();
}

// Marks all scenario / effect pieces inactive (m_Flag bit2) until objTrolleySatSet re-places them.
static void objTrolleySatClear(cObjTrolley* pObj)
{
    TrolleyWork* w = TROLLEY_WK(pObj);
    int i;

    for (i = 0; i < 5; i++) {
        if (w->pSat[i]) {
            w->pSat[i]->setDisable();
        }
        if (w->pEat[i]) {
            w->pEat[i]->setDisable();
        }
    }
}

// Places (or on the first call creates from room archive file 5, shapes 3/2/1 scenario and 6/5/4
// effect) one collision piece per car at the car's parts position + 500, following its yaw.
void objTrolleySatSet(cObjTrolley* pObj)
{
    TrolleyWork* w = TROLLEY_WK(pObj);
    Vec pos;
    Vec rot;
    Vec v;
    cParts* parts;
    cSat** sat = w->pSat;
    u32 i;

    for (i = 0; i < 3; i++) {
        parts = pObj->getPartsPtr(Trolley_parts_tbl[i]);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1.0f;
        PSMTXMultVecSR(parts->mat, &v, &v);
        rot.x = 0.0f;
        rot.y = atan2f(v.x, v.z);
        rot.z = 0.0f;
        pos = parts->world;
        pos.y += 500.0f;
        if (w->pSat[i]) {
            w->pSat[i]->setEnable();
            w->pSat[i]->setCoord(&pos, &rot);
        } else {
            switch (i) {
            case 0:
            default:
                w->pSat[i] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 3);
                break;
            case 1:
                sat[1] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
                break;
            case 2:
                sat[2] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
                break;
            }
        }
        cSat** sat2 = w->pEat;
        if (w->pEat[i]) {
            w->pEat[i]->setEnable();
            w->pEat[i]->setCoord(&pos, &rot);
        } else {
            switch (i) {
            case 0:
            default:
                w->pEat[i] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 6);
                break;
            case 1:
                sat2[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 5);
                break;
            case 2:
                sat2[2] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 4);
                break;
            }
        }
    }
}

// Action button callback: the player jumps off (plobjTrolleyEscape) and the trolley crashes.
void objTrolleyEscapeAction(cObjTrolley* ptr)
{
    TROLLEY_WK(ptr)->Ride_pl = 0;
    SetPlDamage((cEm*) ptr, plobjTrolleyEscape);
    ptr->r_no_0 = 2;
    ptr->r_no_1 = 0;
    ptr->r_no_2 = 0;
    ptr->r_no_3 = 1;
}

// Player damage routine of the jump-off: r_no_2 0/1 the jump motion (mot[4]) with its SEs, 2/3 the
// hang-on motion (mot[6]) with a 90-frame button mash (m_Work1 presses needed: 5 / 10 / 15 by
// Game_level), 4/5 climbs up (mot[7]) and returns control, 6/7 falls (mot[8]) and dies.
void plobjTrolleyEscape(cPlayer* pEm)
{
    cEm* em = (cEm*) pEm;
    cObjTrolley* obj = (cObjTrolley*) em->pEmCatch;
    TrolleyWork* w = TROLLEY_WK((cObjTrolley*) obj);
    cParts* parts = em->getPartsPtr(4);

    em->subArc = pPL->pEmCatch->subArc;
    em->dmg.set(0, 0xF);
    switch (em->r_no_2) {
    case 0:
        em->pos.x = -201027.56f;
        em->pos.y = -51832.78f;
        em->pos.z = 73991.43f;
        em->ang.y = 0.0f;
        MotionSetCore(em, &em->Motion, w->Mot_tbl[4], 0, 0, 0x201, 0);
        em->atari.throughOn();
        em->be_flag &= ~0x10;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        } else {
            if (em->Motion.Seq_frame > 15.7f && em->Motion.Seq_frame < 16.3f) {
                SndCall(1, 0x10, &parts->world, 0, 0, em);
            }
            if (em->Motion.Seq_frame > 23.7f && em->Motion.Seq_frame < 24.3f) {
                SndCall(1, 0x34, &parts->world, 0, 0, em);
            }
            if (em->Motion.Seq_frame > 29.7f && em->Motion.Seq_frame < 30.3f) {
                SndCall(1, 0x4F, &parts->world, 0, 0, em);
            }
        }
        break;
    case 2:
        em->pos.x = -201073.98f;
        em->pos.y = -52360.74f;
        em->pos.z = 95802.8f;
        em->ang.y = 0.0f;
        MotionSetCore(em, &em->Motion, w->Mot_tbl[6], 0, 0, 0x201, 0);
        PlGachaInit();
        ((cPlayer*) em)->m_Work0 = 90;
        ((cPlayer*) em)->m_Work1 = 10;
        if (pG->Game_level <= 2) {
            ((cPlayer*) em)->m_Work1 = 5;
        }
        if (pG->Game_level > 7) {
            ((cPlayer*) em)->m_Work1 = 15;
        }
        em->r_no_2++;
    case 3:
        ActBtn.set(ACT_CLIMB, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
        if (Key.trg & 0x80000000) {
            if (((cPlayer*) em)->m_Work1) {
                ((cPlayer*) em)->m_Work1--;
            }
        }
        MotionMove(em, 0);
        if (((cPlayer*) em)->m_Work0) {
            ((cPlayer*) em)->m_Work0--;
        } else if (((cPlayer*) em)->m_Work1) {
            em->r_no_2 = 6;
        } else {
            em->r_no_2 = 4;
        }
        break;
    case 4:
        em->pos.x = -201078.39f;
        em->pos.y = -52361.7f;
        em->pos.z = 95773.43f;
        em->ang.y = 0.0f;
        MotionSetCore(em, &em->Motion, w->Mot_tbl[7], 0, 0, 0x201, 0);
        em->r_no_2++;
        SndRoomStrStop(2);
        SndStrReq(1, 0x2F, 0x80000003, 0, 0, 0.0f);
    case 5:
        if (MotionMove(em, 0)) {
            em->be_flag |= 0x10;
            em->atari.throughOff();
            EndPlDamage();
        } else {
            if (em->Motion.Seq_frame == 140.0f) {
                FootSeCall(SE_LEON_WALK_R, &em->pos, 0, 0);
            }
            if (em->Motion.Seq_frame == 173.0f) {
                FootSeCall(SE_LEON_WALK_L, &em->pos, 0, 0);
            }
        }
        break;
    case 6:
        em->pos.x = -201078.39f;
        em->pos.y = -52361.7f;
        em->pos.z = 95773.43f;
        em->ang.y = 0.0f;
        MotionSetCore(em, &em->Motion, w->Mot_tbl[8], 0, 0, 0x201, 0);
        SndCall(1, 0x4A, &pPL->getPartsPtr(4)->world, 0, 0, em);
        pG->pl_life = 0;
        em->r_no_2++;
    case 7:
        MotionMove(em, 0);
        break;
    }
    em->subArc = em->subArc2;
}

// Player damage routine when he was still aboard at the crash: the death motion (mot[5]), life 0.
void plobjTrolleyDie(cPlayer* pEm)
{
    cEm* em = (cEm*) pEm;
    cObjTrolley* obj = (cObjTrolley*) em->pEmCatch;
    TrolleyWork* w = TROLLEY_WK((cObjTrolley*) obj);
    u8 step;

    em->subArc = pPL->pEmCatch->subArc;
    em->dmg.set(0, 0xF);
    step = em->r_no_2;
    switch (step) {
    case 0:
        em->pos.x = -201000.0f;
        em->pos.y = -52400.0f;
        em->pos.z = 87300.0f;
        em->ang.y = 0.0f;
        MotionSetCore(em, &em->Motion, w->Mot_tbl[5], 0, 0, 1, 0);
        em->atari.throughOn();
        pG->pl_life = step;
        em->be_flag &= ~0x10;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
    em->subArc = em->subArc2;
}

// Copies the 9 motions from the room and starts the first run motion (frame 0).
void cObjTrolley::setMotion(void** mot_tbl)
{
    TrolleyWork* w = TROLLEY_WK(this);
    int i;

    for (i = 0; i < 9; i++) {
        w->Mot_tbl[i] = mot_tbl[i];
    }
    if (w->Mot_tbl[0]) {
        MotionSetCore(this, &Motion, w->Mot_tbl[0], 0, 0, 0x8001, 0);
    }
}

// Saves the three cars' matrices as Trolley_MatOld before the motion moves them.
void objTrolleyPushMtx(cObjTrolley* pObj)
{
    u32 i;

    for (i = 0; i < 3; i++) {
        PSMTXCopy(pObj->getPartsPtr(Trolley_parts_tbl[i])->mat, Trolley_MatOld[i]);
    }
}

// Which car (0-2) `pos` stands in, using last frame's matrices (1800 x 1000 x 3900 box); -1 none.
int objTrolleyGetTrolleyNo(cObjTrolley* pObj, Vec* pPos)
{
    Mtx inv;
    Vec v;
    int i;

    for (i = 0; i < 3; i++) {
        PSMTXInverse(Trolley_MatOld[i], inv);
        PSMTXMultVec(inv, pPos, &v);
        if (v.x > -900.0f && v.x < 900.0f && v.y > 0.0f && v.y < 1000.0f && v.z > -1950.0f && v.z < 1950.0f) {
            return i;
        }
    }
    return -1;
}

// Same as objTrolleyGetTrolleyNo with this frame's matrices.
int objTrolleyGetTrolleyNo2(cObjTrolley* pObj, Vec* pPos)
{
    Mtx inv;
    Vec v;
    int i;

    for (i = 0; i < 3; i++) {
        PSMTXInverse(pObj->getPartsPtr(Trolley_parts_tbl[i])->mat, inv);
        PSMTXMultVec(inv, pPos, &v);
        if (v.x > -900.0f && v.x < 900.0f && v.y > 0.0f && v.y < 1000.0f && v.z > -1950.0f && v.z < 1950.0f) {
            return i;
        }
    }
    return -1;
}

// Per car: movement (Trolley_vec) and yaw change (Trolley_dir) between last and this frame.
void objTrolleyGetAdjust(cObjTrolley* pObj)
{
    Vec p1;
    Vec p0;
    Vec d;
    cParts* parts;
    f32 a0;
    f32 a1;
    u32 i;

    for (i = 0; i < 3; i++) {
        p0.x = 0.0f;
        p0.y = 500.0f;
        p0.z = 0.0f;
        PSMTXMultVec(Trolley_MatOld[i], &p0, &p0);
        d.x = 0.0f;
        d.y = 0.0f;
        d.z = 1.0f;
        PSMTXMultVecSR(Trolley_MatOld[i], &d, &d);
        a0 = atan2f(d.x, d.z);
        parts = pObj->getPartsPtr(Trolley_parts_tbl[i]);
        p1.x = 0.0f;
        p1.y = 500.0f;
        p1.z = 0.0f;
        PSMTXMultVec(parts->mat, &p1, &p1);
        d.x = 0.0f;
        d.y = 0.0f;
        d.z = 1.0f;
        PSMTXMultVecSR(parts->mat, &d, &d);
        a1 = atan2f(d.x, d.z);
        PSVECSubtract(&p1, &p0, &Trolley_vec[i]);
        Trolley_dir[i] = Muku2(a0, a1, PI);
    }
}

// Carries `em` with the car it stands on: re-expresses its position in the car's new matrix,
// adds the same yaw, flags be_flag 0x20000000; for the player also shifts the extra camera and
// quake_ofs.
void objTrolleySetAdjust(cObjTrolley* pObj, cEm* em)
{
    Mtx inv;
    Vec v;
    Vec d;
    cParts* parts;
    int no;

    no = objTrolleyGetTrolleyNo(pObj, &em->pos);
    if (no == -1) {
        return;
    }
    em->be_flag |= 0x20000000;
    parts = pObj->getPartsPtr(Trolley_parts_tbl[no]);
    PSMTXInverse(Trolley_MatOld[no], inv);
    PSMTXMultVec(inv, &em->pos, &v);
    PSMTXMultVec(parts->mat, &v, &v);
    PSVECSubtract(&v, &em->pos, &d);
    PSVECAdd(&em->Catch_at_adj, &d, &em->Catch_at_adj);
    em->ang.y += Trolley_dir[no];
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    em->setPos(&v);
    if (em->id == 0) {
        if (CamCtrl.m_pExtraCamera) {
            PSVECAdd(&((Camera*) CamCtrl.m_pExtraCamera)->param.at, &d, &((Camera*) CamCtrl.m_pExtraCamera)->param.at);
            PSVECAdd(&((Camera*) CamCtrl.m_pExtraCamera)->param.pos, &d, &((Camera*) CamCtrl.m_pExtraCamera)->param.pos);
        }
        pG->quake_ofs = d;
    }
}

// The player rides along.
void objTrolleyMoveAdjustPL(cObjTrolley* pObj)
{
    objTrolleySetAdjust(pObj, pPL);
}

// Live room enemies (id 0x10..0x40) ride along; the enemy weapon (0x42) recalculates its parent.
void objTrolleyMoveAdjustEM(cObjTrolley* pObj)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if ((em->be_flag & 0x201) == 1) {
            if (em->id == 0x42) {
                ((cEmWep*) em)->setParentMatCalc(1);
            } else if (em->id > 0xF) {
                if (em->id <= 0x40) {
                    objTrolleySetAdjust(pObj, em);
                }
            }
        }
    }
}

// Is `pos` on a car? Returns 1 with the car's parts number and the position in car space (last
// frame's matrix) — used by the room to attach enemies.
int cObjTrolley::ckTrolleyRide(Vec* pPos, u8* pParts_no, Vec* pOffset)
{
    Mtx inv;
    cParts* parts;
    int no;

    no = objTrolleyGetTrolleyNo2(this, pPos);
    if (no == -1) {
        return 0;
    }
    parts = getPartsPtr(Trolley_parts_tbl[no]);
    PSMTXInverse(Trolley_MatOld[no], inv);
    PSMTXMultVec(inv, pPos, pOffset);
    *pParts_no = Trolley_parts_tbl[no];
    return 1;
}

// Moves `pos` with the car it stands on into `out` (out = pos when not on a car); returns 1 if on.
int cObjTrolley::ckTrolleyRideAdjust(Vec* pPos, Vec* pPos2)
{
    Mtx inv;
    Vec v;
    cParts* parts;
    int no;

    *pPos2 = *pPos;
    no = objTrolleyGetTrolleyNo(this, pPos);
    if (no == -1) {
        return 0;
    }
    parts = getPartsPtr(Trolley_parts_tbl[no]);
    PSMTXInverse(Trolley_MatOld[no], inv);
    PSMTXMultVec(inv, pPos, &v);
    PSMTXMultVec(parts->mat, &v, pPos2);
    return 1;
}

// Scenario: begin the ride (Be_flg bit0).
void cObjTrolley::setStart()
{
    TROLLEY_WK(this)->Be_flg |= 1;
}

// Scenario: begin the second run (Be_flg bit1), clears "stopped".
void cObjTrolley::set2ndStart()
{
    TROLLEY_WK(this)->Be_flg |= 2;
    TROLLEY_WK(this)->Be_flg &= ~4;
}

// 1 while the trolley has stopped after the first run (Be_flg bit2).
int cObjTrolley::ckStop()
{
    if (TROLLEY_WK(this)->Be_flg & 4) {
        return 1;
    }
    return 0;
}

// While the front car moves (> 50 units / frame) three 400-radius 0x12 hit spheres 2700 ahead of
// it run down enemies in the way.
void objTrolleyHitCk(cObjTrolley* pObj)
{
    Vec v;
    cParts* parts;

    parts = pObj->getPartsPtr(0);
    if ((parts->world.x - parts->world_old2.x) * (parts->world.x - parts->world_old2.x) +
        (parts->world.y - parts->world_old2.y) * (parts->world.y - parts->world_old2.y) +
        (parts->world.z - parts->world_old2.z) * (parts->world.z - parts->world_old2.z) < 2500.0f) {
        return;
    }
    v.x = 0.0f;
    v.y = 500.0f;
    v.z = 2700.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 400.0f);
    v.x = 800.0f;
    v.y = 500.0f;
    v.z = 2700.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 400.0f);
    v.x = -800.0f;
    v.y = 500.0f;
    v.z = 2700.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 400.0f);
}

// At the crash every living room enemy (id 0x10..0x20) gets hp 0 and is forced into its fall
// routine (r_no_0 2 / r_no_1 7), remembering its yaw in x9BC.
void objTrolleyFallEM(cObjTrolley* pObj)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if ((em->be_flag & 0x201) == 1 && em->id > 0xF && em->id <= 0x20 && em->hp > 0) {
            em->hp = 0;
            TROLLEY_EM_WK(em)->yaw = em->ang.y;
            em->be_flag |= 0x10000;
            em->r_no_0 = 2;
            em->r_no_1 = 7;
            em->r_no_2 = 0;
            em->r_no_3 = 1;
        }
    }
}

// After the break motion the hidden room enemies are told setTrolleyLost (they vanish).
void objTrolleyLostEM(cObjTrolley* pObj)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if ((em->be_flag & 0x201) == 1 && em->id > 0xF && em->id <= 0x20 && (em->be_flag & 2)) {
            ((cEmRoom*) em)->setTrolleyLost();
        }
    }
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
