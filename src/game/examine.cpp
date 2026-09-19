// game/examine: the item examine screen (D:/Bio4/Prog/examine.cpp). ItemExamine takes the item
// or weapon model out of the inventory/merchant screen, centres it, spins it (or lets the player
// rotate/zoom it in the treasure viewer mode 2), draws it over a blurred copy of the frame buffer
// (store/render through the OT) with its own lights and camera, and shows the weapon level bars
// (idSet). exam_info[] holds per-item view rotation, scale, light set and spin axis flags; modes:
// 0 inventory, 1 merchant (ext table), 2 treasure viewer.
#include "types.h"
#include "light.h"
#include "atari.h"
#include "map_obj.h"
#include "widget.h"
#include "gx.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"
#include "global.h"
#include "item.h"
#include "math_sub.h"
#include "esp.h"
#include "est.h"
#include "sscrn.h"
#include "trans.h"
#include "examine.h"

// Item examine view: renders the item model through the item camera into a temporary buffer
// and pastes it back as a screen-sized quad (mode 0 in game, 1 sub screen, 2 puzzle).

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

extern f32 ZNEAR;
extern f32 ZFAR;
extern IDSystem IdSub;  // game/sscrn.cpp
int GetDrawTmpBufType();

extern "C" {
float tanf(float);
void getEFB();
static void gxDraw(f32 x, f32 y, f32 z, f32 alpha, void* buf);
void drawBuffer();
void store();
void render();
ExamInfo* examInfo(int id, int ext);
}

f32 cap_dist_min = 8000.0f;
f32 cap_dist_max = 10000.0f;
static f32 g_rad_x = 0.0f;
f32 cap_xrad_max = 0.3926991f;
static f32 cap_xrad_min = -1.0471976f;

