// game/model: the model class hierarchy (D:/Bio4/Prog/model.cpp). cModel (a cCoord with parts) owns
// a chain of cModelInfo (one per .bin model data + TPL: the body, then added parts models) and a
// linked list of cParts (one per joint, from cPartsMgr), and carries the MotionWork, collision
// info, light info and draw parameters used by every character/object. Also: pointer relocation
// of model/TPL files (calcModelAddr / calcTplAddr and their inverses), bounding boxes, the parts
// and model-info managers (PartsMgr, ModInfoMgr), and the debug skeleton display.
#include "atari.h"
#include "model.h"
#include "motion.h"
#include "global.h"
#include "db_log.h"
#include "va_ppc.h"
#include "main_mem.h"
#include "dbmodule.h"
#include "eprintf.h"
#include "scheduler.h"
#include "math_sub.h"
#include "tpl.h"
#include "ref_access.h"
#include <dolphin/os.h>

// Model / parts / model info (cModel, cParts, cModelInfo) and their pools (PartsMgr, ModInfoMgr).

extern "C" {
void calcModelAddr(cModelData* data);
void calcModelOffset(cModelData* data);
void calcTplOffset(TEXPalette* tpl);
void getBoundingBox(cModelData* data, ModelBound* bound);
void drawBoundingBox(Mtx m, ModelBound* bound);
int GetModelInfoNum(cModelInfo* info);
}
cModelInfo* GetModelInfoAddr(cModelInfo* info, int no);

cModInfoMgr* cModel::mm = &ModInfoMgr;
cPartsMgr* cModel::pm = &PartsMgr;



// Clears a model's light-area state.
// The 0.0 pool load of the light area sinks below the three word stores: an inlined helper
// (integrate.c drops RTX_UNCHANGING_P from the pool MEM).
inline cModelState::cModelState()
{
    m_Flag = FLG_NONE;
    m_LightFlag = LFLG_NONE;
    m_LightNo = 0;
    m_LightPow = 0.0f;
}


// Empty model: collision/light-area info constructed, no parts, no model info, motion cleared,
// alpha_omit 0xFF.
cModel::cModel()
{
    alpha_omit = 0xFF;
    speed.x = 0.0f;
    speed.y = 0.0f;
    speed.z = 0.0f;
    pos_old.x = 0.0f;
    pos_old.y = 0.0f;
    pos_old.z = 0.0f;
    Wall_norm.x = 0.0f;
    Wall_norm.y = 0.0f;
    Wall_norm.z = 0.0f;
    pList = 0;
    r_no_0 = 0;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
    id = 0;
    type = 0;
    nParts = 0;
    pFloor_norm = 0;
    TevScaleGroup = 0;
    kindid = 0;
    ot_type = 0;
    pCldShMd = 0;
    Shd_color = 0;
    CullMode = 0;
    Shader_type = 0;
    Refract_pow = 0;
    Fix_parts = 0;
    Fix_pos.x = 0.0f;
    invisible_trg = 0;
    invisible_old = 0;
    invisible_mode = 0;
    invisible_busy = 0;
    invisible_timer = 0;
    pModelInfo = 0;
    pShadowModelInfo = 0;
    Fix_pos.y = 0.0f;
    Fix_pos.z = 0.0f;
    invisible_factor = 0.0f;
    memclr_asm(&Motion, 0xD0);
    Motion.blend = 0;
    Motion.flip = 0;
    inscreen_pos = 0;
    pPath = 0;
    pTexChg = 0;
}

// Loads a model: relocates bin/tpl, creates the first cModelInfo, builds the parts list from the
// model's joints (initJoint), sets visible/alive flags (invisible_factor 1, or 0 when the
// System_flg 0x800000 fade-in mode), enemies (kindid 0) get be_flag 0x10. Returns the info (0 on failure).
int cModel::modelInit(void* bin, void* tpl)
{
    cModelInfo* info;

    if (bin == NULL) {
        pLog->err(0, 0, "modelInit() : bin_addr == NULL.");
        return 0;
    }
    if (tpl == NULL) {
        pLog->err(0, 0, "modelInit() : tex_addr == NULL.");
        return 0;
    }
    calcModelAddr((cModelData*) bin);
    calcTplAddr((TEXPalette*) tpl);
    if (pModelInfo != NULL) {
        releaseModelInfo();
    }
    info = mm->create(bin, tpl);
    if (info == NULL) {
        pLog->err(0, 0, "modelInit() cModelInfo alloc failed.");
        return 0;
    }
    addModel(info);
    if (initJoint(bin) != 1) {
        pLog->err(0, 0, "ModelInit()  Parts allocate was failed.");
        releaseModelInfo();
        return 0;
    }
    if (SysFlagChk(pG, SYS_INVISIBLE)) {
        invisible_factor = 0.0f;
    } else {
        invisible_factor = 1.0f;
    }
    be_flag |= 6;
    invisible_factor2 = 1.0f;
    U8Set(TevScaleGroup, 0);
    CullMode = 0;
    if (kindid == 0) {
        be_flag |= 0x10;
    }
    pShadowModelInfo = 0;
    Motion.Seq_speed = 1.0f;
    pCldShMd = 0;
    Motion.pAttachCam = 0;
    return (int) info;
}

// Creates nParts cParts for the model's joints, links parents, sets rest offsets and the
// blend/flip tables. 0 when the parts pool is exhausted.
int cModel::initJoint(void* bin)
{
    releaseJoint();
    nParts = ((cModelData*) bin)->nParts;
    if (nParts == 0) {
        return 1;
    }
    if (makePartsList(0) == 0) {
        nParts = 0;
        pList = 0;
        return 0;
    }
    setPartsParent();
    setPartsOffset(bin);
    setJointInfo(bin);
    return 1;
}

// Frees the parts list.
void cModel::releaseJoint()
{
    if (pList != NULL) {
        releasePartsList(0);
    }
}

