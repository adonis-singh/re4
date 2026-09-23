// game/pendulum.cpp: pendulum / cloth chain physics. A chain is a set of model parts (links)
// with neighbour tables; every link keeps its world position, speed and rest length in the
// parts' cParts::Pen (PEN_INFO). PenClothMove/Move2/Move3 are three variants of the same
// simulation (gravity + wind, angle limit, distance constraints with collision volumes, matrix
// update); the pl_cloth / em_cloth / obj units pick one per accessory.

#include "pendulum.h"
#include "pl_cloth.h"
#include "atari.h"
#include "model.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "dbmodule.h"

extern "C" {
// static but declared with C linkage: Bio4.sym names it unmangled
static void PenClothReset(cModel* m, PenCloth* c);
}

Vec GlobalWind = {0.0f, 0.0f, 20.0f};
f32 GlobalWindAdd = 1.0471976f;

// The parts of a chain: through the parts table when the chain owner filled one, else the model's
// parts list. The macro re-reads `no` in each arm (the link loops load `*pp` twice); the inline
// takes an index already in a register (neighbour lookups).
#define PEN_PARTS(m, c, no) ((c)->pPtbl ? (c)->pPtbl[no] : (m)->getPartsPtr(no))
// Parts `no` of the chain owner (through pPtbl when the owner supplied a parts table).
static inline cParts* penPartsNo(cModel* m, PenCloth* c, int no)
{
    if (c->pPtbl) {
        return c->pPtbl[no];
    }
    return m->getPartsPtr(no);
}

// wind = GlobalWind * rate. Inline: the frame address goes straight into the argument register,
// so the loop's own &wind pseudo is not unified with this one.
static inline void penWindScale(Vec* wind, f32 rate)
{
    PSVECScale(&GlobalWind, wind, rate);
}


// Collision of the link p0-p1 against the volumes: the border variant keeps the link end on
// the sphere surface, the plain one pushes it out.
#define PEN_AT_CK(c, p0, p1, at) \
    (((c)->Flag & 0x80) ? penClothAtCkBorder(p0, p1, at) : penClothAtCk(p0, p1, at))

// COMPILER-DIFF: codeless anchors (see docs/research/ "DOL pendulum closer pass 2"). The final loop's
// `if (uw) hit = PEN_AT_CK(..&uw->Pos..); else hit = PEN_AT_CK(..&parts->worldPos..);` diamond is
// cross-jumped by the original the other way round from ours: the two penClothAtCk tails are merged
// (`b` into the else arm's `mr r5; bl penClothAtCk`) and both penClothAtCkBorder tails are kept. jump2
// only produces that when (1) a real insn follows the if arm's inner join so its Border jump is not
// tensioned to the outer join before the AtCk jump has been cross-jumped, and (2) a real insn follows
// each penClothAtCk call so find_basic_blocks does not put its `(use 0)` nop between the call and the
// label. Empty `asm("" : "+r"(hit))` statements are such insns and emit nothing; they must all come
// from one source line because find_cross_jump compares the asms' line numbers, hence the macro.
#define PEN_ANCHOR(v) asm("" : "+r"(v))
#define PEN_AT_CK_DIAMOND(c, w, uw, parts, at, hit)                                       \
    if (uw) {                                                                             \
        if ((c)->Flag & 0x80) {                                                          \
            hit = penClothAtCkBorder(&(w)->Pos, &(uw)->Pos, at);                          \
        } else {                                                                          \
            hit = penClothAtCk(&(w)->Pos, &(uw)->Pos, at);                                \
            PEN_ANCHOR(hit);                                                              \
        }                                                                                 \
        PEN_ANCHOR(hit);                                                                  \
    } else {                                                                              \
        if ((c)->Flag & 0x80) {                                                          \
            hit = penClothAtCkBorder(&(w)->Pos, &(parts)->world, at);                  \
        } else {                                                                          \
            hit = penClothAtCk(&(w)->Pos, &(parts)->world, at);                        \
            PEN_ANCHOR(hit);                                                              \
            PEN_ANCHOR(hit);                                                              \
        }                                                                                 \
    }

// Keep the link end above the floor; a link that landed exactly under its upper neighbour is
// jittered so the constraint solver gets a direction. A do-while body: its loop notes put the
// `w` references inside at loop depth + 1, which is what ranks `w` (r31) above the PRE'd &v
// pseudo in global-alloc for all three Move functions (PEN_FLOOR_CK2/3 and PEN_FIX are plain
// blocks: as do-whiles they push `uw` above `w` in Move3).
#define PEN_FLOOR_CK(m, c, w, i, floorY)                                                  \
    do { if (!((c)->Flag & 0x100)) {                                                          \
        if ((w)->Pos.y < floorY) {                                                        \
            (w)->Pos.y = floorY;                                                          \
            if ((c)->pParent[i] < 0xFF) {                                                     \
                PEN_INFO* uw = &penPartsNo(m, c, (c)->pParent[i])->Pen;                    \
                if ((w)->Pos.x == uw->Pos.x && (w)->Pos.z == uw->Pos.z) {                 \
                    (w)->Pos.x += fRand1_1();                                             \
                    (w)->Pos.z += fRand1_1();                                             \
                }                                                                         \
            }                                                                             \
        }                                                                                 \
    } } while (0)

// Same with the upper work already known (NULL for a root link).
#define PEN_FLOOR_CK2(c, w, uw, floorY)                                                   \
    if (!((c)->Flag & 0x100)) {                                                          \
        if ((w)->Pos.y < floorY) {                                                        \
            (w)->Pos.y = floorY;                                                          \
            if (uw) {                                                                     \
                if ((w)->Pos.x == (uw)->Pos.x && (w)->Pos.z == (uw)->Pos.z) {             \
                    (w)->Pos.x += fRand1_1();                                             \
                    (w)->Pos.z += fRand1_1();                                             \
                }                                                                         \
            }                                                                             \
        }                                                                                 \
    }

// Move3: both ends of a constraint are kept above the floor.
#define PEN_FLOOR_CK3(c, w, uw, floorY)                                                   \
    if (!((c)->Flag & 0x100)) {                                                          \
        if ((w)->Pos.y < floorY) {                                                        \
            (w)->Pos.y = floorY;                                                          \
            if (uw) {                                                                     \
                if ((w)->Pos.x == (uw)->Pos.x && (w)->Pos.z == (uw)->Pos.z) {             \
                    (w)->Pos.x += fRand1_1();                                             \
                    (w)->Pos.z += fRand1_1();                                             \
                }                                                                         \
            }                                                                             \
        }                                                                                 \
        if ((uw)->Pos.y < floorY) {                                                       \
            (uw)->Pos.y = floorY;                                                         \
        }                                                                                 \
    }

#define PEN_FIX(w)                                                                        \
    if ((w)->Flag & 1) {                                                                 \
        (w)->Pos = (w)->Fix_pos;                                                           \
    }

