// game/objPillar: object id 0x1F, the falling pillar of the Salazar / statue fights
// (D:/Bio4/Prog/objPillar.cpp): stands (R0 0), topples onto the player (R0 1 Break) or is thrown
// at him (R0 2 Throw); the player escapes with action button 0x25 (plemEscape / plemEscape2,
// run as player damage routines) or is crushed (objPillarAtkCk); R0 4 Fall drops it as debris.
#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "objPillar.h"
#include "em.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "rnd.h"
#include "quake.h"
#include "pad.h"
#include "player.h"
#include "pl_sub.h"
#include "motion.h"
#include "game.h"
#include "em_sub.h"

extern "C" {
void objPillar_R0_Set(cObjPillar* obj);
void objPillar_R0_Break(cObjPillar* obj);
void objPillar_R0_Throw(cObjPillar* obj);
void objPillar_R0_Escape(cObjPillar* obj);
void objPillar_R0_Fall(cObjPillar* obj);
void objPillarAtkCk(cObjPillar* obj, Vec* pos);
void EscapeAction(cObjPillar* obj);
void EscapeCamMove();
void EscapeAction2(cObjPillar* obj);
void plemEscape2(cPlayer* pl);
void objPillarEatSet(cObjPillar* obj);
}
// The original is a `static plemEscape` (emBar.cpp has a global one); the name carries the split's
// address suffix so the report can pair it with the local symbol.
#define plemEscape plemEscape_8003C33C
extern "C" {
static void plemEscape(cPlayer* pl);

}

void (*ObjPillar_R0_move_tbl[5])(cObjPillar*) = {
    objPillar_R0_Set, objPillar_R0_Break, objPillar_R0_Throw, objPillar_R0_Escape, objPillar_R0_Fall,
};

EmAtkInfo ObjPillar_atk_info = { 1000.0f, PL_DM_AUTO, 1000, 0, 10, 0 };

Camera Cam;   // escape sequence camera

// Creates a pillar (id 0x1F) at pos/rot: capsule collision 400 x 5000, no suspend, no eat yet.
cObj* SetPillar(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    PillarWork* w;

    obj = ObjMgr.create(cObjMgr::ID_PILLAR);
    if (obj == 0) {
        return 0;
    }
    w = PILLAR_WK((cObjPillar*) obj);
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    obj->pos_old = obj->pos;
    if (rot) {
        obj->ang = *rot;
    } else {
        obj->ang.x = 0.0f;
        obj->ang.y = 0.0f;
        obj->ang.z = 0.0f;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    obj->setNoSuspend(1);
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->atari, 0.0f, 0.0f, 0.0f, 400.0f, 400.0f, 400.0f, 5000.0f, 0, 2, 0);
    obj->atari.clrFlag100();
    w->Mot_pl_escape = 0;
    w->Seq_pl_escape = 0;
    w->St_pos = obj->pos;
    w->pEat = 0;
    obj->r_no_0 = 0;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    return obj;
}

// Per-frame: releases the eat collision, runs the R0 routine.
void cObjPillar::move()
{
    if (PILLAR_WK(this)->pEat) {
        PILLAR_WK(this)->pEat->setDisable();
    }
    ObjPillar_R0_move_tbl[r_no_0](this);
}

// Rno0 == 0: standing (Be_flg 1): matrices and the eat collision quad placed.
void objPillar_R0_Set(cObjPillar* pObj)
{
    PillarWork* w = PILLAR_WK(pObj);

    w->Be_flg |= 1;
    w->St_pos = pObj->pos;
    pObj->matUpdate();
    pObj->partsWorldCalc();
    objPillarEatSet(pObj);
}

