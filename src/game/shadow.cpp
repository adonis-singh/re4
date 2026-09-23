// game/shadow.cpp: projected character shadows. Type 4 lights cast the shadow of the enemies,
// the player and the objects near them: the models are rendered from the light into a small
// I8 texture (make_shadow_texture) which is projected back onto the scene (shadowModelTrans).

#include "light.h"
#include "atari.h"
#include "global.h"
#include "model.h"
#include "em.h"
#include "obj.h"
#include "camera.h"
#include "view.h"
#include "main_sub.h"
#include "main_mem.h"
#include "math_sub.h"
#include "db_log.h"
#include "trans_ot.h"
#include "room_tex.h"
#include "dbmodule.h"
#include "eprintf.h"
#include "os_vi.h"
#include "shadow.h"
#include "trans.h"
#include "player.h"
#include "pl_npc.h"

// game/trans.cpp; uninitialised there, so not in trans.h (a header extern reorders trans.cpp's .bss)
extern GXTexObj IndTex[2];

static int SHADOW_NUM_MAX = 0;
static int g_Shd_render_size = 0x100;
static int g_Shd_tex_size = 0x80;
int isSelfUse = 1;
int g_SelfShdNum = 0;
f32 shadow_cammove_size = 1.0f;
// Reference read of a unit static: the load is neither in-struct nor a fixed scalar, so it stays
// ordered against the `mng->dir` stores (make_comn_parallel_light issues the pool -1.0 first).

f32 shadow_add_dir_x_default = 0.1f;
f32 shadow_add_dir_x = shadow_add_dir_x_default;
static int g_Shd_num;
static cObj** g_objTbl;
int g_objNum;
ShadowMng** pSelfShadowMng;
ShadowMng* ShadowMngWork;
static GXLightObj* light_obj;

static void drawTexture2(GXTexObj* tex, s16 x, s16 y, s16 z, s16 w, s16 h);

// Light origin of `m`: lightInfo.ofs in the space of the coord lightInfo.PartsNo selects.
// cLightInfo accessors: the address argument is a fresh `&m->lightInfo` computation at each
// use, which gcse's PRE turns into the copies of the first one the original has.
static inline int LightInfoParts(cLightInfo* li) { return li->PartsNo; }
static inline u8 LightInfoShape(cLightInfo* li) { return li->Flag; }

#define SHD_LIGHT_POS(m, pos, msg)                                                  \
    {                                                                               \
        cLightInfo* li = &(m)->LightInfo;                                           \
        if (li->PartsNo > 0) {                                                          \
            cCoord* p = (m)->getPartsPtr(li->PartsNo - 1);                              \
            if ((u32) p < 0x80000000 || (u32) p > 0x82FFFFFF) {                     \
                pLog->err(0, 0, msg, LightInfoParts(&(m)->LightInfo));              \
                p = (m);                                                            \
            }                                                                       \
            PSMTXMultVecSR(p->mat, &li->Offset, &(pos));                               \
            PSVECAdd(&(pos), &p->world, &(pos));                                 \
        } else {                                                                    \
            PSMTXMultVecSR((m)->mat, &(m)->LightInfo.Offset, &(pos));                  \
            PSVECAdd(&(pos), &(m)->pos, &(pos));                                    \
        }                                                                           \
    }

// Room: how far the shadow receiver is pushed toward the camera (fights z-fighting; default 1.0).
void SetShadowCamMoveSize(f32 size)
{
    shadow_cammove_size = size;
}

// Back to the default camera push (1.0).
void ResetShadowCamMoveSize()
{
    shadow_cammove_size = 1.0f;
}

// Room: the x / z tilt of the parallel shadow lights' direction (0 = straight down).
void SetShadowParallelDirX(f32 x)
{
    shadow_add_dir_x = x;
}

// Back to the default tilt.
void ReetShadowParallelDirX()
{
    shadow_add_dir_x = shadow_add_dir_x_default;
}

// Self-shadow manager `no` registered this frame (trans.cpp SelfShadowSetup).
ShadowMng* GetSelfShadowMng(int no)
{
    return pSelfShadowMng[no];
}

// Room start: creates the shadow receiver objects (cObj kind 3, drawn only into the shadow pass:
// be_flag 0x80 set, 2 clear) from the room's SHD placement file (version <= 0x41; model table
// with the core archive's TPL). Returns their number.
int ShdInit(ShdHeader* data)
{
    u32* ofsTbl;
    ShdEntry* e;
    int i;

    if (data == 0) {
        g_objNum = 0;
        g_objTbl = 0;
        return 0;
    }
    if (data->version > 0x41) {
        pLog->err(0, 0, "ShdInit() INVALID VERSION[%x]", data->version);
        return 0;
    }
    g_objNum = data->num;
#line 185 "D:/Bio4/Prog/shadow.cpp"
    g_objTbl = (cObj**) MEM_CALLOC(g_objNum * 4, 1, 13);
    ofsTbl = (u32*) ((u8*) data + data->tblOfs);
    e = data->entry;
    for (i = 0; i < data->num; i++) {
        cObj* obj = ObjMgr.create();
        if (obj == 0) {
            continue;
        }
        obj->kindid = 3;
        if (data->version <= 0x1F) {
            if (e->shdCol == 0) {
                e->shdCol = 0xFF;
            }
        }
        if (obj->modelInit((u8*) ofsTbl + ofsTbl[e->model], (void*) (pG->pCore->ofs_10 + (u32) pG->pCore)) == 0) {
            ObjMgr.destroy(obj);
            continue;
        }
        obj->be_flag &= ~2;
        obj->be_flag |= 0x80;
        obj->pos = e->pos;
        obj->ang = e->rot;
        obj->scale = e->scale;
        e++;
        {
            ModelBound* bound = &obj->pModelInfo->bound;
            Vec size;
            size.x = bound->size.x;
            size.y = bound->size.y;
            size.z = bound->size.z;
            obj->LightInfo.init2(2, 1, &bound->center, &size, 0x10);
        }
        obj->matUpdate();
        obj->LightInfo.updateMatrix(obj);
        g_objTbl[i] = obj;
    }
    return data->num;
}

// Shadow receiver object `no` (0 with an error when out of range) — the sce_at shadow display areas
// toggle their be_flag 0x80.
cObj* ShdGetObjPtr(int no)
{
    if (no >= g_objNum) {
        pLog->err(0, 0, "SgdGetObjPtr() id over %d", no);
        no = 0;
    }
    return g_objTbl[no];
}

// Boot: no shadow managers in use.
void ShadowInit()
{
    g_Shd_num = 0;
    g_SelfShdNum = 0;
}

// Room start: allocates the 24 shadow managers, the self-shadow pointer table and GX light
// objects; resets the camera push and the parallel tilt.
void ShadowRoomInit()
{
    u32 i;
    ShadowMng* mng;

    SHADOW_NUM_MAX = 24;
#line 317 "D:/Bio4/Prog/shadow.cpp"
    pSelfShadowMng = (ShadowMng**) MEM_CALLOC(SHADOW_NUM_MAX * sizeof(ShadowMng*), 1, 13);
    ShadowMngWork = (ShadowMng*) MEM_CALLOC(SHADOW_NUM_MAX * sizeof(ShadowMng), 1, 13);
    light_obj = (GXLightObj*) MEM_CALLOC(SHADOW_NUM_MAX * sizeof(GXLightObj), 1, 13);
    g_Shd_num = 0;
    g_SelfShdNum = 0;
    mng = ShadowMngWork;
    for (i = 0; i < SHADOW_NUM_MAX; i++, mng++) {
        memclr_asm(mng, sizeof(ShadowMng));
    }
    ResetShadowCamMoveSize();
    ReetShadowParallelDirX();
}

