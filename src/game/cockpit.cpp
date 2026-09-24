// game/cockpit.cpp: the in-game HUD (Cckpt): the life meter (player and partner, with the
// green / yellow / red colour templates), the ammo counter with the bullet type icon, the
// count-down timer of the timed sections and the action button icon. Everything is drawn
// through id sprites (IdSys); the cockpit only updates their textures, colours and rotations.
// Original source: D:/Bio4/Prog/cockpit.cpp.
#include "types.h"
#include "global.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "id_sys.h"
#include "item.h"
#include "player.h"
#include "pl_npc.h"
#include "cockpit.h"

Cockpit Cckpt;
int g_boss_bar_flag = 0;

static int dispBulletDigit(u8 no);

// Game start: resets the id (HUD sprite) system.
void Cockpit::gameInit()
{
    IdSys.roomInit();
}

// Room start: resets the id system, creates the HUD frame ids (IDC_CINESCO) and the sub-displays,
// kills a leftover message window.
void Cockpit::roomInit()
{
    IdSys.roomInit();
    IdTexRoomInit();
    IdTexDataLoad(ARC_PTR(ofs_74), TEX_OWNER_ID_COCKPIT);
    IdSys.set(ARC_PTR(ofs_88), 0xFF, IDC_CINESCO, 0x13, 0, 0);
    IdSys.kill(0xFF, IDC_MSG_WINDOW);
    m_ActBttn.roomInit();
    m_LifeMeter.roomInit();
    m_BlltInfo.roomInit();
    m_CountDown.roomInit();
}

// Per-frame HUD update: life meter, bullet counter, count-down and action button when the HUD
// is created (IDC_LIFE_METER set).
void Cockpit::move()
{
    if (FlagChkSign(pG->Debug_flg, DBG_TEST_MODE) && !DbgFlagChk(pG, DBG_EVENT_TOOL)) {
        DbgFlagOn(pG, DBG_COCKPIT_TOOL);
    } else {
        DbgFlagOff(pG, DBG_COCKPIT_TOOL);
    }
    if (IdSys.setCk(IDC_LIFE_METER)) {
        m_LifeMeter.move();
        m_BlltInfo.move();
        m_ActBttn.move();
        m_CountDown.move();
    }
}

// mode 1 shows the message window backdrop ids (IDC_MSG_WINDOW), 0 removes them.
void Cockpit::msgWindow(int sw)
{
    switch (sw) {
    case 1:
        if (!IdSys.setCk(IDC_MSG_WINDOW)) {
            IdSys.set(ARC_PTR(ofs_94), 0xFF, IDC_MSG_WINDOW, 0x13, 0, 0);
        }
        break;
    case 0:
        IdSys.kill(0xFF, IDC_MSG_WINDOW);
        break;
    }
}

// Shows (1) or hides (0) the life meter ids.
void Cockpit::lifeMeterDisp(int sw)
{
    switch (sw) {
    case 1:
        if (IdSys.setCk(IDC_LIFE_METER)) {
            m_LifeMeter.disp(1);
        } else {
            roomInit();
        }
        break;
    case 0:
        if (IdSys.setCk(IDC_LIFE_METER)) {
            m_LifeMeter.disp(0);
        }
        break;
    }
}

// ---------------------------------------------------------------- LifeMeter

static f32 a_ratio = 0.9f;

// Colour smoothing. The cast-then-deref store is not MEM_IN_STRUCT_P, so a_ratio is reloaded
// after every store (the original reloads it per statement); a plain v[i] hoists it out of the loop
// and a reference parameter turns the address into a stepping pointer.
#define FIDX(p, i) (*(f32*) ((u8*) (p) + (i) * 4))
// Moves colour component i of v one step toward the target t (smoothed colour changes).
static inline void approachIdx(f32* v, f32* t, int i)
{
    FIDX(v, i) = a_ratio * FIDX(v, i) + (1.0f - a_ratio) * t[i];
}

