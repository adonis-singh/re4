// em38 module (D:/Bio4/Prog/em38.cpp): the boss built from several enemies of one class (cModel::type
// 0 = the body that lifts its head and stamps (em38_R1_HeadUp / HeadStamp / Atk), 1 / 2 = the two
// tentacles that hide in the water and grab the player (em38_R1_T_*), 3 = the upper body riding on it
// and 4 = the lower body carrying the parasites (em38BirthParasite / em38ShellControl)).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em38.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "obj00.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "motion.h"
#include "route_ck.h"
#include "act_btn.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include <dolphin/os.h>
#include "em_mod.h"

// The module's 0x34-byte COMMON block: uninitialised template statics of the original object,
// merged into .bss by the REL link.
asm(".comm common_em38,52,4");


// game/em_dm_val.cpp (declared in em10.h, which is not included here: emwep.h's extern "C" plemBackjump would
// turn this unit's static plemBackjump into a global C function).
int GetWepDmVal(cEm* em, u32 wep_no, int near);

typedef void (*Em38Func)(cEm38*);

static void em38_R0_Init(cEm38* em);
static void em38_R0_Move(cEm38* em);
static void em38_R1_br_Dummy(cEm38* em);
static void em38_R1_Wait(cEm38* em);
static void em38_R1_HeadUp(cEm38* em);
static void em38_R1_HeadStamp(cEm38* em);
static void em38_R1_br_Atk(cEm38* em);
static void em38_R1_Atk(cEm38* em);
static void em38_R1_AtkHit(cEm38* em);
static void plem38_AtkHit(cPlayer* pl);
static void em38_R1_T_Wait(cEm38* em);
static void em38_R1_T_Hide(cEm38* em);
static void em38_R1_T_In(cEm38* em);
static void em38_R1_T_Out(cEm38* em);
static void em38_R1_T_Atk(cEm38* em);
static void em38_R1_br_T_MdlAtk(cEm38* em);
static void em38_R1_T_MdlAtk(cEm38* em);
static void em38_R1_br_T_BigAtk(cEm38* em);
static void em38_R1_T_BigAtk(cEm38* em);
static void em38_R1_T_DownAtk(cEm38* em);
static void em38_R1_T_CatchHit(cEm38* em);
static void plem38_CatchHit(cPlayer* pl);
static void em38_R1_U_Wait(cEm38* em);
static void em38_R1_U_Critical(cEm38* em);
static void em38_R1_L_Wait(cEm38* em);
static void em38_R0_Damage(cEm38* em);
static void em38_R1_Dm_Down(cEm38* em);
static void em38_R1_Dm_Upper(cEm38* em);
static void em38_R0_Die(cEm38* em);
static void em38_R1_Die_Body(cEm38* em);
static void em38_R1_Die_Upper(cEm38* em);
static void plemDmStamp(cPlayer* pl);
static void em38EscapeAction(cEm38* em);
static void plemEscape(cPlayer* pl);
static void em38BackjumpAction(cEm38* em);
static void plemBackjump(cPlayer* pl);
static void em38SitAction(cEm38* em);
static void plemSit(cPlayer* pl);


// The shell motion work as the MotionWork the motion library takes.
#define SHELL_MOT(w) ((MotionWork*) &(w)->shellMot)




extern "C" void _prolog()
{
    OSReport("em38 prolog Ok\n");
    EmInitFunc = Em38Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

// Placement-constructs one part of the boss over the manager's cEm slot (EmInitFunc for id 0x38);
// em38_R0_Init does the per-type setup on the first move.
void Em38Init(cEm* em)
{
    new (em) cEm38();
}

// Per-frame damage reaction of every part, from move(). Consumes dmHit: applies em38SetDmVal
// (LifeDownSet2 keeps 1 HP: the parts do not die from weapon fire) and em38BloodSet. The body's
// weak part 0x3B accumulates dmgCnt; at 200 the body goes down (routine 2/0) and the upper body cries
// out through ctrl11. The lower body routes hits on a root's two parts to that root's hp and sinks
// its tentacle (state 6, tentacle hp 1) when it runs out. The upper body at 1 HP is the kill: it and
// the body enter their death routines. Otherwise a hit upper body flinches (routine 2/1). Nothing
// restarts while a routine holds flags 0x10.
void em38DmCk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    YARARE_INFO* part;
    int dmg;
    u32 i;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    em->dmg.m_Timer = 1;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    part = em->dmg.m_pDamageYarare;
    dmg = em38SetDmVal(em);
    LifeDownSet2(em, dmg, 0, 1);
    em38BloodSet(em);
    if (em->type == 0 && part->parts_no == 0x3B) {
        w->dmgCnt += dmg;
    }
    if (em->type == 4) {
        for (i = 0; i < 2; i++) {
            Em38Para* p = &w->para[i];

            if (p->pEm) {
                if (part->parts_no == p->parts + 1 || part->parts_no == p->parts + 2) {
                    p->hp -= dmg;
                    if (p->hp <= 0) {
                        p->hp = 0;
                        if (p->state == 3 || p->state == 5) {
                            p->state = 6;
                            p->pEm->hp = 1;
                            return;
                        }
                    }
                }
            }
        }
    }
    if (em->hp <= 1 && em->type == 3) {
        EmSetDie(em);
        EmSetDieCnt(em);
        em->hp = 0;
        em->r_no_0 = 3;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        if (w->pBody) {
            w->pBody->hp = 0;
            w->pBody->r_no_0 = 3;
            w->pBody->r_no_1 = 0;
            w->pBody->r_no_2 = 0;
            w->pBody->r_no_3 = 0;
        }
    } else {
        if (w->flags & 0x10) {
            return;
        }
        if (em->type == 0) {
            if (part->parts_no == 0x3B) {
                if (!(w->flags & 8) && w->seWait == 0 && w->pUpper) {
                    Ctrl11SetSeEm38(w->pCtrl11, w->pUpper, 0x2D);
                    w->seWait = 180;
                }
                if (w->dmgCnt > 199) {
                    w->dmgCnt = 0;
                    em->setRno(2, 0, 0, 0);
                    return;
                }
            } else {
                if (!(w->flags & 8) && w->seWait == 0 && w->pUpper) {
                    Ctrl11SetSeEm38(w->pCtrl11, w->pUpper, 0x2A);
                    w->seWait = 180;
                }
            }
        }
        if (em->type == 3) {
            if (w->seWait == 0) {
                Ctrl11SetSeEm38(w->pCtrl11, em, 0x23);
            }
            em->setRno(2, 1, 0, 0);
        }
    }
}

Em38Func Em38_R0_move_tbl[4] = {
    em38_R0_Init,
    em38_R0_Move,
    em38_R0_Damage,
    em38_R0_Die,
};

// Pairs of {branch check, routine} per routine-1 state (em38_R0_Move calls both).
static Em38Func Em38_R1_move_tbl[34] = {
    em38_R1_br_Dummy, em38_R1_Wait,
    em38_R1_br_Dummy, em38_R1_HeadUp,
    em38_R1_br_Dummy, em38_R1_HeadStamp,
    em38_R1_br_Atk, em38_R1_Atk,
    em38_R1_br_Dummy, em38_R1_AtkHit,
    em38_R1_br_Dummy, em38_R1_T_Wait,
    em38_R1_br_Dummy, em38_R1_T_Hide,
    em38_R1_br_Dummy, em38_R1_T_In,
    em38_R1_br_Dummy, em38_R1_T_Out,
    em38_R1_br_Dummy, em38_R1_T_Atk,
    em38_R1_br_T_MdlAtk, em38_R1_T_MdlAtk,
    em38_R1_br_T_BigAtk, em38_R1_T_BigAtk,
    em38_R1_br_Dummy, em38_R1_T_DownAtk,
    em38_R1_br_Dummy, em38_R1_T_CatchHit,
    em38_R1_br_Dummy, em38_R1_U_Wait,
    em38_R1_br_Dummy, em38_R1_U_Critical,
    em38_R1_br_Dummy, em38_R1_L_Wait,
};

static Em38Func Em38_R2_move_tbl[2] = {
    em38_R1_Dm_Down,
    em38_R1_Dm_Upper,
};

static Em38Func Em38_R3_move_tbl[2] = {
    em38_R1_Die_Body,
    em38_R1_Die_Upper,
};

// Parts index remap of the flipped motions (cModel::motFlip): the identity.
static u16 em38_flip[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Attacks (em38AtkCk): [0] tentacle sweep, [1] tentacle slam, [2] head stamp, [3] head bite, [4] catch.
static EmAtkInfo em38_atk_tbl[5] = {
    { 1000.0f, PL_DM_AUTO, 500, 0, 0xA, 0 },
    { 1000.0f, PL_DM_AUTO, 0, 0, 0xA, 0 },
    { 1000.0f, PL_DM_AUTO, 500, 0, 0xA, 0 },
    { 1800.0f, PL_DM_AUTO, 9999, 0, 0xA, 0 },
    { 1800.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
};


// Per-frame update of one part. Order: damage, clear the per-frame flags, tick the timers (a
// player on the mid platform, y 1..3 m, keeps resetting the attack wait to 120..240 frames), route,
// the routine table (r_no_0 0xFF = model load failed: destroy), find the other parts, the shell,
// eyes, parasite roots, parts / attack / collision passes, parasite birth, the body's breathing
// sound and its upper body's opening voice, the upper body's light mask by the "weak point shown"
// flag (Status_flg[1] 0x04000000), and the weak point object.
void cEm38::move()
{
    Em38Work* w = EM38_WK(this);

    if (r_no_0) {
        em38DmCk(this);
    }
    w->flags &= ~0xBF;
    if (w->birthTimer) {
        w->birthTimer--;
    }
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->seWait) {
        w->seWait--;
    }
    if (pPL->pos.y > 1000.0f && pPL->pos.y < 3000.0f) {
        w->atkWait = Rnd() % 120 + 120;
    }
    em38RouteCk(this);
    Em38_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em38SearchParts(this);
    if (r_no_0) {
        em38ShellControl(this);
    }
    em38EyeMove(this);
    em38RootMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    em38BirthParasite(this);
    if (type == 0) {
        if (hp > 0) {
            if (w->seTimer) {
                w->seTimer--;
            } else {
                w->seTimer = 60;
                SndCall(8, 0x14, &pos, id, 0, this);
            }
        }
    }
    if (type == 0) {
        if (w->voiceWait && w->pUpper) {
            w->voiceWait--;
            if (w->voiceWait == 0) {
                Ctrl11SetSeEm38(w->pCtrl11, w->pUpper, 0x2E);
                w->seWait = 180;
            }
        }
    }
    if (type == 3) {
        if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
            LightInfo.EnableMask = 0x80;
        } else {
            LightInfo.EnableMask = 4;
        }
    }
    em38WeakMove(this);
}