// Re-allocates the manager tables for `n` shadows (rooms needing more than 24).
void ShadowMngReAlloc(int n)
{
    u32 i;
    ShadowMng* mng;

    SHADOW_NUM_MAX = n;
    if (pSelfShadowMng) {
        Mem_free(pSelfShadowMng);
    }
    if (ShadowMngWork) {
        Mem_free(ShadowMngWork);
    }
    if (light_obj) {
        Mem_free(light_obj);
    }
#line 349 "D:/Bio4/Prog/shadow.cpp"
    pSelfShadowMng = (ShadowMng**) MEM_CALLOC(SHADOW_NUM_MAX * sizeof(ShadowMng*), 1, 13);
    ShadowMngWork = (ShadowMng*) MEM_CALLOC(SHADOW_NUM_MAX * sizeof(ShadowMng), 1, 13);
    light_obj = (GXLightObj*) MEM_CALLOC(SHADOW_NUM_MAX * sizeof(GXLightObj), 1, 13);
    mng = ShadowMngWork;
    for (i = 0; i < SHADOW_NUM_MAX; i++, mng++) {
        memclr_asm(mng, sizeof(ShadowMng));
    }
}

// Frees every manager's shadow texture and clears them (event end / room change).
void ShadowMemClear()
{
    u32 i;
    ShadowMng* mng;

    g_Shd_num = 0;
    g_SelfShdNum = 0;
    mng = ShadowMngWork;
    for (i = 0; i < SHADOW_NUM_MAX; i++, mng++) {
        if (mng->pTex) {
            Mem_free(mng->pTex);
        }
        memclr_asm(mng, sizeof(ShadowMng));
    }
}

// The next free manager this frame (numbered), 0 when all are used.
ShadowMng* getShadowMng()
{
    ShadowMng* mng;

    if (g_Shd_num == SHADOW_NUM_MAX) {
        return 0;
    }
    mng = &ShadowMngWork[g_Shd_num];
    mng->no = g_Shd_num;
    mng->num = 0;
    g_Shd_num++;
    return mng;
}

// Once per frame (render setup): resets the managers, then for every type 4 (shadow) light —
// fixed lights (xD 2) collect the models in their frustum (FixShadowLightSet); the per-model
// lights (xD 0 fit / 1 parallel) get one manager per casting enemy / object (be_flag 0x10; self
// shadows for be_flag 0x04000000 unless Disp_flg 0x8000); during an event only be_flag 0x800
// models cast. Status_flg[2] 0x00100000 = shadow lights exist. Queues the receiver render pass.
void ShadowTrans()
{
    int found;
    cLight* l;
    cEm* em;
    cObj* obj;
    int cnt;
    int cnt2;

    g_Shd_num = 0;
    g_SelfShdNum = 0;
    if (pG->Disp_flg & 0x02000000) {
        return;
    }
    if (DpfFlagChk(pG, DPF_SELF_SHADOW)) {
        isSelfUse = 0;
    } else {
        isSelfUse = 1;
    }
    StaFlagOff(pG, STA_SHADOW_EQCOL);
    StaFlagOff(pG, STA_USE_SHADOW_LIGHT);

    found = 0;
    l = LightMgr.getActiveWork();
    cnt = 0;
    while (l) {
        ShadowLightWork* w;

        if (cnt != 0) {
            l = LightMgr.getNext(l);
            if (l == 0) {
                break;
            }
        } else {
            cnt++;
        }
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (l->Type != 4) {
            continue;
        }
        if (StaFlagChk(pG, STA_BLACKOUT)) {
            if (l->Kind & 0x80) {
                continue;
            }
        }
        w = (ShadowLightWork*) l->work;
        if (w->mode == 5) {
            continue;
        }
        StaFlagOn(pG, STA_USE_SHADOW_LIGHT);
        if (w->mode >= 1 && w->mode <= 4) {
            continue;
        }
        if (l->xD != 2) {
            continue;
        }
        if (LightMgr.checkKind(l->Kind) == 0) {
            continue;
        }
        FixShadowLightSet(l);
        found = 1;
    }

    if (!StaFlagChk(pG, STA_USE_SHADOW_LIGHT)) {
        return;
    }

    em = EmMgr.getActiveWork();
    cnt2 = 0;
    while (em) {
        if (cnt2 != 0) {
            em = EmMgr.getNext(em);
            if (em == 0) {
                break;
            }
        } else {
            cnt2++;
        }
        if ((em->be_flag & 3) != 3) {
            continue;
        }
        if (!(em->be_flag & 0x04000010)) {
            continue;
        }
        if (em == pPL) {
            if (DpfFlagChk(pG, DPF_PL)) {
                continue;
            }
        } else if (em == pSUB) {
            if (DpfFlagChk(pG, DPF_SUBCHAR)) {
                continue;
            }
        } else {
            if (DpfFlagChk(pG, DPF_EM)) {
                continue;
            }
        }
        if (StaFlagChk(pG, STA_SUSPEND)) {
            if (!(em->be_flag & 0x800)) {
                continue;
            }
        }
        {
            // COMPILER-DIFF: 5 (interblock hoist of the `mr r3, em` argument copy above the flag test)
            cEm* e = em;
            asm("" : "+r"(e));
            if (e->be_flag & 0x10) {
                if (Fit_ParallelShadowModelSet(e, 0) == 1) {
                    found = 1;
                }
            }
        }
        if (em->be_flag & 0x04000000) {
            if (isSelfUse) {
                if (Fit_ParallelShadowModelSet(em, 1) == 1) {
                    found = 1;
                }
            }
        }
    }

    obj = ObjMgr.getActiveWork();
    cnt2 = 0;
    while (obj) {
        if (cnt2 != 0) {
            obj = ObjMgr.getNext(obj);
            if (obj == 0) {
                break;
            }
        } else {
            cnt2++;
        }
        if ((obj->be_flag & 3) != 3) {
            continue;
        }
        if (!(obj->be_flag & 0x04000010)) {
            continue;
        }
        if (StaFlagChk(pG, STA_SUSPEND)) {
            if (!(obj->be_flag & 0x800)) {
                continue;
            }
        }
        if (obj->be_flag & 0x10) {
            if (Fit_ParallelShadowModelSet(obj, 0) == 1) {
                found = 1;
            }
        }
        if (obj->be_flag & 0x04000000) {
            if (isSelfUse) {
                if (Fit_ParallelShadowModelSet(obj, 1) == 1) {
                    found = 1;
                }
            }
        }
    }

    if (found) {
        AddOtDirect(0xE, ShadowMngWork, (void (*)()) shadowScrModelRender, 0, 4, NULL, 0.0f);
    }
}

// Registers a shadow of `m` for every fit / parallel shadow light that reaches it (light mask,
// radius, self-shadow enabled when `self`). Returns 1 when at least one was added.
int Fit_ParallelShadowModelSet(cModel* m, int self)
{
    int ret = 0;
    cLight* l;
    int cnt;

    l = LightMgr.getActiveWork();
    cnt = 0;
    while (l) {
        ShadowLightWork* w;
        int inRange;

        if (cnt != 0) {
            l = LightMgr.getNext(l);
            if (l == 0) {
                break;
            }
        } else {
            cnt++;
        }
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (l->Type != 4) {
            continue;
        }
        if (StaFlagChk(pG, STA_BLACKOUT)) {
            if (l->Kind & 0x80) {
                continue;
            }
        }
        w = (ShadowLightWork*) l->work;
        if (w->mode == 5) {
            continue;
        }
        if (l->xD > 1) {
            continue;
        }
        if (!(l->xF & m->LightInfo.EnableMask)) {
            continue;
        }
        if (self) {
            if (w->selfShadow == 0) {
                continue;
            }
        }
        if (LightMgr.checkKind(l->Kind) == 0) {
            continue;
        }
        inRange = 1;
        if (l->Radius != 0.0f) {
            Vec d;
            Vec lpos;
            Vec pos;

            SHD_LIGHT_POS(m, pos, "Fit_ParallelShadowModelSet() cParts NO ERR %d");
            l->getPos(&lpos);
            PSVECSubtract(&lpos, &pos, &d);
            if (PSVECMag(&d) > l->Radius) {
                inRange = 0;
            }
        }
        if (!inRange) {
            continue;
        }
        if (g_Shd_num == SHADOW_NUM_MAX) {
            pLog->warn(0, 0, "ShadowModelSet():SHADOW NUM MAX!!");
            return ret;
        }
        Fit_ParallelShadowModelAddOt(l, m, self);
        ret = 1;
    }
    return ret;
}

