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
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emdoor.h"
#include "emhit.h"
#include "emwindow.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "sscrn.h"
#include "esp.h"
#include "snd.h"
#include <string.h>

// Room 1-05 (D:/Bio4/Prog/r105.cpp): the church; the insignia dial puzzle on the door, the
// cesspit cover with the key item, the Ganado wave after the rescue and the s00/s10 events.

cModelInfo* GetModelInfoAddr(cModelInfo* info, int no);   // game/TexRender.cpp
void Obj18CmfOn(cObj* o, u32 n);                                     // game/obj18.cpp


// One dial part of the door puzzle: the object and its rest matrix.
struct R105MarkObj {
    cObj* obj;    // 0x00
    Mtx mat;      // 0x04
};

// Door puzzle work (R105Work + 0x08).
struct R105Mark {
    u8 mes;               // 0x00  message shown
    s8 sel;               // 0x01  SceMesGetSelection result (1..4 = turn direction)
    u8 pad_2[2];
    int ang;              // 0x04  turn angle (degrees, 0..90)
    int cnt;              // 0x08  open animation frames
    R105MarkObj obj[16];  // 0x0C
    int num;              // 0x34C
};

struct R105Work {
    u8 state;             // 0x00  r105_markTbl index
    u8 sub;               // 0x01  step inside the state
    u8 pad_2[2];
    u32 se;               // 0x04  SndCall handle of the door SE
    R105Mark mk;          // 0x08
    cEmWrap em[16];       // 0x358
};

static R105Work* r105_work;

// Hit effects of attribute type 4 (water)
static const AtEffInfo r105_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

static void r105_moveShelf(int id);
static void r105_movedShelf(int id);
static void r105_keyItem();
static void r105_mark();
static void r105_markInit();
static void r105_markMain();
extern "C" int r105_markOpenCk();
static void r105_markOpen();
static void r105_markEnd();
extern "C" void r105_markDoorOpen();
extern "C" void r105_markMtxInit();
extern "C" void r105_markMtxClean(cObj* obj, Mtx m);
extern "C" void r105_markMtxCopy(Mtx dst, Mtx src);
static void r105_Event();
static void r105_openTerm();
extern "C" void r105_EmSet();
static void r105_StreanChk();
static void r105_bgmCheck();
static void r105_checkDoor();
extern "C" void Evt_R105S00_Func(Event* e);
extern "C" void Evt_R105S10_Func(Event* e);
static void r105_execOpenCover();
static void r105_checkCloseCover();
extern "C" void r105_checkCesspit0();
extern "C" void r105_checkCesspit1();
extern "C" void r105_checkCesspit2();
static void r105_initCesspit();

// The original's .data is 8-aligned (the table is its only content).
asm(".section .data; .balign 8");
static void (*r105_markTbl[4])() = {
    r105_markInit,
    r105_markMain,
    r105_markOpen,
    r105_markEnd,
};

