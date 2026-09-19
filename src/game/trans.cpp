// game/trans.cpp: the model renderer. ModelTrans registers a model in the ordering tables,
// commonScreenMatSub skins its vertices into the primitive buffer, ModelRender / commonModelTrans
// set up the TEV stages of every material and submit the display lists.

#include "atari.h"
#include "light.h"
#include "ctrl.h"
#include "global.h"
#include "model.h"
#include "em.h"
#include "obj.h"
#include "camera.h"
#include "view.h"
#include "main_sub.h"
#include "math_sub.h"
#include "db_log.h"
#include "trans_ot.h"
#include "trans.h"
#include "shadow.h"
#include "room_tex.h"
#include "tpl.h"
#include "TexRender.h"
#include "examine.h"
#include "debug.h"
#include "cloth.h"
#include "filter.h"
#include "foot_shadow.h"
#include "db_cam.h"

#line 1 "D:/Bio4/Prog/trans.cpp"

extern cEm* pPL;   // game/em.cpp
extern cEm* pSUB;  // game/em.cpp
extern int isSelfUse;     // game/shadow.cpp
extern int g_SelfShdNum;  // game/shadow.cpp
extern f32 shd_ofs;       // game/shadow.cpp
extern f32 shd_tex_scale_x;

extern "C" {
void OSReport(const char* fmt, ...);
int EspTrans();
void EspgenTrans();
void ProcessTickGet(int no, const char* name);
void DCStoreRangeNoSync(void* addr, u32 nBytes);
void GXSetCurrentGXThread();
void GXSetDrawSync(u16 token);
void GXSetDrawSyncCallback(void (*cb)(u16));
void bio4_AddBgColor();
void Filter09Render(int);
void PSMTXReorder(Mtx src, f32 dst[4][3]);
void __GXSetIndirectMask(u32 mask);
void GXSetTevIndBumpXYZ(int tev_stage, int ind_stage, int matrix_sel);
void LightSetModel(cModel* m);
void ResetShape(cModelInfo* info, void* dst);
void CalculateShape_new(cModelInfo* info, ShapeData* data, f32 rate, void* dst);
}
void SetDrawTmpBufType(int type);   // game/TmpBuf.cpp (C++)
void drawGround(int big);           // game/db_cam.cpp (C++)
int Filter09GetbUse();              // game/filter09.cpp (C++)
void CameraCurrentProjection();     // game/camera.cpp (C++)
void* GetDrawTmpBufAddr(int type);

// The renderer's view of pG+0x184..0x4F14: the stage counters, the skinning matrix palette, the
// texture objects of the current model and the primitive buffer write pointer.
struct GxWork {
    GxStageWork stage;      // 0x000
    Mtx mtx[0xF8];          // 0x00C
    GXTexObj texObj[0xF8];  // 0x2E8C
    u8* prim;               // 0x4D8C
};
#define GXWORK() ((GxWork*) &pG->gxStage)

// cModel fields past the 0x1D8 the header declares (KNOWN DEBT: cModel is 0x320 in the original).
struct cModelExt {
    u8 pad_0[0x308];
    void* pFootShadowTbl;  // 0x308
    EmLightArea litArea;   // 0x30C
    cTexChg* pTexChg;      // 0x31C
};
#define MODEL_EXT(m) ((cModelExt*) (m))

// Parts (cParts) fields past cCoord: the parts chain and the inverse bind matrix.
struct cPartsWk {
    u8 pad_0[0xF4];
    cPartsWk* next;  // 0xF4
    Mtx bindMat;     // 0xF8
};

// Skinning weights: up to 3 matrices per vertex.
struct WeightExt {
    u16 idx[3];   // 0x00
    u16 num;      // 0x06
    u8 weight[4]; // 0x08  percent
};
struct Weight {
    u8 id[3];    // 0x00
    u8 num;       // 0x03
    u8 wht[4]; // 0x04  percent
};

#define PTR_INVALID(p) ((s32) (p) >= 0 || (u32) (p) > 0x82FFFFFF)
// Written as shifts, not `& ~0x1F`: combine folds (x >> 5) << 5 into an AND whose mask is narrowed by
// nonzero_bits (u16 * 6 -> rlwinm 0,12,26), which a literal `& ~0x1F` never gets.
#define ALIGN32(x) ((((x) + 0x1F) >> 5) << 5)
#define PTR_INVALID2(p) ((u32) (p) - 0x80000000 > 0x02FFFFFF)
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

// u8 -> f32 through GQR2 straight from memory: the compiler only emits psq_l from a stack slot.
#define PSQ_L_U8(p) ({ f32 f_; asm volatile("psq_l %0,0(%1),1,2" : "=f"(f_) : "b"(p) : "memory"); f_; })
// Loads straight into the named variable so the asm output shares the variable's (global) register.
#define PSQ_L_U8_TO(dst, p) asm volatile("psq_l %0,0(%1),1,2" : "=f"(dst) : "b"(p) : "memory")

// Bit test as 0 / 1 (matching helper).
static inline int isBit(u32 f, u32 b)
{
    if (f & b) {
        return 1;
    }
    return 0;
}

struct IntView {
    int v;
};
#define ISET0(x) (((IntView*) &(x))->v = 0)

#define IV(x) (((IntView*) &(x))->v)
static inline void U32Set(u32& d, u32 v) { d = v; }
static inline u16 U16Ref(u16& v) { return v; }
static inline int IRef(int& v) { return v; }
static inline f32 FRef(f32& v) { return v; }
static inline void ISet(int& d, int v) { d = v; }
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(ShadowMng*& d, ShadowMng* v) { d = v; }

u8 min_lod;
u8 max_lod;
f32 lod_bias;
ShadowMng* g_pShdMng;
int tev_stage;
int tev_reg;
int tev_kcolor;
int tex_map;
int tex_coord;
int ind_stage;
int g_material_tex_coord;
int g_specular_tev_stage;
void* g_prev_tpl_addr;
void* g_prev_add_tpl_addr;

GXTexObj g_Get_tex_obj;
Mtx specular_mat;
GXTexObj Specular[9];
GXTexObj GlobalIlmTex[5];
GXTexObj IndTex[2];
GXTlutObj ThermoTlut;

u32 aniso = 0;
// Declared incomplete first: the symbol is encoded as non-small-data (lis/addi at every use)
// even though the 4-byte definition below lands in .sdata.
extern u8 gxCsScale[];
u8 gxCsScale[4] = {2, 2, 2, 2};

