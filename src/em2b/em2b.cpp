// em2b module (D:/Bio4/Prog/em2b.cpp): the giant. Walks after the player, stamps, punches, kicks and
// charges, tears trees / rocks out of the ground and throws them, breaks the village houses and
// scroll objects, catches and strangles the player, and exposes its parasite after enough damage.

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em2b.h"
#include "embarrel.h"
#include "emhit.h"
#include "emtree.h"
#include "emrock.h"
#include "objYagura.h"
#include "TexRender.h"
#include "foot_shadow.h"
#include "obj.h"
#include "obj1d.h"
#include "main.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "act_btn.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "joy.h"
#include "pl_cloth.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "eprintf.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"
#include "obj20.h"

int GetWepDmVal(cEm* em, u32 wep_no, int near);   // em10.h (not included: it pulls emwep.h's global plemBackjump)

// The module's 0x34-byte COMMON block: uninitialised template statics of the original object,
// merged into .bss by the REL link.
asm(".comm common_em2b,52,4");


static void em2b_R0_Init(cEm2b* em);
static void em2b_R0_Move(cEm2b* em);
static void em2b_R1_Wait(cEm2b* em);
static void em2b_R1_FromEvent(cEm2b* em);
static void em2b_R1_R11E_Appear(cEm2b* em);
static void em2b_R1_R224_CageWait(cEm2b* em);
static void em2b_R1_Walk(cEm2b* em);
static void em2b_R1_Turn180(cEm2b* em);
static void em2b_R1_Threat(cEm2b* em);
static void em2b_R1_Stamp(cEm2b* em);
static void em2b_R1_Punch(cEm2b* em);
static void em2b_R1_Hook(cEm2b* em);
static void em2b_R1_UpperCut(cEm2b* em);
static void em2b_R1_Kick(cEm2b* em);
static void em2b_R1_DashAtk(cEm2b* em);
static void em2b_R1_HouseBreak(cEm2b* em);
static void em2b_R1_ScrollBreak(cEm2b* em);
static void em2b_R1_GetTree(cEm2b* em);
static void em2b_R1_TreeAtk(cEm2b* em);
static void em2b_R1_GetRock(cEm2b* em);
static void em2b_R1_ThrowRock(cEm2b* em);
static void em2b_R1_Catch(cEm2b* em);
static void em2b_R1_Strangle(cEm2b* em);
static void plem2b_CatchHand(cPlayer* pl);
static void plem2b_Strangle(cPlayer* pl);
static void em2b_R1_SubCatch(cEm2b* em);
static void subem2b_CatchHand(cSubChar* sub);
static void subem2b_Catch(cSubChar* sub);
static void subem2b_CatchEnd(cSubChar* sub);
static void em2b_R1_BaseAtk(cEm2b* em);
static void em2b_R1_HoleAtk(cEm2b* em);
static void plem2bDmFall(cPlayer* pl);
static void em2b_R0_Damage(cEm2b* em);
static void em2b_R1_Dm_Face(cEm2b* em);
static void em2b_R1_Dm_Tree(cEm2b* em);
static void em2bSetActAtkParasite(cEm2b* em);
static void em2b_R1_Dm_Parasite(cEm2b* em);
static void em2b_R1_Dm_Parasite2(cEm2b* em);
static void plem2b_AtkParasite(cPlayer* pl);
static void em2b_R1_Dm_Rock(cEm2b* em);
static void em2b_R1_Dm_Flash(cEm2b* em);
static void em2b_R1_Dm_Bomb(cEm2b* em);
static void em2b_R0_Die(cEm2b* em);
static void em2b_R1_Die_Normal(cEm2b* em);
static void em2b_R1_Die_Lost(cEm2b* em);
static void em2b_R1_Die_Event(cEm2b* em);
static void em2b_R1_Die_R224Drop(cEm2b* em);
static void plem2b_dm_Stamp(cPlayer* pl);
static void subem2b_dm_Stamp(cSubChar* sub);
static void plem2b_dm_BlowKick(cPlayer* pl);
static void em2bDashEscapeAction(cEm2b* em);
static void plem2bDashEscape(cPlayer* pl);
static void em2bEscapeAction(cEm2b* em);
static void plem2bEscapeTree(cPlayer* pl);
static void plem2bDmBlow(cPlayer* pl);


// The motion flip argument of the two model variants.
static inline int em2bFlip(Em2bWork* w, int a, int b)
{
    switch (w->Ft_axis) {
    case 0:
    default:
        return a;
    case 1:
        return b;
    }
}

// Variant-dependent motion (blend pair per variant).
static inline void em2bVariantMot(cEm2b* em, Em2bWork* w, int a0, int a1, int b0, int b1, int blend, int flip)
{
    void* m0;
    void* m1;

    switch (w->Ft_axis) {
    case 0:
    default:
        m0 = ARC(a0);
        m1 = ARC(a1);
        break;
    case 1:
        m0 = ARC(b0);
        m1 = ARC(b1);
        break;
    }
    MotionSetCore(em, &em->Motion, m0, m1, blend, flip, 0);
}

// Attack wind-up effect by giant variant (0 the normal one: a0 / a1, 1 the chained one: b0 / b1) and
// the hand the motion uses (Motion.Mot_attr bit6 = right).
static inline void em2bVariantEst(cEm2b* em, Em2bWork* w, int a0, int a1, int b0, int b1)
{
    switch (w->Ft_axis) {
    case 0:
    default:
        if (em->Motion.Mot_attr & 0x40) {
            EstSet(em, -1, 0, 0, w->Eff, a0, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            EstSet(em, -1, 0, 0, w->Eff, a1, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        return;
    case 1:
        if (em->Motion.Mot_attr & 0x40) {
            EstSet(em, -1, 0, 0, w->Eff, b0, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            EstSet(em, -1, 0, 0, w->Eff, b1, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        return;
    }
}

// End of an attack routine: the friend (dog) fight sets the guard, a hit goes into the threat.
static inline void em2bAtkEndSet(cEm2b* em, Em2bWork* w)
{
    // COMPILER-DIFF: #13 -- the original never allocates the single-use `w->Dog_wait` load (a REG_EQUIV
    // mem pseudo): reload materialises it in r11, so the global `flags`/`atkHit` pseudos take r0/r9;
    // ours local-allocates the load to r0 first.
    register int x63c asm("r11");

    if (w->pFriend && w->Atk_ck) {
        w->Be_flg |= 0x80;
        w->Go_dog_timer = 900;
        w->Dog_wait = 0;
        em2bNextRtnSet(em);
    } else if (w->pFriend && !(w->Be_flg & 0x80) && (x63c = w->Dog_wait, x63c == 0)) {
        int k = 900;
        u32 f = w->Be_flg | 0x80;

        w->Be_flg = f;
        w->Go_dog_timer = k;
        // COMPILER-DIFF: #13 -- the original's `ori` result and 900 (REG_EQUIV constants) do not die at
        // their stores, so sched1 issues `mr r3,em` (em dies there: weight 0) before them.
        asm("" : "=m"(w->Dog_wait) : "r"(f), "r"(k));
        em2bNextRtnSet(em);
    } else if (w->Atk_ck) {
        w->Dash_wait = 150;
        EmRoutineSet(em, 1, 4, 0, 0);
    } else {
        em2bNextRtnSet(em);
    }
}

// The same for a caller that reads `em` after it (DashAtk): `em` does not die at the `mr r3,em`, so
// arm 2 needs the tied copy below (with only the keep-alive the stores are issued first and jump2
// cross-jumps the arms' tails: 35 words; the plain form swaps the `mr`/`stw` pair: 2 words).
static inline void em2bAtkEndSetL(cEm2b* em, Em2bWork* w)
{
    register int x63c asm("r11"); // COMPILER-DIFF: #13 (see em2bAtkEndSet)

    if (w->pFriend && w->Atk_ck) {
        w->Be_flg |= 0x80;
        w->Go_dog_timer = 900;
        w->Dog_wait = 0;
        em2bNextRtnSet(em);
    } else if (w->pFriend && !(w->Be_flg & 0x80) && (x63c = w->Dog_wait, x63c == 0)) {
        int k = 900;
        u32 f = w->Be_flg | 0x80;
        cEm2b* e;

        // COMPILER-DIFF: #13 -- the target issues `mr r3,em` before the two stores although `em` is
        // live after the switch: a codeless tied copy of `em` declared before the stores gives the
        // argument move the earlier LUID, and the keep-alive below makes the stores non-dying like
        // the original's (weights equal, LUID order decides; without it the dying stores go first).
        asm("" : "=r"(e) : "0"(em));
        w->Be_flg = f;
        w->Go_dog_timer = k;
        asm("" : "=m"(w->Dog_wait) : "r"(f), "r"(k));
        em2bNextRtnSet(e);
    } else if (w->Atk_ck) {
        w->Dash_wait = 150;
        EmRoutineSet(em, 1, 4, 0, 0);
    } else {
        em2bNextRtnSet(em);
    }
}

// Drops the parasite head object with its effects.
static inline void em2bParasiteDelete(Em2bWork* w)
{
    if (w->pParasite) {
        EffectEspDelete(0, w->espKind, w->pParasite, 0);
        EffectEspgenDelete(0, w->espKind, w->pParasite);
        EffectEfmDelete(0, w->espKind, w->pParasite);
        w->pParasite->clearLostWait();
        w->pParasite = 0;
    }
}

// Stamp / punch landing: dust, quake, SE and the stagger check at the parts' world position.
static inline void em2bLandingSet(cEm2b* em, Em2bWork* w, int parts, int se)
{
    Vec* pos = &em->getPartsPtr(parts)->world;

    EstSet(0, -1, pos, 0, w->Eff, 5, 0, ESP_CORE_KIND_NONE, 0, 0);
    em2bQuakeSet(pos);
    SndCall(8, se, pos, em->id, 0, em);
    em2bStaggerCk(em, pos);
}

Em2bFunc Em2b_R0_move_tbl[4] = {
    em2b_R0_Init,
    em2b_R0_Move,
    em2b_R0_Damage,
    em2b_R0_Die,
};

static Em2bFunc Em2b_R1_move_tbl[24] = {
    em2b_R1_Wait,           // 0x00
    em2b_R1_FromEvent,      // 0x01
    em2b_R1_Walk,           // 0x02
    em2b_R1_Turn180,        // 0x03
    em2b_R1_Threat,         // 0x04
    em2b_R1_Stamp,          // 0x05
    em2b_R1_Punch,          // 0x06
    em2b_R1_Hook,           // 0x07
    em2b_R1_UpperCut,       // 0x08
    em2b_R1_Kick,           // 0x09
    em2b_R1_DashAtk,        // 0x0A
    em2b_R1_HouseBreak,     // 0x0B
    em2b_R1_ScrollBreak,    // 0x0C
    em2b_R1_GetTree,        // 0x0D
    em2b_R1_TreeAtk,        // 0x0E
    em2b_R1_GetRock,        // 0x0F
    em2b_R1_ThrowRock,      // 0x10
    em2b_R1_Catch,          // 0x11
    em2b_R1_Strangle,       // 0x12
    em2b_R1_SubCatch,       // 0x13
    em2b_R1_R11E_Appear,    // 0x14
    em2b_R1_BaseAtk,        // 0x15
    em2b_R1_HoleAtk,        // 0x16
    em2b_R1_R224_CageWait,  // 0x17
};

static Em2bFunc Em2b_R1_dm_tbl[7] = {
    em2b_R1_Dm_Face,
    em2b_R1_Dm_Tree,
    em2b_R1_Dm_Parasite,
    em2b_R1_Dm_Parasite2,
    em2b_R1_Dm_Rock,
    em2b_R1_Dm_Flash,
    em2b_R1_Dm_Bomb,
};

static Em2bFunc Em2b_R1_die_tbl[4] = {
    em2b_R1_Die_Normal,
    em2b_R1_Die_Lost,
    em2b_R1_Die_Event,
    em2b_R1_Die_R224Drop,
};

// Motion parts flip table (cModel::motFlip): the mirrored parts index per parts.
static u16 em2b_xflip_tbl[90] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x2F,
    0x30, 0x32, 0x31, 0x33, 0x34, 0x36, 0x35, 0x38, 0x37, 0x3A, 0x39, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
};

// Attack parameters per attack number (em2bAtkCk).
static EmAtkInfo em2b_atk_info[7] = {
    { 1000.0f, PL_DM_AUTO, 800, 0, 10, 0 },
    { 1100.0f, PL_DM_AUTO, 800, 0, 10, 0 },
    { 1500.0f, PL_DM_AUTO, 800, 0, 10, 0 },
    { 1500.0f, PL_DM_AUTO, 800, 0, 10, 0 },
    { 1000.0f, PL_DM_AUTO, 400, 0, 10, 0 },
    { 2000.0f, PL_DM_AUTO, 800, 0, 10, 0 },
    { 1000.0f, PL_DM_AUTO, 800, 0, 10, 0 },
};

Vec em2b_r11e_pos = { -4390.0f, 0.0f, -480.0f };
// Hand object the strangled player hangs on (plem2b_Strangle). A one-member struct so that every
// store through the object reloads it.
static struct {
    cObj* p;
} em2bCatchObj = { 0 };

// Chain cloth tables (em2bClothSet): parts, up / down / side neighbours, swing limits and damping.
static u8 em2b_cloth_parts[12] = { 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0, 0 };
static u8 em2b_cloth_up[12] = { 0xFF, 0x40, 0xFF, 0x42, 0xFF, 0x44, 0xFF, 0x46, 0xFF, 0x48, 0, 0 };
static u8 em2b_cloth_down[12] = { 0x41, 0xFF, 0x43, 0xFF, 0x45, 0xFF, 0x47, 0xFF, 0x49, 0xFF, 0, 0 };
static u8 em2b_cloth_side[12] = { 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0xFF, 0xFF, 0, 0 };
static f32 em2b_cloth_max[10] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
static f32 em2b_cloth_rate[10] = { 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f };

// Collision volumes of the chain cloth (pl_cloth.h CLOTH_AT_SET: parts pair, radius, offsets).
CLOTH_AT_SET em2b_cloth_at[2] = {
    { 0, 4, 4, 1.0f, 300.0f, { 0.0f, -200.0f, 210.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 4, 4, 1.0f, 300.0f, { 0.0f, -300.0f, 250.0f }, { 0.0f, 0.0f, 0.0f } },
};

// Short rope (type 0) and chain (type 3) tables (em2bShortRopeSet / em2bChainSet).
static u8 em2b_rope_parts[8] = { 1, 2, 3, 4, 5, 0, 0, 0 };
static u8 em2b_rope_up[8] = { 0xFF, 1, 2, 3, 4, 0, 0, 0 };
static u8 em2b_rope_down[8] = { 2, 3, 4, 5, 0xFF, 0, 0, 0 };
CLOTH_AT_SET em2b_rope_at[5] = {
    { 0, 3, 3, 1.0f, 650.0f, { -70.0f, 0.0f, 0.0f }, { -70.0f, 0.0f, 0.0f } },
    { 0, 2, 3, 0.5f, 750.0f, { 70.0f, 0.0f, 0.0f }, { 70.0f, 0.0f, 0.0f } },
    { 0, 2, 2, 1.0f, 900.0f, { 70.0f, 0.0f, 0.0f }, { 70.0f, 0.0f, 0.0f } },
    { 0, 5, 5, 1.0f, 700.0f, { 70.0f, 0.0f, 0.0f }, { 70.0f, 0.0f, 0.0f } },
    { 0, 0xB, 0xB, 1.0f, 700.0f, { 70.0f, 0.0f, 0.0f }, { 70.0f, 0.0f, 0.0f } },
};
static u8 em2b_chain_parts[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
static u8 em2b_chain_up[8] = { 0xFF, 1, 2, 3, 4, 5, 6, 7 };
static u8 em2b_chain_down[8] = { 2, 3, 4, 5, 6, 7, 8, 0xFF };
CLOTH_AT_SET em2b_chain_at[5] = {
    { 0, 3, 3, 1.0f, 650.0f, { -70.0f, 0.0f, 0.0f }, { -70.0f, 0.0f, 0.0f } },
    { 0, 2, 3, 0.5f, 750.0f, { 70.0f, 0.0f, 0.0f }, { 70.0f, 0.0f, 0.0f } },
    { 0, 2, 2, 1.0f, 900.0f, { 70.0f, 0.0f, 0.0f }, { 70.0f, 0.0f, 0.0f } },
    { 0, 5, 5, 1.0f, 700.0f, { 70.0f, 0.0f, 0.0f }, { 70.0f, 0.0f, 0.0f } },
    { 0, 0xB, 0xB, 1.0f, 700.0f, { 70.0f, 0.0f, 0.0f }, { 70.0f, 0.0f, 0.0f } },
};
CLOTH_AT_SET em2b_chain_at2[5] = {
    { 0, 2, 3, 0.5f, 900.0f, { 0.0f, 0.0f, 150.0f }, { 0.0f, 0.0f, 150.0f } },
    { 0, 2, 2, 1.0f, 900.0f, { 0.0f, 0.0f, 150.0f }, { 0.0f, 0.0f, 150.0f } },
    { 0, 1, 1, 1.0f, 850.0f, { 0.0f, 0.0f, 150.0f }, { 0.0f, 0.0f, 150.0f } },
    { 0, 0x12, 0x12, 1.0f, 650.0f, { 0.0f, 0.0f, 150.0f }, { 0.0f, 0.0f, 150.0f } },
    { 0, 0x16, 0x16, 1.0f, 650.0f, { 0.0f, 0.0f, 150.0f }, { 0.0f, 0.0f, 150.0f } },
};
asm(".section .data\n\t.balign 8\n\t.text");

// Module entry (SN loader): registers Em2bInit as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em2b prolog Ok\n");
    EmInitFunc = Em2bInit;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm2b class in the manager's work.
void Em2bInit(cEm* em)
{
    new (em) cEm2b();
}

// Destructor: destroys the parasite head object, the ten tentacle objects and the three chain
// objects of the chained variant that are still alive.
cEm2b::~cEm2b()
{
    Em2bWork* w = EM2B_WK(this);
    u32 i;

    if (w->pParasite && w->pParasite->isAlive()) {
        ObjMgr.destroy(w->pParasite);
    }
    for (i = 0; i < 10; i++) {
        if (w->pTen[i]) {
            ObjMgr.destroy(w->pTen[i]);
        }
    }
    if (w->pChain && w->pChain->isAlive()) {
        ObjMgr.destroy(w->pChain);
    }
    if (w->pChain2 && w->pChain2->isAlive()) {
        ObjMgr.destroy(w->pChain2);
    }
    if (w->pChain3 && w->pChain3->isAlive()) {
        ObjMgr.destroy(w->pChain3);
    }
}

// Suspend / resume the giant and every object hanging on it.
void cEm2b::setNoSuspend(int on)
{
    Em2bWork* w = EM2B_WK(this);
    u32 i;

    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
    if (w->pParasite) {
        if (w->pParasite->isAlive()) {
            w->pParasite->setNoSuspend(on);
        } else {
            w->pParasite = 0;
        }
    }
    for (i = 0; i < 10; i++) {
        if (w->pTen[i]) {
            if (w->pTen[i]->isAlive()) {
                w->pTen[i]->setNoSuspend(on);
            } else {
                w->pTen[i] = 0;
            }
        }
    }
    if (w->pChain) {
        if (w->pChain->isAlive()) {
            w->pChain->setNoSuspend(on);
        } else {
            w->pChain = 0;
        }
    }
    if (w->pChain2) {
        if (w->pChain2->isAlive()) {
            w->pChain2->setNoSuspend(on);
        } else {
            w->pChain2 = 0;
        }
    }
    if (w->pChain3) {
        if (w->pChain3->isAlive()) {
            w->pChain3->setNoSuspend(on);
        } else {
            w->pChain3 = 0;
        }
    }
}

// Damage reaction after a hit (em2bDmCk): blood by weapon, then the routine change.
void em2bDmCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    YARARE_INFO* part;
    int near;
    int dmg;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    if (em->dmg.m_Wep == 0x14) {
        return;
    }
    if (w->Go_dog_timer > 30) {
        w->Go_dog_timer -= 30;
    } else {
        w->Go_dog_timer = 0;
        w->Be_flg &= ~0x80;
        if (w->Dog_wait == 0) {
            w->Dog_wait = 600;
        }
    }
    em->dmg.m_Timer = 1;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    part = em->dmg.m_pDamageYarare;
    near = 0;
    if (part->len < 64000000.0f) {
        near = 1;
    }
    dmg = em2bSetDmVal(em);
    w->Parasite_damage -= dmg;
    switch (em->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x26:
    case 0x28:
    case 0x2B:
        if (part->parts_no == 0x3F) {
            EmDmBloodSet2(em, w->Eff, 0x25, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, w->Eff, 0, 0, 0, 0);
        }
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        if (part->parts_no == 0x3F) {
            EmDmBloodSet2(em, w->Eff, 0x25, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, w->Eff, 1, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (part->parts_no == 0x3F) {
            if (near) {
                EmDmBloodSet2(em, w->Eff, 0x26, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, w->Eff, 0x25, 0, 0, 0);
            }
        } else if (near == 0) {
            EmDmBloodSet2(em, w->Eff, 0, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, w->Eff, 2, 0, 0, 0);
            EmDmBloodSet2(em, w->Eff, 0, 1, 0, 0);
        }
        break;
    case 5:
    case 6:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (part->parts_no == 0x3F) {
            EmDmBloodSet2(em, w->Eff, 0x26, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, w->Eff, 2, 0, 0, 0);
            EmDmBloodSet2(em, w->Eff, 0, 1, 0, 0);
        }
        break;
    case 0x17:
    case 0x2A:
        break;
    case 0xD:
        if (part->parts_no == 0x3F) {
            EmDmBloodSet2(em, w->Eff, 0x26, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, w->Eff, 2, 0, 0, 0);
            EmDmBloodSet2(em, w->Eff, 0, 1, 0, 0);
        }
        em->hp = 0;
        break;
    }
    if (part->parts_no == 0x3F) {
        SndCall(8, 0x2F, &em->pos, em->id, 0, em);
        SndCall(8, 0x2D, &em->pos, em->id, 0, em);
    } else {
        SndCall(8, 8, &em->pos, em->id, 0, em);
    }
    if (part->parts_no == 0x3F) {
        if (em->dmg.m_Wep != 0x17 && em->dmg.m_Wep != 0x2A) {
            em->hp -= dmg * 2;
        }
        if (w->Be_flg & 0x2000) {
            if (em->hp > 0) {
                EmRoutineSet(em, 2, 3, 0, 0);
                return;
            }
        } else {
            if (em->hp > 0) {
                return;
            }
        }
    }
    if (em->hp <= 0) {
        EmSetDie(em);
        EmReserveDropItem(em);
        EmSetDieCnt(em);
        em->clearStatus(EM_STATUS_ACTIVE);
        EmRoutineSet(em, 3, 0, 0, 0);
        return;
    }
    if (em->flag & 8) {
        return;
    }
    if (w->Be_flg & 8) {
        return;
    }
    if (!(w->Be_flg & 0x400) && (em->dmg.m_Wep == 0x17 || em->dmg.m_Wep == 0x2A)) {
        EmRoutineSet(em, 2, 5, 0, 0);
        return;
    }
    w->Total_damage += dmg;
    if (w->Total_damage <= 999) {
        switch (em->dmg.m_Wep) {
        case 0xD:
        case 0x12:
        case 0x13:
        case 0x2D:
            EmRoutineSet(em, 2, 6, 0, 0);
            return;
        }
        return;
    }
    w->Total_damage = 0;
    switch (em->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 7:
    case 8:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x21:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        if (w->pTree) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
        if (w->pTree) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (w->pTree) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    }
}

// Per-frame update: damage check, clears the per-frame Be_flg bits, the timers (Rock_wait, Atk_wait,
// Dash_wait ...), route check (em2bRouteCk), the R0 table, then collision and scenario check, the
// debug attack override (Debug_atk_rtn), the parasite hit box (hit[9]) and the dropped tree cleanup.
void cEm2b::move()
{
    Em2bWork* w = EM2B_WK(this);
    cModel* p;
    f32 fl;
    f32 spd;
    f32 moved;

    if (r_no_0) {
        em2bDmCk(this);
    }
    flag &= ~0x10;
    w->Be_flg &= ~0x6D5B;
    if (w->Rock_wait) {
        w->Rock_wait--;
    }
    if (w->Atk_wait) {
        w->Atk_wait--;
    }
    if (w->Dash_wait) {
        w->Dash_wait--;
    }
    if (w->Punch_wait) {
        w->Punch_wait--;
    }
    if (w->Tree_brk_wait) {
        w->Tree_brk_wait--;
    }
    if (w->Go_dog_timer) {
        w->Go_dog_timer--;
        if (w->Go_dog_timer == 0) {
            w->Be_flg &= ~0x80;
            if (w->Dog_wait == 0) {
                w->Dog_wait = 600;
            }
        }
    }
    if (w->Dog_wait) {
        w->Dog_wait--;
    }
    if (w->Catch_power) {
        w->Catch_power--;
    }
    flag &= ~0xC;
    if (w->pFriend && !w->pFriend->isAlive()) {
        w->pFriend = 0;
    }
    em2bRouteCk(this);
    Em2b_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em2bNeckMove(this);
    partsWorldCalc();
    em2bScaleCompress(this);
    p = getPartsPtr(0);
    fl = pos.y + 2500.0f;
    if (p->world.y < fl) {
        atari.m_offset.y = pos.y;
    } else {
        atari.m_offset.y = 2500.0f;
    }
    spd = VEC_DISTXZ(&pos_old, &pos);
    em2bObaHitCk(this);
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    moved = VEC_DISTXZ(&pos, &pos_old);
    if (moved < spd * 0.5f) {
        w->HoseiCnt++;
    } else {
        if (w->HoseiCnt > 45) {
            w->HoseiCnt = 45;
        }
        if (w->HoseiCnt) {
            w->HoseiCnt--;
        }
    }
    em2bFootSe(this);
    em2bFtChgCk(this);
    em2bClothMove(this);
    em2bSearchDog(this);
    if (pG->debug_mode == 7) {
        if ((Joy[0].on & 0x40) && (Joy[0].trg & 0x200)) {
            w->Debug_atk_rtn++;
            if (w->Debug_atk_rtn > 6) {
                w->Debug_atk_rtn = 0;
            }
        }
        switch (w->Debug_atk_rtn) {
        case 0:
            break;
        case 1:
            eprintf(0x20, 0x50, 0, 7, "EM2B:Catch only");
            break;
        case 2:
            eprintf(0x20, 0x50, 0, 7, "EM2B:Upper only");
            break;
        case 3:
            eprintf(0x20, 0x50, 0, 7, "EM2B:Punch only");
            break;
        case 4:
            eprintf(0x20, 0x50, 0, 7, "EM2B:Stamp only");
            break;
        case 5:
            eprintf(0x20, 0x50, 0, 7, "EM2B:Dash only");
            break;
        case 6:
            eprintf(0x20, 0x50, 0, 7, "EM2B:Kick only");
            break;
        }
    }
    if (w->pTree) {
        w->Event_wait = 90;
    }
    if (w->pRock) {
        w->Event_wait = 90;
    }
    if (w->Event_wait) {
        w->Event_wait--;
        flag |= 0x10;
    }
    if (w->pParasite && hp > 0) {
        w->hit[9].flag |= 1;
    } else {
        w->hit[9].flag &= ~1;
    }
    if (w->pTreeBrk) {
        if (w->No_go_sub_timer) {
            w->No_go_sub_timer--;
        } else {
            if (w->Be_flg & 0x200) {
                EstSet(w->pTreeBrk, -1, 0, 0, EFF_ROOM, 0xE, 0, ESP_CORE_KIND_NONE, w->pTreeBrk, 0);
            } else {
                EstSet(w->pTreeBrk, -1, 0, 0, EFF_ROOM, 0xD, 0, ESP_CORE_KIND_NONE, w->pTreeBrk, 0);
            }
            w->pTreeBrk->setLost();
            w->pTreeBrk = 0;
        }
    }
}

// R0 == 0: creation. Builds the model of the variant (type 0..3: plain / chained / the two mercenaries
// ones), the room's ctrl12, priority, collision and the hit boxes, the "look at me" / active status,
// and the start routine by cEm::set: 0 Wait, 1 FromEvent (walks in from the cut scene), 2
// R11E_Appear (bursts through the room 11E gate), 3 R224_CageWait (the caged one of room 224).
static void em2b_R0_Init(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cAtariInfo* at;
    MotionWork* mot;
    int zero;
    int t35;
    f32 one;
    // COMPILER-DIFF: #13 -- the 900/0x23/1.0 init constants are reload-materialised in the original
    // (never allocated), so the 25-store block is issued in pure source order; here the three
    // pseudos are kept alive past the block by a dead asm whose output is pinned to r7 (any pseudo
    // output lands in r8 and perturbs the init2 argument order).
    int r11c;
    register int dmy7 asm("r7");
    Vec v;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(5), ARC(7)) == 0) {
            pLog->err(0, 0, "em2b() TYPE_A ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        w->pHead = ModInfoMgr.create(ARC(6), ARC(7));
        if (w->pHead) {
            em->addModel(w->pHead);
        }
        break;
    case 1:
        if (em->modelInit(ARC(8), ARC(0xA)) == 0) {
            pLog->err(0, 0, "em2b() TYPE_B ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        w->pHead = ModInfoMgr.create(ARC(9), ARC(0xA));
        if (w->pHead) {
            em->addModel(w->pHead);
        }
        break;
    case 2:
        if (em->modelInit(ARC(0xB), ARC(0xD)) == 0) {
            pLog->err(0, 0, "em2b() TYPE_C ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        w->pHead = ModInfoMgr.create(ARC(0xC), ARC(0xD));
        if (w->pHead) {
            em->addModel(w->pHead);
        }
        break;
    case 3:
        if (em->modelInit(ARC(0xE), ARC(0x10)) == 0) {
            pLog->err(0, 0, "em2b() TYPE_C ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        w->pHead = ModInfoMgr.create(ARC(0xF), ARC(0x10));
        if (w->pHead) {
            em->addModel(w->pHead);
        }
        break;
    }
    w->pCtrlGroup = GetCtrlCtrl12();
    at = &em->atari;
    em2bTexrenderInit(em);
    em->pFsdTbl = &Em2b_fs_tbl;
    em->Motion.flip = em2b_xflip_tbl;
    ((cParts*) em->getPartsPtr(0x12))->motParts.flags |= 0x1000;
    ((cParts*) em->getPartsPtr(0x16))->motParts.flags |= 0x1000;
    em2bClothSet(em);
    em->LightInfo.init2(0, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 10000.0f, 10000.0f, 10000.0f }), 2);
    at->init(0.0f, 0.0f, 0.0f, 1700.0f, 1500.0f, 1500.0f, 3000.0f, 1, 0x2000, 0xA);
    at->setPriority(PRI_LV1);
    YarareInit(em, 0.0f, -200.0f, 0.0f, 500.0f, 400.0f, 5, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[0], 0.0f, -100.0f, 0.0f, 900.0f, 1300.0f, 2, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], -80.0f, -1600.0f, 0.0f, 500.0f, 1600.0f, 0x14, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[2], 80.0f, -1600.0f, 0.0f, 500.0f, 1600.0f, 0x18, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[3], -1200.0f, 0.0f, 0.0f, 400.0f, 1600.0f, 9, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 400.0f, 1600.0f, 0xF, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[5], -80.0f, -1200.0f, 0.0f, 550.0f, 1200.0f, 0x13, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[6], 80.0f, -1200.0f, 0.0f, 550.0f, 1200.0f, 0x17, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[7], -1200.0f, 0.0f, 0.0f, 480.0f, 1200.0f, 8, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 480.0f, 1200.0f, 0xE, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 300.0f, 800.0f, 0x3F, 0);
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), EFF_EM2B, 0);
    w->espKind = EspPullCoreKind();
    r11c = 900;
    t35 = 0x23;
    one = 1.0f;
    w->Neck_dir_y = 0.0f;
    w->Tree_brk_wait = r11c;
    w->Compress_y = one;
    w->Eff = t35;
    w->Be_flg = zero;
    w->Ft_axis = zero;
    w->pHouse = 0;
    w->pTree = 0;
    w->pTreeBrk = 0;
    w->No_go_sub_timer = zero;
    w->pTreeTarget = 0;
    w->pRock = 0;
    w->pGoto = 0;
    w->Total_damage = zero;
    w->Rock_wait = zero;
    w->Atk_wait = zero;
    w->pFriend = 0;
    w->Dash_wait = zero;
    w->Punch_wait = r11c;
    w->Go_dog_timer = zero;
    w->Dog_wait = zero;
    w->Event_wait = zero;
    w->pYagura = 0;
    w->Catch_power = zero;
    asm("" : "=r"(dmy7) : "r"(t35), "f"(one), "r"(r11c)); // COMPILER-DIFF: #13
    if (pG->room_id == 0x224) {
        w->Eff = 1;
    }
    asm volatile("" : : "r"(dmy7)); // COMPILER-DIFF: #13
    w->pParasite = (cObj16*) zero;
    mot = &em->Motion;
    {
        u32 i;
        for (i = 0; i < 10; i++) {
            w->pTen[i] = 0;
        }
    }
    w->pChain = 0;
    w->pChain2 = 0;
    w->pChain3 = 0;
    if (em->type == 0) {
        em2bShortRopeSet(em);
    }
    if (em->type == 3) {
        em2bChainSet(em);
    }
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    SetObaModel((cObj*) em, 0x13, &v, 700.0f, 1000.0f, 0);
    SetObaModel((cObj*) em, 0x14, &v, 600.0f, 1000.0f, 0);
    SetObaModel((cObj*) em, 0x15, &v, 500.0f, 1000.0f, 0);
    SetObaModel((cObj*) em, 0x17, &v, 700.0f, 1000.0f, 0);
    SetObaModel((cObj*) em, 0x18, &v, 600.0f, 1000.0f, 0);
    SetObaModel((cObj*) em, 0x19, &v, 500.0f, 1000.0f, 0);
    em->setStatus(EM_STATUS_LOOK_ME);
    em->setStatus(EM_STATUS_ACTIVE);
    {
        int st = em->set;
        switch (st) {
        case 0:
        default:
            MotionSetCore(em, mot, ARC(0x19), 0, 0, 1, 0);
            EmRoutineSet(em, 1, 0, 0, 0);
            break;
        case 1:
            MotionSetCore(em, mot, ARC(0x53), 0, 0, 1, 0);
            EmRoutineSet(em, st, st, 0, 0);
            break;
        case 2:
            MotionSetCore(em, mot, ARC(0x59), 0, 0, 1, 0);
            EmRoutineSet(em, 1, 0x14, 0, 0);
            break;
        case 3:
            MotionSetCore(em, mot, ARC(0x59), 0, 0, 1, 0);
            EmRoutineSet(em, 1, 0x17, 0, 0);
            break;
        }
    }
    MotionMove(em, 0);
    EstSet(em, -1, 0, 0, w->Eff, 3, 0, ESP_CORE_KIND_NONE, em, 0);
    em2b_R0_Move(em);
}

// R0 == 1: marks a routine running (Be_flg bit4) and runs Em2b_R1_move_tbl[r_no_1].
static void em2b_R0_Move(cEm2b* em)
{
    Em2b_R1_move_tbl[em->r_no_1](em);
}

// Standing: the idle motion (with the tree when held), then the next action.
static void em2b_R1_Wait(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (w->pTree) {
            em2bVariantMot(em, w, 0x31, 0x78, 0x27, 0x6E, 30, 5);
        } else {
            int flip = em2bFlip(w, 5, 0x45);

            MotionSetCore(em, &em->Motion, ARC(0x19), ARC(0x62), 30, flip, 0);
        }
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (EmDeadCk(em) && em2bStayCk(em)) {
            EmRoutineSet(em, 1, 2, 0, 0xA);
            return;
        }
        if (StaFlagChk(pG, STA_PL_CATCHED) || EmDeadCk(pPL) || (s16) pG->pl_life <= 0) {
            w->Dash_wait = 30;
        }
        if (em->l_pl > 25000000.0f) {
            w->Dash_wait = 0;
        }
        if (w->Dash_wait == 0 && em2bStayCk(em)) {
            em2bNextRtnSet(em);
            return;
        }
        if (w->Go_rot > 2.35619449f) {
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
}

// Set from an event: the appear motion with its roar.
static void em2b_R1_FromEvent(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x53), 0, 0, 1, 0);
        SndCall(8, 0x2B, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        } else if (em->Motion.Seq_old.Free & 4) {
            em2bAtkRtnCk(em);
        }
        break;
    }
}

// r11e: breaks through the wall.
static void em2b_R1_R11E_Appear(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int step = em->r_no_2;

    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x59), 0, 0, 1, 0);
        EstSet(em, -1, 0, 0, w->Eff, 0x1C, 1, ESP_CORE_KIND_NONE, em, (void*) step);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
}

// r224: waits in the cage until the event releases it.
static void em2b_R1_R224_CageWait(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 5, 0x45);

        MotionSetCore(em, &em->Motion, ARC(0x19), ARC(0x62), 30, flip, 0);
        em->r_no_2++;
    }
    case 1:
        MotionMove(em, 0);
        if (em->flag & 1) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// Walking after the target: the walk blend by distance / difficulty (or with the tree).
static void em2b_R1_Walk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int atk;

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int zero = 0;
        u32 spd;

        w->TmpU32 = zero;
        spd = 0;
        if (em->l_pl > 64000000.0f) {
            spd = 1;
        }
        if (em->l_pl > 225000000.0f) {
            spd = 2;
        }
        if (pG->Game_level <= 1) {
            spd = 0;
        }
        if (w->pTree) {
            spd = 3;
        }
        switch (spd) {
        case 0:
            switch (w->Ft_axis) {
            case 0:
            default:
                w->blendM0 = ARC(0x3D);
                w->blendM1 = ARC(0x3F);
                w->blendM2 = ARC(0x40);
                w->blendA = (int) ARC(0x7F);
                w->blendB = (int) ARC(0x85);
                w->blendC = (int) ARC(0x88);
                w->blendD = 1;
                w->TmpU32 = zero;
                break;
            case 1:
                w->blendM0 = ARC(0x3E);
                w->blendM1 = ARC(0x41);
                w->blendM2 = ARC(0x42);
                w->blendA = (int) ARC(0x82);
                w->blendB = (int) ARC(0x8B);
                w->blendC = (int) ARC(0x8E);
                w->blendD = 1;
                w->TmpU32 = 1;
                break;
            }
            break;
        case 1:
            // COMPILER-DIFF: #12 (cse re-walk): the variant-1 body's `1`s must not fold to `spd`, only to the variant byte
            asm("" : "+r"(spd));
            switch (w->Ft_axis) {
            case 0:
            default:
                w->blendM0 = ARC(0x3D);
                w->blendM1 = ARC(0x3F);
                w->blendM2 = ARC(0x40);
                w->blendA = (int) ARC(0x81);
                w->blendB = (int) ARC(0x87);
                w->blendC = (int) ARC(0x8A);
                w->blendD = spd;
                w->TmpU32 = zero;
                break;
            case 1:
                w->blendM0 = ARC(0x3E);
                w->blendM1 = ARC(0x41);
                w->blendM2 = ARC(0x42);
                w->blendA = (int) ARC(0x84);
                w->blendB = (int) ARC(0x8D);
                w->blendC = (int) ARC(0x90);
                w->blendD = 1;
                w->TmpU32 = 1;
                break;
            }
            break;
        case 2:
            switch (w->Ft_axis) {
            case 0:
            default:
                w->blendM0 = ARC(0x3D);
                w->blendM1 = ARC(0x3F);
                w->blendM2 = ARC(0x40);
                w->blendA = (int) ARC(0x80);
                w->blendB = (int) ARC(0x86);
                w->blendC = (int) ARC(0x89);
                w->blendD = 1;
                w->TmpU32 = zero;
                break;
            case 1:
                w->blendM0 = ARC(0x3E);
                w->blendM1 = ARC(0x41);
                w->blendM2 = ARC(0x42);
                w->blendA = (int) ARC(0x83);
                w->blendB = (int) ARC(0x8C);
                w->blendC = (int) ARC(0x8F);
                w->blendD = 1;
                w->TmpU32 = 1;
                break;
            }
            break;
        case 3:
            switch (w->Ft_axis) {
            case 0:
            default:
                w->blendM0 = ARC(0x32);
                w->blendM1 = ARC(0x34);
                w->blendM2 = ARC(0x35);
                w->blendA = (int) ARC(0x79);
                w->blendB = zero;
                w->blendC = zero;
                w->blendD = 1;
                w->TmpU32 = zero;
                break;
            case 1:
                w->blendM0 = ARC(0x33);
                w->blendM1 = ARC(0x36);
                w->blendM2 = ARC(0x37);
                w->blendA = (int) ARC(0x7A);
                w->blendB = 0;
                w->blendC = 0;
                w->blendD = 1;
                w->TmpU32 = 1;
                break;
            }
            break;
        }
        em2bParasiteDelete(w);
        em2bSetTentacle(em, 0);
        atk = 0;
        w->Hokan = em->r_no_3;
        w->Frame = atk;
        w->Blend = Muku(&em->pos, &w->Go_pos, em->ang.y, 3.14159274f) * -162.338043f;
        if (w->Blend > 255.0f) {
            w->Blend = 255.0f;
        }
        if (w->Blend < -255.0f) {
            w->Blend = -255.0f;
        }
        w->Atk_ck = atk;
        em->r_no_2++;
    }
    case 1:
        em2bBlendMotSet(em, w->blendM0, w->blendM1, w->blendM2, w->blendA, w->blendB, w->blendC, w->blendD);
        if (MotionMove(em, 0)) {
            em2bSearchRockCk(em);
            if (em2bGetRockCk(em)) {
                break;
            }
            em2bSearchTree(em);
            if (em2bGetTreeCk(em)) {
                break;
            }
            atk = em2bAtkRtnCk(em);
            if (atk) {
                break;
            }
            if (w->Go_rot > 2.35619449f) {
                if (w->L_go < 9000000.0f && em2bFriendCk(em) == 0) {
                    if (pG->debug_mode == 7 && w->Debug_atk_rtn) {
                        em->r_no_0 = 1;
                        em->r_no_1 = 3;
                        em->r_no_2 = atk;
                        em->r_no_3 = atk;
                        break;
                    }
                    if ((Rnd() & 1) || pSUB) {
                        EmRoutineSet(em, 1, 0x11, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                } else {
                    EmRoutineSet(em, 1, 3, 0, 0);
                }
            } else if (em2bStayCk(em) == 0) {
                EmRoutineSet(em, 1, atk, atk, atk);
            } else {
                em->r_no_2 = atk;
                em->r_no_3 = atk;
            }
        } else if (em->Motion.Seq_old.Free & 4) {
            em2bAtkRtnCk(em);
        }
        break;
    }
    em2bR11eScrBrkCk(em);
}

// R1 == 3 Turn180: turns around towards the target (variant motions 0x51 / 0x52), then back to the walk.
static void em2b_R1_Turn180(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->pTree) {
            switch (w->Ft_axis) {
            case 0:
            default:
                MotionSetCore(em, &em->Motion, ARC(0x51), ARC(0x9F), 10, 1, 0);
                break;
            case 1:
                MotionSetCore(em, &em->Motion, ARC(0x52), ARC(0xA0), 10, 1, 0);
                break;
            }
        } else {
            int flip = em2bFlip(w, 1, 0x41);

            MotionSetCore(em, &em->Motion, ARC(0x4C), ARC(0x9A), 10, flip, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        } else if (em->Motion.Seq_old.Free & 4) {
            em2bAtkRtnCk(em);
        }
        break;
    }
}

// R1 == 4 Threat: the roar at the player (0x4C, mirrored by side), then the walk / attack choice.
static void em2b_R1_Threat(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 1, 0x41);

        MotionSetCore(em, &em->Motion, ARC(0x55), ARC(0xA2), 10, flip, 0);
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        } else if (em->Motion.Seq_old.Free & 4) {
            em2bAtkRtnCk(em);
        }
        break;
    }
}

