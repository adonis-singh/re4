#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "map_obj.h"
#include "light.h"
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
#include "emhit.h"
#include "em_set.h"
#include "em_wrap.h"
#include "read.h"
#include "player.h"
#include "pl_wep.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "pad.h"
#include "math_sub.h"
#include "motion.h"
#include "TexRender.h"
#include "rnd.h"
#include "debug.h"
#include "db_log.h"

// Room 1-0C (D:/Bio4/Prog/r10c.cpp): the dam and the water wheel; the two gates opened by the
// switch, the hanging crates (hit boxes on ropes) that fall into the water, the ambush after the
// switch and the key item on the gate.

// Rotation speed work of a cObjScr (game/obj02.cpp ObjScrRotWork) at cObj::work.
struct R10cRotWork {
    Vec rotSpd;   // 0x00
};

struct R10cWork {
    TexRenderMng* tex;   // 0x00  water render target
    cEm* em;             // 0x04  the enemy of the area-1 event
    cEm* ems[7];         // 0x08  the ambush after the switch
    cEmHit* hit[3][3];   // 0x24  the three crates' hit boxes
    u32 se[3];           // 0x48  attribute sounds (SeAtSndCall)
    u32 seSwitch;        // 0x54
    u32 seWheelB[2];     // 0x58
    u32 seWheelA[2];     // 0x60
    u32 seGate;          // 0x68
    u32 cnt;             // 0x6C  frames since the room started
    cSat* eat;           // 0x70  water attribute (swapped by eat_swap)
    cSat* eat2;          // 0x74
    cSat* crate[3];      // 0x78  crate collision (follows the crate objects)
};


u8 r10c_texTbl[0x20];
static R10cWork* r10c_work;

Vec r10c_wheelPosA = {88502.0f, -13247.0f, 26126.0f};
static Vec r10c_wheelPosA2 = {86965.0f, -13247.0f, 26126.0f};
static Vec r10c_wheelPosB = {78926.0f, -13247.0f, 52214.0f};
static Vec r10c_plOfs0 = {-158.44f, -2608.9001f, -524.0f};
static Vec r10c_plOfs1 = {-158.44f, -4911.3799f, 120.060005f};
static f32 r10c_switchAcc = 0.008f;
static f32 r10c_crateHalf = 0.5f;
static f32 r10c_crateAcc = 0.0005f;
static f32 r10c_crateGrav = -15.0f;
static f32 r10c_crateSpd = 5.0f;

static void r10c_TestPosMove(int side);
static void r10c_EmEvent_exit();
static void r10c_EmEvent();
static void r10c_StrCheck();
static void r10c_ThunderFlagOn();
static void r10c_ThunderFlagOff();
static void r10c_ThunderMove();
extern "C" void setTexRender();
extern "C" int SwitchExec(cObj* obj, f32* spd, int no, f32 lim, f32 cur);
static void chkSwitchA_exit();
static void chkSwitchA();
static void r10c_EmSet_exit();
static void r10c_EmSet();
static void moveWheel();
extern "C" void EmHitUpdate(cEm* em);
static void SetEmHitAtari();
static void hako_down(cObj* obj);
static void r10c_ItemGet();
extern "C" void eat_swap();

// Room init: pre-reads Ganado 0x12; JumpPoint 1 (arriving from below) sets Room_flg[0] 0x04000000 (the
// lower bank). Creates the pool water attribute, rain on the player, thunder and water-wheel tasks, the
// water render target; areas 3/4 = the ladder climb between banks; area 5 = the drain switch until
// Room_flg bit 5 (then the lever is posed pulled); area 1 = the axe-thrower event until bit 1; the crate
// hit boxes and battle-stream tasks; area 0x80 = the key item. Bit 8 / bit 5 set: the drained layout
// (gates open, drained water attribute, ambush on area 8 unless bit 9), else the full pool with its
// attribute sounds. Bits 11..13: the fallen crates' collision. Areas 0xC/0xD only in Japanese.
void R10cInit()
{
#line 113 "D:/Bio4/Prog/r10c.cpp"
    r10c_work = (R10cWork*) MEM_CALLOC(sizeof(R10cWork), 1, 0xd);
    EmReadSearch(0x12, 0, 0);
    if (pG->JumpPoint == 1) {
        pG->Room_flg[0] |= 0x04000000;
    }
    {
        Vec zero = {0.0f, 0.0f, 0.0f};

        r10c_work->eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 6);
    }
    EstSet(pPL, -1, 0, 0, EFF_PL00, 2, 0x800, ESP_CORE_KIND_NONE, 0, 0);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 3, 0x800, ESP_CORE_KIND_NONE, 0, 0);
    StaFlagOn(pG, STA_ROOM_RAIN);
    SceExec(0x12, (TaskFunc) r10c_ThunderMove, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) moveWheel, 0, 2, SCE_PRIO_DEF_2, 0);
    setTexRender();
    SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r10c_TestPosMove, 0, 1);
    SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r10c_TestPosMove, (void*) 1, 1);
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) chkSwitchA, 0, 1);
    } else {
        SmdGetObjPtr(0xC)->be_flag |= 0x20;
        SmdGetObjPtr(0xC)->pParts->ang.z = -1.6f;
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r10c_EmEvent, 0, 1);
    }
    RsfCheck(G_ROOM_ID, 2);
    RsfCheck(G_ROOM_ID, 3);
    RsfCheck(G_ROOM_ID, 4);
    SceExec(0x12, (TaskFunc) SetEmHitAtari, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r10c_StrCheck, 0, 2, SCE_PRIO_DEF_2, 0);
    SceAtDataSet_exec(0x80, SCE_LEVEL10, 0, (TaskFunc) r10c_ItemGet, 0, 1);
    if (RsfCheck(G_ROOM_ID, 8)) {
        cObj* obj;

        SceAtSetEnable(7, 0);
        obj = SmdGetObjPtr(0x71);
        obj->be_flag |= 0x20;
        obj->pos.y = 2800.0f;
    }
    if (RsfCheck(G_ROOM_ID, 5)) {
        SetSstDispFlag(9, 1);
        pG->Room_flg[0] &= ~0x04000000;
        RsfSet(G_ROOM_ID, 7);
        SetSstDispFlag(0, 0);
        EffectEspDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0);
        EffectEfmDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0);
        SceAtSetEnable(0x11, 0);
        SceAtSetEnable(0x12, 0);
        SmdGetObjPtr(0)->be_flag &= ~2;
        SmdGetObjPtr(0xE)->be_flag |= 2;
        SmdSetTrans(0x55, 0);
        SmdSetTrans(0x5A, 0);
        SmdSetTrans(0x58, 0);
        SmdSetTrans(0x59, 0);
        SmdSetTrans(0x56, 0);
        SmdSetTrans(0x57, 0);
        SmdSetTrans(0x48, 0);
        SceAtSetEnable(0xE, 0);
        SceAtSetEnable(0xF, 1);
        SceAtSetEnable(0xC, 0);
        SceAtSetEnable(0xD, 0);
        eat_swap();
        if (RsfCheck(G_ROOM_ID, 9) == 0) {
            SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r10c_EmSet, 0, 1);
        }
    } else {
        SetSstDispFlag(9, 0);
        SmdSetTrans(0x58, 0);
        SmdSetTrans(0x59, 0);
        r10c_work->se[0] = SeAtSndCall(0);
        r10c_work->se[1] = SeAtSndCall(1);
        r10c_work->se[2] = SeAtSndCall(2);
        SceAtSetEnable(0xE, 1);
        SceAtSetEnable(0xF, 0);
    }
    if (pPL->pos.y < -5000.0f) {
        pG->Room_flg[0] |= 0x04000000;
    }
    {
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};

        if (RsfCheck(G_ROOM_ID, 11)) {
            SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
        }
        if (RsfCheck(G_ROOM_ID, 12)) {
            SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 3);
        }
        if (RsfCheck(G_ROOM_ID, 13)) {
            SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 4);
        }
        r10c_work->crate[0] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
        r10c_work->crate[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
        r10c_work->crate[2] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
    }
    if (pSys->language != 0) {
        SceAtSetEnable(0xC, 0);
        SceAtSetEnable(0xD, 0);
    }
}

