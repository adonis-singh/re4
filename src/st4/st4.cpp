#include "types.h"
#include "room_data.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "global.h"

// Stage 4 room module entry (D:/Bio4/Prog/st4.cpp, the last object of st4_0): registers the room
// Init/Main pairs of every stage-4 room in the DOL's St4_data_tbl, then the SN REL entry points (like
// st2.cpp). Unlike st1.cpp/st2.cpp it includes map_obj.h/light.h/widget.h/atari.h (their strings and the
// cManager<cLight> template block follow the code) and carries an unused static helper.

extern "C" void OSReport(const char* fmt, ...);

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

extern RoomTblEntry St4_data_tbl[18];

// _ctors/_dtors: the linker script's labels on the .ctors/.dtors lists (null terminated)
extern void (*_ctors[])(void);
extern void (*_dtors[])(void);

void R400Init();
void R400Main();
void R402Init();
void R402Main();
void R403Init();
void R403Main();
void R404Init();
void R404Main();
void R405Init();
void R405Main();
void R406Init();
void R406Main();
void R40aInit();
void R40aMain();
void R40bInit();
void R40bMain();
void R40cInit();
void R40cMain();
void R40dInit();
void R40dMain();
void R40eInit();
void R40eMain();
void R40fInit();
void R40fMain();
void R410Init();
void R410Main();
void R411Init();
void R411Main();

// Store one room's Init/Main pair into the DOL's St4_data_tbl (index = room number & 0xFF).
void set(int no, void (*init)(), void (*main)())
{
    St4_data_tbl[no].init = init;
    St4_data_tbl[no].main = main;
}

// Fill the stage table with every room this module compiles (missing indices are rooms that do not exist).
void setTbl()
{
    set(0, R400Init, R400Main);
    set(2, R402Init, R402Main);
    set(3, R403Init, R403Main);
    set(4, R404Init, R404Main);
    set(5, R405Init, R405Main);
    set(6, R406Init, R406Main);
    set(10, R40aInit, R40aMain);
    set(11, R40bInit, R40bMain);
    set(12, R40cInit, R40cMain);
    set(13, R40dInit, R40dMain);
    set(14, R40eInit, R40eMain);
    set(15, R40fInit, R40fMain);
    set(16, R410Init, R410Main);
    set(17, R411Init, R411Main);
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
#line 121 "D:/Bio4/Prog/st4.cpp"
    HALT();
}

// R405Init (r405.cpp) calls it: the Ada game flag
void st4_initAdaGame()
{
    if (!ScfFlagChk(pG, SCF_R405_ADA_GAME_INIT)) {
        ScfFlagOn(pG, SCF_R405_ADA_GAME_INIT);
    }
}
