// game/espgen45: effect controller 45, the weather (open-air) water surface (D:/Bio4/Prog/espgen45.cpp).
// A height-field grid (Espgen42Work: hA/hB height buffers, pos/nrm vertex arrays, a bump texture
// and a display list) that follows the camera target, is stirred by the noise texture 0xFE and drawn
// with a screen-copy refraction, an indirect bump stage, a specular texture and an optional mask.
// Room code overrides position/height/size/colour/parameters through the Estgen45Set* entry points
// (esp4c passes an Esp4cWork). Entry points: Espgen45_Move / _Trans / _SetFreeWork / _Destruct.
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
#include "main_sub.h"
#include "joy.h"

// Effect controller 45: weather water surface (same height-field model as Espgen42, following the
// camera). The Estgen45Set* entry points let the room script (esp4c) override its parameters.
// Espgen42 owns the water init (EspWaterInit): it resets this unit's overrides and g_pWater45.

// Parameter block handed over by esp4c (Esp4cWork, 0x20 bytes; the layout is esp4c.cpp's).
struct Esp4cWork {
    u8 Type;      // 0x00
    u8 Refrect_type;        // 0x01
    u8 Spec_Tex;        // 0x02
    u8 wave_ratio_base;        // 0x03
    s16 Shimmer_pow1;     // 0x04 indirect matrix parameters (SetIndMtx)
    s16 Shimmer_pow2;     // 0x06
    f32 damp;     // 0x08 (Espgen42Work::damp)
    f32 spread;   // 0x0C (Espgen42Work::spread)
    Vec ang;      // 0x10 surface rotation (SetWaterWork45)
    u8 flag;      // 0x1C
    u8 Mask_Tex;       // 0x1D
    u8 x1E;       // 0x1E
    u8 x1F;       // 0x1F
};

extern "C" {
// game/trans_lit.cpp
void commonWaterLightSet(cLight** list, int n, u32 alpha);
// Dolphin SDK performance monitor registers (base/PPCArch.h)
void PPCMtpmc1(u32 v);
void PPCMtpmc2(u32 v);
void PPCMtpmc3(u32 v);
void PPCMtpmc4(u32 v);
void PPCMtmmcr0(u32 v);
void PPCMtmmcr1(u32 v);
void Espgen45_Move00(EspgenWork* w);
void Espgen45_TransSub(EspgenWork* w);
void SetIndMtx_801291F4(Espgen42Work* p);   // the DOL's local SetIndMtx (Espgen42 owns the global one); sym_map name
EspgenWork* SetWaterWork45(EspgenWork* w, Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny, f32 rate);
}

EspgenWork* g_pWater45;
static int g_bTargetCamera = 1;
static int g_bTargetHeight = 1;
static int g_bSizeOverWrite = 0;
static int g_bColorOverWrite = 0;
static int g_bColorMul = 0;
static int g_bSetParam = 0;
static f32 g_Target_x = 0.0f;
static f32 g_Target_y = 0.0f;
static f32 g_Target_z = 0.0f;
static f32 g_Size = 0.0f;
static u8 g_r = 0;
static u8 g_g = 0;
static u8 g_b = 0;
static u8 g_a = 0;
static f32 g_sr = 0.0f;
static f32 g_sg = 0.0f;
static f32 g_sb = 0.0f;
static f32 g_sa = 0.0f;
static f32 inv_mul = 1.0f;
static Esp4cWork g_Free;

static inline void ISet(int& d, int v) { d = v; }
static inline int IGet(int& d) { return d; }
static inline f32 FGet(f32& d) { return d; }
static inline void U8Set(u8& d, u8 v) { d = v; }

// Resets the room override state (camera-follow on, height-follow on, all overwrites off): called by
// EspWaterInit (Espgen42.cpp) at effect system init.
void Espgen45_static_init()
{
    g_bTargetCamera = 1;
    g_bTargetHeight = 1;
    g_Target_x = 10000000000.0f;
    g_Target_y = -10000000000.0f;
    g_Target_z = 100000000000.0f;
    g_bSizeOverWrite = 0;
    g_bColorOverWrite = 0;
    g_bColorMul = 0;
    g_bSetParam = 0;
    g_Size = 0.0f;
    g_r = 0;
    g_g = 0;
    g_b = 0;
    g_a = 0;
    g_sr = 0.0f;
    g_sg = 0.0f;
    g_sb = 0.0f;
    g_sa = 0.0f;
}

// u8 -> f32 through GQR2 from a stack byte (the compiler only emits psq_l from its own fpmem slot). volatile (no
// memory clobber): a volatile asm is a scheduling barrier for everything in RTL order around it, which is what puts
// the pos/cur address adds before the noise lbzx and the neighbour loads after it in the target's loop A.
// Loads straight into the destination variable (no statement-expression temp): the target's `psq_l f10; fsubs f10,f10`
// is one pseudo, the function-level `n`.
#define PSQ_L_U8_TO(dst, p) asm volatile("psq_l %0,0(%1),1,2" : "=f"(dst) : "b"(p), "m"(*(p)))

// Bump texture (I8, 8x4 tiles) index of grid point (x, y). x/8 before y/4 (the two signed divisions are
// separate blocks, so their order is the source order) and `(y / 4) << 5`: with `* 32` fold would
// reassociate the constant onto `(w1) >> 3` and hoist `(w1 >> 3) * 32`; the target keeps `srwi` in the loop.
#define BUMP_INDEX(x, y, w1) (((x) / 8) * 32 + (((y) / 4) << 5) * ((w1) >> 3) + (((y) & 3) << 3) + ((x) & 7))
// Noise texture (0xFE) index of grid point (x, y).
#define NOISE_INDEX(x, y) ((((y) << 6) & 0xB00) + (((x) << 2) & 0xA0) + (((y) & 3) << 3) + ((x) & 7))

