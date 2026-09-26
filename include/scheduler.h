#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"

// Dolphin OS thread/semaphore types for game code. The SDK's dolphin/types.h drags in the
// CodeWarrior libc, so its guard is defined here and the OS headers are included directly.
#define _DOLPHIN_TYPES_H_
#include <dolphin/os/OSThread.h>
#include <dolphin/os/OSSemaphore.h>

// Cooperative task scheduler (game/scheduler.cpp). One OSThread per task slot; TaskScheduler
// runs slots 0..4 once a frame, iTaskScheduler runs slot 4 from interrupt level.
#define TASK_NUM 18
#define TASK_ISR 4  // slot used by iTaskExec

// status: low 7 bits are the state, bit 7 = suspended by TaskSuspend
#define TASK_NONE 0
#define TASK_EXEC 1   // requested, thread not created yet
#define TASK_SLEEP 2  // sleeping `sleep` frames
#define TASK_RUN 3
#define TASK_SUSPEND 0x80

struct TASK {
    u8* pStack;                // 0x00  stack top
    u8 Status;                // 0x04
    u8 Priority;                 // 0x05  > 0xF: waits on the scheduler semaphore
    u8 Task_no;                    // 0x06
private:
    u8 kind;                  // 0x07  2: skip while flags_5010 bit 28, 4: skip while flags_500C bit 20
public:
    u16 suspend_cnt;          // 0x08
    u16 SleepCtr;                // 0x0A
    u16 StackSize;           // 0x0C
    u16 pad_E;
    int arg;                  // 0x10
    u8 pad_14[4];
    OSThread Thread;          // 0x18
    OSThreadQueue Queue;      // 0x330
    void* (*hook)(void*);     // 0x338
    void (*pFunc)(int);        // 0x33C
    void* pModel;              // 0x340
    u8 pad_344[4];

    int isNoSuspend() { return (kind >> 1) & 1; }
    void setNoSuspend(int on)
    {
        if (on) {
            kind |= 2;
        } else {
            kind &= ~2;
        }
    }
    int isDiedemoMove() { return (kind >> 2) & 1; }
    void setDiedemoMove(int on)
    {
        if (on) {
            kind |= 4;
        } else {
            kind &= ~4;
        }
    }
    void setKind(u8 k) { kind = k; }
    u8 getKind() { return kind; }
};                            // 0x348

extern TASK* CTASK_MAIN;  // sentinel "main thread" task (-1)
extern TASK* pCTask;      // task currently being scheduled
extern OSThread* pParentThread;  // thread to return to from the scheduler
extern TASK Task[TASK_NUM];

void TaskSleep(int ctr);
void TaskExit();
void TaskSuspend(int level);
void TaskSignal(int level);
extern "C" {
// Inside extern "C" GCC 2.95 reads `void (*)()` as `void (*)(...)` and then mangles a function
// taking it by value; the same type through a typedef keeps C linkage.
typedef void (*TaskFunc)();
void TaskSchedulerInit();
void TaskAllClear();
u32 GetStackSize(int level);
void TaskScheduler();
void TaskSchedulerMain(TASK* pT);
void stackUsedCheck();
void StackOverflowCheck(TASK* pTask);
void* TaskExec_hook(void* value);
TASK* TaskExec(int prio, TaskFunc func, int arg);
void TaskChain(TaskFunc func, int arg);
void TaskKill(int prio);
u8 TaskStatus(int level);
void SetTaskModelPtr(void* model, TASK* t);
// interrupt-level task variants (dvd.cpp uses them for reads flagged 0x100)
void iTaskScheduler();
TASK* iTaskExec(TaskFunc func);
void iTaskKill();
void iTaskExit();
void iTaskSuspend();
int iTaskStatus();
}

// C++ overload: kill the task `t` (sce_sys SceKill).
void TaskKill(TASK* t);

#endif
