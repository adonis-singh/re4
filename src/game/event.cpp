// game/event: the cutscene / event player (D:/Bio4/Prog/event.cpp). An Event work plays the
// packet stream of an "even""t" file (models, camera, motions, effects, messages, streams) cut by
// cut; EventMgr owns the loaded data tables and the running event; EventDebug is the t_event
// tool state; DatTbl is the name -> data slot table both use.
// 147/147 byte-identical, all sections equal (2026-09-10). Register/layout idioms used here:
//  - EventMgr::construct: `return 1` inside the err arm creates the BARRIER after the err call that
//    loop.c's find_and_verify_loops needs to move the `return i` block of the inlined EvtWorkNo loop
//    behind it (the target's `mr r0,r9; b` after the `bl err; b`).
//  - GetMod: the "pl0300" arm tests its own GetDat result and `goto err`s into the else arm's err
//    body (then arm `bl; cmpwi; beq err; b ok`, no cross-jump of the call).
//  - DelEvt uses fade.h's FadeSetW (P tied to r4); the zero's `li r0,0` after `lis r3` is #13 (the
//    original re-materialises the REG_EQUIV zero at the store; ours allocates r9 and hoists it).
//  - EspToolSetMod: `p = mname` after the strcpy + `c = p[i]` keeps the target's `mr r29,r30` copy
//    (COMPILER-DIFF candidate #3: the original's gcse copies the strcpy argument pseudo right after
//    the call for the loop's `&mname`; ours recomputes it at the block end and coalesces).
//    `BitOn(EvtDebug.pModel[no].flags, ..)` for the `lwzu/stw 0(r9)` RMW pairs.
//  - EspSetModelPtr: `u32 tbl = (u32) EspEvModList; *(cModel**) (tbl + (n << 2)) = m` -- an integer
//    base and a shift index keep both address operands unflagged, so regclass gives the index a BASE
//    register (`stwx r4,r11,r9`); `tbl[n]` on a pointer flags the base (index r0) and `n * 4` makes
//    expand put the MULT first (`add idx,tbl`).
//  - EvtSndStrStop/Play: evtStrNo/evtStrId helpers (u32 base of the member array) for the same reason.
//  - IsExePacket: the middle `return 0` is a `goto ng` to the final `return 0` (jump2 otherwise
//    cross-jumps it into the FIRST copy, COMPILER-DIFF 6 shape).
//  - NameChange: no `dst` local; `nameBuf` used directly, so the return-block use is a gcse PRE copy of
//    the strcpy argument register (`addi r3,r30,132; mr r29,r3`).
// 147/147 byte-identical (2026-09-10).
#include "types.h"
#include "atari.h"
#include "event.h"
#include "light.h"
#include "xml.h"
#include "dbg_button.h"
#include "map_obj.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "model.h"
#include "obj.h"
#include "obj18.h"
#include "em.h"
#include "player.h"
#include "pl_sub.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "esp.h"
#include "espgen.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "sce.h"
#include "scheduler.h"
#include "sscrn.h"
#include "shadow.h"
#include "game.h"
#include "file.h"
#include "datactrl.h"
#include "read.h"
#include "dvd.h"
#include "act_btn.h"
#include "hermite.h"
#include "math_sub.h"
#include "eprintf.h"
#include "foot_shadow.h"

extern "C" {
void OSReport(const char* fmt, ...);
void* memset(void* dst, int c, unsigned int n);
char* strcpy(char* dst, const char* src);
char* strcat(char* dst, const char* src);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, unsigned int n);
unsigned int strlen(const char* s);
char* strchr(const char* s, int c);
char* strstr(const char* s, const char* sub);
// game/eff_sys.cpp
// game/read.cpp: SearchEmModule (C++ linkage) comes from read.h
// game/shape.cpp
void ClrShape(cModel* m);
// game/filter01.cpp
void Filter01SetParam_CamZ(int mode, u8 type, f32 level, f32 camz);
// game/foot_shadow_tbl.cpp (incomplete types: full address, not @sda21)
extern u8 pl_fs_tbl[];
extern u8 Em10_fs_tbl[];
extern u8 Em2c_fs_tbl[];
}

// game/emdata.cpp
void EspEmDataSwapPush(int id);
void EspEmDataSwapPop(int id);
// game/shape.cpp
int ShapeSet(void* work, int frame, void* data, int flags);

// The original cUnit::beginEvent takes an int (sscrn.cpp's view of the vtable).
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define BEGIN_EVENT(p, mode) ((cUnitEvent*) (p))->beginEvent(mode)

// Removes all 16 message slots (event messages are cleared on cancel/end/begin).
// Deletes every message slot (the &cMes pointer is hoisted into a callee-saved register).
static inline void EvtMesDeleteAll()
{
    MessageControl* mes = &cMes;
    int i;

    for (i = 0; i < 16; i++) {
        mes->Delete(i);
    }
}

// Reference store: keeps a following `pG` load below it (global.h FSet for ints).
static inline void IntSet(int& d, int v)
{
    d = v;
}

// StatusFlag bit test helper.
// Status / tool flag test: the `li 1; andis.; bne; li 0; cmpwi` chains.
static inline int EvtChk(u32 f, u32 mask)
{
    return (f & mask) ? 1 : 0;
}

// be_flag bit test helper.
// Reversed form (`li 0; andi.; beq; li 1`), compared against 1 by EspToolSetMod.
static inline int BeFlgChk(cUnit* u, u32 mask)
{
    if (u->be_flag & mask) {
        return 1;
    }
    return 0;
}

// Event model number -> model (EspEvModList slot, 0 when out of range).
// The event model list entry when `no` is a valid index.
static inline cModel* EspEvModGet(int no)
{
    if (no >= 0 && no < 0x80) {
        return EspEvModList[no];
    }
    return 0;
}

// Index of an Event in the manager's array (its effect owner slot), -1 when not found.
// Index of `e` in the manager's work array (-1 when it is not one of them).
static inline int EvtWorkNo(EventMgr* mgr, Event* e)
{
    u32 i;
    for (i = 0; i < mgr->nArray; i++) {
        if ((Event*) ((u8*) mgr->pArray + i * mgr->size) == e) {
            return i;
        }
    }
    return -1;
}

// One Hermite curve of the fog / focus data (64 keys).
struct EvtCurve {
    s32 num;
    HermiteKey key[64];
};

struct EvtFogData {
    EvtCurve start;    // 0x000
    EvtCurve end;      // 0x404
};

struct EvtFocusData {
    EvtCurve near_;    // 0x000
    EvtCurve far_;     // 0x404
    f32 nearLevel;     // 0x808
    f32 farLevel;      // 0x80C
};

// 12-byte model name copied as words (cObj Obj18Work::evName).
struct EvtName {
    u32 w[3];
};

// Mtx as an assignable aggregate (block copy of the zero parts matrix).
struct EvtMtx {
    Mtx m;
};

// Room "EVS" data: a table of offsets to the room's event files.
struct EvsHeader {
    s32 num;     // 0x00
    u32 tblOfs;  // 0x04  EvsEntry[num]
};

struct EvsEntry {
    u32 ofs;     // 0x00  event file offset
    u32 x4;
};

// Object model type table of ExePacket_SetOm (name prefix, prefix length, SetObj18 type).
struct OmTbl {
    char name[0x10];
    int len;
    int type;
};

typedef int (*PacFunc)(Event*);
typedef void (*EvtFunc)(Event*, int);

template <class T>
// Destroys a unit immediately (bypassing the deferred die list) by clearing the manager flag around destroy.
void cManager<T>::destroyNow(T* p)
{
    u8 f = flag;

    flag = 0;
    destroy(p);
    flag = f;
}

EventMgr EvtMgr;
EventDebug EvtDebug;

#define EVT_STR_FRAME 26.85312f
#define EVT_FRAME_RATE 29.97f

// Event unit constructor: only records the manager id.
Event::Event(u8 t) : cUnit(1)
{
    Type = t;
}

// Clears the type; the model table is torn down by ExeEndEvt / DelEvt.
Event::~Event()
{
    Type = 0;
}

// Prepares an event from its loaded "event" header: first packet, a fresh 0x60-entry model table,
// cleared EspEvModList / counters / stream slots, the room's Evt_*_Func table (EvtMgr.GetFunc by
// name) and the cut/frame totals (CalMaxTotalFrame). Returns 0 on bad data or no memory.
int Event::init(char* nm, EvtHeader* data)
{
    u32 i;
    int j;

    if ((int) data >= 0) {
        pLog->err(0, 0, "Event::init : non addr");
        return 0;
    }
    pData = data;
    pPacket = (EvtPacket*) (data->pacOfs + (u32) data);
    if (ModTbl.init(0x60) == 0) {
        pLog->err(0, 0, "Event::init : memory failed");
        return 0;
    }
    for (i = 0; i < 0x80; i++) {
        EspEvModList[i] = 0;
    }
    EndRNo1 = 0;
    EndRNo2 = 0;
    EndRNo3 = 0;
    Id = 0;
    pPrevPacket = 0;
    PModOya = 0;
    NowTotalFrame = 0;
    NowFrame = 0;
    NowCut = 0;
    PPl = 0;
    EmListNo = 0;
    pDatFog = 0;
    pDatFocus = 0;
    DelTimer = 0;
    ChangeNoStr = 0;
    ChangeNowCut = 0;
    pLit = 0;
    for (i = 0; i < 2; i++) {
        NowStr[i] = 0;
    }
    TimerMes = 0;
    for (j = 0; j < 2; j++) {
        SndId[j] = 0;
        NowStr[j] = -1;
    }
    EvtMgr.GetFunc((void**) &PFuncTbl, nm);
    strcpy(Name, nm);
    if (CalMaxTotalFrame(&MaxCut, &MaxTotalFrame) != 0 && CalMaxFrame(&MaxFrame, NowCut) != 0) {
        return 1;
    }
    pLog->err(0, 0, "Event::init : data failed");
    return 0;
}

// One event frame: executes every packet due at (NowCut, NowFrame), then the end-of-event
// automatics (bit 0x400: fade to black 30 frames before the end; bit 0x200: the died demo),
// fog/focus curves, the stream re-sync (bit 0x10000), the room's evt func (mode 1), model
// visibility (ControlTransFlag), the action button and the frame/cut advance. Returns 0 on a
// packet error (the manager then deletes the event).
int Event::Run()
{
    int flg;
    int i;
    f32 frm;
    u32 n;
    int wait;

    MesClear();
    if (SysFlagChk(pG, SYS_SCREEN_STOP) && (NowCut != 0 || NowFrame != 0)) {
        SysFlagOff(pG, SYS_SCREEN_STOP);
    }
    while ((flg = IsExePacket()) != 0) {
        ChkCutZero();
        if (ExePacket() == 0) {
            pLog->err(0, 0, "Event::Run : failed");
            return 0;
        }
        CalNextPacket();
    }
    if (EvtChk(StatusFlag, 0x400)) {
        if (NowTotalFrame == MaxTotalFrame - 0x1E) {
            FadeSetW(2, 0x2D, 0, 0);
            IntSet(DelTimer, 0xF);
            DpfFlagOff(pG, DPF_MESSAGE);
            EvtMesDeleteAll();
        }
    }
    if (EvtChk(StatusFlag, 0x200)) {
        if (!EvtChk(StatusFlag, 0x100)) {
            if (NowTotalFrame == MaxTotalFrame - 0x1E || NowTotalFrame == MaxTotalFrame) {
                SetDiedemoExec();
                IntSet(DelTimer, 0xF);
                DpfFlagOff(pG, DPF_MESSAGE);
                EvtMesDeleteAll();
            }
        }
    }
    if (!EvtChk(EvtDebug.FlagEtc, 0x10000000)) {
        FogMove(this, pDatFog);
    }
    if (!EvtChk(EvtDebug.FlagEtc, 0x08000000)) {
        FocusMove(this, pDatFocus);
    }
    wait = EvtDebug.StfStrTimer;
    if (wait > 0) {
        wait = --EvtDebug.StfStrTimer;
        // COMPILER-DIFF: candidate (the mes `mr.` family). The original keeps `mr r9,r0; cmpwi r9,0`
        // (the decrement temp and `wait` in different registers, the copy not fused into the
        // compare); a volatile ASM_OPERANDS between the copy and the compare is what stops our
        // combine (an operand-less `asm volatile("")` is an ASM_INPUT and does not), and the dead
        // `wait == 1` test below gives `wait` a mention beyond the block so cse keeps it canonical.
        asm volatile("" : : "r"(wait));
        if (wait > 0) {
            goto func;
        }
    }
    if (wait == 1) {
        n = 0;
    }
    if (EvtChk(StatusFlag, 0x10000)) {
        frm = (f32) NowTotalFrame;
        n = (u32) (frm / EVT_STR_FRAME);
        if (frm - (f32) n * EVT_STR_FRAME < 1.0f) {
            EventMgr* m = &EvtMgr;
            StatusFlag &= ~0x10000;
            m->EvtSndStrPlay(&m->NowExeEvtKey, 1, EvtDebug.NowStr[1], 1, frm / EVT_FRAME_RATE);
        }
    }
func:
    ExeFunc(1, 0);
    ControlTransFlag();
    ExecActBtn();
    CalNextFrame();
    return 1;
}

// Appends a model to EspEvModList (event model numbers used by effect records with Core_flg 0x1000).
void Event::EspSetModelPtr(cModel* m)
{
    u32 tbl = (u32) EspEvModList;
    int n = EmListNo;

    if (n >= 0 && n < 0x80) {
        *(cModel**) (tbl + (n << 2)) = m;
    }
    EmListNo++;
}