// Room init (debug trigger 1 re-arms the dial puzzle). Registers the s00/s10/s99 event callbacks, five
// shelf/box item events (items 0x92/0x93/0x94/0x88/0x8B), the key-item camera show at area 0x1A once
// (Room_flg bit 12), BGM task, floor hit effects; area 1 = the locked front door until Key_flg[0]
// 0x02000000 (else object 0x23 hidden); the dial puzzle on area 6 until Room_flg bit 0 (else the door
// parts are removed). Bit 1 = s00 seen (else pre-load r105s00), bit 2 = s10 seen (window 5 broken, area
// 0x17 on; else pre-load r105s10 after s00); after s00 the Ganado wave (r105_EmSet) and the terminal
// once (bit 11). Window 5 takes no damage; door 1 light mask 4; then the cesspit setup.
void R105Init()
{
    cEmWindow* win;
    cEmWindow* win2;
    cEmDoor* door;
    cObj* obj;

    if (DebugTrg(1)) {
        RsfClear(G_ROOM_ID, 0);
    }
#line 73 "D:/Bio4/Prog/r105.cpp"
    r105_work = (R105Work*) MEM_CALLOC(sizeof(R105Work), 1, 0xD);
    EvtMgr.SetFunc("evt_r105s00_func", (void*) Evt_R105S00_Func);
    EvtMgr.SetFunc("evt_r105s10_func", (void*) Evt_R105S10_Func);
    EvtMgr.SetFunc("evt_r105s99_func", (void*) Evt_R105S10_Func);
    SceSetItemEvent(0xA, 0x92, 6, 8, r105_moveShelf, r105_movedShelf, 0x92, 0);
    SceSetItemEvent(0xB, 0x93, 7, 6, r105_moveShelf, r105_movedShelf, 0x93, 0);
    SceSetItemEvent(0xD, 0x94, 8, 9, r105_moveShelf, r105_movedShelf, 0x94, 0);
    SceSetItemEvent(0xE, 0x88, 9, 0xB, r105_moveShelf, r105_movedShelf, 0x88, 0);
    SceSetItemEvent(0xF, 0x8B, 0xA, 0xA, r105_moveShelf, r105_movedShelf, 0x8B, 0);
    if (RsfCheck(G_ROOM_ID, 12) == 0) {
        SceAtDataSet_exec(0x1A, SCE_LEVEL10, 0, r105_keyItem, 0, 1);
    }
    SceExec(0x12, r105_bgmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    EatMgr.registEffInfo(EAT_ET_ROOM0, (AtEffInfo*) &r105_eff_info);
    if (!KyfFlagChk(pG, KYF_R105_TO_R101_DOOR)) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, r105_checkDoor, 0, 1);
    } else {
        SmdSetTrans(0x23, 0);
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        r105_markMtxInit();
        SceAtDataSet_exec(6, SCE_LEVEL10, 0, r105_mark, 0, 1);
        obj = SmdGetObjPtr(0x22);
        if (obj) {
            obj->Shader_type = 1;
            obj->Refract_pow = 2;
        }
    } else {
        r105_markDoorOpen();
        SceAtSetEnable(6, 0);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        if (RsfCheck(G_ROOM_ID, 1)) {
            EvtMgr.EvtReadAram("event/evd/r105s10.evd", 0x15, 0, 0, 0);
        }
        SceAtSetEnable(0x17, 0);
    } else {
        if (getRoomEtcWindow(5, &win, 1)) {
            win->SetBreakModel();
        }
        SceAtSetEnable(0x17, 1);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        EvtMgr.EvtReadAram("event/evd/r105s00.evd", 0x15, 0, 0, 0);
    } else {
        r105_EmSet();
        if (RsfCheck(G_ROOM_ID, 11) == 0) {
            SceExec(0x12, r105_openTerm, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    if (getRoomEtcWindow(5, &win2, 1)) {
        win2->SetEnableDamage(0);
        win2->SetEnableFence(0, 0);
    }
    SceAtSetEnable(8, 0);
    if (getRoomEtcDoor(1, &door, 1)) {
        door->LightInfo.EnableMask = 4;
    }
    SceExec(0x12, r105_initCesspit, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Per frame: when the key item (Item_flg[0] 0x20000000) is picked up for the first time
// (Room_flg[0] 0x40000000), arm area 8 with the s00/s10 event and close-lock door 1.
void R105Main()
{
    cEmDoor* door;

    getRoomEtcDoor(1, &door, 1);
    if (ItfFlagChk(pG, ITF_R105_ITEM) && !(pG->Room_flg[0] & 0x40000000)) {
        pG->Room_flg[0] |= 0x40000000;
        if (RsfCheck(G_ROOM_ID, 1) == 0 || RsfCheck(G_ROOM_ID, 2) == 0) {
            SceAtSetEnable(8, 1);
            SceAtDataSet_exec(8, SCE_LEVEL10, 0, r105_Event, 0, 1);
            if (door) {
                door->setCloseLock();
            }
        }
    }
}

// Item-event "already opened": pose the shelf/box of item `id` open (OpenBoxMain per item: 0x92 shelf
// type 0x19, 0x94/0x8B type 0x1A, 0x88 drawer down, 0x93 slide -500 X).
static void r105_movedShelf(int id)
{
    if (id == 0x92) {
        OpenBoxMain(0, 1, 0x19, 0x2B, 0x2C, -1);
    }
    if (id == 0x94) {
        OpenBoxMain(0, 1, 0x1A, 0x35, 0x36, -1);
    }
    if (id == 0x8B) {
        OpenBoxMain(0, 1, 0x1A, 0x37, 0x38, -1);
    }
    if (id == 0x88) {
        OpenBoxMain(OpenBoxDwXP, 1, 0x18, 0x39, 0xFFFFFFFF, -1);
    }
    if (id == 0x93) {
        OpenBoxMain(OpenBoxPosXM500, 1, 0x1B, 0x3B, 0xFFFFFFFF, 0x93);
    }
}

// Item-event opener: animate the shelf/box of item `id` open (same table as r105_movedShelf).
static void r105_moveShelf(int id)
{
    if (id == 0x92) {
        OpenBoxMain(0, 0, 0x19, 0x2B, 0x2C, -1);
    }
    if (id == 0x94) {
        OpenBoxMain(0, 0, 0x1A, 0x35, 0x36, -1);
    }
    if (id == 0x8B) {
        OpenBoxMain(0, 0, 0x1A, 0x37, 0x38, -1);
    }
    if (id == 0x88) {
        OpenBoxMain(OpenBoxDwXP, 0, 0x18, 0x39, 0xFFFFFFFF, -1);
    }
    if (id == 0x93) {
        OpenBoxMain(OpenBoxPosXM500, 0, 0x1B, 0x3B, 0xFFFFFFFF, 0x93);
    }
}

// Area 0x1A: the camera shows the key item once, then the item area 0x8E runs.
static void r105_keyItem()
{
    if (RsfCheck(G_ROOM_ID, 12) == 0) {
        RsfSet(G_ROOM_ID, 12);
        SceAtSetEnable(0x1A, 0);
        CamCtrl.CutCall(0x10);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceAtSetEnable(0x8E, 1);
        SceAtExecute(0x8E);
        CamCtrl.Comeback(0);
    }
}

// Area 6: the door puzzle state machine.
static void r105_mark()
{
    r105_work->state = 0;
    r105_work->sub = 0;
    for (;;) {
        r105_markTbl[r105_work->state]();
        SceSleep(1);
    }
}

// Dial puzzle state 0: event start, camera cut 1 on the door emblem, the "turn?" message (0x20), -> state 1.
static void r105_markInit()
{
    R105Mark* mk = &r105_work->mk;

    SceEventStart(0);
    mk->mes = 0;
    mk->sel = 1;
    CamCtrl.CutCall(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceMesSet(1, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    r105_work->state = 1;
    r105_work->sub = 0;
}

// Dial puzzle state 1. sub 0: message 0x220/0x2A0 asks the turn direction (1/2 = about X +/-, 3/4 =
// about Y -/+; -1/5/6 = leave -> state 3). sub 1: turn the emblem 4 degrees a frame to 90, then bake the
// matrix; r105_markOpenCk true (emblem upright or half-turned) -> message 3/0x20 and state 2 (open).
static void r105_markMain()
{
    R105Mark* mk = &r105_work->mk;
    Mtx m;
    Mtx tmp;
    int i;

    switch (r105_work->sub) {
    case 0:
        if (mk->mes == 0) {
            SceMesSet(2, 0x220, mk->sel, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        } else {
            SceMesSet(2, 0x2A0, mk->sel, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        }
        mk->mes = 1;
        mk->sel = SceMesGetSelection();
        switch (mk->sel) {
        case 1:
        case 2:
        case 3:
        case 4:
            mk->ang = 0;
            if (mk->obj[0].obj) {
                SndCall(6, 4, &mk->obj[0].obj->pos, 0, 0, 0);
            }
            r105_work->sub = 1;
            break;
        case -1:
        case 5:
        case 6:
            r105_work->state = 3;
            r105_work->sub = 0;
            break;
        }
        break;
    case 1:
        mk->ang += 4;
        if (mk->ang > 89) {
            mk->ang = 90;
        }
        PSMTXIdentity(m);
        switch (mk->sel) {
        case 1:
            PSMTXRotRad(m, 'x', (f32) mk->ang * 0.017453292f);
            break;
        case 2:
            PSMTXRotRad(m, 'x', (f32) -mk->ang * 0.017453292f);
            break;
        case 3:
            PSMTXRotRad(m, 'y', (f32) -mk->ang * 0.017453292f);
            break;
        case 4:
            PSMTXRotRad(m, 'y', (f32) mk->ang * 0.017453292f);
            break;
        }
        for (i = 0; i < mk->num; i++) {
            cObj* obj = mk->obj[i].obj;
            MtxPtr mat = mk->obj[i].mat;

            if (obj) {
                PSMTXConcat(m, mat, tmp);
                r105_markMtxCopy(obj->l_mat, tmp);
                memcpy(obj->mat, obj->l_mat, sizeof(Mtx));
                if (obj->pParts) {
                    obj->partsMatCalc();
                    obj->partsWorldCalc();
                }
            }
        }
        if (mk->ang > 89) {
            cObj* obj = mk->obj[0].obj;
            int ck;

            if (obj) {
                MtxPtr wm = obj->l_mat;
                MtxPtr mat = mk->obj[0].mat;

                r105_markMtxClean(obj, wm);
                r105_markMtxCopy(mat, wm);
            }
            ck = r105_markOpenCk();
            if (ck) {
                if (mk->obj[0].obj) {
                    SndCall(6, 5, &mk->obj[0].obj->pos, 0, 0, 0);
                }
                SceMesSet(3, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
                r105_work->state = 2;
                r105_work->sub = 0;
            } else {
                r105_work->sub = ck;
            }
        }
        break;
    }
}

// 1 when the dial's rotation matrix is the identity or the half turn about Y.
extern "C" int r105_markOpenCk()
{
    // The work pointer is loaded before the three template copies (its `lwz` is issued between
    // the vz.x and vz.y stores in the original: the RTL order decides the sched1 tie).
    R105Mark* mk = &r105_work->mk;
    Vec vx = {1.0f, 0.0f, 0.0f};
    Vec vy = {0.0f, 1.0f, 0.0f};
    Vec vz = {0.0f, 0.0f, 1.0f};
    Mtx m;

    if (mk->obj[0].obj) {
        r105_markMtxCopy(m, mk->obj[0].obj->mat);
    }
    PSMTXMultVec(m, &vx, &vx);
    PSMTXMultVec(m, &vy, &vy);
    PSMTXMultVec(m, &vz, &vz);
    if ((vx.x > 0.5f && vx.y < 0.5f && vx.z < 0.5f && vy.x < 0.5f && vy.y > 0.5f && vy.z < 0.5f && vz.x < 0.5f &&
         vz.y < 0.5f && vz.z > 0.5f) ||
        (vx.x < -0.5f && vx.y < 0.5f && vx.z < 0.5f && vy.x < 0.5f && vy.y > 0.5f && vy.z < 0.5f && vz.x < 0.5f &&
         vz.y < 0.5f && vz.z < -0.5f)) {
        return 1;
    }
    return 0;
}

// Dial puzzle state 2: camera cut 5 with the door SE, the four door parts slide 22.2 units a frame
// (sub 1) until the door is out of the way, then remove them, set Room_flg bit 0, wait for the camera.
static void r105_markOpen()
{
    R105Mark* mk = &r105_work->mk;
    cObj* obj[4];
    cObj* o;
    int i;

    obj[0] = SmdGetObjPtr(0x20);
    obj[1] = SmdGetObjPtr(0x21);
    obj[2] = SmdGetObjPtr(0x22);
    obj[3] = SmdGetObjPtr(0x32);
    switch (r105_work->sub) {
    case 0:
        CamCtrl.CutCall(5);
        o = obj[0];
        if (o) {
            r105_work->se = SndCall(6, 3, &o->pos, 0, 0, 0);
        }
        SmdSetTrans(0x22, 0);
        mk->cnt = 0;
        r105_work->sub++;
        break;
    case 1:
        for (i = 0; i < 4; i++) {
            o = obj[i];
            if (o) {
                o->pos.x -= 22.222222f;
                o->matUpdate();
            }
        }
        if (Key.trg & 0x20000000) {
            CamCtrl.Comeback(0);
            SndStop(r105_work->se, 0);
            mk->cnt = 90;
            SubScreenWait(20);
        }
        mk->cnt++;
        if (mk->cnt > 90) {
            r105_markDoorOpen();
            SceAtSetEnable(6, 0);
            RsfSet(G_ROOM_ID, 0);
            r105_work->sub++;
        }
        break;
    case 2:
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        r105_work->state = 3;
        r105_work->sub = 0;
        break;
    }
}

// Dial puzzle state 3: end the event and kill the puzzle task.
static void r105_markEnd()
{
    SceEventEnd(0);
    SceExit();
}

// Door already solved: hide the four door parts (smd 0x20/0x21/0x22/0x32) and disable area 7.
extern "C" void r105_markDoorOpen()
{
    SmdSetTrans(0x20, 0);
    SmdSetTrans(0x21, 0);
    SmdSetTrans(0x22, 0);
    SmdSetTrans(0x32, 0);
    SceAtSetEnable(7, 0);
}

// Puzzle setup: the emblem objects (smd 0x21/0x22) get a rest matrix rotated 90 deg about Y then -90 deg
// about X, copied into their l_mat / mat.
extern "C" void r105_markMtxInit()
{
    R105Mark* mk = &r105_work->mk;
    Mtx tmp;
    int i;

    mk->obj[0].obj = SmdGetObjPtr(0x21);
    mk->obj[1].obj = SmdGetObjPtr(0x22);
    mk->num = 1;
    for (i = 0; i < mk->num; i++) {
        cObj* obj = mk->obj[i].obj;

        if (obj) {
            PSMTXIdentity(mk->obj[i].mat);
            PSMTXRotRad(tmp, 'y', 1.5707964f);
            PSMTXConcat(tmp, mk->obj[i].mat, mk->obj[i].mat);
            PSMTXRotRad(tmp, 'x', -1.5707964f);
            PSMTXConcat(tmp, mk->obj[i].mat, mk->obj[i].mat);
            r105_markMtxCopy(obj->l_mat, mk->obj[i].mat);
            memcpy(obj->mat, obj->l_mat, sizeof(Mtx));
            if (obj->pParts) {
                obj->partsMatCalc();
                obj->partsWorldCalc();
            }
        }
    }
}

// Snaps every rotation entry to -1 / 0 / 1.
#define R105_MTX_CLEAN(v) \
    if ((v) > 0.5f) {     \
        (v) = 1.0f;       \
    } else if ((v) < -0.5f) { \
        (v) = -1.0f;      \
    } else {              \
        (v) = 0.0f;       \
    }

// Snap the 3x3 rotation of `m` to exact 0 / +-1 after a 90 degree turn (kills float drift).
extern "C" void r105_markMtxClean(cObj* obj, Mtx m)
{
    R105_MTX_CLEAN(m[0][0]);
    R105_MTX_CLEAN(m[0][1]);
    R105_MTX_CLEAN(m[0][2]);
    R105_MTX_CLEAN(m[1][0]);
    R105_MTX_CLEAN(m[1][1]);
    R105_MTX_CLEAN(m[1][2]);
    R105_MTX_CLEAN(m[2][0]);
    R105_MTX_CLEAN(m[2][1]);
    R105_MTX_CLEAN(m[2][2]);
}

// Copies the rotation part only.
extern "C" void r105_markMtxCopy(Mtx dst, Mtx src)
{
    dst[0][0] = src[0][0];
    dst[0][1] = src[0][1];
    dst[0][2] = src[0][2];
    dst[1][0] = src[1][0];
    dst[1][1] = src[1][1];
    dst[1][2] = src[1][2];
    dst[2][0] = src[2][0];
    dst[2][1] = src[2][1];
    dst[2][2] = src[2][2];
}

// Area 8: the s00 event (first visit) or the s10 event (after the rescue).
static void r105_Event()
{
    cEmDoor* door;

    SceEventStart(0);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        pG->Room_flg[0] &= ~0x20000000;
        SndRoomStrStop(0);
        EvtMgr.EvtReadExec("event/evd/r105s00.evd", 0x15, EvtReadFlagFadeOut);
        EvtMgr.EvtReadAram("event/evd/r105s10.evd", 0x15, 0, 0, 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        SceSleep(2);
        r105_EmSet();
    } else {
        RsfSet(G_ROOM_ID, 2);
        EvtMgr.EvtReadExec("event/evd/r105s10.evd", 0x15, EvtReadFlagNone);
        SceAtSetEnable(8, 0);
        getRoomEtcDoor(1, &door, 1);
        if (door) {
            door->setNormal();
        }
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    SceEventEnd(0);
    if (RsfCheck(G_ROOM_ID, 11) == 0) {
        SceSetChapterEnd(CHAPTER_1_2, -1);
        r105_openTerm();
    }
}

// Once (Room_flg bit 11): open the typewriter terminal 5 at the fixed position/angle.
static void r105_openTerm()
{
    RsfSet(G_ROOM_ID, 11);
    OpeSetOpenTerm(5, 6630.0f, 6344.0f, 2664.0f, -0.58f);
}

// The Ganado wave after the s00 event: ESL 0x64..0x66, 0x68, 0x69, 0x6B..0x6E, 0x59 into em[], plus the
// battle-stream task.
extern "C" void r105_EmSet()
{
    r105_work->em[0].setEm(0x64, -1, 0, 1, 1);
    r105_work->em[1].setEm(0x65, -1, 0, 1, 1);
    r105_work->em[2].setEm(0x66, -1, 0, 1, 1);
    r105_work->em[4].setEm(0x68, -1, 0, 1, 1);
    r105_work->em[5].setEm(0x69, -1, 0, 1, 1);
    r105_work->em[7].setEm(0x6B, -1, 0, 1, 1);
    r105_work->em[8].setEm(0x6C, -1, 0, 1, 1);
    r105_work->em[9].setEm(0x6D, -1, 0, 1, 1);
    r105_work->em[10].setEm(0x6E, -1, 0, 1, 1);
    r105_work->em[11].setEm(0x59, -1, 0, 1, 1);
    SceExec(0x12, r105_StreanChk, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Battle stream: starts while an enemy sees the player, fades out otherwise.

// Battle stream task: Room_flg[0] 0x10000000 mirrors SceCkFindPL; on the rising edge start stream 2
// (stopping the room BGM if 0x20000000 says it plays), on the falling edge fade it out over 600 frames.
static void r105_StreanChk()
{
    static const f32 vol = 0.0f;

    pG->Room_flg[0] &= ~0x08000000;
    for (;;) {
        if (SceCkFindPL(0) == 1) {
            pG->Room_flg[0] |= 0x10000000;
        } else {
            pG->Room_flg[0] &= ~0x10000000;
        }
        if (pG->Room_flg[0] & 0x10000000) {
            if (!(pG->Room_flg[0] & 0x08000000)) {
                pG->Room_flg[0] |= 0x08000000;
                if (pG->Room_flg[0] & 0x20000000) {
                    pG->Room_flg[0] &= ~0x20000000;
                    SndRoomStrStop(0);
                    SceSleep(1);
                }
                SndStrReq(0, 2, 0x80000003, 0, 0, *(const f32*) &vol);
                SceSleep(30);
            }
        } else if (pG->Room_flg[0] & 0x08000000) {
            pG->Room_flg[0] &= ~0x08000000;
            SndStrReq(0, 2, 4, 600, 0, *(const f32*) &vol);
            SceSleep(30);
        }
        SceSleep(1);
    }
}

// Marks Room_flg[0] 0x20000000 (room BGM) off at start (the stream task uses it).
static void r105_bgmCheck()
{
    pG->Room_flg[0] &= ~0x20000000;
}

// Area 1: the locked front door.
static void r105_checkDoor()
{
    SceEventStart(1);
    CamCtrl.CutCall(0xD);
    SceSleep(15);
    SmdSetTrans(0x23, 0);
    SceAtDataReset(1);
    SndCall(6, 0xB, 0, 0, 0, 0);
    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    KyfFlagOn(pG, KYF_R105_TO_R101_DOOR);
    ScfFlagOn(pG, SCF_90);
    CamCtrl.Comeback(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Event r105s00 callback: funcMode 0 shows etc model 1; cuts 0xA..0xC toggle model info 3 (a held item)
// of the Leon model pl0000 off / on.
extern "C" void Evt_R105S00_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        setRoomEtcDisp(1, 0, 1);
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            break;
        case 0xB: {
            void* mod;

            if (e->NowFrame == 0) {
                e->GetMod(&mod, "pl0000", 0, 0);
            }
            break;
        }
        case 0xA:
        case 0xC: {
            void* mod;

            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    cModelInfo* info = GetModelInfoAddr(((cModel*) mod)->pModelInfo, 3);

                    if (info) {
                        info->be_flag &= ~0x20;
                    }
                }
            }
            break;
        }
        }
        break;
    case 2:
        setRoomEtcDisp(1, 1, 1);
        break;
    }
}

// Event r105s10 (and s99) callback, Ashley (pl0200): her model info 5 hidden on cut 0 frame 15 and shown
// from cut 1; cut 0 shows etc model 1 and evm2500; cuts 0x13/0x14 swap Ashley's parts (the alternate
// costume model pl8200 when game_costume == 1); funcMode 2 (end) restores etc model 1's display.
extern "C" void Evt_R105S10_Func(Event* e)
{
    void* mod;
    cEmWindow* win;
    cEmWindow* win2;

    switch (e->FuncType) {
    case 0:
        break;
    case 1:
        if (e->NowCut == 0) {
            if (e->NowFrame == 15) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 5, 0);
                }
            }
        } else {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 5, 1);
                }
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                setRoomEtcDisp(1, 0, 1);
                if (e->GetMod(&mod, "evm2500", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    ((cObjUnion*) mod)->o18.be_flag |= 0x40;
                }
            }
            break;
        case 0x14:
            if (e->NowFrame == 2) {
                if (getRoomEtcWindow(5, &win, 1)) {
                    win->SetBreakModel();
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0x13:
        case 0x14:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    ((cObjUnion*) mod)->o18.be_flag |= 0x40;
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    ((cObjUnion*) mod)->o18.be_flag &= ~0x40;
                }
            }
            break;
        }
        if (e->NowCut == 0xE || e->NowCut == 0x13) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    Obj18Work* w = &((cObjUnion*) mod)->o18;

                    if (w && w->child) {
                        ((cObjUnion*) mod)->o18.ObjChainFlagCommon |= 0x04000000;
                        w->child->be_flag &= ~2;
                    }
                }
            }
        } else {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    Obj18Work* w = &((cObjUnion*) mod)->o18;

                    if (w && w->child) {
                        ((cObjUnion*) mod)->o18.ObjChainFlagCommon &= ~0x04000000;
                        w->child->be_flag |= 2;
                    }
                }
            }
        }
        if (pG->game_costume == 1) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl8200", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag &= ~2;
                }
            }
        }
        break;
    case 2:
        setRoomEtcDisp(1, 1, 1);
        if (getRoomEtcWindow(5, &win2, 1)) {
            win2->SetBreakModel();
        }
        SceAtSetEnable(0x17, 1);
        break;
    }
}

