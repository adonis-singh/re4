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
#include "em.h"
#include "emrock.h"
#include "emtorch.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "math_sub.h"
#include "route_ck.h"
#include "area.h"
#include "act_btn.h"
#include "read.h"
#include "stage.h"
#include "snd.h"
#include "esp.h"
#include "rnd.h"
#include "atari_init.h"
#include "st_mgr_event.h"

// Room 2-02 (D:/Bio4/Prog/r202.cpp): the four catapults on the castle wall, the crank that raises
// the wall and the cannon on the tower.

// One catapult: the Ganado operating it, the rock it throws and the areas it fires at.
class cCatapult {
public:
    int emNo;          // 0x00  enemy list entry of the operator
    cEmWrap em;        // 0x04
    cEmRock* rock;     // 0x10
    cObj* obj;         // 0x14  the catapult scroll object
    int state;         // 0x18
    int timer;         // 0x1C
    u8 thrown;         // 0x20
    u8 rockSet;        // 0x21
    u8 targetOn;       // 0x22  throw at targetPos instead of the hit area
    u8 pad_23;
    Vec targetPos;     // 0x24
    u8 enable;         // 0x30
    u8 x31;            // 0x31
    s8 area[4];        // 0x32  player areas that trigger a throw
    s8 at[4];          // 0x36  target area per player area (-1: aim at the player)
    u8 nArea;          // 0x3A
    u8 hitIdx;         // 0x3B  area[] index the player is in
    u8 fire;           // 0x3C  the player is in this catapult's area
    u8 fireAll;        // 0x3D
    u8 pad_3E[2];
    f32 speed;         // 0x40
    SCE_TASK* setRockTask;    // 0x44
    SCE_TASK* throwRockTask;  // 0x48

    void move();
    int checkHitArea();
    void setNewArea(int area, int at);
    void setRock();
    void throwRock();
};

// A patrolling Ganado walking between two points.
class cPatrol {
public:
    cEmWrap em;    // 0x00
    u32 cur;       // 0x0C
    Vec pos[2];    // 0x10

    void getNextTarget(Vec* out);
};

struct R202Work {
    cCatapult cat[4];      // 0x000
    int throwWait;         // 0x130
    cObj* ido0;            // 0x134  the wall sections
    cObj* ido1;            // 0x138
    cObj* crank;           // 0x13C
    cObj* ido2;            // 0x140
    cObj* box;             // 0x144
    f32 idoY0;             // 0x148  wall positions when raised
    f32 idoY1;             // 0x14C
    f32 idoY2;             // 0x150
    SCE_TASK* checkTask;    // 0x154
    u8 pad_158[0x180 - 0x158];
    cEmWrap em180;         // 0x180  the Ganado that opens the gate
    cPatrol pat[2];        // 0x18C
    u32 strId;             // 0x1DC
    cSat* sat;             // 0x1E0
};


static R202Work* r202_work;

// File-scope static const aggregates are deferred to the end of .rodata (after the cManager template
// strings) in declaration order.
static const Vec r202_crankPos = {19974.0f, 11555.0f, -21476.0f};
static const Vec r202_rockOfs = {0.0f, 800.0f, -2400.0f};

// One catapult's table entry.
struct R202CatapultData {
    int objNo;
    f32 ang;
    int emNo;
};


static void r202_execShowView_end();
static void r202_execShowView();
static void r202_execEmSet2();
void r202_openBox_main(int no, int opened);
static void r202_openedBox(int no);
static void r202_openBox(int no);
static void r202_checkBgmPlay(int skip);
static void r202_checkEmPatrol(cPatrol* p);
void r202_initEmPatrol();
static void r202_operateCannon();
static void r202_operateCrank();
void r202_changeIdoSmd(int on);
static void r202_waitRockImpact(cEm* rock);
static void r202_CatapultGo_end();
static void r202_CatapultGo();
void r202_initCatapult();
static void r202_checkCatapult();
void r202_destroyCatapult();
void r202_setFireAll();
static void r202_setRock(cCatapult* c);
static void r202_throwRock(cCatapult* c);
void r202_getTargetPos(Vec* out);

