#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
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
#include "datactrl.h"
#include "esp.h"
#include "est.h"
#include "player.h"
#include "pl_sub.h"
#include "snd.h"
#include "rnd.h"

// Room 1-06 (D:/Bio4/Prog/r106.cpp): the village hall; the boulder, the two Ganado waves, the
// shelves, the shaking closet (Luis tied up inside) and the r106s00 event that ends chapter 1-1.

struct R106Work {
    int x0;
    int strOn;        // 0x04  battle stream running
    cDataUnit* evd;   // 0x08  the event data file
    cObj* closet;     // 0x0C  the closet scroll object
    cEmRock* rock;    // 0x10  the boulder
};

static R106Work* r106_work;


// Hit effects of attribute type 4 (the hall floor)
static const AtEffInfo r106_eff_info = {
    0, {1, 0xFF}, {1, 0xF}, {0, 0xB}, {0, 0xC}, {1, 0xE}, {1, 0xE}, {0, 0x36}, {0xD2, 0},
};

void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp

// setAng through an inline taking the angle by pointer: the frame address is substituted straight
// into the argument register (a fresh `addi r4, r1, ofs`) instead of a PRE'd pseudo.
static inline void r106_emSetAng(cEmWrap* em, Vec* ang) { em->setAng(ang); }

static void r106_checkRollingStone();
extern "C" void r106_setRollingStone();
static void r106_ctrlEm0();
static void r106_ctrlEm1();
extern "C" void r106_openShelf_main(int type, int opened);
static void r106_openedShelf(int type);
static void r106_openShelf(int type);
static void r106_ctrlBgm(int on);
static void r106_Event();
static void r106_shakeClosetBody(cModel* m);
static void r106_shakeClosetDoorR(cModel* m);
static void r106_shakeClosetDoorL(cModel* m);
static void r106_setCloset();
extern "C" void Evt_R106S00_Func(Event* ev);
extern "C" void r106_setEm();

// Room init: Scenario_flg[0] 0x800, the r106s00 event callback, floor hit effects, door 8 gets the lock
// models; two shelf item events (items 0x85/0x86). Until Scenario_flg[0] 0x00200000 (Luis found): area 2
// = the closet event, evd pre-loaded to ARAM, enemies 0x12/0x29/0x2A/0x2E pre-read, areas 4/5 = battle
// stream on/off, the shaking closet; otherwise area 0xE off. Areas 8/9 post two Ganados; the boulder
// unless Room_flg bit 2; the six hall Ganados; rack 0 range; a fixed hit piece at the far wall.
void R106Init()
{
    Vec pos;
    Vec rot;
    cEmDoor* door;
    cEm* rack;
    cEmHit* hit;

#line 66 "D:/Bio4/Prog/r106.cpp"
    r106_work = (R106Work*) MEM_CALLOC(sizeof(R106Work), 1, 0xd);

    ScfFlagOn(pG, SCF_R106_ENTER);
    EvtMgr.SetFunc("evt_r106s00_func", (void*) Evt_R106S00_Func);
    EatMgr.registEffInfo(EAT_ET_ROOM0, (AtEffInfo*) &r106_eff_info);
    if (getRoomEtcDoor(8, &door, 1)) {
        door->setLock(ROOM_ARC_PTR(pG->pRoom, 0x32), ROOM_ARC_PTR(pG->pRoom, 0x33), 0, 0);
    }
    SceSetItemEvent(6, 0x85, 0, 6, r106_openShelf, r106_openedShelf, 0, 0);
    SceSetItemEvent(7, 0x86, 1, 7, r106_openShelf, r106_openedShelf, 1, 0);
    if (!ScfFlagChk(pG, SCF_R106_EVENT)) {
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r106_Event, 0, 1);
        r106_work->evd = DC.setData(EvtMgr.NameChange("evd/r106s00.evd"));
        r106_work->evd->setCommand(CMND_ARAM_LOAD, 0, 0);
        EmReadSearch(0x12, 0, 0x3C0000);
        EmReadSearch(0x29, 0, 0);
        EmReadSearch(0x2A, 0, 0);
        EmReadSearch(0x2E, 0, 0);
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r106_ctrlBgm, (void*) 1, 1);
        SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r106_ctrlBgm, 0, 1);
        SceExec(0x12, (TaskFunc) r106_setCloset, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceAtSetEnable(0xE, 0);
    }
    SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r106_ctrlEm0, 0, 1);
    SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r106_ctrlEm1, 0, 1);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        r106_setRollingStone();
    }
    r106_setEm();
    if (getRoomEtcRack(0, &rack, 1)) {
        ((cEmRack*) rack)->setRange(0.0f, 1800.0f, 0.0f, 2200.0f);
    }
    pos.x = 33528.0f;
    pos.y = -7745.0f;
    pos.z = -10770.0f;
    rot.x = 0.0f;
    rot.y = 2.75f;
    rot.z = 0.0f;
    hit = SetEmHit(ROOM_ARC_PTR(pG->pRoom, 0x36), ROOM_ARC_PTR(pG->pRoom, 0x37), &pos, &rot, 2);
    if (hit != 0) {
        hit->setBeetle(ROOM_ARC_PTR(pG->pRoom, 0x38), ROOM_ARC_PTR(pG->pRoom, 0x3A), ROOM_ARC_PTR(pG->pRoom, 0x39));
    }
}

