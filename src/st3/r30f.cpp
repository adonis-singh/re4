#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "objBull.h"
#include "em.h"
#include "emhit.h"
#include "em_sub.h"
#include "em10.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "room_data.h"
#include "area.h"
#include "mes.h"
#include "game.h"
#include "pad.h"
#include "read.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "rnd.h"
#include "math_sub.h"
#include "vec.h"
#include <string.h>

// Room 3-0F (D:/Bio4/Prog/r30f.cpp): the bulldozer ride. Leon and Ashley drive through the four
// gates, fight off the truck and take the lift up.

struct R30fWork {
    cObj* lift;           // 0x000  the lift platform (room arc 0xC0/0xC4)
    cObjBull* bull;       // 0x004  the bulldozer (SetBull)
    cEmWrap em[90];       // 0x008  the Ganado list entries 0x78.. (setem)
    u8 pad_440[0x49C - 0x440];
    cSat* sat1;           // 0x49C  scenario piece 2 (R30fInit)
    cSat* sat2;           // 0x4A0  piece 3 (AreaSet 1)
    cSat* sat3;           // 0x4A4  piece 4 (AreaSet 4)
    cSat* sat4;           // 0x4A8  piece 5 (AreaSet 5)
    u8 pad_4AC[4];
    int liftFrame;        // 0x4B0  frames on the lift (bull routine 5)
    int liftReset;        // 0x4B4  frames until the lift area enemies are cleared
    int truckNo;          // 0x4B8  truck attack number (0..2)
    cEmHit* hit[2];       // 0x4BC  the truck's two hit models
    u8 pad_4C4[0x4D4 - 0x4C4];
    int truckLife;        // 0x4D4
    u8 pad_4D8[0x4EC - 0x4D8];
    Vec bullPos;          // 0x4EC  bulldozer position of the previous frame (adjust_func)
    u32 shake1;           // 0x4F8  routine 3 shakes shown
    u32 shake2;           // 0x4FC  routine 10 shakes shown
    Vec liftAdd;          // 0x500  bulldozer movement this frame
};


static R30fWork* r30f_wp;
#define r30f_work (r30f_wp)

static f32 reva_rate = 0.008f;
// The truck's two hit boxes (YarareInitCube)
static f32 hit0_x = 0.0f;
static f32 hit0_y = 0.0f;
static f32 hit0_z = -1600.0f;
static f32 hit0_w = 1600.0f;
static f32 hit0_h = 1800.0f;
static f32 hit0_d = 3500.0f;
static f32 hit1_x = 0.0f;
static f32 hit1_y = 0.0f;
static f32 hit1_z = -5100.0f;
static f32 hit1_w = 1600.0f;
static f32 hit1_h = 4000.0f;
static f32 hit1_d = 5500.0f;
static f32 lift_y_rate = 2.01f;

// The room bits live in the second word of the room's save record (RoomData).
// Room_flg bits of this room, from the PS2 symbols; RmfFlagChk(pG, n). objBull reports the lift through
// bits 4 and 9.
enum R30F_FLAG {
    RMF_DOOR1_BREAK = 0,
    RMF_DOOR2_BREAK = 1,
    RMF_DOOR3_BREAK = 2,
    RMF_DOOR4_BREAK = 3,
    RMF_LIFT_START = 4,
    RMF_DBG = 5,
    RMF_PL_RIDE = 6,
    RMF_LIFT_EMSET_START = 7,
    RMF_TRACK_DIE = 8,
    RMF_LIFT_END = 9,
    RMF_LIFT_GO = 10,
    RMF_GATE_OPEN_START = 11,
    RMF_RIFT_STOP_SND_CALL = 12,
    RMF_TRACK_ST1 = 13,
    RMF_TRACK_ST2 = 14,
    RMF_TRACK_ST3 = 15,
    RMF_TRACK_ED1 = 16,
    RMF_TRACK_ED2 = 17,
    RMF_TRACK_ED3 = 18,
    RMF_TO334 = 19,
    RMF_GO_BACK_TRG = 20,
    RMF_GO_BACK_SET = 21,
    RMF_FALL = 64,
};

#define R30F_SAVE_FLAGS (*(u32*) (RoomData.getRoomSavePtr(pG->room_id) + 4))

extern "C" {
cEm* setem(u8 no, int force);
void reva_common_move(cObj* obj, f32 lo, f32 hi);
void EmHitUpdate(cModel* m);
void last_bomb();
void Hit(int no);
static void track_destroy();
static void pl_gurd();
static void track_move();
static void adjust_func(cObj* obj);
static void R30f_ride();
static void plemRide(cPlayer* p);
static void R30f_ride2();
static void door1_break();
static void door2_break();
static void door3_break();
static void door4_break();
void setArea1();
void setArea2();
void setArea3();
void setArea4();
void setArea5();
void AreaSet(u32 no);
void em_destroy_area(int at);
void em_destroy();
void lift_stop_event();
static void lift_stop_task();
static void em_set();
static void lift_start_task();
static void r30f_switch();
void addPos(Vec* add, cModel* m);
void addPos_sca(Vec* add, SceAtWork* at);
void setLiftMoveAdd(Vec* add);
static void em_set2();
static void gate_open();
}

// Room init (the bulldozer ride): the lift adjust vector zeroed; enemy 0x1F pre-read; area 0x17 rides
// on object 0x1E; area layout 1; most areas off, area 0xF = the lift-yard Ganados. JumpPoint 1..4
// skips ahead to that gate (the area layouts and gates already broken); otherwise the ride task starts
// and the bulldozer (cObjBull, SetBull) is created with its motions; the truck hit boxes, the lift
// objects and the rest of the setup follow.
void R30fInit()
{
    R30fWork*& wp = r30f_work;   // reference: `lis work@ha` before the call (the Debug_alloc idiom)
    u32 lv = 0;

    {
        Vec zero = {0.0f, 0.0f, 0.0f};

        adjust_add_set(zero);
    }
#line 87 "D:/Bio4/Prog/r30f.cpp"
    wp = (R30fWork*) MEM_CALLOC(sizeof(R30fWork), 1, 0xd);
    EmReadSearch(0x1F, 0, 0);
    SceAtSetParent(0x17, SmdGetObjPtr(0x1E), 0);
    AreaSet(1);
    SceAtSetEnable(6, 0);
    SceAtSetEnable(0xC, 0);
    SceAtSetEnable(0xD, 0);
    SceAtSetEnable(0xE, 0);
    SceAtSetEnable(0x10, 0);
    SceAtSetEnable(0x14, 0);
    SceAtSetEnable(0x13, 0);
    SceAtSetEnable(0x11, 0);
    SceAtSetEnable(0x12, 0);
    SceAtDataSet_exec(0xF, 0x12, 0, (TaskFunc) em_set2, 0, 1);
    if (pG->JumpPoint != 0) {
        lv = pG->JumpPoint;
        if (lv > 4) {
            lv = 4;
        }
        if (lv != 0) {
            AreaSet(2);
        }
        if (lv > 1) {
            R30F_SAVE_FLAGS |= 0x40000000;
        }
        if (lv > 2) {
            AreaSet(4);
            AreaSet(5);
            AreaSet(6);
        }
        if (lv > 3) {
            AreaSet(4);
            AreaSet(5);
            AreaSet(6);
            AreaSet(7);
        }
    }
    {
        int zero = 0;

    if (R30F_SAVE_FLAGS & 0x40000000) {
        if (lv <= 2) {
            lv = 2;
        }
        AreaSet(2);
        r30f_work->truckNo = 2;
        SceExec(0x12, (TaskFunc) R30f_ride, 0, 0, 2, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 1, 0x801, ESP_CORE_KIND_ROOM01, (void*) zero, (void*) zero);
        SceAtSetEnable(0x18, 0);
        SceAtSetEnable(0x19, 0);
    } else {
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x801, ESP_CORE_KIND_ROOM01, 0, 0);
    }
    }
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};

    r30f_work->bull = (cObjBull*) SetBull(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, lv);
    if (r30f_work->bull) {
        {
            void* mot[12];

            mot[0] = ROOM_ARC_PTR(pG->pRoom, 0x21);
            mot[1] = ROOM_ARC_PTR(pG->pRoom, 0x22);
            mot[2] = ROOM_ARC_PTR(pG->pRoom, 0x23);
            mot[3] = ROOM_ARC_PTR(pG->pRoom, 0x24);
            mot[4] = ROOM_ARC_PTR(pG->pRoom, 0x25);
            mot[5] = ROOM_ARC_PTR(pG->pRoom, 0x26);
            mot[6] = ROOM_ARC_PTR(pG->pRoom, 0x29);
            mot[7] = ROOM_ARC_PTR(pG->pRoom, 0x2A);
            mot[8] = ROOM_ARC_PTR(pG->pRoom, 0x2B);
            mot[9] = ROOM_ARC_PTR(pG->pRoom, 0x2C);
            mot[10] = ROOM_ARC_PTR(pG->pRoom, 0x2D);
            mot[11] = ROOM_ARC_PTR(pG->pRoom, 0x36);
            r30f_work->bull->setMotion(mot);
            r30f_work->bull->setNoSuspend(0);
            if (r30f_work->bull->Motion.pAttachCam == 0) {
#line 163 "D:/Bio4/Prog/r30f.cpp"
                r30f_work->bull->Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 0xd);
            }
        }
    }
    EstSet(r30f_work->bull, -1, 0, 0, EFF_ROOM, 0x11, 0x801, ESP_CORE_KIND_NONE, 0, 0);
    if (pG->room_id_prev == 0xFFF && !StaFlagChk(pG, STA_SUB_ASHLEY)) {
        StaFlagOn(pG, STA_SUB_ASHLEY);
        SubCharInit(1, &pPL->pos, pPL->ang.y);
        SubCharCtrl(1, 0);
    }
    SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) R30f_ride, 0, 1);
    SceAtDataSet_exec(0xA, 0x12, 0, (TaskFunc) r30f_switch, 0, 1);
    {
        Vec zero = {0.0f, 0.0f, 0.0f};

        r30f_work->lift = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x30), ROOM_ARC_PTR(pG->pRoom, 0x31), &zero, &zero, 0x10, 1);
        if (r30f_work->lift) {
#line 195 "D:/Bio4/Prog/r30f.cpp"
            r30f_work->lift->Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 0xd);
        }
    }
    r30f_work->sat1 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x5), 0, (Vec*) &vecZero, (Vec*) &vecZero, 2);
}

