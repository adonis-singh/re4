// game/item: the inventory (attache case) manager cItemMgr and the item/weapon tables (D:/Bio4/Prog/item.cpp).
// 68/78 byte-identical (DOL sweep 6, 2026-09-10). Shapes fixed there: save/load loop over `s[i]`
// with the array base in a pointer local declared first (the target's `addi r28,sd,4` base + i*12
// giv + a `mr` copy for the second loop), arm's `goto ok/ng` return layout, reload_main's two clamp
// variables (n, m), weaponParts' `if (p == 0) return 0` + `if (cnt++ == no)` (loop.c moves the
// found block behind the early-return barrier), reload's `goto done`, combine's single-set `inv`. Open:
// use (COMPILER-DIFF #6, 11 call tails), get / bulletNum (#6 single-insn tails), partsCombine, load (case
// 1/9 duplicated bodies need the switch value in r11), trigger/init/set_stage2/debugNumDisp/combine
// (register ties).
#include "types.h"
#include "atari.h"
#include "map_obj.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "db_log.h"
#include "item.h"
#include "puzzle.h"
#include "mercenaries.h"
#include "eprintf.h"
#include <stdlib.h>
#include "player.h"
#include "em_dm_val.h"


// One weapon (22 bytes): item id, bullet attribute, weapon number/type, bullet item id, magazine size per
// exclusive tune level (1..7).
struct WepInfo {
    u16 id;         // 0x00
    u8 attr;        // 0x02  ItemWork::bullet >> 13 this row applies to
    u8 no;          // 0x03  weapon number (pG->wep_no)
    u8 type;        // 0x04  weapon type (pG->wep_type)
    u8 x5;
    u16 bulletId;   // 0x06
    u16 charge[7];  // 0x08
};

// Max tune level per type (fire, magazine, speed, exclusive) of a weapon.
struct WepLevelInfo {
    u16 id;
    u8 lv[4];
};

// Two items that combine into a third.
struct CombInfo {
    u16 a;
    u16 b;
    u16 result;
};

// Item put into the case by the set_*() presets.
struct ItemSet {
    u16 id;
    u16 num;
};

// ItemWork::bullet: 3-bit attribute and 13-bit bullet count
#define BULLET(p) ((p)->bullet & 0x1FFF)

// Sets the loaded bullet count (low 13 bits of ItemWork::bullet), keeping the attribute bits.
static inline void setBullet(ItemWork* p, u16 n)
{
    p->bullet = (p->bullet & 0xE000) | (n & 0x1FFF);
}


// display order of the item ids (gld_order)
u16 g_item_order[] = {
    0x7F, 0x7E, 0x7D, 0xFE, 0xA9, 0x04, 0x18, 0x00, 0x07, 0x20, 0x46, 0x0E, 0x02, 0x01, 0x38, 0x23,
    0x25, 0x03, 0x21, 0x27, 0x29, 0x2A, 0x37, 0x2C, 0x94, 0x2D, 0x2E, 0x2F, 0x30, 0x34, 0x36, 0x35,
    0x3F, 0x42, 0x43, 0x44, 0x45, 0xAA, 0x08, 0x09, 0x0A, 0x95, 0x97, 0x06, 0x19, 0x1C, 0x12, 0x13,
    0x14, 0x16, 0xA8, 0x15, 0x05, 0x57, 0x58, 0x89, 0x59, 0x8A, 0x5A, 0x5B, 0x5C, 0x5D, 0x5F, 0x60,
    0x61, 0x5E, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0xB8, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1,
    0xC2,
};
int g_item_order_num = sizeof(g_item_order) / sizeof(g_item_order[0]);

const WepInfo wep_info[] = {
    {0x23, 0, 0x02, 0, 1, 0x04, {10, 13, 16, 19, 22, 25, 0}},
    {0x24, 0, 0x02, 1, 1, 0x04, {10, 13, 16, 19, 22, 25, 0}},
    {0x25, 0, 0x03, 0, 1, 0x04, {8, 10, 12, 15, 18, 22, 0}},
    {0x26, 0, 0x03, 2, 1, 0x04, {8, 10, 12, 15, 18, 22, 0}},
    {0x03, 0, 0x11, 0, 0, 0x04, {15, 18, 21, 24, 27, 30, 100}},
    {0x21, 0, 0x01, 0, 1, 0x04, {10, 13, 16, 20, 24, 28, 0}},
    {0x22, 0, 0x01, 1, 1, 0x04, {10, 13, 16, 20, 24, 28, 0}},
    {0x40, 0, 0x01, 0, 1, 0x04, {10, 13, 16, 20, 24, 28, 0}},
    {0x27, 0, 0x04, 0, 1, 0x04, {15, 18, 21, 25, 30, 35, 0}},
    {0x28, 0, 0x04, 1, 1, 0x04, {15, 18, 21, 25, 30, 35, 0}},
    {0x29, 0, 0x05, 0, 0, 0x00, {6, 8, 10, 12, 0, 0, 0}},
    {0x2C, 0, 0x07, 0, 0, 0x18, {6, 8, 10, 12, 15, 18, 0}},
    {0x2D, 0, 0x08, 0, 0, 0x18, {12, 14, 16, 20, 24, 28, 100}},
    {0x2E, 0, 0x09, 0, 1, 0x07, {5, 7, 9, 12, 15, 18, 0}},
    {0x6B, 0, 0x09, 1, 1, 0x07, {5, 7, 9, 12, 15, 18, 0}},
    {0x99, 0, 0x09, 2, 1, 0x07, {5, 7, 9, 12, 15, 18, 0}},
    {0x2F, 0, 0x0A, 0, 1, 0x07, {10, 12, 14, 17, 20, 24, 0}},
    {0x6C, 0, 0x0A, 1, 1, 0x07, {10, 12, 14, 17, 20, 24, 0}},
    {0x51, 0, 0x0A, 2, 1, 0x07, {10, 12, 14, 17, 20, 24, 0}},
    {0x30, 0, 0x0B, 0, 2, 0x20, {30, 50, 100, 150, 200, 250, 0}},
    {0x31, 0, 0x0B, 1, 2, 0x20, {30, 50, 100, 150, 200, 250, 0}},
    {0x32, 0, 0x0B, 2, 2, 0x20, {30, 50, 100, 150, 200, 250, 0}},
    {0x33, 0, 0x0B, 3, 2, 0x20, {30, 50, 100, 150, 200, 250, 0}},
    {0x36, 0, 0x0E, 0, 1, 0x46, {5, 7, 10, 0, 0, 0, 0}},
    {0xAB, 0, 0x0E, 1, 1, 0x46, {5, 7, 10, 0, 0, 0, 0}},
    {0x2A, 0, 0x06, 0, 1, 0x00, {7, 10, 14, 0, 0, 0, 0}},
    {0x2B, 0, 0x06, 1, 1, 0x00, {7, 10, 14, 0, 0, 0, 0}},
    {0x34, 0, 0x0C, 0, 0, 0x6A, {20, 25, 30, 35, 40, 50, 0xFFFF}},
    {0x35, 0, 0x0D, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x17, 0, 0x0D, 1, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x6D, 0, 0x0D, 2, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x37, 0, 0x0F, 0, 0, 0x1A, {3, 4, 5, 6, 8, 10, 0xFFFF}},
    {0x38, 0, 0x10, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x01, 0, 0x13, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x02, 0, 0x16, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x0E, 0, 0x17, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x800, 0, 0x00, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x08, 0, 0x19, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x09, 0, 0x1F, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x0A, 0, 0x20, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x94, 0, 0x21, 0, 0, 0x18, {7, 9, 11, 13, 15, 17, 0}},
    {0x52, 0, 0x1C, 0, 0, 0x72, {0, 0, 0, 0, 0, 0, 0}},
    {0x3E, 0, 0x0B, 0, 0, 0x20, {30, 50, 100, 150, 200, 250, 0}},
};

const WepLevelInfo wep_level_info[] = {
    {0x23, {6, 3, 3, 6}}, {0x24, {6, 3, 3, 6}}, {0x25, {6, 3, 3, 6}}, {0x26, {6, 3, 3, 6}},
    {0x03, {6, 1, 3, 6}}, {0x21, {6, 3, 3, 6}}, {0x22, {6, 3, 3, 6}}, {0x40, {6, 3, 3, 6}},
    {0x27, {6, 3, 3, 6}}, {0x28, {6, 3, 3, 6}}, {0x29, {6, 1, 3, 4}}, {0x2C, {6, 1, 3, 6}},
    {0x2D, {6, 1, 3, 6}}, {0x94, {6, 1, 3, 6}}, {0x2E, {6, 1, 3, 6}}, {0x6B, {6, 1, 3, 6}},
    {0x99, {6, 1, 3, 6}}, {0x2F, {6, 1, 3, 6}}, {0x6C, {6, 1, 3, 6}}, {0x51, {6, 1, 3, 6}},
    {0x30, {6, 1, 3, 6}}, {0x31, {6, 1, 3, 6}}, {0x32, {6, 1, 3, 6}}, {0x33, {6, 1, 3, 6}},
    {0x3E, {6, 1, 3, 6}}, {0x36, {3, 1, 2, 3}}, {0xAB, {3, 1, 2, 3}}, {0x34, {6, 1, 3, 6}},
    {0x2A, {3, 1, 3, 3}}, {0x2B, {3, 1, 3, 3}}, {0x37, {6, 1, 3, 6}},
};

