// game/Espgen43.cpp: effect controller 43, a deformable sand / mud surface. An nx x ny height
// grid (Vec per point) rendered as lit triangle strips through a display list; AddSandPower
// dents the surface at a point (footprints, impacts) with three widening rings, GetSandHeight
// reports the surface height. Status_flg[0] bit1 marks that a sand surface exists this frame.

#include "light.h"
#include "atari.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "rnd.h"
#include "camera.h"
#include "os_vi.h"
#include "db_log.h"

// Byte-identical. AddSandPower's `stfs f1, Add_power` is an asm with a hard-register anti-dependence
// (see the COMPILER-DIFF note there); everything else is plain C.
// Effect controller 43: sand surface. A (nx+1) x (ny+1) height grid drawn as triangle strips
// through a prebuilt display list; AddSandPower pushes the grid down around a world position
// and GetSandHeight samples it (obj09).
struct Espgen43Work {
    Mtx Wld_mat;           // 0x14 grid -> world
    Mtx Inv_mat;           // 0x44 world -> grid
    u16 Width;            // 0x74 grid cells along x
    u16 ny;            // 0x76 grid cells along z
    u8 pad_78[4];
    f32 Size;          // 0x7C cell size
    Vec* pNorBuf;          // 0x80
    Vec* pHeightBuf;          // 0x84
    u8* pDisplayList;            // 0x88 display list
    u32 Dpl_size;        // 0x8C
    GXColor Color;     // 0x90
    GXColor Amb;    // 0x94
    u8 TexNo;          // 0x98
    u8 texRep;         // 0x99 texture repeats across the grid
};

extern "C" {
// game/trans_lit.cpp
void commonClothLightSet(cLight** list, int n, Vec pos, f32 radius);
// game/espgen.cpp
int EspgenApplyFunc(void (*func)(EspgenWork* w));

void AddSandPowerSub(EspgenWork* w);
void GetSandHeightSub(EspgenWork* w);
void Espgen43_Move00(EspgenWork* w);
void Espgen43_TransSub(EspgenWork* w);
EspgenWork* SetSandWork(EspgenWork* w, Vec* pos, Vec* rot, f32 size, f32 sizeRate, u32 nx, u32 ny);
}

static Vec Chk_pos;
static f32 Height_ret;
static f32 Add_power;
static int Height_find;
static inline void ISet(int& d, int v) { d = v; }
static inline f32 FGet(f32& d) { return d; }

// Applies Add_power at Chk_pos to sand generator `w`: raises the hit point and lowers rings of
// radius 3 / 2 / 1 around it by 2% / 10% / 30% of the power, then smooths the grid.
void AddSandPowerSub(EspgenWork* w)
{
    Espgen43Work* p;
    Vec v;
    u32 x;
    u32 z;
    int idx;
    int total;
    int stride;
    int i;
    int j;
    int k;

    if (w->id != 0x43) {
        return;
    }
    p = (Espgen43Work*) w->work;
    v = Chk_pos;
    PSMTXMultVec(p->Inv_mat, &v, &v);
    if (v.x < (f32) (-p->Width / 2)) {
        return;
    }
    if (v.z < (f32) (-p->ny / 2)) {
        return;
    }
    if (v.x > (f32) (p->Width / 2)) {
        return;
    }
    if (v.z > (f32) (p->ny / 2)) {
        return;
    }
    z = (u32) (v.z + (f32) (p->ny / 2));
    x = (u32) (v.x + (f32) (p->Width / 2));
    idx = z * (p->Width + 1) + x;
    p->pHeightBuf[idx].y += FGet(Add_power);
    total = (p->ny + 1) * (p->Width + 1);
    stride = p->Width + 1;
    for (i = -3; i <= 3; i++) {
        for (j = -3; j <= 3; j++) {
            k = idx + i + j * (p->Width + 1);
            if (k <= total && k >= stride) {
                p->pHeightBuf[k].y -= FGet(Add_power) * 0.02f;
            }
        }
    }
    for (i = -2; i <= 2; i++) {
        for (j = -2; j <= 2; j++) {
            k = idx + i + j * (p->Width + 1);
            if (k <= total && k >= stride) {
                p->pHeightBuf[k].y -= FGet(Add_power) * 0.1f;
            }
        }
    }
    for (i = -1; i <= 1; i++) {
        for (j = -1; j <= 1; j++) {
            k = idx + i + j * (p->Width + 1);
            if (k <= total && k >= 0) {
                p->pHeightBuf[k].y += FGet(Add_power) * 0.35f;
            }
        }
    }
    for (i = -3; i <= 3; i++) {
        for (j = -3; j <= 3; j++) {
            k = idx + i + j * (p->Width + 1);
            if (k <= total && k >= stride) {
                p->pHeightBuf[k].y = p->pHeightBuf[k].y * 2.5f + p->pHeightBuf[k + 1].y * 0.5f + p->pHeightBuf[p->ny + k + 1].y * 0.5f +
                              p->pHeightBuf[p->ny + k + 2].y * 0.5f;
                p->pHeightBuf[k].y *= 0.25f;
            }
        }
    }
}

