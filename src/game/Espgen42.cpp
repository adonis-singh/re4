// game/Espgen42.cpp: effect controller 42, the room water surface (see the class note below):
// a height field simulated every frame, lit and drawn with a bump-mapped display list, with
// the AddWaterPower / GetWaterHeight / GetWaterCrossPos entry points the rest of the game uses
// for splashes, floating effects and bullet hits on water.

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

// Effect controller 42: room water surface. A (nx+1) x (ny+1) height field simulated on two
// ping-pong buffers, rendered as triangle strips through a display list with an indirect bump
// texture built every frame from the normals. Shared with the weather water (espgen45):
// AddWaterPower / GetWaterHeight / GetWaterCrossPos test both generators.

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
// game/espgen45.cpp
extern EspgenWork* g_pWater45;

void AddWaterPowerSub(EspgenWork* w);
void GetWaterHeightSub(EspgenWork* w);
void GetWaterCrossPosSub(EspgenWork* w);
void Espgen42_Move00(EspgenWork* w);
void Espgen42_TransSub(EspgenWork* w);
void SetIndMtx(Espgen42Work* p);
EspgenWork* SetWaterWork(EspgenWork* w, Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny, f32 rate);
}

static EspgenWork* g_pWater;
static Vec Chk_pos;
static f32 Height_ret;
static f32 Add_power;
static int Height_find;
static Vec Cross_Chk_pos;
static Vec Cross_Chk_dest;
static Vec Cross_Ret_pos;
static int Cross_find;
int g_bNoWater = 0;

static inline void ISet(int& d, int v) { d = v; }
static inline f32 FGet(f32& d) { return d; }
static inline int IGet(int& d) { return d; }

// Room start: forgets both water generators (42 room water, 45 weather water), resets the
// Espgen45 override state and clears the no-water debug switch.
void EspWaterInit()
{
    Estgen45SetTargetCamera(1);
    g_pWater = NULL;
    g_pWater45 = NULL;
    Espgen45_static_init();
    g_bNoWater = 0;
    Estgen45SetTargetCamera(1);
    Estgen45SetTargetHeight(0);
    Estgen45SetSizeOverWrite(0);
    Estgen45SetColorOverWrite(0);
    Estgen45SetColorMul(0);
    Estgen45SetParamOverWrite(0);
}

// Pushes the height field down around Chk_pos (the cell and its four neighbours). The position is a
// BY-VALUE Vec parameter: integrate.c copies the argument into a stack temp through an address
// pseudo (`addi r11,r1,8; stw 4(r11); stw 8(r11)`) that also feeds the PSMTXMultVec arguments
// (`mr r4,r11`) and dies there; an inline-local `Vec v` gives frame-direct stores and `addi r4,r1,8`.
// `h = p->hB + k` in each arm: jump2 cross-jumps the `slwi; add` tails, so the add sits in another block
// than the load and combine cannot fold it into `lfsux` (target: `add r9,r9,r0; lfs f13,0(r9)`).
static inline void AddWaterPowerCore(EspgenWork* w, Vec v)
{
    Espgen42Work* p = (Espgen42Work*) w->work;
    u32 x;
    u32 z;
    u32 idx;
    int i;

    PSMTXMultVec(p->inv, &v, &v);
    if (v.x < (f32) (-p->nx / 2)) {
        return;
    }
    if (v.z < (f32) (-p->ny / 2)) {
        return;
    }
    if (v.x > (f32) (p->nx / 2)) {
        return;
    }
    if (v.z > (f32) (p->ny / 2)) {
        return;
    }
    z = (u32) (v.z + (f32) (p->ny / 2));
    x = (u32) (v.x + (f32) (p->nx / 2));
    idx = z * (p->nx + 1) + x;
    u32 k = 0;
    f32 pw = 1.0f;
    if (x <= 1) {
        return;
    }
    if (z <= 1) {
        return;
    }
    if (x >= (u32) (p->nx - 2)) {
        return;
    }
    if (z >= (u32) (p->ny - 2)) {
        return;
    }
    for (i = 0; i < 5; i++) {
        switch (i) {
        case 0:
            k = idx - 1;
            pw = 0.8f;
            break;
        case 1:
            k = idx + 1;
            pw = 0.8f;
            break;
        case 2:
            k = idx;
            pw = 1.0f;
            break;
        case 3:
            k = idx - p->nx;
            pw = 0.8f;
            break;
        case 4:
            k = idx + p->nx;
            pw = 0.8f;
            break;
        }
        if (k < (u32) (p->nx * p->ny)) {
            f32* h;
            if (pG->Frame_cnt & 1) {
                h = p->hB + k;
            } else {
                h = p->hA + k;
            }
            *h += FGet(Add_power) * pw;
        }
    }
}