const CombInfo combination_info[] = {
    {0x21, 0x3F, 0x22}, {0x23, 0x3F, 0x24}, {0x25, 0x42, 0x26}, {0x27, 0x3F, 0x28},
    {0x30, 0x43, 0x32}, {0x31, 0x43, 0x33}, {0x2E, 0x44, 0x6B}, {0x99, 0x44, 0x6B},
    {0x2E, 0xC5, 0x99}, {0x6B, 0xC5, 0x99}, {0x2F, 0x45, 0x6C}, {0x51, 0x45, 0x6C},
    {0x2F, 0xC5, 0x51}, {0x6C, 0xC5, 0x51}, {0x36, 0xAA, 0xAB}, {0x06, 0x06, 0x12},
    {0x12, 0x06, 0x13}, {0x06, 0x19, 0x14}, {0x06, 0x1C, 0x16}, {0x19, 0x1C, 0xA8},
    {0x14, 0x1C, 0x15}, {0x16, 0x19, 0x15}, {0xA8, 0x06, 0x15}, {0x5E, 0x5F, 0x62},
    {0x5E, 0x60, 0x63}, {0x5E, 0x61, 0x64}, {0x63, 0x5F, 0x65}, {0x64, 0x5F, 0x66},
    {0x62, 0x60, 0x65}, {0x64, 0x60, 0x67}, {0x62, 0x61, 0x66}, {0x63, 0x61, 0x67},
    {0x67, 0x5F, 0x68}, {0x66, 0x60, 0x68}, {0x65, 0x61, 0x68}, {0x9A, 0x9B, 0x9D},
    {0x9A, 0x9C, 0x9E}, {0x9D, 0x9C, 0x9F}, {0x9E, 0x9B, 0x9F}, {0xB8, 0xB9, 0xBC},
    {0xB8, 0xBA, 0xBD}, {0xB8, 0xBB, 0xBE}, {0xBD, 0xB9, 0xBF}, {0xBE, 0xB9, 0xC0},
    {0xBC, 0xBA, 0xBF}, {0xBE, 0xBA, 0xC1}, {0xBC, 0xBB, 0xC0}, {0xBD, 0xBB, 0xC1},
    {0xC1, 0xB9, 0xC2}, {0xC0, 0xBA, 0xC2}, {0xBF, 0xBB, 0xC2}, {0xC6, 0xC7, 0xCA},
    {0xC6, 0xC8, 0xCB}, {0xC6, 0xC9, 0xCC}, {0xCB, 0xC7, 0xCD}, {0xCC, 0xC7, 0xCE},
    {0xCA, 0xC8, 0xCD}, {0xCC, 0xC8, 0xCF}, {0xCA, 0xC9, 0xCE}, {0xCB, 0xC9, 0xCF},
    {0xCF, 0xC7, 0xD0}, {0xCE, 0xC8, 0xD0}, {0xCD, 0xC9, 0xD0}, {0xD1, 0xD2, 0xD5},
    {0xD1, 0xD3, 0xD6}, {0xD1, 0xD4, 0xD7}, {0xD6, 0xD2, 0xD8}, {0xD7, 0xD2, 0xD9},
    {0xD5, 0xD3, 0xD8}, {0xD7, 0xD3, 0xDA}, {0xD5, 0xD4, 0xD9}, {0xD6, 0xD4, 0xDA},
    {0xDA, 0xD2, 0xDB}, {0xD9, 0xD3, 0xDB}, {0xD8, 0xD4, 0xDB}, {0xA4, 0xA5, 0xA6},
    {0x3A, 0x69, 0x7A},
};

cItemMgr ItemMgr;

// Heals the target selected by m_to_whom (0 = player, else the partner) by n (1/10 life units,
// clamped to the max); 0 when the target was already full.
int healing(u16 life_add)
{
    if (ItemMgr.getToWhom() == 0) {
        if ((s16) pG->pl_life < (s16) pG->pl_life_max) {
            pG->pl_life = life_add + pG->pl_life;
            if ((s16) pG->pl_life > (s16) pG->pl_life_max) {
                pG->pl_life = pG->pl_life_max;
            }
            return 1;
        }
    } else if (ItemMgr.getToWhom() == 1) {
        if ((s16) pG->ashley_life < (s16) pG->ashley_life_max) {
            pG->ashley_life = life_add + pG->ashley_life;
            if ((s16) pG->ashley_life > (s16) pG->ashley_life_max) {
                pG->ashley_life = pG->ashley_life_max;
            }
            return 1;
        }
    }
    return 0;
}

// Number of max-life upgrades already taken: (max - base) mapped onto `levels` steps.
int lifeLevel(int level_up_num, s16 curr_life_max, int init_life_max)
{
    return (int) ((f32) (level_up_num * (curr_life_max - init_life_max)) / (f32) init_life_max + 0.5f);
}

// Merchant display: firepower of weapon `id` at tune level `level` (1-based) relative to the
// handgun's base (0x36 Mine Thrower: fixed 2/4/6); 0 for non-weapons.
f32 getPowerRatio(ITEM_ID id, int level)
{
    f32 ret;

    if (itemType(id) != 1) {
        return 0.0f;
    }
    if (id == 0x36) {
        switch (level) {
        case 1:
            ret = 2.0f;
            break;
        case 2:
            ret = 4.0f;
            break;
        case 3:
            ret = 6.0f;
            break;
        default:
            ret = 0.0f;
            break;
        }
        return ret;
    }
    return WeaponLevelTbl[WeaponId2WeaponNo(id)][level - 1] / WeaponLevelTbl[2][0];
}

// Merchant display: firing speed of the weapon at the level in seconds (shot frames / 30).
f32 getSpeedRatio(ITEM_ID id, int level)
{
    if (itemType(id) != 1) {
        return 0.0f;
    }
    return PlShotFrameTbl[WeaponId2WeaponNo(id)][level - 1] / 30.0f;
}

// Merchant display: reload time of the weapon at the level in seconds.
f32 getReloadRatio(ITEM_ID id, int level)
{
    if (itemType(id) != 1) {
        return 0.0f;
    }
    return PlReloadSpeedTbl[WeaponId2WeaponNo(id)][level - 1] / 30.0f;
}

// Merchant display: magazine capacity of the weapon at the level.
f32 getBulletRatio(ITEM_ID id, int level)
{
    if (itemType(id) != 1) {
        return 0.0f;
    }
    return (u8) WeaponId2ChargeNum(id, level);
}

// Empties every slot and the availability flags (new game).
void cItemMgr::clear()
{
    ItemWork* p = m_pItem;
    int i;

    for (i = 0; i < m_array_num; i++, p++) {
        p->flags = 0;
    }
    flagclear();
    used_id = 0xFFFF;
}

// New game inventory: the four key items/documents, handgun (0x23) at a fixed case position plus
// the starting supplies (no != 0: debug 100 of each). Returns the case type 0.
int cItemMgr::set_game(int trial_flag)
{
    dump(0x7C);
    dump(0x7D);
    dump(0x7E);
    dump(0x7F);
    get(0x7C, 0);
    get(0x23, 1);
    {
        ItemWork* p = newbie();
        p->x = 2;
        p->y = 1;
        p->orient = 0;
        p->board = 1;
        arm(p);
    }
    get(0x04, 20);
    {
        ItemWork* p = newbie();
        p->x = 7;
        p->y = 0;
        p->orient = 0;
        p->board = 1;
    }
    get(0x05, 1);
    {
        ItemWork* p = newbie();
        p->x = 6;
        p->y = 3;
        p->orient = 0;
        p->board = 1;
    }
    if (trial_flag != 0) {
        get(0x30, 0);
        {
            ItemWork* p = newbie();
            p->x = 12;
            p->y = 1;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = newbie();
            p->num = 100;
            p->x = 17;
            p->y = 0;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = newbie();
            p->num = 100;
            p->x = 17;
            p->y = 2;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = newbie();
            p->num = 100;
            p->x = 17;
            p->y = 4;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = newbie();
            p->num = 100;
            p->x = 13;
            p->y = 4;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = newbie();
            p->num = 100;
            p->x = 9;
            p->y = 4;
            p->orient = 0;
            p->board = 1;
        }
    }
    return 0;
}

#define PUT_TABLE(tbl, flag)                                              \
    for (i = 0; i < (int) (sizeof(tbl) / sizeof(tbl[0])); i++) {          \
        PutInCase(tbl[i].id, tbl[i].num, flag);                           \
    }

#define CHARGE(p) setBullet(p, WeaponId2ChargeNum((p)->id, p->getBulletLevel() + 1))

// Separate Ways start set (no == 2): Punisher, TMP, Rifle with tuned levels, TMP stock, ammo, sprays.
int cItemMgr::set_ada(int no)
{
    if (no == 2) {
        ItemWork* p;
        u16 on;
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x21, 1}, {0x30, 1}, {0x2F, 1}, {0x45, 1}, {0x04, 30}, {0x20, 50}, {0x07, 5}, {0x01, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x21);
        on = 1;
        p->setBulletLevel(3);
        p->setPowerLevel(5);
        p->setSpeedLevel(2);
        p->setReloadLevel(1);
        CHARGE(p);
        p = search(0x30);
        p->setBulletLevel(2);
        p->setPowerLevel(5);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        CHARGE(p);
        p = search(0x2F);
        p->setBulletLevel(1);
        p->setPowerLevel(5);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        CHARGE(p);
        search(0x45)->bullet = searchAt(p);
        search(0x45)->lv = on;
        arm(search(0x21));
    }
    return 0;
}

// Start set by player type: 1 Ashley (nothing here), 2 Ada (Assignment Ada: Punisher/TMP/Rifle),
// 3 Krauser (bow + grenades), 4 HUNK (TMP + rounds), 5 Wesker (handgun, Killer7, Rifle, grenades).
int cItemMgr::set_char(int no)
{
    ItemWork* p;   // one function-scope pointer for every case (case 3 gets r31)

    switch (no) {
    case 0: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x27, 1}, {0x94, 1}, {0x04, 30}, {0x18, 10}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x27);
        p->setBulletLevel(2);
        p->setPowerLevel(4);
        p->setSpeedLevel(1);
        p->setReloadLevel(1);
        CHARGE(p);
        p = search(0x94);
        p->setBulletLevel(3);
        p->setPowerLevel(4);
        p->setSpeedLevel(0);
        p->setReloadLevel(2);
        CHARGE(p);
        arm(search(0x27));
        break;
    }
    case 2: {
        u16 on;
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x21, 1}, {0x30, 1}, {0x2F, 1}, {0x45, 1}, {0x04, 30}, {0x20, 100},
                         {0x07, 5}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x21);
        on = 1;
        p->setBulletLevel(3);
        p->setPowerLevel(6);
        p->setSpeedLevel(2);
        p->setReloadLevel(1);
        CHARGE(p);
        p = search(0x30);
        p->setBulletLevel(2);
        p->setPowerLevel(4);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        CHARGE(p);
        p = search(0x2F);
        p->setBulletLevel(1);
        p->setPowerLevel(5);
        p->setSpeedLevel(1);
        p->setReloadLevel(1);
        CHARGE(p);
        search(0x45)->bullet = searchAt(p);
        search(0x45)->lv = on;
        arm(search(0x21));
        break;
    }
    case 3: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x3E, 1}, {0x20, 50}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x3E);
        p->setBulletLevel(2);
        p->setPowerLevel(4);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        CHARGE(p);
        arm(search(0x3E));
        break;
    }
    case 4: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x52, 1}, {0x72, 20}, {0x72, 10}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        arm(search(0x52));
        break;
    }
    case 5: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x2A, 1}, {0x2F, 1}, {0x3F, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1},
                         {0x01, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x02, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x23);
        p->setBulletLevel(5);
        p->setPowerLevel(6);
        p->setSpeedLevel(2);
        p->setReloadLevel(2);
        CHARGE(p);
        search(0x3F)->bullet = searchAt(p);
        search(0x3F)->lv = 1;
        p = search(0x2A);
        p->setBulletLevel(1);
        p->setPowerLevel(1);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        CHARGE(p);
        p = search(0x2F);
        p->setBulletLevel(5);
        p->setPowerLevel(5);
        p->setSpeedLevel(1);
        p->setReloadLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x2F, p->getBulletLevel() + 1));
        arm(search(0x23));
        break;
    }
    }
    return 0;
}