// Routine 0: per-type setup on the first frame. Loads the model of the type (body: archive 4 with
// the extra model 9 / 0xA; tentacles 5; upper body 6; lower body 7, pinned to the origin), the
// identity flip table, a huge light box, a passed-through collision, the type's hit boxes, the
// effect data, the ctrl11 sound controller, the work (first attack in 270 frames, first parasite in
// 240, shell mode 6, opening voice in 10), the weak point object and the parasite roots. Start
// routines: body Wait, tentacles T_Wait at 200 HP, upper body U_Wait, lower body L_Wait.
static void em38_R0_Init(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(8)) == 0) {
            pLog->err(0, 0, "em38() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        w->pInfo = ModInfoMgr.create(ARC(9), ARC(0xA));
        if (w->pInfo) {
            em->addModel(w->pInfo);
        }
        break;
    case 1:
        if (em->modelInit(ARC(5), ARC(8)) == 0) {
            pLog->err(0, 0, "em38() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 2:
        if (em->modelInit(ARC(5), ARC(8)) == 0) {
            pLog->err(0, 0, "em38() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 3:
        if (em->modelInit(ARC(6), ARC(8)) == 0) {
            pLog->err(0, 0, "em38() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 4:
        if (em->modelInit(ARC(7), ARC(8)) == 0) {
            pLog->err(0, 0, "em38() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
        em->ang.x = 0.0f;
        em->ang.y = 0.0f;
        em->ang.z = 0.0f;
        break;
    }
    em->be_flag |= 0x1000;
    em->Motion.flip = em38_flip;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        // Four identical arms; `case 0: default:` is a real tree node whose `beq` jump1 folds away.
        switch (em->type) {
        case 0:
        default:
            em->LightInfo.init2(0, 1, &ofs, &size, 4);
            break;
        case 1:
        case 2:
            em->LightInfo.init2(0, 1, &ofs, &size, 4);
            break;
        case 3:
            em->LightInfo.init2(0, 1, &ofs, &size, 4);
            break;
        case 4:
            em->LightInfo.init2(0, 1, &ofs, &size, 4);
            break;
        }
    }
    em->atari.init(0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);
    em->State.SetLightIgnore();
    em->atari.off();
    switch (em->type) {
    case 0:
    default:
        em38YarareInitBody(em);
        break;
    case 1:
        em38YarareInitTentacle(em);
        break;
    case 2:
        em38YarareInitTentacle(em);
        break;
    case 3:
        em38YarareInitUpper(em);
        break;
    case 4:
        em38YarareInitLower(em);
        break;
    }
    em->setTarget(2, 0.0f, 0.0f, 0.0f);
    EspDataLoad((u32) ARC(0xB), EFF_EM38, 0);
    w->espKind = EspPullCoreKind();
    w->espKind2 = EspPullCoreKind();
    if (em->type == 0) {
        EstSet(em, -1, 0, 0, EFF_EM38, 4, 1, w->espKind2, em, 0);
    }
    w->pCtrl11 = GetCtrlCtrl11();
    w->flags = 0;
    w->eyeX = 0.0f;
    w->eyeY = 0.0f;
    w->blendRate = 0.0f;
    w->dmgCnt = 0;
    w->birthTimer = 240;
    w->atkWait = 270;
    w->mode = 6;
    w->voiceWait = 10;
    em38WeakInit(em);
    em38RootInit(em);
    switch (em->type) {
    case 0:
    default:
        em->setStatus(EM_STATUS_ACTIVE);
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 1, 0);
        MotionMove(em, 0);
        break;
    case 1:
        em->setRno(1, 5, 0, 0);
        MotionSetCore(em, MOTION(em), ARC(0x2F), 0, 0, 5, 0);
        em->hp = 200;
        MotionMove(em, 0);
        break;
    case 2:
        em->setRno(1, 5, 0, 0);
        MotionSetCore(em, MOTION(em), ARC(0x2F), 0, 0, 5, 0);
        em->hp = 200;
        MotionMove(em, 0);
        break;
    case 3:
        em->setStatus(EM_STATUS_ACTIVE);
        em->setRno(1, 0xE, 0, 0);
        MotionSetCore(em, MOTION(em), ARC(0x29), 0, 0, 1, 0);
        MotionMove(em, 0);
        break;
    case 4:
        em->setRno(1, 0x10, 0, 0);
        MotionSetCore(em, MOTION(em), ARC(0x50), 0, 0, 5, 0);
        MotionMove(em, 0);
        break;
    }
    em38_R0_Move(em);
    OSReport("em38 free size = 0x%x\n", 0x900);
}

// Routine 1: runs the branch check and then the behaviour of the current r_no_1 state.
static void em38_R0_Move(cEm38* em)
{
    Em38_R1_move_tbl[em->r_no_1 * 2](em);
    Em38_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the states that have none.
static void em38_R1_br_Dummy(cEm38* em)
{
}

// Body routine 1/0: idle. The idle loop is blended left / right (blendRate eased towards the yaw
// of the player from the head part 4, up to 40 deg). At each loop end: on Game_level up to 1 the
// first loop is skipped half the time; once atkWait is out and the head has settled on the player, a
// 60 % roll bites (routine 1/3); otherwise after waitCnt (2..3) loops the head goes up (1/1).
static void em38_R1_Wait(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    cParts* p = em->getPartsPtr(4);
    f32 t;
    f32 ang;

    switch (em->r_no_2) {
    case 0:
        w->mot[0] = ARC(0xC);
        w->mot[1] = ARC(0xE);
        w->mot[2] = ARC(0xD);
        w->blendKind = 5;
        w->blendA = 0;
        w->blendB = 0;
        w->timer = 0;
        w->waitCnt = (Rnd() & 1) + 2;
        em->r_no_2++;
    case 1:
        t = Muku(&p->world, &pPL->pos, em->ang.y, PI);
        if (t > 0.6981317f) {
            t = 0.6981317f;
        }
        if (t < -0.6981317f) {
            t = -0.6981317f;
        }
        ang = t * 365.26062f;
        w->blendRate = w->blendRate * 0.98f + ang * 0.02f;
        em38BlendMotSet(em, w->mot[0], w->mot[1], w->mot[2], 0, 0, 0, w->blendKind);
        if (MotionMove(em, 0)) {
            w->timer++;
            if (pG->Game_level <= 1 && w->timer <= 1) {
                if ((u8) (Rnd() % 10) > 4) {
                    return;
                }
            }
            if (w->atkWait == 0 && fabsf(w->blendRate - ang) < 10.0f) {
                if ((u8) (Rnd() % 10) > 3) {
                    em->setRno(1, 3, 0, 0);
                    return;
                }
            }
            if (w->waitCnt) {
                w->waitCnt--;
            } else {
                em->setRno(1, 1, 0, 0);
            }
        }
        break;
    }
}

// Body routine 1/1: rears its head (flags 0x20 = ckHeadUp for the tentacles, whose next attacks
// are pushed 120 / 180 frames out). Steps: rise, hold the loop until both tentacles are out of the
// water (ckIn; if not, stamp instead: 1/2), then lower the head back towards the player and Wait.
static void em38_R1_HeadUp(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    cParts* p = em->getPartsPtr(4);
    f32 t;
    f32 ang;

    switch (em->r_no_2) {
    case 0:
        w->mot[0] = ARC(0x20);
        w->mot[1] = ARC(0x23);
        w->mot[2] = ARC(0x22);
        w->mot[3] = ARC(0x21);
        w->blendKind = 1;
        w->blendA = 30;
        w->blendB = 0;
        if (w->pTent[0]) {
            w->pTent[0]->setAtkWait(120);
        }
        if (w->pTent[1]) {
            w->pTent[1]->setAtkWait(180);
        }
        em->r_no_2++;
    case 1:
        w->flags |= 0x20;
        w->blendRate *= 0.98f;
        em38BlendMotSet(em, w->mot[0], w->mot[1], w->mot[2], w->mot[3], 0, 0, w->blendKind);
        if (MotionMove(em, 0) == 0) {
            return;
        }
        em->r_no_2++;
        break;
    case 2:
        w->mot[0] = ARC(0x1D);
        w->mot[1] = ARC(0x1F);
        w->mot[2] = ARC(0x1E);
        w->blendKind = 5;
        w->blendB = w->blendA = 0;
        em->r_no_2++;
    case 3:
        w->flags |= 0x20;
        w->blendRate *= 0.98f;
        em38BlendMotSet(em, w->mot[0], w->mot[1], w->mot[2], 0, 0, 0, w->blendKind);
        if (MotionMove(em, 0)) {
            if (w->pTent[0] && w->pTent[1]) {
                if (w->pTent[0]->ckIn() == 0 || w->pTent[1]->ckIn() == 0) {
                    em->setRno(1, 2, 0, 0);
                    return;
                }
            }
            em->r_no_2++;
        }
        break;
    case 4:
        w->mot[0] = ARC(0x24);
        w->mot[1] = ARC(0x27);
        w->mot[2] = ARC(0x26);
        w->mot[3] = ARC(0x25);
        w->blendKind = 1;
        w->blendB = w->blendA = 0;
        em->r_no_2++;
    case 5:
        w->flags |= 0x20;
        t = Muku(&p->world, &pPL->pos, em->ang.y, PI);
        if (t > 0.6981317f) {
            t = 0.6981317f;
        }
        if (t < -0.6981317f) {
            t = -0.6981317f;
        }
        ang = t * 365.26062f;
        w->blendRate = w->blendRate * 0.98f + ang * 0.02f;
        em38BlendMotSet(em, w->mot[0], w->mot[1], w->mot[2], w->mot[3], 0, 0, w->blendKind);
        if (MotionMove(em, 0)) {
            em->setRno(1, 0, 0, 0);
        }
        break;
    }
}

// Body routine 1/2: the head slams down (tentacle attacks pushed 210 / 240 frames out). On motion
// event bit 0 the catch attack (table 4) is tested at head parts 0x17..0x19; a miss awards the escape
// rank point. Back to Wait.
static void em38_R1_HeadStamp(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    cParts* p = em->getPartsPtr(4);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x13), ARC(0x14), 30, 1, 0);
        if (w->pTent[0]) {
            w->pTent[0]->setAtkWait(210);
        }
        if (w->pTent[1]) {
            w->pTent[1]->setAtkWait(240);
        }
        w->blendRate = 0.0f;
        EstSet(em, -1, 0, 0, EFF_EM38, 0xF, 1, w->espKind, em, 0);
        w->atkHit = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            w->blendRate = 0.0f;
            em->setRno(1, 0, 0, 0);
        } else if (em->Motion.Seq_old.Free & 1) {
            em38AtkCk(em, 4, 0x17);
            em38AtkCk(em, 4, 0x18);
            em38AtkCk(em, 4, 0x19);
        }
        break;
    }
}

// Branch check of the bite: on motion event bit 0 the bite sphere (table 3, instant kill) is tested
// at head part 0x19; a hit rumbles, switches to AtkHit (1/4) and marks the player, body and upper
// body dmType 0x80 (the death-bite scene).
static void em38_R1_br_Atk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    if (em->Motion.Seq_old.Free & 1) {
        w->atkHit = 0;
        em38AtkCk(em, 3, 0x19);
        if (w->atkHit) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xD, 1);
            EmRoutineSetW(em, 1, 4, 0, 0);
            pPL->dmg.m_Timer = 0x80;
            em->dmg.m_Timer = 0x80;
            if (w->pUpper) {
                w->pUpper->dmg.m_Timer = 0x80;
            }
        }
    }
}

