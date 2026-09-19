#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "TexRender.h"
#include "db_log.h"

// Room 2-29 (D:/Bio4/Prog/r229.cpp): the sewer; the water rendered to texture and the camera
// event of area 3.

struct R229Work {
    TexRenderMng* tex[2];   // 0x00  water render targets
    u8 pad_8[4];
    int eff;                // 0x0C  EspPullCoreKind of the camera event's effect
    u32 str;                // 0x10  SndStrReq handle of the camera event
};

// The work pointer is a struct member: every store through the work reloads it.
struct R229WorkPtr {
    R229Work* p;
};

static u8 r229_texTbl0[0x20];
static u8 r229_texTbl1[0x20];
static R229WorkPtr r229_work;

// Water effect table (PlRegistRoomEff): {id, type} pairs, zero terminated.
static const u32 r229_roomEff[] = {1, 0x21, 1, 0x22, 1, 0x23, 1, 0xA, 1, 0xB, 1, 0xC, 0};

static void r229_openTerm();
static void r221_execEmCamera1_end();
static void r221_execEmCamera1();
static void setTexRender();

// Room init (the sewer): System_flg 0x400 cleared; area 3 = the Ganado-in-the-water camera event until
// Room_flg bit 0; the typewriter once (bit 2); the water render targets, the water splash effect table,
// player OT type 5 (wading), Scenario_flg[2] 0x02000000.
void R229Init()
{
    R229Work*& wp = r229_work.p;   // the store's `lis` sits before the mem_calloc call (r30)

    SysFlagOff(pG, SYS_SCREEN_STOP);
#line 56 "D:/Bio4/Prog/r229.cpp"
    wp = (R229Work*) MEM_CALLOC(sizeof(R229Work), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r221_execEmCamera1, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceExec(0x12, (TaskFunc) r229_openTerm, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    setTexRender();
    PlRegistRoomEff((PlRoomEff*) r229_roomEff);
    pPL->ot_type = 5;
    ScfFlagOn(pG, SCF_R229_IN);
}

// Per-frame room main: nothing.
void R229Main()
{
}

// Dead-stripped by the original link (its strings survive after R229Init's): the s00 event setup
// the room never registered.
extern "C" void Evt_R229S00_Func(Event* e);
// Pre-load r229s00 and register its callback (unused by the room's Init in this build).
static void r229_evtSetup()
{
    EvtMgr.EvtReadAram("event/evd/r229s00.evd", 0, 0, 0, 0);
    EvtMgr.SetFunc("evt_r229s00_func", (void*) Evt_R229S00_Func);
}

// Once (Room_flg bit 2): open typewriter terminal 0x11.
static void r229_openTerm()
{
    RsfSet(G_ROOM_ID, 2);
    OpeSetOpenTerm(0x11, 0.0f, 0.0f, 0.0f, 0.0f);
}

// End of the camera event: the player may suspend again, its effect dropped, its stream faded (200 frames),
// SceEventEnd, the sea area flag cleared.
static void r221_execEmCamera1_end()
{
    pPL->setNoSuspend(0);
    if (r229_work.p->eff != 0) {
        EffectEspDelete(0, (u8) r229_work.p->eff, 0, 0);
        EffectEspgenDelete(0, (u8) r229_work.p->eff, 0);
        EffectEfmDelete(0, (u8) r229_work.p->eff, 0);
    }
    if (r229_work.p->str != 0) {
        SndStrReq(r229_work.p->str, 4, 200, 0);
    }
    SceEventEnd(0);
    SetSstAddAreaFlag(0);
}

// Area 3: the camera shows the Ganado in the water (cut 6).
static void r221_execEmCamera1()
{
    RsfSet(G_ROOM_ID, 0);
    while (SceCheckEventStart() == 0) {
        SceSleep(1);
    }
    SetSstAddAreaFlag(2);
    r229_work.p->str = SndStrReq(1, 0x36, 0x80000003, 0, 0, 0.0f);
    r229_work.p->eff = 0;
    SceSetEventCancel(1, (TaskFunc) r221_execEmCamera1_end, 0, -1, 1);
    SceEventStart(0);
    pPL->setNoSuspend(1);
    CamCtrl.CutCall(6);
    r229_work.p->eff = EspPullCoreKind();
    EstSet(0, -1, 0, 0, 1, 4, 1, (u8) r229_work.p->eff, 0, 0);
    SceSleep(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    EffectEspDelete(0, (u8) r229_work.p->eff, 0, 0);
    EffectEspgenDelete(0, (u8) r229_work.p->eff, 0);
    EffectEfmDelete(0, (u8) r229_work.p->eff, 0);
    r229_work.p->eff = 0;
    CamCtrl.Comeback(0);
    r229_work.p->str = 0;
    SceSetEventCancel(0, 0, 0, -1, 1);
    r221_execEmCamera1_end();
}

// TexRender blend setup of one water object.
#define R229_TEX_OBJ(id, tbl, col, v138, v136, v137) \
    obj = SmdGetObjPtr(id);                          \
    obj->pModelInfo->setTexBlendTbl(tbl);                 \
    obj->pModelInfo->setBlendRatio(0xFF);                 \
    obj->pModelInfo->color[3] = col;                      \
    obj->Shader_type = v136;                                \
    obj->Refract_pow = v137;                                \
    obj->Refract_ratio = v138;

// The water surface: two render targets blended into the water objects.
static void setTexRender()
{
    cObj* obj;
    u8* tbl0 = r229_texTbl0;
    u8* tbl1 = r229_texTbl1;

    if (GetTexRenderMgr(&r229_work.p->tex[0])) {
        tbl0[0] = 1;
        tbl0[1] = 0;
        tbl0[4] = 0xF7;
        tbl0[5] = r229_work.p->tex[0]->texId;
        r229_work.p->tex[0]->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, 1, 0, r229_work.p->tex[0]->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    R229_TEX_OBJ(0xA, tbl0, 0xF0, 0x30, 2, 0x10);
    R229_TEX_OBJ(0xC, tbl0, 0xF0, 0x30, 2, 0x10);
    R229_TEX_OBJ(0xD, tbl0, 0xF0, 0x30, 2, 0x10);
    R229_TEX_OBJ(0xE, tbl0, 0xF0, 0x30, 2, 0x10);
    if (GetTexRenderMgr(&r229_work.p->tex[1])) {
        tbl1[0] = 1;
        tbl1[1] = 0;
        tbl1[4] = 0xF7;
        tbl1[5] = r229_work.p->tex[1]->texId;
        r229_work.p->tex[1]->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, 1, 3, r229_work.p->tex[1]->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0xF);
    obj->pModelInfo->setTexBlendTbl(tbl1);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(1);
    obj->pModelInfo->color[3] = 0xF0;
    obj->Shader_type = 2;
    obj->Refract_pow = 8;
    obj->Refract_ratio = 0x30;
}
