// game/pl_sub.cpp: player / partner helpers: costume, data reload, damage entry points, partner
// (Ashley) control, ladder, motion registration, key helpers, water effects.

#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "item.h"
#include "snd.h"
#include "esp.h"
#include "obj.h"
#include "act_btn.h"
#include "rnd.h"
#include "math_sub.h"

extern "C" {
void ReleaseWepData();                         // game/read.cpp
void ReadPlayerData(int type, int costume);    // game/read.cpp
void AddWaterPower(Vec* pos, f32 power);       // game/Espgen42.cpp
void* memset(void* dst, int c, unsigned int n);
}
f32 GetDistance(Vec& v0, Vec& v1);               // game/sub2.cpp (second overload)

extern void (*Pl_func_tbl[7])(cPlayer*);       // game/player.cpp

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Stores through references: scalar MEMs, so pG is reloaded after each of them (the original
// reloads pG after every store to a GlobalWork field in this unit).
static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }

// Routine bytes of the player set through one fresh load of pPL (the four byte stores share one
// register in the original even right after a call, unlike direct `pPL->xFC = ..` stores).
static inline void PlSetRoutine(int a, int b, int c, int d)
{
    cPlayer* p = pPL;

    p->r_no_0 = a;
    p->r_no_1 = b;
    p->r_no_2 = c;
    p->r_no_3 = d;
}

// Switches the player character to pl_type `no` (Leon <-> Ashley chapter): swaps the two life
// maxima and the peseta counts, frees the weapon data, re-picks the costume; pl_flag bit0 = the
// player data must be reloaded.
void PlSelect(int no)
{
    if (pG->pl_type != no) {
        s16 life = pG->pl_life_max;
        u32 tmp;

        U16Set(pG->pl_life_max, pG->ashley_life_max);
        U16Set(pG->ashley_life_max, life);
        pG->pl_life = pG->pl_life_max;
        ReleaseWepData();
        tmp = pG->peseta;
        U32Set(pG->peseta, pG->peseta_bak);
        U32Set(pG->peseta_bak, tmp);
    }
    U8Set(pG->pl_type, no);
    PlSetCostume();
    BitOn16(pG->pl_flag, 1);
}

// Chooses pl_costume: Leon = 2 with the armor item (0xFE), 1 after finding item 0x200000 (the
// chapter 5 outfit), 3 in the special costume mode, else 0; the others use game_costume. Kept as
// is when the save system flags (System_flg bit31 / 0x40000000) are set. Returns the costume.
int PlSetCostume()
{
    if (FlagChkSignW(pG->System_flg, SYS_OMAKE_ADA_GAME) || FlagChkSignW(pG->System_flg, SYS_OMAKE_ETC_GAME)) {
        return pG->pl_costume;
    }
    if (pG->pl_type == 0) {
        if (pG->game_costume != 1) {
            if (ItemMgr.num(0xFE, 0)) {
                U8Set(pG->pl_costume, 2);
            } else if (ScfFlagChk(pG, SCF_R106_EVENT)) {
                U8Set(pG->pl_costume, 1);
            } else {
                U8Set(pG->pl_costume, 0);
            }
        } else {
            U8Set(pG->pl_costume, 3);
        }
    } else {
        U8Set(pG->pl_costume, pG->game_costume);
    }
    BitOn16(pG->pl_flag, 1);
    return pG->pl_costume;
}

// Reloads the player after a character / costume change: drops the weapon, the player archive and
// effects, reads the new data (ReadPlayerData), rebuilds the model, motion table and weapon,
// resets the routine to 0/0 with a pending footwork.
void PlChangeData()
{
    cPlayer* pl;

    pPL->weaponRelease();
    PlDataRelease();
    EspDataRelease(3, 1, 1);
    pPL->push();
    BitOn16(pG->pl_flag, 1);
    ReadPlayerData(pG->pl_type, pG->pl_costume);
    pl = pPL;
    pl->setModel();
    pl->setMotion();
    EspDataLoad((u32) PL_ARC_PTR(pG->pPlayer, 0x1A), 3, 0);
    pPL->weaponInit();
    pl->be_flag |= 0x20;
    pl->r_no_0 = 0;
    pl->r_no_1 = 0;
    pl->r_no_2 = 0;
    pl->r_no_3 = 1;
    pl->m_Hokan = 0;
    pl->m_Frame = 0;
    pl->initCloth();
}

