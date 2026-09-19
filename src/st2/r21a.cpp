#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emdoor.h"
#include "emhit.h"
#include "emwindow.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "item.h"
#include "rnd.h"
#include "snd.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"

// Room 2-1A (D:/Bio4/Prog/r21a.cpp): the storage house. The roof supports the Ganados burn, the
// roof falling in, the locked door and its key.

// One roof support (four in the .data table): work slot, YarareInit box, effect numbers.
struct R21aRoof {
    int slot;    // 0x00  hit[] slot
    f32 x;       // 0x04  YarareInit box
    f32 Y;       // 0x08
    f32 Z;       // 0x0C
    f32 w;       // 0x10
    f32 h;       // 0x14
    int est;     // 0x18  EstSet number of the burning support
    int est2;    // 0x1C  EstSet number when it breaks
    int eff;     // 0x20  effect number (EffectEspDelete / EstSet)
};

struct R21aWork {
    cEmPatrol patrol;   // 0x000
    cEmWrap em[23];     // 0x11C
    cEmHit* hit[4];     // 0x230
    u32 se;             // 0x240  SndCall handle of the burning roof
    u32 str;            // 0x244  SndStrReq handle
    int x248;           // 0x248
    int cnt[23];        // 0x24C  [13]/[14]: frames the two roof Ganados have been dead
};

// One-member struct: every store through the work reloads the pointer.
struct R21aWorkPtr {
    R21aWork* p;
};

static R21aWorkPtr r21a_work;

R21aRoof r21a_roofTbl[4] = {
    {0, -2545.0f, -38.0f, 3534.0f, 500.0f, 0.0f, 3, 7, 5},
    {1, 2545.0f, -191.0f, 3534.0f, 500.0f, 0.0f, 1, 5, 3},
    {2, -2545.0f, -133.0f, -3534.0f, 500.0f, 0.0f, 2, 6, 4},
    {3, 2545.0f, -173.0f, -3534.0f, 500.0f, 0.0f, 0, 4, 2},
};

static Vec r21a_patrolTbl[2] = {{-38800.0f, 4452.0f, 19400.0f}, {-37540.0f, 4452.0f, 15230.0f}};

// The original passes an uninitialised int to cEmDoor::setCloseLock(int) (no r4 setup, r105 idiom).
void cEmDoorSetCloseLock(cEm* door) asm("setCloseLock__7cEmDoori");

static void r21a_movedShelf(int no);
static void r21a_moveShelf(int no);
static void R21aEmSetMain();
static void R21aDoorCheck();
static void R21aDoorMain();
static void R21aDoorEnd();
static void R21aFallRoofStartMain();
static void R21aFallRoofStartEnd();
static void R21aFallRoofDie(int no);
static void R21aFallRoofMove();
static void R21aFallRoofEndMain();
static void R21aFallRoofEndEnd();
static void SceBgmCheck();

// The position set through an inline owning the Vec: the arguments are evaluated before the stores
// (and the inline temps of consecutive calls share one frame slot: R21aFallRoofStartEnd).
static inline void SetPosXYZ(cModel* m, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setPos(&v);
}

// Set a model's rotation from three components (inline owning the Vec).
static inline void SetAngXYZ(cModel* m, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setAng(&v);
}