// Step 0 (the only step) of Espgen45MoveTbl: recentres the surface on the camera target (or the
// override target/height), rebuilds mat/inv with the size and wave_ratio scale, then simulates one
// frame: mode != 1 is a damped two-buffer wave equation (damp/spread) plus noise-texture excitation;
// mode 1 is the spring model (hA height, hB velocity). Both refresh the vertex heights, normals and
// the bump texture, then flush the arrays to memory for the GP. Sets Status_flg[0] bit 0x200
// (water present). Debug: L trigger with Debug_flg[1] 0x00800000 drops a wave in the middle.
void Espgen45_Move00(EspgenWork* w)
{
    static f32 g45_wave_mul = 0.001f;
    static f32 wt_pow = 10.0f;
    Espgen42Work* p = (Espgen42Work*) w->work;
    Vec d0;
    Vec d1;
    Vec v;
    Vec d2;
    Vec d3;
    Mtx m;
    u8 tmp;
    GXTexObj* tex;
    u8* noise;
    f32* c;
    u32 frame;
    f32 size;
    f32 rate;
    u8 rotY;
    u8 mode;
    int i;
    int j;
    int k;
    int nz;   // noise index / byte of both loops: one function-level pseudo (see loop A)
    // loop A's `j / 8` and loop B's `(i / 4) << 5`: one function-level pseudo (r10 in both loops; a block-local
    // `(i / 4) << 5` would be tied into the `mullw` by local-alloc, the target ties the nx term).
    int jx;
    // loop B's `k + p->nx` (before the call) and `p->nx` (after it): one function-level pseudo (r11 in both places,
    // "set 2 times; dies in 2 places" = no local-alloc tie, so the Z block's `addi r9,r11,1` starts a fresh r9 chain).
    int mx;
    // noise value of both loops: also one function-level pseudo (global alloc, f10 in both loops). A block-local `n`
    // ties to the loop-A psq_l output / loop-B frsp result and permutes the loop-A/B FPR names and the sched2 slots
    // of the loop-B sum chain (77 -> 47 words).
    f32 n;
    // normal pointer `&nrm[k]` of both loops: one function-level pseudo (36 refs / 170 insns, 1.06) ranks above loop B's
    // k*4 giv (35 / 169, 1.04) and k (32 / 215, 0.74) in global alloc and takes r30 for both loops; k*4 then finds r30
    // taken and gets r29, k r31. A block-local pointer per loop (18 / 111, 0.65) came after both (k*4 r30, pointer r29).
    Vec* nk;
    // `j & 7` of both loops (noise index and bump index): one function-level pseudo ranks above `i` in global alloc and
    // takes r24 (i r23, (k-nx)*12 r26); a block-local `j & 7` per loop came after i, which then took r24. u8 as in
    // Espgen42 (there the narrow store is the +1 loop.c insn that keeps the 4.0 pool pair for the outer pass).
    u8 j7;

    d0.x = 1.0f;
    d0.z = 0.0f;
    d1.x = 0.0f;
    d1.z = 1.0f;
    d2.x = -1.0f;
    d2.z = 0.0f;
    d3.x = 0.0f;
    d3.z = -1.0f;
    frame = pG->Frame_cnt % 60;
    StaFlagOn(pG, STA_WATER_ALIVE);
    if (g_bTargetCamera == 1) {
        FSet(g_Target_x, pG->Cam.param.at.x);
        FSet(g_Target_z, pG->Cam.param.at.z);
    }
    FSet(p->pos0.x, g_Target_x);
    p->pos0.z = g_Target_z;
    if (IGet(g_bTargetHeight) == 1) {
        p->pos0.y = g_Target_y;
    } else {
        p->pos0.y = p->Base_y;
    }
    size = p->size;
    if (g_bSizeOverWrite == 1) {
        size = g_Size;
    }
    if (g_bSetParam == 0) {
        PSMTXScale(p->mat, size, size * 0.05f + 100.0f, size);
    } else {
        RotMatrix(p->mat, &g_Free.ang);
        PSMTXScale(m, size, size * 0.05f + 100.0f, size);
        PSMTXConcat(p->mat, m, p->mat);
    }
    if (g_bSetParam == 0) {
        rotY = p->rotY;
    } else {
        rotY = g_Free.wave_ratio_base;
    }
    rate = 1.0f - (f32) (int) rotY / 255.0f;
    if (rate == 0.0f) {
        rate = 0.0001f;
    }
    p->mat[1][1] *= rate;
    PSMTXTransApply(p->mat, p->mat, p->pos0.x, p->pos0.y, p->pos0.z);
    PSMTXInverse(p->mat, p->inv);
    PPCMtpmc1(0);
    PPCMtpmc2(0);
    PPCMtpmc3(0);
    PPCMtpmc4(0);
    PPCMtmmcr1(0x78000000);
    PPCMtmmcr0(0x42);
    tex = EspGetTexObj(0xFE, frame);
    if (tex == NULL) {
        return;
    }
    noise = (u8*) GXGetTexObjData(tex) + 0x80000000;
    u32 nx = p->nx;
    f32 hx = (f32) (int) (nx / 2);
    f32 hy = (f32) (int) (p->ny / 2);
    f32 inx = 1.0f / (f32) (int) nx * inv_mul;
    f32 iny = 1.0f / (f32) (int) p->ny * inv_mul;
    if (g_bSetParam == 0) {
        mode = p->mode;
    } else {
        mode = g_Free.Type;
    }
    if (mode != 1) {
        if (DbgFlagChk(pG, DBG_IN_ESP_TOOL) && (Joy[0].on & 0x100)) {
            // The index is the loop variable `k` (target `lwz r31` = k's register, base+index `lfsx f0,hB,k4`).
            k = (int) ((f32) (int) (p->nx * p->ny) * 0.5f);
            // Byte offset in a variable: inside an address `p->hB[k]` expands to `(plus (mult k 4) hB)` (expr.c
            // both_summands puts a MULT first) = `lfsx k4,hB`; a register index keeps `(plus hB k4)` = `lfsx hB,k4`.
            u32 k4 = k * 4;
            *(f32*) ((u8*) p->hB + k4) -= wt_pow;
        }
        f32 damp;
        f32 spread;
        if (g_bSetParam == 0) {
            damp = p->damp;
        } else {
            damp = g_Free.damp;
        }
        f32 cdamp = 2.0f - damp * 4.0f;
        if (g_bSetParam == 0) {
            spread = p->spread;
        } else {
            spread = g_Free.spread;
        }
        f32* cur;
        f32* next;
        if (pG->Frame_cnt & 1) {
            cur = p->hA;
            next = p->hB;
        } else {
            cur = p->hB;
            next = p->hA;
        }
        f32 fy = 1.0f;
        for (i = 1; i < p->ny; i++) {
            f32 fx = 1.0f;
            k = i * (nx + 1);   // two statements: the product lands in k's register (`mullw r31; addi r31,r31,1`)
            k++;
            for (j = 1; j < (int) nx; j++) {
                int i3 = (i & 3) << 3;
                // The k*4 giv is discovered BEFORE the k*12 giv (`&p->pos[k]` below): loop.c emits the giv inits in bl->giv
                // order = reverse discovery, so the preheader is `mr r29,k12 | slwi r31,k,2` (k4 init last) and the two
                // `mulli` take r8/r10 as the target. With `c += k` as the first k*4 use the two inits were swapped.
                u32 k4 = k * 4;
                // Before the (volatile) psq_l: `lwz pos; add c; add pos+k12` issue before the noise lbzx (target order).
                Vec* pv = &p->pos[k];   // a pointer variable: `add pos,k12` (operand order); `p->pos[k].y = ..` gives `add k12,pos`
                // `c` is a function-level pointer set twice per iteration (set_in_loop != 1, so loop.c
                // does not treat it as a giv): the neighbours stay `lfs 4(c)/-4(c)` off `add c = cur + k*4`
                // and `*(c - nx - 1)` becomes `subf` + `lfs -4` off the hoisted `nx*4`.
                c = cur;
                c = (f32*) ((u8*) c + k4);
                // The index in the function-level `nz` (set in both loops = global pseudo, allocated after local-alloc): the
                // byte pseudo then finds r0 free in local-alloc (its fake-lifetime pass would refuse the register of a
                // block-local index dying at the lbzx), and global alloc gives nz r0 too: `lbzx r0,noise,r0; stb r0`.
                j7 = j & 7;
                nz = (((i) << 6) & 0xB00) + (((j) << 2) & 0xA0) + i3 + j7;
                tmp = noise[nz];
                PSQ_L_U8_TO(n, &tmp);
                n -= 80.0f;
                f32 sum = c[-1] + c[1] + *(c - nx - 1) + *(c + nx + 1);
                next[k] = damp * sum + cdamp * cur[k] - next[k];
                // FGet: a reference read is a MEM with neither the struct nor the scalar flag, so it depends on the
                // `stfsx next[k]` store (next's alias base is 0) and issues right after it like the target's
                // `lfs g45_wave_mul`; the plain static read is a fixed scalar that never aliases the in-struct store
                // and floated 6 insns up. The pos address is computed before the store (target `lwz pos` early).
                pv->y = next[k] = (n * FGet(g45_wave_mul) + next[k]) * spread;
                Vec* nrm = p->nrm;   // before the v.x/v.z reads: kept across the call (`lfsx nrm[k].x`, `4(nrm+k*12)`)
                v.x = p->pos[k - 1].y - p->pos[k + 1].y;
                v.y = 2.0f;
                v.z = p->pos[k - nx].y - p->pos[k + nx].y;
                nk = &nrm[k];   // the function-level pointer (see its declaration); `nrm[k].y/.z` below fold onto it in cse
                PSVECScale(&v, nk, 1.0f / 2.3f);
                {
                    // BUMP_INDEX with the function-level `j7` (r24, see its declaration) as the last term.
                    // Both signed divisions through ONE temp `t` (the target's `mr r0,j .. srawi jx,r0 | cmpwi i; mr r0,i`: the
                    // second copy anti-depends on the first srawi in sched1, so the compare issues before it, and both temps
                    // share r0); `jx << 5` (a shift, not `jx * 32`: a MULT in an address sum is put first by expand and would
                    // start the add chain, the target starts it with the i term: `add r9,r9,r0`).
                    int t = j;
                    if (j < 0) t = j + 7;
                    jx = t >> 3;
                    t = i;
                    if (i < 0) t = i + 3;
                    p->bump[((t >> 2) << 5) * ((nx + 1) >> 3) + (jx << 5) + i3 + j7] = (u8) (nrm[k].x * 255.0f * 2.0f + 128.0f);
                }
                nrm[k].x += (fx - hx) * inx;
                nrm[k].z += (fy - hy) * iny;
                nrm[k].y *= 0.25f;
                fx += 1.0f;
                k++;
            }
            fy += 1.0f;
        }
    } else {
        // COMPILER-DIFF: loop.c move_movables. The target hoists one more invariant out of the inner loop before the
        // 4.0 pool pair (threshold 71 - 3 per moved insn), so 4.0 stays for the outer pass (outer preheader) and
        // 0.0018 moves in the inner pass 2. A codeless asm set of `i` used after the inner loop is that extra moved
        // insn (no register: it takes a free callee-saved GPR); emits nothing. The real construct is unknown.
        int dead;
        for (i = 1; i < p->ny; i++) {
            k = i * (p->nx + 1);
            for (j = 1; j < p->nx; j++) {
                asm("" : "=r"(dead) : "r"(i));
                j7 = j & 7;
                nz = (((i) << 6) & 0xB00) + (((j) << 2) & 0xA0) + (((i) & 3) << 3) + j7;   // NOISE_INDEX with the shared j7
                nz = noise[nz];   // same variable: `lbzx r0,noise,r0; xoris r0` (see loop A)
                // As in loop A: the pos address before the hB/hA stores (p has an unknown alias base, so a `lwz pos`
                // placed after them would wait for them; the target issues it before the first `stfsx hB[k]`).
                Vec* pv = &p->pos[k];
                f32* hA = p->hA;
                f32* hB = p->hB;
                c = hA;
                c += k;
                f32 sum = c[-1] + c[1] + *(c - p->ny - 1) + *(c + p->ny + 1);
                // n before the hB[k] update: the 0x4330/pool-double and 80.0 movables precede the 4.0 pair in loop.c's
                // list (the target's inner preheader order is lfd; lfs 80.0; lfs 1.0; ...; 4.0 is in the outer one).
                n = (f32) nz - 80.0f;
                // hB through a byte offset in a variable (`stfsx hB,k4` base first, see the wt_pow store above); hA's
                // address is the cse'd `c` (`(plus hA k4)` from `c += k`, a non-address context) and is base first as is.
                u32 k4 = k * 4;
#define HB (*(f32*) ((u8*) hB + k4))
                HB += sum - hA[k] * 4.0f;
                hA[k] += n * 0.0001f + HB * 0.04f;
                HB *= 0.92f;
#undef HB
                pv->y = n * 0.0018f + hA[k];
                Vec* nrm = p->nrm;
                v.x = p->pos[k - 1].y - p->pos[k + 1].y;
                v.y = 2.0f;
                // `d` first: the `add mx` is then the last use of the `lhz nx` (REG_WEIGHT 0) and issues before the `subf`.
                int d = k - p->nx;
                mx = k + p->nx;
                v.z = p->pos[d].y - p->pos[mx].y;
                nk = &nrm[k];   // the function-level pointer shared with loop A (r30 in both loops, see its declaration)
                PSVECScale(&v, nk, 1.0f / 2.3f);
                {
                    // Loop B's index differs from loop A's: `j / 8` in its own statement (first division, own temp), the i
                    // term FIRST (`jx * nx8`: the nx chain `lhz; addi; srawi` is then evaluated after the i-division's branch,
                    // inside the Z block as in the target, and `mullw r9,r10,r9` ties the nx term), `jq << 5` (see loop A).
                    // COMPILER-DIFF: candidate #17 (r0 occupant): `j / 8` pinned to r0 = a hard-register conflict of the i
                    // copy `t_i` with r0 during the i-division blocks, so global.c's preference override skips r0 and t_i
                    // takes r9 (the target's `mr r9,i`); jq's shift then inherits r0 (`slwi r0,r0,5`).
                    register int jq asm("r0") = j / 8;
                    jx = (i / 4) << 5;
                    mx = p->nx;
                    // COMPILER-DIFF: candidate (sched1 slot fillers). Three codeless frame stores of `v` (dead after the
                    // call) that read `jx`: ready at t2, priority 90 (the `lfsx nrm[k].x` below may alias them), they take
                    // the t2/t3 issue slots and delay that load to t4, so the byte chain (fmuls .. psq_st) runs one cycle
                    // later and the fast-cast loadaddr (`unspec 17`, prints nothing) is born after the `mullw` (jx dead:
                    // it can take r10), the j loadaddr lives longer than the byte (byte r11, loadaddr r8) and `xoris j` is
                    // issued after the first index `add` (jq32 and the xoris share r0). Without them the loadaddr is issued
                    // at t2 and the xoris before the add: 49 words in the Z block. What the original had there is unknown.
                    asm("" : "=m"(v.x) : "r"(jx));
                    asm("" : "=m"(v.y) : "r"(jx));
                    asm("" : "=m"(v.z) : "r"(jx));
                    p->bump[jx * ((mx + 1) >> 3) + (jq << 5) + ((i & 3) << 3) + j7] = (u8) (nrm[k].x * 255.0f * 2.0f + 128.0f);
                }
                nrm[k].x += ((f32) j - (f32) (p->nx / 2)) * (1.0f / (f32) (int) p->nx);
                nk->z += ((f32) i - (f32) (p->ny / 2)) * (1.0f / (f32) (int) p->ny);
                nk->y *= 0.25f;
                k++;
            }
            asm("" : : "r"(dead));   // COMPILER-DIFF: use of the moved asm set (see above); emits nothing
        }
    }
    {
        u32 n = sizeof(Vec) * (p->nx + 1) * (p->ny + 1);   // nx first: fold attaches the 12 to (ny + 1) as the target
        DCStoreRange(p->pos, n);
        DCStoreRange(p->nrm, n);
        DCStoreRange(p->bump, sizeof(Vec) * (p->nx + 1) * (p->ny + 1));
    }
}

