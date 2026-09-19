#include "types.h"
#include "event.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "scheduler.h"
#include "libgpu.h"
#include "room_data.h"
#include "cam_ctrl.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "em_set.h"
#include "mes.h"
#include "snd.h"
#include "fade.h"
#include "pad.h"
#include "db_log.h"
#include "sce_sys.h"

extern "C" {
void SceInitItemEvent();                       // game/sce_com.cpp
void SceAtSetSaveItem();                       // game/sce_at.cpp
void SceAtRoomSet();
void SceAtCheckMoveScrAt();
void* SceAtPtr(int no);
void SubMissionCheck();                        // game/stage.cpp
int getRoomEtcBreak(void* p, cEm** em, int a); // game/EtcModel.cpp
void SubScreenWait(int frames);                // game/sscrn.cpp
void GXDrawDone();
}

int SceAtItemFlgCk(int no);  // game/sce_at.cpp (C++ overload set)

#line 34 "D:/Bio4/Prog/sce_sys.cpp"

// Reference store of a SceSys byte: the inline's `&member` reaches the MEM as a CONST address and folds
// into `stb rX,SceSys+N@l(rH)` (SceSetEventCancel's first store); a plain `SceSys.x = v` legitimises
// `&SceSys` into a lo_sum pseudo first and stores through `N(rP)`.
static inline void U8SetI(u8& d, int v) { d = v; }
// Event flag words at pG->flags_174, indexed by flag number; as an inline the base stays a pointer
// register (lwzx/stwx) instead of folding into the displacement.
static inline u32* eventFlags()
{
    return &pG->Room_flg[0];
}

cSceSys SceSys;
static ScePrim* pCSceTask;
// Struct-member view of pCSceTask (the pLog trick): the store keeps the following p->task load below it.
struct ScePrimPtr {
    ScePrim* p;
};
#define CSCE_TASK (((ScePrimPtr*) &pCSceTask)->p)
static u32 SceExecOt;

// Boot: clears the scenario system.
void ScenarioInit()
{
    memclr_asm(&SceSys, sizeof(cSceSys));
}

// Room start: resets the event / up-cut state and hand-over functions, kills the scenario tasks,
// restores the saved items, runs the room's init function and one scheduler pass (unless the
// debug "no scenario" flag), creates the partner when the save says she is with the player
// (Status_flg[3] 0x4000000), places the enemies from the list and sets up the areas.
void ScenarioRoomInit()
{
    // Store order decides the zero registers and the schedule: the six byte zeros come first, so cse
    // makes their QImode pseudo before any SImode zero exists (a later word zero's low part would
    // otherwise replace it), eventCancel is the last use of that pseudo and pause the last use of
    // the word zero (sched1 issues the dying store of each group first), and the sched2 anti-
    // dependence of the byte stores on the pG load's r9 ranks their group and its `li` last.
    SceSys.event_no_cut_back = 0;
    SceSys.stop_bak_flg = 0;
    SceSys.event_start_cnt = 0;
    SceSys.up_cut_start_cnt = 0;
    SceSys.task_kind_back = 0;
    SceSys.event_cancel_enable = 0;
    SceSys.sndFlag = 1;
    SceSys.cancelFlagNo = -1;
    SceSys.pExitFunc = 0;
    SceSys.pExitParam = 0;
    SceSys.pDoorFunc = 0;
    SceSys.pDoorParam = 0;
    SceSys.pCancelFunc = 0;
    SceSys.cancelArg = 0;
    SceSys.pLadderTask = 0;
    SceSys.m_door_fade_eff = 0;
    SceSys.m_item_get = 0;
    SceSys.pause = 0;
    ScfFlagOff(pGS, SCF_NO_ASHLEY_DIST_CK);
    ScenarioTaskAllOff();
    SceInitItemEvent();
    SceAtSetSaveItem();
    if (!DbgFlagChk(pG, DBG_NO_SCE_EXE)) {
        SceExecInitCondition();
        RoomData.execInitFunc(pG->room_id);
        SceSys.scheduler();
    }
    if (StaFlagChk(pG, STA_SUB_ASHLEY)) {
        SubCharInit(1, &pG->sub_pos, pG->sub_angle);
        SubCharCtrl(SCC_CHASE, 0);
    }
    EmSetFromList();
    SceAtRoomSet();
}