// Debug / demo inventories for stage 1 rooms (weapons with levels, ammo, pesetas); returns the case type.
int cItemMgr::set_stage1(int no)
{
    int ret = 0;

    switch (no) {
    case 0: {
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x2C, 1}, {0x2E, 1}, {0x30, 1}, {0x04, 50}, {0x18, 10}, {0x07, 10},
                         {0x20, 100}, {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x15, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 1);
        search(0x01)->num = 5;
        search(0x02)->num = 5;
        search(0x0E)->num = 5;
        arm(ItemMgr.search(0x23));
        pG->peseta = 0;
        break;
    }
    case 1: {
        ItemWork* p;
        u16 on;
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x25, 1}, {0x2C, 1}, {0x2E, 1}, {0x30, 1}, {0x44, 1}, {0x04, 50}, {0x18, 10}, {0x07, 10},
                         {0x20, 100}, {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x15, 1}, {0x15, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 1);
        p = search(0x2C);
        on = 1;
        p->setPowerLevel(2);
        p->setBulletLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x2C, 2));
        p = search(0x2E);
        p->setBulletLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x2E, 3));
        search(0x44)->bullet = searchAt(p);
        search(0x44)->lv = on;
        search(0x30);
        search(0x25);
        search(0x01)->num = 5;
        search(0x02)->num = 5;
        search(0x0E)->num = 5;
        arm(ItemMgr.search(0x25));
        pG->peseta = 10000;
        break;
    }
    }
    return ret;
}

// Debug / demo inventories for stage 2 rooms; returns the case type.
int cItemMgr::set_stage2(int no)
{
    int ret = 0;
    ItemWork* p;   // function-scope (case 3 takes r31)

    switch (no) {
    case 0: {
        u16 on;
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x25, 1}, {0x30, 1}, {0x2C, 1}, {0x2E, 1}, {0x04, 50}, {0x20, 100}, {0x18, 10}, {0x07, 10},
                         {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x06, 1}, {0x14, 1}, {0x43, 1}, {0x44, 1}};
        PUT_TABLE(tbl, 1);
        p = search(0x25);
        on = 1;
        p->setBulletLevel(1);
        p->setPowerLevel(1);
        p->setSpeedLevel(1);
        p->setReloadLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x25, 2));
        p = search(0x30);
        p->setBulletLevel(2);
        p->setPowerLevel(2);
        p->setSpeedLevel(0);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x30, 3));
        search(0x43)->bullet = searchAt(p);
        search(0x43)->lv = on;
        p = search(0x2C);
        p->setBulletLevel(2);
        p->setPowerLevel(2);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x2C, 3));
        p = search(0x2E);
        p->setBulletLevel(2);
        p->setPowerLevel(2);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x2E, 3));
        search(0x44)->bullet = searchAt(p);
        search(0x44)->lv = on;
        arm(ItemMgr.search(0x25));
        pG->peseta = 40000;
        break;
    }
    case 1: {
        int i;

        ret = 2;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7E, 0);
        ItemSet tbl[] = {{0x27, 1}, {0x94, 1}, {0x2F, 1}, {0x30, 1}, {0x29, 1}, {0x45, 1}, {0x04, 50}, {0x18, 10},
                         {0x07, 10}, {0x20, 100}, {0x00, 10}, {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x05, 1}, {0x06, 1},
                         {0x06, 1}, {0x15, 1}, {0x15, 1}, {0x15, 1}, {0x15, 1}, {0x15, 1}, {0x15, 1}};
        PUT_TABLE(tbl, 2);
        p = search(0x30);
        p->setBulletLevel(3);
        p->setPowerLevel(4);
        p->setSpeedLevel(0);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x30, 4));
        p = search(0x27);
        p->setBulletLevel(3);
        p->setPowerLevel(3);
        p->setSpeedLevel(1);
        p->setReloadLevel(1);
        p = search(0x2F);
        p->setBulletLevel(3);
        p->setPowerLevel(3);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        p = search(0x94);
        p->setBulletLevel(3);
        p->setPowerLevel(3);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x94, 4));
        p = search(0x29);
        p->setBulletLevel(2);
        p->setPowerLevel(2);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        search(0x01)->num = 2;
        search(0x02)->num = 2;
        search(0x0E)->num = 2;
        arm(ItemMgr.search(0x27));
        pG->peseta = 20000;
        break;
    }
    case 2: {
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x2C, 1}, {0x30, 1}, {0x01, 1}, {0x20, 100}, {0x18, 10}, {0x18, 10}};
        PUT_TABLE(tbl, 1);
        p = search(0x30);
        p->setBulletLevel(1);
        p = search(0x2C);
        p->setBulletLevel(4);
        p->setPowerLevel(0);
        p->setSpeedLevel(0);
        p->setReloadLevel(0);
        CHARGE(p);
        arm(ItemMgr.search(0x2C));
        break;
    }
    case 3: {
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x2E, 1}, {0x01, 1}, {0x04, 50}, {0x07, 10}};
        PUT_TABLE(tbl, 1);
        p = search(0x2E);
        p->setBulletLevel(4);
        p->setPowerLevel(0);
        p->setSpeedLevel(0);
        p->setReloadLevel(0);
        CHARGE(p);
        arm(ItemMgr.search(0x23));
        break;
    }
    }
    return ret;
}

// Debug / demo inventories for stage 3 rooms; returns the case type.
int cItemMgr::set_stage3(int no)
{
    int ret = 0;

    switch (no) {
    case 0: {
        ItemWork* p;
        u16 on;
        int i;

        ret = 2;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7E, 0);
        ItemSet tbl[] = {{0x27, 1}, {0x30, 1}, {0x2D, 1}, {0x29, 1}, {0x2F, 1}, {0x04, 50}, {0x20, 100},
                         {0x18, 10}, {0x00, 10}, {0x07, 10}, {0x01, 1}, {0x43, 1}, {0x45, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 2);
        p = search(0x27);
        on = 1;
        p->setBulletLevel(3);
        p->setPowerLevel(3);
        p->setSpeedLevel(1);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x27, p->getBulletLevel() + 1));
        p = search(0x30);
        p->setBulletLevel(5);
        p->setPowerLevel(5);
        p->setSpeedLevel(2);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x30, p->getBulletLevel() + 1));
        search(0x43)->bullet = searchAt(p);
        search(0x43)->lv = on;
        p = search(0x2D);
        p->setBulletLevel(2);
        p->setPowerLevel(2);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x2D, p->getBulletLevel() + 1));
        p = search(0x29);
        p->setBulletLevel(2);
        p->setPowerLevel(4);
        p->setSpeedLevel(0);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x29, p->getBulletLevel() + 1));
        p = search(0x2F);
        p->setBulletLevel(4);
        p->setPowerLevel(4);
        p->setSpeedLevel(0);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x2F, p->getBulletLevel() + 1));
        search(0x45)->bullet = searchAt(p);
        search(0x45)->lv = on;
        arm(ItemMgr.search(0x27));
        pG->peseta = 40000;
        break;
    }
    case 1: {
        ItemWork* p;
        u16 on;
        int i;

        ret = 2;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7E, 0);
        ItemSet tbl[] = {{0x27, 1}, {0x30, 1}, {0x2D, 1}, {0x29, 1}, {0x2F, 1}, {0x04, 50}, {0x20, 100}, {0x18, 10},
                         {0x00, 10}, {0x07, 10}, {0x01, 1}, {0x43, 1}, {0x45, 1}, {0xC5, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 2);
        p = search(0x27);
        on = 1;
        p->setBulletLevel(3);
        p->setPowerLevel(3);
        p->setSpeedLevel(1);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x27, p->getBulletLevel() + 1));
        p = search(0x30);
        p->setBulletLevel(5);
        p->setPowerLevel(5);
        p->setSpeedLevel(2);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x30, p->getBulletLevel() + 1));
        search(0x43)->bullet = searchAt(p);
        search(0x43)->lv = on;
        p = search(0x2D);
        p->setBulletLevel(2);
        p->setPowerLevel(2);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x2D, p->getBulletLevel() + 1));
        p = search(0x29);
        p->setBulletLevel(2);
        p->setPowerLevel(4);
        p->setSpeedLevel(0);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x29, p->getBulletLevel() + 1));
        p = search(0x2F);
        p->setBulletLevel(4);
        p->setPowerLevel(4);
        p->setSpeedLevel(0);
        p->setReloadLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x2F, p->getBulletLevel() + 1));
        search(0xC5)->bullet = searchAt(p);
        search(0xC5)->lv = on;
        arm(ItemMgr.search(0x27));
        pG->peseta = 40000;
        break;
    }
    }
    return ret;
}

// Shooting range loadouts (0: Red9 + Rifle, 1: handgun + shotgun).
int cItemMgr::set_range(int no)
{
    int ret = 0;

    switch (no) {
    case 0: {
        ItemWork* p;
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x2C, 1}, {0x30, 1}, {0x01, 1}, {0x20, 100}, {0x18, 10}, {0x18, 10}};
        PUT_TABLE(tbl, 1);
        p = search(0x2C);
        p->setReloadLevel(1);
        p->setBulletLevel(3);
        setBullet(p, WeaponId2ChargeNum(0x2C, p->getBulletLevel() + 1));
        p = search(0x30);
        p->setBulletLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x30, p->getBulletLevel() + 1));
        arm(ItemMgr.search(0x2C));
        break;
    }
    case 1: {
        ItemWork* p;
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x2E, 1}, {0x01, 1}, {0x04, 50}, {0x07, 10}};
        PUT_TABLE(tbl, 1);
        p = search(0x23);
        p->setSpeedLevel(2);
        p->setReloadLevel(1);
        p->setBulletLevel(2);
        setBullet(p, WeaponId2ChargeNum(0x23, p->getBulletLevel() + 1));
        p = search(0x2E);
        p->setReloadLevel(1);
        p->setBulletLevel(3);
        setBullet(p, WeaponId2ChargeNum(0x2E, p->getBulletLevel() + 1));
        arm(ItemMgr.search(0x23));
        break;
    }
    }
    return ret;
}

