// the split object's .rodata is 8-aligned (no double constant in this unit forces it); emitted
// before the first header string, while the assembler output has no current section yet.
asm(".section .rodata\n\t.balign 8\n\t.text");
#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj00.h"
#include "em.h"
#include "em_wrap.h"
#include "item.h"
#include "item_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "fade.h"
#include "joy.h"
#include "rnd.h"
#include "snd.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "db_log.h"

// Room 2-07 (D:/Bio4/Prog/r207.cpp): the two swords on the wall, the enemies patrolling between
// the four areas of the hall and the wall that slides open once both swords are swapped.

// One enemy: its patrol state and the enemy itself.
struct R207Em {
    int moving;    // 0x00  a r207_GotoPos task drives it
    int gotoNo;    // 0x04  route number (r207_GotoPos)
    cEmWrap em;    // 0x08
};

// One at area: who stands in it this frame (bit 0 the player, bit 4+k enemy k) and the edges.
struct R207At {
    u16 cur;    // 0x00
    u16 prev;   // 0x02
    u16 on;     // 0x04  bits that appeared this frame
    u16 off;    // 0x06  bits that vanished this frame
};

struct R207Work {
    R207Em em[10];     // 0x00
    R207At area[4];    // 0xC8
    int timer;         // 0xE8  frames until the next patrol order
    cObj* item0;       // 0xEC  sword item model (slot 0x80)
    cObj* item1;       // 0xF0  sword item model (slot 0xC4)
    Vec wallOfs;       // 0xF4  wall sword position relative to the wall object
    cSubChar* sub;     // 0x100 partner hidden during the enemy event
};

// The work pointer is a struct member: every store through the work reloads it.
struct R207WorkPtr {
    R207Work* p;
};

static R207WorkPtr r207_work;

static Vec r207_gotoTbl[5] = {
    {-7120.0f, 4149.0f, -1240.0f},
    {-8910.0f, 4149.0f, -11320.0f},
    {342.0f, 4149.0f, -935.0f},
    {1110.0f, 4149.0f, -11430.0f},
    {-5230.0f, 4149.0f, -12065.0f},
};
static Vec r207_swordPos = {890.0f, 2050.0f, -8194.0f};
static Vec r207_wallPos = {-10600.0f, 5200.0f, -10950.0f};
static Vec r207_swordRot = {0.0f, 0.0f, 0.0f};
static Vec r207_wallRot = {0.0f, -1.5707964f, 0.0f};

// `pSUB->atari.flags &= 0xFDFF` through a pointer to the collision info: `addi r9,pSUB,0x2B4` is
// kept (two uses); the volatile halfword store makes the following `work->sub = pSUB` reload pSUB
// (EnemySet: `lwz r0,pSUB` after the `sth`, and pSUB@ha stays in a callee-saved register).
static inline void AtariFlagsAnd(cAtariInfo* a, u16 mask) { *(volatile u16*) &a->m_flag &= mask; }
static inline void AtariFlagsOr(cAtariInfo* a, u16 bit) { a->m_flag |= bit; }
// pPL read as a struct member: the load stays below the preceding store into the work (R207Main).
struct PlPtr { cPlayer* p; };
#define pPLS (((PlPtr*) &pPL)->p)
// pSUB written as a struct member: the following flags load stays below the store (EnemySetEndProc).
struct SubPtr { cSubChar* p; };
#define pSUBS (((SubPtr*) &pSUB)->p)

static void r207_openTerm();
void r207_EmMoveCk();
int r207_CkDist();
int r207_CountEmAlive();
static void r207_GotoPos(R207Em* e);
void r207_ToPos(R207Em* e, int no);
static void r207_EnemySet();
static void r207_EnemySetEndProc();
static void r207_GetSword(int no);
static void r207_CheckSwordYard(int no);
static void r207_CheckUseSword();
void r207_SetSword(int which, int mode);
static void r207_WallMove();
static void r207_WallMoveEndProc();
void r207_ItemModelSet(cModel* m, int mode);
static void r207_ShelfOpen(int id);
static void r207_ShelfOpened(int id);
static void r207_StrCheck();