// t_esp tool: rewinds the event, scans the SetOm/Cam/BeginEvt packets of the whole stream and fills
// EventDebug's model file list (EspToolSetMod for each model).
int Event::EspToolSetDat()
{
    char nm[0x20];
    EvtPacket* pac;
    int no;
    char* p;

    EvtDebug.NowCut = NowCut;
    RunTool(3, 0);
    EvtDebug.NumMod = 0;
    EvtDebug.ClrModelFiles();
    while (IsExePacket()) {
        pac = pPacket;
        if (pac->id > 0x20) {
            pLog->err(0, 0, "Event::ExePacket : id over");
            return 0;
        }
        switch (pac->id) {
        case 6:
            strcpy(EvtDebug.getEvName(), pac->mod.name);
            break;
        case 0xE:
            strcpy(EvtDebug.getCamName(), pac->mod.name);
            break;
        case 0xB:
            no = EvtDebug.NumMod;
            strcpy(EvtDebug.pModel[no].name, pac->mod.bin);
            EspToolSetMod(no, pac->mod.name);
            EvtDebug.NumMod++;
            break;
        }
        CalNextPacket();
    }
    p = nm;
    strcpy(p, (char*) pData);
    strcmp(p, "event/evd/r120s00.evd");
    return 1;
}

// t_esp tool: for model `nm` (costume-adjusted) reads x:/soft/room/event/evd/evt_bin_<model>.xml for
// its bin/tpl file names and records the model pointer number, ot_type, light mask and flags in
// EvtDebug.pModel[no].
void Event::EspToolSetMod(int no, char* nm)
{
    char path[0x100];
    char bin[0x100];
    char tpl[0x100];
    char mname[0x10];
    XmlSimple xml;
    char* pos;
    int modNo;
    cModel* mod;
    char* buf;
    u32 i;
    int size;
    u8 c;
    char* p;

    buf = (char*) Debug_alloc(1000000, 1);
    EvtDebug.pModel[no].pScr = 0;
    strcpy(mname, nm);
    for (i = 2; i < strlen(mname); i++) {
        c = mname[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
            mname[i + 2] = '0';
            mname[i + 3] = '0';
            break;
        }
    }
    if (pG->pl_costume == 1 && strcmp(mname, "pl0000") == 0) {
        strcpy(mname, "pl0800");
    }
    if (pG->pl_costume == 2 && strcmp(mname, "pl0000") == 0) {
        strcpy(mname, "pl0a00");
    }
    sprintf(path, "%s/evt_bin_%s.xml", "x:/soft/room/event/evd", mname);
    size = HDRead(path, buf);
    if (size != 0) {
        buf[size] = 0;
        pos = buf;
        xml.GetXmlStart(&pos, buf, "NameBin");
        do {
            if (xml.GetXmlElem(bin, pos, "NameBin") == 1) {
                if (xml.GetXmlElem(tpl, pos, "NameTpl") == 1) {
                    EvtDebug.AddNameBinTpl(no, bin, tpl);
                }
            }
        } while (xml.GetXmlNext(&pos, pos, "NameBin") != 0);
    }
    strcpy(mname, nm);
    for (u32 j = 2; j < strlen(mname); j++) {
        c = mname[j];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
            mname[j + 3] = '0';
            break;
        }
    }
    if (GetModelPtrNo(&modNo, &mod, mname)) {
        EvtDebug.pModel[no].pModel = (cModel*) modNo;
        EvtDebug.pModel[no].otType = mod->ot_type;
        EvtDebug.pModel[no].lightMask = mod->LightInfo.EnableMask;
        if (mod->z_mode == 1) {
            BitOn(EvtDebug.pModel[no].flags, 0x80000000);
        }
        if (BeFlgChk(mod, 0x1000) == 1) {
            BitOn(EvtDebug.pModel[no].flags, 0x40000000);
        }
        if (strncmp(mname, "scr", 3) == 0) {
            EvtDebug.pModel[no].pScr = mod;
        }
    }
    pLog->mes(0, 0, "t_event->t_esp:%s", nm);
    Debug_free(buf);
}

// Never called (only its string survives in .rodata).
static inline int EspToolSetDatOya(Event* evt, char* nm)
{
    pLog->err(0, 0, "Event::EspToolSetDatOya : no oya[%s]", nm);
    return 0;
}

// Looks a named event model up and returns it with its EspEvModList number; 0 when unknown.
int Event::GetModelPtrNo(int* no, cModel** mod, char* nm)
{
    u8 type;
    cModel* m;
    int i;

    if (no == 0 || mod == 0) {
        return 0;
    }
    *no = -1;
    *mod = 0;
    if (GetMod((void**) &m, nm, &type, 0) == 0) {
        pLog->err(0, 0, "Event::GetModelPtrNo : non name[%s]", nm);
        return 0;
    }
    for (i = 0; i < EmListNo; i++) {
        if (m == EspEvModGet(i)) {
            *mod = m;
            *no = i;
            return 1;
        }
    }
    pLog->err(0, 0, "Event::GetModelPtrNo : non no[%s]", nm);
    return 0;
}

// t_event tool seek: mode 0 = back subFrame frames, 1 = start of the current/previous cut, 2 = next
// cut, 3 = start of the current cut. Rewinds to the first packet and fast-forwards (StatusFlag
// 0x40000000: only set-up packets run, camera/motion start at FFNowFrame) until the target.
int Event::RunTool(int mode, int subFrame)
{
    int frm = NowFrame;
    int c = NowCut;

    switch (mode) {
    case 0:
        frm -= subFrame;
        if (frm < 0) {
            c--;
            if (c < 0) {
                c = 0;
                frm = 0;
            } else {
                if (CalMaxFrame(&frm, c) == 0) {
                    pLog->err(0, 0, "Event::RunTool : failed");
                    return 0;
                }
                frm -= 2;
            }
        }
        break;
    case 1:
        if (frm <= 1) {
            c--;
        }
        frm = 0;
        if (c < 0) {
            c = 0;
        }
        break;
    case 2:
        c++;
        frm = 0;
        if (c >= MaxCut) {
            c = MaxCut - 1;
        }
        break;
    case 3:
        frm = 0;
        if (c < 0) {
            c = 0;
        }
        break;
    }
    pPacket = (EvtPacket*) (pData->pacOfs + (u32) pData);
    toolCut = c;
    NowTotalFrame = 0;
    NowFrame = 0;
    NowCut = 0;
    FFNowFrame = frm;
    toolFrame2 = frm;
    FadeKillAll();
    if (CalMaxFrame(&MaxFrame, 0) == 0) {
        pLog->err(0, 0, "Event::init : data failed");
        return 0;
    }
    StatusFlag |= 0x40000000;
    while (frm > NowFrame || c > NowCut) {
        if (Run() == 0) {
            pLog->err(0, 0, "Event::ToolRun : failed");
            return 0;
        }
    }
    FFNowFrame = 0;
    StatusFlag &= ~0x80000000;
    if (CalMaxFrame(&MaxFrame, NowCut) == 0) {
        pLog->err(0, 0, "Event::init : data failed");
        return 0;
    }
    return 1;
}

// Event skip (START): fades to black, then fast-forwards the remaining packets (StatusFlag 0x08000000,
// motions/camera jump to their last frame; bit 0x10000000 stops at cut EvtCancelCut instead) running
// the player/body moves on the last cut so they settle, then stops the stream, runs the evt func in
// cancel mode (3) and fades back in.
int Event::RunEvtCancel()
{
    u32* key;

    if (EvtChk(StatusFlag, 0x10000000)) {
        if (EvtCancelCut <= NowCut) {
            return 1;
        }
    }
    BitOn(StatusFlag, 0x04000000);
    StaFlagOn(pG, STA_EVENT_CANCEL);
    EvtMesDeleteAll();
    FadeSetW(1, 1, 0, 0);
    TaskSleep(2);
    StatusFlag |= 0x08000000;
    while (!EvtChk(StatusFlag, 0x00800000)) {
        if (EvtChk(StatusFlag, 0x10000000)) {
            if (EvtCancelCut <= NowCut) {
                goto cancel_end;
            }
        }
        if (Run() == 0) {
            pLog->err(0, 0, "Event::RunEvtCancel : failed");
            return 0;
        }
        if (NowCut >= MaxCut - 1) {
            if (!SpfFlagChk(pG, SPF_PL) && (pPL->be_flag & 0x20)
                && (!StaFlagChk(pG, STA_SUSPEND) || (pPL->be_flag & 0x800))) {
                pPL->move();
            }
            if (PPl != 0) {
                PPl->move();
            }
        }
    }
cancel_end:
    StatusFlag &= ~0x08000000;
    EvtMesDeleteAll();
    key = (u32*) Name;
    IntSet(TimerMes, 0);
    DpfFlagOff(pG, DPF_MESSAGE);
    EvtMgr.EvtSndStrStop(key, 1, 1);
    ExeFunc(3, 0);
    if (EvtChk(StatusFlag, 0x10000000)) {
        FadeKill(FADE_NO_ROOM);
        FadeSetW(0x80000001, 0xA, 0, 0);
    }
    return 1;
}

// Requests a cancel from game code (StatusFlag 0x4000) and clears the cancel-to-cut mode.
void Event::CancelSet()
{
    StatusFlag |= 0x4000;
    StatusFlag &= ~0x04000000;
    StatusFlag &= ~0x10000000;
}

// Forbids the player from skipping this event (StatusFlag 0x02000000).
void Event::CancelNoSet()
{
    StatusFlag |= 0x02000000;
}

// Per-frame visibility of the event models (types 0..2): a model whose motion has ended or is not
// set is hidden (be_flag 0x20 / 2 off), otherwise shown; obj18 chained children and parents copy
// the state. Costume-1 (Ashley) replacement models are always hidden. Skipped while DelTimer runs.
void Event::ControlTransFlag()
{
    int n;
    int i;
    cModel* m;
    u8 type;
    cModel* oya;
    int state;
    Obj18Work* w;

    n = ModTbl.GetNumDat();
    if (DelTimer != 0) {
        return;
    }
    for (i = 0; i < n; i++) {
        if (ModTbl.GetDatWkNo((void**) &m, &type, i) == 0) {
            continue;
        }
        if (pG->game_costume == 1) {
            if (ModTbl.ChkDatWkNoName(i, "evmd100") == 1 || ModTbl.ChkDatWkNoName(i, "evm8200") == 1
                || ModTbl.ChkDatWkNoName(i, "evm7100") == 1) {
                m->be_flag &= ~0x20;
                m->be_flag &= ~2;
                continue;
            }
        }
        switch (type) {
        case 0:
        case 1:
        case 2:
            if (m == 0) {
                break;
            }
            state = MotionGetState(m);
            if (Obj18CmfGet((cObj*) m) & 0x04000000) {
                break;
            }
            if (m->kindid == 2) {
                return;
            }
            if (EvtChk(StatusFlag, 0x00100000) || EvtChk(StatusFlag, 0x00400000)) {
                if (NowCut >= MaxCut) {
                    break;
                }
                if (NowCut == MaxCut - 1 && NowFrame > 1) {
                    break;
                }
            }
            if (state == -1 || (state & 4)) {
                m->be_flag &= ~0x20;
                m->be_flag &= ~2;
            } else {
                m->be_flag |= 0x20;
                m->be_flag |= 2;
            }
            if (m->kindid == 1 && m->id == 0x18) {
                w = &((cObj*) m)->o18;
                if (w->type == 3 && w->child != 0 && !(((cObj*) m)->o18.ObjChainFlagCommon & 0x04000000)) {
                    if ((m->be_flag & 0x20) == 0) {
                        w->child->be_flag &= ~0x20;
                    } else {
                        w->child->be_flag |= 0x20;
                    }
                    if (m->isTrans() == 0) {
                        w->child->be_flag &= ~2;
                    } else {
                        w->child->be_flag |= 2;
                    }
                }
                if (obj18GetOya(&oya, (cObj*) m) == 1) {
                    if ((oya->be_flag & 0x20) == 0) {
                        m->be_flag &= ~0x20;
                    } else {
                        m->be_flag |= 0x20;
                    }
                    if (oya->isTrans() == 0) {
                        m->be_flag &= ~2;
                    } else {
                        m->be_flag |= 2;
                    }
                }
            }
            break;
        }
    }
}

// Debug line "[EVENT EXEC] EV cut/frame/total" at (16,32); also snapshots the counters for the tool.
void Event::DebugDisp()
{
    char buf[0x20];
    int col = 0;

    sprintf(buf, "%s%s", pData->room, pData->no);
    if (EvtMgr.NameCheck(buf) == 1) {
        col = 5;
    }
    eprintf(0x10, 0x20, col, 0, "[EVENT EXEC] EV:%s%s CUT:%02d/%02d FRM:%03d/%03d ALL:%04d/%04d", pData->room, pData->no, NowCut, MaxCut,
            NowFrame, MaxFrame, NowTotalFrame, MaxTotalFrame);
    BakNowCut = NowCut;
    BakMaxCut = MaxCut;
    BakNowFrame = NowFrame;
    BakMaxFrame = MaxFrame;
    BakNowTotalFrame = NowTotalFrame;
    BakMaxTotalFrame = MaxTotalFrame;
}

// Debug line "[EVENT TOOL] ..." from the snapshot taken by DebugDisp.
void Event::DebugDispTool()
{
    char buf[0x20];
    int col = 0;

    sprintf(buf, "%s%s", pData->room, pData->no);
    if (EvtMgr.NameCheck(buf) == 1) {
        col = 5;
    }
    eprintf(0x10, 0x10, col, 0, "[EVENT TOOL] EV:%s%s CUT:%02d/%02d FRM:%03d/%03d ALL:%04d/%04d", pData->room, pData->no, BakNowCut,
            BakMaxCut, BakNowFrame, BakMaxFrame, BakNowTotalFrame, BakMaxTotalFrame);
}

// 1 when the current packet is due: its cut is before NowCut, or equal with frame <= NowFrame;
// 0 at the end of the stream (bit 0x00800000) or past the last cut in loop mode (0x20000000).
int Event::IsExePacket()
{
    EvtPacket* pac;

    if (EvtChk(StatusFlag, 0x20000000)) {
        if (NowCut >= MaxCut) {
            return 0;
        }
    }
    if (EvtChk(StatusFlag, 0x00800000)) {
        goto ng;
    }
    pac = pPacket;
    if ((pac->cut == NowCut && pac->frame <= NowFrame) || pac->cut < NowCut) {
        return 1;
    }
ng:
    return 0;
}

