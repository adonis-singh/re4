// pl0a module, second object (D:/Bio4/Prog/pl_klauser.cpp): Krauser: the Leon model set plus the three
// fading arm / mutation models (transMove), the X-button attack routine (pl_R1_KlauserAttack) and the
// tex-render material of the mutation model.
//
// cPlKlauser (pl_mod.h) is the cPlayer of pl_type 4 (Krauser in the mercenaries). On top of the
// Leon-style model set it carries three cModelInfos in krModel[] (em.h): [0] the normal arm, [1]
// the mutated arm, [2] a tex-rendered glow model; Status_flg[3] bit23 (set by the X-button attack)
// selects which arm is faded in by transMove every frame (moveMatCalcBefore). krEffWait counts frames
// until the idle effects (EstSet 0x3F group) are spawned again, x890 is cleared by the interrupt.
// pl_R1_KlauserAttack is the aux routine (r_no_1 == 0xA through pFuncAux) of the mutation attack.
// Pl0aInit is the module's PlInitFunc; the first object of the module is the reduced pl_shotgun.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "pl_mod.h"
#include "db_log.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "joy.h"
#include "motion.h"
#include "TexRender.h"
#include <dolphin/os.h>


// Plain block, not do/while(0) (pl_leon.cpp).

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.

static void pl_R1_KlauserAttack(cPlayer* pl);

static TexRenderMng* pl0aTex;
static u8 pl0aTexTbl[0x20];
static f32 pl0aAlphaBase = 80.0f;
asm(".section .data\n\t.balign 8\n\t.text");   // the module's .data is 8-aligned before the BSS tag

// Gives the mutation glow model (krModel[2]) a texture-render material: allocates a TexRenderMng
// (pl0aTex, replace type 1) whose render texture id goes into the blend table pl0aTexTbl, spawns
// the render-target effect (EstSet type 3/0xC on the manager's mask) and sets the model's blend
// table / ratio 0xFF / blend type 1.
extern "C" void setTexRender(cModelInfo* info)
{
    u8* tbl = pl0aTexTbl;

    if (GetTexRenderMgr(&pl0aTex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = pl0aTex->m_Tex_no;
        pl0aTex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, EFF_PL00, 0xC, pl0aTex->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "SetTexRender() : Manager alloc failed!!");
    }
    info->setTexBlendTbl(tbl);
    info->setBlendRatio(0xFF);
    info->setBlendType(1);
}

// Builds Krauser: the cPlayer work init, the player effects (archive 0x1A as group 3), the model
// set, the equipped weapon module, the routine init, the event motions, startUp; the mutation
// state starts normal (x880 = 1, krEffWait = 1: idle effects next frame, Status_flg[3] bit23 off).
cPlKlauser::cPlKlauser()
{
    init0();
    EspDataLoad((u32) PL_ARC(0x1A), EFF_PL00, 0);
    setModel();
    weaponRelease();
    weaponLoad(pG->weapon_no, pG->weapon_type);
    weaponInit();
    init1();
    setMotion();
    startUp();
    x880 = 1.0f;
    krX7B8 = 0;
    krX7C0 = 0;
    x890 = 0;
    krEffWait = 1;
    StaFlagOff(pG, STA_KLAUSER_TRANSFORM);
    pFsdTbl = pl_fs_tbl;
}

// Installs the character's event / action motions (m_MotTbl 0x5F..0x6C from the player archive
// 0x32..0x3F); the weapon module fills the footwork slots.
void cPlKlauser::setMotion()
{
    PLA_MOT(this, 0x5F, 0x32);
    PLA_MOT(this, 0x60, 0x33);
    PLA_MOT(this, 0x61, 0x34);
    PLA_MOT(this, 0x62, 0x35);
    PLA_MOT(this, 0x63, 0x36);
    PLA_MOT(this, 0x64, 0x37);
    PLA_MOT(this, 0x65, 0x38);
    PLA_MOT(this, 0x66, 0x39);
    PLA_MOT(this, 0x6B, 0x3A);
    PLA_MOT(this, 0x6C, 0x3B);
    PLA_MOT(this, 0x67, 0x3C);
    PLA_MOT(this, 0x68, 0x3D);
    PLA_MOT(this, 0x69, 0x3E);
    PLA_MOT(this, 0x6A, 0x3F);
}

