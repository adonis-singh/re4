#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "main_sub.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "emdoor.h"
#include "em10.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "pl_cloth.h"
#include "pendulum.h"
#include "cam_ctrl.h"
#include "math_sub.h"
#include "mes.h"
#include "rnd.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "TexRender.h"
#include "db_log.h"

// Room 2-13 (D:/Bio4/Prog/r213.cpp): the drawbridge over the lake - the statue ("Su") that is shot
// down, the switch, the two chains holding the bridge and the enemies set after the s00 event.

// Hit boxes of the statue's cEmHit: EmHitWork keeps them from 0x1C.
struct R213SuYarare {
    u8 pad_0[0x1C];
    YARARE_INFO box[5];
};

struct R213Work {
    TexRenderMng* tex;    // 0x00  statue render target
    u32 pad_4;
    cEmHit* hit[5];       // 0x08  [0] the statue, [1] [2] the chains
    cEmWrap em[9];        // 0x1C
    cSat* sat[3];         // 0x88
    cSat* eat[3];         // 0x94
    cObjChain* chain[2];  // 0xA0
    f32 ang;              // 0xA8  bridge angle
    f32 angCur;           // 0xAC  angle BridgeAngMove is heading for
    f32 angDown;          // 0xB0  angle after a chain break
    int breakNum;         // 0xB4  chains broken
    u32 str;              // 0xB8  SndStrPlayBlock handle
};

// The work pointer is a struct member: every store through the work reloads it.
struct R213WorkPtr {
    R213Work* p;
};

// Pointer stores through a reference: the pG loads that follow stay below them.
static inline void PSetSat(cSat*& d, cSat* v) { d = v; }
static inline void PSetTex(TexRenderMng*& d, TexRenderMng* v) { d = v; }
// The Vec address is substituted into the argument register at each call (no PRE copy of the
// frame slot two block-scoped Vecs share).
static inline void SetAngV(cModel* m, Vec* v) { m->setAng(v); }
static inline void SetPosV(cModel* m, Vec* v) { m->setPos(v); }

static u8 r213_texTbl[0x20];
static R213WorkPtr r213_work;
static R213SuYarare* r213_suYarare;
PenCloth r213_cloth;

static Vec r213_satPos = {-14500.0f, 0.0f, -53000.0f};
static Vec r213_satRot = {0.0f, 0.0f, 0.0f};
static f32 r213_suSpdX = 0.07f;
static f32 r213_suSpdY = 0.06f;
static f32 r213_suSpeedZ = 0.04f;
static f32 r213_suAmpX = 0.04f;
static f32 r213_suAmpY = 0.005f;
static f32 r213_suAmpZ = 0.03f;
static u8 r213_chainParts[20] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0, 0};
static u8 r213_chainUp[20] = {0xFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0, 0};
static u8 r213_chainDown[20] = {2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0xFF, 0, 0};
int r213_chainNum = 0x12;
// the split object's .data is 8-aligned
asm(".section .data\n\t.balign 8\n\t.text");

static void OpenedBoxTreasure(int id);
static void OpenBoxTreasure(int id);
void R213SuInit();
static void R213SuMove();
void R213SuBreakModel();
static void R213EventSuBreakMain();
static void R213EventSuBreakEnd();
void R213EmSet();
void R213BridgeInit();
void R213ChainInit(int no, u32 objId, int hitNo, int flagNo);
void R213StatusSetSwitch(int on);
void R213StatusSetBridge(int mode);
void R213StatusSetChain(int mode, int no, u32 objId, int hitNo, int flagNo);
static void R213BridgeManager();
void R213BridgeAngMove(int mode, f32 target);
void R213BridgeAngSet(f32 ang);
void R213ChainAngSet(int no, u32 objId, int hitNo, int flag, f32 ang);
// COMPILER-DIFF 1: R213BridgeAngSet's two calls issue the `fmr f1` after the third / before the
// first integer argument move.
void R213ChainAngSetF3(int no, u32 objId, int hitNo, f32 ang, int flag) asm("R213ChainAngSet__FiUliif");
void R213ChainAngSetF0(f32 ang, int no, u32 objId, int hitNo, int flag) asm("R213ChainAngSet__FiUliif");
void R213ChainDamageCheck(int no, u32 objId, int hitNo, int flagNo);
void R213ChainBreakNumCalc();
static void R213EventSwitchMain();
static void R213EventSwitchEnd();
static void R213EventChainBreakMove(int which);
static void R213EventChainBreakEnd();
static void R213EventBridgeDownMain();
static void R213EventBridgeDownEnd();
static void SceBgmCheck();
static void R213Event();
extern "C" void Evt_R213S00_Func(Event* e);

#define R213_EM_ARC(no) ((void*) (pG->pArc->ofs_##no + (u32) pG->pArc))
// COMPILER-DIFF: 3 -- the varargs view of memset gives the `crclr; bl memset` of the `Vec rot = {0,0,0}`
// libcall for an explicit call (see R213Init).
extern "C" void* r213_memset(void*, ...) asm("memset");