// Puts every parts at its rest position (the joint centres from the model data), computes the rest
// matrices at the origin (lt_inv_mat = inverse rest translation, used by skinning) and then the
// world matrices at the model position; initialises world_old.
void cModel::setPartsOffset(void* bin)
{
    cParts* p = pList;
    ModelDataHead* rec;
    Vec pos;
    u32 i;

    calcModelAddr((cModelData*) bin);
    rec = ((cModelData*) bin)->pHead;
    for (i = 0; i < nParts; i++) {
        p->pos.x = rec->center.x;
        p->pos.y = rec->center.y;
        p->pos.z = rec->center.z;
        rec++;
        p = p->pList;
    }
    pos.x = this->pos.x;
    pos.y = this->pos.y;
    pos.z = this->pos.z;
    PSMTXIdentity(mat);
    PSMTXTrans(mat, 0.0f, 0.0f, 0.0f);
    partsMatCalc();
    partsWorldCalc();
    this->pos.x = pos.x;
    this->pos.y = pos.y;
    this->pos.z = pos.z;
    for (p = pList; p; p = p->pList) {
        PSMTXIdentity(p->lt_inv_mat);
        p->lt_inv_mat[0][3] = -p->mat[0][3];
        p->lt_inv_mat[1][3] = -p->mat[1][3];
        p->lt_inv_mat[2][3] = -p->mat[2][3];
    }
    PSMTXTrans(mat, this->pos.x, this->pos.y, this->pos.z);
    partsMatCalc();
    partsWorldCalc();
    for (p = pList; p; p = p->pList) {
        p->world_old = p->world;
        p->world_old2 = p->world;
    }
}

// Links each parts to its parent joint from the model data (0xFF = the model itself).
void cModel::setPartsParent()
{
    cParts* p = pList;
    ModelDataHead* rec = pModelInfo->model_addr->pHead;
    u32 i;

    for (i = 0; i < nParts; i++) {
        if (rec->parentNo > 0xFE) {
            p->pParent = this;
        } else {
            p->pParent = getPartsPtr(rec->parentNo);
        }
        p = p->pList;
        rec++;
    }
}

// Rebuilds every parts' local matrix (l_mat, copied to mat) from its ang/pos/scale.
void cModel::partsMatCalc()
{
    cParts* p;

    for (p = pList; p; p = p->pList) {
        MtxPtr m = p->l_mat;
        RotMatrix(m, &p->ang);
        TransMatrix(m, &p->pos);
        ScaleMatrix(m, &p->scale);
        PSMTXCopy(m, p->mat);
    }
}

// Base move: nothing.
// Out-of-line inlines. Deferred-inline emission is definition order: ~cModelInfo, then the two
// managers (implicit dtor + in-class memAlloc/memFree/memClear, model.h), ~cParts, ~cModel, then
// these three and getPartsPtr, then the cManager::destroy instantiations and the ~cManager
// instantiations the synthesized manager dtors request in finish_file.
inline void cModel::move()
{
}

// be_flag 0x800: the model keeps moving while the game is suspended (events, pause).
inline void cModel::setNoSuspend(int onoff)
{
    if (onoff) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
}

// 1 when the model is drawn (be_flag bit 1).
inline int cModel::isTrans()
{
    int ret = 0;

    if ((be_flag & 2) && be_flag != 0) {
        ret = 1;
    }
    return ret;
}

// Parts `no` (-1: the model itself); NULL and a log when the chain is shorter. Defined here so
// that the callers below inline it while setPartsParent above calls it.
inline cParts* cModel::getPartsPtr(int idx)
{
    cParts* p = pList;
    int cnt;

    if (idx < 0) {
        return (cParts*) this;
    }
    cnt = idx;
    if (!VALID_PTR(p)) {
        return 0;
    }
    if (be_flag & 0x2000) {
        p += idx;
    } else if (idx--) {
        do {
            cParts* next = p->pList;
            if (!VALID_PTR(next)) {
                pLog->err(0, 0, "cModel::getPartsPtr() cParts NO ERROR %d", cnt);
                return 0;
            }
            p = next;
        } while (idx--);
    }
    return p;
}

// Motion blend: for every parts, mixes the current l_mat (rotation as a quaternion slerp, scale
// and translation linearly) with the parts' ang/pos/scale by `rate` (1 = only the new pose).
void cModel::matBlend(f32 rate)
{
    cParts* p;
    Mtx m;
    Quaternion q0;
    Quaternion q1;
    Quaternion q2;
    Vec pos;
    Vec len;
    Vec vx;
    Vec vy;
    Vec vz;
    Vec trans;

    for (p = pList; p; p = p->pList) {
        // COMPILER-DIFF: 3 (address-copy shape). The original computes `&p->worldMat` into a temp
        // and copies it (`addi r0,r31,60; mr r28,r0`). With the temp pinned to r0 and kept live
        // past the copy by the codeless anchor below (before the PSVECMag call), combine cannot
        // fold the copy into the addi (the r0 set is still needed) and regmove skips hard registers.
        register MtxPtr t asm("r0") = p->l_mat;
        MtxPtr wm = t;

        vx.x = p->l_mat[0][0];
        vx.y = p->l_mat[1][0];
        vx.z = p->l_mat[2][0];
        asm("" : "=m"(vx) : "r"(t)); // COMPILER-DIFF: 3 (keep-alive for the r0 temp)
        len.x = PSVECMag(&vx);
        vy.x = p->l_mat[0][1];
        vy.y = p->l_mat[1][1];
        vy.z = p->l_mat[2][1];
        len.y = PSVECMag(&vy);
        vz.x = p->l_mat[0][2];
        vz.y = p->l_mat[1][2];
        vz.z = p->l_mat[2][2];
        len.z = PSVECMag(&vz);
        trans.x = p->l_mat[0][3];
        trans.y = p->l_mat[1][3];
        trans.z = p->l_mat[2][3];
        if (len.x != 0.0f && len.y != 0.0f && len.z != 0.0f) {
            if (len.x != 1.0f) {
                PSVECScale(&vx, &vx, 1.0f / len.x);
            }
            if (len.y != 1.0f) {
                PSVECScale(&vy, &vy, 1.0f / len.y);
            }
            if (len.z != 1.0f) {
                PSVECScale(&vz, &vz, 1.0f / len.z);
            }
            p->l_mat[0][0] = vx.x;
            wm[1][0] = vx.y;
            wm[2][0] = vx.z;
            wm[0][1] = vy.x;
            wm[1][1] = vy.y;
            wm[2][1] = vy.z;
            wm[0][2] = vz.x;
            wm[1][2] = vz.y;
            wm[2][2] = vz.z;
            wm[0][3] = trans.x;
            wm[1][3] = trans.y;
            wm[2][3] = trans.z;
        }
        {
            f32 inv = 1.0f - rate;
            PSVECScale(&len, &len, inv);
            PSVECScale(&p->scale, &p->scale, rate);
            PSVECAdd(&p->scale, &len, &p->scale);
            pos.x = p->pos.x * rate + p->l_mat[0][3] * inv;
            pos.y = p->pos.y * rate + p->l_mat[1][3] * inv;
            pos.z = p->pos.z * rate + p->l_mat[2][3] * inv;
            p->pos = pos;
            PSMTXIdentity(m);
            RotMatrix(m, &p->ang);
            C_QUATMtx(&q0, m);
            C_QUATMtx(&q1, wm);
            C_QUATSlerp(&q0, &q1, &q2, inv);
            PSMTXQuat(wm, &q2);
            TransMatrix(wm, &p->pos);
            ScaleMatrix(wm, &p->scale);
        }
    }
}