// Starts a button-mash count (m_GachaCtr = 0).
void PlGachaInit()
{
    pPL->m_GachaCtr = 0;
}

// One frame of a button mash: shows the mash action icon and counts every direction / A / B / C
// trigger into m_GachaCtr.
void PlGachaMove()
{
    cPlayer* pl = pPL;

    ActBtn.set(0x2B, 5, 0, 0, 2, 0xB, 0, 0);
    if (Key.trg & 0xF) {
        pl->m_GachaCtr++;
    }
    if (Key.trg & 0xC0000000) {
        pl->m_GachaCtr++;
    }
    if (Key.trg & 0x0C000000) {
        pl->m_GachaCtr++;
    }
}

// Mash presses so far, 1.5x on the easy levels (Game_level <= 2).
int PlGachaGet()
{
    int n = pPL->m_GachaCtr;

    if (pG->Game_level <= 2) {
        n += n / 2;
    }
    return n;
}

// Player hurt voice `no` at the head (parts 4); 0 = one of the three random grunts (9-11).
void PlSetDamageSe(int no)
{
    cPlayer* pl = pPL;

    if (no == 0) {
        u8 r = (u32) Rnd() % 3;

        no = r + 9;
    }
    SndCall(1, no, &pl->getPartsPtr(4)->world, 0, 0, 0);
}

// Player routine as a bit word for the camera / scenario / HUD: bit0 idle, 1 walk / turn, 2 back,
// 3 run, 4 weapon out (5 ready, 6 fire, 0x800 reload, 0x4000 ...), 7 damage, 8 level up, 9 level
// down, 0xA push, 0xD die, 0xF crouch, 0x10 aux, 0x11 whistle-wait, 0x12 ladder, 0x13 fall,
// bit17 (0x20000) in an event, bit31 unknown routine.
u32 PlGetStatus()
{
    cPlayer* pl = pPL;
    u32 st = 0;

    switch (pl->r_no_0) {
    case 0:
        switch (pl->r_no_1) {
        case 0:
            st = 1;
            break;
        case 1:
        case 4:
        case 5:
            st |= 2;
            break;
        case 2:
            st = 4;
            break;
        case 3:
            st = 8;
            break;
        case 6:
        case 0xB:
            if (pl->r_no_2 != 0) {
                st |= 0x10;
            }
            switch (pl->r_no_2) {
            case 1:
                st |= 0x20;
                break;
            case 2:
                st |= 0x40;
                break;
            case 4:
                st |= 0x800;
                break;
            case 6:
                st |= 0x4000;
                break;
            default:
                st |= 0x80000000;
                break;
            }
            break;
        case 0x11:
            st = 0x8000;
            break;
        case 9:
            st = 0x400;
            break;
        case 7:
            st = 0x100;
            break;
        case 0x10:
            st = 0x40000;
            break;
        case 8:
            st = 0x200;
            break;
        case 0x12:
            st = 0x10000;
            break;
        case 0xE:
            st = 0x80000;
            break;
        default:
            st |= 0x80000000;
            break;
        }
        break;
    case 1:
    case 4:
        st |= 0x80;
        break;
    case 2:
        st = 0x2000;
        break;
    default:
        st |= 0x80000000;
        break;
    }
    if (pl->stat & 2) {
        st |= 0x20000;
    }
    return st;
}

// Scenario: puts the player in the crouch routine (0/0x11).
void PlSetCrouch()
{
    cPlayer* pl = pPL;

    pl->r_no_0 = 0;
    pl->r_no_2 = 0;
    pl->r_no_1 = 0x11;
    pl->r_no_3 = 0;
}

// Shows / hides the weapon hand model (type 1 = display slot 0, else slot 1) through cPlWep::setTrans.
void PlSetHand(int type, int on)
{
    int t = 1;

    if (type == 1) {
        t = 0;
    }
    pPL->Wep->setTrans(t, on);
}

