#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "card.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj18.h"
#include "em.h"
#include "emdoor.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "read.h"
#include "dvd.h"
#include "datactrl.h"
#include "esp.h"
#include "est.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "motion.h"
#include "math_sub.h"
#include "act_btn.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "sscrn.h"
#include "fade.h"
#include "snd.h"
#include "rnd.h"
#include "eprintf.h"

// Room 1-17 (D:/Bio4/Prog/r117.cpp): the church interior; the three coloured lights of the
// insignia mechanism, Ashley found upstairs (s00 event, the chandelier swing) and Saddler's
// appearance (s10 event).
//
// Status: 20/21 identical (.rodata/.data/.bss equal). Residue r117_EventChandelier (-4 bytes):
// the block after the first swing loop re-materialises `pPL@ha` into a fresh callee-saved
// register in the target while ours reuses the loop pseudo; the entry block also issues the
// zero-init `li`s before the vtable load chain (#5 interblock region shape).

void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp



struct R117Work {
    union {
        u16 stepMode;     // 0x00  step + mode as one halfword (LightMechanism clears both)
        struct {
            u8 step;      // 0x00  r117_lightMechTbl index
            u8 mode;      // 0x01  LightMechanismMove state
        };
    };
    s8 sel;               // 0x02  selected light (0..2)
    s8 cur[3];            // 0x03  current quarter turn of each light
    s8 tgt[3];            // 0x06  quarter turn of each light object
    u8 pad_9[3];
    cEsp* esp[3];         // 0x0C  the light beams
    cObj* light[3];       // 0x18  the light objects (smd 0x32, 0x30, 0x31)
    cObj* smd;            // 0x24  the chandelier / rope object
    u32 se;               // 0x28  RoomSeCall handle of the mechanism sound
    void* evBin;          // 0x2C  ev0101's model data saved by the s10 event
    void* evTpl;          // 0x30  ev0101's texture palette saved by the s10 event
    cDataUnit* evd0;      // 0x34  evd/r117s00.evd
    cDataUnit* evd1;      // 0x38  evd/r117s10.evd
    ReadModule* mod;      // 0x3C  enemy module 3 (the event data is swapped into its archive)
};

static R117Work* r117_work;
#define W r117_work

static const Vec r117_smdPos = {0.0f, 9826.0f, -2072.0f};
static const Vec r117_smdRot = {0.0f, 0.0f, 0.0f};

// The beam is done when bit 2 of its byte 0xF9 is set.
static inline int r117_espEnd(cEsp* esp)
{
    int on = 0;

    if (((u8*) esp)[0xF9] & 4) {
        on = 1;
    }
    return on;
}

static inline FadeWork* r117_fadeWork(int no) { return &Fade[no]; }

// White fade (start -> end colour words); the colour pair is a local of the inline (fade.h).
static inline void r117_fadeWhite(int no, u32 start, u32 end)
{
    FadeColorPair col;

    *(u32*) &col.start = start;
    *(u32*) &col.end = end;
    FadeSet(no, &col.start, &col.end, 5, 0, 0);
}

extern "C" void r117_MechanismInit();
extern "C" void r117_LightSet(int n);
extern "C" void r117_LightDirCalc(int n);
static void r117_EventAshleyFind();
static void r117_EventSaddlerAppear();
static void r117_LightMechanism();
static void r117_LightMechanismInit();
static void r117_LightMechanismMove();
static void r117_LightMechanismEndProc(int mode);
extern "C" void r117_LightRotate(int no, f32 dir);
extern "C" void r117_MechanismDisarm();
static void r117_EventChandelier();
static void r117_ThunderFlagOn();
static void r117_ThunderFlagOff();
static void r117_ThunderMove();
extern "C" void Evt_R117S00_Func(Event* e);
extern "C" void Evt_R117S10_Func(Event* e);
static void R117S0_WhiteFade();