// Body routine 1/3: the bite lunge (flags 0x80 = ckCritical). Opens the shell (mode 2, 150 frames)
// if it was closed, makes the upper body play its attack pose (setCritical), and homes on the player
// for the first 15 frames through the blend rate. When the motion ends the player escaped: rank
// point, 210-frame attack wait, back to Wait.
static void em38_R1_Atk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    cParts* p = em->getPartsPtr(4);
    f32 t;
    f32 ang;

    switch (em->r_no_2) {
    case 0:
        w->mot[0] = ARC(0xF);
        w->mot[3] = ARC(0x10);
        w->mot[1] = ARC(0x12);
        w->mot[2] = ARC(0x11);
        w->blendKind = 5;
        w->blendA = 30;
        w->blendB = 0;
        if (w->mode == 1 || w->mode == 7) {
            w->mode = 2;
            w->shellTimer = 150;
        }
        w->atkHit = 0;
        if (w->pUpper) {
            w->pUpper->setCritical();
        }
        EstSet(em, -1, 0, 0, EFF_EM38, 6, 1, w->espKind, em, 0);
        w->timer = 15;
        em->r_no_2++;
    case 1:
        w->flags |= 0x80;
        if (w->timer) {
            w->timer--;
            t = Muku(&p->world, &pPL->pos, em->ang.y, PI);
            if (t > 0.6981317f) {
                t = 0.6981317f;
            }
            if (t < -0.6981317f) {
                t = -0.6981317f;
            }
            ang = t * 365.26062f;
            w->blendRate = w->blendRate * 0.995f + ang * 0.005f;
        }
        em38BlendMotSet(em, w->mot[0], w->mot[1], w->mot[2], w->mot[3], 0, 0, w->blendKind);
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            w->atkWait = 210;
            em->setRno(1, 0, 0, 0);
        }
        break;
    }
}

// Body routine 1/4: the bite connected. Plays the swallow motion once with its effect and sound
// and takes over the player (plem38_AtkHit: a death); the routine holds (flags 0x10) as the game
// over runs.
static void em38_R1_AtkHit(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    em->dmg.m_Timer = 2;
    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x1C), 0, 0, 1, 0);
        SetPlDamage(em, plem38_AtkHit);
        w->blendRate = 0.0f;
        EstSet(em, -1, 0, 0, EFF_EM38, 8, 0, ESP_CORE_KIND_NONE, em, 0);
        SndCall(8, 0x21, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
}

// Player damage callback of em38_R1_AtkHit: the player is bitten.
static void plem38_AtkHit(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0: {
        Vec v;

        pl->atari.off();
        v.x = -106.0f;
        v.y = -5817.0f;
        v.z = 13351.18f;
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        pl->ang.y = pl->pEmCatch->ang.y + PI;
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x46), 0, 0, 1, 0);
        pG->pl_life = 0;
        PlSetFace(1);
        pl->r_no_2++;
    }
    case 1:
        MotionMove(pl, 0);
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Tentacle routine 1/5: out of the water, idling (type 1 plays the flipped motion). Sinks (1/8) when
// the body is dead or its own HP is down to 1. Once atkWait is out: with the player up on the
// platform (y over 2 m) it does the big slam (1/0xB) while the body's head is up, or with a 20 % roll
// (not while the body bites, Game_level above 1) the middle slam (1/0xA), else the sweep (1/9); with
// the player below, the down slam (1/0xC) on Game_level above 1. Splashes every 60 frames.
static void em38_R1_T_Wait(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (em->type == 2) {
            MotionSetCore(em, MOTION(em), ARC(0x2F), 0, 30, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x2F), 0, 30, 0x45, 0);
        }
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (w->pBody && w->pBody->hp <= 0) {
            em->setRno(1, 8, 0, 0);
            break;
        }
        if (em->hp <= 1) {
            em->setRno(1, 8, 0, 0);
            break;
        }
        if (w->atkWait) {
            break;
        }
        if (pPL->pos.y > 2000.0f) {
            if (w->pBody) {
                if (w->pBody->ckHeadUp()) {
                    em->setRno(1, 0xB, 0, 0);
                    break;
                }
                if ((u8) (Rnd() % 10) <= 1 && w->pBody->ckCritical() == 0 && pG->Game_level > 1) {
                    em->setRno(1, 0xA, 0, 0);
                    break;
                }
            }
            em->setRno(1, 9, 0, 0);
        } else {
            if (pG->Game_level > 1) {
                em->setRno(1, 0xC, 0, 0);
            }
        }
        break;
    }
    if (w->seTimer) {
        w->seTimer--;
    } else {
        w->seTimer = 60;
        SndCall(8, 0, &em->pos, em->id, 0, em);
    }
}

// Tentacle routine 1/6: hidden under water (flags 0x40 off), holding the submerged pose until the
// lower body's root raises it again (setIn -> 1/7).
static void em38_R1_T_Hide(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->r_no_2++;
        w->flags &= ~0x40;
    case 1:
        if (em->type == 2) {
            MotionSetCore(em, MOTION(em), ARC(0x30), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x30), 0, 0, 0x45, 0);
        }
        MotionMove(em, 0);
        break;
    }
}

// Tentacle routine 1/7: rises out of the water with the splash effects and sound (flags 0x40 =
// ckIn), attack wait cleared, then T_Wait.
static void em38_R1_T_In(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->type == 2) {
            MotionSetCore(em, MOTION(em), ARC(0x30), 0, 0, 5, 0);
            EstSet(0, -1, 0, 0, EFF_EM38, 0x1F, 1, w->espKind, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x30), 0, 0, 0x45, 0);
            EstSet(0, -1, 0, 0, EFF_EM38, 0x21, 1, w->espKind, em, 0);
        }
        EstSet(em, -1, 0, 0, EFF_EM38, 0x23, 1, w->espKind, em, 0);
        w->atkWait = 0;
        SndCall(8, 8, &em->pos, em->id, 0, em);
        w->flags |= 0x40;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->setRno(1, 5, 0, 0);
        }
        break;
    }
}

// Tentacle routine 1/8: sinks back under water (its lingering effects deleted, splash and sound),
// then T_Hide.
static void em38_R1_T_Out(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        EffectEspDelete(1, w->espKind, em, 0);
        EffectEspgenDelete(1, w->espKind, em);
        EffectEfmDelete(1, w->espKind, em);
        if (em->type == 2) {
            MotionSetCore(em, MOTION(em), ARC(0x31), 0, 10, 1, 0);
            EstSet(0, -1, 0, 0, EFF_EM38, 0x20, 1, w->espKind, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x31), 0, 10, 0x41, 0);
            EstSet(0, -1, 0, 0, EFF_EM38, 0x21, 1, w->espKind, em, 0);
        }
        SndCall(8, 9, &em->pos, em->id, 0, em);
        w->flags &= ~0x40;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->setRno(1, 6, 0, 0);
        }
        break;
    }
}

// The attack wait of the tentacle routines by difficulty (pG reloaded after every store).
static inline void em38SetTentAtkWait(Em38Work* w)
{
    w->atkWait = 270;
    if (pG->Game_level <= 2) {
        w->atkWait = 330;
    }
    if (pG->Game_level > 7) {
        w->atkWait = 210;
    }
}

// Tentacle routine 1/9: the horizontal sweep across the platform. The swing is a blend steered
// towards the player from a point 4 m ahead (asymmetric limits so it stays in front of the body).
// The sweep sphere (table 0) is tested at parts 7..0x12 on motion event bit 0. For the first 25
// frames, while the player is on the far side (x beyond +-3 m) and has neither been hit nor escaped,
// the action button prompt offers the duck (em38EscapeAction; the prompt icon depends on whether the
// player is aiming, r_no_0 4). A miss awards the escape point; hp 1 sinks it. Back to T_Wait.
static void em38_R1_T_Atk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    int end;

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        w->blendB = 0;
        w->blendA = 30;
        if (em->type == 2) {
            w->mot[0] = ARC(0x34);
            w->mot[3] = ARC(0x35);
            w->mot[1] = ARC(0x37);
            w->mot[2] = ARC(0x36);
            w->blendKind = 1;
            EstSet(em, -1, 0, 0, EFF_EM38, 0x16, 1, w->espKind, em, 0);
        } else {
            w->mot[0] = ARC(0x34);
            w->mot[3] = ARC(0x35);
            w->mot[1] = ARC(0x36);
            w->mot[2] = ARC(0x37);
            w->blendKind = 0x41;
            EstSet(em, -1, 0, 0, EFF_EM38, 0x17, 1, w->espKind, em, 0);
        }
        em38SetTentAtkWait(w);
        w->atkHit = 0;
        w->escaped = 0;
        w->timer = 25;
        w->actVar = Rnd() & 1;
        em->r_no_2++;
    case 1: {
        Vec v;
        f32 ang;

        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 4000.0f;
        PSMTXMultVec(em->mat, &v, &v);
        if (em->Motion.Mot_attr & 0x40) {
            ang = em->ang.y - 0.6981317f;
        } else {
            ang = em->ang.y + 0.6981317f;
        }
        ang = LIMIT_ANGLE(ang);
        ang = Muku(&v, &pPL->pos, ang, PI);
        if (em->Motion.Mot_attr & 0x40) {
            if (ang > 0.0f) {
                if (ang > 0.34906584f) {
                    ang = 0.34906584f;
                }
                ang *= 730.52124f;
            } else {
                if (ang < -0.7853982f) {
                    ang = -0.7853982f;
                }
                ang *= 324.6761f;
            }
        } else {
            if (ang > 0.0f) {
                if (ang > 0.7853982f) {
                    ang = 0.7853982f;
                }
                ang *= 324.6761f;
            } else {
                if (ang < -0.34906584f) {
                    ang = -0.34906584f;
                }
                ang *= 730.52124f;
            }
        }
        w->blendRate = w->blendRate * 0.8f + ang * 0.2f;
        em38BlendMotSet(em, w->mot[0], w->mot[1], w->mot[2], w->mot[3], 0, 0, w->blendKind);
        end = MotionMove(em, 0);
        if (end) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            em->setRno(1, 5, 0, 0);
        } else if (em->hp <= 1) {
            em->setRno(1, 8, 0, 0);
        } else {
            if (em->Motion.Seq_old.Free & 1) {
                em38AtkCk(em, 0, 7);
                em38AtkCk(em, 0, 8);
                em38AtkCk(em, 0, 9);
                em38AtkCk(em, 0, 0xA);
                em38AtkCk(em, 0, 0xB);
                em38AtkCk(em, 0, 0xC);
                em38AtkCk(em, 0, 0xD);
                em38AtkCk(em, 0, 0xE);
                em38AtkCk(em, 0, 0xF);
                em38AtkCk(em, 0, 0x10);
                em38AtkCk(em, 0, 0x11);
                em38AtkCk(em, 0, 0x12);
            }
            if (pPL->r_no_0 == 1 || pPL->r_no_0 == 2 || (StaFlagChk(pG, STA_PL_CATCHED))) {
                w->escaped = 1;
            }
            if (w->timer && w->atkHit == 0 && w->escaped == 0) {
                w->timer--;
                if ((pPL->pos.x < -3000.0f && em->type == 1) || (pPL->pos.x > 3000.0f && em->type == 2)) {
                    if (pPL->r_no_0 == 4) {
                        if (w->actVar) {
                            ActBtn.set(ACT_GUARD, 0xB, (void*) em38EscapeAction, em, ACTCTR_WEP_SET_IGNORE | ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);
                        } else {
                            ActBtn.set(ACT_GUARD, 0xB, (void*) em38EscapeAction, em, ACTCTR_WEP_SET_IGNORE | ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
                        }
                    } else {
                        if (w->actVar) {
                            ActBtn.set(ACT_GUARD, 0xB, (void*) em38EscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
                        } else {
                            ActBtn.set(ACT_GUARD, 0xB, (void*) em38EscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);
                        }
                    }
                }
            }
        }
        break;
    }
    }
}

