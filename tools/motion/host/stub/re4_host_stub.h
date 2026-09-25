// Host (x86-64) stand-in for the game headers that the compiled game sources include
// (src/game/motion.cpp, ik.cpp, the functions extracted from math_sub.cpp / sub2.cpp / model.cpp
// and the SDK's C matrix code). Only what those translation units need: 32-bit fixed-width types,
// Vec/Mtx/Quaternion, the motion structures (include/model.h, motion.h, cam_ctrl.h) and a
// cCoord/cModel with the members the motion / IK / parts-matrix code touches, plus declarations of
// the functions they call. The byte offsets differ from the GameCube layout (64-bit pointers);
// motion.h's offset-based MOTION macro is redirected to the member. Nothing here changes what the
// game functions compute.
#ifndef RE4_HOST_STUB_H
#define RE4_HOST_STUB_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef int BOOL;

#ifndef NULL
#define NULL 0
#endif

struct Vec {
    f32 x, y, z;
};
struct Quaternion {
    f32 x, y, z, w;
};
typedef f32 Mtx[3][4];
typedef f32 (*MtxPtr)[4];

#define PI 3.1415927f
#define PI2 6.2831855f   // math_sub.cpp

// The game's libm (src/lib fdlibm, newlib 1.8.2) sinf/cosf/acosf, built into the helper as re4_*;
// RotMatrix / VecAngle (math_sub.cpp) and the SDK code resolve to them.
extern "C" f32 re4_sinf(f32 x);
extern "C" f32 re4_cosf(f32 x);
extern "C" f32 re4_acosf(f32 x);
#define sinf re4_sinf
#define cosf re4_cosf
#define acosf re4_acosf

// The SDK functions are compiled from their C sources (src/lib mtx.c / vec.c / quat.c) under the
// PS* names the game calls (the GameCube links the paired-single assembly versions).
#define C_MTXIdentity PSMTXIdentity
#define C_MTXCopy PSMTXCopy
#define C_MTXConcat PSMTXConcat
#define C_MTXInverse PSMTXInverse
#define C_MTXTranspose PSMTXTranspose
#define C_MTXTrans PSMTXTrans
#define C_MTXScale PSMTXScale
#define C_MTXRotTrig PSMTXRotTrig
#define C_MTXRotRad PSMTXRotRad
#define C_MTXRotAxisRad PSMTXRotAxisRad
#define C_MTXQuat PSMTXQuat
#define C_MTXMultVec PSMTXMultVec
#define C_MTXMultVecSR PSMTXMultVecSR
#define C_VECAdd PSVECAdd
#define C_VECSubtract PSVECSubtract
#define C_VECScale PSVECScale
#define C_VECNormalize PSVECNormalize
#define C_VECSquareMag PSVECSquareMag
#define C_VECMag PSVECMag
#define C_VECDotProduct PSVECDotProduct
#define C_VECCrossProduct PSVECCrossProduct
#define VECNormalize PSVECNormalize
#define ASSERTMSGLINE(line, cond, msg) ((void) 0)
#define VALID_PTR(p) ((p) != 0)   // model.cpp: "a relocated pointer into main memory" (main_mem.h)
#define ASSERTMSG1(cond, msg, a) ((void) 0)
#define ASSERTMSG2(cond, msg, a, b) ((void) 0)

// ---- include/model.h / motion.h / cam_ctrl.h subset ---------------------------------------------

struct MotionSeqKey {
    u16 frame;
    u8 Se;
    u8 Free;
};

struct MotionData {
    u16 maxFrame;
    u8 nParts;
};

struct AttachCamera {
    u8 parts[5];
    u8 type;
    u8 frame;
    u8 pad_7;
    Mtx* p_mat;
    Mtx mat;
    Vec camera_data[5];
    u16 history[5][3];
};