// Enemy `no` of the list (entry 0x78 + no); a full room (more than 9 alive) only takes the forced ones.
cEm* setem(u8 no, int force)
{
    if ((u32) SceCountEmAlive(0x10, 0x20) <= 9 || force == 0) {
        r30f_work->em[no].setEm(no + 0x78, 6, 0, 1, 1);
    }
    return r30f_work->em[no].getPtr();
}

// Per frame (the ride script): debug displays; at the bulldozer's goal the exit area 0 runs; every 30
// frames of driving the Ganados too far behind are recycled; at fixed move frames of routines 2 and 4
// the list Ganados (setem) jump aboard / appear, the Ganado boarding cut (em_set) plays, the truck
// starts (track_move) and the gates break (doorN_break) as the bulldozer reaches them.
void R30fMain()
{
    cPlayer* pl;

    SceDebugDisp("R0[%d]", r30f_work->bull->r_no_0);
    SceDebugDisp("FM[%d]", r30f_work->bull->getMoveFrameRtn());
    if (r30f_work->bull->ckGoal() == 1) {
        SceAtExecute(0);
    }
    if (r30f_work->bull->r_no_0 != 5 && (u32) r30f_work->bull->getMoveFrameRtn() % 30 == 0) {
        u32 i;
        u32 j;

        for (j = 0; j < 90; j++) {
            for (i = 0; i < 90; i++) {
                if (r30f_work->em[i].isActive() == 1 && r30f_work->em[i].getPtr()->l_pl > 4000.0f) {
                    Vec p;

                    if (pSUB) {
                        p = pSUB->pos;
                    } else {
                        p = pPL->pos;
                    }
                    p.x += fRand1_1() * 3000.0f;
                    p.z += fRand1_1() * 3000.0f;
                    r30f_work->em[i].setGoto(&p, 0xC);
                }
                if (r30f_work->em[i].isAlive() == 1 && r30f_work->em[i].ckResetEnable() != 0) {
                    r30f_work->em[i].destroy();
                }
            }
        }
    }
    if (r30f_work->bull->getMoveFrameRtn() == 1) {
        SndRoomStrStart(1, 0, 1);
    }
    if (r30f_work->bull->r_no_0 == 2 && r30f_work->bull->getMoveFrameRtn() == 0x1E0) {
        SndStrReq(1, 0xE9, 0x80000003, 0, 0, 0.0f);
    }
    if (r30f_work->bull->r_no_0 == 2 && r30f_work->bull->getMoveFrameRtn() == 0x1FE) {
        SceExec(0x12, (TaskFunc) em_set, 0, 0, 2, 0);
    }
    if (r30f_work->bull->r_no_0 == 2 && r30f_work->bull->getMoveFrameRtn() == 0x26C) {
        setem(0x52, 1);
        setem(0x53, 1);
    }
    if (r30f_work->bull->r_no_0 == 2 && r30f_work->bull->getMoveFrameRtn() == 0x2D0) {
        setem(0x14, 1);
    }
    if (r30f_work->bull->r_no_0 == 2 && r30f_work->bull->getMoveFrameRtn() == 0x370) {
        em_destroy();
    }
    if (r30f_work->bull->r_no_0 == 2 && r30f_work->bull->getMoveFrameRtn() == 0x384) {
        setem(6, 1);
        setem(0x16, 1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 1) {
        setem(0x3B, 1);
        setem(0x3C, 1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x63) {
        setem(0xB, 1);
        setem(0xC, 1);
        setem(0xD, 1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x6E) {
        setem(0xE, 1);
        setem(0x3F, 1);
        setem(0x3D, 1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0xEF) {
        setem(0x15, 0);
        r30f_work->em[0x15].setFlag(1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0xF8) {
        setem(0xF, 0);
        r30f_work->em[0xF].setFlag(1);
        AreaSet(2);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x1FE) {
        setem(0x38, 1);
        setem(0x39, 1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x230) {
        setem(0x4D, 1);
        setem(0x40, 1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x33E) {
        em_destroy_area(3);
        em_destroy();
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x438) {
        setem(0x49, 1);
        setem(0x4A, 1);
        setem(0x4B, 1);
        setem(0x4C, 1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x47E) {
        setem(7, 1);
        r30f_work->em[7].setFlag(1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x4B0) {
        setem(8, 1);
        r30f_work->em[8].setFlag(1);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x4B0) {
        setem(0x2F, 1);
        setem(0x3E, 1);
        setem(0x3A, 1);
        AreaSet(2);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x708) {
        AreaSet(3);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameRtn() == 0x726) {
        SceExec(0x12, (TaskFunc) lift_start_task, 0, 0, 2, 0);
    }
    if (r30f_work->bull->r_no_0 == 5) {
        if (r30f_work->liftFrame == 0) {
            em_destroy_area(4);
            em_destroy();
            SceExec(0x12, (TaskFunc) lift_stop_task, 0, 0, 2, 0);
            r30f_work->liftFrame++;
        }
        if (RmfFlagChk(pG, RMF_LIFT_EMSET_START)) {
            r30f_work->liftFrame++;
        }
        if (((int) R30F_SAVE_FLAGS < 0 && RmfFlagChk(pG, RMF_FALL)) || DebugTrg(0) != 0) {
            if (r30f_work->liftReset == 0) {
                r30f_work->liftReset = 0x5A;
                em_destroy_area(0x15);
                SceAtSetEnable(8, 0);
                SceAtSetEnable(9, 0);
            }
        }
        if (r30f_work->liftFrame == 0x3DE) {
            setem(0x1D, 1);
            r30f_work->em[0x1D].setFlag(1);
            r30f_work->em[0x1D].setFindPL();
            if (r30f_work->em[0x1D].isActive() != 0) {
                r30f_work->em[0x1D].getPtr()->flag |= 0x40;
            }
        }
        if (r30f_work->liftFrame == 0x69A) {
            setem(0x24, 1);
            r30f_work->em[0x24].setFlag(1);
            r30f_work->em[0x24].setFindPL();
            if (r30f_work->em[0x24].isActive() != 0) {
                r30f_work->em[0x24].getPtr()->flag |= 0x40;
            }
        }
        if (r30f_work->liftReset != 0) {
            r30f_work->liftReset--;
            if (r30f_work->liftReset == 0) {
                SndCall(6, 0xC, 0, 0, 0, 0);
                SndCall(6, 0xD, 0, 0, 0, 0);
                RmfFlagOn(pG, RMF_LIFT_START);
                AreaSet(4);
            }
            if (r30f_work->liftReset == 0 || r30f_work->liftReset == 0x1E || r30f_work->liftReset == 0x3C ||
                r30f_work->liftReset == 0x46 || r30f_work->liftReset == 0x50) {
                em_destroy_area(0x15);
            }
        }
    }
    if (r30f_work->bull->ckLiftWait() != 0) {
        if (!RmfFlagChk(pG, RMF_RIFT_STOP_SND_CALL)) {
            RmfFlagOn(pG, RMF_RIFT_STOP_SND_CALL);
            SndCall(6, 0xE, 0, 0, 0, 0);
            SndCall(6, 0xF, 0, 0, 0, 0);
        }
        pl = pPL;
        if (RmfFlagChk(pG, RMF_PL_RIDE)) {
            // Two `andis.`: a folded `(f & A) && !(f & B)` would be one masked compare.
            if (!RmfFlagChk(pG, RMF_GATE_OPEN_START) && pl->checkEvent() == 1 && pPL->pos.y > -8800.0f) {
                RmfFlagOn(pG, RMF_GATE_OPEN_START);
                SceExec(0x12, (TaskFunc) gate_open, 0, 0, 2, 0);
                SceAtSetEnable(6, 0);
                SceAtSetEnable(0xC, 0);
                SceAtSetEnable(0x10, 0);
                SceAtSetEnable(0x11, 0);
                SceAtSetEnable(0x12, 0);
            }
        }
        if (RmfFlagChk(pG, RMF_LIFT_GO)) {
            RmfFlagOn(pG, RMF_LIFT_END);
        }
    }
    if (r30f_work->bull->r_no_0 == 7 && r30f_work->bull->getMoveFrameToLift() == 1) {
        AreaSet(5);
    }
    if (r30f_work->bull->r_no_0 == 7 && r30f_work->bull->getMoveFrameToLift() == 0x96) {
        AreaSet(6);
    }
    if (r30f_work->bull->r_no_0 == 7 && r30f_work->bull->getMoveFrameToLift() == 1) {
        setem(9, 1);
        setem(0xA, 1);
        setem(0x28, 1);
        setem(0x2A, 1);
        em_destroy_area(5);
        em_destroy();
    }
    if (r30f_work->bull->r_no_0 == 7 && r30f_work->bull->getMoveFrameToLift() == 0x140) {
        r30f_work->em[0xA].setFlag(1);
    }
    if (r30f_work->bull->r_no_0 == 7 && r30f_work->bull->getMoveFrameToLift() == 0x1C2) {
        r30f_work->em[9].setFlag(1);
    }
    if (r30f_work->bull->r_no_0 == 7 && r30f_work->bull->getMoveFrameToLift() == 0x258) {
        setem(0x10, 1);
    }
    if (r30f_work->bull->r_no_0 == 7 && r30f_work->bull->getMoveFrameToLift() == 0x26C) {
        setem(0x11, 1);
        setem(0x17, 1);
    }
    if (r30f_work->bull->r_no_0 == 7 && r30f_work->bull->getMoveFrameToLift() == 0x294) {
        setem(0x27, 1);
        if (pG->Game_level > 5) {
            setem(0x20, 1);
        }
    }
    if (r30f_work->bull->r_no_0 == 8 && r30f_work->bull->getMoveFrameToLift() == 1) {
        r30f_work->em[0x28].setFlag(1);
        r30f_work->em[0x2A].setFlag(1);
    }
    if (r30f_work->bull->r_no_0 == 9 && r30f_work->bull->getMoveFrameToLift() == 1) {
        if (pG->Game_level > 8) {
            setem(0x2B, 1);
        }
        setem(0x2C, 1);
        if (pG->Game_level > 6) {
            setem(0x34, 1);
        }
        if (pG->Game_level > 4) {
            setem(0x33, 1);
        }
    }
    if (r30f_work->bull->r_no_0 == 9 && r30f_work->bull->getMoveFrameToLift() == 0xC8) {
        AreaSet(7);
    }
    if (pG->Game_level > 8) {
        if (r30f_work->bull->r_no_0 == 9 && r30f_work->bull->getMoveFrameToLift() == 0x46) {
            r30f_work->em[0x2B].setFlag(1);
        }
    }
    if (r30f_work->bull->r_no_0 == 9 && r30f_work->bull->getMoveFrameToLift() == 0xAA) {
        r30f_work->em[0x2C].setFlag(1);
    }
    if (r30f_work->bull->r_no_0 == 9 && r30f_work->bull->getMoveFrameToLift() == 0x6E && pG->Game_level > 6) {
        r30f_work->em[0x34].setFlag(1);
    }
    if (pG->Game_level > 4) {
        if (r30f_work->bull->r_no_0 == 9 && r30f_work->bull->getMoveFrameToLift() == 0xD2) {
            r30f_work->em[0x33].setFlag(1);
        }
    }
    if (r30f_work->bull->r_no_0 == 9) {
        r30f_work->bull->getMoveFrameToLift();
    }
    if (r30f_work->bull->r_no_0 == 9 && r30f_work->bull->getMoveFrameToLift() == 0x1F4 && pG->Game_level > 2) {
        setem(0x36, 1);
    }
    if (r30f_work->bull->r_no_0 == 4) {
        r30f_work->bull->getMoveFrameToLift();
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameToLift() == 0x301) {
        SceExec(0x12, (TaskFunc) track_move, 0, 0, 2, 0);
    }
    if (r30f_work->bull->r_no_0 == 4 && r30f_work->bull->getMoveFrameToLift() == 0x5B3) {
        SceExec(0x12, (TaskFunc) track_move, 0, 0, 2, 0);
    }
}

// Turns the lever (the model's first part) from `lo` to `hi` with an accelerating speed.
void reva_common_move(cObj* obj, f32 lo, f32 hi)
{
    f32 spd = 0.0f;
    f32* p;
    f32 acc;

    obj->be_flag |= 0x20;
    SndCall(6, 0x1E, &obj->pos, 0, 0, 0);
    p = &obj->pList->ang.z;
    acc = reva_rate;
    for (;;) {
        int up;

        if (hi > lo) {
            *p += spd;
            up = 1;
        } else {
            *p -= spd;
            up = 0;
        }
        if (spd >= 0.0f) {
            if (up ? (*p < hi) : (*p > hi)) {
                spd += acc * 1.85f;
            } else {
                *p = hi;
                return;
            }
        }
        SceSleep(1);
    }
}

// Rebuild a model's matrix (rotation, translation, scale) and its parts after a manual pose change.
void EmHitUpdate(cModel* m)
{
    RotMatrix(m->mat, &m->ang);
    TransMatrix(m->mat, &m->pos);
    ScaleMatrix(m->mat, &m->scale);
    m->partsMatCalc();
    m->partsWorldCalc();
}

// The truck explodes on the bulldozer: both riders take the blast.
void last_bomb()
{
    Vec p;

    pPL->dmg.set(0, 0x80);
    pSUB->dmg.set(0, 0x80);
    r30f_work->hit[0]->hp = 0;
    r30f_work->hit[1]->hp = 0;
    p = r30f_work->lift->pList->pList->world;
    p.y += 1000.0f;
    PlWepHitCheck2(0, &p, &p, 0x13, 2, 15500.0f);
    p.y += 2000.0f;
    PlWepHitCheck2(0, &p, &p, 0x13, 2, 15500.0f);
    r30f_work->hit[0]->hp = 1;
    r30f_work->hit[1]->hp = 1;
}

// Hit box `no` of the truck was shot.
void Hit(int no)
{
    cEmHit* em = r30f_work->hit[no];
    int near = 0;

    if (em->dmg.m_pDamageYarare->len < 64000000.0f) {
        near = 1;
    }
    r30f_work->truckLife -= GetWepDmVal(em, em->dmg.m_Wep, near);
    if (r30f_work->truckLife >= 0) {
        EmDmBloodSet2(r30f_work->hit[no], 1, 6, 0, 0, 0);
        EmDmBloodSet2(r30f_work->hit[no], 1, 0xD, 0, 0, 0);
    }
}

// The truck is destroyed: the camera cut of it falling.
static void track_destroy()
{
    SceEventStart(1);
    CamCtrl.CutCall(0x12);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM02, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM02, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM02, 0);
    if (pSUB) {
        pSUB->setNoSuspend(1);
    }
    r30f_work->lift->setNoSuspend(1);
    r30f_work->bull->setNoSuspend(1);
    EstSet(r30f_work->lift, -1, 0, 0, EFF_ROOM, 0xC, 0x801, ESP_CORE_KIND_NONE, 0, 0);
    SceSleep((u32) MotionGetMaxFrame(&r30f_work->lift->Motion));
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// 80 frames later Leon takes the guard hit (PlSetDamage type 9): the truck's ramming.
static void pl_gurd()
{
    SceSleep(0x50);
    PlSetDamage(PL_DM_AUTO_SML, 0, 0);
}

// The truck (the lift object's motions) drives at the bulldozer; three attacks, the last one
// explodes it.
static void track_move()
{
    u32 frames;
    u32 t;
    u32 hitT;
    int life;
    Vec wp;
    Vec ang;

    SceSleep(1);
    r30f_work->lift->be_flag |= 2;
    if (r30f_work->truckNo == 1) {
        SceEventStart(1);
        r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x41), 0, 0, 0x200, 0);
        r30f_work->lift->setNoSuspend(1);
        EstSet(r30f_work->lift, -1, 0, 0, EFF_ROOM, 0x15, 1, ESP_CORE_KIND_ROOM04, 0, 0);
        SndCall(6, 0x12, &r30f_work->lift->pList->pList->world, 0, 0x80000000, 0);
        SceSleep((u32) MotionGetMaxFrame(&r30f_work->lift->Motion));
        EffectEspDelete(1, ESP_CORE_KIND_ROOM04, 0, 0);
        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM04, 0);
        EffectEfmDelete(1, ESP_CORE_KIND_ROOM04, 0);
        SceEventEnd(0);
    }
    r30f_work->lift->setNoSuspend(0);
    r30f_work->lift->Motion.Seq_speed = 1.0f;
    if (r30f_work->truckNo == 0) {
        r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x3B), 0, 0, 0x200, 0);
    }
    if (r30f_work->truckNo == 1) {
        r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x3E), 0, 0, 0x200, 0);
    }
    if (r30f_work->truckNo == 2) {
        r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2E), 0, 0, 0x200, 0);
    }
    EstSet(r30f_work->lift, -1, 0, 0, EFF_ROOM, 5, 0, ESP_CORE_KIND_ROOM00, 0, 0);
    SndCall(6, 0x16, &r30f_work->lift->pList->pList->world, 0, 0, 0);
    if (r30f_work->truckNo != 1) {
        SndCall(6, 0x12, &r30f_work->lift->pList->pList->world, 0, 0x80000000, 0);
    }
    frames = (u32) MotionGetMaxFrame(&r30f_work->lift->Motion);
    r30f_work->hit[0] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &r30f_work->lift->pos, &r30f_work->lift->ang, 1);
    YarareInitCube(r30f_work->hit[0], hit0_x, hit0_y, hit0_z, hit0_w, hit0_h, hit0_d, 0, YAT_FLAG_ON);
    r30f_work->hit[1] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &r30f_work->lift->pos, &r30f_work->lift->ang, 1);
    YarareInitCube(r30f_work->hit[1], hit1_x, hit1_y, hit1_z, hit1_w, hit1_h, hit1_d, 0, YAT_FLAG_ON);
    r30f_work->truckLife = 0x1F4;
    if (pG->Game_level > 8) {
        r30f_work->truckLife += r30f_work->truckLife;
    }
    t = 0;
    hitT = 0;
    for (;;) {
        SceDebugDisp("HP[%d]", r30f_work->truckLife);
        if (r30f_work->truckNo != 2 && t < frames) {
            r30f_work->hit[0]->hp = 0;
            r30f_work->hit[1]->hp = 0;
            wp = r30f_work->lift->pList->pList->world;
            wp.y += 1000.0f;
            PlWepHitCheck2(0, &wp, &wp, 0x13, 2, 5500.0f);
            wp.y += 2000.0f;
            PlWepHitCheck2(0, &wp, &wp, 0x13, 2, 5500.0f);
            r30f_work->hit[0]->hp = 1;
            r30f_work->hit[1]->hp = 1;
        }
        if (t == 0x5A) {
            SndCall(6, 0x16, &r30f_work->lift->pList->pList->world, 0, 0, 0);
        }
        if (t == 0xB4) {
            SndCall(6, 0x16, &r30f_work->lift->pList->pList->world, 0, 0, 0);
        }
        wp = r30f_work->lift->pList->pList->world;
        {
            Vec p = {0.0f, 0.0f, 0.0f};
            Vec fwd = {0.0f, 0.0f, 1.0f};

            PSMTXMultVec(r30f_work->lift->pList->pList->mat, &fwd, &fwd);
            PSMTXMultVec(r30f_work->lift->pList->pList->mat, &p, &p);
            ang.z = 0.0f;
            ang.x = 0.0f;
            ang.y = GetXZAngle(&p, &fwd);
        }
        r30f_work->hit[0]->pos = wp;
        r30f_work->hit[0]->ang = ang;
        EmHitUpdate(r30f_work->hit[0]);
        r30f_work->hit[1]->pos = wp;
        r30f_work->hit[1]->ang = ang;
        EmHitUpdate(r30f_work->hit[1]);
        life = r30f_work->truckLife;
        if (t < frames) {
            if (r30f_work->hit[0]->ckStatus() == 1) {
                Hit(0);
            } else if (r30f_work->hit[1]->ckStatus() == 1) {
                Hit(1);
            }
        }
        if (life > 0 && r30f_work->truckLife <= 0) {
            if (r30f_work->truckNo == 2) {
                EstSet(r30f_work->lift, -1, 0, 0, EFF_ROOM, 0x19, 1, ESP_CORE_KIND_ROOM02, 0, 0);
            } else {
                EstSet(r30f_work->lift, -1, 0, 0, EFF_ROOM, 7, 1, ESP_CORE_KIND_ROOM02, 0, 0);
            }
            SndCall(6, 0x13, &r30f_work->lift->pList->pList->world, 0, 0x80000000, 0);
        }
        if (t == frames) {
            EffectEspDelete(0, ESP_CORE_KIND_ROOM00, 0, 0);
            EffectEspgenDelete(0, ESP_CORE_KIND_ROOM00, 0);
            EffectEfmDelete(0, ESP_CORE_KIND_ROOM00, 0);
            if (r30f_work->truckLife <= 0) {
                int no = r30f_work->truckNo;

                if (no == 0) {
                    r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x3C), 0, 0, 1, 0);
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0x18, 0, ESP_CORE_KIND_NONE, (void*) no, (void*) no);
                }
                if (r30f_work->truckNo == 1) {
                    r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x3F), 0, 0, 1, 0);
                    SceExec(0x12, (TaskFunc) track_destroy, 0, 0, 2, 0);
                }
                if (r30f_work->truckNo == 2) {
                    r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2F), 0, 0, 0x200, 0);
                    EstSet(r30f_work->lift, -1, 0, 0, EFF_ROOM, 0x14, 0, ESP_CORE_KIND_NONE, r30f_work->lift, 0);
                    last_bomb();
                    SceExec(0x12, (TaskFunc) pl_gurd, 0, 0, 2, 0);
                }
                if (r30f_work->truckNo == 0) {
                    SndCall(6, 0x14, &r30f_work->lift->pList->pList->world, 0, 0x80000000, 0);
                } else {
                    SndCall(6, 0x15, &r30f_work->lift->pList->pList->world, 0, 0x80000000, 0);
                }
            } else {
                KeyStop(0xEFCF0000);
                if (r30f_work->truckNo == 0) {
                    CamCtrl.CutCall(8);
                }
                if (r30f_work->truckNo == 1) {
                    CamCtrl.CutCall(9);
                }
                if (r30f_work->truckNo == 2) {
                    CamCtrl.CutCall(0xA);
                }
                hitT = 0;
                RmfFlagOn(pG, RMF_TRACK_DIE);
                if (r30f_work->truckNo == 0) {
                    r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x3D), 0, 0, 1, 0);
                    EstSet(0, -1, 0, 0, EFF_ROOM, 8, 0, ESP_CORE_KIND_NONE, (void*) hitT, (void*) hitT);
                    EstSet(r30f_work->lift, -1, 0, 0, EFF_ROOM, 9, 0, ESP_CORE_KIND_NONE, r30f_work->lift, (void*) hitT);
                }
                if (r30f_work->truckNo == 1) {
                    r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x40), 0, 0, 1, 0);
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 0, ESP_CORE_KIND_NONE, (void*) hitT, (void*) hitT);
                    EstSet(r30f_work->lift, -1, 0, 0, EFF_ROOM, 0xB, 0, ESP_CORE_KIND_NONE, r30f_work->lift, (void*) hitT);
                }
                if (r30f_work->truckNo == 2) {
                    r30f_work->lift->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x37), 0, 0, 1, 0);
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0x10, 0, ESP_CORE_KIND_NONE, (void*) hitT, (void*) hitT);
                }
                SndCall(6, 0x15, &r30f_work->lift->pList->pList->world, 0, 0x80000000, 0);
            }
            r30f_work->truckNo++;
        }
        t++;
        hitT++;
        life = 0;   // the zero register of the pl_life store and the EstSet arguments below
        if (RmfFlagChk(pG, RMF_TRACK_DIE) && hitT == 0x1E) {
            PlWepHitCheck2(0, &pPL->pos, &pPL->pos, 0x12, 2, 6000.0f);
            pG->pl_life = 0;
            PlSetDamage(PL_DM_BACK, 0, 0);
            EstSet(0, -1, &pPL->pos, 0, EFF_CORE, 0x27, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
            DiedemoExec(0, 0);
        }
        SceSleep(1);
    }
}

