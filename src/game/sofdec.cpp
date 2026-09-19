// game/sofdec: cSofdec, the Sofdec (CRI) movie player front end — loads the movie header, creates
// the CRI handle, converts each decoded frame into a YUV (or ARGB) texture and draws it as a
// screen quad while the game is frozen and its heap swapped out to ARAM; runs either in scheduler
// slot 1 (ThreadMove) or inline from the caller's loop. (D:/Bio4/Prog/sofdec.cpp)
#include "types.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "joy.h"
#include "fade.h"
#include "eprintf.h"
#include "dvd.h"
#include "scheduler.h"
#include "snd.h"
#include "sofdec.h"

extern "C" {
void DCFlushRangeNoSync(void* addr, u32 nBytes);
void OSReport(const char* fmt, ...);
int sprintf(char* dst, const char* fmt, ...);
char* strcpy(char* dst, const char* src);
void* memset(void* dst, int c, u32 n);
}

// CodeWarrior MSL math.h float constants, defined by the CRI headers for this compiler.
f32 __float_nan = 0.0f / 0.0f;
f32 __float_huge = 1.0f / 0.0f;

cSofdec Sofdec;

// Store through a reference (matching helper).
static inline void SetU32(u32& d, u32 v)
{
    d = v;
}

// Sofdec frame count (1/100 s units after scaling) -> h:m:s.frac.
void UsrSfcnt2time(int tscale, int count, int* h, int* m, int* s, int* f)
{
    int t = (int) ((f32) count / (f32) tscale * 100.0f);

    *h = t / 360000;
    *m = t / 6000 - *h * 60;
    *s = t / 100 - *h * 3600 - *m * 60;
    *f = t % 100;
}

// Debug overlay: the movie's play time (h:m:s:f) and frame info.
void disp_info(SofdecApp* app)
{
    MWS_FRM* frm = &app->frm;
    int count, tscale;
    int h, m, s, f;

    mwPlyGetTime(app->hn, &count, &tscale);
    UsrSfcnt2time(tscale, count, &h, &m, &s, &f);
    eprintf(0x20, 0x10, 0, 0, "%s (%3d x %3d)", app->fname, frm->width, frm->height);
    eprintf(0x196, 0x10, 0, 0, "%02d:%02d:%02d.%02d", h, m, s, f);
    eprintf(0x1C6, 0x20, 0, 0, "%5d", frm->fno);
    eprintf(0x20, 0x20, 0, 0, "DECODE SKIP : %d", mwPlyGetNumSkipDec(app->hn));
    eprintf(0x20, 0x30, 0, 0, "DISP SKIP   : %d", mwPlyGetNumSkipDisp(app->hn));
}

// Boot: initialises the CRI Sofdec player for 59.94 Hz display with the error callback.
void SofdecInit()
{
    MWS_PLY_INIT_SFD prm;

    prm.disp_cycle = 59.94f;
    prm.x04 = 1;
    prm.x08 = 1;
    prm.x0C = 1;
    ADXM_SetCbErr(ap_mwply_err_func, NULL);
    mwPlyInitSfdFx(&prm);
}