// EspgenMoveTbl entry for controller type 0x45; frozen while Stop_flg bit 0x40000 is set.
void Espgen45_Move(EspgenWork* w)
{
    static void (*Espgen45MoveTbl[])(EspgenWork*) = {Espgen45_Move00};

    if (SpfFlagChk(pG, SPF_WATER)) {
        return;
    }
    Espgen45MoveTbl[w->step](w);
}

// EspgenTransTbl entry: queues Espgen45_TransSub in OT layer 0x10 (drawn after the opaque scene) and
// clears Status_flg[1] bit 0x20 (the "override parameters changed this frame" flag).
void Espgen45_Trans(EspgenWork* w)
{
    if ((w->flag & 1) && !(w->flag & 2)) {
        AddOtDirect(0x10, w, (void (*)()) Espgen45_TransSub, 1, 0x80, NULL, 0.0f);
    }
    StaFlagOff(pG, STA_ESPGEN45_SET);
}

// Loads indirect texture matrix 1 for the bump stage: S scale indS*0.001+0.01, T scale
// indT*0.007+0.07 (or the Esp4cWork Shimmer_pow1/2 when the parameter override is on).
void SetIndMtx_801291F4(Espgen42Work* p)
{
    f32 m[2][3];
    s16 indS;
    s16 indT;

    if (g_bSetParam == 0) {
        indS = p->indS;
    } else {
        indS = g_Free.Shimmer_pow1;
    }
    if (g_bSetParam == 0) {
        indT = p->indT;
    } else {
        indT = g_Free.Shimmer_pow2;
    }
    m[0][0] = (f32) indS * 0.001f + 0.01f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = (f32) indT * 0.007f + 0.07f;
    m[1][2] = 0.0f;
    GXSetIndTexMtx(1, m, 1);
}