// Partner hand model (cSubChar::setHand).
void SubCharSetHand(int no)
{
    if (pSUB) {
        pSUB->setHand(no);
    }
}

// Custom damage routine: `func` becomes routine 0 == 4 and runs each frame until EndPlDamage;
// `type` is kept in pEmCatch (often the attacking object). 10 invulnerable frames.
void SetPlDamage(cEm* em, void (*func)(cPlayer*))
{
    cPlayer* pl = pPL;

    pl->beginDamage();
    Pl_func_tbl[4] = func;
    PlSetRoutine(4, 0, 0, 0);
    pl->dmg.set(0, 10);
    pl->pEmCatch = em;
}

// Ends a custom damage routine: back to routine 0/0, damage state restored, collision on again
// with the default 400 x 200 cylinder.
void EndPlDamage()
{
    cPlayer* pl = pPL;
    cAtariInfo* at = &pl->atari;

    pl->dmg.clear();
    PlSetRoutine(0, 0, 0, 0);
    pl->subArc = pl->subArc2;
    at->throughOff();
    at->setPriority(0);
    at->set(10, 400.0f, 200.0f);
    pl->endDamage();
}

// Partner: aux routine 0/0xF with two parameters (scenario-specific behaviour).
void SetSubAux(int a, int b)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        pLog->err(0, 0, "ERROR: SetSubAux() ASHLEY NOT FOUND.");
        return;
    }
    sub->r_no_0 = 0;
    sub->r_no_2 = 0;
    sub->subAux0 = a;
    sub->subAux1 = b;
    sub->r_no_1 = 0xF;
    sub->r_no_3 = 0;
}

// Partner: the bulldozer-ride routine (r_no_0 3) with two parameters.
void SetSubBulldozer(int a, int b)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        pLog->err(0, 0, "ERROR: SetSubAux() ASHLEY NOT FOUND.");
        return;
    }
    sub->r_no_1 = 0;
    sub->r_no_2 = 0;
    sub->subAux0 = a;
    sub->subAux1 = b;
    sub->r_no_0 = 3;
    sub->r_no_3 = 0;
}

// Partner damage routine (r_no_0 4): Ashley (id 3) runs the em damage function; the other partners
// play `mot` (subFlags58C 0x40). pEmCatch = type.
void SetSubDamage(cEm* em, void* mot)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        return;
    }
    if (sub->id == 3) {
        sub->setEmFunc();
        sub->r_no_0 = 4;
        sub->r_no_1 = 0;
        sub->r_no_2 = 0;
        sub->r_no_3 = 0;
        sub->dmg.set(0, 10);
        sub->pEmCatch = em;
    } else {
        sub->subMot0 = mot;
        sub->subFlags58C |= 0x40;
        sub->r_no_0 = 4;
        sub->r_no_1 = 0;
        sub->r_no_2 = 0;
        sub->r_no_3 = 0;
        sub->dmg.set(0, 10);
        sub->pEmCatch = em;
    }
}

// Ends the partner damage routine: model reset for Luis (id 4), routine 0/0, collision on.
void EndSubDamage()
{
    cSubChar* sub = pSUB;
    cAtariInfo* at;

    if (sub == 0) {
        return;
    }
    sub->dmg.clear();
    if (sub->id == 4) {
        pSUB->modelSet();
    } else {
        sub->endDamage();
    }
    sub->subArc = sub->subArc2;
    sub->r_no_0 = 0;
    sub->r_no_1 = 0;
    sub->r_no_2 = 0;
    sub->r_no_3 = 0;
    at = &sub->atari;
    at->throughOff();
    at->setPriority(0);
    at->set(10, 400.0f, 200.0f);
}

