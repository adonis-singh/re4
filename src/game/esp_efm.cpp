// game/esp_efm: effect models (Efm) of the effect sequence system (D:/Bio4/Prog/esp_efm.cpp). An
// effect record whose Id is 0xFC..0xFF creates a model object in ObjMgr instead of a sprite:
// obj04 (particle model, Id 0xFF; 0xFC uses a scroll model), obj05 (scatter/debris model, 0xFE)
// or obj09 (rigid body, 0xFD). EfmSeqSet is the entry point from the generator; EfmSetObj04/05/09
// fill the object's work from the record; EfmDelete / EfmDeleteEvent / EfmArrayClear destroy them
// by EfmCore owner, at event end and at room clear.
#include "light.h"
#include "atari.h"
#include "obj.h"
#include "obj04.h"
#include "obj05.h"
#include "obj09.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "scroll.h"
#include "TexRender.h"
#include "db_log.h"

extern "C" {
u32 GetEfmMoveIdMax();
u8 GetEfmMoveId(u32 no);
void EfmDeleteSub(cObj* obj);
void EfmDeleteEventSub(cObj* obj);
cObj* EfmSetObj04(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs);
cObj* EfmSetObj05(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate);
cObj* EfmSetObj09(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate);
void setModTexRender(cObj* obj, int no);
cObj* SetEffModel(void* bin, void* tpl, Vec* pos, Vec* rot);   // embox.cpp declares it `void` locally
}

// The effect code converts with 3.14, not PI.
#undef DEG2RAD
#define DEG2RAD (3.14f / 180.0f)


// Read a pointer member through a reference (no struct flag): the store to the stack local
// `model` in between may alias it, so `scr->pInfo` is reloaded for `tpl` (EfmSeqSet).
template <class T> static inline T PRef(T& v)
{
    return v;
}

// Effect model (Efm) set up: the effect sequence record `gen` creates an obj04 / obj05 / obj09
// work in ObjMgr and fills its work from the record.

u8 EfmIdTbl[4] = {4, 5, 9, 4};

// Light parameters shared by the model set ups (defined after EfmSeqSet: external linkage puts
// them into .rodata at the definition, between the EfmSeqSet and EfmSetObj04 strings; a
// `static const` would be deferred to the end of the unit).
extern const Vec efm_light_pos;
extern const Vec efm_light_size;

u16 g_Core_flg;
u8 g_Core_kind;
cModel* g_Core_pEm;

// Number of Efm move kinds (4: the entries of EfmIdTbl).
u32 GetEfmMoveIdMax()
{
    return 4;
}

// Maps the Efm move kind (0..3) to the ObjMgr object id (4, 5, 9, 4); out of range reads entry 0.
u8 GetEfmMoveId(u32 id)
{
    if (id >= GetEfmMoveIdMax()) {
        id = 0;
    }
    return EfmIdTbl[id];
}

// Destroys every Efm object whose EfmCore matches: flg == a, kind == b, pEm == c (each test skipped
// when the value is 0). Used to remove the effect models an enemy/effect owner spawned.
void EfmDelete(int a, int b, void* c)
{
    cObjMgr* m = &ObjMgr;
    void (*func)(cObj*) = EfmDeleteSub;
    u32 i;

    g_Core_flg = a;
    g_Core_kind = b;
    g_Core_pEm = (cModel*) c;
    for (i = 0; i < m->nArray; i++) {
        func(m->fastAt(i));
    }
}

// Per-object test for EfmDelete: destroys obj04/05/09 works whose core matches the g_Core_* filter.
void EfmDeleteSub(cObj* pObj)
{
    if (pObj->id == 4) {
        Efm04Work* w = EFM04_WK((cObj04*) pObj);
        if ((g_Core_flg == 0 || w->Eff_core.flg == g_Core_flg) && (g_Core_kind == 0 || w->Eff_core.kind == g_Core_kind) &&
            (g_Core_pEm == 0 || w->Eff_core.pEm == g_Core_pEm)) {
            ObjMgr.destroy(pObj);
        }
    }
    if (pObj->id == 5) {
        Efm05Work* w = EFM05_WK((cObj05*) pObj);
        if ((g_Core_flg == 0 || w->Eff_core.flg == g_Core_flg) && (g_Core_kind == 0 || w->Eff_core.kind == g_Core_kind) &&
            (g_Core_pEm == 0 || w->Eff_core.pEm == g_Core_pEm)) {
            ObjMgr.destroy(pObj);
        }
    }
    if (pObj->id == 9) {
        Efm09Work* w = EFM09_WK((cObj09*) pObj);
        if ((g_Core_flg == 0 || w->Eff_core.flg == g_Core_flg) && (g_Core_kind == 0 || w->Eff_core.kind == g_Core_kind) &&
            (g_Core_pEm == 0 || w->Eff_core.pEm == g_Core_pEm)) {
            ObjMgr.destroy(pObj);
        }
    }
}