// Room init (the two-swords hall): first visit presets Room_flg bits 0/5/6 (the swords start on the
// right-hand mounts); the wall sword offset. Wall open (bit 1): the wall object 0x18 posed open, item
// areas off; else the sword mounts: the use-from-inventory watcher, areas 0xF/2 = the empty mount
// prompts, 0xD/0xE = take a sword, each sword item model (slots 0x80 / 0x87) placed on the mount its
// flags (4/5, 6/7) say. Then the enemy event, patrol / stream tasks, two shelf item events, the terminal.
void R207Init()
{
    // One pointer local for the sword item models: NULL for the wall-open state, the at item
    // models otherwise (its redefinitions keep the zero in a callee-saved register).
    cModel* m = NULL;
    cObj* obj;
    void* bin;
    void* tpl;
    R207Work** wp;

    obj = SmdGetObjPtr(0x18);
    // The work address is taken before the calloc call (`lis` above the `bl`, as the plain-pointer
    // rooms do): the store goes through a pointer to the member.
    wp = &r207_work.p;
#line 67 "D:/Bio4/Prog/r207.cpp"
    *wp = (R207Work*) MEM_CALLOC(sizeof(R207Work), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        RsfSet(G_ROOM_ID, 5);
        RsfSet(G_ROOM_ID, 6);
    }
    PSVECSubtract(&r207_wallPos, &obj->pos, &r207_work.p->wallOfs);
    if (RsfCheck(G_ROOM_ID, 1)) {
        SmdGetObjPtr(0x18)->pos.z = -9500.0f;
        SmdGetObjPtr(0x18)->matUpdate();
        SceAtSetEnable(0x80, 0);
        SceAtSetEnable(0x87, 0);
        r207_work.p->item0 = (cObj*) m;
        if (ItemGetBinTplAddr(0x80, &bin, &tpl) == 1) {
            r207_work.p->item0 = SetObj00(bin, tpl, &r207_swordPos, &r207_swordRot);
            SceSleep(1);
        }
    } else {
        SceExec(0x12, (TaskFunc) r207_CheckUseSword, 0, 0, SCE_PRIO_DEF_2, 0);
        SceAtDataSet_exec(0xF, SCE_LEVEL10, 0, (TaskFunc) r207_CheckSwordYard, (void*) 2, 1);
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r207_CheckSwordYard, (void*) 3, 1);
        SceAtSetEnable(0xF, 0);
        SceAtSetEnable(2, 0);
        SceAtDataSet_exec(0xD, SCE_LEVEL10, 0, (TaskFunc) r207_GetSword, (void*) 2, 1);
        SceAtDataSet_exec(0xE, SCE_LEVEL10, 0, (TaskFunc) r207_GetSword, (void*) 3, 1);
        SceAtSetEnable(0xD, 0);
        SceAtSetEnable(0xE, 0);
        if (ItemMgr.num(0x80) == 0) {
            ItfFlagOff(pG, ITF_R207_GOLDEN_SWORD);
            SceAtSetEnable(0x80, 1);
            m = SceAtItemModelPtr(0x80);
            if (RsfCheck(G_ROOM_ID, 4)) {
                r207_ItemModelSet(m, 2);
            } else if (RsfCheck(G_ROOM_ID, 5)) {
                r207_ItemModelSet(m, 3);
            }
        }
        if (ItemMgr.num(0xC4) == 0) {
            ItfFlagOff(pG, ITF_R207_SILVER_SWORD);
            SceAtSetEnable(0x87, 1);
            m = SceAtItemModelPtr(0x87);
            if (RsfCheck(G_ROOM_ID, 6)) {
                r207_ItemModelSet(m, 2);
            } else if (RsfCheck(G_ROOM_ID, 7)) {
                r207_ItemModelSet(m, 3);
            }
        }
        if (RsfCheck(G_ROOM_ID, 4) == 0 && RsfCheck(G_ROOM_ID, 6) == 0) {
            SceAtSetEnable(0xF, 1);
            SceAtSetEnable(0xA, 1);
            SceAtSetEnable(9, 1);
        } else {
            SceAtSetEnable(0xF, 0);
            SceAtSetEnable(0xA, 0);
            SceAtSetEnable(9, 0);
        }
        if (RsfCheck(G_ROOM_ID, 5) == 0 && RsfCheck(G_ROOM_ID, 7) == 0) {
            SceAtSetEnable(2, 1);
            SceAtSetEnable(0xC, 1);
            SceAtSetEnable(0xB, 1);
        } else {
            SceAtSetEnable(2, 0);
            SceAtSetEnable(0xC, 0);
            SceAtSetEnable(0xB, 0);
        }
        SceAtSetEnable(1, 0);
        if (RsfCheck(G_ROOM_ID, 11) == 0) {
            SceExec(0x12, (TaskFunc) r207_EnemySet, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    r207_work.p->em[7].em.setEm(0xD8, -1, 0, 1, 1);
    r207_work.p->em[8].em.setEm(0xD9, -1, 0, 1, 1);
    r207_work.p->em[9].em.setEm(0xDA, -1, 0, 1, 1);
    SceSetItemEvent(3, 0x83, 2, 5, r207_ShelfOpen, (void (*)()) r207_ShelfOpened, 0, 0);
    SceSetItemEvent(4, 0x84, 3, 6, r207_ShelfOpen, (void (*)()) r207_ShelfOpened, 1, 0);
    SceExec(0x12, (TaskFunc) r207_StrCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        SceExec(0x12, (TaskFunc) r207_openTerm, 0, 0, SCE_PRIO_DEF_2, 0);
    }
}

// Per frame: for the four hall areas (at 5/6/8/7) build the occupancy bitmask (bit 0 the player, bit 4+k
// enemy k) and its on/off edges, then run the patrol orders (r207_EmMoveCk).
void R207Main()
{
    int at[4] = {5, 6, 8, 7};
    u32 i;
    u32 k;

    for (i = 0; i < 4; i++) {
        r207_work.p->area[i].cur = 0;
        if (SceAtCheckHitModel(at[i], pPLS)) {
            r207_work.p->area[i].cur |= 1;
        }
        for (k = 0; k < 10; k++) {
            if (k == 2) {
                continue;
            }
            if (r207_work.p->em[k].em.isActive() && SceAtCheckHitModel(at[i], r207_work.p->em[k].em.getPtr())) {
                r207_work.p->area[i].cur |= 1 << (k + 4);
            }
        }
        r207_work.p->area[i].on = (r207_work.p->area[i].prev ^ r207_work.p->area[i].cur) & r207_work.p->area[i].cur;
        r207_work.p->area[i].off = r207_work.p->area[i].prev & (r207_work.p->area[i].prev ^ r207_work.p->area[i].cur);
        r207_work.p->area[i].prev = r207_work.p->area[i].cur;
    }
    r207_EmMoveCk();
    if (Joy[2].trg & 0x10) {
        SceAtSetEnable(0x80, 1);
    }
}

// Once (Room_flg bit 10): typewriter terminal 0xD and autosave.
static void r207_openTerm()
{
    RsfSet(G_ROOM_ID, 10);
    OpeSetOpenTerm(0xD, 0.0f, 0.0f, 0.0f, 0.0f);
    GameSaveSave(&GameSave, pSaveData, -1);
}

// Every 150 frames: the enemy farthest from the player in the area the player just entered is
// sent along a route to another area.
void r207_EmMoveCk()
{
    R207Em* e;
    int k;

    if (r207_work.p->timer != 0) {
        r207_work.p->timer--;
        return;
    }
    if ((u32) r207_CountEmAlive() <= 1) {
        return;
    }
    if (r207_work.p->area[0].on & 1) {
        k = r207_CkDist();
        if (k == -1) {
            return;
        }
        e = &r207_work.p->em[k];
        if ((r207_work.p->area[1].cur >> (k + 4)) & 1) {
            e->gotoNo = 1;
        } else if ((r207_work.p->area[3].cur >> (k + 4)) & 1) {
            e->gotoNo = 0;
        } else if ((r207_work.p->area[2].cur >> (k + 4)) & 1) {
            e->gotoNo = (Rnd() & 1) ? 9 : 12;
        } else {
            e->gotoNo = 14;
        }
    } else if (r207_work.p->area[1].on & 1) {
        k = r207_CkDist();
        if (k == -1) {
            return;
        }
        e = &r207_work.p->em[k];
        if ((r207_work.p->area[0].cur >> (k + 4)) & 1) {
            e->gotoNo = 2;
        } else if ((r207_work.p->area[2].cur >> (k + 4)) & 1) {
            e->gotoNo = 3;
        } else if ((r207_work.p->area[3].cur >> (k + 4)) & 1) {
            e->gotoNo = (Rnd() & 1) ? 11 : 10;
        } else {
            e->gotoNo = 14;
        }
    } else if (r207_work.p->area[2].on & 1) {
        k = r207_CkDist();
        if (k == -1) {
            return;
        }
        e = &r207_work.p->em[k];
        if ((r207_work.p->area[1].cur >> (k + 4)) & 1) {
            e->gotoNo = 5;
        } else if ((r207_work.p->area[3].cur >> (k + 4)) & 1) {
            e->gotoNo = 4;
        } else if ((r207_work.p->area[0].cur >> (k + 4)) & 1) {
            e->gotoNo = (Rnd() & 1) ? 12 : 9;
        } else {
            e->gotoNo = 14;
        }
    } else if (r207_work.p->area[3].on & 1) {
        k = r207_CkDist();
        if (k == -1) {
            return;
        }
        e = &r207_work.p->em[k];
        if ((r207_work.p->area[0].cur >> (k + 4)) & 1) {
            e->gotoNo = 6;
        } else if ((r207_work.p->area[2].cur >> (k + 4)) & 1) {
            e->gotoNo = 7;
        } else if ((r207_work.p->area[1].cur >> (k + 4)) & 1) {
            e->gotoNo = (Rnd() & 1) ? 11 : 8;
        } else {
            e->gotoNo = 14;
        }
    } else {
        return;
    }
    SceExec(0x12, (TaskFunc) r207_GotoPos, (int) e, 0, SCE_PRIO_DEF_2, 0);
    r207_work.p->timer = 150;
}

// The idle enemy farthest from the player (-1: none).
int r207_CkDist()
{
    f32 dist = 0.0f;
    int best = -1;
    u32 k;

    for (k = 0; k < 10; k++) {
        if (k == 2) {
            continue;
        }
        if (r207_work.p->em[k].em.isActive() && r207_work.p->em[k].moving == 0) {
            Vec pos;
            f32 d;

            r207_work.p->em[k].em.getPos(&pos);
            d = PSVECSquareDistance(&pos, &pPL->pos);
            if (dist < d) {
                dist = d;
                best = k;
            }
        }
    }
    return best;
}

// Number of the hall enemies (slots 0..9 except 2) still active.
int r207_CountEmAlive()
{
    int cnt = 0;
    u32 k;

    for (k = 0; k < 10; k++) {
        if (k == 2) {
            continue;
        }
        if (r207_work.p->em[k].em.isActive() == 1) {
            cnt++;
        }
    }
    return cnt;
}

// Walks enemy `e` along its route: a sequence of the five waypoints, 5 = the player.
static void r207_GotoPos(R207Em* e)
{
    e->moving = 1;
    switch (e->gotoNo) {
    case 0:
        r207_ToPos(e, 2);
        r207_ToPos(e, 0);
        break;
    case 1:
        r207_ToPos(e, 2);
        r207_ToPos(e, 3);
        break;
    case 2:
        r207_ToPos(e, 3);
        r207_ToPos(e, 2);
        break;
    case 3:
        r207_ToPos(e, 3);
        r207_ToPos(e, 1);
        break;
    case 4:
        r207_ToPos(e, 1);
        r207_ToPos(e, 0);
        break;
    case 5:
        r207_ToPos(e, 1);
        r207_ToPos(e, 3);
        break;
    case 6:
        r207_ToPos(e, 0);
        r207_ToPos(e, 2);
        break;
    case 7:
        r207_ToPos(e, 0);
        r207_ToPos(e, 1);
        r207_ToPos(e, 4);
        break;
    case 8:
        r207_ToPos(e, 1);
        r207_ToPos(e, 4);
        break;
    case 9:
        r207_ToPos(e, 0);
        break;
    case 10:
        r207_ToPos(e, 1);
        break;
    case 11:
        r207_ToPos(e, 2);
        break;
    case 12:
        r207_ToPos(e, 3);
        break;
    case 13:
        r207_ToPos(e, 4);
        break;
    case 14:
        r207_ToPos(e, 5);
        break;
    }
    e->moving = 0;
}

// Send enemy `e` to waypoint `no` (5 = the player) with goto mode 0xD and wait until it arrives.
void r207_ToPos(R207Em* e, int no)
{
    if (no == 5) {
        e->em.setGoto(&pPL->pos, 0xD);
    } else {
        e->em.setGoto(&r207_gotoTbl[no], 0xD);
    }
    while (e->em.ckGoto() == 0xD) {
        SceSleep(1);
    }
}

// The enemy event: once the player has stood on the upper floor for 5 seconds, three enemies
// break in through the door and chase him.
static void r207_EnemySet()
{
    Vec sePos = {800.0f, 1200.0f, 700.0f};
    Vec gotoPos = {-1834.0f, 4149.0f, -11795.0f};
    u32 cnt = 0;
    // COMPILER-DIFF: #13 -- the `pSUB = NULL` zero is a reload-rematerialised constant in the original
    // (`li r9,0` after the `sth`, reusing the atari pointer's register); a single-use function-scope
    // variable gets its `li` moved next to the store by update_equiv_regs and the same register.
    cSubChar* zero = NULL;

    do {
        if (pPL->pos.y <= 4100.0f) {
            cnt = 0;
        } else {
            cnt++;
        }
        SceSleep(1);
    } while (cnt <= 300);
    while (PlGetStatus() == 0x80 || PlGetStatus() == 0x1000) {
        SceSleep(1);
    }
    RoomSeCall(1, &sePos, 0, 0, 0);
    SceSleep(50);
    RoomSeCall(2, &sePos, 0, 0, 0);
    RsfSet(G_ROOM_ID, 11);
    SceEventStart(1);
    if (pSUB) {
        AtariFlagsAnd(&pSUB->atari, 0xFDFF);
        r207_work.p->sub = pSUB;
        pSUB = zero;
    }
    r207_work.p->em[2].em.setEm(0xD3, -1, 0, 1, 1);
    r207_work.p->em[3].em.setEm(0xD4, -1, 0, 1, 1);
    r207_work.p->em[4].em.setEm(0xD5, -1, 0, 1, 1);
    r207_work.p->em[2].em.setNoSuspend(1);
    r207_work.p->em[3].em.setNoSuspend(1);
    r207_work.p->em[4].em.setNoSuspend(1);
    SceSleep(1);
    CamCtrl.CutCall(7);
    SceSetEventCancel(1, (TaskFunc) r207_EnemySetEndProc, 0, 1, 1);
    r207_work.p->em[2].em.setGoto(&gotoPos, 8);
    SceSleep(30);
    r207_work.p->em[3].em.setGoto(&gotoPos, 0xD);
    r207_work.p->em[4].em.setGoto(&gotoPos, 0xD);
    while (r207_work.p->em[2].em.ckGoto() == 8) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r207_EnemySetEndProc();
}

// After the event: two more waves come in while the leader lives and few enemies remain.
static void r207_EnemySetEndProc()
{
    Vec sePos = {800.0f, 1200.0f, 700.0f};
    Vec gotoPos = {-1834.0f, 4149.0f, -11795.0f};
    int loop = 1;
    int wave = 0;

    if (pGS->Room_flg[0] & 0x40000000) {
        r207_work.p->em[3].em.setGoto(&gotoPos, 0xD);
        r207_work.p->em[4].em.setGoto(&gotoPos, 0xD);
    }
    CamCtrl.Comeback(0);
    r207_work.p->em[2].em.setNoSuspend(0);
    r207_work.p->em[3].em.setNoSuspend(0);
    r207_work.p->em[4].em.setNoSuspend(0);
    if (r207_work.p->sub) {
        pSUBS = r207_work.p->sub;
        AtariFlagsOr(&pSUB->atari, 0x200);
    }
    SceEventEnd(0);
    do {
        if (r207_work.p->em[2].em.isActive() == 0) {
            break;
        }
        if ((u32) r207_CountEmAlive() <= 2 && pPL->pos.y >= 4100.0f) {
            u8 r = Rnd() % 3;

            SceSleep((r + 5) * 30);
            RoomSeCall(1, &sePos, 0, 0, 0);
            SceSleep(50);
            RoomSeCall(2, &sePos, 0, 0, 0);
            switch (wave) {
            case 0:
                wave = 1;
                r207_work.p->em[5].em.setEm(0xD6, -1, 0, 1, 1);
                r207_work.p->em[6].em.setEm(0xD7, -1, 0, 1, 1);
                r207_work.p->em[5].em.setGoto(&gotoPos, 0xD);
                r207_work.p->em[6].em.setGoto(&gotoPos, 0xD);
                // A code-less insn after the last call of each arm (a tied, non-volatile launder of
                // a live variable; the two arms must launder DIFFERENT variables): (1) the block's
                // tail is then not the call, so the second setEm's `li r6..r8` keep setGoto's
                // anti-dependence as a distinct dependent and are issued before the re-set
                // `li r4/r5` (the original's order); (2) jump2 cannot cross-jump arm 0's single
                // `bl setGoto` into arm 1's (the original never merges a one-insn tail).
                // COMPILER-DIFF: #6
                asm("" : "=r"(loop) : "0"(loop));
                break;
            case 1:
                wave = 2;
                r207_work.p->em[0].em.setEm(0xD0, -1, 0, 1, 1);
                loop = 0;
                r207_work.p->em[1].em.setEm(0xD1, -1, 0, 1, 1);
                r207_work.p->em[0].em.setGoto(&gotoPos, 0xD);
                r207_work.p->em[1].em.setGoto(&gotoPos, 0xD);
                asm("" : "=r"(wave) : "0"(wave)); // COMPILER-DIFF: #6 (see case 0)
                break;
            }
        }
        SceSleep(1);
    } while (loop != 0);
}

// Taking a sword off the wall (`no` 2 = left, 3 = right): the prompt, then the at item pickup.
static void r207_GetSword(int no)
{
    int at = 0;
    int mes = 0;

    switch (no) {
    case 2:
        if (RsfCheck(G_ROOM_ID, 4)) {
            at = 0x80;
            mes = 3;
        } else if (RsfCheck(G_ROOM_ID, 6)) {
            at = 0x87;
            mes = 4;
        }
        break;
    case 3:
        if (RsfCheck(G_ROOM_ID, 5)) {
            at = 0x80;
            mes = 5;
        } else if (RsfCheck(G_ROOM_ID, 7)) {
            at = 0x87;
            mes = 6;
        }
        break;
    }
    SceMesSet(mes, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    if (SceMesGetSelection() == 2) {
        SceExit();
    }
    SceAtExecute(at);
    while (SceAtItemFlgCk(at) == 0) {
        SceSleep(1);
    }
    switch (no) {
    case 2:
        if (RsfCheck(G_ROOM_ID, 4)) {
            RsfClear(G_ROOM_ID, 4);
        } else if (RsfCheck(G_ROOM_ID, 6)) {
            RsfClear(G_ROOM_ID, 6);
        }
        SceAtSetEnable(0xD, 0);
        SceAtSetEnable(0xF, 1);
        SceAtSetEnable(9, 1);
        SceAtSetEnable(0xA, 1);
        break;
    case 3:
        if (RsfCheck(G_ROOM_ID, 5)) {
            RsfClear(G_ROOM_ID, 5);
        } else if (RsfCheck(G_ROOM_ID, 7)) {
            RsfClear(G_ROOM_ID, 7);
        }
        SceAtSetEnable(0xE, 0);
        SceAtSetEnable(2, 1);
        SceAtSetEnable(0xB, 1);
        SceAtSetEnable(0xC, 1);
        break;
    }
}

// Standing in front of an empty sword mount: the camera cut and the inventory to pick a sword.
static void r207_CheckSwordYard(int no)
{
    int cut = 7;

    switch (no) {
    case 2:
        cut = 7;
        break;
    case 3:
        cut = 8;
        break;
    }
    if (pSys->language == 0) {
        SceUpCut(cut, -1, -1, UP_CUT_ATTR_CUT_FIX);
    }
    if (ItemMgr.num(0x80) != 0 || ItemMgr.num(0xC4) != 0) {
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
    } else {
        CamCtrl.Comeback(0);
    }
}

// Watches for a sword used from the inventory while standing at a mount.
static void r207_CheckUseSword()
{
    int mode = 2;
    int which = 0;
    int use;

    do {
        use = 0;
        if (ItemMgr.check(0x80)) {
            use = 1;
            which = 0;
            if (SceAtHitCheck(9)) {
                mode = 2;
            } else {
                mode = 3;
            }
        }
        if (ItemMgr.check(0xC4)) {
            use = 1;
            which = 1;
            if (SceAtHitCheck(0xA)) {
                mode = 2;
            } else {
                mode = 3;
            }
        }
        if (use) {
            r207_SetSword(which, mode);
        }
        SceSleep(1);
    } while (RsfCheck(G_ROOM_ID, 1) == 0);
}

// Puts sword `which` (0 = 0x80, 1 = 0xC4) on mount `mode` (2 = left, 3 = right).
void r207_SetSword(int which, int mode)
{
    cObj* obj;
    int at = 0;
    int mes = 0;

    obj = SmdGetObjPtr(0x18);
    switch (mode) {
    case 2:
        SceAtSetEnable(0xF, 0);
        if (which == 0) {
            at = 0x80;
            mes = 1;
            RsfSet(G_ROOM_ID, 4);
            ItfFlagOff(pG, ITF_R207_GOLDEN_SWORD);
        } else {
            at = 0x87;
            mes = 2;
            RsfSet(G_ROOM_ID, 6);
            ItfFlagOff(pG, ITF_R207_SILVER_SWORD);
        }
        SceAtSetEnable(9, 0);
        SceAtSetEnable(0xA, 0);
        break;
    case 3:
        SceAtSetEnable(2, 0);
        if (which == 0) {
            at = 0x80;
            mes = 1;
            RsfSet(G_ROOM_ID, 5);
            ItfFlagOff(pG, ITF_R207_GOLDEN_SWORD);
        } else {
            at = 0x87;
            mes = 2;
            RsfSet(G_ROOM_ID, 7);
            ItfFlagOff(pG, ITF_R207_SILVER_SWORD);
        }
        SceAtSetEnable(0xB, 0);
        SceAtSetEnable(0xC, 0);
        break;
    }
    while (Fade[0].flags & 1) {
        SceSleep(1);
    }
    SceAtSetEnable(at, 1);
    r207_ItemModelSet(SceAtItemModelPtr(at), mode);
    RoomSeCall(3, &obj->pos, 0, 0, 0);
    if (pSys->language == 0) {
        SceMesSet(mes, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    }
    if (RsfCheck(G_ROOM_ID, 4) && RsfCheck(G_ROOM_ID, 7)) {
        SceExec(0x12, (TaskFunc) r207_WallMove, 0, 0, SCE_PRIO_DEF_2, 0);
    }
}

// Both swords swapped: the wall slides open.
static void r207_WallMove()
{
    void* zero = 0;
    cObj* obj;
    void* bin;
    void* tpl;

    obj = SmdGetObjPtr(0x18);
    SceEventStart(1);
    SceAtSetEnable(0x80, 0);
    SceAtSetEnable(0x87, 0);
    SceAtSetEnable(0xD, 0);
    SceAtSetEnable(0xE, 0);
    r207_work.p->item1 = (cObj*) zero;
    if (ItemGetBinTplAddr(0xC4, &bin, &tpl) == 1) {
        r207_work.p->item1 = SetObj00(bin, tpl, &r207_work.p->wallOfs, &r207_wallRot);
        OyaSetObj00(r207_work.p->item1, obj, 0);
        r207_work.p->item1->setNoSuspend(1);
        SceSleep(1);
    }
    r207_work.p->item0 = (cObj*) zero;
    if (ItemGetBinTplAddr(0x80, &bin, &tpl) == 1) {
        r207_work.p->item0 = SetObj00(bin, tpl, &r207_swordPos, &r207_swordRot);
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 1);
    CamCtrl.CutCall(3);
    SceSetEventCancel(1, (TaskFunc) r207_WallMoveEndProc, 0, -1, 1);
    RoomSeCall(0, &obj->pos, 0, 0, obj);
    EstSet(0, -1, 0, 0, 1, 0, 1, 0, (u32) zero, zero);
    while (obj->pos.z < -9500.0f) {
        obj->pos.z += 30.0f;
        obj->matUpdate();
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r207_WallMoveEndProc();
}

// End of the wall slide (also its cancel path): the wall 0x18 snapped open, collision area 1 on,
// Scenario_flg[4] 0x20000000 (the passage is open), camera back, SceEventEnd.
static void r207_WallMoveEndProc()
{
    SmdGetObjPtr(0x18)->pos.z = -9500.0f;
    SmdGetObjPtr(0x18)->matUpdate();
    SceAtSetEnable(1, 1);
    ScfFlagOn(pG, SCF_82);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Places the sword item model on mount `mode` (2 = left, 3 = right) and enables its at.
void r207_ItemModelSet(cModel* m, int mode)
{
    int at = 0;

    m->ot_type = 0;
    switch (mode) {
    case 2:
        m->pos = r207_swordPos;
        m->ang = r207_swordRot;
        at = 0xD;
        break;
    case 3:
        m->pos = r207_wallPos;
        m->ang = r207_wallRot;
        at = 0xE;
        break;
    }
    m->matUpdate();
    SceAtSetEnable(at, 1);
}

// Item-event opener: shelf `id` (objects 7/8 or 4/5) swings open.
static void r207_ShelfOpen(int id)
{
    switch (id) {
    case 0:
        OpenBoxMain(0, 0, 0x1A, 7, 8, -1);
        break;
    case 1:
        OpenBoxMain(0, 0, 0x1A, 4, 5, -1);
        break;
    }
}

// Item-event "already opened": shelf `id` posed open.
static void r207_ShelfOpened(int id)
{
    switch (id) {
    case 0:
        OpenBoxMain(0, 1, 0x1A, 7, 8, -1);
        break;
    case 1:
        OpenBoxMain(0, 1, 0x1A, 4, 5, -1);
        break;
    }
}

// Room stream: from the first enemy that finds the player until none of them is left.
static void r207_StrCheck()
{
    for (;;) {
        while (SceCkFindPL(NULL) != 1) {
            SceSleep(1);
        }
        SndRoomStrStart(1, 0, 1);
        while (SceCountEmAlive(0x11, -1) != 0) {
            SceSleep(1);
        }
        SndRoomStrStop(2);
    }
}

// The next unit's .data is 8-aligned: the split object carries the 4 bytes of padding.
asm(".section .data; .balign 8");
