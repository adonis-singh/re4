#ifndef DB_CAM_H
#define DB_CAM_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "joy.h"

// Debug camera tool (game/db_cam.cpp), instance `CamDbg` (0x20 bytes). Driven from CameraMove.
class debugCamera {
public:
    s8 m_menu_sw;           // 0x00  1 = menu open
    s8 m_sel0;            // 0x01  menu page (sel0_menu_tbl)
    s8 m_sel1;         // 0x02  cursor inside the page
    s8 m_sel2;             // 0x03  left/right counter (menuFlag)
    u8 m_timer;          // 0x04  frames until the Z trigger is checked again
    u8 m_draw_timer;     // 0x05  frames left to draw the target cross
    u8 pad_6[2];
    int m_printNo_bak;     // 0x08  pG->debug_mode saved while the tool is open
    s8 m_cam_no;         // 0x0C  camera cut to play (menuCamera)
    s8 m_cam_play;           // 0x0D  cut playback state
    s8 m_key_type;       // 0x0E  camera_type_tbl index
    s8 m_target_type;    // 0x0F  0 EM, 1 OBJ, 2 PL, 3 ORG, 4 OFF
    s8 m_target_save;    // 0x10  m_target_type saved while a tool forces its own (t_sce_at) (PS2 m_target_save)
    u8 pad_11[3];
    u32 pad_bits : 26; // 0x14
    u32 info_disp : 1; // 0x14  bit 0x20: print the camera in player space
    u32 pad_bits2 : 2;
    u32 along_xyz : 1; // 0x14  bit 0x04: dolly along the world axes
    u32 pad_bits3 : 2;
    u8 m_cam_mode;       // 0x18  CAMERA MODE (0 AREA .. 5 BIRD)
    u8 pad_19[3];
    f32 m_move_gain;          // 0x1C  stick gain (CameraRoomInit resets it)

    void move(CAMERA* pCam, JOY* pJoy, int attr);
    void camera_type_00(CAMERA* pCam, JOY* pJoy);
    void camera_type_01(CAMERA* pCam, JOY* pJoy);
    void menu(CAMERA* pCam, JOY* pJoy);
    int menuCamera(JOY* pJoy);
    int menuFlag(JOY* pJoy);
    int menuHitDisp(JOY* pJoy);
    int menuAdjust(JOY* pJoy);
};

extern debugCamera CamDbg;

void CameraDrawTarget(CAMERA* pCam, int attr);
void CameraDebugInformation();
void moveOnPlaneXZ(Vec* src, Vec* dst);
void drawGround(int flag);
int adjust_qFPS(JOY* joy, int x, int y, int flag, int* out);

// shoulder-camera edit buffers of adjust_qFPS (t_camera's tcEdit_camera_qfps copies them into the cut;
// g_local_trans is static in the DOL, the REL imports it by name)
#include "cam_qfps.h"
extern QfpsOfs g_local_ready[2][3];
extern QfpsOfs g_local_trans[2][3];
extern f32 g_local_floor_ratio;

#endif