// Border quads of the unbounded surface: grid half sizes in grid units; the far edge is
// g45_mul cells out, the near edge g45_mul2, normals follow SetWaterWork45's slope.
#define G45_NXH ((f32) (int) (p->nx >> 1))
#define G45_NXHN ((f32) (-(int) p->nx / 2))
#define G45_NX ((f32) (int) p->nx)
#define G45_NXN ((f32) (-(int) p->nx))
#define G45_NY ((f32) (int) p->ny)
#define G45_NYN ((f32) (-(int) p->ny))
#define G45_NYU ((f32) p->ny)

// One vertex = one inline with all eight values as parameters, normal first: every value is evaluated
// before the first FIFO store (one (f32)(-ny) conversion serves the normal z and the position z), and the
// ny conversion precedes the nx one because the normal arguments come first.
static inline void Vtx45(f32 nx, f32 ny, f32 nz, f32 x, f32 y, f32 z, f32 s, f32 t)
{
    GXPosition3f32(x, y, z);
    GXNormal3f32(nx, ny, nz);
    GXTexCoord2f32(s, t);
}

// Draws the water: lights a dummy model at the surface (5 cloth lights + ambient amb, colour overrides
// applied), copies the screen (below the 56 px border) into draw temp buffer 0xE as the refraction
// texture projected with the camera, adds the bump indirect stage (SetIndMtx), the environment
// specular texture (p->texId, view-space normals) and the mask texture when flag bit 1; then, unless
// flag bit 0, four far border quads (g45_mul cells out) and finally the grid display list.
void Espgen45_TransSub(EspgenWork* w)
{
    static f32 g45_mul = 15.0f;
    static f32 g45_mul2 = 1.0f;
    GxStageWork* st;
    Espgen42Work* p;
    void* buf;
    s32 stage;

    if (!(w->flag & 1) || (w->flag & 2)) {
        return;
    }
    st = &pG->gxStage;
    p = (Espgen42Work*) w->work;
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
    model.pos.x = p->mat[0][3];
    model.pos.y = p->mat[1][3];
    model.pos.z = p->mat[2][3];
    LightMgr.setClothN(&model, 5);
    GXColor amb = p->amb;
    if (g_bColorOverWrite == 1) {
        amb.r = (u8) (g_sr * 255.0f);
        amb.g = (u8) (g_sg * 255.0f);
        amb.b = (u8) (g_sb * 255.0f);
        amb.a = (u8) (g_sa * 255.0f);
    } else if (g_bColorMul == 1) {
        amb.r = (u8) ((f32) amb.r * g_sr);
        amb.g = (u8) ((f32) amb.g * g_sg);
        amb.b = (u8) ((f32) amb.b * g_sb);
        amb.a = (u8) ((f32) amb.a * g_sa);
    }
    commonWaterLightSet(model.LightInfo.pLight, 5, amb.a);
    GXColor white;
    white.r = white.g = white.b = white.a = 0xFF;
    GXSetChanMatColor(4, white);
    GXSetChanAmbColor(4, amb);
    Mtx nrm;
    Mtx mv;
    Mtx tmp;
    PSMTXConcat(pG->Cam.v_mat, p->mat, mv);
    PSMTXCopy(p->mat, tmp);
    tmp[1][1] = p->size * 0.05f + 100.0f;
    PSMTXConcat(pG->Cam.v_mat, tmp, tmp);
    PSMTXInverse(tmp, nrm);
    PSMTXTranspose(nrm, nrm);
    GXLoadNrmMtxImm(nrm, 0);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 4, 5, 0);
    buf = GetDrawTmpBufAddr(0xE);
    if (buf == NULL) {
        pLog->warn(0, 0, "Espgen45() : not enough memory");
        return;
    }
    {
        f32 ofs = 56.0f;
        GXSetTexCopySrc(0, (u32) ofs, (u32) Screen.width, (u32) (Screen.height - ofs));
        GXSetTexCopyDst((u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 1);
        GXCopyTex(buf, 0);
        GXPixModeSync();
        GXInvalidateTexAll();
        {
            GXTexObj tex;
            Mtx tm;
            Mtx pm;
            GXInitTexObj(&tex, buf, (u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 0, 0, 0);
            GXLoadTexObj(&tex, st->texMap);
            C_MTXLightPerspective(pm, pG->Cam.param.fovy, 1.3333334f, 0.5f, -0.6666667f, 0.5f, 0.5f);
            PSMTXConcat(pm, mv, tm);
            GXLoadTexMtxImm(tm, 0x1E, 0);
            GXSetTexCoordGen(st->texCoord, 0, 0, 0x1E);
            GXColor col = p->col;
            if (g_bColorOverWrite == 1) {
                col.r = g_r;
                col.g = g_g;
                col.b = g_b;
                col.a = g_a;
            } else if (g_bColorMul == 1) {
                col.r = (u8) ((f32) col.r * (f32) (int) g_r / 255.0f);
                col.g = (u8) ((f32) col.g * (f32) (int) g_g / 255.0f);
                col.b = (u8) ((f32) col.b * (f32) (int) g_b / 255.0f);
                col.a = (u8) ((f32) col.a * (f32) (int) g_a / 255.0f);
            }
            GXSetTevColor(1, col);
            GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
            GXSetTevColorIn(st->tevStage, 0xF, 2, 8, 0xF);
            if (p->stages > 0) {
                GXSetTevColorOp(st->tevStage, 0, 0, 2, 1, 0);
            } else {
                GXSetTevColorOp(st->tevStage, 0, 0, 0, 1, 0);
            }
            GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
            GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
            stage = st->tevStage;
            st->tevStage++;
            st->texMap++;
            st->texCoord++;
            GXInitTexObj(&tex, p->bump, p->nx, p->ny, 1, 0, 0, 0);
            GXLoadTexObj(&tex, st->texMap);
            GXSetNumIndStages(1);
            GXSetTexCoordGen(st->texCoord, 1, 4, 0x3C);
            GXSetIndTexOrder(0, st->texCoord, st->texMap);
            GXSetIndTexCoordScale(0, 0, 0);
            SetIndMtx_801291F4(p);
            GXSetTevIndWarp(stage, 0, 1, 0, 1);
            st->texCoord++;
            st->texMap++;
            if (p->stages > 1) {
                GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
                GXSetTevColorIn(st->tevStage, 0xF, 0, 0, 0xF);
                GXSetTevColorOp(st->tevStage, 0, 0, 2, 1, 0);
                GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
                GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
                st->tevStage++;
                st->texMap++;
                st->texCoord++;
            }
            if (p->stages > 2) {
                GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
                GXSetTevColorIn(st->tevStage, 0xF, 0, 0, 0xF);
                GXSetTevColorOp(st->tevStage, 0, 0, 2, 1, 0);
                GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
                GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
                st->tevStage++;
                st->texMap++;
                st->texCoord++;
            }
        }
        {
            GXTexObj* tex2 = EspGetTexObj(p->texId, 0);
            if (tex2 == NULL) {
                pLog->err(0, 0, "Espgen45 : TexId[%x] invalid.", p->texId);
                tex2 = &Specular;
            }
            GXLoadTexObj(tex2, st->texMap);
        }
        {
            Mtx ms;
            Mtx mt;
            Mtx m3;
            PSMTXCopy(pG->Cam.v_mat, m3);
            PSMTXInverse(m3, m3);
            PSMTXTranspose(m3, m3);
            PSMTXScale(ms, 1.0f, -0.5f, 0.0f);
            PSMTXTrans(mt, 0.5f, 0.5f, 1.0f);
            PSMTXConcat(ms, m3, m3);
            PSMTXConcat(mt, m3, m3);
            GXLoadTexMtxImm(m3, 0x21, 0);
        }
        GXSetTexCoordGen(st->texCoord, 0, 1, 0x21);
        GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
        GXSetTevColorIn(st->tevStage, 0xF, 0xA, 9, 0);
        GXSetTevColorOp(st->tevStage, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(st->tevStage, 7, 7, 7, 5);
        GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
        st->tevStage++;
        st->texMap++;
        st->texCoord++;
        if ((IGet(g_bSetParam) == 1 && (g_Free.flag & 2)) || (IGet(g_bSetParam) == 0 && (p->flag & 2))) {
            u8 texId;
            EspTexWk* tw;
            if (g_bSetParam == 1) {
                texId = g_Free.Mask_Tex;
            } else {
                texId = p->Mask_Tex;
            }
            tw = EspGetTexWk(texId, 1);
            if (tw == NULL || tw->Owner == 0xD2) {
                pLog->err(0, 0, "ESP : Mask_TexId[%x] no data", texId);
            } else {
                // the same frame slots as the first block's tex/tm: PRE shares their addresses
                GXTexObj tex;
                GXTlutObj tlut;
                TEXDescriptor* td = TEXGet(tw->pTpl, 0);
                TEXHeader* th = td->textureHeader;

                if (th->format == 8 || th->format == 9) {
                    GXInitTexObjCI(&tex, th->data, th->width, th->height, th->format, 0, 0, 0, 1);
                    GXInitTlutObj(&tlut, td->CLUTHeader->data, td->CLUTHeader->format, td->CLUTHeader->numEntries);
                    GXLoadTlut(&tlut, 1);
                } else {
                    GXInitTexObj(&tex, th->data, th->width, th->height, th->format, 0, 0, 0);
                }
                GXLoadTexObj(&tex, st->texMap);
                GXLoadTexMtxImm(tw->mtx, 0x21, 1);
                GXSetTexCoordGen(st->texCoord, 1, 4, 0x21);
                GXSetTevOrder(st->tevStage, st->texCoord, st->texMap, 4);
                GXSetTevColorIn(st->tevStage, 0xF, 0xF, 0xF, 0);
                GXSetTevColorOp(st->tevStage, 0, 0, 0, 1, 0);
                GXSetTevAlphaIn(st->tevStage, 7, 4, 5, 7);
                GXSetTevAlphaOp(st->tevStage, 0, 0, 0, 1, 0);
                st->tevStage++;
                st->texMap++;
                st->texCoord++;
            }
        }
        GXSetNumTevStages(st->tevStage);
        GXSetNumTexGens(st->texCoord);
        if (!(p->flag & 1)) {
            Vec n;
            f32 hx;
            f32 hy;
            f32 inx;
            f32 iny;
            f32 far;

            GXClearVtxDesc();
            GXSetVtxDesc(9, 1);
            GXSetVtxDesc(0xA, 1);
            GXSetVtxDesc(0xD, 1);
            GXSetVtxAttrFmt(0, 9, 1, 4, 0);
            GXSetVtxAttrFmt(0, 0xA, 0, 4, 0);
            GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
            hx = G45_NXH;
            hy = (f32) (int) (p->ny >> 1);
            inx = 1.0f / G45_NX * inv_mul;
            iny = 1.0f / G45_NY * inv_mul;
            far = g45_mul / g45_mul2;
            {
                GXBegin(0x80, 0, 4);
                f32 nx = (g45_mul2 * 0.0f - hx) * inx;
                Vtx45(nx, 0.25f, (G45_NYN * g45_mul2 + hy) * iny * far, G45_NXHN * g45_mul, 0.0f, G45_NYN * 0.5f * g45_mul, 0.0f, 0.0f);
                f32 nz = (g45_mul2 * 0.0f - hy) * iny;
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, (G45_NYN * g45_mul2 + hy) * iny * far, G45_NXH * g45_mul, 0.0f, G45_NYN * 0.5f * g45_mul, 1.0f, 0.0f);
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, nz, G45_NXH * g45_mul2, 0.0f, G45_NYN * 0.5f * g45_mul2, 1.0f, 1.0f);
                n.x = nx;
                n.y = 0.25f;
                n.z = nz;
                Vtx45(n.x, n.y, n.z, G45_NXHN * g45_mul2, 0.0f, G45_NYN * 0.5f * g45_mul2, 0.0f, 1.0f);
            }
            {
                GXBegin(0x80, 0, 4);
                f32 nz = (g45_mul2 * 0.0f - hy) * iny;
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, nz, G45_NXH * g45_mul2, 0.0f, G45_NYN * 0.5f * g45_mul2, 0.0f, 0.0f);
                Vtx45((G45_NX * g45_mul2 - hx) * inx * far, 0.25f, nz, G45_NXH * g45_mul, 0.0f, G45_NYN * 0.5f * g45_mul, 1.0f, 0.0f);
                Vtx45((G45_NX * g45_mul2 - hx) * inx * far, 0.25f, (G45_NY * g45_mul2 - hy) * iny, G45_NXH * g45_mul, 0.0f, G45_NYU * 0.5f * g45_mul, 1.0f, 1.0f);
                n.x = (G45_NX * g45_mul2 - hx) * inx;
                n.y = 0.25f;
                n.z = (G45_NY * g45_mul2 - hy) * iny;
                Vtx45(n.x, n.y, n.z, G45_NXH * g45_mul2, 0.0f, G45_NYU * 0.5f * g45_mul2, 0.0f, 1.0f);
            }
            {
                GXBegin(0x80, 0, 4);
                f32 nx = (g45_mul2 * 0.0f - hx) * inx;
                Vtx45(nx, 0.25f, (G45_NY * g45_mul2 - hy) * iny, G45_NXHN * g45_mul2, 0.0f, G45_NYU * 0.5f * g45_mul2, 0.0f, 1.0f);
                n.x = nx;
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, (G45_NY * g45_mul2 - hy) * iny, G45_NXH * g45_mul2, 0.0f, G45_NYU * 0.5f * g45_mul2, 1.0f, 1.0f);
                Vtx45((G45_NX * g45_mul2 - hx) * inx, 0.25f, (G45_NY * g45_mul2 - hy) * iny * far, G45_NXH * g45_mul, 0.0f, G45_NYU * 0.5f * g45_mul, 1.0f, 0.0f);
                n.z = (G45_NY * g45_mul2 - hy) * iny * far;
                n.y = 0.25f;
                Vtx45(n.x, n.y, n.z, G45_NXHN * g45_mul, 0.0f, G45_NYU * 0.5f * g45_mul, 0.0f, 0.0f);
            }
            {
                GXBegin(0x80, 0, 4);
                f32 nz = (g45_mul2 * 0.0f - hy) * iny;
                f32 nx = (g45_mul2 * 0.0f - hx) * inx;
                Vtx45((G45_NXN * g45_mul2 + hx) * inx * far, 0.25f, nz, G45_NXHN * g45_mul, 0.0f, G45_NYN * 0.5f * g45_mul, 0.0f, 0.0f);
                Vtx45(nx, 0.25f, nz, G45_NXHN * g45_mul2, 0.0f, G45_NYN * 0.5f * g45_mul2, 1.0f, 0.0f);
                Vtx45(nx, 0.25f, (G45_NY * g45_mul2 - hy) * iny, G45_NXHN * g45_mul2, 0.0f, G45_NYU * 0.5f * g45_mul2, 1.0f, 1.0f);
                n.x = (G45_NXN * g45_mul2 + hx) * inx * far;
                n.y = 0.25f;
                n.z = (G45_NY * g45_mul2 - hy) * iny;
                Vtx45(n.x, n.y, n.z, G45_NXHN * g45_mul, 0.0f, G45_NYU * 0.5f * g45_mul, 0.0f, 1.0f);
            }
        }
        GXClearVtxDesc();
        GXSetVtxDesc(9, 3);
        GXSetVtxDesc(10, 3);
        GXSetVtxDesc(13, 1);
        GXSetVtxAttrFmt(0, 9, 1, 4, 0);
        GXSetVtxAttrFmt(0, 10, 0, 4, 0);
        GXSetVtxAttrFmt(0, 13, 1, 4, 0);
        GXSetArray(9, p->pos, sizeof(Vec));
        GXSetArray(10, p->nrm, sizeof(Vec));
        GXCallDisplayList(p->dl, p->dlSize);
        GXSetNumTevStages(1);
        GXSetNumTexGens(0);
        GXSetNumIndStages(0);
        GXSetTevDirect(0);
        GXSetTevDirect(1);
    }
}

