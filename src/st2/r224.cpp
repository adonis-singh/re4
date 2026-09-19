#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "atari_init.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "objYagura.h"
#include "em.h"
#include "em_wrap.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "act_btn.h"
#include "mes.h"
#include "motion.h"
#include "math_sub.h"
#include "st_mgr_event.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "db_log.h"

// Room 2-2-4 (D:/Bio4/Prog/r224.cpp): the water hall. The lever ("reva") that opens the floor
// grate ("gnd"), the lid ("futa") that lets the two Novistadors out, the mine cart ("toroko") ride
// and the exit door.

struct R224Work {
    cObj* obj;       // 0x000  the cart (SetObjSmd)
    cObj* obj2;      // 0x004  the grate collision object
    cEmWrap em0;     // 0x008
    cEmWrap em1;     // 0x014
    Camera cam;      // 0x020  lever camera
    int x118;        // 0x118
    u32 se0;         // 0x11C
    u32 se1;         // 0x120
    Vec plPos;       // 0x124  player position before the enemy event
};

// One-member struct: every store through the work reloads the pointer.
struct R224WorkPtr {
    R224Work* p;
};

static R224WorkPtr r224_work;

static f32 reva_low = 1763.0f;
static f32 reva_high = 1424.0f;
static f32 reva_rate = 0.008f;   // unreferenced
static f32 reva_acc = 3.5f;

// Hit effects of attribute type 2 (water)
static const AtEffInfo r224_eff_info = {
    1, {0xD2, 1}, {1, 0x27}, {0xD2, 1}, {0xD2, 1}, {0xD2, 1}, {0xD2, 1}, {1, 0x27}, {1, 0x27},
};

// The Novistador (em2b) by vtable slot: only the virtual the room calls.
class cEm2b : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2B_WK)
    virtual void v50();
    virtual void v58();
    virtual void v60();
    virtual int ckThrow2();   // 0x68
};

// Atari flag stores through the info's address (r207 idiom): `addi r9, pl, 0x2B4` + lhz/sth 0x1A(r9).
static inline void AtariFlagsAnd(cAtariInfo* a, u16 mask) { a->m_flag &= mask; }
static inline void AtariFlagsOr(cAtariInfo* a, u16 bit) { a->m_flag |= bit; }

struct PlPtr {
    cPlayer* p;
};
#define pPLS (((PlPtr*) &pPL)->p)

