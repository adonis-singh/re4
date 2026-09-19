// game/pl_class.cpp: cPlayer base class: action selection (wall / fence / window / ledge / jump
// checks and their action buttons), damage entry, event begin/end, eye / neck / waist control and
// the three-way motion blend (cMot3). Owns the cPlayer vtable.

#include "atari.h"
#include "dmg.h"
#include "light.h"
#include "player.h"
#include "pl_push.h"
#include "pl_npc.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "joy.h"
#include "snd.h"
#include "motion.h"
#include "math_sub.h"
#include "act_btn.h"
#include "emwindow.h"
#include "sce_sys.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "esp.h"
#include "rnd.h"
#include "em_sub.h"

extern "C" {
double atan2(double y, double x);

int actWallCheck(cPlayer* pl);
int fanceCheck(cPlayer* pl);
int windowCheck(cPlayer* pl, u8* dir, cEmWindow** out);
void fanceOn();
void windowOn(cEmWindow* w);
// Local `fallCheck` (player.cpp owns the global one): the split symbol keeps the address suffix.
static int fallCheck_80172E38(cPlayer* pl);
void fallOn();
void levelUpOn();
void levelDownOn();
void level2UpOn();
void level2DownOn();
void holdOn();
int jumpCheck(cPlayer* pl);
void jumpFallOn();
u32 upDownCk(cPlayer* pl);
}

// cEmWindow::ExeWindowEvent as a SceExec task (the member's address is not a plain function pointer).
int ExeWindowEventTask(cEmWindow* w) asm("ExeWindowEvent__9cEmWindow");

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)
// cPlNeck's checks compile to the folded `addis 0x8000; cmplwi 0x02FFFFFF` range form.
#define VALID_PTR2(p) ((u32) (p) - 0x80000000 <= 0x02FFFFFF)

// The original cUnit::beginEvent/endEvent take an int (KNOWN DEBT, cManager.h); view class for
// the r4 argument (emwindow.cpp BEGIN_EVENT).
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define END_EVENT(p, mode) ((cUnitEvent*) (p))->endEvent(mode)

// `flags &= 0xFFFE` through a u16 reference: the 16-bit mask survives (`rlwinm 16,30`, BitOff16
// gives `clrrwi`) and the following `pPL` load stays below the store (cPlNeck::move).
static inline void And16(u16& f, u16 m) { f &= m; }

// Routine bytes (state, routine, sub routine, step) written through an inline taking ints: the
// stores come out in the original's order (ff, fd, fc, fe for a plain routine change).
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->r_no_0 = r0;
    pl->r_no_1 = r1;
    pl->r_no_2 = r2;
    pl->r_no_3 = r3;
}

// Partner (id 3) dead while the player is in routine 0: routine 6 (die), damage info 0x80. An
// inline member of the class whose vtable this unit owns: emitted here after the destructor.
inline void cPlayer::subCharLiveCheck()
{
    cEm* sub = pSubEm;
    if (sub && sub->id == 3 && sub->hp <= 0 && r_no_0 == 0) {
        PlRoutineSet(this, 6, 0, 0, 0);
        dmg.set(0, 0x80);
        Wep->m_pWep->interrupt();
    }
}

const f32 PlReloadSpeedTbl[45][3] = {
    { 1.0f, 1.0f, 1.0f },
    { 51.0f, 44.0f, 25.0f },
    { 52.0f, 44.0f, 26.0f },
    { 71.0f, 66.0f, 50.0f },
    { 51.0f, 44.0f, 25.0f },
    { 110.0f, 90.0f, 70.0f },
    { 55.0f, 46.0f, 28.0f },
    { 91.0f, 73.0f, 45.0f },
    { 90.0f, 72.0f, 45.0f },
    { 120.0f, 97.0f, 70.0f },
    { 70.0f, 57.0f, 40.0f },
    { 71.0f, 58.0f, 35.0f },
    { 85.0f, 69.0f, 49.0f },
    { 1.0f, 1.0f, 1.0f },
    { 104.0f, 87.0f, 52.0f },
    { 110.0f, 86.0f, 55.0f },
    { 1.0f, 1.0f, 1.0f },
    { 52.0f, 44.0f, 26.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 91.0f, 73.0f, 45.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 51.0f, 44.0f, 25.0f },
    { 71.0f, 58.0f, 35.0f },
    { 71.0f, 58.0f, 35.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 52.0f, 44.0f, 26.0f },
    { 110.0f, 86.0f, 55.0f },
};

const f32 PlReloadEndTbl[45][3] = {
    { 1.0f, 1.0f, 1.0f },
    { 36.0f, 32.0f, 18.0f },
    { 36.0f, 32.0f, 18.0f },
    { 60.0f, 55.0f, 44.0f },
    { 38.0f, 32.0f, 19.0f },
    { 97.0f, 79.0f, 58.0f },
    { 46.0f, 40.0f, 25.0f },
    { 73.0f, 64.0f, 44.0f },
    { 70.0f, 60.0f, 47.0f },
    { 108.0f, 90.0f, 62.0f },
    { 57.0f, 46.0f, 32.0f },
    { 70.0f, 58.0f, 35.0f },
    { 85.0f, 69.0f, 49.0f },
    { 1.0f, 1.0f, 1.0f },
    { 103.0f, 77.0f, 77.0f },
    { 96.0f, 80.0f, 52.0f },
    { 1.0f, 1.0f, 1.0f },
    { 36.0f, 32.0f, 18.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 73.0f, 64.0f, 44.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 36.0f, 32.0f, 18.0f },
    { 70.0f, 58.0f, 35.0f },
    { 120.0f, 98.0f, 70.0f },
    { 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { 36.0f, 32.0f, 18.0f },
    { 36.0f, 32.0f, 18.0f },
};

const f32 PlShotFrameTbl[45][5] = {
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 14.0f, 12.0f, 10.0f, 10.0f, 10.0f },
    { 14.0f, 12.0f, 10.0f, 10.0f, 10.0f },
    { 16.0f, 14.0f, 12.0f, 12.0f, 12.0f },
    { 14.0f, 12.0f, 8.0f, 6.0f, 6.0f },
    { 21.0f, 21.0f, 21.0f, 21.0f, 21.0f },
    { 21.0f, 21.0f, 21.0f, 21.0f, 21.0f },
    { 46.0f, 46.0f, 46.0f, 46.0f, 46.0f },
    { 22.0f, 22.0f, 22.0f, 22.0f, 22.0f },
    { 20.0f, 20.0f, 20.0f, 20.0f, 20.0f },
    { 43.0f, 12.0f, 12.0f, 12.0f, 12.0f },
    { 3.0f, 3.0f, 3.0f, 3.0f, 3.0f },
    { 3.0f, 3.0f, 3.0f, 3.0f, 3.0f },
    { 72.0f, 72.0f, 72.0f, 72.0f, 72.0f },
    { 40.0f, 40.0f, 40.0f, 40.0f, 40.0f },
    { 35.0f, 35.0f, 35.0f, 35.0f, 35.0f },
    { 31.0f, 31.0f, 31.0f, 31.0f, 31.0f },
    { 14.0f, 12.0f, 10.0f, 10.0f, 10.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 20.0f, 20.0f, 20.0f, 20.0f, 20.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 46.0f, 46.0f, 46.0f, 46.0f, 46.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 14.0f, 12.0f, 10.0f, 10.0f, 10.0f },
    { 3.0f, 3.0f, 3.0f, 3.0f, 3.0f },
    { 20.0f, 20.0f, 20.0f, 20.0f, 20.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 16.0f, 16.0f, 16.0f, 16.0f, 16.0f },
    { 16.0f, 16.0f, 16.0f, 16.0f, 16.0f },
};

