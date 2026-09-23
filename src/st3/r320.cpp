#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj15.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "em3d.h"
#include "emdoor.h"
#include "emwindow.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "motion.h"
#include "area.h"
#include "cam_ctrl.h"
#include "snd.h"
#include "est.h"
#include "esp.h"
#include "fade.h"
#include "etc_model.h"
#include "room_data.h"
#include "game.h"
#include "shadow.h"
#include "sscrn.h"
#include "read.h"
#include "model.h"
#include "vec.h"
#include "rnd.h"
#include <string.h>

// Rotate a model from three components written y, x, z (the store order the original has).
static inline void setAngYXZ(cModel* m, f32 y, f32 x, f32 z)
{
    Vec v;

    v.y = y;
    v.x = x;
    v.z = z;
    m->setAng(&v);
}

// Room 3-20 (D:/Bio4/Prog/r320.cpp): the island landing site with the support helicopter (em3d,
// list 0x64): the gatling towers and their gunners, the seven enemy appearance areas, the gates, the
// switches, the gun turrets the helicopter destroys and the s00/s01 events.


struct R320Work {
    cEmWrap heri;           // 0x000  the support helicopter (list 0x64)
    cEmWrap em[57];         // 0x00C  the enemies set by emset (list entry per appearance area)
    cObjGatling* gatling[6]; // 0x2B8
    u8 pad_2D0[8];
    u32 chgCntB;            // 0x2D8  enemies re-set in area b
    u32 chgCntC;            // 0x2DC
    u32 chgCntD;            // 0x2E0
    u8 pad_2E4[4];
    u32 chgCntF;            // 0x2E8
    u8 pad_2EC[8];
    int chgWaitC;           // 0x2F4
    int fireTimer;          // 0x2F8  the gatling towers fire while it is negative
    int heriWait;           // 0x2FC  frames before the helicopter picks its next target
    int atkCnt;             // 0x300  frames until the next helicopter attack
    cSat* sat[11];          // 0x304
    cObj* smd;              // 0x330
    u32 emAlive;            // 0x334  SceCountEmAlive result
    u32 heriTimer;          // 0x338
    int target;             // 0x33C  em[] slot the helicopter attacks (-1: none)
    u8 pad_340[4];
};

static R320Work* r320_work;

// Helicopter attack wait per position (frames), scaled by the difficulty rank in getHeriTimeWait.
// Enemy destinations (setGoto) and the helicopter's patrol positions.
static Vec r320_posA[12] = {
    {59454.0f, 10647.0f, 27863.0f}, {71899.0f, 9544.0f, 27199.0f}, {71899.0f, 13144.0f, 27199.0f}, {59496.0f, 14097.0f, 15085.0f},
    {60655.0f, 10967.0f, 19014.0f}, {76652.0f, 16337.0f, 6189.0f}, {82147.0f, 16337.0f, 3039.0f}, {62429.0f, 11831.0f, 15698.0f},
    {61720.0f, 11831.0f, 14882.0f}, {60862.0f, 11831.0f, 13790.0f}, {62122.0f, 8854.0f, 16711.0f}, {60508.0f, 8854.0f, 16079.0f},
};
static Vec r320_posB[15] = {
    {68045.0f, 12555.0f, 25822.0f}, {74888.0f, 15449.0f, 4126.0f}, {76237.0f, 15449.0f, 7774.0f}, {67666.0f, 4075.0f, 13656.0f},
    {62028.0f, 11742.0f, 45553.0f}, {70823.0f, 7211.0f, 20806.0f}, {72320.0f, 9341.0f, 7829.0f}, {69697.0f, 16455.0f, -16264.0f},
    {59595.0f, 7605.0f, 4854.0f}, {43066.0f, 18369.0f, 7002.0f}, {24723.0f, 14524.0f, -3580.0f}, {50687.0f, 6750.0f, -7091.0f},
    {53767.0f, 8594.0f, 33514.0f}, {61582.0f, 8857.0f, 26520.0f}, {55600.0f, 6852.0f, -3181.0f},
};
static u32 r320_heriTime[9] = {0x168, 0x41A, 0x546, 0x708, 0x960, 0xA8C, 0xC4E, 0xC4E, 0xA8C};
// Lever turn acceleration per frame (reva_common_move).
static f32 r320_revaAccel = 0.008f;

// Room_flg bits of this room, from the PS2 symbols; RmfFlagChk(pG, n).
enum R320_FLAG {
    RMF_TARGET_DESTROY = 0,
    RMF_TARGET1_START = 1,
    RMF_EVENT_CANCEL = 2,
    RMF_TARGET4_START = 3,
    RMF_TARGET5_START = 4,
    RMF_TARGET6_START = 5,
    RMF_GATE2_CLOSE = 35,
    RMF_GATE2_OPEN = 36,
    RMF_EVTR320S01_READ = 48,
    RMF_AREA0 = 64,
    RMF_AREA1 = 65,
    RMF_AREA2 = 66,
    RMF_AREA_DOWN = 67,
    RMF_AREA3 = 68,
    RMF_AREA4 = 69,
    RMF_NO_SHOOT1 = 70,
    RMF_NO_SHOOT2 = 71,
    RMF_NO_SHOOT3 = 72,
    RMF_NO_SHOOT4 = 73,
    RMF_NO_SHOOT5 = 74,
};

#define R320_SAVE_FLAGS (*(u32*) (RoomData.getRoomSavePtr(pG->room_id) + 4))







void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp



// The helicopter into the work; it fires freely below rank 9.
static inline void r320_heriSet()
{
    if (!ScfFlagChk(pG, SCF_R321_HERI_DOWN)) {
        cEm3d* em;

        r320_work->heri.setEm(0x64, -1, 1, 1, 1);
        em = (cEm3d*) r320_work->heri.getPtr();
        if (em) {
            if (pG->Game_level <= 8) {
                em->setFreeFire();
            }
        }
    }
}

extern "C" void emset(int idx, int no);
extern "C" void setMisileUseNum(int num);
extern "C" int getMisileUseNum();
extern "C" void addMisileUseNum();
static void em_all_destroy_task();
static void r320_heri_event();
extern "C" void scr_delete();
extern "C" void scr_set();
extern "C" u32 getHeriTimeWait(int no);
extern "C" void SetHeriTargetEm();
extern "C" int setChange(int idx, int no, int idx2, int no2);
static void appear_a();
static void appear_b_exit();
static void appear_b();
static void appear_c_exit();
static void appear_c();
static void appear_d();
static void appear_e();
static void appear_f_exit();
static void appear_f();
static void appear_g();
extern "C" void reva_common_move(cObj* obj, f32 from, f32 to);
static void reva_b_down();
static void reva_c_down();
extern "C" void Gatling2_set();
static void switch1_move();
static void switch2_move();
static void switch3_move();
static void slide_move();
extern "C" void gate1_open(int no);
extern "C" void gate1_close();
extern "C" void gate2_open();
static void gate2_close();
static void attack_heri0();
static void attack_heri1();
static void attack_heri2();
static void attack_heri3();
static void attack_heri4();
static void destroy_0();
static void destroy_1();
static void destroy_2();
static void destroy_3();
static void destroy_4();
static void destroy_5();
static void destroy_6();
static void Evt_R320S00_Func(Event* e);
static void Evt_R320S01_Func(Event* e);
extern "C" void deleteFarEm(int dist);
static void em_lastset();
static void door_open();
static void door_opened();
static void r320_StrCheck();
static void tower_explode();
static void last_mes();
static void musen_exec();

// The enemy of list entry `no` into em[idx], with its list flags and death bit cleared.
void emset(int idx, int no)
{
    int list;

    pG->Em_list[no].be_flag &= ~2;
    list = pG->em_list_no;
    if (list >= 0) {
        u32* tbl = EM_FLG_ROW(list);

        tbl[(u32) no >> 5] &= ~(0x80000000 >> (no & 31));
    }
    r320_work->em[idx].setEm(no, -1, 1, 1, 1);
    if (r320_work->em[idx].isActive()) {
        r320_work->em[idx].setFindPL();
    }
}

// Missiles used so far (three save flag bits).
void setMisileUseNum(int num)
{
    if (num & 1) {
        R320_SAVE_FLAGS |= 0x1000;
    } else {
        R320_SAVE_FLAGS &= ~0x1000;
    }
    if (num & 2) {
        R320_SAVE_FLAGS |= 0x800;
    } else {
        R320_SAVE_FLAGS &= ~0x800;
    }
    if (num & 4) {
        R320_SAVE_FLAGS |= 0x400;
    } else {
        R320_SAVE_FLAGS &= ~0x400;
    }
}

// Rockets the helicopter has fired so far (0..7), from save record bits 0x1000/0x800/0x400.
int getMisileUseNum()
{
    int num = 0;

    if (R320_SAVE_FLAGS & 0x1000) {
        num = 1;
    }
    if (R320_SAVE_FLAGS & 0x800) {
        num += 2;
    }
    if (R320_SAVE_FLAGS & 0x400) {
        num += 4;
    }
    return num;
}

// One more rocket fired (saturates at 7).
void addMisileUseNum()
{
    if (getMisileUseNum() != 7) {
        setMisileUseNum(getMisileUseNum() + 1);
    }
}

