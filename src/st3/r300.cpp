#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "map_obj.h"
#include "widget.h"
#include "event.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "emrock.h"
#include "em_set.h"
#include "em_wrap.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "model.h"
#include "datactrl.h"
#include "act_btn.h"
#include "mes.h"
#include "game.h"
#include "snd.h"
#include "fade.h"
#include "shadow.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "rnd.h"
#include "math_sub.h"
#include "vec.h"
#include "TexRender.h"
#include "db_log.h"

// Room 3-00 (D:/Bio4/Prog/r300.cpp): the island landing. The searchlight that follows its target, the two
// mirrors the player turns to redirect the laser onto the gate, the Ashley "asl" scene, the found-by-camera
// event, the rock, the enemy waves and their reset counter, and the two render-to-texture water objects.

struct R300Work {
    TexRenderMng* tex[2];   // 0x000
    cEmWrap em[22];         // 0x008  [20] the event Ganado, [21] its replacement
    cObj* smd;              // 0x110  searchlight housing (SetObjSmd)
    cObj* asl;              // 0x114  Ashley model of the asl scene (modelSet)
    Vec lightAng;           // 0x118  housing sway angles
    cEmRock* rock;          // 0x124
    Vec target;             // 0x128  searchlight target
    f32 sweepAng;           // 0x134
    f32 sweepAng2;          // 0x138
    f32 miraAng;            // 0x13C  mirror A rotation target
    f32 mirbAng;            // 0x140  mirror B rotation target
    CAMERA cam;             // 0x144  mirror event camera
    cEmHit* hit;            // 0x23C
    int x240;               // 0x240
    Vec laserPos[2];        // 0x244  laser reflection points
    int sirenTimer;         // 0x25C
    int cnt;                // 0x260  frames the laser has been on the gate
    cEmWrap aslEm;          // 0x264
    f32 doorY;              // 0x270
    u32 se[2];              // 0x274
    int x27C;               // 0x27C
    Vec plPos;              // 0x280  player position saved over the em_set event
    u32 strId;              // 0x28C
    cDataUnit* data[6];     // 0x290  asl model files
    u8 pad_2A8[0x2D0 - 0x2A8];
};

// The beam effect's second end point (past cEsp).
struct R300EspView {
    u8 pad_0[0x100];
    Vec pos2;   // 0x100
};

extern "C" void* r300_memset(void*, ...) asm("memset");


// Reference store: the work pointer and the field are reloaded after it.

// Room id through the struct-member view of pG: the load stays below a preceding member store.
#define GS_ROOM_ID (*(u16*) &pG->stage_no)

static u8 r300_texTbl0[0x20];
static u8 r300_texTbl1[0x20];

static R300Work* r300_work;

// Mirror rotation targets (radians): A default / A on the gate / unused; B on the gate / unused.
static f32 r300_miraAng[6] = {3.7768784f, 2.99148f, 2.2060819f, 1.4206837f, 1.5360988f, 1.2742994f};
static f32 r300_mirbAng[2] = {1.0125f, 0.75070065f};
// Laser path: the emitter and the three target points (only the first is used).
static Vec r300_laser[4] = {{-8993.0f, -10906.0f, -38000.0f}, {-33246.0f, -10906.0f, -40283.0f}, {-20651.0f, -10906.0f, -19685.0f}, {-8992.0f, -10906.0f, -30030.0f}};
static Vec r300_lightTarget = {-30199.0f, -18000.0f, -14034.0f};
static Vec r300_sweepA = {-17458.0f, -18000.0f, -8171.0f};
static Vec r300_sweepB = {-30199.0f, -18000.0f, -14034.0f};
static f32 r300_sweepSpd = 0.007f;
static Vec r300_sweepC = {-26030.0f, -17612.0f, -9145.0f};
static Vec r300_sweepD = {-12080.0f, -17612.0f, -11880.0f};
static f32 r300_sweepSpd2 = 0.031f;
static f32 r300_mirRate = 0.2f;
static f32 r300_laserLen = 0.989f;
static f32 r300_laserLen2 = 0.66f;

// Hit effects of attribute type 2
static const AtEffInfo r300_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

void setResetNum(int n);
int getResetNum();
void incResetNum();
cEm* emset(int no);
void R300emReset();
// The original's setTexRender has C linkage (an unmangled local in the .sym).
extern "C" {
static void setTexRender();
}
static void R300_Event();
static void Evt_R300S00_Func(Event* e);
static void r300_em_reset_task();
static void r300_find_camera_event_exit();
static void r300_find_camera_event();
static void r300_find_camera();
void setAslPos(cObj* obj);
static void r300_asl_exit();
static void r300_asl();
static void r300_find_siren();
void setSearchLightTarget(cObj* light, Vec* target, cObj* light2);
static void r300_mira_exec();
static void r300_mirb_exec();
static void DoorOpen_exit();
static void DoorOpen();
void DrawLaserLine(Vec* from, Vec* to, int r, int g, int b, int a, int type, f32 len);
static void r300_laser_start_exit();
static void r300_laser_start();
static void r300_laser_exec();
static void r300_laser2_exec();
static void r300_laser_door_exec();
static void r300_em_set_exit();
static void r300_em_set();
static void r300_em_set_last();
void modelLoad();
void modelSet();
static void r300_StrCheck();

// The number of enemy resets is kept in room flags 10..12.
void setResetNum(int n)
{
    if (n & 1) {
        RsfSet(G_ROOM_ID, 10);
    } else {
        RsfClear(G_ROOM_ID, 10);
    }
    if (n & 2) {
        RsfSet(G_ROOM_ID, 11);
    } else {
        RsfClear(G_ROOM_ID, 11);
    }
    if (n & 4) {
        RsfSet(G_ROOM_ID, 12);
    } else {
        RsfClear(G_ROOM_ID, 12);
    }
}

// The enemy reset counter (0..7) read back from room save flags 10..12.
int getResetNum()
{
    int n = 0;

    if (RsfCheck(G_ROOM_ID, 10)) {
        n += 1;
    }
    if (RsfCheck(G_ROOM_ID, 11)) {
        n += 2;
    }
    if (RsfCheck(G_ROOM_ID, 12)) {
        n += 4;
    }
    return n;
}

// Advance the reset counter.
void incResetNum()
{
    setResetNum(getResetNum() + 1);
}


// Spawn list entry `no` (any list) already alerted; the raw enemy pointer.
cEm* emset(int no)
{
    cEm* em = setEm(no, -1, 1, 1, 1);

    if (em) {
        ((cEmGanado*) em)->setFindPL();
    }
    return em;
}

// The reset wave: one more alerted Ganado per reset count (0x44, 0x3C, 0xA, 0x3E, ... in order), then
// the counter advances.
void R300emReset()
{
    cEm* em = 0;

    switch ((u32) getResetNum()) {
    case 0:
        em = emset(0x44);
        break;
    case 1:
        em = emset(0x3C);
        break;
    case 2:
        em = emset(0xA);
        break;
    case 3:
        em = emset(0x3E);
        break;
    case 4:
        em = emset(0xC);
        break;
    case 5:
        em = emset(0x4C);
        break;
    case 6:
        em = emset(0x3D);
        break;
    case 7:
        em = emset(0x4F);
        break;
    }
    if (em) {
        incResetNum();
    }
}