// Destroys every Efm object that is neither permanent (core.flg bit 0) nor event-owned (bit 0x800):
// called when an event ends to drop the effect models it left behind.
void EfmDeleteEvent()
{
    cObjMgr* m = &ObjMgr;
    void (*func)(cObj*) = EfmDeleteEventSub;
    u32 i;

    for (i = 0; i < m->nArray; i++) {
        func(m->fastAt(i));
    }
}

// Per-object test for EfmDeleteEvent.
void EfmDeleteEventSub(cObj* pObj)
{
    if (pObj->id == 4) {
        Efm04Work* w = EFM04_WK((cObj04*) pObj);
        if (!(w->Eff_core.flg & 1) && !(w->Eff_core.flg & 0x800)) {
            ObjMgr.destroy(pObj);
        }
    }
    if (pObj->id == 5) {
        Efm05Work* w = EFM05_WK((cObj05*) pObj);
        if (!(w->Eff_core.flg & 1) && !(w->Eff_core.flg & 0x800)) {
            ObjMgr.destroy(pObj);
        }
    }
    if (pObj->id == 9) {
        Efm09Work* w = EFM09_WK((cObj09*) pObj);
        if (!(w->Eff_core.flg & 1) && !(w->Eff_core.flg & 0x800)) {
            ObjMgr.destroy(pObj);
        }
    }
}

// Destroys every Efm object in ObjMgr's alive list (filter cleared): room change / effect reset.
void EfmArrayClear()
{
    g_Core_flg = 0;
    g_Core_kind = 0;
    g_Core_pEm = 0;
    ObjMgr.applyFuncAll(EfmDeleteSub);
}

