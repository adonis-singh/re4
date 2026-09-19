// game/TexRender.cpp: render-to-texture. Up to 8 TexRenderMng targets per frame: each owns an
// EFB copy buffer, an effect texture id (0xF8 + slot) and an OT the callers queue their draws
// into; TransTexRenderMgr schedules the copy after each target's pass. Models show a target
// through their texture blend table (TexRenderModSet), and TexRenderCam* render a pass from an
// event camera motion.
#include "types.h"
#include "global.h"
#include "event.h"
#include "db_log.h"
#include "main_mem.h"
#include "main_sub.h"
#include "view.h"
#include "model.h"
#include "trans_ot.h"
#include "TexRender.h"

// game/model.cpp
cModelInfo* GetModelInfoAddr(cModelInfo* info, int no);
void ModelInfoRefrectOffAll(cModel* m);
void ModelInfoRefrectOn(cModel* m, int no);
// game/trans.cpp
int commonScreenMat(cModel* m);
void lightSetEm(cModel* m);
void ModelRender(cModel* m);
// game/mirror.cpp
void MirrorDraw2(cModel* m);

TexRenderMng g_RndMgr[8];
u32 g_RndMgrNum;
static int g_draw = 0;
int g_TexUse = 0;

// Render target `no` (0..7).
TexRenderMng* GetTexRenderMgrAddr(int no)
{
    return &g_RndMgr[no];
}

// Boot: clears the 8 render targets.
void TexRenderMgrInit()
{
    u32 i;

    g_RndMgrNum = 0;
    for (i = 0; i < 8; i++) {
        g_RndMgr[i].Init();
    }
    g_draw = 0;
}

// Room start: clears the 8 render targets (their buffers belong to the room heap).
void TexRenderMgrRoomInit()
{
    u32 i;

    g_RndMgrNum = 0;
    for (i = 0; i < 8; i++) {
        g_RndMgr[i].Init();
    }
    g_draw = 0;
}

// Claims the next free render target: allocates its buffer, assigns its effect texture id
// (0xF8 + slot, the ids the esp Tool_flg 0x10000 effects draw into) and its OT mask bit
// (8 << slot). 0 with an error when all 8 are used or memory is short.
int GetTexRenderMgr(TexRenderMng** out)
{
    if (g_RndMgrNum == 8) {
        pLog->err(0, 0, "GetTexRenderMgr() : Manager full!!");
        return 0;
    }
    *out = &g_RndMgr[g_RndMgrNum];
    (*out)->Init();
    if (!(*out)->AllocBuf()) {
        return 0;
    }
    (*out)->texId = (u8) g_RndMgrNum + 0xF8;
    {
        // the original stores the mask through a u16 reference (the load of g_RndMgrNum below is
        // not hoisted above a plain member store)
        u16& mask = (*out)->mask;
        mask = 8 << g_RndMgrNum;
    }
    g_RndMgrNum++;
    (*out)->used = 1;
    return 1;
}

// EFB x offset that centres a 2x copy of the texture in the resized frame.
void RenderTexRenderMgr(TexRenderMng* m)
{
    u32 ofs, w, h;

    if (m->m_W_size == 0xE0) {
        ofs = (u32) ((f32) m->m_H_size * 2.0f / 0.875f - (f32) m->m_W_size * 2.0f);
    } else {
        ofs = (m->m_W_size >> 2) + (m->m_W_size >> 4);
    }
    w = m->m_W_size * 2 + ofs;
    h = m->m_H_size * 2;

    if (w > 0x280) {
        pLog->err(0, 0, "RenderTexRenderMgr:: Invalid SX[%d]", w);
        w = 0x280;
    }
    if (h > 0x210) {
        pLog->err(0, 0, "RenderTexRenderMgr:: Invalid SY[%d]", h);
        h = 0x210;
    }
    EFBReSize(w, h);
    GXSetScissor(ofs >> 1, 0, m->m_W_size << 1, m->m_H_size << 1);
}

