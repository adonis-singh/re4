#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
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
#include "emhit.h"
#include "em10.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "math_sub.h"
#include "rnd.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "eprintf.h"
#include "db_log.h"

// Room 2-22 (D:/Bio4/Prog/r222.cpp): the dragon statue hall - the three fire-breathing dragons,
// their shot targets and the treasure boxes that rise once a dragon is dropped.

struct R222Work {
    cEmWrap dragon;      // 0x00
    cEmWrap em1;         // 0x0C
    cEmWrap em2;         // 0x18
    cEmHit* hit[6];      // 0x24
    int hp[6];           // 0x3C
    cSat* sat;           // 0x54
    Vec satPos;          // 0x58
    Vec satRot;          // 0x64
    int seTimer;         // 0x70
    int resetTimer;      // 0x74
    u32 strId;           // 0x78
    f32 boxY1[3];        // 0x7C
    f32 boxY2[3];        // 0x88
    u8 pad_94[0x10];
    f32 fallY;           // 0xA4
};


static R222Work* r222_work;

static Vec r222_zero = {0.0f, 0.0f, 0.0f};  // local in the REL (ADDR16 fields hold S+A)
static f32 r222_angA0 = 0.68f;
static f32 r222_angA1 = 1.08f;
static f32 r222_angA2 = -2.53f;
static f32 r222_angA3 = -2.13f;
static f32 r222_angB0 = 2.77f;
static f32 r222_angB1 = 3.17f;
static f32 r222_angB2 = -0.29f;
static f32 r222_angB3 = 0.13f;
// the split object's .data is padded to 8 bytes
asm(".section .data\n\t.balign 8\n\t.text");

static void r222_TreasureBoxOpen(int id);
static void r222_TreasureBoxOpened(int id);
void r222_BoxMove(cObj* obj, int opened);
static void r222_TreasureBox2Open(int id);
static void r222_TreasureBox2Opened(int id);
void EmHitUpdate(cEmHit* h);
void Hit(int no);
static void dragon_down_ck();
static void dragon_down_exit();
static void dragon_down();
static void box_appear1_exit();
static void box_appear1();
static void dragon_down2();
static void box_appear2_exit();
static void box_appear2();
static void dragon_down3();
static void dragon_appear_exit();
static void dragon_appear();
static void first_cut_exit();
static void first_cut();
static void em_appear_exit();
static void em_appear();
// setResetNum/getResetNum/incResetNum are also defined in r208.cpp (same module): static.
static void setResetNum(int n);
static int getResetNum();
static void incResetNum();
void em_reset();

// A dragon's shot target: hit box on the object's position. A macro, not an inline: integrate.c drops
// RTX_UNCHANGING_P from an inlined body's pool loads, which then wait for the `hit[no]` store (cost 2);
// as pool MEMs of the function itself the YarareInitCube constants are `mem/u` and issue before it.
#define r222_setHit(no, objId)                                                                                  \
    r222_work->hit[no] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &SmdGetObjPtr(objId)->pos, &SmdGetObjPtr(objId)->ang, 1); \
    YarareInitCube(r222_work->hit[no], 0.0f, -3500.0f, 0.0f, 550.0f, 1300.0f, 550.0f, 0, YAT_FLAG_ON)

