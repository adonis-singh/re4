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
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "st_mgr_event.h"
#include "act_btn.h"
#include "item.h"
#include "sscrn.h"
#include "cSceObj.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "math_sub.h"
#include "motion.h"
#include "rnd.h"
#include "db_log.h"

// Room 2-25 (D:/Bio4/Prog/r225.cpp): the graveyard / crank puzzle; a local copy of sce_com's
// SceElevator with the chapter end on the way out.

struct R225Work {
    cObj* crank;   // 0x00
    cObj* door;    // 0x04  the SetObjSmd dummy that rides along with the door
};

// sce_com.cpp SceElevatorData
struct SceElevatorData {
    s32 dir;
    u32 objId;
    Vec pos;
    Vec plPos;
    Vec plRot;
    s32 cut;
    u16 pad_30;
    u16 seStart;
    u16 pad_34;
    u16 seStop;
    Vec jumpPos;
    Vec jumpRot;
    u16 room;
};

static R225Work* r225_work;

// Struct-member view of pPL (MEM_IN_STRUCT_P load): sched2 keeps it below preceding in-struct frame stores.
struct PlPtr {
    cPlayer* p;
};
#define pPLS (((PlPtr*) &pPL)->p)

// Stores through references (not MEM_IN_STRUCT_P): the static pointer / pPL reload after each one.
static inline void FSetP(f32& d, f32 v) { d = v; }
static inline void PSet(cObj*& d, cObj* v) { d = v; }

// Inline helpers owning their locals: the inlined frame is one BLKmode temp slot popped at the end of
// each statement, so every call shares frame slot 8. Argument MEMs are evaluated lazily (the pointer
// before a call in another argument, the load after it: `lwz r30,pPL; bl fRand1_1; lfs 148(r30)`).
static inline void SetPosXYZ(cModel* m, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setPos(&v);
}

// The two fade colours must live in a BLKmode object: a 4-byte GXColor local becomes an ADDRESSOF
// pseudo (SImode) and purge_addressof gives it a permanent frame slot (24/28, 32/36) instead of the
// shared temp at 8/12; a 12-byte struct reuses the Vec slot (temp reuse needs equal modes).
struct FadeColors {
    GXColor c0;
    GXColor c1;
    u32 pad;
};

// FadeSet with two packed RGBA colour words over 30 frames.
static inline void FadeSetRGBA(u32 mode, u32 rgba0, u32 rgba1)
{
    FadeColors c;

    *(u32*) &c.c0 = rgba0;
    *(u32*) &c.c1 = rgba1;
    FadeSet(mode, &c.c0, &c.c1, 30, 0, 0);
}

static SceElevatorData r225_elvArrive = {2, 0x15, {0.0f, 0.0f, 0.0f}, {80130.0f, 1500.0f, -22530.0f}, {0.0f, -1.6f, 0.0f}, -1, 0, 0xE, 0, 0xF, {-3300.0f, 5000.0f, 22200.0f}, {0.0f, 3.14f, 0.0f}, 0x226};
static SceElevatorData r225_elvLeave = {3, 0x15, {0.0f, 0.0f, 0.0f}, {80130.0f, 1500.0f, -22530.0f}, {0.0f, -1.6f, 0.0f}, 8, 0, 0xD, 0, 0xF, {-3300.0f, 5000.0f, 22200.0f}, {0.0f, 3.14f, 0.0f}, 0x226};

static void gnd_open();
static void r225_operateCrank();
void r225_open_door();
static void r225_DoorMes_exec();
static void r225_DoorMes();
static void r225_moveGrave(int dir);
static void r225_checkGrave();
static void first_cut_exit();
static void first_cut();
extern "C" void SceElevator_r225(SceElevatorData* d);