// Set up the links of a chain: the rest direction / length of every link and the half distances
// to its side neighbours.
void PenClothSet(cModel* m, PenCloth* c, f32 min_len)
{
    Vec v;
    cParts* parts;
    cParts* n;   // one variable for the neighbour of both arms (it takes r12 in the original)
    PEN_INFO* w;
    u32 i;

    if (c->Num == 0) {
        return;
    }
    for (i = 0; i < c->Num; i++) {
        if (c->pCloth[i] == 0xFF) {
            continue;
        }
        parts = m->getPartsPtr(c->pCloth[i]);
        parts->motParts.flags |= 0x06000000;
        w = &parts->Pen;
        if (c->pChild[i] == 0xFF) {
            if (c->pParent[i] == 0xFF) {
                w->Length = min_len;
                w->Offset.x = 0.0f;
                w->Offset.y = -min_len;
                w->Offset.z = 0.0f;
                w->Normal.x = 0.0f;
                w->Normal.y = -1.0f;
                w->Normal.z = 0.0f;
                PSMTXMultVec(parts->mat, &w->Offset, &w->Pos);
            } else {
                PEN_INFO* nw;
                n = m->getPartsPtr(c->pParent[i]);
                nw = &n->Pen;
                w->Length = nw->Length;
                w->Offset = nw->Offset;
                w->Normal = nw->Normal;
                w->Pos = parts->world;
                PSMTXMultVecSR(m->mat, &w->Offset, &v);
                PSVECAdd(&w->Pos, &v, &w->Pos);
            }
        } else {
            n = m->getPartsPtr(c->pChild[i]);
            w->Pos = n->world;
            w->Length = GetDistance3(&parts->world, &n->world);
            PSVECSubtract(&n->world, &parts->world, &w->Offset);
#line 119 "D:/Bio4/Prog/pendulum.cpp"
            VECNormalize(&w->Offset, &w->Normal);
        }
        w->Speed.x = 0.0f;
        w->Speed.y = 0.0f;
        w->Speed.z = 0.0f;
    }
    for (i = 0; i < c->Num; i++) {
        if (c->pCloth[i] == 0xFF) {
            continue;
        }
        parts = m->getPartsPtr(c->pCloth[i]);
        w = &parts->Pen;
        if (c->pLeft && c->pLeft[i] < 0xFF) {
            cParts* p = m->getPartsPtr(c->pLeft[i]);
            w->Length_l = GetDistance3(&w->Pos, &p->Pen.Pos) * 0.5f;
        }
        if (c->pRight && c->pRight[i] < 0xFF) {
            cParts* p = m->getPartsPtr(c->pRight[i]);
            w->Length_r = GetDistance3(&w->Pos, &p->Pen.Pos) * 0.5f;
        }
        if (c->pUpLeft && c->pUpLeft[i] < 0xFF) {
            cParts* p = m->getPartsPtr(c->pUpLeft[i]);
            w->Length_lu = GetDistance3(&w->Pos, &p->Pen.Pos) * 0.5f;
        }
        if (c->pUpRight && c->pUpRight[i] < 0xFF) {
            cParts* p = m->getPartsPtr(c->pUpRight[i]);
            w->Length_ru = GetDistance3(&w->Pos, &p->Pen.Pos) * 0.5f;
        }
    }
}

// Pin link `no` at a world position.
void PenClothFixSet(cModel* m, PenCloth* c, int no, Vec* pos)
{
    if (c->pCloth[no] != 0xFF) {
        PEN_INFO* w = &m->getPartsPtr(c->pCloth[no])->Pen;
        w->Fix_pos = *pos;
        w->Flag |= 1;
    }
}

// Unpin link `no` (Pen.Flag bit0 off) so it swings again.
void PenClothFixClear(cModel* m, PenCloth* c, int no)
{
    if (c->pCloth[no] != 0xFF) {
        PEN_INFO* w = &m->getPartsPtr(c->pCloth[no])->Pen;
        w->Flag &= ~1;
    }
}

