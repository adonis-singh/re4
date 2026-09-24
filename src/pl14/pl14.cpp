// pl14 module (D:/Bio4/Prog/pl14.cpp): Luis, the partner of the cabin fight (room 11C). See pl14.h.
// Matching notes: the byte flag fields are s8 (their bit clears compile to full-width rlwinm masks);
// `default:` comes first in most switches; two-case switches whose tree tests 1 before 0 carry an
// empty `case 2:`; cAnalysis::move's scan is a while loop with the scan in its condition.
//
// Luis is the enemy work of id 3 (pSUB) during the cabin siege: LuisInit is the module's
// EmInitFunc. Every frame cSubLuis::move runs damageCheck (the player's / enemies' hits ->
// flags bit0), cAnalysis::move (nearest shootable enemy, route distance to the player, is the
// player aiming a gun / grenade at him), think (picks the cAction mode), cAction::move (the mode's
// step machine, which requests cRoutine routines) and cRoutine::move (the motion routine of
// r_no_0: footwork, damage, die, walk, run, turn, weapon ready / set / fire / down, throw the item,
// down / avoid / up / blast). His gun is a cObjLuisItem hung on his right hand; the thrown item
// (routine 0xD) is another cObjLuisItem that lands as a pickup. Lines he speaks go through cVoice
// (SE + subtitle). Room 11C positions are hard-coded (the stairs, the upper floor, the rack escape).

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "pl14.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "pl_mod.h"
#include "pl_cloth.h"
#include "global.h"
#include "motion.h"
#include "esp.h"
#include "est.h"
#include "em_sub.h"
#include "emhit.h"
#include "at_mod.h"
#include "dmg.h"
#include "etc_model.h"
#include "route_ck.h"
#include "sce_at.h"
#include "snd.h"
#include "mes.h"
#include "rnd.h"
#include "math_sub.h"
#include "db_log.h"
#include <signal.h>
#include <dolphin/os.h>
#include "wep_mod.h"


#line 1 "D:/Bio4/Prog/pl14.cpp"

#define OARC(no) PL_ARC_PTR(owner->subArc, no)
#define EM ((cEm*) this)
#define OEM ((cEm*) owner)

static inline void RoutineSet(cSubLuis* o, int r0)
{
    o->r_no_0 = r0;
    o->r_no_1 = 0;
    o->r_no_2 = 0;
    o->r_no_3 = 0;
}

static inline void RoutineStepClear(cSubLuis* o)
{
    o->r_no_3 = 0;
    o->r_no_2 = 0;
    o->r_no_1 = 0;
}

// LuisInit is public and defined before the first initialised public object: the static initializer's
// key (`global constructors keyed to LuisInit`) is the first public function/initialised object assembled.
// EmInitFunc: placement-constructs Luis in the cEm work, builds his models, inits the work and
// creates his gun object.
void LuisInit(cEm* em)
{
    cSubLuis* luis = new (em) cSubLuis();
    luis->modelSet();
    luis->init();
    luis->equipWeapon();
}

// Routine handlers by routine number (owner->r_no_0); 4 (event) calls cSubLuis::evFunc instead.
void (cRoutine::*cRoutine_move_tbl[18])() = {
    &cRoutine::moveFootwork,
    &cRoutine::moveDamage,
    &cRoutine::moveDie,
    &cRoutine::moveEvent,
    0,
    &cRoutine::moveWalk,
    &cRoutine::moveRun,
    &cRoutine::moveTurn,
    &cRoutine::moveTurn180,
    &cRoutine::moveWepReady,
    &cRoutine::moveWepSet,
    &cRoutine::moveWepFire,
    &cRoutine::moveWepDown,
    &cRoutine::moveThrowItem,
    &cRoutine::moveDown,
    &cRoutine::moveAvoid,
    &cRoutine::moveUp,
    &cRoutine::moveBlast,
};

static int luisBlink = 0;        // frames to the next eye shift
static int luisUnused = 0;       // never read (the second .data word)

static cDelayF luisEye;          // eye yaw: current, target, delay

// Constructor: the three machines start (routine 0 footwork, action mode 2 chase the player), the
// players' foot shadow table, registers himself as pSUB, and the eye yaw blend (luisEye) is reset
// with mix 0.4.
cSubLuis::cSubLuis()
{
    routine.init(this);
    action.status.reset();
    action.init(this);
    analysis.status.reset();
    analysis.init(this);
    status.reset();
    pFsdTbl = pl_fs_tbl;
    pEm = this;
    pSUB = (cSubChar*) this;
    luisEye.reset(0.0f);
    luisEye.setDelay(0.4f);
}

// Destructor (killEm / room change): destroys the gun object and clears pSUB.
cSubLuis::~cSubLuis()
{
    ObjMgr.destroy(pWep);
    pSUB = 0;   // the inlined ~cUnit's be_flag load stays below the store
}

// The light info origin both models use (one static: the inline is expanded where it is defined).
// A helper that RETURNS the address keeps the init2 argument order (`&zero` evaluated before `&size`);
// a helper taking `size` by pointer evaluates the parameter copy first (embarrel idiom).
static inline const Vec* LuisLightZero()
{
    static const Vec zero = { 0.0f, 0.0f, 0.0f };
    return &zero;
}

