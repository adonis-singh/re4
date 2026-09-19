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

// Falling pillar (obj 0x1F): breaks (setBreak) or is thrown (setThrow) at the player, who can
// escape with the action button; the escape / die sequences run as player damage routines.
class cObjPillar : public cObj {
public:
    virtual void move();
    virtual ~cObjPillar() {}

    void setMotion(void* mot);
    int ckSet();
    void setBreak(Vec* pos, void* mot, int a);
    void setThrow(void* mot0, void* mot1, void* motEscape, void* plMot, int a);
    void setFall(void* mot0, void* mot1);
};

extern "C" {
int MotionMove(cModel* m, int a);
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
void GameAddPoint(int no);   // game/game.cpp
int EmAtkHitCk(void* atk, Vec* pos, Vec* oldPos, int flag);   // em_sub.cpp (obj08/obj12 declare it the same way)
}
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);
// The original is a `static plemEscape` (emBar.cpp has a global one); the name carries the split's
// address suffix so the report can pair it with the local symbol.
#define plemEscape plemEscape_8003C33C
extern "C" {
static void plemEscape(cPlayer* pl);

// struct view of pPL: the load stays below the preceding member stores (see cam_ctrl.cpp)
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
}

void (*ObjPillar_R0_move_tbl[5])(cObjPillar*) = {
    objPillar_R0_Set, objPillar_R0_Break, objPillar_R0_Throw, objPillar_R0_Escape, objPillar_R0_Fall,
};

EmAtkInfo ObjPillar_atk_info = { 1000.0f, 8, 1000, 0, 10, 0 };

Camera Cam;   // escape sequence camera

// Creates a pillar (id 0x1F) at pos/rot: capsule collision 400 x 5000, no suspend, no eat yet.
cObj* SetPillar(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    PillarWork* w;

    obj = ObjMgr.create(0x1F);
    if (obj == 0) {
        return 0;
    }
    w = &obj->pillar;
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
    AtariInit(&obj->sub2B4.atari, 0.0f, 0.0f, 0.0f, 400.0f, 400.0f, 400.0f, 5000.0f, 0, 2, 0);
    obj->sub2B4.atari.clrFlag100();
    w->plMot = 0;
    w->plMotA = 0;
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
    if (pillar.pEat) {
        pillar.pEat->m_Flag &= ~4;
    }
    ObjPillar_R0_move_tbl[r_no_0](this);
}

// Rno0 == 0: standing (Be_flg 1): matrices and the eat collision quad placed.
void objPillar_R0_Set(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;

    w->Be_flg |= 1;
    w->St_pos = obj->pos;
    obj->matUpdate();
    obj->partsWorldCalc();
    objPillarEatSet(obj);
}

// Rno0 == 1 (setBreak): the pillar topples with motBreak towards the player: creak sound when he
// is near, crushing hit tests on parts 1/2 (objPillarAtkCk), the escape action button (0x25)
// offered while he stands in front, fade-out 10 frames before the end; removed when Scenario_flg[1]
// 0x200 (the boss died).
void objPillar_R0_Break(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    Mtx inv;
    Vec v;
    u8 step = obj->r_no_2;

    switch (step) {
    case 0:
        w->Timer = (*(u16*) w->motBreak & 0x3FFF) - 10;
        MotionSetCore(obj, &obj->pMotion, w->motBreak, 0, 0, 0x8001, 0);
        w->rnd = Rnd() & 1;
        w->Act_ck = step;
        w->Seid = step;
        obj->r_no_2++;
    case 1:
        if (MotionMove(obj, 0)) {
            obj->be_flag &= ~2;
            obj->r_no_2++;
        } else {
            objPillarAtkCk(obj, &obj->getPartsPtr(1)->world);
            objPillarAtkCk(obj, &obj->getPartsPtr(2)->world);
            if (w->Seid == 0) {
                cModel* parts = obj->getPartsPtr(1);

                if ((parts->world.x - pPL->pos.x) * (parts->world.x - pPL->pos.x) +
                    (parts->world.y - pPL->pos.y) * (parts->world.y - pPL->pos.y) +
                    (parts->world.z - pPL->pos.z) * (parts->world.z - pPL->pos.z) < 16000000.0f) {
                    w->Seid = SndCall(8, 0x2C, &parts->world, 0x31, 0, obj);
                }
            }
            if (w->Timer) {
                w->Timer--;
            } else {
                obj->invisible_factor -= 0.1f;
                if (obj->invisible_factor < 0.0f) {
                    obj->invisible_factor = 0.0f;
                    obj->be_flag &= ~2;
                }
            }
        }
        break;
    }
    obj->partsWorldCalc();
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        ObjMgr.destroy(obj);
        return;
    }
    step = w->Act_ck;
    if (step == 0) {
        PSMTXInverse(obj->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &v);
        if (v.x > -2000.0f && v.x < 2000.0f && v.z > -1000.0f) {
            if (w->rnd) {
                ActBtn.set(0x25, 0xB, (int) EscapeAction, (int) obj, 1, 3, 0, 0);
            } else {
                ActBtn.set(0x25, 0xB, (int) EscapeAction, (int) obj, 1, 4, 0, 0);
            }
        }
    }
}