// Branch check of the middle slam: on motion event bit 0 the slam sphere (table 1) is tested at
// parts 7..0x12; a hit switches to T_CatchHit (1/0xD).
static void em38_R1_br_T_MdlAtk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    if (em->Motion.Seq_old.Free & 1) {
        w->atkHit = 0;
        em38AtkCk(em, 1, 7);
        em38AtkCk(em, 1, 8);
        em38AtkCk(em, 1, 9);
        em38AtkCk(em, 1, 0xA);
        em38AtkCk(em, 1, 0xB);
        em38AtkCk(em, 1, 0xC);
        em38AtkCk(em, 1, 0xD);
        em38AtkCk(em, 1, 0xE);
        em38AtkCk(em, 1, 0xF);
        em38AtkCk(em, 1, 0x10);
        em38AtkCk(em, 1, 0x11);
        em38AtkCk(em, 1, 0x12);
        if (w->atkHit) {
            EmRoutineSetW(em, 1, 0xD, 0, 0);
        }
    }
}

// Tentacle routine 1/0xA: the middle slam onto the platform (the hit is in the branch check, which
// leads to the catch). After 10 frames, for 25 frames, while the player is on this tentacle's side
// (x within 4 m) the action button offers the back jump (em38BackjumpAction). Exits like T_Atk.
static void em38_R1_T_MdlAtk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    int end;

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->type == 2) {
            MotionSetCore(em, MOTION(em), ARC(0x32), ARC(0x33), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM38, 0x14, 1, w->espKind, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x32), ARC(0x33), 10, 0x41, 0);
            EstSet(em, -1, 0, 0, EFF_EM38, 0x15, 1, w->espKind, em, 0);
        }
        em38SetTentAtkWait(w);
        w->atkHit = 0;
        w->escaped = 0;
        w->timer = 25;
        w->timer2 = 10;
        w->actVar = Rnd() & 1;
        em->r_no_2++;
    case 1:
        end = MotionMove(em, 0);
        if (end) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            em->setRno(1, 5, 0, 0);
        } else if (em->hp <= 1) {
            em->setRno(1, 8, 0, 0);
        } else {
            if (pPL->r_no_0 == 1 || pPL->r_no_0 == 2 || (StaFlagChk(pG, STA_PL_CATCHED))) {
                w->escaped = 1;
            }
            if (w->timer2) {
                w->timer2--;
            } else if (w->timer && w->atkHit == 0 && w->escaped == 0) {
                w->timer--;
                if ((pPL->pos.x < 4000.0f && em->type == 1) || (pPL->pos.x > -4000.0f && em->type == 2)) {
                    if (pPL->r_no_0 == 4) {
                        if (w->actVar) {
                            ActBtn.set(ACT_GUARD, 0xB, (void*) em38BackjumpAction, em, ACTCTR_WEP_SET_IGNORE | ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);
                        } else {
                            ActBtn.set(ACT_GUARD, 0xB, (void*) em38BackjumpAction, em, ACTCTR_WEP_SET_IGNORE | ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
                        }
                    } else {
                        if (w->actVar) {
                            ActBtn.set(ACT_GUARD, 0xB, (void*) em38BackjumpAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
                        } else {
                            ActBtn.set(ACT_GUARD, 0xB, (void*) em38BackjumpAction, em, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);
                        }
                    }
                }
            }
        }
        break;
    }
}

// Branch check of the big slam: the slam sphere (table 1) at the tip parts 7..9 on motion event
// bit 0; a hit switches to T_CatchHit.
static void em38_R1_br_T_BigAtk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    if (em->Motion.Seq_old.Free & 1) {
        w->atkHit = 0;
        em38AtkCk(em, 1, 7);
        em38AtkCk(em, 1, 8);
        em38AtkCk(em, 1, 9);
        em38AtkCk(em, 1, 0xA);
        em38AtkCk(em, 1, 0xB);
        em38AtkCk(em, 1, 0xC);
        em38AtkCk(em, 1, 0xD);
        em38AtkCk(em, 1, 0xE);
        em38AtkCk(em, 1, 0xF);
        em38AtkCk(em, 1, 0x10);
        em38AtkCk(em, 1, 0x11);
        em38AtkCk(em, 1, 0x12);
        if (w->atkHit) {
            EmRoutineSetW(em, 1, 0xD, 0, 0);
        }
    }
}

// Tentacle routine 1/0xB: the big slam used while the body's head is up (each type has its own
// motion). After 20 frames, for 30 frames, while the player is within 6 m on this side, the action
// button offers the crouch (tentacle 1, em38SitAction) or the back jump (tentacle 2). Exits like
// T_Atk (the escape point is always awarded here).
static void em38_R1_T_BigAtk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    int end;

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->type == 2) {
            MotionSetCore(em, MOTION(em), ARC(0x3A), ARC(0x3B), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM38, 0x1B, 1, w->espKind, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x3C), ARC(0x3D), 10, 0x41, 0);
            EstSet(em, -1, 0, 0, EFF_EM38, 0x1A, 1, w->espKind, em, 0);
        }
        em38SetTentAtkWait(w);
        w->atkHit = 0;
        w->escaped = 0;
        w->timer = 30;
        w->timer2 = 20;
        w->actVar = Rnd() & 1;
        em->r_no_2++;
    case 1:
        end = MotionMove(em, 0);
        if (end) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            em->setRno(1, 5, 0, 0);
        } else if (em->hp <= 1) {
            em->setRno(1, 8, 0, 0);
        } else {
            if (pPL->r_no_0 == 1 || pPL->r_no_0 == 2 || (StaFlagChk(pG, STA_PL_CATCHED))) {
                w->escaped = 1;
            }
            if (w->timer2) {
                w->timer2--;
            } else if (w->timer && w->atkHit == 0 && w->escaped == 0) {
                w->timer--;
                if ((pPL->pos.x < 6000.0f && em->type == 1) || (pPL->pos.x > -6000.0f && em->type == 2)) {
                    if (pPL->r_no_0 == 4) {
                        if (em->type == 1) {
                            if (w->actVar) {
                                ActBtn.set(ACT_GUARD, 0xB, (void*) em38SitAction, em, ACTCTR_WEP_SET_IGNORE | ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);
                            } else {
                                ActBtn.set(ACT_GUARD, 0xB, (void*) em38SitAction, em, ACTCTR_WEP_SET_IGNORE | ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
                            }
                        } else {
                            if (w->actVar) {
                                ActBtn.set(ACT_GUARD, 0xB, (void*) em38BackjumpAction, em, ACTCTR_WEP_SET_IGNORE | ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);
                            } else {
                                ActBtn.set(ACT_GUARD, 0xB, (void*) em38BackjumpAction, em, ACTCTR_WEP_SET_IGNORE | ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
                            }
                        }
                    } else {
                        if (em->type == 1) {
                            if (w->actVar) {
                                ActBtn.set(ACT_GUARD, 0xB, (void*) em38SitAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
                            } else {
                                ActBtn.set(ACT_GUARD, 0xB, (void*) em38SitAction, em, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);
                            }
                        } else {
                            if (w->actVar) {
                                ActBtn.set(ACT_GUARD, 0xB, (void*) em38BackjumpAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
                            } else {
                                ActBtn.set(ACT_GUARD, 0xB, (void*) em38BackjumpAction, em, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);
                            }
                        }
                    }
                }
            }
        }
        break;
    }
}

// Tentacle routine 1/0xC: the slam at a player who is down below the platform. The stamp sphere
// (table 2) is tested at parts 7..0x12 on motion event bit 0 (plemDmStamp on a hit). Exits like T_Atk.
static void em38_R1_T_DownAtk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    int end;

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->type == 2) {
            MotionSetCore(em, MOTION(em), ARC(0x38), ARC(0x39), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM38, 0x18, 1, w->espKind, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x38), ARC(0x39), 10, 0x41, 0);
            EstSet(em, -1, 0, 0, EFF_EM38, 0x19, 1, w->espKind, em, 0);
        }
        em38SetTentAtkWait(w);
        w->atkHit = 0;
        em->r_no_2++;
    case 1:
        end = MotionMove(em, 0);
        if (end) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            em->setRno(1, 5, 0, 0);
        } else if (em->hp <= 1) {
            em->setRno(1, 8, 0, 0);
        } else if (em->Motion.Seq_old.Free & 1) {
            em38AtkCk(em, 2, 7);
            em38AtkCk(em, 2, 8);
            em38AtkCk(em, 2, 9);
            em38AtkCk(em, 2, 0xA);
            em38AtkCk(em, 2, 0xB);
            em38AtkCk(em, 2, 0xC);
            em38AtkCk(em, 2, 0xD);
            em38AtkCk(em, 2, 0xE);
            em38AtkCk(em, 2, 0xF);
            em38AtkCk(em, 2, 0x10);
            em38AtkCk(em, 2, 0x11);
            em38AtkCk(em, 2, 0x12);
        }
        break;
    }
}

// Tentacle routine 1/0xD: the slam caught the player. Plays the lift-and-throw motion (tentacle 1
// mirrored, the player's r_no_3 set so plem38_CatchHit uses the mirrored side), with the upper body's
// voice at frame 15 and the grab sound at frame 10, then T_Wait.
static void em38_R1_T_CatchHit(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->type == 2) {
            MotionSetCore(em, MOTION(em), ARC(0x3E), ARC(0x3F), 0, 1, 0);
            SetPlDamage(em, plem38_CatchHit);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x3E), ARC(0x3F), 0, 0x41, 0);
            SetPlDamage(em, plem38_CatchHit);
            pPL->r_no_3 = 1;
        }
        w->timer = 91;
        EstSet(em, -1, 0, 0, EFF_EM38, 7, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_frame > 14.7f && em->Motion.Seq_frame < 15.3f && w->pUpper) {
            Ctrl11SetSeEm38(w->pCtrl11, w->pUpper, 0x29);
            w->seWait = 180;
        }
        if (em->Motion.Seq_frame > 9.7f && em->Motion.Seq_frame < 10.3f) {
            SndCall(8, 0x26, &em->getPartsPtr(4)->world, em->id, 0, pPL);
        }
        if (MotionMove(em, 0)) {
            em->setRno(1, 5, 0, 0);
        }
        break;
    }
}

