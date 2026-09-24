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
#include "main.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj00.h"
#include "obj15.h"
#include "em.h"
#include "emhit.h"
#include "emdoor.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "math_sub.h"
#include "read.h"
#include "stage.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "item.h"
#include "sscrn.h"
#include "mes.h"
#include "dvd.h"
#include "datactrl.h"
#include "db_log.h"

// Room 2-09 (D:/Bio4/Prog/r209.cpp): the castle gallery / salon (Salazar's picture gallery): the
// leader Ganado that lures the player through the doors, the gatling and bowgun battles, the four-
// panel picture puzzle that extends the bridge, and the picture behind which the treasure sits.


// Room door driven by the room itself (the seven `cR209Door` records of the work): the four
// balcony doors (0xB3..0xB6, rotating), the salon doors 2/3 and the lift 0xA1 (rising).
class cR209Door {
public:
    u8 mode;        // 0x00  routine (0 wait, 1 open, 2 close)
    u8 step;        // 0x01
    u8 x2;
    u8 x3;
    cObj* obj;      // 0x04
    Vec pos0;       // 0x08  closed position
    f32 rotY0;      // 0x14  closed rot.y
    f32 openH;      // 0x18  height when open
    f32 spd;        // 0x1C  fall speed
    u32 se;         // 0x20  RoomSeCall handle
    int valid;      // 0x24
    u32 id;         // 0x28  scroll object id
    int status;     // 0x2C  0 closed, 1 open, 4 moving
    int timer;      // 0x30
    int flagNo;     // 0x34  door flag index in pG->Scenario_flg
    cSat* sat;      // 0x38  collision of the rotating doors
    cSat* eat;      // 0x3C  event collision of the lift

    void init(u32 id);
    void move();
    void wait();
    void open();
    void close();
    void setOpen();
    void setClose();
    void setOpened();
    void setClosed();
    int getStatus();
};

// Per-enemy record (the leader and the 23 room enemies).
struct R209Em {
    cEmWrap w;           // 0x00
    int pointNo;         // 0x0C  r209_leaderPoint index the enemy walks to
    int snipe;           // 0x10  1: the bowgun enemy walks the balcony (r209_BowgunMove)
    int active;          // 0x14
    int pathReset;       // 0x18  1: restart the path
    Vec* path[5];        // 0x1C  waypoints (0 terminated)
    int pathIdx;         // 0x30
};

struct R209Work {
    cEmWrap leader;      // 0x000
    int leaderPointNo;   // 0x00C
    int pad_010[9];      // 0x010
    cObj* head;          // 0x034  the leader's head object
    R209Em em[23];       // 0x038
    cEmDoor* door9;          // 0x4E4
    int pad_4E8;         // 0x4E8
    cEmDoor* door4;          // 0x4EC
    cEmDoor* door2;          // 0x4F0
    cEmDoor* door3;          // 0x4F4
    u32 plInPlaceCnt;    // 0x4F8
    int leaderInPlace;   // 0x4FC
    int plInPlace;       // 0x500
    int doorState;       // 0x504
    cObjGatling* gatling; // 0x508
    cObj* bridgeObj;     // 0x50C
    cObj* bridge[4];     // 0x510
    u32 atPrev[8];       // 0x520
    u32 atCur[8];        // 0x540
    u32 atOn[8];         // 0x560
    u32 atOff[8];        // 0x580
    cDataUnit* evd;      // 0x5A0
    u8 panel[4];         // 0x5A4  bridge panel state (0/1)
    Vec bridgeOfs[4];    // 0x5A8
    int snipeCnt;        // 0x5D8
    int snipeIdx[8];     // 0x5DC
    int pad_5FC[2];      // 0x5FC
    cSat* sat0;          // 0x604
    cSat* sat1;          // 0x608
    cSat* sat2;          // 0x60C
    cSat* eat0;          // 0x610
    cSat* eat1;          // 0x614
    int pad_618;         // 0x618
    u32 strId;           // 0x61C
    u32 seId;            // 0x620
    cEmWrap picEm[2];    // 0x624
    cR209Door door[7];   // 0x63C
    SCE_TASK* task[4];    // 0x7FC
};


static R209Work* r209_work;




// atCur bit of sniper `i` (0..3) for area `k` (0..7) of the balcony.
#define R209_SNIPE_AT(at, i, k) FlagChkVar(at, (u32) ((i) * 8 + 8 + (k)))
// The same through an inline (the array argument becomes an opaque pointer: `addi base, 0x540`
// once per block instead of the constant folded into every load).
static inline u32 r209_SnipeAt(u32* at, int i, int k)
{
    return FlagChkVar(at, (u32) (i * 8 + 8 + k));
}

// Enemy list entry set by the second battle (r209_2ndBattleEmSet / BowgunAppearEndProc tables).
struct R209EmSet {
    int em;     // R209Work::em index
    int no;     // enemy list entry
    int point;  // r209_bowgunStartPos index (-1: none)
};

static Vec r209_leaderPoint[8] = {
    {-37320.0f, 1950.0f, 24415.0f},
    {-19660.0f, 6000.0f, 27190.0f},
    {-30740.0f, 1000.0f, 16330.0f},
    {-31650.0f, 2000.0f, 21450.0f},
    {-24040.0f, 6000.0f, 22300.0f},
    {-42270.0f, 4000.0f, 10027.0f},
    {-41522.0f, 4000.0f, 7581.0f},
    {-18070.0f, 6000.0f, 8900.0f},
};
static int r209_snipeEmNo[8] = {7, 8, 9, 10, 11, 12, 13, 14};
static Vec r209_bowgunStartPos[4] = {
    {-58048.0f, 7216.0f, 14032.0f},
    {-58390.0f, 7216.0f, 628.0f},
    {-74392.0f, 7216.0f, 266.0f},
    {-74265.0f, 7216.0f, 14347.0f},
};
static Vec r209_zeroVec = {0.0f, 0.0f, 0.0f};
static Vec r209_bowgunPos[12] = {
    {-75080.0f, 7216.0f, 15738.0f},
    {-74170.0f, 7216.0f, 14660.0f},
    {-75500.0f, 7216.0f, 7950.0f},
    {-74670.0f, 7216.0f, -127.0f},
    {-75624.0f, 7216.0f, -1030.0f},
    {-66500.0f, 7216.0f, 0.0f},
    {-57110.0f, 7216.0f, -1060.0f},
    {-58380.0f, 7216.0f, 115.0f},
    {-60500.0f, 7216.0f, 7600.0f},
    {-58050.0f, 7216.0f, 15030.0f},
    {-57115.0f, 7216.0f, 15746.0f},
    {-66500.0f, 7216.0f, 15500.0f},
};
static Vec r209_bowgunPos2[4] = {
    {-75480.0f, 7216.0f, 7465.0f},
    {-66790.0f, 7216.0f, 15670.0f},
    {-57280.0f, 7216.0f, 7780.0f},
    {-66350.0f, 7216.0f, -343.0f},
};
static void (cR209Door::*r209_doorTbl[3])() = {
    &cR209Door::wait,
    &cR209Door::open,
    &cR209Door::close,
};

// Explicitly zero-initialised unreferenced static: GCC 2.95 keeps it in .data (the word after the
// door method table; nothing in the unit reads it).
static int r209_unused = 0;

static void r209_LeaderAction();
static void r209_LeaderPointAtPlayer();
static void r209_LeaderPointAtPlayerEndProc();
extern "C" void r209_LeaderMoveToPoint(int no, int flag);
static void r209_ToPoint1F();
static void r209_ToPoint2FOut();
static void r209_DoorOpen1F(int no);
static void r209_DoorOpen2F(int no);
static void r209_LeaderEscapeToD();
static void r209_LeaderEscapeToDEndProc();
static void r209_DoorMessage();
static void r209_CheckUseSalonKey();
static void r209_GatlingAppear();
static void r209_GatlingAppearEndProc();
static void r209_GatlingEndCheck();
static void r209_GatlingEndCheckEndProc();
static void r209_2ndBattle();
static void r209_2ndBattleEmSet();
static void r209_2ndBattleBowgunAppear();
static void r209_2ndBattleBowgunAppearEndProc();
static void r209_2ndBattleFinish();
static void r209_2ndBattleFinishEndProc();
static void r209_SwitchAppearCheck();
static void r209_SwitchAppearCheckEnd();
static void r209_PotBreakCheck();
static void r209_BridgeAppearCheck();
static void r209_BridgeAppearCheckEnd();
static void r209_OpenPicture(int no);
static void r209_OpenPictureEndProc();
static void r209_ClosePicture();
extern "C" void r209_BowgunCtrl();
extern "C" void r209_BowgunActionSet1(int i, int flag, int pt);
extern "C" void r209_BowgunActionSet2(int i, int flag, int pt);
extern "C" void r209_BowgunActionSet3(int i, int flag, int pt);
extern "C" void r209_BowgunActionSet4(int i, int flag, int pt);
static void r209_BowgunMove();
static void Evt_R209S00_Func(Event* evt);
static void r209_PanelPuzzle();
extern "C" void r209_PanelRotate(cObj** tbl);
extern "C" void r209_PanelPazzleEnd();
extern "C" void r209_PanelPazzleEndEndProc();
static void r209_PanelMes(int no);
static void r209_StrStopReqFunc(int atNo);
static void r209_RotateDoor(int no);
static void r209_TreasureBoxOpen(int id);
static void r209_TreasureBoxOpened(int id);
static void r209_StrPlayCk();
extern "C" int r209_InPlaceCheck(cModel* m);
extern "C" int r209_GanadoSnipeCheck(cEmWrap* w);