// Room init (the church interior, chapter 2-1): thunder task, the chandelier rope object (SetObjSmd from
// room archive 0x1F/0x20), the light mechanism state. Until Ashley is found (Scenario_flg[0] 0x00100000):
// door 0 close-locked, evd r117s00 pre-loaded to ARAM, r117s10 registered with module 3 pre-read, area 7
// = the Ashley event, area 4 = the chandelier swing, the two event callbacks. Afterwards: two Ganados
// (ESL 0x50/0x51) on a fresh visit in Part 0, and the upstairs objects shown.
void R117Init()
{
#line 63 "D:/Bio4/Prog/r117.cpp"
    W = (R117Work*) MEM_CALLOC(sizeof(R117Work), 1, 0xd);

    SceExec(0x12, (TaskFunc) r117_ThunderMove, 0, 0, SCE_PRIO_DEF_2, 0);
    W->smd = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), (Vec*) &r117_smdPos, (Vec*) &r117_smdRot, 0x10, 1);
    W->smd->be_flag |= 0x1000;
    r117_MechanismInit();
    if (!ScfFlagChk(pG, SCF_R117_FIND_ASHLEY)) {
        cEmDoor* door;

        if (getRoomEtcDoor(0, &door, 1)) {
            door->setCloseLock();
        }
        W->evd0 = DC.setData(EvtMgr.NameChange("evd/r117s00.evd"));
        W->evd0->setCommand(CMND_ARAM_LOAD, 0, 0);
        W->evd1 = DC.setData(EvtMgr.NameChange("evd/r117s10.evd"));
        EmReadSearch(3, 0, W->evd1->m_size);
        SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r117_EventAshleyFind, 0, 1);
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r117_EventChandelier, 0, 1);
        EvtMgr.SetFunc("evt_r117s00_func", (void*) Evt_R117S00_Func);
        EvtMgr.SetFunc("evt_r117s10_func", (void*) Evt_R117S10_Func);
        W->mod = SearchEmModule(3);
    } else {
        if (!SysFlagChk(pG, SYS_LOAD_GAME) && pG->Part == 0) {
            setEm(0x50, -1, 0, 1, 0);
            setEm(0x51, -1, 0, 1, 0);
        }
        SmdGetObjPtr(0x2E)->be_flag &= ~2;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x27, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    }
    if (pG->Part == 1) {
        void* zero = 0;

        EstSet(pPL, -1, 0, 0, EFF_PL00, 2, 0x800, ESP_CORE_KIND_NONE, zero, zero);
        EstSet(pPL, -1, 0, 0, EFF_ROOM, 0x26, 0x800, ESP_CORE_KIND_NONE, zero, zero);
    }
}

// The mechanism state from the room flags: bit 1 = solved, bit 2 = started; bits 3..14 hold the
// three current quarter turns (4 flags each), bits 15..26 the three object turns.
extern "C" void r117_MechanismInit()
{
    void* zero = 0;
    int i;

    if (RsfCheck(G_ROOM_ID, 1)) {
        W->cur[0] = 0;
        W->cur[1] = 0;
        W->cur[2] = 0;
        r117_MechanismDisarm();
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x25, 1, ESP_CORE_KIND_ROOM00, zero, zero);
        for (i = 0; i < 4; i++) {
            if (RsfCheck(G_ROOM_ID, i + 0xF)) {
                W->tgt[0] = i;
            }
            if (RsfCheck(G_ROOM_ID, i + 0x13)) {
                W->tgt[1] = i;
            }
            if (RsfCheck(G_ROOM_ID, i + 0x17)) {
                W->tgt[2] = i;
            }
        }
    } else {
        SceAtDataSet_exec(8, SCE_LEVEL10, 8, (TaskFunc) r117_LightMechanism, 0, 1);
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            W->cur[0] = 2;
            W->cur[1] = 3;
            W->cur[2] = 1;
            for (i = 3; i < 0xF; i++) {
                RsfClear(G_ROOM_ID, i);
            }
            RsfSet(G_ROOM_ID, W->cur[0] + 3);
            RsfSet(G_ROOM_ID, W->cur[1] + 7);
            RsfSet(G_ROOM_ID, W->cur[2] + 0xB);
            W->tgt[0] = 0;
            W->tgt[1] = 0;
            W->tgt[2] = 0;
            for (i = 0xF; i < 0x1B; i++) {
                RsfClear(G_ROOM_ID, i);
            }
            RsfSet(G_ROOM_ID, W->tgt[0] + 0xF);
            RsfSet(G_ROOM_ID, W->tgt[1] + 0x13);
            RsfSet(G_ROOM_ID, W->tgt[2] + 0x17);
        } else {
            for (i = 0; i < 4; i++) {
                if (RsfCheck(G_ROOM_ID, i + 3)) {
                    W->cur[0] = i;
                }
                if (RsfCheck(G_ROOM_ID, i + 7)) {
                    W->cur[1] = i;
                }
                if (RsfCheck(G_ROOM_ID, i + 0xB)) {
                    W->cur[2] = i;
                }
                if (RsfCheck(G_ROOM_ID, i + 0xF)) {
                    W->tgt[0] = i;
                }
                if (RsfCheck(G_ROOM_ID, i + 0x13)) {
                    W->tgt[1] = i;
                }
                if (RsfCheck(G_ROOM_ID, i + 0x17)) {
                    W->tgt[2] = i;
                }
            }
            EstSet(0, -1, 0, 0, EFF_ROOM, 0, 1, ESP_CORE_KIND_ROOM00, 0, 0);
            EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_ROOM00, 0, 0);
            EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, 0, 0);
            EstSet(0, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_ROOM00, 0, 0);
            r117_LightSet(1);
        }
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x11, 1, ESP_CORE_KIND_ROOM01, 0, 0);
        SceAtSetEnable(5, 0);
    }
    W->light[0] = SmdGetObjPtr(0x32);
    f32 quarter = 1.5707964f;
    W->light[0]->ang.z = (f32) W->tgt[0] * quarter;
    W->light[0]->matUpdate();
    W->light[1] = SmdGetObjPtr(0x30);
    W->light[1]->ang.z = (f32) W->tgt[1] * quarter;
    W->light[1]->matUpdate();
    W->light[2] = SmdGetObjPtr(0x31);
    W->light[2]->ang.z = (f32) W->tgt[2] * quarter;
    W->light[2]->matUpdate();
}

