#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "st_mgr_event.h"
#include "act_btn.h"
#include "mes.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "motion.h"
#include "math_sub.h"
#include "quake.h"
#include "pad.h"
#include "room_data.h"

// Room 3-18 (D:/Bio4/Prog/r318.cpp): the laser corridor. Three automatic double doors, fifteen
// laser emitters with five patterns (dodged with the action button), the rest-stop event on the
// bench and the elevator to r31a.

// One automatic door: the two halves slide apart along z.
struct R318Door {
    cObj* obj0;    // 0x00
    cObj* obj1;    // 0x04
    f32 z0;        // 0x08  closed z of obj0
    f32 z1;        // 0x0C  closed z of obj1
    u8 open;       // 0x10  1 while the door flag is on
    u8 done;       // 0x11  the halves reached their end position
    u8 pad_12[2];
    cSat* sat0;    // 0x14
    cSat* sat1;    // 0x18
    cSat* eat0;    // 0x1C
    cSat* eat1;    // 0x20
};

struct R318Work {
    R318Door door[3];        // 0x00
    u8 pad_6C[0x90 - 0x6C];
    cObj* laser[15];         // 0x90
    Vec plPos;               // 0xCC  player position saved by the events
    Vec plRot;               // 0xD8
    int xE4;                 // 0xE4
    int escCount;            // 0xE8  playerEscape03: frames until the lasers restart
    int dodgeTimer;          // 0xEC  frames the dodge prompt stays
    int escFrame;            // 0xF0  playerEscape03: frames since the escape started
    u32 str;                 // 0xF4  SndStrReq / SndStrPlayBlock handle
    u32 laserSnd;            // 0xF8  laser hum SndCall handle
};

// sce_com.cpp SceElevatorData
struct SceElevatorData {
    s32 dir;
    u32 objId;
    Vec pos;
    Vec plPos;
    Vec plRot;
    s32 cut;
    u16 pad_30;
    u16 seStart;
    u16 pad_34;
    u16 seStop;
    Vec jumpPos;
    Vec jumpRot;
    u16 room;
};


// Effect sequence record tail: the second position at cEsp+0x100 (a laser beam end point).
struct R318EspView {
    u8 pad_0[0x100];
    Vec pos2;
};


static R318Work* r318_work;

static SceElevatorData r318_elvArrive = {0, 3, {0.0f, 0.0f, 0.0f}, {27850.0f, 826.0f, 4380.0f}, {0.0f, -1.48f, 0.0f}, 2, 0, 2, 0, 1, {3085.0f, 0.0f, -100.0f}, {0.0f, 1.35f, 0.0f}, 0x31A};
static SceElevatorData r318_elvLeave = {1, 3, {0.0f, 0.0f, 0.0f}, {27850.0f, 826.0f, 4380.0f}, {0.0f, -1.48f, 0.0f}, 2, 0, 0, 0, 1, {3085.0f, 0.0f, -100.0f}, {0.0f, 1.35f, 0.0f}, 0x31A};

// cObjScr (game/obj02.cpp) is not in a header: the callback setter of a scripted map object.
void cObjScrSetCallBack(cObj* o, void (*func)(cObj*)) asm("SetCallBack__7cObjScrPFP4cObj_v");

// The laser hit: rumble, quake and the death routine.
static inline void LaserHit();

static void R318ExecSitMain();
static void R318ExecSitEnd();
void R318LaserCallBackFunc(cObj* obj);
void DrawLaserLine(Vec* a, Vec* b, int r, int g, int b_, int alpha, f32 len);
static void R318AutoDoorMgr(int no);
void R318AutoDoorInit(int no, u32 id1, u32 id0);
void R318AutoDoorReset(int no);
void R318AutoDoor(int no, int flagNo, u32 id0, u32 id1);
static void R318ExecSwitchClear();
static void R318ExecSwitchCheck();
void R318ExecSwitchCheckEnd();
static void R318EventLaserStMain();
static void R226EventLaserStEnd();
static void R318EventLaserMgr();
void R318LaserEspInit(int n, int type, int kind);
static void R318EventLaserMove(int no);
void R318EventLaserEnd(int no);
static void playerEscape02(cPlayer* pl);
static void playerEscape03(cPlayer* pl);
static void playerEscape04(cPlayer* pl);
static void playerDie(cPlayer* pl);

// Room init (the laser corridor): areas 0xA/0xB/0xF off; until the corridor was cleared (Room_flg bit
// 0) area 8 = the laser start event and area 9 = the far switch; the three automatic doors' tasks;
// area 1 = the elevator to r31a (arriving from r31a plays its arrival); area 0xE = the bench rest once
// (bit 1); the fifteen laser emitter objects (SetObjSmd with the per-frame callback, hidden).
void R318Init()
{
    void* zero = 0;
    int i;

#line 86 "D:/Bio4/Prog/r318.cpp"
    r318_work = (R318Work*) MEM_CALLOC(sizeof(R318Work), 1, 0xd);
    SceAtSetEnable(0xA, 0);
    SceAtSetEnable(0xB, 0);
    SceAtSetEnable(0xF, 0);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) R318EventLaserStMain, 0, 1);
        SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) R318ExecSwitchCheck, 0, 1);
    }
    SceExec(0x12, (TaskFunc) R318AutoDoorMgr, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) R318AutoDoorMgr, 1, 0, 2, 0);
    SceExec(0x12, (TaskFunc) R318AutoDoorMgr, 2, 0, 2, 0);
    SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) SceElevator, &r318_elvLeave, 1);
    SceAtSetActColor(1, 1);
    if (pG->room_id_prev == 0x31A) {
        SceExec(0x12, (TaskFunc) SceElevator, (int) &r318_elvArrive, 0, 2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(0xE, 0x12, 0, (TaskFunc) R318ExecSitMain, 0, 1);
    }
    EstSet(0, -1, 0, 0, EFF_ROOM, 8, 0x2001, ESP_CORE_KIND_ROOM03, zero, zero);
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    for (i = 0; i < 15; i++) {
        r318_work->laser[i] = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &pos, &rot, 0x10, 1);
        if (r318_work->laser[i]) {
            cObj* laser = r318_work->laser[i];

            cObjScrSetCallBack(laser, R318LaserCallBackFunc);
            laser->LightInfo.EnableMask = 4;
            laser->be_flag = (laser->be_flag | 0x1000) & ~2;
        }
    }
    r318_work->str = 0;
    r318_work->laserSnd = 0;
}

// Per-frame room main: nothing.
void R318Main()
{
}