// Places the model at pos/rot, clears its motion and rebuilds the matrices (player after events).
void cModel::zeroPartsPosInit(Vec* vPos, Vec* vAng)
{
    setPos(vPos);
    setAng(vAng);
    MotionClear(this, 0);
    matUpdate();
}

// Computes every parts' world matrix from its parent (parent mat * l_mat, with non-uniform parent
// scale removed and re-applied), applies pending addRot corrections (flag 0x40000000), stores the
// world position and the accumulated r_scale; parts with motParts flag 2 (IK-fixed) are skipped.
void cModel::partsWorldCalc()
{
    cParts* p;
    Vec tmp;
    Vec sc;
    Mtx m1;
    Mtx m2;

    r_scale = scale;
    p = pList;
    if (!VALID_PTR(p)) {
        pLog->err(2, 0, "partsWorldCalc() MODEL HAS NO PARTS");
        return;
    }
    if (!VALID_PTR(p->pParent)) {
        pLog->err(2, 0, "partsWorldCalc() PARENT ADDR ERR %08x", p->pParent);
        return;
    }
    for (; p; p = p->pList) {
        cCoord* parent = p->pParent;
        MtxPtr m;

        if (p->motParts.flags & 2) {
            continue;
        }
        tmp.x = p->l_mat[0][3];
        tmp.y = p->l_mat[1][3];
        tmp.z = p->l_mat[2][3];
        m = parent->mat;
        PSMTXMultVec(m, &tmp, &p->world);
        if (parent->r_scale.x != parent->r_scale.y || parent->r_scale.y != parent->r_scale.z) {
            sc.x = (parent->r_scale.x != 0.0f) ? 1.0f / parent->r_scale.x : 0.0f;
            sc.y = (parent->r_scale.y != 0.0f) ? 1.0f / parent->r_scale.y : 0.0f;
            sc.z = (parent->r_scale.z != 0.0f) ? 1.0f / parent->r_scale.z : 0.0f;
            PSMTXScale(m1, sc.x, sc.y, sc.z);
            PSMTXConcat(parent->mat, m1, m1);
            PSMTXConcat(m1, p->l_mat, p->mat);
            PSMTXScale(m1, parent->r_scale.x, parent->r_scale.y, parent->r_scale.z);
            PSMTXConcat(p->mat, m1, p->mat);
        } else {
            PSMTXConcat(m, p->l_mat, p->mat);
        }
        m = p->mat;
        if (p->motParts.flags & 0x40000000) {
            p->motParts.flags &= ~0x40000000;
            PSMTXRotRad(m2, 'x', p->inv_offset.x);
            PSMTXConcat(m, m2, m);
            PSMTXRotRad(m2, 'z', p->inv_offset.z);
            PSMTXConcat(m, m2, m);
            PSMTXRotRad(m2, 'y', p->inv_offset.y);
            PSMTXConcat(m2, m, m);
        }
        TransMatrix(m, &p->world);
        p->r_scale.x = parent->r_scale.x * p->scale.x;
        p->r_scale.y = parent->r_scale.y * p->scale.y;
        p->r_scale.z = parent->r_scale.z * p->scale.z;
    }
    Motion.Pos_world = pos;
}

// Attaches the root parts to another model (parent coordinate) with an offset and rotation.
void cModel::setParent(cCoord* pCoord, Vec* pos0, Vec* ang0)
{
    cParts* p = pList;

    p->pParent = pCoord;
    p->pos = *pos0;
    p->ang = *ang0;
}

// Attaches the root parts to parts partsNo of another model.
void cModel::setParent(cModel* pMod, int pno, Vec* pos0, Vec* ang0)
{
    setParent(pMod->getPartsPtr(pno), pos0, ang0);
}

// End of frame: pos_old and each parts' world_old/world_old2 history (used for motion trails and
// collision sweeps).
void cModel::updateOldPos()
{
    cParts* p;

    pos_old = pos;
    for (p = pList; p; p = p->pList) {
        p->world_old2 = p->world_old;
        p->world_old = p->world;
    }
}

// Moves the model and every parts by the delta without recomputing the pose; updates the light
// volume matrix and flags the collision as moved.
void cModel::setPos(Vec* newPos)
{
    Vec d;
    cParts* p;

    PSVECSubtract(newPos, &this->pos, &d);
    p = pList;
    if (PSVECMag(&d) != 0.0f) {
        PSVECAdd(&this->pos, &d, &this->pos);
        PSVECAdd(&pos_old, &d, &pos_old);
        for (; p; p = p->pList) {
            PSVECAdd(&p->world, &d, &p->world);
            p->world_old = p->world;
            p->mat[0][3] += d.x;
            p->mat[1][3] += d.y;
            p->mat[2][3] += d.z;
        }
        mat[0][3] = this->pos.x;
        mat[1][3] = this->pos.y;
        mat[2][3] = this->pos.z;
    }
    LightInfo.updateMatrix(this);
    atari.m_stat |= 1;
}

// Sets the rotation and rebuilds all matrices.
void cModel::setAng(Vec* ang)
{
    this->ang = *ang;
    matUpdate();
}

// Sets the scale and rebuilds all matrices.
void cModel::setSca(Vec* newSca)
{
    scale = *newSca;
    matUpdate();
}