// One simulation frame of a chain (called from the owner's move after the motion): floor from
// parts 0 unless Flag 0x100, collision volumes unless Flag bit0, random-phase wind unless 0x40,
// root speed / gravity unless 0x20; each link integrates its speed, is limited in angle and pulled
// to its rest length from its parent and side neighbours, pushed out of the volumes, then its
// matrix is rebuilt to look along the link. Pen.At_ck = a volume was touched this frame.
void PenClothMove(cModel* m, PenCloth* c)
{
    Mtx mtx;
    Vec v;
    Vec wind;
    Vec a;
    Vec b;
    Vec mpos;
    f32 floorY;
    f32 spdLen;
    f32 ang;
    f32 spdRate;   // also the angle limit of the pMax loop: one variable (10 + 10 refs) outranks d for f31
    f32 d;
    PenAtWork* at;
    cParts* parts;
    PEN_INFO* w;
    PEN_INFO* uw;
    cParts* np;   // neighbour parts (one variable for every lookup: r7 in the original)
    const u8* pp;
    u32 i;
    u32 k;
    int hit;

    if (c->Num == 0) {
        return;
    }
    floorY = -100000.0f;
    parts = m->getPartsPtr(0);
    if (!(c->Flag & 0x100)) {
        floorY = SatMgr.getFloor(&parts->world, 0, 600.0f, 100000.0f, 0) + 30.0f;
    }
    if (m->be_flag & 0x00200000) {
        PenClothReset(m, c);
    }
    at = 0;
    if (!(c->Flag & 1)) {
        cModel* am = m;
        if (c->pEm_at) {
            am = c->pEm_at;
        }
        at = penClothAtMake(am, c->pAtset, c->At_num);
    }
    ang = 1.0f;
    if (!(c->Flag & 0x40)) {
        c->WindSin += fRand0_1() * GlobalWindAdd;
        c->WindSin = LIMIT_ANGLE(c->WindSin);
        penWindScale(&wind, sinf(c->WindSin) + ang);
    }
    for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
        parts = PEN_PARTS(m, c, *pp);
        w = &parts->Pen;
        w->At_ck = 0;
    }

    // gravity, parent speed and wind
    if (!(c->Flag & 0x20)) {
        cParts* root = m->getPartsPtr(0);
        mpos = root->world;
        PSVECSubtract(&root->world, &root->world_old, &a);
        spdLen = SQRTF(a.x * a.x + a.z * a.z);
        if (spdLen > 100.0f) {
            spdLen = 100.0f;
        }
        spdLen *= 0.01f * 5.0f;
        if (m->be_flag & 0x00100000) {
            spdLen = 0.0f;
        } else {
            PSVECScale(&a, &a, c->Move_rate);
        }
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
            PSVECAdd(&w->Pos, &a, &w->Pos);
            w->Pos_old = w->Pos;
            if (c->pGravity) {
                w->Speed.y -= c->pGravity[i];
            } else {
                w->Speed.y -= c->Gravity;
            }
            PSVECAdd(&w->Pos, &w->Speed, &w->Pos);
            if ((c->Flag & 0x200) && c->pWindRate) {
                // COMPILER-DIFF: register pins + address copy (see docs/research/ "DOL pendulum closer
                // pass 2"). The original computes this call's `&v` straight into r5 and copies it to the
                // callee-saved `&v` pseudo before the call (`addi r5,r1,0x38; mr r3; mr r4; mr r28,r5`):
                // the #3 frame-address PRE family. Ours keeps a pseudo for the argument (`addi r30; mr
                // r5,r30`) and copies after the call. The three argument pins fix the argument moves; the
                // plain hard-register copy `pv = pa` survives (r5 is still live for the call, so combine
                // cannot fold it and regmove stops at the call); the codeless asm reading pv with r3/r4
                // inputs puts the copy after the last argument move in sched2, the target's issue order.
                register Vec* pa asm("r5") = &v;
                register Vec* pw asm("r3") = &w->Pos;
                register Vec* pm asm("r4") = &mpos;
                register Vec* pv asm("r28");
                pv = pa;
                asm("" : "+r"(pv) : "r"(pm), "r"(pw));
                PSVECSubtract(pw, pm, pa);
                v.y = 0.0f;
                if (v.x != 0.0f && v.z != 0.0f) {
                    PSVECNormalize(pv, pv);
                    PSVECScale(pv, pv, spdLen);
                    ang = 1.0f;
                    // COMPILER-DIFF: launder (see docs/research/ "DOL pendulum closer pass 2"). Hides ang == 1.0
                    // from cse1 so the `+ 1.0f` below stays a pool load: with the wind block's `+ 1.0f` it
                    // is then loop.c's combined (savings 2) constant movable, re-emitted in the preheader
                    // through emit_move_insn with a fresh high (`lis r9; lfs f30`), which is the original.
                    asm("" : "+f"(ang));
                    if (c->pWindSin) {
                        ang = sinf(LIMIT_ANGLE(c->WindSin + c->pWindSin[i])) + 1.0f;
                        PSVECScale(pv, pv, ang);
                    }
                    if (c->pWindRate) {
                        PSVECScale(pv, pv, ang);
                        PSVECScale(pv, pv, c->pWindRate[i]);
                    }
                    PSVECAdd(&w->Pos, pv, &w->Pos);
                }
            }
            if (DbgFlagChk(pG, DBG_WIND_ON) && !(c->Flag & 0x40) && c->pWindRate) {
                ang = 0.0f;
                if (c->pWindSin) {
                    ang = sinf(LIMIT_ANGLE(c->WindSin + c->pWindSin[i])) + 1.0f;
                    PSVECScale(&GlobalWind, &wind, ang);
                }
                if (c->pWindRate) {
                    PSVECScale(&GlobalWind, &wind, ang);
                    PSVECScale(&wind, &wind, c->pWindRate[i]);
                }
                PSVECAdd(&w->Pos, &wind, &w->Pos);
            }
            PEN_FLOOR_CK(m, c, w, i, floorY);
            PEN_FIX(w);
        }
    }

    // angle limit against the upper link
    if (!(c->Flag & 0x10) && c->pMax) {
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
            if (w->Flag & 1) {
                continue;
            }
            if (c->pParent[i] < 0xFF) {
                np = penPartsNo(m, c, c->pParent[i]);
                uw = &np->Pen;
                parts->world = uw->Pos;
                PSVECSubtract(&uw->Pos, &np->world, &a);
                if (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f) {
                    a = w->Normal;
                } else {
#line 521 "D:/Bio4/Prog/pendulum.cpp"
                    VECNormalize(&a, &a);
                }
            } else {
                PSMTXMultVecSR(parts->mat, &w->Normal, &a);
#line 526 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&a, &a);
            }
            PSVECSubtract(&w->Pos, &parts->world, &b);
            d = SQRTF(b.x * b.x + b.y * b.y + b.z * b.z);
            if (b.x == 0.0f && b.y == 0.0f && b.z == 0.0f) {
                b = w->Normal;
            } else {
#line 536 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&b, &b);
            }
            ang = acosf(PSVECDotProduct(&a, &b));
            spdRate = c->pMax[i];
            if (ang > spdRate && ang < PI - 0.01f) {
                spdRate = ang * 0.2f + spdRate * 0.8f;
                PSVECCrossProduct(&a, &b, &v);
                PSMTXRotAxisRad(mtx, &v, spdRate);
                PSMTXMultVecSR(mtx, &a, &w->Pos);
                PSVECScale(&w->Pos, &w->Pos, d);
                PSVECAdd(&w->Pos, &parts->world, &w->Pos);
            }
        }
    }

    // distance constraints
    if (!(c->Flag & 2)) {
        for (k = 0; k < c->Bundle_num; k++) {
            for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
                parts = PEN_PARTS(m, c, *pp);
                w = &parts->Pen;
                if (!(c->Flag & 4)) {
#define PEN_SIDE_CK(tbl, dist)                                                            \
                    if ((tbl) && (tbl)[i] < 0xFF) {                                       \
                        np = penPartsNo(m, c, (tbl)[i]);                                  \
                        uw = &np->Pen;               \
                        PSVECSubtract(&uw->Pos, &w->Pos, &v);                             \
                        d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);                     \
                        PSVECScale(&v, &v, (dist) / d - 0.5f);                            \
                        PSVECSubtract(&w->Pos, &v, &w->Pos);                              \
                        PSVECAdd(&uw->Pos, &v, &uw->Pos);                                 \
                    }
                    PEN_SIDE_CK(c->pLeft, w->Length_l);
                    PEN_SIDE_CK(c->pRight, w->Length_r);
                    PEN_SIDE_CK(c->pUpLeft, w->Length_lu);
                    PEN_SIDE_CK(c->pUpRight, w->Length_ru);
#undef PEN_SIDE_CK
                }
                if (c->pParent[i] < 0xFF) {
                    np = penPartsNo(m, c, c->pParent[i]);
                    uw = &np->Pen;
                    PSVECSubtract(&uw->Pos, &w->Pos, &v);
                    d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);
                    PSVECScale(&v, &v, 0.5f / d * (w->Length - d));
                    PSVECAdd(&uw->Pos, &v, &uw->Pos);
                    PSVECSubtract(&w->Pos, &v, &w->Pos);
                    hit = PEN_AT_CK(c, &w->Pos, &uw->Pos, at);
                    if (hit) {
                        w->At_ck |= 1;
                    }
                    PEN_FLOOR_CK(m, c, w, i, floorY);
                    PEN_FIX(w);
                    PEN_FIX(uw);
                } else {
                    PSVECSubtract(&parts->world, &w->Pos, &v);
                    d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);
                    PSVECScale(&v, &v, 0.5f / d * (w->Length - d));
                    PSVECSubtract(&w->Pos, &v, &w->Pos);
                    hit = PEN_AT_CK(c, &w->Pos, &parts->world, at);
                    if (hit) {
                        w->At_ck |= 1;
                    }
                    PEN_FLOOR_CK(m, c, w, i, floorY);
                    PEN_FIX(w);
                }
            }
        }
    }

    // matrices
    for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
        parts = PEN_PARTS(m, c, *pp);
        w = &parts->Pen;
        uw = 0;
        if (c->pParent[i] < 0xFF) {
            np = penPartsNo(m, c, c->pParent[i]);
            uw = &np->Pen;
            parts->world = uw->Pos;
        }
        if (!(c->Flag & 8)) {
            PSVECSubtract(&w->Pos, &parts->world, &v);
            if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
                v = w->Normal;
            } else {
#line 810 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&v, &v);
            }
            PSVECScale(&v, &v, w->Length);
            PSVECAdd(&parts->world, &v, &w->Pos);
        }
        PEN_AT_CK_DIAMOND(c, w, uw, parts, at, hit);
        if (hit) {
            w->At_ck |= 1;
        }
        PEN_FLOOR_CK2(c, w, uw, floorY);
        PEN_FIX(w);
        if (c->pRate) {
            spdRate = c->pRate[i];
        } else {
            spdRate = c->Rate;
        }
        if (w->At_ck & 1) {
            spdRate *= 0.3f;
        }
        PSVECSubtract(&w->Pos, &w->Pos_old, &w->Speed);
        PSVECScale(&w->Speed, &w->Speed, spdRate);
        PSMTXMultVecSR(parts->mat, &w->Normal, &a);
        PSVECSubtract(&w->Pos, &parts->world, &b);
        if (b.x == 0.0f && b.y == 0.0f && b.z == 0.0f) {
            b = w->Normal;
        } else {
#line 878 "D:/Bio4/Prog/pendulum.cpp"
            VECNormalize(&b, &b);
        }
        ang = acosf(PSVECDotProduct(&a, &b));
        if (ang > 0.01f && ang < PI - 0.01f) {
            PSVECCrossProduct(&a, &b, &a);
            PSMTXRotAxisRad(mtx, &a, ang);
            PSMTXConcat(mtx, parts->mat, parts->mat);
        }
        TransMatrix(parts->mat, &parts->world);
        if (pG->debug_mode == 8) {
            Draw_line3d(&parts->world, &w->Pos, 0xFFFFFFFF, 0);
            Draw_sphere(&w->Pos, 3.0f, 0xFFFF0000, 1, 1);
        }
    }

    if (pG->debug_mode == 8 && !(c->Flag & 4)) {
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
#define PEN_SIDE_DRAW(tbl)                                                                \
            if ((tbl) && (tbl)[i] < 0xFF) {                                               \
                np = penPartsNo(m, c, (tbl)[i]);                                  \
                        uw = &np->Pen;                       \
                Draw_line3d(&w->Pos, &uw->Pos, 0xFF808080, 0);                            \
            }
            PEN_SIDE_DRAW(c->pLeft);
            PEN_SIDE_DRAW(c->pRight);
            PEN_SIDE_DRAW(c->pUpLeft);
            PEN_SIDE_DRAW(c->pUpRight);
#undef PEN_SIDE_DRAW
        }
    }
}