// Room init (the base yard with the support helicopter): larger shadow pool, the item areas 0x94..0x96
// off, Debug_flg[1] 0x00200000; the helicopter (em3d, ESL 0x64) unless the room state is past it; the
// gates / levers / gatling towers / gun turrets posed from the save record bits; the seven appearance
// areas (8..15, 20), the lever areas, the cable slide, the tower door, the radio, the stream, the s00/
// s01 callbacks.
void R320Init()
{
    cEmWindow* win;
    cEmDoor* door;
    u32 i;

#line 224 "D:/Bio4/Prog/r320.cpp"
    r320_work = (R320Work*) MEM_CALLOC(sizeof(R320Work), 1, 0xd);
    r320_work->target = -1;
    ShadowMngReAlloc(0x100);
    SceAtSetEnable(0x94, 0);
    SceAtSetEnable(0x95, 0);
    SceAtSetEnable(0x96, 0);
    DbgFlagOn(pG, DBG_WARN_LEVEL_LOW);
    if (ScfFlagChk(pG, SCF_R31C_TOWER_EXPLODE) == 0) {
        if ((R320_SAVE_FLAGS & 0x100) == 0) {
            R320_SAVE_FLAGS |= 0x100;
            ScfFlagOn(pG, SCF_R31C_TOWER_EXPLODE);
            SceExec(0x12, (TaskFunc) tower_explode, 0, 0, 2, 0);
        }
    }
    PartsMgr.warnDiv = 100;
    DbgFlagOn(pG, DBG_EMW_ERR_NO_DISP);
    if (getRoomEtcWindow(0x1E, &win, 1)) {
        win->SetBreakModel();
        win->be_flag &= ~2;
    }
    ShadowMngReAlloc(0x100);
    EvtMgr.SetFunc("evt_r320s00_func", (void*) Evt_R320S00_Func);
    EvtMgr.SetFunc("evt_r320s01_func", (void*) Evt_R320S01_Func);
    if (pG->JumpPoint == 1 || pG->JumpPoint == 2) {
        R320_SAVE_FLAGS |= 0x80000000;
        setMisileUseNum(0);
        r320_heriSet();
        R320_SAVE_FLAGS |= 0x40000000;
        R320_SAVE_FLAGS |= 0x20000000;
        R320_SAVE_FLAGS |= 0x10000000;
        R320_SAVE_FLAGS |= 0x08000000;
        R320_SAVE_FLAGS |= 0x00800000;
        R320_SAVE_FLAGS |= 0x00400000;
        R320_SAVE_FLAGS |= 0x00200000;
        R320_SAVE_FLAGS |= 0x00100000;
    }
    if (ScfFlagChk(pG, SCF_R321_HERI_DOWN)) {
        if (pG->em_list_no >= 0) {
            u32* tbl = EM_FLG_ROW(pG->em_list_no);

            tbl[3] |= 0x08000000;
        }
    }
    Vec zeroVec = {0.0f, 0.0f, 0.0f};
    Vec pos;
    Vec rot;
    r320_work->sat[0] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zeroVec, &zeroVec, 1);
    pos.x = 70135.0f;
    pos.y = 12213.0f;
    pos.z = 27432.0f;
    rot.x = 0.0f;
    rot.y = 4.3228312f;
    rot.z = 0.0f;
    r320_work->sat[1] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
    pos.x = 43415.0f;
    pos.y = 9404.0f;
    pos.z = -14004.0f;
    rot.x = 0.0f;
    rot.y = -0.21746802f;
    rot.z = 0.0f;
    r320_work->sat[2] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
    pos.x = 33202.0f;
    pos.y = 10325.0f;
    pos.z = 1747.0f;
    rot.x = 0.0f;
    rot.y = 2.2174408f;
    rot.z = 0.0f;
    r320_work->sat[3] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
    r320_work->sat[4] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zeroVec, &zeroVec, 3);
    r320_work->sat[8] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zeroVec, &zeroVec, 5);
    r320_work->sat[9] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zeroVec, &zeroVec, 6);
    r320_work->sat[10] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zeroVec, &zeroVec, 7);
    pos.x = 43072.0f;
    pos.y = 8728.0f;
    pos.z = -12694.0f;
    rot.x = 0.0f;
    rot.y = -3.3182199f;
    rot.z = 0.0f;
    r320_work->sat[5] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 4);
    pos.x = 27594.0f;
    pos.y = 12609.0f;
    pos.z = -13892.0f;
    rot.x = 0.0f;
    rot.y = 4.166799f;
    rot.z = 0.0f;
    r320_work->sat[6] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 4);
    pos.x = 34259.0f;
    pos.y = 9660.0f;
    pos.z = 820.0f;
    rot.x = 0.0f;
    rot.y = -0.8840093f;
    rot.z = 0.0f;
    r320_work->sat[7] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 4);
    getRoomEtcDoor(2, &door, 1);
    if (door) {
        door->setLock(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 1);
    }
    getRoomEtcDoor(0x18, &door, 1);
    if (door) {
        door->setLock(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 1);
    }
    if ((R320_SAVE_FLAGS & 0x40000000) == 0) {
        pos.x = 60203.0f;
        pos.y = 13064.0f;
        pos.z = 27307.0f;
        rot.x = 0.0f;
        rot.y = -0.76f;
        rot.z = 0.0f;
        r320_work->gatling[0] = SetObjGatling(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), &pos, &rot);
        if (r320_work->gatling[0]) {
            r320_work->gatling[0]->setEat(ROOM_ARC_PTR(pG->pRoom, 0x12), 1);
            r320_work->gatling[0]->setNoSuspend(1);
            if (R320_SAVE_FLAGS & 0x00800000) {
                cEmGanado* em;

                emset(0, 0x53);
                em = (cEmGanado*) r320_work->em[0].getPtr();
                if (em) {
                    em->flag |= 1;
                    em->setGatling(r320_work->gatling[0], ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25),
                                   ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
                }
            }
        }
    }
    if ((R320_SAVE_FLAGS & 0x08000000) == 0) {
        pos.x = 75037.0f;
        pos.y = 15296.0f;
        pos.z = 6038.0f;
        rot.x = 0.0f;
        rot.y = -1.28f;
        rot.z = 0.0f;
        r320_work->gatling[1] = SetObjGatling(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), &pos, &rot);
        if (r320_work->gatling[1]) {
            r320_work->gatling[1]->setEat(ROOM_ARC_PTR(pG->pRoom, 0x12), 1);
            r320_work->gatling[1]->setNoSuspend(1);
            r320_work->gatling[1]->setMaxRot(1.9634955f);
            if (R320_SAVE_FLAGS & 0x00100000) {
                cEmGanado* em;

                emset(0x12, 0x4A);
                em = (cEmGanado*) r320_work->em[0x12].getPtr();
                if (em) {
                    em->flag |= 1;
                    em->setGatling(r320_work->gatling[1], ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25),
                                   ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
                }
            }
        }
    }
    if ((R320_SAVE_FLAGS & 0x80) == 0) {
        SceAtDataSet_exec(0x33, 0x12, 0, (TaskFunc) musen_exec, 0, 1);
    }
    if ((int) R320_SAVE_FLAGS >= 0) {
        SceAtDataSet_exec(5, 0x12, 0, (TaskFunc) r320_heri_event, 0, 1);
        pos.x = 41700.0f;
        pos.y = 13500.0f;
        pos.z = 27200.0f;
        rot.x = 0.0f;
        rot.y = 5.72468f;
        rot.z = 0.0f;
        r320_work->gatling[5] = SetObjGatling(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), &pos, &rot);
        r320_work->gatling[5]->setBreakMode(1);
        r320_work->gatling[5]->setNoSuspend(1);
        EvtMgr.EvtReadAram("event/evd/r320s00.evd", 0, 0, 0, 0);
    } else {
        EmReadSearch(0x1D, 0, 0);
        SceExec(0x12, (TaskFunc) r320_StrCheck, 0, 0, 2, 0);
        r320_heriSet();
        if (R320_SAVE_FLAGS & 0x40000000) {
            addMisileUseNum();
        }
        if (R320_SAVE_FLAGS & 0x20000000) {
            addMisileUseNum();
        }
        if (R320_SAVE_FLAGS & 0x10000000) {
            addMisileUseNum();
        }
        if (R320_SAVE_FLAGS & 0x08000000) {
            addMisileUseNum();
        }
        SmdSetTrans(0x25, 0);
        SmdSetTrans(0x27, 0);
        scr_delete();
    }
    if ((R320_SAVE_FLAGS & 0x00800000) == 0) {
        SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) appear_a, 0, 1);
    }
    if ((R320_SAVE_FLAGS & 0x00400000) == 0) {
        SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) appear_b, 0, 1);
    }
    if ((R320_SAVE_FLAGS & 0x00200000) == 0) {
        SceAtDataSet_exec(0xA, 0x12, 0, (TaskFunc) appear_c, 0, 1);
    }
    if ((R320_SAVE_FLAGS & 0x00100000) == 0) {
        SceAtDataSet_exec(0xB, 0x12, 0, (TaskFunc) appear_d, 0, 1);
    }
    if ((R320_SAVE_FLAGS & 0x00080000) == 0) {
        SceAtDataSet_exec(0xE, 0x12, 0, (TaskFunc) appear_e, 0, 1);
    }
    if ((R320_SAVE_FLAGS & 0x00040000) == 0) {
        SceAtDataSet_exec(0xF, 0x12, 0, (TaskFunc) appear_f, 0, 1);
    }
    if ((R320_SAVE_FLAGS & 0x00020000) == 0) {
        SceAtDataSet_exec(0x14, 0x12, 0, (TaskFunc) appear_g, 0, 1);
    }
    if (R320_SAVE_FLAGS & 0x40000000) {
        EatMgr.destroy(r320_work->sat[10]);
        SmdSetTrans(0x1F, 0);
    }
    if (R320_SAVE_FLAGS & 0x20000000) {
        EatMgr.destroy(r320_work->sat[8]);
        SatMgr.destroy(r320_work->sat[1]);
        SmdSetTrans(0x15, 0);
    }
    if (R320_SAVE_FLAGS & 0x10000000) {
        EatMgr.destroy(r320_work->sat[9]);
        SatMgr.destroy(r320_work->sat[0]);
        SceAtSetEnable(0x12, 0);
        SceAtSetEnable(0x26, 1);
        SmdSetTrans(0x21, 0);
    } else {
        SceAtSetEnable(0x12, 1);
        SceAtSetEnable(0x26, 0);
    }
    if (R320_SAVE_FLAGS & 0x08000000) {
        EatMgr.destroy(r320_work->sat[4]);
        SmdSetTrans(9, 0);
        SmdSetTrans(0x10, 0);
    } else {
        EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    }
    if (R320_SAVE_FLAGS & 0x04000000) {
        EatMgr.destroy(r320_work->sat[5]);
        SatMgr.destroy(r320_work->sat[2]);
        SmdSetTrans(0x16, 0);
        SceAtSetEnable(0x28, 1);
    } else {
        SceAtSetEnable(0x28, 0);
    }
    if (R320_SAVE_FLAGS & 0x02000000) {
        EatMgr.destroy(r320_work->sat[7]);
        SatMgr.destroy(r320_work->sat[3]);
        SmdSetTrans(0x17, 0);
        SceAtSetEnable(0x29, 1);
    } else {
        SceAtSetEnable(0x29, 0);
    }
    int zero = 0;
    SmdGetObjPtr(0x2E)->be_flag |= 0x20;
    SmdGetObjPtr(0x2F)->be_flag |= 0x20;
    SmdGetObjPtr(0x30)->be_flag |= 0x20;
    if ((R320_SAVE_FLAGS & 0x00010000) == 0) {
        SceAtDataSet_exec(0x1B, 0x12, 0, (TaskFunc) switch1_move, 0, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 7, 1, ESP_CORE_KIND_ROOM01, (void*) zero, (void*) zero);
    } else {
        SmdGetObjPtr(0x2E)->pList->ang.z = -1.24f;
        EstSet(0, -1, 0, 0, EFF_ROOM, 8, 1, ESP_CORE_KIND_ROOM01, (void*) zero, (void*) zero);
        gate1_open(1);
    }
    int zero2 = 0;
    if ((R320_SAVE_FLAGS & 0x8000) == 0) {
        SceAtDataSet_exec(0x1C, 0x12, 0, (TaskFunc) switch2_move, 0, 1);
    }
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM02, (void*) zero2, (void*) zero2);
    if ((R320_SAVE_FLAGS & 0x4000) == 0) {
        SceAtDataSet_exec(0x1D, 0x12, 0, (TaskFunc) switch3_move, 0, 1);
    }
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xC, 1, ESP_CORE_KIND_ROOM03, (void*) zero2, (void*) zero2);
    {
        cObj* o = SmdGetObjPtr(0x2B);

        o->be_flag |= 0x20;
        o->pos.y = 10961.0f;
    }
    SceAtSetEnable(0x20, 0);
    if ((R320_SAVE_FLAGS & 0x8000) == 0 || (R320_SAVE_FLAGS & 0x4000) == 0) {
        SceAtDataSet_exec(0x2E, 0x12, 0, (TaskFunc) gate2_close, 0, 1);
    }
    SceAtDataSet_exec(0x1E, 0x12, 0, (TaskFunc) slide_move, 0, 1);
    if ((R320_SAVE_FLAGS & 0x200) == 0) {
        SceAtDataSet_exec(0x2D, 0x12, 0, (TaskFunc) door_open, 0, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x13, 1, ESP_CORE_KIND_ROOM04, 0, 0);
    } else {
        SceExec(0x12, (TaskFunc) door_opened, 0, 0, 2, 0);
    }
    scr_delete();
    if (R320_SAVE_FLAGS & 0x00020000) {
        Gatling2_set();
    }
    if ((R320_SAVE_FLAGS & 0x2000) && (SysFlagChk(pG, SYS_CONTINUE))) {
        SceExec(0x12, (TaskFunc) em_all_destroy_task, 0, 0, 2, 0);
        r320_heriSet();
        SceAtDataSet_exec(0x2B, 0x12, 0, (TaskFunc) em_lastset, 0, 1);
        scr_set();
    }
    {
        Vec smdPos = {0.0f, 0.0f, 0.0f};
        Vec smdRot = {0.0f, 0.0f, 0.0f};
        cEm* dram;

        r320_work->smd = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x29), ROOM_ARC_PTR(pG->pRoom, 0x2A), &smdPos, &smdRot, 0x10, 1);
        r320_work->smd->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2B), 0xA, 0, 1, 0);
        r320_work->smd->Motion.Seq_speed = 0.0f;
        r320_work->smd->be_flag |= 0x1000;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xE, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        SmdSetTrans(0x22, 0);
        for (i = 0; i < 0x40; i++) {
            if (getRoomEtcDram(i, &dram, 0)) {
                dram->atari.m_flag &= ~0x200;
            }
        }
    }
    SceAtDataSet_exec(0x31, 0x12, 0, (TaskFunc) last_mes, 0, 1);
    if (R320_SAVE_FLAGS & 0x00020000) {
        scr_set();
        SceAtSetEnable(0x31, 0);
        SceAtSetEnable(0x36, 0);
        if (r320_work->gatling[2]) {
            r320_work->gatling[2]->setBreak();
        }
        if (r320_work->gatling[3]) {
            r320_work->gatling[3]->setBreak();
        }
        if (r320_work->gatling[4]) {
            r320_work->gatling[4]->setBreak();
        }
    }
}