// The next free texture coordinate slot (tex_coord), with an overflow error past 7.
static inline int getTexCoord()
{
    int tbl[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    if (tex_coord > 7) {
        pLog->err(0, 0, "TexCoord over!");
    }
    return tbl[tex_coord];
}

// The texture matrix index (GX_TEXMTX0 + 3 * tex_coord) for the current coordinate slot.
static inline u32 getTexMtx()
{
    u32 tbl[10] = {0x1E, 0x21, 0x24, 0x27, 0x2A, 0x2D, 0x30, 0x33, 0x36, 0x39};
    if (tex_coord > 9) {
        pLog->err(0, 0, "TexCoord over!");
    }
    return tbl[tex_coord];
}

// The next free texture map (tex_map), error past 7.
static inline int getTexMap()
{
    int tbl[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    if (tex_map > 7) {
        pLog->err(0, 0, "TexMap over!");
    }
    return tbl[tex_map];
}

// The next free TEV constant colour register (tev_kcolor), error past 3.
static inline int getKColor()
{
    int tbl[4] = {0, 1, 2, 3};
    if (tev_kcolor > 3) {
        pLog->err(0, 0, "KColor over!");
    }
    return tbl[tev_kcolor];
}

// The TEV colour-input selector for the current constant colour (GX_TEV_KCSEL_K0..3).
static inline int getKColorSel()
{
    int tbl[4] = {0xC, 0xD, 0xE, 0xF};
    if (tev_kcolor > 3) {
        pLog->err(0, 0, "KColor over!");
    }
    return tbl[tev_kcolor];
}

// The TEV alpha-input selector for the current constant colour (GX_TEV_KASEL_K0_A..).
static inline int getKAlphaSel()
{
    int tbl[4] = {0x1C, 0x1D, 0x1E, 0x1F};
    if (tev_kcolor > 3) {
        pLog->err(0, 0, "KColor over!");
    }
    return tbl[tev_kcolor];
}

// TEV output register for stage slot `no` (GX_TEVREG0..2), error past 2.
static inline int getTevReg(int no)
{
    int tbl[3] = {1, 2, 3};
    if (no > 2) {
        pLog->err(0, 0, "TevReg over!");
    }
    return tbl[no];
}

// Range check of a TEV register slot.
static inline int checkTevReg(int no)
{
    if (no > 2) {
        pLog->err(0, 0, "TevReg over!");
    }
    return no;
}

// Allocates the next TEV register slot after tev_reg; returns its GX register.
static inline int getTevRegNext(int& no)
{
    int tbl[3] = {1, 2, 3};
    no = checkTevReg(tev_reg + 1);
    return tbl[no];
}

// The colour-input selector reading TEV register slot `no` (GX_CC_C0..C2).
static inline int getTevRegC(int no)
{
    int tbl[3] = {2, 4, 6};
    if (no > 2) {
        pLog->err(0, 0, "TevReg over!");
    }
    return tbl[no];
}

// The alpha-input selector reading TEV register slot `no` (GX_CC_A0..A2).
static inline int getTevRegA(int no)
{
    int tbl[3] = {3, 5, 7};
    if (no > 2) {
        pLog->err(0, 0, "TevReg over!");
    }
    return tbl[no];
}

// Variants reading the global directly: the original reloads `tev_reg` after the error call
// instead of keeping a copy in a callee-saved register.
static inline int getTevReg()
{
    int tbl[3] = {1, 2, 3};
    if (tev_reg > 2) {
        pLog->err(0, 0, "TevReg over!");
    }
    return tbl[tev_reg];
}

// The colour-input selector of the current tev_reg slot.
static inline int getTevRegC()
{
    int tbl[3] = {2, 4, 6};
    if (tev_reg > 2) {
        pLog->err(0, 0, "TevReg over!");
    }
    return tbl[tev_reg];
}

class cTevStage {
public:
    int no;
    int getID()
    {
        static int tbl[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        if (no > 15) {
            pLog->err(0, 0, "TevStage over!");
        }
        return tbl[no];
    }
};

class cIndTexStage {
public:
    int no;
    int getID()
    {
        static int tbl[4] = {0, 1, 2, 3};
        if (no > 3) {
            pLog->err(0, 0, "IndStage over!");
        }
        return tbl[no];
    }
};

#define TEV_STAGE_ID() (((cTevStage*) &tev_stage)->getID())
#define IND_STAGE_ID() (((cIndTexStage*) &ind_stage)->getID())

extern "C" {
static void ThermoShaderSetup(cModel* m, cModelInfo* info, ModelPart* part);
static void shaderSetup(cModel* m, cModelInfo* info, ModelPart* part, Mtx mv);
static void TextureBlend(ModelPart* part, cModelInfo* info, int colIn, int alphaIn);
static void TextureBlend2(ModelPart* part, cModelInfo* info, int colIn, int alphaIn);
static void TextureBlend3(ModelPart* part, cModelInfo* info, int colIn, int alphaIn);
static void materialSetup(ModelPart* part, cModelInfo* info, int colIn, int alphaIn);
static void specularSetup(ModelPart* part, cModelInfo* info, int flag);
static void specularSetup2(ModelPart* part, int flag);
static void GlobalIlluminationSetup(ModelPart* part, int nrm8);
static void SetCastShadowLight(cModel* m, Vec* pos, Vec* dir, ShadowMng* mng);
static void ShadowCastSetup(ModelPart* part, cModel* m);
static void SelfShadowSetup(ModelPart* part, cModel* m, ShadowMng* mng);
static void bumpSetup(ModelPart* part, cModelInfo* info);
static void alphaSetup(cModel* m, ModelPart* part, cModelInfo* info, int thermo);
static void CalcSk1_x(void* dst, void* src, u32 n);
static void CalcSk1_x2(void* dst, void* src, u32 n);
static int MakeWeightPaletteExt(WeightExt* w, int n);
static int MakeWeightPalette(Weight* w, int n);
static void updateMatrices(Mtx m, Mtx dst, cModel* model);
static void RefractShaderSetup(cModel* m, cModelInfo* info, ModelPart* part, Mtx mv);
}

// Binds model texture `id` to texture map `map`: from the GX work's texture objects (0..0xF7), or
// a render-to-texture manager's texture (0xF8..).
void org_LoadTexObj(u32 id, int map)
{
    GxWork* gx = GXWORK();

    if (id <= 0xF7) {
        GXLoadTexObj(&gx->texObj[id], map);
    } else {
        GXLoadTexObj(&GetTexRenderMgrAddr(id - 0xF8)->m_Tex_obj, map);
    }
}

// Per frame, the "transform" pass: effects, effect generators and the ctrl manager, shadows,
// cloth, filters and render textures register their draw callbacks, then every object and enemy
// (objTrans / emTrans -> ModelTrans); the primitive buffer written is flushed to memory.
void Trans()
{
    u8* primStart = (u8*) pG->prim_base;
    void (*func)(cModel*);
    cUnit* u;

    if (!DpfFlagChk(pG, DPF_ESP)) {
        EspTrans();
    }
    if (!DpfFlagChk(pG, DPF_WATER)) {
        EspgenTrans();
    }
    if (!DpfFlagChk(pG, DPF_CTRL)) {
        CtrlMgr.trans();
    }
    ProcessTickGet(5, "EspTrans");
    ShadowTrans();
    ProcessTickGet(5, "ShadowTrans");
    ProcessTickGet(5, "MirrorTrans");
    if (!DpfFlagChk(pG, DPF_CLOTH)) {
        ClothDraw();
    }
    ProcessTickGet(5, "ClothTrans");
    if (!DpfFlagChk(pG, DPF_FILTER)) {
        FilterTrans();
    }
    if (!DpfFlagChk(pG, DPF_ESP)) {
        if (!DpfFlagChk(pG, DPF_TEX_RENDER)) {
            TransTexRenderMgr();
        }
    }
    func = objTrans;
    for (u = ObjMgr.pAlive; u != 0;) {
        cUnit* cur = u;
        u = u->pNext;
        func((cModel*) cur);
    }
    func = emTrans;
    for (u = EmMgr.pAlive; u != 0;) {
        cUnit* cur = u;
        u = u->pNext;
        func((cModel*) cur);
    }
    ProcessTickGet(5, "objTrans");
    DCStoreRangeNoSync(primStart, (u8*) pG->prim_base - primStart);
}

// Picks the lights for an enemy model that will be drawn (LightMgr.setModel2), honouring the
// player / partner / enemy hide flags of Disp_flg.
void lightSetEm(cModel* m)
{
    if ((m->be_flag & 3) != 3) {
        return;
    }
    if (m == pPL) {
        if (DpfFlagChk(pG, DPF_PL)) {
            return;
        }
    } else if (m == pSUB) {
        if (DpfFlagChk(pG, DPF_SUBCHAR)) {
            return;
        }
    } else {
        if (DpfFlagChk(pG, DPF_EM)) {
            return;
        }
    }
    LightMgr.setModel2(m);
}

// Picks the lights for an object model that will be drawn (scenery kind 2: Disp_flg 0x08000000,
// other objects 0x10000000 hide them).
void lightSetObj(cModel* m)
{
    if ((m->be_flag & 3) != 3) {
        return;
    }
    if (m->kindid == 2) {
        if (DpfFlagChk(pG, DPF_SCR)) {
            return;
        }
    } else {
        if (DpfFlagChk(pG, DPF_OBJ)) {
            return;
        }
    }
    LightMgr.setModel2(m);
}

// Transform pass of one enemy (skipped when its Disp_flg hide bit is set).
void emTrans(cModel* m)
{
    if (m == pPL) {
        if (DpfFlagChk(pG, DPF_PL)) {
            return;
        }
    } else if (m == pSUB) {
        if (DpfFlagChk(pG, DPF_SUBCHAR)) {
            return;
        }
    } else {
        if (DpfFlagChk(pG, DPF_EM)) {
            return;
        }
    }
    ModelTrans(m);
}

// Transform pass of one object (skipped when hidden by Disp_flg).
void objTrans(cModel* m)
{
    if (m->kindid == 2) {
        if (DpfFlagChk(pG, DPF_SCR)) {
            return;
        }
    } else {
        if (DpfFlagChk(pG, DPF_OBJ)) {
            return;
        }
    }
    ModelTrans(m);
}

// Light origin of the model in world space (lightInfo.ofs in the space of parts x52 - 1).
// `p` is one function-scope local shared by every expansion (a block-scope `p` per expansion is
// local-allocated and cannot take r31).
#define LIGHT_POS(m, li, pos)                                           \
    {                                                                   \
        p = (m)->getPartsPtr((li)->PartsNo - 1);                            \
        PSMTXMultVecSR(p->mat, &(li)->Offset, &(pos));                     \
        PSVECAdd(&(pos), &p->world, &(pos));                         \
    }

// Registers a visible model (be_flag 1 | 2 | 4; in events only be_flag 0x800 models) in the
// ordering tables by ot_type: 0 world depth-sorted (7: plus the 0xB translucent table, drawn a
// second time), 1 model table, 2 world, 3 / 4 / 5 fixed 0xB buckets, 6 table 0x10, 8 table 0x14;
// frustum-culled by the light-info sphere (scaled). Then skins its vertices (commonScreenMat)
// — a skinning failure removes the entry again.
void ModelTrans(cModel* m)
{
    Vec pos;
    int ret = 0xFFFF;
    int ret2 = 0xFFFF;
    int ot;
    int ot2;
    cLightInfo* li;
    f32 radius;
    cModel* p;

    if (StaFlagChk(pG, STA_SUSPEND) && !(m->be_flag & 0x800)) {
        return;
    }
    if (!(m->be_flag & 2)) {
        return;
    }
    if (!(m->be_flag & 4)) {
        return;
    }
    li = &m->LightInfo;
    if (isBit(m->be_flag, 0x1000)) {
        radius = 999999.0f;
    } else {
        radius = SQRTF(m->LightInfo.Size.x * m->LightInfo.Size.x + m->LightInfo.Size.y * m->LightInfo.Size.y + m->LightInfo.Size.z * m->LightInfo.Size.z);
        if (m->scale.x == m->scale.y && m->scale.x == m->scale.z) {
            radius *= m->scale.x;
        } else {
            if (m->scale.x > m->scale.y) {
                if (m->scale.x > m->scale.z) {
                    radius *= m->scale.x;
                } else {
                    radius *= m->scale.z;
                }
            } else {
                if (m->scale.y > m->scale.z) {
                    radius *= m->scale.y;
                } else {
                    radius *= m->scale.z;
                }
            }
        }
    }
    ot = OT_MAX;
    ot2 = OT_MAX;
    switch (m->ot_type) {
    case 7:
        m->be_flag &= ~0x08000000;
        if (m->pParts == 0) {
            ret2 = AddOtWorldPosRadius(m, (void (*)(void*)) ModelRender, &m->pos, radius, 1, 1.0f);
            ot2 = 0x11;
        } else {
            ot2 = 0x11;
            LIGHT_POS(m, li, pos);
            ret2 = AddOtWorldPosRadius(m, (void (*)(void*)) ModelRender, &pos, radius, 1, 1.0f);
        }
        if (ret2 != 0xFFFF) {
            ot = 0xB;
            LIGHT_POS(m, li, pos);
            ret = AddOtDirect(0xB, m, (void (*)()) ModelRender, 1, 2, &pos, radius);
        } else {
            ret = 0;
            ot = 0xB;
            ret |= 0xFFFF;
        }
        break;
    case 0:
        if (m->pParts == 0) {
            ret = AddOtModelPosRadius(m, (void (*)(void*)) ModelRender, &m->pos, radius, 1, 1.0f);
            ot = 0xD;
        } else {
            cModel* p = m->getPartsPtr(0);
            ot = 0xD;
            ret = AddOtModelPosRadius(m, (void (*)(void*)) ModelRender, &p->world, radius, 1, 1.0f);
        }
        break;
    case 1:
        if (m->pParts == 0) {
            ret = AddOtWorldPosRadius(m, (void (*)(void*)) ModelRender, &m->pos, radius, 1, 1.0f);
            ot = 0x11;
        } else {
            ot = 0x11;
            LIGHT_POS(m, li, pos);
            ret = AddOtWorldPosRadius(m, (void (*)(void*)) ModelRender, &pos, radius, 1, 1.0f);
        }
        break;
    case 2:
        ot = 0xB;
        LIGHT_POS(m, li, pos);
        ret = AddOtDirect(0xB, m, (void (*)()) ModelRender, 4, 2, &pos, radius);
        break;
    case 3:
        ot = 0xB;
        LIGHT_POS(m, li, pos);
        ret = AddOtDirect(0xB, m, (void (*)()) ModelRender, 2, 2, &pos, radius);
        break;
    case 4:
        ot = 0xB;
        LIGHT_POS(m, li, pos);
        ret = AddOtDirect(0xB, m, (void (*)()) ModelRender, 1, 2, &pos, radius);
        break;
    case 5:
        ot = 0x10;
        LIGHT_POS(m, li, pos);
        ret = AddOtDirect(0x10, m, (void (*)()) ModelRender, 0, 2, &pos, radius);
        break;
    case 6:
        if (m->pParts == 0) {
            ret = AddOtDirect(0x14, m, (void (*)()) ModelRender, 1, 1, NULL, 0.0f);
            ot = 0x14;
        } else {
            m->getPartsPtr(0);
            ot = 0x14;
            ret = AddOtDirect(0x14, m, (void (*)()) ModelRender, 1, 1, NULL, 0.0f);
        }
        break;
    default:
        pLog->err(0, 0, "ModelTrans() : OT_TYPE[%d] invalid.", m->ot_type);
        ret = 0;
        ret |= 0xFFFF;
        break;
    }
    if (ot == OT_MAX) {
        pLog->err(0, 0, "ModelTrans() : OT_TYPE not init.");
        ret = 0;
        ret |= 0xFFFF;
    }
    if (ret != 0xFFFF) {
        if (commonScreenMat(m) == 0) {
            DeleteOtData(ot, (u16) ret);
            if (m->ot_type == 7) {
                DeleteOtData(ot2, (u16) ret2);
            }
        }
        if (m->kindid == 0) {
            lightSetEm(m);
        } else {
            lightSetObj(m);
        }
    } else {
        if (DbgFlagChk(pG, DBG_LIGHT_TOOL)) {
            if (m->kindid == 0) {
                lightSetEm(m);
            } else {
                lightSetObj(m);
            }
        }
    }
}

// Builds this frame's vertex buffers for a model (and its shadow model info). 0 when the model is
// off / hidden or a buffer could not be had.
int commonScreenMat(cModel* m)
{
    int off = !(m->be_flag & 1);

    if (off) {
        return 0;
    }
    if (!(m->be_flag & 2)) {
        return 0;
    }
    if (!(m->be_flag & 4)) {
        return 0;
    }
    if (commonScreenMatSub(m, m->pModelInfo) == 0) {
        return 0;
    }
    if (m->pShadowModelInfo != 0) {
        if (commonScreenMatSub(m, m->pShadowModelInfo) == 0) {
            return 0;
        }
    }
    return 1;
}

#define UV_WRAP_HI(x)                       \
    {                                       \
        f32 a_ = (x);                       \
        if (a_ > 2.0f) {                    \
            f32 b_;                         \
            do {                            \
                b_ = a_ - 2.0f;             \
                a_ = b_;                    \
            } while (b_ > 2.0f);            \
            (x) = b_;                       \
        }                                   \
    }
#define UV_WRAP_LO(x)                       \
    {                                       \
        f32 a_ = (x);                       \
        if (a_ < 0.0f) {                    \
            f32 b_;                         \
            do {                            \
                b_ = a_ + 2.0f;             \
                a_ = b_;                    \
            } while (b_ < 0.0f);            \
            (x) = b_;                       \
        }                                   \
    }

// For every model info of the list: advances the texture animation / UV scroll, allocates the
// position (and normal) buffers from the prim buffer (unskinned single-part models draw from the
// original vertices), builds the weight palette from the parts matrices, applies the shape morphs
// (be_flag bit1) and skins the vertices (paired-single). 0 on an invalid pointer / full buffer.
int commonScreenMatSub(cModel* m, cModelInfo* info)
{
    calcWeightMat(m);
    for (; info != 0; info = info->pList) {
        ModelData* d = info->pData;
        ModelTexInfo* t = MODEL_TEX(info);
        void* src;
        void* nsrc;
        void* buf;
        u32 nVtx;
        u32 n;
        u32 size;
        u32 asize;

        if (info->flagsDC & 2) {
            t->frame++;
            if (t->frame >= t->anim[1]) {
                t->frame = 0;
            }
        }
        if (!SpfFlagChk(pG, SPF_ESP) && (t->flags & 1)) {
            f32 u = t->u + t->su;
            f32 v = t->v + t->sv;
            t->u = u;
            t->v = v;
            if (u > 2.0f) {
                do {
                    u -= 2.0f;
                } while (u > 2.0f);
                t->u = u;
            }
            UV_WRAP_HI(t->v);
            UV_WRAP_LO(t->u);
            UV_WRAP_LO(t->v);
        }
        if (PTR_INVALID(d)) {
            pLog->err(0, 0, "commonScreenMatSub() : pHeader ptr err.");
            return 0;
        }
        if (d->weight_palette_num <= 1 && d->weight_ext_num <= 0xFF && !(info->be_flag & 2) && d->nParts == 1) {
            continue;
        }
        if (m->be_flag & 0x4000) {
            continue;
        }
        size = d->nVtx * 6;
        asize = size + 31;
        asize = asize >> 5 << 5;
        // The original keeps `size` live past the `+ 31` at one of the three alloc sites, so
        // `size` and `asize` conflict and `size` does not inherit asize's r3 preference
        // (mulli r9 / addi r3 instead of a chained r3). The source form that did this is
        // unknown; the empty asm reproduces the liveness without emitting code.
        asm("" : : "r"(size));
        buf = GetPrimBuff(asize);
        if (PTR_INVALID2(buf)) {
            pLog->warn(0, 0, "commonScreenMatSub() : VTX prim alloc failed.");
            return 0;
        }
        info->pPosBuf[pG->vtx_buf_no] = buf;
        if (d->flags & 0x20000000) {
            size = d->nNrm * 3;
            asize = size + 31;
            asize = asize >> 5 << 5;
        } else {
            size = d->nNrm * 6;
            asize = size + 31;
            asize = asize >> 5 << 5;
        }
        buf = GetPrimBuff(asize);
        if (PTR_INVALID2(buf)) {
            pLog->warn(0, 0, "commonScreenMatSub() : Nor prim alloc failed.");
            return 0;
        }
        info->pNrmBuf[pG->vtx_buf_no] = buf;
        if (d->weight_ext_num > 0xFF) {
            MakeWeightPaletteExt((WeightExt*) d->pWeight, d->weight_ext_num);
        } else {
            MakeWeightPalette((Weight*) d->pWeight, d->weight_palette_num);
        }
        setupGQR6(((d->shift << 24) | (d->shift << 8)) | 0x00070007);
        src = d->vtxOrig;
        if (info->be_flag & 2) {
            int i;
            src = info->pPosBuf[pG->vtx_buf_no];
            for (i = 0; i < 5; i++) {
                if (i == 0) {
                    ResetShape(info, src);
                }
                if (info->shape[i].data) {
                    CalculateShape_new(info, info->shape[i].data, info->shape[i].rate, src);
                }
            }
        }
        nVtx = d->nVtx;
        if (PTR_INVALID(info->pPosBuf[pG->vtx_buf_no]) || PTR_INVALID(src)) {
            pLog->err(0, 0, "ComnScreenMatSub() PTR ERR");
            return 0;
        }
        CalcSk1_x(info->pPosBuf[pG->vtx_buf_no], src, nVtx);
        DCStoreRangeNoSync(info->pPosBuf[pG->vtx_buf_no], d->nVtx * 6);
        setupGQR6(0x32073207);
        nsrc = d->nrmOrig;
        n = d->nNrm;
        if (d->flags & 0x20000000) {
            void* dst = info->pNrmBuf[pG->vtx_buf_no];
            setupGQR6(0x20062006);
            CalcSk1_x2(dst, nsrc, n);
        } else {
            CalcSk1_x(info->pNrmBuf[pG->vtx_buf_no], nsrc, n);
        }
        DCStoreRangeNoSync(info->pNrmBuf[pG->vtx_buf_no], d->nNrm * 6);
    }
    return 1;
}

// Per parts: world matrix x bind matrix into the parts' weight matrix (the skinning palette base).
void calcWeightMat(cModel* m)
{
    Mtx inv;
    Mtx tmp;
    u32 i = 0;
    cPartsWk* p;

    PSMTXInverse(m->pParts->mat, inv);
    for (p = (cPartsWk*) m->pParts; p != 0; p = p->next) {
        PSMTXConcat(inv, ((cModel*) p)->mat, tmp);
        if (i > 0xF7) {
            pLog->err(0, 0, "commonScreenMatSub() SMAT OVERFLOW %d", i);
            break;
        }
        PSMTXConcat(tmp, p->bindMat, pG->mtxPalette[i]);
        i++;
    }
}

// Builds the blended skinning matrices for the extended weight table (u8 percentages, more than
// 255 palette entries). Returns the count.
static int MakeWeightPaletteExt(WeightExt* w0, int n)
{
    GxWork* gx = GXWORK();
    int cnt = 0;
    int i;
    u32 wa = (u32) w0;
#define w ((WeightExt*) wa)

    for (i = 0; i < n; i++, wa += sizeof(WeightExt)) {
        Mtx m;
        f32 total;
        int j;

        memclr_asm(m, sizeof(Mtx));
        total = 0.0f;
        for (j = 0; j < w->num; j++) {
            f32 rate;
            f32* s;
            PSQ_L_U8_TO(rate, &w->weight[j]);
            rate *= 0.01f;
            if (j == w->num - 1) {
                rate = 1.0f - total;
            }
            total += rate;
            s = (f32*) gx->mtx[w->idx[j]];
            m[0][0] += *s++ * rate;
            m[0][1] += *s++ * rate;
            m[0][2] += *s++ * rate;
            m[0][3] += *s++ * rate;
            m[1][0] += *s++ * rate;
            m[1][1] += *s++ * rate;
            m[1][2] += *s++ * rate;
            m[1][3] += *s++ * rate;
            m[2][0] += *s++ * rate;
            m[2][1] += *s++ * rate;
            m[2][2] += *s++ * rate;
            m[2][3] += *s++ * rate;
            cnt++;
        }
        PSMTXReorder(m, (f32(*)[3]) (0xE0000000 + i * 0x30));
    }
#undef w
    return cnt;
}

// Builds the blended skinning matrices for the weight table (sum of parts matrices x weights, the
// last weight takes the remainder). Returns the count.
static int MakeWeightPalette(Weight* w0, int n)
{
    GxWork* gx = GXWORK();
    int cnt;
    int i;
    u32 wa = (u32) w0;
#define w ((Weight*) wa)

    if (PTR_INVALID(w0)) {
        pLog->err(0, 0, "MakeWeightPalette() PTR ERR");
        return 0;
    }
    cnt = 0;
    for (i = 0; i < n; i++, wa += sizeof(Weight)) {
        Mtx m;
        f32 total;
        int j;

        memclr_asm(m, sizeof(Mtx));
        total = 0.0f;
        for (j = 0; j < w->num; j++) {
            f32 rate;
            f32* s;
            PSQ_L_U8_TO(rate, &w->wht[j]);
            rate *= 0.01f;
            if (j == w->num - 1) {
                rate = 1.0f - total;
            }
            total += rate;
            s = (f32*) gx->mtx[w->id[j]];
            m[0][0] += *s++ * rate;
            m[0][1] += *s++ * rate;
            m[0][2] += *s++ * rate;
            m[0][3] += *s++ * rate;
            m[1][0] += *s++ * rate;
            m[1][1] += *s++ * rate;
            m[1][2] += *s++ * rate;
            m[1][3] += *s++ * rate;
            m[2][0] += *s++ * rate;
            m[2][1] += *s++ * rate;
            m[2][2] += *s++ * rate;
            m[2][3] += *s++ * rate;
            cnt++;
        }
        PSMTXReorder(m, (f32(*)[3]) (0xE0000000 + i * 0x30));
    }
#undef w
    return cnt;
}

// Per frame, the draw pass: runs the ordering tables in order (render textures, shadow setup,
// far sub screen, scroll, sub screen, models, shadow draw, near sub screen, effects, world, VU1
// effects, screen, cockpit, id models, messages, after-render, debug, then 0x13..0x15), the
// filter 09 post-process and the draw-sync callback. Skipped output while System_flg 0x800.
void Render()
{
    Camera save;
    GXColor fogCol;
    GXColor c;

    g_prev_tpl_addr = (void*) -1;
    g_prev_add_tpl_addr = (void*) -1;
    GXSetCurrentGXThread();
    if (SysFlagChk(pG, SYS_SCISSOR_ON)) {
        StaFlagOn(pG, STA_SCISSOR);
        SetScissorState();
    }
    LightMgr.setFog();
    ExecOt(0);
    SetDrawTmpBufType(0);
    ExecOt(OT_TYPE_TEX_RENDER1);
    SetDrawTmpBufType(0);
    ExecOt(OT_TYPE_SHADOW_SETUP);
    SetDrawTmpBufType(0);
    ExecOt(OT_TYPE_SUBSCRN_FAR);
    SetDrawTmpBufType(0);
    ExecOt(OT_TYPE_SCROLL);
    SetDrawTmpBufType(0);
    ExecOt(OT_TYPE_SUBSCRN);
    SetDrawTmpBufType(0);
    ExecOt(OT_TYPE_MODEL);
    SetDrawTmpBufType(0);
    ExecOt(OT_TYPE_SHADOW_DRAW);
    SetDrawTmpBufType(0);
    bio4_AddBgColor();
    if (DbgFlagChk(pG, DBG_GROUND_DISP)) {
        drawGround(0);
    }
    ExecOt(OT_TYPE_SUBSCRN_NEAR);
    ClearZbuf();
    ExecOt(OT_TYPE_EFFECT);
    ExecOt(OT_TYPE_WORLD);
    ExecOt(OT_TYPE_EFFECT_VU1);
    ExecOt(OT_TYPE_SCREEN);
    ExecOt(OT_TYPE_COCKPIT);
    ExecOt(OT_TYPE_ID_MODEL);
    ExecOt(OT_TYPE_MESSAGE);
    ExecOt(OT_TYPE_AFTER_RENDER);
    ExecOt(OT_TYPE_DEBUG);
    ExecOt(OT_TYPE_MAX);
    StaFlagOff(pG, STA_SCISSOR);
    SetScissorState();
    ExecOt(0x13);
    save = pG->Cam;
    pG->Cam = itemCamera;
    ExecOt(0x14);
    pG->Cam = save;
    c.r = c.g = c.b = c.a = 0;
    GXSetFog(0, 0.0f, 0.0f, FRef(ZNEAR), FRef(ZFAR), c);
    ExecOt(0x15);
    if (Filter09GetbUse() == 1) {
        Filter09Render(0);
    }
    GXSetDrawSync(0xADEB);
    GXSetDrawSyncCallback(Render_DrawSyncCallback);
    SysFlagOff(pG, SYS_RENDER_END);
}

static const GXColor col64 = {0x40, 0x40, 0x40, 0x40};

// OT callback for a model: z mode by z_mode (0 test+write, 1 test only, 2 off), the alpha-omit
// compare, then commonModelTrans with the camera view; invisible models (invisible_factor 0) skip.
void ModelRender(cModel* m)
{
    static int modeltransalphaupdate = 1;

    if (m->invisible_factor * m->invisible_factor2 == 0.0f) {
        return;
    }
    GXSetAlphaCompare(7, 0, 1, 7, 0);
    if (m->z_mode == 0) {
        GXSetZMode(1, 3, 1);
    } else if (m->z_mode == 1) {
        GXSetZMode(1, 3, 0);
    } else if (m->z_mode == 2) {
        GXSetZMode(1, 7, 0);
    }
    CameraCurrentProjection();
    if (m->be_flag & 0x00020000) {
        GXSetNumChans(1);
        GXSetChanCtrl(0, 0, 0, 0, 0, 2, 2);
        GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
        GXSetChanAmbColor(4, col64);
        GXSetChanMatColor(4, *(GXColor*) m->pModelInfo->color);
    } else {
        LightSetModel(m);
    }
    if (modeltransalphaupdate) {
        GXSetAlphaUpdate(1);
        GXSetDstAlpha(1, 0);
    }
    commonModelTrans(m, m->pModelInfo, pG->Cam.v_mat, 0);
    if (modeltransalphaupdate) {
        GXSetAlphaUpdate(0);
        GXSetDstAlpha(0, 0);
    }
    shaderReset();
    if (DbgFlagChk(pG, DBG_BOUNDING_DISP)) {
        m->drawAllBoundingBox(m->pModelInfo);
    }
}

// Draws every model info of `m`: the shadow-cast light when a shadow light covers it, material
// colour, vertex descriptors / arrays (skinned buffers or the original vertices), cull mode,
// texture objects for the TPL (+ added textures, anisotropy / mip filters, the cTexChg swaps),
// then per part the TEV shader (shaderSetup), blend mode (blend_mode table), alpha compare and
// the display list; foot shadows afterwards. flag bit0 = the shadow / depth pass (no alpha).
void commonModelTrans(cModel* m, cModelInfo* info, Mtx viewMat, int flag)
{
    static int bl[5][4] = {
        {1, 4, 5, 0},
        {1, 4, 1, 0},
        {1, 1, 1, 0},
        {1, 2, 1, 0},
        {0, 1, 0, 0},
    };
    Mtx mv;
    GxWork* gx = GXWORK();
    int efbDone;
    int matSet;

    PSet(g_pShdMng, 0);
    if (StaFlagChk(pG, STA_USE_SHADOW_LIGHT) && (m->be_flag & 0x02000000) && !DpfFlagChk(pG, DPF_CAST_SHADOW) &&
        (StaFlagChk(pG, STA_USE_CAST_SHADOW))) {
        if (!StaFlagChk(pG, STA_PROC_SHD_TEX)) {
        g_pShdMng = GetCastShadowMngPtr(m);
        if (g_pShdMng != 0) {
            ShadowLightWork* w = (ShadowLightWork*) g_pShdMng->pLight->work;
            TEXPalette* tpl;
            if (RoomGetTplAddr(w->texId, &tpl) == 0) {
                pLog->err(0, 0, "SHADOW_CAST : TEX_ID[%x] no data", w->texId);
                g_pShdMng = 0;
            }
            u32 n = m->LightInfo.getLightNum();
            if (n > 7) {
                if (!DbgFlagChk(pG, DBG_CAST_ERR_NO_DISP)) {
                    pLog->err(0, 0, "CAST LIGHT NUM OVER %d", n);
                }
                g_pShdMng = 0;
            }
        }
        }
    }
    efbDone = 0;
    matSet = 0;
    while (info != 0) {
        ModelData* d;
        void* tex;
        f32 (*mat0)[4];
        u16 nParts;
        ModelPart* part;
        u32 i;

        if (!StaFlagChk(pG, STA_PROC_SHD_TEX) && m->ot_type == 7) {
            if (m->be_flag & 0x08000000) {
                if (!(info->be_flag & 0x40)) {
                    info = info->pList;
                    continue;
                }
            } else {
                if (info->be_flag & 0x40) {
                    info = info->pList;
                    continue;
                }
            }
        }
        if (info->be_flag & 0x20) {
            GXSetChanMatColor(4, *(GXColor*) info->color);
            matSet = 1;
        } else if (matSet == 1) {
            GXSetChanMatColor(4, *(GXColor*) m->pModelInfo->color);
        }
        if (PTR_INVALID(info)) {
            pLog->err(0, 0, "commonModelTrans() pModelInfo INVALID PTR %08X", info);
            break;
        }
        if (!(info->be_flag & 8)) {
            info = info->pList;
            continue;
        }
        if (efbDone == 0 && (m->Shader_type == 1 || m->Shader_type == 2) && !(info->be_flag & 4)) {
            efbDone = 1;
            GetEfbTex(m);
        }
        d = info->pData;
        if (PTR_INVALID(d)) {
            pLog->err(0, 0, "commonModelTrans() pHead PTR ERR %08x", d);
            return;
        }
        if (d->nTex > 0xF7) {
            pLog->err(0, 0, "commonModelTrans() TEXOBJ OVERFLOW %d", d->nTex);
            return;
        }
        Mtx inv;
        Mtx nrm;
        Mtx pm;
        if (m->be_flag & 0x4000) {
            PSMTXConcat(m->mat, (f32(*)[4]) &info->x5C, pm);
        } else if (d->weight_palette_num <= 1 && d->weight_ext_num <= 0xFF && !(info->be_flag & 2) && d->nParts == 1) {
            PSMTXConcat(m->getPartsPtr(d->pHead->partsNo)->mat, (f32(*)[4]) &info->x5C, pm);
        } else {
            PSMTXConcat(m->pParts->mat, (f32(*)[4]) &info->x5C, pm);
        }
        mat0 = m->mat;
        PSMTXConcat(viewMat, pm, mv);
        PSMTXInverse(mv, inv);
        PSMTXTranspose(inv, nrm);
        GXLoadPosMtxImm(mv, 0);
        GXLoadNrmMtxImm(nrm, 0);
        GXSetCurrentMtx(0);
        tex = d->pTex;
        GXClearVtxDesc();
        GXSetVtxDesc(9, 3);
        GXSetVtxDesc(10, 3);
        GXSetVtxDesc(13, 3);
        if (d->flags & 0x20000000) {
            GXSetVtxAttrFmt(0, 10, 0, 1, 0);
        } else {
            GXSetVtxAttrFmt(0, 10, 0, 3, 14);
        }
        if (d->flags & 0x80000000) {
            void* clr = d->pClr;
            GXSetVtxAttrFmt(0, 13, 1, 3, 8);
            GXSetVtxDesc(11, 3);
            GXSetVtxAttrFmt(0, 11, 1, 5, 0);
            GXSetArray(11, clr, 4);
        } else {
            GXSetVtxAttrFmt(0, 13, 1, 2, 15);
        }
        GXSetArray(13, tex, 4);
        GXSetVtxAttrFmt(0, 9, 1, 3, d->shift);
        if ((m->be_flag & 0x4000) || (d->weight_palette_num <= 1 && d->weight_ext_num <= 0xFF && !(info->be_flag & 2) && d->nParts == 1)) {
            GXSetArray(9, d->vtxOrig, 8);
            if (d->flags & 0x20000000) {
                GXSetArray(10, d->nrmOrig, 4);
            } else {
                GXSetArray(10, d->nrmOrig, 8);
            }
        } else {
            GXSetArray(9, info->pPosBuf[pG->vtx_buf_no], 6);
            if (d->flags & 0x20000000) {
                GXSetArray(10, info->pNrmBuf[pG->vtx_buf_no], 3);
            } else {
                GXSetArray(10, info->pNrmBuf[pG->vtx_buf_no], 6);
            }
        }
        switch (m->CullMode) {
        case 0:
            GXSetCullMode(1);
            break;
        case 1:
            GXSetCullMode(2);
            break;
        case 2:
            GXSetCullMode(0);
            break;
        }
        if (DbgFlagChk(pG, DBG_BACK_CLIP)) {
            GXSetCullMode(1);
        }
        if (g_prev_tpl_addr != info->tpl_addr || g_prev_add_tpl_addr != info->pAddTpl) {
            u32 n;
            for (i = 0; i < ((TEXPalette*) info->tpl_addr)->numDescriptors + info->nAddTex; i++) {
                TEXPalette* tpl = (TEXPalette*) info->tpl_addr;
                TEXDescriptor* td;
                u8 mip;
                int filt;
                u8 edge;
                if (tpl->numDescriptors == 0) {
                    td = TEXGet(info->pAddTpl, i);
                } else if (i < tpl->numDescriptors) {
                    td = TEXGet(tpl, i);
                } else {
                    td = TEXGet(info->pAddTpl, i - tpl->numDescriptors);
                }
                if (d->flags & 0x80000000) {
                    TEXHeader* wh = td->textureHeader;
                    wh->wrapT = 1;
                    wh->wrapS = 1;
                }
                if (td->textureHeader->minLOD == td->textureHeader->maxLOD) {
                    mip = 0;
                    filt = 1;
                } else {
                    mip = 1;
                    filt = 5;
                }
                if (aniso != 0) {
                    edge = 1;
                } else {
                    edge = td->textureHeader->edgeLODEnable;
                }
                GXInitTexObj(&gx->texObj[i], td->textureHeader->data, td->textureHeader->width, td->textureHeader->height,
                             td->textureHeader->format, td->textureHeader->wrapS, td->textureHeader->wrapT, mip);
                if (mip == 1) {
                    GXInitTexObjLOD(&gx->texObj[i], filt, 1, (f32) min_lod, (f32) max_lod, lod_bias, 0, edge, aniso);
                }
            }
            if (MODEL_EXT(m)->pTexChg != 0) {
                MODEL_EXT(m)->pTexChg->move(gx->texObj);
            }
        }
        PSet(g_prev_tpl_addr, info->tpl_addr);
        PSet(g_prev_add_tpl_addr, info->pAddTpl);
        nParts = d->displist_num;
        part = d->pParts;
        if (m->scale.x == 1.0f && m->scale.y == 1.0f && m->scale.z == 1.0f) {
            updateMatrices(mat0, specular_mat, m);
        } else {
            PSMTXScale(inv, 1.0f / m->scale.x, 1.0f / m->scale.y, 1.0f / m->scale.z);
            PSMTXConcat(inv, mat0, inv);
            updateMatrices(inv, specular_mat, m);
        }
        for (i = 0; i < nParts; i++) {
            u8* p;
            shaderSetup(m, info, part, mv);
            GXSetBlendMode(bl[info->blend_mode][0], bl[info->blend_mode][1], bl[info->blend_mode][2], bl[info->blend_mode][3]);
            if (flag & 1) {
                GXSetAlphaCompare(7, 0, 1, 7, 0);
            }
            if (!(flag & 1)) {
                f32 a = m->invisible_factor * m->invisible_factor2 * info->invisible_factor;
                if (a < 1.0f) {
                    GXColor c = *(GXColor*) info->color;
                    c.a = (u8) ((f32) (int) c.a * a);
                    GXSetChanMatColor(4, c);
                }
            }
            p = (u8*) part + 0x20;
            if ((u32) p & 0x1F) {
#line 2044 "D:/Bio4/Prog/trans.cpp"
                HALT();
            }
            GXCallDisplayList(p, part->size);
            part = (ModelPart*) (p + part->size);
            if (IND_STAGE_ID() != 0) {
                GXSetNumIndStages(0);
                GXSetTevDirect(0);
                GXSetTevDirect(1);
                GXSetTevDirect(2);
                GXSetTevDirect(3);
                GXSetTevDirect(4);
                GXSetTevDirect(5);
                GXSetTevDirect(6);
                GXSetTevDirect(7);
                GXSetTevDirect(8);
            }
        }
        info = info->pList;
    }
    if (!StaFlagChk(pG, STA_PROC_SHD_TEX)) {
        if (MODEL_EXT(m)->pFootShadowTbl != 0 && (m->be_flag & 0x10)) {
            DrawFootShadow((cEm*) m);
        }
    }
    if (!StaFlagChk(pG, STA_PROC_SHD_TEX)) {
        if (m->ot_type == 7) {
            m->be_flag |= 0x08000000;
        }
    }
}

// Applies the model's texture replacements (texture object `from` -> `to` pairs).
void cTexChg::move(GXTexObj* texObj)
{
    int i;
    u8* p = tbl;

    for (i = 0; i < num; i++) {
        texObj[p[0]] = texObj[p[1]];
        p += 2;
    }
}

// Thermal-scope view (Status_flg[1] 0x04000000, the rifle's infrared scope): the part is drawn
// with the thermo palette instead of its material.
static void ThermoShaderSetup(cModel* m, cModelInfo* info, ModelPart* part)
{
    int st;
    int map;

    ISET0(tev_stage);
    ISET0(tev_reg);
    ISET0(tev_kcolor);
    ISET0(tex_map);
    ISET0(tex_coord);
    ISET0(ind_stage);
    GXSetAlphaCompare(4, 0, 1, 4, 0xFF);
    GXSetBlendMode(1, 4, 5, 0);
    st = TEV_STAGE_ID();
    map = getTexMap();
    GXSetTevOrder(st, getTexCoord(), map, 4);
    GXSetTevColorIn(st, 0xF, 0xF, 0xF, 0xA);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, 5);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tex_map++;
    tex_coord++;
    if (part->flags & 4) {
        alphaSetup(m, part, info, 1);
    }
    GXSetNumTevStages(tev_stage);
    GXSetNumTexGens(tex_coord);
    GXSetNumIndStages(ind_stage);
}

// TEV setup of one part: thermo or refraction shaders take over entirely; else the self-shadow
// or cast-shadow stages, the material (texture, blends), specular / bump (part flags), global
// illumination (be_flag 0x01000000), the alpha texture (part flags 4), and the colour scale of
// the model's TevScaleGroup (gxCsScale).
static void shaderSetup(cModel* m, cModelInfo* info, ModelPart* part, Mtx mv)
{
    int selfDone;
    int st;
    int scale;

    if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
        ThermoShaderSetup(m, info, part);
        return;
    }
    if ((m->Shader_type == 1 || m->Shader_type == 2) && m->Refract_ratio != 0xFF && !(info->be_flag & 4)) {
        RefractShaderSetup(m, info, part, mv);
        return;
    }
    ISET0(tev_stage);
    ISET0(tev_reg);
    ISET0(tev_kcolor);
    ISET0(tex_map);
    ISET0(tex_coord);
    ISET0(ind_stage);
    selfDone = 0;
    if (StaFlagChk(pGS, STA_SELF_SHADOW) && isSelfUse) {
        u32 i;
        for (i = 0; i < g_SelfShdNum; i++) {
            if (GetSelfShadowMng(i)->pModel[0] == m) {
                if (selfDone == 1) {
                    pLog->err(0, 0, "SelfShadowLight overlap!!");
                    break;
                }
                selfDone = 1;
                SelfShadowSetup(part, m, GetSelfShadowMng(i));
                materialSetup(part, info, 0, 0);
            }
        }
    }
    if (selfDone == 0) {
        int colIn;
        int alphaIn;
        if (g_pShdMng != 0) {
            ShadowCastSetup(part, m);
            colIn = 0;
            alphaIn = 0;
        } else {
            colIn = 0xA;
            alphaIn = 5;
        }
        materialSetup(part, info, colIn, alphaIn);
    }
    if (part->flags & 0x80) {
        specularSetup2(part, 0);
    } else {
        specularSetup(part, info, 0);
        bumpSetup(part, info);
    }
    if (m->be_flag & 0x01000000) {
        GlobalIlluminationSetup(part, isBit(info->pData->flags, 0x20000000));
    }
    if (part->flags & 4) {
        alphaSetup(m, part, info, 0);
    }
    st = TEV_STAGE_ID();
    GXSetTevOrder(st, 0xFF, 0xFF, 4);
    GXSetTevColorIn(st, 0xF, 0xF, 0xF, 0);
    switch (m->TevScaleGroup) {
    case 0:
    case 1:
        switch (gxCsScale[m->TevScaleGroup]) {
        case 0:
            scale = 0;
            break;
        case 1:
            scale = 1;
            break;
        case 2:
            scale = 2;
            break;
        default:
            scale = 0;
            pLog->err(0, 0, "ShaderSetup() TEV_SCALE ERROR %d", gxCsScale[m->TevScaleGroup]);
            break;
        }
        break;
    case 4:
        scale = 0;
        break;
    case 5:
        scale = 1;
        break;
    case 6:
        scale = 2;
        break;
    default:
        scale = 0;
        pLog->err(0, 0, "ShaderSetup() TEV_SCALE ERROR %d", m->TevScaleGroup);
        break;
    }
    GXSetTevColorOp(st, 0, 0, scale, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, 0);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    GXSetNumTevStages(++tev_stage);
    GXSetNumTexGens(IRef(tex_coord));
    GXSetNumIndStages(IRef(ind_stage));
}

// Load the blend table's texture for `part` (tbl: [0] count, [4 + 2i] part texture id or 0xF7, [5 + 2i] texture id).
static inline void loadBlendTex(ModelPart* part, u8* tbl, int map)
{
    int i;
    for (i = 0; i < tbl[0]; i++) {
        u8* e = &tbl[5] + i * 2;
        if (part->texId == e[-1] || e[-1] == 0xF7) {
            org_LoadTexObj(e[0], map);
        }
    }
}

// Material blend type 0 (blendType 0): the blend-table textures (matched to the part's texture)
// layered over the base stage with the info's blendRatio as constant-colour weight.
static void TextureBlend(ModelPart* part, cModelInfo* info, int colIn, int alphaIn)
{
    int st;
    ModelTexInfo* t;
    int map;
    int coord;
    u32 mtx;
    u8* tbl;
    int i;

    st = TEV_STAGE_ID();
    map = getTexMap();
    coord = getTexCoord();
    t = MODEL_TEX(info);
    mtx = getTexMtx();
    {
        u8* btbl = (u8*) t->blendTbl;
        if (t->blendRatio == 0xFF) {
            loadBlendTex(part, btbl, map);
        }
    }
    GXSetTevOrder(st, coord, map, 4);
    GXSetTevColorIn(st, 0xF, 8, colIn, 0xF);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, alphaIn);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tex_map++;
    tex_coord++;
    if (U16Ref(t->blendRatio) == 0 || t->blendRatio == 0xFF) {
        return;
    }
    st = TEV_STAGE_ID();
    map = getTexMap();
    coord = getTexCoord();
    tbl = (u8*) t->blendTbl;
    for (i = 0; i < tbl[0]; i++) {
        int ofs = i * 2;
        u8* e = tbl + 4;
        u8 id = e[ofs];
        if (part->texId == id || id == 0xF7) {
            int reg;
            GXColor k;
            GXColor kc;
            e = tbl + 5;
            org_LoadTexObj(e[ofs], map);
            if (t->flags & 1) {
                GXSetTexCoordGen2(coord, 1, 4, mtx, 0, 0x7D);
            } else {
                GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
            }
            reg = getTevReg();
            GXSetTevOrder(st, coord, map, 4);
            GXSetTevColorIn(st, 0xF, 8, colIn, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, reg);
            GXSetTevAlphaIn(st, 7, 7, 7, alphaIn);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, reg);
            tev_stage++;
            tex_map++;
            tex_coord++;
            st = TEV_STAGE_ID();
            map = getTexMap();
            coord = getTexCoord();
            k.a = k.b = k.g = k.r = (u8) t->blendRatio;
            kc = k;
            GXSetTevKColor(getKColor(), kc);
            GXSetTevKColorSel(st, getKColorSel());
            GXSetTevKAlphaSel(st, getKAlphaSel());
            tev_kcolor++;
            GXSetTevOrder(st, 0xFF, 0xFF, 4);
            GXSetTevColorIn(st, 0, getTevRegC(), 0xE, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 0, getTevReg(), 6, 7);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
            tev_stage++;
            tev_reg++;
        }
    }
}

