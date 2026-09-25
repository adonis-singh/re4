#include "types.h"
#include "event.h"
#include "light.h"
#include "xml.h"
#include "dbg_tool.h"
#include "atari.h"
#include "global.h"
#include "joy.h"
#include "scheduler.h"
#include "file.h"
#include "est.h"
#include "sce.h"
#include "fade.h"
#include "snd.h"
#include "mes.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "view.h"
#include "db_cam.h"
#include "cockpit.h"
#include "main_sub.h"
#include "st_mgr_event.h"
#include "db_light.h"
#include "db_filelist.h"
#include "db_sctrl.h"
#include "t_util.h"
#include "t_event.h"
#include "player.h"
#include <string.h>
#include <stdio.h>

// Event tool (D:/Bio4/Prog/t_event.cpp): a task object (ToolEvt) with a file menu, the event preview
// (EventMgr::SetEvt of a host .evd file, stop / capture / message display) and the preview sub tools
// (light editor, ESP tool hand-off, fog and focus Hermite curve editors, message list editor).

void DbMenuSetExecTool(const char* name);

// cFileList::init really takes the list buffer and the host directory (the symbol keeps the
// parameterless name); XmlSimple::SetXmlElemStart/End take the element name as well.
int FileListInit(cFileList* l, char* buf, const char* dir) __asm__("init__9cFileList");
int XmlElemStart(XmlSimple* x, char** cur, char* buf, const char* name) __asm__("SetXmlElemStart__9XmlSimplePiPc");
int XmlElemEnd(XmlSimple* x, char** cur, char* buf, const char* name) __asm__("SetXmlElemEnd__9XmlSimplePiPc");

// EvtDebug's leading fields: the event name and the header copy the tool fills at load
struct EvtDebugView {
    char name[0x20];   // 0x00
    EvtHdrCopy hdr;    // 0x20
};
#define EVTDBG ((EvtDebugView*) &EvtDebug)

// cFlag-style bit numbering (from the MSB of flags) over the tool's flag word
static inline u32 FlagBit(u32 f, u32 bit) { return f & bit; }

#define CAM_MOTION_FLAGS(p) (*(u16*) ((u8*) (p) + 0x40))

#define EVT_MES_Y (336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1)

// One "Node" record of the message xml: the eleven text elements in file order.
struct XmlNode {
    char s[11][0x10];
};
enum {
    XN_SETFLG,
    XN_SETOWNER,
    XN_SETEDIT,
    XN_NAMEPAC,
    XN_CUTNO,
    XN_FRAME,
    XN_COMFLAG,
    XN_SETBIN,
    XN_SETTPL,
    XN_DAT0,
    XN_DAT1
};
struct XmlNodeData {
    XmlNode node[100];
    int num;
};

#define XML_NODE_MAX 100
#define XML_BUF_SIZE 0x30D40

// Zeroes the 100 xml Node records.
static inline void XmlNodeDataClear(XmlNodeData* d)
{
    XmlNode* n = d->node;
    int i;
    int j;

    i = XML_NODE_MAX;
    while (i--) {
        char* p = (char*) n;
        j = 11;
        while (j--) {
            memset(p, 0, 0x10);
            p += 0x10;
        }
        n++;
    }
}

// "true" / "True" / "TRUE" -> 1.
static inline int XmlStrToBool(const char* s)
{
    if (strcmp(s, "true") == 0 || strcmp(s, "True") == 0) {
        return 1;
    }
    return strcmp(s, "TRUE") == 0;
}

// Decimal string -> long.
static inline long XmlStrToLong(const char* s)
{
    long v;

    sscanf(s, "%ld", &v);
    return v;
}

// Reads the xml file into `d`: 0 when it is missing or too big.
// `size` is HDRead's result: the read itself is in the array owner (EvtMessRead), so HDRead's buffer
// argument is a fresh `addi r4,r1,N` (no pseudo equivalent to the address exists yet in that ebb).
static inline int EvtReadXml(const char* name, XmlNodeData* d, char* tmp, char* buf, u32 size)
{
    XmlSimple xml;
    char* cur;
    int i;

    buf[size] = 0;
    if (size > XML_BUF_SIZE - 1) {
        pLog->err(0, 0, "ReadXml : FileSize over [%d]", size);
        return 0;
    }
    if (size == 0) {
        pLog->err(0, 0, "ReadXml : File Not Found [%s]", name);
        return 0;
    }
    cur = buf;
    xml.GetXmlStart(&cur, cur, "Node");
    d->num = 0;
    for (i = 0; i < XML_NODE_MAX; i++) {
        XmlNode* n = &d->node[i];

        if (xml.GetXmlElem(tmp, cur, "SetFlg") == 1) {
            strcpy(n->s[XN_SETFLG], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "SetOwner") == 1) {
            strcpy(n->s[XN_SETOWNER], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "SetEdit") == 1) {
            strcpy(n->s[XN_SETEDIT], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "NamePac") == 1) {
            strcpy(n->s[XN_NAMEPAC], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "CutNo") == 1) {
            strcpy(n->s[XN_CUTNO], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "Frame") == 1) {
            strcpy(n->s[XN_FRAME], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "ComFlag") == 1) {
            strcpy(n->s[XN_COMFLAG], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "SetBin") == 1) {
            strcpy(n->s[XN_SETBIN], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "SetTpl") == 1) {
            strcpy(n->s[XN_SETTPL], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "Dat0") == 1) {
            strcpy(n->s[XN_DAT0], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "Dat1") == 1) {
            strcpy(n->s[XN_DAT1], tmp);
        }
        d->num++;
        if (xml.GetXmlNext(&cur, cur, "Node") == 0) {
            break;
        }
    }
    return 1;
}

// The message list from its xml file (path built by the caller).
static inline void EvtMessRead(EventMessageData* m, const char* path)
{
    XmlNodeData d;
    char tmp[0x80];
    char buf[XML_BUF_SIZE];
    u32 size;
    int i;

    XmlNodeDataClear(&d);
    m->num = 0;
    memset(m, 0, sizeof(m->elem));
    size = HDRead(path, buf);
    if (EvtReadXml(path, &d, tmp, buf, size) == 0) {
        pLog->err(0, 0, "ReadData : File Not Found [%s]", path);
        return;
    }
    m->num = d.num;
    for (i = 0; i < m->num; i++) {
        int on = XmlStrToBool(d.node[i].s[XN_SETFLG]);

        if (on == 1) {
            m->elem[i].be_flag = on;
            m->elem[i].No = i;
            m->elem[i].CutNo = XmlStrToLong(d.node[i].s[XN_CUTNO]);
            m->elem[i].Frame = XmlStrToLong(d.node[i].s[XN_FRAME]);
            m->elem[i].MessNo = XmlStrToLong(d.node[i].s[XN_DAT0]);
            m->elem[i].Timer = XmlStrToLong(d.node[i].s[XN_DAT1]);
        } else {
            m->elem[i].be_flag = 0;
            m->elem[i].No = 0;
            m->elem[i].CutNo = 0;
            m->elem[i].Frame = 0;
            m->elem[i].MessNo = 0;
            m->elem[i].Timer = 0;
        }
    }
}

// One-member struct: the tool pointer is a struct member (not a fixed scalar), so it is re-read after
// every store made through it (SubToolMessInit's Set*Func / callback stores reload it each time).
// Not static: the REL's `MessTool@l` fields hold 0 (a global's A only), a local's would hold S+A.
struct MessToolWork {
    cDbgToolMain<EventMessageData::MessElem>* p;
} MessTool;

// cDbgToolMain::CreateMenuWindow of this build of db_toolbase.h: the menu sits one row lower
// (Init(5, 4)) than in the Tools REL's header (Init(5, 3)).
template <class T> static inline void MessCreateMenuWindow(cDbgToolMain<T>* tool)
{
    cDbgWindow* w = new cDbgWindow;

    w->Init(5, 4, " MENU ");
    tool->pMenu = w;
    if (tool->pMenu == 0) {
        pLog->err(0, 0, "CreateMenuWindow(): new failed.");
        return;
    }
    tool->pMenu->AddButton(1, 0, "Edit  ", 0, 0, 0, 0);
    tool->pMenu->AddButton(1, 1, "Load  ", 0, 1, 0, 0);
    tool->pMenu->AddButton(1, 2, "Save  ", 0, 2, 0, 0);
    tool->pMenu->AddButton(1, 3, "Option", 0, 3, 0, 0);
    tool->pMenu->AddButton(1, 4, "Exit  ", 0, 4, 0, 0);
}