// Per-frame update: the common cPlayer::move, then the arm's idle effects (EstSet types 0 and
// 0x15 in group 0x3F on the player) once krEffWait counts down to 0; the debug combination Joy
// 0x640 kills the effects and restarts the count.
void cPlKlauser::move()
{
    cPlayer::move();
    if (krEffWait > 0) {
        krEffWait--;
        if (krEffWait == 0) {
            EstSet(this, -1, 0, 0, EFF_PL00, 0, 0, ESP_CORE_KIND_MARK, this, 0);
            EstSet(this, -1, 0, 0, EFF_PL00, 0x15, 0, ESP_CORE_KIND_MARK, this, 0);
        }
    }
    if ((Joy[0].on & 0x640) == 0x640) {
        EffectEspDelete(0, ESP_CORE_KIND_MARK, this, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_MARK, this);
        EffectEfmDelete(0, ESP_CORE_KIND_MARK, this);
        krEffWait = 1;
    }
}

// Before the matrix calculation of the frame: the arm / glow alpha fades.
void cPlKlauser::moveMatCalcBefore()
{
    transMove();
}

// Fades a model info's alpha byte in (+0x40 up to 0xFF): a store per arm (jump2 merges them; a single
// store of a result variable lets jump1 hoist the constant arm above the compare).
static inline void alphaUp(cModelInfo* m)
{
    int a = m->color[3];

    if (a <= 0xBE) {
        m->color[3] = a + 0x40;
    } else {
        m->color[3] = 0xFF;
    }
}

// Fades it out by `step` (halved above 0x80); written out in both arms (a helper taking `step`
// by reference puts it in a stack slot).
#define ALPHA_DOWN(m, step)                     \
    {                                           \
        int v;                                  \
        if ((m)->color[3] > 0x80) {             \
            step >>= 1;                         \
        }                                       \
        if ((m)->color[3] > step) {             \
            (m)->color[3] = (m)->color[3] - step; \
        } else {                                \
            (m)->color[3] = 0;                  \
        }                                       \
    }

// be_flag bit3 (draw) follows the alpha byte.
static inline void alphaFlag(cModelInfo* m)
{
    if (m->color[3]) {
        m->be_flag |= 8;
    } else {
        m->be_flag &= ~8;
    }
}

// Arm cross-fade: while Status_flg[3] bit23 (mutated) the mutated arm krModel[1] fades in
// (+0x40 per frame) and the normal arm krModel[0] out, else the reverse; be_flag bit3 (draw)
// follows the alpha. The glow model krModel[2] fades out while krEffWait != 0 (effects pending /
// attack cooldown), else fades in and pulses on a 32-frame triangle (x898) between pl0aAlphaBase
// (80) and 255.
void cPlKlauser::transMove()
{
    int step = 0x40;

    if (StaFlagChk(pG, STA_KLAUSER_TRANSFORM)) {
        alphaUp(krModel[1]);
        ALPHA_DOWN(krModel[0], step);
    } else {
        alphaUp(krModel[0]);
        ALPHA_DOWN(krModel[1], step);
    }
    alphaFlag(krModel[0]);
    alphaFlag(krModel[1]);
    if (krEffWait != 0) {
        if (krModel[2]->color[3] > step) {
            krModel[2]->color[3] = krModel[2]->color[3] - step;
        } else {
            krModel[2]->color[3] = 0;
        }
    } else {
        int a;
        f32 p;
        f32 v;

        a = krModel[2]->color[3];
        if (a < 0xFF - step) {
            krModel[2]->color[3] = a + step;
        } else {
            krModel[2]->color[3] = 0xFF;
        }
        if (x898 <= 0xF) {
            p = (f32) krModel[2]->color[3] * (f32) (x898 + 1) * 0.0625f;
        } else {
            p = (f32) krModel[2]->color[3] * (f32) (0x20 - x898) * 0.0625f;
        }
        v = p * (256.0f - pl0aAlphaBase) * 0.00390625f + pl0aAlphaBase;
        if (v > 255.0f) {
            v = 255.0f;
        }
        krModel[2]->color[3] = (int) v;
        x898++;
        if (x898 > 0x1F) {
            x898 = 0;
        }
    }
    alphaFlag(krModel[2]);
}