// Creates the partner enemy if none exists: type 0 = Luis (em 2), 1 = Ashley (em 3, or 5 in the
// alternate costume; id forced to 3), 2 = (em 4); placed at pos / ang, nudged off the player.
void SubCharInit(int type, Vec* pos, f32 ang)
{
    cSubChar* sub;

    if (pSUB != 0) {
        return;
    }
    switch (type) {
    case 0:
    default:
        sub = (cSubChar*) EmMgr.createBack(2);
        break;
    case 1:
        if (pG->game_costume != 1) {
            sub = (cSubChar*) EmMgr.createBack(3);
        } else {
            sub = (cSubChar*) EmMgr.createBack(5);
        }
        if (sub) {
            sub->id = 3;
        }
        break;
    case 2:
        sub = (cSubChar*) EmMgr.createBack(4);
        break;
    }
    if (!VALID_PTR(sub)) {
        pLog->err(0, 0, "SubCharInit() failed.");
        pSUB = 0;
        return;
    }
    sub->setPos(pos);
    {
        Vec rot;

        rot.x = 0.0f;
        rot.z = 0.0f;
        rot.y = ang;
        sub->setAng(&rot);
    }
    if (pos->x == pPL->pos.x && pos->z == pPL->pos.z) {
        sub->pos.x += 1.0f;
        sub->pos.z += 2.0f;
    }
    sub->flag |= 1;
    pSUB = sub;
}

// Partner command: mode 0 follow, 1 wait here, 2 destroy, 3 stop, 4 warp to the player, 5 routine
// 5, 6 re-init, 7 wait; flag bit0 restarts the routine at once, bit1 = manual control (subFlags
// 0x80). Aux parameters are cleared unless she is in the aux routine.
void SubCharCtrl(int mode, int flag)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        return;
    }
    if (sub->hp <= 0) {
        return;
    }
    switch (mode) {
    case 0:
        sub->control(1);
        if (flag & 1) {
            sub->r_no_0 = 0;
            sub->r_no_1 = 0;
            sub->r_no_2 = 0;
            sub->r_no_3 = 1;
            sub->move();
        }
        break;
    case 1:
        sub->control(2);
        if (flag & 1) {
            sub->r_no_0 = 0;
            sub->r_no_1 = 0;
            sub->r_no_2 = 0;
            sub->r_no_3 = 1;
            sub->move();
        }
        break;
    case 2:
        EmMgr.destroy(sub);
        pSUB = 0;
        break;
    case 3:
        sub->control(0);
        break;
    case 4:
        sub->control(3);
        break;
    case 5:
        sub->r_no_0 = 5;
        sub->r_no_1 = 0;
        sub->r_no_2 = 0;
        sub->r_no_3 = 0;
        break;
    case 6:
        sub->control(5);
        break;
    case 7:
        sub->control(6);
        break;
    }
    if (flag & 2) {
        sub->subFlags |= 0x80;
    } else {
        BitOff16(sub->subFlags, 0x80);
    }
    if (sub->r_no_0 != 0 || sub->r_no_1 != 0xF) {
        sub->subAux0 = 0;
    }
    sub->subAux1 = 0;
}

// May the partner take a command now? 1 under manual control, else only when controllable
// (subFlags 0x40), not stopped / moving-to, and in one of the plain routine-0 states.
int SubCharCheckCtrl()
{
    cSubChar* sub = pSUB;

    if (sub->subFlags & 0x80) {
        return 1;
    }
    if (!(sub->subFlags & 0x40)) {
        return 0;
    }
    if (sub->subFlags & 1) {
        return 0;
    }
    if (sub->subFlags & 8) {
        return 0;
    }
    if (sub->r_no_0 != 0) {
        return 0;
    }
    switch (sub->r_no_1) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xE:
    case 0x11:
    case 0x12:
    case 0x13:
        break;
    default:
        return 0;
    }
    return 1;
}

// Partner hide: mode 0 flags her hidden (subX534), mode 1 sends her to hide at `pos` (routine
// 0/0x10, invulnerable).
void SubCharCtrlHide(Vec* pos, int mode)
{
    cSubChar* sub = pSUB;

    switch (mode) {
    case 0:
        sub->subX534 = 1;
        break;
    case 1:
        sub->dmg.set(0, 0x80);
        sub->subHidePos = *pos;
        sub->subHideMode = 1;
        sub->r_no_0 = 0;
        sub->r_no_1 = 0x10;
        sub->r_no_2 = 0;
        sub->r_no_3 = 0;
        break;
    }
}

