// game/em_cloth.cpp: cloth / hair chain tables of the enemy costume models (obj18 type 34, 18,
// 37, 33, 30) and the em2b short rope; PenCloth* (pendulum.cpp) does the simulation.

#include "obj1d.h"
#include "pl_cloth.h"
#include "pendulum.h"
#include "model.h"
#include "math_sub.h"
#include "em_cloth.h"

u8 em34ClothP[2] = {124, 125};
u8 em34ClothUp[2] = {0xFF, 124};
u8 em34ClothDp[2] = {125, 0xFF};
f32 em34ClothMax[2] = {0.3f, 0.4f};
static f32 em34ClothRate[2] = {0.8f, 0.8f};
u8 em34ClothP2[91] = {29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119};
u8 em34ClothUp2[91] = {0xFF, 29, 30, 31, 32, 33, 34, 0xFF, 36, 37, 38, 39, 40, 41, 0xFF, 43, 44, 45, 46, 47, 48, 0xFF, 50, 51, 52, 53, 54, 55, 0xFF, 57, 58, 59, 60, 61, 62, 0xFF, 64, 65, 66, 67, 68, 69, 0xFF, 71, 72, 73, 74, 75, 76, 0xFF, 78, 79, 80, 81, 82, 83, 0xFF, 85, 86, 87, 88, 89, 90, 0xFF, 92, 93, 94, 95, 96, 97, 0xFF, 99, 100, 101, 102, 103, 104, 0xFF, 106, 107, 108, 109, 110, 111, 0xFF, 113, 114, 115, 116, 117, 118};
u8 em34ClothDp2[91] = {30, 31, 32, 33, 34, 35, 0xFF, 37, 38, 39, 40, 41, 42, 0xFF, 44, 45, 46, 47, 48, 49, 0xFF, 51, 52, 53, 54, 55, 56, 0xFF, 58, 59, 60, 61, 62, 63, 0xFF, 65, 66, 67, 68, 69, 70, 0xFF, 72, 73, 74, 75, 76, 77, 0xFF, 79, 80, 81, 82, 83, 84, 0xFF, 86, 87, 88, 89, 90, 91, 0xFF, 93, 94, 95, 96, 97, 98, 0xFF, 100, 101, 102, 103, 104, 105, 0xFF, 107, 108, 109, 110, 111, 112, 0xFF, 114, 115, 116, 117, 118, 119, 0xFF};
u8 em34ClothLp2[91] = {36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
f32 em34ClothMax2[91] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
CLOTH_AT_SET em34ClothAt2_LongStride[16] = {
    {0x0000, 0x12, 0x12, 1.0f, 150.0f, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.75f, 160.0f, {40.0f, 0.0f, 0.0f}, {40.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.5f, 160.0f, {60.0f, 0.0f, 0.0f}, {60.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.25f, 160.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x13, 1.0f, 160.0f, {80.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.75f, 160.0f, {80.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.5f, 160.0f, {80.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.25f, 160.0f, {80.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 150.0f, {-30.0f, 0.0f, 0.0f}, {-30.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.75f, 160.0f, {-40.0f, 0.0f, 0.0f}, {-40.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.5f, 160.0f, {-60.0f, 0.0f, 0.0f}, {-60.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.25f, 160.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x17, 1.0f, 160.0f, {-80.0f, 0.0f, 0.0f}, {-80.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.75f, 160.0f, {-80.0f, 0.0f, 0.0f}, {-80.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.5f, 160.0f, {-80.0f, 0.0f, 0.0f}, {-80.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.25f, 160.0f, {-80.0f, 0.0f, 0.0f}, {-80.0f, 0.0f, 0.0f}},
};
static CLOTH_AT_SET em34ClothAt[5] = {
    {0x0000, 0x04, 0x04, 1.0f, 120.0f, {0.0f, -100.0f, -30.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.3f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x05, 0x05, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0B, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.7f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
u8 em18ClothP[27] = {56, 57, 58, 59, 60, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55};
u8 em18ClothUp[27] = {0xFF, 56, 57, 58, 59, 0xFF, 34, 35, 36, 37, 38, 39, 40, 0xFF, 42, 43, 44, 45, 46, 47, 48, 0xFF, 50, 51, 52, 53, 54};
u8 em18ClothDp[27] = {57, 58, 59, 60, 0xFF, 35, 36, 37, 38, 39, 40, 41, 0xFF, 43, 44, 45, 46, 47, 48, 49, 0xFF, 51, 52, 53, 54, 55, 0xFF};
u8 em18ClothLp[27] = {37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
f32 em18ClothMax[27] = {0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f};
f32 em18ClothRate[27] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
CLOTH_AT_SET em18ClothAt[15] = {
    {0x0000, 0x02, 0x02, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 150.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, -150.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.5f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x01, 1.0f, 170.0f, {0.0f, 100.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x11, 0.5f, 180.0f, {0.0f, 0.0f, 25.0f}, {0.0f, 0.0f, 25.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 180.0f, {0.0f, -150.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 180.0f, {40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.33f, 180.0f, {40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.66f, 180.0f, {40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x13, 1.0f, 180.0f, {40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 180.0f, {-40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.33f, 180.0f, {-40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.66f, 180.0f, {-40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x17, 1.0f, 180.0f, {-40.0f, -30.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
};
u8 em37HairP[8] = {64, 65, 66, 67, 68, 69, 70, 71};
u8 em37HairUp[8] = {0xFF, 64, 65, 66, 0xFF, 68, 0xFF, 70};
static u8 em37HairDp[8] = {65, 66, 67, 0xFF, 69, 0xFF, 71, 0xFF};
static f32 em37HairMax[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 em37HairRate[8] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
CLOTH_AT_SET em37HairAt[8] = {
    {0x0000, 0x03, 0x04, 0.1f, 65.0f, {0.0f, 90.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x04, 0.6f, 75.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 90.0f, {0.0f, -30.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.5f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x05, 0x03, 1.0f, 90.0f, {-50.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x05, 0x03, 1.0f, 100.0f, {-50.0f, -50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x03, 1.0f, 90.0f, {50.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x03, 1.0f, 100.0f, {50.0f, -50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
u8 em37CoatP[39] = {72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110};
static u8 em37CoatUp[39] = {0xFF, 72, 73, 0xFF, 75, 76, 0xFF, 78, 79, 0xFF, 81, 82, 0xFF, 84, 85, 0xFF, 87, 88, 0xFF, 90, 91, 0xFF, 93, 94, 0xFF, 96, 97, 0xFF, 99, 100, 0xFF, 102, 103, 0xFF, 105, 106, 0xFF, 108, 109};
u8 em37CoatDp[39] = {73, 74, 0xFF, 76, 77, 0xFF, 79, 80, 0xFF, 82, 83, 0xFF, 85, 86, 0xFF, 88, 89, 0xFF, 91, 92, 0xFF, 94, 95, 0xFF, 97, 98, 0xFF, 100, 101, 0xFF, 103, 104, 0xFF, 106, 107, 0xFF, 109, 110, 0xFF};
static u8 em37CoatLp[39] = {75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 0xFF, 0xFF, 0xFF};
f32 em37CoatMax[39] = {0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f};
static f32 em37CoatRate[39] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
static CLOTH_AT_SET em37CoatAt[12] = {
    {0x0000, 0x11, 0x12, 0.3f, 110.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.8f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.6f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.4f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.2f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x16, 0.3f, 110.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.8f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.6f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.4f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.2f, 100.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static u8 em33HairP[60] = {91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96};
static u8 em33HairUp[60] = {0xFF, 91, 92, 93, 94, 0xFF, 96, 97, 98, 99, 0xFF, 101, 102, 103, 104, 0xFF, 106, 107, 108, 109, 0xFF, 111, 112, 113, 114, 0xFF, 116, 117, 118, 119, 0xFF, 121, 122, 123, 124, 0xFF, 126, 127, 0x80, 0x81, 0xFF, 0x83, 0x84, 0x85, 0x86, 0xFF, 0x88, 0x89, 0x8A, 0x8B, 0xFF, 0x8D, 0x8E, 0x8F, 0x90, 0xFF, 0x92, 0x93, 0x94, 0x95};
static u8 em33HairDp[60] = {92, 93, 94, 95, 0xFF, 97, 98, 99, 100, 0xFF, 102, 103, 104, 105, 0xFF, 107, 108, 109, 110, 0xFF, 112, 113, 114, 115, 0xFF, 117, 118, 119, 120, 0xFF, 122, 123, 124, 125, 0xFF, 127, 0x80, 0x81, 0x82, 0xFF, 0x84, 0x85, 0x86, 0x87, 0xFF, 0x89, 0x8A, 0x8B, 0x8C, 0xFF, 0x8E, 0x8F, 0x90, 0x91, 0xFF, 0x93, 0x94, 0x95, 0x96, 0xFF};
static u8 em33ClothLp[60] = {96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static f32 em33HairMax[60] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
static CLOTH_AT_SET em33HairAt[17] = {
    {0x0000, 0x10, 0x10, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.2f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.4f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.6f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.8f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.5f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x14, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.5f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.2f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.4f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.6f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.8f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x15, 0x15, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x15, 0x16, 0.5f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 170.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static CLOTH_AT_SET em33HairAtSmall[17] = {
    {0x0000, 0x10, 0x10, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.2f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.4f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.6f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x10, 0x11, 0.8f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.5f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x14, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.5f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.2f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.4f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.6f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x15, 0.8f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x15, 0x15, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x15, 0x16, 0.5f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 144.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static u8 em33HairP2[46] = {0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6};
u8 em33HairUp2[46] = {0xFF, 0x97, 0x98, 0xFF, 0x9A, 0x9B, 0xFF, 0x9D, 0x9E, 0xFF, 0xA0, 0xA1, 0xFF, 0xA3, 0xA4, 0xFF, 0xA6, 0xA7, 0xFF, 0xAA, 0xFF, 0xAC, 0xFF, 0xAE, 0xFF, 0xB0, 0xFF, 0xB2, 0xFF, 0xB4, 0xFF, 0xB6, 0xFF, 0xB9, 0xFF, 0xBB, 0xFF, 0xBD, 0xFF, 0xBF, 0xFF, 0xC1, 0xFF, 0xC3, 0xFF, 0xC5};
static u8 em33HairDp2[46] = {0x98, 0x99, 0xFF, 0x9B, 0x9C, 0xFF, 0x9E, 0x9F, 0xFF, 0xA1, 0xA2, 0xFF, 0xA4, 0xA5, 0xFF, 0xA7, 0xA8, 0xFF, 0xAB, 0xFF, 0xAD, 0xFF, 0xAF, 0xFF, 0xB1, 0xFF, 0xB3, 0xFF, 0xB5, 0xFF, 0xB7, 0xFF, 0xBA, 0xFF, 0xBC, 0xFF, 0xBE, 0xFF, 0xC0, 0xFF, 0xC2, 0xFF, 0xC4, 0xFF, 0xC6, 0xFF};
static u8 em33ClothLp2[46] = {0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xFF, 0xFF, 0xFF, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xFF, 0xFF, 0xFF, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xFF, 0xFF, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xFF, 0xFF};
f32 em33HairMax2[46] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
static CLOTH_AT_SET em33HairAt2[11] = {
    {0x0000, 0x02, 0x02, 1.0f, 220.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 220.0f, {120.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 220.0f, {-120.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x07, 1.0f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.8f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.6f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.4f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0B, 1.0f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.8f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.6f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.4f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static CLOTH_AT_SET em33HairAt2Small[11] = {
    {0x0000, 0x02, 0x02, 1.0f, 187.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 187.0f, {102.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 187.0f, {-120.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x07, 1.0f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.8f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.6f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x07, 0x08, 0.4f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0B, 1.0f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.8f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.6f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0C, 0.4f, 127.5f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
u8 em2bShortRopeP[5] = {1, 2, 3, 4, 5};
u8 em2bShortRopeUp[5] = {0xFF, 1, 2, 3, 4};
u8 em2bShortRopeDp[5] = {2, 3, 4, 5, 0xFF};
static CLOTH_AT_SET em2bRopeAt[5] = {
    {0x0000, 0x03, 0x03, 1.0f, 650.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.5f, 750.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 900.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x05, 0x05, 1.0f, 700.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x0B, 0x0B, 1.0f, 700.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
};
static u8 em30ClothP[49] = {94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E};
static u8 em30ClothUp[49] = {0xFF, 94, 95, 96, 97, 98, 99, 0xFF, 101, 102, 103, 104, 105, 106, 0xFF, 108, 109, 110, 111, 112, 113, 0xFF, 115, 116, 117, 118, 119, 120, 0xFF, 122, 123, 124, 125, 126, 127, 0xFF, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0xFF, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D};
static u8 em30ClothDp[49] = {95, 96, 97, 98, 99, 100, 0xFF, 102, 103, 104, 105, 106, 107, 0xFF, 109, 110, 111, 112, 113, 114, 0xFF, 116, 117, 118, 119, 120, 121, 0xFF, 123, 124, 125, 126, 127, 0x80, 0xFF, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0xFF, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0xFF};
u8 em30ClothLp[49] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87};
static f32 em30ClothMax[49] = {0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 em30ClothWindS[49] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f};
static f32 em30ClothWindR[49] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f};
CLOTH_AT_SET em30ClothAt[2] = {
    {0x0000, 0x01, 0x01, 1.0f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 150.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};
static u8 em30ClothP2[30] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93};
u8 em30ClothUp2[30] = {0xFF, 64, 0xFF, 66, 0xFF, 68, 69, 0xFF, 71, 72, 73, 74, 0xFF, 76, 77, 78, 79, 0xFF, 81, 82, 83, 84, 0xFF, 86, 87, 88, 89, 0xFF, 91, 92};
u8 em30ClothDp2[30] = {65, 0xFF, 67, 0xFF, 69, 70, 0xFF, 72, 73, 74, 75, 0xFF, 77, 78, 79, 80, 0xFF, 82, 83, 84, 85, 0xFF, 87, 88, 89, 90, 0xFF, 92, 93, 0xFF};
static u8 em30ClothLp2[30] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 88, 89, 90};
f32 em30ClothMax2[30] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

// Enemy 34 costume, cloth 1: the 91-node coat / robe chain (bones 29..119, 14 bundles, 16
// collision sets on the legs), gravity 20, damping 0.7; PenClothSet initialises the pendulums.
void Em34ClothSet1(cModel* pEm, PlCloth* pCloth)
{
    {
        // COMPILER-DIFF: 13 (local-alloc qty order): a codeless prio-4 filler issued before the
        // pUp2 `lis` in sched1 equalises the pUp2/pDown2/pMax2 qty lives (r10/r8/r7 in qty order).
        register u32 k asm("r12");
        asm("" : "=r"(k));
        asm("" : "=m"(em34ClothRate[1]) : "r"(k));
    }
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pWindSin = 0;
    pCloth->pWindRate = 0;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->pModel = pEm;
    pCloth->WindSin = 0.0f;
    pCloth->Move_rate = 0.0f;
    pCloth->Num = 91;
    pCloth->pCloth = em34ClothP2;
    pCloth->pLeft = em34ClothLp2;
    pCloth->pParent = em34ClothUp2;
    pCloth->pChild = em34ClothDp2;
    pCloth->pMax = em34ClothMax2;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    pCloth->pAtset = em34ClothAt2_LongStride;
    pCloth->At_num = 16;
    pCloth->Gravity = 20.0f;
    pCloth->Rate = 0.7f;
    pCloth->Bundle_num = 14;
    pCloth->Stretchy = 1.0f;
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of the enemy 34 coat (the model-driven PenClothMove3 variant).
void Em34ClothMove1(cModel* pEm, PlCloth* pCloth)
{
    PenClothMove3(pEm, (PenCloth*) pCloth);
}

// Clears the model's cloth-state bits (be_flag 0x00E00000) so the chains re-seat next frame.
void Em34ClothReset(cModel* pEm)
{
    pEm->be_flag &= ~0x00E00000;
}

// Enemy 34 costume, cloth 2: the two-node dangling part (bones 124 / 125), 5 collision sets,
// gravity 15, damping 0.8.
void Em34ClothSet2(cModel* pEm, PlCloth* pCloth)
{
    pCloth->Num = 2;
    pCloth->pCloth = em34ClothP;
    pCloth->pLeft = 0;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = em34ClothUp;
    pCloth->pChild = em34ClothDp;
    pCloth->pMax = em34ClothMax;
    pCloth->pWindSin = 0;
    pCloth->pWindRate = 0;
    pCloth->pGravity = 0;
    pCloth->pAtset = em34ClothAt;
    pCloth->pRate = em34ClothRate;
    pCloth->At_num = 5;
    pCloth->Gravity = 15.0f;
    pCloth->Rate = 0.8f;
    pCloth->Bundle_num = 4;
    pCloth->pModel = pEm;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 1.0f;
    pCloth->Move_rate = 0.0f;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of enemy 34 cloth 2.
void Em34ClothMove2(cModel* pEm, PlCloth* pCloth)
{
    PenClothMove3(pEm, (PenCloth*) pCloth);
}

// Enemy 18 (robed ganado) costume: the 27-node robe skirt in 4 strands (bones 34..60), 15
// collision sets, gravity 20, very soft (Rate 0.1, Stretchy 0.05); mode != 0 drops the per-node
// rate table and makes it stiff (Stretchy 1).
void Em18ClothSet(cModel* pEm, PlCloth* pCloth, int mode)
{
    pCloth->Num = 27;
    pCloth->pCloth = em18ClothP;
    pCloth->pLeft = em18ClothLp;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = em18ClothUp;
    pCloth->pChild = em18ClothDp;
    pCloth->pMax = em18ClothMax;
    pCloth->pWindSin = 0;
    pCloth->pWindRate = 0;
    pCloth->pGravity = 0;
    pCloth->pAtset = em18ClothAt;
    pCloth->pRate = em18ClothRate;
    pCloth->At_num = 15;
    pCloth->Gravity = 20.0f;
    pCloth->pModel = pEm;
    pCloth->Rate = 0.1f;
    pCloth->Bundle_num = 4;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 0.05f;
    pCloth->Move_rate = 0.0f;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    if (mode) {
        // The x40/x44 stores repeat the defaults: they are real uses for flow/sched1/regalloc
        // (0.1 and 4 live across the branch: f11, callee-saved r28) and reload_cse_regs deletes
        // them as no-op stores before sched2 (no code).
        pCloth->pRate = 0;
        pCloth->Rate = 0.1f;
        pCloth->Bundle_num = 4;
        pCloth->Stretchy = 1.0f;
    }
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of the enemy 18 robe; clears the cloth-state bits afterwards.
void Em18ClothMove(cModel* pEm, PlCloth* pCloth)
{
    PenClothMove3(pEm, (PenCloth*) pCloth);
    pEm->be_flag &= ~0x00E00000;
}

// Enemy 37 costume, hair: an 8-node chain, gravity 10, damping 0.8, Stretchy 0.1.
void Em37HairSet(cModel* pEm, PlCloth* pCloth)
{
    pCloth->Num = 8;
    pCloth->pCloth = em37HairP;
    pCloth->pLeft = 0;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = em37HairUp;
    pCloth->pChild = em37HairDp;
    pCloth->pMax = em37HairMax;
    pCloth->pWindSin = 0;
    pCloth->pWindRate = 0;
    pCloth->pGravity = 0;
    pCloth->pAtset = em37HairAt;
    pCloth->pRate = em37HairRate;
    pCloth->At_num = 8;
    pCloth->Gravity = 10.0f;
    pCloth->Rate = 0.8f;
    pCloth->Bundle_num = 4;
    pCloth->pModel = pEm;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 0.1f;
    pCloth->Move_rate = 0.0f;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of the enemy 37 hair (plain PenClothMove).
void Em37HairMove(cModel* pEm, PlCloth* pCloth)
{
    PenClothMove(pEm, (PenCloth*) pCloth);
}

// Clears the model's cloth-state bits.
void Em37ClothReset(cModel* pEm)
{
    pEm->be_flag &= ~0x00E00000;
}

// Enemy 37 costume, coat: a 39-node chain, gravity 15, damping 0.9.
void Em37CoatSet(cModel* pEm, PlCloth* pCloth)
{
    pCloth->Num = 39;
    pCloth->pCloth = em37CoatP;
    pCloth->pLeft = em37CoatLp;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = em37CoatUp;
    pCloth->pChild = em37CoatDp;
    pCloth->pMax = em37CoatMax;
    pCloth->pWindSin = 0;
    pCloth->pWindRate = 0;
    pCloth->pGravity = 0;
    pCloth->pAtset = em37CoatAt;
    pCloth->pRate = em37CoatRate;
    pCloth->At_num = 12;
    pCloth->Gravity = 15.0f;
    pCloth->Rate = 0.9f;
    pCloth->Bundle_num = 4;
    pCloth->pModel = pEm;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 0.1f;
    pCloth->Move_rate = 0.0f;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of the enemy 37 coat.
void Em37CoatMove(cModel* pEm, PlCloth* pCloth)
{
    PenClothMove(pEm, (PenCloth*) pCloth);
}

// Enemy 33 costume, cloth 1: a 60-node chain, gravity 10, damping 0.8; `small` selects the
// collision set table of the small variant.
void Em33ClothSet(cModel* pEm, PlCloth* pCloth, int mode)
{
    pCloth->Num = 60;
    pCloth->pCloth = em33HairP;
    pCloth->pLeft = em33ClothLp;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = em33HairUp;
    pCloth->pChild = em33HairDp;
    pCloth->pMax = em33HairMax;
    pCloth->pWindSin = 0;
    pCloth->pWindRate = 0;
    if (mode == 0) {
        pCloth->pAtset = em33HairAt;
    } else {
        pCloth->pAtset = em33HairAtSmall;
    }
    pCloth->At_num = 17;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->Gravity = 10.0f;
    pCloth->Rate = 0.8f;
    pCloth->Bundle_num = 4;
    pCloth->pModel = pEm;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 0.1f;
    pCloth->Move_rate = 0.0f;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of enemy 33 cloth 1.
void Em33ClothMove(cModel* pEm, PlCloth* pCloth)
{
    PenClothMove3(pEm, (PenCloth*) pCloth);
}

// Enemy 33 costume, cloth 2: a 46-node chain with the same parameters; `small` as above.
void Em33ClothSet2(cModel* pEm, PlCloth* pCloth, int mode)
{
    pCloth->Num = 46;
    pCloth->pCloth = em33HairP2;
    pCloth->pLeft = em33ClothLp2;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = em33HairUp2;
    pCloth->pChild = em33HairDp2;
    pCloth->pMax = em33HairMax2;
    pCloth->pWindSin = 0;
    pCloth->pWindRate = 0;
    if (mode == 0) {
        pCloth->pAtset = em33HairAt2;
    } else {
        pCloth->pAtset = em33HairAt2Small;
    }
    pCloth->At_num = 11;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->Gravity = 10.0f;
    pCloth->Rate = 0.8f;
    pCloth->Bundle_num = 4;
    pCloth->pModel = pEm;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 0.1f;
    pCloth->Move_rate = 0.0f;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of enemy 33 cloth 2.
void Em33ClothMove2(cModel* pEm, PlCloth* pCloth)
{
    cModel* p;

    p = pEm->getPartsPtr(0xA9);
    p->ang.z = pEm->getPartsPtr(7)->ang.z;
    RotMatrix(p->l_mat, &p->ang);
    TransMatrix(p->l_mat, &p->pos);
    ScaleMatrix(p->l_mat, &p->scale);
    PSMTXConcat(p->pParent->mat, p->l_mat, p->mat);
    p->world.x = p->mat[0][3];
    p->world.y = p->mat[1][3];
    p->world.z = p->mat[2][3];
    p = pEm->getPartsPtr(0xB8);
    p->ang.z = pEm->getPartsPtr(0xB)->ang.z;
    RotMatrix(p->l_mat, &p->ang);
    TransMatrix(p->l_mat, &p->pos);
    ScaleMatrix(p->l_mat, &p->scale);
    PSMTXConcat(p->pParent->mat, p->l_mat, p->mat);
    p->world.x = p->mat[0][3];
    p->world.y = p->mat[1][3];
    p->world.z = p->mat[2][3];
    PenClothMove3(pEm, (PenCloth*) pCloth);
}

// Clears the model's cloth-state bits.
void Em33ClothReset(cModel* pEm)
{
    pEm->be_flag &= ~0x00E00000;
}

// El Gigante (em2b) short rope: creates a chain object from the rope model / TPL, gives it a
// 5-node pendulum chain (gravity 20, damping 0.8) and hangs it between parts 3 and 4 of the
// enemy with fixed offsets. Returns the chain object, NULL when it could not be created.
cObjChain* Em2bShortRopeSet(cModel* m, PlCloth* c, void* bin, void* tpl)
{
    cObjChain* chain;
    Vec pos;
    Vec ofs;
    Vec rot;

    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    chain = SetChain(bin, tpl, &pos, &rot);
    if (chain == 0) {
        return 0;
    }
    c->Num = 5;
    c->pCloth = em2bShortRopeP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->pUpRight = 0;
    c->pParent = em2bShortRopeUp;
    c->pChild = em2bShortRopeDp;
    c->pMax = 0;
    c->pWindSin = 0;
    c->pWindRate = 0;
    c->pGravity = 0;
    c->pRate = 0;
    c->pAtset = em2bRopeAt;
    c->At_num = 5;
    c->Gravity = 20.0f;
    c->Rate = 0.8f;
    c->Bundle_num = 100;
    c->pModel = m;
    c->WindSin = 0.0f;
    c->Stretchy = 0.1f;
    c->Move_rate = 0.0f;
    c->Flag = 0;
    c->pPtbl = 0;
    chain->setChain((PenCloth*) c);
    pos.x = -290.0f;
    pos.y = -162.95f;
    pos.z = 655.0f;
    ofs.x = -290.0f;
    ofs.y = -412.32f;
    ofs.z = 39.15f;
    chain->setParent2(m, 3, &pos, 4, &ofs, 0);
    return chain;
}

// Enemy 30 costume, cloth 1: the 49-node robe with a wind rate table, gravity 30, damping 0.9.
void Em30ClothSet1(cModel* pEm, PlCloth* pCloth)
{
    pCloth->Num = 49;
    pCloth->pCloth = em30ClothP;
    pCloth->pLeft = em30ClothLp;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = em30ClothUp;
    pCloth->pChild = em30ClothDp;
    pCloth->pMax = em30ClothMax;
    pCloth->pWindSin = em30ClothWindS;
    pCloth->pWindRate = em30ClothWindR;
    pCloth->pGravity = 0;
    pCloth->pAtset = em30ClothAt;
    pCloth->pRate = 0;
    pCloth->At_num = 2;
    pCloth->Gravity = 30.0f;
    pCloth->Rate = 0.9f;
    pCloth->Bundle_num = 20;
    pCloth->pModel = pEm;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 1.0f;
    pCloth->Move_rate = 0.0f;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of enemy 30 cloth 1.
void Em30ClothMove1(cModel* pEm, PlCloth* pCloth)
{
    PenClothMove3(pEm, (PenCloth*) pCloth);
}

// Enemy 30 costume, cloth 2: a 30-node chain, gravity 15, damping 0.5, Stretchy 0.05.
void Em30ClothSet2(cModel* pEm, PlCloth* pCloth)
{
    pCloth->Num = 30;
    pCloth->pCloth = em30ClothP2;
    pCloth->pLeft = em30ClothLp2;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = em30ClothUp2;
    pCloth->pChild = em30ClothDp2;
    pCloth->pWindSin = 0;
    pCloth->pWindRate = 0;
    pCloth->pMax = em30ClothMax2;
    pCloth->pAtset = 0;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->At_num = 0;
    pCloth->Gravity = 15.0f;
    pCloth->Rate = 0.5f;
    pCloth->pModel = pEm;
    pCloth->Bundle_num = 0;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 0.05f;
    pCloth->Move_rate = 0.0f;
    pCloth->Flag = 0;
    pCloth->pPtbl = 0;
    PenClothSet(pEm, (PenCloth*) pCloth, 100.0f);
}

// Per-frame simulation of enemy 30 cloth 2.
void Em30ClothMove2(cModel* pEm, PlCloth* pCloth)
{
    PenClothMove3(pEm, (PenCloth*) pCloth);
}

// Clears the model's cloth-state bits.
void Em30ClothReset(cModel* pEm)
{
    pEm->be_flag &= ~0x00E00000;
}

// Never called (dead-stripped by the original linker; only its 0.0f pool entry survives).
static void Em30ClothStop(cModel* m, PlCloth* c)
{
    c->WindSin = 0.0f;
}

asm(".section .sdata; .balign 8");
