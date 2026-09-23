#ifndef ATARI_H
#define ATARI_H

#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "cManager.h"
#include "at_sub.h"
#include "at_sub2.h"
#include "main_mem.h"
#include "cFlag.h"

class cModel;
class cAtariInfo;

#line 8 "D:/Bio4/Prog/atari.h"

// Scenario collision data file (SAT): counts, then the vertex, face normal, edge, polygon and
// block tables back to back (cSat::operator= computes the table pointers).
class cSatFile {
public:
    u8 m_Version;           // 0x00  0xFF for a file (a cSatHeader has bit7 set instead)
    u8 x1;
    u16 m_nVertex;     // 0x02
    u16 m_nNormal;     // 0x04
    u16 m_nEdge;       // 0x06
    u16 x8;
    u16 m_nPolygon;       // 0x0A  polygon total (< 0x2000)
    u16 m_nFloor;          // 0x0C  polygon groups: [0, nA), [nA, nA + nB), the last nC (cSatMgr::disp)
    u16 m_nSlope;          // 0x0E
    u16 m_nWall;          // 0x10
    u16 m_nBlock;         // 0x12
    // 0x14: Vec vtx[nVertex]; Vec nrm[nNormal]; Vec edge[nEdge]; AtPoly poly[nPoly]; cSatBlock blocks

    Vec* getVertexPtr();
    int dataCheck();
};

// Table of SAT files (room collision archive): offsets from the header.
class cSatHeader {
public:
    u8 m_Version;           // 0x00  bit7 set
    u8 pad_1[3];
    u32 ofs[0];      // 0x04

    cSatFile* getSat(int no);
};

// Spatial partition of a SAT: an XZ box holding polygon indices, or (flag bit0) a child block
// chain in place of the indices. `next` is stored as a relative offset in the file
// (cSat::blockInit turns it into a pointer).
class cSatBlock {
public:
    Vec min;         // 0x00  box minimum (y unused)
    Vec m_Size;        // 0x0C  box size
    u16 m_nFloor;          // 0x18  indices of group A (floors: flag 0x40 checks [0, n0 + n1))
    u16 m_nSlope;          // 0x1A  group B
    u16 m_nWall;          // 0x1C  group C (walls: flag 0x80 checks [n0 + n1, n0 + n1 + n2))
    u16 m_Flag;        // 0x1E  bit0: `idx` holds a child cSatBlock
    cSatBlock* m_pList; // 0x20
    u16 idx[0];      // 0x24  polygon indices

    int lineOverlap(Vec* center, Vec* w, Vec* v);
    int hitCheckSphere(Vec* pos0, Vec* pos1, f32 radius);
};

// One scenario collision piece (game/atari.cpp), returned by cSatMgr::create. Owners toggle
// the flag byte (emobj setSatMain / clrSat: bit2 = active).
class cSat : public cUnit {
public:
    enum FLAG {
        FLAG_MEM = 1,      // pFile was allocated by cSatMgr::create (freed by destroy)
        FLAG_ENABLE = 2,   // takes part in the collision checks
    };

    Vec* vtx;        // 0x0C  (the three table pointers double as the AtPolyData the at_sub checks take)
    Vec* norm_p;        // 0x10
    Vec* edge_p;       // 0x14
    AtPoly* poly_p;    // 0x18
    u16 vertex_num;     // 0x1C
    u16 polygon_num;       // 0x1E
    u16 floor_num;          // 0x20
    u16 slope_num;          // 0x22
    u16 wall_num;          // 0x24
    u16 bb_num;         // 0x26
    u16 normal_num;     // 0x28
    cFlag<u8, FLAG> m_Flag;   // 0x2A
    u8 pad_2B;
    u16 edge_num;       // 0x2C
    u8 pad_2E[2];
    cSatBlock* block_p;  // 0x30  root block
    u8 pad_34[0x58 - 0x34];
    cSatFile* pFile; // 0x58
    u32 x5C;         // 0x5C
    Mtx mat;         // 0x60  piece -> world
    Mtx imat;         // 0x90  world -> piece

    cSat() : cUnit(1) { m_Flag.reset(); }
    void init(cSatFile* pSf, Vec* pos, Vec* ang);
    void setCoord(Vec* pos, Vec* ang);
    void setMatrix(Mtx mat0);
    cSat& operator=(cSatFile* f);
    void blockInit(cSatBlock* pBlock);
    void disp(int poly_num, u32 col, int mode);
    // alive and taking part in the checks (hides cUnit::isAlive for cManager<cSat>::destroy)
    int isAlive();
    void setEnable() { m_Flag.on(FLAG_ENABLE); }
    void setDisable() { m_Flag.off(FLAG_ENABLE); }
    int isEnable() { return m_Flag.check(FLAG_ENABLE); }
};