// Room init (the storage house): four shelf item events (items 0x84/0x85/0x88/0x89), window 0x12
// pre-broken and hidden; until the door is unlocked (Room_flg bit 0) area 5 = the door check with the
// key-use watcher, else area 3 off and the bolt object 0x19 hidden; the roof supports' hit boxes and the
// roof state per the saved flags; the Ganado waves and patrol; the battle stream.
void R21aInit()
{
    cEm* win;
    cEm* door;
    u32 i;

    R21aWork*& wp = r21a_work.p;
#line 50 "D:/Bio4/Prog/r21a.cpp"
    wp = (R21aWork*) MEM_CALLOC(sizeof(R21aWork), 1, 0xd);
    SceSetItemEvent(0xA, 0x84, 5, 1, r21a_moveShelf, (void (*)()) r21a_movedShelf, 0x84, 0);
    SceSetItemEvent(9, 0x85, 4, 2, r21a_moveShelf, (void (*)()) r21a_movedShelf, 0x85, 0);
    SceSetItemEvent(9, 0x88, 4, 2, r21a_moveShelf, (void (*)()) r21a_movedShelf, 0x88, 0);
    SceSetItemEvent(9, 0x89, 4, 2, r21a_moveShelf, (void (*)()) r21a_movedShelf, 0x89, 0);
    if (getRoomEtcWindow(0x12, &win, 1)) {
        ((cEmWindow*) win)->SetBreakModel();
        win->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) R21aDoorCheck, 0, 1);
        SceExec(0x12, (TaskFunc) R21aDoorMain, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceAtSetEnable(3, 0);
        SmdSetTrans(0x19, 0);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        cObj* obj;

        SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) R21aFallRoofStartMain, 0, 1);
        SceAtSetEnable(8, 0);
        SmdSetTrans(0x41, 0);
        obj = SmdGetObjPtr(0x3F);
        if (obj) {
            for (int k = 0; k < 4; k++) {
                R21aRoof* r = &r21a_roofTbl[k];
                cEmHit* hit = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), 0, 0, 1);

                if (hit) {
                    hit->setParent(obj, 0, 0);
                    YarareInit(hit, r->x, r->Y, r->Z, r->w, r->h, 0, 1);
                    r21a_work.p->hit[r->slot] = hit;
                }
            }
        }
    } else {
        SceAtSetEnable(6, 0);
        SceAtSetEnable(4, 0);
        SmdSetTrans(0x40, 0);
        getRoomEtcDoor(5, &door, 1);
        if (door) {
            cEmDoorSetCloseLock(door);
        }
        r21a_work.p->em[13].setPtr(0x64, -1, 0);
        r21a_work.p->em[14].setPtr(0x65, -1, 0);
        r21a_work.p->em[15].setPtr(0x66, -1, 0);
        r21a_work.p->em[16].setPtr(0x67, -1, 0);
        r21a_work.p->em[13].destroy();
        r21a_work.p->em[14].destroy();
        r21a_work.p->em[15].destroy();
        r21a_work.p->em[16].destroy();
    }
    SceExec(0x12, (TaskFunc) R21aEmSetMain, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    r21a_work.p->x248 = 0;
    for (i = 0; i < 23; i++) {
        r21a_work.p->cnt[i] = 0;
    }
}

// Per-frame room main: nothing.
void R21aMain()
{
}

// Item-event "already opened": pose the shelf of item `no` slid open (+X 500).
static void r21a_movedShelf(int no)
{
    if (no == 0x84) {
        OpenBoxMain(OpenBoxPosXP500, 1, 0xB, 0x27, -1, -1);
    }
    if (no == 0x85) {
        OpenBoxMain(OpenBoxPosXP500, 1, 0xB, 0x42, -1, -1);
    }
    if (no == 0x88) {
        OpenBoxMain(OpenBoxPosXP500, 1, 0xB, 0x42, -1, -1);
    }
    if (no == 0x89) {
        OpenBoxMain(OpenBoxPosXP500, 1, 0xB, 0x42, -1, -1);
    }
}

// Item-event opener: dust effect (0xF for the first shelf, 0x11 for the others) and the shelf slides open.
static void r21a_moveShelf(int no)
{
    if (no == 0x84) {
        EstSet(0, -1, 0, 0, 1, 0xF, 1, 0, 0, 0);
    } else {
        EstSet(0, -1, 0, 0, 1, 0x11, 1, 0, 0, 0);
    }
    if (no == 0x84) {
        OpenBoxMain(OpenBoxPosXP500, 0, 0xB, 0x27, -1, -1);
    }
    if (no == 0x85) {
        OpenBoxMain(OpenBoxPosXP500, 0, 0xB, 0x42, -1, -1);
    }
    if (no == 0x88) {
        OpenBoxMain(OpenBoxPosXP500, 0, 0xB, 0x42, -1, -1);
    }
    if (no == 0x89) {
        OpenBoxMain(OpenBoxPosXP500, 0, 0xB, 0x42, -1, -1);
    }
}

