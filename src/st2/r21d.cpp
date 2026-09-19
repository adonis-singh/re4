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
#include "em.h"
#include "em_wrap.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "st_mgr_event.h"
#include "mes.h"
#include "rnd.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "fade.h"
#include "sscrn.h"
#include "TexRender.h"
#include "cSceObj.h"
#include "db_log.h"

// Room 2-1D (D:/Bio4/Prog/r21d.cpp): the two laser switches and the iron seal, the fence, the
// grave lift, the death trap pistons and the enemy reset scheduling.

// One death trap piston.
class TRAP {
public:
    u8 step;         // 0x00
    u8 x1;
    u8 x2;
    u8 x3;
    cObj* obj;       // 0x04
    f32 baseY;       // 0x08
    f32 spd;         // 0x0C  fall speed
    int timer;       // 0x10
    int atOn;        // 0x14  the hit area is enabled
    int atNo;        // 0x18  hit area number
    int hitAtNo;     // 0x1C  area checked for the player
    int wait;        // 0x20  frames before the first rise
    int x24;         // 0x24  1: hold at the bottom
    int x28;         // 0x28  extra frames at the top
    int stopFlag;    // 0x2C  1: hold at the top

    void stop(int v);
    void move();
};

struct R21dWork {
    u32 emMax;            // 0x000  alive count that stops the resets
    u32 emNum;            // 0x004  entries used in em[]
    u32 emSetCount;       // 0x008  enemies set so far
    u32 emTotal;          // 0x00C  enemies to set in all
    u32 resetWait;        // 0x010  frames before a reset is placed
    int final;            // 0x014  1 while the final wave is being set
    cEmWrap em[16];       // 0x018
    int resetting[16];    // 0x0D8
    cSceObj fence[2];     // 0x118
    u8 eff;               // 0x308  EspPullCoreKind
    u8 pad_309[3];
    TRAP trap[5];         // 0x30C
    u32 str;              // 0x3FC  SndStrReq handle
    TexRenderMng* tex;    // 0x400
    cSceObj* pSwitch;     // 0x404  the switch being operated
    u8 pad_408[0xC];
    cSceObj deathSw;      // 0x414
    cSceObj sw[2];        // 0x50C
};

struct R21dTrapData {
    u32 objId;
    int atNo;
    int hitAtNo;
    int wait;
};

// The work pointer is a struct member: every store through the work reloads it.
struct R21dWorkPtr {
    R21dWork* p;
};

static u8 r21d_texTbl[0x20];
static R21dWorkPtr r21d_work;

R21dTrapData r21d_trapTbl[5] = {
    {0xF, 1, 5, 0},
    {0x10, 2, 6, 0},
    {0x11, 3, 7, 0xA},
    {0x28, 4, 8, 5},
    {0x12, 0x11, 0x14, 0},
};

int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");

void r21d_checkEmResetPoint(Vec* pos, f32* ang);
static void r21d_setEmReset(int no);
void r21d_searchEmReset();
static void r21d_checkEmReset();
void r21d_addEmSet(int no);
static void r21d_setEmFinal();
void r21d_setEm4();
void r21d_setEm3();
static void r21d_setEm2();
void r21d_initEmSet();
void r21d_initIronSeal(int done);
void r21d_onSwitch(int no, int init);
void r21d_irradiateLaser();
void r21d_operateSwitch_end(u32 n);
static void r21d_operateSwitch(int no);
void r21d_initSwitch();
void r21d_moveFence();
static void r21d_checkFence();
void r21d_initFence();
static void r21d_moveGrave(int dir);
static void r21d_checkGrave();
static void r21d_checkBgmPlay();
static void r21d_setDeathTrap2nd();
static void r21d_moveDeathTrap();
static void r21d_checkDeathTrapSwitch_end();
static void r21d_checkDeathTrapSwitch();
void r21d_initDeathTrapSwitch();
static void setTexRender();

// Struct-member view of pPL: the load stays below a preceding store (r10c).
struct PlPtr {
    cPlayer* p;
};
#define pPLS (((PlPtr*) &pPL)->p)

