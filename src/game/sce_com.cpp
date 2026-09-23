// game/sce_com: scenario helpers shared by the room scripts — the event brackets (SceEventStart /
// SceEventEnd put the whole game into event mode, SceUpCutStart / End freeze it for a close-up),
// messages with camera cuts and yes/no selection, save scratch words, enemy counting / destruction,
// item events (an action button that reveals items), the chapter-end screen, the scenario
// camera, container opening (OpenBoxMain) and the elevator script (SceElevator).
#include "types.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "card.h"
#include "dmg.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "scheduler.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "sce.h"
#include "em.h"
#include "em_set.h"
#include "obj.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "cockpit.h"
#include "id_sys.h"
#include "mes.h"
#include "snd.h"
#include "fade.h"
#include "pad.h"
#include "est.h"
#include "shadow.h"
#include "room_data.h"
#include "cDataSwap.h"
#include "dvd.h"
#include "scroll.h"
#include "rnd.h"
#include "math_sub.h"
#include "sscrn.h"
#include "option.h"
#include "game.h"
#include "eprintf.h"
#include "db_log.h"
#include "va_ppc.h"
#include <dolphin/os.h>

// Scenario helpers shared by the room scripts: event brackets, messages, chapter end, elevators.

extern "C" {
void* __builtin_new(unsigned int size);
void __builtin_delete(void* p);
}


