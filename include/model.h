#ifndef MODEL_H
#define MODEL_H

#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "atariInfo.h"
#include "main_mem.h"

// game/math_sub.cpp (C++ linkage; math_sub.h declares them too)
void RotMatrix(Mtx m, Vec* ang);
void TransMatrix(Mtx m, Vec* pos);
void ScaleMatrix(Mtx m, Vec* scale);

// Coordinate base (game/model.cpp). Layout known only partially; pads keep offsets exact.
class cCoord : public cUnit {
public:
    Mtx mat;        // 0x0C  world matrix (partsWorldCalc: parent->mat * l_mat)
    Mtx l_mat;      // 0x3C  local matrix (rot * trans * scale)
    cCoord* pParent;  // 0x6C  parent coord (parts: the model; pl_ashley concatenates its mat)
    Vec world;      // 0x70
    Vec world_old;  // 0x7C  world of the previous frame (cAtariInfo::getSpeedVector)
    Vec world_old2; // 0x88  (pl_ashley moveBust: GetDistance3 from world)
    Vec pos;        // 0x94
    Vec ang;        // 0xA0
    Vec scale;      // 0xAC
    Vec r_scale;    // 0xB8  scale before MotionHokan rescaled it (blend: interpolated scale)
    Mtx prevMat;    // 0xC4  l_mat of the previous motion (MotionHokan interpolates from it)

    // In-class (eff_sys inlines the constructor into g_EffParentWorld's static initialiser and
    // owns the first `_vt.6cCoord` copy together with the out-of-line ~cCoord/matUpdate bodies).
    // `: cUnit(1)` (not `be_flag = 1` in the body): the be_flag store precedes the vptr store in
    // the inlined copy (cParts::cParts), the body form issues the vptr store first.
    cCoord() : cUnit(1) {
        PSMTXIdentity(mat);
        PSMTXIdentity(l_mat);
        pParent = NULL;
        scale.x = 1.0f;
        scale.y = 1.0f;
        scale.z = 1.0f;
        r_scale.x = 1.0f;
        r_scale.y = 1.0f;
        r_scale.z = 1.0f;
    }
    virtual ~cCoord() {}
    // Rebuilds l_mat (and mat) from ang / pos / scale; cModel overrides it to update the parts too.
    virtual void matUpdate() {
        RotMatrix(l_mat, &ang);
        TransMatrix(l_mat, &pos);
        ScaleMatrix(l_mat, &scale);
        PSMTXCopy(l_mat, mat);
    }
};

class cModel;
class cLight;

// One primitive part of a ModelData (dbmodule DrawObjWireframe): 0x20 header, then the GX-style stream.
struct ModelPart {
    u8 pad_0[0xB];
    u8 flags;        // 0x0B  material flags (trans shaderSetup): bit0 bump, bit1, bit2 alpha texture, bit4 specular texture in the tpl, bit7 specularSetup2
    u8 texId;        // 0x0C  texture id (trans materialSetup)
    u8 bumpTex;      // 0x0D  bump / indirect texture id
    u8 alphaTex;     // 0x0E  alpha texture id
    u8 specTex;      // 0x0F  Specular[] index (0xFF = 0)
    u8 specR;        // 0x10  specular colour
    u8 specG;        // 0x11
    u8 specB;        // 0x12
    u8 specType;     // 0x13  0: konst colour stage, 1: texture alpha
    u8 alphaRef;     // 0x14  alpha compare reference when the model's x103 is 0xFF
    u8 specPow;      // 0x15  specular scale (percent)
    u8 pad_16;
    u8 specTexOrg;   // 0x17  specular texture id when flags bit4 is set
    u32 size;        // 0x18  byte length of the primitive stream following the header
    u32 nPoly;       // 0x1C  polygon count (debug statistics)
};

// Header block ModelData::pHead points at (examine: the item's centre offset).
struct ModelDataHead {
    union {
        u32 x0;      // 0x00
        struct {
            u8 partsNo;   // 0x00  parts the model hangs on when it is not skinned (trans commonModelTrans)
            u8 parentNo;  // 0x01  parts record (model.cpp setPartsParent): parent parts index, 0xFF = the model
            u8 x2;
            u8 x3;
        };
    };
    Vec center;      // 0x04  (examine copies it into parts 0's position); parts record: parts position
};