// Debug menu inventories (all weapons, all treasures, all keys, ...).
int cItemMgr::set_debug(int no)
{
    int ret = 0;

    switch (no) {
    case 0: {
        int i;

        ret = set_game(0);
        ItemSet tbl[] = {{0x05, 1}, {0x05, 1}, {0x08, 1}, {0x09, 1}, {0x0A, 1}, {0x95, 1}, {0x97, 1}, {0x06, 1},
                         {0x19, 1}, {0x1C, 1}, {0x14, 1}, {0x16, 1}, {0x15, 1}, {0x12, 1}, {0x13, 1}};
        PUT_TABLE(tbl, ret);
        arm(ItemMgr.search(0x23));
        break;
    }
    case 1: {
        int i;

        ret = 3;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7F, 0);
        ItemSet tbl[] = {{0x25, 1}, {0x30, 1}, {0x2E, 1}, {0x2C, 1}, {0x36, 1}, {0x35, 1}, {0x44, 1}, {0xAA, 1},
                         {0x42, 1}, {0x43, 1}, {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x04, 50}, {0x46, 5}, {0x05, 1}};
        PUT_TABLE(tbl, 3);
        search(0x01)->num = 5;
        search(0x02)->num = 5;
        search(0x0E)->num = 5;
        arm(ItemMgr.search(0x25));
        break;
    }
    case 2: {
        int i;

        ret = 2;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7E, 0);
        ItemSet tbl[] = {{0x2D, 1}, {0x2F, 1}, {0x30, 1}, {0x27, 1}, {0x29, 1}, {0x23, 1}, {0x05, 1}, {0x05, 1},
                         {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x18, 10}, {0x07, 10}, {0x20, 100}, {0x04, 50}, {0x00, 10}};
        PUT_TABLE(tbl, 2);
        search(0x01)->num = 5;
        search(0x02)->num = 5;
        search(0x0E)->num = 5;
        arm(ItemMgr.search(0x23));
        break;
    }
    case 3: {
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x03, 1}, {0x05, 1}, {0x2A, 1}, {0x21, 1}, {0x25, 1}, {0x27, 1},
                         {0x29, 1}, {0x37, 1}, {0x3F, 1}, {0x04, 50}, {0x00, 10}, {0x1A, 10}};
        PUT_TABLE(tbl, 1);
        arm(ItemMgr.search(0x23));
        break;
    }
    case 4: {
        int i;

        ret = 3;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7F, 0);
        ItemSet tbl[] = {{0x30, 1}, {0x34, 1}, {0x2C, 1}, {0x2D, 1}, {0x94, 1},
                         {0x2E, 1}, {0x2F, 1}, {0x20, 100}, {0x18, 10}, {0x07, 10}};
        PUT_TABLE(tbl, 3);
        arm(ItemMgr.search(0x23));
        break;
    }
    case 5: {
        ItemWork* p;
        u16 on;
        int i;

        ret = 3;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7F, 0);
        ItemSet tbl[] = {{0x36, 1}, {0x17, 1}, {0x35, 1}, {0x35, 1}, {0x6D, 1}, {0xAA, 1}, {0x46, 5}, {0x46, 5},
                         {0x46, 5}, {0x46, 5}, {0x46, 5}, {0x46, 5}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x02, 1},
                         {0x02, 1}, {0x02, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1},
                         {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}};
        PUT_TABLE(tbl, 3);
        p = search(0x36);
        on = 1;
        p->setBulletLevel(2);
        p->setPowerLevel(2);
        p->setSpeedLevel(0);
        p->setReloadLevel(1);
        setBullet(p, WeaponId2ChargeNum(0x36, p->getBulletLevel() + 1));
        search(0xAA)->bullet = searchAt(p);
        search(0xAA)->lv = on;
        arm(ItemMgr.search(0x36));
        break;
    }
    case 6: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x04, 50}, {0x18, 15}, {0x20, 100}, {0x07, 10}, {0x00, 10}, {0x04, 50}, {0x18, 15}, {0x20, 100},
                         {0x07, 10}, {0x00, 10}, {0x04, 50}, {0x18, 15}, {0x20, 100}, {0x07, 10}, {0x00, 10}, {0x04, 50},
                         {0x18, 15}, {0x20, 100}, {0x07, 10}, {0x00, 10}, {0x04, 50}, {0x18, 15}, {0x20, 100}, {0x07, 10},
                         {0x00, 10}, {0x04, 50}, {0x18, 15}, {0x20, 100}, {0x07, 10}, {0x00, 10}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    case 7: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x05, 1}, {0x15, 1}, {0xA8, 1}, {0x16, 1}, {0x14, 1}, {0x13, 1}, {0x12, 1}, {0x1C, 1},
                         {0x19, 1}, {0x06, 1}, {0x05, 1}, {0x15, 1}, {0xA8, 1}, {0x16, 1}, {0x14, 1}, {0x13, 1},
                         {0x12, 1}, {0x1C, 1}, {0x19, 1}, {0x06, 1}, {0x05, 1}, {0x15, 1}, {0xA8, 1}, {0x16, 1},
                         {0x14, 1}, {0x13, 1}, {0x12, 1}, {0x1C, 1}, {0x19, 1}, {0x06, 1}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    case 8: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1},
                         {0x01, 1}, {0x01, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1},
                         {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1},
                         {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    case 9: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1},
                         {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1},
                         {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1},
                         {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1},
                         {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1},
                         {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1},
                         {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1},
                         {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    case 10: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x97, 1}, {0x97, 1}, {0x97, 1}, {0x95, 1}, {0x95, 1}, {0x95, 1},
                         {0x95, 1}, {0x95, 1}, {0x95, 1}, {0x95, 1}, {0x95, 1}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    }
    return ret;
}

// Debug inventory preset dispatcher (Debug_flg[3] set-ups): maps the preset number onto set_game /
// set_stage1..3 / set_range / set_debug; returns the case type used.
int cItemMgr::setUp(int set_no)
{
    int ret = 0;

    switch (set_no) {
    case 0:
        ret = set_game(0);
        break;
    case 1:
        ret = set_game(1);
        break;
    case 2:
        ret = set_stage1(0);
        break;
    case 3:
        ret = set_stage1(1);
        break;
    case 4:
        ret = set_stage2(0);
        break;
    case 5:
        ret = set_stage2(1);
        break;
    case 6:
        ret = set_stage3(0);
        break;
    case 7:
        ret = set_stage3(1);
        break;
    case 8:
        ret = set_range(0);
        break;
    case 9:
        ret = set_range(1);
        break;
    case 10:
        ret = set_debug(0);
        break;
    case 11:
        ret = set_debug(1);
        break;
    case 12:
        ret = set_debug(2);
        break;
    case 13:
        ret = set_debug(3);
        break;
    case 14:
        ret = set_debug(4);
        break;
    case 15:
        ret = set_debug(5);
        break;
    case 16:
        ret = set_debug(6);
        break;
    case 17:
        ret = set_debug(7);
        break;
    case 18:
        ret = set_debug(8);
        break;
    case 19:
        ret = set_debug(9);
        break;
    case 20:
        ret = set_debug(10);
        break;
    case 21:
        ret = set_char(0);
        break;
    case 22:
        ret = set_char(2);
        break;
    }
    return ret;
}


// Game start: clears the inventory and gives the start set (set_game for Leon, set_ada / set_char for
// the extra modes; debug presets via Debug_flg[3]) with 0 pesetas.
void cItemMgr::gameInit()
{
    clear();
    roomInit();
    if (!((s32) pG->System_flg < 0) && !(pG->System_flg & 0x40000000)) {
        if (pG->pl_type == 1) {
            m_char = 0;
        }
        set_game(0);
        pG->peseta = pG->peseta_bak = 0;
        get(0xAC, 1);
        get(0xAD, 1);
        if (FlagChkSign(pG->Debug_flg, DBG_START_ST2) || FlagChkSign(pG->Debug_flg, DBG_START_ST3)) {
            get(0xAE, 1);
            get(0xAF, 1);
            get(0xB0, 1);
            get(0xB1, 1);
            get(0xB2, 1);
            get(0xB3, 1);
            get(0xB4, 1);
            get(0xB5, 1);
            get(0xB6, 1);
            get(0xB7, 1);
        }
        if (DbgFlagChk(pG, DBG_START_ST3)) {
            get(0x48, 1);
            get(0x49, 1);
            get(0x4A, 1);
            get(0x4B, 1);
            get(0x4C, 1);
            get(0x4D, 1);
            get(0x4E, 1);
            get(0x4F, 1);
            get(0x50, 1);
            get(0xF4, 1);
            if (DbgFlagChk(pG, DBG_START_LAST)) {
                get(0xF5, 1);
                get(0xF6, 1);
                get(0xF7, 1);
                get(0xF8, 1);
                get(0xF9, 1);
                get(0xFA, 1);
                get(0xFB, 1);
                get(0xFC, 1);
                get(0xFD, 1);
            }
        }
        if (pG->pl_type == 1) {
            m_char = 1;
        }
    } else {
        if (SysFlagChk(pG, SYS_OMAKE_ADA_GAME)) {
            set_ada(2);
        } else if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
            set_char(pG->pl_type);
        }
    }
}

// Room init: clears the availability flags and selects the inventory set by player type.
void cItemMgr::roomInit()
{
    flagclear();
    if (pG->pl_type == 1) {
        m_char = 1;
    } else {
        m_char = 0;
    }
}


// Boot: allocates the 0x180 slots, the ordering table and the 256-bit availability mask.
int cItemMgr::init()
{
    ItemWork* p;
    int i;
    u32 sz;

    m_array_num = 0x180;
#line 2508 "D:/Bio4/Prog/item.cpp"
    m_pItem = (ItemWork*) MEM_ALLOC(0x180 * sizeof(ItemWork), 1, 13);
    m_p_order_tbl = (ItemOrder*) MEM_ALLOC(m_array_num * sizeof(ItemOrder), 1, 13);
    if (m_pItem == 0) {
        return 0;
    }
    p = m_pItem;
    for (i = 0; i < m_array_num; i++, p++) {
        p->flags = 0;
        // COMPILER-DIFF: #13 (the flag-table size as a reload-materialised constant, weight 0 like the
        // string addi): set inside the loop body, the single set does not dominate the call block, so
        // gcse cprop leaves `sz` a pseudo (sched1 ranks the r3 copy at weight 0 next to the stw) and
        // update_equiv_regs substitutes the constant afterwards.
        sz = 8 * sizeof(u32);
    }
    m_flag_num = 8;
#line 2522 "D:/Bio4/Prog/item.cpp"
    m_pAvailable = (u32*) MEM_ALLOC(sz, 1, 13);
    if (m_pAvailable == 0) {
        Mem_free(m_pItem);
        Mem_free(m_p_order_tbl);
        return 0;
    }
    flagclear();
    used_id = 0xFFFF;
    return 1;
}