// Player damage callback of em38_R1_T_CatchHit: caught by a tentacle, then thrown.
static void plem38_CatchHit(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        pl->atari.off();
        if (pPL->r_no_3) {
            Vec v;

            v.x = -7405.05f;
            v.y = -2000.0f;
            v.z = 11349.85f;
            PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
            pl->ang.y = pl->pEmCatch->ang.y + PI;
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
            MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x47), 0, 0, 0x41, 0);
        } else {
            Vec v;

            v.x = 7405.05f;
            v.y = -2000.0f;
            v.z = 11349.85f;
            PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
            pl->ang.y = pl->pEmCatch->ang.y + PI;
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
            MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x47), 0, 0, 1, 0);
        }
        EstSet(pl, -1, 0, 0, EFF_EM38, 0xE, 0, ESP_CORE_KIND_NONE, pl, 0);
        PlSetFace(1);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0)) {
            LifeDownSet(pPL, 0x226, 0);
            if ((s16) pG->pl_life > 0) {
                pl->r_no_2++;
            }
        } else if (pl->Motion.Seq_frame > 87.7f && pl->Motion.Seq_frame < 88.3f) {
            PlSetDamageSe(0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
        }
        break;
    case 2:
        pl->atari.m_flag |= 0x300;
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x4C), 0, 0, 1, 0);
        SndCall(1, 0x29, &pl->getPartsPtr(0)->world, 0, 0, pPL);
        SndCall(1, 4, &pl->pos, 0, 0, pl);
        EstSet(pl, -1, 0, 0, EFF_EM38, 0x1C, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->r_no_2++;
    case 3:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            if (pl->Motion.Seq_frame > 21.7f && pl->Motion.Seq_frame < 22.3f) {
                SndCall(5, 2, &pl->pos, 0, 0, pl);
            }
            if (pl->Motion.Seq_frame > 19.7f && pl->Motion.Seq_frame < 20.3f) {
                SndCall(5, 3, &pl->pos, 0, 0, pl);
            }
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Upper body routine 1/0xE: idles on the body's head (em38UpperOnBody every frame).
static void em38_R1_U_Wait(cEm38* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x29), 0, 30, 5, 0);
        em38SearchParts(em);
        em->r_no_2++;
    case 1:
        em38UpperOnBody(em);
        MotionMove(em, 0);
        break;
    }
}

// Upper body routine 1/0xF (setCritical from the body's bite): the attack pose with its voice,
// then back to U_Wait.
static void em38_R1_U_Critical(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x2C), 0, 10, 1, 0);
        em38SearchParts(em);
        Ctrl11SetSeEm38(w->pCtrl11, em, 0x27);
        w->seWait = 180;
        em->r_no_2++;
    case 1:
        em38UpperOnBody(em);
        if (MotionMove(em, 0)) {
            em->setRno(1, 0xE, 0, 0);
        }
        break;
    }
}

// Lower body routine 1/0x10: its only state; loops the idle motion while em38RootMove swings the
// parasite roots.
static void em38_R1_L_Wait(cEm38* em)
{
    em38SearchParts(em);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x50), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
}

// Routine 2: damage reactions (r_no_1: 0 the body's down, 1 the upper body's flinch); flags 8
// keeps em38DmCk from re-triggering.
static void em38_R0_Damage(cEm38* em)
{
    EM38_WK(em)->flags |= 8;
    Em38_R2_move_tbl[em->r_no_1](em);
}

// Body routine 2/0: knocked down after 200 damage on the weak part (ckDown). Steps: the collapse
// (blend eased to centre; a closing shell is reset), then the shell opens for 240 frames (the first
// down uses the long opening mode 0xA, flags 0x100, later ones mode 2), the down loop for 190 frames,
// and the get-up; back to Wait with r_no_3 0xA.
static void em38_R1_Dm_Down(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    switch (em->r_no_2) {
    case 0:
        w->mot[0] = ARC(0x15);
        w->mot[1] = ARC(0x1A);
        w->mot[2] = ARC(0x1B);
        w->mot[3] = ARC(0x16);
        w->blendKind = 1;
        w->blendA = 30;
        w->blendB = 0;
        if (w->mode == 3 || w->mode == 5) {
            w->mode = 6;
        }
        EffectEspDelete(1, w->espKind, em, 0);
        EffectEspgenDelete(1, w->espKind, em);
        EffectEfmDelete(1, w->espKind, em);
        em->r_no_2++;
    case 1:
        w->blendRate *= 0.9f;
        em38BlendMotSet(em, w->mot[0], w->mot[1], w->mot[2], w->mot[3], 0, 0, w->blendKind);
        if (MotionMove(em, 0)) {
            if (w->mode == 1 || w->mode == 7) {
                if (w->flags & 0x100) {
                    w->mode = 2;
                } else {
                    w->mode = 0xA;
                    w->flags |= 0x100;
                }
            }
            w->shellTimer = 240;
            w->blendRate = 0.0f;
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 3, 5, 0);
        w->timer = 190;
        em->r_no_2++;
    case 3:
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x18), ARC(0x19), 3, 1, 0);
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            em->setRno(1, 0, 0, 0xA);
        }
        break;
    }
}

// Upper body routine 2/1: one of two flinch motions (coin flip) riding on the body, then U_Wait.
static void em38_R1_Dm_Upper(cEm38* em)
{
    switch (em->r_no_2) {
    case 0:
        if ((u8) (Rnd() % 10) > 4) {
            MotionSetCore(em, MOTION(em), ARC(0x2A), 0, 5, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x2B), 0, 5, 1, 0);
        }
        em38SearchParts(em);
        em->r_no_2++;
    case 1:
        em38UpperOnBody(em);
        if (MotionMove(em, 0)) {
            em->setRno(1, 0xE, 0, 0);
        }
        break;
    }
}

// Routine 3: death (r_no_1: 0 the body, 1 the upper body), entered from em38DmCk when the upper
// body's HP reaches 1.
static void em38_R0_Die(cEm38* em)
{
    EM38_WK(em)->flags |= 8;
    Em38_R3_move_tbl[em->r_no_1](em);
}

// Body routine 3/0: the death motion with the death stream sound, the body's effects deleted and
// the collision passed through; at frame 200 the shell switches to its death mode 8.
static void em38_R1_Die_Body(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x28), 0, 3, 1, 0);
        em->clearStatus(EM_STATUS_ACTIVE);
        EffectEspDelete(1, w->espKind2, em, 0);
        EffectEspgenDelete(1, w->espKind2, em);
        EffectEfmDelete(1, w->espKind2, em);
        SndStrReq(1, 0x30, 0x80000003, 0, 0, 0.0f);
        em->atari.off();
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0) == 0) {
            if (em->Motion.Seq_frame > 199.7f && em->Motion.Seq_frame < 200.3f) {
                w->mode = 8;
            }
        }
        break;
    }
}

// Upper body routine 3/1: the death motion riding on the body; the collision's player-block bits
// are cleared once it ends.
static void em38_R1_Die_Upper(cEm38* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x2E), 0, 3, 1, 0);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->r_no_2++;
    case 1:
        em38UpperOnBody(em);
        if (MotionMove(em, 0)) {
            em->atari.m_flag &= ~0x300;
        }
        break;
    }
}

// Per-frame target selection while alive: routes to the player (flags bit 0 when a route exists;
// routeAng is the yaw to the route point, zero during init) and copies it as the target; when a
// partner is present (flags bit 1) and the player is unreachable or farther, the partner's route
// becomes the target (flags bit 2).
void em38RouteCk(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->r_no_0 == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->l_pl = 100000000.0f;
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->l_pl;
    w->pTarget = pPL;
    w->flags &= ~4;
    if (w->flags & 2) {
        if (!(w->flags & 1) || em->l_pl > em->l_sub) {
            w->targetPos = w->subRoutePos;
            w->targetAng = w->subAng;
            w->targetAngAbs = w->subAngAbs;
            w->targetDist = em->l_sub;
            w->pTarget = pSUB;
            w->flags |= 4;
        }
    }
}

// Eyes of the body (parts 0x3A) follow the player's head.
void em38EyeMove(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    if (em->type == 0) {
        Vec b;
        Vec c;
        Vec a;
        cParts* p;
        f32 ang;
        f32 len;
        f32 e;

        p = (cParts*) em->getPartsPtr(0x1F);
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 1.0f;
        PSMTXMultVecSR(p->mat, &a, &a);
        ang = atan2f(a.x, a.z);
        p = (cParts*) em->getPartsPtr(0x3A);
        b = p->world;
        c = pPL->getPartsPtr(4)->world;
        w->eyeY = w->eyeY * 0.5f + Muku(&b, &c, ang, 1.0471976f) * 0.5f;
        PSVECSubtract(&c, &p->world, &a);
        len = SQRTF(a.x * a.x + a.z * a.z);
        e = -atan2f(a.y, len);
        if (e > 0.7853982f) {
            e = 0.7853982f;
        }
        if (e < -0.7853982f) {
            e = -0.7853982f;
        }
        w->eyeX = w->eyeX * 0.5f + e * 0.5f;
        p->motParts.flags |= 0x40000000;
        p->inv_offset.x = w->eyeX;
        p->inv_offset.y = w->eyeY;
        p->inv_offset.z = 0.0f;
    }
}


// Finds the other parts of the boss among the alive enemies.
void em38SearchParts(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm38* p = (cEm38*) EmMgr.fastAt(i);

        if (p->isAlive() && p->id == 0x38 && p != em) {
            switch (p->type) {
            case 0:
                if (w->pBody == 0) {
                    w->pBody = p;
                }
                break;
            case 1:
                if (w->pTent[0] == 0) {
                    w->pTent[0] = p;
                    w->para[0].pEm = p;
                }
                break;
            case 2:
                if (w->pTent[1] == 0) {
                    w->pTent[1] = p;
                    w->para[1].pEm = p;
                }
                break;
            case 3:
                if (w->pUpper == 0) {
                    w->pUpper = p;
                }
                break;
            case 4:
                if (w->pLower == 0) {
                    w->pLower = p;
                }
                break;
            }
        }
    }
}

// The upper body rides on parts 4 of the body.
void em38UpperOnBody(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    em->Motion.Mot_flag &= ~0x40000000;
    if (em->type == 3 && w->pBody) {
        cParts* p = w->pBody->getPartsPtr(4);

        em->pos.x = 0.0f;
        em->pos.y = -128.0f;
        em->pos.z = 1450.0f;
        em->ang.x = 0.0f;
        em->ang.y = 0.0f;
        em->ang.z = 0.0f;
        RotMatrix(em->mat, &em->ang);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        PSMTXConcat(p->mat, em->mat, em->mat);
        em->Motion.Mot_flag |= 0x40000000;
    }
}

// The parasite enemy (em25 module): only the two virtuals this module calls, by slot.
class cEm25 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM25_WK)
    virtual void v09();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void v14();
    virtual void v15();
    virtual void v16();
    virtual void v17();
    virtual void v18();
    virtual void v19();
    virtual int isFree();
    virtual void setBirth(Vec* pos, f32 ang);
};

