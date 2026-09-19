#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "game.h"
#include "db_log.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emdoor.h"
#include "emhit.h"
#include "emrock.h"
#include "etc_model.h"
#include "read.h"
#include "esp.h"
#include "est.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "math_sub.h"
#include "item.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "sscrn.h"
#include "snd.h"

// Room 1-1e (D:/Bio4/Prog/r11e.cpp): the village path with the two huts and the two fences the
// Ganados break through, the two boulders on the props, the giant's appearance and Ashley's
// escape.

struct R11eWork {
    cEmRock* rock[2];   // 0x00  the boulders on the props
    cSat* sat[4];       // 0x08  hut A / hut B / fence A / fence B collision
    cSat* eat[4];       // 0x18  ... attribute collision
    cEmWrap em;         // 0x28  the giant
    u32 strId;          // 0x34  SndStrReq handle of the giant stream
};

static R11eWork* r11e_work;

// Pointer stores through references: the work pointer and the field are reloaded after them.
static inline void PSet(cEmRock*& d, cEmRock* v) { d = v; }
static inline void PSet(cSat*& d, cSat* v) { d = v; }    // the pG reload of the next create waits for the store

// The original's .data is 8-aligned (r105 has the same).
asm(".section .data; .balign 8");
static Vec r11e_koyaAPos = {13389.0f, -56.0f, 38152.0f};
static Vec r11e_koyaBPos = {20505.0f, -56.0f, 36293.0f};
static Vec r11e_sakuAPos = {1579.0f, 0.0f, 35259.0f};
static Vec r11e_sakuBPos = {44737.0f, 0.0f, 37383.0f};
static Vec r11e_koyaARot = {0.0f, -3.2183871f, 0.0f};
static Vec r11e_koyaBRot = {0.0f, -2.9024825f, 0.0f};
static Vec r11e_sakuARot = {0.0f, 0.7773697f, 0.0f};
static Vec r11e_sakuBRot = {0.0f, 1.5707964f, 0.0f};
static Vec r11e_ashleyTarget = {-5734.0f, 11312.0f, 23207.0f};
static Vec r11e_sakuATarget = {-40.0f, 1500.0f, 33133.0f};
static Vec r11e_sakuBTarget = {42462.0f, 1500.0f, 37512.0f};
static Vec r11e_rockAPos = {-5776.0f, 10062.0f, 21841.0f};
static Vec r11e_rockARot = {0.0f, 0.0f, 0.0f};
static Vec r11e_rockBPos = {32079.0f, 10069.0f, 32556.0f};
static Vec r11e_rockBRot = {0.0f, -1.5707964f, 0.0f};

extern "C" void funcAshley(cModel* m);
static void koya_destroy_check();
extern "C" void koyaA_destroy();
extern "C" void koyaB_destroy();
extern "C" void koyaA_delete();
extern "C" void koyaB_delete();
extern "C" void sakuA_destroy();
extern "C" void sakuB_destroy();
extern "C" void sakuA_delete();
extern "C" void sakuB_delete();
static void r11e_move_sasaeki1();
static void r11e_EmSet_exit();
static void r11e_EmSet();
static void r11e_str_check();
static void r11e_checkDoor102KeyUse();
static void r11e_checkDoor();

