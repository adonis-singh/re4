#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emdoor.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "motion.h"
#include "atariInfo.h"
#include "mercenaries.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "rnd.h"
#include "eprintf.h"
#include "TexRender.h"
#include "db_log.h"
#include <string.h>


// Room 4-04 (D:/Bio4/Prog/r404.cpp): the Mercenaries castle courtyard; the enemy waves per area, the
// chainsaw sister after enough kills, the banister slide and the three treasure cases.

struct R404Work {
    u32 cnt;             // 0x00  enemies alive (SceCountEmAlive)
    u32 emId;            // 0x04  id of list entry 0xF5 (the chainsaw one)
    u32 total;           // 0x08  enemies set so far
    TexRenderMng* tex;   // 0x0C
    cObj* slide;         // 0x10  the banister slide object (SetObjSmd)
    u32 cnt1_4;          // 0x14  per-area set counts
    u32 cnt2_5;          // 0x18
    u32 cnt3;            // 0x1C
    u32 cnt6;            // 0x20
    u32 cnt13;           // 0x24
    u32 cnt14;           // 0x28
    u32 cnt15;           // 0x2C
};



static u8 r404_texTbl[0x20];
static R404Work* r404_work;

static u32 r404_seFrame = 80;
// The split object's .data is 8-aligned (4 -> 8 bytes).
asm(".section .data\n\t.balign 8\n\t.text");

// The room's MercSysInitRoom parameters are 0x6C bytes (the DOL reads the first 0x5C).
struct R404MercInit {
    MercInit m;
    u32 x5C[4];
};

// Store through a reference: the following pG load stays below it.


void r404_openBox_main(int no, int mode);
static void r404_openedBox(int no);
static void r404_openBox(int no);
u8 r404_initEmSet();
int r404_setEm(u32 no, int force);
void r404_setEm1_4();
void r404_setEm2_5();
void r404_setEm3();
void r404_setEm6();
void r404_setEm13();
void r404_setEm14();
void r404_setEm15();
void r404_setEm7();
void r404_setEm8();
void r404_setEm9();
void r404_setEm10();
void r404_setEm11();
void r404_setEm12();
void r404_setEmChainSaw();
static void r404_checkEmSetA();
static void r404_checkEmSetB();
static void r404_checkEmSetC();
static void r404_checkEmSetD();
static void r404_checkEmSetK();
static void r404_checkEmSetL();
static void r404_checkEmSetM();
void r404_checkEmSetE();
void r404_checkEmSetF();
void r404_checkEmSetG();
void r404_checkEmSetH();
void r404_checkEmSetI();
void r404_checkEmSetJ();
static void r404_checkEmSetChainSaw();
static void r404_execEmSetCheck();
static void setTexRender();
static void slide_move();

// Room init (Mercenaries: the castle courtyard): the enemy count task, the reflecting floor render
// target, the banister slide object (Ada's or Leon's motion) with area 0; the Mercenaries system with
// the room's messages and a random start; three treasure case item events.
void R404Init()
{
#line 45 "D:/Bio4/Prog/r404.cpp"
    r404_work = (R404Work*) MEM_CALLOC(sizeof(R404Work), 1, 0xd);
    r404_work->emId = 0;
    SceExec(0x12, (TaskFunc) r404_execEmSetCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    setTexRender();
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    {
        r404_work->slide = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &pos, &rot, 0x10, 1);
    }
    if (pG->pl_type == 2) {
        r404_work->slide->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 0, 0, 1, 0);
    } else {
        r404_work->slide->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 1, 0);
    }
    r404_work->slide->Motion.Seq_speed = 0.0f;
    r404_work->slide->be_flag |= 0x1000;
    r404_work->slide->setNoSuspend(0);
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) slide_move, 0, 1);
    {
        u8 n = r404_initEmSet();
        R404MercInit init;

        memset(&init, 0, sizeof(init));
        init.m.pos = pPL->pos;
        init.m.rot = pPL->ang;
        init.m.CamNo = 0;
        switch ((u32) n) {
        case 0:
        case 1:
        default:
            init.m.smdMot = ROOM_ARC_PTR(pG->pRoom, 0x25);
            break;
        case 2:
            init.m.smdMot = ROOM_ARC_PTR(pG->pRoom, 0x26);
            break;
        }
        init.m.ClearScore = 30000;
        init.m.mesStart = 1;
        init.m.mesA8 = 0xC;
        init.m.mesAC = 0xD;
        init.m.MesNoStart03 = 0xE;
        init.m.mes[0] = 2;
        init.m.mes[1] = 3;
        init.m.mes[2] = 4;
        init.m.mes[3] = 5;
        init.m.mes[4] = 6;
        init.m.mes[5] = 7;
        init.m.mes[6] = 8;
        init.m.mes[7] = 9;
        init.m.mes[8] = 0xA;
        init.m.mes[9] = 0xB;
        MercSysInitRoom(&init.m);
    }
    SceSetItemEvent(0x19, 0xA5, 0xD, -1, r404_openBox, r404_openedBox, 1, 0);
    SceSetItemEvent(0x1A, 0xA6, 0xE, -1, r404_openBox, r404_openedBox, 2, 0);
    SceSetItemEvent(0x1B, 0x93, 0xF, -1, r404_openBox, r404_openedBox, 3, 0);
}

