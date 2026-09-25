#ifndef ITEM_H
#define ITEM_H

#include "types.h"

typedef u16 ITEM_ID;   // item id

// One inventory slot (cItemMgr::pItems[], 0xE bytes).
struct ItemWork {
    u16 id;        // 0x00  item id
    u16 num;       // 0x02  count / bullets
    u8 flags;      // 0x04  bit0 in use
    u8 type;       // 0x05  inventory type (cItemMgr::type selects the visible set)
    union {
        u16 lv;    // 0x06  weapon tune levels, one nibble each: fire << 12 | mag << 8 | speed << 4 | ex (merchant)
                   //       weapon parts (type 9): 1 = attached; files (type 0xA): lv8[0] = countFiles()
        u8 lv8[2]; // 0x06  the same two bytes
    };
    u16 bullet;    // 0x08  top 3 bits: weapon slot attribute (sscrn: pG->bullet_type), low 13: bullets loaded
                   //       weapon parts (type 9): slot index of the weapon it is attached to (0xFFFF = none)
    s8 x;          // 0x0A  case position (cells * 2) and orientation (puzzle pzlPlayer::save)
    s8 y;          // 0x0B
    s8 orient;     // 0x0C
    u8 board;      // 0x0D  1 = in the case, 0 = on the spare board

    int isAlive(int chr)
    {
        if (flags & 1) {
            return type == (u8) chr;
        }
        return 0;
    }
    int isEmpty() { return (flags & 1) ? 0 : 1; }
    int getPowerLevel() { return lv >> 12; }
    int getSpeedLevel() { return (lv >> 8) & 0xF; }
    int getReloadLevel() { return (lv >> 4) & 0xF; }
    int getBulletLevel() { return (u8) lv & 0xF; }
    void setPowerLevel(int level) { lv = (lv & 0x0FFF) | (level << 12); }
    void setSpeedLevel(int level) { lv = (lv & 0xF0FF) | (level << 8); }
    void setReloadLevel(int level) { lv = (lv & 0xFF0F) | (level << 4); }
    void setBulletLevel(int level) { lv = (lv & 0xFFF0) | level; }
    int getBulletType() { return bullet >> 13; }
    void setBulletType(int type) { bullet = (bullet & 0x1FFF) | (type << 13); }
};

// cItemMgr::ordering() output (cItemMgr::pOrder[], 8 bytes): the in-use slots holding one item id.
struct ItemOrder {
    ItemWork* p_item;  // 0x00
    u16 num;         // 0x04  copy of item->num
    u8 pad_6[2];
};

// itemInfo() result (game/item.cpp).
struct ItemInfo {
    u16 id;        // 0x00  item id (PS2 ITEM_INFO id)
    u8 type;       // 0x02  1 weapon, 2 ammo, 3 = weapon with a magazine (sscrn: empty check), 5/0xC treasure, 9 weapon part, 0xA file ...
    u8 defNum;         // 0x03  default count when get(id, 0)
    u16 maxNum;        // 0x04  max count per slot
};

// One saved slot (cItemMgr::save/load, 12 bytes; 0x180 of them after the 4-byte header).
struct ItemSaveWork {
    u16 id;        // 0x00  item id, bit 15 = ItemWork::type 1; 0xFFFF = empty
    u16 num;       // 0x02  num (weapons/parts: lv)
    u16 bullet;    // 0x04  weapons/parts: bullet; files: lv8[0]
    u8 pad_6[2];
    s8 x;          // 0x08
    s8 y;          // 0x09
    s8 orient;     // 0x0A
    u8 board;      // 0x0B
};

struct ItemSaveData {
    u16 wep_id;               // 0x00
    u16 arm_no;              // 0x02  slot index of the equipped weapon, 0xFFFF = none
    ItemSaveWork item_list[0x180];// 0x04
};                           // 0x1204 = cItemMgr::saveDataSize()

// Inventory manager (game/item.cpp, 0x30 bytes).
class cItemMgr {
private:
    u32* m_pAvailable;                // 0x00  one bit per item id (available()/use(): items usable this frame)
    s32 m_flag_num;                 // 0x04  words in pFlags (8)
    u16 used_id;                // 0x08  item id use() handed to check(), 0xFFFF = none
    u8 pad_A[2];
    ItemWork* m_pWep;             // 0x0C  equipped weapon slot (NULL = bare hands)
    u16 m_wep_id;                  // 0x10  equipped weapon item id
    s8 m_to_whom;                     // 0x12  0 player, 1 sub character heals (sce_at clears it before use())
    u8 m_char;                    // 0x13  inventory type (num(id) / search count only this type)
public:
    ItemWork* m_pItem;           // 0x14
    ItemWork* m_pNew;            // 0x18  slot the last get() filled (puzzle PutInCase copies the piece position into it)
    s32 m_array_num;                 // 0x1C
    ItemOrder* m_p_order_tbl;          // 0x20  ordering() result (merchant: sorted slots of one item id)
    s32 m_order_tbl_num;                 // 0x24  entries in pOrder
    u32 m_bonus_time;                    // 0x28  (sce_at: number shown with item 0x73; get(0x73, n): mercenaries add time)
    u32 m_bonus_point;                    // 0x2C  (sce_at: number shown with item 0x75; get(0x75, n): mercenaries bonus time)

