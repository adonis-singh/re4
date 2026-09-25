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
#include "trans.h"

// game/model.cpp
cModelInfo* GetModelInfoAddr(cModelInfo* info, int no);
void ModelInfoRefrectOffAll(cModel* m);
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
int GetTexRenderMgr(TexRenderMng** ppMgr)
{
    if (g_RndMgrNum == 8) {
        pLog->err(0, 0, "GetTexRenderMgr() : Manager full!!");
        return 0;
    }
    *ppMgr = &g_RndMgr[g_RndMgrNum];
    (*ppMgr)->Init();
    if (!(*ppMgr)->AllocBuf()) {
        return 0;
    }
    (*ppMgr)->SetTexNo((u8) g_RndMgrNum + 0xF8);
    (*ppMgr)->SetCoreFlg(8 << g_RndMgrNum);
    g_RndMgrNum++;
    (*ppMgr)->SetAlive(1);
    return 1;
}

// EFB x offset that centres a 2x copy of the texture in the resized frame.
void RenderTexRenderMgr(TexRenderMng* m)
{
    u32 ofs, w, h;

    if (m->GetWSize() == 0xE0) {
        ofs = (u32) ((f32) m->GetHSize() * 2.0f / 0.875f - (f32) m->GetWSize() * 2.0f);
    } else {
        ofs = (m->GetWSize() >> 2) + (m->GetWSize() >> 4);
    }
    w = m->GetWSize() * 2 + ofs;
    h = m->GetHSize() * 2;

    if (w > 0x280) {
        pLog->err(0, 0, "RenderTexRenderMgr:: Invalid SX[%d]", w);
        w = 0x280;
    }
    if (h > 0x210) {
        pLog->err(0, 0, "RenderTexRenderMgr:: Invalid SY[%d]", h);
        h = 0x210;
    }
    EFBReSize(w, h);
    GXSetScissor(ofs >> 1, 0, m->GetWSize() << 1, m->GetHSize() << 1);
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
        if (m->GetWSize() == 0xE0) {
            ofs = (u32) ((f32) m->GetHSize() * 2.0f / 0.875f - (f32) m->GetWSize() * 2.0f);
        } else {
            ofs = (m->GetWSize() >> 2) + (m->GetWSize() >> 4);
        }
        // The original reloads m->sx and m->sy here in both paths: a memory kill at the top of the
        // join block makes neither load anticipatable, so gcse does not PRE the if-arm's m->sy
        // load into the else arm (an empty asm keeps the two conversion paths' jumps on the join).
        asm volatile("" : : : "memory");
        w = m->GetWSize() * 2;
        h = m->GetHSize() * 2;
        if (w > 0x280) {
            pLog->err(0, 0, "CopyTexRenderMgr:: Invalid SX[%d]", w);
            w = 0x280;
        }
        if (h > 0x210) {
            pLog->err(0, 0, "CopyTexRenderMgr:: Invalid SY[%d]", h);
            h = 0x210;
        }
        GXSetTexCopySrc(ofs >> 1, 0, w, h);
        GXSetTexCopyDst(m->GetWSize(), m->GetHSize(), 6, 1);
        GXCopyTex(m->GetBufAddr(), 1);
        GXSetAlphaUpdate(0);
        GXSetCopyFilter(rmode->aa, rmode->sample_pattern, 1, rmode->vfilter);
        GXPixModeSync();
        GXInvalidateTexAll();
        switch (m->GetRepeatType()) {
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
            pLog->err(0, 0, "TexRenderMng:: Invalid REPTYPE[%d]", m->GetRepeatType());
            wrap = 2;
            break;
        }
        GXInitTexObj(m->GetTexObj(), m->GetBufAddr(), m->GetWSize(), m->GetHSize(), 6, wrap, wrap, 0);
        GXInitTexObjLOD(m->GetTexObj(), 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
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
        if (g_RndMgr[i].IsAlive() != 0) {
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
    m_Be_flg = 0;
    m_Texture_buffer = NULL;
    m_Tex_no = 0;
    pad = 0;
    m_Core_flg = 0;
    m_W_size = 0x80;
    m_H_size = 0x80;
    m_Rep_type = 0;
}

// Allocates the target's RGBA8 buffer (w x h x 4); 0 with an error when memory is short.
int TexRenderMng::AllocBuf()
{
#line 323 "D:/Bio4/Prog/TexRender.cpp"
    m_Texture_buffer = MEM_ALLOC(GetTexBufSize(), 1, 13);
    if (m_Texture_buffer == NULL) {
        pLog->err(0, 0, "TexRenderMng::AllocBuf() : not enough memory");
        return 0;
    }
    return 1;
}

// Frees and re-allocates the buffer after a size change.
void TexRenderMng::ReAllocBuf()
{
    if (m_Texture_buffer != NULL) {
        Mem_free(m_Texture_buffer);
    }
    AllocBuf();
}

// Claims a target of `size` x `size` (0 = default 128) with wrap mode repType and (re)allocates
// its buffer.
void TexRenderInit(TexRenderMng** ppMgr, int size, int repType)
{
    if (!GetTexRenderMgr(ppMgr)) {
        pLog->err(0, 0, "TexRenderInit() : Manager alloc failed!!");
    }
    if (size != 0) {
        TexRenderMng* m = *ppMgr;
        m->SetWHSize(size, size);
        (*ppMgr)->ReAllocBuf();
    }
    (*ppMgr)->SetRepeatType(repType);
}

// Makes parts `parts` of model `m` show the render target: installs `tbl` as the parts' texture
// blend table with the target's texture id, blend ratio 0xFF (blend type 1 unless kept), and
// turns the reflection mapping on for that parts only (unless kept); alpha into the model.
void TexRenderModSet(cModel* pMod, int modelInfoNo, u8* pBlendTbl, TexRenderMng* pMgr, int alphaFlag, int refractFlag, int blendAddFlag, int zModeFlag, f32 invisibleFactor)
{
    cModelInfo* info;

    if (pMgr == NULL) {
        pLog->err(0, 0, "TexRenderModSet() : Manager alloc failed!!");
        return;
    }
    if (pMod == NULL) {
        pLog->err(0, 0, "TexRenderModSet() : failed!!");
        return;
    }
    pBlendTbl[0] = 1;
    pBlendTbl[1] = 0;
    pBlendTbl[4] = 0xF7;
    pBlendTbl[5] = pMgr->GetTexNo();
    info = GetModelInfoAddr(pMod->pModelInfo, modelInfoNo);
    if (info != NULL) {
        info->be_flag |= 8;
        info->setTexBlendTbl(pBlendTbl);
        info->setBlendRatio(0xFF);
        if (alphaFlag == 0) {
            info->setBlendType(1);
        }
        if (blendAddFlag == 0) {
            info->blend_mode = 1;
        }
    }
    if (refractFlag == 0) {
        pMod->Shader_type = 2;
        pMod->Refract_pow = 0x10;
        pMod->Refract_ratio = 0x90;
        ModelInfoRefrectOffAll(pMod);
        ModelInfoRefrectOn(pMod, modelInfoNo);
    }
    if (zModeFlag == 0) {
        pMod->z_mode = 2;
    }
    pMod->invisible_factor = invisibleFactor;
}

// Undoes TexRenderModSet on every parts of the model (blend ratio 0, default blend table).
void TexRenderModRes(cModel* pMod, u32 modelInfoNo)
{
    cModelInfo* info;

    if (pMod == NULL) {
        pLog->err(0, 0, "TexRenderModRes() : failed!!");
        return;
    }
    info = GetModelInfoAddr(pMod->pModelInfo, modelInfoNo);
    if (info != NULL) {
        info->be_flag &= ~8;
        info->setBlendRatio(0);
        info->resetTexBlendTbl();
    }
    pMod->Shader_type = 0;
    pMod->Refract_pow = 0x10;
    pMod->Refract_ratio = 0x90;
}

// Queues the model's normal render (ModelRender) into render target OT `ot` (the model drawn
// into the texture).
void TexRenderModAddOt(int otType, cModel* pMod)
{
    StaFlagOn(pG, STA_TEX_RENDER);
    if (pMod == NULL) {
        pLog->err(0, 0, "TexRenderModSet() : failed!!");
        return;
    }
    if (commonScreenMat(pMod)) {
        lightSetEm(pMod);
        AddOtDirect(otType, pMod, (void (*)()) ModelRender, 3, 1, NULL, 0.0f);
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
// flag 0x40000000, the last frame with 0x08000000), saves pG->Camera and installs the motion camera
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
    pWk->save = pG->Camera;
    pG->Camera = *pWk->pCam;
    C_MTXPerspective(pG->Camera.ProjMat, pG->Camera.param.fovy, 4.0f / 3.0f, ((F32S*) &ZNEAR)->v, ((F32S*) &ZFAR)->v);
    C_MTXLookAt(pG->Camera.v_mat, &pG->Camera.param.pos, &pG->Camera.Up, &pG->Camera.param.at);
}

// After the passes: restores pG->Camera.
void CamRenderAfter(TexRenderCam* pWk)
{
    pG->Camera = pWk->save;
}