// Creates the life meter ids (player, and the partner's when Ashley is present), reads the
// colour templates (units 0x0F..0x11: fine / caution / danger) and starts the smoothed values at
// the current life.
void LifeMeter::roomInit()
{
    IdUnit* u;

    IdSys.set(ARC_PTR(ofs_7C), 0xFF, IDC_LIFE_METER, 0x13, 5, 0);
    IdSys.unitPtr(0x40, IDC_LIFE_METER)->be_flag &= ~8;
    IdSys.unitPtr(0x41, IDC_LIFE_METER)->be_flag &= ~8;
    IdSys.unitPtr(0x42, IDC_LIFE_METER)->be_flag &= ~8;
    switch (pG->pl_type) {
    case 0:
        IdSys.unitPtr(0x40, IDC_LIFE_METER)->be_flag |= 8;
        break;
    case 1:
        IdSys.unitPtr(0x41, IDC_LIFE_METER)->be_flag |= 8;
        break;
    case 2:
        IdSys.unitPtr(0x42, IDC_LIFE_METER)->be_flag |= 8;
        break;
    }
    disp(1);
    m_life = (f32) (s16) pG->pl_life;
    m_life_sub = (f32) (s16) pG->ashley_life;
    u = IdSys.unitPtr(0x11, IDC_LIFE_METER);
    m_state_color0[0][0] = u->col0[0];
    m_state_color0[0][1] = u->col0[1];
    m_state_color0[0][2] = u->col0[2];
    m_state_color0[0][3] = u->col0[3];
    m_state_color1[0][0] = u->col1[0];
    m_state_color1[0][1] = u->col1[1];
    m_state_color1[0][2] = u->col1[2];
    m_state_color1[0][3] = u->col1[3];
    u = IdSys.unitPtr(0x10, IDC_LIFE_METER);
    m_state_color0[1][0] = u->col0[0];
    m_state_color0[1][1] = u->col0[1];
    m_state_color0[1][2] = u->col0[2];
    m_state_color0[1][3] = u->col0[3];
    m_state_color1[1][0] = u->col1[0];
    m_state_color1[1][1] = u->col1[1];
    m_state_color1[1][2] = u->col1[2];
    m_state_color1[1][3] = u->col1[3];
    u = IdSys.unitPtr(0x0F, IDC_LIFE_METER);
    m_state_color0[2][0] = u->col0[0];
    m_state_color0[2][1] = u->col0[1];
    m_state_color0[2][2] = u->col0[2];
    m_state_color0[2][3] = u->col0[3];
    m_state_color1[2][0] = u->col1[0];
    m_state_color1[2][1] = u->col1[1];
    m_state_color1[2][2] = u->col1[2];
    m_state_color1[2][3] = u->col1[3];
    flags = 0;
    move();
}

#define METER_ANGLE(lv, max, range, base) ((f32) (lv) * (range) / (max) + (base))
#define METER_ROT(base, rate, lo) ((base) - ((rate) - (lo)) * 45.0f)