// Variant with the stiffness ang (x4C) in the constraints and no motion wind.
void PenClothMove2(cModel* m, PenCloth* c)
{
    Mtx mtx;
    Vec v;
    Vec wind;
    Vec a;
    Vec b;
    f32 floorY;
    f32 ang;
    f32 spdRate;   // also the angle limit of the pMax loop: one variable (10 + 10 refs) outranks d for f31
    f32 d;
    PenAtWork* at;
    cParts* parts;
    PEN_INFO* w;
    PEN_INFO* uw;
    cParts* np;   // neighbour parts (one variable for every lookup: r7 in the original)
    register const u8* pp asm("r21");   // COMPILER-DIFF: #17 (register pin): pp above i in global-alloc
    u32 i;
    u32 k;
    int hit;

    if (c->Num == 0) {
        return;
    }
    floorY = -100000.0f;
    parts = m->getPartsPtr(0);
    if (!(c->Flag & 0x100)) {
        floorY = SatMgr.getFloor(&parts->world, 0, 600.0f, 100000.0f, 0) + 30.0f;
    }
    if (m->be_flag & 0x00200000) {
        PenClothReset(m, c);
    }
    at = 0;
    if (!(c->Flag & 1)) {
        cModel* am = m;
        if (c->pEm_at) {
            am = c->pEm_at;
        }
        at = penClothAtMake(am, c->pAtset, c->At_num);
    }
    ang = 1.0f;
    if (!(c->Flag & 0x40)) {
        c->WindSin += fRand0_1() * GlobalWindAdd;
        c->WindSin = LIMIT_ANGLE(c->WindSin);
        penWindScale(&wind, sinf(c->WindSin) + ang);
    }
    for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
        parts = PEN_PARTS(m, c, *pp);
        w = &parts->Pen;
        w->At_ck = 0;
    }

    if (!(c->Flag & 0x20)) {
        if (m->be_flag & 0x00100000) {
            cParts* root = m->getPartsPtr(0);
            PSVECSubtract(&root->world, &root->world_old, &a);
        } else {
            cParts* root = m->getPartsPtr(0);
            PSVECSubtract(&root->world, &root->world_old, &a);
            PSVECScale(&a, &a, c->Move_rate);
        }
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
            PSVECAdd(&w->Pos, &a, &w->Pos);
            w->Pos_old = w->Pos;
            if (c->pGravity) {
                w->Speed.y -= c->pGravity[i];
            } else {
                w->Speed.y -= c->Gravity;
            }
            PSVECAdd(&w->Pos, &w->Speed, &w->Pos);
            if (DbgFlagChk(pG, DBG_WIND_ON) && !(c->Flag & 0x40) && c->pWindRate) {
                ang = 0.0f;
                if (c->pWindSin) {
                    ang = sinf(LIMIT_ANGLE(c->WindSin + c->pWindSin[i])) + 1.0f;
                    PSVECScale(&GlobalWind, &wind, ang);
                }
                if (c->pWindRate) {
                    PSVECScale(&GlobalWind, &wind, ang);
                    PSVECScale(&wind, &wind, c->pWindRate[i]);
                }
                PSVECAdd(&w->Pos, &wind, &w->Pos);
            }
            PEN_FLOOR_CK(m, c, w, i, floorY);
            PEN_FIX(w);
        }
    }

    if (!(c->Flag & 0x10) && c->pMax) {
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
            if (w->Flag & 1) {
                continue;
            }
            if (c->pParent[i] < 0xFF) {
                np = penPartsNo(m, c, c->pParent[i]);
                uw = &np->Pen;
                parts->world = uw->Pos;
                PSVECSubtract(&uw->Pos, &np->world, &a);
                if (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f) {
                    a = w->Normal;
                } else {
#line 1209 "D:/Bio4/Prog/pendulum.cpp"
                    VECNormalize(&a, &a);
                }
            } else {
                PSMTXMultVecSR(parts->mat, &w->Normal, &a);
#line 1214 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&a, &a);
            }
            PSVECSubtract(&w->Pos, &parts->world, &b);
            d = SQRTF(b.x * b.x + b.y * b.y + b.z * b.z);
            if (b.x == 0.0f && b.y == 0.0f && b.z == 0.0f) {
                b = w->Normal;
            } else {
#line 1224 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&b, &b);
            }
            ang = acosf(PSVECDotProduct(&a, &b));
            spdRate = c->pMax[i];
            if (ang > spdRate && ang < PI - 0.01f) {
                spdRate = ang * 0.2f + spdRate * 0.8f;
                PSVECCrossProduct(&a, &b, &v);
                PSMTXRotAxisRad(mtx, &v, spdRate);
                PSMTXMultVecSR(mtx, &a, &w->Pos);
                PSVECScale(&w->Pos, &w->Pos, d);
                PSVECAdd(&w->Pos, &parts->world, &w->Pos);
            }
        }
    }

    if (!(c->Flag & 2)) {
        for (k = 0; k < c->Bundle_num; k++) {
            for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
                parts = PEN_PARTS(m, c, *pp);
                w = &parts->Pen;
                if (!(c->Flag & 4)) {
#define PEN_SIDE_CK(tbl, dist)                                                            \
                    if ((tbl) && (tbl)[i] < 0xFF) {                                       \
                        np = penPartsNo(m, c, (tbl)[i]);                                  \
                        uw = &np->Pen;               \
                        PSVECSubtract(&uw->Pos, &w->Pos, &v);                             \
                        d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);                     \
                        PSVECScale(&v, &v, (d * 0.5f - (dist)) * c->Stretchy / d);             \
                        PSVECAdd(&w->Pos, &v, &w->Pos);                                   \
                        PSVECSubtract(&uw->Pos, &v, &uw->Pos);                            \
                    }
                    PEN_SIDE_CK(c->pLeft, w->Length_l);
                    PEN_SIDE_CK(c->pRight, w->Length_r);
                    PEN_SIDE_CK(c->pUpLeft, w->Length_lu);
                    PEN_SIDE_CK(c->pUpRight, w->Length_ru);
#undef PEN_SIDE_CK
                }
                if (c->pParent[i] < 0xFF) {
                    np = penPartsNo(m, c, c->pParent[i]);
                    uw = &np->Pen;
                    PSVECSubtract(&uw->Pos, &w->Pos, &v);
                    d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);
                    PSVECScale(&v, &v, (d - w->Length) * 0.5f * c->Stretchy / d);
                    PSVECAdd(&w->Pos, &v, &w->Pos);
                    PSVECSubtract(&uw->Pos, &v, &uw->Pos);
                    hit = PEN_AT_CK(c, &w->Pos, &uw->Pos, at);
                    if (hit) {
                        w->At_ck |= 1;
                    }
                    PEN_FLOOR_CK(m, c, w, i, floorY);
                    PEN_FIX(w);
                    PEN_FIX(uw);
                } else {
                    PSVECSubtract(&parts->world, &w->Pos, &v);
                    d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);
                    PSVECScale(&v, &v, (d - w->Length) * 0.5f * c->Stretchy / d);
                    PSVECAdd(&w->Pos, &v, &w->Pos);
                    hit = PEN_AT_CK(c, &w->Pos, &parts->world, at);
                    if (hit) {
                        w->At_ck |= 1;
                    }
                    PEN_FLOOR_CK(m, c, w, i, floorY);
                    PEN_FIX(w);
                }
            }
        }
    }

    for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
        parts = PEN_PARTS(m, c, *pp);
        w = &parts->Pen;
        uw = 0;
        if (c->pParent[i] < 0xFF) {
            np = penPartsNo(m, c, c->pParent[i]);
            uw = &np->Pen;
            parts->world = uw->Pos;
        }
        if (!(c->Flag & 8)) {
            PSVECSubtract(&w->Pos, &parts->world, &v);
            if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
                v = w->Normal;
            } else {
#line 1514 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&v, &v);
            }
            PSVECScale(&v, &v, w->Length);
            PSVECAdd(&parts->world, &v, &w->Pos);
        }
        PEN_AT_CK_DIAMOND(c, w, uw, parts, at, hit);
        if (hit) {
            w->At_ck |= 1;
        }
        PEN_FLOOR_CK2(c, w, uw, floorY);
        PEN_FIX(w);
        if (c->pRate) {
            spdRate = c->pRate[i];
        } else {
            spdRate = c->Rate;
        }
        if (w->At_ck & 1) {
            spdRate *= 0.3f;
        }
        PSVECSubtract(&w->Pos, &w->Pos_old, &w->Speed);
        PSVECScale(&w->Speed, &w->Speed, spdRate);
        PSMTXMultVecSR(parts->mat, &w->Normal, &a);
        PSVECSubtract(&w->Pos, &parts->world, &b);
        if (b.x == 0.0f && b.y == 0.0f && b.z == 0.0f) {
            b = w->Normal;
        } else {
#line 1582 "D:/Bio4/Prog/pendulum.cpp"
            VECNormalize(&b, &b);
        }
        ang = acosf(PSVECDotProduct(&a, &b));
        if (ang > 0.01f && ang < PI - 0.01f) {
            PSVECCrossProduct(&a, &b, &a);
            PSMTXRotAxisRad(mtx, &a, ang);
            PSMTXConcat(mtx, parts->mat, parts->mat);
        }
        TransMatrix(parts->mat, &parts->world);
        if (pG->debug_mode == 8) {
            Draw_line3d(&parts->world, &w->Pos, 0xFFFFFFFF, 0);
            Draw_sphere(&w->Pos, 10.0f, 0xFFFF0000, 1, 1);
        }
    }

    if (pG->debug_mode == 8 && !(c->Flag & 4)) {
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            PEN_INFO* nw;
            u32 no;
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
            if (c->pLeft && c->pLeft[i] < 0xFF) {
                np = penPartsNo(m, c, c->pLeft[i]);
                nw = &np->Pen;
                Draw_line3d(&w->Pos, &nw->Pos, 0xFF808080, 0);
            }
            if (c->pRight && c->pRight[i] < 0xFF) {
                np = penPartsNo(m, c, c->pRight[i]);
                nw = &np->Pen;
                Draw_line3d(&w->Pos, &nw->Pos, 0xFF808080, 0);
            }
            if (c->pUpLeft) {
                no = 0xFF;
                if (c) {
                    no = c->pUpLeft[i];
                }
                if (no < 0xFF) {
                    np = penPartsNo(m, c, no);
                    nw = &np->Pen;
                    Draw_line3d(&w->Pos, &nw->Pos, 0xFF808080, 0);
                }
            }
            if (c->pUpRight) {
                no = 0xFF;
                if (c) {
                    no = c->pUpRight[i];
                }
                if (no < 0xFF) {
                    np = penPartsNo(m, c, no);
                    nw = &np->Pen;
                    Draw_line3d(&w->Pos, &nw->Pos, 0xFF808080, 0);
                }
            }
        }
    }
}