// The three light beams for step n (0 = off, 1..4 = the effect sets 5/13/9 .. 8/16/12).
extern "C" void r117_LightSet(int n)
{
    u8 type[3];

    if (W->esp[0] != 0) {
        PushEsp(W->esp[0]);
    }
    if (W->esp[1] != 0) {
        PushEsp(W->esp[1]);
    }
    if (W->esp[2] != 0) {
        PushEsp(W->esp[2]);
    }
    switch (n) {
    case 1:
        type[0] = 5;
        type[1] = 0xD;
        type[2] = 9;
        break;
    case 2:
        type[0] = 6;
        type[1] = 0xE;
        type[2] = 0xA;
        break;
    case 3:
        type[0] = 7;
        type[1] = 0xF;
        type[2] = 0xB;
        break;
    case 4:
        type[0] = 8;
        type[1] = 0x10;
        type[2] = 0xC;
        break;
    default: {
        R117Work* w = W;

        w->esp[2] = 0;
        w->esp[1] = 0;
        w->esp[0] = 0;
        break;
    }
    }
    if (n != 0) {
        if (EspEstSetSelect(EFF_ROOM, type[0], 0, &W->esp[0], 1) == 1) {
            W->esp[0]->m_Ang.z = (f32) W->cur[0] * 1.5707964f;
        } else {
            W->esp[0] = 0;
        }
        if (EspEstSetSelect(EFF_ROOM, type[1], 0, &W->esp[1], 1) == 1) {
            W->esp[1]->m_Ang.z = (f32) W->cur[1] * 1.5707964f;
        } else {
            W->esp[1] = 0;
        }
        if (EspEstSetSelect(EFF_ROOM, type[2], 0, &W->esp[2], 1) == 1) {
            W->esp[2]->m_Ang.z = (f32) W->cur[2] * 1.5707964f;
        } else {
            W->esp[2] = 0;
        }
    }
}

// Per frame, once the mechanism was started (Room_flg bit 2): debug-print the three quarter turns and
// keep the three light objects turned toward their beams.
void R117Main()
{
    if (RsfCheck(G_ROOM_ID, 2)) {
        eprintf(30, 20, 0, 0, "%d%d%d", W->cur[0], W->cur[1], W->cur[2]);
    }
    if (RsfCheck(G_ROOM_ID, 2)) {
        r117_LightDirCalc(0);
        r117_LightDirCalc(1);
        r117_LightDirCalc(2);
    }
}

// Turn light object n (smd 0x1B / 0x19 / 0x1A) toward its beam.
extern "C" void r117_LightDirCalc(int n)
{
    Vec dir;
    Vec rot;
    Mtx m;
    Vec axis = {0.0f, 1.0f, 0.0f};
    u32 id;

    switch (n) {
    case 0:
        id = 0x1B;
        break;
    case 1:
        id = 0x19;
        break;
    case 2:
        id = 0x1A;
        break;
    default:
        return;
    }
    PSVECSubtract(&SmdGetObjPtr(id)->pos, &W->esp[n]->m_Pos, &dir);
    SetOrientationZX(&dir, &axis, m);
    Matrix2AxisAngle(m, &rot);
    SmdGetObjPtr(id)->ang = rot;
    SmdGetObjPtr(id)->matUpdate();
}

// Area 7: Ashley is found (the s00 event), the sub screen terminal, the door opens.
static void r117_EventAshleyFind()
{
    cEmDoor* door;

    ScfFlagOn(pG, SCF_R117_FIND_ASHLEY);
    ScfFlagOff(pG, SCF_90);
    if (W->evd0->waitLoadOk() == 1) {
        MemorySwap(W->mod->pArc, (u32) W->evd0->m_addr, W->evd0->m_size);
        EvtMgr.SetEvt(W->mod->pArc, (u32*) 0);
        while (EvtMgr.IsAliveEvt(&EvtMgr.NowExeEvtKey, 0, 0) != 0) {
            SceSleep(1);
        }
        SysFlagOn(pG, SYS_SCREEN_STOP);
        MemorySwap(W->mod->pArc, (u32) W->evd0->m_addr, W->evd0->m_size);
        W->evd0->setCommand(CMND_DEL_DATA, 0, 0);
    }
    StaFlagOn(pG, STA_SUB_ASHLEY);
    SubCharInit(1, &pPL->pos, pPL->ang.y);
    SubCharCtrl(SCC_BEHIND, 0);
    SceSleep(2);
    OpeOwTypeSet(3);
    OpeSetOpenTerm(0xA, 0.0f, 0.0f, 0.0f, 0.0f);
    if (pG->game_cnt == 0) {
        FadeSetW(1, 0, 0, 0);
        SceAtExecute(0x8F);
        SceSleep(1);
    }
    W->evd1->setCommand(CMND_ARAM_LOAD, 0, 0);
    SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r117_EventSaddlerAppear, 0, 1);
    if (getRoomEtcDoor(0, &door, 1)) {
        door->setNormal();
    }
}

