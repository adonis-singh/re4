#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj01.h"
#include "em.h"
#include "emtorch.h"
#include "emswitch.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "math_sub.h"
#include "act_btn.h"
#include "item.h"
#include "mes.h"
#include "sscrn.h"
#include "fade.h"
#include "game.h"
#include "snd.h"
#include "esp.h"
#include "st_mgr_event.h"

// Room 2-0D (D:/Bio4/Prog/r20d.cpp): the hall with the three crank-raised fences, the round switch
// of the picture puzzle, the lantern throwing and the ten pass-through spots.

// One fence: the scroll object, its collision piece and the raise direction.
class cFence {
public:
    cObj* obj;     // 0x00
    cSat* sat;     // 0x04
    Vec pos;       // 0x08  lowered position
    Vec dir;       // 0x14  raise vector
    f32 t;         // 0x20  0 lowered .. 1 raised

    void move(f32 t);
    void init(struct R20dFenceData* d);
};

struct R20dFenceData {
    int objNo;     // 0x00
    f32 w;         // 0x04  collision size x
    f32 d;         // 0x08  collision size z
    Vec dir;       // 0x0C
};

// One lantern the player can pick up and throw at an enemy.
class cLanternUnit {
public:
    cEm* em;         // 0x00  the torch enemy
    cEm* target;     // 0x04
    u32 state;       // 0x08  0 idle, 1 being thrown, 2 done
    int step;        // 0x0C
    int active;      // 0x10
    void* mot[6];    // 0x14  [0] the etc archive, [1..5] player motions

    void destroy();
    void check();
    cEm* getTargetPos(Vec* out);
    static void throwLantern(cLanternUnit* u);
    void setThrowLantern(Vec* target);
};

class cLantern {
public:
    cLanternUnit* units;   // 0x00
    int num;               // 0x04

    void initLantern(void* arc, void* m1, void* m2, void* m3, void* m4, void* m5);
    static void checkLantern(cLantern* p);
};

struct R20dWork {
    cLantern lantern;      // 0x00
    int nFence;            // 0x08
    cFence fence[3];       // 0x0C
    cObj* crank[3];        // 0x78
    cObj* roundSwitch;     // 0x84
    int rsFlip;            // 0x88
    int x8C;               // 0x8C
    int x90;               // 0x90
    f32 wallY;             // 0x94
};

// One pass-through spot: where the player lands, facing which way, and the camera cut.
struct R20dThroughData {
    Vec pos;       // 0x00
    f32 ang;       // 0x0C
    f32 dist;      // 0x10
    int cut;       // 0x14
};

// The work pointer is a struct member: every store through the work reloads it.
struct R20dWorkPtr {
    R20dWork* p;
};

static R20dWorkPtr r20d_work;

static R20dFenceData r20d_fenceData[3] = {
    {0x18, 3000.0f, 300.0f, {0.0f, 2000.0f, 0.0f}},
    {0x29, 300.0f, 3000.0f, {0.0f, 2000.0f, 0.0f}},
    {0x2A, 300.0f, 2100.0f, {0.0f, 0.0f, 2000.0f}},
};

static const R20dThroughData r20d_throughData[10] = {
    {{-4491.0f, 0.0f, 8557.0f}, -1.5707964f, 1500.0f, 0x3F},
    {{-6274.0f, 0.0f, 8557.0f}, 1.5707964f, 1500.0f, 0x38},
    {{-3539.0f, 0.0f, 27204.0f}, -1.5707964f, 1500.0f, -1},
    {{-5201.0f, 0.0f, 27447.0f}, 1.5707964f, 1500.0f, -1},
    {{-6273.0f, 0.0f, 13093.0f}, 1.5707964f, 1500.0f, 0x3E},
    {{-4436.0f, 0.0f, 13093.0f}, -1.5707964f, 1500.0f, 0x3D},
    {{-10338.0f, 0.0f, 13459.0f}, 0.0f, 1500.0f, -1},
    {{-10290.0f, 0.0f, 15281.0f}, 3.1415927f, 1500.0f, -1},
    {{-10165.0f, 0.0f, 10512.0f}, 0.0f, 1500.0f, -1},
    {{-10242.0f, 0.0f, 12142.0f}, 3.1415927f, 1500.0f, -1},
};

static inline void ObjPSet(cObj*& d, cObj* v) { d = v; }
struct PlPtr { cPlayer* p; };
#define pPLS (((PlPtr*) &pPL)->p)
static inline void AtariFlagsAnd(cAtariInfo* a, u16 mask) { a->m_flag &= mask; }
static inline void AtariFlagsOr(cAtariInfo* a, u16 bit) { a->m_flag |= bit; }

static void r20d_checkBgmPlay();
void r20d_openShelf_main(int no, int opened);
static void r20d_openedShelf(int no);
static void r20d_openShelf(int no);
void r20d_openDrawer_main(int no, int opened);
static void r20d_openedDrawer(int no);
static void r20d_openDrawer(int no);
static void r20d_checkDoor();
static void r20d_setEm();
static void r20d_checkSwitch(int opened);
void r20d_initCrank();
static void r20d_operateCrank(int no);
static void r20d_execFlagOn();
void r20d_initFence();
static void r20d_execDeathTrap();
static void r20d_moveWall();
void r20d_checkPictureCombination();
static void r20d_checkSalazarCrestUse();
static void r20d_execRoundSwitch();
void r20d_initRoundSwitch();
static void r20d_execThrough(int no);