// Debug: draws the joint tree (spheres, parent links, local axes) with parts numbers on screen.
void cModel::debugSkeletonDisp()
{
    cParts* p;
    Vec scr;
    Vec ax;
    Vec ay;
    Vec az;
    Vec wp;
    u32 i;
    // COMPILER-DIFF: candidate (gcse PRE pseudo numbering). Four dead pool constants (labels consumed
    // at expand, the loads deleted before gcse, the entries never output) move the 10.0/1.0 pool
    // labels from .LC24/.LC25 to .LC28/.LC29: the PRE'd highs are numbered by hash bucket
    // ((h(name) + 81) % 151 here) and .LC29 wraps to bucket 0 below .LC28's 150, so it is
    // allocated first and takes r22, the 10.0 high r21, like the original (see exception.cpp).
    f32 lc0 = 101.125f;
    f32 lc1 = 102.125f;
    f32 lc2 = 103.125f;
    f32 lc3 = 104.125f;

    p = pList;
    if (p == NULL) {
        return;
    }
    Draw_sphere(&pos, 15.0f, 0xFFFF, 1, 1);
    Draw_line3d(&p->world, &pos, 0xFFFFFFFF, 0);
    ax.x = 25.0f;
    ax.y = 0.0f;
    ax.z = 0.0f;
    ay.x = 0.0f;
    ay.y = 25.0f;
    ay.z = 0.0f;
    az.x = 0.0f;
    az.y = 0.0f;
    az.z = 25.0f;
    PSMTXMultVec(p->mat, &ax, &ax);
    PSMTXMultVec(p->mat, &ay, &ay);
    PSMTXMultVec(p->mat, &az, &az);
    Draw_line3d(&p->world, &ax, 0xFFFF0000, 0);
    Draw_line3d(&p->world, &ay, 0xFF00FF00, 0);
    Draw_line3d(&p->world, &az, 0xFF0000FF, 0);
    Draw_sphere(&p->world, 15.0f, 0xFFFF, 1, 1);
    wp = p->world;
    GetScreenPos(&wp, &scr);
    scr.x += 10.0f;
    scr.y += 10.0f;
    if (scr.z < 1.0f) {
        eprintf2(8, 0xE, (int) scr.x, (int) scr.y, 1, 0, "[%02d]", 0);
    }
    p = p->pList;
    for (i = 1; p; p = p->pList, i++) {
        if (p->pParent) {
            Draw_line3d(&p->world, &p->pParent->world, 0xFFFF, 0);
        }
        Draw_sphere(&p->world, 10.0f, 0xFF0000FF, 1, 1);
        ax.x = 25.0f;
        ax.y = 0.0f;
        ax.z = 0.0f;
        ay.x = 0.0f;
        ay.y = 25.0f;
        ay.z = 0.0f;
        az.x = 0.0f;
        az.y = 0.0f;
        az.z = 25.0f;
        PSMTXMultVec(p->mat, &ax, &ax);
        PSMTXMultVec(p->mat, &ay, &ay);
        PSMTXMultVec(p->mat, &az, &az);
        Draw_line3d(&p->world, &ax, 0xFFFF0000, 0);
        Draw_line3d(&p->world, &ay, 0xFF00FF00, 0);
        Draw_line3d(&p->world, &az, 0xFF0000FF, 0);
        wp = p->world;
        GetScreenPos(&wp, &scr);
        if (scr.z < 1.0f) {
            int dx;
            int dy;

            switch (i & 7) {
            default:
                dx = 0;
                dy = 0;
                break;
            case 1:
                dx = -30;
                dy = 12;
                break;
            case 2:
                dx = 10;
                dy = -12;
                break;
            case 3:
                dx = -30;
                dy = -12;
                break;
            case 4:
                dx = -40;
                dy = 0;
                break;
            case 5:
                dx = 0;
                dy = 24;
                break;
            case 6:
                dx = 0;
                dy = -24;
                break;
            case 7:
                dx = 50;
                dy = 0;
                break;
            }
            eprintf2(8, 0xE, (int) scr.x + dx, (int) scr.y + dy, 4, 0, "[%02d]", i);
        }
        if (pG->debug_mode == 7) {
            wp.x = 100.0f;
            wp.y = 0.0f;
            wp.z = 0.0f;
            PSMTXMultVec(p->mat, &wp, &wp);
            Draw_line3d(&p->world, &wp, 0xFFFF0000, 0);
            wp.x = 0.0f;
            wp.y = 100.0f;
            wp.z = 0.0f;
            PSMTXMultVec(p->mat, &wp, &wp);
            Draw_line3d(&p->world, &wp, 0xFF00FF00, 0);
            wp.x = 0.0f;
            wp.y = 0.0f;
            wp.z = 100.0f;
            PSMTXMultVec(p->mat, &wp, &wp);
            Draw_line3d(&p->world, &wp, 0xFF0000FF, 0);
        }
    }
}

// Appends a model info (extra parts model: weapon, head, clothes) to the model's chain.
void cModel::addModel(cModelInfo* pInfo)
{
    cModelInfo* p;

    if (pModelInfo == NULL) {
        pModelInfo = pInfo;
        return;
    }
    for (p = pModelInfo; p->pList; p = p->pList) {
    }
    p->pList = pInfo;
}

// Remembers parts `no`'s world position so partsFixAdjust can keep it planted (foot lock).
void cModel::partsFixMemory(int fix_parts)
{
    cParts* p;

    if (pList == NULL) {
        Fix_parts = 0;
        return;
    }
    p = getPartsPtr(fix_parts);
    Fix_pos = p->world;
    Fix_parts = fix_parts + 1;
}

// Moves the model in XZ so the remembered parts stays where it was, then recomputes the parts
// world positions; clears the lock.
void cModel::partsFixAdjust()
{
    cParts* p;
    Vec d;

    if (Fix_parts == 0) {
        return;
    }
    p = getPartsPtr(Fix_parts - 1);
    d.x = p->world.x - Fix_pos.x;
    d.z = p->world.z - Fix_pos.z;
    pos.x -= d.x;
    pos.z -= d.z;
    PartsWorldPosCalc(this);
    Fix_parts = 0;
}

// Kills the model: clears alive/visible flags and frees its parts and model infos.
void cModel::push()
{
    if (isAlive()) {
        be_flag &= ~0x22;
        releasePartsList(0);
        releaseModelInfo();
    }
}

// Debug: draws the bounding box of every model info in the chain.
void cModel::drawAllBoundingBox(cModelInfo* pModelInfo)
{
    for (; pModelInfo; pModelInfo = pModelInfo->pList) {
        drawBoundingBox(mat, &pModelInfo->bound);
    }
}


// New model info: white colour, identity matrix, visible, opaque.
cModelInfo::cModelInfo() : cUnit(1)
{
    static u32 col = 0xFFFFFFFF;

    colorWord = col;
    PSMTXIdentity(mat);
    be_flag |= 8;
    invisible_factor = 1.0f;
}

