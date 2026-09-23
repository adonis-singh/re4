// game/obj16: object id 0x16, the plaga head (D:/Bio4/Prog/obj16.cpp): the parasite that bursts
// from a Ganado's neck, hung on parts `parts_no` of its `body` model and owned by `target` (the
// enemy work). `type` selects the plaga kind (2/0xB tentacle, 3/0xD head-biter, 4 spider ...).
// R1 routines: 0 Set (plain motion), 1 CoreMove (idle / spit attack cycle), 2 Atk (bite), 3
// Critical (the decapitating bite), 4 Damage. It tracks the player with the neck (obj16NeckMove),
// spawns its drip/glow effects, and shrinks/fades away (Lost_wait) once the body dies.
#include "atari.h"
#include "light.h"
#include "ctrl.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "esp.h"
#include "est.h"
#include "global.h"
#include "main.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pad.h"
#include "quake.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "motion.h"
#include "em_sub.h"

// Enemy head (obj 0x16): the head / mouth model of the plaga-carrying enemies, hung on a parts of
// its body (`o16.body`). It turns toward the player (obj16NeckMove), bites (R1_Atk, R1_Critical),
// takes damage motions (R1_Damage) and fades out once its enemies are dead.
class cObj16 : public cObjUnion {
public:
    virtual void move();
    virtual ~cObj16() {}

    void setScale(Vec* s);
    void setDieEff();
    void clearLostWait();
    void setLostWait(int n);
    void setMotData(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8,
                    void* m9, void* m10);
    void setPlDmgMot(void* mot, void* seq);
    void setAtk(u8 flag);
    void setCritical();
    void setDamage();
    int ckAtkEnable();
    void setBurn();
    int ckAtkHit();
};

// Model part as obj16NeckMove writes it: the parts rotation and the override flag.
struct Obj16Parts {
    u8 pad_0[0x128];
    Vec rot;        // 0x128
    u8 pad_134[0x1C0 - 0x134];
    u32 flags;      // 0x1C0  bit30: rotation override
};

extern "C" {
cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot);
void obj16_R1_Set(cObj16* obj);
void obj16_R1_CoreMove(cObj16* obj);
void obj16_R1_Atk(cObj16* obj);
void obj16_R1_Critical(cObj16* obj);
void obj16_R1_Damage(cObj16* obj);
void MotSetObj16(cObj* obj, void* mot, int a, int b);
void obj16MatCalc(cObj16* obj);
int obj16AtkCk(cObj16* obj, u32 kind, int partsNo);
void obj16PlHeadLost(cObj16* obj);
static void obj16NeckMove(cObj16* obj);
void plemDmMStar(cPlayer* pl);
}

void (*Obj16_R1_move_tbl[5])(cObj16*) = {
    obj16_R1_Set, obj16_R1_CoreMove, obj16_R1_Atk, obj16_R1_Critical, obj16_R1_Damage,
};

// Attack parameters per obj16AtkCk kind (EmAtkHitCk).
EmAtkInfo obj16_atk_info[4] = {
    { 500.0f, PL_DM_AUTO, 0x320, 0, 0xA, 0 },
    { 500.0f, PL_DM_AUTO, 0x320, 0, 0xA, 0 },
    { 500.0f, PL_DM_AUTO, 0x1F4, 0, 0xA, 0 },
    { 800.0f, PL_DM_AUTO, 0x270F, 0, 0xA, 0 },
};

// Creates the head on parts partsNo of `body` for enemy `target`, scale growing from 0 to Scale,
// effect owner kinds 0x3D/0x3E, appear timer 60, Lost_wait 150. 0 when target/body have no parts.
cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot)
{
    cObj* obj;
    Obj16Work* w;

    if (target == 0) {
        return 0;
    }
    if (target->pParts == 0) {
        return 0;
    }
    if (body == 0) {
        return 0;
    }
    if (body->pParts == 0) {
        return 0;
    }
    obj = ObjMgr.createBack(cObjMgr::ID_EM10_PARASITE);
    if (obj == 0) {
        return 0;
    }
    w = &((cObj16*) obj)->o16;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj16() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 2000.0f, 2000.0f, 2000.0f };

    obj->sub2B4.atari.throughOn();
    obj->LightInfo.init2(0, 1, &p0, &p1, 2);
    obj->type = type;
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    if (rot) {
        obj->ang = *rot;
    } else {
        obj->ang.x = 0.0f;
        obj->ang.y = 0.0f;
        obj->ang.z = 0.0f;
    }
    obj->pos_old = obj->pos;
    obj->scale.x = 0.0f;
    obj->scale.y = 0.0f;
    obj->scale.z = 0.0f;
    w->Scale.x = 1.0f;
    w->Scale.y = 1.0f;
    w->Scale.z = 1.0f;
    w->pCtrlGroup = GetCtrlCtrl12();
    w->body = body;
    w->parts_no = partsNo;
    w->target = target;
    w->Se_wait = 60;
    w->Seid = 0;
    w->Eff_wait = 0;
    w->EffKindId = 0x3D;
    w->EffKindId2 = 0x3E;
    w->Lost_wait = 150;
    w->Eff_wait2 = (Rnd() & 3) + 9;
    w->Neck_dir = 0.0f;
    w->Appear_timer = 60;
    w->Eff_wait3 = 0;
    w->mot[0] = 0;
    w->mot[1] = 0;
    w->mot[2] = 0;
    w->mot[4] = 0;
    w->mot[5] = 0;
    w->mot[6] = 0;
    w->mot[7] = 0;
    w->mot[8] = 0;
    w->mot[9] = 0;
    w->mot[10] = 0;
    w->Mot_pl_dm = 0;
    w->Seq_pl_dm = 0;
    w->mot[3] = 0;
    w->x6C = 0;
    w->Atk_wait = 0;
    w->Atk_timer = 0;
    w->Wait_mode = 0;
    w->Atk_ck = 0;
    obj16MatCalc((cObj16*) obj);
    w->Wait_mno = 0;
    obj->r_no_0 = 1;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    if (target->be_flag & 0x800) {
        obj->setNoSuspend(1);
    } else {
        obj->setNoSuspend(0);
    }
    return obj;
}

