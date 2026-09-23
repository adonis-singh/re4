// game/emwindow.cpp: window / fence enemy (cEmWindow): breakable room windows the player and
// the enemies jump through (ExeWindowEvent), with a scenario field (SceAtCreateFieldAt) on
// each side.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "event.h"
#include "emwindow.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "sce.h"
#include "player.h"
#include "pl_wep.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "motion.h"
#include "obj13.h"
#include "est.h"
#include "em_sub.h"
#include "sce_at.h"

// One row of WindowData (0x48 bytes), indexed by cModel::type (WindowType).
struct WindowDataRow {
    int field;        // 0x00  1: scenario field / collision quad on each side
    int hpType;       // 0x04  0: any group-0 weapon breaks it, 1: -500, 2: -250
    int breakEff;     // 0x08  1: effect 8 when the window starts broken
    int breakEff2;    // 0x0C  1: effect 9 when the model changes
    char bin[12];     // 0x10  broken model ("" = hide the model)
    char tpl[12];     // 0x1C
    f32 sizeX;        // 0x28
    f32 sizeY;        // 0x2C
    f32 sizeZ;        // 0x30
    int lowSat;       // 0x34  1: the collision quad sits half way down
    int satType;      // 0x38  1: scenario quad 0x40 / effect quad 0x400000, else the cube form
    int frame;        // 0x3C  1: 200 units of frame around the glass
    int amb;          // 0x40  EtcSetAddAmb argument
    u8 type2;         // 0x44  cModel::x12F
    u8 pad_45[3];
};


WindowDataRow WindowData[29] = {
    { 1, 1, 0, 0, "et0001.bin", "et0000.tpl", 1300.0f, 1400.0f, 1.0f, 0, 1, 1, 6, 1 },
    { 1, 3, 0, 0, "et0701.bin", "et0700.tpl", 1300.0f, 1400.0f, 1.0f, 0, 1, 1, 6, 0 },
    { 0, 1, 0, 0, "et1d01.bin", "et1d00.tpl", 2200.0f, 3800.0f, 1.0f, 0, 1, 1, 6, 1 },
    { 0, 0, 1, 0, "", "", 800.0f, 1010.0f, 1.0f, 1, 1, 1, 6, 1 },
    { 1, 2, 1, 0, "et2901.bin", "et2900.tpl", 2800.0f, 1400.0f, 1.0f, 1, 1, 1, 6, 1 },
    { 0, 1, 0, 0, "et2c01.bin", "et2c00.tpl", 1000.0f, 650.0f, 1.0f, 0, 1, 1, 6, 1 },
    { 0, 0, 1, 0, "et3501.bin", "et3500.tpl", 2400.0f, 1500.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 0, 1, 0, "et3601.bin", "et3600.tpl", 1400.0f, 1500.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 0, 0, 0, "et4401.bin", "et4401.tpl", 2600.0f, 200.0f, 1.0f, 1, 1, 0, 6, 0 },
    { 0, 0, 1, 0, "", "", 1500.0f, 700.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 1, 2, 1, 0, "et4a01.bin", "et4a00.tpl", 2800.0f, 1400.0f, 1.0f, 1, 1, 1, 6, 1 },
    { 0, 1, 1, 0, "et5001.bin", "et5000.tpl", 1100.0f, 1100.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 1, 1, 0, "et5101.bin", "et5101.tpl", 800.0f, 900.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 0, 1, 0, "et5201.bin", "et5201.tpl", 1000.0f, 1400.0f, 1000.0f, 1, 0, 0, 6, 1 },
    { 0, 0, 1, 0, "et5301.bin", "et5301.tpl", 400.0f, 2000.0f, 400.0f, 1, 0, 0, 6, 1 },
    { 0, 0, 1, 1, "", "", 570.0f, 480.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 1, 0, 0, "et5501.bin", "et5501.tpl", 3600.0f, 2000.0f, 1.0f, 1, 1, 0, 7, 1 },
    { 0, 1, 0, 0, "et5601.bin", "et5601.tpl", 1500.0f, 1500.0f, 1.0f, 1, 1, 0, 7, 1 },
    { 0, 1, 0, 0, "et5701.bin", "et5701.tpl", 1500.0f, 1500.0f, 1.0f, 1, 1, 0, 7, 1 },
    { 0, 1, 0, 0, "et5801.bin", "et5801.tpl", 1200.0f, 1400.0f, 1.0f, 1, 1, 0, 14, 0 },
    { 0, 1, 0, 0, "et5a01.bin", "et5a01.tpl", 1200.0f, 1400.0f, 1.0f, 1, 1, 0, 15, 0 },
    { 0, 1, 0, 0, "et5b01.bin", "et5b01.tpl", 650.0f, 1100.0f, 1.0f, 1, 1, 0, 15, 1 },
    { 0, 1, 0, 0, "et5c01.bin", "et5c01.tpl", 650.0f, 1100.0f, 1.0f, 1, 1, 0, 15, 1 },
    { 0, 1, 0, 0, "et5d01.bin", "et5d01.tpl", 1700.0f, 1000.0f, 1.0f, 1, 1, 0, 16, 0 },
    { 0, 1, 0, 0, "et5e01.bin", "et5e01.tpl", 1700.0f, 1000.0f, 1.0f, 1, 1, 0, 16, 0 },
    { 0, 1, 0, 0, "et5f01.bin", "et5f01.tpl", 2800.0f, 1600.0f, 1.0f, 1, 1, 0, 15, 0 },
    { 0, 1, 0, 0, "et6001.bin", "et6001.tpl", 2800.0f, 1600.0f, 1.0f, 1, 1, 0, 15, 0 },
    { 0, 0, 1, 1, "", "", 570.0f, 480.0f, 1.0f, 1, 1, 0, 6, 1 },
    { 0, 0, 1, 1, "", "", 400.0f, 250.0f, 1.0f, 1, 1, 0, 6, 1 },
};