int PlKeyReloadType = 0;
int PlReloadDirect = 0;

// Wall in front of the player (400 up, 1000 ahead) with a fence/ledge behind it: the wall's hit
// point / normal / attribute go to actWallHit / actWallNrm / actWallAttr, x400 becomes the angle
// facing the wall. 1 when both shoulders (+-350) also see the wall.
int actWallCheck(cPlayer* pl)
{
    pl->m_ActAttr = 0;
    Vec p0 = { 0.0f, 400.0f, 0.0f };

    PSVECAdd(&p0, &pl->pos, &p0);
    Vec p1 = { 0.0f, 400.0f, 1000.0f };
    Vec hit;
    Vec nrm;
    Vec ofs;
    Vec rot;
    Vec p2;
    Vec p3;
    int ret;

    PSMTXMultVec(pl->mat, &p1, &p1);
    if (SatMgr.hitCheck(&p0, &p1, 0, &nrm, 0, 0) == 0) {
        return 0;
    }
    PSVECScale(&nrm, &p1, -1000.0f);
    PSVECAdd(&p1, &p0, &p1);
    ret = SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    if (ret == 0) {
        return 0;
    }
    PSVECScale(&nrm, &ofs, -1000.0f);
    pl->m_Fwork0 = (f32) atan2(-nrm.x, -nrm.z);
    rot.x = 0.0f;
    rot.y = pl->m_Fwork0;
    rot.z = 0.0f;
    p0.x = 350.0f;
    p0.y = 300.0f;
    p0.z = 0.0f;
    RotVector(&p0, &rot, &p0);
    PSVECAdd(&p0, &pl->pos, &p0);
    PSVECAdd(&p0, &ofs, &p1);
    ret &= SatMgr.hitCheck(&p0, &p1, 0, 0, 0, 0);
    if (ret == 0) {
        return 0;
    }
    p2.x = -350.0f;
    p2.y = 300.0f;
    p2.z = 0.0f;
    RotVector(&p2, &rot, &p2);
    PSVECAdd(&p2, &pl->pos, &p2);
    PSVECAdd(&p2, &ofs, &p3);
    ret &= SatMgr.hitCheck(&p2, &p3, 0, 0, 0, 0);
    if (ret == 0) {
        return 0;
    }
    pl->m_ActAttr = ret;
    pl->m_ActNorm = nrm;
    pl->m_ActCross = hit;
    return 1;
}

// Fence (wall attribute bit5) the player may climb: nothing between the shoulders behind the fence
// and a floor within 300 of the player's height there. Sets x3E0 (climb flag).
int fanceCheck(cPlayer* pl)
{
    Vec p0;
    Vec p1;
    Vec rot;
    Vec dir;
    Vec hit;
    int hit0;
    int hit1;

    if (StaFlagChk(pG, STA_NO_FENCE)) {
        return 0;
    }
    if ((pl->m_ActAttr & 0x20) == 0) {
        return 0;
    }
    rot.x = 0.0f;
    rot.y = pl->m_Fwork0;
    rot.z = 0.0f;
    PSVECScale(&pl->m_ActNorm, &dir, -1000.0f);
    p0.x = 400.0f;
    p0.y = 300.0f;
    p0.z = 0.0f;
    RotVector(&p0, &rot, &p0);
    PSVECAdd(&p0, &pl->pos, &p0);
    PSVECAdd(&p0, &dir, &p0);
    p1.x = -400.0f;
    p1.y = 300.0f;
    p1.z = 0.0f;
    RotVector(&p1, &rot, &p1);
    PSVECAdd(&p1, &pl->pos, &p1);
    PSVECAdd(&p1, &dir, &p1);
    hit0 = SatMgr.hitCheck(&p0, &p1, 0, 0, 0, 0);
    hit1 = SatMgr.hitCheck(&p1, &p0, 0, 0, 0, 0);
    if ((hit0 | hit1) != 0) {
        return 0;
    }
    PSVECScale(&pl->m_ActNorm, &p0, -1500.0f);
    PSVECAdd(&p0, &pl->m_ActCross, &p0);
    p1.x = p0.x;
    p1.y = p0.y - 10000.0f;
    p1.z = p0.z;
    SatMgr.hitCheck(&p0, &p1, &hit, 0, 0, 0);
    if (fabsf(hit.y - pl->pos.y) > 300.0f) {
        return 0;
    }
    pl->m_Work0 = 1;
    return 1;
}

// Window in front of the player: 1 with its break direction and the window when the player may go
// through it (and no rack is in the way); PlFancePos = 400 behind the window.
int windowCheck(cPlayer* pl, u8* dir, cEmWindow** out)
{
    Vec p0 = { 0.0f, 400.0f, 0.0f };

    PSVECAdd(&p0, &pl->pos, &p0);
    Vec p1 = { 0.0f, 400.0f, 1000.0f };
    Vec wdir;
    Vec wpos;
    u16 status;
    cEmWindow* win;
    Vec* d = &wdir;

    RotVector(&p1, &pl->ang, &p1);
    PSVECAdd(&p1, &pl->pos, &p1);
    if (ChkWindow(pl, &p0, &p1, 1, &status, d, &wpos, &win) == 1) {
        pl->m_Fwork0 = (f32) atan2(-d->x, -d->z);
        pl->m_Work0 = 0;
        PSVECScale(d, &PlFancePos, 400.0f);
        PSVECAdd(&PlFancePos, &wpos, &PlFancePos);
        if (EmRackCk(pl, &pl->pos, atan2f(-wdir.x, -wdir.z)) == 0) {
            return 0;
        }
        *dir = win->ChkBreakDir(&pl->pos);
        *out = win;
        return 1;
    }
    return 0;
}

// Action button: climb the fence (routine 1/0xC; x3E0 set = climb up, else a jump down with sub 5).
void fanceOn()
{
    cPlayer* pl = pPL;

    if (pl->m_Work0) {
        PlFanceFlag = 0;
        PlRoutineSet(pl, 0, 0xC, 0, 0);
    } else {
        PlRoutineSet(pl, 0, 0xC, 0, 5);
        PlFanceFlag = 1;
    }
    pl->dmg.set(0, 10);
}

// Action button: go through the window (its event as a scenario task).
void windowOn(cEmWindow* w)
{
    SceExec(0x12, (TaskFunc) ExeWindowEventTask, (int) w, 2, SCE_PRIO_DEF_2, 0);
    PlFanceFlag = 1;
}