// Enemy manager work `i` (address recomputed per use, em_set idiom).
static inline cEm* r224_emWork(u32 i)
{
    return (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
}

static void r224_cam_task();
static void r224_em_set_exit();
static void r224_em_set();
static void r224_toroko();
static void reva_common_move();
static void futa_move();
static void gnd_open();
void gnd_close();
static void reva_move();
static void em_die_ck();
void door_open(int no);
void door_close();
static void r224_door_mes();
static void r224_str_check();

// Room init (the Novistador water hall): area 3 = the cart ride, area 4 = the lever; water hit effects;
// the cart object with its motion paused and an ambient boost; a bare collision object for the grate;
// the lever / grate / lid / door posed per Room_flg bits (fight done, grate open); the two Novistador
// handles and the battle stream.
void R224Init()
{
    cObj* yagura;

#line 74 "D:/Bio4/Prog/r224.cpp"
    r224_work.p = (R224Work*) MEM_CALLOC(sizeof(R224Work), 1, 0xd);
    SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r224_toroko, 0, 1);
    SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) reva_move, 0, 1);
    EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &r224_eff_info);
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    r224_work.p->obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &pos, &rot, 0x10, 1);
    r224_work.p->obj->motionSet(ROOM_ARC_PTR(pGS->pRoom, 0x23), 0xA, 0, 1, 0);
    r224_work.p->obj->motSpeedRate = 0.0f;
    {
        cObj* obj = r224_work.p->obj;

        obj->be_flag |= 8;
        obj->AddAmb_r = 0x3C;
        obj->AddAmb_g = 0x50;
        obj->AddAmb_b = 0x64;
    }
    r224_work.p->obj2 = ObjMgr.create(2);
    r224_work.p->obj2->pos.x = -4413.0f;
    r224_work.p->obj2->pos.y = 0.0f;
    r224_work.p->obj2->pos.z = -543.0f;
    AtariInit(&r224_work.p->obj2->atari, 0.0f, 0.0f, 0.0f, 0.0f, 5100.0f, 5100.0f, 50000.0f, 0, 0x18, 0);
    AtariFlagsAnd(&r224_work.p->obj2->atari, 0xFEFF);
    AtariFlagsAnd(&r224_work.p->obj2->atari, 0xFDFF);
    SmdGetObjPtr(0x13)->be_flag |= 0x20;
    SmdGetObjPtr(0x14)->be_flag |= 0x20;
    SceAtSetEnable(5, 0);
    SceAtSetEnable(6, 0);
    SceAtSetEnable(7, 1);
    SceExec(0x12, (TaskFunc) r224_str_check, 0, 2, SCE_PRIO_DEF_2, 0);
    PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    {
        Vec rot2;

        pos.x = 5113.53f;
        pos.y = 0.0f;
        pos.z = 7702.28f;
        rot2.x = 0.0f;
        rot2.y = 0.9817477f;
        rot2.z = 0.0f;
        yagura = SetYagura(ROOM_ARC_PTR(pG->pRoom, 0x2A), ROOM_ARC_PTR(pG->pRoom, 0x2B), &pos, &rot2);
    }
    if (yagura) {
        ((cObjYagura*) yagura)->setMotionVib(ROOM_ARC_PTR(pG->pRoom, 0x2C));
        yagura->be_flag |= 8;
        yagura->AddAmb_r = 0x1E;
        yagura->AddAmb_g = 0x28;
        yagura->AddAmb_b = 0x28;
    }
    if (RsfCheck(G_ROOM_ID, 1)) {
        cObj* obj;

        obj = SmdGetObjPtr(0x16);
        obj->be_flag |= 0x20;
        obj->pos.y = 7838.0f;
        obj = SmdGetObjPtr(0x12);
        obj->be_flag |= 0x20;
        obj->pos.y = 4700.0f;
        SceAtSetEnable(8, 0);
    } else {
        cObj* obj;

        EmReadSearch(0x2B, 0, 0);
        obj = SmdGetObjPtr(0x12);
        obj->be_flag |= 0x20;
        obj->pos.y = 4700.0f;
        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r224_em_set, 0, 1);
    }
}