// Creates a window / fence enemy (id 0x46) from a model / TPL at pos / rot: `type` indexes
// WindowData (size, hp rule, break model, collision form), `etcNo` the room etc flag that
// remembers the broken state, `arc` the etc archive holding the break model and jump motions.
// NULL when no work or init fails.
cEmWindow* SetWindow(void* bin, void* tpl, Vec* pos, Vec* rot, int type, u8 etcNo, void* arc)
{
    cEmWindow* em;

    em = (cEmWindow*) EmMgr.create(0x46);
    if (em == 0) {
        pLog->err(0, 0, "SetWindow():create failed");
        return 0;
    }
    if (em->init(bin, tpl, pos, rot, type, etcNo, arc) == 0) {
        return 0;
    }
    return em;
}

// Window crossing test for a character moving pos0 -> pos1: the target field (SceAtCheckFieldInfo
// at pos1) must be field `id` of a window that allows this kind of fence user (player /
// partners: kind 1, enemies: kind 2; the player cannot cross an intact type 1 window; NPCs need a
// ladder-style approach), and the move must cross the window plane. Returns 1 with the crossing
// direction (window -z or +z in world), the window position, its status word and the window.
int ChkWindow(cModel* pModTar, Vec* pos0, Vec* pos1, int field_id, u16* etc_flag, Vec* pNorm, Vec* pCenter, cEmWindow** o_pEm)
{
    SceAtField* info;
    cEmWindow* win;

    if (o_pEm) {
        *o_pEm = 0;
    }
    if (pModTar == 0) {
        pLog->err(0, 0, "SceAtCheck : param error");
        return 0;
    }
    info = SceAtCheckFieldInfo(pos1);
    if (info == 0) {
        return 0;
    }
    if (info->value != field_id) {
        return 0;
    }
    win = (cEmWindow*) info->pModel;
    if (win == 0) {
        pLog->err(0, 0, "SceAtCheck : failed");
        return 0;
    }
    if (win->id != 0x46) {
        return 0;
    }
    if (pModTar->id <= 0xF) {
        if (win->ChkEnableFence(1) == 0) {
            return 0;
        }
    } else {
        if (win->ChkEnableFence(2) == 0) {
            return 0;
        }
    }
    if (pModTar->id == 0) {
        if ((!(win->ChkStatus() & 1)) && win->type == WindowTypeEt07) {
            return 0;
        }
    }
    if (pModTar->id != 3 && pModTar->id <= 0xF) {
        if (LadderNearCk(&win->pos) == 0) {
            return 0;
        }
    }
    if (Front_check(win, pos0, PI / 2) == 0 && Front_check(win, pos1, PI / 2) == 1) {
        pNorm->x = 0.0f;
        pNorm->y = 0.0f;
        pNorm->z = -1.0f;
        RotVector(pNorm, &win->ang, pNorm);
        pCenter->x = win->pos.x;
        pCenter->y = win->pos.y;
        pCenter->z = win->pos.z;
        if (o_pEm) {
            *o_pEm = win;
        }
        *etc_flag = win->ChkStatus();
        return 1;
    }
    if (Front_check(win, pos0, PI / 2) == 1 && Front_check(win, pos1, PI / 2) == 0) {
        pNorm->x = 0.0f;
        pNorm->y = 0.0f;
        pNorm->z = 1.0f;
        RotVector(pNorm, &win->ang, pNorm);
        pCenter->x = win->pos.x;
        pCenter->y = win->pos.y;
        pCenter->z = win->pos.z;
        if (o_pEm) {
            *o_pEm = win;
        }
        *etc_flag = win->ChkStatus();
        return 1;
    }
    return 0;
}