// YUV -> RGB TEV setup: stage 0/2 take the UV (IA8) map, stage 1 the Y map.
void setTevPrm(int mapY, int mapUV)
{
    union {
        GXColor c;
        u32 w;
    } kc;

    GXSetNumTexGens(2);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetTexCoordGen(1, 1, 4, 0x3C);
    GXSetNumTevStages(4);

    GXSetTevOrder(0, 0, mapUV, 0xFF);
    GXSetTevColorIn(0, 0xF, 8, 0xE, 2);
    GXSetTevColorOp(0, 0, 0, 0, 0, 0);
    GXSetTevAlphaIn(0, 7, 4, 6, 1);
    GXSetTevAlphaOp(0, 1, 0, 0, 0, 0);
    GXSetTevKColorSel(0, 0xC);
    GXSetTevKAlphaSel(0, 0x1C);
    GXSetTevSwapMode(0, 0, 1);

    GXSetTevOrder(1, 1, mapY, 0xFF);
    GXSetTevColorIn(1, 0xF, 8, 0xE, 0);
    GXSetTevColorOp(1, 0, 0, 1, 0, 0);
    GXSetTevAlphaIn(1, 7, 4, 6, 0);
    GXSetTevAlphaOp(1, 0, 0, 1, 0, 0);
    GXSetTevKColorSel(1, 0xD);
    GXSetTevKAlphaSel(1, 0x1D);
    GXSetTevSwapMode(1, 0, 0);

    GXSetTevOrder(2, 0, mapUV, 0xFF);
    GXSetTevColorIn(2, 0xF, 8, 0xE, 0);
    GXSetTevColorOp(2, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(2, 7, 4, 6, 0);
    GXSetTevAlphaOp(2, 1, 0, 0, 1, 0);
    GXSetTevKColorSel(2, 0xE);
    GXSetTevKAlphaSel(2, 0x1E);
    GXSetTevSwapMode(2, 0, 2);

    GXSetTevOrder(3, 0xFF, 0xFF, 0xFF);
    GXSetTevColorIn(3, 0, 1, 0xE, 0xF);
    GXSetTevColorOp(3, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(3, 7, 7, 7, 7);
    GXSetTevAlphaOp(3, 0, 0, 0, 1, 0);
    GXSetTevSwapMode(3, 0, 0);
    GXSetTevKColorSel(3, 0xF);

    {
        GXColorS10 c = {-111, 0, -138, 68};
        GXSetTevColorS10(1, c);
    }
    kc.w = 0x6600FF32;
    GXSetTevKColor(0, kc.c);
    kc.w = 0x94009494;
    GXSetTevKColor(1, kc.c);
    kc.w = 0xCB0005CF;
    GXSetTevKColor(2, kc.c);
    kc.w = 0x00FF0000;
    GXSetTevKColor(3, kc.c);
    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 0, 3, 3, 3);
    GXSetTevSwapModeTable(2, 0, 0, 3, 0);
    GXSetNumChans(0);
    GXSetNumIndStages(0);
}

// Restores the TEV / texgen state the YUV shader replaced.
void restoreTevPrm()
{
    GXSetZMode(1, 7, 0);
    GXSetBlendMode(0, 1, 0, 0xF);
    GXSetNumTexGens(1);
    GXSetNumChans(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 0xFF);
    GXSetTevOp(0, 3);
    GXSetTevSwapMode(0, 0, 0);
    GXSetTevSwapMode(1, 0, 0);
    GXSetTevSwapMode(2, 0, 0);
    GXSetTevSwapMode(3, 0, 0);
    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 0, 0, 0, 3);
    GXSetTevSwapModeTable(2, 1, 1, 1, 3);
    GXSetTevSwapModeTable(3, 2, 2, 2, 3);
}

// Sofdec error callback: prints the message and hangs.
void ap_mwply_err_func(void* obj, const char* msg)
{
    OSReport("%s\n", msg);
    for (;;) {
    }
}

// Draws the current frame: mode 0 the Y / UV planes through the YUV -> RGB TEV setup on a full
// screen quad, mode 1 an ARGB texture on a 3D polygon.
void cSofdec::drawTex()
{
    Mtx tm;

    switch (mode) {
    case 0:
        if (drw.tex.yuv.bufY == NULL) {
            return;
        }
        setCamera(&drw);
        GXLoadTexObj(&drw.tex.yuv.texY, 0);
        GXLoadTexObj(&drw.tex.yuv.texUV, 1);
        setTevPrm(0, 1);
        GXSetBlendMode(1, 1, 0, 0);
        PSMTXIdentity(tm);
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXClearVtxDesc();
        GXSetVtxDesc(9, 1);
        GXSetVtxDesc(0xD, 1);
        GXSetVtxAttrFmt(0, 9, 1, 3, 0);
        GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
        drawQuad(&drw);
        break;
    case 1:
        GXLoadTexObj(&drw.tex.argb.tex, 0);
        GXSetNumTevStages(1);
        GXSetBlendMode(1, 4, 5, 0);
        PSMTXIdentity(tm);
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXClearVtxDesc();
        GXSetVtxDesc(9, 1);
        GXSetVtxDesc(0xD, 1);
        GXSetVtxAttrFmt(0, 9, 1, 3, 0);
        GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
        drawPolygon(&drw);
        break;
    }
    restoreTevPrm();
}