// Rno0 == 2 (setThrow): lifted (motThrow0 from frame 31, dust effect), then flies at 500 units/frame
// towards the player (motThrow1, trail effect) for 90 frames with crushing hit tests along its
// length; the escape button is offered once it is within 1000 units.
void objPillar_R0_Throw(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    Vec d;
    Vec v;
    Mtx m;
    cModel* parts;
    f32 len;
    u8 step = obj->r_no_2;
    u8 esc;

    switch (step) {
    case 0:
        MotionSetCore(obj, &obj->pMotion, w->motThrow0, 0, 0, 0x8001, 0x1F);
        w->rnd = Rnd() & 1;
        w->Act_ck = 1;
        EstSet((int) obj, -1, 0, 0, 0x29, 0x22, 0, 0, (u32) obj, (void*) step);
        w->Seid = step;
        obj->r_no_2++;
    case 1:
        if (MotionMove(obj, 0)) {
            obj->r_no_2++;
        }
        break;
    case 2:
        parts = obj->getPartsPtr(0);
        obj->pos = parts->world;
        w->Spd.x = 0.0f;
        w->Spd.y = 0.0f;
        w->Spd.z = 500.0f;
        v = pPLS->pos;
        v.y += 1000.0f;
        PSVECSubtract(&v, &obj->pos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z) / 500.0f;
        if (len == 0.0f) {
            w->Spd.y = 0.0f;
        } else {
            w->Spd.y = d.y / len;
        }
        PSMTXMultVecSR(obj->mat, &w->Spd, &w->Spd);
        MotionSetCore(obj, &obj->pMotion, w->motThrow1, 0, 0, 0x8005, 0);
        w->rnd = Rnd() & 1;
        w->Act_ck = 0;
        w->Timer = 90;
        EstSet((int) obj, -1, 0, 0, 0x29, 0x23, 1, 0x40, (u32) obj, 0);
        obj->r_no_2++;
    case 3:
        PSVECAdd(&obj->pos, &w->Spd, &obj->pos);
        MotionMove(obj, 0);
        if (w->Seid == 0) {
            parts = obj->getPartsPtr(0);
            len = (parts->world.x - pPL->pos.x) * (parts->world.x - pPL->pos.x) +
                  (parts->world.y - pPL->pos.y) * (parts->world.y - pPL->pos.y) +
                  (parts->world.z - pPL->pos.z) * (parts->world.z - pPL->pos.z);
            if (len < 16000000.0f) {
                w->Seid = SndCall(8, 0x2C, &parts->world, 0x31, 0, obj);
            }
        }
        if (w->Timer == 0) {
            obj->invisible_factor -= 0.1f;
            if (obj->invisible_factor < 0.0f) {
                EffectEspDelete(1, 0x40, (u32) obj, 0);
                EffectEspgenDelete(1, 0x40, (int) obj);
                EffectEfmDelete(1, 0x40, (int) obj);
                obj->invisible_factor = 0.0f;
                ObjMgr.destroy(obj);
                return;
            }
        } else {
            w->Timer--;
        }
        parts = obj->getPartsPtr(0);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 2000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 3000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -1000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -2000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -3000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        break;
    }
    obj->partsWorldCalc();
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        ObjMgr.destroy(obj);
        return;
    }
    esc = w->Act_ck;
    if (esc == 0) {
        PSMTXRotRad(m, 'y', GetXZAngle(&obj->pos_old, &obj->pos));
        TransMatrix(m, &obj->pos);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &pPL->pos, &d);
        if (d.z < 1000.0f) {
            w->Act_ck = 1;
        }
        if (w->rnd) {
            ActBtn.set(0x25, 0xB, (int) EscapeAction2, (int) obj, 1, 3, 0, 0);
        } else {
            ActBtn.set(0x25, 0xB, (int) EscapeAction2, (int) obj, 1, 4, 0, 0);
        }
    }
}