// the save / load callback setters (one tool-pointer read for the two stores of each)
template <class T> static inline void MessSetSaveFunc(cDbgToolMain<T>* tool, int (*f)(void*), void* arg)
{
    tool->pSaveFunc = f;
    tool->saveArg = arg;
}
// Installs the message tool's LOAD / SAVE callbacks (CallbackLoad / CallbackSave) in the window.
template <class T> static inline void MessSetLoadFunc(cDbgToolMain<T>* tool, int (*f)(void*), void* arg)
{
    tool->pLoadFunc = f;
    tool->loadArg = arg;
}

int IsWorkAlive(EventMessageData::MessElem* w);
void SetWorkAlive(EventMessageData::MessElem* w, int alive);
int GetWorkNo(EventMessageData::MessElem* w);
void SetWorkNo(EventMessageData::MessElem* w, int no);
void InitWork(EventMessageData::MessElem* w, int no);
int CallbackCutNoExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
void CallbackCutNoUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
int CallbackFrameExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
void CallbackFrameUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
int CallbackMessNoExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
void CallbackMessNoUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
int CallbackTimerExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
void CallbackTimerUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
int CallbackSave(void* arg);
int CallbackLoad(void* arg);

// Event tool entry (debug menu 30): constructs the ToolEvt and runs it until it exits.
void ToolEvent()
{
    ToolEvt tool;

    tool.Run();
}

// Suspends game task `task` (the event pauses).
void ToolEvt::EvtTaskSuspend(int task)
{
    if (!(EtcFlag & 0x4000)) {
        TaskSuspend(task);
        EtcFlag |= 0x4000;
    }
}

// Resumes game task `task`.
void ToolEvt::EvtTaskSignal(int task)
{
    if (EtcFlag & 0x4000) {
        TaskSignal(task);
        EtcFlag &= ~0x4000;
    }
}

// Tool start: default tool flags, pads 1/2, the host file list of the room's .evd files
// (x:\soft\room\event\evd\r<room>s??.evd), the 8 MB event buffer, the S-curve editor work and the
// message data on the Debug heap; main menu.
ToolEvt::ToolEvt()
{
    char path[0x100];

    r_no_0 = 0;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
    r_no_0_sub = 0;
    r_no_1_sub = 0;
    r_no_2_sub = 0;
    r_no_3_sub = 0;
    FFTimer = 0;
    StopTimer = 0;
    CurveNo = 0;
    CursolMain = 0;
    CursolSub = 0;
    CursolFog = 0;
    CursolFocus = 0;
    DebugCameraFlag = 0;
    DebugCameraTimer = 0;
    EtcFlag = 0;
    ListCur = 0;
    ListBase = 0;
    CaptureTimer = 0;
    PFil = 0;
    pTl = 0;
    pJoy1 = 0;
    pJoy2 = 0;
    x10C0[0] = 0;
    x10C0[1] = 0;
    x10C0[2] = 0;
    x10C0[3] = 0;
    x10C0[4] = 0;
    x10C0[5] = 0;
    x10C0[6] = 0;
    x10C0[7] = 0;
    x1108 = 0;
    EvtTaskSuspend(0);
    TutilInitDefault();
    if (pG->room_id == 0x10B) {
        EffectEspDelete(0, ESP_CORE_KIND_ROOM00, 0, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_ROOM00, 0);
        EffectEfmDelete(0, ESP_CORE_KIND_ROOM00, 0);
        EffectEspDelete(0, ESP_CORE_KIND_ROOM01, 0, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_ROOM01, 0);
        EffectEfmDelete(0, ESP_CORE_KIND_ROOM01, 0);
    }
    EvtMgr.ToolCoreEvdDel();
    sprintf(path, "%sr%x%02xs??.evd", "x:\\soft\\room\\event\\evd\\", pG->stage_no, pG->room_no);
    if (FileListInit(&DbgFileList, path, "x:\\soft\\room\\event\\evd\\") == 0) {
        EtcFlag |= TefBit(TefExit);
    }
    DbgFlagOn(pG, DBG_EVENT_TOOL);
    PFil = Debug_alloc(8000000, 1);
    memclr_asm(PFil, 4);
    PDatDbSctrl = (DbSctrlWork*) Debug_alloc(1000000, 1);
    memclr_asm(PDatDbSctrl, 1000000);
    PMesDat = (EventMessageData*) Debug_alloc(1000000, 1);
    memclr_asm(PMesDat, 1000000);
    CursolMain = 0;
    pJoy1 = &Joy[0];
    pJoy2 = &Joy[1];
    CursolSub = 0;
    CursolFog = 0;
    CursolFocus = 0;
    Cckpt.endCountDownTimer();
    Cckpt.transCountDownTimer(0);
    LightMgr.roomLitSet(0);
    LightMgr.update(0, -1);
    pTl = new cLightTool;
}

// Frees the buffers and restores the flags.
ToolEvt::~ToolEvt()
{
    delete pTl;
    DbgFlagOff(pG, DBG_EVENT_TOOL);
    pPL->endEvent(0);
    EvtTaskSignal(0);
    TutilQuitDefault();
    TaskExit();
}

static void (*runTbl[3])(ToolEvt*) = {ToolEvt::MainMenu, ToolEvt::MainPreview, ToolEvt::MainExit};

// One frame: runTbl[r_no_0] (MainMenu / MainPreview / MainExit).
void ToolEvt::Run()
{
    while (!(EtcFlag & TefBit(TefExit))) {
        runTbl[r_no_0](this);
        TaskSleep(1);
    }
}

// Event paused (StatusFlag bit 31, game task suspended): the stick left/right or X/Y step it: X
// runs one cut back, Y one cut forward (RunTool), stick right plays one frame, holding the stick
// auto-repeats every 10 frames (FFTimer); otherwise it stays stopped.
void ToolEvt::RunStop(ToolEvt* t, Event* ev)
{
    int i;

    ev->StatusFlag |= EvtStfBit(EvtStfToolStop);
    EvtTaskSuspend(0);
    if ((t->pJoy1->on & 0x30000) || (t->pJoy1->trg & 0xC00)) {
        int flg;

        if (!(t->pJoy1->on & 0x10)) {
            // COMPILER-DIFF: candidate #12 (fallthrough-arm form): the original stores a fresh `li r0,0`; a
            // literal 0 here is related by cse (record_jump_equiv on the not-taken `bne`) to the `andi.`
            // result and that register is stored instead (t_mv mvInit). `(t & 8) >> 4` is a zero cse cannot
            // fold; combine folds it (nonzero_bits) to a fresh constant after cse2.
            t->FFTimer = ((u32) t & 0x8) >> 4;
        }
        flg = 0;
        if (--t->FFTimer <= 0) {
            t->FFTimer = 10;
            flg = 1;
        }
        if (flg == 0 && !(t->pJoy1->trg & 0xC00)) {
            return;
        }
        FadeKill(0);
        EvtTaskSignal(0);
        ev->DebugDisp();
        if (t->pJoy1->trg & 0x400) {
            cMes.Clear();
            ev->RunTool(2, 0);
        } else if (t->pJoy1->trg & 0x800) {
            cMes.Clear();
            ev->RunTool(1, 0);
        } else if (t->pJoy1->on & 0x10000) {
            DbgFlagOn(pG, DBG_NO_EST_CALL);
            ev->RunTool(0, 2);
        } else if (ev->Run() == 0) {
            pLog->err(0, 0, "EventMgr::Run : failed");
        }
    } else {
        t->FFTimer = 0;
    }
}

static TOOL_MENU mainMenu[2] = {
    {1, "PREVIEW", 0},
    {1, "TOOL EXIT", 0},
};

// r_no_0 0: PREVIEW / TOOL EXIT -> r_no_0 1 / 2.
void ToolEvt::MainMenu(ToolEvt* t)
{
    int sel;
    int zero = 0;

    eprintf(0x38, 0x30, 5, 0, "MENU");
    t->EtcFlag &= ~TefBit(TefStop);
    sel = ToolMenuDisp_cur(0x40, 0x40, 1, &t->CursolMain, mainMenu, sizeof(mainMenu), t->pJoy1);
    if (sel != -1) {
        t->r_no_0 = sel + 1;
        t->r_no_1 = zero;
        t->r_no_2 = zero;
        t->r_no_3 = zero;
    }
}

