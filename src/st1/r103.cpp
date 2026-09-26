#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
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
#include "emitem.h"
#include "read.h"
#include "esp.h"
#include "est.h"
#include "etc_model.h"
#include "snd.h"
#include "vec.h"

// Room 1-03 (D:/Bio4/Prog/r103.cpp, in st1_1 and st1_3): the village house; the corpses, the shelf
// item events, the cesspit item and its cover, the sub-mission target and the battle stream.

struct R103Work {
    u32 eff;   // 0x00  EspPullCoreKind() of the cesspit item glow
};

// Cesspit: object / area numbers
struct R103Cesspit {
    u32 cover;      // 0x00  scroll object: the cesspit cover
    u32 lid;        // 0x04  scroll object: the lid
    int itemAt;     // 0x08  item area
    int itemAt2;    // 0x0C  item area the model moves to
    int at10;       // 0x10
    int at14;       // 0x14
    int at18;       // 0x18  cover area (r103_execOpenCover)
};

// Shelf: the two door objects
struct R103Shelf {
    u8 door[2];
};

static R103Work* r103_work;

// The original's .data is 8-aligned (r105 has the same).
asm(".section .data; .balign 8");
R103Cesspit r103_cesspit = {0x52, 0x53, 0x8A, 0x9E, 6, 3, 0xA};
static R103Shelf r103_shelf0 = {{0x57, 0x58}};
static R103Shelf r103_shelf1 = {{0x59, 0x5A}};
static R103Shelf r103_shelf2 = {{0x5B, 0x5C}};

// Hit effects of attribute type 4
static const AtEffInfo r103_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

static void r103_getFile();
extern "C" void r103_openShelf_main(R103Shelf* s, int opened);
extern "C" void r103_openedShelf(R103Shelf* s);
extern "C" void r103_openShelf(R103Shelf* s);
extern "C" void r103_setCorpse(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8, void* m9);
extern "C" void r103_setSubMissionTarget(u32 objNo);
static void r103_execOpenCover(R103Cesspit* c);
static void r103_checkCloseCover(R103Cesspit* c);
extern "C" void r103_checkCesspit0(R103Cesspit* c);
extern "C" void r103_checkCesspit1(R103Cesspit* c);
extern "C" void r103_checkCesspit2(R103Cesspit* c);
extern "C" void r103_initCesspit(R103Cesspit* c);
static void r103_BgmStartCheck();

// Room init (in st1_1 and st1_3): the ten corpse models only outside region 0 (Japan hides them and
// area 0xB), Scenario_flg[0] 0x1000, battle-stream timer, the cesspit and its sub-mission target on
// object 8, floor hit effects, rack 6 range, three shelf item events (items 0x92/0x81/0x83), and the
// glowing file at area 0x80 until Item_flg[0] 0x800.
void R103Init()
{
    cEm* rack;

#line 72 "D:/Bio4/Prog/r103.cpp"
    r103_work = (R103Work*) MEM_CALLOC(sizeof(R103Work), 1, 0xd);

    if (pSys->eff_country == 0) {
        SceAtSetEnable(0xB, 0);
    } else {
        r103_setCorpse(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21),
                       ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24),
                       ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27),
                       ROOM_ARC_PTR(pG->pRoom, 0x28));
        EstSet(0, -1, 0, 0, EFF_ROOM, 5, 0, ESP_CORE_KIND_NONE, 0, 0);
    }
    ScfFlagOn(pG, SCF_R103_ENTER);
    SceExec(0x12, (TaskFunc) r103_BgmStartCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r103_initCesspit, (int) &r103_cesspit, 0, SCE_PRIO_DEF_2, 0);
    r103_setSubMissionTarget(8);
    EatMgr.registEffInfo(EAT_ET_ROOM0, (AtEffInfo*) &r103_eff_info);
    if (getRoomEtcRack(6, &rack, 1)) {
        ((cEmRack*) rack)->setRange(0.0f, 3000.0f, 0.0f, 3000.0f);
    }
    SceSetItemEvent(7, 0x92, 0, 0xA, (void (*)(int)) r103_openShelf, (void (*)(int)) r103_openedShelf, (int) &r103_shelf0, 0);
    SceSetItemEvent(8, 0x81, 1, 0xB, (void (*)(int)) r103_openShelf, (void (*)(int)) r103_openedShelf, (int) &r103_shelf1, 0);
    SceSetItemEvent(9, 0x83, 2, 9, (void (*)(int)) r103_openShelf, (void (*)(int)) r103_openedShelf, (int) &r103_shelf2, 0);
    if (!ItfFlagChk(pG, ITF_R103_FILE)) {
        r103_work->eff = EspPullCoreKind();
        EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, (u8) r103_work->eff, 0, 0);
        SceAtDataSet_exec(0x80, SCE_LEVEL10, 0, (TaskFunc) r103_getFile, 0, 1);
    }
    SceAtSetActColor(2, 1);
}