// Room init (the castle wall: catapults, the wall crank and the cannon). Before the wall is passed (Room_flg
// bit 1): the four catapults; area 5 = the gate-catapult event until bit 2 (else the wall shown fallen,
// the stream task, the gate Ganado 0x2A alerted); the patrols, the crank, the cannon, the show view and
// second wave areas. Afterwards the cleared layout. Box item events.
void R202Init()
{
#line 58 "D:/Bio4/Prog/r202.cpp"
    r202_work = (R202Work*) MEM_CALLOC(sizeof(R202Work), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        r202_initCatapult();
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r202_CatapultGo, 0, 1);
            r202_changeIdoSmd(0);
        } else {
            r202_changeIdoSmd(1);
            SceExec(0x12, (TaskFunc) r202_checkBgmPlay, 0, 0, SCE_PRIO_DEF_2, 0);
            pG->Room_flg[0] |= 0x80000000;
            r202_work->em180.setPtr(0x2A, 2, 0);
            r202_work->em180.setFlag(1);
        }
    } else {
        r202_changeIdoSmd(1);
    }
    Vec zero = {0.0f, 0.0f, 0.0f};
    r202_work->crank = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x2A), ROOM_ARC_PTR(pG->pRoom, 0x2B), (Vec*) &r202_crankPos,
                                   &zero, 0x10, 1);
    r202_work->ido0 = SmdGetObjPtr(0x33);
    r202_work->ido1 = SmdGetObjPtr(0x34);
    r202_work->ido2 = SmdGetObjPtr(0x68);
    r202_work->ido0->be_flag |= 0x20;
    r202_work->ido1->be_flag |= 0x20;
    r202_work->ido2->be_flag |= 0x20;
    {
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};

        r202_work->sat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 1);
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            r202_work->idoY1 = r202_work->ido1->pos.y;
            r202_work->idoY0 = r202_work->ido0->pos.y;
            r202_work->idoY2 = r202_work->ido2->pos.y;
            r202_work->ido1->pos.y -= 7539.0f;
            r202_work->ido0->pos.y -= 7539.0f;
            r202_work->ido2->pos.y -= 7539.0f;
            pos.y = -7539.0f;
            r202_work->sat->setCoord(&pos, &rot);
            SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r202_operateCrank, 0, 1);
            SceAtSetEnable(0x15, 1);
            SceAtSetEnable(0x16, 0);
            r202_initEmPatrol();
        } else if (RsfCheck(G_ROOM_ID, 1) == 0) {
            SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r202_operateCannon, 0, 1);
            SmdGetObjPtr(0x25)->be_flag |= 2;
            SmdGetObjPtr(0x6A)->be_flag &= ~2;
            SceAtSetEnable(0x15, 0);
            SceAtSetEnable(0x16, 0);
            r202_initEmPatrol();
        } else {
            SceAtSetEnable(3, 0);
            SmdGetObjPtr(0x25)->be_flag &= ~2;
            SmdGetObjPtr(0x6A)->be_flag |= 2;
            SceAtSetEnable(0x11, 0);
            SceAtSetEnable(0x15, 0);
            SceAtSetEnable(0x16, 1);
            if (FlagChk(pG->Em_flg[2], 48) || FlagChk(pG->Em_flg[3], 48) || FlagChk(pG->Em_flg[4], 30)) {
                switch (checkEmListNo(pG->room_id)) {
                case 2:
                    EmListSetAlive(0x30, 0);
                    break;
                case 3:
                    EmListSetAlive(0x30, 0);
                    break;
                case 4:
                    EmListSetAlive(0x1E, 0);
                    break;
                }
            }
        }
    }
    r202_work->box = ObjMgr.create(cObjMgr::ID_SCROLL);
    r202_work->box->pos.x = 20221.0f;
    r202_work->box->pos.y = 0.0f;
    r202_work->box->pos.z = -26500.0f;
    r202_work->box->atari.init(0.0f, 0.0f, 0.0f, 0.0f, 3000.0f, 3000.0f, 50000.0f, 0, 0x18, 0);
    SceSetItemEvent(0x13, 0x81, 4, 6, r202_openBox, r202_openedBox, 0, 0);
    SceSetItemEvent(0x14, 0x83, 5, 7, r202_openBox, r202_openedBox, 1, 0);
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceAtDataSet_exec(0x17, SCE_LEVEL10, 0, (TaskFunc) r202_execEmSet2, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        SceAtDataSet_exec(0x18, SCE_LEVEL10, 0, (TaskFunc) r202_execShowView, 0, 1);
    }
}

// Per-frame room main: nothing.
void R202Main()
{
}