inline int cSat::isAlive()
{
    if ((be_flag & 0x201) == 1 && isEnable()) {
        return 1;
    }
    return 0;
}

// Scenario collision manager (game/atari.cpp; SatMgr is the room, EatMgr the effect set).
class cSatMgr : public cManager<cSat> {
public:
    int type;        // 0x34  copied to SEck (at_sub attribute filter bypass) by every hitCheck2

    cSatMgr();
#line 381 "D:/Bio4/Prog/atari.h"
    virtual void* memAlloc(u32 size) { return MEM_ALLOC(size, 1, 13); }
    virtual void memFree(void* p) { Mem_free(p); }
    virtual void memClear(cSat* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual void destroy(cSat* pEm);
    virtual int construct(cSat* pSat, u32 room_no);

    // Runtime scenario piece from a 4-corner polygon (createFloorSat / createBoxSat / createSat by
    // flag bits 0x200 / 0x100); returns the registered piece or NULL.
    cSat* create(Vec* pPos, Vec* pAng, Vec* pVec, f32 height, u32 attr, u32 flag);
    // Piece from prebuilt collision data (obj15 cObjGatling::setEat: EatMgr.create(data, 0, &pos, &rot, type)).
    cSat* create(void* data, int flag, Vec* pos, Vec* rot, u8 type);
    // Ray from `top` down to `bottom`; returns the hit attribute, hit point in `hit`; `attr`
    // receives the address of the hit polygon's normal (in the piece's space).
    int hitCheck2(Vec* top, Vec* bottom, Vec* pCross, u32* ppNorm, int flag, int mask);
    // Line segment `a`-`b` against the scenario; hit point and normal out. Returns 0 when nothing was hit.
    int hitCheck(Vec* pos0, Vec* pos1, Vec* pCross, Vec* pNorm, int flag, int mask);
    // Floor height under `pos`, searching `up` above and `down` below it.
    f32 getFloor(Vec* pos, u32* ppNorm, f32 above_limit, f32 below_limit, int mask);
    // Sphere of radius `r` moving from `a` to `b` against the scenario; `b` is pushed out of the
    // polygons (cLight::hitAdjust). Returns 1 when the sphere was adjusted.
    int polySphereCk(Vec* a, Vec* b, f32 radius, int flag, Vec* pNorm, int mask);
    // Debug draw of the collision polygons (t_option "SCROLL VIEW").
    void disp(int mode);
    // Model against the scenario (obj00: `SatMgr.check(this, 0)`).
    int check(cModel* pMod, int mask);
    int checkRect(cModel* pMod);
    int checkAir(cModel* pMod, int mask);
    f32 scrAtCheckSphere(cModel* pMod, cAtariInfo* pAt, int mask);
    f32 scrAtCheckSphereAir(cModel* pMod, cAtariInfo* pAt, int mask);
    void wallAdjust(Vec* pNorm, Vec* pos_old, Vec* pos_new, f32 radius, int flag, int mask);
    // Sphere of radius `r` moving from `oldPos` to `pos`; `pos` is pushed out of the polygons and
    // the hit normal goes to `nrm` (zero when nothing was hit). obj01 grenade bounce.
    void adjust(Vec* pNorm, Vec* pos_old, Vec* pos_new, f32 radius, int flag, int mask);
};

extern cSatMgr SatMgr;

enum EAT_EFFECT_TYPE {
    EAT_ET_NORMAL = 0,
    EAT_ET_BULLET = 1,
    EAT_ET_WATER = 2,
    EAT_ET_PAD = 3,
    EAT_ET_ROOM0 = 4,
    EAT_ET_ROOM1 = 5,
    EAT_ET_ROOM2 = 6,
    EAT_ET_ROOM3 = 7
};

// Effect collision manager (game/atari.cpp `EatMgr`, 0x260 bytes).
class cEatMgr : public cSatMgr {
public:
    AtEffInfo effInfo[8];  // 0x38  per effect type hit effect ids
    u8 useFlag[8];           // 0x258  effInfo[i] registered

    cEatMgr();
    virtual void log(const char* fmt, ...);

    void initEffInfo();
    void registEffInfo(int type, AtEffInfo* pEi);
    AtEffInfo* getEffInfo(int type);
};

extern cEatMgr EatMgr;

extern "C" {
// Effect type of a hitCheck attribute word (game/at_sub.cpp).
int EatGetEffectType(u32 rgba);
}

#endif