// Stamp: turns towards the target, stamps with the right or left foot (motion flag bit6), then
// the follow-up stamp when the player is still near.
static void em2b_R1_Stamp(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int step = em->r_no_2;

    w->Be_flg |= 0x10;
    switch (step) {
    case 0:
        w->blendM0 = ARC(0x20);
        w->blendM1 = ARC(0x21);
        w->blendM2 = ARC(0x22);
        w->blendA = (int) ARC(0x69);
        w->blendB = step;
        w->blendC = step;
        switch (w->Ft_axis) {
        case 0:
        default:
            w->blendD = 0x41;
            break;
        case 1:
            w->blendD = 1;
            break;
        }
        w->Hokan = 10;
        w->Frame = 0;
        w->Blend = Muku(&em->pos, &w->Go_pos, em->ang.y, 1.57079637f) * -162.338043f;
        if (w->Blend > 255.0f) {
            w->Blend = 255.0f;
        }
        if (w->Blend < -255.0f) {
            w->Blend = -255.0f;
        }
        w->Atk_ck = 0;
        w->Timer = 60;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            w->Blend = w->Blend * 0.899999976f + Muku(&em->pos, &w->Go_pos, em->ang.y, 1.57079637f) * -162.338043f * 0.100000001f;
            if (w->Blend > 255.0f) {
                w->Blend = 255.0f;
            }
            if (w->Blend < -255.0f) {
                w->Blend = -255.0f;
            }
        }
        em2bBlendMotSet(em, w->blendM0, w->blendM1, w->blendM2, w->blendA, w->blendB, w->blendC, w->blendD);
        if (em->Motion.Seq_old.Free & 1) {
            if (em->Motion.Mot_attr & 0x40) {
                cModel* p = em->getPartsPtr(0x14);
                em2bAtkCk(em, &p->world, &p->world_old2, 0);
                p = em->getPartsPtr(0x15);
                em2bAtkCk(em, &p->world, &p->world_old2, 0);
                em2bR11eScrBrkCk2(em, &p->world, 3000.0f);
            } else {
                cModel* p = em->getPartsPtr(0x18);
                em2bAtkCk(em, &p->world, &p->world_old2, 0);
                p = em->getPartsPtr(0x19);
                em2bAtkCk(em, &p->world, &p->world_old2, 0);
                em2bR11eScrBrkCk2(em, &p->world, 3000.0f);
            }
        }
        if (em->Motion.Seq_old.Free & 2) {
            if (em->Motion.Mot_attr & 0x40) {
                em2bLandingSet(em, w, 0x15, 5);
            } else {
                em2bLandingSet(em, w, 0x19, 5);
            }
        }
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        } else if ((em->Motion.Seq_old.Free & 0x20) && w->Atk_ck) {
            em->r_no_2++;
        } else if ((em->Motion.Seq_old.Free & 4) && w->Atk_ck == 0) {
            em2bNextRtnSet(em);
        }
        break;
    case 2: {
        int flip = em2bFlip(w, 1, 0x41);

        MotionSetCore(em, &em->Motion, ARC(0x2C), ARC(0x73), 30, flip, 0);
        em->r_no_2++;
    }
    case 3:
        if (MotionMove(em, 0)) {
            em2bAtkEndSet(em, w);
        }
        break;
    }
}

// R1 == 6 Punch: the straight punch (variant wind-up effect), em2bAtkCk along the striking hand
// (parts 0x10 / 0xA by Motion.Mot_attr bit6) on the hit frames, then the walk.
static void em2b_R1_Punch(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x23), ARC(0x6A), 10, flip, 0);
        w->Atk_ck = 0;
        w->Tree_brk_wait = 1800;
        em->r_no_2++;
    }
    case 1:
        if (em->Motion.Seq_old.Free & 1) {
            cModel* p;
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0x10);
            } else {
                p = em->getPartsPtr(0xA);
            }
            em2bAtkCk(em, &p->world, &p->world_old2, 1);
            em2bR11eScrBrkCk2(em, &p->world, 3000.0f);
        }
        if (em->Motion.Seq_old.Free & 2) {
            if (em->Motion.Mot_attr & 0x40) {
                em2bLandingSet(em, w, 0x10, 7);
            } else {
                em2bLandingSet(em, w, 0xA, 7);
            }
        }
        if (MotionMove(em, 0)) {
            em2bAtkEndSet(em, w);
        } else if ((em->Motion.Seq_old.Free & 4) && w->Atk_ck == 0) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// R1 == 7 Hook: the hook punch (effects 0x19/0x15 or 0x1A/0x16 by variant), em2bAtkCk along the hand
// and forearm parts (0x10/0xF or 0xA/9), then the walk.
static void em2b_R1_Hook(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip = 1;
        if (w->Pl_dir < 0.0f) {
            flip = 0x41;
        }
        em2bVariantMot(em, w, 0x45, 0x93, 0x46, 0x94, 10, flip);
        em2bVariantEst(em, w, 0x19, 0x15, 0x1A, 0x16);
        w->Atk_ck = 0;
        em->r_no_2++;
    }
    case 1:
        if (em->Motion.Seq_old.Free & 1) {
            cModel* p;
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0x10);
            } else {
                p = em->getPartsPtr(0xA);
            }
            em2bAtkCk(em, &p->world, &p->world_old2, 2);
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0xF);
            } else {
                p = em->getPartsPtr(9);
            }
            em2bAtkCk(em, &p->world, &p->world_old2, 2);
            em2bR11eScrBrkCk2(em, &p->world, 3000.0f);
        }
        if (MotionMove(em, 0)) {
            em2bAtkEndSet(em, w);
        } else if ((em->Motion.Seq_old.Free & 4) && w->Atk_ck == 0) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// R1 == 8 UpperCut: the uppercut (effects 0x17/0x13 or 0x18/0x14), em2bAtkCk along hand, forearm and
