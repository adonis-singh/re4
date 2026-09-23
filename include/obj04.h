#ifndef OBJ04_H
#define OBJ04_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Effect model work (game/obj04.cpp `Efm04`): a thrown/falling particle-like model.
struct Efm04Work {
    EfmCore Eff_core;       // 0x00
    f32 D_speed;            // 0x0C  speed *= D_speed every frame
    Vec Speed_plus;         // 0x10  added to speed every frame
    Vec Ang_plus;           // 0x1C  added to rot every frame
    f32 Size_base_x;        // 0x28
    f32 Size_base_y;        // 0x2C
    f32 Size_mul;           // 0x30  Size_mul += Size_plus, Size_plus *= D_size_plus
    f32 Size_plus;          // 0x34
    f32 D_size_plus;        // 0x38
    u8 Col_start_r;         // 0x3C  start colour (EspGenWork x9C..x9F)
    u8 Col_start_g;         // 0x3D
    u8 Col_start_b;         // 0x3E
    u8 Col_start_a;         // 0x3F  alpha at the end of the fade-in
    f32 Col_r;              // 0x40
    f32 Col_g;              // 0x44
    f32 Col_b;              // 0x48
    f32 Col_a;              // 0x4C
    f32 Col_d_r;            // 0x50  fade-out multipliers
    f32 Col_d_g;            // 0x54
    f32 Col_d_b;            // 0x58
    f32 Col_d_a;            // 0x5C
    u16 Col_max_cnt;        // 0x60
    u16 Col_start_cnt;      // 0x62
    u16 Pos_start_cnt;      // 0x64
    u16 Size_start_cnt;     // 0x66
    u16 Life_max;           // 0x68  0 = forever
    u16 Life_time;          // 0x6A
    cModel* pMod;           // 0x6C  (esp_efm: the sequence's parent model)
    u32 Guid_pMod;          // 0x70
    cCoord* pParts;         // 0x74  pEffParentWorld when detached
    u8 Release_time;        // 0x78  frame to re-orient along the parent (0xFF = never)
    u8 Parts_no;            // 0x79  parts of the parent the model follows (PS2 OBJ04_FREE Parts_no)
    u8 Flg;                 // 0x7A  bit0: came to rest
    u8 Motion_no;           // 0x7B  EspGetEfmMotAddr motion (EspGenWork WorkSp8[2]) (PS2 OBJ04_FREE Motion_no)
    u32 Tool_flg;           // 0x7C  bit0: floor collision, bit1: scenario collision, bit3: MotionMove
    f32 Pt_hit_size;        // 0x80
    Vec RefRate;            // 0x84  x/z: horizontal, y: vertical rebound rate (EfmSetObj04: EspGenWork xE4 * 0.1; PS2 OBJ04_FREE RefRate)
};

// Effect model (Efm): a model thrown from an effect that flies, fades and bounces off the
// scenario/floor, following its parent until `rotFrame`.
class cObj04 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Efm04Work

    virtual void move();
};

#define EFM04_WK(o) ((Efm04Work*) (o)->free)

#endif