// The bench: Leon sits down and rests (the stream plays a message).
static void R318ExecSitMain()
{
    cPlayer* pl = pPL;

    SceEventStart(0);
    r318_work->str = SndStrReq(1, 0xEB, 0x80000003, 0, 0, 0.0f);
    r318_work->plPos = pPL->pos;
    r318_work->plRot = pPL->ang;
    SceSetEventCancel(1, (TaskFunc) R318ExecSitEnd, 0, -1, 1);
    pPL->pos.x = 0.0f;
    pPL->pos.y = 0.0f;
    pPL->pos.z = 0.0f;
    pPL->ang.x = 0.0f;
    pPL->ang.y = 0.0f;
    pPL->ang.z = 0.0f;
    pPL->atari.off();
    pPL->beginEvent(0);
    pPL->setNoSuspend(1);
    pl->Wep->setTrans(0, 1);
    MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x87), 0, 0, 0x201, 0);
    MotionMove(pPL, 0);
    while (MotionGetState(pPL) == 0) {
        SceSleep(1);
    }
    SceMesCamSndSet(3, -1, -1, 4);
    {
        u32 str = SndStrPlayBlock(1, 0xEC, 0.0f);

        r318_work->str = str;
    }
    MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x88), 0, 0, 0x201, 0);
    MotionMove(pPL, 0);
    while (MotionGetState(pPL) == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R318ExecSitEnd();
}

// End of the bench rest (also its cancel path): Leon placed beside the bench facing -1.57 rad, out of
// event mode with collision and weapon back, the stream stopped, SceEventEnd.
static void R318ExecSitEnd()
{
    cPlayer* pl = pPL;

    pPL->pos.x = 19760.0f;
    pPL->pos.y = 448.0f;
    pPL->pos.z = 4375.0f;
    pPL->ang.x = 0.0f;
    pPL->ang.y = -1.57f;
    pPL->ang.z = 0.0f;
    pPL->setNoSuspend(0);
    pPL->endEvent(0);
    pPL->atari.on();
    pl->Wep->setTrans(1, 0);
    SndStrReq(r318_work->str, 8, 0, 0);
    SceEventEnd(0);
}

// Draws the beam between the emitter's parts 2 and 4 while the lasers are on. Both `Vec` copies load
// their first word through the copy's own address register (`mr r10,r28; lwzu r8,0x70(r10)` /
// `mr r9,r29; lwzu r11,0x70(r9)`): combine's movsi_update of `P = p + 0x70` with `(mem P)`, which
// needs cse not to rewrite `(mem P)` into the "costlier equivalent" `(mem (plus p 0x70))`
// (find_best_addr) and p2/p4 kept live past the loads (no local-alloc tie of P to p, hence the `mr`).
// - a: `pa` is the hard register r10 set to a copy of p2 and then re-set to `pa + 0x70` (cse never
//   canonicalises a hard register and the `(plus pa 0x70)` entry mentions the re-set register), the
//   copy `pa = q` staying a real insn because q is laundered between the copy and the increment
//   (combine's use_crosses_set_p), so the update merges with op0 = op1 = r10.
// - b: `pb = &p4->worldPos` blinded by the LOOP_END-blinded dead test (cse1 follows the branch around
//   `pb = 0` and invalidates pb, cse2 folds the test and starts a fresh ebb), the `mr r9,r29` being
//   reload's "0"-constraint copy.
// Scheduling: `q = p2` is the no-op move r28 = r28 after allocation (p2 prefers r28), deleted before
// sched2; at sched1 it is the loop-notes barrier that keeps D (`addi r11,r1,8`) out of the lwzu
// cycle so `lis/lfs` take it (H short-lived -> r9), and at sched2 the barrier moves to `mr r10,r28`
// (p4's copy alone before it, D after lfs). The trailing anchor keeps q/p4 live.
void R318LaserCallBackFunc(cObj* obj)
{
    if ((pG->Room_flg[0] & 0x00020000) && obj->isTrans() == 1) {
        register cParts* q asm("r28");
        register Vec* pa asm("r10");
        cParts* p2 = GetPartsAddr(obj->pList, 2);
        cParts* p4 = GetPartsAddr(obj->pList, 4);
        int k = 2;
        do {
        } while (0);
        q = p2;
        pa = (Vec*) q;
        asm("" : "+r"(q));
        pa = (Vec*) ((u8*) pa + 0x70);
        Vec* pb = &p4->world;
        if (k != 2) {
            pb = 0;
        }
        Vec a = *pa;
        Vec b = *pb;

        DrawLaserLine(&a, &b, 0xFF, 0xFF, 0xFF, 0xFF, 192000.0f);
        asm("" : : "r"(q), "r"(p4));
    }
}

// Draw one laser beam from a to b: three effect sprites (set 1 kind 0xA) stretched along the segment
// with the given RGBA scaling.
void DrawLaserLine(Vec* a, Vec* b, int r, int g, int b_, int alpha, f32 len)
{
    Vec d;
    cEsp* esp;
    u32 i;

    for (i = 0; i < 3; i++) {
        if (EspEstSetSelect(EFF_ROOM, 0xA, i, &esp, 1)) {
            ((R318EspView*) esp)->pos2 = *a;
            esp->m_Pos = *a;
            PSVECSubtract(b, a, &d);
            esp->m_Speed = d;
            esp->m_Col_r *= (f32) r;
            esp->m_Col_g *= (f32) g;
            esp->m_Col_b *= (f32) b_;
            esp->m_Col_a *= (f32) alpha;
            esp->m_Col_r *= 0.003921569f;
            esp->m_Col_g *= 0.003921569f;
            esp->m_Col_b *= 0.003921569f;
            esp->m_Col_a *= 0.003921569f;
        }
    }
}

// Task per automatic door `no`: door 0 (flag 0x40, halves 0xC/0xD), 1 (0x41, 0xE/0xF), 2 (0x42, 0x11/0x10).
static void R318AutoDoorMgr(int no)
{
    if (no == 0) {
        R318AutoDoor(0, 0x40, 0xC, 0xD);
    }
    if (no == 1) {
        R318AutoDoor(1, 0x41, 0xE, 0xF);
    }
    if (no == 2) {
        R318AutoDoor(2, 0x42, 0x11, 0x10);
    }
}

