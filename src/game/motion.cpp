// game/motion: skeletal animation playback (D:/Bio4/Prog/motion.cpp). A MotionWork plays a
// MotionData (per joint: kind byte, Fcc key type, and per-axis Hermite key streams) on a cModel's
// parts. MotionSetCore starts a motion (optionally through a sequence table of 10.6 fixed-point
// frames with SE/free bytes, with `hokan` frames of blend from the previous pose); MotionMove
// advances it each game frame: root speed applied to the model (Mot_attr bit 0), a second
// blended MotionWork (blend/Brate), the per-parts keys (MotionMoveCore), leg IK, the pose
// interpolation (MotionHokan) and the quaternion blend table. Fcc_get_data_* decode the ten key
// stream layouts (f32 / s16 values and tangents).
#include "motion.h"
#include "global.h"
#include "db_log.h"
#include "math_sub.h"
#include "main_mem.h"
#include "player.h"
#include "eprintf.h"
#include <string.h>


// Matrix copy written out as loops (the original never calls PSMTXCopy for these), a variant of
// MTX_COPY (vec.h). Shape matters (all four sites byte-identical only this way): dst pointer first,
// the row counter `i_ = 2` between the two pointers, `for (; i_ != -1; i_--)` (a `while (i_--)`
// leaves the folded `li 2` behind the source pointer), `d_++` before `s_++` (gcse numbers the
// hoisted `+16` pseudos in that order).
#define MTX_COPY_DOWN(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        int i_ = 2;                      \
        MtxPtr s_ = (src);               \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        for (; i_ != -1; i_--) {         \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        }                                \
    }

// Root key history for the current flip state: hist[flip][rot = 0 / pos = 1]
#define MOT_HIST(w, flip, n) ((u16*) ((u8*) (w) + ((flip) * 12 + 8 + (n) * 6)))

// Fixed 10.6 sequence frame -> float
#define SEQ_FRAME(k) ((f32)((k) >> 6) + (f32)((k) & 0x3F) * 0.015625f)

typedef void (*FccGetData)(u8* data, int i0, int i1, f32* val, f32* tan);