// OT callback (slot's OT, after its render pass): copies the EFB into the target's buffer at
// half size, initialises the texture object with the wrap mode (0 mirror, 1 repeat, 2 clamp)
// and restores the screen size.
void CopyTexRenderMgr(TexRenderMng* m)
{
    static u8 vfilter[7] __attribute__((aligned(32))) = {32, 0, 0, 0, 0, 0, 32};

    if (StaFlagChk(pG, STA_TEX_RENDER)) {
        GXRenderModeObj* rmode = &Rmode;
        u32 ofs, w, h;
        int wrap;

        GXSetCopyFilter(0, rmode->sample_pattern, 0, vfilter);
        GXSetAlphaUpdate(1);
        if (m->m_W_size == 0xE0) {
            ofs = (u32) ((f32) m->m_H_size * 2.0f / 0.875f - (f32) m->m_W_size * 2.0f);
        } else {
            ofs = (m->m_W_size >> 2) + (m->m_W_size >> 4);
        }
        // The original reloads m->sx and m->sy here in both paths: a memory kill at the top of the
        // join block makes neither load anticipatable, so gcse does not PRE the if-arm's m->sy
        // load into the else arm (an empty asm keeps the two conversion paths' jumps on the join).
        asm volatile("" : : : "memory");
        w = m->m_W_size * 2;
        h = m->m_H_size * 2;
        if (w > 0x280) {
            pLog->err(0, 0, "CopyTexRenderMgr:: Invalid SX[%d]", w);
            w = 0x280;
        }
        if (h > 0x210) {
            pLog->err(0, 0, "CopyTexRenderMgr:: Invalid SY[%d]", h);
            h = 0x210;
        }
        GXSetTexCopySrc(ofs >> 1, 0, w, h);
        GXSetTexCopyDst(m->m_W_size, m->m_H_size, 6, 1);
        GXCopyTex(m->buf, 1);
        GXSetAlphaUpdate(0);
        GXSetCopyFilter(rmode->aa, rmode->sample_pattern, 1, rmode->vfilter);
        GXPixModeSync();
        GXInvalidateTexAll();
        switch (m->m_Rep_type) {
        case 1:
            wrap = 1;
            break;
        case 2:
            wrap = 0;
            break;
        case 0:
            wrap = 2;
            break;
        default:   // its own `wrap = 2` (cross-jumped into case 0): with a fallthrough the err block's
                   // string `lis` gains an anti-dependence on the call and is scheduled before `lwz pLog`
            pLog->err(0, 0, "TexRenderMng:: Invalid REPTYPE[%d]", m->m_Rep_type);
            wrap = 2;
            break;
        }
        GXInitTexObj(&m->m_Tex_obj, m->buf, m->m_W_size, m->m_H_size, 6, wrap, wrap, 0);
        GXInitTexObjLOD(&m->m_Tex_obj, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
        g_draw = 1;
    }
    if (m == &g_RndMgr[g_RndMgrNum - 1]) {
        StaFlagOff(pG, STA_TEX_RENDER);
        ScreenReSize(0x200, 0x1C0);
        SetScissorState();
    }
}

// Draw registration: for every used target queues its clear / render pass and the EFB copy
// (CopyTexRenderMgr) in the target's own OT, so the textures are ready before the world pass.
void TransTexRenderMgr()
{
    u32 i;

    for (i = 0; i < (u32) g_RndMgrNum; i++) {
        if (g_RndMgr[i].used != 0) {
            AddOtDirect((u16) i, &g_RndMgr[i], (void (*)()) RenderTexRenderMgr, 9, 0x800, NULL, 0.0f);
            AddOtDirect((u16) i, &g_RndMgr[i], (void (*)()) CopyTexRenderMgr, 0, 0x800, NULL, 0.0f);
        }
    }
    {
        u32 use = DbgFlagChk(pG, DBG_TEX_RENDER_ALL);
        if (use) {
            use = 1;
        }
        g_TexUse = use;
    }
}

// A cleared, unused target.
TexRenderMng::TexRenderMng()
{
    Init();
}

// Resets the target: unused, no buffer, 128 x 128, mirror wrap.
void TexRenderMng::Init()
{
    used = 0;
    buf = NULL;
    texId = 0;
    x29 = 0;
    mask = 0;
    m_W_size = 0x80;
    m_H_size = 0x80;
    m_Rep_type = 0;
}

// Allocates the target's RGBA8 buffer (w x h x 4); 0 with an error when memory is short.
int TexRenderMng::AllocBuf()
{
#line 323 "D:/Bio4/Prog/TexRender.cpp"
    buf = MEM_ALLOC(m_W_size * m_H_size * 4, 1, 13);
    if (buf == NULL) {
        pLog->err(0, 0, "TexRenderMng::AllocBuf() : not enough memory");
        return 0;
    }
    return 1;
}

// Frees and re-allocates the buffer after a size change.
void TexRenderMng::ReAllocBuf()
{
    if (buf != NULL) {
        Mem_free(buf);
    }
    AllocBuf();
}

// Claims a target of `size` x `size` (0 = default 128) with wrap mode repType and (re)allocates
// its buffer.
void TexRenderInit(TexRenderMng** out, int size, int repType)
{
    if (!GetTexRenderMgr(out)) {
        pLog->err(0, 0, "TexRenderInit() : Manager alloc failed!!");
    }
    if (size != 0) {
        TexRenderMng* m = *out;
        m->m_W_size = size;
        m->m_H_size = size;
        (*out)->ReAllocBuf();
    }
    (*out)->m_Rep_type = repType;
}

// Makes parts `parts` of model `m` show the render target: installs `tbl` as the parts' texture
// blend table with the target's texture id, blend ratio 0xFF (blend type 1 unless kept), and
// turns the reflection mapping on for that parts only (unless kept); alpha into the model.
void TexRenderModSet(cModel* m, int parts, u8* tbl, TexRenderMng* mgr, int keepBlendType, int keepRefrect, int keepD6, int keep12C, f32 alpha)
{
    cModelInfo* info;

    if (mgr == NULL) {
        pLog->err(0, 0, "TexRenderModSet() : Manager alloc failed!!");
        return;
    }
    if (m == NULL) {
        pLog->err(0, 0, "TexRenderModSet() : failed!!");
        return;
    }
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = mgr->texId;
    info = GetModelInfoAddr(m->pModelInfo, parts);
    if (info != NULL) {
        info->be_flag |= 8;
        info->setTexBlendTbl(tbl);
        info->setBlendRatio(0xFF);
        if (keepBlendType == 0) {
            info->setBlendType(1);
        }
        if (keepD6 == 0) {
            info->blend_mode = 1;
        }
    }
    if (keepRefrect == 0) {
        m->Shader_type = 2;
        m->Refract_pow = 0x10;
        m->Refract_ratio = 0x90;
        ModelInfoRefrectOffAll(m);
        ModelInfoRefrectOn(m, parts);
    }
    if (keep12C == 0) {
        m->z_mode = 2;
    }
    m->invisible_factor = alpha;
}

// Undoes TexRenderModSet on every parts of the model (blend ratio 0, default blend table).
void TexRenderModRes(cModel* m)
{
    cModelInfo* info;
    int parts;  // never set in the original (GetModelInfoAddr gets whatever is in r4)

    if (m == NULL) {
        pLog->err(0, 0, "TexRenderModRes() : failed!!");
        return;
    }
    info = GetModelInfoAddr(m->pModelInfo, parts);
    if (info != NULL) {
        info->be_flag &= ~8;
        info->setBlendRatio(0);
        info->resetTexBlendTbl();
    }
    m->Shader_type = 0;
    m->Refract_pow = 0x10;
    m->Refract_ratio = 0x90;
}

// Queues the model's normal render (ModelRender) into render target OT `ot` (the model drawn
// into the texture).
void TexRenderModAddOt(int ot, cModel* m)
{
    StaFlagOn(pG, STA_TEX_RENDER);
    if (m == NULL) {
        pLog->err(0, 0, "TexRenderModSet() : failed!!");
        return;
    }
    if (commonScreenMat(m)) {
        lightSetEm(m);
        AddOtDirect(ot, m, (void (*)()) ModelRender, 3, 1, NULL, 0.0f);
    }
}

// Queues the model's mirror render (MirrorDraw2) for the texture.
void TexRenderModAddOtMirror(int ot, cModel* m)
{
    StaFlagOn(pG, STA_TEX_RENDER);
    if (m == NULL) {
        pLog->err(0, 0, "TexRenderModSet() : failed!!");
        return;
    }
    if (commonScreenMat(m)) {
        AddOtDirect(0x10, m, (void (*)()) MirrorDraw2, 0, 0x400, NULL, 0.0f);
    }
    m->be_flag |= 8;
    {
        u8 c = 0xFF;
        m->AddAmb_b = m->AddAmb_g = m->AddAmb_r = c;
    }
    m->invisible_factor = 0.4f;
}

// Queues a camera swap around render target OT `ot`: CamRenderPrev before its passes and
// CamRenderAfter after them, using the event's camera motion `data`.
void TexRenderCamAddOt(int ot, TexRenderCam* pWk, TexRenderEvt* evt, void* data)
{
    pWk->pEvt = evt;
    pWk->data = data;
    AddOtDirect(ot, pWk, (void (*)()) CamRenderPrev, 4, 1, NULL, 0.0f);
    AddOtDirect(ot, pWk, (void (*)()) CamRenderAfter, 2, 1, NULL, 0.0f);
}

struct F32S {
    f32 v;
};

// Event flag test.
static inline bool evtFlag(TexRenderEvt* e, u32 bit)
{
    return (e->flags & bit) != 0;
}

// Before the render-to-texture passes: builds a CameraMotion at the event's frame (frameB with
// flag 0x40000000, the last frame with 0x08000000), saves pG->Cam and installs the motion camera
// with its projection / view matrices.
void CamRenderPrev(TexRenderCam* pWk)
{
    TexRenderEvt* e = pWk->pEvt;
    int frame = e->frame;

    if (evtFlag(e, 0x40000000)) {
        frame = e->frameB;
    }
    if (evtFlag(e, 0x08000000)) {
        frame = e->frameEnd - 1;
    }
    pWk->pCam = new (&pWk->cam) CameraMotion(pWk->data, 0, 0, (f32) frame);
    pWk->pCam->move();
    pWk->save = pG->Cam;
    pGS->Cam = *pWk->pCam;
    C_MTXPerspective(pGS->Cam.ProjMat, pGS->Cam.param.fovy, 4.0f / 3.0f, ((F32S*) &ZNEAR)->v, ((F32S*) &ZFAR)->v);
    C_MTXLookAt(pG->Cam.v_mat, &pG->Cam.param.pos, &pG->Cam.up, &pG->Cam.param.at);
}

// After the passes: restores pG->Cam.
void CamRenderAfter(TexRenderCam* pWk)
{
    pG->Cam = pWk->save;
}