// One frame in: remove every Ganado (0x10..0x20) — the return visit's clean-up.
static void em_all_destroy_task()
{
    SceSleep(1);
    SceDestroyEm(0x10, 0x20);
}

// The helicopter's arrival event (area 5).
static void r320_heri_event()
{
    R320_SAVE_FLAGS |= 0x80000000;
    SceSleep(1);
    EvtMgr.EvtReadExec("event/evd/r320s00.evd", 0, EvtReadFlagNone);
    if (RmfFlagChk(pG, RMF_EVENT_CANCEL) == 0) {
        EvtMgr.EvtReadExec("event/evd/r320s01.evd", 0, EvtReadFlagNone);
    } else {
        SndRoomStrStart(1, 0, 1);
    }
    pPL->setPos(27120.0f, 7699.0f, 45134.0f);
    setAngYXZ(pPL, 2.46f, 0.0f, 0.0f);
    CamCtrl.Comeback(0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xF, 1, ESP_CORE_KIND_NONE, 0, 0);
    EmReadSearch(0x1D, 0, 0);
    SmdSetTrans(0x25, 0);
    SmdSetTrans(0x27, 0);
    setMisileUseNum(0);
    r320_work->gatling[5]->setBreak();
    if (!ScfFlagChk(pG, SCF_R321_HERI_DOWN)) {
        r320_work->heri.setEm(0x64, -1, 1, 1, 1);
    }
    scr_delete();
    SceExec(0x12, (TaskFunc) r320_StrCheck, 0, 0, 2, 0);
}

// Hide the gun-tower scroll objects (0x16/0x17/0x18) and the yard props 0x12/0x32/0x33 (for the events).
void scr_delete()
{
    SmdSetTrans(0x16, 0);
    SmdSetTrans(0x17, 0);
    SmdSetTrans(0x18, 0);
    SmdSetTrans(0x12, 0);
    SmdSetTrans(0x32, 0);
    SmdSetTrans(0x33, 0);
}

// Show the gun towers still standing (save record bits 0x04000000/0x02000000/0x01000000 = destroyed) and the props.
void scr_set()
{
    if ((R320_SAVE_FLAGS & 0x04000000) == 0) {
        SmdSetTrans(0x16, 1);
    }
    if ((R320_SAVE_FLAGS & 0x02000000) == 0) {
        SmdSetTrans(0x17, 1);
    }
    if ((R320_SAVE_FLAGS & 0x01000000) == 0) {
        SmdSetTrans(0x18, 1);
    }
    SmdSetTrans(0x12, 1);
    SmdSetTrans(0x32, 1);
    SmdSetTrans(0x33, 1);
}

// Frames the helicopter waits at position `no`, scaled by the difficulty rank.
u32 getHeriTimeWait(int no)
{
    u32 t = r320_heriTime[no];

    if (pG->Game_level <= 2) {
        t = (u32) ((f32) t * 0.4f);
    }
    if (pG->Game_level == 3) {
        t = (u32) ((f32) t * 0.5f);
    }
    if (pG->Game_level == 4) {
        t = (u32) ((f32) t * 0.6f);
    }
    if (pG->Game_level == 5) {
        t = (u32) ((f32) t * 0.7f);
    }
    if (pG->Game_level == 6) {
        t = (u32) ((f32) t * 1.0f);
    }
    if (pG->Game_level == 7) {
        t = (u32) ((f32) t * 1.7f);
    }
    if (pG->Game_level == 8) {
        t = (u32) ((f32) t * 2.8f);
    }
    if (pG->Game_level == 9) {
        t = (u32) ((f32) t * 4.5f);
    }
    if (pG->Game_level == 10) {
        t = (u32) ((f32) t * 5.5f);
    }
    return t;
}