// Public dent entry: deforms every live sand surface at `pos` by `power`; no-op unless a sand
// surface exists this frame (Status_flg[0] bit1).
void AddSandPower(Vec* pos, f32 power)
{
    if (StaFlagChk(pG, STA_SAND_ALIVE)) {
        // COMPILER-DIFF: word copy with the .z word pinned to r11 (the original issues `stfs Add_power`
        // in the first cycle in both schedulers). Scalar `u32` loads are not MEM_IN_STRUCT_P, so the
        // plain `Add_power` store gates them (priority 7) and takes the first cycle in sched1 too;
        // that moves the Chk_pos high's birth one slot later, which would rank it above the .z word
        // (2 refs) in local-alloc and give it r11. Pinning the .z word removes that qty: high takes
        // r10 (r11 busy), addi r8, W0 r0, W4 r9 as in the original. Order 0,4,8 keeps the addi's
        // death on the .z store so sched1 issues S0, S8, S4.
        register u32 c asm("r11");
        u32* s = (u32*) pos;
        u32* d = (u32*) &Chk_pos;
        u32 a, b;
        Add_power = power;
        ISet(Height_find, 0);
        a = s[0];
        b = s[1];
        c = s[2];
        d[0] = a;
        d[1] = b;
        d[2] = c;
        EspgenApplyFunc(AddSandPowerSub);
    }
}

// Height test of Chk_pos on sand generator `w`: inside the grid, the surface plane height (the
// grid is treated as flat) is stored in Height_ret.
void GetSandHeightSub(EspgenWork* w)
{
    Espgen43Work* p;
    Vec v;

    if (w->id != 0x43) {
        return;
    }
    p = (Espgen43Work*) w->work;
    v = Chk_pos;
    PSMTXMultVec(p->Inv_mat, &v, &v);
    if (v.x < (f32) (-p->Width / 2)) {
        return;
    }
    if (v.z < (f32) (-p->ny / 2)) {
        return;
    }
    if (v.x > (f32) (p->Width / 2)) {
        return;
    }
    if (v.z > (f32) (p->ny / 2)) {
        return;
    }
    v.y = 0.0f;
    PSMTXMultVec(p->Wld_mat, &v, &v);
    if (v.y > Height_ret) {
        Height_ret = v.y;
    }
    Height_find = 1;
}

// Surface height under `pos` on any live sand generator: 1 and *height, 0 when none covers it.
int GetSandHeight(Vec* pos, f32* height)
{
    if (!StaFlagChk(pG, STA_SAND_ALIVE)) {
        return 0;
    }
    ISet(Height_find, 0);
    FSet(Height_ret, -100000000.0f);
    Chk_pos = *pos;
    EspgenApplyFunc(GetSandHeightSub);
    *height = Height_ret;
    return Height_find;
}

// Step 0, every frame: sets Status_flg[0] bit1 and recomputes the vertex normals from the
// neighbouring heights, flushing both buffers for the GP.
#line 246 "D:/Bio4/Prog/Espgen43.cpp"
void Espgen43_Move00(EspgenWork* w)
{
    Espgen43Work* p = (Espgen43Work*) w->work;
    Vec v;
    int i;
    int j;
    int k;
    u32 n;

    StaFlagOn(pG, STA_SAND_ALIVE);
    for (i = 1; i < p->ny; i++) {
        k = i * (p->Width + 1);
        for (j = 1; j < p->Width; j++) {
            Vec* n = &p->pNorBuf[k];
            Vec* q = &p->pHeightBuf[k];
            v.x = q[-1].y - q[1].y;
            v.y = 2.0f;
            v.z = p->pHeightBuf[k - p->Width].y - p->pHeightBuf[k + p->Width].y;
            VECNormalize(&v, n);
            k++;
        }
    }
    n = sizeof(Vec) * (p->Width + 1) * (p->ny + 1);
    DCStoreRange(p->pHeightBuf, n);
    DCStoreRange(p->pNorBuf, n);
}