// Automatic door `no`: the two half objects (script-moved), their closed z, and a collision / attribute
// piece per half (3000 tall, polygons along z).
void R318AutoDoorInit(int no, u32 id1, u32 id0)
{
    R318Door* d = &r318_work->door[no];
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};

    d->obj0 = SmdGetObjPtr(id0);
    d->obj1 = SmdGetObjPtr(id1);
    if (d->obj0 && d->obj1) {
        d->obj0->be_flag |= 0x20;
        d->obj1->be_flag |= 0x20;
        d->z0 = d->obj0->pos.z;
        d->z1 = d->obj1->pos.z;
        Vec poly0[4] = {{-100.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 1500.0f}, {-100.0f, 0.0f, 1500.0f}};
        Vec poly1[4] = {{-100.0f, 0.0f, -1500.0f}, {100.0f, 0.0f, -1500.0f}, {100.0f, 0.0f, 0.0f}, {-100.0f, 0.0f, 0.0f}};
        f32 r = 3000.0f;

        pos.x = d->obj0->pos.x;
        pos.y = d->obj0->pos.y;
        pos.z = d->obj0->pos.z;
        d->sat0 = SatMgr.create(&pos, &rot, poly0, r, 0x40, 0);
        d->sat1 = SatMgr.create(&pos, &rot, poly1, r, 0x40, 0);
        d->eat0 = EatMgr.create(&pos, &rot, poly0, r, 0x40, 0);
        d->eat1 = EatMgr.create(&pos, &rot, poly1, r, 0x40, 0);
    }
}

// Snap automatic door `no` closed: halves back at their closed z with their pieces following, flags cleared.
void R318AutoDoorReset(int no)
{
    R318Door* d = &r318_work->door[no];

    if (d->obj0 && d->obj1) {
        d->open = 0;
        d->done = 0;
        d->obj0->setPos(d->obj0->pos.x, d->obj0->pos.y, d->z0);
        d->obj1->setPos(d->obj1->pos.x, d->obj1->pos.y, d->z1);
        if (d->sat0) {
            d->sat0->setCoord(&d->obj0->pos, &d->obj0->ang);
        }
        if (d->sat1) {
            d->sat1->setCoord(&d->obj1->pos, &d->obj1->ang);
        }
        if (d->eat0) {
            d->eat0->setCoord(&d->obj0->pos, &d->obj0->ang);
        }
        if (d->eat1) {
            d->eat1->setCoord(&d->obj1->pos, &d->obj1->ang);
        }
    }
}

// Door task: opens while event flag `flagNo` is set, closes when it is clear. The loop never exits,
// so gcse's PRE (the block-based lcm: anticipation is killed only by the last block) finds
// `high(pG)` and `high(1700.0f)` anticipated at the preheader and hoists both (`lis` before the loop);
// the target keeps `lis r9` at each use. The LOOP_END-blinded dead `return` right before the loop
// gives the preheader an exit edge at gcse time (antin 0 -> no insertion), cse2 folds it (no code),
// and loop.c still hoists the `1` (`li r31,1`). `k` is set at the block top so the notes' scheduling
// barrier is the block's first insn (`lis LC`), leaving the ofs/mask/lfs order to sched2 (`ofs`
// first so flagNo dies at the `and`, tie with the `lis 0x8000` -> LUID order).
void R318AutoDoor(int no, int flagNo, u32 id0, u32 id1)
{
    R318Door* d = &r318_work->door[no];

    R318AutoDoorInit(no, id0, id1);
    R318AutoDoorReset(no);
    if (d->obj0 && d->obj1) {
        int k = 2;

        do {
        } while (0);
        f32 spd = 100.0f;
        u32 ofs = ((u32) flagNo >> 5) << 2;  // one rlwinm before the loop
        u32 mask = 0x80000000 >> (flagNo & 0x1F);

        if (k != 2) {
            return;
        }
        for (;;) {
            u32* flags;

            SceSleep(1);
            flags = &pG->Room_flg[0];
            if (*flags & 0x40000000) {
                continue;
            }
            if (*(u32*) (ofs + (u32) flags) & mask) {
                if (d->open == 0) {
                    d->open = 1;
                    d->done = 0;
                    SndCall(6, 4, &d->obj0->pos, 0, 0, 0);
                }
                if (d->done == 1) {
                    continue;
                }
                d->obj0->pos.z += spd;
                d->obj1->pos.z -= spd;
                if (d->obj0->pos.z >= d->z0 + 1700.0f) {
                    d->obj0->pos.z = d->z0 + 1700.0f;
                    d->obj1->pos.z = d->z1 - 1700.0f;
                    d->done = 1;
                }
            } else {
                if (d->open == 1) {
                    d->open = 0;
                    d->done = 1;
                    SndCall(6, 4, &d->obj0->pos, 0, 0, 0);
                }
                if (d->done == 0) {
                    continue;
                }
                d->obj0->pos.z -= spd;
                d->obj1->pos.z += spd;
                if (d->obj0->pos.z <= d->z0) {
                    d->obj0->pos.z = d->z0;
                    d->obj1->pos.z = d->z1;
                    d->done = 0;
                }
            }
            d->obj0->setPos(d->obj0->pos.x, d->obj0->pos.y, d->obj0->pos.z);
            d->obj1->setPos(d->obj1->pos.x, d->obj1->pos.y, d->obj1->pos.z);
            if (d->sat0) {
                d->sat0->setCoord(&d->obj0->pos, &d->obj0->ang);
            }
            if (d->sat1) {
                d->sat1->setCoord(&d->obj1->pos, &d->obj1->ang);
            }
            if (d->eat0) {
                d->eat0->setCoord(&d->obj0->pos, &d->obj0->ang);
            }
            if (d->eat1) {
                d->eat1->setCoord(&d->obj1->pos, &d->obj1->ang);
            }
        }
    }
}

// The switch at the far end: the lasers shut down.
static void R318ExecSwitchClear()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        int i;

        RsfSet(G_ROOM_ID, 0);
        SceEventStart(0);
        CamCtrl.CutCall(4);
        for (i = 0; i < 30; i++) {
            SceSleep(1);
        }
        EffectDelete(0x2001, ESP_CORE_KIND_ROOM03);
        EstSet(0, -1, 0, 0, EFF_ROOM, 8, 0x2001, ESP_CORE_KIND_ROOM03, 0, 0);
        SndCall(6, 7, &pPL->pos, 0, 0, 0);
        SndCall(6, 8, 0, 0, 0, 0);
        SceMesSet(1, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        pG->Room_flg[0] &= ~0x40000000;
        SceAtSetEnable(0xF, 0);
        SceAtSetEnable(9, 0);
        SceAtSetEnable(0xA, 0);
        SceAtSetEnable(0xB, 0);
        SceEventEnd(0);
    }
}