// Model data referenced by a bin (game/model.cpp `ModelData`); only the flag word is known.
struct ModelData {
    ModelDataHead* pHead;  // 0x00
    u8 pad_4[0xC - 0x4];
    void* pClr;      // 0x0C  vertex colour array (GX_VA_CLR0, RGBA8; used when flags bit31 is set)
    void* pTex;      // 0x10  texture coordinate array (GX_VA_TEX0)
    void* pWeight;   // 0x14  skinning weights (trans MakeWeightPalette: Weight[x18] or WeightExt[x2A])
    u8 weight_palette_num;  // 0x18  Weight entries of pWeight (trans MakeWeightPalette); <= 1 with nParts == 1: rigid, original arrays
    u8 nParts;       // 0x19  parts count (cModel::setModel copies it into cModel::nParts)
    u16 displist_num;  // 0x1A  primitive (display list) part count (dbmodule DrawObjWireframe)
    struct ModelPart* pParts;  // 0x1C  first part header (0x20 bytes + primitive stream)
    u32 flags;       // 0x20  bit31: s16 tex coords (frac 8), bit30 (0x40000000): SmxGetFlag bit1, bit29: s8 normals
    u32 nTex;        // 0x24  texture count (trans: must be <= 0xF7)
    u8 shift;        // 0x28  vertex fixed-point shift (dbmodule: scale = 1 / (1 << shift))
    u8 pad_29;
    u16 weight_ext_num;  // 0x2A  extended weight entries (> 0xFF: pWeight is a WeightExt table)
    u32 shapeOfs;    // 0x2C  offset of the shape (vertex delta) table (shape.cpp)
    void* vtxOrig;   // 0x30  original vertex positions (shape.cpp ResetShape source)
    void* nrmOrig;   // 0x34  original vertex normals
    u16 nVtx;        // 0x38  vertex count (8 bytes each)
    u16 nNrm;        // 0x3A  normal count
    u32 version;     // 0x3C  0x20010801 / 0x20030817 / 0x20030818 (model.cpp: the two tables below exist from 0x20030818)
    u32 blendTbl;    // 0x40  MotionWork::blendTbl (cModel::setJointInfo); a file offset until calcModelAddr relocates it
    u32 flipTbl;     // 0x44  MotionWork::flip points 4 bytes into it (setJointInfo)
};

// Shape (morph) animation data referenced by cModelInfo::pShape (game/shape.cpp).
struct ShapeData {
    u16 nFrame;      // 0x00  frame count (low 14 bits)
    u8 num;          // 0x02  channel count
    // u8  idx[num]      0x03  shape table index per channel
    // u16 flags[num]    0x03 + num  bit2: active, bits 12-15: interpolation type
    // s32 table[num]    4-aligned after that, preceded by a marker word (< 0 once relocated)
};

// One active shape channel of a model (cModelInfo+0xA8, 5 entries).
struct ShapeKey {
    f32 rate;        // 0x00
    ShapeData* data; // 0x04
};

// Bounding volume of a model (cModelInfo+0x38).
struct ModelBound {
    Vec min;             // 0x00
    Vec center;          // 0x0C  light info origin (cLightInfo::init2 p0)
    Vec size;            // 0x18  (cLightInfo::init2 p1, copied field by field to the stack)
};