// Room init (the gallery): the seven room doors (four balcony doors 0xB3..0xB6, salon doors 2/3, the
// picture lift 0xA1), the salon key door (area 2 + key watcher until Room_flg bit 2), the second battle
// (bit 4), the leader chase until bit 3 (the leader 0x7D and its escorts, list 3; areas 0xA = it points
// at the player, 0x16 = its escape, 0xD = stream stop; the gatling item hidden), the bridge panels and
// their puzzle (bit 6 = solved, area 0x1E + the four panel messages), the stage flag / picture / pot
// areas, the item events and the battle stream.
void R209Init()
{
    u32 i;
    cModel* m;

#line 94 "D:/Bio4/Prog/r209.cpp"
    r209_work = (R209Work*) MEM_CALLOC(sizeof(R209Work), 1, 0xd);
    getRoomEtcDoor(4, &r209_work->door4, 1);
    r209_work->door[0].init(0xB3);
    r209_work->door[1].init(0xB4);
    r209_work->door[2].init(0xB5);
    r209_work->door[3].init(0xB6);
    r209_work->door[4].init(2);
    r209_work->door[5].init(3);
    r209_work->door[6].init(0xA1);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        if (getRoomEtcDoor(9, &r209_work->door9, 1) == 1) {
            r209_work->door9->setCloseLock();
            SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r209_DoorMessage, 0, 1);
            SceExec(0x12, (TaskFunc) r209_CheckUseSalonKey, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        r209_work->evd = DC.setData(EvtMgr.NameChange("evd/r209s00.evd"));
        EmReadSearch(0x1A, 0, r209_work->evd->m_size);
        r209_work->evd->setCommand(CMND_ARAM_LOAD, 0, 0);
        EvtMgr.SetFunc("evt_r209s00_func", (void*) Evt_R209S00_Func);
        getRoomEtcDoor(4, &r209_work->door4, 1);
        SceExec(0x12, (TaskFunc) r209_2ndBattle, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        Vec ofs = {0.0f, -2.0f, 180.0f};
        Vec rot0 = {0.0f, 0.0f, 0.0f};

        ScfFlagOff(pG, SCF_8c);
        r209_work->leader.setEm(0x7D, 3, 1, 0, 0);
        r209_work->head = SetObj00(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21), &ofs, &rot0);
        OyaSetObj00(r209_work->head, r209_work->leader.getPtr(), 2);
        EstSet(r209_work->head, -1, 0, 0, EFF_CORE, 0x2D, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        r209_work->em[4].w.setEm(0x7E, 3, 1, 0, 0);
        r209_work->em[5].w.setEm(0x7F, 3, 1, 0, 0);
        r209_work->em[6].w.setEm(0x8B, 3, 1, 0, 0);
        r209_work->em[7].w.setEm(0x8D, 3, 1, 0, 0);
        r209_work->em[1].w.setEm(0x7B, 3, 1, 0, 0);
        r209_work->em[2].w.setEm(0x7C, 3, 1, 0, 0);
        getRoomEtcDoor(2, &r209_work->door2, 1);
        getRoomEtcDoor(3, &r209_work->door3, 1);
        SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r209_LeaderPointAtPlayer, 0, 1);
        SceAtDataSet_exec(0x16, SCE_LEVEL10, 0, (TaskFunc) r209_LeaderEscapeToD, 0, 1);
        SceAtDataSet_exec(0xD, SCE_LEVEL10, 0, (TaskFunc) r209_StrStopReqFunc, (void*) 0xD, 1);
        r209_work->strId = SndStrReq(0, 0x14, 0x80000003, 0, 0, 0.0f);
        SceAtSetEnable(0x8F, 0);
    }
    if (RsfCheck(G_ROOM_ID, 3)) {
        SmdGetObjPtr(0xAC)->be_flag &= ~2;
        SceAtSetEnable(0xC, 0);
    }
    r209_work->bridgeObj = SmdGetObjPtr(0x84);
    r209_work->bridge[0] = SmdGetObjPtr(0x81);
    r209_work->bridge[1] = SmdGetObjPtr(0x82);
    r209_work->bridge[2] = SmdGetObjPtr(0x83);
    r209_work->bridge[3] = SmdGetObjPtr(0xB2);
    for (i = 0; i < 4; i++) {
        PSVECSubtract(&r209_work->bridge[i]->pos, &r209_work->bridgeObj->pos, &r209_work->bridgeOfs[i]);
    }
    if (RsfCheck(G_ROOM_ID, 6)) {
        u32 j;
        f32 rotY[4] = {0.0f, PI, PI, 0.0f};

        r209_work->bridgeObj->pos.z = 14530.0f;
        r209_work->bridgeObj->matUpdate();
        for (j = 0; j < 4; j++) {
            PSVECAdd(&r209_work->bridgeObj->pos, &r209_work->bridgeOfs[j], &r209_work->bridge[j]->pos);
            r209_work->bridge[j]->ang.y = rotY[j];
            r209_work->bridge[j]->matUpdate();
        }
        SceAtSetEnable(0x21, 0);
    } else {
        SceAtDataSet_exec(0x1E, SCE_LEVEL10, 0, (TaskFunc) r209_PanelPuzzle, 0, 1);
        if (pSys->language == 0) {
            SceAtDataSet_exec(0x2E, SCE_LEVEL10, 0, (TaskFunc) r209_PanelMes, (void*) 0, 1);
            SceAtDataSet_exec(0x2F, SCE_LEVEL10, 0, (TaskFunc) r209_PanelMes, (void*) 1, 1);
            SceAtDataSet_exec(0x30, SCE_LEVEL10, 0, (TaskFunc) r209_PanelMes, (void*) 2, 1);
            SceAtDataSet_exec(0x31, SCE_LEVEL10, 0, (TaskFunc) r209_PanelMes, (void*) 3, 1);
        }
    }
    SmdGetObjPtr(2)->be_flag &= ~2;
    SmdGetObjPtr(3)->be_flag &= ~2;
    r209_work->sat2 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r209_zeroVec, &r209_zeroVec, 3);
    if (RsfCheck(*(u16*) &pG->stage_no, 8) == 0) {
        SmdGetObjPtr(0xAD)->pos.z = 0.0f;
        SmdGetObjPtr(0xAD)->matUpdate();
    } else {
        SatMgr.destroy(r209_work->sat2);
        r209_work->sat1 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r209_zeroVec, &r209_zeroVec, 1);
        r209_work->eat1 = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r209_zeroVec, &r209_zeroVec, 2);
    }
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        SmdGetObjPtr(1)->pos.y = 4000.0f;
        SmdGetObjPtr(1)->matUpdate();
        SmdGetObjPtr(0xB7)->pos.y = 1491.0f;
        SmdGetObjPtr(0xB7)->matUpdate();
    } else {
        r209_work->sat0 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r209_zeroVec, &r209_zeroVec, 2);
        r209_work->eat0 = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r209_zeroVec, &r209_zeroVec, 3);
    }
    SceAtSetEnable(0x82, 1);
    m = SceAtItemModelPtr(0x82);
    if (m != NULL) {
        Vec sca = {0.65f, 0.65f, 0.65f};

        m->setSca(&sca);
    }
    SceSetItemEvent(0x17, 0x82, 0xA, 0x17, r209_TreasureBoxOpen, r209_TreasureBoxOpened, 0xB7, 0);
}