// Event skip: the skip key or the skip flag; the event is marked skipped.
#define R21D_SKIP ((Key.trg & 0x20000000) || (pG->Room_flg[0] & 0x80000000))
#define R21D_SKIP_SET() pG->Room_flg[0] |= 0x80000000
// fade.h's FadeSetW with `zero`/`black` locals (zero first): the loop-hoisted constants of moveFence's
// skip block get the target's registers (0 -> r27, 0xFF -> r28) and store order (start, end).
static inline void r21d_FadeSetW(int no, u32 time, u32 z, int late)
{
    FadeColorPair col;
    u32 black;
    u32 zero;

    zero = 0;
    black = 0xFF;
    if (no & 0x80000000) {
        *(u32*) &col.start = black;
    } else {
        *(u32*) &col.start = zero;
    }
    if (no & 0x80000000) {
        *(u32*) &col.end = zero;
    } else {
        *(u32*) &col.end = black;
    }
    FadeSet(no, &col.start, &col.end, time, z, late);
}

// Room init: JumpPoint 1/2 presets the fence and both switches done (Room_flg bits 0/7/8); the enemy
// reset scheduling, the two laser switches, the fence, area 0xC = the grave lift (arriving from r225
// normally rides it down first); the player's room motions; the seal render target.
void R21dInit()
{
    R21dWork*& wp = r21d_work.p;   // the pG load that follows stays below the store
#line 46 "D:/Bio4/Prog/r21d.cpp"
    wp = (R21dWork*) MEM_CALLOC(sizeof(R21dWork), 1, 0xd);
    if (pGS->JumpPoint == 1 || pGS->JumpPoint == 2) {
        RsfSet(G_ROOM_ID, 0);
        RsfSet(G_ROOM_ID, 7);
        RsfSet(G_ROOM_ID, 8);
    }
    r21d_initEmSet();
    r21d_initSwitch();
    r21d_initFence();
    SceAtDataSet_exec(0xC, SCE_LEVEL10, 0, (TaskFunc) r21d_checkGrave, 0, 1);
    if (pG->room_id_prev == 0x225 && FlagChkSignW(pG->System_flg, SYS_LOAD_GAME) == 0 && FlagChkSignW(pG->System_flg, SYS_CONTINUE) == 0) {
        SceExec(0x12, (TaskFunc) r21d_moveGrave, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0, 0, 0, 0,
                   ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24),
                   ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26));
    SceExec(0x12, (TaskFunc) r21d_checkBgmPlay, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Per-frame room main: nothing.
void R21dMain()
{
}

// Picks the position / angle an enemy is reset to, by the player's height and the room state.
void r21d_checkEmResetPoint(Vec* pos, f32* ang)
{
    if (pPL->pos.y < -3850.0f) {
        pos->x = -12579.0f;
        pos->y = 4400.0f;
        pos->z = -14930.0f;
        *ang = 0.243f;
    } else if (pPL->pos.y < 4028.0f) {
        switch (Rnd() & 1) {
        case 0:
            pos->x = -28698.0f;
            pos->y = 16066.0f;
            pos->z = 25966.0f;
            *ang = 2.371f;
            break;
        case 1:
            pos->x = 1576.0f;
            pos->y = 16113.0f;
            pos->z = -22417.0f;
            *ang = -0.7f;
            break;
        default:
            return;
        }
    } else if (RsfCheck(G_ROOM_ID, 5) == 0) {
        u8 r = Rnd() % 3;

        switch (r) {
        case 0:
            pos->x = -11182.0f;
            pos->y = 764.0f;
            pos->z = -13492.0f;
            *ang = 0.243f;
            break;
        case 1:
            pos->x = -28698.0f;
            pos->y = 16066.0f;
            pos->z = 25966.0f;
            *ang = 2.371f;
            break;
        case 2:
            pos->x = 1576.0f;
            pos->y = 16113.0f;
            pos->z = -22417.0f;
            *ang = -0.7f;
            break;
        default:
            return;
        }
    } else {
        u8 r = Rnd() % 3;

        switch (r) {
        case 0:
            pos->x = -28698.0f;
            pos->y = 16066.0f;
            pos->z = 25966.0f;
            *ang = 2.371f;
            break;
        case 1:
            pos->x = -1693.0f;
            pos->y = 16113.0f;
            pos->z = 29731.0f;
            *ang = 3.0f;
            break;
        case 2:
            pos->x = 1576.0f;
            pos->y = 16113.0f;
            pos->z = -22417.0f;
            *ang = -0.7f;
            break;
        default:
            return;
        }
    }
}

// Task: resets enemy `no` after the wait (at once when the final wave is running).
static void r21d_setEmReset(int no)
{
    Vec pos;
    u32 i;

    for (i = 0; i < r21d_work.p->resetWait; i++) {
        if (r21d_work.p->final == 1) {
            SceSleep(Rnd() & 0x1F);
            break;
        }
        SceSleep(1);
    }
    Vec ang = {0.0f, 0.0f, 0.0f};

    r21d_checkEmResetPoint(&pos, &ang.y);
    r21d_work.p->em[no].setReset((int) &pos, (int) &ang);
    r21d_work.p->resetting[no] = 0;
}

// Starts a reset task for the first enemy that may be reset.
void r21d_searchEmReset()
{
    u32 i = 0;

    if (i < r21d_work.p->emNum) {
        do {
            if (r21d_work.p->em[i].ckResetEnable() == 1) {
                if (r21d_work.p->resetting[i] == 1) {
                    break;
                }
                r21d_work.p->resetting[i] = 1;
                SceExec(0x12, (TaskFunc) r21d_setEmReset, i, 0, SCE_PRIO_DEF_2, 0);
                r21d_work.p->emSetCount++;
                break;
            }
            i++;
        } while (i < r21d_work.p->emNum);
    }
}

// Task: keeps the enemy count up through resets.
static void r21d_checkEmReset()
{
    SceSleep(10);
    for (;;) {
        if (r21d_work.p->emSetCount < r21d_work.p->emTotal) {
            if ((u32) SceCountEmAlive(0x2D, -1) < r21d_work.p->emMax) {
                r21d_searchEmReset();
            }
        }
        SceSleep(1);
    }
}

// Sets list entry `no` into the next free wrap.
void r21d_addEmSet(int no)
{
    cEmWrapSetEmI(&r21d_work.p->em[r21d_work.p->emNum], no, -1, 0, 1, 1);
    r21d_work.p->emNum++;
    r21d_work.p->emSetCount++;
}

// Task: the final wave after both switches.
static void r21d_setEmFinal()
{
    RsfSet(G_ROOM_ID, 6);
    r21d_work.p->emMax = 10;
    r21d_work.p->emSetCount = 0;
    r21d_work.p->resetWait = 450;
    r21d_work.p->emTotal = 10;
    r21d_work.p->final = 1;
    SceSleep(300);
    r21d_work.p->final = 0;
}

// The last three enemies (0x25..0x27), counted into emSetCount.
void r21d_setEm4()
{
    setEm(0x25, -1, 0, 1, 1);
    setEm(0x26, -1, 0, 1, 1);
    setEm(0x27, -1, 0, 1, 1);
    r21d_work.p->emSetCount += 3;
}

// Two more list entries (0x15/0x18) into the wraps; the alive cap rises by two.
void r21d_setEm3()
{
    r21d_addEmSet(0x15);
    r21d_addEmSet(0x18);
    r21d_work.p->emMax += 2;
}

// Areas 0xD/0x10 once (Room_flg bit 5): four more list entries (0x1B..0x1E) join the reset pool.
static void r21d_setEm2()
{
    RsfSet(G_ROOM_ID, 5);
    SceAtSetEnable(0xD, 0);
    SceAtSetEnable(0x10, 0);
    r21d_addEmSet(0x1B);
    r21d_addEmSet(0x1C);
    r21d_addEmSet(0x1D);
    r21d_addEmSet(0x1E);
}

// The reset scheduler: alive cap 4, 450-frame reset wait, 15 enemies in all; the first four list
// entries (0x14/0x16/0x17/0x19), the reset task, and areas 0xD/0x10 = the second group.
void r21d_initEmSet()
{
    u32 i;

    r21d_work.p->emMax = 4;
    r21d_work.p->resetWait = 450;
    r21d_work.p->emTotal = 15;
    r21d_addEmSet(0x14);
    r21d_addEmSet(0x16);
    r21d_addEmSet(0x17);
    r21d_addEmSet(0x19);
    for (i = 0; i < 16; i++) {
        r21d_work.p->resetting[i] = 0;
    }
    SceExec(0x12, (TaskFunc) r21d_checkEmReset, 0, 0, SCE_PRIO_DEF_2, 0);
    SceAtDataSet_exec(0xD, SCE_LEVEL10, 0, (TaskFunc) r21d_setEm2, 0, 1);
    SceAtDataSet_exec(0x10, SCE_LEVEL10, 0, (TaskFunc) r21d_setEm2, 0, 1);
}

// The iron seal: shown while the lasers are on (done 0), replaced by the opened one (done 1).
void r21d_initIronSeal(int done)
{
    if (done == 0) {
        if (SmdGetObjPtr(0x1F)) {
            SmdGetObjPtr(0x1F)->be_flag |= 2;
        }
        if (SmdGetObjPtr(0x20)) {
            SmdGetObjPtr(0x20)->be_flag |= 2;
        }
        if (SmdGetObjPtr(4)) {
            SmdGetObjPtr(4)->be_flag &= ~2;
        }
    } else {
        if (SmdGetObjPtr(0x1F)) {
            SmdGetObjPtr(0x1F)->be_flag &= ~2;
        }
        if (SmdGetObjPtr(0x20)) {
            SmdGetObjPtr(0x20)->be_flag |= 2;
        }
        if (SmdGetObjPtr(4)) {
            SmdGetObjPtr(4)->be_flag |= 2;
        }
        SceAtSetEnable(0, 0);
        SceAtSetEnable(0xA, 0);
        setTexRender();
    }
}

// Switch `no` goes down and its laser lights up (init 1: set the end state without the event).
void r21d_onSwitch(int no, int init)
{
    cObj* obj = 0;
    int cut1 = 0;
    int cut2 = 0;
    int lightA = 0;
    int lightB = 0;
    int kind = 0;
    u32 i;

    switch (no) {
    case 0:
        r21d_work.p->pSwitch = &r21d_work.p->sw[0];
        obj = SmdGetObjPtr(0x1A);
        cut1 = 0x13;
        cut2 = 0x15;
        lightA = 0xD;
        lightB = 0xE;
        kind = 1;
        break;
    case 1:
        r21d_work.p->pSwitch = &r21d_work.p->sw[1];
        obj = SmdGetObjPtr(0x1A);
        cut1 = 0x14;
        cut2 = 0x16;
        lightA = 0xF;
        lightB = 0x10;
        kind = 2;
        break;
    }
    if (obj) {
        if (init == 0) {
            CamCtrl.CutCall(cut1);
            for (i = 0; i < 10; i++) {
                if (R21D_SKIP) {
                    R21D_SKIP_SET();
                    break;
                }
                SceSleep(1);
            }
        } else {
            r21d_work.p->pSwitch->setEndPos();
        }
        EstSet(0, -1, 0, 0, 1, (u8) lightA, 1, r21d_work.p->eff, 0, 0);
        if (init == 0) {
            while (CamCtrl.IsMotionEnd() == 0) {
                if (R21D_SKIP) {
                    R21D_SKIP_SET();
                    break;
                }
                r21d_work.p->pSwitch->move();
                SceSleep(1);
            }
            CamCtrl.CutCall((s8) cut2);
            for (i = 0; i < 10; i++) {
                if (R21D_SKIP) {
                    R21D_SKIP_SET();
                    break;
                }
                SceSleep(1);
            }
        }
        EstSet(0, -1, 0, 0, 1, (u8) lightB, 1, r21d_work.p->eff, 0, 0);
        obj->pModelInfo->flagsDC |= 1;
        obj->pModelInfo->uvScrollU = 0.01677f;
        obj->pModelInfo->uvScrollV = 0.01343f;
        LightMgr.onKind((u8) kind);
        if (init == 0) {
            cLight* l = LightMgr.getKindLight((u8) kind);
            f32 target = l->Radius;
            f32 step = target / 30.0f;

            l->Radius = 0.0f;
            while (CamCtrl.IsMotionEnd() == 0) {
                if (R21D_SKIP) {
                    R21D_SKIP_SET();
                    break;
                }
                if (l->Radius <= target) {
                    l->Radius += step;
                }
                SceSleep(1);
            }
            l->Radius = target;
        }
    }
}

// The laser beam between the two switches (cuts 0x17 / 0x18).
void r21d_irradiateLaser()
{
    EstSet(0, -1, 0, 0, 1, 0x11, 1, r21d_work.p->eff, 0, 0);
    CamCtrl.CutCall(0x17);
    while (CamCtrl.IsMotionEnd() == 0) {
        if (R21D_SKIP) {
            R21D_SKIP_SET();
            break;
        }
        SceSleep(1);
    }
    CamCtrl.CutCall(0x18);
    while (CamCtrl.IsMotionEnd() == 0) {
        if (R21D_SKIP) {
            R21D_SKIP_SET();
            break;
        }
        SceSleep(1);
    }
}

// End of a switch event: `n` switches are down.
void r21d_operateSwitch_end(u32 n)
{
    SceEventEnd(0);
    if (pG->Room_flg[0] & 0x80000000) {
        FadeSetW(0x80000000, 10, 0, 0);
        SubScreenWait(20);
        if (r21d_work.p->str != 0) {
            SndStrReq(r21d_work.p->str, 8, 0, 0);
        }
        r21d_work.p->pSwitch->setEndPos();
    }
    switch (n) {
    case 0:
        break;
    case 1:
        r21d_setEm3();
        break;
    case 2:
        SceExec(0x12, (TaskFunc) r21d_setEmFinal, 0, 0, SCE_PRIO_DEF_2, 0);
        r21d_initIronSeal(1);
        EffectEspDelete(0, r21d_work.p->eff, 0, 0);
        EffectEspgenDelete(0, r21d_work.p->eff, 0);
        EffectEfmDelete(0, r21d_work.p->eff, 0);
        break;
    }
}

// Area 0xE / 0xF: operate switch `no`.
static void r21d_operateSwitch(int no)
{
    int atNo = 0;
    u32 count;

    switch (no) {
    case 0:
        atNo = 0xE;
        break;
    case 1:
        atNo = 0xF;
        break;
    }
    SceAtSetEnable(atNo, 0);
    SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case -1:
    case 0:
    case 2:
        SceAtSetEnable(atNo, 1);
        return;
    case 1:
        goto yes;
    default:
        goto yes;
    }
yes:
    switch (no) {
    case 0:
        RsfSet(G_ROOM_ID, 7);
        r21d_setEm4();
        break;
    case 1:
        RsfSet(G_ROOM_ID, 8);
        break;
    }
    count = 0;
    if (RsfCheck(G_ROOM_ID, 7)) {
        count = 1;
    }
    if (RsfCheck(G_ROOM_ID, 8)) {
        count++;
    }
    U32Set(r21d_work.p->str, 0);
    pG->Room_flg[0] &= 0x7FFFFFFF;
    SceEventStart(0);
    switch (count) {
    case 0:
        break;
    case 1:
        r21d_work.p->str = SndStrReq(1, 0x2E, 0x80000003, 0, 0, 0.0f);
        r21d_onSwitch(no, 0);
        break;
    case 2:
        r21d_work.p->str = SndStrReq(1, 0x2D, 0x80000003, 0, 0, 0.0f);
        r21d_onSwitch(no, 0);
        r21d_irradiateLaser();
        break;
    }
    r21d_operateSwitch_end(count);
}