// The collision pieces are set up in R213Init itself: `pos` at the frame base is the frame pointer
// (fresh `addi r6,r1,8` per create call). The original's `&rot` is the memset argument pseudo P
// (`addi r3,r1,24`, computed after the pos memset) copied into the create calls' register between
// the argument move and the call (`li r4,0; mr r31,r3`), and `&door0` (the PRE copy `addi r26,r1,40`)
// sits after the rot memset; our gcse/sched1 hoist both above the preceding call (COMPILER-DIFF 3 (c)).
// Spelled out: the hard-register `a3` is the argument (its set depends on the pos memset's r3
// clobber), `pr = a3` is the copy, and the codeless volatile asm keeps the `&door0` insertion at the
// block end behind the second memset.
void R213Init()
{
#line 67 "D:/Bio4/Prog/r213.cpp"
    r213_work.p = (R213Work*) MEM_CALLOC(sizeof(R213Work), 1, 0xd);
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot;
    Vec* pr;
    cEm* door0;
    cEm* door1;
    u32 i;

    {
        register Vec* a3 asm("r3"); // COMPILER-DIFF: 3
        a3 = &rot;
        pr = a3;
        r213_memset(a3, 0, sizeof(Vec));
    }
    asm volatile(""); // COMPILER-DIFF: 3
    for (i = 0; i < 3; i++) {
        r213_work.p->sat[i] = 0;
        r213_work.p->eat[i] = 0;
    }
    PSetSat(r213_work.p->sat[0], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, pr, 1));
    PSetSat(r213_work.p->eat[0], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, pr, 1));
    PSetSat(r213_work.p->sat[1], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, pr, 2));
    PSetSat(r213_work.p->sat[2], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, pr, 3));
    r213_work.p->eat[2] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r213_satPos, &r213_satRot, 2);
    EvtMgr.SetFunc("evt_r213s00_func", (void*) Evt_R213S00_Func);
    if (getRoomEtcDoor(0x22, &door0, 1) && getRoomEtcDoor(0x23, &door1, 1)) {
        ((cEmDoor*) door0)->setDoor((cEmDoor*) door1);
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtSetEnable(2, 1);
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) R213Event, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r213s00.evd", 0x2D, 0, 0, 0);
    } else {
        SceAtSetEnable(2, 0);
        R213EmSet();
    }
    R213SuInit();
    SceSetItemEvent(0xA, 0x81, 9, 9, OpenBoxTreasure, (void (*)()) OpenedBoxTreasure, 0x81, 0);
    SceSetItemEvent(0xB, 0x84, 0xA, 0xC, OpenBoxTreasure, (void (*)()) OpenedBoxTreasure, 0x84, 0);
    SceSetItemEvent(0xC, 0x90, 0xB, 0xD, OpenBoxTreasure, (void (*)()) OpenedBoxTreasure, 0x90, 0);
    R213BridgeInit();
    SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    SetSstAddAreaFlag(0x800);
}

// Per-frame room main: nothing.
void R213Main()
{
}

// Item-event "already opened": pose the chest / drawer of item `id` open (0x81 chest, 0x84 / 0x90 drawers -Z).
static void OpenedBoxTreasure(int id)
{
    if (id == 0x81) {
        OpenBoxMain(OpenBoxUpXM, 1, 0x5B, 0x42, -1, -1);
    }
    if (id == 0x84) {
        OpenBoxMain(OpenBoxPosZM500, 1, 0x1B, 0x44, -1, -1);
    }
    if (id == 0x90) {
        OpenBoxMain(OpenBoxPosZM500, 1, 0x1B, 0x45, -1, -1);
    }
}

// Item-event opener: animate the chest / drawer of item `id` open.
static void OpenBoxTreasure(int id)
{
    if (id == 0x81) {
        OpenBoxMain(OpenBoxUpXM, 0, 0x5B, 0x42, -1, -1);
    }
    if (id == 0x84) {
        OpenBoxMain(OpenBoxPosZM500, 0, 0x1B, 0x44, -1, -1);
    }
    if (id == 0x90) {
        OpenBoxMain(OpenBoxPosZM500, 0, 0x1B, 0x45, -1, -1);
    }
}

// The statue: its render target, hit boxes and hit points (already broken: the broken model).
void R213SuInit()
{
    cObj* obj;
    u8* tbl = r213_texTbl;

    PSetTex(r213_work.p->tex, 0);
    if (RsfCheck(G_ROOM_ID, 2)) {
        R213SuBreakModel();
    } else {
        SceAtSetEnable(0x87, 0);
        SceAtSetEnable(0x88, 0);
        SceAtSetEnable(0x89, 0);
        SceAtSetEnable(0x8A, 0);
        SceAtSetEnable(0x8B, 0);
        SceAtSetEnable(0x8C, 0);
        SceAtSetEnable(0x8D, 0);
        SceAtSetEnable(0x8E, 0);
        if (GetTexRenderMgr(&r213_work.p->tex)) {
            tbl[0] = 1;
            tbl[1] = 0;
            tbl[4] = 0xF7;
            tbl[5] = r213_work.p->tex->texId;
            EstSet(0, -1, 0, 0, 1, 0, r213_work.p->tex->mask | 1, 2, 0, 0);
        } else {
            pLog->err(0, 0, "R213Init() : Manager alloc failed!!");
        }
        obj = SmdGetObjPtr(0x12);
        if (obj) {
            obj->pModelInfo->setTexBlendTbl(tbl);
            obj->pModelInfo->setBlendRatio(0xFF);
            obj->Shader_type = 1;
            obj->Refract_pow = 0xF;
            obj->Refract_ratio = 0xB4;
            obj->pModelInfo->setSpecular(0xFF, 0xFF, 0xFF);
        }
        EstSet(0, -1, 0, 0, 1, 5, 1, 2, 0, 0);
        SceExec(0x12, (TaskFunc) R213SuMove, 0, 0, SCE_PRIO_DEF_2, 0);
        {
            cObj* o = SmdGetObjPtr(0x12);

            if (o) {
                r213_work.p->hit[0] = SetEmHit(R213_EM_ARC(20), R213_EM_ARC(24), &o->pos, 0, 1);
            if (r213_work.p->hit[0]) {
                cEmHit* hit = r213_work.p->hit[0];

                r213_suYarare = (R213SuYarare*) hit->free;
                YarareInit(hit, 0.0f, -9000.0f, 0.0f, 6000.0f, 15000.0f, 0, 0x81);
                YarareAdd(hit, &r213_suYarare->box[0], 200.0f, -12000.0f, 600.0f, 5000.0f, 0.0f, 0, 0x81);
                YarareAdd(hit, &r213_suYarare->box[1], 100.0f, -15000.0f, 800.0f, 3700.0f, 0.0f, 0, 0x81);
                YarareAdd(hit, &r213_suYarare->box[2], 300.0f, -17500.0f, 500.0f, 2900.0f, 0.0f, 0, 0x81);
                YarareAdd(hit, &r213_suYarare->box[3], 500.0f, -20000.0f, 300.0f, 1800.0f, 0.0f, 0, 0x81);
                hit->hp = 6000;
            }
            }
        }
    }
}