// Per frame: counts frames, mirrors Item_flg[0] 0x00400000 (the key taken) into Scenario_flg[0] 0x100,
// and keeps the three crate collision pieces on their swinging scroll objects 0x61..0x63.
void R10cMain()
{
    r10c_work->cnt = r10c_work->cnt + 1;
    if (ItfFlagChk(pG, ITF_R11C_ITEM)) {
        ScfFlagOn(pG, SCF_R10C_GET_CREST);
    }
    r10c_work->crate[0]->setCoord(&SmdGetObjPtr(0x61)->pos, &SmdGetObjPtr(0x61)->ang);
    r10c_work->crate[1]->setCoord(&SmdGetObjPtr(0x62)->pos, &SmdGetObjPtr(0x62)->ang);
    r10c_work->crate[2]->setCoord(&SmdGetObjPtr(0x63)->pos, &SmdGetObjPtr(0x63)->ang);
}

// Areas 3 / 4: Leon climbs down (side 0) or up (side 1) the ladder to the other bank.
static void r10c_TestPosMove(int side)
{
    Vec pos = {66428.0f, -827.0f, 28530.0f};
    Vec ang = {0.0f, -1.5707964f, 0.0f};
    Vec out;
    cPlayer* pl = pPL;
    // `pG`: the struct-view pG load is not a fixed scalar, so sched1 keeps it behind the two
    // template copies and their word 4/8 loads and stores come out in source order (the plain
    // `pG` load is hoisted above them and the 8/4 pair flips).
    u32 flags = pG->Stop_flg;
    cObj* obj;

    KeyStop(0xEFCF0000ULL);
    pG->Stop_flg = 0xFFFFFFFF;
    SpfFlagOff(pG, SPF_SCE);
    FadeSetW(2, 10, 0, 0);
    SceSleep(10);
    SmdSetTrans(0x5C, 0);
    pG->Stop_flg = flags;
    FadeSetW(0x80000002, 10, 0, 0);
    SceEventStart(0);
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    PlSetHand(1, 0);
    if (side == 0) {
        SndStrReq(1, 0x25, 0x80000003, 0, 0, 0.0f);
        {
            Mtx m;
            cPlayer* p;

            low_RotMatrix(m, &ang);
            TransMatrix(m, &pos);
            PSMTXMultVec(m, &r10c_plOfs0, &out);
            p = pPL;
            p->setPos(&out);
            p->setAng(&ang);
            obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &ang, 0x10, 1);
            if (obj == 0) {
                pLog->err(0, 0, "R10cTestPosMove : set failed");
                return;
            }
            pPL->setNoSuspend(1);
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 10, 0, 0x201, 0);
            obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 10, 0, 1, 0);
            SceSleep((u32) MotionGetMaxFrame(&pPL->Motion) - 30);
            FadeSetW(2, 30, 0, 0);
            SceSleep(30);
            pPL->setNoSuspend(0);
            ObjMgr.destroy(obj);
        }
        {
            Vec pos2 = {68595.0f, -15000.0f, 26806.0f};
            Vec ang2;
            f32 ry = 1.82f;
            cPlayer* p;
            Vec* pa = &ang2;

            p = pPL;
            p->setPos(&pos2);
            ang2.x = 0.0f;
            pa->y = ry;
            ang2.z = 0.0f;
            p->setAng(&ang2);
        }
        if (RsfCheck(G_ROOM_ID, 5)) {
            pG->Room_flg[0] &= ~0x04000000;
        } else {
            pG->Room_flg[0] |= 0x04000000;
        }
    } else {
        SndStrReq(1, 0x26, 0x80000003, 0, 0, 0.0f);
        {
            Mtx m;
            cPlayer* p;

            low_RotMatrix(m, &ang);
            TransMatrix(m, &pos);
            PSMTXMultVec(m, &r10c_plOfs1, &out);
            p = pPL;
            p->setPos(&out);
            p->setAng(&ang);
            obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &ang, 0x10, 1);
            if (obj == 0) {
                pLog->err(0, 0, "R10cTestPosMove : set failed");
                return;
            }
            pPL->setNoSuspend(1);
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 10, 0, 1, 0);
            obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 10, 0, 1, 0);
            SceSleep((u32) MotionGetMaxFrame(&pPL->Motion) - 30);
            FadeSetW(2, 30, 0, 0);
            SceSleep(30);
            pPL->setNoSuspend(0);
            ObjMgr.destroy(obj);
        }
        {
            Vec pos2 = {65038.0f, -768.0f, 28296.0f};
            Vec ang2;
            f32 ry = -1.56f;
            cPlayer* p;
            Vec* pa = &ang2;

            p = pPL;
            p->setPos(&pos2);
            ang2.x = 0.0f;
            pa->y = ry;
            ang2.z = 0.0f;
            p->setAng(&ang2);
        }
        pG->Room_flg[0] &= ~0x04000000;
    }
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->Wep->setTrans(1, 0);
    SceEventEnd(0);
    CamCtrl.m_QuasiFPS.setPlayerLocation(pPL->mat, pPL->pFloor_norm);
    FadeSetW(0x80000002, 30, 0, 0);
    SmdSetTrans(0x5C, 1);
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        RsfSet(G_ROOM_ID, 10);
        OpeOwTypeSet(2);
    }
}


