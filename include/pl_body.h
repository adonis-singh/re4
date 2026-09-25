#ifndef PL_BODY_H
#define PL_BODY_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "math_sub.h"
#include "main_mem.h"

// Face shape motion data built by cPlBody::makeSpaeData (PS2 PL_SHAPE_DATA, 0x58 bytes): a header,
// two key tables and four keys.
struct SHAPE_MOT_HEADER {
    u32 max_frame;  // 0x00  0x101
    u32 tbl_num;    // 0x04  2
};
struct SHAPE_MOT_TBL {
    u32 offset;     // 0x00  byte offset of the table's keys (0x18 / 0x38)
    u16 shape_id;   // 0x04
    u16 key_num;    // 0x06  2
};
struct SHAPE_MOT {
    s32 frame;      // 0x00  0 / 0x100
    f32 value;      // 0x04
    f32 r_value;    // 0x08
    f32 l_value;    // 0x0C
};
struct SpaeData {
    SHAPE_MOT_HEADER head;  // 0x00
    SHAPE_MOT_TBL tbl[2];   // 0x08
    SHAPE_MOT mot[4];       // 0x18
};

// Player body helper (game/pl_body.cpp, `new`ed by cPlayer::init0 into cEm::pBody at 0x794, 0xF0
// bytes): the extra model infos hung off the player (hands, head, face, hair) and the data pointers
// they were built from (pl_leon setModel/setRightHand/...), the waist twist and the weapon hand.
class cPlBody {
public:
    void* pRightData;            // 0x00  right hand model data (0 = none)
    void* pLeftData;             // 0x04  left hand model data (0 = none)
    void* pHeadData;             // 0x08  head model data
    void* pWepHand;              // 0x0C  weapon hand model data (initWepHand; setRightHand(1) uses it)
    cModelInfo* m_pFace;          // 0x10  head model info (face shape animation target of ShapeSet/ShapeEnd)
    cModelInfo* m_pArmR;         // 0x14  (PS2 m_pArmR; only cleared on GC)
    cModelInfo* m_pArmL;         // 0x18  (PS2 m_pArmL; only cleared on GC)
    cModelInfo* m_pHandR;          // 0x1C  right hand model info
    cModelInfo* m_pHandL;           // 0x20  left hand model info
    cModelInfo* m_pHead;         // 0x24
    cModelInfo* m_pHair;         // 0x28  (flags |= 0x40)
    cModelInfo* m_pKnife;        // 0x2C  the knife model info (setKnife scales its matrix to 0 / 1)
    u32 nowLhandNo;                  // 0x30  current left hand item no
    u32 oldLhandNo;              // 0x34  previous one (setLeftHand(0x63) restores it)
    cModel* m_pMod;              // 0x38
    f32 m_WaistY;                   // 0x3C  waist twist angle (waistSet)
    SpaeData spae[2];            // 0x40

    cPlBody(cModel* model);
    void move();
    void waistSet(f32 y);
    void setKnife(bool on)
    {
        // Stores through face would make the compiler reload m_pKnife after each one.
        cModelInfo* face = m_pKnife;

        if (VALID_PTR(face)) {
            if (on) {
                face->mat[2][2] = 1.0f;
                face->mat[1][1] = 1.0f;
                face->mat[0][0] = 1.0f;
            } else {
                face->mat[2][2] = 0.0f;
                face->mat[1][1] = 0.0f;
                face->mat[0][0] = 0.0f;
            }
        }
    }
    void waistMove();
    void makeSpaeData();
    void initWepHand(u32 addr);
};

#endif