// Per-frame: hides / shows the partner meter, computes the life levels (20 segments for the
// player from pl_life_max, 5 for the partner), turns the meter needles by the smoothed life
// ratio, picks the colour template by life level (green / yellow / red) and eases the meter
// colours toward it, then writes them into the meter ids.
void LifeMeter::move()
{
    f32 a[4];
    f32 b[4];
    f32 c[4];
    f32 d[4];
    IdUnit* u;
    IdUnit* u2;
    IdUnit* u3;
    IdUnit* u4;
    cPlayer* pl = pPL;
    IdUnit* src = 0;
    IdUnit* src2;
    f32 ang;
    f32 rate;
    int i;

    if (pSUB && pSUB->id == 3) {
        IdUnit* p = IdSys.unitPtr(1, IDC_LIFE_METER);
        p->be_flag |= 8;
        p = IdSys.unitPtr(3, IDC_LIFE_METER);
        p->be_flag |= 8;
    } else {
        IdUnit* p = IdSys.unitPtr(1, IDC_LIFE_METER);
        p->be_flag &= ~8;
        p = IdSys.unitPtr(3, IDC_LIFE_METER);
        p->be_flag &= ~8;
    }
    m_life_level = lifeLevel(20, pG->pl_life_max, 1200);
    m_life_level_sub = lifeLevel(5, pG->ashley_life_max, 600);
    ang = METER_ANGLE(m_life_level, 20.0f, -135.0f, -45.0f);
    IdSys.unitPtr(0xFE, IDC_LIFE_METER)->rot0.z = ang;
    ang = METER_ANGLE(m_life_level_sub, 5.0f, 90.0f, 0.0f);
    IdSys.unitPtr(2, IDC_LIFE_METER)->rot0.z = ang;

    m_life = a_ratio * m_life + (1.0f - a_ratio) * (f32) (s16) pG->pl_life;
    m_life_sub = a_ratio * m_life_sub + (1.0f - a_ratio) * (f32) (s16) pG->ashley_life;

    u = IdSys.unitPtr(7, IDC_LIFE_METER);
    u2 = IdSys.unitPtr(8, IDC_LIFE_METER);
    u3 = IdSys.unitPtr(9, IDC_LIFE_METER);
    u->be_flag &= ~8;
    u2->be_flag &= ~8;
    u3->be_flag &= ~8;
    rate = m_life / 400.0f;
    if (rate > 4.0f) {
        u->be_flag |= 8;
        u2->be_flag |= 8;
        u3->be_flag |= 8;
        u->rot0.z = 90.0f;
        u2->rot0.z = 0.0f;
        u3->rot0.z = METER_ROT(0.0f, rate, 4.0f);
    } else if (rate > 2.0f) {
        u->be_flag |= 8;
        u2->be_flag |= 8;
        u->rot0.z = 90.0f;
        u2->rot0.z = METER_ROT(90.0f, rate, 2.0f);
    } else if (rate >= 0.0f) {
        u->be_flag |= 8;
        u->rot0.z = METER_ROT(180.0f, rate, 0.0f);
    }

    u = IdSys.unitPtr(4, IDC_LIFE_METER);
    u2 = IdSys.unitPtr(5, IDC_LIFE_METER);
    u->be_flag &= ~8;
    u2->be_flag &= ~8;
    rate = m_life_sub / 200.0f;
    if (rate > 3.0f) {
        u->be_flag |= 8;
        u2->be_flag |= 8;
        u->rot0.z = 180.0f;
        u2->rot0.z = (rate - 3.0f) * 30.0f - 180.0f;
    } else if (rate >= 0.0f) {
        u->be_flag |= 8;
        u->rot0.z = (rate - 0.0f) * 30.0f + 88.0f;
    }

    switch (pl->getLifeLevel()) {
    case 0:
        for (i = 0; i < 4; i++) {
            a[i] = (f32) m_state_color0[0][i];
            b[i] = (f32) m_state_color1[0][i];
        }
        break;
    case 1:
        for (i = 0; i < 4; i++) {
            a[i] = (f32) m_state_color0[1][i];
            b[i] = (f32) m_state_color1[1][i];
        }
        break;
    case 2:
        for (i = 0; i < 4; i++) {
            a[i] = (f32) m_state_color0[2][i];
            b[i] = (f32) m_state_color1[2][i];
        }
        break;
    }
    for (i = 0; i < 4; i++) {
        approachIdx(m_color0, a, i);
        approachIdx(m_color1, b, i);
    }

    if ((s16) pG->ashley_life > (s16) pG->ashley_life_max * 3 / 4) {
        for (i = 0; i < 4; i++) {
            c[i] = (f32) m_state_color0[0][i];
            d[i] = (f32) m_state_color1[0][i];
        }
    } else if ((s16) pG->ashley_life > (s16) pG->ashley_life_max / 4) {
        for (i = 0; i < 4; i++) {
            c[i] = (f32) m_state_color0[1][i];
            d[i] = (f32) m_state_color1[1][i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            c[i] = (f32) m_state_color0[2][i];
            d[i] = (f32) m_state_color1[2][i];
        }
    }
    for (i = 0; i < 4; i++) {
        approachIdx(m_color0_sub, c, i);
        approachIdx(m_color1_sub, d, i);
    }

    switch (pl->getLifeLevel()) {
    case 0:
        src = IdSys.unitPtr(0x11, IDC_LIFE_METER);
        break;
    case 1:
        src = IdSys.unitPtr(0x10, IDC_LIFE_METER);
        break;
    case 2:
        src = IdSys.unitPtr(0x0F, IDC_LIFE_METER);
        break;
    }
    {
        IdUnit* p = IdSys.unitPtr(0x12, IDC_LIFE_METER);

        p->col0[0] = (u8) m_color0[0];
        p->col0[1] = (u8) m_color0[1];
        p->col0[2] = (u8) m_color0[2];
        p->col0[3] = 0xFF;
        p->col1[0] = (u8) m_color1[0];
        p->col1[1] = (u8) m_color1[1];
        p->col1[2] = (u8) m_color1[2];
        p->col1[3] = 0xFF;
        p->curve[2] = src->curve[2];
        p->loop_flag |= 4;
    }

    if ((s16) pG->ashley_life > (s16) pG->ashley_life_max * 3 / 4) {
        src2 = IdSys.unitPtr(0x11, IDC_LIFE_METER);
    } else if ((s16) pG->ashley_life > (s16) pG->ashley_life_max / 4) {
        src2 = IdSys.unitPtr(0x10, IDC_LIFE_METER);
    } else {
        src2 = IdSys.unitPtr(0x0F, IDC_LIFE_METER);
    }
    {
        IdUnit* p = IdSys.unitPtr(3, IDC_LIFE_METER);

        p->col0[0] = (u8) m_color0_sub[0];
        p->col0[1] = (u8) m_color0_sub[1];
        p->col0[2] = (u8) m_color0_sub[2];
        p->col0[3] = 0xFF;
        p->col1[0] = (u8) m_color1_sub[0];
        p->col1[1] = (u8) m_color1_sub[1];
        p->col1[2] = (u8) m_color1_sub[2];
        p->col1[3] = 0xFF;
        p->curve[2] = src2->curve[2];
        p->loop_flag |= 4;
    }

    u = IdSys.unitPtr(0x13, IDC_LIFE_METER);
    u4 = IdSys.unitPtr(0x14, IDC_LIFE_METER);
    switch (pl->getLifeLevel()) {
    case 0:
        u->be_flag &= ~8;
        u4->be_flag &= ~8;
        break;
    case 1:
        u->be_flag |= 8;
        u4->be_flag &= ~8;
        break;
    case 2:
        u->be_flag &= ~8;
        u4->be_flag |= 8;
        break;
    }

    u = IdSys.unitPtr(0x18, IDC_LIFE_METER);
    u4 = IdSys.unitPtr(0x19, IDC_LIFE_METER);
    if ((s16) pG->ashley_life > (s16) pG->ashley_life_max * 3 / 4) {
        u->be_flag &= ~8;
        u4->be_flag &= ~8;
    } else if ((s16) pG->ashley_life > (s16) pG->ashley_life_max / 4) {
        u->be_flag |= 8;
        u4->be_flag &= ~8;
    } else {
        u->be_flag &= ~8;
        u4->be_flag |= 8;
    }

    if (pSUB) {
        IdSys.unitPtr(0x1A, IDC_LIFE_METER)->be_flag &= ~8;
        IdSys.unitPtr(0x1B, IDC_LIFE_METER)->be_flag &= ~8;
        IdSys.unitPtr(0x1C, IDC_LIFE_METER)->be_flag &= ~8;
        IdSys.unitPtr(0x1D, IDC_LIFE_METER)->be_flag &= ~8;
        IdSys.unitPtr(0x1E, IDC_LIFE_METER)->be_flag &= ~8;
        if (pSUB->id == 3) {
            u32 cond = SubCharGetCondition();
            if (cond & 8) {
                IdSys.unitPtr(0x1C, IDC_LIFE_METER)->be_flag |= 8;
            } else {
                if (cond & 1) {
                    IdSys.unitPtr(0x1E, IDC_LIFE_METER)->be_flag |= 8;
                } else if (cond & 2) {
                    IdSys.unitPtr(0x1A, IDC_LIFE_METER)->be_flag |= 8;
                }
                if (cond & 0x10) {
                    IdSys.unitPtr(0x1D, IDC_LIFE_METER)->be_flag |= 8;
                }
                if (cond & 0x24) {
                    IdSys.unitPtr(0x1B, IDC_LIFE_METER)->be_flag |= 8;
                }
            }
        }
    }
}

// Stops (sw 0) or restarts (sw 1) the meter's id animation timer.
void LifeMeter::fix(int flag)
{
    IdUnit* u = IdSys.unitPtr(0, IDC_LIFE_METER);

    u->be_flag |= 8;
    if (flag == 0) {
        u->rev_flag |= 0xF;
        IdSys.setTime(u, 0);
    } else {
        Hermite1* h = u->curve[0];
        u8 t = (u8) h->key[h->num - 1].t;

        u->rev_flag &= ~0xF;
        IdSys.setTime(u, t);
    }
}

// Shows (1) / hides (0) the meter root id.
void LifeMeter::disp(int sw)
{
    IdUnit* u;

    switch (sw) {
    case 1:
        u = IdSys.unitPtr(0, IDC_LIFE_METER);
        u->rev_flag |= 0xF;
        u->be_flag |= 8;
        break;
    case 0:
        u = IdSys.unitPtr(0, IDC_LIFE_METER);
        u->rev_flag &= ~0xF;
        u->be_flag &= ~8;
        break;
    }
}

// Starts the meter's slide-out animation (rev_flag) when the HUD leaves.
void LifeMeter::frameOut()
{
    IdUnit* u = IdSys.unitPtr(0, IDC_LIFE_METER);

    u->rev_flag &= ~0xF;
    u->be_flag |= 8;
}

// Starts the meter's slide-in animation.
void LifeMeter::frameIn()
{
    IdUnit* u = IdSys.unitPtr(0, IDC_LIFE_METER);

    u->rev_flag |= 0xF;
    u->be_flag |= 8;
}

// ---------------------------------------------------------------- ActionButton

void ActionButton::roomInit()
{
    IdSys.set(ARC_PTR(ofs_80), 1, IDC_ACT_BUTTON, 0x13, 5, 0);
    IdSys.set(ARC_PTR(ofs_80), 0xF0, IDC_ACT_BUTTON, 0x13, 5, 0);
    m_disp_flag_old = 0;
    m_disp_flag = 0;
}

// Per-frame: when the prompt button `no` (set by ActBtn) changed, rebuilds the action button ids
// (frame + the icon for A / B / X / Y / L / R / Z / stick...).
void ActionButton::move()
{
    if (m_disp_flag != m_disp_flag_old) {
        u8 id;

        switch (m_disp_flag) {
        case 6:
            id = 9;
            break;
        case 2:
            id = 2;
            break;
        case 3:
            id = 3;
            break;
        case 4:
            id = 4;
            break;
        case 0xC:
            id = 5;
            break;
        case 0xB:
            id = 6;
            break;
        case 5:
            id = 7;
            break;
        case 9:
            id = 0xA;
            break;
        case 0xA:
            id = 0xB;
            break;
        case 0xD:
            id = 0xC;
            break;
        case 0xE:
            id = 8;
            break;
        case 0xF:
            id = 0xD;
            break;
        case 0x10:
            id = 0xE;
            break;
        case 1:
            id = 0;
            break;
        default:
            id = 0;
            break;
        }
        IdSys.kill(0xFF, IDC_ACT_BUTTON);
        IdSys.set(ARC_PTR(ofs_80), 1, IDC_ACT_BUTTON, 0x13, 5, 0);
        IdSys.set(ARC_PTR(ofs_80), 0xF0, IDC_ACT_BUTTON, 0x13, 5, 0);
        if (m_disp_flag != 0) {
            IdSys.set(ARC_PTR(ofs_80), id, IDC_ACT_BUTTON, 0x13, 5, 0);
        }
    }
    m_disp_flag_old = m_disp_flag;
}

// ---------------------------------------------------------------- BulletInfo

void BulletInfo::roomInit()
{
    m_mark_old = -1;
}

// Per-frame ammo display: the equipped weapon's loaded count as three digit ids (leading zeros
// hidden; the "empty" id when 0), and the bullet type icon (IDC_BLLT_ICON) for the weapon, parented
// to the HUD frame; hidden for weapons without a counter (knife, infinite launchers).
void BulletInfo::move()
{
    u8 digit[3];
    IdUnit* u[3];
    IdUnit* empty;
    int noBullet = 0;
    cItemMgr* im = &ItemMgr;
    int wepNo;
    u16 num;
    u8 mark;
    int i;

    wepNo = WeaponId2WeaponNo(im->weaponId());
    num = im->bulletNum();
    if (num == 0) {
        u16 id;

        if (itemType(im->weaponId()) == 1) {
            id = WeaponId2BulletId(im->weapon()->id, im->weapon()->getBulletType());
        } else {
            id = WeaponId2BulletId(im->weaponId(), 0);
        }
        noBullet = ItemMgr.search(id) == 0;
    }
    for (i = 0; i < 3; i++) {
        digit[i] = num % 10;
        num /= 10;
    }
    empty = IdSys.unitPtr(0x3F, IDC_LIFE_METER);
    empty->be_flag &= ~8;
    u[0] = IdSys.unitPtr(0xB, IDC_LIFE_METER);
    u[0]->tex_flag |= 2;
    u[0]->texNo = digit[0];
    u[1] = IdSys.unitPtr(0xA, IDC_LIFE_METER);
    u[1]->tex_flag |= 2;
    u[1]->texNo = digit[1];
    u[2] = IdSys.unitPtr(0x17, IDC_LIFE_METER);
    u[2]->tex_flag |= 2;
    u[2]->texNo = digit[2];

    mark = dispBulletIconMarkNo(wepNo);
    if (mark == 0xFF) {
        IdSys.kill(0xFF, IDC_BLLT_ICON);
    }
    if (m_mark_old != mark) {
        if (mark == 0xFF) {
            IdSys.kill(0xFF, IDC_BLLT_ICON);
        } else {
            IdSys.kill(0xFF, IDC_BLLT_ICON);
            IdSys.set(ARC_PTR(ofs_98), mark, IDC_BLLT_ICON, 0x13, 5, 0);
            IdUnit* p = IdSys.unitPtr(mark, IDC_BLLT_ICON);
            IdSys.unitParent(IdSys.unitPtr(0x30, IDC_LIFE_METER), p);
        }
    }
    m_mark_old = mark;

    if (dispBulletDigit(wepNo) == 1) {
        if (noBullet == 0) {
            u[0]->be_flag |= 8;
            u[1]->be_flag |= 8;
            u[2]->be_flag |= 8;
        } else {
            u[0]->be_flag &= ~8;
            u[1]->be_flag &= ~8;
            u[2]->be_flag &= ~8;
            empty->be_flag |= 8;
        }
    } else {
        u[0]->be_flag &= ~8;
        u[1]->be_flag &= ~8;
        u[2]->be_flag &= ~8;
    }
    if (digit[2] == 0) {
        u[2]->be_flag &= ~8;
    }
}

// 1 when weapon number `no` shows an ammo count (guns), 0 for the knife / thrown / special
// weapons and while the player is in the no-weapon states.
static int dispBulletDigit(u8 no)
{
    if (pG->pl_type == 1) {
        return 0;
    }
    if (pPL->r_no_0 == 0 && pPL->r_no_1 == 0xF) {
        return 0;
    }
    switch (no) {
    case 1:
    case 2:
    case 3:
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
    case 0xF:
    case 0x11:
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x1C:
    case 0x1D:
    case 0x1F:
    case 0x20:
    case 0x21:
        return 1;
    }
    return 0;
}

// Bullet icon (IDC_BLLT_ICON id) for weapon number `no`: handgun / shotgun / rifle / magnum / TMP /
// launcher / mine... ammo pictures; 0xFF none.
u8 dispBulletIconMarkNo(u8 weapon_no)
{
    if (pG->pl_type == 1) {
        return 0xFF;
    }
    if (pPL->r_no_0 == 0 && pPL->r_no_1 == 0xF) {
        if (pG->room_id == 0x333) {
            return 0xFF;
        }
        return 0x33;
    }
    switch (weapon_no) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 0xB:
    case 0xC:
    case 0xF:
    case 0x11:
        return 0x31;
    case 7:
    case 8:
    case 0x21:
        return 0x37;
    case 9:
    case 0xA:
    case 0x1D:
        return 0x36;
    case 0xD:
        return 0x38;
    case 0xE:
        return 0x39;
    case 0x10:
        return 0x34;
    case 0x13:
    case 0x16:
    case 0x17:
        return 0x32;
    case 0x15:
        return 0x33;
    case 0x19:
    case 0x1F:
    case 0x20:
        return 0x3A;
    }
    return 0xFF;
}