// Area 0xC: opens the cesspit cover.
static void r105_execOpenCover()
{
    cObj* obj;

    RsfSet(G_ROOM_ID, 3);
    obj = SmdGetObjPtr(0x30);
    SndCall(6, 0x5F, &obj->pos, 0, 0, 0);
    // `step` a variable (f31 across the call); the exit store on the break path keeps the peeled
    // exit test unfolded so jump2 merges the two exit jumps (docs/matching.md COMPILER-DIFF #7/#9).
    f32 step = 0.06981317f;

    for (;;) {
        obj->pParts->ang.z -= step;
        if (obj->pParts->ang.z < -(73.0f * 0.01f)) {
            obj->pParts->ang.z = -(73.0f * 0.01f);
            break;
        }
        SceSleep(1);
    }
    SceAtSetEnable(9, 1);
}

// Shooting the cover's hit box knocks it off and lifts the lid.
static void r105_checkCloseCover()
{
    cObj* cover;
    cObj* lid;
    cEmHit* hit;

    cover = SmdGetObjPtr(0x31);
    lid = SmdGetObjPtr(0x30);
    cover->be_flag |= 0x20;
    lid->be_flag |= 0x20;
    hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &cover->pos,
                   &cover->ang, 0);
    {
        const f32 w = 100.0f;
        const f32 h = 2000.0f;
        const f32 x = 0.0f;
        const f32 z = 50.0f;
        YarareInitCube(hit, x, x, z, w, h, w, 0, YAT_FLAG_ON);
    }
    do {
        if (hit->ckStatus() == 1) {
            break;
        }
        SceSleep(1);
    } while (1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x10, 0, ESP_CORE_KIND_NONE, 0, 0);
    SndCall(6, 0x5D, &cover->pos, 0, 0, 0);
    cover->be_flag &= ~2;
    SceAtSetEnable(0x11, 1);
    RsfSet(G_ROOM_ID, 4);
    {
        const f32 deg = 0.017453292f;
        f32 spd = 0.0f;
        f32 lim = 0.69f;

        do {
            lid->pParts->ang.z += spd;
            if (lid->pParts->ang.z > lim) {
                lid->pParts->ang.z = 0.69f;
                break;
            }
            spd += 0.017453292f;
            SceSleep(1);
        } while (1);
    }
    SndCall(6, 0x5E, &lid->pos, 0, 0, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x11, 0, ESP_CORE_KIND_NONE, 0, 0);
    RsfSet(G_ROOM_ID, 4);
    lid->pParts->ang.z -= 0.06981317f;
    SceSleep(1);
    lid->pParts->ang.z -= 0.02617994f;
    SceSleep(1);
    lid->pParts->ang.z += 0.02617994f;
    SceSleep(1);
    lid->pParts->ang.z += 0.06981317f;
    SceSleep(1);
}