// Partner: walk to (x, y, z) with parameter w (193 = special values), unless already going there;
// flag bit0 sets subFlags 0x10.
void SubCharMoveTo(int flag, f32 x, f32 y, f32 z, f32 w)
{
    cSubChar* sub = pSUB;

    if ((sub->subFlags & 8) && x == sub->subMoveTo[0] && y == sub->subMoveTo[1] && z == sub->subMoveTo[2] &&
        w == sub->subMoveTo[3]) {
        return;
    }
    sub->subMoveTo[0] = x;
    sub->subMoveTo[1] = y;
    sub->subMoveTo[2] = z;
    sub->subMoveTo[3] = w;
    sub->control(4);
    sub->analyze();
    sub->move();
    BitOff16(sub->subFlags2, 0x40);
    if (flag & 1) {
        sub->subFlags |= 0x10;
    }
}

// Scenario: puts the player on a ladder at pos / ang: level > 1 climbs up (m_Work0 = level - 2
// rungs), level < -1 climbs down (r_no_2 0xA); routine 0/0x10.
void PlSetLadder(Vec* pos, int level, f32 ang)
{
    cPlayer* pl;

    if (level >= -1 && level <= 1) {
        pLog->err(0, 0, "ERROR PlSetLadder() level set error %d", level);
        return;
    }
    pl = pPL;
    pl->atari.throughOn();
    pl->dmg.set(0, 0x80);
    Vec v = {0.0f, 0.0f, 0.0f};
    Vec rot;

    PSMTXMultVecSR(pl->mat, &v, &v);
    PSVECAdd(&v, pos, &v);
    pl->setPos(&v);
    rot.x = 0.0f;
    rot.z = 0.0f;
    rot.y = ang;
    pl->setAng(&rot);
    pl->r_no_1 = 0x10;
    pl->r_no_2 = 0;
    pl->r_no_3 = 0;
    pl->r_no_0 = 0;
    if (level > 0) {
        pl->m_Work0 = level - 2;
    } else {
        pl->r_no_2 = 0xA;
        pl->m_Work0 = -2 - level;
    }
}

// Neck look mode (0 off, 1 on, 2 next frame).
void PlSetNeck(int mode)
{
    pPL->Neck->setMode(mode);
}

// Ends the aim camera; if the weapon was out, shows it again and goes back to routine 0/0.
void PlEndCamera()
{
    cPlayer* pl = pPL;

    if (pl->endCamera()) {
        if (PlGetStatus() & 0x10) {
            pl->Wep->m_pWep->setDisp(1, 1);
            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
        }
    }
}

void PlRegistMotion(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8,
                    void* m9, void* m10, void* m11)
{
    cPlayer* pl = pPL;

    if (!VALID_PTR(pl)) {
        pLog->err(0, 0, "PlRegistMotion() pPL PTR ERROR. 0x%08x", pl);
        return;
    }
    if (m0) {
        pl->m_MotTbl2[0] = m0;
    }
    if (m1) {
        pl->m_MotTbl2[1] = m1;
    }
    if (m2) {
        pl->m_MotTbl2[2] = m2;
    }
    if (m3) {
        pl->m_MotTbl2[3] = m3;
    }
    if (m4) {
        pl->m_MotTbl2[4] = m4;
    }
    if (m5) {
        pl->m_MotTbl2[5] = m5;
    }
    if (m6) {
        pl->m_MotTbl2[6] = m6;
    }
    if (m7) {
        pl->m_MotTbl2[7] = m7;
    }
    if (m8) {
        pl->m_MotTbl2[8] = m8;
    }
    if (m9) {
        pl->m_MotTbl2[9] = m9;
    }
    if (m10) {
        pl->m_MotTbl2[10] = m10;
    }
    if (m11) {
        pl->m_MotTbl2[11] = m11;
    }
}