// Puts `em` at list entry `l` (the Vecs are the inline's own, shared by both copies).
static inline void r300_setEmAngR(cEmWrap* em, f32 ry)
{
    Vec ang;

    ang.x = 0.0f;
    ang.y = ry;
    ang.z = 0.0f;
    em->setAng(&ang);
}

// An ESL entry's position (1/10 units -> world) and yaw (rot[1] in 1/32768 turns -> degrees).
static inline void r300_getListPos(EmListData* l, Vec* pos, f32& ry)
{
    pos->x = (f32) l->pos[0] * 10.0f;
    pos->y = (f32) l->pos[1] * 10.0f;
    pos->z = (f32) l->pos[2] * 10.0f;
    ry = (f32) (l->rot[1] * 360 / 32768);
}

// Set an enemy's rotation to a Y-only yaw through the caller's Vec.
static inline void r300_setEmAng(cEmWrap* em, Vec* ang, f32 ry)
{
    ang->x = 0.0f;
    ang->y = ry;
    ang->z = 0.0f;
    em->setAng(ang);
}

// Room init (the island landing): Debug_flg[1] 0x20000; JumpPoint skips the landing event; the s00
// (and s99) callback; two Key_flg[1] bits; the water render targets; the player's room motions;
// the searchlight objects and the dropping rock (until Room_flg bit 4); the landing event once (bit
// 0); area 1 = Ashley carried through the gate (bit 3), area 0xF = the camera post (bit 2); the gate
// already burnt open (bit 5) or the two mirrors (areas 6/7) and the laser start (area 0xB) / the laser
// look areas (bit 7); the gate Ganado events (areas 13/14/16), the reset waves and the stream.
void R300Init()
{
#line 185 "D:/Bio4/Prog/r300.cpp"
    r300_work = (R300Work*) MEM_CALLOC(sizeof(R300Work), 1, 0xd);
    DbgFlagOn(pG, DBG_EMW_ERR_NO_DISP);
    if (pG->JumpPoint != 0) {
        RsfSet(G_ROOM_ID, 0);
    }
    EvtMgr.SetFunc("evt_r300s00_func", (void*) Evt_R300S00_Func);
    EvtMgr.SetFunc("evt_r300s99_func", (void*) Evt_R300S00_Func);
    KyfFlagOn(pG, KYF_ST1_17);
    KyfFlagOn(pG, KYF_ST1_25);
    setTexRender();
    PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    {
        Vec pos = {40449.0f, -20000.0f, -40999.0f};
        Vec rot = {0.0f, -2.19f, 0.0f};

        r300_work->smd = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x2A), ROOM_ARC_PTR(pG->pRoom, 0x2B), &pos, &rot, 0x10, 1);
        r300_work->smd->setNoSuspend(0);
    }
    EatMgr.registEffInfo(2, (AtEffInfo*) &r300_eff_info);
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        const f32 w = -250.0f;
        const f32 h = 450.0f;

        EstSet(SmdGetObjPtr(0x37), -1, 0, 0, EFF_ROOM, 6, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        r300_work->hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore),
                                    &SmdGetObjPtr(0x37)->pos, &SmdGetObjPtr(0x37)->ang, 0);
        YarareInitCube(r300_work->hit, 0.0f, w, 0.0f, h, h, h, 0, YAT_FLAG_ON);
    } else {
        LightMgr.offKind(1);
    }
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        Vec pos;
        Vec rot;

        pos.x = -5451.59f;
        pos.y = -12058.55f;
        pos.z = -13416.52f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        r300_work->rock = SetRock(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &pos, &rot, 3);
        if (r300_work->rock) {
            r300_work->rock->setDropMot2(ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24),
                                         ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x25),
                                         ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28),
                                         ROOM_ARC_PTR(pG->pRoom, 0x29));
            r300_work->rock->setNoSuspend(1);
        }
        {
            EmListData d;

            d.id = 0x1D;
            d.type = 0xE;
            d.set = 0x1C;
            d.flag = 0;
            d.pos[0] = -0x24D;
            d.pos[1] = -0x2B2;
            d.pos[2] = -0x453;
            d.rot[0] = 0;
            d.rot[1] = 0x3DDD;
            d.rot[2] = 0;
            d.hp = 0;
            d.Guard_r = 1;
            d.Character = 1;
            EmSetEvent(&d);

            d.id = 0x1D;
            d.type = 0xF;
            d.set = 0x1C;
            d.flag = 0;
            d.pos[0] = -0x292;
            d.pos[1] = -0x2B2;
            d.pos[2] = -0x45E;
            d.rot[0] = 0;
            d.rot[1] = 0x2FA4;
            d.rot[2] = 0;
            d.hp = 0;
            d.Guard_r = 1;
            d.Character = 1;
            EmSetEvent(&d);

            d.id = 0x1D;
            d.type = 0x10;
            d.set = 0x1C;
            d.flag = 0;
            d.pos[0] = -0x1E8;
            d.pos[1] = -0x2B2;
            d.pos[2] = -0x470;
            d.rot[0] = 0;
            d.rot[1] = -0x11C7;
            d.rot[2] = 0;
            d.hp = 0;
            d.Guard_r = 1;
            d.Character = 1;
            EmSetEvent(&d);
        }
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceExec(0x12, (TaskFunc) R300_Event, 0, 0, 2, 0);
    } else {
        SetShadowCamMoveSize(0.0f);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        memclr_asm(pG->item_save, 0x1000);
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) r300_asl, 0, 1);
        r300_work->sirenTimer = (u8) (Rnd() % 60u);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(0xF, 0x12, 0, (TaskFunc) r300_find_camera, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 5)) {
        SmdGetObjPtr(0x42)->be_flag |= 0x20;
        SmdGetObjPtr(0x42)->pos.y += 3000.0f;
        SceAtSetEnable(8, 0);
        SmdGetObjPtr(0x3C)->ang.y = r300_work->miraAng = r300_miraAng[1];
        SmdGetObjPtr(0x3D)->ang.y = r300_work->mirbAng = r300_mirbAng[0];
        pG->Room_flg[0] |= 0x40000000;
        r300_work->cnt = 1;
        SceExec(0x12, (TaskFunc) r300_StrCheck, 0, 0, 2, 0);
        SceAtSetEnable(6, 0);
        SceAtSetEnable(7, 0);
    } else {
        r300_work->miraAng = r300_miraAng[0];
        r300_work->mirbAng = 0.85f;
        SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r300_mira_exec, 0, 1);
        SceAtDataSet_exec(7, 0x12, 0, (TaskFunc) r300_mirb_exec, 0, 1);
        if (RsfCheck(G_ROOM_ID, 7)) {
            SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) r300_laser_exec, 0, 1);
            SceAtDataSet_exec(0xA, 0x12, 0, (TaskFunc) r300_laser2_exec, 0, 1);
            SceAtDataSet_exec(0xC, 0x12, 0, (TaskFunc) r300_laser_door_exec, 0, 1);
        } else {
            SceAtDataSet_exec(0xB, 0x12, 0, (TaskFunc) r300_laser_start, 0, 1);
        }
    }
    if (RsfCheck(G_ROOM_ID, 7)) {
        EstSet(0, -1, 0, 0, EFF_ROOM, 9, 1, ESP_CORE_KIND_NONE, 0, 0);
    }
    if (SysFlagChk(pG, SYS_CONTINUE) && RsfCheck(G_ROOM_ID, 5) == 0) {
        SndRoomStrStart(1, 0, 1);
    }
    {
        Vec target;

        r300_work->target = r300_lightTarget;
        target = r300_lightTarget;
        setSearchLightTarget(SmdGetObjPtr(0x37), &target, SmdGetObjPtr(0x36));
    }
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        SceAtDataSet_exec(0xD, 0x12, 0, (TaskFunc) r300_em_set, 0, 1);
        SceAtDataSet_exec(0xE, 0x12, 0, (TaskFunc) r300_em_set, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        SceAtDataSet_exec(0x10, 0x12, 0, (TaskFunc) r300_em_set_last, 0, 1);
    } else {
        cEmWrap em[2];

        em[0].setEm(3, -1, 1, 1, 1);
        em[1].setEm(4, -1, 1, 1, 1);
        if (pG->room_id_prev == 0x301) {
            EmListData* l;
            f32 ry;

            Vec pos;

            l = &pG->Em_list[0x65];
            r300_getListPos(l, &pos, ry);
            em[0].setPos(&pos);
            r300_setEmAngR(&em[0], ry);
            l = &pG->Em_list[0x55];
            r300_getListPos(l, &pos, ry);
            em[1].setPos(&pos);
            r300_setEmAngR(&em[1], ry);
        }
    }
    modelLoad();
}