// Room init: until the crank was turned (Room_flg bit 0) area 4 = the crank with areas 5/6 off and 0xA
// on, else the raised layout (plate object 0x27 moved); until the key door is open (bit 1) area 0xC =
// its message with the key-use watcher and area 0xD, else the door posed open; the grave slab, the
// first look (area 0x10 once), the elevator data and its area.
void R225Init()
{
#line 81 "D:/Bio4/Prog/r225.cpp"
    r225_work = (R225Work*) MEM_CALLOC(sizeof(R225Work), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r225_operateCrank, 0, 1);
        SceAtSetEnable(5, 0);
        SceAtSetEnable(6, 0);
        SceAtSetEnable(0xA, 1);
    } else {
        SceAtSetEnable(5, 1);
        SceAtSetEnable(6, 1);
        SceAtSetEnable(0xA, 0);
        SmdGetObjPtr(0x27)->be_flag |= 0x20;
        SmdGetObjPtr(0x27)->pos.x = 1393.0f;
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(0xC, SCE_LEVEL10, 0, (TaskFunc) r225_DoorMes, 0, 1);
        SceExec(0x12, (TaskFunc) r225_DoorMes_exec, 0, 0, SCE_PRIO_DEF_2, 0);
        SceAtSetEnable(0xD, 1);
    } else {
        SmdGetObjPtr(0x25)->be_flag |= 0x20;
        SmdGetObjPtr(0x25)->pos.y = 4579.0f;
        SceAtSetEnable(0xD, 0);
    }
    if (pG->room_id_prev == 0x21B) {
        RsfSet(G_ROOM_ID, 2);
    }
    if (RsfCheck(G_ROOM_ID, 2)) {
        SmdGetObjPtr(0x28)->be_flag |= 0x20;
        SmdGetObjPtr(0x28)->pos.x = 71275.0f;
        SceAtSetEnable(9, 1);
        SceAtSetEnable(8, 1);
        SceAtSetEnable(0xB, 0);
    } else {
        SceAtSetEnable(9, 0);
        SceAtSetEnable(8, 0);
        SceAtSetEnable(0xB, 1);
    }
    SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) SceElevator_r225, &r225_elvLeave, 1);
    if (pG->room_id_prev == 0x226) {
        if (!SysFlagChk(pG, SYS_CONTINUE)) {
            if (!SysFlagChk(pG, SYS_LOAD_GAME)) {
                SceExec(0x12, (TaskFunc) SceElevator_r225, (int) &r225_elvArrive, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r225_checkGrave, 0, 1);
    if (pG->room_id_prev == 0x21D) {
        if (!SysFlagChk(pG, SYS_CONTINUE)) {
            if (!SysFlagChk(pG, SYS_LOAD_GAME)) {
                SceExec(0x12, (TaskFunc) r225_moveGrave, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtDataSet_exec(0x10, SCE_LEVEL10, 0, (TaskFunc) first_cut, 0, 1);
    }
    {
        Vec pos;
        Vec rot;
        cEmHit* hit;

        pos.x = 24048.0f;
        pos.y = 5550.0f;
        pos.z = -12503.0f;
        rot.x = 0.0f;
        rot.y = -1.08f;
        rot.z = 0.0f;
        hit = SetEmHit(ROOM_ARC_PTR(pG->pRoom, 0x35), ROOM_ARC_PTR(pG->pRoom, 0x36), &pos, &rot, 2);
        if (hit) {
            hit->setBeetle(ROOM_ARC_PTR(pG->pRoom, 0x37), ROOM_ARC_PTR(pG->pRoom, 0x39), ROOM_ARC_PTR(pG->pRoom, 0x38));
        }
    }
    ScfFlagOn(pG, SCF_R225_IN);
}

// Per-frame room main: nothing.
void R225Main()
{
}

// The crank raised the ground plate: the door slides open.
static void gnd_open()
{
    SceEventStart(0);
    CamCtrl.CutCall(7);
    SndCall(6, 5, 0, 0, 0, 0);
    SmdGetObjPtr(0x27)->be_flag |= 0x20;
    while (SmdGetObjPtr(0x27)->pos.x < 1393.0f) {
        SceSleep(1);
        SmdGetObjPtr(0x27)->pos.x += 22.0f;
    }
    SmdGetObjPtr(0x27)->pos.x = 1393.0f;
    SceSleep(35);
    RsfSet(G_ROOM_ID, 0);
    SceAtSetEnable(5, 1);
    SceAtSetEnable(6, 1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 4: Leon turns the crank; the plate rises with the crank speed.
static void r225_operateCrank()
{
    int spd = 0;
    int cur = 0;
    int acc = 0;
    KeyWork* key;

    pG->Room_flg[0] |= 0x80000000;
    {
        // COMPILER-DIFF: candidate (sched2 issue-slot filler). The target's block 0 leaves the second
        // slot of cycle 3 empty (only `lis pPL@ha`) although the free `li 0` inits and the hoisted
        // `lis` are ready, so every filler lands one slot later than ours. A codeless asm on the call
        // argument (ready one cycle after `li r3,22`, priority = the argument's) takes a slot before
        // the fillers without moving any real insn.
        u32 n;
        asm("" : "=r"(n) : "0"(0x16));
        PSet(r225_work->crank, SmdGetObjPtr(n));
    }
    BitOn(r225_work->crank->be_flag, 0x20);
    ((cUnitEventView*) pPL)->beginEvent(0);
    PlSetHand(1, 0);
    ((cUnitEventView*) r225_work->crank)->beginEvent(0);
    CamCtrl.CutCall(5);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 3, 0, 5, (int) ROOM_ARC_PTR(pG->pRoom, 0x20));
    r225_work->crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2A), 3, 0, 5, (int) ROOM_ARC_PTR(pG->pRoom, 0x2B));
    {
        Vec pos = {82071.0f, 1500.0f, -18900.0f};
        cPlayer* pl;
        Vec* rot;

        // pPLS: the scalar `pPL` load would not depend on the template stores above it and its chain
        // through the in-struct `stfs pos.y` -> crank loads (unknown base) outranks the `r225_work` load.
        pos.y = pPLS->pos.y;
        FSetP(pPL->ang.y, r225_work->crank->ang.y - 1.5707964f);
        pl = pPL;
        rot = &pl->ang;
        pl->setPos(&pos);
        pl->setAng(rot);
    }
    key = &Key;
    // `if (a && !b) {body} else break;` and the early `if (!(x < 800)) { gnd_open(); break; }`: the
    // fall-through body is the in-line path (the target's gnd_open arm sits at the loop's end).
    while (1) {
        CamCtrl.CutCall(5);
        if (MotionCheckCrossFrame(&pPL->Motion, 0.0f) == 1 || MotionCheckCrossFrame(&pPL->Motion, 50.0f) == 1 ||
            MotionCheckCrossFrame(&pPL->Motion, 100.0f) == 1) {
            SndCall(6, 0x35, &pPL->pos, 0, 0, 0);
            SndCall(6, 4, &pPL->pos, 0, 0, 0);
        }
        if ((PlGetStatus() & 0x20000) && !(key->trg & 0x40000000)) {
            int lvl;
            void* mot;
            void* mot2;

            acc++;
            if (acc > 8) {
                acc = 8;
                spd -= 5;
                if (spd < 0) {
                    spd = 0;
                }
            }
            // COMPILER-DIFF: candidate (gcse table size / global-alloc tie window). Three dead tests
            // (`lvl` is redefined below; compare + branch survive to flow2) add 12 insns before gcse:
            // the PRE table grows from 223 to 229 buckets, which puts the hoisted `CamCtrl@ha` pseudo
            // before `.LC27@ha`/`r225_work@ha` (their priorities tie: 3 refs, lengths within 14 insns),
            // giving the target's r19/r18/r17. Fewer or simpler tests miss the tie window.
            if (spd == 0x12345) lvl = 0;
            if (spd == 0x23456) lvl = 0;
            if (spd == 0x34567) lvl = 0;
            lvl = spd / 20;
            if (lvl > 7) {
                lvl = 7;
            }
            if (lvl != cur) {
                u32 frame;
                u32 max;
                f32 ratio;

                cur = lvl;
                switch (lvl) {
                default:
                case 0:
                    mot = ROOM_ARC_PTR(pG->pRoom, 0x20);
                    mot2 = ROOM_ARC_PTR(pG->pRoom, 0x2B);
                    break;
                case 1:
                    mot = ROOM_ARC_PTR(pG->pRoom, 0x21);
                    mot2 = ROOM_ARC_PTR(pG->pRoom, 0x2C);
                    break;
                case 2:
                    mot = ROOM_ARC_PTR(pG->pRoom, 0x22);
                    mot2 = ROOM_ARC_PTR(pG->pRoom, 0x2D);
                    break;
                case 3:
                    mot = ROOM_ARC_PTR(pG->pRoom, 0x23);
                    mot2 = ROOM_ARC_PTR(pG->pRoom, 0x2E);
                    break;
                case 4:
                    mot = ROOM_ARC_PTR(pG->pRoom, 0x24);
                    mot2 = ROOM_ARC_PTR(pG->pRoom, 0x2F);
                    break;
                case 5:
                    mot = ROOM_ARC_PTR(pG->pRoom, 0x25);
                    mot2 = ROOM_ARC_PTR(pG->pRoom, 0x30);
                    break;
                case 6:
                    mot = ROOM_ARC_PTR(pG->pRoom, 0x26);
                    mot2 = ROOM_ARC_PTR(pG->pRoom, 0x31);
                    break;
                case 7:
                    mot = ROOM_ARC_PTR(pG->pRoom, 0x27);
                    mot2 = ROOM_ARC_PTR(pG->pRoom, 0x32);
                    break;
                }
                // COMPILER-DIFF: candidate (loop.c pass-2 threshold). The 2^52 conversion magic must
                // stay in the loop; with 262 pass-2 insns and threshold 71 it is hoisted (4*71 >= 262).
                // This dead product (flow1 deletes it, `ratio` is redefined below) supplies two
                // invariants that are moved first -- `high(3.7)` and the forced pool load -- so the
                // threshold reaches 65 (4*65 = 260 < 264) before the magic is considered.
                ratio = pPL->pos.y * 3.7f;
                max = *(u16*) mot;
                ratio = pPL->frame / (f32) pPL->frameMax;
                frame = (u32) ((f32) max * ratio);
                frame++;
                if (frame >= max) {
                    frame = 0;
                }
                pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 3, (u16) frame, 5, (int) mot);
                r225_work->crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2A), 3, (u16) frame, 5, (int) mot2);
            }
            SmdGetObjPtr(0x27)->be_flag |= 0x20;
            if (!(SmdGetObjPtr(0x27)->pos.x < 800.0f)) {
                gnd_open();
                break;
            }
            SmdGetObjPtr(0x27)->pos.x += (f32) (lvl + 1) * 1.2f;
            if (key->trg & 0x80000) {
                spd += acc;
                acc = 0;
                if (spd > 159) {
                    spd = 159;
                }
            }
            ActBtn.set(0x2A, 5, 0, 0, 2, 2, 0, 0);
            SceSleep(1);
        } else {
            break;
        }
    }
    r225_work->crank->motionPause();
    PlSetHand(0, 0);
    ((cUnitEventView*) pPL)->endEvent(0);
    ((cUnitEventView*) r225_work->crank)->endEvent(0);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtSetEnable(4, 1);
    } else {
        SceAtSetEnable(4, 0);
    }
    CamCtrl.Comeback(0);
    pG->Room_flg[0] &= ~0x80000000;
}