// Material blend type 1: the blend-table texture layered over the base with the blendRatio
// weight; the alpha comes from the second texture (probably: differs from type 0 in the alpha path).
static void TextureBlend2(ModelPart* part, cModelInfo* info, int colIn, int alphaIn)
{
    int st;
    ModelTexInfo* t;
    int map;
    int coord;
    u32 mtx;
    u8* tbl;
    int i;

    st = TEV_STAGE_ID();
    map = getTexMap();
    coord = getTexCoord();
    t = MODEL_TEX(info);
    mtx = getTexMtx();
    {
        u8* btbl = (u8*) t->blendTbl;
        if (t->blendRatio == 0xFF) {
            loadBlendTex(part, btbl, map);
        }
    }
    GXSetTevOrder(st, coord, map, 4);
    GXSetTevColorIn(st, 0xF, 8, colIn, 0xF);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 4, alphaIn, 7);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tex_map++;
    tex_coord++;
    if (U16Ref(t->blendRatio) == 0 || t->blendRatio == 0xFF) {
        return;
    }
    st = TEV_STAGE_ID();
    map = getTexMap();
    coord = getTexCoord();
    tbl = (u8*) t->blendTbl;
    for (i = 0; i < tbl[0]; i++) {
        int ofs = i * 2;
        u8* e = tbl + 4;
        u8 id = e[ofs];
        if (part->texId == id || id == 0xF7) {
            int reg;
            GXColor k;
            GXColor kc;
            e = tbl + 5;
            org_LoadTexObj(e[ofs], map);
            if (t->flags & 1) {
                GXSetTexCoordGen2(coord, 1, 4, mtx, 0, 0x7D);
            } else {
                GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
            }
            reg = getTevReg();
            GXSetTevOrder(st, coord, map, 4);
            GXSetTevColorIn(st, 0xF, 8, colIn, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, reg);
            GXSetTevAlphaIn(st, 7, 7, 7, 4);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, reg);
            tev_stage++;
            tex_map++;
            tex_coord++;
            st = TEV_STAGE_ID();
            map = getTexMap();
            coord = getTexCoord();
            k.a = k.b = k.g = k.r = (u8) t->blendRatio;
            kc = k;
            GXSetTevKColor(getKColor(), kc);
            GXSetTevKColorSel(st, getKColorSel());
            GXSetTevKAlphaSel(st, getKAlphaSel());
            tev_kcolor++;
            GXSetTevOrder(st, 0xFF, 0xFF, 4);
            GXSetTevColorIn(st, 0, getTevRegC(), 0xE, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 0);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
            tev_stage++;
            tev_reg++;
        }
    }
}