// Task: the statue breathes (scale) and takes the shots.
static void R213SuMove()
{
    cObj* o11 = SmdGetObjPtr(0x11);
    cObj* o12 = SmdGetObjPtr(0x12);

    if (o11 && o12) {
        f32 a0 = 0.0f;
        f32 a1 = 0.0f;
        f32 a2 = 0.0f;
        int timer = 0;

        o11->be_flag |= 0x20;
        o12->be_flag |= 0x20;
        for (;;) {
            if (--timer <= 0) {
                u8 r = Rnd() % 20;

                timer = r + 60;
                SndCall(6, 3, &o11->pos, 0, 0, 0);
            }
            a0 += r213_suSpdX;
            a1 += r213_suSpdY;
            a2 += r213_suSpeedZ;
            o11->scale.x = sinf(a0) * r213_suAmpX + 1.0f;
            o11->scale.y = cosf(a1) * r213_suAmpY + 1.0f;
            o11->scale.z = sinf(a2) * r213_suAmpZ + 1.0f;
            o12->scale.x = sinf(a0) * r213_suAmpX + 1.0f;
            o12->scale.y = cosf(a1) * r213_suAmpY + 1.0f;
            o12->scale.z = sinf(a2) * r213_suAmpZ + 1.0f;
            if (RsfCheck(*(u16*) &pGS->stage_no, 2) == 0) {   // struct view: the pG load stays below the scale store
                cEmHit* hit = r213_work.p->hit[0];

                if (hit) {
                    if (hit->ckStatus() == 1) {
                        Vec pos;
                        Vec dir;
                        int dm;

                        switch (hit->dmg.m_Wep) {
                        case 0xB:
                        case 0xC:
                        case 0x1B:
                        case 0x27:
                            EmDmBloodSet2(hit, 1, 0x10, 0, 0, 2);
                            break;
                        default:
                            EmDmBloodSet2(hit, 1, 6, 0, 0, 2);
                            break;
                        }
                        if (EmGetDmPos(hit, &pos, &dir)) {
                            SndCall(6, 0xB, &pos, 0, 0, 0);
                        }
                        dm = 100;
                        if (hit->dmg.m_Wep <= 0x2D) {
                            dm = GetWepDmVal(hit, hit->dmg.m_Wep, 0);
                            if (GetWepSizeGroup(hit->dmg.m_Wep) == 0) {
                                dm = (u32) ((f32) dm * 0.64f);
                            }
                        }
                        hit->hp -= dm;
                        if (hit->hp <= 0) {
                            hit->hp = 0;
                            SceExec(0x12, (TaskFunc) R213EventSuBreakMain, 0, 0, SCE_PRIO_DEF_2, 0);
                            return;
                        }
                    }
                }
            }
            SceSleep(1);
        }
    }
}

// The broken statue: its models off, the pieces' areas on, the enemies behind it gone.
void R213SuBreakModel()
{
    SmdSetTrans(0x11, 0);
    SmdSetTrans(0x12, 0);
    SceAtSetEnable(0x87, 1);
    SceAtSetEnable(0x88, 1);
    SceAtSetEnable(0x89, 1);
    SceAtSetEnable(0x8A, 1);
    SceAtSetEnable(0x8B, 1);
    SceAtSetEnable(0x8C, 1);
    SceAtSetEnable(0x8D, 1);
    SceAtSetEnable(0x8E, 1);
    if (r213_work.p->sat[0]) {
        r213_work.p->sat[0]->m_Flag &= ~4;
    }
    if (r213_work.p->eat[0]) {
        r213_work.p->eat[0]->m_Flag &= ~4;
    }
    r213_work.p->em[0].setFlag(0x20000000);
    r213_work.p->em[1].setFlag(0x20000000);
    r213_work.p->em[2].setFlag(0x20000000);
    r213_work.p->em[4].setFlag(0x20000000);
    r213_work.p->em[5].setFlag(0x20000000);
    r213_work.p->em[7].setFlag(0x20000000);
    r213_work.p->em[8].setFlag(0x20000000);
    r213_work.p->em[3].setFlag(0x20000000);
    r213_work.p->em[6].setFlag(0x20000000);
    r213_work.p->em[3].setHp(0);
    r213_work.p->em[6].setHp(0);
}

