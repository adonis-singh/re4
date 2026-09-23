#ifndef OBJ05_H
#define OBJ05_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Effect model with loose parts (game/obj05.cpp `Efm05`): the obj04 scale / colour fade with the
// parts burst parameters; each parts keeps its own state in its cModel (Kaboom_flg / Kaboom_spd /
// Kaboom_ang_spd, OBJ05_KABOOM below).
struct Efm05Work {
    EfmCore Eff_core;       // 0x00
    Vec Ang_plus;           // 0x0C  added to rot every frame
    f32 Size_base_x;        // 0x18
    f32 Size_base_y;        // 0x1C
    f32 Size_mul;           // 0x20  Size_mul += Size_plus, Size_plus *= D_size_plus
    f32 Size_plus;          // 0x24
    f32 D_size_plus;        // 0x28
    u8 Col_start_r;         // 0x2C  start colour (EspGenWork x9C..x9F)
    u8 Col_start_g;         // 0x2D
    u8 Col_start_b;         // 0x2E
    u8 Col_start_a;         // 0x2F  alpha at the end of the fade-in
    f32 Col_r;              // 0x30
    f32 Col_g;              // 0x34
    f32 Col_b;              // 0x38
    f32 Col_a;              // 0x3C
    f32 Col_d_r;            // 0x40  fade-out multipliers
    f32 Col_d_g;            // 0x44
    f32 Col_d_b;            // 0x48
    f32 Col_d_a;            // 0x4C
    u16 Col_max_cnt;        // 0x50
    u16 Col_start_cnt;      // 0x52
    u16 Pos_start_cnt;      // 0x54  (PS2 OBJ05_FREE Pos_start_cnt)
    u16 Size_start_cnt;     // 0x56
    u16 Life_max;           // 0x58  0 = forever
    u16 Life_time;          // 0x5A
    u32 Tool_flg;           // 0x5C  bit0: floor collision, bit1: scenario collision, bit3: parts tip over
    Vec Kaboom_pos;         // 0x60  burst centre relative to pos (Efm05RotMatrix rotates it)
    u8 Kaboom_pow;          // 0x6C  burst speed (* 4096 / range)
    u8 Kaboom_spd;          // 0x6D  burst range growth per frame (0xFF: everything at once)
    u8 Kaboom_rnd;          // 0x6E  speed random (/ 32)
    u8 Kaboom_rot;          // 0x6F  rotation speed random (* 0.005)
    f32 Kaboom_gravity;     // 0x70  added to the parts speed y
    f32 Kaboom_d_spd;       // 0x74  parts speed *= Kaboom_d_spd
    u32 Pt_hit_size;        // 0x78
    Vec RefRate;            // 0x7C  x/z: horizontal, y: vertical rebound rate (EfmSetObj05: EspGenWork xE4 * 0.1; PS2 OBJ05_FREE RefRate)
    u32 Rand_seed;          // 0x88  fRandSeed1_1 seed
};

// Loose-parts burst state (PS2 cParts anonymous union, Kaboom_flg/Kaboom_spd/Kaboom_ang_spd
// branch), overlaid on the parts' cModel from 0x128: same idiom as pendulum.cpp's PEN_WORK,
// motion.h's IK_PARTS and em3c.h's EM3C_BOMB, all of which reuse the same "player" fields
// (`pFloor_norm` on) for whatever a parts of that kind actually needs there.
struct Obj05PartsKaboom {
    u32 Kaboom_flg;      // 0x128  0 waiting, 1 flying, 2 at rest
    Vec Kaboom_spd;      // 0x12C
    Vec Kaboom_ang_spd;  // 0x138
};

#define OBJ05_KABOOM(p) ((Obj05PartsKaboom*) &(p)->pFloor_norm)

// Effect model with loose parts (Efm05): the model scales and fades like obj04 while each parts
// bursts away from `center` once it comes within `range`, flying with its own speed / rotation
// speed (kept in the parts' cModel at 0x128) and bouncing off the scenario / floor.
class cObj05 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Efm05Work

    virtual void move();
};

#define EFM05_WK(o) ((Efm05Work*) (o)->free)

#endif