// Per frame: with exactly three Ganados alive and the rock still hanging, the rock is dropped when the
// player is under it (flag bit 0); the searchlight target sweeps between the sweep points until the
// camera found the player (Room_flg bit 2), then follows the player (Room_flg[2] bit 31) or stays;
// the light and beam objects are aimed at the smoothed target.
void R300Main()
{
    Vec v0;
    Vec v1;
    Vec v2;
    Vec v3;
    Vec v4;
    Vec v5;
    Vec v6;

    if (SceCountEmAlive(0x10, 0x20) == 3 && r300_work->rock && (s16) pG->pl_life > 0) {
        int skip = 1;

        if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
            skip = 0;
        }
        if (skip == 0) {
            if ((pPL->pos.x - r300_work->rock->pos.x) * (pPL->pos.x - r300_work->rock->pos.x) +
                    (pPL->pos.z - r300_work->rock->pos.z) * (pPL->pos.z - r300_work->rock->pos.z) <
                6250000.0f) {
                RsfSet(G_ROOM_ID, 4);
                r300_work->rock->flag |= 1;
            }
        }
    }
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        if (r300_work->hit->ckStatus() == 1) {
            r300_work->hit->hp = 0;
            RsfSet(GS_ROOM_ID, 6);
            EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
            EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
            EstSet(0, -1, &SmdGetObjPtr(0x37)->pos, &SmdGetObjPtr(0x37)->ang, EFF_ROOM, 7, 1, ESP_CORE_KIND_NONE, 0, 0);
            LightMgr.offKind(1);
        }
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            f32 s;

            r300_work->sweepAng += r300_sweepSpd;
            r300_work->sweepAng = LIMIT_ANGLE(r300_work->sweepAng);
            s = SINF(r300_work->sweepAng) * 0.5f + 0.5f;
            PSVECScale(&r300_sweepA, &v1, s);
            PSVECScale(&r300_sweepB, &v2, 1.0f - s);
            PSVECAdd(&v1, &v2, &v0);
            r300_work->sweepAng2 += r300_sweepSpd2;
            r300_work->sweepAng2 = LIMIT_ANGLE(r300_work->sweepAng2);
            s = SINF(r300_work->sweepAng2) * 0.5f + 0.5f;
            PSVECScale(&r300_sweepC, &v1, s);
            PSVECScale(&r300_sweepD, &v2, 1.0f - s);
            PSVECAdd(&v1, &v2, &v3);
            PSVECAdd(&r300_sweepC, &r300_sweepD, &v4);
            PSVECScale(&v4, &v4, 0.5f);
            PSVECSubtract(&v3, &v4, &v3);
            PSVECAdd(&v0, &v3, &v0);
        } else if (pG->Room_flg[2] & 0x80000000) {
            v0 = pPL->pos;
            v0.y += 0.0f;
        } else {
            v0 = r300_work->target;
        }
        PSVECSubtract(&v0, &r300_work->target, &v1);
        PSVECScale(&v1, &v1, 0.035f);
        PSVECAdd(&r300_work->target, &v1, &r300_work->target);
        v2 = r300_work->target;
        setSearchLightTarget(SmdGetObjPtr(0x37), &v2, SmdGetObjPtr(0x36));
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0 && (pG->Room_flg[2] & 0x40000000)) {
        r300_work->sirenTimer--;
        if (r300_work->sirenTimer < 0) {
            r300_work->sirenTimer = (u8) (Rnd() % 180u) + 90;
            SndCall(6, 0x19, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
        }
    }
    if (pG->Room_flg[2] & 0x20000000) {
        if (!(pG->Room_flg[0] & 0x08000000)) {
            pG->Room_flg[0] |= 0x08000000;
            SmdSetTrans(0x13, 0);
            SmdSetTrans(0x15, 0);
            SmdSetTrans(0x16, 0);
            SmdSetTrans(0x1A, 0);
        }
    } else {
        if (pG->Room_flg[0] & 0x08000000) {
            pG->Room_flg[0] &= ~0x08000000;
            SmdSetTrans(0x13, 1);
            SmdSetTrans(0x15, 1);
            SmdSetTrans(0x16, 1);
            SmdSetTrans(0x1A, 1);
        }
    }
    r300_work->lightAng.x += 0.05f;
    r300_work->lightAng.y += 0.035f;
    r300_work->lightAng.z += 0.065f;
    r300_work->lightAng.x = LIMIT_ANGLE(r300_work->lightAng.x);
    r300_work->lightAng.y = LIMIT_ANGLE(r300_work->lightAng.y);
    r300_work->lightAng.z = LIMIT_ANGLE(r300_work->lightAng.z);
    r300_work->smd->pos.y = SINF(r300_work->lightAng.x) * 100.0f + -20000.0f;
    r300_work->smd->ang.x = SINF(r300_work->lightAng.y) * 0.05f;
    r300_work->smd->ang.z = COSF(r300_work->lightAng.z) * 0.01f;
    {
        cObj* obj;
        f32 a;

        obj = SmdGetObjPtr(0x3C);
        obj->be_flag |= 0x20;
        a = obj->ang.y + Muku2(obj->ang.y, r300_work->miraAng, 0.25f);
        obj->ang.y += (a - obj->ang.y) * r300_mirRate;
        obj = SmdGetObjPtr(0x3D);
        obj->be_flag |= 0x20;
        a = obj->ang.y + Muku2(obj->ang.y, r300_work->mirbAng, 0.25f);
        obj->ang.y += (a - obj->ang.y) * r300_mirRate;
    }
    pG->Room_flg[0] &= ~0x20000000;
    if (RsfCheck(GS_ROOM_ID, 7)) {
        f32 dAng;

        v0 = r300_laser[0];
        v1 = r300_laser[1];
        DrawLaserLine(&v0, &v1, 0xFF, 0, 0, 0x80, 0, 192000.0f);
        dAng = __builtin_fabsf(SmdGetObjPtr(0x3D)->ang.y - r300_mirbAng[0]);
        v0.y = 0.0f;
        v0.x = SINF(SmdGetObjPtr(0x3D)->ang.y);
        v0.z = COSF(SmdGetObjPtr(0x3D)->ang.y);
        PSVECSubtract(&r300_laser[1], &r300_laser[0], &v1);
        PSVECScale(&v0, &v2, PSVECDotProduct(&v0, &v1) * -2.0f);
        PSVECAdd(&v1, &v2, &v1);
        if (dAng < 0.0055f) {
            PSVECScale(&v1, &v1, r300_laserLen);
        } else {
            PSVECScale(&v1, &v1, r300_laserLen * 3.0f);
        }
        PSVECAdd(&v1, &r300_laser[1], &r300_work->laserPos[0]);
        v3 = r300_laser[1];
        v5 = r300_work->laserPos[0];
        DrawLaserLine(&v3, &v5, 0xFF, 0, 0, 0x80, 1, 128000.0f);
        pG->Room_flg[0] |= 0x20000000;
        if (dAng < 0.0055f) {
            if (!(pG->Room_flg[0] & 0x40000000)) {
                SndCall(6, 0x12, &SmdGetObjPtr(0x3C)->pos, 0, 0, 0);
            }
            pG->Room_flg[0] |= 0x40000000;
            dAng = __builtin_fabsf(SmdGetObjPtr(0x3C)->ang.y - r300_miraAng[1]);
            v0.y = 0.0f;
            v0.x = SINF(SmdGetObjPtr(0x3C)->ang.y);
            v0.z = COSF(SmdGetObjPtr(0x3C)->ang.y);
            PSVECSubtract(&r300_work->laserPos[0], &r300_laser[1], &v1);
            PSVECScale(&v0, &v2, PSVECDotProduct(&v0, &v1) * -2.0f);
            PSVECAdd(&v1, &v2, &v1);
            if (dAng < 0.0055f) {
                PSVECScale(&v1, &v1, r300_laserLen2);
            } else {
                PSVECScale(&v1, &v1, r300_laserLen2 * 3.0f);
            }
            PSVECAdd(&v1, &r300_work->laserPos[0], &r300_work->laserPos[1]);
            v3 = r300_work->laserPos[0];
            v6 = r300_work->laserPos[1];
            DrawLaserLine(&v3, &v6, 0xFF, 0, 0, 0x80, 1, 64000.0f);
            if (dAng < 0.0055f) {
                f32 m1;
                f32 m2;

                m1 = Muku2(SmdGetObjPtr(0x3C)->ang.y, r300_work->miraAng, 0.25f);
                m2 = Muku2(SmdGetObjPtr(0x3D)->ang.y, r300_work->mirbAng, 0.25f);
                if (__builtin_fabsf(m1) < 0.0055f && __builtin_fabsf(m2) < 0.0055f) {
                    r300_work->miraAng = r300_miraAng[1];
                    r300_work->mirbAng = r300_mirbAng[0];
                    r300_work->cnt++;
                    if (r300_work->cnt > 0x18) {
                        if (!(pG->Room_flg[0] & 0x80000000)) {
                            pG->Room_flg[0] |= 0x80000000;
                        }
                    }
                } else {
                    r300_work->cnt = 0;
                }
            } else {
                pG->Room_flg[0] &= ~0x80000000;
            }
        } else {
            pG->Room_flg[0] &= ~0x40000000;
            pG->Room_flg[0] &= ~0x80000000;
        }
    }
    if (pG->Room_flg[0] & 0x40000000) {
        if (!(pG->Room_flg[0] & 0x04000000)) {
            pG->Room_flg[0] |= 0x04000000;
            EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM01, 0, 0);
        }
    } else {
        if (pG->Room_flg[0] & 0x04000000) {
            pG->Room_flg[0] &= ~0x04000000;
            EffectEspDelete(1, ESP_CORE_KIND_ROOM01, 0, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_ROOM01, 0);
            EffectEfmDelete(1, ESP_CORE_KIND_ROOM01, 0);
        }
    }
}