extern "C" {
void Fcc_get_data_000(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_001(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_002(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_010(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_011(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_012(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_020(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_021(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_022(u8* d, int i0, int i1, f32* v, f32* t);
void Fcc_get_data_033(u8* d, int i0, int i1, f32* v, f32* t);
void dummy(u8* d, int i0, int i1, f32* v, f32* t);
}

// Moves every parts' world position/matrix by the model's displacement since the last pose
// computation (Pos_world), without recomputing the pose (cheap follow after setPos-style moves).
void PartsWorldPosCalc(cModel* pMod)
{
    cParts* p;
    Vec d;

    p = pMod->pList;
    PSVECSubtract(&pMod->pos, &MOTION(pMod)->Pos_world, &d);
    pMod->mat[0][3] += d.x;
    pMod->mat[1][3] += d.y;
    pMod->mat[2][3] += d.z;
    if (PSVECMag(&d) != 0.0f) {
        for (; p != 0; p = p->pList) {
            PSVECAdd(&p->world, &d, &p->world);
            p->mat[0][3] += d.x;
            p->mat[1][3] += d.y;
            p->mat[2][3] += d.z;
        }
        MOTION(pMod)->Pos_world = pMod->pos;
    }
}

// Drops the blended second motion.
void MotionBlendOff(cModel* pEm)
{
    MOTION(pEm)->blend = 0;
}

// Pauses the motion (Mot_attr bit 3: the sequence frame stops advancing).
void MotionPause(cModel* pEm)
{
    MOTION(pEm)->Mot_attr |= 8;
}

// Removes the motion: every parts back to its bind pose (position from the bind matrices, zero
// rotation, unit scale unless flag bit 0), the attach camera reset, pMot = NULL.
void MotionClear(cModel* pEm, int flag)
{
    MotionWork* w = MOTION(pEm);
    cParts* p;
    Mtx tmp;
    Mtx inv;
    int i;
    int j;

    for (p = pEm->pList; p != 0; p = p->pList) {
        if (pEm != p->pParent) {
            PSMTXInverse(p->lt_inv_mat, tmp);
            PSMTXConcat(((cParts*) p->pParent)->lt_inv_mat, tmp, inv);
        } else {
            PSMTXInverse(p->lt_inv_mat, inv);
        }
        p->pos.x = inv[0][3];
        p->pos.y = inv[1][3];
        p->pos.z = inv[2][3];
        p->ang.x = 0.0f;
        p->ang.y = 0.0f;
        p->ang.z = 0.0f;
        if (!(flag & 1)) {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
    }
    if (w->pAttachCam != 0) {
        for (i = 0; i < 5; i++) {
            w->pAttachCam->parts[i] = 0xFF;
        }
        w->pAttachCam->type = 0;
        for (j = 0; j < 5; j++) {
            memclr_asm(&w->pAttachCam->camera_data[j], sizeof(Vec));
        }
    }
    MOTION(pEm)->pMot = 0;
}

// Starts motion `data` on work w (usually MOTION(m)): resets root pos/rot state, Mot_attr = flags
// (bit 0 apply root speed, 1 reverse, 2 loop, 6 flip left/right, ...), the sequence (seq table
// or linear over maxFrame+1 frames) starting at `frame`, the joint tables and key stream
// pointers (relocated once), IK chains (unless Mot_flag 0x10000000), the root pos/rot joint
// indices and attach camera channels, clears the key histories, sets the `hokan` blend frames
// (saving the current l_mat as prevMat), and samples the root at the start/end to get the
// motion's total displacement (Pos_dist/Ang_dist) for looping. Mot_flag 0x20000000 keeps the
// blend motion.
void MotionSetCore(cModel* m, void* w_, void* data_, void* seq_, int hokan, int flags, int frame)
{
    MotionWork* w = (MotionWork*) w_;
    MotionData* data = (MotionData*) data_;
    u16* seq = (u16*) seq_;
    HermitePrm prm;
    HermitePrm* pp = &prm;
    u16 hist0[3] = { 0, 0, 0 };
    u16 hist1[3] = { 0, 0, 0 };
    Vec v0;
    Vec v1;
    Vec v2;
    u32* tbl;
    cParts* p;
    AttachCamera* cam;
    int f;
    int i;

    if (!(w->Mot_flag & 0x20000000)) {
        MOTION(m)->blend = 0;
    }
    w->Pos_old = vecZero;
    w->Pos = w->Pos_old;
    w->Pos_dist = vecZero;
    w->Ang_old = vecZero;
    w->Ang = w->Ang_old;
    w->Ang_dist = vecZero;
    w->pMot = data;
    w->Mot_attr = flags;
    w->Mot_state = 0;
    w->Mot_flag = (w->Mot_flag & 0x7FFFFFFF) | 0x04000000;
    if (data == 0) {
        if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
#line 273
            pLog->err(0, 0, "MotionSetCore():%d pMot == NULL", __LINE__);
        }
        return;
    }
    if (seq == 0) {
        w->pSeq_top = 0;
        w->Seq_frame_num = data->maxFrame & 0x3FFF;
        if (!(flags & 4)) {
            w->Seq_frame_num++;
        }
        if ((u32) frame >= w->Seq_frame_num) {
            frame = (u16) (w->Seq_frame_num - 1);
        }
        w->Seq.Se = 0;
        w->Seq.Free = 0;
        w->Seq_frame = (f32) (u16) frame;
        w->Seq_old.Se = 0;
        w->Seq_old.Free = 0;
        w->Seq_old2.Se = 0;
        w->Seq_old2.Free = 0;
    } else {
        w->pSeq_top = (MotionSeqKey*) (seq + 2);
        w->Seq_frame_num = seq[0];
        if ((u32) frame >= w->Seq_frame_num) {
            frame = (u16) (w->Seq_frame_num - 1);
        }
        w->Seq_frame = (f32) (u16) frame;
        w->Mot_attr = (((u8*) seq)[2] & 1) ? (flags | 0x1000) : (flags & 0xEFFF);
    }
    if (w->pSeq_top == 0) {
        w->Seq.frame = (u16) (w->Seq_frame * 64.0f);
        w->Seq_old.frame = (u16) (w->Seq_frame * 64.0f);
    } else {
        MotionSeqKey k = w->pSeq_top[(u16) w->Seq_frame];

        w->Seq = k;
        w->Seq_old = k;
        w->Seq_old2 = k;
        w->Seq_old2.Se = 0;
        w->Seq_old2.Free = 0;
    }
    w->Mot_frame_max = (f32) w->pMot->maxFrame;
    w->Joint_num = w->pMot->nParts;
    w->pJoint_kind = (u16*) ((u8*) w->pMot + 3);
    w->pJoint_no = (u8*) w->pMot + (w->Joint_num * 2 + 3);
    if (!(w->Mot_flag & 0x10000000)) {
        IKInit(m, w);
    }
    // Two statements: the end pointer lives in `tbl` (r10) before the align (one expression ties the
    // partsNo reload to the sum and allocates it).
    tbl = (u32*) ((u32) w->pJoint_no + w->Joint_num);
    tbl = (u32*) (((u32) tbl + 3) & ~3);
    tbl++;
    if ((s32) tbl[0] >= 0) {
        for (i = 0; i < w->Joint_num; i++) {
            tbl[i] += (u32) w->pMot;
        }
    }
    w->pHermite_data = tbl;
    w->Null_rot = 0xFFFF;
    w->Null_pos = 0xFFFF;
    if (w->pAttachCam != 0) {
        for (i = 0; i < 5; i++) {
            w->pAttachCam->parts[i] = 0xFF;
        }
        w->pAttachCam->type = 0;
        for (i = 0; i < 5; i++) {
            memclr_asm(&w->pAttachCam->camera_data[i], sizeof(Vec));
        }
    }
    for (i = 0; i < w->Joint_num; i++) {
        u16 info = w->pJoint_kind[i];
        int kind = info & 0xFF;
        int ch = (info >> 8) & 0xF;
        u8 pno;

        if (kind == 1) {
            w->Null_pos = i;
            continue;
        }
        if (kind == 0x40) {
            w->Null_rot = i;
            continue;
        }
        if (w->pAttachCam == 0) {
            continue;
        }
        if (ch != 6 && ch != 7) {
            continue;
        }
        if (w->Mot_attr & 0x100) {
            continue;
        }
        if (ch == 6) {
            w->pAttachCam->type = 1;
        } else if (ch == 7) {
            w->pAttachCam->type = 2;
        }
        pno = w->pJoint_no[i];
        if (kind == 4) {
            if (pno == 0) {
                w->pAttachCam->parts[0] = i;
            } else if (pno == 1) {
                w->pAttachCam->parts[1] = i;
            } else if (pno == 4) {
                w->pAttachCam->parts[4] = i;
            }
        } else if (kind == 2) {
            if (pno == 2) {
                w->pAttachCam->parts[2] = i;
            } else if (pno == 3) {
                w->pAttachCam->parts[3] = i;
            }
        }
    }
    for (p = m->pList; p != 0; p = p->pList) {
        if (p->motParts.flags & 0x04000000) {
            continue;
        }
        memclr_asm(p->getKeyHist()[0], 6);
        memclr_asm(p->getKeyHist()[1], 6);
        memclr_asm(p->getKeyHist()[2], 6);
        memclr_asm(p->getKeyHist()[3], 6);
        memclr_asm(p->getKeyHist()[4], 6);
        memclr_asm(p->getKeyHist()[5], 6);
    }
    w->Key_hist[0][0][0] = w->Key_hist[0][0][1] = w->Key_hist[0][0][2] = 0;
    w->Key_hist[0][1][0] = w->Key_hist[0][1][1] = w->Key_hist[0][1][2] = 0;
    w->Key_hist[1][0][0] = w->Key_hist[1][0][1] = w->Key_hist[1][0][2] = 0;
    w->Key_hist[1][1][0] = w->Key_hist[1][1][1] = w->Key_hist[1][1][2] = 0;
    if (w->pAttachCam != 0) {
        memclr_asm(w->pAttachCam->history[0], 6);
        memclr_asm(w->pAttachCam->history[1], 6);
        memclr_asm(w->pAttachCam->history[2], 6);
        memclr_asm(w->pAttachCam->history[3], 6);
        memclr_asm(w->pAttachCam->history[4], 6);
    }
    if (hokan != 0) {
        w->Hokan_frame = hokan;
        w->Hokan_cnt = hokan;
        for (p = m->pList; p != 0; p = p->pList) {
            PSMTXCopy(p->l_mat, p->prevMat);
        }
    } else {
        w->Hokan_frame = 0;
        w->Hokan_cnt = 0;
    }
    w->Mot_state = 0;
    if (w->Mot_attr & 2) {
        f = frame + 1;
        if (f >= w->Seq_frame_num) {
            f = w->Seq_frame_num - 1;
        }
    } else {
        f = frame - 1;
        if (f < 0) {
            f = 0;
        }
    }
    u32 zero = 0;
    pp->flags = zero;
    pp->maxFrame = w->Mot_frame_max;
    if (w->Mot_attr & 2) {
        if (!(w->Mot_attr & 0x1000)) {
            asm("" : : "r"(zero));  // COMPILER-DIFF: dead use makes the zero global (r11), w->flags takes r0
            pp->flags = 2;
        }
    } else {
        if (w->Mot_attr & 0x1000) {
            pp->flags = 2;
        } else {
            pp->flags = 0;
        }
    }
    if (w->pSeq_top == 0) {
        w->Mot_frame = (f32) f;
    } else {
        u16 k = w->pSeq_top[f].frame;
        w->Mot_frame = (f32) (k >> 6) + (f32) (k & 0x3F) * 0.0015625f;
    }
    w->Mot_frame_sav = w->Mot_frame;
    w->Mot_frame_old = w->Mot_frame;
    pp->frame = w->Mot_frame;
    if (w->Null_pos != 0xFFFF) {
        pp->type = w->pJoint_kind[w->Null_pos] >> 12;
        pp->key = (u8*) w->pHermite_data[w->Null_pos];
        HermiteInterpolation(pp, &w->Pos, hist0);
        w->Pos_old = w->Pos;
    }
    if (w->Null_rot != 0xFFFF) {
        pp->type = w->pJoint_kind[w->Null_rot] >> 12;
        pp->key = (u8*) w->pHermite_data[w->Null_rot];
        HermiteInterpolation(pp, &w->Ang, hist1);
        w->Ang_old = w->Ang;
    }
    if (w->Null_pos != 0xFFFF) {
        pp->frame = 0.0f;
        pp->type = w->pJoint_kind[w->Null_pos] >> 12;
        pp->key = (u8*) w->pHermite_data[w->Null_pos];
        HermiteInterpolation(pp, &v0, hist0);
        pp->frame = w->Mot_frame_max;
        pp->flags |= 2;
        HermiteInterpolation(pp, &v1, hist0);
        PSVECSubtract(&v1, &v0, &w->Pos_dist);
    }
    if (w->Null_rot != 0xFFFF) {
        pp->frame = 0.0f;
        pp->type = w->pJoint_kind[w->Null_rot] >> 12;
        pp->key = (u8*) w->pHermite_data[w->Null_rot];
        HermiteInterpolation(pp, &v0, hist1);
        pp->frame = w->Mot_frame_max;
        pp->flags |= 2;
        HermiteInterpolation(pp, &v1, hist1);
        PSVECSubtract(&v1, &v0, &w->Ang_dist);
        VecRadLimit(&w->Ang_dist);
    }
    cam = w->pAttachCam;
    if (cam != 0) {
        if (cam->type != 0) {
            if (cam->parts[4] != 0xFF) {
                pp->frame = 0.0f;
                pp->type = w->pJoint_kind[cam->parts[4]] >> 12;
                pp->key = (u8*) w->pHermite_data[cam->parts[4]];
                HermiteInterpolation(pp, &v2, hist0);
            }
            cam->frame = 0;
            asm volatile("" : : : "memory");  // COMPILER-DIFF: anchor, the type load stays after the frame store
            if (cam->type == 1) {
                cam->p_mat = &m->mat;
            } else if (cam->type == 2) {
                MTX_COPY_DOWN(m->mat, cam->mat);
                cam->p_mat = &cam->mat;
            }
        }
        if (w->pAttachCam != 0) {
            if (w->pAttachCam->type != 0) {
                CamCtrl.registAttachCamera(w->pAttachCam, m);
            } else {
                CamCtrl.deleteAttachCamera(w->pAttachCam, m);
            }
        }
    }
}

// One frame of the model's motion: with Mot_attr bit 0 moves the model by the root speed (mixed
// with the blend motion by Brate), evaluates the main motion (MotionMoveCore + sequence step),
// then the blend motion either as a matrix slerp (matBlend) or, for Mot_flag sign-bit blends, as
// an additive pose; runs the leg IK on the unscaled model, the hokan interpolation and the
// quaternion blend table (blendTbl: dst = slerp(c, a, percent)). Returns Mot_state (1/2 looped,
// 4/8 ended).
u32 MotionMove(cModel* pEm, Camera* pCamera)
{
    static int new_add = 1;
    cParts* p;
    Vec spd;
    Vec rot;
    Vec spd2;
    Vec rot2;
    f32 rate;
    f32 inv;

    if (MOTION(pEm)->blend != 0) {
        MOTION(pEm)->blend->Seq_speed = MOTION(pEm)->Seq_speed;
    }
    if (MOTION(pEm)->Mot_attr & 1) {
        MotionGetSpeed(pEm, MOTION(pEm), 0, &spd, &rot);
        if (MOTION(pEm)->blend != 0) {
            MOTION(pEm)->blend->Mot_flag |= 0x08000000;
            rate = MOTION(pEm)->blend->Brate;
            if (!(MOTION(pEm)->blend->Mot_flag & 0x80000000)) {
                if (rate != 0.0f) {
                    MOTION(pEm)->blend->Hokan_cnt = 0;
                    MotionGetSpeed(pEm, MOTION(pEm)->blend, 0, &spd2, &rot2);
                    inv = 1.0f - rate;
                    VecLinearCombination(&spd2, rate, &spd, inv, &spd);
                    VecLinearCombination(&rot2, rate, &rot, inv, &rot);
                }
            } else {
                if (rate != 0.0f) {
                    MOTION(pEm)->blend->Hokan_cnt = 0;
                    MotionGetSpeed(pEm, MOTION(pEm)->blend, 0, &spd2, &rot2);
                    VecLinearCombination(&spd2, rate, &spd, 1.0f, &spd);
                    VecLinearCombination(&rot2, rate, &rot, 1.0f, &rot);
                }
            }
        }
        MotionAddSpeed(pEm, MOTION(pEm), &spd, &rot);
    }
    MotionMoveCore(pEm, MOTION(pEm), 0);
    MotionSequenceCtrl(MOTION(pEm));
    pEm->partsMatCalc();
    MOTION(pEm)->Mot_attr &= ~0x2000;
    if (MOTION(pEm)->blend != 0) {
        MOTION(pEm)->blend->Mot_flag |= 0x08000000;
        rate = MOTION(pEm)->blend->Brate;
        if (!(MOTION(pEm)->blend->Mot_flag & 0x80000000)) {
            if (rate != 0.0f) {
                MotionMoveCore(pEm, MOTION(pEm)->blend, 0);
                MotionSequenceCtrl(MOTION(pEm)->blend);
                pEm->matBlend(MOTION(pEm)->blend->Brate);
            } else {
                MotionSequenceCtrl(MOTION(pEm)->blend);
            }
        } else {
            MOTION(pEm)->Mot_attr |= 0x2000;
            for (p = pEm->pList; p != 0; p = p->pList) {
                if (p->motParts.flags & 0x03000000) {
                    continue;
                }
                p->motParts.pos = p->pos;
                p->motParts.rot = p->ang;
                p->motParts.scale = p->scale;
                memclr_asm(&p->ang, sizeof(Vec));
                memclr_asm(&p->pos, sizeof(Vec));
                if (new_add) {
                    memclr_asm(&p->scale, sizeof(Vec));
                }
            }
            MotionMoveCore(pEm, MOTION(pEm)->blend, 0);
            MotionSequenceCtrl(MOTION(pEm)->blend);
            for (p = pEm->pList; p != 0; p = p->pList) {
                if (p->motParts.flags & 0x03000000) {
                    continue;
                }
                if (new_add) {
                    PSVECAdd(&p->motParts.pos, &p->pos, &p->pos);
                    PSVECAdd(&p->motParts.rot, &p->ang, &p->ang);
                    PSVECAdd(&p->motParts.scale, &p->scale, &p->scale);
                } else {
                    Vec rotAdd;
                    Vec rotScl;
                    Vec posAdd;
                    Vec posScl;

                    PSVECScale(&p->ang, &rotScl, rate);
                    PSVECScale(&p->pos, &posScl, rate);
                    PSVECAdd(&p->motParts.pos, &posScl, &posAdd);
                    PSVECAdd(&p->motParts.rot, &rotScl, &rotAdd);
                    p->pos = posAdd;
                    p->ang = rotAdd;
                    RotMatrix(p->l_mat, &p->ang);
                    TransMatrix(p->l_mat, &p->pos);
                    ScaleMatrix(p->l_mat, &p->scale);
                    PSMTXCopy(p->l_mat, p->mat);
                }
            }
            if (new_add) {
                pEm->matBlend(MOTION(pEm)->blend->Brate);
            }
        }
    }
    {
        Mtx tmp;
        Mtx save;

        MTX_COPY_DOWN(pEm->mat, save);
        if (pEm->scale.x != 0.0f || pEm->scale.y != 0.0f || pEm->scale.z != 0.0f) {
            PSMTXScale(tmp, 1.0f / pEm->scale.x, 1.0f / pEm->scale.y, 1.0f / pEm->scale.z);
        } else {
            PSMTXIdentity(tmp);
        }
        PSMTXConcat(pEm->mat, tmp, tmp);
        MTX_COPY_DOWN(tmp, pEm->mat);
        pEm->partsWorldCalc();
        InverseKinematics(pEm, 1);
        MTX_COPY_DOWN(save, pEm->mat);
        pEm->partsWorldCalc();
    }
    if (MOTION(pEm)->Hokan_cnt != 0) {
        MotionHokan(pEm, MOTION(pEm));
        pEm->partsWorldCalc();
    }
    if (MOTION(pEm)->blendTbl != 0) {
        int n = *(s32*) MOTION(pEm)->blendTbl;
        u16* tbl = MOTION(pEm)->blendTbl + 2;
        int i;

        {
            Mtx m0;
            Mtx m1;
            Mtx mq;
            Quaternion q0;
            Quaternion q1;
            Quaternion q2;
            Mtx inv;
            cParts* dst;
            cParts* a;
            cParts* c;
            int per;
            f32 r;

            for (i = 0; i < n; i++) {
                dst = pEm->getPartsPtr(*tbl++);
                a = pEm->getPartsPtr(*tbl++);
                c = pEm->getPartsPtr(*tbl++);
                per = *tbl++;
                r = (f32) per / 100.0f;
                if (dst->motParts.flags & 0x10000) {
                    dst->motParts.flags &= ~0x10000;
                    continue;
                }
                dst->motParts.flags &= ~0x10000000;
                PSMTXIdentity(m0);
                m0[0][0] = a->mat[0][0];
                m0[0][1] = a->mat[0][1];
                m0[0][2] = a->mat[0][2];
                m0[1][0] = a->mat[1][0];
                m0[1][1] = a->mat[1][1];
                m0[1][2] = a->mat[1][2];
                m0[2][0] = a->mat[2][0];
                m0[2][1] = a->mat[2][1];
                m0[2][2] = a->mat[2][2];
                C_QUATMtx(&q0, m0);
                PSMTXIdentity(m1);
                m1[0][0] = c->mat[0][0];
                m1[0][1] = c->mat[0][1];
                m1[0][2] = c->mat[0][2];
                m1[1][0] = c->mat[1][0];
                m1[1][1] = c->mat[1][1];
                m1[1][2] = c->mat[1][2];
                m1[2][0] = c->mat[2][0];
                m1[2][1] = c->mat[2][1];
                m1[2][2] = c->mat[2][2];
                C_QUATMtx(&q1, m1);
                C_QUATSlerp(&q1, &q0, &q2, r);
                PSMTXQuat(mq, &q2);
                dst->mat[0][0] = mq[0][0];
                dst->mat[0][1] = mq[0][1];
                dst->mat[0][2] = mq[0][2];
                dst->mat[1][0] = mq[1][0];
                dst->mat[1][1] = mq[1][1];
                dst->mat[1][2] = mq[1][2];
                dst->mat[2][0] = mq[2][0];
                dst->mat[2][1] = mq[2][1];
                dst->mat[2][2] = mq[2][2];
                dst->r_scale.x = c->r_scale.x * (1.0f - r) + a->r_scale.x * r;
                dst->r_scale.y = c->r_scale.y * (1.0f - r) + a->r_scale.y * r;
                dst->r_scale.z = c->r_scale.z * (1.0f - r) + a->r_scale.z * r;
                ScaleMatrix(dst->mat, &dst->r_scale);
                PSMTXInverse(dst->pParent->mat, inv);
                PSMTXConcat(inv, dst->mat, dst->l_mat);
            }
        }
    }
    pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
    return MOTION(pEm)->Mot_state;
}

// Advances a secondary MotionWork (no root speed): pose, sequence, hokan. Returns its Mot_state.
u16 MotionMoveSub(cModel* pEm, MotionWorkSub* w)
{
    MotionMoveCore(pEm, w, 0);
    MotionSequenceCtrl(w);
    if (w->Hokan_cnt != 0) {
        MotionHokan(pEm, w);
    }
    return w->Mot_state;
}

// Evaluates the pose at the current sequence frame: for each motion joint decodes the Hermite
// keys of the axes it animates (kind bit 1 rotation, else rot + pos + scale) into the parts'
// ang/pos/scale (with the left/right flip remap and mirroring when Mot_attr 0x40), skipping parts
// flagged 0x20000000; attach-camera channels 6/7 go to the AttachCamera outputs. Rebuilds the
// model matrix unless Mot_flag 0x40000000.
void MotionMoveCore(cModel* pEm, MotionWorkSub* w, Camera* pCamera)
{
    HermitePrm prm;
    HermitePrm* pp = &prm;
    AttachCamera* cam;
    cParts* p;
    u16* flipTbl = MOTION(pEm)->flip;
    int n = w->Joint_num;
    int i = 0;
    int flip;

    if (w->pMot == 0) {
        return;
    }
    if (!(w->Mot_attr & 0x8000)) {
        w->Mot_frame = SEQ_FRAME(w->Seq.frame);
    } else {
        w->Mot_frame = w->Seq_frame;
    }
    w->Mot_frame_old = w->Mot_frame_sav;
    w->Mot_frame_sav = w->Mot_frame;
    pp->frame = w->Mot_frame;
    pp->maxFrame = w->Mot_frame_max;
    pp->flags = 0;
    if (w->Mot_attr & 4) {
        if (w->pSeq_top == 0) {
            pp->flags = 4;
        }
    }
    if (w->Mot_attr & 2) {
        if (!(w->Mot_attr & 0x1000)) {
            pp->flags |= 2;
        } else {
            pp->flags &= ~2;
        }
    } else {
        if (w->Mot_attr & 0x1000) {
            pp->flags |= 2;
        } else {
            pp->flags &= ~2;
        }
    }
    if (!(w->Mot_flag & 0x40000000)) {
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        ScaleMatrix(pEm->mat, &pEm->scale);
    }
    flip = 0;
    if (w->Mot_flag & 0x08000000) {
        flip = 1;
    }
    w->Mot_flag &= ~0x04000000;
    cam = 0;
    if (w->pAttachCam != 0 && w->pAttachCam->type != 0) {
        cam = w->pAttachCam;
    }
    if ((w->Mot_attr & 0x40) && flipTbl == 0) {
        w->Mot_attr &= ~0x40;
#line 1067
        pLog->err(0, 0, "MotionMoveCore():%d Flip Info Error!", __LINE__);
    }
    do {
        int kind = w->pJoint_kind[i] & 0xFF;
        u16 info = w->pJoint_kind[i];
        int ch = (info >> 8) & 0xF;
        int pno = w->pJoint_no[i];

        if (ch == 6 || ch == 7) {
            if (cam == 0) {
                continue;
            }
            pp->type = info >> 12;
            pp->key = (u8*) w->pHermite_data[i];
            if (i == cam->parts[0]) {
                HermiteInterpolation(pp, &cam->camera_data[0], cam->history[0]);
                if (w->Mot_attr & 0x40) {
                    cam->camera_data[0].x = -cam->camera_data[0].x;
                }
            } else if (i == cam->parts[1]) {
                HermiteInterpolation(pp, &cam->camera_data[1], cam->history[1]);
                if (w->Mot_attr & 0x40) {
                    cam->camera_data[1].x = -cam->camera_data[1].x;
                }
            } else if (i == cam->parts[2]) {
                HermiteInterpolation(pp, &cam->camera_data[2], cam->history[2]);
                if (w->Mot_attr & 0x40) {
                    cam->camera_data[2].y = -cam->camera_data[2].y;
                }
            } else if (i == cam->parts[3]) {
                HermiteInterpolation(pp, &cam->camera_data[3], cam->history[3]);
            } else if (i == cam->parts[4]) {
                HermiteInterpolation(pp, &cam->camera_data[4], cam->history[4]);
                cam->frame = (u8) (cam->camera_data[4].y / 100.0f);
            }
            continue;
        }
        if (w->Mot_attr & 0x40) {
            u16 fp = flipTbl[pno];
            if (fp != 0xFFFF) {
                pno = (s16) fp;
            }
        }
        if (pno >= pEm->nParts) {
            static const char* kind_str[] = { "Em", "Obj", "Scr", "Shadow", "Mirror" };

            if (pPL == pEm) {
                pLog->err(0, 0, "MotionMoveCore(): Pl, Invalid parts %d.", pno);
            } else {
                pLog->err(0, 0, "MotionMoveCore(): %s[%0xh], Invalid parts %d. [0x%x]", kind_str[pEm->kindid], pEm->id, pno, pEm);
            }
        }
        p = pEm->getPartsPtr(pno);
        if (p == 0) {
            continue;
        }
        if (p->motParts.flags & 0x20000000) {
            continue;
        }
        p->motParts.flags |= 0x10010000;
        if (w->Mot_flag & 0x80000000) {
            p->motParts.flags |= 0x80000000;
        }
        pp->type = w->pJoint_kind[i] >> 12;
        pp->key = (u8*) w->pHermite_data[i];
        if (p->motParts.flags & 0x04000000) {
            pp->flags |= 8;
        } else {
            pp->flags &= ~8;
        }
        if (kind & 2) {
            HermiteInterpolation(pp, &p->ang, p->getKeyHist()[flip ? 3 : 0]);
            VecRadLimit(&p->ang);
            if (w->Mot_attr & 0x40) {
                p->ang.y = -p->ang.y;
                p->ang.z = -p->ang.z;
            }
        } else if (kind & 4) {
            HermiteInterpolation(pp, &p->pos, p->getKeyHist()[flip ? 4 : 1]);
            if (w->Mot_attr & 0x40) {
                p->pos.x = -p->pos.x;
            }
        } else if (kind & 8) {
            HermiteInterpolation(pp, &p->scale, p->getKeyHist()[flip ? 5 : 2]);
        } else if (kind & 0x30) {
            HermiteInterpolation(pp, &p->ang, p->getKeyHist()[flip ? 3 : 0]);
            VecRadLimit(&p->ang);
            if (w->Mot_attr & 0x40) {
                p->ang.y = -p->ang.y;
                p->ang.z = -p->ang.z;
            }
        }
    } while (++i < n);
}

// |d| < eps.
// The caller forms `1.0f - v`: with the subtraction inside the inline body the argument copy
// (a load) precedes the constant load in RTL and sched1 keeps that order (equal prio/weight).
static inline int nearZero(f32 d, f32 eps)
{
    if (fabsf(d) < eps) {
        return 1;
    }
    return 0;
}

// Pose interpolation over the Hokan_frame frames after a motion change: each parts' l_mat is
// blended between prevMat and the new pose (translation linear, rotation by quaternion slerp,
// scale linear or cancelled by the parent's scale with flag 0x20000).
void MotionHokan(cModel* m, MotionWorkSub* w)
{
    static int g_scale_cancel = 1;
    static f32 epsilon = 0.00002f;
    static f32 EPS = 0.1f;
    cParts* p;
    Vec pos;
    Quaternion q0;
    Quaternion q1;
    Quaternion q2;
    Mtx m0;
    Mtx m1;
    Vec c0;
    Vec c1;
    Vec c2;
    f32 t;
    f32 u;
    f32 s0, s1, s2;
    f32 n0, n1, n2;
    f32 sx, sz, sy;

    w->Hokan_cnt--;
    t = (f32) (w->Hokan_frame - w->Hokan_cnt);
    t /= (f32) w->Hokan_frame;
    u = 1.0f - t;
    for (p = m->pList; p != 0; p = p->pList) {
        if (!(MOTION(m)->Mot_attr & 0x2000)) {
            if (!(p->motParts.flags & 0x10000000)) {
                continue;
            }
            p->motParts.flags &= ~0x10000000;
        } else {
            if (!(p->motParts.flags & 0x80000000)) {
                continue;
            }
        }
        pos.x = p->prevMat[0][3] * u + p->l_mat[0][3] * t;
        pos.y = p->prevMat[1][3] * u + p->l_mat[1][3] * t;
        pos.z = p->prevMat[2][3] * u + p->l_mat[2][3] * t;
        c0.x = p->prevMat[0][0];
        c0.y = p->prevMat[1][0];
        c0.z = p->prevMat[2][0];
        c1.x = p->prevMat[0][1];
        c1.y = p->prevMat[1][1];
        c1.z = p->prevMat[2][1];
        c2.x = p->prevMat[0][2];
        c2.y = p->prevMat[1][2];
        c2.z = p->prevMat[2][2];
        s0 = PSVECMag(&c0);
        s1 = PSVECMag(&c1);
        s2 = PSVECMag(&c2);
        if (s0 != 0.0f) {
            PSVECScale(&c0, &c0, 1.0f / s0);
        }
        if (s1 != 0.0f) {
            PSVECScale(&c1, &c1, 1.0f / s1);
        }
        if (s2 != 0.0f) {
            PSVECScale(&c2, &c2, 1.0f / s2);
        }
        m0[0][0] = c0.x;
        m0[1][0] = c0.y;
        m0[2][0] = c0.z;
        m0[0][1] = c1.x;
        m0[1][1] = c1.y;
        m0[2][1] = c1.z;
        m0[0][2] = c2.x;
        m0[1][2] = c2.y;
        m0[2][2] = c2.z;
        c0.x = p->l_mat[0][0];
        c0.y = p->l_mat[1][0];
        c0.z = p->l_mat[2][0];
        c1.x = p->l_mat[0][1];
        c1.y = p->l_mat[1][1];
        c1.z = p->l_mat[2][1];
        c2.x = p->l_mat[0][2];
        c2.y = p->l_mat[1][2];
        c2.z = p->l_mat[2][2];
        n0 = PSVECMag(&c0);
        n1 = PSVECMag(&c1);
        n2 = PSVECMag(&c2);
        if (n0 != 0.0f) {
            PSVECScale(&c0, &c0, 1.0f / n0);
        }
        if (n1 != 0.0f) {
            PSVECScale(&c1, &c1, 1.0f / n1);
        }
        if (n2 != 0.0f) {
            PSVECScale(&c2, &c2, 1.0f / n2);
        }
        m1[0][0] = c0.x;
        m1[1][0] = c0.y;
        m1[2][0] = c0.z;
        m1[0][1] = c1.x;
        m1[1][1] = c1.y;
        m1[2][1] = c1.z;
        m1[0][2] = c2.x;
        m1[1][2] = c2.y;
        m1[2][2] = c2.z;
        C_QUATMtx(&q0, m0);
        C_QUATMtx(&q1, m1);
        C_QUATSlerp(&q0, &q1, &q2, t);
        PSMTXQuat(p->l_mat, &q2);
        if (g_scale_cancel) {
            // `one` is loaded before the first scale load (the target's constant load comes first).
            f32 one = 1.0f;
            if (!(nearZero(one - p->scale.x, epsilon) && nearZero(one - p->scale.y, epsilon) && nearZero(one - p->scale.z, epsilon))) {
                if (nearZero(1.0f - p->pParent->r_scale.x * p->scale.x, EPS) && nearZero(1.0f - p->pParent->r_scale.y * p->scale.y, EPS) &&
                    nearZero(1.0f - p->pParent->r_scale.z * p->scale.z, EPS)) {
                    p->motParts.flags |= 0x20000;
                }
            }
        } else {
            p->motParts.flags &= ~0x20000;
        }
        if (p->motParts.flags & 0x20000) {
            // Source order x, y, z: sched1 issues first the load where the pParent pointer dies (the last one).
            sx = 1.0f / p->pParent->scale.x;
            sy = 1.0f / p->pParent->scale.y;
            sz = 1.0f / p->pParent->scale.z;
        } else {
            sx = s0 * u + n0 * t;
            sy = s1 * u + n1 * t;
            sz = s2 * u + n2 * t;
        }
        // Before the products: the store is ready early and wins the LSU slot on LUID (equal priority).
        p->motParts.flags &= ~0x20000;
        p->l_mat[0][0] *= sx;
        p->l_mat[1][0] *= sx;
        p->l_mat[2][0] *= sx;
        p->l_mat[0][1] *= sy;
        p->l_mat[1][1] *= sy;
        p->l_mat[2][1] *= sy;
        p->l_mat[0][2] *= sz;
        p->l_mat[1][2] *= sz;
        p->l_mat[2][2] *= sz;
        p->r_scale = p->scale;
        p->scale.x = sx;
        p->scale.y = sy;
        p->scale.z = sz;
        TransMatrix(p->l_mat, &pos);
        {
            MtxPtr pm;
            asm("" : "=r"(pm) : "0"(p->prevMat));  // COMPILER-DIFF: launder, the extra insn on the r4 path ranks both addi r4 above mr r3
            PSMTXCopy(p->l_mat, pm);
        }
    }
}

// Root displacement of this frame: samples the root pos/rot joints at the current frame, subtracts
// the previous sample (adding/subtracting Pos_dist/Ang_dist across a loop), rotates the position
// delta by the previous root yaw into model space, mirrors it for flipped motions, and with
// Mot_attr 0x400 blends the XZ speed from the previous motion's speed over the hokan frames.
void MotionGetSpeed(cModel* pEm, MotionWorkSub* w, int flg, Vec* Pos_move, Vec* Ang_move)
{
    HermitePrm prm;
    HermitePrm* pp = &prm;
    Vec a = { 0.0f, 0.0f, 0.0f };
    Vec b = { 0.0f, 0.0f, 0.0f };
    Mtx rm;
    int flip;

    w->Mot_frame = SEQ_FRAME(w->Seq.frame);
    pp->flags = 0;
    w->Ang_old = w->Ang;
    w->Pos_old = w->Pos;
    if (w->Mot_attr & 2) {
        if (w->Mot_attr & 0x1000) {
            pp->flags = 0;
        } else {
            pp->flags = 2;
        }
    } else {
        if (w->Mot_attr & 0x1000) {
            pp->flags = 2;
        } else {
            pp->flags = 0;
        }
    }
    flip = 0;
    if (w->Mot_flag & 0x08000000) {
        flip = 1;
    }
    if (w->Null_pos != 0xFFFF) {
        pp->frame = w->Mot_frame;
        pp->maxFrame = w->Mot_frame_max;
        pp->key = (u8*) w->pHermite_data[w->Null_pos];
        pp->type = w->pJoint_kind[w->Null_pos] >> 12;
        HermiteInterpolation(pp, &a, MOT_HIST(w, flip, 1));
    }
    if (w->Null_rot != 0xFFFF) {
        pp->frame = w->Mot_frame;
        pp->maxFrame = w->Mot_frame_max;
        pp->key = (u8*) w->pHermite_data[w->Null_rot];
        pp->type = w->pJoint_kind[w->Null_rot] >> 12;
        HermiteInterpolation(pp, &b, MOT_HIST(w, flip, 0));
    }
    PSVECSubtract(&b, &w->Ang_old, Ang_move);
    PSVECSubtract(&a, &w->Pos_old, Pos_move);
    if (w->Mot_state & 1) {
        PSVECAdd(Pos_move, &w->Pos_dist, Pos_move);
        PSVECAdd(Ang_move, &w->Ang_dist, Ang_move);
    } else if (w->Mot_state & 2) {
        PSVECSubtract(Pos_move, &w->Pos_dist, Pos_move);
        PSVECSubtract(Ang_move, &w->Ang_dist, Ang_move);
    }
    PSMTXRotRad(rm, 'y', w->Ang_old.y);
    rm[0][2] = -rm[0][2];
    rm[2][0] = -rm[2][0];
    PSMTXMultVecSR(rm, Pos_move, Pos_move);
    if (w->Mot_attr & 0x40) {
        if (MOTION(pEm)->flip == 0) {
            w->Mot_attr &= ~0x40;
#line 1642
            pLog->err(0, 0, "MotionMoveCore():%d Flip Info Error!", __LINE__);
        } else {
            Ang_move->y = -Ang_move->y;
            Ang_move->z = -Ang_move->z;
            Pos_move->x = -Pos_move->x;
        }
    }
    if (w->Mot_attr & 0x400) {
        int cnt = w->Hokan_cnt;

        if (cnt - 1 > 0) {
            int mx = w->Hokan_frame + 1;
            f32 t = (f32) (mx - cnt) / (f32) w->Hokan_frame;
            f32 u = 1.0f - t;
            Pos_move->x = w->Pos_move_old.x * u + Pos_move->x * t;
            Pos_move->z = w->Pos_move_old.z * u + Pos_move->z * t;
        }
    }
    if (!(flg & 8)) {
        w->Pos_move_old = *Pos_move;
        w->Ang = b;
        w->Pos = a;
    }
}

// Applies a root delta to the model: position rotated by the model matrix, rotation added.
void MotionAddSpeed(cModel* pEm, MotionWorkSub* w, Vec* Pos_move, Vec* Ang_move)
{
    Vec t;

    PSMTXMultVecSR(pEm->mat, Pos_move, &t);
    PSVECAdd(&pEm->pos, &t, &pEm->pos);
    PSVECAdd(&pEm->ang, Ang_move, &pEm->ang);
}

// Root position/rotation keys at the previous sequence frame (Seq_old), without touching the state.
void MotionGetPosition(cModel* pEm, Vec* pPos, Vec* pAng)
{
    MotionWork* w = MOTION(pEm);
    HermitePrm prm;
    HermitePrm* pp;
    int flip;

    pPos->x = pPos->y = pPos->z = 0.0f;
    pAng->x = pAng->y = pAng->z = 0.0f;
    w->Mot_frame = SEQ_FRAME(w->Seq_old.frame);
    pp = &prm;
    pp->flags = 0;
    asm("" : : "r"(pp));  // COMPILER-DIFF: pp must outrank w for r31
    if (w->Mot_attr & 2) {
        if (!(w->Mot_attr & 0x1000)) {
            pp->flags = 2;
        }
    } else {
        if (w->Mot_attr & 0x1000) {
            pp->flags = 2;
        } else {
            pp->flags = 0;
        }
    }
    flip = 0;
    if (w->Mot_flag & 0x08000000) {
        flip = 1;
    }
    if (w->Null_pos != 0xFFFF) {
        pp->frame = w->Mot_frame;
        pp->maxFrame = w->Mot_frame_max;
        pp->key = (u8*) w->pHermite_data[w->Null_pos];
        pp->type = w->pJoint_kind[w->Null_pos] >> 12;
        HermiteInterpolation(pp, pPos, MOT_HIST(w, flip, 1));
    }
    if (w->Null_rot != 0xFFFF) {
        pp->frame = w->Mot_frame;
        pp->maxFrame = w->Mot_frame_max;
        pp->key = (u8*) w->pHermite_data[w->Null_rot];
        pp->type = w->pJoint_kind[w->Null_rot] >> 12;
        HermiteInterpolation(pp, pAng, MOT_HIST(w, flip, 0));
    }
}

// Advances the sequence frame by Seq_speed * pG->Speed per frame (unless paused): forward or
// reverse (Mot_attr bit 1), looping (bit 2: Mot_state 1/2) or clamping at the end (Mot_state
// 4/8); then resolves the motion frame (10.6 fixed) from the sequence table with interpolation
// between table entries, or linearly. Returns Mot_state.
u16 MotionSequenceCtrl(MotionWorkSub* w)
{
    f32 f;

    w->Seq_old2 = w->Seq_old;
    if (!(w->Mot_attr & 8)) {
        w->Mot_state = 0;
        if (w->Mot_attr & 2) {
            if (w->Mot_attr & 4) {
                if (w->Seq_frame <= 0.0f) {
                    w->Mot_state = 2;
                    f = w->Seq_frame + (f32) (int) w->Seq_frame_num;
                } else {
                    f = w->Seq_frame - w->Seq_speed * pG->Speed;
                }
                w->Seq_frame = f;
            } else {
                if (w->Seq_frame <= 0.0f) {
                    w->Mot_state = 8;
                    w->Seq_frame = 0.0f;
                } else {
                    f = w->Seq_frame - w->Seq_speed * pG->Speed;
                    w->Seq_frame = f;
                }
            }
        } else {
            w->Seq_frame = w->Seq_frame + w->Seq_speed * pG->Speed;
            if (w->Mot_attr & 4) {
                u16 max = w->Seq_frame_num;

                if (w->Seq_frame >= (f32) max) {
                    w->Mot_state = 1;
                    w->Seq_frame = w->Seq_frame - (f32) (int) max;
                }
            } else {
                u16 max = w->Seq_frame_num;

                if (w->Seq_frame >= (f32) max) {
                    w->Mot_state = 4;
                    w->Seq_frame = (f32) (max - 1);
                }
            }
        }
        w->Seq_old = w->Seq;
        if (w->pSeq_top == 0) {
            w->Seq.frame = (u16) (w->Seq_frame * 64.0f);
        } else {
            // One `sf` variable carries seqFrame, the fraction (`sf -= (f32)(int)fi`) and the
            // else-arm product (`sf *=`): 9 refs rank it above mf and the 0x43300000 double.
            f32 sf = w->Seq_frame;
            u16 fi = (u16) sf;

            w->Seq = w->pSeq_top[fi];
            if ((f32) fi != sf) {
                u16 nx = (u16) (sf + 1.0f);

                sf -= (f32) (int) fi;
                if (nx >= w->Seq_frame_num) {
                    f32 mf = w->Mot_frame_max * 64.0f;

                    if (mf == (f32) (int) w->pSeq_top[fi].frame) {
                        w->Seq.frame = (u16) (sf * 64.0f);
                    } else if (w->pSeq_top[0].frame == 0) {
                        w->Seq.frame = w->pSeq_top[fi].frame + (u16) (sf * (mf - (f32) (int) w->pSeq_top[fi].frame));
                    }
                } else {
                    sf *= (f32) (w->pSeq_top[nx].frame - w->pSeq_top[fi].frame);
                    w->Seq.frame = w->pSeq_top[fi].frame + (u16) sf;
                }
            }
            if ((f32) (int) w->Seq.frame > w->Mot_frame_max * 64.0f) {
                pLog->err(0, 0, "MotSeqCtrl(@0x%08x): %.2f Invalid Seq. Frame", w, (f32) (int) w->Seq.frame * 0.015625f);
            }
        }
    } else {
        w->Seq_old = w->Seq;
        w->Seq.Se = 0;
        w->Mot_state &= 0xFFF0;
    }
    return w->Mot_state;
}

// Frame count word of an FCV (camera curve) block.
u16 FcvGetMaxFrame(u16* pData)
{
    return pData[0];
}

// Last frame of the motion (-1 without motion).
f32 MotionGetMaxFrame(MotionWorkSub* w)
{
    if (w->pMot == 0) {
        return -1.0f;
    }
    return w->Mot_frame_max;
}

// Current motion frame (-1 without motion).
f32 MotionGetCurrentFrame(MotionWorkSub* w)
{
    if (w->pMot == 0) {
        return -1.0f;
    }
    return w->Mot_frame;
}

// 1 when the motion passed `frame` since the previous update (handles loops); 0 on the first
// frame after a set (Mot_flag 0x04000000). Used to trigger footsteps/attacks at key frames.
int MotionCheckCrossFrame(MotionWorkSub* w, f32 frame)
{
    f32 cur;
    f32 prev;

    if (w->pMot == 0) {
        return 0;
    }
    if (w->Mot_flag & 0x04000000) {
        return 0;
    }
    cur = w->Mot_frame;
    prev = w->Mot_frame_old;
    if (frame == 0.0f && cur == 0.0f && prev == 0.0f) {
        return 1;
    }
    if (cur >= prev) {
        if (frame <= cur && frame > prev) {
            return 1;
        }
        return 0;
    }
    if (frame > prev || frame <= cur) {
        return 1;
    }
    return 0;
}

// Mot_state of the model's motion, -1 when none is set.
int MotionGetState(cModel* m)
{
    MotionWork* w = MOTION(m);

    if (w->pMot == 0) {
        return -1;
    }
    return w->Mot_state;
}

// Evaluates one joint's three axis key streams at prm->frame: finds the key pair around the frame
// starting from the per-axis history index (hist, updated unless flags bit 3), wraps for looping
// motions (flags 4), holds the last key past the end, and Hermite-interpolates value/tangent pairs
// decoded by Fcc_get_data_tbl[prm->type]. Returns 1 when a history index was invalid.
int HermiteInterpolation(HermitePrm* prm, Vec* out, u16* hist)
{
    static FccGetData Fcc_get_data_tbl[16] = {
        Fcc_get_data_000, Fcc_get_data_001, Fcc_get_data_002, dummy,
        Fcc_get_data_010, Fcc_get_data_011, Fcc_get_data_012, dummy,
        Fcc_get_data_020, Fcc_get_data_021, Fcc_get_data_022, dummy,
        dummy,            dummy,            dummy,            Fcc_get_data_033,
    };
    f32 r = 0.0f;
    f32 frame = prm->frame;
    u8* p = prm->key;
    f32* o = (f32*) out;
    u16* hp = hist - 1;
    f32 f0 = r;
    f32 f1 = r;
    int ret = 0;
    int axis = 0;
    u16* frames;
    u8* data;
    f32 val[2];
    f32 tan[2];
    int n;
    int last = 0;
    int idx;
    int cnt;
    int found;

    for (; axis <= 2; axis++) {
        n = *(u16*) p;
        frames = (u16*) (p + 2);
        data = p + n * 2 + 2;
        hp++;
        p = data + Fcc_next_axis_addr(prm->type, n);
        cnt = n;
        found = 0;
        if (prm->maxFrame <= frame) {
            if ((prm->flags & 6) == 4) {
                frame -= prm->maxFrame;
                if (!(prm->flags & 8)) {
                    *hp = 0;
                }
            } else {
                Fcc_get_data_tbl[prm->type](data, n - 1, 0, val, tan);
                cnt = 0;
                found = 1;
                r = val[0];
            }
        }
        if (!(prm->flags & 8)) {
            idx = *hp;
        } else {
            idx = 0;
        }
        {
            // The target keeps `n - 1` in a local (r0) and copies it to `last` BEFORE the compare
            // (gcse's pre_insert_copies shape); a plain copy is coalesced by regmove. The r0 pin plus
            // the codeless use in the error arm keep m live past the compare, so combine cannot fold
            // the copy and regmove's forward scan stops at the branch (COMPILER-DIFF: gcse copy kept).
            register int m asm("r0") = n - 1;
            last = m;
            asm("" : : "r"(last));                    // COMPILER-DIFF: last must outrank n for r30
            if (idx > m) {
                asm("" : : "r"(m));                   // COMPILER-DIFF: keep-alive for the r0 temp
                pLog->err(0, 0, "H.I.(): axis=%d, hist=%d nFrm=%d, Invalid key history.", axis, idx, n);
                idx = 0;
                ret = 1;
            }
        }
        if (cnt != 0) {
            asm("" : "+r"(idx));  // COMPILER-DIFF: the table lis is issued before the fp init
            u16* fp = (u16*) (idx * 2 + (u32) frames);

            do {
                f0 = (f32) *fp;
                if (f0 == frame) {
                    Fcc_get_data_tbl[prm->type](data, idx, 0, val, tan);
                    r = val[0];
                    if (!(prm->flags & 8)) {
                        *hp = idx;
                    }
                    found = 1;
                    break;
                }
                if (f0 < frame) {
                    int nx = idx + 1;

                    if (nx > last) {
                        nx = 0;
                    }
                    f1 = (f32) frames[nx];
                    if (frame < f1) {
                        Fcc_get_data_tbl[prm->type](data, idx, nx, val, tan);
                        if (!(prm->flags & 8)) {
                            *hp = idx;
                        }
                        break;
                    }
                }
                if ((prm->flags & 1) || f0 > frame) {
                    fp--;
                    idx--;
                    if (idx < 0) {
                        fp = (u16*) (last * 2 + (u32) frames);
                        idx = last;
                    }
                } else {
                    idx++;
                    fp++;
                    if (idx > last) {
                        fp = frames;
                        idx = 0;
                    }
                }
            } while (--cnt);
        }
        if (!found) {
            r = hermite(val, tan, (frame - f0) / (f1 - f0));
        }
        o[axis] = r;
    }
    return ret;
}

// Byte-wise big-endian reads of the key data (the streams are unaligned).
typedef union {
    f32 f;
    struct {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 b3;
    } b;
} FccF32;

typedef union {
    s16 s;
    struct {
        u8 hi;
        u8 lo;
    } b;
} FccS16;

#define FCC_F32(dst, i)      \
    cf.b.b0 = d[(i)];        \
    cf.b.b1 = d[(i) + 1];    \
    cf.b.b2 = d[(i) + 2];    \
    cf.b.b3 = d[(i) + 3];    \
    (dst) = cf.f;
#define FCC_S16(dst, i)      \
    cs.b.hi = d[(i)];        \
    cs.b.lo = d[(i) + 1];    \
    (dst) = (f32) cs.s * 0.0001f;
#define FCC_S8(dst, i) (dst) = (f32) (s8) d[(i)] * 0.0001f;

// Key layout 0: f32 value, f32 in-tangent, f32 out-tangent (12 bytes/key); reads keys i0, i1.
void Fcc_get_data_000(u8* d, int i, int j, f32* x, f32* t)
{
    FccF32 cf;

    FCC_F32(x[0], i * 12);
    FCC_F32(x[1], j * 12);
    FCC_F32(t[0], i * 12 + 8);
    FCC_F32(t[1], j * 12 + 4);
}

// Key layout 1: f32 value, s16 tangents (8 bytes/key).
void Fcc_get_data_001(u8* d, int i, int j, f32* x, f32* t)
{
    FccF32 cf;
    FccS16 cs;

    FCC_F32(x[0], i * 8);
    FCC_F32(x[1], j * 8);
    FCC_S16(t[0], i * 8 + 6);
    FCC_S16(t[1], j * 8 + 4);
}

// Key layout 2: f32 value, s8 tangents (6 bytes/key).
void Fcc_get_data_002(u8* d, int i, int j, f32* x, f32* t)
{
    FccF32 cf;

    FCC_F32(x[0], i * 6);
    FCC_F32(x[1], j * 6);
    FCC_S8(t[0], i * 6 + 5);
    FCC_S8(t[1], j * 6 + 4);
}

// Key layout 4: s16 value, f32 tangents (10 bytes/key).
void Fcc_get_data_010(u8* d, int i, int j, f32* x, f32* t)
{
    FccF32 cf;
    FccS16 cs;

    FCC_S16(x[0], i * 10);
    FCC_S16(x[1], j * 10);
    FCC_F32(t[0], i * 10 + 6);
    FCC_F32(t[1], j * 10 + 2);
}

// Key layout 5: s16 value, s16 tangents (6 bytes/key). s16 and s8 fields are in 1/10000 units (FCC_S16 / FCC_S8).
void Fcc_get_data_011(u8* d, int i, int j, f32* x, f32* t)
{
    static int flag = 0;
    FccS16 cs;

    if (flag) {
        x[0] = (f32) *(s16*) &d[i * 6] * 0.0001f;
        x[1] = (f32) *(s16*) &d[j * 6] * 0.0001f;
        t[0] = (f32) *(s16*) &d[i * 6 + 4] * 0.0001f;
        t[1] = (f32) *(s16*) &d[j * 6 + 2] * 0.0001f;
    } else {
        FCC_S16(x[0], i * 6);
        FCC_S16(x[1], j * 6);
        FCC_S16(t[0], i * 6 + 4);
        FCC_S16(t[1], j * 6 + 2);
    }
}

// Key layout 6: s16 value, s8 tangents (4 bytes/key).
void Fcc_get_data_012(u8* d, int i, int j, f32* x, f32* t)
{
    FccS16 cs;

    FCC_S16(x[0], i * 4);
    FCC_S16(x[1], j * 4);
    FCC_S8(t[0], i * 4 + 3);
    FCC_S8(t[1], j * 4 + 2);
}

// Key layout 8: s8 value, f32 tangents (9 bytes/key).
void Fcc_get_data_020(u8* d, int i, int j, f32* x, f32* t)
{
    FccF32 cf;

    FCC_S8(x[0], i * 9);
    FCC_S8(x[1], j * 9);
    FCC_F32(t[0], i * 9 + 5);
    FCC_F32(t[1], j * 9 + 1);
}

// Key layout 9: s8 value, s16 tangents (5 bytes/key).
void Fcc_get_data_021(u8* d, int i, int j, f32* x, f32* t)
{
    FccS16 cs;

    FCC_S8(x[0], i * 5);
    FCC_S8(x[1], j * 5);
    FCC_S16(t[0], i * 5 + 3);
    FCC_S16(t[1], j * 5 + 1);
}

// Key layout 10: s8 value, s8 tangents (3 bytes/key).
void Fcc_get_data_022(u8* d, int i, int j, f32* x, f32* t)
{
    FCC_S8(x[0], i * 3);
    FCC_S8(x[1], j * 3);
    FCC_S8(t[0], i * 3 + 2);
    FCC_S8(t[1], j * 3 + 1);
}

// Key layout 15: f32 value only, no tangents (4 bytes/key; stepped/linear data).
void Fcc_get_data_033(u8* d, int i, int j, f32* x, f32* t)
{
    FccF32 cf;

    FCC_F32(x[0], i * 4);
    FCC_F32(x[1], j * 4);
}

// Unused key layouts.
void dummy(u8* d, int i, int j, f32* x, f32* t)
{
}

// Byte size of an axis stream of n keys for key layout `type` (offset to the next axis).
int Fcc_next_axis_addr(int fmt, int n)
{
    switch (fmt) {
    case 0:
        return n * 12;
    case 1:
        return n * 8;
    case 2:
        return n * 6;
    case 3:
        return -1;
    case 4:
        return n * 10;
    case 5:
        return n * 6;
    case 6:
        return n * 4;
    case 7:
        return -1;
    case 8:
        return n * 9;
    case 9:
        return n * 5;
    case 10:
        return n * 3;
    case 11:
        return -1;
    case 12:
        return -1;
    case 13:
        return -1;
    case 14:
        return -1;
    case 15:
        return n * 4;
    }
    return -1;
}

// Debug speed display: dead-stripped by the linker, only the strings, constant pools and statics
// remain. The three zeroed 4-byte statics survive as one unnamed 12-byte .sdata object; this stand-in
// keeps the split label as its name (and the section forced) so strip_unused leaves it in place.
static Vec lbl_80314C44 __attribute__((section(".sdata"))) = { 0.0f, 0.0f, 0.0f };

// Debug (dead-stripped): header line of the motion speed display.
// The two label pointers at .rodata+0x218 (relocated words) are a function-local static table
// declared AFTER the first eprintf: its strings and the table are assembled when the declaration
// is reached, i.e. after "MOTION SPEED ---" and before the "%s" of the following call.
void MotionSpeedDispHeader(int x, int y, int who)
{
    eprintf(x, y, 0, 0, "MOTION SPEED ---");
    {
        static const char* const who_str[2] = { "GLOBAL: ", "PLAYER: " };
        eprintf(x, y + 10, 0, 0, "%s", who_str[who]);
    }
}

// Debug (dead-stripped): prints the model's motion frame/speed values.
void MotionSpeedDisp(cModel* m, int x, int y)
{
    MotionWork* w = MOTION(m);

    eprintf(x, y, 0, 0, "%s", (char*) w->pMot);
    lbl_80314C44.x = lbl_80314C44.x * 0.01f + w->Pos_move_old.x * 10.0f * 0.5f;
    eprintf(x, y + 10, 0, 0, "%.2f", (f32) x * lbl_80314C44.x);
    if (lbl_80314C44.y == 0.0f) {
        lbl_80314C44.z = 2.0f;
    }
}