// The two laser switches (objects 0x23/0x24): 2-frame move1 presses; each not yet thrown (Room_flg
// bits 7/8) gets its area (0xE/0xF), else is posed down with its laser; both done -> the seal opens.
void r21d_initSwitch()
{
    cObj* o23;
    cObj* o24;

    r21d_work.p->eff = EspPullCoreKind();
    Vec a = {0.0f, 0.0f, 0.0f};
    Vec b = {0.0f, 0.0f, 0.0f};
    Vec d;

    o23 = SmdGetObjPtr(0x23);
    a.x = -43739.0f;
    a.y = 1389.0f;
    a.z = -11530.0f;
    o24 = SmdGetObjPtr(0x24);
    b.x = 32504.002f;
    b.y = 5980.0f;
    b.z = 8967.0f;
    PSVECSubtract(&a, &o23->pos, &d);
    r21d_work.p->sw[0].initMove1_pos(o23, 2, &d, 0.0f, 0.0f);
    PSVECSubtract(&b, &o24->pos, &d);
    r21d_work.p->sw[1].initMove1_pos(o24, 2, &d, 0.0f, 0.0f);
    if (RsfCheck(G_ROOM_ID, 7) == 0 || RsfCheck(G_ROOM_ID, 8) == 0) {
        if (RsfCheck(G_ROOM_ID, 7) == 0) {
            SceAtDataSet_exec(0xE, SCE_LEVEL10, 0, (TaskFunc) r21d_operateSwitch, 0, 1);
            LightMgr.offKind(1);
        } else {
            r21d_onSwitch(0, 1);
        }
        if (RsfCheck(G_ROOM_ID, 8) == 0) {
            SceAtDataSet_exec(0xF, SCE_LEVEL10, 0, (TaskFunc) r21d_operateSwitch, (void*) 1, 1);
            LightMgr.offKind(2);
        } else {
            r21d_onSwitch(1, 1);
        }
        r21d_initIronSeal(0);
    } else {
        LightMgr.offKind(1);
        LightMgr.offKind(2);
        r21d_onSwitch(0, 1);
        r21d_onSwitch(1, 1);
        EffectEspDelete(0, r21d_work.p->eff, 0, 0);
        EffectEspgenDelete(0, r21d_work.p->eff, 0);
        EffectEfmDelete(0, r21d_work.p->eff, 0);
        r21d_initIronSeal(1);
    }
}