// upper arm (0x10/0xF/0xE or 0xA/9/8), then the walk.
static void em2b_R1_UpperCut(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip = 1;
        if (w->Pl_dir < 0.0f) {
            flip = 0x41;
        }
        em2bVariantMot(em, w, 0x43, 0x91, 0x44, 0x92, 10, flip);
        em2bVariantEst(em, w, 0x17, 0x13, 0x18, 0x14);
        w->Atk_ck = 0;
        em->r_no_2++;
    }
    case 1:
        if (em->Motion.Seq_old.Free & 1) {
            cModel* p;
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0x10);
            } else {
                p = em->getPartsPtr(0xA);
            }
            em2bAtkCk(em, &p->world, &p->world_old2, 2);
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0xF);
            } else {
                p = em->getPartsPtr(9);
            }
            em2bAtkCk(em, &p->world, &p->world_old2, 2);
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0xE);
            } else {
                p = em->getPartsPtr(8);
            }
            em2bAtkCk(em, &p->world, &p->world_old2, 2);
            em2bR11eScrBrkCk2(em, &p->world, 3000.0f);
        }
        if (MotionMove(em, 0)) {
            em2bAtkEndSet(em, w);
        } else if ((em->Motion.Seq_old.Free & 4) && w->Atk_ck == 0) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// Kick: the kick, then (near and by chance) a second one turning after the target.
static inline void em2bKickStart(cEm2b* em, Em2bWork* w, int step)
{
    if (w->Go_dir < 0.0f) {
        switch (w->Ft_axis) {
        case 0:
        default:
            em->r_no_3 = step;
            MotionSetCore(em, &em->Motion, ARC(0x2A), ARC(0x71), 10, 1, 0);
            break;
        case 1:
            em->r_no_3 = 1;
            MotionSetCore(em, &em->Motion, ARC(0x2A), ARC(0x9B), 10, 0x41, 0);
            break;
        }
    } else {
        switch (w->Ft_axis) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x2A), ARC(0x71), 10, 0x41, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x2A), ARC(0x9B), 10, 1, 0);
            break;
        }
    }
}

// R1 == 9 Kick: the kick (em2bKickStart by variant), em2bAtkCk along the kicking foot (0x18 / 0x14);
// a hit sets Dash_wait 150 and goes to Threat, a miss on a far player may chain a second kick turning
// after him; then the walk.
static void em2b_R1_Kick(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int step = em->r_no_2;

    w->Be_flg |= 0x10;
    switch (step) {
    case 0:
        em2bKickStart(em, w, step);
        w->Atk_ck = 0;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 1) {
            cModel* p;
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0x18);
            } else {
                p = em->getPartsPtr(0x14);
            }
            em2bAtkCk(em, &p->world, &p->world_old2, 4);
            em2bR11eScrBrkCk2(em, &p->world, 3000.0f);
        }
        if (MotionMove(em, 0)) {
            if (w->Atk_ck) {
                w->Dash_wait = 150;
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                em2bNextRtnSet(em);
            }
        } else if ((em->Motion.Seq_old.Free & 0x20) && w->Atk_ck && em->l_pl > 16000000.0f && (Rnd() & 1)) {
            em->r_no_2++;
        } else if ((em->Motion.Seq_old.Free & 4) && w->Atk_ck == 0) {
            em2bNextRtnSet(em);
        }
        break;
    case 2: {
        int flip = em2bFlip(w, 0x41, 1);

        if (em->r_no_3) {
            MotionSetCore(em, &em->Motion, ARC(0x2B), ARC(0x72), 10, flip, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x58), ARC(0xA5), 10, flip, 0);
        }
        w->Atk_ck = 0;
        w->Timer = 15;
        em->r_no_2++;
    }
    case 3:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.0245436933f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_old.Free & 1) {
            cModel* p;
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0xA);
            } else {
                p = em->getPartsPtr(0x10);
            }
            em2bAtkCk(em, &p->world, &p->world_old2, 1);
            em2bR11eScrBrkCk2(em, &p->world, 3000.0f);
        }
        if (em->Motion.Seq_old.Free & 2) {
            if (em->Motion.Mot_attr & 0x40) {
                em2bLandingSet(em, w, 0xA, 7);
            } else {
                em2bLandingSet(em, w, 0x10, 7);
            }
        }
        if (MotionMove(em, 0)) {
            em2bAtkEndSet(em, w);
        }
        break;
    }
}

// Charge: turns onto the target, runs until it hits the scenario three times ahead of itself.
static void em2b_R1_DashAtk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x47), ARC(0x95), 10, flip, 0);
        w->Atk_ck = 0;
        w->Punch_wait = 1800;
        w->Timer = 15;
        em->r_no_2++;
    }
    case 1: {
        int end;

        w->Be_flg |= 0x100;
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.0245436933f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        end = MotionMove(em, 0);
        if (em->Motion.Seq_old.Free & 1) {
            Vec a = em->pos;
            Vec b = em->pos_old;
            a.y += 500.0f;
            b.y += 500.0f;
            em2bAtkCk(em, &a, &b, 5);
            em2bDashScrCk(em, &em->pos, 3500.0f);
        }
        if (end) {
            em->r_no_2++;
        }
        break;
    }
    case 2: {
        int flip = em2bFlip(w, 0x45, 5);

        MotionSetCore(em, &em->Motion, ARC(0x48), ARC(0x96), 10, flip, 0);
        w->HoseiCnt = 0;
        em->r_no_2++;
    }
    case 3:
        w->Be_flg |= 0x100;
        MotionMove(em, 0);
        if (em->Motion.Seq_old.Free & 1) {
            Vec a = em->pos;
            Vec b = em->pos_old;
            a.y += 500.0f;
            b.y += 500.0f;
            em2bAtkCk(em, &a, &b, 5);
            em2bDashScrCk(em, &em->pos, 3500.0f);
        }
        if (w->HoseiCnt > 2) {
            em->r_no_2++;
        } else {
            Vec a;
            Vec b;

            a.x = 0.0f;
            a.y = 500.0f;
            a.z = 0.0f;
            b.x = 0.0f;
            b.y = 500.0f;
            b.z = 3000.0f;
            PSMTXMultVec(em->mat, &a, &a);
            PSMTXMultVec(em->mat, &b, &b);
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                em->r_no_2++;
            } else {
                a.x = 1000.0f;
                a.y = 500.0f;
                a.z = 0.0f;
                b.x = 1000.0f;
                b.y = 500.0f;
                b.z = 3000.0f;
                PSMTXMultVec(em->mat, &a, &a);
                PSMTXMultVec(em->mat, &b, &b);
                if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                    em->r_no_2++;
                } else {
                    a.x = -1000.0f;
                    a.y = 500.0f;
                    a.z = 0.0f;
                    b.x = -1000.0f;
                    b.y = 500.0f;
                    b.z = 3000.0f;
                    PSMTXMultVec(em->mat, &a, &a);
                    PSMTXMultVec(em->mat, &b, &b);
                    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                        em->r_no_2++;
                    }
                }
            }
        }
        break;
    case 4: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x49), ARC(0x97), 10, flip, 0);
        w->Atk_ck = 0;
        w->HoseiCnt = 0;
        EstSet(em, -1, 0, 0, w->Eff, 0xE, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    }
    case 5:
        if (MotionMove(em, 0)) {
            em2bAtkEndSetL(em, w);
        }
        break;
    }
    if (em2bPlDashEscapeCk(em)) {
        ActBtn.set(ACT_GUARD, 0xB, (void*) em2bDashEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
    }
}

// Room 119 house break flags (pG->flags_174): the upper bits per house, the second set while intact.
static inline void em2bHouseFlagSet(EmiEntry* h)
{
    if (h->state == 0) {
        switch (h->sub) {
        case 0:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_A);
            RmfFlagOn(pG, RMF_R119_DESTROY_YANE_A);
            break;
        case 1:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_B);
            RmfFlagOn(pG, RMF_R119_DESTROY_YANE_B);
            break;
        case 2:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_C);
            RmfFlagOn(pG, RMF_R119_DESTROY_YANE_C);
            break;
        }
    } else {
        switch (h->sub) {
        case 0:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_A);
            break;
        case 1:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_B);
            break;
        case 2:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_C);
            break;
        }
    }
}

// Marks the house EMI entry (pHouse) as broken (state 3) and, in room 119, raises its room flag.
static inline void em2bHouseBreakSet(Em2bWork* w)
{
    EmiEntry* h = w->pHouse;

    if (h) {
        if (pG->stage_no == 1 && pG->room_no == 0x19) {
            em2bHouseFlagSet(h);
        }
        w->pHouse->state = 3;
        w->pHouse = 0;
    }
}

// Hand landing without dust: quake, SE and the stagger check at the parts' world position.
static inline cModel* em2bHandLanding(cEm2b* em, int parts)
{
    cModel* p = em->getPartsPtr(parts);
    Vec* pos = &p->world;

    em2bQuakeSet(pos);
    SndCall(8, 7, pos, em->id, 0, em);
    em2bStaggerCk(em, pos);
    return p;
}

// em2bHandLanding on a parts pointer the caller already holds (em2b_R1_HouseBreak: one function-scope `p` for
// every hand, allocated before `em` -- r31 -- because its live length is short and it crosses the calls here).
static inline void em2bHandLandingP(cEm2b* em, cModel* p)
{
    Vec* pos = &p->world;

    em2bQuakeSet(pos);
    SndCall(8, 7, pos, em->id, 0, em);
    em2bStaggerCk(em, pos);
}

// The player inside 6000 of the landing hand is knocked down.
static inline void em2bHandLandingPlCk(cModel* p)
{
    if ((s16) pG->pl_life > 0 && !EmDeadCk(pPL)) {
        f32 dx = pPL->pos.x - p->world.x;
        f32 dy = pPL->pos.y - p->world.y;
        f32 dz = pPL->pos.z - p->world.z;
        if (dx * dx + dy * dy + dz * dz < 36000000.0f) {
            PlSetDamage(PL_DM_AUTO_SML, 0, 0);
        }
    }
}


// Both hands slam into the house: the first blow marks it hit, the second breaks it.
static void em2b_R1_HouseBreak(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cModel* p;

    if (em->r_no_2 == 0 && !(w->Be_flg & 0x20)) {
        w->Be_flg |= 0x20;
        em->r_no_2 = 2;
    }
    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x25), ARC(0x6C), 10, flip, 0);
        w->Atk_ck = 0;
        em->r_no_2++;
    }
    case 1:
        if (w->Timer) {
            w->Timer--;
            if (w->pHouse) {
                em->ang.y += Muku(&em->pos, &w->pHouse->pos, em->ang.y, 0.0981747732f);
            }
        }
        if (em->Motion.Seq_old.Free & 1) {
            p = em->getPartsPtr(0x10);
            em2bAtkCk(em, &p->world, &p->world_old2, 1);
            p = em->getPartsPtr(0xA);
            em2bAtkCk(em, &p->world, &p->world_old2, 1);
        }
        if (em->Motion.Seq_old.Free & 2) {
            em2bHouseBreakSet(w);
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0x10);
                em2bHandLandingP(em, p);
            } else {
                p = em->getPartsPtr(0xA);
                em2bHandLandingP(em, p);
            }
            em2bHandLandingPlCk(p);
        }
        if (em->Motion.Seq_frame > 72.7f && em->Motion.Seq_frame < 73.3f) {
            EstSet(em, -1, 0, 0, w->Eff, 0x1B, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        }
        break;
    case 2: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x5B), ARC(0xA8), 10, flip, 0);
        w->Atk_ck = 0;
        w->Timer = 10;
        em->r_no_2++;
    }
    case 3: {
        int end;

        if (w->Timer) {
            w->Timer--;
            if (w->pHouse) {
                em->ang.y += Muku(&em->pos, &w->pHouse->pos, em->ang.y, 0.0981747732f);
            }
        }
        if (em->Motion.Seq_old.Free & 1) {
            p = em->getPartsPtr(0x10);
            em2bAtkCk(em, &p->world, &p->world_old2, 1);
            p = em->getPartsPtr(0xA);
            em2bAtkCk(em, &p->world, &p->world_old2, 1);
        }
        if (em->Motion.Seq_old.Free & 2) {
            EstSet(em, -1, 0, 0, w->Eff, 0x1B, 0, ESP_CORE_KIND_NONE, em, 0);
            em2bHouseBreakSet(w);
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0x10);
                em2bHandLandingP(em, p);
            } else {
                p = em->getPartsPtr(0xA);
                em2bHandLandingP(em, p);
            }
            em2bHandLandingPlCk(p);
        }
        if (em->Motion.Seq_old.Free & 0x10) {
            if (w->pHouse) {
                switch (w->pHouse->sub) {
                case 0:
                    RmfFlagOn(pG, RMF_R119_DESTROY_YANE_A);
                    break;
                case 1:
                    RmfFlagOn(pG, RMF_R119_DESTROY_YANE_B);
                    break;
                case 2:
                    RmfFlagOn(pG, RMF_R119_DESTROY_YANE_C);
                    break;
                }
                w->pHouse->state = 1;
            }
            if (em->Motion.Mot_attr & 0x40) {
                em2bHandLanding(em, 0x10);
            } else {
                em2bHandLanding(em, 0xA);
            }
        }
        end = MotionMove(em, 0);
        if (end) {
            if (w->pHouse) {
                f32 dx = pPL->pos.x - w->pHouse->pos.x;
                f32 dz = pPL->pos.z - w->pHouse->pos.z;
                if (dx * dx + dz * dz < 2250000.0f) {
                    em->r_no_2 = 0;
                    break;
                }
                w->pHouse = 0;
            }
            em2bNextRtnSet(em);
        } else if ((em->Motion.Seq_old.Free & 4) && w->pHouse) {
            f32 dx = pPL->pos.x - w->pHouse->pos.x;
            f32 dz = pPL->pos.z - w->pHouse->pos.z;
            if (dx * dx + dz * dz < 2250000.0f) {
                em->r_no_2 = end;
            }
        }
        break;
    }
    }
}

// Both hands slam onto a scroll object (em2bDashScrCk breaks it).
static void em2b_R1_ScrollBreak(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cModel* p;

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x25), ARC(0x6C), 10, flip, 0);
        w->Atk_ck = 0;
        em->r_no_2++;
    }
    case 1:
        if (w->Timer) {
            w->Timer--;
            if (w->pHouse) {
                em->ang.y += Muku(&em->pos, &w->pHouse->pos, em->ang.y, 0.0981747732f);
            }
        }
        if (em->Motion.Seq_old.Free & 1) {
            p = em->getPartsPtr(0x10);
            em2bAtkCk(em, &p->world, &p->world_old2, 1);
            p = em->getPartsPtr(0xA);
            em2bAtkCk(em, &p->world, &p->world_old2, 1);
        }
        if (em->Motion.Seq_old.Free & 2) {
            em2bDashScrCk(em, &em->getPartsPtr(0xA)->world, 1000.0f);
            em2bDashScrCk(em, &em->getPartsPtr(0x10)->world, 1000.0f);
            if (em->Motion.Mot_attr & 0x40) {
                em2bHandLanding(em, 0x10);
            } else {
                em2bHandLanding(em, 0xA);
            }
        }
        if (em->Motion.Seq_frame > 72.7f && em->Motion.Seq_frame < 73.3f) {
            EstSet(em, -1, 0, 0, w->Eff, 0x1B, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// Tears the searched tree out: turns to it, hangs the tree on the hand when the motion ends.
static void em2b_R1_GetTree(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cEmTree* tree = w->pTree;
    Vec dv;
    Mtx m; // function scope: its slot stays in use, so case 1's `s` reuses v's 16-byte slot (frame 80) instead of a merged m+v slot

    switch (em->r_no_2) {
    case 0: {
        cModel* p;
        f32 d;

        w->pTree = (cEmTree*) w->pTreeTarget;
        w->pTreeTarget = 0;
        tree = w->pTree;
        em->ang.y = GetXZAngle(&em->pos, &tree->pos);
        p = tree->getPartsPtr(1);
        d = Muku2(tree->ang.y, em->ang.y, 3.14159274f);
        tree->ang.y += d;
        p->ang.y -= d;
        PSMTXRotRad(m, 'y', tree->ang.y);
        TransMatrix(m, &tree->pos);
        {
            Vec v;

            v.x = 534.859985f;
            v.y = 0.0f;
            v.z = -2875.87988f;
            PSMTXMultVec(m, &v, &v);
            PSVECSubtract(&v, &em->pos, &w->moveVec);
        }
        tree->atari.m_flag &= ~0x200;
        MotionSetCore(tree, &tree->Motion, ARC(0xAE), 0, 0, 1, 0);
        tree->setCatch();
        MotionSetCore(em, &em->Motion, ARC(0x26), ARC(0x6D), 10, 1, 0);
        w->Ft_axis = 0;
        EstSet(tree, -1, 0, 0, EFF_ROOM, 6, 0, ESP_CORE_KIND_NONE, tree, 0);
        EstSet(em, -1, 0, 0, w->Eff, 9, 0, ESP_CORE_KIND_NONE, em, 0);
        w->Be_flg &= ~0x200;
        w->TmpV = em->pos;
        em->r_no_2++;
    }
    case 1: {
        Vec s;

        if (tree) {
            PSVECSubtract(&em->pos, &w->TmpV, &dv);
            dv.y = 0.0f;
            PSVECAdd(&tree->pos, &dv, &tree->pos);
        }
        PSVECScale(&w->moveVec, &s, 0.1f);
        PSVECAdd(&em->pos, &s, &em->pos);
        PSVECSubtract(&w->moveVec, &s, &w->moveVec);
        if (MotionMove(em, 0)) {
            MotionSetCore(tree, &tree->Motion, ARC(0xAF), 0, 0, 1, 0);
            tree->pos.x = 0.0f;
            tree->pos.y = 0.0f;
            tree->pos.z = 0.0f;
            tree->ang.x = 0.0f;
            tree->ang.y = 0.0f;
            tree->ang.z = 0.0f; // last: the dying store is issued first
            tree->setParent(em, 0x10, 0);
            em2bNextRtnSet(em);
        } else {
            w->TmpV = em->pos;
        }
        break;
    }
    }
}

// Swings the held tree; the tree breaks on a hit and is dropped when its timer runs out.
static void em2b_R1_TreeAtk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cEmTree* tree = w->pTree;

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (w->Go_rot > 1.57079637f) {
            MotionSetCore(tree, &tree->Motion, ARC(0xB2), 0, 0, 0, 0);
            MotionSetCore(em, &em->Motion, ARC(0x57), ARC(0xA4), 10, 1, 0);
            EstSet(em, -1, 0, 0, w->Eff, 0xD, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            MotionSetCore(tree, &tree->Motion, ARC(0xB1), 0, 0, 0, 0);
            MotionSetCore(em, &em->Motion, ARC(0x56), ARC(0xA3), 10, 1, 0);
            EstSet(em, -1, 0, 0, w->Eff, 7, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        w->Atk_ck = 0;
        w->Timer = 126;
        em->r_no_2++;
    case 1: {
        int end = MotionMove(em, 0);

        if (end) {
            em2bNextRtnSet(em);
            break;
        }
        if (em->Motion.Seq_old.Free & 1) {
            int hit = em2bTreeAtkCk(em);
            int scr = em2bTreeAtkScrCk(em);
            if ((hit || scr) && tree->hp > 1) {
                cModel* p;
                tree->hp = 1;
                p = w->pTree->getPartsPtr(2);
                p->scale.x = 0.0f;
                p->scale.y = 0.0f;
                p->scale.z = 0.0f;
                EstSet(tree, -1, 0, 0, EFF_ROOM, 0xA, 0, ESP_CORE_KIND_NONE, tree, 0);
                w->Be_flg |= 0x200;
            }
            em->flag |= 4;
        }
        if (em->Motion.Seq_old.Free & 4) {
            ActBtn.set(ACT_STOOP, 0xB, (void*) em2bEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
        }
        if (w->Timer) {
            w->Timer--;
            if (w->Timer == 0) {
                if (w->Be_flg & 0x200) {
                    EstSet(tree, -1, 0, 0, EFF_ROOM, 0xE, 0, ESP_CORE_KIND_NONE, tree, 0);
                } else {
                    EstSet(tree, -1, 0, 0, EFF_ROOM, 0xD, 0, ESP_CORE_KIND_NONE, tree, 0);
                }
                SndCall(8, 0x31, &tree->pos, em->id, 0, tree);
                tree->setLost();
                w->pTree = 0;
            }
        }
        if ((em->Motion.Seq_old.Free & 2) && tree) {
            Vec v = tree->getPartsPtr(0)->world;
            v.y = em->pos.y;
            tree->clearParent();
            tree->pos = v;
            tree->ang.y = em->ang.y;
            MotionSetCore(tree, &tree->Motion, ARC(0xB3), 0, 0, 1, 0);
        }
        break;
    }
    }
}

// Tears a rock out of the ground and hangs it on the hand.
static void em2b_R1_GetRock(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x28), ARC(0x6F), 10, 1, 0);
        w->pGoto = 0;
        EstSet(em, -1, 0, 0, w->Eff, 8, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        } else if (em->Motion.Seq_old.Free & 1) {
            Vec pos;
            Vec rot;

            pos.x = -317.329987f;
            pos.y = -10.7299995f;
            pos.z = 627.23999f;
            rot.x = 0.0f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            w->pRock = SetRock(ARC(0x15), ARC(0x16), &pos, &rot, 0);
            if (w->pRock) {
                w->pRock->setParent(em, 0xA, 0);
            }
        }
        break;
    }
}

// Throws the held rock at the target (the friend when fighting it).
static void em2b_R1_ThrowRock(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x54), ARC(0xA1), 10, 1, 0);
        w->Atk_ck = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        } else if (em->Motion.Seq_old.Free & 1) {
            em->flag |= 4;
            if (w->pRock) {
                cModel* p = em->getPartsPtr(0xA);
                Mtx m;
                Vec spd;
                f32 ang;
                EmAtkInfo* atk;

                if ((w->Be_flg & 4) && w->pFriend) {
                    ang = GetXZAngle(&p->world, &w->pFriend->pos);
                } else {
                    ang = GetXZAngle(&p->world, &pPL->pos);
                }
                ang = Muku2(em->ang.y, ang, 0.785398185f);
                PSMTXRotRad(m, 'y', LIMIT_ANGLE(ang + em->ang.y));
                spd.x = 0.0f;
                spd.y = 100.0f;
                spd.z = 400.0f;
                PSMTXMultVecSR(m, &spd, &spd);
                atk = &em2b_atk_info[6];
                if ((s16) pG->pl_life > 1) {
                    atk->flag |= 4;
                } else {
                    atk->flag &= ~4;
                }
                w->pRock->setThrow(&spd, atk);
                w->pRock->setSeFall(8, 0xA, em->id);
                w->pRock->setEffFall(1, 7);
                w->pRock = 0;
            }
        }
        break;
    }
}

// Grab: the hand that reaches the player (or the partner) starts the strangle.
static void em2b_R1_Catch(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    f32 d;

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int one = 1;

        asm("" : "+r"(one)); // COMPILER-DIFF: candidate (gcse cprop): the 2nd/3rd arms pass the `li r30,1` register, the 1st a fresh `li r8,1`
        w->Ft_axis = 1;
        if (em2bPlRunCk(em) && w->L_go < 16000000.0f) {
            MotionSetCore(em, &em->Motion, ARC(0x50), ARC(0x9E), 10, 1, 0);
        } else if (w->Go_rot < 1.57079637f) {
            MotionSetCore(em, &em->Motion, ARC(0x38), ARC(0x7B), 10, one, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x50), ARC(0x9E), 10, one, 0);
        }
        w->Atk_ck = 0;
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
            break;
        }
        em->partsWorldCalc();
        if (em->Motion.Seq_old.Free & 1) {
            cModel* p;
            Vec v;

            em->flag |= 4;
            if (em->Motion.Mot_attr & 0x40) {
                p = em->getPartsPtr(0x10);
            } else {
                p = em->getPartsPtr(0xA);
            }
            em2bR11eScrBrkCk2(em, &p->world, 3000.0f);
            if (w->Atk_ck == 0) {
                v = pPL->pos;
                v.y += 1000.0f;
                d = (p->world.x - v.x) * (p->world.x - v.x) + (p->world.y - v.y) * (p->world.y - v.y)
                    + (p->world.z - v.z) * (p->world.z - v.z);
                if (d < 4000000.0f && !EmDeadCk(pPL)) {
                    pPL->dmg.m_Timer = 2;
                    SetPlDamage(em, plem2b_CatchHand);
                    SndCall(8, 0x24, &p->world, em->id, 0, em);
                    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
                    w->Atk_ck = 1;
                }
            }
            if (w->Atk_ck == 0 && pSUB) {
                v = pSUB->pos;
                v.y += 1000.0f;
                d = (p->world.x - v.x) * (p->world.x - v.x) + (p->world.y - v.y) * (p->world.y - v.y)
                    + (p->world.z - v.z) * (p->world.z - v.z);
                if (d < 4000000.0f) {
                    SetSubDamage(em, (void (*)()) subem2b_CatchHand);
                    LifeDownSet2(pSUB, 300, 0, 1);
                    SndCall(8, 0x24, &p->world, em->id, 0, em);
                    w->Atk_ck = 2;
                }
            }
        }
        if ((em->Motion.Seq_old.Free & 0x20) && w->Atk_ck == 1) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        } else if ((em->Motion.Seq_old.Free & 0x20) && w->Atk_ck == 2) {
            EmRoutineSet(em, 1, 0x13, 0, 0);
        } else if ((em->Motion.Seq_old.Free & 4) && w->Atk_ck == 0) {
            em2bNextRtnSet(em);
        }
        break;
    }
    if (w->Atk_ck) {
        em->flag |= 8;
    }
}

