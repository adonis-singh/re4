#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "TexRender.h"
#include "cSceObj.h"
#include "db_log.h"

// Room 4-06 (D:/Bio4/Prog/r406.cpp): the mine of Assignment Ada (Leon's motion set on a normal game):
// the Ganado waves keyed to the item, areas 2 / 3 / 8 and the alert areas 0xB / 0xC (counting the dead
// of the list), the rock wall only the mine explosives break, the water render target and a shelf.

struct R406Work {
    TexRenderMng* tex;   // 0x00
    cSceObj shelf;       // 0x04
};


static u8 r406_texTbl[0x20];
static R406Work* r406_work;

// Hit effects of attribute type 2 (water). The original link 8-aligned r40a's .rodata after this
// table (a zero word); r40a's split object is 4-aligned, so the pad word is carried behind the table.
static const struct {
    AtEffInfo info;
    u32 pad;
} r406_eff_info = {
    {1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F}},
    0,
};

// Death bit of entry `no` of the loaded enemy list (0 while no list is loaded).
static inline u32 r406_emDead(int no)
{
    int list = pG->em_list_no;
    u32 v;

    if (list >= 0) {
        v = FlagChk(pG->Em_flg[list], no);
    } else {
        v = 0;
    }
    return v;
}

void r406_openShelf_main(int no, int mode);
static void r406_openShelf(int no);
static void r406_openedShelf(int no);
static void r406_checkRockWall();
static void r406_setFindPL1();
static void r406_setFindPL2();
static void r406_checkEmSet1();
static void r406_checkEmSet2();
static void r406_checkEmSet3();
static void r406_checkEmSet4();
void setTexRender();