// Allocates a controller from the pool and builds a water surface on it (SetWaterWork45).
// Dead-stripped from the DOL (string kept, no pool: STRIP_UNUSED): pulls a generator and sets the
// surface up.
static EspgenWork* SetWater(Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny, f32 rate)
{
    EspgenWork* w;

    if (PullEspgen(&w) == 0) {
        pLog->err(0, 0, "Espgen45 : work pull failed");
        return NULL;
    }
    return SetWaterWork45(w, pos, rot, size, nx, ny, rate);
}

// Builds the surface work: id 0x45, nx x ny cells of `size` units, matrix (with the rotation
// override), allocates hA/hB/pos/nrm/bump and the strip display list (memory group 13), fills the
// zig-zag triangle strip indices/UVs, the flat grid positions (random +-0.2 ripple), the sloped
// normals and zero edge heights. Returns NULL (controller released) when an allocation fails.
EspgenWork* SetWaterWork45(EspgenWork* w, Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny, f32 rate)
{
    Espgen42Work* p = (Espgen42Work*) w->work;
    Mtx m;
    u32 n;
    u8* d;
    int i;
    int j;
    int k;
    f32 fx;
    f32 fy;

    w->id = 0x45;
    p->nx = nx;
    p->ny = ny;
    p->size = size;
    p->damp = 0.05f;
    p->spread = 0.95f;
    p->pos0 = *pos;
    if (IGet(g_bSetParam) == 0) {
        PSMTXScale(p->mat, p->size, p->size * 0.05f + 100.0f, p->size);
    } else {
        RotMatrix(p->mat, &g_Free.ang);
        PSMTXScale(m, p->size, p->size * 0.05f + 100.0f, p->size);
        PSMTXConcat(p->mat, m, p->mat);
    }
    PSMTXTransApply(p->mat, p->mat, p->pos0.x, p->pos0.y, p->pos0.z);
    PSMTXInverse(p->mat, p->inv);
    if (rate == 0.0f) {
        rate = 0.0001f;
    }
    p->mat[1][1] *= rate;
    n = sizeof(f32) * (p->nx + 1) * (p->ny + 1);
#line 1452 "D:/Bio4/Prog/espgen45.cpp"
    p->hA = (f32*) MEM_ALLOC(n, 1, 13);
    if (p->hA == NULL) {
        goto nomem;
    }
    memclr_asm(p->hA, n);
#line 1459 "D:/Bio4/Prog/espgen45.cpp"
    p->hB = (f32*) MEM_ALLOC(n, 1, 13);
    if (p->hB == NULL) {
        goto nomem;
    }
    memclr_asm(p->hB, n);
    n = sizeof(Vec) * (p->nx + 1) * (p->ny + 1);
#line 1468 "D:/Bio4/Prog/espgen45.cpp"
    p->pos = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pos == NULL) {
        goto nomem;
    }
    memclr_asm(p->pos, n);