// The switch panel: the first press starts the last laser pattern with Leon at the panel, the
// second press (flag 0x01000000 already set) resets the emitters.
static void R318ExecSwitchCheck()
{
    cObj* laser;

    SndCall(6, 6, &pPL->pos, 0, 0, 0);
    if (pG->Room_flg[0] & 0x01000000) {
        int i;

        if (pG->Room_flg[0] & 0x40) {
            return;
        }
        pG->Room_flg[0] |= 0x40;
        void* tbl[15] = {ROOM_ARC_PTR(pG->pRoom, 0x42), ROOM_ARC_PTR(pG->pRoom, 0x43), ROOM_ARC_PTR(pG->pRoom, 0x44),
                         ROOM_ARC_PTR(pG->pRoom, 0x45), ROOM_ARC_PTR(pG->pRoom, 0x46), ROOM_ARC_PTR(pG->pRoom, 0x47),
                         ROOM_ARC_PTR(pG->pRoom, 0x48), ROOM_ARC_PTR(pG->pRoom, 0x49), ROOM_ARC_PTR(pG->pRoom, 0x4A),
                         ROOM_ARC_PTR(pG->pRoom, 0x4B), ROOM_ARC_PTR(pG->pRoom, 0x4C), ROOM_ARC_PTR(pG->pRoom, 0x4D),
                         ROOM_ARC_PTR(pG->pRoom, 0x4E), ROOM_ARC_PTR(pG->pRoom, 0x4F), ROOM_ARC_PTR(pG->pRoom, 0x50)};

        for (i = 0; i < 15; i++) {
            laser = r318_work->laser[i];
            if (laser) {
                MotionSetCore(laser, &laser->Motion, tbl[i], 0, 0, 0, (u16) (FcvGetMaxFrame((u16*) tbl[i]) - 20));
            }
        }
    } else {
        int i;

        pG->Room_flg[0] |= 0x01000000;
        SceUpCut(2, 4, -1, 4);
        CamCtrl.Comeback(0);
        void* mot = ROOM_ARC_PTR(pG->pRoom, 0x61);
        void* tbl[15] = {ROOM_ARC_PTR(pG->pRoom, 0x52), ROOM_ARC_PTR(pG->pRoom, 0x53), ROOM_ARC_PTR(pG->pRoom, 0x54),
                         ROOM_ARC_PTR(pG->pRoom, 0x55), ROOM_ARC_PTR(pG->pRoom, 0x56), ROOM_ARC_PTR(pG->pRoom, 0x57),
                         ROOM_ARC_PTR(pG->pRoom, 0x58), ROOM_ARC_PTR(pG->pRoom, 0x59), ROOM_ARC_PTR(pG->pRoom, 0x5A),
                         ROOM_ARC_PTR(pG->pRoom, 0x5B), ROOM_ARC_PTR(pG->pRoom, 0x5C), ROOM_ARC_PTR(pG->pRoom, 0x5D),
                         ROOM_ARC_PTR(pG->pRoom, 0x5E), ROOM_ARC_PTR(pG->pRoom, 0x5F), ROOM_ARC_PTR(pG->pRoom, 0x60)};

        r318_work->plPos = pPL->pos;
        __builtin_memcpy((u8*) r318_work + 0xD8, &pPL->ang, sizeof(Vec));
        pPL->pos.x = 0.0f;
        pPL->pos.y = 0.0f;
        pPL->pos.z = 0.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = 0.0f;
        pPL->ang.z = 0.0f;
        pPL->atari.off();
        pPL->beginEvent(0);
        pPL->setNoSuspend(1);
        MotionSetCore(pPL, &pPL->Motion, mot, 0, 0, 0x201, 0);
        EstSet(pPL, -1, 0, 0, EFF_ROOM, 0xC, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        MotionMove(pPL, 0);
        for (i = 0; i < 15; i++) {
            laser = r318_work->laser[i];
            if (laser) {
                MotionSetCore(laser, &laser->Motion, tbl[i], 0, 0, 0, 0);
                laser->be_flag |= 2;
            }
        }
        R318LaserEspInit(15, 0, 2);
        SndCall(6, 3, 0, 0, 0, 0);
        r318_work->xE4 = 0;
        while (MotionGetState(pPL) == 0) {
            pPL->dmg.m_Timer = 5;
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R318ExecSwitchCheckEnd();
    }
}

// End of the far switch event (also its cancel path): Leon solid and out of event mode at the saved
// position / rotation, the switch effect dropped, facing -PI/2.
void R318ExecSwitchCheckEnd()
{
    pPL->atari.on();
    pPL->setNoSuspend(0);
    pPL->endEvent(0);
    pPL->pos.x = r318_work->plPos.x;
    pPL->pos.y = r318_work->plPos.y;
    pPL->pos.z = r318_work->plPos.z;
    pPL->ang.x = r318_work->plRot.x;
    pPL->ang.y = r318_work->plRot.y;
    pPL->ang.z = r318_work->plRot.z;
    EffectDelete(0x2001, ESP_CORE_KIND_ROOM04);
    {
        Vec v;

        v.x = 0.0f;
        v.z = 0.0f;
        v.y = -1.5707964f;
        pPL->setAng(&v);
    }
    SceExec(0x12, (TaskFunc) R318EventLaserMove, 4, 0, 2, 0);
    SceExit();
}

// Entering the corridor: the door closes behind Leon and the first three emitters start.
static void R318EventLaserStMain()
{
    if ((pG->Room_flg[0] & 0x80000000) == 0) {
        R318Door* d;

        pG->Room_flg[0] |= 0x80000000;
        pG->Room_flg[0] |= 0x40000000;
        SceAtSetEnable(8, 0);
        pG->Room_flg[0] &= ~0x20;
        SceEventStart(0);
        SceSetEventCancel(1, (TaskFunc) R226EventLaserStEnd, 0, -1, 1);
        pPL->beginEvent(0);
        pPL->setNoSuspend(1);
        CamCtrl.CutCall(3);
        d = &r318_work->door[0];
        if (d->obj0 && d->obj1) {
            int i;

            for (;;) {
                d->obj0->pos.z -= 100.0f;
                d->obj1->pos.z += 100.0f;
                if (d->obj0->pos.z <= d->z0) {
                    d->obj0->pos.z = d->z0;
                    d->obj1->pos.z = d->z1;
                    break;
                }
                SceSleep(1);
            }
            R318AutoDoorReset(0);
            EffectDelete(0x2001, ESP_CORE_KIND_ROOM03);
            EstSet(0, -1, 0, 0, EFF_ROOM, 9, 0x2001, ESP_CORE_KIND_ROOM03, 0, 0);
            {
                void* tbl[3] = {ROOM_ARC_PTR(pG->pRoom, 0x84), ROOM_ARC_PTR(pG->pRoom, 0x85), ROOM_ARC_PTR(pG->pRoom, 0x86)};

                for (i = 0; i < 3; i++) {
                    cObj* laser = r318_work->laser[i];

                    if (laser) {
                        MotionSetCore(laser, &laser->Motion, tbl[i], 0, 0, 0, 0);
                        laser->be_flag |= 2;
                    }
                }
            }
            pG->Room_flg[0] |= 0x20;
            R318LaserEspInit(3, 0, 2);
            SndCall(6, 3, 0, 0, 0, 0);
            {
                cObj* laser = r318_work->laser[0];

                if (laser) {
                    while (MotionGetState(laser) == 0) {
                        SceSleep(1);
                    }
                }
            }
            SceSetEventCancel(0, 0, 0, -1, 1);
            R226EventLaserStEnd();
        }
    }
}

// End of the corridor entry event: the laser manager task starts, door 0 reset shut, the entry effect
// dropped, the first three emitters lit once (Room_flg[0] 0x20, SE 3), the corridor effect, areas 0xA/0xB on.
static void R226EventLaserStEnd()
{
    void* zero = 0;

    SceExec(0x12, (TaskFunc) R318EventLaserMgr, 0, 0, 2, 0);
    R318AutoDoorReset(0);
    EffectDelete(0x2001, ESP_CORE_KIND_ROOM00);
    if ((pG->Room_flg[0] & 0x20) == 0) {
        void* tbl[3] = {ROOM_ARC_PTR(pG->pRoom, 0x84), ROOM_ARC_PTR(pG->pRoom, 0x85), ROOM_ARC_PTR(pG->pRoom, 0x86)};

        pG->Room_flg[0] |= 0x20;
        R318LaserEspInit(3, 0, 2);
        SndCall(6, 3, 0, 0, 0, 0);
    }
    EffectDelete(0x2001, ESP_CORE_KIND_ROOM03);
    EstSet(0, -1, 0, 0, EFF_ROOM, 9, 0x2001, ESP_CORE_KIND_ROOM03, zero, zero);
    SceAtSetEnable(0xA, 1);
    SceAtSetEnable(0xB, 1);
    pPL->setNoSuspend(0);
    pPL->endEvent(0);
    SceEventEnd(0);
    SceExit();
}

// Starts the laser patterns as the player passes the trigger areas (flags_17C bits).
static void R318EventLaserMgr()
{
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 0)) {
            return;
        }
        if ((pG->Room_flg[2] & 0x10000000) && (pG->Room_flg[0] & 0x20000000) == 0) {
            pG->Room_flg[0] |= 0x20000000;
            SceExec(0x12, (TaskFunc) R318EventLaserMove, 0, 0, 2, 0);
        }
        if ((pG->Room_flg[2] & 0x08000000) && (pG->Room_flg[0] & 0x10000000) == 0) {
            pG->Room_flg[0] |= 0x10000000;
            R318EventLaserEnd(0);
            SceExec(0x12, (TaskFunc) R318EventLaserMove, 1, 0, 2, 0);
        }
        if ((pG->Room_flg[2] & 0x04000000) && (pG->Room_flg[0] & 0x08000000) == 0) {
            pG->Room_flg[0] |= 0x08000000;
            R318EventLaserEnd(1);
            SceExec(0x12, (TaskFunc) R318EventLaserMove, 2, 0, 2, 0);
        }
        if ((pG->Room_flg[2] & 0x02000000) && FlagChkSign(pG->Room_flg, 21) && FlagChkSign(pG->Room_flg, 5) == 0) {
            pG->Room_flg[0] |= 0x04000000;
            SceExec(0x12, (TaskFunc) R318EventLaserMove, 3, 0, 2, 0);
        }
        SceSleep(1);
    }
}