// Room init: giant (0x2B) pre-read; area 1 = the locked door until Key_flg[0] 0x00080000 (with the
// key-use watcher); the hut/fence collapse watcher; collision and attribute pieces for huts A/B and
// fences A/B; the pieces already destroyed per Room_flg bits 0..3 are removed; the boulders on the
// props, the giant's appearance area and the battle stream.
void R11eInit()
{
    cEm* door;

#line 75 "D:/Bio4/Prog/r11e.cpp"
    r11e_work = (R11eWork*) MEM_CALLOC(sizeof(R11eWork), 1, 0xd);

    EmReadSearch(0x2B, 0, 0);
    if (!(pG->Key_flg[0] & 0x00080000)) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r11e_checkDoor, 0, 1);
        SceExec(0x12, (TaskFunc) r11e_checkDoor102KeyUse, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    SceExec(0x12, (TaskFunc) koya_destroy_check, 0, 0, SCE_PRIO_DEF_2, 0);
    PSet(r11e_work->sat[0], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x28), 0, &r11e_koyaAPos, &r11e_koyaARot, 0));
    PSet(r11e_work->sat[1], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x28), 0, &r11e_koyaBPos, &r11e_koyaBRot, 0));
    PSet(r11e_work->sat[2], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x2A), 0, &r11e_sakuAPos, &r11e_sakuARot, 0));
    PSet(r11e_work->sat[3], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x2A), 0, &r11e_sakuBPos, &r11e_sakuBRot, 0));
    PSet(r11e_work->eat[0], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x29), 0, &r11e_koyaAPos, &r11e_koyaARot, 0));
    PSet(r11e_work->eat[1], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x29), 0, &r11e_koyaBPos, &r11e_koyaBRot, 0));
    PSet(r11e_work->eat[2], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x2A), 0, &r11e_sakuAPos, &r11e_sakuARot, 0));
    PSet(r11e_work->eat[3], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x2A), 0, &r11e_sakuBPos, &r11e_sakuBRot, 0));
    getRoomEtcDoor(0xE, &door, 1);
    getRoomEtcDoor(0xF, &door, 1);
    if (RsfCheck(G_ROOM_ID, 0)) {
        koyaA_delete();
    }
    if (RsfCheck(G_ROOM_ID, 1)) {
        koyaB_delete();
    }
    if (RsfCheck(G_ROOM_ID, 2)) {
        sakuA_delete();
    }
    if (RsfCheck(G_ROOM_ID, 3)) {
        sakuB_delete();
    }
    SceExec(0x12, (TaskFunc) r11e_move_sasaeki1, 0, 0, SCE_PRIO_DEF_2, 0);
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        r11e_work->strId = SndStrReq(1, 0x4C, 0x80000001, 0, 0, 0.0f);
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r11e_EmSet, 0, 1);
    }
    SceExec(0x12, (TaskFunc) r11e_str_check, 0, 2, SCE_PRIO_DEF_2, 0);
}

// Ashley's escape motions (SetSubAux routine): three motions in a row, turning towards the target.
extern "C" void funcAshley(cModel* m)
{
    switch (m->r_no_2) {
    case 0:
        SubCharSetHand(3);
        m->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2B), 0xA, 0, 1, 0);
        m->r_no_2 = 1;
    case 1:
        if (m->motionMove()) {
            m->r_no_2 = 2;
        }
        break;
    case 2:
        m->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2C), 0xA, 0, 1, 0);
        m->r_no_2 = 3;
    case 3:
        if (m->motionMove()) {
            m->r_no_2 = 4;
        }
        break;
    case 4:
        m->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2D), 0xA, 0, 1, 0);
        m->r_no_2 = 5;
    default:
        if (m->motionMove()) {
            m->r_no_0 = 0;
            m->r_no_1 = 0;
            m->r_no_2 = 0;
            m->r_no_3 = 0;
            SubCharSetHand(0);
        }
        break;
    }
    m->ang.y += Muku(&m->pos, &r11e_ashleyTarget, m->ang.y, 0.62831855f);
}

// Per frame: once the giant fight started (Room_flg bit 6) and no giant (0x2B) is alive, camera area 1 off.
void R11eMain()
{
    if (RsfCheck(G_ROOM_ID, 6) && SceCountEmAlive(0x2B, -1) == 0) {
        CamCtrl.AreaOnOff(1, 0, 0);
    }
}