// Carries the riders and the lift objects along with the bulldozer while it moves on the lift.
static void adjust_func(cObj* obj)
{
    Vec zero = {0.0f, 0.0f, 0.0f};

    adjust_add_set(zero);
    if (!RmfFlagChk(pG, RMF_LIFT_END)) {
        SmdGetObjPtr(0x1E)->be_flag |= 0x20;
        PSVECSubtract(&r30f_work->bull->pList->pos, &r30f_work->bullPos, &r30f_work->liftAdd);
        setLiftMoveAdd(&r30f_work->liftAdd);
        r30f_work->liftAdd.y *= lift_y_rate;
        adjust_add_set(r30f_work->liftAdd);
        r30f_work->bullPos = r30f_work->bull->pList->pos;
    }
}

// The ride: Leon and Ashley climb on, then the bulldozer's routines drive the events.
static void R30f_ride()
{
    cPlayer* pl = pPL;
    int truck;
    int adjust;

    ScfFlagOn(pG, SCF_NO_ASHLEY_DIST_CK);
    if (!(R30F_SAVE_FLAGS & 0x40000000)) {
        SceEventStart(0);
        SndStrReq(1, 0xE6, 0x80000003, 0, 0, 0.0f);
        r30f_work->bull->setNoSuspend(1);
        pl->setRightHand(1);
        pl->Wep->setTrans(0, 0);
        PlSetHand(1, 0);
        SubCharCtrl(5, 0);
        {
            Vec p;
            Vec ang;
            cPlayer* pl2;
            cSubChar* sub;

            p = r30f_work->bull->pList->pos;
            Matrix2AxisAngle(r30f_work->bull->pList->pList->mat, &ang);
            Vec ofs = {0.0f, 0.0f, -4600.4004f};
            Mtx m;
            PSMTXRotRad(m, 'y', ang.y);
            PSMTXMultVec(m, &ofs, &ofs);
            PSVECAdd(&p, &ofs, &ofs);
            pl2 = pPL;
            pl2->setPos(&ofs);
            pl2->setAng(&ang);
            sub = SUB_CHAR();
            if (sub) {
                sub->setPos(&p);
                sub->setAng(&ang);
            }
        }
        pPL->setNoSuspend(1);
        if (pSUB) {
            pSUB->setNoSuspend(1);
        }
        MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x27), 0, 0, 0x201, 0);
        if (pSUB) {
            MotionSetCore(pSUB, &pSUB->Motion, ROOM_ARC_PTR(pG->pRoom, 0x28), 0, 0, 0x201, 0);
        }
        SceSleep((u32) MotionGetMaxFrame(&pPL->Motion));
        pPL->setNoSuspend(0);
        if (pSUB) {
            pSUB->setNoSuspend(0);
        }
        r30f_work->bull->setNoSuspend(0);
        SceEventEnd(0);
        PlSetHand(0, 0);
        pl->setRightHand(1);
        pl->Wep->setTrans(1, 0);
    } else {
        SceEventStart(0);
        SceSleep(1);
        CamCtrl.Comeback(0x40);
        SceEventEnd(0);
    }
    RmfFlagOn(pG, RMF_PL_RIDE);
    r30f_work->bull->setRide();
    r30f_work->bull->setSubBullDrive();
    SceAtSetEnable(0x18, 0);
    SceAtSetEnable(0x19, 0);
    SceAtSetEnable(0x1A, 0);
    // The inits sit right before the loop: a call as the insn before LOOP_BEG makes loop.c insert a
    // `(use 0)` that takes an issue slot in the block's schedule (the `lwz flags` would move up a slot).
    truck = 0;
    adjust = 0;
    for (;;) {
        if (r30f_work->bull->r_no_0 == 1 && r30f_work->bull->getMoveFrameToLift() == 1 && pSUB) {
            SndCall(6, 0x1F, &pSUB->pos, 0, 0, 0);
        }
        if (r30f_work->bull->r_no_0 == 3 && r30f_work->bull->getMoveFrameToLift() == 1 && pSUB) {
            SndCall(6, 0x1F, &pSUB->pos, 0, 0, 0);
        }
        if (r30f_work->bull->r_no_0 == 3 && r30f_work->bull->getMoveFrameToLift() == 0x35) {
            if (r30f_work->shake1 <= 1) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 3, 0, ESP_CORE_KIND_NONE, 0, 0);
                if (pSUB) {
                    SndCall(6, 0x21, &pSUB->pos, 0, 0, 0);
                }
            }
            r30f_work->shake1++;
        }
        if (r30f_work->bull->r_no_0 == 8 && r30f_work->bull->getMoveFrameToLift() == 1 && pSUB) {
            SndCall(6, 0x1F, &pSUB->pos, 0, 0, 0);
        }
        if (r30f_work->bull->r_no_0 == 0xA && r30f_work->bull->getMoveFrameToLift() == 1 && pSUB) {
            SndCall(6, 0x1F, &pSUB->pos, 0, 0, 0);
        }
        if (r30f_work->bull->r_no_0 == 0xA && r30f_work->bull->getMoveFrameToLift() == 0x35) {
            if (r30f_work->shake2 <= 1) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 0x12, 0, ESP_CORE_KIND_NONE, 0, 0);
                if (pSUB) {
                    SndCall(6, 0x21, &pSUB->pos, 0, 0, 0);
                }
            }
            r30f_work->shake2++;
        }
        if (r30f_work->truckLife <= 0) {
            if (r30f_work->bull->r_no_0 == 0xB && r30f_work->bull->getMoveFrameToLift() == 0xDC && pSUB) {
                SndCall(6, 0x1F, &pSUB->pos, 0, 0, 0);
            }
            if (r30f_work->bull->r_no_0 == 0xB && r30f_work->bull->getMoveFrameToLift() == 0xF5) {
                SndStrReq(1, 0xEE, 0x80000003, 0, 0, 0.0f);
            }
        }
        if (r30f_work->bull->r_no_0 == 0xB && r30f_work->bull->getMoveFrameToLift() == 0x140) {
            SceAtExecute(0);
        }
        if (r30f_work->bull->ckBreak1st() != 0 && !RmfFlagChk(pG, RMF_DOOR1_BREAK)) {
            RmfFlagOn(pG, RMF_DOOR1_BREAK);
            SceExec(0x12, (TaskFunc) door1_break, 0, 0, 2, 0);
        }
        if (r30f_work->bull->ckBreak2nd() != 0 && !RmfFlagChk(pG, RMF_DOOR2_BREAK)) {
            RmfFlagOn(pG, RMF_DOOR2_BREAK);
            SceExec(0x12, (TaskFunc) door2_break, 0, 0, 2, 0);
        }
        if (r30f_work->bull->ckBreak3rd() != 0 && !RmfFlagChk(pG, RMF_DOOR3_BREAK)) {
            RmfFlagOn(pG, RMF_DOOR3_BREAK);
            SceExec(0x12, (TaskFunc) door3_break, 0, 0, 2, 0);
        }
        if (r30f_work->bull->ckBreak4th() != 0 && !RmfFlagChk(pG, RMF_DOOR4_BREAK)) {
            RmfFlagOn(pG, RMF_DOOR4_BREAK);
            SceExec(0x12, (TaskFunc) door4_break, 0, 0, 2, 0);
        }
        if (r30f_work->bull->ckLift() != 0) {
            r30f_work->bull->setAdjustMode(0, adjust_func);
            if (adjust == 0) {
                adjust = 1;
                r30f_work->bullPos = r30f_work->bull->pList->pos;
                SceAtSetEnable(0x14, 1);
                SceAtSetEnable(0x13, 1);
            }
        } else {
            r30f_work->bull->setAdjustMode(1, 0);
            Vec zero = {0.0f, 0.0f, 0.0f};
            adjust_add_set(zero);
        }
        if (r30f_work->bull->ckTruckGo() != 0) {
            if (truck == 0) {
                truck = 1;
                SceExec(0x12, (TaskFunc) track_move, 0, 4, 2, 0);
            }
            if (r30f_work->truckLife <= 0) {
                r30f_work->bull->setBreakTruck();
            }
        }
        SceSleep(1);
    }
}