// End of the show view: stream faded (200 frames), camera back, SceEventEnd.
static void r202_execShowView_end()
{
    SndStrReq(r202_work->strId, 4, 200, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Show the wall: camera cut 8 with the stream.
static void r202_execShowView()
{
    RsfSet(G_ROOM_ID, 7);
    SceSetEventCancel(1, (TaskFunc) r202_execShowView_end, 0, -1, 1);
    r202_work->strId = SndStrReq(0, 0x18, 0x80000003, 0, 0, 0.0f);
    SceEventStart(0);
    CamCtrl.CutCall(8);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r202_execShowView_end();
}

// Two more Ganados that already know where the player is.
static void r202_execEmSet2()
{
    RsfSet(G_ROOM_ID, 6);
    cEmWrap em0;
    cEmWrap em1;
    em0.setEm(0x49, 2, 0, 1, 1);
    em1.setEm(0x4A, 2, 0, 1, 1);
    em0.setFindPL();
    em1.setFindPL();
}

// Box `no` opens (opened: already open, snap the lid).
void r202_openBox_main(int no, int opened)
{
    Vec ang = {0.0f, 0.0f, 0.0f};
    cObj* obj = 0;

    switch (no) {
    case 0:
        obj = SmdGetObjPtr(0x7A);
        ang.x = -1.7f;
        break;
    case 1:
        obj = SmdGetObjPtr(0x7C);
        ang.z = 1.7f;
        break;
    default:
        SceExit();
        break;
    }
    if (obj != 0) {
        obj->be_flag |= 0x20;
        if (opened == 1) {
            obj->pList->ang = ang;
        } else {
            Vec step;
            int i;

            PSVECScale(&ang, &step, 0.033333335f);
            SndCall(6, 0x5B, 0, 0, 0, 0);
            for (i = 30; i != 0; i--) {
                if (obj != 0) {
                    PSVECAdd(&obj->pList->ang, &step, &obj->pList->ang);
                }
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": pose box `no` open.
static void r202_openedBox(int no)
{
    r202_openBox_main(no, 1);
}

// Item-event opener: animate box `no` open.
static void r202_openBox(int no)
{
    r202_openBox_main(no, 0);
}

// Battle stream from the first contact until the wall is passed and the Ganados are dead.
static void r202_checkBgmPlay(int skip)
{
    if (skip == 0) {
        while (SceCkFindPL(0) != 1) {
            SceSleep(1);
        }
        SndRoomStrStart(1, 3, 1);
        while (RsfCheck(G_ROOM_ID, 1) == 0) {
            SceSleep(1);
        }
    }
    while (SceCountEmAlive(0x10, 0x20) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}

// Advance to the other of the two patrol points and return it.
void cPatrol::getNextTarget(Vec* out)
{
    cur++;
    if (cur >= 2) {
        cur = 0;
    }
    *out = pos[cur];
}

// Task: walks patrol `p`'s Ganado between its two points (goto mode 6) until the wall is passed (Room_flg
// bit 1), the Ganado dies or it spots the player.
static void r202_checkEmPatrol(cPatrol* p)
{
    Vec t;

    p->getNextTarget(&t);
    p->em.setGoto(&t, 6);
    SceSleep(1);
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 1)) {
            return;
        }
        if (p->em.isAlive() == 1) {
            if (p->em.ckFindPL() == 1) {
                return;
            }
            if (p->em.ckGoto() == 0) {
                p->getNextTarget(&t);
                p->em.setGoto(&t, 6);
            }
        }
        SceSleep(1);
    }
}

// The two patrolling Ganados (0x2C / 0x32, list 2) on the wall walk with their point pairs.
void r202_initEmPatrol()
{
    Vec p0 = {13401.0f, 3073.0f, -22270.0f};
    Vec p1 = {6305.0f, 3073.0f, -22288.0f};
    Vec p2 = {13910.0f, 3073.0f, -23208.0f};
    Vec p3 = {6564.0f, 3073.0f, -23599.0f};

    r202_work->pat[0].em.setEm(0x2C, 2, 0, 1, 1);
    r202_work->pat[0].cur = 1;
    r202_work->pat[0].pos[0] = p0;
    r202_work->pat[0].pos[1] = p1;
    r202_work->pat[1].em.setEm(0x32, 2, 0, 1, 1);
    r202_work->pat[1].cur = 0;
    r202_work->pat[1].pos[0] = p2;
    r202_work->pat[1].pos[1] = p3;
    if (r202_work->pat[0].em.isAlive() == 1) {
        SceExec(0x12, (TaskFunc) r202_checkEmPatrol, (int) &r202_work->pat[0], 0, SCE_PRIO_DEF_2, 0);
    }
    if (r202_work->pat[1].em.isAlive() == 1) {
        SceExec(0x12, (TaskFunc) r202_checkEmPatrol, (int) &r202_work->pat[1], 0, SCE_PRIO_DEF_2, 0);
    }
}

// The cannon shot: the wall falls, the catapults are destroyed and the partner is recovered.
static void r202_operateCannon()
{
    Vec v;
    cEm* torch;
    void* zero;

    SceEventStart(0);
    RsfSet(G_ROOM_ID, 1);
    ScfFlagOn(pG, SCF_70);
    r202_destroyCatapult();
    pPL->setNoSuspend(1);
    if (pSUB != 0) {
        if (!(SubCharGetStatus() & 0x02000000)) {
            pSUB->setNoSuspend(1);
            pSUB->endEvent(0);
        } else {
            pG->Room_flg[0] |= 0x01000000;
            if (RouteCkPosToPosDis(&pPL->pos, &pSUB->pos) > 10000.0f) {
                pG->Room_flg[0] |= 0x00800000;
            } else if (SceAtHitCheck(0x19) == 1) {
                pG->Room_flg[0] |= 0x00400000;
            } else {
                pG->Room_flg[0] |= 0x00200000;
            }
        }
    }
    zero = NULL;
    CamCtrl.CutCall(5);
    SceSleep(20);
    SceAtSetEnable(0x11, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 4, 1, ESP_CORE_KIND_NONE, zero, zero);
    SndCall(6, 9, 0, 0, 0, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, ESP_CORE_KIND_NONE, zero, zero);
    SceSleep(35);
    SndCall(6, 0xA, 0, 0, 0, 0);
    SmdGetObjPtr(0x25)->be_flag &= ~2;
    if (getRoomEtcTorch(8, &torch, 1) != 0) {
        ((cEmTorch*) torch)->setBreak();
    }
    if (getRoomEtcTorch(9, &torch, 1) != 0) {
        ((cEmTorch*) torch)->setBreak();
    }
    SceSleep(10);
    if (!(pG->Room_flg[0] & 0x01000000)) {
        int hit = SceAtHitCheck(0x10);

        if (hit == 1) {
            cSubChar* sub = SUB_CHAR();

            if (sub != 0) {
                v = sub->pos;
                v.z -= 100.0f;
                sub->hp = hit;
                PlWepHitCheck2(0, &v, &v, 0x12, 3, 5000.0f);
                SceSleep(10);
                if (pSUB->hp <= 0) {
                    SceExit();
                }
            }
        }
    }
    SceSleep(60);
    SmdGetObjPtr(0x6A)->be_flag |= 2;
    SceSleep(60);
    SceDestroyEm(0x10, 0x20);
    {
        ReadModule* m = SearchEmModule(0x11);

        if (m) {
            EspDataRelease(EFF_EM10, 0, 1);
            InitModule(m);
        }
    }
    if (pSUB != 0 && (pG->Room_flg[0] & 0x01000000)) {
        EmMgr.destroy(pSUB);
    }
    SceSleep(2);
    setEm(0x30, 2, 0, 1, 1);
    EmListSetAlive(0x30, 1);
    if (pG->Room_flg[0] & 0x01000000) {
        SubCharInit(1, &pPL->pos, pPL->ang.y);
        SubCharCtrl(SCC_CHASE, 0);
    }
    CamCtrl.Comeback(0);
    SceAtSetEnable(3, 0);
    SceAtSetEnable(0x16, 1);
    pPL->setNoSuspend(0);
    if (pSUB != 0) {
        pSUB->setNoSuspend(0);
    }
    SceEventEnd(0);
    {
        cSubChar* sub = SUB_CHAR();

        if (sub != 0) {
            if (pG->Room_flg[0] & 0x01000000) {
                if (pG->Room_flg[0] & 0x00800000) {
                    Vec p;

                    VecSet(&p, -6366.0f, 6600.0f, -39153.0f);
                    sub->setPos(&p);
                } else if (pG->Room_flg[0] & 0x00400000) {
                    sub->setPos(&pPL->pos);
                } else {
                    v.x = 3215.0f;
                    v.y = 9032.0f;
                    v.z = -27553.0f;
                    sub->setPos(&v);
                }
                SubCharCtrl(SCC_CHASE, 0);
            }
        }
    }
    SceSleep(30);
    SceExec(0x12, (TaskFunc) r202_checkBgmPlay, 1, 0, SCE_PRIO_DEF_2, 0);
}

// The player turns the crank: the wall rises while the button is held.
static void r202_operateCrank()
{
    int spd = 0;
    int lastMot = 0;
    int accel = 0;

    SceAtSetEnable(2, 0);
    pPL->beginEvent(0);
    PlSetHand(1, 0);
    r202_work->crank->beginEvent(0);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 3, 0, 5, ROOM_ARC_PTR(pG->pRoom, 0x22));
    r202_work->crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2C), 3, 0, 5, ROOM_ARC_PTR(pG->pRoom, 0x2D));
    CamCtrl.CutCall(3);
    {
        Vec v = {516.5f, 0.0f, -500.0f};
        cPlayer* pl;
        Vec* pr;

        PSMTXMultVec(r202_work->crank->mat, &v, &v);
        v.y = pPL->pos.y;
        pPL->ang.y = r202_work->crank->ang.y - 1.5707964f;
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
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x22);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2D);
                break;
            case 1:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x23);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2E);
                break;
            case 2:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x24);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2F);
                break;
            case 3:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x25);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x30);
                break;
            case 4:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x26);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x31);
                break;
            case 5:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x27);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x32);
                break;
            case 6:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x28);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x33);
                break;
            case 7:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x29);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x34);
                break;
            }
            n = *(u16*) m0;
            rate = pPL->Motion.Seq_frame / (f32) pPL->Motion.Seq_frame_num;
            frame = (u32) ((f32) n * rate);
            frame++;
            if (frame >= n) {
                frame = 0;
            }
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 3, frame, 5, m0);
            r202_work->crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2C), 3, frame, 5, m1);
        }
        if (MotionCheckCrossFrame(&pPL->Motion, 0.0f) == 1) {
            SndCall(6, 0x35, 0, 0, 0, 0);
            SndCall(6, 7, 0, 0, 0, 0);
        }
        if (MotionCheckCrossFrame(&pPL->Motion, 50.0f) == 1) {
            SndCall(6, 0x35, 0, 0, 0, 0);
            SndCall(6, 7, 0, 0, 0, 0);
        }
        if (MotionCheckCrossFrame(&pPL->Motion, 100.0f) == 1) {
            SndCall(6, 0x35, 0, 0, 0, 0);
            SndCall(6, 7, 0, 0, 0, 0);
        }
        {
            f32 dy = (f32) (mot + 1) * 3.0f;

            r202_work->ido1->pos.y += dy;
            r202_work->ido0->pos.y += dy;
            r202_work->ido2->pos.y += dy;
            Vec pos = {0.0f, 0.0f, 0.0f};
            Vec rot = {0.0f, 0.0f, 0.0f};
            pos.y = r202_work->ido1->pos.y - r202_work->idoY1;
            r202_work->sat->setCoord(&pos, &rot);
            if (r202_work->ido1->pos.y > r202_work->idoY1) {
                r202_work->ido1->pos.y = r202_work->idoY1;
                r202_work->ido0->pos.y = r202_work->idoY0;
                r202_work->ido2->pos.y = r202_work->idoY2;
                pos.y = 0.0f;
                r202_work->sat->setCoord(&pos, &rot);
                break;
            }
        }
        if (Key.trg & 0x00080000) {
            spd += accel;
            accel = 0;
            if (spd > 159) {
                spd = 159;
            }
        }
        ActBtn.set(ACT_ROTATE, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
        SceSleep(1);
    }
    r202_work->crank->motionPause();
    PlSetHand(0, 0);
    pPL->endEvent(2);
    r202_work->crank->endEvent(0);
    SceAtSetEnable(0x15, 0);
    if (r202_work->ido1->pos.y == r202_work->idoY1) {
        EstSet(0, -1, 0, 0, EFF_ROOM, 3, 0, ESP_CORE_KIND_NONE, 0, 0);
        RsfSet(G_ROOM_ID, 0);
        SndCall(6, 8, 0, 0, 0, 0);
        SceSleep(30);
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r202_operateCannon, 0, 1);
    } else {
        SceAtSetEnable(2, 1);
    }
    CamCtrl.Comeback(0);
}