// Generator entry for an effect model record: resolves the parent (gen->Parent_no is a scroll object
// unless info->flg bit 0x1000), maps gen->Id 0xFF/0xFE/0xFD/0xFC to move kind 0/1/2/3, fetches the
// model+tpl (Efm table by Tex_id, or the scroll object's model for kind 3), creates the obj04/05/09
// in ObjMgr with the light set-up from Tool_flg (0x80 -> 4, 0x20000 -> 8, else 0x10), then calls the
// kind's EfmSetObj. info->flg bit 0 makes the object survive suspends; an owner model with
// be_flag 0x9 passes its AddAmb colour down. Returns the object or 0 on any failure (logged).
cObj* EfmSeqSet(EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs)
{
    cObj* obj = 0;
    Vec size;
    Vec center;
    void* model;
    void* tpl;
    u32 moveId;

    if (!(info->flg & 0x1000) && gen->Parent_no != 0) {
        parent = SmdGetObjPtr(gen->Parent_no - 1);
        if (parent == 0) {
            pLog->err(0, 0, "ESP_EFM : PARENT_NO[%d] Invalid.", gen->Parent_no);
            return 0;
        }
    }
    switch (gen->Id) {
    case 0xFF:
        moveId = 0;
        break;
    case 0xFE:
        moveId = 1;
        break;
    case 0xFD:
        moveId = 2;
        break;
    case 0xFC:
        moveId = 3;
        break;
    default:
        pLog->err(0, 0, "ESP_EFM : EFM_MOVE_ID[%x] is invalid.", gen->Id);
        return 0;
    }
    if (moveId >= GetEfmMoveIdMax()) {
        pLog->err(0, 0, "ESP_EFM : EFM_MOVE_ID[%x] is invalid.", moveId);
        return 0;
    }
    if (moveId == 3) {
        cObj* scr = SmdGetGroupObjPtr(gen->Tex_id);
        if (scr == 0) {
            pLog->err(0, 0, "ESP_EFM : SCR_MODEL_NO[%x] is invalid.", gen->Tex_id);
            return 0;
        }
        model = PRef(scr->pModelInfo)->model_addr;
        tpl = PRef(scr->pModelInfo)->tpl_addr;
    } else {
        if (EspGetEfmAddr(gen->Tex_id, &model, &tpl) == 0) {
            pLog->err(0, 0, "ESP_EFM : EFM_ID[%x] is invalid.", gen->Tex_id);
            return 0;
        }
    }
    switch (moveId) {
    case 0:
    case 3: {
        int light;
        obj = ObjMgr.createBack(cObjMgr::ID_ESP);
        if (obj == 0) {
            break;
        }
        if (obj->modelInit(model, tpl) == 0) {
            ObjMgr.destroy(obj);
            pLog->err(0, 0, "ESP_EFM : ModelInit() failed.");
            return 0;
        }
        obj->atari.off();
        light = 0x10;
        if (gen->Tool_flg & 0x80) {
            light = 4;
        }
        if (gen->Tool_flg & 0x20000) {
            light = 8;
        }
        if (moveId == 3) {
            ModelBound* bound = &obj->pModelInfo->bound;
            size.x = bound->size.x;
            size.y = bound->size.y;
            size.z = bound->size.z;
            PSVECSubtract(&bound->center, &obj->pList->pos, &center);
            obj->LightInfo.init2(2, 1, &center, &size, light);
            obj->alpha_omit = 0x80;
        } else {
            obj->LightInfo.init2(0, 1, &efm_light_pos, &efm_light_size, light);
        }
        obj->id = GetEfmMoveId(moveId);
        obj = EfmSetObj04(obj, gen, info, seed, parent, m, x, rate, ofs);
        if (obj && (info->flg & 1)) {
            obj->setNoSuspend(1);
        }
        break;
    }
    case 1: {
        int light;
        obj = ObjMgr.createBack(cObjMgr::ID_KABOOM);
        if (obj == 0) {
            break;
        }
        if (obj->modelInit(model, tpl) == 0) {
            ObjMgr.destroy(obj);
            pLog->err(0, 0, "ESP_EFM : ModelInit() failed.");
            return 0;
        }
        obj->atari.off();
        light = 0x10;
        if (gen->Tool_flg & 0x80) {
            light = 4;
        }
        if (gen->Tool_flg & 0x20000) {
            light = 8;
        }
        obj->LightInfo.init2(0, 1, &efm_light_pos, &efm_light_size, light);
        obj->id = GetEfmMoveId(1);
        obj = EfmSetObj05(obj, gen, info, seed, parent, m, x, rate);
        if (obj && (info->flg & 1)) {
            obj->setNoSuspend(1);
        }
        break;
    }
    case 2:
        obj = ObjMgr.createBack(cObjMgr::ID_ESP2);
        if (obj == 0) {
            break;
        }
        if (obj->modelInit(model, tpl) == 0) {
            ObjMgr.destroy(obj);
            pLog->err(0, 0, "ESP_EFM : ModelInit() failed.");
            return 0;
        }
        obj->atari.off();
        obj->LightInfo.init2(0, 1, &efm_light_pos, &efm_light_size, 0x10);
        obj->id = GetEfmMoveId(2);
        obj = EfmSetObj09(obj, gen, info, seed, parent, m, x, rate);
        if (obj && (info->flg & 1)) {
            obj->setNoSuspend(1);
        }
        break;
    }
    if (info->pEm != 0 && obj != 0 && (info->pEm->be_flag & 9) == 9) {
        u8 r = info->pEm->AddAmb_r;
        u8 g = info->pEm->AddAmb_g;
        u8 b = info->pEm->AddAmb_b;
        obj->SetAddAmb(r, g, b);
    }
    return obj;
}

const Vec efm_light_pos = {0.0f, 0.0f, 0.0f};
const Vec efm_light_size = {1000.0f, 1000.0f, 0.0f};