// Per frame (the helicopter AI): while the helicopter can pick a target, after its wait it takes the
// next standing gun tower (save bits) or an enemy (SetHeriTargetEm); the attack counter versus
// getHeriTimeWait(rockets fired) fires the next rocket; the yard's phases advance by the save bits.
void R320Main()
{
    cEm3d* heri;
    int cnt;

    heri = (cEm3d*) r320_work->heri.getPtr();
    if (heri) {
        if (r320_work->heriWait > 0) {
            r320_work->heriWait = r320_work->heriWait - 1;
        }
        if (heri->ckSelectEnable()) {
            if (r320_work->heriWait <= 0) {
                if (R320_SAVE_FLAGS & 0x00800000) {
                    if (r320_work->em[0].isActive() == 0) {
                        R320_SAVE_FLAGS |= 0x40000000;
                    }
                }
                r320_work->heriTimer = r320_work->heriTimer + 1;
                if (r320_work->heriTimer <= 0x12B || r320_work->emAlive == 0) {
                    cEm3d* em = (cEm3d*) r320_work->heri.getPtr();

                    if (RmfFlagChk(pG, RMF_AREA0)) {
                        em->setPatrolPos(&r320_posB[4]);
                    }
                    if (RmfFlagChk(pG, RMF_AREA1)) {
                        em->setPatrolPos(&r320_posB[4]);
                    }
                    if (RmfFlagChk(pG, RMF_AREA2)) {
                        em->setPatrolPos(&r320_posB[5]);
                    }
                    if (RmfFlagChk(pG, RMF_AREA3)) {
                        em->setPatrolPos(&r320_posB[6]);
                    }
                    if (RmfFlagChk(pG, RMF_AREA4)) {
                        if ((R320_SAVE_FLAGS & 0x04000000) == 0) {
                            em->setPatrolPos(&r320_posB[9]);
                        } else if ((R320_SAVE_FLAGS & 0x02000000) == 0) {
                            em->setPatrolPos(&r320_posB[10]);
                        } else if ((R320_SAVE_FLAGS & 0x01000000) == 0) {
                            em->setPatrolPos(&r320_posB[11]);
                        }
                    }
                } else {
                    if (r320_work->heriTimer > 0xE10) {
                        r320_work->heriTimer = 0;
                    }
                    SetHeriTargetEm();
                }
                cnt = r320_work->atkCnt;
            if ((R320_SAVE_FLAGS & 0x40000000) == 0 && RmfFlagChk(pG, RMF_AREA0) && (R320_SAVE_FLAGS & 0x00800000) && r320_work->em[0].isActive()) {
                r320_work->atkCnt = r320_work->atkCnt + 1;
                if (r320_work->atkCnt > getHeriTimeWait(getMisileUseNum())) {
                    SceExec(0x12, (TaskFunc) attack_heri0, 0, 0, 2, 0);
                    r320_work->atkCnt = 0;
                }
            }
            if ((R320_SAVE_FLAGS & 0x20000000) == 0 && RmfFlagChk(pG, RMF_AREA1) && (R320_SAVE_FLAGS & 0x00200000)) {
                r320_work->atkCnt = r320_work->atkCnt + 1;
                if (r320_work->atkCnt > getHeriTimeWait(getMisileUseNum())) {
                    SceExec(0x12, (TaskFunc) attack_heri1, 0, 0, 2, 0);
                    r320_work->atkCnt = 0;
                }
            }
            if ((R320_SAVE_FLAGS & 0x10000000) == 0 && RmfFlagChk(pG, RMF_AREA2) && (R320_SAVE_FLAGS & 0x00400000)) {
                r320_work->atkCnt = r320_work->atkCnt + 1;
                if (r320_work->atkCnt > getHeriTimeWait(getMisileUseNum())) {
                    SceExec(0x12, (TaskFunc) attack_heri2, 0, 0, 2, 0);
                    r320_work->atkCnt = 0;
                }
            }
            if ((R320_SAVE_FLAGS & 0x08000000) == 0 && RmfFlagChk(pG, RMF_AREA3) && (R320_SAVE_FLAGS & 0x00100000)) {
                r320_work->atkCnt = r320_work->atkCnt + 1;
                if (r320_work->atkCnt > getHeriTimeWait(getMisileUseNum())) {
                    SceExec(0x12, (TaskFunc) attack_heri3, 0, 0, 2, 0);
                    r320_work->atkCnt = 0;
                }
            }
            if (RmfFlagChk(pG, RMF_AREA4) && (R320_SAVE_FLAGS & 0x01000000) == 0) {
                r320_work->atkCnt = r320_work->atkCnt + 1;
                if (r320_work->atkCnt > getHeriTimeWait(getMisileUseNum())) {
                    SceExec(0x12, (TaskFunc) attack_heri4, 0, 0, 2, 0);
                    r320_work->atkCnt = 0;
                }
            }
            if (cnt != r320_work->atkCnt) {
                SceDebugDisp("CNT[%d/%d]", r320_work->atkCnt, getHeriTimeWait(getMisileUseNum()));
            }
            }
        }
    }
    if ((R320_SAVE_FLAGS & 0x00800000) && (R320_SAVE_FLAGS & 0)) {
    }
    if ((R320_SAVE_FLAGS & 0x00400000) && (R320_SAVE_FLAGS & 0x10000000) == 0 && (R320_SAVE_FLAGS & 0x00100000) == 0) {
        if (setChange(4, 0x58, 6, 0x54)) {
            r320_work->em[6].setGoto(&r320_posA[7], 0xC);
        }
        if (setChange(5, 0x59, 7, 0x55)) {
            r320_work->em[7].setGoto(&r320_posA[8], 0xC);
        }
        if (r320_work->chgCntB <= 3) {
            if (setChange(6, 0x54, 6, 0x54)) {
                r320_work->em[6].setGoto(&r320_posA[7], 0xC);
                r320_work->chgCntB = r320_work->chgCntB + 1;
            }
            if (setChange(7, 0x55, 7, 0x55)) {
                r320_work->em[7].setGoto(&r320_posA[7], 0xC);
                r320_work->chgCntB = r320_work->chgCntB + 1;
            }
            if ((R320_SAVE_FLAGS & 0x20000000) == 0 && r320_work->chgCntB == 0) {
                if (setChange(8, 0x49, 8, 0x49)) {
                    r320_work->em[8].setGoto(&r320_posB[0], 0xC);
                    r320_work->chgCntB = r320_work->chgCntB + 1;
                }
            }
        }
    }
    if (((R320_SAVE_FLAGS & 0x00400000) || (R320_SAVE_FLAGS & 0x00200000)) && (R320_SAVE_FLAGS & 0x20000000) == 0
        && RmfFlagChk(pG, RMF_TARGET1_START) == 0 && (R320_SAVE_FLAGS & 0x08000000) == 0) {
        if (r320_work->chgWaitC > 0) {
            r320_work->chgWaitC = r320_work->chgWaitC - 1;
        } else {
            u32 n = r320_work->chgCntC;

            if (n <= 7) {
                if (setChange(9, 0x4B, 9, 0x4B)) {
                    r320_work->chgCntC = r320_work->chgCntC + 1;
                }
                if (setChange(0xA, 0x4F, 0xA, 0x4F)) {
                    r320_work->chgCntC = r320_work->chgCntC + 1;
                }
                if (r320_work->chgCntC == 1) {
                    r320_work->chgCntC = 2;
                    emset(0xB, 0x5B);
                }
                if (n != r320_work->chgCntC && r320_work->chgCntC > 4) {
                    r320_work->chgWaitC = 0x78;
                }
            }
        }
    }
    if ((R320_SAVE_FLAGS & 0x00200000) && (R320_SAVE_FLAGS & 0x00100000) == 0) {
        if (RmfFlagChk(pG, RMF_AREA_DOWN)) {
            if (r320_work->chgCntD == 0) {
                r320_work->chgCntD = 1;
                emset(0xE, 0x46);
                r320_work->em[0xE].setGoto(&r320_posB[13], 0xC);
                r320_work->em[0xF].setGoto(&r320_posB[12], 0xC);
                emset(0x10, 0x48);
                r320_work->em[0x10].setGoto(&r320_posB[12], 0xC);
                r320_work->chgCntD = r320_work->chgCntD + 1;
                r320_work->chgCntD = r320_work->chgCntD + 1;
            } else if (r320_work->chgCntD <= 3) {
                if (setChange(0xF, 0x47, 0xF, 0x47)) {
                    r320_work->chgCntD = r320_work->chgCntD + 1;
                    r320_work->em[0xF].setGoto(&r320_posB[12], 0xC);
                }
                if (setChange(0x10, 0x48, 0x10, 0x48)) {
                    r320_work->chgCntD = r320_work->chgCntD + 1;
                    r320_work->em[0x10].setGoto(&r320_posB[12], 0xC);
                }
            }
        } else {
            if (r320_work->chgCntD <= 3) {
                if (setChange(0xA, 0x4F, 0x37, 0x60)) {
                    r320_work->chgCntD = r320_work->chgCntD + 1;
                    r320_work->em[0x37].setGoto(&pPL->pos, 0xC);
                }
                if (setChange(0x37, 0x60, 0x37, 0x60)) {
                    r320_work->chgCntD = r320_work->chgCntD + 1;
                    r320_work->em[0x37].setGoto(&pPL->pos, 0xC);
                }
            }
        }
    }
    if ((R320_SAVE_FLAGS & 0) == 0 && (R320_SAVE_FLAGS & 0x00040000) && (R320_SAVE_FLAGS & 0x00020000) == 0) {
        Vec pos = {53523.0f, 12868.0f, 7344.0f};

        if (r320_work->chgCntF <= 5) {
            if (RmfFlagChk(pG, RMF_AREA3)) {
                if (r320_work->em[0x2D].ckResetEnable()) {
                    if (setChange(0x2D, 0x42, 0x11, 0x4D) || setChange(0x11, 0x4D, 0x11, 0x4D)) {
                        r320_work->em[0x11].setGoto(&pos, 0xC);
                        r320_work->chgCntF = r320_work->chgCntF + 1;
                    }
                }
            }
            if (r320_work->chgCntF == 0) {
                if (r320_work->em[0x38].ckResetEnable()) {
                    if (setChange(0x38, 0x61, 7, 0x55) || setChange(7, 0x55, 7, 0x55)) {
                        r320_work->em[7].setGoto(&pos, 0xC);
                        r320_work->chgCntF = r320_work->chgCntF + 1;
                    }
                }
            }
            if (AreaHitCheck(&SceAtPtr(0xE)->area, &pPL->pos) == 0) {
                if (r320_work->em[0x2C].ckResetEnable()) {
                    if (setChange(0x2C, 0x41, 0x18, 0x5C) || setChange(0x18, 0x5C, 0x18, 0x5C)) {
                        r320_work->em[0x18].setGoto(&pos, 0xC);
                        r320_work->chgCntF = r320_work->chgCntF + 1;
                    }
                }
            }
        }
    }
    if (R320_SAVE_FLAGS & 0x00020000) {
        if (setChange(0x1D, 0x39, 0x28, 0x44)) {
            r320_work->em[0x28].setGoto(&pPL->pos, 6);
        }
        if (setChange(0x28, 0x44, 0x27, 0x3E)) {
            r320_work->em[0x27].setGoto(&pPL->pos, 0xC);
        }
        if (setChange(0x22, 0x2B, 0x30, 0x33)) {
            r320_work->em[0x30].setGoto(&pPL->pos, 6);
        }
        if (setChange(0x30, 0x33, 0x24, 0x37)) {
            r320_work->em[0x24].setGoto(&pPL->pos, 6);
        }
        if (setChange(0x1B, 0x2F, 0x29, 0x45)) {
            r320_work->em[0x29].setGoto(&pPL->pos, 0xC);
        }
        if (setChange(0x29, 0x45, 0x26, 0x3D)) {
            r320_work->em[0x26].setGoto(&pPL->pos, 6);
        }
        if (setChange(0x20, 0x2E, 0x2E, 0x31)) {
            r320_work->em[0x2E].setGoto(&pPL->pos, 6);
        }
        if (setChange(0x2E, 0x31, 0x32, 0x35)) {
            r320_work->em[0x32].setGoto(&pPL->pos, 0xC);
        }
        if (setChange(0x35, 0x2D, 0x31, 0x34)) {
            r320_work->em[0x31].setGoto(&pPL->pos, 0xC);
        }
        if (setChange(0x33, 0x36, 0x2F, 0x32)) {
            r320_work->em[0x2F].setGoto(&pPL->pos, 0xC);
        }
        r320_work->fireTimer = r320_work->fireTimer - 1;
        if (r320_work->fireTimer < 0) {
            if (r320_work->gatling[2]) {
                r320_work->gatling[2]->stopFire();
            }
            if (r320_work->gatling[3]) {
                r320_work->gatling[3]->stopFire();
            }
            if (r320_work->gatling[4]) {
                r320_work->gatling[4]->stopFire();
            }
        }
        if (r320_work->fireTimer < -0x4B) {
            r320_work->fireTimer = 0x78;
            if (r320_work->gatling[2]) {
                r320_work->gatling[2]->setFire();
            }
            if (r320_work->gatling[3]) {
                r320_work->gatling[3]->setFire();
            }
            if (r320_work->gatling[4]) {
                r320_work->gatling[4]->setFire();
            }
        }
        if ((RmfFlagChk(pG, RMF_NO_SHOOT1) && r320_work->em[0x1B].isActive()) || (RmfFlagChk(pG, RMF_NO_SHOOT2) && r320_work->em[0x1B].isActive())) {
            if (r320_work->gatling[4]) {
                r320_work->gatling[4]->stopFire();
            }
        }
        {
            // A named copy of the flag word: jump.c thread_jumps does not thread a user variable, so
            // the 0x01000000 test is not folded into the previous condition's failure path.
            u32 f = pG->Room_flg[2];

            if ((f & 0x01000000) && r320_work->em[0x1B].isActive()) {
                if (r320_work->gatling[2]) {
                    r320_work->gatling[2]->stopFire();
                }
            }
        }
        if (RmfFlagChk(pG, RMF_NO_SHOOT3) && r320_work->em[0x1B].isActive()) {
            if (r320_work->gatling[4]) {
                r320_work->gatling[4]->stopFire();
            }
        }
        if (RmfFlagChk(pG, RMF_NO_SHOOT4) && r320_work->em[0x25].isActive()) {
            if (r320_work->gatling[3]) {
                r320_work->gatling[3]->stopFire();
            }
        }
        if (RmfFlagChk(pG, RMF_NO_SHOOT5)) {
            if (r320_work->gatling[2]) {
                r320_work->gatling[2]->stopFire();
            }
        }
        if (RmfFlagChk(pG, RMF_TARGET4_START) == 0 && r320_work->em[0x25].isAlive() && r320_work->em[0x25].isActive() == 0) {
            SceAtSetEnable(0x95, 1);
        }
        if (RmfFlagChk(pG, RMF_TARGET5_START) == 0 && r320_work->em[0x1B].isAlive() && r320_work->em[0x1B].isActive() == 0) {
            SceAtSetEnable(0x94, 1);
        }
        if (RmfFlagChk(pG, RMF_TARGET6_START) == 0 && r320_work->em[0x20].isAlive() && r320_work->em[0x20].isActive() == 0) {
            SceAtSetEnable(0x96, 1);
        }
    }
    r320_work->emAlive = SceCountEmAlive(0x10, 0x20);
    if (r320_work->gatling[4] && (r320_work->gatling[4]->be_flag & 2) == 0) {
        SceAtSetEnable(0x36, 0);
    }
}
// The helicopter's rocket target: the active enemy farthest from the player, re-picked every 140
// frames or when the current one is gone.
void SetHeriTargetEm()
{
    int chg = 0;

    if (r320_work->target != -1) {
        if (r320_work->em[r320_work->target].isActive() == 1) {
            cEm3d* heri = (cEm3d*) r320_work->heri.getPtr();

            if (heri) {
                if (pG->Game_level <= 6) {
                    heri->setPatrolPos(&r320_work->em[r320_work->target].getPtr()->pos);
                }
            }
        } else {
            chg = 1;
        }
    }
    if (r320_work->heriTimer % 140 == 1 || chg) {
        int best = -1;
        f32 dist = 0.0f;
        u32 i;

        for (i = 0; i <= 0x38; i++) {
            if (i == 0x20 || i == 0x1B || i == 0x25 || i == 0 || i == 0x12 || i == 0x1A) {
                continue;
            }
            if (r320_work->em[i].isActive() == 1) {
                if (r320_work->em[i].getPtr()->l_pl > dist) {
                    best = i;
                    dist = r320_work->em[i].getPtr()->l_pl;
                }
            }
        }
        if (best != -1) {
            r320_work->target = best;
        }
    }
}

// Replaces the enemy of em[idx] (list entry `no`) by em[idx2] (entry `no2`) once the first one is
// gone: the same slot resets the enemy, another slot creates it. 1 when an enemy was set.
#define R320_EM_OFS(n) ((n) * sizeof(cEmWrap) + 0xC)
#define R320_EM(ofs) (*(cEmWrap*) ((u8*) r320_work + (ofs)))

// Replace enemy slot idx2 (list no2) by slot idx (list no) once the old one may be reset, unless more
// than 9 are alive or the new entry is not spawned yet; 1 when the swap happened.
int setChange(int idx, int no, int idx2, int no2)
{
    if (r320_work->emAlive > 9) {
        goto fail;
    }
    if ((pG->Em_list[no].be_flag & 2) == 0) {
        goto fail;
    }
    if (idx == idx2) {
        if (R320_EM(R320_EM_OFS(idx2)).isAlive() == 0) {
            return 0;
        }
        if (R320_EM(R320_EM_OFS(idx2)).ckResetEnable() == 0) {
            return 0;
        }
        R320_EM(R320_EM_OFS(idx2)).setReset();
        R320_EM(R320_EM_OFS(idx2)).setFindPL();
        return 1;
    } else {
        int list;

        if (pG->Em_list[no2].be_flag & 2) {
            goto fail;
        }
        if (r320_work->em[idx].isActive()) {
            goto fail;
        }
        list = pG->em_list_no;
        if (list >= 0) {
            u32* tbl = EM_FLG_ROW(list);

            tbl[(u32) no2 >> 5] &= ~(0x80000000 >> (no2 & 31));
        }
        R320_EM(R320_EM_OFS(idx2)).setEm(no2, 7, 0, 1, 1);
        R320_EM(R320_EM_OFS(idx2)).setFindPL();
        return 1;
    }
fail:
    return 0;
}