// The huts and fences fall when their event flags come up.
static void koya_destroy_check()
{
    for (;;) {
        if ((pG->Room_flg[0] & 0x80000000) && RsfCheck(G_ROOM_ID, 0) == 0) {
            RsfSet(G_ROOM_ID, 0);
            koyaA_destroy();
        }
        if ((pG->Room_flg[0] & 0x40000000) && RsfCheck(G_ROOM_ID, 1) == 0) {
            RsfSet(G_ROOM_ID, 1);
            koyaB_destroy();
        }
        if ((pG->Room_flg[0] & 0x20000000) && RsfCheck(G_ROOM_ID, 2) == 0) {
            RsfSet(G_ROOM_ID, 2);
            sakuA_destroy();
        }
        if ((pG->Room_flg[0] & 0x10000000) && RsfCheck(G_ROOM_ID, 3) == 0) {
            RsfSet(G_ROOM_ID, 3);
            sakuB_destroy();
        }
        SceSleep(1);
    }
}

// Hut A is smashed by the giant: crash SE, debris effect, models and collision removed.
extern "C" void koyaA_destroy()
{
    Vec pos = {13370.0f, -70.0f, 38135.0f};
    Vec rot = {0.0f, 3.054326f, 0.0f};

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    EstSet(0, -1, &pos, &rot, 1, 5, 0, 0, 0, 0);
    koyaA_delete();
}

// Hut B is smashed by the giant (see koyaA_destroy).
extern "C" void koyaB_destroy()
{
    Vec pos = {20500.0f, -60.0f, 36273.0f};
    Vec rot = {0.0f, 3.3684855f, 0.0f};

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    EstSet(0, -1, &pos, &rot, 1, 5, 0, 0, 0, 0);
    koyaB_delete();
}

// Remove hut A: collision pieces, scroll objects 4/5, item areas 0x80..0x84 off.
extern "C" void koyaA_delete()
{
    SatMgr.destroy(r11e_work->sat[0]);
    EatMgr.destroy(r11e_work->eat[0]);
    SmdSetTrans(4, 0);
    SmdSetTrans(5, 0);
    SceAtSetEnable(0x80, 0);
    SceAtSetEnable(0x81, 0);
    SceAtSetEnable(0x82, 0);
    SceAtSetEnable(0x83, 0);
    SceAtSetEnable(0x84, 0);
}

// Remove hut B: collision pieces, scroll objects 6/7, item areas 0x85..0x8B off.
extern "C" void koyaB_delete()
{
    SatMgr.destroy(r11e_work->sat[1]);
    EatMgr.destroy(r11e_work->eat[1]);
    SmdSetTrans(6, 0);
    SmdSetTrans(7, 0);
    SceAtSetEnable(0x85, 0);
    SceAtSetEnable(0x86, 0);
    SceAtSetEnable(0x87, 0);
    SceAtSetEnable(0x88, 0);
    SceAtSetEnable(0x89, 0);
    SceAtSetEnable(0x8A, 0);
    SceAtSetEnable(0x8B, 0);
}

// Fence A is broken through: crash SE, debris effect, door 0xE breaks toward its target, fence removed.
extern "C" void sakuA_destroy()
{
    Vec pos = {1329.0f, 0.0f, 35360.0f};
    Vec rot = {0.0f, -2.3561945f, 0.0f};
    cEm* door;

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    EstSet(0, -1, &pos, &rot, 1, 3, 0, 0, 0, 0);
    if (getRoomEtcDoor(0xE, &door, 1)) {
        ((cEmDoor*) door)->setBreak(&r11e_sakuATarget);
    }
    sakuA_delete();
}

// Fence B is broken through (door 0xF), see sakuA_destroy.
extern "C" void sakuB_destroy()
{
    Vec pos = {44629.0f, 0.0f, 37658.0f};
    Vec rot = {0.0f, -1.5707964f, 0.0f};
    cEm* door;

    SndCall(8, 0x1B, &pos, 0x2B, 0, 0);
    EstSet(0, -1, &pos, &rot, 1, 3, 0, 0, 0, 0);
    if (getRoomEtcDoor(0xF, &door, 1)) {
        ((cEmDoor*) door)->setBreak(&r11e_sakuBTarget);
    }
    sakuB_delete();
}