// Per-model info block (game/model.cpp `cModelInfo`, at cModel+0x15C), a cUnit managed by
// ModInfoMgr (be_flag 0x00: bit1 has shape animation (shape.cpp), 0x40 pl_leon eye; next 0x04;
// vptr 0x08). Partial layout.
class cModelInfo : public cUnit {
public:
    ModelData* pData;    // 0x0C
    void* tpl_addr;          // 0x10  texture palette of the model (eff_sys RoomEfmRegist)
    cModelInfo* pList;   // 0x14  next parts info
    u8 pad_18[0x38 - 0x18];
    ModelBound bound;    // 0x38
    union {
        Mtx mat;         // 0x5C .. 0x8C  (cModelInfo::cModelInfo: identity)
        struct {
            f32 x5C;             // 0x5C  (pl_leon setModel: face info zeroes 0x5C/0x70/0x84)
            u8 pad_60[0x70 - 0x60];
            f32 x70;             // 0x70
            u8 pad_74[0x84 - 0x74];
            f32 x84;             // 0x84
            u8 pad_88[4];
        };
    };
    union {
        u8 color[4];     // 0x8C  RGBA (word store; 0xFF fill when the RGB part is 0)
        u32 colorWord;   // 0x8C  (cModelInfo::cModelInfo: 0xFFFFFFFF)
    };
    u8 color2[4];        // 0x90  second RGBA (0x93 = 0 or 0xFF)
    void* pPosBuf[2];    // 0x94  double-buffered vertex position arrays (pG->vtx_buf_no selects)
    void* pNrmBuf[2];    // 0x9C  double-buffered vertex normal arrays
    ShapeData* pShape;   // 0xA4  current shape animation, NULL when none (shape.cpp)
    ShapeKey shape[5];   // 0xA8  blended shapes
    u32 shapeFlags;      // 0xD0  1: loop, 2: hold last frame, 4: reverse, 8: x100 weights
    s16 shape_frame;      // 0xD4
    u8 blend_mode;       // 0xD6  GXSetBlendMode table index (trans bl[]; scroll: previous color[3]; emwindow 2, TexRender 1)
    u8 pad_D7;
    f32 invisible_factor;  // 0xD8  material alpha scale 0..1 (trans: model invisible_factor * invisible_factor2 * this < 1 -> scaled mat colour)
    u16 flagsDC;         // 0xDC  bit0: has uv scroll, bit1: texture animation (pTexAnim), bit2: texBlendTbl set, bit3: alpha tex coord
    u16 blendRatio;      // 0xDE  (TexRender: 0xFF while rendered to texture); low byte = TEV konst colour
    u8 anm_no;           // 0xE0  texture animation frame (trans commonScreenMatSub; PS2 TEXANM_INFO.anm_no)
    u8 blendType;        // 0xE1
    u8 pad_E2[2];
    void* texBlendTbl;   // 0xE4  (TexRender: 6-byte table {1, 0, ?, ?, 0xF7, tex id})
    f32 uvU;             // 0xE8  current uv scroll offset (trans materialSetup tex matrix)
    f32 uvV;             // 0xEC
    f32 uvScrollU;       // 0xF0  uv scroll speed per frame
    f32 uvScrollV;       // 0xF4
    u8* pTexAnim;        // 0xF8  texture animation table: [1] = frame count, [4 + frame] = texture id
    u8 pad_FC[0x11C - 0xFC];
    u32 nAddTex;         // 0x11C  textures of pAddTpl appended after pTpl's (commonModelTrans)
    struct TEXPalette* pAddTpl;  // 0x120  additional texture palette (addTplAddr)

    cModelInfo();
    void setTplAddr(void* tpl);   // pTpl = tpl, relocated
    void addTplAddr(void* tpl);
    void setSpecular(u8 r, u8 g, u8 b);   // specular colour of every part
    void setTexBlendTbl(void* tbl);
    void resetTexBlendTbl();
    void setBlendRatio(u16 ratio);
    void setBlendType(u8 type);
};

// Light set of a model (game/lightInfo.cpp), embedded in cModel at 0x164 (0x74 bytes).
class cLightInfo {
public:
    Mtx imat;         // 0x00  light space matrix (lightHitCheckBBox transforms the light into it)
    cLight* pLight[8];        // 0x30  lights applied to the model (cLightMgr::setModel2 / setCloth)
    u8 EnableMask;   // 0x50  cLight::xF kind mask the model accepts (0x41: parent lights only)
    u8 Flag;         // 0x51  bits 0-1: 2 = follow the model matrix (obj04: updateMatrix each frame); hit check shape (0 cylinder, 1/3 sphere, 2 box)
    s8 PartsNo;      // 0x52  parts index + 1 the light origin follows (getPos), 0 = model
    u8 x53;          // 0x53
    u32 SelectMask;  // 0x54  bit i: light i never applies (setModel2; scroll: SmxWork.x4)
    Vec Offset;         // 0x58  light origin offset in the space of the coord x52 selects (shadow.cpp)
    Vec Size;        // 0x64  hit check size: x radius, y half height (cylinder), xyz box half size
    f32 Radius;      // 0x70  bounding radius from size (init2: cylinder x + y, box length, sphere x)

    cLightInfo();
    int init2(int type, int partsNo, const Vec* pOffset, const Vec* pSize, int mask);  // type -> Flag, partsNo -> PartsNo, mask -> EnableMask
    void updateMatrix(cModel* m);
    u32 getLightNum();
    cModel* getPos(cModel* m, Vec* out);  // light origin of `m` (the parts x52 - 1 selects); returns the coord it belongs to
};