// Area 8: the first gatling gunner and his partner.
static void appear_a()
{
    cEmGanado* em;

    R320_SAVE_FLAGS |= 0x00800000;
    emset(1, 0x5A);
    emset(0, 0x53);
    em = (cEmGanado*) r320_work->em[0].getPtr();
    if (em) {
        em->flag |= 1;
        em->setGatling(r320_work->gatling[0], ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25),
                       ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
    }
    r320_work->em[0].setNoSuspend(1);
    r320_work->em[1].setNoSuspend(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventStart(1);
    CamCtrl.CutCall(0xC);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    r320_work->em[0].setNoSuspend(0);
    r320_work->em[1].setNoSuspend(0);
    r320_work->gatling[0]->ang.y = -0.91607f;
    r320_work->gatling[0]->pList->pList->pList->ang.x = 0.35561f;
}

// End of the area-9 cut: camera back, SceEventEnd, Ganados em[2..5] may suspend.
static void appear_b_exit()
{
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r320_work->em[2].setNoSuspend(0);
    r320_work->em[3].setNoSuspend(0);
    r320_work->em[5].setNoSuspend(0);
    r320_work->em[4].setNoSuspend(0);
}

// Area 9: the group at the second gate.
static void appear_b()
{
    if ((R320_SAVE_FLAGS & 0x00100000) == 0 && (R320_SAVE_FLAGS & 0x00400000) == 0) {
        R320_SAVE_FLAGS |= 0x00400000;
        deleteFarEm(5);
        emset(2, 0x4E);
        emset(3, 0x50);
        emset(4, 0x58);
        emset(5, 0x59);
        emset(9, 0x4B);
        emset(0xA, 0x4F);
        r320_work->em[2].setGoto(&r320_posA[7], 0xC);
        r320_work->em[3].setGoto(&r320_posA[8], 0xC);
        r320_work->em[5].setGoto(&r320_posA[9], 0xC);
        r320_work->em[4].setGoto(&r320_posA[10], 0xC);
        if ((R320_SAVE_FLAGS & 0x20000000) == 0) {
            emset(8, 0x49);
            r320_work->em[8].setGoto(&r320_posB[0], 0xC);
        }
        r320_work->em[2].setNoSuspend(1);
        r320_work->em[3].setNoSuspend(1);
        r320_work->em[5].setNoSuspend(1);
        r320_work->em[4].setNoSuspend(1);
        SceEventStart(1);
        CamCtrl.CutCall(0xD);
        SceSetEventCancel(1, (TaskFunc) appear_b_exit, 0, -1, 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        appear_b_exit();
    }
}

// End of the area-10 cut: camera back, SceEventEnd, Status_flg[2] 0x02000000 off, em[9..0xD] may suspend.
static void appear_c_exit()
{
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    r320_work->em[9].setNoSuspend(0);
    r320_work->em[0xA].setNoSuspend(0);
    r320_work->em[0xC].setNoSuspend(0);
    r320_work->em[0xD].setNoSuspend(0);
}

// Area 10: the group behind the second gate.
static void appear_c()
{
    if ((R320_SAVE_FLAGS & 0x00100000) == 0) {
        R320_SAVE_FLAGS |= 0x00200000;
        deleteFarEm(3);
        emset(0xA, 0x4F);
        emset(0xC, 0x4C);
        emset(0xD, 0x52);
        emset(0x11, 0x4D);
        r320_work->em[9].setNoSuspend(1);
        r320_work->em[0xA].setNoSuspend(1);
        r320_work->em[0xC].setNoSuspend(1);
        r320_work->em[0xD].setNoSuspend(1);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceEventStart(1);
        CamCtrl.CutCall(0xE);
        SceSetEventCancel(1, (TaskFunc) appear_c_exit, 0, -1, 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        appear_c_exit();
    }
}

// Area 11: the second gatling tower group; the door behind the player breaks.
static void appear_d()
{
    cEmGanado* em;
    cEmDoor* door;
    u32 i;

    if (SceCheckEventStart()) {
        SceAtSetEnable(0xB, 0);
        R320_SAVE_FLAGS |= 0x00100000;
        deleteFarEm(3);
        for (i = 0; i <= 0x38; i++) {
            if (r320_work->em[i].isAlive() == 1) {
                if (r320_work->em[i].ckResetEnable()) {
                    r320_work->em[i].destroy();
                }
            }
        }
        emset(0x11, 0x4D);
        emset(0x12, 0x4A);
        em = (cEmGanado*) r320_work->em[0x12].getPtr();
        if (em) {
            em->flag |= 1;
            em->setGatling(r320_work->gatling[1], ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25),
                           ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
        }
        emset(0x13, 0x51);
        emset(0x14, 0x56);
        emset(0x15, 0x57);
        emset(0x16, 0x5D);
        emset(0x18, 0x5C);
        emset(0x19, 0x5E);
        r320_work->em[0x14].setGoto(&r320_posB[1], 0xC);
        r320_work->em[0x16].setGoto(&r320_posB[2], 0xC);
        r320_work->em[0x11].setGoto(&r320_posB[3], 6);
        getRoomEtcDoor(0x18, &door, 1);
        if (door) {
            door->setBreak(&pPL->pos);
        }
        r320_work->em[0x12].setNoSuspend(1);
        r320_work->em[0x14].setNoSuspend(1);
        r320_work->em[0x15].setNoSuspend(1);
        r320_work->em[0x16].setNoSuspend(1);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceEventStart(1);
        pPL->setNoSuspend(1);
        r320_work->gatling[1]->ang.y = -1.68495f;
        r320_work->gatling[1]->pList->pList->pList->ang.x = 1.08596f;
        CamCtrl.CutCall(0xF);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        pPL->setNoSuspend(0);
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
        r320_work->em[0x12].setNoSuspend(0);
        r320_work->em[0x14].setNoSuspend(0);
        r320_work->em[0x15].setNoSuspend(0);
        r320_work->em[0x16].setNoSuspend(0);
        r320_work->gatling[1]->ang.y = -1.68495f;
        r320_work->gatling[1]->pList->pList->pList->ang.x = 1.08596f;
    }
}

// Area 14: the door to the tower breaks.
static void appear_e()
{
    R320_SAVE_FLAGS |= 0x00080000;
    deleteFarEm(8);
    scr_set();
    {
        Vec pos = {72593.0f, 9390.0f, 7073.0f};
        cEmDoor* door;

        getRoomEtcDoor(2, &door, 1);
        if (door) {
            door->setBreak(&pos);
        }
    }
    emset(0x17, 0x28);
}

// End of the area-15 cut: camera back, SceEventEnd, Status_flg[2] off, em[0x1A] may suspend.
static void appear_f_exit()
{
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    r320_work->em[0x1A].setNoSuspend(0);
}

// Area 15: the helicopter moves on, the enemies left behind are cleared and the tower group set.
static void appear_f()
{
    cEm3d* heri;
    SceAtWork* at;
    u32 i;

    R320_SAVE_FLAGS |= 0x00040000;
    heri = (cEm3d*) r320_work->heri.getPtr();
    if (heri) {
        heri->setPatrolPos(&r320_posB[8]);
    }
    at = SceAtPtr(0x13);
    for (i = 0; i <= 0x38; i++) {
        if (r320_work->em[i].isAlive() == 1) {
            if (AreaHitCheck(&at->area, &r320_work->em[i].getPtr()->pos)) {
                r320_work->em[i].destroy();
            }
        }
    }
    deleteFarEm(3);
    for (i = 0; i <= 0x38; i++) {
        if (r320_work->em[i].isAlive() == 1) {
            if (r320_work->em[i].ckResetEnable()) {
                r320_work->em[i].destroy();
            }
        }
    }
    for (i = 0; i <= 0x38; i++) {
        if (r320_work->em[i].isActive() == 1) {
            r320_work->em[i].setGoto(&r320_posB[14], 0xC);
        }
    }
    scr_set();
    Gatling2_set();
    emset(0x1A, 0x29);
    emset(0x38, 0x61);
    emset(0x2B, 0x40);
    emset(0x2C, 0x41);
    emset(0x2D, 0x42);
    r320_work->em[0x2B].setGoto(&r320_posB[14], 0xC);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventStart(1);
    r320_work->em[0x1A].setNoSuspend(1);
    SceSetEventCancel(1, (TaskFunc) appear_f_exit, 0, -1, 1);
    CamCtrl.CutCall(0x1D);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    appear_f_exit();
}

// Area 20: the three gatling towers of the last yard.
static void appear_g()
{
    cEmGanado* em;
    u32 i;

    if (SceCheckEventStart()) {
        SceAtSetEnable(0x14, 0);
        R320_SAVE_FLAGS |= 0x00020000;
        for (i = 0; i <= 0x38; i++) {
            if (i == 0x35 || i == 0x33) {
                continue;
            }
            if (r320_work->em[i].isAlive() == 1) {
                r320_work->em[i].destroy();
            }
        }
        scr_set();
        Gatling2_set();
        gate1_close();
        emset(0x25, 0x30);
        em = (cEmGanado*) r320_work->em[0x25].getPtr();
        if (em) {
            em->flag |= 1;
            if (r320_work->gatling[2]) {
                em->setGatling(r320_work->gatling[2], ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25),
                               ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
            }
        }
        emset(0x1B, 0x2F);
        em = (cEmGanado*) r320_work->em[0x1B].getPtr();
        if (em) {
            em->flag |= 1;
            if (r320_work->gatling[3]) {
                em->setGatling(r320_work->gatling[3], ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25),
                               ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
            }
        }
        emset(0x20, 0x2E);
        em = (cEmGanado*) r320_work->em[0x20].getPtr();
        if (em) {
            em->flag |= 1;
            if (r320_work->gatling[4]) {
                em->setGatling(r320_work->gatling[4], ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25),
                               ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
            }
        }
        emset(0x1C, 0x38);
        emset(0x1D, 0x39);
        emset(0x21, 0x2A);
        emset(0x22, 0x2B);
        emset(0x1F, 0x3C);
        r320_work->em[0x20].setNoSuspend(1);
        r320_work->em[0x1B].setNoSuspend(1);
        r320_work->em[0x25].setNoSuspend(1);
        r320_work->em[0x1C].setNoSuspend(1);
        r320_work->em[0x1D].setNoSuspend(1);
        r320_work->em[0x21].setNoSuspend(1);
        r320_work->em[0x22].setNoSuspend(1);
        SceEventStart(1);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        CamCtrl.CutCall(0x1B);
        SceSleep(0xF);
        if (r320_work->gatling[2]) {
            r320_work->gatling[2]->setFire();
        }
        if (r320_work->gatling[3]) {
            r320_work->gatling[3]->setFire();
        }
        if (r320_work->gatling[4]) {
            r320_work->gatling[4]->setFire();
        }
        r320_work->fireTimer = -0x4B;
        SceSleep(0x4B);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
        if (r320_work->gatling[2]) {
            r320_work->gatling[2]->ang.y = 0.0157f;
        }
        if (r320_work->gatling[3]) {
            r320_work->gatling[3]->ang.y = 1.2666f;
        }
        if (r320_work->gatling[4]) {
            r320_work->gatling[4]->ang.y = 0.7414f;
        }
        r320_work->em[0x20].setNoSuspend(0);
        r320_work->em[0x1B].setNoSuspend(0);
        r320_work->em[0x25].setNoSuspend(0);
        r320_work->em[0x1C].setNoSuspend(0);
        r320_work->em[0x1D].setNoSuspend(0);
        r320_work->em[0x21].setNoSuspend(0);
        r320_work->em[0x22].setNoSuspend(0);
        SceSleep(0xF);
        if (r320_work->gatling[2]) {
            r320_work->gatling[2]->stopFire();
        }
        if (r320_work->gatling[3]) {
            r320_work->gatling[3]->stopFire();
        }
        if (r320_work->gatling[4]) {
            r320_work->gatling[4]->stopFire();
        }
        r320_work->fireTimer = 0;
        r320_work->em[0x22].setGoto(&pPL->pos, 0xC);
    }
}

// Turns the lever object's handle (its parts' z rotation) from `from` to `to`, accelerating.
void reva_common_move(cObj* obj, f32 from, f32 to)
{
    f32 spd = 0.0f;
    f32* rz;
    f32 accel;
    int up;

    obj->be_flag |= 0x20;
    SndCall(6, 0xF, &obj->pos, 0, 0, 0);
    rz = &obj->pList->ang.z;
    accel = r320_revaAccel;
    while (1) {
        if (to > from) {
            *rz += spd;
            up = 1;
        } else {
            *rz -= spd;
            up = 0;
        }
        if (spd >= 0.0f) {
            if (up ? *rz < to : *rz > to) {
                spd += accel * 1.85f;
            } else {
                *rz = to;
                break;
            }
        }
        SceSleep(1);
    }
}

// Lever B (object 0x2F) swings from -1.24 to -0.59 rad.
static void reva_b_down()
{
    reva_common_move(SmdGetObjPtr(0x2F), -1.24f, -0.59f);
}

// Lever C (object 0x30) swings from -1.24 to -0.59 rad.
static void reva_c_down()
{
    reva_common_move(SmdGetObjPtr(0x30), -1.24f, -0.59f);
}

// The three gatling towers of the last yard (skipped once the first exists).
void Gatling2_set()
{
    Vec pos;
    Vec rot;

    if (r320_work->gatling[2] == 0) {
        if ((R320_SAVE_FLAGS & 0x04000000) == 0) {
            pos.x = 42195.0f;
            pos.y = 9557.0f;
            pos.z = -10542.0f;
            rot.x = 0.0f;
            rot.y = 0.21f;
            rot.z = 0.0f;
            r320_work->gatling[2] = SetObjGatling(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), &pos, &rot);
            if (r320_work->gatling[2]) {
                r320_work->gatling[2]->setEat(ROOM_ARC_PTR(pG->pRoom, 0x12), 1);
                r320_work->gatling[2]->setNoSuspend(1);
                r320_work->gatling[2]->setMaxRot(1.5707964f);
            }
        }
        if ((R320_SAVE_FLAGS & 0x02000000) == 0) {
            pos.x = 36452.0f;
            pos.y = 10493.0f;
            pos.z = 70.0f;
            rot.x = 0.0f;
            rot.y = 1.72f;
            rot.z = 0.0f;
            r320_work->gatling[3] = SetObjGatling(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), &pos, &rot);
            if (r320_work->gatling[3]) {
                r320_work->gatling[3]->setEat(ROOM_ARC_PTR(pG->pRoom, 0x12), 1);
                r320_work->gatling[3]->setNoSuspend(1);
                r320_work->gatling[3]->setMaxRot(1.5707964f);
            }
        }
        if ((R320_SAVE_FLAGS & 0x01000000) == 0) {
            pos.x = 30977.0f;
            pos.y = 6697.0f;
            pos.z = -10530.0f;
            rot.x = 0.0f;
            rot.y = 1.15f;
            rot.z = 0.0f;
            r320_work->gatling[4] = SetObjGatling(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), &pos, &rot);
            if (r320_work->gatling[4]) {
                r320_work->gatling[4]->setEat(ROOM_ARC_PTR(pG->pRoom, 0x12), 1);
                r320_work->gatling[4]->setNoSuspend(1);
            }
        }
    }
}