// Work setup: model, WindowData sizes, a hit box, scenario / effect collision quads (lowSat /
// satType forms), 1000 hp, unlockable, and for `field` windows the floor probe plus two scenario
// jump fields (SceAtCreateFieldAt, one on each side, 3000 reach, 80 degree cone). A window whose
// etc flag says broken loads its break model at once.
int cEmWindow::init(void* bin, void* tpl, Vec* pos_, Vec* rot_, int type_, u8 etcNo, void* arc)
{
    EmWindowWork* w = EMWINDOW_WK(this);
    Vec pt[4];
    Vec size;
    Vec satPos;
    SceAtField* out;
    f32 frame;
    int cube;
    int no;

    if (modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetWindow():modelInit failed");
        EmMgr.destroy(this);
        return 0;
    }
    EmObjInit();
    setEtc(etcNo);
    w->arc = arc;
    if (pos_) {
        pos = *pos_;
    }
    if (rot_) {
        ang = *rot_;
    }
    RotMatrix(l_mat, &ang);
    TransMatrix(l_mat, &pos);
    ScaleMatrix(l_mat, &scale);
    PSMTXCopy(l_mat, mat);
    type = type_;
    if (WindowData[type_].field == 0) {
        SetEnableFence(0, 0);
    }
    EtcSetAddAmb(this, WindowData[type].amb);
    switch (type) {
    case WindowTypeEt52:
    case WindowTypeEt53:
        pModelInfo->blend_mode = 2;
        break;
    }
    {
        static const Vec zero = { 0.0f, 0.0f, 0.0f };
        static const Vec lsize = { 2000.0f, 2000.0f, 2000.0f };

        LightInfo.init2(0, 1, &zero, &lsize, 0x10);
    }
    if (WindowData[type].frame == 1) {
        frame = 200.0f;
    } else {
        frame = 0.0f;
    }
    size.x = (WindowData[type].sizeX + frame * 2.0f) * 0.5f;
    size.y = WindowData[type].sizeY + frame * 2.0f;
    size.z = WindowData[type].sizeZ * 0.5f;
    satPos.x = 0.0f;
    satPos.z = 0.0f;
    if (WindowData[type].lowSat == 1) {
        satPos.y = -(WindowData[type].sizeY * 0.5f) - frame;
    } else {
        satPos.y = -frame;
    }
    if (WindowData[type].satType == 1) {
        cube = 0;
        if (WindowData[type].field == 1) {
            setSat(&satPos, 0x40, 0, 0, size.x, size.y, size.z);
        }
        setEat(&satPos, 0x400000, 0, 0, size.x, size.y, size.z);
    } else {
        cube = 1;
        if (WindowData[type].field == 1) {
            setSat(&satPos, 0x40, 0, 1, size.x, size.y, size.z);
        }
        setEat(&satPos, 0, 0, 1, size.x * 0.7f, size.y, size.z * 0.7f);
    }
    size.z = (WindowData[type].sizeZ + 50.0f) * 0.5f;
    setYarare(0, &satPos, 0x21, cube, size.x, size.y, size.z);
    be_flag &= ~0x01000000;
    hp_max = hp = 1000;
    setStatus(EM_STATUS_LOCKOFF);
    setStatus(EM_STATUS_ASHLEY_NO_HELP);
    atari.setPriority(PRI_LV3);
    atari.off();
    lockParts = 0;
    lockOfs.x = 0.0f;
    lockOfs.y = 0.0f;
    lockOfs.z = 0.0f;
    be_flag &= ~0x10;
    ot_type = WindowData[type].type2;
    setNoSuspend(1);
    if (WindowData[type].field == 1) {
        CalFloor();
        pt[0].x = 500.0f;
        pt[0].y = -2000.0f;
        pt[0].z = -1000.0f;
        pt[1].x = 500.0f;
        pt[1].y = -2000.0f;
        pt[1].z = 1000.0f;
        pt[2].x = -500.0f;
        pt[2].y = -2000.0f;
        pt[2].z = 1000.0f;
        pt[3].x = -500.0f;
        pt[3].y = -2000.0f;
        pt[3].z = -1000.0f;
        no = SceAtCreateFieldAt(this, pt, 3, 0, 0, 3000.0f, 5, 0.0f, 0, 1.3962635f, 1, &out);
        if (no == -1) {
            pLog->err(0, 0, "move : SceAt no create");
        }
        w->fieldAt[0] = no;
        pt[0].x = 500.0f;
        pt[0].y = -2000.0f;
        pt[0].z = -1000.0f;
        pt[1].x = 500.0f;
        pt[1].y = -2000.0f;
        pt[1].z = 1000.0f;
        pt[2].x = -500.0f;
        pt[2].y = -2000.0f;
        pt[2].z = 1000.0f;
        pt[3].x = -500.0f;
        pt[3].y = -2000.0f;
        pt[3].z = -1000.0f;
        no = SceAtCreateFieldAt(this, pt, 3, 0, 0, 3000.0f, 5, -PI, 0, 1.3962635f, 1, &out);
        if (no == -1) {
            pLog->err(0, 0, "move : SceAt no create");
        }
        w->fieldAt[1] = no;
    }
    return 1;
}