// One sequence key (MotionData sequence table entry / MotionWork::key*).
struct MotionSeqKey {
    u16 frame;  // 0x00  motion frame in 10.6 fixed point
    u8 Se;      // 0x02  sound number + 1 to play at this key, 0 = none (PS2 SEQUENCE_DATA.Se)
    u8 Free;    // 0x03  free bits: player sound kind (low 3 bits) / object event bits (PS2 SEQUENCE_DATA.Free)
};

// Key-frame data header (the `data` given to MotionSetCore). Packed:
//   u16 maxFrame (low 14 bits), u8 nParts, u16 parts[nParts], u8 partsNo[nParts],
//   4-aligned u32 keyOfs[nParts] (relocated in place to absolute key pointers).
struct MotionData {
    u16 maxFrame;  // 0x00
    u8 nParts;     // 0x02
};

struct AttachCamera;   // cam_ctrl.h

// Per-model motion work (game/motion.cpp), cModel::mot at cModel+0x1D8, 0xDC bytes. The first
// 0xD0 bytes are what cModel::cModel clears (MotionWorkSub in em.h is that prefix).
struct MotionWork {
    MotionData* pMot;     // 0x00  NULL = no motion
    u32* pHermite_data;          // 0x04  per parts key data
    u16 Key_hist[2][2][3];    // 0x08  root key history [flip][rot/pos][axis]
    f32 Mot_frame_max;         // 0x20
    f32 Mot_frame;            // 0x24
    f32 Mot_frame_sav;        // 0x28
    f32 Mot_frame_old;       // 0x2C
    u8 Joint_num;            // 0x30
    u8 pad_31[3];
    u8* pJoint_no;          // 0x34  model parts index per motion parts
    u16* pJoint_kind;       // 0x38  low byte: kind (1 root pos, 0x40 root rot, 2/4/8/0x30 rot/pos/scale), bits 8-11: attach camera channel, bits 12-15: Fcc type
    u16 Null_pos;       // 0x3C  motion parts index of the root position (0xFFFF = none)
    u16 Null_rot;       // 0x3E  motion parts index of the root rotation
    u16 Mot_attr;            // 0x40  bit0: move the model by the root speed, bit1: reverse, bit2: loop, bit3: pause, bit6: flip, bit8, bit10: hokan speed blend, bit12: sequence reverse, bit13: blend parts, bit15: frame from seqFrame
    u16 Mot_state;            // 0x42  MotionSequenceCtrl result: 1 looped, 2 looped (reverse), 4 end, 8 end (reverse)
    u32 Mot_flag;           // 0x44  bit26: cross frame disabled, bit27: flip hist, bit28: no IK, bit29: keep blend, bit30: no matrix, bit31
    Vec Pos;              // 0x48  root position (current)
    Vec Pos_old;          // 0x54
    Vec Pos_dist;         // 0x60  root position change over the whole motion
    Vec Pos_world;          // 0x6C  PartsWorldPosCalc: position the parts were computed at
    Vec Pos_move_old;            // 0x78  last root speed
    Vec Ang;              // 0x84  root rotation (current)
    Vec Ang_old;          // 0x90
    Vec Ang_dist;         // 0x9C
    MotionSeqKey* pSeq_top;    // 0xA8  sequence table (NULL = linear)
    MotionSeqKey Seq;    // 0xAC  current
    MotionSeqKey Seq_old;    // 0xB0  previous
    MotionSeqKey Seq_old2;    // 0xB4  before previous
    f32 Seq_frame;         // 0xB8  frame in sequence time
    u16 Seq_frame_num;           // 0xBC  sequence length
    u8 pad_BE[2];
    f32 Seq_speed;        // 0xC0  frames per game frame
    u8 Hokan_frame;          // 0xC4  interpolation frames from the previous pose
    u8 Hokan_cnt;          // 0xC5  frames left
    u8 pad_C6[2];
    f32 Brate;        // 0xC8  weight of this work when it is another model's blend motion
    AttachCamera* pAttachCam;    // 0xCC
    MotionWork* blend;    // 0xD0  second motion blended in by MotionMove
    u16* flip;            // 0xD4  parts index remap for flipped motions
    u16* blendTbl;        // 0xD8  {count, (dst, a, b, percent)...} quaternion blended parts
};

