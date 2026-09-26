// em3e module (D:/Bio4/Prog/emmark.cpp): the shooting-gallery targets (cEmMark) of room 22c. The room
// creates a target from an EmMarkData record; the target then runs its instruction list (begin: rise,
// stay: wait N frames, move: walk to an integer position, end: fold down / vanish) and reports hits to
// the room through R22cHitMark / R22cHitEffect.
//
// em3eInit is the module's EmInitFunc; the room (st2 r22c) then calls cEmMark::init with the
// record. Types 0..6 are the targets (0 Ganado, 1, 2 Ashley = the "don't shoot" target that folds
// when older targets remain, 3 the bottle, 4 / 5 tougher ones, 6 with a bonus box on hit[3]),
// 0xA..0xF the gallery scenery walls (hp 1000, hits reported as R22cHitEffect). r_no_0 is the
// running instruction (0 begin, 1 end, 2 stay, 3 move, 4 none), r_no_1 / r_no_2 its steps; the
// target pops up by rotating ang.x from PI/2 to 0 and folds back down. EmMarkWork (em3e.h):
// pInst the instruction cursor, age frames since the pop-up, timer, downTimer, hit[] boxes.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "em3e.h"
#include "emhit.h"
#include "em_sub.h"
#include "esp.h"
#include "snd.h"
#include "scroll.h"
#include "pl_wep.h"
#include "player.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include <dolphin/os.h>
#include "em_mod.h"


typedef void (*EmMarkFunc)(cEmMark*);

static void emmark_begin(cEmMark* em);
static void emmark_end(cEmMark* em);
static void emmark_stay(cEmMark* em);
static void emmark_move(cEmMark* em);
static void emmark_none(cEmMark* em);

// EmInitFunc: placement-constructs a target in the cEm work.
void em3eInit(cEm* em)
{
    new (em) cEmMark();
}

// Constructor: targets cannot be locked on.
cEmMark::cEmMark()
{
    setStatus(EM_STATUS_LOCKOFF);
}

// Room entry: init from an EmMarkData record (type, instruction list, integer position).
void cEmMark::init(EmMarkData* d)
{
    init(d->type, d->inst, (f32) d->X, (f32) d->Y, (f32) d->Z);
}