// Wall up (on = 1) or down: swap the two scroll model sets.
void r202_changeIdoSmd(int on)
{
    if (on == 1) {
        SmdGetObjPtr(0x2C)->be_flag &= ~2;
        SmdGetObjPtr(0x6E)->be_flag &= ~2;
        SmdGetObjPtr(0x6D)->be_flag |= 2;
        SmdGetObjPtr(0x70)->be_flag |= 2;
    } else {
        SmdGetObjPtr(0x2C)->be_flag |= 2;
        SmdGetObjPtr(0x6E)->be_flag |= 2;
        SmdGetObjPtr(0x6D)->be_flag &= ~2;
        SmdGetObjPtr(0x70)->be_flag &= ~2;
    }
}

// The rock of the gate catapult lands: the wall changes.
static void r202_waitRockImpact(cEm* rock)
{
    while (rock->checkStatus(EM_STATUS_ACTIVE) == 1) {
        SceSleep(1);
    }
    r202_changeIdoSmd(1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 6, 0, ESP_CORE_KIND_NONE, 0, 0);
    SndCall(6, 0xB, 0, 0, 0, 0);
}

// End of the gate-catapult cutscene (also its cancel path, Room_flg[0] 0x02000000 = cancelled): unless
// cancelled the gate Ganado is alerted, catapult 2 set to launch and the rock-impact watcher started;
// the actors may suspend, the catapult task resumes, camera back, SceEventEnd; a second later the
// fixed target is dropped.
static void r202_CatapultGo_end()
{
    if (!(pG->Room_flg[0] & 0x02000000)) {
        cEmRock* rock = r202_work->cat[2].rock;

        r202_work->em180.setFlag(1);
        r202_work->cat[2].state = 5;
        SceExec(0x12, (TaskFunc) r202_waitRockImpact, (int) rock, 4, SCE_PRIO_DEF_2, 0);
    }
    r202_work->em180.setNoSuspend(0);
    r202_work->cat[2].em.setNoSuspend(0);
    r202_work->cat[2].rock->setNoSuspend(0);
    r202_work->checkTask->setKind(r202_work->checkTask->getKind() & ~2);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceSleep(60);
    r202_work->cat[2].targetOn = 0;
}