// Sets the room's Ganados: the first group at once, the reinforcements once eight of them are
// down (list flag) and the roof group once the roof event ran.
static void R21aEmSetMain()
{
    SceSleep(1);
    r21a_work.p->em[0].setPtr(0x53, -1, 0);
    r21a_work.p->em[1].setPtr(0x54, -1, 0);
    r21a_work.p->em[2].setPtr(0x55, -1, 0);
    r21a_work.p->em[3].setPtr(0x56, -1, 0);
    r21a_work.p->em[4].setPtr(0x58, -1, 0);
    r21a_work.p->em[5].setPtr(0x59, -1, 0);
    r21a_work.p->em[6].setPtr(0x5B, -1, 0);
    r21a_work.p->em[7].setPtr(0x5C, -1, 0);
    r21a_work.p->patrol.SetPatrol(0x5B, r21a_patrolTbl, 2, 0, 0);
    for (;;) {
        if (ItfFlagChk(pG, ITF_R21A_ITEM)) {
            if (RsfCheck(G_ROOM_ID, 2) == 0) {
                int cnt = 0;

                if (r21a_work.p->em[0].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[1].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[2].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[3].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[4].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[5].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[6].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[7].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[17].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[18].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[19].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[20].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[21].isActive() == 0) {
                    cnt++;
                }
                if (r21a_work.p->em[22].isActive() == 0) {
                    cnt++;
                }
                if (cnt > 8) {
                    RsfSet(G_ROOM_ID, 2);
                    r21a_work.p->em[8].setPtr(0x5E, -1, 0);
                    r21a_work.p->em[9].setPtr(0x5F, -1, 0);
                    r21a_work.p->em[10].setPtr(0x60, -1, 0);
                }
            }
        }
        if (pG->Room_flg[0] & 0x01000000) {
            if (RsfCheck(G_ROOM_ID, 3) == 0) {
                RsfSet(G_ROOM_ID, 3);
                r21a_work.p->em[17].setPtr(0x69, -1, 0);
                r21a_work.p->em[18].setPtr(0x6A, -1, 0);
                r21a_work.p->em[19].setPtr(0x6B, -1, 0);
                r21a_work.p->em[20].setPtr(0x6C, -1, 0);
                r21a_work.p->em[21].setPtr(0x6D, -1, 0);
                r21a_work.p->em[22].setPtr(0x6E, -1, 0);
            }
        }
        SceSleep(1);
    }
}

// Area 5, the locked door: up-cut 1/8; with the key (item 0x7B) held the item screen opens to use it.
static void R21aDoorCheck()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceUpCut(1, -1, 8, UP_CUT_ATTR_CUT_FIX);
        if (ItemMgr.num(0x7B) == 0) {
            CamCtrl.Comeback(0);
        } else {
            SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
        }
    }
}

// The key used on the door: the bolt slides open.
static void R21aDoorMain()
{
    cObj* obj;
    Vec pos;
    void* zero;
    int i;

    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        while (ItemMgr.check(0x7B) != 1) {
            SceSleep(1);
        }
        zero = 0;
        SceEventStart(1);
        ScfFlagOn(pG, SCF_86);
        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(5, 0);
        obj = SmdGetObjPtr(0x19);
        if (obj) {
            SndCall(6, 7, &obj->pos, 0, 0, 0);
        }
        SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        SceSetEventCancel(1, (TaskFunc) R21aDoorEnd, 0, -1, 1);
        CamCtrl.CutCall(3);
        obj = SmdGetObjPtr(0x19);
        if (obj) {
            const f32 base = -25321.0f;

            EstSet(0, -1, 0, 0, 1, 0x10, 1, 0, (u32) zero, zero);
            SndCall(6, 9, &obj->pos, 0, 0, 0);
            for (i = 0; i < 60; i++) {
                f32 y = obj->pos.y;
                f32 z = obj->pos.z;

                pos.x = base + (f32) i * 1933.0f / 60.0f;
                pos.y = y;
                pos.z = z;
                obj->setPos(&pos);
                SceSleep(1);
            }
        }
        SceSleep(10);
        SceSetEventCancel(0, 0, 0, -1, 1);
        R21aDoorEnd();
    }
}