// Sets (and relocates) the texture palette.
void cModelInfo::setTplAddr(void* tpl)
{
    tpl_addr = tpl;
    calcTplAddr((TEXPalette*) tpl);
}

// Sets an additional texture palette (nAddTex textures appended to the model's texture indices).
void cModelInfo::addTplAddr(void* tpl)
{
    pAddTpl = (TEXPalette*) tpl;
    calcTplAddr((TEXPalette*) tpl);
    nAddTex = pAddTpl->numDescriptors;
}

// Installs a texture blend table (per material texture replacement, e.g. tex-render / damage
// textures) and marks it active (flagsDC bit 2).
void cModelInfo::setTexBlendTbl(void* pTbl)
{
    texBlendTbl = pTbl;
    flagsDC |= 4;
}

// Removes the texture blend table.
void cModelInfo::resetTexBlendTbl()
{
    texBlendTbl = 0;
    flagsDC &= ~4;
}

// Blend ratio (0..255) between the original and blend-table textures.
void cModelInfo::setBlendRatio(u16 ratio)
{
    blendRatio = ratio;
}

// Selects the TEV blend type used by the blend table.
void cModelInfo::setBlendType(u8 type)
{
    blendType = type;
}

// Sets the specular colour of every material of the model data.
void cModelInfo::setSpecular(u8 r, u8 g, u8 b)
{
    cModelData* d = model_addr;
    ModelPart* part = d->pParts;
    u32 n = d->displist_num;
    u32 i;

    for (i = 0; i < n; i++) {
        part->specR = r;
        part->specG = g;
        part->specB = b;
        {
            u8* next = (u8*) (part + 1);
            part = (ModelPart*) (next + part->size);
        }
    }
}

// Halts (sleep loop) on a model file with an unknown version.
// Never called (the original linker dropped the bodies): the "not bin data" wait loop
// cModInfoMgr::create inlines, and the two shadow model registrations.
static inline void notBinData()
{
    for (;;) {
        eprintf(0x64, 0x190, 0, 0, "not bin data");
        TaskSleep(1);
    }
}

static int lbl_80314C2C = 0;
// Dead like the two functions below; the linker dropped them but kept this table (the 0x10 zero
// bytes between the cCoord and cUnit vtable copies in .rodata).
static const s32 ShadowPtNum[4] = { 0, 0, 0, 0 };

// Leftover shadow model bookkeeping: only the "PtNum Over" check survives.
static void ShadowModelInit(int em, int sh)
{
    if (lbl_80314C2C + ShadowPtNum[em] > sh) {
        pLog->err(0, 0, "ShadowModelInit():PtNum Over (Em:%d/Sh:%d)", em, sh);
    }
}

// Leftover shadow model bookkeeping (see ShadowModelInit).
static void AddShadowModel(int em, int sh)
{
    if (lbl_80314C2C + ShadowPtNum[em] > sh) {
        pLog->err(0, 0, "AddShadowModel():PtNum over(Em:%d/Sh:%d)", em, sh);
    }
}

// Removes (and destroys) the model info whose data is `data` from the chain; 0 when absent.
int cModel::deleteModelData(cModelData* addr)
{
    cModelInfo* prev = NULL;
    cModelInfo* info = pModelInfo;

    if (info->model_addr == addr) {
        pModelInfo = info->pList;
        cModel::mm->destroy(info);
        return 1;
    }
    while (info) {
        if (info->model_addr == addr) {
            prev->pList = info->pList;
            cModel::mm->destroy(info);
            return 1;
        }
        prev = info;
        info = info->pList;
    }
    return 0;
}

// Removes and destroys a model info from the chain; 0 when absent.
int cModel::deleteModelInfo(cModelInfo* addr)
{
    cModelInfo* prev = NULL;
    cModelInfo* info = pModelInfo;

    if (info == addr) {
        pModelInfo = info->pList;
        cModel::mm->destroy(info);
        return 1;
    }
    while (info) {
        if (info == addr) {
            prev->pList = info->pList;
            cModel::mm->destroy(info);
            return 1;
        }
        prev = info;
        info = info->pList;
    }
    return 0;
}

// Replaces the model info holding `data` by newInfo in place (costume/damage model swaps).
int cModel::swapModelInfo(cModelData* addr, cModelInfo* pSwapInfo)
{
    cModelInfo* prev = NULL;
    cModelInfo* info = pModelInfo;

    if (info->model_addr == addr) {
        pModelInfo = info->pList;
        cModel::mm->destroy(info);
        addModel(pSwapInfo);
        return 1;
    }
    while (info) {
        if (info->model_addr == addr) {
            prev->pList = pSwapInfo;
            pSwapInfo->pList = info->pList;
            cModel::mm->destroy(info);
            return 1;
        }
        prev = info;
        info = info->pList;
    }
    return 0;
}

// After the owning archive moved in memory by ofs (be_flag 0x80000 models): shifts the data/tpl
// pointers of every model info and reconverts them to offsets for the next relocation.
void cModel::moveDataAddr(int ofsAddr)
{
    cModelInfo* info;

    if (be_flag & 0x80000) {
        return;
    }
    for (info = pModelInfo; info; info = info->pList) {
        if (ofsAddr != 0) {
            info->model_addr = (cModelData*) ((u8*) info->model_addr + ofsAddr);
            info->tpl_addr = (u8*) info->tpl_addr + ofsAddr;
        } else {
            calcModelOffset(info->model_addr);
            calcTplOffset((TEXPalette*) info->tpl_addr);
        }
    }
}

// Converts a model file's section offsets (colour, texture, joint heads, weights, parts, original
// vertices/normals, blend/flip tables) to pointers, once (pClr sign bit marks the state). Halts
// when the parts block is not 32-byte aligned.
// Relocates the file offsets of a model bin to pointers (once: pClr is a pointer afterwards).
void calcModelAddr(cModelData* d)
{
    u8* base = (u8*) d;

    if ((int) d->pClr < 0) {
        return;
    }
    d->pClr = base + (u32) d->pClr;
    d->pTex = base + (u32) d->pTex;
    d->pHead = (ModelDataHead*) (base + (u32) d->pHead);
    d->pWeight = base + (u32) d->pWeight;
    d->pParts = (ModelPart*) (base + (u32) d->pParts);
    d->vtxOrig = base + (u32) d->vtxOrig;
    d->nrmOrig = base + (u32) d->nrmOrig;
    if (d->version > 0x20030817) {
        if (d->blendTbl != 0) {
            d->blendTbl = (u32) base + d->blendTbl;
        }
        if (d->flipTbl != 0) {
            d->flipTbl = (u32) base + d->flipTbl;
        }
    }
    if ((u32) d->pParts & 0x1F) {
#line 1907 "D:/Bio4/Prog/model.cpp"
        HALT();
    }
}