// End of the axe event: destroys the event Ganado, drops its effects, camera back, SceEventEnd, clears
// Status_flg[2] 0x02000000, spawns ESL 3 and 4 (the real enemies of the bank), puts Leon at the ladder
// foot and clears Room_flg[0] 0x08000000.
static void r10c_EmEvent_exit()
{
    EmMgr.destroy(r10c_work->em);
    pPL->setNoSuspend(0);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    {
        EmListData* l = &pG->Em_list[3];

        l->be_flag |= 1;
        l->set = 0;
    }
    EmSetFromList2(3, 1);
    pG->Em_list[4].be_flag |= 1;
    EmSetFromList2(4, 1);
    pPL->setPos(6609.0f, 0.0f, 17172.0f);
    pPL->setAng(0.0f, 0.56f, 0.0f);
    pG->Room_flg[0] &= ~0x08000000;
}

// Area 1: the Ganado on the far bank throws its axe (camera cuts 0x11..0x13).
static void r10c_EmEvent()
{
    int cnt = 0;

    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        cEm* em;

        RsfSet(G_ROOM_ID, 1);
        pG->Room_flg[0] &= ~0x04000000;
        {
            EmListData* l = &pG->Em_list[2];

            em = EmSetFromList2(2, 1);
            l->set = 0;
        }
        em->setNoSuspend(1);
        r10c_work->em = em;
        EstSet(em, -1, 0, 0, EFF_ROOM, 0x1F, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        em->flag |= 1;
        MotionSetCore(em, &em->Motion, ROOM_ARC_PTR(pG->pRoom, 0x26), 0, 0, 1, 0);
        SndStrReq(1, 0x23, 0x80000003, 0, 0, 0.0f);
        SceEventStart(0);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceSetEventCancel(1, (TaskFunc) r10c_EmEvent_exit, 0, -1, 1);
        CamCtrl.CutCall(0x11);
        while (CamCtrl.IsMotionEnd() == 0) {
            cnt++;
            SceSleep(1);
        }
        CamCtrl.CutCall(0x12);
        while (CamCtrl.IsMotionEnd() == 0) {
            if (cnt++ == 219) {
                pG->Room_flg[0] |= 0x10000000;
                pG->Room_flg[0] |= 0x08000000;
            }
            SceSleep(1);
        }
        pPL->setNoSuspend(1);
        pPL->setPos(3145.0f, 0.0f, 10394.0f);
        pPL->setAng(0.0f, 0.4f, 0.0f);
        CamCtrl.CutCall(0x13);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r10c_EmEvent_exit();
    }
}

// Battle stream while an enemy sees the player.
static void r10c_StrCheck()
{
    int on = 0;

    for (;;) {
        if ((SceCkFindPL(0) == 1 || (pG->Room_flg[0] & 0x08000000)) && !(pG->Room_flg[0] & 0x04000000)) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else if (on == 1) {
            SndRoomStrStop(3);
            on = 0;
        }
        SceSleep(1);
    }
}

// Lightning on: nothing in this room (outdoors, no lit window object).
static void r10c_ThunderFlagOn()
{
}

// Lightning off: nothing.
static void r10c_ThunderFlagOff()
{
}

// Thunder every 90..235 frames.
static void r10c_ThunderMove()
{
    int cnt;

    SceSleep(1);
    EffSetToolStateCallBack(0, r10c_ThunderFlagOn, r10c_ThunderFlagOff);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    for (;;) {
        if (cnt == 0) {
            if (!StaFlagChk(pG, STA_CAMERA_IN_ROOM)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_NONE, 0, 0);
                {
                    u8 r = Rnd() % 30;
                    cnt = r * 5 + 90;
                }
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}

// TexRender blend setup of one water object (the byte stores are scheduled differently per block).
#define R10C_TEX_OBJ(id, col, v138, v136, v137) \
    obj = SmdGetObjPtr(id);                     \
    obj->pModelInfo->setTexBlendTbl(tbl);            \
    obj->pModelInfo->setBlendRatio(0xFF);            \
    obj->pModelInfo->color[3] = col;                 \
    obj->Shader_type = v136;                           \
    obj->Refract_pow = v137;                           \
    obj->Refract_ratio = v138;

// The water surface: a render target blended into the water objects.
extern "C" void setTexRender()
{
    cObj* obj;
    u8* tbl = r10c_texTbl;

    if (GetTexRenderMgr(&r10c_work->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r10c_work->tex->m_Tex_no;
        r10c_work->tex->m_Rep_type = 1;
        r10c_work->tex->m_H_size = r10c_work->tex->m_W_size = 0x40;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, r10c_work->tex->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "R10cInit() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0xE);
    obj->be_flag &= ~2;
    R10C_TEX_OBJ(5, 0xF0, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(7, 0xF0, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0, 0xF0, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0x48, 0xC0, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0x55, 0xFF, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0x56, 0xFF, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0x57, 0xFF, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0x58, 0xFF, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0x59, 0xFF, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0x5A, 0xFF, 0xA0, 2, 0x12);
    R10C_TEX_OBJ(0xE, 0xF0, 0, 2, 2);
}

// Turns the switch lever's parts towards `lim` with the accelerating speed `*spd`; 1 when it arrived.
extern "C" int SwitchExec(cObj* obj, f32* spd, int no, f32 lim, f32 cur)
{
    int dir;

    if (lim > cur) {
        obj->pParts->ang.z += *spd;
        dir = 1;
    } else {
        obj->pParts->ang.z -= *spd;
        dir = 0;
    }
    if (*spd >= 0.0f) {
        if (dir ? (obj->pParts->ang.z < lim) : (obj->pParts->ang.z > lim)) {
            *spd += r10c_switchAcc * 1.85f;
        } else {
            *spd = -r10c_switchAcc;
            obj->pParts->ang.z = lim;
            return 1;
        }
    }
    return 0;
}

// End of the drain-switch event: stops the machinery sounds, disables the pool areas 0x11/0x12, arms the
// ambush on area 8, camera back, SceEventEnd, drops the water effects, sets Room_flg bit 7 (drained),
// swaps the water attribute (eat_swap) and re-arranges the gate/water scroll objects and areas for the
// drained layout.
static void chkSwitchA_exit()
{
    SndStop(r10c_work->seWheelB[0], 0);
    SndStop(r10c_work->seWheelB[1], 0);
    SndStop(r10c_work->se[0], 0);
    SndStop(r10c_work->se[1], 0);
    SndStop(r10c_work->seGate, 0);
    SndStop(r10c_work->se[2], 0);
    SndStop(r10c_work->seSwitch, 0);
    SndStop(r10c_work->seWheelA[0], 0);
    SndStop(r10c_work->seWheelA[1], 0);
    SceAtSetEnable(0x11, 0);
    SceAtSetEnable(0x12, 0);
    r10c_work->cnt = 0;
    SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r10c_EmSet, 0, 1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_EFFAREA_USE_CAM);
    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM04, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM04, 0);
    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM02, 0, 0);
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM02, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM02, 0);
    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM03, 0, 0);
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM03, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM03, 0);
    SetSstDispFlag(9, 1);
    RsfSet(G_ROOM_ID, 7);
    SetSstDispFlag(0, 0);
    EffectEspDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0);
    EffectEfmDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0);
    SceAtSetEnable(0x11, 0);
    SceAtSetEnable(0x12, 0);
    SndStop(r10c_work->se[0], 0);
    SndStop(r10c_work->se[1], 0);
    SndStop(r10c_work->se[2], 0);
    SmdGetObjPtr(0)->be_flag &= ~2;
    SmdGetObjPtr(0xE)->be_flag |= 2;
    SmdSetTrans(0x55, 0);
    SmdSetTrans(0x5A, 0);
    SmdSetTrans(0x58, 0);
    SmdSetTrans(0x59, 0);
    SmdSetTrans(0x56, 0);
    SmdSetTrans(0x57, 0);
    SmdSetTrans(0x48, 0);
    SceAtSetEnable(0xE, 0);
    SceAtSetEnable(0xF, 1);
    SceAtSetEnable(0xC, 0);
    SceAtSetEnable(0xD, 0);
    eat_swap();
    GameSave.save(pSaveData, -1);
}