// Takes a manager for light `l` casting model `m`, builds its light matrices (fit or parallel by
// the light's xD) and queues the shadow texture render (OT 8).
void Fit_ParallelShadowModelAddOt(cLight* l, cModel* m, int self)
{
    ShadowMng* mng = getShadowMng();

    if (mng == 0) {
        pLog->warn(0, 0, "ShadowModelSet():SHADOW NUM MAX!!");
        return;
    }
    mng->pLight = l;
    mng->pModel[mng->num++] = m;
    if (l->xD == 0) {
        make_comn_fit_light(mng, mng->pModel[0]);
    } else {
        make_comn_parallel_light(mng, mng->pModel[0]);
    }
    if (self) {
        mng->self = 1;
    } else {
        mng->self = 0;
    }
    AddOtDirect(8, mng, (void (*)()) shadowModelRender, 1, 4, NULL, 0.0f);
}

// A fixed shadow light: gathers up to 8 enemies / objects inside its frustum (visible, matching
// the light mask) into one manager and queues the shadow texture render.
void FixShadowLightSet(cLight* l)
{
    ShadowMng tmp;
    ShadowMng* mng = 0;
    cEm* em;
    cObj* obj;
    int cnt;

    tmp.pLight = l;
    make_fix_light(&tmp);

    em = EmMgr.getActiveWork();
    cnt = 0;
    while (em) {
        if (cnt != 0) {
            em = EmMgr.getNext(em);
            if (em == 0) {
                break;
            }
        } else {
            cnt++;
        }
        if ((em->be_flag & 0x13) != 0x13) {
            continue;
        }
        if (em == pPL) {
            if (DpfFlagChk(pG, DPF_PL)) {
                continue;
            }
        } else if (em == pSUB) {
            if (DpfFlagChk(pG, DPF_SUBCHAR)) {
                continue;
            }
        } else {
            if (DpfFlagChk(pG, DPF_EM)) {
                continue;
            }
        }
        if (StaFlagChk(pG, STA_SUSPEND)) {
            if (!(em->be_flag & 0x800)) {
                continue;
            }
        }
        if (!(l->xF & em->LightInfo.EnableMask)) {
            continue;
        }
        if (shadowChkInFrustum(&tmp, em) == 0) {
            continue;
        }
        if (mng == 0) {
            mng = getShadowMng();
            mng->pLight = l;
            make_fix_light(mng);
        }
        if (mng->num > 7) {
            pLog->warn(0, 0, "FixShadowLightSet() : too many models[%d]", mng->num);
            break;
        }
        mng->pModel[mng->num++] = em;
    }

    obj = ObjMgr.getActiveWork();
    cnt = 0;
    while (obj) {
        if (cnt != 0) {
            obj = ObjMgr.getNext(obj);
            if (obj == 0) {
                break;
            }
        } else {
            cnt++;
        }
        if ((obj->be_flag & 0x13) != 0x13) {
            continue;
        }
        if (obj->kindid == 2) {
            if (DpfFlagChk(pG, DPF_SCR)) {
                return;
            }
        } else {
            if (DpfFlagChk(pG, DPF_OBJ)) {
                return;
            }
        }
        if (StaFlagChk(pG, STA_SUSPEND)) {
            if (!(obj->be_flag & 0x800)) {
                continue;
            }
        }
        if (shadowChkInFrustum(&tmp, obj) == 0) {
            continue;
        }
        if (!(l->xF & obj->LightInfo.EnableMask)) {
            continue;
        }
        if (mng == 0) {
            mng = getShadowMng();
            mng->pLight = l;
            make_fix_light(mng);
        }
        if (mng->num > 7) {
            pLog->warn(0, 0, "FixShadowLightSet() : too many models[%d]", mng->num);
            break;
        }
        mng->pModel[mng->num++] = obj;
    }

    if (mng) {
        AddOtDirect(8, mng, (void (*)()) shadowModelRender, 1, 4, NULL, 0.0f);
    }
}

// OT callback: allocates the manager's I8 shadow texture on first use and renders it
// (make_shadow_texture); Status_flg[1] 0x100 while rendering.
void shadowModelRender(ShadowMng* mng)
{
    StaFlagOn(pG, STA_PROC_SHD_TEX);
    if (mng->pTex == 0) {
#line 846 "D:/Bio4/Prog/shadow.cpp"
        mng->pTex = MEM_ALLOC(g_Shd_tex_size * g_Shd_tex_size, 1, 13);
        DCInvalidateRange(mng->pTex, g_Shd_tex_size * g_Shd_tex_size);
        if (mng->pTex == 0) {
            pLog->warn(0, 0, "ShadowModelRender() : not enough memory");
            return;
        }
    }
    make_shadow_texture(mng);
    StaFlagOff(pG, STA_PROC_SHD_TEX);
}

// Light matrices of a "fit" shadow: from the light position (or its `pos` source) looking at the
// model's light-info point, with the field of view just covering the model's light-info size
// (1..89.95 degrees, minus angleSub): lookAt and the texture projection texMat.
void make_comn_fit_light(ShadowMng* mng, cModel* m)
{
    cLight* l = mng->pLight;
    ShadowLightWork* w = (ShadowLightWork*) l->work;
    Vec pos;
    f32 dist;
    f32 r;

    if (w->flags & 2) {
        l->getPos2(&w->pos, &mng->lightPos);
    } else {
        l->getPos(&mng->lightPos);
    }
    mng->target = m->pos;
    SHD_LIGHT_POS(m, pos, "LightHitCheck() cCoord NO ERR %d");
    mng->target = pos;
    PSVECSubtract(&mng->target, &mng->lightPos, &mng->dir);
    dist = PSVECMag(&mng->dir);
#line 916 "D:/Bio4/Prog/shadow.cpp"
    VECNormalize(&mng->dir, &mng->dir);
    switch (LightInfoShape(&m->LightInfo) & 3) {
    case 1:
        r = m->LightInfo.Size.x + m->LightInfo.Size.x * 0.1f + 200.0f;
        break;
    case 0:
        r = m->LightInfo.Size.y + m->LightInfo.Size.y * 0.1f + 200.0f;
        break;
    default:
        r = SQRTF(m->LightInfo.Size.x * m->LightInfo.Size.x + m->LightInfo.Size.y * m->LightInfo.Size.y) * 1.15f + 100.0f;
        break;
    }
    mng->fov = atan2f(r, dist) * (360.0f / PI);
    mng->fov -= (f32) (int) w->angleSub;
    {
        // COMPILER-DIFF: 13 (local-alloc qty order): r11 pinned after the 1.0 load and before the
        // conversion's lfd, so the fpmem loadaddr cannot take r11 and the 1.0 pool high gets it.
        register u32 k asm("r11");
        asm("" : "=r"(k) : "f"(1.0f));
        asm("" : "=m"(pos.x) : "r"(k));
        if (mng->fov < 1.0f) {
            mng->fov = 1.0f;
        }
    }
    if (mng->fov > 89.95f) {
        Vec v;
        mng->fov = 89.95f;
        PSVECSubtract(&mng->lightPos, &mng->target, &v);
#line 942 "D:/Bio4/Prog/shadow.cpp"
        VECNormalize(&v, &v);
        PSVECScale(&v, &v, r);
        PSVECAdd(&mng->target, &v, &mng->lightPos);
    }
    {
        Vec up = {0.0f, 1.0f, 0.0f};
        Mtx proj;
        C_MTXLookAt(mng->lookAt, &mng->lightPos, &up, &mng->target);
        C_MTXLightPerspective(proj, mng->fov, 1.0f, 0.5f, -0.5f, 0.5f, 0.5f);
        PSMTXConcat(proj, mng->lookAt, mng->texMat);
    }
}