// Area 6: Saddler appears (the s10 event), chapter 1-3 ends.
static void r117_EventSaddlerAppear()
{
    Vec pos = {9245.0f, -2507.0f, 2706.0f};
    Vec ang;
    Vec* pa = &ang;
    Event* ev;

    SceEventStart(0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    EmMgr.destroy(pSUB);
    StaFlagOff(pG, STA_SUB_ASHLEY);
    SceSleep(3);
    if (W->evd1->waitLoadOk() == 1) {
        MemorySwap(W->mod->pArc, (u32) W->evd1->m_addr, W->evd1->m_size);
        if (EvtMgr.SetEvt(W->mod->pArc, (u32*) &ev)) {
            ev->StatusFlag |= EvtStfBit(EvtStfFadeOut);
        }
        while (EvtMgr.IsAliveEvt(&EvtMgr.NowExeEvtKey, 0, 0) != 0) {
            SceSleep(1);
        }
        MemorySwap(W->mod->pArc, (u32) W->evd1->m_addr, W->evd1->m_size);
        W->evd1->setCommand(CMND_DEL_DATA, 0, 0);
    }
    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM01, 0, 0);
    void* zero = 0;
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM01, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM01, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x27, 0x2001, ESP_CORE_KIND_ROOM01, zero, zero);
    SceEventEnd(0);
    f32 ry = -0.46134f;
    StaFlagOn(pG, STA_SUB_ASHLEY);
    cPlayer* pl = pPL;
    Vec* pp = &pos;
    pl->setPos(pp);
    ang.x = 0.0f;
    pa->y = ry;
    ang.z = 0.0f;
    pl->setAng(pa);
    SubCharInit(1, &pPL->pos, pPL->ang.y);
    SubCharCtrl(SCC_CHASE, 0);
    SndBgmTblSet(0x117, 1);
    SceSetChapterEnd(CHAPTER_2_1, -1);
    EstSet(pPL, -1, 0, 0, EFF_PL00, 2, 0x800, ESP_CORE_KIND_NONE, zero, zero);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 0x26, 0x800, ESP_CORE_KIND_NONE, zero, zero);
}

static void (*r117_lightMechTbl[2])() = {r117_LightMechanismInit, r117_LightMechanismMove};

// Area 8: the mechanism task.
static void r117_LightMechanism()
{
    W->stepMode = 0;
    for (;;) {
        r117_lightMechTbl[W->step]();
        SceSleep(1);
    }
}

// Mechanism step 0: event start; the first time (Room_flg bit 2) camera cut 4 shows the beams lighting
// up (LightSet(1), four effects, SE 0xF); then camera cut 2 on the dials and -> step 1 (Move).
static void r117_LightMechanismInit()
{
    SceEventStart(1);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        CamCtrl.CutCall(4);
        SceSleep(30);
        r117_LightSet(1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        RoomSeCall(0xF, 0, 0, 0, 0);
        RsfSet(G_ROOM_ID, 2);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSleep(30);
    }
    CamCtrl.CutCall(2);
    W->step++;
}

