#include "types.h"
#include "room_data.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "sofdec.h"
#include "event.h"

// Stage 3 (island) room module entry (D:/Bio4/Prog/st3.cpp, the same object ends every st3_* REL): registers
// the room Init/Main pairs of every stage-3 room in the DOL's St3_data_tbl, then the SN REL entry points (like
// st2.cpp/st4.cpp). Includes map_obj.h/light.h/widget.h/sofdec.h/event.h (their strings and the cManager<cLight>
// template block follow the code) and carries a never-called inline whose "movie/r333_ev.sfd" string survives.

extern "C" void OSReport(const char* fmt, ...);

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

extern RoomTblEntry St3_data_tbl[52];

// _ctors/_dtors: the linker script's labels on the .ctors/.dtors lists (null terminated)
extern void (*_ctors[])(void);
extern void (*_dtors[])(void);

void R300Init();
void R300Main();
void R301Init();
void R301Main();
void R303Init();
void R303Main();
void R304Init();
void R304Main();
void R305Init();
void R305Main();
void R306Init();
void R306Main();
void R307Init();
void R307Main();
void R308Init();
void R308Main();
void R309Init();
void R309Main();
void R30aInit();
void R30aMain();
void R30bInit();
void R30bMain();
void R30cInit();
void R30cMain();
void R30dInit();
void R30dMain();
void R30eInit();
void R30eMain();
void R30fInit();
void R30fMain();
void R310Init();
void R310Main();
void R311Init();
void R311Main();
void R312Init();
void R312Main();
void R315Init();
void R315Main();
void R316Init();
void R316Main();
void R317Init();
void R317Main();
void R318Init();
void R318Main();
void R31aInit();
void R31aMain();
void R31bInit();
void R31bMain();
void R31cInit();
void R31cMain();
void R31dInit();
void R31dMain();
void R320Init();
void R320Main();
void R321Init();
void R321Main();
void R325Init();
void R325Main();
void R326Init();
void R326Main();
void R327Init();
void R327Main();
void R328Init();
void R328Main();
void R329Init();
void R329Main();
void R330Init();
void R330Main();
void R331Init();
void R331Main();
void R332Init();
void R332Main();
void R333Init();
void R333Main();

// Store one room's Init/Main pair into the DOL's St3_data_tbl (index = room number & 0xFF).
void set(int no, void (*init)(), void (*main)())
{
    St3_data_tbl[no].init = init;
    St3_data_tbl[no].main = main;
}

// Fill the stage table with every room this module compiles (missing indices are rooms that do not exist).
void setTbl()
{
    set(0, R300Init, R300Main);
    set(1, R301Init, R301Main);
    set(3, R303Init, R303Main);
    set(4, R304Init, R304Main);
    set(5, R305Init, R305Main);
    set(6, R306Init, R306Main);
    set(7, R307Init, R307Main);
    set(8, R308Init, R308Main);
    set(9, R309Init, R309Main);
    set(10, R30aInit, R30aMain);
    set(11, R30bInit, R30bMain);
    set(12, R30cInit, R30cMain);
    set(13, R30dInit, R30dMain);
    set(14, R30eInit, R30eMain);
    set(15, R30fInit, R30fMain);
    set(16, R310Init, R310Main);
    set(17, R311Init, R311Main);
    set(18, R312Init, R312Main);
    set(21, R315Init, R315Main);
    set(22, R316Init, R316Main);
    set(23, R317Init, R317Main);
    set(24, R318Init, R318Main);
    set(26, R31aInit, R31aMain);
    set(27, R31bInit, R31bMain);
    set(28, R31cInit, R31cMain);
    set(29, R31dInit, R31dMain);
    set(32, R320Init, R320Main);
    set(33, R321Init, R321Main);
    set(37, R325Init, R325Main);
    set(38, R326Init, R326Main);
    set(39, R327Init, R327Main);
    set(40, R328Init, R328Main);
    set(41, R329Init, R329Main);
    set(48, R330Init, R330Main);
    set(49, R331Init, R331Main);
    set(50, R332Init, R332Main);
    set(51, R333Init, R333Main);
}

// REL entry point (called by the loader after linking): run the static constructors, then register the rooms.
extern "C" void _prolog()
{
    void (**p)(void);

    for (p = _ctors; *p; p++) {
        (*p)();
    }
    setTbl();
    OSReport("prolog...\n");
}

// REL exit point (before unlinking): run the static destructors.
extern "C" void _epilog()
{
    void (**p)(void);

    for (p = _dtors; *p; p++) {
        (*p)();
    }
    OSReport("epilog...\n");
}

