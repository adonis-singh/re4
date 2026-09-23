#ifndef LIGHT_H
#define LIGHT_H

#include "types.h"
#include "vec.h"
#include "gx.h"
#include "cManager.h"
#include "db_log.h"
#include "main_mem.h"
#include "lightPath.h"

class cModel;
class cEm;
class cCoord;

// Spot block of a light (0x40 bytes, cLight+0x38 / cLightWork+0x2C). Only the direction is known.
struct LightSpot {
    Vec Normal;        // 0x00 direction
    union {
        f32 A0;    // 0x0C  spot cutoff angle (GXInitLightSpot); custom: a0
        u32 flags;     // 0x0C  parallel: bit0 = direction is in view space
    };
    f32 A1;          // 0x10  distance fade width (trans_lit); custom: a1
    f32 A2;            // 0x14  custom attenuation
    f32 K0;            // 0x18
    f32 K1;            // 0x1C
    f32 K2;            // 0x20
    u8 pad_24[0x40 - 0x24];
};

// Per-type work block (0x80 bytes, cLight+0x78 / cLightWork+0x6C). The first word is a colour
// (cLit::versionUp copies it to the base colour for type 1 lights).
struct LightSub {
    GXColor color;     // 0x00
    u8 pad_4[0x80 - 0x4];
};

// Path block (0x40 bytes, cLight+0xF8 / cLightWork+0xEC).
struct LightPath {
    u8 pad_0[0x40];
};

// One light entry of a light cut in the .lit file (0x12C bytes); cLight::operator= loads it.
class cLight;
class cLightWork {
public:
    u8 BeFlag;           // 0x00  -> cLight::be_flag
    u8 xD;             // 0x01  -> cLight::xD (spot type: 3 / 6 have a direction)
    u8 Type;           // 0x02  -> cLight::type (per-type move handler, construct id)
    u8 xF;             // 0x03  -> cLight::xF (screen kind mask; 0x10 cloth, 0x40 set by versionUp)
    Vec Pos;           // 0x04
    f32 Radius;        // 0x10  -> cLight::Radius
    GXColor Col;     // 0x14
    f32 Intensity;         // 0x18
    u8 ParentType;     // 0x1C
    u8 Kind;           // 0x1D
    u8 Attribute;           // 0x1E
    u8 Priority;            // 0x1F
    u32 ParentNo;      // 0x20  parts no << 16 | parent no
    u16 HitRadius;           // 0x24  hit adjust radius
    u16 Dummy82;       // 0x26  (PS2 cLightWork Dummy82)
    u32 Dummy9;        // 0x28  (PS2 cLightWork Dummy9)
    LightSpot spot;    // 0x2C
    LightSub sub;      // 0x6C
    LightPath path;    // 0xEC

    cLightWork& operator=(cLight& l);
};