// Remove fence A: collision pieces, scroll objects 0xE/0xF/0x10.
extern "C" void sakuA_delete()
{
    SatMgr.destroy(r11e_work->sat[2]);
    EatMgr.destroy(r11e_work->eat[2]);
    SmdSetTrans(0xE, 0);
    SmdSetTrans(0xF, 0);
    SmdSetTrans(0x10, 0);
}

// Remove fence B: collision pieces, scroll objects 0x11/0x12/0x13.
extern "C" void sakuB_delete()
{
    SatMgr.destroy(r11e_work->sat[3]);
    EatMgr.destroy(r11e_work->eat[3]);
    SmdSetTrans(0x12, 0);
    SmdSetTrans(0x11, 0);
    SmdSetTrans(0x13, 0);
}

// The two boulders on their props: a hit enemy on each prop, the rock drops once it is shot.
static void r11e_move_sasaeki1()
{
    cObj* objA;
    cEmHit* hitA = 0;
    cEmHit* hitB = 0;
    cObj* objB;
    Vec pos;
    Vec rot;
    void* tpl;

    objA = SmdGetObjPtr(0x23);
    objB = SmdGetObjPtr(0x24);
    EspDataLoad((u32) ROOM_ARC_PTR(pG->pRoom, 0x22), 0xC8, 0);
    if (EspGetEfmTplAddr(0x20, &tpl) == 0) {
        pLog->err(0, 0, "IWA init: EFM[%02x] TPL not regist.", 0x20);
        return;
    }
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        hitA = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &objA->pos, &objA->ang, 0);
        objA->be_flag |= 0x20;
        {
            const f32 w = 2000.0f;    // const: pool order w, h before 0.0, uses stay literal (sched ties)
            const f32 h = 800.0f;
            YarareInitCube(hitA, 0.0f, 0.0f, 0.0f, w, h, w, 0, 1);
        }
        pos.x = -8182.04f;
        pos.y = 0.0f;
        pos.z = 21888.12f;
        rot.x = 0.0f;
        rot.y = 1.5707964f;
        rot.z = 0.0f;
        PSet(r11e_work->rock[0], SetRock(ROOM_ARC_PTR(pG->pRoom, 0x23), tpl, &pos, &rot, 3));
        if (r11e_work->rock[0] != 0) {
            r11e_work->rock[0]->setDropMot(ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
            r11e_work->rock[0]->setNoSuspend(1);
        }
    } else {
        objA->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        hitB = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &objB->pos, &objB->ang, 0);
        objB->be_flag |= 0x20;
        {
            const f32 w = 2000.0f;
            const f32 h = 800.0f;
            YarareInitCube(hitB, 0.0f, 0.0f, 0.0f, w, h, w, 0, 1);
        }
        pos.x = 31737.52f;
        pos.y = 0.0f;
        pos.z = 28681.43f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        PSet(r11e_work->rock[1], SetRock(ROOM_ARC_PTR(pG->pRoom, 0x23), tpl, &pos, &rot, 3));
        if (r11e_work->rock[1] != 0) {
            r11e_work->rock[1]->setDropMot(ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
            r11e_work->rock[1]->setNoSuspend(1);
        }
    } else {
        objB->be_flag &= ~2;
    }
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 4) == 0 && hitA->ckStatus() == 1) {
            RsfSet(G_ROOM_ID, 4);
            EstSet(0, -1, &r11e_rockAPos, &r11e_rockARot, 1, 0, 0, 0, 0, 0);
            BitOff(objA->be_flag, 2);
            r11e_work->rock[0]->flag |= 1;
        }
        if (RsfCheck(G_ROOM_ID, 5) == 0 && hitB->ckStatus() == 1) {
            RsfSet(G_ROOM_ID, 5);
            EstSet(0, -1, &r11e_rockBPos, &r11e_rockBRot, 1, 0, 0, 0, 0, 0);
            BitOff(objB->be_flag, 2);
            r11e_work->rock[1]->flag |= 1;
        }
        SceSleep(1);
    }
}