// ---------------------------------------------------------------- CountDown

void CountDown::roomInit()
{
    IdSys.set(ARC_PTR(ofs_84), 0xFF, IDC_COUNT_DOWN, 0x13, 5, 0);
    IdSys.unitPtr(0x10, IDC_COUNT_DOWN)->be_flag &= ~8;
    IdSys.unitPtr(0x10, IDC_COUNT_DOWN)->rev_flag &= ~0xF;
    m_state &= ~TIMER_STA_ALIVE;
    m_state &= ~TIMER_STA_ERASE;
    initTime(0, 0, 0);
    warnTime(0, 0, 0);
}

struct Digits {
    u8 hi;
    u8 lo;
};

// d += v through a reference (keeps the load / store order of the original).
static inline void U32Add(u32& d, u32 v)
{
    d += v;
}

// the inline keeps the two tests apart (fold would merge them into one `andis. 0xa`)
static inline u32 chkFlag5014(u32 b)
{
    return pG->Status_flg[2] & b;
}

// Per-frame count-down (mercenaries / timed events): pauses during events / stops, adds the
// bonus seconds queued in pG->cdown_add_sec, counts m_frame down (unless Debug_flg[1] 0x10000
// or Status_flg[0] 0x40000 freeze it), switches the digits to the warning colour below
// m_warn_frame, and writes minutes / seconds / hundredths into the digit ids (the hundredths
// jitter through a small table so they look busy).
void CountDown::move()
{
    f32 tbl[6] = {0.0f, 1.0f, -1.0f, 0.0f, 1.5f, -0.5f};
    int run = 1;
    IdUnit* u;
    IdUnit* p;
    u32 t;
    Digits d;
    s8 oldTens;
    s8 newTens;
    f32 ft;

    if (getState(TIMER_STA_ALIVE) == 0) {
        run = 0;
    }
    if (run == 0) {
        return;
    }
    if (!chkFlag5014(0x00080000) && !chkFlag5014(0x00020000) &&
        (StaFlagChk(pG, STA_SUSPEND) || (SpfFlagChk(pG, SPF_PL)))) {
        setState(TIMER_STA_PAUSE);
    } else {
        unsetState(TIMER_STA_PAUSE);
    }
    if (pG->time_bonus != 0) {
        m_frame += pG->time_bonus * 30;
        pG->time_bonus = 0;
    }
    if (!DbgFlagChk(pG, DBG_TIMER_STOP) && !StaFlagChk(pG, STA_SUB_SCRN) && !(m_state & TIMER_STA_PAUSE)) {
        if (m_frame != 0) {
            m_frame = m_frame - 1;
        } else {
            m_frame = 0;
        }
    }
    if (m_frame < m_warn_frame) {
        IdUnit* s;

        u = IdSys.unitPtr(0x10, IDC_COUNT_DOWN);
        s = IdSys.unitPtr(8, IDC_COUNT_DOWN);
        u->col0[0] = (u8) s->col[0];
        u->col0[1] = (u8) s->col[1];
        u->col0[2] = (u8) s->col[2];
        u->col0[3] = (u8) s->col[3];
    }
    oldTens = m_centisecond / 10;
    ft = (f32) m_frame * 10.0f / 3.0f + 0.5f;
    t = (u32) ft;
    m_centisecond = t % 100;
    t /= 100;
    m_minute = t / 60;
    m_second = t % 60;
    newTens = m_centisecond / 10;
    if (oldTens != newTens) {
        m_counter = (m_counter + 1) % 6;
    }

    p = IdSys.unitPtr(6, IDC_COUNT_DOWN);
    p->texNo = 0xB;
    p->tex_flag |= 2;
    p = IdSys.unitPtr(7, IDC_COUNT_DOWN);
    p->texNo = 0xC;
    p->tex_flag |= 2;

    d.hi = m_minute / 10;
    d.lo = m_minute % 10;
    p = IdSys.unitPtr(0, IDC_COUNT_DOWN);
    p->texNo = d.hi;
    p->tex_flag |= 2;
    p = IdSys.unitPtr(1, IDC_COUNT_DOWN);
    p->texNo = d.lo;
    p->tex_flag |= 2;

    d.hi = m_second / 10;
    d.lo = m_second % 10;
    p = IdSys.unitPtr(2, IDC_COUNT_DOWN);
    p->texNo = d.hi;
    p->tex_flag |= 2;
    p = IdSys.unitPtr(3, IDC_COUNT_DOWN);
    p->texNo = d.lo;
    p->tex_flag |= 2;

    d.hi = m_centisecond / 10;
    d.lo = m_centisecond % 10;
    p = IdSys.unitPtr(4, IDC_COUNT_DOWN);
    p->texNo = d.hi;
    p->tex_flag |= 2;
    if (ft != 0.0f) {
        t = (u32) ft;
        t = t / 10 * 10;
        ft -= (f32) t;
        d.lo = (u8) (ft + tbl[m_counter]);
        d.lo = d.lo % 10;
    }
    p = IdSys.unitPtr(5, IDC_COUNT_DOWN);
    p->texNo = d.lo;
    p->tex_flag |= 2;
}