// The mechanism menu: pick a light (mode 0), turn it (mode 1), check the pattern (mode 2), open
// the way (mode 3).
static void r117_LightMechanismMove()
{
    u8 effTbl[4][4][4] = {
        {{0x17, 0x18, 0x16, 0x15}, {0x21, 0x24, 0x23, 0x22}, {0x1D, 0x20, 0x1F, 0x1E}, {0x19, 0x1C, 0x1B, 0x1A}},
        {{0x1A, 0x19, 0x1C, 0x1B}, {0x15, 0x17, 0x18, 0x16}, {0x22, 0x21, 0x24, 0x23}, {0x1E, 0x1D, 0x20, 0x1F}},
        {{0x1F, 0x1E, 0x1D, 0x20}, {0x1B, 0x1A, 0x19, 0x1C}, {0x16, 0x15, 0x17, 0x18}, {0x23, 0x22, 0x21, 0x24}},
        {{0x24, 0x23, 0x22, 0x21}, {0x20, 0x1F, 0x1E, 0x1D}, {0x1C, 0x1B, 0x1A, 0x19}, {0x18, 0x16, 0x15, 0x17}},
    };
    f32 angTbl[4] = {0.0f, 1.0f, 2.0f, 3.0f};
    cEsp* esp2;
    int i;
    int sel;

    switch (W->mode) {
    case 0:
        cMes.MesSet(3, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 0x100012, 0, 0, 4);
        sel = SceMesGetSelection();
        switch (sel) {
        case -1:
        case 5:
            r117_LightMechanismEndProc(0);
            SceExit();
            W->step++;
            break;
        case 4:
            W->mode = 2;
            break;
        default:
            W->mode++;
            W->sel = sel - 1;
            break;
        }
        break;
    case 1: {
        s8 oldCur = W->cur[W->sel];
        s8 oldTgt = W->tgt[W->sel];

        cMes.MesSet(4, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 0x100012, 0, 0, 4);
        if (SceMesGetSelection() != 1) {
            W->mode--;
        } else {
            W->cur[W->sel]--;
            W->tgt[W->sel]--;
            W->cur[W->sel] = W->cur[W->sel] < 0 ? 3 : (W->cur[W->sel] > 3 ? 0 : W->cur[W->sel]);
            W->tgt[W->sel] = W->tgt[W->sel] < 0 ? 3 : (W->tgt[W->sel] > 3 ? 0 : W->tgt[W->sel]);
            r117_LightRotate(W->sel, -1.0f);
            RsfClear(G_ROOM_ID, W->sel * 4 + oldCur + 3);
            RsfClear(G_ROOM_ID, W->sel * 4 + oldTgt + 0xF);
            RsfSet(G_ROOM_ID, W->sel * 4 + W->cur[W->sel] + 3);
            RsfSet(G_ROOM_ID, W->sel * 4 + W->tgt[W->sel] + 0xF);
        }
        break;
    }
    case 2: {
        cEsp* esp;

        CamCtrl.CutCall(6);
        r117_LightSet(2);
        esp = W->esp[0];
        while (r117_espEnd(esp) == 0) {
            SceSleep(1);
        }
        EffectEspDelete(1, ESP_CORE_KIND_ROOM01, 0, 0);
        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM01, 0);
        EffectEfmDelete(1, ESP_CORE_KIND_ROOM01, 0);
        r117_LightSet(4);
        if (EspEstSetSelect(EFF_ROOM, effTbl[W->cur[0]][W->cur[1]][W->cur[2]], 0, &esp2, 1) == 1) {
            esp2->m_Ang.z = angTbl[W->cur[0]] * 1.5707964f;
        }
        if (W->cur[0] == 0 && W->cur[1] == 0 && W->cur[2] == 0) {
            RoomSeCall(4, 0, 0, 0, 0);
            EstSet(0, -1, 0, 0, EFF_ROOM, 4, 1, ESP_CORE_KIND_ROOM00, 0, 0);
            SceSleep(60);
            W->mode++;
        } else {
            SceSleep(15);
            SceMesSet(5, 0x20, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
            CamCtrl.CutCall(7);
            r117_LightSet(3);
            EstSet(0, -1, 0, 0, EFF_ROOM, 0x11, 1, ESP_CORE_KIND_ROOM01, 0, 0);
            esp = W->esp[0];
            while (r117_espEnd(esp) == 0) {
                SceSleep(1);
            }
            r117_LightSet(1);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            CamCtrl.CutCall(2);
            W->mode = 0;
        }
        break;
    }
    case 3:
        RsfSet(G_ROOM_ID, 1);
        SceAtDataReset(8);
        CamCtrl.CutCall(5);
        W->se = RoomSeCall(0xD, 0, 0, 0, 0);
        SceSetEventCancel(1, (TaskFunc) r117_LightMechanismEndProc, 1, -1, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x28, 1, ESP_CORE_KIND_NONE, 0, 0);
        {
            f32 spd = 22.0f;

            for (i = 0; i < 150; i++) {
                SmdGetObjPtr(1)->pos.y += spd;
                SmdGetObjPtr(1)->matUpdate();
                SmdGetObjPtr(2)->pos.y += spd;
                SmdGetObjPtr(2)->matUpdate();
                if (i == 0x6E) {
                    RoomSeCall(0xE, 0, 0, 0, 0);
                }
                SceSleep(1);
            }
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r117_LightMechanismEndProc(1);
        SceExit();
        break;
    }
}

// Leave the mechanism (mode 1: cancelled while the way opens).
static void r117_LightMechanismEndProc(int mode)
{
    Vec pos = {-2910.0f, 4000.0f, 6610.0f};
    Vec ang;

    if (mode != 0) {
        void* zero = 0;

        SndStop(W->se, 0);
        r117_MechanismDisarm();
        RsfClear(G_ROOM_ID, 2);
        EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
        EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
        r117_LightSet(0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x25, 0x801, ESP_CORE_KIND_ROOM00, zero, zero);
    }
    f32 ry = -3.11f;
    cPlayer* pl = pPL;
    Vec* pa = &ang;
    pl->setPos(&pos);
    ang.x = 0.0f;
    pa->y = ry;
    ang.z = 0.0f;
    pl->setAng(pa);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Turn beam and object `no` a quarter turn in direction `dir` over 10 frames.
extern "C" void r117_LightRotate(int no, f32 dir)
{
    f32 step[10] = {1.0f, 3.0f, 6.0f, 10.0f, 15.0f, 20.0f, 25.0f, 6.0f, 3.0f, 1.0f};
    int i;

    RoomSeCall(5, 0, 0, 0, 0);
    for (i = 0; i < 10; i++) {
        f32 d = step[i] * 0.017453292f * dir;

        W->esp[no]->m_Ang.z += d;
        W->light[no]->ang.z += d;
        W->light[no]->matUpdate();
        SceSleep(1);
    }
    W->esp[no]->m_Ang.z = (f32) W->cur[no] * 1.5707964f;
    W->light[no]->ang.z = (f32) W->tgt[no] * 1.5707964f;
    W->light[no]->matUpdate();
}

// The lights are solved: areas 9/0xA/0xC/0xD/3/4 off, the gate objects 1/2 hidden (the way up opens),
// area 5 (the stairs) on.
extern "C" void r117_MechanismDisarm()
{
    SceAtSetEnable(9, 0);
    SceAtSetEnable(0xA, 0);
    SmdGetObjPtr(1)->be_flag &= ~2;
    SmdGetObjPtr(2)->be_flag &= ~2;
    SceAtSetEnable(0xC, 0);
    SceAtSetEnable(0xD, 0);
    SceAtSetEnable(4, 0);
    SceAtSetEnable(3, 0);
    SceAtSetEnable(5, 1);
}

// Area 4: the player swings across on the chandelier.
static void r117_EventChandelier()
{
    f32 pz = 0.0f;
    f32 px = 0.0f;
    Vec ang;
    void* motPl = 0;
    void* motSmd = 0;
    int dir = 0;
    u32 cnt;
    int ok;
    int loop;

    pPL->beginEvent(0);
    W->smd->beginEvent(0);
    pPL->pos.x = -258.0f;
    pPL->pos.z = r117_smdPos.z - 5927.0f;
    {
        f32 ry;
        cPlayer* pl;

        pl = pPL;
        ry = ((Vec*) &r117_smdRot)->y; // non-const view: the load stays below the pos.z store
        pl->setPos(&pl->pos);
        ang.x = 0.0f;
        ang.z = 0.0f;
        ang.y = ry;
        pl->setAng(&ang);
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 3, 0, 1, 0);
    W->smd->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 3, 0, 1, 0);
    PlSeCall(0x29, &pPL->pos, 0, 0, 0);
    cnt = 0;
    do {
        if (cnt++ == 0x1D) {
            RoomSeCall(8, &pPL->pos, 0, 0, 0);
        }
        if (MotionGetState(pPL) & 4) {
            break;
        }
        SceSleep(1);
    } while (1);
    // The dead loop's notes keep this block's pPL `lis` out of the first loop's cse path (the original
    // re-materialises pPL@ha here); `cnt = 0` after the call keeps flow's `(use 0)` nop out of the
    // sched1 slot before the second loop (the same lever as r208 footingB_up).
    do { } while (0);
    ok = 1;
    loop = 1;
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 3, 0, 5, 0);
    W->smd->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 3, 0, 5, 0);
    RoomSeCall(0x13, &pPL->pos, 0, 0, 0);
    cnt = 0;
    do {
        if (MotionGetState(pPL) & 1) {
            RoomSeCall(0x13, &pPL->pos, 0, 0, 0);
        }
        if (ok != 0) {
            ActBtn.set(ACT_JUMP_MOVE, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
            if (Key.trg & 0x80000ULL) {
                if (pPL->pos.z >= -1000.0f) {
                    dir = 1;
                    px = -606.0f;
                    pz = r117_smdPos.z + 1964.0f;
                    motPl = ROOM_ARC_PTR(pG->pRoom, 0x27);
                    motSmd = ROOM_ARC_PTR(pG->pRoom, 0x23);
                } else {
                    dir = 0;
                    px = -647.0f;
                    pz = r117_smdPos.z + 926.0f;
                    motPl = ROOM_ARC_PTR(pG->pRoom, 0x28);
                    motSmd = ROOM_ARC_PTR(pG->pRoom, 0x24);
                }
                loop = 0;
            }
        }
        cnt++;
        if (cnt > 0x31) {
            ok = 1;
        }
        SceSleep(1);
    } while (loop != 0);
    pPL->pos.x = px;
    pPL->pos.z = pz;
    {
        f32 ry;
        cPlayer* pl;

        pl = pPL;
        ry = pl->ang.y;
        pl->setPos(&pl->pos);
        ang.x = 0.0f;
        ang.z = 0.0f;
        ang.y = ry;
        pl->setAng(&ang);
    }
    pPL->motionSet(motPl, 3, 0, 0x201, 0);
    W->smd->motionSet(motSmd, 3, 0, 1, 0);
    PlSeCall(0x29, &pPL->pos, 0, 0, 0);
    cnt = 0;
    while (!(MotionGetState(pPL) & 4)) {
        cnt++;
        if (dir == 1) {
            if (cnt == 0x1F) {
                FootSeCall(0xD, &pPL->pos, 0, 0);
            } else if (cnt == 0x21) {
                FootSeCall(0xE, &pPL->pos, 0, 0);
            }
        } else {
            if (cnt == 0x26) {
                FootSeCall(SE_LEON_FALL_BODY, &pPL->pos, 0, 0);
            }
        }
        if (cnt == 0x1E) {
            RoomSeCall(0x14, &W->smd->pos, 0, 0, 0);
        }
        SceSleep(1);
    }
    pPL->endEvent(0);
    W->smd->endEvent(0);
}