// X button (cPlayer::actionSelect / PlKnifeMove): with the arm normal and the effects settled
// (krEffWait == 0) starts the mutation attack: routine 1 = 0xA (aux) with pFuncAux =
// pl_R1_KlauserAttack, krEffWait = -1 (glow off). Returns 1 when the routine was taken.
int cPlKlauser::checkXbutton()
{
    if ((Joy[0].trg & 0x400) && !StaFlagChk(pG, STA_KLAUSER_TRANSFORM) && krEffWait == 0) {
        pFuncAux = pl_R1_KlauserAttack;
        r_no_0 = 0;
        r_no_1 = 0xA;
        r_no_2 = 0;
        r_no_3 = 0;
        krEffWait = -1;
        return 1;
    }
    return 0;
}

// Builds the model set: the body (4/5) as the base model, the head (6/7, Body->pShape), the
// normal arm (8/9, krModel[0], be_flag 0x20), the mutated arm (0xA/0xB, krModel[1], alpha 0 and
// hidden), the face (0xE/9, Body->m_pKnife with the blend weights zeroed), an extra part (0xF/0x10)
// and the glow model (0x18/0x19, krModel[2], hidden, invisible_factor 0.9999, tex-render
// material); TEV scale group 1, bare right hand, left hand 1.
void cPlKlauser::setModel()
{
    cModelInfo* info;
    cModelInfo* face;

    info = (cModelInfo*) modelInit(PL_ARC(4), PL_ARC(5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlKlauser::setModel() failed.");
        return;
    }
    info = ModInfoMgr.create(PL_ARC(6), PL_ARC(7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlKlauser::setModel() failed.");
        return;
    }
    addModel(info);
    Body->m_pFace = info;
    info = ModInfoMgr.create(PL_ARC(8), PL_ARC(9));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlKlauser::setModel() failed.");
        return;
    }
    krModel[0] = info;
    info->be_flag |= 0x20;
    addModel(info);
    info = ModInfoMgr.create(PL_ARC(0xA), PL_ARC(0xB));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlKlauser::setModel() failed.");
        return;
    }
    krModel[1] = info;
    info->color[3] = 0;
    info->be_flag |= 0x20;
    info->be_flag &= ~8;
    addModel(info);
    info = ModInfoMgr.create(PL_ARC(0xE), PL_ARC(9));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    Body->m_pKnife = info;
    face = Body->m_pKnife;
    if (VALID_PTR(face)) {
        face->mat[2][2] = 0.0f;
        face->mat[1][1] = 0.0f;
        face->mat[0][0] = 0.0f;
    }
    info = ModInfoMgr.create(PL_ARC(0xF), PL_ARC(0x10));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    info = ModInfoMgr.create(PL_ARC(0x18), PL_ARC(0x19));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    krModel[2] = info;
    info->be_flag &= ~8;
    krModel[2]->color[3] = 0;
    krModel[2]->invisible_factor = 0.9999f;
    setTexRender(info);
    TevScaleGroup = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(1);
}