// Espgen move entry for id 0x43: dispatches on w->step.
void Espgen43_Move(EspgenWork* w)
{
    static void (*Espgen43MoveTbl[])(EspgenWork*) = {Espgen43_Move00};

    Espgen43MoveTbl[w->step](w);
}

// Queues Espgen43_TransSub in the world OT (0x10, layer 1, priority 0x80) while live.
void Espgen43_Trans(EspgenWork* w)
{
    if ((w->flag & 1) && !(w->flag & 2)) {
        AddOtDirect(0x10, w, (void (*)()) Espgen43_TransSub, 1, 0x80, NULL, 0.0f);
    }
}

// Draws the sand grid: lights from commonClothLightSet, material / ambient colours, texture
// TexNo, then the pre-built display list of triangle strips.
void Espgen43_TransSub(EspgenWork* w)
{
    GxStageWork* st;
    Espgen43Work* p;
    GXTexObj* tex;
    GXTlutObj* tlut;
    f32 r;

    if (!(w->flag & 1)) {
        return;
    }
    if (w->flag & 2) {
        return;
    }
    st = &pG->gxStage;
    p = (Espgen43Work*) w->work;
    st->tevStage = 0;
    st->texMap = 0;
    st->texCoord = 0;
    CameraCurrentProjection();
    GXSetCullMode(0);
    GXSetZMode(1, 3, 1);
    cModel model;
    u8 modelPad[0x320 - sizeof(cModel)];
    PSMTXIdentity(model.mat);
    {
        static const Vec p0 = {0.0f, 0.0f, 0.0f};
        static const Vec p1 = {10000.0f, 10000.0f, 10000.0f};
        model.LightInfo.init2(1, 0, &p0, &p1, 0x10);
    }
    model.pos.x = p->Wld_mat[0][3];
    model.pos.y = p->Wld_mat[1][3];
    model.pos.z = p->Wld_mat[2][3];
    LightMgr.setClothN(&model, 5);
    if (model.LightInfo.Size.x > model.LightInfo.Size.y) {
        r = model.LightInfo.Size.x;
    } else {
        r = model.LightInfo.Size.y;
    }
    commonClothLightSet(model.LightInfo.pLight, 5, model.pos, r);
    GXSetChanMatColor(4, p->Color);
    {
        Mtx nrm;
        Mtx mv;
        PSMTXConcat(pG->Cam.v_mat, p->Wld_mat, mv);
        PSMTXInverse(mv, nrm);
        PSMTXTranspose(nrm, nrm);
        GXLoadNrmMtxImm(nrm, 0);
        GXLoadPosMtxImm(mv, 0);
    }
    GXSetBlendMode(1, 4, 5, 0);
    tex = EspGetTexObj(p->TexNo, 0);
    if (tex == NULL) {
        tex = &Specular;
    }
    GXLoadTexObj(tex, st->texMap);
    GXSetTexCoordGen2(st->texCoord, 1, 4, 0x3C, 0, 0x7D);
    tlut = EspGetTlutObj(p->TexNo);
    if (tlut != NULL) {
        GXLoadTlut(tlut, 0);
    }
    GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
    GXSetTevColorIn(st->tevStage, 0xF, 0xA, 8, 0xF);
    GXSetTevColorOp(st->tevStage, 0, 0, 2, 1, 0);
    GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
    GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
    st->tevStage++;
    st->texMap++;
    st->texCoord++;
    GXSetNumTevStages(st->tevStage);
    GXSetNumTexGens(st->texCoord);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 3);
    GXSetVtxDesc(10, 3);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 10, 0, 4, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXSetArray(9, p->pHeightBuf, sizeof(Vec));
    GXSetArray(10, p->pNorBuf, sizeof(Vec));
    GXCallDisplayList(p->pDisplayList, p->Dpl_size);
}