// Static item table: type (1 weapon, 2 ammo, 3 knife-like, 5/12 treasure, 6 grenade, 7 map, 8 money,
// 9 weapon part, 10 file, 11 key, 13 gem, 14 ...), default pick-up count and max per slot for every id.
void itemInfo(ITEM_ID id, ItemInfo* info)
{
    switch (id) {
    case 0x03:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x36:
    case 0x37:
    case 0x3E:
    case 0x40:
    case 0x51:
    case 0x52:
    case 0x6B:
    case 0x6C:
    case 0x94:
    case 0x99:
    case 0xAB:
        info->type = 1;
        info->defNum = 0;
        info->maxNum = 0;
        break;
    case 0x01:
    case 0x02:
    case 0x0E:
    case 0x17:
    case 0x35:
    case 0x38:
    case 0x6D:
        info->type = 3;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0x04:
        info->type = 2;
        info->defNum = 10;
        info->maxNum = 50;
        break;
    case 0x00:
        info->type = 2;
        info->defNum = 5;
        info->maxNum = 10;
        break;
    case 0x18:
        info->type = 2;
        info->defNum = 10;
        info->maxNum = 15;
        break;
    case 0x07:
        info->type = 2;
        info->defNum = 5;
        info->maxNum = 10;
        break;
    case 0x6A:
        info->type = 2;
        info->defNum = 50;
        info->maxNum = 100;
        break;
    case 0x20:
        info->type = 2;
        info->defNum = 50;
        info->maxNum = 100;
        break;
    case 0x1A:
        info->type = 2;
        info->defNum = 10;
        info->maxNum = 10;
        break;
    case 0x46:
        info->type = 2;
        info->defNum = 1;
        info->maxNum = 5;
        break;
    case 0xA0:
        info->type = 2;
        info->defNum = 5;
        info->maxNum = 10;
        break;
    case 0x72:
        info->type = 2;
        info->defNum = 5;
        info->maxNum = 20;
        break;
    case 0x3F:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0xAA:
    case 0xC5:
        info->type = 9;
        info->defNum = 0;
        info->maxNum = 0;
        break;
    case 0x05:
    case 0x08:
    case 0x09:
    case 0x0A:
        info->type = 6;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0x06:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x19:
    case 0x1C:
    case 0x95:
    case 0x97:
    case 0xA8:
        info->type = 6;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0x56:
    case 0x57:
    case 0x5F:
    case 0x60:
    case 0x61:
    case 0x77:
    case 0xA1:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xC7:
    case 0xC8:
    case 0xC9:
    case 0xD2:
    case 0xD3:
    case 0xD4:
        info->type = 12;
        info->defNum = 1;
        info->maxNum = 999;
        break;
    case 0x1B:
    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B:
    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x70:
    case 0x89:
    case 0x8A:
    case 0x8F:
    case 0x90:
    case 0x91:
    case 0x93:
    case 0x96:
    case 0x98:
    case 0x9A:
    case 0x9B:
    case 0x9C:
    case 0x9D:
    case 0x9E:
    case 0x9F:
    case 0xA2:
    case 0xB8:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xC6:
    case 0xCA:
    case 0xCB:
    case 0xCC:
    case 0xCD:
    case 0xCE:
    case 0xCF:
    case 0xD0:
    case 0xD1:
    case 0xD5:
    case 0xD6:
    case 0xD7:
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
        info->type = 5;
        info->defNum = 1;
        info->maxNum = 999;
        break;
    case 0x78:
        info->type = 8;
        info->defNum = 30;
        info->maxNum = 999;
        break;
    case 0x79:
        info->type = 8;
        info->defNum = 30;
        info->maxNum = 999;
    case 0x71:
        info->type = 8;
        info->defNum = 5;
        info->maxNum = 999;
        break;
    case 0x73:
        info->type = 8;
        info->defNum = 30;
        info->maxNum = 999;
        break;
    case 0x75:
        info->type = 8;
        info->defNum = 30;
        info->maxNum = 999;
        break;
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
    case 0x50:
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB7:
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
        info->type = 10;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0x54:
    case 0x55:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
    case 0xA9:
    case 0xFE:
        info->type = 11;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    case 0xDC:
    case 0xDD:
    case 0xDE:
    case 0xDF:
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE5:
    case 0xE6:
    case 0xE7:
    case 0xE8:
    case 0xE9:
    case 0xEA:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
    case 0xF0:
    case 0xF1:
    case 0xF2:
    case 0xF3:
        info->type = 13;
        info->defNum = 1;
        info->maxNum = 999;
        break;
    case 0x0C:
        info->type = 14;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    default:
        info->type = 7;
        info->defNum = 1;
        info->maxNum = 1;
        break;
    }
}

// Initialises a slot for item `id`: weapons get their level nibbles (special fixed ones: 0x21
// Punisher upgrade from Scenario_flg, 0x3E, ...) and a full first magazine, parts start detached,
// files record the count of files owned.
void cItemMgr::construct(ItemWork* p, ITEM_ID id)
{
    p->flags = 1;
    p->id = id;
    p->num = 0;
    p->board = 0;
    p->x = 0;
    p->y = 0;
    p->orient = 0;
    p->type = m_char;
    if (itemType(id) == 1) {
        switch (id) {
        case 0x40:
            p->id = 0x21;
            if (ScfFlagChk(pG, SCF_ST1_SUB_PERFECT)) {
                p->setPowerLevel(1);
            } else {
                p->setPowerLevel(0);
            }
            p->setSpeedLevel(0);
            p->setReloadLevel(0);
            p->setBulletLevel(0);
            break;
        case 0x34:
            p->setBulletLevel(5);
            p->setPowerLevel(6);
            p->setSpeedLevel(0);
            p->setReloadLevel(2);
            break;
        default:
            p->setBulletLevel(0);
            p->setPowerLevel(0);
            p->setSpeedLevel(0);
            p->setReloadLevel(0);
            asm volatile("");
            break;
        }
        p->bullet = BULLET(p);
        setBullet(p, WeaponId2ChargeNum(id, 1));
    }
    if (itemType(id) == 9) {
        p->lv = 0;
        p->bullet = 0xFFFF;
    }
    if (itemType(id) == 10) {
        p->lv8[0] = countFiles();
    }
}

// Slot by index (0 when out of range).
ItemWork* cItemMgr::at(int i)
{
    if (i < m_array_num) {
        return &m_pItem[i];
    }
    return 0;
}

// Index of a slot (-1 when not in the pool).
int cItemMgr::searchAt(ItemWork* p)
{
    int i;

    for (i = 0; i < m_array_num; i++) {
        if (p == &m_pItem[i]) {
            return i;
        }
    }
    return -1;
}

// Sort key of a slot for the treasure list: its position in g_item_order (0xFF unknown).
u8 gld_order(u8 no)
{
    int n = g_item_order_num;
    int i;

    if (ItemMgr.at(no) == 0) {
        return 0xFF;
    }
    for (i = 0; i < n; i++) {
        if (g_item_order[i] == ItemMgr.at(no)->id) {
            return i;
        }
    }
    return 0xFF;
}

// qsort comparator for the treasure list (by g_item_order).
int gld_cmp(const void* a, const void* b)
{
    return gld_order(*(u8*) a) - gld_order(*(u8*) b);
}

// Builds the sub-screen's slot list: all == 0 -> case items first (*pNum), then treasures (*pNum2)
// sorted by gld_order; all != 0 -> every slot index (0xFF for the other set). Returns the count.
int cItemMgr::makeItemList(u8* p_list, int flag, s8* key_cnt, s8* gld_cnt)
{
    ItemWork* p = m_pItem;
    int cnt = 0;
    int i;

    *gld_cnt = 0;
    *key_cnt = 0;
    for (i = 0; i < m_array_num; i++, p++) {
        if (flag == 0) {
            if (p->isAlive(m_char)) {
                switch (itemType(p->id)) {
                case 5:
                case 12:
                    (*gld_cnt)++;
                    break;
                case 0:
                case 7:
                    p_list[*key_cnt] = i;
                    (*key_cnt)++;
                    break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 6:
                case 8:
                case 9:
                    break;
                }
            }
        } else {
            if (p->isAlive(m_char) == 0) {
                p_list[i] = 0xFF;
            } else {
                p_list[i] = i;
            }
            cnt++;
        }
    }
    if (flag == 0) {
        int j = 0;

        cnt = *key_cnt + *gld_cnt;
        p = m_pItem;
        for (i = 0; i < m_array_num; i++, p++) {
            if (p->isAlive(m_char)) {
                u8 type = itemType(p->id);

                if (type == 5 || type == 12) {
                    p_list[*key_cnt + j] = i;
                    j++;
                }
            }
        }
        qsort(p_list + *key_cnt, *gld_cnt, 1, gld_cmp);
    }
    return cnt;
}

// First slot holding item `id` in the current set.
ItemWork* cItemMgr::search(u16 id)
{
    ItemWork* p = m_pItem;
    int i;

    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isAlive(m_char) && p->id == id) {
            return p;
        }
    }
    return 0;
}

// Slot holding item `id` with the smallest count (the ammo box to use up first).
ItemWork* cItemMgr::minimumSearch(ITEM_ID id)
{
    ItemWork* p = m_pItem;
    ItemWork* best = 0;
    int min = 10000000;
    int i;

    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isAlive(m_char) && id == p->id && p->num < min) {
            min = p->num;
            best = p;
        }
    }
    return best;
}

// qsort comparator: descending count.
int order_cmp(const void* a, const void* b)
{
    return ((ItemOrder*) b)->num - ((ItemOrder*) a)->num;
}

// Fills m_p_order_tbl with the slots of item `id` sorted by descending count (reload order).
void cItemMgr::ordering(ITEM_ID id)
{
    ItemWork* p = m_pItem;
    int n = 0;
    int i;

    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isAlive(m_char) && id == p->id) {
            m_p_order_tbl[n].p_item = p;
            m_p_order_tbl[n].num = p->num;
            n++;
        }
    }
    m_order_tbl_num = n;
    qsort(m_p_order_tbl, n, sizeof(ItemOrder), order_cmp);
}

// Adds n pesetas (capped at 99,999,999).
int addMoney(int n)
{
    pG->peseta = pG->peseta + n;
    if ((s32) pG->peseta > 99999998) {
        pG->peseta = 99999999;
    }
    return 1;
}

// Inline wrappers: the caller's `&inf` is substituted into the hard-register argument set (recomputed
// `addi r4,r1,16` per call, not PRE'd into a callee-saved register).
static inline void itemInfoIW(ITEM_ID id, ItemInfo* inf)
{
    itemInfo(id, inf);
}


// Picks up `num` of item `id` (0 = the item's default count): money ids 0x7C..0x7E become pesetas,
// 0xF? bonus time/points go to the mercenaries timer, stackables top up an existing slot up to
// maxNum (0 when full), otherwise a new slot is constructed (m_pNew). Returns 0 when nothing was taken.
int cItemMgr::get(ITEM_ID id, int num)
{
    ItemInfo info;
    ItemInfo inf;
    ItemInfo* pInfo = &info;
    ItemWork* p;
    int max;
    int i;

    switch (id) {
    case 0x78:
        if (num != 0) {
            return addMoney(num * 10);
        }
        return addMoney(300);
    case 0x79:
        if (num != 0) {
            return addMoney(num * 100);
        }
        return addMoney(3000);
    case 0x71:
        if (num != 0) {
            return addMoney(num * 1000);
        }
        return addMoney(5000);
    case 0x73:
        m_bonus_time = num;
        MercSysSetAddTime(num);
        pG->time_bonus = m_bonus_time;
        return 1;
    case 0x75:
        if (num == 0) {
            num = 30;
        }
        m_bonus_point = num * 30;
        MercSysSetBonusTime(num * 30);
        return 1;
    }
    itemInfoIW(id, &inf);
    switch (inf.type) {
    case 5:
    case 12:
    case 13:
        p = m_pItem;
        for (i = 0; i < m_array_num; i++, p++) {
            if (p->isAlive(m_char) && id == p->id) {
                int total;

                if (num == 0) {
                    itemInfoIW(id, &inf);
                    num = inf.defNum;
                }
                total = p->num + num;
                itemInfo(p->id, &inf);
                if (total <= inf.maxNum) {
                    {
                        int t = num + p->num;

                        p->num = t;
                    }
                    return 1;
                }
                return 0;
            }
        }
        break;
    }
    itemInfo(id, pInfo);
    if (pInfo->type == 1 || pInfo->type == 9) {
        num = 1;
        max = 1;
    } else {
        if (num == 0) {
            num = pInfo->defNum;
        }
        max = pInfo->maxNum;
    }
    if (num > max) {
        num = (u16) max;
        pLog->err(0, 0, "cItemMgr::get(): Volume of ITEM(0x%02x) is OOL.", id);
    }
    m_pNew = 0;
    p = m_pItem;
    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isEmpty()) {
            m_pNew = p;
            construct(p, id);
            p->num = num;
            break;
        }
    }
    if (m_pNew == 0) {
        pLog->err(0, 0, "cItemMgr::get() Can't create item.");
        return 0;
    }
    return 1;
}