// Beam effects on the first `n` emitters.
void R318LaserEspInit(int n, int type, int kind)
{
    int i;

    for (i = 0; i < n; i++) {
        cObj* laser = r318_work->laser[i];

        if (laser) {
            void* zero;

            if (type == 0) {
                pG->Room_flg[0] |= 0x00020000;
            }
            if (type == 2) {
                r318_work->laserSnd = SndCall(6, 0xC, &laser->pos, 0, 0, 0);
            }
            zero = 0;
            EstSet(laser, -1, 0, 0, EFF_ROOM, (u8) type, 1, (u8) kind, zero, zero);
        }
    }
}

// Leon touched a beam: Room_flg[0] 0x00040000, rumble + quake, the death routine (playerDie).
static inline void LaserHit()
{
    BitOn(pG->Room_flg[0], 0x00040000);
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
    QuakeExec(0, 0, 5, 22.0f, 2);
    SetPlDamage(0, playerDie);
}

// The dodge button: X pressed with A held, or A pressed with X held (a macro: the condition branches
// directly, an inline function would materialise the boolean).
#define DodgePressed() ((Key.trg & 0x00400000 && Key.on & 0x00800000) || (Key.on & 0x00400000 && Key.trg & 0x00800000))

// One laser pattern: `no` selects the motions, the count of emitters and the dodge distances.
static void R318EventLaserMove(int no)
{
    Vec hit;
    Vec nrm;
    void* motA[5] = {ROOM_ARC_PTR(pG->pRoom, 0x7F), ROOM_ARC_PTR(pG->pRoom, 0x80), ROOM_ARC_PTR(pG->pRoom, 0x81),
                     ROOM_ARC_PTR(pG->pRoom, 0x82), ROOM_ARC_PTR(pG->pRoom, 0x83)};
    // Entries 2..4 hold the array's own address in the target (preTbl reuses the freed slot of motA's
    // constructor temporary; they are never read: pre is only set for pattern 1).
    void** preTbl[5] = {motA, motA, (void**) preTbl, (void**) preTbl, (void**) preTbl};
    // The count tables are non-constant initialisers in the original (one variable element): no .rodata
    // template and no memset — the compiler stores the elements one by one from registers, after a
    // `(clobber (mem:BLK))` of the array (which is what keeps the tbl temporary's dead element-0 store
    // alive in flow's dead-store scan, `stw r14,0x100(r1)`).
    int zero = 0;
    int preCnt[5] = {zero, 5, zero, zero, zero};
    void* motB[3] = {ROOM_ARC_PTR(pG->pRoom, 0x62), ROOM_ARC_PTR(pG->pRoom, 0x63), ROOM_ARC_PTR(pG->pRoom, 0x64)};
    void* motC[5] = {ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25),
                     ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27)};
    void* motD[5] = {ROOM_ARC_PTR(pG->pRoom, 0x28), ROOM_ARC_PTR(pG->pRoom, 0x29), ROOM_ARC_PTR(pG->pRoom, 0x2A),
                     ROOM_ARC_PTR(pG->pRoom, 0x2B), ROOM_ARC_PTR(pG->pRoom, 0x2C)};
    void* motE[8] = {ROOM_ARC_PTR(pG->pRoom, 0x65), ROOM_ARC_PTR(pG->pRoom, 0x66), ROOM_ARC_PTR(pG->pRoom, 0x67),
                     ROOM_ARC_PTR(pG->pRoom, 0x68), ROOM_ARC_PTR(pG->pRoom, 0x69), ROOM_ARC_PTR(pG->pRoom, 0x6A),
                     ROOM_ARC_PTR(pG->pRoom, 0x6B), ROOM_ARC_PTR(pG->pRoom, 0x6C)};
    void* motF[15] = {ROOM_ARC_PTR(pG->pRoom, 0x42), ROOM_ARC_PTR(pG->pRoom, 0x43), ROOM_ARC_PTR(pG->pRoom, 0x44),
                      ROOM_ARC_PTR(pG->pRoom, 0x45), ROOM_ARC_PTR(pG->pRoom, 0x46), ROOM_ARC_PTR(pG->pRoom, 0x47),
                      ROOM_ARC_PTR(pG->pRoom, 0x48), ROOM_ARC_PTR(pG->pRoom, 0x49), ROOM_ARC_PTR(pG->pRoom, 0x4A),
                      ROOM_ARC_PTR(pG->pRoom, 0x4B), ROOM_ARC_PTR(pG->pRoom, 0x4C), ROOM_ARC_PTR(pG->pRoom, 0x4D),
                      ROOM_ARC_PTR(pG->pRoom, 0x4E), ROOM_ARC_PTR(pG->pRoom, 0x4F), ROOM_ARC_PTR(pG->pRoom, 0x50)};
    void** tbl[5] = {motB, motC, motD, motE, motF};
    int cntB = 3;
    int cnt[5] = {cntB, 5, 5, 8, 15};
    f32 dist[5] = {2000.0f, 2000.0f, 4000.0f, 1700.0f, 8000.0f};
    f32 dist2[5] = {100.0f, 100.0f, 100.0f, 2500.0f, 1000.0f};
    int pre;
    int esp;
    int mode;
    int hokan;
    int i;
    cObj* laser;  // one variable for every loop (r29 throughout; a block-local copy takes r3 in mode 1)
    // The four `[no]` reads happen once, before the flag stores and the switch (lwzx into r24/r25/r27/r21).
    void** preMot = preTbl[no];
    int preNum = preCnt[no];
    void** mot = tbl[no];
    int num = cnt[no];

    pG->Room_flg[0] &= ~0x00080000;
    pG->Room_flg[0] &= ~0x00040000;
    pG->Room_flg[0] &= ~0x00010000;
    r318_work->escCount = 0;
    r318_work->xE4 = 0;
    r318_work->dodgeTimer = 0;
    switch (no) {
    case 0:
        pre = 0;
        esp = 0;
        mode = 2;
        hokan = 4;
        break;
    case 1:
        pre = 1;
        esp = 1;
        mode = 2;
        hokan = 4;
        break;
    case 3:
        pre = 0;
        esp = 1;
        mode = 1;
        hokan = 0;
        break;
    case 4:
        r318_work->xE4 = 0x100;
        pre = 0;
        esp = 0;
        mode = 0;
        hokan = 0;
        break;
    case 2:
    default:
        pre = 0;
        esp = 1;
        mode = 0;
        hokan = 0;
        break;
    }
    if (esp == 1) {
        int type = 0;
        int kind = 2;

        if (no == 3) {
            type = 2;
            kind = 4;
        } else {
            SndCall(6, 3, 0, 0, 0, 0);
        }
        R318LaserEspInit(num, type, kind);
    }
    if (pre == 1) {
        for (i = 0; i < preNum; i++) {
            laser = r318_work->laser[i];
            if (laser) {
                MotionSetCore(laser, &laser->Motion, preMot[i], 0, 0, (u16) hokan, 0);
                laser->be_flag |= 2;
            }
        }
        laser = r318_work->laser[0];
        while (MotionGetState(laser) == 0) {
            SceSleep(1);
        }
    }
    for (int j = 0; j < num; j++) {  // its own counter: the pre loop's `i` count is r28, this one's r30
        laser = r318_work->laser[j];
        if (laser) {
            MotionSetCore(laser, &laser->Motion, mot[j], 0, 0, (u16) hokan, 0);
            laser->be_flag |= 2;
        }
    }
    SceSleep(1);
    while ((pG->Room_flg[0] & 0x00080000) == 0) {
        if ((pG->Room_flg[0] & 0x00040000) == 0) {
            if (mode == 2) {
                for (i = 0; i < num; i++) {
                    laser = r318_work->laser[i];
                    if (laser) {
                        cParts* p2 = laser->getPartsPtr(2);
                        cParts* p4 = laser->getPartsPtr(4);

                        if (p2 && p4 && EmAtkLineHitCk(&p2->world, &p4->world, &hit, &nrm, 0)) {
                            LaserHit();
                        }
                    }
                }
            }
            if (mode == 1) {
                laser = r318_work->laser[0];
                if (laser) {
                    cParts* p2 = laser->getPartsPtr(2);

                    if (p2) {
                        if (__builtin_fabsf(p2->world.x - pPL->pos.x) <= dist[no]) {
                            // The zeros are variables set at the block top: `li r10,0` / `li r30,0` before
                            // the flag test, not rematerialised at the use (update_equiv_regs does not move a
                            // REG_EQUIV init to its use inside a loop).
                            int zero = 0;

                            if ((pG->Room_flg[0] & 0x00010000) == 0) {
                                r318_work->dodgeTimer = 60;
                                pG->Room_flg[0] |= 0x00010000;
                            }
                            ActBtn.set(ACT_GUARD, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_SCE, zero);
                            if (DodgePressed()) {
                                pG->Room_flg[0] |= 0x00040000;
                                SetPlDamage(0, playerEscape03);
                            }
                        }
                        u32 zero2 = 0;
                        if (pG->Room_flg[0] & 0x00010000) {
                            r318_work->dodgeTimer--;
                            if (r318_work->dodgeTimer <= 0 || __builtin_fabsf(p2->world.x - pPL->pos.x) >= dist[no]) {
                                if (r318_work->laserSnd) {
                                    SndStop(r318_work->laserSnd, 0);
                                    r318_work->laserSnd = zero2;
                                }
                                EffectDelete(1, ESP_CORE_KIND_ROOM02);
                                R318LaserEspInit(num, 0, 2);
                                SndCall(6, 3, 0, 0, 0, 0);
                                LaserHit();
                            }
                        }
                    }
                }
            }
            if (mode == 0) {
                laser = r318_work->laser[0];
                if (laser) {
                    cParts* p2 = laser->getPartsPtr(2);

                    if (p2) {
                        if (__builtin_fabsf(p2->world.x - pPL->pos.x) <= dist[no]) {
                            ActBtn.set(ACT_GUARD, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_SCE, mode);
                            if (DodgePressed()) {
                                pG->Room_flg[0] |= 0x00040000;
                                switch (no) {
                                case 2:
                                    SetPlDamage(0, playerEscape02);
                                    break;
                                case 4:
                                    SetPlDamage(0, playerEscape04);
                                    break;
                                }
                            }
                        }
                        if (__builtin_fabsf(p2->world.x - pPL->pos.x) <= dist2[no]) {
                            LaserHit();
                        }
                    }
                    if (no != 4) {
                        if (MotionGetState(laser)) {
                            pG->Room_flg[0] |= 0x00080000;
                            R318EventLaserEnd(no);
                        }
                    }
                }
            }
        }
        SceSleep(1);
    }
}