// HALT() (db_log.h) with the __FILE__ string materialised first: the string lands in .rodata before
// "HALT %s(%d)\n" here, the other way round in every other unit.
#define HALT_FILE_FIRST()                                         \
    {                                                             \
        const char* file_ = __FILE__;                             \
        OSReport("HALT %s(%d)\n", file_, __LINE__);               \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// One pending item event (SceSetItemEvent), 0x20 bytes, new'd.
struct SceItemEvent {
    u8 flag;              // 0x00  room save flag set when the event ran
    u8 pad_1;
    s16 atNo;             // 0x02  trigger area
    s16 cut;              // 0x04  camera cut (-1 = none)
    s16 item[8];          // 0x06  item areas enabled by the event (-1 = none)
    void (*func)(int);    // 0x18
    int arg;              // 0x1C
};

// Elevator script data (SceElevator task argument).
struct SceElevatorData {
    s32 dir;              // 0x00  0/2: arrive, 1/3: leave (1/0 move down)
    u32 objId;            // 0x04  scroll object of the cage
    Vec pos;              // 0x08  cage rest position
    Vec plPos;            // 0x14  player position on the cage
    Vec plRot;            // 0x20
    s32 cut;              // 0x2C  camera cut (-1 = none)
    u16 pad_30;
    u16 seStart;          // 0x32
    u16 pad_34;
    u16 seStop;           // 0x36
    Vec jumpPos;          // 0x38  room jump destination
    Vec jumpRot;          // 0x44
    u16 room;             // 0x50
};

static void* ItemEventTbl[16];
static Camera SceCam;

// Begins a scenario event (nestable; only the outermost call acts): the calling scenario task is
// marked as an event task, mode 0 puts every enemy / object / damage area into event mode, kills
// the effects and the level-5 tasks, mode 1 only stops the ladder camera task; the aim camera
// ends, keys are stopped (0xEFCF0000 kept), the life meter and cockpit ids hide, the player is
// invulnerable, Stop_flg 0x100 / 0x400000 and SE block 2 stops. Status_flg[0] 0x1000 = in event.
void SceEventStart(int mode)
{
    cSceSys* s;

    if (SceSys.checkCTaskRange() == 1) {
        s = &SceSys;
        if (s->task_kind_back == 0) {
            s->task_kind_back = SceCTask()->task->flag;
            SceCTask()->task->flag |= 2;
        }
    }
    if (SceSys.event_start_cnt != 0) {
        SceSys.event_start_cnt++;
        return;
    }
    SceSys.event_start_cnt++;
    s = &SceSys;
    s->system_bak = pG->System_flg;
    if (mode == 0) {
        EmMgr.beginEvent(0);
        ObjMgr.beginEvent(0);
        s->event_no_cut_back = 1;
        EffectEventDelete();
        DmgMgr.beginEvent(0);
        SceKill(5);
    } else {
        if (s->pLadderTask) {
            SceKill(s->pLadderTask);
        }
    }
    PlEndCamera();
    LightMgr.beginEvent();
    StaFlagOn(pG, STA_EVENT);
    StaFlagOn(pG, STA_SUSPEND);
    KeyStop(0xEFCF0000);
    if (StaFlagChk(pG, STA_BINOCULAR)) {
        CamCtrl.LowerBinocular();
    }
    Cckpt.lifeMeterDisp(0);
    IdSys.dispSw(IDC_LIFE_METER, 0);
    SceSys.m_dmg_bak = pPL->dmg;
    pPL->dmg.set(0, 0x80);
    SysFlagOn(pG, SYS_SCISSOR_ON);
    SpfFlagOn(pG, SPF_ACTBTN);
    SpfFlagOn(pG, SPF_SCE_AT);
    SndBlkStop(2);
}

// The value is evaluated before the `->task` load (`lbz x70` between the call and `lwz 8(r3)`).
static inline void SceTaskFlagSet(ScePrim* p, u8 v) { p->task->flag = v; }

// Ends the event when the nesting count drops to 0: enemies / objects back from event mode (mode
// passed to cEm::endEvent) with the camera returned, the player's damage state restored, lights,
// HUD and keys back, System_flg 0x800 as before the event; the sub screen stays closed 10 frames.
void SceEventEnd(int mode)
{
    cSceSys* s = &SceSys;

    if (s->event_start_cnt == 0) {
        pLog->err(0, 0, "SceEventEnd: CALLS TO MACH");
    }
    if (--s->event_start_cnt != 0) {
        return;
    }
    if (s->event_no_cut_back == 1) {
        s->event_no_cut_back = 0;
        EmMgr.endEvent(mode);
        ObjMgr.endEvent(0);
        CamCtrl.Comeback(0);
        pPL->dmg.clear();
    } else {
        pPL->dmg = s->m_dmg_bak;
    }
    LightMgr.endEvent();
    StaFlagOff(pG, STA_EVENT_CANCEL);
    StaFlagOff(pG, STA_EVENT);
    StaFlagOff(pG, STA_SUSPEND);
    SpfFlagOff(pG, SPF_KEY);
    SysFlagOff(pG, SYS_SCREEN_STOP);
    Cckpt.lifeMeterDisp(1);
    IdSys.dispSw(IDC_LIFE_METER, 1);
    SpfFlagOff(pG, SPF_ACTBTN);
    SpfFlagOff(pG, SPF_SCE_AT);
    ShadowMemClear();
    if (SceSys.system_bak & 0x800) {
        SysFlagOn(pG, SYS_SCISSOR_ON);
    } else {
        SysFlagOff(pG, SYS_SCISSOR_ON);
    }
    if (SceSys.checkCTaskRange() == 1) {
        s = &SceSys;
        if (s->task_kind_back != 0) {
            SceTaskFlagSet(SceCTask(), s->task_kind_back);
        }
    }
    SceSys.task_kind_back = 0;
    SubScreenWait(10);
}

// Begins an "up cut" (a short close-up with the game frozen, e.g. item pick-up, messages): marks
// the task as an event task, saves Stop_flg, stops all keys and most movement (Stop_flg all but a
// few bits), hides the HUD; Disp_flg 0x40000000 / 0x20000000 hide the player weapon / partner.
void SceUpCutStart()
{
    cSceSys* s;

    if (SceSys.checkCTaskRange() == 1) {
        s = &SceSys;
        if (s->task_kind_back == 0) {
            s->task_kind_back = SceCTask()->task->flag;
            SceCTask()->task->flag |= 2;
        }
    }
    if (SceSys.stop_bak_flg == 0) {
        SceSys.stop_bak = pG->Stop_flg;
        SceSys.stop_bak_flg = 1;
    }
    KeyStop(0xEFCF0000);
    DpfFlagOn(pG, DPF_PL);
    DpfFlagOn(pG, DPF_SUBCHAR);
    pPL->atari.offSca();
    StaFlagOn(pG, STA_SUSPEND);  // the pG load waits for the clrFlag100 store
    (pG->Stop_flg = 0xFFFFFFFF);
    SpfFlagOff(pG, SPF_CAMERA);
    SpfFlagOff(pG, SPF_EARTHQUAKE);
    SpfFlagOff(pG, SPF_EM);
    SpfFlagOff(pG, SPF_ESP);
    SpfFlagOff(pG, SPF_OBJ);
    SpfFlagOff(pG, SPF_SCE);
    SpfFlagOff(pG, SPF_SE_CALC);
    SpfFlagOff(pG, SPF_LIGHT);
    SpfFlagOff(pG, SPF_ID_SYSTEM);
    Cckpt.lifeMeterDisp(0);
    IdSys.dispSw(IDC_LIFE_METER, 0);
}

// Ends an up cut: restores Stop_flg, the display flags, the player collision flag, the task kind
// and the HUD.
void SceUpCutEnd()
{
    cSceSys* s = &SceSys;

    SpfFlagOff(pG, SPF_KEY);
    DpfFlagOff(pG, DPF_PL);
    DpfFlagOff(pG, DPF_SUBCHAR);
    pPL->atari.onSca();
    StaFlagOff(pG, STA_SUSPEND);  // the pG load waits for the setFlag100 store
    if (s->stop_bak_flg == 1) {
        pG->Stop_flg = s->stop_bak;
        s->stop_bak_flg = 0;
    }
    if (s->checkCTaskRange() == 1) {
        if (s->task_kind_back != 0) {
            ScePrim* p = SceCTask();
            u8 v = s->task_kind_back;  // read before the task pointer (both loads after the call)
            p->task->flag = v;
        }
    }
    SceSys.task_kind_back = 0;
    Cckpt.lifeMeterDisp(1);
    IdSys.dispSw(IDC_LIFE_METER, 1);
    SubScreenWait(10);
}

// 1 when the player is in a state an event may take him from (routine 0).
int SceCheckEventStart()
{
    return pPL->checkEvent() == 1;
}

// Room: function `a` (with parameter `b`) SceSys runs when the room is left.
void SceSetRoomExitFunc(TaskFunc pFunc, void* param)
{
    SceSys.pExitFunc = pFunc;
    SceSys.pExitParam = param;
}

// Room script scratch word `no` (0..63) in the save data (pG->save_free_work).
void SetFree(int no, u32 val)
{
    u32* tbl;

    if (no > 0x3F) {
        return;
    }
    tbl = pG->save_free_work;
    tbl[no] = val;
}

// Reads a save scratch word (0 when out of range).
u32 GetFree(int no)
{
    u32* tbl;

    if (no <= 0x3F) {
        tbl = pG->save_free_work;
        return tbl[no];
    }
    return 0;
}

// Shows room message `no` at (x, y): flags bit0 the plain style, bit5 keep the camera cut, bit6 /
// 7 / 8 / 9 / 1 the cMes attribute bits (selection box, yes/no...), `sel` - 1 = initial cursor;
// hides the life meter and, unless flags bit4, waits until the message closes.
void SceMesSet(int no, u32 flags, int sel, int x, int y)
{
    u32 attr;

    attr = 0x1002;
    if (flags & 1) {
        attr = 0x1001;
    }
    if (flags & 0x20) {
        attr |= 0x10;
    }
    if (flags & 0x40) {
        attr |= 0x1000000;
    }
    if (flags & 0x80) {
        attr |= 0x40;
    }
    if (flags & 0x100) {
        attr |= 0x800000;
    }
    if (flags & 0x200) {
        attr |= 0x100000;
    }
    if (flags & 2) {
        attr |= 0x2000000;
    }
    cMes.MesSet(no, x, y, attr, 0, 0, 4);
    cMes.m_Msg[0].m_cur = sel - 1;
    Cckpt.lifeMeterDisp(0);
    if (!(flags & 0x10)) {
        SceMesWait();
    }
}

// Message `no` with an optional camera cut and SE (block 6), then waits for it.
void SceMesCamSndSet(int mes_no, int cam_no, int se_no, int attr)
{
    if (cam_no != -1) {
        CamCtrl.CutCall((s8) cam_no);
    }
    if (se_no != -1) {
        SndCall(6, se_no, 0, 0, 0, 0);
    }
    SceMesSet(mes_no, cam_no == -1 ? 0 : 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
}

// Up-cut message through SceAtSetMes: message `a` (flags bit0 = type 1), camera cut `b`, SE `c`
// (block 0 when flags bit1); waits for the message.
void SceUpCut(int a, int b, int c, int attr)
{
    SceAtMesData m;

    // Both flag bytes stored in each arm (jump2 cross-jumps the else arm's store into the
    // then arm's): the byte stays in r0 and `sth a` precedes the `&m` argument.
    if (attr & 1) {
        m.type = 1;
    } else {
        m.type = 0;
    }
    if (attr & 2) {
        m.seBlk = 1;
    } else {
        m.seBlk = 0;
    }
    m.no = a;
    m.camCut = b + 1;
    m.se = c + 1;
    m.flag = attr;
    SceAtSetMes(&m);
    SceMesWait();
}

// Sleeps until the yes/no message is answered; returns 1 yes, 2 no.
int SceMesGetSelection()
{
    int r;

    if ((r = cMes.getWork()->m_sel) == 0) {
        do {
            SceSleep(1);
        } while ((r = cMes.getWork()->m_sel) == 0);
    }
    return r;
}

// Sleeps while message slot 0 is open.
void SceMesWait()
{
    while (cMes.m_Msg[0].m_state & 1) {
        SceSleep(1);
    }
}

// The thunder SE (block 6, 0x1D).
void SceSndCallThunder()
{
    SndCall(6, 0x1D, 0, 0, 0, 0);
}

// 1 when `em` exists, is alive and active.
int SceCheckEmAlive(cEm* pEm)
{
    if (pEm == 0) {
        return 0;
    }
    if (!pEm->isAlive()) {
        return 0;
    }
    if (pEm->checkStatus(EM_STATUS_ACTIVE) == 0) {
        return 0;
    }
    return 1;
}

// Number of living, active enemies with id in lo..hi (hi -1 = just lo).
int SceCountEmAlive(int em_id, int em_id_end)
{
    int cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);
        if (em_id_end == -1) {
            em_id_end = em_id;
        }
        if (em->id >= em_id && em->id <= em_id_end) {
            if (SceCheckEmAlive(em) == 1) {
                cnt++;
            }
        }
    }
    return cnt;
}

