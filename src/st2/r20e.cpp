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
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "motion.h"
#include "math_sub.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "shadow.h"
#include "item.h"
#include "sscrn.h"
#include "db_log.h"

// Room 2-0E (D:/Bio4/Prog/r20e.cpp): the maze with its three switch-driven fences, the sliding
// picture puzzle behind the crest door, the armor statues and the treasure shelves/boxes.

// One fence of the maze: lifted (state 1) or lowered (state 0) by the switch tasks.
class cFence20e {
public:
    cObj* obj;     // 0x00
    int state;     // 0x04  1 = up
    int force;     // 0x08  1 = snap to the end position on the next move
    int active;    // 0x0C  moving
    f32 baseY;     // 0x10  lowered height
    f32 spd;       // 0x14  fall speed
    int atNo;      // 0x18  sce_at area of the fence
    int camNo;     // 0x1C  cut shown while it moves
    int flagNo;    // 0x20  room save flag

    void move();
    void set(int no, int force);
    int check();
    int checkActive();
    int getCamNo();
    void init(const struct R20eFenceData* d);
};

struct R20eFenceData {
    u32 objId;    // 0x00
    int atNo;     // 0x04
    int camNo;    // 0x08
    int flagNo;   // 0x0C
};

// One puzzle piece: the scroll object and its slide towards `target`.
struct R20ePiece {
    int state;     // 0x00  0 idle, 1 start sliding, 2 sliding
    int visible;   // 0x04
    cObj* obj;     // 0x08
    Vec target;    // 0x0C
    Vec spd;       // 0x18
    int cnt;       // 0x24  frames left
};

struct R20eCell {
    Vec pos;       // 0x00
    s8 piece;      // 0x0C  piece in this cell, -1 = empty
    u8 pad_D[3];
};

struct R20ePuzzle {
    int cx;                  // 0x00  cursor
    int cy;                  // 0x04
    int hidden;              // 0x08  the piece taken out of the board
    cObj* frame;             // 0x0C  cursor frame object
    R20ePiece piece[9];      // 0x10
    R20eCell cell[3][3];     // 0x178  [x][y]
};

struct R20eWork {
    u8 pad_0[0x14];
    R20ePuzzle puzzle;       // 0x14
    f32 crestDoorY;          // 0x21C
    u8 pad_220[0xC];
    cFence20e fence[3];      // 0x22C
    cEm* rack;               // 0x298
    int effKind;             // 0x29C
    u32 snd;                 // 0x2A0
};

static R20eWork* r20e_work;

// `&p->piece[i]` as integer arithmetic, index first (`mulli; add idx, p; addi 0x10`): the array
// subscript folds the 0x10 into the product and puts the pointer first in the add.
#define PUZZLE_PIECE(p, i) ((R20ePiece*) ((i) * sizeof(R20ePiece) + (u32) (p) + 0x10))
// `&p->cell[x][y]` as integer arithmetic, `(x*48 + 0x178) + p + y*16`: fold moves the constant to
// the other operand (`(V+C)+A -> V+(A+C)`), so the tree is `x*48 + (p + 0x178) + y*16`, which is what
// the puzzleMove loops need: `PUZZLE_CELL(p, 0, cy)` gives `p + (cy*16 + 0x178)` (`addi r0,r11,0x178;
// add r9,r30,r0`), `PUZZLE_CELL(p, cx, 0)` gives `cx*48 + (p + 0x178)`, and in the slide loops
// expand's EXPAND_SUM association makes the row `to` cell `(cy16 + (k48 + p)) + 0x148` (same operand
// order as `from`, so reload_cse turns the second `add` into `mr r9,r11`) but the column `to` cell
// `((cx48 + p) + k16) + 0x168` (the other order: a separate `add r9,r9,r6`).  The `y*16` is never
// grouped with 0x178 (`(k-1)*16` must distribute to `k*16 - 16` under EXPAND_SUM).
#define PUZZLE_CELL(p, x, y) ((R20eCell*) ((x) * sizeof(R20eCell[3]) + 0x178 + (u32) (p) + (y) * sizeof(R20eCell)))

// The address of the caller's Vec goes straight into the argument register (no PRE'd pseudo).
static inline void SetAngV(cModel* m, Vec* v)
{
    m->setAng(v);
}

// Store through a scalar reference: pPL is reloaded for the following call (r22a idiom).
static inline void FSetP(f32& d, f32 v) { d = v; }
// Atari flag stores through the info's address (r207 idiom): `addi r9, pl, 0x2B4` + lhz/sth 0x1A(r9).
static inline void AtariFlagsAnd(cAtariInfo* a, u16 mask) { a->m_flag &= mask; }
static inline void AtariFlagsOr(cAtariInfo* a, u16 bit) { a->m_flag |= bit; }
// Struct view of pPL: cse invalidates an in-struct pPL load at the following in-struct flags store
// (true_dependence), so the next `pPL->atari` reloads pPL and recomputes the address (the plain
// scalar load survives the store and gets cse'd into `mr r3, r9`).
struct PlPtr {
    cPlayer* p;
};
#define pPLS (((PlPtr*) &pPL)->p)

struct R20eThrough {
    Vec pos;       // 0x00
    f32 angY;      // 0x0C
    f32 dist;      // 0x10
    int x14;       // 0x14
};

static R20eFenceData r20e_fenceTbl[3] = {
    {0x37, 0xE, 0x19, 5},
    {0x38, 0xF, 0x17, 6},
    {0x39, 0x10, 0x18, 7},
};

static const R20eThrough r20e_throughTbl[2] = {
    {{16154.0f, 0.0f, 29417.0f}, 3.1415927f, 1500.0f, -1},
    {{15967.0f, 0.0f, 27508.0f}, 0.0f, 1500.0f, -1},
};
static const u32 r20e_pieceObjId[3][3] = {{0x17, 0x18, 0x19}, {0x1A, 0x1B, 0x1C}, {0x1D, 0x1E, 0x1F}};
static const s8 r20e_initLayout[3][3] = {{1, 4, 0}, {2, -1, 3}, {5, 7, 6}};
static const s8 r20e_solvedLayout[3][3] = {{0, 3, 6}, {1, 4, 7}, {2, 5, -1}};

