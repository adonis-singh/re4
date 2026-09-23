#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "sofdec.h"
#include "flag_rsf.h"
#include "global.h"
#include "game.h"
#include "main_sub.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj02.h"
#include "em.h"
#include "emhit.h"
#include "em_wrap.h"
#include "player.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "read.h"
#include "math_sub.h"

// Room 2-17 (D:/Bio4/Prog/r217.cpp): the timber (kakuzai) puzzle: five hit boxes on the beams,
// the lever that drops the hooked-up scaffold, the three enemy waves and the door.

struct R217Work {
    cEmWrap em[10];      // 0x000
    cEmHit* hit[5];      // 0x078  the beam hit boxes
    int cnt0;            // 0x08C
    int cnt1;            // 0x090
    int cnt2;            // 0x094
    int idx0;            // 0x098
    int side1;           // 0x09C
    int idx2;            // 0x0A0
    int timer;           // 0x0A4
    Vec pos[76];         // 0x0A8  saved scale of the scaffold objects
};


static Vec r217_savePos[76];
static R217Work* r217_work;

static u8 r217_objTbl[76] = {
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73,
};
static int r217_emTbl[10] = {0xDB, 0xDC, 0xDD, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x82, 0x83};
// hit box size (w, h, d) and offset (y, z): one-element arrays, so the (in-struct) loads stay below the hit store
static f32 r217_cubeW[1] = {70.0f};
static f32 r217_cubeH[1] = {140.0f};
static f32 r217_cubeD[1] = {480.0f};
static f32 r217_cubeY[1] = {-70.0f};
static f32 r217_cubeZ[1] = {0.0f};
static Vec r217_pos0[4] = {
    {5593.7f, 13062.0f, -6484.2f},
    {-2430.0f, 8021.1f, 4087.2f},
    {-6125.0f, 8021.1f, -6583.2f},
    {5026.0f, 8021.1f, -6853.2f},
};
static Vec r217_pos1[4] = {
    {-5730.0f, 8021.1f, 4087.2f},
    {-5593.7f, 8021.0f, -6484.2f},
    {6125.0f, 8021.1f, -6583.2f},
    {5026.0f, 8021.1f, 6853.2f},
};
static f32 r217_switchAcc = 0.008f;
static f32 r217_pad = 0.0f;

static void r217_2nd_set();
static void r217_1st_set();
static void r217_3rd_set();
static void r217_close_door();
void r217_open_door();
void KakuzaiMove(f32 dy);
static void r217_Puzzle_exit();
static void r217_Puzzle();
static void r217_hikkakari_move();
extern "C" int SwitchExec(cObj* obj, f32* spd, int no, f32 lim, f32 cur);


// The scaffold objects keep their scale in the first Vec of the object work.
#define R217_OBJ_VEC(o) ((Vec*) ((cObjScr*) (o))->free)

// Upper 16 bits of cEm::flags_324 set = dead.
static inline int r217_emDead(cEm* e)
{
    int dead = 1;

    if (!e->dmg.m_Flag && !e->dmg.m_Timer) {
        dead = 0;
    }
    return dead;
}