// The first gate's lever: opens gate 1 and saves.
static void switch1_move()
{
    int zero = 0;

    R320_SAVE_FLAGS |= 0x00010000;
    SceEventStart(1);
    CamCtrl.CutCall(0x12);
    reva_common_move(SmdGetObjPtr(0x2E), -0.59f, -1.24f);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM01, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM01, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM01, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 8, 1, ESP_CORE_KIND_ROOM01, (void*) zero, (void*) zero);
    SndCall(6, 0x14, 0, 0, 0, 0);
    SceSleep(0xF);
    gate1_open(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    R320_SAVE_FLAGS |= 0x2000;
    GameSave.save(pSaveData, -1);
    SceAtDataSet_exec(0x2B, 0x12, 0, (TaskFunc) em_lastset, 0, 1);
}

// The second gate's two levers: the gate opens once both are pulled.
static void switch2_move()
{
    int zero = 0;

    R320_SAVE_FLAGS |= 0x8000;
    SceEventStart(1);
    CamCtrl.CutCall(0x13);
    reva_common_move(SmdGetObjPtr(0x2F), -0.59f, -1.24f);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM02, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM02, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM02, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM02, (void*) zero, (void*) zero);
    SndCall(6, 0x14, 0, 0, 0, 0);
    SceSleep(0xF);
    if (R320_SAVE_FLAGS & 0x4000) {
        gate2_open();
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The third lever: save bit 0x4000, camera cut 0x14 while the lever (0x30) swings back with its effect
// and SE, then the following gate.
static void switch3_move()
{
    int zero = 0;

    R320_SAVE_FLAGS |= 0x4000;
    SceEventStart(1);
    CamCtrl.CutCall(0x14);
    reva_common_move(SmdGetObjPtr(0x30), -0.59f, -1.24f);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM03, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM03, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM03, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xC, 1, ESP_CORE_KIND_ROOM03, (void*) zero, (void*) zero);
    SndCall(6, 0x14, 0, 0, 0, 0);
    SceSleep(0xF);
    if (R320_SAVE_FLAGS & 0x8000) {
        gate2_open();
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The player slides down the cable to the yard (area 30).
static void slide_move()
{
    cPlayer* pl = pPL;
    cEm3d* heri;
    u32 frames;
    u32 i;

    heri = (cEm3d*) r320_work->heri.getPtr();
    if (heri) {
        heri->setPatrolPos(&r320_posB[7]);
    }
    pl->beginAction();
    AtariOffRaw(&pPL->atari, 0xFEFF);
    AtariOffRaw(&pPL->atari, 0xFDFF);
    pPL->atari.setPriority(1);
    pPL->dmg.set(0, 0x80);
    pl->be_flag &= ~0x10;
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    PlSetHand(1, 0);
    pPL->setPos(58200.0f, 16588.34f, -11855.78f);
    pPL->setAng(0.0f, 0.0f, 0.0f);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x28), 0, 0, 0x201, 0);
    r320_work->smd->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2B), 0xA, 0, 1, 0);
    r320_work->smd->Motion.Seq_speed = 1.0f;
    SndCall(6, 0x18, 0, 0, 0, 0);
    frames = (u32) MotionGetMaxFrame(&pPL->Motion);
    for (i = 0; i < frames; i++) {
        if (i > 0x3C && i < frames - 0x1E) {
            PlWepHitCheck2(0, &pPL->pos, &pPL->pos, 0x13, 2, 1500.0f);
        }
        SceSleep(1);
        if (i == 0x50) {
            SndCall(6, 0x19, 0, 0, 0, 0);
        }
    }
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->Wep->setTrans(1, 0);
    pl->endAction(5);
    pPL->dmg.clear();
    AtariOnRaw(&pPL->atari, 0x100);
    AtariOnRaw(&pPL->atari, 0x200);
    pPL->atari.setPriority(0);
    pl->be_flag |= 0x10;
}

// The first gate rises (no = 1: set open at once).
void gate1_open(int no)
{
    cObj* o;

    KyfFlagOn(pG, KYF_ST1_20);
    scr_set();
    o = SmdGetObjPtr(0x2A);
    o->be_flag |= 0x20;
    if (no == 0) {
        SceEventStart(1);
        CamCtrl.CutCall(0x15);
        SndCall(6, 0x10, &o->pos, 0, 0, 0);
        while (o->pos.y < 10961.0f) {
            o->pos.y += 100.0f;
            SceSleep(1);
        }
        SndCall(6, 0x11, &o->pos, 0, 0, 0);
        SceSleep(0xF);
        CamCtrl.Comeback(0);
        SceEventEnd(0);
    }
    o->pos.y = 10961.0f;
    SceAtSetEnable(0x1F, 0);
    SceAtSetEnable(0x27, 0);
}

