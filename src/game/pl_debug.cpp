// game/pl_debug: player debug helpers — the "maho" cheats (button sequences registered in
// cPlMaho: no death, infinite ammo, skeleton display, collision off, kaiouken speed-ups, Ashley
// teleport), the per-frame debug draws (scroll hit test, collision quad test, local coordinate
// finder, damage capsules, position marker), PlWepMotSet and the DrawGage life bar.

#include "player.h"
#include "global.h"
#include "atari.h"
#include "em.h"
#include "joy.h"
#include "db_log.h"
#include "eprintf.h"
#include "dbmodule.h"
#include "math_sub.h"

extern cModel* pSUB;

int MotionSetCore(cModel* m, void* work, void* data, int a, int b, int c, int d);  // game/motion.cpp
extern "C" void EmYarareDisp(cModel* m);                                            // game/em_sub.cpp
extern "C" void DrawOba(cModel* m);                                                 // game/at_mod.cpp

int scr_hit_check = 0;
static int sat_make_test = 0;
int local_coord_test = 0;

u8 PlCapNum[25];

// Cheat: no death (Debug_flg[2] 0x800000).
void mahoMuteki()
{
    DbgFlagOn(pG, DBG_NO_DEATH);
    pLog->mes(0, 0, "NO DEATH ON");
}

// Cheat: infinite ammo (Debug_flg[2] 0x400000).
void mahoInfBul()
{
    DbgFlagOn(pG, DBG_INF_BULLET);
    pLog->mes(0, 0, "INF BULLET ON");
}

// Cheat: collision skeleton display on (Debug_flg[0] / Disp_flg 0x8000000).
void mahoSkelOn()
{
    DbgFlagOn(pG, DBG_SAT_DISP);
    DpfFlagOn(pG, DPF_SCR);
    pLog->mes(0, 0, "SKELTON ON");
}

// Cheat: collision skeleton display off.
void mahoSkelOff()
{
    DbgFlagOff(pG, DBG_SAT_DISP);
    DpfFlagOff(pG, DPF_SCR);
    pLog->mes(0, 0, "SKELTON OFF");
}

// Cheat: teleports the partner (pSUB) to the player.
void mahoCallSc()
{
    if (pSUB) {
        pSUB->setPos(&pPL->pos);
        pLog->mes(0, 0, "ASHLEY TELEPORT");
    }
}

// Cheat: kaiouken (speed-up) off.
static void mahoKaiouOff()
{
    DbgFlagOff(pG, DBG_KAIOUKEN);
    pLog->mes(0, 0, "KAIOUKEN OFF");
}

// Cheat: kaiouken x2 (Debug_flg[2] 0x10000, PlKaiou 0).
void mahoKaiou2()
{
    DbgFlagOn(pG, DBG_KAIOUKEN);
    PlKaiou = 0;
    pLog->mes(0, 0, "KAIOUKEN x2");
}

// Cheat: kaiouken x3 (PlKaiou 1).
void mahoKaiou3()
{
    DbgFlagOn(pG, DBG_KAIOUKEN);
    PlKaiou = 1;
    pLog->mes(0, 0, "KAIOUKEN x3");
}

// Cheat: kaiouken x4 (PlKaiou 2).
void mahoKaiou4()
{
    DbgFlagOn(pG, DBG_KAIOUKEN);
    PlKaiou = 2;
    pLog->mes(0, 0, "KAIOUKEN x4");
}

// Cheat: the player passes through collision.
void mahoThroughOn()
{
    pPL->atari.throughOn();
    pLog->mes(0, 0, "PL THROUGH ON");
}

// Cheat: collision back on.
void mahoThroughOff()
{
    pPL->atari.throughOff();
    pLog->mes(0, 0, "PL THROUGH OFF");
}