void r20e_getFenceNo(int sw, int* up, int* down);
static void r20e_checkSwitch_end(int sw);
static void r20e_checkSwitch(int sw);
static void r20e_checkEnableSwitch3();
void r20e_initMaze();
static void r20e_execThrough(int no);
static void r20e_moveCrestDoor(int open, int init);
void r20d_moveArmorStatue(int noAnim);
static void r20d_getSnakeObject();
void r20e_startArmor();
static void r20d_getSalazarCrest_end();
static void r20d_getSalazarCrest();
static void r20e_checkFinalPieceUse_end();
static void r20e_checkFinalPieceUse();
static void r20d_checkPuzzle2();
static void r20d_checkPuzzle();
void r20e_initPuzzle();
void r20e_openBox_main(int no, int opened);
static void r20e_openedBox(int no);
static void r20e_openBox(int no);
void r20e_openShelf_main(int no, int opened);
static void r20e_openedShelf(int no);
static void r20e_openShelf(int no);

// Room init (the maze / puzzle room of Ashley's section): larger shadow pool, the sliding picture
// puzzle; in Ashley's section (pl_type 1) area 6 = taking the Salazar crest (item 0x80, lit) until it
// is taken, the snake item 0x85 scaled down; the crest door, the armor statues and knights per the saved
// flags; the maze fences and switches; shelf / box item events.
void R20eInit()
{
    cObj* obj;

#line 51 "D:/Bio4/Prog/r20e.cpp"
    r20e_work = (R20eWork*) MEM_CALLOC(sizeof(R20eWork), 1, 0xd);
    ShadowMngReAlloc(0x100);
    r20e_initPuzzle();
    if (pG->pl_type == 1) {
        if (SceAtItemFlgCk(0x80) == 0) {
            cModel* m;

            SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r20d_getSalazarCrest, 0, 1);
            SceAtSetEnable(0x80, 1);
            m = SceAtItemModelPtr(0x80);
            m->LightInfo.EnableMask |= 4;
            SceAtSetEnable(0x85, 1);
            m = SceAtItemModelPtr(0x85);
            if (m) {
                Vec sca = {0.55f, 0.55f, 0.55f};

                m->setSca(&sca);
            }
            SceAtSetEnable(0x85, 0);
        } else {
            r20d_moveArmorStatue(1);
            r20e_startArmor();
            r20e_openedBox(1);
        }
        r20e_initMaze();
    }
    SceSetItemEvent(2, 0x81, 0, 2, (void (*)(int)) r20e_openShelf, (void (*)()) r20e_openedShelf, 0, 0);
    SceSetItemEvent(3, 0x83, 1, 3, (void (*)(int)) r20e_openShelf, (void (*)()) r20e_openedShelf, 1, 0);
    SceSetItemEvent(4, 0x82, 2, 4, (void (*)(int)) r20e_openBox, (void (*)()) r20e_openedBox, 0, 0);
    SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r20e_execThrough, 0, 1);
    SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r20e_execThrough, (void*) 1, 1);
    obj = SmdGetObjPtr(0x43);
    if (obj) {
        obj->be_flag &= ~2;
    }
    if (SceAtItemFlgCk(0x89) == 0) {
        cModel* m;

        SceAtSetEnable(0x89, 1);
        m = SceAtItemModelPtr(0x89);
        m->setNoSuspend(1);
        m->LightInfo.EnableMask |= 4;
    }
    if (getRoomEtcRack(8, &r20e_work->rack, 1)) {
        ((cEmRack*) r20e_work->rack)->setRange(0.0f, 1180.0f, 0.0f, 5000.0f);
    }
    if (pG->pl_type == 1) {
        EstSet((int) pPL, -1, 0, 0, 1, 2, 1, 0, 0, 0);
    }
    r20e_work->effKind = EspPullCoreKind();
}

// Per-frame room main: nothing.
void R20eMain()
{
}

// Per-frame fence step while active: state 1 rises 50 units a frame to base + 2200 (then its area off),
// state 0 drops with growing speed back to base (then its area on).
void cFence20e::move()
{
    if (obj && active) {
        if (state == 1) {
            f32 up = 2200.0f;
            f32 y = obj->pos.y + 50.0f;

            // COMPILER-DIFF: #5 (codeless sched1 slot): local-alloc marks a qty's FAKE lifetime
            // (birth-2 .. death+2), so the 2200 high may share the dead 50.0 high's r9 only if its
            // `lis` is issued two insns after `lfs f13` in sched1.  This anchor (no code; an output
            // dependence on the `stfs`, prio 9) takes the second issue slot of that cycle ahead of
            // the `lis` (prio 6); sched2 then puts `lis r9` after `lfs f13` and `lfs f12` after `fadds`.
            asm("" : "=m"(spd));
            obj->pos.y = y;
            if (obj->pos.y > baseY + up || force == 1) {
                obj->pos.y = baseY + up;
                SceAtSetEnable(atNo, 0);
                active = 0;
            }
        } else {
            spd += 20.0f;
            obj->pos.y -= spd;
            if (obj->pos.y < baseY || force == 1) {
                obj->pos.y = baseY;
                active = 0;
            }
        }
        force = 0;
    }
}

// Request state `no` (1 up / 0 down); frc = 1 snaps on the next move. Mirrors the state into the room
// save flag flagNo and re-enables the fence's collision area when lowering.
void cFence20e::set(int no, int frc)
{
    force = frc;
    if (state != no) {
        active = 1;
        spd = 0.0f;
        state = no;
        if (no == 0) {
            SceAtSetEnable(atNo, 1);
            RsfClear(G_ROOM_ID, flagNo);
        } else {
            RsfSet(G_ROOM_ID, flagNo);
        }
    }
}

// The fence's target state (1 up, 0 down).
int cFence20e::check()
{
    return state;
}

// 1 while the fence is still moving.
int cFence20e::checkActive()
{
    return active;
}

// The camera cut that shows this fence moving.
int cFence20e::getCamNo()
{
    return camNo;
}