    ItemWork* newbie() { return m_pNew; }
    void setToWhom(int who) { m_to_whom = who; }
    s8 getToWhom() { return m_to_whom; }
    ItemWork* weapon() { return m_pWep; }
    u16 weaponId() { return m_wep_id; }
    void clear();
    int set_game(int trial_flag);
    int set_ada(int no);
    int set_char(int no);
    int set_stage1(int no);
    int set_stage2(int no);
    int set_stage3(int no);
    int set_range(int no);
    int set_debug(int no);
    int setUp(int set_no);
    void gameInit();
    void roomInit();
    int init();
    void construct(ItemWork* out, ITEM_ID room_no);  // fill a slot template for item `id` (puzzle PutInCase)
    ItemWork* at(int i);       // 0x8001DB5C: slot `no` of pItems, NULL when no >= nItems
    int searchAt(ItemWork* p);  // 0x8001DB80: slot index of `p`, -1 if not in pItems
    int makeItemList(u8* p_list, int flag, s8* key_cnt, s8* gld_cnt);
    ItemWork* search(u16 id);   // 0x8001DED0: the in-use slot of this->type holding `id`, NULL if none
    ItemWork* minimumSearch(ITEM_ID id);
    void ordering(ITEM_ID id);      // 0x8001DFD0: collect the in-use slots holding `id` into pOrder (qsort by order_cmp)
    int get(ITEM_ID id, int num);
    int use(ItemWork* p);       // 0x8001E3BC
    void erase(ItemWork* p);    // remove slot `p` (puzzle removeExtraPiece)
    int dump(ITEM_ID id);           // 0x8001E970: drop item `id`
    int dump(ItemWork* p);
    int dumpAll(ItemWork* p);
    int dumpType(int type);
    u16 num(int id, u8 type);   // 0x8001EAE4: count of item `id` of the given type (pl_sub: num(0xFE, 0))
    u16 num(int id);            // 0x8001EB54: count of item `id` of this->type
    u16 num(ItemWork* p);
    int combine(ItemWork* a, ItemWork* b, int flag);  // merge b into a (puzzle cmbPiece)
    int partsCombine(ItemWork* pWeapon, ItemWork* pParts);
    int available(ITEM_ID id);      // 0x8001F2C4
    void flagclear();           // 0x8001F2E8
    int check(ITEM_ID id);
    int arm(ItemWork* p);       // 0x8001F350: equip `p` (NULL: bare hands)
    // equipped weapon (this->xC), objWep: reloadable(x, 0) / reload(x, 0) / trigger(x)
    int reloadable();           // 0x8001F470
    int reloadable(ItemWork* p, int flag);
    int reload();               // 0x8001F5B4
    int reload(ItemWork* p, int flag);
    int trigger();              // 0x8001F7E8
    int trigger(ItemWork* p);
    u16 weaponId(ItemWork* p);
    ItemWork* weaponParts(ItemWork* p, int no);
    u16 bulletNumTotal(int bllt_id);
    u16 bulletNum();            // 0x8001FC20: bulletNumCurrent() of the equipped weapon
    u16 bulletNumCurrent();     // 0x8001FC40
    u16 bulletNum(ITEM_ID id);
    u16 bulletNum(ItemWork* p);
    int saveDataSize();
    void save(void* pData);
    void load(void* pData);
    int offboardDump(ItemWork* p_get_item);
    void takeOver();
    int countFiles();
    void debugNumDisp(int print_page);
    void debugWeapon(ITEM_ID id);
};

extern cItemMgr ItemMgr;

// weapon number/type -> item id (0xFFFF when unknown)
u16 WeaponNo2WeaponId(u8 wep_no, u8 type);
// life meter level of a max life `max` (cockpit: lifeLevel(20, pl_life_max, 1200))
int lifeLevel(int level_up_num, s16 curr_life_max, int init_life_max);

extern "C" {
// item id -> weapon number / type (0xFF when unknown), item attributes
u8 WeaponId2WeaponNo(ITEM_ID id);
u8 WeaponId2WeaponType(ITEM_ID id);
void itemInfo(ITEM_ID id, ItemInfo* info);
// weapon item id -> its bullet item id (attr: ItemWork::x8 >> 13), charge count, max tune level per type
u16 WeaponId2BulletId(ITEM_ID id, int bllt_type);
u16 WeaponId2ChargeNum(ITEM_ID id, int level);
int WeaponId2MaxLevel(ITEM_ID id, int type);
// weapon tune ratios at tune level `level` (examine: power x10 / speed, reload x100 percent)
f32 getPowerRatio(ITEM_ID id, int level);
f32 getSpeedRatio(ITEM_ID id, int level);
f32 getReloadRatio(ITEM_ID id, int level);
f32 getBulletRatio(ITEM_ID id, int level);
// heal the player (ItemMgr.x12 0) or the sub character (1) by `n`; 0 when already at max
int healing(u16 life_add);
int addMoney(int n);
u16 bareHand();
int itemCombineCheck(ITEM_ID id);
int itemCombine(ITEM_ID srcA, ITEM_ID srcB, u16* dst);
int reload_main(ItemWork* pItem_A, ItemWork* pItem_B, int charge_num);
u8 gld_order(u8 no);
int gld_cmp(const void* a, const void* b);
int order_cmp(const void* a, const void* b);
}

// Each reads one field of the ItemInfo that itemInfo() fills (type, defNum, maxNum: offsets 2, 3 and 4),
// as the three free inlines of the original (an ItemInfo temp at every call site). Free inlines carry no
// symbol, so these three identifiers are not recovered from the original.
inline u8 itemType(ITEM_ID id)
{
    ItemInfo info;
    itemInfo(id, &info);
    return info.type;
}

inline u8 itemDefNum(ITEM_ID id)
{
    ItemInfo info;
    itemInfo(id, &info);
    return info.defNum;
}

inline u16 itemMaxNum(ITEM_ID id)
{
    ItemInfo info;
    itemInfo(id, &info);
    return info.maxNum;
}

extern u16 g_item_order[];
extern int g_item_order_num;

#endif