// Room init (the dragon hall): Debug_flg[1] 0x20000, Status_flg[1] bit 0, no water splashes, the moving
// objects marked script-moved. Per dragon (Room_flg bits 0/1/2 = fallen): fallen -> its collision
// pieces and the boxes it revealed, else its two shot-target hit boxes (r222_setHit) and the dragon enemy
// (0x16 / 0x14 / 0x15). The entrance cut once (bit 4), the side dragons on area 6 until bit 5, the
// dragon watcher, item area 0x80 and the treasure box item events.
void R222Init()
{
    cModel* m;

    // Reference store for the calloc result: its `lis r222_work@ha` is hoisted into a callee-saved register
    // before the call (the r11b/r402 idiom); the sixth be_flag store is a BitOn so the RsfCheck's `lwz pG`
    // stays below it. The reference is declared after the pG flag store so that `high(pG)` is the earlier
    // gcse expression: the two PRE'd highs fill the prologue's free slots in first-occurrence order.
    DbgFlagOn(pG, DBG_EMW_ERR_NO_DISP);
    R222Work*& wp = r222_work;
#line 70 "D:/Bio4/Prog/r222.cpp"
    wp = (R222Work*) MEM_CALLOC(sizeof(R222Work), 1, 0xd);
    StaFlagOn(pG, STA_LASERSITE_NOADD);
    Espgen42SetNoWater(1);
    SmdGetObjPtr(0xA)->be_flag |= 0x20;
    SmdGetObjPtr(0xB)->be_flag |= 0x20;
    SmdGetObjPtr(0xF)->be_flag |= 0x20;
    SmdGetObjPtr(0x10)->be_flag |= 0x20;
    SmdGetObjPtr(0x14)->be_flag |= 0x20;
    SmdGetObjPtr(0x15)->be_flag |= 0x20;
    if (RsfCheck(G_ROOM_ID, 0)) {
        SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r222_zero, &r222_zero, 4);
        EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r222_zero, &r222_zero, 5);
        SceAtSetEnable(5, 0);
        SmdGetObjPtr(0x14)->be_flag &= ~2;
        SmdGetObjPtr(0x15)->be_flag &= ~2;
        SmdGetObjPtr(0x16)->be_flag &= ~2;
        SmdGetObjPtr(0x17)->be_flag &= ~2;
        SmdGetObjPtr(0x2E)->be_flag |= 0x20;
        SmdGetObjPtr(0x2E)->pos.y = 0.0f;
    } else {
        SceAtSetEnable(5, 1);
        r222_setHit(0, 0x16);
        r222_setHit(1, 0x17);
        r222_work->hp[0] = 7000;
        r222_work->hp[1] = 7000;
        SmdGetObjPtr(0x14)->pos.y += 30000.0f;
        SmdGetObjPtr(0x15)->pos.y += 30000.0f;
        SmdGetObjPtr(0x16)->pos.y += 30000.0f;
        SmdGetObjPtr(0x17)->pos.y += 30000.0f;
    }
    r222_work->dragon.setEm(0x16, -1, 1, 1, 1);
    if (RsfCheck(G_ROOM_ID, 1)) {
        SmdGetObjPtr(0xA)->be_flag &= ~2;
        SmdGetObjPtr(0xB)->be_flag &= ~2;
        SmdGetObjPtr(0xD)->be_flag &= ~2;
        SmdGetObjPtr(0xE)->be_flag &= ~2;
    } else {
        r222_setHit(2, 0xD);
        r222_setHit(3, 0xE);
        r222_work->hp[2] = 7000;
        r222_work->hp[3] = 7000;
        SmdGetObjPtr(0x25)->be_flag |= 0x20;
        SmdGetObjPtr(8)->be_flag |= 0x20;
        SmdGetObjPtr(9)->be_flag |= 0x20;
        SmdGetObjPtr(0x26)->be_flag |= 0x20;
        SmdGetObjPtr(0x27)->be_flag |= 0x20;
        r222_work->boxY1[0] = SmdGetObjPtr(0x25)->pos.y;
        r222_work->boxY1[1] = SmdGetObjPtr(8)->pos.y;
        r222_work->boxY1[2] = SmdGetObjPtr(9)->pos.y;
        SmdGetObjPtr(0x25)->pos.y = -3677.0f;
        SmdGetObjPtr(8)->pos.y = -3660.0f;
        SmdGetObjPtr(9)->pos.y = -3338.0f;
        SmdGetObjPtr(0x26)->pList->ang.z = -1.5707964f;
        SmdGetObjPtr(0x27)->pList->ang.z = 1.5707964f;
        SceAtSetEnable(9, 0);
    }
    if (RsfCheck(G_ROOM_ID, 2)) {
        SmdGetObjPtr(0xF)->be_flag &= ~2;
        SmdGetObjPtr(0x10)->be_flag &= ~2;
        SmdGetObjPtr(0x12)->be_flag &= ~2;
        SmdGetObjPtr(0x13)->be_flag &= ~2;
    } else {
        r222_setHit(4, 0x12);
        r222_setHit(5, 0x13);
        r222_work->hp[4] = 7000;
        r222_work->hp[5] = 7000;
        SmdGetObjPtr(0x29)->be_flag |= 0x20;
        SmdGetObjPtr(6)->be_flag |= 0x20;
        SmdGetObjPtr(7)->be_flag |= 0x20;
        SmdGetObjPtr(0x2A)->be_flag |= 0x20;
        SmdGetObjPtr(0x2B)->be_flag |= 0x20;
        r222_work->boxY2[0] = SmdGetObjPtr(0x29)->pos.y;
        r222_work->boxY2[1] = SmdGetObjPtr(6)->pos.y;
        r222_work->boxY2[2] = SmdGetObjPtr(7)->pos.y;
        SmdGetObjPtr(0x29)->pos.y = -3589.0f;
        SmdGetObjPtr(6)->pos.y = -3573.0f;
        SmdGetObjPtr(7)->pos.y = -3250.0f;
        SmdGetObjPtr(0x2A)->pList->ang.z = -1.5707964f;
        SmdGetObjPtr(0x2B)->pList->ang.z = 1.5707964f;
        SceAtSetEnable(0xA, 0);
    }
    r222_work->sat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r222_zero, &r222_zero, 6);
    r222_work->satPos.x = -26987.0f;
    r222_work->satPos.y = -1460.0f;
    r222_work->satPos.z = -8329.0f;
    r222_work->satRot.y = SmdGetObjPtr(1)->pList->ang.y;
    r222_work->sat->setCoord(&r222_work->satPos, &r222_work->satRot);
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        RsfSet(G_ROOM_ID, 4);
        SceExec(0x12, (TaskFunc) first_cut, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) em_appear, 0, 1);
    } else {
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            r222_work->em1.setEm(0x14, -1, 1, 1, 1);
        }
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            r222_work->em2.setEm(0x15, -1, 1, 1, 1);
        }
    }
    SceExec(0x12, (TaskFunc) dragon_down_ck, 0, 0, SCE_PRIO_DEF_2, 0);
    SceAtSetEnable(0x80, 1);
    m = SceAtItemModelPtr(0x80);
    if (m) {
        Vec sca = {0.65f, 0.65f, 0.65f};

        m->setSca(&sca);
    }
    SceSetItemEvent(1, 0x80, 6, 8, r222_TreasureBoxOpen, r222_TreasureBoxOpened, 0x1A, 0);
    SceSetItemEvent(9, 0x82, 7, 0xD, r222_TreasureBox2Open, r222_TreasureBox2Opened, 9, 0);
    SceSetItemEvent(0xA, 0x81, 8, 0xF, r222_TreasureBox2Open, r222_TreasureBox2Opened, 7, 0);
}