// Per-frame: damage check and the object base move; Rno0 0 records the rest rotation and lights
// the intact window's ambient effect (est 8 of eff, Core_kind 0x31), Rno0 1 applies the shake
// (random +-1 degree yaw for `shake` frames), Rno0 4 plays a motion.
void cEmWindow::move()
{
    EmWindowWork* w = EMWINDOW_WK(this);
    u8 eff;

    eff = getEff();
    DmCk();
    EmObjMove();
    switch (r_no_0) {
    case 0:
        w->rotBase.x = ang.x;
        w->rotBase.y = ang.y;
        w->rotBase.z = ang.z;
        r_no_0 = 1;
        if ((!(this->ChkStatus() & 1))) {
            if (WindowData[type].breakEff == 1) {
                EstSet(0, -1, &pos, &ang, eff, 8, 0x801, ESP_CORE_KIND_EMWINDOW00, this, 0);
            }
        }
        break;
    case 1:
        if (w->shake != 0) {
            ang.y = w->rotBase.y + (f32) (s8) ((s8) Rnd() % 3) * PI / 180.0f;
            w->shake--;
            if (w->shake <= 0) {
                ang.x = w->rotBase.x;
                ang.y = w->rotBase.y;
                ang.z = w->rotBase.z;
            }
        }
        break;
    case 2:
    case 3:
        break;
    case 4:
        MotionMove(this, 0);
        break;
    }
}