// Task: the statue collapses (cut 5).
static void R213EventSuBreakMain()
{
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        cEmHit* hit;
        int i;

        RsfSet(G_ROOM_ID, 2);
        SceEventStart(1);
        hit = r213_work.p->hit[0];
        if (hit) {
            EffectEspDelete(0, 2, (u32) hit, 0);
            EffectEspgenDelete(0, 2, (int) hit);
            EffectEfmDelete(0, 2, (int) hit);
        }
        EffectEspDelete(1, 2, 0, 0);
        EffectEspgenDelete(1, 2, 0);
        EffectEfmDelete(1, 2, 0);
        EstSet(0, -1, 0, 0, 1, 3, 0x2001, 3, 0, 0);
        SmdSetTrans(0x11, 0);
        SmdSetTrans(0x12, 0);
        SceSetEventCancel(1, (TaskFunc) R213EventSuBreakEnd, 0, -1, 1);
        r213_work.p->str = SndStrPlayBlock(1, 4, 0.0f);
        CamCtrl.CutCall(5);
        for (i = 210; i != 0; i--) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R213EventSuBreakEnd();
    }
}

// End of the statue collapse (also its cancel path): stream stopped, the broken model shown, effects
// dropped, camera back, SceEventEnd, task exit.
static void R213EventSuBreakEnd()
{
    SndStrReq(r213_work.p->str, 8, 0, 0);
    R213SuBreakModel();
    EffectEspDelete(0x2001, 3, 0, 0);
    EffectEspgenDelete(0x2001, 3, 0);
    EffectEfmDelete(0x2001, 3, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// The enemies of the s00 event; the two behind the statue only while it stands.
void R213EmSet()
{
    r213_work.p->em[0].setPtr(0xC2, -1, 0);
    r213_work.p->em[1].setPtr(0xC3, -1, 0);
    r213_work.p->em[2].setPtr(0xC4, -1, 0);
    r213_work.p->em[4].setPtr(0xC6, -1, 0);
    r213_work.p->em[5].setPtr(0xC7, -1, 0);
    r213_work.p->em[7].setPtr(0xC9, -1, 0);
    r213_work.p->em[8].setPtr(0xCA, -1, 0);
    if (RsfCheck(G_ROOM_ID, 2)) {
        r213_work.p->em[0].setFlag(0x20000000);
        r213_work.p->em[1].setFlag(0x20000000);
        r213_work.p->em[2].setFlag(0x20000000);
        r213_work.p->em[4].setFlag(0x20000000);
        r213_work.p->em[5].setFlag(0x20000000);
        r213_work.p->em[7].setFlag(0x20000000);
        r213_work.p->em[8].setFlag(0x20000000);
    } else {
        r213_work.p->em[3].setPtr(0xC5, -1, 0);
        r213_work.p->em[6].setPtr(0xC8, -1, 0);
    }
}

// The drawbridge: area 4 = the switch until pulled (Room_flg bit 1), the two chains (objects 0x40/0x41,
// hit boxes 1/2, flags 3/4), the broken count; while the bridge has not fallen (bit 7) the chain watcher
// runs with the bridge up (bit 1 clear) or held by the chains, else the bridge lies down.
void R213BridgeInit()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) R213EventSwitchMain, 0, 1);
        R213StatusSetSwitch(0);
    } else {
        R213StatusSetSwitch(1);
    }
    R213ChainInit(0, 0x40, 1, 3);
    R213ChainInit(1, 0x41, 2, 4);
    R213ChainBreakNumCalc();
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        SceExec(0x12, (TaskFunc) R213BridgeManager, 0, 0, SCE_PRIO_DEF_2, 0);
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            R213StatusSetBridge(0);
        } else {
            R213StatusSetBridge(1);
        }
    } else {
        R213StatusSetBridge(2);
    }
}

// Chain `no`: created; intact and switch pulled -> its hit box; broken (flagNo) -> falling (bridge up) or gone (bridge down).
void R213ChainInit(int no, u32 objId, int hitNo, int flagNo)
{
    R213StatusSetChain(0, no, objId, hitNo, flagNo);
    if (RsfCheck(G_ROOM_ID, flagNo) == 0) {
        if (RsfCheck(G_ROOM_ID, 1)) {
            R213StatusSetChain(1, no, objId, hitNo, flagNo);
        }
    } else {
        if (RsfCheck(G_ROOM_ID, 7) == 0) {
            R213StatusSetChain(2, no, objId, hitNo, flagNo);
        } else {
            R213StatusSetChain(3, no, objId, hitNo, flagNo);
        }
    }
}

// The switch lever: pulled (on 1) lies flat.
void R213StatusSetSwitch(int on)
{
    if (on == 1) {
        cObj* obj = SmdGetObjPtr(0x1E);

        if (obj) {
            Vec a;
            f32 ry = obj->ang.y;
            f32 rz = obj->ang.z;

            a.x = 1.5707964f;
            a.y = ry;
            a.z = rz;
            obj->setAng(&a);
        }
    }
}