// The player's ride motion as a damage routine (R30f_ride2: back on the bulldozer after the lift).
static void plemRide(cPlayer* p)
{
    switch (p->r_no_2) {
    case 0:
        pPL->dmg.set(0, 0x80);
        pPL->atari.offSca();
        pPL->atari.setPriority(2);
        MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x27), 0, 0, 0x201, 0);
        p->r_no_3 = 0;
        p->r_no_2++;
    case 1:
        p->r_no_3++;
        if (MotionMove(p, 0) != 0 || p->r_no_3 == 0x3C) {
            pPL->dmg.clear();
            pPL->atari.onSca();
            pPL->atari.setPriority(0);
            EndPlDamage();
            p->dmg.set(0, 0x1E);
            RmfFlagOn(pG, RMF_PL_RIDE);
        }
        break;
    }
}

// Put Leon back on the bulldozer's cab position (part offset rotated by 1.572 rad) after the lift ride,
// with the ride motion routine.
static void R30f_ride2()
{
    Vec p;
    Vec ang;
    cPlayer* pl;

    p = r30f_work->bull->pList->pos;
    ang.z = 0.0f;
    ang.y = 1.572f;
    ang.x = 0.0f;
    Vec ofs = {0.0f, 0.0f, -4600.4004f};
    Mtx m;
    PSMTXRotRad(m, 'y', ang.y);
    PSMTXMultVec(m, &ofs, &ofs);
    PSVECAdd(&p, &ofs, &ofs);
    pl = pPL;
    ofs.z += pl->pos.z - p.z;
    pl->setPos(&ofs);
    pl->setAng(&ang);
    pPL->dmg.m_Timer = 2;
    SetPlDamage(0, plemRide);
}