// Rno0 == 1 (setBreak): the pillar topples with Mot towards the player: creak sound when he
// is near, crushing hit tests on parts 1/2 (objPillarAtkCk), the escape action button (0x25)
// offered while he stands in front, fade-out 10 frames before the end; removed when Scenario_flg[1]
// 0x200 (the boss died).
void objPillar_R0_Break(cObjPillar* pObj)
{
    PillarWork* w = PILLAR_WK(pObj);
    Mtx inv;
    Vec v;
    u8 step = pObj->r_no_2;

    switch (step) {
    case 0:
        w->Timer = (*(u16*) w->Mot & 0x3FFF) - 10;
        MotionSetCore(pObj, &pObj->Motion, w->Mot, 0, 0, 0x8001, 0);
        w->TmpU32 = Rnd() & 1;
        w->Act_ck = step;
        w->Seid = step;
        pObj->r_no_2++;
    case 1:
        if (MotionMove(pObj, 0)) {
            pObj->be_flag &= ~2;
            pObj->r_no_2++;
        } else {
            objPillarAtkCk(pObj, &pObj->getPartsPtr(1)->world);
            objPillarAtkCk(pObj, &pObj->getPartsPtr(2)->world);
            if (w->Seid == 0) {
                cParts* parts = pObj->getPartsPtr(1);

                if ((parts->world.x - pPL->pos.x) * (parts->world.x - pPL->pos.x) +
                    (parts->world.y - pPL->pos.y) * (parts->world.y - pPL->pos.y) +
                    (parts->world.z - pPL->pos.z) * (parts->world.z - pPL->pos.z) < 16000000.0f) {
                    w->Seid = SndCall(8, 0x2C, &parts->world, 0x31, 0, pObj);
                }
            }
            if (w->Timer) {
                w->Timer--;
            } else {
                pObj->invisible_factor -= 0.1f;
                if (pObj->invisible_factor < 0.0f) {
                    pObj->invisible_factor = 0.0f;
                    pObj->be_flag &= ~2;
                }
            }
        }
        break;
    }
    pObj->partsWorldCalc();
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        ObjMgr.destroy(pObj);
        return;
    }
    step = w->Act_ck;
    if (step == 0) {
        PSMTXInverse(pObj->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &v);
        if (v.x > -2000.0f && v.x < 2000.0f && v.z > -1000.0f) {
            if (w->TmpU32) {
                ActBtn.set(ACT_GUARD, 0xB, (void*) EscapeAction, pObj, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
            } else {
                ActBtn.set(ACT_GUARD, 0xB, (void*) EscapeAction, pObj, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);
            }
        }
    }
}