// Stub the loader binds unresolved imports to: reports and halts (the HALT line number is baked in).
extern "C" void _unresolved()
{
    OSReport("unresolved...\n");
#line 195 "D:/Bio4/Prog/st3.cpp"
    HALT();
}

// The island count-down (Cckpt's CountDown): r331 starts it, every island room's Main polls it through
// st3_checkCountDown, and st3_dieDemoEvent plays the r333 movie and the death demo when it runs out. Only
// st3_3's copy of this object keeps these bodies (r331 calls them); the other modules' links dead-stripped
// them like em_wrap.cpp's members (strings kept: "movie/r333_ev.sfd" is st3_dieDemoEvent's).
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "main_sub.h"
#include "game.h"
#include "player.h"
#include "cockpit.h"
#include "datactrl.h"
#include "snd.h"
#include "fade.h"

void st3_checkCountDown();
void st3_dieDemoEvent();
void st3_endCountDown();

// Reference store: pG is reloaded after it.
static inline void S16Set(s16& d, s16 v) { d = v; }

// Sets the count-down (frames, clamped at 0) and mirrors it into free word 2.
void st3_setCountDownTimer(int frame)
{
    CountDown* cd;

    if (frame < 0) {
        frame = 0;
    }
    cd = Cckpt.getCountDown();
    cd->m_state |= 1;
    cd->initTimeFrame(frame);
    SetFree(2, cd->getFrame());
}

// Frames left on the cockpit count-down.
int st3_getCountDownTimer()
{
    return Cckpt.getCountDown()->getFrame();
}

// Resumes the count-down from free word 2 and shows it.
static inline void st3_resumeCountDown()
{
    u32 frame = GetFree(2);
    CountDown* cd = Cckpt.getCountDown();

    cd->m_state |= 1;
    cd->initTimeFrame(frame);
    cd->frameIn();
}

// Start (or resume after a room change) the island escape count-down: Scenario_flg[1] 0x80 = running,
// Scenario_flg[2] 0x200 (time-over handled) cleared; the frame count lives in free word 2 across rooms.
void st3_startCountDown()
{
    ScfFlagOff(pG, SCF_ST3_COUNT_DOWN_DIE);
    if (ScfFlagChk(pG, SCF_ST3_COUNT_DOWN_START) == 0) {
        ScfFlagOn(pG, SCF_ST3_COUNT_DOWN_START);
        st3_resumeCountDown();
    } else {
        st3_resumeCountDown();
    }
}

// Every island room's Main: when the running count-down reaches zero, the death demo event.
void st3_checkCountDown()
{
    if (ScfFlagChk(pG, SCF_ST3_COUNT_DOWN_START)) {
        int over = 0;
        CountDown* cd = Cckpt.getCountDown();

        SetFree(2, cd->getFrame());
        if (cd->checkState(1)) {
            over = (cd->m_frame == 0);
        }
        if (over == 1) {
            if (ScfFlagChk(pG, SCF_ST3_COUNT_DOWN_DIE) == 0) {
                ScfFlagOn(pG, SCF_ST3_COUNT_DOWN_DIE);
                ScenarioTaskAllOff();
                SceExec(0x12, (TaskFunc) st3_dieDemoEvent, 0, 2, 2, 0);
            }
        }
    }
}

// Time over: black screen, count-down off, all data / events dropped, sounds stopped, the r333_ev movie
// (the island explodes) queued, Leon's hp zeroed with Status_flg[3] 0x01000000, then the death demo.
void st3_dieDemoEvent()
{
    int i;

    SceEventStart(0);
    systemVISetBlack(1);
    st3_endCountDown();
    DC.deleteAll();
    EvtMgr.DelAll();
    SceSleep(1);
    SndAllStop();
    for (i = 5; i != 0; i--) {
        SceSleep(1);
    }
    systemVISetBlack(1);
    Sofdec.Initialize("movie/r333_ev.sfd", 0);
    SceSleep(1);
    st3_endCountDown();
    FadeSetW(2, 0, 0, 0);
    SceSleep(1);
    S16Set(pPL->hp, 0);
    StaFlagOn(pG, STA_EVENT_CANCEL);
    DiedemoExec(0, 1);
    SceSleep(1);
    SceEventEnd(0);
}

// Stop and hide the count-down (Scenario_flg[1] 0x80 off).
void st3_endCountDown()
{
    CountDown* cd = Cckpt.getCountDown();

    ScfFlagOff(pG, SCF_ST3_COUNT_DOWN_START);
    cd->disp(0);
    cd->m_state &= ~1;
    cd->frameOut();
}

// The count-down state test: the module build had it inline in the header after the class (a linkonce copy
// follows the code; the DOL's is game/mercenaries.cpp's).
inline int CountDown::checkState(u32 bit)
{
    return (m_state & bit) ? 1 : 0;
}