// Screen-sized quad (texture width x height, centred) with the frame texture.
void cSofdec::drawQuad(SofdecDraw* d)
{
    Mtx tm, m;
    s16 hw = d->tex.width / 2;
    s16 hh = d->tex.height / 2 + 1;
    s16 nhw = -hw;
    s16 nhh = -hh;

    PSMTXTrans(tm, 0.0f, 0.0f, 0.0f);
    PSMTXConcat(d->mtx, tm, m);
    GXLoadPosMtxImm(m, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3s16(hw, hh, 0);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3s16(hw, nhh, 0);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3s16(nhw, nhh, 0);
    GXTexCoord2f32(0.0f, 1.0f);
    GXPosition3s16(nhw, hh, 0);
    GXTexCoord2f32(0.0f, 0.0f);
}

// The frame on a 512-unit square 800 in front of the camera (mode 1).
void cSofdec::drawPolygon(SofdecDraw* d)
{
    Mtx tm, m, sm, rx, ry, rz;

    PSMTXTrans(tm, 0.0f, 0.0f, -800.0f);
    PSMTXConcat(d->mtx, tm, m);
    PSMTXScale(sm, 512.0f, 512.0f, 512.0f);
    PSMTXConcat(m, sm, m);
    PSMTXRotRad(rx, 'X', 0.0f);
    PSMTXRotRad(ry, 'Y', 0.0f);
    PSMTXRotRad(rz, 'Z', 0.0f);
    PSMTXConcat(m, rx, m);
    PSMTXConcat(m, ry, m);
    PSMTXConcat(m, rz, m);
    GXLoadPosMtxImm(m, 0);
    GXDrawTorus(0.4f, 0x10, 0xC);
}

// Orthographic-like frustum for the framebuffer size, camera 400 back looking at the origin.
void cSofdec::setCamera(SofdecDraw* d)
{
    Mtx44 proj;
    Vec up = {0.0f, 1.0f, 0.0f};
    Vec pos = {0.0f, 0.0f, 400.0f};
    Vec target = {0.0f, 0.0f, 0.0f};
    f32 hh, hw;

    hh = (f32) (Rmode.xfbHeight / 2);
    hw = (f32) (Rmode.fbWidth / 2);
    C_MTXFrustum(proj, hh, -hh, -hw, hw, 400.0f, 3000.0f);
    GXSetProjection(proj, 0);
    C_MTXLookAt(d->mtx, &pos, &up, &target);
}

// Converts a decoded frame into the texture buffers (Y8 + UV 4:4 planes in mode 0, ARGB8888 in
// mode 1), allocating them on the first frame.
void cSofdec::loadMvFrmFx(MWPLY hn, MWS_FRM* frm)
{
    SofdecTex* tex = &drw.tex;

    switch (mode) {
    case 0:
        if (tex->yuv.bufY == NULL) {
            allocTexMem(tex, frm->width, frm->height);
        }
        mwPlyFxSetOutBufSize(hn, drw.tex.width, tex->height);
        mwPlyFxCnvFrmY84C44(hn, frm, tex->yuv.bufY, tex->yuv.bufUV);
        DCFlushRangeNoSync(tex->yuv.bufY, tex->yuv.sizeY);
        DCFlushRangeNoSync(tex->yuv.bufUV, tex->yuv.sizeUV);
        break;
    case 1:
        if (tex->argb.buf == NULL) {
            allocTexMem(tex, frm->width, frm->height);
        }
        mwPlyFxSetOutBufPitchHeight(hn, drw.tex.width * 4, tex->height);
        mwPlyFxCnvFrmARGB8888(hn, frm, tex->argb.buf);
        DCFlushRangeNoSync(tex->argb.buf, tex->argb.size);
        break;
    }
}

// Allocates the frame texture (width rounded to 32; mode 0: Y plane + half-size UV plane) and
// clears it to black.
void cSofdec::allocTexMem(SofdecTex* tex, int w, int h)
{
    switch (mode) {
    case 0: {
        u16 w2 = (w / 2 + 31) & ~31;
        u16 h2 = h / 2;

        tex->height = (u16) h;
        tex->width = (u16) (w2 * 2);
        tex->yuv.sizeY = GXGetTexBufferSize(tex->width, tex->height, 1, 0, 0);
        tex->yuv.sizeUV = GXGetTexBufferSize(w2, h2, 3, 0, 0);
#line 489 "D:/Bio4/Prog/sofdec.cpp"
        tex->yuv.bufY = MEM_ALLOC(tex->yuv.sizeY, 1, 13);
        tex->yuv.bufUV = MEM_ALLOC(tex->yuv.sizeUV, 1, 13);
        if (tex->yuv.bufY == NULL || tex->yuv.bufUV == NULL) {
            OSReport("can't allocate tex buf.\n");
            break;
        }
        clrTexMem(tex);
        GXInitTexObj(&tex->yuv.texY, tex->yuv.bufY, tex->width, tex->height, 1, 0, 0, 0);
        GXInitTexObj(&tex->yuv.texUV, tex->yuv.bufUV, w2, h2, 3, 0, 0, 0);
        break;
    }
    case 1:
        tex->height = h;
        tex->width = (w + 31) & ~31;
        tex->argb.size = GXGetTexBufferSize(tex->width, tex->height, 6, 0, 0);
#line 518 "D:/Bio4/Prog/sofdec.cpp"
        tex->argb.buf = MEM_ALLOC(tex->argb.size, 1, 13);
        if (tex->argb.buf == NULL) {
            OSReport("can't allocate tex buf.\n");
            break;
        }
        clrTexMem(tex);
        GXInitTexObj(&tex->argb.tex, tex->argb.buf, tex->width, tex->height, 6, 0, 0, 0);
        break;
    }
}

// Clears the frame texture to black (Y 0, UV 0x80 / ARGB 0).
void cSofdec::clrTexMem(SofdecTex* tex)
{
    switch (mode) {
    case 0:
        if (tex->yuv.bufY != NULL) {
            memset_asm(tex->yuv.bufY, 0, tex->yuv.sizeY);
            memset_asm(tex->yuv.bufUV, 0x80, tex->yuv.sizeUV);
        }
        break;
    case 1:
        if (tex->argb.buf != NULL) {
            memset_asm(tex->argb.buf, 0, tex->argb.size);
        }
        break;
    }
}

// Fresh draw state with the default camera.
void cSofdec::initDraw(SofdecDraw* d)
{
    memclr_asm(d, sizeof(SofdecDraw));
    setCamera(d);
}

// Reads the movie's header (first 0x5000 bytes) for its width / height and resets the app state.
void cSofdec::initApp(const char* fname)
{
    static MWS_SFD_HDRINF info;
    void* buf;
    int req;

    initDraw(&drw);
    memclr_asm(&app, sizeof(SofdecApp));
    app.disp = 1;
    app.hn = NULL;
    app.xE4 = 0;
    strcpy(app.fname, fname);
#line 615 "D:/Bio4/Prog/sofdec.cpp"
    buf = MEM_ALLOC(0x5000, 1, 13);
    req = DvdReadN(fname, buf, 0, 0, 0x5000, 0x11, __FILE__, __LINE__);
    Dvd.ReadCheck(req, NULL, NULL, NULL);
    mwPlyGetHdrInf(buf, 0x5000, &info);
    Mem_free(buf);
    width = info.width;
    m_height = info.height;
}

// Creates the Sofdec handle (work buffer sized for the movie, 8 Mbps, 4 frame pool, 2 streams)
// and starts playback of the file. Returns 0 on failure.
int cSofdec::startApp()
{
    MWS_PLY_CPRM_SFD* cprm = &app.cprm;
    MWPLY hn;

    cprm->compo_mode = 0;
    cprm->ftype = 1;
    cprm->max_bps = 8000000;
    cprm->nfrm_pool_wk = 4;
    cprm->max_width = width;
    cprm->max_height = m_height;
    cprm->max_stm = 2;
    cprm->wksize = mwPlyCalcWorkCprmSfd(cprm);
#line 649 "D:/Bio4/Prog/sofdec.cpp"
    cprm->work = MEM_ALLOC(cprm->wksize, 1, 13);
    if (cprm->work == NULL) {
        ap_mwply_err_func(NULL, "Can't Malloc.");
        return 0;
    }
    app.work = cprm->work;
    hn = mwPlyCreateSofdec(cprm);
    if (hn == NULL) {
        Mem_free(app.work);
        ap_mwply_err_func(NULL, "Can't Create Handle.");
        return 0;
    }
    app.hn = hn;
    mwPlyStartFname(hn, app.fname);
    return 1;
}

// Playback start: screen black, VI sync every frame, the screen resized to 512 wide, texture
// cleared; fadeIn = show the first frame when it arrives.
void cSofdec::initSync()
{
    systemVISetBlack(1);
    fadeIn = 1;
    m_vcnt_save = GetSystemVcnt();
    SetSystemVcnt(1);
    if (Screen.width != 512.0f) {
        resized = 1;
        ScreenReSize(0x200, 0x1C0);
    }
    clrTexMem(&drw.tex);
    OSReport("Movie Play : %s \n", app.fname);
}

// One frame of playback: START / a button skips (m_be_flag 0x20), the CRI main tick, the newest
// decoded frame converted; returns 0 when the movie ended / failed / was skipped.
int cSofdec::appMain()
{
    MWS_FRM frm;
    int stat;

    if (Joy[0].trg & 0x1200) {
        m_be_flag |= 0x20;
        return 0;
    }
    ADXM_ExecMain();
    mwPlyGetCurFrm(app.hn, &frm);
    if (frm.bufadr != NULL) {
        loadMvFrmFx(app.hn, &frm);
        app.frm = frm;
        mwPlyRelCurFrm(app.hn);
    }
    stat = mwPlyGetStat(app.hn);
    if (stat == MWE_PLY_STAT_PLAYEND || stat == MWE_PLY_STAT_ERROR) {
        return 0;
    }
    app.stat = stat;
    return 1;
}

// Draws the frame once playback has started (status > 1), lifting the black screen on the first.
void cSofdec::draw()
{
    if (app.stat > 1) {
        drawTex();
        if (fadeIn == 1) {
            FadeKill(0);
            systemVISetBlack(0);
            fadeIn = 0;
        }
        fno = ((u16*) &app.frm.fno)[1];
        if (app.disp == 1) {
            disp_info(&app);
        }
    }
}

// Playback end: destroys the handle and buffers, restores the screen size, Disp_flg / Stop_flg /
// VI count, swaps the game heap back in from ARAM (unless Status_flg[2] 0x8000 kept it), clears
// the movie flags (Status_flg[0] 0x10000000, System_flg 0x00100000, m_be_flag bit0).
void cSofdec::finishMovie()
{
    mwPlyDestroy(app.hn);
    app.hn = NULL;
    Mem_free(app.work);
    if (drw.tex.yuv.bufY != NULL) {
        Mem_free(drw.tex.yuv.bufY);
        Mem_free(drw.tex.yuv.bufUV);
        drw.tex.yuv.bufY = NULL;
        drw.tex.yuv.bufUV = NULL;
    }
    systemVISetBlack(1);
    if (resized != 0) {
        ScreenReSize(0x280, 0x1C0);
    }
    SetU32(pG->Disp_flg, m_disp_flg_bak);
    SetU32(pG->Stop_flg, save170);
    SetSystemVcnt(m_vcnt_save);
    StaFlagOff(pG, STA_MOVIE_ON);
    if (!StaFlagChk(pG, STA_TITLE)) {
        MemDestroyHeap(11);
        Aram.DmaTransReq(1, 0x740000, heapStart, 0x500000, 1);
        MemSignalHeap(m_save_cur_heap);
        MemSetCurrentHeap(m_save_cur_heap);
    }
    SysFlagOff(pG, SYS_TRANS_STOP);
    if (!chkFlag(0x100)) {
        systemVISetBlack(0);
    }
    m_be_flag &= ~1;
}

// Prepares the movie: falls back to "movie/dmy.sfd" when the file is missing, freezes and hides
// the game (Stop_flg / Disp_flg all set), swaps the current heap out to ARAM and creates a 5 MB
// movie heap (unless Status_flg[2] 0x8000); System_flg 0x00100000 = movie mode. 0 when no file.
int cSofdec::initWork(const char* fname)
{
    if (Dvd.FileExistCheck(fname, NULL) == -1) {
        OSReport("File not found : %s\n", fname);
        sprintf(path, "movie/dmy.sfd");
        if (Dvd.FileExistCheck(path, NULL) == -1) {
            return 0;
        }
    }
    SetU32(save170, pG->Stop_flg);
    SetU32(pG->Stop_flg, 0xFFFFFFFF);
    SetU32(m_disp_flg_bak, pG->Disp_flg);
    SetU32(pG->Disp_flg, 0xFFFFFFFF);
    if (!StaFlagChk(pG, STA_TITLE)) {
        m_save_cur_heap = MemGetCurrentHeap();
        heapStart = MemGetHeapStartAddr(m_save_cur_heap);
        Aram.DmaTransReq(0, heapStart, 0x740000, 0x500000, 1);
        MemSuspendHeap(m_save_cur_heap);
        MemCreateHeap(11, heapStart, heapStart + 0x500000);
        MemSetCurrentHeap(11);
    }
    SysFlagOn(pG, SYS_TRANS_STOP);
    return 1;
}

// Starts movie `fname` (see initSub).
int cSofdec::Initialize(const char* fname, u32 flags)
{
    return initSub(fname, flags);
}

// Starts movie `fname` (see initSub).
int cSofdec::Initialize(cString& fname, u32 flags)
{
    return initSub(fname.c_str(), flags);
}

// Starts a movie unless one plays: flags 0x200 = run it inline from the caller's loop (Move), else
// in scheduler slot 1 (ThreadMove) with slot 0 suspended; all sounds stopped. Returns 1 if started.
int cSofdec::initSub(const char* fname, u32 flags)
{
    if (StaFlagChk(pG, STA_MOVIE_ON)) {
        return 0;
    }
    sprintf(path, "%s", fname);
    if (!(flags & 0x200)) {
        TaskSuspend(0);
        TaskExec(1, (TaskFunc) ThreadMove, (int) this);
    } else {
        if (!initWork(fname)) {
            return 0;
        }
        initApp(path);
        startApp();
        StaFlagOn(pG, STA_MOVIE_ON);
        initSync();
        m_be_flag = flags | 1;
    }
    SndAllStop();
    return 1;
}

// One playback frame (decode + draw); returns 1 when the movie has finished (resources released).
int cSofdec::Move()
{
    int ret = 0;
    int r = appMain();

    draw();
    if (r == 0) {
        ret = 1;
        finishMovie();
    }
    return ret;
}

// Task body: plays the movie to its end, then resumes slot 0 and exits.
void cSofdec::ThreadMove(cSofdec* s)
{
    int r = s->initWork(s->path);

    if (r == 1) {
        s->initApp(s->path);
        s->startApp();
        StaFlagOn(pG, STA_MOVIE_ON);
        s->initSync();
        s->m_be_flag = 1;
        while (s->Move() == 0) {
            TaskSleep(1);
        }
    }
    TaskSignal(0);
    TaskExit();
}

// Pauses / resumes playback (m_be_flag bit2).
void cSofdec::PlayPause(int pause)
{
    if (pause == 1) {
        m_be_flag |= 4;
    } else {
        m_be_flag &= ~4;
    }
    mwPlyPause(app.hn, pause);
}
