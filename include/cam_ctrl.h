#ifndef CAM_CTRL_H
#define CAM_CTRL_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "cam_qfps.h"

class cCamera;
class cModel;
struct CameraCut;

// ---------------------------------------------------------------------------
// Room camera data ("B40x" file). Layout after the 0x10 header:
//   CameraAreaRec[numArea]   0x10 each
//   CameraAreaInfo[numArea]  0x30 each
//   CameraCut[numCut]        0x34 each
//   CameraLerp[numLerp]      0x10 each
// File offsets are relocated to pointers by CameraControl::calcAddr.
// ---------------------------------------------------------------------------

struct CameraAreaInfo {  // hit area
    u8 enable;    // 0x00
    s8 area_no;   // 0x01
    s8 camera_no; // 0x02
    u8 attr;      // 0x03  bit 4 = ?, bit 8 = ?, 0x20 set from 8 by calcAddr, 0x40 = check dir, 0x80 = no light update
    f32 dir;      // 0x04  facing angle the player must have (attr & 0x40)
    u8 attr2;     // 0x08  matched against battle/state attribute
    u8 attr3;     // 0x09  third attribute byte (t_camera TcAdat::attr3; 0xFF = none)
    u8 pad_A[0x20 - 0x0A];
    f32 height;   // 0x20
    f32 base_y;   // 0x24
    s32 num;      // 0x28  polygon vertex count
    Vec* points;  // 0x2C
};

struct CameraAreaRec {  // area -> cut link
    u8 type;              // 0x00  camera type of the linked cut (t_camera tcTypeTbl)
    u8 pad_1[7];
    CameraAreaInfo* area; // 0x08
    CameraCut* cut;       // 0x0C
};

struct CameraCut {
    u8 x0;          // 0x00
    s8 camera_no;   // 0x01
    s8 type;        // 0x02  CameraControl state selector
    u8 flags;       // 0x03  bit 0: aim_ofs valid
    Vec aim_ofs;    // 0x04  added to the player position to get the aim point
    u16* frames;    // 0x10  key frame times
    f32 floor_ratio; // 0x14  shoulder camera floor ratio (cam_qfps setAreaData)
    u8 pad_18[0x20 - 0x18];
    s32 num;        // 0x20  key count
    Vec* pos;       // 0x24
    Vec* at;        // 0x28
    f32* roll;      // 0x2C
    f32* fovy;      // 0x30
};

struct CameraLerp {
    u8 enable;     // 0x00
    s8 area_from;  // 0x01
    s8 cam_from;   // 0x02
    s8 area_to;    // 0x03
    s8 cam_to;     // 0x04
    u8 pad_5[3];
    s32 frame;     // 0x08
    u8 pad_C[4];
};

struct CameraDataHeader {
    char version[4]; // 0x00  "B400".."B404"
    u8 numCut;       // 0x04
    u8 numArea;      // 0x05
    u8 numLerp;      // 0x06
    u8 pad_7[0x10 - 0x07];
};

// Per-attach-camera record registered by other units (only the frame count is used here).
struct AttachCamera {
    u8 parts[5];    // 0x00  motion parts index feeding each channel (0xFF = none): 0/1 pos, 2/3 rot, 4 misc
    u8 type;        // 0x05  0 = off, 1 = follows the model matrix, 2 = own matrix copy (MotionSetCore)
    u8 frame;       // 0x06  (u8)(out[4].y / 100)
    u8 pad_7;
    Mtx* p_mat;      // 0x08  &model->mat or &mat
    Mtx mat;        // 0x0C
    Vec camera_data[5];     // 0x3C  interpolated channels (MotionMoveCore)
    u16 history[5][3]; // 0x78  key history per channel / axis
};