// Parts-side motion state (cParts::motParts at 0x174): a parts has no light set, the cLightInfo
// area of a cModel holds this instead.
struct MotionParts {
    Vec pos;         // 0x174  pose before the blend motion was applied
    Vec rot;         // 0x180
    Vec scale;       // 0x18C
    union {
        u32 x198;    // 0x198
        f32 ikAng;   // 0x198  ik: previous twist angle of the effector (InverseKinematics)
    };
    u16 hist[6][3];  // 0x19C  key history: rot, pos, scale; then the same for the flipped histories
    u32 flags;       // 0x1C0  bit0 / bit16: animated this frame, bit1: skip partsWorldCalc, bit17: scale cancelled, bit24-25: skip blend, bit26: no cross frame, bit28: hokan pending, bit29: skip, bit30: apply cParts::addRot, bit31: hokan pending (blend)
                     //        ik (game/ik.cpp): bit2: IK chain root, bit4: 4-joint chain, bit6/bit11: floor search range, bit7: no IK,
                     //        bit8: heel-to-toe, bit9: no floor, bit10: reach limit, bit12: twist, bit13-15: IK plane axis
};

// Light area block (game/light_area.cpp), cModel::litArea at cModel+0x30C: scales one light's
// colour on the model.
struct EmLightArea {
    u32 x0;          // 0x00
    u32 flags;       // 0x04  bit0 active, bit1 scale valid
    s32 lightNo;     // 0x08  cLight::x140 of the light to scale
    f32 scale;       // 0x0C

    int chk(u32 bit)
    {
        if (flags & bit) {
            return 1;
        }
        return 0;
    }
    void on(u32 bit) { flags |= bit; }   // player.cpp init1: `addi rX,this,0x30C; lwz/stw 4(rX)`
};

// The object units' view of cModel+0x2B4 .. 0x320 (`obj->sub2B4.atari`, `sub2B4.pFootShadowTbl`);
// aliases the cAtariInfo / pFootShadowTbl members of cModel below. cAtariInfo is wrapped so that
// the struct has no constructor of its own and can sit in cModel's union.
struct ObjSub2B4 {
    union {
        struct {
            cAtariInfo atari;     // 0x00 .. 0x4C  (flags at 0x1A)
        };
    };
    u8 pad_4C[0x54 - 0x4C];
    void* pFootShadowTbl; // 0x54 (cModel+0x308)  foot shadow table (event ExePacket_SetOm)
    u8 pad_58[0x6C - 0x58];

    void clrFlags(u16 mask) { atari.m_flag &= mask; }
};

// Model info pool (game/model.cpp `ModInfoMgr`, 0x34 bytes): a cManager<cModelInfo>; the
// player units call the inline cManager::destroy on it (pl_ashley setRightHand/setLeftHand).
// The destructor is the implicit one and the three memory hooks are in-class: model.o emits them
// as `~cModInfoMgr, memAlloc, memFree, memClear` right after ~cModelInfo (the deferred-inline
// order is the definition order; a user-written dtor would instantiate ~cManager early).
class cModInfoMgr : public cManager<cModelInfo> {
public:
    cModInfoMgr();
    virtual void* memAlloc(u32 size) { return MemAlloc(size, 1); }
    virtual void memFree(void* p) { MemFree(p); }
    virtual void memClear(cModelInfo* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual int construct(cModelInfo* p, u32 id);

    cModelInfo* create(void* bin, void* tpl);
};

extern cModInfoMgr ModInfoMgr;

// Model parts (game/model.cpp), 0x1D8 bytes, allocated from PartsMgr (cManager<cParts>(0x1D8)):
// a cCoord with the parts chain and the bind matrix; the rest holds the motion / IK state
// (motion.h IkParts at 0xF8 / MotionParts at 0x174, pendulum.h PenParts). cModel::pParts and
// cModel::getPartsPtr() are typed cModel* throughout the sources; index a parts array through
// this type (`((cParts*) m->pParts)[2]`, objBull), a cModel* has the wrong stride.
class cParts : public cCoord {
public:
    cParts* pList;   // 0xF4  next parts of the model (the cModel::pParts chain; createSequential links them). Not `next`: cManager<cParts> must keep using cUnit::next
    Mtx lt_inv_mat;     // 0xF8  bind pose matrix (motion.h PARTS_BIND_MAT); setPartsOffset: identity with -mat translation
    Vec addRot;      // 0x128  rotation partsWorldCalc applies (x, then z, then y) while motParts.flags bit30 is set (ik.cpp overlays IkParts len / mat here)
    u8 pad_134[0x174 - 0x134];
    MotionParts motParts;  // 0x174 .. 0x1C4
    u8 pad_1C4[0x1D8 - 0x1C4];

