#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "pl_npc.h"
#include "etc_model.h"
#include "esp.h"
#include "est.h"
#include "player.h"
#include "pl_sub.h"
#include "motion.h"
#include "math_sub.h"
#include "act_btn.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "snd.h"
#include "flr_at.h"
#include "rnd.h"

// Room 1-13 (D:/Bio4/Prog/r113.cpp): the village house in the storm; r103's cesspit / shelves /
// sub-mission target, the closet Ashley hides in, the ride-on-shoulder event and the thunder task.

struct R113Work {
    u32 x0;
    u32 eff;      // 0x04  EspPullCoreKind() of the file item glow
    u32 strId;    // 0x08  SndStrReq handle of the event stream
};

// r103's cesspit table (r103_initCesspit)
struct R113Cesspit {
    u32 cover;
    u32 lid;
    int itemAt;
    int itemAt2;
    int at10;
    int at14;
    int at18;
};

struct R113Shelf {
    u8 door[2];
};

static R113Work* r113_work;

// The original's .data is 8-aligned (r105 has the same).
asm(".section .data; .balign 8");
static R113Cesspit r113_cesspit = {0x52, 0x53, 0x81, 0x9F, 6, 5, 7};
static R113Shelf r113_shelf0 = {{0x57, 0x58}};
static R113Shelf r113_shelf1 = {{0x59, 0x5A}};
static R113Shelf r113_shelf2 = {{0x5B, 0x5C}};

// Hit effects of attribute type 4
static const AtEffInfo r113_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

// r103.cpp (the same module)
extern "C" void r103_initCesspit(R113Cesspit* c);
extern "C" void r103_setSubMissionTarget(u32 objNo);
extern "C" void r103_openedShelf(R113Shelf* s);
extern "C" void r103_openShelf(R113Shelf* s);

static void r113_getFile();
static void r113_execHide(int mode);
static void r113_EventRideShoulder_end();
static void r113_EventRideShoulder();
static void r113_checkAshleyPos();
static void r113_DoorCheck();
static void r113_ThunderMove();

// Room init (Ashley with Leon, storm): thunder task, rain effects on the player, Status_flg[1] 0x400
// (raining); area 2 = front door check; area 3 = the window shoulder-ride prompt while the door is still
// locked (Key_flg[0] 0x08000000 clear) and Ashley is following (Status_flg[3] 0x04000000); the shared
// r103 cesspit, sub-mission target 8, rack 6 range, three shelf item events (items 0x8E/0x8F/0x8B), the
// closet hide spot (area 4), and the glowing file item at area 0x82 until Item_flg[0] 0x800 is taken.
void R113Init()
{
    cEm* rack;
    void* zero = 0;

#line 73 "D:/Bio4/Prog/r113.cpp"
    r113_work = (R113Work*) MEM_CALLOC(sizeof(R113Work), 1, 0xd);

    SceExec(0x12, (TaskFunc) r113_ThunderMove, 0, 0, SCE_PRIO_DEF_2, 0);
    EstSet(pPL, -1, 0, 0, EFF_PL00, 2, 0x800, ESP_CORE_KIND_NONE, zero, zero);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 4, 0x800, ESP_CORE_KIND_NONE, zero, zero);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 3, 0x800, ESP_CORE_KIND_NONE, zero, zero);
    StaFlagOn(pG, STA_ROOM_RAIN);
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r113_DoorCheck, 0, 1);
    if (!KyfFlagChk(pG, KYF_R113_TO_R11C_DOOR) && StaFlagChk(pG, STA_SUB_ASHLEY)) {
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r113_checkAshleyPos, 0, 1);
    }
    EatMgr.registEffInfo(EAT_ET_ROOM0, (AtEffInfo*) &r113_eff_info);
    SceExec(0x12, (TaskFunc) r103_initCesspit, (int) &r113_cesspit, 0, SCE_PRIO_DEF_2, 0);
    r103_setSubMissionTarget(8);
    if (getRoomEtcRack(6, &rack, 1)) {
        ((cEmRack*) rack)->setRange(0.0f, 3000.0f, 0.0f, 3000.0f);
    }
    SceSetItemEvent(8, 0x8E, 0, 0xA, (void (*)(int)) r103_openShelf, (void (*)(int)) r103_openedShelf, (int) &r113_shelf0, 0);
    SceSetItemEvent(9, 0x8F, 1, 0xB, (void (*)(int)) r103_openShelf, (void (*)(int)) r103_openedShelf, (int) &r113_shelf1, 0);
    SceSetItemEvent(0xA, 0x8B, 2, 9, (void (*)(int)) r103_openShelf, (void (*)(int)) r103_openedShelf, (int) &r113_shelf2, 0);
    SceAtDataSet_hide(4, r113_execHide);
    FlrAtSetDefVal(0, 0, 3);
    if (!ItfFlagChk(pG, ITF_R103_FILE)) {
        r113_work->eff = EspPullCoreKind();
        EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, (u8) r113_work->eff, 0, 0);
        SceAtDataSet_exec(0x82, SCE_LEVEL10, 0, (TaskFunc) r113_getFile, 0, 1);
    }
}

// Per-frame room main: nothing.
void R113Main()
{
}

// The file item was taken: its glow goes away.
static void r113_getFile()
{
    SceAtExecute(0x82);
    EffectEspDelete(0, (u8) r113_work->eff, 0, 0);
    EffectEspgenDelete(0, (u8) r113_work->eff, 0);
    EffectEfmDelete(0, (u8) r113_work->eff, 0);
}