// Builds the target at (x, y, z): the model by type (targets from the module archive, walls
// 0xA..0xF from the room's scroll objects), type 5 six times larger and upside down, a light
// area (1 m for targets, 10 m for walls), hp (1 / 5 / 10 / 1000), the effects (archive 4 as
// group 0x33), the hit cubes (head / chest / body / legs, plus the bonus box of type 6 and the
// wall panels), type 6's bonus glow, and the instruction cursor at `inst` with age 0.
void cEmMark::init(u8 type, EmMarkInst* inst, f32 x, f32 y, f32 z)
{
    static const Vec ofs = { 0.0f, 0.0f, 0.0f };
    void* bin;
    void* tpl;

    pos.x = x;
    pos.y = y;
    pos.z = z;
    this->type = type;
    switch (this->type) {
    case 0:
    default:
        bin = EM_ARC(this, 5);
        tpl = EM_ARC(this, 6);
        break;
    case 1:
        bin = EM_ARC(this, 9);
        tpl = EM_ARC(this, 0xA);
        break;
    case 2:
        bin = EM_ARC(this, 0xD);
        tpl = EM_ARC(this, 0xE);
        break;
    case 3:
        bin = EM_ARC(this, 0x15);
        tpl = EM_ARC(this, 0x16);
        break;
    case 4:
        bin = EM_ARC(this, 5);
        tpl = EM_ARC(this, 6);
        break;
    case 5:
        bin = EM_ARC(this, 5);
        tpl = EM_ARC(this, 6);
        break;
    case 6:
        bin = EM_ARC(this, 0xF);
        tpl = EM_ARC(this, 0x10);
        break;
    case 0xA:
        bin = SmdGetObjPtr(5)->pModelInfo->model_addr;
        tpl = SmdGetObjPtr(5)->pModelInfo->tpl_addr;
        break;
    case 0xB:
        bin = SmdGetObjPtr(6)->pModelInfo->model_addr;
        tpl = SmdGetObjPtr(6)->pModelInfo->tpl_addr;
        break;
    case 0xC:
        bin = SmdGetObjPtr(0x33)->pModelInfo->model_addr;
        tpl = SmdGetObjPtr(0x33)->pModelInfo->tpl_addr;
        break;
    case 0xD:
        bin = SmdGetObjPtr(0x34)->pModelInfo->model_addr;
        tpl = SmdGetObjPtr(0x34)->pModelInfo->tpl_addr;
        break;
    case 0xE:
        bin = SmdGetObjPtr(0x35)->pModelInfo->model_addr;
        tpl = SmdGetObjPtr(0x35)->pModelInfo->tpl_addr;
        break;
    case 0xF:
        bin = SmdGetObjPtr(0x36)->pModelInfo->model_addr;
        tpl = SmdGetObjPtr(0x36)->pModelInfo->tpl_addr;
        break;
    }
    modelInit(bin, tpl);
    if (this->type == 5) {
        scale.x = 6.0f;
        scale.y = 6.0f;
        scale.z = 6.0f;
        pos.y -= 1000.0f;
        pList->ang.x = PI;
        pList->ang.y = 0.0f;
        pList->ang.z = 0.0f;
    }
    {
        Vec size;
        f32 r;
        int lv;

        if (this->type <= 9) {
            r = 1000.0f;
            lv = 2;
        } else {
            r = 10000.0f;
            lv = 0x10;
        }
        size.x = r;
        size.y = r;
        size.z = 0.0f;
        LightInfo.init2(0, 1, &ofs, &size, lv);
    }
    switch (this->type) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 6:
        hp = 1;
        break;
    case 4:
        hp = 5;
        break;
    case 5:
        hp = 10;
        break;
    default:
        hp = 1000;
        break;
    }
    EspDataLoad((u32) EM_ARC(this, 4), EFF_EM3E, 0);
    {
        const f32 depth = 150.0f;

        switch (this->type) {
        case 0:
        case 1:
        case 2:
        case 4:
        case 6:
            YarareInitCube(this, 0.0f, 1800.0f, 0.0f, 180.0f, 400.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[0], 0.0f, 1400.0f, 0.0f, 370.0f, 400.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[1], 0.0f, 1000.0f, 0.0f, 350.0f, 500.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[2], 0.0f, 400.0f, 0.0f, 300.0f, 600.0f, depth, 1, YAT_FLAG_ON);
            if (this->type == 6) {
                YarareAddCube(this, &EMMARK(this)->hit[3], -650.0f, 1200.0f, 0.0f, 200.0f, 1100.0f, 50.0f, 1, YAT_FLAG_ON);
            }
            break;
        case 3:
            YarareInit(this, 0.0f, -500.0f, 0.0f, 300.0f, depth, 1, YAT_FLAG_ON);
            break;
        case 5:
            YarareInit(this, 0.0f, 400.0f, 0.0f, 200.0f, 0.0f, 0, YAT_FLAG_ON);
            break;
        case 0xA:
            YarareInitCube(this, -2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, YAT_FLAG_ON);
            break;
        case 0xB:
            YarareInitCube(this, 2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, YAT_FLAG_ON);
            break;
        case 0xC:
            YarareInitCube(this, -2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[0], -2700.0f, 800.0f, -26800.0f, 1700.0f, 4000.0f, depth, 1, YAT_FLAG_ON);
            break;
        case 0xD:
            YarareInitCube(this, 2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[0], 600.0f, 2800.0f, -26800.0f, 500.0f, 1500.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[1], 3900.0f, 2400.0f, -26800.0f, 600.0f, 1200.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[2], 1300.0f, 700.0f, -26800.0f, 500.0f, 1500.0f, depth, 1, YAT_FLAG_ON);
            break;
        case 0xE:
            YarareInitCube(this, -2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[0], 400.0f, 2200.0f, -26800.0f, 400.0f, 1500.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[1], -2300.0f, 3600.0f, -26800.0f, 2300.0f, 1800.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[2], -600.0f, 3900.0f, -26800.0f, 700.0f, 1400.0f, depth, 1, YAT_FLAG_ON);
            break;
        case 0xF:
            YarareInitCube(this, 2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[0], 1400.0f, 2200.0f, -26800.0f, 1400.0f, 1600.0f, depth, 1, YAT_FLAG_ON);
            YarareAddCube(this, &EMMARK(this)->hit[1], 2300.0f, 3600.0f, -26800.0f, 2300.0f, 1800.0f, depth, 1, YAT_FLAG_ON);
            break;
        }
    }
    if (this->type == 6) {
        EstSet(this, -1, 0, 0, EFF_EM3E, 7, 0, ESP_CORE_KIND_NONE, this, 0);
    }
    EMMARK(this)->pInst = inst;
    EMMARK(this)->age = 0;
}

