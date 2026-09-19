#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
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
#include "area.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_wrap.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "motion.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "item.h"
#include "item_model.h"
#include "sscrn.h"

// Room 2-23 (D:/Bio4/Prog/r223.cpp): the mine cart ride start, the two levers (reva2 / reva3),
// the lift platform (dai) and the enemy waves.

struct R223Work {
    cEmWrap em[23];   // 0x000
    cObj* toroko;     // 0x114  the cart
    cObj* dai;        // 0x118  the lift platform (item model 0x8D)
    u32 str;          // 0x11C  SndStrReq handle
    u32 se[3];        // 0x120  SndCall handles
};

// The work pointer is a struct member: every store through the work reloads it.
struct R223WorkPtr {
    R223Work* p;
};

static R223WorkPtr r223_work;

static f32 reva_rate = 0.008f;    // lever turn speed step
static f32 reva_acc = 3.5f;       // lift speed step
static f32 reva2_lo = -2.175f;    // lever 2 angle range
static f32 reva2_hi = -0.491f;
static f32 reva3_lo = 5675.0f;    // lift height range (lever 3)
static f32 reva3_hi = 5385.0f;
static f32 dai_lo = 5675.0f;      // lift height range (cart stop)
static f32 dai_hi = 5385.0f;

int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");   // COMPILER-DIFF: #4 (int table entries reach the s16 parameter untruncated)

// COMPILER-DIFF: #8 -- the original's prologue copies `fmr f28,f1; fmr f29,f2` before `mr r29,r5` (mode);
// ours orders the copies by parameter order, so the definition declares lo/hi before mode (same
// argument registers) under the original mangled name as a C symbol (the r226 playerPillarDownCk route).
extern "C" void reva_common_move__FP4cObjiiff(cObj* obj, int axis, f32 lo, f32 hi, int mode);
#define reva_common_move(obj, axis, mode, lo, hi) reva_common_move__FP4cObjiiff(obj, axis, lo, hi, mode)
static void reva2_use_after_reva3_exit();
void reva2_use_after_reva3();
static void toroko_go_and_stop_exit();
static void toroko_go_and_stop();
void reva2_use_pre_reva3();
static void reva2_move();
static void reva3_move();
void toroko_move1();
void toroko_move2();
static void r223_GanadoEscapeCheck();
int isZouenGo();
int isZouenGo2();
void setFlagStart(cEmWrap* em);
int execReset(int no, u16* cnt);
int setChange(int mode, int flagNo, int oldNo, int newNo, int emId);
static void r223_EmApper_exit();
static void r223_EmApper();
static void r223_EmCheck();
void dai_down_stop();
void dai_down_end();
static void r223_ItemGet();
static void r223_ItemUse_exec();
static void r223_ItemUse();
static void r223_Bomb();
static void r223_StrCheck();

// The position/angle set with the object fetched first (SmdGetObjPtr before the stores into the
// caller's Vec; an inline taking the Vec would keep its address in a register).
#define SET_POS_XYZ(o, v, X, Y, Z)  \
    {                               \
        cModel* m_ = (o);           \
        (v).x = X;                  \
        (v).y = Y;                  \
        (v).z = Z;                  \
        m_->setPos(&(v));           \
    }
#define SET_ANG_XYZ(o, v, X, Y, Z)  \
    {                               \
        cModel* m_ = (o);           \
        (v).x = X;                  \
        (v).y = Y;                  \
        (v).z = Z;                  \
        m_->setAng(&(v));           \
    }