// The gate event: catapult 2 fires at the wall.
static void r202_CatapultGo()
{
    void* zero = NULL;
    cEmRock* rock;

    RsfSet(G_ROOM_ID, 2);
    SceSetEventCancel(1, (TaskFunc) r202_CatapultGo_end, 0, -1, 1);
    r202_work->em180.setPtr(0x2A, 2, 0);
    SceEventStart(1);
    CamCtrl.CutCall(4);
    SndRoomStrStart(1, 0, 1);
    r202_work->em180.setNoSuspend(1);
    r202_work->cat[2].em.setNoSuspend(1);
    r202_work->cat[2].rock->setNoSuspend(1);
    r202_work->checkTask->setNoSuspend(1);
    r202_work->cat[2].targetPos.x = 21929.0f;
    r202_work->cat[2].targetPos.y = 5598.0f;
    r202_work->cat[2].targetPos.z = -37644.0f;
    r202_work->cat[2].targetOn = 1;
    r202_work->cat[2].fire = 1;
    pG->Room_flg[0] |= 0x80000000;
    rock = r202_work->cat[2].rock;
    EstSet(rock, -1, 0, 0, EFF_ROOM, 0, 1, EMROCK_WK(rock)->espKind, rock, zero);
    SceSleep(60);
    pG->Room_flg[0] |= 0x02000000;
    r202_work->em180.setFlag(1);
    r202_work->cat[2].state = 5;
    SceExec(0x12, (TaskFunc) r202_waitRockImpact, (int) rock, 0, SCE_PRIO_DEF_2, 0);
    SceSleep(60);
    SceSetEventCancel(0, 0, 0, -1, 1);
    r202_CatapultGo_end();
}

