#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "event.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "em.h"
#include "em_wrap.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "item.h"
#include "sscrn.h"
#include "datactrl.h"
#include "read.h"
#include "dvd.h"
#include "snd.h"

// Room 2-03 (D:/Bio4/Prog/r203.cpp): the key item on the shelf, the Ganado waves and the s00
// event (meeting Ashley again).

struct R203Work {
    cEmWrap em[3];      // 0x00  (the tables index up to em[8]: the original work was smaller than its use)
    s8 pt[3];           // 0x24  wandering way point per task (overlaps em[3] in the original)
    u8 pad_27[0xA8 - 0x27];
    cDataUnit* data;    // 0xA8  the s00 event data
};

// The work pointer is a struct member: every store through the work reloads it.
struct R203WorkPtr {
    R203Work* p;
};

static R203WorkPtr r203_work;

// List numbers from the int tables are passed on without truncation (COMPILER-DIFF 4).
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");

static Vec r203_wanderPos[3] = {
    {-54690.0f, 3172.0f, 6440.0f},
    {-38430.0f, 4155.0f, -9730.0f},
    {-49710.0f, 4155.0f, -18410.0f},
};

static void r203_LockDoor();
static void r209_CheckUseKey();
static void r203_GanadoEscape();
static void r203_GetKeyItem();
static void r203_GanadoWandering(int no);
static void r203_EventMeetAgain();
static void r203_TreasureBoxOpen(int id);
static void r203_TreasureBoxOpened(int id);
static void r203_ShelfOpen();
static void r203_ShelfOpened();
static void r203_StreamCheck();
extern "C" void Evt_R203S00_Func(Event* e);