// One light work (sizeof 0x1D4). Per-type modules (light01..light10) keep their state in `work`.
class cLight : public cUnit {
public:
    u8 xC;             // 0x0C
    u8 xD;             // 0x0D  spot type (setSpotNormal accepts 3 and 6)
    u8 Type;           // 0x0E  per-type move handler index
    u8 xF;             // 0x0F  screen kind mask
    Vec Pos;           // 0x10
    f32 Radius;        // 0x1C  attenuation range (trans_lit: intensity * (Radius - d) / Radius; esp11: sizeX * scale * 10)
    GXColor Col;     // 0x20 base color
    f32 Intensity;         // 0x24
    u8 ParentType;     // 0x28  0 none, 1 enemy, 2 scroll group, 3 room etc model, 4 object
    u8 Kind;           // 0x29  (0x7F = item light)
    u8 Attribute;           // 0x2A  (db_work "ATTR")
    u8 Priority;            // 0x2B
    union {
        u32 ParentNo;  // 0x2C  parts no << 16 | parent no
        struct {
            u16 partsNo;   // 0x2C
            u16 no;        // 0x2E
        } parent;
    };
    u16 HitRadius;           // 0x30  hit adjust radius
    u16 Dummy82;       // 0x32  (PS2 cLight Dummy82)
    u32 Dummy9;        // 0x34  (PS2 cLight Dummy9)
    union {
        Vec normal;        // 0x38 direction
        LightSpot spot;    // 0x38 .. 0x78
    };
    union {
        u8 work[0x40];     // 0x78 per-light-type work area
        LightSub sub;      // 0x78 .. 0xF8
    };
    LightPath path;    // 0xF8 .. 0x138
    u8 Rno0;           // 0x138  per-type routine step (light05/light07 path lights)
    u8 Rno1;           // 0x139  (PS2 Rno1; db_light clears Rno0..Rno3 together)
    u8 Rno2;           // 0x13A  (PS2 Rno2)
    u8 Rno3;           // 0x13B  (PS2 Rno3)
    GXColor DispCol;  // 0x13C color actually applied
    u16 LitIndex;      // 0x140  index in the cut (0xFFFF = none; trans_lit compares it zero-extended)
    u8 pad_142[2];
    Vec World;        // 0x144  position actually applied (db_work draws a sphere of radius x1C here)
    cModel* pParent;   // 0x150
#ifndef LIGHT_H_CLIGHT_154
    // The debug tool objects (tools/db_light.cpp) were built against a light.h revision where cLight
    // ended here (the tool's cLight member is 0x154 bytes); the DOL's is 0x1D4.
    u8 pad_154[0x1D4 - 0x154];
#endif

    cLight();
    virtual ~cLight() {}
    // the position actually applied (inlined into the hit checks; the out-of-line copy is stripped)
    void getPos(Vec* dst) { *dst = World; }
    void move();
    cLight& operator=(cLightWork& w);
    int checkScr();
    void setPartsNo(int pno);
    int setParent(u8 type, u32 no);
    int setParent(cModel* pMod);
    cModel* calcParent();
    cCoord* getCoord();
    int isParent(cModel* pMod);
    int getPos2(Vec* pLiPos, Vec* pPos);
    int calcPos(Vec* pLiPos, Vec* pPos);
    int getNormal(Vec* pInNorm, Vec* pNorm);
    void setTrans(int on_off);
    void hitAdjust();
    void setSpotNormal(Vec* normal);
    void setSpotTarget(Vec* pos);
};

class cLight01 : public cLight {
public:
    cLight01();
};

class cLight07 : public cLight {
public:
    cLight07();
};

class cLight08 : public cLight {
public:
    cLight08();
};

// Path file header (cLightMgr::initPath): count, then offsets to each path from the header.
struct LightPathHeader {
    u8 num;            // 0x00
    u8 pad_1[3];
    // 0x04: u32[num] byte offset of each path from the header (0 = none)
};

// Fog block (cLightEnv+0x8, copied to `fogNew` by setEnv).
struct LightFog {
    s32 Type;          // 0x00  GX fog type (0 = off)
    f32 Start;         // 0x04
    f32 End;           // 0x08
    GXColor Color;     // 0x0C
};

// Wind of the pendulum system (cLightEnv+0xEC).
class cPenWind {
public:
    s8 direction;            // 0x00  angle -128..127 (units of pi/127)
    u8 power;          // 0x01
    u8 frequency;             // 0x02

    void set();
};