// Bind a maze fence from its table entry (object, area, camera cut, save flag) and snap it to the saved state.
void cFence20e::init(const R20eFenceData* d)
{
    obj = SmdGetObjPtr(d->objId);
    if (obj) {
        obj->be_flag |= 0x20;
        state = 0;
        active = 0;
        baseY = obj->pos.y;
        atNo = d->atNo;
        camNo = d->camNo;
        force = 0;
        flagNo = d->flagNo;
        if (RsfCheck(pGS->room_id, flagNo)) {
            set(1, 1);
        } else {
            set(0, 1);
        }
        move();
    }
}

// The fences a switch lifts / lowers.
void r20e_getFenceNo(int sw, int* up, int* down)
{
    switch ((u32) sw) {
    case 0:
        *up = 0;
        *down = 2;
        break;
    case 1:
        *up = 1;
        *down = 2;
        break;
    case 2:
        *up = 2;
        *down = 0;
        break;
    }
}

// End of a switch operation (also its cancel path): the raised / lowered fences snapped, the SE stopped,
// Ashley may suspend, camera back, SceEventEnd.
static void r20e_checkSwitch_end(int sw)
{
    int up = 0;
    int down = 0;

    r20e_getFenceNo(sw, &up, &down);
    r20e_work->fence[up].set(1, 1);
    r20e_work->fence[up].move();
    r20e_work->fence[down].set(0, 1);
    r20e_work->fence[down].move();
    if (r20e_work->snd) {
        SndStop(r20e_work->snd, 0);
    }
    pPL->setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Maze switch `sw` (areas 9/0xA/0xB): yes/no message 3; yes -> the switch's fence pair moves (one up, one
// down) under their camera cuts with SEs; player-cancellable.
static void r20e_checkSwitch(int sw)
{
    SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case 1:
    default: {
        int up = 0;
        int down = 0;

        r20e_getFenceNo(sw, &up, &down);
        SndCall(6, 2, 0, 0, 0, 0);
        SceEventStart(1);
        r20e_work->snd = 0;
        SceSetEventCancel(1, (TaskFunc) r20e_checkSwitch_end, sw, -1, 1);
        pPL->setNoSuspend(1);
        if (r20e_work->fence[up].check() == 0) {
            int act;

            CamCtrl.CutCall((s8) r20e_work->fence[up].getCamNo());
            SceSleep(15);
            U32Set(r20e_work->snd, SndCall(6, 3, 0, 0, 0, 0));
            r20e_work->fence[up].set(1, 0);
            while (r20e_work->fence[up].move(), (act = r20e_work->fence[up].checkActive()) != 0) {
                SceSleep(1);
            }
            SndCall(6, 4, 0, 0, 0, 0);
            r20e_work->snd = 0;
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            SceSleep(15);
            CamCtrl.Comeback(0);
        }
        if (r20e_work->fence[down].check() == 1) {
            int act;

            CamCtrl.CutCall((s8) r20e_work->fence[down].getCamNo());
            if (sw == 1) {
                if (pPL->pos.z < 25200.0f) {
                    FSetP(pPL->pos.z, 25200.0f);
                    pPL->setPos(&pPL->pos);
                    pPL->be_flag |= 0x200000;
                }
            }
            SceSleep(15);
            U32Set(r20e_work->snd, SndCall(6, 5, 0, 0, 0, 0));
            r20e_work->fence[down].set(0, 0);
            while (r20e_work->fence[down].move(), (act = r20e_work->fence[down].checkActive()) != 0) {
                SceSleep(1);
            }
            SndCall(6, 6, 0, 0, 0, 0);
            r20e_work->snd = 0;
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            SceSleep(15);
            CamCtrl.Comeback(0);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r20e_checkSwitch_end(sw);
        break;
    }
    case -1:
    case 0:
    case 2:
        break;
    }
}

// Switch 3 works only while the rack is off its area.
static void r20e_checkEnableSwitch3()
{
    int on = 0;

    SceAtSetEnable(0xB, 0);
    SceSleep(1);
    for (;;) {
        if (on == 0) {
            if (SceAtCheckHitModel(0x12, r20e_work->rack) == 0) {
                SceAtSetEnable(0xB, 1);
                on = 1;
            }
        } else {
            if (SceAtCheckHitModel(0x12, r20e_work->rack) == 1) {
                SceAtSetEnable(0xB, 0);
                on = 0;
            }
        }
        SceSleep(1);
    }
}

// The three switch areas, the switch-3 enable watcher and the three fences from r20e_fenceTbl.
void r20e_initMaze()
{
    u32 i;

    SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r20e_checkSwitch, 0, 1);
    SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r20e_checkSwitch, (void*) 1, 1);
    SceAtDataSet_exec(0xB, SCE_LEVEL10, 0, (TaskFunc) r20e_checkSwitch, (void*) 2, 1);
    SceExec(0x12, (TaskFunc) r20e_checkEnableSwitch3, 0, 0, SCE_PRIO_DEF_2, 0);
    for (i = 0; i < 3; i++) {
        r20e_work->fence[i].init(&r20e_fenceTbl[i]);
    }
}

// The player climbs through one of the two maze openings.
static void r20e_execThrough(int no)
{
    cPlayer* pl = pPL;
    // pool order: the 10.0 of the second MotionCheckCrossFrame comes first
    const f32 frame10 = 10.0f;
    const R20eThrough* t;
    Vec d;
    f32 step;
    u32 i;

    pl->beginAction();
    AtariFlagsAnd(&pPLS->atari, 0xFEFF);
    pPLS->atari.setPriority(PRI_LV1);
    pPL->dmg.set(0, 0x80);
    t = &r20e_throughTbl[no];
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 10, 0, 0x201, 0);
    PSVECSubtract(&t->pos, &pPL->pos, &d);
    PSVECScale(&d, &d, 0.1f);
    step = Muku2(pPL->ang.y, t->angY, 3.1415927f) * 0.1f;
    for (i = 0; i < 10; i++) {
        cPlayer* p;
        f32 ry;

        PSVECAdd(&pPL->pos, &d, &pPL->pos);
        FAdd(pPL->ang.y, step);
        p = pPL;
        ry = p->ang.y;
        p->setPos(&p->pos);
        {
            Vec ang;

            ang.x = 0.0f;
            ang.y = ry;
            ang.z = 0.0f;
            SetAngV(p, &ang);
        }
        SceSleep(1);
    }
    {
        cPlayer* p;
        f32 ry;

        ry = t->angY;
        p = pPL;
        p->setPos((Vec*) &t->pos);
        {
            Vec ang;

            ang.x = 0.0f;
            ang.y = ry;
            ang.z = 0.0f;
            SetAngV(p, &ang);
        }
    }
    while (MotionGetState(pPL) != 4) {
        SceSleep(1);
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 5, 0, 0x205, 0);
    {
        f32 dist2 = t->dist * t->dist;

        do {
            if (MotionCheckCrossFrame(&pPL->Motion, 0.0f) == 1) {
                SndCall(6, 0xE, 0, 0, 0, 0);
            }
            if (MotionCheckCrossFrame(&pPL->Motion, frame10) == 1) {
                SndCall(6, 0xD, 0, 0, 0, 0);
            }
            // `if (!c) {...} else break;` keeps the loop un-rotated (the exit jump targets the
            // else label, not the loop end)
            if (!(PSVECSquareDistance(&t->pos, &pPL->pos) > dist2)) {
                SceSleep(1);
            } else {
                break;
            }
        } while (1);
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 10, 0, 0x201, 0);
    while (MotionGetState(pPL) != 4) {
        SceSleep(1);
    }
    pl->endAction(8);
    pPL->dmg.clear();
    AtariFlagsOr(&pPLS->atari, 0x100);
    pPLS->atari.setPriority(0);
}