// Delete the effects the head owns and the head itself.
#define OBJ16_LOST(obj, w)                                    \
    EffectEspDelete(0, (w)->EffKindId, obj, 0);         \
    EffectEspgenDelete(0, (w)->EffKindId, obj);         \
    EffectEfmDelete(0, (w)->EffKindId, obj);            \
    EffectEspDelete(0, (w)->EffKindId2, obj, 0);        \
    EffectEspgenDelete(0, (w)->EffKindId2, obj);        \
    EffectEfmDelete(0, (w)->EffKindId2, obj);           \
    ObjMgr.destroy(obj)

// Per-frame: dies with target/body; fades and shrinks when the target is dead or Lost_wait ran out
// (deleting its effects); eases scale to Scale (half size while the body's x39D flag); runs the R1
// routine; the looping plaga sound and the periodic drip/glow effects per type (thermal mode swaps
// the light class); follows the target's no-suspend flag.
void cObj16::move()
{
    Obj16Work* w = &o16;
    int alive;
    const f32 decRate = 0.9f;
    const f32 addRate = 0.1f;

    if (w->target && (w->target->be_flag & 0x201) != 1) {
        OBJ16_LOST(this, w);
        return;
    }
    if (w->body && (w->body->be_flag & 0x201) != 1) {
        OBJ16_LOST(this, w);
        return;
    }
    if (w->At_hit_wait) {
        w->At_hit_wait--;
    }
    switch (type) {
    case 8:
    case 9:
    case 0xA:
    case 0x11:
        break;
    default:
        if (w->Be_flag & 1) {
            if (w->Lost_wait) {
                w->Lost_wait--;
            }
        }
        break;
    }
    if (w->target) {
        switch (type) {
        case 6:
        case 8:
        case 9:
        case 0xA:
        case 0x11:
            break;
        default:
            if (!(w->Be_flag & 1)) {
                if (((cEm*) w->target)->hp > 0) {
                    w->Lost_wait = 90;
                }
            }
            break;
        }
        alive = 0;
        if (w->Lost_wait) {
            alive = 1;
        }
        switch (type) {
        case 6:
        case 8:
        case 9:
        case 0xA:
        case 0x11:
            break;
        default:
            if (!(w->Be_flag & 1)) {
                if (((cEm*) w->target)->hp > 0) {
                    alive = 1;
                }
            }
            break;
        }
        if (alive == 0) {
            scale.y = scale.z = scale.x = scale.x * decRate;
            invisible_factor *= 0.85f;
            if (invisible_factor <= 0.01f) {
                invisible_factor = 0.0f;
                OBJ16_LOST(this, w);
                return;
            }
        } else {
            scale.x = scale.x * decRate + w->Scale.x * addRate;
            scale.y = scale.y * decRate + w->Scale.y * addRate;
            scale.z = scale.z * decRate + w->Scale.z * addRate;
        }
    }
    if (w->Appear_timer) {
        w->Appear_timer--;
    }
    if (w->body && type == 1) {
        if (((cEm*) w->body)->x39D) {
            w->Scale.x = 0.5f;
            w->Scale.y = 0.5f;
            w->Scale.z = 0.5f;
        } else {
            w->Scale.x = 1.0f;
            w->Scale.y = 1.0f;
            w->Scale.z = 1.0f;
        }
    }
    w->Atk_enable = 0;
    Obj16_R1_move_tbl[r_no_1](this);
    if (w->target) {
        switch (type) {
        case 2:
        case 3:
            if (((cEm*) w->target)->hp > 0) {
                if (w->Se_wait) {
                    w->Se_wait--;
                } else {
                    w->Se_wait = 29;
                    w->Seid = SndCall(8, 0x13, &w->target->pos, w->target->id, 0, 0);
                }
            } else {
                SndStop(w->Seid, 0);
            }
            break;
        }
        if (w->target) {
            switch (type) {
            case 2:
            case 3:
                if (w->Eff_wait) {
                    if (--w->Eff_wait == 0) {
                        w->Eff_wait = 2;
                        if (be_flag & 0x800) {
                            EstSet(this, -1, 0, 0, EFF_EM10, 0x2E, 1, ESP_CORE_KIND_NONE, this, 0);
                        } else {
                            EstSet(this, -1, 0, 0, EFF_EM10, 0x2E, 0, ESP_CORE_KIND_NONE, this, 0);
                        }
                    }
                }
                break;
            case 0xB:
            case 0xD:
                if (w->Eff_wait) {
                    if (--w->Eff_wait == 0) {
                        w->Eff_wait = 2;
                        if (be_flag & 0x800) {
                            EstSet(this, -1, 0, 0, EFF_EM3C, 0x16, 1, ESP_CORE_KIND_NONE, this, 0);
                        } else {
                            EstSet(this, -1, 0, 0, EFF_EM3C, 0x16, 0, ESP_CORE_KIND_NONE, this, 0);
                        }
                    }
                }
                break;
            }
        }
    }
    switch (type) {
    case 1:
        if (w->Eff_wait2) {
            if (--w->Eff_wait2 == 0) {
                w->Eff_wait2 = (Rnd() & 3) + 15;
                if (be_flag & 0x800) {
                    EstSet(this, -1, 0, 0, EFF_EM10, 4, 1, ESP_CORE_KIND_NONE, this, 0);
                } else {
                    EstSet(this, -1, 0, 0, EFF_EM10, 4, 0, ESP_CORE_KIND_NONE, this, 0);
                }
            }
        }
        break;
    case 0xC:
        if (w->Eff_wait2) {
            if (--w->Eff_wait2 == 0) {
                w->Eff_wait2 = (Rnd() & 3) + 15;
                if (be_flag & 0x800) {
                    EstSet(this, -1, 0, 0, EFF_EM3C, 0x10, 1, ESP_CORE_KIND_NONE, this, 0);
                } else {
                    EstSet(this, -1, 0, 0, EFF_EM3C, 0x10, 0, ESP_CORE_KIND_NONE, this, 0);
                }
            }
        }
        break;
    case 2:
        if (w->Wait_mode) {
            if (w->Eff_wait2) {
                if (--w->Eff_wait2 == 0) {
                    w->Eff_wait2 = 3;
                    EstSet(this, -1, 0, 0, EFF_EM10, 0x52, 0, ESP_CORE_KIND_NONE, this, 0);
                }
            }
        }
        break;
    case 0xD:
        if (w->Wait_mode) {
            if (w->Eff_wait2) {
                if (--w->Eff_wait2 == 0) {
                    w->Eff_wait2 = 3;
                    EstSet(this, -1, 0, 0, EFF_EM3C, 0x18, 0, ESP_CORE_KIND_NONE, this, 0);
                }
            }
        }
        break;
    case 5:
        if (w->Eff_wait2) {
            if (--w->Eff_wait2 == 0) {
                w->Eff_wait2 = (Rnd() & 3) + 15;
                if (be_flag & 0x800) {
                    EstSet(this, -1, 0, 0, EFF_EM22, 0xE, 1, ESP_CORE_KIND_NONE, this, 0);
                } else {
                    EstSet(this, -1, 0, 0, EFF_EM22, 0xE, 0, ESP_CORE_KIND_NONE, this, 0);
                }
            }
        }
        break;
    case 6:
        if (w->Eff_wait2) {
            if (--w->Eff_wait2 == 0) {
                w->Eff_wait2 = (Rnd() & 3) + 9;
                if (be_flag & 0x800) {
                    EstSet(this, -1, 0, 0, EFF_EM22, 0xF, 1, ESP_CORE_KIND_NONE, this, 0);
                } else {
                    EstSet(this, -1, 0, 0, EFF_EM22, 0xF, 0, ESP_CORE_KIND_NONE, this, 0);
                }
            }
        }
        break;
    }
    if (w->target) {
        if (w->target->be_flag & 0x800) {
            setNoSuspend(1);
        } else {
            setNoSuspend(0);
        }
    }
    if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
        LightInfo.EnableMask = 4;
    } else {
        LightInfo.EnableMask = 2;
    }
}