    cParts();
    virtual ~cParts() {}
};

// Parts pool (game/model.cpp `PartsMgr`, 0x34 bytes): a cManager<cParts> (game.cpp instantiates
// roomInit / arrayAlloc / arrayFree / dispWorkNum on it); no other member is known.
class cPartsMgr : public cManager<cParts> {
public:
    cPartsMgr();
    virtual void* memAlloc(u32 size) { return MemAlloc(size, 1); }
    virtual void memFree(void* p) { MemFree(p); }
    virtual void memClear(cParts* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual int construct(cParts* p, u32 id);

    // `n` consecutive free works linked through pNext (the sequential parts list cModel::be_flag
    // bit13 marks), NULL when no run is free.
    cParts* createSequential(u32 n);
};
extern cPartsMgr PartsMgr;

struct MotionWorkSub;   // em.h
class cTexChg;          // trans.h

// Model (game/model.cpp), sizeof 0x320: cEm / cObj / cMap fields start at 0x320. The parts hanging
// off pParts are cParts (0x1D8, above); the sources address them as cModel* (cCoord members only).
class cModel : public cCoord {
public:
    union {
        cModel* pParts;      // 0xF4 child parts list (a cParts chain; every source addresses it as cModel*)
        cParts* pList;  // 0xF4 the same pointer typed as the parts (model.cpp)
    };
    u32 serial;      // 0xF8  identity check for parent links (obj04: parent->serial == work.parentSerial)

    u8 r_no_0;  // 0xFC  routine / state
    u8 r_no_1;  // 0xFD  routine index (move table)
    u8 r_no_2;  // 0xFE  step
    u8 r_no_3;  // 0xFF  (t_option clears FC..FF after a weapon change)

    u8 id;           // 0x100
    u8 type;         // 0x101 per-object sub type
    u8 nParts;       // 0x102
    u8 alpha_omit;         // 0x103  (scroll: 0x80 = SmxSetFlag bit3, 0xFF = off)
    Vec speed;       // 0x104
    Vec pos_old;      // 0x110  position before the speed was added (obj04 collision segment)
    Vec Wall_norm;     // 0x11C  normal of the wall the scenario check pushed the model out of (atari scrAtCheckSphere), zero when none
    union {
        struct {
            Vec* pFloor_norm;  // 0x128  player: floor normal the shoulder camera tilts with (cam_qfps setPlayerLocation)
            u8 z_mode;         // 0x12C  (TexRenderModSet sets 2)
            u8 TevScaleGroup;         // 0x12D  (pl_leon setModel sets 1)
            u8 kindid;         // 0x12E  2 = scroll (Smd) object
            u8 ot_type;         // 0x12F  scroll: SmxWork.type2 (3 by default)
            void* pCldShMd;  // 0x130  (db_work "pCldShMd")
            u8 Shd_color;       // 0x134  (db_work "SHD COL")
            u8 CullMode;         // 0x135  scroll: SmxWork.x3, db_work "CullMode"
            u8 Shader_type;         // 0x136  TexRender: 2 while rendered to texture, 0 after
            u8 Refract_pow;         // 0x137  TexRender: 0x10
            u8 Refract_ratio;         // 0x138  TexRender: 0x90
            u8 AddAmb_r;         // 0x139  mirror: 0xFF; trans_lit adds it to the ambient colour
            u8 AddAmb_g;         // 0x13A  mirror: 0xFF; trans_lit adds it to the ambient colour
            u8 AddAmb_b;         // 0x13B  mirror: 0xFF; trans_lit adds it to the ambient colour
            int Fix_parts;    // 0x13C  parts index + 1 whose world position partsFixAdjust holds (partsFixMemory), 0 = none
            Vec Fix_pos;      // 0x140  that parts' world position when it was fixed
            u8 invisible_trg;         // 0x14C  (cModel::cModel: 0)
            u8 invisible_old;         // 0x14D
            u8 invisible_mode;         // 0x14E
            u8 invisible_busy;         // 0x14F
        };
        // Effect model parts physics (obj05 cObj05::move runs its parts as loose particles).
        struct {
            int efmStat;     // 0x128  0 waiting, 1 flying, 2 at rest
            Vec efmSpd;      // 0x12C
            Vec efmRotSpd;   // 0x138
        };
    };
    // 0x150..0x15C: pendulum parts treat these three words as a Vec (obj14 adds the hit impulse
    // to parts 1/2 here); the object itself keeps its alpha at 0x154.
    union {
        f32 x150;          // 0x150
        u32 invisible_timer;         // 0x150  (cModel::cModel clears it as a word)
    };
    f32 invisible_factor;             // 0x154  0..1 (obj04: work color a / 255)
    f32 invisible_factor2;              // 0x158
    cModelInfo* pModelInfo;     // 0x15C
    cModelInfo* pShadowModelInfo; // 0x160  (db_work "pShMdIfo")
    cLightInfo LightInfo;  // 0x164 .. 0x1D8