// Fills an obj04 (particle model) work from the record: position/speed/acceleration/angle/rotation
// speed with the R_* random spreads (angles in degrees -> radians), scale (Size_base*0.005), colour
// and fade timers, blend mode, draw order (ot_type 2 when Tool_flg 0x400000, 1 when translucent),
// then attaches it: Parts_no 0xFF world with matrix m, 0xF8..0xFE world, otherwise parts Parts_no of
// `parent` (Tool_flg 0x20: only the parent's rotation, no follow). Tool_flg bit 2 spawns an est
// child effect, bit 3 starts motion WorkSp8[2]. Returns 0 (object destroyed) on a bad parts number.
cObj* EfmSetObj04(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs)
{
    Efm04Work* w = EFM04_WK((cObj04*) obj);
    Vec v;
    Mtx mtx;
    void* mot;
    f32 rnd;
    cParts* parts;

    obj->be_flag |= 0x4000;
    w->Eff_core = *info;
    w->Parts_no = gen->Parts_no;
    w->Tool_flg = gen->Tool_flg;
    obj->pos = gen->Pos;
    obj->pos.x += gen->R_pos.x * fRandSeed1_1(seed);
    obj->pos.y += gen->R_pos.y * fRandSeed1_1(seed);
    obj->pos.z += gen->R_pos.z * fRandSeed1_1(seed);
    obj->speed = gen->Speed;
    obj->speed.x += gen->R_speed.x * fRandSeed1_1(seed);
    obj->speed.y += gen->R_speed.y * fRandSeed1_1(seed);
    obj->speed.z += gen->R_speed.z * fRandSeed1_1(seed);
    w->D_speed = gen->D_speed;
    w->Speed_plus = gen->Speed_plus;
    w->Speed_plus.x += gen->R_speed_plus.x * fRandSeed1_1(seed);
    w->Speed_plus.y += gen->R_speed_plus.y * fRandSeed1_1(seed);
    w->Speed_plus.z += gen->R_speed_plus.z * fRandSeed1_1(seed);
    obj->ang = gen->Ang;
    obj->ang.x += gen->R_ang.x * fRandSeed1_1(seed);
    obj->ang.y += gen->R_ang.y * fRandSeed1_1(seed);
    obj->ang.z += gen->R_ang.z * fRandSeed1_1(seed);
    PSVECScale(&obj->ang, &obj->ang, DEG2RAD);
    w->Ang_plus = gen->Ang_plus;
    w->Ang_plus.x += gen->R_ang_plus.x * fRandSeed1_1(seed);
    w->Ang_plus.y += gen->R_ang_plus.y * fRandSeed1_1(seed);
    w->Ang_plus.z += gen->R_ang_plus.z * fRandSeed1_1(seed);
    PSVECScale(&w->Ang_plus, &w->Ang_plus, DEG2RAD);
    w->Size_base_x = gen->Size_base_x * 0.005f;
    w->Size_base_y = gen->Size_base_y * 0.005f;
    w->Size_mul = 1.0f;
    rnd = gen->R_size_base * fRandSeed1_1(seed) * 0.005f;
    w->Size_base_x += rnd;
    w->Size_base_y += rnd;
    w->Size_plus = gen->Size_plus;
    w->D_size_plus = gen->D_size_plus;
    w->Col_start_r = gen->Col_start_r;
    w->Col_start_g = gen->Col_start_g;
    w->Col_start_b = gen->Col_start_b;
    w->Col_start_a = gen->Col_start_a;
    w->Col_r = (f32) gen->Col_start_r;
    w->Col_g = (f32) gen->Col_start_g;
    w->Col_b = (f32) gen->Col_start_b;
    w->Col_a = (f32) gen->Col_start_a;
    w->Col_d_r = gen->Col_d_r;
    w->Col_d_g = gen->Col_d_g;
    w->Col_d_b = gen->Col_d_b;
    w->Col_d_a = gen->Col_d_a;
    if (gen->Tool_flg & 0x400000) {
        obj->ot_type = 2;
    } else if (w->Col_a < 250.0f) {
        obj->ot_type = 1;
    } else {
        obj->ot_type = 0;
    }
    w->Col_max_cnt = gen->Col_max_cnt;
    w->Col_start_cnt = gen->Col_start_cnt;
    w->Pos_start_cnt = gen->Pos_start_cnt;
    w->Size_start_cnt = gen->Size_start_cnt;
    w->Life_max = gen->Life_max;
    w->Life_time = gen->Life_time;
    w->Release_time = gen->Release_time;
    obj->pModelInfo->color[0] = (u8) w->Col_r;
    obj->pModelInfo->color[1] = (u8) w->Col_g;
    obj->pModelInfo->color[2] = (u8) w->Col_b;
    obj->pModelInfo->color[3] = 0xFF;
    if (w->Col_max_cnt == 0) {
        obj->invisible_factor = w->Col_a * (1.0f / 255.0f);
    } else {
        obj->invisible_factor = 0.0f;
    }
    obj->pModelInfo->blend_mode = gen->Blend_type;
    obj->scale.y = w->Size_base_y * w->Size_mul;
    obj->scale.z = obj->scale.x = w->Size_base_x * w->Size_mul;
    w->Pt_hit_size = (f32) (int) gen->xD4;
    if (gen->prm.w.xCC != 0) {
        u8 c = gen->prm.b.xCF;
        obj->SetAddAmb(c, c, c);
    }
    if (gen->prm.w.xD0 != 0) {
        setModTexRender(obj, gen->prm.w.xD0 - 1);
    }
    w->RefRate = gen->Vec1;
    PSVECScale(&w->RefRate, &w->RefRate, 0.1f);
    if (!(w->Tool_flg & 0x40)) {
        obj->LightInfo.SelectMask = 0;
        obj->be_flag |= 0x20000;
    }
    if (w->Tool_flg & 0x200000) {
        obj->setZMode(1);
    }
    switch (w->Parts_no) {
    case 0xFF:
        w->pParts = pEffParentWorld;
        Efm04RotMatrix(obj, m);
        break;
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
        w->pParts = pEffParentWorld;
        break;
    case 0xFE:
        w->pParts = pEffParentWorld;
        if (gen->Release_time != 0) {
            pLog->warn(0, 0, "ESP_EFM : ReleaseTime not 0 but no parent.");
        }
        break;
    default:
        if (parent == 0) {
            pLog->err(0, 0, "ESP_EFM : PARTS_NO[%d] but Not on parts.", w->Parts_no);
            ObjMgr.destroy(obj);
            return 0;
        }
        if (w->Parts_no < parent->nParts) {
            if (w->Tool_flg & 0x20) {
                parts = parent->getPartsPtr(w->Parts_no);
                PSMTXIdentity(mtx);
                RotMatrix(mtx, &parent->ang);
                PSMTXMultVecSR(mtx, &obj->pos, &v);
                mtx[0][3] = parts->mat[0][3] + v.x;
                mtx[1][3] = parts->mat[1][3] + v.y;
                mtx[2][3] = parts->mat[2][3] + v.z;
                w->pParts = pEffParentWorld;
                Efm04RotMatrix(obj, mtx);
            } else {
                // pMod/Guid_pMod stored through a word pointer: the store `(mem link)` has a
                // register address, so cse1 (following the `beq` into this arm) treats it as
                // aliasing `w->Parts_no` and the getPartsPtr argument is reloaded (`lbz r4,0x79(w)`);
                // combine folds the address back into `stw 0x6C(w)`. A plain member store never
                // conflicts (same base, disjoint offsets) and the arm reuses the switch register.
                u32* link = (u32*) &w->pMod;
                link[0] = (u32) parent;
                link[1] = parent->guid;
                w->pParts = parent->getPartsPtr(w->Parts_no);
                if (ofs) {
                    PSVECAdd(&obj->pos, ofs, &obj->pos);
                }
            }
        } else {
            pLog->err(0, 0, "ESP_EFM : PARTS_NO[%d] is invalid(MAX:%d).", w->Parts_no, parent->nParts);
            ObjMgr.destroy(obj);
            return 0;
        }
        break;
    }
    if (w->Tool_flg & 4) {
        EstSet(obj, -1, 0, 0, gen->WorkSp8[0], gen->WorkSp8[1], 0, ESP_CORE_KIND_NONE, obj, 0);
    }
    if (w->Tool_flg & 8) {
        w->Motion_no = gen->WorkSp8[2];
        if (EspGetEfmMotAddr(gen->Tex_id, w->Motion_no, &mot)) {
            switch (gen->WorkSp8[3]) {
            case 0:
                MotionSetCore(obj, &obj->Motion, mot, 0, 0, 0, 0);
                break;
            case 1:
                MotionSetCore(obj, &obj->Motion, mot, 0, 0, 4, 0);
                break;
            default:
                pLog->err(0, 0, "ESP_EFM04 : MotionType[%d] is invalid.", gen->WorkSp8[3]);
                break;
            }
        } else {
            pLog->err(0, 0, "ESP_EFM04 : MotionNo[%d] is invalid.", w->Motion_no);
            w->Tool_flg |= 8;
        }
    }
    obj->move();
    return obj;
}