// Right hand model: 1 = the weapon module's hand (Body->pWepHand), 0 and 2..9 = the bare hand
// (0x12, texture 0x11), any other value a model data pointer; replaces Body->pRight. HALTs
// (pl_klauser.cpp line 586) when the model info cannot be created.
void cPlKlauser::setRightHand(int no)
{
    cModelInfo* info;
    void* data;

    if (Body->m_pHandR) {
        deleteModelInfo(Body->m_pHandR);
        Body->m_pHandR = 0;
        Body->pRightData = 0;
    }
    switch ((u32) no) {
    case 0:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
        data = PL_ARC(0x12);
        break;
    case 1:
        data = Body->pWepHand;
        break;
    default:
        data = (void*) no;
        break;
    }
    if ((info = ModInfoMgr.create(data, PL_ARC(0x11))) != 0) {
        addModel(info);
        Body->m_pHandR = info;
        Body->pRightData = data;
    }
    if (!info) {
#line 586 "D:/Bio4/Prog/pl_klauser.cpp"
        HALT();
    }
}

// Left hand model: 0..6, 8, 9 the bare hand (0x14), 7 the gripping hand (0x15), 0x63 = the
// previous one, any other value a model data pointer; replaces Body->pLeft.
void cPlKlauser::setLeftHand(u32 no)
{
    cModelInfo* info;
    void* data;

    if (Body->m_pHandL) {
        deleteModelInfo(Body->m_pHandL);
        Body->m_pHandL = 0;
        Body->pLeftData = 0;
    }
    if (no == 0x63) {
        no = Body->oldLhandNo;
    }
    switch (no) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 8:
    case 9:
        data = PL_ARC(0x14);
        break;
    case 7:
        data = PL_ARC(0x15);
        break;
    default:
        data = (void*) no;
        break;
    }
    Body->oldLhandNo = Body->nowLhandNo;
    Body->nowLhandNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pG->pPlayer, 0x11));
    if (info == 0) {
        pLog->err(0, 0, "cPlKlauser::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        Body->m_pHandL = info;
        Body->pLeftData = data;
    }
}

// Krauser has no face shapes.
void cPlKlauser::setFace(int no)
{
}

// Head swap by number (only 0 does anything): replaces the head model with the archive's 0xC and
// forgets Body->pShape.
void cPlKlauser::setHead(int no)
{
    cModelInfo* info;

    if (no != 0) {
        return;
    }
    if (Body->m_pFace == 0) {
        return;
    }
    deleteModelInfo(Body->m_pFace);
    Body->m_pFace = 0;
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 0xC), PL_ARC_PTR(pG->pPlayer, 7));
    if (info) {
        addModel(info);
    }
}

// Head swap with explicit model / texture data (events): replaces the head model.
void cPlKlauser::setHead(void* bin, void* tpl)
{
    cModelInfo* info;

    if (Body->m_pFace == 0) {
        return;
    }
    deleteModelInfo(Body->m_pFace);
    Body->m_pFace = 0;
    info = ModInfoMgr.create(bin, tpl);
    if (info) {
        addModel(info);
    }
}