// Strangles the caught player: the button mash escape or the death by squeezing.
static void em2b_R1_Strangle(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.throughOn();
        em2bCatchPosSet(em);
        {
            int flip = em2bFlip(w, 0x41, 1);

            MotionSetCore(em, &em->Motion, ARC(0x39), ARC(0x7C), 0, flip, 0);
        }
        SetPlDamage(em, plem2b_Strangle);
        PlGachaInit();
        w->Timer = 70;
        w->Timer2 = 0;
        StaFlagOn(pG, STA_SUSPEND);
        pPL->setNoSuspend(1);
        em->setNoSuspend(1);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        GameAddPoint(LVADD_PL_DAMAGE);
        em->r_no_2++;
    case 1:
        em->flag |= 8;
        if (w->Timer) {
            w->Timer--;
        } else {
            if (w->Timer2) {
                w->Timer2--;
            } else {
                w->Timer2 = 4;
                EstSet(em, -1, 0, 0, w->Eff, 0x12, 0, ESP_CORE_KIND_NONE, em, 0);
            }
            LifeDownSet2(pPL, 15, 0, 1);
            if ((s16) pG->pl_life > 1) {
                PlGachaMove();
            }
            if ((s16) pG->pl_life <= 1) {
                pG->pl_life = 0;
                em->r_no_2 = 4;
                MotionMove(em, 0);
                break;
            }
        }
        if (MotionMove(em, 0)) {
            AtariOnV(&em->atari, 0x300); // throughOff(): the volatile view keeps the following `lwz pG` below the sth
            if ((s16) pG->pl_life <= 1) {
                pG->pl_life = 0;
                em->r_no_2 = 4;
                break;
            }
            em->r_no_2 = 2;
        } else if ((u32) PlGachaGet() > 30) {
            em->r_no_2 = 2;
        }
        break;
    case 2: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x3A), ARC(0x7D), 10, flip, 0);
        EstSet(em, -1, 0, 0, w->Eff, 0x10, 0, ESP_CORE_KIND_NONE, em, 0);
        w->Total_damage += 200;
        StaFlagOff(pG, STA_SUSPEND);
        pPL->setNoSuspend(0);
        em->setNoSuspend(0);
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
        AtariOnV(&em->atari, 0x300); // throughOff(): the volatile view keeps the following `lwz pG` below the sth
        w->Timer = 60;
        em->r_no_2++;
    }
    case 3:
        if (w->Timer) {
            w->Timer--;
            em->flag |= 8;
        }
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        }
        break;
    case 4: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x3B), ARC(0x7E), 10, flip, 0);
        EstSet(em, -1, 0, 0, w->Eff, 0x23, 0, ESP_CORE_KIND_NONE, em, 0);
        AtariOnV(&em->atari, 0x300); // throughOff(): the volatile view keeps the following `lwz pG` below the sth
        StaFlagOff(pG, STA_SUSPEND);
        pPL->setNoSuspend(0);
        em->setNoSuspend(0);
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
        em->r_no_2++;
    }
    case 5:
        em->flag |= 8;
        MotionMove(em, 0);
        break;
    }
}

// Player caught by the hand: hangs on the hand parts' matrix until the strangle starts.
static void plem2b_CatchHand(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.m_Timer = 2;
    switch (pl->r_no_2) {
    case 0:
        pl->atari.throughOn();
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xBE), 0, 0, 0, 0);
        PlSetFace(1);
        pl->be_flag &= ~0x10;
        PlSetDamageSe(9);
        pl->r_no_2++;
    case 1: {
        cModel* p = pl->pEmCatch->getPartsPtr(0xA);
        Vec pos;
        Vec rot;

        rot.x = -0.05022185f;
        rot.y = 1.9358388f;
        rot.z = -0.4252966f;
        pos.x = -356.279999f;
        pos.y = 162.869995f;
        pos.z = 543.849976f;
        RotMatrix(pl->mat, &rot);
        TransMatrix(pl->mat, &pos);
        ScaleMatrix(pl->mat, &pl->scale);
        PSMTXConcat(p->mat, pl->mat, pl->mat);
        pl->pos.x = pl->mat[0][3];
        pl->pos.y = pl->mat[1][3];
        pl->pos.z = pl->mat[2][3];
        pl->Motion.Mot_flag |= 0x40000000;
        PSMTXMultVec(p->mat, &pos, &pl->pos);
        PSVECSubtract(&pl->pos, &pl->pEmCatch->pos, &pos);
        pl->ang.x = 0.0f;
        pl->ang.y = atan2f(pos.x, pos.z);
        pl->ang.z = 0.0f;
        MotionMove(pl, 0);
        break;
    }
    }
    pl->partsWorldCalc();
    em2bBlowCamMove((cEm2b*)pl->pEmCatch, 1.0f);
    pl->subArc = pl->subArc2;
}

// Strangled player: follows the giant's step (xFE), the button mash blends the struggle motion.
static void plem2b_Strangle(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 2;
    switch (pl->r_no_2) {
    case 0: {
        Vec v;

        pl->Motion.Mot_flag &= ~0x40000000;
        v.x = 1436.07996f;
        v.y = 0.0f;
        v.z = 89.5199966f;
        pl->ang.x = 0.0f;
        pl->ang.y = pl->pEmCatch->ang.y + 3.14159274f;
        pl->ang.z = 0.0f;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        PlSetFace(1);
        pl->Wep->setTrans(0, 0);
        em2bCatchObj.p = ObjMgr.create(cObjMgr::ID_PL_WEAPON);
        if (em2bCatchObj.p) {
            em2bCatchObj.p->modelInit(EM_ARC(pl, 0x18), EM_ARC(pl, 0x17));
            em2bCatchObj.p->atari.m_flag &= 0xFCFF;
            em2bCatchObj.p->pParts->pParent = pPL->getPartsPtr(0xA);
            em2bCatchObj.p->LightInfo.init2(1, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 500.0f, 0.0f, 0.0f }), 1);
            ((cObjWep*) em2bCatchObj.p)->m_pParent = pPL;
            em2bCatchObj.p->setNoSuspend(1);
            em2bCatchObj.p->getPartsPtr(1)->ang.y = 3.14159274f;
        }
        pl->atari.throughOn();
        pl->m_Hokan = 0;
        pl->m_Frame = 0;
        pl->m_Blend = 0.0f;
        pl->r_no_2++;
    }
    case 1:
        pl->m_Blend = (f32) (u32) PlGachaGet() * 0.0333333351f * 255.0f;
        if (pl->m_Blend > 255.0f) {
            pl->m_Blend = 255.0f;
        }
        plBlendMotSet(pl, EM_ARC(pl, 0xBA), EM_ARC(pl, 0xBD), 0, 0);
        MotionMove(pl, 0);
        pl->r_no_2 = pPL->pEmCatch->r_no_2;
        if (pl->Motion.Seq_frame > 77.6999969f && pl->Motion.Seq_frame < 78.3000031f) {
            pl->m_Work0 = SndCall(8, 0x28, &pl->getPartsPtr(0)->world, pl->pEmCatch->id, 0, pl);
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xC, 1);
        }
        break;
    case 2: {
        Vec v;

        v.x = -297.160004f;
        v.y = 0.0f;
        v.z = 1962.04004f;
        pl->ang.x = 0.0f;
        pl->ang.y = pl->pEmCatch->ang.y + 3.14159274f;
        pl->ang.z = 0.0f;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xBB), 0, 0, 1, 0);
        VibSetClearType(1);
        SndStop(pl->m_Work0, 0);
        PlSetDamageSe(0x10);
        pl->r_no_2++;
    }
    case 3:
        if (MotionMove(pl, 0)) {
            if (em2bCatchObj.p) {
                ObjMgr.destroy(em2bCatchObj.p);
                em2bCatchObj.p = 0;
            }
            pl->Wep->setTrans(1, 0);
            pl->be_flag |= 0x10;
            EndPlDamage();
        }
        break;
    case 4: {
        Vec v;

        v.x = -205.490005f;
        v.y = 0.0f;
        v.z = 2179.71997f;
        pl->ang.x = 0.0f;
        pl->ang.y = pl->pEmCatch->ang.y + 3.14159274f;
        pl->ang.z = 0.0f;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xBC), 0, 0, 1, 0);
        pG->pl_life = 0;
        PlSetDamageSe(0xA);
        VibSetClearType(1);
        pl->r_no_2++;
    }
    case 5:
        MotionMove(pl, 0);
        if (pl->Motion.Seq_frame > 63.7000008f && pl->Motion.Seq_frame < 64.3000031f) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Partner caught: squeezed until her life runs out or the parasite timer ends, then dropped.
static void em2b_R1_SubCatch(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x4A), ARC(0x98), 0, flip, 0);
        SetSubDamage(em, (void (*)()) subem2b_Catch);
        GameAddPoint(LVADD_PL_DAMAGE);
        w->Parasite_damage = 100;
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 4;
            break;
        }
        if ((s16) pG->ashley_life <= 1) {
            em->flag |= 8;
        }
        if (em->Motion.Seq_old.Free & 1) {
            LifeDownSet2(pSUB, 15, 0, 1);
            if ((s16) pG->ashley_life <= 1) {
                em->flag |= 8;
                em->r_no_2++;
                break;
            }
        }
        if (!(em->flag & 8) && (s16) pG->ashley_life > 1 && w->Parasite_damage <= 0) {
            em->r_no_2 = 4;
        }
        break;
    case 2: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x5C), ARC(0xA9), 0, flip, 0);
        EstSet(em, -1, 0, 0, w->Eff, 0x24, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    }
    case 3:
        em->flag |= 8;
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    case 4: {
        int flip = em2bFlip(w, 0x41, 1);

        MotionSetCore(em, &em->Motion, ARC(0x4B), ARC(0x99), 10, flip, 0);
        SetSubDamage(em, (void (*)()) subem2b_CatchEnd);
        w->Catch_power = 450;
        em->r_no_2++;
    }
    case 5:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
}

// Partner caught by the hand: hangs on the hand parts' matrix.
static void subem2b_CatchHand(cSubChar* sub)
{
    cSubChar* s = SUB_CHAR();

    s->subArc = s->pEmCatch->subArc;
    s->dmg.m_Timer = 2;
    StaFlagOn(pG, STA_SUB_CATCHED);
    switch (s->r_no_2) {
    case 0:
        s->atari.m_flag &= 0xFCFF;
        MotionSetCore(s, &s->Motion, PL_ARC_PTR(s->subArc, 0xD1), 0, 0, 0, 0);
        SubCharSetFace(1);
        s->be_flag &= ~0x10;
        s->r_no_2++;
    case 1: {
        cModel* p = s->pEmCatch->getPartsPtr(0xA);
        Vec pos;
        Vec rot;

        rot.x = -0.05022185f;
        rot.y = 1.9358388f;
        rot.z = -0.4252966f;
        pos.x = -356.279999f;
        pos.y = 162.869995f;
        pos.z = 543.849976f;
        RotMatrix(s->mat, &rot);
        TransMatrix(s->mat, &pos);
        ScaleMatrix(s->mat, &s->scale);
        PSMTXConcat(p->mat, s->mat, s->mat);
        s->Motion.Mot_flag |= 0x40000000;
        PSMTXMultVec(p->mat, &pos, &s->pos);
        PSVECSubtract(&s->pos, &s->pEmCatch->pos, &pos);
        s->ang.x = 0.0f;
        s->ang.y = atan2f(pos.x, pos.z);
        s->ang.z = 0.0f;
        MotionMove(s, 0);
        if (s->pEmCatch->hp <= 0) {
            SetSubDamage(s->pEmCatch, (void (*)()) subem2b_CatchEnd);
        }
        break;
    }
    }
    s->subArc = s->subArc2;
}

// Partner squeezed: follows the giant's step; her life is emptied when the timer ends.
static void subem2b_Catch(cSubChar* sub)
{
    cSubChar* s = SUB_CHAR();

    StaFlagOn(pG, STA_SUB_CATCHED);
    s->subArc = s->pEmCatch->subArc;
    s->dmg.m_Timer = 2;
    switch (s->r_no_2) {
    case 0: {
        Vec v;

        s->Motion.Mot_flag &= ~0x40000000;
        v.x = 1271.0f;
        v.y = 0.0f;
        v.z = 478.769989f;
        s->ang.x = 0.0f;
        s->ang.y = s->pEmCatch->ang.y + 3.14159274f;
        s->ang.z = 0.0f;
        LIMIT_ANGLE(s->ang.y);
        PSMTXMultVec(s->pEmCatch->mat, &v, &s->pos);
        MotionSetCore(s, &s->Motion, PL_ARC_PTR(s->subArc, 0xD2), 0, 0, 1, 0);
        SubCharSetFace(1);
        s->r_no_2++;
    }
    case 1:
        MotionMove(s, 0);
        s->r_no_2 = s->pEmCatch->r_no_2;
        if (s->pEmCatch->r_no_0 != 1 && s->pEmCatch->r_no_1 != 0x13) {
            SetSubDamage(s->pEmCatch, (void (*)()) subem2b_CatchEnd);
        }
        break;
    case 2:
        MotionSetCore(s, &s->Motion, PL_ARC_PTR(s->subArc, 0xD4), 0, 0, 1, 0);
        s->m_Work0 = 67;
        s->r_no_2++;
    case 3:
        MotionMove(s, 0);
        if (s->m_Work0) {
            s->m_Work0--;
            if (s->m_Work0 == 0) {
                pG->ashley_life = 0;
            }
        }
        if ((s16) pG->ashley_life > 0 && (s->pEmCatch->r_no_0 != 1 && s->pEmCatch->r_no_1 != 0x13)) {
            SetSubDamage(s->pEmCatch, (void (*)()) subem2b_CatchEnd);
        }
        break;
    }
    s->subArc = s->subArc2;
}

// Partner dropped: falls in front of the giant.
static void subem2b_CatchEnd(cSubChar* sub)
{
    cSubChar* s = SUB_CHAR();

    s->subArc = s->pEmCatch->subArc;
    StaFlagOn(pG, STA_SUB_CATCHED);
    switch (s->r_no_2) {
    case 0: {
        Vec v;

        s->Motion.Mot_flag &= ~0x40000000;
        v.x = -284.0f;
        v.y = 0.0f;
        v.z = 2067.51001f;
        s->ang.x = 0.0f;
        s->ang.y = s->pEmCatch->ang.y + 3.14159274f;
        s->ang.z = 0.0f;
        LIMIT_ANGLE(s->ang.y);
        PSMTXMultVec(s->pEmCatch->mat, &v, &s->pos);
        s->dmg.m_Timer = 0x1E;
        MotionSetCore(s, &s->Motion, PL_ARC_PTR(s->subArc, 0xD3), 0, 0, 1, 0);
        SubCharSetFace(1);
        s->r_no_2++;
    }
    case 1:
        if (MotionMove(s, 0)) {
            s->be_flag |= 0x10;
            EndSubDamage();
        }
        break;
    }
    s->subArc = s->subArc2;
}

// Stamps the ground next to the tower (room 224): shakes it and drops the player standing on it.
static void em2b_R1_BaseAtk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0:
        em2bYaguraSearch(em);
        {
            int flip = em2bFlip(w, 0x41, 1);

            MotionSetCore(em, &em->Motion, ARC(0xE5), 0, 10, flip, 0);
        }
        w->Atk_ck = 0;
        w->Timer = 15;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.0981747732f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_frame > 44.7000008f && em->Motion.Seq_frame < 45.2999992f) {
            em2bPlFallCK(em);
            if (w->pYagura) {
                w->pYagura->setVib();
            }
            SndCall(6, 0xC, &em->getPartsPtr(0)->world, 0, 0, em);
        }
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// Room 224 hole: climbs out at the fixed position, grabs the player who comes near the hole.
static void em2b_R1_HoleAtk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cModel* p = em->getPartsPtr(0);
    f32 d;

    switch (em->r_no_2) {
    case 0:
        em->pos = em2b_r11e_pos;
        if (pG->room_id == 0x224) {
            void* tbl = w->Tex_buf;

            if (w->pMgr) {
                em->pModelInfo->setTexBlendTbl(tbl);
                em->pModelInfo->setBlendRatio(0xFF);
                em->pModelInfo->setBlendType(2);
                if (w->pHead) {
                    w->pHead->setTexBlendTbl(tbl);
                    w->pHead->setBlendRatio(0xFF);
                    w->pHead->setBlendType(2);
                }
            }
        }
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0xE6), ARC(0xE7), 0, 1, 0);
        MotionMove(em, 0);
        if (!RmfFlagChk(pG, RMF_R119_DESTROY_KOYA_A)) {
            em->r_no_2 = 4;
            break;
        }
        if ((s16) pG->pl_life > 0 && !EmDeadCk(pPL)) {
            f32 dx = pPL->pos.x - em2b_r11e_pos.x;
            f32 dz = pPL->pos.z - em2b_r11e_pos.z;
            d = dx * dx + dz * dz;
            if (d < 64000000.0f) {
                em->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0xE6), ARC(0xE7), 0, 1, 0);
        w->Atk_ck = 0;
        EstSet(em, -1, 0, 0, EFF_ROOM, 0x1D, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
            break;
        }
        if (em->Motion.Seq_old.Free & 4) {
            SndCall(6, 0xD, &p->world, 0, 0, em);
        }
        if (em->Motion.Seq_old.Free & 2) {
            SndCall(6, 0xE, &p->world, 0, 0, em);
        }
        if ((s16) pG->pl_life > 0 && !EmDeadCk(pPL) && (em->Motion.Seq_old.Free & 1) && w->Atk_ck == 0) {
            Vec v;
            f32 dx;
            f32 dz;

            cModel* hp = em->getPartsPtr(0xA);
            v = pPL->pos;
            dz = hp->world.z - v.z;
            dx = hp->world.x - v.x;
            d = dx * dx + dz * dz;
            if (d < 2250000.0f && !EmDeadCk(pPL)) {
                pPL->dmg.m_Timer = 0x80;
                pG->pl_life = 0;
                SetPlDamage(em, plem2b_CatchHand);
                SndCall(8, 0x24, &hp->world, em->id, 0, em);
                VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
                w->Atk_ck = 1;
            }
        }
        break;
    case 4:
        break;
    }
}

// The player standing higher than the giant's feet + 2000 (on the tower) falls off.
void em2bPlFallCK(cEm2b* em)
{
    if (EmDeadCk(pPL)) {
        return;
    }
    if ((s16) pG->pl_life <= 0) {
        return;
    }
    if (pPL->pos.y < em->pos.y + 2000.0f) {
        return;
    }
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
    pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
    SetPlDamage(em, plem2bDmFall);
}

// Player knocked off the tower: falls until the floor, then lands and gets up.
static void plem2bDmFall(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 2;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xEA), 0, 3, 0x201, 0);
        pl->atari.throughOn();
        if ((s16) pG->pl_life > 0) {
            PlSetDamageSe(0);
        } else {
            PlSetDamageSe(0xD);
        }
        pl->m_Work0 = 62;
        pl->m_Work1 = 40;
        pl->r_no_2++;
    case 1:
        pl->ang.y += Muku2(pl->ang.y, -2.18000007f, 0.196349546f);
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        if (pl->m_Work1) {
            pl->m_Work1--;
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_C);
        }
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            f32 y = SatMgr.getFloor(&pl->pos, 0, pl->pos_old.y - pl->pos.y + 2000.0f, 100000.0f, 0);
            if (pl->pos.y < y) {
                pl->pos.y = y;
                if (pl->Motion.Seq_frame > 63.7000008f && pl->Motion.Seq_frame < 64.3000031f) {
                    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
                }
                MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xE9), 0, 3, 1, 0);
                MotionMove(pl, 0);
                pl->r_no_2++;
                break;
            }
        }
        MotionMove(pl, 0);
        break;
    case 2:
        LifeDownSet(pPL, 500, 0);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xE9), 0, 3, 1, 0);
        pl->r_no_2++;
    case 3:
        if (MotionMove(pl, 0) && (s16) pG->pl_life > 0) {
            pl->atari.throughOff();
            EmRoutineSet(pPL, 1, 0, 0xA, 0);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// R0 == 2: damage (Be_flg bit3), runs Em2b_R1_dm_tbl[r_no_1].
static void em2b_R0_Damage(cEm2b* em)
{
    EM2B_WK(em)->Be_flg |= 8;
    Em2b_R1_dm_tbl[em->r_no_1](em);
}

// Creates the parasite head object on the neck parts (0x3E) with its idle motion and effect.
static inline void em2bParasiteSet(cEm2b* em, Em2bWork* w, int hokan)
{
    w->pParasite = (cObj16*) SetObj16(ARC(0xD6), ARC(0xD7), em, em, 0x3E, 8, 0, 0);
    if (w->pParasite) {
        MotSetObj16(w->pParasite, ARC(0xDA), 0, hokan);
        EstSet(w->pParasite, -1, 0, 0, w->Eff, 0x1E, 0, w->espKind, w->pParasite, 0);
    }
}

// Face damage: kneels, the parasite comes out of the neck and can be attacked while it is out.
static void em2b_R1_Dm_Face(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 0x4000;
    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 5, 0x45);

        if (!(w->Be_flg & 0x1000) && pG->stage_no == 1 && pG->room_no == 0x19) {
            MotionSetCore(em, &em->Motion, ARC(0x2D), ARC(0x74), 30, flip, 100);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x2D), ARC(0x74), 30, flip, 0);
        }
        EstSet(em, -1, 0, 0, w->Eff, 0xA, 0, ESP_CORE_KIND_NONE, em, 0);
        if (w->pRock) {
            EmAtkInfo* atk = &em2b_atk_info[6];

            if ((s16) pG->pl_life > 1) {
                atk->flag |= 4;
            } else {
                atk->flag &= ~4;
            }
            w->pRock->setFall(atk);
            w->pRock->setSeFall(8, 0xA, em->id);
            w->pRock->setEffFall(1, 7);
            w->pRock = 0;
        }
        em2bParasiteDelete(w);
        em2bSetTentacle(em, 0);
        w->pParasite = (cObj16*) SetObj16(ARC(0xD6), ARC(0xD7), em, em, 0x3E, 8, 0, 0);
        if (w->pParasite) {
            if (!(w->Be_flg & 0x1000) && pG->stage_no == 1 && pG->room_no == 0x19) {
                MotSetObj16(w->pParasite, ARC(0xDA), 0, 100);
            } else {
                MotSetObj16(w->pParasite, ARC(0xDA), 0, 0);
            }
            EstSet(w->pParasite, -1, 0, 0, w->Eff, 0x1E, 0, w->espKind, w->pParasite, 0);
        }
        em2bSetTentacle(em, 1);
        w->Be_flg |= 0x1000;
        em->r_no_2++;
    }
    case 1:
        em->flag |= 8;
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2: {
        int flip = em2bFlip(w, 5, 0x45);

        MotionSetCore(em, &em->Motion, ARC(0x2E), ARC(0x75), 30, flip, 0);
        if (w->pParasite) {
            MotSetObj16(w->pParasite, ARC(0xD8), 4, 0);
        }
        em->r_no_2++;
    }
    case 3: {
        int end;

        em->flag |= 8;
        w->Be_flg |= 0x2000;
        end = MotionMove(em, 0);
        if (end) {
            em->r_no_2++;
        } else if (em->l_pl < 25000000.0f) {
            ActBtn.set(ACT_CLIMB, 0xB, (void*) em2bSetActAtkParasite, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, end);
        }
        break;
    }
    case 4: {
        int flip = em2bFlip(w, 5, 0x45);

        MotionSetCore(em, &em->Motion, ARC(0x2F), ARC(0x76), 30, flip, 0);
        if (w->pParasite) {
            MotSetObj16(w->pParasite, ARC(0xDB), 0, 0);
        }
        w->Total_damage = 0;
        w->Timer = 15;
        em->r_no_2++;
    }
    case 5:
        w->Be_flg |= 0x10;
        w->Be_flg &= ~8;
        if (MotionMove(em, 0)) {
            em2bParasiteDelete(w);
            em2bSetTentacle(em, 0);
            em2bNextRtnSet(em);
        }
        break;
    }
}