// Partner: room-supplied motions subMot0 / subMot1 (non-zero ones only).
void SubCharRegistMotion(void* m0, void* m1)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        pLog->err(0, 0, "SubCharRegistMotion() NO SUBCHAR");
        return;
    }
    if (m0) {
        sub->subMot0 = m0;
    }
    if (m1) {
        sub->subMot1 = m1;
    }
}

// Room water effect table for the player (ripple, walk splash, run splash).
void PlRegistRoomEff(PlRoomEff* eff)
{
    pPL->m_pEffRoom = eff;
}

// After a reload of the bow / launcher / grenade types the weapon object refreshes the player's
// motion table (shows the new round).
void PlReloadBullet()
{
    cPlayer* pl = pPL;

    switch (pG->weapon_no) {
    case 0xD:
    case 0x13:
    case 0x16:
    case 0x17:
        pl->Wep->m_pWep->setMotion(pl);
        break;
    }
}

// Fire key held; 0 (and Status_flg[0] 0x4000 "blocked shot") while firing is forbidden
// (Status_flg[0] 0x200000 or the laser's don't-fire bit); room 11C also sets Room_flg[0] 0x20000000.
int joyFireOn()
{
    if (Key.on & 0x80) {
        if (StaFlagChk(pG, STA_ACT_DONT_FIRE) || (StaFlagChk(pG, STA_PL_DONT_FIRE))) {
            StaFlagOn(pG, STA_PL_ACTION);
            if (pG->stage_no == 1 && pG->room_no == 0x1C && (StaFlagChk(pG, STA_PL_DONT_FIRE))) {
                BitOn(pG->Room_flg[0], 0x20000000);
            }
            return 0;
        }
        return 1;
    }
    return 0;
}

// Fire key pressed this frame, with the same block as joyFireOn.
int joyFireTrg()
{
    if (Key.trg & 0x80) {
        if (StaFlagChk(pG, STA_ACT_DONT_FIRE) || (StaFlagChk(pG, STA_PL_DONT_FIRE))) {
            StaFlagOn(pG, STA_PL_ACTION);
            return 0;
        }
        return 1;
    }
    return 0;
}

// Aim key held for the gun: with the knife-key option off (pSys->Config_flg 0x04000000 clear) only
// Leon / Krauser, and not while the L trigger (knife) is held; the weapon's own keyKamae decides.
// With the option on all gun characters, unless stat 0x1000 (knife key mode).
int joyKamae()
{
    cPlayer* pl = pPL;
    cPlWep* wep;

    if (CfgFlagChk(pSys, CFG_KNIFE_MODE) == 0) {
        switch (pG->pl_type) {
        case 0:
        case 4:
            if (joyLKamae() != 0) {
                return 0;
            }
        case 2:
        case 3:
        case 5:
            break;
        default:
            return 0;
        }
        wep = pPL->Wep;
        if (wep == 0 || wep->m_pWep == 0) {
            pLog->err(0, 0, "joyKamae() PTR ERR");
            return 0;
        }
        // `goto` to the shared `return 0`: with a plain `return 0` jump.c hoists a set over the
        // branch (`li 1; bne end; li 0`) because the following `li 1` sets the same register; the
        // jump to the label gives the target's `beq ret0; li 1; b end`.
        if (wep->m_pWep->keyKamae() == 0) {
            goto ng;
        }
        return 1;
    } else {
        switch (pG->pl_type) {
        case 0:
        case 2:
        case 3:
        case 4:
        case 5:
            break;
        default:
            return 0;
        }
        wep = pl->Wep;
        if (wep == 0 || wep->m_pWep == 0) {
            pLog->err(0, 0, "joyKamae() PTR ERR");
            return 0;
        }
        if (pl->stat & 0x1000) {
            return 0;
        }
        if (wep->m_pWep->keyKamae()) {
            return 1;
        }
    }
ng:
    return 0;
}