// The giant's appearance is over: Leon and Ashley are put back, the second giant is set.
static void r11e_EmSet_exit()
{
    EffectEspDelete(1, 0, (u32) r11e_work->em.getPtr(), 0);
    EffectEspgenDelete(1, 0, (int) r11e_work->em.getPtr());
    EffectEfmDelete(1, 0, (int) r11e_work->em.getPtr());
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r11e_work->em.setNoSuspend(0);
    {
        Vec p;

        p.x = 219.0f;
        p.y = 0.0f;
        p.z = 25932.0f;
        pPL->setPos(&p);
        p.x = 0.0f;
        p.y = 3.1f;
        p.z = 0.0f;
        pPL->setAng(&p);
    }
    GamePointBossReset();
    r11e_work->em.destroy();
    {
        cEmWrap em;
        Vec p;
        cEmWrap* pe;
        Vec* pp = &p;

        em.setEm(0xF0, -1, 1, 1, 1);
        pe = &em;    // after setEm: its `this` stays a fresh `addi r3,r1,8`, the pe addi is hoisted anyway
        p.x = 529.26f;
        pp->y = -25.64f;
        pp->z = 5624.71f;
        pe->setPos(pp);
        p.x = 0.0f;
        pp->y = -0.3f;
        p.z = 0.0f;
        pe->setAng(pp);
        if (RsfCheck(G_ROOM_ID, 4) == 0 && pSUB != 0 && (SubCharGetStatus() & 0x20000000)) {
            p.x = 219.0f;
            pp->z = 23932.0f;
            pp->y = 0.0f;
            pSUB->setPos(pp);
            p.x = 0.0f;
            pp->y = 3.1f;
            pp->z = 0.0f;
            pSUB->setAng(pp);
            SetSubAux((int) funcAshley, 0);
            SndCall(6, 2, &pSUB->pos, 0, 0, 0);
        }
    }
}

// The giant appears: camera cuts 2 and 3 with the stream.
static void r11e_EmSet()
{
    EmListData* e;

    RsfSet(G_ROOM_ID, 6);
    r11e_work->em.setEm(0xF1, -1, 1, 1, 1);
    e = EM_LIST(0xF0);
    e->pos[0] = -0xA5;
    e->pos[1] = 8;
    e->pos[2] = 0xC06;
    e->rot[1] = 0x3FA4;
    e->be_flag |= 1;
    e->set = 1;
    SceEventStart(0);
    SndStrReq(r11e_work->strId, 2, 0, 0);
    r11e_work->em.setNoSuspend(1);
    SceSetEventCancel(1, (TaskFunc) r11e_EmSet_exit, 0, -1, 1);
    CamCtrl.CutCall(2);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        CamCtrl.CutCall(3);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r11e_EmSet_exit();
}

// Battle stream while a giant is alive and attacking.
static void r11e_str_check()
{
    int on = 0;

    for (;;) {
        int find = 0;
        u32 i;

        for (i = 0; i < EmMgr.nArray; i++) {
            cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

            if (em->id == 0x2B && em->checkStatus(EM_STATUS_ACTIVE) != 0 && em->hp > 0 && (em->be_flag & 0x201) == 1) {
                find = 1;
            }
        }
        if (find == 1) {
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

// The player uses the key on the door.
static void r11e_checkDoor102KeyUse()
{
    while (ItemMgr.check(0x8B) != 1) {
        SceSleep(1);
    }
    ItemMgr.dump(0x8B);
    SndCall(6, 5, 0, 0, 0, 0);
    SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    pG->Key_flg[0] |= 0x00080000;
    SceAtDataReset(1);
}

// Area 1, the locked door: the up-cut message; with the key (item 0x8B) held the item screen opens to use it.
static void r11e_checkDoor()
{
    SceUpCut(0, -1, 4, 0);
    if (ItemMgr.num(0x8B) != 0) {
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
    }
}