// Damage while holding the tree: drops the tree and the parasite comes out.
static void em2b_R1_Dm_Tree(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cEmTree* tree = w->pTree;

    w->Be_flg |= 0x4000;
    switch (em->r_no_2) {
    case 0: {
        Vec v;

        tree->clearParent();
        v.x = 2494.25f;
        v.y = 0.0f;
        v.z = 1835.70996f;
        PSMTXMultVec(em->mat, &v, &tree->pos);
        tree->ang.y = em->ang.y;
        MotionSetCore(tree, &tree->Motion, ARC(0xB0), 0, 0, 1, 0);
        w->pTreeBrk = tree;
        w->No_go_sub_timer = 15;
        w->pTree = 0;
        MotionSetCore(em, &em->Motion, ARC(0x4E), ARC(0x9C), 10, 1, 0);
        EstSet(em, -1, 0, 0, w->Eff, 0xA, 0, ESP_CORE_KIND_NONE, em, 0);
        em2bParasiteDelete(w);
        em2bSetTentacle(em, 0);
        em2bParasiteSet(em, w, 0);
        em2bSetTentacle(em, 1);
        em->r_no_2++;
    }
    case 1:
        em->flag |= 8;
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 2, 0, 2, 0);
        }
        break;
    }
}

// Action button on the exposed parasite: both go into the parasite attack routine.
static void em2bSetActAtkParasite(cEm2b* em)
{
    EmRoutineSet(em, 2, 2, 0, 0);
    em->dmg.set(0, 30);
    pPL->dmg.set(0, 30);
}

// Killed through the parasite: the death with the dead-body collision off.
static inline void em2bParasiteDieSet(cEm2b* em)
{
    em->hp = 0;
    EmSetDie(em);
    EmReserveDropItem(em);
    EmSetDieCnt(em);
    em->clearStatus(EM_STATUS_ACTIVE);
    em->atari.m_flag &= ~0x100;
    EmRoutineSet(em, 3, 0, 0, 0);
}

// Action button prompt of the parasite attack (the button is chosen at random above rank 1).
static inline void em2bParasiteBtnSet(Em2bWork* w)
{
    if (w->Button_mode) {
        ActBtn.set(ACT_QUICK_STICK, 0xB, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_B_RAPID, ACT_FUNC_NORMAL, 0);
    } else {
        ActBtn.set(ACT_QUICK_STICK, 0xB, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
    }
}

// Parasite attack: the player climbs the back and slashes the parasite while the button is mashed.
static void em2b_R1_Dm_Parasite(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0: {
        int side = 0;

        if (w->Pl_rot < 1.57079637f) {
            side = 1;
        }
        em->atari.throughOn();
        em2bCatchPosSet(em);
        MotionSetCore(em, &em->Motion, ARC(0x30), ARC(0x77), 0, 1, 0);
        SetPlDamage(em, plem2b_AtkParasite);
        pPL->r_no_3 = side;
        if (w->pParasite) {
            MotSetObj16(w->pParasite, ARC(0xD9), 0, 0);
        }
        w->TmpU32 = 0;
        w->Timer = 45;
        if (pG->Game_level <= 1) {
            w->Timer = 30;
        }
        if (pG->Game_level <= 3) {
            w->Timer = 40;
        }
        if (pG->Game_level > 6) {
            w->Timer = 50;
        }
        if (pG->Game_level > 9) {
            w->Timer = 55;
        }
        w->Button_mode = Rnd() & 1;
        if (pG->Game_level <= 1) {
            w->Button_mode = 0;
        }
        StaFlagOn(pG, STA_SUSPEND);
        pPL->setNoSuspend(1);
        em->setNoSuspend(1);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        em->r_no_2++;
    }
    case 1:
        em->flag |= 8;
        if (w->Timer) {
            w->Timer--;
        } else if (w->Button_mode) {
            if (Key.trg & 0x40000) {
                w->TmpU32++;
            }
            ActBtn.set(ACT_QUICK_STICK, 0xB, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_B_RAPID, ACT_FUNC_NORMAL, 0);
        } else {
            if (Key.trg & 0x80000) {
                w->TmpU32++;
            }
            ActBtn.set(ACT_QUICK_STICK, 0xB, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
        }
        if (MotionMove(em, 0)) {
            em->atari.throughOff();
            if (w->TmpU32 == 0) {
                em->r_no_2 = 8;
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 2:
        em->r_no_2++;
    case 3:
        em->r_no_2++;
    case 4:
        MotionSetCore(em, &em->Motion, ARC(0x2E), 0, 0, 1, 0);
        if (w->pParasite) {
            MotSetObj16(w->pParasite, ARC(0xD8), 0, 0);
        }
        w->Timer = 114;
        em->r_no_2++;
    case 5:
        em->flag |= 8;
        em2bParasiteBtnSet(w);
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 6:
        MotionSetCore(em, &em->Motion, ARC(0x5E), ARC(0xAA), 0, 1, 0);
        if (w->pParasite) {
            MotSetObj16(w->pParasite, ARC(0xDD), 0, 0);
        }
        w->Timer = 15;
        EstSet(em, -1, 0, 0, w->Eff, 0xB, 0, ESP_CORE_KIND_NONE, em, 0);
        w->TmpU32 = 0;
        StaFlagOff(pG, STA_SUSPEND);
        pPL->setNoSuspend(0);
        em->setNoSuspend(0);
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
        em->r_no_2++;
    case 7:
        if (w->Timer) {
            w->Timer--;
            if (w->Timer == 0) {
                em2bSetTentacle(em, 0);
            }
        }
        if (MotionMove(em, 0)) {
            em2bParasiteDelete(w);
            em2bSetTentacle(em, 0);
            if (em->hp <= 0) {
                em2bParasiteDieSet(em);
            } else {
                em2bNextRtnSet(em);
            }
        } else {
            if (em->Motion.Seq_frame > 74.6999969f && em->Motion.Seq_frame < 75.3000031f) {
                em2bParasiteDelete(w);
                em2bSetTentacle(em, 0);
                if (em->hp <= 0) {
                    em2bParasiteDieSet(em);
                    if (em2bCatchObj.p) {
                        ObjMgr.destroy(em2bCatchObj.p);
                        em2bCatchObj.p = 0;
                    }
                    pPL->Wep->setTrans(1, 0);
                    break;
                }
                w->TmpU32 = 1;
            }
            if (w->TmpU32) {
                w->Be_flg &= ~8;
            }
        }
        break;
    case 8:
        MotionSetCore(em, &em->Motion, ARC(0x5F), ARC(0xAB), 0, 1, 0);
        EstSet(em, -1, 0, 0, w->Eff, 0x22, 0, ESP_CORE_KIND_NONE, em, 0);
        if (w->pParasite) {
            MotSetObj16(w->pParasite, ARC(0xDE), 0, 0);
        }
        w->Total_damage = 0;
        StaFlagOff(pG, STA_SUSPEND);
        pPL->setNoSuspend(0);
        em->setNoSuspend(0);
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
        em->r_no_2++;
    case 9:
        if (MotionMove(em, 0)) {
            em2bParasiteDelete(w);
            em2bSetTentacle(em, 0);
            w->Dash_wait = 150;
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
}

// Parasite killed from outside: the parasite dies on the back and the giant collapses.
static void em2b_R1_Dm_Parasite2(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 0x4800;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x5E), ARC(0xAA), 120, 1, 0);
        if (w->pParasite) {
            MotSetObj16(w->pParasite, ARC(0xDD), 0, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em2bParasiteDelete(w);
            em2bSetTentacle(em, 0);
            if (em->hp <= 0) {
                em2bParasiteDieSet(em);
            } else {
                em2bNextRtnSet(em);
            }
        } else if ((em->Motion.Seq_old.Free & 4) && em->hp <= 0) {
            em2bParasiteDieSet(em);
        }
        break;
    }
}

// The parasite attack button of the giant the player is on (Key.trg bit per em2bWork::atkBtn).
// A macro: the inline's `return 1; return 0;` is materialised (`li r0,1; b; li r0,0; cmpwi`) at both users.
#define em2bParasiteBtnCk(w) (((w)->Button_mode == 0 && (Key.trg & 0x80000)) || ((w)->Button_mode == 1 && (Key.trg & 0x40000)))

// Puts the player at `v` in the giant's frame with its rotation.
static inline void em2bPlOnEmSet(cPlayer* pl, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    pl->ang.y = pl->pEmCatch->ang.y;
    PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
}

// Same, facing the other way (the Vec temp shares the frame slot of em2bPlOnEmSet's; a caller `Vec v` in the
// other arm pushes the inline's temp to a second slot and its address into a pseudo).
static inline void em2bPlOnEmSetRev(cPlayer* pl, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    pl->ang.y = LIMIT_ANGLE(pl->ang.y = pl->pEmCatch->ang.y + 3.14159274f);
    PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
}

// Player attacking the parasite: climbs the back, slashes it while the button is mashed, is thrown off.
static void plem2b_AtkParasite(cPlayer* pl)
{
    Em2bWork* w = EM2B_WK(pPL->pEmCatch);

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 2;
    switch (pl->r_no_2) {
    case 0:
        if (pl->r_no_3) {
            em2bPlOnEmSetRev(pl, 1634.15002f, 0.0f, 6410.41016f);
            MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC6), 0, 0, 1, 0);
        } else {
            em2bPlOnEmSet(pl, -93.6600037f, 0.0f, -4031.65991f);
            MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xB9), 0, 0, 1, 0);
        }
        pl->Wep->setTrans(0, 0);
        em2bCatchObj.p = ObjMgr.create(cObjMgr::ID_PL_WEAPON);
        if (em2bCatchObj.p) {
            em2bCatchObj.p->modelInit(EM_ARC(pl, 0x18), EM_ARC(pl, 0x17));
            em2bCatchObj.p->atari.m_flag &= 0xFCFF;
            em2bCatchObj.p->pParts->pParent = pPL->getPartsPtr(0xA);
            em2bCatchObj.p->LightInfo.init2(1, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 500.0f, 0.0f, 0.0f }), 1);
            ((cObjWep*) em2bCatchObj.p)->m_pParent = pPL;
            em2bCatchObj.p->setNoSuspend(1);
        }
        pl->atari.m_flag &= 0xFCFF; // throughOn(): the inline's `this` pseudo gives `addi r9,pl,692` and keeps the xFE load below the store
        pl->r_no_2++;
    case 1:
        MotionMove(pl, 0);
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        if (pl->Motion.Seq_frame > 14.6999998f && pl->Motion.Seq_frame < 15.3000002f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->Motion.Seq_frame > 23.7000008f && pl->Motion.Seq_frame < 24.2999992f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if ((pl->Motion.Seq_frame > 31.7000008f && pl->Motion.Seq_frame < 32.2999992f) || (pl->Motion.Seq_frame > 41.7000008f && pl->Motion.Seq_frame < 42.2999992f)
            || (pl->Motion.Seq_frame > 55.7000008f && pl->Motion.Seq_frame < 56.2999992f) || (pl->Motion.Seq_frame > 73.6999969f && pl->Motion.Seq_frame < 74.3000031f)) {
            SndCall(8, 0x2C, &pl->pos, pl->pEmCatch->id, 0, pl);
        }
        break;
    case 2:
        em2bPlOnEmSet(pl, -214.699997f, 0.0f, 807.640015f);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC7), 0, 0, 1, 0);
        pl->m_Work0 = 0;
        pl->r_no_2++;
    case 3:
        em2bParasiteAtkCamMove((cEm2b*)pl->pEmCatch);
        if (em2bParasiteBtnCk(w)) {
            pl->m_Work0++;
        }
        if (MotionMove(pl, 0)) {
            pl->r_no_2++;
        }
        break;
    case 4:
        pl->m_Work0 *= 100;
        pl->m_Work1 = 0;
        pl->m_Work2 = 0;
        pl->m_Work6 = 0;
        pl->m_Work7 = 0;
        SndCall(1, 0x10, &pl->pos, 0, 0, pl);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC8), EM_ARC(pl, 0xC9), 3, 5, 0);
        pl->r_no_2++;
    case 5: {
        int lvl;

        em2bParasiteAtkCamMove((cEm2b*)pl->pEmCatch);
        if (em2bParasiteBtnCk(w)) {
            lvl = 8;
            if (pG->Game_level <= 1) {
                lvl = 12;
            }
            if (pG->Game_level <= 3) {
                lvl = 10;
            }
            if (pG->Game_level > 6) {
                lvl = 6;
            }
            if (pG->Game_level > 9) {
                lvl = 4;
            }
            pl->m_Work0 += lvl;
        }
        lvl = ((int) pl->m_Work0 + 90) / 100;
        if (lvl > 7) {
            lvl = 7;
        }
        if (lvl < 0) {
            lvl = 0;
        }
        if (lvl != pl->m_Work1) {
            void* mot;
            u32 len;
            u32 n;

            pl->m_Work1 = lvl;
            switch (lvl) {
            case 0:
            default:
                mot = EM_ARC(pl, 0xC9);
                break;
            case 1:
                mot = EM_ARC(pl, 0xCA);
                break;
            case 2:
                mot = EM_ARC(pl, 0xCB);
                break;
            case 3:
                mot = EM_ARC(pl, 0xCC);
                break;
            case 4:
                mot = EM_ARC(pl, 0xCD);
                break;
            case 5:
                mot = EM_ARC(pl, 0xCE);
                break;
            case 6:
                mot = EM_ARC(pl, 0xCF);
                break;
            case 7:
                mot = EM_ARC(pl, 0xD0);
                break;
            }
            f32 ratio = pl->Motion.Seq_frame / (f32) pl->Motion.Seq_frame_num; // the u16 psq_l division before the u32 double trick (r225 rule)
            len = ((MotionData*) mot)->maxFrame;
            n = (u32) ((f32) len * ratio) + 1;
            if (n >= len) {
                n = 0;
            }
            MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC8), mot, pl->Motion.Hokan_cnt, 5, (u16) n);
        }
        MotionMove(pl, 0);
        if ((pl->Motion.Mot_frame >= 10.0f && pl->Motion.Mot_frame <= 15.0f) || (pl->Motion.Mot_frame >= 44.0f && pl->Motion.Mot_frame <= 49.0f)) {
            if (pl->m_Work6 == 0) {
                SndCall(8, 0x30, &pl->pos, pl->pEmCatch->id, 0, pl);
            }
            pl->m_Work6 = 1;
        } else {
            pl->m_Work6 = 0;
        }
        if ((pl->Motion.Mot_frame >= 50.0f && pl->Motion.Mot_frame <= 55.0f) || (pl->Motion.Mot_frame >= 13.0f && pl->Motion.Mot_frame <= 18.0f)) {
            if (pl->m_Work7 == 0) {
                SndCall(8, 0x2F, &pl->pos, pl->pEmCatch->id, 0, pl);
                SndCall(8, 0x2D, &pl->pos, pl->pEmCatch->id, 0, pl);
                if (w->pParasite) {
                    MotSetObj16(w->pParasite, EM_ARC(pl, 0xD8), 0, 3);
                    EstSet(w->pParasite, -1, 0, 0, w->Eff, 0x1F, 0, ESP_CORE_KIND_NONE, w->pParasite, 0);
                }
                EstSet(pl, -1, 0, 0, w->Eff, 0x20, 0, ESP_CORE_KIND_NONE, pl, 0);
                pl->pEmCatch->hp -= 70;
            }
            pl->m_Work7 = 1;
        } else {
            pl->m_Work7 = 0;
        }
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        break;
    }
    case 6:
        em2bPlOnEmSet(pl, -107.860001f, 0.0f, 898.02002f);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC4), 0, 0, 1, 0);
        pl->r_no_2++;
    case 7:
        if (MotionMove(pl, 0)) {
            if (em2bCatchObj.p) {
                ObjMgr.destroy(em2bCatchObj.p);
                em2bCatchObj.p = 0;
            }
            pl->Wep->setTrans(1, 0);
            EndPlDamage();
        } else {
            if (pl->Motion.Seq_frame > 31.7000008f && pl->Motion.Seq_frame < 32.2999992f) {
                SndCall(1, 0x4E, &pl->pos, 0, 0, pl);
            }
            if (pl->Motion.Seq_frame > 37.7000008f && pl->Motion.Seq_frame < 38.2999992f) {
                SndCall(5, 0x14, &pl->pos, 0, 0, pl);
            }
            if ((pl->Motion.Seq_frame > 52.7000008f && pl->Motion.Seq_frame < 53.2999992f) || (pl->Motion.Seq_frame > 65.6999969f && pl->Motion.Seq_frame < 66.3000031f)) {
                SndCall(5, 0, &pl->pos, 0, 0, pl);
                SndCall(5, 1, &pl->pos, 0, 0, pl);
            }
        }
        break;
    case 8:
        em2bPlOnEmSet(pl, -60.8300018f, 0.0f, 1075.98999f);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC5), 0, 0, 1, 0);
        pl->r_no_2++;
    case 9:
        if (pl->Motion.Seq_frame > 86.6999969f && pl->Motion.Seq_frame < 87.3000031f) {
            LifeDownSet(pPL, 800, 0);
            if ((s16) pG->pl_life <= 0) {
                PlSetDamageSe(0xD);
            } else {
                PlSetDamageSe(0xA);
            }
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
        }
        if (MotionMove(pl, 0) && (s16) pG->pl_life > 0) {
            if (em2bCatchObj.p) {
                ObjMgr.destroy(em2bCatchObj.p);
                em2bCatchObj.p = 0;
            }
            pl->Wep->setTrans(1, 0);
            pl->atari.throughOff();
            EmRoutineSet(pPL, 1, 0, 0xA, 0);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Camera of the parasite attack: a fixed offset in the player's frame, lifted to the head parts.
void em2bParasiteAtkCamMove(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cModel* p;
    Vec pos;
    Vec at;

    w->Cam.param.fovy = 50.0f;
    p = pPL->getPartsPtr(0);
    at.x = 0.0f;
    at.y = 0.0f;
    at.z = 1000.0f;
    pos.x = -3000.0f;
    pos.y = 0.0f;
    pos.z = 1000.0f;
    PSMTXMultVec(pPL->mat, &pos, &pos);
    PSMTXMultVec(pPL->mat, &at, &at);
    pos.y += p->world.y - pPL->pos.y;
    at.y += p->world.y - pPL->pos.y;
    w->Cam.param.pos = pos;
    w->Cam.param.at = at;
    w->Cam.Up.x = 0.0f;
    w->Cam.Up.y = 1.0f;
    w->Cam.Up.z = 0.0f;
    w->Cam.Distance = VEC_DIST(&w->Cam.param.pos, &w->Cam.param.at);
    CameraSetOrientationUp(&w->Cam);
    CamCtrl.m_pExtraCamera = (s32) &w->Cam;
}

// Hit by the thrown-back rock.
static void em2b_R1_Dm_Rock(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0: {
        int flip = em2bFlip(w, 5, 0x45);

        MotionSetCore(em, &em->Motion, ARC(0x5A), ARC(0xA7), 30, flip, 0);
        em2bParasiteDelete(w);
        em2bSetTentacle(em, 0);
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// Drops the held tree to the ground next to the foot (flash / bomb damage).
static inline void em2bTreeDrop(cEm2b* em, Em2bWork* w)
{
    cEmTree* tree = w->pTree;

    if (tree) {
        Vec v;

        tree->clearParent();
        v.x = 2494.25f;
        v.y = 0.0f;
        v.z = 1835.70996f;
        PSMTXMultVec(em->mat, &v, &tree->pos);
        tree->ang.y = em->ang.y;
        MotionSetCore(tree, &tree->Motion, ARC(0xB0), 0, 0, 1, 0);
        w->pTreeBrk = tree;
        w->No_go_sub_timer = 15;
        w->pTree = 0;
    }
}

// Flash grenade damage.
static void em2b_R1_Dm_Flash(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int rtn = em->r_no_2;

    w->Be_flg |= 0x400;
    w->Be_flg &= ~8;
    switch (rtn) {
    case 0:
        em2bTreeDrop(em, w);
        {
            int flip = em2bFlip(w, 5, 0x45);

            MotionSetCore(em, &em->Motion, ARC(0x60), ARC(0xAC), 15, flip, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// Explosion damage.
static void em2b_R1_Dm_Bomb(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int rtn = em->r_no_2;

    w->Be_flg |= 0x400;
    w->Be_flg &= ~8;
    switch (rtn) {
    case 0:
        em2bTreeDrop(em, w);
        {
            int flip = em2bFlip(w, 5, 0x45);

            MotionSetCore(em, &em->Motion, ARC(0x61), ARC(0xAD), 15, flip, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em2bNextRtnSet(em);
        }
        break;
    }
}

// R0 == 3: death (Be_flg bit3), runs Em2b_R1_die_tbl[r_no_1].
static void em2b_R0_Die(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    w->Be_flg |= 8;
    Em2b_R1_die_tbl[em->r_no_1](em);
}

// Normal death: falls forward; while falling the feet crush and the player can dash out from under.
static void em2b_R1_Die_Normal(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x4F), ARC(0x9D), 3, 1, 0);
        em2bParasiteDelete(w);
        em2bSetTentacle(em, 0);
        EstSet(em, -1, 0, 0, w->Eff, 0x11, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->clearStatus(EM_STATUS_ACTIVE);
            em->setStatus(EM_STATUS_ITEMSET);
            EmSetDropItem(em);
            w->Compress_y = 1.0f;
            EmRoutineSet(em, 3, 1, 0, 0);
        } else {
            if (em->Motion.Seq_old.Free & 1) {
                em2bPressPlCk(em);
                em2bPressSubCk(em);
            }
            if (em->Motion.Seq_old.Free & 4) {
                Mtx inv;
                Vec v;

                PSMTXInverse(em->mat, inv);
                PSMTXMultVec(inv, &pPL->pos, &v);
                if (v.x > -3000.0f && v.x < 3000.0f && v.y > -2000.0f && v.y < 2000.0f && v.z > 0.0f && v.z < 12000.0f) {
                    ActBtn.set(ACT_GUARD, 0xB, (void*) em2bDashEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
                }
            }
        }
        break;
    }
}

// Lost: sinks into the ground shrinking, then fades out with the rope / chain objects.
static void em2b_R1_Die_Lost(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.throughOn();
        w->Timer = 30;
        w->Timer2 = 150;
        SndCall(8, 0x34, &em->pos, em->id, 0, em);
        w->Compress_y = 1.0f;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Compress_y -= 0.00300000003f;
            if (w->Compress_y < 0.100000001f) {
                w->Compress_y = 0.100000001f;
            }
            em->pos.y -= 6.0f;
        }
        TransMatrix(em->mat, &em->pos);
        if (w->Timer2) {
            w->Timer2--;
            break;
        }
        em->invisible_factor -= 0.100000001f;
        if (w->pChain) {
            w->pChain->invisible_factor = em->invisible_factor;
        }
        if (w->pChain2) {
            w->pChain2->invisible_factor = em->invisible_factor;
        }
        if (w->pChain2) {
            w->pChain3->invisible_factor = em->invisible_factor;
        }
        if (em->invisible_factor <= 0.0f) {
            em->invisible_factor = 0.0f;
            em->be_flag &= ~2;
            em->be_flag |= 0x4000;
            if (w->pChain) {
                w->pChain->be_flag &= ~2;
            }
            if (w->pChain2) {
                w->pChain2->be_flag &= ~2;
            }
            if (w->pChain3) {
                w->pChain3->be_flag &= ~2;
            }
            em->r_no_2++;
        }
        break;
    }
}

// Event death (room 119): lies down at a fixed spot facing the player, the trees are lost.
static void em2b_R1_Die_Event(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int rtn = em->r_no_2;

    switch (rtn) {
    case 0: {
        Vec v;

        em->pos.x = 114638.0f;
        em->pos.y = 2100.0f;
        em->pos.z = 5681.25f;
        em->ang.y = GetXZAngle(&em->pos, &pPL->pos);
        MotionSetCore(em, &em->Motion, ARC(0x3C), 0, 0, 1, 0);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->setStatus(EM_STATUS_ITEMSET);
        em->atari.setFlag200();
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        SetObaModel((cObj*) em, 3, &v, 1000.0f, 1000.0f, 0);
        SetObaModel((cObj*) em, 7, &v, 1000.0f, 1000.0f, 0);
        SetObaModel((cObj*) em, 0xD, &v, 1000.0f, 1000.0f, 0);
        if (w->pTree) {
            w->pTree->setLost();
            w->pTree->be_flag &= ~2;
            w->pTree = 0;
        }
        if (w->pTreeBrk) {
            w->pTreeBrk->setLost();
            w->pTreeBrk->be_flag &= ~2;
            w->pTreeBrk = 0;
        }
        em->r_no_2++;
    }
    case 1:
        MotionMove(em, 0);
        break;
    }
}

// Room 224: dropped from the cage into the lava.
static void em2b_R1_Die_R224Drop(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cModel* p = em->getPartsPtr(0);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0xE3), ARC(0xE4), 3, 1, 0);
        em2bParasiteDelete(w);
        em2bSetTentacle(em, 0);
        EstSet(em, -1, 0, 0, EFF_ROOM, 0x1C, 1, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 2) {
            SndCall(6, 0xD, &p->world, 0, 0, em);
        }
        if (em->Motion.Seq_old.Free & 1) {
            SndCall(6, 0xE, &p->world, 0, 0, em);
        }
        if (MotionMove(em, 0)) {
            em->clearStatus(EM_STATUS_ACTIVE);
            em->setStatus(EM_STATUS_ITEMSET);
            EmRoutineSet(em, 1, 0x16, 0, 0);
        }
        break;
    }
}

// Route / target update: the player route (with a far look-ahead point when the player is far and
// behind), then the current target (dog target, goto point, friend, partner) and its angle / distance.
void em2bRouteCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    Vec v;
    Vec a;
    Vec plPos;
    Vec d;
    f32 dist;

    if (em->hp <= 0) {
        return;
    }
    if (pG->stage_no == 1 && pG->room_no == 0x19) {
        w->Pl_pos = pPL->pos;
    } else {
        if ((em->flag & 0x80000000) && !((dist = SQRTF(em->l_pl)) < 8000.0f)) {
            if (dist > 25000.0f) {
                dist = 25000.0f;
            }
            dist *= 3.99999990e-05f;
            v.x = dist * -15000.0f;
            v.y = 500.0f;
            v.z = 0.0f;
            PSMTXMultVec(pPL->mat, &v, &v);
            plPos = pPL->pos;
            plPos.y += 500.0f;
            if (SatMgr.hitCheck(&plPos, &v, &a, 0, 0, 0)) {
                PSVECSubtract(&plPos, &a, &d);
#line 6032 "D:/Bio4/Prog/em2b.cpp"
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, 350.0f);
                PSVECAdd(&a, &d, &v);
            }
            plPos = v;
        } else {
            plPos = pPL->pos;
        }
        if (pPL->pos.y > em->pos.y + 2000.0f && em->l_pl < 36000000.0f) {
            w->Pl_pos = plPos;
            w->Be_flg |= 1;
        } else if (RouteCkToPos(em, &plPos, &w->Pl_pos, 0, 0)) {
            w->Be_flg |= 1;
        }
    }
    w->Pl_dir = Muku(&em->pos, &w->Pl_pos, em->ang.y, 3.14159274f);
    w->Pl_rot = fabsf(w->Pl_dir);
    if (em->r_no_0 == 0) {
        w->Pl_dir = 0.0f;
        w->Pl_rot = 0.0f;
        em->l_pl = 100000000.0f;
    }
    w->Go_pos = w->Pl_pos;
    w->Go_dir = w->Pl_dir;
    w->Go_rot = w->Pl_rot;
    w->L_go = em->l_pl;
    w->pEm = pPL;
    if (w->pTreeTarget) {
        RouteCkToPos(em, &w->pTreeTarget->pos, &w->Go_pos, 0, 0);
        w->Go_dir = Muku(&em->pos, &w->Go_pos, em->ang.y, 3.14159274f);
        w->Go_rot = fabsf(w->Go_dir);
        w->L_go = (em->pos.x - w->pTreeTarget->pos.x) * (em->pos.x - w->pTreeTarget->pos.x) +
                        (em->pos.z - w->pTreeTarget->pos.z) * (em->pos.z - w->pTreeTarget->pos.z);
        w->pEm = w->pTreeTarget;
    } else if (w->pGoto) {
        w->Go_pos = w->pGoto->pos;
        w->Go_dir = Muku(&em->pos, &w->Go_pos, em->ang.y, 3.14159274f);
        w->Go_rot = fabsf(w->Go_dir);
        w->L_go = (em->pos.x - w->pGoto->pos.x) * (em->pos.x - w->pGoto->pos.x) +
                        (em->pos.z - w->pGoto->pos.z) * (em->pos.z - w->pGoto->pos.z);
        w->pEm = w->pTreeTarget;
    } else if ((w->Be_flg & 0x80) && w->pFriend) {
        w->Go_pos = w->pFriend->pos;
        w->Go_dir = Muku(&em->pos, &w->Go_pos, em->ang.y, 3.14159274f);
        w->Go_rot = fabsf(w->Go_dir);
        w->L_go = (em->pos.x - w->pFriend->pos.x) * (em->pos.x - w->pFriend->pos.x) +
                        (em->pos.z - w->pFriend->pos.z) * (em->pos.z - w->pFriend->pos.z);
        w->pEm = w->pFriend;
    } else if (pSUB && w->Catch_power == 0) {
        f32 dist = sqrtf((em->pos.x - pSUB->pos.x) * (em->pos.x - pSUB->pos.x) +
                         (em->pos.z - pSUB->pos.z) * (em->pos.z - pSUB->pos.z)) + 3000.0f;

        if (dist * dist < em->l_pl) {
            RouteCkToPos(em, &pSUB->pos, &w->Go_pos, 0, 0);
            w->Go_dir = Muku(&em->pos, &w->Go_pos, em->ang.y, 3.14159274f);
            w->Go_rot = fabsf(w->Go_dir);
            w->L_go = (em->pos.x - pSUB->pos.x) * (em->pos.x - pSUB->pos.x) +
                            (em->pos.z - pSUB->pos.z) * (em->pos.z - pSUB->pos.z);
            w->pEm = *(cPlayer* volatile*) &pSUB; // reloaded: the tail must not cross-jump with the pFriend arm's
        }
    }
}