// mode 0: up, 1: held by the chains left, 2: down.
void R213StatusSetBridge(int mode)
{
    if (mode == 2) {
        RsfSet(G_ROOM_ID, 4);
        if (r213_work.p->sat[1]) {
            r213_work.p->sat[1]->m_Flag &= ~4;
        }
        if (r213_work.p->sat[2]) {
            r213_work.p->sat[2]->m_Flag |= 4;
        }
    } else {
        if (r213_work.p->sat[1]) {
            r213_work.p->sat[1]->m_Flag |= 4;
        }
        if (r213_work.p->sat[2]) {
            r213_work.p->sat[2]->m_Flag &= ~4;
        }
    }
    if (mode == 0) {
        r213_work.p->ang = 0.34906587f;
    }
    if (mode == 1) {
        r213_work.p->ang = (f32) r213_work.p->breakNum * 0.0017453294f + 0.7853982f;
    }
    if (mode == 2) {
        r213_work.p->ang = 1.5707964f;
    }
    r213_work.p->angCur = r213_work.p->ang;
    R213BridgeAngSet(r213_work.p->ang);
}

// mode 0: create the chain object, 1: its hit box, 2: broken (falls), 3: broken and gone.
void R213StatusSetChain(int mode, int no, u32 objId, int hitNo, int flagNo)
{
    Vec fix[2] = {{-19550.0f, 5730.0f, -53900.0f}, {-19550.0f, 5730.0f, -52100.0f}};

    switch (mode) {
    case 0: {
        cObjChain* chain;

        r213_cloth.Num = r213_chainNum;
        r213_cloth.Bundle_num = 200;
        r213_cloth.pCloth = r213_chainParts;
        r213_cloth.pParent = r213_chainUp;
        r213_cloth.pChild = r213_chainDown;
        // Statement order = local-alloc order of the four pool constants (0.8 f0, 50 f13, 0.1 f12,
        // 0.0 f11): the non-dying x48 store sits behind three zero stores in sched1 (0.0's life
        // 63 > 1.5x the others'), x4C last puts its store after the dying x54 (0.1's life = 50's,
        // the earlier qty wins). sched2 issues the FP stores first anyway (4 dependents vs 2).
        r213_cloth.Gravity = 50.0f;
        r213_cloth.Rate = 0.8f;
        r213_cloth.pLeft = 0;
        r213_cloth.pRight = 0;
        r213_cloth.pUpLeft = 0;
        r213_cloth.WindSin = 0.0f;
        r213_cloth.Move_rate = 0.0f;
        r213_cloth.pUpRight = 0;
        r213_cloth.pMax = 0;
        r213_cloth.pWindSin = 0;
        r213_cloth.pWindRate = 0;
        r213_cloth.pAtset = 0;
        r213_cloth.pGravity = 0;
        r213_cloth.pRate = 0;
        r213_cloth.At_num = 0;
        r213_cloth.pEm_at = 0;
        r213_cloth.Flag = 0;
        r213_cloth.pPtbl = 0;
        r213_cloth.Stretchy = 0.1f;
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};

        chain = SetChain(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot);
        r213_work.p->chain[no] = chain;
        if (chain) {
            ((cModel*) chain)->setNoSuspend(1);
            chain->setChain(&r213_cloth);
            PenClothFixSet((cModel*) chain, &r213_cloth, 0x10, &fix[no]);
        }
        break;
    }
    case 1: {
        cObj* obj = SmdGetObjPtr(objId);

        if (obj) {
            r213_work.p->hit[hitNo] = SetEmHit(R213_EM_ARC(20), R213_EM_ARC(24), &obj->pos, 0, 1);
            if (r213_work.p->hit[hitNo]) {
                cEmHit* hit = r213_work.p->hit[hitNo];

                YarareInit(hit, -300.0f, 0.0f, 0.0f, 500.0f, 0.0f, 0, 1);
                hit->hp = 1;
            }
        }
        break;
    }
    case 2:
    case 3:
        RsfSet(G_ROOM_ID, flagNo);
        if (r213_work.p->hit[hitNo]) {
            r213_work.p->hit[hitNo]->hp = 0;
        }
        if (r213_work.p->chain[no]) {
            if (mode == 3) {
                ((cModel*) r213_work.p->chain[no])->be_flag &= ~2;
            } else {
                PenClothFixClear((cModel*) r213_work.p->chain[no], &r213_cloth, 0x10);
            }
        }
        SmdSetTrans(objId, 0);
        break;
    }
}

// Task: watches the chains while the bridge is up; a broken chain drops the bridge a little.
static void R213BridgeManager()
{
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        u8 broken[2];
        u8 unused[2];
        int i;

        for (;;) {
            int prev = r213_work.p->breakNum;

            for (i = 0; i < 2; i++) {
                broken[i] = 0;
            }
            if (RsfCheck(G_ROOM_ID, 3)) {
                broken[0] = 1;
            }
            if (RsfCheck(G_ROOM_ID, 4)) {
                broken[1] = 1;
            }
            R213ChainDamageCheck(0, 0x40, 1, 3);
            R213ChainDamageCheck(1, 0x41, 2, 4);
            R213ChainBreakNumCalc();
            if (r213_work.p->breakNum - prev > 0) {
                int which = 0;

                FSet(r213_work.p->angDown, (f32) (r213_work.p->breakNum - prev) * 0.0017453294f + r213_work.p->ang);
                if (RsfCheck(G_ROOM_ID, 3)) {
                    if (broken[0] == 0) {
                        which = 0;
                    }
                }
                if (RsfCheck(G_ROOM_ID, 4)) {
                    if (broken[1] == 0) {
                        which = 1;
                    }
                }
                SceExec(0x12, (TaskFunc) R213EventChainBreakMove, which, 0, SCE_PRIO_DEF_2, 0);
            }
            if (RsfCheck(G_ROOM_ID, 3) && RsfCheck(G_ROOM_ID, 4)) {
                break;
            }
            SceSleep(1);
        }
    }
}