#line 1475 "D:/Bio4/Prog/espgen45.cpp"
    p->nrm = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->nrm == NULL) {
        goto nomem;
    }
    memclr_asm(p->nrm, n);
#line 1484 "D:/Bio4/Prog/espgen45.cpp"
    p->bump = (u8*) MEM_ALLOC(sizeof(Vec) * p->nx * p->ny, 1, 13);
    if (p->bump == NULL) {
        goto nomem;
    }
    p->dlSize = ((p->nx + 1) * 2 * p->ny * 12 + 0x61) & ~0x1F;
#line 1497 "D:/Bio4/Prog/espgen45.cpp"
    p->dl = (u8*) MEM_ALLOC(p->dlSize, 1, 13);
    if (p->dl == NULL) {
    nomem:
        pLog->err(0, 0, "Espgen45 : not enough memory");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->dl, p->dlSize);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 3);
    GXSetVtxDesc(10, 3);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 10, 0, 4, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    d = p->dl;
    *d = 0;
    d++;
    *d = 0x98;
    d++;
    *(u16*) d = (p->nx + 1) * 2 * p->ny;
    d += 2;
    for (i = 0; i < p->ny; i++) {
        k = i * (p->nx + 1);
        for (j = 0; j < p->nx + 1; j++) {
            *(u16*) d = k;
            d += 2;
            *(u16*) d = k;
            d += 2;
            *(f32*) d = (f32) j / (f32) (p->nx + 1);
            d += 4;
            *(f32*) d = (f32) i / (f32) (p->ny + 1);
            d += 4;
            *(u16*) d = p->nx + (k + 1);
            d += 2;
            *(u16*) d = p->nx + (k + 1);
            d += 2;
            *(f32*) d = (f32) j / (f32) (p->nx + 1);
            d += 4;
            *(f32*) d = (f32) (i + 1) / (f32) (p->ny + 1);
            d += 4;
            k++;
        }
        i++;
        if (i < p->ny) {
            for (j = p->nx; j >= 0; j--) {
                k = i * (p->nx + 1) + j;
                *(u16*) d = k;
                d += 2;
                *(u16*) d = k;
                d += 2;
                *(f32*) d = (f32) j / (f32) (p->nx + 1);
                d += 4;
                *(f32*) d = (f32) i / (f32) (p->ny + 1);
                d += 4;
                *(u16*) d = p->nx + (k + 1);
                d += 2;
                *(u16*) d = p->nx + (k + 1);
                d += 2;
                *(f32*) d = (f32) j / (f32) (p->nx + 1);
                d += 4;
                *(f32*) d = (f32) (i + 1) / (f32) (p->ny + 1);
                d += 4;
            }
        }
    }
    // One counter pair for the init loops: `jj` (inner fRand loop, then the two x edges: it crosses the
    // call, so callee-saved r28) and `i2`/`idx` (fRand rows, `i2 = p->ny` for the far edge, the two y edges).
    fy = 0.0f;
    int jj;
    int i2;
    int idx;
    for (i2 = 0; i2 < p->ny + 1; i2++) {
        idx = i2 * (p->nx + 1);
        fx = 0.0f;
        for (jj = 0; jj < p->nx + 1; jj++) {
            p->pos[idx].x = fx - (f32) (int) (p->nx / 2);
            p->pos[idx].y = fRand1_1() * 0.2f;
            fx += 1.0f;
            p->pos[idx].z = fy - (f32) (int) (p->ny / 2);
            p->nrm[idx].x = 0.0f;
            p->nrm[idx].y = 1.0f;
            p->nrm[idx].z = 0.0f;
            p->hA[idx] = 0.0f;
            p->hB[idx] = 0.0f;
            Vec* n = &p->nrm[idx];
            n->x += ((f32) jj - (f32) (int) (p->nx / 2)) * (1.0f / (f32) (int) p->nx);
            n->z += ((f32) i2 - (f32) (int) (p->ny / 2)) * (1.0f / (f32) (int) p->ny);
            n->y *= 0.25f;
            idx++;
        }
        fy += 1.0f;
    }
    {
        static f32 g45_init_y = 0.0f;
        static f32 g45_init_y2 = 0.0f;
        for (jj = 0; jj < p->nx + 1; jj++) {
            p->pos[jj].y = FGet(g45_init_y);
        }
        i2 = p->ny;
        idx = i2 * (p->nx + 1);
        for (jj = 0; jj < p->nx + 1; jj++) {
            p->pos[idx + jj].y = FGet(g45_init_y);
        }
        // The y edges also go through `idx` (one pseudo across all four loops = the target's r8 in every
        // loop), and the far edge is `idx = row; idx += nx` (the product lands in idx's register, not a temp).
        for (i2 = 0; i2 < p->ny + 1; i2++) {
            idx = i2 * (p->nx + 1);
            p->pos[idx].y = FGet(g45_init_y2);
        }
        for (i2 = 0; i2 < p->ny + 1; i2++) {
            idx = i2 * (p->nx + 1);
            idx += p->nx;
            p->pos[idx].y = FGet(g45_init_y2);
        }
    }
    // Block-local sizes at the tail: local-alloc ties the `nx + 1` temp into them (`addi r30; mullw r30`);
    // the function-level `n` is only the MEM_ALLOC size.
    {
        u32 n2 = sizeof(Vec) * (p->nx + 1) * (p->ny + 1);
        DCStoreRange(p->pos, n2);
        DCStoreRange(p->nrm, n2);
    }
    DCStoreRange(p->bump, sizeof(Vec) * (p->nx + 1) * (p->ny + 1));
    {
        u32 n3 = sizeof(f32) * (p->nx + 1) * (p->ny + 1);
        DCStoreRange(p->hA, n3);
        DCStoreRange(p->hB, n3);
    }
    DCStoreRange(p->dl, p->dlSize);
    return w;
}