// Dead-stripped from the DOL (pool and string kept): pulls a generator and sets the grid up.
static int SetSand(Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny)
{
    EspgenWork* w;

    if (PullEspgen(&w) == 0) {
        pLog->err(0, 0, "Espgen43 : work pull failed");
        return 0;
    }
    return (int) SetSandWork(w, pos, rot, size, 1.0f, nx, ny);
}

// Texture coordinate wrap: keeps the repeat in 0..1 by mirroring at 1.
#define TEX_WRAP(v)                                                                                 \
    while ((v) > 2.0f) {                                                                            \
        (v) -= 2.0f;                                                                                \
    }                                                                                               \
    while ((v) > 1.0f) {                                                                            \
        (v) = 2.0f - (v);                                                                           \
    }

// Builds the grid at pos / rot with cell `size` (height axis scaled by sizeRate): allocates the
// height and normal buffers and the display list (texture repeated texRep times across the
// grid). Returns NULL (and releases the generator) on memory failure.
EspgenWork* SetSandWork(EspgenWork* w, Vec* pos, Vec* rot, f32 size, f32 sizeRate, u32 nx, u32 ny)
{
    Espgen43Work* p = (Espgen43Work*) w->work;
    Mtx m;
    u32 n;
    u8* d;
    int i;
    int j;
    int k;
    f32 rep;
    f32 fx;
    f32 fy;

    w->id = 0x43;
    p->Width = nx;
    p->ny = ny;
    p->Size = size;
    RotMatrix(p->Wld_mat, rot);
    PSMTXScale(m, p->Size, p->Size * sizeRate, p->Size);
    PSMTXConcat(p->Wld_mat, m, p->Wld_mat);
    PSMTXTransApply(p->Wld_mat, p->Wld_mat, pos->x, pos->y, pos->z);
    PSMTXInverse(p->Wld_mat, p->Inv_mat);
    n = sizeof(Vec) * (p->Width + 1) * (p->ny + 1);
#line 445 "D:/Bio4/Prog/Espgen43.cpp"
    p->pHeightBuf = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pHeightBuf == NULL) {
        pLog->err(0, 0, "Espgen43 : not enough memory");
        PushEspgen(w);
        return 0;
    }
    memclr_asm(p->pHeightBuf, n);
#line 452 "D:/Bio4/Prog/Espgen43.cpp"
    p->pNorBuf = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pNorBuf == NULL) {
        pLog->err(0, 0, "Espgen43 : not enough memory");
        PushEspgen(w);
        return 0;
    }
    memclr_asm(p->pNorBuf, n);
    p->Dpl_size = ((p->Width + 1) * (p->ny + p->ny) * 12 + 0x61) & ~0x1F;