// Per frame: debug lines only.
void R404Main()
{
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
}

// The three treasure cases (item events 0x19..0x1B, arg 1..3).
void r404_openBox_main(int no, int mode)
{
    switch ((u32) no) {
    case 1:
        OpenBoxMain(OpenBoxPartsUpXM, mode, 0x5B, 0x74, -1, -1);
        break;
    case 2:
        OpenBoxMain(OpenBoxPartsUpZP, mode, 0x5B, 0x75, -1, -1);
        break;
    case 3:
        OpenBoxMain(OpenBoxPartsUpXP, mode, 0x5B, 0x76, -1, -1);
        break;
    }
}

// Item-event "already opened": case `no` posed open.
static void r404_openedBox(int no)
{
    r404_openBox_main(no, 1);
}

// Item-event opener: animate case `no` open.
static void r404_openBox(int no)
{
    r404_openBox_main(no, 0);
}

// The random start (0..2): Leon's position and the first wave; returns the start number.
u8 r404_initEmSet()
{
    u8 n = Rnd() % 3;
    Vec pos[3] = {{-640.0f, 0.0f, -15890.0f}, {2734.0f, 8000.0f, -26577.0f}, {26356.0f, 3000.0f, -32655.0f}};
    Vec rot[3] = {{0.0f, 3.13f, 0.0f}, {0.0f, -2.18f, 0.0f}, {0.0f, -3.06f, 0.0f}};
    // `&rot[n]` as the pos base plus the frame-slot distance, both computed AFTER the two template
    // copies (the target forms `(n*12 + &pos) + 0x28`). ONE variable `t` for both address chains:
    // `t += &pos` re-sets the (mult n 12) holder before the second `n * 12`, so cse1 keeps a second
    // real mulli (gcse PREs only the first occurrence into the block between the template copies),
    // and the re-set chain's output/anti dependences give the target's issue order.
    u32 t = n * 12;
    t += (u32) &pos;
    Vec* b = (Vec*) t;
    t = n * 12;
    t += (u32) &pos;
    t += 0x28;
    Vec* r = (Vec*) t;
    cPlayer* pl = pPL;

    pl->setPos(b);
    pl->setAng(r);
    switch ((u32) n) {
    case 0:
        setEm(0xC8, -1, 1, 1, 1);
        setEm(0xC9, -1, 1, 1, 1);
        setEm(0xCA, -1, 1, 1, 1);
        setEm(0xCB, -1, 1, 1, 1);
        setEm(0xCC, -1, 1, 1, 1);
        setEm(0xCD, -1, 1, 1, 1);
        setEm(0xCE, -1, 1, 1, 1);
        setEm(0xCF, -1, 1, 1, 1);
        setEm(0xD0, -1, 1, 1, 1);
        setEm(0xD1, -1, 1, 1, 1);
        break;
    case 1:
        setEm(0xA0, -1, 1, 1, 1);
        setEm(0xA1, -1, 1, 1, 1);
        setEm(0xA2, -1, 1, 1, 1);
        setEm(0xA3, -1, 1, 1, 1);
        setEm(0xA4, -1, 1, 1, 1);
        setEm(0xA5, -1, 1, 1, 1);
        setEm(0xA6, -1, 1, 1, 1);
        setEm(0xA7, -1, 1, 1, 1);
        setEm(0xA8, -1, 1, 1, 1);
        setEm(0xA9, -1, 1, 1, 1);
        break;
    case 2:
        setEm(0xAA, -1, 1, 1, 1);
        setEm(0xAB, -1, 1, 1, 1);
        setEm(0xAC, -1, 1, 1, 1);
        setEm(0xAD, -1, 1, 1, 1);
        setEm(0xAE, -1, 1, 1, 1);
        setEm(0xB8, -1, 1, 1, 1);
        setEm(0xBB, -1, 1, 1, 1);
        setEm(0xBF, -1, 1, 1, 1);
        setEm(0xC3, -1, 1, 1, 1);
        setEm(0xC6, -1, 1, 1, 1);
        break;
    }
    return n;
}