// Light matrices of a "parallel" (directional) shadow: the light sits at the model, pointing down
// tilted by shadow_add_dir_x and the light's rotX / rotY, fov covering the model's size.
void make_comn_parallel_light(ShadowMng* mng, cModel* m)
{
    cLight* l = mng->pLight;
    ShadowLightWork* w;
    Vec rot;
    Vec v;
    Mtx rm;
    Vec pos;
    f32 dist;
    f32 r;

    SHD_LIGHT_POS(m, pos, "LightHitCheck() cCoord NO ERR %d");
    mng->target = pos;
    mng->lightPos = mng->target;
    mng->dir.x = shadow_add_dir_x;
    mng->dir.y = -1.0f;
    mng->dir.z = shadow_add_dir_x;
    PSVECNormalize(&mng->dir, &mng->dir);
    w = (ShadowLightWork*) l->work;
    {
        const f32 zero = 0.0f;
        rot.x = (f32) w->rotX * 6.2831855f / 360.0f;
        rot.y = (f32) w->rotY * 6.2831855f / 360.0f;
        rot.z = zero;
    }
    RotMatrix(rm, &rot);
    PSMTXMultVecSR(rm, &mng->dir, &v);
    PSVECScale(&v, &v, -5000.0f);
    PSVECAdd(&mng->lightPos, &v, &mng->lightPos);
    PSVECSubtract(&mng->target, &mng->lightPos, &mng->dir);
    dist = PSVECMag(&mng->dir);
#line 1022 "D:/Bio4/Prog/shadow.cpp"
    VECNormalize(&mng->dir, &mng->dir);
    switch (LightInfoShape(&m->LightInfo) & 3) {
    case 1:
        r = m->LightInfo.Size.x + m->LightInfo.Size.x * 0.2f + 200.0f;
        break;
    case 0:
        r = m->LightInfo.Size.y + m->LightInfo.Size.y * 0.2f + 200.0f;
        break;
    default:
        r = SQRTF(m->LightInfo.Size.x * m->LightInfo.Size.x + m->LightInfo.Size.y * m->LightInfo.Size.y + m->LightInfo.Size.z * m->LightInfo.Size.z) * 1.2f + 200.0f;
        break;
    }
    mng->fov = atan2f(r, dist) * (360.0f / PI);
    mng->fov -= w->angleSub;
    {
        // COMPILER-DIFF: 13 (local-alloc qty order): see make_comn_fit_light.
        register u32 k asm("r11");
        asm("" : "=r"(k) : "f"(1.0f));
        asm("" : "=m"(pos.x) : "r"(k));
        if (mng->fov < 1.0f) {
            mng->fov = 1.0f;
        }
    }
    if (mng->fov > 89.95f) {
        Vec v2;
        mng->fov = 89.95f;
        PSVECSubtract(&mng->lightPos, &mng->target, &v2);
#line 1048 "D:/Bio4/Prog/shadow.cpp"
        VECNormalize(&v2, &v2);
        PSVECScale(&v2, &v2, r);
        PSVECAdd(&mng->target, &v2, &mng->lightPos);
    }
    {
        Vec up = {0.0f, 1.0f, 0.0f};
        Mtx proj;
        C_MTXLookAt(mng->lookAt, &mng->lightPos, &up, &mng->target);
        C_MTXLightPerspective(proj, mng->fov, 1.0f, 0.5f, -0.5f, 0.5f, 0.5f);
        PSMTXConcat(proj, mng->lookAt, mng->texMat);
    }
}

// Light matrices of a fixed shadow light: its own position and normal (rotX / rotY), fov = the
// light's `angle` (0 = 90 degrees).
#line 1061
void make_fix_light(ShadowMng* mng)
{
    Vec rot;
    Vec axis = {0.0f, 1.0f, 0.0f};
    Mtx m1;
    Mtx m2;
    cLight* l = mng->pLight;
    ShadowLightWork* w = (ShadowLightWork*) l->work;

    l->getPos(&mng->lightPos);
    mng->target = mng->lightPos;
    mng->dir.x = 0.0f;
    mng->dir.y = -1.0f;
    mng->dir.z = 0.0f;
    PSVECNormalize(&mng->dir, &mng->dir);
    rot.x = (f32) w->rotX * 6.2831855f / 360.0f;
    rot.y = (f32) w->rotY * 6.2831855f / 360.0f;
    rot.z = 0.0f;
    PSMTXRotRad(m1, 'x', rot.x);
    PSMTXRotAxisRad(m2, &axis, rot.y);
    PSMTXConcat(m2, m1, m1);
    PSMTXMultVecSR(m1, &mng->dir, &mng->dir);
    l->getNormal(&mng->dir, &mng->dir);
    if (mng->dir.x == 0.0f && mng->dir.z == 0.0f) {
        mng->dir.x = 0.01f;
    }
    PSVECAdd(&mng->target, &mng->dir, &mng->target);
    mng->fov = (f32) w->angle;
    if (mng->fov == 0.0f) {
        mng->fov = 90.0f;
    }
    {
        Vec up = {0.0f, 1.0f, 0.0f};
        Mtx proj;
        C_MTXLookAt(mng->lookAt, &mng->lightPos, &up, &mng->target);
        C_MTXLightPerspective(proj, mng->fov, 1.0f, 0.5f, -0.5f, 0.5f, 0.5f);
        PSMTXConcat(proj, mng->lookAt, mng->texMat);
    }
}

// Copies the rendered shadow square out of the EFB into the manager's texture, scaled by sx / sy.
void SoftShadowGetEFB(ShadowMng* mng, f32 sx, f32 sy, int clear)
{
    int size;

    GXSetAlphaUpdate(1);
    size = (u16) g_Shd_tex_size;
    GXSetTexCopySrc(0, 0, (u32) ((f32) size / sx), (u32) ((f32) size / sx));
    GXSetTexCopyDst((u32) ((f32) size / sy), (u32) ((f32) size / sy), 0x27, 1);
    GXCopyTex(mng->pTex, clear);
    GXPixModeSync();
    GXInvalidateTexAll();
}

// Draws the shadow texture as a screen quad (size / div, at x / y, texture offset u / v, alpha,
// scale) — one blur tap of the soft shadow.
void SoftShadowGXDraw(ShadowMng* mng, f32 x, f32 y, f32 z, u32 div, f32 u, f32 v, f32 alpha, f32 scale)
{
    GXTexObj tex;
    Mtx44 proj;
    Mtx pos;
    GXColor amb;
    u16 size = g_Shd_tex_size;
    u16 w;
    f32 tw;
    u8 a;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 1);
    w = size / div;
    GXInitTexObj(&tex, mng->pTex, w, w, 1, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    C_MTXOrtho(proj, 0.0f, (f32) (u32) Screen.height, 0.0f, (f32) (u32) Screen.width, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(pos);
    GXLoadPosMtxImm(pos, 0);
    GXSetCurrentMtx(0);
    GXSetNumTevStages(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x3C, 0, 0x7D);
    GXSetChanCtrl(0, 1, 1, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 1, 1, 0, 2, 1);
    GXSetChanAmbColor(4, amb);
    GXSetNumChans(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 8);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 4);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    a = (u8) alpha;
    tw = (f32) size / scale;
    GXPosition3f32(x + 0.0f, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, a);
    GXTexCoord2f32(u + 0.0f, v + 0.0f);
    GXPosition3f32(x + tw, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, a);
    GXTexCoord2f32(u + 1.0f, v + 0.0f);
    GXPosition3f32(x + tw, y + tw, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, a);
    GXTexCoord2f32(u + 1.0f, v + 1.0f);
    GXPosition3f32(x + 0.0f, y + tw, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, a);
    GXTexCoord2f32(u + 0.0f, v + 1.0f);
}


// Blurs the shadow texture: several down / up-scaled draw-and-copy passes (more with the light's
// `soft` count) over the EFB.
void MakeSoftShadow(ShadowMng* mng)
{
    static f32 fa = 1.0f;
    static f32 fb = 2.0f;
    static f32 fc = 1.0f;
    static int fd = 2;
    f32 z = 65530.0f;
    f32 zero0 = 0.0f;
    f32 zero;
    f32 alpha;
    ShadowLightWork* w;
    int a;

    SetNoScissor();
    // Two zero variables: the pool 0.0 is a declaration initialiser (f28, loaded before SetNoScissor,
    // live through the whole function) and the argument variable is a copy of it, assigned here and
    // again at the top of the else arm (`fmr f31,f28` twice; a second cse ebb keeps the second copy).
    zero = zero0;
    SoftShadowGetEFB(mng, 0.5f, 1.0f, 1);
    a = 0xFF;
    alpha = (f32) (u8) a;
    SoftShadowGXDraw(mng, zero, zero, z, 1, zero, zero, alpha, 1.0f);
    w = (ShadowLightWork*) mng->pLight->work;
    if (w->soft > 1) {
        SoftShadowGetEFB(mng, 1.0f, 2.0f, 1);
        SoftShadowGXDraw(mng, zero, zero, z, 2, zero, zero, alpha, 2.0f);
        if (w->soft > 2) {
            SoftShadowGetEFB(mng, fa, fb, 1);
            a = 0x80;
            SoftShadowGXDraw(mng, zero, zero, z, fd, zero, zero, (f32) (u8) a, fc);
        }
        SoftShadowGetEFB(mng, 1.0f, 2.0f, 1);
        SoftShadowGXDraw(mng, zero, zero, z, 2, zero, zero, alpha, 0.25f);
    } else {
        zero = zero0;
        SoftShadowGetEFB(mng, 1.0f, 2.0f, 1);
        SoftShadowGXDraw(mng, zero, zero, z, 2, zero, zero, alpha, 0.5f);
    }
    SoftShadowGetEFB(mng, 0.5f, 1.0f, 1);
    SetScissorState();
}