// The fence rises (event, cut 0x10).
void r21d_moveFence()
{
    u32 i;

    EstSet(0, -1, 0, 0, 1, 8, 0, 0, 0, 0);
    SndCall(6, 5, 0, 0, 0, 0);
    CamCtrl.CutCall(0x10);
    while (1) {
        if (R21D_SKIP) {
            R21D_SKIP_SET();
            r21d_work.p->fence[0].setEndPos();
            r21d_work.p->fence[1].setEndPos();
            break;
        }
        r21d_work.p->fence[1].move();
        if (r21d_work.p->fence[0].move() == 0) {
            break;
        }
        SceSleep(1);
    }
    SndCall(6, 6, 0, 0, 0, 0);
    for (i = 0; i < 15; i++) {
        if (R21D_SKIP) {
            R21D_SKIP_SET();
            r21d_FadeSetW(0x80000000, 10, 0, 0);
            SubScreenWait(20);
            break;
        }
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
}

// Area 0x13: the fence switch.
static void r21d_checkFence()
{
    SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case -1:
    case 0:
    case 2:
        break;
    case 1:
        goto yes;
    default:
        goto yes;
    }
    return;
yes:
    RsfSet(G_ROOM_ID, 0);
    ScfFlagOn(pG, SCF_87);
    SceAtSetEnable(0x13, 0);
    pG->Room_flg[0] &= 0x7FFFFFFF;
    SceEventStart(0);
    r21d_moveFence();
    SceEventEnd(0);
    SceAtSetEnable(0x12, 0);
    SceExec(0x12, (TaskFunc) r21d_moveDeathTrap, 0, 6, SCE_PRIO_DEF_2, 0);
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        SceAtDataSet_exec(0x15, SCE_LEVEL10, 0, (TaskFunc) r21d_checkDeathTrapSwitch, 0, 1);
    }
    GameSaveSave(&GameSave, pSaveData, -1);
}