// Per frame: while Room_flg[0] 0x20000000 (standing at the cart) show action button 0x1B; pressing it
// (Key.trg 0x00080000) starts the cart ride.
void R224Main()
{
    if (pG->Room_flg[0] & 0x20000000) {
        u32 v = pG->Room_flg[0] & ~0x20000000;

        pG->Room_flg[0] = v;
        // COMPILER-DIFF: 13 (the stack-argument zero reuses `v`: a two-set pseudo has no REG_EQUIV,
        // its `li` waits for the `stw` that reads v and it shares v's r0, like the
        // reload-materialised original; a fresh `0` is born early and takes r9 from pG)
        v = 0;
        ActBtn.set(0x1B, 5, 0, 0, 2, 1, 0, v);
        if (Key.trg & 0x00080000) {
            SceExec(0x12, (TaskFunc) r224_toroko, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
}

// Camera task of the enemy event: cut 6, then cut 7 unless the fight is already over (Room_flg[0] 0x04000000).
static void r224_cam_task()
{
    CamCtrl.CutCall(6);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if ((pG->Room_flg[0] & 0x04000000) == 0) {
        CamCtrl.CutCall(7);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
}

// End of the Novistador entrance: Scenario_flg[4] 0x00400000 (exit) off, SEs stopped, the door 0x16
// snapped shut, both Novistadors made solid (atari 0x300) and placed on the floor alerted, Room_flg[0]
// 0x04000000, area 0 = the shut-door message, the death watcher, boss points reset.
static void r224_em_set_exit()
{
    Vec v;

    ScfFlagOff(pG, SCF_89);
    SndStop(r224_work.p->se0, 0);
    SndStop(r224_work.p->se1, 0);
    SmdGetObjPtr(0x16)->pos.y = 7838.0f;
    AtariFlagsOr(&r224_work.p->em0.getPtr()->atari, 0x300);
    AtariFlagsOr(&r224_work.p->em1.getPtr()->atari, 0x300);
    pGS->Room_flg[0] |= 0x04000000;
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r224_door_mes, 0, 1);
    SceExec(0x12, (TaskFunc) em_die_ck, 0, 0, SCE_PRIO_DEF_2, 0);
    GamePointBossReset();
    AtariFlagsOr(&r224_work.p->em0.getPtr()->atari, 0x300);
    AtariFlagsOr(&r224_work.p->em1.getPtr()->atari, 0x300);
    v.x = 7812.0f;
    v.y = 0.0f;
    v.z = -2048.0f;
    r224_work.p->em0.setPos(&v);
    v.x = 12864.0f;
    v.y = 0.0f;
    v.z = -4906.0f;
    r224_work.p->em1.setPos(&v);
    r224_work.p->em0.setFlag(1);
    r224_work.p->em1.setFlag(1);
    r224_work.p->em0.setNoSuspend(0);
    r224_work.p->em1.setNoSuspend(0);
    SceEventEnd(0);
    pPL->setPos(&r224_work.p->plPos);
    door_close();
}

// The Novistadors drop in: the player is moved back, the door closes.
static void r224_em_set()
{
    Vec v;

    r224_work.p->plPos = pPLS->pos;
    {
        cPlayer* pl = pPLS;

        v.x = 3000.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        pl->setPos(&v);
    }
    SceEventStart(0);
    SceExec(0x12, (TaskFunc) r224_cam_task, 0, 0, SCE_PRIO_DEF_2, 0);
    r224_work.p->em0.setEm(0xC8, -1, 1, 1, 1);
    r224_work.p->em1.setEm(0xC9, -1, 1, 1, 1);
    r224_work.p->em0.setNoSuspend(1);
    r224_work.p->em1.setNoSuspend(1);
    if (r224_work.p->em0.getPtr()) {
        AtariFlagsAnd(&r224_work.p->em0.getPtr()->atari, 0xFCFF);
        AtariFlagsAnd(&r224_work.p->em1.getPtr()->atari, 0xFCFF);
        v.x = 16700.0f;
        v.y = 0.0f;
        v.z = -7320.0f;
        r224_work.p->em0.setPos(&v);
        v.x = 19580.0f;
        v.y = 0.0f;
        v.z = -8040.0f;
        r224_work.p->em1.setPos(&v);
    }
    SceSetEventCancel(1, (TaskFunc) r224_em_set_exit, 0, -1, 1);
    r224_work.p->se0 = SndCall(6, 2, &SmdGetObjPtr(0x16)->pos, 0, 0, 0);
    r224_work.p->se1 = SndCall(6, 6, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
    SceAtDataReset(0);
    SceAtSetEnable(8, 0);
    EstSet(0, -1, 0, 0, 1, 0x3E, 1, 3, 0, 0);
    {
        f32 spd = 100.0f;
        cObj* obj;

        while (obj = SmdGetObjPtr(0x16), obj->be_flag |= 0x20, obj->pos.y += spd, !(obj->pos.y > 7838.0f)) {
            SceSleep(1);
        }
    }
    SndCall(6, 3, &SmdGetObjPtr(0x16)->pos, 0, 0, 0);
    SndCall(6, 7, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
    r224_work.p->em0.setFlag(1);
    SceSleep(10);
    r224_work.p->em1.setFlag(1);
    SceSleep(60);
    AtariFlagsOr(&r224_work.p->em0.getPtr()->atari, 0x300);
    AtariFlagsOr(&r224_work.p->em1.getPtr()->atari, 0x300);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r224_em_set_exit();
}

// The mine cart ride out of the hall.
static void r224_toroko()
{
    cPlayer* pl = pPL;
    Vec v;
    u32 frames;

    if (pG->Room_flg[0] & 0x08000000) {
        return;
    }
    pG->Room_flg[0] |= 0x08000000;
    pl->beginAction();
    AtariFlagsAnd(&pPLS->atari, 0xFEFF);
    AtariFlagsAnd(&pPLS->atari, 0xFDFF);
    pPLS->atari.setPriority(PRI_LV1);
    pPL->dmg.set(0, 0x80);
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    PlSetHand(1, 0);
    {
        cModel* parts = r224_work.p->obj->getPartsPtr(0);

        v.x = -29.17f;
        v.y = 0.0f;
        v.z = -269.88998f;
        PSMTXMultVec(parts->mat, &v, &v);
    }
    v.y = 6000.0f;
    SndCall(6, 8, 0, 0, 0, 0);
    pPL->setPos(&v);
    pPLS->ang.y = -2.1991148f;
    pPLS->setNoSuspend(1);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 0x201, 0);
    r224_work.p->obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 0, 0, 1, 0);
    r224_work.p->obj->motSpeedRate = 1.0f;
    frames = (u32) MotionGetMaxFrame(&pPLS->Motion);
    SceSleep(90);
    SndCall(6, 9, 0, 0, 0, 0);
    SceSleep(frames - 90);
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->Wep->setTrans(1, 0);
    pl->endAction(0);
    pPL->dmg.clear();
    AtariFlagsOr(&pPLS->atari, 0x100);
    AtariFlagsOr(&pPLS->atari, 0x200);
    pPLS->atari.setPriority(0);
    pG->Room_flg[0] &= ~0x08000000;
}

// The lever handle swings to its other end and back.
// COMPILER-DIFF: 12: the target loads `spd = 0.0f` before SndCall and RE-LOADS the same pool 0.0
// for the hoisted loop compare (`lfs f26`), each with its own `lis`; our cse folds every
// constant-pool load to its CONST_DOUBLE and rewrites the hoisted one as `fmr f26,f31` from spd,
// and merges the two `high` pseudos of one symbol into a callee-saved register. The 0.0 word is
// therefore the named .rodata word r224_zero in the pool entry's slot (a top-level asm: it must be
// a LOCAL label with a fixed name -- a function-local static gets a numbered private name, a
// file-scope `static const` is deferred to the end of the file, and a global symbol leaves the
// REL's @l fields unresolved), declared `extern const f32` so the MEM is /u like a pool load and
// sched1 does not order it after the be_flag store. spd's init reads it through the asm-labelled
// alias r224_zero_v (a different SYMBOL_REF: cse cannot merge its `high` or its MEM with the
// compare constant's), `zero = r224_zero` is the compare constant, the in-loop reload reads it
// through FCRef (a plain MEM: loop.c hoists its high like the target's r28; the symbol's first
// occurrence after `reva_acc` keeps gcse's expression index order = the preheader `lis` order;
// the name's gcse bucket (20) must stay below reva_high's (35)). `BitOn` for be_flag makes the
// `lfs acc` depend on the store (target: `lfs f30,acc` last). The dead `if (spd == 1.85f) up = 0;`
// is a 4th ref for the hoisted 1.85 constant so it ranks above zero in global-alloc (f27 vs f26;
// zero has no REG_EQUIV doubling, the pool constant has).
static inline f32 FCRef(const f32& v) { return v; }
asm(".section \".rodata\"\n\t.align 2\nr224_zero:\n\t.long 0\n\t.section \".text\"");
extern const f32 r224_zero;
extern const f32 r224_zero_v asm("r224_zero");

// The lever handle (smd 0x3F) slides between reva_low and reva_high with acceleration reva_acc and its SE.
static void reva_common_move()
{
    cObj* obj = SmdGetObjPtr(0x3F);
    f32* py = &obj->pos.y;
    f32 lo = reva_low;
    f32 spd;
    f32 hi;
    f32 acc;
    f32 zero;

    spd = r224_zero_v;
    hi = reva_high;
    SndCall(6, 4, &obj->pos, 0, 0, 0);
    BitOn(obj->be_flag, 0x20);
    acc = reva_acc;
    zero = r224_zero;
    for (;;) {
        int up;

        if (hi > lo) {
            *py += spd;
            up = 1;
        } else {
            *py -= spd;
            up = 0;
        }
        if (spd >= zero) {
            if (up ? (*py < hi) : (*py > hi)) {
                spd += acc * 1.85f;
            } else if (pG->Room_flg[0] & 0x40000000) {
                spd = FCRef(r224_zero);
                *py = reva_high;
            } else {
                spd = -acc;
            }
        } else {
            if (up ? (*py > lo) : (*py < lo)) {
                spd -= acc;
            } else {
                *py = reva_low;
                SceAtSetEnable(4, 1);
                return;
            }
        }
        if (spd == 1.85f) { // COMPILER-DIFF: 12 (dead test, see above)
            up = 0;
        }
        SceSleep(1);
    }
}

// The lid opens: the Novistadors come out of the pit and the grate opens under them.
static void futa_move()
{
    cEm* em0;
    cEm* em1;
    void* zero;

    SceSleep(15);
    pG->Room_flg[0] |= 0x80000000;
    em0 = r224_work.p->em0.getPtr();
    em1 = r224_work.p->em1.getPtr();
    if (em0 && ((cEm2b*) em0)->ckThrow2() == 1) {
        RsfSet(G_ROOM_ID, 0);
        em0->setNoSuspend(1);
    }
    if (RsfCheck(G_ROOM_ID, 0)) {
        if (em1) {
            Vec p = {-4450.0f, 0.0f, -562.0f};
            Vec d;

            PSVECSubtract(&em1->pos, &p, &d);
            if (PSVECMag(&d) < 4000.0f) {
#line 507 "D:/Bio4/Prog/r224.cpp"
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, 4000.0f);
                PSVECAdd(&em1->pos, &d, &em1->pos);
            }
        }
    } else if (em1 && ((cEm2b*) em1)->ckThrow2() == 1) {
        RsfSet(G_ROOM_ID, 0);
        em1->setNoSuspend(1);
    }
    zero = 0;
    CamCtrl.CutCall(5);
    SceEventStart(1);
    EstSet(0, -1, 0, 0, 1, 0x40, 1, 2, (u32) zero, zero);
    gnd_open();
    SceSleep(60);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (em0) {
        em0->setNoSuspend(0);
    }
    if (em1) {
        em1->setNoSuspend(0);
    }
    SceSleep(750);
    EffectEspgenDelete(0, 2, 0);
    SceSleep(15);
    pG->Room_flg[0] &= 0x7FFFFFFF;
    SceSleep(135);
    EstSet(0, -1, 0, 0, 1, 0x3F, 1, 2, (u32) zero, zero);
    gnd_close();
    if (RsfCheck(G_ROOM_ID, 0)) {
        pG->Room_flg[0] |= 0x10000000;
    }
}

// The floor grate halves (smd 0x13/0x14) swing open to 1.3 rad with growing speed (SE 0xA); the grate
// collision goes solid, area 4 off, areas 5/6 on, 7 off.
static void gnd_open()
{
    f32 spd = 0.01f;
    f32 acc = 0.005f;

    SmdGetObjPtr(0x13)->setNoSuspend(1);
    SmdGetObjPtr(0x14)->setNoSuspend(1);
    SmdGetObjPtr(0x13)->ang.x = 0.0f;
    SmdGetObjPtr(0x14)->ang.x = 0.0f;
    SceAtSetEnable(4, 0);
    AtariFlagsOr(&r224_work.p->obj2->atari, 0x100);
    AtariFlagsOr(&r224_work.p->obj2->atari, 0x200);
    SceAtSetEnable(5, 1);
    SceAtSetEnable(6, 1);
    SceAtSetEnable(7, 0);
    SndCall(6, 0xA, &SmdGetObjPtr(0x13)->pos, 0, 0, 0);
    for (;;) {
        spd += acc;
        if (SmdGetObjPtr(0x13)->ang.x > 1.3f) {
            break;
        }
        SmdGetObjPtr(0x13)->ang.x += spd;
        SmdGetObjPtr(0x14)->ang.x -= spd;
        SceSleep(1);
    }
}

// The grate halves swing shut (SE 0xB) and snap to 0.
void gnd_close()
{
    f32 spd = 0.01f;
    f32 acc = 0.005f;

    SmdGetObjPtr(0x13)->be_flag |= 0x20;
    SmdGetObjPtr(0x14)->be_flag |= 0x20;
    AtariFlagsOr(&r224_work.p->obj2->atari, 0x100);
    AtariFlagsOr(&r224_work.p->obj2->atari, 0x200);
    SndCall(6, 0xB, &SmdGetObjPtr(0x13)->pos, 0, 0, 0);
    for (;;) {
        spd += acc;
        if (SmdGetObjPtr(0x13)->ang.x < 0.0f) {
            break;
        }
        SmdGetObjPtr(0x13)->ang.x -= spd;
        SmdGetObjPtr(0x14)->ang.x += spd;
        SceSleep(1);
    }
    SmdGetObjPtr(0x13)->ang.x = 0.0f;
    {
        cObj* o = SmdGetObjPtr(0x14);

        o->ang.x = 0.0f;
        // COMPILER-DIFF: 13 -- the target issues `li r3,5; li r4,0` after the store; ours issues
        // the free `li r4,0` in the stfs's cycle (2-issue, `li r3` waits for the stfs's r3) in
        // sched1 and again in sched2. The codeless barrier keeps both argument `li`s after the
        // store, where the tie falls back to their emission order.
        asm volatile("");
        SceAtSetEnable(5, 0);

    }
    SceAtSetEnable(6, 0);
    SceAtSetEnable(7, 1);
    AtariFlagsAnd(&r224_work.p->obj2->atari, 0xFEFF);
    AtariFlagsAnd(&r224_work.p->obj2->atari, 0xFDFF);
    pGS->Room_flg[0] &= ~0x40000000;
}

// The lever: the player holds the button, the camera swings with the stick, the grate opens.
static void reva_move()
{
    int state = 0;
    u32 frames;

    if (RsfCheck(G_ROOM_ID, 0)) {
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        return;
    }
    ((cUnitEventView*) pPL)->beginEvent(0);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 3, 0, 1, 0);
    frames = (u32) MotionGetMaxFrame(&pPL->Motion);
    {
        Vec p = {-13250.0f, 0.0f, -7400.0f};

        pPLS->ang.y = -2.5132742f;
        pPLS->setPos(&p);
    }
    while ((PlGetStatus() & 0x00020000) && !(Key.trg & 0x40000000)) {
        if (state == 0) {
            if (--frames == 0) {
                pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x27), 5, 0, 5, 0);
                state = 1;
                r224_work.p->cam = pG->Cam;
            }
        } else if (state == 1) {
            Camera* cam;
            Vec d;
            Mtx mtx;
            f32 ang;
            f32 add;

            ActBtn.set(0x14, 5, 0, 0, 2, 1, 0, 0);
            cam = &r224_work.p->cam;
            add = 0.0f;
            d.x = cam->param.at.x - cam->param.pos.x;
            d.y = cam->param.at.y - cam->param.pos.y;
            d.z = cam->param.at.z - cam->param.pos.z;
            ang = atan2f(d.x, d.z) * 57.295776f;
            if (ang < 90.0f) {
                if (Key.on & 0x8) {
                    add = 2.5f;
                }
            }
            if (ang > 50.0f) {
                if (Key.on & 0x4) {
                    add -= 2.5f;
                }
            }
            PSMTXRotRad(mtx, 'y', add * 0.017453292f);
            PSMTXMultVecSR(mtx, &d, &d);
            PSVECAdd(&d, &cam->param.pos, &cam->param.at);
            CameraSetOrientationUp(&r224_work.p->cam);
            CamCtrl.m_pExtraCamera = (s32) &r224_work.p->cam;
            if (Key.trg & 0x00080000) {
                pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x28), 5, 0, 1, 0);
                frames = (u32) MotionGetMaxFrame(&pPL->Motion);
                SceExec(0x12, (TaskFunc) reva_common_move, 0, 0, SCE_PRIO_DEF_2, 0);
                state = 2;
                if ((pG->Room_flg[0] & 0x40000000) == 0) {
                    pG->Room_flg[0] |= 0x40000000;
                    SceExec(0x12, (TaskFunc) futa_move, 0, 0, SCE_PRIO_DEF_2, 0);
                }
            }
        } else {
            if (--frames == 0) {
                break;
            }
        }
        SceSleep(1);
    }
    {
        cPlayer* pl = pPL;

        ((cUnitEventView*) pl)->endEvent(0);
        pl->m_Hokan = 0xC;
    }
}