    // 0x1D8 .. 0x2B4  motion work (motion.h MOTION(m), cMotBase `m->Motion`). The names the
    // character (cEm) and object (cObj) units use for its fields alias it.
    union {
        MotionWork Motion;                // 0x1D8
        struct {
            void* pMotion;             // 0x1D8  Motion.pMot: current motion data, NULL = stopped (pl_push stopTarget)
            u8 pad_1DC[0x218 - 0x1DC];
            u16 motFlags;              // 0x218  Motion.Mot_attr (bit0: move the model by the root speed; pl_npc clears it)
            u16 motState;              // 0x21A  Motion.Mot_state (emobj EmObjMove clears it when no motion plays)
            u32 motFlags2;             // 0x21C  Motion.Mot_flag (emhit: bit30 = no matrix update before MotionMove; obj26MatCalc/objMissile set it when following a parent)
            u8 pad_220[0x244 - 0x220];
            Vec satPos;                // 0x244  Motion.Pos_world: pos after the scenario collision moved the model (atari at_pos_calc)
            u8 pad_250[0x28A - 0x250];
            u8 seNo;                   // 0x28A  Motion.Seq_old.x2: sound number + 1 to play at parts 0 this frame (emMove SndCall(8, ...)), 0 = none
            union {
                u8 seFlags28B;         // 0x28B  Motion.Seq_old.x3: player: sound kind of the motion key (low 3 bits, pl_class seqSeCtrl)
                u8 motEvent;           // 0x28B  event bits of the current sequence key (objRobo SE / effects)
            };
            u8 pad_28C[4];
            union {
                f32 frame;             // 0x290  Motion.Seq_frame: motion frame (db_cam prints it as an int)
                f32 motFrame;          // 0x290  (objGondola R0_Up waits for frame 4105)
            };
            union {
                u16 frameMax;          // 0x294  Motion.Seq_frame_num
                u16 motSeqMax;         // 0x294  (objRocket: the rocket burns out at seqFrame >= seqMax - 1)
            };
            u8 pad_296[2];
            f32 motSpeedRate;          // 0x298  Motion.Seq_speed (objWep resetMotion: 1.0)
            u8 pad_29C;
            u8 motHokanCnt;            // 0x29D  Motion.Hokan_cnt: blend frames passed back to MotionSetCore (emrock/em2b/pl0f escape run)
            u8 pad_29E[6];
            void* p2A4;                // 0x2A4  Motion.pAttachCam: AttachCamera / 0x98-byte EmWork2A4 (player.cpp mem_alloc; cam_ctrl reads its byte 5; objRobo SetObjRobo)
            union {
                MotionWorkSub* blendMot;   // 0x2A8  Motion.blend: second motion blended in (pl_class: &m_SubMot / cMot3::work)
                MotionWork* motBlend;      // 0x2A8  (objGondola setVib: the sub motion work)
            };
            u16* pXFlip;              // 0x2AC  Motion.flip: parts index remap of flipped motions (emdoor: emDoor_xflip_tbl)
            u32 pDblJnt;                  // 0x2B0  Motion.blendTbl (obj18: parts matrices are only recomputed while 0)
        };
    };
    // 0x2B4 .. 0x320  collision info, foot shadow table, light area, texture change. cAtariInfo
    // has a constructor, so it is wrapped in an anonymous struct (no member constructor call);
    // cModel::cModel constructs it explicitly where the original does (after cLightInfo's).
    union {
        struct {
            cAtariInfo atari;          // 0x2B4 .. 0x300  (rect size at 0x2C0/0x2C4)
            u32 inscreen_pos;                  // 0x300  (cModel::cModel clears it)
            u32 pPath;                  // 0x304  (cModel::cModel clears it)
            void* pFootShadowTbl;      // 0x308  foot shadow table (pl_leon: pl_fs_tbl; trans FootShadow)
            EmLightArea litArea;       // 0x30C .. 0x31C  light_area: per-light colour scale (trans_lit lightSetColor)
            cTexChg* pTexChg;          // 0x31C  texture change work (trans commonModelTrans: pTexChg->move)
        };
        struct {
            ObjSub2B4 sub2B4;          // 0x2B4  the object units' names for the same bytes
        };
    };

