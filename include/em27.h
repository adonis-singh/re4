#ifndef EM27_H
#define EM27_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cCtrl;

// Work of the em27 enemy (em27 module, D:/Bio4/Prog/em27.cpp), overlaid on cEm from 0x3E0.
struct Em27Work {
    u32 Be_flg;            // 0x000 (0x3E0)  bit3: damage/die routine, bit4: waiting (no scale reset), bit6: no push,
                          //                bit7: dying (no water effect), bit8: water height set by the room
    int Timer;            // 0x004 (0x3E4)  frames left in the current routine
    int upDown;           // 0x008 (0x3E8)  die: 1 = surfaced (bob), else frames until the next splash
    u8 pad_C[0x24 - 0xC];
    f32 L_go;              // 0x024 (0x404)  read by R1_Walk (never written here)
    u8 pad_28[0x40 - 0x28];
    Vec target;           // 0x040 (0x420)  swim target (10000 past the player, home, or a random point)
    u8 pad_4C[4];
    Vec home;             // 0x050 (0x430)  position at init
    Vec Spd;              // 0x05C (0x43C)  current local speed (em27SetSPeed lerps it); .y is the die bob angle
    Vec Spd_t;        // 0x068 (0x448)  speed the routine wants
    Vec Start_pos;          // 0x074 (0x454)  position at init
    Vec Start_ang;          // 0x080 (0x460)  rotation at init
    u8 pad_8C[0xA4 - 0x8C];
    f32 waterHeight;      // 0x0A4 (0x484)  surface height (setWaterHeight / GetWaterHeight)
    int Dash_wait;        // 0x0A8 (0x488)  frames until the next dash / turn is allowed
    int Esc_timer;       // 0x0AC (0x48C)  frames the fish heads for the player
    int Go_timer;         // 0x0B0 (0x490)  set when a new random target was chosen, cleared next frame
    u8 pad_B4;
    u8 dieVariant;        // 0x0B5 (0x495)  Rnd() & 1 at death: which die / float motion
    u8 pad_B6[2];
    cCtrl* pCtrl12;       // 0x0B8 (0x498)  GetCtrlCtrl12()
    cCtrl* pCtrlPlAvoid;       // 0x0BC (0x49C)  GetCtrlCtrl11()
};

#define EM27_WK(em) ((Em27Work*) (((cEm27*) (em))->free))

// Enemy 0x27 (the lake fish, em27 module). The stage rooms call setWaterHeight.
class cEm27 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM27_WK)
    virtual void move();

    void setWaterHeight(f32 h);   // setWaterHeight__5cEm27f
};

void Em27Init(cEm* em);
void em27DmCk(cEm27* em);
void em27SetSPeed(cEm27* em, f32 rate);
void em27ScaleReset(cEm27* em);
void em27ObaHitCk(cEm27* em);
int em27MotionMoveScale(cEm27* em);
void em27WaterEffSet(cEm27* em);
int em27JumpCk(cEm27* em);

#endif