static ExamInfo exam_info[225] = {
    { 0x00, 0, { 60.0f, 0.0f, 0.0f }, 1.1f, 4, 0, 0 },
    { 0x01, 0, { 0.0f, 0.0f, 20.0f }, 1.1f, 4, 0, 0 },
    { 0x02, 0, { 0.0f, 0.0f, 20.0f }, 1.1f, 4, 0, 0 },
    { 0x03, 0, { 0.0f, 0.0f, 4.0f }, 2.2f, 2, 0, 0 },
    { 0x04, 0, { 60.0f, 0.0f, 0.0f }, 1.0f, 4, 0, 0 },
    { 0x05, 0, { 0.0f, 0.0f, 40.0f }, 1.1f, 4, 0, 0 },
    { 0x06, 0, { 30.0f, 0.0f, 0.0f }, 1.7f, 4, 1, 0 },
    { 0x07, 0, { 60.0f, 0.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0x08, 0, { 0.0f, 0.0f, 30.0f }, 0.8f, 0, 0, 0 },
    { 0x09, 0, { 0.0f, 0.0f, 30.0f }, 0.8f, 0, 0, 0 },
    { 0x0A, 0, { 0.0f, 0.0f, 30.0f }, 0.8f, 0, 0, 0 },
    { 0x0B, 0, { 10.0f, 0.0f, 0.0f }, 1.0f, 4, 1, 1 },
    { 0x0C, 0, { -30.0f, 0.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0x0D, 0, { -20.0f, 0.0f, 20.0f }, 1.7f, 4, 0, 0 },
    { 0x0E, 0, { 0.0f, 0.0f, 20.0f }, 1.1f, 4, 0, 0 },
    { 0x0F, 0, { 60.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0x10, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 1, 0, 0 },
    { 0x11, 0, { 90.0f, -160.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x12, 0, { 0.0f, 0.0f, 0.0f }, 1.2f, 1, 0, 0 },
    { 0x13, 0, { 0.0f, 0.0f, 0.0f }, 1.2f, 1, 0, 0 },
    { 0x14, 0, { 0.0f, 0.0f, 0.0f }, 1.2f, 1, 0, 0 },
    { 0x15, 0, { 0.0f, 0.0f, 0.0f }, 1.2f, 1, 0, 0 },
    { 0x16, 0, { 0.0f, 0.0f, 0.0f }, 1.2f, 1, 0, 0 },
    { 0x17, 0, { 0.0f, 0.0f, 5.0f }, 2.5f, 2, 0, 0 },
    { 0x18, 0, { 60.0f, 0.0f, 0.0f }, 1.1f, 4, 0, 0 },
    { 0x19, 0, { 30.0f, 0.0f, 0.0f }, 1.7f, 4, 1, 0 },
    { 0x1A, 0, { 50.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x1C, 0, { 30.0f, 0.0f, 0.0f }, 1.7f, 4, 1, 0 },
    { 0x1D, 0, { 50.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x1E, 0, { -20.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x1F, 0, { -20.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x20, 0, { 60.0f, 0.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0x21, 0, { -30.0f, 0.0f, -20.0f }, 1.4f, 0, 0, 0 },
    { 0x22, 0, { -30.0f, 0.0f, -14.0f }, 1.8f, 0, 0, 0 },
    { 0x23, 0, { -30.0f, 0.0f, -20.0f }, 1.5f, 0, 0, 0 },
    { 0x24, 0, { -30.0f, 0.0f, -14.0f }, 1.8f, 0, 0, 0 },
    { 0x25, 0, { -30.0f, 0.0f, -14.0f }, 1.8f, 0, 0, 0 },
    { 0x26, 0, { 0.0f, 0.0f, 0.0f }, 2.3f, 2, 0, 0 },
    { 0x27, 0, { -30.0f, 0.0f, -14.0f }, 1.5f, 0, 0, 0 },
    { 0x28, 0, { 0.0f, 0.0f, -8.0f }, 1.8f, 0, 0, 0 },
    { 0x29, 0, { 70.0f, 0.0f, 20.0f }, 2.1f, 4, 0, 0 },
    { 0x2A, 0, { 70.0f, 0.0f, 20.0f }, 1.5f, 4, 0, 0 },
    { 0x2B, 0, { 70.0f, 0.0f, 14.0f }, 1.8f, 4, 0, 0 },
    { 0x2C, 0, { -30.0f, 0.0f, 4.0f }, 2.4f, 2, 0, 0 },
    { 0x2D, 0, { 0.0f, 0.0f, 5.0f }, 2.1f, 2, 0, 0 },
    { 0x2E, 0, { 0.0f, 0.0f, -5.0f }, 2.4f, 2, 0, 0 },
    { 0x2F, 0, { 30.0f, 0.0f, -5.0f }, 2.4f, 2, 0, 0 },
    { 0x30, 0, { -30.0f, 0.0f, 12.0f }, 1.6f, 4, 0, 0 },
    { 0x31, 0, { -30.0f, 0.0f, 12.0f }, 2.0f, 4, 0, 0 },
    { 0x32, 0, { -25.0f, 0.0f, 12.0f }, 2.0f, 4, 0, 0 },
    { 0x33, 0, { -30.0f, 0.0f, 12.0f }, 1.8f, 4, 0, 0 },
    { 0x34, 0, { -30.0f, 0.0f, -10.0f }, 2.4f, 2, 0, 0 },
    { 0x35, 0, { 0.0f, 0.0f, 5.0f }, 2.5f, 2, 0, 0 },
    { 0x36, 0, { 0.0f, 0.0f, 0.0f }, 2.2f, 2, 0, 0 },
    { 0x37, 0, { -30.0f, 0.0f, -14.0f }, 2.0f, 0, 0, 0 },
    { 0x38, 0, { -20.0f, 0.0f, 20.0f }, 1.7f, 4, 0, 0 },
    { 0x39, 0, { -20.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x3A, 0, { 70.0f, -30.0f, 0.0f }, 1.25f, 4, 0, 0 },
    { 0x3B, 0, { 60.0f, 0.0f, 20.0f }, 1.3f, 4, 0, 0 },
    { 0x3C, 0, { -10.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x3D, 0, { 0.0f, 0.0f, 0.0f }, 0.8f, 4, 0, 0 },
    { 0x3E, 0, { 15.0f, 0.0f, 7.0f }, 2.3f, 4, 0, 0 },
    { 0x3F, 0, { 0.0f, 0.0f, 0.0f }, 1.4f, 2, 0, 0 },
    { 0x40, 0, { 0.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x41, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0x42, 0, { 0.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x43, 0, { 0.0f, 0.0f, 0.0f }, 1.4f, 2, 0, 0 },
    { 0x44, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0x45, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0x46, 0, { 35.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x47, 0, { 30.0f, 0.0f, 0.0f }, 1.3f, 4, 1, 1 },
    { 0x51, 0, { 30.0f, 0.0f, -5.0f }, 2.4f, 2, 0, 0 },
    { 0x52, 0, { 65.0f, 0.0f, 0.0f }, 1.8f, 2, 0, 0 },
    { 0x56, 0, { 10.0f, 0.0f, 0.0f }, 1.3f, 3, 0, 0 },
    { 0x57, 0, { 80.0f, 0.0f, 0.0f }, 0.8f, 3, 0, 0 },
    { 0x58, 0, { 40.0f, 0.0f, 0.0f }, 1.6f, 4, 0, 0 },
    { 0x59, 0, { 35.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0x5A, 0, { 30.0f, 0.0f, 0.0f }, 1.6f, 4, 0, 0 },
    { 0x5B, 0, { 90.0f, 0.0f, 0.0f }, 1.7f, 4, 0, 0 },
    { 0x5C, 0, { -30.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x5D, 0, { -20.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x5E, 0, { 5.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x5F, 0, { 90.0f, 0.0f, 60.0f }, 0.8f, 0, 0, 0 },
    { 0x60, 0, { 90.0f, 0.0f, 60.0f }, 0.8f, 0, 0, 0 },
    { 0x61, 0, { 90.0f, 0.0f, 60.0f }, 0.8f, 0, 0, 0 },
    { 0x62, 0, { 5.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x63, 0, { 5.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x64, 0, { 5.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x65, 0, { 5.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x66, 0, { 5.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x67, 0, { 5.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x68, 0, { 5.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x69, 0, { 70.0f, -30.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x6A, 0, { 50.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x6B, 0, { 0.0f, 0.0f, -5.0f }, 2.4f, 2, 0, 0 },
    { 0x6C, 0, { 30.0f, 0.0f, -5.0f }, 2.4f, 2, 0, 0 },
    { 0x6D, 0, { 0.0f, 0.0f, 30.0f }, 1.1f, 4, 0, 0 },
    { 0x6E, 0, { 15.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x6F, 0, { 15.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x70, 0, { 0.0f, 50.0f, -65.0f }, 2.0f, 4, 0, 0 },
    { 0x71, 0, { 120.0f, -40.0f, -120.0f }, 1.25f, 4, 0, 0 },
    { 0x72, 0, { 0.0f, 0.0f, -20.0f }, 1.8f, 4, 0, 0 },
    { 0x73, 0, { -30.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x74, 0, { -30.0f, -30.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0x75, 0, { -30.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x77, 0, { 80.0f, 0.0f, 0.0f }, 0.9f, 4, 0, 0 },
    { 0x78, 0, { 20.0f, 0.0f, 0.0f }, 0.8f, 4, 0, 0 },
    { 0x79, 0, { 30.0f, 0.0f, 0.0f }, 1.1f, 4, 0, 0 },
    { 0x7A, 0, { 70.0f, -30.0f, 0.0f }, 1.45f, 4, 0, 0 },
    { 0x7B, 0, { 90.0f, -30.0f, -60.0f }, 1.3f, 4, 0, 0 },
    { 0x80, 0, { -180.0f, 30.0f, 110.0f }, 2.3f, 4, 0, 0 },
    { 0x81, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0x82, 0, { 60.0f, -30.0f, 0.0f }, 1.6f, 4, 0, 0 },
    { 0x83, 0, { -30.0f, -30.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0x84, 0, { -30.0f, -30.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0x85, 0, { -20.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x86, 0, { -20.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x87, 0, { -20.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0x88, 0, { 0.0f, 0.0f, 0.0f }, 1.6f, 4, 0, 0 },
    { 0x89, 0, { 40.0f, 0.0f, 0.0f }, 1.6f, 4, 0, 0 },
    { 0x8A, 0, { 35.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0x8B, 0, { 60.0f, 0.0f, 20.0f }, 1.3f, 0, 0, 0 },
    { 0x8C, 0, { 60.0f, 0.0f, 20.0f }, 1.3f, 4, 0, 0 },
    { 0x8D, 0, { -30.0f, 0.0f, 10.0f }, 1.1f, 1, 0, 0 },
    { 0x8E, 0, { 30.0f, 0.0f, 10.0f }, 1.0f, 4, 0, 0 },
    { 0x8F, 0, { 40.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x90, 0, { 10.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0x91, 0, { 70.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0x92, 0, { -30.0f, -30.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0x93, 0, { 15.0f, 0.0f, 0.0f }, 2.6f, 4, 1, 1 },
    { 0x94, 0, { -30.0f, 0.0f, 4.0f }, 2.4f, 2, 0, 0 },
    { 0x95, 0, { 0.0f, 0.0f, 0.0f }, 1.3f, 0, 0, 0 },
    { 0x96, 0, { 30.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0x97, 0, { 0.0f, 0.0f, 0.0f }, 2.2f, 2, 0, 0 },
    { 0x98, 0, { 70.0f, 0.0f, 0.0f }, 1.4f, 4, 0, 0 },
    { 0x99, 0, { 0.0f, 0.0f, -5.0f }, 2.2f, 2, 0, 0 },
    { 0x9A, 0, { -12.0f, 0.0f, 0.0f }, 2.2f, 4, 0, 0 },
    { 0x9B, 0, { 70.0f, 0.0f, 0.0f }, 1.1f, 4, 0, 0 },
    { 0x9C, 0, { 80.0f, 0.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0x9D, 0, { -12.0f, 0.0f, 0.0f }, 2.15f, 4, 0, 0 },
    { 0x9E, 0, { -12.0f, 0.0f, 0.0f }, 2.2f, 4, 0, 0 },
    { 0x9F, 0, { -12.0f, 0.0f, 0.0f }, 2.2f, 4, 0, 0 },
    { 0xA0, 0, { 40.0f, 0.0f, 0.0f }, 1.2f, 4, 0, 0 },
    { 0xA1, 0, { 80.0f, 0.0f, 0.0f }, 0.9f, 0, 0, 0 },
    { 0xA2, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xA3, 0, { 75.0f, 0.0f, 20.0f }, 1.4f, 4, 0, 0 },
    { 0xA4, 0, { 60.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0xA5, 0, { 60.0f, 0.0f, 0.0f }, 1.3f, 4, 0, 0 },
    { 0xA6, 0, { 60.0f, 0.0f, 0.0f }, 1.6f, 4, 0, 0 },
    { 0xA7, 0, { 90.0f, 0.0f, -70.0f }, 1.3f, 4, 0, 0 },
    { 0xA8, 0, { 0.0f, 0.0f, 0.0f }, 1.2f, 1, 0, 0 },
    { 0xA9, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xAA, 0, { -20.0f, 0.0f, 0.0f }, 1.0f, 4, 0, 0 },
    { 0xAB, 0, { 0.0f, 0.0f, 0.0f }, 2.2f, 2, 0, 0 },
    { 0xAC, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xAD, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xAE, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xAF, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB0, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB1, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB2, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB3, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB4, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB5, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB6, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB7, 0, { 0.0f, 0.0f, 0.0f }, 1.0f, 0, 0, 0 },
    { 0xB8, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0xB9, 0, { 100.0f, -10.0f, -130.0f }, 0.8f, 0, 0, 0 },
    { 0xBA, 0, { 100.0f, -10.0f, -130.0f }, 0.8f, 0, 0, 0 },
    { 0xBB, 0, { 100.0f, -10.0f, -130.0f }, 0.8f, 0, 0, 0 },
    { 0xBC, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0xBD, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0xBE, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0xBF, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0xC0, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0xC1, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0xC2, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 1, 1 },
    { 0xC3, 0, { 0.0f, -40.0f, -60.0f }, 1.3f, 4, 0, 0 },
    { 0xC4, 0, { -180.0f, 30.0f, 110.0f }, 2.3f, 4, 0, 0 },
    { 0xC6, 0, { 60.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0xC7, 0, { 10.0f, 0.0f, 30.0f }, 0.8f, 4, 0, 0 },
    { 0xC8, 0, { 10.0f, 0.0f, 30.0f }, 0.8f, 4, 0, 0 },
    { 0xC9, 0, { 10.0f, 0.0f, 30.0f }, 0.8f, 4, 0, 0 },
    { 0xCA, 0, { 60.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0xCB, 0, { 60.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0xCC, 0, { 60.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0xCD, 0, { 60.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0xCE, 0, { 60.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0xCF, 0, { 60.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0xD0, 0, { 60.0f, 0.0f, 0.0f }, 1.9f, 4, 0, 0 },
    { 0xD1, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0xD2, 0, { 90.0f, 10.0f, 90.0f }, 0.8f, 4, 0, 0 },
    { 0xD3, 0, { 80.0f, 0.0f, 10.0f }, 0.8f, 4, 0, 0 },
    { 0xD4, 0, { -80.0f, 0.0f, 170.0f }, 0.8f, 4, 0, 0 },
    { 0xD5, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0xD6, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0xD7, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0xD8, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0xD9, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0xDA, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0xDB, 0, { 0.0f, 0.0f, 0.0f }, 1.5f, 4, 0, 0 },
    { 0xDC, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xDD, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xDE, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xDF, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE0, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE1, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE2, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE3, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE4, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE5, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE6, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE7, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE8, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xE9, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xEA, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xEB, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xEC, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xED, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xEE, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xEF, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xF0, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xF1, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xF2, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
    { 0xF3, 0, { 0.0f, 0.0f, 0.0f }, 20.0f, 0, 0, 0 },
};
ExamInfo exam_info_ext[2] = {
    { 0x59, 0, { -5.0f, 0.0f, 0.0f }, 2.1f, 4, 0, 0 },
    { 0x8A, 0, { -5.0f, 0.0f, 0.0f }, 2.1f, 4, 0, 0 },
};

ItemExamine itemExam;
Camera itemCamera;
static void* local_buff;

// Copies the frame buffer (half size, RGBA8) into local_buff for the examine background.
void getEFB()
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W, SCR_H);
    GXSetTexCopyDst(SCR_W >> 1, SCR_H >> 1, 6, 1);
    GXCopyTex(local_buff, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

// Draws the stored screen texture as a full-screen quad at depth z with the given alpha.
static void gxDraw(f32 x, f32 y, f32 z, f32 alpha, void* buf)
{
    GXTexObj tex;

    GXInitTexObj(&tex, buf, SCR_W >> 1, SCR_H >> 1, 6, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x + 0.0f, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(x + (f32) SCR_W, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(x + (f32) SCR_W, y + (f32) SCR_H, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(x + 0.0f, y + (f32) SCR_H, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(0.0f, 1.0f);
}

// Draws the stored frame back as the examine background (ortho, far Z, alpha blend, colour only).
void drawBuffer()
{
    Mtx44 proj;
    Mtx mv;
    static int operation_flag = 1;
    static int z = 65530;
    static u8 alpha = 0xFF;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetNumTevStages(1);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetChanCtrl(0, 1, 1, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 1, 1, 0, 2, 1);
    GXSetNumChans(1);
    GXSetTevOrder(0, 0, 0, 4);
    if (operation_flag) {
        GXSetTevColorIn(0, 15, 15, 15, 8);
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(0, 7, 7, 7, 5);
        GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    } else {
        GXSetTevOp(0, 3);
    }
    GXSetZMode(1, 7, 1);
    gxDraw(0.0f, 0.0f, (f32) z, (f32) alpha, local_buff);
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

// OT callback (type 0x12) queued by setup: grabs the frame into draw temp buffer 0x10.
void store()
{
    local_buff = GetDrawTmpBufAddr(0x10);
    if (local_buff == 0) {
        pLog->warn(0, 0, "store() : not enough memory");
    } else {
        getEFB();
    }
}

// OT callback queued by move: draws the stored background (fog off), warning when the temp buffer
// was taken by someone else.
void render()
{
    GXColor col = { 0, 0, 0, 0 };
    int type;

    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    DCInvalidateRange(local_buff, 0x38000);
    type = GetDrawTmpBufType();
    if (type != 0x10 && type != 0) {
        pLog->err(0, 0, "ItemExamine render(): 0x%02x use TmpBuf", GetDrawTmpBufType());
    }
    drawBuffer();
    LightMgr.setFog();
}

// View parameters for item `id`: the merchant ext table first when ext != 0, then the main table;
// 0 when the item has no entry.
ExamInfo* examInfo(int id, int ext)
{
    ExamInfo* p;
    int i;
    int n;

    if (ext) {
        n = 2;
        p = exam_info_ext;
        for (i = 0; i < n; i++, p++) {
            if (id == p->id) {
                return p;
            }
        }
    }
    p = exam_info;
    for (i = 0; i < 225; i++, p++) {
        if (id == p->id) {
            return p;
        }
    }
    return 0;
}

// Queues the frame grab (store) for this frame when blur/render permission allows.
void ItemExamine::setup()
{
    static u16 ot_type = 0x12;
    static u8 ot_no = 0;
    static u16 ot_kind = 0x400;

    if (Render_checkBlurPermission()) {
        AddOtDirect(ot_type, (void*) 0xCDCDCDCD, store, ot_no, ot_kind, 0, 0.0f);
    }
}

// Builds the 2D overlay: for weapons in the merchant mode the four level bars (firepower, firing
// speed, reload, capacity) as id units 0x27, with the current level, the max level and the
// upgrade colours from IdSub; then the frame ids (0x26) from the exam/common archives.
void ItemExamine::idSet()
{
    SubScreenWork* wk = &SubScreenWk;
    ItemInfo inf;
    int d[3];
    int kind;
    int j;
    int n;
    int val;
    int base;
    int base2;
    int started;

    switch (m_scrn_flag) {
    case 0:
        m_pIdSys = &IdSys;
        break;
    case 1:
    case 2:
        m_pIdSys = &IdSub;
        break;
    }
    switch (m_scrn_flag) {
    case 1:
        m_pIdSys->set(SS_ARC_PTR(wk->pCmmn, 15), 0xFF, 0x27, 0x15, 2, 0);
        itemInfo(m_item_id, &inf);
        if (inf.type == 1) {
            val = 0;
            base = 0;
            base2 = 0;
            for (kind = 0; kind <= 3; kind++) {
                switch (kind) {
                case 0:
                    val = (int) (getPowerRatio(m_item_id, lv[0]) * 10.0f + 0.5f);
                    base = 0x11;
                    base2 = 0xF1;
                    break;
                case 1:
                    val = (int) (getSpeedRatio(m_item_id, lv[1]) * 100.0f + 0.5f);
                    base = 0x21;
                    base2 = 0xE1;
                    break;
                case 2:
                    val = (int) (getReloadRatio(m_item_id, lv[2]) * 100.0f + 0.5f);
                    base = 0x31;
                    base2 = 0xD1;
                    break;
                case 3:
                    val = (int) getBulletRatio(m_item_id, lv[3]);
                    base = 0x41;
                    base2 = 0xC1;
                    break;
                }
                for (j = 0; j < 3; j++) {
                    d[j] = val % 10;
                    val /= 10;
                }
                started = 0;
                for (j = 2; j >= 0; j--) {
                    IdUnit* u = m_pIdSys->unitPtr(base2 + j, 0x27);
                    u->tex_flag |= 2;
                    u->texNo = d[j];
                    if (kind == 3) {
                        if (started == 0 && d[j] == 0) {
                            u->be_flag &= ~8;
                        } else {
                            started = 1;
                            u->be_flag |= 8;
                        }
                    } else {
                        if (kind == 0 && j == 2 && d[j] == 0) {
                            u->be_flag &= ~8;
                        } else {
                            u->be_flag |= 8;
                        }
                    }
                }
                for (n = 0; n <= 5; n++) {
                    IdUnit* a;
                    IdUnit* b;
                    IdUnit* c;
                    IdUnit* u;
                    int l;

                    u = m_pIdSys->unitPtr(base + n, 0x27);
                    if (n < WeaponId2MaxLevel(m_item_id, kind)) {
                        u->be_flag |= 8;
                    } else {
                        u->be_flag &= ~8;
                    }
                    a = IdSub.unitPtr(1, 0x27);
                    b = IdSub.unitPtr(2, 0x27);
                    c = IdSub.unitPtr(3, 0x27);
                    l = lv[kind];
                    if (n < l) {
                        if (l > WeaponId2MaxLevel(m_item_id, kind)) {
                            c = a;
                        } else {
                            c = b;
                        }
                    }
                    u->col0[0] = c->col0[0];
                    u->col0[1] = c->col0[1];
                    u->col0[2] = c->col0[2];
                    u->col0[3] = c->col0[3];
                }
            }
        } else {
            IdUnit* u = m_pIdSys->unitPtr(0, 0x27);
            u->be_flag &= ~8;
            u->rev_flag |= 0xF;
        }
        break;
    case 2:
        m_pIdSys->set(SS_ARC_PTR(wk->pExam, 8), 0xFF, 0x27, 0x15, 2, 0);
        m_pIdSys->set(SS_ARC_PTR(wk->pExam, 9), 0xFF, 0x27, 0x15, 2, 0);
        break;
    }
    switch (m_scrn_flag) {
    case 0:
    case 1:
        m_pIdSys->set((void*) (pG->pArc->ofs_78 + (u32) pG->pArc), 0xFF, 0x26, 0x13, 0, 0);
        break;
    case 2:
        m_pIdSys->set(SS_ARC_PTR(wk->pExam, 7), 0xFF, 0x26, 0x13, 0, 0);
        break;
    }
}

// Starts examining `model_` of item `id_` in mode `mode_`: saves the model's flags/pos/ang/ot and
// its root parts, recentres the root on the model bound centre, applies the exam_info rotation
// (degrees), builds the id overlay, sets the treasure camera (mode 2), creates the three lights of
// the exam light set (pArc ofs_58..68 by ExamInfo::light) and starts the item's est (owner 0xD1).
void ItemExamine::init(u16 id_, cModel* model_, u8 mode_)
{
    static f32 c0 = -0.5f;
    ModelDataHead* h;
    cModel* parts;
    cLit* lit;
    ArcFile* arc;
    int i;
    u16 no;

    m_pModel = model_;
    m_item_id = id_;
    m_be_flag_bak = model_->be_flag;
    m_pos_bak = model_->pos;
    m_ang_bak = m_pModel->ang;
    m_ot_type_bak = m_pModel->ot_type;
    m_pModel->be_flag |= 0x4000;
    m_pList_pParent_bak = m_pModel->pParts->pParent;
    m_pModel->pParts->pParent = m_pModel;
    h = m_pModel->pModelInfo->pData->pHead;
    m_pList_pos_bak = m_pModel->pParts->pos;
    m_pModel->pParts->pos.x = h->center.x;
    m_pModel->pParts->pos.y = h->center.y;
    m_pModel->pParts->pos.z = h->center.z;
    m_pList_ang_bak = m_pModel->pParts->ang;
    m_pModel->pParts->ang.x = m_pModel->pParts->ang.y = m_pModel->pParts->ang.z = 0.0f;
    m_scrn_flag = mode_;
    idSet();
    switch (m_scrn_flag) {
    case 1:
        m_pInfo = examInfo(m_item_id, 1);
        break;
    case 0:
        m_pInfo = examInfo(m_item_id, 0);
        break;
    case 2:
        m_pInfo = 0;
        break;
    }
    m_pModel->ot_type = 6;
    if (m_pInfo) {
        m_pModel->ang.x = m_pInfo->rot.x * 3.1415927f / 180.0f;
        m_pModel->ang.y = m_pInfo->rot.y * 3.1415927f / 180.0f;
        m_pModel->ang.z = m_pInfo->rot.z * 3.1415927f / 180.0f;
    } else {
        m_pModel->ang.x = 0.0f;
        m_pModel->ang.y = 0.0f;
        m_pModel->ang.z = 0.0f;
    }
    m_pModel->pos.x = 0.0f;
    m_pModel->pos.y = 0.0f;
    m_pModel->pos.z = 0.0f;
    m_pModel->matUpdate();
    m_pModel->be_flag &= ~0x20;
    parts = m_pModel->getPartsPtr(0);
    if (parts) {
        PSVECScale(&parts->world, &m_pModel->pos, -1.0f);
    } else {
        pLog->err(0, 0, "ItemExamine(): Parts 0 not found.");
    }
    m_pModel->matUpdate();
    m_pModel->partsMatCalc();
    m_pModel->partsWorldCalc();
    if (m_scrn_flag == 2) {
        Vec p = { 0.0f, 0.0f, 0.0f };
        Vec mid;
        Vec at;
        Vec* p0;
        f32 c;

        p.z = cap_dist_max;
        p0 = &m_pModel->getPartsPtr(0)->world;
        c = c0;
        VecLinearCombination(p0, &m_pModel->getPartsPtr(1)->world, c, 1.0f - c0, &mid);
        PSVECScale(&mid, &mid, 0.5f);
        PSVECAdd(&mid, &p, &at);
        itemCamera.param.at = mid;
        itemCamera.param.pos = at;
        itemCamera.dist = cap_dist_max;
        g_rad_x = 0.0f;
    } else {
        arc = pG->pArc;
        lit = (cLit*) (arc->ofs_58 + (u32) arc);
        if (m_pInfo) {
            switch (m_pInfo->light) {
            case 0:
                lit = (cLit*) (arc->ofs_58 + (u32) arc);
                break;
            case 1:
                lit = (cLit*) (arc->ofs_5C + (u32) arc);
                break;
            case 2:
                lit = (cLit*) (arc->ofs_60 + (u32) arc);
                break;
            case 3:
                lit = (cLit*) (arc->ofs_64 + (u32) arc);
                break;
            case 4:
                lit = (cLit*) (arc->ofs_68 + (u32) arc);
                break;
            default:
                lit = (cLit*) (pG->pArc->ofs_58 + (u32) pG->pArc);
                break;
            }
        }
        for (i = 0; i <= 2; i++) {
            m_pLight[i] = LightMgr.create(lit, 0, i, 0);
        }
        LightMgr.offKind(0x7F);
        no = m_item_id;
        if (m_scrn_flag == 1) {
            switch (no) {
            case 0xA:
                no = 0x1B;
                break;
            case 0x89:
            case 0x8A:
                return;
            }
        } else {
            if (no == 0x89 && pG->room_id == 0x113) {
                return;
            }
        }
        if (EspGetEstAddr(0xD1, (u8) no, 1)) {
            EstSet((int) m_pModel, -1, 0, 0, 0xD1, (u8) no, 0xA001, 0x3B, (u32) m_pModel, 0);
        }
    }
}

// Sets the weapon levels shown by the bars (-1 = not upgradable).
void ItemExamine::level(s8 pwr, s8 spd, s8 rld, s8 blt)
{
    lv[0] = pwr;
    lv[1] = spd;
    lv[2] = rld;
    lv[3] = blt;
}

// Per-frame: spins the model pi/60 rad per frame about the axis chosen by ExamInfo rot0/rot1
// (world y or model y; the treasure viewer lets the stick rotate and C up/down zoom between
// cap_dist_min/max instead), fits the camera so the model bound spans the id frame, lights it and
// queues the background render.
void ItemExamine::move()
{
    static Vec _campos = { 0.0f, 0.0f, 0.0f };
    static Vec _target = { 0.0f, 0.0f, 0.0f };
    static Vec _up = { 0.0f, 1.0f, 0.0f };
    static f32 _fovy = 30.0f;
    static f32 ROT_Y_STEP = 60.0f;
    static f32 dist_add = 100.0f;
    static u16 ot_type = 0x12;
    static u8 ot_no = 0;
    static u16 ot_kind = 0x400;
    Mtx m;
    Vec axis;
    Vec zero = { 0.0f, 0.0f, 0.0f };
    f32 step;
    int rotMode;

    rotMode = 0;
    if (m_item_id == 0x93) {
        step = ROT_Y_STEP * 2.0f;
    } else {
        step = ROT_Y_STEP;
    }
    if (m_pInfo) {
        switch (m_scrn_flag) {
        case 1:
            switch (m_pInfo->rot1) {
            case 0:
                rotMode = 0;
                break;
            case 1:
                rotMode = 1;
                break;
            }
            break;
        case 0:
            switch (m_pInfo->rot0) {
            case 0:
                rotMode = 0;
                break;
            case 1:
                rotMode = 1;
                break;
            }
            break;
        }
        switch (rotMode) {
        case 0:
            axis.x = 0.0f;
            axis.y = 1.0f;
            axis.z = 0.0f;
            PSMTXIdentity(m);
            MtxRotAxisPosRad(m, &axis, &zero, 3.1415927f / step);
            break;
        case 1:
            axis.x = m_pModel->mat[0][1];
            axis.y = m_pModel->mat[1][1];
            axis.z = m_pModel->mat[2][1];
            PSMTXIdentity(m);
            MtxRotAxisPosRad(m, &axis, &zero, 3.1415927f / step);
            break;
        }
    } else {
        PSMTXIdentity(m);
        if (m_scrn_flag != 2) {
            axis.x = 0.0f;
            axis.y = 1.0f;
            axis.z = 0.0f;
            MtxRotAxisPosRad(m, &axis, &zero, 3.1415927f / step);
        }
    }
    PSMTXConcat(m, m_pModel->mat, m_pModel->mat);
    m_pModel->partsMatCalc();
    m_pModel->partsWorldCalc();
    if (m_scrn_flag == 2) {
        if (Key.on & 0xC00000) {
            f32 d = itemCamera.dist;
            if (Key.on & 0x400000) {
                d += dist_add;
            }
            if (Key.on & 0x800000) {
                d -= dist_add;
            }
            CameraCamposDistance(&itemCamera, d < cap_dist_min ? cap_dist_min : (d > cap_dist_max ? cap_dist_max : d));
        }
        if (Key.stickX != 0) {
            CameraCamposRot(&itemCamera, 'Y', (f32) Key.stickX * -0.06666667f * 0.017453292f);
        }
        if (Key.stickY != 0) {
            f32 r = (f32) Key.stickY * 0.05f * 0.017453292f;
            if (g_rad_x + r <= cap_xrad_min) {
                r = cap_xrad_min - g_rad_x;
                g_rad_x = cap_xrad_min;
            } else if (g_rad_x + r >= cap_xrad_max) {
                r = cap_xrad_max - g_rad_x;
                g_rad_x = cap_xrad_max;
            } else {
                g_rad_x = g_rad_x + r;
            }
            CameraCamposRot(&itemCamera, 'X', r);
        }
        itemCamera.param.roll = 0.0f;
        itemCamera.param.fovy = _fovy;
        CameraSetOrientationRoll(&itemCamera);
    } else {
        ModelBound* b = &m_pModel->pModelInfo->bound;
        Vec a;
        Vec c;
        Vec e;
        f32 dist;
        f32 r0;
        f32 r1;
        f32 r2;
        f32 h;
        f32 len;
        f32 dif;
        f32 r;
        f32 y;
        f32 z;

        len = SQRTF(b->size.x * b->size.x + b->size.y * b->size.y + b->size.z * b->size.z);
        if (m_pInfo) {
            len /= m_pInfo->scale;
        }
        a = m_pIdSys->unitPtr(0xF2, 0x26)->scr;
        c = m_pIdSys->unitPtr(0xF3, 0x26)->scr;
        PSVECAdd(&a, &c, &e);
        PSVECScale(&e, &e, 0.5f);
        h = 0.5f;
        dist = 240.0f / tanf(_fovy * 0.017453292f * h);
        r0 = atan2f(a.y, dist);
        r1 = atan2f(c.y, dist);
        r2 = atan2f(e.y, dist);
        dif = fabsf(r0 - r1);
        r = len * tanf((3.1415927f - dif) * h);
        y = r * SINF(r2);
        z = r * COSF(r2);
        _target.y = -y;
        _campos.y = -y;
        _campos.z = z;
        itemCamera.param.pos = _campos;
        itemCamera.param.at = _target;
        itemCamera.up = _up;
        itemCamera.param.fovy = _fovy;
    }
    C_MTXPerspective(itemCamera.ProjMat, itemCamera.param.fovy, 1.3333334f, ZNEAR, ZFAR);
    C_MTXLookAt(itemCamera.v_mat, &itemCamera.param.pos, &itemCamera.up, &itemCamera.param.at);
    LightMgr.setModel2(m_pModel);
    if (!StaFlagChk(pG, STA_SUB_SCRN)) {
        AddOtDirect(ot_type, (void*) 0xCDCDCDCD, render, ot_no, ot_kind, 0, 0.0f);
    }
}

// Draws the examined model.
void ItemExamine::trans()
{
    ModelTrans(m_pModel);
}

// Leaves the screen: deletes the item's effects (Core_flg 0xA001 kind 0x3B), the id overlays and the
// three lights, and re-enables the room light kinds.
void ItemExamine::quit()
{
    int i;

    m_pModel->ot_type = m_ot_type_bak;
    EffectEspDelete(0xA001, 0x3B, (u32) m_pModel, 0);
    EffectEspgenDelete(0xA001, 0x3B, (int) m_pModel);
    EffectEfmDelete(0xA001, 0x3B, (int) m_pModel);
    m_pIdSys->kill(0xFF, 0x26);
    m_pIdSys->kill(0xFF, 0x27);
    for (i = 0; i <= 2; i++) {
        LightMgr.destroy(m_pLight[i]);
    }
    LightMgr.onKind(0x7F);
}

// Restores the model's saved flags, position, angle, ot type and root parts.
void ItemExamine::reset()
{
    m_pModel->be_flag = m_be_flag_bak;
    m_pModel->pos = m_pos_bak;
    m_pModel->ang = m_ang_bak;
    m_pModel->ot_type = m_ot_type_bak;
    m_pModel->pParts->pParent = m_pList_pParent_bak;
    m_pModel->pParts->pos = m_pList_pos_bak;
    m_pModel->pParts->ang = m_pList_ang_bak;
    m_pModel->matUpdate();
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