// Sets (or resets) list entry `no`; force 0: only while fewer than 10 are alive. 1 when it was set.
int r404_setEm(u32 no, int force)
{
    if (force == 0 && r404_work->cnt > 9) {
        return 0;
    }
    cEmWrap em;
    if (!(pG->Em_list[no].be_flag & 2)) {
        em.setEm(no, -1, 1, 1, 1);
    } else {
        em.setPtr(no, -1, 1);
        if (em.ckResetEnable() == 0) {
            return 0;
        }
        em.setReset();
    }
    if (no % 5 != 0) {
        em.setGoto(&pPL->pos, 0xC);
    }
    r404_work->total++;
    r404_work->cnt++;
    return 1;
}

// Wave 1/4 (area 9): list entries 0xF0..0xF2, 0xE6..0xE8 while fewer than 10 are alive; counts the sets.
void r404_setEm1_4()
{
    if (r404_setEm(0xF0, 0) == 1) {
        r404_work->cnt1_4++;
    }
    if (r404_setEm(0xF1, 0) == 1) {
        r404_work->cnt1_4++;
    }
    if (r404_setEm(0xF2, 0) == 1) {
        r404_work->cnt1_4++;
    }
    if (r404_setEm(0xE6, 0) == 1) {
        r404_work->cnt1_4++;
    }
    if (r404_setEm(0xE7, 0) == 1) {
        r404_work->cnt1_4++;
    }
}

// Wave 2/5 (area 0xA): 0xDC..0xDE, 0xEB..0xED.
void r404_setEm2_5()
{
    if (r404_setEm(0xDC, 0) == 1) {
        r404_work->cnt2_5++;
    }
    if (r404_setEm(0xDD, 0) == 1) {
        r404_work->cnt2_5++;
    }
    if (r404_setEm(0xDE, 0) == 1) {
        r404_work->cnt2_5++;
    }
    if (r404_setEm(0xEB, 0) == 1) {
        r404_work->cnt2_5++;
    }
    if (r404_setEm(0xEC, 0) == 1) {
        r404_work->cnt2_5++;
    }
}

// Wave 3 (area 0xC): 0xF6..0xFB.
void r404_setEm3()
{
    if (r404_setEm(0xF6, 0) == 1) {
        r404_work->cnt3++;
    }
    if (r404_setEm(0xF7, 0) == 1) {
        r404_work->cnt3++;
    }
    if (r404_setEm(0xF8, 0) == 1) {
        r404_work->cnt3++;
    }
    if (r404_setEm(0xF9, 0) == 1) {
        r404_work->cnt3++;
    }
    if (r404_setEm(0xFA, 0) == 1) {
        r404_work->cnt3++;
    }
}

// Wave 6: 0xE1..0xE6.
void r404_setEm6()
{
    if (r404_setEm(0xE1, 0) == 1) {
        r404_work->cnt6++;
    }
    if (r404_setEm(0xE2, 0) == 1) {
        r404_work->cnt6++;
    }
    if (r404_setEm(0xE3, 0) == 1) {
        r404_work->cnt6++;
    }
    if (r404_setEm(0xE4, 0) == 1) {
        r404_work->cnt6++;
    }
    if (r404_setEm(0xE5, 0) == 1) {
        r404_work->cnt6++;
    }
}