// Material blend type 2: the blend-table texture over the base (blendRatio 0 = no second
// texture) with the alpha kept from the input (`use_alp`).
static void TextureBlend3(ModelPart* part, cModelInfo* info, int colIn, int alphaIn)
{
    static int use_alp = 1;
    ModelTexInfo* t;
    int st;
    int map;
    int coord;
    u32 mtx;
    u8* tbl;
    int i;

    st = TEV_STAGE_ID();
    map = getTexMap();
    coord = getTexCoord();
    t = MODEL_TEX(info);
    mtx = getTexMtx();
    GXSetTevOrder(st, coord, map, 4);
    GXSetTevColorIn(st, 0xF, 8, colIn, 0xF);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, alphaIn);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tex_map++;
    tex_coord++;
    if (U16Ref(t->blendRatio) == 0) {
        return;
    }
    st = TEV_STAGE_ID();
    map = getTexMap();
    coord = getTexCoord();
    tbl = (u8*) t->blendTbl;
    for (i = 0; i < tbl[0]; i++) {
        int ofs = i * 2;
        u8* e = tbl + 4;
        u8 id = e[ofs];
        if (part->texId == id || id == 0xF7) {
            u8 texId;
            u8* e2 = tbl + 5;
            texId = e2[ofs];
            org_LoadTexObj(texId, map);
            if (t->flags & 1) {
                GXSetTexCoordGen2(coord, 1, 4, mtx, 0, 0x7D);
            } else {
                GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
            }
            if (use_alp) {
                int reg;
                int reg2;
                GXColor k;
                GXColor kc;
                reg = getTevReg();
                GXSetTevOrder(st, coord, map, 4);
                GXSetTevColorIn(st, 0xF, 8, colIn, 0xF);
                GXSetTevColorOp(st, 0, 0, 0, 1, reg);
                GXSetTevAlphaIn(st, 7, 7, 7, 5);
                GXSetTevAlphaOp(st, 0, 0, 0, 1, reg);
                tev_stage++;
                tex_map++;
                tex_coord++;
                st = TEV_STAGE_ID();
                map = getTexMap();
                coord = getTexCoord();
                org_LoadTexObj(texId, map);
                if (t->flags & 1) {
                    GXSetTexCoordGen2(coord, 1, 4, mtx, 0, 0x7D);
                } else {
                    GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
                }
                k.a = k.b = k.g = k.r = (u8) t->blendRatio;
                kc = k;
                GXSetTevKColor(getKColor(), kc);
                GXSetTevKColorSel(st, getKColorSel());
                GXSetTevKAlphaSel(st, getKAlphaSel());
                tev_kcolor++;
                reg = getTevRegNext(reg2);
                GXSetTevOrder(st, coord, map, 0xFF);
                if (t->blendRatio > 0xFF) {
                    GXSetTevColorIn(st, 0xF, 0xC, 9, 0xE);
                    GXSetTevColorOp(st, 0, 0, 0, 1, reg);
                    GXSetTevAlphaIn(st, 6, 6, 7, 4);
                    GXSetTevAlphaOp(st, 0, 0, 0, 1, reg);
                } else {
                    GXSetTevColorIn(st, 0xF, 0xE, 9, 0xF);
                    GXSetTevColorOp(st, 0, 0, 0, 1, reg);
                    GXSetTevAlphaIn(st, 7, 6, 4, 7);
                    GXSetTevAlphaOp(st, 0, 0, 0, 1, reg);
                }
                tev_stage++;
                tex_map++;
                tex_coord++;
                st = TEV_STAGE_ID();
                GXSetTevOrder(st, 0xFF, 0xFF, 4);
                GXSetTevColorIn(st, 0, getTevRegC(), getTevRegA(reg2), 0xF);
                GXSetTevColorOp(st, 0, 0, 0, 1, 0);
                GXSetTevAlphaIn(st, 0, alphaIn, getTevReg(reg2), 7);
                GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
                tev_stage++;
                tev_reg += 2;
            } else {
                int reg = getTevReg();
                GXSetTevOrder(st, coord, map, 4);
                GXSetTevColorIn(st, 0xF, 8, colIn, 0xF);
                GXSetTevColorOp(st, 0, 0, 0, 1, reg);
                GXSetTevAlphaIn(st, 7, 7, 7, alphaIn);
                GXSetTevAlphaOp(st, 0, 0, 0, 1, reg);
                tev_stage++;
                tex_map++;
                tex_coord++;
                st = TEV_STAGE_ID();
                map = getTexMap();
                coord = getTexCoord();
                org_LoadTexObj(texId, map);
                if (t->flags & 1) {
                    GXSetTexCoordGen2(coord, 1, 4, mtx, 0, 0x7D);
                } else {
                    GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
                }
                GXSetTevOrder(st, coord, map, 0xFF);
                GXSetTevColorIn(st, 0, getTevRegC(), 9, 0xF);
                GXSetTevColorOp(st, 0, 0, 0, 1, 0);
                GXSetTevAlphaIn(st, 0, getTevReg(), 4, 7);
                GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
                tev_stage++;
                tex_map++;
                tex_coord++;
                tev_reg++;
            }
        }
    }
}