// 1 when the item is used on the partner (m_to_whom) or the player type is Ashley.
static inline int useSubChar(cItemMgr* m)
{
    if (m->getToWhom() == 0) {
        return pG->pl_type == 1;
    }
    return 1;
}

// Uses an item from the sub screen: herbs/eggs/sprays heal (fail when life is full), the yellow
// herb mixes raise the max life (20 steps for the player, 5 for Ashley), keys/documents mark
// used_id for the scenario check, and consumables are removed when their count hits 0. Returns 0
// when the item could not be used.
int cItemMgr::use(ItemWork* p)
{
    if (p == 0) {
        return 0;
    }
    if (itemType(p->id) == 1) {
    ng:
        return 0;
    }
    if (p->num == 0) {
        goto ng;
    }
    switch (p->id) {
    case 0x01:
    case 0x02:
    case 0x0E:
    case 0x17:
    case 0x35:
        p->num--;
        if (DbgFlagChk(pG, DBG_INF_BULLET2)) {
            if (p->num != 0) {
                return 1;
            }
            p->num = 5;
        }
        goto chk;
    case 0x15:
    case 0x16: {
        int hp = 0;
        int ok = 0;
        int heal = 0;

        if (!useSubChar(this)) {
            int level = lifeLevel(20, pG->pl_life_max, 1200);

            if (level <= 19) {
                level++;
                pG->pl_life_max = 1200;
                pG->pl_life_max += (int) ((f32) (level * 60) + 0.5f);
                ok = 1;
            }
        } else if (pG->pl_type == 1) {
            int level = lifeLevel(5, pG->pl_life_max, 600);

            if (level <= 4) {
                level++;
                pG->pl_life_max = 600;
                pG->pl_life_max += (int) ((f32) (level * 120) + 0.5f);
                ok = 1;
            }
        } else {
            int level = lifeLevel(5, pG->ashley_life_max, 600);

            if (level <= 4) {
                level++;
                pG->ashley_life_max = 600;
                pG->ashley_life_max += (int) ((f32) (level * 120) + 0.5f);
                ok = 1;
            }
        }
        switch (p->id) {
        case 0x16:
            heal = 600;
            break;
        case 0x15:
            heal = 2400;
            break;
        }
        if (heal != 0) {
            if (healing(heal) != 0) {
                hp = 1;
            }
        }
        if (ok == 0 && hp == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    }
    case 0x08:
        if (healing(400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x09:
        if (healing(800) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x0A:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x05:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x95:
        if (healing(900) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x97:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x06:
        if (healing(600) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x12:
        if (healing(1300) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x13:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x14:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    default:
        {
            int no = p->id;

            if (!(m_pAvailable[no >> 5] & (0x80000000 >> (no & 0x1F)))) {
                return 0;
            }
        }
        flagclear();
        used_id = p->id;
        if (p->id == 0x84 || p->id == 0x92) {
            goto chk;
        }
        break;
    }
    p->num--;
chk:
    if (p->num == 0) {
        p->flags = 0;
    }
    return 1;
}

// Frees a slot; a weapon also detaches its parts and, if it was armed, re-arms the bare hand.
void cItemMgr::erase(ItemWork* p)
{
    p->flags = 0;
    if (itemType(p->id) == 1) {
        int idx = ItemMgr.searchAt(p);
        ItemWork* q = m_pItem;
        int i;

        for (i = 0; i < m_array_num; i++, q++) {
            if (q->isAlive(m_char)) {
                if (itemType(q->id) == 9 && q->lv == 1 && idx == q->bullet) {
                    q->lv = 0;
                    q->bullet = 0xFFFF;
                }
            }
        }
        if (p == weapon()) {
            ItemMgr.arm(0);
        }
    } else {
        if (itemType(ItemMgr.weaponId()) == 1) {
            ItemMgr.arm(ItemMgr.weapon());
        }
    }
}

// Discards the first slot of item `id`.
int cItemMgr::dump(ITEM_ID id)
{
    return dump(search(id));
}

// Discards a slot (erase); 0 when p is NULL.
int cItemMgr::dump(ItemWork* p)
{
    if (p == 0) {
        return 0;
    }
    if (p->num != 0) {
        p->num--;
    }
    if (p->num == 0) {
        erase(p);
    }
    return 1;
}

// Discards a slot regardless of count.
int cItemMgr::dumpAll(ItemWork* p)
{
    if (p == 0) {
        return 0;
    }
    p->num = 0;
    erase(p);
    return 1;
}

// Discards every item of ItemInfo type t (e.g. type 7 maps at a new round).
int cItemMgr::dumpType(int type)
{
    ItemWork* p = m_pItem;
    int i;

    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isAlive(m_char)) {
            if (type == itemType(p->id)) {
                dumpAll(p);
            }
        }
    }
    return 1;
}

// Total count of item `id` in inventory set t.
u16 cItemMgr::num(int id, u8 type)
{
    ItemWork* p = m_pItem;
    u16 n = 0;
    int i;

    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isAlive(type) && p->id == id) {
            n += p->num;
        }
    }
    return n;
}

// Total count of item `id` in the current set.
u16 cItemMgr::num(int id)
{
    ItemWork* p = m_pItem;
    u16 n = 0;
    int i;

    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isAlive(m_char) && p->id == id) {
            n += p->num;
        }
    }
    return n;
}

// Count of a slot (0 for NULL).
u16 cItemMgr::num(ItemWork* p)
{
    if (p == 0) {
        return 0;
    }
    return p->num;
}

// 1 when the item appears in the combination table (can be combined with something).
int itemCombineCheck(ITEM_ID id)
{
    int i;

    for (i = 0; i < (int) (sizeof(combination_info) / sizeof(combination_info[0])); i++) {
        if (id == combination_info[i].a || id == combination_info[i].b) {
            return 1;
        }
    }
    return 0;
}

// Looks up the combination of two ids in either order; *result = the product. 0 when none.
int itemCombine(ITEM_ID srcA, ITEM_ID srcB, u16* dst)
{
    int i;

    for (i = 0; i < (int) (sizeof(combination_info) / sizeof(combination_info[0])); i++) {
        if (srcA == combination_info[i].a && srcB == combination_info[i].b) {
            *dst = combination_info[i].result;
            return 1;
        }
    }
    for (i = 0; i < (int) (sizeof(combination_info) / sizeof(combination_info[0])); i++) {
        if (srcB == combination_info[i].a && srcA == combination_info[i].b) {
            *dst = combination_info[i].result;
            return 1;
        }
    }
    return 0;
}

// Combines two slots: weapon + its ammo (flag != 0) reloads it or, for the other ammo attribute,
// swaps the loaded ammo type (bullet_type follows when armed); weapon + part attaches the part;
// same stackable ids merge up to maxNum; otherwise the combination table (herb mixes etc.)
// replaces a with the product. Returns 1 when something happened.
int cItemMgr::combine(ItemWork* a, ItemWork* b, int flag)
{
    u16 newId;
    int ret = 0;
    u16 ida;
    u16 idb;

    if (a == 0 || b == 0) {
        return 0;
    }
    if (a == b && b->num <= 1) {
        return 0;
    }
    if (flag != 0) {
        ida = weaponId(a);
        idb = weaponId(b);
    } else {
        ida = a->id;
        idb = b->id;
    }
    if (itemType(a->id) == 1) {
        int lv = a->getBulletLevel() + 1;

        if (itemType(b->id) == 2) {
            int attr;
            u8 attr8;

            if (flag == 0) {
                ret = 0;
                goto end;
            }
            attr = a->getBulletType();
            attr8 = attr;
            if (idb == WeaponId2BulletId(ida, attr8)) {
                ret = reload_main(a, b, WeaponId2ChargeNum(ida, lv));
            } else {
                int inv = attr == 0;
                if (idb == WeaponId2BulletId(ida, inv)) {
                    int n = b->num;

                    b->id = WeaponId2BulletId(ida, attr8);
                    b->num = BULLET(a);
                    if (b->num == 0) {
                        erase(b);
                    }
                    a->bullet = (inv << 13) | (n & 0x1FFF);
                    {
                        register ItemWork* arm asm("r0"); // COMPILER-DIFF: 17 (local-alloc fake-lifetime parity: the m_pWep load reuses r0 right after the x8 store's value dies)

                        arm = m_pWep;
                        if (a == arm) {
                            pG->bullet_type = a->getBulletType();
                        }
                    }
                    ret = 1;
                }
            }
        } else if (itemType(b->id) == 9) {
            ret = partsCombine(a, b);
        }
    } else if (itemType(b->id) == 1) {
        int lv = b->getBulletLevel() + 1;

        if (itemType(a->id) == 2) {
            int attr;
            u8 attr8;

            if (flag == 0) {
                ret = 0;
                goto end;
            }
            attr = b->getBulletType();
            attr8 = attr;
            if (ida == WeaponId2BulletId(idb, attr8)) {
                ret = reload_main(b, a, WeaponId2ChargeNum(idb, lv));
            } else {
                int inv = attr == 0;
                if (ida == WeaponId2BulletId(idb, inv)) {
                    int n = a->num;

                    a->id = WeaponId2BulletId(idb, attr8);
                    a->num = BULLET(b);
                    if (a->num == 0) {
                        erase(a);
                    }
                    b->bullet = (inv << 13) | (n & 0x1FFF);
                    {
                        register ItemWork* arm asm("r0"); // COMPILER-DIFF: 17 (local-alloc fake-lifetime parity: the m_pWep load reuses r0 right after the x8 store's value dies)

                        arm = m_pWep;
                        if (b == arm) {
                            pG->bullet_type = b->getBulletType();
                        }
                    }
                    ret = 1;
                }
            }
        } else if (itemType(a->id) == 9) {
            ret = partsCombine(b, a);
        }
    } else if (itemType(a->id) == 2 || itemType(a->id) == 3) {
        if (ida == idb) {
            int room;

            room = itemMaxNum(a->id) - a->num;
            if (room == 0) {
                ret = 0;
            } else {
                if (b->num < room) {
                    a->num = a->num + b->num;
                    b->num = 0;
                } else {
                    a->num = a->num + room;
                    b->num = b->num - room;
                }
                ret = 1;
            }
        }
    } else {
        ret = itemCombine(ida, idb, &newId);
        if (ret != 0) {
            a->num--;
            b->num--;
            if (a->num != 0) {
                get(newId, 1);
            } else {
                itemType(newId);
                a->id = newId;
                a->num = 1;
            }
        }
    }
    if (a->num == 0) {
        erase(a);
    }
    if (b->num == 0) {
        erase(b);
    }
end:
    return ret;
}

// Attaches a weapon part (stock/scope) to a weapon: detaches it from its previous weapon and any
// same-kind part already on the target, records the weapon slot in part->bullet; the armed weapon
// id is recomputed.
int cItemMgr::partsCombine(ItemWork* pWeapon, ItemWork* pParts)
{
    u16 newId;
    ItemWork* list[2];
    ItemWork* p;
    ItemWork** lp;
    int ret;
    int idx;
    int n;
    int i;

    ret = itemCombine(pWeapon->id, pParts->id, &newId);
    if (ret != 0) {

        if (pParts->lv != 0) {
        pParts->lv = 0;
        if (m_pWep != 0 && m_pWep == at(pParts->bullet)) {
            m_wep_id = weaponId(m_pWep);
        }
        pParts->bullet = 0xFFFF;
    }
    p = m_pItem;
    idx = searchAt(pWeapon);
    list[0] = 0;
    list[1] = 0;
    n = 0;
    i = 0;
    if (i < m_array_num) {
        lp = list;
        do {
            if (p->isAlive(m_char)) {
                if (itemType(p->id) == 9 && p->lv == 1 && idx == p->bullet) {
                    *lp++ = p;
                    n++;
                }
            }
            p++;
        } while (++i < m_array_num);
    }
    for (int j = 0; j < n; j++) {
        if (list[j]->id == pParts->id) {
            list[j]->lv = 0;
            list[j]->bullet = 0xFFFF;
            list[j] = 0;
        }
    }
    if (n == 1) {
        if (list[0] != 0) {
            list[0]->lv = 0;
            list[0]->bullet = 0xFFFF;
            list[0] = 0;
        }
    }
        pParts->bullet = searchAt(pWeapon);
        pParts->lv = 1;
        if (m_pWep != 0 && m_pWep == pWeapon) {
            m_wep_id = weaponId(pWeapon);
        }
    }
    return ret;
}

// Marks item `id` usable here (the sub screen offers "Use"); the room/scenario sets these.
int cItemMgr::available(ITEM_ID id)
{
    FlagOn(m_pAvailable, id);
}

// Clears the usable-item mask.
void cItemMgr::flagclear()
{
    int i;

    for (i = 0; i < m_flag_num; i++) {
        m_pAvailable[i] = 0;
    }
}

// Scenario poll: 1 (once) when item `id` was just used from the sub screen.
int cItemMgr::check(ITEM_ID id)
{
    if (id == used_id) {
        used_id = 0xFFFF;
        return 1;
    }
    return 0;
}

// The weapon id of empty hands (0x800).
u16 bareHand()
{
    return 0x800;
}

// Equips a weapon/knife/grenade slot (NULL = bare hands): sets m_pWep, m_wep_id (with parts) and
// the pG weapon level values. 0 when the item is not equippable.
int cItemMgr::arm(ItemWork* p)
{
    if (p == 0) {
        m_pWep = p;
        m_wep_id = bareHand();
        goto ok;
    }
    if (p->num == 0) {
        goto ng;
    }
    if (itemType(p->id) == 1 || itemType(p->id) == 3 || itemType(p->id) == 6) {
        m_pWep = p;
        m_wep_id = weaponId(p);
        m_pWep->bullet = BULLET(m_pWep);
        if (itemType(p->id) == 1) {
            pG->weapon_lv_power = p->getPowerLevel();
            pG->weapon_lv_speed = p->getSpeedLevel();
            pG->weapon_lv_reload = p->getReloadLevel();
            pG->weapon_lv_blt = p->getBulletLevel();
        }
        goto ok;
    }
ng:
    return 0;
ok:
    return 1;
}

// 1 when the armed weapon can be reloaded.
int cItemMgr::reloadable()
{
    return reloadable(m_pWep, 0);
}

// 1 when the weapon is not full and ammo of its current (or, for flag, the other) attribute is
// carried; the rocket launcher 0x52 counts loose rockets; debug infinite ammo always 1.
int cItemMgr::reloadable(ItemWork* p, int flag)
{
    int ret = 0;
    u16 id;
    u16 have;
    u16 max;
    u32 chargeNum;

    if (p == 0) {
        return 0;
    }
    id = weaponId(p);
    if (id == 0x52) {
        return 0;
    }
    WeaponId2ChargeNum(id, p->getBulletLevel() + 1);
    have = BULLET(p);
    chargeNum = WeaponId2ChargeNum(id, p->getBulletLevel() + 1);
    max = chargeNum;
    if (have < max) {
        if (search(WeaponId2BulletId(id, p->getBulletType())) != 0) {
            ret = 1;
        } else if (flag != 0 && (p->id == 0x36 || p->id == 0xAB)) {
            if (search(WeaponId2BulletId(p->id, p->getBulletType() == 0)) != 0) {
                ret = 1;
            }
        }
        if (DbgFlagChk(pG, DBG_INF_BULLET2)) {
            ret = 1;
        }
    }
    return ret;
}

// Reloads the armed weapon.
int cItemMgr::reload()
{
    return reload(m_pWep, 0);
}

// Reloads weapon p from the smallest ammo stacks until full; flag != 0 lets the mine thrower /
// 0xAB switch to the other ammo attribute when the current one is out. Debug infinite ammo fills
// it directly. Returns 1 when any rounds were loaded.
int cItemMgr::reload(ItemWork* p, int flag)
{
    int ret = 0;
    u16 id;
    u16 bid;

    if (p == 0) {
        return 0;
    }
    id = weaponId(p);
    if (itemType(id) != 1) {
        goto done;
    }
    if (DbgFlagChk(pG, DBG_INF_BULLET2)) {
        setBullet(p, WeaponId2ChargeNum(id, p->getBulletLevel() + 1));
        return 0;
    }
    bid = WeaponId2BulletId(id, p->getBulletType());
    if (flag != 0 && (p->id == 0x36 || p->id == 0xAB)) {
        if (ItemMgr.bulletNum() == 0 && ItemMgr.num(bid) == 0) {
            p->setBulletType(p->getBulletType() == 0);
            bid = WeaponId2BulletId(p->id, p->getBulletType());
            if (p == m_pWep) {
                pG->bullet_type = p->getBulletType();
            }
        }
    }
    ordering(bid);
    while (reloadable(p, flag) != 0 && num(bid) != 0) {
        combine(p, minimumSearch(bid), 1);
        ret = 1;
    }
done:
    return ret;
}

// Moves rounds from the ammo stack into the weapon up to `max` loaded; returns 1 when any moved.
int reload_main(ItemWork* pItem_A, ItemWork* pItem_B, int charge_num)
{
    int have = BULLET(pItem_A);
    int room = charge_num - have;
    int n;
    int m;

    if (room == 0) {
        return 0;
    }
    if (pItem_B->num >= charge_num) {
        n = charge_num;
    } else {
        n = pItem_B->num;
    }
    if (n > room) {
        m = room;
    } else {
        m = n;
    }
    pItem_A->bullet = (pItem_A->bullet & 0xE000) | ((have + m) & 0x1FFF);
    pItem_B->num -= m;
    return 1;
}

// Fires the armed item: weapons consume a loaded round (trigger(m_pWep)); thrown items consume one
// of the smallest stack of m_wep_id.
int cItemMgr::trigger()
{
    switch (itemType(m_wep_id)) {
    case 1:
        return trigger(m_pWep);
    case 3:
    case 6: {
        ItemWork* p = m_pWep;
        register int id asm("r9"); // COMPILER-DIFF: #2 (the original masks the u16 member before the call)
        id = m_wep_id;
        asm("" : "+r"(id));

        if (p->flags == 0 || id != p->id) {
            p = minimumSearch((u16) id);
        }
        return trigger(p);
    }
    }
    return 0;
}

// Consumes one round of a weapon (rocket launcher: a loose rocket; disposable 0x35 is dropped and
// unarmed) or one grenade; 0 when empty. Debug_flg[2] 0x00400000 = infinite.
int cItemMgr::trigger(ItemWork* p)
{
    if (DbgFlagChk(pG, DBG_INF_BULLET)) {
        return 1;
    }
    if (p == 0) {
        return 0;
    }
    if (p->id == 0x6D) {
        return 1;
    }
    WeaponId2ChargeNum(p->id, p->getBulletLevel() + 1);
    switch (itemType(p->id)) {
    case 1: {
        int n;

        if (p->id == 0x52) {
            return dump(0x72);
        }
        n = BULLET(p);
        if (n == 0) {
            return 0;
        }
        setBullet(p, n - 1);
        return 1;
    }
    case 3:
    case 6:
        if (DbgFlagChk(pG, DBG_INF_BULLET2)) {
            return 1;
        }
        if (p->id == 0x35) {
            ItemMgr.arm(0);
        }
        return dump(p);
    }
    return 0;
}

// The effective weapon id of a slot: the base id combined with every attached part (stock/scope
// variants); non-weapons return their own id.
u16 cItemMgr::weaponId(ItemWork* p)
{
    u16 id;
    ItemWork* q = m_pItem;
    int idx = searchAt(p);
    int i;

    id = p->id;
    if (itemType(id) != 1) {
        return p->id;
    }
    for (i = 0; i < m_array_num; i++, q++) {
        if (q->isAlive(m_char)) {
            if (itemType(q->id) == 9 && q->lv == 1 && idx == q->bullet) {
                itemCombine(id, q->id, &id);
            }
        }
    }
    return id;
}

// The no-th part attached to weapon slot p (0 when none).
ItemWork* cItemMgr::weaponParts(ItemWork* p, int no)
{
    ItemWork* q = m_pItem;
    int idx = searchAt(p);
    int cnt = 0;
    int i;

    if (p == 0) {
        return 0;
    }
    for (i = 0; i < m_array_num; i++, q++) {
        if (q->isAlive(m_char)) {
            if (itemType(q->id) == 9 && q->lv == 1 && idx == q->bullet) {
                if (cnt++ == no) {
                    return q;
                }
            }
        }
    }
    return 0;
}

// Rounds of ammo type `bulletId` carried plus those loaded in every weapon that uses it.
u16 cItemMgr::bulletNumTotal(int bllt_id)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    u16 total = num(bllt_id);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (bllt_id == wep_info[i].bulletId) {
            total += bulletNum(wep_info[i].id);
        }
    }
    return total;
}