static TOOL_MENU previewMenu[3] = {
    {1, "YES", 0},
    {1, "NO", 0},
    {1, "CONVERT AND LOAD", 0},
};

static void (*subRunTbl[3])(ToolEvt*, Event*) = {ToolEvt::SubMenuMain, ToolEvt::SubMenuFog, ToolEvt::SubMenuFocus};

// r_no_0 1, the preview. r_no_1 0 picks an .evd from the host list (A), 1 "DATA LOAD OK?" YES / NO
// / CONVERT AND LOAD reads it, 2 starts the event (EvtMgr.SetEvt) and inits the fog / focus
// curves, 3 runs it: sub tools (light, camera, fog, focus, message) take the pads, START / stick
// stop the event (RunStop), B leaves (or ends the capture), CAPTURE writes screenshots per frame
// to D:/bio4/Room/Sc_shot; 4 the PREVIEW MENU (SubMenuMain / Fog / Focus by r_no_0_sub).
void ToolEvt::MainPreview(ToolEvt* t)
{
    char path[0x140];

    switch (t->r_no_1) {
    case 0:
        strcpy(t->ToolFileName, DbgFileList.disp(0x40, 0x30, 0x14));
        if (t->pJoy1->trg & 0x100) {
            strcpy(EVTDBG->name, t->ToolFileName);
            t->r_no_1++;
        }
        if (t->pJoy1->trg & 0x200) {
            t->r_no_0 = 0;
            t->r_no_1 = 0;
            t->r_no_2 = 0;
            t->r_no_3 = 0;
        }
        break;
    case 1:
        memclr_asm(t->roomNo, 0x10);
        memclr_asm(t->eventNo, 0x10);
        sscanf(t->ToolFileName, "%c%c%c%c%c%c%c.evd", &t->roomNo[0], &t->roomNo[1], &t->roomNo[2], &t->roomNo[3], &t->eventNo[0],
               &t->eventNo[1], &t->eventNo[2]);
        eprintf(0x38, 0x30, 5, 0, "DATA LOAD OK?");
        switch (ToolMenuDisp(0x40, 0x40, 1, previewMenu, sizeof(previewMenu), t->pJoy1)) {
        case 0:
            sprintf(path, "%s/%s", "x:/soft/room/event/evd", EvtMgr.NameChange(t->ToolFileName));
            HDRead(path, t->PFil);
            t->r_no_1++;
            break;
        case 2:
            sprintf(path, "%s/%s", "x:/soft/room/event/evd", EvtMgr.NameChange(t->ToolFileName));
            HDRead(path, t->PFil);
            t->r_no_1++;
            break;
        default:
            if (!(t->pJoy1->trg & 0x200)) {
                break;
            }
        case 1:
            t->r_no_0 = 0;
            t->r_no_1 = 0;
            t->r_no_2 = 0;
            t->r_no_3 = 0;
            break;
        }
        break;
    case 2: {
        Event* ev;

        t->EvtTaskSignal(0);
        SceEventStart(0);
        EvtHdrCopy* h = (EvtHdrCopy*) t->PFil;

        t->hdr = *h;
        EvtDebugView* d = EVTDBG;

        d->hdr = *h;
        if (EvtMgr.SetEvt(t->PFil, (u32*) &ev) == 0) {
            pLog->err(0, 0, "ToolEvt_Main_Preview : failed");
            t->r_no_0 = 1;
            t->r_no_1 = 4;
            t->r_no_2 = 0;
            t->r_no_3 = 0;
            break;
        }
        t->EtcFlag &= ~TefBit(TefPrevSubMenu);
        t->SubToolFogWkInit(t, ev);
        t->SubToolFocusWkInit(t, ev);
        t->StopTimer = 1;
        ev->StatusFlag |= EvtStfBit(EvtStfStartWait);
        t->r_no_1++;
        break;
    }
    case 3: {
        Event* ev;

        if (SysFlagChk(pG, SYS_SCREEN_STOP)) {
            SysFlagOff(pG, SYS_SCREEN_STOP);
        }
        if (EvtMgr.GetEvt(EvtMgr.GetNowExeEvtNamePtr(), (void**) &ev) == 0) {
            pLog->err(0, 0, "ToolEvt_Main_Preview : failed");
            t->r_no_0 = 1;
            t->r_no_1 = 4;
            t->r_no_2 = 0;
            t->r_no_3 = 0;
            break;
        }
        if (t->EtcFlag & TefBit(TefToolLight)) {
            t->SubToolLightMove(t);
        } else if (t->SubToolCameraMove(t) != 0) {
            break;
        }
        if (t->EtcFlag & TefBit(TefToolFog)) {
            t->SubToolFogMove(t, ev);
        }
        if (t->EtcFlag & TefBit(TefToolFocus)) {
            t->SubToolFocusMove(t, ev);
        }
        if (t->pJoy2->trg & 0x400) {
            pG->debug_mode = 0;
        }
        if (t->pJoy2->trg & 0x800) {
            pG->debug_mode = 1;
        }
        if (t->EtcFlag & TefBit(TefStop)) {
            eprintf(0x1D0, 0x10, 0x16, 0, "STOP");
        }
        if (t->EtcFlag & TefBit(TefPrevSubMenu)) {
            subRunTbl[t->r_no_0_sub](t, ev);
            break;
        }
        if (t->pJoy1->trg & 0x200) {
            t->EtcFlag |= TefBit(TefPrevSubMenu);
        }
        if (t->EtcFlag & TefBit(TefCaptureReq)) {
            if (++t->CaptureTimer > 1) {
                t->EtcFlag &= ~TefBit(TefCaptureReq);
                t->EtcFlag |= TefBit(TefCaptureRun);
                if (t->EtcFlag & TefBit(TefCaptureFullSize)) {
                    ScreenShotStart("D:/bio4/Room/Sc_shot/r100", 0, 0);
                } else {
                    ScreenShotStart("D:/bio4/Room/Sc_shot/r100", 0, 1);
                }
            }
        }
        if (t->EtcFlag & TefBit(TefCaptureRun)) {
            if (t->EtcFlag & TefBit(TefCaptureEnd)) {
                if (++t->CaptureTimer > 1) {
                    t->EtcFlag &= ~TefBit(TefCaptureEnd);
                    t->EtcFlag |= TefBit(TefPrevSubMenu);
                    if (t->EtcFlag & TefBit(TefCaptureRun)) {
                        u32* fp = &t->EtcFlag;

                        *fp &= ~0x00400000;
                        pG->debug_mode = 1;
                        ScreenShotEnd();
                    }
                }
            }
            if ((ev->NowCut >= ev->MaxCut && (t->EtcFlag & TefBit(TefCaptureRun))) || (t->pJoy1->trg & 0x200)) {
                if (!(t->EtcFlag & TefBit(TefCaptureEnd))) {
                    t->EtcFlag |= TefBit(TefCaptureEnd);
                    t->CaptureTimer = 0;
                }
            }
        }
        ev->DebugDispTool();
        if (t->StopTimer != 0) {
            if (--t->StopTimer <= 0) {
                t->StopTimer = 0;
                t->EtcFlag |= TefBit(TefStop);
                ev->StatusFlag |= EvtStfBit(EvtStfToolExec);
            }
        }
        if ((!(t->EtcFlag & TefBit(TefStop)) && ((t->pJoy1->on & 0x30000) || (t->pJoy1->trg & 0xE00))) ||
            FlagBit(t->EtcFlag, TefBit(TefStopOnReq)) || FlagBit(t->EtcFlag, TefBit(TefStopOffReq)) || (t->pJoy1->trg & 0x100)) {
            t->EtcFlag ^= TefBit(TefStop);
            if (t->EtcFlag & TefBit(TefStopOnReq)) {
                t->EtcFlag |= TefBit(TefStop);
            }
            if (t->EtcFlag & TefBit(TefStopOffReq)) {
                t->EtcFlag &= ~TefBit(TefStop);
            }
            t->FFTimer = 0;
            t->EtcFlag &= ~(TefBit(TefStopOnReq) | TefBit(TefStopOffReq));
            if (t->EtcFlag & TefBit(TefStop)) {
                t->EvtTaskSuspend(0);
                EventMgr* m = &EvtMgr;
                char* pp = m->GetNowExeEvtNamePtr();

                m->EvtSndStrStop(pp, 1, 0);
                m->EvtSndStrStop(pp, 0, 0);
            } else {
                t->EvtTaskSignal(0);
                if (!FlagBit(t->EtcFlag, TefBit(TefCaptureRun)) && !FlagBit(t->EtcFlag, TefBit(TefCaptureReq))) {
                    if (!ev->FlgCkStatus(EvtStfStartWait)) {
                        ev->StatusFlag |= EvtStfBit(EvtStfStrTime);
                        EvtDebug.SetStfStrTimer(60);
                        SndAllStop();
                    }
                }
            }
            ev->StatusFlag &= ~EvtStfBit(EvtStfStartWait);
        }
        {
            u32* sp = &ev->StatusFlag;

            *sp &= ~0x80000000;
            sp = &ev->StatusFlag;
            *sp &= ~0x40000000;
        }
        DbgFlagOff(pG, DBG_NO_EST_CALL);
        if (t->EtcFlag & TefBit(TefStop)) {
            t->RunStop(t, ev);
        }
        if (t->EtcFlag & 0x8000) {
            t->SubToolMessMove(t, ev);
        }
        break;
    }
    case 4:
        t->EvtTaskSignal(0);
        TaskSleep(2);
        t->EvtTaskSuspend(0);
        EventMgr* m = &EvtMgr;
        char* pp = m->GetNowExeEvtNamePtr();

        m->EvtSndStrStop(pp, 1, 1);
        m->EvtSndStrStop(pp, 0, 1);
        SceEventEnd(0);
        if (!(t->EtcFlag & TefBit(TefRemainEnd))) {
            t->r_no_0 = 0;
            t->r_no_1 = 0;
            t->r_no_2 = 0;
            t->r_no_3 = 0;
        } else {
            t->EtcFlag |= TefBit(TefExit);
        }
        break;
    }
}