f32 shd_ofs = 2000.0f;
f32 shd_tex_scale_x = 0.0003f;  // trans.cpp SelfShadowSetup reads it

#define SHD_NO_SELF(mng) (!isSelfUse || !((mng)->self & 1))

// Renders the manager's shadow map: the casting models drawn from the light (lookAt, per-model
// alpha fading with the distance for lights with a radius; the model's own shadow model info when
// it has one; self shadows use the depth variant), optional light-map texture multiplied in
// (flags bit0 room texture / fixed-light texId), soft blur, then copied to the I8 texture; sets
// Status_flg[1] 0x4000 when the light asks (setStatus).
void make_shadow_texture(ShadowMng* mng)
{
    ShadowLightWork* w = (ShadowLightWork*) mng->pLight->work;
    u32 scrW;
    u32 scrH;
    u16 rs;
    u16 ts;
    u32 i;
    int flag2;
    int flag1;

    GXSetColorUpdate(0);
    GXSetAlphaUpdate(1);
    GXSetZMode(0, 7, 0);
    GXSetCullMode(0);
    GXSetBlendMode(0, 1, 0, 0);
    scrW = (u32) Screen.width;
    scrH = (u32) Screen.height;
    GXInvalidateTexAll();
    rs = g_Shd_render_size;
    ts = g_Shd_tex_size;
    GXSetViewport(0.0f, 0.0f, (f32) rs, (f32) rs, 0.0f, 1.0f);
    GXSetScissor(2, 2, rs - 4, rs - 4);
    if (SHD_NO_SELF(mng)) {
        GXColor c;
        GXSetNumChans(1);
        GXSetChanCtrl(4, 0, 0, 0, 0, 2, 2);
        c.r = c.g = c.b = c.a = 0;
        GXSetChanAmbColor(4, c);
        c.r = c.g = c.b = c.a = mng->pLight->Col.r;
        GXSetChanMatColor(4, c);
        GXSetNumTexGens(0);
        GXSetNumTevStages(1);
        GXSetTevOrder(0, 0xFF, 0xFF, 4);
        GXSetTevOp(0, 4);
    } else {
        static int shd_tex_no = 0;
        static u8 s_c_p = 0x80;
        Mtx tm;
        Mtx sm;
        Mtx trans;
        GXColor c;

        pSelfShadowMng[g_SelfShdNum] = mng;
        g_SelfShdNum++;
        StaFlagOn(pG, STA_SELF_SHADOW);
        GXLoadTexObj(&IndTex[shd_tex_no], 0);
        Vec up = {0.0f, 1.0f, 0.0f};
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
        PSMTXConcat(tm, mng->pModel[0]->pList->mat, tm);
        PSMTXConcat(trans, tm, tm);
        PSMTXConcat(sm, tm, tm);
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXSetTexCoordGen2(0, 1, 0, 0x1E, 0, 0x7D);
        GXSetNumChans(1);
        GXSetChanCtrl(4, 0, 0, 0, 0, 2, 2);
        c.r = c.g = c.b = c.a = 0;
        GXSetChanAmbColor(4, c);
        c.r = c.g = c.b = c.a = s_c_p;
        GXSetChanMatColor(4, c);
        GXSetNumTexGens(1);
        GXSetNumTevStages(1);
        GXSetTevOrder(0, 0, 0, 4);
        GXSetTevColorIn(0, 0xF, 0xF, 0xF, 8);
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(0, 7, 7, 7, 4);
        GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
        GXSetBlendMode(1, 1, 0, 0);
        GXSetAlphaCompare(7, 0, 1, 7, 0);
        GXSetZMode(1, 3, 1);
    }
    GXClearBoundingBox();
    {
    Mtx44 proj;
    C_MTXPerspective(proj, mng->fov, 1.0f, ZNEAR, ZFAR);
    GXSetProjection(proj, 0);
    for (i = 0; i < mng->num; i++) {
        cModel* m = mng->pModel[i];
        cModel* p;
        u8 a;

        a = mng->pLight->Col.r * (256 - m->Shd_color) / 256;
        if (mng->pLight->Radius != 0.0f) {
            Vec d;
            Vec lpos;
            Vec pos;
            f32 rate;

            mng->pLight->getPos(&lpos);
            SHD_LIGHT_POS(m, pos, "Fit_ParallelShadowModelSet() cParts NO ERR %d");
            PSVECSubtract(&lpos, &pos, &d);
            rate = PSVECMag(&d) / mng->pLight->Radius;
            if (rate > 0.7f) {
                rate = 1.0f - rate;
                rate *= 10.0f / 3.0f;
                a = (f32) (int) a * rate;
            }
        }
        if (SHD_NO_SELF(mng)) {
            GXSetDstAlpha(1, a);
        }
        p = mng->pModel[i];
    NEXT_MODEL:
        {
            cModelInfo* info;
            cModel* n;
            if (p->pShadowModelInfo != 0) {
                info = p->pShadowModelInfo;
            } else {
                info = p->pModelInfo;
            }
            if (SHD_NO_SELF(mng)) {
                commonModelTrans(p, info, mng->lookAt, 1);
            } else {
                shadowModelTrans2(p, info, mng->lookAt);
            }
            n = (cModel*) p->pCldShMd;
            if (n && (n->be_flag & 0x12)) {
                p = n;
                goto NEXT_MODEL;
            }
        }
    }
    }
    GXSetViewport(0.0f, 0.0f, (f32) scrW, (f32) scrH, 0.0f, 1.0f);
    GXSetDstAlpha(0, 0);
    if (SHD_NO_SELF(mng)) {
        flag2 = 0;
        if (w->flags & 4) {
            flag2 = 1;
        }
        if (w->flags & 1) {
            GXTexObj* tex;
            GXTlutObj* tlut;
            if (RoomGetTexObj(w->texId, 0, &tex)) {
                RoomGetTlutObj(w->texId, &tlut);
                TransLightTexture(tex, tlut, 0, 0, 1, g_Shd_render_size, g_Shd_render_size, mng, flag2, 1);
            }
        } else if (mng->pLight->xD == 2 && w->texId != 0xFF) {
            GXTexObj* tex;
            GXTlutObj* tlut;
            if (RoomGetTexObj(0, 0, &tex)) {
                RoomGetTlutObj(0, &tlut);
                TransLightTexture(tex, tlut, 0, 0, 1, g_Shd_render_size, g_Shd_render_size, mng, flag2, 0);
            }
        }
    }
    SetScissorState();
    GXSetCopyFilter(0, 0, 0, 0);
    if (w->soft) {
        MakeSoftShadow(mng);
    } else {
        GXSetTexCopySrc(0, 0, rs, rs);
        if (rs == ts) {
            GXSetTexCopyDst(ts, ts, 0x27, 0);
        } else {
            GXSetTexCopyDst(ts, ts, 0x27, 1);
        }
        GXSetAlphaUpdate(1);
        GXCopyTex(mng->pTex, 1);
        GXPixModeSync();
    }
    if (w->setStatus) {
        StaFlagOn(pG, STA_SHADOW_EQCOL);
    }
    GXSetAlphaUpdate(0);
    GXSetCopyFilter(Rmode.aa, Rmode.sample_pattern, 1, Rmode.vfilter);
    GXSetViewport(0.0f, 0.0f, (f32) scrW, (f32) scrH, 0.0f, 1.0f);
    SetScissorState();
    GXInitTexObj(&mng->texObj, mng->pTex, ts, ts, 1, 0, 0, 0);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    GXSetDstAlpha(0, 0);
}