// Rno0 == 3 (escape succeeded): the pillar plays Mot_escape from the player's position/heading
// (it passes over him), then is removed.
void objPillar_R0_Escape(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;

    switch (obj->r_no_2) {
    case 0:
        memcpy((u8*) obj + ((u32) &((cObj*) 0)->pos), &pPL->pos, sizeof(Vec));
        memcpy((u8*) obj + ((u32) &((cObj*) 0)->ang), &pPL->ang, sizeof(Vec));
        MotionSetCore(obj, &obj->pMotion, w->Mot_escape, 0, 0, 0x8001, 0);
        SndStop(w->Seid, 0);
        w->Seid = SndCall(8, 0x2C, &obj->getPartsPtr(0)->world, 0x31, 0, obj);
        obj->r_no_2++;
    case 1:
        if (MotionMove(obj, 0)) {
            EffectEspDelete(1, 0x40, (u32) obj, 0);
            EffectEspgenDelete(1, 0x40, (int) obj);
            EffectEfmDelete(1, 0x40, (int) obj);
            obj->invisible_factor = 0.0f;
            ObjMgr.destroy(obj);
            return;
        }
        break;
    }
    obj->partsWorldCalc();
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        ObjMgr.destroy(obj);
    }
}

// Rno0 == 4 (setFall): drops from parts 0's position (gravity 15/frame), lands on the floor with
// motFall1 and a sound, fades out after 30 frames.
void objPillar_R0_Fall(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    f32 floor;

    switch (obj->r_no_2) {
    case 0:
        MotionSetCore(obj, &obj->pMotion, w->motFall0, 0, 0, 0x8004, 0);
        w->Spd.x = 0.0f;
        w->Spd.y = -100.0f;
        w->Spd.z = 0.0f;
        obj->r_no_2++;
    case 1:
        PSVECAdd(&obj->pos, &w->Spd, &obj->pos);
        w->Spd.y -= 15.0f;
        floor = EatMgr.getFloor(&obj->pos_old, 600.0f, 100000.0f, 0, 0);
        if (obj->pos.y < floor) {
            obj->pos.y = floor;
            SndCall(8, 0x26, &obj->pos, 0x31, 0, obj);
            MotionSetCore(obj, &obj->pMotion, w->motFall1, 0, 0, 0x8001, 0);
            MotionMove(obj, 0);
            obj->r_no_2++;
        } else {
            MotionMove(obj, 0);
        }
        break;
    case 2:
        obj->r_no_2++;
        w->Timer = 30;
    case 3:
        MotionMove(obj, 0);
        if (w->Timer == 0) {
            obj->invisible_factor -= 0.1f;
            if (obj->invisible_factor < 0.0f) {
                EffectEspDelete(1, 0x40, (u32) obj, 0);
                EffectEspgenDelete(1, 0x40, (int) obj);
                EffectEfmDelete(1, 0x40, (int) obj);
                obj->invisible_factor = 0.0f;
                ObjMgr.destroy(obj);
                return;
            }
        } else {
            w->Timer--;
        }
        break;
    }
    obj->partsWorldCalc();
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        ObjMgr.destroy(obj);
    }
}

// Sets the break motion.
void cObjPillar::setMotion(void* mot)
{
    pillar.motBreak = mot;
}

// 1 while the pillar is still standing.
int cObjPillar::ckSet()
{
    if (pillar.Be_flg & 1) {
        return 1;
    }
    return 0;
}