// Rounds available for the armed item (HUD counter).
u16 cItemMgr::bulletNum()
{
    return bulletNumCurrent();
}

// Rounds of the armed item: loaded rounds for weapons, carried count for thrown items.
u16 cItemMgr::bulletNumCurrent()
{
    switch (itemType(m_wep_id)) {
    case 1:
        return bulletNum(m_pWep);
    case 3:
    case 6:
        return bulletNum(m_wep_id);
    }
    return 0;
}

// Loaded rounds summed over every slot of weapon `id`.
u16 cItemMgr::bulletNum(ITEM_ID id)
{
    ItemWork* p = m_pItem;
    u16 total = 0;
    int i;

    if (id == 0x6D) {
        return 1;
    }
    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isAlive(m_char) && p->id == id) {
            total += bulletNum(p);
        }
    }
    return total;
}

// Loaded rounds of a weapon slot (rocket launcher: loose rockets; grenades/knife: count); debug
// infinite ammo returns 100.
u16 cItemMgr::bulletNum(ItemWork* p)
{
    int n;

    if (!DbgFlagChk(pG, DBG_INF_BULLET2) && (DbgFlagChk(pG, DBG_INF_BULLET))) {
        return 100;
    }
    if (p == 0) {
        return 0;
    }
    switch (itemType(p->id)) {
    case 1:
        if (p->id == 0x52) {
            return num(0x72);
        }
        n = BULLET(p);
        break;
    case 3:
        return p->num;
    case 6:
        switch (p->id) {
        case 8:
        case 9:
        case 10:
            return p->num;
        default:
            return 0;
        }
    default:
        n = 0;
        break;
    }
    return n;
}

