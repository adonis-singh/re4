#ifndef CAM_EXTRA_H
#define CAM_EXTRA_H

#include "types.h"
#include "vec.h"
#include "camera.h"

class cModel;
class cParts;

// Placement new used to construct camera objects inside CameraControl::extra_buf.
#ifndef PLACEMENT_NEW_DEFINED
#define PLACEMENT_NEW_DEFINED
inline void* operator new(unsigned int, void* p) { return p; }
#endif

// Base class of the special-purpose cameras (game/cam_extra.cpp, cam_motion.cpp).
// GNU v2 layout: Camera data first, then the vtable pointer at 0xF8 (size 0xFC).
//   vtable slot 0 (0x08): virtual destructor
//   vtable slot 1 (0x10): move()
class cCamera : public Camera {
public:
    virtual ~cCamera() {}
    virtual void move() = 0;
    void operator delete(void*, unsigned int) {}
};

// Screen-id (widget) application base: init/move/quit driven by the owning camera.
class IDApplication {
public:
    virtual ~IDApplication() {}
    virtual void init(void* p) {}
    virtual void move(void* p) {}
    virtual void quit(void* p) {}
    void operator delete(void*, unsigned int) {}
};

// Class declaration order below = the DOL's .text order of cam_extra.cpp. Vtables are emitted in
// reverse declaration order at finish_file (LookDownEm, LookAt, PushObject, Binocular, IdBinocular,
// Scope, IdScope, AttachedToMotion, IDApplication, cCamera in the DOL's .rodata). Do not reorder.

class CameraAttachedToMotion : public cCamera {
public:
    cModel* m_pModel;  // 0xFC

    CameraAttachedToMotion(cModel* model);
    virtual ~CameraAttachedToMotion();
    virtual void move();
};

// Scope reticle ids (IdSys unit 0x25).
class IdScope : public IDApplication {
public:
    s32 m_pos_time_sav;  // 0x04
    s32 m_size_time_sav;  // 0x08

    virtual void init(void* size);
    virtual void move(void* zoom);
    virtual void quit(void* p);
    void save(void*);
    void load(void*);
};

// Filter0a focus blur animation.
struct FocusAnimation {
    u8 m_anim_on;    // 0x00
    s32 m_counter;   // 0x04
    f32 m_focus_frame;   // 0x08
    u8 m_alpha_max;    // 0x0C

    void init(int mask_id);
    void move(int anim_flag);
    void quit();
    void clear();
};

class CameraScope : public cCamera {
public:
    Vec pos_ofs;      // 0x0FC
    Vec m_rad;          // 0x108
    f32 angle_x;      // 0x114
    u8 pad_118[8];
    f32 angle_min;    // 0x120
    u8 pad_124[8];
    f32 angle_max;    // 0x12C
    u8 pad_130[8];
    f32 m_zoom_ratio;         // 0x138
    Vec m_rnd;         // 0x13C  (x, y used; z = x144)
    u8 pad_148[13];
    u8 type;          // 0x155
    u8 pad_156[2];
    IdScope m_id;       // 0x158
    FocusAnimation m_focus;  // 0x164

    CameraScope(Vec* pos, Vec* at);
    virtual ~CameraScope();
    virtual void move();
    void setParam(f32 zoom_ratio, f32 x_radian);
    void getParam(f32* zoom_ratio, f32* x_radian);
    void getTrajectory(Vec* p_pos0, Vec* p_pos1)
    {
        *p_pos0 = param.pos;
        *p_pos1 = param.at;
    }
};

// Binocular ids (IdSys unit 0x27).
class IdBinocular : public IDApplication {
public:
    Vec m_pos0_L;    // 0x04  screen positions of the heading scale ends
    Vec m_pos0_R;    // 0x10
    Vec m_pos0_C;    // 0x1C
    f32 m_fovy_old;    // 0x28  fovy of the previous frame
    Vec m_meter_pos0;   // 0x2C  zoom gauge origin
    f32 m_meter_h0;   // 0x38
    f32 m_meter_w0;   // 0x3C

    void init(Camera* cam, void* tex, void* data);
    virtual void move(void* cam);
    virtual void quit(void* cam);
    void cutin(void* arg);
};

class CameraBinocular : public cCamera {
public:
    s32 m_flag;          // 0x0FC
    Vec m_rad;         // 0x100  view angles (x yaw, y pitch), clamped between m_rad_low / m_rad_up
    Vec m_rad_low;     // 0x10C  setRange lower limits (default -60 deg)
    Vec m_rad_up;      // 0x118  setRange upper limits (default 60 deg)
    f32 m_zoom_ratio;  // 0x124  zoom 0..1 (fovy gain = 1 - 0.9 * zoom)
    IdBinocular m_id;    // 0x128
    FocusAnimation m_focus;  // 0x168
    Vec m_campos;     // 0x178
    Vec m_target;      // 0x184
    Vec m_up_vec;      // 0x190
    void* id_a;        // 0x19C
    void* id_b;        // 0x1A0

    CameraBinocular(Vec* pos, Vec* at, void* id_a, void* id_b);
    virtual ~CameraBinocular();
    virtual void move();
    void setRange(f32 x_low, f32 x_up, f32 y_low, f32 y_up);
    void* getEffAddr() { return id_a; }
    void* getUwfAddr() { return id_b; }
};

class CameraPushObject : public cCamera {
public:
    CameraPushObject();
    virtual ~CameraPushObject();
    virtual void move();
};

class CameraLookAt : public cCamera {
public:
    u8 pad_FC[4];
    cParts* m_target_parts;  // 0x100  hand parts looked at

    CameraLookAt(Camera* cam);
    virtual ~CameraLookAt();
    virtual void move();
};

class CameraLookDownEm : public cCamera {
public:
    cParts* m_target_parts;  // 0xFC

    CameraLookDownEm(void* em, Vec* ofs);
    virtual ~CameraLookDownEm();
    virtual void move();
};

#endif