// Area 5: the switch that opens the gates and drains the pool (camera cuts 0x14..0x19).
static void chkSwitchA()
{
    SceEventStart(0);
    CamCtrl.CutCall(0x14);
    SceMesSet(0, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    if (SceMesGetSelection() == 1) {
        f32 spd;

        SceAtSetEnable(5, 0);
        StaFlagOn(pG, STA_EFFAREA_USE_CAM);
        SndCall(6, 3, 0, 0, 0, 0);
        pG->Room_flg[0] |= 0x04000000;
        CamCtrl.CutCall(0x14);
        spd = 0.0f;
        SmdGetObjPtr(0xC)->be_flag |= 0x20;
        while (SwitchExec(SmdGetObjPtr(0xC), &spd, 2, -1.6f, 0.0f) == 0) {
            SceSleep(1);
        }
        SceSleep(15);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(1, (TaskFunc) chkSwitchA_exit, 0, -1, 1);
        RsfSet(G_ROOM_ID, 5);
        EstSet(0, -1, 0, 0, EFF_ROOM, 4, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        r10c_work->seSwitch = SndCall(6, 0xF, &SmdGetGroupObjPtr(4)->pos, 0, 0, 0);
        CamCtrl.CutCall(0x15);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        RsfSet(G_ROOM_ID, 6);
        CamCtrl.CutCall(0x16);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(0x17);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(0x18);
        EstSet(0, -1, 0, 0, EFF_ROOM, 9, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
        SceSleep(80);
        RsfSet(G_ROOM_ID, 7);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SmdSetTrans(0x56, 0);
        SmdSetTrans(0x57, 0);
        // COMPILER-DIFF: candidate #17 -- the last EstSet's two stack-argument zeros come from one
        // pseudo set here (`li r29,0` before SmdSetTrans(0x48), callee-saved across the calls) instead of
        // a reload-materialised `li r0,0` at the stores; its live range also orders the two CamCtrl
        // highs (r29/r31) like the target.
        void* z = 0;
        SmdSetTrans(0x48, 0);
        SceAtSetEnable(0xE, 0);
        SceAtSetEnable(0xF, 1);
        SceAtSetEnable(0xC, 0);
        SceAtSetEnable(0xD, 0);
        eat_swap();
        SndStop(r10c_work->se[0], 0);
        SndStop(r10c_work->se[1], 0);
        SndStop(r10c_work->se[2], 0);
        SmdGetObjPtr(0)->be_flag &= ~2;
        SmdGetObjPtr(0xE)->be_flag |= 2;
        SetSstDispFlag(0, 0);
        EffectEspDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0);
        EffectEfmDelete(0, ESP_CORE_KIND_ROOM_AREA01, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 8, 0x2001, ESP_CORE_KIND_ROOM04, z, z); // COMPILER-DIFF: candidate #17 (both 0, see `z`)
        CamCtrl.CutCall(0x19);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SetSstDispFlag(9, 1);
        SceSetEventCancel(0, 0, 0, -1, 1);
        SceSetEventCancel(0, 0, 0, -1, 1);
        chkSwitchA_exit();
    } else {
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        StaFlagOff(pG, STA_EFFAREA_USE_CAM);
    }
}

// End of the ambush cutscene: camera back, SceEventEnd, the seven Ganados may suspend again.
static void r10c_EmSet_exit()
{
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r10c_work->ems[0]->setNoSuspend(0);
    r10c_work->ems[1]->setNoSuspend(0);
    r10c_work->ems[2]->setNoSuspend(0);
    r10c_work->ems[3]->setNoSuspend(0);
    r10c_work->ems[4]->setNoSuspend(0);
    r10c_work->ems[5]->setNoSuspend(0);
    r10c_work->ems[6]->setNoSuspend(0);
}

// Area 8: the ambush on the drained pool floor (camera cut 0x1B).

// Area 8 after the drain (once, Room_flg bit 9): 30 frames later stream 7 and camera cut 0x1B show the
// seven Ganados (ESL 6..0xC) arriving; player-cancellable.
static void r10c_EmSet()
{
    // The 0.0 is loaded after the flags_174 store: a pool constant would move above it (pool loads
    // never depend on stores), a `static const` read through a reference stays below (r104 idiom).
    static const f32 vol = 0.0f;

    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        RsfSet(G_ROOM_ID, 9);
        SceSleep(30);
        pG->Room_flg[0] &= ~0x04000000;
        SndStrReq(1, 7, 3, 0, 0, *(const f32*) &vol);
        SceEventStart(1);
        CamCtrl.CutCall(0x1B);
        r10c_work->ems[0] = setEm(6, -1, 1, 1, 1);
        r10c_work->ems[1] = setEm(7, -1, 1, 1, 1);
        r10c_work->ems[2] = setEm(8, -1, 1, 1, 1);
        r10c_work->ems[3] = setEm(9, -1, 1, 1, 1);
        r10c_work->ems[4] = setEm(0xA, -1, 1, 1, 1);
        r10c_work->ems[5] = setEm(0xB, -1, 1, 1, 1);
        r10c_work->ems[6] = setEm(0xC, -1, 1, 1, 1);
        r10c_work->ems[0]->setNoSuspend(1);
        r10c_work->ems[1]->setNoSuspend(1);
        r10c_work->ems[2]->setNoSuspend(1);
        r10c_work->ems[3]->setNoSuspend(1);
        r10c_work->ems[4]->setNoSuspend(1);
        r10c_work->ems[5]->setNoSuspend(1);
        r10c_work->ems[6]->setNoSuspend(1);
        SceSetEventCancel(1, (TaskFunc) r10c_EmSet_exit, 0, -1, 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r10c_EmSet_exit();
    }
}

// The water wheel, the sluice gates and the pool level.
static void moveWheel()
{
    f32 spdMax = 0.0163f;
    f32 spdAcc = 0.00023812501f;
    f32 spdB = 0.0f;
    f32 spdA = 0.0f;
    f32 gateSpd = 0.0f;
    f32 poolSpd = 30.0f;
    R10cRotWork* wheelA = (R10cRotWork*) ((cObjUnion*) SmdGetObjPtr(6))->work;
    R10cRotWork* wheelA2 = (R10cRotWork*) ((cObjUnion*) SmdGetObjPtr(0xA))->work;
    R10cRotWork* wheelB;
    R10cRotWork* wheelA3 = (R10cRotWork*) ((cObjUnion*) SmdGetObjPtr(0x33))->work;
    wheelB = (R10cRotWork*) ((cObjUnion*) SmdGetObjPtr(8))->work;
    cObj* cogA;
    cObj* cogB;
    cObj* gate;
    cObj* gate2;
    cObj* pool;
    cObj* pool2;

    cogA = SmdGetGroupObjPtr(0x35);
    cogB = SmdGetGroupNext(cogA);
    cogA->be_flag |= 0x20;
    cogB->be_flag |= 0x20;
    gate = SmdGetGroupObjPtr(4);
    gate2 = SmdGetGroupObjPtr(0x5A);
    gate->be_flag |= 0x20;
    gate2->be_flag |= 0x20;
    pool = SmdGetGroupObjPtr(0x6D);
    pool2 = SmdGetGroupObjPtr(0x72);
    pool->be_flag |= 0x20;
    pool2->be_flag |= 0x20;
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 5)) {
            if (r10c_work->cnt < 100) {
                gate->ang.y = -1.2f;
            }
            if (gate->ang.y > -1.2f) {
                SmdSetTrans(0x58, 1);
                SmdSetTrans(0x59, 1);
                SmdSetTrans(0x56, 0);
                SmdSetTrans(0x57, 0);
                SmdSetTrans(0x48, 0);
                SceAtSetEnable(0xE, 0);
                SceAtSetEnable(0xF, 1);
                SceAtSetEnable(0xC, 0);
                SceAtSetEnable(0xD, 0);
                eat_swap();
                gateSpd += 0.001f;
                gate->ang.y -= gateSpd;
                if (gate->ang.y <= -1.2f) {
                    gate->ang.y = -1.2f;
                }
                gate2->ang.y = gate->ang.y;
            }
        }
        if (RsfCheck(G_ROOM_ID, 7)) {
            if (r10c_work->cnt > 100 && poolSpd == 30.0f) {
                r10c_work->seGate = SndCall(6, 0xA, &pool->pos, 0, 0, 0);
                SndStop(r10c_work->se[1], 0);
            }
            poolSpd -= 20.0f;
            if (pool->pos.y > 300.0f) {
                pool->pos.y += poolSpd;
                pool2->pos.y += poolSpd;
            }
            if (pool->pos.y <= -500.0f) {
                pool->pos.y = -500.0f;
                pool2->pos.y = -500.0f;
            }
            if (pool->pos.y < 1300.0f) {
                cObj* water = SmdGetObjPtr(0);

                water->be_flag |= 0x20;
                water->pos.y += (pool->pos.y - 1300.0f + -647.0f - water->pos.y) * 0.12f;
            }
        }
        if (RsfCheck(G_ROOM_ID, 6) == 0 && RsfCheck(G_ROOM_ID, 7) == 0) {
            if (spdA == 0.0f) {
                EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM02, 0, 0);
                EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM02, 0);
                EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM02, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, 5, 0x2001, ESP_CORE_KIND_ROOM02, 0, 0);
                r10c_work->seWheelA[0] = SndCall(6, 0x56, &r10c_wheelPosA, 0, 0, 0);
                r10c_work->seWheelA[1] = SndCall(6, 0x59, &r10c_wheelPosA2, 0, 0, 0);
            }
            spdA += spdAcc;
            if (spdA > spdMax) {
                spdA = spdMax;
            }
        } else {
            spdA *= 0.95f;
            if (spdA != 0.0f && spdA < 0.0001f) {
                spdA = 0.0f;
                EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM02, 0, 0);
                EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM02, 0);
                EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM02, 0);
                SndCall(6, 0x5C, &r10c_wheelPosA, 0, 0, 0);
                SndCall(6, 0x61, &r10c_wheelPosA2, 0, 0, 0);
            }
        }
        wheelA->rotSpd.z = spdA;
        wheelA2->rotSpd.z = -spdA;
        wheelA3->rotSpd.y = -spdA;
        if (RsfCheck(G_ROOM_ID, 6)) {
            if (RsfCheck(G_ROOM_ID, 7)) {
                if (spdB != 0.0f) {
                    spdB = 0.0f;
                    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM03, 0, 0);
                    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM03, 0);
                    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM03, 0);
                    SndStop(r10c_work->seWheelB[0], 0);
                    SndStop(r10c_work->seWheelB[1], 0);
                }
            } else {
                if (spdB == 0.0f) {
                    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM03, 0, 0);
                    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM03, 0);
                    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM03, 0);
                    EstSet(0, -1, 0, 0, EFF_ROOM, 6, 0x2001, ESP_CORE_KIND_ROOM03, 0, 0);
                    r10c_work->seWheelB[0] = SndCall(6, 0x56, &r10c_wheelPosB, 0, 0, 0);
                    r10c_work->seWheelB[1] = SndCall(6, 0x59, &r10c_wheelPosB, 0, 0, 0);
                }
                spdB += spdAcc;
                if (spdB > spdMax) {
                    spdB = spdMax;
                }
            }
        } else {
            spdB *= 0.95f;
            if (spdB != 0.0f && spdB < 0.0001f) {
                spdB = 0.0f;
                EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM03, 0, 0);
                EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM03, 0);
                EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM03, 0);
                SndStop(r10c_work->seWheelB[0], 0);
                SndStop(r10c_work->seWheelB[1], 0);
            }
        }
        wheelB->rotSpd.z = spdB;
        cogA->ang.y += spdB;
        cogB->ang.y -= spdB;
        SceSleep(1);
    }
}