// Base material stage: the part's texture (animated frame when the info animates), texgen by UV
// or by the UV-scroll matrix, then one of the blend types when the info blends (flags bit2),
// producing the colour from `colIn` / `alphaIn`.
static void materialSetup(ModelPart* part, cModelInfo* info, int colIn, int alphaIn)
{
    int st;
    ModelTexInfo* t;
    int map;
    int coord;
    u8 texId;

    st = TEV_STAGE_ID();
    map = getTexMap();
    coord = getTexCoord();
    ISet(g_material_tex_coord, coord);
    t = MODEL_TEX(info);
    texId = part->texId;
    if ((t->flags & 2) && t->anim != 0) {
        u8* tbl = t->anim + 4;
        texId = tbl[t->frame];
    }
    org_LoadTexObj(texId, map);
    if (t->flags & 1) {
        u32 mtx = getTexMtx();
        Mtx m;
        PSMTXIdentity(m);
        m[0][2] = t->u;
        m[1][2] = t->v;
        GXLoadTexMtxImm(m, mtx, 1);
        GXSetTexCoordGen2(coord, 1, 4, mtx, 0, 0x7D);
    } else {
        GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
    }
    if (t->flags & 4) {
        switch (t->blendType) {
        case 0:
            TextureBlend(part, info, colIn, alphaIn);
            break;
        case 1:
            TextureBlend2(part, info, colIn, alphaIn);
            break;
        case 2:
            TextureBlend3(part, info, colIn, alphaIn);
            break;
        default:
            pLog->err(0, 0, "Model BlendType invalid.[%x]", t->blendType);
            break;
        }
        return;
    }
    GXSetTevOrder(st, coord, map, 4);
    GXSetTevColorIn(st, 0xF, 8, colIn, 0xF);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, alphaIn);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tex_map++;
    tex_coord++;
}