// Lightning on: the window object 0 to the bright colour (0x5F/0x87/0x9B).
static void r117_ThunderFlagOn()
{
    SmdGetObjPtr(0)->pModelInfo->color[0] = 0x5F;
    SmdGetObjPtr(0)->pModelInfo->color[1] = 0x87;
    SmdGetObjPtr(0)->pModelInfo->color[2] = 0x9B;
}

// Lightning off: the window object 0 back to its dim colour (0x32/0x35/0x35).
static void r117_ThunderFlagOff()
{
    u8 c = 0x35;

    SmdGetObjPtr(0)->pModelInfo->color[0] = 0x32;
    SmdGetObjPtr(0)->pModelInfo->color[1] = c;
    SmdGetObjPtr(0)->pModelInfo->color[2] = c;
}

// Thunder every 90..235 frames (150..295 after the first), lit through the effect tool state.
static void r117_ThunderMove()
{
    int cnt;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    EffSetToolStateCallBack(0, r117_ThunderFlagOn, r117_ThunderFlagOff);
    for (;;) {
        if (cnt == 0) {
            if (EffGetAreaState(2) != 0) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 0x14, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            {
                u8 r = Rnd() % 30;
                cnt = r * 8 + 150;
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}

// Event r117s00 handler: the etc models, the chandelier rope and the light sources.
extern "C" void Evt_R117S00_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        setRoomEtcDisp(0, 0, 1);
        setRoomEtcDisp(3, 0, 1);
        setRoomEtcDisp(4, 0, 1);
        setRoomEtcDisp(5, 0, 1);
        setRoomEtcDisp(6, 0, 1);
        setRoomEtcDisp(9, 0, 1);
        setRoomEtcDisp(0xA, 0, 1);
        break;
    case 1: {
        switch (e->NowCut) {
        case 4:
        case 6:
        case 7: {
            void* mod;

            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                    OBJ18_WK((cObj18*) mod)->be_flag |= 0x40;
                }
            }
            break;
        }
        default: {
            void* mod;

            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                    OBJ18_WK((cObj18*) mod)->be_flag &= ~0x40;
                }
            }
            break;
        }
        }
        void* mod;
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                    ((cModel*) mod)->LightInfo.EnableMask = 0x40;
                }
                if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                    ((cModel*) mod)->LightInfo.EnableMask = 0x20;
                }
                if (e->GetMod(&mod, "evmb300", 0, 0) == 1) {
                    ((cModel*) mod)->LightInfo.EnableMask = 8;
                    ((cModel*) mod)->be_flag |= 0x80;
                }
                if (e->GetMod(&mod, "evmb310", 0, 0) == 1) {
                    ((cModel*) mod)->LightInfo.EnableMask = 8;
                    ((cModel*) mod)->be_flag |= 0x80;
                }
            }
            break;
        case 6:
        case 7:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
            break;
        }
        break;
    }
    case 2:
        setRoomEtcDisp(0, 1, 1);
        setRoomEtcDisp(3, 1, 1);
        setRoomEtcDisp(4, 1, 1);
        setRoomEtcDisp(5, 1, 1);
        setRoomEtcDisp(6, 1, 1);
        setRoomEtcDisp(9, 1, 1);
        setRoomEtcDisp(0xA, 1, 1);
        break;
    }
}