// Turns the head towards the current target's head (damped) while a routine runs.
void em2bNeckMove(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cModel* t;
    cParts* p;
    Vec v;

    if ((w->Be_flg & 0x80) && w->pFriend) {
        t = w->pFriend->getPartsPtr(0);
    } else if ((w->Be_flg & 4) && pSUB) {
        t = pSUB->getPartsPtr(4);
    } else {
        t = pPL->getPartsPtr(4);
    }
    p = (cParts*) em->getPartsPtr(4);
    v.x = 0.0f;
    v.y = 250.0f;
    v.z = 0.0f;
    PSMTXMultVec(t->mat, &v, &v);
    if (w->Be_flg & 0x10) {
        w->Neck_dir_y = w->Neck_dir_y * 0.899999976f + Muku(&em->pos, &t->world, em->ang.y, 1.04719758f) * 0.100000001f;
    } else {
        w->Neck_dir_y = w->Neck_dir_y * 0.899999976f;
    }
    p = (cParts*) em->getPartsPtr(3);
    p->motParts.flags |= 0x40000000;
    p->inv_offset.x = 0.0f;
    p->inv_offset.y = w->Neck_dir_y;
    p->inv_offset.z = 0.0f;
}

// Two-motion blend: m0 on the main motion work, m1 (Blend < 0) or m2 on the blend work with the
// weight |Blend|; Hokan / Frame give the hokan frames and start frame (the stamp aim).
void em2bBlendMotSet(cEm2b* em, void* m0, void* m1, void* m2, int a, int b, int c, int d)
{
    Em2bWork* w = EM2B_WK(em);
    MotionWork* bm;
    f32 val = fabsf(w->Blend);
    void* m;
    int arg;

    MotionSetCore(em, &em->Motion, m0, (void*) a, (u8) w->Hokan, (u16) d, (u16) w->Frame);
    if (w->Blend < 0.0f) {
        m = m1;
        arg = b;
    } else {
        m = m2;
        arg = c;
    }
    bm = EM2B_BLEND_MOT(w);
    MotionSetCore(em, bm, m, (void*) arg, (u8) w->Hokan, (u16) d, (u16) w->Frame);
    em->Motion.blend = bm;
    bm->Brate = val * 0.00390625f;
    if (w->Hokan) {
        w->Hokan--;
    }
    w->Frame++;
    if ((u32) w->Frame >= em->Motion.Seq_frame_num) {
        w->Frame = 0;
    }
}

// Chain cloth of the type 1 giant (the chain on the arm).
void em2bClothSet(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    if (em->type == 1) {
        int zero;
        int num;
        const u8* parts;
        const u8* side;
        const u8* up;
        const u8* down;
        const f32* max;
        CLOTH_AT_SET* at;
        const f32* rate;
        int two;
        f32 g20;
        f32 g08;
        int four;
        f32 g005;
        f32 zf;
        int flags;
        // Store order = the original's LUID order: its constants are reload-materialised and no store
        // carries a death (COMPILER-DIFF: #13); ours must keep every constant alive past the
        // block (the two "=m" keep-alives name fields the block does not store), else the dying stores
        // are issued first. The 0.0 is expanded after 0.8 for the pool order.
        zero = 0;
        w->Cloth.pPtbl = (cModel**) zero;
        num = 10;
        w->Cloth.Num = num;
        parts = em2b_cloth_parts;
        w->Cloth.pCloth = parts;
        side = em2b_cloth_side;
        w->Cloth.pLeft = side;
        up = em2b_cloth_up;
        w->Cloth.pParent = up;
        down = em2b_cloth_down;
        w->Cloth.pChild = down;
        max = em2b_cloth_max;
        w->Cloth.pMax = max;
        at = em2b_cloth_at;
        w->Cloth.pAtset = at;
        rate = em2b_cloth_rate;
        w->Cloth.pRate = rate;
        two = 2;
        w->Cloth.At_num = two;
        g20 = 20.0f;
        w->Cloth.Gravity = g20;
        g08 = 0.800000012f;
        w->Cloth.Rate = g08;
        zf = 0.0f;
        four = 4;
        w->Cloth.Bundle_num = four;
        g005 = 0.0500000007f;
        w->Cloth.Stretchy = g005;
        w->Cloth.Move_rate = zf;
        flags = 0x100;
        w->Cloth.Flag = flags;
        w->Cloth.pRight = (const u8*) zero;
        w->Cloth.pUpLeft = (const u8*) zero;
        w->Cloth.pUpRight = (const u8*) zero;
        w->Cloth.pWindSin = (const f32*) zero;
        w->Cloth.pWindRate = (const f32*) zero;
        w->Cloth.pGravity = (const f32*) zero;
        w->Cloth.pEm_at = em;
        w->Cloth.WindSin = zf;
        asm("" : "=m"(w->Dog_wait) : "r"(zero), "r"(num), "r"(parts), "r"(side), "r"(up), "r"(down), "r"(max), "r"(at)); // COMPILER-DIFF: #13
        asm("" : "=m"(w->Event_wait) : "r"(rate), "r"(two), "f"(g20), "f"(g08), "r"(four), "f"(g005), "f"(zf), "r"(flags)); // COMPILER-DIFF: #13
        PenClothSet(em, &w->Cloth, 100.0f);
    }
}

// Per frame: the chained variant's (type 1) cloth simulation (PenClothMove3).
void em2bClothMove(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    if (em->type == 1) {
        PenClothMove3(em, &w->Cloth);
    }
}

// Attack hit check of attack `no` between the two positions: the player gets the matching damage
// callback, a hit shakes the camera and the pad.
int em2bAtkCk(cEm2b* em, Vec* a, Vec* b, int no)
{
    Em2bWork* w = EM2B_WK(em);
    EmAtkInfo* atk;
    int hit;

    em->flag |= 4;
    if (w->Atk_ck == 0) {
        atk = &em2b_atk_info[no];
        if ((s16) pG->pl_life > 1) {
            atk->flag |= 4;
        } else {
            atk->flag &= ~4;
        }
        hit = EmAtkHitCk(atk, a, b, 0);
        if (hit) {
            if (hit & 1) {
                w->Atk_ck = 1;
                switch ((u32) no) {
                case 0:
                case 1:
                    pPL->pos.x = a->x;
                    pPL->pos.z = a->z;
                    SetPlDamage(em, plem2b_dm_Stamp);
                    break;
                case 2:
                case 3:
                    pPL->ang.y = GetXZAngle(a, b);
                    SetPlDamage(em, plem2bDmBlow);
                    break;
                case 5:
                    pPL->ang.y = GetXZAngle(&pPL->pos, b);
                    SetPlDamage(em, plem2bDmBlow);
                    break;
                case 4:
                    pPL->ang.y = em->ang.y + 3.14159274f;
                    pPL->ang.y = LIMIT_ANGLE(pPL->ang.y);
                    SetPlDamage(em, plem2b_dm_BlowKick);
                    break;
                }
                QuakeExec(0, 0, 5, 22.0f, 2);
                SndCall(8, 0x32, &em->pos, em->id, 0, em);
                VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
                return 1;
            }
            if (hit & 2) {
                w->Atk_ck = 1;
                QuakeExec(0, 0, 5, 22.0f, 2);
                SndCall(8, 0x32, &em->pos, em->id, 0, em);
                VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
                return 1;
            }
        }
    }
    return 0;
}

// Stamped flat.
static void plem2b_dm_Stamp(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xB8), 0, 3, 1, 0);
        PlSetFace(1);
        pl->atari.m_flag &= ~0x200;
        PlSetDamageSe(0);
        pl->r_no_2++;
    case 1:
        em2bStampCamMove((cEm2b*)pl->pEmCatch);
        if (MotionMove(pl, 0) || (pl->Motion.Seq_frame > 49.7000008f && pl->Motion.Seq_frame < 50.2999992f)) {
            if ((s16) pG->pl_life > 0) {
                pl->atari.throughOff();
                EmRoutineSet(pPL, 1, 0, 0xA, 0);
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Ashley's damage routine when stamped on (em2bPressSubCk): the crushed motion 0xD5 from the giant's
// archive, held (she is dead).
static void subem2b_dm_Stamp(cSubChar* sub)
{
    cSubChar* s = SUB_CHAR();

    s->subArc = s->pEmCatch->subArc;
    StaFlagOn(pG, STA_SUB_CATCHED);
    switch (s->r_no_2) {
    case 0:
        MotionSetCore(s, &s->Motion, PL_ARC_PTR(s->subArc, 0xD5), 0, 0, 1, 0);
        s->r_no_2++;
    case 1:
        MotionMove(s, 0);
        break;
    }
    s->subArc = s->subArc2;
}

// Kicked away (the kick attack).
static void plem2b_dm_BlowKick(cPlayer* pl)
{
    int rtn = pl->r_no_2;

    pl->subArc = pl->pEmCatch->subArc;
    switch (rtn) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC_PTR(pG->pPlayer, 0x51), 0, 3, 1, 0);
        PlSetFace(1);
        PlSetDamageSe(0);
        pl->dmg.m_Timer = 0xA;
        EstSet(pl, -1, 0, 0, EFF_PL00, ChkWaterEffectEnable(&pl->pos) ? 6 : 5, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->r_no_2++;
    case 1:
        if (pl->Motion.Seq_frame > 16.7000008f && pl->Motion.Seq_frame < 17.2999992f) {
            SndCall(5, 5, &pPL->pos, pPL->id, 0, pPL);
        }
        if (MotionMove(pl, 0) && (s16) pG->pl_life > 0) {
            pl->atari.throughOff();
            EmRoutineSet(pPL, 1, 0, 0xA, 0);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Action button under the falling giant: dash out.
static void em2bDashEscapeAction(cEm2b* em)
{
    SetPlDamage(em, plem2bDashEscape);
    GameAddPoint(LVADD_CRITICALHIT);
}

// Dash out from under the falling giant.
static void plem2bDashEscape(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 2;
    if (pSUB) {
        pSUB->dmg.m_Timer = 2;
    }
    switch (pl->r_no_2) {
    case 0:
        if (Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, 3.14159274f) < 0.0f) {
            MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC1), EM_ARC(pl, 0xC2), 3, 1, 0);
        } else {
            MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC1), EM_ARC(pl, 0xC2), 3, 0x41, 0);
        }
        AtariOffV(&pl->atari, 0xFDFF);
        if (pSUB) {
            AtariOffV(&pSUB->atari, 0xFDFF);
        }
        GameAddPoint(LVADD_ESCAPEATTACK);
        if (pSUB) {
            pSUB->be_flag &= ~2;
        }
        SndCall(1, 0x48, &pl->pos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->world, 0, 0, pl);
        pl->m_Work0 = 50;
        pl->m_Work1 = 15;
        pl->r_no_2++;
    case 1:
        em2bEscapeCamMove((cEm2b*)pl->pEmCatch);
        if (pl->m_Work1) {
            pl->ang.y += Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, 0.196349546f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        MotionMove(pl, 0);
        if (pl->Motion.Seq_frame > 11.6999998f && pl->Motion.Seq_frame < 12.3000002f) {
            EstSet(0, -1, &pl->pos, 0, EFF_PL00, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            AtariOnV(&pl->atari, 0x200);
            if (pSUB) {
                AtariOnV(&pSUB->atari, 0x200);
            }
            EndPlDamage();
            if (pSUB) {
                pSUB->be_flag |= 2;
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Event camera of the escape scenes: behind the player, pulled in to the scenario hit.
void em2bEscapeCamMove(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    GlobalWork* g = pG;
    Vec a;
    Vec b;
    Vec c;

    w->Cam.param.fovy = g->Camera.param.fovy;
    a.x = -376.0f;
    a.y = 575.0f;
    a.z = -1831.0f;
    b.x = -244.0f;
    b.y = 809.0f;
    b.z = 52.5999985f;
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    PosToPos(&g->Camera.param.at, &b, &w->Cam.param.at, 1.0f);
    PosToPos(&g->Camera.param.pos, &a, &w->Cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->Cam.param.at, &w->Cam.param.pos, &c, 0, 0x8000, 0)) {
        Vec d;
        f32 len;

        PSVECSubtract(&c, &w->Cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 6834 "D:/Bio4/Prog/em2b.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->Cam.param.at, &d, &w->Cam.param.pos);
    }
    w->Cam.Up.x = 0.0f;
    w->Cam.Up.y = 1.0f;
    w->Cam.Up.z = 0.0f;
    w->Cam.Distance = VEC_DIST(&w->Cam.param.pos, &w->Cam.param.at);
    CameraSetOrientationUp(&w->Cam);
    CamCtrl.m_pExtraCamera = (s32) &w->Cam;
}

// Foot landing of the walk: quake, step SE, dust; the chain giant rattles.
void em2bFootSe(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    u32 no;
    cModel* p = 0;
    Vec* pos;

    if (em->Motion.Seq_old.Se == 0) {
        return;
    }
    no = em->Motion.Seq_old.Se - 1;
    if (no > 1) {
        return;
    }
    em->Motion.Seq_old.Se = 0;
    if (em->Motion.Mot_attr & 0x40) {
        switch (no) {
        case 0:
            no = 1;
            break;
        case 1:
            no = 0;
            break;
        }
    }
    switch (no) {
    case 0:
        p = em->getPartsPtr(0x15);
        break;
    case 1:
        p = em->getPartsPtr(0x19);
        break;
    }
    pos = &p->world;
    em2bQuakeSet(pos);
    SndCall(8, no, pos, em->id, 0, em);
    EstSet(0, -1, pos, 0, w->Eff, 4, 0, ESP_CORE_KIND_NONE, 0, 0);
    if (w->Be_flg & 0x100) {
        EstSet(em, -1, 0, 0, w->Eff, 0xF, 0, ESP_CORE_KIND_NONE, em, 0);
    }
    if (em->type == 3 && w->pChain) {
        SndCall(6, 0x11, &em->getPartsPtr(0)->world, em->id, 0, em);
    }
}

// Motion key bit7: swap the model variant (the foot / hand side tables).
void em2bFtChgCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    if (em->Motion.Seq_old.Free & 0x80) {
        switch (w->Ft_axis) {
        case 0:
            w->Ft_axis = 1;
            break;
        case 1:
            w->Ft_axis = 0;
            break;
        }
    }
}

// Camera quake scaled by the distance of the position from the camera.
void em2bQuakeSet(Vec* pos)
{
    Camera* cam = &pG->Camera;
    f32 d = (pos->x - cam->param.pos.x) * (pos->x - cam->param.pos.x) + (pos->y - cam->param.pos.y) * (pos->y - cam->param.pos.y) +
            (pos->z - cam->param.pos.z) * (pos->z - cam->param.pos.z);

    if (d < 400000000.0f) {
        f32 power = 10.0f;

        if (d > 25000000.0f) {
            power = 8.0f;
        }
        if (d > 100000000.0f) {
            power = 6.0f;
        }
        if (d > 225000000.0f) {
            power = 4.0f;
        }
        QuakeExec(0, 0, 5, power, 2);
    }
}

// Type 0: the short rope hanging between the neck parts.
void em2bShortRopeSet(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cObjChain* chain;
    Vec pos;
    Vec b;
    Vec rot;
    // COMPILER-DIFF: 13 -- the original issues the 25-store block in pure source order (no store carries a
    // register death); ours needs every constant kept alive past its store.  The three keep-alives are
    // placed where their sched2 slots are free: C reads x50 (ready after the first store, only r0/f31
    // inputs so it ranks below `mr r3,r5`/`addi r4`), A writes a u16 view of x58 (output dependence:
    // ready after the second store; a different mode keeps flow from deleting the real store), B a u32
    // view of x48 (ready after the third).  Operand duplicates set the local-alloc ranks (refs):
    // parts x3 > up/down/at x2 > k100 x2 > five > chain.
    const u8* parts;
    const u8* up;
    const u8* down;
    CLOTH_AT_SET* at;
    int five;
    f32 g20;
    f32 g08;
    int k100;
    f32 g01;
    int zero;
    f32 zf;

    zf = 0.0f;
    pos.x = zf;
    pos.y = zf;
    pos.z = zf;
    rot.x = zf;
    rot.y = zf;
    rot.z = zf;
    chain = SetChain(ARC(0x11), ARC(0x12), &pos, &rot);
    w->rope[1].Move_rate = zf;
    w->rope[1].pEm_at = em;
    w->rope[1].WindSin = zf;
    parts = em2b_rope_parts;
    w->rope[1].pCloth = parts;
    up = em2b_rope_up;
    w->rope[1].pParent = up;
    down = em2b_rope_down;
    w->rope[1].pChild = down;
    at = em2b_rope_at;
    w->rope[1].pAtset = at;
    five = 5;
    w->rope[1].At_num = five;
    g20 = 20.0f;
    w->rope[1].Gravity = g20;
    g08 = 0.800000012f;
    w->rope[1].Rate = g08;
    k100 = 100;
    w->rope[1].Bundle_num = k100;
    g01 = 0.100000001f;
    w->rope[1].Stretchy = g01;
    w->pChain = (cObj*) chain;
    w->rope[1].Num = five;
    zero = 0;
    w->rope[1].pPtbl = (cModel**) zero;
    w->rope[1].pLeft = (const u8*) zero;
    w->rope[1].pRight = (const u8*) zero;
    w->rope[1].pUpLeft = (const u8*) zero;
    w->rope[1].pUpRight = (const u8*) zero;
    w->rope[1].pMax = (const f32*) zero;
    w->rope[1].pWindSin = (const f32*) zero;
    w->rope[1].pWindRate = (const f32*) zero;
    w->rope[1].pGravity = (const f32*) zero;
    w->rope[1].pRate = (const f32*) zero;
    w->rope[1].Flag = zero;
    asm("" : "=m"(w->Dog_wait) : "r"(zero), "f"(zf), "m"(w->rope[1].Move_rate));                                              // COMPILER-DIFF: 13
    asm("" : "=m"(*(u16*) &w->rope[1].pEm_at) : "r"(chain), "r"(parts), "r"(parts), "r"(parts), "r"(up), "r"(up), "r"(down), "r"(down)); // COMPILER-DIFF: 13
    asm("" : "=m"(*(u32*) &w->rope[1].WindSin) : "r"(at), "r"(at), "r"(k100), "r"(k100), "r"(five), "f"(g20), "f"(g08), "f"(g01)); // COMPILER-DIFF: 13
    chain->setChain(&w->rope[1]);
    pos.x = -290.0f;
    pos.y = -162.949997f;
    pos.z = 655.0f;
    b.x = -290.0f;
    b.y = -412.320007f;
    b.z = 39.1500015f;
    ((cObjChain*) w->pChain)->setParent2(em, 3, &pos, 4, &b, 0);
}

// Type 3: the three chains on the arms.
void em2bChainSet(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    PenCloth* c = &w->rope[1];
    cObjChain* chain;
    Vec pos;
    Vec rot;

    w->rope[0].Num = 8;
    w->rope[0].pCloth = em2b_chain_parts;
    w->rope[0].pLeft = 0;
    w->rope[0].pRight = 0;
    w->rope[0].pUpLeft = 0;
    w->rope[0].pUpRight = 0;
    w->rope[0].pParent = em2b_chain_up;
    w->rope[0].pChild = em2b_chain_down;
    w->rope[0].pMax = 0;
    w->rope[0].pWindSin = 0;
    w->rope[0].pWindRate = 0;
    w->rope[0].pAtset = em2b_chain_at;
    w->rope[0].pGravity = 0;
    w->rope[0].pRate = 0;
    w->rope[0].At_num = 5;
    w->rope[0].pEm_at = em;
    w->rope[0].Gravity = 30.0f;
    w->rope[0].Rate = 0.800000012f;
    w->rope[0].Bundle_num = 0;
    w->rope[0].WindSin = 0.0f;
    w->rope[0].Move_rate = 0.0f;
    w->rope[0].Stretchy = 1.0f;
    w->rope[0].Flag = 0;
    w->rope[0].pPtbl = 0;
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    chain = SetChain(ARC(0x13), ARC(0x14), &pos, &rot);
    w->pChain = (cObj*) chain;
    chain->setChain(&w->rope[0]);
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    ((cObjChain*) w->pChain)->setParent(em, 0x3B, &pos, 0);

    w->rope[1].Num = 8;
    w->rope[1].pCloth = em2b_chain_parts;
    w->rope[1].pLeft = 0;
    w->rope[1].pRight = 0;
    w->rope[1].pUpLeft = 0;
    w->rope[1].pUpRight = 0;
    w->rope[1].pParent = em2b_chain_up;
    w->rope[1].pChild = em2b_chain_down;
    w->rope[1].pMax = 0;
    w->rope[1].pWindSin = 0;
    w->rope[1].pWindRate = 0;
    w->rope[1].pAtset = 0;
    w->rope[1].pGravity = 0;
    w->rope[1].pRate = 0;
    w->rope[1].At_num = 0;
    w->rope[1].pEm_at = em;
    w->rope[1].Gravity = 30.0f;
    w->rope[1].Rate = 0.800000012f;
    w->rope[1].Bundle_num = 0;
    w->rope[1].WindSin = 0.0f;
    w->rope[1].Move_rate = 0.0f;
    w->rope[1].Stretchy = 1.0f;
    w->rope[1].Flag = 0;
    w->rope[1].pPtbl = 0;
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    chain = SetChain(ARC(0x13), ARC(0x14), &pos, &rot);
    w->pChain2 = (cObj*) chain;
    chain->setChain(c);
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    ((cObjChain*) w->pChain2)->setParent(em, 0x40, &pos, 0);

    w->rope[1].Num = 8;
    w->rope[1].pCloth = em2b_chain_parts;
    w->rope[1].pLeft = 0;
    w->rope[1].pRight = 0;
    w->rope[1].pUpLeft = 0;
    w->rope[1].pUpRight = 0;
    w->rope[1].pParent = em2b_chain_up;
    w->rope[1].pChild = em2b_chain_down;
    w->rope[1].pMax = 0;
    w->rope[1].pWindSin = 0;
    w->rope[1].pWindRate = 0;
    w->rope[1].pAtset = em2b_chain_at2;
    w->rope[1].pGravity = 0;
    w->rope[1].pRate = 0;
    w->rope[1].At_num = 5;
    w->rope[1].pEm_at = em;
    w->rope[1].Gravity = 30.0f;
    w->rope[1].Rate = 0.800000012f;
    w->rope[1].Bundle_num = 0;
    w->rope[1].WindSin = 0.0f;
    w->rope[1].Move_rate = 0.0f;
    w->rope[1].Stretchy = 1.0f;
    w->rope[1].Flag = 0;
    w->rope[1].pPtbl = 0;
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    chain = SetChain(ARC(0x13), ARC(0x14), &pos, &rot);
    w->pChain3 = (cObj*) chain;
    chain->setChain(c);
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    ((cObjChain*) w->pChain3)->setParent(em, 0x41, &pos, 0);
}

// Room 119: is the player inside an intact house near enough to break?
int em2bPlInHouseCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    EmiData* tbl;
    int i;

    if (pG->stage_no != 1 || pG->room_no != 0x19) {
        return 0;
    }
    tbl = pG->pEmi;
    if (tbl == 0) {
        return 0;
    }
    if (w->Be_flg & 0x80) {
        return 0;
    }
    for (i = 0; i < tbl->n; i++) {
        EmiEntry* h = &tbl->entry[i];

        if (h->type != 3) {
            continue;
        }
        if (h->state == 3) {
            continue;
        }
        if ((pPL->pos.x - h->pos.x) * (pPL->pos.x - h->pos.x) + (pPL->pos.z - h->pos.z) * (pPL->pos.z - h->pos.z) > 2250000.0f) {
            continue;
        }
        w->pHouse = h;
        return 1;
    }
    return 0;
}

// Looks for a tree to tear out in front of the giant; it becomes the target.
int em2bSearchTree(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    u32 i;

    if (w->pTree || w->pTreeTarget || w->pRock || w->pGoto || w->pHouse || w->Rock_wait || (w->Be_flg & 0x80)) {
        return 0;
    }
    if (w->Be_flg & 4) {
        return 0;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEmTree* e = (cEmTree*) EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x49) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (!e->ckCatch()) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            36000000.0f) {
            continue;
        }
        if (fabsf(Muku(&em->pos, &e->pos, em->ang.y, 3.14159274f)) > 0.785398185f) {
            continue;
        }
        w->pTreeTarget = e;
        RouteCkToPos(em, &e->pos, &w->Go_pos, 0, 0);
        w->Go_dir = Muku(&em->pos, &w->Go_pos, em->ang.y, 3.14159274f);
        w->Go_rot = fabsf(w->Go_dir);
        w->L_go = (em->pos.x - w->pTreeTarget->pos.x) * (em->pos.x - w->pTreeTarget->pos.x) +
                        (em->pos.z - w->pTreeTarget->pos.z) * (em->pos.z - w->pTreeTarget->pos.z);
        return 1;
    }
    return 0;
}

// Near enough to the target tree and facing it: go and get it.
int em2bGetTreeCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cEm* t = w->pTreeTarget;

    if (t == 0) {
        return 0;
    }
    if ((em->pos.x - t->pos.x) * (em->pos.x - t->pos.x) + (em->pos.y - t->pos.y) * (em->pos.y - t->pos.y) +
            (em->pos.z - t->pos.z) * (em->pos.z - t->pos.z) >
        12250000.0f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &t->pos, em->ang.y, 3.14159274f)) > 0.785398185f) {
        return 0;
    }
    w->Atk_wait = Rnd() % 1800 + 1800;
    EmRoutineSet(em, 1, 0xD, 0, 0);
    return 1;
}