EmMarkFunc emmark_tbl[6] = {
    emmark_begin,
    emmark_end,
    emmark_stay,
    emmark_move,
    emmark_none,
    0,
};

// Per-frame update (emMove): the hit check, the Ashley target's fold-down check while standing,
// the current instruction, age counts while staying / moving, then the matrix.
void cEmMark::move()
{

    damageCheck();
    if (r_no_0 > 1) {
        downCheck();
    }
    emmark_tbl[r_no_0](this);
    switch (r_no_0) {
    case 2:
    case 3:
        EMMARK(this)->age++;
        break;
    }
    matUpdate();
}

// Number of other live targets (not the Ashley types 2 / 3) that have been up at least `age` frames.
int countOldMark(cEmMark* self, int age)
{
    int n = 0;
    cEm* em;

    for (em = (cEm*) EmMgr.getActiveWork(); em; em = EmMgr.getNext(em)) {
        if (em == self) {
            continue;
        }
        if (!em->isAlive()) {
            continue;
        }
        if (em->id != 0x3E) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        switch (em->type) {
        case 2:
        case 3:
            break;
        default:
            if (EMMARK(em)->age >= age) {
                n++;
            }
            break;
        }
    }
    return n;
}

// The Ashley target (type 2) folds down (setDown) 10 frames after the last older target has gone.
void cEmMark::downCheck()
{

    if (type != 2) {
        return;
    }
    if (countOldMark(this, EMMARK(this)->age)) {
        EMMARK(this)->downTimer = 10;
    } else if (EMMARK(this)->downTimer > 0) {
        EMMARK(this)->downTimer--;
    } else {
        setDown();
    }
}

// Instruction 0 (begin): the pop-up SE (targets only) and the rise from ang.x PI/2 to 0 in 10
// frames, then the next instruction.
static void emmark_begin(cEmMark* em)
{
    int st = em->r_no_1;

    switch (st) {
    case 0:
        if (em->type <= 9) {
            SndCall(6, 0, &em->pos, 0, 0, 0);
        }
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->ang.x = PI / 2.0f;
        break;
    case 1:
        em->r_no_2++;
        em->ang.x -= PI / 20.0f;
        if (em->r_no_2 > 9) {
            em->ang.x = 0.0f;
            em->setNextInstruction();
        }
        break;
    }
}

// Instruction 1 (end, also entered by a killing hit / setDown): an unhit target waits 10 frames
// first; hp = 0, the fold-down SE, the fold from 0 to PI/2 in 10 frames, then the work is destroyed.
static void emmark_end(cEmMark* em)
{
    switch (em->r_no_1) {
    case 0:
        if (em->hp > 0) {
            em->r_no_2 = 10;
            em->r_no_1 = 1;
        } else {
            em->r_no_1 = 10;
        }
        em->hp = 0;
        break;
    case 1:
        em->r_no_2--;
        if (em->r_no_2 == 0) {
            em->r_no_1 = 10;
        }
        break;
    case 0xA:
        SndCall(6, 0, &em->pos, 0, 0, 0);
        em->r_no_2 = 0;
        em->r_no_1 = 0xB;
        break;
    case 0xB:
        em->r_no_2++;
        em->ang.x += PI / 20.0f;
        if (em->r_no_2 > 9) {
            em->r_no_1 = 0xC;
        }
        break;
    case 0xC:
        EmMgr.destroy(em);
        em->r_no_1 = 0xD;
        break;
    case 0xD:
        break;
    }
}

