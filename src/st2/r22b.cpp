#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "fade.h"

// Room 2-2B (D:/Bio4/Prog/r22b.cpp): the s00 event that ends chapter 4-1 (SceSetChapterEnd(CHAPTER_4_1))
// and its fades; nothing else is in the room.

struct R22bWork {
    u8 dummy;
};

static R22bWork* r22b_work;

extern "C" void R22bEventS00();
extern "C" void Evt_R22bS00_Func(Event* e);


// Room init: registers the s00 callback and, unless Room_flg bit 0 (seen), pre-loads r22bs00 and starts the event task.
void R22bInit()
{
#line 33 "D:/Bio4/Prog/r22b.cpp"
    r22b_work = (R22bWork*) MEM_CALLOC(sizeof(R22bWork), 1, 0xd);
    EvtMgr.SetFunc("evt_r22bs00_func", (void*) Evt_R22bS00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceExec(0x12, (TaskFunc) R22bEventS00, 0, 2, SCE_PRIO_DEF_2, 0);
        EvtMgr.EvtReadAram("event/evd/r22bs00.evd", 0, 0, 0, 0);
    }
}

// Per-frame room main: nothing.
void R22bMain()
{
}

// Once (Room_flg bit 0): System_flg 0x400 (no pause / no room change), play r22bs00, then end chapter 4-1.
extern "C" void R22bEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        EvtMgr.EvtReadExec("event/evd/r22bs00.evd", 0, EvtReadFlagNone);
        SceSetChapterEnd(CHAPTER_4_1, 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
    }
}

// Event r22bs00 callback: fade-outs / fade-ins at fixed frames of cuts 0 and 1 (skipped when the event is skipped).
extern "C" void Evt_R22bS00_Func(Event* e)
{
    if (e->FuncType == 1) {
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                int skip = e->FlgCkStatus(EvtStfToolFrontExec);

                if (skip == 0) {
                    FadeSetW(0x80000002, 25, 0, 0);
                }
            }
            if (e->NowFrame == 0x4E) {
                int skip = e->FlgCkStatus(EvtStfToolFrontExec);

                if (skip == 0) {
                    FadeSetW(2, 12, 0, 0);
                }
            }
            break;
        case 1:
            if (e->NowFrame == 0) {
                int skip = e->FlgCkStatus(EvtStfToolFrontExec);

                if (skip == 0) {
                    FadeSetW(0x80000002, 12, 0, 0);
                }
            }
            if (e->NowFrame == 0x7B) {
                int skip = e->FlgCkStatus(EvtStfToolFrontExec);

                if (skip == 0) {
                    FadeSetW(2, 6, 0, 0);
                }
            }
            break;
        case 2:
            if (e->NowFrame == 0) {
                int skip = e->FlgCkStatus(EvtStfToolFrontExec);

                if (skip == 0) {
                    FadeSetW(2, 0, 0, 0);
                }
            }
            if (e->NowFrame == 6) {
                int skip = e->FlgCkStatus(EvtStfToolFrontExec);

                if (skip == 0) {
                    FadeSetW(0x80000002, 5, 0, 0);
                }
            }
            break;
        case 5:
            if (e->NowFrame == 0x59) {
                int skip = e->FlgCkStatus(EvtStfToolFrontExec);

                if (skip == 0) {
                    FadeSetW(2, 25, 0, 0);
                }
            }
            break;
        }
    }
}