// Item-event opener: chest `id` lid up (+Z).
static void r222_TreasureBoxOpen(int id)
{
    OpenBoxMain(OpenBoxUpZP, 0, 0x5B, id, -1, -1);
}

// Item-event "already opened": chest `id` posed open.
static void r222_TreasureBoxOpened(int id)
{
    OpenBoxMain(OpenBoxUpZP, 1, 0x5B, id, -1, -1);
}

// Treasure box lid: swings the parts open (`opened` 1: already open).
// 2.1206448f as a named .rodata word in the pool position (see the COMPILER-DIFF note in r222_BoxMove).
// It has to be a LOCAL label: a C `extern const` definition is emitted in the same place but its
// global symbol makes the -r link keep the @l relocations unresolved (the REL's instruction fields
// then hold 0 instead of the section offset), and a `static const` at file scope is deferred to the
// end of the file. The wait body reads the word through the asm-labelled alias (a distinct
// SYMBOL_REF: gcse cannot share the `high` r10 of the arms with it, the target reloads `lis r11`).
asm(".section \".rodata\"\n\t.align 2\nr222_k212:\n\t.long 0x4007b8a5\n\t.section \".text\"");
extern const f32 r222_k212;
extern const f32 r222_k212_v asm("r222_k212");

// A revealed box's lid (parts ang.x) swings to 2.12 rad in 0.05 steps with its SE, or snaps there when opened == 1.
void r222_BoxMove(cObj* obj, int opened)
{
    SndCall(6, 0x5B, &obj->pos, 0, 0, 0);
    obj->be_flag |= 0x20;
    // COMPILER-DIFF: #3 (the original's gcse shares one `high` of the 2.12 constant between the two arms
    // of the `if`; ours computes it per arm, the block-based LCM never hoists from two sibling arms).
    // The dead address computation puts the `high` in this block (r10, live across the branch); cse
    // and gcse then make both arms' loads use it. The `lo_sum` is dead and flow deletes it.
    {
        const f32* pk = &r222_k212;
    }
    if (opened == 1) {
        obj->pList->ang.x = r222_k212;
    } else {
        f32 lim;
        f32 r;
        // a goto loop: the constants are reloaded per iteration; the limit is computed in both
        // predecessors of `test` (the target loads 2.12 there and compares/stores that register)
        r = obj->pList->ang.x;
        lim = r222_k212;
        obj->pList->ang.x = r + 0.05f;
        goto test;
    wait:
        SceSleep(1);
        obj->pList->ang.x += 0.05f;
        lim = r222_k212_v;
    test:
        if (!(obj->pList->ang.x > lim)) {
            goto wait;
        }
        obj->pList->ang.x = lim;
    }
}

// Item-event opener for the risen boxes: object `id`'s lid swings open.
static void r222_TreasureBox2Open(int id)
{
    r222_BoxMove(SmdGetObjPtr(id), 0);
}

// Item-event "already opened": object `id`'s lid posed open.
static void r222_TreasureBox2Opened(int id)
{
    r222_BoxMove(SmdGetObjPtr(id), 1);
}