// Moves the item model of area 0x8D onto area 0x9B's model position and hides it.
static inline void r105_setItemModel(SceAtWork* at)
{
    SceAtWork* at2;

    at->item.id = 0x8A;
    at2 = SceAtPtr(0x9B);
    if (at2->item.pModel && at->item.pModel) {
        at2->item.pModel->pos = at->item.pModel->pos;
        at2->item.pModel->ang = at->item.pModel->ang;
        at->item.pModel->be_flag &= ~2;
        at->item.pModel = at2->item.pModel;
    }
}

// Cesspit state 0 (cover still on, Room_flg bit 4 clear): once the item is found (bit 5) move its model
// onto the lid; when the cover comes off, retarget the find SE, enable area 0x11, arm the cover-open
// prompt on area 0xC and fall through to state 1.
extern "C" void r105_checkCesspit0()
{
    SceAtWork* at = SceAtPtr(0x8D);

    for (;;) {
        if (RsfCheck(G_ROOM_ID, 4) == 0) {
            if (RsfCheck(G_ROOM_ID, 5) == 0 && SceAtItemFindFlgCk(0x8D) == 1) {
                RsfSet(G_ROOM_ID, 5);
                r105_setItemModel(at);
            }
            SceSleep(1);
        } else {
            break;
        }
    }
    at->item.seFind = 0x5B;
    SceAtSetEnable(0x11, 1);
    if (RsfCheck(G_ROOM_ID, 5)) {
        SceAtSetEnable(0x8D, 0);
    }
    SceAtDataSet_exec(0xC, SCE_LEVEL10, 0, r105_execOpenCover, 0, 1);
    SceAtSetEnable(9, 0);
    r105_checkCesspit1();
}