// Registers the "maho" button-sequence cheats (digits = d-pad / buttons, A/B = the confirm key).
void cPlayer::debugInit()
{
    pMaho = new cPlMaho;
    pMaho->regist("43243262A", mahoSkelOn);
    pMaho->regist("43243262B", mahoSkelOff);
    pMaho->regist("456456A", mahoThroughOn);
    pMaho->regist("456456B", mahoThroughOff);
    pMaho->regist("40404040A", mahoKaiou4);
    pMaho->regist("404040A", mahoKaiou3);
    pMaho->regist("4040A", mahoKaiou2);
    pMaho->regist("4040B", mahoKaiouOff);
    pMaho->regist("0426A", mahoMuteki);
    pMaho->regist("0462A", mahoInfBul);
    pMaho->regist("243A", mahoCallSc);
}

// Debug (scr_hit_check): casts a 5000-unit line forward from 1000 above the player through the
// scroll collision and draws the hit / normal.
void scrHitCheck(cPlayer* pl)
{
    if (scr_hit_check) {
        static u32 hcFlag = 0x8000;
        static u32 hcMask = 0;
        Vec top;
        Vec dir;
        Vec hit;
        Vec nrm;
        int ret;

        top = pl->pos;
        top.y += 1000.0f;
        dir.x = 0.0f;
        dir.y = 0.0f;
        dir.z = 5000.0f;
        PSMTXMultVecSR(pl->mat, &dir, &dir);
        PSVECAdd(&dir, &top, &dir);
        ret = SatMgr.hitCheck(&top, &dir, &hit, &nrm, hcFlag, hcMask);
        Draw_line3d(&top, &hit, ret ? 0xFFFF0000 : 0xFFFFFFFF, 0);
        if (ret) {
            PSVECScale(&nrm, &dir, 1000.0f);
            PSVECAdd(&dir, &hit, &dir);
            Draw_line3d(&hit, &dir, 0xFFFFFFFF, 0);
        }
    }
}

// Debug (sat_make_test): A creates a 2000 x 2000 collision quad 1000 ahead of the player.
void satMakeTest(cPlayer* pl)
{
    if (sat_make_test) {
        static cSat* pS0 = 0;
        static Vec quad[4] = {
            {-1000.0f, 0.0f, -1000.0f},
            {1000.0f, 0.0f, -1000.0f},
            {1000.0f, 0.0f, 1000.0f},
            {-1000.0f, 0.0f, 1000.0f},
        };
        static Vec z0 = {0.0f, 0.0f, 1000.0f};
        Vec pos;
        cSatMgr* sat = &SatMgr;

        eprintf(100, 100, 0, 0, "%d", sat->nArray);
        if (Joy[0].on & JOY_A) {
            if (pS0) {
                SatMgr.destroy(pS0);   // on the object: devirtualised `bl destroy__7cSatMgrP4cSat`
                pS0 = 0;
            }
            RotVector(&z0, &pl->ang, &pos);
            PSVECAdd(&pos, &pl->pos, &pos);
            pS0 = sat->create(&pos, &pl->ang, quad, 0, 0x200, 0.0f);
        }
    }
}

// Debug (local_coord_test): moves a point in parts space of the player (pad 2 stick / triggers,
// Y / X change the parts) and prints its coordinates — for finding attachment offsets.
void localCoordTest(cPlayer* pl)
{
    if (local_coord_test) {
        static Vec vpos;
        static u16 pl_db_parts_no = 0;
        Mtx m;

        vpos.x += (f32) Joy[2].stickX / 100.0f;
        vpos.y += (f32) Joy[2].stickY / 100.0f;
        vpos.z = vpos.z + (f32) Joy[2].triggerRight / 200.0f - (f32) Joy[2].triggerLeft / 200.0f;
        if (Joy[0].trg & JOY_Y) {
            pl_db_parts_no++;
        }
        if (Joy[0].trg & JOY_X) {
            pl_db_parts_no--;
        }
        PSMTXConcat(pG->Cam.v_mat, pl->getPartsPtr(pl_db_parts_no)->mat, m);
        Draw_local_pos(&vpos, 1000, m);
        eprintf(40, 100, 0, 0, "%5.2f", vpos.x);
        eprintf(40, 116, 0, 0, "%5.2f", vpos.y);
        eprintf(40, 132, 0, 0, "%5.2f", vpos.z);
    }
}

