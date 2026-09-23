#ifndef OBJ09_H
#define OBJ09_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Rigid body effect model work (game/obj09.cpp, set up by esp_efm EfmSetObj09): a box of
// `size` with mass / moments of inertia, pushed by `spd` (momentum). Extends to cObj+0x3D8.
struct Efm09Work {
    EfmCore core;         // 0x00
    f32 mass;             // 0x0C  size.x * size.y * size.z / 1e9 * mass_mul
    Vec moment;           // 0x10  moments of inertia: moment_mul * mass * (size.y^2 + size.z^2) / 12, ... (obj09 dwdt; PS2 OBJ09_FREE Ig)
    u8 pad_1C[4];
    Vec pos;              // 0x20  = basePos at set up
    Vec basePos;          // 0x2C  EspGenWork x0C + random (y + 0.0001)
    Mtx mat;              // 0x38  identity at set up
    Vec spd;              // 0x68  EspGenWork x24 + random, * mass * 100 (obj09: velocity)
    Vec w;                // 0x74  0 at set up (obj09: world angular velocity, mat * rotSpd) (PS2 OBJ09_FREE w)
    Vec size;             // 0x80  EspGenWork xD8..xE0 * 100 + 250
    Vec force;            // 0x8C  force accumulated by AddForce, cleared every CalcVel
    Vec torque;           // 0x98  torque accumulated by AddForce
    Vec rotSpd;           // 0xA4  EspGenWork x70 + random (overlaps cObj attr / callBack): local angular velocity
};

// Rigid body effect model (Efm09): a box with mass and moments of inertia, integrated with a
// second order Runge-Kutta step (CalcVel / Calc), colliding with the scenario at its eight
// corners (calcPointHit), with the other rigid bodies (Obj09HitCheck), the water, the sand and
// the player.
class cObj09 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Efm09Work

    virtual void move();
};

#define EFM09_WK(o) ((Efm09Work*) (o)->free)

#endif