// Cesspit state 1 (cover off, lid closed, bit 3 clear): toggles the open prompt (area 0xC) with the item
// found / taken flags (Room_flg[0] 0x04000000 / 0x02000000); when the lid opens (bit 3) the item sits on
// the lid; exits if the item is already taken, else state 2.
extern "C" void r105_checkCesspit1()
{
    SceAtWork* at = SceAtPtr(0x8D);

    for (;;) {
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            if (RsfCheck(G_ROOM_ID, 5) == 0) {
                if (!(pG->Room_flg[0] & 0x04000000)) {
                    if (SceAtItemFindFlgCk(0x8D) == 1) {
                        SceAtSetEnable(0xC, 0);
                        pG->Room_flg[0] |= 0x04000000;
                    }
                } else if (!(pG->Room_flg[0] & 0x02000000)) {
                    if (SceAtItemFlgCk(0x8D) == 1) {
                        SceAtSetEnable(0xC, 1);
                        pG->Room_flg[0] |= 0x02000000;
                    }
                }
            }
            SceSleep(1);
        } else {
            break;
        }
    }
    at->item.seFind = 0x5A;
    SceAtSetEnable(0x11, 0);
    SceAtSetEnable(0xC, 0);
    if (SceAtItemFlgCk(0x8D) == 1) {
        SceExit();
    }
    if (RsfCheck(G_ROOM_ID, 5)) {
        at->item.flag2 |= 0x10;
        at->item.pModel->pos.y += 10.0f;
        SceAtSetEnable(0x8D, 1);
    }
    r105_checkCesspit2();
}