// The crest door of the puzzle room: `open` 1 lifts it, `init` 1 places it without animation.
static void r20e_moveCrestDoor(int open, int init)
{
    cObj* obj;

    obj = SmdGetObjPtr(0x16);
    if (obj) {
        obj->be_flag |= 0x20;
        if (open == 1) {
            ScfFlagOn(pG, SCF_7c);
        } else {
            ScfFlagOff(pG, SCF_7c);
        }
        if (init == 1) {
            r20e_work->crestDoorY = obj->pos.y;
            if (open == 1) {
                SceAtSetEnable(5, 0);
                obj->pos.y += 3000.0f;
            }
        } else if (open == 1) {
            u32 i;

            EstSet(0, -1, 0, 0, 1, 0, 1, (u8) r20e_work->effKind, 0, 0);
            U32Set(r20e_work->snd, SndCall(6, 0x24, 0, 0, 0, 0));
            for (i = 0; i < 75; i++) {
                obj->pos.y += 40.0f;
                SceSleep(1);
            }
            obj->pos.y = r20e_work->crestDoorY + 3000.0f;
            SndCall(6, 0x25, 0, 0, 0, 0);
            r20e_work->snd = 0;
            SceAtSetEnable(5, 0);
        } else {

            SceAtSetEnable(5, 1);
            SndCall(6, 0x26, 0, 0, 0, 0);
            {
                // pool order: the step before the 0.0 the speed starts from; declared after the
                // calls so the dead initialiser's `lis` is not live across them (else it is the
                // high of the peel's 15.0 load, hoisted into r30 above SndCall)
                const f32 add = 15.0f;
                f32 spd = 0.0f;

                // the compiler peels the first step (jump1 duplicate_loop_exit_test); FSub keeps
                // the work-pointer load below the pos.y store (a plain member store is a varying
                // struct store that never conflicts with the fixed scalar load)
                while (1) {
                    FSub(obj->pos.y, spd);
                    spd += add;
                    if (obj->pos.y < r20e_work->crestDoorY) {
                        obj->pos.y = r20e_work->crestDoorY;
                        break;
                    }
                    SceSleep(1);
                }
            }
            SndCall(6, 0x27, 0, 0, 0, 0);
        }
    }
}

// The two armor statues turn to face the room (`noAnim` 1: already turned).
void r20d_moveArmorStatue(int noAnim)
{
    cObj* o23;
    cObj* o24;

    o23 = SmdGetObjPtr(0x23);
    o24 = SmdGetObjPtr(0x24);
    if (o23 && o24) {
        o23->be_flag |= 0x20;
        o24->be_flag |= 0x20;
        if (noAnim != 1) {
            u32 i;

            EstSet(0, -1, 0, 0, 1, 1, 1, (u8) r20e_work->effKind, 0, 0);
            U32Set(r20e_work->snd, SndCall(6, 7, 0, 0, 0, 0));
            for (i = 0; i < 90; i++) {
                o23->pParts->ang.y += 0.034906585f;
                o24->pParts->ang.y += 0.034906585f;
                SceSleep(1);
            }
        }
        // weight lever: the two extra refs rank o23 above noAnim in global-alloc (o23 r31, noAnim r30,
        // o24 r29, i r28)
        do {
            o23->pParts->ang.y = 3.1415927f;
            o24->pParts->ang.y = 3.1415927f;
        } while (0);
    }
}