// Recomputes a hit box's matrices after its position was set by hand.
extern "C" void EmHitUpdate(cEm* em)
{
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// The three hanging crates: their hit boxes swing with them and drop them when shot.
static void SetEmHitAtari()
{
    Vec ofsA;
    Vec ofsB;
    Vec ofsC;
    Mtx m;
    Vec tmp;
    f32 angC;
    f32 angB;
    f32 angA;
    f32 spdB;
    f32 spdC;
    f32 spdA;

    // COMPILER-DIFF: 3 -- the original's `high(0.0)` for the `angX = 0.0f` loads below is a
    // callee-saved pseudo born at the function top (`lis r27` after the first call, dying at the
    // `lfs f30`); ours computes it in place. A dead pool load into a hard register creates that
    // high here: cse1 forwards the high (a constant) to the later load, the hard-register value
    // itself is invalidated by the calls (no `fmr` forwarding), and the dead set is deleted.
    {
        f32 z;
        z = 0.0f;
    }
    PSVECSubtract(&SmdGetObjPtr(0x61)->pos, &SmdGetObjPtr(0x5E)->pos, &ofsA);
    PSVECSubtract(&SmdGetObjPtr(0x62)->pos, &SmdGetObjPtr(0x5F)->pos, &ofsB);
    PSVECSubtract(&SmdGetObjPtr(0x63)->pos, &SmdGetObjPtr(0x60)->pos, &ofsC);
    SmdGetObjPtr(0x5E)->be_flag |= 0x20;
    SmdGetObjPtr(0x5F)->be_flag |= 0x20;
    SmdGetObjPtr(0x60)->be_flag |= 0x20;
    SmdGetObjPtr(0x61)->be_flag |= 0x20;
    SmdGetObjPtr(0x62)->be_flag |= 0x20;
    SmdGetObjPtr(0x63)->be_flag |= 0x20;
    SmdGetObjPtr(0x6A)->be_flag |= 0x20;
    SmdGetObjPtr(0x6B)->be_flag |= 0x20;
    // Reference store: the RsfCheck `lwz pG`/`lhz room_id` must stay below this store (the
    // scalar-reference rule), which puts them behind the 0.0 load like the original.
    (SmdGetObjPtr(0x6C)->be_flag |= 0x20);
    angA = 0.0f;
    angB = 0.0f;
    angC = 0.0f;
    // Pool order: the then-arm's YarareInitCube constants precede 0.01/0.05/0.06 in the original
    // pool although the spd loads sit in this block; the folded const declarations create the
    // entries here without code.
    {
        const f32 k750 = 750.0f;
        const f32 k1500 = 1500.0f;
        const f32 k100 = 100.0f;
        const f32 k1800 = 1800.0f;
        const f32 k450 = 450.0f;
        const f32 k500 = 500.0f;
    }
    spdA = 0.01f;
    spdB = 0.05f;
    spdC = 0.06f;
    if (RsfCheck(G_ROOM_ID, 14) == 0) {
        r10c_work->hit[0][0] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x61)->pos, &SmdGetObjPtr(0x61)->ang, 0);
        YarareInitCube(r10c_work->hit[0][0], 0.0f, 0.0f, 0.0f, 750.0f, 1500.0f, 750.0f, 0, YAT_FLAG_ON);
        r10c_work->hit[0][1] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x61)->pos, &SmdGetObjPtr(0x61)->ang, 0);
        YarareInitCube(r10c_work->hit[0][1], 0.0f, 1500.0f, 0.0f, 100.0f, 1800.0f, 100.0f, 0, YAT_FLAG_ON);
        r10c_work->hit[0][2] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x61)->pos, &SmdGetObjPtr(0x61)->ang, 0);
        YarareInitCube(r10c_work->hit[0][2], 0.0f, 450.0f, 0.0f, 500.0f, 1500.0f, 500.0f, 0, YAT_FLAG_ON);
    } else {
        cObj* obj;

        SmdSetTrans(0x6A, 0);
        SceExec(0x12, (TaskFunc) hako_down, (int) SmdGetObjPtr(0x61), 0, SCE_PRIO_DEF_2, 0);
        obj = SmdGetObjPtr(0x61);
        obj->pos.x = 93412.0f;
        obj->pos.y = -16601.0f;
        obj->pos.z = 32568.0f;
    }
    if (RsfCheck(G_ROOM_ID, 15) == 0) {
        r10c_work->hit[1][0] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x62)->pos, &SmdGetObjPtr(0x62)->ang, 0);
        YarareInitCube(r10c_work->hit[1][0], 0.0f, 0.0f, 0.0f, 750.0f, 1500.0f, 750.0f, 0, YAT_FLAG_ON);
        r10c_work->hit[1][1] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x62)->pos, &SmdGetObjPtr(0x61)->ang, 0);
        YarareInitCube(r10c_work->hit[1][1], 0.0f, 1500.0f, 0.0f, 100.0f, 1800.0f, 100.0f, 0, YAT_FLAG_ON);
        r10c_work->hit[1][2] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x62)->pos, &SmdGetObjPtr(0x61)->ang, 0);
        YarareInitCube(r10c_work->hit[1][2], 0.0f, 450.0f, 0.0f, 500.0f, 1500.0f, 500.0f, 0, YAT_FLAG_ON);
    } else {
        cObj* obj;

        SmdSetTrans(0x6B, 0);
        SceExec(0x12, (TaskFunc) hako_down, (int) SmdGetObjPtr(0x62), 0, SCE_PRIO_DEF_2, 0);
        obj = SmdGetObjPtr(0x62);
        obj->pos.x = 93412.0f;
        obj->pos.y = -16601.0f;
        obj->pos.z = 42216.0f;
    }
    if (RsfCheck(G_ROOM_ID, 16) == 0) {
        r10c_work->hit[2][0] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x63)->pos, &SmdGetObjPtr(0x63)->ang, 0);
        YarareInitCube(r10c_work->hit[2][0], 0.0f, 0.0f, 0.0f, 750.0f, 1500.0f, 750.0f, 0, YAT_FLAG_ON);
        r10c_work->hit[2][1] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x63)->pos, &SmdGetObjPtr(0x61)->ang, 0);
        YarareInitCube(r10c_work->hit[2][1], 0.0f, 1500.0f, 0.0f, 100.0f, 1800.0f, 100.0f, 0, YAT_FLAG_ON);
        r10c_work->hit[2][2] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                          &SmdGetObjPtr(0x63)->pos, &SmdGetObjPtr(0x61)->ang, 0);
        YarareInitCube(r10c_work->hit[2][2], 0.0f, 450.0f, 0.0f, 500.0f, 1500.0f, 500.0f, 0, YAT_FLAG_ON);
    } else {
        cObj* obj;

        SmdSetTrans(0x6C, 0);
        SceExec(0x12, (TaskFunc) hako_down, (int) SmdGetObjPtr(0x63), 0, SCE_PRIO_DEF_2, 0);
        obj = SmdGetObjPtr(0x63);
        obj->pos.x = 93412.0f;
        obj->pos.y = -16601.0f;
        obj->pos.z = 46723.0f;
    }
    for (;;) {
        angA += spdA * r10c_crateHalf;
        angB += spdB * r10c_crateHalf;
        angC += spdC * r10c_crateHalf;
        if (angA > 0.0f) {
            spdA -= r10c_crateAcc;
        } else {
            spdA += r10c_crateAcc;
        }
        if (angB > 0.0f) {
            spdB -= r10c_crateAcc;
        } else {
            spdB += r10c_crateAcc;
        }
        if (angC > 0.0f) {
            spdC -= r10c_crateAcc;
        } else {
            spdC += r10c_crateAcc;
        }
        if (RsfCheck(G_ROOM_ID, 14) == 0) {
            SmdGetObjPtr(0x5E)->ang.y = angA;
            SmdGetObjPtr(0x61)->ang.y = angA;
            SmdGetObjPtr(0x6A)->ang.y = angA;
            RotMatrix(m, &SmdGetObjPtr(0x5E)->ang);
            PSMTXMultVec(m, &ofsA, &tmp);
            PSVECAdd(&tmp, &SmdGetObjPtr(0x5E)->pos, &tmp);
            SmdGetObjPtr(0x61)->pos = tmp;
            SmdGetObjPtr(0x6A)->pos.x = tmp.x;
            SmdGetObjPtr(0x6A)->pos.z = tmp.z;
            r10c_work->hit[0][0]->pos = tmp;
            r10c_work->hit[0][1]->pos = tmp;
            r10c_work->hit[0][2]->pos = tmp;
            r10c_work->hit[0][0]->ang.y = angA;
            r10c_work->hit[0][1]->ang.y = angA;
            r10c_work->hit[0][2]->ang.y = angA;
            EmHitUpdate(r10c_work->hit[0][0]);
            EmHitUpdate(r10c_work->hit[0][1]);
            EmHitUpdate(r10c_work->hit[0][2]);
        }
        if (RsfCheck(G_ROOM_ID, 15) == 0) {
            SmdGetObjPtr(0x5F)->ang.y = angB;
            SmdGetObjPtr(0x62)->ang.y = angB;
            SmdGetObjPtr(0x6B)->ang.y = angB;
            RotMatrix(m, &SmdGetObjPtr(0x5F)->ang);
            PSMTXMultVec(m, &ofsB, &tmp);
            PSVECAdd(&tmp, &SmdGetObjPtr(0x5F)->pos, &tmp);
            SmdGetObjPtr(0x62)->pos = tmp;
            SmdGetObjPtr(0x6B)->pos.x = tmp.x;
            SmdGetObjPtr(0x6B)->pos.z = tmp.z;
            r10c_work->hit[1][0]->pos = tmp;
            r10c_work->hit[1][1]->pos = tmp;
            r10c_work->hit[1][2]->pos = tmp;
            r10c_work->hit[1][0]->ang.y = angB;
            r10c_work->hit[1][1]->ang.y = angB;
            r10c_work->hit[1][2]->ang.y = angB;
            EmHitUpdate(r10c_work->hit[1][0]);
            EmHitUpdate(r10c_work->hit[1][1]);
            EmHitUpdate(r10c_work->hit[1][2]);
        }
        if (RsfCheck(G_ROOM_ID, 16) == 0) {
            SmdGetObjPtr(0x60)->ang.y = angC;
            SmdGetObjPtr(0x63)->ang.y = angC;
            SmdGetObjPtr(0x6C)->ang.y = angC;
            RotMatrix(m, &SmdGetObjPtr(0x60)->ang);
            PSMTXMultVec(m, &ofsC, &tmp);
            PSVECAdd(&tmp, &SmdGetObjPtr(0x60)->pos, &tmp);
            SmdGetObjPtr(0x63)->pos = tmp;
            SmdGetObjPtr(0x6C)->pos.x = tmp.x;
            SmdGetObjPtr(0x6C)->pos.z = tmp.z;
            r10c_work->hit[2][0]->pos = tmp;
            r10c_work->hit[2][1]->pos = tmp;
            r10c_work->hit[2][2]->pos = tmp;
            r10c_work->hit[2][0]->ang.y = angC;
            r10c_work->hit[2][1]->ang.y = angC;
            r10c_work->hit[2][2]->ang.y = angC;
            EmHitUpdate(r10c_work->hit[2][0]);
            EmHitUpdate(r10c_work->hit[2][1]);
            EmHitUpdate(r10c_work->hit[2][2]);
        }
        if (RsfCheck(G_ROOM_ID, 14) == 0) {
            if (r10c_work->hit[0][0]->ckStatus() == 1 || r10c_work->hit[0][1]->ckStatus() == 1 ||
                r10c_work->hit[0][2]->ckStatus() == 1) {
                r10c_work->hit[0][0]->hp = 0;
                r10c_work->hit[0][1]->hp = 0;
                r10c_work->hit[0][2]->hp = 0;
                SmdSetTrans(0x6A, 0);
                SndCall(6, 1, &SmdGetObjPtr(0x61)->pos, 0, 0, 0);
                EstSet(0, -1, &SmdGetObjPtr(0x61)->pos, 0, EFF_ROOM, 0xA, 0, ESP_CORE_KIND_NONE, 0, 0);
                SceExec(0x12, (TaskFunc) hako_down, (int) SmdGetObjPtr(0x61), 0, SCE_PRIO_DEF_2, 0);
                RsfSet(G_ROOM_ID, 14);
            }
        }
        if (RsfCheck(G_ROOM_ID, 15) == 0) {
            if (r10c_work->hit[1][0]->ckStatus() == 1 || r10c_work->hit[1][1]->ckStatus() == 1 ||
                r10c_work->hit[1][2]->ckStatus() == 1) {
                r10c_work->hit[1][0]->hp = 0;
                r10c_work->hit[1][1]->hp = 0;
                r10c_work->hit[1][2]->hp = 0;
                SmdSetTrans(0x6B, 0);
                SndCall(6, 1, &SmdGetObjPtr(0x62)->pos, 0, 0, 0);
                EstSet(0, -1, &SmdGetObjPtr(0x62)->pos, 0, EFF_ROOM, 0xA, 0, ESP_CORE_KIND_NONE, 0, 0);
                SceExec(0x12, (TaskFunc) hako_down, (int) SmdGetObjPtr(0x62), 0, SCE_PRIO_DEF_2, 0);
                RsfSet(G_ROOM_ID, 15);
            }
        }
        if (RsfCheck(G_ROOM_ID, 16) == 0) {
            if (r10c_work->hit[2][0]->ckStatus() == 1 || r10c_work->hit[2][1]->ckStatus() == 1 ||
                r10c_work->hit[2][2]->ckStatus() == 1) {
                r10c_work->hit[2][0]->hp = 0;
                r10c_work->hit[2][1]->hp = 0;
                r10c_work->hit[2][2]->hp = 0;
                SmdSetTrans(0x6C, 0);
                SndCall(6, 1, &SmdGetObjPtr(0x63)->pos, 0, 0, 0);
                EstSet(0, -1, &SmdGetObjPtr(0x63)->pos, 0, EFF_ROOM, 0xA, 0, ESP_CORE_KIND_NONE, 0, 0);
                SceExec(0x12, (TaskFunc) hako_down, (int) SmdGetObjPtr(0x63), 0, SCE_PRIO_DEF_2, 0);
                RsfSet(G_ROOM_ID, 16);
            }
        }
        SceSleep(1);
    }
}