// Room init (Ashley's section, pl_type 1): the throwable lanterns from the room archive, the three
// fences and their cranks, the enemy clean-up, the round switch (opened / not per Room_flg bit 0), the
// ten pass-through areas (6..9, 2..5, 0x1C/0x1D), area 0x10 = the exit door check; the round switch and
// picture wall; one shelf and three drawer item events; the battle stream; action colour on 0x1B.
void R20dInit()
{
#line 55 "D:/Bio4/Prog/r20d.cpp"
    r20d_work.p = (R20dWork*) MEM_CALLOC(sizeof(R20dWork), 1, 0xd);
    if (pG->pl_type == 1) {
        r20d_work.p->lantern.initLantern(ROOM_ARC_PTR(pG->pRoom, 0xE), ROOM_ARC_PTR(pG->pRoom, 0x3D),
                                         ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x3E),
                                         ROOM_ARC_PTR(pG->pRoom, 0x3F), ROOM_ARC_PTR(pG->pRoom, 0x40));
        r20d_initFence();
        r20d_initCrank();
        SceExec(0x12, (TaskFunc) r20d_setEm, 0, 0, SCE_PRIO_DEF_2, 0);
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            SceExec(0x12, (TaskFunc) r20d_checkSwitch, 0, 0, SCE_PRIO_DEF_2, 0);
        } else {
            SceExec(0x12, (TaskFunc) r20d_checkSwitch, 1, 0, SCE_PRIO_DEF_2, 0);
        }
        SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 0, 1);
        SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 1, 1);
        SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 2, 1);
        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 3, 1);
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 4, 1);
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 5, 1);
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 6, 1);
        SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 7, 1);
        SceAtDataSet_exec(0x1C, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 8, 1);
        SceAtDataSet_exec(0x1D, SCE_LEVEL10, 0, (TaskFunc) r20d_execThrough, (void*) 9, 1);
        SceAtDataSet_exec(0x10, SCE_LEVEL10, 0, (TaskFunc) r20d_checkDoor, 0, 1);
    } else {
        cObj* o = SmdGetObjPtr(0x18);

        if (o) {
            o->be_flag |= 0x20;
            o->pos.y += 2100.0f;
        }
        r20d_initFence();
        r20d_work.p->fence[0].move(1.0f);
        r20d_work.p->fence[1].move(1.0f);
        r20d_work.p->fence[2].move(1.0f);
    }
    r20d_initRoundSwitch();
    SceSetItemEvent(0x14, 0x81, 4, 6, r20d_openShelf, (void (*)()) r20d_openedShelf, 0, 0);
    SceSetItemEvent(0x15, 0x80, 5, 5, r20d_openDrawer, (void (*)()) r20d_openedDrawer, 0, 0);
    SceSetItemEvent(0x16, 0x83, 6, 7, r20d_openDrawer, (void (*)()) r20d_openedDrawer, 1, 0);
    SceSetItemEvent(0x17, 0x86, 7, 8, r20d_openDrawer, (void (*)()) r20d_openedDrawer, 2, 0);
    SceExec(0x12, (TaskFunc) r20d_checkBgmPlay, 0, 0, SCE_PRIO_DEF_2, 0);
    SceAtSetActColor(0x1B, 1);
}

// Per-frame room main: nothing.
void R20dMain()
{
}

// Battle stream 3 from the first Ganado that spots Ashley until none (ids 0x10..0x20) is alive.
static void r20d_checkBgmPlay()
{
    while (SceCkFindPL(0) != 1) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 3, 1);
    while (SceCountEmAlive(0x10, 0x20) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}