// Kills scenario slots 5..17 and empties the ordering table.
void ScenarioTaskAllOff()
{
    u32 i;

    ClearOTagR(SceSys.SceTaskOt, 16);
    memclr_asm(SceSys.prim, sizeof(SceSys.prim));
    for (i = 5; i <= 17; i++) {
        SceKill((int) i);
    }
}

// Per frame before the room script: debug print y reset, the deferred conditions checked.
void scenarioLoopBeforeInit()
{
    SceSys.m_debug_disp_y = 0x3C;
    if (pSUB != 0) {
        SceSys.m_debug_disp_y = 0x5A;
    }
    SceExecCheckCondition();
}

// Per frame after the tasks: sub-mission check and the moving collision areas.
void scenarioLoopAfterInit()
{
    SubMissionCheck();
    SceAtCheckMoveScrAt();
}

// Once per frame (game loop): unless an item pick-up or a pause holds the scenario, the event
// cancel key, the room's main function and the event manager run; then all scenario tasks
// (scheduler), then the after-hooks. Skipped in the debug "no scenario" mode.
void ScenarioMove()
{
    if (DbgFlagChk(pG, DBG_NO_SCE_EXE)) {
        return;
    }
    scenarioLoopBeforeInit();
    if (SceSys.m_item_get == 0 && SceSys.pause == 0) {
        if (!StaFlagChk(pG, STA_DIEDEMO)) {
            scenarioCheckEventCancel();
            RoomData.execMainFunc(pG->room_id);
        }
        if (!SpfFlagChk(pG, SPF_EVT) || (DbgFlagChk(pG, DBG_NO_EVENT))) {
            EvtMgr.Run();
        }
    }
    SceSys.scheduler();
    scenarioLoopAfterInit();
}

// Iteration start over the scenario tasks (ordering table head).
u32* scenarioSetOtStart()
{
    return &SceSys.SceTaskOt[15];
}

// Next ScePrim in the task ordering table after `p`; 0 at the end.
u32* scenarioGetOtAddr(u32* p)
{
    u32 v;

    while ((v = *p) != 0xFFFFFFFF) {
        p = (u32*) (v | 0x80000000);
        if ((s32) v < 0) {
            return p;
        }
    }
    return 0;
}

// Unlinks the ScePrim of task `t` from the ordering table (and drops its event-cancel role).
void SceTaskDelete(TASK* t)
{
    ScePrim* p = (ScePrim*) scenarioSetOtStart();

    while ((p = (ScePrim*) scenarioGetOtAddr((u32*) p)) != 0) {
        if (p->task == t) {
            DelPrim(&SceSys.SceTaskOt[15], (u32*) p);
            if (p->cancel == 1) {
                p->cancel = 0;
                SceSys.event_cancel_enable = 0;
            }
            break;
        }
    }
}