// A shot crate falls into the pool, floats to the dam and becomes a stepping stone.
static void hako_down(cObj* obj)
{
    f32 spdX = 0.0f;
    f32 spdY = -20.0f;
    // `const`: both uses fold to the literal, so the limit is a pool load inside the loop (262
    // instead of 260 real insns at loop.c pass 2) that pass 2 hoists ahead of the 0.0 copy; the
    // extra `threshold -= 3` step leaves the -100.0 of the `spdY < -100.0f` arm in the arm (the
    // original reloads it there) and puts the hoisted limit behind the 0.022 pair (r11/r9).
    const f32 lim = -16141.0f;
    int splash = 0;
    int landed = 0;

    for (;;) {
        f32 dy = obj->pos.y - lim;

        spdY += r10c_crateGrav;
        if (dy < 0.0f) {
            if (spdY < -100.0f) {
                spdY *= 0.8f;
            } else {
                spdY *= 0.935f;
            }
            spdY += r10c_crateGrav * (dy * 0.0025f);
        }
        if (obj->pos.x < 93412.0f && obj->pos.y < lim) {
            if (splash == 0) {
                f32 h;

                splash = 1;
                SndCall(6, 0xD, &obj->pos, 0, 0, 0);
                if (GetWaterHeight(&obj->pos, &h)) {
                    Vec v;

                    v = obj->pos;
                    v.y = 0.0f;
                    EstSet(0, -1, &v, 0, EFF_ROOM, 7, 0, ESP_CORE_KIND_NONE, 0, 0);
                }
            }
            spdX += r10c_crateSpd;
            if (spdX > 100.0f) {
                spdX = 100.0f;
            }
        }
        if (obj->pos.x >= 93412.0f) {
            obj->pos.x = 93412.0f;
            obj->ang.y *= 0.96f;
            spdX = 0.0f;
            if (landed == 0) {
                landed = 1;
                if (r10c_work->cnt > 100) {
                    SndCall(6, 0xE, &obj->pos, 0, 0, 0);
                }
                Vec pos = {0.0f, 0.0f, 0.0f};
                Vec rot = {0.0f, 0.0f, 0.0f};

                if (obj == SmdGetObjPtr(0x61)) {
                    RsfSet(G_ROOM_ID, 11);
                    SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
                }
                if (obj == SmdGetObjPtr(0x62)) {
                    RsfSet(G_ROOM_ID, 12);
                    SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 3);
                }
                if (obj == SmdGetObjPtr(0x63)) {
                    RsfSet(G_ROOM_ID, 13);
                    SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 4);
                }
            }
            obj->pos.y += (-16601.0f - obj->pos.y) * 0.05f;
            spdY *= 0.5f;
        }
        if (splash == 1) {
            if (obj == SmdGetObjPtr(0x61)) {
                obj->pos.z += (32568.0f - obj->pos.z) * 0.022f;
            }
            if (obj == SmdGetObjPtr(0x62)) {
                obj->pos.z += (42216.0f - obj->pos.z) * 0.022f;
            }
            if (obj == SmdGetObjPtr(0x63)) {
                obj->pos.z += (46723.0f - obj->pos.z) * 0.022f;
            }
            obj->ang.y *= 0.97f;
        }
        obj->pos.x += spdX;
        obj->pos.y += spdY;
        SceSleep(1);
    }
}