// The closet door swings open (mode 0) or closed while Ashley hides.
static void r113_execHide(int mode)
{
    cObj* door;

    door = SmdGetObjPtr(0x55);
    door->be_flag |= 0x20;
    if (mode == 0) {
        const f32 lim = 1.692f;
        const f32 add = 0.1f;
        f32 spd = 0.0f;

        // A real loop (LOOP_BEG note after the call = sched1 barrier: the entry jump depends on the
        // li r4..r8 but not on li r3, which the call re-sets, so `li r3,6` is issued last as in the
        // target). The asm keeps jump1 from peeling the exit test (asm_noperands in the exit code).
        SndCall(6, 0x14, &pSUB->pos, 0, 0, 0);
        for (;;) {
            door->pList->ang.z += spd;
            asm("" : "+f"(spd)); // COMPILER-DIFF: candidate #9
            spd += add;
            if (door->pList->ang.z > lim) {
                break;
            }
            SceSleep(1);
        }
        door->pList->ang.z = lim;
    } else {
        SndCall(6, 0x13, &pSUB->pos, 0, 0, 0);
        goto close;
    wait_close:
        SceSleep(1);
    close:
        door->pList->ang.z -= 0.2f;
        if (!(door->pList->ang.z < 0.0f)) {
            goto wait_close;
        }
        door->pList->ang.z = 0.0f;
    }
}

// End of the shoulder-ride event: stop its stream, re-arm and run collision area 2 (the door check), SceEventEnd.
static void r113_EventRideShoulder_end()
{
    if (r113_work->strId != 0) {
        SndStrReq(r113_work->strId, 8, 0, 0);
    }
    SceAtDataReset(2);
    SceAtExecute(2);
    SceSleep(1);
    SceEventEnd(0);
}

// Leon lifts Ashley onto his shoulders to reach the window.
static void r113_EventRideShoulder()
{
    int i;

    SceAtSetEnable(3, 0);
    KyfFlagOn(pG, KYF_R113_TO_R11C_DOOR);
    r113_work->strId = 0;
    SceEventStart(0);
    SceSetEventCancel(1, (TaskFunc) r113_EventRideShoulder_end, 0, -1, 1);
    SubCharCtrl(SCC_AUX_MOT, 0);
    r113_work->strId = SndStrReq(1, 0x27, 0x80000003, 0, 0, 0.0f);
    pPL->setNoSuspend(1);
    pSUB->setNoSuspend(1);
    PlSetHand(1, 0);
    {
        Vec pos0 = {3237.0f, 767.0f, -28450.0f};
        Vec pos1 = {0.0f, 0.0f, 662.5f};
        Vec pos2 = {100.49f, 0.0f, -474.38f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        Vec pos;
        Vec pos3;
        Mtx m;
        Vec ang;
        const f32 ryc = 1.5707964f;

        rot.y = -1.5707964f;
        low_RotMatrix(m, &rot);
        TransMatrix(m, &pos0);
        PSMTXMultVec(m, &pos1, &pos);
        {
            // OPEN: the original hoists this constant's `lis` into r17 at the function top and issues
            // the `lfs f31` right before setPos; ours keeps both at the declaration.
            f32 ry = ryc;
            cPlayer* pl = pPL;
            Vec* pa = &ang;

            pl->setPos(&pos);
            ang.x = 0.0f;
            pa->y = ry;
            ang.z = 0.0f;
            pl->setAng(&ang);
        }
        low_RotMatrix(m, &pPL->ang);
        TransMatrix(m, &pos);
        PSMTXMultVec(m, &pos2, &pos3);
        {
            cSubChar* sub = SUB_CHAR();
            Vec* prot = &pPL->ang;

            sub->setPos(&pos3);
            sub->setAng(prot);
        }
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 0xA, 0, 1, 0);
        pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x20), 0xA, 0, 1, 0);
    }
    while (MotionGetState(pPL) != 4) {
        SceSleep(1);
    }
    PlSetHand(0, 0);
    pPL->endEvent(0);
    pPL->setNoSuspend(1);
    SceSleep(60);
    CamCtrl.Comeback(0);
    SceSleep(30);
    SndCall(6, 0xB, 0, 0, 0, 0);
    SceSleep(10);
    SndCall(6, 0, 0, 0, 0, 0);
    SceMesSet(5, 0xF0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    SceSleep(75);
    cMes.Clear();
    r113_work->strId = 0;
    SceSetEventCancel(0, 0, 0, -1, 1);
    r113_EventRideShoulder_end();
}

// The window area: the action button for the shoulder ride while Ashley is with Leon.
static void r113_checkAshleyPos()
{
    if (CheckAshleyActive() == 1) {
        ActBtn.set(ACT_RIDE_SHOULDER, 5, (void*) r113_EventRideShoulder, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_SCE, 0);
    }
}

// The front door: the up-cut until the window event is done.
static void r113_DoorCheck()
{
    if (!KyfFlagChk(pG, KYF_R113_TO_R11C_DOOR)) {
        SceUpCut(0, -1, 0xA, 0);
    } else {
        SceAtExecute(2);
    }
}

// Thunder every 90..235 frames: the flash in the lit area or, with the storm flag clear, the window.
static void r113_ThunderMove()
{
    int cnt;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    for (;;) {
        if (cnt == 0) {
            if (EffGetAreaState(7)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_NONE, 0, 0);
            } else if (!StaFlagChk(pG, STA_CAMERA_IN_ROOM)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 90;
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}