// The bridge swings down to `target` (mode 1: with the chain effect), then shakes.
void R213BridgeAngMove(int mode, f32 target)
{
    int i;

    if (r213_work.p->angCur > target) {
        return;
    }
    r213_work.p->angCur = target;
    f32 t = 0.5f;
    f32 step = 0.05f;
    f32 amp = 2.0f;
    f32 dec = 0.2f;

    if (r213_work.p->ang < r213_work.p->angCur) {
        do {
            r213_work.p->ang += t * 3.1415927f / 180.0f;
            t += step;
            if (r213_work.p->ang >= r213_work.p->angCur) {
                r213_work.p->ang = r213_work.p->angCur;
            }
            R213BridgeAngSet(r213_work.p->ang);
            SceSleep(1);
        } while (r213_work.p->ang < r213_work.p->angCur);
    }
    if (mode == 1) {
        EstSet(0, -1, 0, 0, 1, 0xD, 1, 6, 0, 0);
    }
    for (i = 10; i != 0; i--) {
        amp -= dec;
        if (amp < 0.1f) {
            amp = 0.1f;
        }
        R213BridgeAngSet(r213_work.p->ang + fRand1_1() * amp * 3.1415927f / 180.0f);
        SceSleep(1);
    }
    R213BridgeAngSet(r213_work.p->ang);
}

// Sets the bridge (and its collision and chains) to angle `ang`.
void R213BridgeAngSet(f32 ang)
{
    cObj* obj;

    if (ang > 1.5707964f) {
        ang = 1.5707964f;
    }
    if (ang < 0.0f) {
        ang = 0.0f;
    }
    ang = -ang;
    obj = SmdGetObjPtr(0x3B);
    if (obj) {
        Vec a;
        f32 rx = obj->ang.x;
        f32 ry = obj->ang.y;

        a.x = rx;
        a.y = ry;
        a.z = ang;
        obj->setAng(&a);
        if (r213_work.p->eat[2]) {
            r213_work.p->eat[2]->setCoord(&r213_satPos, &obj->ang);
        }
    }
    R213ChainAngSetF3(0, 0x40, 3, ang, 0);
    R213ChainAngSetF0(ang, 1, 0x41, 4, 0);
}

// Hangs chain `no` from the bridge; the loose chain end object follows it (flag: always).
void R213ChainAngSet(int no, u32 objId, int hitNo, int flag, f32 ang)
{
    Vec ofs[2] = {{-180.0f, 5500.0f, -900.0f}, {-180.0f, 5500.0f, 900.0f}};
    Vec p;
    Mtx m;
    cObj* bridge = SmdGetObjPtr(0x3B);

    if (bridge) {
        cObjChain* chain = r213_work.p->chain[no];

        if (chain) {
            RotMatrix(m, &bridge->ang);
            PSMTXMultVec(m, &ofs[no], &p);
            PSVECAdd(&p, &bridge->pos, &p);
            ((cModel*) chain)->setPos(&p);
            {
                Vec a;
                f32 rz = bridge->ang.z * 0.22f - 1.5707964f;
                f32 rx = ((cModel*) chain)->ang.x;
                f32 ry = ((cModel*) chain)->ang.y;

                a.x = rx;
                a.y = ry;
                a.z = rz;
                SetAngV((cModel*) chain, &a);
            }
            if (RsfCheck(G_ROOM_ID, 8)) {
                cObj* obj = SmdGetObjPtr(objId);

                if (obj) {
                    cModel* parts = ((cModel*) chain)->getPartsPtr(0x11);

                    if (parts->world.x > obj->pos.x || flag != 0) {
                        Vec t;
                        Vec a2;
                        f32 ry;
                        f32 rx;
                        f32 ry2;

                        t.x = parts->world.x;
                        t.y = obj->pos.y;
                        t.z = obj->pos.z;
                        ry = GetXYAngle(&obj->pos, &((cModel*) chain)->pos);
                        SetPosV(obj, &t);
                        rx = obj->ang.x;
                        ry2 = obj->ang.y;
                        a2.x = rx;
                        a2.y = ry2;
                        a2.z = ry;
                        obj->setAng(&a2);
                    }
                }
            }
        }
    }
}

// Damage on chain `no`'s hit box; it breaks at 0 hp.
void R213ChainDamageCheck(int no, u32 objId, int hitNo, int flagNo)
{
    if (RsfCheck(G_ROOM_ID, flagNo) == 0) {
        cEmHit* hit = r213_work.p->hit[hitNo];

        if (hit) {
            if (hit->ckStatus() == 1) {
                int dm;

                EmDmBloodSet2(hit, 1, 9, 0, 0, 4);
                dm = 100;
                if (hit->dmg.m_Wep <= 0x2D) {
                    dm = GetWepDmVal(hit, hit->dmg.m_Wep, 0);
                }
                hit->hp -= dm;
                if (hit->hp <= 0) {
                    Vec pos;
                    Vec dir;

                    hit->hp = 0;
                    if (EmGetDmPos(hit, &pos, &dir)) {
                        SndCall(6, 0xA, &hit->pos, 0, 0, 0);
                        dir.x = 0.0f;
                        dir.y = 0.0f;
                        dir.z = 0.0f;
                        EstSet(0, -1, &pos, &dir, 1, 0xA, 1, 5, 0, 0);
                        R213StatusSetChain(2, no, objId, hitNo, flagNo);
                    }
                }
            }
        }
    }
}

