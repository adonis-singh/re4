#ifndef SCE_SYS_H
#define SCE_SYS_H

#include "types.h"
#include "em.h"
#include "scheduler.h"
#include "global.h"

// game/sce_sys.cpp: scenario task system. Scenario tasks run in scheduler slots 5..17 and are
// linked into an ordering table (libgpu OTag) by priority.

// One scenario task (0xC bytes), SceSys.prim[slot - 5]
struct SCE_TASK {
    u32 tag;          // 0x00  OTag link
    u8 exec_flag;     // 0x04  scheduled this frame
    u8 cancel_flag;   // 0x05  1 = this task is the event-cancel target
    u8 pad_6[2];
private:
    TASK* pTask;      // 0x08
public:
    TASK* getTaskPtr() { return pTask; }
    void setTaskPtr(TASK* task) { pTask = task; }
    int isNoSuspend() { return pTask->isNoSuspend(); }
    void setNoSuspend(int on) { pTask->setNoSuspend(on); }
    int isDiedemoMove() { return pTask->isDiedemoMove(); }
    void setDiedemoMove(int on) { pTask->setDiedemoMove(on); }
    void setKind(u8 kind) { pTask->setKind(kind); }
    u8 getKind() { return pTask->getKind(); }
};

// Deferred scenario execution condition (0x14 bytes, SceExecOt list)
struct SceCond {
    u32 next;             // 0x00
    void (*func)();       // 0x04
    void* arg;            // 0x08
    u8 prio;              // 0x0C
    u8 flag;              // 0x0D
    u8 type;              // 0x0E  0 em dead flag, 1 camera area, 2 enemy life, 3 callback, 4 etc break, 5 item flag
    u8 pad_F;
    void* param;          // 0x10
};

class cSceSys {
private:
    int m_draw_done;      // 0x00  1 = GXDrawDone before the next task
    int m_chapter_end;    // 0x04  nonzero from SceSetChapterEnd until the chapter end task runs: scenario stopped
public:
    void (*pExitFunc)();         // 0x08
    void* pExitParam;             // 0x0C
    void (*pDoorFunc)();        // 0x10
    void* pDoorParam;            // 0x14
    void (*pCancelFunc)(); // 0x18  task started by SceExecEventCancel
    int pCancelParam;        // 0x1C
    u32 SceTaskOt[16];         // 0x20  ordering table, otag[15] is the list head
    u32 stop_bak;         // 0x60  pG->Stop_flg saved by SceUpCutStart
    u32 system_bak;              // 0x64  pG->flags_54 saved by SceEventStart
    u32 cancel_stop_bak;         // 0x68  pG->flags_170 before the event cancel
    u8 stop_bak_flg;               // 0x6C  1 = an up-cut is running (flags_170 saved in x60)
    u8 event_no_cut_back;               // 0x6D  1 = SceEventStart(0) told the managers
    u8 event_start_cnt;               // 0x6E  SceEventStart nesting count
    u8 up_cut_start_cnt;               // 0x6F
    u8 task_kind_back;               // 0x70  task flag saved by SceEventStart / SceUpCutStart
    u8 event_cancel_enable;       // 0x71  1 = the running event may be cancelled
    s8 m_room_flag;      // 0x72  flags_174 bit set when the event is cancelled (-1 = none)
    u8 m_init_loop_flag;  // 0x73  set while readEmData waits inside a scenario task
    u8 m_chapter_no;      // 0x74  chapter number (SceSetChapterEnd)
    u8 m_door_fade_eff;   // 0x75  fade effect at the door jump (SceAtDoor doorFadeEff; game: 0 filter fade, 1 quick fade)
    u8 m_item_get;        // 0x76  set while the sceAtGetItem task runs (SceSys move skips while set)
    u8 x77;               // 0x77
    s16 m_chapter_door;   // 0x78  door area the chapter end returns through (-1 = none; sce_com)
    u16 m_debug_disp_y;   // 0x7A  debug print y (0x3C, 0x5A with a sub character; sce_com +0xF per line)
    int m_str_stop_flag;  // 0x7C  1 = SndEventStrStop on event cancel
    cDmgInfo m_dmg_bak;         // 0x80
    SCE_TASK prim[13];     // 0x98  slots 5..17
    SCE_TASK* pLadderTask; // 0x134  sceAtLadder task (SceEventStart(!0) kills it)

    void scheduler();
    void setInitLoop(int flag) { m_init_loop_flag = flag; }
    bool checkInitLoop() { return m_init_loop_flag; }
    int checkCTaskRange();
    void setDrawDone(int flag) { m_draw_done = flag; }
    int checkDrawDone() { return m_draw_done; }
    void setChapterEnd(int chapter_no, int door)
    {
        m_chapter_end = 1;
        m_chapter_no = chapter_no;
        m_chapter_door = door;
    }
    void clearChapterEnd() { m_chapter_end = 0; }
    int checkChapterEnd() { return m_chapter_end; }
};

extern cSceSys SceSys;

extern "C" {
void ScenarioInit();
void ScenarioRoomInit();
void ScenarioTaskAllOff();
void scenarioLoopBeforeInit();
void scenarioLoopAfterInit();
void ScenarioMove();
u32* scenarioSetOtStart();
u32* scenarioGetOtAddr(u32* pSceOt);
void SceTaskDelete(TASK* t);
enum SCE_PRIORITY {
    SCE_PRIO_0 = 0,
    SCE_PRIO_1 = 1,
    SCE_PRIO_2 = 2,
    SCE_PRIO_3 = 3,
    SCE_PRIO_4 = 4,
    SCE_PRIO_5 = 5,
    SCE_PRIO_6 = 6,
    SCE_PRIO_7 = 7,
    SCE_PRIO_8 = 8,
    SCE_PRIO_9 = 9,
    SCE_PRIO_10 = 10,
    SCE_PRIO_11 = 11,
    SCE_PRIO_12 = 12,
    SCE_PRIO_13 = 13,
    SCE_PRIO_14 = 14,
    SCE_PRIO_15 = 15,
    SCE_PRIO_DEF_0 = 0,
    SCE_PRIO_DEF_1 = 1,
    SCE_PRIO_DEF_2 = 2,
    SCE_PRIO_ACT = 5,
    SCE_PRIO_ACT_2 = 6,
    SCE_PRIO_ACT_3 = 7,
    SCE_PRIO_GET = 8,
    SCE_PRIO_ATTACK = 11,
    SCE_PRIO_ATTACK_2 = 12
};

SCE_TASK* SceExec(int prio, TaskFunc func, int arg, u8 flag, int otPrio, void* model);
void SceSleep(int ctr);
void SceExit();
SCE_TASK* SceCTask();
void SceExecInitCondition();
int SceExecCheckCondition_sub(SceCond* pP);
void SceExecCheckCondition();
void SceExecLinkCondition(int type, void* param, u8 prio, TaskFunc func, void* arg, u8 flag);
void SceExecLinkEmDead(void* param, u8 prio, TaskFunc func, void* arg, u8 flag);
int EmMoveActiveCheck(cEm* pEm);
void SceExecEventCancel();
void SceSetEventCancel(int on, TaskFunc func, int arg, int flagNo, int sndFlag);
int scenarioCheckEventCancel();
}

void SceKill(u32 level);
void SceKill(SCE_TASK* p);
void SceKill(TASK* t);
void SceKill(void (*func)(int));

// Em_flg row address as an integer (the original adds the list offset after the row index) (sce_at, sce_sys).
static inline u32 emDeadRow(int n)
{
    return (u32) EM_FLG_ROW(n);
}

#endif