// The two fence halves (objects 0x13/0x21): 60-frame move1 up by 2800. Not yet raised (Room_flg bit
// 0): area 0x13 = the fence event; else posed raised, area 0x12 off, the death-trap pistons run, and the
// second-switch lever unless bit 9.
void r21d_initFence()
{
    Vec d = {0.0f, 2800.0f, 0.0f};
    cObj* o13 = SmdGetObjPtr(0x13);
    cObj* o21 = SmdGetObjPtr(0x21);

    if (o13 && o21) {
        r21d_work.p->fence[0].initMove1_pos(o13, 60, &d, 20.0f, 0.0f);
        r21d_work.p->fence[1].initMove1_pos(o21, 60, &d, 20.0f, 0.0f);
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(0x13, SCE_LEVEL10, 0, (TaskFunc) r21d_checkFence, 0, 1);
    } else {
        r21d_work.p->fence[0].setReverse(1);
        r21d_work.p->fence[1].setReverse(1);
        SceAtSetEnable(0x12, 0);
        SceExec(0x12, (TaskFunc) r21d_moveDeathTrap, 0, 6, SCE_PRIO_DEF_2, 0);
        if (RsfCheck(G_ROOM_ID, 9) == 0) {
            SceAtDataSet_exec(0x15, SCE_LEVEL10, 0, (TaskFunc) r21d_checkDeathTrapSwitch, 0, 1);
        }
    }
    r21d_initDeathTrapSwitch();
}

