// Host driver around the game's own motion code. Python calls these through ctypes.
//
// The helper hosts a real cModel (stub layout) with a cParts chain built like cModel::initJoint
// does from the .bin's parts records, then runs the game's MotionSetCore / MotionMove on it: key
// decoding (HermiteInterpolation, Fcc_get_data_*), MotionMoveCore, cModel::partsMatCalc /
// partsWorldCalc, the leg/arm IK (ik.cpp) and the quaternion blend table, all compiled from the
// game sources (see Makefile). The motion image handed in is the little-endian re-serialisation
// of a motion (fcv.serialise(m, '<')): same layout as the file, every u16/f32/s16 field
// byte-swapped, so the game's byte-wise readers produce the GameCube values unchanged. The image
// is copied into memory below 4 GB (MAP_32BIT) because MotionSetCore relocates the key offsets in
// place as 32-bit pointers.
//
// Hand-written here (no game source available for the host): the C stand-ins for the paired-
// single SDK routines that have no C version (SQRTF, LIMIT_ANGLE, memclr_asm), the log / camera /
// floor-collision stubs, and the model construction.
#include "re4_host_stub.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define API extern "C" __attribute__((visibility("default")))

// ---- stubs for the globals the game code references ----------------------------------------------

static Global g_global = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 1.0f };
Global* pG = &g_global;
cModel* pPL = NULL;
static Log g_log;
Log* pLog = &g_log;
CamCtrlStub CamCtrl;
SatMgrStub SatMgr;
extern const Vec vecZero = { 0.0f, 0.0f, 0.0f };

static int g_errors = 0;