// The lower body releases a parasite (em25) from one of three points every 240 frames.
void em38BirthParasite(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    u32 i;

    if (w->birthTimer == 0 && em->type == 4 && em->hp > 0) {
        for (i = 0; i < EmMgr.getArrayNum(); i++) {
            cEm25* p = (cEm25*) (cEm38*) EmMgr.fastAt(i);

            // isAlive() (not the open-coded flag test): the inline's extra RTL keeps the loop
            // above loop.c's 71-insn threshold in pass 2, so `li 240` stays inside the loop
            if (p->isAlive() && p->id == 0x25) {
                if (p->isFree()) {
                    Vec v;

                    switch ((u8) (Rnd() % 3)) {
                    case 2:
                        v.x = 3540.0f;
                        v.y = 0.0f;
                        v.z = -15050.0f;
                        break;
                    case 1:
                        v.x = 0.0f;
                        v.y = 0.0f;
                        v.z = -12450.0f;
                        break;
                    case 0:
                    default:
                        v.x = 3540.0f;
                        v.y = 0.0f;
                        v.z = -15050.0f;
                        break;
                    }
                    p->setBirth(&v, 0.0f);
                    w->birthTimer = 240;
                    return;
                }
            }
        }
    }
}

// Debug switch: the shell motion stays off while set.
static int em38_shell_off = 0;
// The original link 8-aligns the end of .data (the ngcld BSS tag follows): the 4 pad bytes after it.
asm(".section .data\n\t.balign 8\n\t.text");

// The shell on the body's back (a second motion work) opens when the body is hurt.
void em38ShellControl(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    if (em38_shell_off) {
        return;
    }
    if (em->type != 0) {
        return;
    }
    switch (w->mode) {
    case 0:
        w->shellMot.Seq_speed = 1.0f;
        MotionSetCore(em, &w->shellMot, ARC(0x41), 0, 0, 4, 0);
        w->mode++;
    case 1:
        MotionMoveCore(em, SHELL_MOT(w), 0);
        MotionSequenceCtrl(SHELL_MOT(w));
        if (em->hp <= 0) {
            w->mode = 2;
        }
        break;
    case 2:
        w->shellMot.Seq_speed = 1.0f;
        MotionSetCore(em, &w->shellMot, ARC(0x42), 0, 0, 0, 0);
        w->mode++;
        SndCall(8, 0x12, &em->pos, em->id, 0, em);
        EstSet(em, -1, 0, 0, EFF_EM38, 5, 0, ESP_CORE_KIND_NONE, em, 0);
    case 3:
        MotionMoveCore(em, SHELL_MOT(w), 0);
        if (MotionSequenceCtrl(SHELL_MOT(w))) {
            w->mode++;
        }
        break;
    case 4:
        w->shellMot.Seq_speed = 1.0f;
        MotionSetCore(em, &w->shellMot, ARC(0x40), 0, 0, 4, 0);
        w->mode++;
    case 5:
        MotionMoveCore(em, SHELL_MOT(w), 0);
        MotionSequenceCtrl(SHELL_MOT(w));
        if (em->hp > 0) {
            if (w->shellTimer) {
                w->shellTimer--;
            } else {
                w->mode++;
            }
        }
        break;
    case 6:
        w->shellMot.Seq_speed = 1.0f;
        MotionSetCore(em, &w->shellMot, ARC(0x43), 0, 0, 0, 0);
        SndCall(8, 0x13, &em->pos, em->id, 0, em);
        w->mode++;
    case 7:
        MotionMoveCore(em, SHELL_MOT(w), 0);
        if (MotionSequenceCtrl(SHELL_MOT(w))) {
            w->mode = 0;
        } else if (em->hp <= 0) {
            w->mode = 2;
        }
        break;
    case 8:
        w->shellMot.Seq_speed = 1.0f;
        MotionSetCore(em, &w->shellMot, ARC(0x44), 0, 0, 0, 0);
        w->mode++;
    case 9:
        MotionMoveCore(em, SHELL_MOT(w), 0);
        MotionSequenceCtrl(SHELL_MOT(w));
        break;
    case 0xA:
        w->shellMot.Seq_speed = 1.0f;
        MotionSetCore(em, &w->shellMot, ARC(0x42), 0, 0, 0, 0);
        w->mode++;
        SndCall(8, 0x12, &em->pos, em->id, 0, em);
        EstSet(em, -1, 0, 0, EFF_EM38, 0x24, 0, ESP_CORE_KIND_NONE, em, 0);
    case 0xB:
        MotionMoveCore(em, SHELL_MOT(w), 0);
        if (MotionSequenceCtrl(SHELL_MOT(w))) {
            w->mode = 4;
        }
        break;
    }
}

// Motion m0 (m3 as its sequence) blended with m1 (rate > 0) or m2 by |blendRate| / 256.
void em38BlendMotSet(cEm38* em, void* m0, void* m1, void* m2, void* m3, int a, int b, int kind)
{
    Em38Work* w = EM38_WK(em);
    f32 rate = fabsf(w->blendRate);
    MotionWorkSub* bm;
    void* m;
    int seq;

    MotionSetCore(em, MOTION(em), m0, m3, (u8) w->blendA, (u16) kind, (u16) w->blendB);
    if (w->blendRate > 0.0f) {
        m = m1;
        seq = a;
    } else {
        m = m2;
        seq = b;
    }
    bm = &w->blendMot;
    MotionSetCore(em, bm, m, (void*) seq, (u8) w->blendA, (u16) kind, (u16) w->blendB);
    em->Motion.blend = bm;
    bm->Brate = rate * (1.0f / 256.0f);
    if (w->blendA) {
        w->blendA--;
    }
    w->blendB++;
    if (w->blendB >= em->Motion.Seq_frame_num) {
        w->blendB = 0;
    }
}

// Hit feedback of the pending damage at the hit part: the body's weak part 0x3B gets a special
// sound and a splash effect sized by weapon class (handgun / rapid fire / shotgun by range / heavy),
// other body parts and the lower body's part 3 the hard-shell clang with sparks, and everything else
// the blood effect sized the same way (the upper body with its own hit sound). Knife (0x14) and
// weapon 0 give no effect.
void em38BloodSet(cEm38* em)
{
    YARARE_INFO* part = em->dmg.m_pDamageYarare;
    cParts* p;
    int near;

    near = 0;
    if (part->len < 36000000.0f) {
        near = 1;
    }
    p = em->getPartsPtr(part->parts_no - 1);
    if (em->type == 0) {
        if (part->parts_no == 0x3B) {
            SndCall(8, 5, &p->world, em->id, 0, em);
            switch (em->dmg.m_Wep) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 0xE:
            case 0x10:
            case 0x11:
            case 0x26:
            case 0x2B:
                EstSet(em, -1, 0, 0, EFF_EM38, 9, 0, ESP_CORE_KIND_NONE, em, 0);
                break;
            case 0xB:
            case 0xC:
            case 0x1B:
            case 0x1D:
            case 0x27:
                EstSet(em, -1, 0, 0, EFF_EM38, 0xA, 0, ESP_CORE_KIND_NONE, em, 0);
                break;
            case 7:
            case 8:
            case 0x21:
                if (near == 0) {
                    EstSet(em, -1, 0, 0, EFF_EM38, 9, 0, ESP_CORE_KIND_NONE, em, 0);
                } else {
                    EstSet(em, -1, 0, 0, EFF_EM38, 0xB, 0, ESP_CORE_KIND_NONE, em, 0);
                }
                break;
            case 5:
            case 6:
            case 9:
            case 0xA:
            case 0xD:
            case 0xF:
            case 0x12:
            case 0x13:
            case 0x15:
            case 0x28:
            case 0x29:
            case 0x2C:
            case 0x2D:
                EstSet(em, -1, 0, 0, EFF_EM38, 0xB, 0, ESP_CORE_KIND_NONE, em, 0);
                break;
            case 0:
            case 0x14:
                break;
            }
        } else {
            SndCall(8, 0x1A, &p->world, em->id, 0, em);
            EmDmBloodSet2(em, 0x2E, 3, 0, 0, 0);
        }
    } else if (em->type == 4 && part->parts_no == 3) {
        SndCall(8, 0x1A, &p->world, em->id, 0, em);
        EmDmBloodSet2(em, 0x2E, 3, 0, 0, 0);
    } else {
        if (em->type == 1 || em->type == 2 || em->type == 4) {
            SndCall(8, 5, &p->world, em->id, 0, em);
        }
        if (em->type == 3) {
            SndCall(8, 0x1B, &p->world, em->id, 0, em);
        }
        switch (em->dmg.m_Wep) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 0xE:
        case 0x10:
        case 0x11:
        case 0x26:
        case 0x2B:
            EmDmBloodSet2(em, 0x2E, 0, 0, 0, 0);
            break;
        case 0xB:
        case 0xC:
        case 0x1B:
        case 0x1D:
        case 0x27:
            EmDmBloodSet2(em, 0x2E, 1, 0, 0, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (near) {
                EmDmBloodSet2(em, 0x2E, 2, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x2E, 0, 0, 0, 0);
            }
            break;
        case 5:
        case 6:
        case 9:
        case 0xA:
        case 0xD:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x15:
        case 0x28:
        case 0x29:
        case 0x2C:
        case 0x2D:
            EmDmBloodSet2(em, 0x2E, 2, 0, 0, 0);
            break;
        case 0:
        case 0x14:
            break;
        }
    }
}