// Room init: Room_flg bits 2/3 preset (beams 0/1 count as done); each beam hit box (Room_flg bits 2..6
// clear) is a cEmHit cube on scroll object 0x83+i, else the beam is hidden; the 76 scaffold objects'
// scales saved. Before the scaffold dropped (Scenario_flg[1] 0x40000000): area 5 = the lever puzzle,
// enemy 0x11 pre-read, area 1 = the closed-door message; after: the lever posed, the third wave area 9,
// the first three Ganados, the door object 0x26 raised.
void R217Init()
{
#line 174 "D:/Bio4/Prog/r217.cpp"
    r217_work = (R217Work*) MEM_CALLOC(sizeof(R217Work), 1, 0xd);
    RsfSet(G_ROOM_ID, 2);
    RsfSet(G_ROOM_ID, 3);
    for (u32 i = 0; i < 5; i++) {
        if (RsfCheck(G_ROOM_ID, i + 2) == 0) {
            r217_work->hit[i] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                         &SmdGetObjPtr(0x83 + i)->pos, &SmdGetObjPtr(0x83 + i)->ang, 0);
            YarareInitCube(r217_work->hit[i], 0.0f, r217_cubeY[0], r217_cubeZ[0], r217_cubeW[0], r217_cubeH[0], r217_cubeD[0], 0, YAT_FLAG_ON);
        } else {
            SmdGetObjPtr(0x83 + i)->be_flag &= ~2;
        }
    }
    for (u32 j = 0; j < 76; j++) {
        PSVECScale(R217_OBJ_VEC(SmdGetObjPtr(r217_objTbl[j])), &r217_work->pos[j], 1.0f);
    }
    if (!ScfFlagChk(pG, SCF_R217_PUZZLE_CLEAR)) {
        for (u32 k = 0; k < 76; k++) {
            SmdGetObjPtr(r217_objTbl[k])->be_flag &= ~0x20;
        }
        SeAtSetOnOff(0, 0);
        SeAtSetOnOff(1, 0);
        SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r217_Puzzle, 0, 1);
        EmReadSearch(0x11, 0, 0);
    } else {
        SmdGetObjPtr(0x88)->be_flag |= 0x20;
        SmdGetObjPtr(0x88)->pList->ang.x = 1.6f;
        SceAtSetEnable(5, 0);
        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r217_3rd_set, 0, 1);
        for (u32 n = 0; n < 3; n++) {
            r217_work->em[n].setEm(r217_emTbl[n], -1, 0, 1, 1);
        }
    }
    if (!ScfFlagChk(pG, SCF_R217_PUZZLE_CLEAR)) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r217_close_door, 0, 1);
    } else {
        SmdGetObjPtr(0x26)->be_flag |= 0x20;
        SmdGetObjPtr(0x26)->pos.y = 6790.0f;
    }
}