// The two water surfaces: render targets blended into the water objects.
static void setTexRender()
{
    cObj* obj;
    u8* tbl0 = r300_texTbl0;
    u8* tbl1 = r300_texTbl1;

    if (GetTexRenderMgr(&r300_work->tex[0])) {
        tbl0[0] = 1;
        tbl0[1] = 0;
        tbl0[4] = 0xF7;
        tbl0[5] = r300_work->tex[0]->m_Tex_no;
        r300_work->tex[0]->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, r300_work->tex[0]->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "R300Init() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0xC);
    obj->pModelInfo->setTexBlendTbl(tbl0);
    obj->pModelInfo->setBlendRatio(0xFF);
    if (GetTexRenderMgr(&r300_work->tex[1])) {
        tbl1[0] = 1;
        tbl1[1] = 0;
        tbl1[4] = 0xF7;
        tbl1[5] = r300_work->tex[1]->m_Tex_no;
        r300_work->tex[1]->m_Rep_type = 1;
        {
            TexRenderMng* t = r300_work->tex[1];

            t->m_W_size = 0x20;
            t->m_H_size = 0x20;
        }
        EstSet(0, -1, 0, 0, EFF_ROOM, 4, r300_work->tex[1]->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "R300Init() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0xE);
    obj->pModelInfo->setTexBlendTbl(tbl1);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(1);
}

// The landing event (r300s00), then the player is put on the beach.
static void R300_Event()
{
    u32 zero = 0;

    SceEventStart(0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    SceSleep(1);
    EvtMgr.EvtReadAram("event/evd/r300s00.evd", 0x1D, 0, 0, 0);
    StaFlagOn(pG, STA_CAMERA_SET_ROOM);
    SceSleep(1);
    SceSleep(1);
    EvtMgr.EvtReadExec("event/evd/r300s00.evd", 0x1D, EvtReadFlagNone);
    {
        FadeColorPair col;

        *(u32*) &col.start = 0xFF;
        *(u32*) &col.end = zero;
        FadeSet(0x80000002, &col.start, &col.end, 60, 0, 0);
    }
    SceEventEnd(0);
    StaFlagOff(pG, STA_CAMERA_SET_ROOM);
    {
        Vec pos = {28550.0f, -17000.0f, -40370.0f};
        Vec rot = {0.0f, -2.23f, 0.0f};

        // The struct-member view of pPL (the r10c idiom): the `this` load is not a fixed scalar, so it
        // ranks below the two pointer-based pos word stores (true dependence, store latency 2) and
        // those outrank the rot word0 pool load, which then sinks below them like the target.
        pPL->setPos(&pos);
        pPL->setAng(&rot);
    }
    CamCtrl.Comeback(0);
    OpeSetOpenTerm(0x13, 0.0f, 0.0f, 0.0f, 0.0f);
    SetShadowCamMoveSize(0.0f);
    SndRoomStrStart(1, 0, 1);
    SndBgmTblSet(0x300, 3);
}

// Event r300s00 callback (the landing by boat): System_flg 0x800 off; the evm4000 boat and the other
// event models' draw flags per cut; the end restores the room.
static void Evt_R300S00_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        SysFlagOff(pG, SYS_SCISSOR_ON);
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "evm4000", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm6800", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ((cModel*) mod)->ot_type = 1;
                }
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    ((cModel*) mod)->ot_type = 1;
                }
                if (e->GetMod(&mod, "obm3600", 0, 0) == 1) {
                    cParts* p;

                    ((cModel*) mod)->be_flag |= 0x80;
                    p = ((cModel*) mod)->getPartsPtr(3);
                    if (p) {
                        p->scale.x = 0.0f;
                        p->scale.y = 0.0f;
                        p->scale.z = 0.0f;
                    }
                }
                if (e->GetMod(&mod, "obm4d00", 0, 0) == 1) {
                    ((cModel*) mod)->ot_type = 1;
                    ((cModel*) mod)->z_mode = 1;
                }
            }
            break;
        case 8:
        case 11:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 1, 0);
                }
            }
            break;
        case 7:
        case 9:
        case 10:
        case 12:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 1, 1);
                }
            }
            break;
        case 13:
            if (e->NowFrame == 0x52) {
                FadeSetW(2, 60, 0, 0);
            }
            break;
        }
        break;
    case 2:
        SysFlagOn(pG, SYS_SCISSOR_ON);
        break;
    }
}