// Inverse of calcModelAddr: pointers back to offsets (before the file is moved or saved).
void calcModelOffset(cModelData* d)
{
    u8* base = (u8*) d;

    if ((int) d->pClr >= 0) {
        return;
    }
    d->pClr = (void*) ((u8*) d->pClr - base);
    d->pTex = (void*) ((u8*) d->pTex - base);
    d->pHead = (ModelDataHead*) ((u8*) d->pHead - base);
    d->pWeight = (void*) ((u8*) d->pWeight - base);
    d->pParts = (ModelPart*) ((u8*) d->pParts - base);
    d->vtxOrig = (void*) ((u8*) d->vtxOrig - base);
    d->nrmOrig = (void*) ((u8*) d->nrmOrig - base);
    if (d->version > 0x20030817) {
        if (d->blendTbl != 0) {
            d->blendTbl -= (u32) base;
        }
        if (d->flipTbl != 0) {
            d->flipTbl -= (u32) base;
        }
    }
}

// The bin moved by `ofs` bytes (block.cpp compaction): shift its pointers.
void slideModelAddr(u32 addr, int ofs)
{
    cModelData* d = (cModelData*) addr;

    if ((int) d->pClr >= 0) {
        calcModelAddr(d);
    }
    d->pClr = (u8*) d->pClr + ofs;
    d->pTex = (u8*) d->pTex + ofs;
    d->pHead = (ModelDataHead*) ((u8*) d->pHead + ofs);
    d->pWeight = (u8*) d->pWeight + ofs;
    d->pParts = (ModelPart*) ((u8*) d->pParts + ofs);
    d->vtxOrig = (u8*) d->vtxOrig + ofs;
    d->nrmOrig = (u8*) d->nrmOrig + ofs;
    if (d->version > 0x20030817) {
        if (d->blendTbl != 0) {
            d->blendTbl += ofs;
        }
        if (d->flipTbl != 0) {
            d->flipTbl += ofs;
        }
    }
}

// Converts a TPL's descriptor/texture header/image offsets to pointers, once.
void calcTplAddr(TEXPalette* tpl)
{
    u32 i;

    if (tpl == NULL) {
        return;
    }
    if ((int) tpl->descriptorArray < 0) {
        return;
    }
    tpl->descriptorArray = (TEXDescriptor*) ((u32) tpl->descriptorArray + (u32) tpl);
    for (i = 0; i < tpl->numDescriptors; i++) {
        if (tpl->descriptorArray[i].textureHeader != NULL) {
            tpl->descriptorArray[i].textureHeader = (TEXHeader*) ((u32) tpl->descriptorArray[i].textureHeader + (u32) tpl);
            if (tpl->descriptorArray[i].textureHeader->unpacked == 0) {
                tpl->descriptorArray[i].textureHeader->data = (void*) ((u32) tpl->descriptorArray[i].textureHeader->data + (u32) tpl);
                tpl->descriptorArray[i].textureHeader->unpacked = 1;
            }
        }
    }
}

// Inverse of calcTplAddr.
void calcTplOffset(TEXPalette* tpl)
{
    u32 i;

    if ((int) tpl->descriptorArray >= 0) {
        return;
    }
    for (i = 0; i < tpl->numDescriptors; i++) {
        if (tpl->descriptorArray[i].textureHeader != NULL) {
            if (tpl->descriptorArray[i].textureHeader->unpacked != 0) {
                tpl->descriptorArray[i].textureHeader->data = (void*) ((u8*) tpl->descriptorArray[i].textureHeader->data - (u8*) tpl);
                tpl->descriptorArray[i].textureHeader->unpacked = 0;
            }
            tpl->descriptorArray[i].textureHeader = (TEXHeader*) ((u8*) tpl->descriptorArray[i].textureHeader - (u8*) tpl);
        }
    }
    tpl->descriptorArray = (TEXDescriptor*) ((u8*) tpl->descriptorArray - (u8*) tpl);
}

// Shifts a relocated TPL's pointers by ofs.
void slideTplAddr(void* p, int ofs)
{
    TEXPalette* tpl = (TEXPalette*) p;
    u32 i;

    if ((int) tpl->descriptorArray >= 0) {
        calcTplAddr(tpl);
    }
    tpl->descriptorArray = (TEXDescriptor*) ((u8*) tpl->descriptorArray + ofs);
    for (i = 0; i < tpl->numDescriptors; i++) {
        if (tpl->descriptorArray[i].textureHeader != NULL) {
            tpl->descriptorArray[i].textureHeader = (TEXHeader*) ((u8*) tpl->descriptorArray[i].textureHeader + ofs);
            tpl->descriptorArray[i].textureHeader->data = (u8*) tpl->descriptorArray[i].textureHeader->data + ofs;
        }
    }
}

// Destroys every model info of the chain.
void cModel::releaseModelInfo()
{
    cModelInfo* info = pModelInfo;

    while (info) {
        cModelInfo* dead = info;
        info = info->pList;
        mm->destroy(dead);
    }
    pModelInfo = 0;
}

// Allocates n (0 = nParts) cParts: a contiguous run when the pool has one (be_flag 0x2000, fast
// indexing), else one by one; links them as pList. 0 when the pool is full (list freed).
int cModel::makePartsList(int n)
{
    u32 num;
    cParts* p;
    u32 i;

    if (n == 0) {
        num = nParts;
    } else {
        num = n;
    }
    p = (cParts*) this;
    pList = pm->createSequential(num);
    if (pList != NULL) {
        be_flag |= 0x2000;
    } else {
        for (i = 0; i < num; i++) {
            cParts* np = pm->create();
            if (np == NULL) {
                releasePartsList(0);
                return 0;
            }
            p->pList = np;
            p = np;
        }
    }
    return 1;
}