// Per-frame room main: nothing.
void R103Main()
{
}

// The file item was taken: its glow goes away.
static void r103_getFile()
{
    SceAtExecute(0x80);
    EffectEspDelete(0, (u8) r103_work->eff, 0, 0);
    EffectEspgenDelete(0, (u8) r103_work->eff, 0);
    EffectEfmDelete(0, (u8) r103_work->eff, 0);
}

// Open shelf `s` (opened != 0: already open): the two doors turn 110 degrees over 30 frames.
extern "C" void r103_openShelf_main(R103Shelf* s, int opened)
{
    cObj* a;
    cObj* b;

    a = SmdGetObjPtr(s->door[0]);
    b = SmdGetObjPtr(s->door[1]);
    if (a != 0 && b != 0) {
        a->be_flag |= 0x20;
        b->be_flag |= 0x20;
        if (opened == 1) {
            f32 ra = -1.9198622f;
            f32 rb = 1.9198622f;
            // COMPILER-DIFF: candidate #17 (local-alloc qty order of the two pool highs). The
            // original allocates the -1.92 high first (r9); ours allocates the +1.92 high first
            // because the `lwz pList` fills the cycle between its `lis` and `lfs` in sched1, so
            // its span is longer. A pseudo -> hard-register copy of the pList pointer (kept
            // from combine by the volatile load and the keep-alive asm, a no-op deleted by
            // reload_cse after both take r10) is issued between the second `lis` and its `lfs`
            // in sched1, which equalises the spans and lets the qty number decide.
            cModel* pa = *(cModel* volatile*) &a->pList;
            register cModel* pa2 asm("r10");

            pa2 = pa;
            pa2->ang.y = ra;
            asm("" : "=m"(a->be_flag) : "r"(pa2));
            b->pList->ang.y = rb;
        } else {
            int i;

            SndCall(6, 0x1A, 0, 0, 0, 0);
            for (i = 30; i != 0; i--) {
                if (a != 0) {
                    a->pList->ang.y += -0.063995406f;
                }
                if (b != 0) {
                    b->pList->ang.y += 0.063995406f;
                }
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": pose shelf `s` open without the animation.
extern "C" void r103_openedShelf(R103Shelf* s)
{
    r103_openShelf_main(s, 1);
}

// Item-event opener: animate shelf `s` open when its item is taken.
extern "C" void r103_openShelf(R103Shelf* s)
{
    r103_openShelf_main(s, 0);
}

// The ten corpses: scroll objects with the corpse parts models and a motion, darkened by a third.
extern "C" void r103_setCorpse(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8, void* m9)
{
    cEm em;
    // The ctor's `this` pseudo (`addi r3, r1, 8; mr r29, r3`) is what the original addresses subArc
    // through (`0x378(r29)`); a member access on `em` folds to the frame, so the store and the row
    // loads go through the pointer (see r100's R100Init).
    cEm* pe = &em;
    int i;

    *(PlArc**) ((u8*) pe + 0x378) = (PlArc*) EmReadSearch(0x12, 0, 0);
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    {
        // model / two parts models / third parts model / textures / motion. A struct row would
        // get a memset per row (C++ TYPE_FIELDS holds the class name too, so expr.c's field count
        // never matches the initializer); the original used a plain 2-D pointer array.
        void* tbl[10][6] = {
            {PL_ARC_PTR(pe->subArc, 0x1BC), PL_ARC_PTR(pe->subArc, 0x1BE), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1BD), m0},
            {PL_ARC_PTR(pe->subArc, 0x1D8), PL_ARC_PTR(pe->subArc, 0x1DA), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1D9), m1},
            {PL_ARC_PTR(pe->subArc, 0x1E0), PL_ARC_PTR(pe->subArc, 0x1E3), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1E1), m2},
            {PL_ARC_PTR(pe->subArc, 0x1BC), PL_ARC_PTR(pe->subArc, 0x1BE), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1DD), m3},
            {PL_ARC_PTR(pe->subArc, 0x1D8), PL_ARC_PTR(pe->subArc, 0x1DA), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1BD), m4},
            {PL_ARC_PTR(pe->subArc, 0x1E0), PL_ARC_PTR(pe->subArc, 0x1E3), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1D9), m5},
            {PL_ARC_PTR(pe->subArc, 0x1BC), PL_ARC_PTR(pe->subArc, 0x1BE), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1E1), m6},
            {PL_ARC_PTR(pe->subArc, 0x1D8), PL_ARC_PTR(pe->subArc, 0x1DA), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1DD), m7},
            {PL_ARC_PTR(pe->subArc, 0x1E0), PL_ARC_PTR(pe->subArc, 0x1E3), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1BD), m8},
            {PL_ARC_PTR(pe->subArc, 0x1BC), PL_ARC_PTR(pe->subArc, 0x1BE), PL_ARC_PTR(pe->subArc, 0x1C0), PL_ARC_PTR(pe->subArc, 0x1C5), PL_ARC_PTR(pe->subArc, 0x1D9), m9},
        };

        for (i = 0; i < 10; i++) {
            cObj* obj;
            cModelInfo* mi;

            obj = SetObjSmd(tbl[i][0], tbl[i][4], &pos, &rot, 2, 1);
            if ((mi = ModInfoMgr.create(tbl[i][1], tbl[i][4])) != 0) {
                obj->addModel(mi);
            }
            if ((mi = ModInfoMgr.create(tbl[i][2], tbl[i][4])) != 0) {
                obj->addModel(mi);
            }
            if ((mi = ModInfoMgr.create(tbl[i][3], tbl[i][4])) != 0) {
                obj->addModel(mi);
            }
            obj->motionSet(tbl[i][5], 0, 0, 1, 0);
            for (mi = obj->pModelInfo; mi != 0; mi = mi->pList) {
                mi->color[0] = mi->color[0] * 2 / 3;
                mi->color[1] = mi->color[1] * 2 / 3;
                mi->color[2] = mi->color[2] * 2 / 3;
            }
        }
    }
}