// Knife stance key: Key 0x800 for Leon / Krauser (knife-key option off), or the aim key while
// stat 0x1000 with the option on.
int joyLKamae()
{
    cPlayer* pl = pPL;

    if (CfgFlagChk(pSys, CFG_KNIFE_MODE) == 0) {
        if (pG->pl_type == 0 || pG->pl_type == 4) {
            if (Key.on & 0x800) {
                return 1;
            }
        }
    } else {
        if (pG->pl_type == 0 || pG->pl_type == 4) {
            if (pl->stat & 0x1000) {
                if (Key.on & 0x10) {
                    return 1;
                }
            } else {
                return 0;
            }
        }
    }
    return 0;
}

// Player in water: a ripple (effect table entry 0) every 13 frames, a walk splash (1) when he moved
// more than 1000 since the last frame, a run splash (2) above 6000, and a wave push; not while
// carried (Status_flg[1] 0x200000).
void PlWaterProc(cPlayer* pl)
{
    static f32 wavePower = 0.055f;
    static f32 spd0 = 1000.0f;
    static f32 spd1 = 6000.0f;
    static u8 hamonTimer;
    static u8 sibukiTimer;
    static Vec m_PosOldWater;
    f32 dist;

    if (StaFlagChk(pG, STA_PL_BOAT)) {
        return;
    }
    if (pl->m_pEffRoom == 0) {
        pLog->err(2, 0, "PL WATER EFF NOT REGIST");
        return;
    }
    hamonTimer++;
    {
        u8 t = hamonTimer % 13;

        if (t == 0) {
            EstSet((int) pl, -1, 0, 0, pl->m_pEffRoom[0].id, pl->m_pEffRoom[0].type, 0, 0, (u32) pl, (void*) t);
        }
    }
    dist = GetDistance(&m_PosOldWater, &pl->pos);
    if (sibukiTimer) {
        sibukiTimer--;
    } else if (dist > spd1) {
        EstSet((int) pl, -1, 0, 0, pl->m_pEffRoom[2].id, pl->m_pEffRoom[2].type, 0, 0, (u32) pl, (void*) sibukiTimer);
        sibukiTimer = 10;
    } else if (dist > spd0) {
        EstSet((int) pl, -1, 0, 0, pl->m_pEffRoom[1].id, pl->m_pEffRoom[1].type, 0, 0, (u32) pl, (void*) sibukiTimer);
        sibukiTimer = 0x10;
    }
    if (dist > spd0) {
        AddWaterPower(&pPL->pos, wavePower);
    }
    m_PosOldWater = pl->pos;
}

// Back to the idle routine with a footwork (unless in a boat / crouch routine).
void PlMotionReset()
{
    cPlayer* pl = pPL;

    if (pl->r_no_0 != 0) {
        return;
    }
    if (pl->r_no_1 == 0xF) {
        return;
    }
    if (pl->r_no_1 == 0x11) {
        return;
    }
    pl->m_Frame = 0;
    pl->m_Hokan = 0;
    pl->r_no_0 = 0;
    pl->r_no_1 = 0;
    pl->r_no_3 = 1;
    pl->r_no_2 = 0;
}

// May the partner be healed? 1 when within 5000 and in a plain routine-0 state, -1 when busy, 0
// when too far.
int SubCharCheckHealing()
{
    cSubChar* sub;
    // `const f32`: the pool address (`lis r30`) is computed at the declaration and kept across the
    // call while the `lfs` itself is issued after it; a plain `f32` local loads f31 before the call.
    const f32 limit = 25000000.0f;

    if (GetDistance(pPL->pos, pSUB->pos) > limit) {
        return 0;
    }
    sub = pSUB;
    if (sub->r_no_0 != 0) {
        return -1;
    }
    switch (sub->r_no_1) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
    case 6:
    case 7:
    case 0xE:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
        break;
    default:
        return -1;
    }
    return 1;
}

// Partner back to idle with a footwork (only from routine 0/0 or 0/1). Returns 1 when done.
int SubCharMotionReset()
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        return 0;
    }
    if (sub->r_no_0 != 0) {
        return 0;
    }
    if (sub->r_no_1 != 0 && sub->r_no_1 != 1) {
        return 0;
    }
    sub->r_no_0 = 0;
    sub->r_no_1 = 0;
    sub->r_no_3 = 1;
    sub->r_no_2 = 0;
    return 1;
}

