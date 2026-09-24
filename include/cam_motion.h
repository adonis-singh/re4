#ifndef CAM_MOTION_H
#define CAM_MOTION_H

#include "types.h"
#include "vec.h"
#include "cam_extra.h"
#include "motion.h"

// Camera key-frame playback work (same layout as the front of MotionWork, 0xD0 bytes).
struct CameraMotionWork {
    MotionData* data;   // 0x00
    u32* keyTbl;        // 0x04
    u16 hist[4][3];     // 0x08  key history per motion parts (pos, at, roll, fovy)
    f32 maxFrame;       // 0x20
    f32 frame;          // 0x24
    u8 pad_28[8];
    u8 nParts;          // 0x30
    u8 pad_31[3];
    u8* partsNo;        // 0x34
    u16* partsInfo;     // 0x38
    u8 pad_3C[4];
    u16 flags;          // 0x40  bit2: loop, bit3: pause
    u8 pad_42[2];
    u32 state;          // 0x44  CameraSequenceCtrl result: 1 looped, 4 end
    u8 pad_48[0xC4 - 0x48];
    u8 hokan;           // 0xC4
    u8 pad_C5[0xD0 - 0xC5];
};

// Keyframed camera motion (game/cam_motion.cpp). Derives from cCamera (vptr at 0xF8).
class CameraMotion : public cCamera {
public:
    s32 m_state;                       // 0xFC  1 when the motion has finished
    CameraMotionWork m_info;         // 0x100 motion work (getMotionInfoPtr)
    Mtx* m_p_base_mat;                 // 0x1D0

    CameraMotion(void* data, int hokan, int flags, f32 frame);
    virtual ~CameraMotion();
    virtual void move();
    s32 getState() { return m_state; }
    CameraMotionWork* getInfoPtr() { return &m_info; }
    void setBaseMatPtr(Mtx* p_mat) { m_p_base_mat = p_mat; }
};

extern "C" u32 CameraSequenceCtrl(CameraMotionWork* w);

#endif