// Wave 13 (area 0x17): 0xAF..0xB4.
void r404_setEm13()
{
    if (r404_setEm(0xAF, 0) == 1) {
        r404_work->cnt13++;
    }
    if (r404_setEm(0xB0, 0) == 1) {
        r404_work->cnt13++;
    }
    if (r404_setEm(0xB1, 0) == 1) {
        r404_work->cnt13++;
    }
    if (r404_setEm(0xB2, 0) == 1) {
        r404_work->cnt13++;
    }
    if (r404_setEm(0xB3, 0) == 1) {
        r404_work->cnt13++;
    }
}

// Wave 14 (area 0x16): 0xD3..0xD8.
void r404_setEm14()
{
    if (r404_setEm(0xD3, 0) == 1) {
        r404_work->cnt14++;
    }
    if (r404_setEm(0xD4, 0) == 1) {
        r404_work->cnt14++;
    }
    if (r404_setEm(0xD5, 0) == 1) {
        r404_work->cnt14++;
    }
    if (r404_setEm(0xD6, 0) == 1) {
        r404_work->cnt14++;
    }
    if (r404_setEm(0xD7, 0) == 1) {
        r404_work->cnt14++;
    }
}

// Wave 15 (area 0x18): 0xD2, 0xD8..0xDC.
void r404_setEm15()
{
    if (r404_setEm(0xD2, 0) == 1) {
        r404_work->cnt15++;
    }
    if (r404_setEm(0xD8, 0) == 1) {
        r404_work->cnt15++;
    }
    if (r404_setEm(0xD9, 0) == 1) {
        r404_work->cnt15++;
    }
    if (r404_setEm(0xDA, 0) == 1) {
        r404_work->cnt15++;
    }
    if (r404_setEm(0xDB, 0) == 1) {
        r404_work->cnt15++;
    }
}

// The one-shot waves (room save flags 7..12, 6 for the chainsaw sister).
void r404_setEm7()
{
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        RsfSet(G_ROOM_ID, 7);
        r404_setEm(0xB4, 1);
        r404_setEm(0xB5, 1);
        r404_setEm(0xB6, 1);
        r404_setEm(0xB7, 1);
    }
}

// One-shot wave 8 (Room_flg bit 8, area 0x10): 0xBC..0xBE forced.
void r404_setEm8()
{
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        RsfSet(G_ROOM_ID, 8);
        r404_setEm(0xBC, 1);
        r404_setEm(0xBD, 1);
        r404_setEm(0xBE, 1);
    }
}

// One-shot wave 9 (bit 9, area 0x11 or the door area 0x15): 0xB9/0xBA and more forced.
void r404_setEm9()
{
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        RsfSet(G_ROOM_ID, 9);
        cEmWrap a;
        cEmWrap b;
        r404_setEm(0xB9, 1);
        r404_setEm(0xBA, 1);
        a.setPtr(0xB9, -1, 1);
        b.setPtr(0xBA, -1, 1);
        a.setFlag(1);
        b.setFlag(1);
    }
}

// One-shot wave 10 (bit 10, area 0x12): 0xFC..0xFE forced.
void r404_setEm10()
{
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        RsfSet(G_ROOM_ID, 10);
        r404_setEm(0xFC, 1);
        r404_setEm(0xFD, 1);
        r404_setEm(0xFE, 1);
    }
}

// One-shot wave 11 (bit 11, area 0x13): 0xC4/0xC5 forced.
void r404_setEm11()
{
    if (RsfCheck(G_ROOM_ID, 11) == 0) {
        RsfSet(G_ROOM_ID, 11);
        r404_setEm(0xC4, 1);
        r404_setEm(0xC5, 1);
    }
}