// Fills an obj05 (scattering multi-parts model) work from the record: position/angle/rotation speed
// with random spreads, scale, colours and fade timers, the scatter centre (Vec0 +- Vec2), bounce
// (Vec1/10), pow/rangeStep/rnd/rotAmp from Work8, gravity (-xCC/10) and speed damping (1 - xD0/1000);
// orientation from matrix m or the parent parts' matrix; each parts starts with Kaboom_flg 0.
cObj* EfmSetObj05(cObj* obj, EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate)
{
    Efm05Work* w = EFM05_WK((cObj05*) obj);
    Vec v;
    Mtx mtx;
    f32 rnd;
    cParts* parts;
    cParts* p;
    u32 i;

    w->Eff_core = *info;
    w->Tool_flg = gen->Tool_flg;
    w->Rand_seed = *seed;
    obj->pos = gen->Pos;
    obj->pos.x += gen->R_pos.x * fRandSeed1_1(seed);
    obj->pos.y += gen->R_pos.y * fRandSeed1_1(seed);
    obj->pos.z += gen->R_pos.z * fRandSeed1_1(seed);
    obj->ang = gen->Ang;
    obj->ang.x += gen->R_ang.x * fRandSeed1_1(seed);
    obj->ang.y += gen->R_ang.y * fRandSeed1_1(seed);
    obj->ang.z += gen->R_ang.z * fRandSeed1_1(seed);
    PSVECScale(&obj->ang, &obj->ang, DEG2RAD);
    if ((w->Tool_flg & 0x10) && parent) {
        Matrix2AxisAngle(parent->pList->mat, &v);
        PSVECAdd(&obj->ang, &v, &obj->ang);
    }
    if (w->Tool_flg & 0x200000) {
        obj->setZMode(1);
    }
    w->Ang_plus = gen->Ang_plus;
    w->Ang_plus.x += gen->R_ang_plus.x * fRandSeed1_1(seed);
    w->Ang_plus.y += gen->R_ang_plus.y * fRandSeed1_1(seed);
    w->Ang_plus.z += gen->R_ang_plus.z * fRandSeed1_1(seed);
    PSVECScale(&w->Ang_plus, &w->Ang_plus, DEG2RAD);
    w->Size_base_x = gen->Size_base_x * 0.005f;
    w->Size_base_y = gen->Size_base_y * 0.005f;
    w->Size_mul = 1.0f;
    rnd = gen->R_size_base * fRandSeed1_1(seed);
    w->Size_base_x += rnd;
    w->Size_base_y += rnd;
    w->Size_plus = gen->Size_plus;
    w->D_size_plus = gen->D_size_plus;
    w->Col_start_r = gen->Col_start_r;
    w->Col_start_g = gen->Col_start_g;
    w->Col_start_b = gen->Col_start_b;
    w->Col_start_a = gen->Col_start_a;
    w->Col_r = (f32) gen->Col_start_r;
    w->Col_g = (f32) gen->Col_start_g;
    w->Col_b = (f32) gen->Col_start_b;
    w->Col_a = (f32) gen->Col_start_a;
    w->Col_d_r = gen->Col_d_r;
    w->Col_d_g = gen->Col_d_g;
    w->Col_d_b = gen->Col_d_b;
    w->Col_d_a = gen->Col_d_a;
    if (gen->Tool_flg & 0x400000) {
        obj->ot_type = 2;
    } else if (w->Col_a < 250.0f) {
        obj->ot_type = 1;
    } else {
        obj->ot_type = 0;
    }
    w->Col_max_cnt = gen->Col_max_cnt;
    w->Col_start_cnt = gen->Col_start_cnt;
    w->Pos_start_cnt = gen->Pos_start_cnt;
    w->Size_start_cnt = gen->Size_start_cnt;
    w->Life_max = gen->Life_max;
    w->Life_time = gen->Life_time;
    obj->pModelInfo->color[0] = (u8) w->Col_r;
    obj->pModelInfo->color[1] = (u8) w->Col_g;
    obj->pModelInfo->color[2] = (u8) w->Col_b;
    obj->pModelInfo->color[3] = 0xFF;
    if (w->Col_max_cnt == 0) {
        obj->invisible_factor = w->Col_a * (1.0f / 255.0f);
    } else {
        obj->invisible_factor = 0.0f;
    }
    obj->pModelInfo->blend_mode = gen->Blend_type;
    if (!(w->Tool_flg & 0x40)) {
        obj->LightInfo.SelectMask = 0;
        obj->be_flag |= 0x20000;
    }
    w->Kaboom_pos = gen->Vec0;
    w->Kaboom_pos.x += gen->Vec2.x * fRandSeed1_1(seed);
    w->Kaboom_pos.y += gen->Vec2.y * fRandSeed1_1(seed);
    w->Kaboom_pos.z += gen->Vec2.z * fRandSeed1_1(seed);
    w->RefRate = gen->Vec1;
    PSVECScale(&w->RefRate, &w->RefRate, 0.1f);
    w->Kaboom_pow = gen->Work8[0];
    w->Kaboom_spd = gen->Work8[1];
    w->Kaboom_rnd = gen->Work8[2];
    w->Kaboom_rot = gen->Work8[3];
    w->Kaboom_gravity = (f32) (int) gen->prm.w.xCC * -0.1f;
    w->Kaboom_d_spd = 1.0f - (f32) (int) gen->prm.w.xD0 * 0.001f;
    w->Pt_hit_size = gen->xD4;
    switch (gen->Parts_no) {
    case 0xFF:
        Efm05RotMatrix(obj, m);
        break;
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
        Efm05RotMatrix(obj, m);
        break;
    case 0xFE:
        if (gen->Release_time != 0) {
            pLog->warn(0, 0, "ESP_EFM : ReleaseTime not 0 but no parent.");
        }
        break;
    default:
        if (parent == 0) {
            pLog->err(0, 0, "ESP_EFM : PARTS_NO[%d] but Not on parts.", gen->Parts_no);
            ObjMgr.destroy(obj);
            return 0;
        }
        if (gen->Parts_no < parent->nParts) {
            if (w->Tool_flg & 0x20) {
                parts = parent->getPartsPtr(gen->Parts_no);
                PSMTXIdentity(mtx);
                RotMatrix(mtx, &parent->ang);
                PSMTXMultVecSR(mtx, &obj->pos, &v);
                mtx[0][3] = parts->mat[0][3];
                mtx[1][3] = parts->mat[1][3];
                mtx[2][3] = parts->mat[2][3];
                Efm05RotMatrix(obj, mtx);
            } else {
                parts = parent->getPartsPtr(gen->Parts_no);
                Efm05RotMatrix(obj, parts->mat);
            }
        } else {
            pLog->err(0, 0, "ESP_EFM : PARTS_NO[%d] is invalid(MAX:%d).", gen->Parts_no, parent->nParts);
            ObjMgr.destroy(obj);
            return 0;
        }
        break;
    }
    for (p = obj->pList, i = 0; i < obj->nParts; i++, p = p->pList) {
        p->Kaboom_flg = 0;
    }
    obj->matUpdate();
    if (w->Tool_flg & 4) {
        EstSet(obj, -1, 0, 0, gen->WorkSp8[0], gen->WorkSp8[1], 0, ESP_CORE_KIND_NONE, obj, 0);
    }
    if (gen->WorkSp8[2] != 0) {
        setModTexRender(obj, gen->WorkSp8[2] - 1);
    }
    obj->be_flag |= 0x1000;
    obj->move();
    return obj;
}