// Specular / environment stage (part flags 0x13): the specular texture selected by the part,
// normal-based texgen, colour from the part's spec colour or the info colour2.
static void specularSetup(ModelPart* part, cModelInfo* info, int flag)
{
    GXColor col;
    GXColor kc;
    Mtx tmp;
    f32 scale;
    int st;
    int map;
    int coord;
    u32 mtx;
    u8 type;

    if ((part->flags & 0x13) == 0) {
        return;
    }
    if (info->color2[3] == 0) {
        col.r = part->specR;
        col.g = part->specG;
        col.b = part->specB;
        col.a = 0xFF;
    } else {
        col.r = info->color2[0];
        col.g = info->color2[1];
        col.b = info->color2[2];
        col.a = 0xFF;
    }
    scale = (f32) (int) part->specPow * 0.01f;
    if (scale == 0.0f) {
        scale = 0.5f;
    }
    if (isBit(info->pData->flags, 0x20000000) == 1) {
        scale *= 0.5f;
    }
    {
        Mtx s;
        Mtx t;
        PSMTXScale(s, scale, -scale, 0.0f);
        PSMTXTrans(t, 0.5f, 0.5f, 1.0f);
        PSMTXConcat(s, specular_mat, tmp);
        PSMTXConcat(t, tmp, tmp);
    }
    type = part->specType;
    st = TEV_STAGE_ID();
    map = getTexMap();
    g_specular_tev_stage = st;
    if (!(part->flags & 0x10)) {
        u32 idx = part->specTex;
        if (idx == 0xFF) {
            idx = 0;
        }
        GXLoadTexObj(&Specular[idx], map);
    } else {
        org_LoadTexObj(part->specTexOrg, map);
    }
    coord = getTexCoord();
    mtx = getTexMtx();
    GXLoadTexMtxImm(tmp, mtx, 0);
    GXSetTexCoordGen2(coord, 0, 1, mtx, 0, 0x7D);
    GXSetTevOrder(st, coord, map, 4);
    switch (type) {
    case 0: {
        int reg = getTevReg();
        GXSetTevColorIn(st, 0xF, 8, 0xA, 0xF);
        if (flag) {
            GXSetTevColorOp(st, 0, 0, 2, 1, reg);
        } else {
            GXSetTevColorOp(st, 0, 0, 0, 1, reg);
        }
        GXSetTevAlphaIn(st, 7, 7, 7, 0);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        tev_stage++;
        tex_map++;
        st = TEV_STAGE_ID();
        GXSetTevKColorSel(st, getKColorSel());
        kc = col;
        GXSetTevKColor(getKColor(), kc);
        tev_kcolor++;
        GXSetTevOrder(st, 0xFF, 0xFF, 4);
        GXSetTevColorIn(st, 0xF, 0xE, getTevRegC(), 0);
        GXSetTevColorOp(st, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 0);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        tev_stage++;
        tev_reg++;
        break;
    }
    case 1:
        GXSetTevColorIn(st, 0xF, 9, 0, 0);
        GXSetTevColorOp(st, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 0);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        tev_stage++;
        tex_map++;
        break;
    }
    tex_coord++;
}

// Bump-mapped specular variant (part flags 0x80): indirect stage with the bump texture
// perturbing the specular lookup.
static void specularSetup2(ModelPart* part, int flag)
{
    static f32 bp_mx = 0.0f;
    static f32 bp_my = 0.0f;
    static f32 mul_x = 1.0f;
    static f32 mul_y = -1.0f;
    static f32 pow = -0.02f;
    int st;

    do {
        if (!(part->flags & 3)) {
            return;
        }
    } while (0);
    st = TEV_STAGE_ID();
    {
        int map = getTexMap();
        org_LoadTexObj(part->bumpTex, map);
        int coord = getTexCoord();
        GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
        GXSetTevOrder(st, coord, map, 4);
        GXSetTevColorIn(st, 0xF, 8, 0xC, 0xF);
        GXSetTevColorOp(st, 0, 0, 0, 1, getTevReg());
        GXSetTevAlphaIn(st, 7, 7, 7, 0);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        tev_stage++;
        tex_map++;
        tex_coord++;
    }
    st = TEV_STAGE_ID();
    {
        int coord = getTexCoord();
        u32 mtx = getTexMtx();
        int map = getTexMap();
        Mtx tmp;
        org_LoadTexObj(part->bumpTex, map);
        PSMTXIdentity(tmp);
        tmp[0][2] = bp_mx;
        tmp[1][2] = bp_my;
        GXLoadTexMtxImm(tmp, mtx, 1);
        GXSetTexCoordGen2(coord, 1, 4, mtx, 0, 0x7D);
        GXSetTevOrder(st, coord, map, 4);
        tex_map++;
        tex_coord++;
        __GXSetIndirectMask(0);
        {
            f32 mx = pow * mul_x;
            f32 my = pow * mul_y;
            f32 ang = 0.0f;
            f32 indMtx[2][3];
            indMtx[0][0] = -my * sinf(ang);
            indMtx[0][1] = mx * cosf(ang);
            indMtx[0][2] = ang;
            indMtx[1][0] = my * cosf(ang);
            indMtx[1][1] = mx * sinf(ang);
            indMtx[1][2] = ang;
            GXSetIndTexMtx(2, indMtx, 1);
            int map = getTexMap();
            int coord = getTexCoord();
            GXLoadTexObj(&IndTex[0], map);
            u32 mtx = getTexMtx();
            Mtx s;
            Mtx t;
            PSMTXScale(s, 0.5f, -0.5f, 0.0f);
            PSMTXTrans(t, 0.5f, 0.5f, 1.0f);
            PSMTXConcat(s, specular_mat, tmp);
            PSMTXConcat(t, tmp, tmp);
            GXLoadTexMtxImm(tmp, mtx, 0);
            GXSetTexCoordGen2(coord, 0, 1, mtx, 0, 0x7D);
            int ind = IND_STAGE_ID();
            GXSetIndTexOrder(ind, coord, map);
            GXSetIndTexCoordScale(ind, 0, 0);
            GXSetTevIndWarp(st, ind, 1, 0, 2);
            ind_stage++;
            tex_map++;
            tex_coord++;
        }
        GXSetTevColorIn(st, 0xF, getTevRegC(), 0xC, 8);
        GXSetTevColorOp(st, 1, 1, 0, 1, getTevReg());
        GXSetTevAlphaIn(st, 7, 7, 7, 0);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        tev_stage++;
    }
    st = TEV_STAGE_ID();
    GXSetTevOrder(st, 0xFF, 0xFF, 4);
    GXSetTevColorIn(st, 0xF, 0, getTevRegC(), 0xF);
    GXSetTevColorOp(st, 0, 0, 1, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, 0);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tev_reg++;
}

// Adds the global illumination texture stage (normal-based lookup; scale 0.25 for 8-bit normals);
// off with Disp_flg 0x00080000.
static void GlobalIlluminationSetup(ModelPart* part, int nrm8)
{
    Mtx tmp;
    int st;
    int map;
    int coord;
    u32 mtx;

    if (DpfFlagChk(pG, DPF_GLB_ILM)) {
        return;
    }
    {
        Mtx s;
        Mtx t;
        f32 scale = 0.5f;
        if (nrm8 == 1) {
            scale = 0.25f;
        }
        PSMTXScale(s, scale, -scale, 0.0f);
        PSMTXTrans(t, 0.5f, 0.5f, 1.0f);
        PSMTXConcat(s, specular_mat, tmp);
        PSMTXConcat(t, tmp, tmp);
    }
    st = TEV_STAGE_ID();
    map = getTexMap();
    GXLoadTexObj(&GlobalIlmTex[0], map);
    coord = getTexCoord();
    mtx = getTexMtx();
    GXLoadTexMtxImm(tmp, mtx, 0);
    GXSetTexCoordGen2(coord, 0, 1, mtx, 0, 0x7D);
    GXSetTevOrder(st, coord, map, 4);
    GXSetTevColorIn(st, 0xF, 8, 0, 0xF);
    GXSetTevColorOp(st, 0, 0, 1, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, 0);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tex_map++;
    tex_coord++;
}

// The GX light used by the shadow-cast pass: at the shadow light's position / direction, with the
// cast colours (cast_col1..3) as material / ambient.
static void SetCastShadowLight(cModel* m, Vec* pos, Vec* dir, ShadowMng* mng)
{
    static u8 cast_col1 = 0xC0;
    static GXColor cast_col2 = {0xFF, 0xFF, 0xFF, 0xFF};
    static GXColor cast_col3 = {0, 0, 0, 0xFF};
    GXLightObj lobj;
    Vec p;
    Vec d;
    GXColor c;
    GXColor k = {0xFF, 0, 0, 0};
    u32 n;
    u32 mask;

    k.a = k.b = k.g = 0xFF;
    PSMTXMultVec(pG->Cam.v_mat, pos, &p);
    GXInitLightPos(&lobj, p.x, p.y, p.z);
    PSMTXMultVecSR(pG->Cam.v_mat, dir, &d);
    GXInitLightDir(&lobj, d.x, d.y, d.z);
    GXInitLightSpot(&lobj, 89.0f, 4);
    GXInitLightDistAttn(&lobj, 0.0f, 0.0f, 0);
    if (m != 0) {
        EmLightArea* la = &MODEL_EXT(m)->litArea;
        if (la->chk(1) == 1 && la->chk(2) == 1 && la->lightNo == mng->pLight->LitIndex) {
            k.r = k.r * (u8) la->scale;
            k.g = k.g * (u8) la->scale;
            k.b = k.b * (u8) la->scale;
        }
    }
    c = k;
    GXInitLightColor(&lobj, c);
    n = m->LightInfo.getLightNum();
    if (n > 7) {
        pLog->err(4, 0, "CastLight: light num over!!");
        return;
    }
    mask = 1 << n;
    GXLoadLightObjImm(&lobj, mask);
    GXSetNumChans(2);
    GXSetChanCtrl(1, 1, 0, 0, mask, 2, 1);
    GXSetChanCtrl(3, 0, 0, 0, 0, 2, 2);
    c = cast_col2;
    GXSetChanMatColor(5, c);
    c = cast_col3;
    GXSetChanAmbColor(5, c);
}

// Adds the stage that projects the shadow light's texture (g_pShdMng texMat x the parts matrix)
// onto the part, modulating the colour.
static void ShadowCastSetup(ModelPart* part, cModel* m)
{
    GXTexObj* tex;
    GXTlutObj* tlut;
    GXColor k;
    GXColor kc;
    ShadowMng* mng = g_pShdMng;
    ShadowLightWork* w;
    int coord;
    u32 mtx;
    int map;
    int st;

    coord = getTexCoord();
    mtx = getTexMtx();
    map = getTexMap();
    st = TEV_STAGE_ID();
    // Declared after the table-copying getters: the frame slot for tm is the merged, freed
    // getTexCoord/getTexMtx table slots at 0x8 (tm shares the base register with them).
    Mtx tm;
    PSMTXConcat(mng->texMat, m->pParts->mat, tm);
    GXLoadTexMtxImm(tm, mtx, 0);
    GXSetTexCoordGen2(coord, 0, 0, mtx, 0, 0x7D);
    w = (ShadowLightWork*) mng->pLight->work;
    if (RoomGetTexObj(w->texId, 0, &tex)) {
        GXLoadTexObj(tex, map);
        if (RoomGetTlutObj(w->texId, &tlut)) {
            GXLoadTlut(tlut, 0);
        }
    }
    if (w->mode == 3 || w->mode == 4) {
        SetCastShadowLight(m, &mng->lightPos, &mng->dir, mng);
    }
    k.r = mng->pLight->Col.r;
    k.g = mng->pLight->Col.g;
    k.b = mng->pLight->Col.b;
    k.a = mng->pLight->Col.a;
    kc = k;
    GXSetTevKColor(getKColor(), kc);
    switch (w->mode) {
    case 1:
        GXSetTevKColorSel(st, getKColorSel());
        GXSetTevKAlphaSel(st, getKAlphaSel());
        tev_kcolor++;
        // Each arm carries the stage's tail through `tev_stage++` so the arm does not end in a
        // call (flow's post-call nop would stop jump2 cross-jumping the shared `li r7; bl`).
        if (U16Ref(w->flags) & 4) {
            GXSetTevOrder(st, coord, map, 0xFF);
            GXSetTevColorIn(st, 0xE, 0xF, 8, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 7);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
            tev_stage++;
        } else {
            GXSetTevOrder(st, coord, map, 0xFF);
            GXSetTevColorIn(st, 0xF, 8, 0xE, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 7);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
            tev_stage++;
        }
        st = TEV_STAGE_ID();
        GXSetTevOrder(st, 0xFF, 0xFF, 4);
        GXSetTevColorIn(st, 0xA, 0xF, 0, 0xF);
        GXSetTevColorOp(st, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 5);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        break;
    case 2:
        GXSetTevKColorSel(st, getKColorSel());
        GXSetTevKAlphaSel(st, getKAlphaSel());
        tev_kcolor++;
        if (U16Ref(w->flags) & 4) {
            GXSetTevOrder(st, coord, map, 4);
            GXSetTevColorIn(st, 0xE, 0xF, 8, 0xA);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 5);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        } else {
            GXSetTevOrder(st, coord, map, 4);
            GXSetTevColorIn(st, 0xF, 8, 0xE, 0xA);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 5);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        }
        break;
    case 3:
        GXSetTevOrder(st, coord, map, 5);
        GXSetTevColorIn(st, 0xF, 8, 0xA, 0xF);
        GXSetTevColorOp(st, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 5);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        tev_stage++;
        st = TEV_STAGE_ID();
        GXSetTevKColorSel(st, getKColorSel());
        GXSetTevKAlphaSel(st, getKAlphaSel());
        tev_kcolor++;
        if (U16Ref(w->flags) & 4) {
            GXSetTevOrder(st, 0xFF, 0xFF, 4);
            GXSetTevColorIn(st, 0xE, 0xF, 0, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 7);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
            tev_stage++;
        } else {
            GXSetTevOrder(st, 0xFF, 0xFF, 4);
            GXSetTevColorIn(st, 0xF, 0, 0xE, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 7);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
            tev_stage++;
        }
        st = TEV_STAGE_ID();
        GXSetTevOrder(st, 0xFF, 0xFF, 4);
        GXSetTevColorIn(st, 0xA, 0xF, 0, 0xF);
        GXSetTevColorOp(st, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 5);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        break;
    case 4:
        if (w->flags & 4) {
            GXSetTevOrder(st, coord, map, 5);
            GXSetTevColorIn(st, 0xA, 0xF, 8, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 5);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
            tev_stage++;
            st = TEV_STAGE_ID();
            GXSetTevKColorSel(st, getKColorSel());
            GXSetTevKAlphaSel(st, getKAlphaSel());
            tev_kcolor++;
            GXSetTevOrder(st, 0xFF, 0xFF, 4);
            GXSetTevColorIn(st, 0xF, 0, 0xE, 0xA);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 5);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        } else {
            GXSetTevOrder(st, coord, map, 5);
            GXSetTevColorIn(st, 0xF, 8, 0xA, 0xF);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 5);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
            tev_stage++;
            st = TEV_STAGE_ID();
            GXSetTevKColorSel(st, getKColorSel());
            GXSetTevKAlphaSel(st, getKAlphaSel());
            tev_kcolor++;
            GXSetTevOrder(st, 0xFF, 0xFF, 4);
            GXSetTevColorIn(st, 0xF, 0, 0xE, 0xA);
            GXSetTevColorOp(st, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(st, 7, 7, 7, 5);
            GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        }
        break;
    }
    tev_stage++;
    tex_coord++;
    tex_map++;
}