// B-spline rail work used by the Track/RailPan/RailBehind cameras (static CamBSpline, 0x3B8).
// Parametrize() fits the cut's key positions with de_Boor_Cox basis functions (up to 26 keys),
// searchRail() picks the segment/parameter nearest the aim point, BSpline() evaluates the curve.
struct CameraBSpline {
    s32 k;          // 0x000  spline degree (min(2, num - 1))
    f32 t;          // 0x004  curve parameter
    s32 seg;        // 0x008  key index the parameter was searched from
    s32 num;        // 0x00C  key count
    f32 px[26];     // 0x010  control points
    f32 py[26];     // 0x078
    f32 pz[26];     // 0x0E0
    f32 ax[26];     // 0x148
    f32 ay[26];     // 0x1B0
    f32 az[26];     // 0x218
    f32 roll[26];   // 0x280
    f32 fovy[26];   // 0x2E8
    f32 basis[26];  // 0x350  de_Boor_Cox output

    CameraBSpline() {}  // empty: makes CamBSpline emit at its definition (cam_ctrl .bss order)
};

// ---------------------------------------------------------------------------

class CameraInterpolation {
public:
    CameraParam param; // 0x00
    s32 frame;         // 0x20

    void set(int frame, CameraParam* p);
    void move(CameraParam* arg);
};

class CameraSmooth {
public:
    u8 pad_0[0xF8];
    u32 m_flag;         // 0xF8  bit 0 = reinit on next move
    f32 m_ratio;         // 0xFC
    CameraParam param; // 0x100
    u8 pad_120[0x12C - 0x120];

    void init(CameraParam* p);
    void move(CameraParam* arg);
    void setRatio(f32 ratio) { m_ratio = ratio; }
    CameraParam* getCamPtr() { return &param; }
    void setFlag() { m_flag |= 1; }
    void unsetFlag() { m_flag &= ~1; }
};

class CameraControl {
public:
    u8 m_attached_cam_flag_old;                        // 0x00
    u8 m_attach_cam_flag;                        // 0x01
    u8 m_attach_num;                // 0x02
    u8 x3;                        // 0x03
    AttachCamera* m_p_attach[3];  // 0x04
    cModel* m_p_model[3];      // 0x10
    cModel* m_p_attach_model_old;           // 0x1C
    f32 m_scope_zoom;             // 0x20
    f32 m_scope_ang_x;             // 0x24
    u8 be_flag;                  // 0x28  bit 0 = data valid, bit 2 = disabled
    u8 pad_29[3];
    u32 m_system_flag;                 // 0x2C
    u32 m_state_flag;                 // 0x30
    u8 r0;                     // 0x34
    u8 r1;                 // 0x35
    u8 r2;                       // 0x36
    u8 r0_old;                // 0x37
    CameraParam cur;              // 0x38
    u32 counter_58;               // 0x58
    CameraDataHeader* pCamData;       // 0x5C
    Camera camera;                // 0x60
    Mtx prev_mat;                 // 0x158  camera matrix CamStick2World keeps while the cut changes
    u8 pad_188[0x250 - 0x188];
    Camera* m_pExtraCamera;       // 0x250  boss/event camera (em2a/em2b/em2c/em2d); nonzero blocks the fall-check in Check()
    CameraInterpolation m_Inter;   // 0x254
    CameraQuasiFPS m_QuasiFPS;          // 0x278
    u8 m_Free[0x200];          // 0x48C  placement storage for cCamera subclasses
    cCamera* m_pProc;               // 0x68C
    s8 areaNo;                   // 0x690
    s8 areaSuffix;                      // 0x691
    s8 cameraNo;                 // 0x692
    u8 m_cut_attr;                 // 0x693
    CameraAreaRec* area_rec;      // 0x694
    s32 Battle_delay;             // 0x698
    Vec Aim;                      // 0x69C
    Vec upcut_pos;                   // 0x6A8
    Vec upcut_ang;                    // 0x6B4
    Vec upcut_scale;                   // 0x6C0
    f32 m_behind_fovy;                     // 0x6CC
    f32 m_side_play;                     // 0x6D0
    f32 m_back_play;                     // 0x6D4
    f32 m_ang_h_limit;                     // 0x6D8
    f32 m_ang_v_limit;                     // 0x6DC
    s32 m_quick_cnt;                     // 0x6E0
    f32 m_key_speed;                     // 0x6E4
    f32 m_behind_A_ratio;                     // 0x6E8
    Vec campos_ofs;                  // 0x6EC
    Vec target_ofs;                   // 0x6F8