// The 0x45 branch is a hand-written second copy, not the same inline: its `pw` assignments go through a
// temporary (`FSet(pw, 0.8f)` = a f32 parameter), which the loop optimiser hoists as `lfs f11`/`fmr f10,f12`
// with `fmr f12,fN` in the cases; the 0x42 copy keeps `lfs` in the cases with only the `lis` hoisted.
static inline void AddWaterPowerCore45(EspgenWork* w, Vec v)
{
    Espgen42Work* p = (Espgen42Work*) w->work;
    u32 x;
    u32 z;
    u32 idx;
    int i;

    PSMTXMultVec(p->inv, &v, &v);
    if (v.x < (f32) (-p->nx / 2)) {
        return;
    }
    if (v.z < (f32) (-p->ny / 2)) {
        return;
    }
    if (v.x > (f32) (p->nx / 2)) {
        return;
    }
    if (v.z > (f32) (p->ny / 2)) {
        return;
    }
    z = (u32) (v.z + (f32) (p->ny / 2));
    x = (u32) (v.x + (f32) (p->nx / 2));
    idx = z * (p->nx + 1) + x;
    u32 k = 0;
    f32 pw = 1.0f;
    if (x <= 1) {
        return;
    }
    if (z <= 1) {
        return;
    }
    if (x >= (u32) (p->nx - 2)) {
        return;
    }
    if (z >= (u32) (p->ny - 2)) {
        return;
    }
    for (i = 0; i < 5; i++) {
        switch (i) {
        case 0:
            k = idx - 1;
            FSet(pw, 0.8f);
            break;
        case 1:
            k = idx + 1;
            FSet(pw, 0.8f);
            break;
        case 2:
            k = idx;
            FSet(pw, 1.0f);
            break;
        case 3:
            k = idx - p->nx;
            FSet(pw, 0.8f);
            break;
        case 4:
            k = idx + p->nx;
            FSet(pw, 0.8f);
            break;
        }
        if (k < (u32) (p->nx * p->ny)) {
            f32* h;
            if (pG->Frame_cnt & 1) {
                h = p->hB + k;
            } else {
                h = p->hA + k;
            }
            *h += FGet(Add_power) * pw;
        }
    }
}

// Applies the pending Add_power at Chk_pos to generator `w` (id 0x42 or 0x45 layout).
void AddWaterPowerSub(EspgenWork* w)
{
    if (w->id == 0x42) {
        AddWaterPowerCore(w, Chk_pos);
    } else if (w->id == 0x45) {
        AddWaterPowerCore45(w, Chk_pos);
    }
}

// Public splash entry (footsteps, bullets, bodies): pushes the water height field down by
// power x 5 at `pos` on every live water surface. No-op unless a water surface exists this frame
// (Status_flg[0] 0x200).
void AddWaterPower(Vec* pos, f32 power)
{
    if (StaFlagChk(pG, STA_WATER_ALIVE)) {
        Height_find = 0;
        FSet(Add_power, power * 5.0f);
        Chk_pos = *pos;
        EspgenWork* w = g_pWater;
        if (w != NULL && (w->flag & 1) && !(w->flag & 2)) {
            AddWaterPowerSub(w);
        }
        if (g_pWater45 != NULL && (g_pWater45->flag & 1) && !(g_pWater45->flag & 2)) {
            AddWaterPowerSub(g_pWater45);
        }
    }
}