// Topples the pillar away from `pos` (towards the player when he is in front) with the player's
// escape motion mot/a; collision off.
void cObjPillar::setBreak(Vec* pos, void* mot, int a)
{
    PillarWork* w = &pillar;

    BitOff(w->Be_flg, 1);
    FSet(ang.y, GetXZAngle(pos, &this->pos));
    if (fabsf(Muku(&this->pos, &pPL->pos, ang.y, PI)) < PI / 2) {
        ang.y = GetXZAngle(&this->pos, &pPL->pos);
    }
    w->plMot = mot;
    w->plMotA = a;
    w->Break_pos = *pos;
    sub2B4.atari.throughOn();
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// The boss throws the pillar: lift/throw motions, the pillar's escape motion and the player's
// escape motion; aimed at the player when within 30 degrees.
void cObjPillar::setThrow(void* mot0, void* mot1, void* motEscape, void* plMot, int a)
{
    PillarWork* w = &pillar;

    BitOff(w->Be_flg, 1);
    if (fabsf(Muku(&pos, &pPL->pos, ang.y, PI)) < 0.5235988f) {
        ang.y = GetXZAngle(&pos, &pPL->pos);
    }
    w->motThrow0 = mot0;
    w->motThrow1 = mot1;
    w->Mot_escape = motEscape;
    w->plMot = plMot;
    w->plMotA = a;
    sub2B4.atari.throughOn();
    r_no_0 = 2;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Drops the pillar (fall / land motions).
void cObjPillar::setFall(void* mot0, void* mot1)
{
    PillarWork* w = &pillar;

    BitOff(w->Be_flg, 1);
    pos = getPartsPtr(0)->world;
    w->motFall0 = mot0;
    w->motFall1 = mot1;
    sub2B4.atari.throughOn();
    r_no_0 = 4;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Crushing hit test at pos with ObjPillar_atk_info (fatal flag 4 only when the player has more than
// 500 life): on the player damage type 8, blood, death effects when killed, quake, sound,
// vibration; on the partner quake + vibration.
void objPillarAtkCk(cObjPillar* obj, Vec* pos)
{
    PillarWork* w = &obj->pillar;
    EmAtkInfo* atk = &ObjPillar_atk_info;
    int hit;

    if ((s16) pG->pl_life > 500) {
        atk->flag |= 4;
    } else {
        atk->flag &= ~4;
    }
    hit = EmAtkHitCk(atk, pos, &w->St_pos, 1);
    if (hit) {
        if (hit & 1) {
            pPL->ang.y = GetXZAngle(&pPL->pos, &w->St_pos);
            PlSetDamage(8, 0, 0);
            EmPlBloodSet2(obj, pos, 1, 0x29, 0x3D);
            if ((s16) pG->pl_life <= 0) {
                EstSet((int) pPL, -1, 0, 0, 0x29, 0x3A, 0, 0, (u32) pPL, 0);
            } else {
                EstSet((int) pPL, -1, 0, 0, 0x29, 0x3B, 0, 0, (u32) pPL, 0);
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            SndCall(8, 0x25, &pPL->pos, 0x31, 0, pPL);
            w->Act_ck = 1;
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        if (hit & 2) {
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
    }
}

// Action button 0x25 during Break: the player dives out of the way (plemEscape), difficulty points.
void EscapeAction(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    u8 one = 1;

    if (!ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        w->Act_ck = one;
        SetPlDamage((cEm*) obj, plemEscape);
        GameAddPoint(9);
    }
}

// Player escape routine: turns towards the fall point, plays the dive motion (mirrored when the
// pillar comes from the left) with a custom camera (EscapeCamMove), dust and sounds, then
// EndPlDamage.
static void plemEscape(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjPillar* obj = (cObjPillar*) em->pEmCatch;
    PillarWork* w = &obj->pillar;
    f32 ang;

    em->dmg.m_Timer = 2;
    switch (em->r_no_2) {
    case 0:
        // the dead 0.0f store creates the pool `lis` in this block before the call, so
        // update_equiv_regs leaves it there (callee-saved) instead of moving it after Muku
        ang = 0.0f;
        ang = Muku(&em->pos, &w->Break_pos, em->ang.y, PI);
        if (ang < 0.0f) {
            MotionSetCore(em, &em->pMotion, w->plMot, w->plMotA, 3, 0x41, 0);
        } else {
            MotionSetCore(em, &em->pMotion, w->plMot, w->plMotA, 3, 1, 0);
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
        if (em->frame > 11.7f && em->frame < 12.3f) {
            EstSet(0, -1, &em->pos, 0, 3, 0x13, 0, 0, 0, 0);
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

    Cam.param.fovy = g->Cam.param.fovy;
    p0.x = -376.0f;
    p0.y = 575.0f;
    p0.z = -1831.0f;
    p1.x = -244.0f;
    p1.y = 809.0f;
    p1.z = 52.6f;
    PSMTXMultVec(pPL->mat, &p0, &p0);
    PSMTXMultVec(pPL->mat, &p1, &p1);
    PosToPos(&g->Cam.param.at, &p1, &Cam.param.at, 1.0f);
    PosToPos(&g->Cam.param.pos, &p0, &Cam.param.pos, 1.0f);
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
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.m_pExtraCamera = (s32) cam;
    }
}

// Action button 0x25 during Throw: the player ducks (plemEscape2) and the pillar goes to Escape.
void EscapeAction2(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;

    if (!ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        w->Act_ck = 1;
        SetPlDamage((cEm*) obj, plemEscape2);
        obj->r_no_0 = 3;
        obj->r_no_1 = 0;
        obj->r_no_2 = 0;
        obj->r_no_3 = 0;
    }
}

// Player duck routine: faces the pillar's start, plays the escape motion with effect and sounds,
// then EndPlDamage.
void plemEscape2(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjPillar* obj = (cObjPillar*) em->pEmCatch;
    PillarWork* w = &obj->pillar;
    u8 step;

    em->dmg.m_Timer = 2;
    step = em->r_no_2;
    switch (step) {
    case 0:
        em->ang.y = GetXZAngle(&em->pos, &w->St_pos);
        MotionSetCore(em, &em->pMotion, w->plMot, w->plMotA, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x39, 0, 0, (u32) em, (void*) step);
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
void objPillarEatSet(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
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
        w->pEat = EatMgr.create(&obj->pos, &obj->ang, poly, 0, 0, h);
    } else {
        w->pEat->m_Flag |= 4;
        w->pEat->setCoord(&obj->pos, &obj->ang);
    }
}