// Shows (1) / hides (0) the count-down ids (m_state bit4 = hidden).
void CountDown::disp(int sw)
{
    IdUnit* u;

    switch (sw) {
    case 1:
        u = IdSys.unitPtr(0x10, IDC_COUNT_DOWN);
        u->rev_flag |= 0xF;
        u->be_flag |= 8;
        unsetState(TIMER_STA_ERASE);
        break;
    case 0:
        u = IdSys.unitPtr(0x10, IDC_COUNT_DOWN);
        u->rev_flag &= ~0xF;
        u->be_flag &= ~8;
        setState(TIMER_STA_ERASE);
        break;
    }
}

// Slide-in animation of the count-down frame (30 frames).
void CountDown::frameIn()
{
    IdUnit* u = IdSys.unitPtr(0x10, IDC_COUNT_DOWN);

    u->be_flag |= 8;
    u->rev_flag &= ~0xF;
    IdSys.setTime(u, 0x1E);
}

// Slide-out animation of the count-down frame.
void CountDown::frameOut()
{
    IdSys.unitPtr(0x10, IDC_COUNT_DOWN)->rev_flag |= 0xF;
}

#define TIME_FRAME(m, s, c) ((u32) ((((f32) (m) * 60.0f + (f32) (s)) * 100.0f + (f32) (c)) * 3.0f / 10.0f + 0.5f))

