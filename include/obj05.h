#ifndef OBJ05_H
#define OBJ05_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Effect model with loose parts (game/obj05.cpp `Efm05`): the obj04 scale / colour fade with the
// parts burst parameters; each parts keeps its own state in its cModel (efmStat / efmSpd / efmRotSpd).
struct Efm05Work {
    EfmCore core;         // 0x00
    Vec rotSpd;           // 0x0C  added to rot every frame
    f32 scaleXZ;          // 0x18
    f32 scaleY;           // 0x1C
    f32 scale;            // 0x20  scale = scale + scaleSpd, scaleSpd *= scaleDamp
    f32 scaleSpd;         // 0x24
    f32 scaleDamp;        // 0x28
    u8 r0;                // 0x2C  start colour (EspGenWork x9C..x9F; PS2 OBJ05_FREE Col_start_r..a)
    u8 g0;                // 0x2D
    u8 b0;                // 0x2E
    u8 a0;                // 0x2F  alpha at the end of the fade-in
    f32 r;                // 0x30
    f32 g;                // 0x34
    f32 b;                // 0x38
    f32 a;                // 0x3C
    f32 rMul;             // 0x40  fade-out multipliers
    f32 gMul;             // 0x44
    f32 bMul;             // 0x48
    f32 aMul;             // 0x4C
    u16 fadeStart;        // 0x50
    u16 fadeLen;          // 0x52
    u16 Pos_start_cnt;    // 0x54  (PS2 OBJ05_FREE Pos_start_cnt)
    u16 scaleStart;       // 0x56
    u16 life;             // 0x58  0 = forever
    u16 frame;            // 0x5A
    u32 flags;            // 0x5C  bit0: floor collision, bit1: scenario collision, bit3: parts tip over
    Vec center;           // 0x60  burst centre relative to pos (Efm05RotMatrix rotates it)
    u8 pow;               // 0x6C  burst speed (* 4096 / range)
    u8 rangeStep;         // 0x6D  burst range growth per frame (0xFF: everything at once)
    u8 rnd;               // 0x6E  speed random (/ 32)
    u8 rotAmp;            // 0x6F  rotation speed random (* 0.005)
    f32 grav;             // 0x70  added to the parts speed y
    f32 spdDamp;          // 0x74  parts speed *= spdDamp
    u32 groundOfs;        // 0x78
    Vec bounce;           // 0x7C  x/z: horizontal, y: vertical rebound rate (EfmSetObj05: EspGenWork xE4 * 0.1; PS2 OBJ05_FREE RefRate)
    u32 seed;             // 0x88  fRandSeed1_1 seed
};

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