// Looks for a rock spot (EMI kind 4) in front of the giant; it becomes the goto target.
int em2bSearchRockCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int i;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (w->pTree || w->pTreeTarget || w->pRock || w->pGoto || w->pHouse || w->Atk_wait || (w->Be_flg & 0x80)) {
        return 0;
    }
    if (w->Be_flg & 4) {
        return 0;
    }
    for (i = 0; i < pG->pEmi->n; i++) {
        EmiEntry* e = &pG->pEmi->entry[i];

        if (e->type != 4) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            144000000.0f) {
            continue;
        }
        if (fabsf(Muku(&em->pos, &e->pos, em->ang.y, 3.14159274f)) > 0.785398185f) {
            continue;
        }
        w->pGoto = e;
        w->Go_pos = e->pos;
        w->Go_dir = Muku(&em->pos, &w->Go_pos, em->ang.y, 3.14159274f);
        w->Go_rot = fabsf(w->Go_dir);
        w->L_go = (em->pos.x - w->pGoto->pos.x) * (em->pos.x - w->pGoto->pos.x) +
                        (em->pos.z - w->pGoto->pos.z) * (em->pos.z - w->pGoto->pos.z);
        return 1;
    }
    return 0;
}

// Near enough to the rock spot and facing it: tear out the rock.
int em2bGetRockCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    EmiEntry* e = w->pGoto;

    if (e == 0) {
        return 0;
    }
    if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
            (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
        20250000.0f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &e->pos, em->ang.y, 3.14159274f)) > 0.785398185f) {
        return 0;
    }
    w->Atk_wait = Rnd() % 1800 + 1800;
    EmRoutineSet(em, 1, 0xF, 0, 0);
    return 1;
}

// Tree swing hit check: the player within the swept sector in front of the tree gets blown away.
int em2bTreeAtkCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cEmTree* tree = w->pTree;
    cModel* p;
    Vec a;
    Vec b;
    f32 ang;
    f32 aang;

    if (tree == 0) {
        return 0;
    }
    if (w->Atk_ck) {
        return 0;
    }
    if (EmDeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    p = tree->getPartsPtr(0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 15000.0f;
    b.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    PSMTXMultVec(p->mat, &b, &b);
    ang = GetXZAngle(&a, &b);
    ang = Muku(&a, &pPL->pos, ang, 3.14159274f);
    aang = fabsf(ang);
    if ((a.x - pPL->pos.x) * (a.x - pPL->pos.x) + (a.y - pPL->pos.y) * (a.y - pPL->pos.y) + (a.z - pPL->pos.z) * (a.z - pPL->pos.z) >
        225000000.0f) {
        return 0;
    }
    if (ang > 0.261799395f) {
        return 0;
    }
    if (aang > 0.523598790f) {
        return 0;
    }
    w->Atk_ck = 1;
    if ((s16) pG->pl_life > 1) {
        LifeDownSet2(pPL, 800, 0, 1);
    } else {
        LifeDownSet(pPL, 800, 0);
    }
    em->ang.y = ang + 1.57079637f;
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    SndCall(8, 0xF, &pPL->pos, em->id, 0, pPL);
    SndCall(8, 0x32, &pPL->pos, em->id, 0, pPL);
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
    SetPlDamage(em, plem2bDmBlow);
    return 1;
}

// Tree swing against the scenery: the houses within the tree's reach in front of it break.
int em2bTreeAtkScrCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    cEmTree* tree = w->pTree;
    Vec a;
    Vec b;
    f32 d;
    f32 ang;
    int hit;
    int i;

    if (tree == 0) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 8000.0f;
    b.z = 0.0f;
    PSMTXMultVec(tree->mat, &a, &a);
    PSMTXMultVec(tree->mat, &b, &b);
    d = (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z);
    if (d < 10000.0f) {
        return 0;
    }
    ang = GetXZAngle(&a, &b);
    hit = 0;
    if (pG->pEmi == 0) {
        return 0;
    }
    for (i = 0; i < pG->pEmi->n; i++) {
        EmiEntry* h = &pG->pEmi->entry[i];

        if (h->type != 3) {
            continue;
        }
        if (h->state == 3) {
            continue;
        }
        if (d < (h->pos.x - a.x) * (h->pos.x - a.x) + (h->pos.z - a.z) * (h->pos.z - a.z)) {
            continue;
        }
        if (fabsf(Muku(&a, &h->pos, ang, 3.14159274f)) > 0.392699093f) {
            continue;
        }
        if (pG->stage_no == 1 && pG->room_no == 0x19) {
            em2bHouseFlagSet(h);
        }
        h->state = 3;
        hit = 1;
    }
    if (hit) {
        return 1;
    }
    return 0;
}

// Dash against the scenery: the houses and the type 3 rocks within `rad` of the position break.
void em2bDashScrCk(cEm2b* em, Vec* pos, f32 rad)
{
    int i;
    f32 d; // one variable for both loops' distances (the fmadds result is tied to it: f13 in both)

    if (pG->pEmi == 0) {
        return;
    }
    // em2bHouseBrkCk written out: one `i` for both loops (callee-saved r30 in the house loop too).
    for (i = 0; i < pG->pEmi->n; i++) {
        EmiEntry* h = &pG->pEmi->entry[i];

        if (h->type != 3) {
            continue;
        }
        if (h->state == 3) {
            continue;
        }
        d = (h->pos.x - pos->x) * (h->pos.x - pos->x) + (h->pos.z - pos->z) * (h->pos.z - pos->z);
        if (d > (rad + 3000.0f) * (rad + 3000.0f)) {
            continue;
        }
        if (pG->stage_no == 1 && pG->room_no == 0x19) {
            em2bHouseFlagSet(h);
            h->state = 3;
        }
        if (pG->stage_no == 1 && pG->room_no == 0x1E && h->state == 0) {
            switch (h->sub) {
            case 0:
                RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_C);
                h->state = 3;
                break;
            case 1:
                RmfFlagOn(pG, RMF_R119_DESTROY_YANE_A);
                h->state = 3;
                break;
            case 2:
                RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_A);
                h->state = 3;
                break;
            case 3:
                RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_B);
                h->state = 3;
                break;
            }
        }
    }
    for (i = 0; i < (int) EmMgr.getArrayNum(); i++) {
        cEmRock* e = (cEmRock*) EmMgr.fastAt(i);
        cModel* p;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x4A) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((cEm*) e == em) {
            continue;
        }
        if (e->type != 3) {
            continue;
        }
        p = e->getPartsPtr(0);
        {
            EmRockWork* rw = EMROCK_WK(e);
            f32 r = rw->Radius + rad;

            d = (p->world.x - pos->x) * (p->world.x - pos->x) + (p->world.y - pos->y) * (p->world.y - pos->y) +
                (p->world.z - pos->z) * (p->world.z - pos->z);
            if (d < r * r) {
                // Dead test (flow deletes the store, jump2 the compare): a second non-zero-offset use of
                // `rw` keeps combine from folding the pointer into the radius load (`addi r9,e,992;
                // lfs 16(r9)` like the target); placed before the call so rw is not callee-saved.
                if (rw->Timer == 0) {
                    d = 0.0f;
                }
                e->setBreakR11E();
            }
        }
    }
}

// Room 11E: a house in the way of the walk (ahead, towards the target, the giant stuck) breaks.
void em2bR11eScrBrkCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    int i;

    if (pG->pEmi == 0) {
        return;
    }
    if (pG->stage_no != 1 || pG->room_no != 0x1E) {
        return;
    }
    for (i = 0; i < pG->pEmi->n; i++) {
        EmiEntry* h = &pG->pEmi->entry[i];

        if (h->type != 3) {
            continue;
        }
        if (h->state == 3) {
            continue;
        }
        if ((h->pos.x - em->pos.x) * (h->pos.x - em->pos.x) + (h->pos.z - em->pos.z) * (h->pos.z - em->pos.z) > 30250000.0f) {
            continue;
        }
        if (fabsf(Muku2(GetXZAngle(&em->pos, &h->pos), GetXZAngle(&em->pos, &w->Go_pos), 3.14159274f)) > 0.785398185f) {
            continue;
        }
        if (w->L_go < 64000000.0f) {
            continue;
        }
        if (h->state != 0) {
            continue;
        }
        switch (h->sub) {
        case 0:
            if (w->HoseiCnt > 2) {
                RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_C);
                h->state = 3;
            }
            break;
        case 1:
            if (w->HoseiCnt > 2) {
                RmfFlagOn(pG, RMF_R119_DESTROY_YANE_A);
                h->state = 3;
            }
            break;
        case 2:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_A);
            h->state = 3;
            break;
        case 3:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_B);
            h->state = 3;
            break;
        }
    }
}

// Room 11E: the houses 2 / 3 within `rad` of the position break (hand landings).
void em2bR11eScrBrkCk2(cEm2b* em, Vec* pos, f32 rad)
{
    int i;

    if (pG->pEmi == 0) {
        return;
    }
    if (pG->stage_no != 1 || pG->room_no != 0x1E) {
        return;
    }
    for (i = 0; i < pG->pEmi->n; i++) {
        EmiEntry* h = &pG->pEmi->entry[i];

        if (h->type != 3) {
            continue;
        }
        if (h->state == 3) {
            continue;
        }
        if ((h->pos.x - pos->x) * (h->pos.x - pos->x) + (h->pos.z - pos->z) * (h->pos.z - pos->z) > rad * rad) {
            continue;
        }
        if (h->state != 0) {
            continue;
        }
        switch (h->sub) {
        case 1:
            break;
        case 2:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_A);
            h->state = 3;
            break;
        case 3:
            RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_B);
            h->state = 3;
            break;
        }
    }
}

// The blown-away player breaks the houses he flies into.
void em2bPlBlowAtkScrCk(cPlayer* pl)
{
    int i;

    if (pG->pEmi == 0) {
        return;
    }
    for (i = 0; i < pG->pEmi->n; i++) {
        EmiEntry* h = &pG->pEmi->entry[i];

        if (h->type != 3) {
            continue;
        }
        if (h->state == 3) {
            continue;
        }
        if ((h->pos.x - pl->pos.x) * (h->pos.x - pl->pos.x) + (h->pos.z - pl->pos.z) * (h->pos.z - pl->pos.z) > 9000000.0f) {
            continue;
        }
        if (pG->stage_no == 1 && pG->room_no == 0x19) {
            em2bHouseFlagSet(h);
            h->state = 3;
        }
        if (pG->stage_no == 1 && pG->room_no == 0x1E && h->state == 0) {
            switch (h->sub) {
            case 0:
                RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_C);
                h->state = 3;
                break;
            case 1:
                RmfFlagOn(pG, RMF_R119_DESTROY_YANE_A);
                h->state = 3;
                break;
            case 2:
                RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_A);
                h->state = 3;
                break;
            case 3:
                RmfFlagOn(pG, RMF_R119_DESTROY_KOYA_B);
                h->state = 3;
                break;
            }
        }
    }
}

// Action button of the tree swing: the player ducks under the tree.
static void em2bEscapeAction(cEm2b* em)
{
    SetPlDamage(em, plem2bEscapeTree);
    GameAddPoint(LVADD_CRITICALHIT);
}

// Ducks under the swung tree.
static void plem2bEscapeTree(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 30);
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, pl->m_MotTbl[0], pl->m_MotTbl[1], 5, 5, 0);
        pl->m_Work0 = 15;
        pl->r_no_2++;
        break;
    case 1:
        if (!MotionMove(pl, 0) && !(pl->pEmCatch->flag & 4) && pl->m_Work0 != 0) {
            pl->m_Work0--;
        } else {
            pl->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xBF), 0, 3, 1, 0);
        GameAddPoint(LVADD_ESCAPEATTACK);
        pl->r_no_2++;
    case 3:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Blown away by a swing / the tree: flies until a wall stops him, lands, gets up.
static void plem2bDmBlow(cPlayer* pl)
{
    Em2bWork* w = EM2B_WK(pl->pEmCatch);

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 30);
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xB4), 0, 5, 1, 0);
        PlSetFace(1);
        pl->m_Work0 = 1;
        pl->r_no_2++;
    case 1:
        em2bBlowCamMove((cEm2b*)pl->pEmCatch, 0.300000012f);
        if (MotionMove(pl, 0)) {
            pl->r_no_2++;
        } else {
            em2bPlBlowAtkScrCk(pl);
            if (pl->m_Work0) {
                pl->m_Work0--;
            } else if (pl->Wall_norm.x != 0.0f || pl->Wall_norm.y != 0.0f || pl->Wall_norm.z != 0.0f) {
                pl->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xB5), 0, 5, 1, 0);
        EstSet(pl, -1, 0, 0, w->Eff, 6, 0, ESP_CORE_KIND_NONE, pl, 0);
        SndCall(8, 0x1A, &pl->pos, pl->pEmCatch->id, 0, pl);
        SndCall(1, 9, &pl->pos, 0, 0, pl);
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
        pl->r_no_2++;
    case 3:
        em2bBlowCamMove((cEm2b*)pl->pEmCatch, 0.300000012f);
        if (MotionMove(pl, 0) && (s16) pG->pl_life > 0) {
            pl->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xB6), EM_ARC(pl, 0xB7), 5, 1, 0);
        SndCall(1, 0x29, &pl->getPartsPtr(0)->world, 0, 0, pPL);
        SndCall(1, 4, &pl->getPartsPtr(0)->world, 0, 0, pPL);
        pl->r_no_2++;
    case 5:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Camera of the blow: the game camera position, the target pulled towards the player.
void em2bBlowCamMove(cEm2b* em, f32 rate)
{
    Em2bWork* w = EM2B_WK(em);
    GlobalWork* g = pG;

    cModel* p;

    w->Cam.param.fovy = g->Camera.param.fovy;
    w->Cam.param.pos = g->Camera.param.pos;
    p = pPL->getPartsPtr(0);
    PosToPos(&g->Camera.param.at, &p->world, &w->Cam.param.at, rate);
    w->Cam.Up.x = 0.0f;
    w->Cam.Up.y = 1.0f;
    w->Cam.Up.z = 0.0f;
    w->Cam.Distance = VEC_DIST(&w->Cam.param.pos, &w->Cam.param.at);
    CameraSetOrientationUp(&w->Cam);
    CamCtrl.m_pExtraCamera = (s32) &w->Cam;
}

// Camera of the stamp: pulled behind and above the player.
void em2bStampCamMove(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    GlobalWork* g = pG;
    Vec v;
    cModel* p;

    w->Cam.param.fovy = g->Camera.param.fovy;
    v.x = 0.0f;
    v.y = 3000.0f;
    v.z = -3000.0f;
    PSMTXMultVec(pPL->mat, &v, &v);
    PosToPos(&g->Camera.param.pos, &v, &w->Cam.param.pos, 0.100000001f);
    p = pPL->getPartsPtr(0);
    PosToPos(&g->Camera.param.at, &p->world, &w->Cam.param.at, 0.300000012f);
    w->Cam.Up.x = 0.0f;
    w->Cam.Up.y = 1.0f;
    w->Cam.Up.z = 0.0f;
    w->Cam.Distance = VEC_DIST(&w->Cam.param.pos, &w->Cam.param.at);
    CameraSetOrientationUp(&w->Cam);
    CamCtrl.m_pExtraCamera = (s32) &w->Cam;
}

// Event placement: position / angle and the wait pose.
void cEm2b::setPos(Vec* p, f32 ang)
{
    Em2bWork* w = EM2B_WK(this);
    cEm2b* em = this;

    if (p) {
        pos = *p;
        pos_old = pos;
        this->ang.y = ang;
        if (w->pTree) {
            em2bVariantMot(this, w, 0x31, 0x78, 0x27, 0x6E, 0, 5);
        } else {
            int flip = em2bFlip(w, 5, 0x45);

            MotionSetCore(this, &Motion, ARC(0x19), ARC(0x62), 0, flip, 0);
        }
        MotionMove(this, 0);
        partsWorldCalc();
        EmRoutineSet(this, 1, 2, 0, 0xA);
    }
}

// Player blend motion (the strangle): the neck work is the second motion, the rate from 0x500.
void plBlendMotSet(cPlayer* pl, void* m0, void* m1, int a, int b)
{
    MotionWork* bm;
    f32 val = fabsf(pl->m_Blend);

    MotionSetCore(pl, &pl->Motion, m0, (void*) a, pl->m_Hokan, 1, pl->m_Frame);
    bm = (MotionWork*) &pl->m_SubMot;
    MotionSetCore(pl, bm, m1, (void*) b, pl->m_Hokan, 1, pl->m_Frame);
    pl->Motion.blend = bm;
    bm->Brate = val * 0.00390625f;
    if (pl->m_Hokan) {
        pl->m_Hokan--;
    }
    pl->m_Frame++;
    if (pl->m_Frame >= pl->Motion.Seq_frame_num) {
        pl->m_Frame = 0;
    }
}

// Event death: the trees are lost, the die routine runs.
void cEm2b::setEventDie()
{
    Em2bWork* w = EM2B_WK(this);

    if (w->pTree) {
        w->pTree->setLost();
        w->pTree->be_flag &= ~2;
        w->pTree = 0;
    }
    if (w->pTreeBrk) {
        w->pTreeBrk->setLost();
        w->pTreeBrk->be_flag &= ~2;
        w->pTreeBrk = 0;
    }
    EmRoutineSet(this, 3, 2, 0, 0);
}

// Unused: always 0 (kept for the original's dead code).
int em2bStaggerCk(cEm2b* em, Vec* pos)
{
    return 0;
}

// The first living dog becomes the friend the giant fights.
int em2bSearchDog(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    u32 i;

    if (w->pFriend) {
        return 0;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x21) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        w->pFriend = e;
        w->Go_dog_timer = 900;
        w->Dog_wait = 0;
        w->Be_flg |= 0x80;
        return 1;
    }
    return 0;
}

// Goes into the threat for half a second.
static inline void em2bThreatSet(cEm2b* em, Em2bWork* w)
{
    w->Dash_wait = 30;
    EmRoutineSet(em, 1, 0, 0, 0);
}