// End of the door unlock: area 3 off, the bolt object 0x19 hidden, camera back, SceEventEnd, autosave.
static void R21aDoorEnd()
{
    SceAtSetEnable(3, 0);
    SmdSetTrans(0x19, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    GameSaveSave(&GameSave, pSaveData, -1);
}

// The lamp knocked over: the roof supports catch fire.
static void R21aFallRoofStartMain()
{
    cObj* obj = SmdGetObjPtr(0x41);

    if (pG->Room_flg[0] & 0x80000000) {
        return;
    }
    pG->Room_flg[0] |= 0x80000000;
    SceAtSetEnable(7, 0);
    r21a_work.p->se = 0;
    r21a_work.p->str = 0;
    for (int i = 0; i < 4; i++) {
        if (obj) {
            EstSet((int) obj, -1, 0, 0, 1, (u8) r21a_roofTbl[i].est, 1, (u8) r21a_roofTbl[i].eff, 0, 0);
        }
    }
    SceEventStart(0);
    SceDestroyEm(0x10, 0x20);
    pG->Room_flg[0] |= 0x00800000;
    SceSleep(2);
    SndRoomStrStop(6);
    r21a_work.p->str = SndStrReq(0, 0x20, 0x80000003, 0, 0, 0.0f);
    SceSetEventCancel(1, (TaskFunc) R21aFallRoofStartEnd, 0, -1, 1);
    CamCtrl.CutCall(4);
    EstSet(0, -1, 0, 0, 1, 0xC, 1, 0, 0, 0);
    SmdSetTrans(0x41, 1);
    if (obj) {
        SndCall(6, 5, &obj->pos, 0, 0, 0);
        for (int i = 0; i < 10; i++) {
            SetPosXYZ(obj, obj->pos.x, (f32) i * -2361.0f / 10.0f + 1503.0f, obj->pos.z);
            SceSleep(1);
        }
        SetPosXYZ(obj, obj->pos.x, -858.0f, obj->pos.z);
        SndCall(6, 6, &obj->pos, 0, 0, 0);
        for (int i = 0; i < 10; i++) {
            SetAngXYZ(obj, 0.0f, fRand1_1() * 3.1415927f / 180.0f, 0.0f);
            SceSleep(1);
        }
        SetAngXYZ(obj, 0.0f, 0.0f, 0.0f);
    }
    SceSleep(10);
    CamCtrl.CutCall(5);
    r21a_work.p->em[13].setEm(0x64, -1, 0, 1, 1);
    r21a_work.p->em[13].setNoSuspend(1);
    r21a_work.p->em[14].setEm(0x65, -1, 0, 1, 1);
    r21a_work.p->em[14].setNoSuspend(1);
    SceSleep(40);
    r21a_work.p->em[13].setNoSuspend(0);
    r21a_work.p->em[14].setNoSuspend(0);
    CamCtrl.CutCall(6);
    obj = SmdGetObjPtr(0x3F);
    if (obj) {
        EstSet(0, -1, 0, 0, 1, 0xD, 1, 0, 0, 0);
        r21a_work.p->se = SndCall(6, 0, &obj->pos, 0, 0, 0);
        for (int i = 0; i < 10; i++) {
            SetAngXYZ(obj, 0.0f, fRand1_1() * 3.1415927f / 180.0f, 0.0f);
            SceSleep(1);
        }
        SetAngXYZ(obj, 0.0f, 0.0f, 0.0f);
        SceSleep(20);
        for (int i = 0; i < 40; i++) {
            SetPosXYZ(obj, obj->pos.x, obj->pos.y - 10.0f, obj->pos.z);
            SceSleep(1);
        }
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R21aFallRoofStartEnd();
}

// End of the roof-fire start (also its cancel path): area 8 on, the fallen lamp object 0x41 shown at
// its rest pose, the roof 0x3F at its start height with the fire SE, door 5 close-locked, then the roof
// task and the Ganado group.
static void R21aFallRoofStartEnd()
{
    cObj* obj;
    cEm* door;

    SceAtSetEnable(8, 1);
    SmdSetTrans(0x41, 1);
    obj = SmdGetObjPtr(0x41);
    if (obj) {
        SetAngXYZ(obj, 0.0f, 0.0f, 0.0f);
        SetPosXYZ(obj, obj->pos.x, -858.0f, obj->pos.z);
    }
    obj = SmdGetObjPtr(0x3F);
    if (obj) {
        if (r21a_work.p->se == 0) {
            r21a_work.p->se = SndCall(6, 0, &obj->pos, 0, 0, 0);
        }
        SetAngXYZ(obj, 0.0f, 0.0f, 0.0f);
        SetPosXYZ(obj, obj->pos.x, 3095.0f, obj->pos.z);
    }
    getRoomEtcDoor(5, &door, 1);
    if (door) {
        cEmDoorSetCloseLock(door);
    }
    if (r21a_work.p->em[13].isActive() == 0) {
        r21a_work.p->em[13].setEm(0x64, -1, 0, 1, 1);
        r21a_work.p->em[13].setNoSuspend(0);
    }
    if (r21a_work.p->em[14].isActive() == 0) {
        r21a_work.p->em[14].setEm(0x65, -1, 0, 1, 1);
        r21a_work.p->em[14].setNoSuspend(0);
    }
    SceExec(0x12, (TaskFunc) R21aFallRoofMove, 0, 0, SCE_PRIO_DEF_2, 0);
    SceEventEnd(0);
    SceExit();
}

// The roof lands on the player: the death camera.
static void R21aFallRoofDie(int no)
{
    cObj* obj = SmdGetObjPtr(0x3F);

    if (obj) {
        // `spd` before the two templates: its conversion insns sit between the copies' address
        // pseudos in sched1, which decides local-alloc's r29/r28 order for &camAt and &obj->pos
        // (the gcse copy of &obj->pos then coalesces into r28 and the loop's 0.0 takes r28 as a GPR).
        f32 spd = (f32) no + 20.0f;
        Vec camPos = {-29467.0f, 3027.0f, -24785.0f};
        Vec camAt = {-31480.0f, 429.0f, -32174.0f};
        f32 fovy = 50.0f;
        SceEventStart(0);
        SetPosXYZ(obj, obj->pos.x, 1500.0f, obj->pos.z);
        SndCall(6, 0xC, &obj->pos, 0, 0, 0);
        // A real loop (rotated by expand_end_loop into `b body; sleep: ..; body: ..; bns sleep`):
        // its preheader is where loop.c/gcse put the `mr r27,r29; mr r26,r30` copies of &camAt and
        // the inline Vec, i.e. after the SndCall; the goto form hoisted them into the prologue.
        for (;;) {
            SceCamMove(&camPos, &camAt, fovy);
            spd += 10.0f;
            SetPosXYZ(obj, obj->pos.x, obj->pos.y - spd, obj->pos.z);
            if (obj->pos.y <= -1500.0f) {
                break;
            }
            SceSleep(1);
        }
        EstSet((int) obj, -1, 0, 0, 1, 8, 1, 0, 0, 0);
        SndCall(6, 1, &obj->pos, 0, 0, 0);
        for (int i = 0; i < 10; i++) {
            f32 ax;
            f32 az;

            SceCamMove(&camPos, &camAt, fovy);
            ax = fRand1_1() * 3.1415927f / 180.0f * 2.0f;
            az = fRand1_1() * 3.1415927f / 180.0f * 2.0f;
            SetAngXYZ(obj, ax, 0.0f, az);
            SceSleep(1);
        }
        SetAngXYZ(obj, 0.0f, 0.0f, 0.0f);
        for (int i = 0; i < 60; i++) {
            SceCamMove(&camPos, &camAt, fovy);
            SceSleep(1);
        }
        DiedemoExec(0, 0);
    }
}

// Scalar reference store (st_room.h idiom): the pG load that follows stays below it.
static inline void S16Set(s16& d, s16 v) { d = v; }


// The burning roof: shakes, drops in steps, then falls (onto the player if he stands under it).
static void R21aFallRoofMove()
{
    cObj* obj = SmdGetObjPtr(0x3F);
    int step;
    int cnt;
    f32 spd;
    int i;

    if (obj == 0) {
        return;
    }
    cnt = 0;   // before step/spd: its `li` then sits between the hoisted PI/180 loads (PI/180 f27/f28 tie)
    step = 0;
    spd = 0.0f;
    EstSet(0, -1, 0, 0, 1, 0xB, 1, 6, 0, 0);
    for (;;) {
        switch (step) {
        case 0:
            cnt++;
            SetAngXYZ(obj, 0.0f, fRand1_1() * 3.1415927f / 180.0f * 0.1f, 0.0f);
            if (cnt % 10 == 0) {
                EstSet((int) obj, -1, 0, 0, 1, 8, 1, 0, 0, 0);
            }
            SetPosXYZ(obj, obj->pos.x, obj->pos.y + -0.88611114f, obj->pos.z);
            if (obj->pos.y <= 1500.0f) {
                EstSet((int) obj, -1, 0, 0, 1, 8, 1, 0, 0, 0);
                spd = 0.0f;
                cnt = 0;
                step = 1;
            }
            break;
        case 1:
            cnt++;
            SetAngXYZ(obj, 0.0f, fRand1_1() * 3.1415927f / 180.0f * 0.3f, 0.0f);
            if (cnt % 5 == 0) {
                EstSet((int) obj, -1, 0, 0, 1, 8, 1, 0, 0, 0);
            }
            if (cnt > 30) {
                SetAngXYZ(obj, 0.0f, 0.0f, 0.0f);
                spd = 0.0f;
                cnt = 0;
                step = 2;
            }
            break;
        case 2:
            spd += 10.0f;
            SetPosXYZ(obj, obj->pos.x, obj->pos.y - spd, obj->pos.z);
            if (obj->pos.y <= 1400.0f) {
                cnt = 0;
                spd = 0.0f;
                step++;
            }
            break;
        case 3:
            cnt++;
            if (cnt > 10) {
                cnt = 0;
                spd = 0.0f;
                step = 4;
            }
            break;
        case 4:
            spd += 10.0f;
            SetPosXYZ(obj, obj->pos.x, obj->pos.y - spd, obj->pos.z);
            if (obj->pos.y <= 1100.0f) {
                cnt = 0;
                step++;
            }
            break;
        case 5:
            cnt++;
            if (cnt > 1) {
                cnt = 0;
                step = 6;
            }
            break;
        case 6:
            EffectEspDelete(1, 6, 0, 0);
            spd += 10.0f;
            EffectEspgenDelete(1, 6, 0);
            EffectEfmDelete(1, 6, 0);
            SetPosXYZ(obj, obj->pos.x, obj->pos.y - spd, obj->pos.z);
            if (obj->pos.y <= 0.0f) {
                SceExec(0x12, (TaskFunc) R21aFallRoofDie, (int) spd, 0, SCE_PRIO_DEF_2, 0);
                return;
            }
            break;
        }
        for (i = 0; i < 4; i++) {
            cEmHit* hit = r21a_work.p->hit[r21a_roofTbl[i].slot];

            if (hit && hit->ckStatus() == 1) {
                hit->hp = 0;
                FlagOnVar(&pGS->Room_flg, (u32) (i + 1));
                EffectEspDelete(1, (u8) r21a_roofTbl[i].eff, 0, 0);
                EffectEspgenDelete(1, (u8) r21a_roofTbl[i].eff, 0);
                EffectEfmDelete(1, (u8) r21a_roofTbl[i].eff, 0);
                if (obj) {
                    EstSet((int) obj, -1, 0, 0, 1, (u8) r21a_roofTbl[i].est2, 1, 0, 0, 0);
                    SndCall(6, 2, &hit->pos, 0, 0, 0);
                }
            }
        }
        if (FlagChkSign(pG->Room_flg, 1) && FlagChkSign(pG->Room_flg, 2) && FlagChkSign(pG->Room_flg, 3) &&
            FlagChkSign(pG->Room_flg, 4)) {
            EffectEspDelete(1, 6, 0, 0);
            EffectEspgenDelete(1, 6, 0);
            EffectEfmDelete(1, 6, 0);
            SceExec(0x12, (TaskFunc) R21aFallRoofEndMain, 0, 0, SCE_PRIO_DEF_2, 0);
            return;
        }
        if (r21a_work.p->em[13].isActive() == 0) {
            if ((pG->Room_flg[0] & 0x04000000) == 0) {
                IntSet(r21a_work.p->cnt[13], r21a_work.p->cnt[13] + 1);
                if (r21a_work.p->cnt[13] > 0x1C1) {
                    BitOn(pG->Room_flg[0], 0x04000000);
                    r21a_work.p->em[15].setEm(0x66, -1, 0, 1, 1);
                    r21a_work.p->em[15].setNoSuspend(0);
                }
            }
        }
        if (r21a_work.p->em[14].isActive() == 0) {
            if ((pG->Room_flg[0] & 0x02000000) == 0) {
                IntSet(r21a_work.p->cnt[14], r21a_work.p->cnt[14] + 1);
                if (r21a_work.p->cnt[14] > 0x1C1) {
                    BitOn(pG->Room_flg[0], 0x02000000);
                    r21a_work.p->em[16].setEm(0x67, -1, 0, 1, 1);
                    r21a_work.p->em[16].setNoSuspend(0);
                }
            }
        }
        SceSleep(1);
    }
}

// All four supports broken: the roof comes down.
static void R21aFallRoofEndMain()
{
    cObj* obj;
    Vec v;

    if (RsfCheck(G_ROOM_ID, 1)) {
        return;
    }
    RsfSet(G_ROOM_ID, 1);
    SceEventStart(0);
    SndStop(r21a_work.p->se, 0);
    SceSetEventCancel(1, (TaskFunc) R21aFallRoofEndEnd, 0, -1, 1);
    CamCtrl.CutCall(6);
    obj = SmdGetObjPtr(0x3F);
    if (obj) {
        SndCall(6, 1, &obj->pos, 0, 0, 0);
        for (int i = 0; i < 10; i++) {
            v.y = fRand1_1() * 3.1415927f / 180.0f;
            v.x = 0.0f;
            v.z = 0.0f;
            obj->setAng(&v);
            SceSleep(1);
        }
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        obj->setAng(&v);
    }
    if (r21a_work.p->str) {
        SndStrReq(r21a_work.p->str, 4, 1000, 0);
    }
    r21a_work.p->str = 0;
    SceSleep(30);
    CamCtrl.CutCall(7);
    obj = SmdGetObjPtr(0x40);
    if (obj) {
        const f32 base = -31648.0f;

        EstSet(0, -1, 0, 0, 1, 0xE, 1, 0, 0, 0);
        SndCall(6, 0xA, &obj->pos, 0, 0, 0);
        for (int i = 0; i < 60; i++) {
            f32 y = obj->pos.y;
            f32 z = obj->pos.z;

            v.x = base + (f32) i * 1882.0f / 60.0f;
            v.y = y;
            v.z = z;
            obj->setPos(&v);
            SceSleep(1);
        }
    }
    SceSleep(10);
    SceSetEventCancel(0, 0, 0, -1, 1);
    R21aFallRoofEndEnd();
}

// End of the roof collapse: the roof object levelled, its stream stopped, areas 6/4 off, the burning
// roof object 0x40 hidden, SceEventEnd, task exit.
static void R21aFallRoofEndEnd()
{
    cObj* obj = SmdGetObjPtr(0x3F);
    Vec v;

    if (obj) {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        obj->setAng(&v);
    }
    if (r21a_work.p->str) {
        SndStrReq(r21a_work.p->str, 8, 0, 0);
    }
    SceAtSetEnable(6, 0);
    SceAtSetEnable(4, 0);
    SmdSetTrans(0x40, 0);
    SceEventEnd(0);
    SceExit();
}

// Battle stream while a Ganado has found the player; ends once Room_flg[0] 0x00800000 (the roof fell).
static void SceBgmCheck()
{
    int playing = 0;

    for (;;) {
        if ((pG->Room_flg[0] & 0x00800000) == 0) {
            if (SceCkFindPL(NULL) == 1) {
                if (playing == 0) {
                    SndRoomStrStart(1, 0, 1);
                    playing = 1;
                }
            } else if (playing == 1) {
                SndRoomStrStop(3);
                playing = 0;
            }
            SceSleep(1);
        } else {
            break;
        }
    }
}