// Runs every linked scenario task once this frame in ordering-table order (all held while an item
// pick-up / pause runs, except slot 5); a task that exited is unlinked; in the room-load routine
// (Rno0 2) a task waiting on a read sleeps a frame, and `wait` inserts a GXDrawDone. The current
// task pointers are swapped in and out around each task.
void cSceSys::scheduler()
{
    OSThread* parent = pParentThread;
    TASK* ctask;
    ScePrim* p;
    u8 running;
    u32 i;
    int waitRead;

    pParentThread = 0;
    ctask = pCTask;
    if (SceSys.m_item_get == 0 && SceSys.pause == 0) {
        for (i = 0; i < 13; i++) {
            prim[i].running = 0;
        }
    } else {
        prim[0].running = 0;
        for (i = 1; i < 13; i++) {
            prim[i].running = 1;
        }
    }
    p = (ScePrim*) scenarioSetOtStart();
    while ((p = (ScePrim*) scenarioGetOtAddr((u32*) p)) != 0) {
        if (SpfFlagChk(pG, SPF_SCE)) {
            break;
        }
        running = p->running;
        if (running != 0) {
            continue;
        }
        p->running = 1;
        CSCE_TASK = p;
        pCTask = p->task;
        TaskSchedulerMain(p->task);
        if (pCTask->Status == 3) {
            SceTaskDelete(pCTask);
            pCTask->Status = running;
        }
        if (pG->Rno0 == 2) {
            waitRead = (m_init_loop_flag != 0) ? 1 : 0;
            if (waitRead) {
                pParentThread = parent;
                pCTask = ctask;
                TaskSleep(1);
                parent = pParentThread;
                ctask = pCTask;
                pParentThread = 0;
                p->running = 0;
            }
        }
        if (wait == 1) {
            pParentThread = parent;
            pCTask = ctask;
            GXDrawDone();
            parent = pParentThread;
            ctask = pCTask;
            pParentThread = 0;
            p->running = 0;
            wait = 0;
        }
        p = (ScePrim*) scenarioSetOtStart();
    }
    pParentThread = parent;
    pCTask = ctask;
}

// Starts a scenario task: prio 0 = call `func(arg)` directly; prio 5..17 = that scheduler slot,
// > 17 = the highest free slot. Linked into ordering slot otPrio (SCE_PRIO_*), OS priority 0xE,
// `model` as the task's model; `flag` (0 = inherit the caller's event kind). Warns when fewer
// than 3 slots remain. Returns the ScePrim, 0 when none is free.
ScePrim* SceExec(int prio, TaskFunc func, int arg, u8 flag, int otPrio, void* model)
{
    TASK* t;
    ScePrim* p;
    u32 no;
    int busy;
    int i;

    if (prio == 0) {
        SetTaskModelPtr(model, 0);
        ((void (*)(int)) func)(arg);
        return 0;
    }
    if ((u32) prio > 17) {
        // A goto loop: no loop notes, so the Task[no] address is not strength-reduced.
        prio = 0;
        no = 17;
        if (Task[no].Status == 0) {
            prio = 17;
        } else {
        retry:
            no--;
            if (no > 5) {
                if (Task[no].Status != 0) {
                    goto retry;
                }
                prio = no;
            }
        }
    }
    if ((u32) (prio - 5) > 12) {
        pLog->err(0, 0, "SCE_TASK DON'T EXEC");
        return 0;
    }
    t = TaskExec(prio, func, arg);
    if (t == 0) {
        return 0;
    }
    p = &SceSys.prim[prio - 5];
    p->cancel = 0;
    p->task = t;
    AddPrim(&SceSys.SceTaskOt[otPrio], (u32*) p);
    t->Priority = 0xE;
    SetTaskModelPtr(model, t);
    if (flag != 0) {
        t->flag = flag;
    } else if (SceSys.checkCTaskRange() == 0) {
        t->flag = 1;
    } else {
        t->flag = pCTask->flag;
    }
    if (!DbgFlagChk(pG, DBG_WARN_LEVEL_LOW)) {
        busy = 0;
        for (i = 17; i >= 6; i--) {
            if (Task[i].Status != 0) {
                busy++;
            }
        }
        if ((u32) (12 - busy) <= 2) {
            pLog->warn(0, 0, "SCE_TASK remain num %d", 12 - busy);
        }
    }
    return p;
}

// TaskSleep, only from inside a scenario task.
void SceSleep(int frames)
{
    if (SceSys.checkCTaskRange() != 0) {
        TaskSleep(frames);
    }
}

// Ends the calling scenario task (unlinked, TaskExit).
void SceExit()
{
    if (SceSys.checkCTaskRange() != 0) {
        SceTaskDelete(pCTask);
        TaskExit();
    }
}

// Kills scenario slot `prio`.
void SceKill(int prio)
{
    SceTaskDelete(&Task[prio]);
    TaskKill(prio);
}

// Kills the task of a ScePrim.
void SceKill(ScePrim* p)
{
    SceTaskDelete(p->task);
    TaskKill(p->task);
}

// Kills scenario task `t`.
void SceKill(TASK* t)
{
    SceTaskDelete(t);
    TaskKill(t);
}