// Binds the motion blend table and flip table of a version 0x20030818 model file to the MotionWork.
void cModel::setJointInfo(void* pHead)
{
    cModelData* d = (cModelData*) pHead;

    if (d->version == 0x20030818) {
        if (d->blendTbl != 0) {
            Motion.blendTbl = (u16*) d->blendTbl;
        } else {
            Motion.blendTbl = 0;
        }
        if (d->flipTbl != 0) {
            Motion.flip = (u16*) (d->flipTbl + 4);
        } else {
            Motion.flip = 0;
        }
    } else {
        Motion.blendTbl = 0;
        Motion.flip = 0;
    }
}

// Frees the parts from index `no` to the end (0 = all, clearing pList/nParts).
void cModel::releasePartsList(int idx)
{
    cParts* p;
    cParts* prev;

    p = (cParts*) getPartsPtr(idx);
    if (!VALID_PTR(p)) {
        if (idx != 0) {
            pLog->err(0, 0, "releasePartsList() idx INVALID. %d", idx);
        }
        return;
    }
    while (p) {
        cParts* dead = p;
        p = p->pList;
        pm->destroy(dead);
    }
    if (idx == 0) {
        pList = 0;
        nParts = 0;
        return;
    }
    prev = (cParts*) getPartsPtr(idx - 1);
    prev->pList = 0;
}

// Starts a motion on this model (MotionSetCore wrapper with reordered arguments).
void cModel::motionSet(void* mot, u8 hokan, u16 frame, u16 stat, void* seq)
{
    MotionSetCore(this, &Motion, mot, seq, hokan, stat, frame);
}

// Advances the motion one frame (MotionMove).
int cModel::motionMove()
{
    return MotionMove(this, 0);
}

// Pauses the motion.
void cModel::motionPause()
{
    MotionPause(this);
}

// Rebuilds the model matrix (cCoord), all parts matrices and the light volume matrix.
void cModel::matUpdate()
{
    cCoord::matUpdate();
    if (pList != NULL) {
        partsMatCalc();
        partsWorldCalc();
    }
    LightInfo.updateMatrix(this);
}

// Parts constructor: nothing beyond cModel.
cParts::cParts()
{
}

// Bounding box of the original vertices (s16 * 2^-shift, 8 bytes each): centre and half size.
void getBoundingBox(cModelData* d, ModelBound* pBox)
{
    f32 maxZ = -65536.0f;
    f32 maxY = -65536.0f;
    f32 maxX = -65536.0f;
    f32 minZ = 65536.0f;
    f32 minY = 65536.0f;
    f32 minX = 65536.0f;
    u32 n = d->nVtx;
    u8 shift = d->shift;
    s16* v = (s16*) d->vtxOrig;
    u32 i;

    if (n != 0) {
        f32 sc = (f32) (1 << shift);
        i = n;
        do {
            f32 x = (f32) v[0] / sc;
            f32 y = (f32) v[1] / sc;
            f32 z = (f32) v[2] / sc;
            if (maxX < x) {
                maxX = x;
            }
            if (maxY < y) {
                maxY = y;
            }
            if (maxZ < z) {
                maxZ = z;
            }
            if (minX > x) {
                minX = x;
            }
            if (minY > y) {
                minY = y;
            }
            if (minZ > z) {
                minZ = z;
            }
            v += 4;
        } while (--i != 0);
    }
    pBox->size.x = (maxX - minX) * 0.5f;
    pBox->size.y = (maxY - minY) * 0.5f;
    pBox->size.z = (maxZ - minZ) * 0.5f;
    pBox->center.x = maxX - pBox->size.x;
    pBox->center.y = maxY - pBox->size.y;
    pBox->center.z = maxZ - pBox->size.z;
}

// Manager of the cParts pool.
cPartsMgr::cPartsMgr() : cManager<cParts>(sizeof(cParts), 0)
{
    setName("cPartsMgr");
}

// Manager warnings to the log.
void cPartsMgr::log(const char* pStr, ...)
{
    va_list ap;

    va_start(ap, pStr);
    pLog->vwarn(6, 0, pStr, ap);
}

// Unit construction: placement-new of cParts.
int cPartsMgr::construct(cParts* p, u32 id)
{
    new (p) cParts;
    return 1;
}

// Work `no` of a parts manager, NULL when out of range.
static inline cParts* PartsMgrWork(cPartsMgr* m, u32 no)
{
    if (no >= m->nArray) {
        return 0;
    }
    return m->fastAt(no);
}

// Allocates n consecutive free parts slots (linked as pList) so the model can index them directly;
// 0 when no run of n is free.
cParts* cPartsMgr::createSequential(u32 n)
{
    u32 i;
    u32 j;
    u32 lim;

    // `lim` is recomputed in the loop test: the entry guard's `n + 1` and the hoisted loop copy
    // give the `addi r0; mr r7, r0` pair. A `lim = n + 1` before the loop folds them into one
    // register; `nArray - (n + 1)` is reassociated by fold to `(nArray - 1) - n`.
    for (i = 0; lim = n + 1, i < nArray - lim; i++) {
        int ok;

        if (!PartsMgrWork(this, i)->isEmpty()) {
            continue;
        }
        ok = 1;
        for (j = 0; j < n; j++) {
            if (!PartsMgrWork(this, i + j)->isEmpty()) {
                ok = 0;
            }
        }
        if (ok) {
            cParts* first;
            cParts* p;

            first = create(0, i);
            p = first;
            for (j = 1; j < n; j++) {
                cParts* np = create(0, i + j);
                p->pList = np;
                p = np;
            }
            return first;
        }
    }
    return 0;
}

cPartsMgr PartsMgr;

// Manager of the cModelInfo pool.
cModInfoMgr::cModInfoMgr() : cManager<cModelInfo>(sizeof(cModelInfo), 0)
{
    setName("cModInfoMgr");
}

// Manager warnings to the log.
void cModInfoMgr::log(const char* pStr, ...)
{
    va_list ap;

    va_start(ap, pStr);
    pLog->vwarn(6, 0, pStr, ap);
}

// Unit construction: placement-new of cModelInfo.
int cModInfoMgr::construct(cModelInfo* p, u32 id)
{
    new (p) cModelInfo;
    return 1;
}

// Creates a model info for a bin/tpl pair: relocates both, checks the file version (0x20010801 or
// 0x20030818, else halts), flags shape (morph) data (be_flag 2) and computes the bounding box.
cModelInfo* cModInfoMgr::create(void* bin, void* tpl)
{
    cModelInfo* info = cManager<cModelInfo>::create();

    if (info != NULL) {
        cModelData* d = (cModelData*) bin;

        calcModelAddr(d);
        calcTplAddr((TEXPalette*) tpl);
        info->tpl_addr = tpl;
        info->model_addr = d;
        if (d->version != 0x20010801 && d->version != 0x20030818) {
            notBinData();
        }
        if (d->shapeOfs != 0) {
            info->be_flag |= 2;
        }
        getBoundingBox(info->model_addr, &info->bound);
    }
    return info;
}