// Fills an obj09 (rigid body) work: start position/velocity with spreads, box size = Vec0*100+250
// (1/1000 units), mass = volume/1e9 * mass_mul, moments of inertia of the box * moment_mul, scale
// from the size (Efm 0x7C and 0x21 use a smaller visual scale).
cObj* EfmSetObj09(cObj* pObj, EspGenWork* pSeq, EfmCore* pCore, u32* pRand_seed, cModel* pMod, Mtx pMat, int flg, f32 ang)
{
    Efm09Work* w = EFM09_WK((cObj09*) pObj);
    static f32 mass_mul = 1.0f;
    static f32 moment_mul = 2.0f;

    pObj->be_flag |= 0x10;
    if (DbgFlagChk(pG, DBG_IN_ESP_TOOL)) {
        DpfFlagOff(pG, DPF_SHADOW);
    }
    w->Eff_core = *pCore;
    w->X = pSeq->Pos;
    w->X.x += pSeq->R_pos.x * fRandSeed1_1(pRand_seed);
    w->X.y += pSeq->R_pos.y * fRandSeed1_1(pRand_seed);
    w->X.z += pSeq->R_pos.z * fRandSeed1_1(pRand_seed);
    w->X.y += 0.0001f;
    w->prev_X = w->X;
    w->V = pSeq->Speed;
    w->V.x += pSeq->R_speed.x * fRandSeed1_1(pRand_seed);
    w->V.y += pSeq->R_speed.y * fRandSeed1_1(pRand_seed);
    w->V.z += pSeq->R_speed.z * fRandSeed1_1(pRand_seed);
    PSMTXIdentity(w->R);
    w->w.x = 0.0f;
    w->w.y = 0.0f;
    w->w.z = 0.0f;
    w->wg = pSeq->Ang_plus;
    w->wg.x += pSeq->R_ang_plus.x * fRandSeed1_1(pRand_seed);
    w->wg.y += pSeq->R_ang_plus.y * fRandSeed1_1(pRand_seed);
    w->wg.z += pSeq->R_ang_plus.z * fRandSeed1_1(pRand_seed);
    w->size.x = pSeq->Vec0.x * 100.0f + 250.0f;
    w->size.y = pSeq->Vec0.y * 100.0f + 250.0f;
    w->size.z = pSeq->Vec0.z * 100.0f + 250.0f;
    w->m = w->size.x * w->size.y * w->size.z / 1000000000.0f;
    w->m *= mass_mul;
    PSVECScale(&w->V, &w->V, w->m * 100.0f);
    w->Ig.x = moment_mul * w->m * (w->size.y * w->size.y + w->size.z * w->size.z) / 12.0f;
    w->Ig.y = moment_mul * w->m * (w->size.x * w->size.x + w->size.z * w->size.z) / 12.0f;
    w->Ig.z = moment_mul * w->m * (w->size.x * w->size.x + w->size.y * w->size.y) / 12.0f;
    pObj->scale = w->size;
    PSVECScale(&pObj->scale, &pObj->scale, 0.01f);
    if (pSeq->Tex_id == 0x7C) {
        pObj->scale.x *= 0.05f;
        pObj->scale.y *= 0.05f;
        pObj->scale.z *= 0.05f;
    }
    if (pSeq->Tex_id == 0x21) {
        pObj->scale.x *= 0.5f;
        pObj->scale.y *= 0.5f;
        pObj->scale.z *= 0.5f;
    }
    pObj->move();
    return pObj;
}