// Dead-stripped by the original linker (pool "%f", 10000.0f and the static `tang` remain).
static void tangentTest(cPlayer* pl)
{
    static Vec tang;

    tang = pl->speed;
    eprintf(40, 100, 0, 0, "%f", tang.x * 10000.0f);
}

// Debug draws each frame: the tests above, nearest enemy search, the damage capsules
// (EmYarareDisp), the oba collision (Debug_flg[2] 0x10000000) and the position marker (PlDbFlag bit1).
void cPlayer::debugMove()
{
    scrHitCheck(this);
    satMakeTest(this);
    emSearch();
    localCoordTest(this);
    EmYarareDisp(this);
    if (DbgFlagChk(pG, DBG_OBA_VIEW)) {
        DrawOba(this);
    }
    if (PlDbFlag & 2) {
        Draw_pos(&pos, 1000);
    }
}

// Finds the nearest living enemy (distance only; the result is not kept — debug leftover).
void cPlayer::emSearch()
{
    f32 min = 100000.0f;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((em->be_flag & 0x201) == 1 && em->hp > 0) {
            f32 d = GetDistance3(&pos, &em->pos);
            if (d > min) {
                continue;
            }
            min = d;
        }
    }
}

// Plays weapon motion `no` (0-2 PlWepMot[], 3 the stand motion) on the player with a 3-frame blend.
void PlWepMotSet(int no)
{
    void* mot = 0;
    cPlayer* pl = pPL;

    switch (no) {
    case 0:
        mot = PlWepMot[0];
        break;
    case 1:
        mot = PlWepMot[1];
        break;
    case 2:
        mot = PlWepMot[2];
        break;
    case 3:
        mot = pl->m_MotTbl[0];
        break;
    }
    MotionSetCore(pl, &pl->pMotion, mot, 0, 3, 5, 0);
}

// Empty cheat table.
cPlMaho::cPlMaho()
{
    reset();
    num = 0;
}

// Resets every cheat's input progress (rno).
void cPlMaho::reset()
{
    int i;

    for (i = 0; i < 30; i++) {
        tbl[i].rno = 0;
    }
}

// Adds a cheat: `code` is the button sequence, `func` runs when it is completed.
void cPlMaho::regist(const char* code, void (*func)())
{
    PlMahoEntry* e = &tbl[num];

    e->rno = 0;
    e->timer = 0;
    e->func = func;
    e->pSpell = code;
    num++;
}

// DrawGage: `len` is the one variable for both widths and the right edge (a multi-set pseudo is
// never tied to the dying product, so `fw*fnow` stays in f12 like the original); the original's
// `fadds f25,f13,f25` (fx first) comes from adding a cse-deleted copy of len (see the note there).
void DrawGage(int x, int y, int h, int w, int now, int max, int color)
{
    Vec pos;
    Vec size;
    f32 len, fx, fy, fw, fnow, fmax, fh;

    if (now > max) {
        now = max;
    }
    fx = (f32) x;
    fy = (f32) y;
    fw = (f32) w;
    fnow = (f32) now;
    fmax = (f32) max;
    fh = (f32) h;
    len = fw * fnow / fmax;

    pos.x = fx;
    pos.y = fy;
    pos.z = 1.0f;
    size.x = len;
    size.y = fh;
    size.z = 1.0f;
    // `len = fx + len` is expanded with the destination operand first (`fadds len,len,fx`:
    // expand_binop swaps a commutative op when op1 is the target rtx); the original has `fadds
    // len,fx,len`. Adding a copy of len keeps op1 != target, cse canonicalises the copy back to
    // len (deleting it) and leaves the operand order alone.
    f32 l2 = len;
    len = fx + l2;
    Draw_quad(&pos, &size, color);

    pos.x = len;
    pos.y = fy;
    pos.z = 1.0f;
    len = fw * (1.0f - fnow / fmax);
    size.x = len;
    size.y = fh;
    size.z = 1.0f;
    Draw_quad(&pos, &size, color & 0xFF000000);
}

// the split object's .sdata is 8-aligned
asm(".section .sdata; .balign 8");
