#ifndef OBJ18_H
#define OBJ18_H

#include "obj.h"

#include "types.h"
#include "vec.h"

class cObj;
class cModel;

// game/obj18.cpp: event costume / cloth objects (C++ linkage; obj18.cpp declares them itself).
cObj* SetObj18(void* bin, void* tpl, Vec* pos, Vec* rot, int type);
int DelObj18(cObj* pObj);
void OyaSetObj18(cObj* obj, cModel* oya, int partsNo);
int obj18GetOya(cModel** oya, cObj* pObj);
void Obj18CmfSet(cObj* pObj, u32 commonFlag);
u32 Obj18CmfGet(cObj* pObj);

// Event costume / cloth model work (game/obj18.cpp): a model that follows a parts of its parent
// (like obj00) and runs one of the cloth simulations by `type`.
struct Obj18Work {
    u32 be_flag;            // 0x00  bit3: blending toward the parent, bit6: cloth simulation off
    u8 pad_4[0xC];
    cModel* pEm_oya;          // 0x10  parent
    int oya_parts;          // 0x14
    f32 oya_hokan;             // 0x18  blend rate (1.0 = parent matrix)
    f32 oya_hokan_add;          // 0x1C
    u8 pad_20[0x14];
    Mtx hokan_mat;              // 0x34  previous parent matrix
    u32 x64;              // 0x64
    u32 obj18_type;             // 0x68  OBJ18_TYPE (SetObj18 type, cloth set)
    u32 CommonFlag;              // 0x6C  Obj18CmfSet/Get flag bits
    cObj* child;          // 0x70  ribbon / rope object created by SetObj18
    int ObjChainFlagCommon;              // 0x74  bit26 (0x04000000): event ControlTransFlag skips the child flags
    char NameMod[12];     // 0x78  event model name of the packet that created it (event ExePacket_SetOm; PS2 NameMod)
    u8 DebugFlag;         // 0x84
    u8 pad_85[3];
};

// Event costume / cloth model: follows a parts of its parent with a slerp blend and runs the
// cloth simulation selected by `type` (player costumes, enemy cloth sets, the ribbon / rope).
class cObj18 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Obj18Work

    virtual void move();
    virtual ~cObj18() {}
};

#define OBJ18_WK(o) ((Obj18Work*) (o)->free)

#endif