// Frees the six grid buffers and clears g_pWater45.
void Espgen45_Destruct(EspgenWork* w)
{
    Espgen42Work* p = (Espgen42Work*) w->work;

    if (p->hA != NULL) {
        Mem_free(p->hA);
        p->hA = NULL;
    }
    if (p->hB != NULL) {
        Mem_free(p->hB);
        p->hB = NULL;
    }
    if (p->pos != NULL) {
        Mem_free(p->pos);
        p->pos = NULL;
    }
    if (p->nrm != NULL) {
        Mem_free(p->nrm);
        p->nrm = NULL;
    }
    if (p->bump != NULL) {
        Mem_free(p->bump);
        p->bump = NULL;
    }
    if (p->dl != NULL) {
        Mem_free(p->dl);
        p->dl = NULL;
    }
    g_pWater45 = NULL;
}

// Builds the water from the effect record: grid WorkSp8[0..1] (default 64, max 184, rounded down to a
// multiple of 8), wave ratio WorkSp8[2], Tool_flg bit 0 = no border quads, bit 0x4000 = mask texture
// MaskTex_id; colour Col_start, ambient Col_d*255, mode Work8[0] (2: damp/spread from Work8[1..2]),
// specular Tex_id, indirect strengths prm.h xCE/xD2, stages Work8[3]. Registers g_pWater45 and runs
// the first move. Returns 0 when the noise texture 0xFE or memory is missing.
int Espgen45_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct)
{
    Espgen42Work* p = (Espgen42Work*) w->work;
    Vec r;
    u32 nx = 0x40;
    u32 ny = 0x40;
    f32 rate;

    if (EspGetTexObj(0xFE, 0) == NULL) {
        pLog->err(0, 0, "Espgen45 : WaterTex(0xfe) not found!");
        return 0;
    }
    if (rec->Tool_flg & 1) {
        p->flag |= 1;
    }
    if (rec->Tool_flg & 0x4000) {
        p->flag |= 2;
        p->Mask_Tex = rec->MaskTex_id;
        p->flag |= 1;
    }
    if (rec->WorkSp8[0] != 0) {
        nx = rec->WorkSp8[0];
        if (nx > 0xB8) {
            nx = 0xB8;
            pLog->warn(0, 0, "ESP_WATER : width > 184");
        }
    }
    if (rec->WorkSp8[1] != 0) {
        ny = rec->WorkSp8[1];
        if (ny > 0xB8) {
            ny = 0xB8;
            pLog->warn(0, 0, "ESP_WATER : height > 184");
        }
    }
    if (nx & 7) {
        u32 n = nx - (nx & 7);
        pLog->warn(0, 0, "ESP_WATER : width (%d -> %d)", nx, n);
        nx = n;
    }
    if (ny & 7) {
        u32 n = ny - (ny & 7);
        pLog->warn(0, 0, "ESP_WATER : height (%d -> %d)", ny, n);
        ny = n;
    }
    rate = 1.0f - (f32) (int) rec->WorkSp8[2] / 255.0f;
    p->rotY = rec->WorkSp8[2];
    PSVECScale(&rec->Ang, &r, 6.28f / 360.0f);
    if (SetWaterWork45(w, (Vec*) &rec->Pos.x, &r, rec->Size_base_x, nx, ny, rate) != 0) {
        p->col.r = rec->Col_start_r;
        p->col.g = rec->Col_start_g;
        p->col.b = rec->Col_start_b;
        p->col.a = rec->Col_start_a;
        p->amb.r = rec->Col_d_r * 255.0f;
        p->amb.g = rec->Col_d_g * 255.0f;
        p->amb.b = rec->Col_d_b * 255.0f;
        p->amb.a = rec->Col_d_a * 255.0f;
        p->mode = rec->Work8[0];
        p->Base_y = p->pos0.y;
        if (p->mode == 2) {
            p->damp = 0.5f - (f32) (s8) rec->Work8[1] * 0.005f;
            if (p->damp > 0.5f) {
                p->damp = 0.5f;
            }
            if (p->damp < 0.0f) {
                p->damp = 0.0f;
            }
            p->spread = 0.99f - (f32) (int) rec->Work8[2] * 0.001f;
        }
        p->texId = rec->Tex_id;
        p->indS = rec->prm.h.xCE;
        p->indT = rec->prm.h.xD2;
        p->stages = rec->Work8[3];
        g_pWater45 = w;
        Espgen45_Move(w);
        return 1;
    }
    return 0;
}