// The key door slides open (with a dummy copy of the door model riding along).
void r225_open_door()
{
    f32 spd;
    f32 max;

    ScfFlagOn(pG, SCF_85);
    SceEventStart(0);
    CamCtrl.CutCall(9);
    SceSleep(15);
    {
        Vec pos = {78329.0f, 3145.0f, -22522.0f};
        Vec rot = {1.5707964f, 1.5707964f, 0.0f};
        r225_work->door = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x33), ROOM_ARC_PTR(pG->pRoom, 0x34), &pos, &rot, 0x10, 1);
    }
    EstSet(0, -1, 0, 0, 1, 0, 1, 2, 0, 0);
    SndCall(6, 3, 0, 0, 0, 0);
    SceSleep(30);
    spd = 0.0f;
    max = 50.0f;
    SndCall(6, 1, 0, 0, 0, 0);
    SmdGetObjPtr(0x25)->be_flag |= 0x20;
    while (SmdGetObjPtr(0x25)->pos.y < 4579.0f) {
        spd += (max - spd) * 0.1f;
        FAdd(SmdGetObjPtr(0x25)->pos.y, spd);
        if (r225_work->door) {
            r225_work->door->pos.y += spd;
        }
        SceSleep(1);
    }
    SndCall(6, 2, 0, 0, 0, 0);
    SceSleep(25);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    EffectEspDelete(0, 2, 0, 0);
    EffectEspgenDelete(0, 2, 0);
    EffectEfmDelete(0, 2, 0);
    SceAtSetEnable(0xD, 0);
    SceAtSetEnable(0xC, 0);
}