static TOOL_MENU yesNoMenu[2] = {
    {1, "YES", 0},
    {1, "NO", 0},
};

// r_no_0 2: EXIT YES/NO, then the tool ends.
void ToolEvt::MainExit(ToolEvt* t)
{
    eprintf(0x38, 0x30, 5, 0, "EXIT OK?");
    switch (ToolMenuDisp(0x40, 0x40, 3, yesNoMenu, sizeof(yesNoMenu), t->pJoy1)) {
    case 0:
        t->EtcFlag |= TefBit(TefExit);
    default:
        if (!(t->pJoy1->trg & 0x200)) {
            break;
        }
    case 1:
        t->r_no_0 = 0;
        t->r_no_1 = 0;
        t->r_no_2 = 0;
        t->r_no_3 = 0;
        break;
    }
}

// Ends the previewed event: EvtMgr deletes it, messages cleared, fade killed.
void ToolEvt::EventDel(Event* ev)
{
    EvtTaskSignal(0);
    ev->StatusFlag &= ~EvtStfBit(EvtStfToolExec);
    ev->StatusFlag |= EvtStfBit(EvtStfNoFunc);
    ev->RunEvtCancel();
    EvtMgr.DelEvt(ev, 0);
    ev->StatusFlag &= ~EvtStfBit(EvtStfNoFunc);
}

static TOOL_MENU subMainMenu[8] = {
    {1, "CONTINUE", 0},
    {1, "LIGHT TOOL", 0},
    {1, "ESP   TOOL", 0},
    {1, "FOG   TOOL", 0},
    {1, "FOCUS TOOL", 0},
    {1, "MESS  TOOL", 0},
    {1, "CAPTURE", 0},
    {1, "PREVIEW EXIT", 0},
};

// PREVIEW MENU: CONTINUE, LIGHT TOOL (embedded db_light on pad 3), ESP TOOL (hands over to the
// effect editor module), FOG TOOL / FOCUS TOOL (their menus), MESS TOOL (message list editor),
// CAPTURE (frame capture on), PREVIEW EXIT.
void ToolEvt::SubMenuMain(ToolEvt* t, Event* ev)
{
    eprintf(0x38, 0x30, 5, 0, "PREVIEW MENU");
    switch (ToolMenuDisp_cur(0x40, 0x40, 1, &t->CursolSub, subMainMenu, sizeof(subMainMenu), t->pJoy1)) {
    case 0:
        t->EtcFlag &= ~TefBit(TefPrevSubMenu);
        break;
    case 1:
        if (!(t->EtcFlag & TefBit(TefToolLight))) {
            t->SubToolLightInit(t, 1);
        } else {
            t->SubToolLightInit(t, 0);
        }
        break;
    case 2:
        ev->EspToolSetDat();
        t->EtcFlag |= TefBit(TefRemainEnd);
        EvtDebug.FlagOnEtc(FlagEvent2Esp);
        DbMenuSetExecTool("ESP TOOL");
        t->EventDel(ev);
        t->r_no_1 = 4;
        break;
    case 3:
        t->r_no_0_sub = 1;
        t->r_no_1_sub = 0;
        t->r_no_2_sub = 0;
        t->r_no_3_sub = 0;
        t->CursolFog = 0;
        break;
    case 4:
        t->r_no_0_sub = 2;
        t->r_no_1_sub = 0;
        t->r_no_2_sub = 0;
        t->r_no_3_sub = 0;
        t->CursolFocus = 0;
        break;
    case 5:
        if (!(t->EtcFlag & 0x8000)) {
            t->SubToolMessInit(t, 1);
        } else {
            t->SubToolMessInit(t, 0);
        }
        break;
    case 6:
        t->CaptureTimer = 0;
        t->EtcFlag |= TefBit(TefStopOffReq) | TefBit(TefCaptureReq);
        t->EtcFlag &= ~TefBit(TefPrevSubMenu);
        t->EtcFlag &= ~TefBit(TefCaptureFullSize);
        if (t->pJoy1->on & 0x10) {
            t->EtcFlag |= TefBit(TefCaptureFullSize);
        } else {
            pG->debug_mode = 0;
        }
        break;
    case 7:
        t->EventDel(ev);
        t->r_no_1 = 4;
        break;
    }
}

static TOOL_MENU fogMenu[6] = {
    {1, "EDIT START", 0},
    {1, "EDIT END", 0},
    {1, "INIT", 0},
    {1, "LOAD", 0},
    {1, "SAVE", 0},
    {1, "FOG TOOL END", 0},
};

// FOG TOOL MENU of the current cut (<event>_<cut>.fog): EDIT START / EDIT END (the S-curve editor
// on the fog start / end curves), INIT, LOAD, SAVE (host x:/soft/room/event/...), FOG TOOL END.
void ToolEvt::SubMenuFog(ToolEvt* t, Event* ev)
{
    char dir[0x100];
    char path[0x100];
    char name[0x100];

    strcpy(dir, "x:/soft/room/event");
    sprintf(path, "%s/%s/%s/etc/%s_%03d.fog", dir, t->roomNo, t->eventNo, t->eventNo, ev->NowCut);
    sprintf(name, "[%s_%03d.fog]", t->eventNo, ev->NowCut);
    ev->FogMove(ev, &t->DatFogWk);
    eprintf(0x38, 0x30, 5, 0, "FOG TOOL MENU");
    switch (ToolMenuDisp_cur(0x40, 0x40, 1, &t->CursolFog, fogMenu, sizeof(fogMenu), t->pJoy1)) {
    case 0:
        if (!(t->EtcFlag & TefBit(TefToolFog))) {
            t->SubToolFogInit(t, 1, ev, 0);
        } else {
            t->SubToolFogInit(t, 0, 0, 0);
        }
        break;
    case 1:
        if (!(t->EtcFlag & TefBit(TefToolFog))) {
            t->SubToolFogInit(t, 1, ev, 1);
        } else {
            t->SubToolFogInit(t, 0, 0, 0);
        }
        break;
    case 2:
        if (t->SubMenuSelectYesNo(t, "INIT", "")) {
            memset(&t->DatFogWk, 0, sizeof(EvtFogData));
        }
        t->SubToolFogWkInit(t, ev);
        break;
    case 3:
        if (t->SubMenuSelectYesNo(t, "LOAD", name)) {
            HDRead(path, &t->DatFogWk);
        }
        break;
    case 4:
        if (t->SubMenuSelectYesNo(t, "SAVE", name)) {
            HDWrite(path, &t->DatFogWk, sizeof(EvtFogData));
        }
        break;
    case 5:
        if (t->SubMenuSelectYesNo(t, "EXIT", "")) {
            t->r_no_0_sub = 0;
        }
        break;
    }
}