// Instruction 2 (stay): waits inst->count frames (springing back upright after a hit), then the
// next instruction.
static void emmark_stay(cEmMark* em)
{
    EmMarkInst* inst = EMMARK(em)->pInst;

    switch (em->r_no_1) {
    case 0:
        EMMARK(em)->timer = inst->count;
        em->r_no_1 = 1;
        break;
    case 1:
        EMMARK(em)->timer--;
        if (EMMARK(em)->timer <= 0) {
            em->setNextInstruction();
        }
        break;
    }
    em->standSpring();
}

// Instruction 3 (move): slides towards the integer target position at inst->spd units per frame
// (springing upright), snaps onto it and takes the next instruction.
static void emmark_move(cEmMark* em)
{
    EmMarkInst* inst = EMMARK(em)->pInst;
    Vec target;
    Vec dir;

    switch (em->r_no_1) {
    case 0:
        em->r_no_1 = 1;
    case 1:
        target.x = (f32) inst->x;
        target.y = (f32) inst->Y;
        target.z = (f32) inst->Z;
        if (GetDistance(target, em->pos) > (f32) (inst->spd * inst->spd)) {
            PSVECSubtract(&target, &em->pos, &dir);
#line 533 "D:/Bio4/Prog/emmark.cpp"
            VECNormalize(&dir, &dir);
            PSVECScale(&dir, &dir, (f32) inst->spd);
            PSVECAdd(&em->pos, &dir, &em->pos);
        } else {
            em->pos = target;
            em->setNextInstruction();
        }
        break;
    }
    em->standSpring();
}

// Instruction 4 (none): the target holds (the walls).
static void emmark_none(cEmMark* em)
{
}

// Hit of the frame (cEm::dmg.m_Flag set by PlWepHitCheck2): a wall reports its panel (setEffWall);
// a target loses 1 hp; the hit kind is 5 for grenades, 2 for type 6's bonus box, 1 for the head
// box (hitInfo), 0 for the body. Killed: the break effect and R22cHitMark(type, headshot, pos,
// 1, age), then the end instruction; still alive: a hit effect, R22cHitMark(..., 3 / 2, ..., 0,
// age) and the target tilts back PI/7.
void cEmMark::damageCheck()
{
    int eff;

    if (hp > 0 && dmg.m_Flag) {
        if (type > 9) {
            setEffWall();
            dmg.clear();
            return;
        }
        hp--;
        if (dmg.m_Wep == 0x12 || dmg.m_Wep == 0x13) {
            eff = 5;
        } else if (type == 6 && dmg.m_pDamageYarare == &EMMARK(this)->hit[3]) {
            eff = 2;
        } else {
            eff = dmg.m_pDamageYarare == &hitInfo;
        }
        if (hp <= 0) {
            setEff(1, eff);
            R22cHitMark(type, eff != 0, &dmg.m_pDamageYarare->cross, 1, EMMARK(this)->age);
            r_no_0 = 1;
            r_no_1 = 0;
            r_no_2 = 0;
            r_no_3 = 0;
        } else {
            setEff(0, eff);
            R22cHitMark(type, eff != 0 ? 3 : 2, &dmg.m_pDamageYarare->cross, 0, EMMARK(this)->age);
            ang.x -= PI / 7.0f;
            dmg.clear();
        }
    }
}