// Pattern end: the beams go out (sparks at the emitter parts), the next trigger flag is set. The
// target keeps a second register for part 2 (`mr r28,r30` between the two GetPartsAddr calls) used
// for `->mat` and `&->worldPos`, the original register only by the `worldPos.x` load: a hard-register
// copy (cse never canonicalises it back). laser, t and p4 stay live to the end of the block (anchor):
// laser then conflicts with p4 (r29, not the freed r31) and the two `&->worldPos` args do not kill
// their register, so sched1 leaves them in argument order (`li r3; li r4; addi r5`).
void R318EventLaserEnd(int no)
{
    int i;

    for (i = 0; i < 15; i++) {
        cObj* laser = r318_work->laser[i];

        if (laser) {
            void* zero = 0;

            laser->be_flag &= ~2;
            if (no != 4) {
                register cParts* t asm("r28");
                cParts* p2 = GetPartsAddr(laser->pList, 2);
                t = p2;
                cParts* p4 = GetPartsAddr(laser->pList, 4);
                Vec rot[2];  // one array: rot[1] at fp+0x1c (two Vec locals would be 8-aligned)

                Matrix2AxisAngle(t->mat, &rot[0]);
                Matrix2AxisAngle(p4->mat, &rot[1]);
                if (p2->world.x != 0.0f) {
                    EstSet(0, -1, &t->world, &rot[0], EFF_ROOM, 6, 0x801, ESP_CORE_KIND_NONE, zero, zero);
                    EstSet(0, -1, &p4->world, &rot[1], EFF_ROOM, 7, 0x801, ESP_CORE_KIND_NONE, zero, zero);
                }
                asm("" : : "r"(laser), "r"(t), "r"(p4));
            }
        }
    }
    switch (no) {
    case 0:
        pG->Room_flg[0] |= 0x1000;
        break;
    case 1:
        pG->Room_flg[0] |= 0x800;
        break;
    case 2:
        pG->Room_flg[0] |= 0x400;
        break;
    case 3:
        pG->Room_flg[0] |= 0x200;
        break;
    case 4:
        pG->Room_flg[0] |= 0x100;
        break;
    }
    EffectDelete(1, ESP_CORE_KIND_ROOM00);
    pG->Room_flg[0] &= ~0x00020000;
    pG->Room_flg[0] |= 0x00080000;
    if (no == 4) {
        SceExec(0x12, (TaskFunc) R318ExecSwitchClear, 0, 0, 2, 0);
    }
}