// The double-door shelf opens (opened: already open, snap the doors).
void r20d_openShelf_main(int no, int opened)
{
    f32 ang = 0.0f;
    cObj* a = 0;
    cObj* b = 0;

    switch (no) {
    case 0:
        a = SmdGetObjPtr(0x21);
        b = SmdGetObjPtr(0x22);
        ang = -2.22f;
        break;
    default:
        SceExit();
        break;
    }
    if (a != 0 && b != 0) {
        a->be_flag |= 0x20;
        b->be_flag |= 0x20;
        if (opened == 1) {
            a->pParts->ang.y = ang;
            b->pParts->ang.y = -ang;
        } else {
            int i;

            ang /= 30.0f;
            SndCall(6, 0x1A, 0, 0, 0, 0);
            for (i = 30; i != 0; i--) {
                a->pParts->ang.y += ang;
                b->pParts->ang.y -= ang;
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": pose shelf `no` open.
static void r20d_openedShelf(int no)
{
    r20d_openShelf_main(no, 1);
}

// Item-event opener: animate shelf `no` open.
static void r20d_openShelf(int no)
{
    r20d_openShelf_main(no, 0);
}

// Drawer `no` slides out together with the item model in it.
void r20d_openDrawer_main(int no, int opened)
{
    Vec d = {0.0f, 0.0f, 0.0f};
    cObj* obj = 0;
    cModel* item = 0;

    switch ((u32) no) {
    case 0:
        obj = SmdGetObjPtr(0x23);
        item = SceAtItemModelPtr(0x80);
        d.x = 300.0f;
        break;
    case 1:
        obj = SmdGetObjPtr(0x25);
        item = SceAtItemModelPtr(0x83);
        d.x = -400.0f;
        break;
    case 2:
        obj = SmdGetObjPtr(0x28);
        item = SceAtItemModelPtr(0x86);
        d.z = 300.0f;
        break;
    default:
        SceExit();
        break;
    }
    if (obj != 0) {
        obj->be_flag |= 0x20;
        if (opened == 1) {
            PSVECAdd(&obj->pos, &d, &obj->pos);
            if (item != 0) {
                PSVECAdd(&item->pos, &d, &item->pos);
            }
        } else {
            Vec step;
            int i;

            PSVECScale(&d, &step, 0.033333335f);
            SndCall(6, 0x1B, 0, 0, 0, 0);
            for (i = 30; i != 0; i--) {
                PSVECAdd(&obj->pos, &step, &obj->pos);
                if (item != 0) {
                    PSVECAdd(&item->pos, &step, &item->pos);
                }
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": pose drawer `no` slid out.
static void r20d_openedDrawer(int no)
{
    r20d_openDrawer_main(no, 1);
}

// Item-event opener: animate drawer `no` sliding out.
static void r20d_openDrawer(int no)
{
    r20d_openDrawer_main(no, 0);
}

// Area 0x10, the exit door: once unlocked (Key_flg[0] 0x00200000) sets Scenario_flg[1] 0x02000000
// and switches control back to Leon (PlSelect(0)) before running the door area.
static void r20d_checkDoor()
{
    if (!(pG->Key_flg[0] & 0x00200000)) {
        SceAtExecute(0x10);
    } else {
        ScfFlagOn(pG, SCF_R20D_END_OF_ASHLEY_PLAY);
        PlSelect(0);
        SceAtExecute(0x10);
    }
}

// One frame in: if the key item (Item_flg[0] 0x4000) was already taken, remove every Ganado (0x10..0x20).
static void r20d_setEm()
{
    SceSleep(1);
    if (ItfFlagChk(pG, ITF_R20E_SALAZAR_CREST)) {
        SceDestroyEm(0x10, 0x20);
        SceExit();
    }
}

// The switch that raises / lowers fence 0. The three 0.0 loads (the `t != 0.0` compare, `spd = 0.0`,
// the tail `move(0.0f)`) share one high (`lis r29`, callee-saved) only when every occurrence is
// inside the outer loop: gcse PRE then inserts the single `high(LC)` at the end of the preheader
// and all three loads become redundant copies of it (a `z0 = 0.0f` before the loop is its own
// non-redundant occurrence and PRE re-inserts a second high for the other two). loop.c hoists the
// compare's load to the preheader (`lfs f27`); the other two stay in their blocks.
static void r20d_checkSwitch(int opened)
{
    cEm* sw;
    int open;
    f32 zero;

    getRoomEtcSwitch(0xF, &sw, 1);
    if (sw == 0) {
        SceExit();
    }
    if (opened == 0) {
        open = 0;
        ((cEmSwitch*) sw)->setClosed();
    } else {
        open = 1;
        ((cEmSwitch*) sw)->setOpened();
    }
    for (;;) {
        f32 t = r20d_work.p->fence[0].t;
        f32 z0 = 0.0f;

        if (open == 0) {
            if (t != z0) {
                open = 1;
                ((cEmSwitch*) sw)->setOpen();
                if (pG->Room_flg[0] & 0x40000000) {
                    continue;
                }
            }
            if (((cEmSwitch*) sw)->ckOpen() == 1) {
                SndCall(6, 0x24, 0, 0, 0, 0);
                open = 1;
                for (;;) {
                    t += 0.02f;
                    r20d_work.p->fence[0].move(t);
                    if (t >= 1.0f) {
                        SndCall(6, 0x25, 0, 0, 0, 0);
                        SceAtSetEnable(0, 0);
                        r20d_work.p->fence[0].move(1.0f);
                        goto sleep;
                    }
                    if (((cEmSwitch*) sw)->ckOpen() == 0) {
                        goto sleep;
                    }
                    SceSleep(1);
                }
            }
        } else {
            if (((cEmSwitch*) sw)->ckOpen() == 0) {
                f32 spd;

                SndCall(6, 0x26, 0, 0, 0, 0);
                spd = 0.0f;
                open = 0;
                zero = spd;
                SceAtSetEnable(0, 1);
                for (;;) {
                    r20d_work.p->fence[0].move(t);
                    t -= spd;
                    spd += 0.005f;
                    if (!(t < zero)) {
                        if (((cEmSwitch*) sw)->ckOpen() == 1) {
                            goto sleep;
                        }
                        SceSleep(1);
                    } else {
                        break;
                    }
                }
                SndCall(6, 0x27, 0, 0, 0, 0);
                r20d_work.p->fence[0].move(0.0f);
            }
        }
    sleep:
        SceSleep(1);
    }
}

// The three crank objects at their fixed spots; each fence not yet raised (Room_flg bits 0/1/8) gets its
// crank area (0/1/0xE) as the operate task, else the fence is posed raised.
void r20d_initCrank()
{
    Vec rot = {0.0f, 1.5707964f, 0.0f};
    Vec p0 = {-3147.0f, 1000.0f, 9699.0f};
    Vec p1 = {-1630.0f, 1000.0f, 18360.0f};
    Vec p2 = {-1630.0f, 1000.0f, 24914.0f};

    ObjPSet(r20d_work.p->crank[0], SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x29), ROOM_ARC_PTR(pG->pRoom, 0x2A), &p0, &rot, 0x10, 1));
    ObjPSet(r20d_work.p->crank[1], SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x29), ROOM_ARC_PTR(pG->pRoom, 0x2A), &p1, &rot, 0x10, 1));
    ObjPSet(r20d_work.p->crank[2], SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x29), ROOM_ARC_PTR(pG->pRoom, 0x2A), &p2, &rot, 0x10, 1));
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r20d_operateCrank, (void*) 0, 1);
    } else {
        r20d_work.p->fence[0].move(1.0f);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r20d_operateCrank, (void*) 1, 1);
    } else {
        r20d_work.p->fence[1].move(1.0f);
    }
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        SceAtDataSet_exec(0xE, SCE_LEVEL10, 0, (TaskFunc) r20d_operateCrank, (void*) 2, 1);
    } else {
        r20d_work.p->fence[2].move(1.0f);
    }
}