// Variant with the parallel collision check on every constraint and the side neighbours kept
// above the floor too.
void PenClothMove3(cModel* m, PenCloth* c)
{
    Mtx mtx;
    Vec v;
    Vec wind;
    Vec a;
    Vec b;
    f32 floorY;
    f32 ang;
    f32 spdRate;   // also the angle limit of the pMax loop: one variable (10 + 10 refs) outranks d for f31
    f32 d;
    PenAtWork* at;
    cParts* parts;
    PEN_INFO* w;
    PEN_INFO* uw;
    cParts* np;   // neighbour parts (one variable for every lookup: r7 in the original)
    const u8* pp;
    u32 i;
    u32 k;
    int hit;

    if (c->Num == 0) {
        return;
    }
    floorY = -100000.0f;
    parts = m->getPartsPtr(0);
    if (!(c->Flag & 0x100)) {
        floorY = SatMgr.getFloor(&parts->world, 0, 600.0f, 100000.0f, 0) + 30.0f;
    }
    if (m->be_flag & 0x00200000) {
        PenClothReset(m, c);
    }
    at = 0;
    if (!(c->Flag & 1)) {
        cModel* am = m;
        if (c->pEm_at) {
            am = c->pEm_at;
        }
        at = penClothAtMake(am, c->pAtset, c->At_num);
    }
    ang = 1.0f;
    if (!(c->Flag & 0x40)) {
        c->WindSin += fRand0_1() * GlobalWindAdd;
        c->WindSin = LIMIT_ANGLE(c->WindSin);
        penWindScale(&wind, sinf(c->WindSin) + ang);
    }
    for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
        parts = PEN_PARTS(m, c, *pp);
        w = &parts->Pen;
        w->At_ck = 0;
    }

    if (!(c->Flag & 0x20)) {
        if (m->be_flag & 0x00100000) {
            cParts* root = m->getPartsPtr(0);
            PSVECSubtract(&root->world, &root->world_old, &a);
        } else {
            cParts* root = m->getPartsPtr(0);
            PSVECSubtract(&root->world, &root->world_old, &a);
            PSVECScale(&a, &a, c->Move_rate);
        }
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
            PSVECAdd(&w->Pos, &a, &w->Pos);
            w->Pos_old = w->Pos;
            if (c->pGravity) {
                w->Speed.y -= c->pGravity[i];
            } else {
                w->Speed.y -= c->Gravity;
            }
            PSVECAdd(&w->Pos, &w->Speed, &w->Pos);
            if (DbgFlagChk(pG, DBG_WIND_ON) && !(c->Flag & 0x40) && c->pWindRate) {
                ang = 0.0f;
                if (c->pWindSin) {
                    ang = sinf(LIMIT_ANGLE(c->WindSin + c->pWindSin[i])) + 1.0f;
                    PSVECScale(&GlobalWind, &wind, ang);
                }
                if (c->pWindRate) {
                    PSVECScale(&GlobalWind, &wind, ang);
                    PSVECScale(&wind, &wind, c->pWindRate[i]);
                }
                PSVECAdd(&w->Pos, &wind, &w->Pos);
            }
            PEN_FLOOR_CK(m, c, w, i, floorY);
            PEN_FIX(w);
        }
    }

    if (!(c->Flag & 0x10) && c->pMax) {
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
            if (w->Flag & 1) {
                continue;
            }
            if (c->pParent[i] < 0xFF) {
                np = penPartsNo(m, c, c->pParent[i]);
                uw = &np->Pen;
                parts->world = uw->Pos;
                PSVECSubtract(&uw->Pos, &np->world, &a);
                if (a.x == 0.0f && a.y == 0.0f && a.z == 0.0f) {
                    a = w->Normal;
                } else {
#line 1882 "D:/Bio4/Prog/pendulum.cpp"
                    VECNormalize(&a, &a);
                }
            } else {
                PSMTXMultVecSR(parts->mat, &w->Normal, &a);
#line 1887 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&a, &a);
            }
            PSVECSubtract(&w->Pos, &parts->world, &b);
            d = SQRTF(b.x * b.x + b.y * b.y + b.z * b.z);
            if (b.x == 0.0f && b.y == 0.0f && b.z == 0.0f) {
                b = w->Normal;
            } else {
#line 1897 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&b, &b);
            }
            ang = acosf(PSVECDotProduct(&a, &b));
            spdRate = c->pMax[i];
            if (ang > spdRate && ang < PI - 0.01f) {
                spdRate = ang * 0.2f + spdRate * 0.8f;
                PSVECCrossProduct(&a, &b, &v);
                PSMTXRotAxisRad(mtx, &v, spdRate);
                PSMTXMultVecSR(mtx, &a, &w->Pos);
                PSVECScale(&w->Pos, &w->Pos, d);
                PSVECAdd(&w->Pos, &parts->world, &w->Pos);
            }
        }
    }

    if (!(c->Flag & 2)) {
        for (k = 0; k < c->Bundle_num; k++) {
            for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
                parts = PEN_PARTS(m, c, *pp);
                w = &parts->Pen;
                if (!(c->Flag & 4)) {
                    if (c->pUpLeft && c->pUpLeft[i] < 0xFF) {
                np = penPartsNo(m, c, c->pUpLeft[i]);
                uw = &np->Pen;
                        penClothAtCkParallel(&w->Pos, &uw->Pos, at);
                        PEN_FLOOR_CK3(c, w, uw, floorY);
                        PEN_FIX(w);
                        PEN_FIX(uw);
                    }
                    if (c->pUpRight && c->pUpRight[i] < 0xFF) {
                np = penPartsNo(m, c, c->pUpRight[i]);
                uw = &np->Pen;
                        penClothAtCkParallel(&w->Pos, &uw->Pos, at);
                        PEN_FLOOR_CK3(c, w, uw, floorY);
                        PEN_FIX(w);
                        PEN_FIX(uw);
                    }
#define PEN_SIDE_CK(tbl, dist)                                                            \
                    if ((tbl) && (tbl)[i] < 0xFF) {                                       \
                        np = penPartsNo(m, c, (tbl)[i]);                                  \
                        uw = &np->Pen;               \
                        penClothAtCkParallel(&w->Pos, &uw->Pos, at);                      \
                        PSVECSubtract(&uw->Pos, &w->Pos, &v);                             \
                        d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);                     \
                        PSVECScale(&v, &v, (d * 0.5f - (dist)) * c->Stretchy / d);             \
                        PSVECAdd(&w->Pos, &v, &w->Pos);                                   \
                        PSVECSubtract(&uw->Pos, &v, &uw->Pos);                            \
                        PEN_FLOOR_CK3(c, w, uw, floorY);                                  \
                        PEN_FIX(w);                                                       \
                        PEN_FIX(uw);                                                      \
                    }
                    PEN_SIDE_CK(c->pLeft, w->Length_l);
                    PEN_SIDE_CK(c->pRight, w->Length_r);
#undef PEN_SIDE_CK
                }
                if (c->pParent[i] < 0xFF) {
                    np = penPartsNo(m, c, c->pParent[i]);
                    uw = &np->Pen;
                    penClothAtCkParallel(&w->Pos, &uw->Pos, at);
                    PSVECSubtract(&uw->Pos, &w->Pos, &v);
                    d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);
                    PSVECScale(&v, &v, (d - w->Length) * 0.5f * c->Stretchy / d);
                    PSVECAdd(&w->Pos, &v, &w->Pos);
                    PSVECSubtract(&uw->Pos, &v, &uw->Pos);
                    PEN_FLOOR_CK3(c, w, uw, floorY);
                    PEN_FIX(w);
                    PEN_FIX(uw);
                } else {
                    hit = PEN_AT_CK(c, &w->Pos, &parts->world, at);
                    if (hit) {
                        w->At_ck |= 1;
                    }
                    PSVECSubtract(&parts->world, &w->Pos, &v);
                    d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);
                    PSVECScale(&v, &v, (d - w->Length) * 0.5f * c->Stretchy / d);
                    PSVECAdd(&w->Pos, &v, &w->Pos);
                    if (!(c->Flag & 0x100)) {
                        if (w->Pos.y < floorY) {
                            w->Pos.y = floorY;
                        }
                    }
                    PEN_FIX(w);
                }
            }
        }
    }

    for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
        parts = PEN_PARTS(m, c, *pp);
        w = &parts->Pen;
        uw = 0;
        if (c->pParent[i] < 0xFF) {
            np = penPartsNo(m, c, c->pParent[i]);
            uw = &np->Pen;
            parts->world = uw->Pos;
        }
        if (!(c->Flag & 8)) {
            PSVECSubtract(&w->Pos, &parts->world, &v);
            if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
                v = w->Normal;
            } else {
#line 2255 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&v, &v);
            }
            PSVECScale(&v, &v, w->Length);
            PSVECAdd(&parts->world, &v, &w->Pos);
        }
        PEN_AT_CK_DIAMOND(c, w, uw, parts, at, hit);
        if (hit) {
            w->At_ck |= 1;
        }
        PEN_FLOOR_CK2(c, w, uw, floorY);
        PEN_FIX(w);
        if (c->pRate) {
            spdRate = c->pRate[i];
        } else {
            spdRate = c->Rate;
        }
        if (w->At_ck & 1) {
            spdRate *= 0.3f;
        }
        PSVECSubtract(&w->Pos, &w->Pos_old, &w->Speed);
        PSVECScale(&w->Speed, &w->Speed, spdRate);
        PSMTXMultVecSR(parts->mat, &w->Normal, &a);
        PSVECSubtract(&w->Pos, &parts->world, &b);
        if (b.x == 0.0f && b.y == 0.0f && b.z == 0.0f) {
            b = w->Normal;
        } else {
#line 2323 "D:/Bio4/Prog/pendulum.cpp"
            VECNormalize(&b, &b);
        }
        ang = acosf(PSVECDotProduct(&a, &b));
        if (ang > 0.01f && ang < PI - 0.01f) {
            PSVECCrossProduct(&a, &b, &a);
            PSMTXRotAxisRad(mtx, &a, ang);
            PSMTXConcat(mtx, parts->mat, parts->mat);
        }
        TransMatrix(parts->mat, &parts->world);
        if (pG->debug_mode == 8) {
            Draw_line3d(&parts->world, &w->Pos, 0xFFFFFFFF, 0);
            Draw_sphere(&w->Pos, 3.0f, 0xFFFF0000, 1, 1);
        }
    }

    if (pG->debug_mode == 8 && !(c->Flag & 4)) {
        for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
            parts = PEN_PARTS(m, c, *pp);
            w = &parts->Pen;
            if (c->pLeft && c->pLeft[i] < 0xFF) {
                np = penPartsNo(m, c, c->pLeft[i]);
                uw = &np->Pen;
                Draw_line3d(&w->Pos, &uw->Pos, 0xFFFF8080, 0);
            }
            if (c->pRight && c->pRight[i] < 0xFF) {
                np = penPartsNo(m, c, c->pRight[i]);
                uw = &np->Pen;
                Draw_line3d(&w->Pos, &uw->Pos, 0xFFFF8080, 0);
            }
            if (c->pUpLeft && c->pUpLeft[i] < 0xFF) {
                np = penPartsNo(m, c, c->pUpLeft[i]);
                uw = &np->Pen;
                Draw_line3d(&w->Pos, &uw->Pos, 0xFF8080FF, 0);
            }
            if (c->pUpRight && c->pUpRight[i] < 0xFF) {
                np = penPartsNo(m, c, c->pUpRight[i]);
                uw = &np->Pen;
                Draw_line3d(&w->Pos, &uw->Pos, 0xFF8080FF, 0);
            }
        }
    }
}