// Cesspit state 2 (lid open): waits for the item to be found and moves its model onto the lid once (bit 5).
extern "C" void r105_checkCesspit2()
{
    SceAtWork* at = SceAtPtr(0x8D);

    while (1) {
        if (RsfCheck(G_ROOM_ID, 5) == 0 && SceAtItemFindFlgCk(0x8D) == 1) {
            RsfSet(G_ROOM_ID, 5);
            r105_setItemModel(at);
            break;
        }
        SceSleep(1);
    }
}

// Cesspit setup from the saved state: bit 4 clear -> cover on (hit box + state 0 task); bit 4 set, bit 3
// clear -> cover off, lid closed (open prompt, state 1); bit 3 set -> lid open (state 2).
static void r105_initCesspit()
{
    SceAtWork* at = SceAtPtr(0x8D);

    SceAtSetEnable(0x9B, 1);
    SmdGetObjPtr(0x30)->be_flag |= 0x20;
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        SceExec(0x12, r105_checkCloseCover, 0, 0, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, r105_checkCesspit0, 0, 0, SCE_PRIO_DEF_2, 0);
        SceAtSetEnable(0x11, 0);
        SceAtSetEnable(0x8D, 1);
    } else {
        SmdGetObjPtr(0x31)->be_flag &= ~2;
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            SmdGetObjPtr(0x30)->pParts->ang.z = 0.69f;
            SceAtDataSet_exec(0xC, SCE_LEVEL10, 0, r105_execOpenCover, 0, 1);
            SceAtSetEnable(9, 0);
            if (RsfCheck(G_ROOM_ID, 5)) {
                SceAtSetEnable(0x8D, 1);
                SceAtSetEnable(0x8D, 0);
                SceAtSetEnable(0x11, 1);
            } else if (SceAtItemFindFlgCk(0x8D) == 1) {
                SceAtSetEnable(0x11, 1);
                SceAtSetEnable(0x8D, 1);
            } else {
                SceAtSetEnable(0x11, 1);
            }
            SceExec(0x12, r105_checkCesspit1, 0, 0, SCE_PRIO_DEF_2, 0);
        } else {
            SmdGetObjPtr(0x30)->pParts->ang.z = -(73.0f * 0.01f);
            SceAtSetEnable(0x11, 0);
            if (SceAtItemFlgCk(0x8D) == 0) {
                SceExec(0x12, r105_checkCesspit2, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    SceSleep(1);
    if (RsfCheck(G_ROOM_ID, 5)) {
        r105_setItemModel(at);
    }
}