// Room init (the mine cart start / lift platform): Debug_flg[1] 0x20000 (silence cEmWrap errors);
// JumpPoint 1 presets the lift state by debug trigger 1; Room_flg bit 6 (cart available) set; areas 3/4
// = levers 2/3, area 5 = the escape check; the cart and lift objects; the initial Ganados (list 5) and
// the wave task; item area 0x80 (the lift key item) enabled once the lift came down (bit 9); area 0x11
// = using item 0x8D on the platform until bit 10 (else the bomb areas are off); the ambient stream; the
// lift group posed by bits 7/9 (cart gone / lift down); two more Ganados 0xE0/0xE1.
void R223Init()
{
    f32 h;

#line 67 "D:/Bio4/Prog/r223.cpp"
    R223Work*& wp = r223_work.p;
    wp = (R223Work*) MEM_CALLOC(sizeof(R223Work), 1, 0xD);
    DbgFlagOn(pG, DBG_EMW_ERR_NO_DISP);
    if (pG->JumpPoint == 1) {
        if (DebugTrg(1)) {
            RsfSet(G_ROOM_ID, 6);
            RsfClear(G_ROOM_ID, 9);
        } else {
            RsfClear(G_ROOM_ID, 6);
            RsfSet(G_ROOM_ID, 9);
        }
    }
    RsfSet(G_ROOM_ID, 6);
    SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) reva2_move, 0, 1);
    SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) reva3_move, 0, 1);
    SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r223_GanadoEscapeCheck, 0, 1);
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    r223_work.p->toroko = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, 0x10, 1);
    void* bin;
    void* tpl;
    if (ItemGetBinTplAddr(0x8D, &bin, &tpl)) {
        r223_work.p->dai = SetObjSmd(bin, tpl, &pos, &rot, 0x10, 1);
    }
    r223_work.p->em[0].setEm(0xD4, 5, 0, 1, 1);
    r223_work.p->em[2].setEm(0xD9, 5, 1, 1, 1);
    r223_work.p->em[3].setEm(0xDB, 5, 1, 1, 1);
    r223_work.p->em[4].setEm(0xD7, 5, 1, 1, 1);
    r223_work.p->em[5].setEm(0xDA, 5, 1, 1, 1);
    r223_work.p->em[7].setEm(0xD2, 5, 1, 1, 1);
    r223_work.p->em[8].setEm(0xD3, 5, 1, 1, 1);
    if (setChange(1, 4, 3, 0x10, 0xCF) == 1) {
        AreaGetCenterPos(&pos, &SceAtPtr(0xB)->area);
        r223_work.p->em[16].setGoto(&pos, 7);
    }
    SceExec(0x12, (TaskFunc) r223_EmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        SceAtSetEnable(0x80, 0);
    }
    SceAtDataSet_exec(0x80, SCE_LEVEL10, 0, (TaskFunc) r223_ItemGet, 0, 1);
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        SceAtDataSet_exec(0x11, SCE_LEVEL10, 0, (TaskFunc) r223_ItemUse, 0, 1);
        SceExec(0x12, (TaskFunc) r223_ItemUse_exec, 0, 0, SCE_PRIO_DEF_2, 0);
        EstSet(0, -1, 0, 0, 1, 0x1E, 1, 2, 0, 0);
    } else {
        SceAtSetEnable(0x11, 0);
        SceAtSetEnable(0x12, 0);
        SceAtSetEnable(0x13, 0);
    }
    SceExec(0x12, (TaskFunc) r223_StrCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    if (RsfCheck(G_ROOM_ID, 7)) {
        SET_POS_XYZ(SmdGetObjPtr(0x16), pos, -7794.0f, 4315.0f, -37212.0f);
        SET_ANG_XYZ(SmdGetObjPtr(0x16), pos, 0.0f, -0.121f, 0.0f);
        SmdGetObjPtr(0x16)->be_flag |= 0x20;
        SmdGetObjPtr(0x16)->be_flag |= 2;
        r223_work.p->toroko->be_flag &= ~2;
        SET_POS_XYZ(r223_work.p->dai, pos, -7935.68f, 4376.0f, -37032.8f);
        if (RsfCheck(G_ROOM_ID, 9)) {
            h = 3600.0f;
            EstSet(0, -1, 0, 0, 1, 0x16, 1, 3, 0, 0);
            EstSet(0, -1, 0, 0, 1, 0x18, 1, 3, 0, 0);
        } else {
            h = 1200.0f;
            SmdGetObjPtr(0x19)->be_flag |= 0x20;
            SmdGetObjPtr(0x19)->pos.y = 5385.0f;
            EstSet(0, -1, 0, 0, 1, 0x17, 1, 3, 0, 0);
            EstSet(0, -1, 0, 0, 1, 0x19, 1, 3, 0, 0);
        }
        {
            cObj* o12 = SmdGetObjPtr(0xC);
            cObj* o5b = SmdGetObjPtr(0x5B);

            o12->be_flag |= 0x20;
            o5b->be_flag |= 0x20;
            o12->pos.y -= h;
            o5b->pos.y -= h;
            r223_work.p->dai->pos.y -= h;
            SmdGetObjPtr(0x16)->pos.y -= h;
        }
    } else {
        EstSet(0, -1, 0, 0, 1, 0x16, 1, 3, 0, 0);
        EstSet(0, -1, 0, 0, 1, 0x18, 1, 3, 0, 0);
    }
    r223_work.p->em[21].setEm(0xE0, -1, 1, 1, 1);
    r223_work.p->em[22].setEm(0xE1, -1, 1, 1, 1);
    PlRegistMotion(0, 0, 0, 0, 0, 0, 0, 0, ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23),
                   ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25));
}

