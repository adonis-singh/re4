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

// cPlNeck's checks compile to the folded `addis 0x8000; cmplwi 0x02FFFFFF` range form.
#define VALID_PTR2(p) ((u32) (p) - 0x80000000 <= 0x02FFFFFF)




// Partner (id 3) dead while the player is in routine 0: routine 6 (die), damage info 0x80. An
// inline member of the class whose vtable this unit owns: emitted here after the destructor.
inline void cPlayer::subCharLiveCheck()
{
    cEm* sub = pSUB;
    if (sub && sub->id == 3 && sub->hp <= 0 && r_no_0 == 0) {
        EmRoutineSet(this, 6, 0, 0, 0);
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
int actWallCheck(cPlayer* pEm)
{
    pEm->m_ActAttr = 0;
    Vec p0 = { 0.0f, 400.0f, 0.0f };

    PSVECAdd(&p0, &pEm->pos, &p0);
    Vec p1 = { 0.0f, 400.0f, 1000.0f };
    Vec hit;
    Vec nrm;
    Vec ofs;
    Vec rot;
    Vec p2;
    Vec p3;
    int ret;

    PSMTXMultVec(pEm->mat, &p1, &p1);
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
    pEm->m_Fwork0 = (f32) atan2(-nrm.x, -nrm.z);
    rot.x = 0.0f;
    rot.y = pEm->m_Fwork0;
    rot.z = 0.0f;
    p0.x = 350.0f;
    p0.y = 300.0f;
    p0.z = 0.0f;
    RotVector(&p0, &rot, &p0);
    PSVECAdd(&p0, &pEm->pos, &p0);
    PSVECAdd(&p0, &ofs, &p1);
    ret &= SatMgr.hitCheck(&p0, &p1, 0, 0, 0, 0);
    if (ret == 0) {
        return 0;
    }
    p2.x = -350.0f;
    p2.y = 300.0f;
    p2.z = 0.0f;
    RotVector(&p2, &rot, &p2);
    PSVECAdd(&p2, &pEm->pos, &p2);
    PSVECAdd(&p2, &ofs, &p3);
    ret &= SatMgr.hitCheck(&p2, &p3, 0, 0, 0, 0);
    if (ret == 0) {
        return 0;
    }
    pEm->m_ActAttr = ret;
    pEm->m_ActNorm = nrm;
    pEm->m_ActCross = hit;
    return 1;
}

// Fence (wall attribute bit5) the player may climb: nothing between the shoulders behind the fence
// and a floor within 300 of the player's height there. Sets x3E0 (climb flag).
int fanceCheck(cPlayer* pEm)
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
    if ((pEm->m_ActAttr & 0x20) == 0) {
        return 0;
    }
    rot.x = 0.0f;
    rot.y = pEm->m_Fwork0;
    rot.z = 0.0f;
    PSVECScale(&pEm->m_ActNorm, &dir, -1000.0f);
    p0.x = 400.0f;
    p0.y = 300.0f;
    p0.z = 0.0f;
    RotVector(&p0, &rot, &p0);
    PSVECAdd(&p0, &pEm->pos, &p0);
    PSVECAdd(&p0, &dir, &p0);
    p1.x = -400.0f;
    p1.y = 300.0f;
    p1.z = 0.0f;
    RotVector(&p1, &rot, &p1);
    PSVECAdd(&p1, &pEm->pos, &p1);
    PSVECAdd(&p1, &dir, &p1);
    hit0 = SatMgr.hitCheck(&p0, &p1, 0, 0, 0, 0);
    hit1 = SatMgr.hitCheck(&p1, &p0, 0, 0, 0, 0);
    if ((hit0 | hit1) != 0) {
        return 0;
    }
    PSVECScale(&pEm->m_ActNorm, &p0, -1500.0f);
    PSVECAdd(&p0, &pEm->m_ActCross, &p0);
    p1.x = p0.x;
    p1.y = p0.y - 10000.0f;
    p1.z = p0.z;
    SatMgr.hitCheck(&p0, &p1, &hit, 0, 0, 0);
    if (fabsf(hit.y - pEm->pos.y) > 300.0f) {
        return 0;
    }
    pEm->m_Work0 = 1;
    return 1;
}

// Window in front of the player: 1 with its break direction and the window when the player may go
// through it (and no rack is in the way); PlFancePos = 400 behind the window.
int windowCheck(cPlayer* pEm, u8* pDir, cEmWindow** pEmWindow_out)
{
    Vec p0 = { 0.0f, 400.0f, 0.0f };

    PSVECAdd(&p0, &pEm->pos, &p0);
    Vec p1 = { 0.0f, 400.0f, 1000.0f };
    Vec wdir;
    Vec wpos;
    u16 status;
    cEmWindow* win;
    Vec* d = &wdir;

    RotVector(&p1, &pEm->ang, &p1);
    PSVECAdd(&p1, &pEm->pos, &p1);
    if (ChkWindow(pEm, &p0, &p1, 1, &status, d, &wpos, &win) == 1) {
        pEm->m_Fwork0 = (f32) atan2(-d->x, -d->z);
        pEm->m_Work0 = 0;
        PSVECScale(d, &PlFancePos, 400.0f);
        PSVECAdd(&PlFancePos, &wpos, &PlFancePos);
        if (EmRackCk(pEm, &pEm->pos, atan2f(-wdir.x, -wdir.z)) == 0) {
            return 0;
        }
        *pDir = win->ChkBreakDir(&pEm->pos);
        *pEmWindow_out = win;
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
        EmRoutineSet(pl, 0, 0xC, 0, 0);
    } else {
        EmRoutineSet(pl, 0, 0xC, 0, 5);
        PlFanceFlag = 1;
    }
    pl->dmg.set(0, 10);
}

// Action button: go through the window (its event as a scenario task).
void windowOn(cEmWindow* pEmWindow)
{
    SceExec(0x12, (TaskFunc) cEmWindow::ExeWindowEvent, (int) pEmWindow, 2, SCE_PRIO_DEF_2, 0);
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

    EmRoutineSet(pl, 0, 0xE, 0, 0);
    pPL->dmg.set(0, 0x80);
}

// Action button callback: climb up the step (routine 0/7 pl_R1_LevelUp, m_Work0 kind 0).
void levelUpOn()
{
    cPlayer* pl = pPL;

    EmRoutineSet(pl, 0, 7, 0, 0);
    pPL->m_Work0 = 0;
    pPL->dmg.set(0, 10);
}

// Action button callback: climb down the step (routine 0/8 pl_R1_LevelDown, kind 0).
void levelDownOn()
{
    cPlayer* pl = pPL;

    EmRoutineSet(pl, 0, 8, 0, 0);
    pPL->m_Work0 = 0;
    pPL->dmg.set(0, 10);
}

// Action button callback: climb up the high step (kind 1).
void level2UpOn()
{
    cPlayer* pl = pPL;

    EmRoutineSet(pl, 0, 7, 0, 0);
    pPL->m_Work0 = 1;
    pPL->dmg.set(0, 10);
}

// Action button callback: climb down the high step (kind 1).
void level2DownOn()
{
    cPlayer* pl = pPL;

    EmRoutineSet(pl, 0, 8, 0, 0);
    pPL->m_Work0 = 1;
    pPL->dmg.set(0, 10);
}

// Action button callback: grab the pushable object (routine 0/9 pl_R1_ObjPush).
void holdOn()
{
    cPlayer* pl = pPL;

    EmRoutineSet(pl, 0, 9, 0, 0);
}

// Wall (attribute bit19) 600 ahead at both shoulders the player may jump over: jumpDir = -normal,
// jumpHeight = the floor 3800 behind it relative to the player (room 226 zeroes it past 1500).
int jumpCheck(cPlayer* pEm)
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
    PSMTXMultVec(pEm->mat, &p0, &p0);
    p1.x = 300.0f;
    p1.y = 400.0f;
    p1.z = 600.0f;
    PSMTXMultVec(pEm->mat, &p1, &p1);
    attr = SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    p0.x = -300.0f;
    p0.y = 400.0f;
    p0.z = 0.0f;
    PSMTXMultVec(pEm->mat, &p0, &p0);
    p1.x = -300.0f;
    p1.y = 400.0f;
    p1.z = 600.0f;
    PSMTXMultVec(pEm->mat, &p1, &p1);
    attr &= SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    if (attr & 0x80000) {
        pEm->m_JumpVec.x = -nrm.x;
        pEm->m_JumpVec.y = nrm.y;
        pEm->m_JumpVec.z = -nrm.z;
        PSVECScale(&pEm->m_JumpVec, &p2, dist);
        PSVECAdd(&p2, &hit, &p2);
        p2.y += up;
        h = SatMgr.getFloor(&p2, 0, 600.0f, 100000.0f, 0) - pEm->pos.y;
        pEm->m_JumpAdjY = h;
        if (pG->stage_no == 2 && pG->room_no == 0x26) {
            if (fabsf(h) > up) {
                pEm->m_JumpAdjY = 0.0f;
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

    EmRoutineSet(pl, 0, 0x13, 0, 0);
    pPL->ang.y = pPL->ang.y + Muku3(pPL->ang.y, &pl->m_JumpVec, 3.1415927f);
    pPL->dmg.set(0, 0x80);
}

// Motion set with the damaged (m0/m1) or normal (m2/m3) pair by dmMotCk().
void cPlayer::motionSet(void* m0, void* seq0, void* m1, void* seq1, int hokan, int frame)
{
    if (dmMotCk()) {
        MotionSetCore(this, MOTION(this), m0, seq0, hokan, 5, frame);
    } else {
        MotionSetCore(this, MOTION(this), m1, seq1, hokan, 5, frame);
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
                EmRoutineSet(this, zero, 6, zero, zero);
                return 1;
            }
    if (joyLKamae()) {
        EmRoutineSet(this, 0, 0xB, 0, 0);
        return 1;
    }
    if (r_no_1 != 5 && (Key.trg & 0x100)) {
        EmRoutineSet(this, 0, 5, 0, 0);
        return 1;
    }
    if (r_no_1 == 0 && ((Key.on & 4) || (Key.on & 8))) {
        EmRoutineSet(this, 0, 4, 0, 0);
        return 1;
    }
    if (r_no_1 != 1 && r_no_1 != 3 && (Key.on & 1)) {
        EmRoutineSet(this, 0, 1, 0, 0);
        return 1;
    }
    if (r_no_1 != 2 && (Key.on & 2)) {
        EmRoutineSet(this, 0, 2, 0, 0);
        return 1;
    }
    if (keyReload() && Wep->m_pWep && Wep->m_pWep->reloadable()) {
        EmRoutineSet(this, 0, 6, 4, 0);
        m_Work0 = 1;
        return 1;
    }
    actWallCheck(this);
    if (Push->catchCheck()) {
        ActBtn.set(ACT_PUSH, 2, (void*) holdOn, 0, ACTCTR_NO_TRG, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
    if (fallCheck_80172E38(this)) {
        ActBtn.set(ACT_JUMP_DOWN, 2, (void*) fallOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
    if (fanceCheck(this)) {
        ActBtn.set(ACT_JUMP_OVER, 2, (void*) fanceOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
    if (jumpCheck(this)) {
        ActBtn.set(ACT_JUMP_OVER, 2, (void*) jumpFallOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
    if (windowCheck(this, &dir, &win)) {
        int broken = win->ChkStatus() & 1;
        if (broken) {
            if (dir == 2) {
                ActBtn.set(ACT_JUMP_OUT, 2, (void*) windowOn, win, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
            } else {
                ActBtn.set(ACT_JUMP_OVER, 2, (void*) fanceOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
            }
        } else {
            if (dir == 0) {
                ActBtn.set(ACT_JUMP_OUT, 2, (void*) windowOn, win, broken, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
            }
            if (dir == 1) {
                ActBtn.set(ACT_JUMP_IN, 2, (void*) windowOn, win, broken, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
            }
            if (dir == 2) {
                ActBtn.set(ACT_JUMP_OUT, 2, (void*) windowOn, win, broken, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
            }
        }
    }
    switch (upDownCk(this)) {
    case 1:
        ActBtn.set(ACT_GO_UP, 2, (void*) levelUpOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        break;
    case 2:
        ActBtn.set(ACT_GET_DOWN, 2, (void*) levelDownOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        break;
    case 3:
        ActBtn.set(ACT_GO_UP, 2, (void*) level2UpOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        break;
    case 4:
        ActBtn.set(ACT_GET_DOWN_1M, 2, (void*) level2DownOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
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
    case DMG_TYPE_GRENADE_BLAST:
    case DMG_TYPE_GRENADE:
        setDamage(0, 0, 123.0f, 0, 8);
        break;
    case DMG_TYPE_FIRE:
    case DMG_TYPE_LAMP:
        setDamage(0, 3, 123.0f, 0, 0x19);
        break;
    case DMG_TYPE_FLAME:
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
            EmRoutineSet(this, 1, 0, 0, kind);
            break;
        case 7:
        case 8:
            EmRoutineSet(this, 1, 1, 0, 0);
            break;
        case 9:
            EmRoutineSet(this, 1, 2, 0, 0);
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
void cPlayer::setNoSuspend(int onoff)
{
    if (onoff) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
    if (Wep->m_pWep) {
        Wep->m_pWep->setNoSuspend(onoff);
    }
    if (Wep->m_pWepHand) {
        Wep->m_pWepHand->setNoSuspend(onoff);
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
    Motion.blend = 0;
}

// Motion sequence sound (seNo, set by the motion key): foot sounds by parts / kind, sand splash.
void cPlayer::seqSeCtrl()
{
    u32 no;
    int sub;
    int parts;
    u16 kind;

    StaFlagOff(pG, STA_PL_SE_FOOT);
    if (Motion.Seq_old.Se == 0) {
        return;
    }
    no = Motion.Seq_old.Se - 1;
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
    sub = Motion.Seq_old.Free & 7;
    if (sub == 0) {
        switch (no) {
        case 0:
        case 1:
            SndCall(1, 0x2A, &pList->world, id, 0, 0);
            break;
        case 2:
        case 3:
        case 0xD:
        case 0xE:
        case 0x14:
            SndCall(1, 0x2F, &pList->world, id, 0, 0);
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
    Motion.Seq_old.Se = 0;
    if ((Motion.Seq_frame >= 1.0f && Motion.Seq_frame <= 4.0f) || (Motion.Seq_frame >= 8.0f && Motion.Seq_frame <= 16.0f)) {
        AddSandPower(pPL->pos, -0.5f);
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
u32 upDownCk(cPlayer* pEm)
{
    if (pEm->m_ActAttr & 0x200000) {
        return 1;
    }
    if (pEm->m_ActAttr & 0x2000) {
        return 2;
    }
    if (pEm->m_ActAttr & 0x1000) {
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
    if (stat.check(F_BINOCULAR)) {
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
    if (stat.check(F_EVENT)) {
        this->endEvent(0);
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

// Event start: interrupt, routine 5 (0: idle footwork, 1: sub 2).
void cPlayer::beginEvent(u32 flag)
{
    interrupt();
    Neck->m_MotR = 0;
    switch (flag) {
    case 0:
        EmRoutineSet(this, 5, 0, 0, 0);
        MotionBlendOff(this);
        atari.off();
        be_flag |= 0x04000000;
        if (Wep->m_pWep) {
            Wep->m_pWep->resetMotion();
        }
        setFootwork();
        m_Flag &= ~0x100;
        break;
    case 1:
        EmRoutineSet(this, 5, 2, 0, 0);
        break;
    }
    stat.on(F_EVENT);
}

// Stop everything the routines left running: cameras, neck, motion speed, weapon, face, sounds.
void cPlayer::interrupt()
{
    cModelInfo* face;

    endCamera();
    stat.on(F_SHADOW);
    m_BbtnCnt = 0;
    Neck->m_Mode = 1;
    MOTION(this)->Seq_speed = 1.0f;
    stat.off(F_CROUCH);
    ang.y += pList->ang.y;
    pList->ang.y = 0.0f;
    if (Wep->m_pWep) {
        if (Wep->m_pWepHand) {
            Wep->m_pWepHand->setDisp(1, 1);
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
        face->mat[2][2] = 0.0f;
        face->mat[1][1] = 0.0f;
        face->mat[0][0] = 0.0f;
    }
    if (pG->pl_type == 4 && (StaFlagChk(pG, STA_KLAUSER_TRANSFORM))) {
        StaFlagOff(pG, STA_KLAUSER_TRANSFORM);
        x890 = 0;
        if (krEffWait == -1) {
            krEffWait = 1;
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

    if (stat.check(F_THERMO)) {
        stat.off(F_THERMO);
        StaFlagOff(pG, STA_THERMO_GRAPH);
        if (StaFlagChk(pG, STA_SUB_SCRN)) {
            LightMgr.update(CamCtrl.areaNo, 0);
        }
    }
    if (stat.check(F_SCOPE)) {
        CamCtrl.endScope();
        if (StaFlagChk(pG, STA_SUB_SCRN)) {
            CameraMove();
        }
        stat.off(F_SCOPE);
        be_flag |= 2;
        ret = 1;
    }
    if (stat.check(F_BINOCULAR)) {
        CamCtrl.LowerBinocular();
        if (StaFlagChk(pG, STA_SUB_SCRN)) {
            CameraMove();
        }
        stat.off(F_BINOCULAR);
        SpfFlagOff(pG, SPF_KEY);
        ret = 1;
    }
    if (stat.check(F_OBJPUSH)) {
        CamCtrl.endPushObject();
        if (StaFlagChk(pG, STA_SUB_SCRN)) {
            CameraMove();
        }
        stat.off(F_OBJPUSH);
        ret = 1;
    }
    return ret;
}

// Event end.
void cPlayer::endEvent(u32 mode)
{
    endEvent0(mode);
}

// Event end (stat bit1 set): the player is drawn / collides / moves again, invulnerable for 10
// frames, neck on; when alive mode 0 = back to routine 0/0 with a pending footwork (r_no_3 1),
// 1 = m_Flag 0x100 (return when the event motion ends), 2 = routine 0/0 at once.
void cPlayer::endEvent0(u32 mode)
{
    int one = 1;

    if (!(stat.check(F_EVENT))) {
        return;
    }
    be_flag |= 2;
    atari.on();
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
            EmRoutineSet(this, 0, 0, 0, one);
            break;
        case 1:
            m_Flag |= 0x100;
            break;
        case 2:
            EmRoutineSet(this, 0, 0, 0, 0);
            break;
        default:
            pLog->err(0, 0, "PL::endEvent() UNKNOWN MODE.");
            break;
        }
    }
    stat.off(F_EVENT);
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
    EmRoutineSet(this, 5, 0, 0, 0);
    MotionBlendOff(this);
    if (Wep->m_pWep) {
        Wep->m_pWep->resetMotion();
    }
    setFootwork();
    stat.on(F_EVENT);
}

// Action end: back to routine 0 with sub routine `routine` pending (m_Hokan).
// `one` at function scope with a single use in another block: update_equiv_regs moves its `li`
// next to the `stb`, so the short-lived constant outranks the flags chain for r0.
void cPlayer::endAction(int hokan)
{
    int one = 1;

    if (stat.check(F_EVENT)) {
        be_flag |= 2;
        setNoSuspend(0);
        m_Hokan = hokan;
        stat.off(F_EVENT);
        m_Frame = 0;
        EmRoutineSet(this, 0, 0, 0, one);
    }
}

// Motion speed of the player and the weapon (Leon only).
void cPlayer::setSlow(f32 speed)
{
    if (pG->pl_type != 0) {
        return;
    }
    MOTION(this)->Seq_speed = speed;
    if (Wep->m_pWep) {
        MOTION(Wep->m_pWep)->Seq_speed = speed;
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

// Eyelid (parts 0x1C) blink sequence on `timer` and the eye direction (parts 0x20/0x21) wander
// (eyeDir).
void cPlayer::moveEyeNormal()
{
    static cDelayF eyeDir;
    static int timer;
    cParts* p;

    p = getPartsPtr(0x1C);
    // Every case written out separately in ascending order: jump2 cross-jumps the identical
    // bodies into the last copy, which gives the target's body layout (3, 4, 0x1E, 0x58, 0x5A,
    // 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62) and constant-pool order.
    switch (timer++) {
    default:
        p->ang.x = 0.0f;
        break;
    case 0: {
        eyeDir = ((f32) (Rnd() % 200) * 0.01f - 1.0f) * 3.1415927f * 0.1f;
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
        eyeDir = 0.0f;
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
            eyeDir += ((f32) (Rnd() % 200) * 0.01f - 1.0f) * 0.03141592815518379f;
            eyetime = Rnd() % 3 + 2;
        }
    }
    eyeDir.limit(-0.3141592741012573f, 0.3141592741012573f);
    p = getPartsPtr(0x20);
    p->ang.y = eyeDir;
    p->matUpdate();
    p = getPartsPtr(0x21);
    p->ang.y = eyeDir;
    p->matUpdate();
    eyeDir.move();
}

int lbl_80314CDC = 0;   // unreferenced 4-byte .sdata word between moveEyeNormal's statics and the neck

// Eyes driven by the motion: parts 0x21 follows parts 0x20.
void cPlayer::moveEyeMotion()
{
    cParts* a;
    cParts* b;

    a = getPartsPtr(0x20);
    b = getPartsPtr(0x21);
    b->ang = a->ang;
    b->matUpdate();
}

// Updates the body / weapon matrices and lets the weapon object draw its laser (flag bit1).
void cPlayer::setLaserSight(int draw, int noCalc)
{
    Body->move();
    partsWorldCalc();
    Wep->m_pWep->partsWorldCalc();
    Wep->m_pWep->flag.on(cObjWep::F_ON_LASER_SIGHT_D);
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

    if (!(stat.check(F_SHADOW)) || pG->Camera.param.pos.y < pos.y || !pFloor_norm || pFloor_norm->y < 0.8f) {
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
    m_NeckY = 0.0f;
    m_lockCtr = 0;
    m_MotR = 0;
    m_MotL = 0;
    m_Flag = 0;
    m_Mode = 1;
}

// Neck motions: motR (frame `frame`) is set at once; the look timer restarts.
void cPlNeck::init(void* motR, void* motL, int frame)
{
    m_MotR = motR;
    m_MotL = motL;
    if (VALID_PTR2(motR) && VALID_PTR2(motL)) {
        motSet(motR, frame);
        move();
        m_Flag = 0;
        if (m_lockCtr > 0x10000000) {
            m_lockCtr = 0x7FFFFFFF;
        } else {
            m_lockCtr = 0;
            m_NeckY = 0.0f;
        }
    }
}

// Turn the head towards the target enemy (6 degrees a frame, +-45), switching the left / right
// motion at the centre; the blend rate is the angle over 45 degrees.
void cPlNeck::move()
{
    cParts* head;
    cEm* em;

    if (m_Mode == 0) {
        return;
    }
    if (m_Mode == 2) {
        m_Mode = 1;
        return;
    }
    if (!VALID_PTR2(m_MotR)) {
        return;
    }
    if (!VALID_PTR2(m_MotL)) {
        return;
    }
    if (pPL->r_no_0 > 1) {
        return;
    }
    head = pPL->getPartsPtr(0);
    em = getTarget();
    if (em) {
        if (em != m_pLastTarget) {
            m_pLastTarget = em;
            m_lockCtr = em->checkStatus(EM_STATUS_LOOK_ME) ? 0x7FFFFFFF : 20;
        }
    } else {
        if (m_pLastTarget && m_pLastTarget->hp <= 0) {
            m_pLastTarget = 0;
            m_lockCtr = 0;
        }
    }
    if (pPL->r_no_0 != 0 || pPL->r_no_1 > 3) {
        m_lockCtr = 0;
    }
    if (m_lockCtr) {
        f32 a = GetXZAngleLocal(&head->world, &m_pLastTarget->pos, pPL->ang.y);
        if (a > m_NeckY) {
            if (a - m_NeckY > 0.10471975803375244f) {
                m_NeckY = m_NeckY + 0.10471975803375244f;
            } else {
                m_NeckY = a;
            }
        } else if (a < m_NeckY) {
            if (a - m_NeckY < -0.10471975803375244f) {
                m_NeckY = m_NeckY - 0.10471975803375244f;
            } else {
                m_NeckY = a;
            }
        }
        if (m_NeckY > 0.7853981852531433f) {
            m_NeckY = 0.7853981852531433f;
        }
        if (m_NeckY < -0.7853981852531433f) {
            m_NeckY = -0.7853981852531433f;
        }
        if (m_NeckY < 0.0f && m_Flag) {
            m_Flag &= 0xFFFE;
            motSet(m_MotR, (u16) pPL->Motion.Seq_frame);
        } else if (m_NeckY > 0.0f && !m_Flag) {
            m_Flag |= 1;
            motSet(m_MotL, (u16) pPL->Motion.Seq_frame);
        }
        m_lockCtr--;
    } else {
        if (m_NeckY != 0.0f) {
            if (m_NeckY > 0.10471975803375244f) {
                m_NeckY = m_NeckY - 0.10471975803375244f;
            } else if (m_NeckY < -0.10471975803375244f) {
                m_NeckY = m_NeckY + 0.10471975803375244f;
            } else {
                m_NeckY = 0.0f;
            }
        }
    }
    if (pPL->Motion.blend) {
        f32 rate = m_NeckY / 0.7853981852531433f;
        if (!(m_Flag & 1)) {
            rate = -rate;
        }
        pPL->Motion.blend->Brate = rate;
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
    p->m_SubMot.Mot_flag |= 0x10000000;
    MotionSetCore(p, &p->m_SubMot, data, 0, 8, 5, frame);
    p->m_SubMot.Mot_flag &= ~0x10000000;
    p->Motion.blend = &p->m_SubMot;
    p->Motion.blend->Brate = 1.0f;
    p->Motion.blend->Mot_flag |= 0x80000000;
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
    for (em = EmMgr.getActiveWork(); em; em = EmMgr.getNext(em)) {
        if (em->checkStatus(EM_STATUS_LOCKOFF)) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        if (em == pPL) {
            continue;
        }
        if (em == pSUB) {
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
void cPlNeck::setMode(int mode)
{
    m_Mode = mode;
}

// Waist straight.
cPlWaist::cPlWaist()
{
    m_Ang.y = 0.0f;
}

// Moves the waist angle toward `target` by `rate` (0..1 per frame); returns the change applied.
f32 cPlWaist::set(f32 dir, f32 rate)
{
    f32 old = m_Ang.y;

    m_Ang.y = m_Ang.y * (1.0f - rate) + dir * rate;
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
void cMot3::set(cModel* m, void* m0, void* m1, void* m2, void* seq, u8 b, int c, u16 d, u16 e)
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
    MotionSetCore(m, MOTION(m), m0, seq, mode, d, e);
    set0(m1, e, mode);
    ((cEm*) m)->Motion.blend->Brate = 0.0f;
}

// Blend motion `m` (frame a, hokan b) into the model's motion.
void cMot3::set0(void* m, u8 a, int b)
{
    MotionSetCore(m_pEm, &work, m, 0, b, 4, a);
    switch (m_Mode) {
    case 0:
        break;
    case 1:
        work.Mot_flag |= 0x80000000;
        break;
    }
    ((cEm*) m_pEm)->Motion.blend = &work;
}

// Blend rate -1..1: crossing 0 switches the blended motion (mot1 below, mot2 above) at the current
// frame; the work's blend rate is |rate|.
void cMot3::move(f32 r0)
{
    if (m_pEm == 0) {
        return;
    }
    if (((cEm*) m_pEm)->Motion.blend == 0) {
        return;
    }
    if (r0 > 1.0f) {
        r0 = 1.0f;
    }
    if (r0 < -1.0f) {
        r0 = -1.0f;
    }
    if (m_Rate < 0.0f && r0 >= 0.0f) {
        set0(mot1, (u8) ((cEm*) m_pEm)->Motion.Seq_frame, 2);
    } else if (m_Rate >= 0.0f && r0 < 0.0f) {
        set0(mot2, (u8) ((cEm*) m_pEm)->Motion.Seq_frame, 2);
    }
    m_Rate = r0;
    if (r0 < 0.0f) {
        r0 = -r0;
    }
    ((cEm*) m_pEm)->Motion.blend->Brate = r0;
}

const f32 cPlayer::SPEED_WALK_TURN = 0.0418879f;
const f32 cPlayer::SPEED_RUN_TURN = 0.0418879f;
const f32 cPlWaist::ROT_LIMIT = 0.39269909262657166f;