// The four catapults from the table (object, yaw, operator list entry): state 0, fire areas per
// catapult (player area -> target area), catapult 2 aimed at the wall; then the per-frame task.
void r202_initCatapult()
{
    R202CatapultData tbl[4] = {
        {0x40, -0.2268928f, 0x2F},
        {0x41, 0.13212143f, 0x2B},
        {0x42, 1.5837117f, 0x2E},
        {0x43, 3.3759902f, 0x21},
    };
    u32 i;
    // The loop's zero is one pseudo (set inside the body, hoisted by loop.c) that the after-loop
    // `cat[2].timer = zero` store reuses (`stw r28,180`).
    int zero;

    for (i = 0; i < 4; i++) {
        r202_work->cat[i].enable = 1;
        r202_work->cat[i].x31 = 1;
        r202_work->cat[i].obj = SmdGetObjPtr(tbl[i].objNo);
        r202_work->cat[i].obj->be_flag |= 0x20;
        r202_work->cat[i].obj->ang.y = LIMIT_ANGLE(tbl[i].ang);
        zero = 0;
        r202_work->cat[i].state = zero;
        r202_work->cat[i].timer = zero;
        r202_work->cat[i].thrown = zero;
        r202_work->cat[i].nArea = zero;
        r202_work->cat[i].speed = 3500.0f;
        r202_work->cat[i].emNo = tbl[i].emNo;
        r202_work->cat[i].setRockTask = 0;
        r202_work->cat[i].throwRockTask = 0;
    }
    // COMPILER-DIFF: #3 (dead test as a gcse block boundary). The after-loop `lwz work` x9 read the PRE
    // reaching register (`lis r27` in bb 0) directly in the original; ours re-materialises the copy (cse2,
    // `high` cost 0). With `&r202_work` computed here and the dead `if` ending the block, cse1 rewrites
    // the following highs to this block's pseudo, PRE turns it into `P = R`, cprop pass 2 propagates R into
    // the later block, and cse2 folds `i <= 3` from the loop exit's `ble` (the block, the pointer and the
    // rematerialised `lis` all die before scheduling).
    R202Work** wp = &r202_work;
    if (i <= 3) {
        *wp = 0;
    }
    r202_work->cat[1].setNewArea(6, 7);
    r202_work->cat[2].setNewArea(6, 0xE);
    r202_work->cat[3].setNewArea(6, 0xF);
    r202_work->cat[3].setNewArea(8, 9);
    r202_work->cat[1].setNewArea(0xA, 0xB);
    r202_work->cat[0].setNewArea(0xA, 0x12);
    r202_work->cat[0].setNewArea(0xC, -1);
    r202_work->cat[0].speed = 7000.0f;
    r202_work->cat[2].timer = zero;
    r202_work->checkTask = SceExec(0x12, (TaskFunc) r202_checkCatapult, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Add a (player area -> target area) pair to the catapult's fire table (max 4).
void cCatapult::setNewArea(int a, int b)
{
    if (nArea < 4) {
        area[nArea] = a;
        at[nArea] = b;
        nArea++;
    }
}

// Every frame: which catapult's area the player stands in, and the catapult state machines.
static void r202_checkCatapult()
{
    u32 i;

    SceSleep(1);
    for (i = 0; i < 4; i++) {
        r202_work->cat[i].em.setPtr(r202_work->cat[i].emNo, 2, 0);
    }
    for (;;) {
        int cur = 0;

        if (SceAtHitCheck(6) == 1) {
            cur = 2;
        }
        if (SceAtHitCheck(8) == 1) {
            cur = 3;
        }
        if (SceAtHitCheck(0xA) == 1) {
            cur = 1;
        }
        if (SceAtHitCheck(0xC) == 1) {
            cur = 0;
        }
        for (i = 0; i < 4; i++) {
            if (cur == i) {
                r202_work->cat[i].fire = 1;
            } else {
                r202_work->cat[i].fire = 0;
            }
            r202_work->cat[i].move();
        }
        r202_work->throwWait = r202_work->throwWait - 1;
        if (RsfCheck(G_ROOM_ID, 1)) {
            break;
        }
        SceSleep(1);
    }
}

// The cannon shot: destroy every catapult's rock and kill their load / throw tasks.
void r202_destroyCatapult()
{
    u32 i;

    for (i = 0; i < 4; i++) {
        EmMgr.destroy(r202_work->cat[i].rock);
        if (r202_work->cat[i].setRockTask) {
            SceKill(r202_work->cat[i].setRockTask);
        }
        if (r202_work->cat[i].throwRockTask) {
            SceKill(r202_work->cat[i].throwRockTask);
        }
    }
}

// Let every catapult fire as soon as the player is in range.
void r202_setFireAll()
{
    u32 i;

    for (i = 0; i < 4; i++) {
        r202_work->cat[i].fireAll = 1;
    }
}

// Per-frame catapult state machine (while the operator lives): 0 wait timer, 1 order a rock loaded
// (r202_setRock task), 2 wait for it, 3/4 wait for the player in a fire area (with the shared throwWait
// gap), 5 launch (r202_throwRock task), 6 wait for the rock to land, then a random reload delay.
void cCatapult::move()
{
    if (em.isActive() == 0) {
        return;
    }
    if (em.checkStatus(EM_STATUS_ACTIVE) == 0) {
        return;
    }
    if (em.getHp() <= 0) {
        return;
    }
    if (enable == 0) {
        return;
    }
    switch (state) {
    case 0:
        if (timer <= 0) {
            state = 1;
        }
        timer--;
        break;
    case 1:
        setRockTask = SceExec(0x12, (TaskFunc) r202_setRock, (int) this, 0, SCE_PRIO_DEF_2, 0);
        rockSet = 0;
        state = 2;
        break;
    case 2:
        if (rockSet == 1) {
            if (pG->Room_flg[0] & 0x80000000) {
                state = 3;
            }
        }
        break;
    case 3:
        if (fire == 1) {
            if (r202_work->em180.isActive() != 0) {
                Vec p;

                em.getPos(&p);
                SndCall(6, 2, &p, 0, 0, 0);
            }
        }
        rock->setEffAlways(1, 0);
        state = 4;
        timer = 60;
        break;
    case 4:
        if (timer <= 0) {
            if (r202_work->throwWait <= 0) {
                if (checkHitArea() == 1) {
                    if (fire == 1 || fireAll == 1) {
                        fireAll = 0;
                        r202_work->throwWait = 15;
                        state = 5;
                    }
                }
            }
        }
        timer--;
        break;
    case 5:
        if (fire == 1) {
            if (r202_work->em180.isActive() != 0) {
                Vec p;

                em.getPos(&p);
                SndCall(6, 3, &p, 0, 0, 0);
            }
            r202_setFireAll();
        }
        r202_work->throwWait = 15;
        timer = 30;
        state = 6;
        break;
    case 6:
        if (timer <= 0) {
            state = 7;
        }
        timer--;
        break;
    case 7:
        throwRockTask = SceExec(0x12, (TaskFunc) r202_throwRock, (int) this, 0, SCE_PRIO_DEF_2, 0);
        state = 8;
        break;
    case 8:
        if (thrown == 1) {
            u32 r;

            thrown = 1;
            r = Rnd();
            state = 0;
            timer = (u8) (r / 10);
        }
        break;
    }
}

// 1 when the player stands in one of the catapult's areas (hitIdx tells which).
int cCatapult::checkHitArea()
{
    int i;

    for (i = 0; i < nArea; i++) {
        if (SceAtHitCheck(area[i]) == 1) {
            hitIdx = i;
            return 1;
        }
    }
    return 0;
}

// The operator picks up a rock and loads the catapult.
static void r202_setRock(cCatapult* c)
{
    c->em.setFlag(1);
    c->em.motionSet(ROOM_ARC_PTR(pG->pRoom, 0x35), 0xA, 0, 1, 0);
    SceSleep(54);
    {
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        cEmRock* rock;

        rock = SetRock(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, 0);
        c->rock = rock;
        rock->setParent(c->em.getPtr(), 0xA, 0);
    }
    SceSleep(50);
    if (c->em.checkStatus(EM_STATUS_ACTIVE) != 0) {
        int i;

        c->setRock();
        {
            // COMPILER-DIFF: candidate #17 (local-alloc qty order): the original's sched1 issued the
            // -0.024 high before the 0.0 high, so its local-alloc gave obj/pList r10/r11 and the -0.024
            // high r8; ours issues the highs the other way round (equal priority, LUID) and names them
            // r10/r9/r11. Value-carrying pins on the two pointers give the target's names.
            register cObj* o asm("r10");
            register cParts* pp asm("r11");
            o = c->obj;
            pp = o->pList;
            pp->ang.x = 0.0f;
        }
        for (i = 10; i != 0; i--) {
            c->obj->pList->ang.x += -0.024137001f;
            SceSleep(1);
        }
        c->rockSet = 1;
    }
    c->setRockTask = 0;
}

// The rock moves from the operator's hand onto the catapult arm.
void cCatapult::setRock()
{
    Vec p;
    Vec ofs = {0.0f, 0.0f, 0.0f};

    ofs.x = r202_rockOfs.x / obj->scale.x;
    ofs.y = r202_rockOfs.y / obj->scale.y;
    ofs.z = r202_rockOfs.z / obj->scale.z;
    p = ofs;
    rock->pos = p;
    rock->setParent((cEm*) obj, 0, 0);
}

// The arm swings: the rock flies, the arm comes back and bounces.
static void r202_throwRock(cCatapult* c)
{
    int i;
    f32 v;
    f32 lim;

    c->obj->pList->ang.x = -0.24137f;
    for (i = 10; i != 0; i--) {
        c->obj->pList->ang.x += 0.13986999f;
        SceSleep(1);
    }
    c->throwRock();
    // Pool order: the loop step -0.0349 precedes -0.0698 in the original's pool; the const declaration
    // creates the entry here while every use stays the literal (no code change).
    const f32 step = -0.034906585f;
    v = -0.06981317f;
    lim = -0.24137f;
    // Loop shapes (see docs/matching.md COMPILER-DIFF #7/#9, both closed as source forms): the exit store on
    // the break path keeps jump1 from folding the peeled exit test; this loop's exit code is > 20 insns
    // at jump1 (the constant in `v +=` costs 3), so the peel is made by the pre-cse2 jump pass, after
    // loop.c hoisted the exit store's constant (`fmr f28,f30`); the bounce loop's exit code is never
    // peeled (> 20 insns, then gcse's preheader insertions).
    for (;;) {
        c->obj->pList->ang.x += v;
        v += -0.034906585f;
        if (c->obj->pList->ang.x < lim) {
            c->obj->pList->ang.x = -0.24137f;
            break;
        }
        SceSleep(1);
    }
    for (i = 0; i < 4; i++) {
        lim *= 0.5f;
        v *= -0.5f;
        for (;;) {
            c->obj->pList->ang.x += v;
            v += -0.034906585f;
            if (c->obj->pList->ang.x < lim && v < 0.0f) {
                c->obj->pList->ang.x = lim;
                break;
            }
            SceSleep(1);
        }
    }
    c->throwRockTask = 0;
}

// Throw the rock at the target (the fixed position, the hit area's target area or the player).
void cCatapult::throwRock()
{
    Vec from;
    Vec to;
    Vec spd;

    from.x = rock->pList->mat[0][3];
    from.y = rock->pList->mat[1][3];
    from.z = rock->pList->mat[2][3];
    if (targetOn == 1) {
        to = targetPos;
    } else {
        int n = at[hitIdx];

        if (n >= 0) {
            AreaGetInsidePos(&to, &SceAtPtr(n)->area);
        } else {
            r202_getTargetPos(&to);
        }
    }
    CalcParabolaVector(&spd, &from, &to, speed);
    rock->setThrow2(&spd, 0);
    thrown = 1;
}

// A point between the player and 1000 units behind him, along the wall he faces.
void r202_getTargetPos(Vec* out)
{
    Vec a = {0.0f, 0.0f, 1000.0f};
    Vec b = {0.0f, 0.0f, -1000.0f};

    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    SatMgr.hitCheck(&pPL->pos, &a, &a, 0, 0, 0);
    PSVECSubtract(&a, &b, &a);
    PSVECScale(&a, &a, fRand0_1());
    PSVECAdd(&a, &b, out);
}