// The routine selecting rock throw / dash: coin flips guarded by the partner and the held rock.
static inline void em2bRockOrKickSet(cEm2b* em, Em2bWork* w)
{
    if (((Rnd() & 7) || pSUB) && w->pRock == 0) {
        EmRoutineSet(em, 1, 0x11, 0, 0);
    } else {
        EmRoutineSet(em, 1, 7, 0, 0);
    }
}

// Attack routine selection at the end of the wait / walk. 1 = a routine was set.
// The `return 1` after each if/else region is shared (one labelled `li r3,1; b END` per region):
// jump2 merges those copies into the Debug test's copy through a NEW label, and jumps to a new
// label never cross-jump again -- that is what keeps the identical EmRoutineSet(1, 0x11/7/9) arms
// apart in the target (COMPILER-DIFF #6 mechanism, no tag needed).
int em2bAtkRtnCk(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    if (StaFlagChk(pG, STA_PL_CATCHED) || EmDeadCk(pPL) || (s16) pG->pl_life <= 0) {
        if (em->l_pl < 49000000.0f) {
            em2bThreatSet(em, w);
            return 1;
        }
    }
    if (w->pTree) {
        if (w->L_go < 49000000.0f) {
            EmRoutineSet(em, 1, 0xE, 0, 0);
            return 1;
        }
        return 0;
    }
    if (w->Dash_wait) {
        return 0;
    }
    if (w->pTreeTarget) {
        return 0;
    }
    if (w->pGoto) {
        return 0;
    }
    if (pG->debug_mode == 7) {
        if (em2bAtkRtnCkDebug(em)) {
            return 1;
        }
        if (w->Debug_atk_rtn) {
            return 0;
        }
    }
    if (pPL->pos.y > em->pos.y + 2000.0f) {
        if (w->Pl_rot < 0.785398185f && em->l_pl < 16000000.0f) {
            EmRoutineSet(em, 1, 0x15, 0, 0);
            return 1;
        }
        return 0;
    }
    if (em2bPlInHouseCk(em)) {
        if (w->Go_rot < 0.785398185f && w->L_go < 25000000.0f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
            return 1;
        }
        return 0;
    }
    if (w->pRock && w->Go_rot < 0.785398185f && w->L_go > 36000000.0f) {
        EmRoutineSet(em, 1, 0x10, 0, 0);
        return 1;
    }
    if (w->Go_rot < 0.174532920f && w->L_go > 36000000.0f) {
        int dash = w->Punch_wait == 0;

        if (w->Be_flg & 0x80) {
            dash = 0;
        }
        if (w->pRock) {
            dash = 0;
        }
        if (em2bFriendCk(em)) {
            dash = 0;
        }
        if (pSUB) {
            dash = 0;
        }
        if (pG->room_id == 0x224 && !RmfFlagChk(pG, RMF_R119_DESTROY_YANE_A)) {
            dash = 0;
        }
        if (dash && em2bInScreenCk(em)) {
            EmRoutineSet(em, 1, 0xA, 0, 0);
            return 1;
        }
    }
    if (w->HoseiCnt > 3 && w->Go_rot < 0.523598790f) {
        w->HoseiCnt = 0;
        EmRoutineSet(em, 1, 0xC, 0, 0);
        return 1;
    }
    if (pSUB == 0) {
        if (w->Tree_brk_wait && w->Go_rot < 0.392699093f && w->L_go > 30250000.0f && w->L_go < 42250000.0f) {
            if (pG->Game_level <= 1 && (em->r_no_0 != 1 || em->r_no_1 != 0) && Rnd() % 10 > 4) {
                em2bThreatSet(em, w);
                return 1;
            }
            if (em2bPlRunCk(em)) {
                if ((Rnd() & 1) && w->pRock == 0) {
                    EmRoutineSet(em, 1, 0x11, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 9, 0, 0);
                }
            } else {
                EmRoutineSet(em, 1, 6, 0, 0);
            }
            return 1;
        }
        if (pSUB == 0 && w->Go_rot > 1.22173047f && w->L_go < 16000000.0f && pG->Game_level > 1) {
            EmRoutineSet(em, 1, 8, 0, 0);
            return 1;
        }
    }
    if (w->Go_rot < 0.392699093f && w->L_go > 6250000.0f && w->L_go < 12250000.0f) {
        if (pG->Game_level <= 1 && (em->r_no_0 != 1 || em->r_no_1 != 0) && Rnd() % 10 > 4) {
            em2bThreatSet(em, w);
            return 1;
        }
        if (em2bPlRunCk(em) && w->pRock == 0) {
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return 1;
        }
        if ((Rnd() & 1) && pSUB == 0) {
            EmRoutineSet(em, 1, 5, 0, 0);
        } else if (!em2bFriendCk(em)) {
            em2bRockOrKickSet(em, w);
        }
        return 1;
    }
    if (w->Go_rot < 0.698131680f && w->L_go < 9000000.0f) {
        if (pG->Game_level <= 1 && (em->r_no_0 != 1 || em->r_no_1 != 0) && Rnd() % 10 > 4) {
            em2bThreatSet(em, w);
            return 1;
        }
        if ((Rnd() & 3) && pSUB == 0) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else if (!em2bFriendCk(em)) {
            em2bRockOrKickSet(em, w);
        }
        return 1;
    }
    if (w->Go_rot > 2.09439516f && w->L_go < 9000000.0f && pG->Game_level > 1 && !em2bFriendCk(em)) {
        em2bRockOrKickSet(em, w);
        return 1;
    }
    if (em2bPlRunCk(em) && pSUB == 0 && !(w->Be_flg & 0x80) && pG->Game_level > 1) {
        if (w->Go_rot < 0.628318548f && w->L_go < 25000000.0f) {
            EmRoutineSet(em, 1, 9, 0, 0);
            return 1;
        }
        if (w->L_go < 25000000.0f && w->pRock == 0) {
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return 1;
        }
    }
    return 0;
}

// Debug page 7: the attack forced by Em2bWork::debugAtk when the target is in its range.
int em2bAtkRtnCkDebug(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    switch (w->Debug_atk_rtn) {
    case 1:
        if (w->Go_rot < 0.392699093f && w->L_go > 6250000.0f && w->L_go < 12250000.0f) {
            if (em2bPlRunCk(em)) {
                EmRoutineSet(em, 1, 0x11, 0, 0);
                return 1;
            } else {
                EmRoutineSet(em, 1, 0x11, 0, 0);
                return 1;
            }
        }
        if (w->Go_rot < 0.628318548f && w->L_go < 9000000.0f) {
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return 1;
        }
        if (w->Go_rot < 2.09439516f && w->L_go < 9000000.0f) {
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return 1;
        }
        if (em2bPlRunCk(em) && w->L_go < 25000000.0f) {
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return 1;
        }
        break;
    case 2:
        if (w->Go_rot < 0.392699093f && w->L_go > 6250000.0f && w->L_go < 12250000.0f) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return 1;
        }
        if (w->Go_rot < 0.628318548f && w->L_go < 9000000.0f) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return 1;
        }
        if (w->Go_rot > 1.22173047f && w->L_go < 16000000.0f) {
            EmRoutineSet(em, 1, 8, 0, 0);
            return 1;
        }
        if (w->Go_rot < 2.09439516f && w->L_go < 9000000.0f) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return 1;
        }
        break;
    case 3:
        if (w->Go_rot < 0.392699093f && w->L_go > 30250000.0f && w->L_go < 42250000.0f) {
            EmRoutineSet(em, 1, 6, 0, 0);
            return 1;
        }
        break;
    case 4:
        if (w->Go_rot < 0.392699093f && w->L_go > 6250000.0f && w->L_go < 12250000.0f) {
            EmRoutineSet(em, 1, 5, 0, 0);
            return 1;
        }
        break;
    case 5:
        if (w->Go_rot < 0.174532920f && w->L_go > 36000000.0f && em2bInScreenCk(em)) {
            EmRoutineSet(em, 1, 0xA, 0, 0);
            return 1;
        }
        break;
    case 6:
        if (w->Go_rot < 0.628318548f && w->L_go < 9000000.0f) {
            EmRoutineSet(em, 1, 9, 0, 0);
            return 1;
        }
        if (em2bPlRunCk(em) && w->Go_rot < 0.628318548f && w->L_go < 25000000.0f) {
            EmRoutineSet(em, 1, 9, 0, 0);
            return 1;
        }
        break;
    case 0:
    default:
        break;
    }
    return 0;
}

// The routine after an attack: rock / tree pickup, an attack, else turn or walk.
void em2bNextRtnSet(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);

    em2bSearchRockCk(em);
    if (em2bGetRockCk(em)) {
        return;
    }
    em2bSearchTree(em);
    if (em2bGetTreeCk(em)) {
        return;
    }
    if (em2bAtkRtnCk(em)) {
        return;
    }
    if (w->Go_rot > 2.35619450f) {
        if (w->L_go < 9000000.0f && !em2bFriendCk(em)) {
            if (pG->debug_mode == 7 && w->Debug_atk_rtn) {
                EmRoutineSet(em, 1, 3, 0, 0);
                return;
            }
            if ((Rnd() & 1) || pSUB) {
                EmRoutineSet(em, 1, 0x11, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            return;
        }
        EmRoutineSet(em, 1, 3, 0, 0);
    } else {
        EmRoutineSet(em, 1, 2, 0, 0xA);
    }
}

// Is the root parts on screen?
int em2bInScreenCk(cEm2b* em)
{
    Vec scr;
    Vec pos = em->getPartsPtr(0)->world;

    GetScreenPos(&pos, &scr);
    if (scr.z > 1.0f) {
        return 0;
    }
    if (scr.x < -300.0f || scr.x > 812.0f) {
        return 0;
    }
    if (scr.y < -300.0f) {
        return 0;
    }
    if (scr.y > 748.0f) {
        return 0;
    }
    return 1;
}

// Motion key bit2 and the player near / in the box in front: the dash escape button shows.
int em2bPlDashEscapeCk(cEm2b* em)
{
    Mtx inv;
    Vec v;

    if (!(em->Motion.Seq_old.Free & 4)) {
        return 0;
    }
    if (em->l_pl < 9000000.0f) {
        return 1;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x > 2500.0f) {
        return 0;
    }
    if (v.x < -2500.0f) {
        return 0;
    }
    if (v.z > 7000.0f) {
        return 0;
    }
    if (v.z < -2500.0f) {
        return 0;
    }
    return 1;
}

// Is the player running (routine 0/3) in front of the giant, within a 7000 wide lane?
int em2bPlRunCk(cEm2b* em)
{
    Mtx inv;
    Vec v;

    if (pPL->r_no_0 != 0) {
        return 0;
    }
    if (pPL->r_no_1 != 3) {
        return 0;
    }
    if (pG->Game_level <= 3) {
        return 0;
    }
    if (em2bFriendCk(em)) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, 3.14159274f)) > 1.57079637f) {
        return 0;
    }
    PSMTXInverse(pPL->mat, inv);
    PSMTXMultVec(inv, &em->pos, &v);
    if (v.x > 3500.0f) {
        return 0;
    }
    if (v.x < -3500.0f) {
        return 0;
    }
    return 1;
}

// Falling giant: the player under one of the body parts is crushed.
int em2bPressPlCk(cEm2b* em)
{
    int parts[5] = { 3, 2, 0, 0x12, 0x16 };
    Vec pos;
    u32 i;

    if (EmDeadCk(pPL)) {
        return 0;
    }
    pos = pPL->pos;
    pos.y += 1500.0f;
    for (i = 0; i < 5; i++) {
        cModel* m = em->getPartsPtr(parts[i]);

        if ((pos.x - m->world.x) * (pos.x - m->world.x) + (pos.y - m->world.y) * (pos.y - m->world.y) +
                (pos.z - m->world.z) * (pos.z - m->world.z) <
            9000000.0f) {
            LifeDownSet(pPL, 9999, 0);
            SetPlDamage(em, plem2b_dm_Stamp);
            return 1;
        }
    }
    return 0;
}

// Stamp on the partner: when one of the giant's feet / body parts (3, 2, 0, 0x12, 0x16) is within 3000
// units of Ashley's chest she is killed (ashley_life 0, subem2b_dm_Stamp). 1 = crushed.
int em2bPressSubCk(cEm2b* em)
{
    int parts[5] = { 3, 2, 0, 0x12, 0x16 };
    Vec pos;
    u32 i;

    if (pSUB == 0) {
        return 0;
    }
    if (EmDeadCk(pSUB)) {
        return 0;
    }
    pos = pSUB->pos;
    pos.y += 1500.0f;
    for (i = 0; i < 5; i++) {
        cModel* m = em->getPartsPtr(parts[i]);

        if ((pos.x - m->world.x) * (pos.x - m->world.x) + (pos.y - m->world.y) * (pos.y - m->world.y) +
                (pos.z - m->world.z) * (pos.z - m->world.z) <
            9000000.0f) {
            pG->ashley_life = 0;
            SetSubDamage(em, (void (*)()) subem2b_dm_Stamp);
            return 1;
        }
    }
    return 0;
}

// Moves the giant to the catch spot (EMI kind 0xD) nearest to the player, facing its angle.
void em2bCatchPosSet(cEm2b* em)
{
    Vec pos;
    f32 best;
    f32 ang;
    int i;

    if (pG->stage_no == 1 && pG->room_no == 0x19) {
        em->pos.x = 114800.0f;
        em->pos.y = 2230.0f;
        em->pos.z = 8000.0f;
        return;
    }
    if (pG->pEmi == 0) {
        return;
    }
    pos = em->pos;
    best = 1.0e16f;
    ang = em->ang.y;
    for (i = 0; i < pG->pEmi->n; i++) {
        EmiEntry* e = &pG->pEmi->entry[i];
        f32 d;

        if (e->type != 0xD) {
            continue;
        }
        d = (pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z);
        if (d > best) {
            continue;
        }
        best = d;
        ang = e->rotY;
        if (e->sub == 1) {
            pos = e->pos;
        }
    }
    if (fabsf(Muku2(em->ang.y, ang, 3.14159274f)) < 1.57079637f) {
        em->ang.y = ang;
    } else {
        em->ang.y = ang + 3.14159274f;
    }
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    em->pos = pos;
}

// Deletes the parasite's tentacles; with set != 0 creates the six of them (slots 1, 3, 4, 7 unused),
// each a tenth of the motion further in.
void em2bSetTentacle(cEm2b* em, int set)
{
    Em2bWork* w = EM2B_WK(em);
    u16 step = (((MotionData*) ARC(0xE2))->maxFrame & 0x3FFF) / 10u;
    u32 i;

    for (i = 0; i <= 9; i++) {
        if (w->pTen[i]) {
            w->pTen[i]->clearLostWait();
            w->pTen[i] = 0;
        }
    }
    if (set == 0) {
        return;
    }
    for (i = 0; i <= 9; i++) {
        Vec pos;
        Vec rot;
        Vec scale;

        if (i == 1 || i == 3 || i == 4 || i == 7) {
            continue;
        }
        switch (i) {
        case 0:
        default:
            pos.x = -474.64f;
            pos.y = 1089.79f;
            pos.z = -718.95f;
            break;
        case 1:
            pos.x = 690.68f;
            pos.y = 909.27f;
            pos.z = -592.16f;
            break;
        case 2:
            pos.x = -193.15f;
            pos.y = 750.07f;
            pos.z = -833.08f;
            break;
        case 3:
            pos.x = 60.29f;
            pos.y = 695.31f;
            pos.z = -703.91f;
            break;
        case 4:
            pos.x = 684.77f;
            pos.y = 1087.43f;
            pos.z = -437.2f;
            break;
        case 5:
            pos.x = -190.33f;
            pos.y = 1285.31f;
            pos.z = -590.69f;
            break;
        case 6:
            pos.x = 578.94f;
            pos.y = 1367.31f;
            pos.z = -413.35f;
            break;
        case 7:
            pos.x = 46.9f;
            pos.y = 1166.25f;
            pos.z = -573.56f;
            break;
        case 8:
            pos.x = -88.42f;
            pos.y = 1504.13f;
            pos.z = -523.64f;
            break;
        case 9:
            pos.x = 176.79f;
            pos.y = 1178.84f;
            pos.z = -605.74f;
            break;
        }
        switch (i) {
        case 0:
        default:
            rot.x = -1.2217305f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            break;
        case 1:
            rot.x = -1.5707964f;
            rot.y = 0.2617994f;
            rot.z = 0.0f;
            break;
        case 2:
            rot.x = -1.4824479f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            break;
        case 3:
            rot.x = -1.9198623f;
            rot.y = -0.5235988f;
            rot.z = 0.0f;
            break;
        case 4:
            rot.x = -1.5707964f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            break;
        case 5:
            rot.x = -1.7453293f;
            rot.y = 1.3962634f;
            rot.z = 0.34906584f;
            break;
        case 6:
            rot.x = -1.3089969f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            break;
        case 7:
            rot.x = -2.0943952f;
            rot.y = 0.0f;
            rot.z = 0.34906584f;
            break;
        case 8:
            rot.x = -1.2217305f;
            rot.y = 0.0f;
            rot.z = 0.5235988f;
            break;
        case 9:
            rot.x = -0.69813174f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            break;
        }
        switch (i) {
        case 0:
        default:
            scale.x = 1.5f;
            scale.y = 1.5f;
            scale.z = 1.5f;
            break;
        case 1:
            scale.x = 1.5f;
            scale.y = 1.2f;
            scale.z = 1.5f;
            break;
        case 2:
            scale.x = 1.5f;
            scale.y = 0.8f;
            scale.z = 1.5f;
            break;
        case 3:
            scale.x = 1.5f;
            scale.y = 1.3f;
            scale.z = 1.5f;
            break;
        case 4:
            scale.x = 1.5f;
            scale.y = 2.0f;
            scale.z = 1.5f;
            break;
        case 5:
            scale.x = 1.5f;
            scale.y = 1.3f;
            scale.z = 1.5f;
            break;
        case 6:
            scale.x = 1.5f;
            scale.y = 0.9f;
            scale.z = 1.5f;
            break;
        case 7:
            scale.x = 1.5f;
            scale.y = 1.5f;
            scale.z = 1.5f;
            break;
        case 8:
            scale.x = 1.5f;
            scale.y = 1.0f;
            scale.z = 1.5f;
            break;
        case 9:
            scale.x = 1.5f;
            scale.y = 1.0f;
            scale.z = 1.5f;
            break;
        }
        w->pTen[i] = (cObj16*) SetObj16(ARC(0xE0), ARC(0xE1), em, em, 2, 9, &pos, &rot);
        if (w->pTen[i]) {
            MotSetObj16(w->pTen[i], ARC(0xE2), 4, step * i);
            w->pTen[i]->setScale(&scale);
        }
    }
}

// Damage of the weapon that hit: half more on the head (parts 5), a quarter on the armoured variants.
int em2bSetDmVal(cEm2b* em)
{
    YARARE_INFO* part = em->dmg.m_pDamageYarare;
    int near = 0;
    int dmg;

    if (part->len < 64000000.0f) {
        near = 1;
    }
    dmg = 10;
    if (em->dmg.m_Wep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmg.m_Wep, near);
    }
    if (part->parts_no == 5) {
        dmg += dmg / 2;
    }
    switch (em->type) {
    case 1:
    case 3:
        dmg = dmg / 4 + 1;
        break;
    case 0:
    case 2:
        break;
    }
    return dmg;
}

// Another giant in battle is nearer to the player: wait (except far away, or at the room 224 drop).
int em2bStayCk(cEm2b* em)
{
    int cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2B) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(EM_STATUS_ACTIVE)) {
            continue;
        }
        if (e->l_pl < em->l_pl) {
            cnt++;
        }
    }
    if (cnt == 0 || em->l_pl > 144000000.0f) {
        return 1;
    }
    if (pG->room_id == 0x224) {
        if ((em2b_r11e_pos.x - em->pos.x) * (em2b_r11e_pos.x - em->pos.x) +
                (em2b_r11e_pos.z - em->pos.z) * (em2b_r11e_pos.z - em->pos.z) <
            16000000.0f) {
            return 1;
        }
    }
    return 0;
}

// Pushes this giant 3000 away from any other giant in battle it overlaps.
void em2bObaHitCk(cEm2b* em)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);
        Vec d;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2B) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(EM_STATUS_ACTIVE)) {
            continue;
        }
        PSVECSubtract(&em->pos, &e->pos, &d);
        d.y = 0.0f;
        if (d.x * d.x + d.z * d.z > 9000000.0f) {
            continue;
        }
#line 9308 "D:/Bio4/Prog/em2b.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 3000.0f);
        PSVECAdd(&e->pos, &d, &em->pos);
        PartsWorldPosCalc(em);
    }
}

// 1 while the parasite head object is out of the back (pParasite).
int cEm2b::ckParasite()
{
    Em2bWork* w = EM2B_WK(this);

    if (w->pParasite) {
        return 1;
    }
    return 0;
}

// Die routine 1 (lost): squashes every parts by the y scale rate, keeping the world positions.
void em2bScaleCompress(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    Mtx m;
    Vec s;
    cParts* p;

    if (em->r_no_0 != 3) {
        return;
    }
    if (em->r_no_1 != 1) {
        return;
    }
    PSMTXIdentity(m);
    s.y = w->Compress_y;
    s.z = s.x = 1.0f;
    ScaleMatrix(m, &s);
    for (p = em->pList; p; p = p->pList) {
        PSMTXConcat(m, p->mat, p->mat);
        p->mat[0][3] = p->world.x;
        p->mat[1][3] = p->world.y;
        p->mat[2][3] = p->world.z;
    }
}

// Another living giant in battle exists.
int em2bFriendCk(cEm2b* em)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2B) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (!e->checkStatus(EM_STATUS_ACTIVE)) {
            continue;
        }
        return 1;
    }
    return 0;
}

// Room 224: the giant near the opened floor hatch falls to its death; further out it is pulled
// 8000 from the hatch centre.
int cEm2b::ckR224Drop()
{
    Vec d;
    f32 dist;

    if (hp <= 0) {
        return 0;
    }
    if (pG->room_id != 0x224) {
        return 0;
    }
    dist = (pos.x - em2b_r11e_pos.x) * (pos.x - em2b_r11e_pos.x) + (pos.z - em2b_r11e_pos.z) * (pos.z - em2b_r11e_pos.z);
    if (dist > 64000000.0f) {
        return 0;
    }
    if (!RmfFlagChk(pG, RMF_R119_DESTROY_KOYA_A)) {
        return 0;
    }
    if (dist > 25000000.0f) {
        PSVECSubtract(&pos, &em2b_r11e_pos, &d);
#line 9432 "D:/Bio4/Prog/em2b.cpp"
        VECNormalize(&d, &d);
        d.y = 0.0f;
        PSVECScale(&d, &d, 8000.0f);
        PSVECAdd(&em2b_r11e_pos, &d, &d);
        d.y = 0.0f;
        cModel::setPos(&d);
        return 0;
    }
    // A local for the zero: stored directly, EmRoutineSet's two 0 arguments below (past three
    // calls) share a register with this store instead of getting their own.
    s16 n = 0;
    hp = n;
    EmSetDie(this);
    EmReserveDropItem(this);
    EmSetDieCnt(this);
    atari.throughOn();
    EmRoutineSet(this, 3, 3, 0, 0);
    return 1;
}

// The tower (obj 0x39) within 8000 of the giant, for the base attack.
void em2bYaguraSearch(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    u32 i;

    w->pYagura = 0;
    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObj* o = ObjMgr.fastAt(i);

        if ((o->be_flag & 0x201) != 1) {
            continue;
        }
        if (o->id != 0x39) {
            continue;
        }
        if ((em->pos.x - o->pos.x) * (em->pos.x - o->pos.x) + (em->pos.y - o->pos.y) * (em->pos.y - o->pos.y) +
                (em->pos.z - o->pos.z) * (em->pos.z - o->pos.z) >
            64000000.0f) {
            continue;
        }
        w->pYagura = (cObjYagura*) o;
        return;
    }
}

// Room 224: the texture render manager of the hole attack's freeze effect.
void em2bTexrenderInit(cEm2b* em)
{
    Em2bWork* w = EM2B_WK(em);
    u8* tbl = w->Tex_buf;

    if (pG->room_id != 0x224) {
        return;
    }
    w->pMgr = Ctrl12GetTexRenderEm2b(w->pCtrlGroup);
    if (w->pMgr == 0) {
        pLog->err(0, 0, "em2bTexrenderInit:: Manager alloc failed!!");
        return;
    }
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = w->pMgr->m_Tex_no;
    w->pMgr->m_Rep_type = 1;
    w->pMgr->m_H_size = w->pMgr->m_W_size = 0x40;
}

// 1 while the giant kneels with the parasite exposed (Be_flg bit14: Dm_Face / Dm_Tree), the window for
// the player's climb-and-slash attack.
int cEm2b::ckSit()
{
    Em2bWork* w = EM2B_WK(this);

    if (w->Be_flg & 0x4000) {
        return 1;
    }
    return 0;
}