// Rno0 == 2 (setThrow): lifted (Mot_catch from frame 31, dust effect), then flies at 500 units/frame
// towards the player (motThrow1, trail effect) for 90 frames with crushing hit tests along its
// length; the escape button is offered once it is within 1000 units.
void objPillar_R0_Throw(cObjPillar* pObj)
{
    PillarWork* w = PILLAR_WK(pObj);
    Vec d;
    Vec v;
    Mtx m;
    cParts* parts;
    f32 len;
    u8 step = pObj->r_no_2;
    u8 esc;

    switch (step) {
    case 0:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_catch, 0, 0, 0x8001, 0x1F);
        w->TmpU32 = Rnd() & 1;
        w->Act_ck = 1;
        EstSet(pObj, -1, 0, 0, EFF_EM31, 0x22, 0, ESP_CORE_KIND_NONE, pObj, (void*) step);
        w->Seid = step;
        pObj->r_no_2++;
    case 1:
        if (MotionMove(pObj, 0)) {
            pObj->r_no_2++;
        }
        break;
    case 2:
        parts = pObj->getPartsPtr(0);
        pObj->pos = parts->world;
        w->Spd.x = 0.0f;
        w->Spd.y = 0.0f;
        w->Spd.z = 500.0f;
        v = pPL->pos;
        v.y += 1000.0f;
        PSVECSubtract(&v, &pObj->pos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z) / 500.0f;
        if (len == 0.0f) {
            w->Spd.y = 0.0f;
        } else {
            w->Spd.y = d.y / len;
        }
        PSMTXMultVecSR(pObj->mat, &w->Spd, &w->Spd);
        MotionSetCore(pObj, &pObj->Motion, w->Mot_throw, 0, 0, 0x8005, 0);
        w->TmpU32 = Rnd() & 1;
        w->Act_ck = 0;
        w->Timer = 90;
        EstSet(pObj, -1, 0, 0, EFF_EM31, 0x23, 1, ESP_CORE_KIND_OBJPILLAR, pObj, 0);
        pObj->r_no_2++;
    case 3:
        PSVECAdd(&pObj->pos, &w->Spd, &pObj->pos);
        MotionMove(pObj, 0);
        if (w->Seid == 0) {
            parts = pObj->getPartsPtr(0);
            len = (parts->world.x - pPL->pos.x) * (parts->world.x - pPL->pos.x) +
                  (parts->world.y - pPL->pos.y) * (parts->world.y - pPL->pos.y) +
                  (parts->world.z - pPL->pos.z) * (parts->world.z - pPL->pos.z);
            if (len < 16000000.0f) {
                w->Seid = SndCall(8, 0x2C, &parts->world, 0x31, 0, pObj);
            }
        }
        if (w->Timer == 0) {
            pObj->invisible_factor -= 0.1f;
            if (pObj->invisible_factor < 0.0f) {
                EffectEspDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj, 0);
                EffectEspgenDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj);
                EffectEfmDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj);
                pObj->invisible_factor = 0.0f;
                ObjMgr.destroy(pObj);
                return;
            }
        } else {
            w->Timer--;
        }
        parts = pObj->getPartsPtr(0);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(pObj->mat, &v, &v);
        objPillarAtkCk(pObj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1000.0f;
        PSMTXMultVec(pObj->mat, &v, &v);
        objPillarAtkCk(pObj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 2000.0f;
        PSMTXMultVec(pObj->mat, &v, &v);
        objPillarAtkCk(pObj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 3000.0f;
        PSMTXMultVec(pObj->mat, &v, &v);
        objPillarAtkCk(pObj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -1000.0f;
        PSMTXMultVec(pObj->mat, &v, &v);
        objPillarAtkCk(pObj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -2000.0f;
        PSMTXMultVec(pObj->mat, &v, &v);
        objPillarAtkCk(pObj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -3000.0f;
        PSMTXMultVec(pObj->mat, &v, &v);
        objPillarAtkCk(pObj, &v);
        break;
    }
    pObj->partsWorldCalc();
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        ObjMgr.destroy(pObj);
        return;
    }
    esc = w->Act_ck;
    if (esc == 0) {
        PSMTXRotRad(m, 'y', GetXZAngle(&pObj->pos_old, &pObj->pos));
        TransMatrix(m, &pObj->pos);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &pPL->pos, &d);
        if (d.z < 1000.0f) {
            w->Act_ck = 1;
        }
        if (w->TmpU32) {
            ActBtn.set(ACT_GUARD, 0xB, (void*) EscapeAction2, pObj, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
        } else {
            ActBtn.set(ACT_GUARD, 0xB, (void*) EscapeAction2, pObj, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);
        }
    }
}

// Rno0 == 3 (escape succeeded): the pillar plays Mot_escape from the player's position/heading
// (it passes over him), then is removed.
void objPillar_R0_Escape(cObjPillar* pObj)
{
    PillarWork* w = PILLAR_WK(pObj);

    switch (pObj->r_no_2) {
    case 0:
        memcpy((u8*) pObj + ((u32) &((cObj*) 0)->pos), &pPL->pos, sizeof(Vec));
        memcpy((u8*) pObj + ((u32) &((cObj*) 0)->ang), &pPL->ang, sizeof(Vec));
        MotionSetCore(pObj, &pObj->Motion, w->Mot_escape, 0, 0, 0x8001, 0);
        SndStop(w->Seid, 0);
        w->Seid = SndCall(8, 0x2C, &pObj->getPartsPtr(0)->world, 0x31, 0, pObj);
        pObj->r_no_2++;
    case 1:
        if (MotionMove(pObj, 0)) {
            EffectEspDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj);
            EffectEfmDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj);
            pObj->invisible_factor = 0.0f;
            ObjMgr.destroy(pObj);
            return;
        }
        break;
    }
    pObj->partsWorldCalc();
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        ObjMgr.destroy(pObj);
    }
}

