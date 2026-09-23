#ifndef OBJ12_H
#define OBJ12_H

#include "types.h"
#include "vec.h"
#include "obj.h"


// Hanging / thrown object work (game/obj12.cpp `cObj12`): the obj00 layout with a life counter,
// the landing SE moved to 0x68 and a rope `type` selecting the three rope offsets.
struct Obj12Work {
    u32 be_flag;            // 0x00  bit2: falling, bit3: blending toward the parent, bit7: keep the parent matrix, bit8: thrown, bit9: fading out after `life`
    void* pMot;           // 0x04
    int Motion_info;        // 0x08  MotionMove result of this frame
    u32 mot_attr;           // 0x0C
    cModel* pEm_oya;          // 0x10  parent
    int oya_parts;          // 0x14
    f32 oya_hokan;             // 0x18  blend rate (1.0 = parent matrix)
    f32 oya_hokan_add;          // 0x1C
    s16 fallSpd[3][3];    // 0x20  rope point speeds * 10 (fallSpd[0] is the throw speed)
    u8 pad_32[2];
    Mtx hokan_mat;              // 0x34  previous parent matrix
    int Lost_wait;             // 0x64  frames before the fade out
    u8 fall_se_id;             // 0x68  landing SE (0xFF = none)
    u8 fall_se_no;              // 0x69
    u8 fall_em_id;              // 0x6A
    u8 fall_se_ck;          // 0x6B
    u8 fall_type;              // 0x6C  rope offsets table index (setFall)
};

// Hanging object that can be thrown and falls as a three-point rope (obj00 variant with a rope
// type, a life counter and a throw routine).
class cObj12 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Obj12Work

    virtual void move();
    virtual ~cObj12() {}
    void setParent(cModel* oya, int partsNo, int noNormalize);
    void chainMove();
    void setFall(Vec* spd, u8 type);
    void setFallSe(u8 blk, u8 no, u8 id);
    void fallMove();
    void throwMove();
    void setBurn();
};

#define OBJ12_WK(o) ((Obj12Work*) (o)->free)

cObj12* SetObj12(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
