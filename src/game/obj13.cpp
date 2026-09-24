// game/obj13: object id 0x13, the ladder (D:/Bio4/Prog/obj13.cpp). LadderWork status: 0 standing
// (climbable), 1 knocked down, 2/3 falling, 4 in motion. The player climbs it (action button 8 ->
// plobjLadderClimb, motions mot[0..3]), kicks it down (button 0xA -> plobjLadderDown, mot[5..10])
// and puts it back up (button 0xB -> plobjLadderReset, mot[4]); the partner climbs with
// subobjLadderClimb (mot[16..19]). R1 routines: 0 Set (standing), 1 Fall (with damage areas), 2
// Down, 3 Reset. A paired ladder shares the collision flags; breakWindow smashes windows at the top.
#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "obj20.h"
#include "obj13.h"
#include "em.h"
#include "emwindow.h"
#include "global.h"
#include "math_sub.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "snd.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "at_mod.h"
#include "etc_model.h"
#include "motion.h"

extern "C" {
cObj* SetLadder(void* bin, void* tpl, Vec* pos, Vec* rot, int no);
void objLadder_R1_Set(cObjLadder* obj);
void objLadder_R1_Fall(cObjLadder* obj);
void objLadder_R1_Down(cObjLadder* obj);
void objLadder_R1_Reset(cObjLadder* obj);
void objLadderSatSet(cObjLadder* obj);
void objLadderClimbActEvtCk(cObjLadder* obj);
void objLadderActClimb(cObjLadder* obj);
void plobjLadderClimb(cPlayer* pl);
int SubLadderClimbCk(cEm* em);
int SubLadderClimbCk2(cEm* em);
void subobjLadderClimb(cEm* em);
void objLadderClimbCamMove(cEm* em);
void objLadderDownActEvtCk(cObjLadder* obj);
void objLadderActDown(cObjLadder* obj);
void plobjLadderDown(cPlayer* pl);
void objLadderDownCamMove(cEm* em);
void objLadderResetActEvtCk(cObjLadder* obj);
void objLadderActReset(cObjLadder* obj);
void plobjLadderReset(cPlayer* pl);
void objLadderResetCamMove(cEm* em);
int LadderNearCk(Vec* pos);
void LadderEventTrans(int mode);
}

void (*ObjLadder_R1_move_tbl[4])(cObjLadder*) = {
    objLadder_R1_Set, objLadder_R1_Fall, objLadder_R1_Down, objLadder_R1_Reset,
};

// Creates ladder etc `no` (skipped when its etc flag bit 0 says it is gone): model, box collision,
// 6 rungs, standing at pos/rot with the ladder parts tilted -110 degrees.
cObj* SetLadder(void* bin, void* tpl, Vec* pos, Vec* rot, int no)
{
    cObj* obj;
    LadderWork* w;
    cParts* parts;
    u16* flg;

    flg = GetEtcFlgPtr(no, pG->room_id);
    if (flg && (*flg & 1)) {
        return 0;
    }
    obj = ObjMgr.create(cObjMgr::ID_LADDER);
    if (obj == 0) {
        return 0;
    }
    w = LADDER_WK((cObjLadder*) obj);
    w->Etc_no = no;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->atari, 0.0f, 1000.0f, -700.0f, 330.0f, 600.0f, 600.0f, 1000.0f, 0, 2, 0);
    obj->atari.m_flag &= ~0x100;
    obj->atari.m_flag |= 0x10;
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
    }
    parts = obj->getPartsPtr(0);
    parts->ang.x = -1.9198622f;
    parts->ang.y = 0.0f;
    parts->ang.z = 0.0f;
    w->Ladder_num = 6;
    w->Status = 0;
    w->St_pos = obj->pos;
    w->St_dir = obj->ang.y;
    w->Climb_wait = 0;
    w->Reset_wait = 0;
    w->Down_wait = 0;
    obj->type = 0;
    w->pSat = 0;
    w->Cam_no = -1;
    w->pHosei = 0;
    return obj;
}

// Per-frame: timers, collision off while hidden (flags bit 1), the R1 routine, enemy-attack check
// and collision update.
void cObjLadder::move()
{
    LadderWork* w = LADDER_WK(this);

    if (w->Climb_wait) {
        w->Climb_wait--;
    }
    if (w->Reset_wait) {
        w->Reset_wait--;
    }
    if (LADDER_WK(this)->be_flag & 2) {
        atari.offOba();
        if (w->pHosei) {
            w->pHosei->atari.offOba();
        }
    } else {
        ObjLadder_R1_move_tbl[r_no_1](this);
        EmAtCheck((cEm*) this);
        atari.move();
    }
}

// Rno1 == 0: standing: resets pose to basePos/baseRotY, collision on (also the pair), offers the
// climb and kick-down action buttons.
void objLadder_R1_Set(cObjLadder* pObj)
{
    LadderWork* w = LADDER_WK(pObj);
    cParts* parts;

    pObj->pos = w->St_pos;
    pObj->ang.x = 0.0f;
    pObj->ang.y = w->St_dir;
    pObj->ang.z = 0.0f;
    parts = pObj->getPartsPtr(0);
    parts->ang.x = -1.9198622f;
    parts->ang.y = 0.0f;
    parts->ang.z = 0.0f;
    pObj->matUpdate();
    objLadderSatSet(pObj);
    objLadderClimbActEvtCk(pObj);
    objLadderDownActEvtCk(pObj);
    LADDER_WK(pObj)->be_flag &= ~4;
    pObj->atari.onOba();
    if (w->pHosei) {
        w->pHosei->atari.onOba();
    }
}