// Rno0 == 4 (setFall): drops from parts 0's position (gravity 15/frame), lands on the floor with
// motFall1 and a sound, fades out after 30 frames.
void objPillar_R0_Fall(cObjPillar* pObj)
{
    PillarWork* w = PILLAR_WK(pObj);
    f32 floor;

    switch (pObj->r_no_2) {
    case 0:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_fall, 0, 0, 0x8004, 0);
        w->Spd.x = 0.0f;
        w->Spd.y = -100.0f;
        w->Spd.z = 0.0f;
        pObj->r_no_2++;
    case 1:
        PSVECAdd(&pObj->pos, &w->Spd, &pObj->pos);
        w->Spd.y -= 15.0f;
        floor = EatMgr.getFloor(&pObj->pos_old, 0, 600.0f, 100000.0f, 0);
        if (pObj->pos.y < floor) {
            pObj->pos.y = floor;
            SndCall(8, 0x26, &pObj->pos, 0x31, 0, pObj);
            MotionSetCore(pObj, &pObj->Motion, w->Mot_landing, 0, 0, 0x8001, 0);
            MotionMove(pObj, 0);
            pObj->r_no_2++;
        } else {
            MotionMove(pObj, 0);
        }
        break;
    case 2:
        pObj->r_no_2++;
        w->Timer = 30;
    case 3:
        MotionMove(pObj, 0);
        if (w->Timer == 0) {
            pObj->invisible_factor -= 0.1f;
            if (pObj->invisible_factor < 0.0f) {
                EffectEspDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj, 0);
                EffectEspgenDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj);
                EffectEfmDelete(1, ESP_CORE_KIND_OBJPILLAR, pObj);
                pObj->invisible_factor = 0.0f;
                ObjMgr.destroy(pObj);
                return;
            }
        } else {
            w->Timer--;
        }
        break;
    }
    pObj->partsWorldCalc();
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        ObjMgr.destroy(pObj);
    }
}

// Sets the break motion.
void cObjPillar::setMotion(void* mot)
{
    PILLAR_WK(this)->Mot = mot;
}

// 1 while the pillar is still standing.
int cObjPillar::ckSet()
{
    if (PILLAR_WK(this)->Be_flg & 1) {
        return 1;
    }
    return 0;
}