// 1 when the model's light-info sphere (scaled) lies inside the light's cone / range (Debug_flg[2]
// 0x800 draws the sphere).
int shadowChkInFrustum(ShadowMng* mng, cModel* m)
{
    int ret = 0;
    cLightInfo* li = &m->LightInfo;
    Vec d;
    Vec pos;
    f32 r;
    f32 dist;
    f32 dot;
    f32 x1C;

    r = PSVECMag(&m->LightInfo.Size) * 0.99f;
    if (m->scale.x != 1.0f || m->scale.y != 1.0f || m->scale.z != 1.0f) {
        f32 s = m->scale.x;
        if (s < m->scale.y) {
            s = m->scale.y;
        }
        if (s < m->scale.z) {
            s = m->scale.z;
        }
        r *= s;
    }
    if (li->PartsNo > 0) {
        cCoord* p = m->getPartsPtr(li->PartsNo - 1);
        if ((u32) p < 0x80000000 || (u32) p > 0x82FFFFFF) {
            pLog->err(0, 0, "shadowChkInFrustum() cCoord NO ERR %d", li->PartsNo);
            p = m;
        }
        PSMTXMultVecSR(p->mat, &li->Offset, &pos);
        PSVECAdd(&pos, &p->world, &pos);
    } else {
        PSMTXMultVecSR(m->mat, &li->Offset, &pos);
        PSVECAdd(&pos, &m->pos, &pos);
    }
    PSVECSubtract(&pos, &mng->lightPos, &d);
    dist = PSVECMag(&d);
    if (dist < r) {
        return 1;
    }
    dot = PSVECDotProduct(&d, &mng->dir);
    x1C = mng->pLight->Radius;
    if (x1C == 0.0f) {
        return 1;
    }
    if (dot > 0.0f) {
        if (dot < r + x1C) {
            Vec c;
            Vec v;
            f32 s;
            PSVECScale(&mng->dir, &v, dot);
            PSVECAdd(&mng->lightPos, &v, &c);
            PSVECSubtract(&pos, &c, &v);
            s = dot * sinf(mng->fov * 6.2831855f / 360.0f);
            if (PSVECMag(&v) < s + r) {
                ret = 1;
            }
        }
    }
    if (DbgFlagChk(pG, DBG_SHADOW_LIGHT)) {
        if (ret == 1) {
            Draw_sphere(&pos, r, 0x80800080, 1, 1);
        } else {
            Draw_sphere(&pos, r, 0x30303030, 1, 1);
        }
    }
    return ret;
}

// Draws receiver model `m` with the shadow textures of the (up to 4 per pass) fit / parallel
// managers whose light reaches it (fixed-light and self-shadow managers excluded); kind 3
// receivers use their own colour (shdCol).
void ProcShadowScrModel(cModel* m, ShadowMng* mngs)
{
    ShadowMng* tbl[SHADOW_NUM_MAX];
    u32 num = 0;
    u32 i;
    u32 n;
    u32 shdNum = g_Shd_num;

    for (i = 0; i < shdNum; i++, mngs++) {
        if (mngs->pLight->xD > 2) {
            continue;
        }
        if (((ShadowLightWork*) mngs->pLight->work)->mode != 0) {
            continue;
        }
        if (mngs->self & 1) {
            continue;
        }
        if (shadowChkInFrustum(mngs, m) == 0) {
            continue;
        }
        tbl[num++] = mngs;
    }
    if (num == 0) {
        return;
    }
    if (m->kindid == 3) {
        m->be_flag |= 2;
        commonScreenMat(m);
        m->be_flag &= ~2;
    }
    GXSetProjection(pG->Camera.ProjMat, 0);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetAlphaCompare(4, 0, 1, 4, 0xFF);
    n = (num - 1) / 4 + 1;
    for (i = 0; i < n; i++) {
        u32 cnt = num - i * 4;
        if (cnt > 4) {
            cnt = 4;
        }
        shadowModelTrans(m, m->pModelInfo, pG->Camera.v_mat, &tbl[i * 4], cnt);
    }
}

// OT callback (OT 0xE): draws every receiver (objects and enemies with be_flag 0x80) through
// ProcShadowScrModel, then the fixed-light managers project their textures; debug_mode 0xE /
// Debug_flg[1] 0x04000000 show the shadow textures on screen.
void shadowScrModelRender(ShadowMng* mngs)
{
    cObj* obj;
    cEm* em;
    ShadowMng* mng;
    int cnt;
    u32 i;
    int n;
    int num;

    obj = ObjMgr.getActiveWork();
    cnt = 0;
    while (obj) {
        if (cnt != 0) {
            obj = ObjMgr.getNext(obj);
            if (obj == 0) {
                break;
            }
        } else {
            cnt++;
        }
        {
            // COMPILER-DIFF: 5 (interblock hoist of the `mr r3, obj` argument copy above the flag test)
            cObj* o = obj;
            asm("" : "+r"(o));
            if (o->be_flag & 1) {
                if (o->be_flag & 0x80) {
                    ProcShadowScrModel(o, mngs);
                }
            }
        }
    }
    em = EmMgr.getActiveWork();
    cnt = 0;
    while (em) {
        if (cnt != 0) {
            em = EmMgr.getNext(em);
            if (em == 0) {
                break;
            }
        } else {
            cnt++;
        }
        if (em->be_flag & 1) {
            if (em->be_flag & 0x80) {
                ProcShadowScrModel(em, mngs);
            }
        }
    }
    for (i = 0; i < 8; i++) {
        GXSetTevSwapMode(i, 0, 0);
    }
    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 0, 1, 2, 3);
    GXSetTevSwapModeTable(2, 0, 1, 2, 3);
    GXSetTevSwapModeTable(3, 0, 1, 2, 3);
    mng = mngs;
    num = g_Shd_num;
    for (n = 0; n < num; n++) {
        ShadowLightWork* w = (ShadowLightWork*) mng->pLight->work;
        if (w->mode >= 1 && w->mode <= 4) {
            mng++;
            continue;
        }
        if (pG->debug_mode == 0xE || (DbgFlagChk(pG, DBG_DRAW_SH_TEX) && mng->pTex)) {
            drawTexture2(&mng->texObj, 0x1A0, mng->no * 0x58 + 0x40, 1, 0x50, 0x50);
        }
        if (pG->debug_mode == 0xE || (DbgFlagChk(pG, DBG_DRAW_SH_TEX))) {
            eprintf(0x180, mng->no * 0x58 + 0x90, 4, 0, "F:%f ", mng->fov);
        }
        mng++;
    }
    DbgFlagOff(pG, DBG_DRAW_SH_TEX);
}

