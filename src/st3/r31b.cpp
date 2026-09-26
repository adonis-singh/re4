#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "em32.h"
#include "emhit.h"
#include "emdoor.h"
#include "eprintf.h"
#include "emswitch.h"
#include "emBarred.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"
#include "st_mgr_event.h"
#include "cockpit.h"
#include "mes.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "motion.h"
#include "math_sub.h"
#include "rnd.h"
#include "game.h"
#include "room_data.h"

// Room 3-1b (D:/Bio4/Prog/r31b.cpp): the U-3 ("It") cage corridor: three rooms with shutter
// pairs opened by switch pairs, a death timer, the cages that fall, and the gondola.
//
// STATUS: byte-identical. The three levers that closed it: the `pPL` struct-member view for the
// setPos after a pair of `Vec = {..}` template copies (R31bMain, R31bExecEventS00), the dead
// `zero = em` second set that keeps `zero` out of the cse2 class merge (R31bExecRoom03U3Main), and
// the single-use `hp` constant that fills one sched1 slot in block 0 (R31bInit).
//
struct R31bWork {
    cSat* sat[17];          // 0x000  scenario collision pieces per shutter/door object
    cSat* eat[17];          // 0x044  enemy collision pieces ([13..16] the four EatMgr planes)
    Vec satPos[17];         // 0x088  the pieces' positions (the fall puts them back)
    cEmWrap em;             // 0x154  U-3 (list 0x14)
    cEmHit* koushi[7];      // 0x160  the lattice switch hit enemies
    Vec savePos;            // 0x17C  the room-1 shutter position while U-3 lifts it
    int switchCount;        // 0x188  switches pressed in the current room
    u32 snd;                // 0x18C  running shutter SndCall handle
    u32 str;                // 0x190  start-camera stream handle
    cObj* smd;              // 0x194  the door-front dummy model
};

// The work pointer is a struct member: every store through the work reloads it (r203).
static R31bWork* r31b_work;
// Global in the original (.sym scope:global): the REL relocation carries the symbol, the ADDR16 field is 0.
cParts* r31b_plParts;   // .bss 0x18  player parts 10 (R31bMain)

// The player after the fall; the room's scroll objects ([no] = the cage room, the count in
// r31b_objNum); the lattice (kanaami) objects, 25 per room; the room-3 lattice pair lists; the
// gondola positions ([0] = start, [1] = stop, per direction: one 2x2 array, `r31b_gondolaPos[1]` is
// addressed as the start table + 0x18) and the player's yaw on it. The non-static ones are global in
// the original.
Vec r31b_fallPlPos[3] = {{-10850.0f, 0.0f, 1000.0f}, {8150.0f, 0.0f, 1000.0f}, {26650.0f, 0.0f, 1000.0f}};
int r31b_objNum[4] = {12, 19, 31, 0};
int r31b_objTbl[4][31] = {
    {17, 18, 19, 20, 21, 182, 183, 184, 185, 186, 187, 213},
    {47, 48, 49, 50, 51, 158, 159, 160, 161, 192, 193, 194, 195, 196, 197, 198, 199, 200, 214},
    {77, 78, 79, 80, 81, 146, 147, 148, 148, 148, 149, 150, 151, 152, 153, 156, 157, 203, 204, 205, 206, 211, 233,
     215, 225, 237, 241, 246, 247, 248, 249},
    {0},
};
static int r31b_kanaamiTbl[4][25] = {
    {26, 25, 24, 23, 22, 31, 30, 29, 28, 27, 36, 35, 34, 33, 32, 41, 40, 39, 38, 37, 46, 45, 44, 43, 42},
    {56, 55, 54, 53, 52, 61, 60, 59, 58, 57, 66, 65, 64, 63, 62, 71, 70, 69, 68, 67, 76, 75, 74, 73, 72},
    {86, 85, 84, 83, 82, 91, 90, 89, 88, 87, 96, 95, 94, 93, 92, 101, 100, 99, 98, 97, 106, 105, 104, 103, 102},
    {0},
};
static int r31b_kanaami3a[6] = {203, 204, 205, 206, 211, 233};
static int r31b_kanaami3b[6] = {237, 241, 246, 247, 248, 249};
Vec r31b_gondolaPos[2][2] = {
    {{80250.0f, 10547.0f, -21375.0f}, {-8875.0f, 10547.0f, -21375.0f}},
    {{-8875.0f, 10547.0f, -21375.0f}, {80250.0f, 10547.0f, -21375.0f}},
};
f32 r31b_gondolaAng[3] = {4.712389f, 1.5707964f, 0.0f};

static void R31bExecEventS00();
static void R31bStartCameraMain();
static void R31bStartCameraCancel();
void R31bStartCameraEnd();
static void R31bExecSwitchMain(int no);
void R31bExecSwitchMainSub(int no, int flagNo, int count, int atNo, int cut);
static void R31bExecSwitchEnd(int no);
void R31bExecSwitchEndSub(int no, int room, int flagNo, int count, int emMode, int light, int esp, int smdOff);
static void R31bExecShutterOpenMain(int no);
void R31bExecShutterOpenMainSub(int no, int flagNo, u32 objId, int satNo, u32 lampId, int koushiNo, int kind, int cut, int atNo);
static void R31bExecShutterOpenEnd(int no);
void R31bExecShutterOpenEndSub(int no, int atNo, u32 objId, int satNo);
static void R31bExecDeathTimerMain(int no);
void R31bExecDeathTimerMainSub(int no, int light, int frames);
static void R31bExecDoorMain(int no);
void R31bExecDoorMainSub(int no, int flagOpen, int flagDoor, int doorFlag, int atNo, int cut, int cut2, int light, u32 objId0, u32 objId1, int est);
static void R31bExecDoorEnd(int no);
void R31bExecDoorEndSub(int no, int emMode, int light, u32 objId0, u32 objId1, int satNo);
static void R31bExecFallMain(int no);
void R31bExecFallMainSub(int no, int flagNo, int cut);
void R31bExecFallRoom(int no, f32 spd);
static void R31bExecFallEnd(int no);
void R31bExecFallEndSub(int no, u32 objId, int satNo, int flagNo);
void R31bSmdTransOff(int no);
static void R31bExecEscapeMain();
static void R31bExecEscapeEnd();
static void R31bExecRoom01U3Main();
static void R31bExecRoom01U3End();
static void R31bExecRoom02U3Main();
static void R31bExecRoom02U3End();
static void R31bExecRoom03U3Main();
static void R31bExecRoom03U3End();
static void R31bExecRoom03U3DieMain();
void R31bExecRoom03U3DieEnd();
static void R31bExecGondolaMain(int dir);
static void R31bExecGondolaEnd(int dir);
void R31bDoorSat(int no);
void R31bDoorSatSub(int no, u32 objId, int satNo);
void R31bKoushiSat(int no);
void R31bKoushiSatSub1(int no, u32 objId, int satNo, int type);
void R31bKoushiSatSub2(int no, u32 lampId, int koushiNo);
void R31bKoushiSatCk2(int no, int flagNo, int koushiNo);
static void R31bEmSetMain();
void R31bLightAllOn();
void R31bLight(int no);
void R31bKanaamiTrans(u8 room, u8 no, int on);
void R31bKanaamiRoom03Trans(int no, int on);
extern "C" void Evt_R31BS00_Func(Event* e);

