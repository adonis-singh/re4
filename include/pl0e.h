#ifndef PL0E_H
#define PL0E_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "em.h"

class cObj;

// Work of the jet ski (pl0e module, D:/Bio4/Prog/pl0e.cpp), overlaid on cEm from 0x3E0. The ski is a
// cEm the room script drives: setRail(path) puts it on a rail object (SetObj00) it follows with
// pl0ePathMove, setRide makes the player (PlBoatMove / plboat_R2_*) and the partner (subBoat*)
// ride it, set2ndRail is the second rail of the chase.
struct Pl0eWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: rail input frozen this frame (jump), bit1: on the 2nd rail,
                          //                bit2: sunk, bit3: jump motion set
    int timer;            // 0x004  frames until the engine SE stops after a jump
    u8 pad_8[8];
    int fall;             // 0x010  jump: the ski started to fall
    f32 rotY;             // 0x014  rot.y at the start of the frame (pl0eCamMove roll)
    Vec swayAmp;          // 0x018  sway amplitude (x / z used), decays by 0.96 per frame
    Vec swayPhase;        // 0x024  sway phase
    u8 pad_30[0x14];
    f32 rollPhase;        // 0x044  roll noise phase
    f32 pitchPhase;       // 0x048  pitch noise phase
    f32 roll;             // 0x04C
    f32 pitch;            // 0x050
    f32 x54;              // 0x054
    f32 spdXZ;            // 0x058  |pos - oldPos| in the XZ plane
    f32 dirAng;           // 0x05C  heading change (Muku2 towards the movement direction)
    f32 dirAngAbs;        // 0x060
    u8 pad_64[4];
    int cnt68;            // 0x068  0x1D countdown outside rooms 10D / 10E
    u8 pad_6C[4];
    u8 espKind;           // 0x070  EspPullCoreKind at creation
    u8 x71;               // 0x071
    u8 x72;               // 0x072
    u8 pad_73[5];
    int x78;              // 0x078
    cObj* pWave;          // 0x07C  wave object (SetObj00) moved under the ski (pl0eWaveMove)
    u8 pad_80[4];
    cObj* pRailObj;       // 0x084  rail object (SetObj00) the path is attached to
    void* pPath;          // 0x088  rail path data (setRail argument)
    f32 spd;              // 0x08C  speed along the path per frame (pl0e_spd_max / boost / slow)
    f32 dist;             // 0x090  distance along the path
    f32 length;           // 0x094  PathGetLength
    u16 seg;              // 0x098  PathGetPosEm segment
    u8 pad_9A[2];
    Vec pathPos;          // 0x09C  PathGetPosEm position of this frame
    Vec pathPosOld;       // 0x0A8  ... of the previous frame
    Vec ofs;              // 0x0B4  offset from the path in the ski's frame (x: left / right input)
    f32 spdX;             // 0x0C0  lateral speed
    f32 spdY;             // 0x0C4  vertical speed (jump)
    f32 floorY0;          // 0x0C8
    f32 floorY1;          // 0x0CC
    f32 sink;             // 0x0D0  96000 at start, drained on the 2nd rail below max speed; <= 0 sinks the ski
    int xD4;              // 0x0D4  20 when sunk / after a jump miss
    f32 xD8;              // 0x0D8
    f32 camRate;          // 0x0DC  camera offset blend (0 .. 1) driven by the speed
    u8 pad_E0[0x10];
    Vec ofsF0;            // 0x0F0  position offset in the ski's frame (pl0eBoatControl)
    u8 pad_FC[4];
    u32 seNo;             // 0x100  engine SE handle (SndCall 8/0xA)
    u16 pitch104;         // 0x104  engine SE doppler pitch (0 .. 500)
    u8 jumpCnt;           // 0x106  frames since the jump started
    u8 pad_107;
    f32 blendRate;        // 0x108  lean blend (-255 .. 255), sign selects the left / right motion
    int hokan;            // 0x10C  blend motion hokan counter
    u32 frame;            // 0x110  blend motion frame
    u32 frameOld;         // 0x114
    MotionWorkSub blendMot;   // 0x118 (0x4F8) .. 0x1E8  the blended lean motion
};

#define PL0E_WK(em) ((Pl0eWork*) (((cPl0e*) (em))->free))

class cPl0e : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (PL0E_WK)
    // constructor / destructor implicit (Pl0eInit: new (em) cPl0e())
    virtual void move();
    virtual void setPos(Vec* pos, f32 ang);
    virtual void stopEngine();
    virtual void setRail(void* path);
    virtual void set2ndRail();
    virtual void setRide();
};

void Pl0eInit(cEm* em);
void pl0eBoatControl(cPl0e* em);
void pl0eGetBoatDir(cPl0e* em);
void pl0eBoatRoll(cPl0e* em);
void pl0eCamMove(cPl0e* em);
void pl0eRideActEvtCk(cPl0e* em);
void pl0eScrAdjust(cPl0e* em);
void plboatBlendMotSet(cPlayer* pl, void* m0, void* m1, void* m2, int a, int b, int c);
void subBlendMotSet(cSubChar* sub, void* m0, void* m1, void* m2, int a, int b, int c);
void plOnJet(cPlayer* pl);
void subOnJet(cSubChar* sub, cPl0e* boat);
void pl0ePathMove(cPl0e* em, int jump);
void pl0ePathGetTarget(cPl0e* em, Vec* out);
int pl0eSlopeControl(cPl0e* em);
void pl0eBlendMotSet(cPl0e* em, void* m0, void* m1, void* m2, int a, int b, int c);
int pl0eJumpCk(cPl0e* em);
int pl0eCrashCk(cPl0e* em);
int pl0eSinkCk(cPl0e* em);
int pl0eJumpMissCk(cPl0e* em);
void pl0eWaveMove(cPl0e* em);

#endif