// Size of the item save block.
int cItemMgr::saveDataSize()
{
    return sizeof(ItemSaveData);
}

// Writes every slot to the save block (id with the set bit, levels/bullets for weapons and parts,
// counts, case position) plus the armed slot index and weapon id.
void cItemMgr::save(void* pData)
{
    ItemSaveData* sd = (ItemSaveData*) pData;
    ItemSaveWork* s = sd->item_list;
    ItemWork* p = m_pItem;
    int i;

    sd->arm_no = 0xFFFF;
    for (i = 0; i < 0x180; i++) {
        memclr_asm(&s[i], sizeof(ItemSaveWork));
        s[i].id = 0xFFFF;
    }
    for (i = 0; i < m_array_num; i++, p++) {
        memclr_asm(&s[i], sizeof(ItemSaveWork));
        if (p->isEmpty()) {
            register int m1 asm("r0"); // COMPILER-DIFF: #13 (the 0xFFFF re-materialised at the store)
            m1 = -1;
            s[i].id = m1;
        } else {
            s[i].id = p->id;
            if (p->type == 1) {
                s[i].id |= 0x8000;
            }
            switch (itemType(p->id)) {
            case 1:
            case 9:
                s[i].num = p->lv;
                s[i].bullet = p->bullet;
                break;
            case 10:
                s[i].num = p->num;
                s[i].bullet = p->lv8[0];
                break;
            default:
                s[i].num = p->num;
                break;
            }
            s[i].x = p->x;
            s[i].y = p->y;
            s[i].orient = p->orient;
            s[i].board = p->board;
        }
        if (p == m_pWep) {
            sd->arm_no = i;
        }
    }
    sd->wep_id = m_wep_id;
}

// Restores the slots from the save block and re-arms the saved slot.
void cItemMgr::load(void* pData)
{
    ItemSaveData* sd = (ItemSaveData*) pData;
    ItemSaveWork* s = sd->item_list;
    ItemWork* p = m_pItem;
    int i;

    m_pWep = 0;
    for (i = 0; i < m_array_num; i++, p++) {
        if (s[i].id != 0xFFFF) {
            int wep = 0;

            if (s[i].id & 0x8000) {
                wep = 1;
            }
            construct(p, s[i].id & 0x7FFF);
            if (wep) {
                p->type = 1;
            } else {
                p->type = 0;
            }
            switch (itemType(p->id)) {
            case 1:
                p->lv = s[i].num;
                p->bullet = s[i].bullet;
                p->num = 1;
                break;
            case 9:
                p->lv = s[i].num;
                p->bullet = s[i].bullet;
                p->num = 1;
                break;
            case 10:
                p->num = s[i].num;
                p->lv8[0] = s[i].bullet;
                break;
            default:
                p->num = s[i].num;
                break;
            }
            p->x = s[i].x;
            p->y = s[i].y;
            p->orient = s[i].orient;
            p->board = s[i].board;
        } else {
            p->flags = 0;
        }
        if (sd->arm_no != 0xFFFF && i == sd->arm_no) {
            m_pWep = p;
        }
    }
    m_wep_id = sd->wep_id;
}

// Discards every case item left on the spare board (board == 0) except `keep` (closing the
// attache case); unarms a discarded weapon.
int cItemMgr::offboardDump(ItemWork* p_get_item)
{
    ItemWork* p = m_pItem;
    int i;

    for (i = 0; i < m_array_num; i++, p++) {
        if (p->isAlive(m_char)) {
            switch (itemType(p->id)) {
            case 1:
            case 2:
            case 3:
            case 6:
            case 9:
                if (p->board == 0) {
                    if (p_get_item != p) {
                        erase(p);
                    }
                    if (m_pWep == p) {
                        m_pWep = 0;
                        m_wep_id = bareHand();
                    }
                }
                break;
            }
        }
    }
    return 1;
}

// Ashley -> Leon hand-over: moves set-1 items into Leon's set (rotating their case position by 90
// degrees onto the spare board, merging stackables up to the max).
void cItemMgr::takeOver()
{
    int i;

    if (m_char != 0) {
        pLog->err(0, 0, "cItemMgr::takeOver() Incorrect character type.");
        return;
    }
    for (i = 0; i < ItemMgr.m_array_num; i++) {
        ItemWork* p = ItemMgr.at(i);

        if (p->isAlive(1)) {
            switch (itemType(p->id)) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 6: {
                f32 fx = (f32) p->x * 0.5f;
                f32 fy = 5.0f - (f32) p->y * 0.5f;
                s8 o = p->orient;

                p->x = (s8) (fy * 2.0f);
                p->y = (s8) (fx * 2.0f);
                if (o < 0) {
                } else if (o <= 3) {
                    p->orient++;
                    if (p->orient > 3) {
                        p->orient -= 4;
                    }
                } else if (o <= 7) {
                    p->orient++;
                    if (p->orient > 7) {
                        p->orient -= 4;
                    }
                }
                p->board = 0;
                break;
            }
            case 12: {
                ItemWork* q = ItemMgr.search(p->id);

                if (q != 0) {
                    u16 max;

                    q->num += p->num;
                    max = itemMaxNum(q->id);
                    if (q->num > max) {
                        q->num = itemMaxNum(q->id);
                    }
                    p->flags = 0;
                }
                break;
            }
            }
            p->type = 0;
        }
    }
}

// Number of files (type 10) owned.
int cItemMgr::countFiles()
{
    int n = 0;
    int i;

    for (i = 0; i < ItemMgr.m_array_num; i++) {
        ItemWork* p = ItemMgr.at(i);

        if (!p->isAlive(m_char)) {
            if (itemType(p->id) == 10) {
                n++;
            }
        }
    }
    return n;
}

// Weapon table: (weapon number, variant type) -> item id.
u16 WeaponNo2WeaponId(u8 wep_no, u8 type)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (wep_no == wep_info[i].no && type == wep_info[i].type) {
            return wep_info[i].id;
        }
    }
    pLog->err(0, 0, "WeaponNo2WeaponId(): (%d, %d) not found.", wep_no, type);
    return 0xFFFF;
}

// Weapon table: item id -> weapon number (index of the player tables).
u8 WeaponId2WeaponNo(ITEM_ID id)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (id == wep_info[i].id) {
            return wep_info[i].no;
        }
    }
    pLog->err(0, 0, "WeaponId2WeaponNo(): id 0x%02x not found.", id);
    return 0xFF;
}

// Weapon table: item id -> variant type.
u8 WeaponId2WeaponType(ITEM_ID id)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (id == wep_info[i].id) {
            return wep_info[i].type;
        }
    }
    pLog->err(0, 0, "WeaponId2WeaponType(): id 0x%02x not found.", id);
    return 0xFF;
}

// Weapon table: ammo item id for the weapon and ammo attribute (0 normal, 1 alternate).
u16 WeaponId2BulletId(ITEM_ID id, int bllt_type)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (id == wep_info[i].id && bllt_type == wep_info[i].attr) {
            return wep_info[i].bulletId;
        }
    }
    pLog->err(0, 0, "WeaponId2BulletId(): id 0x%02x not found.", id);
    return 0xFFFF;
}

// Weapon table: magazine capacity at capacity level `level` (1-based).
u16 WeaponId2ChargeNum(ITEM_ID id, int level)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (id == wep_info[i].id) {
            return (u8) wep_info[i].charge[level - 1];
        }
    }
    pLog->err(0, 0, "WeaponId2ChargeNum(): id 0x%02x not found.", id);
    return 0xFF;
}

// Max tune level of the weapon for stat `type` (0 fire, 1 speed, 2 reload, 3 capacity); 0 = not tunable.
int WeaponId2MaxLevel(ITEM_ID id, int type)
{
    int i;

    for (i = 0; i < (int) (sizeof(wep_level_info) / sizeof(wep_level_info[0])); i++) {
        if (id == wep_level_info[i].id) {
            return wep_level_info[i].lv[type];
        }
    }
    return 1;
}

// Debug list of every owned item id and count (debug_mode 0x10).
void cItemMgr::debugNumDisp(int print_page)
{
    static int sX = 48;
    static int sY = 5;
    int row = 0;
    int color = 0;
    int id = 0;
    int col = 0;

    for (; id <= 0xFE; id++) {
        int n;

        if (itemType(id) == 2) {
            n = bulletNumTotal((u16) id);
        } else {
            n = num((u16) id);
        }
        if (n != 0) {
            switch (itemType(id)) {
            case 0:
                color = 1;
                break;
            case 1:
                color = 2;
                break;
            case 2:
                color = 3;
                break;
            case 3:
                color = 4;
                break;
            case 5:
                color = 6;
                break;
            case 6:
                color = 7;
                break;
            case 7:
                color = 18;
                break;
            case 4:
            case 8:
                color = 0;
                break;
            case 9:
                color = 20;
                break;
            case 10:
                color = 22;
                break;
            case 11:
                color = 23;
                break;
            case 12:
                color = 24;
                break;
            case 13:
                color = 19;
                break;
            case 14:
                color = 5;
                break;
            }
            eprintf((sX - col) * 8, (sY + row++) * 14, color, print_page, "Item%02x : %03d", id, n);
        }
        if (row > 21) {
            row = 0;
            col += 14;
        }
    }
}

// Debug: arms weapon `id`, picking one up first when it is not owned (levels untouched).
void cItemMgr::debugWeapon(ITEM_ID id)
{
    ItemWork* p = search(id);

    if (p != 0) {
        m_pWep = p;
    } else {
        get(id, 0);
        m_pWep = newbie();
    }
    m_wep_id = id;
}

// the split object pads .sdata to 8 bytes (lbl_80313F4C)
asm(".section .sdata; .balign 8");