// Self-shadow stage: the model's own depth shadow map compared through an indirect texture (the
// IndTex ramp) so parts in their own shadow darken.
static void SelfShadowSetup(ModelPart* part, cModel* m, ShadowMng* mng)
{
    static int shd_tex_no = 0;
    static f32 shd_z = -1.001f;
    int coord;
    u32 mtx;
    int map;
    int st;
    ShadowLightWork* w;
    u32 i;

    coord = getTexCoord();
    mtx = getTexMtx();
    map = getTexMap();
    st = TEV_STAGE_ID();
    GXLoadTexObj(&IndTex[shd_tex_no], map);
    // Declared after the table-copying getters: tm and up take the merged, freed table slots at
    // 0x8/0x38, sm and trans follow at 0x50/0x80, the second round of tables lands at 0xb0/0xd0.
    Mtx tm;
    Vec up = {0.0f, 1.0f, 0.0f};
    Mtx sm;
    Mtx trans;
    sm[0][0] = 0.0f;
    sm[0][1] = 0.0f;
    sm[0][2] = 0.0f;
    sm[0][3] = 0.0f;
    sm[1][0] = 0.0f;
    sm[1][1] = 0.0f;
    sm[1][3] = 0.0f;
    sm[2][0] = 0.0f;
    sm[2][1] = 0.0f;
    sm[2][2] = 0.0f;
    sm[2][3] = 0.0f;
    sm[1][2] = shd_tex_scale_x;
    PSMTXIdentity(trans);
    trans[2][3] = shd_ofs + PSVECDistance(&mng->lightPos, &mng->target);
    C_MTXLookAt(tm, &mng->lightPos, &up, &mng->target);
    PSMTXConcat(tm, mng->pModel[0]->pParts->mat, tm);
    PSMTXConcat(trans, tm, tm);
    PSMTXConcat(sm, tm, tm);
    GXLoadTexMtxImm(tm, mtx, 1);
    GXSetTexCoordGen2(coord, 1, 0, mtx, 0, 0x7D);
    GXSetTevOrder(st, coord, map, 4);
    GXSetTevColorIn(st, 0xF, 0xC, 9, 0xF);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, 5);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tex_coord++;
    tex_map++;
    st = TEV_STAGE_ID();
    coord = getTexCoord();
    mtx = getTexMtx();
    map = getTexMap();
    PSMTXConcat(mng->texMat, m->pParts->mat, tm);
    GXLoadTexMtxImm(tm, mtx, 0);
    GXSetTexCoordGen2(coord, 0, 0, mtx, 0, 0x7D);
    GXLoadTexObj(&mng->texObj, map);
    GXSetTevOrder(st, coord, map, 0xFF);
    GXSetTevColorIn(st, 0xC, 0xF, 9, 0);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, 0);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    // Reference-setter stores are neither MEM_SCALAR_P nor MEM_IN_STRUCT_P, so the in-struct load
    // of mng->pLight below cannot be scheduled above them (plain `x++` stores are scalar and let it).
    ISet(tev_stage, tev_stage + 1);
    ISet(tex_coord, tex_coord + 1);
    ISet(tex_map, tex_map + 1);
    w = (ShadowLightWork*) mng->pLight->work;
    for (i = 0; i < w->selfShadow; i++) {
        st = TEV_STAGE_ID();
        GXSetTevOrder(st, 0xFF, 0xFF, 0xFF);
        GXSetTevColorIn(st, 0xF, 0, 0, 0xF);
        GXSetTevColorOp(st, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 0);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        tev_stage++;
    }
    st = TEV_STAGE_ID();
    GXSetTevOrder(st, 0xFF, 0xFF, 4);
    GXSetTevColorIn(st, 0xF, 0xA, 0, 0xF);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 7, 7, 0);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
}

// Bump stage (part flags bit0): the part's bump texture (or the blend table's) as an indirect
// texture perturbing the base lookup.
static void bumpSetup(ModelPart* part, cModelInfo* info)
{
    int off = !(part->flags & 1);
    ModelTexInfo* t;
    int map;
    int coord;
    int ind;
    u8 texId;

    if (off) {
        return;
    }
    t = MODEL_TEX(info);
    __GXSetIndirectMask(0);
    texId = part->bumpTex;
    map = getTexMap();
    if (info->flagsDC & 4) {
        u8* tbl = t->blendTbl;
        int i;
        for (i = 0; i < tbl[0]; i++) {
            u8* e = &tbl[5] + i * 2;
            if (part->bumpTex == e[-1] || e[-1] == 0xF7) {
                texId = e[0];
            }
        }
    }
    org_LoadTexObj(texId, map);
    coord = getTexCoord();
    GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
    ind = IND_STAGE_ID();
    GXSetIndTexOrder(ind, coord, map);
    GXSetIndTexCoordScale(ind, 0, 0);
    GXSetTevIndBumpXYZ(g_specular_tev_stage, ind, 1);
    tex_map++;
    tex_coord++;
    ind_stage++;
}