// Work init (LuisInit): light area, a 300 x 900 collision cylinder, lock-on point on parts 4, not
// lockable (EM_STATUS_LOCKOFF), hp 1200, 5 hits from the player before he goes down (m_PlAtack),
// be_flag bit25, his effects (archive 0x34 as group 7), the hair cloth, the hit boxes (torso,
// head, legs, arms; YarareInit / YarareAdd), the idle motion 0x40 and the three racks of the room.
void cSubLuis::init()
{
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    TevScaleGroup = 1;
    LightInfo.init2(0, 1, LuisLightZero(), &p1, 0x40);
    // COMPILER-DIFF: #1 (FPR argument moves before the int `li`s)
    atari.init(0.0f, -200.0f, 0.0f, 300.0f, 200.0f, 400.0f, 900.0f, 1, 0x1000, 10);
    {
        cSubLuis* s = pEm;
        s->setTarget(4, 0.0f, 0.0f, 0.0f);
    }
    setStatus(EM_STATUS_LOCKOFF);
    hp = hp_max = 0x4B0;
    m_PlAtack = 5;
    be_flag |= 0x2000000;
    m_LeonHp = pG->pl_life;   // struct view: the pG load does not wait for the dmgCnt byte store
    m_okTime = 0;
    thankCtr = 0;
    set = 0;
    EspDataLoad((u32) SUB_ARC(this, 0x34 / 4), EFF_PL04, 0);
    PlClothSetLuis(this, &luisHair);
    YarareInit(EM, 0.0f, -30.0f, 0.0f, 150.0f, 100.0f, 2, YAT_FLAG_ON);
    YarareAdd(EM, &m_Yarare[0], 0.0f, 0.0f, 0.0f, 170.0f, 120.0f, 3, YAT_FLAG_ON);
    YarareAdd(EM, &m_Yarare[1], -20.0f, -300.0f, 0.0f, 120.0f, 300.0f, 0x13, YAT_FLAG_ON);
    YarareAdd(EM, &m_Yarare[2], 20.0f, -300.0f, 0.0f, 120.0f, 300.0f, 0x17, YAT_FLAG_ON);
    YarareAdd(EM, &m_Yarare[3], 0.0f, 0.0f, 20.0f, 140.0f, 65.0f, 5, YAT_FLAG_ON);
    YarareAdd(EM, &m_Yarare[4], -20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x14, YAT_FLAG_ON);
    YarareAdd(EM, &m_Yarare[5], 20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x18, YAT_FLAG_ON);
    YarareAdd(EM, &m_Yarare[6], -350.0f, 0.0f, 0.0f, 100.0f, 350.0f, 9, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(EM, &m_Yarare[7], 0.0f, 0.0f, 0.0f, 100.0f, 350.0f, 0xF, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(EM, &m_Yarare[8], -180.0f, 0.0f, 0.0f, 120.0f, 180.0f, 8, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(EM, &m_Yarare[9], 0.0f, 0.0f, 0.0f, 120.0f, 180.0f, 0xE, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    MotionSetCore(pEm, &pEm->Motion, SUB_ARC(this, 0x40 / 4), 0, 0, 5, 0);
    motionMove();
    getRoomEtcRack(0, &pRackWk[0], 1);
    getRoomEtcRack(1, &pRackWk[1], 1);
    getRoomEtcRack(2, &pRackWk[2], 1);
}

// Builds the model set from the partner archive: the body (0x10/0x14) as the base model, the hair
// (0x18/0x1C), the face (0x20/0x1C, pFace) and three more parts (0x24, 0x28, 0x2C with the body texture).
void cSubLuis::modelSet()
{
    cModelInfo* info;

    if (!modelInit(SUB_ARC(this, 0x10 / 4), SUB_ARC(this, 0x14 / 4))) {
        pLog->err(0, 0, "cSubLuis::init() failed.");
    }
    info = ModInfoMgr.create(SUB_ARC(this, 0x18 / 4), SUB_ARC(this, 0x1C / 4));
    if (info) addModel(info);
    pFace = ModInfoMgr.create(SUB_ARC(this, 0x20 / 4), SUB_ARC(this, 0x1C / 4));
    if (pFace) addModel(pFace);
    if (0) pLog->err(0, 0, "setFace() FAILED. %d", 0);
    info = ModInfoMgr.create(SUB_ARC(this, 0x24 / 4), SUB_ARC(this, 0x14 / 4));
    if (info) addModel(info);
    info = ModInfoMgr.create(SUB_ARC(this, 0x28 / 4), SUB_ARC(this, 0x14 / 4));
    if (info) addModel(info);
    info = ModInfoMgr.create(SUB_ARC(this, 0x2C / 4), SUB_ARC(this, 0x14 / 4));
    if (info) addModel(info);
}

// Per-frame update (emMove): clears Status_flg[1] bit16 / [2] bit29 (partner request bits), then
// damage -> analysis -> think -> action -> routine, the eyes and neck, the parts matrices, the
// hair cloth, the enemy / scenery collision (EmAtCheck, SatMgr.check, atari.move) and the motion
// key sounds.
void cSubLuis::move()
{
    StaFlagOff(pG, STA_TAKEAWAY);
    StaFlagOff(pG, STA_SUB_CATCHED);
    damageCheck();
    analysis.move();
    think();
    action.move(&analysis, &routine);
    routine.move();
    moveEye();
    neckMove();
    partsWorldCalc();
    PlClothMoveLuis(this, &luisHair);
    EmAtCheck(EM);
    SatMgr.check(this, 0);
    atari.move();
    PartsWorldPosCalc(this);
    seqSeCtrl();
}

// Picks the action mode of the frame (nothing while in damage / die). flags bit0 (hit) -> damage
// (5) or die (6, when the event routine was running); else in priority order: a grenade aimed at
// him (analysis bit4) -> avoid (0xA); down and no longer aimed at -> get up (9); flags bit1 (5
// player hits) -> attack the player (4); aimed at by the player -> down (8); upstairs (set 2)
// and not yet there -> go upstairs (3); ground floor with a rack coming -> escape it (0xC), or
// the room 11C opening (0xB); a target -> attack (1), or every 1800 frames with the player on the
// same floor the item gift (7); nothing -> chase the player (2). Also the worry line when the
// player's life changed (every 90 frames) and the reaction line after enemy damage (cnt).
void cSubLuis::think()
{
    static const Vec upPos = { 112160.0f, 3182.64f, -51016.84f };

    {
        cAction* a = &action;
        if (a->type == 5) return;
        if (a->type == 6) return;
    }

    if (status.check(F_DAMAGED)) {
        if (r_no_0 == 4) action.set(6);
        else action.set(5);
        status.off(F_DAMAGED);
        analysis.status.off(cAnalysis::S_PL_DOWN);
    } else {
        if (analysis.status.check(cAnalysis::S_GRENADE)) {
            action.set(0xA);
        } else if (!analysis.status.check(cAnalysis::S_PL_AIM) && analysis.status.check(cAnalysis::S_PL_DOWN)) {
            action.set(9);
        } else if (status.check(F_PL_ATTACKED)) {
            action.set(4);
        } else if (analysis.status.check(cAnalysis::S_PL_AIM)) {
            action.set(8);
        } else if (set == 2 && !status.check(F_2F)) {
            action.set(3);
            if (GetDistance(*(Vec*) &upPos, pos) < 1000000.0f) status.on(F_2F);
        } else if (set == 1 && (rackCheck() || analysis.status.check(cAnalysis::S_ESC_RACK))) {
            action.set(0xC);
        } else if (set == 1 && !action.status.check(cAction::S_11C_BEGIN)) {
            action.set(0xB);
        } else if (analysis.pEmNear) {
            if (analysis.status.check(cAnalysis::S_GIVE_ITEM) && !stairCheck(pPL) && !stairCheck(this) && sameFloorCheck(this, pPL) &&
                (s16) pG->pl_life > 0) {
                action.set(7);
                analysis.status.off(cAnalysis::S_GIVE_ITEM);
            } else {
                action.set(1);
            }
        } else {
            action.set(2);
        }
    }

    if ((s16) pG->pl_life != m_LeonHp && (s16) pG->pl_life > 0 && sameFloorCheck(this, pPL)) {
        if (m_okTime == 0) {
            m_LeonHp = pG->pl_life;
            routine.voice.set(0x5A, 0x10, 60);
            m_okTime = 0x5A;
        }
    }
    if (m_okTime) m_okTime--;

    if (thankCtr == 1) {
        if (Rnd() & 0x30) routine.voice.set(0x5B, 7, 60);
        else routine.voice.set(0x5C, 8, 60);
    }
    if (thankCtr) thankCtr--;
}

// The first rack is alive, pushed past z -49000 and Luis is within 3 m of the rack spot: sets
// analysis bit7 (escape the rack) and returns 1.
int cSubLuis::rackCheck()
{
    static const Vec rackPos = { 107455.0f, 4.0f, -47540.0f };

    // Pool order (dist before zlim) and, with zlim a const local, the pos.z load is issued before the
    // pool `lis` (the rack pointer's r9 is then reused for the high half, pos.z lands in f0).
    const f32 dist = 9000000.0f;
    const f32 zlim = -49000.0f;

    if (pRackWk[0]->hp <= 0) return 0;
    if (pRackWk[0]->pos.z < zlim) return 0;
    if (GetDistance(&pos, (Vec*) &rackPos) > dist) return 0;
    analysis.status.on(cAnalysis::S_ESC_RACK);
    return 1;
}

// Routine machine init: routine bytes zero, no saved routines, no burst, routine 0 (footwork).
void cRoutine::init(cSubLuis* o)
{
    owner = o;
    o->r_no_3 = 0;
    o->r_no_2 = 0;
    o->r_no_1 = 0;
    o->r_no_0 = 0;
    intStack[2] = 0xFF;
    intStack[1] = 0xFF;
    intStack[0] = 0xFF;
    shotCnt = 0;
    set(0);
}

// Runs the routine of r_no_0 (4 = the scenario's event function m_pFunc) and the voice timer.
int cRoutine::move()
{
    if (owner->r_no_0 == 4) {
        owner->m_pFunc();
    } else {
        (this->*cRoutine_move_tbl[owner->r_no_0])();
    }
    voice.move();
    return 1;
}

// Routine 0: the idle motion 0x40.
void cRoutine::moveFootwork()
{
    if (owner->r_no_1 == 0) {
        owner->motionSet(OARC(0x40 / 4), 5, 0, 5, 0);
        owner->r_no_1 = 1;
    }
    owner->motionMove();
}

// Routine 1: the damage reaction of work[0] (0..5 flinches 0xB4..0xC8, 6/7 knock-downs 0xCC /
// 0xD4 that stay down (step 0xA), 8 the grenade blast 0xDC followed by the get-up 0xD8); the
// complaint line of work[1] (hits left) at frame 20 when the player is on his floor; the damage
// info is cleared at the motion's end and the routine ends two steps later.
void cRoutine::moveDamage()
{
    void* mot = 0;

    switch (owner->r_no_1) {
    case 0:
        switch (work[0]) {
        case 0: mot = OARC(0xB4 / 4); owner->r_no_1 = 1; break;
        case 1: mot = OARC(0xB8 / 4); owner->r_no_1 = 1; break;
        case 2: mot = OARC(0xBC / 4); owner->r_no_1 = 1; break;
        case 3: mot = OARC(0xC0 / 4); owner->r_no_1 = 1; break;
        case 4: mot = OARC(0xC4 / 4); owner->r_no_1 = 1; break;
        case 5: mot = OARC(0xC8 / 4); owner->r_no_1 = 1; break;
        case 6: mot = OARC(0xCC / 4); owner->r_no_1 = 0xA; break;
        case 7: mot = OARC(0xD4 / 4); owner->r_no_1 = 0xA; break;
        case 8: mot = OARC(0xDC / 4); owner->r_no_1 = 0x14; break;
        }
        MotionSetCore(owner, &owner->Motion, mot, 0, 3, 1, 0);
        SndCall(8, 9, &owner->pList->world, owner->id, 0, 0);
        owner->thankCtr = 0;
    case 1:
        if (MotionCheckCrossFrame(&owner->Motion, 20.0f) && work[1] && sameFloorCheck(owner, pPL)) {
            switch (work[1]) {
            case 4: voice.set(0x5D, 3, 60); break;
            case 3: voice.set(0x5E, 4, 60); break;
            case 2: voice.set(0x5F, 5, 60); break;
            case 1: voice.set(0x60, 6, 60); break;
            }
            work[1] = 0;
        }
        if (owner->motionMove()) {
            owner->dmg.clear();
            owner->r_no_1 = 0x32;
        }
        break;
    case 0xA:
        MotionMove(owner, 0);
        break;
    case 0x14:
        if (MotionMove(owner, 0)) owner->r_no_1 = 0x15;
        break;
    case 0x15:
        MotionSetCore(owner, &owner->Motion, OARC(0xD8 / 4), 0, 3, 1, 0);
        owner->r_no_1 = 0x16;
    case 0x16:
        if (MotionMove(owner, 0)) {
            owner->dmg.clear();
            owner->r_no_1 = 0x32;
        }
        break;
    case 0x32:
        owner->r_no_1 = 0x33;
        break;
    case 0x33:
        end();
        break;
    }
}

// Routine 2: death (only through the scenario, damageCheck stat 0x0400xxxx): the fall 0xCC/0xD0
// with the death SE, dmg.m_Timer bit7 (dead), the collision moved to parts 4; the motion then holds.
void cRoutine::moveDie()
{
    switch (owner->r_no_1) {
    case 0:
        MotionSetCore(owner, &owner->Motion, OARC(0xCC / 4), OARC(0xD0 / 4), 5, 1, 0);
        SndCall(1, 0xD, &owner->getPartsPtr(4)->world, owner->id, 0, 0);
        owner->dmg.m_Timer |= 0x80;
        owner->atari.m_parts_no = 4;
        owner->r_no_1 = 1;
        break;
    case 1:
        if (owner->motionMove()) owner->r_no_1 = 2;
        break;
    case 2:
        owner->motionMove();
        break;
    }
}

// Routine 3 (event placeholder): nothing; the real event routine is 4 (m_pFunc).
void cRoutine::moveEvent()
{
}

// Routine 5: walk (motion 0x58) along the route network towards `target` (turning 0.209 rad per
// frame); ends within `dist` of it.
void cRoutine::moveWalk()
{
    Vec out;

    RouteCkToPos(OEM, &pos, &out, 0, 0);
    if (owner->r_no_1 == 0) {
        owner->motionSet(OARC(0x58 / 4), 5, 0, 5, 0);
        owner->r_no_1 = 1;
    }
    owner->ang.y += Muku(&owner->pos, &out, owner->ang.y, 0.20943952f);
    owner->motionMove();
    if (GetDistance(&owner->pos, &pos) < dist * dist) end();
}

// Routine 6: run (motion 0x68) along the route network towards `target`; ends within `dist` of it
// once the route check reports the last leg (r == 1).
void cRoutine::moveRun()
{
    Vec out;
    int r;

    r = RouteCkToPos(OEM, &pos, &out, 0, 0);
    if (owner->r_no_1 == 0) {
        owner->motionSet(OARC(0x68 / 4), 5, 0, 5, 0);
        owner->r_no_1 = 1;
    }
    owner->ang.y += Muku(&owner->pos, &out, owner->ang.y, 0.20943952f);
    owner->motionMove();
    if (GetDistance(&owner->pos, &pos) < dist * dist && r == 1) end();
}

// Routine 9: draw the gun (motion 0x78) while turning to pTarget (0.449 rad per frame); at the
// end -> routine 0xA (aim).
void cRoutine::moveWepReady()
{
    switch (owner->r_no_1) {
    case 0:
        owner->motionSet(OARC(0x78 / 4), 10, 0, 1, 0);
        owner->r_no_1 = 1;
    case 1:
        if (pTarget) owner->ang.y += Muku(&owner->pos, &pTarget->pos, owner->ang.y, 0.44879895f);
        if (owner->motionMove()) {
            end();
            set(0xA);
        }
        break;
    }
}

// Routine 0xA: aim at pTarget: the three-way aim idle (0x7C level, 0x94 / 0x98 up / down) with
// the mot3 rate = the elevation to the target's head, turning 0.209 rad per frame; without a
// valid target -> routine 0xC (lower the gun). Ends at once (the action decides the next step).
void cRoutine::moveWepSet()
{
    Vec d;

    switch (owner->r_no_1) {
    case 0:
        mot3.set(owner, OARC(0x7C / 4), OARC(0x94 / 4), OARC(0x98 / 4), 0, 3, 0, 4, 0);
        if (VALID_PTR(pTarget) && VALID_PTR(pTarget->pList)) {
            PSVECSubtract(&pTarget->pList->world, &owner->pList->world, &d);
            rate = VecElevation(&d);
        } else {
            owner->motionMove();
            RoutineSet(owner, 0xC);
            break;
        }
        owner->r_no_1 = 1;
        end();
    case 1:
        mot3.move(rate);
        if (VALID_PTR(pTarget)) owner->ang.y += Muku(&owner->pos, &pTarget->pos, owner->ang.y, 0.20943952f);
        break;
    }
    owner->motionMove();
}

// Routine 0xB: shoot at pTarget. Steps 0/1 aim (0x7C/0x94/0x98 on the elevation) and turn until
// within 0.196 rad; step 2: a target no longer valid ends the burst (a kill after 10 / 30 dead
// targets gets a line) -> routine 0xB restarts; else up to 10 shots per burst (fire motions
// 0xF4/0xF8/0xFC + shot()), the 11th is the reload 0x100; step 3 plays the motion and re-enters
// 0xB (the three arms are identical: a wall between them or a live target both re-aim).
void cRoutine::moveWepFire()
{
    Vec d;
    f32 a;
    const f32 lim = 0.19634955f;   // pool order: the fabsf limit precedes Muku's PI/8

    switch (owner->r_no_1) {
    case 0:
        if (pTarget == 0) {
            end();
            break;
        }
        PSVECSubtract(&pTarget->pList->world, &owner->pList->world, &d);
        rate = VecElevation(&d);
        mot3.set(owner, OARC(0x7C / 4), OARC(0x94 / 4), OARC(0x98 / 4), 0, 3, 0, 4, 0);
        owner->r_no_1 = 1;
    case 1:
        mot3.move(rate);
        if (pTarget == 0) {
            end();
            break;
        }
        a = Muku(&owner->pos, &pTarget->pos, owner->ang.y, 0.3926991f);
        owner->ang.y += a;
        owner->motionMove();
        if (fabsf(a) < lim) owner->r_no_1 = 2;
        break;
    case 2:
        if (!isTarget(owner, pTarget)) {
            if (pTarget && pTarget->hp <= 0) {
                m_ShootDown++;
                switch (m_ShootDown) {
                case 10: voice.set(0x62, 0xE, 60); break;
                case 30: voice.set(0x63, 0xF, 60); break;
                }
            }
            pTarget = 0;
            owner->motionMove();
            RoutineSet(owner, 0xB);
            break;
        }
        shotCnt++;
        if (shotCnt <= 10) {
            mot3.set(owner, OARC(0xF4 / 4), OARC(0xF8 / 4), OARC(0xFC / 4), 0, 3, 0, 4, 0);
            shot();
        } else {
            owner->motionSet(OARC(0x100 / 4), 10, 0, 1, 0);
            shotCnt = 0;
        }
        owner->r_no_1 = 3;
    case 3:
        if (shotCnt) mot3.move(rate);
        if (owner->motionMove()) {
            if (SatMgr.hitCheck(&owner->pList->world, &pTarget->pList->world, 0, 0, 0, 0)) {
                RoutineSet(owner, 0xB);
            } else if (pTarget->hp > 0) {
                RoutineSet(owner, 0xB);
            } else {
                RoutineSet(owner, 0xB);
            }
        }
        break;
    }
}

// Routine 0xC: lower the gun (motion 0x88), then end.
void cRoutine::moveWepDown()
{
    switch (owner->r_no_1) {
    case 0:
        owner->motionSet(OARC(0x88 / 4), 10, 0, 1, 0);
        owner->r_no_1 = 1;
    case 1:
        if (owner->motionMove()) end();
        break;
    case 2:
        break;
    }
}

// Routine 0xD: throw an item to the player: motion 0x124 with the "catch" line, turning to the
// player for the first 30 frames, the item object created at frame 18 (setItem); ends with the motion.
void cRoutine::moveThrowItem()
{
    switch (owner->r_no_1) {
    case 0:
        owner->motionSet(OARC(0x124 / 4), 10, 0, 1, 0);
        voice.set(0x59, 1, 60);
        owner->r_no_1 = 1;
    case 1:
        if (MotionCheckCrossFrame(&owner->Motion, 18.0f)) setItem();
        if (owner->Motion.Seq_frame <= 30.0f) {
            owner->ang.y += Muku(&owner->pos, &pPL->pos, owner->ang.y, 0.31415927f);
        }
        if (owner->motionMove()) end();
        break;
    }
}

// Creates the thrown item (ObjMgr id 0x1E) at his right hand, flying towards the player.
void cRoutine::setItem()
{
    cObjLuisItem* item = (cObjLuisItem*) ObjMgr.create(cObjMgr::ID_LUIS_ITEM);
    item->init(&owner->getPartsPtr(10)->world, owner->ang.y);
}

// Routine 0xE: duck (the player aims at him): the crouch 0x118, then the crouched idle blend
// 0x11C / 0x128 / 0x12C whose rate turns his upper body towards the player (+-PI/2 mapped to
// -1..1); the routine ends when the crouch is reached and holds until the action changes.
void cRoutine::moveDown()
{
    f32 a;

    switch (owner->r_no_1) {
    case 0:
        owner->motionSet(OARC(0x118 / 4), 10, 0, 1, 0);
        owner->r_no_1 = 1;
    case 1:
        if (owner->motionMove()) {
            mot3.set(owner, OARC(0x11C / 4), OARC(0x128 / 4), OARC(0x12C / 4), 0, 3, 1, 4, 0);
            rate = 0.0f;
            owner->r_no_1 = 2;
            end();
        }
        break;
    case 2:
        rate = rate * (PI / 2);   // reference store: the pPL load stays below it
        a = Muku(&owner->pos, &pPL->pos, owner->ang.y - rate, 0.31415927f);
        rate = (rate - a) / (PI / 2);
        if (rate > 1.0f) rate = 1.0f;
        else if (rate < -1.0f) rate = -1.0f;
        mot3.move(rate);
        owner->motionMove();
        break;
    }
}

// Routine 0x10: stand up from the crouch (motion 0x120), then end.
void cRoutine::moveUp()
{
    switch (owner->r_no_1) {
    case 0:
        owner->motionSet(OARC(0x120 / 4), 10, 0, 1, 0);
        owner->r_no_1 = 1;
    case 1:
        if (owner->motionMove()) end();
        break;
    }
}

// Routine 0x11: the blast knock-back motion 0x130 (never requested by the action machine), then end.
void cRoutine::moveBlast()
{
    switch (owner->r_no_1) {
    case 0:
        owner->motionSet(OARC(0x130 / 4), 10, 0, 1, 0);
        owner->r_no_1 = 1;
    case 1:
        if (owner->motionMove()) end();
        break;
    }
}

// Routine 0xF: dodge a grenade: within 3 m of the player he turns his back to the player's facing,
// else towards the player; the dive 0x114 with his own damage info armed (dmg.set 0x80); the
// damage info is cleared and the routine ends with the motion.
void cRoutine::moveAvoid()
{
    switch (owner->r_no_1) {
    case 0:
        if (GetDistance(&pPL->pos, &pSUB->pos) < 9000000.0f) {
            owner->ang.y = LIMIT_ANGLE(pPL->ang.y + PI);
        } else {
            owner->ang.y += Muku(&owner->pos, &pPL->pos, owner->ang.y, 2 * PI);
        }
        owner->motionSet(OARC(0x114 / 4), 10, 0, 1, 0);
        owner->dmg.set(0, 0x80);
        owner->r_no_1 = 1;
    case 1:
        if (owner->motionMove()) {
            owner->dmg.clear();
            end();
        }
        break;
    }
}

// Routine 7: a turn-in-place motion (work[0]: 0 left 0x48, 1 right 0x50) held for work[1] frames.
void cRoutine::moveTurn()
{
    void* mot;

    if (owner->r_no_1 == 0) {
        if (work[0]) mot = OARC(0x50 / 4);
        else mot = OARC(0x48 / 4);
        owner->motionSet(mot, 5, 0, 5, 0);
        owner->r_no_1 = 1;
    }
    owner->motionMove();
    if (work[1]) {
        work[1]--;
        if (work[1] == 0) end();
    }
}

// Routine 8: the 180-degree turn motion 0x70, ends with it.
void cRoutine::moveTurn180()
{
    if (owner->r_no_1 == 0) {
        owner->motionSet(OARC(0x70 / 4), 5, 0, 5, 0);
        owner->r_no_1 = 1;
    }
    if (owner->motionMove()) end();
}

// Starts routine `no` when its priority allows it (1) or refuses (0).
// Priorities: 0 for footwork / walk / run / turns / weapon ready-set-down / down / up, 1 for fire /
// throw / avoid / blast (actions), 2 for damage. A higher priority interrupts (the interrupted
// routine number is remembered in saved[]), an equal one replaces; the step bytes are cleared and
// the ended flag reset.
int cRoutine::set(int no)
{
    int p;
    int r;
    int ret;

    switch (no) {
    default:
        pLog->err(0, 0, "LUIS: unknnown routine was set %d", no);
        return 0;
    case 0: p = 0; r = 0; break;
    case 1: p = 2; r = 1; break;
    case 5: p = 0; r = 5; break;
    case 6: p = 0; r = 6; break;
    case 7: p = 0; r = 7; break;
    case 8: p = 0; r = 8; break;
    case 9: p = 0; r = 9; break;
    case 0xA: p = 0; r = 0xA; break;
    case 0xB: p = 1; r = 0xB; break;
    case 0xC: p = 0; r = 0xC; break;
    case 0xD: p = 1; r = 0xD; break;
    case 0xE: p = 1; r = 0xE; break;
    case 0xF: p = 2; r = 0xF; break;
    case 0x10: p = 1; r = 0x10; break;
    }

    if (p > intLevel) {
        intStack[intLevel] = owner->r_no_0;
        intLevel = p;
        owner->r_no_0 = r;
        RoutineStepClear(owner);
        flag &= ~1;
        ret = 1;
    } else if (p == intLevel) {
        owner->r_no_0 = r;
        RoutineStepClear(owner);
        flag &= ~1;
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

// Marks the running routine finished (priority back to 0, flags bit0); the action machine polls eor().
void cRoutine::end()
{
    intLevel = 0;
    flag |= 1;
}

// "End of routine": 1 once the running routine called end().
int cRoutine::eor()
{
    int r = 0;
    if (flag & 1) r = 1;
    return r;
}

// Action machine init: mode / request 2 (chase the player), step bytes zero.
void cAction::init(cSubLuis* o)
{
    rno3 = 0;
    rno2 = 0;
    rno1 = 0;
    owner = o;
    rno0 = 2;
    type = 2;
}

// Runs the step machine of the requested mode: 0 wait (routine 0 once), 5 damage (routine 1,
// back to chase when it ends), 6 die (nothing), the others in their move* functions.
void cAction::move(cAnalysis* an, cRoutine* rt)
{
    switch (rno0) {
    case 0:
        if (rno1 == 0) {
            rt->set(0);
            rno1 = 1;
        }
        break;
    case 1: moveAttack(an, rt); break;
    case 2: moveChasePl(an, rt); break;
    case 3: moveGo2F(an, rt); break;
    case 4: moveAttackPl(an, rt); break;
    case 5:
        if (rno1 == 0) {
            rt->set(1);
            rno1 = 1;
        } else if (rt->eor()) {
            type = 2;
            set(0);
        }
        break;
    case 6: break;
    case 7: moveGiveItem(an, rt); break;
    case 8: moveDown(an, rt); break;
    case 9: moveUp(an, rt); break;
    case 0xA: moveAvoid(an, rt); break;
    case 0xB: move11cBegin(an, rt); break;
    case 0xC: moveEscRack(an, rt); break;
    }
}

// Mode 1 (attack the nearest enemy): draw (routine 9) on the analysis target, aim (0xA) for a
// random 0..29 frames, fire (0xB), then after each burst retarget (a closer target within 2000,
// or a new one when the old is gone / dead) and aim again; no target left -> lower the gun (0xC)
// and restart.
void cAction::moveAttack(cAnalysis* an, cRoutine* rt)
{
    switch (rno1) {
    case 0:
        if (an->pEmNear) {
            rt->set(9);
            rt->pTarget = an->pEmNear;
            rno1 = 1;
        } else {
            rt->set(0);
        }
        break;
    case 1:
        if (rt->eor()) rno1 = 2;
        break;
    case 2:
        rt->set(0xA);
        timer = (u8) (Rnd() % 30);
        rno1 = 3;
        break;
    case 3:
        if (--timer == -1) {
            if (an->pEmNear) {
                rt->set(0xB);
                rt->pTarget = an->pEmNear;
                rno1 = 4;
            } else {
                rt->pTarget = an->pEmNear;
            }
        }
        break;
    case 4:
        if (rt->eor()) {
            if (an->pEmNear) {
                if (an->pEmNear != rt->pTarget && an->pEmNearDist < 2000.0f) rt->pTarget = an->pEmNear;
                if (!(rt->pTarget && rt->pTarget->isAlive() && rt->pTarget->hp > 0)) {
                    rt->pTarget = an->pEmNear;
                }
                rno1 = 3;
                timer = (u8) (Rnd() % 30);
            } else {
                rt->pTarget = an->pEmNear;
                rt->set(0xC);
                rno1 = 5;
            }
        }
        break;
    case 5:
        if (rt->eor()) rno1 = 0;
        break;
    }
}

// Mode 3 (go upstairs, set == 2): run (routine 6) to the foot of the stairs (the "upstairs" line
// once, when the player is still low), then on to the upper-floor spot; holds there.
void cAction::moveGo2F(cAnalysis* an, cRoutine* rt)
{
    static const Vec stairPos = { 112500.0f, 1247.0f, -46690.0f };
    static const Vec upPos = { 112160.0f, 3182.64f, -51016.84f };
    const f32 lowY = 2500.0f;   // pool order: the player-height limit precedes the 1000.0 distance

    switch (rno1) {
    case 0:
        if (rt->set(6)) {
            rt->pos = stairPos;
            rt->dist = 1000.0f;
            if (!an->status.check(cAnalysis::S_SAY_2F)) {
                an->status.on(cAnalysis::S_SAY_2F);
                if (pPL->pos.y < lowY) rt->voice.set(0x58, 2, 60);
            }
            rno1 = 1;
        }
        break;
    case 1:
        if (rt->eor()) {
            rt->pos = upPos;
            rno1 = 2;
        }
        break;
    case 2:
        rt->eor();
        break;
    }
}

// Mode 4 (the player shot him 5 times): only raises Room_flg[0] bit29 -- the room script takes
// over (the "Luis shoots back" fail state).
void cAction::moveAttackPl(cAnalysis* an, cRoutine* rt)
{
    if (rno1 == 0) {
        RmfFlagOn(pG, RMF_LUIS_ANGRY);
        rno1 = 1;
    }
}

// Mode 7 (give the item): the throw routine 0xD (target = the player, 4 m), then holds.
void cAction::moveGiveItem(cAnalysis* an, cRoutine* rt)
{
    switch (rno1) {
    case 0:
        rt->set(0xD);
        rt->pos = pPL->pos;
        rt->dist = 4000.0f;
        rno1 = 1;
        break;
    case 1:
        rt->eor();
        break;
    }
}

// Mode 8 (the player aims at him): the crouch routine 0xE; analysis bit2 (down) once it settles.
void cAction::moveDown(cAnalysis* an, cRoutine* rt)
{
    switch (rno1) {
    case 0:
        rt->set(0xE);
        rno1 = 1;
    case 1:
        if (rt->eor()) {
            an->status.on(cAnalysis::S_PL_DOWN);
            rno1 = 2;
        }
        break;
    case 2:
        break;
    }
}

// Mode 9 (no longer aimed at): the stand-up routine 0x10; clears analysis bit2 when done.
void cAction::moveUp(cAnalysis* an, cRoutine* rt)
{
    switch (rno1) {
    case 0:
        rt->set(0x10);
        rno1 = 1;
    case 1:
        if (rt->eor()) {
            an->status.off(cAnalysis::S_PL_DOWN);
            rno1 = 2;
        }
        break;
    case 2:
        break;
    }
}

// Mode 0xA (grenade aimed at him): the dodge routine 0xF; clears analysis bit4 when done.
void cAction::moveAvoid(cAnalysis* an, cRoutine* rt)
{
    switch (rno1) {
    case 0:
        rt->set(0xF);
        rno1 = 1;
    case 1:
        if (rt->eor()) an->status.off(cAnalysis::S_GRENADE);
        break;
    }
}

// Mode 0xB (the room 11C opening, ground floor, once: flags bit0 / bit1): stands (routine 0)
// looking 90 degrees left, then from step 0xF0 turns the head back to the front in 8 frames.
void cAction::move11cBegin(cAnalysis* an, cRoutine* rt)
{
    cSubLuis* o = owner;

    switch (rno1) {
    case 0:
        if (status.check(S_11C_INIT)) {
            status.on(S_11C_BEGIN);
            break;
        }
        status.on(S_11C_INIT);
        rt->set(0);
        rno2 = 0;
        rno1 = 1;
    case 1:
        switch (rno2) {
        default: o->neckSet(-PI / 2, PI); break;
        case 0xF0: o->neckSet(-1.3962634f, PI); break;
        case 0xF1: o->neckSet(-1.0471976f, PI); break;
        case 0xF2: o->neckSet(-0.6981317f, PI); break;
        case 0xF3: o->neckSet(-0.34906584f, PI); break;
        case 0xF4: o->neckSet(-0.17453292f, PI); break;
        case 0xF5: o->neckSet(-0.08726646f, PI); break;
        case 0xF6: o->neckSet(-0.034906585f, PI); break;
        case 0xF7: o->neckSet(-0.017453292f, PI); break;
        case 0xF8:
            rno1 = 2;
            status.on(S_11C_BEGIN);
            break;
        }
        rno2++;
        break;
    case 2:
        break;
    }
}

// Mode 0xC (a rack is being pushed onto him): run (routine 6) to the escape spot (within 500);
// clears analysis bit7 when there.
void cAction::moveEscRack(cAnalysis* an, cRoutine* rt)
{
    static const Vec escPos = { 114536.0f, 4.0f, -51880.0f };

    switch (rno1) {
    case 0:
        if (rt->set(6)) {
            rt->pos = escPos;
            rt->dist = 500.0f;
            rno1 = 1;
        }
    case 1:
        if (rt->eor()) an->status.off(cAnalysis::S_ESC_RACK);
        break;
    }
}

// Mode 2 (no target: stay with the player): idle (routine 0) for a random 30..119 frames, then a
// turn (7, random direction, 10..59 frames) or a 180 turn (8); when the player is reachable
// (chasePlAreaCheck) and farther than 2 m, waits up to 210 frames then walks (5) to within 1.5 m;
// farther than 5 m -> runs (6) to within 1.5 m.
void cAction::moveChasePl(cAnalysis* an, cRoutine* rt)
{
    f32 plDist = an->plDist;

    switch (rno1) {
    case 0:
        rt->set(0);
        timer = (u8) (Rnd() % 90) + 30;
        rno1 = 1;
    case 1:
        if (chasePlAreaCheck() == 1) {
            if (plDist > 5000.0f) rno1 = 0x1E;
            else if (plDist > 2000.0f) rno1 = 0x14;
        } else if (--timer == 0) {
            rno1 = 2;
        }
        break;
    case 2:
        if (Rnd() & 7) {
            rt->set(7);
            rt->work[0] = Rnd() & 1;
            rt->work[1] = (u8) (Rnd() % 50) + 10;
        } else {
            rt->set(8);
        }
        rno1 = 3;
        break;
    case 0x14:
        timer = 0;
        rno1 = 0x15;
    case 0x15:
        if ((u32) ++timer > 210) rno1 = 0x16;
        if (plDist > 5000.0f) rno1 = 0x1E;
        break;
    case 0x16:
        rt->set(5);
        rt->pos = pPL->pos;
        rt->dist = 1500.0f;
        rno1 = 0x17;
        break;
    case 0x17:
        if (rt->eor()) rno1 = 0;
        if (plDist > 5000.0f) rno1 = 0x1E;
        break;
    case 0x1E:
        rt->set(6);
        rt->pos = pPL->pos;
        rt->dist = 1500.0f;
        rno1 = 0x1F;
    case 0x1F:
    case 3:
        if (rt->eor()) rno1 = 0;
        break;
    }
}

// Requests action mode `m`: die (6) always; nothing while in damage (5); from down (8) / up (9)
// only avoid, damage or once the step machine reached step 2; otherwise any different mode.
// Accepted: mode = req = m, step bytes zero.
void cAction::set(int m)
{
    int ok = 0;

    if (m == 6) {
        ok = 1;
    } else if (type != 5) {
        if ((type == 8 && m != 8) || (type == 9 && m != 9)) {
            if (m == 0xA || m == 5 || rno1 == 2) ok = 1;
        } else if (m != type) {
            ok = 1;
        }
    }
    if (ok) {
        type = m;
        rno0 = m;
        rno3 = 0;
        rno2 = 0;
        rno1 = 0;
    }
}

// May he walk to the player? Outside room 11C always; in 11C only while the player is on his floor
// (set 1: ground floor part of the cabin, set 2: upstairs) -- not on the stairs / porch side.
int cAction::chasePlAreaCheck()
{
    if (pG->stage_no != 1 || pG->room_no != 0x1C) return 1;
    switch (owner->set) {
    default:
        return 1;
    case 1:
        if (pPL->pos.z > -47920.0f || pPL->pos.y > 500.0f) return 0;
        return 1;
    case 2:
        if (pPL->pos.z > -47920.0f || pPL->pos.y < 3000.0f) return 0;
        return 1;
    }
}

// Analysis init: no target, scan index 0, player distance far, the periodic-gift flag clear.
void cAnalysis::init(cSubLuis* o)
{
    // Store order from the weight model: cnt is the zero's last use (issued first of the zero stores),
    // the byte RMW of flags comes last in source.
    owner = o;
    pEmNearDist = 0.0f;
    pEmNear = 0;
    iem = 0;
    time = 0;
    plDist = 1000000.0f;
    status.off(S_GIVE_ITEM);
}

// A door enemy between the two points.
int doorHitCheck(Vec* a, Vec* b)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);
        if (em && em->isAlive() && em->hp > 0 && (em->id == 0x41 || em->id == 0x4E) &&
            emLineAtCk(em, a, b, 1e16f, 0)) {
            return 1;
        }
    }
    return 0;
}

// Per-frame analysis: one round-robin step of the enemy scan (isTarget) keeps the nearest valid
// target in pTarget / pEmNearDist (a dead / hidden one is dropped); the route distance to the
// player; aimCheck; every 1800 frames flags bit3 (offer an item); a grenade held near his height
// (greThrowCheck) counts grenadeTimer up and sets flags bit4 (dodge) after 30 frames (hand grenade), 5
// (incendiary / flash) or 1 (rocket), decaying when none is held.
void cAnalysis::move()
{
    cEm* found;
    cEm* em;
    u32 i;
    f32 d;

    time++;
    // Round-robin scan from the entry after idx, until a target is found or it wraps around.
    i = iem;
    while (!isTarget(owner, em = EmMgr.fastAt((i = (i + 1) % EmMgr.getArrayNum())))) {
        if (i == iem) {
            found = 0;
            goto scanned;
        }
    }
    iem = i;
    // COMPILER-DIFF: register tie (global-alloc priority): the loop notes count em's refs double, so em
    // (r30) is allocated before `this` (r29) like the original.
    do { found = em; } while (0);
scanned:

    if (pEmNear && !isTarget(owner, pEmNear)) pEmNear = 0;
    if (found && isTarget(owner, found)) {
        d = GetDistance(owner->pos, found->pos);
        if (pEmNear) {
            if (d < GetDistance(owner->pos, pEmNear->pos)) {
                pEmNear = found;
                pEmNearDist = d;
            }
        } else {
            pEmNear = found;
            pEmNearDist = d;
        }
    }

    plDist = RouteCkPosToPosDis(&owner->pos, &pPL->pos);
    aimCheck();
    if (time % 1800 == 0) status.on(S_GIVE_ITEM);

    switch ((u32) greThrowCheck()) {   // unsigned range tests (cmplwi), the EQ tests stay cmpwi
    case 0x13:
        if (grenadeTimer & 0x80) {
            grenadeTimer = 0;
        } else {
            grenadeTimer++;
            if (grenadeTimer > 30) status.on(S_GRENADE);
        }
        break;
    case 0x16:
    case 0x17:
        if (grenadeTimer & 0x80) {
            grenadeTimer = 0;
        } else {
            grenadeTimer++;
            if (grenadeTimer > 5) status.on(S_GRENADE);
        }
        break;
    case 0xD:
        if (grenadeTimer & 0x80) {
            grenadeTimer = 0;
        } else {
            grenadeTimer++;
            if (grenadeTimer > 1) status.on(S_GRENADE);
        }
        break;
    default:
        if (grenadeTimer < -5) {
            if (status.check(S_GRENADE)) status.off(S_GRENADE);
        } else {
            grenadeTimer--;
        }
        break;
    }
}

// Is the player aiming at him? Sets flags bit1; returns 1 when the check applies to the weapon.
// Knife (0x10): aiming within 2 m and facing him; grenades (0x13/0x16/0x17): never; any gun: the
// laser target is him, or he stays flagged until the aim direction is more than 10 degrees off.
int cAnalysis::aimCheck()
{
    switch (pG->weapon_no) {
    case 0x10:
        status.off(S_PL_AIM);
        if ((PlGetStatus() & 0x10) && GetDistance(pPL->pos, pSUB->pos) < 4000000.0f &&
            Front_check(pPL, pSUB, pPL->ang.y)) {
            status.on(S_PL_AIM);
            return 1;
        }
        break;
    default:
        if (PlGetStatus() & 0x10) {
            if (pPL->Wep->m_pWep->m_SightEm == (cEm*) owner) {
                status.on(S_PL_AIM);
            } else {
                f32 dir = PlGetDirY();
                if (fabsf(GetXZAngleLocal(&pPL->pos, &owner->pos, dir)) > 0.17453292f) status.off(S_PL_AIM);
            }
            return 1;
        }
    case 0x13:
    case 0x16:
    case 0x17:
        status.off(S_PL_AIM);
        break;
    }
    return 0;
}

// One shot: a hit line from 1.6 m above / 0.5 m ahead of him to the target's lock-on parts as
// weapon 3 (Red9 damage) through PlWepHitCheck2 with his own hp zeroed for the call (so he is
// not in the target list), the muzzle flash on the gun object and the shot SE.
void cRoutine::shot()
{
    Vec p;
    Vec t;
    s16 hp;

    if (pTarget == 0) return;
    p.x = 0.0f;
    p.y = 1600.0f;
    p.z = 500.0f;
    PSMTXMultVecSR(owner->mat, &p, &p);
    PSVECAdd(&p, &owner->pos, &p);
    t = pTarget->getPartsPtr(pTarget->lockParts)->world;
    hp = owner->hp;
    owner->hp = 0;
    PlWepHitCheck2(0, &p, &t, 3, 0, 6000.0f);
    owner->hp = hp;
    EstSet(owner->pWep, -1, 0, 0, EFF_PL04, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
    SndCall(8, 0, &owner->pList->world, owner->id, 0, 0);
}

// Plays the motion key sound (seNo) at its parts.
void cSubLuis::seqSeCtrl()
{
    u8 k = Motion.Seq_old.Se;
    u32 se;
    int parts;
    u16 blk;

    if (k == 0) return;
    se = k - 1;
    switch (se) {
    case 0:
    case 2:
        parts = 0x14;
        blk = 5;
        se += 7;
        break;
    case 1:
    case 3:
        parts = 0x18;
        blk = 5;
        se += 7;
        break;
    case 4:
    case 5:
        parts = 0;
        blk = 5;
        se += 7;
        break;
    default:
        parts = 0;
        blk = 8;
        break;
    }
    SndCall(blk, (u16) se, &getPartsPtr(parts)->world, id, 0, 0);
    Motion.Seq_old.Se = 0;
}

// Damage of the frame -> flags bit0 and the routine's work[]: stat 0x0400xxxx (scenario kill) ->
// die; a damage area hit (DmgMgr) -> flinch 3; a registered enemy / player hit (dmg.m_Flag) by weapon
// (dmg.m_Wep): the player's guns count down m_PlAtack (at 0 flags bit1 = attack the player, at 1
// EM_STATUS_DONT_FIRE), flinch 2 from the front / 3 from behind, pTarget = the player; grenade 0x13
// -> blast 8; 0x17 (flash) ignored; 0x18 -> flinch 2. Returns 1 when a reaction was set.
int cSubLuis::damageCheck()
{
    int dead;

    if (r_no_0 == 4 && r_no_1 == 0) {
        action.set(6);
        return 1;
    }
    dead = dmg.m_Flag != 0 || dmg.m_Timer != 0;
    if (!dead && (s16) pG->pl_life > 0 && DmgMgr.hitCheck(&getPartsPtr(0)->world, 0) == 1) {
        routine.work[0] = 3;
        dmg.m_Flag = dead;
        dmg.m_Timer = 0x80;
        status.on(F_DAMAGED);
        return 1;
    }
    if (dmg.m_Flag == 0) return 0;

    analysis.status.off(cAnalysis::S_PL_DMG);
    routine.work[1] = 0;
    switch (dmg.m_Wep) {
    default:
        m_PlAtack--;
        if (m_PlAtack == 0) {
            status.on(F_PL_ATTACKED);
        } else {
            if (m_PlAtack == 1) setStatus(EM_STATUS_DONT_FIRE);
            analysis.status.on(cAnalysis::S_PL_DMG);
            routine.work[1] = m_PlAtack;
        }
        routine.pTarget = pPL;
        dmg.m_Timer = 1;
        if (Front_check(this, &dmg.m_PosFrom, PI / 2)) routine.work[0] = 2;
        else routine.work[0] = 3;
        SndCall(8, 0x13, &pEm->pList->world, pEm->id, 0, 0);
        break;
    case 0x13:
        dmg.m_Timer = 1;
        routine.work[0] = 8;
        break;
    case 0x17:
        dmg.m_Flag = 0;
        return 0;
    case 0x18:
        dmg.m_Timer = 1;
        routine.work[0] = 2;
        break;
    }
    status.on(F_DAMAGED);
    dmg.m_Flag = 0;
    dmg.m_Timer = 0x80;
    return 1;
}

// Creates his gun (ObjMgr id 0xB, model 0x38/0x3C of the partner archive) hung on his right hand
// (parts 10) with a light area and himself as the weapon parent.
void cSubLuis::equipWeapon()
{
    pWep = ObjMgr.createBack(cObjMgr::ID_PL_WEAPON);
    if (pWep == 0) {
        pLog->err(0, 0, "Luis.equipWeapon() CREATE FAILED");
    } else {
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };
        pWep->modelInit(SUB_ARC(this, 0x38 / 4), SUB_ARC(this, 0x3C / 4));
        pWep->atari.m_flag &= 0xFCFF;
        pWep->pList->pParent = getPartsPtr(10);
        pWep->LightInfo.init2(1, 1, LuisLightZero(), &p1, 1);
        ((cObjWep*) pWep)->m_pParent = this;
    }
}

// End of an enemy's grab / damage routine on him (pl_sub EndSubDamage): a reaction line in 30
// frames when the attacker is still marked, back to action mode 0 and the routine ended.
void cSubLuis::endDamage()
{
    if (status.check(F_KARAMI) && pEmCatch && ((cEm*) pEmCatch)->dmg.m_Timer) thankCtr = 30;
    status.off(F_KARAMI);
    action.set(0);
    routine.end();
}


// Eyes: the eyelid (parts 0x1C) blink animation on luisEyeTimer (a blink at 0..6, a double blink
// from 0x5A, random pause), the eye yaw target (luisEye) picked randomly at each blink and
// jittered every 2..4 frames (luisBlink), clamped to +-0.314 and applied to both eye parts 0x20 / 0x21.
void cSubLuis::moveEye()
{
    static int luisEyeTimer;   // eyelid animation frame; a function-local static so it precedes the ctor'd luisEye in .bss
    cParts* p = getPartsPtr(0x1C);
    // `u8 r` is block-scoped in both Rnd blocks: one function-scope `r` is a two-set global pseudo (r0)
    // where the target ties the masked remainder to the Rnd result (`clrlwi r3, r3, 24`).

    switch (luisEyeTimer++) {
    default: p->ang.x = 0.0f; break;
    case 0: {
        u8 r = Rnd() % 200;
        luisEye = (r * 0.01f - 1.0f) * PI * 0.1f;
        p->ang.x = 0.17453292f;
        break;
    }
    case 1: p->ang.x = 0.34906584f; break;
    case 2: p->ang.x = 0.6981317f; break;
    case 3: p->ang.x = 0.62831855f; break;
    case 4: p->ang.x = 0.4886922f; break;
    case 5: p->ang.x = 0.34906584f; break;
    case 6: p->ang.x = 0.17453292f; break;
    case 0x1E:
        luisEye = 0.0f;
        break;
    case 0x58:
        luisEyeTimer = (Rnd() & 3) ? 0 : 0x5A;
        break;
    case 0x5A: p->ang.x = 0.17453292f; break;
    case 0x5B: p->ang.x = 0.34906584f; break;
    case 0x5C: p->ang.x = 0.6981317f; break;
    case 0x5D: p->ang.x = 0.5934119f; break;
    case 0x5E: p->ang.x = 0.6632251f; break;
    case 0x5F: p->ang.x = 0.6981317f; break;
    case 0x60: p->ang.x = 0.5235988f; break;
    case 0x61: p->ang.x = 0.34906584f; break;
    case 0x62:
        p->ang.x = 0.17453292f;
        luisEyeTimer = 10;
        break;
    }
    p->matUpdate();

    if (--luisBlink < 0) {
        u8 r = Rnd() % 200;
        luisEye += (r * 0.01f - 1.0f) * 0.03141593f;
        luisBlink = (u8) (Rnd() % 3) + 2;
    }
    luisEye.limit(-0.31415927f, 0.31415927f);

    getPartsPtr(0x20)->ang.y = luisEye;
    getPartsPtr(0x20)->cCoord::matUpdate();
    getPartsPtr(0x21)->ang.y = luisEye;
    getPartsPtr(0x21)->cCoord::matUpdate();
    luisEye.move();
}

// Turns the head yaw neckY towards `ang` by at most `limit` this frame and marks it set (flags bit3).
void cSubLuis::neckSet(f32 ang, f32 limit)
{
    neckY += Muku2(neckY, ang, limit);
    status.on(F_NECK_SET);
}

// Applies the head yaw: without a neckSet this frame it returns to 0 (0.628 rad per frame); the
// neck parts (3) gets the additional rotation.
void cSubLuis::neckMove()
{
    cParts* p;
    const f32 spd = 0.62831855f;   // pool order: the turn speed precedes the 0.0

    if (status.check(F_NECK_SET)) {
        status.off(F_NECK_SET);
    } else {
        neckY += Muku2(neckY, 0.0f, spd);
    }
    p = pEm->getPartsPtr(3);
    ((cParts*) p)->motParts.flags |= 0x40000000;
    ((cParts*) p)->inv_offset.y = neckY;
}

// No line playing.
cVoice::cVoice()
{
    on = 0;
    time = 0;
    seId = 0xF0F0F0F0;
}

// Speaks a line: stops the previous voice, clears the subtitles when the last line has run out,
// plays the voice SE (bank 8) at him and shows message `mesNo` at the subtitle position; the line
// lasts `time` frames.
void cVoice::set(int mesNo, u16 seNo, int time)
{
    int i;

    if (seId != 0xF0F0F0F0) SndStop(seId, 0);
    if (this->time <= 1) {
        MessageControl* mes = &cMes;
        this->time = 0;
        on = 0;
        cMes.Clear();
    }
    if ((s16) pG->pl_life > 0) {
        seId = SndCall(8, seNo, &pSUB->pList->world, pSUB->id, 0, 0);
        cMes.MesSet(mesNo, 100, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 0x1000051, 0, 0, 4);   // fold swaps the two subtrahends
    }
    this->time = time;
    on = 1;
}

// Counts the line down and clears the subtitles when it ends.
void cVoice::move()
{
    int i;

    if (on == 1) {
        if (time-- < 0) {
            MessageControl* mes = &cMes;
            on = 0;
            cMes.Clear();
        }
    }
}

// On the stairs of room 11C.
int stairCheck(cModel* m)
{
    if (pG->stage_no != 1 || pG->room_no != 0x1C) return 0;
    if (m->pos.x > 109070.0f && m->pos.x < 114620.0f && m->pos.z > -47600.0f && m->pos.z < -45930.0f) return 1;
    return 0;
}

// The two models are within 1 m in height (same floor of the cabin).
int sameFloorCheck(cModel* a, cModel* b)
{
    const f32 lim = 1000.0f;   // the pool `lis` is expanded here, above the fabsf barrier

    return fabsf(a->pos.y - b->pos.y) < lim;
}

// ObjInitFunc[0x1E]: placement-constructs the thrown item object.
void luisItemInit(cObj* obj)
{
    new (obj) cObjLuisItem();
}

// The thrown item: the room archive's item model at his hand, a glow effect (group 0x3C), flying
// towards the player at 7 % of the distance per frame with a small downward acceleration.
void cObjLuisItem::init(Vec* p, f32 rotY)
{
    modelInit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore));
    setPos(p);
    ang.y = rotY;
    ang.x = 0.0f;
    ang.z = 0.0f;
    EstSet(this, -1, 0, 0, EFF_CORE, 0x2D, 0, ESP_CORE_KIND_LUIS_ITEM, this, 0);
    PSVECSubtract(&pPL->pos, &pos, &v);
    PSVECScale(&v, &v, 0.07f);
    a.x = 0.0f;
    a.y = -fabsf(v.y) * 0.03f;
    a.z = 0.0f;
    timer = 0;
}

// Thrown item update: r_no_0 0 flies (pushed off walls, dropped to the floor on a hit; gone after
// 150 frames), 1 lands: turns into a pickup (SceAtCreateItemAt) chosen by need -- ammo short:
// handgun ammo 4 (75 %), else 1 / 2 / 0xE; health short: herb 5 (21 %) or 6 -- and destroys itself.
void cObjLuisItem::move()
{
    Vec hit;
    Vec nrm;
    int b;
    u8 r;
    u16 item;

    switch (r_no_0) {
    case 0:
        PSVECAdd(&pos, &v, &pos);
        PSVECAdd(&v, &a, &v);
        if (SatMgr.hitCheck(&pos_old, &pos, &hit, &nrm, 0, 0)) {
            if (nrm.y < 0.5f && nrm.y > -0.5f) {
                PSVECScale(&nrm, &nrm, 200.0f);
                PSVECAdd(&hit, &nrm, &pos);
            }
            pos.y = SatMgr.getFloor(&pos, 0, 600.0f, 100000.0f, 0);
            r_no_0 = 1;
        }
        timer++;
        if (timer > 150) {
            EffectEspDelete(0, ESP_CORE_KIND_LUIS_ITEM, this, 0);
            EffectEspgenDelete(0, ESP_CORE_KIND_LUIS_ITEM, this);
            EffectEfmDelete(0, ESP_CORE_KIND_LUIS_ITEM, this);
            ObjMgr.destroy(this);
        }
        break;
    case 1:
        b = (u32) GetBulletPoint() < (u32) GetRecoveryPoint();
        r = Rnd() % 100;
        switch (b) {
        case 0:
            if (r > 0x4A) {
                if (r <= 0x4F) item = 1;
                else if (r <= 0x59) item = 2;
                else item = 0xE;
            } else {
                item = 4;
            }
            break;
        case 1:
            item = r > 0x14 ? 6 : 5;
            break;
        default:
            item = 4;
            break;
        }
        EffectEspDelete(0, ESP_CORE_KIND_LUIS_ITEM, this, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_LUIS_ITEM, this);
        EffectEfmDelete(0, ESP_CORE_KIND_LUIS_ITEM, this);
        SceAtCreateItemAt(&pos, item, 0, -1, -1, 0, -1);
        ObjMgr.destroy(this);
        break;
    }
    matUpdate();
}

// The grenade the player holds near his height: the throw routine to answer with, 0 = none.
int greThrowCheck()
{
    cObj* o;

    for (o = ObjMgr.getActiveWork(); o; o = ObjMgr.getNext(o)) {
        if (fabsf(o->pos.y - (pSUB->pos.y + 2000.0f)) < 2500.0f) {
            switch (o->id) {
            case 0x1A: return 0x13;
            case 0x29: return 0x16;
            case 0x2A: return 0x17;
            case 0x22: return 0xD;
            }
        }
    }
    return 0;
}

// Can Luis shoot enemy `em`? Alive, visible, a real enemy (id > 0xF), lockable, no effect
// collision or door enemy between them, and the weapon target list from him to it does not put
// something else in front.
int isTarget(cSubLuis* luis, cEm* em)
{
    WepTarget list[2];
    Vec hit;
    Vec nrm;
    u32 attr;

    if (!VALID_PTR(em)) {
        pLog->err(0, 0, "LUIS isTarget() INVALIED PTR 0x%08x", em);
        return 0;
    }
    if (!VALID_PTR(em) || !em->isAlive() || em->hp <= 0 || em->id <= 0xF || em->checkStatus(EM_STATUS_LOCKOFF) ||
        EatMgr.hitCheck(&luis->pList->world, &em->pList->world, 0, 0, 0, 0x400000) ||
        doorHitCheck(&luis->pList->world, &em->pList->world)) {
        return 0;
    }
    if (GetWepTargetList2(&luis->pList->world, &em->pList->world, list, 2, &hit, &nrm, &attr, 2, 0) > 1 &&
        list[1].em != em) {
        return 0;
    }
    return 1;
}

// REL entry: registers Luis as the enemy init function and the thrown item's constructor slot.
extern "C" void _prolog()
{
    EmInitFunc = LuisInit;
    ObjInitFunc[0x1E] = luisItemInit;
    OSReport("LUIS prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x1E] = 0;
    OSReport("LUIS epilog Ok\n");
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