// The player turns crank `no`: fence `no` rises while the button is held.
static void r20d_operateCrank(int no)
{
    cObj* crank = 0;
    int idx = 0;
    f32 t;
    int spd;
    int lastMot;
    int accel;
    u32 seId;

    pG->Room_flg[0] |= 0x40000000;
    switch ((u32) no) {
    case 0:
        SceAtSetEnable(0, 0);
        crank = r20d_work.p->crank[0];
        CamCtrl.CutCall(2);
        break;
    case 1:
        SceAtSetEnable(1, 0);
        idx = 1;
        crank = r20d_work.p->crank[1];
        CamCtrl.CutCall(3);
        break;
    case 2:
        SceAtSetEnable(0xE, 0);
        crank = r20d_work.p->crank[2];
        idx = 2;
        CamCtrl.CutCall(0xE);
        break;
    }
    // `idx*36 + work + 0x2c` (mult first): the target adds the scaled index to the work pointer and
    // keeps 0x2c as the displacement (`add r11,r28,r11; lfs f31,0x2c(r11)`); the zero-inits of the
    // loop state follow the switch (issued in the block before the beginEvent calls).
    t = *(f32*) (idx * sizeof(cFence) + (u32) r20d_work.p + 0x2c);
    spd = 0;
    lastMot = 0;
    accel = 0;
    seId = 0;
    ((cUnitEventView*) pPL)->beginEvent(0);
    ((cUnitEventView*) crank)->beginEvent(0);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x34), 3, 0, 5, (int) ROOM_ARC_PTR(pG->pRoom, 0x35));
    crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2B), 3, 0, 5, (int) ROOM_ARC_PTR(pG->pRoom, 0x2C));
    {
        Vec v = {427.81f, 0.0f, -563.42f};
        cPlayer* pl;
        Vec* pr;

        PSMTXMultVec(crank->mat, &v, &v);
        v.y = pPL->pos.y;
        FSet(pPL->ang.y, crank->ang.y - 1.5707964f);
        pl = pPL;
        pr = &pl->ang;
        pl->setPos(&v);
        pl->setAng(pr);
    }
    while ((PlGetStatus() & 0x00020000) && !(Key.trg & 0x40000000)) {
        int mot;

        accel++;
        if (accel > 8) {
            accel = 8;
            spd -= 5;
            if (spd < 0) {
                spd = 0;
            }
        }
        mot = spd / 20;
        if (mot > 7) {
            mot = 7;
        }
        if (mot != lastMot) {
            void* m0;
            void* m1;
            u32 n;
            u32 frame;
            f32 rate;

            lastMot = mot;
            switch (mot) {
            default:
            case 0:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x35);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2C);
                break;
            case 1:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x36);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2D);
                break;
            case 2:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x37);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2E);
                break;
            case 3:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x38);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2F);
                break;
            case 4:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x39);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x30);
                break;
            case 5:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x3A);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x31);
                break;
            case 6:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x3B);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x32);
                break;
            case 7:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x3C);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x33);
                break;
            }
            n = *(u16*) m0;
            rate = pPL->frame / (f32) pPL->frameMax;
            frame = (u32) ((f32) n * rate);
            frame++;
            if (frame >= n) {
                frame = 0;
            }
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x34), 3, (u16) frame, 5, (int) m0);
            crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2B), 3, (u16) frame, 5, (int) m1);
        }
        if (MotionCheckCrossFrame(&pPL->Motion, 0.0f) == 1) {
            SndCall(6, 0x35, 0, 0, 0, 0);
            seId = SndCall(6, 2, &r20d_work.p->fence[idx].obj->pos, 0, 0, 0);
        }
        if (MotionCheckCrossFrame(&pPL->Motion, 50.0f) == 1) {
            SndCall(6, 0x35, 0, 0, 0, 0);
            seId = SndCall(6, 2, &r20d_work.p->fence[idx].obj->pos, 0, 0, 0);
        }
        if (MotionCheckCrossFrame(&pPL->Motion, 100.0f) == 1) {
            SndCall(6, 0x35, 0, 0, 0, 0);
            seId = SndCall(6, 2, &r20d_work.p->fence[idx].obj->pos, 0, 0, 0);
        }
        t += (f32) (mot + 1) * 0.0005f;
        r20d_work.p->fence[idx].move(t);
        if (t >= 1.0f) {   // `>=` (not `!(t < 1.0f)`): the GE code prints the `cror un,eq,gt; bso` pair
            t = 1.0f;
            r20d_work.p->fence[idx].move(t);
            break;
        }
        if (Key.trg & 0x00080000) {
            spd += accel;
            accel = 0;
            if (spd > 159) {
                spd = 159;
            }
        }
        ActBtn.set(0x2A, 5, 0, 0, 2, 2, 0, 0);
        SceSleep(1);
    }
    FadeSetW(0x80000001, 5, 0, 0);
    ((cUnitEventView*) pPL)->endEvent(0);
    crank->motionPause();
    ((cUnitEventView*) crank)->endEvent(0);
    if (t >= 1.0f) {
        if (seId != 0) {
            SndStop(seId, 0);
        }
        SndCall(6, 3, &r20d_work.p->fence[idx].obj->pos, 0, 0, 0);
    } else {
        switch ((u32) no) {
        case 0:
            SceAtSetEnable(0, 1);
            break;
        case 1:
            SceAtSetEnable(1, 1);
            break;
        case 2:
            SceAtSetEnable(0xE, 1);
            break;
        }
    }
    pG->Room_flg[0] &= ~0x40000000;
    CamCtrl.Comeback(0);
}