// The sub-mission target (etc item 0x13) hangs on scroll object `objNo` until it is taken.
extern "C" void r103_setSubMissionTarget(u32 objNo)
{
    EtcItem* item;
    u16* flg;

    flg = GetEtcFlgPtr(0x13, pG->room_id);
    if (!(*flg & 1)) {
        if (getRoomEtcItem(0x13, &item, 1)) {
            cObj* obj = SmdGetObjPtr(objNo);

            PSVECSubtract(&((cEmItem*) item)->pos, &obj->pos, &((cEmItem*) item)->pos);
            ((cEmItem*) item)->pos.x = -120.0f;
            ((cEmItem*) item)->pos.z = 0.0f;
            ((cEmItem*) item)->setParent(obj, 0, 0);
            ((cEmItem*) item)->setRotType(2);
        }
    }
}

// The cesspit cover swings open.
static void r103_execOpenCover(R103Cesspit* c)
{
    cObj* lid;

    ScfFlagOn(pG, SCF_R103_OPEN_COVER);
    lid = SmdGetObjPtr(c->lid);
    SndCall(6, 9, &lid->pos, 0, 0, 0);
    // `step` a variable (f31 across the call); the exit store on the break path keeps the peeled
    // exit test unfolded so jump2 merges the two exit jumps (docs/matching.md COMPILER-DIFF #7/#9).
    f32 step = 0.06981317f;

    for (;;) {
        lid->pList->ang.x -= step;
        if (lid->pList->ang.x < -1.83f) {
            lid->pList->ang.x = -1.83f;
            break;
        }
        SceSleep(1);
    }
    SceAtSetEnable(c->at14, 1);
}