// Topples the pillar away from `pos` (towards the player when he is in front) with the player's
// escape motion mot/a; collision off.
void cObjPillar::setBreak(Vec* pos, void* mot, void* pl_seq)
{
    PillarWork* w = PILLAR_WK(this);

    w->Be_flg &= ~1;
    ang.y = GetXZAngle(pos, &this->pos);
    if (fabsf(Muku(&this->pos, &pPL->pos, ang.y, PI)) < PI / 2) {
        ang.y = GetXZAngle(&this->pos, &pPL->pos);
    }
    w->Mot_pl_escape = mot;
    w->Seq_pl_escape = pl_seq;
    w->Break_pos = *pos;
    atari.throughOn();
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// The boss throws the pillar: lift/throw motions, the pillar's escape motion and the player's
// escape motion; aimed at the player when within 30 degrees.
void cObjPillar::setThrow(void* mot0, void* mot1, void* motEscape, void* plMot, void* pl_seq)
{
    PillarWork* w = PILLAR_WK(this);

    w->Be_flg &= ~1;
    if (fabsf(Muku(&pos, &pPL->pos, ang.y, PI)) < 0.5235988f) {
        ang.y = GetXZAngle(&pos, &pPL->pos);
    }
    w->Mot_catch = mot0;
    w->Mot_throw = mot1;
    w->Mot_escape = motEscape;
    w->Mot_pl_escape = plMot;
    w->Seq_pl_escape = pl_seq;
    atari.throughOn();
    r_no_0 = 2;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Drops the pillar (fall / land motions).
void cObjPillar::setFall(void* mot0, void* mot1)
{
    PillarWork* w = PILLAR_WK(this);

    w->Be_flg &= ~1;
    pos = getPartsPtr(0)->world;
    w->Mot_fall = mot0;
    w->Mot_landing = mot1;
    atari.throughOn();
    r_no_0 = 4;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Crushing hit test at pos with ObjPillar_atk_info (fatal flag 4 only when the player has more than
// 500 life): on the player damage type 8, blood, death effects when killed, quake, sound,
// vibration; on the partner quake + vibration.
void objPillarAtkCk(cObjPillar* pObj, Vec* pPos)
{
    PillarWork* w = PILLAR_WK(pObj);
    EmAtkInfo* atk = &ObjPillar_atk_info;
    int hit;

    if ((s16) pG->pl_life > 500) {
        atk->flag |= 4;
    } else {
        atk->flag &= ~4;
    }
    hit = EmAtkHitCk(atk, pPos, &w->St_pos, 1);
    if (hit) {
        if (hit & 1) {
            pPL->ang.y = GetXZAngle(&pPL->pos, &w->St_pos);
            PlSetDamage(PL_DM_AUTO, 0, 0);
            EmPlBloodSet2(pObj, pPos, 1, 0x29, 0x3D);
            if ((s16) pG->pl_life <= 0) {
                EstSet(pPL, -1, 0, 0, EFF_EM31, 0x3A, 0, ESP_CORE_KIND_NONE, pPL, 0);
            } else {
                EstSet(pPL, -1, 0, 0, EFF_EM31, 0x3B, 0, ESP_CORE_KIND_NONE, pPL, 0);
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            SndCall(8, 0x25, &pPL->pos, 0x31, 0, pPL);
            w->Act_ck = 1;
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        }
        if (hit & 2) {
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        }
    }
}

// Action button 0x25 during Break: the player dives out of the way (plemEscape), difficulty points.
void EscapeAction(cObjPillar* ptr)
{
    PillarWork* w = PILLAR_WK(ptr);
    u8 one = 1;

    if (!ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        w->Act_ck = one;
        SetPlDamage((cEm*) ptr, plemEscape);
        GameAddPoint(9);
    }
}

// Player escape routine: turns towards the fall point, plays the dive motion (mirrored when the
// pillar comes from the left) with a custom camera (EscapeCamMove), dust and sounds, then
// EndPlDamage.
static void plemEscape(cPlayer* pEm)
{
    cEm* em = (cEm*) pEm;
    cObjPillar* obj = (cObjPillar*) em->pEmCatch;
    PillarWork* w = PILLAR_WK(obj);
    f32 ang;

    em->dmg.m_Timer = 2;
    switch (em->r_no_2) {
    case 0:
        // the dead 0.0f store creates the pool `lis` in this block before the call, so
        // update_equiv_regs leaves it there (callee-saved) instead of moving it after Muku
        ang = 0.0f;
        ang = Muku(&em->pos, &w->Break_pos, em->ang.y, PI);
        if (ang < 0.0f) {
            MotionSetCore(em, &em->Motion, w->Mot_pl_escape, w->Seq_pl_escape, 3, 0x41, 0);
        } else {
            MotionSetCore(em, &em->Motion, w->Mot_pl_escape, w->Seq_pl_escape, 3, 1, 0);
        }
        SndCall(1, 0x48, &em->pos, 0, 0, em);
        SndCall(1, 0x11, &em->getPartsPtr(4)->world, 0, 0, em);
        memclr_asm(&Cam, sizeof(Camera));
        ((cPlayer*) em)->m_Work0 = 50;
        ((cPlayer*) em)->m_Work1 = 15;
        em->r_no_2++;
    case 1:
        EscapeCamMove();
        if (((cPlayer*) em)->m_Work1) {
            em->ang.y += Muku(&em->pos, &w->Break_pos, em->ang.y, PI / 16);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        MotionMove(em, 0);
        if (em->Motion.Seq_frame > 11.7f && em->Motion.Seq_frame < 12.3f) {
            EstSet(0, -1, &em->pos, 0, EFF_PL00, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(5, 5, &em->pos, 0, 0, em);
        }
        if (((cPlayer*) em)->m_Work0) {
            ((cPlayer*) em)->m_Work0--;
        } else {
            EndPlDamage();
        }
        break;
    }
}

// Extra camera for the dive: behind/above the player, pulled in when the scenery blocks it.
void EscapeCamMove()
{
    Vec p0;
    Vec p1;
    Vec hit;
    Vec d;
    f32 len;
    GlobalWork* g = pG;

    Cam.param.fovy = g->Camera.param.fovy;
    p0.x = -376.0f;
    p0.y = 575.0f;
    p0.z = -1831.0f;
    p1.x = -244.0f;
    p1.y = 809.0f;
    p1.z = 52.6f;
    PSMTXMultVec(pPL->mat, &p0, &p0);
    PSMTXMultVec(pPL->mat, &p1, &p1);
    PosToPos(&g->Camera.param.at, &p1, &Cam.param.at, 1.0f);
    PosToPos(&g->Camera.param.pos, &p0, &Cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&Cam.param.at, &Cam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &Cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 875 "D:/Bio4/Prog/objPillar.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&Cam.param.at, &d, &Cam.param.pos);
    }
    {
        Camera* cam = &Cam;
        Vec* cp = &cam->param.pos;
        Vec* ca = &cam->param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Action button 0x25 during Throw: the player ducks (plemEscape2) and the pillar goes to Escape.
void EscapeAction2(cObjPillar* ptr)
{
    PillarWork* w = PILLAR_WK(ptr);

    if (!ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        w->Act_ck = 1;
        SetPlDamage((cEm*) ptr, plemEscape2);
        ptr->r_no_0 = 3;
        ptr->r_no_1 = 0;
        ptr->r_no_2 = 0;
        ptr->r_no_3 = 0;
    }
}

// Player duck routine: faces the pillar's start, plays the escape motion with effect and sounds,
// then EndPlDamage.
void plemEscape2(cPlayer* pEm)
{
    cEm* em = (cEm*) pEm;
    cObjPillar* obj = (cObjPillar*) em->pEmCatch;
    PillarWork* w = PILLAR_WK(obj);
    u8 step;

    em->dmg.m_Timer = 2;
    step = em->r_no_2;
    switch (step) {
    case 0:
        em->ang.y = GetXZAngle(&em->pos, &w->St_pos);
        MotionSetCore(em, &em->Motion, w->Mot_pl_escape, w->Seq_pl_escape, 0, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM31, 0x39, 0, ESP_CORE_KIND_NONE, em, (void*) step);
        SndCall(1, 0x48, &em->pos, 0, 0, em);
        SndCall(1, 0x11, &em->getPartsPtr(4)->world, 0, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EndPlDamage();
        }
        break;
    }
}

// Places the pillar's eat collision quad (created on first use) at its base.
void objPillarEatSet(cObjPillar* pObj)
{
    PillarWork* w = PILLAR_WK(pObj);
    Vec poly[4];
    f32 r = 400.0f;
    f32 h = 5000.0f;

    if (w->pEat == 0) {
        poly[0].x = -r;
        poly[0].y = 0.0f;
        poly[0].z = -r;
        poly[1].x = r;
        poly[1].y = 0.0f;
        poly[1].z = -r;
        poly[2].x = r;
        poly[2].y = 0.0f;
        poly[2].z = r;
        poly[3].x = -r;
        poly[3].y = 0.0f;
        poly[3].z = r;
        w->pEat = EatMgr.create(&pObj->pos, &pObj->ang, poly, h, 0, 0);
    } else {
        w->pEat->setEnable();
        w->pEat->setCoord(&pObj->pos, &pObj->ang);
    }
}