// Executes the current packet through packetTbl (id 0..0x20). In tool fast-forward (0x40000000)
// only the set-up/camera/motion/shape/effect/fog/focus packets run; in loop mode (0x20000000) only
// ids 6..0x14 and 0x1D..0x1F. Returns 0 on a bad id or handler failure.
int Event::ExePacket()
{
    static PacFunc packetTbl[] = {
        &Event::ExePacket_BeginEvt,
        &Event::ExePacket_SetPl,
        &Event::ExePacket_SetEm,
        &Event::ExePacket_SetOm,
        &Event::ExePacket_SetParts,
        &Event::ExePacket_SetList,
        &Event::ExePacket_Cam,
        &Event::ExePacket_CamPos,
        &Event::ExePacket_CamDammy,
        &Event::ExePacket_Pos,
        &Event::ExePacket_PosPl,
        &Event::ExePacket_Mot,
        &Event::ExePacket_Shp,
        &Event::ExePacket_Esp,
        &Event::ExePacket_Lit,
        &Event::ExePacket_Str,
        &Event::ExePacket_Se,
        &Event::ExePacket_Mes,
        &Event::ExePacket_Func,
        &Event::ExePacket_ParentOn,
        &Event::ExePacket_ParentOff,
        &Event::ExePacket_EndPl,
        &Event::ExePacket_EndEm,
        &Event::ExePacket_EndOm,
        &Event::ExePacket_EndParts,
        &Event::ExePacket_EndList,
        &Event::ExePacket_EndEvt,
        &Event::ExePacket_EndPac,
        &Event::ExePacket_SetEff,
        &Event::ExePacket_Fade,
        &Event::ExePacket_Fog,
        &Event::ExePacket_Focus,
        &Event::ExePacket_SetMdt,
    };
    int id = pPacket->id;

    if (id > 0x20) {
        pLog->err(0, 0, "Event::ExePacket : id over");
        return 0;
    }
    if (!EvtChk(StatusFlag, 0x08000000)) {
        if (EvtChk(StatusFlag, 0x40000000)) {
            switch (id) {
            case 6 ... 0xC:
            case 0xE:
            case 0x11 ... 0x14:
            case 0x1D ... 0x1F:
                break;
            default:
                return 1;
            }
        } else if (EvtChk(StatusFlag, 0x20000000)) {
            switch (id) {
            case 6 ... 0x14:
            case 0x1D ... 0x1F:
                break;
            default:
                return 1;
            }
        }
    }
    if (packetTbl[pPacket->id](this) == 0) {
        pLog->err(0, 0, "Event::ExePacket : exec error");
        return 0;
    }
    return 1;
}

// Packet 0 (BeginEvt): nothing to do (the begin work is ExeBeginEvt).
int Event::ExePacket_BeginEvt(Event* evt)
{
    return 1;
}