// Dodge of pattern 2: Leon rolls under the beams.
static void playerEscape02(cPlayer* pl)
{
    void* mot = ROOM_ARC_PTR(pG->pRoom, 0x32);
    void* tbl[5] = {ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2E), ROOM_ARC_PTR(pG->pRoom, 0x2F),
                    ROOM_ARC_PTR(pG->pRoom, 0x30), ROOM_ARC_PTR(pG->pRoom, 0x31)};
    int i;

    switch (pl->r_no_2) {
    case 0:
        r318_work->plPos = pPL->pos;
        __builtin_memcpy((u8*) r318_work + 0xD8, &pPL->ang, sizeof(Vec));
        pPL->pos.x = 0.0f;
        pPL->pos.y = 0.0f;
        pPL->pos.z = 0.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = 0.0f;
        pPL->ang.z = 0.0f;
        pPL->atari.off();
        MotionSetCore(pl, &pl->Motion, mot, 0, 0, 0x201, 0);
        EstSet(pPL, -1, 0, 0, EFF_ROOM, 0xB, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        r318_work->str = SndStrPlayBlock(1, 0xC3, 0.0f);
        for (i = 0; i < 5; i++) {
            cObj* laser = r318_work->laser[i];

            if (laser) {
                MotionSetCore(laser, &laser->Motion, tbl[i], 0, 0, 0, 0);
            }
        }
        pl->r_no_2++;
    case 1:
        pl->dmg.m_Timer = 0x78;
        if (MotionMove(pl, 0)) {
            pG->Room_flg[0] |= 0x00080000;
            R318EventLaserEnd(2);
            pPL->atari.on();
            EffectDelete(0x2001, ESP_CORE_KIND_ROOM04);
            EndPlDamage();
        }
        break;
    }
}