// Eye control mode (0 wander, 1 from the motion).
void PlSetEyeMode(u8 mode)
{
    pPL->m_EyeMode = mode;
}

// The player's facing yaw including the waist twist (aim direction).
f32 PlGetDirY()
{
    return pPL->ang.y + pPL->Waist->m_Ang.y;
}

// Room registers the boss enemy and its room flag (the special rocket launcher's insta-kill).
void PlRegistBoss(void* a, void* b)
{
    pPL->m_pBoss = a;
    pPL->m_pBossRmf = b;
}

// Leon wears the armor (costume 2 / 3) — no damage; off with System_flg 0x20.
int PlIsArmor()
{
    if (SysFlagChk(pG, SYS_HARD_MODE)) {
        return 0;
    }
    if (pG->pl_type != 0) {
        return 0;
    }
    return pG->pl_costume == 2 || pG->pl_costume == 3;
}

// Partner-call key (0x200) while Ashley is around (Status_flg[1] bit2) and the player is idle /
// walking: interrupts and starts the whistle routine 0/0x14. Returns 1 when started.
int PlSetWhistle()
{
    cPlayer* pl;

    if (!(Key.trg & 0x200)) {
        return 0;
    }
    if (StaFlagChk(pG, STA_BINOCULAR)) {
        return 0;
    }
    if (!StaFlagChk(pG, STA_SUBCHAR_CTRL)) {
        return 0;
    }
    pl = pPL;
    if ((u32) pl->r_no_1 > 6) {
        return 0;
    }
    if (pl->r_no_1 == 6) {
        switch (pl->r_no_2) {
        case 2:
        case 4:
        case 6:
            return 0;
        }
    }
    pl->interrupt();
    pl->r_no_0 = 0;
    pl->r_no_1 = 0x14;
    pl->r_no_2 = 0;
    pl->r_no_3 = 0;
    return 1;
}

// Weapon in use for hit / camera purposes: 0x10 (knife) while in the knife routine or holding the
// knife key, else weapon_no.
int PlGetWeaponNo()
{
    cPlayer* pl = pPL;

    if (((pl->r_no_0 == 0 && pl->r_no_1 == 0xB) && pl->r_no_2 != 3) || joyLKamae()) {
        return 0x10;
    }
    return pG->weapon_no;
}

// Player face 0 neutral, 1 pain, 2.
void PlSetFace(int no)
{
    int f;

    switch (no) {
    default:
        f = 0;
        break;
    case 1:
        f = 1;
        break;
    case 2:
        f = 2;
        break;
    }
    pPL->setFace(f);
}

// Ashley's face (id 3 only).
void SubCharSetFace(int no)
{
    if (pSUB->id == 3) {
        pSUB->setFace(no);
    }
}

// Frees everything that belongs to the player archive before a reload: the weapon / item objects
// (ids 0x1A, 0x23, 0x29, 0x2A, 0x3A) and the enemies of id 0x4F.
void PlDataRelease()
{
    cObj* obj;
    cObj* objCur;
    cObj* objNext;
    cEm* em;
    cEm* emCur;
    cEm* emNext;

    // Guarded do/while loops testing `next` at the bottom: a `while (obj)` with the switch body
    // is not rotated by expand_end_loop (test at the top, `b top` at the bottom).
    obj = ObjMgr.pAlive;
    if (obj) {
        do {
            objCur = obj;
            objNext = (cObj*) objCur->pNext;
            obj = objNext;
            switch (objCur->id) {
            case 0x1A:
            case 0x23:
            case 0x29:
            case 0x2A:
            case 0x3A:
                ObjMgr.destroy(objCur);
                break;
            }
        } while (objNext);
    }
    em = EmMgr.pAlive;
    if (em) {
        do {
            emCur = em;
            emNext = (cEm*) emCur->pNext;
            em = emNext;
            if (emCur->id == 0x4F) {
                EmMgr.destroy(emCur);
            }
        } while (emNext);
    }
}

// the split object's .sdata is padded to 8 bytes after the three floats
asm(".section .sdata; .balign 8");