// Gate 1 (scroll object 0x1B) falls over.
static void door1_break()
{
    cObj* o;
    u32 i;

    o = SmdGetObjPtr(0x1B);
    SceSleep(0x32);
    EstSet(0, -1, 0, 0, EFF_ROOM, 2, 0, ESP_CORE_KIND_NONE, 0, 0);
    o->be_flag |= 0x20;
    o->pos.x = -38892.9f;
    o->pos.y = -31964.8f;
    o->pos.z = 108930.0f;
    SndCall(6, 0x20, &o->pos, 0, 0, 0);
    SceAtSetEnable(0x1A, 0);
    {
        Vec add = {200.0f, 0.0f, 0.0f};
        f32 spd = 0.0f;

        for (i = 0; i < 12; i++) {
            PSVECAdd(&o->pos, &add, &o->pos);
            add.y -= 43.0f;
            o->ang.z -= spd;
            if (o->ang.z < -1.657f) {
                o->ang.z = -1.657f;
            }
            spd += 0.03f;
            SceSleep(1);
        }
    }
}

// Gate 2 (0x1C).
static void door2_break()
{
    cObj* o;
    u32 i;

    o = SmdGetObjPtr(0x1C);
    SceSleep(0x32);
    EstSet(0, -1, 0, 0, EFF_ROOM, 4, 0, ESP_CORE_KIND_NONE, 0, 0);
    o->be_flag |= 0x20;
    o->pos.x = -10455.8f;
    o->pos.y = -38089.5f;
    o->pos.z = 76813.7f;
    o->ang.x = 1.5707964f;
    o->ang.y = 1.5707964f;
    o->ang.z = 1.5707964f;
    SndCall(6, 0x20, &o->pos, 0, 0, 0);
    SceAtSetEnable(0x1B, 0);
    {
        Vec add = {0.0f, 0.0f, -200.0f};
        f32 spd = 0.0f;

        for (i = 0; i < 12; i++) {
            PSVECAdd(&o->pos, &add, &o->pos);
            add.y -= 40.0f;
            o->ang.y += spd;
            if (o->ang.y > 3.1415927f) {
                o->ang.y = 3.1415927f;
            }
            spd += 0.03f;
            SceSleep(1);
        }
    }
}