// The grave lift (object 7) rises (dir 1) or sinks back (dir 0) with Leon on it.
static void r21d_moveGrave(int dir)
{
    cObj* obj = SmdGetObjPtr(7);

    if (obj) {
        obj->be_flag |= 0x20;
        pPLS->setNoSuspend(1);
        Vec d = {0.0f, 3000.0f, 0.0f};
        cSceObj grave;
        u32 i;

        grave.initMove1_pos(obj, 90, &d, 10.0f, 0.0f);
        grave.setVibration(10, 10, 2.0f, 1.0f, 4.0f);
        {
            cPlayer* pl = pPL;
            u32 n;

            if (pl) {
                for (n = 0; n < 4; n++) {
                    if (grave.sub[n] == NULL) {
                        grave.sub[n] = pl;
                        break;
                    }
                }
            }
        }
        SceEventStart(0);
        DpfFlagOn(pG, DPF_SHADOW);
        if (dir == 1) {
            CamCtrl.CutCall(0xE);
            SndCall(6, 7, 0, 0, 0, 0);
            for (i = 0; i < 90; i++) {
                grave.move();
                if (i == 60) {
                    FadeSetW(1, 30, 0, 0);
                }
                SceSleep(1);
            }
        } else {
            CamCtrl.CutCall(0xF);
            grave.setReverse(1);
            SndCall(6, 7, 0, 0, 0, 0);
            for (i = 0; i < 90; i++) {
                grave.move();
                SceSleep(1);
            }
            SndCall(6, 8, 0, 0, 0, 0);
            SceSleep(15);
            CamCtrl.Comeback(0);
        }
        DpfFlagOff(pG, DPF_SHADOW);
        SceEventEnd(0);
    }
}