// Fence at `t` of its way up.
void cFence::move(f32 t)
{
    Vec d = dir;

    PSVECScale(&d, &d, t);
    PSVECAdd(&pos, &d, &d);
    obj->pos = d;
    this->t = t;
    Vec rot = {0.0f, 0.0f, 0.0f};
    sat->setCoord(&obj->pos, &rot);
}

// Bind fence data: the scroll object (script-moved), lowered position, raise vector, and a 4-corner
// collision piece (SAT) around it sized from the data's w/d.
void cFence::init(R20dFenceData* d)
{
    f32 hz;
    f32 hx;

    obj = SmdGetObjPtr(d->objNo);
    obj->be_flag |= 0x20;
    pos = obj->pos;
    dir.x = d->dir.x;
    dir.y = d->dir.y;
    dir.z = d->dir.z;
    t = 0.0f;
    hx = d->w * 0.5f + 100.0f;
    hz = d->d * 0.5f + 100.0f;
    Vec rot = {0.0f, 0.0f, 0.0f};
    Vec v[4] = {{-hx, -1100.0f, -hz}, {hx, -1100.0f, -hz}, {hx, -1100.0f, hz}, {-hx, -1100.0f, hz}};
    sat = SatMgr.create(&obj->pos, &rot, v, 0x40, 0, 4100.0f);
}

// Area 0x19: marks all three fences raised (Room_flg bits 0/1/8) — the exit shortcut after the puzzle.
static void r20d_execFlagOn()
{
    RsfSet(G_ROOM_ID, 0);
    RsfSet(G_ROOM_ID, 1);
    RsfSet(G_ROOM_ID, 8);
}

// Initialise the three fences from r20d_fenceData and arm the area-0x19 flag setter.
void r20d_initFence()
{
    u32 i;

    r20d_work.p->nFence = 3;
    for (i = 0; i < r20d_work.p->nFence; i++) {
        r20d_work.p->fence[i].init(&r20d_fenceData[i]);
    }
    SceAtDataSet_exec(0x19, SCE_LEVEL10, 0, (TaskFunc) r20d_execFlagOn, 0, 1);
}

// Death trap area: Ashley dies (death demo 0).
static void r20d_execDeathTrap()
{
    DiedemoExec(0, 0);
}

// The wall of the picture puzzle rises.
static void r20d_moveWall()
{
    Vec p;
    cPlayer* pl;
    cObj* wall;
    u32 i;
    f32 spd;

    SceEventStart(0);
    pPL->setNoSuspend(1);
    p.x = 4161.0f;
    p.y = -1000.0f;
    p.z = 13197.0f;
    pl = pPL;
    f32 ry = -1.388f;
    pl->setPos(&p);
    p.x = 0.0f;
    p.y = ry;
    p.z = 0.0f;
    pl->setAng(&p);
    CamCtrl.CutCall(0xC);
    wall = SmdGetObjPtr(0x17);
    wall->be_flag |= 0x20;
    const f32 n = 90.0f;
    spd = (4000.0f - wall->pos.y) / n;
    SndCall(6, 8, 0, 0, 0, 0);
    // `i = 0` right after the call: the loop label then follows an insn, not the call, so flow adds no
    // `(use 0)` nop (weight 0) that would take the issue slot beside `bl SceEventStart` from `li i,0`.
    i = 0;
    do {
        wall->pos.y += spd;
        i++;
        SceSleep(1);
    } while ((f32) i < n);
    SndCall(6, 9, 0, 0, 0, 0);
    wall->pos.y = 4000.0f;
    SceAtSetEnable(0x11, 0);
    SceAtSetEnable(0x18, 0);
    CamCtrl.Comeback(0);
    pPL->setNoSuspend(0);
    SceEventEnd(0);
}

// Never-called debug helper of the original object: the original REL link dead-stripped its body and
// kept its constant pool (1.0, the signed int->float double, 120, PI/180, 2000, -1, PI/135) right after
// r20d_moveWall's pool; the unit is in modules.py STRIP_UNUSED so ours drops the body too.
static void r20d_dbgWall(int frame)
{
    f32 rate = 1.0f;
    f32 t = (f32) frame;
    f32 ang = t / 120.0f * 0.017453292f;
    f32 y = 2000.0f * ang;

    if (y < -1.0f) {
        rate = 0.023271058f;
    }
    pPL->pos.y = y * rate;
}

// The round switch put the pictures right: the wall rises, Room_flg bit 2, Scenario_flg[3] 4 (the way
// on opens), the switch area 0xD off.
void r20d_checkPictureCombination()
{
    SceExec(0x12, (TaskFunc) r20d_moveWall, 0, 0, SCE_PRIO_DEF_2, 0);
    RsfSet(G_ROOM_ID, 2);
    ScfFlagOn(pG, SCF_7d);
    SceAtSetEnable(0xD, 0);
}

// Task: waits for the Salazar crest (item 0xF) to be used at the switch, then Room_flg bit 3, camera
// cut 0x3C, SE, the crest model (item area 0x85) shown, message 2.
static void r20d_checkSalazarCrestUse()
{
    while (ItemMgr.check(0xF) != 1) {
        SceSleep(1);
    }
    SceEventStart(0);
    RsfSet(G_ROOM_ID, 3);
    CamCtrl.CutCall(0x3C);
    SceSleep(20);
    SndCall(6, 4, 0, 0, 0, 0);
    SceAtSetEnable(0x85, 1);
    SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    SceEventEnd(0);
}