// Packet 1 (SetPl): puts the real player into the event (beginEvent, no suspend) as model type 0.
int Event::ExePacket_SetPl(Event* evt)
{
    EvtPacket* pac = evt->pPacket;

    BEGIN_EVENT(pPL, 0);
    pPL->setNoSuspend(1);
    if (evt->SetMod(pac->mod.name, pPL, 0, 0, 2, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetPl : failed");
        return 1;
    }
    evt->EspSetModelPtr(pPL);
    return 1;
}

// Packet 2 (SetEm): unused.
int Event::ExePacket_SetEm(Event* evt)
{
    return 1;
}

// Packet 3 (SetOm): creates an event body as an obj18 from the bin/tpl named in the packet; the model
// name prefix (pl00, em10, evm.., scr, wep, ...) selects the obj18 type and foot shadow table;
// "pl0000" becomes the event player body (PPl). Registered as model type 2.
int Event::ExePacket_SetOm(Event* evt)
{
    EvtPacket* pac = evt->pPacket;
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    void* bin;
    void* tpl;
    int type;
    cObj* obj;
    int i;

    if (EvtMgr.GetBin(&bin, pac->mod.bin, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetOm : dat failed");
        return 1;
    }
    if (EvtMgr.GetBin(&tpl, pac->mod.tpl, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetOm : dat failed");
        return 1;
    }
    {
        OmTbl tbl[] = {
            {"pl00", 4, 1},     {"pl01", 4, 2},      {"evm90", 5, 2},    {"evm72", 5, 0x10},  {"pl02", 4, 3},
            {"pl04", 4, 4},     {"pl0c", 4, 3},      {"pl03", 4, 3},     {"em10", 4, 6},      {"em11", 4, 6},
            {"em12", 4, 6},     {"em13", 4, 6},      {"em14", 4, 6},     {"em15", 4, 6},      {"em16", 4, 6},
            {"em17", 4, 6},     {"em18", 4, 7},      {"evm54", 5, 7},    {"em19", 4, 6},      {"em1a", 4, 6},
            {"em1b", 4, 6},     {"em1c", 4, 6},      {"em1d", 4, 6},     {"em1e", 4, 6},      {"em1h", 4, 6},
            {"evm50", 5, 6},    {"em1g", 4, 6},      {"em1f", 4, 6},     {"em2b", 4, 0xB},    {"em34", 4, 8},
            {"evm35", 5, 0x12}, {"em37", 4, 9},      {"em30", 4, 0xA},   {"em3300a", 7, 0x14}, {"em3300", 6, 0x13},
            {"evm51", 5, 0x15}, {"evm52", 5, 0x16},  {"evm53", 5, 0x15}, {"evm82", 5, 0x17},  {"em39", 4, 0x18},
            {"pl", 2, 5},       {"em", 2, 0xC},      {"evm", 3, 0},      {"ev", 2, 0xD},      {"obm", 3, 0},
            {"et", 2, 0xE},     {"scr", 3, 0xF},     {"wep", 3, 0x10},   {"eff", 3, 0x11},
        };
        int num = sizeof(tbl) / sizeof(OmTbl);
        type = 0;
        for (i = 0; i < num; i++) {
            if (strncmp(pac->mod.name, tbl[i].name, tbl[i].len) == 0) {
                type = tbl[i].type;
                break;
            }
        }
    }
    obj = SetObj18(bin, tpl, &pos, &rot, type);
    if (obj == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetOm : om set failed");
        return 1;
    }
    *(EvtName*) obj->o18.evName = *(EvtName*) pac->mod.name;
    obj->sub2B4.atari.throughOn();
    switch (type) {
    case 1 ... 4:
    case 7 ... 0xB:
    case 0x10:
    case 0x12 ... 0x16:
    case 0x18:
        obj->be_flag |= 0x10;
        obj->be_flag |= 0x05000000;
        break;
    }
    switch (type) {
    case 1 ... 4:
    case 0x18:
        obj->be_flag |= 0x10;
        obj->sub2B4.pFootShadowTbl = pl_fs_tbl;
        break;
    case 6:
        obj->be_flag |= 0x10;
        obj->sub2B4.pFootShadowTbl = Em10_fs_tbl;
        break;
    case 0x13 ... 0x16:
        obj->be_flag |= 0x10;
        obj->sub2B4.pFootShadowTbl = Em2c_fs_tbl;
        break;
    }
    obj->be_flag |= 0x02001000;
    obj->setNoSuspend(1);
    obj->be_flag &= ~2;
    Obj18CmfSet(obj, pac->flag);
    if (strcmp(pac->mod.name, "pl0000") == 0) {
        evt->PPl = obj;
    }
    if (evt->SetMod(pac->mod.name, obj, 2, 0, 2, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetOm : failed");
        return 1;
    }
    evt->EspSetModelPtr(obj);
    return 1;
}

// Packet 4 (SetParts): attaches a parts model to a parent, substituting the costume-specific
// files (pl000e / ev0100.tpl) for Leon's alternate costumes and Ashley's armour.
int Event::ExePacket_SetParts(Event* evt)
{
    EvtPacket* pac = evt->pPacket;

    switch (pG->game_costume) {
    case 0:
    default:
        switch (pG->pl_costume) {
        case 1:
        case 2:
            if (strcmp(pac->parts.name, "ev000e") == 0 || strcmp(pac->parts.name, "ev001e") == 0) {
                return evt->ExePacket_SetPartsSub(pac->parts.name, "em/pl00/pl000e.bin", "em/pl00/pl000a.tpl", pac->parts.oya);
            }
            break;
        }
        return evt->ExePacket_SetPartsSub(pac->parts.name, pac->parts.bin, pac->parts.tpl, pac->parts.oya);
    case 1:
        if (strcmp(pac->parts.name, "ev000e") == 0) {
            return evt->ExePacket_SetPartsSub(pac->parts.name, "em/pl00/pl000e.bin", "em/pl00/pl000a.tpl", pac->parts.oya);
        }
        if (strcmp(pac->parts.name, "ev0104") == 0 || strcmp(pac->parts.name, "ev0105") == 0) {
            return evt->ExePacket_SetPartsSub(pac->parts.name, pac->parts.bin, "event/model/ev0100/ev0100.tpl", pac->parts.oya);
        }
        return evt->ExePacket_SetPartsSub(pac->parts.name, pac->parts.bin, pac->parts.tpl, pac->parts.oya);
    }
}

// Creates the parts cModelInfo from bin/tpl and adds it to parent `oya` (the ev*02 head parts also
// set the parent's parts offset); registered as model type 3.
int Event::ExePacket_SetPartsSub(char* nm, char* bin, char* tpl, char* oya)
{
    void* b;
    void* t;
    cModel* m;
    cModelInfo* info;

    if (EvtMgr.GetBin(&b, bin, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : dat failed");
        return 1;
    }
    if (EvtMgr.GetBin(&t, tpl, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : dat failed");
        return 1;
    }
    if (GetMod((void**) &m, oya, 0, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : oya non");
        return 1;
    }
    if (strcmp(nm, "ev0002") == 0 || strcmp(nm, "ev0102") == 0 || strcmp(nm, "ev0202") == 0 || strcmp(nm, "ev3002") == 0
        || strcmp(nm, "ev0402") == 0) {
        m->setPartsOffset(b);
    }
    info = ModInfoMgr.create(b, t);
    if (info == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : Parts set failed");
        return 1;
    }
    m->addModel(info);
    if (SetMod(nm, info, 3, 0, 2, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : failed");
        return 1;
    }
    return 1;
}

// Packet 5 (SetList): unused.
int Event::ExePacket_SetList(Event* evt)
{
    return 1;
}

// Packet 0x1C (SetEff): loads the event's effect data as effect owner 0xC4 + effNo (bit 0x40000 =
// loaded, released in ExeEndEvt).
int Event::ExePacket_SetEff(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;

    if (evt->effNo == -1 || evt->effNo > 1) {
        pLog->err(0, 0, "Event::ExePacket_SetEff : NoWork failed");
        return 1;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetEff : dat failed");
        return 1;
    }
    if (EspDataLoad((u32) dat, evt->effNo + 0xC4, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetEff : failed");
        return 1;
    }
    evt->StatusFlag |= 0x00040000;
    return 1;
}

// Packet 0x20 (SetMdt): installs the event's message data as MesData.ptr[1] (bit 0x2000 makes
// MesSet use message file 0xF2).
int Event::ExePacket_SetMdt(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;

    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetMdt : dat failed");
        return 1;
    }
    MesData.ptr[1] = (u8*) dat;
    evt->StatusFlag |= 0x2000;
    return 1;
}

// Packet 6 (Cam) = start of a cut: starts the camera motion (from FFNowFrame in tool seek, the last
// frame in cancel), clears fog/focus curves and all model motions, deletes the previous cut's
// effects and starts this cut's est (EventCutEstSet).
int Event::ExePacket_Cam(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;
    int frm = 0;
    void* zero;

    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Cam : dat failed");
        return 1;
    }
    zero = 0;
    if (EvtChk(evt->StatusFlag, 0x40000000)) {
        frm = evt->FFNowFrame;
    }
    if (EvtChk(evt->StatusFlag, 0x08000000)) {
        frm = evt->MaxFrame - 1;
    }
    CamCtrl.MotionSet(dat, 0, (f32) frm);
    pPL->be_flag |= 0x00200000;
    evt->pDatFog = (EvtFogData*) zero;
    evt->pDatFocus = (EvtFocusData*) zero;
    evt->MotClear();
    if (!EvtChk(evt->StatusFlag, 0x08000000)) {
        EventCutEffDelete();
        if (EvtChk(evt->StatusFlag, 0x40000000) == 0 || (EvtChk(evt->StatusFlag, 0x40000000) && pac->cut == evt->toolCut)) {
            EventCutEstSet(evt->effNo + 0xC4, evt->NowCut);
        }
    }
    return 1;
}

// Packet 7 (CamPos): unused.
int Event::ExePacket_CamPos(Event* evt)
{
    return 1;
}

// Packet 8 (CamDammy) = a cut without camera data (its length is val.no); nothing to execute.
int Event::ExePacket_CamDammy(Event* evt)
{
    return 1;
}

// Packet 9 (Pos): places a model (or "cam0000", the camera base matrix) at pos (units) / rot
// (degrees), optionally relative to a parent ("oya0000" = PModOya; flag sign bit) and, with flag
// 0x40000000, chains an obj18 to the parent's parts.
int Event::ExePacket_Pos(Event* evt)
{
    Vec pos;
    Vec rot;
    cModel* m;
    cModel* oya;
    EvtPacket* pac = evt->pPacket;
    char* nm = pac->pos.name;

    if (strcmp(nm, "cam0000") != 0) {
        if (evt->GetMod((void**) &m, nm, 0, 0) == 0) {
            pLog->err(0, 0, "Event::ExePacket_Pos : mod failed");
            return 1;
        }
    }
    pos.x = (f32) pac->pos.pos[0];
    pos.y = (f32) pac->pos.pos[1];
    pos.z = (f32) pac->pos.pos[2];
    rot.x = (f32) pac->pos.rot[0] * 3.1415927f / 180.0f;
    rot.y = (f32) pac->pos.rot[1] * 3.1415927f / 180.0f;
    rot.z = (f32) pac->pos.rot[2] * 3.1415927f / 180.0f;
    if (strcmp(pac->pos.oya, "") != 0) {
        if (strcmp(pac->pos.oya, "oya0000") == 0) {
            if (evt->PModOya == 0) {
                pLog->err(0, 0, "Event::ExePacket_Pos : oya failed");
                return 1;
            }
            oya = evt->PModOya;
        } else if (evt->GetMod((void**) &oya, pac->pos.oya, 0, 0) == 0) {
            pLog->err(0, 0, "Event::ExePacket_Pos : oya failed");
            return 1;
        }
    }
    if (pac->flag & 0x80000000) {
        PSMTXMultVec(oya->mat, &pos, &pos);
        rot.x += oya->ang.x;
        rot.y += oya->ang.y;
        rot.z += oya->ang.z;
    }
    if (pac->flag & 0x40000000) {
        if (m->kindid == 1 && m->id == 0x18) {
            OyaSetObj18((cObj*) m, oya, pac->pos.partsNo);
            m->LightInfo.Flag = 1;
        }
    }
    if (strcmp(pac->pos.name, "cam0000") == 0) {
        RotMatrix(evt->MatCamOya, &rot);
        TransMatrix(evt->MatCamOya, &pos);
        CamCtrl.setMotionBaseMatPtr(&evt->MatCamOya);
    } else {
        m->setPos(&pos);
        m->setAng(&rot);
    }
    return 1;
}

// Packet 0xA (PosPl): unused.
int Event::ExePacket_PosPl(Event* evt)
{
    return 1;
}

// Packet 0xB (Mot): starts motion `bin` on the named model (frame FFNowFrame / last frame in tool
// and cancel modes) and flags the player / body types for be_flag 0x00200000 (event motion).
int Event::ExePacket_Mot(Event* evt)
{
    cModel* m;
    void* dat;
    EvtPacket* pac = evt->pPacket;
    int frm = 0;
    u32 t;

    if (EvtChk(evt->StatusFlag, 0x40000000)) {
        frm = evt->FFNowFrame;
    }
    if (EvtChk(evt->StatusFlag, 0x08000000)) {
        frm = evt->MaxFrame - 1;
    }
    if (evt->GetMod((void**) &m, pac->mod.name, 0, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Mot : mod failed");
        return 1;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.bin, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Mot : dat failed");
        return 1;
    }
    if (*(u16*) dat == 0) {
        return 1;
    }
    MotionClear(m, 1);
    MotionSetCore(m, &((cEm*) m)->pMotion, dat, 0, 0, 1, (u16) frm);
    if (m->kindid == 0 && m->id == 0) {
        m->be_flag |= 0x00200000;
    }
    ClrShape(m);
    if (m->kindid == 1 && m->id == 0x18) {
        Obj18Work* w = &((cObj*) m)->o18;
        t = w->type;
        if ((t >= 1 && t <= 4) || t == 7 || t == 8 || t == 9 || t == 0xA || t == 0x13 || t == 0x14 || t == 0x15 || t == 0x16
            || t == 0xB) {
            m->be_flag |= 0x00200000;
        }
        if (w->type == 3 && w->child != 0) {
            w->child->be_flag |= 0x00200000;
        }
    }
    return 1;
}

// Packet 0xC (Shp): starts a face shape animation on the model (or its cModelInfo for a type-2 body).
int Event::ExePacket_Shp(Event* evt)
{
    cModel* m;
    u8 type;
    void* dat;
    EvtPacket* pac = evt->pPacket;
    int frm = 0;
    void* w;

    if (EvtChk(evt->StatusFlag, 0x40000000)) {
        frm = evt->FFNowFrame;
    }
    if (EvtChk(evt->StatusFlag, 0x08000000)) {
        frm = evt->MaxFrame - 1;
    }
    if (evt->GetMod((void**) &m, pac->mod.name, &type, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Shp : mod failed");
        return 1;
    }
    if (type == 2) {
        w = m->pModelInfo;
    } else {
        w = m;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.bin, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Shp : dat failed");
        return 1;
    }
    ShapeSet(w, (s16) frm, dat, 2);
    return 1;
}

// Packet 0xD (Esp): starts an est on the named model (or the world): type 0 = owner 1 (common event
// effects), 5 = the event's own effect data (0xC4 + effNo), 6 = owner 0x54; flag sign bit places it
// relative to PModOya.
int Event::ExePacket_Esp(Event* evt)
{
    Vec pos;
    Vec rot;
    cModel* m;
    EvtPacket* pac = evt->pPacket;
    char* nm = pac->esp.name;
    int ret;
    int e;

    ret = strcmp(nm, "");
    if (ret == 0) {
        m = 0;
    } else if (evt->GetMod((void**) &m, nm, 0, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Esp : mod failed");
        return 1;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    if (pac->flag & 0x80000000) {
        if (evt->PModOya == 0) {
            pLog->err(0, 0, "Event::ExePacket_Esp : oya failed");
            return 1;
        }
        PSMTXMultVec(evt->PModOya->mat, &pos, &pos);
        rot.x += evt->PModOya->ang.x;
        rot.y += evt->PModOya->ang.y;
        rot.z += evt->PModOya->ang.z;
    }
    if (pac->esp.type == 0) {
        EstSet((int) m, -1, &pos, &rot, 1, pac->esp.parts, 1, 0, 0, 0);
    }
    if (pac->esp.type == 5) {
        e = evt->effNo;
        if (e == -1 || e > 1) {
            pLog->err(0, 0, "Event::ExePacket_SetEff : NoWork failed");
            return 1;
        }
        EstSet((int) m, -1, &pos, &rot, e + 0xC4, pac->esp.parts, 1, (u8) (e + 0x37), 0, 0);
    }
    if (pac->esp.type == 6) {
        EstSet((int) m, -1, &pos, &rot, 0x54, pac->esp.parts, 1, 0, 0, 0);
    }
    return 1;
}

// Packet 0xE (Lit): switches the room lighting to the event's light data (not repeated in tool seek).
int Event::ExePacket_Lit(Event* evt)
{
    cLit* dat;
    EvtPacket* pac = evt->pPacket;

    if (EvtChk(EvtDebug.FlagEtc, 0x20000000)) {
        return 1;
    }
    if (EvtMgr.GetBin((void**) &dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Lit : dat failed");
        return 1;
    }
    if (EvtChk(evt->StatusFlag, 0x40000000)) {
        if (evt->toolCut != evt->NowCut || evt->pLit == dat) {
            return 1;
        }
    }
    evt->pLit = dat;
    LightMgr.roomLitSet(dat);
    LightMgr.update(0, -1);
    return 1;
}

// Packet 0x1E (Fog): installs the fog start/end Hermite curves played by FogMove.
int Event::ExePacket_Fog(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;

    if (EvtChk(EvtDebug.FlagEtc, 0x10000000)) {
        return 1;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Fog : dat failed");
        return 1;
    }
    evt->pDatFog = dat;
    return 1;
}

// Packet 0x1F (Focus): installs the depth-of-field near/far curves played by FocusMove.
int Event::ExePacket_Focus(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;

    if (EvtChk(EvtDebug.FlagEtc, 0x08000000)) {
        return 1;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Focus : dat failed");
        return 1;
    }
    evt->pDatFocus = dat;
    return 1;
}

// Never called (only its string survives in .rodata).
static inline const char* EvtDebugEvdName()
{
    return "event/evd/r100s40.evd";
}

// Packet 0xF (Str): starts stream (voice/music) val.arg in stream block val.no (0 = BGM block;
// flag 0x20000000 = no frame sync); ChangeNoStr overrides the stream number once.
int Event::ExePacket_Str(Event* evt)
{
    char nm[0x40];
    EvtPacket* pac = evt->pPacket;
    char* key = nm;
    int no;
    int blk;

    strcpy(key, evt->Name);
    blk = pac->val.no;
    no = pac->val.arg;
    if (evt->ChangeNoStr != 0) {
        no = evt->ChangeNoStr;
        evt->ChangeNoStr = 0;
    }
    if (blk == 0) {
        EvtMgr.EvtSndStrPlay((u32*) key, 0, no, 0, 0.0f);
    } else if (!(pac->flag & 0x20000000)) {
        EvtMgr.EvtSndStrPlay((u32*) key, blk, no, 1, 0.0f);
    } else {
        EvtMgr.EvtSndStrPlay((u32*) key, blk, no, 0, 0.0f);
    }
    return 1;
}

// Packet 0x10 (Se): plays sound effect (val.no, val.arg) at the player position.
int Event::ExePacket_Se(Event* evt)
{
    EvtPacket* pac = evt->pPacket;

    SndCall((u16) pac->val.no, (u16) pac->val.arg, &pPL->pos, 0, 0, 0);
    return 1;
}

// Packet 0x1D (Fade): fade slot 2 in (val.no == 0) or out over val.time frames.
int Event::ExePacket_Fade(Event* evt)
{
    EvtPacket* pac = evt->pPacket;

    if (pac->val.no == 0) {
        FadeSetW(0x80000002, pac->val.time, 0, 0);
    } else {
        FadeSetW(2, pac->val.time, 0, 0);
    }
    return 1;
}

// Packet 0x11 (Mes): shows subtitle val.no for val.arg frames at the bottom of the screen.
int Event::ExePacket_Mes(Event* evt)
{
    EvtPacket* pac;

    if (EvtChk(EvtDebug.FlagEtc, 0x04000000)) {
        return 1;
    }
    if (DbgFlagChk(pG, DBG_CAPTION_OFF)) {
        return 1;
    }
    pac = evt->pPacket;
    evt->MesSet(pac->val.no, pac->val.arg, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    return 1;
}

// Packet 0x12 (Func): calls entry val.no of the room's event function table with val.arg.
int Event::ExePacket_Func(Event* evt)
{
    u32 tbl = evt->PFuncTbl;
    EvtPacket* pac = evt->pPacket;
    EvtFunc fn;

    if (tbl == 0) {
        pLog->err(0, 0, "Event::ExePacket_Func: func failed");
        return 1;
    }
    fn = *(EvtFunc*) (pac->val.no * 4 + tbl);
    fn(evt, pac->val.arg);
    return 1;
}

// Packet 0x13 (ParentOn): unused.
int Event::ExePacket_ParentOn(Event* evt)
{
    return 1;
}

// Packet 0x14 (ParentOff): unused.
int Event::ExePacket_ParentOff(Event* evt)
{
    return 1;
}

// Packet 0x15 (EndPl): unused.
int Event::ExePacket_EndPl(Event* evt)
{
    return 1;
}

// Packet 0x16 (EndEm): unused.
int Event::ExePacket_EndEm(Event* evt)
{
    return 1;
}

// Packet 0x17 (EndOm): unused.
int Event::ExePacket_EndOm(Event* evt)
{
    return 1;
}

// Packet 0x18 (EndParts): unused.
int Event::ExePacket_EndParts(Event* evt)
{
    return 1;
}

// Packet 0x19 (EndList): unused.
int Event::ExePacket_EndList(Event* evt)
{
    return 1;
}

// Packet 0x1A (EndEvt): unused (the end work is ExeEndEvt).
int Event::ExePacket_EndEvt(Event* evt)
{
    return 1;
}

// Packet 0x1B (EndPac) terminates the stream; nothing to execute.
int Event::ExePacket_EndPac(Event* evt)
{
    return 1;
}

// Event start (first Run after SetEvt): tells the scenario system (SceEventStart, bit 0x80 = "true"
// start), sets Status_flg[2] 0x80000/0x10000 (event running), loads the event font, runs the evt
// func in begin mode (0), registers Leon's own model files as bins, clears messages and inits the
// event sound unless the header's sndFlag sign bit is set.
void Event::ExeBeginEvt(Event* evt, int mode)
{
    int i;

    if (EvtChk(evt->StatusFlag, 0x80)) {
        pLog->mes(0, 0, "Event::ExeBeginEvt : SceEventStart(true)");
        SceEventStart(1);
    } else {
        SceEventStart(0);
    }
    StaFlagOn(pG, STA_EVENT_SYSYTEM);
    StaFlagOn(pG, STA_EFFAREA_USE_CAM);
    StaFlagOff(pG, STA_EVENT_CANCEL);
    cMes.loadEventFont();
    ExeFunc(0, 0);
    if (pG->pl_type == 0) {
        EvtMgr.SetBin("em/pl00/pl000a.bin", PL_ARC_PTR(pG->pPlayer, 4), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000a.tpl", PL_ARC_PTR(pG->pPlayer, 5), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000d.bin", PL_ARC_PTR(pG->pPlayer, 9), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000b.tpl", PL_ARC_PTR(pG->pPlayer, 7), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000e.bin", PL_ARC_PTR(pG->pPlayer, 0xA), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000l.bin", PL_ARC_PTR(pG->pPlayer, 0x10), 0, 2);
        EvtMgr.SetBin("etc/core/dummy.bin", (void*) (pG->pArc->ofs_20 + (u32) pG->pArc), 0, 2);
        EvtMgr.SetBin("etc/core/dummy.tpl", (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), 0, 2);
    }
    EvtMesDeleteAll();
    SysFlagOn(pG, SYS_SCREEN_STOP);
    if (!EvtChk(evt->pData->sndFlag, 0x80000000)) {
        SndEventInit();
    }
}

// Event end: moves the real player to the event body's position/heading (unless bit 0x800), returns
// the partner behind the player (unless bit 0x40), releases every registered model (player
// endEvent0, obj18 bodies destroyed, parts destroyed, type-5 motions cleared), the effect data,
// all event effects, restores room lighting and the camera, clears messages/shadows, runs the evt
// func in end mode (2), reloads the stage font and ends the event sound / scenario state.
void Event::ExeEndEvt(Event* evt, u32 mode)
{
    Vec pos;
    Vec rot;
    u8 type;
    cModel* m;
    int n;
    int i;
    cPlayer* pl;

    if (!EvtChk(evt->StatusFlag, 0x800)) {
        pos = pPL->pos;
        rot = pPL->ang;
        if (evt->PPl != 0) {
            EvtMgr.GetZeroPartsWorldPos(evt->PPl, &pos, &rot);
            evt->PPl = 0;
        }
        pPL->zeroPartsPosInit(&pos, &rot);
    }
    if (!EvtChk(evt->StatusFlag, 0x40)) {
        SubCharCtrl(SCC_BEHIND, 0);
    }
    n = evt->ModTbl.GetNumDat();
    for (i = 0; i < n; i++) {
        if (evt->ModTbl.GetDatWkNo((void**) &m, &type, i) == 0) {
            continue;
        }
        switch (type) {
        case 0:
            pl = pPL;
            if (mode & 0x10000000) {
                pl->endEvent0(1);
            } else {
                pl->endEvent0(0);
            }
            break;
        case 2:
            OyaSetObj18((cObj*) m, 0, 0);
            DelObj18((cObj*) m);
        case 3:
            ObjMgr.destroy((cObj*) m);
            break;
        case 5:
            MotionClear(m, 0);
            break;
        case 1:
        case 4:
            break;
        }
        evt->ModTbl.DelDatWkNo(i);
    }
    if (evt->effNo != -1 && evt->effNo <= 1) {
        if (EvtChk(evt->StatusFlag, 0x00040000)) {
            EspDataRelease(evt->effNo + 0xC4, 1, 1);
        } else {
            pLog->err(0, 0, "Event::ExeEndEvt: no EspDataRelease");
        }
    }
    EventAllEffDelete();
    if (LightMgr.roomLitCheck() == 0) {
        LightMgr.roomLitSet(0);
        LightMgr.update(0, -1);
    }
    if (CamCtrl.IsMotionSet() == 1) {
        CamCtrl.setMotionBaseMatPtr(0);
        CamCtrl.Comeback(0);
    }
    DpfFlagOff(pG, DPF_MESSAGE);
    cMes.roomInit();
    EvtMesDeleteAll();
    ShadowMemClear();
    ExeFunc(2, 0);
    pPL->move();
    SysFlagOn(pG, SYS_START_EVT_SKIP);
    SubScreenWait(0xF);
    cMes.loadStageFont();
    if (!EvtChk(evt->pData->sndFlag, 0x80000000)) {
        SndEventEnd();
    }
    StaFlagOff(pG, STA_EVENT_SYSYTEM);
    StaFlagOff(pG, STA_EFFAREA_USE_CAM);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Calls the room's "evt_<room><no>_func" handler with funcMode = mode (0 begin, 1 run, 2 end,
// 3 cancel); skipped when bit 0x20000 (no func) or during cancel fast-forward for mode 1.
int Event::ExeFunc(int mode, int param)
{
    char nm[0x30];
    char a[8];
    char b[8];
    void* fn;

    if (EvtChk(StatusFlag, 0x00020000)) {
        return 1;
    }
    if (mode == 1 && EvtChk(StatusFlag, 0x08000000)) {
        return 1;
    }
    funcMode = mode;
    strcpy(a, pData->room);
    strcpy(b, pData->no);
    strcpy(nm, "evt_");
    strcat(nm, a);
    strcat(nm, b);
    strcat(nm, "_func");
    if (EvtMgr.GetFunc(&fn, nm) == 0) {
        return 0;
    }
    if (fn == 0) {
        pLog->err(0, 0, "Event::ExeFunc: func failed");
        return 1;
    }
    ((EvtFunc) fn)(this, param);
    return 1;
}

// Advances to the next packet; sets bit 0x00800000 when the stream is exhausted.
void Event::CalNextPacket()
{
    pPrevPacket = pPacket;
    pPacket = (EvtPacket*) ((u8*) pPacket + pPacket->size);
    if ((u32) pPacket >= (u32) pData + pData->pacOfs + pData->pacSize) {
        StatusFlag |= 0x00800000;
    }
}

// Advances NowFrame/NowTotalFrame; a pending ChangeNowCut jumps to that cut; at MaxFrame moves to the
// next cut and recomputes its length.
void Event::CalNextFrame()
{
    char buf[0x20];
    int zero = 0;

    if (EvtChk(StatusFlag, 0x20000000)) {
        if (NowCut >= MaxCut) {
            return;
        }
    }
    if (ChangeNowCut != 0) {
        NowCut = ChangeNowCut - 1;
        NowFrame = MaxFrame;
        ChangeNowCut = zero;
    }
    NowFrame++;
    NowTotalFrame++;
    if (NowFrame < MaxFrame) {
        return;
    }
    NowFrame = zero;
    NowCut++;
    if (CalMaxFrame(&MaxFrame, NowCut) == 0) {
        pLog->err(0, 0, "Event::init : data failed");
    }
}

// Runs the evt func once when the packets cross from the pre-roll (cut -1) into cut 0.
void Event::ChkCutZero()
{
    if (pPrevPacket != 0 && pPrevPacket->cut < 0 && pPacket->cut == 0) {
        ExeFunc(1, 0);
    }
}

// Counts the cuts of the stream (Cam and CamDammy packets) up to EndPac.
int Event::CalMaxCut(int* out)
{
    EvtPacket* p;
    int n;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    n = 0;
    p = (EvtPacket*) (pData->pacOfs + (u32) pData);
    while (p->id != 0x1B) {
        if (p->id > 0x20) {
            pLog->err(0, 0, "Event::CalMaxFrame : id over");
            return 0;
        }
        if (p->id == 6) {
            n++;
        }
        if (p->id == 8) {
            n++;
        }
        p = (EvtPacket*) ((u8*) p + p->size);
    }
    *out = n;
    return 1;
}

// Length in frames of cut `noCut`: the camera motion's frame count + 1, or a CamDammy's val.no.
int Event::CalMaxFrame(int* out, int noCut)
{
    void* dat;
    EvtPacket* p;
    int n;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    n = 0;
    p = (EvtPacket*) (pData->pacOfs + (u32) pData);
    while (p->id != 0x1B) {
        if (p->id > 0x20) {
            pLog->err(0, 0, "Event::CalMaxFrame : id over");
            return 0;
        }
        if (p->id == 6) {
            if (noCut == n) {
                if (EvtMgr.GetBin(&dat, p->mod.name, 0) == 0) {
                    pLog->err(0, 0, "Event::CalMaxFrame : dat failed");
                    return 0;
                }
                *out = *(u16*) dat + 1;
                return 1;
            }
            n++;
        }
        if (p->id == 8) {
            if (noCut == n) {
                *out = p->val.no;
                return 1;
            }
            n++;
        }
        p = (EvtPacket*) ((u8*) p + p->size);
    }
    *out = 0;
    return 1;
}

// Cut count and total frame count of the event.
int Event::CalMaxTotalFrame(int* outCut, int* outTotal)
{
    int mc;
    int mf;
    int sum;
    int i;

    if (outCut == 0 || outTotal == 0) {
        return 0;
    }
    *outCut = 0;
    *outTotal = 0;
    if (CalMaxCut(&mc) == 0) {
        pLog->err(0, 0, "Event::CalMaxTotalFrame : CalMaxCut failed");
        return 0;
    }
    mf = 0;
    sum = 0;
    for (i = 0; i < mc; i++) {
        if (CalMaxFrame(&mf, i) == 0) {
            pLog->err(0, 0, "Event::CalMaxTotalFrame : CalMaxFrame failed");
            return 0;
        }
        sum += mf;
    }
    *outCut = mc;
    *outTotal = sum;
    return 1;
}


// FadeSet with black->clear (no sign bit) or clear->black colours.
static inline void EvtFadeSetW(int no, u32 time, u32 z, int late)
{
    FadeColorPair col;
    u32 c;

    if (no & 0x80000000) {
        c = 0xFF;
        *(u32*) &col.start = c;
        c = 0;
        *(u32*) &col.end = c;
    } else {
        *(u32*) &col.start = 0;
        *(u32*) &col.end = 0xFF;
    }
    FadeSet(no, &col.start, &col.end, time, z, late);
}

// Starts the death demo from a died-demo event (bit 0x100 = done) and fades back in when a cancel
// fade is up.
void Event::SetDiedemoExec()
{
    StatusFlag |= 0x100;
    if (EvtChk(StatusFlag, 0x04000000)) {
        FadeKill(FADE_NO_ROOM);
        EvtFadeSetW(0x80000001, 0xA, 0, 0);
    }
    DiedemoExec(0, 1);
}

// Enables the action-button prompt `no` for the event (button mash counting).
void Event::BeginActBtn(int no)
{
    memset(&actBtnOn, 0, 0xC);
    actBtnNo = no;
    actBtnOn = 1;
}

// Disables the action-button prompt.
void Event::EndActBtn()
{
    actBtnOn = 0;
}

// Number of presses counted while the prompt was up.
int Event::GetActBtnCount()
{
    return actBtnCount;
}

// Per-frame prompt: shows ActBtn `actBtnNo` and counts A-button (Key.trg 0x80000) presses.
void Event::ExecActBtn()
{
    if (actBtnOn != 1) {
        return;
    }
    DpfFlagOff(pG, DPF_MESSAGE);
    ActBtn.set(actBtnNo, 5, 0, 0, 2, 2, 0, 0);
    SpfFlagOff(pG, SPF_ACTBTN);
    if (Key.trg & 0x80000) {
        actBtnCount++;
    }
}

// Shows subtitle `no` (message file 0xF0, or 0xF2 with an event mdt) for `time` frames at (x, y);
// -1 ends the current one. Suppressed in Japanese (language 1).
void Event::MesSet(int no, int time, int x, int y)
{
    int i;

    if (pSys->language != 1) {
        DpfFlagOff(pG, DPF_MESSAGE);
        if (no == -1) {
            cMes.WaitEnd(0);
        } else {
            EvtMesDeleteAll();
            if (EvtChk(StatusFlag, 0x2000)) {
                SceMesSet(no, 0xF2, 1, x, y);
            } else {
                SceMesSet(no, 0xF0, 1, x, y);
            }
        }
    }
    MesNoOld = no;
    TimerMes = time;
}

// Counts the subtitle timer down and restores Disp_flg 0x800 (HUD) when it expires.
void Event::MesClear()
{
    int no;

    if (TimerMes > 0) {
        TimerMes--;
        if (TimerMes <= 0) {
            IntSet(TimerMes, 0);
            DpfFlagOn(pG, DPF_MESSAGE);
        }
    }
    no = 0;
    EvtDebug.mesCnt[no]++;
}

// Applies the cut's fog start/end Hermite curves at the current frame.
void Event::FogMove(Event* evt, void* fog)
{
    f32 start;
    f32 end;
    f32 t;
    EvtFogData* d = (EvtFogData*) fog;
    int frame = evt->NowFrame;

    if (d == 0) {
        return;
    }
    t = (f32) frame;
    if (Hermite_1CurveCalc((Hermite1*) &d->start, t, &start)) {
        LightMgr.setFogStart(start);
    }
    if (Hermite_1CurveCalc((Hermite1*) &d->end, t, &end)) {
        LightMgr.setFogEnd(end);
    }
    LightMgr.setFog();
}

// Applies the cut's depth-of-field near/far curves (Filter01 camera-Z blur) at the current frame.
void Event::FocusMove(Event* evt, void* focus)
{
    f32 near_;
    f32 far_;
    f32 t;
    EvtFocusData* d = (EvtFocusData*) focus;
    int frame = evt->NowFrame;

    if (EvtChk(evt->StatusFlag, 0x40000000)) {
        return;
    }
    if (d == 0) {
        return;
    }
    t = (f32) frame;
    if (Hermite_1CurveCalc((Hermite1*) &d->near_, t, &near_)) {
        Filter01SetParam_CamZ(0, 1, d->nearLevel, near_);
    }
    if (Hermite_1CurveCalc((Hermite1*) &d->far_, t, &far_)) {
        Filter01SetParam_CamZ(1, 1, d->farLevel, far_);
    }
}

// Clears the motion of every event model of types 0/1/2/5 (start of a cut).
void Event::MotClear()
{
    cModel* m;
    u8 type;
    int n;
    int i;

    n = ModTbl.GetNumDat();
    for (i = 0; i < n; i++) {
        if (ModTbl.GetDatWkNo((void**) &m, &type, i) == 0) {
            continue;
        }
        switch (type) {
        case 0:
        case 1:
        case 2:
        case 5:
            if (Obj18CmfGet((cObj*) m) & 0x04000000) {
                break;
            }
            if (m->kindid == 2) {
                return;
            }
            MotionClear(m, 1);
            break;
        }
    }
}

// Registers a model in the event's name table (type 0 player, 2 obj18 body, 3 parts, 5 external).
int Event::SetMod(char* nm, void* mod, u8 type, void* dat2, u8 flag, int* wkNo)
{
    int no;

    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (ModTbl.SetDat(nm, mod, type, dat2, flag, &no) == 0) {
        pLog->err(0, 0, "Event::SetMod : failed");
        return 0;
    }
    if (wkNo != 0) {
        *wkNo = no;
    }
    return 1;
}

// Looks a model up by event name; Debug_flg[3] bit 3 redirects "pl0200" to "pl0300".
int Event::GetMod(void** mod, char* nm, u8* type, int* wkNo)
{
    u8 t;
    void* m;
    int no;
    int ret;

    if (mod == 0) {
        return 0;
    }
    *mod = 0;
    if (type != 0) {
        *type = 0;
    }
    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (DbgFlagChk(pG, DBG_ADA_OMAKE_EV) && strcmp(nm, "pl0200") == 0) {
        nm = "pl0300";
        if (ModTbl.GetDat(&m, &t, nm, &no) == 0) {
            goto err;
        }
    } else if (ModTbl.GetDat(&m, &t, nm, &no) == 0) {
    err:
        {
            register int pin asm("r27"); // COMPILER-DIFF: #17
            asm volatile("" : "=r"(pin));
            asm volatile("" : : "r"(pin));
        }
        pLog->err(0, 0, "Event::GetMod : mod failed[%s]", nm);
        return 0;
    }
    {
        // COMPILER-DIFF: #17. r27 was used-so-far in the original's global-alloc pass 0 and
        // conflicted with nm (err block) and type/wkNo/mod (tail) but not with `this`, which
        // therefore took r27 while mod fell to r28. No code is emitted.
        register int pin asm("r27");
        asm volatile("" : "=r"(pin));
        asm volatile("" : : "r"(pin));
    }
    if (type != 0) {
        *type = t;
    }
    // COMPILER-DIFF: #17 (companion). A codeless memory-operand asm = one more real insn inside the
    // wkNo compare's live range only, so the type compare (equal length, lower pseudo) is allocated
    // first and takes cr4 as in the original; a register-tied asm here is deleted as dead.
    asm("" : "+m"(no));
    if (wkNo != 0) {
        *wkNo = no;
    }
    *mod = m;
    return 1;
}

// Never called (only its string survives in .rodata).
static inline int EventDelMod(Event* evt, char* nm)
{
    if (evt->ModTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "Event::DelMod : failed");
        return 0;
    }
    return 1;
}

// Manager for at most 2 simultaneous events.
EventMgr::EventMgr() : cManager<Event>(sizeof(Event), 2)
{
}

// Nothing to release.
EventMgr::~EventMgr()
{
}

// Unit construction hook: runs the Event constructor and gives it its array index as effect slot.
int EventMgr::construct(Event* p, u32 id)
{
    Event* e;
    int no;

    e = p->ctorI(id);
    if (e) {
        no = EvtWorkNo(this, e);
        e->effNo = no;
        if (no == -1 || no > 1) {
            pLog->err(0, 0, "EventMgr::construct : getWorkNo failed");
            return 1;
        }
    }
    return 1;
}

// System init: names the manager.
int EventMgr::init()
{
    setName("EventMgr");
    return 1;
}

// Room init: allocates the evd (0x20), bin (0x140), func (0x10) and read (8) tables, clears the
// running-event key and the window FCV pointers.
int EventMgr::myRoomInit()
{
    int i;

    if (EvdTbl.init(0x20) == 0 || BinTbl.init(0x140) == 0 || FuncTbl.init(0x10) == 0 || ReadTbl.init(8) == 0) {
        pLog->err(0, 0, "EventMgr::init : memory failed");
        return 0;
    }
    memclr_asm(&NowExeEvtKey, sizeof(u32));
    for (i = 0; i < 0x20; i++) {
        pUnit[i] = 0;
    }
    ClearEmWindowFcv();
    return 1;
}

// Never called (only its string survives in .rodata).
static inline int EventMgrEnd(EventMgr* mgr)
{
    if (mgr->EvdTbl.end() == 0 || mgr->BinTbl.end() == 0 || mgr->FuncTbl.end() == 0 || mgr->ReadTbl.end() == 0) {
        pLog->err(0, 0, "EventMgr::end : failed");
        return 0;
    }
    return 1;
}

// Deletes every live event immediately (room change).
int EventMgr::DelAll()
{
    u32 i;
    Event* e;

    for (i = 0; i < nArray; i++) {
        e = (Event*) ((u8*) pArray + size * i);
        if (e->isAlive()) {
            DelEvt(e, 0);
        }
    }
    return 1;
}

// Per-frame: for each live, not finished/paused event runs ExeBeginEvt on its first frame, Run(),
// the START-button / requested cancel (unless forbidden, finished or died-demo), and when the
// stream is done counts DelTimer down and deletes it (or parks it: bits 0x00100000 -> 0x00080000,
// 0x00400000 -> 0x00200000 keep the event alive for the caller).
int EventMgr::Run()
{
    u32 i;
    Event* e;

    dieCheck();
    for (i = 0; i < nArray; i++) {
        e = (Event*) ((u8*) pArray + size * i);
        if (!e->isAlive()) {
            continue;
        }
        if (EvtChk(e->StatusFlag, 0x00080000)) {
            continue;
        }
        if (EvtChk(e->StatusFlag, 0x00200000)) {
            continue;
        }
        if (EvtChk(e->StatusFlag, 0x80000000)) {
            continue;
        }
        e->DebugDisp();
        if (EvtChk(e->StatusFlag, 0x01000000)) {
            e->StatusFlag &= ~0x01000000;
            e->ExeBeginEvt(e, 0);
        }
        if (!EvtChk(e->StatusFlag, 0x00800000)) {
            if (e->Run() == 0) {
                pLog->err(0, 0, "EventMgr::Run : failed");
                DelEvt(e, 0);
                continue;
            }
            if (EvtChk(e->StatusFlag, 0x20000000)) {
                continue;
            }
            if (!EvtChk(e->StatusFlag, 0x02000000) && !EvtChk(e->StatusFlag, 0x04000000) && !EvtChk(e->StatusFlag, 0x00800000)
                && !EvtChk(e->StatusFlag, 0x100) && ((Key.trg & 0x20000000) || EvtChk(e->StatusFlag, 0x4000))) {
                e->RunEvtCancel();
            }
        }
        if (EvtChk(e->StatusFlag, 0x00800000)) {
            if (e->DelTimer != 0) {
                e->DelTimer--;
                continue;
            }
            if (EvtChk(e->StatusFlag, 0x00100000)) {
                e->StatusFlag |= 0x00080000;
                continue;
            }
            if (EvtChk(e->StatusFlag, 0x00400000)) {
                e->StatusFlag |= 0x00200000;
                continue;
            }
            DelEvt(e, 1);
        }
    }
    return 1;
}

// 1 when an event named *key is alive (chk != 1 ignores parked ones); *out receives the Event.
int EventMgr::IsAliveEvt(u32* key, int out, int chk)
{
    char nm[0x20];
    u32 i;
    Event* e;

    for (i = 0; i < nArray; i++) {
        char* p = nm;
        e = (Event*) ((u8*) pArray + size * i);
        if (!e->isAlive()) {
            continue;
        }
        if (chk != 1) {
            if (EvtChk(e->StatusFlag, 0x00080000)) {
                continue;
            }
        }
        strcpy(p, e->Name);
        if (strcmp(p, (char*) key) != 0) {
            continue;
        }
        if (out != 0) {
            *(Event**) out = e;
        }
        return 1;
    }
    return 0;
}

// Preloads an event file into ARAM (skipped with Debug_flg[0] 0x02000000).
int EventMgr::EvtReadAram(char* nm, int em, int* out, int wait, u32 sz)
{
    int ret = 0;

    if (!DbgFlagChk(pG, DBG_EVENT_TOOL)) {
        ret = EvtReadSub(nm, 1, em, out, wait, sz);
    }
    return ret;
}

// Loads an event file into main RAM (see EvtReadSub).
int EventMgr::EvtReadMram(char* nm, int em, int* out, int wait, u32 sz)
{
    return EvtReadSub(nm, 0, em, out, wait, sz);
}

// 1 when the event name is one of the 37 cutscenes with a separate Ashley-armour version (only in
// game_costume 1).
int EventMgr::NameCheck(char* nm)
{
    char tbl[37][0x20] = {
        "r105s10", "r117s00", "r117s10", "r11cs00", "r11cs10", "r11fs00", "r200s00", "r201s00", "r203s00", "r204s00",
        "r206s10", "r206s20", "r20bs00", "r212s00", "r213s00", "r214s00", "r215s00", "r215s01", "r22as00", "r300s00",
        "r304s00", "r30as00", "r30bs00", "r30cs00", "r310s00", "r316s00", "r317s05", "r325s00", "r329s00", "r330s00",
        "r331s00", "r331s10", "r332s00", "r332s10", "r332s20", "r333s00", "r333s10",
    };
    int i;
    int n = 37;

    if (pG->game_costume != 1) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (strstr(nm, tbl[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

// Returns the file name to load: in game_costume 1 the "rXXXsYY" of a NameCheck event becomes "sXXXsYY".
char* EventMgr::NameChange(char* nm)
{
    char* p;

    if (strlen(nm) > 0x1F) {
        pLog->err(0, 0, "EventMgr::EvtRead : Name size long failed [%s]", nm);
        return nm;
    }
    strcpy(NameTmp, nm);
    if (pG->game_costume == 1) {
        p = strchr(NameTmp, 'r');
        if (p != 0 && NameCheck(p) == 1) {
            *p = 's';
        }
    }
    return NameTmp;
}

// Loads event file `nm` through the data cache: registers a read slot, and either queues an ARAM
// load (aram != 0) or loads to MRAM; with an enemy module `em` the event is swapped into that
// module's archive memory (MemorySwap, EspEmDataSwapPush) so it borrows the enemy's space.
// *out receives the address. Returns 0 on any failure (logged).
int EventMgr::EvtReadSub(char* nm, int aram, int em, int* out, int wait, u32 sz)
{
    cDataUnit* unit = 0;
    u32 no = 0;
    int fresh = 0;
    char* p;
    u32 size;
    void* r;
    void* addr;
    ReadModule* mod;

    if (out != 0) {
        *out = 0;
    }
    if (GetRead((void**) &unit, (int*) &no, nm) == 0) {
        p = strstr(nm, "evd/");
        if (p == 0) {
            pLog->err(0, 0, "EventMgr::EvtRead : Name failed [%s]", nm);
            return 0;
        }
        p = NameChange(p);
        unit = DC.setData(p);
        if (unit == 0) {
            pLog->err(0, 0, "EventMgr::EvtRead : DC.setData failed [%s]", p);
            return 0;
        }
        if (SetRead(nm, (int*) &no, unit) == 0) {
            pLog->err(0, 0, "EventMgr::EvtRead : SetRead failed");
            return 0;
        }
        fresh = 1;
    }
    if (no > 7) {
        DelRead(nm);
        pLog->err(0, 0, "EventMgr::EvtRead : WkNo failed [%d]", no);
        return 0;
    }
    readEm[no].em = em;
    readEm[no].swapped = 0;
    if (aram == 0) {
        if (em != 0) {
            if (fresh == 1) {
                if (sz > unit->m_size) {
                    size = sz;
                } else {
                    size = unit->m_size;
                }
                r = EmReadSearch(em, 0, size);
                if (out != 0) {
                    *out = (int) r;
                }
                unit->setCommand(CMND_ARAM_LOAD, 0, 1);
            }
            if (unit->waitLoadOk() == 0) {
                unit->setCommand(CMND_CLEAR_DATA, 0, 0);
                DelRead(nm);
                pLog->err(0, 0, "readEvent() : out of memory (0x%x)[%s]", unit->m_size, nm);
                return 0;
            }
            EspEmDataSwapPush(em);
            mod = SearchEmModule(em);
            if (mod == 0) {
                DelRead(nm);
                pLog->err(0, 0, "EventMgr::EvtRead : no id SearchEmModule [%x]", em);
                return 0;
            }
            if (unit->m_size > mod->size) {
                DelRead(nm);
                pLog->err(0, 0, "EventMgr::EvtRead : event size too large!![%d]>[%d]", unit->m_size, mod->size);
                return 0;
            }
            MemorySwap(mod->pArc, (u32) unit->m_addr, unit->m_size);
            readEm[no].swapped = 1;
            r = mod->pArc;
            if (out != 0) {
                *out = (int) r;
            }
        } else {
            unit->setCommand(CMND_MRAM_LOAD, 0, 1);
            if (unit->waitUseOk() == 0) {
                unit->setCommand(CMND_CLEAR_DATA, 0, 0);
                DelRead(nm);
                pLog->err(0, 0, "readEvent() : out of memory (0x%x)[%s]", unit->m_size, nm);
                return 0;
            }
            addr = unit->m_addr;
            if (out != 0) {
                *out = (int) addr;
            }
        }
    } else {
        if (em != 0) {
            if (sz > unit->m_size) {
                size = sz;
            } else {
                size = unit->m_size;
            }
            r = EmReadSearch(em, 0, size);
            if (out != 0) {
                *out = (int) r;
            }
        }
        if (wait == 0) {
            unit->setCommand(CMND_ARAM_LOAD, 0, 0);
        } else {
            unit->setCommand(CMND_ARAM_LOAD, 0, 1);
        }
    }
    return 1;
}

// The scenario's "play event" call: marks the event state, loads the file (MRAM, into module `em`),
// creates the event with the option bits (2 died demo + keep, 0x40 keep alive, 0x20 no player
// reposition, 0x10 auto fade, 0x80 true scenario start, 0x100 no partner recall, 4 fade in after,
// 0x200 wait for SceCheckEventStart), sleeps until it is gone, then frees the file and clears the
// event state (unless kept). Returns 0 when the load failed.
int EventMgr::EvtReadExec(char* nm, int em, u32 flags)
{
    int addr;
    Event* evt;
    int ret = 1;

    if (flags & 0x200) {
        while (SceCheckEventStart() != 1) {
            SceSleep(1);
        }
    }
    if (flags & 0x80) {
        pLog->mes(0, 0, "EventMgr::EvtReadExec : SceEventStart(true)");
        SceEventStart(1);
    } else {
        SceEventStart(0);
    }
    StaFlagOn(pG, STA_EVENT_SYSYTEM);
    StaFlagOn(pG, STA_EFFAREA_USE_CAM);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    if (em != 0) {
        SceSleep(2);
    }
    if (EvtReadMram(nm, em, &addr, 0, 0)) {
        if (EvtMgr.SetEvt((void*) addr, (u32*) &evt)) {
            if (flags & 2) {
                BitOn(evt->StatusFlag, 0x00100000);
                BitOn(evt->StatusFlag, 0x200);
            }
            if (flags & 0x40) {
                BitOn(evt->StatusFlag, 0x00100000);
            }
            if (flags & 0x20) {
                BitOn(evt->StatusFlag, 0x800);
            }
            if (flags & 0x10) {
                BitOn(evt->StatusFlag, 0x400);
            }
            if (flags & 0x80) {
                BitOn(evt->StatusFlag, 0x80);
            }
            if (flags & 0x100) {
                BitOn(evt->StatusFlag, 0x40);
            }
        }
        if (flags & 4) {
            SceSleep(1);
            FadeSetW(0x80000002, 0x1E, 0, 0);
        }
        {
            EventMgr* m = &EvtMgr;
            while (IsAliveEvt(&m->NowExeEvtKey, 0, 0) != 0) {
                SceSleep(1);
            }
        }
        if (flags & 2) {
            return 1;
        }
        if (flags & 0x40) {
            return 1;
        }
        EvtFree(nm);
    } else {
        pLog->err(0, 0, "EventMgr::EvtReadExec : mem over");
        ret = 0;
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    StaFlagOff(pG, STA_EVENT_SYSYTEM);
    StaFlagOff(pG, STA_EFFAREA_USE_CAM);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    return ret;
}

// Releases a loaded event file: swaps the enemy module memory back if it was borrowed and clears
// the data cache unit.
int EventMgr::EvtFree(char* nm)
{
    cDataUnit* unit = 0;
    u32 no = 0;
    int em;
    ReadModule* mod;

    if (GetRead((void**) &unit, (int*) &no, nm) == 0) {
        pLog->err(0, 0, "EventMgr::EvtFree : NameEvt failed [%s]", nm);
        return 0;
    }
    DelRead(nm);
    if (no > 7) {
        pLog->err(0, 0, "EventMgr::EvtFree : WkNo failed [%d]", no);
        return 0;
    }
    em = readEm[no].em;
    if (unit != 0) {
        if (unit->waitLoadOk() == 0) {
            pLog->err(0, 0, "EvtFree() : out of memory (0x%x)[%s]", unit->m_size, nm);
        }
        if (em != 0 && readEm[no].swapped == 1) {
            mod = SearchEmModule(em);
            MemorySwap(mod->pArc, (u32) unit->m_addr, unit->m_size);
            readEm[no].swapped = 0;
            EspEmDataSwapPop(em);
        }
        unit->setCommand(CMND_CLEAR_DATA, 0, 0);
    }
    return 1;
}

// t_event tool: drops all evd and bin registrations.
void EventMgr::ToolCoreEvdDel()
{
    EvdTbl.DelAll(0);
    BinTbl.DelAll(0);
}

// Starts an event from a loaded "event" block: validates the tag, registers it (SetEvd) and creates
// the Event; *key receives it. Refused while Stop_flg 0x400 or Debug_flg[3] 0x80.
int EventMgr::SetEvt(void* data, u32* key)
{
    Event* evt;
    EvtHeader* hdr = (EvtHeader*) data;

    if (SpfFlagChk(pG, SPF_EVT)) {
        return 0;
    }
    if (DbgFlagChk(pG, DBG_NO_EVENT)) {
        return 0;
    }
    if (key != 0) {
        *key = 0;
    }
    if ((int) hdr >= 0) {
        pLog->err(0, 0, "EventMgr::SetEvs : non addr[%x]", hdr);
        return 0;
    }
    if (*(u32*) hdr->tag != 0x6576656E || hdr->tag[4] != 't') {
        pLog->err(0, 0, "EventMgr::SetEvt : invalid data");
        return 0;
    }
    if (EvtMgr.SetEvd((char*) hdr, hdr, 0, 2) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : SetEvd failed[%s]", hdr);
        return 0;
    }
    if (EvtMgr.SetEvt((char*) hdr, &evt) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : SetEvt failed[%s]", hdr);
        return 0;
    }
    if (key != 0) {
        *key = (u32) evt;
    }
    return 1;
}

// Creates and inits the Event for registered evd `nm`, marks it begin-pending (0x01000000) and records
// it as the running event name.
int EventMgr::SetEvt(char* nm, Event** out)
{
    void* evd;
    Event* evt;

    if (SpfFlagChk(pG, SPF_EVT)) {
        return 0;
    }
    if (DbgFlagChk(pG, DBG_NO_EVENT)) {
        return 0;
    }
    if (out != 0) {
        *out = 0;
    }
    evt = create(0);
    if (evt == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : create failed[%s]", nm);
        return 0;
    }
    if (GetEvd(&evd, nm, 0) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : non read[%s]", nm);
        DelEvt(evt, 0);
        return 0;
    }
    if (evt->init(nm, (EvtHeader*) evd) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : init failed[%s]", nm);
        DelEvt(evt, 0);
        return 0;
    }
    evt->StatusFlag |= 0x01000000;
    {
        EventMgr* m = &EvtMgr;
        strcpy(m->NowExeEvtName, nm);
    }
    if (out != 0) {
        *out = evt;
    }
    return 1;
}

// Finds a live event by name (parked ones included).
int EventMgr::GetEvt(u32* key, void** out)
{
    return IsAliveEvt(key, (int) out, 1);
}

// Ends and destroys an event: ExeEndEvt, then (flag == 1) one frame later the unit is destroyed, its
// evd unregistered and, if a cancel fade is up, the screen faded back in.
int EventMgr::DelEvt(void* evt_, int flag)
{
    char nm[0x20];
    Event* evt = (Event*) evt_;
    int fade = EvtChk(evt->StatusFlag, 0x04000000);
    int zero;

    switch (evt->EndRNo2) {
    case 0:
        evt->ExeEndEvt(evt, 0);
        if (flag == 1) {
            SysFlagOn(pG, SYS_SCREEN_STOP);
            evt->EndRNo3 = 0;
            evt->EndRNo2++;
            return 1;
        }
        break;
    case 1:
        evt->EndRNo3++;
        if (evt->EndRNo3 <= 0) {
            return 1;
        }
        SysFlagOff(pG, SYS_SCREEN_STOP);
        break;
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    {
        char* p = nm;
        strcpy(p, evt->Name);
        destroyNow(evt);
        DelEvd(p);
    }
    strcpy(NowExeEvtName, "");
    zero = 0; // COMPILER-DIFF: #13 (single-use zero set in another block: update_equiv_regs moves the li to the store, r0)
    if (fade) {
        FadeKill(FADE_NO_ROOM);
        {
            u32 col[2];
            u32* c = col;
            c[0] = 0xFF;
            c[1] = zero; // FadeSetW written out: the zero-offset store folds to the frame, `4(P)` keeps P tied to r4
            FadeSet(0x80000001, (GXColor*) c, (GXColor*) (c + 1), 0xA, 0, 0);
        }
    }
    return 1;
}

// Registers a named data block (model/motion/camera/... file) for the event packets.
int EventMgr::SetBin(char* nm, void* data, void* dat2, int flag)
{
    if ((int) data >= 0) {
        pLog->err(0, 0, "EventMgr::SetBin : non addr[%s]", nm);
        return 0;
    }
    if (BinTbl.SetDat(nm, data, 7, dat2, flag, 0) == 0) {
        pLog->err(0, 0, "EventMgr::SetBin : failed");
        return 0;
    }
    return 1;
}

// Looks a registered bin up by name; in the tool (flagGet) a missing one is read from x:/soft/room/.
int EventMgr::GetBin(void** out, const char* nm, int flagGet)
{
    u8 type;
    void* dat;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    if (BinTbl.GetDat(&dat, &type, nm, 0) == 0) {
        char path[0x100];
        pLog->warn(0, 0, "EventMgr::GetBin : non data[%s]", nm);
        if (flagGet == 0) {
            strcpy(path, "x:/soft/room/");
            strcat(path, nm);
            if (HDReadDebugAlloc(path, &dat, 1) == 0) {
                pLog->err(0, 0, "EventMgr::GetBin : non read[%s]", path);
                return 0;
            }
            if (SetBin((char*) nm, dat, dat, 2) == 0) {
                pLog->err(0, 0, "EventMgr::GetBin : non SetBin[%s]", path);
                return 0;
            }
        } else {
            pLog->err(0, 0, "EventMgr::GetBin : non read[%s]", nm);
            return 0;
        }
    }
    *out = dat;
    return 1;
}

// Unregisters a bin.
int EventMgr::DelBin(char* nm)
{
    if (BinTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "EventMgr::DelBin : failed");
        return 0;
    }
    return 1;
}

// Registers an event data block and every bin listed in its header table.
int EventMgr::SetEvd(char* nm, void* data, void* dat2, int flag)
{
    EvtHeader* hdr = (EvtHeader*) data;
    EvtBinEntry* e;
    int i;

    if ((int) hdr >= 0) {
        pLog->err(0, 0, "EventMgr::SetEvd : non addr[%s]", nm);
        return 0;
    }
    if (*(u32*) hdr->tag != 0x6576656E || hdr->tag[4] != 't') {
        pLog->err(0, 0, "EventMgr::SetEvd : invalid data[%s]", nm);
        return 0;
    }
    if (EvdTbl.ChkDat(nm) == 1) {
        return 1;
    }
    if (EvdTbl.SetDat(nm, hdr, 8, dat2, flag, 0) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvd : failed");
        return 0;
    }
    for (i = 0; i < hdr->nBin; i++) {
        e = (EvtBinEntry*) (i * sizeof(EvtBinEntry) + (hdr->binOfs + (u32) hdr));
        if (SetBin(e->name, (u8*) (e->ofs + (u32) hdr), 0, flag) == 0) {
            pLog->err(0, 0, "EventMgr::SetEvd : failed");
            return 0;
        }
    }
    return 1;
}

// Looks a registered evd up by name (tool: read from disk when missing).
int EventMgr::GetEvd(void** out, char* nm, int flagGet)
{
    u8 type;
    void* dat;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    if (EvdTbl.GetDat(&dat, &type, nm, 0) == 0) {
        char path[0x100];
        pLog->warn(0, 0, "EventMgr::GetEvd : non data[%s]", nm);
        if (flagGet == 0) {
            strcpy(path, "x:/soft/room/");
            strcat(path, nm);
            if (HDReadDebugAlloc(path, &dat, 1) == 0) {
                pLog->err(0, 0, "EventMgr::GetEvd : non read[%s]", path);
                return 0;
            }
            if (SetEvd(nm, dat, dat, 2) == 0) {
                pLog->err(0, 0, "EventMgr::GetEvd : non SetBin[%s]", path);
                return 0;
            }
        } else {
            pLog->err(0, 0, "EventMgr::GetEvd : non read[%s]", nm);
            return 0;
        }
    }
    *out = dat;
    return 1;
}

// Unregisters an evd and all its bins.
int EventMgr::DelEvd(char* nm)
{
    EvtHeader* hdr;
    int i;

    if (GetEvd((void**) &hdr, nm, 1) == 0) {
        pLog->err(0, 0, "EventMgr::GetEvd : non read[%s]", nm);
        return 0;
    }
    for (i = 0; i < hdr->nBin; i++) {
        DelBin(((EvtBinEntry*) (hdr->binOfs + (u32) hdr))[i].name);
    }
    if (EvdTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "EventMgr::DelEvd : failed");
        return 0;
    }
    return 1;
}

// Registers a room's event function table under the event name (rooms call this at init).
int EventMgr::SetFunc(char* nm, void* func)
{
    if (FuncTbl.SetDat(nm, func, 0, 0, 0, 0) == 0) {
        pLog->err(0, 0, "EventMgr::SetFunc : failed");
        return 0;
    }
    return 1;
}

// Never called (only its string survives in .rodata).
static inline int EventMgrDelFunc(EventMgr* mgr, char* nm)
{
    if (mgr->FuncTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "EventMgr::DelFunc : failed");
        return 0;
    }
    return 1;
}

// Looks an event function (table) up by name.
int EventMgr::GetFunc(void** out, char* nm)
{
    u8 type;
    void* f;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    if (FuncTbl.GetDat(&f, &type, nm, 0) == 0) {
        return 0;
    }
    *out = f;
    return 1;
}

// Registers a loading data cache unit under the event file name; *wkNo = its slot.
int EventMgr::SetRead(char* nm, int* wkNo, void* unit)
{
    int no = 0;

    if (wkNo == 0) {
        return 0;
    }
    *wkNo = 0;
    if (ReadTbl.SetDat(nm, unit, 0, 0, 2, &no) == 0) {
        pLog->err(0, 0, "EventMgr::SetRead : failed");
        return 0;
    }
    *wkNo = no;
    return 1;
}

// Finds the data cache unit of a loading event file.
int EventMgr::GetRead(void** out, int* wkNo, char* nm)
{
    u8 type;
    int d;

    if (out == 0 || wkNo == 0) {
        return 0;
    }
    *out = 0;
    *wkNo = 0;
    if (ReadTbl.GetDat((void**) &d, &type, nm, 0) == 0) {
        return 0;
    }
    *out = (void*) d;
    if (ReadTbl.GetWkNo(&d, nm) == 0) {
        return 0;
    }
    *wkNo = d;
    return 1;
}

// Unregisters a read slot.
int EventMgr::DelRead(char* nm)
{
    if (ReadTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "EventMgr::DelRead : failed");
        return 0;
    }
    return 1;
}

// Registers every event contained in an "evs" bundle (table of offsets) as evd.
int EventMgr::SetEvs(void* evs)
{
    EvsHeader* hdr = (EvsHeader*) evs;
    EvsEntry* tbl;
    u8* p;
    int i;

    if ((int) hdr >= 0) {
        pLog->err(0, 0, "EventMgr::SetEvs : non addr");
        return 0;
    }
    tbl = (EvsEntry*) (hdr->tblOfs + (u32) hdr);
    for (i = 0; i < hdr->num; i++) {
        EvsEntry* e = &tbl[i];
        p = (u8*) (e->ofs + (u32) hdr);
        if (SetEvd((char*) p, p, 0, 0) == 0) {
            pLog->err(0, 0, "EventMgr::SetEvs : SetEvd failed[%s]", p);
            return 0;
        }
    }
    return 1;
}

// Event stream slot accessors through an integer base: `evt->strNo[blk]` forces `evt + 0xC0` into a
// pointer-flagged temp (regclass then wants the index in GENERAL_REGS, r0); a `u32` base variable
// keeps both unflagged so the shifted index takes a BASE register (`lwzx r29,r10,r11`).
static inline int& evtStrNo(Event* evt, int blk) { u32 p = (u32) evt->NowStr; return *(int*) (p + (blk << 2)); }
static inline u32& evtStrId(Event* evt, int blk) { u32 p = (u32) evt->SndId; return *(u32*) (p + (blk << 2)); }

// Stops the stream playing in block `blk` of the named event and waits for it to end (mode 1 also
// waits for the request to clear); clears the block's slot.
int EventMgr::EvtSndStrStop(u32* key, int blk, int mode)
{
    Event* evt;
    int no;
    u32 id;

    if (GetEvt(key, (void**) &evt) != 1) {
        return 0;
    }
    no = evtStrNo(evt, blk);
    id = evtStrId(evt, blk);
    if (no != -1) {
        if (id != 0) {
            if (SndStrReq(id, 8, 0, 0) == 1) {
                do {
                    if (mode == 0) {
                        break;
                    }
                    if (mode == 1) {
                        TaskSleep(1);
                    }
                    if (mode == 2) {
                        SceSleep(1);
                    }
                } while (SndStrStatusCk(id, 0x10) != 0);
            }
        } else {
            if (SndStrReq(blk, no, 8, 0, 0, 0.0f) == 1) {
                do {
                    if (mode == 0) {
                        break;
                    }
                    if (mode == 1) {
                        TaskSleep(1);
                    }
                    if (mode == 2) {
                        SceSleep(1);
                    }
                } while (SndStrStatusCk(blk, no, 0x10) != 0);
            }
        }
        evtStrId(evt, blk) = 0;
        evtStrNo(evt, blk) = -1;
        OSReport("EventMgr::EvtSndStrStop : stop (%d)\n", blk);
        return 1;
    }
    return 0;
}

// Starts stream `no` in block `blk` for the named event (mode 1: wait until it is playing, then
// unpause); block 0 goes through the room BGM start. Records the id/number in the event and EvtDebug.
void EventMgr::EvtSndStrPlay(u32* key, int blk, int no, int mode, f32 vol)
{
    Event* evt;
    u32 id;
    int cnt;

    if (GetEvt(key, (void**) &evt) == 1) {
        id = 0;
        if (no == -1) {
            pLog->err(0, 0, "EventMgr::EvtSndStrPlay noStr == TarNon");
            return;
        }
        if (blk == 0) {
            SndRoomStrStart(1, no, 1);
        } else if (mode == 0) {
            SndStrReq(blk, no, 0x80000003, 0, 0, 0.0f);
        } else {
            id = SndStrReq(blk, no, 1, 0, 0, vol);
            if (id != 0) {
                cnt = 0;
                for (;;) {
                    if (mode == 1) {
                        TaskSleep(1);
                    }
                    if (mode == 2) {
                        SceSleep(1);
                    }
                    if (SndStrStatusCk(id, 2) == 1) {
                        break;
                    }
                    cnt++;
                    if (cnt > 0x95) {
                        SndStrReq(id, 8, 0, 0);
                        pLog->err(0, 0, "EventMgr::EvtSndStrPlay : sleep timer over");
                        return;
                    }
                }
                SndStrReq(id, 2, 0, 0);
            }
        }
        evtStrId(evt, blk) = id;
        evtStrNo(evt, blk) = no;
        EvtDebug.NowStr[blk] = no;
        OSReport("EventMgr::EvtSndStrPlay : start (%d)-(%d)\n", blk, no);
    }
}

// World position of the model's root parts snapped to the floor, and the heading of its child parts:
// where the real player is put when an event body ends.
int EventMgr::GetZeroPartsWorldPos(cModel* m, Vec* pos, Vec* rot)
{
    Vec v;
    EvtMtx mtx;
    cModel* parts = m->pParts;

    if (parts == 0) {
        return 0;
    }
    pos->x = parts->world.x;
    pos->y = parts->world.y;
    pos->z = parts->world.z;
    pos->y = SatMgr.getFloor(pos, 600.0f, 100000.0f, 0, 0);
    if (parts->pParts == 0) {
        return 0;
    }
    v.x = 0.0f;
    v.z = 1.0f;
    v.y = 0.0f;
    mtx = *(EvtMtx*) parts->pParts->mat;
    mtx.m[0][3] = 0.0f;
    mtx.m[1][3] = 0.0f;
    mtx.m[2][3] = 0.0f;
    PSMTXMultVec(mtx.m, &v, &v);
    rot->y = LIMIT_ANGLE(atan2f(v.x, v.z));
    rot->x = 0.0f;
    rot->z = 0.0f;
    return 1;
}

// Clears the three window-break camera (FCV) pointers.
void EventMgr::ClearEmWindowFcv()
{
    emWindowFcv[0] = 0;
    emWindowFcv[1] = 0;
    emWindowFcv[2] = 0;
}

// Stores the window-break camera data (window 1 in/out, window 2 out) for the room.
void EventMgr::SetEmWindowFcv(void* a, void* b, void* c)
{
    emWindowFcv[0] = a;
    emWindowFcv[1] = b;
    emWindowFcv[2] = c;
}

// Returns the window-break camera data.
void EventMgr::GetEmWindowFcv(void** win1FIn, void** win1FOut, void** win2FOut)
{
    if (win1FIn != 0) {
        *win1FIn = emWindowFcv[0];
    }
    if (win1FOut != 0) {
        *win1FOut = emWindowFcv[1];
    }
    if (win2FOut != 0) {
        *win2FOut = emWindowFcv[2];
    }
}

// t_event tool state; nothing to construct.
EventDebug::EventDebug()
{
}

// Nothing to release.
EventDebug::~EventDebug()
{
}

// Room init: clears the tool's disable bits (FlagEtc).
int EventDebug::myRoomInit()
{
    FlagEtc = 0;
    return 1;
}

// Clears the 0x60 model file records of the tool.
void EventDebug::ClrModelFiles()
{
    int i;

    for (i = 0; i < 0x60; i++) {
        memset_asm(&pModel[i], 0, sizeof(EvtDebugModel));
    }
}

// Adds a bin/tpl file pair to model record `no`.
int EventDebug::AddNameBinTpl(int no, char* bin, char* tpl)
{
    EvtDebugModel* m = &pModel[no];
    int n = m->nBin;

    if (n > 0xF) {
        pLog->err(0, 0, "EventDebug::AddNameBinTpl : num failed");
        return 0;
    }
    strcpy(m->bin[n], bin);
    strcpy(m->tpl[n], tpl);
    m->nBin++;
    return 1;
}

// Name -> data table; empty until init.
DatTbl::DatTbl()
{
    pWork = 0;
}

// Frees the table.
DatTbl::~DatTbl()
{
    end();
}

// Allocates n entries (memory group 13).
int DatTbl::init(int n)
{
    NumDatTbl = n;
    if (n < 0) {
        NumDatTbl = 0;
    }
#line 5749 "D:/Bio4/Prog/event.cpp"
    pWork = (DatTblEntry*) MEM_ALLOC(NumDatTbl * sizeof(DatTblEntry), 1, 0xD);
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::init : memory failed");
        return 0;
    }
    memclr_asm(pWork, NumDatTbl * sizeof(DatTblEntry));
    return 1;
}

// Frees the entries.
int DatTbl::end()
{
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::end : memory failed");
        return 0;
    }
    if (DelAll(1) == 0) {
        pLog->err(0, 0, "cDatTbl::end : failed");
        return 0;
    }
    Mem_free(pWork);
    pWork = 0;
    return 1;
}

// Registers (name, data, type); a name already present only bumps its reference Count. *wkNo = slot.
int DatTbl::SetDat(const char* nm, void* dat, u8 type, void* dat2, u8 flag, int* wkNo)
{
    int i;

    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::SetDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::SetDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < NumDatTbl; i++) {
        if ((pWork[i].FlagBe8 & 1) && strcmp(pWork[i].Name, nm) == 0) {
            pWork[i].Count++;
            return 1;
        }
    }
    for (i = 0; i < NumDatTbl; i++) {
        if (!(pWork[i].FlagBe8 & 1)) {
            memclr_asm(&pWork[i], sizeof(DatTblEntry));
            pWork[i].FlagBe8 = flag | 1;
            strcpy(pWork[i].Name, nm);
            pWork[i].Dat = dat;
            pWork[i].Etc = type;
            pWork[i].dat2 = dat2;
            pWork[i].Count = 1;
            if (wkNo != 0) {
                *wkNo = i;
            }
            return 1;
        }
    }
    pLog->err(0, 0, "cDatTbl::SetDat : non space[%s]", nm);
    return 0;
}

// Finds an entry by name; 0 when absent.
int DatTbl::GetDat(void** dat, u8* type, const char* nm, int* wkNo)
{
    int i;

    if (dat == 0 || type == 0) {
        return 0;
    }
    *dat = 0;
    *type = 0;
    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::GetDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::GetDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < NumDatTbl; i++) {
        if ((pWork[i].FlagBe8 & 1) && strcmp(pWork[i].Name, nm) == 0) {
            *dat = pWork[i].Dat;
            *type = pWork[i].Etc;
            if (wkNo != 0) {
                *wkNo = i;
            }
            return 1;
        }
    }
    return 0;
}

// 1 when the name is registered.
int DatTbl::ChkDat(const char* nm)
{
    int i;

    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::ChkDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::ChkDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < NumDatTbl; i++) {
        if ((pWork[i].FlagBe8 & 1) && strcmp(pWork[i].Name, nm) == 0) {
            return 1;
        }
    }
    return 0;
}

// Slot number of a name.
int DatTbl::GetWkNo(int* wkNo, const char* nm)
{
    int i;

    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::ChkDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::ChkDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < NumDatTbl; i++) {
        if ((pWork[i].FlagBe8 & 1) && strcmp(pWork[i].Name, nm) == 0) {
            *wkNo = i;
            return 1;
        }
    }
    return 0;
}

// Table capacity (slots to scan).
int DatTbl::GetNumDat()
{
    return NumDatTbl;
}

// Entry by slot; 0 when the slot is empty.
int DatTbl::GetDatWkNo(void** dat, u8* type, int wkNo)
{
    if (dat == 0) {
        return 0;
    }
    *dat = 0;
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::GetDatWkNo : memory failed[%d]", wkNo);
        return 0;
    }
    if (wkNo >= NumDatTbl) {
        pLog->err(0, 0, "cDatTbl::GetDatWkNo : work_no failed[%d]", wkNo);
        return 0;
    }
    if (pWork[wkNo].FlagBe8 & 1) {
        *dat = pWork[wkNo].Dat;
        *type = pWork[wkNo].Etc;
        return 1;
    }
    return 0;
}

// 1 when slot wkNo holds the given name.
int DatTbl::ChkDatWkNoName(int wkNo, const char* nm)
{
    DatTblEntry* e;

    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::GetDatWkNo : memory failed[%d]", wkNo);
        return 0;
    }
    if (wkNo >= NumDatTbl) {
        pLog->err(0, 0, "cDatTbl::GetDatWkNo : work_no failed[%d]", wkNo);
        return 0;
    }
    e = (DatTblEntry*) (wkNo * sizeof(DatTblEntry) + (u32) pWork);
    if ((e->FlagBe8 & 1) && strcmp(e->Name, nm) == 0) {
        return 1;
    }
    return 0;
}

// Drops one reference of the slot; frees it (and its debug dat2 buffer) when the count reaches 0.
int DatTbl::DelDatWkNo(int wkNo)
{
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::DelDatWkNo : memory failed[%d]", wkNo);
        return 0;
    }
    if (wkNo >= NumDatTbl) {
        pLog->err(0, 0, "cDatTbl::DelDatWkNo : work_no failed[%d]", wkNo);
        return 0;
    }
    if (pWork[wkNo].FlagBe8 & 1) {
        pWork[wkNo].Count--;
        if ((s16) pWork[wkNo].Count <= 0 && (pWork[wkNo].FlagBe8 & 2)) {
            if (pWork[wkNo].dat2 != 0) {
                Debug_free(pWork[wkNo].dat2);
            }
            memclr_asm(&pWork[wkNo], sizeof(DatTblEntry));
        }
        return 1;
    }
    pLog->err(0, 0, "cDatTbl::DelDatWkNo : non dat[%d]", wkNo);
    return 0;
}

// Drops one reference of the named entry.
int DatTbl::DelDat(const char* nm)
{
    int i;

    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::DelDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::DelDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < NumDatTbl; i++) {
        if ((pWork[i].FlagBe8 & 1) && strcmp(pWork[i].Name, nm) == 0) {
            pWork[i].Count--;
            if ((s16) pWork[i].Count <= 0 && (pWork[i].FlagBe8 & 2)) {
                if (pWork[i].dat2 != 0) {
                    Debug_free(pWork[i].dat2);
                }
                memclr_asm(&pWork[i], sizeof(DatTblEntry));
            }
            return 1;
        }
    }
    pLog->err(0, 0, "cDatTbl::DelDat : non dat[%s]", nm);
    return 0;
}

// Clears every entry (all != 0 also the ones flagged permanent).
int DatTbl::DelAll(int all)
{
    int i;

    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::DelAll : memory failed");
        return 0;
    }
    for (i = 0; i < NumDatTbl; i++) {
        if ((pWork[i].FlagBe8 & 1) && ((pWork[i].FlagBe8 & 2) || all == 0)) {
            if (pWork[i].dat2 != 0) {
                Debug_free(pWork[i].dat2);
            }
            memclr_asm(&pWork[i], sizeof(DatTblEntry));
        }
    }
    return 1;
}

// Starts stream `no` in block `blk` and waits until it is playing (scenario helper).
int SndStrPlayBlock(int blk, int no, f32 vol)
{
    u32 id = SndStrReq(blk, no, 1, 0, 0, vol);

    if (id != 0) {
        do {
            SceSleep(1);
        } while (SndStrStatusCk(id, 2) != 1);
        SndStrReq(id, 2, 0, 0);
    }
    return id;
}

// Stops the stream in block `blk` and waits for it to end.
void SndStrStopBlock(int blk)
{
    u32 id = blk;

    if (SndStrReq(id, 8, 0, 0) == 1) {
        do {
            SceSleep(1);
        } while (SndStrStatusCk(id, 0x10) != 0);
    }
}

// Six unreferenced zero-initialised words (Bio4.sym has no name for them).
int lbl_803149C8 = 0;
int lbl_803149CC = 0;
int lbl_803149D0 = 0;
int lbl_803149D4 = 0;
int lbl_803149D8 = 0;
int lbl_803149DC = 0;