// Dodge of pattern 3: Leon ducks, the beams sweep twice, then he stands up.
static void playerEscape03(cPlayer* pl)
{
    void* mot0 = ROOM_ARC_PTR(pG->pRoom, 0x75);
    void* tbl0[8] = {ROOM_ARC_PTR(pG->pRoom, 0x6D), ROOM_ARC_PTR(pG->pRoom, 0x6E), ROOM_ARC_PTR(pG->pRoom, 0x6F),
                     ROOM_ARC_PTR(pG->pRoom, 0x70), ROOM_ARC_PTR(pG->pRoom, 0x71), ROOM_ARC_PTR(pG->pRoom, 0x72),
                     ROOM_ARC_PTR(pG->pRoom, 0x73), ROOM_ARC_PTR(pG->pRoom, 0x74)};
    void* mot1 = ROOM_ARC_PTR(pG->pRoom, 0x7E);
    void* tbl1[8] = {ROOM_ARC_PTR(pG->pRoom, 0x76), ROOM_ARC_PTR(pG->pRoom, 0x77), ROOM_ARC_PTR(pG->pRoom, 0x78),
                     ROOM_ARC_PTR(pG->pRoom, 0x79), ROOM_ARC_PTR(pG->pRoom, 0x7A), ROOM_ARC_PTR(pG->pRoom, 0x7B),
                     ROOM_ARC_PTR(pG->pRoom, 0x7C), ROOM_ARC_PTR(pG->pRoom, 0x7D)};

    switch (pl->r_no_2) {
    case 0: {
        int i;

        r318_work->plPos = pPL->pos;
        __builtin_memcpy((u8*) r318_work + 0xD8, &pPL->ang, sizeof(Vec));
        pPL->pos.x = 0.0f;
        pPL->pos.y = 0.0f;
        pPL->pos.z = 0.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = 0.0f;
        pPL->ang.z = 0.0f;
        pPL->atari.off();
        MotionSetCore(pl, &pl->Motion, mot0, 0, 0, 0x201, 0);
        EstSet(pPL, -1, 0, 0, EFF_ROOM, 0xD, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        r318_work->str = SndStrPlayBlock(1, 0xC4, 0.0f);
        for (i = 0; i < 8; i++) {
            cObj* laser = r318_work->laser[i];

            if (laser) {
                MotionSetCore(laser, &laser->Motion, tbl0[i], 0, 0, 0, 0);
            }
        }
        r318_work->escCount = 0;
        r318_work->escFrame = 0;
        pl->r_no_2++;
    }
    case 1:
        if (r318_work->escCount <= 9) {
            r318_work->escCount++;
        }
        if (r318_work->escCount == 10) {
            r318_work->escCount = 11;
            if (r318_work->laserSnd) {
                SndStop(r318_work->laserSnd, 0);
                r318_work->laserSnd = 0;
            }
            EffectDelete(1, ESP_CORE_KIND_ROOM02);
            R318LaserEspInit(8, 0, 2);
            SndCall(6, 3, 0, 0, 0, 0);
        }
        pl->dmg.m_Timer = 0x78;
        r318_work->escFrame++;
        if (r318_work->escFrame > 0x20) {
            ActBtn.set(ACT_GUARD, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_SCE, 0);
            if (DodgePressed()) {
                pl->r_no_2++;
                break;
            }
        }
        if (MotionMove(pl, 0) || r318_work->escFrame > 0x3E) {
            pG->Room_flg[0] |= 0x00040000;
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            SetPlDamage(0, playerDie);
        }
        break;
    case 2: {
        int i;

        pPL->pos.x = 0.0f;
        pPL->pos.y = 0.0f;
        pPL->pos.z = 0.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = 0.0f;
        pPL->ang.z = 0.0f;
        MotionSetCore(pl, &pl->Motion, mot1, 0, 0, 0x201, 0);
        EstSet(pPL, -1, 0, 0, EFF_ROOM, 0xF, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        r318_work->str = SndStrPlayBlock(1, 0xC5, 0.0f);
        for (i = 0; i < 8; i++) {
            cObj* laser = r318_work->laser[i];

            if (laser) {
                MotionSetCore(laser, &laser->Motion, tbl1[i], 0, 0, 0, 0);
            }
        }
        r318_work->escFrame = 0;
        pl->r_no_2++;
    }
    case 3:
        pl->dmg.m_Timer = 0x78;
        if (MotionMove(pl, 0)) {
            pG->Room_flg[0] |= 0x00080000;
            R318EventLaserEnd(3);
            pPL->atari.on();
            EffectDelete(0x2001, ESP_CORE_KIND_ROOM04);
            EndPlDamage();
        }
        break;
    }
}

// Dodge of the last pattern: Leon jumps between the beams.
static void playerEscape04(cPlayer* pl)
{
    void* mot = ROOM_ARC_PTR(pG->pRoom, 0x51);
    void* tbl[15] = {ROOM_ARC_PTR(pG->pRoom, 0x33), ROOM_ARC_PTR(pG->pRoom, 0x34), ROOM_ARC_PTR(pG->pRoom, 0x35),
                     ROOM_ARC_PTR(pG->pRoom, 0x36), ROOM_ARC_PTR(pG->pRoom, 0x37), ROOM_ARC_PTR(pG->pRoom, 0x38),
                     ROOM_ARC_PTR(pG->pRoom, 0x39), ROOM_ARC_PTR(pG->pRoom, 0x3A), ROOM_ARC_PTR(pG->pRoom, 0x3B),
                     ROOM_ARC_PTR(pG->pRoom, 0x3C), ROOM_ARC_PTR(pG->pRoom, 0x3D), ROOM_ARC_PTR(pG->pRoom, 0x3E),
                     ROOM_ARC_PTR(pG->pRoom, 0x3F), ROOM_ARC_PTR(pG->pRoom, 0x40), ROOM_ARC_PTR(pG->pRoom, 0x41)};
    int i;

    switch (pl->r_no_2) {
    case 0:
        r318_work->plPos = pPL->pos;
        __builtin_memcpy((u8*) r318_work + 0xD8, &pPL->ang, sizeof(Vec));
        pPL->pos.x = 0.0f;
        pPL->pos.y = 0.0f;
        pPL->pos.z = 0.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = 0.0f;
        pPL->ang.z = 0.0f;
        pPL->atari.off();
        MotionSetCore(pl, &pl->Motion, mot, 0, 0, 0x201, 0);
        EstSet(pPL, -1, 0, 0, EFF_ROOM, 0xD, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        r318_work->str = SndStrPlayBlock(1, 0xC6, 0.0f);
        for (i = 0; i < 15; i++) {
            cObj* laser = r318_work->laser[i];

            if (laser) {
                MotionSetCore(laser, &laser->Motion, tbl[i], 0, 0, 0, 0);
            }
        }
        pl->r_no_2++;
    case 1:
        pl->dmg.m_Timer = 0x78;
        if (MotionMove(pl, 0)) {
            pG->Room_flg[0] |= 0x00080000;
            R318EventLaserEnd(4);
            pPL->atari.on();
            EffectDelete(0x2001, ESP_CORE_KIND_ROOM04);
            EndPlDamage();
        }
        break;
    }
}

// Cut by the lasers.
static void playerDie(cPlayer* pl)
{
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 3, 0x201, 0);
        EstSet(pl, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(6, 5, 0, 0, 0, 0);
        pG->pl_life = 0;
        PlSetDamageSe(0xA);
        pl->r_no_2++;
    case 1:
        if (pl->Motion.Seq_frame > 19.7f && pl->Motion.Seq_frame < 20.3f) {
            SndCall(5, 5, 0, 0, 0, 0);
        }
        if (pl->Motion.Seq_frame > 22.7f && pl->Motion.Seq_frame < 23.3f) {
            PlSetDamageSe(0xD);
        }
        MotionMove(pl, 0);
        break;
    }
}