void Log::err(int, int, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("[game log] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    g_errors++;
}

void CamCtrlStub::registAttachCamera(AttachCamera*, cModel*) {}
void CamCtrlStub::deleteAttachCamera(AttachCamera*, cModel*) {}

// No scenario collision on the host: "no floor" (InverseKinematics keeps the key target).
f32 SatMgrStub::getFloor(Vec*, u32*, f32, f32, int) { return -100000.0f; }

extern "C" void OSReport(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

extern "C" void eprintf(int, int, int, int, const char*, ...) {}

extern "C" void memclr_asm(void* dst, u32 n) { memset(dst, 0, n); }

extern "C" void cModel_matBlend(cModel* m, f32 rate) { m->matBlend(rate); }

API int mot_host_errors(void) { return g_errors; }
API void mot_host_clear_errors(void) { g_errors = 0; }

// ---- paired-single routines without a C source --------------------------------------------------

// math_sub.cpp SQRTF: frsqrte + one Newton step on the GameCube; the correctly rounded sqrt here.
f32 SQRTF(f32 x)
{
    if (x <= 0.00001f) {
        return 0.0f;
    }
    return sqrtf(x);
}

// math_sub.cpp LIMIT_ANGLE (asm loop): wrap into [-PI, PI).
f32 LIMIT_ANGLE(f32 x)
{
    while (x >= PI) {
        x -= PI2;
    }
    while (x < -PI) {
        x += PI2;
    }
    return x;
}

// ---- the hosted model ------------------------------------------------------------------------------

struct HostModel {
    cEm model;
    cEm* parts;
    int n;
    u8* image;        // MAP_32BIT copy of the motion image MotionSetCore relocated
    size_t imageSize;
};

static void coordInit(cCoord* c)
{
    memset(c, 0, sizeof(*c));
    PSMTXIdentity(c->mat);
    PSMTXIdentity(c->l_mat);
    c->scale.x = c->scale.y = c->scale.z = 1.0f;
    c->r_scale.x = c->r_scale.y = c->r_scale.z = 1.0f;
}

// cModel::initJoint for a .bin: n parts, parent[i] (< 0: the model), rest positions; the parts
// list is the parts order (getPartsPtr walks it), the bind matrices come from cModel::setPartsOffset
// (identity with the negated rest world position). Model at the origin, unit scale.
API HostModel* mot_model_create(int n, const int* parent, const float* restPos)
{
    HostModel* h = (HostModel*) calloc(1, sizeof(HostModel));
    h->n = n;
    h->parts = (cEm*) calloc(n, sizeof(cEm));
    cEm* m = &h->model;
    coordInit(m);
    m->nParts = (u8) n;
    m->pList = n ? &h->parts[0] : NULL;
    m->Motion.Seq_speed = 1.0f;
    for (int i = 0; i < n; i++) {
        cEm* p = &h->parts[i];
        coordInit(p);
        p->pList = (i + 1 < n) ? &h->parts[i + 1] : NULL;
        p->pParent = parent[i] < 0 ? (cCoord*) m : (cCoord*) &h->parts[parent[i]];
        p->pos.x = restPos[3 * i];
        p->pos.y = restPos[3 * i + 1];
        p->pos.z = restPos[3 * i + 2];
    }
    // setPartsOffset
    m->partsMatCalc();
    m->partsWorldCalc();
    for (int i = 0; i < n; i++) {
        cEm* p = &h->parts[i];
        PSMTXIdentity(p->lt_inv_mat);
        p->lt_inv_mat[0][3] = -p->mat[0][3];
        p->lt_inv_mat[1][3] = -p->mat[1][3];
        p->lt_inv_mat[2][3] = -p->mat[2][3];
    }
    m->partsMatCalc();
    m->partsWorldCalc();
    for (int i = 0; i < n; i++) {
        h->parts[i].world_old = h->parts[i].world;
        h->parts[i].world_old2 = h->parts[i].world;
    }
    return h;
}

API void mot_model_destroy(HostModel* h)
{
    if (h->image) {
        munmap(h->image, h->imageSize);
    }
    free(h->parts);
    free(h);
}

// cModel::setJointInfo: the model's quaternion blend table {s32 count, u16 (dst, a, c, percent)...}
// (NULL to clear). MotionMove applies it after the IK.
API void mot_model_blend_table(HostModel* h, u16* tbl)
{
    h->model.Motion.blendTbl = tbl;
}

// MotionSetCore(m, &m->Motion, image, seq 0, hokan 0, flags, frame 0). ik 0: Mot_flag 0x10000000
// skips IKInit, no chain is solved. Returns the joint count.
API int mot_model_set(HostModel* h, const u8* image, size_t size, int flags, int ik)
{
    if (h->image) {
        munmap(h->image, h->imageSize);
        h->image = NULL;
    }
    h->image = (u8*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (h->image == MAP_FAILED) {
        h->image = NULL;
        return -1;
    }
    h->imageSize = size;
    memcpy(h->image, image, size);
    cEm* m = &h->model;
    m->Motion.Mot_flag = ik ? 0 : 0x10000000;
    MotionSetCore(m, &m->Motion, h->image, 0, 0, flags, 0);
    return m->Motion.Joint_num;
}

// One game frame at motion frame `frame`: MotionMove with the sequence frame forced (Mot_attr
// 0x8000: Mot_frame = Seq_frame), which runs MotionMoveCore, partsMatCalc, partsWorldCalc, the IK
// (when mot_model_set initialised the chains), the blend table.
// The model stays at the origin (Mot_attr bit 0 clear: the root keys do not move it).
API u16 mot_model_frame(HostModel* h, float frame)
{
    cEm* m = &h->model;
    m->Motion.Mot_attr |= 0x8000;
    m->Motion.Mot_attr &= ~1;
    m->Motion.Seq_frame = frame;
    return MotionMove(m, 0);
}

// Places the model (cModel pos/ang/scale; MotionMoveCore rebuilds its matrix from them), so the
// parts world matrices can be compared with a game whose player stands somewhere.
API void mot_model_place(HostModel* h, const float* pos, const float* ang, const float* scale)
{
    cEm* m = &h->model;
    memcpy(&m->pos, pos, sizeof(Vec));
    memcpy(&m->ang, ang, sizeof(Vec));
    memcpy(&m->scale, scale, sizeof(Vec));
}

// Root joint values of the current frame (MotionWork Pos / Ang after MotionGetSpeed would have
// read them): sampled directly like MotionGetPosition does, without touching the model.
API void mot_model_root(HostModel* h, float* pos, float* rot)
{
    Vec p, r;
    MotionGetPosition(&h->model, &p, &r);
    memcpy(pos, &p, sizeof(Vec));
    memcpy(rot, &r, sizeof(Vec));
}

// Parts state after mot_model_frame: pos/ang/scale (3 floats each), l_mat/mat (12 floats each,
// row-major 3x4), world (3), MotionParts flags. Any pointer may be NULL.
API void mot_model_read(HostModel* h, float* pos, float* ang, float* scale, float* l_mat, float* mat, float* world, u32* flags)
{
    for (int i = 0; i < h->n; i++) {
        cEm* p = &h->parts[i];
        if (pos) memcpy(pos + 3 * i, &p->pos, sizeof(Vec));
        if (ang) memcpy(ang + 3 * i, &p->ang, sizeof(Vec));
        if (scale) memcpy(scale + 3 * i, &p->scale, sizeof(Vec));
        if (l_mat) memcpy(l_mat + 12 * i, p->l_mat, sizeof(Mtx));
        if (mat) memcpy(mat + 12 * i, p->mat, sizeof(Mtx));
        if (world) memcpy(world + 3 * i, &p->world, sizeof(Vec));
        if (flags) flags[i] = p->motParts.flags;
    }
}

API float mot_model_max_frame(HostModel* h) { return h->model.Motion.Mot_frame_max; }

// ---- stateless helpers -----------------------------------------------------------------------------

API void mot_rot_matrix(const float* ang, float* m)
{
    Vec a = { ang[0], ang[1], ang[2] };
    RotMatrix(*(Mtx*) m, &a);
}

API void mot_quat_from_matrix(const float* m, float* q)
{
    Quaternion r;
    C_QUATMtx(&r, *(const Mtx*) m);
    q[0] = r.x;
    q[1] = r.y;
    q[2] = r.z;
    q[3] = r.w;
}

API float mot_hermite(const float* p, const float* v, float t)
{
    return hermite((f32*) p, (f32*) v, t);
}