// The cover: a hit enemy on the scroll object; once shot it falls, the lid opens and bounces.
static void r103_checkCloseCover(R103Cesspit* c)
{
    cObj* cover;
    cObj* lid;
    cEmHit* hit;

    cover = SmdGetObjPtr(c->cover);
    lid = SmdGetObjPtr(c->lid);
    cover->be_flag |= 0x20;
    lid->be_flag |= 0x20;
    cover->ang.x = -0.5235988f;
    hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &cover->pos, &cover->ang, 0);
    {
        // `const`: the single-use constants are loaded in declaration order (w, x, h, z), not in
        // argument order
        const f32 w = 100.0f;
        const f32 h = 1200.0f;
        const f32 x = 0.0f;
        const f32 z = 50.0f;
        YarareInitCube(hit, x, x, z, w, h, w, 0, YAT_FLAG_ON);
    }
    do {
        if (hit->ckStatus() == 1) {
            break;
        }
        SceSleep(1);
    } while (1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x10, 0, ESP_CORE_KIND_NONE, 0, 0);
    SndCall(6, 7, &cover->pos, 0, 0, 0);
    cover->be_flag &= ~2;
    SceAtSetEnable(c->at10, 1);
    ScfFlagOn(pG, SCF_R103_CLOSE_COVER);
    {
        const f32 deg = 0.017453292f;
        f32 spd = 0.0f;
        f32 lim = 1.12f;

        do {
            lid->pList->ang.x += spd;
            if (lid->pList->ang.x > lim) {
                lid->pList->ang.x = 1.12f;
                break;
            }
            spd += 0.017453292f;
            SceSleep(1);
        } while (1);
    }
    SndCall(6, 8, &lid->pos, 0, 0, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x11, 0, ESP_CORE_KIND_NONE, 0, 0);
    ScfFlagOn(pG, SCF_R103_CLOSE_COVER);
    lid->pList->ang.x -= 0.06981317f;
    SceSleep(1);
    lid->pList->ang.x -= 0.02617994f;
    SceSleep(1);
    lid->pList->ang.x += 0.02617994f;
    SceSleep(1);
    lid->pList->ang.x += 0.06981317f;
    SceSleep(1);
}

// The item model of area `at` moves to area `at2`'s model position and `at2` takes it over.
static inline void r103_moveItemModel(SceAtWork* at, SceAtWork* at2)
{
    if (at2->item.pModel != 0 && at->item.pModel != 0) {
        at2->item.pModel->pos = at->item.pModel->pos;
        at2->item.pModel->ang = at->item.pModel->ang;
        at->item.pModel->be_flag &= ~2;
        at->item.pModel = at2->item.pModel;
    }
}

// Cesspit state 0: the cover is still closed; the item found in it moves onto the lid.
extern "C" void r103_checkCesspit0(R103Cesspit* c)
{
    SceAtWork* at;

    at = SceAtPtr(c->itemAt);
    while (1) {
        if (!ScfFlagChk(pG, SCF_R103_CLOSE_COVER)) {
            if (!ScfFlagChk(pG, SCF_R103_ITEM_IN_CESSPIT)) {
                if (SceAtItemFindFlgCk(c->itemAt) == 1) {
                    ScfFlagOn(pG, SCF_R103_ITEM_IN_CESSPIT);
                    at->item.id = 0x89;
                    r103_moveItemModel(at, SceAtPtr(c->itemAt2));
                }
            }
            SceSleep(1);
        } else {
            break;
        }
    }
    at->item.seFind = 5;
    SceAtSetEnable(c->at10, 1);
    if (ScfFlagChk(pG, SCF_R103_ITEM_IN_CESSPIT)) {
        SceAtSetEnable(c->itemAt, 0);
    }
    SceAtDataSet_exec(c->at18, SCE_LEVEL10, 0, (TaskFunc) r103_execOpenCover, c, 1);
    SceAtSetEnable(c->at14, 0);
    r103_checkCesspit1(c);
}

