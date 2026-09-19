#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "datactrl.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "item.h"
#include "mes.h"
#include "sscrn.h"
#include "fade.h"
#include "snd.h"
#include "rnd.h"

// Room 1-04 (D:/Bio4/Prog/r104.cpp): the farm; the double door, the enemy reset waves per area,
// the patrolling Ganados, the shelves / boxes to open and the four events (s00 arrival, s10, s20).

// One reset wave: three enemies with their flag numbers (the reset table entries).
struct R104ResetData {
    int em[3];        // 0x00  list entries
    int flagA;        // 0x0C  save flag: wave 1 done
    int flagB;        // 0x10  wave 2 done
    int flagC;        // 0x14  wave 3 done
    int flagD;        // 0x18  pG->flags_174 bit: the wave was set once
};

struct EmReset : R104ResetData {
    cEmWrap em[3];    // 0x1C
};

// One patrol: the enemy and its second way point.
struct R104PatrolData {
    int no;           // 0x00
    Vec pos;          // 0x04
};

class cPatrol104 {
public:
    int active;       // 0x00
    cEmWrap em;       // 0x04
    u32 nPoint;       // 0x10
    u32 cur;          // 0x14
    Vec pos[2];       // 0x18
};

struct R104Work {
    int pad_0;
    u32 emNum;        // 0x04  enemies to kill before the next wave
    u32 resetCnt;     // 0x08  waves done
    EmReset reset[4]; // 0x0C
    u32 strId;        // 0x10C
    cEmDoor* door0;   // 0x110
    cEmDoor* door1;   // 0x114
};

static R104Work* r104_work;

// The original's .rodata and .data are 8-aligned (the .rodata end is padded to 0x2a8; r105 has the same).
// Both tables are global in the REL (ADDR16 fields hold A only).
asm(".section .rodata; .balign 8; .section .data; .balign 8");
R104ResetData r104_resetData[4] = {
    {{0xD9, 0xDA, 0xDB}, 2, 3, 4, 1},
    {{0xDC, 0xDD, 0xDE}, 5, 6, 7, 2},
    {{0xDF, 0xE0, 0xE1}, 8, 9, 0xA, 3},
    {{0xE2, 0xE3, 0xE4}, 0xB, 0xC, 0xD, 4},
};

R104PatrolData r104_patrolData[7] = {
    {0xF0, {16350.0f, 70.0f, -4050.0f}},
    {0xF1, {31430.0f, 6740.0f, -16170.0f}},
    {0xF3, {28880.0f, 7900.0f, -29940.0f}},
    {0xF4, {4690.0f, -1950.0f, -4430.0f}},
    {0xF5, {5860.0f, -2120.0f, -12750.0f}},
    {0xF6, {11080.0f, -90.0f, -18990.0f}},
    {0xF7, {24710.0f, 3570.0f, -27760.0f}},
};

// COMPILER-DIFF #4: the original passes the int table entry to the s16 parameter without
// truncation (`lwz` straight into r4); an int-parameter view of the callee (r11d).
int cEmWrapSetPtrI(cEmWrap* w, int no, int list, int errOn) asm("setPtr__7cEmWrapsSci");
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");


// The rooms call Event::FlgOnStatus out of line (event.h has it in-class).
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");

// The running event's key (&EvtMgr.x34 as an accessor result: the address is formed last).
static inline u32* evtKey(EventMgr* m) { return &m->NowExeEvtKey; }

static void r104_checkBgmPlay();
static void r104_execEmDash();
extern "C" void r104_openBox_main(int no, int opened);
static void r104_openedBox();
static void r104_openBox(int no);
extern "C" void r104_openShelf_main(int no, int opened);
static void r104_openedShelf();
static void r104_openShelf(int no);
extern "C" void cPatrol104_getNextTarget(cPatrol104* p, Vec* out);
extern "C" void cPatrol104_init(cPatrol104* p, R104PatrolData* d);
extern "C" void cPatrol104_move(cPatrol104* p);
static void r104_initEmPatrol();
static void r104_execShowView_end();
static void r104_execShowView();
static void r104_checkDoor107KeyUse();
static void r104_checkDoor107();
extern "C" void EmReset_init(EmReset* r, R104ResetData* d);
extern "C" int EmReset_set(EmReset* r);
extern "C" int r104_checkNowArea();
extern "C" void r104_execEmReset(int area);
static void r104_checkEmReset();
static void r104_execEvent20();
static void r104_execEvent10();
static void r104_execEvent00();
static void r104_succeedAction();
static void Evt_R104S00_Func(Event* e);
static void Evt_R104S01_Func(Event* e);

