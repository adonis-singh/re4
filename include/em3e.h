#ifndef EM3E_H
#define EM3E_H

#include "types.h"
#include "vec.h"
#include "em.h"

// em3e module = D:/Bio4/Prog/emmark.cpp (Bio4.emmark.sym): the shooting-gallery targets of room 22c
// (cEmMark, model id 0x3E). The room (st2_4 r22c.cpp) creates them with cEmMark::init and scores hits
// through R22cHitMark / R22cHitEffect; a target runs an instruction list (EmMarkInst) from the room data.

// One instruction of a target's script: `type` selects the routine (0 begin, 1 end, 2 stay, 3 move,
// 4 none) and its size (4, 4, 8, 0x14, 0 bytes).
struct EmMarkInst {
    int type;             // 0x00  routine (low byte is stored into xFC)
    union {
        int count;        // 0x04  stay: frames
        int x;            // 0x04  move: target position (integer)
    };
    int Y;                // 0x08
    int Z;                // 0x0C
    int spd;              // 0x10  move: speed per frame
};

// Room-side description of one target (st2_4 data): type byte, integer position, then the script.
struct EmMarkData {
    u8 pad_0[7];
    u8 type;              // 0x07
    int X;                // 0x08
    int Y;                // 0x0C
    int Z;                // 0x10
    EmMarkInst inst[1];   // 0x14  first instruction
};

// The target's own fields: the original cEmMark derived from a 0x3E0-byte cEm and declared these as
// class members (the code addresses them straight off `this`, `lwz 0x4d4(r31)`), so they are a
// whole-object view here rather than a work pointer.
struct EmMarkView {
    u8 em[0x3E0];
    YARARE_INFO hit[4];     // 0x3E0  extra hit boxes (YarareAddCube)
    EmMarkInst* pInst;    // 0x4B0  current instruction
    int timer;            // 0x4B4  emmark_stay: frames left
    u8 pad_4B8[0x4D4 - 0x4B8];
    int age;              // 0x4D4  frames spent in routines 2/3 (countOldMark compares it)
    int downTimer;        // 0x4D8  downCheck: frames until setDown
};

#define EMMARK(em) ((EmMarkView*) (em))

class cEmMark : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMMARK_WK)
    cEmMark();
    virtual void move();
    void init(EmMarkData* p_data);
    void init(u8 type, EmMarkInst* inst, f32 x, f32 y, f32 z);
    void downCheck();
    void damageCheck();
    int setEff(int a, int kind);
    int setEffWall();
    int setEffWallNormal();
    void headBomb();
    void setNextInstruction();
    void standSpring();
    void setDown();
};

void em3eInit(cEm* em);
int countOldMark(cEmMark* self, int age);

// st2_4 r22c.cpp (the room owns them; called from the module)
void R22cHitMark(int type, int a, Vec* pos, int b, int age);
void R22cHitEffect(int no);

#endif