static TOOL_MENU focusMenu[8] = {
    {1, "EDIT NEAR", 0},
    {1, "EDIT FAR", 0},
    {1, "LEVEL NEAR", 0},
    {1, "LEVEL FAR", 0},
    {1, "INIT", 0},
    {1, "LOAD", 0},
    {1, "SAVE", 0},
    {1, "FOCUS TOOL END", 0},
};

// FOCUS TOOL MENU of the current cut (<event>_<cut>.fcs): EDIT NEAR / FAR (S-curve editor on the
// focus distance curves), LEVEL NEAR / FAR (blur levels), INIT, LOAD, SAVE, FOCUS TOOL END.
void ToolEvt::SubMenuFocus(ToolEvt* t, Event* ev)
{
    char dir[0x100];
    char path[0x100];
    char name[0x100];

    strcpy(dir, "x:/soft/room/event");
    sprintf(path, "%s/%s/%s/etc/%s_%03d.fcs", dir, t->roomNo, t->eventNo, t->eventNo, ev->NowCut);
    sprintf(name, "[%s_%03d.fcs]", t->eventNo, ev->NowCut);
    ev->FocusMove(ev, &t->DatFocusWk);
    eprintf(0x38, 0x30, 5, 0, "FOCUS TOOL MENU");
    switch (ToolMenuDisp_cur(0x40, 0x40, 1, &t->CursolFocus, focusMenu, sizeof(focusMenu), t->pJoy1)) {
    case 0:
        if (!(t->EtcFlag & TefBit(TefToolFocus))) {
            t->SubToolFocusInit(t, 1, ev, 0);
        } else {
            t->SubToolFocusInit(t, 0, 0, 0);
        }
        break;
    case 1:
        if (!(t->EtcFlag & TefBit(TefToolFocus))) {
            t->SubToolFocusInit(t, 1, ev, 1);
        } else {
            t->SubToolFocusInit(t, 0, 0, 0);
        }
        break;
    case 2:
        t->SubMenuEditFocusLevel(t, ev, "NEAR", &t->DatFocusWk.nearLevel);
        break;
    case 3:
        t->SubMenuEditFocusLevel(t, ev, "FAR ", &t->DatFocusWk.farLevel);
        break;
    case 4:
        if (t->SubMenuSelectYesNo(t, "INIT", "")) {
            memset(&t->DatFocusWk, 0, sizeof(EvtFocusData));
        }
        t->SubToolFocusWkInit(t, ev);
        break;
    case 5:
        if (t->SubMenuSelectYesNo(t, "LOAD", name)) {
            HDRead(path, &t->DatFocusWk);
        }
        break;
    case 6:
        if (t->SubMenuSelectYesNo(t, "SAVE", name)) {
            HDWrite(path, &t->DatFocusWk, sizeof(EvtFocusData));
        }
        break;
    case 7:
        if (t->SubMenuSelectYesNo(t, "EXIT", "")) {
            t->r_no_0_sub = 0;
        }
        break;
    }
}

static TOOL_MENU yesNoMenu2[2] = {
    {1, "YES", 0},
    {1, "NO", 0},
};

// "<s1> <s2> OK?" YES/NO; 1 on YES (the caller keeps calling until the answer).
int ToolEvt::SubMenuSelectYesNo(ToolEvt* t, const char* s1, const char* s2)
{
    TaskSleep(1);
    for (;;) {
        eprintf(0x38, 0x30, 5, 0, "%s %s OK?", s1, s2);
        switch (ToolMenuDisp(0x40, 0x40, 3, yesNoMenu2, sizeof(yesNoMenu2), t->pJoy1)) {
        case 0:
            return 1;
        case 1:
            return 0;
        }
        TaskSleep(1);
    }
}

// EDIT FOCUS LEVEL [NEAR/FAR]: up/down +-1, left/right +-0.1 (clamped at 0); 1 on A/B (done).
int ToolEvt::SubMenuEditFocusLevel(ToolEvt* t, Event* ev, const char* name, f32* level)
{
    TaskSleep(1);
    while (1) {
        eprintf(0x38, 0x30, 0x16, 0, "EDIT FOCUS LEVEL [%s] : %f", name, *level);
        if (Joy[0].rep & 0x10000) {
            *level -= 1.0f;
        }
        if (Joy[0].rep & 0x20000) {
            *level += 1.0f;
        }
        if (Joy[0].rep & 0x1) {
            *level -= 0.1f;
        }
        if (Joy[0].rep & 0x2) {
            *level += 0.1f;
        }
        if (*level < 0.0f) {
            *level = 0.0f;
        }
        if (*level > 10.0f) {
            *level = 10.0f;
        }
        if (FlagBit(t->pJoy1->trg, 0x100) || FlagBit(t->pJoy1->trg, 0x200)) {
            break;
        }
        ev->FocusMove(ev, &t->DatFocusWk);
        TaskSleep(1);
    }
    return 0;
}

// START toggles the debug camera on pad 1 (CamDbg); returns 1 while the camera has the pad (B
// releases it).
int ToolEvt::SubToolCameraMove(ToolEvt* /*t*/)
{
    if (DebugCameraFlag != 0) {
        if (pJoy1->trg & 0x1000) {
            DebugCameraFlag = 0;
            if (!DbgFlagChk(pG, DBG_DBG_CAM)) {
                SpfFlagOff(pG, SPF_CAMERA);
            }
        } else {
            CamDbg.move(&pG->Camera, &Joy[0], 0);
            if (DebugCameraTimer++ & 8) {
                eprintf2(0xE, 0x12, 0xAA, 0x18, 6, 0, "CAMERA MODE");
            }
            if (pJoy1->trg & 0x200) {
                CAM_MOTION_FLAGS(CamCtrl.getMotionInfoPtr()) |= 8;
                SpfFlagOff(pG, SPF_CAMERA);
            } else {
                CAM_MOTION_FLAGS(CamCtrl.getMotionInfoPtr()) &= ~8;
                SpfFlagOn(pG, SPF_CAMERA);
            }
            CameraMove();
        }
        return 1;
    }
    if (pJoy1->trg & 0x1000) {
        DebugCameraFlag = 1;
        EtcFlag |= TefBit(TefStopOnReq);
    }
    return 0;
}

// Creates (sw 1) or deletes the embedded cLightTool; the tool then reads pads 3/4 (SubToolIn).
void ToolEvt::SubToolLightInit(ToolEvt* t, int sw)
{
    int i;

    if (sw == 1) {
        cMes.Clear();
        EvtDebug.FlagOnEtc(FlagLightTool);
        DbgFlagOn(pG, DBG_BACK_CLIP);
    } else {
        EvtDebug.FlagOffEtc(FlagLightTool);
        SpfFlagOff(pG, SPF_CAMERA);
        DbgFlagOff(pG, DBG_BACK_CLIP);
        TaskSleep(1);
    }
    SubToolIn(t, sw, TefToolLight);
}

// Runs the embedded light editor; closes it when it quits.
void ToolEvt::SubToolLightMove(ToolEvt* /*t*/)
{
    cLightTool* lt = pTl;

    if ((u32) lt >= 0x80000000 && (u32) lt <= 0x82FFFFFF) {
        if (lt->move() == 0) {
            SubToolLightInit(this, 0);
        }
        View.move();
    }
}