// Same shape as AddWaterPowerSub: a by-value Vec inline called once per id; jump2 cross-jumps the
// two copies into one body (w allocated before p: r30/r29).
static inline void GetWaterHeightCore(EspgenWork* w, Vec v)
{
    Espgen42Work* p = (Espgen42Work*) w->work;

    PSMTXMultVec(p->inv, &v, &v);
    if (v.x < (f32) (-p->nx / 2)) {
        return;
    }
    if (v.z < (f32) (-p->ny / 2)) {
        return;
    }
    if (v.x > (f32) (p->nx / 2)) {
        return;
    }
    if (v.z > (f32) (p->ny / 2)) {
        return;
    }
    v.y = 0.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (v.y > Height_ret) {
        Height_ret = v.y;
    }
    Height_find = 1;
}

// Runs the height test for Chk_pos on generator `w` (only ids 0x42 / 0x45).
void GetWaterHeightSub(EspgenWork* w)
{
    if (w->id == 0x42) {
        GetWaterHeightCore(w, Chk_pos);
    } else if (w->id == 0x45) {
        GetWaterHeightCore(w, Chk_pos);
    }
}

// Debug switch: makes GetWaterHeight report "no water" everywhere.
void Espgen42SetNoWater(int on)
{
    g_bNoWater = on;
}

// Surface height under `pos`: 1 and *height when the point (in grid space) lies inside the room
// water or the weather water (an unbounded weather surface answers its plane height); 0 when no
// live water covers it or no water exists this frame.
int GetWaterHeight(Vec* pos, f32* height)
{
    if (!StaFlagChk(pG, STA_WATER_ALIVE)) {
        return 0;
    }
    if (g_bNoWater == 1) {
        return 0;
    }
    ISet(Height_find, 0);
    FSet(Height_ret, -100000000.0f);
    Chk_pos = *pos;
    if (g_pWater != NULL) {
        if (!(g_pWater->flag & 1) || (g_pWater->flag & 2)) {
            if (g_pWater45 != NULL) {
                if (!(g_pWater45->flag & 1) || (g_pWater45->flag & 2)) {
                    return 0;
                }
            }
        }
    }
    if (g_pWater != NULL && (g_pWater->flag & 1) && !(g_pWater->flag & 2)) {
        GetWaterHeightSub(g_pWater);
    }
    if (g_pWater45 != NULL && (g_pWater45->flag & 1) && !(g_pWater45->flag & 2)) {
        Espgen42Work* p = (Espgen42Work*) g_pWater45->work;
        if (p->flag & 1) {
            GetWaterHeightSub(g_pWater45);
        } else {
            Vec v = {0.0f, 0.0f, 0.0f};
            PSMTXMultVec(p->mat, &v, &v);
            Height_find = 1;
            Height_ret = v.y;
        }
    }
    *height = Height_ret;
    return Height_find;
}