// Rno1 == 1: falling: after downTimer plays the fall motion (crash sound on Motion.Seq_old.Free bit 0), on
// its end status 1 and two damage areas (kind 3) along the fallen ladder, then Rno1 = 2.
void objLadder_R1_Fall(cObjLadder* pObj)
{
    LadderWork* w = LADDER_WK(pObj);
    Vec v;

    w->Status = 4;
    switch (pObj->r_no_2) {
    case 0:
        pObj->r_no_2++;
    case 1:
        if (w->Down_wait) {
            if (--w->Down_wait == 0) {
                w->Status = 3;
            }
        }
        if (pObj->Motion.pMot) {
            if (pObj->Motion.Seq_old.Free & 1) {
                SndCall(6, 0x3F, &pObj->pos, 0, 0, 0);
            }
            if (MotionMove(pObj, 0)) {
                w->Status = 1;
                pObj->r_no_0 = 1;
                pObj->r_no_1 = 2;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            }
        }
        break;
    }
    pObj->partsWorldCalc();
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(pObj->mat, &v, &v);
    v.y = pObj->pos.y;
    DmgMgr.set(DMG_TYPE_PUSH, 2, &v, 1500.0f, 1000.0f);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 2000.0f;
    PSMTXMultVec(pObj->mat, &v, &v);
    v.y = pObj->pos.y;
    DmgMgr.set(DMG_TYPE_PUSH, 2, &v, 1500.0f, 1000.0f);
    objLadderSatSet(pObj);
    pObj->atari.offOba();
    if (w->pHosei) {
        w->pHosei->atari.offOba();
    }
}

// Rno1 == 2: lying down: offers the reset action button; collision off.
void objLadder_R1_Down(cObjLadder* pObj)
{
    LadderWork* w = LADDER_WK(pObj);

    w->Status = 1;
    pObj->matUpdate();
    objLadderResetActEvtCk(pObj);
    objLadderSatSet(pObj);
    pObj->atari.offOba();
    if (w->pHosei) {
        w->pHosei->atari.offOba();
    }
}

// Rno1 == 3: being put up: plays the reset motion (sound + breakWindow on Motion.Seq_old.Free bit 0), then
// standing (Rno1 = 0).
void objLadder_R1_Reset(cObjLadder* pObj)
{
    LadderWork* w = LADDER_WK(pObj);

    w->Status = 4;
    switch (pObj->r_no_2) {
    case 0:
        pObj->r_no_2++;
    case 1:
        if (pObj->Motion.pMot) {
            if (pObj->Motion.Seq_old.Free & 1) {
                SndCall(6, 0x41, &pObj->pos, 0, 0, 0);
                pObj->breakWindow();
            }
            if (MotionMove(pObj, 0)) {
                w->Status = 0;
                pObj->r_no_0 = 1;
                pObj->r_no_1 = 0;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            }
        }
        break;
    }
    pObj->partsWorldCalc();
    objLadderSatSet(pObj);
    pObj->atari.offOba();
    if (w->pHosei) {
        w->pHosei->atari.offOba();
    }
}

// LadderWork status (0 standing, 1 down, 2/3 falling, 4 moving).
int cObjLadder::getStatus()
{
    return LADDER_WK(this)->Status;
}

// Ladder type (1 = the top is 1000 lower: hatch variant).
int cObjLadder::getType()
{
    return type;
}

// 1 when the ladder can be climbed now (standing, not hidden, climbTimer expired).
int cObjLadder::ckClimb()
{
    LadderWork* w = LADDER_WK(this);

    if (w->Status != 0) {
        return 0;
    }
    if (w->Climb_wait != 0) {
        return 0;
    }
    u32 off = LADDER_WK(this)->be_flag & 2;
    return off == 0;
}

// Blocks further climbs for 90 frames (someone is on it).
void cObjLadder::setClimb()
{
    LADDER_WK(this)->Climb_wait = 90;
}

// Number of rungs (climb motion loops).
int cObjLadder::getLadderNum()
{
    return LADDER_WK(this)->Ladder_num;
}

// Sets the rung count and type.
void cObjLadder::setLadderInfo(int num, u8 t)
{
    LADDER_WK(this)->Ladder_num = num;
    type = t;
}