// Cesspit state 1: the cover is open; the item area follows the found / taken flags.
extern "C" void r103_checkCesspit1(R103Cesspit* c)
{
    SceAtWork* at;

    at = SceAtPtr(c->itemAt);
    while (1) {
        if (!ScfFlagChk(pG, SCF_R103_OPEN_COVER)) {
            if (!ScfFlagChk(pG, SCF_R103_ITEM_IN_CESSPIT)) {
                if (!(pG->Room_flg[0] & 0x80000000)) {
                    if (SceAtItemFindFlgCk(c->itemAt) == 1) {
                        SceAtSetEnable(c->at18, 0);
                        pG->Room_flg[0] |= 0x80000000;
                    }
                } else if (!(pG->Room_flg[0] & 0x40000000)) {
                    if (SceAtItemFlgCk(c->itemAt) == 1) {
                        SceAtSetEnable(c->at18, 1);
                        pG->Room_flg[0] |= 0x40000000;
                    }
                }
            }
            SceSleep(1);
        } else {
            break;
        }
    }
    at->item.seFind = 4;
    SceAtSetEnable(c->at10, 0);
    SceAtSetEnable(c->at18, 0);
    if (SceAtItemFlgCk(c->itemAt) == 1) {
        SceExit();
    }
    if (ScfFlagChk(pG, SCF_R103_ITEM_IN_CESSPIT)) {
        at->item.flag2 |= 0x10;
        at->item.pModel->pos.y += 10.0f;
        SceAtSetEnable(c->itemAt, 1);
    }
    r103_checkCesspit2(c);
}

// Cesspit state 2: the lid is open; the item found moves onto the lid.
extern "C" void r103_checkCesspit2(R103Cesspit* c)
{
    SceAtWork* at;

    at = SceAtPtr(c->itemAt);
    while (1) {
        if (!ScfFlagChk(pG, SCF_R103_ITEM_IN_CESSPIT) && SceAtItemFindFlgCk(c->itemAt) == 1) {
            ScfFlagOn(pG, SCF_R103_ITEM_IN_CESSPIT);
            at->item.id = 0x89;
            r103_moveItemModel(at, SceAtPtr(c->itemAt2));
            break;
        }
        SceSleep(1);
    }
}

// Cesspit setup from the saved state.
extern "C" void r103_initCesspit(R103Cesspit* c)
{
    SceAtWork* at;

    at = SceAtPtr(c->itemAt);
    SceAtSetEnable(c->itemAt2, 1);
    SmdGetObjPtr(c->lid)->be_flag |= 0x20;
    if (!ScfFlagChk(pG, SCF_R103_CLOSE_COVER)) {
        SceExec(0x12, (TaskFunc) r103_checkCloseCover, (int) c, 0, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) r103_checkCesspit0, (int) c, 0, SCE_PRIO_DEF_2, 0);
        SceAtSetEnable(c->at10, 0);
        SceAtSetEnable(c->itemAt, 1);
    } else {
        SmdGetObjPtr(c->cover)->be_flag &= ~2;
        if (!ScfFlagChk(pG, SCF_R103_OPEN_COVER)) {
            SmdGetObjPtr(c->lid)->pList->ang.x = 1.12f;
            SceAtDataSet_exec(c->at18, SCE_LEVEL10, 0, (TaskFunc) r103_execOpenCover, c, 1);
            SceAtSetEnable(c->at14, 0);
            if (ScfFlagChk(pG, SCF_R103_ITEM_IN_CESSPIT)) {
                SceAtSetEnable(c->itemAt, 1);
                SceAtSetEnable(c->itemAt, 0);
                SceAtSetEnable(c->at10, 1);
            } else if (SceAtItemFindFlgCk(c->itemAt) == 1) {
                SceAtSetEnable(c->at10, 1);
                SceAtSetEnable(c->itemAt, 1);
            } else {
                SceAtSetEnable(c->at10, 1);
            }
            SceExec(0x12, (TaskFunc) r103_checkCesspit1, (int) c, 0, SCE_PRIO_DEF_2, 0);
        } else {
            SmdGetObjPtr(c->lid)->pList->ang.x = -1.83f;
            SceAtSetEnable(c->at10, 0);
            if (SceAtItemFlgCk(c->itemAt) == 0) {
                SceExec(0x12, (TaskFunc) r103_checkCesspit2, (int) c, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    SceSleep(1);
    if (ScfFlagChk(pG, SCF_R103_ITEM_IN_CESSPIT)) {
        at->item.id = 0x89;
        r103_moveItemModel(at, SceAtPtr(c->itemAt2));
    }
}

// Battle stream 15 seconds after the room start.
static void r103_BgmStartCheck()
{
    u32 cnt = 0;

    while (1) {
        cnt++;
        if (cnt > 0x383) {
            SndRoomStrStart(1, 0xF, 1);
            SceExit();
        }
        SceSleep(1);
    }
}
