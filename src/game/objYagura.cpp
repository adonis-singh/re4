// game/objYagura: the ladder object (yagura = tower/scaffold) placed by the room scripts
// (SetYagura, ObjMgr id 0x39): a static model with a pass-through collision box whose only
// behaviour is the vibration motion the room hands it (setMotionVib / setVib) when the player
// climbs or kicks it; the climbing itself is the player's ladder routine (pl_R1_Ladder).
#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "objYagura.h"
#include "global.h"
#include "motion.h"

extern "C" {
void objYagura_R0_Set(cObjYagura* obj);
}

void (*ObjYagura_R0_move_tbl[1])(cObjYagura*) = { objYagura_R0_Set };

// Creates the ladder object (ObjMgr id 0x39) at pos / rot with a 700 x 1000 pass-through collision
// box and a 5000-unit light; called by the room scenarios. Returns 0 when creation fails.
cObj* SetYagura(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    YaguraWork* w;

    obj = ObjMgr.create(cObjMgr::ID_YAGURA);
    if (obj == 0) {
        return 0;
    }
    w = YAGURA_WK((cObjYagura*) obj);
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->sub2B4.atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->sub2B4.atari.throughOn();
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
    w->Mot_vib = 0;
    obj->r_no_0 = 0;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    return obj;
}

// Per frame: the single routine (ObjYagura_R0_move_tbl[r_no_0]).
void cObjYagura::move()
{
    ObjYagura_R0_move_tbl[r_no_0](this);
}

// r_no_0 == 0 (the only routine): plays the vibration motion to its end, else just updates matrices.
void objYagura_R0_Set(cObjYagura* pObj)
{
    if (pObj->Motion.pMot) {
        if (MotionMove(pObj, 0)) {
            pObj->Motion.pMot = 0;
        }
    } else {
        pObj->matUpdate();
    }
}

// Remembers the motion setVib() plays (room scenario sets it from its archive).
void cObjYagura::setMotionVib(void* mot)
{
    YAGURA_WK(this)->Mot_vib = mot;
}

// Starts the vibration motion (the ladder shakes when the player climbs / kicks it).
void cObjYagura::setVib()
{
    if (YAGURA_WK(this)->Mot_vib) {
        MotionSetCore(this, &Motion, YAGURA_WK(this)->Mot_vib, 0, 0, 0, 0);
    }
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