// The player turns the round switch: the two pictures swap.
static void r20d_execRoundSwitch()
{
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceUpCut(1, 0x3C, -1, UP_CUT_ATTR_CUT_FIX);
        if (ItemMgr.num(0xF) == 0) {
            CamCtrl.Comeback(0);
        } else {
            SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
        }
        SceExit();
    }
    SceMesSet(0, 0x200, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
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
    {
        f32 ang;
        cPlayer* pl;

        SceEventStart(0);
        pPL->setNoSuspend(1);
        Vec d = {287.49002f, 0.0f, -544.75f};
        SndCall(6, 5, 0, 0, 0, 0);
        if (r20d_work.p->rsFlip == 0) {
            ang = -1.5707964f;
        } else {
            d.x = -d.x;
            d.z = -d.z;
            ang = 1.5707964f;
        }
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 0, 0, 1, 0);
        r20d_work.p->roundSwitch->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 0, 0, 1, 0);
        r20d_work.p->rsFlip ^= 1;
        CamCtrl.CutCall(4);
        FSet(pPL->pos.x, r20d_work.p->roundSwitch->pos.x + d.x);
        FSet(pPL->pos.z, r20d_work.p->roundSwitch->pos.z + d.z);
        FSet(pPL->ang.y, ang);
        FSet(r20d_work.p->roundSwitch->ang.y, ang);
        pl = pPL;
        pl->setPos(&pl->pos);
        pl->setAng(&pl->ang);
        asm("" : : "r"(pl)); // COMPILER-DIFF: 12 (regmove operand pick): `pl` must not die at the
                             // `addi r4,pl,0xa0` argument insn, or regmove rewrites it as `addi r4,r3`
                             // after the `mr r3,pl` copy; the codeless use after the call keeps the death there.
        SceSleep(30);
        while (MotionGetState(pPL) != 4) {
            SceSleep(1);
        }
        pPL->setNoSuspend(0);
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        r20d_checkPictureCombination();
    }
}

// The round switch object and its state; the crest slot model (item area 0x85) shown only once the
// crest is in (Room_flg bit 3); in Ashley's section area 0xD = the switch until the puzzle is solved
// (bit 2), with the crest-use watcher.
void r20d_initRoundSwitch()
{
    Vec pos = {3926.0f, 0.0f, 13987.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cModel* m;

    r20d_work.p->roundSwitch = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24), &pos, &rot, 0x10, 1);
    r20d_work.p->rsFlip = 0;
    r20d_work.p->x8C = 0;
    r20d_work.p->x90 = 0;
    r20d_work.p->wallY = SmdGetObjPtr(0x19)->pos.y;
    SceAtSetEnable(0x12, 0);
    SceAtSetEnable(0x85, 1);
    m = SceAtItemModelPtr(0x85);
    if (m) {
        m->setNoSuspend(1);
        m->LightInfo.EnableMask = (m->LightInfo.EnableMask & ~0x20) | 0x10;
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtSetEnable(0x85, 0);
    }
    if (pG->pl_type == 1) {
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            SceAtDataSet_exec(0xD, SCE_LEVEL10, 0, (TaskFunc) r20d_execRoundSwitch, 0, 1);
            SceExec(0x12, (TaskFunc) r20d_checkSalazarCrestUse, 0, 0, SCE_PRIO_DEF_2, 0);
            SceAtDataSet_exec(0x13, SCE_LEVEL10, 0, (TaskFunc) r20d_execDeathTrap, 0, 1);
            SceAtSetEnable(0x13, 0);
        } else {
            SmdGetObjPtr(0x17)->pos.y = 4000.0f;
            SmdGetObjPtr(0x17)->be_flag |= 0x20;
            SceAtSetEnable(0x11, 0);
            SceAtSetEnable(0x18, 0);
        }
    } else {
        SmdGetObjPtr(0x17)->pos.y = 4000.0f;
        SmdGetObjPtr(0x17)->be_flag |= 0x20;
        SceAtSetEnable(0x11, 0);
    }
}