// Default fog curves: start / end constant at the current LightMgr fog over the event length.
int ToolEvt::SubToolFogWkInit(ToolEvt* t, Event* ev)
{
    t->DatFogWk.start.num = 2;
    t->DatFogWk.start.key[0].t = 0.0f;
    t->DatFogWk.start.key[0].v = LightMgr.getFogStart();
    t->DatFogWk.start.key[0].out = 0.0f;
    t->DatFogWk.start.key[0].in = 0.0f;
    t->DatFogWk.start.key[1].t = (f32) ev->MaxFrame;
    t->DatFogWk.start.key[1].v = LightMgr.getFogStart();
    t->DatFogWk.start.key[1].out = 0.0f;
    t->DatFogWk.start.key[1].in = 0.0f;
    t->DatFogWk.end.num = 2;
    t->DatFogWk.end.key[0].t = 0.0f;
    t->DatFogWk.end.key[0].v = LightMgr.getFogEnd();
    t->DatFogWk.end.key[0].out = 0.0f;
    t->DatFogWk.end.key[0].in = 0.0f;
    t->DatFogWk.end.key[1].t = (f32) ev->MaxFrame;
    t->DatFogWk.end.key[1].v = LightMgr.getFogEnd();
    t->DatFogWk.end.key[1].out = 0.0f;
    t->DatFogWk.end.key[1].in = 0.0f;
    return 1;
}

// Opens (sw 1) the S-curve editor on the fog start (which 0) or end (1) curve, x = frames, y up
// to 100000; or closes it.
void ToolEvt::SubToolFogInit(ToolEvt* t, int sw, Event* ev, int which)
{
    if (sw == 1) {
        EvtDebug.FlagOnEtc(FlagFogTool);
        if (ev->NowCut > 99) {
            return;
        }
        if (which == 0) {
            t->SctrlToolInit(t, (Hermite1*) &t->DatFogWk.start, (f32) ev->MaxFrame, 100000.0f);
        } else {
            t->SctrlToolInit(t, (Hermite1*) &t->DatFogWk.end, (f32) ev->MaxFrame, 100000.0f);
        }
        t->CurveNo = which;
    } else {
        EvtDebug.FlagOffEtc(FlagFogTool);
        TaskSleep(1);
    }
    SubToolIn(t, sw, TefToolFog);
}

// Runs the fog curve editor; when it quits, back to the fog menu.
void ToolEvt::SubToolFogMove(ToolEvt* t, Event* ev)
{
    if (DbSctrl(t->PDatDbSctrl, 0x20, 0x20) == 0) {
        SubToolFogInit(t, 0, 0, 0);
    }
    if (t->CurveNo == 0) {
        eprintf(0x38, 0x30, 0x16, 0, "FOG START");
    } else {
        eprintf(0x38, 0x30, 0x16, 0, "FOG END");
    }
    ev->FogMove(ev, &t->DatFogWk);
}

// Default focus curves: near 0 / far 10000 constant over the event length.
void ToolEvt::SubToolFocusWkInit(ToolEvt* t, Event* ev)
{
    t->DatFocusWk.near_.num = 2;
    t->DatFocusWk.near_.key[0].t = 0.0f;
    t->DatFocusWk.near_.key[0].v = 0.0f;
    t->DatFocusWk.near_.key[0].out = 0.0f;
    t->DatFocusWk.near_.key[0].in = 0.0f;
    t->DatFocusWk.near_.key[1].t = (f32) ev->MaxFrame;
    t->DatFocusWk.near_.key[1].v = 0.0f;
    t->DatFocusWk.near_.key[1].out = 0.0f;
    t->DatFocusWk.near_.key[1].in = 0.0f;
    t->DatFocusWk.far_.num = 2;
    t->DatFocusWk.far_.key[0].t = 0.0f;
    t->DatFocusWk.far_.key[0].v = 10000.0f;
    t->DatFocusWk.far_.key[0].out = 0.0f;
    t->DatFocusWk.far_.key[0].in = 0.0f;
    t->DatFocusWk.far_.key[1].t = (f32) ev->MaxFrame;
    t->DatFocusWk.far_.key[1].v = 10000.0f;
    t->DatFocusWk.far_.key[1].out = 0.0f;
    t->DatFocusWk.far_.key[1].in = 0.0f;
    t->DatFocusWk.nearLevel = 5.0f;
    t->DatFocusWk.farLevel = 5.0f;
}

// Opens the S-curve editor on the focus near (which 0) / far (1) curve, or closes it.
void ToolEvt::SubToolFocusInit(ToolEvt* t, int sw, Event* ev, int which)
{
    if (sw == 1) {
        EvtDebug.FlagOnEtc(FlagFocusTool);
        if (ev->NowCut > 99) {
            return;
        }
        if (which == 0) {
            t->SctrlToolInit(t, (Hermite1*) &t->DatFocusWk.near_, (f32) ev->MaxFrame, 10000.0f);
        } else {
            t->SctrlToolInit(t, (Hermite1*) &t->DatFocusWk.far_, (f32) ev->MaxFrame, 10000.0f);
        }
        t->CurveNo = which;
    } else {
        EvtDebug.FlagOffEtc(FlagFocusTool);
        TaskSleep(1);
    }
    SubToolIn(t, sw, TefToolFocus);
}

// Runs the focus curve editor; when it quits, back to the focus menu.
void ToolEvt::SubToolFocusMove(ToolEvt* t, Event* ev)
{
    if (DbSctrl(t->PDatDbSctrl, 0x20, 0x20) == 0) {
        SubToolFocusInit(t, 0, 0, 0);
    }
    if (t->CurveNo == 0) {
        eprintf(0x38, 0x30, 0x16, 0, "FOCUS NEAR");
    } else {
        eprintf(0x38, 0x30, 0x16, 0, "FOCUS FAR");
    }
    ev->FocusMove(ev, &t->DatFocusWk);
}

// Opens (sw 1) the message list editor: a cDbgToolMain over EventMessageData::elem (columns CutNo /
// Frame / MessNo / Timer) with LOAD / SAVE of evt_<room><event>_mes.xml; or closes it.
void ToolEvt::SubToolMessInit(ToolEvt* t, int sw)
{
    EventMessageData* m = t->PMesDat;
    int i;

    if (sw == 1) {
        char path[0x80];

        EvtDebug.FlagOnEtc(FlagMessTool);
        cMes.Clear();
        sprintf(path, "%s/evt_%s%s_mes.xml", "x:/soft/room/event/evd", t->roomNo, t->eventNo);
        // COMPILER-DIFF: candidate (gcse table size): the edit-window ctor anchor (dbg_tool.h) is one
        // more insn at gcse entry (1219 -> 1220), which turns the expression hash table from 609 to
        // 611 buckets and wraps `t->room`/`t->no` (raw hashes 17713/17729 -> buckets 605/10) and the
        // clear loop's `i - 1`/`p + 0xb0` (13400/13574) into the wrong PRE numbering = the wrong
        // allocno order on their priority ties. 17 insns nobody ever executes (set, cmpwi, branch,
        // 7 x mulli+addi) move it to 619 buckets, where both pairs are in the original order. cse1
        // cannot see `z` past the LOOP_END note, cse2 folds the test, the arm is unreachable and the
        // set dead before sched1: no code, no live-length change.
        {
            int z = 0;
            do { } while (0);
            if (z > 128) { z = z * 77 + 1; z = z * 78 + 2; z = z * 79 + 3; z = z * 80 + 4; z = z * 81 + 5; z = z * 82 + 6; z = z * 83 + 7; }
        }
        EvtMessRead(m, path);
        MessTool.p = new cDbgToolMain<EventMessageData::MessElem>;
        MessCreateMenuWindow(MessTool.p);
        MessTool.p->CreateFileWindows(0x16, 0xA, "X:\\Soft\\Room\\event\\", "test", ".txt");
        MessSetSaveFunc(MessTool.p, CallbackSave, t);
        MessSetLoadFunc(MessTool.p, CallbackLoad, t);
        // COMPILER-DIFF: candidate #5 (sched1 tie): the original issues `loadArg = t` before `pLoadFunc =
        // CallbackLoad` (both prio 13, LUID order ours); a codeless anchor keeps the CallbackLoad address
        // alive past its store so the store no longer kills a register (INSN_REG_WEIGHT 0 vs -1).
        asm("" : "=m"(path[0]) : "r"(CallbackLoad));
        MessTool.p->CreateEditWindow(0xA, 4, m->elem, "No  ==CutNo== ==Frame== ==MessNo= ==Timer==", 5, XML_NODE_MAX);
        MessTool.p->AddEditColumn(4, "         ", 1, CallbackCutNoExec, CallbackCutNoUpdate);
        MessTool.p->AddEditColumn(0xE, "         ", 2, CallbackFrameExec, CallbackFrameUpdate);
        MessTool.p->AddEditColumn(0x18, "         ", 3, CallbackMessNoExec, CallbackMessNoUpdate);
        MessTool.p->AddEditColumn(0x22, "         ", 4, CallbackTimerExec, CallbackTimerUpdate);
        MessTool.p->SetIsWorkAliveFunc(IsWorkAlive);
        MessTool.p->SetSetWorkAliveFunc(SetWorkAlive);
        MessTool.p->SetGetWorkNoFunc(GetWorkNo);
        MessTool.p->SetSetWorkNoFunc(SetWorkNo);
        MessTool.p->SetInitWorkFunc(InitWork);
        MessTool.p->InitAllWork();
        CallbackLoad(t);
    } else {
        EvtDebug.FlagOffEtc(FlagMessTool);
        cMes.Clear();
        if (MessTool.p) {
            delete MessTool.p;
        }
        TaskSleep(1);
    }
    SubToolIn(t, sw, TefToolMess);
}