// Kills every scenario task running `func`.
void SceKill(void (*func)(int))
{
    ScePrim* p = (ScePrim*) scenarioSetOtStart();

    while ((p = (ScePrim*) scenarioGetOtAddr((u32*) p)) != 0) {
        if (p->task->pFunc == func) {
            SceKill(p->task);
        }
    }
}

// 1 when the current task is a scenario task (slot 5..17).
int cSceSys::checkCTaskRange()
{
    if (pCTask == 0) {
        return 0;
    }
    if (pCTask == CTASK_MAIN) {
        return 0;
    }
    return (u32) (pCTask->Task_no - 5) <= 12;
}

// The ScePrim of the scenario task being run.
ScePrim* SceCTask()
{
    return pCSceTask;
}

// Empties the deferred-condition list.
void SceExecInitCondition()
{
    ClearOTagR(&SceExecOt, 1);
}

// Em_flg row address as an integer (the original adds the list offset after the row index), as in sce_at.
static inline u32 emDeadRow(int n)
{
    return n * 32 + (u32) pG + 0x501C;
}

// Is the condition met? type 0 enemy list entry dead (Em_flg bit), 1 camera area == param, 2
// enemy `param` dead and in its die routine, 3 callback returns 1, 4 etc model `param` broken, 5
// item area `param` taken.
int SceExecCheckCondition_sub(SceCond* pP)
{
    cEm* em;
    u32* row;
    u32 no;
    u32 bit;

    switch (pP->type) {
    case 0:
        no = (u32) pP->param;
        if (pG->em_list_no >= 0) {
            // Row address as integer arithmetic (index first, the list offset added last), like sce_at.
            bit = *(u32*) (((no >> 5) << 2) + emDeadRow(pG->em_list_no)) & (0x80000000 >> (no & 31));
        } else {
            bit = 0;
        }
        if (bit != 0) {
            return 1;
        }
        break;
    case 1:
        if (CamCtrl.CurrentAreaNo() == (int) pP->param) {
            return 1;
        }
        break;
    case 2:
        em = (cEm*) pP->param;
        // Raw (non-struct) read: keeps the load behind the store of `em` to its stack slot.
        if (*(s16*) ((u32) em + 0x320) <= 0 && em->r_no_0 == 3) {
            return 1;
        }
        break;
    case 3:
        if (((int (*)()) pP->param)() == 1) {
            return 1;
        }
        break;
    case 4:
        if (getRoomEtcBreak(pP->param, &em, 1) == 1 && em->hp <= 0) {
            return 1;
        }
        break;
    case 5:
        if (SceAtPtr((int) pP->param) != 0 && SceAtItemFlgCk((int) pP->param) == 1) {
            return 1;
        }
        break;
    }
    return 0;
}

// Per frame: fires (SceExec) and frees every deferred condition that is met.
void SceExecCheckCondition()
{
    u32 v = SceExecOt;
    SceCond* c;

    if (v == 0xFFFFFFFF) {
        return;
    }
    do {
        c = (SceCond*) (v | 0x80000000);
        if ((s32) v < 0) {
            if (SceExecCheckCondition_sub(c) == 1) {
                if (c->func != 0) {
                    SceExec(c->prio, c->func, c->arg, c->flag, SCE_PRIO_DEF_2, 0);
                }
                DelPrim(&SceExecOt, (u32*) c);
                Mem_free(c);
            }
        }
        v = c->next;
    } while (v != 0xFFFFFFFF);
}

// Room: run `func(arg)` as a scenario task (prio, flag) once condition `type` / `param` holds.
void SceExecLinkCondition(int type, void* param, u8 prio, TaskFunc func, int arg, u8 flag)
{
#line 608
    SceCond* c = (SceCond*) MEM_ALLOC(sizeof(SceCond), 1, 13);

    c->type = type;
    c->param = param;
    c->prio = prio;
    c->func = func;
    c->arg = arg;
    c->flag = flag;
    AddPrim(&SceExecOt, (u32*) c);
}