// The player squeezes through spot `no`: walk to the spot, the pass-through motion, walk on.
static void r20d_execThrough(int no)
{
    cPlayer* pl = pPL;
    const R20dThroughData* d;
    Vec step;
    f32 da;
    u32 i;
    f32 dist;
    const f32 frame = 10.0f;   // pool order: 10 before 0.1/PI/0.0

    pl->beginAction();
    // pPLS (struct view) on both sides of the `sth atari.flags` store: cse1 then invalidates the first
    // pPL load and setPriority reloads pPL (target: `lwz r3,pPL@l; addi r3,r3,0x2b4`).
    AtariFlagsAnd(&pPLS->atari, 0xFEFF);
    pPLS->atari.setPriority(PRI_LV1);
    pPL->dmg.set(0, 0x80);
    d = &r20d_throughData[no];
    if (d->cut >= 0) {
        CamCtrl.CutCall((s8) d->cut);
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x20), 0xA, 0, 0x201, 0);
    PSVECSubtract(&d->pos, &pPL->pos, &step);
    PSVECScale(&step, &step, 0.1f);
    da = Muku2(pPL->ang.y, d->ang, 3.1415927f) * 0.1f;
    // `a` is block-local in the loop and in the block after it (same frame slot 0x18); the target
    // computes `addi r4,r1,0x18` in both blocks and keeps the rot.y value in a callee-saved FPR across
    // setPos (FAdd = reference store, so the pPL reload and the separate rot.y load follow it).
    for (i = 0; i < 10; i++) {
        cPlayer* p;
        f32 ry;
        Vec a;

        PSVECAdd(&pPL->pos, &step, &pPL->pos);
        FAdd(pPL->ang.y, da);
        p = pPL;
        ry = p->ang.y;
        p->setPos(&p->pos);
        a.y = ry;
        a.x = 0.0f;
        a.z = 0.0f;
        p->setAng(&a);
        // COMPILER-DIFF: 3 (frame-address PRE): the r31 clobber kills gcse's transparency for
        // `(plus fp 0x18)` in the loop body, so `&a` is not PRE'd into a callee-saved register.
        // The operand must be `p`: the asm's anti-dependence on `mr r3,p` gives that copy the same
        // dependent count as `addi r4,r1,0x18` (both 4), and sched2's LUID tie-break issues the
        // copy first like the target; with `d` as the operand the addi had one dependent more.
        asm("" : "=r"(p) : "0"(p) : "r31");
        SceSleep(1);
    }
    {
        cPlayer* p = pPL;
        Vec a;
        f32 ry = d->ang;

        p->setPos((Vec*) &d->pos);
        a.y = ry;
        a.x = 0.0f;
        a.z = 0.0f;
        p->setAng(&a);
    }
    while (MotionGetState(pPL) != 4) {
        SceSleep(1);
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 5, 0, 0x205, 0);
    dist = d->dist * d->dist;
    while (1) {   // `while (1)` (not `for (;;)`): jump1 lays the SceSleep out at the loop bottom, exit by `bgt`
        if (MotionCheckCrossFrame(&pPL->Motion, 0.0f) == 1) {
            SndCall(6, 0xE, 0, 0, 0, 0);
        }
        if (MotionCheckCrossFrame(&pPL->Motion, frame) == 1) {
            SndCall(6, 0xD, 0, 0, 0, 0);
        }
        if (PSVECSquareDistance(&d->pos, &pPL->pos) > dist) {
            break;
        }
        SceSleep(1);
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 0xA, 0, 0x201, 0);
    while (MotionGetState(pPL) != 4) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    pl->endAction(8);
    pPL->dmg.clear();
    AtariFlagsOr(&pPLS->atari, 0x100);
    pPLS->atari.setPriority(0);
}

// Every torch / lamp of the room (etc types 0xB and 0x10) becomes a lantern unit.
void cLantern::initLantern(void* arc, void* m1, void* m2, void* m3, void* m4, void* m5)
{
    cEm* em;
    u32 i;

    num = 0;
    for (i = 0; i < 64; i++) {
        if (getRoomEtc(i, 0xB, &em, 0) == 1 || getRoomEtc(i, 0x10, &em, 0) == 1) {
            num++;
        }
    }
#line 1270 "D:/Bio4/Prog/r20d.cpp"
    units = (cLanternUnit*) MEM_ALLOC(num * sizeof(cLanternUnit), 1, 0xd);
    num = 0;
    for (i = 0; i < 64; i++) {
        if (getRoomEtc(i, 0xB, &em, 0) == 1 || getRoomEtc(i, 0x10, &em, 0) == 1) {
            units[num].em = em;
            units[num].active = 1;
            units[num].state = 0;
            units[num].step = 0;
            units[num].mot[0] = arc;
            units[num].mot[1] = m1;
            units[num].mot[2] = m2;
            units[num].mot[3] = m3;
            units[num].mot[4] = m4;
            units[num].mot[5] = m5;
            num++;
        }
    }
    SceExec(0x12, (TaskFunc) cLantern::checkLantern, (int) this, 0, SCE_PRIO_DEF_2, 0);
}

// Task over all lantern units: state 0 checks the pickup prompt, 1 is being thrown, 2 destroys the unit.
void cLantern::checkLantern(cLantern* p)
{
    for (;;) {
        u32 i;

        for (i = 0; i < p->num; i++) {
            cLanternUnit* u = (cLanternUnit*) (i * sizeof(cLanternUnit) + (u32) p->units);

            if (u->active != 0) {
                switch (u->state) {
                case 0:
                    u->check();
                    break;
                case 1:
                    break;
                case 2:
                    u->destroy();
                    break;
                }
            }
        }
        SceSleep(1);
    }
}

// Remove the lantern enemy and deactivate the unit.
void cLanternUnit::destroy()
{
    if (em) {
        EmMgr.destroy(em);
    }
    em = 0;
    active = 0;
}

// Prompt the throw when the player stands next to the lantern.
void cLanternUnit::check()
{
    Vec a = em->pos;
    Vec b = pPL->pos;
    f32 d;

    a.y += 200.0f;
    if (b.y >= a.y) {
        return;
    }
    if (b.y < a.y - 2000.0f) {
        return;
    }
    const f32 lim = 1000000.0f;   // declared before the call: its `lis` is hoisted above it (callee-saved r30)
    b.y = a.y;
    d = PSVECSquareDistance(&b, &a);
    if (!(d < lim)) {
        return;
    }
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) != 0) {
        return;
    }
    ActBtn.set(0x17, 5, (int) cLanternUnit::throwLantern, (int) this, 0, 1, 1, 0);
}

// The enemy the lantern flies at (NULL: 10000 units in front of the player, out = that point).
cEm* cLanternUnit::getTargetPos(Vec* out)
{
    Vec p = pPL->pos;
    cModel* t;

    p.y += 1800.0f;
    t = SearchTargetEm(&p, 0, 400000000.0f);
    if (t != 0) {
        *out = t->pos;
        return (cEm*) t;
    }
    {
        Vec rot = {0.0f, 0.0f, 0.0f};
        Vec fwd = {0.0f, 0.0f, 10000.0f};
        Mtx m;

        rot.y = LIMIT_ANGLE(pPL->ang.y + GetXZAngleLocal(&pPL->pos, &em->pos, pPL->ang.y) + 3.1415927f);
        low_RotMatrix(m, &rot);
        TransMatrix(m, &pPL->pos);
        PSMTXMultVec(m, &fwd, out);
    }
    return 0;
}