cModInfoMgr ModInfoMgr;

// Parts `no` by walking the pList chain from `parts`.
cParts* GetPartsAddr(cParts* pList, u32 idx)
{
    int cnt = idx;

    if (!VALID_PTR(pList)) {
        pLog->err(0, 0, "GetPartsAddr() PTR ERROR %08X", pList);
        return 0;
    }
    if (idx--) {
        do {
            cParts* next = pList->pList;
            if (!VALID_PTR(next)) {
                pLog->err(0, 0, "GetPartsAddr() cParts NO ERROR %d", cnt);
                break;
            }
            pList = next;
        } while (idx--);
    }
    return pList;
}

// Model info `no` of a chain.
cModelInfo* GetModelInfoAddr(cModelInfo* info, int no)
{
    int cnt = no;

    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "GetModelInfoAddr() PTR ERROR %08X", info);
        return 0;
    }
    if (no--) {
        do {
            cModelInfo* next = info->pList;
            if (!VALID_PTR(next)) {
                pLog->err(0, 0, "GetModelInfoAddr() cModelInfo NO ERROR %d", cnt);
                break;
            }
            info = next;
        } while (no--);
    }
    return info;
}

// Length of a model info chain (error above 100).
int GetModelInfoNum(cModelInfo* info)
{
    int n = 1;
    int i;

    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "GetModelInfoNum() PTR ERROR %08X", info);
        return 0;
    }
    for (i = 0; i < 100; i++) {
        cModelInfo* next = info->pList;
        if (!VALID_PTR(next)) {
            return n;
        }
        info = next;
        n++;
    }
    pLog->err(0, 0, "GetModelInfoNum() PTR NUM 100 ?");
    return 0;
}

// Excludes every model info of m from the reflection (mirror/water) render (be_flag 4).
void ModelInfoRefrectOffAll(cModel* pMod)
{
    int n;
    int i;

    if (pMod == NULL) {
        pLog->err(0, 0, "ModelInfoRefrectOffAll() : failed!!");
        return;
    }
    n = GetModelInfoNum(pMod->pModelInfo);
    for (i = 0; i < n; i++) {
        cModelInfo* info = GetModelInfoAddr(pMod->pModelInfo, i);
        if (info) {
            info->be_flag |= 4;
        }
    }
}

// Re-includes model info `no` in the reflection render.
void ModelInfoRefrectOn(cModel* pMod, int modelInfoNo)
{
    cModelInfo* info;

    if (pMod == NULL) {
        pLog->err(0, 0, "ModelInfoRefrectOn() : failed!!");
        return;
    }
    info = GetModelInfoAddr(pMod->pModelInfo, modelInfoNo);
    if (info) {
        info->be_flag &= ~4;
    }
}

// Shows/hides model info `no` (be_flag 8).
void ModelInfoSetTrans(cModel* pMod, int modelInfoNo, int flag)
{
    cModelInfo* info = GetModelInfoAddr(pMod->pModelInfo, modelInfoNo);

    if (info) {
        if (flag == 1) {
            info->be_flag |= 8;
        } else {
            info->be_flag &= ~8;
        }
    }
}

// Debug: draws a bounding box transformed by m as 12 lines.
void drawBoundingBox(Mtx m, ModelBound* pBox)
{
    static u8 ptbl[6][4] = {
        {0, 1, 3, 2}, {4, 5, 7, 6}, {0, 1, 5, 4}, {3, 2, 6, 7}, {1, 3, 7, 5}, {2, 0, 4, 6},
    };
    Vec v[8];
    Vec q[4];
    Vec* c;
    int i;
    u32 j;
    f32 sx = pBox->size.x;
    f32 sy = pBox->size.y;
    f32 sz = pBox->size.z;

    c = v;
    c->x = -sx; c->y = -sy; c->z = -sz;
    c++;
    c->x = sx; c->y = -sy; c->z = -sz;
    c++;
    c->x = -sx; c->y = -sy; c->z = sz;
    c++;
    c->x = sx; c->y = -sy; c->z = sz;
    c++;
    c->x = -sx; c->y = sy; c->z = -sz;
    c++;
    c->x = sx; c->y = sy; c->z = -sz;
    c++;
    c->x = -sx; c->y = sy; c->z = sz;
    c++;
    c->x = sx; c->y = sy; c->z = sz;
    // A counted loop: loop.c's giv init for `&v[j]` is emitted in the preheader after gcse's
    // `&q[k]` insertions (LUID order decides the sched2 slot of `mr r30,r24`) and the eliminated biv
    // compares the stepped pointer against `&v[7]` (`cmplw; ble`), which the do-while form with an
    // explicit end pointer gave as well but with the copy issued before the addis.
    for (j = 0; j < 8; j++) {
        PSVECAdd(&v[j], &pBox->center, &v[j]);
    }
    PSMTXMultVecArray(m, v, v, 8);
    for (i = 0; i < 6; i++) {
        q[0] = v[ptbl[i][0]];
        q[1] = v[ptbl[i][1]];
        q[2] = v[ptbl[i][2]];
        q[3] = v[ptbl[i][3]];
        Draw_line3d(&q[0], &q[1], 0x20FFFFFF, 0);
        Draw_line3d(&q[1], &q[2], 0x20FFFFFF, 0);
        Draw_line3d(&q[2], &q[3], 0x20FFFFFF, 0);
        Draw_line3d(&q[3], &q[0], 0x20FFFFFF, 0);
    }
}

// Too many lights on the model: flag it, draw a marker line and its bounding boxes in red.
void cModel::error()
{
    Vec v;

    if (DbgFlagChk(pG, DBG_ERROR_CK)) {
        be_flag |= 0x80000000;
        v.x = pos.x;
        v.y = pos.y + 50000.0f;
        v.z = pos.z;
        Draw_line3d(&pos, &v, 0xFFFFFFFF, 0);
        if (pModelInfo) {
            drawAllBoundingBox(pModelInfo);
            pModelInfo->color[0] = 0xFF;
            {
                cModelInfo* info = pModelInfo;
                info->color[2] = 0x40;
                info->color[1] = 0x40;
            }
        }
    }
}