// Dead-stripped from the DOL: only its constant pool survives (0.0, PI - 0.01, 0.2, 0.8, 0.01,
// 0.3, 3.0 between PenClothMove3's and penClothAtMake's).
static void penClothLinkMove(cModel* parts, PEN_INFO* w, Vec* a, Vec* b, f32 max, Mtx mtx, f32 rate)
{
    f32 ang;

    if (b->x == 0.0f && b->y == 0.0f && b->z == 0.0f) {
        *b = w->Normal;
    }
    ang = acosf(PSVECDotProduct(a, b));
    if (ang > max && ang < PI - 0.01f) {
        PSMTXRotAxisRad(mtx, a, ang * 0.2f + max * 0.8f);
    }
    if (ang > 0.01f) {
        rate *= 0.3f;
    }
    PSVECScale(&w->Speed, &w->Speed, rate);
    Draw_sphere(&w->Pos, 3.0f, 0xFFFF0000, 1, 1);
}

// World position of a point given in a parts' space. Inline: the addresses of the frame locals
// passed through it are set straight into the argument registers (no PRE copies). The do-while
// (a macro body in the original) puts a loop note before the call's argument sets: the first of
// them is a scheduling barrier, so the following call no longer anti-depends on the previous
// call's `addi r5, r1, 8` through r1 (ours ranked it first by dependant count; the target issues
// r4, r3, r5) and the &v0/&up/&ax pseudos get the target's callee-saved order.
static inline void penPartsWorldPos(cParts* p, const Vec* ofs, Vec* out)
{
    do {
        PSMTXMultVec(p->mat, ofs, out);
    } while (0);
}