// Rebuilds the hit box matrix from its position / rotation / scale.
void EmHitUpdate(cEmHit* h)
{
    MtxPtr m = h->mat;

    RotMatrix(m, &h->ang);
    TransMatrix(m, &h->pos);
    ScaleMatrix(m, &h->scale);
    h->partsMatCalc();
    h->partsWorldCalc();
}

// Damage on shot target `no`.
void Hit(int no)
{
    r222_work->hp[no] -= GetWepDmVal(r222_work->hit[no], r222_work->hit[no]->dmg.m_Wep, 0);
    EmDmBloodSet2(r222_work->hit[no], 1, 0xD, 0, 0, 0);
    SndCall(6, 0xF, &r222_work->hit[no]->pos, 0, 0, 0);
}

// Per frame (debug trigger 0 drops the main dragon): reads the rotating platform's angle (object 1's
// parts) and, per dragon zone flag in Room_flg[2], toggles Status_flg[0] bit 3 and the fire areas 8 / 7
// when the platform faces a dragon's mouth (the angle windows r222_angA*/B*); moves the platform
// collision with the angle; a random creak SE every 30 frames.
void R222Main()
{
    f32 ry;
    // The SE timer's reset value lives in a pseudo set here and used once: update_equiv_regs moves the
    // `li` next to the store AFTER sched1, so sched1 issues the store at t1 (with the template's `lis`)
    // and the template's word-0 load gets the work pointer's register.
    int seReset = 30;

    if (DebugTrg(0)) {
        SceExec(0x12, (TaskFunc) dragon_down, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    SmdGetObjPtr(1)->pList->ang.y = LIMIT_ANGLE(SmdGetObjPtr(1)->pList->ang.y);
    ry = SmdGetObjPtr(1)->pList->ang.y;
    if ((ry > r222_angA0 && ry < r222_angA1) || (ry > r222_angA2 && ry < r222_angA3)) {
        if (pG->Room_flg[2] & 0x80000000) {
            StaFlagOff(pG, STA_PL_JUMP_OFF);
        }
        eprintf(0xD8, 0x38, 4, 0, "%f", ry);
        SceAtSetEnable(8, 0);
    } else {
        if (pG->Room_flg[2] & 0x80000000) {
            StaFlagOn(pG, STA_PL_JUMP_OFF);
        }
        SceAtSetEnable(8, 1);
        eprintf(0xD8, 0x38, 2, 0, "%f", ry);
    }
    SmdGetObjPtr(1)->pList->ang.y = LIMIT_ANGLE(SmdGetObjPtr(1)->pList->ang.y);
    ry = SmdGetObjPtr(1)->pList->ang.y;
    if ((ry > r222_angB0 && ry < r222_angB1) || (ry > r222_angB2 && ry < r222_angB3)) {
        if (pG->Room_flg[2] & 0x40000000) {
            StaFlagOff(pG, STA_PL_JUMP_OFF);
        }
        eprintf(0xD8, 0x46, 4, 0, "%f", ry);
        SceAtSetEnable(7, 0);
    } else {
        if (pG->Room_flg[2] & 0x40000000) {
            StaFlagOn(pG, STA_PL_JUMP_OFF);
        }
        eprintf(0xD8, 0x46, 2, 0, "%f", ry);
        SceAtSetEnable(7, 1);
    }
    r222_work->satRot.y = SmdGetObjPtr(1)->pList->ang.y;
    r222_work->sat->setCoord(&r222_work->satPos, &r222_work->satRot);
    if (r222_work->seTimer <= 0) {
        r222_work->seTimer = seReset;
        Vec pos = {-37635.0f, -7165.0f, -10599.0f};

        pos.x += fRand1_1() * 32107.0f;
        pos.z += fRand1_1() * 12726.0f;
        SndCall(6, 4, &pos, 0, 0, 0);
    } else {
        r222_work->seTimer--;
    }
}

// One dragon's pair of shot targets follows its objects; the dragon drops when both are destroyed
// (written out per dragon: an inline taking the wrap / task pointers precomputes them).
#define R222_DRAGON_CHECK(no, objA, objB, em, down)                                     \
    r222_work->hit[no]->pos = SmdGetObjPtr(objA)->pos;                                \
    r222_work->hit[no + 1]->pos = SmdGetObjPtr(objB)->pos;                            \
    EmHitUpdate(r222_work->hit[no]);                                                  \
    EmHitUpdate(r222_work->hit[no + 1]);                                              \
    if (r222_work->hit[no]->ckStatus() == 1) {                                        \
        Hit(no);                                                                        \
    }                                                                                   \
    if (r222_work->hit[no + 1]->ckStatus() == 1) {                                    \
        Hit(no + 1);                                                                    \
    }                                                                                   \
    if (r222_work->hp[no] <= 0 || r222_work->hp[no + 1] <= 0) {                     \
        r222_work->em.setHp(0);                                                       \
        SceExec(0x12, (TaskFunc) down, 0, 0, SCE_PRIO_DEF_2, 0);                                     \
    }

// Task: per dragon not yet fallen, keep its shot targets on their objects, apply hits, and start its
// fall when a target is destroyed, when the dragon may be reset, or when it dropped below y -10000;
// the main dragon's wake-up cut once Room_flg[0] bit 31 (bit 3); enemy resets while bit 31.
static void dragon_down_ck()
{
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            if (pG->Room_flg[0] & 0x80000000) {
                em_reset();
            }
            R222_DRAGON_CHECK(0, 0x16, 0x17, dragon, dragon_down);
            if (RsfCheck(G_ROOM_ID, 3) == 0 && (pG->Room_flg[0] & 0x80000000)) {
                RsfSet(G_ROOM_ID, 3);
                SceExec(0x12, (TaskFunc) dragon_appear, 0, 0, SCE_PRIO_DEF_2, 0);
            }
            if ((r222_work->dragon.isAlive() == 1 && r222_work->dragon.ckResetEnable() != 0) || r222_work->dragon.getPosY() < -10000.0f) {
                SceExec(0x12, (TaskFunc) dragon_down, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            R222_DRAGON_CHECK(2, 0xD, 0xE, em1, dragon_down2);
            if ((r222_work->em1.isAlive() == 1 && r222_work->em1.ckResetEnable() != 0) || r222_work->em1.getPosY() < -10000.0f) {
                SceExec(0x12, (TaskFunc) dragon_down2, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            R222_DRAGON_CHECK(4, 0x12, 0x13, em2, dragon_down3);
            if ((r222_work->em2.isAlive() == 1 && r222_work->em2.ckResetEnable() != 0) || r222_work->em2.getPosY() < -10000.0f) {
                SceExec(0x12, (TaskFunc) dragon_down3, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
        SceSleep(1);
    }
}

// End of the main dragon's fall (also its cancel path): the pit object 0x2E snapped to fallY, SceEventEnd, effect dropped.
static void dragon_down_exit()
{
    SmdGetObjPtr(0x2E)->pos.y = r222_work->fallY;
    SceEventEnd(0);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
}

// The main dragon falls into the pit.
static void dragon_down()
{
    int zero = 0;
    cObj* obj;
    cObj* o14;
    cObj* o15;
    f32 h;
    u32 n;
    u32 i;

    SceEventStart(0);
    obj = SmdGetObjPtr(0x2E);
    obj->be_flag |= 0x20;
    obj->pos.y = -7500.0f;
    RsfSet(pG->room_id, 0);
    SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r222_zero, &r222_zero, 4);
    EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r222_zero, &r222_zero, 5);
    SceAtSetEnable(5, 0);
    o14 = SmdGetObjPtr(0x14);
    o15 = SmdGetObjPtr(0x15);
    o14->pos.y = -2362.0f;
    o15->pos.y = -2362.0f;
    o14->ang.y = 1.5708f;
    o15->ang.y = 1.5708f;
    r222_work->dragon.setNoSuspend(1);
    EstSet(o14, -1, 0, 0, EFF_ROOM, 7, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    CamCtrl.CutCall(3);
    MotionSetCore(o14, &o14->Motion, ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 1, 0);
    MotionSetCore(o15, &o15->Motion, ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 1, 0);
    SndStrReq(1, 5, 0x80000003, 0, 0, 0.0f);
    SceSleep(15);
    EstSet(0, -1, &SmdGetObjPtr(0x16)->pos, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    EstSet(0, -1, &SmdGetObjPtr(0x17)->pos, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    SmdGetObjPtr(0x16)->be_flag &= ~2;
    SmdGetObjPtr(0x17)->be_flag &= ~2;
    SndCall(6, 0xD, &SmdGetObjPtr(0x16)->pos, 0, 0, 0);
    SceSleep(60);
    SndCall(6, 0xE, &o14->pos, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    EstSet(o14, -1, 0, 0, EFF_ROOM, 8, 1, ESP_CORE_KIND_NONE, 0, 0);
    CamCtrl.CutCall(4);
    // `&obj->pos` written at both SndCall sites (no pointer local): gcse PREs it into this block's end,
    // between LOOP_BEG and the entry jump of the poll loop, so loop.c ignores that loop and its test
    // reloads `lis CamCtrl@ha` per iteration like the first one (the r113 execHide phony-loop shape).
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    h = 7500.0f;
    r222_work->fallY = obj->pos.y + h;
    SceSetEventCancel(1, (TaskFunc) dragon_down_exit, 0, -1, 1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 9, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    CamCtrl.CutCall(5);
    n = 120;
    h = h / (f32) n;
    SndCall(6, 0x10, &obj->pos, 0, 0, 0);
    for (i = 0; i < n; i++) {
        obj->pos.y += h;
        SceSleep(1);
    }
    SndCall(6, 0x11, &obj->pos, 0, 0, 0);
    SceSleep(15);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    dragon_down_exit();
}

// End of the first box rise (also its cancel path): the box objects 0x25/8/9 snapped to their final
// heights, Room_flg[0] 0x20000000 off, camera back, SceEventEnd, item area 9 on.
static void box_appear1_exit()
{
    SmdGetObjPtr(0x25)->pos.y = r222_work->boxY1[0];
    SmdGetObjPtr(8)->pos.y = r222_work->boxY1[1];
    SmdGetObjPtr(9)->pos.y = r222_work->boxY1[2];
    pG->Room_flg[0] &= ~0x20000000;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtSetEnable(9, 1);
}

// The first treasure box rises out of the floor.
static void box_appear1()
{
    u32 i;
    f32 t;
    f32 y[3];

    while (pG->Room_flg[0] & 0x10000000) {
        SceSleep(1);
    }
    i = 0;
    SceEventStart(1);
    pG->Room_flg[0] |= 0x20000000;
    CamCtrl.CutCall(0xC);
    SmdGetObjPtr(0x26)->pList->ang.z = 0.0f;
    SmdGetObjPtr(0x27)->pList->ang.z = 0.0f;
    y[0] = SmdGetObjPtr(0x25)->pos.y;
    y[1] = SmdGetObjPtr(8)->pos.y;
    y[2] = SmdGetObjPtr(9)->pos.y;
    SceSetEventCancel(1, (TaskFunc) box_appear1_exit, 0, -1, 1);
    t = 0.0f;   // between the call and the loop: keeps flow's `(use 0)` nop out of the sched1 slots (r208 footingB_up)
    for (i = 0; i < 60; i++) {
        f32 r;

        if (i == 30) {
            SndCall(6, 0x12, 0, 0, 0, 0);
        }
        r = 1.0f - t;
        SmdGetObjPtr(0x25)->pos.y = y[0] * r + r222_work->boxY1[0] * t;
        SmdGetObjPtr(8)->pos.y = y[1] * r + r222_work->boxY1[1] * t;
        SmdGetObjPtr(9)->pos.y = y[2] * r + r222_work->boxY1[2] * t;
        t += 0.016666668f;
        SceSleep(1);
    }
    SceSleep(15);
    SceSetEventCancel(0, 0, 0, -1, 1);
    box_appear1_exit();
}

// The second dragon drops onto the box lift.
static void dragon_down2()
{
    Vec pos;
    int zero = 0;
    cObj* oA;
    cObj* oB;
    f32 spd;

    RsfSet(G_ROOM_ID, 1);
    oA = SmdGetObjPtr(0xA);
    oB = SmdGetObjPtr(0xB);
    EstSet(0, -1, &SmdGetObjPtr(0xD)->pos, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    EstSet(0, -1, &SmdGetObjPtr(0xE)->pos, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    SmdGetObjPtr(0xD)->be_flag &= ~2;
    SmdGetObjPtr(0xE)->be_flag &= ~2;
    SndCall(6, 0xD, &SmdGetObjPtr(0xD)->pos, 0, 0, 0);
    MotionSetCore(oA, &oA->Motion, ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 1, 0);
    MotionSetCore(oB, &oB->Motion, ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 1, 0);
    // hit / i / zero2 declared after the EstSets: `zero` then has the latest last-mention when cse
    // canonicalises its stack stores (a zero declared at the top with them would lose them to `hit`) and
    // stays block-local (r30, local-alloc); sched1 hoists the three `li`s to the block top anyway.
    int hit = 0;
    u32 i = 0;
    int zero2 = 0;

    spd = 0.0f;
    for (i = 0; i < 90; i++) {
        spd += -15.0f;
        oA->pos.y += spd;
        oB->pos.y += spd;
        SceSleep(1);
        if (hit == 0 && oA->pos.y < -3500.0f) {
            pos = oA->pos;
            pos.y = -10000.0f;
            EstSet(0, -1, &pos, 0, EFF_ROOM, 0xC, 1, ESP_CORE_KIND_NONE, (void*) zero2, (void*) zero2);
            hit = 1;
            SndCall(6, 0xE, &pos, 0, 0, 0);
        }
    }
    SceExec(0x12, (TaskFunc) box_appear1, 0, 0, SCE_PRIO_DEF_2, 0);
}

// End of the second box rise: objects 0x29/6/7 snapped up, Room_flg[0] 0x10000000 off, camera back, item area 0xA on.
static void box_appear2_exit()
{
    SmdGetObjPtr(0x29)->pos.y = r222_work->boxY2[0];
    SmdGetObjPtr(6)->pos.y = r222_work->boxY2[1];
    SmdGetObjPtr(7)->pos.y = r222_work->boxY2[2];
    pG->Room_flg[0] &= ~0x10000000;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtSetEnable(0xA, 1);
}

// The second treasure box rises out of the floor.
static void box_appear2()
{
    u32 i;
    f32 t;
    f32 y[3];

    while (pG->Room_flg[0] & 0x20000000) {
        SceSleep(1);
    }
    i = 0;
    SceEventStart(1);
    pG->Room_flg[0] |= 0x10000000;
    CamCtrl.CutCall(0xE);
    SmdGetObjPtr(0x2A)->pList->ang.z = 0.0f;
    SmdGetObjPtr(0x2B)->pList->ang.z = 0.0f;
    y[0] = SmdGetObjPtr(0x29)->pos.y;
    y[1] = SmdGetObjPtr(6)->pos.y;
    y[2] = SmdGetObjPtr(7)->pos.y;
    SceSetEventCancel(1, (TaskFunc) box_appear2_exit, 0, -1, 1);
    t = 0.0f;   // between the call and the loop: keeps flow's `(use 0)` nop out of the sched1 slots (r208 footingB_up)
    for (i = 0; i < 60; i++) {
        f32 r;

        if (i == 30) {
            SndCall(6, 0x12, 0, 0, 0, 0);
        }
        r = 1.0f - t;
        SmdGetObjPtr(0x29)->pos.y = y[0] * r + r222_work->boxY2[0] * t;
        SmdGetObjPtr(6)->pos.y = y[1] * r + r222_work->boxY2[1] * t;
        SmdGetObjPtr(7)->pos.y = y[2] * r + r222_work->boxY2[2] * t;
        t += 0.016666668f;
        SceSleep(1);
    }
    SceSleep(15);
    SceSetEventCancel(0, 0, 0, -1, 1);
    box_appear2_exit();
}

// The third dragon drops onto the box lift.
static void dragon_down3()
{
    Vec pos;
    int zero = 0;
    cObj* oA;
    cObj* oB;
    f32 spd;

    RsfSet(G_ROOM_ID, 2);
    oA = SmdGetObjPtr(0xF);
    oB = SmdGetObjPtr(0x10);
    MotionSetCore(oA, &oA->Motion, ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 1, 0);
    MotionSetCore(oB, &oB->Motion, ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 1, 0);
    EstSet(0, -1, &SmdGetObjPtr(0x12)->pos, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    EstSet(0, -1, &SmdGetObjPtr(0x13)->pos, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    SmdGetObjPtr(0x12)->be_flag &= ~2;
    SmdGetObjPtr(0x13)->be_flag &= ~2;
    SndCall(6, 0xD, &SmdGetObjPtr(0x12)->pos, 0, 0, 0);
    // hit / i / zero2 declared after the EstSets: `zero` then has the latest last-mention when cse
    // canonicalises its stack stores (a zero declared at the top with them would lose them to `hit`) and
    // stays block-local (r30, local-alloc); sched1 hoists the three `li`s to the block top anyway.
    int hit = 0;
    u32 i = 0;
    int zero2 = 0;

    spd = 0.0f;
    for (i = 0; i < 90; i++) {
        spd += -15.0f;
        oA->pos.y += spd;
        oB->pos.y += spd;
        SceSleep(1);
        if (hit == 0 && oA->pos.y < -3500.0f) {
            pos = oA->pos;
            pos.y = -10000.0f;
            EstSet(0, -1, &pos, 0, EFF_ROOM, 0xC, 1, ESP_CORE_KIND_NONE, (void*) zero2, (void*) zero2);
            hit = 1;
            SndCall(6, 0xE, &pos, 0, 0, 0);
        }
    }
    SceExec(0x12, (TaskFunc) box_appear2, 0, 0, SCE_PRIO_DEF_2, 0);
}

// End of the dragon wake-up cut: SceEventEnd, the dragon may suspend again.
static void dragon_appear_exit()
{
    SceEventEnd(0);
    r222_work->dragon.setNoSuspend(0);
}

// Cutscene: camera cut 2 on the main dragon waking (effect 0xA); player-cancellable.
static void dragon_appear()
{
    SceEventStart(0);
    r222_work->dragon.setNoSuspend(1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_NONE, 0, 0);
    SceSetEventCancel(1, (TaskFunc) dragon_appear_exit, 0, -1, 1);
    CamCtrl.CutCall(2);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    dragon_appear_exit();
}

// End of the entrance cut: SceEventEnd, the dragons' mouth objects hidden, the stream stopped when the
// cut was skipped (Room_flg[2] 0x20000000).
static void first_cut_exit()
{
    SceEventEnd(0);
    SmdGetObjPtr(0x16)->be_flag |= 2;
    SmdGetObjPtr(0x17)->be_flag |= 2;
    SmdGetObjPtr(0x14)->be_flag |= 2;
    SmdGetObjPtr(0x15)->be_flag |= 2;
    if (pG->Room_flg[2] & 0x20000000) {   // struct view: the pG load stays below the be_flag store
        SndStrReq(r222_work->strId, 8, 0, 0);
    }
}

// The entrance cut: the dragons wake up.
static void first_cut()
{
    SmdGetObjPtr(0x16)->be_flag &= ~2;
    SmdGetObjPtr(0x17)->be_flag &= ~2;
    SmdGetObjPtr(0x14)->be_flag &= ~2;
    SmdGetObjPtr(0x15)->be_flag &= ~2;
    SceEventStart(0);
    r222_work->strId = SndStrReq(0, 0x2E, 0x80000003, 0, 0, 0.0f);
    SceSetEventCancel(1, (TaskFunc) first_cut_exit, 0, 0x42, 1);
    CamCtrl.CutCall(0xA);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(0xB);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(6);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    first_cut_exit();
}

// End of the side dragons' appearance: they may suspend again, SceEventEnd.
static void em_appear_exit()
{
    r222_work->em1.setNoSuspend(0);
    r222_work->em2.setNoSuspend(0);
    SceEventEnd(0);
}

// The two side dragons are set once the player walks in.
static void em_appear()
{
    RsfSet(G_ROOM_ID, 5);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        r222_work->em1.setEm(0x14, -1, 1, 1, 1);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        r222_work->em2.setEm(0x15, -1, 1, 1, 1);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceEventStart(0);
        r222_work->em1.setNoSuspend(1);
        r222_work->em2.setNoSuspend(1);
        CamCtrl.CutCall(7);
        SceSetEventCancel(1, (TaskFunc) em_appear_exit, 0, -1, 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        em_appear_exit();
    }
}

// The enemy reset counter lives in room save flags 9..13.
static void setResetNum(int n)
{
    if (n & 1) {
        RsfSet(G_ROOM_ID, 9);
    } else {
        RsfClear(G_ROOM_ID, 9);
    }
    if (n & 2) {
        RsfSet(G_ROOM_ID, 10);
    } else {
        RsfClear(G_ROOM_ID, 10);
    }
    if (n & 4) {
        RsfSet(G_ROOM_ID, 11);
    } else {
        RsfClear(G_ROOM_ID, 11);
    }
    if (n & 8) {
        RsfSet(G_ROOM_ID, 12);
    } else {
        RsfClear(G_ROOM_ID, 12);
    }
    if (n & 0x10) {
        RsfSet(G_ROOM_ID, 13);
    } else {
        RsfClear(G_ROOM_ID, 13);
    }
}

// The enemy reset counter (0..31) read back from room save flags 9..13.
static int getResetNum()
{
    int n = 0;

    if (RsfCheck(G_ROOM_ID, 9)) {
        n = 1;
    }
    if (RsfCheck(G_ROOM_ID, 10)) {
        n += 2;
    }
    if (RsfCheck(G_ROOM_ID, 11)) {
        n += 4;
    }
    if (RsfCheck(G_ROOM_ID, 12)) {
        n += 8;
    }
    if (RsfCheck(G_ROOM_ID, 13)) {
        n += 0x10;
    }
    return n;
}

// Advance the reset counter (saturates at 31).
static void incResetNum()
{
    if (getResetNum() != 0x1F) {
        setResetNum(getResetNum() + 1);
    }
}

// Places the next enemy of the reset list every 210 frames while few are alive.
void em_reset()
{
    if ((u32) getResetNum() <= 7 && (u32) SceCountEmAlive(0x10, 0x20) <= 3) {
        if (r222_work->resetTimer > 0) {
            r222_work->resetTimer--;
        } else {
            // the remainder in its own variable: a fresh register (r9) instead of the call result's r3
            u32 idx = (u32) getResetNum() % 3;
            cEm* em = EmSetEvent(&pG->Em_list[0x19 + idx]);

            if (em) {
                em->flag |= 1;
            }
            incResetNum();
            r222_work->resetTimer = 210;
        }
    }
}