// Gate 3 (0x1D) is blown open.
static void door3_break()
{
    SceSleep(0x32);
    SmdSetTrans(0x1D, 0);
    if (pSUB) {
        SndCall(6, 0x20, &pSUB->pos, 0, 0, 0);
    }
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xF, 0, ESP_CORE_KIND_NONE, 0, 0);
    SceAtSetEnable(0x1C, 0);
    Vec p = {40378.0f, -3424.0f, -56312.0f};
    PlWepHitCheck2(0, &p, &p, 0x12, 2, 7000.0f);
}

// Gate 4 (0x1A): Ashley points, the gate falls, the message.
static void door4_break()
{
    cObj* o;
    u32 i;

    o = SmdGetObjPtr(0x1A);
    SceSleep(0xA);
    SndCall(6, 0x16, &pSUB->pos, 0, 0, 0);
    SceSleep(0x28);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
    o->be_flag |= 0x20;
    o->pos.x = 3744.86f;
    o->pos.y = -6478.23f;
    o->pos.z = -56759.2f;
    o->ang.x = 1.5707964f;
    o->ang.y = 1.5707964f;
    o->ang.z = 1.5707964f;
    SceAtSetEnable(0x1D, 0);
    SndCall(6, 0x20, &o->pos, 0, 0, 0);
    {
        Vec add = {0.0f, 0.0f, -200.0f};
        f32 spd = 0.0f;

        for (i = 0; i < 12; i++) {
            PSVECAdd(&o->pos, &add, &o->pos);
            add.y -= 40.0f;
            o->ang.y += spd;
            if (o->ang.y > 3.1415927f) {
                o->ang.y = 3.1415927f;
            }
            SceSleep(1);
            spd += 0.03f;
        }
    }
    if (pSUB) {
        SndCall(6, 4, &pSUB->pos, 0, 0, 0);
    }
    r30f_work->bull->setSubBullFinger();
    cMes.MesSet(6, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 0x02000052, 0, 0, 4);
    SceSleep(0xF);
    SndCall(6, 0x16, &pSUB->pos, 0, 0, 0);
}

// The scroll blocks shown per area.
void setArea1()
{
    SmdSetTrans(0x1F, 1);
    SmdSetTrans(0x20, 1);
    SmdSetTrans(0x21, 0);
    SmdSetTrans(0x22, 0);
    SmdSetTrans(0x23, 0);
    SmdSetTrans(0x24, 0);
}

// Scroll block layout for section 2 (objects 0x20/0x21 shown).
void setArea2()
{
    SmdSetTrans(0x1F, 0);
    SmdSetTrans(0x20, 1);
    SmdSetTrans(0x21, 1);
    SmdSetTrans(0x22, 0);
    SmdSetTrans(0x23, 0);
    SmdSetTrans(0x24, 0);
}

// Scroll block layout for section 3 (objects 0x21..0x23 shown).
void setArea3()
{
    SmdSetTrans(0x1F, 0);
    SmdSetTrans(0x20, 0);
    SmdSetTrans(0x21, 1);
    SmdSetTrans(0x22, 1);
    SmdSetTrans(0x23, 1);
    SmdSetTrans(0x24, 0);
}

// Scroll block layout for section 4 (objects 0x22..0x24 shown).
void setArea4()
{
    SmdSetTrans(0x1F, 0);
    SmdSetTrans(0x20, 0);
    SmdSetTrans(0x21, 0);
    SmdSetTrans(0x22, 1);
    SmdSetTrans(0x23, 1);
    SmdSetTrans(0x24, 1);
}