// Build the world space collision volumes of the frame into the locked cache work.
PenAtWork* penClothAtMake(cModel* m, CLOTH_AT_SET* at, int n)
{
    PenAtWork* wk;
    Vec v0;
    Vec v1;
    Vec c;
    Vec ax;
    Vec d;
    Vec up;
    Vec zero;
    PenAt* a;
    u32 i;

    if (at == 0 || n == 0 || m == 0) {
        return 0;
    }
    wk = (PenAtWork*) 0xE0000000;
    a = wk->at;
    wk->num = n;
    wk->pAt = a;
    for (i = 0; i < n; i++, at++, a++) {
        u8 no1 = at->P2;
        f32 rate = at->Weight;
        cParts* p0 = m->getPartsPtr(at->P1);
        cParts* p1 = m->getPartsPtr(no1);

        PSMTXMultVec(p0->mat, &at->Ofs1, &v0);
        penPartsWorldPos(p1, &at->Ofs2, &v1);
        // &v1 inside the inline is a fresh `addi r5, r1, 0x18` (hard-reg arg set, never PRE'd); the case
        // bodies share one pseudo that loop.c hoists (`addi r26, r1, 0x18`). See docs/matching.md "FadeSet colour pair".
        Vec* pv1 = &v1;
        switch (at->Type) {
        case 0:
        default:
            a->type = 0;
            PosToPos(pv1, &v0, &c, rate);
            a->p0 = c;
            a->r = at->R;
            // struct view of pG: the fixed-scalar load would otherwise be hoisted between the
            // copy's word stores (and the copy issued 4, 0, 8 through the extra r9 anti-dependence).
            if (DbgFlagChk(pG, DBG_CLOTH_AT_DISP)) {
                Draw_sphere(&c, at->R, 0x80808080, 1, 1);
            }
            break;
        case 1:
            a->type = 1;
            a->r = at->R;
            a->p0 = v0;
            a->p1 = *pv1;
            a->len = GetDistance3(&v0, pv1);
            PSVECSubtract(pv1, &v0, &d);
            ax.x = fabsf(d.x);
            ax.z = fabsf(d.z);
            if (ax.x < ax.z) {
                up.x = 1.0f;
                up.y = 0.0f;
                up.z = 0.0f;
            } else {
                up.x = 0.0f;
                up.y = 0.0f;
                up.z = 1.0f;
            }
            PSVECCrossProduct(&d, &up, &ax);
            PSVECCrossProduct(&ax, &d, &up);
#line 2793 "D:/Bio4/Prog/pendulum.cpp"
            VECNormalize(&ax, &ax);
            VECNormalize(&d, &d);
            VECNormalize(&up, &up);
            a->mat[0][0] = ax.x;
            a->mat[1][0] = ax.y;
            a->mat[2][0] = ax.z;
            a->mat[0][1] = d.x;
            a->mat[1][1] = d.y;
            a->mat[2][1] = d.z;
            a->mat[0][2] = up.x;
            a->mat[1][2] = up.y;
            a->mat[2][2] = up.z;
            TransMatrix(a->mat, &v0);
            PSMTXInverse(a->mat, a->inv);
            if (DbgFlagChk(pG, DBG_CLOTH_AT_DISP)) {
                zero.x = 0.0f;
                zero.y = 0.0f;
                zero.z = 0.0f;
                Draw_cylinderMtx(a->mat, &zero, a->r, a->len, 0x80808080);
            }
            break;
        }
    }
    return wk;
}

// Push the link end `pos` (hanging from `up`) out of the spheres; 1 when it hit one.
int penClothAtCk(Vec* pos, Vec* up, PenAtWork* wk)
{
    Mtx mtx;
    Vec v;
    Vec v2;
    Vec ax;
    f32 len;
    f32 rr;
    f32 dd;
    f32 d;
    f32 ang;
    int hit;
    u32 i;
    PenAt* a;

    if (wk == 0) {
        return 0;
    }
    len = GetDistance3(pos, up);
    hit = 0;
    a = wk->pAt;
    for (i = 0; i < wk->num; i++, a++) {
        PSVECSubtract(pos, &a->p0, &v);
        dd = v.x * v.x + v.y * v.y + v.z * v.z;
        rr = a->r * a->r;
        if (dd < rr) {
            PSVECSubtract(&a->p0, up, &v);
            d = SQRTF(v.x * v.x + v.y * v.y + v.z * v.z);
            ang = acosf((rr - len * len - d * d) / (len * -2.0f * d));
            if (ang > 0.01f && ang < PI - 0.01f) {
                PSVECSubtract(pos, up, &v2);
                PSVECCrossProduct(&v, &v2, &ax);
                PSMTXRotAxisRad(mtx, &ax, ang);
#line 2893 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&v, &v);
                PSVECScale(&v, &v, len);
                PSMTXMultVec(mtx, &v, &v);
                PSVECAdd(up, &v, pos);
            } else {
#line 2899 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&v, &v);
                PSVECScale(&v, &v, a->r + 1.0f);
                PSVECAdd(&a->p0, &v, pos);
            }
            hit = 1;
        }
    }
    return hit;
}

// Keep the link `up`-`pos` outside the spheres: a link crossing a sphere is rotated around `up`
// until it touches the surface; 1 when a sphere was hit.
int penClothAtCkBorder(Vec* pos, Vec* up, PenAtWork* wk)
{
    Mtx mtx;
    Vec d;
    Vec pu;
    Vec w;
    Vec ax;
    Vec tmp;
    f32 lenSq;
    f32 invLenSq;
    f32 len;
    f32 rad;
    f32 dd;
    f32 ang;
    f32 t;
    f32 wSq;
    f32 puSq;
    f32 rr;
    int n;
    int ret;
    PenAt* a;

    if (wk == 0) {
        return 0;
    }
    PSVECSubtract(pos, up, &d);
    lenSq = d.x * d.x + d.y * d.y + d.z * d.z;
    if (lenSq == 0.0f) {
        return 0;
    }
    invLenSq = 1.0f / lenSq;
    len = SQRTF(lenSq);
    ret = 0;
    n = wk->num;
    a = wk->pAt - 1;
    while (n--) {
        a++;
        if (a->type != 0) {
            continue;
        }
        w.x = pos->x - a->p0.x;
        w.y = pos->y - a->p0.y;
        w.z = pos->z - a->p0.z;
        d.x = pos->x - up->x;
        d.y = pos->y - up->y;
        d.z = pos->z - up->z;
        wSq = w.x * w.x + w.y * w.y + w.z * w.z;
        pu.x = a->p0.x - up->x;
        pu.y = a->p0.y - up->y;
        pu.z = a->p0.z - up->z;
        if (pu.x == 0.0f && pu.y == 0.0f && pu.z == 0.0f) {
            pu.y = 1.0f;
        }
        puSq = pu.x * pu.x + pu.y * pu.y + pu.z * pu.z;
        rr = a->r * a->r;
        if (puSq < rr) {
            continue;
        }
        if (wSq > rr) {
            if (puSq > (len + a->r) * (len + a->r)) {
                continue;
            }
            w.x = a->p0.x - up->x;
            w.y = a->p0.y - up->y;
            w.z = a->p0.z - up->z;
            t = w.x * d.x + w.y * d.y + w.z * d.z;
            t *= invLenSq;
            if (t < 0.0f) {
                continue;
            }
            if (t > 1.0f) {
                continue;
            }
            w.x = d.x * t + up->x - a->p0.x;
            w.y = d.y * t + up->y - a->p0.y;
            w.z = d.z * t + up->z - a->p0.z;
            if (w.x * w.x + w.y * w.y + w.z * w.z > a->r * a->r) {
                continue;
            }
        }
        if (puSq - a->r * a->r > lenSq) {
            w.x = a->p0.x - up->x;
            w.y = a->p0.y - up->y;
            w.z = a->p0.z - up->z;
            rad = a->r + 0.0001f;
            dd = SQRTF(w.x * w.x + w.y * w.y + w.z * w.z);
            ang = acosf((rad * rad - len * len - dd * dd) / (len * -2.0f * dd));
            if (ang > 0.01f && ang < PI - 0.01f) {
                tmp.x = pos->x - up->x;
                tmp.y = pos->y - up->y;
                tmp.z = pos->z - up->z;
                PSVECCrossProduct(&w, &tmp, &ax);
                PSMTXRotAxisRad(mtx, &ax, ang);
#line 3096 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&w, &w);
                w.x *= len;
                w.y *= len;
                w.z *= len;
                PSMTXMultVec(mtx, &w, &w);
                pos->x = up->x + w.x;
                pos->y = up->y + w.y;
                pos->z = up->z + w.z;
            } else {
#line 3108 "D:/Bio4/Prog/pendulum.cpp"
                VECNormalize(&w, &w);
                w.x *= rad;
                w.y *= rad;
                w.z *= rad;
                pos->x = a->p0.x + w.x;
                pos->y = a->p0.y + w.y;
                pos->z = a->p0.z + w.z;
            }
        } else {
            dd = SQRTF(puSq);
            ang = asinf(a->r / dd) + 0.0001f;
            PSVECCrossProduct(&pu, &d, &ax);
            PSMTXRotAxisRad(mtx, &ax, ang);
#line 3133 "D:/Bio4/Prog/pendulum.cpp"
            VECNormalize(&pu, &w);
            w.x *= len;
            w.y *= len;
            w.z *= len;
            PSMTXMultVecSR(mtx, &w, &w);
            tmp.x = up->x + w.x;
            tmp.y = up->y + w.y;
            tmp.z = up->z + w.z;
            *pos = tmp;
        }
        ret = 1;
    }
    return ret;
}

