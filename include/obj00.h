#ifndef OBJ00_H
#define OBJ00_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Plain scroll-less object (game/obj00.cpp): a model with a motion, hung on a parent model's parts
// with OyaSetObj00 (room scripts: r11d's sister object).
cObj* SetObj00(void* bin, void* tpl, Vec* pos, Vec* rot);
void MotSetObj00(cObj* obj, void* mot, int prm, int a);
void OyaSetObj00(cObj* obj, cModel* oya, int partsNo);

// Hanging object work (game/obj00.cpp): follows a parts of its parent (`oya`) with a slerp
// blend, or falls as a three-point rope (obj00FallMove).
struct Obj00Work {
    u32 be_flag;            // 0x00  bit2: falling, bit3: blending toward the parent, bit5: fading out
    void* pMot;           // 0x04
    int motA;             // 0x08  MotionSetCore 4th argument
    u32 mot_attr;           // 0x0C  low 16 bits: MotionSetCore 6th argument
    cModel* pEm_oya;          // 0x10  parent
    int oya_parts;          // 0x14  parts of the parent to follow
    f32 oya_hokan;             // 0x18  blend rate (1.0 = parent matrix)
    f32 oya_hokan_add;          // 0x1C
    s16 fallSpd[3][3];    // 0x20  rope point speeds * 10
    u8 pad_32[2];
    Mtx hokan_mat;              // 0x34  previous parent matrix
    u8 fall_se_id;             // 0x64  landing SE
    u8 fall_se_no;              // 0x65
    u8 fall_em_id;              // 0x66
    u8 fall_se_ck;          // 0x67
};

// Hanging object (lamp, sign, ...): follows a parts of its parent with a slerp blend, falls as a
// three-point rope when cut, fades out when flagged.
class cObj00 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Obj00Work

    virtual void move();
    virtual ~cObj00() {}
    void setScrAtari(f32 r);
};

#define OBJ00_WK(o) ((Obj00Work*) (o)->free)

#endif