// Damage check: a damage volume hit or a registered weapon hit (not knife / grenades, and only
// while damage is enabled) takes hp by the window's hpType and the weapon size group (group 0
// small arms: instant / -500 / -250, groups 1 / 2 always break); at hp 0 breaks the window
// (SetBreakAll toward the hit), otherwise a crack SE and the hit est 5.
void cEmWindow::DmCk()
{
    int a = 0;
    int hit = 0;
    u8 eff;
    int pEmCatch;
    u8 wep;

    eff = getEff();
    pEmCatch = 0;
    if (hp > 0) {
        pEmCatch = DmgMgr.hitCheck(&pos, 0);
        switch (pEmCatch) {
        case 1:
        case 4:
        case 5:
        case 7:
            hit = 1;
            break;
        }
    }
    if (dmg.m_Flag == 0 && hit == 0) {
        return;
    }
    wep = dmg.m_Wep;
    dmg.m_Flag = 0;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0x2A) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    switch (wep) {
    case 0xB:
    case 0xC:
    case 0x11:
        dmg.m_Timer = 0;
        break;
    }
    if (ChkEnableDamage() == 0) {
        return;
    }
    switch (pEmCatch) {
    case 1:
    case 4:
    case 5:
    case 7:
        hp = 0;
        a = 0;
        break;
    }
    switch (GetWepSizeGroup(dmg.m_Wep)) {
    case 0:
        if (WindowData[type].hpType == 0) {
            hp = 0;
        }
        if (WindowData[type].hpType == 1) {
            hp -= 500;
        }
        if (WindowData[type].hpType == 2) {
            hp -= 250;
        }
        a = 0;
        break;
    case 1:
        hp = 0;
        a = 1;
        break;
    case 2:
        hp = 0;
        a = 0;
        break;
    default:
        if (hit == 0) {
            return;
        }
        break;
    }
    if (hp <= 0) {
        SetBreakAll(&dmg.m_PosFrom, a, 0);
    } else if (type == WindowTypeEt07) {
        if (eff != 0xFF) {
            EmDmBloodSet2(this, eff, 5, 0, 0, 0);
        }
    } else {
        SndCall(6, 0x2E, &pos, 0, 0, this);
        if (eff != 0xFF) {
            EmDmBloodSet3(this, eff, 5, 0, 0x801, 0x31);
        }
    }
}