// Runs the message list editor; in preview it fires the entries whose cut / frame the event
// reached (ev->MesSet with their timer), the edited row's message shown at once.
void ToolEvt::SubToolMessMove(ToolEvt* t, Event* ev)
{
    EventMessageData::MessElem unused; // COMPILER-DIFF: frame-only T local (24 bytes) of the original
    int i;

    if (MessTool.p->Update() == 0) {
        SubToolMessInit(t, 0);
        return;
    }
    MessTool.p->Disp();
    if (t->EtcFlag & TefBit(TefStop)) {
        // the message column (cx 3) of the cursor row shows its message
        int cx = MessTool.p->pEdit->GetCx();
        int no = MessTool.p->pEdit->GetCurrentNo();

        if (cx == 3) {
            EventMessageData* m;
            EventMessageData::MessElem* e;

            m = t->PMesDat;
            e = &m->elem[no];

            if (IsWorkAlive(e)) {
                if (e->MessNo == -1) {
                    EventMessageData::MessElem* p = 0;
                    int cnt = 1;
                    int j;
                    int k;

                    // Back-search over the preceding -1 records, hand-peeled: the target's loop is
                    // loop.c-shaped (`subi p; addi cnt; subic. k; blt; lwzu messNo; mr p; cmpwi; beq`)
                    // with giv inits `(m + no*24) - 8` (reload_cse'd to `mr rT,e; subi rT,rT,8`) and
                    // `e - 24`; no loop.c spelling found gives biv init `no` with a reduced `k - 1`
                    // giv, so the induction variables are written out. The giv init `m + no*24` is
                    // spelled `m - (-(no*24))` so cse does not fold it into `e` (a different
                    // expression until combine makes it `add T,m,A`, which reload_cse then rewrites
                    // to the target's `mr T,e`); a plain `(u8*) m + ofs` is cse'd to `subi T,e,8`.
                    k = no - 1;
                    if (k >= 0 && (p = &m->elem[k])->MessNo == -1) {
                        EventMessageData::MessElem* q = e - 1;
                        u32 ofs = no * sizeof(EventMessageData::MessElem);
                        s32 nofs = -(s32) ofs;
                        u8* base = (u8*) m - nofs;
                        s32* mp = (s32*) (base - 8);
                        do {
                            q--;
                            cnt++;
                            if (--k < 0) {
                                break;
                            }
                            mp -= 6;
                            p = q;
                        } while (*mp == -1);
                    }
                    ev->MesSet(p->MessNo, 0, 100, EVT_MES_Y);
                    for (j = 0; j < cnt; j++) {
                        cMes.Move();
                        ev->MesSet(-1, 0, 100, EVT_MES_Y);
                    }
                    cMes.Move();
                } else {
                    ev->MesSet(e->MessNo, 0, 100, EVT_MES_Y);
                }
            }
        }
    }
    {
        EventMessageData::MessElem* e;
        // The mesCnt block of the original: a pointer to the struct address (`addi rB,rD,0xc4`) for
        // mesCnt[2]/[1] (`4(rB)`, `0(rB)`) and a second one formed after the fourth eprintf for
        // mesCnt[0] (`lwzu`, the same register carries it into the loop's `stwx no,rA,no`); the
        // loop's mesCnt[1]/[2] stores go through a third, loop-fresh pointer (hoisted, `mr r26,r28`).
        // Only a struct pointer with a leading array reproduces this: `p->v[k]` is an ARRAY_REF
        // whose address stays inside the MEM (`(plus rB idx)`: cse leaves it, combine folds the
        // zero index), a plain `s32*` computes the address as a value that cse rewrites to
        // `0xc4(rD)`, and a reference/pointer to array is pointer arithmetic in this frontend.
        // A struct pointer also keeps `&EvtDebug` the cse class head (a bare `&EvtDebug.mesCnt[1]`
        // makes the `EvtDebug+0xc4` constant the head and derives `&EvtDebug` from it with a `subi`).
        struct MesCntView { s32 v[3]; };
        EventDebug* d = &EvtDebug;
        MesCntView* m = (MesCntView*) &d->mesCnt[1];
        MesCntView* x;

        i = 0;
        eprintf(0x50, 0x90, 0, 0, "%3d", m->v[1]);
        eprintf(0xA0, 0x90, 0, 0, "%3d", ev->NowCut);
        eprintf(0xF0, 0x90, 0, 0, "%3d", ev->NowFrame);
        eprintf(0x140, 0x90, 0, 0, "%3d", m->v[i]);
        x = (MesCntView*) &d->mesCnt[0];
        eprintf(0x190, 0x90, 0, 0, "%3d", x->v[i]);
        e = t->PMesDat->elem;
        for (i = 0; i < XML_NODE_MAX; i++, e++) {
            if (IsWorkAlive(e) && ev->NowCut == e->CutNo && ev->NowFrame == e->Frame) {
                int no = 0;
                int mes;
                MesCntView* y = (MesCntView*) &d->mesCnt[1];

                ev->MesSet(e->MessNo, e->Timer, 100, EVT_MES_Y);
                // the record's message number is re-read into a local before the three stores
                // (sched1: the load ahead of `stwx no`, then the stores in statement order)
                mes = e->MessNo;
                x->v[no] = no;
                y->v[no] = mes;
                y->v[1] = i;
            }
        }
    }
}

// A sub tool takes over (sw 1: flag `bit` set, the tool reads pads 3/4) or gives back (pads 1/2).
void ToolEvt::SubToolIn(ToolEvt* t, int sw, int bit)
{
    if (sw == 1) {
        t->EtcFlag &= ~TefBit(TefPrevSubMenu);
        FlagOnVar(&t->EtcFlag, (u32) bit);
        t->pJoy1 = &Joy[2];
        t->pJoy2 = &Joy[3];
    } else {
        t->EtcFlag |= TefBit(TefPrevSubMenu);
        FlagOffVar(&t->EtcFlag, (u32) bit);
        t->pJoy1 = &Joy[0];
        t->pJoy2 = &Joy[1];
    }
}

// Sets up the S-curve editor on `curve` ("Frame" / "Param" axes, range -0.2..1.2 of xMax / yMax,
// grid lock 1), cursor on the first key.
void ToolEvt::SctrlToolInit(ToolEvt* t, Hermite1* curve, f32 xMax, f32 yMax)
{
    memset(t->PDatDbSctrl, 0, sizeof(DbSctrlWork));
    t->PDatDbSctrl->curve = curve;
    SctrlSetAxisLabel(t->PDatDbSctrl, "Frame", "Param");
    t->PDatDbSctrl->gridX = xMax;
    t->PDatDbSctrl->gridY = yMax;
    t->PDatDbSctrl->grid.x = 1.0f;
    t->PDatDbSctrl->grid.y = 1.0f;
    t->PDatDbSctrl->flags = 1;
    SctrlInitAxisRange(t->PDatDbSctrl, xMax * 1.2f, xMax * -0.2f, yMax * 1.2f, yMax * -0.2f);
    if (t->PDatDbSctrl->curve->num <= 1) {
        SctrlInitCursor(t->PDatDbSctrl, 0.0f, 0.0f);
    } else {
        SctrlInitCursor(t->PDatDbSctrl, t->PDatDbSctrl->curve->key[0].t, t->PDatDbSctrl->curve->key[0].v);
    }
}

// cDbgToolMain hook: entry in use (SetFlg).
int IsWorkAlive(EventMessageData::MessElem* w)
{
    if (w->be_flag & 1) {
        return 1;
    }
    return 0;
}