// Waits for the key item to be used, then opens the door.
static void r225_DoorMes_exec()
{
    while (ItemMgr.check(0x82) != 1) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 1);
    r225_open_door();
}

// Area 12: the door message; the item screen when Leon has the key.
static void r225_DoorMes()
{
    SceUpCut(0, -1, 6, 0);
    if (ItemMgr.num(0x82) != 0) {
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
    }
}

// The grave slab (object 0x2E) sinks (dir 1) or rises back (dir 0) with Leon on it.
static void r225_moveGrave(int dir)
{
    cObj* obj = SmdGetObjPtr(0x2E);

    if (obj) {
        pPL->setNoSuspend(1);
        Vec d = {0.0f, -3000.0f, 0.0f};
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
            CamCtrl.CutCall(6);
            SndCall(6, 7, 0, 0, 0, 0);
            for (i = 0; i < 90; i++) {
                grave.move();
                if (i == 60) {
                    FadeSetW(1, 30, 0, 0);
                }
                SceSleep(1);
            }
        } else {
            CamCtrl.CutCall(0xA);
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

// Area 0: the grave goes down and the room changes.
static void r225_checkGrave()
{
    r225_moveGrave(1);
    SceAtExecute(0);
}

// End of the first look: SceEventEnd; the room stream stopped if it had started (Room_flg[0] 0x40000000).
static void first_cut_exit()
{
    SceEventEnd(0);
    if (pG->Room_flg[0] & 0x40000000) {
        SndRoomStrStop(0);
    }
}

// Area 16: the first look at the graveyard (cut 12).
static void first_cut()
{
    RsfSet(G_ROOM_ID, 3);
    SceEventStart(0);
    SndRoomStrStart(1, 0, 1);
    SceSetEventCancel(1, (TaskFunc) first_cut_exit, 0, 1, 1);
    CamCtrl.CutCall(0xC);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    first_cut_exit();
}

// sce_com's SceElevator without the flags_5014 bit and with the chapter end when leaving the
// first time (room save flag 4).
void SceElevator_r225(SceElevatorData* d)
{
    cPlayer* pl = pPL;
    cObj* obj;
    f32 accel;
    f32 maxSpd;
    f32 minSpd;
    f32 stopDist;
    f32 stopDist2;
    f32 spd;
    f32 step;
    f32 move;
    int faded;
    int done;
    FadeWork* fade;
    u32 white;
    int i;
    int j;
    u32 hSnd;

    obj = SmdGetObjPtr(d->objId);
    if (obj == 0) {
        return;
    }
    maxSpd = 100.0f;
    minSpd = 10.0f;
    accel = 2.0f;
    stopDist = CalcStopDist(maxSpd, accel);
    stopDist2 = stopDist + 4000.0f;
    SceEventStart(0);
    faded = 0;
    done = 0;
    obj->setNoSuspend(1);
    obj->setPos(&d->pos);
    pPL->setNoSuspend(1);
    ((cUnitEventView*) pPL)->beginEvent(0);
    pPL->setPos(&d->plPos);
    pPL->setAng(&d->plRot);
    pPL->be_flag &= ~0x10;
    if (d->cut != -1) {
        CamCtrl.CutCall((s8) d->cut);
    }
    if (d->dir == 1 || d->dir == 3) {
        SndCall(6, d->seStart, &obj->pos, 0, 0, 0);
        spd = accel;
        for (i = 0; i < 10; i++) {
            obj->setPos(&d->pos);
            pPL->setPos(&d->plPos);
            SetPosXYZ(obj, obj->pos.x, fRand1_1() * 10.0f + obj->pos.y, obj->pos.z);
            SetPosXYZ(pPL, pPL->pos.x, fRand1_1() * 10.0f + pPL->pos.y, pPL->pos.z);
            SceSleep(1);
        }
        obj->setPos(&d->pos);
        pPL->setPos(&d->plPos);
        // Up loop: a noted loop that loop.c does not process (entered by the goto below = "multiple
        // entry points"), laid out `b TOP; SLEEP: SceSleep; spd += accel; TOP: ...`. The loop notes
        // give flow's depth-2 ref weights (accel above minSpd in the FPR order, the faded compare's
        // CC pseudo allocated before `done` -> cr4) and update_equiv_regs leaves `white` in bb 7;
        // loop.c must stay out or it single-usage-replaces the pG/RoomData highs of the RsfCheck
        // block with the gcse reaching registers (a 14th GPR). `fade`/`white` computed here are the
        // target's `lis/addi &Fade[2]` and `li 255` before the loop.
        fade = &Fade[2];
        white = 0xFF;
        goto up_top;
        for (;;) {
            SceSleep(1);
            // COMPILER-DIFF: candidate (sched1 loop-note barrier). The target issues `spd += accel`
            // after the SceSleep call; sched1 only keeps it there behind a loop note.
            do { } while (0);
            spd += accel;
        up_top:
            // COMPILER-DIFF: candidate #12 (reverse). The target's pPL load of the second SetPosXYZ
            // uses the gcse reaching register of high(pPL) (`lwz r9,pPL@l(r23)`) while ours
            // re-materialises `lis`; a dead pPL read at the loop top shares its high with that
            // load, so the final gcse cprop substitutes the reaching register there.
            cPlayer* p = pPL;
            if (spd > maxSpd) {
                spd = maxSpd;
            }
            step = spd;
            if (d->dir == 1) {
                step = -spd;
            }
            SetPosXYZ(obj, obj->pos.x, obj->pos.y + step, obj->pos.z);
            SetPosXYZ(pPL, pPL->pos.x, pPL->pos.y + step, pPL->pos.z);
            if (faded == 0) {
                if (spd >= maxSpd) {
                    FadeSetRGBA(2, 0, white);
                    faded = 1;
                }
            } else if ((fade->flags & 1) == 0) {
                if (RsfCheck(G_ROOM_ID, 4) == 0) {
                    // COMPILER-DIFF: candidate #12 (fallthrough-arm form). Our cse carries the
                    // RsfCheck block's pG/RoomData highs into this arm (and cse2 through a
                    // do-while(0)); the original re-materialises both `lis` here. The bare
                    // volatile asm flushes cse's table in both passes.
                    asm volatile("" : : "r"(d));
                    RsfSet(G_ROOM_ID, 4);
                    SceEventEnd(0);
                    SceSetChapterEnd(CHAPTER_4_3, 1);
                    for (;;) {
                        SceSleep(1);
                    }
                }
                SceAtExecRoomJump(d->room, &d->jumpPos, &d->jumpRot, 0);
                break;
            }
        }
    }
    if (d->dir == 0 || d->dir == 2) {
        StaFlagOff(pG, STA_SUSPEND);
        spd = maxSpd;
        move = stopDist2;
        if (d->dir == 0) {
            move = -move;
        }
        SetPosXYZ(obj, obj->pos.x, obj->pos.y + move, obj->pos.z);
        SetPosXYZ(pPL, pl->pos.x, pPL->pos.y + move, pl->pos.z);
        CamCtrl.Comeback(0);
        FadeSetRGBA(0x80000002, 0xFF, 0);
        hSnd = SndCall(6, d->seStart, &obj->pos, 0, 0, 0);
        // Down loop: `for (;;) { body; if (done) { tail; break; } SceSleep(1); }` -- expand_end_loop
        // rotates it (`b TOP; SLEEP; TOP: body; beq SLEEP`), the gcse insertions before the entry
        // jump land after LOOP_BEG (loop.c: "phony", the in-loop `lis pG` stays) and behind the
        // sched1 note barrier (`addi r29,r1,8` after the SndCall), and the tail inside the loop puts
        // LOOP_END before the shake preheader (`lis/li` after the setAng call). `y` is only the
        // fabs operand (__builtin_fabsf: the volatile asm would block the `fmr f12,f13` copy of the
        // PRE'd `obj->pos.y` re-read), `move` is a second step variable so `step` dies in the up
        // loop and `spd` stays cse-canonical there (`fneg f31,f30`).
        for (;;) {
            f32 y = obj->pos.y;
            if (__builtin_fabsf(d->pos.y - y) < stopDist) {
                spd -= accel;
                if (spd < minSpd) {
                    spd = minSpd;
                }
            }
            move = spd;
            if (d->dir != 0) {
                move = -move;
            }
            SetPosXYZ(obj, obj->pos.x, obj->pos.y + move, obj->pos.z);
            SetPosXYZ(pPL, pPL->pos.x, pPL->pos.y + move, pPL->pos.z);
            {
                Vec q = {0.0f, 0.0f, 0.0f};
                q.y = move;
                pG->quake_ofs = q;
            }
            done = 0;
            if (d->dir == 0) {
                if (obj->pos.y >= d->pos.y) {
                    done = 1;
                }
            }
            if (d->dir == 2) {
                if (obj->pos.y <= d->pos.y) {
                    done = 1;
                }
            }
            if (done != 0) {
                if (hSnd) {
                    SndStop(hSnd, 0);
                }
                SndCall(6, d->seStop, &obj->pos, 0, 0, 0);
                obj->setPos(&d->pos);
                pPL->setPos(&d->plPos);
                pPL->setAng(&d->plRot);
                break;
            }
            SceSleep(1);
        }
        for (j = 0; j < 10; j++) {
            obj->setPos(&d->pos);
            pPL->setPos(&d->plPos);
            SetPosXYZ(obj, obj->pos.x, fRand1_1() * 10.0f + obj->pos.y, obj->pos.z);
            SetPosXYZ(pPL, pPL->pos.x, fRand1_1() * 10.0f + pPL->pos.y, pPL->pos.z);
            SceSleep(1);
        }
        obj->setPos(&d->pos);
        pPL->setPos(&d->plPos);
    }
    pPL->be_flag |= 0x10;
    SceEventEnd(0);
    SceExit();
}