// Hit effect: the bottle (3) shatters (effect 8 / 9 by weapon) and vanishes; the Ashley target
// (2) a plain hit effect; kind 2 (the bonus box) swaps in the opened model, effect 6 and a
// grenade-type blast hit check around it; kind 1 (headshot) or a grenade kill blows the head off
// (headBomb); a body hit the plain effect (0, 1 for the shotgun) at the hit point. Returns 1.
int cEmMark::setEff(int a, int kind)
{
    const f32 range = 6000.0f;
    Vec p;

    p = pos;
    if (type != 3) {
        p.y += 1000.0f;
    } else {
        p.y -= 500.0f;
    }
    if (type == 3) {
        int k;

        k = 9;
        if (dmg.m_Wep != 7 && dmg.m_Wep != 0x13) {
            k = 8;
        }
        EstSet(this, -1, &pos, 0, EFF_EM3E, k, 0, ESP_CORE_KIND_NONE, this, 0);
        SndCall(6, 7, &p, 0, 0, 0);
        be_flag &= ~2;
    } else if (type == 2) {
        int k = dmg.m_Wep == 7;

        EstSet(this, -1, &dmg.m_pDamageYarare->cross, 0, EFF_EM3E, k, 0, ESP_CORE_KIND_NONE, this, 0);
        SndCall(6, 1, &p, 0, 0, 0);
    } else if (kind == 2) {
        modelInit(EM_ARC(this, 0x13), EM_ARC(this, 0x14));
        EstSet(this, -1, &pos, 0, EFF_EM3E, 6, 0, ESP_CORE_KIND_NONE, this, 0);
        SndCall(6, 1, &p, 0, 0, 0);
        PlWepHitCheck2(0, &pos, &pos, 0x13, 0, range);
    } else if (kind != 0) {
        headBomb();
    } else if (dmg.m_Wep == 0x13) {
        static const Vec up = { 0.0f, 1500.0f, 0.0f };
        static const Vec down = { 0.0f, -500.0f, 0.0f };

        headBomb();
        if (type != 3) {
            PSVECAdd(&pos, &up, &dmg.m_pDamageYarare->cross);
        } else {
            PSVECAdd(&pos, &down, &dmg.m_pDamageYarare->cross);
        }
    } else {
        int k = dmg.m_Wep == 7;

        EstSet(this, -1, &dmg.m_pDamageYarare->cross, 0, EFF_EM3E, k, 0, ESP_CORE_KIND_NONE, this, 0);
        SndCall(6, 1, &p, 0, 0, 0);
    }
    return 1;
}

// Wall hit: the panel hit boxes of types 0xC..0xF map to the room's R22cHitEffect numbers (0..6);
// any other spot gets the plain hit effect. Returns 1.
int cEmMark::setEffWall()
{

    switch (type) {
    case 0xC:
        if (dmg.m_pDamageYarare == &EMMARK(this)->hit[0]) {
            R22cHitEffect(0);
        } else {
            setEffWallNormal();
        }
        break;
    case 0xD:
        if (dmg.m_pDamageYarare == &EMMARK(this)->hit[0]) {
            R22cHitEffect(1);
        } else if (dmg.m_pDamageYarare == &EMMARK(this)->hit[1]) {
            R22cHitEffect(2);
        } else if (dmg.m_pDamageYarare == &EMMARK(this)->hit[2]) {
            R22cHitEffect(3);
        } else {
            setEffWallNormal();
        }
        break;
    case 0xE:
        if (dmg.m_pDamageYarare == &EMMARK(this)->hit[0]) {
            R22cHitEffect(5);
        } else if (dmg.m_pDamageYarare == &EMMARK(this)->hit[1]) {
            R22cHitEffect(4);
        } else if (dmg.m_pDamageYarare == &EMMARK(this)->hit[2]) {
            R22cHitEffect(6);
        } else {
            setEffWallNormal();
        }
        break;
    case 0xF:
        if (dmg.m_pDamageYarare == &EMMARK(this)->hit[0]) {
            R22cHitEffect(5);
        } else if (dmg.m_pDamageYarare == &EMMARK(this)->hit[1]) {
            R22cHitEffect(4);
        } else {
            setEffWallNormal();
        }
        break;
    }
    return 1;
}

// Plain wall hit effect (0, 1 for the shotgun) at the hit point. Returns 1.
int cEmMark::setEffWallNormal()
{
    Vec p;

    int k = dmg.m_Wep == 7;

    EstSet(this, -1, &dmg.m_pDamageYarare->cross, 0, EFF_EM3E, k, 0, ESP_CORE_KIND_NONE, this, 0);
    p.x = pos.x;
    p.y = pos.y + 1000.0f;
    p.z = pos.z;
    return 1;
}