#line 466 "D:/Bio4/Prog/Espgen43.cpp"
    p->pDisplayList = (u8*) MEM_ALLOC(p->Dpl_size, 1, 13);
    if (p->pDisplayList == NULL) {
        pLog->err(0, 0, "Espgen43 : not enough memory");
        PushEspgen(w);
        return 0;
    }
    memclr_asm(p->pDisplayList, p->Dpl_size);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 3);
    GXSetVtxDesc(10, 3);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 10, 0, 4, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    d = p->pDisplayList;
    *d = 0;
    d++;
    *d = 0x98;
    d++;
    *(u16*) d = (p->Width + 1) * (p->ny + p->ny);
    d++;
    d++;
    rep = p->texRep;
    for (i = 0; i < p->ny; i++) {
        k = i * (p->Width + 1);
        for (j = 0; j < p->Width + 1; j++) {
            *(u16*) d = k;
            d++;
            d++;
            *(u16*) d = k;
            d++;
            d++;
            k++;
            *(f32*) d = (f32) j / p->Width * rep;
            TEX_WRAP(*(f32*) d);
            d++;
            d++;
            d++;
            d++;
            *(f32*) d = (f32) i / p->ny * rep;
            TEX_WRAP(*(f32*) d);
            d++;
            d++;
            d++;
            d++;
            *(u16*) d = p->Width + k;
            d++;
            d++;
            *(u16*) d = p->Width + k;
            d++;
            d++;
            *(f32*) d = (f32) j / p->Width * rep;
            TEX_WRAP(*(f32*) d);
            d++;
            d++;
            d++;
            d++;
            *(f32*) d = (f32) (i + 1) / p->ny * rep;
            TEX_WRAP(*(f32*) d);
            d++;
            d++;
            d++;
            d++;
        }
        i++;
        if (i < p->ny) {
            for (j = p->Width; j >= 0; j--) {
                k = i * (p->Width + 1) + j;
                *(u16*) d = k;
                d++;
            d++;
                *(u16*) d = k;
                d++;
            d++;
                *(f32*) d = (f32) j / p->Width * rep;
                TEX_WRAP(*(f32*) d);
                d++;
            d++;
            d++;
            d++;
                k++;
                *(f32*) d = (f32) i / p->ny * rep;
                TEX_WRAP(*(f32*) d);
                d++;
            d++;
            d++;
            d++;
                *(u16*) d = p->Width + k;
                d++;
            d++;
                *(u16*) d = p->Width + k;
                d++;
            d++;
                *(f32*) d = (f32) j / p->Width * rep;
                TEX_WRAP(*(f32*) d);
                d++;
            d++;
            d++;
            d++;
                *(f32*) d = (f32) (i + 1) / p->ny * rep;
                TEX_WRAP(*(f32*) d);
                d++;
                d++;
                d++;
                d++;
            }
        }
    }
    fy = 0.0f;
    {
        int x;
        int y;
        int idx;
        for (y = 0; y < p->ny + 1; y++) {
            fx = 0.0f;
            idx = y * (p->Width + 1);
            for (x = 0; x < p->Width + 1; x++) {
                p->pHeightBuf[idx].x = fx - (f32) (p->Width / 2);
                p->pHeightBuf[idx].y = fRand1_1() * 0.15f;
                p->pHeightBuf[idx].z = fy - (f32) (p->ny / 2);
                p->pNorBuf[idx].x = 0.0f;
                p->pNorBuf[idx].y = 1.0f;
                p->pNorBuf[idx].z = 0.0f;
                fx += 1.0f;
                idx++;
            }
            fy += 1.0f;
        }
    }
    {
        u32 n2 = sizeof(Vec) * (p->Width + 1) * (p->ny + 1);
        DCStoreRange(p->pHeightBuf, n2);
        DCStoreRange(p->pNorBuf, n2);
    }
    DCStoreRange(p->pDisplayList, p->Dpl_size);
    return w;
}

// Frees the height, normal and display list buffers.
void Espgen43_Destruct(EspgenWork* w)
{
    Espgen43Work* p = (Espgen43Work*) w->work;

    if (p->pHeightBuf != NULL) {
        Mem_free(p->pHeightBuf);
        p->pHeightBuf = NULL;
    }
    if (p->pNorBuf != NULL) {
        Mem_free(p->pNorBuf);
        p->pNorBuf = NULL;
    }
    if (p->pDisplayList != NULL) {
        Mem_free(p->pDisplayList);
        p->pDisplayList = NULL;
    }
}

// Espgen SetFreeWork for id 0x43: grid size prm 0xCC / 0xD0 (default 64, max 256), colours /
// ambient from the record, texture Tex_id, repeat 2^Work8[0], height scale Size_plus + 1; runs
// one move step at once.
int Espgen43_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct)
{
    Espgen43Work* p = (Espgen43Work*) w->work;
    Vec r;
    u32 nx = 0x40;
    u32 ny = 0x40;

    if (rec->prm.w.xCC != 0) {
        nx = rec->prm.w.xCC;
        if (nx > 0x100) {
            nx = 0x100;
        }
    }
    if (rec->prm.w.xD0 != 0) {
        ny = rec->prm.w.xD0;
        if (ny > 0x100) {
            ny = 0x100;
        }
    }
    p->Color.r = rec->Col_start_r;
    p->Color.g = rec->Col_start_g;
    p->Color.b = rec->Col_start_b;
    p->Color.a = rec->Col_start_a;
    p->Amb.r = rec->Col_d_r * 255.0f;
    p->Amb.g = rec->Col_d_g * 255.0f;
    p->Amb.b = rec->Col_d_b * 255.0f;
    p->Amb.a = rec->Col_d_a * 255.0f;
    p->TexNo = rec->Tex_id;
    p->texRep = 1 << (s8) rec->Work8[0];
    PSVECScale(&rec->Ang, &r, 6.28f / 360.0f);
    if (SetSandWork(w, (Vec*) &rec->Pos.x, &r, rec->Size_base_x, rec->Size_plus + 1.0f, nx, ny) == NULL) {
        return 0;
    }
    Espgen43_Move(w);
    return 1;
}

asm(".section .sdata; .balign 8");