// Room init (the U-3 cage corridor): the three cage rooms in turn — each not yet passed (Room_flg bits
// 2/5/8) gets its two lattice switch areas, its exit door area and its collision / lattice objects,
// else it is hidden as fallen (R31bSmdTransOff); room 3 also the escape (area 0x22) and U-3's
// appearance (area 0x23, bit 0xB); the gondola areas 0xF/0x10 posed by bit 0xD; the s00 event on area 4
// until bit 0xC; the start camera, the lights and the U-3 handle.
void R31bInit()
{
    cEm* sw0;
    cEm* sw1;
    cEm* barred;
    cEmDoor* door;
    cEmDoor* door2;
    cObj* obj;
    cEmHit* hit;
    int i;
    int no = 3;
    // A second constant local of block 0 (used once, as the stored hp value below): its `li` is a
    // codeless sched1 filler -- cse/cprop cannot fold a constant into a store, update_equiv_regs moves
    // the single-use init next to the store, so the bytes are the literal's -- and it takes one
    // free slot ahead of the gcse-hoisted `addi rX,r1,N` address pseudos, which puts each of them
    // in the target's call segment (`addi r26` after the memset argument moves, `addi r25` after
    // the first memset). Post-reload an `addi rN,r1,N` cannot cross a call, so sched2 keeps them there.
    int hp = 0;

    // A local for the allocation result: assigned to r31b_work directly, it feeds the same
    // slot-order tie the hp local above resolves (see its comment).
#line 279 "D:/Bio4/Prog/r31b.cpp"
    R31bWork* w = (R31bWork*) MEM_CALLOC(sizeof(R31bWork), 1, 0xd);
    r31b_work = w;
    Espgen42SetNoWater(1);
    R31bLightAllOn();
    // Frame order pos0, rot0, pos, rot (0x10..0x40): the Vecs are declared here, after the calls.
    Vec pos0 = {0, 0, 0};
    Vec rot0 = {0, 0, 0};

    for (i = 0; i < 17; i++) {
        r31b_work->sat[i] = 0;
        r31b_work->eat[i] = 0;
    }
    r31b_work->eat[13] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos0, &rot0, 4);
    r31b_work->eat[14] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos0, &rot0, 3);
    r31b_work->eat[15] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos0, &rot0, 2);
    r31b_work->eat[16] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos0, &rot0, 1);
    getRoomEtcSwitch(5, &sw0, 1);
    getRoomEtcSwitch(7, &sw1, 1);
    getRoomEtcBarred(9, &barred, 1);
    if (sw0 && sw1 && barred) {
        ((cEmSwitch*) sw0)->setBarred((cEmBarred*) barred);
        ((cEmSwitch*) sw0)->setConnectSwitch((cEmSwitch*) sw1);
        ((cEmSwitch*) sw1)->setBarred((cEmBarred*) barred);
        ((cEmSwitch*) sw1)->setConnectSwitch((cEmSwitch*) sw0);
        ((cEmSwitch*) sw0)->setClosed();
        ((cEmSwitch*) sw1)->setClosed();
        ((cEmBarred*) barred)->setClosed();
    }
    getRoomEtcSwitch(6, &sw0, 1);
    getRoomEtcSwitch(8, &sw1, 1);
    getRoomEtcBarred(0xA, &barred, 1);
    if (sw0 && sw1 && barred) {
        ((cEmSwitch*) sw0)->setBarred((cEmBarred*) barred);
        ((cEmSwitch*) sw0)->setConnectSwitch((cEmSwitch*) sw1);
        ((cEmSwitch*) sw1)->setBarred((cEmBarred*) barred);
        ((cEmSwitch*) sw1)->setConnectSwitch((cEmSwitch*) sw0);
        ((cEmSwitch*) sw0)->setClosed();
        ((cEmSwitch*) sw1)->setClosed();
        ((cEmBarred*) barred)->setClosed();
    }
    EvtMgr.SetFunc("evt_r31bs00_func", (void*) Evt_R31BS00_Func);
    R31bDoorSat(0);
    R31bDoorSat(1);
    R31bDoorSat(2);
    R31bDoorSat(3);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        R31bKoushiSat(0);
        SceAtDataSet_exec(0x11, 0x12, 0, (TaskFunc) R31bExecSwitchMain, (void*) 0, 1);
        SceAtDataSet_exec(0x12, 0x12, 0, (TaskFunc) R31bExecSwitchMain, (void*) 1, 1);
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) R31bExecDoorMain, (void*) 0, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, ESP_CORE_KIND_ROOM02, 0, 0);
    } else {
        R31bLight(3);
        R31bSmdTransOff(0);
    }
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        R31bKoushiSat(1);
        SceAtDataSet_exec(0x15, 0x12, 0, (TaskFunc) R31bExecSwitchMain, (void*) 2, 1);
        SceAtDataSet_exec(0x16, 0x12, 0, (TaskFunc) R31bExecSwitchMain, (void*) 3, 1);
        SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) R31bExecDoorMain, (void*) 1, 1);
        obj = SmdGetObjPtr(0xC1);
        if (obj) {
            obj->setPos(obj->pos.x, 3000.0f, obj->pos.z);
            if (r31b_work->sat[8]) {
                r31b_work->sat[8]->setCoord(&obj->pos, &obj->ang);
            }
            if (r31b_work->eat[8]) {
                r31b_work->eat[8]->setCoord(&obj->pos, &obj->ang);
            }
        }
        LightMgr.offKind(0x13);
        if (r31b_work->koushi[no]) {
            r31b_work->koushi[no]->hp = hp;
        }
        SceAtSetEnable(0x18, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 7, 1, ESP_CORE_KIND_ROOM03, 0, 0);
    } else {
        R31bLight(5);
        R31bSmdTransOff(1);
    }
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        R31bKoushiSat(2);
        SceAtDataSet_exec(0x1C, 0x12, 0, (TaskFunc) R31bExecSwitchMain, (void*) 4, 1);
        SceAtDataSet_exec(0x1D, 0x12, 0, (TaskFunc) R31bExecSwitchMain, (void*) 5, 1);
        SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) R31bExecDoorMain, (void*) 2, 1);
        SceAtDataSet_exec(0x22, 0x12, 0, (TaskFunc) R31bExecEscapeMain, (void*) 0, 1);
        obj = SmdGetObjPtr(0x93);
        if (obj) {
            obj->setPos(obj->pos.x, 125.0f, obj->pos.z);
            if (r31b_work->sat[11]) {
                r31b_work->sat[11]->setCoord(&obj->pos, &obj->ang);
            }
            if (r31b_work->eat[11]) {
                r31b_work->eat[11]->setCoord(&obj->pos, &obj->ang);
            }
        }
        Vec pos = {0, 0, 0};
        Vec rot = {0, 0, 0};

        r31b_work->smd = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28), &pos, &rot, 0x10, 1);
        if (r31b_work->smd) {
            cObj* smd = r31b_work->smd;

            smd->be_flag |= 0x1000;
            MotionSetCore(smd, &smd->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2A), 0, 0, 5, 0);
        }
        for (i = 0; i < 6; i++) {
            R31bKanaamiRoom03Trans(i, 1);
        }
        EstSet(0, -1, 0, 0, EFF_ROOM, 8, 1, ESP_CORE_KIND_ROOM04, 0, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 9, 1, ESP_CORE_KIND_ROOM05, 0, 0);
    } else {
        R31bSmdTransOff(2);
        R31bLightAllOn();
    }
    if (RsfCheck(G_ROOM_ID, 0xB) == 0) {
        R31bKoushiSat(3);
        SceAtDataSet_exec(0x23, 0x12, 0, (TaskFunc) R31bExecRoom03U3Main, 0, 1);
        getRoomEtcDoor(4, &door, 1);
        if (door) {
            door->setCloseLock();
        }
    } else {
        getRoomEtcDoor(4, &door2, 1);
        if (door2) {
            door2->setNormal();
        }
        SceAtSetEnable(0x24, 0);
    }
    obj = SmdGetObjPtr(0xEA);
    if (obj) {
        obj->setPos(obj->pos.x, 7313.0f, obj->pos.z);
        if (r31b_work->sat[12]) {
            r31b_work->sat[12]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->eat[12]) {
            r31b_work->eat[12]->setCoord(&obj->pos, &obj->ang);
        }
    }
    KyfFlagOn(pG, KYF_ST1_16);
    SceAtDataSet_exec(0xF, 0x12, 0, (TaskFunc) R31bExecGondolaMain, (void*) 0, 1);
    SceAtDataSet_exec(0x10, 0x12, 0, (TaskFunc) R31bExecGondolaMain, (void*) 1, 1);
    obj = SmdGetObjPtr(0xA3);
    if (obj) {
        int side;

        if (RsfCheck(G_ROOM_ID, 0xD)) {
            SceAtSetEnable(0xF, 0);
            SceAtSetEnable(0x10, 1);
            if (r31b_work->sat[3]) {
                r31b_work->sat[3]->setEnable();
            }
            if (r31b_work->sat[4]) {
                r31b_work->sat[4]->setDisable();
            }
            side = 1;
        } else {
            SceAtSetEnable(0xF, 1);
            SceAtSetEnable(0x10, 0);
            if (r31b_work->sat[3]) {
                r31b_work->sat[3]->setDisable();
            }
            if (r31b_work->sat[4]) {
                r31b_work->sat[4]->setEnable();
            }
            side = 0;
        }
        obj->setPos(r31b_gondolaPos[0][side].x, r31b_gondolaPos[0][side].y, r31b_gondolaPos[0][side].z);
        if (r31b_work->eat[16]) {
            r31b_work->eat[16]->setCoord(&obj->pos, &obj->ang);
        }
    }
    if (RsfCheck(G_ROOM_ID, 0xC) == 0) {
        SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) R31bExecEventS00, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r31bs00.evd", (u8) GetEmIdFromList(0x14), 0, 0, 0);
    } else {
        SceExec(0x12, (TaskFunc) R31bEmSetMain, 0, 0, 2, 0);
    }
    void* zero = 0;

    if ((pG->Room_flg[0] & 0x10) == 0) {
        SceAtDataSet_exec(0x25, 0x12, 0, (TaskFunc) R31bStartCameraMain, 0, 1);
    }
    r31b_work->switchCount = 0;
    EstSet(0, -1, 0, 0, EFF_ROOM, 3, 0x2001, ESP_CORE_KIND_ROOM01, zero, zero);
    r31b_work->str = 0;
    Vec pos;
    Vec rot;
    pos.x = 92385.0f;
    pos.y = 9383.0f;
    pos.z = -8372.0f;
    rot.x = 0.0f;
    rot.y = 1.57f;
    rot.z = 0.0f;
    hit = SetEmHit(ROOM_ARC_PTR(pG->pRoom, 0x2E), ROOM_ARC_PTR(pG->pRoom, 0x2F), &pos, &rot, 2);
    if (hit) {
        hit->setBeetle(ROOM_ARC_PTR(pG->pRoom, 0x30), ROOM_ARC_PTR(pG->pRoom, 0x32), ROOM_ARC_PTR(pG->pRoom, 0x31));
    }
}

// Per frame (debug): trigger 1 before U-3 is out (Room_flg bit 0xC) spawns it (0x14, cEm32) in phase 2
// at a fixed spot; the count-down check while the timer runs.
void R31bMain()
{
    if (DebugTrg(1) && RsfCheck(G_ROOM_ID, 0xC) == 0) {
        cEm32* em;

        r31b_work->em.setEm(0x14, -1, 0, 1, 1);
        em = (cEm32*) r31b_work->em.getPtr();
        if (em) {
            em->setNext(2);
        }
        {
            Vec pos = {27530.0f, 0.0f, 760.0f};
            Vec ang = {0.0f, 1.32f, 0.0f};

            pPL->setPos(&pos);
            pPL->setAng(&ang);
        }
        R31bLightAllOn();
        LightMgr.update(0x21, -1);
        StaFlagOn(pG, STA_LIT_NO_UPDATE);
    }
    r31b_plParts = pPL->getPartsPtr(0xA);
    if (RsfCheck(G_ROOM_ID, 0xB) == 0 && RsfCheck(G_ROOM_ID, 0xC)) {
        cEm32* em;

        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            R31bKoushiSatCk2(0, 0x10, 0);
            R31bKoushiSatCk2(1, 0x11, 1);
        }
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            R31bKoushiSatCk2(2, 0x14, 2);
            R31bKoushiSatCk2(3, 0x15, 3);
            R31bKoushiSatCk2(4, 0x16, 4);
        }
        if (RsfCheck(G_ROOM_ID, 8) == 0) {
            R31bKoushiSatCk2(5, 0x1A, 5);
        }
        em = (cEm32*) r31b_work->em.getPtr();
        if (em) {
            u32 no = em->getBreakNo();

            if (no != 0xFF) {
                R31bKanaamiTrans((u8) (no / 25), (u8) (no % 25), 0);
            }
            int no2 = em->getBreakNo2();

            if (no2 != 0xFF) {
                R31bKanaamiRoom03Trans(no2, 0);
            }
            if (RsfCheck(G_ROOM_ID, 0x17) == 0 && (pG->Room_flg[2] & 0x80000000)) {
                SceExec(0x12, (TaskFunc) R31bExecRoom01U3Main, 0, 0, 2, 0);
            }
            if (RsfCheck(G_ROOM_ID, 0xB) == 0 && RsfCheck(G_ROOM_ID, 0x1C) && em->hp <= 0) {
                SceExec(0x12, (TaskFunc) R31bExecRoom03U3DieMain, 0, 0, 2, 0);
            }
        }
    }
}