// Per frame during the bowgun battle (stage flag 3): builds the occupancy bits of the eight balcony
// areas 0x26..0x2D (player / each bowgun Ganado) with their on/off edges and runs the bowgun
// controller (r209_BowgunCtrl); also steps the seven doors.
void R209Main()
{
    u32 i;
    u32 j;
    u32 n;
    R209Em* w;
    u32 flags[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int atNo[8] = {0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D};
    u32 atNum = 8;

    r209_work->snipeCnt = 0;
    if (RsfCheck(*(u16*) &pG->stage_no, 3)) {
        for (i = 0; i < 8; i++) {
            w = &r209_work->em[r209_snipeEmNo[i]];
            if (w->active == 1 && w->w.isActive() == 1 && w->w.ckParasite() == 0) {
                r209_work->snipeIdx[r209_work->snipeCnt] = r209_snipeEmNo[i];
                r209_work->snipeCnt++;
            }
        }
        // do-while: a `for` keeps a duplicated entry test `i < atNum` that cse1 cannot fold (atNum is a
        // variable set in another block), and gcse then sees a path around the loop and leaves the pPL
        // high inside it; without the test it is PRE'd to the block before the first loop (`lis r28`).
        i = 0;
        do {
            if (SceAtCheckHitModel(atNo[i], pPL)) {
                FlagOnVar(flags, (u32) i);
                FlagOnVar(flags, (u32) ((i >> 1) + 0x80));
            }
            i++;
        } while (i < atNum);
        // The outer counter is `i` (the same variable as the other loops keeps `i + 1` at the latch as a
        // biv instead of a PRE-hoisted copy). The bit index is a loop.c giv of `j` written as two
        // consecutive sets of one variable (consec_sets_giv): one giv with benefit 2 adds and no
        // not-worth intermediate, so `j` still has only reducible givs and is eliminated into the
        // pointer compare, and the giv init `addi bit,base8,8` is emitted after the loop's movables.
        for (i = 0; i < (u32) r209_work->snipeCnt; i++) {
            u32 base8 = i * 8;
            int idx = r209_work->snipeIdx[i];

            for (j = 0; j < atNum; j++) {
                u32 bit = j + base8;
                bit += 8;
                if (SceAtCheckHitModel(atNo[j], r209_work->em[idx].w.getPtr())) {
                    // Not a pointer-base bit-set macro: the block's three local qtys (idx, amt/shift, val/or) are
                    // hand-sorted by local-alloc, and with a pointer base the idx pseudo prefers
                    // GENERAL_REGS and takes r0.  An integer (u32) base makes both plus operands
                    // half-BASE_REGS, so idx takes r9 and the shift amount r0 like the target
                    // (`lwzx r11,r9,r28`); `fb` must be read before the shift constant's first use
                    // so the hoisted `mr r28,r20` copy keeps its LUID ahead of `lis r24,0x8000`.
                    u32 fb = (u32) flags;
                    u32 ofs = ((u32) bit >> 5) << 2;
                    *(u32*) (ofs + fb) |= 0x80000000 >> (bit & 31);
                }
            }
        }
        if (pG->Room_flg[2] & 0x00200000) {
            u32* f = flags;  // through the flags pointer pseudo (`8(r20)`), not the frame slot
            f[2] |= 0x80000000;
        }
    }
    for (i = 0; i < 8; i++) {
        r209_work->atPrev[i] = r209_work->atCur[i];
        r209_work->atCur[i] = flags[i];
        r209_work->atOn[i] = flags[i] & ~r209_work->atPrev[i];
        r209_work->atOff[i] = r209_work->atPrev[i] & ~flags[i];
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        if (r209_work->leader.isActive() == 0) {
            RsfSet(G_ROOM_ID, 3);
            r209_work->head->be_flag &= ~2;
            EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
            EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
        } else {
            if (RsfCheck(G_ROOM_ID, 1) && RsfCheck(G_ROOM_ID, 7) == 0 && (pG->Room_flg[0] & 0x00200000)) {
                RsfSet(G_ROOM_ID, 7);
                SceExec(0x12, (TaskFunc) r209_GatlingAppear, 0, 0, SCE_PRIO_DEF_2, 0);
            }
            if (r209_work->leader.ckGoto() != 1) {
                r209_work->leaderPointNo = -1;
            }
            r209_work->leaderInPlace = r209_InPlaceCheck(r209_work->leader.getPtr());
            r209_work->plInPlace = r209_InPlaceCheck(pPL);
            if (r209_work->leaderInPlace == 0) {
                pG->Room_flg[0] |= 0x20000000;
            }
            if (r209_work->plInPlace == 0) {
                pG->Room_flg[0] |= 0x40000000;
                r209_work->plInPlaceCnt++;
            } else {
                r209_work->plInPlaceCnt = 0;
            }
            // two `andis.`: separate ifs keep fold from merging the bit tests
            if (pG->Room_flg[0] & 0x40000000) {
                if (pG->Room_flg[0] & 0x20000000) {
                    if (r209_work->leaderInPlace == 1 && r209_work->plInPlace == 1) {
                        SceAtSetEnable(0x19, 0);
                        SceAtSetEnable(0x1A, 0);
                        SceAtSetEnable(0x1C, 0);
                        SceAtSetEnable(0x1D, 0);
                        r209_work->leader.getPtr()->Character = 0;
                    }
                }
            }
        }
    }
    r209_BowgunCtrl();
    n = SceCountEmAlive(0x10, 0x20);
    if (n == 0 && r209_work->strId) {
        SndStrReq(r209_work->strId, 4, 600, 0);
        r209_work->strId = n;
    }
    for (i = 0; i < 7; i++) {
        r209_work->door[i].move();
    }
}

// 0 while the model stands inside one of the three "in place" areas.
extern "C" int r209_InPlaceCheck(cModel* m)
{
    if (SceAtCheckHitModel(5, m)) {
        return 0;
    }
    if (SceAtCheckHitModel(6, m)) {
        return 0;
    }
    return SceAtCheckHitModel(7, m) == 0;
}

// 1 while the player's laser sight points at the enemy.
extern "C" int r209_GanadoSnipeCheck(cEmWrap* w)
{
    if (w->isActive()) {
        cEm* em = w->getPtr();

        if (pPL->Wep->m_pWep->m_SightEm == em) {
            return 1;
        }
    }
    return 0;
}

// Task: keeps the leader alerted until Room_flg bit 1; while the player's laser is on it, it dodges to
// the next r209_leaderPoint.
static void r209_LeaderAction()
{
    R209Work* wp = r209_work;
    int loop = 1;
    int moved = 0;
    cEm* em = wp->leader.getPtr();

    while (loop) {
        wp->leader.setFindPL();
        if (RsfCheck(G_ROOM_ID, 1)) {
            loop = 0;
        }
        if (wp->leader.isActive()) {
            int snipe = r209_GanadoSnipeCheck(&wp->leader);

            em->dmg.m_Timer = 2;
            if (moved == 0 && snipe == 1) {
                moved = 1;
                r209_LeaderMoveToPoint(6, 1);
            }
        } else {
            loop = 0;
        }
        SceSleep(1);
    }
}

// Area 0xA: once door 1 has been opened, a cutscene in which the leader (with its escorts kept
// updating) points at the player and the escorts 6/7 turn hostile; player-cancellable.
static void r209_LeaderPointAtPlayer()
{
    cEmDoor* door;
    int opened = 0;

    getRoomEtcDoor(1, &door, 1);
    r209_work->leader.setFindPL();
    do {
        if (door->flag & 0x10000000) {
            opened = 1;
        }
        SceSleep(1);
    } while (opened == 0);
    SceEventStart(1);
    r209_work->leader.setNoSuspend(1);
    r209_work->head->setNoSuspend(1);
    r209_work->em[4].w.setNoSuspend(1);
    r209_work->em[5].w.setNoSuspend(1);
    r209_work->em[6].w.setNoSuspend(1);
    r209_work->em[7].w.setNoSuspend(1);
    CamCtrl.CutCall(3);
    r209_work->leader.setGoto(&pPL->pos, 8);
    SceSetEventCancel(1, (TaskFunc) r209_LeaderPointAtPlayerEndProc, 0, -1, 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(2);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r209_LeaderPointAtPlayerEndProc();
}

// End of the pointing cutscene (also its cancel path): camera back, the actors may suspend, escorts
// 6/7 alerted, SceEventEnd; waits for the leader's goto 8 to end and sends it on.
static void r209_LeaderPointAtPlayerEndProc()
{
    CamCtrl.Comeback(0);
    r209_work->leader.setNoSuspend(0);
    r209_work->head->setNoSuspend(0);
    r209_work->em[4].w.setNoSuspend(0);
    r209_work->em[5].w.setNoSuspend(0);
    r209_work->em[6].w.setNoSuspend(0);
    r209_work->em[7].w.setNoSuspend(0);
    r209_work->em[6].w.setFlag(1);
    r209_work->em[7].w.setFlag(1);
    SceEventEnd(0);
    while (r209_work->leader.ckGoto() == 8) {
        SceSleep(1);
    }
    r209_LeaderMoveToPoint(5, 1);
    SceExec(0x12, (TaskFunc) r209_LeaderAction, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Send the leader to r209_leaderPoint[no] with goto mode `flag` (1 run, 8 taunt walk).
extern "C" void r209_LeaderMoveToPoint(int no, int flag)
{
    r209_work->leaderPointNo = no;
    r209_work->leader.setGoto(&r209_leaderPoint[no], flag);
}

// The leader's first-floor route: points 1, 0, then 2 (running).
static void r209_ToPoint1F()
{
    r209_LeaderMoveToPoint(1, 1);
    while (r209_work->leader.ckGoto() == 1) {
        SceSleep(1);
    }
    r209_LeaderMoveToPoint(0, 1);
    while (r209_work->leader.ckGoto() == 1) {
        SceSleep(1);
    }
    r209_LeaderMoveToPoint(2, 1);
}

// The leader runs out of the second floor to point 4.
static void r209_ToPoint2FOut()
{
    r209_LeaderMoveToPoint(4, 1);
}

// Balcony door area `no` on the first floor: the leader (if alive) opens the door ahead of the player
// and runs on (area 0x1A: only once it is in place), the escorts follow.
static void r209_DoorOpen1F(int no)
{
    Vec ang;

    if (r209_work->leader.isActive() == 0) {
        SceExit();
    }
    if (r209_work->leader.getPtr()->hp <= 0) {
        SceExit();
    }
    SceAtSetEnable(no, 0);
    if (no == 0x1A) {
        if (r209_work->leader.isActive()) {
            if (r209_work->leaderInPlace == 0) {
                if (SceAtCheckHitModel(5, r209_work->leader.getPtr()) == 0) {
                    cEmWrap* w = &r209_work->leader;
                    f32 angY = 2.32f;

                    w->setPos(&r209_leaderPoint[0]);
                    ang.x = 0.0f;
                    ang.y = angY;
                    ang.z = 0.0f;
                    w->setAng(&ang);
                }
                while ((r209_work->door2->flag & 0x10000000) == 0) {
                    SceSleep(1);
                }
                r209_LeaderMoveToPoint(4, 1);
                r209_work->em[0].w.setEm(0x79, 3, 0, 0, 0);
            } else {
                r209_work->leader.setPos(&r209_leaderPoint[1]);
                SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r209_ToPoint2FOut, 0, 1);
                r209_work->em[0].w.setEm(0x79, 3, 0, 0, 0);
            }
        }
        while (r209_work->door2->flag & 0x10000000) {
            SceSleep(1);
        }
        if (r209_work->plInPlace == 0) {
            r209_work->doorState = 1;
        }
    } else {
        if (r209_work->doorState == 2) {
            if (r209_work->leaderInPlace != 1) {
                SceAtSetEnable(0x19, 0);
                SceAtSetEnable(0x1A, 0);
                SceAtSetEnable(0x1C, 0);
                SceAtSetEnable(0x1D, 0);
                r209_LeaderMoveToPoint(3, 1);
                r209_work->leader.getPtr()->Character = 0;
                SceExit();
            } else {
                goto inPlace;
            }
        } else if (r209_work->leaderInPlace == 1) {
            if (r209_work->plInPlaceCnt > 0x95) {
inPlace:
                pG->Room_flg[0] |= 0x00200000;
                SceAtSetEnable(0x19, 0);
                SceAtSetEnable(0x1A, 0);
                SceAtSetEnable(0x1C, 0);
                SceAtSetEnable(0x1D, 0);
                SceExit();
            } else {
                r209_LeaderMoveToPoint(1, 1);
            }
        }
    }
    SceAtSetEnable(no, 1);
}

// Door area `no` on the second floor: as r209_DoorOpen1F; area 0x1D also spawns Ganado 0x79.
static void r209_DoorOpen2F(int no)
{
    Vec ang;

    if (r209_work->leader.isActive() == 0) {
        SceExit();
    }
    if (r209_work->leader.getPtr()->hp <= 0) {
        SceExit();
    }
    SceAtSetEnable(no, 0);
    if (no == 0x1D) {
        r209_work->em[0].w.setEm(0x79, 3, 0, 0, 0);
        if (r209_work->leaderInPlace == 0) {
            if (SceAtCheckHitModel(7, r209_work->leader.getPtr()) == 0) {
                cEmWrap* w = &r209_work->leader;
                f32 angY = 2.32f;

                w->setPos(&r209_leaderPoint[1]);
                ang.x = 0.0f;
                ang.y = angY;
                ang.z = 0.0f;
                w->setAng(&ang);
            }
            while ((r209_work->door3->flag & 0x10000000) == 0) {
                SceSleep(1);
            }
            r209_LeaderMoveToPoint(2, 1);
        } else if (r209_work->task[0] == 0) {
            r209_work->task[0] = SceExec(0x12, (TaskFunc) r209_ToPoint1F, 0, 0, SCE_PRIO_DEF_2, 0);
        }
        while (r209_work->door3->flag & 0x10000000) {
            SceSleep(1);
        }
        if (r209_work->plInPlace == 0) {
            r209_work->doorState = 2;
        }
    } else {
        if (r209_work->doorState == 1) {
            if (r209_work->leaderInPlace != 1) {
                SceAtSetEnable(0x19, 0);
                SceAtSetEnable(0x1A, 0);
                SceAtSetEnable(0x1C, 0);
                SceAtSetEnable(0x1D, 0);
                r209_work->leader.getPtr()->Character = 0;
                SceExit();
            } else {
                goto inPlace;
            }
        } else if (r209_work->leaderInPlace == 1) {
            if (r209_work->plInPlaceCnt > 0x95) {
inPlace:
                pG->Room_flg[0] |= 0x00200000;
                SceAtSetEnable(0x19, 0);
                SceAtSetEnable(0x1A, 0);
                SceAtSetEnable(0x1C, 0);
                SceAtSetEnable(0x1D, 0);
                SceExit();
            } else {
                r209_LeaderMoveToPoint(0, 1);
            }
        }
    }
    SceAtSetEnable(no, 1);
}

// Area 0x16: once the player is idle, the leader runs to the exit door D (object 0xAC) and through
// it in a cutscene; player-cancellable.
static void r209_LeaderEscapeToD()
{
    Vec pos0 = {-41412.0f, 4000.0f, 10379.0f};
    Vec pos;
    cObj* obj = SmdGetObjPtr(0xAC);
    R209Work* wp = r209_work;

    if (wp->leader.isActive() == 0) {
        SceExit();
    }
    while (PlGetStatus() != 1) {
        SceSleep(1);
    }
    {
        Vec* p = &pos;
        f32 angY;

        pos.x = -41412.0f;
        p->y = 4000.0f;
        p->z = 10379.0f;
        angY = 2.27f;
        wp->leader.setPos(p);
        pos.x = 0.0f;
        p->y = angY;
        pos.z = 0.0f;
        wp->leader.setAng(p);
    }
    SceEventStart(1);
    SceAtSetEnable(0xC, 0);
    wp->leader.setNoSuspend(1);
    r209_work->head->setNoSuspend(1);
    r209_LeaderMoveToPoint(7, 1);
    CamCtrl.CutCall(0xC);
    pG->Room_flg[0] &= ~0x00100000;
    SceSetEventCancel(1, (TaskFunc) r209_LeaderEscapeToDEndProc, 0, 0xB, 1);
    r209_work->seId = RoomSeCall(3, &obj->pos, 0, 0, 0);
    while (obj->pos.y < 2900.0f) {
        obj->pos.y += 33.0f;
        obj->matUpdate();
        SceSleep(1);
        wp->leader.getPtr()->atari.offOba();
    }
    RoomSeCall(4, &obj->pos, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SmdGetObjPtr(0xAC)->be_flag &= ~2;
    CamCtrl.CutCall(0xD);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r209_LeaderEscapeToDEndProc();
}

// End of the escape cutscene (also its cancel path, Room_flg[0] 0x00100000): the leader placed behind
// door D facing 1.11 rad and the room flags for the next phase set.
static void r209_LeaderEscapeToDEndProc()
{
    cEmWrap* w = &r209_work->leader;
    Vec pos;

    if (pG->Room_flg[0] & 0x00100000) {
        f32 angY;

        pos.x = -20980.0f;
        pos.y = 6000.0f;
        pos.z = 7828.0f;
        angY = 1.11f;   // pool order: after the position constants
        w->setPos(&pos);
        pos.x = 0.0f;
        pos.y = angY;
        pos.z = 0.0f;
        w->setAng(&pos);
        SmdGetObjPtr(0xAC)->be_flag &= ~2;
        SndStop(r209_work->seId, 0);
    }
    CamCtrl.Comeback(0);
    RsfSet(G_ROOM_ID, 1);
    w->getPtr()->atari.onOba();
    w->setNoSuspend(0);
    r209_work->head->setNoSuspend(0);
    SceAtDataSet_exec(0x1A, SCE_LEVEL10, 0, (TaskFunc) r209_DoorOpen1F, (void*) 0x1A, 1);
    SceAtDataSet_exec(0x19, SCE_LEVEL10, 0, (TaskFunc) r209_DoorOpen1F, (void*) 0x19, 1);
    SceAtDataSet_exec(0x1D, SCE_LEVEL10, 0, (TaskFunc) r209_DoorOpen2F, (void*) 0x1D, 1);
    SceAtDataSet_exec(0x1C, SCE_LEVEL10, 0, (TaskFunc) r209_DoorOpen2F, (void*) 0x1C, 1);
    ScfFlagOn(pG, SCF_8c);
    SceEventEnd(0);
    while (w->ckGoto() == 1) {
        SceSleep(1);
    }
    while (SceAtHitCheck(0xB) == 0 && r209_GanadoSnipeCheck(&r209_work->leader) == 0) {
        SceSleep(1);
    }
    r209_LeaderMoveToPoint(1, 1);
}

// Area 2, the locked salon door: up-cut 0/1; with the salon key (item 0xA3) held the item screen opens.
static void r209_DoorMessage()
{
    SceUpCut(0, -1, 1, UP_CUT_ATTR_CUT_FIX);
    if (ItemMgr.num(0xA3)) {
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
    } else {
        CamCtrl.Comeback(0);
    }
}

// Task: waits for the salon key (item 0xA3) to be used, then Room_flg bit 2, Scenario_flg[3] 0x1000,
// door 9 becomes normal, message up-cut 1/2.
static void r209_CheckUseSalonKey()
{
    while (ItemMgr.check(0xA3) == 0) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 2);
    ScfFlagOn(pG, SCF_73);
    SceAtSetEnable(2, 0);
    r209_work->door9->setNormal();
    SceUpCut(1, -1, 2, 0);
}

// The gatling Ganado's entrance (if the leader lives): the leader with the gatling (cObjGatling) rises
// on the lift in a cutscene with SE; player-cancellable.
static void r209_GatlingAppear()
{
    Vec pos;
    Vec rot;
    cEm* em;

    if (r209_work->leader.isActive() == 0) {
        SceExit();
    }
    if (r209_work->leader.getPtr()->hp <= 0) {
        SceExit();
    }
    SceEventStart(1);
    r209_work->door2->setNoSuspend(0);
    r209_work->door3->setNoSuspend(0);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SndStrReq(r209_work->strId, 4, 400, 0);
    r209_work->strId = SndStrReq(0, 0x1B, 0x80000003, 0, 0, 0.0f);
    pos.x = -27050.0f;
    pos.y = 1000.0f;
    pos.z = 13348.0f;
    rot.x = 0.0f;
    rot.y = 0.7853982f;
    rot.z = 0.0f;
    r209_work->gatling = SetObjGatling(ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25), &pos, &rot);
    if (r209_work->gatling != NULL) {
        r209_work->gatling->setEat(ROOM_ARC_PTR(pG->pRoom, 0x12), 1);
    }
    em = r209_work->leader.getPtr();
    ((cEmGanado*) em)->setGatling(r209_work->gatling, ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28), ROOM_ARC_PTR(pG->pRoom, 0x29));
    em->setNoSuspend(1);
    r209_work->head->setNoSuspend(1);
    r209_work->gatling->setNoSuspend(1);
    CamCtrl.CutCall(0x11);
    pG->Room_flg[0] &= ~0x00100000;
    SceSetEventCancel(1, (TaskFunc) r209_GatlingAppearEndProc, 0, 0xB, 1);
    r209_work->seId = RoomSeCall(7, &r209_work->gatling->pos, 0, 0, 0);
    while (r209_work->gatling->pos.y < 3250.0f) {
        r209_work->gatling->pos.y += 30.0f;
        SceSleep(1);
    }
    r209_work->gatling->pos.y = 3250.0f;
    RoomSeCall(8, &r209_work->gatling->pos, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r209_GatlingAppearEndProc();
}

// End of the gatling entrance (also its cancel path, Room_flg[0] 0x00100000): SE stopped, the gatling
// snapped to y 3250, camera back, the actors may suspend, SceEventEnd, the leader alerted, the
// gatling end watcher starts.
static void r209_GatlingAppearEndProc()
{
    if (pG->Room_flg[0] & 0x00100000) {
        SndStop(r209_work->seId, 0);
        r209_work->gatling->pos.y = 3250.0f;
    }
    CamCtrl.Comeback(0);
    r209_work->leader.setNoSuspend(0);
    r209_work->head->setNoSuspend(0);
    r209_work->gatling->setNoSuspend(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    r209_work->leader.setFlag(1);
    SceExec(0x12, (TaskFunc) r209_GatlingEndCheck, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Task: when the leader (gatling) dies, a cutscene shows the key item 0x8F it drops; cancellable.
static void r209_GatlingEndCheck()
{
    cModel* m;

    while (r209_work->leader.isActive() == 1) {
        SceSleep(1);
    }
    SceSleep(15);
    SceEventStart(1);
    SceAtSetEnable(0x8F, 1);
    m = SceAtItemModelPtr(0x8F);
    if (m != NULL) {
        m->setNoSuspend(1);
        m->LightInfo.EnableMask &= ~0x20;
        m->LightInfo.EnableMask |= 0x10;
    }
    CamCtrl.CutCall(0x18);
    SceSetEventCancel(1, (TaskFunc) r209_GatlingEndCheckEndProc, 0, -1, 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r209_GatlingEndCheckEndProc();
}

// End of the gatling-death cut: camera back, the item model may suspend, SceEventEnd.
static void r209_GatlingEndCheckEndProc()
{
    cModel* m = SceAtItemModelPtr(0x8F);

    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (m != NULL) {
        m->setNoSuspend(0);
        m->LightInfo.EnableMask |= 0x20;
        m->LightInfo.EnableMask &= ~0x10;
    }
}

// Task: once door 4 is opened (Room_flg bit 3) the second battle event r209s00 plays from the event
// data unit, then the bowgun battle is set up (r209_2ndBattleEmSet).
static void r209_2ndBattle()
{
    ReadModule* m = SearchEmModule(0x1A);

    while ((r209_work->door4->flag & 0x10000000) == 0) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 3);
    SceDestroyEm(0x10, 0x20);
    memclr_asm(r209_work->em, sizeof(R209Em) * 23);
    SceSleep(2);
    SndBgmTblSet(0x209, 1);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    SubScreenWait(60);
    if (r209_work->evd->waitLoadOk() == 1) {
        MemorySwap(m->pArc, (u32) r209_work->evd->m_addr, r209_work->evd->m_size);
        EvtMgr.SetEvt(m->pArc, (u32*) 0);
        while (EvtMgr.IsAliveEvt(&EvtMgr.NowExeEvtKey, 0, 0)) {
            SceSleep(1);
        }
        MemorySwap(m->pArc, (u32) r209_work->evd->m_addr, r209_work->evd->m_size);
        r209_work->evd->setCommand(CMND_DEL_DATA, 0, 0);
    }
    SceAtSetEnable(0, 0);
    {
        cModel* pl = pPL;
        Vec pos;
        f32 angY;

        pos.x = -57725.0f;
        pos.y = 4000.0f;
        pos.z = 7285.0f;
        angY = -1.74f;   // pool order: after the position constants
        pl->setPos(&pos);
        pos.x = 0.0f;
        pos.y = angY;
        pos.z = 0.0f;
        pl->setAng(&pos);
    }
    if (r209_work->em[0].w.setEm(0x93, 3, 1, 0, 0) == 1) {
        r209_work->em[0].active = 1;
        r209_work->em[0].snipe = 1;
        r209_work->em[0].w.setFindPL();
    }
    if (r209_work->em[1].w.setEm(0x94, 3, 1, 0, 0) == 1) {
        r209_work->em[1].active = 1;
        r209_work->em[1].snipe = 1;
    }
    if (r209_work->em[2].w.setEm(0x95, 3, 1, 0, 0) == 1) {
        r209_work->em[2].active = 1;
        r209_work->em[2].snipe = 1;
    }
    if (r209_work->em[3].w.setEm(0x96, 3, 1, 0, 0) == 1) {
        r209_work->em[3].active = 1;
        r209_work->em[3].snipe = 1;
    }
    if (r209_work->em[15].w.setEm(0x88, 3, 1, 0, 0) == 1) {
        r209_work->em[15].snipe = 1;
    }
    if (r209_work->em[16].w.setEm(0x89, 3, 1, 0, 0) == 1) {
        r209_work->em[16].active = 1;
        r209_work->em[16].snipe = 1;
    }
    SceAtDataSet_exec(0x15, SCE_LEVEL10, 0, (TaskFunc) r209_SwitchAppearCheck, 0, 1);
    SceExec(0x12, (TaskFunc) r209_PotBreakCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r209_2ndBattleEmSet, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r209_StrPlayCk, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Task: the second battle's stepped enemy waves (bowgun Ganados on the balconies, the rotating doors)
// until Room_flg bit 4.
static void r209_2ndBattleEmSet()
{
    int arg = 0;
    u32 step = 0;
    R209EmSet tbl[3] = {{4, 0xA3, -1}, {5, 0xA4, -1}, {6, 0xA5, -1}};
    u32 done = 0;
    u32 i;
    int n;
    u32 cnt;
    int open;

    while (1) {
        if (RsfCheck(G_ROOM_ID, 4)) {
            break;
        }
        n = SceCountEmAlive(0x10, 0x20);
        switch (step) {
        case 0:
            if ((u32) n <= 2) {
                step++;
                SceExec(0x12, (TaskFunc) r209_2ndBattleBowgunAppear, 0, 0, SCE_PRIO_DEF_2, 0);
            }
            break;
        case 1:
            if ((u32) r209_work->snipeCnt <= 2) {
                SceExec(0x12, (TaskFunc) r209_RotateDoor, 4, 0, SCE_PRIO_DEF_2, 0);
                step = 2;
                SceExec(0x12, (TaskFunc) r209_RotateDoor, 5, 0, SCE_PRIO_DEF_2, 0);
            }
            break;
        case 2:
            step = 3;
            break;
        case 3:
            if (RsfCheck(G_ROOM_ID, 8) && (u32) r209_work->snipeCnt <= 1) {
                // `li r25,4 .. mr r29,r25` is gcse's PRE of step+1 across the loop.
                if (pG->Game_level > 7) {
                    for (i = 0; i < 3; i++) {
                        r209_work->em[tbl[i].em].w.setEm(tbl[i].no, 3, 1, 0, 0);
                        r209_work->em[tbl[i].em].active = 1;
                        r209_work->em[tbl[i].em].snipe = 1;
                        r209_work->em[tbl[i].em].w.setFindPL();
                    }
                }
                step++;
            }
            break;
        }
        open = 0;
        if (FlagChkVar(r209_work->atOn, 64) && (pG->Room_flg[0] & 0x04000000) == 0) {
            if ((done ^ 1) & 1) {
                cnt = r209_work->em[0].w.isActive() == 0;
                if (r209_work->em[1].w.isActive() == 0) {
                    cnt++;
                }
                if (r209_work->em[2].w.isActive() == 0) {
                    cnt++;
                }
                if (r209_work->em[3].w.isActive() == 0) {
                    cnt++;
                }
                if (cnt > 1) {
                    done |= 1;
                    open = 1;
                    arg = 0;
                }
            }
            if ((done & 2) == 0) {
                if (RsfCheck(G_ROOM_ID, 5)) {
                    done |= 2;
                    open = 1;
                    arg = 1;
                }
            }
            if (open) {
                SceExec(0x12, (TaskFunc) r209_OpenPicture, arg, 0, SCE_PRIO_DEF_2, 0);
            }
        }
        SceSleep(1);
    }
}

// Cutscene: the three balcony doors rotate open (r209_RotateDoor tasks) under camera cut 0x12 as the
// bowgun Ganados appear; player-cancellable.
static void r209_2ndBattleBowgunAppear()
{
    R209Work* wp = r209_work;   // one load for the four task stores (pointer stores alias the work pointer)

    wp->task[3] = 0;
    wp->task[2] = 0;
    wp->task[1] = 0;
    wp->task[0] = 0;
    SceEventStart(1);
    r209_work->task[0] = SceExec(0x12, (TaskFunc) r209_RotateDoor, 0, 0, SCE_PRIO_DEF_2, 0);
    r209_work->task[1] = SceExec(0x12, (TaskFunc) r209_RotateDoor, 1, 0, SCE_PRIO_DEF_2, 0);
    r209_work->task[2] = SceExec(0x12, (TaskFunc) r209_RotateDoor, 2, 0, SCE_PRIO_DEF_2, 0);
    pG->Room_flg[0] &= ~0x00100000;
    SceSetEventCancel(1, (TaskFunc) r209_2ndBattleBowgunAppearEndProc, 0, 0xB, 1);
    CamCtrl.CutCall(0x12);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r209_work->task[3] = SceExec(0x12, (TaskFunc) r209_RotateDoor, 3, 0, SCE_PRIO_DEF_2, 0);
    CamCtrl.CutCall(0x13);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r209_2ndBattleBowgunAppearEndProc();
}

// End of the bowgun entrance (also its cancel path): the door tasks killed and the doors snapped, camera back, SceEventEnd.
static void r209_2ndBattleBowgunAppearEndProc()
{
    R209EmSet tbl[4] = {{9, 0x98, 3}, {0xB, 0x9A, 1}, {0xC, 0x9B, 0}, {0xA, 0x99, 2}};
    u32 i;

    if (pG->Room_flg[0] & 0x00100000) {
        for (i = 0; i < 4; i++) {
            r209_work->door[i].setClosed();
            if (r209_work->task[i] != NULL) {
                SceKill(r209_work->task[i]);
            }
            r209_work->em[tbl[i].em].w.setEm(tbl[i].no, 3, 1, 0, 0);
            r209_work->em[tbl[i].em].active = 1;
            r209_work->em[tbl[i].em].snipe = 1;
            r209_work->em[tbl[i].em].w.setPos(&r209_bowgunStartPos[i]);
            r209_work->em[tbl[i].em].w.setNoSuspend(0);
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExec(0x12, (TaskFunc) r209_BowgunMove, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Item area 0x82 taken (the battle's prize): camera cut 4 while salon door 3 (door[5]) opens; cancellable.
static void r209_2ndBattleFinish()
{
    SceAtDataReset(0x82);
    SceAtExecute(0x82);
    while (SceAtItemFlgCk(0x82) == 0) {
        SceSleep(1);
    }
    SceEventStart(1);
    pG->Room_flg[0] &= ~0x00100000;
    SceSetEventCancel(1, (TaskFunc) r209_2ndBattleFinishEndProc, 0, 0xB, 1);
    CamCtrl.CutCall(4);
    r209_work->door[5].setOpen();
    while (r209_work->door[5].getStatus() != 1) {
        SceSleep(1);
    }
    CamCtrl.CutCall(5);
    r209_work->door[4].setOpen();
    while (r209_work->door[4].getStatus() != 1) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r209_2ndBattleFinishEndProc();
}

// End of the finish cut: door[5] snapped open, camera back, SceEventEnd, Room_flg bit 4.
static void r209_2ndBattleFinishEndProc()
{
    if (pG->Room_flg[0] & 0x00100000) {
        r209_work->door[4].setOpened();
        r209_work->door[5].setOpened();
    }
    SceAtSetEnable(0, 1);
    r209_work->door4->setNormal();
    SmdGetObjPtr(2)->be_flag &= ~2;
    SmdGetObjPtr(3)->be_flag &= ~2;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    RsfSet(G_ROOM_ID, 4);
}

// Task: once (Room_flg bit 5) when the condition is met, camera cut 6 shows the bridge switch appearing; cancellable.
static void r209_SwitchAppearCheck()
{
    cObj* obj1 = SmdGetObjPtr(1);
    cObj* objB7 = SmdGetObjPtr(0xB7);

    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        SceMesSet(4, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        if (SceMesGetSelection() == 1) {
            RsfSet(G_ROOM_ID, 5);
            RoomSeCall(0x17, 0, 0, 0, 0);
            SceEventStart(1);
            CamCtrl.CutCall(6);
            EstSet(0, -1, 0, 0, EFF_ROOM, 0, 1, ESP_CORE_KIND_ROOM00, 0, 0);
            pG->Room_flg[0] &= ~0x00100000;
            SceSetEventCancel(1, (TaskFunc) r209_SwitchAppearCheckEnd, 0, 0xB, 1);
            int seId = RoomSeCall(0xD, 0, 0, 0, 0);
            const f32 lim = 11225.0f;
            const f32 limB = 8716.0f;
            // COMPILER-DIFF: candidate (sched1 order of the pool/work highs): the seId store through the
            // loaded work pointer has alias base 0, so ours chains it to the obj loads and issues
            // `lis work` before `lis lim`; the codeless asm ties the store to the lim load instead.
            R209Work* wp = r209_work;
            asm("" : "=r"(seId), "=r"(wp) : "0"(seId), "1"(wp), "f"(lim));
            wp->seId = seId;
            obj1->be_flag |= 0x20;
            objB7->be_flag |= 0x20;

            while (obj1->pos.y < lim) {
                obj1->pos.y += 50.0f;
                objB7->pos.y += 50.0f;
                SceSleep(1);
            }
            obj1->pos.y = lim;
            objB7->pos.y = limB;
            RoomSeCall(0xE, 0, 0, 0, 0);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            SceSetEventCancel(0, 0, 0, -1, 1);
            r209_SwitchAppearCheckEnd();
        }
    }
}

// End of the switch appearance: the switch snapped up, camera back, SceEventEnd.
static void r209_SwitchAppearCheckEnd()
{
    cObj* obj1 = SmdGetObjPtr(1);
    cObj* objB7 = SmdGetObjPtr(0xB7);

    if (pG->Room_flg[0] & 0x00100000) {
        if (r209_work->seId) {
            SndStop(r209_work->seId, 0);
        }
        EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
        EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
    }
    obj1->pos.y = 11225.0f;
    objB7->pos.y = 8716.0f;
    SceSleep(2);
    obj1->be_flag &= ~0x20;
    objB7->be_flag &= ~0x20;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r209_work->sat0 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r209_zeroVec, &r209_zeroVec, 2);
    r209_work->eat0 = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r209_zeroVec, &r209_zeroVec, 3);
    SceAtSetEnable(0x15, 0);
}

// Task: waits for the pot (a hit box) to be broken, then arms area 0x1B with the bridge appearance.
static void r209_PotBreakCheck()
{
    cEm* box;

    if (getRoomEtcBox(0x20, &box, 1) == 1) {
        while (box->hp > 0) {
            SceSleep(1);
        }
    }
    SceAtDataSet_exec(0x1B, SCE_LEVEL10, 0, (TaskFunc) r209_BridgeAppearCheck, 0, 1);
}

// Area 0x1B after the switch (bits 5 set, 8 clear): camera cut 0xE while the bridge object slides out
// (pos.z to its limit) with SE; cancellable.
static void r209_BridgeAppearCheck()
{
    cObj* obj = SmdGetObjPtr(0xAD);

    if (RsfCheck(G_ROOM_ID, 5) && RsfCheck(G_ROOM_ID, 8) == 0) {
        SceMesSet(5, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        if (SceMesGetSelection() == 1) {
            RoomSeCall(0x17, 0, 0, 0, 0);
            RsfSet(G_ROOM_ID, 8);
            SceAtSetEnable(0x1B, 0);
            SceEventStart(1);
            EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_ROOM00, 0, 0);
            CamCtrl.CutCall(0xE);
            pG->Room_flg[0] &= ~0x00100000;
            SceSetEventCancel(1, (TaskFunc) r209_BridgeAppearCheckEnd, 0, 0xB, 1);
            obj->be_flag |= 0x20;
            int seId = RoomSeCall(0xF, 0, 0, 0, 0);
            const f32 lim = 3322.0f;
            // COMPILER-DIFF: candidate (sched1 order of the pool/work highs), see r209_SwitchAppearCheck
            R209Work* wp = r209_work;
            asm("" : "=r"(seId), "=r"(wp) : "0"(seId), "1"(wp), "f"(lim));
            wp->seId = seId;
            while (obj->pos.z < lim) {
                obj->pos.z += 50.0f;
                SceSleep(1);
            }
            obj->pos.z = 3322.0f;
            RoomSeCall(0x10, 0, 0, 0, 0);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            SceSetEventCancel(0, 0, 0, -1, 1);
            r209_BridgeAppearCheckEnd();
        }
    } else {
        SceMesSet(6, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    }
}

// End of the bridge appearance: the bridge and its four panels snapped, camera back, SceEventEnd, the panel puzzle armed.
static void r209_BridgeAppearCheckEnd()
{
    cObj* obj = SmdGetObjPtr(0xAD);
    cEmDoor* door;

    if (pG->Room_flg[0] & 0x00100000) {
        if (r209_work->seId) {
            SndStop(r209_work->seId, 0);
        }
        EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
        EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
    }
    obj->pos.z = 3322.0f;
    SceSleep(2);
    obj->be_flag &= ~0x20;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r209_work->sat1 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r209_zeroVec, &r209_zeroVec, 1);
    r209_work->eat1 = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r209_zeroVec, &r209_zeroVec, 2);
    SatMgr.destroy(r209_work->sat2);
    r209_work->em[21].w.setEm(0xA1, 3, 1, 0, 0);
    r209_work->em[22].w.setEm(0xA2, 3, 1, 0, 0);
    // the rest of the function inside the hit arm: the sleep body is laid out after it (`beq SLEEP; ..; b END`)
    // and `lis work@ha` / `&door` are hoisted before the loop
    while (1) {
        if (SceAtHitCheck(0x25) != 0) {
            getRoomEtcDoor(0x21, &door, 1);
            if (door != NULL && door->hp > 0 && (door->flag & 0x10000000) == 0) {
                SceSleep(1);
            }
            r209_work->em[21].w.setFlag(1);
            r209_work->em[22].w.setFlag(1);
            break;
        }
        SceSleep(1);
    }
}

// Picture `no` (0..2) chosen: the two Ganados hidden behind it (list 3 pairs 0x9D/0x9E, 0x9F/0xA0,
// 0xA1/0xA2) are set, camera cut 0xF while the picture lift (door[6]) opens; cancellable.
static void r209_OpenPicture(int no)
{
    pG->Room_flg[0] |= 0x04000000;
    while (r209_work->door[6].getStatus() != 0) {
        SceSleep(1);
    }
    // unsigned index: `cmplwi 1; blt case0` without a `cmpwi 0` test
    switch ((u32) no) {
    case 0:
        r209_work->picEm[0].setEm(0x9D, 3, 1, 0, 0);
        r209_work->picEm[1].setEm(0x9E, 3, 1, 0, 0);
        break;
    case 1:
        r209_work->picEm[0].setEm(0x9F, 3, 1, 0, 0);
        r209_work->picEm[1].setEm(0xA0, 3, 1, 0, 0);
        break;
    case 2:
        r209_work->picEm[0].setEm(0xA1, 3, 1, 0, 0);
        r209_work->picEm[1].setEm(0xA2, 3, 1, 0, 0);
        break;
    }
    SceEventStart(1);
    r209_work->picEm[0].setNoSuspend(1);
    r209_work->picEm[1].setNoSuspend(1);
    CamCtrl.CutCall(0xF);
    pG->Room_flg[0] &= ~0x00100000;
    SceSetEventCancel(1, (TaskFunc) r209_OpenPictureEndProc, 0, 0xB, 1);
    r209_work->door[6].setOpen();
    while (r209_work->door[6].getStatus() != 1) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r209_OpenPictureEndProc();
}

// End of the picture opening: the lift snapped open, camera back, SceEventEnd.
static void r209_OpenPictureEndProc()
{
    if (pG->Room_flg[0] & 0x00100000) {
        r209_work->door[6].setOpened();
    }
    CamCtrl.Comeback(0);
    r209_work->picEm[0].setNoSuspend(0);
    r209_work->picEm[1].setNoSuspend(0);
    SceEventEnd(0);
    r209_work->picEm[0].setFlag(1);
    r209_work->picEm[1].setFlag(1);
    SceSleep(150);
    SceExec(0x12, (TaskFunc) r209_ClosePicture, 0, 0, SCE_PRIO_DEF_2, 0);
}

// The picture lift (door[6]) closes again and the task waits for it.
static void r209_ClosePicture()
{
    r209_work->door[6].setClose();
    while (r209_work->door[6].getStatus() != 0) {
        SceSleep(1);
    }
    r209_work->picEm[0].destroy();
    r209_work->picEm[1].destroy();
    pG->Room_flg[0] &= ~0x04000000;
}

// Every frame of the second battle: the bowgun enemies on the balcony are sent to the position
// facing the area the player stands in (atOn bits 128..131 = the four sides, bits 0/2/4/6 = the
// near half of each side).
extern "C" void r209_BowgunCtrl()
{
    R209Work* wp = r209_work;
    u32 n = wp->snipeCnt;
    int f;
    u32 hit;

    if (n == 0) {
        return;
    }
    if (FlagChk(wp->atOn, 128)) {
        hit = FlagChkVar(wp->atOn, 0);
        f = hit == 0;
        switch (n) {
        case 1:
            r209_BowgunActionSet1(0, f, 8);
            break;
        case 2:
            r209_BowgunActionSet1(0, f, 6);
            r209_BowgunActionSet1(1, f, 10);
            break;
        case 3:
            r209_BowgunActionSet1(0, f, 6);
            r209_BowgunActionSet1(1, f, 8);
            r209_BowgunActionSet1(2, f, 10);
            break;
        case 4:
            r209_BowgunActionSet1(0, f, 6);
            r209_BowgunActionSet1(1, f, 7);
            r209_BowgunActionSet1(2, f, 9);
            r209_BowgunActionSet1(3, f, 10);
            break;
        }
    } else if (FlagChk(wp->atOn, 129)) {
        hit = FlagChk(wp->atOn, 2);
        f = hit == 0;
        switch (n) {
        case 1:
            r209_BowgunActionSet2(0, f, 5);
            break;
        case 2:
            r209_BowgunActionSet2(0, f, 4);
            r209_BowgunActionSet2(1, f, 6);
            break;
        case 3:
            r209_BowgunActionSet2(0, f, 4);
            r209_BowgunActionSet2(1, f, 5);
            r209_BowgunActionSet2(2, f, 6);
            break;
        case 4:
            r209_BowgunActionSet2(0, f, 4);
            r209_BowgunActionSet2(1, f, 3);
            r209_BowgunActionSet2(2, f, 7);
            r209_BowgunActionSet2(3, f, 6);
            break;
        }
    } else if (FlagChk(wp->atOn, 130)) {
        hit = FlagChk(wp->atOn, 4);
        f = hit == 0;
        switch (n) {
        case 1:
            r209_BowgunActionSet3(0, f, 2);
            break;
        case 2:
            r209_BowgunActionSet3(0, f, 0);
            r209_BowgunActionSet3(1, f, 4);
            break;
        case 3:
            r209_BowgunActionSet3(0, f, 0);
            r209_BowgunActionSet3(1, f, 2);
            r209_BowgunActionSet3(2, f, 4);
            break;
        case 4:
            r209_BowgunActionSet3(0, f, 0);
            r209_BowgunActionSet3(1, f, 1);
            r209_BowgunActionSet3(2, f, 3);
            r209_BowgunActionSet3(3, f, 4);
            break;
        }
    } else if (FlagChk(wp->atOn, 131)) {
        hit = FlagChk(wp->atOn, 6);
        f = hit == 0;
        switch (n) {
        case 1:
            r209_BowgunActionSet4(0, f, 0xB);
            break;
        case 2:
            r209_BowgunActionSet4(0, f, 0);
            r209_BowgunActionSet4(1, f, 0xA);
            break;
        case 3:
            r209_BowgunActionSet4(0, f, 0);
            r209_BowgunActionSet4(1, f, 0xA);
            r209_BowgunActionSet4(2, f, 0xB);
            break;
        case 4:
            r209_BowgunActionSet4(0, f, 9);
            r209_BowgunActionSet4(1, f, 0xA);
            r209_BowgunActionSet4(2, f, 0);
            r209_BowgunActionSet4(3, f, 1);
            break;
        }
    }
}

// Path of sniper `i` toward position `pt` when the player is on side 0: the corner waypoints
// (r209_bowgunPos2) are inserted by the side the enemy stands on (atCur bits 8 + i * 8 ..).
extern "C" void r209_BowgunActionSet1(int i, int flag, int pt)
{
    R209Em* e = &r209_work->em[r209_work->snipeIdx[i]];
    u32* at;
    u32 c0;
    int cnt = 0;

    e->pathReset = 1;
    memclr_asm(e->path, 0x14);
    at = r209_work->atCur;
    c0 = R209_SNIPE_AT(at, i, 0);
    if ((c0 && R209_SNIPE_AT(at, i, 1)) || R209_SNIPE_AT(at, i, 1)) {
        e->path[cnt] = &r209_bowgunPos2[3];
        cnt = 1;
        if (pt >= 8 && pt <= 10) {
            e->path[cnt] = &r209_bowgunPos2[2];
            cnt = 2;
        }
    } else if (R209_SNIPE_AT(at, i, 6) || R209_SNIPE_AT(at, i, 7)) {
        if (pt >= 8 && pt <= 10) {
            e->path[cnt] = &r209_bowgunPos2[2];
            cnt = 1;
        }
    } else if (c0) {
        e->path[cnt] = &r209_bowgunPos2[1];
        cnt = 1;
        if (pt >= 6 && pt <= 8) {
            e->path[cnt] = &r209_bowgunPos2[2];
            cnt = 2;
        }
    } else if (R209_SNIPE_AT(at, i, 2) || R209_SNIPE_AT(at, i, 3)) {
        if (pt >= 6 && pt <= 8) {
            e->path[cnt] = &r209_bowgunPos2[2];
            cnt = 1;
        }
    }
    e->path[cnt] = &r209_bowgunPos[pt];
}

// Bowgun Ganado `i` (snipeIdx) gets path variant 2 toward balcony point `pt`: the way points of
// r209_bowgunPos2 / r209_bowgunPos chosen by which side `pt` is on; the path restarts.
extern "C" void r209_BowgunActionSet2(int i, int flag, int pt)
{
    R209Em* e = &r209_work->em[r209_work->snipeIdx[i]];
    int cnt = 0;

    e->pathReset = 1;
    memclr_asm(e->path, 0x14);
    if (r209_SnipeAt(r209_work->atCur, i, 0) && r209_SnipeAt(r209_work->atCur, i, 1)) {
        if (pt >= 5 && pt <= 7) {
            e->path[cnt] = &r209_bowgunPos2[3];
            cnt = 1;
        }
    } else if (r209_SnipeAt(r209_work->atCur, i, 2) || r209_SnipeAt(r209_work->atCur, i, 3)
               || r209_SnipeAt(r209_work->atCur, i, 0)) {
        e->path[cnt] = &r209_bowgunPos2[0];
        cnt = 1;
        if (pt >= 5 && pt <= 7) {
            e->path[cnt] = &r209_bowgunPos2[3];
            cnt = 2;
        }
    } else if (r209_SnipeAt(r209_work->atCur, i, 4) || r209_SnipeAt(r209_work->atCur, i, 5)) {
        if (pt >= 3 && pt <= 5) {
            e->path[cnt] = &r209_bowgunPos2[3];
            cnt = 1;
        }
    }
    e->path[cnt] = &r209_bowgunPos[pt];
}

// Bowgun Ganado `i` gets path variant 3 toward `pt` (the other side's way points).
extern "C" void r209_BowgunActionSet3(int i, int flag, int pt)
{
    R209Em* e = &r209_work->em[r209_work->snipeIdx[i]];
    u32* at;
    u32 c0;
    int cnt = 0;

    e->pathReset = 1;
    memclr_asm(e->path, 0x14);
    at = r209_work->atCur;
    c0 = R209_SNIPE_AT(at, i, 4);
    if ((c0 && R209_SNIPE_AT(at, i, 5)) || R209_SNIPE_AT(at, i, 5)) {
        e->path[cnt] = &r209_bowgunPos2[3];
        cnt = 1;
        if (pt >= 0 && pt <= 2) {
            e->path[cnt] = &r209_bowgunPos2[0];
            cnt = 2;
        }
    } else if (R209_SNIPE_AT(at, i, 6) || R209_SNIPE_AT(at, i, 7)) {
        if (pt >= 0 && pt <= 2) {
            e->path[cnt] = &r209_bowgunPos2[0];
            cnt = 1;
        }
    } else if (c0) {
        e->path[cnt] = &r209_bowgunPos2[1];
        cnt = 1;
        if (pt >= 2 && pt <= 4) {
            e->path[cnt] = &r209_bowgunPos2[0];
            cnt = 2;
        }
    } else if (R209_SNIPE_AT(at, i, 2) || R209_SNIPE_AT(at, i, 3)) {
        if (pt >= 2 && pt <= 4) {
            e->path[cnt] = &r209_bowgunPos2[0];
            cnt = 1;
        }
    }
    e->path[cnt] = &r209_bowgunPos[pt];
}

// Bowgun Ganado `i` gets path variant 4 toward `pt`.
extern "C" void r209_BowgunActionSet4(int i, int flag, int pt)
{
    R209Em* e = &r209_work->em[r209_work->snipeIdx[i]];
    u32* at;
    u32 c0;
    int cnt = 0;

    e->pathReset = 1;
    memclr_asm(e->path, 0x14);
    at = r209_work->atCur;
    c0 = R209_SNIPE_AT(at, i, 0);
    if ((c0 && R209_SNIPE_AT(at, i, 1)) || R209_SNIPE_AT(at, i, 0)) {
        if (pt >= 9 && pt <= 11) {
            e->path[cnt] = &r209_bowgunPos2[1];
            cnt = 1;
        }
    } else if (R209_SNIPE_AT(at, i, 6) || R209_SNIPE_AT(at, i, 7) || R209_SNIPE_AT(at, i, 1)) {
        e->path[cnt] = &r209_bowgunPos2[0];
        cnt = 1;
        if (pt >= 9 && pt <= 11) {
            e->path[cnt] = &r209_bowgunPos2[1];
            cnt = 2;
        }
    } else if (R209_SNIPE_AT(at, i, 4) || R209_SNIPE_AT(at, i, 5)) {
        if ((pt >= 0 && pt <= 1) || pt == 11) {
            e->path[cnt] = &r209_bowgunPos2[1];
            cnt = 1;
        }
    }
    e->path[cnt] = &r209_bowgunPos[pt];
}

// Task: walks every active bowgun enemy along its waypoint list.
static void r209_BowgunMove()
{
    u32 i;

    for (;;) {
        for (i = 0; i < 8; i++) {
            R209Em* e = &r209_work->em[r209_snipeEmNo[i]];

            if (e->w.isActive() && e->snipe != 0) {
                if (e->pathReset != 0) {
                    e->w.setFindPL();
                    e->pathIdx = 0;
                    e->w.setGoto(e->path[e->pathIdx], 1);
                    e->pathReset = 0;
                    e->pathIdx++;
                } else if (e->w.ckGoto() != 1) {
                    Vec* p;

                    e->w.setFindPL();
                    p = e->path[e->pathIdx];
                    if (p) {
                        e->w.setGoto(p, 1);
                        e->pathIdx++;
                    }
                }
            }
        }
        SceSleep(1);
    }
}

// Event r209s00 (the leader's escape through the lift): the scroll lift objects follow the event.
static void Evt_R209S00_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* o;

    switch (e->FuncType) {
    case 0:
        r209_work->door4->setClose();
        r209_work->door4->setCloseLock();
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "evm4300", 0, 0) == 1) {
                cLight* l = LightMgr.getKindLight(1);

                if (l != 0) {
                    l->setParent((cModel*) mod);
                }
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                o = SmdGetObjPtr(2);
                if (o) {
                    e->SetMod("scr0000", o, 5, 0, 2, 0);
                    o->setPos(&pos);
                    o->setAng(&rot);
                    o->be_flag |= 0x20;
                    e->EspSetModelPtr(o);
                }
                o = SmdGetObjPtr(3);
                if (o) {
                    e->SetMod("scr0001", o, 5, 0, 2, 0);
                    o->setPos(&pos);
                    o->setAng(&rot);
                    o->be_flag |= 0x20;
                    e->EspSetModelPtr(o);
                }
                SmdGetObjPtr(2)->be_flag &= ~2;
                SmdGetObjPtr(3)->be_flag &= ~2;
            }
            break;
        case 0xD:
            if (e->NowFrame == 0) {
                SmdGetObjPtr(2)->be_flag |= 2;
                SmdGetObjPtr(3)->be_flag |= 2;
            }
            break;
        }
        break;
    case 2: {
        SmdWork* w;

        SmdGetObjPtr(2)->be_flag |= 2;
        SmdGetObjPtr(3)->be_flag |= 2;
        w = SmdGetWorkPtr(2);
        o = SmdGetObjPtr(2);
        if (o && w) {
            o->setPos(&w->pos);
            o->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(3);
        o = SmdGetObjPtr(3);
        if (o && w) {
            o->setPos(&w->pos);
            o->setAng(&w->rot);
        }
        break;
    }
    }
}

// Task (AT 0x1E): the bridge panel puzzle menu. Selections 1-4 flip pairs/triples of panels,
// 5 leaves, 6 leaves and flips nothing more; the puzzle is solved when the four panels show the
// pattern of `chk`.
static void r209_PanelPuzzle()
{
    cObj* tbl[4];
    u8 chk[4][2] = {{1, 2}, {3, 2}, {2, 1}, {2, 3}};
    int loop = 1;
    int quit = 0;
    u32 i;
    u8 sum;

    SceEventStart(1);
    CamCtrl.CutCall(9);
    SceSleep(1);
    SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    do {
        tbl[3] = 0;
        tbl[2] = 0;
        tbl[1] = 0;
        tbl[0] = 0;
        SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        switch (SceMesGetSelection()) {
        case 1:
            tbl[0] = r209_work->bridge[0];
            tbl[1] = r209_work->bridge[1];
            break;
        case 2:
            tbl[0] = r209_work->bridge[0];
            tbl[1] = r209_work->bridge[1];
            tbl[2] = r209_work->bridge[2];
            break;
        case 3:
            tbl[1] = r209_work->bridge[1];
            tbl[2] = r209_work->bridge[2];
            tbl[3] = r209_work->bridge[3];
            break;
        case 4:
            tbl[2] = r209_work->bridge[2];
            tbl[3] = r209_work->bridge[3];
            break;
        case 6:
            quit = 1;
        case 5:
            loop = 0;
            break;
        }
        if (tbl[0] != 0 || tbl[1] != 0 || tbl[2] != 0 || tbl[3] != 0) {
            r209_PanelRotate(tbl);
        }
    } while (loop != 0);
    sum = 0;
    for (i = 0; i < 4; i++) {
        sum += chk[i][r209_work->panel[i]];
    }
    if (sum == 6) {
        if (quit == 0) {
            r209_PanelPazzleEnd();
            return;
        }
    } else if (quit == 0) {
        SceMesSet(7, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    }
    for (i = 0; i < 4; i++) {
        tbl[i] = 0;
        if (r209_work->panel[i] == 1) {
            tbl[i] = r209_work->bridge[i];
        }
    }
    if (tbl[0] != 0 || tbl[1] != 0 || tbl[2] != 0 || tbl[3] != 0) {
        r209_PanelRotate(tbl);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Turns the panels of `tbl` half a turn and flips their state.
extern "C" void r209_PanelRotate(cObj** tbl)
{
    f32 t = 0.0f;
    u32 i;

    RoomSeCall(0x13, 0, 0, 0, 0);
    while (t < PI) {
        for (i = 0; i < 4; i++) {
            if (tbl[i]) {
                tbl[i]->ang.y += 0.1f;
                tbl[i]->matUpdate();
            }
        }
        t += 0.1f;
        SceSleep(1);
    }
    for (i = 0; i < 4; i++) {
        if (tbl[i]) {
            r209_work->panel[i] ^= 1;
            tbl[i]->ang.y = r209_work->panel[i] != 0 ? PI : 0.0f;
            tbl[i]->matUpdate();
        }
    }
}

// The solved bridge slides out.
extern "C" void r209_PanelPazzleEnd()
{
    u32 i;

    CamCtrl.CutCall(0x10);
    RoomSeCall(0x14, 0, 0, 0, 0);
    r209_work->bridgeObj->be_flag |= 0x20;
    if (r209_work->bridgeObj->pos.z < 14530.0f) {
        f32 spd = 75.0f;

        do {
            r209_work->bridgeObj->pos.z += spd;
            for (i = 0; i < 4; i++) {
                r209_work->bridge[i]->pos.z += spd;
                r209_work->bridge[i]->matUpdate();
            }
            SceSleep(1);
        } while (r209_work->bridgeObj->pos.z < 14530.0f);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r209_PanelPazzleEndEndProc();
}

// End of the panel puzzle (also its cancel path): the bridge object snapped to z 14530 with its four
// panels re-attached, the puzzle areas (0x1E, 0x21, 0x2E..0x31) off.
extern "C" void r209_PanelPazzleEndEndProc()
{
    u32 i;

    r209_work->bridgeObj->pos.z = 14530.0f;
    for (i = 0; i < 4; i++) {
        PSVECAdd(&r209_work->bridgeObj->pos, &r209_work->bridgeOfs[i], &r209_work->bridge[i]->pos);
    }
    SceAtSetEnable(0x1E, 0);
    *(u32*) (RoomData.getRoomSavePtr(pG->room_id) + 4) |= 0x02000000;
    SceAtSetEnable(0x21, 0);
    SceAtSetEnable(0x2E, 0);
    SceAtSetEnable(0x2F, 0);
    SceAtSetEnable(0x30, 0);
    SceAtSetEnable(0x31, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    GameSave.save(pSaveData, -1);
}

// Task (AT 0x2E..0x31): the message of panel `no` for its current state.
static void r209_PanelMes(int no)
{
    int tbl[4][2] = {{8, 9}, {10, 11}, {12, 13}, {14, 15}};

    SceUpCut(tbl[no][r209_work->panel[no]], -1, -1, 0);
}

// Task (AT 0xD): the streamed music stops when the player leaves the area.
static void r209_StrStopReqFunc(int atNo)
{
    if (r209_work->strId != 0) {
        SndStrReq(r209_work->strId, 4, 600, 0);
        r209_work->strId = 0;
        SceAtSetEnable(atNo, 0);
    }
}

// Task: bowgun enemy `no` (table entry) comes out of its balcony door, walks to its start
// position, the door closes behind it.
static void r209_RotateDoor(int no)
{
    int atNo[4] = {0xE, 0xF, 0x10, 0x11};
    R209EmSet tbl[8] = {
        {9, 0x98, 3},
        {0xB, 0x9A, 1},
        {0xC, 0x9B, 0},
        {0xA, 0x99, 2},
        {7, 0x90, 2},
        {8, 0x91, 0},
        {0xD, 0x87, 2},
        {0xE, 0x8A, 3},
    };
    int d = tbl[no].point;
    int id = tbl[no].no;
    int em = tbl[no].em;
    R209Em* e;

    while (r209_work->door[d].getStatus() != 0) {
        SceSleep(1);
    }
    e = &r209_work->em[em];
    if (e->w.setEm(id, 3, 1, 0, 0) == 0) {
        SceExit();
    }
    r209_work->em[em].active = 1;
    e = &r209_work->em[em];
    e->w.setNoSuspend(1);
    r209_work->door[d].setOpen();
    while (r209_work->door[d].getStatus() != 1) {
        SceSleep(1);
    }
    e->w.setGoto(&r209_bowgunStartPos[d], 1);
    while (SceAtCheckHitModel(atNo[d], e->w.getPtr()) != 0 && e->w.isActive() != 0) {
        SceSleep(1);
    }
    r209_work->em[em].snipe = 1;
    // Pointer-arithmetic element access: the address is formed as (W + d*64) + 0x674 (`add; lwz`);
    // door[d].sat folds the work offset first ((W + 0x674) + d*64: `addi; lwzx`).
    (*(r209_work->door + d)).sat->setEnable();
    SceSleep(0x1E);
    r209_work->door[d].setClose();
    while (r209_work->door[d].getStatus() != 0) {
        SceSleep(1);
    }
    e->w.setNoSuspend(0);
}

// Item-event opener: chest `id` lid up (+X); its item area 0x82 then triggers the battle finish.
static void r209_TreasureBoxOpen(int id)
{
    OpenBoxMain(OpenBoxUpXP, 0, 0x5B, id, -1, -1);
    SceAtDataSet_exec(0x82, SCE_LEVEL10, 0, (TaskFunc) r209_2ndBattleFinish, 0, 1);
}

// Item-event "already opened": chest `id` posed open.
static void r209_TreasureBoxOpened(int id)
{
    OpenBoxMain(OpenBoxUpXP, 1, 0x5B, id, -1, -1);
}

// Task: the battle music plays while enemies 0x10..0x1F are alive.
static void r209_StrPlayCk()
{
    for (;;) {
        while (SceCountEmAlive(0x10, 0x20) == 0) {
            SceSleep(1);
        }
        SndRoomStrStart(1, 0, 1);
        while (SceCountEmAlive(0x10, 0x20) != 0) {
            SceSleep(1);
        }
        SndRoomStrStop(3);
    }
}

// Bind door `id`: the four balcony doors rotate about Y (with a collision piece), doors 2/3 and the
// lift 0xA1 rise; the closed pose, opening height and door flag bit are set per id.
void cR209Door::init(u32 id_)
{
    Vec zero = {0.0f, 0.0f, 0.0f};

    valid = 0;
    id = id_;
    obj = SmdGetObjPtr(id_);
    if (obj) {
        obj->be_flag |= 0x20;
        pos0 = obj->pos;
        rotY0 = obj->ang.y;
        openH = 0.0f;
        switch (id) {
        case 0xA1:
            openH = 1738.0f;
            eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &obj->pos, &obj->ang, 4);
            break;
        case 2:
            flagNo = SCF_8b;
            openH = 2700.0f;
            break;
        case 3:
            flagNo = SCF_74;
            openH = 2700.0f;
            break;
        case 0xB3:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 8);
            break;
        case 0xB4:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 7);
            break;
        case 0xB5:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 6);
            break;
        case 0xB6:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 5);
            break;
        }
        mode = 0;
        valid = 1;
    }
}

// Per-frame step: run the current mode (wait / open / close) of r209_doorTbl.
void cR209Door::move()
{
    if (valid != 0) {
        (this->*r209_doorTbl[mode])();
    }
}

// Mode 0: idle.
void cR209Door::wait()
{
}

// Mode 1: open SE; a rotating door turns to +-90 degrees, a rising one climbs to pos0 + openH; on
// arrival its collision / area follow and status 1.
void cR209Door::open()
{
    int se;

    switch (step) {
    case 0:
        switch (id) {
        case 0xA1:
            se = 0x11;
            break;
        case 3:
            se = 3;
            break;
        case 2:
            se = 5;
            break;
        case 0xB3:
            se = 0x18;
            break;
        case 0xB4:
            se = 0x1A;
            break;
        case 0xB5:
            se = 0x1C;
            break;
        case 0xB6:
            se = 0x1E;
            break;
        default:
            se = -1;
            break;
        }
        if (se != -1) {
            this->se = RoomSeCall((u16) se, &obj->pos, 0, 0, obj);
        }
        step++;
    case 1:
        switch (id) {
        case 2:
        case 3:
        case 0xA1:
            obj->pos.y += 40.0f;
            if (obj->pos.y > pos0.y + openH) {
                obj->pos.y = pos0.y + openH;
                step++;
            }
            if (eat) {
                eat->setCoord(&obj->pos, &obj->ang);
            }
            break;
        default:
            obj->ang.y += 0.05f;
            if (obj->ang.y > PI) {
                obj->ang.y = -PI;
                step++;
            }
            break;
        }
        break;
    case 2:
        switch (id) {
        case 0xA1:
            se = 0x12;
            break;
        case 2:
            se = 6;
            r209_work->door4->setNormal();
            break;
        case 3:
            se = 4;
            SceAtSetEnable(0, 1);
            break;
        case 0xB3:
            se = 0x19;
            sat->setDisable();
            break;
        case 0xB4:
            se = 0x1B;
            sat->setDisable();
            break;
        case 0xB5:
            se = 0x1D;
            sat->setDisable();
            break;
        case 0xB6:
            se = 0x1F;
            sat->setDisable();
            break;
        default:
            se = -1;
            break;
        }
        if (se != -1) {
            this->se = RoomSeCall((u16) se, &obj->pos, 0, 0, obj);
        }
        obj->pos = pos0;
        obj->pos.y += openH;
        status = 1;
        mode = 0;
        step = 0;
        if (flagNo) {
            FlagOnVar(&pG->Scenario_flg, (u32) flagNo);
        }
        break;
    }
}

// Mode 2: close SE; rotate back / drop with growing speed to the closed pose, then a short shake; status 0.
void cR209Door::close()
{
    int se;

    switch (step) {
    case 0:
        switch (id) {
        case 0xA1:
            spd = -40.0f;
            se = 0x11;
            break;
        case 3:
            SceAtSetEnable(0, 0);
            spd = -50.0f;
            se = -1;
            break;
        case 2:
            r209_work->door4->setCloseLock();
            spd = -50.0f;
            se = -1;
            break;
        case 0xB3:
            sat->setEnable();
            se = 0x18;
            break;
        case 0xB4:
            sat->setEnable();
            se = 0x1A;
            break;
        case 0xB5:
            sat->setEnable();
            se = 0x1C;
            break;
        case 0xB6:
            sat->setEnable();
            se = 0x1E;
            break;
        default:
            se = -1;
            break;
        }
        if (se != -1) {
            this->se = RoomSeCall((u16) se, &obj->pos, 0, 0, obj);
        }
        step++;
    case 1:
        switch (id) {
        case 2:
        case 3:
            spd -= 10.0f;
        case 0xA1:
            obj->pos.y += spd;
            if (obj->pos.y < pos0.y) {
                obj->pos.y = pos0.y;
                step++;
            }
            if (eat) {
                eat->setCoord(&obj->pos, &obj->ang);
            }
            break;
        default:
            obj->ang.y += 0.05f;
            if (obj->ang.y > 0.0f) {
                obj->ang.y = 0.0f;
                step++;
            }
            break;
        }
        break;
    case 2:
        switch (id) {
        case 0xA1:
            se = 0x12;
            break;
        case 2:
        case 3:
            se = -1;
            break;
        case 0xB3:
            se = 0x19;
            break;
        case 0xB4:
            se = 0x1B;
            break;
        case 0xB5:
            se = 0x1D;
            break;
        case 0xB6:
            se = 0x1F;
            break;
        default:
            se = -1;
            break;
        }
        if (se != -1) {
            this->se = RoomSeCall((u16) se, &obj->pos, 0, 0, obj);
        }
        status = 0;
        mode = 0;
        step = 0;
        if (flagNo) {
            FlagOffVar(&pG->Scenario_flg, (u32) flagNo);
        }
        break;
    }
}

// Request opening (mode 1) unless open / opening; status 4 = moving.
void cR209Door::setOpen()
{
    if (valid == 0) {
        return;
    }
    if (status == 1) {
        return;
    }
    if (mode == 1) {
        return;
    }
    status = 4;
    mode = 1;
    step = 0;
}

// Request closing (mode 2) unless closed / closing.
void cR209Door::setClose()
{
    if (valid == 0) {
        return;
    }
    if (status == 0) {
        return;
    }
    if (mode == 2) {
        return;
    }
    status = 4;
    mode = 2;
    step = 0;
}

// Snap the door open (pose, collision, door flag set, SE stopped).
void cR209Door::setOpened()
{
    if (valid == 0) {
        return;
    }
    status = 1;
    mode = 0;
    step = 0;
    obj->pos.y = pos0.y + openH;
    switch (id) {
    case 3:
        SceAtSetEnable(0, 1);
        break;
    case 2:
        r209_work->door4->setNormal();
        break;
    case 0xA1:
        break;
    default:
        obj->ang.y = -PI;
        sat->setDisable();
        break;
    }
    SndStop(se, 0);
    if (flagNo) {
        FlagOnVar(&pG->Scenario_flg, (u32) flagNo);
    }
}

// Snap the door closed (pose, collision, door flag cleared, SE stopped).
void cR209Door::setClosed()
{
    if (valid == 0) {
        return;
    }
    status = 0;
    mode = 0;
    step = 0;
    obj->pos.y = pos0.y;
    switch (id) {
    case 3:
        SceAtSetEnable(0, 0);
        break;
    case 2:
        r209_work->door4->setCloseLock();
        break;
    case 0xA1:
        break;
    default:
        obj->ang.y = 0.0f;
        sat->setEnable();
        break;
    }
    SndStop(se, 0);
    if (flagNo) {
        FlagOffVar(&pG->Scenario_flg, (u32) flagNo);
    }
}

// 0 closed, 1 open, 4 moving; -1 when the door has no object.
int cR209Door::getStatus()
{
    if (valid != 0) {
        return status;
    }
    return -1;
}

// STUBS_END