// Event r117s10 handler: the event models' light sets, the ev0101 texture swap, the white fades.
extern "C" void Evt_R117S10_Func(Event* e)
{
    void* mod;
    void* mod2;
    void* bin;

    if (e->FuncType != 1) {
        return;
    }
    switch (e->NowCut) {
    case 8:
    case 9:
    case 10:
        if (e->NowFrame == 0) {
            if (e->GetMod(&mod, "evm4200", 0, 0) == 1) {
                cLight* l = LightMgr.getKindLight(1);

                if (l != 0) {
                    l->setParent((cModel*) mod);
                }
            }
        }
        break;
    }
    switch (e->NowCut) {
    case 0: {
        void* m;

        if (e->NowFrame == 0) {
            if (e->GetMod(&m, "evm3100", 0, 0) == 1) {
                ((cModel*) m)->LightInfo.EnableMask = 0x10;
            }
            if (e->GetMod(&m, "pl0100", 0, 0) == 1) {
                ((cModel*) m)->LightInfo.EnableMask = 0x40;
            }
            if (e->GetMod(&mod2, "ev0101", 0, 0) == 1) {
                W->evBin = ((cModelInfo*) mod2)->model_addr;
                W->evTpl = ((cModelInfo*) mod2)->tpl_addr;
            }
            if (e->GetMod(&m, "em3000", 0, 0) == 1) {
                ((cModel*) m)->be_flag |= 0x10;
                ((cModel*) m)->be_flag |= 0x04000000;
                ((cModel*) m)->be_flag |= 0x01000000;
            }
            if (e->GetMod(&m, "evm5000", 0, 0) == 1) {
                ((cModel*) m)->be_flag |= 0x10;
                ((cModel*) m)->be_flag |= 0x04000000;
                ((cModel*) m)->be_flag |= 0x01000000;
                ((cModel*) m)->LightInfo.EnableMask = 2;
            }
            if (e->GetMod(&m, "evm5010", 0, 0) == 1) {
                ((cModel*) m)->be_flag |= 0x10;
                ((cModel*) m)->be_flag |= 0x04000000;
                ((cModel*) m)->be_flag |= 0x01000000;
                ((cModel*) m)->LightInfo.EnableMask = 2;
            }
            if (e->GetMod(&m, "obm5500", 0, 0) == 1) {
                ((cModel*) m)->be_flag |= 0x10;
                ((cModel*) m)->be_flag |= 0x04000000;
                ((cModel*) m)->be_flag |= 0x01000000;
            }
        }
        break;
    }
    case 7:
        if (e->NowFrame == 0x23) {
            int skip = 1;

            if (!(e->StatusFlag & EvtStfBit(EvtStfToolFrontExec))) {
                skip = 0;
            }
            if (skip == 0) {
                SceExec(0x12, (TaskFunc) R117S0_WhiteFade, 0, 2, SCE_PRIO_DEF_2, 0);
            }
        }
        break;
    case 8:
    case 9:
        if (e->NowFrame == 0x19) {
            int skip = 1;

            if (!(e->StatusFlag & EvtStfBit(EvtStfToolFrontExec))) {
                skip = 0;
            }
            if (skip == 0) {
                SceExec(0x12, (TaskFunc) R117S0_WhiteFade, 0, 2, SCE_PRIO_DEF_2, 0);
            }
        }
        break;
    case 10:
        if (e->NowFrame == 0) {
            if (e->GetMod(&mod2, "ev0101", 0, 0) == 1) {
                if (EvtMgr.GetBin(&bin, "event/model/ev0100/ev0100a.tpl", 0) == 1) {
                    ((cModelInfo*) mod2)->setTplAddr(bin);
                }
            }
        }
        if (e->NowFrame == 0x55) {
            int skip = 1;

            if (!(e->StatusFlag & EvtStfBit(EvtStfToolFrontExec))) {
                skip = 0;
            }
            if (skip == 0) {
                SceExec(0x12, (TaskFunc) R117S0_WhiteFade, 0, 2, SCE_PRIO_DEF_2, 0);
            }
        }
        break;
    case 0xB:
        if (e->NowFrame == 0) {
            if (e->GetMod(&mod2, "ev0101", 0, 0) == 1) {
                ((cModelInfo*) mod2)->setTplAddr(W->evTpl);
            }
        }
        break;
    case 0x14:
        if (e->NowFrame == 0) {
            SmdGetObjPtr(0x27)->be_flag &= ~2;
            SmdGetObjPtr(0x28)->be_flag &= ~2;
        }
        break;
    case 0x20:
        if (e->NowFrame == 0) {
            SmdGetObjPtr(0x2E)->be_flag &= ~2;
        }
        break;
    }
}

// White flash: fade to white over 5 frames, then back.
static void R117S0_WhiteFade()
{
    r117_fadeWhite(2, 0, 0xFFFFFFFF);
    while (r117_fadeWork(2)->flags & 1) {
        SceSleep(1);
    }
    r117_fadeWhite(0x80000002, 0xFFFFFFFF, 0);
}