// Intersects the segment Cross_Chk_pos -> Cross_Chk_dest with the room water plane (id 0x42)
// and stores the hit in Cross_Ret_pos when it lies inside the grid.
void GetWaterCrossPosSub(EspgenWork* w)
{
    Espgen42Work* p;
    Vec d;
    Vec hit;
    Vec v;
    Vec v2;
    f32 t;

    if (w->id == 0x42) {
        PSVECSubtract(&Cross_Chk_dest, &Cross_Chk_pos, &d);
        p = (Espgen42Work*) w->work;
        v.z = 0.0f;
        v.y = 0.0f;
        v.x = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        t = (v.y - Cross_Chk_pos.y) / (Cross_Chk_dest.y - Cross_Chk_pos.y);
        if (t < 0.0f) {
            return;
        }
        PSVECScale(&d, &d, t);
        PSVECAdd(&Cross_Chk_pos, &d, &hit);
        PSMTXMultVec(p->inv, &hit, &v);
        if (v.x < (f32) (-p->nx / 2)) {
            return;
        }
        if (v.z < (f32) (-p->ny / 2)) {
            return;
        }
        if (v.x > (f32) (p->nx / 2)) {
            return;
        }
        if (v.z > (f32) (p->ny / 2)) {
            return;
        }
        Cross_Ret_pos = hit;
        Cross_find = 1;
    } else if (w->id == 0x45) {
        PSVECSubtract(&Cross_Chk_dest, &Cross_Chk_pos, &d);
        p = (Espgen42Work*) w->work;
        v2.z = 0.0f;
        v2.y = 0.0f;
        v2.x = 0.0f;
        PSMTXMultVec(p->mat, &v2, &v2);
        t = (v2.y - Cross_Chk_pos.y) / (Cross_Chk_dest.y - Cross_Chk_pos.y);
        if (t < 0.0f) {
            return;
        }
        PSVECScale(&d, &d, t);
        PSVECAdd(&Cross_Chk_pos, &d, &hit);
        if (p->flag & 1) {
            PSMTXMultVec(p->inv, &hit, &v2);
            if (v2.x < (f32) (-p->nx / 2)) {
                return;
            }
            if (v2.z < (f32) (-p->ny / 2)) {
                return;
            }
            if (v2.x > (f32) (p->nx / 2)) {
                return;
            }
            if (v2.z > (f32) (p->ny / 2)) {
                return;
            }
        }
        Cross_Ret_pos = hit;
        Cross_find = 1;
    }
}

// Where the segment pos -> pos + dir crosses a live water surface: 1 and *out on a hit
// (bullet splashes, item drops), 0 otherwise.
int GetWaterCrossPos(Vec* pos, Vec* dir, Vec* out)
{
    if (!StaFlagChk(pG, STA_WATER_ALIVE)) {
        return 0;
    }
    if (dir->x == 0.0f && dir->y == 0.0f && dir->z == 0.0f) {
        return 0;
    }
    ISet(Cross_find, 0);
    Cross_Chk_pos = *pos;
    PSVECAdd(pos, dir, &Cross_Chk_dest);
    if (g_pWater != NULL) {
        if (!(g_pWater->flag & 1) || (g_pWater->flag & 2)) {
            if (g_pWater45 != NULL) {
                if (!(g_pWater45->flag & 1) || (g_pWater45->flag & 2)) {
                    return 0;
                }
            }
        }
    }
    if (g_pWater != NULL && (g_pWater->flag & 1) && !(g_pWater->flag & 2)) {
        GetWaterCrossPosSub(g_pWater);
    }
    if (g_pWater45 != NULL && (g_pWater45->flag & 1) && !(g_pWater45->flag & 2)) {
        GetWaterCrossPosSub(g_pWater45);
    }
    *out = Cross_Ret_pos;
    return IGet(Cross_find);
}

// Bump texture (I8, 8x4 tiles) index of grid point (x, y). x/8 before y/4 (the two signed divisions are
// separate blocks, so their order is the source order) and `(y / 4) << 5`: with `* 32` fold would
// reassociate the constant onto `(w1) >> 3` and hoist `(w1 >> 3) * 32`; the target keeps `srwi` in the loop.
#define BUMP_INDEX(x, y, w1) (((x) / 8) * 32 + (((y) / 4) << 5) * ((w1) >> 3) + (((y) & 3) << 3) + ((x) & 7))
// Noise texture (0xFE) index of grid point (x, y).
#define NOISE_INDEX(x, y) ((((y) << 6) & 0xB00) + (((x) << 2) & 0xA0) + (((y) & 3) << 3) + ((x) & 7))

// u8 -> f32 through GQR2 from a stack byte (the compiler only emits psq_l from its own fpmem slot). volatile (no
// memory clobber): a volatile asm is a scheduling barrier for everything in RTL order around it, which is what puts
// the pos/cur address adds before the noise lbzx and the neighbour loads after it in the target's loop A.
// Loads straight into the destination variable (no statement-expression temp): the target's `psq_l f10; fsubs f10,f10`
// is one pseudo, the function-level `n`.
#define PSQ_L_U8_TO(dst, p) asm volatile("psq_l %0,0(%1),1,2" : "=f"(dst) : "b"(p), "m"(*(p)))