// Routine 1 / 0xA (cPlayer::pAuxFunc): the X-button mutation attack. r_no_2 steps: 0/1 the arm
// transforms (motion 0x8A, Status_flg[3] bit23 on, a 1000..2000-unit damage area, the change
// effects / SEs, the weapon hidden); 0xA/0xB the mutated stance 0x8B (stick turns; A button ->
// the slash 0x14, X/B -> revert 0x1E); 0x14/0x15 the slash 0x8C: PlWepHitCheck2 as weapon 0x2D
// over 3 m for the first 15 frames with the player's own damage info armed (dmg.set 0x80) and
// atari priority raised, the arm reverts at frame 30, then a 0x546-frame cooldown (krEffWait) and
// back to footwork; 0x1E/0x1F the revert motion 0x89 (bit23 off, weapon shown) into footwork.
static void pl_R1_KlauserAttack(cPlayer* pl)
{
    const f32 hitLen = 3000.0f;   // pool order: the case-0x15 constant comes first

    switch (pl->r_no_2) {
    case 0:
        pl->motionSet(PL_ARC(0x8A), 5, 0, 1, 0);
        pl->x890 = 10;
        StaFlagOn(pG, STA_KLAUSER_TRANSFORM);
        pl->Neck->clear();
        DmgMgr.set(DMG_TYPE_PUSH, 0x1E, &pl->pos, 1000.0f, 2000.0f);
        EffectEspDelete(0, ESP_CORE_KIND_MARK, pl, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_MARK, pl);
        EffectEfmDelete(0, ESP_CORE_KIND_MARK, pl);
        EstSet(pl, -1, 0, 0, EFF_PL00, 0xA, 0, ESP_CORE_KIND_MARK, pl, 0);
        SndCall(1, 0x51, &pl->pos, 0, 0, 0);
        SndCall(1, 0x52, &pl->pos, 0, 0, 0);
        pl->Wep->m_pWep->setDisp(1, 0);
        pl->r_no_2 = 1;
        // fallthrough
    case 1:
        if (pl->motionMove()) {
            pl->r_no_2 = 0xA;
        }
        break;
    case 0xA:
        pl->motionSet(PL_ARC(0x8B), 5, 0, 1, 0);
        pl->r_no_2 = 0xB;
        break;
    case 0xB:
        if (Key.on & 4) {
            pl->ang.y -= cPlayer::SPEED_WALK_TURN;
        }
        if (Key.on & 8) {
            pl->ang.y += cPlayer::SPEED_WALK_TURN;
        }
        pl->motionMove();
        if (Joy[0].trg & 0x100) {
            pl->r_no_2 = 0x14;
        } else if (Joy[0].trg & 0x600) {
            pl->r_no_2 = 0x1E;
        }
        break;
    case 0x14:
        pl->motionSet(PL_ARC(0x8C), 5, 0, 1, 0);
        pl->atari.setPriority(PRI_LV2);
        pl->dmg.set(0, 0x80);
        EstSet(pl, -1, 0, 0, EFF_PL00, 0xB, 0, ESP_CORE_KIND_MARK, pl, 0);
        SndCall(1, 0x53, &pl->pos, 0, 0, 0);
        pl->r_no_2 = 0x15;
        // fallthrough
    case 0x15:
        if (pl->Motion.Seq_frame <= 15.0f) {
            PlWepHitCheck2(0, &pl->pos, &pl->pos, 0x2D, 0, hitLen);
        }
        if (MotionCheckCrossFrame(&pl->Motion, 30.0f)) {
            pl->x890 = 0x14;
            StaFlagOff(pG, STA_KLAUSER_TRANSFORM);
        }
        if (pl->motionMove()) {
            pl->dmg.clear();
            pl->Wep->m_pWep->setDisp(1, 1);
            pl->atari.setPriority(0);
            EffectEspDelete(0, ESP_CORE_KIND_MARK, pl, 0);
            EffectEspgenDelete(0, ESP_CORE_KIND_MARK, pl);
            EffectEfmDelete(0, ESP_CORE_KIND_MARK, pl);
            pl->krEffWait = 0x546;
            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
        }
        break;
    case 0x1E:
        pl->motionSet(PL_ARC(0x89), 5, 0, 1, 0);
        pl->x890 = 0x14;
        StaFlagOff(pG, STA_KLAUSER_TRANSFORM);
        pl->krEffWait = 1;
        EffectEspDelete(0, ESP_CORE_KIND_MARK, pl, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_MARK, pl);
        EffectEfmDelete(0, ESP_CORE_KIND_MARK, pl);
        SndCall(1, 0x52, &pl->pos, 0, 0, 0);
        pl->Wep->m_pWep->setDisp(1, 1);
        pl->r_no_2 = 0x1F;
        // fallthrough
    case 0x1F:
        if (pl->motionMove()) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
        }
        break;
    }
}

// PlInitFunc: placement-constructs Krauser in the player's cEm work.
void Pl0aInit(cEm* em)
{
    new (em) cPlKlauser();
}

// REL entry: registers the player constructor.
extern "C" void _prolog()
{
    PlInitFunc = Pl0aInit;
    OSReport("Pl0a Klauser prolog Ok\n");
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