// Task: whenever seven or fewer Ganados are alive, the next reset enemy comes (R300emReset).
static void r300_em_reset_task()
{
    for (;;) {
        if ((u32) SceCountEmAlive(0x10, 0x20) <= 7) {
            R300emReset();
        }
        SceSleep(1);
    }
}

// The camera event is over: the three Ganado run at the player.
static void r300_find_camera_event_exit()
{
    pPL->setNoSuspend(0);
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;

    em0.setEm(0x49, -1, 1, 1, 1);
    em1.setEm(0x4A, -1, 1, 1, 1);
    em2.setEm(5, -1, 1, 1, 1);
    em0.setNoSuspend(0);
    em1.setNoSuspend(0);
    em2.setNoSuspend(0);
    em0.setGoto(&pPL->pos, 0xC);
    em1.setGoto(&pPL->pos, 0xC);
    em2.setGoto(&pPL->pos, 0xC);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExec(0x12, (TaskFunc) r300_StrCheck, 0, 0, 2, 0);
}

// Found by the camera: the siren, the player put at the gate, the three Ganado sent for him.
static void r300_find_camera_event()
{
    SceExec(0x12, (TaskFunc) r300_find_siren, 0, 0, 2, 0);
    int zero = 0;
    SndRoomStrStop(3);
    SndBgmTblSet(0x300, 1);
    SndRoomStrStart(1, 0, 1);
    pG->Room_flg[0] |= 0x10000000;
    SceSleep(0xF);
    RsfSet(G_ROOM_ID, 2);
    SmdGetObjPtr(0)->type = zero;
    {
        // x/z take f0/f13 by local-alloc lifetime: with the three stores in x/y/z order, x's load
        // is issued before z's and its lifetime also covers the `addi r4,r1,8` and the pPL load, so
        // z is allocated first (f0). The function-local static keeps x's .rodata word ahead of y/z
        // (statics and pool words are emitted in creation order) while its load is expanded after
        // z's: sched1 issues z, then x, and z's lifetime now spans x's load; the store order x/z stays.
        static const f32 k_x = -19430.0f;
        Vec pos;
        f32 fz;

        pos.y = -18000.0f;
        fz = -7920.0f;
        pos.x = k_x;
        pos.z = fz;
        pPL->setPos(&pos);
        // The y angle is loaded before the x/z zero stores: the z store (its zero dies there) is
        // scheduled first, then y, then x (z/y/x).
        f32 ry = -3.04f;
        pos.z = pos.x = 0.0f;
        pos.y = ry;
        pPL->setAng(&pos);
    }
    r300_work->target.x = -27548.0f;
    r300_work->target.y = -18000.0f;
    r300_work->target.z = -10660.0f;
    SceEventStart(0);
    SceSetEventCancel(1, (TaskFunc) r300_find_camera_event_exit, 0, -1, 1);
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    pPL->setNoSuspend(1);
    CamCtrl.CutCall(0x18);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x31), 0xA, 0, 1, 0);
    SceSleep((u32) MotionGetMaxFrame(&pPL->Motion));
    pPL->setNoSuspend(0);
    {
        cEmWrap em0;
        cEmWrap em1;
        cEmWrap em2;

        em0.setEm(0x49, -1, 1, 1, 1);
        em1.setEm(0x4A, -1, 1, 1, 1);
        em2.setEm(5, -1, 1, 1, 1);
        em0.setNoSuspend(1);
        em1.setNoSuspend(1);
        em2.setNoSuspend(1);
        {
            Vec gpos = {-26388.0f, -18000.0f, -15860.0f};

            em0.setGoto(&gpos, 0xC);
            em1.setGoto(&gpos, 0xC);
        }
        em0.setFindPL();
        em1.setFindPL();
        em2.setFindPL();
        CamCtrl.CutCall(0xA);
        SceSleep(0xF);
        SndCall(8, 0x1A, &em2.getPtr()->pos, em2.getPtr()->id, 0, em2.getPtr());
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r300_find_camera_event_exit();
}

// Area 15: the camera post. The lookouts appear and, unless the rock is done with, the event runs.
static void r300_find_camera()
{
    setEm(0, -1, 1, 1, 1);
    setEm(1, -1, 1, 1, 1);
    setEm(2, -1, 1, 1, 1);
    setEm(7, -1, 1, 1, 1);
    setEm(0xB, -1, 1, 1, 1);
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceExec(0x12, (TaskFunc) r300_find_camera_event, 0, 0, 2, 0);
    }
}

// Ashley's model follows the carrying Ganado.
void setAslPos(cObj* obj)
{
    Vec ofs;

    ofs.x = -176.17f;
    ofs.y = 0.0f;
    ofs.z = 40.95f;
    PSMTXMultVec(obj->mat, &ofs, &r300_work->asl->pos);
    r300_work->asl->ang.y = obj->ang.y + 3.1415927f;
    r300_work->asl->ang.y = LIMIT_ANGLE(r300_work->asl->ang.y);
}

// End of the Ashley scene (also its cancel path): the stream stopped unless it ended itself, the
// carrying Ganado and Ashley's model destroyed, SEs stopped, the gate 0x42 back down, area 8 on,
// Room_flg bit 3, the player released, camera back, SceEventEnd.
static void r300_asl_exit()
{
    if (!(pG->Room_flg[0] & 0x02000000)) {
        SndStrReq(r300_work->strId, 8, 0, 0);
    }
    r300_work->aslEm.destroy();
    ObjMgr.destroy(r300_work->asl);
    SndStop(r300_work->se[0], 0);
    SndStop(r300_work->se[1], 0);
    SmdGetObjPtr(0x42)->pos.y = r300_work->doorY;
    SceAtSetEnable(8, 1);
    RsfSet(G_ROOM_ID, 3);
    pPL->setNoSuspend(0);
    SpfFlagOff(pG, SPF_PL);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    GameSave.save(pSaveData, -1);
}