// Light cut: environment block (0x104 bytes) followed by nLight cLightWork entries. The
// manager keeps a copy of the current one at cLightMgr+0x38 (returned by getEnvPtr).
struct cLightEnv {
    GXColor AmbientScr;     // 0x00  model ambient (trans_lit LightSetModel / cloth / water; versionUp 0x23 copies it to AmbientEm / AmbientEsp)
    u32 nLight;      // 0x04
    LightFog Fog;    // 0x08  Type: gx_sub: 0 = the background colour has no rgb (alpha only); Color: fog / background colour
    LightFog MirrorFog;   // 0x18  mirror fog (db_light "MIRROR FOG")
    s32 FocusZ;         // 0x28  focus depth (screen z, 0..65535)
    u8 FocusFlag;          // 0x2C
    u8 FocusLevel;          // 0x2D  focus level (0 = depth of field off)
    u8 FocusMode;          // 0x2E  focus mode (0 near, 1 far)
    u8 blur_rate;    // 0x2F  Filter00SetAlpha
    u8 tuneOn;       // 0x30  bit0: tune colours below are valid
    u8 pad_31[3];
    GXColor Tune[3]; // 0x34
    u8 tev_scale[4];  // 0x40  [0..1] -> gxCsScale; db_light shows [2] as a third on/off (PS2 tev_scale[4])
    f32 far_play_ratio;     // 0x44  far plane = fog end * (1 - farRate) + 1
    u8 Hokan;        // 0x48  fog interpolation frames
    u8 pad_49[0xEC - 0x49];
    cPenWind wind;   // 0xEC
    u8 pad_EF;       // 0xEF
    u8 blur_type;     // 0xF0  Filter00SetType
    s8 blur_power;    // 0xF1  Filter00SetPower
    u8 min_lod;       // 0xF2
    u8 max_lod;       // 0xF3
    u8 aniso;        // 0xF4
    s8 contrast[3];  // 0xF5  Filter00SetContrast
    f32 lod_bias;     // 0xF8
    GXColor AmbientEm;  // 0xFC  ambient of models without lightInfo.x50 bits 3/4 (trans_lit)
    GXColor AmbientEsp;  // 0x100  ambient of effects / lightInfo.x50 bit3 models (trans_lit)

    cLightWork* getLightWork(int no);
    u32 getSize();
};

// Light data file (.lit): cut offset table, then the cuts.
class cLit {
public:
    u16 CutNum;          // 0x00
    u8 Version;        // 0x02
    u8 nMaxLight;      // 0x03
    // 0x04: u32[nCut] byte offset of each cut from the file start (0 = none)

    cLightEnv* getCut(u16 no);
    int getSafeCutNo(int cutNo);
    int versionUp();
    u32 getMaxLight();
};

// Light list a model / effect draws with (cModel::lightInfo.pLight, EspLightList).
struct EspLightList {
    cLight* p[8];      // 0x00
    u8 num;            // 0x20
};

#line 463 "D:/Bio4/Prog/light.h"
class cLightMgr : public cManager<cLight> {
public:
    static const f32 FarDistance;  // dead-stripped from the DOL (keys the unit's static ctor name)

    cLit* pLitHeader;            // 0x34  lit the cuts are taken from (the room lit by default)
    cLightEnv LightEnv;         // 0x38 .. 0x13C  current cut environment
    u32 kindFlags[8];      // 0x13C  kind enable bits (onKind / offKind)
    u8 pad_15C[0x17C - 0x15C];
    LightPathHeader* pLitPath;  // 0x17C
    cLit* m_pLitCore;            // 0x180  core lit
    cLit* m_pLitRoom;            // 0x184  room lit
    cLit* m_pLitRoom2;            // 0x188  third lit
    int m_oldCutNo;             // 0x18C
    u8 m_Hokan;           // 0x190  fog interpolation frames left
    u8 m_logMode;              // 0x191
    u8 pad_192[2];
    f32 ElecPower;         // 0x194
    u8 pad_198[4];
    GXColor m_Tune[3];       // 0x19C
    f32 m_ColBrendRate;      // 0x1A8
    u32 dbFlag;              // 0x1AC
    cLit* dbMem;            // 0x1B0  lit built by the light tool (db_light updateLit), x1AC bit0: valid
    u8 pad_1B4[0x204 - 0x1B4];
    u32 x204;              // 0x204