// Destroys every living enemy with id in lo..hi (hi -1 = just lo) and clears its list entry.
void SceDestroyEm(int em_id, int em_id_end)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);
        if (em_id_end == -1) {
            em_id_end = em_id;
        }
        if (em->id >= em_id && em->id <= em_id_end) {
            if (em->isAlive()) {
                EmListData* l = GetListPtrFromEm(em);
                if (l) {
                    l->be_flag &= ~1;
                }
                EmMgr.destroy(em);
            }
        }
    }
}

// Room start: no pending item events.
void SceInitItemEvent()
{
    void** p = ItemEventTbl;
    int i;

    for (i = 0; i < 16; i++) {
        *p++ = 0;
    }
    // Strings of a debug prompt the original kept around (dead code); its HALT emits the
    // "D:/Bio4/Prog/sce_com.cpp" string before "HALT %s(%d)\n" (the flag_rsf.h checks below reuse
    // both, with the fmt high first).
    if (0) {
        pLog->err(0, 0, "Do you use the PLANTER?");
        pLog->err(0, 0, "You used the PLANTER.");
        pLog->err(0, 0, "You cannot use a PLANTER.");
        pLog->err(0, 0, "              >YES  NO");
        pLog->err(0, 0, "               YES >NO");
#line 444 "D:/Bio4/Prog/sce_com.cpp"
        HALT_FILE_FIRST();
    }
}

#include "flag_rsf.h"
#include <stdio.h>
#include <string.h>

extern "C" void SceExecItemEvent(SceItemEvent* e);

// Task of an item event (the action button on its area): disables the area, enables the linked
// item areas, sets the room save flag, plays the camera cut while `func(arg)` runs, then forgets
// the event.
void SceExecItemEvent(SceItemEvent* data)
{
    u32 i;
    int flag;  // `lbz` straight into the callee-saved register (a u8 local adds an `mr` copy)
    u16 room;

    SceAtSetEnable(data->atNo, 0);
    for (i = 0; i <= 7; i++) {
        if (data->item[i] >= 0) {
            cModel* m;
            SceAtSetEnable(data->item[i], 1);
            m = SceAtItemModelPtr(data->item[i]);
            if (m) {
                m->setNoSuspend(1);
            }
        }
    }
    flag = data->flag;
    room = pG->room_id;
    RsfSet(room, flag);
    SceUpCutStart();
    if (data->cut >= 0) {
        CamCtrl.CutCall((s8) data->cut);
        data->func(data->arg);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSleep(0xF);
        CamCtrl.Comeback(0);
    } else {
        data->func(data->arg);
    }
    SceUpCutEnd();
    for (i = 0; i < 16; i++) {
        SceItemEvent* p = (SceItemEvent*) ItemEventTbl[i];
        if (p && p->atNo == data->atNo) {
            ItemEventTbl[i] = 0;
        }
    }
    __builtin_delete(data);
}