// Room override: 1 = the surface follows the camera target (default), 0 = uses Estgen45SetTargetPos.
void Estgen45SetTargetCamera(int on)
{
    g_bTargetCamera = on;
}

// Room override: 1 = surface height from Estgen45SetHeight, 0 = the record's Base_y.
void Estgen45SetTargetHeight(int on)
{
    g_bTargetHeight = on;
}

// Room override: use the Estgen45SetSize size instead of the record's.
void Estgen45SetSizeOverWrite(int on)
{
    g_bSizeOverWrite = on;
}

// Room override: replace the record colours by the Estgen45SetColor values.
void Estgen45SetColorOverWrite(int on)
{
    g_bColorOverWrite = on;
}

// Room override: multiply the record colours by the Estgen45SetColor values.
void Estgen45SetColorMul(int on)
{
    g_bColorMul = on;
}

// Room override: take Type/wave ratio/damp/spread/indirect/rotation/mask from the Esp4cWork block.
void Estgen45SetParamOverWrite(int on)
{
    g_bSetParam = on;
}

// Sets the override centre of the surface and flags Status_flg[1] bit 0x20.
void Estgen45SetTargetPos(f32 x, f32 z)
{
    FSet(g_Target_x, x);
    FSet(g_Target_z, z);
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

// Sets the override water height.
void Estgen45SetHeight(f32 h)
{
    FSet(g_Target_y, h);
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

// Sets the override cell size.
void Estgen45SetSize(f32 size)
{
    FSet(g_Size, size);
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

// Sets the override tev colour (r,g,b,a) and ambient/scale factors (rs..as, 0..1).
void Estgen45SetColor(u8 r, u8 g, u8 b, u8 a, f32 rs, f32 gs, f32 bs, f32 as)
{
    U8Set(g_r, r);
    U8Set(g_g, g);
    U8Set(g_b, b);
    U8Set(g_a, a);
    FSet(g_sr, rs);
    FSet(g_sg, gs);
    FSet(g_sb, bs);
    FSet(g_sa, as);
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

// Copies the esp4c parameter block used when the parameter override is on.
void Estgen45SetParam(Esp4cWork* w)
{
    g_Free = *w;
    StaFlagOn(pG, STA_ESPGEN45_SET);
}

asm(".section .sdata; .balign 8");