// Push both ends of the link `up`-`pos` out of the volumes, keeping the link parallel.
// The three-term sums (dot, d1, d0) are two statements each (`x + y; += z`): the partial sum is
// then the variable's own pseudo (the target's `fmadds f7,..,f7`), which gives dot six refs and
// the shortest live range, so global-alloc hands out f7/f6/f5/f4/f3 to dot, rr, p1.x/y/z in that
// order. In the cylinder case l1/l0 are computed before ld (their loads come first), and rr last.
void penClothAtCkParallel(Vec* pos, Vec* up, PenAtWork* wk)
{
    Vec p1;
    Vec p0;
    Vec d;
    Vec nd;
    f32 lenSq;
    f32 len;
    f32 invLenSq;
    f32 push;
    int n;
    PenAt* a;

    if (wk == 0) {
        return;
    }
    p1 = *up;
    p0 = *pos;
    PSVECSubtract(&p0, &p1, &d);
    d.x = p0.x - p1.x;
    d.y = p0.y - p1.y;
    d.z = p0.z - p1.z;
    if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
        d.y = 1.0f;
    }
    PSVECNormalize(&d, &nd);
    lenSq = d.x * d.x + d.y * d.y + d.z * d.z;
    len = SQRTF(lenSq);
    if (len == 0.0f) {
        return;
    }
    invLenSq = 1.0f / lenSq;
    n = wk->num;
    a = wk->pAt - 1;
    while (n--) {
        a++;
        if (a->type == 0) {
            Vec v;
            f32 dot;
            f32 t;
            f32 rr;
            f32 d1;
            f32 d0;

            rr = a->r * a->r;
            dot = (a->p0.x - p1.x) * d.x + (a->p0.y - p1.y) * d.y;
            dot += (a->p0.z - p1.z) * d.z;
            t = dot * invLenSq;
            v.x = nd.x * t + p1.x - a->p0.x;
            v.y = nd.y * t + p1.y - a->p0.y;
            v.z = nd.z * t + p1.z - a->p0.z;
            push = v.x * v.x + v.y * v.y + v.z * v.z;
            if (push >= rr) {
                continue;
            }
            d1 = (a->p0.x - p1.x) * (a->p0.x - p1.x) + (a->p0.y - p1.y) * (a->p0.y - p1.y);
            d1 += (a->p0.z - p1.z) * (a->p0.z - p1.z);
            d0 = (a->p0.x - p0.x) * (a->p0.x - p0.x) + (a->p0.y - p0.y) * (a->p0.y - p0.y);
            d0 += (a->p0.z - p0.z) * (a->p0.z - p0.z);
            if (d1 > rr && d0 > rr) {
                if (dot < 0.0f) {
                    continue;
                }
                if (dot >= lenSq) {
                    continue;
                }
            }
            push = a->r - SQRTF(push);
            if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
                v.y = 1.0f;
            }
            PSVECNormalize(&v, &v);
            v.x *= push;
            v.y *= push;
            v.z *= push;
            p1.x += v.x;
            p1.y += v.y;
            p1.z += v.z;
            p0.x += v.x;
            p0.y += v.y;
            p0.z += v.z;
        } else {
            Vec lp1;
            Vec lp0;
            Vec ld;
            Vec tmp;
            Vec q;
            f32 dot;
            f32 t;
            f32 rr;
            f32 ldSq;
            f32 l1;
            f32 l0;

            PSMTXMultVec(a->inv, &p1, &lp1);
            PSMTXMultVec(a->inv, &p0, &lp0);
            l1 = lp1.x * lp1.x + lp1.z * lp1.z;
            l0 = lp0.x * lp0.x + lp0.z * lp0.z;
            ld.x = lp0.x - lp1.x;
            ld.y = lp0.y - lp1.y;
            ld.z = lp0.z - lp1.z;
            tmp = ld;
            ld.y = 0.0f;
            dot = (-lp1.x) * ld.x + (-lp1.z) * ld.z;
            ldSq = ld.x * ld.x + ld.z * ld.z;
            rr = a->r * a->r;
            if (l1 > rr && l0 > rr) {
                if (dot < 0.0f) {
                    continue;
                }
                if (dot >= ldSq) {
                    continue;
                }
            }
            t = dot / ldSq;
            if (ld.x == 0.0f && ld.z == 0.0f) {
                ld.x = 1.0f;
            }
            PSVECNormalize(&tmp, &tmp);
            q.x = tmp.x * t + lp1.x;
            q.y = tmp.y * t + lp1.y;
            q.z = tmp.z * t + lp1.z;
            if (q.y < 0.0f) {
                continue;
            }
            if (q.y > a->len) {
                continue;
            }
            q.y = 0.0f;
            push = q.x * q.x + q.z * q.z;
            if (push >= a->r * a->r) {
                continue;
            }
            push = a->r - SQRTF(push);
            if (q.x == 0.0f && q.y == 0.0f && q.z == 0.0f) {
                continue;
            }
            PSVECNormalize(&q, &q);
            q.x *= push;
            q.y *= push;
            q.z *= push;
            lp1.x += q.x;
            lp1.y += q.y;
            lp1.z += q.z;
            lp0.x += q.x;
            lp0.y += q.y;
            lp0.z += q.z;
            PSMTXMultVec(a->mat, &lp1, &p1);
            PSMTXMultVec(a->mat, &lp0, &p0);
        }
    }
    *up = p1;
    *pos = p0;
}

// Put every link back to its rest pose (the model was warped: be_flag 0x00200000).
static void PenClothReset(cModel* m, PenCloth* c)
{
    cParts* parts;
    PEN_INFO* w;
    const u8* pp;
    u32 i;

    for (i = 0, pp = c->pCloth; i < c->Num; i++, pp++) {
        cCoord* parent;

        parts = PEN_PARTS(m, c, *pp);
        w = &parts->Pen;
        parent = parts->pParent;
        RotMatrix(parts->l_mat, &parts->ang);
        TransMatrix(parts->l_mat, &parts->pos);
        ScaleMatrix(parts->l_mat, &parts->scale);
        PSMTXConcat(parent->mat, parts->l_mat, parts->mat);
        PSMTXMultVec(parent->mat, &parts->pos, &parts->world);
        PSMTXMultVec(parts->mat, &w->Offset, &w->Pos);
        w->Pos_old = w->Pos;
        w->Speed.x = 0.0f;
        w->Speed.y = 0.0f;
        w->Speed.z = 0.0f;
    }
    {
        cParts* root = m->getPartsPtr(0);
        root->world_old = root->world;
        root->world_old2 = root->world;
    }
}

// Room / scenario wind for all cloth: direction `dir` (yaw, radians) at strength power * 20 units,
// and the per-frame random phase step x * PI/3 (GlobalWindAdd).
void PenWindSet(f32 dir, f32 power, f32 x)
{
    static const Vec vec0 = {0.0f, 0.0f, 1.0f};
    // the constants enter the pool at their declaration (before the 0.0 of rot), the uses are
    // folded to the literals
    const f32 powerRate = 20.0f;
    const f32 addRate = PI / 3.0f;
    Vec rot;

    rot.x = 0.0f;
    rot.y = dir;
    rot.z = 0.0f;
    RotVector((Vec*) &vec0, &rot, &GlobalWind);
    PSVECScale(&GlobalWind, &GlobalWind, power * powerRate);
    GlobalWindAdd = x * addRate;
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata; .balign 8");