struct MotionWorkSub {
    MotionData* pMot;
    u32* pHermite_data;   // u32 key pointers: the motion image lives in the low 4 GB (MAP_32BIT)
    u16 Key_hist[2][2][3];
    f32 Mot_frame_max;
    f32 Mot_frame;
    f32 Mot_frame_sav;
    f32 Mot_frame_old;
    u8 Joint_num;
    u8 pad_31[3];
    u8* pJoint_no;
    u16* pJoint_kind;
    u16 Null_pos;
    u16 Null_rot;
    u16 Mot_attr;
    u16 Mot_state;
    u32 Mot_flag;
    Vec Pos;
    Vec Pos_old;
    Vec Pos_dist;
    Vec Pos_world;
    Vec Pos_move_old;
    Vec Ang;
    Vec Ang_old;
    Vec Ang_dist;
    MotionSeqKey* pSeq_top;
    MotionSeqKey Seq;
    MotionSeqKey Seq_old;
    MotionSeqKey Seq_old2;
    f32 Seq_frame;
    u16 Seq_frame_num;
    u8 pad_BE[2];
    f32 Seq_speed;
    u8 Hokan_frame;
    u8 Hokan_cnt;
    u8 pad_C6[2];
    f32 Brate;
    AttachCamera* pAttachCam;
};

struct MotionWork : public MotionWorkSub {
    MotionWorkSub* blend;
    u16* flip;
    u16* blendTbl;
};

typedef u16 HERMITE_KEY[3];

struct MotionParts {
    Vec pos;
    Vec rot;
    Vec scale;
    union {
        u32 x198;
        f32 ikAng;
    };
    HERMITE_KEY hist[6];
    u32 flags;
};

struct HermitePrm {
    f32 frame;
    f32 maxFrame;
    u32 flags;
    u8 type;
    u8 pad_D[3];
    u8* key;
};

class cCoord {
public:
    u32 be_flag;
    Mtx mat;
    Mtx l_mat;
    cCoord* pParent;
    Vec world;
    Vec world_old;
    Vec world_old2;
    Vec pos;
    Vec ang;
    Vec scale;
    Vec r_scale;
    Mtx prevMat;

    void matCalc();
};

class cModel;
typedef cModel cParts;   // the game's cParts is its own class; one class here for models and parts

class cModel : public cCoord {
public:
    cParts* pList;   // parts: next parts of the model's list; model: first parts
    u8 id;
    u8 nParts;
    u8 kindid;
    MotionWork Motion;
    Mtx lt_inv_mat;   // cParts members (include/model.h)
    MotionParts motParts;
    f32 length;
    Mtx ik_inv_mat;
    Vec up_vector;
    Vec ik_dir;
    Vec inv_offset;

    HERMITE_KEY* getKeyHist() { return motParts.hist; }
    cParts* getPartsPtr(int no);
    void partsMatCalc();
    void partsWorldCalc();
    void matBlend(f32 rate);
};

class cMotModel : public cModel {
public:
};

// ik.cpp reads the model as a cEm (collision flags, status bits)
struct AtariStub {
    u16 m_flag;
};
class cEm : public cModel {
public:
    AtariStub atari;
    u32 status;
    int checkStatus(int stat) { return (status >> stat) & 1; }
};
#define EM_STATUS_IK_OFF 3

#define MOTION(m) (&((cMotModel*) (m))->Motion)

// ---- globals the files reference ----------------------------------------------------------------

struct Global {
    u32 Debug_flg[4];
    u32 System_flg[4];
    f32 Speed;          // motion frame step per game frame (MotionSequenceCtrl: Seq_speed * Speed)
};
extern Global* pG;
extern const Vec vecZero;   // main.h / main.cpp
// include/global.h flag accessors (the host keeps every flag clear): bit `no` of a u32 flag array,
// MSB first, as the game's FlagChk macro.
enum { DBG_TEST_MODE = 0, DBG_ERROR_CK = 105, SYS_INVISIBLE = 8 };
#define FlagChk(base, no) (((u32*) (base))[(no) >> 5] & (0x80000000u >> ((no) & 31)))
#define DbgFlagChk(g, n) FlagChk((g)->Debug_flg, n)
#define SysFlagChk(g, n) FlagChk((g)->System_flg, n)
extern cModel* pPL;

struct Log {
    void err(int a, int b, const char* fmt, ...);
};
extern Log* pLog;

struct CamCtrlStub {
    void registAttachCamera(AttachCamera* cam, cModel* m);
    void deleteAttachCamera(AttachCamera* cam, cModel* m);
};
extern CamCtrlStub CamCtrl;

struct SatMgrStub {
    f32 getFloor(Vec* pos, u32* attr, f32 up, f32 down, int flag);   // include/atari.h cSatMgr::getFloor
};
extern SatMgrStub SatMgr;

// ---- functions the files call -------------------------------------------------------------------