// Ledge to drop from (wall attribute bits 4 / 20): fallDir = -wall normal.
static int fallCheck_80172E38(cPlayer* pl)
{
    if (pl->m_ActAttr & 0x100010) {
        pl->m_FallVec.x = -pl->m_ActNorm.x;
        pl->m_FallVec.y = pl->m_ActNorm.y;
        pl->m_FallVec.z = -pl->m_ActNorm.z;
        return 1;
    }
    return 0;
}

// Action button callback: jump down the ledge (routine 0/0xE pl_R1_Fall), invulnerable (dmg 0x80).
void fallOn()
{
    cPlayer* pl = pPL;

    PlRoutineSet(pl, 0, 0xE, 0, 0);
    pPL->dmg.set(0, 0x80);
}

// Action button callback: climb up the step (routine 0/7 pl_R1_LevelUp, m_Work0 kind 0).
void levelUpOn()
{
    cPlayer* pl = pPL;

    PlRoutineSet(pl, 0, 7, 0, 0);
    BitSet(pPL->m_Work0, 0);
    pPL->dmg.set(0, 10);
}

// Action button callback: climb down the step (routine 0/8 pl_R1_LevelDown, kind 0).
void levelDownOn()
{
    cPlayer* pl = pPL;

    PlRoutineSet(pl, 0, 8, 0, 0);
    BitSet(pPL->m_Work0, 0);
    pPL->dmg.set(0, 10);
}

// Action button callback: climb up the high step (kind 1).
void level2UpOn()
{
    cPlayer* pl = pPL;

    PlRoutineSet(pl, 0, 7, 0, 0);
    BitSet(pPL->m_Work0, 1);
    pPL->dmg.set(0, 10);
}

// Action button callback: climb down the high step (kind 1).
void level2DownOn()
{
    cPlayer* pl = pPL;

    PlRoutineSet(pl, 0, 8, 0, 0);
    BitSet(pPL->m_Work0, 1);
    pPL->dmg.set(0, 10);
}

// Action button callback: grab the pushable object (routine 0/9 pl_R1_ObjPush).
void holdOn()
{
    cPlayer* pl = pPL;

    PlRoutineSet(pl, 0, 9, 0, 0);
}