// Room: makes area `atNo` an item event (action button 0x10) that reveals item area `itemNo`
// (added to an existing event on the same area) with camera cut `cut` and `func(arg)`; if room
// save flag `flagNo` is already set the event is skipped, the item enabled (unless taken) and
// `doneFunc(arg)` run instead. `enable` shows the item model beforehand.
void SceSetItemEvent(int atNo, int itemNo, int flagNo, int cut, void (*func)(int), void (*doneFunc)(int), int arg, int enable)
{
    u16 room = pG->room_id;
    SceItemEvent* e;   // the searched entry; the new'd one is a second variable (one pseudo for both
                       // is live across the search loop's j / e+6 / j*2 temps and takes r8 there)
    SceItemEvent* ne;
    u32 i;
    u32 j;
    u32 k;

    if (RsfCheck(room, flagNo)) {
        SceAtSetEnable(atNo, 0);
        if (itemNo >= 0) {
            if (SceAtItemFlgCk(itemNo) == 0) {
                SceAtSetEnable(itemNo, 1);
            }
        }
        SceExec(0x12, (TaskFunc) doneFunc, arg, 0, SCE_PRIO_DEF_2, 0);
        return;
    }
    if (itemNo >= 0) {
        if (enable == 0) {
            SceAtSetEnable(itemNo, 0);
        } else {
            cModel* m;
            SceAtSetEnable(itemNo, 1);
            m = SceAtItemModelPtr(itemNo);
            SceAtSetEnable(itemNo, 0);
            if (m) {
                m->setTrans(1);
            }
        }
    }
    for (i = 0; i < 16; i++) {
        e = (SceItemEvent*) ItemEventTbl[i];
        if (e && e->atNo == atNo) {
            // The slot search: item[0] tested and stored with the folded offset, then a loop entered
            // by a `goto` INTO its body (a jump into the loop invalidates it for loop.c: no giv for
            // j*2, `e + 6` recomputed per iteration, and the exit block's guard targets a label of
            // another loop so find_and_verify_loops leaves `item[0] = itemNo; return` in place).
            // COMPILER-DIFF: `li j,0` spelled as a mask combine folds to 0. A C `j = 0` gets cse's
            // REG_EQUAL note, and update_equiv_regs (the first set of j in chain order) doubles j's
            // live length (11 -> 22: priority 30909 < e+6 48000 / j*2 40000, so j is allocated third
            // and lands in r10). cse cannot fold `(e >> 16) & 0xFFFF0000` (no nonzero-bits logic) so
            // the set carries no note; combine folds it to `(set j 0)` after cse2, j keeps 11 (61818)
            // and is allocated first (r9, e+6 r11, j*2 r10, e r8 = the target). Also keeps j a
            // pseudo: a hard-reg pin makes combine fold expand_mult's `copy + j` into `slwi` where
            // the target has `add`.
            j = ((u32) e >> 16) & 0xFFFF0000;
            if (e->item[0] >= 0) {
                goto next;
            }
            e->item[0] = itemNo;
            return;
            do {
            next:
                j++;
                if (j > 7) {
                    return;
                }
            } while (e->item[j] >= 0);
            e->item[j] = itemNo;
            return;
        }
    }
    for (i = 0; i <= 15; i++) {
        if (ItemEventTbl[i] == 0) {
            break;
        }
    }
    if (i == 16) {
        pLog->err(0, 0, "SceSetItemEvent(): TBL num over");
        return;
    }
    if (SceAtPtr(atNo)) {
        SceAtPtr(atNo)->trigger = 8;
        SceAtPtr(atNo)->actBtnKind = 0x10;
        SceAtPtr(atNo)->otNo = 5;
    }
    ne = (SceItemEvent*) __builtin_new(sizeof(SceItemEvent));
    for (k = 0; k < 8; k++) {
        ne->item[k] = -1;
    }
    ne->item[0] = itemNo;
    ItemEventTbl[i] = ne;
    ne->cut = cut;
    ne->func = func;
    ne->arg = arg;
    ne->flag = flagNo;
    ne->atNo = atNo;
    SceAtDataSet_exec(atNo, SCE_LEVEL10, 0, (TaskFunc) SceExecItemEvent, ne, 1);
}

// Chapter counter (0..) -> displayed "chapter-section" numbers (1-1 .. 5-4 plus the extra ones).
void getChapterSection(int no, int* chap, int* sect)
{
    switch (no) {
    case 0:
        *chap = 1;
        *sect = 1;
        break;
    case 1:
        *chap = 1;
        *sect = 2;
        break;
    case 2:
        *chap = 1;
        *sect = 3;
        break;
    case 3:
        *chap = 2;
        *sect = 1;
        break;
    case 4:
        *chap = 2;
        *sect = 2;
        break;
    case 5:
        *chap = 2;
        *sect = 3;
        break;
    case 6:
        *chap = 3;
        *sect = 1;
        break;
    case 7:
        *chap = 3;
        *sect = 2;
        break;
    case 8:
        *chap = 3;
        *sect = 3;
        break;
    case 9:
        *chap = 3;
        *sect = 4;
        break;
    case 0xA:
        *chap = 4;
        *sect = 1;
        break;
    case 0xB:
        *chap = 4;
        *sect = 2;
        break;
    case 0xC:
        *chap = 4;
        *sect = 3;
        break;
    case 0xD:
        *chap = 4;
        *sect = 4;
        break;
    case 0xE:
        *chap = 5;
        *sect = 1;
        break;
    case 0xF:
        *chap = 5;
        *sect = 2;
        break;
    case 0x10:
        *chap = 5;
        *sect = 3;
        break;
    case 0x11:
        *chap = 5;
        *sect = 4;
        break;
    case 0x12:
        *chap = 6;
        *sect = 1;
        break;
    default:
        *chap = 1;
        *sect = 1;
        break;
    }
}

// Reference setters: the original stores these GlobalWork fields through references (pG reloaded after each).
static inline void U16Zero(u16& d) { d = 0; }  // HImode zero (its own `li`), reference store

