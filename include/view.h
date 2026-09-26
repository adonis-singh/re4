#ifndef VIEW_H
#define VIEW_H

#include "types.h"
#include "vec.h"
#include "camera.h"

// View frustum: 6 plane normals and 8 corner points (near 0..3, far 4..7), 0xC0 bytes.
struct ViewFrustum {
    Vec normal[6];  // 0x00
    Vec point[8];   // 0x48
    Vec xA8[2];     // 0xA8
};

// Bounding sphere of the frustum (0x14 bytes)
struct ViewSphere {
    Vec center;  // 0x00
    f32 radius;  // 0x0C
    f32 x10;     // 0x10
};

// Camera view volume (game/view.cpp `View`, 0x3A4 bytes).
class VIEW {
public:
    u8 pad_0[4];
    CAMERA* _p_camera;             // 0x04
    f32 _aspect;               // 0x08
    f32 _fovy;                 // 0x0C
    f32 _zfar;                 // 0x10
    f32 _znear;                // 0x14
    f32 _old_fovy;              // 0x18
    f32 _old_zfar;              // 0x1C
    u8 pad_20[0x54 - 0x20];
    ViewFrustum local;        // 0x054  half-width frustum, camera space
    ViewFrustum world;        // 0x114  half-width frustum, world space
    ViewFrustum localFull;    // 0x1D4  full frustum, camera space
    ViewFrustum worldFull;    // 0x294  full frustum, world space
    u8 pad_354[0x37C - 0x354];
    ViewSphere _l_sphere_outer;        // 0x37C  camera space
    ViewSphere _sphere_outer;   // 0x390  world space

    void gameInit(CAMERA* p_camera);
    void roomInit();
    void init();
    void move();
    void setFarPlane(f32 zfar);
    void initPerspective(f32 fovy, f32 aspect, f32 znear, f32 zfar);
    void orientation();
};

extern VIEW View;
extern f32 ZNEAR;
extern f32 ZFAR;
// orthographic projection bounds (game/view.cpp .sbss, after ZFAR)
extern f32 ORTHO_T;
extern f32 ORTHO_B;
extern f32 ORTHO_L;
extern f32 ORTHO_R;

#endif