// The s00 event: U-3 breaks in.
static void R31bExecEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0xC) == 0) {
        cEm* em;

        RsfSet(G_ROOM_ID, 0xC);
        SceAtSetEnable(4, 0);
        SceEventStart(0);
        EvtMgr.EvtReadExec("event/evd/r31bs00.evd", (u8) GetEmIdFromList(0x14), EvtReadFlagNone);
        R31bLight(1);
        {
            Vec pos = {-21730.0f, 0.0f, 3800.0f};
            Vec ang = {0.0f, 2.81f, 0.0f};

            pPL->setPos(&pos);
            pPL->setAng(&ang);
        }
        SceEventEnd(0);
        ScfFlagOn(pG, SCF_R31B_U3);
        GamePointBossReset();
        r31b_work->em.setEm(0x14, -1, 0, 1, 1);
        em = r31b_work->em.getPtr();
        if (em) {
            // The boss pointer the life meter shows: stored at Cckpt+0.
            *(cEm**) &Cckpt = em;
        }
        EstSet(0, -1, 0, 0, EFF_ROOM, 3, 0x2001, ESP_CORE_KIND_ROOM01, 0, 0);
    }
}

// The start camera (the room overview) with its stream.
static void R31bStartCameraMain()
{
    if (RsfCheck(G_ROOM_ID, 0x1B) == 0) {
        RsfSet(G_ROOM_ID, 0x1B);
        SceAtSetEnable(0x25, 0);
        SceEventStart(1);
        SceSetEventCancel(1, (TaskFunc) R31bStartCameraCancel, 0, -1, 1);
        r31b_work->str = SndStrReq(0, 0x2E, 0x80000003, 0, 0, 0.0f);
        CamCtrl.CutCall(0x28);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R31bStartCameraEnd();
    }
}

// Cancel path of the start camera: fade reset, its stream stopped, a 10-frame fade-in, then the common end.
static void R31bStartCameraCancel()
{
    FadeSetW(0, 0, 0, 0);
    if (r31b_work->str) {
        SndStrStopBlock(r31b_work->str);
        r31b_work->str = 0;
    }
    FadeSetW(0x80000000, 10, 0, 0);
    R31bStartCameraEnd();
}

