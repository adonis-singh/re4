#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_wrap.h"
#include "esp.h"
#include "snd.h"
#include "TexRender.h"
#include "db_log.h"

// Room 3-03 (D:/Bio4/Prog/r303.cpp): the water render target, the shelf, the boxes and the door that
// falls in once the Ganado of the corridor is set.

struct R303Work {
    TexRenderMng* tex;   // 0x00
};


static u8 r303_texTbl[0x20];
static R303Work* r303_work;

Vec r303_doorPos = {-15576.0f, 156.0f, 12168.0f};
Vec r303_doorAng = {-0.0644f, 0.1753f, 1.57095f};

void r303_TanaMove(int mode);
static void r303_openTana(int no);
static void r303_openedTana(int no);
static void r303_DuraluminCaseOpen(int no);
static void r303_DuraluminCaseOpened(int no);
static void r303_DustBoxOpen(int no);
static void r303_DustBoxOpened(int no);
static void oneshot_bgm();
static void door_down();
void setTexRender();

// Room init: the water render target, no splashes; until Room_flg bit 0 area 3 = the door-fall event,
// else the door object 0xB posed fallen; shelf / duralumin case / dust box item events; a one-shot
// stream on area 6 (bit 3); Scenario_flg[2] 0x00200000.
void R303Init()
{
#line 52 "D:/Bio4/Prog/r303.cpp"
    r303_work = (R303Work*) MEM_CALLOC(sizeof(R303Work), 1, 0xd);
    setTexRender();
    Espgen42SetNoWater(1);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) door_down, 0, 1);
    } else {
        SmdGetObjPtr(0xB)->setPos(&r303_doorPos);
        SmdGetObjPtr(0xB)->setAng(&r303_doorAng);
    }
    SceSetItemEvent(5, 0x80, 2, 1, r303_openTana, r303_openedTana, 0, 0);
    SceSetItemEvent(7, 0x81, 4, 3, r303_DuraluminCaseOpen, r303_DuraluminCaseOpened, 0x19, 0);
    SceSetItemEvent(8, 0x83, 5, 2, r303_DustBoxOpen, r303_DustBoxOpened, 0x1B, 0);
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) oneshot_bgm, 0, 1);
    }
    ScfFlagOn(pG, SCF_R303_IN);
}

// The shelf swings open (mode 1: already open).
void r303_TanaMove(int mode)
{
    cObj* obj = SmdGetObjPtr(0x17);

    obj->be_flag |= 0x20;
    // r204_BoxMove idiom: the `const f32` limit declared before the if (its pool high shared by the arm
    // and the loop entry in a callee-saved register), the step inside the loop body.
    const f32 lim = 2.83f;
    if (mode == 1) {
        obj->ang.z = lim;
    } else {
        SndCall(6, 0x1C, &obj->pos, 0, 0, 0);
        while (1) {
            const f32 spd = 0.09f;

            obj->ang.y += spd;
            if (obj->ang.y > lim) {
                obj->ang.y = lim;
                break;
            }
            SceSleep(1);
        }
    }
}

// Item-event opener: the shelf swings open.
static void r303_openTana(int no)
{
    r303_TanaMove(0);
}

// Item-event "already opened": the shelf posed open.
static void r303_openedTana(int no)
{
    r303_TanaMove(1);
}

// Item-event opener: the duralumin case (OpenBoxMain type 7) opens.
static void r303_DuraluminCaseOpen(int no)
{
    OpenBoxMain(7, 0, 0x18, no, -1, -1);
}

// Item-event "already opened": the case posed open.
static void r303_DuraluminCaseOpened(int no)
{
    OpenBoxMain(7, 1, -1, no, -1, -1);
}

// Item-event opener: the dust box (type 8) lid opens.
static void r303_DustBoxOpen(int no)
{
    OpenBoxMain(8, 0, 4, no, -1, -1);
}

// Item-event "already opened": the dust box posed open.
static void r303_DustBoxOpened(int no)
{
    OpenBoxMain(8, 1, -1, no, -1, -1);
}

// Per-frame room main: nothing.
void R303Main()
{
}

// Area 6: the one-shot stream.

// Area 6 once (Room_flg bit 3): stream 0x34 plays once.
static void oneshot_bgm()
{
    // The 0.0 is loaded after the RsfSet store: a pool constant would move above it (pool loads never
    // depend on stores), a `static const` read through a reference stays below (r40e idiom).
    static const f32 vol = 0.0f;

    RsfSet(G_ROOM_ID, 3);
    SndStrReq(0, 0x34, 0x80000003, 0, 0, *(const f32*) &vol);
}

// Area 3: the door falls over while a Ganado steps through it.
static void door_down()
{
    Vec pos;
    Vec ang;
    cEm* em;
    u32 cnt;
    f32 t;
    u8 zero = 0;

    RsfSet(G_ROOM_ID, 0);
    SmdGetObjPtr(0xB)->type = zero;
    em = setEm(0x24, -1, 1, 1, 1);
    em->flag |= 1;
    EstSet(em, -1, 0, 0, EFF_ROOM, 0x10, 0, ESP_CORE_KIND_NONE, em, (void*) zero);
    EstSet(0, -1, 0, 0, EFF_ROOM, 2, 0, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    t = 0.01f;
    cnt = 0;
    SndCall(6, 2, &SmdGetObjPtr(0xB)->pos, 0, 0, 0);
    for (;;) {
        if (cnt > 60) {
            em->hp = 0;
        } else {
            cnt++;
        }
        pos = SmdGetObjPtr(0xB)->pos;
        ang = SmdGetObjPtr(0xB)->ang;
        pos.x += (r303_doorPos.x - pos.x) * 0.4f;
        pos.y += (r303_doorPos.y - pos.y) * 0.4f;
        pos.z += (r303_doorPos.z - pos.z) * 0.4f;
        ang.x += (r303_doorAng.x - ang.x) * 0.05f;
        ang.y += (r303_doorAng.y - ang.y) * 0.05f;
        ang.z += t;
        if (ang.z > r303_doorAng.z) {
            ang.z = r303_doorAng.z;
        }
        t += 0.03f;
        SmdGetObjPtr(0xB)->setPos(&pos);
        SmdGetObjPtr(0xB)->setAng(&ang);
        SceSleep(1);
    }
}

// The water surface: a render target blended into the two water objects.
void setTexRender()
{
    cObj* obj;
    u8* tbl = r303_texTbl;

    if (GetTexRenderMgr(&r303_work->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r303_work->tex->GetTexNo();
        r303_work->tex->SetRepeatType(1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, r303_work->tex->GetCoreFlg() | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "SetTexrender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0xC);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(2);
    obj = SmdGetObjPtr(0x10);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(2);
}