// Area 0xC: the grave lift down and the chapter end.
static void r21d_checkGrave()
{
    r21d_moveGrave(1);
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        RsfSet(G_ROOM_ID, 4);
        SceSetChapterEnd(CHAPTER_4_2, 0xC);
    } else {
        SceAtExecute(0xC);
    }
}

// Task: the battle stream while enemies have found the player.
static void r21d_checkBgmPlay()
{
    while (SceCkFindPL(0) != 1) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 3, 1);
    while (SceCountEmAlive(0x2D, -1) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}

// Hold this piston at the top (v = 1) or let it cycle again.
void TRAP::stop(int v)
{
    stopFlag = v;
}

#define R21D_TRAP_HIT()                                                                        \
    if (SceAtHitCheck(hitAtNo) == 1) {                                                         \
        if (!StaFlagChk(pG, STA_DIEDEMO)) {                                                  \
            SndCall(6, 0xA, 0, 0, 0, 0);                                                       \
            pPL->dmg.set(0, 0x80);                                                             \
            pPL->setNoSuspend(1);                                                              \
            ((cUnitEventView*) pPL)->beginEvent(0);                                            \
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 10, 0, 0x101, 0);                 \
            DiedemoExec(30, 0);                                                                \
        }                                                                                      \
    }

// One piston: rises, holds, drops and crushes the player under it.
void TRAP::move()
{
    const f32 lim = 1600.0f;
    const f32 up = 35.0f;
    const f32 grav = 40.0f;
    const f32 height = 2800.0f;

    if (wait > 0) {
        wait--;
        return;
    }
    switch (step) {
    case 0:
        SndCall(6, 2, &obj->pos, 0, 0, 0);
        step = 1;
    case 1:
        obj->pos.y += up;
        if (atOn == 1) {
            if (obj->pos.y - baseY >= lim) {
                SceAtSetEnable(atNo, 0);
                atOn = 0;
            }
            R21D_TRAP_HIT();
        }
        if (obj->pos.y > baseY + height) {
            obj->pos.y = baseY + height;
            spd = 0.0f;
            step = 2;
            timer = x28 + 12;
        }
        break;
    case 2:
        if (stopFlag == 1) {
            break;
        }
        timer--;
        if (timer <= 0) {
            step = 3;
        }
        break;
    case 3:
        obj->pos.y -= spd;
        spd += grav;
        if (atOn == 0) {
            if (obj->pos.y - baseY <= lim) {
                SceAtSetEnable(atNo, 1);
                atOn = 1;
            }
        }
        if (obj->pos.y < baseY) {
            Vec v = {0.0f, -4400.0f, 0.0f};

            PSVECAdd(&v, &obj->pos, &v);
            EstSet(0, -1, &v, 0, 1, 0, 1, 0, 0, 0);
            SndCall(6, 3, &obj->pos, 0, 0, 0);
            obj->pos.y = baseY;
            timer = 30;
            step = 4;
        }
        break;
    case 4:
        if (x24 == 1) {
            break;
        }
        timer--;
        if (timer <= 0) {
            step = 0;
        }
        break;
    }
    if (atOn == 1) {
        R21D_TRAP_HIT();
    }
}

// The pistons in front of the second switch stop at the top.
static void r21d_setDeathTrap2nd()
{
    r21d_work.p->trap[0].stop(1);
    r21d_work.p->trap[1].stop(1);
    r21d_work.p->trap[3].stop(1);
}