// Room init: doors 1/2 paired as a double door with be_flag 8 and an ambient boost; the s00/s01 event
// callbacks. First visit (Room_flg bit 1 clear, debug trigger 1 skips) runs the arrival event, else the
// kill-count reset waves and the patrols start at once. Area 0x11 = event s10 once (bit 21), area 0x12 =
// event s20 once (bit 22, pre-loaded after s00). Item area 0x97 is the door-107 key, hidden until
// Key_flg[0] 0x00400000 (area 0 = locked door message + key-use watcher). Area 0xA = the view once
// (bit 14), area 0xE = the dash-in wave once (bit 15); three shelf and two box item events; BGM task.
void R104Init()
{
    cModel* m;

#line 49 "D:/Bio4/Prog/r104.cpp"
    r104_work = (R104Work*) MEM_CALLOC(sizeof(R104Work), 1, 0xd);

    if (getRoomEtcDoor(1, (cEm**) &r104_work->door0, 1)) {
        if (getRoomEtcDoor(2, (cEm**) &r104_work->door1, 1)) {
            r104_work->door0->setDoor(r104_work->door1);
        }
        {
            cEmDoor* d = r104_work->door0;

            BitOn(d->be_flag, 8);
            d->AddAmb_b = d->AddAmb_g = d->AddAmb_r = 0x28;
        }
        {
            cEmDoor* d = r104_work->door1;

            BitOn(d->be_flag, 8);
            d->AddAmb_b = d->AddAmb_g = d->AddAmb_r = 0x28;
        }
    }
    EvtMgr.SetFunc("evt_r104s00_func", (void*) Evt_R104S00_Func);
    EvtMgr.SetFunc("evt_r104s01_func", (void*) Evt_R104S01_Func);
    if (DebugTrg(1) == 1) {
        RsfSet(G_ROOM_ID, 1);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceExec(0x12, (TaskFunc) r104_execEvent00, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceExec(0x12, (TaskFunc) r104_checkEmReset, 0, 0, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) r104_initEmPatrol, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 21) == 0) {
        SceAtDataSet_exec(0x11, SCE_LEVEL10, 0, (TaskFunc) r104_execEvent10, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 22) == 0) {
        if (RsfCheck(G_ROOM_ID, 1)) {
            EvtMgr.EvtReadAram("event/evd/r104s20.evd", 0, 0, 0, 0);
        }
        SceAtDataSet_exec(0x12, SCE_LEVEL10, 0, (TaskFunc) r104_execEvent20, 0, 1);
    }
    SceAtSetEnable(0x97, 1);
    m = SceAtItemModelPtr(0x97);
    if (m != 0) {
        m->LightInfo.EnableMask = (m->LightInfo.EnableMask & ~0x20) | 0x10;
        m->setNoSuspend(1);
    }
    if (!(pG->Key_flg[0] & 0x00400000)) {
        SceAtSetEnable(0x97, 0);
        SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r104_checkDoor107, 0, 1);
        SceExec(0x12, (TaskFunc) r104_checkDoor107KeyUse, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 14) == 0) {
        SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r104_execShowView, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 15) == 0) {
        SceAtDataSet_exec(0xE, SCE_LEVEL10, 0, (TaskFunc) r104_execEmDash, 0, 1);
    }
    SceSetItemEvent(0xB, 0x84, 0x10, 7, r104_openShelf, r104_openedShelf, 0, 0);
    SceSetItemEvent(0xC, 0x8E, 0x11, 8, r104_openShelf, r104_openedShelf, 1, 0);
    SceSetItemEvent(0xD, 0x89, 0x12, 9, r104_openShelf, r104_openedShelf, 2, 0);
    SceSetItemEvent(0xF, 0x8F, 0x13, 0xA, r104_openBox, r104_openedBox, 0, 0);
    SceSetItemEvent(0x10, 0x90, 0x14, 0xB, r104_openBox, r104_openedBox, 1, 0);
    SceExec(0x12, (TaskFunc) r104_checkBgmPlay, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Per-frame room main: nothing.
void R104Main()
{
}

// Battle stream from the first contact until every wave is done and the Ganados are dead.
static void r104_checkBgmPlay()
{
    while (SceCkFindPL(0) != 1) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 3, 1);
    while (!(RsfCheck(G_ROOM_ID, 3) && RsfCheck(G_ROOM_ID, 6) && RsfCheck(G_ROOM_ID, 9) && RsfCheck(G_ROOM_ID, 12))) {
        SceSleep(1);
    }
    while (SceCountEmAlive(0x10, 0x20) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}

// Three more Ganados rush in from the back.
static void r104_execEmDash()
{
    RsfSet(G_ROOM_ID, 15);
    EM_LIST(0xF2)->be_flag |= 1;
    EM_LIST(0xFC)->be_flag |= 1;
    EM_LIST(0xFD)->be_flag |= 1;
    setEm(0xF2, -1, 0, 1, 1);
    setEm(0xFC, -1, 0, 1, 1);
    setEm(0xFD, -1, 0, 1, 1);
    r104_work->emNum += 3;
}

// Box `no` opens (opened: already open, snap the lid).
extern "C" void r104_openBox_main(int no, int opened)
{
    f32 ang = 0.0f;
    cObj* obj = 0;

    switch (no) {
    case 0:
        obj = SmdGetObjPtr(0x36);
        ang = 1.7f;
        break;
    case 1:
        obj = SmdGetObjPtr(0x38);
        ang = 1.7f;
        break;
    default:
        SceExit();
        break;
    }
    if (obj != 0) {
        obj->be_flag |= 0x20;
        if (opened == 1) {
            obj->pParts->ang.z = ang;
        } else {
            int i;

            ang /= 30.0f;
            SndCall(6, 0x5B, 0, 0, 0, 0);
            for (i = 30; i != 0; i--) {
                if (obj != 0) {
                    obj->pParts->ang.z += ang;
                }
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": pose box `no` open (no comes in r3 untouched, see the note).
static void r104_openedBox()
{
    int no; // uninitialised in the original: the item event passes `no` in r3 and the void-parameter
            // helper forwards whatever r3 holds (the target is `li r4,1; bl` with r3 untouched)
    r104_openBox_main(no, 1);
}

// Item-event opener: animate box `no` open.
static void r104_openBox(int no)
{
    r104_openBox_main(no, 0);
}

// Shelf `no` opens.
extern "C" void r104_openShelf_main(int no, int opened)
{
    f32 ang = 0.0f;
    cObj* obj = 0;

    switch ((u32) no) {
    case 0:
        obj = SmdGetObjPtr(0x29);
        ang = -2.0943952f;
        break;
    case 1:
        obj = SmdGetObjPtr(0x2A);
        ang = -2.7925267f;
        break;
    case 2:
        obj = SmdGetObjPtr(0x2B);
        ang = -2.7925267f;
        break;
    default:
        SceExit();
        break;
    }
    if (obj != 0) {
        obj->be_flag |= 0x20;
        if (opened == 1) {
            obj->pParts->ang.y = ang;
        } else {
            int i;

            ang /= 30.0f;
            SndCall(6, 0x1C, 0, 0, 0, 0);
            for (i = 30; i != 0; i--) {
                if (obj != 0) {
                    obj->pParts->ang.y += ang;
                }
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": pose shelf `no` open.
static void r104_openedShelf()
{
    int no; // same as r104_openedBox
    r104_openShelf_main(no, 1);
}

// Item-event opener: animate shelf `no` open.
static void r104_openShelf(int no)
{
    r104_openShelf_main(no, 0);
}

// Advance the two-point patrol to its next way point (wraps) and return that position.
extern "C" void cPatrol104_getNextTarget(cPatrol104* p, Vec* out)
{
    p->cur++;
    if (p->cur >= p->nPoint) {
        p->cur = 0;
    }
    *out = p->pos[p->cur];
}

// Bind a patrol to the Ganado of table entry d: way points = its spawn position and d->pos; starts it
// walking (goto mode 6) toward the table point. Inactive if the enemy is not alive.
extern "C" void cPatrol104_init(cPatrol104* p, R104PatrolData* d)
{
    cEmWrapSetPtrI(&p->em, d->no, -1, 0);
    if (p->em.isAlive() == 1) {
        p->active = 1;
        p->em.getPos(&p->pos[0]);
        p->pos[1] = d->pos;
        p->cur = 1;
        p->nPoint = 2;
        p->em.setGoto(&d->pos, 6);
    } else {
        p->active = 0;
    }
}

// Per-frame patrol step: stops (active = 0) when the enemy dies or spots the player; otherwise, each time
// the goto finished, walks to the other way point.
extern "C" void cPatrol104_move(cPatrol104* p)
{
    Vec t;

    if (p->active == 0) {
        return;
    }
    if (p->em.isAlive() == 0 || p->em.ckFindPL() == 1) {
        p->active = 0;
    }
    if (p->em.ckGoto() == 0) {
        cPatrol104_getNextTarget(p, &t);
        p->em.setGoto(&t, 6);
    }
}

// The seven patrolling Ganados walk between their start and the table point.
static void r104_initEmPatrol()
{
    u32 i;
    u32 n = 7;

    SceSleep(1);
    cPatrol104 pat[7];
    for (i = 0; i < n; i++) {
        cPatrol104_init(&pat[i], &r104_patrolData[i]);
    }
    SceSleep(1);
    for (;;) {
        for (i = 0; i < n; i++) {
            cPatrol104_move(&pat[i]);
        }
        SceSleep(1);
    }
}

// End of the view event: stream faded out over 50 frames, camera back, SceEventEnd.
static void r104_execShowView_end()
{
    SndStrReq(r104_work->strId, 4, 50, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Show the farm: camera cut 4 with the stream.
static inline f32 FCRef(const f32& v) { return v; }

// Area 0xA once (Room_flg bit 14): stream 0x15 with camera cut 4 (the look over the area), clearing
// Status_flg[1] 0x10000000, until the camera motion ends; player-cancellable.
static void r104_execShowView()
{
    // The 0.0 is loaded after the RsfSet store: a pool constant would move above it (pool loads never
    // depend on stores), a `static const` read through a reference stays below (docs/matching.md, cSceObj).
    static const f32 vol = 0.0f;

    RsfSet(G_ROOM_ID, 14);
    r104_work->strId = SndStrReq(0, 0x15, 0x80000003, 0, 0, FCRef(vol));
    SceSetEventCancel(1, (TaskFunc) r104_execShowView_end, 0, -1, 1);
    SceEventStart(1);
    StaFlagOff(pG, STA_SUSPEND);
    CamCtrl.CutCall(4);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r104_execShowView_end();
}

// The player holds the key: the door unlocks with a camera cut and the message.
static void r104_checkDoor107KeyUse()
{
    while (ItemMgr.check(0xA6) != 1) {
        SceSleep(1);
    }
    SceEventStart(0);
    CamCtrl.CutCall(0xC);
    SceSleep(20);
    SceAtSetEnable(0x97, 1);
    SndCall(6, 3, 0, 0, 0, 0);
    SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    pG->Key_flg[0] |= 0x00400000;
    SceAtDataReset(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The locked door: the sub screen when the key or both halves are held.
static void r104_checkDoor107()
{
    SceUpCut(0, 0xC, 2, UP_CUT_ATTR_CUT_FIX);
    if (ItemMgr.num(0xA6) == 0) {
        if (ItemMgr.num(0xA4) == 1 && ItemMgr.num(0xA5) == 1) {
            SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
        } else {
            CamCtrl.Comeback(0);
        }
    } else {
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
    }
}

// A reset wave from its table entry: the wave flags of an interrupted run are cleared.
extern "C" void EmReset_init(EmReset* r, R104ResetData* d)
{
    r->R104ResetData::em[0] = d->em[0];
    r->R104ResetData::em[1] = d->em[1];
    r->R104ResetData::em[2] = d->em[2];
    r->flagA = d->flagA;
    r->flagB = d->flagB;
    r->flagC = d->flagC;
    IntSet(r->flagD, d->flagD);   // reference store: keeps the `lwz pG` of the RsfCheck below the copies
    if (RsfCheck(G_ROOM_ID, r->flagC) == 0) {
        if (RsfCheck(G_ROOM_ID, r->flagB)) {
            RsfClear(G_ROOM_ID, r->flagB);
        } else {
            RsfClear(G_ROOM_ID, r->flagA);
        }
    }
}

// Sets the wave (first time) or resets its three enemies; 1 when it ran.
extern "C" int EmReset_set(EmReset* r)
{
    if (RsfCheck(G_ROOM_ID, r->flagC)) {
        return 0;
    }
    if (FlagChkVar(&pG->Room_flg, (u32) r->flagD) == 0) {
        cEmWrapSetEmI(&r->em[0], r->R104ResetData::em[0], -1, 0, 0, 1);
        cEmWrapSetEmI(&r->em[1], r->R104ResetData::em[1], -1, 0, 0, 1);
        cEmWrapSetEmI(&r->em[2], r->R104ResetData::em[2], -1, 0, 0, 1);
        FlagOnVar(&pG->Room_flg, (u32) r->flagD);
    } else {
        if (r->em[0].ckResetEnable() != 1 || r->em[1].ckResetEnable() != 1 || r->em[2].ckResetEnable() != 1) {
            return 0;
        }
        r->em[0].setReset();
        r->em[1].setReset();
        r->em[2].setReset();
    }
    U32Set(r104_work->emNum, r104_work->emNum + 3);
    U32Set(r104_work->resetCnt, r104_work->resetCnt + 1);
    if (RsfCheck(G_ROOM_ID, r->flagA) == 0) {
        RsfSet(G_ROOM_ID, r->flagA);
    } else if (RsfCheck(G_ROOM_ID, r->flagB) == 0) {
        RsfSet(G_ROOM_ID, r->flagB);
    } else {
        RsfSet(G_ROOM_ID, r->flagC);
    }
    return 1;
}

// The area the player stands in (0..3), -1 elsewhere.
extern "C" int r104_checkNowArea()
{
    if (SceAtHitCheck(4) == 1) {
        return 0;
    }
    if (SceAtHitCheck(5) == 1) {
        return 1;
    }
    if (SceAtHitCheck(6) == 1 || SceAtHitCheck(7) == 1) {
        return 2;
    }
    if (SceAtHitCheck(8) == 1) {
        return 3;
    }
    return -1;
}

// The next wave for the player's area, by the wave flags.
extern "C" void r104_execEmReset(int area)
{
    switch (area) {
    case 0:
        if (RsfCheck(G_ROOM_ID, 8) == 0) {
            EmReset_set(&r104_work->reset[2]);
            return;
        }
        if (RsfCheck(G_ROOM_ID, 11) == 0) {
            EmReset_set(&r104_work->reset[3]);
            return;
        }
        if (RsfCheck(G_ROOM_ID, 9) == 0) {
            if (EmReset_set(&r104_work->reset[2]) == 0) {
                EmReset_set(&r104_work->reset[3]);
            }
            return;
        }
        if (RsfCheck(G_ROOM_ID, 12) == 0) {
            EmReset_set(&r104_work->reset[3]);
            return;
        }
        break;
    case 1:
        if (RsfCheck(G_ROOM_ID, 11) == 0) {
            EmReset_set(&r104_work->reset[3]);
            return;
        }
        if (RsfCheck(G_ROOM_ID, 8) == 0) {
            EmReset_set(&r104_work->reset[2]);
            return;
        }
        if (RsfCheck(G_ROOM_ID, 12) == 0) {
            if (EmReset_set(&r104_work->reset[3]) == 0) {
                EmReset_set(&r104_work->reset[2]);
            }
            return;
        }
        if (RsfCheck(G_ROOM_ID, 9) == 0) {
            EmReset_set(&r104_work->reset[2]);
            return;
        }
        break;
    case 2:
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            EmReset_set(&r104_work->reset[0]);
            return;
        }
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            EmReset_set(&r104_work->reset[1]);
            return;
        }
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            if (EmReset_set(&r104_work->reset[0]) == 0) {
                EmReset_set(&r104_work->reset[1]);
            }
            return;
        }
        if (RsfCheck(G_ROOM_ID, 6) == 0) {
            EmReset_set(&r104_work->reset[1]);
            return;
        }
        break;
    case 3:
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            EmReset_set(&r104_work->reset[1]);
            return;
        }
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            EmReset_set(&r104_work->reset[0]);
            return;
        }
        if (RsfCheck(G_ROOM_ID, 6) == 0) {
            if (EmReset_set(&r104_work->reset[1]) == 0) {
                EmReset_set(&r104_work->reset[0]);
            }
            return;
        }
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            EmReset_set(&r104_work->reset[0]);
            return;
        }
        break;
    }
}

// Counts the kills and starts the next wave for the player's area.
static void r104_checkEmReset()
{
    u32 i;
    int alive;
    int area;

    SceSleep(1);
    U32Set(r104_work->emNum, SceCountEmAlive(0x10, 0x20));
    if (r104_work->emNum <= 10) {
        r104_work->emNum = 10;
    }
    r104_work->resetCnt = 0;
    for (i = 0; i < 4; i++) {
        EmReset_init(&r104_work->reset[i], &r104_resetData[i]);
    }
    for (;;) {
        SceSleep(1);
        alive = SceCountEmAlive(0x10, 0x20);
        if (SceAtHitCheck(3) == 1) {
            RsfSet(G_ROOM_ID, 5);
            RsfSet(G_ROOM_ID, 6);
            RsfSet(G_ROOM_ID, 7);
        }
        area = r104_checkNowArea();
        if (area < 0) {
            continue;
        }
        if (r104_work->emNum - alive < r104_work->resetCnt * 3 + 3) {
            continue;
        }
        r104_execEmReset(area);
    }
}

// The house event (s20): the player is put on the other side, the sub screen shows the file.
static void r104_execEvent20()
{
    cPlayer* pl;

    while (SceCheckEventStart() == 0) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 22);
    EvtMgr.EvtReadExec("event/evd/r104s20.evd", 0x13, 0);
    FadeSetW(1, 0, 0, 0);
    setEm(0x5E, -1, 0, 1, 1);
    Vec pos;
    Vec* pa = &pos;
    pl = pPL;
    pos.x = -11843.0f;
    pa->y = 0.0f;
    pa->z = 12672.0f;
    f32 ry = 0.5f;
    pl->setPos(&pos);
    pos.x = 0.0f;
    pa->y = ry;
    pos.z = 0.0f;
    pl->setAng(&pos);
    FadeSetW(1, 0, 0, 0);
    SubScreenOpen(SS_OPEN_SHOP, 0);
}

// Area 0x11 once (Room_flg bit 21): sets Scenario_flg[1] 0x20000000 and plays event r104s10 (slot 0x13).
static void r104_execEvent10()
{
    ScfFlagOn(pG, SCF_R104_MEET_MERCHANT);
    RsfSet(G_ROOM_ID, 21);
    EvtMgr.EvtReadExec("event/evd/r104s10.evd", 0x13, 0);
}

// Arrival (s00): the intro event with the doors open-locked, then the enemies and the patrols.
static void r104_execEvent00()
{
    RsfSet(G_ROOM_ID, 1);
    if (Rnd() & 0x80) {
        pG->Room_flg[0] |= 0x04000000;
    } else {
        pG->Room_flg[0] &= ~0x04000000;
    }
    int skip = 0;
    DC.setAramSort(0);
    // The user variable on one side of the two tests keeps the pre-cse1 thread_jumps from threading the
    // first `beq` past the second test (rtx_equal_for_thread_p rejects REG_USERVAR_P pseudos): the second
    // compare is cse-deleted but its `bne` survives, as in the original (docs/research/, st1_1 pass 2).
    u32 f = pG->System_flg;
    if (f & 0x40) {
        skip = 1;
    }
    if (!SysFlagChk(pG, SYS_START_EVT_SKIP)) {
        SceEventStart(0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        EvtMgr.EvtReadAram("event/evd/r104s01.evd", 0, 0, 0, 0);
        EvtMgr.EvtReadAram("event/evd/r104s02.evd", 0, 0, 0, 0);
        EvtMgr.EvtReadAram("event/evd/r104s10.evd", 0, 0, 0, 0);
        EvtMgr.EvtReadAram("event/evd/r104s20.evd", 0, 0, 0, 0);
        if (r104_work->door0 != 0) {
            r104_work->door0->setOpenLock(1);
        }
        if (r104_work->door1 != 0) {
            r104_work->door1->setOpenLock(0);
        }
        if (EvtMgr.EvtReadExec("event/evd/r104s00.evd", 0, 0x20)) {
            SysFlagOn(pG, SYS_SCREEN_STOP);
            if (pG->Room_flg[0] & 0x80000000) {
                EvtMgr.EvtReadExec("event/evd/r104s01.evd", 0, 0x20);
            } else {
                SysFlagOff(pG, SYS_START_EVT_SKIP);
                EvtMgr.EvtReadExec("event/evd/r104s02.evd", 0, 2);
                for (;;) {
                    SceSleep(1);
                }
            }
        }
        if (r104_work->door0 != 0) {
            r104_work->door0->setNormal();
        }
        if (r104_work->door1 != 0) {
            r104_work->door1->setNormal();
        }
        SceEventEnd(0);
    }
    setEm(0xF0, -1, 0, 1, 1);
    setEm(0xF1, -1, 0, 1, 1);
    setEm(0xF3, -1, 0, 1, 1);
    setEm(0xF4, -1, 0, 1, 1);
    setEm(0xF5, -1, 0, 1, 1);
    setEm(0xF6, -1, 0, 1, 1);
    setEm(0xF7, -1, 0, 1, 1);
    setEm(0xF8, -1, 0, 1, 1);
    setEm(0xF9, -1, 0, 1, 1);
    setEm(0xFA, -1, 0, 1, 1);
    setEm(0xFB, -1, 0, 1, 1);
    setEm(0xFE, -1, 0, 1, 1);
    SceExec(0x12, (TaskFunc) r104_initEmPatrol, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r104_checkEmReset, 0, 0, SCE_PRIO_DEF_2, 0);
    SndBgmTblSet(0x104, 1);
    if (skip == 0) {
        OpeSetOpenTerm(4, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    DC.setAramSort(1);
}

// Action-button success callback of the s00 event: Room_flg[0] bit 31 (the event's QTE passed).
static void r104_succeedAction()
{
    pG->Room_flg[0] |= 0x80000000;
}

// Event r104s00 callback: funcMode 0 marks status 3 and sets the cancel cut 0x1E; cuts 0/1/3 parent the
// kind-1 light to the event model evm4200; later cuts set the fade and the draw / CMF flags of the event
// models per cut.
static void Evt_R104S00_Func(Event* e)
{
    void* mod;
    void* mod2;
    int fadeOn;

    switch (e->funcMode) {
    case 0:
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 0x1E;
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
        case 1:
        case 3:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evm4200", 0, 0) == 1) {
                    cLight* l = LightMgr.getKindLight(1);

                    if (l != 0) {
                        l->setParent((cModel*) mod);
                    }
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                fadeOn = 1;
                if (!(e->StatusFlag & 0x40000000)) {
                    fadeOn = 0;
                }
                if (fadeOn == 0) {
                    FadeSetW(2, 0, 0, 0);
                }
                if (e->GetMod(&mod2, "evm4500", 0, 0) == 1) {
                    ((cModel*) mod2)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod2, "evm4600", 0, 0) == 1) {
                    ((cModel*) mod2)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod2, "evm3700", 0, 0) == 1) {
                    ((cModel*) mod2)->ot_type = 1;
                }
            }
            if (e->NowFrame == 120) {
                fadeOn = 1;
                if (!(e->StatusFlag & 0x40000000)) {
                    fadeOn = 0;
                }
                if (fadeOn == 0) {
                    FadeSetW(0x80000002, 60, 0, 0);
                }
            }
            break;
        case 0x1E:
            SpfFlagOff(pG, SPF_ACTBTN);
            if (!(pG->Room_flg[0] & 0x80000000)) {
                DpfFlagOff(pG, DPF_MESSAGE);
                if (!(pG->Room_flg[0] & 0x04000000)) {
                    ActBtn.set(0x25, 5, (int) r104_succeedAction, 0, 0x42, 4, 0, 0);
                } else {
                    ActBtn.set(0x25, 5, (int) r104_succeedAction, 0, 0x42, 3, 0, 0);
                }
            } else {
                e->CancelSet();
            }
            break;
        }
        break;
    case 2:
        break;
    case 3:
        fadeOn = 1;
        if (!(e->StatusFlag & 0x4000)) {
            fadeOn = 0;
        }
        if (fadeOn == 0) {
            EvtMgr.EvtSndStrPlay(evtKey(&EvtMgr), 1, 0x86, 1, 0.0f);
        }
        break;
    }
}

// Event r104s01 callback: on its first frame make the event model evm4500 draw with be_flag 0x10.
static void Evt_R104S01_Func(Event* e)
{
    void* mod;

    if (e->funcMode == 1 && e->NowCut == 0 && e->NowFrame == 0) {
        if (e->GetMod(&mod, "evm4500", 0, 0) == 1) {
            ((cModel*) mod)->be_flag |= 0x10;
        }
    }
}