// The jump-through event: picks the player's window motion (pl00537 / 538 / 536.fcv, or the
// stage 4 Ada set) by ChkBreakDir (1 from the front, 2 no floor behind, 0 default), runs it under
// SceEventStart with the ladder-style camera, breaks the window at the right frame (SetBreakAll
// size 1, event style) and moves the player through; returns 1 when done.
int cEmWindow::ExeWindowEvent(cEmWindow* pEm)
{
    EmWindowWork* w;
    Vec plPos;
    void* fcv[3];
    void* mot;
    int i;

    if (pEm == 0) {
        pLog->err(0, 0, "WindowEvent : ptr faild!");
        return 0;
    }
    w = EMWINDOW_WK(pEm);
    fcv[0] = GetEtcAddr(w->arc, "pl00537.fcv");
    fcv[1] = GetEtcAddr(w->arc, "pl00538.fcv");
    fcv[2] = GetEtcAddr(w->arc, "pl00536.fcv");
    mot = fcv[0];
    if (pG->stage_no == 4 && pG->pl_type == 2) {
        EvtMgr.GetEmWindowFcv(&fcv[0], &fcv[1], &fcv[2]);
    }
    SceEventStart(0);
    pEm->SetStatus(2);
    LadderEventTrans(0);
    pPL->beginEvent(0);
    pPL->setNoSuspend(1);
    plPos.x = pPL->pos.x;
    plPos.y = pPL->pos.y;
    plPos.z = pPL->pos.z;
    pPL->setFace(2);
    w->breakDir = pEm->ChkBreakDir(&pPL->pos);
    switch (w->breakDir) {
    case 0:
        pPL->pos.x = 90.0f;
        pPL->pos.y = -1000.0f;
        pPL->pos.z = -1610.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = 0.0f;
        pPL->ang.z = 0.0f;
        mot = fcv[0];
        break;
    case 1:
        pPL->pos.x = -210.0f;
        pPL->pos.y = -1000.0f;
        pPL->pos.z = 1990.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = PI;
        pPL->ang.z = 0.0f;
        mot = fcv[1];
        break;
    case 2:
        pPL->pos.x = 0.0f;
        pPL->pos.y = -1000.0f;
        pPL->pos.z = -980.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = 0.0f;
        pPL->ang.z = 0.0f;
        mot = fcv[2];
        break;
    }
    PSMTXMultVec(pEm->mat, &pPL->pos, &pPL->pos);
    pPL->ang.x = pPL->ang.x + pEm->ang.x;
    pPL->ang.y = pPL->ang.y + pEm->ang.y;
    pPL->ang.z = pPL->ang.z + pEm->ang.z;
    if (mot) {
        MotionSetCore(pPL, &pPL->Motion, mot, 0, 0, 0x201, 0);
    }
    for (i = 0; (pPL->Motion.Mot_state & 4) == 0; i++) {
        switch (w->breakDir) {
        case 0:
            if ((!(pEm->ChkStatus() & 1)) && i == 0xF) {
                pEm->SetBreakAll(&plPos, 1, 1);
            }
            if (i == 0x19 && pG->room_id == 0x11F) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            if (i == 0) {
                EstSet(pPL, -1, 0, 0, w->eff, 8, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            if (i == 3) {
                SndCall(1, 0x29, &pPL->pos, 0, 0, pPL);
            }
            if (i == 4) {
                SndCall(1, 0x34, &pPL->pos, 0, 0, pPL);
            }
            if (i == 5) {
                SndCall(1, 0x43, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x23) {
                SndCall(5, 5, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x29) {
                SndCall(5, 0x12, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x2C) {
                SndCall(5, 0x13, &pPL->pos, 0, 0, pPL);
            }
            break;
        case 1:
            if ((!(pEm->ChkStatus() & 1)) && i == 0x10) {
                pEm->SetBreakAll(&plPos, 1, 1);
            }
            if (i == 0) {
                EstSet(pPL, -1, 0, 0, w->eff, 2, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            if (i == 3) {
                SndCall(1, 0x29, &pPL->pos, 0, 0, pPL);
            }
            if (i == 4) {
                SndCall(1, 0x34, &pPL->pos, 0, 0, pPL);
            }
            if (i == 5) {
                SndCall(1, 0x43, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x2D) {
                SndCall(5, 5, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x33) {
                SndCall(5, 0x12, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x37) {
                SndCall(5, 0x13, &pPL->pos, 0, 0, pPL);
            }
            break;
        case 2:
            if ((!(pEm->ChkStatus() & 1)) && i == 0xD) {
                pEm->SetBreakAll(&plPos, 1, 1);
            }
            if (i == 0) {
                EstSet(pPL, -1, 0, 0, w->eff, 4, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            if (i == 3) {
                SndCall(1, 0x29, &pPL->pos, 0, 0, pPL);
            }
            if (i == 4) {
                SndCall(1, 0x34, &pPL->pos, 0, 0, pPL);
            }
            if (i == 5) {
                SndCall(1, 0x43, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x28) {
                SndCall(5, 5, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x37) {
                SndCall(5, 0x12, &pPL->pos, 0, 0, pPL);
            }
            if (i == 0x39) {
                SndCall(5, 0x13, &pPL->pos, 0, 0, pPL);
            }
            break;
        }
        SceSleep(1);
    }
    pPL->setFace(0);
    LadderEventTrans(1);
    SceEventEnd(0);
    return 1;
}

// Probes the floor 2000 units on both sides of the window; `floor` = 1 when one side drops more
// than 3000 (a jump down, not a walk through).
void cEmWindow::CalFloor()
{
    EmWindowWork* w = EMWINDOW_WK(this);
    Vec v;
    Vec bottom;
    Vec hit;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 2000.0f;
    RotVector(&v, &ang, &v);
    PSVECAdd(&v, &pos, &v);
    bottom.x = v.x;
    bottom.y = v.y - 320000.0f;
    bottom.z = v.z;
    SatMgr.hitCheck(&v, &bottom, &hit, 0, 0, 0);
    if (hit.y < pos.y - 3000.0f) {
        w->floor = 1;
    } else {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -2000.0f;
        RotVector(&v, &ang, &v);
        PSVECAdd(&v, &pos, &v);
        bottom.x = v.x;
        bottom.y = v.y - 320000.0f;
        bottom.z = v.z;
        SatMgr.hitCheck(&v, &bottom, &hit, 0, 0, 0);
        if (hit.y < pos.y - 3000.0f) {
            w->floor = 1;
        } else {
            w->floor = 0;
        }
    }
}

// 1 when there is no floor behind the window (CalFloor).
u8 cEmWindow::GetFloor()
{
    return EMWINDOW_WK(this)->floor;
}

// Break direction for a body at `p`: 1 in front of the window, else 2 when there is no floor
// behind it, else 0.
int cEmWindow::ChkBreakDir(Vec* pPos)
{
    if (Front_check(this, pPos, PI / 2) != 0) {
        return 1;
    }
    return GetFloor() == 1 ? 2 : 0;
}

// The window's room etc flag word (bit0 = broken); 0 when it has none.
int cEmWindow::ChkStatus()
{
    u16* flg;

    flg = GetEtcFlgPtr(getEtc(), pG->room_id);
    if (flg) {
        return *flg;
    }
    return 0;
}

// ORs `f` into the window's etc flag word.
void cEmWindow::SetStatus(u16 flag)
{
    u16* flg;

    flg = GetEtcFlgPtr(getEtc(), pG->room_id);
    if (flg) {
        *flg |= flag;
    }
}

// A body bumped the window: rattle SE, 10 frames of shake, -50 hp (never below 1).
int cEmWindow::SetShake()
{
    EmWindowWork* w = EMWINDOW_WK(this);

    if (type == WindowTypeEt07) {
        SndCall(6, 0x38, &pos, 0, 0, this);
    } else {
        SndCall(6, 0x3A, &pos, 0, 0, this);
    }
    w->shake = 10;
    hp -= 50;
    if (hp <= 0) {
        hp = 1;
    }
    return 1;
}

// Breaks the window (unless flag 3 says already broken): break SE by type, the shard est for the
// direction of `p` / size / event style, then the broken model and collision removal.
int cEmWindow::SetBreakAll(Vec* pPos, int break_size, int breakType)
{
    if (ChkEtcFlag(3) == 0) {
        switch (type) {
        case WindowTypeEt44:
            SndCall(6, 0x38, &pos, 0, 0, this);
            break;
        case WindowTypeEt07:
            SndCall(6, 0x39, &pos, 0, 0, this);
            break;
        default:
            SndCall(6, 0x3B, &pos, 0, 0, this);
            break;
        }
        SetBreakEsp(ChkBreakDir(pPos), break_size, breakType);
        SetBreakModel();
    }
    return 1;
}

// Swaps in the WindowData break model (or hides the model when none), spawns the optional est 9,
// removes the collision, sets etc bit0 (broken) and deletes the ambient effects (Core_kind 0x31).
int cEmWindow::SetBreakModel()
{
    EmWindowWork* w = EMWINDOW_WK(this);
    void* bin;

    if (strcmp(WindowData[type].bin, "") != 0) {
        bin = GetEtcAddr(w->arc, WindowData[type].bin);
        SetChangeModel(bin, GetEtcAddr(w->arc, WindowData[type].tpl));
    } else {
        be_flag &= ~2;
    }
    if (WindowData[type].breakEff2 == 1) {
        EstSet(0, -1, &pos, &ang, w->eff, 9, 1, ESP_CORE_KIND_NONE, 0, 0);
    }
    SetAtariOff();
    SetStatus(1);
    EffectEspDelete(0, ESP_CORE_KIND_EMWINDOW00, this, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_EMWINDOW00, this);
    EffectEfmDelete(0, ESP_CORE_KIND_EMWINDOW00, this);
    return 1;
}

// Re-initialises the model from another bin / TPL; 0 on failure.
int cEmWindow::SetChangeModel(void* bin, void* tpl)
{
    if (modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetEmObj : modelInit failed");
        return 0;
    }
    return 1;
}

// Removes the window's collision (atari, scenario and effect quads), hp 0, damage re-enabled.
int cEmWindow::SetAtariOff()
{
    atari.m_flag &= ~0x300;
    clrSat();
    clrEat();
    hp = 0;
    SetEnableDamage(1);
    return 1;
}

// Spawns the shard est of `eff`: id from tblA (weapon break) or tblB (event break) by direction
// (0..3) and size kind (0 small / 1 large), at the window origin with its rotation.
int cEmWindow::SetBreakEsp(int dir_type, int break_size, int breakType)
{
    Vec p;
    Vec r;
    int tblA[4][2] = { { 6, 0 }, { 7, 1 }, { 6, 0 }, { 7, 1 } };
    int tblB[4][2] = { { 6, 0 }, { 7, 1 }, { 3, 3 }, { 7, 1 } };
    u8 eff;
    int id;

    eff = getEff();
    if (break_size > 1) {
        break_size = 1;
        pLog->err(0, 0, "EmWindow : break_size limit over");
    }
    if (breakType == 0) {
        if (dir_type > 3) {
            dir_type = 0;
            pLog->err(0, 0, "EmWindow : espid faild!");
        }
        id = tblA[dir_type][break_size];
    } else {
        if (dir_type > 2) {
            dir_type = 0;
            pLog->err(0, 0, "EmWindow : espid faild!");
        }
        id = tblB[dir_type][break_size];
    }
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = 0.0f;
    r.x = 0.0f;
    r.y = 0.0f;
    r.z = 0.0f;
    PSMTXMultVec(mat, &p, &p);
    r.x += ang.x;
    r.y += ang.y;
    r.z += ang.z;
    EstSet(0, -1, &p, &r, eff, (u8) id, 0x801, ESP_CORE_KIND_NONE, 0, 0);
}

// Enables / disables weapon damage (etc bit 0 = disabled).
void cEmWindow::SetEnableDamage(int flag)
{
    int v = 1;

    if (flag == 1) {
        v = 0;
    }
    SetEtcFlag(0, v);
}

// 1 when weapon damage is enabled.
int cEmWindow::ChkEnableDamage()
{
    if (ChkEtcFlag(0) == 1) {
        return 0;
    }
    return 1;
}

// Sets / clears bit `no` of the window's own flag table (0 damage off, 1 / 2 fence kinds off, 3
// broken).
void cEmWindow::SetEtcFlag(u32 flag, int boolType)
{
    if (boolType == 1) {
        FlagOnVar(EMWINDOW_WK(this)->etcFlag, flag);
    } else {
        FlagOffVar(EMWINDOW_WK(this)->etcFlag, flag);
    }
}

// Bit `no` of the window's own flag table.
int cEmWindow::ChkEtcFlag(u32 flag)
{
    u32* flg = EMWINDOW_WK(this)->etcFlag;

    if (flg[flag >> 5] & (0x80000000 >> (flag & 0x1F))) {
        return 1;
    }
    return 0;
}

// Allows / forbids crossing for fence users of `kind` (1 player side, 2 enemy side, 0 both).
int cEmWindow::SetEnableFence(int flag, int enableFlag)
{
    EmWindowWork* w = EMWINDOW_WK(this);

    if (enableFlag == 0 || enableFlag == 1) {
        if (flag == 1) {
            EMWINDOW_WK(this)->etcFlag[0] &= ~0x40000000;
        } else {
            EMWINDOW_WK(this)->etcFlag[0] |= 0x40000000;
        }
    }
    if (enableFlag == 0 || enableFlag == 2) {
        if (flag == 1) {
            w->etcFlag[0] &= ~0x20000000;
        } else {
            w->etcFlag[0] |= 0x20000000;
        }
    }
    return 1;
}

// 1 when fence users of `kind` (1 / 2) may cross this window.
int cEmWindow::ChkEnableFence(int enableFlag)
{
    u32 t;

    if (enableFlag == 1) {
        t = EMWINDOW_WK(this)->etcFlag[0] & 0x40000000;
        return t == 0;
    }
    if (enableFlag == 2) {
        t = EMWINDOW_WK(this)->etcFlag[0] & 0x20000000;
        return t == 0;
    }
    return 0;
}