// Room init (the mine, Assignment Ada): the water render target; Ada's (pl_type 2) or Leon's room
// motions; water hit effects; the enemy waves — wave 1 on the item (Room_flg bit 0), waves 2/3 on
// areas 2/3 (bits 1/2), the alert areas 0xB/0xC (bits 6/7), wave 4 on area 8 (bit 5); the rock wall
// until bit 3 (else area 5 off); the shelf mover and item event.
void R406Init()
{
    cObj* obj;

#line 57 "D:/Bio4/Prog/r406.cpp"
    r406_work = (R406Work*) MEM_CALLOC(sizeof(R406Work), 1, 0xd);
    setTexRender();
    if (pG->pl_type == 2) {
        PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x2B),
                       ROOM_ARC_PTR(pG->pRoom, 0x2C), ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2E), 0, 0,
                       ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28), ROOM_ARC_PTR(pG->pRoom, 0x29),
                       ROOM_ARC_PTR(pG->pRoom, 0x2A));
    } else {
        PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0, 0, 0, 0,
                       ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23),
                       ROOM_ARC_PTR(pG->pRoom, 0x24));
    }
    EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &r406_eff_info.info);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceExec(0x12, (TaskFunc) r406_checkEmSet1, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r406_checkEmSet2, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r406_checkEmSet3, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceAtDataSet_exec(0xB, SCE_LEVEL10, 0, (TaskFunc) r406_setFindPL1, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        SceAtDataSet_exec(0xC, SCE_LEVEL10, 0, (TaskFunc) r406_setFindPL2, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        SceExec(0x12, (TaskFunc) r406_checkEmSet4, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceExec(0x12, (TaskFunc) r406_checkRockWall, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceAtSetEnable(5, 0);
        obj = SmdGetObjPtr(0x2D);
        if (obj) {
            obj->be_flag &= ~2;
        }
    }
    obj = SmdGetObjPtr(0x3C);
    if (obj) {
        Vec d = {0.0f, 3270.0f, 0.0f};

        r406_work->shelf.initMove1_pos(obj, 90, &d, 20.0f, 0.0f);
        r406_work->shelf.setEndPos();
        SceSetItemEvent(6, 0x82, 4, 5, r406_openShelf, r406_openedShelf, 0, 0);
    }
}

// Per-frame room main: nothing.
void R406Main()
{
}

// Shelf 0 (chest 0x2E, parts lid up +Z) opens (mode 1: snap).
void r406_openShelf_main(int no, int mode)
{
    if (no == 0) {
        OpenBoxMain(OpenBoxPartsUpZP, mode, 0x5B, 0x2E, -1, -1);
    }
}

// Item-event opener: animate the shelf open.
static void r406_openShelf(int no)
{
    r406_openShelf_main(no, 0);
}

// Item-event "already opened": the shelf posed open.
static void r406_openedShelf(int no)
{
    r406_openShelf_main(no, 1);
}

// The breakable rock wall: a hit object that only the mine explosives break.
static void r406_checkRockWall()
{
    Vec pos = {9293.0f, -6600.0f, 8202.0f};
    cEm* dram;

    if (getRoomEtcDram(0x12, &dram, 1)) {
        pos = dram->pos;
    }
    {
        Vec rot = {0.0f, 0.0f, 0.0f};
        cEmHit* em = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &pos, &rot, 1);

        YarareInit(em, 0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 0, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        SceSleep(1);
        for (;;) {
            if (em->ckStatus() == 1) {
                u32 wep = em->ckDmgWeapon();

                switch (wep) {
                case 0xD:
                case 0x12:
                case 0x13: {
                    cObj* obj;
                    u32 zero = 0;

                    RsfSet(G_ROOM_ID, 3);
                    SceAtSetEnable(5, 0);
                    EmMgr.destroy(em);
                    obj = SmdGetObjPtr(0x2D);
                    if (obj) {
                        obj->be_flag &= ~2;
                    }
                    SndCall(6, 2, &obj->pos, 0, 0, 0);
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
                    SceExit();
                    break;
                }
                }
            }
            SceSleep(1);
        }
    }
}

// Area 0xB: Ganado 0x35 notices the player.
static void r406_setFindPL1()
{
    cEmWrap em;

    em.setPtr(0x35, -1, 1);
    em.setFindPL();
}

// Area 0xC: Ganado 0x34 notices the player and turns hostile.
static void r406_setFindPL2()
{
    cEmWrap em;

    em.setPtr(0x34, -1, 1);
    em.setFindPL();
    em.setFlag(1);
}

// Wave 1: once item 0x82 is taken (Room_flg bit 0), with two of list 40..42 dead, Ganados 0x3A/0x3B come.
static void r406_checkEmSet1()
{
    u32 cnt;

    while (SceAtItemFlgCk(0x82) != 1) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 0);
    cnt = 0;
    if (r406_emDead(40)) {
        cnt++;
    }
    if (r406_emDead(41)) {
        cnt++;
    }
    if (r406_emDead(42)) {
        cnt++;
    }
    if (cnt > 1) {
        setEm(0x3A, -1, 1, 1, 1);
        setEm(0x3B, -1, 1, 1, 1);
    }
}

// Area 2 (Room_flg bit 1): counts the dead of list 40..45; enough -> Ganado 0x2E walks to area 4's centre.
static void r406_checkEmSet2()
{
    u32 cnt;

    RsfSet(G_ROOM_ID, 1);
    cnt = 0;
    if (r406_emDead(40)) {
        cnt++;
    }
    if (r406_emDead(41)) {
        cnt++;
    }
    if (r406_emDead(42)) {
        cnt++;
    }
    if (r406_emDead(43)) {
        cnt++;
    }
    if (r406_emDead(44)) {
        cnt++;
    }
    if (r406_emDead(45)) {
        cnt++;
    }
    {
        cEmWrap em;

        if (cnt > 1) {
            Vec pos;

            em.setEm(0x2E, -1, 1, 1, 1);
            SceAtGetCenterPos(&pos, 4);
            em.setGoto(&pos, 1);
            while (em.ckGoto()) {
                SceSleep(1);
            }
            em.setFindPL();
        }
    }
}

// Area 3 (Room_flg bit 2): counts the dead of list 47..52; more than two -> Ganados 0x37/0x38/0x39.
static void r406_checkEmSet3()
{
    u32 cnt;

    RsfSet(G_ROOM_ID, 2);
    cnt = 0;
    if (r406_emDead(47)) {
        cnt++;
    }
    if (r406_emDead(48)) {
        cnt++;
    }
    if (r406_emDead(49)) {
        cnt++;
    }
    if (r406_emDead(50)) {
        cnt++;
    }
    if (r406_emDead(51)) {
        cnt++;
    }
    if (r406_emDead(52)) {
        cnt++;
    }
    if (r406_emDead(53)) {
        cnt++;
    }
    if (cnt > 2) {
        setEm(0x37, -1, 1, 1, 1);
        setEm(0x38, -1, 1, 1, 1);
        setEm(0x39, -1, 1, 1, 1);
    }
}

// Task: when the player stands in area 8 with two of list 40..42 dead, Ganado 0x36 comes (Room_flg bit 5).
static void r406_checkEmSet4()
{
    cEmWrap em;
    Vec pos;

    while (1) {
        if (SceAtHitCheck(8) == 1) {
            u32 cnt = 0;

            if (r406_emDead(40)) {
                cnt++;
            }
            if (r406_emDead(41)) {
                cnt++;
            }
            if (r406_emDead(42)) {
                cnt++;
            }
            if (cnt > 1) {
                em.setEm(0x36, -1, 1, 1, 1);
                RsfSet(G_ROOM_ID, 5);
                break;
            }
        }
        SceSleep(1);
    }
    SceAtGetCenterPos(&pos, 9);
    em.setGoto(&pos, 0xB);
    while (em.ckGoto()) {
        SceSleep(1);
    }
    em.setFindPL();
}

// The water surface: a render target blended into the water object.
void setTexRender()
{
    cObj* obj;
    u8* tbl = r406_texTbl;

    if (GetTexRenderMgr(&r406_work->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r406_work->tex->GetTexNo();
        r406_work->tex->SetRepeatType(1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 3, r406_work->tex->GetCoreFlg() | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "SetTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0x15);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
}