// Per frame: after the drop, the second wave when Room_flg[2] bit 31 (once, bit 1); each beam hit box
// that was shot (or debug trigger 1) is marked (bits 2..6) with SE / effect and its beam hidden, all five
// -> bit 8. Then the crossbow Ganados' repositioning: em[0]/em[2] hop between the r217_pos tables after
// two / three shots at the player's level, em[1] alternates sides every 240 frames, all three re-alerted
// every 360 frames.
void R217Main()
{
    u32 i;
    u32 done;

    if (ScfFlagChk(pG, SCF_R217_PUZZLE_CLEAR)) {
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            if (pG->Room_flg[2] & 0x80000000) {
                RsfSet(G_ROOM_ID, 1);
                SceExec(0x12, (TaskFunc) r217_2nd_set, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    done = 0;
    for (i = 0; i < 5; i++) {
        if (RsfCheck(G_ROOM_ID, i + 2) == 0) {
            if (r217_work->hit[i]->ckStatus() == 1 || DebugTrg(1) != 0) {
                Vec v;

                RsfSet(G_ROOM_ID, i + 2);
                v = r217_work->hit[i]->ang;
                v.y += 1.5707964f;
                SndCall(6, 4, &r217_work->hit[i]->pos, 0, 0, 0);
                EstSet(0, -1, &r217_work->hit[i]->pos, &v, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
                SmdGetObjPtr(0x83 + i)->be_flag &= ~2;
            }
        } else {
            done++;
        }
    }
    if (done == 5) {
        RsfSet(G_ROOM_ID, 8);
    }
    if (ScfFlagChk(pG, SCF_R217_PUZZLE_CLEAR)) {
        int hit;
        cEm* e;

        if (!(pG->Room_flg[0] & 0x80000000)) {
            if (r217_work->em[0].ckFindPL()) {
                r217_work->em[0].setFlag(1);
            }
        }
        r217_work->timer--;
        if (r217_work->timer <= 0) {
            r217_work->timer = 360;
            r217_work->em[0].setFlag(1);
            r217_work->em[1].setFlag(1);
            r217_work->em[2].setFlag(1);
        }
        hit = 0;
        e = r217_work->em[0].getPtr();
        if (e) {
            if (r217_emDead(e)) {
                if (r217_work->cnt0 != 0) {
                    hit = 1;
                }
            }
        }
        if (r217_work->em[0].ckBowgunFire() == 1 || hit) {
            if (__builtin_fabsf(pPL->pos.y - r217_work->em[0].getPosY()) < 1000.0f) {
                r217_work->cnt0++;
                if (r217_work->cnt0 > 1) {
                    R217Work* w;

                    r217_work->cnt0 = 0;
                    r217_work->em[0].setGoto(&r217_pos0[r217_work->idx0], 1);
                    w = r217_work;
                    if (w->idx0 > 2) {
                        w->idx0 = 1;
                    } else {
                        w->idx0++;
                    }
                }
            }
        }
        if (__builtin_fabsf(pPL->pos.y - r217_work->em[1].getPosY()) < 1000.0f) {
            r217_work->cnt1--;
            if (r217_work->cnt1 < 0) {
                if (r217_work->side1 == 0) {
                    r217_work->side1 = 1;
                    r217_work->em[1].setGoto(&r217_pos1[0], 0xC);
                    r217_work->cnt1 = 240;
                } else if (r217_work->side1 == 1) {
                    r217_work->side1 = 0;
                    r217_work->em[1].setGoto(&r217_pos1[1], 0xC);
                    r217_work->cnt1 = 240;
                }
            }
        }
        e = r217_work->em[2].getPtr();
        if (e) {
            if (r217_emDead(e)) {
                if (r217_work->cnt2 != 0) {
                    hit = 1;
                }
            }
        }
        if (r217_work->em[1].ckBowgunFire() == 1 || hit) {
            if (__builtin_fabsf(pPL->pos.y - r217_work->em[2].getPosY()) < 1000.0f) {
                r217_work->cnt2++;
                if (r217_work->cnt2 > 2) {
                    R217Work* w;

                    r217_work->cnt2 = 0;
                    r217_work->em[2].setGoto(&r217_pos1[r217_work->idx2], 0xC);
                    w = r217_work;
                    if (w->idx2 > 2) {
                        w->idx2 = 1;
                    } else {
                        w->idx2++;
                    }
                }
            }
        }
    }
    if (RsfCheck(G_ROOM_ID, 1)) {
        if ((u32) SceCountEmAlive(0x10, 0x20) <= 4) {
            if (!(pG->Room_flg[0] & 0x02000000)) {
                pG->Room_flg[0] |= 0x02000000;
                Vec p = {-5674.0f, 0.0f, -8930.0f};
                r217_work->em[8].setGoto(&p, 1);
                r217_work->em[9].setGoto(&pPL->pos, 1);
            }
        }
    }
}

// The second wave: shout SEs, Ganados em[3..9] (table entries 3..9) spawn alerted; em[3] runs at the
// player, em[7] runs to a fixed point under camera cut 4, then cut 9 while em[8] is turned to face 2.99
// rad; cutscene ends after the camera motions.
static void r217_2nd_set()
{
    SndCall(6, 6, 0, 0, 0, 0);
    SceSleep(30);
    SndCall(6, 7, 0, 0, 0, 0);
    SceEventStart(1);
    for (u32 i = 3; i < 8; i++) {
        r217_work->em[i].setEm(r217_emTbl[i], -1, 0, 1, 1);
        r217_work->em[i].setFlag(1);
        r217_work->em[i].setNoSuspend(1);
    }
    for (u32 i = 8; i < 10; i++) {
        r217_work->em[i].setEm(r217_emTbl[i], -1, 0, 1, 1);
        r217_work->em[i].setNoSuspend(1);
    }
    r217_work->em[3].setGoto(&pPL->pos, 8);
    Vec p = {-5674.0f, 0.0f, -8930.0f};
    Vec ang;
    CamCtrl.CutCall(4);
    SceSleep(30);
    r217_work->em[7].setGoto(&p, 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(9);
    for (u32 i = 0; i < 20; i++) {
        cEmWrap* e = &r217_work->em[8];
        Vec* pa = &ang;
        f32 y = 2.99f;

        ang.x = 0.0f;
        pa->y = y;
        ang.z = 0.0f;
        e->setAng(pa);
        SceSleep(1);
    }
    StaFlagOff(pG, STA_SUSPEND);
    pPL->dmg.set(0, 0x80);
    for (u32 i = 8; i < 10; i++) {
        r217_work->em[i].setFlag(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        Vec* pa = &ang;
        f32 y = 2.99f;

        ang.x = 0.0f;
        pa->y = y;
        ang.z = 0.0f;
        r217_work->em[8].setAng(pa);
        SceSleep(1);
    }
    for (u32 i = 3; i < 8; i++) {
        r217_work->em[i].setNoSuspend(0);
    }
    for (u32 i = 8; i < 10; i++) {
        r217_work->em[i].setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    pPL->dmg.clear();
}

// Area 0xA after the drop: the first three Ganados (table entries 0..2, the crossbow men on the beams).
static void r217_1st_set()
{
    u32 i;

    for (i = 0; i < 3; i++) {
        r217_work->em[i].setEm(r217_emTbl[i], -1, 0, 1, 1);
    }
}

// Area 9 (the third wave): empty in this build.
static void r217_3rd_set()
{
}

// Area 1 before the drop: message 2 (the door will not open).
static void r217_close_door()
{
    SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
}

// The door rises (cut 10).
void r217_open_door()
{
    f32 spd;
    f32 max;

    CamCtrl.CutCall(0xA);
    spd = 0.0f;
    max = 50.0f;
    SndCall(6, 8, &SmdGetObjPtr(0x26)->pos, 0, 0, 0);
    SmdGetObjPtr(0x26)->be_flag |= 0x20;
    while (SmdGetObjPtr(0x26)->pos.y < 6790.0f) {
        spd += (max - spd) * 0.1f;
        SmdGetObjPtr(0x26)->pos.y += spd;
        SceSleep(1);
    }
    SndCall(6, 9, &SmdGetObjPtr(0x26)->pos, 0, 0, 0);
    SceSleep(15);
    SceAtDataReset(1);
}

// Moves the five beams by dy.
void KakuzaiMove(f32 dy)
{
    u32 id;

    for (id = 0x83; id <= 0x87; id++) {
        SmdGetObjPtr(id)->be_flag |= 0x20;
        SmdGetObjPtr(id)->pos.y += dy;
    }
}

// End of the lever puzzle: camera back, SceEventEnd; when the scaffold dropped (Scenario_flg[1]
// 0x40000000): Scenario_flg[3] 0x40, the door object 0x26 raised to y 6790, the attribute sounds back
// on, the scaffold objects restored to their saved scale, areas 0xA/9 = the waves, area 5 off, autosave,
// then the two shout SEs.
static void r217_Puzzle_exit()
{
    u32 i;

    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (ScfFlagChk(pG, SCF_R217_PUZZLE_CLEAR)) {
        ScfFlagOn(pG, SCF_79);
        SmdGetObjPtr(0x26)->be_flag |= 0x20;
        SmdGetObjPtr(0x26)->pos.y = 6790.0f;
        SceAtDataReset(1);
        SndSePauseAll(0);
        SeAtSetOnOff(0, 1);
        SeAtSetOnOff(1, 1);
        for (i = 0; i < 76; i++) {
            Vec* v = R217_OBJ_VEC(SmdGetObjPtr(r217_objTbl[i]));

            PSVECScale(&r217_work->pos[i], v, 1.0f);
            SmdGetObjPtr(r217_objTbl[i])->be_flag |= 0x20;
        }
        SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r217_1st_set, 0, 1);
        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r217_3rd_set, 0, 1);
        SceAtSetEnable(5, 0);
        GameSave.save(pSaveData, -1);
        SceSleep(90);
        SndCall(6, 6, 0, 0, 0, 0);
        SceSleep(30);
        SndCall(6, 7, 0, 0, 0, 0);
    }
}

// The lever: either the scaffold drops (cuts 6..8, the movie) or it just rattles.
static void r217_Puzzle()
{
    u32 i;
    u32 k;

    SceEventStart(0);
    CamCtrl.CutCall(3);
    f32 spd = 0.0f;
    SmdGetObjPtr(0x88)->be_flag |= 0x20;
    SndCall(6, 5, 0, 0, 0, 0);
    while (SwitchExec(SmdGetObjPtr(0x88), &spd, 2, 1.6f, 0.0f) == 0) {
        SceSleep(1);
    }
    SceSleep(30);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        CamCtrl.CutCall(6);
        SndCall(6, 3, 0, 0, 0, 0);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(9);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(4);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(5);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(6);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(3);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(9);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(4);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(6);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        SceSleep(5);
        SceExec(0x12, (TaskFunc) r217_hikkakari_move, 0, 0, SCE_PRIO_DEF_2, 0);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        SmdGetObjPtr(0x88)->pList->ang.x = 0.0f;
    } else {
        ScfFlagOn(pG, SCF_R217_PUZZLE_CLEAR);
        SceSetEventCancel(1, (TaskFunc) r217_Puzzle_exit, 0, -1, 1);
        CamCtrl.CutCall(7);
        SceSleep(5);
        for (i = 0; i < 76; i++) {
            PSVECScale(R217_OBJ_VEC(SmdGetObjPtr(r217_objTbl[i])), &r217_savePos[i], -1.0f);
            SmdGetObjPtr(r217_objTbl[i])->be_flag |= 0x20;
        }
        for (k = 0; k < 11; k++) {
            for (i = 0; i < 76; i++) {
                cObj* o = SmdGetObjPtr(r217_objTbl[i]);

                PSVECScale(&r217_savePos[i], R217_OBJ_VEC(o), (f32) k * 0.1f);
                SmdGetObjPtr(r217_objTbl[i])->be_flag |= 0x20;
            }
            SceSleep(1);
        }
        SeAtSetOnOff(0, 1);
        SeAtSetOnOff(1, 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(8);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceAtSetEnable(5, 0);
        SndSePauseAll(1);
        CamCtrl.CutCall(0xA);
        systemVISetBlack(1);
        Sofdec.Initialize("movie/r214_ev.sfd", 0);
        SceSleep(1);
        SeAtSndCall(0);
        SeAtSndCall(1);
        r217_open_door();
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r217_Puzzle_exit();
}

// One rattle of the scaffold: the objects shrink and the beams shake.
static void r217_hikkakari_move()
{
    u32 i;

    for (i = 0; i < 76; i++) {
        Vec* v = R217_OBJ_VEC(SmdGetObjPtr(r217_objTbl[i]));

        PSVECScale(v, v, -0.5f);
    }
    for (i = 0; i < 76; i++) {
        SmdGetObjPtr(r217_objTbl[i])->be_flag |= 0x20;
    }
    KakuzaiMove(-7.0f);
    SceSleep(1);
    KakuzaiMove(-3.0f);
    SceSleep(1);
    for (i = 0; i < 76; i++) {
        SmdGetObjPtr(r217_objTbl[i])->be_flag &= ~0x20;
    }
    SceSleep(1);
    for (i = 0; i < 76; i++) {
        SmdGetObjPtr(r217_objTbl[i])->be_flag |= 0x20;
    }
    for (i = 0; i < 76; i++) {
        Vec* v = R217_OBJ_VEC(SmdGetObjPtr(r217_objTbl[i]));

        PSVECScale(v, v, -1.0f);
    }
    KakuzaiMove(3.0f);
    SceSleep(1);
    KakuzaiMove(7.0f);
    SceSleep(1);
    for (i = 0; i < 76; i++) {
        Vec* v = R217_OBJ_VEC(SmdGetObjPtr(r217_objTbl[i]));

        PSVECScale(v, v, 2.0f);
    }
    for (i = 0; i < 76; i++) {
        SmdGetObjPtr(r217_objTbl[i])->be_flag &= ~0x20;
    }
    SceSleep(60);
}

// Turns the lever's parts towards `lim` with the accelerating speed `*spd`; 1 when it arrived (r10c).
extern "C" int SwitchExec(cObj* obj, f32* spd, int no, f32 lim, f32 cur)
{
    int dir;

    if (lim > cur) {
        obj->pList->ang.x += *spd;
        dir = 1;
    } else {
        obj->pList->ang.x -= *spd;
        dir = 0;
    }
    if (*spd >= 0.0f) {
        if (dir ? (obj->pList->ang.x < lim) : (obj->pList->ang.x > lim)) {
            *spd += r217_switchAcc * 1.85f;
        } else {
            *spd = -r217_switchAcc;
            obj->pList->ang.x = lim;
            return 1;
        }
    }
    return 0;
}