// Rno1 == 0: plays the current motion and follows the body.
void obj16_R1_Set(cObj16* obj)
{
    if (obj->Motion.pMot) {
        MotionMove(obj, 0);
    }
    obj16MatCalc(obj);
}

// Rno1 == 1 (idle cycle): Rno2 0 starts the idle/wait motion (mot[0..2]) with a random timer, 1
// loops it deciding to attack (types 2/0xB spit when the player is near / far by random), 2 the
// attack wind-up (mot[10]) with its effects, 3 the spit (mot[9]) with the spit effects; obj16AtkCk
// kind 2 on parts 0x10..0x15 while spitting.
void obj16_R1_CoreMove(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    int atk;
    f32 dist;

    w->Atk_enable = 1;
    w->Atk_ck = 0;
    atk = 0;
    switch (obj->r_no_2) {
    case 0:
        if (w->Wait_mode) {
            MotionSetCore(obj, &obj->Motion, w->mot[2], 0, 3, 4, (u8) ((u32) Rnd() % 15));
        } else if (Rnd() & 1) {
            MotionSetCore(obj, &obj->Motion, w->mot[0], 0, 3, 4, 0);
        } else {
            MotionSetCore(obj, &obj->Motion, w->mot[1], 0, 3, 4, 0);
        }
        w->Timer = (u8) ((u32) Rnd() % 5) + 15;
        obj->r_no_2++;
    case 1:
        if (MotionMove(obj, 0)) {
            if (obj->type == 4) {
                break;
            }
            if (obj->type == 3 || obj->type == 0xD) {
                obj->r_no_2 = 0;
                break;
            }
            if (obj->type == 2 || obj->type == 0xB || obj->type == 0xE) {
                cModel* p = obj->getPartsPtr(0);
                dist = (p->world.x - pPL->pos.x) * (p->world.x - pPL->pos.x) +
                       (p->world.y - pPL->pos.y) * (p->world.y - pPL->pos.y) +
                       (p->world.z - pPL->pos.z) * (p->world.z - pPL->pos.z);
                if (w->Wait_mode) {
                    if ((Rnd() & 3) == 0 && dist > 49000000.0f) {
                        obj->r_no_2 = 2;
                        break;
                    }
                } else {
                    if ((Rnd() & 3) == 0 || dist < 25000000.0f) {
                        obj->r_no_2 = 4;
                        break;
                    }
                }
            } else if ((Rnd() & 3) == 0) {
                obj->r_no_2 = 0;
                break;
            }
        }
        if (obj->type == 2 && w->Wait_mode) {
            atk = 1;
            if (w->Timer) {
                w->Timer--;
            } else {
                w->Timer = (u8) ((u32) Rnd() % 5) + 15;
                SndCall(8, 0x10, &w->target->pos, w->target->id, 0, 0);
            }
        }
        if (obj->type == 0xB && w->Wait_mode) {
            atk = 1;
            if (w->Timer) {
                w->Timer--;
            } else {
                w->Timer = (u8) ((u32) Rnd() % 5) + 15;
                SndCall(8, 0x21, &w->target->pos, w->target->id, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(obj, &obj->Motion, w->mot[10], 0, 3, 0, 0);
        if (obj->type == 2) {
            EstSet(obj, -1, 0, 0, EFF_EM10, 0x53, 0, ESP_CORE_KIND_NONE, obj, 0);
            EffectEspDelete(0, w->EffKindId2, obj, 0);
            EffectEspgenDelete(0, w->EffKindId2, obj);
            EffectEfmDelete(0, w->EffKindId2, obj);
        }
        if (obj->type == 0xB) {
            EstSet(obj, -1, 0, 0, EFF_EM3C, 0x19, 0, ESP_CORE_KIND_NONE, obj, 0);
            EffectEspDelete(0, w->EffKindId2, obj, 0);
            EffectEspgenDelete(0, w->EffKindId2, obj);
            EffectEfmDelete(0, w->EffKindId2, obj);
        }
        w->Timer = 36;
        obj->r_no_2++;
    case 3:
        if (w->Timer) {
            if (--w->Timer == 0) {
                w->Wait_mode = 0;
            }
        }
        if (MotionMove(obj, 0)) {
            w->Wait_mode = 0;
            obj->r_no_2 = 0;
        }
        break;
    case 4:
        MotionSetCore(obj, &obj->Motion, w->mot[9], 0, 3, 0, 0);
        if (obj->type == 2) {
            EstSet(obj, -1, 0, 0, EFF_EM10, 0xA, 0, ESP_CORE_KIND_NONE, obj, 0);
            EstSet(obj, -1, 0, 0, EFF_EM10, 0x51, 0, w->EffKindId2, obj, 0);
            w->Eff_wait2 = 3;
        }
        if (obj->type == 0xB) {
            EstSet(obj, -1, 0, 0, EFF_EM3C, 0xC, 0, ESP_CORE_KIND_NONE, obj, 0);
            EstSet(obj, -1, 0, 0, EFF_EM3C, 0x17, 0, w->EffKindId2, obj, 0);
            w->Eff_wait2 = 3;
        }
        w->Timer = 20;
        obj->r_no_2++;
    case 5:
        if (w->Timer) {
            if (--w->Timer == 0) {
                w->Wait_mode = 1;
            }
        }
        if (MotionMove(obj, 0)) {
            obj->r_no_2 = 0;
        }
        break;
    }
    obj16MatCalc(obj);
    if (atk) {
        obj16AtkCk(obj, 2, 0x10);
        obj16AtkCk(obj, 2, 0x11);
        obj16AtkCk(obj, 2, 0x12);
        obj16AtkCk(obj, 2, 0x13);
        obj16AtkCk(obj, 2, 0x14);
        obj16AtkCk(obj, 2, 0x15);
    }
}

// Rno1 == 2 (bite attack, setAtk): Rno2 0 spit motion (mot[9]), 1 one of two bite motions
// (mot[3]/mot[4], r_no_3 forces the second) with their effects and sound, 2 the bite window
// (atkTimer) with obj16AtkCk kind 0/1, 3 recovery; Atk_ck reports a hit to the body.
void obj16_R1_Atk(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    int atk;
    int flag;

    if (obj->r_no_2 == 0 && w->Wait_mode) {
        obj->r_no_2 = 2;
    }
    w->Atk_ck = 0;
    atk = 0;
    switch (obj->r_no_2) {
    case 0:
        MotionSetCore(obj, &obj->Motion, w->mot[9], 0, 0xA, 0, 0);
        if (obj->type == 2) {
            EstSet(obj, -1, 0, 0, EFF_EM10, 0xA, 0, ESP_CORE_KIND_NONE, obj, 0);
            EstSet(obj, -1, 0, 0, EFF_EM10, 0x51, 0, w->EffKindId2, obj, 0);
            w->Eff_wait2 = 3;
        }
        if (obj->type == 0xB) {
            EstSet(obj, -1, 0, 0, EFF_EM3C, 0xC, 0, ESP_CORE_KIND_NONE, obj, 0);
            EstSet(obj, -1, 0, 0, EFF_EM3C, 0x17, 0, w->EffKindId2, obj, 0);
            w->Eff_wait2 = 3;
        }
        obj->r_no_2++;
    case 1:
        if (MotionMove(obj, 0)) {
            w->Wait_mode = 1;
            obj->r_no_2++;
        }
        break;
    case 2:
        if ((Rnd() & 1) || obj->r_no_3) {
            MotionSetCore(obj, &obj->Motion, w->mot[3], 0, 0xA, 0, 0);
            w->Timer = 34;
            w->Timer2 = 8;
            obj->r_no_3 = 0;
            if (obj->type == 2) {
                EstSet(obj, -1, 0, 0, EFF_EM10, 0x6C, 0, ESP_CORE_KIND_NONE, obj, 0);
            }
            if (obj->type == 0xB) {
                EstSet(obj, -1, 0, 0, EFF_EM3C, 0x1C, 0, ESP_CORE_KIND_NONE, obj, 0);
            }
        } else {
            MotionSetCore(obj, &obj->Motion, w->mot[4], 0, 3, 0, 0);
            w->Timer = 28;
            w->Timer2 = 6;
            obj->r_no_3 = 1;
            if (obj->type == 2) {
                EstSet(obj, -1, 0, 0, EFF_EM10, 0x6F, 0, ESP_CORE_KIND_NONE, obj, 0);
            }
            if (obj->type == 0xB) {
                EstSet(obj, -1, 0, 0, EFF_EM3C, 0x1F, 0, ESP_CORE_KIND_NONE, obj, 0);
            }
        }
        if (obj->type == 2) {
            SndCall(8, 9, &w->target->pos, w->target->id, 0, 0);
        }
        if (obj->type == 0xB) {
            SndCall(8, 0x1E, &w->target->pos, w->target->id, 0, 0);
        }
        obj->r_no_2++;
    case 3:
        if (MotionMove(obj, 0)) {
            obj->r_no_0 = 1;
            obj->r_no_1 = 1;
            obj->r_no_2 = 0;
            obj->r_no_3 = 0;
        } else {
            if (w->Timer) {
                if (--w->Timer == 0) {
                    if (obj->type == 2) {
                        SndCall(8, 0xA, &w->target->pos, w->target->id, 0, 0);
                    }
                    if (obj->type == 0xB) {
                        SndCall(8, 0x1F, &w->target->pos, w->target->id, 0, 0);
                    }
                }
            } else {
                if (w->Timer2) {
                    w->Timer2--;
                    atk = 1;
                }
            }
            if (obj->Motion.Seq_frame > 44.7f && obj->Motion.Seq_frame < 45.3f) {
                if (obj->type == 2) {
                    SndCall(8, 0xF, &w->target->pos, w->target->id, 0, 0);
                }
                if (obj->type == 0xB) {
                    SndCall(8, 0x20, &w->target->pos, w->target->id, 0, 0);
                }
            }
        }
        break;
    }
    obj16MatCalc(obj);
    if (atk) {
        flag = 0;
        if (obj->r_no_3) {
            flag = 1;
        }
        obj16AtkCk(obj, flag, 0x10);
        obj16AtkCk(obj, flag, 0x11);
        obj16AtkCk(obj, flag, 0x12);
        obj16AtkCk(obj, flag, 0x13);
        obj16AtkCk(obj, flag, 0x14);
        obj16AtkCk(obj, flag, 0x15);
    }
}

// Rno1 == 3 (decapitation bite, types 3/0xD): Rno2 0 wind-up (mot[9]), 1 chooses the bite by the
// victim's head position (player or nearer partner: high mot[6], near mot[3], mid mot[4], far
// mot[5]), 2 plays it with sounds at frames 10 and 42 and obj16AtkCk kind 3 on parts 9 after
// Timer (kills the player outright through obj16PlHeadLost).
void obj16_R1_Critical(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    int atk;
    Vec head;
    Vec tgt;
    f32 d;
    cModel* p;

    w->Atk_ck = 0;
    atk = 0;
    switch (obj->r_no_2) {
    case 0:
        MotionSetCore(obj, &obj->Motion, w->mot[9], 0, 3, 0, 0);
        if (obj->type == 3) {
            EstSet(obj, -1, 0, 0, EFF_EM10, 0x5F, 0, ESP_CORE_KIND_NONE, obj, 0);
            SndCall(8, 9, &w->target->pos, w->target->id, 0, 0);
        }
        if (obj->type == 0xD) {
            EstSet(obj, -1, 0, 0, EFF_EM3C, 0x1B, 0, ESP_CORE_KIND_NONE, obj, 0);
            SndCall(8, 0x28, &w->target->pos, w->target->id, 0, 0);
        }
        w->Timer = 0;
        obj->r_no_2++;
    case 1:
        if (MotionMove(obj, 0)) {
            obj->r_no_2++;
        }
        break;
    case 2:
        head.x = obj->mat[0][3];
        head.y = obj->mat[1][3];
        head.z = obj->mat[2][3];
        // one `p` for both parts lookups: a pointer local assigned in two blocks is not
        // local-allocated, so the worldPos copy's address register is not tied to r3
        p = pPL->getPartsPtr(3);
        tgt = p->world;
        if (pSUB && w->body) {
            d = VEC_DIST(&w->body->pos, &pPL->pos);
            if (d > VEC_DIST(&w->body->pos, &pSUB->pos) +
                        3000.0f) {
                p = pSUB->getPartsPtr(3);
                tgt = p->world;
            }
        }
        // `w->timer = 14` repeated in every arm: the last arm's block then does not end in a
        // call (no flow nop), so all four tails cross-jump into one MotionSetCore
        if (head.y - tgt.y > 500.0f) {
            MotionSetCore(obj, &obj->Motion, w->mot[6], 0, 0, 0, 0);
            w->Timer = 14;
        } else {
            f32 d2 = (head.x - tgt.x) * (head.x - tgt.x) + (head.z - tgt.z) * (head.z - tgt.z);
            if (d2 < 1440000.0f) {
                MotionSetCore(obj, &obj->Motion, w->mot[3], 0, 0, 0, 0);
                w->Timer = 14;
            } else if (d2 < 3240000.0f) {
                MotionSetCore(obj, &obj->Motion, w->mot[4], 0, 0, 0, 0);
                w->Timer = 14;
            } else {
                MotionSetCore(obj, &obj->Motion, w->mot[5], 0, 0, 0, 0);
                w->Timer = 14;
            }
        }
        if (obj->type == 3) {
            EstSet(obj, -1, 0, 0, EFF_EM10, 0x5D, 0, ESP_CORE_KIND_NONE, obj, 0);
            SndCall(8, 0xA, &w->target->pos, w->target->id, 0, 0);
        }
        if (obj->type == 0xD) {
            EstSet(obj, -1, 0, 0, EFF_EM3C, 0xB, 0, ESP_CORE_KIND_NONE, obj, 0);
            SndCall(8, 0x29, &w->target->pos, w->target->id, 0, 0);
        }
        obj->r_no_2++;
    case 3:
        if (MotionMove(obj, 0)) {
            obj->r_no_0 = 1;
            obj->r_no_1 = 1;
            obj->r_no_2 = 0;
            obj->r_no_3 = 0;
        } else {
            if (w->Timer) {
                if (--w->Timer == 0) {
                    atk = 1;
                }
            }
            if (obj->Motion.Seq_frame > 9.7f && obj->Motion.Seq_frame < 10.3f) {
                if (obj->type == 3) {
                    SndCall(8, 0xAB, &w->target->pos, w->target->id, 0, 0);
                }
                if (obj->type == 0xD) {
                    SndCall(8, 0x2B, &w->target->pos, w->target->id, 0, 0);
                }
            }
            if (obj->Motion.Seq_frame > 41.7f && obj->Motion.Seq_frame < 42.3f) {
                if (obj->type == 3) {
                    SndCall(8, 0xF, &w->target->pos, w->target->id, 0, 0);
                }
                if (obj->type == 0xD) {
                    SndCall(8, 0x2A, &w->target->pos, w->target->id, 0, 0);
                }
            }
        }
        break;
    }
    obj16MatCalc(obj);
    if (atk) {
        if (obj16AtkCk(obj, 3, 9)) {
            if (obj->type == 3) {
                EstSet(obj, -1, 0, 0, EFF_EM10, 0x5E, 0, ESP_CORE_KIND_NONE, obj, 0);
            }
            if (obj->type == 0xD) {
                EstSet(obj, -1, 0, 0, EFF_EM3C, 0xE, 0, ESP_CORE_KIND_NONE, obj, 0);
            }
        }
    }
}

// Rno1 == 4 (hit reaction, setDamage): plays mot[7]/mot[8] (types 3/0xD shrink to 0.3 for 30
// frames), the pain sound loop for type 4, then back to the idle cycle (a live body may go
// straight to an attack for type 2).
void obj16_R1_Damage(cObj16* obj)
{
    Obj16Work* w = &obj->o16;

    switch (obj->r_no_2) {
    case 0:
        if (w->Wait_mode) {
            MotionSetCore(obj, &obj->Motion, w->mot[8], 0, 0, 0, 0);
        } else {
            MotionSetCore(obj, &obj->Motion, w->mot[7], 0, 0, 0, 0);
        }
        if (obj->type == 3 || obj->type == 0xD) {
            w->Scale.x = 0.3f;
            w->Scale.y = 0.3f;
            w->Scale.z = 0.3f;
        }
        if (w->target && (obj->type == 2 || obj->type == 3)) {
            SndCall(8, 0x88, &w->target->pos, w->target->id, 0, 0);
        }
        if (obj->type == 3) {
            if (w->body) {
                EstSet(w->body, -1, 0, 0, EFF_EM10, 0x86, 0, ESP_CORE_KIND_NONE, w->body, 0);
            }
        }
        w->Timer = 30;
        w->Timer2 = 0;
        obj->r_no_2++;
    case 1:
        if (obj->type == 3 || obj->type == 0xD) {
            if (w->Timer) {
                if (--w->Timer == 0) {
                    w->Scale.x = 1.0f;
                    w->Scale.y = 1.0f;
                    w->Scale.z = 1.0f;
                }
            }
        }
        if (obj->type == 4) {
            if (w->target) {
                if (w->Timer2) {
                    w->Timer2--;
                } else {
                    w->Timer2 = 29;
                    w->Seid = SndCall(8, 0x13, &w->target->pos, w->target->id, 0, 0);
                }
            }
        }
        if (MotionMove(obj, 0)) {
            if (obj->type == 4) {
                if (w->target) {
                    SndStop(w->Seid, 0);
                }
            }
            if (w->body && ((cEm*) w->body)->hp > 0 && (u8) ((u32) Rnd() % 10) > 5 && obj->type == 2) {
                obj->r_no_0 = 1;
                obj->r_no_1 = 2;
                obj->r_no_2 = 0;
                obj->r_no_3 = 0;
            } else {
                obj->r_no_0 = 1;
                obj->r_no_1 = 1;
                obj->r_no_2 = 0;
                obj->r_no_3 = 0;
            }
        }
        break;
    }
    obj16MatCalc(obj);
}

// Starts a motion on the head with attr a and frame b (10-frame blend).
void MotSetObj16(cObj* obj, void* mot, int a, int b)
{
    if (obj == 0) {
        return;
    }
    MotionSetCore(obj, &obj->Motion, mot, 0, 0xA, (u16) a, (u16) b);
}

// Places the head: its pos/ang/scale under the body parts' matrix (or free), then the parts with
// the neck tracking rotation applied.
void obj16MatCalc(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    cModel* p;

    if (w->body) {
        p = w->body->getPartsPtr(w->parts_no);
        RotMatrix(obj->mat, &obj->ang);
        TransMatrix(obj->mat, &obj->pos);
        ScaleMatrix(obj->mat, &obj->scale);
        PSMTXConcat(p->mat, obj->mat, obj->mat);
        obj->Motion.Mot_flag |= 0x40000000;
    } else {
        RotMatrix(obj->l_mat, &obj->ang);
        TransMatrix(obj->l_mat, &obj->pos);
        ScaleMatrix(obj->l_mat, &obj->scale);
        PSMTXCopy(obj->l_mat, obj->mat);
    }
    if (obj->Motion.pMot == 0) {
        obj->partsMatCalc();
    }
    obj16NeckMove(obj);
    obj->partsWorldCalc();
}

// Target scale the head eases towards.
void cObj16::setScale(Vec* pScale)
{
    o16.Scale = *pScale;
}

// Starts the death drip effect (Eff_wait 3).
void cObj16::setDieEff()
{
    o16.Eff_wait = 3;
}

// Makes the head fade out now (Be_flag bit 0, Lost_wait 0).
void cObj16::clearLostWait()
{
    o16.Lost_wait = 0;
    o16.Be_flag |= 1;
}

// Fades the head out after n frames.
void cObj16::setLostWait(int wait)
{
    o16.Lost_wait = wait;
    o16.Be_flag |= 1;
}

// Installs the 11 head motions (idle, wait, wait2, bites 3..6, damage 7/8, spit 9, wind-up 10) and
// starts the idle/wait one.
void cObj16::setMotData(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8,
                        void* m9, void* m10)
{
    Obj16Work* w = &o16;

    w->mot[0] = m0;
    w->mot[1] = m1;
    w->mot[2] = m2;
    w->mot[3] = m3;
    w->mot[4] = m4;
    w->mot[5] = m5;
    w->mot[6] = m6;
    w->mot[7] = m7;
    w->mot[8] = m8;
    w->mot[9] = m9;
    w->mot[10] = m10;
    if (type == 3 || type == 0xD) {
        MotionSetCore(this, &Motion, m2, 0, 0, 4, 0);
    } else {
        MotionSetCore(this, &Motion, m0, 0, 0, 4, 0);
    }
    w->x6C = 0;
    if (type == 3) {
        EstSet(this, -1, 0, 0, EFF_EM10, 0x5C, 0, w->EffKindId, this, 0);
        EstSet(this, -1, 0, 0, EFF_EM10, 0x70, 0, w->EffKindId2, this, 0);
    }
    if (type == 0xD) {
        EstSet(this, -1, 0, 0, EFF_EM3C, 0x1A, 0, w->EffKindId, this, 0);
        EstSet(this, -1, 0, 0, EFF_EM3C, 0xF, 0, w->EffKindId2, this, 0);
    }
    r_no_0 = 1;
    r_no_1 = 1;
    r_no_2 = 1;
    r_no_3 = 0;
}

// Player damage motion (and sequence) played on a bite hit (plemDmMStar).
void cObj16::setPlDmgMot(void* mot, void* seq)
{
    Obj16Work* w = &o16;

    w->Mot_pl_dm = mot;
    w->Seq_pl_dm = seq;
}

// Starts the bite attack routine (flag -> r_no_3: force the second bite motion).
void cObj16::setAtk(u8 mode)
{
    r_no_0 = 1;
    r_no_1 = 2;
    o16.Atk_ck = 0;
    r_no_2 = 0;
    r_no_3 = mode;
}

// Starts the decapitation routine.
void cObj16::setCritical()
{
    r_no_0 = 1;
    r_no_1 = 3;
    o16.Atk_ck = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Starts the hit reaction routine.
void cObj16::setDamage()
{
    r_no_0 = 1;
    r_no_1 = 4;
    r_no_2 = 0;
    r_no_3 = 0;
}

// 1 while the head is in its idle cycle (an attack may be started).
int cObj16::ckAtkEnable()
{
    if (o16.Atk_enable == 0) {
        return 0;
    }
    return 1;
}

// The player is dead (the upper half of the damage word).
static inline int PlIsDead()
{
    return pPL->dmg.m_Flag || pPL->dmg.m_Timer;
}

// Attack hit test of parts partsNo with obj16_atk_info[kind] (0/1 bite, 2 spit, 3 decapitation):
// on a player hit plays the blood/hit effects, sound, vibration and quake, puts the player into the
// damage motion (plemDmMStar, turned towards/away from the body), kind 3 kills him
// (obj16PlHeadLost); a partner hit (bit 1) kills the partner (LifeDownSet 9999). Sets Atk_ck.
int obj16AtkCk(cObj16* obj, u32 atk_type, int parts_no)
{
    Obj16Work* w = &obj->o16;
    cModel* body = w->body;
    cModel* p;
    Vec* pp;
    Vec plPos;
    EmAtkInfo info;
    Vec pos;
    int hit;
    f32 ang;

    if (body == 0) {
        return 0;
    }
    if (((cEm*) body)->hp <= 0) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (PlIsDead()) {
        return 0;
    }
    if (w->At_hit_wait != 0) {
        if (atk_type == 2) {
            return 0;
        }
    }
    p = obj->getPartsPtr(parts_no);
    pp = &p->world;
    pos = *pp;
    if (atk_type == 3) {
        pos.y -= 500.0f;
    }
    plPos = pPL->pos;
    plPos.y += 1600.0f;
    if (pos.y > body->pos.y + 2500.0f) {
        return 0;
    }
    info = obj16_atk_info[atk_type];
    hit = EmAtkHitCk(&info, pp, pp, 0);
    if (hit & 1) {
        switch (atk_type) {
        default:
        case 0:
            if (obj->type == 2) {
                EmPlBloodSet2(obj, pp, 1, 0x10, 0x6D);
            }
            Ctrl12Set(w->pCtrlGroup, CTRL12_ID_EM10_NOT_NEAR, 0x1E);
            if (obj->type == 2 && w->target) {
                SndCall(8, 0x3E, &w->target->pos, w->target->id, 0, 0);
            }
            if (obj->type == 0xB && w->target) {
                SndCall(8, 0x12, &w->target->pos, w->target->id, 0, 0);
            }
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->Mot_pl_dm && (s16) pG->pl_life > 0 && w->body) {
                if (obj->type == 2) {
                    EstSet(pPL, -1, 0, 0, EFF_EM10, 0x74, 0, ESP_CORE_KIND_NONE, pPL, 0);
                }
                if (obj->type == 0xB) {
                    EstSet(pPL, -1, 0, 0, EFF_EM3C, 9, 0, ESP_CORE_KIND_NONE, pPL, 0);
                }
                SetPlDamage((cEm*) obj, plemDmMStar);
                if (fabsf(Muku(&pPL->pos, &w->body->pos, pPL->ang.y, PI)) < PI / 2) {
                    ang = Muku(&pPL->pos, &w->body->pos, pPL->ang.y, PI);
                    pPL->ang.y = pPL->ang.y + ang;
                    pPL->r_no_3 = 0;
                } else {
                    ang = Muku(&w->body->pos, &pPL->pos, pPL->ang.y, PI);
                    pPL->ang.y = pPL->ang.y + ang;
                    pPL->r_no_3 = 1;
                }
            } else {
                if (obj->type == 2) {
                    EstSet(obj, -1, 0, 0, EFF_EM10, 0x73, 0, ESP_CORE_KIND_NONE, obj, 0);
                }
                if (obj->type == 0xB) {
                    EstSet(obj, -1, 0, 0, EFF_EM3C, 8, 0, ESP_CORE_KIND_NONE, obj, 0);
                }
            }
            break;
        case 1:
            if (obj->type == 2) {
                EmPlBloodSet2(obj, pp, 1, 0x10, 0x6E);
            }
            Ctrl12Set(w->pCtrlGroup, CTRL12_ID_EM10_NOT_NEAR, 0x1E);
            if (obj->type == 2 && w->target) {
                SndCall(8, 0x3E, &w->target->pos, w->target->id, 0, 0);
            }
            if (obj->type == 0xB && w->target) {
                SndCall(8, 0x12, &w->target->pos, w->target->id, 0, 0);
            }
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (obj->type == 2) {
                EstSet(obj, -1, 0, 0, EFF_EM10, 0x73, 0, ESP_CORE_KIND_NONE, obj, 0);
            }
            break;
        case 2:
            w->At_hit_wait = 90;
            if (obj->type == 2) {
                EmPlBloodSet2(obj, pp, 1, 0x10, 0x72);
            }
            if (obj->type == 0xB) {
                EmPlBloodSet2(obj, pp, 1, 0x31, 7);
            }
            Ctrl12Set(w->pCtrlGroup, CTRL12_ID_EM10_NOT_NEAR, 0x1E);
            if (obj->type == 2 && w->target) {
                SndCall(8, 0x3E, &w->target->pos, w->target->id, 0, 0);
            }
            if (obj->type == 0xB && w->target) {
                SndCall(8, 0x12, &w->target->pos, w->target->id, 0, 0);
            }
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (obj->type == 2) {
                EstSet(obj, -1, 0, 0, EFF_EM10, 0x73, 0, ESP_CORE_KIND_NONE, obj, 0);
            }
            break;
        case 3:
            obj16PlHeadLost(obj);
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            QuakeExec(0, 0, 5, 22.0f, 2);
            break;
        }
        w->Atk_ck = 1;
    }
    if (hit & 2) {
        if (atk_type != 3) {
            Ctrl12Set(w->pCtrlGroup, CTRL12_ID_EM10_NOT_NEAR, 0x1E);
        } else {
            if (pSUB->id & 3) {
                LifeDownSet(pSUB, 9999, 0);
            }
        }
    }
    return 1;
}

// The decapitation: player life 0, death damage type 6, region-specific scream, the player's head
// hidden and the body's own head-lost effect by enemy id.
// The head bit the player's head off: game over.
void obj16PlHeadLost(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    u8 region;

    pG->pl_life = 0;
    PlSetDamage(PL_DM_FRONT, 0, 0);
    region = pSys->eff_country;
    if (region == 0) {
        PlSetDamageSe(0xD);
        if (w->body) {
            switch (w->body->id) {
            case 0x10 ... 0x17:
            case 0x19 ... 0x20:
                EstSet(pPL, -1, 0, 0, EFF_EM10, 0x57, 0, ESP_CORE_KIND_NONE, pPL, 0);
                break;
            case 0x3C:
                EstSet(pPL, -1, 0, 0, EFF_EM3C, 0x28, 0, ESP_CORE_KIND_NONE, pPL, 0);
                break;
            }
        }
    } else {
        pPL->setHead(0);
        if (w->body) {
            switch (w->body->id) {
            case 0x10 ... 0x17:
            case 0x19 ... 0x20:
                EstSet(pPL, -1, 0, 0, EFF_EM10, 0x45, 0, ESP_CORE_KIND_NONE, pPL, 0);
                break;
            case 0x3C:
                EstSet(pPL, -1, 0, 0, EFF_EM3C, 0x27, 0, ESP_CORE_KIND_NONE, pPL, 0);
                break;
            }
        }
        SndCall(1, 0x3E, &pPL->pos, 0, 0, 0);
    }
}

// Eases Neck_dir towards the direction of the player (or the nearer partner) and writes it as an
// override rotation (flag 0x40000000) on the neck parts (1/2 for the tentacle types, others per type).
// Turn the neck parts (1, 2) toward the player and tilt the head (parts 0) along the body parts.
static void obj16NeckMove(cObj16* obj)
{
    Obj16Work* w = &obj->o16;
    cModel* body = w->body;
    Vec tgt;
    Vec dir;
    f32 ang;
    Obj16Parts* p;

    if (body == 0) {
        return;
    }
    switch (obj->type) {
    case 2:
    case 3:
    case 0xB:
    case 0xD:
    case 0xE:
        break;
    default:
        return;
    }
    tgt = pPL->pos;
    if (pSUB) {
        f32 d = VEC_DIST(&body->pos, &pPL->pos);
        if (d > VEC_DIST(&body->pos, &pSUB->pos) +
                    3000.0f) {
            tgt = pSUB->pos;
        }
    }
    tgt.y += 1600.0f;
    w->Neck_dir = w->Neck_dir * 0.9f + Muku(&body->pos, &tgt, body->ang.y, PI / 2) * 0.1f;
    switch (obj->type) {
    case 2:
    case 0xE:
        ang = w->Neck_dir * 0.5f;
        p = (Obj16Parts*) obj->getPartsPtr(1);
        p->rot.x = 0.0f;
        p->rot.y = ang;
        p->flags |= 0x40000000;
        p->rot.z = 0.0f;
        p = (Obj16Parts*) obj->getPartsPtr(2);
        p->rot.x = 0.0f;
        p->rot.y = ang;
        p->flags |= 0x40000000;
        p->rot.z = 0.0f;
        if (w->body) {
            cModel* bp = w->body->getPartsPtr(w->parts_no);
            dir.x = 0.0f;
            dir.y = 0.0f;
            dir.z = 1.0f;
            PSMTXMultVecSR(bp->mat, &dir, &dir);
            if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
                dir.z = 1.0f;
            }
#line 1850 "D:/Bio4/Prog/obj16.cpp"
            VECNormalize(&dir, &dir);
            ang = asinf(dir.y);
            p = (Obj16Parts*) obj->getPartsPtr(0);
            p->rot.x = ang;
            p->rot.y = 0.0f;
            p->flags |= 0x40000000;
            p->rot.z = 0.0f;
        }
        break;
    case 3:
    case 0xB:
    case 0xD:
        ang = w->Neck_dir * 0.5f;
        p = (Obj16Parts*) obj->getPartsPtr(1);
        p->rot.x = 0.0f;
        p->rot.y = ang;
        p->flags |= 0x40000000;
        p->rot.z = 0.0f;
        p = (Obj16Parts*) obj->getPartsPtr(2);
        p->rot.x = 0.0f;
        p->rot.y = ang;
        p->flags |= 0x40000000;
        p->rot.z = 0.0f;
        break;
    }
}

// Tints the head dark (burnt).
void cObj16::setBurn()
{
    cModelInfo* info;

    for (info = pModelInfo; info; info = info->pList) {
        info->color[0] = 0x20;
        info->color[1] = 0x20;
        info->color[2] = 0x20;
    }
}

// 1 when the last attack routine hit.
int cObj16::ckAtkHit()
{
    return o16.Atk_ck ? 1 : 0;
}

// Player damage routine for a plaga bite (SetPlDamage): plays Mot_pl_dm (mirrored blend for
// r_no_3), damage sound, then EndPlDamage.
// Player damage routine while the head holds him (SetPlDamage callback).
void plemDmMStar(cPlayer* pEm)
{
    Obj16Work* w = &((cObj16*) pPL->pEmCatch)->o16;
    int hokan;

    if (pEm->r_no_3 == 0) {
        pEm->dmg.set(0, 2);
    }
    switch (pEm->r_no_2) {
    case 0:
        if (pEm->r_no_3) {
            hokan = 0x41;
        } else {
            hokan = 1;
        }
        MotionSetCore(pEm, &pEm->Motion, w->Mot_pl_dm, (void*) w->Seq_pl_dm, 3, hokan, 0);
        PlSetDamageSe(0);
        if (pEm->r_no_3) {
            pEm->dmg.set(0, 0xF);
        }
        pEm->r_no_2++;
    case 1:
        if (MotionMove(pEm, 0)) {
            EndPlDamage();
            if (pEm->r_no_3 == 0) {
                pEm->dmg.set(0, 0xF);
            }
        }
        break;
    }
}