// Per-frame room main: nothing.
void R223Main()
{
}

// Moves obj->rot.x (axis 1) or obj->pos.y towards hi from lo with an accelerating speed and
// back. mode 1: stop at the end, mode 2: start going back.
// COMPILER-DIFF: #8 -- the original copies f1/f2 to f28/f29 before `mode` to r29: FP parameters
// declared before `mode` under the mangled name (see the declaration above).
extern "C" void reva_common_move__FP4cObjiiff(cObj* obj, int axis, f32 lo, f32 hi, int mode)
{
    f32 spd = 0.0f;
    f32* p;
    f32 acc;

    if (axis == 1) {
        SndCall(6, 3, &obj->pos, 0, 0, 0);
    } else {
        SndCall(6, 4, &obj->pos, 0, 0, 0);
    }
    obj->be_flag |= 0x20;
    if (axis == 1) {
        p = &obj->ang.x;
        acc = reva_rate;
    } else {
        p = &obj->pos.y;
        acc = reva_acc;
    }
    if (mode == 2) {
        spd = -acc;
    }
    for (;;) {
        int up;

        if (hi > lo) {
            *p += spd;
            up = 1;
        } else {
            *p -= spd;
            up = 0;
        }
        if (spd >= 0.0f) {
            if (up ? (*p < hi) : (*p > hi)) {
                spd += acc * 1.85f;
            } else {
                spd = -acc;
                if (mode == 1) {
                    return;
                }
            }
        } else {
            if (up ? (*p > lo) : (*p < lo)) {
                spd -= acc;
            } else {
                return;
            }
        }
        SceSleep(1);
    }
}