// Room: run `func(arg)` when enemy `param` has died.
void SceExecLinkEmDead(void* param, u8 prio, TaskFunc func, int arg, u8 flag)
{
    SceExecLinkCondition(2, param, prio, func, arg, flag);
}

// 1 when `em` is alive, shown, active (be_flag 1 / 2 / 0x20), has hp and is not the player.
int EmMoveActiveCheck(cEm* em)
{
    if (!(em->be_flag & 1)) {
        return 0;
    }
    if (!(em->be_flag & 0x20)) {
        return 0;
    }
    if (!(em->be_flag & 2)) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (em == pPL) {
        return 0;
    }
    return 1;
}

// Skips the running event (the START key during a cancellable event): Stop_flg restored,
// messages closed, event stream stopped (sndFlag), the cancel flag set, the event task killed and
// the registered continuation started in its slot, fade back in.
void SceExecEventCancel()
{
    MessageControl* mes = &cMes;
    cSceSys* s;
    int i;
    u32 no;
    u32 slot;

    pG->Stop_flg = SceSys.cancel_stop_bak;
    for (i = 0; i <= 0xF; i++) {
        mes->Delete(i);
    }
    s = &SceSys;
    if (s->sndFlag == 1) {
        SndEventStrStop(0);
    }
    if (s->cancelFlagNo >= 0) {
        no = s->cancelFlagNo;
        eventFlags()[no >> 5] |= 0x80000000 >> (no & 31);
    }
    for (slot = 5; slot <= 17; slot++) {
        if (s->prim[slot - 5].cancel == 1) {
            SceKill((int) slot);
            s->prim[slot - 5].cancel = 0;
            break;
        }
    }
    if (SceSys.pCancelFunc != 0) {
        SceExec(slot, SceSys.pCancelFunc, SceSys.cancelArg, 2, SCE_PRIO_DEF_2, 0);
    }
    FadeSetW(0x80000000, 10, 0, 0);
}

// Event script: makes the calling task skippable (`on`): `func(arg)` runs after a skip, event flag
// `flagNo` (cleared now) is set by the skip, sndFlag = stop the event stream too.
void SceSetEventCancel(int on, TaskFunc func, int arg, int flagNo, int sndFlag)
{
    u32 no;
    cSceSys* s;

    if (on != 1) {
        on = 0;
    }
    U8SetI(SceSys.event_cancel_enable, on);
    SceCTask()->cancel = on;
    if (flagNo >= 0) {
        no = flagNo;
        eventFlags()[no >> 5] &= ~(0x80000000 >> (no & 31));
    }
    // COMPILER-DIFF: 12 (AROUND form): the loop notes end cse1's path from the skipped `if` block, so
    // the tail's `&SceSys` is a fresh lis/addi instead of `eventCancel's address - 113`.
    do { } while (0);
    SceSys.pCancelFunc = func;
    SceSys.cancelArg = arg;
    SceSys.cancelFlagNo = flagNo;
    SceSys.sndFlag = sndFlag;
    {
        // COMPILER-DIFF: candidate #17 (global.c pass 0 regs_used_so_far): r30 used-so-far makes `on`
        // take r30 in pass 0 and flagNo r31 (stock priorities give on r31, flagNo r30).
        register int pin asm("r30");
        asm volatile("" : "=r"(pin));
    }
}

// Per frame: the skip key (Key 0x20000000) during a cancellable event fades to black and runs
// SceExecEventCancel. Returns 1 when skipped.
int scenarioCheckEventCancel()
{
    cSceSys* s = &SceSys;
    GXColor start;
    GXColor end;

    if (s->event_cancel_enable == 1 && (Key.trg & 0x20000000)) {
        *(u32*) &end = 0xFF;
        *(u32*) &start = 0;
        FadeSet(0, &start, &end, 0, 0, 0);
        s->cancel_stop_bak = pG->Stop_flg;
        pG->Stop_flg = 0xFFFFFFFF;
        KeyStop(0xEFCF0000);
        SceExecEventCancel();
        s->event_cancel_enable = 0;
        SubScreenWait(0x14);
        return 1;
    }
    return 0;
}