// Alpha texture stage (part flags 4): alpha compare against the part's alphaRef (or the model's
// alpha_omit), the alpha texture multiplied into the output alpha.
static void alphaSetup(cModel* m, ModelPart* part, cModelInfo* info, int thermo)
{
    int map;
    int st;
    int coord;
    u8 ref;

    GXSetZCompLoc(0);
    ref = m->alpha_omit;
    if (ref == 0xFF) {
        GXSetAlphaCompare(4, part->alphaRef, 1, 4, 0xFF);
    } else {
        GXSetAlphaCompare(4, ref, 1, 4, 0xFF);
    }
    map = getTexMap();
    org_LoadTexObj(part->alphaTex, map);
    st = TEV_STAGE_ID();
    if ((info->flagsDC & 8) || thermo) {
        coord = getTexCoord();
        GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
        tex_coord++;
    } else {
        coord = g_material_tex_coord;
    }
    GXSetTevOrder(st, coord, map, 4);
    GXSetTevColorIn(st, 0xF, 0xF, 0xF, 0);
    GXSetTevColorOp(st, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(st, 7, 4, 0, 7);
    GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    tev_stage++;
    tex_map++;
}

// Frame start: flips the double-buffered primitive / vertex buffer (vtx_buf_no) and resets the
// allocation pointer.
void SetPrimBuffPtr()
{
    GxWork* gx = GXWORK();

    if (pG->prim_cnt == 0) {
        return;
    }
    U32Set(pG->vtx_buf_no, pG->vtx_buf_no ^ 1);
    gx->prim = (u8*) pG->prim_cnt + pG->nPrim * pG->vtx_buf_no;
}

// Allocates `size` bytes (32-byte aligned) from this frame's primitive buffer; 0 when exhausted.
void* GetPrimBuff(int size)
{
    GxWork* gx = GXWORK();
    u8* base = (u8*) pG->prim_cnt;
    u8* limit;
    u8* p;
    u8* next;

    if (PTR_INVALID(base)) {
        pLog->err(0, 0, "GetPrimBuff() PTR ERR %08X", base);
        return 0;
    }
    limit = base + pG->nPrim * (pG->vtx_buf_no + 1);
    size = (size + 0x1F) / 32 * 32;
    p = gx->prim;
    next = p + size;
    if (limit < next) {
        pLog->err(0, 0, "GetPrimBuff() : OVERFLOW");
        return 0;
    }
    gx->prim = next;
    return p;
}

// Resets the TEV / indirect / texgen state and the allocation counters (tev_stage, tev_reg,
// tex_coord, tex_map, tev_kcolor) between models.
void shaderReset()
{
    if (IND_STAGE_ID() != 0) {
        GXSetNumIndStages(0);
        GXSetTevDirect(0);
        GXSetTevDirect(1);
        GXSetTevDirect(2);
        GXSetTevDirect(3);
        GXSetTevDirect(4);
        GXSetTevDirect(5);
        GXSetTevDirect(6);
        GXSetTevDirect(7);
        GXSetTevDirect(8);
        GXSetTevDirect(9);
        GXSetTevDirect(10);
        GXSetTevDirect(11);
        GXSetTevDirect(12);
        GXSetTevDirect(13);
        GXSetTevDirect(14);
        GXSetTevDirect(15);
    }
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    GXSetChanCtrl(1, 0, 0, 0, 0, 0, 2);
    GXSetAlphaCompare(7, 0, 1, 7, 0);
}

// Dead-stripped in the DOL (STRIP_UNUSED): only its constant pool (0.0, 448.0, 1.0 and the signed
// int -> float double) remains in .rodata between GetPrimBuff and updateMatrices.
static void primBuffDebugDisp(int n)
{
    f32 zero = 0.0f;
    f32 h = 448.0f;
    f32 one = 1.0f;
    GXSetViewport(zero, zero, h, one, (f32) n, one);
}

// Skin `n` vertices (s16 x/y/z + s16 matrix index, 8 bytes) from src into dst (s16 x/y/z, 6 bytes)
// with the matrix palette in locked cache (0xE0000000, ROMtx 0x30 each). GQR6 holds the fixed point scale.
static void CalcSk1_x(void* dst, void* src, u32 n)
{
    asm volatile(
        "lha 9, 6(4)\n"
        "subi 3, 3, 6\n"
        "li 0, 0\n"
        "subi 4, 4, 8\n"
        "mulli 9, 9, 0x30\n"
        "addis 9, 9, 0xE000\n"
        "mtctr 5\n"
        "1:\n"
        "psq_l 0, 0(9), 0, 0\n"
        "psq_l 1, 8(9), 1, 0\n"
        "psq_l 2, 0xc(9), 0, 0\n"
        "psq_l 3, 0x14(9), 1, 0\n"
        "psq_l 4, 0x18(9), 0, 0\n"
        "psq_l 5, 0x20(9), 1, 0\n"
        "psq_l 6, 0x24(9), 0, 0\n"
        "psq_l 7, 0x2c(9), 1, 0\n"
        "psq_lu 8, 8(4), 0, 6\n"
        "psq_l 9, 4(4), 1, 6\n"
        "ps_madds0 12, 0, 8, 6\n"
        "ps_madds0 13, 1, 8, 7\n"
        "ps_madds1 12, 2, 8, 12\n"
        "ps_madds1 13, 3, 8, 13\n"
        "ps_madds0 12, 4, 9, 12\n"
        "ps_madds0 13, 5, 9, 13\n"
        "psq_stu 12, 6(3), 0, 6\n"
        "psq_st 13, 4(3), 1, 6\n"
        "lha 9, 0xe(4)\n"
        "mulli 9, 9, 0x30\n"
        "addis 9, 9, 0xE000\n"
        "bdnz 1b\n");
}

// Same for s8 normals (s8 x/y/z + u8 matrix index, 4 bytes) into s8 x/y/z (3 bytes).
static void CalcSk1_x2(void* dst, void* src, u32 n)
{
    asm volatile(
        "lbz 9, 3(4)\n"
        "subi 3, 3, 3\n"
        "li 0, 0\n"
        "subi 4, 4, 4\n"
        "mulli 9, 9, 0x30\n"
        "addis 9, 9, 0xE000\n"
        "mtctr 5\n"
        "1:\n"
        "psq_l 0, 0(9), 0, 0\n"
        "psq_l 1, 8(9), 1, 0\n"
        "psq_l 2, 0xc(9), 0, 0\n"
        "psq_l 3, 0x14(9), 1, 0\n"
        "psq_l 4, 0x18(9), 0, 0\n"
        "psq_l 5, 0x20(9), 1, 0\n"
        "psq_l 6, 0x24(9), 0, 0\n"
        "psq_l 7, 0x2c(9), 1, 0\n"
        "psq_lu 8, 4(4), 0, 6\n"
        "psq_l 9, 2(4), 1, 6\n"
        "ps_madds0 12, 0, 8, 6\n"
        "ps_madds0 13, 1, 8, 7\n"
        "ps_madds1 12, 2, 8, 12\n"
        "ps_madds1 13, 3, 8, 13\n"
        "ps_madds0 12, 4, 9, 12\n"
        "ps_madds0 13, 5, 9, 13\n"
        "psq_stu 12, 3(3), 0, 6\n"
        "psq_st 13, 2(3), 1, 6\n"
        "lbz 9, 7(4)\n"
        "mulli 9, 9, 0x30\n"
        "addis 9, 9, 0xE000\n"
        "bdnz 1b\n");
}

// Sets GQR6 (the paired-single quantisation register the skinning loads use).
void setupGQR6(u32 v)
{
    asm volatile("mtspr 918, %0" : : "r"(v));
}

// Relocate a TPL whose texture headers also carry a CLUT (thermo palette).
void CalcTplAddrC8(TEXPalette* tpl)
{
    u32 i;

    if (tpl == 0) {
        return;
    }
    if ((s32) tpl->descriptorArray < 0) {
        return;
    }
    tpl->descriptorArray = (TEXDescriptor*) ((u32) tpl->descriptorArray + (u32) tpl);
    for (i = 0; i < tpl->numDescriptors; i++) {
        TEXDescriptor* td = &tpl->descriptorArray[i];
        td->textureHeader = (TEXHeader*) ((u32) tpl + (u32) td->textureHeader);
        td->textureHeader->data = (void*) ((u32) tpl + (u32) td->textureHeader->data);
        if (td->CLUTHeader != 0) {
            td->CLUTHeader = (CLUTHeader*) ((u32) tpl + (u32) td->CLUTHeader);
            td->CLUTHeader->data = (void*) ((u32) tpl + (u32) td->CLUTHeader->data);
        }
    }
}

// Boot (CoreDataRead): the specular environment textures, the two indirect ramp textures and the
// thermal palette from the core archive.
void SpecularInit(TEXPalette* spec, TEXPalette* ind, TEXPalette* ind2, TEXPalette* thermo)
{
    u32 i;
    u32 ns;
    u32 n;
    u32 n2;
    TEXDescriptor* td;
    TEXHeader* h;
    CLUTHeader* cl;

    calcTplAddr(spec);
    ns = spec->numDescriptors;
    for (i = 0; i < ns; i++) {
        td = TEXGet(spec, i);
        h = td->textureHeader;
        GXInitTexObj(&Specular[i], h->data, h->width, h->height, h->format, h->wrapS, h->wrapT, 0);
    }
    calcTplAddr(ind);
    n = ind->numDescriptors;
    for (i = 0; i < n; i++) {
        td = TEXGet(ind, i);
        h = td->textureHeader;
        GXInitTexObj(&IndTex[i], h->data, h->width, h->height, h->format, 0, 0, 0);
    }
    calcTplAddr(ind2);
    n2 = ind2->numDescriptors;
    for (i = 0; i < n2; i++) {
        td = TEXGet(ind2, i);
        h = td->textureHeader;
        GXInitTexObj(&IndTex[n + i], h->data, h->width, h->height, h->format, 0, 0, 0);
    }
    CalcTplAddrC8(thermo);
    td = TEXGet(thermo, 0);
    cl = td->CLUTHeader;
    GXInitTlutObj(&ThermoTlut, cl->data, cl->format, cl->numEntries);
}

// Boot: the global illumination textures from the core archive.
void GlobalIlmTexInit(TEXPalette* tpl)
{
    u32 i;
    u32 n;

    calcTplAddr(tpl);
    n = tpl->numDescriptors;
    for (i = 0; i < n; i++) {
        TEXDescriptor* td = TEXGet(tpl, i);
        TEXHeader* h = td->textureHeader;
        GXInitTexObj(&GlobalIlmTex[i], h->data, h->width, h->height, h->format, h->wrapS, h->wrapT, 0);
    }
}

// Refraction: model-view matrix and its inverse transpose loaded as the position / normal
// matrices, `dst` = the texture matrix mapping normals to the screen.
static void updateMatrices(Mtx m, Mtx dst, cModel* model)
{
    Mtx mv;
    Mtx t;
    Mtx r;
    Mtx inv;
    f32 ind[2][3];
    Mtx inv2;

    PSMTXConcat(pG->Cam.v_mat, m, mv);
    PSMTXInverse(mv, inv);
    PSMTXTranspose(inv, t);
    PSMTXInverse(mv, inv2);
    PSMTXTranspose(inv2, dst);
    PSMTXScale(inv, 0.4f, 0.4f, 0.4f);
    PSMTXConcat(inv, t, r);
    ind[0][0] = r[0][0];
    ind[0][1] = r[0][1];
    ind[0][2] = r[0][2];
    ind[1][0] = r[1][0];
    ind[1][1] = -r[1][1];
    ind[1][2] = r[1][2];
    GXSetIndTexMtx(1, ind, 0);
}

// Refraction shader (Shader_type 1 / 2, Refract_ratio): the captured screen (render texture)
// looked up through an indirect texture built from the normals, mixed by the refract ratio.
static void RefractShaderSetup(cModel* m, cModelInfo* info, ModelPart* part, Mtx mv)
{
    static f32 mul_x = 1.0f;
    static f32 mul_y = 1.0f;
    GXColor k;
    int st;
    int map;
    int coord;
    u32 mtx;
    u8 kv;
    s8 kvs;
    int scale;

    ISET0(tev_stage);
    ISET0(tev_reg);
    ISET0(tev_kcolor);
    ISET0(tex_map);
    ISET0(tex_coord);
    ISET0(ind_stage);
    st = TEV_STAGE_ID();
    map = getTexMap();
    coord = getTexCoord();
    GXLoadTexObj(&g_Get_tex_obj, map);
    {
        Mtx m2;
        Mtx proj;
        mtx = getTexMtx();
        C_MTXLightPerspective(proj, pG->Cam.param.fovy, 1.33333333f, 0.5f, -0.66666667f, 0.5f, 0.5f);
        PSMTXConcat(proj, mv, m2);
        GXLoadTexMtxImm(m2, mtx, 0);
    }
    GXSetTexCoordGen2(coord, 0, 0, mtx, 0, 0x7D);
    GXSetTevOrder(st, coord, map, 4);
    kv = 0xFF;
    switch (gxCsScale[m->TevScaleGroup]) {
    case 0:
        break;
    case 1:
        kv = 0x80;
        break;
    case 2:
        kv = 0x40;
        break;
    }
    kvs = kv;
    k.a = kv;
    k.r = k.g = k.b = kvs;
    GXSetTevKColor(getKColor(), k);
    GXSetTevKColorSel(st, getKColorSel());
    GXSetTevKAlphaSel(st, getKAlphaSel());
    tev_kcolor++;
    if (m->Refract_ratio == 0) {
        GXSetTevColorIn(st, 0xF, 8, 0xE, 0xF);
        GXSetTevColorOp(st, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 5);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
    } else {
        int reg = getTevReg();
        GXSetTevColorIn(st, 0xF, 8, 0xE, 0xF);
        GXSetTevColorOp(st, 0, 0, 0, 1, reg);
        GXSetTevAlphaIn(st, 7, 7, 7, 5);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, reg);
        tev_reg++;
    }
    tex_map++;
    tex_coord++;
    if (m->Shader_type == 1) {
        f32 indMtx[2][3];
        f32 s;
        int map;
        int coord;
        __GXSetIndirectMask(0);
        s = (f32) m->Refract_pow * 0.001953125f;
        indMtx[0][0] = 0.0f;
        indMtx[0][1] = s * mul_x;
        indMtx[0][2] = 0.0f;
        indMtx[1][0] = s * mul_y;
        indMtx[1][1] = 0.0f;
        indMtx[1][2] = 0.0f;
        GXSetIndTexMtx(2, indMtx, 1);
        map = getTexMap();
        coord = getTexCoord();
        GXLoadTexObj(&IndTex[0], map);
        GXSetTexCoordGen2(coord, 1, 1, 0x3C, 0, 0x7D);
        {
            int ind = IND_STAGE_ID();
            GXSetIndTexOrder(ind, coord, map);
            GXSetIndTexCoordScale(ind, 0, 0);
            GXSetTevIndWarp(st, ind, 1, 0, 2);
        }
        tex_map++;
        tex_coord++;
        tev_stage++;
        ind_stage++;
    } else {
        f32 indMtx[2][3];
        f32 s;
        int map;
        int coord;
        ModelTexInfo* t = MODEL_TEX(info);
        __GXSetIndirectMask(0);
        s = (f32) m->Refract_pow * 0.001953125f;
        indMtx[0][0] = 0.0f;
        indMtx[1][0] = s;
        indMtx[0][2] = -s;
        indMtx[0][1] = s;
        indMtx[1][1] = 0.0f;
        indMtx[1][2] = s * 1.35f;
        GXSetIndTexMtx(2, indMtx, 1);
        map = getTexMap();
        coord = getTexCoord();
        if (info->flagsDC & 4) {
            loadBlendTex(part, t->blendTbl, map);
        } else {
            org_LoadTexObj(part->texId, map);
        }
        GXSetTexCoordGen2(coord, 1, 4, 0x3C, 0, 0x7D);
        {
            int ind = IND_STAGE_ID();
            GXSetIndTexOrder(ind, coord, map);
            GXSetIndTexCoordScale(ind, 0, 0);
            GXSetTevIndWarp(st, ind, 1, 0, 2);
        }
        tex_map++;
        tex_coord++;
        tev_stage++;
        ind_stage++;
    }
    if (m->Refract_ratio == 0) {
        specularSetup(part, info, 0);
        bumpSetup(part, info);
    } else {
        if (g_pShdMng != 0) {
            ShadowCastSetup(part, m);
            materialSetup(part, info, 0, 0);
        } else {
            materialSetup(part, info, 0xA, 5);
        }
        if (m->be_flag & 0x01000000) {
            GlobalIlluminationSetup(part, isBit(info->pData->flags, 0x20000000));
        }
        GXColor k;
        int st;
        st = TEV_STAGE_ID();
        k.a = k.b = k.g = k.r = m->Refract_ratio;
        GXSetTevKColor(getKColor(), k);
        GXSetTevKColorSel(st, getKColorSel());
        GXSetTevKAlphaSel(st, getKAlphaSel());
        tev_kcolor++;
        GXSetTevOrder(st, 0xFF, 0xFF, 4);
        GXSetTevColorIn(st, 2, 0, 0xE, 0xF);
        GXSetTevColorOp(st, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 0);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        tev_stage++;
        specularSetup(part, info, 0);
        bumpSetup(part, info);
        if (part->flags & 4) {
            alphaSetup(m, part, info, 0);
        }
    }
    {
        int st;
        st = TEV_STAGE_ID();
        GXSetTevOrder(st, 0xFF, 0xFF, 4);
        GXSetTevColorIn(st, 0xF, 0xF, 0xF, 0);
        switch (m->TevScaleGroup) {
        case 0:
        case 1:
            switch (gxCsScale[m->TevScaleGroup]) {
            case 0:
                scale = 0;
                break;
            case 1:
                scale = 1;
                break;
            case 2:
                scale = 2;
                break;
            default:
                scale = 0;
                pLog->err(0, 0, "ShaderSetup() TEV_SCALE ERROR %d", gxCsScale[m->TevScaleGroup]);
                break;
            }
            break;
        case 4:
            scale = 0;
            break;
        case 5:
            scale = 1;
            break;
        case 6:
            scale = 2;
            break;
        default:
            scale = 0;
            pLog->err(0, 0, "ShaderSetup() TEV_SCALE ERROR %d", m->TevScaleGroup);
            break;
        }
        GXSetTevColorOp(st, 0, 0, scale, 1, 0);
        GXSetTevAlphaIn(st, 7, 7, 7, 0);
        GXSetTevAlphaOp(st, 0, 0, 0, 1, 0);
        GXSetNumTevStages(++tev_stage);
        GXSetNumTexGens(IRef(tex_coord));
        GXSetNumIndStages(IRef(ind_stage));
    }
}

// Copy the frame buffer below the top 56 lines at half size into g_Get_tex_obj (refraction source).
void GetEfbTex(cModel* m)
{
    void* buf = GetDrawTmpBufAddr(5);
    f32 ofs;

    if (buf == 0) {
        pLog->warn(0, 0, "RefractShaderSetup() : not enough memory");
        return;
    }
    ofs = 56.0f;
    GXSetTexCopySrc(0, (u32) ofs, (u32) Screen.width, (u32) (Screen.height - ofs));
    GXSetTexCopyDst((u32) Screen.width >> 1, (u16) ((f32) ((u32) Screen.height >> 1) - ofs), 6, 1);
    GXCopyTex(buf, 0);
    GXPixModeSync();
    GXInvalidateTexAll();
    GXInitTexObj(&g_Get_tex_obj, buf, (u32) Screen.width >> 1, (u16) ((f32) ((u32) Screen.height >> 1) - ofs), 6, 0, 0, 0);
}

// Clear the Z buffer to TEST_Z with a full-screen quad.
void ClearZbuf()
{
    static f32 TEST_Z = -0.99999f;
    static f32 TEST_Z2 = 0.0f;
    Mtx44 proj;
    Mtx pos;
    GXColor mat;

    GXSetColorUpdate(0);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 1);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, 1.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(pos);
    GXLoadPosMtxImm(pos, 0);
    GXSetCurrentMtx(0);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetNumTexGens(0);
    GXSetChanCtrl(0, 0, 0, 0, 0, 2, 2);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
    GXSetTevOrder(0, 0xFF, 0xFF, 0xFF);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 7);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    {
        GXColor c = { 8, 8, 0x80, 0x1C };
        mat = c;
    }
    GXSetChanMatColor(4, mat);
    GXSetBlendMode(1, 4, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(0.0f, 0.0f, TEST_Z);
    GXPosition3f32(512.0f, 0.0f, TEST_Z);
    GXPosition3f32(512.0f, 448.0f, TEST_Z);
    GXPosition3f32(0.0f, 448.0f, TEST_Z);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
}