// Room init: once the key item was taken (Scenario_flg[0] 0x00010000) two Ganados (0x27/0x29) plus three
// wanderers (0x34..0x36, list 2); otherwise the nine Ganados of the table (list 2), area 0x8A = the key
// pickup wave and the key-carrier's escape. Area 1 = the locked door with the key-use watcher until
// Key_flg[0] 0x00020000. Until Room_flg bit 3: Ashley initialised as follower, r203s00 pre-loaded,
// area 3 = the reunion event. Battle stream, one chest and one shelf item event.
void R203Init()
{
    int tbl[9][2] = {
        {0, 0x44}, {1, 0x45}, {2, 0x46}, {3, 0x47}, {4, 0x48},
        {5, 0x37}, {6, 0x39}, {7, 0x3A}, {8, 0x3B},
    };

#line 69 "D:/Bio4/Prog/r203.cpp"
    r203_work.p = (R203Work*) MEM_CALLOC(sizeof(R203Work), 1, 0xd);
    if (ScfFlagChk(pG, SCF_R201_EVENT00)) {
        setEm(0x27, -1, 0, 1, 0);
        setEm(0x29, -1, 0, 1, 0);
        if (r203_work.p->em[0].setEm(0x34, 2, 0, 1, 0) == 1) {
            SceExec(0x12, (TaskFunc) r203_GanadoWandering, 0, 0, SCE_PRIO_DEF_2, 0);
        }
        if (r203_work.p->em[1].setEm(0x35, 2, 0, 1, 0) == 1) {
            SceExec(0x12, (TaskFunc) r203_GanadoWandering, 1, 0, SCE_PRIO_DEF_2, 0);
        }
        if (r203_work.p->em[2].setEm(0x36, 2, 0, 1, 0) == 1) {
            SceExec(0x12, (TaskFunc) r203_GanadoWandering, 2, 0, SCE_PRIO_DEF_2, 0);
        }
    } else {
        u32 i;

        for (i = 0; i < 9; i++) {
            cEmWrapSetEmI(&r203_work.p->em[tbl[i][0]], tbl[i][1], 2, 0, 1, 0);
        }
        SceAtDataSet_exec(0x8A, SCE_LEVEL10, 0, (TaskFunc) r203_GetKeyItem, 0, 1);
        SceExec(0x12, (TaskFunc) r203_GanadoEscape, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if ((pG->Key_flg[0] & 0x00020000) == 0) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r203_LockDoor, 0, 1);
        SceExec(0x12, (TaskFunc) r209_CheckUseKey, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        if (StaFlagChk(pG, STA_SUB_ASHLEY) == 0) {
            StaFlagOn(pG, STA_SUB_ASHLEY);
            SubCharInit(1, &pPL->pos, pPL->ang.y);
            SubCharCtrl(SCC_CHASE, 0);
        }
        r203_work.p->data = DC.setData(EvtMgr.NameChange("evd/r203s00.evd"));
        r203_work.p->data->setCommand(CMND_ARAM_LOAD, 0, 0);
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r203_EventMeetAgain, 0, 1);
        EvtMgr.SetFunc("evt_r203s00_func", (void*) Evt_R203S00_Func);
    }
    SceExec(0x12, (TaskFunc) r203_StreamCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    SceSetItemEvent(7, 0x8A, 4, 3, r203_TreasureBoxOpen, (void (*)()) r203_TreasureBoxOpened, 0x17, 0);
    SceSetItemEvent(8, 0x88, 6, 4, (void (*)(int)) r203_ShelfOpen, r203_ShelfOpened, 0, 0);
}

// Per-frame room main: nothing.
void R203Main()
{
}

// The door needs the key item: open the item screen when the player has it.
static void r203_LockDoor()
{
    SceUpCut(0, -1, 3, UP_CUT_ATTR_CUT_FIX);
    if (ItemMgr.num(0xA7)) {
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
    } else {
        CamCtrl.Comeback(0);
    }
}

// Task: waits for key item 0xA7 to be used, then unlocks the door (Key_flg[0] 0x00020000, area 1
// re-armed) with message up-cut 1/4. (Named after r209's copy.)
static void r209_CheckUseKey()
{
    while (ItemMgr.check(0xA7) == 0) {
        SceSleep(1);
    }
    SceAtDataReset(1);
    pG->Key_flg[0] |= 0x00020000;
    SceUpCut(1, -1, 4, 0);
}

// The Ganado with the key runs off through two way points; the others chase the player.
static void r203_GanadoEscape()
{
    Vec pos[3] = {
        {-42953.0f, 4155.0f, -15051.0f},
        {-50199.0f, 4155.0f, -19637.0f},
        {-48575.0f, 4155.0f, -7620.0f},
    };

    while (r203_work.p->em[6].ckFindPL() != 1) {
        SceSleep(1);
    }
    r203_work.p->em[6].setGoto(&pos[0], 1);
    while (r203_work.p->em[6].ckGoto() == 1) {
        SceSleep(1);
    }
    r203_work.p->em[6].setGoto(&pos[1], 1);
    SceSleep(150);
    if (!(pG->Room_flg[2] & 0x80000000)) {
        r203_work.p->em[0].setGoto(&pPL->pos, 0xD);
        r203_work.p->em[1].setGoto(&pPL->pos, 0xD);
        r203_work.p->em[2].setGoto(&pPL->pos, 0xD);
    }
}

// Area 0x8A: up to five more Ganados appear, depending on how many are already in the area.
static void r203_GetKeyItem()
{
    int tbl[5][2] = {
        {9, 0x4B}, {10, 0x4C}, {11, 0x4D}, {12, 0x4E}, {13, 0x4F},
    };
    int idx[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    Vec pos[2] = {
        {-61082.0f, 4189.0f, 1275.0f},
        {-65049.0f, 3156.0f, 8665.0f},
    };
    u32 cnt = 0;
    u32 n = 5;
    u32 i;

    SceAtDataReset(0x8A);
    SceAtExecute(0x8A);
    for (i = 0; i < 9; i++) {
        if (SceAtCheckHitModel(2, r203_work.p->em[idx[i]].getPtr()) == 1) {
            cnt++;
        }
    }
    if (cnt <= 5) {
        n = 5 - cnt;
    }
    for (i = 0; i < n; i++) {
        cEmWrapSetEmI(&r203_work.p->em[tbl[i][0]], tbl[i][1], 2, 0, 1, 0);
        r203_work.p->em[tbl[i][0]].setGoto(&pos[i & 1], 0xD);
    }
}

// Wandering task of Ganado `no`: walks the three way points until it finds the player.
static void r203_GanadoWandering(int no)
{
    int on = 1;

    r203_work.p->pt[no] = no;
    while (on) {
        cEmWrap* em = &r203_work.p->em[no];

        if (em->ckFindPL() == 1 || em->isActive() == 0) {
            on = 0;
        } else if (em->ckGoto() != 6) {
            r203_work.p->pt[no]++;
            {
                s8* pt = r203_work.p->pt;

                pt[no] = pt[no] < 0 ? 2 : (pt[no] > 2 ? 0 : pt[no]);
            }
            em->setGoto((Vec*) (r203_work.p->pt[no] * sizeof(Vec) + (u32) r203_wanderPos), 6);
        }
        SceSleep(1);
        // Dead test (store dead in flow, compare in flow2): its extra basic block takes the loop to 11
        // blocks, above haifa's MAX_RGN_BLOCKS (10), so sched1 forms no interblock region and nothing
        // is hoisted above the branches (the original's shape; its loop had one more block).
        if (r203_work.p->data == 0) {
            em = 0;
        }
    }
}

// Area 3: the s00 event, then Leon and Ashley are placed at the door.
static void r203_EventMeetAgain()
{
    Vec pos = {-27823.0f, 4155.0f, -7863.0f};
    Vec ang;
    Vec* pa = &ang;
    ReadModule* m;

    RsfSet(G_ROOM_ID, 3);
    m = SearchEmModule(0x11);
    SceEventStart(0);
    if (r203_work.p->data->waitLoadOk() == 1) {
        MemorySwap(m->pArc, (u32) r203_work.p->data->m_addr, r203_work.p->data->m_size);
        EvtMgr.SetEvt(m->pArc, 0);
        while (EvtMgr.IsAliveEvt(&EvtMgr.NowExeEvtKey, 0, 0) != 0) {
            SceSleep(1);
        }
        MemorySwap(m->pArc, (u32) r203_work.p->data->m_addr, r203_work.p->data->m_size);
        r203_work.p->data->setCommand(CMND_DEL_DATA, 0, 0);
    }
    {
        f32 ry = -2.45f;
        cPlayer* pl = pPL;
        Vec* pp = &pos;

        pl->setPos(pp);
        ang.x = 0.0f;
        pa->y = ry;
        ang.z = 0.0f;
        pl->setAng(pa);
        {
            cSubChar* sub = pSUB;

            if (sub) {
                sub->setPos(&pPL->pos);
                ang.x = 0.0f;
                pa->y = ry;
                ang.z = 0.0f;
                sub->setAng(pa);
                SubCharCtrl(SCC_CHASE, 0);
            }
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    GameSaveSave(&GameSave, pSaveData, -1);
    // global-alloc pass 0 (regs_used_so_far): with r29 and f31 ever-live, the 0.0 pseudo takes f31,
    // m/ry share r29 and &ang falls to r31 in pass 1 as in the target (no code emitted).
    register int pin asm("r29");   // COMPILER-DIFF: candidate #17
    register f32 fpin asm("fr31"); // COMPILER-DIFF: candidate #17
    asm("" : "=r"(pin));
    asm("" : "=f"(fpin));
    asm("" : : "r"(pin), "f"(fpin));
}

// Item-event opener: chest `id` lid up (+Z).
static void r203_TreasureBoxOpen(int id)
{
    OpenBoxMain(OpenBoxUpZP, 0, 0x5B, id, -1, -1);
}

// Item-event "already opened": chest `id` posed open.
static void r203_TreasureBoxOpened(int id)
{
    OpenBoxMain(OpenBoxUpZP, 1, 0x5B, id, -1, -1);
}

// Item-event opener: the shelf (objects 0x1B/0x1A) swings open.
static void r203_ShelfOpen()
{
    OpenBoxMain(0, 0, 0x1A, 0x1B, 0x1A, -1);
}

// Item-event "already opened": the shelf posed open.
static void r203_ShelfOpened()
{
    OpenBoxMain(0, 1, 0x1A, 0x1B, 0x1A, -1);
}

// Battle stream while a Ganado within 30000 has found the player.
static void r203_StreamCheck()
{
    f32 lim = 900000000.0f;
    int on = 0;

    for (;;) {
        int find = 0;
        u32 i;

        for (i = 0; i < EmMgr.nArray; i++) {
            cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

            if (em->id >= 0x10 && em->id <= 0x20 && em->checkStatus(EM_STATUS_ACTIVE) != 0 && em->hp > 0 && (em->be_flag & 0x201) == 1
                && ((cEmGanado*) em)->ckFindPL() == 1 && em->plDist2 < lim) {
                find = 1;
            }
        }
        if (find == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else {
            if (on == 1) {
                SndRoomStrStop(3);
                on = 0;
            }
        }
        SceSleep(1);
        // Dead test (r108 str_check idiom): keeps the second loop pass from hoisting the inner
        // loop's EmMgr high out of the outer loop (the original's two EmMgr chains).
        if (EmMgr.size == 0) {
            find = 1;
        }
        find = 2;
    }
}

// Event r203s00 callback: light mask 2 on the pl0400 model on its first frame.
extern "C" void Evt_R203S00_Func(Event* e)
{
    if (e->funcMode == 1 && e->NowCut == 0 && e->NowFrame == 0) {
        void* mod;

        if (e->GetMod(&mod, "pl0400", 0, 0) == 1) {
            ((cModel*) mod)->LightInfo.EnableMask = 2;
        }
    }
}

// The module's .data continues 8-aligned.
asm(".section .data; .balign 8");