// End of the lift descent (also its cancel path): drop the effect, snap the lift group (objects 0xC,
// 0x5B, the cart, the platform, 0x16) to the bottom heights, stop the SEs, show the platform, item area
// 0x80 on, camera back, SceEventEnd.
static void reva2_use_after_reva3_exit()
{
    cObj* o12;
    cObj* o5b;

    EffectEspDelete(1, 4, 0, 0);
    EffectEspgenDelete(1, 4, 0);
    EffectEfmDelete(1, 4, 0);
    o12 = SmdGetObjPtr(0xC);
    o5b = SmdGetObjPtr(0x5B);
    o12->be_flag |= 0x20;
    o5b->be_flag |= 0x20;
    o12->pos.y = -2607.0f;
    o5b->pos.y = -350.0f;
    r223_work.p->toroko->pos.y = -3599.0f;
    r223_work.p->dai->pos.y = 776.0f;
    SmdGetObjPtr(0x16)->pos.y = 715.0f;
    SndStop(r223_work.p->se[0], 0);
    SndStop(r223_work.p->se[1], 0);
    SndStop(r223_work.p->se[2], 0);
    r223_work.p->dai->be_flag &= ~2;
    SceAtSetEnable(0x80, 1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Lever 2 after lever 3 (Room_flg bit 9): the lever swings, then the lift group rides down to the bottom
// (dai_down_end) as a cancellable event.
void reva2_use_after_reva3()
{
    RsfSet(G_ROOM_ID, 9);
    reva_common_move(SmdGetObjPtr(0x18), 1, 1, reva2_lo, reva2_hi);
    SceSleep(15);
    SmdGetObjPtr(0x18)->ang.x = reva2_lo;
    SceEventEnd(0);
    SceEventStart(1);
    SceSetEventCancel(1, (TaskFunc) reva2_use_after_reva3_exit, 0, -1, 1);
    dai_down_end();
    SceSetEventCancel(0, 0, 0, -1, 1);
    reva2_use_after_reva3_exit();
}

// End of the cart ride (also its cancel path): stream stopped, the cart-stand 0x16 and platform snapped
// to the far position, the cart hidden, lever 3 object 0x19 raised, the lift objects 0xC/0x5B at their
// middle heights, effects dropped.
static void toroko_go_and_stop_exit()
{
    Vec pos;
    cObj* o12;
    cObj* o5b;

    SndStrReq(r223_work.p->str, 8, 0, 0);
    SET_POS_XYZ(SmdGetObjPtr(0x16), pos, -7794.0f, 4315.0f, -37212.0f);
    SET_ANG_XYZ(SmdGetObjPtr(0x16), pos, 0.0f, -0.121f, 0.0f);
    SmdGetObjPtr(0x16)->be_flag |= 0x20;
    SmdGetObjPtr(0x16)->be_flag |= 2;
    r223_work.p->toroko->be_flag &= ~2;
    SET_POS_XYZ(r223_work.p->dai, pos, -7935.68f, 4376.0f, -37032.8f);
    EffectEspDelete(1, 4, 0, 0);
    EffectEspgenDelete(1, 4, 0);
    EffectEfmDelete(1, 4, 0);
    SmdGetObjPtr(0x19)->be_flag |= 0x20;
    SmdGetObjPtr(0x19)->pos.y = 5385.0f;
    o12 = SmdGetObjPtr(0xC);
    o5b = SmdGetObjPtr(0x5B);
    o12->be_flag |= 0x20;
    o5b->be_flag |= 0x20;
    o12->pos.y = -207.0f;
    o5b->pos.y = 2050.0f;
    r223_work.p->toroko->pos.y = -1200.0f;
    r223_work.p->dai->pos.y = 3176.0f;
    SmdGetObjPtr(0x16)->pos.y = 3115.0f;
    EffectEspDelete(1, 3, 0, 0);
    EffectEspgenDelete(1, 3, 0);
    EffectEfmDelete(1, 3, 0);
    EstSet(0, -1, 0, 0, 1, 0x17, 1, 3, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0x19, 1, 3, 0, 0);
    SndStop(r223_work.p->se[0], 0);
    SndStop(r223_work.p->se[1], 0);
    SndStop(r223_work.p->se[2], 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r223_work.p->em[18].setEm(0xD1, 5, 1, 1, 1);
}

// Lever 2 the first time (bit 7): the cart rolls off (toroko_move1) and the lift drops to the middle stop.
static void toroko_go_and_stop()
{
    SceEventStart(1);
    SceSetEventCancel(1, (TaskFunc) toroko_go_and_stop_exit, 0, -1, 1);
    toroko_move1();
    dai_down_stop();
    SceSetEventCancel(0, 0, 0, -1, 1);
    toroko_go_and_stop_exit();
}

// Lever 2 before lever 3: the lever swings; message 3 if the cart is not there (bit 6 clear), the cart
// ride once (bit 7), message 5 afterwards.
void reva2_use_pre_reva3()
{
    reva_common_move(SmdGetObjPtr(0x18), 1, 1, reva2_lo, reva2_hi);
    SceSleep(15);
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceMesSet(3, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    } else if (RsfCheck(G_ROOM_ID, 7) == 0) {
        RsfSet(G_ROOM_ID, 7);
        SceExec(0x12, (TaskFunc) toroko_go_and_stop, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceMesSet(5, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    }
    SmdGetObjPtr(0x18)->ang.x = reva2_lo;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 3, lever 2: camera cut 0xA; message 4 if already used at the bottom (bit 9), else the yes/no
// message 2 and the pre- / post-lever-3 action.
static void reva2_move()
{
    SceEventStart(1);
    CamCtrl.CutCall(0xA);
    if (RsfCheck(G_ROOM_ID, 9)) {
        SceMesSet(4, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        CamCtrl.Comeback(0);
        SceEventEnd(0);
    } else {
        SceMesSet(2, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        if (SceMesGetSelection() == 1) {
            if (RsfCheck(G_ROOM_ID, 8)) {
                reva2_use_after_reva3();
            } else {
                reva2_use_pre_reva3();
            }
        } else {
            CamCtrl.Comeback(0);
            SceEventEnd(0);
        }
    }
}

// Area 4, lever 3: camera cut 0xB; message 7 unless the cart went and the lever is unused (bits 7 set,
// 8 clear); yes -> bit 8, the lever object 0x19 drops with SE and effects, then toroko_move2.
static void reva3_move()
{
    SceEventStart(1);
    CamCtrl.CutCall(0xB);
    if (RsfCheck(G_ROOM_ID, 7) == 0 || RsfCheck(G_ROOM_ID, 8)) {
        SceMesSet(7, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    } else {
        SceMesSet(6, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        if (SceMesGetSelection() == 1) {
            RsfSet(G_ROOM_ID, 8);
            reva_common_move(SmdGetObjPtr(0x19), 0, 2, reva3_lo, reva3_hi);
            EffectEspDelete(1, 3, 0, 0);
            EffectEspgenDelete(1, 3, 0);
            EffectEfmDelete(1, 3, 0);
            r223_work.p->se[0] = SndCall(6, 0, 0, 0, 0, 0);
            EstSet(0, -1, 0, 0, 1, 0x16, 1, 3, 0, 0);
            EstSet(0, -1, 0, 0, 1, 0x18, 1, 3, 0, 0);
            SceSleep(15);
            toroko_move2();
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The cart rolls out: stream 1, the cart motion with a dust effect, camera cuts 8 and 9, then the
// cart-stand 0x16 and platform are moved to the far end and the cart hidden.
void toroko_move1()
{
    Vec pos;

    r223_work.p->str = SndStrReq(1, 1, 0x80000003, 0, 0, 0.0f);
    SmdSetTrans(0x16, 0);
    MotionSetCore(r223_work.p->toroko, &r223_work.p->toroko->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
    EstSet((int) r223_work.p->toroko, -1, 0, 0, 1, 0x1C, 1, 4, 0, 0);
    CamCtrl.CutCall(8);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SET_POS_XYZ(SmdGetObjPtr(0x16), pos, -7794.0f, 4315.0f, -37212.0f);
    SET_ANG_XYZ(SmdGetObjPtr(0x16), pos, 0.0f, -0.121f, 0.0f);
    SmdGetObjPtr(0x16)->be_flag |= 0x20;
    SmdGetObjPtr(0x16)->be_flag |= 2;
    r223_work.p->toroko->be_flag &= ~2;
    SET_POS_XYZ(r223_work.p->dai, pos, -7935.68f, 4376.0f, -37032.8f);
}

// Second cart move: empty in this build.
void toroko_move2()
{
}

// Area 5: empty in this build.
static void r223_GanadoEscapeCheck()
{
}

// 1 when the lift has been used and at least two of the four bridge enemies are gone.
int isZouenGo()
{
    u32 n = 0;

    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        return 0;
    }
    if (r223_work.p->em[7].isActive() == 0) {
        n = 1;
    }
    if (r223_work.p->em[8].isActive() == 0) {
        n++;
    }
    if (r223_work.p->em[4].isActive() == 0) {
        n++;
    }
    if (r223_work.p->em[5].isActive() == 0) {
        n++;
    }
    return n > 1;
}

// 1 once lever 3 was used (Room_flg bit 8).
int isZouenGo2()
{
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        return 0;
    }
    return 1;
}

// Alert a Ganado (flag bit 0 = start hostile) if the handle holds a live Ganado.
void setFlagStart(cEmWrap* em)
{
    if (em->isAlive() == 1 && em->isNormalGanade() == 1) {
        em->getPtr()->flag |= 1;
    }
}

// Counts frames while em[no] may be reset; resets it after 105 of them.
int execReset(int no, u16* cnt)
{
    if (r223_work.p->em[no].ckResetEnable() == 1) {
        (*cnt)++;
        if (*cnt > 0x69) {
            *cnt = 0;
            r223_work.p->em[no].setReset();
            setFlagStart(&r223_work.p->em[no]);
            return 1;
        }
    }
    return 0;
}

// Once em[oldNo] is gone (or, mode 1, once flagNo is already set) sets em[newNo] as emId.
int setChange(int mode, int flagNo, int oldNo, int newNo, int emId)
{
    if (RsfCheck(G_ROOM_ID, flagNo) == 0) {
        if (r223_work.p->em[oldNo].isActive() == 0) {
            RsfSet(G_ROOM_ID, flagNo);
            cEmWrapSetEmI(&r223_work.p->em[newNo], emId, 5, 1, 1, 1);
            setFlagStart(&r223_work.p->em[newNo]);
            return 1;
        }
    } else if (mode == 1) {
        cEmWrapSetEmI(&r223_work.p->em[newNo], emId, 5, 1, 1, 1);
        setFlagStart(&r223_work.p->em[newNo]);
        return 1;
    }
    return 0;
}

// End of the Ganado 0xCC entrance cutscene: it may suspend again, camera back, SceEventEnd, Status_flg[2] 0x02000000 off.
static void r223_EmApper_exit()
{
    r223_work.p->em[13].setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
}

// Cutscene: Ganado 0xCC (list 5) appears alerted under camera cuts 0x12 and 0x13; player-cancellable.
static void r223_EmApper()
{
    SceEventStart(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    r223_work.p->em[13].setEm(0xCC, 5, 1, 1, 1);
    setFlagStart(&r223_work.p->em[13]);
    r223_work.p->em[13].setNoSuspend(1);
    SceSetEventCancel(1, (TaskFunc) r223_EmApper_exit, 0, -1, 1);
    CamCtrl.CutCall(0x12);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(0x13);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r223_EmApper_exit();
}

// Task: the enemy waves, the patrol of em[0..2] between their two areas, the resets.
static void r223_EmCheck()
{
    Vec pt[8];
    u16 cnt[6] = {0, 0, 0, 0, 0, 0};
    Vec c;
    u8 flag[4];
    int i;
    u32 timer;

    AreaGetCenterPos(&pt[0], &SceAtPtr(6)->area);
    AreaGetCenterPos(&pt[1], &SceAtPtr(7)->area);
    AreaGetCenterPos(&pt[2], &SceAtPtr(8)->area);
    AreaGetCenterPos(&pt[3], &SceAtPtr(9)->area);
    AreaGetCenterPos(&pt[4], &SceAtPtr(9)->area);
    AreaGetCenterPos(&pt[5], &SceAtPtr(0xA)->area);
    AreaGetCenterPos(&pt[6], &SceAtPtr(0xC)->area);
    AreaGetCenterPos(&pt[7], &SceAtPtr(0xB)->area);
    for (i = 0; (u32) i < 4; i++) {
        flag[i] = 1;
        r223_work.p->em[i].setGoto(&pt[i * 2 + 1], 6);
    }
    timer = 0;
    for (;;) {
        for (i = 0; (u32) i < 4; i++) {
            if (i == 3 || r223_work.p->em[i].ckFindPL() == 1) {
                break;
            }
            if (r223_work.p->em[i].ckGoto() == 0) {
                if (flag[i] == 0) {
                    flag[i] = 1;
                    r223_work.p->em[i].setGoto(&pt[i * 2 + 1], 6);
                } else {
                    flag[i] = 0;
                    r223_work.p->em[i].setGoto(&pt[i * 2], 6);
                }
            }
        }
        if (RsfCheck(G_ROOM_ID, 0) == 0 && (pG->Room_flg[2] & 0x80000000) && isZouenGo() == 1) {
            RsfSet(G_ROOM_ID, 0);
            r223_work.p->em[9].setEm(0xDC, 5, 1, 1, 1);
            r223_work.p->em[10].setEm(0xDD, 5, 1, 1, 1);
            r223_work.p->em[11].setEm(0xDE, 5, 1, 1, 1);
            r223_work.p->em[12].setEm(0xDF, 5, 1, 1, 1);
            AreaGetCenterPos(&c, &SceAtPtr(0xD)->area);
            r223_work.p->em[9].setGoto(&c, 7);
            r223_work.p->em[10].setGoto(&c, 7);
            r223_work.p->em[11].setGoto(&c, 7);
            r223_work.p->em[12].setGoto(&c, 7);
            r223_work.p->em[13].setGoto(&c, 7);
        }
        if (RsfCheck(G_ROOM_ID, 1) == 0 && (pG->Room_flg[2] & 0x80000000) && isZouenGo2() == 1) {
            RsfSet(G_ROOM_ID, 1);
            SceExec(0x12, (TaskFunc) r223_EmApper, 0, 0, SCE_PRIO_DEF_2, 0);
        }
        timer++;
        if (timer > 100) {
            timer = 100;
            if (r223_work.p->em[3].isActive() == 1 && (pG->Room_flg[2] & 0x80000000) && RsfCheck(G_ROOM_ID, 2) == 0) {
                RsfSet(G_ROOM_ID, 2);
                r223_work.p->em[3].setGoto(&pPL->pos, 8);
                r223_work.p->em[17].setEm(0xD0, 5, 1, 1, 1);
                r223_work.p->em[20].setEm(0xD6, 5, 1, 1, 1);
                setFlagStart(&r223_work.p->em[17]);
                setFlagStart(&r223_work.p->em[20]);
                AreaGetCenterPos(&c, &SceAtPtr(0xB)->area);
                r223_work.p->em[17].setGoto(&c, 7);
            }
        }
        if (setChange(0, 4, 3, 0x10, 0xCF) == 1) {
            AreaGetCenterPos(&c, &SceAtPtr(0xB)->area);
            r223_work.p->em[16].setGoto(&c, 7);
        }
        setChange(0, 5, 2, 0x13, 0xD5);
        if (setChange(0, 4, 3, 0x10, 0xCF) != 0) {
            AreaGetCenterPos(&c, &SceAtPtr(0xB)->area);
            r223_work.p->em[16].setGoto(&c, 7);
        }
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            if (execReset(0x14, cnt) == 1) {
                RsfSet(G_ROOM_ID, 3);
            }
        }
        SceSleep(1);
    }
}

// The cart ride down: lowers the lift group over 120 frames with the camera cuts, the lift lever
// scene on the way.
void dai_down_stop()
{
    cObj* o12;
    cObj* o5b;
    int first = 1;
    int cnt = 0;
    u32 i;

    o12 = SmdGetObjPtr(0xC);
    o5b = SmdGetObjPtr(0x5B);
    o12->be_flag |= 0x20;
    o5b->be_flag |= 0x20;
    SmdGetObjPtr(0xA)->be_flag |= 0x20;
    SmdGetObjPtr(0xB)->be_flag |= 0x20;
    SmdGetObjPtr(0xC)->be_flag |= 0x20;
    SmdGetObjPtr(0xD)->be_flag |= 0x20;
    CamCtrl.CutCall(0xE);
    r223_work.p->se[1] = SndCall(6, 5, &r223_work.p->toroko->pos, 0, 0, 0);
    for (i = 0; i < 120; i++) {
        o12->pos.y -= 10.0f;
        o5b->pos.y -= 10.0f;
        r223_work.p->toroko->pos.y -= 10.0f;
        SmdGetObjPtr(0x16)->pos.y -= 10.0f;
        r223_work.p->dai->pos.y -= 10.0f;
        SmdGetObjPtr(0xA)->pParts->ang.z += 0.035f;
        SmdGetObjPtr(0xB)->pParts->ang.z -= 0.035f;
        SmdGetObjPtr(0xC)->pParts->ang.y += 0.035f;
        SmdGetObjPtr(0xD)->pParts->ang.x += 0.035f;
        if (first == 1) {
            if (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            } else {
                CamCtrl.CutCall(0xF);
                first = 0;
                EstSet(0, -1, 0, 0, 1, 0x1B, 1, 4, 0, 0);
            }
        } else {
            if (cnt++ == 0x31) {
                CamCtrl.CutCall(0xB);
                reva_common_move(SmdGetObjPtr(0x19), 0, 1, dai_lo, dai_hi);
                SceSleep(15);
                EffectEspDelete(1, 3, 0, 0);
                EffectEspgenDelete(1, 3, 0);
                EffectEfmDelete(1, 3, 0);
                r223_work.p->se[0] = SndCall(6, 0, 0, 0, 0, 0);
                EstSet(0, -1, 0, 0, 1, 0x17, 1, 3, 0, 0);
                EstSet(0, -1, 0, 0, 1, 0x19, 1, 3, 0, 0);
                SceSleep(35);
                CamCtrl.CutCall(0xF);
            }
            SceSleep(1);
        }
    }
    r223_work.p->se[2] = SndCall(6, 6, &r223_work.p->toroko->pos, 0, 0, 0);
    SceSleep(15);
    SceSleep(30);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
}

// The lift group goes down to the bottom.
void dai_down_end()
{
    cObj* o12;
    cObj* o5b;
    f32 spd;

    o12 = SmdGetObjPtr(0xC);
    o5b = SmdGetObjPtr(0x5B);
    o12->be_flag |= 0x20;
    o5b->be_flag |= 0x20;
    SmdGetObjPtr(0xA)->be_flag |= 0x20;
    SmdGetObjPtr(0xB)->be_flag |= 0x20;
    SmdGetObjPtr(0xC)->be_flag |= 0x20;
    SmdGetObjPtr(0xD)->be_flag |= 0x20;
    CamCtrl.CutCall(0x10);
    EstSet(0, -1, 0, 0, 1, 0x1A, 1, 4, 0, 0);
    r223_work.p->se[1] = SndCall(6, 5, &r223_work.p->toroko->pos, 0, 0, 0);
    spd = 13.333333f;
    while (o5b->pos.y > -350.0f) {
        o12->pos.y -= spd;
        o5b->pos.y -= spd;
        r223_work.p->toroko->pos.y -= spd;
        SmdGetObjPtr(0x16)->pos.y -= spd;
        r223_work.p->dai->pos.y -= spd;
        SmdGetObjPtr(0xA)->pParts->ang.z += 0.035f;
        SmdGetObjPtr(0xB)->pParts->ang.z -= 0.035f;
        SmdGetObjPtr(0xC)->pParts->ang.y += 0.035f;
        SmdGetObjPtr(0xD)->pParts->ang.x += 0.035f;
        SceSleep(1);
    }
    r223_work.p->se[2] = SndCall(6, 6, &r223_work.p->toroko->pos, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(15);
}

// Item area 0x80 (on the lowered platform): re-arm and run it, then wait until the item is taken.
static void r223_ItemGet()
{
    SceAtDataReset(0x80);
    SceAtExecute(0x80);
    while (SceAtItemFlgCk(0x80) == 0) {
        SceSleep(1);
    }
}

// Task: waits for item 0x8D (the dynamite) to be used, places it on the platform (Room_flg bit 10) and
// starts the bomb task.
static void r223_ItemUse_exec()
{
    Vec pos;

    while (ItemMgr.check(0x8D) != 1) {
        SceSleep(1);
    }
    SET_POS_XYZ(r223_work.p->dai, pos, -650.0f, 800.0f, -32100.0f);
    BitOn(r223_work.p->dai->be_flag, 2);
    RsfSet(G_ROOM_ID, 10);
    SceExec(0x12, (TaskFunc) r223_Bomb, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Area 0x11: the up-cut 8/0xC look; with item 0x8D held the item screen opens to use it.
static void r223_ItemUse()
{
    SceUpCut(8, 0xC, -1, 0);
    if (ItemMgr.num(0x8D) != 0) {
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
    }
}

// The dynamite: fuse SE and effect for 150 frames, then the blast (effect 0x1F, SE 8, a 6000-radius
// player-weapon hit of type 0x12), the platform item hidden, areas 0x12/0x13 off, Ganados em[21]/em[22]
// alerted and made hostile.
static void r223_Bomb()
{
    Vec pos = {-2039.0f, 525.0f, -31894.0f};

    SceAtSetEnable(0x11, 0);
    SndCall(6, 7, &pos, 0, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0x1D, 1, 2, 0, 0);
    SceSleep(150);
    EffectEspDelete(1, 2, 0, 0);
    EffectEspgenDelete(1, 2, 0);
    EffectEfmDelete(1, 2, 0);
    EstSet(0, -1, 0, 0, 1, 0x1F, 0, 0, 0, 0);
    r223_work.p->dai->be_flag &= ~2;
    SndCall(6, 8, &pos, 0, 0, 0);
    PlWepHitCheck2(0, &pos, &pos, 0x12, 2, 6000.0f);
    SceAtSetEnable(0x12, 0);
    SceAtSetEnable(0x13, 0);
    r223_work.p->em[21].setFindPL();
    r223_work.p->em[22].setFindPL();
    if (r223_work.p->em[21].isActive()) {
        r223_work.p->em[21].getPtr()->Character = 0;
    }
    if (r223_work.p->em[22].isActive()) {
        r223_work.p->em[22].getPtr()->Character = 0;
    }
}

// Task: the ambient stream while the player is in area 0.
static void r223_StrCheck()
{
    int on = 0;
    u32 str = 0;

    for (;;) {
        if (SceCkFindPL(0) == 1) {
            if (on == 0) {
                on = 1;
                str = SndStrReq(0, 0x10, 3, 0, 0, 0.0f);
            }
        } else {
            if (on == 1) {
                SndStrReq(str, 4, 600, 0);
                on = 0;
            }
        }
        SceSleep(1);
    }
}
