#ifndef OBJ1B_H
#define OBJ1B_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Spear work (game/obj1b.cpp `cObjSpear`): thrown (R1_Throw), stuck in a parts of the enemy it hit
// (R1_Parent), then falling as a three-point rope (R1_Fall) and fading out (R1_LostWait / Lost).
struct SpearWork {
    u32 Be_flg;             // 0x00  bit0: keep the parent parts matrix as it is (no axis normalize)
    int Timer;              // 0x04  LostWait: frames before the fade (120); Throw: frames between the flight SEs
    int Timer2;             // 0x08  Throw: flight frames left (60)
    u8 pad_C[0xC];
    cModel* pEm_oya;        // 0x18
    int oya_parts;          // 0x1C
    Vec spd[3];             // 0x20  rope point speeds (R1_Fall)
    Vec throw_v;            // 0x44
    EmAtkInfo* pAtk;        // 0x50  always 0, never dereferenced (PS2 ATK_INFO*)
    int Lost_wait;          // 0x54  frames until the spear falls off its parent (1800)
    int Eff_timer;          // 0x58  frames of the stuck-in-boss effect (600, every 2nd frame)
    u8 se_id_fall;          // 0x5C  landing SE (0xFF = none)
    u8 se_no_fall;          // 0x5D
    u8 em_id_fall;          // 0x5E
    u8 se_ck_fall;          // 0x5F
    u8 fall_type;           // 0x60  rope offsets table row (R1_Fall)
    u8 se_id_hit;           // 0x61
    u8 se_no_hit;           // 0x62
    u8 em_id_hit;           // 0x63
    u8 se_id_dm;            // 0x64
    u8 se_no_dm;            // 0x65
    u8 em_id_dm;            // 0x66
    u8 se_id_throw;         // 0x67  flight SE (0xFF = none)
    u8 se_no_throw;         // 0x68
    u8 em_id_throw;         // 0x69
    u8 eff_id_fall;         // 0x6A  landing effect (0xFF = none)
    u8 est_id_fall;         // 0x6B
    u8 eff_id_hit;          // 0x6C
    u8 est_id_hit;          // 0x6D
    u8 eff_id_dm;           // 0x6E
    u8 est_id_dm;           // 0x6F
    u8 EffKindId;           // 0x70  effect owner id deleted on landing (0x32)
};

// Spear (obj 0x1B): thrown by an enemy (R1_Throw), sticks into the enemy it hits (R1_Parent:
// follows a parts of the target), falls off as a three-point rope (R1_Fall) and fades out (Lost).
class cObjSpear : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  SpearWork

    virtual void move();
    virtual void beginEvent(u32 mode);
    virtual ~cObjSpear() {}
    void setParent(cModel* parent, int partsNo, int noNormalize);
    void setFall(u8 type, Vec* dir);
    void setThrow(Vec* dir);
    void setLost();
};

#define SPEAR_WK(o) ((SpearWork*) (o)->free)

#endif