// Area 6 after the crest: camera cut 7 while the armor statue's box opens and the snake item (0x85) is
// taken; then the statues turn and the knights wake.
static void r20d_getSnakeObject()
{
    cModel* m;

    SceAtSetEnable(6, 0);
    SceAtSetEnable(0x85, 1);
    m = SceAtItemModelPtr(0x85);
    if (m) {
        m->setNoSuspend(1);
    }
    SceEventStart(0);
    LightMgr.onKind(0x7F);
    CamCtrl.CutCall(7);
    r20e_openBox(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    while (SceAtItemFlgCk(0x85) == 0) {
        SceSleep(1);
    }
    SceExec(0x12, (TaskFunc) r20e_moveCrestDoor, 1, 0, SCE_PRIO_DEF_2, 0);
    {
        cEmWrap em0;
        cEmWrap em1;
        cEmWrap em2;
        cEmWrap em3;
        cEmWrap em4;

        em0.setPtr(0xBB, 3, 0);
        em1.setPtr(0xBC, 3, 0);
        em0.destroy();
        em1.destroy();
        em2.setEm(0xC1, 3, 0, 1, 1);
        em3.setEm(0xC2, 3, 0, 1, 1);
        em4.setEm(0xB2, 3, 0, 1, 1);
    }
    r20e_startArmor();
    GameSaveSave(&GameSave, pSaveData, -1);
}

// The armor knights wake up.
void r20e_startArmor()
{
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;
    cEmWrap em3;
    cEmWrap em4;
    cEmWrap em5;
    cEmWrap em6;

    em0.setPtr(0xBD, 3, 0);
    em1.setPtr(0xBE, 3, 0);
    em2.setPtr(0xBF, 3, 0);
    em3.setPtr(0xC0, 3, 0);
    em4.setPtr(0xC1, 3, 0);
    em5.setPtr(0xC2, 3, 0);
    em6.setPtr(0xB2, 3, 0);
    em0.setFlag(1);
    em1.setFlag(1);
    em2.setFlag(1);
    em3.setFlag(1);
    em0.setBeFlag(0x10000, 1);
    em1.setBeFlag(0x10000, 1);
    em2.setBeFlag(0x10000, 1);
    em3.setBeFlag(0x10000, 1);
    em4.setBeFlag(0x10000, 1);
    em5.setBeFlag(0x10000, 1);
    em6.setBeFlag(0x10000, 1);
}

// End of the crest pickup event (also its cancel path, Room_flg[0] bit 31): drop the effect, stop the
// SE, snap the armor statues 0x23/0x24 turned (PI) and the crest door up; camera back, SceEventEnd.
static void r20d_getSalazarCrest_end()
{
    if (pG->Room_flg[0] & 0x80000000) {
        cObj* o23;
        cObj* o24;
        cObj* obj;

        EffectEspDelete(0, (u8) r20e_work->effKind, 0, 0);
        EffectEspgenDelete(0, (u8) r20e_work->effKind, 0);
        EffectEfmDelete(0, (u8) r20e_work->effKind, 0);
        if (r20e_work->snd) {
            SndStop(r20e_work->snd, 0);
        }
        o23 = SmdGetObjPtr(0x23);
        o24 = SmdGetObjPtr(0x24);
        if (o23) {
            o23->be_flag |= 0x20;
            o23->pParts->ang.y = 3.1415927f;
        }
        if (o24) {
            o24->be_flag |= 0x20;
            o24->pParts->ang.y = 3.1415927f;
        }
        obj = SmdGetObjPtr(0x16);
        if (obj) {
            obj->pos.y = r20e_work->crestDoorY;
        }
        SceAtSetEnable(5, 1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtSetEnable(6, 1);
    SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r20d_getSnakeObject, 0, 1);
}

// Area 6: camera cut 7, the crest item 0x80 is taken, then the armor statues turn with SE / effect and
// the crest door opens; player-cancellable.
static void r20d_getSalazarCrest()
{
    cModel* m;

    SceAtSetEnable(6, 0);
    m = SceAtItemModelPtr(0x80);
    if (m) {
        m->setNoSuspend(1);
    }
    SceEventStart(0);
    LightMgr.onKind(0x7F);
    CamCtrl.CutCall(7);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceAtExecute(0x80);
    while (SceAtItemFlgCk(0x80) == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(7);
    r20e_work->snd = 0;
    SceSetEventCancel(1, (TaskFunc) r20d_getSalazarCrest_end, 0, 0, 1);
    SceSleep(15);
    r20d_moveArmorStatue(0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(15);
    CamCtrl.CutCall(0x1A);
    r20e_moveCrestDoor(0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r20d_getSalazarCrest_end();
}

// End of the final-piece event (also its cancel path): effect / SE dropped, area 5 off, the crest door
// 0x16 snapped up 3000; camera back, SceEventEnd.
static void r20e_checkFinalPieceUse_end()
{
    if (pG->Room_flg[0] & 0x80000000) {
        cObj* obj;

        EffectEspDelete(0, (u8) r20e_work->effKind, 0, 0);
        EffectEspgenDelete(0, (u8) r20e_work->effKind, 0);
        EffectEfmDelete(0, (u8) r20e_work->effKind, 0);
        if (r20e_work->snd) {
            SndStop(r20e_work->snd, 0);
        }
        SceAtSetEnable(5, 0);
        obj = SmdGetObjPtr(0x16);
        if (obj) {
            obj->pos.y = r20e_work->crestDoorY + 3000.0f;
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The last piece is put in once the player has it.
static void r20e_checkFinalPieceUse()
{
    int used;
    R20ePuzzle* p;
    R20ePiece* pc;

    SceAtSetEnable(0xD, 1);
    while ((used = ItemMgr.check(0x1D)) != 1) {
        SceSleep(1);
    }
    SceEventStart(0);
    RsfSet(G_ROOM_ID, 4);
    SceAtSetEnable(5, 0);
    SceAtSetEnable(1, 0);
    SceAtSetEnable(0x11, 0);
    CamCtrl.CutCall(5);
    SceSleep(30);
    SceMesSet(2, 0x30, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    SndCall(6, 1, 0, 0, 0, 0);
    p = &r20e_work->puzzle;
    pc = PUZZLE_PIECE(p, p->hidden);
    pc->visible = used;
    if (pc->obj) {
        pc->obj->be_flag |= 2;
    }
    SceSleep(30);
    SceMesWait();
    {
        // the function address evaluated before the store: `addi r4,end@l` is issued before `stw snd`
        TaskFunc fn = (TaskFunc) r20e_checkFinalPieceUse_end;

        r20e_work->snd = 0;
        SceSetEventCancel(1, fn, 0, 0, 1);
    }
    CamCtrl.CutCall(6);
    r20e_moveCrestDoor(1, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r20e_checkFinalPieceUse_end();
}

// The puzzle from outside: the look up-cut 1/5, or with the final piece (item 0x1D) held camera cut 5
// and the item screen to use it.
static void r20d_checkPuzzle2()
{
    if (ItemMgr.num(0x1D) == 0) {
        SceUpCut(1, 5, -1, 0);
    } else {
        SceEventStart(0);
        CamCtrl.CutCall(5);
        SceSleep(20);
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
        SceEventEnd(0);
    }
}

// Every cell holds the piece of the solved layout.
static inline int r20e_checkSolved(R20ePuzzle* p)
{
    int x;
    int y;

    for (y = 0; y < 3; y++) {
        const s8* t = &r20e_solvedLayout[0][y];
        R20eCell* c = &p->cell[0][y];

        for (x = 0; x < 3; x++) {
            s8 v = *t;

            t += 3;
            if (c->piece != v) {
                return 0;
            }
            c += 3;
        }
    }
    return 1;
}

// One frame of a piece's slide; returns whether it is moving.  `frames` is the int->float
// conversion of `cnt`, declared BEFORE `one`: cse folds the conversion to 4.0 and loop.c
// re-materialises it as a pool load when it hoists the movable, so the 4.0 pool entry is created
// after 1.0 (pool order 10.0, 1.0, 4.0) while the hoisted loads come out frames-first (loop body
// order) -- the first-loaded constant gets f30, so `one` is f31 (`fdivs f1,f31,f30`).
static inline int r20e_movePiece(R20ePiece* pc)
{
    int cnt = 4;
    f32 frames = cnt;
    f32 one = 1.0f;

    if (pc->visible == 0) {
        return 0;
    }
    if (pc->state != 0) {
        switch (pc->state) {
        case 1:
            pc->cnt = cnt;
            PSVECSubtract(&pc->target, &pc->obj->pos, &pc->spd);
            PSVECScale(&pc->spd, &pc->spd, one / frames);
            pc->state++;
        case 2:
            PSVECAdd(&pc->obj->pos, &pc->spd, &pc->obj->pos);
            pc->cnt--;
            if (pc->cnt <= 0) {
                pc->obj->pos = pc->target;
                pc->state = 0;
            }
            break;
        }
        return 1;
    }
    return 0;
}

// The cell-address chain: a pointer PARAMETER of an inline receives its argument through
// `copy_to_mode_reg` of the EXPAND_SUM sum, i.e. `force_operand` computes the sum INTO the
// parameter pseudo (`c = cy16 + cxp; c = c + 0x178`).  cse cannot rewrite the copy's first word
// `(mem c)` into `(mem (plus X 0x178))` because the `(plus c 0x178)` table entry mentions the
// re-set register (REG_IN_TABLE != REG_TICK), so combine forms `lwzu r11,0x178(r9)`; a local
// pointer variable or a direct `->pos` goes through fresh pseudos and cse picks the costlier
// equivalent address (`addi r11,r9,376; lwz r10,376(r9)`).  The destination `Vec&` is the
// caller's block-local temp (all slide blocks share 8(r1); an inline-local Vec would get its own
// frame slot per copy).
static inline void r20e_framePos(Vec& pos, R20eCell* c)
{
    pos = c->pos;
}

// The slide's source cell: the `to` chain must be computed BEFORE the `from` address (cse would
// otherwise fold the chain into copies of `from`'s pseudos and rewrite `(mem to)`), and the piece
// load before the copy's stores (a later `p->cy` read would reload cy).  Only statement order
// inside one inline gives this order; inline arguments are not evaluated left to right.
static inline int r20e_cellPos(Vec& pos, R20eCell* to, R20ePuzzle* p, int fx, int fy)
{
    int pc = PUZZLE_CELL(p, fx, fy)->piece;

    pos = to->pos;
    return pc;
}

// `q` as a chained parameter keeps `addi r11,r11,0x10` (the `+0x10` is not folded into the
// `stw 0xc(r11)`/`stw 0(r11)` offsets).
static inline void r20e_setPiece(R20ePiece* q, const Vec& pos)
{
    q->target = pos;
    q->state = 1;
}

// `pc` is the caller's (puzzleMove-scope) variable: with four sets in four loops it is a global
// allocno (r7); a block-local `pc` is tied to the `lbz` byte by local-alloc and takes r0.
#define r20e_slidePiece(p, fx, fy, tx, ty)                                    \
    {                                                                         \
        Vec pos;                                                              \
        pc = r20e_cellPos(pos, PUZZLE_CELL(p, tx, ty), p, fx, fy);            \
                                                                              \
        r20e_setPiece(PUZZLE_PIECE(p, pc), pos);                              \
        PUZZLE_CELL(p, tx, ty)->piece = pc;                                   \
        PUZZLE_CELL(p, fx, fy)->piece = -1;                                   \
    }

// Cursor moves and slides of one frame; 1 once the puzzle is solved.
static inline int r20e_puzzleMove(R20ePuzzle* p)
{
    int i;
    int busy;
    int pc;

    if (r20e_checkSolved(p) == 1) {
        return 1;
    }
    if (Key.rep & 0x04000000) {
        p->cx++;
    }
    if (Key.rep & 0x08000000) {
        p->cx--;
    }
    if (Key.rep & 0x02000000) {
        p->cy++;
    }
    if (Key.rep & 0x01000000) {
        p->cy--;
    }
    p->cx = p->cx < 0 ? 0 : (p->cx > 2 ? 2 : p->cx);
    p->cy = p->cy < 0 ? 0 : (p->cy > 2 ? 2 : p->cy);
    if (p->frame) {
        // cy*16 first, then (cx*48 + p), 0x178 last: `mulli cx; add p; add cy16; lwzu 0x178`
        Vec pos;

        r20e_framePos(pos, (R20eCell*) (p->cy * 0x10 + (p->cx * 0x30 + (u32) p) + 0x178));
        pos.y += 10.0f;
        p->frame->pos = pos;
        p->frame->be_flag |= 2;
    }
    for (i = 0; i < 9; i++) {
        busy = r20e_movePiece(&p->piece[i]);
    }
    if (busy) {
        return 0;
    }
    // `cy` is a local (the row scan's cell pointer is a loop-invariant + biv step, `lbz 12(c)`
    // with `addi c,48` at the latch: a `c + 12` giv has benefit 0 and is not reduced), `cx` is
    // NOT: the column scan re-reads `p->cx`, gcse PREs the load at the end of the test block
    // and cse2 turns the recomputation into the copy `mr r10,r9`.  The occupied-cell test is a
    // nested `if` around both scans, not an early `return 0`: the return arm's `li r0,0` would be
    // hoisted above the `beq` by jump1 instead of cross-jumping into the shared `li r0,0`.
    if (Key.trg & 0x00080000) {
        int cy = p->cy;

        if (PUZZLE_CELL(p, p->cx, p->cy)->piece != -1) {
            R20eCell* c;
            int j;
            int k;

            for (j = 0, c = PUZZLE_CELL(p, 0, cy); j < 3; j++, c += 3) {
                if (c->piece == -1) {
                    SndCall(6, 0, 0, 0, 0, 0);
                    if (j < p->cx) {
                        for (k = j + 1; k <= p->cx; k++) {
                            r20e_slidePiece(p, k, p->cy, k - 1, p->cy);
                        }
                    } else {
                        for (k = j - 1; k >= p->cx; k--) {
                            r20e_slidePiece(p, k, p->cy, k + 1, p->cy);
                        }
                    }
                    return 0;
                }
            }
            for (j = 0, c = PUZZLE_CELL(p, p->cx, 0); j < 3; j++, c++) {
                if (c->piece == -1) {
                    SndCall(6, 0, 0, 0, 0, 0);
                    if (j < p->cy) {
                        for (k = j + 1; k <= p->cy; k++) {
                            r20e_slidePiece(p, p->cx, k, p->cx, k - 1);
                        }
                    } else {
                        for (k = j - 1; k >= p->cy; k--) {
                            r20e_slidePiece(p, p->cx, k, p->cx, k + 1);
                        }
                    }
                    return 0;
                }
            }
        }
    }
    return 0;
}

// `w` is assigned before BOTH SceMesSet calls: the second set (in the loop arm) has its
// REGNO_FIRST_UID outside the loop, so `reg_in_basic_block_p` fails and loop.c does not treat the
// `+4` as a movable -- the lo_sum of cMes then stands alone (savings 1, life 3) and stays in the arm
// (`addi r9,r20,cMes@l; addi r9,r9,4`) instead of being hoisted with the `+4` folded into the reloc.
static void r20d_checkPuzzle()
{
    int cancel = 0;
    MesWork* w;

    SceEventStart(0);
    CamCtrl.CutCall(5);
    SceSleep(1);
    w = cMes.getWork();
    SceMesSet(0, 0x200, 1, 0x64, 0x150 - w->lineSpace - w->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case -1:
    case 2:
        cancel = 1;
        break;
    case 0:
    case 1:
        break;
    }
    while (cancel == 0) {
        R20ePuzzle* p = &r20e_work->puzzle;
        // COMPILER-DIFF: candidate (loop.c pass-2 insn_count). Two dead sets of `key` (flow deletes
        // both). (1) They make the cancel test's `&Key` a multi-set pseudo (may_not_optimize), so it
        // no longer forces its `high`, and the Key.rep block's `high(Key)` -- which that high is
        // combined into -- drops from savings 3 x life 3 to 2 x 2: 71 * 4 < 624 insns, not hoisted
        // in loop pass 2 (the original's loop had >= 640 real insns at that pass, or an equivalent
        // structure; ours has 624 and hoists `lis r23,Key@ha` to the preheader). (2) The second
        // set's two insns move gcse's expression table from 331 to 333 buckets so the reaching regs
        // of `high(pG)` and `high(RoomData)` keep the target's allocation order (r25/r24).
        KeyWork* key = 0;
        int result;

        result = r20e_puzzleMove(p);
        key = (KeyWork*) (p + result);
        if (result == 1) {
            int frame;
            int i;

            if (r20e_work->puzzle.frame) {
                r20e_work->puzzle.frame->be_flag &= ~2;
            }
            for (frame = 0; frame < 30; frame++) {
                R20ePuzzle* q = &r20e_work->puzzle;

                for (i = 0; i < 9; i++) {
                    r20e_movePiece(&q->piece[i]);
                }
                SceSleep(1);
            }
            RsfSet(G_ROOM_ID, 3);
            SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r20d_checkPuzzle2, 0, 1);
            SceExec(0x12, (TaskFunc) r20e_checkFinalPieceUse, 0, 0, SCE_PRIO_DEF_2, 0);
            w = cMes.getWork();
            SceMesSet(1, 0, 1, 0x64, 0x150 - w->lineSpace - w->m_font_h - 1);
            break;
        }
        key = &Key;
        if (key->trg & 0x00040000) {
            if (r20e_work->puzzle.frame) {
                r20e_work->puzzle.frame->be_flag &= ~2;
            }
            break;
        }
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Places piece `pc` of cell `c`: the Vec temp is the inline's own local (integrate substitutes its
// frame address, no PRE), while the loops and their counters belong to the caller.  Both layout
// nests step TWO cell pointers of equal value (`c` for the pos loads, its copy `cs` for the piece
// store, `mr r7,r11` in the preheader, two `addi ,48` in the latch): with one pointer the target's
// split cannot be reproduced (25 forms, pass 8).  The latch order is the source order (`c += 3`
// first, like the target); cse's `(set REG0 REG1)` special case would then swap the lo_sum's
// destination to the later-mentioned `cs`, but it only fires when the copy `cs = c` DIRECTLY
// follows `c`'s set, so the `x = 0` statement sits between the two.  The first layout pass in
// r20e_initPuzzle is this macro over the function's own `x`/`y` (the target's r28 serves as y in
// both the object loop and the layout loop): with the counter shared, `y + 1` stays a latch biv in
// the object loop instead of being PRE'd across the inner loop (pl0f BoatControl rule), which is
// what forms the `y*4`/`y*16` givs and the `subic.` count-down.  The else arm's layout has its own
// block-local counters (caller-saved r8 in the target).  The table is a `u32` (not a pointer)
// computed before the nest: `add y,tbl` keeps the written operand order (a pointer would go first),
// and the `lis/addi` is a plain statement instead of a loop.c hoist.
// Both nests are wrapped in `do { } while (0)`: the copy temps x/y/z (3 refs each, flow weights refs
// by loop depth) and the `mulli` temp (2 refs) are global allocnos; the mulli temp inherits `pc`'s
// r0 preference (expand_preferences: pc dies at the mulli) and, when it ranks BELOW x, puts r0 into
// x's `regs_someone_prefers` so pass 0 skips r0 (ours: x r10, z r8, y r7 and the loop pointers shift
// down one register).  At depth 3 (function + two loops) x = 3*9/11 = 2.45 outranks the temp 2*6/7 =
// 1.71; the extra loop level makes it 3*12/11 = 3.27 vs 3*8/7 = 3.43 and x takes r0 like the target
// (y r8, z r10, cs r7).  The do-while's loop notes are a sched1 barrier: everything the target issues
// before `li y,0` (the table address, `lwz r20e_work`) is computed before it, the rest inside.
static inline void r20e_placePiece(R20ePuzzle* p, R20eCell* c, R20eCell* cs, s8 pc)
{
    cs->piece = pc;
    if (pc != -1) {
        R20ePiece* q = PUZZLE_PIECE(p, pc);
        Vec pos;

        pos = c->pos;
        if (q->obj) {
            q->obj->pos = pos;
        }
    }
}

// `y = 0` is the caller's (inside its do-while, before the puzzle pointer of the else arm).
#define R20E_SET_LAYOUT(p, tbl)                                            \
    for (; y < 3; y++) {                                                   \
        R20eCell* c = &(p)->cell[0][y];                                    \
        x = 0;                                                             \
        R20eCell* cs = c;                                                  \
        for (; x < 3; x++) {                                               \
            r20e_placePiece(p, c, cs, ((const s8*) (y + (tbl)))[x * 3]);   \
            c += 3;                                                        \
            cs += 3;                                                       \
        }                                                                  \
    }

// The 3x3 sliding picture puzzle: the nine piece objects on their cells (r20e_pieceObjId), the empty
// cell, the solved layout; already solved (saved flag) -> pieces posed solved, else the cursor / slide
// state and the puzzle area.
void r20e_initPuzzle()
{
    R20ePuzzle* p = &r20e_work->puzzle;
    R20eCell* last = &p->cell[2][2];
    int n = 0;
    int x;
    int y;

    for (y = 0; y < 3; y++) {
        for (x = 0; x < 3; x++) {
            cObj* obj = SmdGetObjPtr(r20e_pieceObjId[x][y]);
            R20ePiece* pc = &p->piece[n];

            if (obj) {
                obj->be_flag |= 0x20;
                p->cell[x][y].pos = obj->pos;
                pc->obj = obj;
                pc->visible = 1;
                obj->be_flag |= 2;
                pc->state = 0;
            }
            n++;
        }
    }
    p->frame = SmdGetObjPtr(0x48);
    if (p->frame) {
        p->frame->be_flag |= 0x20;
        p->frame->be_flag &= ~2;
        p->frame->z_mode = 2;
    }
    {
        int hidden = n - 1;
        R20ePiece* pc = PUZZLE_PIECE(p, hidden);

        p->hidden = hidden;
        pc->visible = 0;
        if (pc->obj) {
            pc->obj->be_flag &= ~2;
        }
    }
    p->cx = 2;
    p->cy = 2;
    last->piece = -1;
    {
        u32 tbl = (u32) r20e_initLayout;
        do {
            y = 0;
            R20E_SET_LAYOUT(p, tbl);
        } while (0);
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r20d_checkPuzzle, 0, 1);
        SceAtSetEnable(0xD, 0);
        r20e_moveCrestDoor(0, 1);
    } else {
        {
            R20eWork* w = r20e_work;
            u32 tbl = (u32) r20e_solvedLayout;
            int x;
            int y;
            do {
                y = 0;
                R20ePuzzle* q = &w->puzzle;
                R20E_SET_LAYOUT(q, tbl);
            } while (0);
        }
        if (RsfCheck(G_ROOM_ID, 4) == 0) {
            SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r20d_checkPuzzle2, 0, 1);
            SceExec(0x12, (TaskFunc) r20e_checkFinalPieceUse, 0, 0, SCE_PRIO_DEF_2, 0);
            r20e_moveCrestDoor(0, 1);
        } else {
            R20ePuzzle* q = &r20e_work->puzzle;
            R20ePiece* pc = PUZZLE_PIECE(q, q->hidden);

            pc->visible = 1;
            if (pc->obj) {
                pc->obj->be_flag |= 2;
            }
            r20e_moveCrestDoor(1, 1);
            SceAtSetEnable(0x11, 0);
        }
    }
}

// Treasure box lids: 0 the chest, 1 the armor statue's box.
void r20e_openBox_main(int no, int opened)
{
    Vec spd = {0.0f, 0.0f, 0.0f};
    cObj* obj = 0;

    switch (no) {
    case 0:
        obj = SmdGetObjPtr(0xF);
        spd.z = -1.7f;
        break;
    case 1:
        obj = SmdGetObjPtr(0x24);
        spd.x = -1.3f;
        break;
    default:
        SceExit();
        break;
    }
    if (obj) {
        obj->be_flag |= 0x20;
        if (opened == 1) {
            PSVECAdd(&obj->pParts->ang, &spd, &obj->pParts->ang);
        } else {
            Vec step;
            int i;

            PSVECScale(&spd, &step, 1.0f / 30.0f);
            SndCall(6, 0x5B, 0, 0, 0, 0);
            for (i = 0; i < 30; i++) {
                PSVECAdd(&obj->pParts->ang, &step, &obj->pParts->ang);
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": pose box `no` open.
static void r20e_openedBox(int no)
{
    r20e_openBox_main(no, 1);
}

// Item-event opener: animate box `no` open.
static void r20e_openBox(int no)
{
    r20e_openBox_main(no, 0);
}

// Shelf doors: the two door objects swing apart.
void r20e_openShelf_main(int no, int opened)
{
    f32 ang = 0.0f;
    cObj* a = 0;
    cObj* b = 0;

    switch (no) {
    case 0:
        a = SmdGetObjPtr(0x13);
        b = SmdGetObjPtr(0x12);
        ang = 2.22f;
        break;
    case 1:
        a = SmdGetObjPtr(0x14);
        b = SmdGetObjPtr(0x15);
        ang = 2.22f;
        break;
    default:
        SceExit();
        break;
    }
    if (a && b) {
        a->be_flag |= 0x20;
        b->be_flag |= 0x20;
        if (opened == 1) {
            a->pParts->ang.y = ang;
            b->pParts->ang.y = -ang;
        } else {
            int i;

            ang /= 30.0f;
            SndCall(6, 0x19, 0, 0, 0, 0);
            for (i = 0; i < 30; i++) {
                a->pParts->ang.y += ang;
                b->pParts->ang.y -= ang;
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": pose shelf `no` open.
static void r20e_openedShelf(int no)
{
    r20e_openShelf_main(no, 1);
}

// Item-event opener: animate shelf `no` open.
static void r20e_openShelf(int no)
{
    r20e_openShelf_main(no, 0);
}