// Area 0x80: the key item on the gate; the gate rises and Leon is put on the far side.
static void r10c_ItemGet()
{
    SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    if (SceMesGetSelection() == 2) {
        SceExit();
    }
    SceAtDataReset(0x80);
    SceAtExecute(0x80);
    while (SceAtItemFlgCk(0x80) == 0) {
        SceSleep(1);
    }
    SceEventStart(0);
    CamCtrl.CutCall(0x1A);
    {
        f32 spd = 25.0f;

        EstSet(0, -1, 0, 0, EFF_ROOM, 0xC, 1, ESP_CORE_KIND_ROOM01, 0, 0);
        SndCall(6, 0, 0, 0, 0, 0);
        do {
            cObj* obj = SmdGetObjPtr(0x71);

            obj->be_flag |= 0x20;
            obj->pos.y += spd;
            spd *= 0.997f;
            if (obj->pos.y > 2400.0f) {
                break;
            }
            SceSleep(1);
        } while (1);
    }
    SndCall(6, 6, 0, 0, 0, 0);
    SceAtSetEnable(7, 0);
    RsfSet(G_ROOM_ID, 8);
    ScfFlagOn(pG, SCF_9f);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    {
        Vec pos = {40147.0f, -14349.0f, 36583.0f};
        Vec ang = {0.0f, -1.4f, 0.0f};

        pPL->setPos(&pos);
        pPL->setAng(&ang);
    }
    OpeSetOpenTerm(9, 0.0f, 0.0f, 0.0f, 0.0f);
}

// Swaps the pool's water attribute for the drained one.
extern "C" void eat_swap()
{
    if (!(pG->Room_flg[0] & 0x02000000)) {
        pG->Room_flg[0] |= 0x02000000;
        Vec zero = {0.0f, 0.0f, 0.0f};

        r10c_work->eat2 = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 7);
        EatMgr.destroy(r10c_work->eat);
    }
}