// breakNum = the number of broken chains (Room_flg bits 3/4).
void R213ChainBreakNumCalc()
{
    IntSet(r213_work.p->breakNum, 0);
    if (RsfCheck(G_ROOM_ID, 3)) {
        r213_work.p->breakNum++;
    }
    if (RsfCheck(G_ROOM_ID, 4)) {
        r213_work.p->breakNum++;
    }
}

// Area 4: the switch lowers the bridge onto the chains.
static void R213EventSwitchMain()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        cObj* obj;

        SceEventStart(1);
        CamCtrl.CutCall(3);
        SceSleep(10);
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        if (SceMesGetSelection() != 1) {
            CamCtrl.Comeback(0);
            SceEventEnd(0);
            return;
        }
        RsfSet(G_ROOM_ID, 1);
        SceAtSetEnable(4, 0);
        SceSetEventCancel(1, (TaskFunc) R213EventSwitchEnd, 0, -1, 1);
        obj = SmdGetObjPtr(0x1E);
        if (obj) {
            SndCall(6, 0, &obj->pos, 0, 0, 0);

            // Literal constants: loop.c hoists the step pair and the limit's `lfs`, while the
            // limit's `lis` is gcse's PRE copy (its high is also computed after the loop), so the
            // preheader is `lis lim; lis step; lfs step; lfs lim` like the target.
            for (;;) {
                Vec a;
                f32 rx = obj->ang.x + 0.06981317f;
                f32 ry = obj->ang.y;
                f32 rz = obj->ang.z;

                a.x = rx;
                a.y = ry;
                a.z = rz;
                obj->setAng(&a);
                if (!(obj->ang.x >= 1.5707964f)) {
                    SceSleep(1);
                } else {
                    break;
                }
            }
            do { } while (0); // COMPILER-DIFF: #12 (the 1.5707964 store below is reloaded, not the loop's hoisted register)
            {
                Vec a;
                f32 ry = obj->ang.y;
                f32 rz = obj->ang.z;

                a.x = 1.5707964f;
                a.y = ry;
                a.z = rz;
                obj->setAng(&a);
            }
        }
        if (RsfCheck(G_ROOM_ID, 7) == 0) {
            cObj* o41;

            CamCtrl.CutCall(4);
            SceSleep(10);
            {
                cObj* o = SmdGetObjPtr(0x3B);

                if (o) {
                    SndCall(6, 7, &o->pos, 0, 0, 0);
                }
            }
            R213BridgeAngMove(1, 0.7853982f);
            SceSleep(20);
            CamCtrl.CutCall(7);
            SceSleep(10);
            RsfSet(G_ROOM_ID, 8);
            {
                cObj* o40 = SmdGetObjPtr(0x40);

                EstSet(0, -1, &o40->pos, &o40->ang, 1, 0xC, 1, 6, 0, 0);
            }
            o41 = SmdGetObjPtr(0x41);
            EstSet(0, -1, &o41->pos, &o41->ang, 1, 0xC, 1, 6, 0, 0);
            SndCall(6, 8, &o41->pos, 0, 0, 0);
            R213BridgeAngMove(0, r213_work.p->ang + 0.0017453294f);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
                // COMPILER-DIFF: candidate #17 -- dead test (o41 is not read again; jump2 deletes the
                // compare/load). Its in-loop pG read is a 4th, loop-weighted ref of the PRE'd pG high,
                // which breaks the equal-priority tie with the RoomData high in the target's favour (r28/r27).
                if (pG->Room_flg[0] & 0x80000000) {
                    o41 = 0;
                }
            }
            SceSleep(20);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R213EventSwitchEnd();
    }
}