// Chapter end task (SceSetChapterEnd): kills the running event, freezes the game, swaps the room
// data out to load the chapter result id data ("SS/<lang>/chapNN.dat"), shows the ChapterEnd
// screen with the "save?" message (0x80); with a door area the player is moved through it for
// the save (pG->chapter, counters reset, GameSave.save), a yes saves to the card; then everything
// is restored and the door executed (fade effect 2), or the BGM restarts and the pause ends.
void SceChapterEnd()
{
    cDataSwap swap;
    static u32 stop_bak;
    static u32 disp_bak;
    static char chap_data_name[0x20];
    static u32 MARGIN = 0x20000;
    void* evt;
    int chap;
    int sec;
    u32 len;
    void* data;
    ChapterEnd* ce;
    int req;
    int sel;
    u16 room;
    u8 x4F9E;
    EventMgr* ev = &EvtMgr;
    u32* key = &ev->NowExeEvtKey;

    pG->chapter = SceSys.m_chapter_no + 1;
    if (ev->IsAliveEvt(key, 0, 1)) {
        ev->GetEvt(key, &evt);
        ev->DelEvt(evt, 0);
    }
    if (Fade[1].flags & 1) {
        SceSleep(1);
    }
    sel = 0;
    disp_bak = pG->Disp_flg;
    pG->Disp_flg = 0xFFFFFFFF;
    DpfFlagOff(pG, DPF_ID_SYSTEM);
    DpfFlagOff(pG, DPF_MESSAGE);
    DpfFlagOff(pG, DPF_COCKPIT);
    stop_bak = pG->Stop_flg;
    pG->Stop_flg = 0xFFFFFFFF;
    SpfFlagOff(pG, SPF_SCE);
    SpfFlagOff(pG, SPF_ID_SYSTEM);
    SceSleep(2);
    chap = 0;
    sec = 0;
    getChapterSection(SceSys.m_chapter_no, &chap, &sec);
    if (SceSys.m_chapter_no == 0x11) {
        sprintf(chap_data_name, "SS/___/chap06.dat");
    } else if (SceSys.m_chapter_no == 0xD) {
        sprintf(chap_data_name, "SS/___/chap07.dat");
    } else {
        sprintf(chap_data_name, "SS/___/chap%02ld.dat", chap);
    }
    setLangExt3(chap_data_name + 3);
    Dvd.FileExistCheck(chap_data_name, &len);
    len = len + 0xC;
    len = len + MARGIN;
    swap.SwapOut((u32) pG->pRoom, len, 0);
    ce = (ChapterEnd*) __builtin_new(sizeof(ChapterEnd));
#line 994 "D:/Bio4/Prog/sce_com.cpp"
    req = DVD_READ_N(chap_data_name, 0, 0, 0, 0, 5);
    Dvd.ReadCheck(req, 0, 0, &data);
    ce->init(data, SceSys.m_chapter_no);
    ce->move();
    FadeKillAll();
    FadeSetW(0x80000000, 10, 0, 0);
    SceSleep(0xF);
    SceMesSet(0x80, 1, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    Vec plPos;
    Vec plRot;
    // The two zeros are assigned after the FadeSetW so its `col.end = 0` keeps its own zero pseudo (the
    // one the U8Set/U16Set stores below reuse, r27); room before x4F9E (sched1 LUID order of the `li`s),
    // room declared first (the global-alloc tie for r25/r24).
    room = 0;
    x4F9E = 0;
    if (SceSys.m_chapter_door >= 0) {
        plPos = pPL->pos;
        plRot = pPL->ang;
        room = pG->room_id;
        x4F9E = pG->Part;
        if (SceAtPtr(SceSys.m_chapter_door)->type == 1) {
            pPL->pos.x = SceAtPtr(SceSys.m_chapter_door)->dstPos.x;
            pPL->pos.y = SceAtPtr(SceSys.m_chapter_door)->dstPos.y;
            pPL->pos.z = SceAtPtr(SceSys.m_chapter_door)->dstPos.z;
            pPL->ang.y = SceAtPtr(SceSys.m_chapter_door)->dstAngle;  // the pG load of room_id_prev waits for the store
            (pG->room_id_prev = pG->room_id);
            pG->Part_old = pG->Part;
            pG->stage_no = SceAtPtr(SceSys.m_chapter_door)->dstStage;
            pG->room_no = SceAtPtr(SceSys.m_chapter_door)->dstRoom;
            pG->Part = SceAtPtr(SceSys.m_chapter_door)->dstPart;
            pG->JumpPoint = 0;
            pG->r_continue_cnt = 0;
        } else {
            pLog->err(0, 0, "SceChapterEnd(): Door at faild");
        }
    }
    U16Zero(pG->c_continue_cnt);
    pG->c_kill_cnt = 0;
    pG->c_hit_cnt = 0;
    pG->c_shot_cnt = 0;
    GameSave.save(pSaveData, 2);
    sel = SceMesGetSelection();
    if (sel == 1) {
        SndCall(0, 4, 0, 0, 0, 0);
    } else {
        SndCall(0, 5, 0, 0, 0, 0);
    }
    FadeSetW(0, 5, 0, 0);
    SceSleep(5);
    ce->quit();
    __builtin_delete(ce);
    swap.SwapIn();
    if (sel == 1) {
        CardSave(0, 10);
        SceSleep(1);
    }
    pG->Disp_flg = disp_bak;
    pG->Stop_flg = stop_bak;
    FadeSetW(0, 0, 0, 0);
    FadeKill(FADE_NO_ROOM);
    if (SceSys.m_chapter_door >= 0) {
        // pos / ang through byte pointers: `&pPL->pos` changes the schedule of this block (27 words)
        memcpy((u8*) pPL + 0x94, &plPos, sizeof(Vec));
        memcpy((u8*) pPL + 0xA0, &plRot, sizeof(Vec));
        pG->room_id = room;
        pG->Part = x4F9E;
        if (SceAtPtr(SceSys.m_chapter_door)) {
            SceAtPtr(SceSys.m_chapter_door)->doorFadeEff = 2;
            SceAtExecute(SceSys.m_chapter_door);
        }
    } else {
        SndRoomBgmStartCheck(1);
        SndRoomStrStartCheck();
        FadeSetW(0x80000000, 10, 0, 0);
        SceSys.pause = 0;
    }
}

// Room: end of chapter `chapter` — fades to black, stops the music, and runs SceChapterEnd as an
// event (door area `doorAt` is taken afterwards, -1 = none).
void SceSetChapterEnd(int ChapterNo, int door_at_no)
{
    GXColor c0;
    GXColor c1;

    *(u32*) &c0 = 0;
    *(u32*) &c1 = 0xFF;
    FadeSet(0, &c0, &c1, 1, 0, 0);
    SndRoomStrStop(1);
    SndRoomBgmStop(0, 0);
    SndRoomBgmStop(1, 0);
    SndSeAbsFadeOutAll_sec(1);
    SceEventStart(0);
    SpfFlagOff(pG, SPF_SCE);
    SceSys.pause = 1;
    SceSys.m_chapter_no = ChapterNo;
    SceSys.m_chapter_door = door_at_no;
    SetGameTime();
    SceExec(5, (TaskFunc) SceChapterEnd, 0, 0, SCE_PRIO_DEF_2, 0);
    SceSleep(1);
    SceEventEnd(0);
}

// Puts the scenario camera (SceCam) at pos looking at `at` with `fovy` and makes it the extra
// camera (CamCtrl.m_pExtraCamera).
void SceCamMove(Vec* pCamPos, Vec* pTarget, f32 fovy)
{
    SceCam.param.pos = *pCamPos;
    SceCam.param.at = *pTarget;
    SceCam.param.fovy = fovy;
    SceCam.Up.x = 0.0f;
    SceCam.Up.y = 1.0f;
    SceCam.Up.z = 0.0f;
    SceCam.Distance = VEC_DIST(&SceCam.param.pos, &SceCam.param.at);
    CameraSetOrientationUp(&SceCam);
    CamCtrl.m_pExtraCamera = (s32) &SceCam;
}

// Opens / closes a container (scroll objects id1 / id2 = lids or doors, the item model `itemNo`
// inside): mode 0 animates the lids over 30 frames by `type` (0..8, 0x13..0x19: swing / slide axes
// and directions) with SE `se` and drops the item onto the floor; mode 1 sets the end pose at once.
void OpenBoxMain(int type, int mode, int se, u32 id1, u32 id2, int itemNo)
{
    cObj* o1 = 0;
    cObj* o2 = 0;
    cModel* item = 0;
    f32 dy = 0.0f;
    // Case bodies are laid out in source order: case 0x17 (both doors, 160 deg) follows case 0 in
    // both switches. Every loop declares its own `int i`: the two-frame waits' counters are then
    // short-lived pseudos (5 refs / ~8 insns) that global alloc places first, taking r31 before
    // `type` (r30) and `o1` (r29); id2 and the 30-frame counter reuse r31 afterwards. One shared
    // `int i` (35 refs / 482 insns) sorts below them and rotates the three.
    // Constant-pool order: the 30-frame totals, their per-frame steps (folded divisions: the
    // decimal step literals are one ulp off) and the drop step enter the pool here; every use
    // below is folded to the literal.
    const f32 ryA = -1.9198622f, ryB = 1.9198622f, ryC = -2.7925267f, ryD = 2.7925267f;
    const f32 rxA = 1.7f, rxB = -1.7f, rzA = 1.5707964f, rzB = -1.5707964f, pxA = 500.0f, pxB = -500.0f;
    const f32 syA = ryA / 30.0f, syB = ryB / 30.0f, syC = ryC / 30.0f, syD = ryD / 30.0f;
    const f32 sxA = rxA / 30.0f, sxB = rxB / 30.0f, szA = rzA / 30.0f, szB = rzB / 30.0f, spA = pxA / 30.0f, spB = pxB / 30.0f;
    const f32 dropStep = 10.0f;

    if (id1 != -1) {
        o1 = SmdGetObjPtr(id1);
    }
    if (id2 != -1) {
        o2 = SmdGetObjPtr(id2);
    }
    if (itemNo != -1) {
        item = SceAtItemModelPtr(itemNo);
    }
    if (o1) {
        o1->setMove(1);
    }
    if (o2) {
        o2->setMove(1);
    }
    if (mode == 0) {
        switch (type) {
        case 0x15:
            for (int i = 0; i < 2; i++) {
                SceSleep(1);
            }
            for (int i = 0; i < 2; i++) {
                if (o1) {
                    o1->pList->ang.z += -0.034906585f;
                }
                SceSleep(1);
            }
            for (int i = 0; i < 3; i++) {
                SceSleep(1);
            }
            break;
        case 0x16:
            for (int i = 0; i < 5; i++) {
                SceSleep(1);
            }
            break;
        }
        if (se != -1) {
            SndCall(6, se, 0, 0, 0, 0);
        }
        for (int i = 0; i < 30; i++) {
            switch (type) {
            case 0:
                if (o1) {
                    o1->ang.y += (-1.9198622f / 30.0f);
                }
                if (o2) {
                    o2->ang.y += (1.9198622f / 30.0f);
                }
                break;
            case 0x17:
                if (o1) {
                    o1->ang.y += (-2.7925267f / 30.0f);
                }
                if (o2) {
                    o2->ang.y += (2.7925267f / 30.0f);
                }
                break;
            case 1:
            case 0x13:
                if (o1) {
                    o1->ang.y += (-1.9198622f / 30.0f);
                }
                break;
            case 2:
            case 0x14:
                if (o1) {
                    o1->ang.y += (1.9198622f / 30.0f);
                }
                break;
            case 0x18:
                if (o1) {
                    o1->ang.y += (-2.7925267f / 30.0f);
                }
                break;
            case 0x19:
                if (o1) {
                    o1->ang.y += (2.7925267f / 30.0f);
                }
                break;
            case 3:
                if (o1) {
                    o1->ang.x += (1.7f / 30.0f);
                }
                break;
            case 4:
                if (o1) {
                    o1->ang.x += (-1.7f / 30.0f);
                }
                break;
            case 5:
                if (o1) {
                    o1->ang.z += (1.7f / 30.0f);
                }
                break;
            case 6:
                if (o1) {
                    o1->ang.z += (-1.7f / 30.0f);
                }
                break;
            case 7:
                if (o1) {
                    o1->pList->ang.x += (1.7f / 30.0f);
                }
                break;
            case 8:
                if (o1) {
                    o1->pList->ang.x += (-1.7f / 30.0f);
                }
                break;
            case 9:
                if (o1) {
                    o1->pList->ang.z += (1.7f / 30.0f);
                }
                break;
            case 0xA:
                if (o1) {
                    o1->pList->ang.z += (-1.7f / 30.0f);
                }
                break;
            case 0xB:
                if (o1) {
                    o1->ang.x += (1.5707964f / 30.0f);
                }
                break;
            case 0xC:
                if (o1) {
                    o1->ang.x += (-1.5707964f / 30.0f);
                }
                break;
            case 0xD:
                if (o1) {
                    o1->ang.z += (1.5707964f / 30.0f);
                }
                break;
            case 0xE:
                if (o1) {
                    o1->ang.z += (-1.5707964f / 30.0f);
                }
                break;
            case 0xF:
                if (o1) {
                    o1->pos.x += (500.0f / 30.0f);
                }
                if (item) {
                    item->pos.x += (500.0f / 30.0f);
                }
                break;
            case 0x10:
                if (o1) {
                    o1->pos.x += (-500.0f / 30.0f);
                }
                if (item) {
                    item->pos.x += (-500.0f / 30.0f);
                }
                break;
            case 0x11:
                if (o1) {
                    o1->pos.z += (500.0f / 30.0f);
                }
                if (item) {
                    item->pos.z += (500.0f / 30.0f);
                }
                break;
            case 0x12:
                if (o1) {
                    o1->pos.z += (-500.0f / 30.0f);
                }
                if (item) {
                    item->pos.z += (-500.0f / 30.0f);
                }
                break;
            case 0x15:
                dy -= 10.0f;
                if (o1) {
                    o1->pos.y += dy;
                    o1->pList->ang.z += -0.017453292f;
                }
                break;
            case 0x16:
                dy -= 10.0f;
                if (o1) {
                    o1->pos.y += dy;
                }
                break;
            }
            SceSleep(1);
        }
    } else {
        switch (type) {
        case 0:
            if (o1) {
                o1->ang.y += -1.9198622f;
            }
            if (o2) {
                o2->ang.y += 1.9198622f;
            }
            break;
        case 0x17:
            if (o1) {
                o1->ang.y += -2.7925267f;
            }
            if (o2) {
                o2->ang.y += 2.7925267f;
            }
            break;
        case 1:
        case 0x13:
            if (o1) {
                o1->ang.y += -1.9198622f;
            }
            break;
        case 2:
        case 0x14:
            if (o1) {
                o1->ang.y += 1.9198622f;
            }
            break;
        case 0x18:
            if (o1) {
                o1->ang.y += -2.7925267f;
            }
            break;
        case 0x19:
            if (o1) {
                o1->ang.y += 2.7925267f;
            }
            break;
        case 3:
            if (o1) {
                o1->ang.x += 1.7f;
            }
            break;
        case 4:
            if (o1) {
                o1->ang.x += -1.7f;
            }
            break;
        case 5:
            if (o1) {
                o1->ang.z += 1.7f;
            }
            break;
        case 6:
            if (o1) {
                o1->ang.z += -1.7f;
            }
            break;
        case 7:
            if (o1) {
                o1->pList->ang.x += 1.7f;
            }
            break;
        case 8:
            if (o1) {
                o1->pList->ang.x += -1.7f;
            }
            break;
        case 9:
            if (o1) {
                o1->pList->ang.z += 1.7f;
            }
            break;
        case 0xA:
            if (o1) {
                o1->pList->ang.z += -1.7f;
            }
            break;
        case 0xB:
            if (o1) {
                o1->ang.x += 1.5707964f;
            }
            break;
        case 0xC:
            if (o1) {
                o1->ang.x += -1.5707964f;
            }
            break;
        case 0xD:
            if (o1) {
                o1->ang.z += 1.5707964f;
            }
            break;
        case 0xE:
            if (o1) {
                o1->ang.z += -1.5707964f;
            }
            break;
        case 0xF:
            if (o1) {
                o1->pos.x += 500.0f;
            }
            if (item) {
                item->pos.x += 500.0f;
            }
            break;
        case 0x10:
            if (o1) {
                o1->pos.x += -500.0f;
            }
            if (item) {
                item->pos.x += -500.0f;
            }
            break;
        case 0x11:
            if (o1) {
                o1->pos.z += 500.0f;
            }
            if (item) {
                item->pos.z += 500.0f;
            }
            break;
        case 0x12:
            if (o1) {
                o1->pos.z += -500.0f;
            }
            if (item) {
                item->pos.z += -500.0f;
            }
            break;
        case 0x15:
        case 0x16:
            break;
        }
    }
}

extern "C" void SceElevator(SceElevatorData* d);


// Shape from r225.cpp's SceElevator_r225 (SetPosXYZ / FadeSetRGBA inline helpers, the goto-entered up
// loop, the down loop with its tail inside). Residue closed by the `jp` pin: `done` (10 refs, live
// length 106 x4 from update_equiv_regs' two `done = 0` REG_EQUIV doublings = 424, priority 707) sorts
// below gcse's `&d->pos` copy (9 refs / 380 = 710) in global alloc, so the copy takes r25 and done r24;
// the target has done r25 / copy r24 (done's length there is 105 -> 420 -> 714: one pre-reload insn
// fewer somewhere in its range). Holding `&d->jumpPos` (the target's r25 in the up loop, where done is
// dead) in r25 makes the copy take r24 and done r25 without touching anything else.
void SceElevator(SceElevatorData* d)
{
    cPlayer* pl = pPL;
    cObj* obj;
    f32 accel;
    f32 maxSpd;
    f32 minSpd;
    f32 stopDist;
    f32 stopDist2;
    f32 spd;
    f32 step;
    f32 move;
    int faded;
    int done;
    FadeWork* fade;
    u32 white;
    int i;
    int j;
    u32 hSnd;
    register Vec* jp asm("r25");  // COMPILER-DIFF: register pin (see the comment above the function)

    obj = SmdGetObjPtr(d->objId);
    if (obj == 0) {
        return;
    }
    maxSpd = 100.0f;
    minSpd = 10.0f;
    accel = 2.0f;
    stopDist = CalcStopDist(maxSpd, accel);
    stopDist2 = stopDist + 4000.0f;
    SceEventStart(0);
    faded = 0;
    done = 0;
    StaFlagOn(pG, STA_TIMER_NO_PAUSE);
    obj->setNoSuspend(1);
    obj->setPos(&d->pos);
    pPL->setNoSuspend(1);
    pPL->beginEvent(0);
    pPL->setPos(&d->plPos);
    pPL->setAng(&d->plRot);
    pPL->be_flag &= ~0x10;
    CamCtrl.Comeback(0);
    if (d->cut != -1) {
        CamCtrl.CutCall((s8) d->cut);
    }
    if (d->dir == 1 || d->dir == 3) {
        SndCall(6, d->seStart, &obj->pos, 0, 0, 0);
        spd = accel;
        jp = &d->jumpPos;
        for (i = 0; i < 10; i++) {
            obj->setPos(&d->pos);
            pPL->setPos(&d->plPos);
            obj->setPos(obj->pos.x, fRand1_1() * 10.0f + obj->pos.y, obj->pos.z);
            pPL->setPos(pPL->pos.x, fRand1_1() * 10.0f + pPL->pos.y, pPL->pos.z);
            SceSleep(1);
        }
        obj->setPos(&d->pos);
        pPL->setPos(&d->plPos);
        // Up loop: a noted loop that loop.c does not process (entered by the goto below = "multiple
        // entry points"), laid out `b TOP; SLEEP: SceSleep; spd += accel; TOP: ...` (r225.cpp).
        fade = &Fade[2];
        white = 0xFF;
        goto up_top;
        for (;;) {
            SceSleep(1);
            // COMPILER-DIFF: candidate (sched1 loop-note barrier). The target issues `spd += accel`
            // after the SceSleep call; sched1 only keeps it there behind a loop note.
            do { } while (0);
            spd += accel;
        up_top:
            if (spd > maxSpd) {
                spd = maxSpd;
            }
            step = spd;
            if (d->dir == 1) {
                step = -spd;
            }
            obj->setPos(obj->pos.x, obj->pos.y + step, obj->pos.z);
            pPL->setPos(pPL->pos.x, pPL->pos.y + step, pPL->pos.z);
            if (faded == 0) {
                if (spd >= maxSpd) {
                    FadeSetRGBA(2, 0, white);
                    faded = 1;
                }
            } else if ((fade->flags & 1) == 0) {
                StaFlagOff(pG, STA_TIMER_NO_PAUSE);
                SceAtExecRoomJump(d->room, jp, &d->jumpRot, 0);
                break;
            }
        }
    }
    if (d->dir == 0 || d->dir == 2) {
        StaFlagOff(pG, STA_SUSPEND);
        spd = maxSpd;
        move = stopDist2;
        if (d->dir == 0) {
            move = -move;
        }
        obj->setPos(obj->pos.x, obj->pos.y + move, obj->pos.z);
        pPL->setPos(pl->pos.x, pPL->pos.y + move, pl->pos.z);
        CamCtrl.Comeback(0);
        FadeSetRGBA(0x80000002, 0xFF, 0);
        hSnd = SndCall(6, d->seStart, &obj->pos, 0, 0, 0);
        // Down loop: `for (;;) { body; if (done) { tail; break; } SceSleep(1); }` (r225.cpp): the
        // rotated loop with the tail inside it; `y` is only the fabs operand, `move` is a second
        // step variable so `step` dies in the up loop.
        for (;;) {
            f32 y = obj->pos.y;
            if (__builtin_fabsf(d->pos.y - y) < stopDist) {
                spd -= accel;
                if (spd < minSpd) {
                    spd = minSpd;
                }
            }
            move = spd;
            if (d->dir != 0) {
                move = -move;
            }
            obj->setPos(obj->pos.x, obj->pos.y + move, obj->pos.z);
            pPL->setPos(pPL->pos.x, pPL->pos.y + move, pPL->pos.z);
            {
                Vec q = {0.0f, 0.0f, 0.0f};
                q.y = move;
                pG->quake_ofs = q;
            }
            done = 0;
            if (d->dir == 0) {
                if (obj->pos.y >= d->pos.y) {
                    done = 1;
                }
            }
            if (d->dir == 2) {
                if (obj->pos.y <= d->pos.y) {
                    done = 1;
                }
            }
            if (done != 0) {
                if (hSnd) {
                    SndStop(hSnd, 0);
                }
                SndCall(6, d->seStop, &obj->pos, 0, 0, 0);
                obj->setPos(&d->pos);
                pPL->setPos(&d->plPos);
                pPL->setAng(&d->plRot);
                break;
            }
            SceSleep(1);
        }
        for (j = 0; j < 10; j++) {
            obj->setPos(&d->pos);
            pPL->setPos(&d->plPos);
            obj->setPos(obj->pos.x, fRand1_1() * 10.0f + obj->pos.y, obj->pos.z);
            pPL->setPos(pPL->pos.x, fRand1_1() * 10.0f + pPL->pos.y, pPL->pos.z);
            SceSleep(1);
        }
        obj->setPos(&d->pos);
        pPL->setPos(&d->plPos);
    }
    StaFlagOff(pG, STA_TIMER_NO_PAUSE);
    pPL->be_flag |= 0x10;
    SceEventEnd(0);
    SceExit();
}

// Prints a debug line of the scenario (stacked 15 pixels apart each call; the y resets per frame).
void SceDebugDisp(const char* mes, ...)
{
    va_list ap;

    va_start(ap, mes);
    char buf[0x100];  // declared after va_start: the register save area and ap get their slots first
    vsprintf(buf, mes, ap);
    eprintf(0x14, (s16) SceSys.m_debug_disp_y, 0, 1, "%s", buf);
    SceSys.m_debug_disp_y += 0xF;
}

// Called from title.cpp with an argument (`DebugTrg(1)`): the parameter exists, the body ignores it.
int DebugTrg(int)
{
    return 0;
}

template <class T>
// beginEvent(mode) on every live unit of the manager.
void cManager<T>::beginEvent(int mode)
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        T* p = fastAt(i);
        if (p->isAlive()) {
            p->beginEvent(mode);
        }
    }
}

template <class T>
// endEvent(mode) on every live unit of the manager.
void cManager<T>::endEvent(int mode)
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        T* p = fastAt(i);
        if (p->isAlive()) {
            p->endEvent(mode);
        }
    }
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