// Task: runs the pistons until the grave lift has been used.
static void r21d_moveDeathTrap()
{
    u32 i;
    u32 n;

    for (i = 0; i < 5; i++) {
        const R21dTrapData* d = &r21d_trapTbl[i];
        u32 base = i * sizeof(TRAP) + (u32) r21d_work.p;
        cObj* obj = SmdGetObjPtr(d->objId);
        TRAP* t;

        base += 0x30C;   // reassigned: cse cannot re-base the zero-offset store on the old value
        t = (TRAP*) base;
        t->obj = obj;
        t->obj->be_flag |= 0x20;
        t->baseY = t->obj->pos.y;
        t->atNo = d->atNo;
        t->hitAtNo = d->hitAtNo;
        t->wait = d->wait;
        t->atOn = 1;
        t->x24 = 0;
        t->x28 = 0;
        t->stopFlag = 0;
        t->step = 0;
        t->x1 = 0;
        t->x2 = 0;
        t->x3 = 0;
    }
    SceSleep(1);
    if (RsfCheck(G_ROOM_ID, 9)) {
        SceExec(0x12, (TaskFunc) r21d_setDeathTrap2nd, 0, 2, SCE_PRIO_DEF_2, 0);
    }
    while (1) {
        if (RsfCheck(G_ROOM_ID, 4) == 0) {
            for (n = 0; n < 5; n++) {
                r21d_work.p->trap[n].move();
            }
            SceSleep(1);
        } else {
            break;
        }
    }
    for (n = 0; n < 5; n++) {
        r21d_work.p->trap[n].stop(1);
    }
}

// End of the lever event (also its cancel path, Room_flg[0] bit 31): the lever snapped, the second
// switch's pistons stop at the top unless already (0x40000000), camera back, SceEventEnd.
static void r21d_checkDeathTrapSwitch_end()
{
    if (pG->Room_flg[0] & 0x80000000) {
        r21d_work.p->deathSw.setEndPos();
        if (!(pG->Room_flg[0] & 0x40000000)) {
            SceExec(0x12, (TaskFunc) r21d_setDeathTrap2nd, 0, 2, SCE_PRIO_DEF_2, 0);
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 0x15: the lever that stops the pistons of the second switch.
static void r21d_checkDeathTrapSwitch()
{
    SceMesSet(4, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case -1:
    case 0:
    case 2:
        break;
    case 1:
        goto yes;
    default:
        goto yes;
    }
    return;
yes:
    RsfSet(G_ROOM_ID, 9);
    SceAtSetEnable(0x15, 0);
    pG->Room_flg[0] &= ~0x40000000;
    SceSetEventCancel(1, (TaskFunc) r21d_checkDeathTrapSwitch_end, 0, 0, 1);
    SceEventStart(0);
    CamCtrl.CutCall(0x19);
    SceSleep(15);
    SndCall(6, 9, 0, 0, 0, 0);
    while (r21d_work.p->deathSw.move() != 0) {
        SceSleep(1);
    }
    pG->Room_flg[0] |= 0x40000000;
    SceExec(0x12, (TaskFunc) r21d_setDeathTrap2nd, 0, 2, SCE_PRIO_DEF_2, 0);
    SceSleep(150);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r21d_checkDeathTrapSwitch_end();
}

// The piston lever (object 0x26): an 8-frame move1 turn of 1.3 rad about Z on its parts parent, with a
// shake; posed thrown when Room_flg bit 9.
void r21d_initDeathTrapSwitch()
{
    Vec r = {0.0f, 0.0f, 1.3f};
    cObj* obj = SmdGetObjPtr(0x26);

    r21d_work.p->deathSw.initMove1_ang(obj, 8, &r, 30.0f, 0.0f, 4);
    r21d_work.p->deathSw.setVibration(2, 2, 3.0f, 0.0f, 3.0f);
    if (RsfCheck(G_ROOM_ID, 9)) {
        r21d_work.p->deathSw.setEndPos();
    }
}

// The opened seal: a render target blended into object 4.
static void setTexRender()
{
    cObj* obj;
    u8* tbl = r21d_texTbl;

    if (GetTexRenderMgr(&r21d_work.p->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r21d_work.p->tex->texId;
        r21d_work.p->tex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, 1, 0x12, r21d_work.p->tex->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(4);
    obj->pModelInfo->blend_mode = 1;
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(1);
}