// Wall (attribute bit19) 600 ahead at both shoulders the player may jump over: jumpDir = -normal,
// jumpHeight = the floor 3800 behind it relative to the player (room 226 zeroes it past 1500).
int jumpCheck(cPlayer* pl)
{
    Vec p0;
    Vec p1;
    Vec nrm;
    Vec hit;
    Vec p2;
    int attr;
    f32 h;
    const f32 dist = 3800.0f;
    const f32 up = 1500.0f;

    if (StaFlagChk(pG, STA_PL_JUMP_OFF)) {
        return 0;
    }
    p0.x = 300.0f;
    p0.y = 400.0f;
    p0.z = 0.0f;
    PSMTXMultVec(pl->mat, &p0, &p0);
    p1.x = 300.0f;
    p1.y = 400.0f;
    p1.z = 600.0f;
    PSMTXMultVec(pl->mat, &p1, &p1);
    attr = SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    p0.x = -300.0f;
    p0.y = 400.0f;
    p0.z = 0.0f;
    PSMTXMultVec(pl->mat, &p0, &p0);
    p1.x = -300.0f;
    p1.y = 400.0f;
    p1.z = 600.0f;
    PSMTXMultVec(pl->mat, &p1, &p1);
    attr &= SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    if (attr & 0x80000) {
        pl->m_JumpVec.x = -nrm.x;
        pl->m_JumpVec.y = nrm.y;
        pl->m_JumpVec.z = -nrm.z;
        PSVECScale(&pl->m_JumpVec, &p2, dist);
        PSVECAdd(&p2, &hit, &p2);
        p2.y += up;
        h = SatMgr.getFloor(&p2, 600.0f, 100000.0f, 0, 0) - pl->pos.y;
        FSet(pl->m_JumpAdjY, h);
        if (pG->stage_no == 2 && pG->room_no == 0x26) {
            if (fabsf(h) > up) {
                pl->m_JumpAdjY = 0.0f;
            }
        } else {
            if (fabsf(h) > up) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

// Action button: jump over the wall (routine 1/0x13), facing jumpDir.
void jumpFallOn()
{
    cPlayer* pl = pPL;

    PlRoutineSet(pl, 0, 0x13, 0, 0);
    FSet(pPL->ang.y, pPL->ang.y + Muku3(&pl->m_JumpVec, pPL->ang.y, 3.1415927f));
    pPL->dmg.set(0, 0x80);
}

// Motion set with the damaged (m0/m1) or normal (m2/m3) pair by dmMotCk().
void cPlayer::motionSet(void* m0, void* seq0, void* m1, void* seq1, int hokan, int frame)
{
    if (dmMotCk()) {
        MotionSetCore(this, MOTION(this), m0, (int) seq0, hokan, 5, frame);
    } else {
        MotionSetCore(this, MOTION(this), m1, (int) seq1, hokan, 5, frame);
    }
}

// 1 when the player is hurt enough for the damaged motions (life above 2/3 of the maximum, or Leon
// with the rocket launcher).
int dmMotCk()
{
    int max;

    switch (pG->pl_type) {
    case 1:
        max = 600;
        break;
    case 2:
        max = 1200;
        break;
    case 0:
    default:
        max = 1200;
        break;
    }
    if (pG->pl_type == 0 && pG->weapon_no == 0xD && pG->weapon_type == 2) {
        return 1;
    }
    return (s16) pG->pl_life >= max * 2 / 3;
}

// Routine 1 (move) sub routine selection from the keys, then the action button prompts.
int cPlayer::actionSelect()
{
    u8 dir;
    cEmWindow* win;

    if (r_no_0 != 0) {
        return 0;
    }
    if ((Key.on & 1) && (Key.on & 2)) {
        return 0;
    }
    {
        {
            int zero = 0;

            StaFlagOn(pG, STA_SSCRN_ENABLE);
            if (joyKamae()) {
                PlRoutineSet(this, zero, 6, zero, zero);
                return 1;
            }
    if (joyLKamae()) {
        PlRoutineSet(this, 0, 0xB, 0, 0);
        return 1;
    }
    if (r_no_1 != 5 && (Key.trg & 0x100)) {
        PlRoutineSet(this, 0, 5, 0, 0);
        return 1;
    }
    if (r_no_1 == 0 && ((Key.on & 4) || (Key.on & 8))) {
        PlRoutineSet(this, 0, 4, 0, 0);
        return 1;
    }
    if (r_no_1 != 1 && r_no_1 != 3 && (Key.on & 1)) {
        PlRoutineSet(this, 0, 1, 0, 0);
        return 1;
    }
    if (r_no_1 != 2 && (Key.on & 2)) {
        PlRoutineSet(this, 0, 2, 0, 0);
        return 1;
    }
    if (keyReload() && Wep->m_pWep && Wep->m_pWep->reloadable()) {
        PlRoutineSet(this, 0, 6, 4, 0);
        m_Work0 = 1;
        return 1;
    }
    actWallCheck(this);
    if (Push->catchCheck()) {
        ActBtn.set(6, 2, (int) holdOn, 0, 0x10, 1, 0, 0);
    }
    if (fallCheck_80172E38(this)) {
        ActBtn.set(4, 2, (int) fallOn, 0, 0, 1, 0, 0);
    }
    if (fanceCheck(this)) {
        ActBtn.set(5, 2, (int) fanceOn, 0, 0, 1, 0, 0);
    }
    if (jumpCheck(this)) {
        ActBtn.set(5, 2, (int) jumpFallOn, 0, 0, 1, 0, 0);
    }
    if (windowCheck(this, &dir, &win)) {
        int broken = win->ChkStatus() & 1;
        if (broken) {
            if (dir == 2) {
                ActBtn.set(2, 2, (int) windowOn, (int) win, 0, 1, 0, 0);
            } else {
                ActBtn.set(5, 2, (int) fanceOn, 0, 0, 1, 0, 0);
            }
        } else {
            if (dir == 0) {
                ActBtn.set(2, 2, (int) windowOn, (int) win, broken, 1, 0, 0);
            }
            if (dir == 1) {
                ActBtn.set(3, 2, (int) windowOn, (int) win, broken, 1, 0, 0);
            }
            if (dir == 2) {
                ActBtn.set(2, 2, (int) windowOn, (int) win, broken, 1, 0, 0);
            }
        }
    }
    switch (upDownCk(this)) {
    case 1:
        ActBtn.set(8, 2, (int) levelUpOn, 0, 0, 1, 0, 0);
        break;
    case 2:
        ActBtn.set(9, 2, (int) levelDownOn, 0, 0, 1, 0, 0);
        break;
    case 3:
        ActBtn.set(8, 2, (int) level2UpOn, 0, 0, 1, 0, 0);
        break;
    case 4:
        ActBtn.set(0x40, 2, (int) level2DownOn, 0, 0, 1, 0, 0);
        break;
    }
    checkXbutton();
        }
    }
    return 0;
}

// Scenario damage areas (DmgMgr) against parts 0: fire / blast / spike damage.
void cPlayer::dmgCheck()
{
    int dead = 1;

    if (!dmg.m_Flag && !dmg.m_Timer) {
        dead = 0;
    }
    if (dead) {
        return;
    }
    if ((s16) pG->pl_life <= 0) {
        return;
    }
    switch (DmgMgr.hitCheck(&getPartsPtr(0)->world, 0)) {
    case 2:
    case 8:
        setDamage(0, 0, 123.0f, 0, 8);
        break;
    case 1:
    case 5:
        setDamage(0, 3, 123.0f, 0, 0x19);
        break;
    case 4:
        setDamage(0, 10, 123.0f, 0, 0x18);
        break;
    }
}

// Damage entry: life down, and once the accumulated count passes 0xFE a damage routine (0/kind,
// kind 7-8: routine 1/1, kind 9: routine 1/2) facing `ang` (123.0 = keep the direction).
void cPlayer::setDamage(u8 kind, int arg, f32 ang, int a, int b)
{
    beginDamage();
    LifeDownSet2(this, arg, 0, 1);
    m_ConDmTimer += b;
    if (m_ConDmTimer > 0xFE) {
        if (ang != 123.0f) {
            f32 d = Muku2(this->ang.y, ang, 3.1415927f);
            if (d < 1.5707964f && d > -1.5707964f) {
                m_Fwork0 = ang;
                switch (kind) {
                case 0:
                case 1:
                    kind = 1;
                    break;
                case 2:
                case 3:
                    kind = 3;
                    break;
                case 4:
                case 5:
                    kind = 5;
                    break;
                }
            } else {
                m_Fwork0 = LIMIT_ANGLE(ang + 3.1415927f);
                switch (kind) {
                case 0:
                case 1:
                    kind = 0;
                    break;
                case 2:
                case 3:
                    kind = 2;
                    break;
                case 4:
                case 5:
                    kind = 4;
                    break;
                }
            }
        } else {
            m_Fwork0 = 123.0f;
        }
        switch (kind) {
        default:
            PlRoutineSet(this, 1, 0, 0, kind);
            break;
        case 7:
        case 8:
            PlRoutineSet(this, 1, 1, 0, 0);
            break;
        case 9:
            PlRoutineSet(this, 1, 2, 0, 0);
            break;
        }
        m_ConDmTimer = 0;
    }
    m_ConDmFlag = 1;
}

// Fade the player (and the weapon) out while pG->flags_500C bit13 is set.
void cPlayer::visibleCtrl()
{
    if (StaFlagChk(pG, STA_PL_INVISIBLE)) {
        invisible_factor -= 0.1f;
        if (invisible_factor < 0.0f) {
            invisible_factor = 0.0f;
        }
    } else {
        invisible_factor = 1.0f;
    }
    if (Wep->m_pWep) {
        Wep->m_pWep->invisible_factor = invisible_factor;
    }
}

// 1 when the sub screen may open in the current routine.
int cPlayer::subScrCheck()
{
    if (r_no_0 != 0) {
        return 0;
    }
    if (r_no_1 == 6) {
        if (r_no_2 == 2 || r_no_2 == 4 || r_no_2 == 5) {
            return 0;
        }
    } else if (r_no_1 == 0xF) {
        if (r_no_2 == 2) {
            return 1;
        }
        return r_no_2 == 0xA;
    } else if (r_no_1 > 4 && r_no_1 != 0x11) {
        return 0;
    }
    return 1;
}

// be_flag 0x800 (keep moving while the game is suspended, e.g. during the sub screen) on the player
// and its weapon object.
void cPlayer::setNoSuspend(int on)
{
    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
    if (Wep->m_pWep) {
        Wep->m_pWep->setNoSuspend(on);
    }
    if (Wep->pObj2) {
        Wep->pObj2->setNoSuspend(on);
    }
}

// Idle footwork motion (m_MotTbl[0]/[1] or the damaged pair), no blend.
void cPlayer::setFootwork()
{
    int frame;
    int hokan;

    if (r_no_3 & 4) {
        partsFixMemory(0x13);
    }
    if (r_no_3 & 2) {
        frame = m_Frame;
        hokan = m_Hokan;
    } else {
        frame = 0;
        hokan = 5;
    }
    motionSet(m_MotTbl[0], m_MotTbl[1], PL_ARC_PTR(pG->pPlayer, 0x32), PL_ARC_PTR(pG->pPlayer, 0x33), hokan, frame);
    blendMot = 0;
}

// Motion sequence sound (seNo, set by the motion key): foot sounds by parts / kind, sand splash.
void cPlayer::seqSeCtrl()
{
    u32 no;
    int sub;
    int parts;
    u16 kind;

    StaFlagOff(pG, STA_PL_SE_FOOT);
    if (seNo == 0) {
        return;
    }
    no = seNo - 1;
    switch (no) {
    case 0:
    case 2:
    case 0xD:
        parts = 0x15;
        StaFlagOn(pG, STA_PL_SE_FOOT);
        kind = 5;
        break;
    case 1:
    case 3:
    case 0xE:
    case 0x14:
        parts = 0x19;
        StaFlagOn(pG, STA_PL_SE_FOOT);
        kind = 5;
        break;
    case 4:
    case 5:
        parts = 0;
        kind = 5;
        break;
    default:
        parts = 0;
        kind = 1;
        break;
    }
    sub = seFlags28B & 7;
    if (sub == 0) {
        switch (no) {
        case 0:
        case 1:
            SndCall(1, 0x2A, &pParts->world, id, 0, 0);
            break;
        case 2:
        case 3:
        case 0xD:
        case 0xE:
        case 0x14:
            SndCall(1, 0x2F, &pParts->world, id, 0, 0);
            break;
        }
    }
    switch (sub) {
    case 0:
        break;
    case 1:
        kind = 1;
        break;
    case 2:
        kind = 6;
        break;
    case 3:
        kind = 2;
        break;
    case 4:
        break;
    }
    SndCall(kind, no, &getPartsPtr(parts)->world, id, 0, 0);
    seNo = 0;
    if ((frame >= 1.0f && frame <= 4.0f) || (frame >= 8.0f && frame <= 16.0f)) {
        AddSandPower(&pPL->pos, -0.5f);
    }
}

// Aiming: the aim key held (Leon / Krauser), or the weapon / knife routine in a ready state.
int cPlayer::isKamae()
{
    if ((Key.on & 0x10) && (Key.on & 0x800)) {
        if (pG->pl_type == 0 || pG->pl_type == 4) {
            return 1;
        }
    }
    if (r_no_1 == 6) {
        if (r_no_2 == 0 && r_no_3 == 0) {
            return 0;
        }
        if (r_no_2 == 4) {
            if (PlReloadDirect == 1) {
                return 0;
            }
        }
        // `goto ng` (not `return 0`): the `||` then branches straight to the final block and no
        // label precedes this `return 1`, so jump2 cannot cross-jump it into the later copies
        // (a CODE_LABEL before the scanned tail lowers find_cross_jump's minimum); the later
        // copies merge into this one instead, which is the original's survivor.
        if (r_no_2 == 6 || r_no_2 == 3) {
            goto ng;
        }
        return 1;
    }
    if (r_no_1 == 0xB) {
        if (r_no_2 != 3) {
            return 1;
        }
        if (joyKamae() == 0) {
            return 0;
        }
        return 1;
    }
ng:
    return 0;
}

// Copies the control keys into Status_flg[0]: 0x40000000 / 0x20000000 action key trigger / held
// (Key 0x400), 0x4000 the partner-call key (0x80000).
void cPlayer::checkCtrl()
{
    if (Key.trg & 0x400) {
        StaFlagOn(pG, STA_PL_CHECK);
    }
    if (Key.on & 0x400) {
        StaFlagOn(pG, STA_PL_CHECK2);
    }
    if (Key.trg & 0x80000) {
        StaFlagOn(pG, STA_PL_ACTION);
    }
}

// Level change wall attributes: 1 up, 2 down, 3 up (kind 2), 0 none.
u32 upDownCk(cPlayer* pl)
{
    if (pl->m_ActAttr & 0x200000) {
        return 1;
    }
    if (pl->m_ActAttr & 0x2000) {
        return 2;
    }
    if (pl->m_ActAttr & 0x1000) {
        return 3;
    }
    return 0;
}

// Applies the controller layout (pSys->pad_type); both layouts use keyConfigTypeA.
void cPlayer::keyConfig()
{
    switch (pSys->pad_type) {
    case 0:
        keyConfigTypeA();
        break;
    case 1:
        keyConfigTypeA();
        break;
    }
}

// Key 0x100 (run / partner) is re-issued while x4FE counts down after a trigger.
void cPlayer::keyConfigTypeA()
{
    Key.trg &= ~0x100;
    if (m_BbtnCnt) {
        m_BbtnCnt--;
    }
    if (Key.trg & 0x40000000) {
        m_BbtnCnt = 2;
    }
    if ((Key.on & 2) && m_BbtnCnt != 0) {
        Key.trg |= 0x100;
    }
}

// 1 when an action button may be taken in the current routine.
int cPlayer::actCheck()
{
    if (stat & 4) {
        return 0;
    }
    if (r_no_0 != 0) {
        return 0;
    }
    switch (r_no_1) {
    case 5:
    case 7:
    case 8:
    case 9:
    case 0xC:
    case 0xE:
    case 0x10:
        return 0;
    }
    return 1;
}

// Damage start: ends a running event (stat bit1) and interrupts the weapon / neck.
void cPlayer::beginDamage()
{
    if (stat & 2) {
        END_EVENT(this, 0);
    }
    interrupt();
}

// Damage end: default face, and the Krauser (type 4) bow's arrow display back on.
void cPlayer::endDamage()
{
    cPlayer* pl = pPL;

    pl->setFace(0);
    if (pl->type == 4 && pG->weapon_no == 0x1C) {
        Wep->m_pWep->setDisp(2, 1);
    }
}

// 0 fine (above 2/3), 1 caution (above 1/3), 2 danger.
int cPlayer::getLifeLevel()
{
    int max;
    s16 low;
    int ret;

    switch (pG->pl_type) {
    case 0:
        max = 1200;
        break;
    case 1:
        max = 600;
        break;
    case 2:
        max = 1200;
        break;
    case 3:
        max = 1200;
        break;
    case 4:
        max = 1200;
        break;
    case 5:
        max = 1200;
        break;
    default:
        pLog->err(0, 0, "cPlayer::getLifeLevel() UNKNOWN PL TYPE %d", pG->pl_type);
        return 2;
    }
    if ((s16) pG->pl_life > max * 2 / 3) {
        return 0;
    }
    low = max / 3;
    ret = 2;
    if ((s16) pG->pl_life > low) {
        ret = 1;
    }
    return ret;
}

// Event start (mode in r4, see player.h): interrupt, routine 5 (0: idle footwork, 1: sub 2).
void cPlayer::beginEvent()
{
    register int modeReg asm("r4");
    int mode = modeReg;

    interrupt();
    Neck->motL = 0;
    switch (mode) {
    case 0:
        PlRoutineSet(this, 5, 0, 0, 0);
        MotionBlendOff(this);
        atari.throughOn();
        be_flag |= 0x04000000;
        if (Wep->m_pWep) {
            Wep->m_pWep->resetMotion();
        }
        setFootwork();
        m_Flag &= ~0x100;
        break;
    case 1:
        PlRoutineSet(this, 5, 2, 0, 0);
        break;
    }
    stat |= 2;
}

// Stop everything the routines left running: cameras, neck, motion speed, weapon, face, sounds.
void cPlayer::interrupt()
{
    cModelInfo* face;

    endCamera();
    stat |= 0x800;
    m_BbtnCnt = 0;
    Neck->m_Mode = 1;
    MOTION(this)->Seq_speed = 1.0f;
    stat &= ~0x40;
    ang.y += pParts->ang.y;
    pParts->ang.y = 0.0f;
    if (Wep->m_pWep) {
        if (Wep->pObj2) {
            Wep->pObj2->setDisp(1, 1);
        }
        Wep->m_pWep->interrupt();
        switch (pG->weapon_no) {
        case 0xD:
        case 0x13:
        case 0x16:
        case 0x17:
            Wep->m_pWep->setMotion(this);
            break;
        case 0x1C:
            setRightHand(0);
            break;
        }
    }
    face = Body->pFace;
    if (VALID_PTR(face)) {
        face->x84 = 0.0f;
        face->x70 = 0.0f;
        face->x5C = 0.0f;
    }
    if (pG->pl_type == 4 && (StaFlagChk(pG, STA_KLAUSER_TRANSFORM))) {
        StaFlagOff(pG, STA_KLAUSER_TRANSFORM);
        x890 = 0;
        if (x894 == -1) {
            x894 = 1;
        }
    }
    if (m_SeId) {
        SndStop(m_SeId, 1);
        m_SeId = 0;
    }
}

// End the special cameras the routines started (scope, binocular, push object); 1 when one was on.
int cPlayer::endCamera()
{
    int ret = 0;

    if (stat & 0x200) {
        BitOff(stat, 0x200);
        StaFlagOff(pG, STA_THERMO_GRAPH);
        if (StaFlagChk(pG, STA_SUB_SCRN)) {
            LightMgr.update(CamCtrl.areaNo, 0);
        }
    }
    if (stat & 0x10) {
        CamCtrl.endScope();
        if (StaFlagChk(pG, STA_SUB_SCRN)) {
            CameraMove();
        }
        stat &= ~0x10;
        be_flag |= 2;
        ret = 1;
    }
    if (stat & 4) {
        CamCtrl.LowerBinocular();
        if (StaFlagChk(pG, STA_SUB_SCRN)) {
            CameraMove();
        }
        BitOff(stat, 4);
        SpfFlagOff(pG, SPF_KEY);
        ret = 1;
    }
    if (stat & 8) {
        CamCtrl.endPushObject();
        if (StaFlagChk(pG, STA_SUB_SCRN)) {
            CameraMove();
        }
        stat &= ~8;
        ret = 1;
    }
    return ret;
}

// Event end (mode in r4, see player.h).
void cPlayer::endEvent()
{
    register u32 modeReg asm("r4");

    endEvent0(modeReg);
}

// Event end (stat bit1 set): the player is drawn / collides / moves again, invulnerable for 10
// frames, neck on; when alive mode 0 = back to routine 0/0 with a pending footwork (r_no_3 1),
// 1 = m_Flag 0x100 (return when the event motion ends), 2 = routine 0/0 at once.
void cPlayer::endEvent0(u32 mode)
{
    int one = 1;

    if (!(stat & 2)) {
        return;
    }
    be_flag |= 2;
    atari.throughOff();
    be_flag |= 0x200000;
    be_flag &= ~0x04000000;
    setNoSuspend(0);
    dmg.set(0, 10);
    Neck->m_Mode = one;
    if ((s16) pG->pl_life > 0) {
        switch (mode) {
        case 0:
            m_Hokan = 0;
            m_Frame = 0;
            PlRoutineSet(this, 0, 0, 0, one);
            break;
        case 1:
            m_Flag |= 0x100;
            break;
        case 2:
            PlRoutineSet(this, 0, 0, 0, 0);
            break;
        default:
            pLog->err(0, 0, "PL::endEvent() UNKNOWN MODE.");
            break;
        }
    }
    stat &= ~2;
}

// 1 while the player is in routine 0 (normal control) and an event may take him.
int cPlayer::checkEvent()
{
    return r_no_0 == 0;
}

// Action (routine 5) start: like an event without the neck / weapon interrupt.
void cPlayer::beginAction()
{
    endCamera();
    PlRoutineSet(this, 5, 0, 0, 0);
    MotionBlendOff(this);
    if (Wep->m_pWep) {
        Wep->m_pWep->resetMotion();
    }
    setFootwork();
    stat |= 2;
}

// Action end: back to routine 0 with sub routine `routine` pending (m_Hokan).
// `one` at function scope with a single use in another block: update_equiv_regs moves its `li`
// next to the `stb`, so the short-lived constant outranks the flags chain for r0.
void cPlayer::endAction(int routine)
{
    int one = 1;

    if (stat & 2) {
        be_flag |= 2;
        setNoSuspend(0);
        m_Hokan = routine;
        stat &= ~2;
        m_Frame = 0;
        PlRoutineSet(this, 0, 0, 0, one);
    }
}

// Motion speed of the player and the weapon (Leon only).
void cPlayer::setSlow(f32 rate)
{
    if (pG->pl_type != 0) {
        return;
    }
    MOTION(this)->Seq_speed = rate;
    if (Wep->m_pWep) {
        MOTION(Wep->m_pWep)->Seq_speed = rate;
    }
}

// Eye / eyelid control each frame (not for HUNK, not when dead): m_EyeMode 0 wander / blink, 1 from
// the motion's face data.
void cPlayer::moveEye()
{
    if (pG->pl_type == 3) {
        return;
    }
    if ((s16) pG->pl_life <= 0) {
        return;
    }
    switch (m_EyeMode) {
    case 0:
        moveEyeNormal();
        break;
    case 1:
        moveEyeMotion();
        break;
    }
}

// Eye direction state of moveEyeNormal: a class with a constructor, so the static local gets the
// `_.tmp_0` guard and three float stores the original has.
struct PlEyeDir {
    f32 x;   // current
    f32 y;   // target
    f32 z;   // mix
    PlEyeDir() { x = y = z = 0.0f; }
};

// Eyelid (parts 0x1C) blink sequence on `timer` and the eye direction (parts 0x20/0x21) wander:
// eyeDir = { current, target, mix }.
void cPlayer::moveEyeNormal()
{
    static PlEyeDir eyeDir;
    static int timer;
    cModel* p;

    p = getPartsPtr(0x1C);
    // Every case written out separately in ascending order: jump2 cross-jumps the identical
    // bodies into the last copy, which gives the target's body layout (3, 4, 0x1E, 0x58, 0x5A,
    // 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62) and constant-pool order.
    switch (timer++) {
    default:
        p->ang.x = 0.0f;
        break;
    case 0: {
        f32 y = ((f32) (Rnd() % 200) * 0.01f - 1.0f) * 3.1415927f * 0.1f;
        eyeDir.y = y;
        if (eyeDir.z == 0.0f) {
            eyeDir.x = y;
        }
        p->ang.x = 0.0872664600610733f;
        break;
    }
    case 1:
        p->ang.x = 0.1745329201221466f;
        break;
    case 2:
        p->ang.x = 0.3490658402442932f;
        break;
    case 3:
        p->ang.x = 0.3141592741012573f;
        break;
    case 4:
        p->ang.x = 0.24434609711170197f;
        break;
    case 5:
        p->ang.x = 0.1745329201221466f;
        break;
    case 6:
        p->ang.x = 0.0872664600610733f;
        break;
    case 0x1E:
        eyeDir.y = 0.0f;
        if (eyeDir.z == 0.0f) {
            eyeDir.x = 0.0f;
        }
        break;
    case 0x58:
        if (Rnd() & 3) {
            timer = 0;
        } else {
            timer = 0x5A;
        }
        break;
    case 0x5A:
        p->ang.x = 0.0872664600610733f;
        break;
    case 0x5B:
        p->ang.x = 0.1745329201221466f;
        break;
    case 0x5C:
        p->ang.x = 0.3490658402442932f;
        break;
    case 0x5D:
        p->ang.x = 0.296705961227417f;
        break;
    case 0x5E:
        p->ang.x = 0.33161255717277527f;
        break;
    case 0x5F:
        p->ang.x = 0.3490658402442932f;
        break;
    case 0x60:
        p->ang.x = 0.2617993950843811f;
        break;
    case 0x61:
        p->ang.x = 0.1745329201221466f;
        break;
    case 0x62:
        p->ang.x = 0.0872664600610733f;
        timer = 10;
        break;
    }
    p->matUpdate();
    {
        static int eyetime = 0;

        if (--eyetime < 0) {
            f32 d = ((f32) (Rnd() % 200) * 0.01f - 1.0f) * 0.03141592815518379f;
            eyeDir.y += d;
            if (eyeDir.z == 0.0f) {
                eyeDir.x = eyeDir.y;
            }
            eyetime = Rnd() % 3 + 2;
        }
    }
    {
        PlEyeDir* e = &eyeDir;
        f32 mn = -0.3141592741012573f;
        f32 mx = 0.3141592741012573f;
        if (e->y < mn) {
            e->y = mn;
        } else if (e->y > mx) {
            e->y = mx;
        }
        if (e->z == 0.0f) {
            e->x = e->y;
        }
    }
    p = getPartsPtr(0x20);
    p->ang.y = eyeDir.x;
    p->matUpdate();
    p = getPartsPtr(0x21);
    p->ang.y = eyeDir.x;
    p->matUpdate();
    eyeDir.x = eyeDir.x * eyeDir.z + eyeDir.y * (1.0f - eyeDir.z);
}

int lbl_80314CDC = 0;   // unreferenced 4-byte .sdata word between moveEyeNormal's statics and the neck

// Eyes driven by the motion: parts 0x21 follows parts 0x20.
void cPlayer::moveEyeMotion()
{
    cModel* a;
    cModel* b;

    a = getPartsPtr(0x20);
    b = getPartsPtr(0x21);
    b->ang = a->ang;
    b->matUpdate();
}

// Updates the body / weapon matrices and lets the weapon object draw its laser (wep.disp bit1).
void cPlayer::setLaserSight(int draw, int noCalc)
{
    Body->move();
    partsWorldCalc();
    Wep->m_pWep->partsWorldCalc();
    Wep->m_pWep->wep.disp |= 2;
    Wep->m_pWep->drawLaserSight(draw, noCalc);
}

// Binocular sequence: 1 raise sound -> 2 wait for the key -> 3 end camera -> 0.
void cPlayer::moveBinocular()
{
    switch (m_BinoRno) {
    case 0:
        break;
    case 1:
        SndCall(1, 2, &getPartsPtr(3)->world, 0, 0, 0);
        m_BinoRno = 2;
        break;
    case 2:
        if ((Key.trg & 0x800) || (Key.trg & 0x40000000)) {
            m_BinoRno = 3;
        }
        break;
    case 3:
        endCamera();
        m_BinoRno = 0;
        break;
    }
}

// Shadow colour fade: darker while the camera is above the player on a flat floor.
void cPlayer::shadowCtrl()
{
    int on;

    if (!(stat & 0x800) || pG->Cam.param.pos.y < pos.y || !pFloor_norm || pFloor_norm->y < 0.8f) {
        on = 0;
    } else {
        on = 1;
    }
    if (on) {
        if (Shd_color > 0xF) {
            Shd_color -= 0x10;
        } else {
            Shd_color = 0;
        }
    } else {
        if (Shd_color <= 0xEF) {
            Shd_color += 0x10;
        } else {
            Shd_color = 0xFF;
        }
    }
}

// Reload key by PlKeyReloadType: 0 aim + R trigger, 1 the aim key alone (PlReloadDirect = no aim
// key), 2 aim + a C stick direction. 1 when reloading was asked.
int cPlayer::keyReload()
{
    int ret;

    switch (PlKeyReloadType) {
    default:
        ret = 0;
        if ((Joy[0].on & 0x20) && (Joy[0].trg & 0x200)) {
            ret = 1;
        }
        PlReloadDirect = 0;
        return ret;
    case 1:
        ret = (Joy[0].on >> 6) & 1;
        if (ret) {
            PlReloadDirect = ((Joy[0].on ^ 0x20) >> 5) & 1;
        }
        return ret;
    case 2:
        ret = 0;
        if ((Joy[0].on & 0x20) && (Joy[0].on & 0x00F00000)) {
            ret = 1;
        }
        PlReloadDirect = 0;
        return ret;
    }
}

// Neck control for player `p`: no motions yet, mode 1 (looking on).
cPlNeck::cPlNeck(cPlayer* p)
{
    pl = p;
    ang = 0.0f;
    m_lockCtr = 0;
    motL = 0;
    motR = 0;
    m_Flag = 0;
    m_Mode = 1;
}

// Neck motions: the left turn (frame `frame`) is set at once; the look timer restarts.
void cPlNeck::init(void* l, void* r, int frame)
{
    motL = l;
    motR = r;
    if (VALID_PTR2(l) && VALID_PTR2(r)) {
        motSet(l, frame);
        move();
        m_Flag = 0;
        if (m_lockCtr > 0x10000000) {
            m_lockCtr = 0x7FFFFFFF;
        } else {
            m_lockCtr = 0;
            ang = 0.0f;
        }
    }
}

// Turn the head towards the target enemy (6 degrees a frame, +-45), switching the left / right
// motion at the centre; the blend rate is the angle over 45 degrees.
void cPlNeck::move()
{
    cModel* head;
    cEm* em;

    if (m_Mode == 0) {
        return;
    }
    if (m_Mode == 2) {
        m_Mode = 1;
        return;
    }
    if (!VALID_PTR2(motL)) {
        return;
    }
    if (!VALID_PTR2(motR)) {
        return;
    }
    if (pPL->r_no_0 > 1) {
        return;
    }
    head = pPL->getPartsPtr(0);
    em = getTarget();
    if (em) {
        if (em != target) {
            target = em;
            m_lockCtr = em->checkStatus(EM_STATUS_LOOK_ME) ? 0x7FFFFFFF : 20;
        }
    } else {
        if (target && target->hp <= 0) {
            target = 0;
            m_lockCtr = 0;
        }
    }
    if (pPL->r_no_0 != 0 || pPL->r_no_1 > 3) {
        m_lockCtr = 0;
    }
    if (m_lockCtr) {
        f32 a = GetXZAngleLocal(&head->world, &target->pos, pPL->ang.y);
        if (a > ang) {
            if (a - ang > 0.10471975803375244f) {
                ang = ang + 0.10471975803375244f;
            } else {
                ang = a;
            }
        } else if (a < ang) {
            if (a - ang < -0.10471975803375244f) {
                ang = ang - 0.10471975803375244f;
            } else {
                ang = a;
            }
        }
        if (ang > 0.7853981852531433f) {
            ang = 0.7853981852531433f;
        }
        if (ang < -0.7853981852531433f) {
            ang = -0.7853981852531433f;
        }
        if (ang < 0.0f && m_Flag) {
            And16(m_Flag, 0xFFFE);
            motSet(motL, (u16) pPL->frame);
        } else if (ang > 0.0f && !m_Flag) {
            BitOn16(m_Flag, 1);
            motSet(motR, (u16) pPL->frame);
        }
        m_lockCtr--;
    } else {
        if (ang != 0.0f) {
            if (ang > 0.10471975803375244f) {
                ang = ang - 0.10471975803375244f;
            } else if (ang < -0.10471975803375244f) {
                ang = ang + 0.10471975803375244f;
            } else {
                ang = 0.0f;
            }
        }
    }
    if (pPL->blendMot) {
        f32 rate = ang / 0.7853981852531433f;
        if (!(m_Flag & 1)) {
            rate = -rate;
        }
        pPL->blendMot->blendRate = rate;
    }
}

// Neck motion `data` into the player's neck work (frame `frame`, no IK), blended at rate 1.
void cPlNeck::motSet(void* data, int frame)
{
    cPlayer* p = pPL;

    if (!VALID_PTR2(data)) {
        pLog->err(0, 0, "cPlNeck::motSet() ILEGAL PTR WAS SET %08X", data);
        return;
    }
    p->m_SubMot.flags2 |= 0x10000000;
    MotionSetCore(p, &p->m_SubMot, data, 0, 8, 5, frame);
    p->m_SubMot.flags2 &= ~0x10000000;
    p->blendMot = &p->m_SubMot;
    p->blendMot->blendRate = 1.0f;
    p->blendMot->flags2 |= 0x80000000;
}

// Nearest alive enemy (not in battle) within 5000 of parts 3, seen from there; enemies with status
// bit9 count as 100000 closer.
cEm* cPlNeck::getTarget()
{
    cEm* em;
    cEm* best = 0;
    f32 bestDist = 25000000.0f;
    Vec* from;

    from = &pPL->getPartsPtr(3)->world;
    for (em = EmMgr.pAlive; em; em = (cEm*) em->pNext) {
        if (em->checkStatus(EM_STATUS_LOCKOFF)) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        if (em == pPL) {
            continue;
        }
        if (em == pSubEm) {
            continue;
        }
        Vec* to = &em->getPartsPtr(em->lockParts)->world;
        f32 d = GetDistance(from, to);
        if (em->checkStatus(EM_STATUS_LOOK_ME)) {
            d -= 100000.0f;
        }
        if (d < bestDist) {
            if (SatMgr.hitCheck(from, to, 0, 0, 0, 0) == 0) {
                bestDist = d;
                best = em;
            }
        }
    }
    return best;
}

// 0 off, 1 on, 2 = turn on next frame.
void cPlNeck::setMode(int m)
{
    m_Mode = m;
}

// Waist straight.
cPlWaist::cPlWaist()
{
    m_Ang.y = 0.0f;
}

// Moves the waist angle toward `target` by `rate` (0..1 per frame); returns the change applied.
f32 cPlWaist::set(f32 target, f32 rate)
{
    f32 old = m_Ang.y;

    m_Ang.y = m_Ang.y * (1.0f - rate) + target * rate;
    return m_Ang.y - old;
}

// No model attached, rate 0.
cMot3::cMot3()
{
    m_Mode = 0;
    m_Rate = 0.0f;
}

// Attaches the blend to model `m`: m0 becomes its own motion (MotionSetCore frame a, hokan b,
// flags d / e), m1 the first blended motion (rate < 0), m2 the second (rate > 0); c = m_Mode.
void cMot3::set(cModel* m, void* m0, void* m1, void* m2, int a, u8 b, int c, u16 d, u16 e)
{
    // COMPILER-DIFF: 2 (narrow-argument extension at entry). The original zero-extends the u8
    // parameter into a callee-saved register (`clrlwi r28, r9, 24`) before both int uses; ours
    // treats it as promoted: combine knows the promoted r9 has only 8 nonzero bits and strips
    // any mask written on `b`. Reading the incoming register through a pin gives the
    // zero_extendqisi2 no LOG_LINK to fold through (and no CC clobber: sched1 weight 0, so it
    // is ranked like the original's insn).
    register int rb asm("r9");
    int mode = (u8) rb;

    m_pEm = m;
    m_Rate = 0.0f;
    mot0 = m0;
    mot1 = m1;
    mot2 = m2;
    m_Mode = c;
    MotionSetCore(m, MOTION(m), m0, a, mode, d, e);
    set0(m1, e, mode);
    ((cEm*) m)->blendMot->blendRate = 0.0f;
}

// Blend motion `m` (frame a, hokan b) into the model's motion.
void cMot3::set0(void* m, u8 a, int b)
{
    MotionSetCore(m_pEm, &work, m, 0, b, 4, a);
    switch (m_Mode) {
    case 0:
        break;
    case 1:
        work.flags2 |= 0x80000000;
        break;
    }
    ((cEm*) m_pEm)->blendMot = &work;
}

// Blend rate -1..1: crossing 0 switches the blended motion (mot1 below, mot2 above) at the current
// frame; the work's blend rate is |rate|.
void cMot3::move(f32 r)
{
    if (m_pEm == 0) {
        return;
    }
    if (((cEm*) m_pEm)->blendMot == 0) {
        return;
    }
    if (r > 1.0f) {
        r = 1.0f;
    }
    if (r < -1.0f) {
        r = -1.0f;
    }
    if (m_Rate < 0.0f && r >= 0.0f) {
        set0(mot1, (u8) ((cEm*) m_pEm)->frame, 2);
    } else if (m_Rate >= 0.0f && r < 0.0f) {
        set0(mot2, (u8) ((cEm*) m_pEm)->frame, 2);
    }
    m_Rate = r;
    if (r < 0.0f) {
        r = -r;
    }
    ((cEm*) m_pEm)->blendMot->blendRate = r;
}

const f32 cPlayer::SPEED_WALK_TURN = 0.0418879f;
const f32 cPlayer::SPEED_RUN_TURN = 0.0418879f;
const f32 cPlWaist::ROT_LIMIT = 0.39269909262657166f;