// Puts the ladder in the standing routine.
void cObjLadder::setStand()
{
    LADDER_WK(this)->Status = 0;
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Puts the ladder down instantly (room load state).
void cObjLadder::setDowned()
{
    LadderWork* w = LADDER_WK(this);
    cParts* parts;

    w->Status = 1;
    atari.offOba();
    if (w->pHosei) {
        w->pHosei->atari.offOba();
    }
    parts = getPartsPtr(0);
    parts->ang.x = 0.0f;
    parts->ang.y = 0.0f;
    parts->ang.z = 0.0f;
    r_no_0 = 1;
    r_no_1 = 2;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Starts the kick-down fall with motion `mot` after 17 frames.
void cObjLadder::setDown(void* mot, void* seq)
{
    LadderWork* w = LADDER_WK(this);

    w->Status = 2;
    w->Down_wait = 17;
    atari.offOba();
    if (w->pHosei) {
        w->pHosei->atari.offOba();
    }
    MotionSetCore(this, &Motion, mot, seq, 0, 1, 0);
    r_no_0 = 1;
    r_no_1 = 1;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Starts the fall from the current tilt: picks the fall motion frame matching the parts angle
// (enemy kicked it while the player climbs).
void cObjLadder::setDown2()
{
    LadderWork* w = LADDER_WK(this);
    void* mot = w->mot_tbl[10];
    void* a = w->mot_tbl[15];
    cParts* parts;
    int frame;

    w->Down_wait = 0;
    w->Status = 3;
    parts = getPartsPtr(0);
    frame = 0;
    if (parts->ang.x > -1.5707964f) {
        frame = 4;
    }
    if (parts->ang.x > -1.3962634f) {
        frame = 6;
    }
    if (parts->ang.x > -1.2217305f) {
        frame = 8;
    }
    if (parts->ang.x > -1.0471976f) {
        frame = 0xA;
    }
    if (parts->ang.x > -0.87266463f) {
        frame = 0xB;
    }
    if (parts->ang.x > -0.6981317f) {
        frame = 0xC;
    }
    if (parts->ang.x > -0.5235988f) {
        frame = 0xD;
    }
    if (parts->ang.x > -0.34906584f) {
        frame = 0xE;
    }
    atari.offOba();
    if (w->pHosei) {
        w->pHosei->atari.offOba();
    }
    MotionSetCore(this, &Motion, mot, a, 0, 1, frame);
    r_no_0 = 1;
    r_no_1 = 1;
    r_no_2 = 0;
    r_no_3 = 0;
}

// 1 when the ladder is down and the reset reserve timer expired.
int cObjLadder::ckReset()
{
    LadderWork* w = LADDER_WK(this);

    if (w->Status != 1) {
        return 0;
    }
    return w->Reset_wait == 0;
}

// Starts the reset motion (t 0/1 = the two variants mot[6]/mot[8]).
void cObjLadder::setReset(int mode)
{
    LadderWork* w = LADDER_WK(this);

    w->Status = 4;
    switch (mode) {
    case 0:
    default:
        MotionSetCore(this, &Motion, w->mot_tbl[6], w->mot_tbl[11], 0, 1, 0);
        break;
    case 1:
        MotionSetCore(this, &Motion, w->mot_tbl[8], w->mot_tbl[13], 0, 1, 0);
        break;
    }
    r_no_0 = 1;
    r_no_1 = 3;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Remembers the hidden state before an event (flags bit 3).
void cObjLadder::setTransOld()
{
    if (LADDER_WK(this)->be_flag & 2) {
        LADDER_WK(this)->be_flag |= 8;
    } else {
        LADDER_WK(this)->be_flag &= ~8;
    }
}

// Restores the hidden state after an event.
void cObjLadder::getTransOld()
{
    if (LADDER_WK(this)->be_flag & 8) {
        setOff();
    } else {
        setOn();
    }
}

// Hides the ladder (flags bit 1, not drawn, no collision).
void cObjLadder::setOff()
{
    LADDER_WK(this)->be_flag |= 2;
    be_flag &= ~2;
}

// Shows the ladder again.
void cObjLadder::setOn()
{
    LADDER_WK(this)->be_flag &= ~2;
    be_flag |= 2;
}

// Blocks the reset for 60 frames.
void cObjLadder::setResetReserve()
{
    LADDER_WK(this)->Reset_wait = 60;
}

// Collision flag 0x200 (blocking) on the ladder and its pair only while it is not standing.
void objLadderSatSet(cObjLadder* pObj)
{
    LadderWork* w = LADDER_WK(pObj);

    pObj->atari.offOba();
    if (w->pHosei) {
        w->pHosei->atari.offOba();
    }
    if (w->Status != 0) {
        return;
    }
    pObj->atari.onOba();
    if (w->pHosei) {
        w->pHosei->atari.onOba();
    }
}

// Offers the climb action button (8) when the player stands in front of the standing ladder
// (within the local box -500..1000 z, +-800 x, +-500 y) facing it.
void objLadderClimbActEvtCk(cObjLadder* pObj)
{
    LadderWork* w = LADDER_WK(pObj);
    Mtx m;
    Vec v;

    if (!(w->be_flag & 1)) {
        return;
    }
    if (w->Status) {
        return;
    }
    if (w->Climb_wait) {
        return;
    }
    if (pPL->r_no_0 != 0) {
        return;
    }
    if (fabsf(Muku2(pPL->ang.y, pObj->ang.y, PI)) < PI / 2.0f) {
        return;
    }
    PSMTXRotRad(m, 'y', pObj->ang.y);
    TransMatrix(m, &pObj->pos);
    PSMTXInverse(m, m);
    PSMTXMultVec(m, &pPL->pos, &v);
    if (v.z > 1000.0f) {
        return;
    }
    if (v.z < -500.0f) {
        return;
    }
    if (v.x > 800.0f) {
        return;
    }
    if (v.x < -800.0f) {
        return;
    }
    if (fabsf(v.y) > 500.0f) {
        return;
    }
    ActBtn.set(ACT_GO_UP, 5, (void*) objLadderActClimb, pObj, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
}

// Action button 8: blocks the ladder and puts the player into plobjLadderClimb.
void objLadderActClimb(cObjLadder* ptr)
{
    ptr->setClimb();
    SetPlDamage((cEm*) ptr, plobjLadderClimb);
}

// Player climb routine (via SetPlDamage): Rno2 0 snaps the player in front of the ladder and
// starts the mount motion (camera cut w->Cam_no), 1 loops the climb motion ladderNum times with
// step sounds, 2 the dismount, then returns control (camera Comeback).
void plobjLadderClimb(cPlayer* pEm)
{
    cEm* em = (cEm*) pEm;
    cObjLadder* obj = (cObjLadder*) em->pEmCatch;
    LadderWork* w = LADDER_WK(obj);
    Mtx m;
    Vec v;
    f32 fl;

    em->subArc = pPL->pEmCatch->subArc;
    StaFlagOn(pG, STA_PL_LADDER);
    em->dmg.set(0, 0xF);
    switch (em->r_no_2) {
    case 0:
        PSMTXRotRad(m, 'y', obj->ang.y);
        TransMatrix(m, &obj->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 300.0f;
        PSMTXMultVec(m, &v, &em->pos);
        em->ang.y = obj->ang.y + PI;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, w->mot_tbl[0], 0, 5, 1, 0);
        em->atari.off();
        ((cPlayer*) em)->m_Work0 = obj->getLadderNum();
        em->be_flag &= ~0x10;
        if (w->Cam_no != -1) {
            CamCtrl.CutCall((s8) w->Cam_no);
        }
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_frame > 9.7f && em->Motion.Seq_frame < 10.3f) {
            SndCall(6, 0x43, &em->pos, 0, 0, 0);
        }
        if (em->Motion.Seq_frame > 17.7f && em->Motion.Seq_frame < 18.3f) {
            SndCall(6, 0x42, &em->pos, 0, 0, 0);
        }
        if (MotionMove(em, 0)) {
            ((cPlayer*) em)->m_Work0 -= 4;
            if ((int) ((cPlayer*) em)->m_Work0 > 0) {
                em->r_no_2++;
            } else {
                em->r_no_2 = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, w->mot_tbl[1], 0, 5, 5, 0);
        em->r_no_2++;
    case 3:
        if (em->Motion.Seq_frame > 11.7f && em->Motion.Seq_frame < 12.3f) {
            SndCall(6, 0x43, &em->pos, 0, 0, 0);
        }
        if (em->Motion.Seq_frame > 21.7f && em->Motion.Seq_frame < 22.3f) {
            SndCall(6, 0x42, &em->pos, 0, 0, 0);
        }
        if (MotionMove(em, 0)) {
            ((cPlayer*) em)->m_Work0 -= 2;
            if ((int) ((cPlayer*) em)->m_Work0 > 0) {
                break;
            }
            em->r_no_2 = 4;
        }
        break;
    case 4:
        if (obj->getType() == 1) {
            MotionSetCore(em, &em->Motion, w->mot_tbl[3], 0, 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, w->mot_tbl[2], 0, 5, 1, 0);
        }
        if (w->Cam_no != -1) {
            CamCtrl.Comeback(0);
        }
        ((cPlayer*) em)->m_Work0 = 0;
        em->r_no_2++;
    case 5:
        if (obj->getType() == 1) {
            if (em->Motion.Seq_frame > 10.7f && em->Motion.Seq_frame < 11.3f) {
                SndCall(6, 0x43, &em->pos, 0, 0, 0);
            }
            if (em->Motion.Seq_frame > 32.7f && em->Motion.Seq_frame < 33.3f) {
                SndCall(5, 0xD, &em->getPartsPtr(0x14)->world, em->id, 0, 0);
            }
            if (em->Motion.Seq_frame > 35.7f && em->Motion.Seq_frame < 36.3f) {
                SndCall(5, 0xE, &em->getPartsPtr(0x18)->world, em->id, 0, 0);
            }
        } else {
            if (em->Motion.Seq_frame > 10.7f && em->Motion.Seq_frame < 11.3f) {
                SndCall(6, 0x43, &em->pos, 0, 0, 0);
            }
            if (em->Motion.Seq_frame > 24.7f && em->Motion.Seq_frame < 25.3f) {
                SndCall(5, 0xD, &em->getPartsPtr(0x14)->world, em->id, 0, 0);
            }
            if (em->Motion.Seq_frame > 34.7f && em->Motion.Seq_frame < 35.3f) {
                SndCall(5, 0xE, &em->getPartsPtr(0x18)->world, em->id, 0, 0);
            }
        }
        ((cPlayer*) em)->m_Work0++;
        if (obj->getType() != 1 && (int) ((cPlayer*) em)->m_Work0 > 0x17) {
            fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
            if (em->pos.y < fl) {
                em->pos.y = em->pos.y * 0.9f + fl * 0.1f;
            }
        }
        if (MotionMove(em, 0)) {
            fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
            }
            em->be_flag |= 0x10;
            EndPlDamage();
            em->atari.m_flag |= 0x100;
            em->atari.m_flag &= ~0x10;
        }
        break;
    }
    if (w->Cam_no == -1) {
        objLadderClimbCamMove(em);
    }
    em->subArc = em->subArc2;
}

// Partner: 1 (and starts subobjLadderClimb) when a climbable ladder is in front of the partner.
int SubLadderClimbCk(cEm* pEm)
{
    const f32 distLim = 1000000.0f;
    const f32 heightLim = 40000.0f;
    const f32 angLim = PI / 2.0f;
    u32 i;

    if (pSUB == 0) {
        return 0;
    }
    if (SUB_CHAR()->Route_h < 1000.0f) {
        return 0;
    }
    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObjLadder* obj = (cObjLadder*) ObjMgr.fastAt(i);

        if (obj->isAlive() && obj->id == 0x13 && obj->ckClimb()) {
            if ((pEm->pos.x - obj->pos.x) * (pEm->pos.x - obj->pos.x) + (pEm->pos.y - obj->pos.y) * (pEm->pos.y - obj->pos.y) +
                    (pEm->pos.z - obj->pos.z) * (pEm->pos.z - obj->pos.z) >
                distLim) {
                continue;
            }
            if (fabsf(Muku(&pEm->pos_old, &obj->pos, pEm->ang.y, PI)) > angLim) {
                continue;
            }
            if (fabsf(pEm->pos.y - obj->pos.y) > heightLim) {
                continue;
            }
            if (pEm->l_pl > 100000000.0f || pEm->pos.y + 1000.0f < pPL->pos.y) {
                LADDER_WK(obj)->be_flag |= 4;
                SetSubDamage((cEm*) obj, (void (*)()) subobjLadderClimb);
                obj->setClimb();
                return 1;
            }
        }
    }
    return 0;
}

// Partner: 1 when a standing ladder is within reach (no action started).
int SubLadderClimbCk2(cEm* pEm)
{
    u32 i;

    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObjLadder* obj = (cObjLadder*) ObjMgr.fastAt(i);

        if (obj->isAlive() && obj->id == 0x13 && obj->getStatus() != 0) {
            f32 dy = pEm->pos.y - obj->pos.y;

            if ((pEm->pos.x - obj->pos.x) * (pEm->pos.x - obj->pos.x) + dy * dy +
                    (pEm->pos.z - obj->pos.z) * (pEm->pos.z - obj->pos.z) >
                1000000.0f) {
                continue;
            }
            if (fabsf(dy) > 40000.0f) {
                continue;
            }
            return 1;
        }
    }
    return 0;
}

// Partner climb routine: mount (mot[16]), loop (mot[17]), dismount (mot[18/19]).
void subobjLadderClimb(cEm* pl)
{
    cEm* em = pSUB;
    cObjLadder* obj = (cObjLadder*) em->pEmCatch;
    LadderWork* w = LADDER_WK(obj);
    Mtx m;
    Vec p;
    Vec rot;
    f32 fl;

    LADDER_WK(obj)->be_flag |= 4;
    em->dmg.set(0, 2);
    switch (em->r_no_2) {
    case 0:
        PSMTXRotRad(m, 'y', obj->ang.y);
        TransMatrix(m, &obj->pos);
        p.x = 0.0f;
        p.y = 0.0f;
        p.z = 300.0f;
        PSMTXMultVec(m, &p, &p);
        rot.x = rot.z = 0.0f;
        rot.y = obj->ang.y + PI;
        rot.y = LIMIT_ANGLE(rot.y);
        ((cSubChar*) em)->m_MotBase.set((cMotModel*) em, &p, &rot, 10);
        MotionSetCore(em, &em->Motion, w->mot_tbl[16], 0, 5, 1, 0);
        em->atari.m_flag &= ~0x100;
        em->atari.m_flag |= 0x10;
        ((cSubChar*) em)->flg.on(cSubChar::F_SHADOW_OFF);
        ((cSubChar*) em)->m_Work0 = obj->getLadderNum();
        ((cSubChar*) em)->m_Work1 = 8;
        em->r_no_2++;
    case 1:
        if (((cSubChar*) em)->m_Work1) {
            ((cSubChar*) em)->m_Work1--;
        } else {
            StaFlagOn(pG, STA_SUB_LADDER);
        }
        if (em->Motion.Seq_frame > 8.7f && em->Motion.Seq_frame < 9.3f) {
            SndCall(6, 0x46, &em->pos, 0, 0, 0);
        }
        if (em->Motion.Seq_frame > 17.7f && em->Motion.Seq_frame < 18.3f) {
            SndCall(6, 0x45, &em->pos, 0, 0, 0);
        }
        if (MotionMove(em, 0)) {
            ((cSubChar*) em)->m_Work0 -= 4;
            if (((cSubChar*) em)->m_Work0 > 0) {
                em->r_no_2++;
            } else {
                em->r_no_2 = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, w->mot_tbl[17], 0, 5, 5, 0);
        em->r_no_2++;
    case 3:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (em->Motion.Seq_frame > 11.7f && em->Motion.Seq_frame < 12.3f) {
            SndCall(6, 0x46, &em->pos, 0, 0, 0);
        }
        if (em->Motion.Seq_frame > 20.7f && em->Motion.Seq_frame < 21.3f) {
            SndCall(6, 0x45, &em->pos, 0, 0, 0);
        }
        if (MotionMove(em, 0)) {
            ((cSubChar*) em)->m_Work0 -= 2;
            if (((cSubChar*) em)->m_Work0 > 0) {
                break;
            }
            em->r_no_2 = 4;
        }
        break;
    case 4:
        if (obj->getType() == 1) {
            MotionSetCore(em, &em->Motion, w->mot_tbl[19], 0, 5, 1, 0);
            ((cSubChar*) em)->m_Work1 = 0x28;
        } else {
            MotionSetCore(em, &em->Motion, w->mot_tbl[18], 0, 5, 1, 0);
            ((cSubChar*) em)->m_Work1 = 0x23;
        }
        ((cSubChar*) em)->m_Work0 = 0;
        em->r_no_2++;
    case 5:
        if (((cSubChar*) em)->m_Work1) {
            ((cSubChar*) em)->m_Work1--;
            StaFlagOn(pG, STA_SUB_LADDER);
        }
        if (obj->getType() == 1) {
            if (em->Motion.Seq_frame > 11.7f && em->Motion.Seq_frame < 12.3f) {
                SndCall(6, 0x43, &em->pos, 0, 0, 0);
            }
            if (em->Motion.Seq_frame > 22.7f && em->Motion.Seq_frame < 23.3f) {
                SndCall(5, 0xD, &em->getPartsPtr(0x14)->world, em->id, 0, 0);
            }
            if (em->Motion.Seq_frame > 42.7f && em->Motion.Seq_frame < 43.3f) {
                SndCall(5, 0xE, &em->getPartsPtr(0x18)->world, em->id, 0, 0);
                ((cSubChar*) em)->flg.off(cSubChar::F_SHADOW_OFF);
            }
        } else {
            if (em->Motion.Seq_frame > 11.7f && em->Motion.Seq_frame < 12.3f) {
                SndCall(6, 0x43, &em->pos, 0, 0, 0);
            }
            if (em->Motion.Seq_frame > 22.7f && em->Motion.Seq_frame < 23.3f) {
                SndCall(5, 0xD, &em->getPartsPtr(0x14)->world, em->id, 0, 0);
            }
            if (em->Motion.Seq_frame > 35.7f && em->Motion.Seq_frame < 36.3f) {
                SndCall(5, 0xE, &em->getPartsPtr(0x18)->world, em->id, 0, 0);
                ((cSubChar*) em)->flg.off(cSubChar::F_SHADOW_OFF);
            }
        }
        ((cSubChar*) em)->m_Work0++;
        if (obj->getType() != 1 && ((cSubChar*) em)->m_Work0 > 0x17) {
            fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
            if (em->pos.y < fl) {
                em->pos.y = em->pos.y * 0.9f + fl * 0.1f;
            }
        }
        if (MotionMove(em, 0)) {
            fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
            }
            EndSubDamage();
            ((cSubChar*) em)->flg.off(cSubChar::F_SHADOW_OFF);
            em->atari.m_flag |= 0x100;
            em->atari.m_flag &= ~0x10;
        }
        break;
    }
}

// Distance between two points.
static inline f32 LadderCamDist(Vec* a, Vec* b)
{
    return VEC_DIST(a, b);
}

// Extra camera during the climb: follows the climber from behind/below.
void objLadderClimbCamMove(cEm* pEm)
{
    static CAMERA objLadderClimbCam = { 0 };
    GlobalWork* g = pG;
    Vec camPos;
    Vec camAt;
    cParts* parts;

    parts = pEm->getPartsPtr(0);
    camPos.x = 0.0f;
    camPos.y = 300.0f;
    camPos.z = -1800.0f;
    camAt.x = 0.0f;
    camAt.y = 300.0f;
    camAt.z = 0.0f;
    PSMTXMultVec(parts->mat, &camPos, &camPos);
    PSMTXMultVec(parts->mat, &camAt, &camAt);
    PosToPos(&g->Camera.param.at, &camAt, &objLadderClimbCam.param.at, 1.0f);
    PosToPos(&g->Camera.param.pos, &camPos, &objLadderClimbCam.param.pos, 1.0f);
    objLadderClimbCam.Up.x = 0.0f;
    objLadderClimbCam.Up.y = 1.0f;
    objLadderClimbCam.Up.z = 0.0f;
    objLadderClimbCam.Distance = LadderCamDist(&objLadderClimbCam.param.pos, &objLadderClimbCam.param.at);
    objLadderClimbCam.param.fovy = 55.0f;
    CameraSetOrientationUp(&objLadderClimbCam);
    CamCtrl.SetExtraCamera(&objLadderClimbCam);
}

// Offers the kick-down action button (0xA) when the player is at the top of the standing ladder
// (hatch type: flag 0x20 variant).
void objLadderDownActEvtCk(cObjLadder* pObj)
{
    LadderWork* w = LADDER_WK(pObj);
    Mtx m;
    Vec v;
    f32 n;

    if (!(w->be_flag & 1)) {
        return;
    }
    if (w->Status) {
        return;
    }
    if (pPL->r_no_0 != 0) {
        return;
    }
    if (fabsf(Muku2(pPL->ang.y, pObj->ang.y, PI)) > PI / 2.0f) {
        return;
    }
    PSMTXRotRad(m, 'y', pObj->ang.y);
    v.x = 0.0f;
    n = (f32) w->Ladder_num;
    v.y = n * 533.3329f;
    v.z = n * -194.1173f;
    PSMTXMultVecSR(m, &v, &v);
    PSVECAdd(&pObj->pos, &v, &v);
    if (pObj->type == 1) {
        v.y -= 1000.0f;
    }
    TransMatrix(m, &v);
    PSMTXInverse(m, m);
    PSMTXMultVec(m, &pPL->pos, &v);
    if (v.z < -1000.0f) {
        return;
    }
    if (v.z > 500.0f) {
        return;
    }
    if (v.x > 800.0f) {
        return;
    }
    if (v.x < -800.0f) {
        return;
    }
    if (fabsf(v.y) > 500.0f) {
        return;
    }
    if (w->be_flag & 4) {
        ActBtn.set(ACT_KNOCK_DOWN, 5, (void*) objLadderActDown, pObj, ACTCTR_NO_EXEC, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    } else {
        ActBtn.set(ACT_KNOCK_DOWN, 5, (void*) objLadderActDown, pObj, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
}

// Action button 0xA: puts the player into plobjLadderDown.
void objLadderActDown(cObjLadder* ptr)
{
    LadderWork* w = LADDER_WK(ptr);

    if (!(w->be_flag & 4)) {
        SetPlDamage((cEm*) ptr, plobjLadderDown);
        w->Climb_wait = 90;
    }
}

// Player kick-down routine: kick motion (mot[5] or the hatch variant mot[9]) and setDown of the
// ladder, then back to control.
void plobjLadderDown(cPlayer* pEm)
{
    cEm* em = (cEm*) pEm;
    cObjLadder* obj = (cObjLadder*) em->pEmCatch;
    LadderWork* w = LADDER_WK(obj);
    Mtx m;
    Vec v;

    em->subArc = pPL->pEmCatch->subArc;
    em->dmg.set(0, 0xF);
    switch (em->r_no_2) {
    case 0:
        PSMTXRotRad(m, 'y', obj->ang.y);
        v.x = 0.0f;
        v.y = (f32) w->Ladder_num * 533.3329f;
        v.z = (f32) w->Ladder_num * -194.1173f;
        PSMTXMultVecSR(m, &v, &v);
        PSVECAdd(&obj->pos, &v, &v);
        TransMatrix(m, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -700.0f;
        PSMTXMultVec(m, &v, &v);
        em->pos.x = v.x;
        em->pos.z = v.z;
        em->ang.y = obj->ang.y;
        if (obj->getType() == 1) {
            MotionSetCore(em, &em->Motion, w->mot_tbl[5], 0, 5, 1, 0);
            obj->setDown(w->mot_tbl[7], w->mot_tbl[12]);
        } else {
            MotionSetCore(em, &em->Motion, w->mot_tbl[9], 0, 5, 1, 0);
            obj->setDown(w->mot_tbl[10], w->mot_tbl[14]);
        }
        em->atari.off();
        em->r_no_2++;
    case 1:
        if (obj->getType() == 1) {
            if (em->Motion.Seq_frame > 16.7f && em->Motion.Seq_frame < 17.3f) {
                SndCall(6, 0x40, &em->pos, 0, 0, 0);
            }
        } else {
            if (em->Motion.Seq_frame > 12.7f && em->Motion.Seq_frame < 13.3f) {
                SndCall(6, 0x44, &em->pos, 0, 0, 0);
            }
        }
        if (MotionMove(em, 0)) {
            EndPlDamage();
            em->dmg.set(0, 0x1E);
            em->atari.on();
        }
        break;
    }
    objLadderDownCamMove(em);
    em->subArc = em->subArc2;
}

// Extra camera for the kick-down.
void objLadderDownCamMove(cEm* pEm)
{
    static CAMERA objLadderDownCam = { 0 };
    GlobalWork* g = pG;
    Vec camPos;
    Vec camAt;

    camPos.x = 0.0f;
    camPos.y = 2500.0f;
    camPos.z = -500.0f;
    camAt.x = 0.0f;
    camAt.y = 1000.0f;
    camAt.z = 500.0f;
    PSMTXMultVec(pEm->mat, &camPos, &camPos);
    PSMTXMultVec(pEm->mat, &camAt, &camAt);
    PosToPos(&g->Camera.param.at, &camAt, &objLadderDownCam.param.at, 1.0f);
    PosToPos(&g->Camera.param.pos, &camPos, &objLadderDownCam.param.pos, 1.0f);
    objLadderDownCam.Up.x = 0.0f;
    objLadderDownCam.Up.y = 1.0f;
    objLadderDownCam.Up.z = 0.0f;
    objLadderDownCam.Distance = LadderCamDist(&objLadderDownCam.param.pos, &objLadderDownCam.param.at);
    objLadderDownCam.param.fovy = 55.0f;
    CameraSetOrientationUp(&objLadderDownCam);
    CamCtrl.SetExtraCamera(&objLadderDownCam);
}

// Offers the reset action button (0xB) when the player stands at the foot of the fallen ladder.
void objLadderResetActEvtCk(cObjLadder* pObj)
{
    LadderWork* w = LADDER_WK(pObj);

    if (!(w->be_flag & 1)) {
        return;
    }
    if (w->Status != 1) {
        return;
    }
    if (pPL->r_no_0 != 0) {
        return;
    }
    if ((pObj->pos.x - pPL->pos.x) * (pObj->pos.x - pPL->pos.x) + (pObj->pos.z - pPL->pos.z) * (pObj->pos.z - pPL->pos.z) > 2250000.0f) {
        return;
    }
    if (fabsf(pObj->pos.y - pPL->pos.y) > 500.0f) {
        return;
    }
    ActBtn.set(ACT_STAND, 5, (void*) objLadderActReset, pObj, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
}

// Action button 0xB: puts the player into plobjLadderReset.
void objLadderActReset(cObjLadder* ptr)
{
    SetPlDamage((cEm*) ptr, plobjLadderReset);
    ptr->setResetReserve();
}

// Player reset routine: the lift motion (mot[4]) and setReset of the ladder.
void plobjLadderReset(cPlayer* pEm)
{
    cEm* em = (cEm*) pEm;
    cObjLadder* obj = (cObjLadder*) em->pEmCatch;
    LadderWork* w = LADDER_WK(obj);
    Mtx m;
    Vec v;
    int motA;

    em->subArc = pPL->pEmCatch->subArc;
    em->dmg.set(0, 0xF);
    switch (em->r_no_2) {
    case 0:
        PSMTXRotRad(m, 'y', obj->ang.y);
        TransMatrix(m, &obj->pos);
        if (Muku2(obj->ang.y, em->ang.y, PI) < 0.0f) {
            v.x = 890.014f;
            v.y = 0.0f;
            v.z = 1450.51f;
            motA = 1;
            em->ang.y = obj->ang.y - PI / 2.0f;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        } else {
            v.x = -890.014f;
            v.y = 0.0f;
            v.z = 1450.51f;
            motA = 0x41;
            em->ang.y = obj->ang.y + PI / 2.0f;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        PSMTXMultVec(m, &v, &em->pos);
        MotionSetCore(em, &em->Motion, w->mot_tbl[4], 0, 5, motA, 0);
        obj->setReset(0);
        em->atari.m_flag |= 0x10;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EndPlDamage();
            em->dmg.set(0, 0x1E);
            em->atari.m_flag &= ~0x10;
        }
        break;
    }
    objLadderResetCamMove(em);
    em->subArc = em->subArc2;
}

// Extra camera for the reset.
void objLadderResetCamMove(cEm* pEm)
{
    static CAMERA objLadderResetCam = { 0 };
    GlobalWork* g = pG;
    Vec camPos;
    Vec camAt;

    camPos.x = 0.0f;
    camPos.y = 1300.0f;
    camPos.z = -1000.0f;
    camAt.x = 0.0f;
    camAt.y = 1800.0f;
    camAt.z = 0.0f;
    PSMTXMultVec(pEm->mat, &camPos, &camPos);
    PSMTXMultVec(pEm->mat, &camAt, &camAt);
    PosToPos(&g->Camera.param.at, &camAt, &objLadderResetCam.param.at, 1.0f);
    PosToPos(&g->Camera.param.pos, &camPos, &objLadderResetCam.param.pos, 1.0f);
    objLadderResetCam.Up.x = 0.0f;
    objLadderResetCam.Up.y = 1.0f;
    objLadderResetCam.Up.z = 0.0f;
    objLadderResetCam.Distance = LadderCamDist(&objLadderResetCam.param.pos, &objLadderResetCam.param.at);
    objLadderResetCam.param.fovy = 55.0f;
    CameraSetOrientationUp(&objLadderResetCam);
    CamCtrl.SetExtraCamera(&objLadderResetCam);
}

// Installs the 20 motion pointers (player/partner/ladder motions) from the room's table.
void cObjLadder::setMotion(void** pMot)
{
    LadderWork* w = LADDER_WK(this);

    w->mot_tbl[0] = *pMot++;
    w->mot_tbl[1] = *pMot++;
    w->mot_tbl[2] = *pMot++;
    w->mot_tbl[3] = *pMot++;
    w->mot_tbl[4] = *pMot++;
    w->mot_tbl[5] = *pMot++;
    w->mot_tbl[6] = *pMot++;
    w->mot_tbl[7] = *pMot++;
    w->mot_tbl[8] = *pMot++;
    w->mot_tbl[9] = *pMot++;
    w->mot_tbl[10] = *pMot++;
    w->mot_tbl[11] = *pMot++;
    w->mot_tbl[12] = *pMot++;
    w->mot_tbl[13] = *pMot++;
    w->mot_tbl[14] = *pMot++;
    w->mot_tbl[15] = *pMot++;
    w->mot_tbl[16] = *pMot++;
    w->mot_tbl[17] = *pMot++;
    w->mot_tbl[18] = *pMot++;
    w->mot_tbl[19] = *pMot++;
    LADDER_WK(this)->be_flag |= 1;
}

// 0 when a standing ladder's top is within 2000 of `pos`.
int LadderNearCk(Vec* pPos)
{
    Mtx m;
    Vec v;
    u32 i;

    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObjLadder* obj = (cObjLadder*) ObjMgr.fastAt(i);
        LadderWork* w = LADDER_WK(obj);

        if (obj->isAlive() && obj->id == 0x13 && w->Status == 0 && !(LADDER_WK(obj)->be_flag & 2)) {
            PSMTXRotRad(m, 'y', obj->ang.y);
            v.x = 0.0f;
            v.y = (f32) w->Ladder_num * 533.3329f;
            v.z = (f32) w->Ladder_num * -194.1173f;
            PSMTXMultVecSR(m, &v, &v);
            PSVECAdd(&obj->pos, &v, &v);
            if (obj->type == 1) {
                v.y -= 1000.0f;
            }
            if ((pPos->x - v.x) * (pPos->x - v.x) + (pPos->y - v.y) * (pPos->y - v.y) + (pPos->z - v.z) * (pPos->z - v.z) < 4000000.0f) {
                return 0;
            }
        }
    }
    return 1;
}

// Event start (0): hides every ladder remembering its state; end (1): restores it.
void LadderEventTrans(int flag)
{
    u32 i;

    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObjLadder* obj = (cObjLadder*) ObjMgr.fastAt(i);

        if (obj->isAlive() && obj->id == 0x13) {
            if (flag == 1) {
                obj->getTransOld();
            } else {
                obj->setTransOld();
                obj->setOff();
            }
        }
    }
}

// Breaks the windows (em 0x46) within 2000 of the ladder's top.
void cObjLadder::breakWindow()
{
    LadderWork* w = LADDER_WK(this);
    Mtx m;
    Vec v;
    f32 n;
    u32 i;

    PSMTXRotRad(m, 'y', ang.y);
    TransMatrix(m, &pos);
    v.x = 0.0f;
    n = (f32) w->Ladder_num;
    v.y = n * 533.3329f;
    v.z = n * -194.1173f;
    PSMTXMultVec(m, &v, &v);
    if (type == 1) {
        v.y -= 1000.0f;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEmWindow* em = (cEmWindow*) EmMgr.fastAt(i);

        if (em->isAlive() && em->id == 0x46 && em->hp > 0 && (em->ChkStatus() & 1) == 0) {
            if ((em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.y - v.y) * (em->pos.y - v.y) + (em->pos.z - v.z) * (em->pos.z - v.z) <
                4000000.0f) {
                em->SetBreakAll(&v, 0, 0);
            }
        }
    }
}

// Camera cut used while climbing (-1 = none).
void cObjLadder::setCamera(int cam_no)
{
    LADDER_WK(this)->Cam_no = cam_no;
}