// The throw action: turn to the lantern, pick it up, turn to the target, throw.
void cLanternUnit::throwLantern(cLanternUnit* u)
{
    // Unreferenced: the five pool words {PI, PI/4, PI/2, 7PI/8, PI/8} between getTargetPos's and this
    // function's constants (an unused function-local static const array is emitted before the pool).
    static const f32 angTbl[5] = {3.1415927f, 0.7853982f, 1.5707964f, 2.7488935f, 0.3926991f};
    Vec pos;
    // The original keeps `st` (0) in r29 for the whole head and never folds it: `stw r29,0xc(u)` then
    // `add r29,u,r29; lwz r4,0x18(r29)` (a byte offset into mot[]), with the two stores issued before
    // `lis pPL@ha`. Ours folds a user variable known to be 0 (cse) and lets the fixed-scalar pPL load
    // pass the struct stores; the hard register + launder hide the value, the reference setters order
    // the stores.
    register int st asm("r29"); // COMPILER-DIFF: #12 (user variable constant not folded)
    int cnt = 0;
    // COMPILER-DIFF: #2 (value-carrying FPR pin): the 0.0 pseudo is f30 and its `turn` copy f31 in the
    // target; local-alloc ties them the other way round.
    register f32 zero asm("fr30");
    f32 turn;

    st = 0;
    asm("" : "+r"(st)); // COMPILER-DIFF: #12
    // COMPILER-DIFF: #12 (companion of the `st` launder): the launder gives `li r29,0` one priority level
    // over the `mr r31,r3` parameter copy; the same codeless copy on `u` restores the tie (the copy leads).
    asm("" : "+r"(u));
    IntSet(u->step, st);
    U32Set(u->state, 1);
    ((cUnitEventView*) pPL)->beginEvent(0);
    // pPLS (struct view) for the pPL read that precedes the `sth atari.flags` store: the store then
    // invalidates it in cse1 and `dmg.set` reloads pPL (target: two `lwz pPL@l`).
    AtariFlagsOr(&pPLS->atari, 0x100);
    pPLS->dmg.set(0, 0x80);
    u->target = u->getTargetPos(&pos);
    // `u + st + 0x18` (not `u->mot + st + 4`): the target adds `u` first (`add r29,u,st`).
    pPL->motionSet(*(void**) ((u8*) u + st + 0x18), 0xA, 0, 1, 0);
    u->em->be_flag |= 0x20;
    zero = 0.0f;
    turn = zero;
    Vec pos2 = pos;
    while ((PlGetStatus() & 0x00020000) && MotionGetState(pPL) != 4) {
        if (u->target) {
            pos = u->target->pos;
        }
        switch ((u32) u->step) {   // unsigned: `cmplwi 2; bgt`; `case 4` balances the tree at root 2
        case 0:
            turn = Muku(&pPL->pos, &u->em->pos, pPL->ang.y, 3.1415927f);
            pPL->ang.y = LIMIT_ANGLE(pPL->ang.y + turn);
            u->step++;
        case 1:
            if (MotionCheckCrossFrame(&pPL->Motion, 10.0f) == 1) {
                Vec ofs = {-132.59999f, -245.22f, -174.37f};
                Vec rot = {1.5118269f, -0.35282877f, -1.6762177f};

                u->em->pos = ofs;
                u->em->ang = rot;
                ((cEmTorch*) u->em)->setParent(pPLS, 0xA, 0);
                u->step++;
                cnt = 0;
                f32 a = LIMIT_ANGLE(pPLS->ang.y + zero);
                turn = Muku(&pPL->pos, &pos, a, 3.1415927f) / 15.0f;
                pos2 = pos;
            }
            break;
        case 2:
            pPL->ang.y = LIMIT_ANGLE(pPL->ang.y + turn);
            if (cnt == 15) {
                u->step++;
            }
            cnt++;
            break;
        case 3:
            if (MotionCheckCrossFrame(&pPL->Motion, 33.0f) == 1) {
                u->setThrowLantern(&pos);
                SndCall(1, 0xF, 0, 0, 0, 0);
                SndCall(1, 0x11, 0, 0, 0, 0);
                u->state = 2;
                u->step++;
            }
            break;
        case 4:
            break;
        }
        if (u->em) {
            u->em->matUpdate();
        }
        SceSleep(1);
    }
    pPL->dmg.clear();
    ((cUnitEventView*) pPL)->endEvent(2);
}

// The lantern leaves the hand: an obj01 flies to the target along a parabola.
void cLanternUnit::setThrowLantern(Vec* target)
{
    Vec from;
    Vec rot;
    Vec spd;
    void* tpl;
    void* bin;
    cObj* obj;
    void* zero = NULL;
    const f32 spd0 = 20.0f;   // pool order (20 first) and `lis r25` at the top

    from.x = em->pParts->mat[0][3];
    from.y = em->pParts->mat[1][3];
    from.z = em->pParts->mat[2][3];
    Matrix2AxisAngle(em->pParts->mat, &rot);
    bin = GetEtcAddr(mot[0], "et1000.bin");
    EspGetEfmTplAddr(0xF, &tpl);
    CalcParabolaVector(&spd, &from, target, PSVECDistance(&from, target) / 10.0f + 1.0f);
    obj = SetObj01(bin, tpl, &from, &rot, &spd, spd0, 50.0f, 0xD2, 5);
    Obj01SetEst(obj, 0, 0x10, 3, 1, 1, 0, 0x14, (int) zero, (int) zero);
    EstSet((int) obj, -1, 0, 0, 1, 0, 0, 0, (u32) obj, zero);
}