    int HermiteExport(CameraCut* pCdat, u8* buf);
    int IsChangeCamera();
    void Comeback(int);
    void Disable();
    void SetExtraCamera(Camera* p_cam) { m_pExtraCamera = p_cam; }
    void AreaCheckOnOff(int sw);
    u8 AreaNum();
    int CurrentAreaNo();
    int CurrentCameraNo();
    CameraCut* DataSearch(int cameraNo);
    CameraLerp* LerpDataSearch(int srcNo, int srcSuf, int dstNo, int dstSuf);
    CameraDataHeader* calcAddr(CameraDataHeader* head);
    void RoomDataRead(CameraDataHeader* pBuff);
    void CoreDataRead(CameraDataHeader* data);
    void AreaOnOff(int No, int Suffix, int OnOff);
    void SetAreaAttr(int No, int Suffix, u8 attr);
    void UnsetAreaAttr(int No, int Suffix, u8 attr);
    void CutCall(int cutNo);
    void switchCamera(CameraAreaRec* rec);
    void areaHitCheck();
    void roomInit();
    void Check();
    void Move();
    void CalcAim(CameraCut* pCdat);
    f32 getCameraPitch();
    void r0_Wait();
    void r0_Debug();
    void r0_Fix();
    void r0_Pan();
    void r0_Track();
    void r0_RailPan();
    void r0_UpCut();
    void r0_RailBehind();
    void r0_Free();
    void resetCameraAngle();
    f32 getCameraDirection();
    void debugDrawRail(CameraCut* pCdat);
    void UpCutCall(int cutNo, Vec* pos, Vec* ang, Vec* scale, int data_sel);
    void startPushObject();
    void endPushObject();
    void StartLookDownEm(void* pEm);
    void EndLookDownEm();
    void startScope(Vec* campos, Vec* target);
    void endScope();
    void getTrajectory(Vec* p_pos0, Vec* p_pos1);
    void saveScopeParam();
    void loadScopeParam();
    void SetBinocularRange(f32 x_low, f32 x_up, f32 y_low, f32 y_up);
    void HoldBinocular(void* id_a, void* id_b, Vec* pos, Vec* at);
    void LowerBinocular();
    void GetBinocularIDAddr(void** eff_addr, void** uwf_addr);
    void MotionSet(void* motion, int frame, f32 speed);
    int IsMotionSet();
    int IsMotionEnd();
    void setMotionBaseMatPtr(Mtx* p_mat);
    void* getMotionInfoPtr();
    void clearAttachCamera();
    void registAttachCamera(AttachCamera* p_attach, cModel* p_model);
    void deleteAttachCamera(AttachCamera* p_attach, cModel* p_model);
    int getAttachCameraNum() { return m_attach_num; }
    cModel* getAttachModel(cModel* p_model);
    AttachCamera* getAttachCamera(cModel* p_model);
    void checkAttachCamera();

    // Empty ctor/dtor: cam_ctrl's `__static_initialization_and_destruction_0` and the
    // `global constructors/destructors keyed to g_pToolCamData` pair.
    CameraControl() {}
    ~CameraControl() {}
};

extern CameraControl CamCtrl;
extern CameraSmooth CamSmth;
extern void* g_pToolCamData;

int cameraDataVersion(char* verStr);
int cameraHitCheck(Vec* pos, Vec* nrm, Vec* from, Vec* to);
void CameraSetCutData(Camera* pCam, CameraCut* pData);
int areaAttr(CameraAreaInfo* p_area, u8 cut_attr, u8 char_type);
int areaHit(Vec* pPos, CameraAreaInfo* pArea, f32 dir_y);
int area_hit_p3(Vec* pPos, CameraAreaInfo* pArea);
int area_hit_pN(Vec* pPos, CameraAreaInfo* pArea);
void CamCtrlShoulderSetSearchFrame(s16 frame);
void CamCtrlShoulderSetAim(Vec* pos);
void Parametrize(CameraCut* pCdat, CameraBSpline* pB);
void BSpline(CameraBSpline* bs, Camera* cam, int mode);
void searchRail(CameraBSpline* bs, CameraCut* cut, Vec* aim, int mode);


#endif