// Gate 1 (object 0x2A) drops 100 units a frame to y 8124 under camera cut 0x17 with SE; the player frozen meanwhile.
void gate1_close()
{
    cObj* o;

    SceEventStart(0);
    pPL->setNoSuspend(1);
    CamCtrl.CutCall(0x17);
    o = SmdGetObjPtr(0x2A);
    o->be_flag |= 0x20;
    SndCall(6, 0x12, &o->pos, 0, 0, 0);
    while (o->pos.y > 8124.0f) {
        o->pos.y -= 100.0f;
        SceSleep(1);
    }
    SndCall(6, 0x13, &o->pos, 0, 0, 0);
    SceSleep(0xF);
    SceAtSetEnable(0x1F, 1);
    pPL->setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Gate 2 (object 0x2B) rises 100 units a frame to y 10961 under camera cut 0x16 with SE; Key_flg[1]
// 0x00200000 and Room_flg[1] 0x08000000.
void gate2_open()
{
    cObj* o;

    KyfFlagOn(pG, KYF_ST1_19);
    RmfFlagOn(pG, RMF_GATE2_OPEN);
    SceEventStart(1);
    CamCtrl.CutCall(0x16);
    o = SmdGetObjPtr(0x2B);
    o->be_flag |= 0x20;
    SndCall(6, 0x10, &o->pos, 0, 0, 0);
    while (o->pos.y < 10961.0f) {
        o->pos.y += 100.0f;
        SceSleep(1);
    }
    SndCall(6, 0x11, &o->pos, 0, 0, 0);
    SceSleep(0xF);
    SceAtSetEnable(0x20, 0);
    SceAtSetEnable(0x31, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The second gate closes again (both levers reset).
static void gate2_close()
{
    cObj* o;

    RmfFlagOn(pG, RMF_GATE2_CLOSE);
    if (r320_work->gatling[2]) {
        r320_work->gatling[2]->be_flag &= ~0x20;
    }
    if (r320_work->gatling[3]) {
        r320_work->gatling[3]->be_flag &= ~0x20;
    }
    if (r320_work->gatling[4]) {
        r320_work->gatling[4]->be_flag &= ~0x20;
    }
    SceEventStart(1);
    CamCtrl.CutCall(0x1F);
    SceExec(0x12, (TaskFunc) reva_b_down, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) reva_c_down, 0, 0, 2, 0);
    SceSleep(0xF);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM02, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM02, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM02, 0);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM03, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM03, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM03, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 9, 1, ESP_CORE_KIND_ROOM02, 0, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_ROOM03, 0, 0);
    SndCall(6, 0x14, 0, 0, 0, 0);
    o = SmdGetObjPtr(0x2B);
    o->be_flag |= 0x20;
    SndCall(6, 0x10, &o->pos, 0, 0, 0);
    while (o->pos.y > 8184.0f) {
        o->pos.y -= 100.0f;
        SceSleep(1);
    }
    o->pos.y = 8184.0f;
    SndCall(6, 0x11, &o->pos, 0, 0, 0);
    SceSleep(0xF);
    SceAtSetEnable(0x20, 1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (r320_work->gatling[2]) {
        r320_work->gatling[2]->be_flag |= 0x20;
    }
    if (r320_work->gatling[3]) {
        r320_work->gatling[3]->be_flag |= 0x20;
    }
    if (r320_work->gatling[4]) {
        r320_work->gatling[4]->be_flag |= 0x20;
    }
}

// The helicopter's rocket attacks on the gun towers (targets 0..6, destroy_N follows the hit).
static void attack_heri0()
{
    cEm3d* heri = (cEm3d*) r320_work->heri.getPtr();

    if (heri) {
        heri->setTarget(0, 12000.0f);
    }
    SceExec(0x12, (TaskFunc) destroy_0, 0, 0, 2, 0);
    addMisileUseNum();
}

// Rocket attack on gun tower 1: the helicopter targets it (setTarget(1, 24000)) after the common setup.
static void attack_heri1()
{
    if ((R320_SAVE_FLAGS & 0x40000000) == 0) {
        attack_heri0();
    } else {
        cEm3d* heri = (cEm3d*) r320_work->heri.getPtr();

        if (heri) {
            heri->setTarget(1, 24000.0f);
        }
        RmfFlagOn(pG, RMF_TARGET1_START);
        SceExec(0x12, (TaskFunc) destroy_1, 0, 0, 2, 0);
        addMisileUseNum();
    }
}

// Rocket attack on gun tower 2.
static void attack_heri2()
{
    if ((R320_SAVE_FLAGS & 0x40000000) == 0) {
        attack_heri0();
    } else {
        cEm3d* heri = (cEm3d*) r320_work->heri.getPtr();

        if (heri) {
            heri->setTarget(2, 24000.0f);
        }
        SceExec(0x12, (TaskFunc) destroy_2, 0, 0, 2, 0);
        addMisileUseNum();
    }
}

// Rocket attack on target 3.
static void attack_heri3()
{
    cEm3d* heri = (cEm3d*) r320_work->heri.getPtr();

    if (heri) {
        heri->setTarget(3, 24000.0f);
    }
    SceExec(0x12, (TaskFunc) destroy_3, 0, 0, 2, 0);
    addMisileUseNum();
}

// Rocket attack on target 4 (with its own camera and enemy handling).
static void attack_heri4()
{
    cEm3d* heri = (cEm3d*) r320_work->heri.getPtr();

    if (heri) {
        if ((R320_SAVE_FLAGS & 0x04000000) == 0 && (R320_SAVE_FLAGS & 0x02000000) == 0) {
            if (fRand0_1() > 0.5f) {
                heri->setTarget(4, 3000.0f);
                SceExec(0x12, (TaskFunc) destroy_4, 0, 0, 2, 0);
            } else {
                heri->setTarget(5, 3000.0f);
                SceExec(0x12, (TaskFunc) destroy_5, 0, 0, 2, 0);
            }
        } else if ((R320_SAVE_FLAGS & 0x04000000) == 0) {
            heri->setTarget(4, 3000.0f);
            SceExec(0x12, (TaskFunc) destroy_4, 0, 0, 2, 0);
        } else if ((R320_SAVE_FLAGS & 0x02000000) == 0) {
            heri->setTarget(5, 12000.0f);
            SceExec(0x12, (TaskFunc) destroy_5, 0, 0, 2, 0);
        } else if ((R320_SAVE_FLAGS & 0x01000000) == 0) {
            heri->setTarget(6, 5000.0f);
            SceExec(0x12, (TaskFunc) destroy_6, 0, 0, 2, 0);
        } else {
            return;
        }
        addMisileUseNum();
    }
}

// The rocket hits: each gun tower's collision goes, its explosion effect plays under a camera cut
// and the enemies around it take the blast (PlWepHitCheck2 type 0x12).
static void destroy_0()
{
    cEm3d* heri = (cEm3d*) r320_work->heri.getPtr();

    if (heri) {
        while (heri->ckMissileFire() == 0) {
            SceSleep(1);
        }
        r320_work->heri.setNoSuspend(1);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceEventStart(1);
        CamCtrl.CutCall(8);
        while (!RmfFlagChk(pG, RMF_TARGET_DESTROY)) {
            SceSleep(1);
        }
        int zero = 0;
        RmfFlagOff(pG, RMF_TARGET_DESTROY);
        R320_SAVE_FLAGS |= 0x40000000;
        EatMgr.destroy(r320_work->sat[10]);
        PlWepHitCheck2(0, &r320_posA[0], &r320_posA[0], 0x12, 3, 7000.0f);
        Vec pos = {59934.0f, 11941.0f, 27319.0f};
        Vec rot = {0.0f, -0.4537856f, 0.0f};
        EstSet(0, -1, &pos, &rot, EFF_ROOM, 3, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
        SmdSetTrans(0x1F, 0);
        SceSleep(0x1E);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        r320_work->heri.setNoSuspend(0);
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
        r320_work->gatling[0]->setBreak();
        r320_work->em[0].destroy();
        r320_work->heriWait = 0x96;
    }
}

// Gun tower 1 destroyed: its collision off, explosion effect under a camera cut, blast damage around it, save bit set.
static void destroy_1()
{
    while (!RmfFlagChk(pG, RMF_TARGET_DESTROY)) {
        SceSleep(1);
    }
    RmfFlagOff(pG, RMF_TARGET_DESTROY);
    R320_SAVE_FLAGS |= 0x20000000;
    EatMgr.destroy(r320_work->sat[8]);
    SatMgr.destroy(r320_work->sat[1]);
    PlWepHitCheck2(0, &r320_posA[1], &r320_posA[1], 0x12, 3, 7000.0f);
    PlWepHitCheck2(0, &r320_posA[2], &r320_posA[2], 0x12, 3, 7000.0f);
    SceSleep(0xF);
    r320_work->heri.setNoSuspend(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventStart(1);
    CamCtrl.CutCall(0xA);
    Vec pos = {70721.0f, 12357.0f, 27023.0f};
    Vec rot = {0.0f, 4.3633232f, 0.0f};
    EstSet(0, -1, &pos, &rot, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, 0, 0);
    Vec pos2 = {70717.0f, 12400.0f, 27019.0f};
    Vec rot2 = {0.0f, -1.9198622f, 0.0f};
    EstSet(0, -1, &pos2, &rot2, EFF_ROOM, 2, 1, ESP_CORE_KIND_NONE, 0, 0);
    SmdSetTrans(0x15, 0);
    SceSleep(0x1E);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r320_work->heri.setNoSuspend(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    r320_work->heriWait = 0x96;
    SceAtDataSet_exec(0x30, 0x12, 0, (TaskFunc) appear_b, 0, 1);
}

// Gun tower 2 destroyed (as destroy_1).
static void destroy_2()
{
    SceAtWork* at;
    SceAtWork* w;

    while (!RmfFlagChk(pG, RMF_TARGET_DESTROY)) {
        SceSleep(1);
    }
    int zero = 0;
    RmfFlagOff(pG, RMF_TARGET_DESTROY);
    R320_SAVE_FLAGS |= 0x10000000;
    EatMgr.destroy(r320_work->sat[9]);
    PlWepHitCheck2(0, &r320_posA[3], &r320_posA[3], 0x12, 3, 7000.0f);
    PlWepHitCheck2(0, &r320_posA[4], &r320_posA[4], 0x12, 3, 7000.0f);
    SceSleep(0xF);
    r320_work->heri.setNoSuspend(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventStart(1);
    CamCtrl.CutCall(9);
    Vec pos = {59514.0f, 11200.0f, 16319.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    EstSet(0, -1, &pos, &rot, EFF_ROOM, 4, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    SmdSetTrans(0x21, 0);
    SceSleep(0x1E);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SatMgr.destroy(r320_work->sat[0]);
    SceAtSetEnable(0x12, 0);
    SceAtSetEnable(0x26, 1);
    at = SceAtPtr(0x2F);
    if (AreaHitCheck(&at->area, &pPL->pos)) {
        pPL->setPos(58557.0f, 11819.0f, 13270.0f);
        CamCtrl.Comeback(0);
    }
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (w->type == 3) {
            if (AreaHitCheck(&at->area, &w->dstPos)) {
                SceAtSetEnable(w->no, 0);
            }
        }
    }
    r320_work->heri.setNoSuspend(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    r320_work->heriWait = 0x96;
}

// Target 3 destroyed (as destroy_1).
static void destroy_3()
{
    cEmDoor* door;

    while (!RmfFlagChk(pG, RMF_TARGET_DESTROY)) {
        SceSleep(1);
    }
    int zero = 0;
    RmfFlagOff(pG, RMF_TARGET_DESTROY);
    R320_SAVE_FLAGS |= 0x08000000;
    EatMgr.destroy(r320_work->sat[4]);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
    PlWepHitCheck2(0, &r320_posA[5], &r320_posA[5], 0x12, 3, 7000.0f);
    PlWepHitCheck2(0, &r320_posA[6], &r320_posA[6], 0x12, 3, 7000.0f);
    SceSleep(0xF);
    getRoomEtcDoor(2, &door, 1);
    if (door) {
        door->setBreak(&pPL->pos);
    }
    r320_work->heri.setNoSuspend(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventStart(1);
    CamCtrl.CutCall(0xB);
    Vec pos = {79324.0f, 15600.0f, 4458.0f};
    Vec rot = {0.0f, 0.34906584f, 0.0f};
    EstSet(0, -1, &pos, &rot, EFF_ROOM, 5, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    SmdSetTrans(9, 0);
    SmdSetTrans(0x10, 0);
    SceSleep(0x1E);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r320_work->heri.setNoSuspend(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    r320_work->heriWait = 0x96;
}

// Target 4 destroyed (as destroy_1).
static void destroy_4()
{
    RmfFlagOn(pG, RMF_TARGET4_START);
    while (!RmfFlagChk(pG, RMF_TARGET_DESTROY)) {
        SceSleep(1);
    }
    int zero = 0;
    RmfFlagOff(pG, RMF_TARGET_DESTROY);
    R320_SAVE_FLAGS |= 0x04000000;
    EatMgr.destroy(r320_work->sat[5]);
    SceSleep(0xF);
    r320_work->heri.setNoSuspend(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventStart(1);
    CamCtrl.CutCall(0x1A);
    Vec pos = {42666.0f, 9445.0f, -14165.0f};
    Vec rot = {0.0f, -0.41887903f, 0.0f};
    EstSet(0, -1, &pos, &rot, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    PlWepHitCheck2(0, &pos, &pos, 0x12, 3, 7000.0f);
    SmdSetTrans(0x16, 0);
    SceSleep(0x1E);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r320_work->heri.setNoSuspend(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SatMgr.destroy(r320_work->sat[2]);
    SceAtSetEnable(0x28, 1);
    r320_work->heriWait = 0x96;
    if (AreaHitCheck(&SceAtPtr(0x34)->area, &pPL->pos)) {
        pPL->setPos(45469.0f, 9538.0f, -12354.0f);
        CamCtrl.Comeback(0);
    }
    if (RmfFlagChk(pG, RMF_NO_SHOOT3)) {
        emset(0xB, 0x5B);
    }
}

// Target 5 destroyed (as destroy_1).
static void destroy_5()
{
    RmfFlagOn(pG, RMF_TARGET5_START);
    while (!RmfFlagChk(pG, RMF_TARGET_DESTROY)) {
        SceSleep(1);
    }
    int zero = 0;
    RmfFlagOff(pG, RMF_TARGET_DESTROY);
    R320_SAVE_FLAGS |= 0x02000000;
    EatMgr.destroy(r320_work->sat[7]);
    SceSleep(0xF);
    SceEventStart(1);
    CamCtrl.CutCall(0x18);
    Vec pos = {31723.0f, 10563.0f, 2719.0f};
    Vec rot = {0.0f, -4.2184606f, 0.0f};
    EstSet(0, -1, &pos, &rot, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    PlWepHitCheck2(0, &pos, &pos, 0x12, 3, 7000.0f);
    SmdSetTrans(0x17, 0);
    SceSleep(0x1E);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SatMgr.destroy(r320_work->sat[3]);
    SceAtSetEnable(0x29, 1);
    if (AreaHitCheck(&SceAtPtr(0x35)->area, &pPL->pos)) {
        pPL->setPos(34660.0f, 10469.0f, 5587.0f);
        CamCtrl.Comeback(0);
    }
    r320_work->heriWait = 0x96;
}

// Target 6 destroyed (as destroy_1); the last one frees the way to the tower.
static void destroy_6()
{
    RmfFlagOn(pG, RMF_TARGET6_START);
    while (!RmfFlagChk(pG, RMF_TARGET_DESTROY)) {
        SceSleep(1);
    }
    int zero = 0;
    RmfFlagOff(pG, RMF_TARGET_DESTROY);
    R320_SAVE_FLAGS |= 0x01000000;
    EatMgr.destroy(r320_work->sat[6]);
    SceSleep(0xF);
    SceEventStart(1);
    CamCtrl.CutCall(0x19);
    Vec pos = {25412.0f, 13235.0f, -14502.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    EstSet(0, -1, &pos, &rot, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    PlWepHitCheck2(0, &pos, &pos, 0x12, 3, 7000.0f);
    SmdSetTrans(0x18, 0);
    SceSleep(0x1E);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r320_work->heriWait = 0x96;
}

// Event script hooks: s00 is the helicopter's arrival, s01 its landing at the tower.
static void Evt_R320S00_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;

    switch (e->FuncType) {
    case 0:
        SmdSetTrans(0x25, 1);
        SmdSetTrans(0x27, 1);
        EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM04, 0);
        EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM04, 0);
        break;
    case 1: {
        void* mod;

        if (e->NowCut == 0 && e->NowFrame == 1) {
            EvtMgr.EvtReadAram("event/evd/r320s01.evd", 0, 0, 0, 0);
            RmfFlagOn(pG, RMF_EVTR320S01_READ);
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            SmdSetTrans(0x27, 0);
            if (e->GetMod(&mod, "evm7900", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 1;
            }
            if (e->GetMod(&mod, "evm8000", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 1;
            }
            if (e->GetMod(&mod, "evma000", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 1;
            }
            if (e->GetMod(&mod, "evm3200", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 0x40;
            }
            obj = SmdGetObjPtr(0x22);
            if (obj) {
                SmdSetTrans(0x22, 1);
                e->SetMod("scr0000", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&rot);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            SmdSetTrans(0x28, 0);
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            SmdSetTrans(3, 0);
            SmdSetTrans(0x15, 0);
            SmdSetTrans(0x16, 0);
            SmdSetTrans(0x24, 0);
            SmdSetTrans(8, 0);
            SmdSetTrans(0xA, 0);
            SmdSetTrans(0x12, 0);
            SmdSetTrans(0x17, 0);
            SmdSetTrans(0x18, 0);
        }
        if (e->NowCut == 7 && e->NowFrame == 0x78) {
            int skip = EvtSkipCk(e);

            if (skip == 0) {
                FadeSetW(2, e->MaxFrame - 0x78, 0, 0);
            }
        }
        break;
    }
    case 2: {
        SmdWork* w = SmdGetWorkPtr(0x22);

        obj = SmdGetObjPtr(0x22);
        if (obj && w) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        SmdSetTrans(0x22, 0);
        SmdSetTrans(0x28, 1);
        SmdSetTrans(1, 1);
        SmdSetTrans(2, 1);
        SmdSetTrans(3, 1);
        SmdSetTrans(0x15, 1);
        SmdSetTrans(0x16, 1);
        SmdSetTrans(0x24, 1);
        SmdSetTrans(8, 1);
        SmdSetTrans(0xA, 1);
        SmdSetTrans(0x12, 1);
        SmdSetTrans(0x17, 1);
        SmdSetTrans(0x18, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xE, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        break;
    }
    case 3:
        RmfFlagOn(pG, RMF_EVENT_CANCEL);
        if (RmfFlagChk(pG, RMF_EVTR320S01_READ)) {
            EvtMgr.EvtFree("event/evd/r320s01.evd");
        }
        break;
    }
}

// Event r320s01 callback (the helicopter lands at the tower): props 0x25/0x27 shown for the event, the
// evm7900 / evm8000 models per cut; the end restores the yard.
static void Evt_R320S01_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;

    switch (e->FuncType) {
    case 0:
        SmdSetTrans(0x25, 1);
        SmdSetTrans(0x27, 1);
        EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM04, 0);
        EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM04, 0);
        break;
    case 1: {
        void* mod;

        if (e->NowCut == 0 && e->NowFrame == 0) {
            SmdSetTrans(0x27, 0);
            if (e->GetMod(&mod, "evm7900", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 1;
            }
            if (e->GetMod(&mod, "evm8000", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 1;
            }
            if (e->GetMod(&mod, "evma000", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 1;
            }
            if (e->GetMod(&mod, "evm3200", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 0x40;
            }
            obj = SmdGetObjPtr(0x22);
            if (obj) {
                SmdSetTrans(0x22, 1);
                e->SetMod("scr0000", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&rot);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            {
                int skip = EvtSkipCk(e);

                if (skip == 0) {
                    FadeSetW(0x80000002, 0x1E, 0, 0);
                }
            }
        }
        switch (e->NowCut) {
        case 5:
        case 6:
        case 7:
        case 8:
        case 0xF:
            if (e->NowFrame == 0) {
                SmdSetTrans(0x28, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                SmdSetTrans(0x28, 1);
            }
            break;
        }
        if (e->NowCut == 0xF) {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x34, 0);
            }
        } else {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x34, 1);
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                SmdSetTrans(3, 0);
                SmdSetTrans(0x15, 0);
                SmdSetTrans(0x16, 0);
                SmdSetTrans(0x24, 0);
                SmdSetTrans(8, 0);
                SmdSetTrans(9, 0);
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x12, 0);
                SmdSetTrans(0x17, 0);
                SmdSetTrans(0x18, 0);
            }
            break;
        case 0xF:
            if (e->NowFrame == 0) {
                SmdSetTrans(1, 0);
                SmdSetTrans(2, 0);
                SmdSetTrans(0x25, 1);
            }
            if (e->NowFrame > 5) {
                if (e->GetMod(&mod, "evm7900", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "evm8000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "evma000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            } else {
                if (e->GetMod(&mod, "evm7900", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "evm8000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "evma000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
            if (e->NowFrame == 0x1E) {
                SmdSetTrans(0x25, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                SmdSetTrans(1, 1);
                SmdSetTrans(2, 1);
            }
            break;
        }
        break;
    }
    case 2: {
        SmdWork* w = SmdGetWorkPtr(0x22);

        obj = SmdGetObjPtr(0x22);
        if (obj && w) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        SmdSetTrans(0x22, 0);
        SmdSetTrans(0x28, 1);
        SmdSetTrans(1, 1);
        SmdSetTrans(2, 1);
        SmdSetTrans(3, 1);
        SmdSetTrans(0x15, 1);
        SmdSetTrans(0x16, 1);
        SmdSetTrans(0x24, 1);
        SmdSetTrans(8, 1);
        SmdSetTrans(9, 1);
        SmdSetTrans(0xA, 1);
        SmdSetTrans(0x12, 1);
        SmdSetTrans(0x17, 1);
        SmdSetTrans(0x18, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xE, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        break;
    }
    }
}

// Destroys the active enemies farthest from the player until `keep` are left.
void deleteFarEm(int keep)
{
    r320_work->emAlive = SceCountEmAlive(0x10, 0x20);
    if (r320_work->emAlive > keep) {
        int cnt = r320_work->emAlive - keep;
        int k;

        for (k = 0; k < cnt; k++) {
            f32 far = 0.0f;
            int best = -1;
            u32 i;

            for (i = 0; i <= 0x38; i++) {
                if (i == 0x20 || i == 0x1B || i == 0x25 || i == 0 || i == 0x12 || i == 0x1A) {
                    continue;
                }
                if (r320_work->em[i].isActive() == 1) {
                    f32 d = (r320_work->em[i].getPtr()->pos.x - pPL->pos.x) * (r320_work->em[i].getPtr()->pos.x - pPL->pos.x)
                            + (r320_work->em[i].getPtr()->pos.y - pPL->pos.y) * (r320_work->em[i].getPtr()->pos.y - pPL->pos.y)
                            + (r320_work->em[i].getPtr()->pos.z - pPL->pos.z) * (r320_work->em[i].getPtr()->pos.z - pPL->pos.z);

                    if (d > 3000.0f && d > far) {
                        far = d;
                        best = i;
                    }
                }
            }
            if (best != -1) {
                r320_work->em[best].destroy();
            }
        }
    }
}

// The last two Ganados of the yard (slots 0x35 / 0x33 from list entries 0x2D / 0x36).
static void em_lastset()
{
    emset(0x35, 0x2D);
    emset(0x33, 0x36);
}

// The tower door opens (area 45): its parts slides aside under a camera cut, then the game saves.
static void door_open()
{
    int zero = 0;
    u32 i;

    KyfFlagOn(pG, KYF_ST1_21);
    R320_SAVE_FLAGS |= 0x200;
    EffectEspDelete(1, ESP_CORE_KIND_ROOM04, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM04, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM04, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x14, 1, ESP_CORE_KIND_ROOM04, (void*) zero, (void*) zero);
    SndCall(6, 0x14, 0, 0, 0, 0);
    SceAtSetEnable(0x2C, 0);
    SceEventStart(1);
    CamCtrl.CutCall(0x25);
    SceSleep(0xF);
    SndCall(6, 0x1A, &SmdGetObjPtr(0x36)->pos, 0, 0, 0);
    SndCall(6, 0x1B, &SmdGetObjPtr(0x36)->pos, 0, 0, 0);
    SmdGetObjPtr(0x36)->be_flag |= 0x20;
    for (i = 0; i <= 0x1D; i++) {
        SmdGetObjPtr(0x36)->pList->pos.x -= 38.0f;
        SceSleep(1);
    }
    SceSleep(0xF);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    R320_SAVE_FLAGS |= 0x2000;
    GameSave.save(pSaveData, -1);
}

// The tower door already open: area 0x2C off, its parts slid aside by 1140, the open-door effect.
static void door_opened()
{
    SceSleep(1);
    SceAtSetEnable(0x2C, 0);
    SmdGetObjPtr(0x36)->be_flag |= 0x20;
    SmdGetObjPtr(0x36)->pList->pos.x -= 1140.0f;
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x14, 1, ESP_CORE_KIND_ROOM04, 0, 0);
}

// The battle stream plays while the player is seen (or until the last yard is reached).
static void r320_StrCheck()
{
    int on = 0;

    for (;;) {
        if (SceCkFindPL(0) == 1 || (R320_SAVE_FLAGS & 0x00020000) == 0) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else {
            if (on == 1) {
                SndRoomStrStop(3);
                on = 0;
            }
        }
        SceSleep(1);
    }
}

// Three frames later: the tower explosion stream 0xEE.
static void tower_explode()
{
    SceSleep(3);
    SndStrReq(1, 0xEE, 0x80000003, 0, 0, 0.0f);
}

// The message at the last yard: the towers stay hidden while the cut plays.
static void last_mes()
{
    if (r320_work->gatling[2]) {
        r320_work->gatling[2]->be_flag &= ~0x20;
    }
    if (r320_work->gatling[3]) {
        r320_work->gatling[3]->be_flag &= ~0x20;
    }
    if (r320_work->gatling[4]) {
        r320_work->gatling[4]->be_flag &= ~0x20;
    }
    SceUpCut(1, 0x1F, -1, 0);
    if (r320_work->gatling[2]) {
        r320_work->gatling[2]->be_flag |= 0x20;
    }
    if (r320_work->gatling[3]) {
        r320_work->gatling[3]->be_flag |= 0x20;
    }
    if (r320_work->gatling[4]) {
        r320_work->gatling[4]->be_flag |= 0x20;
    }
}

// The radio (area 0x33).
static void musen_exec()
{
    R320_SAVE_FLAGS |= 0x80;
    OpeSetOpenTerm(0x15, 0.0f, 0.0f, 0.0f, 0.0f);
}