// Step 0, every frame: the wave simulation. Sets Status_flg[0] 0x200 (water present), then for
// every interior grid point integrates the two height buffers (neighbour sum spring, damping
// 0.92) plus the frame's noise texture (0xFE, 60 frames), writes the vertex heights, the
// normals and the I8 bump texture (tilted by grid position so the edges shade flat). Mode 1 is
// the cheaper single-pass variant; in the effect tool the B button drops the whole surface.
void Espgen42_Move00(EspgenWork* w)
{
    static f32 wt_pow = 10.0f;
    // p at the declaration (w's only use): combine folds the parameter copy into `p = (plus r3 20)` AFTER the
    // function-begin note, so alias.c's base for p is the hard reg r3, which the later `li r3,0`s reset to 0
    // (unknown). With an unknown base every p-based load conflicts with the frame stores of `v` and the stores
    // through hA/hB/next: loop B's `lhz nx` for v.z issues after `stfs v.x/v.y` and `lwz pos` after the hB
    // stores, as in the target. `p = w->work` after the tex call keeps w's pseudo (live across the call), p's
    // base is then the argument ADDRESS and the loads float above the frame stores (-44 words).
    Espgen42Work* p = (Espgen42Work*) w->work;
    Vec d0;
    Vec d1;
    Vec v;
    Vec d2;
    Vec d3;
    u8 tmp;
    GXTexObj* tex;
    u8* noise;
    f32* c;
    u32 frame;
    int i;
    int j;
    int k;
    int nz;   // noise index / byte of both loops: one function-level pseudo (see loop A)
    // loop A's `j / 8` and loop B's `(i / 4) << 5`: one function-level pseudo (r10 in both loops; a block-local
    // `(i / 4) << 5` would be tied into the `mullw` by local-alloc, the target ties the nx term).
    int jx;
    // loop B's `k + p->nx` (before the call) and `p->nx` (after it): one function-level pseudo (r11 in both places,
    // `-dl` says "set 2 times; dies in 2 places" = no local-alloc tie, so the Z block's `addi r9,r11,1` starts a
    // fresh r9 chain as in the target).
    int mx;
    // noise value of both loops: also one function-level pseudo (global alloc, f10 in both loops). A block-local `n`
    // ties to the loop-A psq_l output / loop-B frsp result and permutes the loop-B FPR names (f12/f13/f0) and the
    // sched2 slots of `lfs 4(r9)` / `stw r22 | addi r3` (34 -> 11 words).
    f32 n;
    // normal pointer `&nrm[k]` of both loops: one function-level pseudo (36 refs / 168 insns, 1.07) ranks above loop A's
    // k*12 giv (26 / 102, 1.02), loop B's k*4 giv (35 / 167, 1.05) and k (32 / 217, 0.74) in global alloc and takes r30
    // for both loops; the givs then open r31, k gets r28. A block-local pointer per loop (18 / 110, 0.65) came after k.
    Vec* nk;
    // `j & 7` of both loops (noise index and bump index): one function-level pseudo (18 refs / 156 insns) ranks above
    // `i` (54 / 698) in global alloc and takes r24, i r23, (k-nx)*12 r26; a block-local `j & 7` per loop (9 refs) came
    // after i, which then took r24. u8, not int: the narrow store is one more loop.c insn in loop B (176 -> 177, the
    // 4.0 pool pair must stay for the outer pass: 44*2*2 = 176 < 177); combine strips the zero-extension at both uses.
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
    u32 ny = p->ny;
    f32 hx = (f32) (int) (nx / 2);
    f32 hy = (f32) (int) (ny / 2);
    f32 inx = 1.0f / (f32) (int) nx;
    f32 iny = 1.0f / (f32) (int) ny;
    if (p->mode != 1) {
        if (DbgFlagChk(pG, DBG_IN_ESP_TOOL) && (Joy[0].on & 0x100)) {
            // The index is the loop variable `k` (target `lwz r28` = k's register, base+index `lfsx f0,hB,k4`).
            k = (int) ((f32) (int) (nx * ny) * 0.5f);
            // Byte offset in a variable: inside an address `p->hB[k]` expands to `(plus (mult k 4) hB)` (expr.c
            // both_summands puts a MULT first) = `lfsx k4,hB`; a register index keeps `(plus hB k4)` = `lfsx hB,k4`.
            u32 k4 = k * 4;
            *(f32*) ((u8*) p->hB + k4) -= wt_pow;
        }
        f32 damp = p->damp;
        f32 cdamp = 2.0f - damp * 4.0f;
        f32 spread = p->spread;
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
            k = i * (nx + 1);   // two statements: the product lands in k's register (`mullw r28; addi r28,r28,1`)
            k++;
            for (j = 1; j < (int) nx; j++) {
                int i3 = (i & 3) << 3;   // set before the dead test below, so loop.c still hoists it (maybe_never)
                // The k*4 giv is discovered BEFORE the k*12 giv (this statement precedes the dead test's `k * 12`): loop.c
                // emits the giv inits in bl->giv order = reverse discovery, so the preheader is `mr r31,k12 | slwi r27,k,2`
                // (k4 init last). With `c += k` as the first k*4 use (after the test) the two inits were swapped.
                u32 k4 = k * 4;
                // COMPILER-DIFF: candidate (loop.c insn_count): dead test, +4 real insns at loop time (lwz/cmpwi/bne/li;
                // gone by jump2). With 130 (not 126) insns the 0.25 pool pair is "not desirable" in the inner loop
                // (threshold 71 - 3*13 moves = 32, 32*2*2 = 128 < 130) and the OUTER scan hoists it into its
                // preheader (f17); window 129..152. The compare must not read a giv: a `k * 12` compare gave loop A's
                // k*12 giv +3 depth-weighted refs (29 / 102, 1.14) and ranked it above the shared `nk` pointer (1.07),
                // which then lost r30 to it.
                if (p->mode == 3) c = NULL;
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
                f32 h = damp * sum + cdamp * cur[k];
                h -= next[k];
                h = n * 0.0002f + h;
                h *= spread;
                pv->y = next[k] = h;   // the pos address is computed before the next[k] store (target `lwz pos` early)
                Vec* nrm = p->nrm;   // before the v.x/v.z reads: kept across the call (`lfsx nrm[k].x`, `4(nrm+k*12)`)
                v.x = p->pos[k - 1].y - p->pos[k + 1].y;
                v.y = 2.0f;
                v.z = p->pos[k - nx].y - p->pos[k + nx].y;
                nk = &nrm[k];   // the function-level pointer (see its declaration); `nrm[k].y/.z` below fold onto it in cse
                PSVECNormalize(&v, nk);
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
        // 4.0 pool pair (threshold 71 - 3 per moved insn), so 4.0 stays for the outer pass (f20, outer preheader) and
        // 0.0018 moves in the inner pass 2 (f25). A codeless asm set of `i` used after the inner loop is that extra
        // moved insn (no register: it takes the free r19); the input-only asm is +1 loop.c insn_count (175 -> 177;
        // 44*4 = 176 must be below it). Both emit nothing. The real construct is unknown.
        int dead;
        for (i = 1; i < p->ny; i++) {
            k = i * (p->nx + 1);
            for (j = 1; j < p->nx; j++) {
                asm("" : "=r"(dead) : "r"(i));
                asm("" : : "r"(i));
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
                PSVECNormalize(&v, nk);
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
                    // at t2 and the xoris before the add: 45 words in the Z block. What the original had there is unknown.
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

// Espgen move entry for id 0x42: runs the step function unless Stop_flg 0x40000 freezes water.
void Espgen42_Move(EspgenWork* w)
{
    static void (*Espgen42MoveTbl[])(EspgenWork*) = {Espgen42_Move00};

    if (SpfFlagChk(pG, SPF_WATER)) {
        return;
    }
    Espgen42MoveTbl[w->step](w);
}

// Queues Espgen42_TransSub in the world OT (0x10, layer 1, priority 0x80) while the generator is
// live and not suspended.
void Espgen42_Trans(EspgenWork* w)
{
    if ((w->flag & 1) && !(w->flag & 2)) {
        AddOtDirect(0x10, w, (void (*)()) Espgen42_TransSub, 1, 0x80, NULL, 0.0f);
    }
}

// Loads indirect texture matrix 1 with the bump scale (indS x 0.001 + 0.01, indT x 0.007 + 0.07).
void SetIndMtx(Espgen42Work* p)
{
    f32 m[2][3];

    m[0][0] = (f32) (s16) p->indS * 0.001f + 0.01f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = (f32) (s16) p->indT * 0.007f + 0.07f;
    m[1][2] = 0.0f;
    GXSetIndTexMtx(1, m, 1);
}

// Draws the water: sets up a temporary model with 5 lights (commonWaterLightSet), the material /
// ambient colours, position and normal matrices, the water texture (texId) with the bump map as
// an indirect stage plus `stages` extra TEV stages, then calls the pre-built display list of
// triangle strips; restores the TEV state afterwards.
void Espgen42_TransSub(EspgenWork* w)
{
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
    commonWaterLightSet(model.LightInfo.pLight, 5, p->amb.a);
    GXColor white;
    white.r = white.g = white.b = white.a = 0xFF;
    GXSetChanMatColor(4, white);
    GXSetChanAmbColor(4, p->amb);
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
        pLog->warn(0, 0, "Espgen42() : not enough memory");
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
            GXSetTevColor(1, p->col);
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
            SetIndMtx(p);
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
                pLog->err(0, 0, "Espgen42 : TexId[%x] invalid.", p->texId);
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
        GXSetNumTevStages(st->tevStage);
        GXSetNumTexGens(st->texCoord);
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

// Dead-stripped from the DOL (string kept): pulls a generator and sets the surface up.
static EspgenWork* SetWater(Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny, f32 rate)
{
    EspgenWork* w;

    if (PullEspgen(&w) == 0) {
        pLog->err(0, 0, "Espgen42 : work pull failed");
        return NULL;
    }
    return SetWaterWork(w, pos, rot, size, nx, ny, rate);
}

// Builds an nx x ny water grid of cell `size` at pos / rot (y scaled by size x 0.05 + 100, then
// `rate`): allocates the height, position, normal, bump buffers and the display list of
// (nx + 1) x 2 strip vertices per row with texture coordinates, and fills the flat start state.
// Returns NULL (and releases the generator) on memory failure.
EspgenWork* SetWaterWork(EspgenWork* w, Vec* pos, Vec* rot, f32 size, u32 nx, u32 ny, f32 rate)
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

    w->id = 0x42;
    p->damp = 0.05f;
    p->spread = 0.95f;
    p->nx = nx;
    p->ny = ny;
    p->size = size;
    RotMatrix(p->mat, rot);
    PSMTXScale(m, p->size, p->size * 0.05f + 100.0f, p->size);
    PSMTXConcat(p->mat, m, p->mat);
    PSMTXTransApply(p->mat, p->mat, pos->x, pos->y, pos->z);
    PSMTXInverse(p->mat, p->inv);
    if (rate == 0.0f) {
        rate = 0.0001f;
    }
    p->mat[1][1] *= rate;
    n = sizeof(f32) * (p->nx + 1) * (p->ny + 1);
#line 1050 "D:/Bio4/Prog/Espgen42.cpp"
    p->hA = (f32*) MEM_ALLOC(n, 1, 13);
    if (p->hA == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->hA, n);
#line 1057 "D:/Bio4/Prog/Espgen42.cpp"
    p->hB = (f32*) MEM_ALLOC(n, 1, 13);
    if (p->hB == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->hB, n);
    n = sizeof(Vec) * (p->nx + 1) * (p->ny + 1);
#line 1066 "D:/Bio4/Prog/Espgen42.cpp"
    p->pos = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->pos == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->pos, n);
#line 1073 "D:/Bio4/Prog/Espgen42.cpp"
    p->nrm = (Vec*) MEM_ALLOC(n, 1, 13);
    if (p->nrm == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    memclr_asm(p->nrm, n);
#line 1082 "D:/Bio4/Prog/Espgen42.cpp"
    p->bump = (u8*) MEM_ALLOC(sizeof(Vec) * p->nx * p->ny, 1, 13);
    if (p->bump == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
        PushEspgen(w);
        return NULL;
    }
    p->dlSize = ((p->nx + 1) * 2 * p->ny * 12 + 0x61) & ~0x1F;
#line 1095 "D:/Bio4/Prog/Espgen42.cpp"
    p->dl = (u8*) MEM_ALLOC(p->dlSize, 1, 13);
    if (p->dl == NULL) {
        pLog->err(0, 0, "Eg42:not mem");
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
            p->pos[idx].z = fy - (f32) (int) (p->ny / 2);
            p->nrm[idx].x = 0.0f;
            p->nrm[idx].y = 1.0f;
            p->nrm[idx].z = 0.0f;
            p->hA[idx] = 0.0f;
            p->hB[idx] = 0.0f;
            fx += 1.0f;
            idx++;
        }
        fy += 1.0f;
    }
    {
        static f32 g42_init_y = 0.0f;
        static f32 g42_init_y2 = 0.0f;
        for (jj = 0; jj < p->nx + 1; jj++) {
            p->pos[jj].y = FGet(g42_init_y);
        }
        i2 = p->ny;
        idx = i2 * (p->nx + 1);
        for (jj = 0; jj < p->nx + 1; jj++) {
            p->pos[idx + jj].y = FGet(g42_init_y);
        }
        // The y edges also go through `idx` (one pseudo across all four loops = the target's r8 in every
        // loop), and the far edge is `idx = row; idx += nx` (the product lands in idx's register, not a temp).
        for (i2 = 0; i2 < p->ny + 1; i2++) {
            idx = i2 * (p->nx + 1);
            p->pos[idx].y = FGet(g42_init_y2);
        }
        for (i2 = 0; i2 < p->ny + 1; i2++) {
            idx = i2 * (p->nx + 1);
            idx += p->nx;
            p->pos[idx].y = FGet(g42_init_y2);
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

// Frees all grid buffers and forgets the room water generator.
void Espgen42_Destruct(EspgenWork* w)
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
    g_pWater = NULL;
}

// Espgen SetFreeWork for id 0x42 (room water from the room's effect data): grid size WorkSp8[0..1]
// (default 64, max 184, rounded down to 8), height rate WorkSp8[2], colours / ambient from the
// record, mode Work8[0] (2: damp / spread from Work8[1..2]), texture Tex_id, bump parameters
// prm 0xCE / 0xD2, extra TEV stages Work8[3]. Requires the noise texture 0xFE. Runs one move
// step at once.
int Espgen42_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct)
{
    Espgen42Work* p = (Espgen42Work*) w->work;
    Vec r;
    u32 nx = 0x40;
    u32 ny = 0x40;
    f32 rate;

    if (EspGetTexObj(0xFE, 0) == NULL) {
        pLog->err(0, 0, "Espgen42 : WaterTex(0xfe) not found!");
        return 0;
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
    PSVECScale(&rec->Ang, &r, 6.28f / 360.0f);
    if (SetWaterWork(w, (Vec*) &rec->Pos.x, &r, rec->Size_base_x, nx, ny, rate) != NULL) {
        p->col.r = rec->Col_start_r;
        p->col.g = rec->Col_start_g;
        p->col.b = rec->Col_start_b;
        p->col.a = rec->Col_start_a;
        p->amb.r = rec->Col_d_r * 255.0f;
        p->amb.g = rec->Col_d_g * 255.0f;
        p->amb.b = rec->Col_d_b * 255.0f;
        p->amb.a = rec->Col_d_a * 255.0f;
        p->mode = rec->Work8[0];
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
        g_pWater = w;
        Espgen42_Move(w);
        return 1;
    }
    return 0;
}

asm(".section .sdata; .balign 8");