// Task: when no Novistador (0x2B) is alive, Room_flg bit 1, 270 frames later the doors open and the exit unlocks.
static void em_die_ck()
{
    while (SceCountEmAlive(0x2B, -1) != 0) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 1);
    SceSleep(270);
    door_open(0);
    ScfFlagOn(pG, SCF_89);
}

// The exit door 0x16 (and, with no == 0, the grille 0x12 at double speed) rise 100 units a frame under
// camera cut 6 with dust; area 0 reset, area 8 off.
void door_open(int no)
{
    f32 spd;

    SndCall(6, 2, &SmdGetObjPtr(0x16)->pos, 0, 0, 0);
    SndCall(6, 6, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
    SceAtDataReset(0);
    SceAtSetEnable(8, 0);
    SceEventStart(1);
    CamCtrl.CutCall(6);
    EstSet(0, -1, 0, 0, 1, 0x3E, 1, 3, 0, 0);
    spd = 100.0f;
    for (;;) {
        cObj* obj = SmdGetObjPtr(0x16);

        obj->be_flag |= 0x20;
        obj->pos.y += spd;
        if (obj->pos.y > 7838.0f) {
            break;
        }
        if (no == 0) {
            obj = SmdGetObjPtr(0x12);
            obj->be_flag |= 0x20;
            obj->pos.y += spd + spd;
        }
        SceSleep(1);
    }
    SndCall(6, 3, &SmdGetObjPtr(0x16)->pos, 0, 0, 0);
    SndCall(6, 7, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The exit door and grille drop (50 / 150 units a frame) to their closed heights; area 0 = the shut
// message, area 8 on.
void door_close()
{
    SndCall(6, 2, &SmdGetObjPtr(0x16)->pos, 0, 0, 0);
    SndCall(6, 6, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r224_door_mes, 0, 1);
    SceAtSetEnable(8, 1);
    for (;;) {
        u32 cnt = 0;
        cObj* obj;

        obj = SmdGetObjPtr(0x16);
        obj->be_flag |= 0x20;
        if (obj->pos.y > 2767.0f) {
            obj->pos.y -= 50.0f;
        } else {
            obj->pos.y = 2767.0f;
            cnt = 1;
        }
        obj = SmdGetObjPtr(0x12);
        obj->be_flag |= 0x20;
        if (obj->pos.y > 1570.0f) {
            obj->pos.y -= 150.0f;
        } else {
            obj->pos.y = 1570.0f;
            cnt++;
        }
        if (cnt <= 1) {
            SceSleep(1);
        } else {
            break;
        }
    }
    SndCall(6, 3, &SmdGetObjPtr(0x16)->pos, 0, 0, 0);
    SndCall(6, 7, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
}

// Area 0 while the door is shut: knock SE and message 1.
static void r224_door_mes()
{
    SndCall(6, 1, 0, 0, 0, 0);
    SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
}

// The battle stream plays while a Novistador is active.
static void r224_str_check()
{
    int playing = 0;

    for (;;) {
        int found = 0;
        u32 i;

        for (i = 0; i < EmMgr.nArray; i++) {
            cEm* em = r224_emWork(i);

            if (em->id == 0x2B && em->checkStatus(EM_STATUS_ACTIVE) != 0 && em->hp > 0 && (em->be_flag & 0x201) == 1) {
                found = 1;
            }
        }
        if (found == 1) {
            if (playing == 0) {
                SndRoomStrStart(5, 0, 1);
                playing = 1;
            }
        } else if (playing == 1) {
            SndRoomStrStop(3);
            playing = 0;
        }
        SceSleep(1);
    }
}

// The next unit's .data starts 8-aligned.
asm(".section .data; .balign 8");