// Hit boxes of the body: the root sphere plus the extra boxes along the neck, head and back (the
// weak part 0x3B among them).
void em38YarareInitBody(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    YarareInit(em, 0.0f, 0.0f, 0.0f, 1000.0f, 1000.0f, 3, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 1000.0f, 1000.0f, 4, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 1000.0f, 500.0f, 5, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[2], 0.0f, -800.0f, 1000.0f, 1100.0f, 1000.0f, 5, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 500.0f, 1000.0f, 0x12, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 500.0f, 1000.0f, 0x13, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 500.0f, 1000.0f, 0x14, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 500.0f, 1000.0f, 0x15, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[7], 0.0f, 0.0f, 0.0f, 500.0f, 1000.0f, 0x16, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 500.0f, 1000.0f, 0x17, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 500.0f, 1000.0f, 0x18, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[10], 0.0f, 0.0f, 0.0f, 500.0f, 1000.0f, 0x19, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[11], 0.0f, -100.0f, 0.0f, 650.0f, 900.0f, 0x1A, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAddCube(em, &w->hit[12], -500.0f, -200.0f, 0.0f, 500.0f, 200.0f, 500.0f, 6, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[13], -500.0f, -200.0f, 400.0f, 500.0f, 200.0f, 1200.0f, 7, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[14], -500.0f, -100.0f, 200.0f, 500.0f, 200.0f, 900.0f, 8, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[15], -500.0f, -200.0f, 0.0f, 700.0f, 200.0f, 900.0f, 9, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[16], -500.0f, -200.0f, 200.0f, 700.0f, 200.0f, 1000.0f, 0xA, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[17], -500.0f, -100.0f, 300.0f, 700.0f, 200.0f, 700.0f, 0xB, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[18], 500.0f, -200.0f, 0.0f, 500.0f, 200.0f, 700.0f, 0xC, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[19], 500.0f, -200.0f, 400.0f, 500.0f, 200.0f, 1200.0f, 0xD, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[20], 500.0f, -100.0f, 200.0f, 500.0f, 200.0f, 900.0f, 0xE, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[21], 500.0f, -200.0f, 0.0f, 700.0f, 200.0f, 900.0f, 0xF, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[22], 500.0f, -200.0f, 0.0f, 700.0f, 200.0f, 1000.0f, 0x10, YAT_FLAG_ON);
    YarareAddCube(em, &w->hit[23], 500.0f, -100.0f, 0.0f, 700.0f, 200.0f, 700.0f, 0x11, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[24], 0.0f, 0.0f, 20.0f, 320.0f, 0.0f, 0x3B, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[25], 300.0f, -100.0f, 600.0f, 450.0f, 900.0f, 0x1B, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
}

// Hit boxes of a tentacle: the root sphere plus one box per segment.
void em38YarareInitTentacle(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    YarareInit(em, 0.0f, 0.0f, 0.0f, 1000.0f, 2000.0f, 2, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 950.0f, 1900.0f, 3, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 900.0f, 1800.0f, 4, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 860.0f, 1700.0f, 5, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 820.0f, 1600.0f, 6, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 800.0f, 1500.0f, 7, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 760.0f, 1400.0f, 8, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 720.0f, 1300.0f, 9, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[7], 0.0f, 0.0f, 0.0f, 680.0f, 1200.0f, 0xA, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 640.0f, 1100.0f, 0xB, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 600.0f, 1000.0f, 0xC, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[10], 0.0f, 0.0f, 0.0f, 560.0f, 900.0f, 0xD, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[11], 0.0f, 0.0f, 0.0f, 520.0f, 800.0f, 0xE, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[12], 0.0f, 0.0f, 0.0f, 480.0f, 700.0f, 0xF, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[13], 0.0f, 0.0f, 0.0f, 440.0f, 600.0f, 0x10, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[14], 0.0f, 0.0f, 0.0f, 400.0f, 500.0f, 0x11, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[15], 0.0f, 0.0f, 0.0f, 380.0f, 400.0f, 0x12, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
}

// Hit boxes of the upper body: a human-sized root sphere plus the torso / head boxes.
void em38YarareInitUpper(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    YarareInit(em, 0.0f, 0.0f, 0.0f, 400.0f, 200.0f, 2, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 400.0f, 100.0f, 3, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 300.0f, 200.0f, 6, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[2], 0.0f, -300.0f, 0.0f, 150.0f, 300.0f, 0xB, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[3], 0.0f, -300.0f, 0.0f, 150.0f, 300.0f, 0xC, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[4], 0.0f, -300.0f, 0.0f, 150.0f, 300.0f, 0xD, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x11, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 120.0f, 400.0f, 0x12, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[7], 0.0f, 0.0f, 0.0f, 150.0f, 50.0f, 0x14, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
}

// Hit boxes of the lower body: a 4 x 8 x 3 m root cube plus the boxes of the two parasite roots
// (parts + 1 / parts + 2 of each, which em38DmCk routes to the root's hp).
void em38YarareInitLower(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    YarareInitCube(em, 0.0f, -2000.0f, 0.0f, 2000.0f, 4000.0f, 1500.0f, 3, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[0], 0.0f, 500.0f, 0.0f, 2800.0f, 0.0f, 0x10, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], 0.0f, 500.0f, 0.0f, 2800.0f, 0.0f, 0x12, YAT_FLAG_ON);
}

// Damage of the pending hit: the weapon table value (near = within 6 m for the range falloff), 20
// for weapon ids past 0x2D.
int em38SetDmVal(cEm38* em)
{
    int near;
    int dmg;

    near = 0;
    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    {
        u32 no = em->dmg.m_Wep;

        dmg = 20;
        if (no <= 0x2D) {
            dmg = GetWepDmVal(em, no, near);
        }
    }
    return dmg;
}

// Tests attack `no` swept from part `parts`' previous to its current world position.
int em38AtkCk(cEm38* em, int no, int parts)
{
    cParts* p = em->getPartsPtr(parts);

    return em38AtkCk2(em, no, &p->world, &p->world_old);
}

// Attack sphere `no` of em38_atk_tbl swept from `a` to `b` against the player (once per attack:
// atkHit). A player hit: the sweep / stamp / catch take the player over with plemDmStamp, the bite
// (3) kills outright (Debug_flg[2] 0x00800000 disables it). Any hit shakes the camera and rumbles.
// Returns 1 on a hit.
int em38AtkCk2(cEm38* em, u32 no, Vec* a, Vec* b)
{
    Em38Work* w = EM38_WK(em);
    int hit;

    if (w->atkHit) {
        return 0;
    }
    hit = EmAtkHitCk(&em38_atk_tbl[no], a, b, 1);
    if (hit) {
        if (hit & 1) {
            w->atkHit = 1;
            switch (no) {
            case 0:
            case 2:
                SetPlDamage(em, plemDmStamp);
                SndCall(8, 0xD, &em->pos, em->id, 0, em);
                break;
            case 3:
                if (!DbgFlagChk(pG, DBG_NO_DEATH)) {
                    pG->pl_life = 0;
                }
                break;
            case 4:
                // Allocation lever (loop notes, no code): the 8th weighted `em` ref ranks em above
                // `no` in global-alloc (em r31, no r30).
                do { SetPlDamage(em, plemDmStamp); } while (0);
                break;
            }
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
        return 1;
    }
    return 0;
}

// Player damage callback of the stamp / sweep attacks (em38AtkCk2).
static void plemDmStamp(cPlayer* pl)
{
    pl->dmg.set(0, 10);
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x49), 0, 5, 1, 0);
            EstSet(pl, -1, 0, 0, EFF_EM38, 0xD, 0, ESP_CORE_KIND_NONE, pl, 0);
            PlSetDamageSe(0xD);
        } else {
            MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x48), 0, 5, 1, 0);
            EstSet(pl, -1, 0, 0, EFF_EM38, 0xC, 0, ESP_CORE_KIND_NONE, pl, 0);
            PlSetDamageSe(0);
        }
        PlSetFace(1);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0) && (s16) pG->pl_life > 0) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            if (pl->Motion.Seq_frame > 4.7f && pl->Motion.Seq_frame < 5.3f) {
                SndCall(5, 5, &pl->pos, 0, 0, pl);
            }
            if ((s16) pG->pl_life > 0) {
                if ((pl->Motion.Seq_frame > 87.7f && pl->Motion.Seq_frame < 88.3f) || (pl->Motion.Seq_frame > 116.7f && pl->Motion.Seq_frame < 117.3f)) {
                    SndCall(5, 0, &pl->pos, 0, 0, pl);
                }
                if ((pl->Motion.Seq_frame > 106.7f && pl->Motion.Seq_frame < 107.3f) || (pl->Motion.Seq_frame > 130.7f && pl->Motion.Seq_frame < 131.3f)) {
                    SndCall(5, 1, &pl->pos, 0, 0, pl);
                }
                if (pl->Motion.Seq_frame > 59.7f && pl->Motion.Seq_frame < 60.3f) {
                    SndCall(1, 4, &pl->pos, 0, 0, pl);
                    SndCall(1, 0x29, &pl->getPartsPtr(0)->world, 0, 0, pPL);
                }
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Body only: 1 while the head is reared (em38_R1_HeadUp), queried by the tentacles.
int cEm38::ckHeadUp()
{
    if (type != 0) {
        return 0;
    }
    if ((EM38_WK(this)->flags & 0x20) == 0) {
        return 0;
    }
    return 1;
}

// Pushes this part's next attack `frames` out (the body staggers the tentacles with it).
void cEm38::setAtkWait(int frames)
{
    EM38_WK(this)->atkWait = frames;
}

// Action button of em38_R1_T_Atk: the player ducks under the tentacle.
static void em38EscapeAction(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    SetPlDamage(em, plemEscape);
    w->escaped = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player damage callback of the duck (em38EscapeAction, dmType 0x1E: invulnerable): probes 2 m to
// each side for walls and picks the free side (random when both are free) for the dodge roll under
// the tentacle, with the event camera (em38EscapeCamMove); ends when the motion finishes.
static void plemEscape(cPlayer* pEm)
{
    pEm->subArc = pEm->pEmCatch->subArc;
    pEm->dmg.m_Timer = 0x1E;
    switch (pEm->r_no_2) {
    case 0: {
        Vec a;
        Vec b;
        int side;
        int hit;

        if (Muku(&pEm->pEmCatch->pos, &pEm->pos, pEm->ang.y, PI) > 0.0f) {
            side = 1;
        } else {
            side = 0;
        }
        side = Rnd() & 1;
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        hit = 0;
        PSMTXMultVec(pEm->mat, &a, &a);
        PSMTXMultVec(pEm->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            hit |= 1;
        }
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = -2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(pEm->mat, &a, &a);
        PSMTXMultVec(pEm->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            hit |= 2;
        }
        if (hit & 1) {
            side = 1;
        }
        if (hit & 2) {
            side = 0;
        }
        if (side) {
            MotionSetCore(pEm, MOTION(pEm), EM_ARC(pEm, 0x4A), EM_ARC(pEm, 0x4B), 5, 1, 0);
        } else {
            MotionSetCore(pEm, MOTION(pEm), EM_ARC(pEm, 0x4A), EM_ARC(pEm, 0x4B), 5, 0x41, 0);
        }
        GameAddPoint(LVADD_ESCAPEATTACK);
        SndCall(1, 0x48, &pEm->pos, 0, 0, pEm);
        SndCall(1, 0x11, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        pEm->m_Work0 = 50;
        pEm->m_Work1 = 15;
        pEm->r_no_2++;
    }
    case 1:
        if (pEm->m_Work2) {
            em38EscapeCamMove((cEm38*)pEm->pEmCatch);
            if (pEm->Motion.Seq_frame > 11.7f && pEm->Motion.Seq_frame < 12.3f) {
                EstSet(0, -1, &pEm->pos, 0, EFF_PL00, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
                SndCall(5, 5, &pEm->pos, 0, 0, pEm);
            }
        }
        if (pEm->m_Work1) {
            pEm->ang.y += Muku(&pEm->pos, &pEm->pEmCatch->pos, pEm->ang.y, 0.3926991f);
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        }
        if (MotionMove(pEm, 0)) {
            pEm->m_Work0 = 0;
        }
        if (pEm->m_Work0) {
            pEm->m_Work0--;
        } else {
            EndPlDamage();
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Event camera of the escape scenes: behind the player, pulled in to the scenario hit.
void em38EscapeCamMove(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    GlobalWork* g = pG;
    Vec a;
    Vec b;
    Vec c;

    // volatile store: keeps the w-relative address (a reference setter folds it to em+0xCA8) and
    // orders every later memory op behind it, which issues the store before the pool loads
    *(volatile f32*) &w->cam.param.fovy = g->Camera.param.fovy;
    a.x = -376.0f;
    a.y = 575.0f;
    a.z = -1831.0f;
    b.x = -244.0f;
    b.y = 809.0f;
    b.z = 52.6f;
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    PosToPos(&g->Camera.param.at, &b, &w->cam.param.at, 1.0f);
    PosToPos(&g->Camera.param.pos, &a, &w->cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &c, 0, 0x8000, 0)) {
        Vec d;
        f32 len;

        PSVECSubtract(&c, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 3806 "D:/Bio4/Prog/em38.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->cam.param.at, &d, &w->cam.param.pos);
    }
    w->cam.Up.x = 0.0f;
    w->cam.Up.y = 1.0f;
    w->cam.Up.z = 0.0f;
    w->cam.Distance = VEC_DIST(&w->cam.param.pos, &w->cam.param.at);
    CameraSetOrientationUp(&w->cam);
    CamCtrl.SetExtraCamera(&w->cam);
}

// Action button of em38_R1_T_MdlAtk / T_BigAtk: the player jumps back.
static void em38BackjumpAction(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    SetPlDamage(em, plemBackjump);
    w->escaped = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player damage callback of the back jump (em38BackjumpAction, dmType 0x1E: invulnerable): the
// jump-back motion from frame 5, turning to face the tentacle for the first 15 frames, with the
// jump / landing sounds; ends when the motion finishes.
static void plemBackjump(cPlayer* pEm)
{
    pEm->subArc = pEm->pEmCatch->subArc;
    pEm->dmg.m_Timer = 0x1E;
    switch (pEm->r_no_2) {
    case 0:
        MotionSetCore(pEm, MOTION(pEm), EM_ARC(pEm, 0x4E), 0, 5, 1, 5);
        EstSet(pEm, -1, 0, 0, EFF_PL00, 0x14, 0, ESP_CORE_KIND_NONE, pEm, 0);
        SndCall(1, 0x43, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        SndCall(1, 0x44, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        pEm->m_Work0 = 50;
        pEm->m_Work1 = 15;
        pEm->m_Work2 = 0;
        pEm->r_no_2++;
    case 1:
        if (pEm->m_Work1) {
            pEm->ang.y += Muku(&pEm->pos, &pEm->pEmCatch->pos, pEm->ang.y, 0.3926991f);
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        }
        if (pEm->Motion.Seq_frame > 10.7f && pEm->Motion.Seq_frame < 11.3f) {
            SndCall(1, 0x4F, &pEm->pos, 0, 0, pEm);
        }
        if (pEm->Motion.Seq_frame > 21.7f && pEm->Motion.Seq_frame < 22.3f) {
            SndCall(5, 0x14, &pEm->pos, 0, 0, pEm);
        }
        if ((pEm->Motion.Seq_frame > 36.7f && pEm->Motion.Seq_frame < 37.3f) || (pEm->Motion.Seq_frame > 49.7f && pEm->Motion.Seq_frame < 50.3f)) {
            SndCall(5, 2, &pEm->pos, 0, 0, pEm);
        }
        if ((pEm->Motion.Seq_frame > 37.7f && pEm->Motion.Seq_frame < 38.3f) || (pEm->Motion.Seq_frame > 50.7f && pEm->Motion.Seq_frame < 51.3f)) {
            SndCall(5, 3, &pEm->pos, 0, 0, pEm);
        }
        if (MotionMove(pEm, 0)) {
            pEm->m_Work0 = 0;
        }
        if (pEm->m_Work0) {
            pEm->m_Work0--;
        } else {
            EndPlDamage();
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Action button of em38_R1_T_BigAtk (tentacle 1): the player crouches.
static void em38SitAction(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    SetPlDamage(em, plemSit);
    w->escaped = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player damage callback of the crouch (em38SitAction): the duck motion under tentacle 1's big
// slam, an escape rank point, and the damage ends with the motion.
static void plemSit(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 30);
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x4F), 0, 5, 1, 0);
        GameAddPoint(LVADD_ESCAPEATTACK);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// The two parasite roots of the lower body (parts 0xF / 0x10 and 0x11 / 0x12).
void em38RootInit(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    if (em->type != 4) {
        return;
    }
    w->para[0].state = 0;
    w->para[0].x1 = 0;
    w->para[0].parts = 0xF;
    w->para[0].parts2 = 0x10;
    w->para[0].timer = 600;
    w->para[0].effTimer = 0;
    w->para[0].swingAng = 0;
    w->para[0].phase = 0.0f;
    w->para[0].ang = 0.0f;
    w->para[0].pEm = 0;
    w->para[1].state = 0;
    w->para[1].x1 = 0;
    w->para[1].parts = 0x11;
    w->para[1].parts2 = 0x12;
    w->para[1].timer = 600;
    w->para[1].effTimer = 0;
    w->para[1].swingAng = 0;
    w->para[1].phase = 0.0f;
    w->para[1].ang = 0.0f;
    w->para[1].pEm = 0;
}

// Lower body only, per frame while it and the body live: the state machine of each parasite root.
// 0/1 wait 750..1050 frames, 2/3 rise over 120 frames (the tentacle surfaces at 30 left: setIn, root
// hp 200), 4/5 stay out until the tentacle is beaten to 1 HP, 6/7 sink over 120 frames and restart.
// Rising winds the root's swing forward (angSpd up to 5 deg/frame), sinking winds it back, idle
// decays it; a fast swing splashes every 7 frames. The swing is laid out as a 30-degree stepped
// angle over the root's two parts (swingAng / 30 whole steps plus the remainder split between them,
// mirrored for root 0) with a breathing y scale, written into the parts' local matrices.
void em38RootMove(cEm38* em)
{
    Em38Work* w = EM38_WK(em);
    u32 i;

    if (em->type != 4) {
        return;
    }
    if (em->hp <= 0) {
        return;
    }
    if (w->pBody == 0) {
        return;
    }
    for (i = 0; i < 2; i++) {
        Em38Para* p = &w->para[i];
        int dir;

        if (p->pEm == 0) {
            continue;
        }
        dir = 0;
        switch (p->state) {
        case 0:
            p->timer = (int) Rnd() % 300 + 750;
            p->state++;
        case 1:
            if (p->timer) {
                p->timer--;
            } else {
                p->state++;
            }
            break;
        case 2:
            p->riseTimer = 120;
            p->hp = 200;
            p->state++;
        case 3:
            dir = 1;
            if (p->riseTimer) {
                p->riseTimer--;
                if (p->riseTimer == 30) {
                    p->pEm->setIn();
                }
            } else {
                p->state++;
            }
            break;
        case 4:
            p->state++;
        case 5:
            if (p->pEm->hp <= 1) {
                p->state++;
            }
            break;
        case 6:
            p->riseTimer = 120;
            p->state++;
        case 7:
            dir = 2;
            if (p->riseTimer) {
                p->riseTimer--;
            } else {
                p->state = 0;
            }
            break;
        }
        switch (dir) {
        case 0:
        default:
            p->angSpd *= 0.95f;
            break;
        case 1:
            p->swingAng++;
            if (p->swingAng > 359) {
                p->swingAng = 0;
            }
            p->angSpd += 0.0017453292f;
            if (p->angSpd > 0.08726646f) {
                p->angSpd = 0.08726646f;
            }
            break;
        case 2:
            p->swingAng--;
            if (p->swingAng < 0) {
                p->swingAng = 359;
            }
            p->angSpd -= 0.017453292f;
            if (p->angSpd < -0.08726646f) {
                p->angSpd = -0.08726646f;
            }
            break;
        }
        p->ang += p->angSpd;
        p->ang = LIMIT_ANGLE(p->ang);
        if (w->pBody->hp > 0 && fabsf(p->angSpd) > 0.034906585f) {
            if (p->effTimer) {
                p->effTimer--;
            } else {
                p->effTimer = 7;
                if (i == 0) {
                    EstSet(0, -1, 0, 0, EFF_EM38, 0x1D, 0, ESP_CORE_KIND_NONE, 0, 0);
                } else {
                    EstSet(0, -1, 0, 0, EFF_EM38, 0x1E, 0, ESP_CORE_KIND_NONE, 0, 0);
                }
            }
        }
        {
            s16 q = p->swingAng / 30;
            s16 rem = p->swingAng % 30;
            f32 base = (f32) q * 0.5235988f + p->ang;
            f32 a1;
            f32 a2;
            f32 scale;
            cParts* m;

            if (rem > 14) {
                a1 = 0.5235988f;
            } else {
                a1 = (f32) rem * 0.034906585f;
            }
            if (rem <= 14) {
                a2 = 0.0f;
            } else {
                a2 = (f32) (rem - 15) * 0.034906585f;
            }
            a2 += base;
            a1 = LIMIT_ANGLE(a1 + base);
            a2 = LIMIT_ANGLE(a2);
            if (i == 0) {
                a1 = -a1;
                a2 = -a2;
            }
            scale = SINF(p->phase) * 0.1f + 1.0f;
            p->phase += 0.08726646f;
            m = em->getPartsPtr(p->parts);
            m->ang.y = a1;
            m->scale.y = scale;
            RotMatrix(m->l_mat, &m->ang);
            TransMatrix(m->l_mat, &m->pos);
            ScaleMatrix(m->l_mat, &m->scale);
            m = em->getPartsPtr(p->parts2);
            m->ang.y = a2;
            m->scale.y = scale;
            RotMatrix(m->l_mat, &m->ang);
            TransMatrix(m->l_mat, &m->pos);
            ScaleMatrix(m->l_mat, &m->scale);
        }
    }
}

// Tentacles only (from the lower body's root): a hidden tentacle (T_Hide) surfaces at 200 HP (T_In).
void cEm38::setIn()
{
    if (type == 1 || type == 2) {
        if (r_no_1 == 6) {
            hp = 200;
            setRno(1, 7, 0, 0);
        }
    }
}

// Tentacles only: 1 while out of the water (flags 0x40).
int cEm38::ckIn()
{
    if (type != 1 && type != 2) {
        return 0;
    }
    if (EM38_WK(this)->flags & 0x40) {
        return 1;
    }
    return 0;
}

// Upper body only (from the body's bite): plays the attack pose (U_Critical).
void cEm38::setCritical()
{
    if (type == 3) {
        setRno(1, 0xF, 0, 0);
    }
}

// Upper body only: 1 while flags 0x80 (the bite) is set. The tentacles call it on the body, whose
// type is 0, so it reads 0 there.
int cEm38::ckCritical()
{
    if (type != 3) {
        return 0;
    }
    if (EM38_WK(this)->flags & 0x80) {
        return 1;
    }
    return 0;
}

// Body only: 1 while knocked down (routine 2/0).
int cEm38::ckDown()
{
    if (type != 0) {
        return 0;
    }
    return r_no_0 == 2 && r_no_1 == 0;
}

// The weak point object (obj00) on parts 0x3A of the body.
void em38WeakInit(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    w->pWeak = 0;
    if (em->type == 0) {
        Vec pos;
        Vec rot;

        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pWeak = SetObj00(ARC(0x51), ARC(0x52), &pos, &rot);
        if (w->pWeak) {
            w->pWeak->scale.x = 1.3f;
            w->pWeak->scale.y = 1.3f;
            w->pWeak->scale.z = 1.3f;
            w->pWeak->LightInfo.EnableMask = 0x80;
            OyaSetObj00(w->pWeak, em, 0x3A);
            w->pWeak->atari.off();
        }
    }
}

// Body only, per frame: the weak point object is displayed while Status_flg[1] 0x04000000 (the
// shell is open) and the body lives.
void em38WeakMove(cEm38* em)
{
    Em38Work* w = EM38_WK(em);

    if (em->type != 0) {
        return;
    }
    if (w->pWeak == 0) {
        return;
    }
    if (StaFlagChk(pG, STA_THERMO_GRAPH) && em->hp > 0) {
        w->pWeak->be_flag |= 2;
    } else {
        w->pWeak->be_flag &= ~2;
    }
}
