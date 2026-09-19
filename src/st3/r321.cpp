#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_wrap.h"
#include "player.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "TexRender.h"
#include "db_log.h"

// Room 3-21 (D:/Bio4/Prog/r321.cpp): the yard where the support helicopter is shot down (event
// r321s00 on area 2, Scenario_flg[2] bit 31): afterwards the wreck model with its smoke and a fire
// render target, four Ganados and a typewriter.

struct R321Work {
    TexRenderMng* tex;   // 0x0  render target of the wreck's fire
};

static u8 r321_texTbl[0x20];
static R321Work* r321_work;

static void r321_heri_down();

// Position a model from three components (inline owning the Vec).
static inline void setPosXYZ(cModel* m, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setPos(&v);
}

// Rotate a model about Y only.
static inline void setAngY(cModel* m, f32 y)
{
    Vec v;

    v.x = 0.0f;
    v.y = y;
    v.z = 0.0f;
    m->setAng(&v);
}
extern "C" void Evt_R321S00_Func(Event* e);
void setTexRender();
void break_heri_set();

// Room init: Debug_flg[1] 0x00200000; the wreck fire render target; until the crash (Room_flg bit 0)
// area 2 = the helicopter crash event (pre-loaded to MRAM), else object 0xF hidden and the wreck placed;
// the s00 callback.
void R321Init()
{
    DbgFlagOn(pG, DBG_WARN_LEVEL_LOW);
#line 45 "D:/Bio4/Prog/r321.cpp"
    r321_work = (R321Work*) MEM_CALLOC(sizeof(R321Work), 1, 0xd);
    setTexRender();
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) r321_heri_down, 0, 1);
        EvtMgr.EvtReadMram("event/evd/r321s00.evd", 0, 0, 0, 0);
    } else {
        SmdSetTrans(0xF, 0);
        break_heri_set();
    }
    EvtMgr.SetFunc("evt_r321s00_func", (void*) Evt_R321S00_Func);
}

// Per-frame room main: nothing.
void R321Main()
{
}

// Area 2: Room_flg bit 0, Scenario_flg[2] bit 31 (the helicopter is lost), event r321s00, Leon placed
// at the crash site facing 1.663 rad, the typewriter 0x16, the wreck, four Ganados (0x67..0x6A).
static void r321_heri_down()
{
    RsfSet(G_ROOM_ID, 0);
    ScfFlagOn(pG, SCF_R321_HERI_DOWN);
    EvtMgr.EvtReadExec("event/evd/r321s00.evd", 0, 0);
    setPosXYZ(pPL, 38020.0f, 13688.0f, -39718.0f);
    setAngY(pPL, 1.663f);
    OpeSetOpenTerm(0x16, 0.0f, 0.0f, 0.0f, 0.0f);
    break_heri_set();
    setEm(0x67, -1, 1, 1, 1);
    setEm(0x68, -1, 1, 1, 1);
    setEm(0x69, -1, 1, 1, 1);
    setEm(0x6A, -1, 1, 1, 1);
}

// Event r321s00 callback (the helicopter is shot down): scroll object 0x17 hidden; cut 1 parents the
// kind-1 light to Leon; the helicopter / wreck event models are positioned and swapped per cut; the
// end restores the room.
extern "C" void Evt_R321S00_Func(Event* e)
{
    Vec pos;
    Vec rot;
    void* mod;
    void* mod2;

    pos.x = 0.0f;
    pos.y = 80.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    switch (e->funcMode) {
    case 0:
        SmdSetTrans(0x17, 0);
        break;
    case 1:
        if (e->NowCut == 1) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    cLight* light = LightMgr.getKindLight(1);

                    if (light) {
                        light->setParent((cModel*) mod);
                    }
                }
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "evm3200", 0, 0) == 1) {
                    ((cModel*) mod2)->LightInfo.EnableMask = 0x40;
                }
                if (e->GetMod(&mod2, "evmc400", 0, 0) == 1) {
                    ((cModel*) mod2)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod2, "em1e00", 0, 0) == 1) {
                    ((cModel*) mod2)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod2, "em1g00", 0, 0) == 1) {
                    ((cModel*) mod2)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod2, "evm3200", 0, 0) == 1) {
                    ((cModel*) mod2)->be_flag |= 0x10;
                }
            }
            break;
        case 0x1C:
            if (e->NowFrame == 0) {
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0xB, 0);
            }
            break;
        }
        if (e->NowCut > 0x10) {
            if (e->NowFrame == 0) {
                SmdSetTrans(0xF, 0);
            }
        } else {
            if (e->NowFrame == 0) {
                SmdSetTrans(0xF, 1);
            }
        }
        break;
    case 2:
        SmdSetTrans(0xA, 1);
        SmdSetTrans(0xB, 1);
        SmdSetTrans(0xF, 0);
        SmdSetTrans(0x17, 1);
        break;
    }
}

// The wreck's fire rendered to texture, blended into scroll object 0.
void setTexRender()
{
    cObj* obj;
    u8* tbl = r321_texTbl;

    if (GetTexRenderMgr(&r321_work->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r321_work->tex->texId;
        IntSet(r321_work->tex->m_Rep_type, 1);
        EstSet(0, -1, 0, 0, 1, 0, r321_work->tex->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(1);
    obj->pModelInfo->color[3] = 0xF0;
}

// The crashed helicopter model and its smoke.
void break_heri_set()
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;

    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, 0x10, 1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 0xA, 0, 1, 0);
    obj->be_flag |= 0x1000;
    obj->motSpeedRate = 0.0f;
    obj->setNoSuspend(1);
    EstSet(0, -1, 0, 0, 1, 1, 1, 0, 0, 0);
    EspGenSetMoveLoop(0xC8);
}