    cModel();
    virtual ~cModel() {}
    virtual void matUpdate();
    virtual void move();
    virtual void setNoSuspend(int on);

    cModel* getPartsPtr(int no);  // -1: the model itself; NULL (and a log) when out of range
    int modelInit(void* bin, void* tpl);  // returns the cModelInfo* (pl_leon range-checks it)
    int initJoint(void* bin);     // parts list from the bin's parts records (makePartsList / setPartsParent / setPartsOffset / setJointInfo)
    void releaseJoint();          // releasePartsList(0) when there are parts
    void setPartsParent();        // pParent of every parts from the bin records
    void matBlend(f32 rate);      // parts pose = rate * own pose + (1 - rate) * worldMat pose (motion.cpp MotionMove blends)
    void setSca(Vec* scale);      // scale = *scale; matUpdate()
    int deleteModelData(ModelData* data);   // destroy the info using `data` (1 when found)
    int swapModelInfo(ModelData* data, cModelInfo* info);   // replace the info using `data` by `info` (1 when found)
    void releaseModelInfo();      // destroy every info
    int makePartsList(int n);     // n parts (0: nParts) from PartsMgr, sequential when possible (be_flag bit13)
    void setJointInfo(void* bin); // mot.blendTbl / mot.flip from the bin (version 0x20030818)
    void releasePartsList(int no);  // destroy the parts from `no` on (0: all)
    void motionPause();
    void addModel(cModelInfo* info);
    int deleteModelInfo(cModelInfo* info);   // 1 when the info was in the list
    void partsMatCalc();
    void partsWorldCalc();
    void setPos(Vec* pos);
    void setAng(Vec* ang);
    void updateOldPos();   // oldPos = pos for the model and its parts (emMove)
    void push();   // pl_sub PlChangeData
    void drawAllBoundingBox(cModelInfo* info);
    void debugSkeletonDisp();
    void error();  // too many lights: flags the model and logs it
    // MotionSetCore(this, &motion (0x1D8), data, a, b, c, d) / MotionMove(this, 0)
    void motionSet(void* data, int a, int b, int c, int d);  // void: a following call then keeps its arg li`s ranked below the `this` copy (pl_knife down00)
    int motionMove();
    int isTrans();  // be_flag bit1 (visible) and be_flag != 0 (objWep / objRocket)
    // Hang parts 0 on `parent` at pos / rot (objRocket loadRocket); the 4-argument form
    // selects parts `partsNo` of the parent (-1: the parent itself).
    void setParent(cModel* parent, Vec* pos, Vec* rot);
    void setParent(cModel* parent, int partsNo, Vec* pos, Vec* rot);
    void moveDataAddr(int ofs);   // model data moved by `ofs` bytes (block.cpp memory compaction)
    // Copies the parts positions of a model bin (parts bin of an event costume, event SetPartsSub).
    void setPartsOffset(void* bin);
    // setPos / setAng / MotionClear(0) / matUpdate() (event ExeEndEvt puts the player back).
    void zeroPartsPosInit(Vec* pos, Vec* rot);
    void partsFixMemory(int no);  // (pl_class setFootwork: 0x13)
    void partsFixAdjust();        // (player cPlayer::move)

    // The managers the models allocate from (game/model.cpp, .sdata: &ModInfoMgr / &PartsMgr;
    // sscrn SubScreenExitCore restores them after the sub screen swapped the area out).
    static class cModInfoMgr* mm;
    static class cPartsMgr* pm;
};


// game/model.cpp (C linkage): parts `no` of a parts list (NULL when out of range).
extern "C" cModel* GetPartsAddr(cModel* parts, int no);
// game/model.cpp (C linkage): relocate a TPL's file offsets to pointers (trans SpecularInit).
extern "C" void calcTplAddr(struct TEXPalette* tpl);

// game/model.cpp: shows / hides model info `no` of `m` (the rooms hide the player's weapon models).
extern "C" void ModelInfoSetTrans(cModel* m, int no, int on);
// game/model.cpp: turns on the reflection flag of model info `no` (r11b: the water render targets).
void ModelInfoRefrectOn(cModel* m, int no);

#endif