// Scroll block layout for section 5 (objects 0x23/0x24 shown).
void setArea5()
{
    SmdSetTrans(0x1F, 0);
    SmdSetTrans(0x20, 0);
    SmdSetTrans(0x21, 0);
    SmdSetTrans(0x22, 0);
    SmdSetTrans(0x23, 1);
    SmdSetTrans(0x24, 1);
}

// Switch to section `no` of the ride: the scroll block layout and the collision pieces (created /
// destroyed as the bulldozer advances).
void AreaSet(u32 no)
{
    switch (no) {
    case 1:
        setArea1();
        r30f_work->sat2 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x5), 0, (Vec*) &vecZero, (Vec*) &vecZero, 3);
        break;
    case 2:
        setArea2();
        break;
    case 3:
        SatMgr.destroy(r30f_work->sat1);
        break;
    case 4:
        setArea3();
        r30f_work->sat3 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x5), 0, (Vec*) &vecZero, (Vec*) &vecZero, 4);
        break;
    case 5:
        SatMgr.destroy(r30f_work->sat2);
        r30f_work->sat4 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x5), 0, (Vec*) &vecZero, (Vec*) &vecZero, 5);
        break;
    case 6:
        setArea4();
        SceAtSetEnable(0xE, 0);
        SceAtSetEnable(0x14, 0);
        SceAtSetEnable(0x13, 0);
        break;
    case 7:
        setArea5();
        break;
    }
}

// Removes the live list enemies standing in area `at`.
void em_destroy_area(int at)
{
    SceAtWork* w = SceAtPtr(at);
    u32 i;

    for (i = 0; i < 90; i++) {
        if (r30f_work->em[i].isAlive() == 1 && AreaHitCheck(&w->area, &r30f_work->em[i].getPtr()->pos) != 0) {
            r30f_work->em[i].destroy();
        }
    }
}

// Destroy every live list Ganado that may be reset (out of sight and idle).
void em_destroy()
{
    u32 i;

    for (i = 0; i < 90; i++) {
        if (r30f_work->em[i].isAlive() == 1 && r30f_work->em[i].ckResetEnable() != 0) {
            r30f_work->em[i].destroy();
        }
    }
}