    cLightMgr();
    virtual void* memAlloc(u32 size) { return MEM_ALLOC(size, 1, 13); }
    virtual void memFree(void* p) { Mem_free(p); }
    virtual void memClear(cLight* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual int construct(cLight* pSat, u32 room_no);

    void init(void (**funcTbl)(cLight*));
    int roomInit(cLit* core, cLit* room, cLit* third);
    cLight* create(cLightWork* pLw);
    cLight* createBack(cLightWork* pLw);
    cLight* create(cLit* lit, int cutNo, int lightNo, int flag);
    cLight* createBack(cLit* lit, int cutNo, int lightNo, int flag);
    cLight* create(int kind, int type, int no, int x);  // 0x8014E21C (esp11)
    cLight* createBack(int litNo, int cutNo, int lightNo, int flag);
    f32 setElecPower(f32 d);
    int setElecPower2(u8 id, u8 flag);
    int onKind(u8 kind);
    int offKind(u8 kind);
    int checkKind(u8 kind);
    cLight* getKindLight(u8 kind);
    int roomLitSet(cLit* lit);
    int roomLitCheck();
    int move();
    void hokanMove();
    cLightEnv* getEnvPtr();  // 0x8014EFCC: &this->env (at +0x38)
    void setModel2(cModel* pMod);
    void setCloth(cModel* pMod, u32 lightNum);
    void setEsp(EspLightList* pEnv, u8 enableMask);
    int update(int area_no, int camera_no);
    int setThermo();
    int registCut(cLightEnv* pLe, int hokan);
    cLightEnv* getCutAddr(int type, int cutNo);
    void setFogStart(f32 datStart);
    void setFogEnd(f32 datEnd);
    f32 getFogStart();
    f32 getFogEnd();
    void setFog();           // 0x8014FAC8
    void setBlur();
    void deleteScr();
    void offScr(u8 enable);
    int countScr();
    int setEnv(cLightEnv* pLe, int hokan);
    void setTune(cLightEnv* pLe);
    int setMipmap(cLightEnv* pLe);
    int loadLit(cLightWork* pLw, u32 nL);
    int saveLit(cLightWork* pLw);
    cLit** getLitPPtr();
    int initPath(LightPathHeader* p);
    cLightPathData* getPathPtr(u8 id);
    LightPathHeader* getPathHeader();
    void setItemLight();
    void beginEvent();
    void endEvent();
    void dbSetRoomLit(cLit* pLit);
    void inSscrn();
    void outSscrn(u32 mode);

    // In-class inlines. GCC 2.95 emits every inline member of a class whose vtable it emits, so
    // light.cpp gets bodies for these that the original linker dead-stripped (light.cpp is in
    // STRIP_UNUSED); their strings stay: "D:/Bio4/Prog/light.h" opens light.cpp's .rodata and every
    // unit including light.h carries the cManager<cLight>::create(int) strings followed by the
    // "create() failed %s id:%d" one of create(int, u32) (esp05, obj14/obj20/obj26/objYagura, ...).
    cLight* getWork(u32 no) {
        if (no >= nArray) {
            dbgAssert(__FILE__, __LINE__);
        }
        return (cLight*)((u8*)pArray + size * no);
    }
    cLight* createNew() { return cManager<cLight>::create(); }
    cLight* createNo(int id, u32 no) { return cManager<cLight>::create(id, no); }
};

extern cLightMgr LightMgr;

// Declared after the manager: the vtables come out in reverse declaration order
// (cLight06, cLight02, cLightMgr, cManager<cLight>, cLight, cUnit in light.cpp's .rodata).
class cLight02 : public cLight {
public:
    cLight02() {}
};

class cLight06 : public cLight {
public:
    cLight06() {}
};

// per-type move handlers (light01.cpp .. light10.cpp)
void Light00_Move(cLight* pLi);
void Light01_Move(cLight* pLi);
void Light02_Move(cLight* pLi);
void Light03_Move(cLight* pLi);
void Light04_Move(cLight* pLi);
void Light05_Move(cLight* pLi);
void Light06_Move(cLight* pLi);
void Light07_Move(cLight* pLi);
void Light08_Move(cLight* pLi);
void Light10_Move(cLight* pLight);

// game/light.cpp
void lightMove(cLight* pLi);
int lightHitCheck(cModel* pMod, cLight* pLight);
int lightHitCheckSphere(cModel* pMod, cLight* pLight);
int lightHitCheckCylinder(cModel* pMod, cLight* pLight);
int lightHitCheckBBox(cModel* pMod, cLight* pLight);

#endif