extern "C" {
void memclr_asm(void* dst, u32 n);
void OSReport(const char* fmt, ...);
void PSVECAdd(const Vec* a, const Vec* b, Vec* out);
void PSVECSubtract(const Vec* a, const Vec* b, Vec* out);
void PSVECScale(const Vec* a, Vec* out, f32 s);
void PSVECNormalize(const Vec* a, Vec* out);
f32 PSVECSquareMag(const Vec* a);
f32 PSVECMag(const Vec* a);
f32 PSVECDotProduct(const Vec* a, const Vec* b);
void PSVECCrossProduct(const Vec* a, const Vec* b, Vec* out);
void PSMTXIdentity(Mtx m);
void PSMTXCopy(const Mtx src, Mtx dst);
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab);
u32 PSMTXInverse(const Mtx src, Mtx inv);
void PSMTXTranspose(const Mtx src, Mtx dst);
void PSMTXTrans(Mtx m, f32 x, f32 y, f32 z);
void PSMTXScale(Mtx m, f32 x, f32 y, f32 z);
void PSMTXRotTrig(Mtx m, char axis, f32 s, f32 c);
void PSMTXRotRad(Mtx m, char axis, f32 rad);
void PSMTXRotAxisRad(Mtx m, const Vec* axis, f32 rad);
void PSMTXQuat(Mtx m, const Quaternion* q);
void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst);
void PSMTXMultVecSR(const Mtx m, const Vec* src, Vec* dst);
void C_QUATMtx(Quaternion* q, const Mtx m);
void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t);
void IKInit(cModel* m, MotionWorkSub* w);
void InverseKinematics(cModel* m, int flag);
void ikCalc(cParts* root, cParts* joint, cParts* eff);
void cModel_matBlend(cModel* m, f32 rate);
int HermiteInterpolation(HermitePrm* prm, Vec* out, u16* hist);
int Fcc_next_axis_addr(int type, int n);
void PartsWorldPosCalc(cModel* m);
void MotionBlendOff(cModel* m);
void MotionPause(cModel* m);
void MotionClear(cModel* m, int flag);
struct CAMERA;   // camera.h; MotionMove / MotionMoveCore take a CAMERA* the host passes as NULL
u32 MotionMove(cModel* m, CAMERA* pCamera);
u16 MotionMoveSub(cModel* m, MotionWorkSub* w);
void MotionMoveCore(cModel* m, MotionWorkSub* w, CAMERA* pCamera);
void MotionHokan(cModel* m, MotionWorkSub* w);
void MotionGetSpeed(cModel* m, MotionWorkSub* w, int flag, Vec* pos, Vec* rot);
void MotionAddSpeed(cModel* m, MotionWorkSub* w, Vec* pos, Vec* rot);
void MotionGetPosition(cModel* m, Vec* pos, Vec* rot);
u16 MotionSequenceCtrl(MotionWorkSub* w);
u16 FcvGetMaxFrame(u16* data);
f32 MotionGetMaxFrame(MotionWorkSub* w);
f32 MotionGetCurrentFrame(MotionWorkSub* w);
int MotionCheckCrossFrame(MotionWorkSub* w, f32 frame);
int MotionGetState(cModel* m);
void eprintf(int x, int y, int a, int b, const char* fmt, ...);
}
void RotMatrix(Mtx m, Vec* ang);
void TransMatrix(Mtx m, Vec* pos);
void ScaleMatrix(Mtx m, Vec* scale);
void SetOrientationZX(Vec* z, Vec* x, Mtx m);
void SetOrientationZY(Vec* z, Vec* y, Mtx m);
f32 VecAngle(Vec* a, Vec* b);
void VecRadLimit(Vec* v);
void VecLinearCombination(Vec* a, f32 s, Vec* b, f32 t, Vec* out);   // include/math_sub.h
f32 LIMIT_ANGLE(f32 x);
f32 SQRTF(f32 x);
f32 hermite(f32* p, f32* v, f32 t);
f32 RootSumSquare3(Vec* v);
f32 GetDistance3(Vec* a, Vec* b);
void MotionSetCore(cModel* m, void* w, void* data, void* seq, int hokan, int flags, int frame);   // include/motion.h

// include/model.h cCoord::matCalc: l_mat from ang / pos / scale, copied to mat.
inline void cCoord::matCalc()
{
    RotMatrix(l_mat, &ang);
    TransMatrix(l_mat, &pos);
    ScaleMatrix(l_mat, &scale);
    PSMTXCopy(l_mat, mat);
}

#endif