// cDbgToolMain hook: sets / clears the in-use flag.
void SetWorkAlive(EventMessageData::MessElem* w, int alive)
{
    if (alive == 1) {
        w->be_flag |= 1;
    } else {
        w->be_flag &= ~1;
    }
}

// cDbgToolMain hook: entry number.
int GetWorkNo(EventMessageData::MessElem* w)
{
    return w->No;
}

// cDbgToolMain hook: entry number.
void SetWorkNo(EventMessageData::MessElem* w, int no)
{
    w->No = no;
}

// New entry: cut 0, frame 0, message -1 (none), timer 0.
void InitWork(EventMessageData::MessElem* w, int no)
{
    memclr_asm(w, sizeof(EventMessageData::MessElem));
    w->No = no;
}

// pad step of the value editors: -1 / +1 (x10 with A held)
static inline int EvtEditStep()
{
    int step = 0;

    if (Joy[0].rep & 0x10001) {
        step = -1;
    }
    if (Joy[0].rep & 0x20002) {
        step = 1;
    }
    if (Joy[0].on & 0x100) {
        step *= 10;
    }
    return step;
}

// exec callbacks return 1 while editing
static inline int EvtEditDone()
{
    u32 t = Joy[0].trg & 0x200;
    return t == 0;
}

// CutNo column pressed: left/right +-1 (A x10); 0 on B.
int CallbackCutNoExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    w->CutNo += EvtEditStep();
    eprintf(0xAA, 0xA0, 4, 0, "CutNo   : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->CutNo);
    return EvtEditDone();
}

// CutNo column text.
void CallbackCutNoUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    char buf[0x40];

    sprintf(buf, "%9ld", w->CutNo);
    DbgButtonSetName(b, buf);
}

// Frame column pressed: left/right +-1 (A x10); 0 on B.
int CallbackFrameExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    w->Frame += EvtEditStep();
    eprintf(0xAA, 0xA0, 4, 0, "Frame   : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->Frame);
    return EvtEditDone();
}

// Frame column text.
void CallbackFrameUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    char buf[0x40];

    sprintf(buf, "%9ld", w->Frame);
    DbgButtonSetName(b, buf);
}

// MessNo column pressed: left/right +-1 (A x10, -1 = clear the message); 0 on B.
int CallbackMessNoExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    w->MessNo += EvtEditStep();
    if (w->MessNo < -1) {
        w->MessNo = -1;
    }
    eprintf(0xAA, 0xA0, 4, 0, "MessNo  : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->MessNo);
    return EvtEditDone();
}

// MessNo column text.
void CallbackMessNoUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    char buf[0x40];

    sprintf(buf, "%9ld", w->MessNo);
    DbgButtonSetName(b, buf);
}

// Timer column pressed: left/right +-1 (A x10); 0 on B.
int CallbackTimerExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    w->Timer += EvtEditStep();
    eprintf(0xAA, 0xA0, 4, 0, "Timer   : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->Timer);
    return EvtEditDone();
}

// Timer column text.
void CallbackTimerUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    char buf[0x40];

    sprintf(buf, "%9ld", w->Timer);
    DbgButtonSetName(b, buf);
}

// Writes the node records of `d` as the message xml into buf and saves it as `name`.
// Returns the write cursor; the HDWrite is in the array owner (EvtMessWrite): its `cur - buf` forces
// the buffer address into a pseudo right before the call, which cse then uses for the r4 argument
// (`mr r4,r14`), where a `buf` parameter would be substituted into a fresh `addi`.
static inline char* EvtWriteXml(XmlNodeData* d, char* tmp, char* buf)
{
    XmlSimple xml;
    char* cur;
    int i;

    cur = buf;
    xml.SetXmlStart((int*) &cur, buf);
    for (i = 0; i < d->num; i++) {
        XmlNode* n = &d->node[i];

        XmlElemStart(&xml, &cur, cur, "Node");
        strcpy(tmp, n->s[XN_SETFLG]);
        xml.SetXmlElem((int*) &cur, cur, "SetFlg", tmp);
        strcpy(tmp, n->s[XN_SETOWNER]);
        xml.SetXmlElem((int*) &cur, cur, "SetOwner", "3");
        strcpy(tmp, n->s[XN_SETEDIT]);
        xml.SetXmlElem((int*) &cur, cur, "SetEdit", "true");
        strcpy(tmp, n->s[XN_NAMEPAC]);
        xml.SetXmlElem((int*) &cur, cur, "NamePac", "\203\201\203b\203Z\201[\203W");
        strcpy(tmp, n->s[XN_CUTNO]);
        xml.SetXmlElem((int*) &cur, cur, "CutNo", tmp);
        strcpy(tmp, n->s[XN_FRAME]);
        xml.SetXmlElem((int*) &cur, cur, "Frame", tmp);
        strcpy(tmp, n->s[XN_COMFLAG]);
        xml.SetXmlElem((int*) &cur, cur, "ComFlag", "0");
        strcpy(tmp, n->s[XN_SETBIN]);
        xml.SetXmlElem((int*) &cur, cur, "SetBin", "false");
        strcpy(tmp, n->s[XN_SETTPL]);
        xml.SetXmlElem((int*) &cur, cur, "SetTpl", "false");
        strcpy(tmp, n->s[XN_DAT0]);
        xml.SetXmlElem((int*) &cur, cur, "Dat0", tmp);
        strcpy(tmp, n->s[XN_DAT1]);
        xml.SetXmlElem((int*) &cur, cur, "Dat1", tmp);
        XmlElemEnd(&xml, &cur, cur, "Node");
    }
    xml.SetXmlEnd((int*) &cur, cur);
    return cur;
}

// The message list to its xml file (path built by the caller).
static inline void EvtMessWrite(EventMessageData* m, const char* path)
{
    XmlNodeData d;
    char tmp[0x80];
    char buf[XML_BUF_SIZE];
    char* cur;
    EventMessageData::MessElem* e;
    int i;

    XmlNodeDataClear(&d);
    d.num = 0;
    for (i = 0; i < XML_NODE_MAX; i++) {
        e = &m->elem[i];
        if (e->be_flag & 1) {
            // integer arithmetic keeps the written order (`mulli; add rMul, rBase; addi off`);
            // `d.node[d.num]` puts the base first.
#define CUR_NODE ((XmlNode*) (d.num * sizeof(XmlNode) + (u32) d.node))
            sprintf(CUR_NODE->s[XN_SETFLG], "true");
            sprintf(CUR_NODE->s[XN_CUTNO], "%ld", e->CutNo);
            sprintf(CUR_NODE->s[XN_FRAME], "%ld", e->Frame);
            sprintf(CUR_NODE->s[XN_DAT0], "%ld", e->MessNo);
            sprintf(CUR_NODE->s[XN_DAT1], "%ld", e->Timer);
#undef CUR_NODE
            d.num++;
        }
    }
    cur = EvtWriteXml(&d, tmp, buf);
    HDWrite(path, buf, cur - buf);
}

// SAVE of the message tool: writes the message list as evt_<room><event>_mes.xml (one Node per
// used entry) on the host.
int CallbackSave(void* arg)
{
    ToolEvt* t = (ToolEvt*) arg;
    EventMessageData* m = t->PMesDat;
    // COMPILER-DIFF: candidate (gcse PRE pseudo numbering): 13 dead pseudos before the inlined
    // clear loop put its `i - 1` PRE pseudo in a lower hash bucket than `n + 1` (allocated first -> higher register).
    int dead0, dead1, dead2, dead3, dead4, dead5, dead6, dead7, dead8, dead9, dead10, dead11, dead12;
    char path[0x100];

    sprintf(path, "%s/evt_%s%s_mes.xml", "x:/soft/room/event/evd", t->roomNo, t->eventNo);
    EvtMessWrite(m, path);
    return 0;
}

// LOAD of the message tool: parses the xml back into the message list.
int CallbackLoad(void* arg)
{
    ToolEvt* t = (ToolEvt*) arg;
    EventMessageData* m = t->PMesDat;
    char path[0x100];

    sprintf(path, "%s/evt_%s%s_mes.xml", "x:/soft/room/event/evd", t->roomNo, t->eventNo);
    EvtMessRead(m, path);
    return 0;
}