// Dead-stripped in the DOL (STRIP_UNUSED): its colour table and constant pool remain in .rodata.
static void shadowShaderSetup(ShadowMng** tbl, u32 num)
{
    static const GXColor col_tbl[4] = {{0xFF, 0, 0, 0}, {0, 0xFF, 0, 0}, {0, 0, 0xFF, 0}, {0, 0, 0, 0xFF}};
    u32 i;

    for (i = 0; i < num; i++) {
        ShadowMng* mng = tbl[i];
        Vec p;
        Vec d;
        PSMTXMultVec(pG->Camera.v_mat, &mng->lightPos, &p);
        GXInitLightPos(&light_obj[i], p.x, p.y, p.z);
        PSMTXMultVecSR(pG->Camera.v_mat, &mng->dir, &d);
        GXInitLightDir(&light_obj[i], d.x, d.y, d.z);
        GXInitLightAttn(&light_obj[i], 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        GXInitLightSpot(&light_obj[i], mng->fov * 0.9f, 1);
        GXInitLightColor(&light_obj[i], col_tbl[i]);
        GXLoadLightObjImm(&light_obj[i], 1 << i);
    }
}

// TEV / texgen setup for projecting `num` shadow textures onto one model part (per-part variant).
void shadowShaderSetup2(cModel* m, ModelPart* part, ShadowMng** tbl, u32 num)
{
    static const GXColor col_tbl[4] = {{0xFF, 0, 0, 0}, {0, 0xFF, 0, 0}, {0, 0, 0xFF, 0}, {0, 0, 0, 0xFF}};
    static const GXColor col0 = {0, 0, 0, 0};
    static const GXColor col255 = {0xFF, 0xFF, 0xFF, 0xFF};
    GxStageWork* gs = &pG->gxStage;
    ShadowMng* mng;
    u32 lightMask;
    u32 i;

    gs->tevStage = 0;
    gs->texMap = 0;
    gs->texCoord = 0;
    lightMask = 0;
    for (i = 0; i < num; i++) {
        Vec p;
        mng = tbl[i];
        Vec d;
        PSMTXMultVec(pG->Camera.v_mat, &mng->lightPos, &p);
        GXInitLightPos(&light_obj[i], p.x, p.y, p.z);
        PSMTXMultVecSR(pG->Camera.v_mat, &mng->dir, &d);
        GXInitLightDir(&light_obj[i], d.x, d.y, d.z);
        GXInitLightAttn(&light_obj[i], 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        GXInitLightSpot(&light_obj[i], mng->fov * 0.9f, 1);
        GXInitLightColor(&light_obj[i], col_tbl[i]);
        lightMask |= 1 << i;
        GXLoadLightObjImm(&light_obj[i], 1 << i);
    }
    GXSetZMode(1, 3, 0);
    for (i = 0; i < num; i++) {
        Mtx tm;
        mng = tbl[i];
        PSMTXConcat(mng->texMat, m->pList->mat, tm);
        GXLoadTexMtxImm(tm, 0x1E + i * 3, 0);
        GXSetTexCoordGen2(gs->texCoord, 0, 0, 0x1E + i * 3, 0, 0x7D);
        GXLoadTexObj(&mng->texObj, gs->texMap);
        GXSetTevDirect(gs->tevStage);
        GXSetTevOrder(gs->tevStage, gs->texCoord, gs->texMap, 4);
        GXSetTevSwapMode(gs->tevStage, i, i);
        GXSetTevSwapModeTable(i, i, i, i, i);
        if (gs->tevStage == 0) {
            GXSetTevColorIn(gs->tevStage, 8, 0xF, 0xC, 0xF);
            GXSetTevColorOp(gs->tevStage, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(gs->tevStage, 7, 4, 5, 7);
            GXSetTevAlphaOp(gs->tevStage, 0, 0, 0, 1, 0);
        } else {
            GXSetTevColorIn(gs->tevStage, 8, 0xF, 0xC, 0);
            GXSetTevColorOp(gs->tevStage, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(gs->tevStage, 7, 4, 5, 0);
            GXSetTevAlphaOp(gs->tevStage, 0, 0, 0, 1, 0);
        }
        gs->tevStage++;
        gs->texMap++;
        gs->texCoord++;
    }
    if (StaFlagChk(pG, STA_SHADOW_EQCOL)) {
        cLight* l;
        GXColor k;
        u8 c;
        mng = tbl[0];
        l = mng->pLight;
        k.a = l->Col.r;
        c = l->Col.r;
        k.b = c;
        k.g = c;
        k.r = c;
        GXSetTevKColor(0, k);
        GXSetTevKColorSel(gs->tevStage, 0xC);
        GXSetTevKAlphaSel(gs->tevStage, 0x1C);
        GXSetTevDirect(gs->tevStage);
        GXSetTevOrder(gs->tevStage, 0xFF, 0xFF, 4);
        GXSetTevColorIn(gs->tevStage, 0, 0xF, 0xE, 0xF);
        GXSetTevColorOp(gs->tevStage, 0xE, 0, 0, 1, 0);
        GXSetTevAlphaIn(gs->tevStage, 0, 7, 6, 7);
        GXSetTevAlphaOp(gs->tevStage, 0xE, 0, 0, 1, 0);
        gs->tevStage++;
        gs->texMap++;
        gs->texCoord++;
    }
    if (DbgFlagChk(pG, DBG_SHADOW_LIGHT)) {
        Mtx id;
        gs->tevStage = 0;
        gs->texMap = 0;
        gs->texCoord = 0;
        PSMTXIdentity(id);
        GXLoadTexMtxImm(id, 0x1E, 1);
        GXSetTexCoordGen2(gs->texCoord, 1, 4, 0x1E, 0, 0x7D);
        GXSetTevOrder(gs->tevStage, gs->texCoord, 0xFF, 4);
        GXSetBlendMode(1, 1, 0, 0);
        GXSetCullMode(1);
        for (i = 0; i < num; i++) {
            lightMask |= 1 << i;
        }
        GXSetTevColorIn(gs->tevStage, 0xF, 0xA, 0xC, 0xF);
        GXSetTevColorOp(gs->tevStage, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(gs->tevStage, 7, 7, 7, 4);
        GXSetTevAlphaOp(gs->tevStage, 0, 0, 0, 1, 0);
        GXSetTevSwapMode(gs->tevStage, 0, 0);
        GXSetTevSwapModeTable(0, 0, 1, 2, 3);
        gs->tevStage++;
        gs->texMap++;
        gs->texCoord++;
        GXSetTevColorIn(gs->tevStage, 0xF, 0, 0xC, 0xD);
        GXSetTevColorOp(gs->tevStage, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(gs->tevStage, 7, 7, 7, 0);
        GXSetTevAlphaOp(gs->tevStage, 0, 0, 0, 1, 0);
        gs->tevStage++;
        GXSetTevColorIn(gs->tevStage, 0xF, 0, 0xD, 0xF);
        GXSetTevColorOp(gs->tevStage, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(gs->tevStage, 7, 7, 7, 0);
        GXSetTevAlphaOp(gs->tevStage, 0, 0, 0, 1, 0);
        gs->tevStage++;
    }
    GXSetNumChans(1);
    GXSetChanCtrl(0, 1, 0, 0, lightMask, 2, 1);
    GXSetChanCtrl(2, 1, 0, 0, lightMask, 2, 1);
    GXSetChanAmbColor(4, col0);
    GXSetChanMatColor(4, col255);
    GXSetNumTevStages(gs->tevStage);
    GXSetNumTexGens(gs->texCoord);
}

// Draws model `m` as a shadow receiver: model-view (pushed toward the camera by
// shadow_cammove_size) and the texture matrices of the `num` managers, then the parts.
void shadowModelTrans(cModel* m, cModelInfo* info, Mtx viewMat, ShadowMng** tbl, u32 num)
{
    static int use_shd_cammove = 1;
    Mtx mv;
    Mtx inv;
    Mtx nrm;
    u32 i;

    if (use_shd_cammove) {
        Vec look;
        Mtx tr;
        CameraGetLookVec(&pG->Camera, &look);
        PSVECScale(&look, &look, shadow_cammove_size);
        PSMTXTrans(tr, look.x, look.y, look.z);
        PSMTXConcat(tr, m->pList->mat, tr);
        PSMTXConcat(viewMat, tr, mv);
    } else {
        PSMTXConcat(viewMat, m->pList->mat, mv);
    }
    PSMTXInverse(mv, inv);
    PSMTXTranspose(inv, nrm);
    GXLoadPosMtxImm(mv, 0);
    GXLoadNrmMtxImm(nrm, 0);
    GXSetCurrentMtx(0);

    for (; info != 0; info = info->pList) {
        cModelData* d = info->model_addr;
        void* texArr = d->pTex;
        u16 nParts;
        ModelPart* part;

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
            void* clrArr = d->pClr;
            GXSetVtxAttrFmt(0, 13, 1, 3, 8);
            GXSetVtxDesc(11, 3);
            GXSetVtxAttrFmt(0, 11, 1, 5, 0);
            GXSetArray(11, clrArr, 4);
        } else {
            GXSetVtxAttrFmt(0, 13, 1, 2, 15);
        }
        GXSetArray(9, info->pPosBuf[pG->DblBufIdx], 6);
        if (d->flags & 0x20000000) {
            GXSetArray(10, info->pNrmBuf[pG->DblBufIdx], 3);
        } else {
            GXSetArray(10, info->pNrmBuf[pG->DblBufIdx], 6);
        }
        GXSetArray(13, texArr, 4);
        GXSetVtxAttrFmt(0, 9, 1, 3, d->shift);
        if ((d->weight_palette_num <= 1 && d->weight_ext_num <= 0xFF && !(info->be_flag & 2) && d->nParts == 1) || (m->be_flag & 0x4000)) {
            GXSetArray(9, d->vtxOrig, 8);
            if (d->flags & 0x20000000) {
                GXSetArray(10, d->nrmOrig, 4);
            } else {
                GXSetArray(10, d->nrmOrig, 8);
            }
        }
        GXSetCullMode(1);
        nParts = d->displist_num;
        part = d->pParts;
        for (i = 0; i < nParts; i++) {
            u8* p;
            shadowShaderSetup2(m, part, tbl, num);
            p = (u8*) part + 0x20;
            GXCallDisplayList(p, part->size);
            part = (ModelPart*) (p + part->size);
        }
    }
    if (DbgFlagChk(pG, DBG_SHADOW_LIGHT)) {
        for (i = 0; i < num; i++) {
            ShadowMng* mng = tbl[i];
            Draw_corn2(&mng->lightPos, &mng->dir, mng->pLight->Radius, mng->fov, 0xFFFFFFFF);
        }
    }
}

// Draws model `m` into a self-shadow map from the light view (position / normal matrices only).
void shadowModelTrans2(cModel* m, cModelInfo* info, Mtx viewMat)
{
    Mtx mv;
    Mtx inv;
    Mtx nrm;
    u32 i;

    PSMTXConcat(viewMat, m->pList->mat, mv);
    PSMTXInverse(mv, inv);
    PSMTXTranspose(inv, nrm);
    GXLoadPosMtxImm(mv, 0);
    GXLoadNrmMtxImm(nrm, 0);
    GXSetCurrentMtx(0);

    for (; info != 0; info = info->pList) {
        cModelData* d = info->model_addr;
        void* texArr = d->pTex;
        u16 nParts;
        ModelPart* part;

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
            void* clrArr = d->pClr;
            GXSetVtxAttrFmt(0, 13, 1, 3, 8);
            GXSetVtxDesc(11, 3);
            GXSetVtxAttrFmt(0, 11, 1, 5, 0);
            GXSetArray(11, clrArr, 4);
        } else {
            GXSetVtxAttrFmt(0, 13, 1, 2, 15);
        }
        GXSetArray(9, info->pPosBuf[pG->DblBufIdx], 6);
        GXSetArray(10, info->pNrmBuf[pG->DblBufIdx], 6);
        GXSetArray(13, texArr, 4);
        GXSetVtxAttrFmt(0, 9, 1, 3, d->shift);
        if ((d->weight_palette_num <= 1 && d->weight_ext_num <= 0xFF && !(info->be_flag & 2) && d->nParts == 1) || (m->be_flag & 0x4000)) {
            GXSetArray(9, d->vtxOrig, 8);
            if (d->flags & 0x20000000) {
                GXSetArray(10, d->nrmOrig, 4);
            } else {
                GXSetArray(10, d->nrmOrig, 8);
            }
        }
        GXSetCullMode(1);
        nParts = d->displist_num;
        part = d->pParts;
        for (i = 0; i < nParts; i++) {
            u8* p = (u8*) part + 0x20;
            GXCallDisplayList(p, part->size);
            part = (ModelPart*) (p + part->size);
        }
    }
}

// Multiplies a light-map texture (room texture or the light's own) over the rendered shadow square
// (flag1 = through the tlut, flag2 = the light's flags bit2 blend variant).
void TransLightTexture(GXTexObj* tex, GXTlutObj* tlut, s16 x, s16 y, s16 z, s16 w, s16 h, ShadowMng* mng, int flag2, int flag1)
{
    GXColor c;
    int scale;

    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
    c.r = c.g = c.b = c.a = 0xFF;
    GXSetChanAmbColor(0, c);
    GXSetChanMatColor(0, c);
    GXColor k = {0xFF, 0, 0, 0};
    k.a = k.b = k.g = 0xFF;
    Mtx m;
    Mtx44 proj;
    PSMTXIdentity(m);
    GXLoadTexObj(tex, 0);
    if (tlut) {
        GXLoadTlut(tlut, 0);
    }
    GXLoadTexMtxImm(m, 0x1E, 1);
    GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
    GXSetNumTexGens(1);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevKColor(0, k);
    GXSetTevKColorSel(0, 0xC);
    GXSetTevKAlphaSel(0, 0x1C);
    GXSetTevOp(0, 4);
    scale = 0;
    if (flag1) {
        scale = 2;
    }
    if (flag2) {
        GXSetTevColorIn(0, 0xF, 0xC, 8, 0xF);
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(0, 6, 7, 4, 7);
        GXSetTevAlphaOp(0, 0, 0, scale, 1, 0);
    } else {
        GXSetTevColorIn(0, 0xF, 0xC, 8, 0xF);
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(0, 7, 6, 4, 7);
        GXSetTevAlphaOp(0, 0, 0, scale, 1, 0);
    }
    C_MTXOrtho(proj, 0.0f, (f32) (u32) Screen.height, 0.0f, (f32) (u32) Screen.width, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 6, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(0, 7, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3s16(x, y, z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3s16(x + w, y, z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3s16(x + w, y + h, z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3s16(x, y + h, z);
    GXTexCoord2f32(0.0f, 1.0f);
}

// Draws a texture as a screen-space quad at (x, y, z) of w x h.
static void drawTexture2(GXTexObj* tex, s16 x, s16 y, s16 z, s16 w, s16 h)
{
    GXColor c;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
    c.r = c.g = c.b = c.a = 0xFF;
    GXSetChanAmbColor(0, c);
    GXSetChanMatColor(0, c);
    Mtx m;
    Mtx44 proj;
    PSMTXIdentity(m);
    GXLoadTexObj(tex, 0);
    GXLoadTexMtxImm(m, 0x1E, 1);
    GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
    GXSetNumTexGens(1);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevOp(0, 3);
    GXSetTevSwapMode(0, 0, 1);
    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 3, 3, 3, 3);
    C_MTXOrtho(proj, 0.0f, (f32) (u32) Screen.height, 0.0f, (f32) (u32) Screen.width, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 1);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3s16(x, y, z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3s16(x + w, y, z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3s16(x + w, y + h, z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3s16(x, y + h, z);
    GXTexCoord2f32(0.0f, 1.0f);
    GXSetTevSwapMode(0, 0, 0);
    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 0, 1, 2, 3);
}

// The manager of the first type 4 light that would cast `m` (light mask, frustum), building its
// matrices; 0 when none — trans.cpp projects that light's texture onto models flagged be_flag
// 0x02000000 (the "shadow cast" receive pass).
ShadowMng* GetCastShadowMngPtr(cModel* m)
{
    ShadowMng tmp;
    u32 i;
    u32 n = LightMgr.getArrayNum();

    for (i = 0; i < n; i++) {
        cLight* l = (cLight*) LightMgr.fastAt(i);
        ShadowLightWork* w;
        ShadowMng* mng;

        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (l->Type != 4) {
            continue;
        }
        w = (ShadowLightWork*) l->work;
        if (w->mode == 5) {
            continue;
        }
        if (!(l->xF & m->LightInfo.EnableMask)) {
            continue;
        }
        if (i <= 0x1F) {
            if (!((1 << i) & m->LightInfo.SelectMask)) {
                continue;
            }
        }
        if (w->mode < 1 || w->mode > 4) {
            continue;
        }
        if (l->xD != 2) {
            continue;
        }
        if (LightMgr.checkKind(l->Kind) == 0) {
            continue;
        }
        tmp.pLight = l;
        make_fix_light(&tmp);
        if (shadowChkInFrustum(&tmp, m) == 0) {
            continue;
        }
        mng = getShadowMng();
        if (mng == 0) {
            pLog->err(0, 0, "ShadowCast:too many model!!");
        } else {
            mng->pLight = l;
            make_fix_light(mng);
            mng->pModel[mng->num++] = m;
        }
        return mng;
    }
    return 0;
}