// One-shot wave 12 (bit 12, area 0x14): 0xC0..0xC2 forced.
void r404_setEm12()
{
    if (RsfCheck(G_ROOM_ID, 12) == 0) {
        RsfSet(G_ROOM_ID, 12);
        r404_setEm(0xC0, 1);
        r404_setEm(0xC1, 1);
        r404_setEm(0xC2, 1);
    }
}

// The chainsaw sister once (bit 6) after enough kills: 0xF5 or 0xFB by the start side.
void r404_setEmChainSaw()
{
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        RsfSet(G_ROOM_ID, 6);
        if (pPL->pos.x > 10000.0f) {
            r404_setEm(0xF5, 1);
        } else {
            r404_setEm(0xFB, 1);
        }
    }
}

// The area polls: each wave keeps refilling while Leon stands in its area, up to 15 (10) sets.
static void r404_checkEmSetA()
{
    for (;;) {
        if (SceAtHitCheck(8) == 1) {
            r404_setEm6();
            if (r404_work->cnt6 > 0xE) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Area poll: wave 2/5 keeps refilling while Leon stands in area 0xA (up to its set cap).
static void r404_checkEmSetB()
{
    for (;;) {
        if (SceAtHitCheck(9) == 1) {
            r404_setEm1_4();
            if (r404_work->cnt1_4 > 0xE) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Area poll: wave 3 while in area 0xC.
static void r404_checkEmSetC()
{
    for (;;) {
        if (SceAtHitCheck(0xA) == 1) {
            r404_setEm2_5();
            if (r404_work->cnt2_5 > 0xE) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Area poll: wave 13 while in area 0x17.
static void r404_checkEmSetD()
{
    for (;;) {
        if (SceAtHitCheck(0xC) == 1) {
            r404_setEm3();
            if (r404_work->cnt3 > 0xE) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Area poll: wave 14 while in area 0x16.
static void r404_checkEmSetK()
{
    for (;;) {
        if (SceAtHitCheck(0x17) == 1) {
            r404_setEm13();
            if (r404_work->cnt13 > 9) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Area poll: wave 15 while in area 0x18.
static void r404_checkEmSetL()
{
    for (;;) {
        if (SceAtHitCheck(0x16) == 1) {
            r404_setEm14();
            if (r404_work->cnt14 > 0xE) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Area poll: wave 6 (its area).
static void r404_checkEmSetM()
{
    for (;;) {
        if (SceAtHitCheck(0x18) == 1) {
            r404_setEm15();
            if (r404_work->cnt15 > 0xE) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Area poll: one-shot wave 7 when in area 0xF with 10 or fewer alive.
void r404_checkEmSetE()
{
    if (r404_work->cnt <= 0xA && SceAtHitCheck(0xF) == 1) {
        r404_setEm7();
    }
}

// Area poll: one-shot wave 8 when in area 0x10 with 10 or fewer alive.
void r404_checkEmSetF()
{
    if (r404_work->cnt <= 0xA && SceAtHitCheck(0x10) == 1) {
        r404_setEm8();
    }
}

// Area poll: one-shot wave 9 when in area 0x11 (or 0x15 with the door open).
void r404_checkEmSetG()
{
    cEmDoor* door;

    getRoomEtcDoor(0x11, &door, 1);
    if (door != 0 && r404_work->cnt <= 0xF) {
        if (SceAtHitCheck(0x11) == 1 || (SceAtHitCheck(0x15) == 1 && door->ckOpen() == 1)) {
            r404_setEm9();
        }
    }
}

// Area poll: one-shot wave 10 when in area 0x12 with 10 or fewer alive.
void r404_checkEmSetH()
{
    if (r404_work->cnt <= 0xA && SceAtHitCheck(0x12) == 1) {
        r404_setEm10();
    }
}

// Area poll: one-shot wave 11 when in area 0x13 with 15 or fewer alive.
void r404_checkEmSetI()
{
    if (r404_work->cnt <= 0xF && SceAtHitCheck(0x13) == 1) {
        r404_setEm11();
    }
}

// Area poll: one-shot wave 12 when in area 0x14 with 15 or fewer alive.
void r404_checkEmSetJ()
{
    if (r404_work->cnt <= 0xF && SceAtHitCheck(0x14) == 1) {
        r404_setEm12();
    }
}

// COMPILER-DIFF 9: the original enters the rotated loop through `b test` without duplicating the test.
static void r404_checkEmSetChainSaw()
{
    goto test;
sleep:
    SceSleep(1);
test:
    if (r404_work->total - r404_work->cnt <= 0x1D) {
        goto sleep;
    }
    r404_setEmChainSaw();
}

// The enemy count task: starts the area polls, then keeps the alive count and the debug counters.
static void r404_execEmSetCheck()
{
    SceSleep(1);
    r404_work->emId = GetEmIdFromList(0xF5);
    r404_work->cnt = r404_work->total = SceCountEmAlive(r404_work->emId, -1);   // chain: total stored first
    r404_work->cnt1_4 = 0;
    r404_work->cnt2_5 = 0;
    r404_work->cnt3 = 0;
    r404_work->cnt6 = 0;
    r404_work->cnt13 = 0;
    r404_work->cnt14 = 0;
    r404_work->cnt15 = 0;
    SceExec(0x12, (TaskFunc) r404_checkEmSetA, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r404_checkEmSetB, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r404_checkEmSetC, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r404_checkEmSetD, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r404_checkEmSetK, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r404_checkEmSetL, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r404_checkEmSetM, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r404_checkEmSetChainSaw, 0, 0, SCE_PRIO_DEF_2, 0);
    for (;;) {
        r404_work->cnt = SceCountEmAlive(r404_work->emId, -1);
        eprintf(0x1E, 0x8C, 0, 0, "em_num:%d", r404_work->cnt);
        eprintf(0x1E, 0x9C, 0, 0, "total :%d", r404_work->total);
        r404_checkEmSetE();
        r404_checkEmSetF();
        r404_checkEmSetG();
        r404_checkEmSetH();
        r404_checkEmSetI();
        r404_checkEmSetJ();
        SceSleep(1);
    }
}

// The render target of the reflecting floor (object 0xC7).
static void setTexRender()
{
    cObj* obj;
    u8* tbl = r404_texTbl;

    if (GetTexRenderMgr(&r404_work->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r404_work->tex->m_Tex_no;
        r404_work->tex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, r404_work->tex->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0xC7);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(1);
    obj->pModelInfo->color[3] = 0xF0;
}

// Area 0: Leon slides down the banister (the slide object plays its motion along).
static void slide_move()
{
    cPlayer* pl = pPL;
    Vec v;
    u32 max;
    u32 i;

    pl->beginAction();
    pPL->atari.offSca();
    pPL->atari.offOba();
    pPL->atari.setPriority(PRI_LV1);
    pPL->dmg.set(0, 0x80);
    pl->be_flag &= ~0x10;
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    PlSetHand(1, 0);
    if (pG->pl_type == 2) {
        cModel* m = pPL;

        v.x = 8271.77f;
        v.y = 7622.3f;
        v.z = -56912.93f;
        m->setPos(&v);
        v.y = -1.5707964f;
        v.x = 0.0f;
        v.z = 0.0f;
        pPL->setAng(&v);
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0x201, 0);
        r404_work->slide->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 0, 0, 1, 0);
    } else {
        cModel* m = pPL;

        v.x = 8349.89f;
        v.y = 7626.05f;
        v.z = -56921.73f;
        m->setPos(&v);
        v.y = -1.3962634f;
        v.x = 0.0f;
        v.z = 0.0f;
        pPL->setAng(&v);
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 0x201, 0);
        r404_work->slide->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 1, 0);
    }
    r404_work->slide->Motion.Seq_speed = 1.0f;
    SndCall(6, 0, 0, 0, 0, 0);
    max = (u32) MotionGetMaxFrame(&pPL->Motion);
    for (i = 0; i < max; i++) {
        SceSleep(1);
        if (i == r404_seFrame) {
            SndCall(6, 1, 0, 0, 0, 0);
        }
    }
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->Wep->setTrans(1, 0);
    pl->endAction(5);
    pPL->dmg.clear();
    pPL->atari.onSca();
    pPL->atari.onOba();
    pPL->atari.setPriority(0);
    pl->be_flag |= 0x10;
}