// Per-frame room main: nothing.
void R106Main()
{
}

// Sets room flag 2 once the boulder has fallen.
static void r106_checkRollingStone()
{
    for (;;) {
        if (r106_work->rock != 0 && (r106_work->rock->flag & 1)) {
            RsfSet(G_ROOM_ID, 2);
        }
        SceSleep(1);
    }
}

// The boulder ("IWA") with its player motions and the three Ganados pushing it.
extern "C" void r106_setRollingStone()
{
    Vec pos;
    Vec rot;
    void* tpl;
    cEmRock* rock;

    pos.x = 19880.0f;
    pos.y = 6030.0f;
    pos.z = -1540.0f;
    rot.x = 0.0f;
    rot.y = 1.813549f;
    rot.z = 0.0f;
    EspDataLoad((u32) ROOM_ARC_PTR(pG->pRoom, 0x2E), EFF_OBM1F, 0);
    if (EspGetEfmTplAddr(0x20, &tpl) == 0) {
        pLog->err(0, 0, "IWA init: EFM[%02x] TPL not regist.", 0x20);
        return;
    }
    rock = SetRock(ROOM_ARC_PTR(pG->pRoom, 0x20), tpl, &pos, &rot, 1);
    r106_work->rock = rock;
    if (rock != 0) {
        void* mot[16];

        mot[0] = ROOM_ARC_PTR(pG->pRoom, 0x21);
        mot[1] = ROOM_ARC_PTR(pG->pRoom, 0x22);
        mot[2] = ROOM_ARC_PTR(pG->pRoom, 0x23);
        mot[3] = ROOM_ARC_PTR(pG->pRoom, 0x24);
        mot[4] = ROOM_ARC_PTR(pG->pRoom, 0x25);
        mot[5] = ROOM_ARC_PTR(pG->pRoom, 0x26);
        mot[6] = ROOM_ARC_PTR(pG->pRoom, 0x27);
        mot[7] = ROOM_ARC_PTR(pG->pRoom, 0x28);
        mot[8] = ROOM_ARC_PTR(pG->pRoom, 0x29);
        mot[9] = ROOM_ARC_PTR(pG->pRoom, 0x2A);
        mot[10] = ROOM_ARC_PTR(pG->pRoom, 0x2B);
        mot[11] = ROOM_ARC_PTR(pG->pRoom, 0x2C);
        mot[12] = ROOM_ARC_PTR(pG->pRoom, 0x2D);
        mot[13] = ROOM_ARC_PTR(pG->pRoom, 0x2F);
        mot[14] = ROOM_ARC_PTR(pG->pRoom, 0x30);
        mot[15] = ROOM_ARC_PTR(pG->pRoom, 0x31);
        rock->setPlMotion(mot);
        rock->setScale(4.2f);
    }
    {
        EmListData d;

        d.id = 0x12;
        d.type = 3;
        d.set = 0x1C;
        d.flag = 0;
        d.pos[0] = 0x78D;
        d.pos[1] = 0x193;
        d.pos[2] = -0x1E3;
        d.rot[0] = 0;
        d.rot[1] = 0x1C7;
        d.rot[2] = 0;
        d.hp = 0;
        d.Guard_r = 1;
        d.Character = 1;
        EmSetEvent(&d);

        d.id = 0x12;
        d.type = 1;
        d.set = 0x1C;
        d.flag = 0;
        d.pos[0] = 0x81A;
        d.pos[1] = 0x193;
        d.pos[2] = -0x1D8;
        d.rot[0] = 0;
        d.rot[1] = -0x7D2;
        d.rot[2] = 0;
        d.hp = 0;
        d.Guard_r = 1;
        d.Character = 1;
        EmSetEvent(&d);

        d.id = 0x12;
        d.type = 0;
        d.set = 0x1C;
        d.flag = 0;
        d.pos[0] = 0x6EC;
        d.pos[1] = 0x19A;
        d.pos[2] = -0x1C2;
        d.rot[0] = 0;
        d.rot[1] = 0x999;
        d.rot[2] = 0;
        d.hp = 0;
        d.Guard_r = 1;
        d.Character = 1;
        EmSetEvent(&d);
    }
    SceExec(0x12, (TaskFunc) r106_checkRollingStone, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Area 8: Ganado 0x8F walks to its post.
static void r106_ctrlEm0()
{
    Vec pos = {118015.0f, -7720.0f, -18544.0f};
    cEmWrap em;

    em.setPtr(0x8F, -1, 0);
    em.setGoto(&pos, 1);
}

// Area 9: Ganado 0x8C walks to its post and turns.
static void r106_ctrlEm1()
{
    Vec pos = {120200.0f, -7720.0f, -23730.0f};
    f32 ry = 1.305f;
    cEmWrap em;
    Vec ang;

    em.setPtr(0x8C, -1, 0);
    em.setGoto(&pos, 1);
    while (em.ckGoto() != 0) {
        SceSleep(1);
    }
    ang.y = ry;
    ang.x = 0.0f;
    ang.z = 0.0f;
    r106_emSetAng(&em, &ang);
}

// Open shelf `type` (opened != 0: already open): the two doors turn 110 degrees over 30 frames.
extern "C" void r106_openShelf_main(int type, int opened)
{
    // b declared first: its `li` is the first insn of block 0 (LUID tie of the two zero inits).
    cObj* b = 0;
    cObj* a = 0;

    switch (type) {
    case 0:
        a = SmdGetObjPtr(0x29);
        b = SmdGetObjPtr(0x2A);
        break;
    case 1:
        a = SmdGetObjPtr(0x2B);
        b = SmdGetObjPtr(0x2C);
        break;
    }
    if (a != 0 && b != 0) {
        a->be_flag |= 0x20;
        b->be_flag |= 0x20;
        if (opened == 1) {
            f32 ra = -1.9198622f;
            f32 rb = 1.9198622f;
            // COMPILER-DIFF: candidate #17 (local-alloc qty order of the two pool highs), the
            // r103 openShelf_main recipe: the volatile load + hard-register copy + keep-alive asm
            // put a no-op `mr r10,r10` (deleted by reload_cse) between the second `lis` and its
            // `lfs` in sched1, so the two highs' spans tie and the -1.92 high is allocated first.
            // The keep-alive reads b (not a) so the a/b global-alloc order is unchanged.
            cModel* pa = *(cModel* volatile*) &a->pList;
            register cModel* pa2 asm("r10");

            pa2 = pa;
            pa2->ang.y = ra;
            asm("" : "=m"(b->be_flag) : "r"(pa2));
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

// Item-event "already opened": pose shelf `type` open.
static void r106_openedShelf(int type)
{
    r106_openShelf_main(type, 1);
}

// Item-event opener: animate shelf `type` open.
static void r106_openShelf(int type)
{
    r106_openShelf_main(type, 0);
}

// Battle stream while the player is in area 4 (on = 1) / 5 (on = 0).
static void r106_ctrlBgm(int on)
{
    if (on == 1) {
        if (r106_work->strOn == 0) {
            r106_work->strOn = on;
            SndRoomStrStart(1, 3, 1);
        }
    } else {
        if (r106_work->strOn == 1) {
            r106_work->strOn = 0;
            SndRoomStrStop(3);
        }
    }
}

// Event r106s00: the enemies go, the event plays, chapter 1-1 ends.
static void r106_Event()
{
    Event* ev;

    ScfFlagOn(pG, SCF_R106_EVENT);
    ScfFlagOn(pG, SCF_R106_CONFINEED_WITH_LUIS);
    SceEventStart(0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    SndRoomStrStop(3);
    EmMgr.destroyAll();
    SceSleep(2);
    EmReadInit();
    r106_work->evd->setCommand(CMND_MRAM_LOAD, 0, 1);
    if (r106_work->evd->waitLoadOk()) {
        EventMgr* evt;

        if (EvtMgr.SetEvt(r106_work->evd->m_addr, (u32*) &ev)) {
            ev->StatusFlag |= EvtStfBit(EvtStfFadeOut);
        }
        evt = &EvtMgr;
        while (evt->IsAliveEvt(&evt->NowExeEvtKey, 0, 0) != 0) {
            SceSleep(1);
        }
    }
    r106_work->evd->setCommand(CMND_DEL_DATA, 0, 0);
    PlSetCostume();
    SceEventEnd(0);
    SceSetChapterEnd(0, 3);
}

// The closet rocks: body tilt and back. OPEN (r103 execOpenCover has the same shape): the
// original's loop-test blocks have their leading load / compare duplicated into both predecessors
// (no loop notes, constants reloaded after the call); no source form gives that with our cc1plus.
// The swing-open halves are a peeled first step + a goto loop inside the `if` (no loop notes, so the
// step constant is reloaded per iteration like the target); the peel's own compare is what the target
// cross-jumps into the loop's `ble` (`fadds; fcmpu; stfs; b L`). The `goto open; open:` form (jumping into
// the loop's test) reloads rot before the compare instead.
static void r106_shakeClosetBody(cModel* m)
{
    f32 lim = fRand0_1() * 0.015707962f + 0.006981317f;
    f32 spd = fRand0_1() * 0.008726646f + 0.004363323f;

    for (;;) {
        m->ang.x += spd;
        if (m->ang.x > lim) {
            m->ang.x = lim;
            break;
        }
        SceSleep(1);
    }
    for (;;) {
        m->ang.x -= spd;
        if (m->ang.x < 0.0f) {
            m->ang.x = 0.0f;
            break;
        }
        SceSleep(1);
    }
}

// The right closet door swings open a little and shuts.
static void r106_shakeClosetDoorR(cModel* m)
{
    f32 lim = fRand0_1() * 0.034906585f + 0.034906585f;

    // The exit store inside the break path keeps jump1 from folding the peeled exit test's
    // `ble TOP` over `b END`; jump2's fall-through cross-jump then merges the two exit jumps
    // (`cmp; b TEST; ...; TEST: ble TOP`) -- see docs/matching.md COMPILER-DIFF #7/#9 (not a diff).
    for (;;) {
        m->ang.y += 0.02617994f;
        if (m->ang.y > lim) {
            m->ang.y = lim;
            break;
        }
        SceSleep(1);
    }
    for (;;) {
        m->ang.y -= 0.02617994f;
        if (m->ang.y < 0.0f) {
            m->ang.y = 0.0f;
            break;
        }
        SceSleep(1);
    }
}

// The left closet door swings open a little and shuts.
static void r106_shakeClosetDoorL(cModel* m)
{
    f32 lim = fRand0_1() * 0.06981317f - 0.06981317f;

    for (;;) {
        m->ang.y -= 0.05235988f;
        if (m->ang.y < lim) {
            m->ang.y = lim;
            break;
        }
        SceSleep(1);
    }
    for (;;) {
        m->ang.y += 0.05235988f;
        if (m->ang.y > 0.0f) {
            m->ang.y = 0.0f;
            break;
        }
        SceSleep(1);
    }
}

// The closet Luis is tied up in: created, then it shakes every 5..260 frames until the event.
static void r106_setCloset()
{
    Vec pos = {158202.0f, -9297.0f, -43582.0f};
    Vec rot = {0.0f, -1.5707964f, 0.0f};
    cObj* obj;
    cParts* body;
    cParts* doorR;
    cParts* doorL;
    int cnt = 0;

    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x34), ROOM_ARC_PTR(pG->pRoom, 0x35), &pos, &rot, 0x10, 1);
    r106_work->closet = obj;
    body = GetPartsAddr(obj->pList, 0);
    doorR = GetPartsAddr(obj->pList, 2);
    doorL = GetPartsAddr(obj->pList, 1);
    while (!ScfFlagChk(pG, SCF_R106_EVENT)) {
        if (cnt <= 0) {
            Vec sp = {157059.0f, -9245.0f, -43597.0f};

            SndCall(6, 4, &sp, 0, 0, 0);
            SceExec(0x12, (TaskFunc) r106_shakeClosetBody, (int) body, 0, SCE_PRIO_DEF_2, 0);
            SceExec(0x12, (TaskFunc) r106_shakeClosetDoorR, (int) doorR, 0, SCE_PRIO_DEF_2, 0);
            SceExec(0x12, (TaskFunc) r106_shakeClosetDoorL, (int) doorL, 0, SCE_PRIO_DEF_2, 0);
            cnt = ((Rnd() >> 2) & 0xFF) + 5;
        }
        cnt--;
        SceSleep(1);
    }
}

// Event r106s00 handler: the closet, the Ganado models in the doorway, the weapon.
extern "C" void Evt_R106S00_Func(Event* ev)
{
    void* mod;

    switch (ev->FuncType) {
    case 0:
        r106_work->closet->be_flag &= ~2;
        break;
    case 1:
        switch (ev->NowCut) {
        case 0:
            if (ev->NowFrame == 0) {
                if (ev->GetMod(&mod, "obm4000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag |= 2;
                }
            } else {
                return;
            }
            break;
        case 0x10:
            if (ev->NowFrame == 0) {
                EffectEspDelete(0, ESP_CORE_KIND_ROOM_AREA09, 0, 0);
                EffectEspgenDelete(0, ESP_CORE_KIND_ROOM_AREA09, 0);
                EffectEfmDelete(0, ESP_CORE_KIND_ROOM_AREA09, 0);
            } else {
                return;
            }
            break;
        case 0x11:
            if (ev->NowFrame == 0) {
                if (ev->GetMod(&mod, "obm4000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag &= ~2;
                }
            } else {
                return;
            }
            break;
        case 0x12:
            if (ev->NowFrame == 0x41) {
                if (ev->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag |= 2;
                }
                if (ev->GetMod(&mod, "pl0400", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag |= 2;
                }
            }
            break;
        }
        if (ev->NowFrame == 0) {
            if (ev->GetMod(&mod, "wep0200", 0, 0) == 1) {
                if (ev->NowCut > 3) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag &= ~2;
                } else {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag |= 2;
                }
            }
        }
        break;
    case 2:
        r106_work->closet->be_flag |= 2;
        break;
    }
}

// The six Ganados of the hall.
extern "C" void r106_setEm()
{
    EmListData d;

    d.rot[0] = 0;
    d.rot[2] = 0;
    d.id = 0x29;
    d.pos[0] = 0x1E5C;
    d.pos[1] = -0x23E;
    d.pos[2] = -0xA30;
    d.rot[1] = -0x1EEE;
    d.type = 0;
    d.set = 1;
    d.flag = 0;
    d.Character = 0;
    d.hp = 0xA;
    d.Guard_r = 0xB;
    EmSetEvent(&d);

    d.id = 0x29;
    d.pos[0] = 0x1E2B;
    d.pos[1] = -0x23E;
    d.pos[2] = -0xA30;
    d.rot[1] = -0x1EEE;
    d.type = 0;
    d.set = 1;
    d.flag = 0;
    d.Character = 0;
    d.hp = 0xA;
    d.Guard_r = 0xA;
    EmSetEvent(&d);

    d.id = 0x29;
    d.pos[0] = 0x1EDE;
    d.pos[1] = -0x23E;
    d.pos[2] = -0xA33;
    d.rot[1] = -0x1EEE;
    d.type = 0;
    d.set = 1;
    d.flag = 0;
    d.Character = 0;
    d.hp = 0xA;
    d.Guard_r = 0xB;
    EmSetEvent(&d);

    d.id = 0x2E;
    d.pos[0] = 0x2C94;
    d.pos[1] = -0x339;
    d.pos[2] = -0xD20;
    d.rot[1] = -0xDDD;
    d.type = 1;
    d.set = 0;
    d.flag = 0;
    d.Character = 0;
    d.hp = 0x3E8;
    d.Guard_r = 0;
    EmSetEvent(&d);

    d.id = 0x2E;
    d.pos[0] = 0x2B67;
    d.pos[1] = -0x292;
    d.pos[2] = -0xDC6;
    d.rot[1] = 0x3BBB;
    d.type = 0;
    d.set = 1;
    d.flag = 0;
    d.Character = 0;
    d.hp = 0x3E8;
    d.Guard_r = 0;
    EmSetEvent(&d);

    d.id = 0x2E;
    d.pos[0] = 0x2AA7;
    d.pos[1] = -0x290;
    d.pos[2] = -0xCD6;
    d.rot[1] = -0x25B0;
    d.type = 1;
    d.set = 1;
    d.flag = 0;
    d.Character = 0;
    d.hp = 0x3E8;
    d.Guard_r = 0;
    EmSetEvent(&d);
}