// Sets the remaining time from minutes / seconds / hundredths and starts the count-down.
void CountDown::initTime(int m, int s, int c)
{
    IdUnit* u;

    m_frame = TIME_FRAME(m, s, c);
    m_minute = m;
    m_second = s;
    m_centisecond = c;
    m_counter = 0;
    u = IdSys.unitPtr(0x10, IDC_COUNT_DOWN);
    u->col0[0] = 0xFF;
    u->col0[1] = 0xFF;
    u->col0[2] = 0xFF;
    u->col0[3] = 0xFF;
}

// Sets the remaining time in frames (30 / s).
void CountDown::initTimeFrame(u32 f)
{
    m_frame = f;
}

// The remaining time below which the digits turn to the warning colour.
void CountDown::warnTime(int m, int s, int c)
{
    m_warn_frame = TIME_FRAME(m, s, c);
}

// Dead-stripped in the original (STRIP_UNUSED): only its constant pool survives after warnTime's.
static u32 cockpit_dead_time(int m, int s, int c)
{
    return TIME_FRAME(m, s, c);
}

// Current minutes / seconds / hundredths.
void CountDown::getTime(int* m, int* s, int* c)
{
    *m = m_minute;
    *s = m_second;
    *c = m_centisecond;
}

// Remaining frames.
u32 CountDown::getFrame()
{
    return m_frame;
}

// Saves the display state (before an event hides the HUD).
void CountDown::saveDisp()
{
    int run;

    savedFlags = m_state;
    run = 1;
    if (getState(TIMER_STA_ALIVE) == 0) {
        run = 0;
    }
    if (run) {
        disp(0);
    }
}

// Restores the saved display state.
void CountDown::loadDisp()
{
    int run = 1;

    if (getState(TIMER_STA_ALIVE) == 0) {
        run = 0;
    }
    if (run) {
        if (!(savedFlags & 0x10)) {
            m_state = savedFlags;
            disp(1);
            frameIn();
        }
    }
}