// Creates a permanent, world-parented obj04 for a plain model (bin/tpl) at pos/rot with white colour,
// unit scale and no motion: used by room code for static effect models (embox declares it too).
cObj* SetEffModel(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    Efm04Work* w;

    obj = ObjMgr.createBack(cObjMgr::ID_ESP);
    if (obj) {
        if (obj->modelInit(bin, tpl) == 0) {
            ObjMgr.destroy(obj);
            pLog->err(0, 0, "ESP_EFM : ModelInit() failed.");
            return 0;
        }
        obj->atari.off();
        obj->LightInfo.init2(0, 1, &efm_light_pos, &efm_light_size, 0x10);
        obj->id = 4;
        obj->setNoSuspend(1);
        w = EFM04_WK((cObj04*) obj);
        w->Eff_core.flg = 1;
        obj->be_flag |= 0x4000;
        w->Parts_no = 0;
        w->Tool_flg = 0;
        obj->pos = *pos;
        w->D_speed = 0.0f;
        obj->ang = *rot;
        w->Size_base_x = 1.0f;
        w->Size_base_y = 1.0f;
        w->Size_mul = 1.0f;
        w->Col_d_r = 1.0f;
        w->Col_d_g = 1.0f;
        w->Col_start_r = 0xFF;
        w->Col_start_g = 0xFF;
        w->Col_start_b = 0xFF;
        w->Col_start_a = 0xFF;
        w->Col_r = (f32) w->Col_start_r;
        w->Col_g = (f32) w->Col_start_g;
        w->Col_b = (f32) w->Col_start_b;
        w->Col_a = (f32) w->Col_start_a;
        w->Col_d_b = 1.0f;
        w->Col_d_a = 1.0f;
        obj->ot_type = 1;
        obj->pModelInfo->color[0] = (u8) w->Col_r;
        obj->pModelInfo->color[1] = (u8) w->Col_g;
        obj->pModelInfo->color[2] = (u8) w->Col_b;
        obj->pModelInfo->color[3] = 0xFF;
        if (w->Col_max_cnt == 0) {
            obj->invisible_factor = w->Col_a * (1.0f / 255.0f);
        } else {
            obj->invisible_factor = 0.0f;
        }
        obj->scale.y = w->Size_base_y * w->Size_mul;
        obj->scale.z = obj->scale.x = w->Size_base_x * w->Size_mul;
        w->pParts = pEffParentWorld;
        obj->move();
    }
    return obj;
}

// Makes the object draw with texture-render buffer `no` (refraction shader: Shader_type 1,
// Refract_pow 0xF, Refract_ratio 0xB4) when that buffer is in use.
void setModTexRender(cObj* pMod, int no)
{
    static u8 buf[0x20];
    u8* tbl = buf;
    TexRenderMng* mgr = GetTexRenderMgrAddr(no);

    if (mgr->used == 0) {
        return;
    }
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = mgr->m_Tex_no;
    pMod->pModelInfo->setTexBlendTbl(tbl);
    pMod->pModelInfo->setBlendRatio(0xFF);
    pMod->Shader_type = 1;
    pMod->Refract_pow = 0xF;
    pMod->Refract_ratio = 0xB4;
}

// .sdata alignment padding of the split object
asm(".section .sdata; .balign 8");
