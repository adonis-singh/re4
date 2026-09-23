#ifndef ATARIINFO_H
#define ATARIINFO_H

#include "types.h"
#include "vec.h"

class cModel;

enum PRIORITY {
    PRI_NORMAL = 0,
    PRI_LV1 = 1,
    PRI_LV2 = 2,
    PRI_LV3 = 3
};

// Character collision info (game/atariInfo.cpp), 0x4C bytes; embedded in cPlayer at 0x2B4.
// The flag helpers are parameterless in-class inlines on purpose: the original accesses go
// `addi rX,this,0x2B4; lhz 0x1A(rX); andi. 0xFCFF` (address computed once, used by the load and
// the store; the mask folded to 16 bits). A direct member access folds the address into one
// displacement; an inline taking the bit as a parameter gives `rlwinm` instead of `andi.`.
class cAtariInfo {
public:
    Vec m_offset;         // 0x00  offset from the model (rotated by the model's rot)
    f32 m_radius;       // 0x0C  push rectangle half size along local X (pl_push) / cylinder radius
    f32 m_radius2;       // 0x10  along local Z
    f32 m_height;           // 0x14  half height
    s16 m_parts_no;     // 0x18  parts index + 1 the info follows, 0 = the model (pl_dmg Pl_R0_Die sets 4)
    u16 m_flag;       // 0x1A  bit1: rectangle (dispRect), bits 3-4: priority, bits 8-9 (0x300): collide with enemies
    f32 m_radius_n;      // 0x1C  rect size `move` interpolates m_radius/m_radius2 towards
    f32 m_radius2_n;      // 0x20
    u16 m_hokan;         // 0x24  frames left of the interpolation
    u16 m_stat;         // 0x26  (init0 sets 1) bit0: no character collision this frame (at_mod EmAtCheck)
    cModel* m_pMod;   // 0x28  model pushed along with this one (at_mod At_em_sphere_sphere_ck)
    f32 m_radius3;         // 0x2C
    Vec m_Pos;    // 0x30  getPos result of this frame (at_mod EmAtCheck)
    Vec m_oldPos; // 0x3C  m_Pos of the previous frame
    union {
        u32 x48;             // 0x48
        cAtariInfo* m_pList;    // 0x48  next info of the chain (at_mod DrawOba)
    };

    cAtariInfo();
    void init0(f32 ox, f32 oy, f32 oz, f32 rs, f32 ro, f32 ra, f32 h, int pno, int hokan, int flags);
    // init(..., parts, flags, hokan) = init0(..., parts, hokan, flags); m_flag |= 1
    void init(f32 ox, f32 oy, f32 oz, f32 rs, f32 ro, f32 ra, f32 h, int pno, int flags, int hokan);
    void setPriority(int pri);  // flags bits 3-4
    // mode < 0: rect = (100, 100), rect2 = a/b, cnt = -mode; mode == 0: rect = rect2 = a/b; > 0: rect2 only, cnt = mode
    void set(int mode, f32 a, f32 b);
    void move();
    // World position (`getPos`) and the positions before/after this frame's move (`getSpeedVector`).
    void getSpeedVector(cModel* m, Vec* oldPos, Vec* pos);
    void getPos(cModel* m, Vec* out);
    void disp(cModel* pMod);
    void dispRect(cModel* pMod);
    // PS2's collision switches: 0x100 against the scenario (Sca), 0x200 against other characters (Oba).
    void on() { m_flag |= 0x300; }
    void off() { m_flag &= ~0x300; }
    void onSca() { m_flag |= 0x100; }
    void offSca() { m_flag &= ~0x100; }
    void onOba() { m_flag |= 0x200; }
    void offOba() { m_flag &= ~0x200; }
};

#endif