// End of the switch event (also its cancel path): the lever posed pulled, Room_flg bit 8, the intact
// chains get their hit boxes, the bridge rests on the chains, effect dropped, camera back, SceEventEnd.
static void R213EventSwitchEnd()
{
    R213StatusSetSwitch(1);
    RsfSet(G_ROOM_ID, 8);
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        R213StatusSetChain(1, 0, 0x40, 1, 3);
    }
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        R213StatusSetChain(1, 1, 0x41, 2, 4);
    }
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        R213StatusSetBridge(1);
    }
    EffectEspDelete(1, 6, 0, 0);
    EffectEspgenDelete(1, 6, 0);
    EffectEfmDelete(1, 6, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Task: a chain broke; the bridge drops to angDown (cut 7 / 8 by chain).
static void R213EventChainBreakMove(int which)
{
    SceEventStart(0);
    SceSetEventCancel(1, (TaskFunc) R213EventChainBreakEnd, 0, -1, 1);
    if (which == 0) {
        CamCtrl.CutCall(7);
    } else {
        CamCtrl.CutCall(8);
    }
    R213BridgeAngMove(0, r213_work.p->angDown);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(10);
    SceSetEventCancel(0, 0, 0, -1, 1);
    R213EventChainBreakEnd();
}

// End of a chain-break drop: bridge state 1 (on the chains); both chains gone -> the bridge-down event.
static void R213EventChainBreakEnd()
{
    R213StatusSetBridge(1);
    if (RsfCheck(G_ROOM_ID, 3) && RsfCheck(G_ROOM_ID, 4)) {
        SceExec(0x12, (TaskFunc) R213EventBridgeDownMain, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    SceEventEnd(0);
}

// Task: both chains gone, the bridge falls (cut 6).
static void R213EventBridgeDownMain()
{
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        cObj* obj;

        RsfSet(G_ROOM_ID, 7);
        SceEventStart(0);
        SceSetEventCancel(1, (TaskFunc) R213EventBridgeDownEnd, 0, -1, 1);
        CamCtrl.CutCall(6);
        obj = SmdGetObjPtr(0x3B);
        if (obj) {
            SndCall(6, 9, &obj->pos, 0, 0, 0);
        }
        EstSet(0, -1, 0, 0, 1, 0xF, 1, 7, 0, 0);
        R213BridgeAngMove(0, 1.5707964f);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSleep(20);
        SceSetEventCancel(0, 0, 0, -1, 1);
        R213EventBridgeDownEnd();
    }
}

// End of the bridge fall: bridge state 2 (down), effect dropped, both chain objects removed, SceEventEnd.
static void R213EventBridgeDownEnd()
{
    R213StatusSetBridge(2);
    EffectEspDelete(1, 7, 0, 0);
    EffectEspgenDelete(1, 7, 0);
    EffectEfmDelete(1, 7, 0);
    R213StatusSetChain(3, 0, 0x40, 1, 3);
    R213StatusSetChain(3, 1, 0x41, 2, 4);
    SceEventEnd(0);
}

// Task: the battle stream while enemies have found the player.
static void SceBgmCheck()
{
    int on = 0;

    for (;;) {
        if (SceCkFindPL(0) == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else {
            if (on == 1) {
                SndRoomStrStop(3);
                on = 0;
            }
        }
        SceSleep(1);
    }
}

// Area 2: the s00 event.
static void R213Event()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(2, 0);
        ScfFlagOn(pG, SCF_R213_ASHLEY_LOST);
        SubCharCtrl(SCC_KILL, 0);
        StaFlagOff(pG, STA_SUB_ASHLEY);
        EvtMgr.EvtReadExec("event/evd/r213s00.evd", 0x2D, 0);
        R213EmSet();
    }
}

// Event r213s00 callback: sea area flag 0x800 during the event; cut 0 shows the boss model em2d00,
// drops the room effect and pauses the statue render target; later cuts set the near clip (200) and
// the models' flags; the end restores the render target.
extern "C" void Evt_R213S00_Func(Event* e)
{
    f32 clip = 200.0f;

    switch (e->funcMode) {
    case 0:
        break;
    case 1:
        SetSstAddAreaFlag(0x800);
        switch (e->NowCut) {
        case 0:
            SetSstAddAreaFlag(0);
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "em2d00", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x10;
                }
                EffectEspDelete(0x4001, 1, 0, 0);
                EffectEspgenDelete(0x4001, 1, 0);
                EffectEfmDelete(0x4001, 1, 0);
                if (r213_work.p->tex) {
                    EffectEspDelete(r213_work.p->tex->mask | 1, 2, 0, 0);
                    EffectEspgenDelete(r213_work.p->tex->mask | 1, 2, 0);
                    EffectEfmDelete(r213_work.p->tex->mask | 1, 2, 0);
                }
                SpfFlagOn(pG, SPF_ESP_AREA);
            }
            SetNearClipDist(clip);
            break;
        case 1: {
            int frame = e->NowFrame;

            if (frame == 0) {
                EffectEspDelete(0x4001, 1, 0, 0);
                EffectEspgenDelete(0x4001, 1, 0);
                EffectEfmDelete(0x4001, 1, 0);
                if (r213_work.p->tex) {
                    EffectEspDelete(r213_work.p->tex->mask | 1, 2, 0, 0);
                    EffectEspgenDelete(r213_work.p->tex->mask | 1, 2, 0);
                    EffectEfmDelete(r213_work.p->tex->mask | 1, 2, 0);
                }
                StaFlagOff(pG, STA_EVENT);
                SstSet(1, 0xFFFF, 1, 0, 0x2F, 0);
                if (r213_work.p->tex) {
                    EstSet(0, -1, 0, 0, 1, 0, r213_work.p->tex->mask | 1, 2, frame, (void*) frame);
                }
                StaFlagOn(pG, STA_EVENT);
                SpfFlagOff(pG, SPF_ESP_AREA);
            }
            break;
        }
        case 4:
            SetSstAddAreaFlag(0);
            break;
        case 6:
            SetNearClipDist(clip);
            break;
        }
        break;
    case 2: {
        int frame;

        EffectEspDelete(0x4001, 1, 0, 0);
        EffectEspgenDelete(0x4001, 1, 0);
        EffectEfmDelete(0x4001, 1, 0);
        if (r213_work.p->tex) {
            EffectEspDelete(r213_work.p->tex->mask | 1, 2, 0, 0);
            EffectEspgenDelete(r213_work.p->tex->mask | 1, 2, 0);
            EffectEfmDelete(r213_work.p->tex->mask | 1, 2, 0);
        }
        frame = 0;
        StaFlagOff(pG, STA_EVENT);
        SstSet(1, 0xFFFF, 1, 0, 0x2F, 0);
        if (r213_work.p->tex) {
            EstSet(0, -1, 0, 0, 1, 0, r213_work.p->tex->mask | 1, 2, frame, (void*) frame);
        }
        StaFlagOn(pG, STA_EVENT);
        SpfFlagOff(pG, SPF_ESP_AREA);
        SetSstAddAreaFlag(0x800);
        break;
    }
    }
}