// End of the start camera: camera back, SceEventEnd.
void R31bStartCameraEnd()
{
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Lattice switch `no` (0..5): dispatch to R31bExecSwitchMainSub with the switch's flag, its area and camera cut.
static void R31bExecSwitchMain(int no)
{
    if (no == 0) {
        R31bExecSwitchMainSub(0, 0xE, 2, 0x11, 2);
    }
    if (no == 1) {
        R31bExecSwitchMainSub(1, 0xF, 2, 0x12, 0x11);
    }
    if (no == 2) {
        R31bExecSwitchMainSub(2, 0x12, 2, 0x15, 0x19);
    }
    if (no == 3) {
        R31bExecSwitchMainSub(3, 0x13, 2, 0x16, 0x18);
    }
    if (no == 4) {
        R31bExecSwitchMainSub(4, 0x18, 2, 0x1C, 0x1B);
    }
    if (no == 5) {
        R31bExecSwitchMainSub(5, 0x19, 2, 0x1D, 0x1D);
    }
}

// A lattice switch pressed: the second one of a pair opens the shutter.
void R31bExecSwitchMainSub(int no, int flagNo, int count, int atNo, int cut)
{
    if (RsfCheck(G_ROOM_ID, flagNo) == 0) {
        RsfSet(G_ROOM_ID, flagNo);
        SceAtSetEnable(atNo, 0);
        r31b_work->switchCount++;
        SceEventStart(1);
        SceSetEventCancel(1, (TaskFunc) R31bExecSwitchEnd, no, -1, 1);
        r31b_work->snd = 0;
        if (cut != -1) {
            CamCtrl.CutCall((s8) cut);
        }
        SndCall(6, 3, &pPL->pos, 0, 0, 0);
        if (r31b_work->switchCount >= count) {
            SceMesSet(1, 0x20, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
            SndCall(6, 2, &pPL->pos, 0, 0, 0);
            SceMesSet(6, 0x20, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        } else {
            SceMesSet(0, 0x20, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        }
        if (no == 4) {
            cObj* obj;

            CamCtrl.CutCall(0xF);
            obj = SmdGetObjPtr(0x93);
            if (obj) {
                int i;

                r31b_work->snd = SndCall(6, 4, &obj->pos, 0, 0, 0);
                for (i = 0; i < 30; i++) {
                    obj->setPos(obj->pos.x, (f32) i * 2875.0f / 30.0f + 125.0f, obj->pos.z);
                    SceSleep(1);
                }
                SndCall(6, 5, &obj->pos, 0, 0, 0);
                r31b_work->snd = 0;
            }
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R31bExecSwitchEnd(no);
    }
}

// End of lattice switch `no`: dispatch to R31bExecSwitchEndSub with the room, flag, U-3 mode, light set and effect.
static void R31bExecSwitchEnd(int no)
{
    if (no == 0) {
        R31bExecSwitchEndSub(0, 0, 0, 2, 3, 2, 0xD, 4);
    }
    if (no == 1) {
        R31bExecSwitchEndSub(1, 0, 0, 2, 3, 2, 0xC, 4);
    }
    if (no == 2) {
        R31bExecSwitchEndSub(2, 1, 3, 2, 3, 4, 0x10, 5);
    }
    if (no == 3) {
        R31bExecSwitchEndSub(3, 1, 3, 2, 3, 4, 0x11, 5);
    }
    if (no == 4) {
        R31bExecSwitchEndSub(4, 2, 6, 2, 3, 6, 0x1A, 6);
    }
    if (no == 5) {
        R31bExecSwitchEndSub(5, 2, 6, 2, 3, 6, 0x15, 6);
    }
}

// End of a switch press: the switch light off; when `count` switches of the pair are down the shutter
// flag is set, the light set changed, the switch effect dropped and the shutter opens (U-3 gets
// `emMode`); camera back, SceEventEnd.
void R31bExecSwitchEndSub(int no, int room, int flagNo, int count, int emMode, int light, int esp, int smdOff)
{
    cEm32* em;
    int opened = 0;

    LightMgr.offKind((u8) esp);
    if (r31b_work->switchCount >= count) {
        opened = 1;
        r31b_work->switchCount = 0;
        RsfSet(G_ROOM_ID, flagNo);
        R31bLight(light);
        EffectEspDelete(1, (u8) smdOff, 0, 0);
        EffectEspgenDelete(1, (u8) smdOff, 0);
        EffectEfmDelete(1, (u8) smdOff, 0);
        SceExec(0x12, (TaskFunc) R31bExecDeathTimerMain, room, 0, 2, 0);
    }
    em = (cEm32*) r31b_work->em.getPtr();
    if (em) {
        em->setNext(emMode);
    }
    if (no == 4) {
        cObj* obj = SmdGetObjPtr(0x93);

        if (obj) {
            obj->setPos(obj->pos.x, 3000.0f, obj->pos.z);
            if (r31b_work->sat[11]) {
                r31b_work->sat[11]->setCoord(&obj->pos, &obj->ang);
            }
            if (r31b_work->eat[11]) {
                r31b_work->eat[11]->setCoord(&obj->pos, &obj->ang);
            }
            if (r31b_work->snd) {
                SndCall(6, 5, &obj->pos, 0, 0, 0);
                r31b_work->snd = 0;
            }
        }
        SceExec(0x12, (TaskFunc) R31bExecRoom02U3Main, 0, 0, 2, 0);
    }
    if (room == 2 && opened != 0) {
        R31bLightAllOn();
        LightMgr.update(0x21, -1);
        StaFlagOn(pG, STA_LIT_NO_UPDATE);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Shutter `no` (0..5): dispatch to R31bExecShutterOpenMainSub with its flag, object, collision slot,
// lamp, lattice, light kind, cut and area.
static void R31bExecShutterOpenMain(int no)
{
    if (no == 0) {
        R31bExecShutterOpenMainSub(0, 0x10, 0xBA, 5, 0xB8, 0, 0xE, 0x12, 0x13);
    }
    if (no == 1) {
        R31bExecShutterOpenMainSub(1, 0x11, 0xBB, 6, 0xB9, 1, 0xF, 0x13, 0x14);
    }
    if (no == 2) {
        R31bExecShutterOpenMainSub(2, 0x14, 0xC0, 7, 0xC3, 2, 0x12, 0x17, 0x17);
    }
    if (no == 3) {
        R31bExecShutterOpenMainSub(3, 0x15, 0xC1, 8, 0xC5, 3, 0x13, 0x15, 0x18);
    }
    if (no == 4) {
        R31bExecShutterOpenMainSub(4, 0x16, 0xC6, 9, 0xC8, 4, 0x14, 0x16, 0x19);
    }
    if (no == 5) {
        R31bExecShutterOpenMainSub(5, 0x1A, 0x97, 0xA, 0x98, 5, 0x16, 0x1C, 0x1E);
    }
}

// A shutter opens: the lamp turns, the collision pieces follow the shutter up.
void R31bExecShutterOpenMainSub(int no, int flagNo, u32 objId, int satNo, u32 lampId, int koushiNo, int kind, int cut, int atNo)
{
    cObj* obj;

    RsfSet(G_ROOM_ID, 0x1D);
    if (RsfCheck(G_ROOM_ID, flagNo) == 0) {
        RsfSet(G_ROOM_ID, flagNo);
        LightMgr.offKind((u8) kind);
        SmdSetTrans(lampId, 0);
        SceAtSetEnable(atNo, 0);
        obj = SmdGetObjPtr(lampId);
        if (obj) {
            Vec rot = obj->ang;

            rot.y += 3.1415927f;
            rot.y = LIMIT_ANGLE(rot.y);
            EstSet(0, -1, &obj->pos, &rot, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        }
        r31b_work->snd = 0;
        if (r31b_work->koushi[koushiNo]) {
            r31b_work->koushi[koushiNo]->hp = 0;
        }
        if (RsfCheck(G_ROOM_ID, 0x1D) == 0) {
            SceEventStart(1);
            SceSetEventCancel(1, (TaskFunc) R31bExecShutterOpenEnd, no, -1, 1);
        }
        if (RsfCheck(G_ROOM_ID, 0x1D) == 0 && cut != -1) {
            CamCtrl.CutCall((s8) cut);
        }
        obj = SmdGetObjPtr(objId);
        if (obj) {
            int i;

            r31b_work->snd = SndCall(6, 4, &obj->pos, 0, 0, 0);
            for (i = 0; i < 30; i++) {
                obj->setPos(obj->pos.x, (f32) i * 2875.0f / 30.0f + 125.0f, obj->pos.z);
                if (r31b_work->sat[satNo]) {
                    r31b_work->sat[satNo]->setCoord(&obj->pos, &obj->ang);
                }
                if (r31b_work->eat[satNo]) {
                    r31b_work->eat[satNo]->setCoord(&obj->pos, &obj->ang);
                }
                SceSleep(1);
            }
            SndCall(6, 5, &obj->pos, 0, 0, 0);
            r31b_work->snd = 0;
        }
        if (RsfCheck(G_ROOM_ID, 0x1D) == 0) {
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
        }
        if (RsfCheck(G_ROOM_ID, 0x1D) == 0) {
            SceSetEventCancel(0, 0, 0, -1, 1);
        }
        R31bExecShutterOpenEnd(no);
    }
}

// End of shutter `no`: dispatch to R31bExecShutterOpenEndSub.
static void R31bExecShutterOpenEnd(int no)
{
    if (no == 0) {
        R31bExecShutterOpenEndSub(0, 0x13, 0xBA, 5);
    }
    if (no == 1) {
        R31bExecShutterOpenEndSub(1, 0x14, 0xBB, 6);
    }
    if (no == 2) {
        R31bExecShutterOpenEndSub(2, 0x17, 0xC0, 7);
    }
    if (no == 3) {
        R31bExecShutterOpenEndSub(3, 0x18, 0xC1, 8);
    }
    if (no == 4) {
        R31bExecShutterOpenEndSub(4, 0x19, 0xC6, 9);
    }
    if (no == 5) {
        R31bExecShutterOpenEndSub(5, 0x1E, 0x97, 0xA);
    }
}

// End of a shutter opening (also its cancel path): the shutter snapped to y 3000 with its collision
// pieces following, SE 5, the passage area on, camera back, SceEventEnd.
void R31bExecShutterOpenEndSub(int no, int atNo, u32 objId, int satNo)
{
    cObj* obj = SmdGetObjPtr(objId);

    if (obj) {
        obj->setPos(obj->pos.x, 3000.0f, obj->pos.z);
        if (r31b_work->sat[satNo]) {
            r31b_work->sat[satNo]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->eat[satNo]) {
            r31b_work->eat[satNo]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->snd) {
            SndCall(6, 5, &obj->pos, 0, 0, 0);
            r31b_work->snd = 0;
        }
    }
    SceAtSetEnable(atNo, 0);
    if (RsfCheck(G_ROOM_ID, 0x1D) == 0) {
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        SceExit();
    }
}

// The death timer of cage room `no` (0..2): 900 frames with the room's light set.
static void R31bExecDeathTimerMain(int no)
{
    if (no == 0) {
        R31bExecDeathTimerMainSub(0, 2, 900);
    }
    if (no == 1) {
        R31bExecDeathTimerMainSub(1, 5, 900);
    }
    if (no == 2) {
        R31bExecDeathTimerMainSub(2, 8, 900);
    }
}

// The count down while the cage room is open: runs out into the fall.
void R31bExecDeathTimerMainSub(int no, int light, int frames)
{
    int frame;

    pG->Room_flg[0] |= 0x40000000;
    Cckpt.transCountDownTimer(1);
    Cckpt.startCountDownTimer(0, 30, 0);
    Cckpt.setWarningTime(0, 10, 0);
    frame = 0;
    // Both exits are returns: a `break` would let expand_end_loop roll the flag test to the loop end.
    for (;;) {
        int over;

        if ((pG->Room_flg[0] & 0x40000000) == 0) {
            Cckpt.endCountDownTimer();
            Cckpt.transCountDownTimer(0);
            return;
        }
        over = Cckpt.isZeroCountDownTimer();
        if (over == 1) {
            pG->Room_flg[0] |= 0x80000000;
            SceExec(0x12, (TaskFunc) R31bExecFallMain, no, 0, 2, 0);
            return;
        }
        if (frame == frame / 90 * 90) {
            SndCall(6, 0xA, 0, 0, 0, 0);
        }
        SceSleep(1);
        frame++;
    }
}

// Exit door of cage room `no` (0..2): dispatch to R31bExecDoorMainSub with its flags, area, cuts, light
// set, door objects and effect.
static void R31bExecDoorMain(int no)
{
    if (no == 0) {
        R31bExecDoorMainSub(0, 0, 1, 0x23, 3, 3, 0x24, 3, 0x13, 0xD5, 4);
    }
    if (no == 1) {
        R31bExecDoorMainSub(1, 3, 4, 0x24, 6, 8, 0x25, 5, 0x31, 0xD6, 5);
    }
    if (no == 2) {
        R31bExecDoorMainSub(2, 6, 7, 0x25, 8, 0xA, 0xA, 7, 0x4F, 0xD7, 5);
    }
}

// The exit door of a cage room: opens when the shutters are open, otherwise the "locked" message.
void R31bExecDoorMainSub(int no, int flagOpen, int flagDoor, int doorFlag, int atNo, int cut, int cut2, int light, u32 objId0, u32 objId1, int est)
{
    if (RsfCheck(G_ROOM_ID, flagOpen)) {
        if (RsfCheck(G_ROOM_ID, flagDoor) == 0) {
            cObj* obj0;
            cObj* obj1;

            RsfSet(G_ROOM_ID, flagDoor);
            FlagOnVar(&pG->Key_flg, (u32) doorFlag);
            SceAtSetEnable(atNo, 0);
            if (no != 2) {
                pG->Room_flg[0] &= ~0x40000000;
            }
            SceEventStart(0);
            pG->Room_flg[0] &= ~0x20000000;
            SceSetEventCancel(1, (TaskFunc) R31bExecDoorEnd, no, 2, 1);
            if (cut != -1) {
                CamCtrl.CutCall((s8) cut);
            }
            obj0 = SmdGetObjPtr(objId0);
            obj1 = SmdGetObjPtr(objId1);
            if (obj0 && obj1) {
                int i;

                SndCall(6, 9, &obj0->pos, 0, 0, 0);
                for (i = 0; i < 60; i++) {
                    f32 z = (f32) i * -4000.0f / 60.0f + 1277.0f;

                    obj0->setPos(obj0->pos.x, obj0->pos.y, z);
                    obj1->setPos(obj1->pos.x, obj1->pos.y, z);
                    SceSleep(1);
                }
            }
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            if (no == 2) {
                cObj* smd;
                int i;

                if (cut != -1) {
                    CamCtrl.CutCall(0x23);
                }
                smd = r31b_work->smd;
                if (smd) {
                    SndCall(6, 0x12, &smd->pos, 0, 0, 0);
                    smd->setNoSuspend(1);
                    smd->setPos(0.0f, 0.0f, 0.0f);
                    {
                        Vec ang;

                        ang.x = 0.0f;
                        ang.y = 0.0f;
                        ang.z = 0.0f;
                        smd->setAng(&ang);
                    }
                    MotionSetCore(smd, &smd->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2C), 0, 0, 1, 0);
                }
                for (i = 0; i < 120; i++) {
                    SceSleep(1);
                }
            } else {
                int i;

                if (cut2 != -1) {
                    CamCtrl.CutCall((s8) cut2);
                }
                EstSet(0, -1, 0, 0, EFF_ROOM, (u8) est, 0x2001, ESP_CORE_KIND_NONE, 0, 0);
                pPL->beginEvent(0);
                pPL->setNoSuspend(1);
                Vec tbl[3] = {{-10850.0f, 0.0f, 1500.0f}, {8150.0f, 0.0f, 1500.0f}, {26650.0f, 0.0f, 1500.0f}};
                pPL->setPos(&tbl[no]);
                {
                    Vec ang;

                    ang.x = 0.0f;
                    ang.z = 0.0f;
                    ang.y = 1.5f;
                    pPL->setAng(&ang);
                }
                SndCall(6, 0xF, 0, 0, 0, 0);
                i = 0;
                while (CamCtrl.IsMotionEnd() != 1) {
                    if (i > 0xE) {
                        R31bExecFallRoom(no, 500.0f);
                    }
                    SceSleep(1);
                    i++;
                }
                pPL->setNoSuspend(0);
            }
            SceSetEventCancel(0, 0, 0, -1, 1);
            R31bExecDoorEnd(no);
        }
    } else {
        cObj* obj0 = SmdGetObjPtr(objId0);

        if (obj0) {
            SndCall(6, 1, &obj0->pos, 0, 0, 0);
        }
        SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        SceSleep(1);
    }
}

// End of the exit door of room `no`: dispatch to R31bExecDoorEndSub.
static void R31bExecDoorEnd(int no)
{
    if (no == 0) {
        R31bExecDoorEndSub(0, 0, 3, 0x13, 0xD5, 0);
    }
    if (no == 1) {
        R31bExecDoorEndSub(1, 1, 5, 0x31, 0xD6, 1);
    }
    if (no == 2) {
        R31bExecDoorEndSub(2, 2, 7, 0x4F, 0xD7, 2);
    }
}

// End of a door opening (also its cancel path): both door halves snapped open (z -2723) with their
// collision following, the light set, U-3's mode, camera back, SceEventEnd.
void R31bExecDoorEndSub(int no, int emMode, int light, u32 objId0, u32 objId1, int satNo)
{
    cObj* obj0 = SmdGetObjPtr(objId0);
    cObj* obj1 = SmdGetObjPtr(objId1);

    if (obj0 && obj1) {
        obj0->setPos(obj0->pos.x, obj0->pos.y, -2723.0f);
        obj1->setPos(obj1->pos.x, obj1->pos.y, -2723.0f);
        if (r31b_work->sat[satNo]) {
            r31b_work->sat[satNo]->setCoord(&obj0->pos, &obj0->ang);
        }
        if (r31b_work->eat[satNo]) {
            r31b_work->eat[satNo]->setCoord(&obj0->pos, &obj0->ang);
        }
    }
    R31bLight(light);
    if (no != 2) {
        cEm32* em = (cEm32*) r31b_work->em.getPtr();

        if (em) {
            em->setNext(emMode);
        }
        if (pG->Room_flg[0] & 0x20000000) {
            R31bExecFallEnd(no);
            return;
        }
        SceExec(0x12, (TaskFunc) R31bExecFallMain, no, 0, 2, 0);
    }
    pPL->setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Cage room `no` (0..2) falls: dispatch to R31bExecFallMainSub with its flag and camera cut.
static void R31bExecFallMain(int no)
{
    if (no == 0) {
        R31bExecFallMainSub(0, 2, 0xB);
    }
    if (no == 1) {
        R31bExecFallMainSub(1, 5, 0xC);
    }
    if (no == 2) {
        R31bExecFallMainSub(2, 8, 0xD);
    }
}

// The cage room falls away under the player (the death timer ran out or the door opened).
void R31bExecFallMainSub(int no, int flagNo, int cut)
{
    if (RsfCheck(G_ROOM_ID, flagNo) == 0) {
        RsfSet(G_ROOM_ID, flagNo);
        SceEventStart(0);
        SceSetEventCancel(1, (TaskFunc) R31bExecFallEnd, no, -1, 1);
        if (pG->Room_flg[0] & 0x80000000) {
            SndCall(6, 0x10, 0, 0, 0, 0);
            pPL->beginEvent(0);
            pPL->setNoSuspend(1);
            pPL->atari.off();
            Vec tbl[3] = {{-14000.0f, 0.0f, 0.0f}, {5000.0f, 0.0f, 0.0f}, {23500.0f, 0.0f, 0.0f}};
            Vec ang = {0, 0, 0};
            pPL->setPos(&tbl[no]);
            pPL->setAng(&ang);
            MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 0x201, 0);
            while (MotionGetState(pPL) == 0) {
                SceSleep(1);
            }
            pPL->setNoSuspend(0);
            SndCall(6, 0xF, 0, 0, 0, 0);
        }
        if (cut != -1) {
            CamCtrl.CutCall((s8) cut);
        }
        while (CamCtrl.IsMotionEnd() != 1) {
            R31bExecFallRoom(no, 900.0f);
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R31bExecFallEnd(no);
    }
}

// Every object of the room (and its lattices) drops by `spd`.
void R31bExecFallRoom(int no, f32 spd)
{
    int n = r31b_objNum[no];
    int i;

    for (i = 0; i < n; i++) {
        cObj* o = SmdGetGroupObjPtr(r31b_objTbl[no][i]);

        if ((u32) o < 0x80000000 || (u32) o > 0x82FFFFFF) {
            pLog->err(0, 0, "R31bExecFallMainSub INVALID INDEX %d", r31b_objTbl[no][i]);
            return;
        }
        do {
            o->setPos(o->pos.x, o->pos.y - spd, o->pos.z);
            o = SmdGetGroupNext(o);
        } while (o);
    }
    int m = 25;
    for (i = 0; i < m; i++) {
        cObj* o = SmdGetGroupObjPtr(r31b_kanaamiTbl[no][i]);

        if ((u32) o < 0x80000000 || (u32) o > 0x82FFFFFF) {
            pLog->err(0, 0, "R31bExecFallMainSub INVALID INDEX %d", r31b_objTbl[no][i]);
            return;
        }
        do {
            o->setPos(o->pos.x, o->pos.y - spd, o->pos.z);
            o = SmdGetGroupNext(o);
        } while (o);
    }
}

// End of the fall of room `no`: dispatch to R31bExecFallEndSub.
static void R31bExecFallEnd(int no)
{
    if (no == 0) {
        R31bExecFallEndSub(0, 0x13, 0, 2);
    }
    if (no == 1) {
        R31bExecFallEndSub(1, 0x31, 1, 5);
    }
    if (no == 2) {
        R31bExecFallEndSub(2, 0x4F, 2, 8);
    }
}

// End of a room fall (also its cancel path): the room flag set, the room hidden, its collision moved
// to the fallen position, the light set, SceEventEnd.
void R31bExecFallEndSub(int no, u32 objId, int satNo, int flagNo)
{
    cObj* obj;

    RsfSet(G_ROOM_ID, flagNo);
    R31bSmdTransOff(no);
    obj = SmdGetObjPtr(objId);
    if (obj) {
        if (r31b_work->sat[satNo]) {
            r31b_work->sat[satNo]->setCoord(&r31b_work->satPos[satNo], &obj->ang);
        }
        if (r31b_work->eat[satNo]) {
            r31b_work->eat[satNo]->setCoord(&r31b_work->satPos[satNo], &obj->ang);
        }
    }
    if (pG->Room_flg[0] & 0x80000000) {
        pPL->atari.on();
        DiedemoExec(0, 0);
    } else {
        pPL->setPos(&r31b_fallPlPos[no]);
        pPL->setAng(0.0f, 1.5f, 0.0f);
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        SceExit();
    }
}

// Hide a fallen room: its objects, lattices and the enemy collision of its walls.
void R31bSmdTransOff(int no)
{
    int n = r31b_objNum[no];

    for (int i = 0; i < n; i++) {
        SmdSetTrans(r31b_objTbl[no][i], 0);
    }
    for (int i = 0; i < 25; i++) {
        SmdSetTrans(r31b_kanaamiTbl[no][i], 0);
    }
    if (no == 0) {
        if (r31b_work->eat[13]) {
            r31b_work->eat[13]->setDisable();
        }
        if (r31b_work->eat[0]) {
            r31b_work->eat[0]->setDisable();
        }
        if (r31b_work->eat[5]) {
            r31b_work->eat[5]->setDisable();
        }
        if (r31b_work->eat[6]) {
            r31b_work->eat[6]->setDisable();
        }
        SceAtSetEnable(0x8A, 0);
        SceAtSetEnable(0x8D, 0);
    }
    if (no == 1) {
        if (r31b_work->eat[14]) {
            r31b_work->eat[14]->setDisable();
        }
        if (r31b_work->eat[1]) {
            r31b_work->eat[1]->setDisable();
        }
        if (r31b_work->eat[7]) {
            r31b_work->eat[7]->setDisable();
        }
        if (r31b_work->eat[8]) {
            r31b_work->eat[8]->setDisable();
        }
        if (r31b_work->eat[9]) {
            r31b_work->eat[9]->setDisable();
        }
        SceAtSetEnable(0x8C, 0);
        SceAtSetEnable(0x8E, 0);
    }
    if (no == 2) {
        if (r31b_work->eat[15]) {
            r31b_work->eat[15]->setDisable();
        }
        if (r31b_work->eat[2]) {
            r31b_work->eat[2]->setDisable();
        }
        if (r31b_work->eat[10]) {
            r31b_work->eat[10]->setDisable();
        }
        if (r31b_work->eat[11]) {
            r31b_work->eat[11]->setDisable();
        }
        if (r31b_work->eat[12]) {
            r31b_work->eat[12]->setDisable();
        }
    }
}

// The escape from the third room: U-3 breaks through, the player runs, the room collapses.
static void R31bExecEscapeMain()
{
    int no = 2;

    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        cEm32* em;
        cObj* smd;
        int n;
        int frame;

        RsfSet(G_ROOM_ID, 8);
        SceAtSetEnable(0x22, 0);
        pG->Room_flg[0] &= ~0x40000000;
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceEventStart(0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        r31b_work->str = SndStrPlayBlock(1, 0x34, 0.0f);
        SysFlagOff(pG, SYS_SCREEN_STOP);
        SceSetEventCancel(1, (TaskFunc) R31bExecEscapeEnd, 0, -1, 1);
        em = (cEm32*) r31b_work->em.getPtr();
        if (em) {
            em->setNext(2);
            em->setNoSuspend(1);
            em->setPos(0.0f, 0.0f, 0.0f);
            em->setAng(0.0f, 0.0f, 0.0f);
        }
        smd = r31b_work->smd;
        if (smd) {
            smd->setNoSuspend(1);
            smd->setPos(0.0f, 0.0f, 0.0f);
            smd->setAng(0.0f, 0.0f, 0.0f);
            MotionSetCore(smd, &smd->Motion, ROOM_ARC_PTR(pG->pRoom, 0x29), 0, 0, 1, 0);
        }
        pPL->beginEvent(0);
        pPL->setNoSuspend(1);
        pPL->setPos(0.0f, 0.0f, 0.0f);
        pPL->setAng(0.0f, 0.0f, 0.0f);
        MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x25), 0, 0, 0x201, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_NONE, 0, 0);
        SmdSetTrans(0x4D, 0);
        SmdSetTrans(0x95, 0);
        SmdSetTrans(0x98, 0);
        SmdSetTrans(0x99, 0);
        n = r31b_objNum[no];
        for (int i = 0; i < n; i++) {
            cObj* o = SmdGetGroupObjPtr(r31b_objTbl[no][i]);

            while (o) {
                MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x26), 0, 0, 0x201, 0);
                o->be_flag |= 0x20;
                o = SmdGetGroupNext(o);
            }
        }
        for (int i = 0; i < 25; i++) {
            SmdSetTrans(r31b_kanaamiTbl[no][i], 0);
        }
        for (int i = 0; i < 6; i++) {
            SmdSetTrans(r31b_kanaami3a[i], 0);
            SmdSetTrans(r31b_kanaami3b[i], 0);
        }
        frame = 0;
        while (MotionGetState(pPL) == 0) {
            if (frame == 250) {
                FadeSetW(2, 30, 0, 0);
                SndRoomStrStop(3);
            }
            frame++;
            SceSleep(1);
        }
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        for (int i = 0; i < 20; i++) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R31bExecEscapeEnd();
    }
}

// End of the escape from room 3 (also its cancel path): stream stopped, the room hidden, the battle
// stream off, the player's motion reset, U-3 placed for the next phase, the room flag set.
static void R31bExecEscapeEnd()
{
    cEm* em;
    cObj* smd;
    int n;

    SndStrReq(r31b_work->str, 8, 0, 0);
    R31bSmdTransOff(2);
    SndRoomStrStop(3);
    pPL->motionSet(pPL->m_MotTbl[0], pPL->m_MotTbl[1], pPL->m_MotTbl[0x5F], pPL->m_MotTbl[0x60], 0, 0);
    MotionMove(pPL, 0);
    pPL->setNoSuspend(0);
    pPL->setPos(37120.0f, 4265.0f, -1500.0f);
    pPL->setAng(0.0f, 0.72f, 0.0f);
    em = r31b_work->em.getPtr();
    if (em) {
        em->be_flag &= ~2;
    }
    smd = r31b_work->smd;
    if (smd) {
        MotionSetCore(smd, &smd->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2D), 0, 0, 1, 0);
    }
    n = r31b_objNum[2];
    for (int i = 0; i < n; i++) {
        SmdSetTrans(r31b_objTbl[2][i], 0);
    }
    for (int i = 0; i < 25; i++) {
        SmdSetTrans(r31b_kanaamiTbl[2][i], 0);
    }
    R31bLightAllOn();
    StaFlagOff(pG, STA_LIT_NO_UPDATE);
    FadeSetW(0x80000002, 40, 0, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    GameSave.save(pSaveData, -1);
    SceExit();
}

// U-3 lifts the first room's shutter.
static void R31bExecRoom01U3Main()
{
    if (RsfCheck(G_ROOM_ID, 0x17) == 0) {
        cObj* obj;

        RsfSet(G_ROOM_ID, 0x17);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceEventStart(0);
        LightMgr.onKind(0x13);
        r31b_work->em.setFlag(1);
        r31b_work->em.setNoSuspend(1);
        pPL->beginEvent(0);
        pPL->setNoSuspend(1);
        pPL->setAng(0.0f, 3.1415927f, 0.0f);
        MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x22), 0, 0, 0x201, 0);
        pPL->setPos(-2357.5f, pPL->pos.y, -36.0f);
        // Declared after the player set-up: its slot is the freed SetPosXYZ/SetAngXYZ temp (8(r1)); the
        // `= {0,0,0}` memset libcall puts the address in a pseudo that the setPos below reuses (r27).
        Vec zero = {0, 0, 0};

        obj = SmdGetObjPtr(0xC1);
        if (obj) {
            SndCall(6, 0xE, &pPL->pos, 0, 0, 0);
            r31b_work->savePos = obj->pos;
            obj->setPos(&zero);
            obj->be_flag |= 0x20;
            MotionSetCore(obj, &obj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x23), 0, 0, 1, 0);
        }
        SceSetEventCancel(1, (TaskFunc) R31bExecRoom01U3End, 0, -1, 1);
        while (MotionGetState(pPL) == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R31bExecRoom01U3End();
    }
}

// End of U-3's room-1 shutter lift (also its cancel path): the shutter snapped, U-3 into its next phase, camera back.
static void R31bExecRoom01U3End()
{
    cObj* obj;
    cEm32* em;

    if (r31b_work->koushi[3]) {
        r31b_work->koushi[3]->hp = 1;
    }
    SceAtSetEnable(0x18, 1);
    obj = SmdGetObjPtr(0xC1);
    if (obj) {
        obj->setPos(r31b_work->savePos.x, 125.0f, r31b_work->savePos.z);
        if (r31b_work->sat[8]) {
            r31b_work->sat[8]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->eat[8]) {
            r31b_work->eat[8]->setCoord(&obj->pos, &obj->ang);
        }
        MotionClear(obj, 1);
    }
    r31b_work->em.setNoSuspend(0);
    em = (cEm32*) r31b_work->em.getPtr();
    if (em) {
        em->setNext(6);
    }
    pPL->setNoSuspend(0);
    pPL->setPos(-1450.0f, 0.0f, 113.0f);
    pPL->setAng(0.0f, -0.48f, 0.0f);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceExit();
}

// U-3 shows up in the second room.
static void R31bExecRoom02U3Main()
{
    if (RsfCheck(G_ROOM_ID, 0x1F) == 0) {
        RsfSet(G_ROOM_ID, 0x1F);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceEventStart(0);
        CamCtrl.CutCall(0x22);
        r31b_work->em.setFlag(1);
        r31b_work->em.setNoSuspend(1);
        pPL->beginEvent(0);
        pPL->setNoSuspend(1);
        pPL->setPos(12696.0f, 0.0f, 7280.0f);
        pPL->setAng(0.0f, -2.4f, 0.0f);
        SceSetEventCancel(1, (TaskFunc) R31bExecRoom02U3End, 0, -1, 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R31bExecRoom02U3End();
    }
}

// End of U-3's room-2 appearance (also its cancel path): U-3 released into its next phase, camera back, SceEventEnd.
static void R31bExecRoom02U3End()
{
    cEm32* em;

    r31b_work->em.setNoSuspend(0);
    em = (cEm32*) r31b_work->em.getPtr();
    if (em) {
        em->setNext(7);
    }
    pPL->setNoSuspend(0);
    pPL->endEvent(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceExit();
}

// The third room: U-3 catches the player, the shutter comes down.
static void R31bExecRoom03U3Main()
{
    if (RsfCheck(G_ROOM_ID, 0x1C) == 0) {
        cEm32* em;
        cObj* obj;
        // The player EstSet's two zero words come from `zero`, kept in its own callee-saved register
        // (`li r24, 0` in this block; a REG_EQUIV constant, so global allocates it last), while the
        // enemy EstSet's literal zeros are canonicalised onto the RsfCheck `andi.` result known 0 on
        // this path. cse1 keeps `zero` because its last mention (the dead `zero = em` below) lies
        // beyond the ebb of the player arm (make_regs_eqv puts it first in the class); the dead
        // set is what keeps that true at cse2 as well (the uses in the enemy arm are rewritten by
        // cse1), and flow deletes it before local-alloc counts the sets.
        void* zero = 0;

        RsfSet(G_ROOM_ID, 0x1C);
        KyfFlagOff(pG, KYF_ST1_16);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceEventStart(0);
        StaFlagOn(pG, STA_LIT_NO_UPDATE);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        SndRoomStrStart(1, 0, 1);
        r31b_work->str = SndStrPlayBlock(1, 0x35, 0.0f);
        pPL->beginEvent(0);
        pPL->setNoSuspend(1);
        pPL->setPos(0.0f, 0.0f, 0.0f);
        pPL->setAng(0.0f, 0.0f, 0.0f);
        MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2B), 0, 0, 0x201, 0);
        em = (cEm32*) r31b_work->em.getPtr();
        if (em) {
            em->setNext(4);
            em->be_flag |= 2;
        }
        r31b_work->em.setFlag(1);
        r31b_work->em.setNoSuspend(1);
        if (pPL) {
            EstSet(pPL, -1, 0, 0, EFF_ROOM, 0xB, 0x2001, ESP_CORE_KIND_ROOM06, zero, zero);
        }
        if (em) {
            EstSet(em, -1, 0, 0, EFF_ROOM, 0xC, 0x2001, ESP_CORE_KIND_ROOM06, 0, 0);
            zero = em;
        }
        SceSleep(1);
        SysFlagOff(pG, SYS_SCREEN_STOP);
        SceSetEventCancel(1, (TaskFunc) R31bExecRoom03U3End, 0, -1, 1);
        for (int i = 0; i < 293; i++) {
            SceSleep(1);
        }
        obj = SmdGetObjPtr(0xEA);
        if (obj) {
            r31b_work->snd = SndCall(6, 4, &obj->pos, 0, 0, 0);
            for (int i = 0; i < 10; i++) {
                obj->setPos(obj->pos.x, (f32) i * -3000.0f / 10.0f + 7313.0f, obj->pos.z);
                SceSleep(1);
            }
            obj->setPos(obj->pos.x, 4313.0f, obj->pos.z);
            SndCall(6, 5, &obj->pos, 0, 0, 0);
            r31b_work->snd = 0;
        }
        while (MotionGetState(pPL) == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R31bExecRoom03U3End();
    }
}

// End of the room-3 catch cut (also its cancel path): the shutter snapped down, U-3 and the player
// released, the fight in room 3 begins.
static void R31bExecRoom03U3End()
{
    cObj* obj;
    cEm32* em;

    SndStrReq(r31b_work->str, 8, 0, 0);
    EffectDelete(0x2001, ESP_CORE_KIND_ROOM06);
    obj = SmdGetObjPtr(0xEA);
    if (obj) {
        obj->setPos(obj->pos.x, 4313.0f, obj->pos.z);
        if (r31b_work->sat[12]) {
            r31b_work->sat[12]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->eat[12]) {
            r31b_work->eat[12]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->snd) {
            SndCall(6, 5, &obj->pos, 0, 0, 0);
            r31b_work->snd = 0;
        }
    }
    MotionClear(pPL, 0);
    MotionMove(pPL, 0);
    pPL->setNoSuspend(0);
    pPL->setPos(54117.0f, 4266.0f, 13190.0f);
    pPL->setAng(0.0f, -2.147f, 0.0f);
    em = (cEm32*) r31b_work->em.getPtr();
    if (em) {
        em->setNext(5);
    }
    StaFlagOff(pG, STA_LIT_NO_UPDATE);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceExit();
}

// U-3 dies in the third room: the shutter lifts again.
static void R31bExecRoom03U3DieMain()
{
    if (RsfCheck(G_ROOM_ID, 0xB) == 0) {
        cEm32* em;
        cObj* obj;

        RsfSet(G_ROOM_ID, 0xB);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        SceEventStart(0);
        SndRoomStrStop(3);
        r31b_work->em.setNoSuspend(1);
        em = (cEm32*) r31b_work->em.getPtr();
        if (em) {
            while (em->ckDie() == 0) {
                SceSleep(1);
            }
        }
        CamCtrl.CutCall(0x1E);
        obj = SmdGetObjPtr(0xEA);
        if (obj) {
            int i;

            r31b_work->snd = SndCall(6, 4, &obj->pos, 0, 0, 0);
            for (i = 0; i < 60; i++) {
                obj->setPos(obj->pos.x, (f32) i * 3000.0f / 60.0f + 4313.0f, obj->pos.z);
                SceSleep(1);
            }
            obj->setPos(obj->pos.x, 7313.0f, obj->pos.z);
            SndCall(6, 5, &obj->pos, 0, 0, 0);
            r31b_work->snd = 0;
        }
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R31bExecRoom03U3DieEnd();
    }
}

// End of U-3's death cut: the shutter lifted, the count-down off, the exit gondola available.
void R31bExecRoom03U3DieEnd()
{
    cObj* obj = SmdGetObjPtr(0xEA);
    cEmDoor* door;

    if (obj) {
        obj->setPos(obj->pos.x, 7313.0f, obj->pos.z);
        if (r31b_work->sat[12]) {
            r31b_work->sat[12]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->eat[12]) {
            r31b_work->eat[12]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->snd) {
            SndCall(6, 5, &obj->pos, 0, 0, 0);
            r31b_work->snd = 0;
        }
    }
    getRoomEtcDoor(4, &door, 1);
    if (door) {
        door->setNormal();
    }
    SceAtSetEnable(0x24, 0);
    KyfFlagOn(pG, KYF_ST1_15);
    KyfFlagOn(pG, KYF_ST1_16);
    pPL->setPos(55080.0f, 4266.0f, 13710.0f);
    pPL->setAng(0.0f, -2.718f, 0.0f);
    pPL->matUpdate();
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceExit();
}

// The gondola between the cage corridor and the far platform (dir 0 = across, 1 = back).
static void R31bExecGondolaMain(int dir)
{
    f32 maxSpd = 100.0f;
    f32 minSpd = 10.0f;
    f32 accel = 2.0f;
    cObj* obj;
    f32 spd;
    f32 rate;
    f32 stopDist;
    f32 move;
    int faded;
    int frame;
    int i;
    FadeWork* fade;

    obj = SmdGetObjPtr(0xA3);
    if (obj == 0) {
        return;
    }
    if (dir == 0) {
        RsfSet(G_ROOM_ID, 0xD);
        SceAtSetEnable(0xF, 0);
        SceAtSetEnable(0x10, 1);
    } else {
        RsfClear(G_ROOM_ID, 0xD);
        SceAtSetEnable(0xF, 1);
        SceAtSetEnable(0x10, 0);
    }
    SceEventStart(0);
    SceSetEventCancel(1, (TaskFunc) R31bExecGondolaEnd, dir, -1, 1);
    r31b_work->snd = 0;
    faded = 0;
    obj->setNoSuspend(1);
    obj->setPos(&r31b_gondolaPos[0][dir]);
    pPL->setNoSuspend(1);
    pPL->beginEvent(0);
    obj->setPos(r31b_gondolaPos[0][dir].x, r31b_gondolaPos[0][dir].y, r31b_gondolaPos[0][dir].z);
    pPL->setPos(obj->pos.x, pPL->pos.y, obj->pos.z);
    pPL->setAng(pPL->ang.x, r31b_gondolaAng[dir], pPL->ang.z);
    spd = 0.0f;
    if (dir == 0) {
        CamCtrl.CutCall(0x1F);
    } else {
        CamCtrl.CutCall(0x20);
    }
    r31b_work->snd = SndCall(6, 0xB, &obj->pos, 0, 0, 0);
    rate = 1.0f;
    for (i = 0; i < 10; i++) {
        rate -= 0.2f;
        if (rate < 0.1f) {
            rate = 0.1f;
        }
        obj->setAng(obj->ang.x, obj->ang.y, fRand1_1() * rate * 3.1415927f / 180.0f);
        SceSleep(1);
    }
    obj->setAng(0.0f, 0.0f, 0.0f);
    frame = 0;
    fade = &Fade[2];
    for (;;) {
        spd += accel;
        if (spd > maxSpd) {
            spd = maxSpd;
        }
        if (dir == 0) {
            obj->setPos(obj->pos.x - spd, obj->pos.y, obj->pos.z);
            pPL->setPos(obj->pos.x, pPL->pos.y, obj->pos.z);
        } else {
            obj->setPos(obj->pos.x + spd, obj->pos.y, obj->pos.z);
            pPL->setPos(obj->pos.x, pPL->pos.y, obj->pos.z);
        }
        if (faded == 0) {
            if (frame == 90) {
                FadeSetW(2, 30, 0, 0);
                faded = 1;
            }
        } else if ((fade->flags & 1) == 0) {
            break;
        }
        SceSleep(1);
        frame++;
    }
    obj->setNoSuspend(1);
    obj->setPos(&r31b_gondolaPos[1][dir]);
    pPL->setNoSuspend(1);
    pPL->beginEvent(0);
    stopDist = CalcStopDist(spd, accel);
    move = stopDist + 4000.0f;
    if (dir != 0) {
        move = 0.0f - move;
    }
    obj->setPos(r31b_gondolaPos[1][dir].x + move, r31b_gondolaPos[1][dir].y, r31b_gondolaPos[1][dir].z);
    spd = maxSpd;
    pPL->setPos(obj->pos.x, pPL->pos.y, obj->pos.z);
    CamCtrl.Comeback(0);
    FadeSetW(0x80000002, 30, 0, 0);
    while (1) {
        if (__builtin_fabsf(r31b_gondolaPos[1][dir].x - obj->pos.x) < stopDist) {
            spd -= accel;
            if (spd < minSpd) {
                spd = minSpd;
            }
        }
        if (dir == 0) {
            obj->setPos(obj->pos.x - spd, obj->pos.y, obj->pos.z);
            pPL->setPos(obj->pos.x, pPL->pos.y, obj->pos.z);
            if (obj->pos.x <= r31b_gondolaPos[1][dir].x) {
                break;
            }
            Vec q = {0, 0, 0};

            q.x = -spd;
            pG->quake_ofs = q;
        } else {
            obj->setPos(obj->pos.x + spd, obj->pos.y, obj->pos.z);
            pPL->setPos(obj->pos.x, pPL->pos.y, obj->pos.z);
            if (obj->pos.x >= r31b_gondolaPos[1][dir].x) {
                break;
            }
            Vec q = {0, 0, 0};

            q.x = spd;
            pG->quake_ofs = q;
        }
        SceSleep(1);
    }
    SndCall(6, 0xC, &obj->pos, 0, 0, 0);
    r31b_work->snd = 0;
    rate = 1.0f;
    for (i = 0; i < 10; i++) {
        rate -= 0.2f;
        if (rate < 0.1f) {
            rate = 0.1f;
        }
        obj->setAng(obj->ang.x, obj->ang.y, fRand1_1() * rate * 3.1415927f / 180.0f);
        SceSleep(1);
    }
    obj->setAng(0.0f, 0.0f, 0.0f);
    SceSetEventCancel(0, 0, 0, -1, 1);
    R31bExecGondolaEnd(dir);
}

// End of the gondola ride (dir): the gondola and player snapped to the arrival side, the areas swapped
// (Room_flg bit 0xD), camera back, SceEventEnd.
static void R31bExecGondolaEnd(int dir)
{
    cObj* obj = SmdGetObjPtr(0xA3);

    FadeKill(2);
    if (obj) {
        obj->setPos(r31b_gondolaPos[1][dir].x, r31b_gondolaPos[1][dir].y, r31b_gondolaPos[1][dir].z);
        obj->setAng(0.0f, 0.0f, 0.0f);
        pPL->setPos(obj->pos.x, pPL->pos.y, obj->pos.z);
        pPL->setAng(pPL->ang.x, r31b_gondolaAng[dir], pPL->ang.z);
        if (r31b_work->eat[16]) {
            r31b_work->eat[16]->setCoord(&obj->pos, &obj->ang);
        }
        if (r31b_work->snd) {
            SndCall(6, 0xC, &obj->pos, 0, 0, 0);
            r31b_work->snd = 0;
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Collision planes of cage room `no`'s door halves (both objects).
void R31bDoorSat(int no)
{
    if (no == 0) {
        R31bDoorSatSub(0, 0x13, 0);
    }
    if (no == 1) {
        R31bDoorSatSub(1, 0x31, 1);
    }
    if (no == 2) {
        R31bDoorSatSub(2, 0x4F, 2);
    }
}

// Collision plane of a cage room door (scenario and enemy).
void R31bDoorSatSub(int no, u32 objId, int satNo)
{
    Vec poly[4] = {{-130.0f, 0.0f, -2000.0f}, {130.0f, 0.0f, -2000.0f}, {130.0f, 0.0f, 2000.0f}, {-130.0f, 0.0f, 2000.0f}};
    f32 h = 5000.0f;
    cObj* obj = SmdGetObjPtr(objId);

    if (obj) {
        r31b_work->sat[satNo] = SatMgr.create(&obj->pos, &obj->ang, poly, h, 0, 0x100);
        r31b_work->eat[satNo] = EatMgr.create(&obj->pos, &obj->ang, poly, h, 0x40, 0x100);
        r31b_work->satPos[satNo] = obj->pos;
    }
}

// Collision planes and switch-lamp hit enemies of cage room `no`'s two shutters.
void R31bKoushiSat(int no)
{
    if (no == 0) {
        R31bKoushiSatSub1(0, 0xBA, 5, 0);
        R31bKoushiSatSub1(0, 0xBB, 6, 0);
        R31bKoushiSatSub2(0, 0xB8, 0);
        R31bKoushiSatSub2(0, 0xB9, 1);
    }
    if (no == 1) {
        R31bKoushiSatSub1(1, 0xC0, 7, 0);
        R31bKoushiSatSub1(1, 0xC1, 8, 1);
        R31bKoushiSatSub1(1, 0xC6, 9, 4);
        R31bKoushiSatSub2(1, 0xC3, 2);
        R31bKoushiSatSub2(1, 0xC5, 3);
        R31bKoushiSatSub2(1, 0xC8, 4);
    }
    if (no == 2) {
        R31bKoushiSatSub1(2, 0x97, 0xA, 0);
        R31bKoushiSatSub1(2, 0x93, 0xB, 3);
        R31bKoushiSatSub2(2, 0x98, 5);
    }
    if (no == 3) {
        R31bKoushiSatSub1(3, 0xEA, 0xC, 2);
    }
}

// Collision plane of a shutter: five shapes.
void R31bKoushiSatSub1(int no, u32 objId, int satNo, int type)
{
    Vec poly0[4] = {{-1700.0f, 0.0f, -100.0f}, {1700.0f, 0.0f, -100.0f}, {1700.0f, 0.0f, 100.0f}, {-1700.0f, 0.0f, 100.0f}};
    Vec poly1[4] = {{-100.0f, 0.0f, -1700.0f}, {100.0f, 0.0f, -1700.0f}, {100.0f, 0.0f, 1700.0f}, {-100.0f, 0.0f, 1700.0f}};
    Vec poly2[4] = {{-2200.0f, 0.0f, -100.0f}, {2200.0f, 0.0f, -100.0f}, {2200.0f, 0.0f, 100.0f}, {-2200.0f, 0.0f, 100.0f}};
    Vec poly3[4] = {{-1275.0f, 0.0f, -100.0f}, {1375.0f, 0.0f, -100.0f}, {1375.0f, 0.0f, 100.0f}, {-1375.0f, 0.0f, 100.0f}};
    Vec poly4[4] = {{-1325.0f, 0.0f, -100.0f}, {1325.0f, 0.0f, -100.0f}, {1325.0f, 0.0f, 100.0f}, {-1325.0f, 0.0f, 100.0f}};
    // The two heights as named constants: their (folded-away) initialisers put 2500.0 and 3500.0
    // into the pool before the 0.0 default, and the 2500.0 one leaves the `lis` the cases share.
    const f32 h1 = 2500.0f;
    const f32 h2 = 3500.0f;
    Vec* poly = 0;
    f32 h = 0.0f;
    cObj* obj;

    switch (type) {
    case 0:
        poly = poly0;
        h = h1;
        break;
    case 1:
        poly = poly1;
        h = h1;
        break;
    case 2:
        poly = poly2;
        h = h2;
        break;
    case 3:
        poly = poly3;
        h = h1;
        break;
    case 4:
        poly = poly4;
        h = h1;
        break;
    }
    obj = SmdGetObjPtr(objId);
    if (obj) {
        r31b_work->sat[satNo] = SatMgr.create(&obj->pos, &obj->ang, poly, h, 0, 0x100);
        r31b_work->eat[satNo] = EatMgr.create(&obj->pos, &obj->ang, poly, h, 0x40, 0x100);
    }
}

// The hit enemy of a lattice switch lamp.
void R31bKoushiSatSub2(int no, u32 lampId, int koushiNo)
{
    cObj* obj = SmdGetObjPtr(lampId);

    if (obj) {
        r31b_work->koushi[koushiNo] = SetEmHit(ROOM_ARC_PTR(pG->pCore, 8), ROOM_ARC_PTR(pG->pCore, 9), &obj->pos, &obj->ang, 1);
        if (r31b_work->koushi[koushiNo]) {
            YarareInitCube(r31b_work->koushi[koushiNo], 0.0f, -400.0f, -150.0f, 300.0f, 800.0f, 150.0f, 0, YAT_FLAG_ON);
        }
    }
}

// A lamp shot: the shutter opens.
void R31bKoushiSatCk2(int no, int flagNo, int koushiNo)
{
    if (RsfCheck(G_ROOM_ID, flagNo) == 0) {
        if (r31b_work->koushi[koushiNo] && r31b_work->koushi[koushiNo]->ckStatus() == 1) {
            SndCall(6, 8, &r31b_work->koushi[koushiNo]->pos, 0, 0, 0);
            SceExec(0x12, (TaskFunc) R31bExecShutterOpenMain, no, 0, 2, 0);
        }
    }
}

// One frame in: the U-3 handle is attached to the live boss (revisit / debug).
static void R31bEmSetMain()
{
    cEm32* em;

    SceSleep(1);
    r31b_work->em.setEm(0x14, -1, 0, 1, 1);
    em = (cEm32*) r31b_work->em.getPtr();
    if (em) {
        em->setNext(4);
        *(cEm**) &Cckpt = em;
    }
}

// Turn all 27 light kinds on.
void R31bLightAllOn()
{
    int i;

    for (i = 0; i < 27; i++) {
        LightMgr.onKind((u8) i);
    }
}

// Light set `no`: all kinds on, then the kinds the set turns off (per room / shutter state).
void R31bLight(int no)
{
    R31bLightAllOn();
    switch (no) {
    case 1:
        LightMgr.offKind(7);
        LightMgr.offKind(8);
        LightMgr.offKind(9);
        break;
    case 2:
        LightMgr.offKind(2);
        LightMgr.offKind(8);
        LightMgr.offKind(9);
        LightMgr.offKind(0xE);
        LightMgr.offKind(0xF);
        LightMgr.offKind(0xD);
        LightMgr.offKind(0xC);
        break;
    case 3:
        LightMgr.offKind(2);
        LightMgr.offKind(7);
        LightMgr.offKind(8);
        LightMgr.offKind(9);
        LightMgr.offKind(0xC);
        LightMgr.offKind(0xD);
        LightMgr.offKind(0xE);
        LightMgr.offKind(0xF);
        break;
    case 4:
        LightMgr.offKind(2);
        LightMgr.offKind(3);
        LightMgr.offKind(7);
        LightMgr.offKind(9);
        LightMgr.offKind(0xC);
        LightMgr.offKind(0xD);
        LightMgr.offKind(0xE);
        LightMgr.offKind(0xF);
        LightMgr.offKind(0x12);
        LightMgr.offKind(0x13);
        LightMgr.offKind(0x14);
        LightMgr.offKind(0x10);
        LightMgr.offKind(0x11);
        break;
    case 5:
        LightMgr.offKind(2);
        LightMgr.offKind(3);
        LightMgr.offKind(7);
        LightMgr.offKind(8);
        LightMgr.offKind(9);
        LightMgr.offKind(0xC);
        LightMgr.offKind(0xD);
        LightMgr.offKind(0xE);
        LightMgr.offKind(0xF);
        LightMgr.offKind(0x10);
        LightMgr.offKind(0x11);
        LightMgr.offKind(0x12);
        LightMgr.offKind(0x13);
        LightMgr.offKind(0x14);
        break;
    case 6:
        LightMgr.offKind(2);
        LightMgr.offKind(3);
        LightMgr.offKind(4);
        LightMgr.offKind(7);
        LightMgr.offKind(8);
        LightMgr.offKind(0xC);
        LightMgr.offKind(0xD);
        LightMgr.offKind(0xE);
        LightMgr.offKind(0xF);
        LightMgr.offKind(0x10);
        LightMgr.offKind(0x11);
        LightMgr.offKind(0x12);
        LightMgr.offKind(0x13);
        LightMgr.offKind(0x14);
        LightMgr.offKind(0x16);
        LightMgr.offKind(0x1A);
        LightMgr.offKind(0x15);
        break;
    case 7:
        LightMgr.offKind(2);
        LightMgr.offKind(3);
        LightMgr.offKind(4);
        LightMgr.offKind(7);
        LightMgr.offKind(8);
        LightMgr.offKind(9);
        LightMgr.offKind(0xC);
        LightMgr.offKind(0xD);
        LightMgr.offKind(0xE);
        LightMgr.offKind(0xF);
        LightMgr.offKind(0x10);
        LightMgr.offKind(0x11);
        LightMgr.offKind(0x12);
        LightMgr.offKind(0x13);
        LightMgr.offKind(0x14);
        LightMgr.offKind(0x15);
        LightMgr.offKind(0x16);
        LightMgr.offKind(0x1A);
        break;
    case 8:
        LightMgr.offKind(2);
        LightMgr.offKind(3);
        LightMgr.offKind(4);
        LightMgr.offKind(5);
        LightMgr.offKind(7);
        LightMgr.offKind(8);
        LightMgr.offKind(9);
        LightMgr.offKind(0xC);
        LightMgr.offKind(0xD);
        LightMgr.offKind(0xE);
        LightMgr.offKind(0xF);
        LightMgr.offKind(0x10);
        LightMgr.offKind(0x11);
        LightMgr.offKind(0x12);
        LightMgr.offKind(0x13);
        LightMgr.offKind(0x14);
        LightMgr.offKind(0x15);
        LightMgr.offKind(0x16);
        LightMgr.offKind(0x17);
        LightMgr.offKind(0x18);
        LightMgr.offKind(0x19);
        LightMgr.offKind(0x1A);
        break;
    }
}

// Show / hide lattice `no` (0..24) of cage room `room` (0..2) from r31b_kanaamiTbl.
void R31bKanaamiTrans(u8 room, u8 no, int on)
{
    int r = room;
    int n = no;

    if (r <= 2 && n <= 24) {
        SmdSetTrans(r31b_kanaamiTbl[r][n], on);
    }
}

// Room 3's lattices: swap the intact (a) and broken (b) object of lattice `no` (0..5).
void R31bKanaamiRoom03Trans(int no, int on)
{
    if (no <= 5) {
        if (on == 1) {
            SmdSetTrans(r31b_kanaami3a[no], 1);
            SmdSetTrans(r31b_kanaami3b[no], 0);
        } else {
            SmdSetTrans(r31b_kanaami3a[no], 0);
            SmdSetTrans(r31b_kanaami3b[no], 1);
        }
    }
}

// Event r31bs00 callback (U-3 breaks in): the entrance effect dropped and scroll objects 0x82/0x6B/0xF4
// swapped; pl0010 (Leon) ot_type 2 and evma300's light mask on cut 0.
void Evt_R31BS00_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        EffectDelete(0x2001, ESP_CORE_KIND_ROOM01);
        SmdSetTrans(0x82, 0);
        SmdSetTrans(0x6B, 1);
        SmdSetTrans(0xF4, 0);
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 2;
            }
            if (e->GetMod(&mod, "evma300", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 2;
            }
        }
        break;
    case 2:
        SmdSetTrans(0x82, 1);
        SmdSetTrans(0x6B, 0);
        SmdSetTrans(0xF4, 1);
        break;
    }
}

// The count-down state test: the module build had it inline in the header after the class (a
// linkonce copy follows the room's code; the DOL's is game/mercenaries.cpp's).
// local copy: a header definition changes this unit's allocation (declaration order)
inline int CountDown::checkState(u32 state)
{
    return (m_state & state) ? 1 : 0;
}