// Headshot: swaps in the headless model of the type and plays the head-burst effect (kind by
// type; the shotgun / grenade variants 4 / 5 / 9) with its SE.
void cEmMark::headBomb()
{
    void* bin;
    void* tpl;
    int kind;

    switch (dmg.m_Wep) {
    case 2:
    case 0xB:
    case 0xC:
    default:
        switch (type) {
        case 0:
        default:
            kind = 2;
            bin = EM_ARC(this, 7);
            tpl = EM_ARC(this, 8);
            break;
        case 1:
            kind = 3;
            bin = EM_ARC(this, 0xB);
            tpl = EM_ARC(this, 0xC);
            break;
        case 2:
        case 3:
            kind = 8;
            bin = EM_ARC(this, 7);
            tpl = EM_ARC(this, 8);
            break;
        case 4:
            kind = 3;
            bin = EM_ARC(this, 7);
            tpl = EM_ARC(this, 8);
            break;
        case 5:
            kind = 3;
            bin = EM_ARC(this, 7);
            tpl = EM_ARC(this, 8);
            break;
        case 6:
            kind = 2;
            bin = EM_ARC(this, 0x11);
            tpl = EM_ARC(this, 0x12);
            break;
        }
        break;
    case 7:
    case 0x13:
        switch (type) {
        case 0:
        default:
            kind = 4;
            bin = EM_ARC(this, 7);
            tpl = EM_ARC(this, 8);
            break;
        case 1:
            kind = 5;
            bin = EM_ARC(this, 0xB);
            tpl = EM_ARC(this, 0xC);
            break;
        case 2:
        case 3:
            kind = 9;
            bin = EM_ARC(this, 7);
            tpl = EM_ARC(this, 8);
            break;
        case 4:
            kind = 5;
            bin = EM_ARC(this, 7);
            tpl = EM_ARC(this, 8);
            break;
        case 5:
            kind = 5;
            bin = EM_ARC(this, 7);
            tpl = EM_ARC(this, 8);
            break;
        case 6:
            kind = 4;
            bin = EM_ARC(this, 0x11);
            tpl = EM_ARC(this, 0x12);
            break;
        }
        break;
    }
    modelInit(bin, tpl);
    EstSet(this, -1, &pos, 0, EFF_EM3E, kind, 0, ESP_CORE_KIND_NONE, this, 0);
    SndCall(6, 2, &pos, 0, 0, 0);
}

// Advances the instruction cursor past the current record (begin / end 4 bytes, stay 8, move
// 0x14, none 0) and starts the new one: r_no_0 = its type byte, steps zero.
void cEmMark::setNextInstruction()
{
    int size;

    switch (EMMARK(this)->pInst->type) {
    case 0:
        size = 4;
        break;
    case 1:
        size = 4;
        break;
    case 2:
        size = 8;
        break;
    case 3:
        size = 0x14;
        break;
    case 4:
        size = 0;
        break;
    default:
        pLog->err(0, 0, "cEmMark::setNextInst() ERR %d", EMMARK(this)->pInst->type);
        return;
    }
    EMMARK(this)->pInst = (EmMarkInst*) ((u8*) EMMARK(this)->pInst + size);
    r_no_0 = ((u8*) EMMARK(this)->pInst)[3];
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Springs a hit target back upright: ang.x (tilted negative by a hit) returns to 0 by PI/20 per frame.
void cEmMark::standSpring()
{
    const f32 step = PI / 20.0f;

    if (ang.x < 0.0f) {
        if (ang.x < -step) {
            ang.x += step;
        } else {
            ang.x = 0.0f;
        }
    }
}

// Folds a live, unhit target down: the end instruction (also called by the room).
void cEmMark::setDown()
{
    if (hp <= 0) {
        return;
    }
    if (dmg.m_Flag != 0) {
        return;
    }
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// REL entry: registers the target constructor.
extern "C" void _prolog()
{
    OSReport("emMark prolog Ok\n");
    EmInitFunc = em3eInit;
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