// Area 1: Ashley carried through the gate.
static void r300_asl()
{
    SceEventStart(1);
    r300_work->strId = SndStrReq(0, 0x39, 0x80000003, 0, 0, 0.0f);
    pPL->setNoSuspend(1);
    SpfFlagOn(pG, SPF_PL);
    SmdGetObjPtr(0x42)->be_flag |= 0x20;
    r300_work->doorY = SmdGetObjPtr(0x42)->pos.y;
    SmdGetObjPtr(0x42)->pos.y += 3000.0f;
    SceAtSetEnable(8, 0);
    modelSet();
    r300_work->aslEm.setEm(0x27, -1, 1, 1, 1);
    SceSetEventCancel(1, (TaskFunc) r300_asl_exit, 0, -1, 1);
    if (r300_work->aslEm.getPtr() != 0) {
        u32 i = 0;

        r300_work->aslEm.setNoSuspend(1);
        CamCtrl.CutCall(0x15);
        while (CamCtrl.IsMotionEnd() == 0) {
            setAslPos((cObj*) r300_work->aslEm.getPtr());
            if (i++ == 0x95) {
                r300_work->se[0] = SndCall(6, 0xD, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
            }
            if (i > 0x96) {
                if (i <= 0xC2) {
                    SmdGetObjPtr(0x42)->pos.y -= 66.666664f;
                }
                setAslPos((cObj*) r300_work->aslEm.getPtr());
            }
            if (i == 0xC) {
                r300_work->se[1] = SndCall(6, 0xF, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
            }
            if (i == 0x7A) {
                SndCall(6, 0x10, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
            }
            if (i == 0x89) {
                SndCall(6, 0x10, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
            }
            SceSleep(1);
        }
    }
    pG->Room_flg[0] |= 0x02000000;
    SceSetEventCancel(0, 0, 0, -1, 1);
    r300_asl_exit();
}

// The alarm siren SE at the camera post every 360 frames (the loop body runs once: i == 0 is never true again).
static void r300_find_siren()
{
    u32 i;

    i = 0;
    do {
        Vec pos = {-9506.0f, -1238.0f, -25208.0f};

        i++;
        SndCall(6, 6, &pos, 0, 0, 0);
        SceSleep(0x168);
    } while (i == 0);
}

// Aims the searchlight (and its beam object) at `target`.
void setSearchLightTarget(cObj* light, Vec* target, cObj* light2)
{
    Vec d;
    Vec rot;
    Mtx m;
    f32 len;

    PSVECSubtract(&light->pos, target, &d);
    light->be_flag |= 0x20;
    light2->be_flag |= 0x20;
    len = SQRTF(d.x * d.x + d.z * d.z);
    rot.x = 0.0f;
    rot.y = atan2f(d.x, d.z) - 1.5707964f;
    rot.z = 1.5707964f - atan2f(len, d.y);
    RotMatrixZXY(m, &rot);
    Matrix2AxisAngle(m, &rot);
    light->ang = rot;
    rot.x = 0.0f;
    rot.y = atan2f(d.x, d.z) - 1.5707964f;
    rot.z = 0.0f;
    RotMatrixZXY(m, &rot);
    Matrix2AxisAngle(m, &rot);
    light2->ang = rot;
}

// Turns the mirror camera matrix towards the beam from `from` to `to`, damped against the mirror's rotation.
static inline void r300_rotToLaser(Mtx m, f32 toX, f32 fromX, f32 toZ, f32 fromZ, u32 no)
{
    f32 ang = atan2f(toX - fromX, toZ - fromZ);

    PSMTXRotRad(m, 'y', Muku2(ang, SmdGetObjPtr(no)->ang.y, 3.1415927f) * 0.3f + ang);
}

// Area 6: mirror A. The player turns it with the stick while the camera looks along the beam.
static void r300_mira_exec()
{
    SceEventStart(1);
    CamCtrl.CutCall(0x11);
    SceSleep(1);
    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    if (SceMesGetSelection() == 1) {
        CamCtrl.CutCall(0xD);
        SceSleep(1);
        r300_work->cnt = 0;
        pG->Room_flg[0] &= ~0x80000000;
        r300_work->cam = pG->Camera;
        CameraControl* cc = &CamCtrl;
        while (1) {
            ActBtn.set(ACT_OPERATION, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_GACHA, ACT_FUNC_NORMAL, 0);
            SpfFlagOff(pG, SPF_ACTBTN);
            if (r300_work->cnt == 0) {
                if (Key.trg & 0x40000000) {
                    break;
                }
                if (Key.on & 0x08000000) {
                    r300_work->miraAng += 0.0052359877f;
                }
                if (Key.on & 0x04000000) {
                    r300_work->miraAng -= 0.0052359877f;
                }
            }
            if (r300_work->cnt == 1) {
                SndCall(6, 0x12, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
            }
            if (pG->Room_flg[0] & 0x80000000) {
                SceExec(0x12, (TaskFunc) DoorOpen, 0, 0, 2, 0);
                break;
            }
            if (r300_work->miraAng > 3.92f) {
                r300_work->miraAng = 3.92f;
            }
            if (r300_work->miraAng < 2.59f) {
                r300_work->miraAng = 2.59f;
            }
            Vec pos = {-20664.0f, -10229.0f, -19603.0f};
            Mtx m;
            Vec dir;
            CAMERA* cam = &r300_work->cam;
            dir.x = cam->param.at.x - pos.x;
            dir.y = 0.0f;
            dir.z = cam->param.at.z - pos.z;
            dir.z = PSVECMag(&dir);
            dir.x = 0.0f;
            dir.y = 0.0f;
            if (pG->Room_flg[0] & 0x40000000) {
                r300_rotToLaser(m, r300_work->laserPos[1].x, r300_work->laserPos[0].x, r300_work->laserPos[1].z, r300_work->laserPos[0].z, 0x3C);
            } else {
                PSMTXRotRad(m, 'y', SmdGetObjPtr(0x3C)->ang.y);
            }
            PSMTXMultVecSR(m, &dir, &dir);
            dir.y = cam->param.at.y - pos.y;
            PSVECAdd(&dir, &pos, &cam->param.at);
            dir.x = cam->param.pos.x - pos.x;
            dir.y = 0.0f;
            dir.z = cam->param.pos.z - pos.z;
            dir.z = PSVECMag(&dir);
            dir.x = 0.0f;
            dir.y = 0.0f;
            if (pG->Room_flg[0] & 0x40000000) {
                r300_rotToLaser(m, r300_work->laserPos[1].x, r300_work->laserPos[0].x, r300_work->laserPos[1].z, r300_work->laserPos[0].z, 0x3C);
            } else {
                PSMTXRotRad(m, 'y', SmdGetObjPtr(0x3C)->ang.y);
            }
            PSMTXMultVecSR(m, &dir, &dir);
            dir.x = -dir.x;
            dir.y = cam->param.pos.y - pos.y;
            dir.z = -dir.z;
            PSVECAdd(&dir, &pos, &cam->param.pos);
            r300_work->cam.param.roll = 0.0f;
            CameraSetOrientationRoll(&r300_work->cam);
            cc->SetExtraCamera(&r300_work->cam);
            SceSleep(1);
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 7: mirror B.
static void r300_mirb_exec()
{
    SceEventStart(1);
    CamCtrl.CutCall(0x12);
    SceSleep(1);
    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    if (SceMesGetSelection() == 1) {
        CamCtrl.CutCall(0xC);
        SceSleep(1);
        r300_work->cnt = 0;
        pG->Room_flg[0] &= ~0x80000000;
        r300_work->cam = pG->Camera;
        Vec* lp = r300_laser;
        CameraControl* cc = &CamCtrl;
        while (1) {
            ActBtn.set(ACT_OPERATION, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_GACHA, ACT_FUNC_NORMAL, 0);
            SpfFlagOff(pG, SPF_ACTBTN);
            if (r300_work->cnt == 0) {
                if (Key.trg & 0x40000000) {
                    break;
                }
                if (Key.on & 0x08000000) {
                    r300_work->mirbAng += 0.0052359877f;
                }
                if (Key.on & 0x04000000) {
                    r300_work->mirbAng -= 0.0052359877f;
                }
            }
            if (r300_work->cnt == 1) {
                SndCall(6, 0x12, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
            }
            if (pG->Room_flg[0] & 0x80000000) {
                SceExec(0x12, (TaskFunc) DoorOpen, 0, 0, 2, 0);
                break;
            }
            if (r300_work->mirbAng > 1.26f) {
                r300_work->mirbAng = 1.26f;
            }
            if (r300_work->mirbAng < 0.67f) {
                r300_work->mirbAng = 0.67f;
            }
            Vec pos = {-33306.0f, -10229.0f, -40392.0f};
            Mtx m;
            Vec dir;
            CAMERA* cam = &r300_work->cam;
            dir.x = cam->param.at.x - pos.x;
            dir.y = 0.0f;
            dir.z = cam->param.at.z - pos.z;
            dir.z = PSVECMag(&dir);
            dir.x = 0.0f;
            dir.y = 0.0f;
            if (pG->Room_flg[0] & 0x20000000) {
                r300_rotToLaser(m, r300_work->laserPos[0].x, lp[1].x, r300_work->laserPos[0].z, lp[1].z, 0x3D);
            } else {
                PSMTXRotRad(m, 'y', SmdGetObjPtr(0x3D)->ang.y);
            }
            PSMTXMultVecSR(m, &dir, &dir);
            dir.y = cam->param.at.y - pos.y;
            PSVECAdd(&dir, &pos, &cam->param.at);
            dir.x = cam->param.pos.x - pos.x;
            dir.y = 0.0f;
            dir.z = cam->param.pos.z - pos.z;
            dir.z = PSVECMag(&dir);
            dir.x = 0.0f;
            dir.y = 0.0f;
            if (pG->Room_flg[0] & 0x20000000) {
                r300_rotToLaser(m, r300_work->laserPos[0].x, lp[1].x, r300_work->laserPos[0].z, lp[1].z, 0x3D);
            } else {
                PSMTXRotRad(m, 'y', SmdGetObjPtr(0x3D)->ang.y);
            }
            PSMTXMultVecSR(m, &dir, &dir);
            dir.x = -dir.x;
            dir.y = cam->param.pos.y - pos.y;
            dir.z = -dir.z;
            PSVECAdd(&dir, &pos, &cam->param.pos);
            r300_work->cam.param.roll = 0.0f;
            CameraSetOrientationRoll(&r300_work->cam);
            cc->SetExtraCamera(&r300_work->cam);
            SceSleep(1);
        }
        if (pG->Room_flg[0] & 0x40000000) {
            r300_work->mirbAng = r300_mirbAng[0];
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// End of the gate opening (also its cancel path): Key_flg[0] 0x80, the laser effects dropped, the
// gate 0x42 snapped down to y -8825, Room_flg bit 5, the mirror / laser / gate areas off, camera back.
static void DoorOpen_exit()
{
    KyfFlagOn(pG, KYF_ST1_00);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM02, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM02, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM02, 0);
    SmdGetObjPtr(0x42)->be_flag |= 0x20;
    SmdGetObjPtr(0x42)->pos.y = -8825.0f;
    RsfSet(GS_ROOM_ID, 5);
    SceAtSetEnable(8, 0);
    SceAtSetEnable(6, 0);
    SceAtSetEnable(7, 0);
    SceAtSetEnable(9, 0);
    SceAtSetEnable(0xA, 0);
    SceAtSetEnable(0xC, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    pG->Room_flg[0] &= ~0x10000000;
    if (SceCountEmAlive(0x10, 0x20) == 0) {
        setEm(0x45, -1, 1, 1, 1);
        setEm(0x39, -1, 1, 1, 1);
        setEm(0x41, -1, 1, 1, 1);
        setEm(0x42, -1, 1, 1, 1);
    }
    GameSave.save(pSaveData, -1);
}

// The laser burnt through: the gate opens.
static void DoorOpen()
{
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        u32 i;

        SceEventStart(1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_ROOM02, 0, 0);
        SceSetEventCancel(1, (TaskFunc) DoorOpen_exit, 0, -1, 1);
        CamCtrl.CutCall(0xE);
        SceSleep(1);
        SceSleep(0x14);
        SceSleep(0x28);
        SmdGetObjPtr(0x42)->be_flag |= 0x20;
        SndCall(6, 0xD, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
        for (i = 0; i <= 0x2C; i++) {
            SmdGetObjPtr(0x42)->pos.y += 66.666664f;
            SceSleep(1);
        }
        SceSleep(0x23);
        SceSetEventCancel(0, 0, 0, -1, 1);
        DoorOpen_exit();
    }
}

// The beam from `from` to `to` (cut at the first collision), as the three effect 8 pieces coloured r/g/b/a.
void DrawLaserLine(Vec* from, Vec* to, int r, int g, int b, int a, int type, f32 len)
{
    Vec hit;
    Vec d;
    cEsp* esp;
    u32 i;

    if (EatMgr.hitCheck(from, to, &hit, 0, 0, 0)) {
        *to = hit;
    }
    for (i = 0; i <= 2; i++) {
        if (EspEstSetSelect(EFF_ROOM, 8, i, &esp, 1)) {
            ((R300EspView*) esp)->pos2 = *from;
            esp->m_Pos = *from;
            PSVECSubtract(to, from, &d);
            esp->m_Speed = d;
            esp->m_Col_r *= (f32) r;
            esp->m_Col_g *= (f32) g;
            esp->m_Col_b *= (f32) b;
            esp->m_Col_a *= (f32) a;
            esp->m_Col_r *= 0.003921569f;
            esp->m_Col_g *= 0.003921569f;
            esp->m_Col_b *= 0.003921569f;
            esp->m_Col_a *= 0.003921569f;
        }
    }
}

// End of the laser start cut: camera back, SceEventEnd, areas 9/0xA = the laser look messages, 0xC =
// the gate message, area 0xB off.
static void r300_laser_start_exit()
{
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) r300_laser_exec, 0, 1);
    SceAtDataSet_exec(0xA, 0x12, 0, (TaskFunc) r300_laser2_exec, 0, 1);
    SceAtDataSet_exec(0xC, 0x12, 0, (TaskFunc) r300_laser_door_exec, 0, 1);
    SceAtSetEnable(0xB, 0);
}

// Area 11: the laser is switched on.
static void r300_laser_start()
{
    EstSet(0, -1, 0, 0, EFF_ROOM, 9, 1, ESP_CORE_KIND_NONE, 0, 0);
    SceEventStart(1);
    RsfSet(G_ROOM_ID, 7);
    SndCall(6, 0x11, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
    SceSetEventCancel(1, (TaskFunc) r300_laser_start_exit, 0, -1, 1);
    CamCtrl.CutCall(0xF);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(0x10);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r300_laser_start_exit();
}

// Area 9: camera cut 0x13 on the laser with message 1.
static void r300_laser_exec()
{
    SceEventStart(1);
    CamCtrl.CutCall(0x13);
    SceSleep(1);
    SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 0xA: camera cut 0x14 on the second beam with message 2.
static void r300_laser2_exec()
{
    SceEventStart(1);
    CamCtrl.CutCall(0x14);
    SceSleep(1);
    SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 0xC: message 3 (the gate must be burnt open).
static void r300_laser_door_exec()
{
    SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
}

// End of the gate Ganado event (also its cancel path): the event Ganado em[20] swapped for the list
// Ganado 0xBF (alerted), Room_flg bit 8, camera back, SceEventEnd, Status_flg[2] 0x02000000 off, the
// player put back, the reset task starts.
static void r300_em_set_exit()
{
    r300_work->em[20].destroy();
    r300_work->em[21].setEm(0xBF, -1, 1, 1, 1);
    r300_work->em[21].setFindPL();
    RsfSet(G_ROOM_ID, 8);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    pPL->setPos(&r300_work->plPos);
    SceExec(0x12, (TaskFunc) r300_em_reset_task, 0, 0, 2, 0);
}

// Areas 13 / 14: the Ganado event at the gate.
static void r300_em_set()
{
    u32 i;

    SceAtSetEnable(0xD, 0);
    SceAtSetEnable(0xE, 0);
    if (RsfCheck(G_ROOM_ID, 13) == 0) {
        SndRoomStrStop(3);
        SndBgmTblSet(0x300, 1);
        SndRoomStrStart(1, 0, 1);
        SceExec(0x12, (TaskFunc) r300_StrCheck, 0, 0, 2, 0);
    }
    SceEventStart(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    Vec pos = {-28500.0f, -14200.0f, -24783.0f};
    f32 ry = 1.32f;
    r300_work->em[20].setEm(0x4B, -1, 1, 1, 1);
    ((cEmGanado*) r300_work->em[20].getPtr())->setEvtMotion(ROOM_ARC_PTR(pG->pRoom, 0x33), ROOM_ARC_PTR(pG->pRoom, 0x34), 0, 0);
    r300_work->em[20].setNoSuspend(1);
    r300_work->em[20].setFindPL();
    r300_work->plPos = pPL->pos;
    // COMPILER-DIFF: #5 (local-alloc qty order) -- y/z of `p` take f13/f0 in the target: z's lifetime is
    // one insn shorter than y's in its sched1 order, i.e. one insn sits between the two loads. The
    // codeless anchor is a fake store to the work pointer word (symbol-based: no alias with the frame
    // stores or the struct-view `pPL` load, an output dependence on the three plPos word stores so it
    // becomes ready right after them); it is issued in the free slot beside y's load, before z's.
    asm("" : "=m"(r300_work));
    {
        Vec p;
        Vec* pp = &p;

        p.x = -28074.0f;
        pp->y = -18000.0f;
        pp->z = -14061.0f;
        // The `this` load through the struct view stays below the plPos word stores: the work pointer
        // load then ranks below the `pPL` load and the three constant `lis` (the target's order).
        pPL->setPos(pp);
    }
    CamCtrl.CutCall(0x16);
    SceSetEventCancel(1, (TaskFunc) r300_em_set_exit, 0, -1, 1);
    SceSleep(0xA);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r300_work->em[20].setPos(&pos);
    {
        Vec ang;

        r300_setEmAng(&r300_work->em[20], &ang, ry);
    }
    for (i = 0; i <= 0x22; i++) {
        r300_work->em[20].getPtr()->move();
    }
    CamCtrl.CutCall(0x17);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r300_em_set_exit();
}

// Area 16: the last two Ganado.
static void r300_em_set_last()
{
    setEm(3, -1, 1, 1, 1);
    setEm(4, -1, 1, 1, 1);
    RsfSet(G_ROOM_ID, 9);
}

// Ashley's model files (costume 1 has its own set).
void modelLoad()
{
    if (pG->game_costume != 1) {
        r300_work->data[0] = DC.setData("etc/pl010a.bin");
        r300_work->data[1] = DC.setData("etc/pl010a.tpl");
        r300_work->data[2] = DC.setData("etc/pl010d.bin");
        r300_work->data[3] = DC.setData("etc/pl010e.bin");
        r300_work->data[4] = DC.setData("etc/pl01rh00.bin");
        r300_work->data[5] = DC.setData("etc/pl01lh00.bin");
    } else {
        r300_work->data[0] = DC.setData("etc/pl050a.bin");
        r300_work->data[1] = DC.setData("etc/pl050a.tpl");
        r300_work->data[2] = DC.setData("etc/pl050d.bin");
        r300_work->data[3] = DC.setData("etc/pl050e.bin");
        r300_work->data[4] = DC.setData("etc/pl05rh00.bin");
        r300_work->data[5] = DC.setData("etc/pl05lh00.bin");
    }
    r300_work->data[0]->setCommand(1, 0, 1);
    r300_work->data[1]->setCommand(1, 0, 1);
    r300_work->data[2]->setCommand(1, 0, 1);
    r300_work->data[3]->setCommand(1, 0, 1);
    r300_work->data[4]->setCommand(1, 0, 1);
    r300_work->data[5]->setCommand(1, 0, 1);
}

// Ashley's carried model: SetObjSmd from the loaded model files with her motion and the extra model
// infos (hair / costume parts) added.
void modelSet()
{
    Vec zero;
    Vec* pz = &zero;
    cModelInfo* info;

    r300_memset(pz, 0, sizeof(Vec));
    r300_work->asl = SetObjSmd(r300_work->data[0]->m_addr, r300_work->data[1]->m_addr, pz, pz, 0x10, 1);
    MotionSetCore(r300_work->asl, &r300_work->asl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x30), 0, 0, 5, 0);
    info = ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2F));
    if (info) {
        r300_work->asl->addModel(info);
    }
    info = ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x2C), ROOM_ARC_PTR(pG->pRoom, 0x2E));
    if (info) {
        r300_work->asl->addModel(info);
    }
    info = ModInfoMgr.create(r300_work->data[2]->m_addr, r300_work->data[1]->m_addr);
    if (info) {
        r300_work->asl->addModel(info);
    }
    info = ModInfoMgr.create(r300_work->data[3]->m_addr, r300_work->data[1]->m_addr);
    if (info) {
        r300_work->asl->addModel(info);
    }
    info = ModInfoMgr.create(r300_work->data[4]->m_addr, r300_work->data[1]->m_addr);
    if (info) {
        r300_work->asl->addModel(info);
    }
    info = ModInfoMgr.create(r300_work->data[5]->m_addr, r300_work->data[1]->m_addr);
    if (info) {
        r300_work->asl->addModel(info);
    }
}

// The room stream plays while the player is seen (or found by the camera).
static void r300_StrCheck()
{
    int on = 0;

    RsfSet(G_ROOM_ID, 13);
    for (;;) {
        if (SceCkFindPL(0) == 1 || (pG->Room_flg[0] & 0x10000000)) {
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