// The lift arrives: the camera cut with Ashley pointing at the lever.
void lift_stop_event()
{
    SceEventStart(1);
    CamCtrl.CutCall(0x10);
    EstSet(r30f_work->bull, -1, 0, 0, EFF_ROOM, 0x16, 1, ESP_CORE_KIND_ROOM03, 0, 0);
    if (pSUB) {
        SndCall(6, 1, &pSUB->pos, 0, 0, 0);
        pSUB->setNoSuspend(1);
    }
    r30f_work->bull->setNoSuspend(1);
    r30f_work->bull->setSubBullFinger();
    cMes.MesSet(3, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 0x02000052, 0, 0, 4);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(0x13);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    r30f_work->bull->setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// On the lift: the save, the lever object, the lift's descent and the area switches until the gate opens.
static void lift_stop_task()
{
    cObj* o;
    int cnt;
    int eff;

    if (R30F_SAVE_FLAGS & 0x40000000) {
        SceDestroyEm(0x10, 0x20);
        pPL->ang.y -= 1.57f;
        CamCtrl.Comeback(0);
    }
    cnt = 0;
    eff = 0;
    R30F_SAVE_FLAGS |= 0x40000000;
    GameSave.save(pSaveData, -1);
    SmdGetObjPtr(0x12)->be_flag |= 0x20;
    SmdGetObjPtr(0x12)->pList->ang.z = 1.38f;
    o = SmdGetObjPtr(0xD);
    o->be_flag |= 0x20;
    SndCall(6, 0x1C, &o->pos, 0, 0, 0);
    SceAtSetEnable(0xE, 1);
    SceAtSetEnable(0x10, 1);
    SceAtSetEnable(0x11, 1);
    SceAtSetEnable(0x12, 1);
    SceAtDataSet_exec(0xC, 0x12, 0, (TaskFunc) R30f_ride2, 0, 1);
    while (!RmfFlagChk(pG, RMF_GATE_OPEN_START)) {
        if (cnt++ == 0x3B) {
            lift_stop_event();
        }
        if (o->pos.y > -36243.0f) {
            o->pos.y -= 150.0f;
        } else {
            if (eff == 0) {
                // `eff = 1` before the EstSet: cse canonicalises the stack zeros to the class member with the
                // latest last mention, which must not be `eff`.
                eff = 1;
                EstSet(0, -1, 0, 0, EFF_ROOM, 0xE, 0, ESP_CORE_KIND_NONE, 0, 0);
                SndCall(6, 0x1D, &o->pos, 0, 0, 0);
                EffectEspDelete(1, ESP_CORE_KIND_ROOM02, 0, 0);
                EffectEspgenDelete(1, ESP_CORE_KIND_ROOM02, 0);
                EffectEfmDelete(1, ESP_CORE_KIND_ROOM02, 0);
                r30f_work->lift->be_flag &= ~2;
            }
            o->pos.y = -36243.0f;
            SceAtSetEnable(0xD, 1);
        }
        if (r30f_work->bull->r_no_0 == 6 && (u32) r30f_work->bull->getMoveFrameToLift() > 0x226) {
            SceAtSetEnable(6, 0);
            SceAtSetEnable(0xC, 1);
        } else if (RmfFlagChk(pG, RMF_FALL)) {
            RmfFlagOff(pG, RMF_PL_RIDE);
            SceAtSetEnable(6, 0);
            SceAtSetEnable(0xC, 1);
        } else {
            SceAtSetEnable(6, 1);
            SceAtSetEnable(0xC, 0);
        }
        SceSleep(1);
    }
}

// The Ganados jump onto the bulldozer: the cut with Ashley looking back.
static void em_set()
{
    u32 i = 0;
    cEm* e;

    e = setem(0x50, 1);
    if (e) {
        e->setNoSuspend(1);
    }
    e = setem(0x51, 1);
    if (e) {
        e->setNoSuspend(1);
    }
    e = setem(0x54, 1);
    if (e) {
        e->setNoSuspend(1);
    }
    e = setem(0x55, 1);
    if (e) {
        e->setNoSuspend(1);
    }
    e = setem(0x13, 1);
    if (e) {
        e->setNoSuspend(1);
    }
    e = setem(0x12, 1);
    if (e) {
        e->setNoSuspend(1);
    }
    SceSleep(0xA);
    SceEventStart(0);
    r30f_work->bull->setNoSuspend(1);
    if (pSUB) {
        pSUB->setNoSuspend(1);
    }
    CamCtrl.CutCall(0x14);
    if (pSUB) {
        SndCall(6, 3, &pSUB->pos, 0, 0, 0);
    }
    r30f_work->bull->setSubBullLookBack();
    cMes.MesSet(5, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 0x02000052, 0, 0, 4);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(5);
    while (i < 100) {
        SceSleep(1);
        i++;
    }
    r30f_work->em[0x50].setNoSuspend(0);
    r30f_work->em[0x51].setNoSuspend(0);
    r30f_work->em[0x54].setNoSuspend(0);
    r30f_work->em[0x55].setNoSuspend(0);
    r30f_work->em[0x12].setNoSuspend(0);
    r30f_work->em[0x13].setNoSuspend(0);
    r30f_work->bull->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The lift starts: the camera follows the platform down to the lever.
static void lift_start_task()
{
    SceSleep(1);
    SceEventStart(0);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    r30f_work->bull->setNoSuspend(0);
    int se = 0;
    SmdGetObjPtr(0x1E)->be_flag |= 0x20;
    Vec a = {31033.0f, -33039.0f, -24540.0f};
    Vec b = {28406.0f, -40949.0f, -24540.0f};
    f32 t = 0.0f;
    CamCtrl.CutCall(6);
    SndCall(6, 0xC, 0, 0, 0, 0);
    SndCall(6, 0xD, 0, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        if (t < 1.0f) {
            Vec s;
            Vec u;

            t += 0.0125f;
            PSVECScale(&a, &s, 1.0f - t);
            PSVECScale(&b, &u, t);
            PSVECAdd(&s, &u, &SmdGetObjPtr(0x1E)->pos);
        } else {
            t = 1.0f;
            if (se == 0) {
                SndCall(6, 0xE, 0, 0, 0, 0);
                se = 1;
                SndCall(6, 0xF, 0, 0, 0, 0);
            }
        }
        SceSleep(1);
    }
    SmdGetObjPtr(0x1E)->pos = b;
    CamCtrl.CutCall(7);
    reva_common_move(SmdGetObjPtr(0x12), 0.0f, 1.38f);
    SceSleep(3);
    SndCall(6, 0x22, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
    EffectEspDelete(0x801, ESP_CORE_KIND_ROOM01, 0, 0);
    EffectEspgenDelete(0x801, ESP_CORE_KIND_ROOM01, 0);
    EffectEfmDelete(0x801, ESP_CORE_KIND_ROOM01, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 1, 0x801, ESP_CORE_KIND_ROOM01, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    r30f_work->bull->setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The lever at the top: turned back, the lift goes down again.
static void r30f_switch()
{
    SceEventStart(1);
    CamCtrl.CutCall(0x11);
    SceSleep(1);
    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    if (SceMesGetSelection() == 1) {
        reva_common_move(SmdGetObjPtr(0x12), 1.38f, 0.0f);
        EffectEspDelete(0x801, ESP_CORE_KIND_ROOM01, 0, 0);
        EffectEspgenDelete(0x801, ESP_CORE_KIND_ROOM01, 0);
        EffectEfmDelete(0x801, ESP_CORE_KIND_ROOM01, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x801, ESP_CORE_KIND_ROOM01, 0, 0);
        SndCall(6, 0x22, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
        SceSleep(0xF);
        SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        R30F_SAVE_FLAGS |= 0x80000000;
        SceAtSetEnable(0xA, 0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Move a model by `add` (setPos of pos + add).
void addPos(Vec* add, cModel* m)
{
    Vec p;

    p = m->pos;
    PSVECAdd(&p, add, &p);
    m->setPos(&p);
}

// Moves a 4-point area with the lift.
void addPos_sca(Vec* add, SceAtWork* at)
{
    at->area.u.xz4.p[0].x += add->x;
    at->area.u.xz4.p[0].z += add->z;
    at->area.u.xz4.p[1].x += add->x;
    at->area.u.xz4.p[1].z += add->z;
    at->area.u.xz4.p[2].x += add->x;
    at->area.u.xz4.p[2].z += add->z;
    at->area.u.xz4.p[3].x += add->x;
    at->area.u.xz4.p[3].z += add->z;
    at->area.u.xz4.floor += add->y;
}

// Everything on the lift moves with it: the riders, the camera, the enemies, the scenario pieces and
// the areas.
void setLiftMoveAdd(Vec* add)
{
    Vec v;
    u32 i;
    // One at/s1/s2/c for all nine SceAt blocks: function-scope pseudos, so s1 is the first
    // callee-saved allocation (r31) and the loop offset / &v take r31 in global.c pass 0.
    SceAtWork* at;
    cSat* s1;
    cSat* s2;
    Vec c;

    addPos(add, pPL);
    v = *add;
    if (CamCtrl.m_pExtraCamera != 0) {
        PSVECAdd(&CamCtrl.m_pExtraCamera->param.at, &v, &CamCtrl.m_pExtraCamera->param.at);
        PSVECAdd(&CamCtrl.m_pExtraCamera->param.pos, &v, &CamCtrl.m_pExtraCamera->param.pos);
    }
    pG->quake_ofs = v;
    // Struct-member view of pSUB: its load is not hoisted above the quake_ofs copy (the pG trick).
    if (pSUB) {
        addPos(add, pSUB);
    }
    addPos(add, SmdGetObjPtr(0x1E));
    for (i = 0; i < 90; i++) {
        if (r30f_work->em[i].isAlive() == 1 && (r30f_work->em[i].getPtr()->be_flag & 2)) {
            addPos(add, r30f_work->em[i].getPtr());
        }
    }
    at = SceAtPtr(7);
    s1 = at->scr.pSat;
    s2 = at->scr.pEat;
    if (s1) {
        c.x = s1->mat[0][3];
        c.y = at->scr.pSat->mat[1][3];
        c.z = at->scr.pSat->mat[2][3];
        PSVECAdd(&c, add, &c);
        s1->setCoord(&c, (Vec*) &vecZero);
        if (s2) {
            s2->setCoord(&c, (Vec*) &vecZero);
        }
    }
    at = SceAtPtr(0xE);
    s1 = at->scr.pSat;
    if (s1) {
        c.x = s1->mat[0][3];
        c.y = at->scr.pSat->mat[1][3];
        c.z = at->scr.pSat->mat[2][3];
        PSVECAdd(&c, add, &c);
        s1->setCoord(&c, (Vec*) &vecZero);
    }
    at = SceAtPtr(0x10);
    s1 = at->scr.pSat;
    if (s1) {
        c.x = s1->mat[0][3];
        c.y = at->scr.pSat->mat[1][3];
        c.z = at->scr.pSat->mat[2][3];
        PSVECAdd(&c, add, &c);
        s1->setCoord(&c, (Vec*) &vecZero);
    }
    at = SceAtPtr(0x13);
    s1 = at->scr.pSat;
    if (s1) {
        c.x = s1->mat[0][3];
        c.y = at->scr.pSat->mat[1][3];
        c.z = at->scr.pSat->mat[2][3];
        PSVECAdd(&c, add, &c);
        s1->setCoord(&c, (Vec*) &vecZero);
    }
    at = SceAtPtr(0x14);
    s1 = at->scr.pSat;
    if (s1) {
        c.x = s1->mat[0][3];
        c.y = at->scr.pSat->mat[1][3];
        c.z = at->scr.pSat->mat[2][3];
        PSVECAdd(&c, add, &c);
        s1->setCoord(&c, (Vec*) &vecZero);
    }
    at = SceAtPtr(0x11);
    s1 = at->scr.pSat;
    if (s1) {
        c.x = s1->mat[0][3];
        c.y = at->scr.pSat->mat[1][3];
        c.z = at->scr.pSat->mat[2][3];
        PSVECAdd(&c, add, &c);
        s1->setCoord(&c, (Vec*) &vecZero);
    }
    at = SceAtPtr(0x12);
    s1 = at->scr.pSat;
    if (s1) {
        c.x = s1->mat[0][3];
        c.y = at->scr.pSat->mat[1][3];
        c.z = at->scr.pSat->mat[2][3];
        PSVECAdd(&c, add, &c);
        s1->setCoord(&c, (Vec*) &vecZero);
    }
    at = SceAtPtr(6);
    s1 = at->scr.pSat;
    if (s1) {
        c.x = s1->mat[0][3];
        c.y = at->scr.pSat->mat[1][3];
        c.z = at->scr.pSat->mat[2][3];
        PSVECAdd(&c, add, &c);
        s1->setCoord(&c, (Vec*) &vecZero);
    }
    addPos_sca(add, SceAtPtr(6));
    addPos_sca(add, SceAtPtr(0xC));
    addPos_sca(add, SceAtPtr(0xB));
}

// Area 0xF: the Ganados of the lift yard.
static void em_set2()
{
    if (pG->Game_level > 5) {
        setem(0x1A, 1);
    }
    if (pG->Game_level > 4) {
        setem(0x1B, 1);
    }
    SceSleep(1);
    setem(0x1C, 1);
    setem(0x1E, 1);
    SceSleep(1);
    setem(0x1F, 1);
    setem(0x25, 1);
    SceSleep(1);
    setem(0x26, 1);
    setem(0x31, 1);
    SceSleep(1);
    setem(0x32, 1);
    if (pG->Game_level > 6) {
        setem(0x45, 1);
    }
    r30f_work->em[0x32].setGoto(&pPL->pos, 6);
    RmfFlagOn(pG, RMF_LIFT_EMSET_START);
}

// The exit gate (0x17 / 0x18) slides open.
static void gate_open()
{
    u32 i = 0;

    SceSleep(0xF);
    SndCall(6, 0x10, &SmdGetObjPtr(0x17)->pos, 0, 0, 0);
    SmdGetObjPtr(0x17)->be_flag |= 0x20;
    SmdGetObjPtr(0x18)->be_flag |= 0x20;
    for (i = 0; i < 60; i++) {
        SmdGetObjPtr(0x17)->pos.z += 100.0f;
        SmdGetObjPtr(0x18)->pos.z -= 100.0f;
        SceSleep(1);
    }
    RmfFlagOn(pG, RMF_LIFT_GO);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM03, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM03, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM03, 0);
}
