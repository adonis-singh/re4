// game/pl_cloth.cpp: player cloth simulation setup: hair / jacket / holster (Leon), hair / skirt /
// sweater / ribbon (Ashley), hair (Luis), dress / hair / ribbon (Ada). The chain tables give each
// link its model part and neighbours; PenCloth* (pendulum.cpp) does the simulation.

#include "obj1d.h"
#include "event.h"
#include "atari.h"
#include "pl_cloth.h"
#include "pendulum.h"
#include "model.h"
#include "global.h"
#include "math_sub.h"

PlCloth leonHair;
PlCloth leonJacket;
PlCloth leonHolster;
PlCloth girlHair;
PlCloth girlSkirt;
PlCloth girlSweater;
PlCloth luisHair;
PlCloth adaDress;
PlCloth adaHair;
PlCloth adaRibbon;

void testHairSetLeon(cModel* pl, PlCloth* pCloth);
void testHairMoveLeon(cModel* pl, PlCloth* pCloth);
void testJacketSetLeon(cModel* pl, PlCloth* pCloth);
void testJacketMoveLeon(cModel* pl, PlCloth* pCloth);
void testHolsterSetLeon(cModel* pl, PlCloth* pCloth);
void testHolsterMoveLeon(cModel* pl, PlCloth* pCloth);
void testHairSetGirl(cModel* pl, PlCloth* pCloth, int evt);
void testHairMoveGirl(cModel* pl, PlCloth* pCloth);
void testSkirtSetGirl(cModel* pl, PlCloth* pCloth, int evt);
void testSkirtMoveGirl(cModel* pl, PlCloth* pCloth);
void testSweaterSetGirl(cModel* pl, PlCloth* c);
void testSweaterMoveGirl(cModel* pl, PlCloth* c);
void testRibbonSetGirl(cModel* pl, PlCloth* pCloth);
void testRibbonMoveGirl(cModel* pl, PlCloth* pCloth);
void girlLapelMove(cModel* pl);
void testHairSetLuis(cModel* pl, PlCloth* pCloth);
void testHairMoveLuis(cModel* pl, PlCloth* pCloth);
void testDressSetAda(cModel* pl, PlCloth* c, int evt);
void testDressMoveAda(cModel* pl, PlCloth* c);
void testHairSetAda(cModel* pl, PlCloth* c);
void testHairMoveAda(cModel* pl, PlCloth* c);

// leonHair
u8 leonHairP[21] = {65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85};
u8 leonHairUp[21] = {0xFF, 65, 66, 0xFF, 68, 0xFF, 70, 0xFF, 72, 0xFF, 74, 0xFF, 76, 0xFF, 78, 0xFF, 80, 0xFF, 82, 0xFF, 84};
u8 leonHairDp[21] = {66, 67, 0xFF, 69, 0xFF, 71, 0xFF, 73, 0xFF, 75, 0xFF, 77, 0xFF, 79, 0xFF, 81, 0xFF, 83, 0xFF, 85, 0xFF};
f32 leonHairMax[21] = {0.4f, 0.5f, 0.6f, 0.4f, 0.5f, 0.4f, 0.5f, 0.3f, 0.3f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.3f, 0.3f, 0.4f, 0.5f, 0.4f, 0.5f};
f32 leonHairWindS[21] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.5f, 1.5f, 2.0f, 2.0f, 2.5f, 2.5f, 3.0f, 3.0f, -2.5f, -2.5f, -2.0f, -2.0f, -1.5f, -1.5f, 0.0f, 0.0f};
f32 leonHairWindR[21] = {0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
CLOTH_AT_SET leonHairAt[4] = {
    {0x0000, 0x04, 0x04, 1.0f, 90.0f, {0.0f, 90.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 85.0f, {0.0f, 90.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 90.0f, {0.0f, 100.0f, 25.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 75.0f, {0.0f, 65.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
};

// leonJacket
u8 leonJacketP[24] = {86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109};
u8 leonJacketLp[24] = {0xFF, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 108, 109, 0xFF, 0xFF, 0xFF};
u8 leonJacketRp[24] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
u8 leonJacketUp[24] = {0xFF, 86, 87, 0xFF, 89, 0xFF, 91, 0xFF, 93, 0xFF, 95, 0xFF, 97, 0xFF, 99, 0xFF, 101, 0xFF, 103, 0xFF, 105, 0xFF, 107, 108};
u8 leonJacketDp[24] = {87, 88, 0xFF, 90, 0xFF, 92, 0xFF, 94, 0xFF, 96, 0xFF, 98, 0xFF, 100, 0xFF, 102, 0xFF, 104, 0xFF, 106, 0xFF, 108, 109, 0xFF};
f32 leonJacketMax[24] = {0.1f, 0.2f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.3f, 0.1f, 0.2f, 0.3f};
f32 leonJacketWindS[24] = {0.0f, 0.0f, 0.0f, 0.4f, 0.4f, 0.9f, 0.9f, 1.2f, 1.2f, 1.5f, 1.5f, 1.7f, 1.7f, 1.9f, 1.9f, 2.1f, 2.1f, 2.4f, 2.4f, 2.8f, 2.8f, 3.1f, 3.1f, 3.1f};
f32 leonJacketWindR[24] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
CLOTH_AT_SET leonJacketAt[6] = {
    {0x0000, 0x02, 0x02, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 130.0f, {0.0f, -50.0f, 20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x11, 0.5f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 140.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.4f, 125.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x16, 0.4f, 125.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};

// leonHolster
u8 leonHolsterP[2] = {29, 30};
u8 leonHolsterLp[2] = {0xFF, 0xFF};
u8 leonHolsterRp[2] = {0xFF, 0xFF};
u8 leonHolsterUp[2] = {0xFF, 0xFF};
u8 leonHolsterDp[2] = {0xFF, 0xFF};
f32 leonHolsterMax[2] = {0.1f, 0.1f};
f32 leonHolsterWindS[2] = {0.0f, 1.5f};
f32 leonHolsterWindR[2] = {0.1f, 0.1f};
CLOTH_AT_SET leonHolsterAt[1] = {
    {0x0000, 0x02, 0x02, 1.0f, 90.0f, {0.0f, 50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};

// girlHair
u8 girlHairP[21] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84};
u8 girlHairUp[21] = {0xFF, 64, 0xFF, 66, 0xFF, 68, 0xFF, 70, 71, 0xFF, 73, 74, 0xFF, 76, 77, 0xFF, 79, 80, 0xFF, 82, 83};
u8 girlHairDp[21] = {65, 0xFF, 67, 0xFF, 69, 0xFF, 71, 72, 0xFF, 74, 75, 0xFF, 77, 78, 0xFF, 80, 81, 0xFF, 83, 84, 0xFF};
f32 girlHairMax[21] = {0.4f, 0.5f, 0.4f, 0.5f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f, 0.3f, 0.4f, 0.5f};
f32 girlHairMaxEvt[21] = {0.4f, 0.5f, 0.1f, 0.3f, 0.4f, 0.5f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
f32 girlHairWindS[21] = {0.0f, 0.2f, 1.0f, 1.2f, 0.0f, 0.2f, 1.0f, 1.2f, 1.4f, 0.0f, 0.2f, 0.4f, 1.0f, 1.2f, 1.4f, 0.0f, 0.2f, 0.4f, 1.0f, 1.2f, 1.4f};
f32 girlHairWindR[21] = {0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f};
CLOTH_AT_SET girlHairAt[5] = {
    {0x0000, 0x04, 0x04, 1.0f, 72.0f, {0.0f, 50.0f, -10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x04, 0.5f, 75.0f, {0.0f, 60.0f, 0.0f}, {0.0f, 60.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 100.0f, {0.0f, -20.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {-3.0f, 100.0f, 42.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 68.0f, {-3.0f, 60.0f, 45.0f}, {0.0f, 0.0f, 0.0f}},
};
CLOTH_AT_SET girlHairAtEvt[7] = {
    {0x0000, 0x04, 0x04, 1.0f, 65.0f, {0.0f, 50.0f, -10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x04, 0.5f, 80.0f, {0.0f, 50.0f, 0.0f}, {0.0f, 50.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 90.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 65.0f, {0.0f, 100.0f, 35.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 60.0f, {0.0f, 70.0f, 50.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.3f, 100.0f, {-50.0f, 0.0f, -10.0f}, {-50.0f, 0.0f, -10.0f}},
    {0x0000, 0x02, 0x03, 0.3f, 100.0f, {50.0f, 0.0f, -10.0f}, {50.0f, 0.0f, -10.0f}},
};

// girlSkirt
u8 girlSkirtP[48] = {93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140};
u8 girlSkirtLp[48] = {96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 93, 94, 95};
u8 girlSkirtUp[48] = {0xFF, 93, 94, 0xFF, 96, 97, 0xFF, 99, 100, 0xFF, 102, 103, 0xFF, 105, 106, 0xFF, 108, 109, 0xFF, 111, 112, 0xFF, 114, 115, 0xFF, 117, 118, 0xFF, 120, 121, 0xFF, 123, 124, 0xFF, 126, 127, 0xFF, 129, 130, 0xFF, 132, 133, 0xFF, 135, 136, 0xFF, 138, 139};
u8 girlSkirtDp[48] = {94, 95, 0xFF, 97, 98, 0xFF, 100, 101, 0xFF, 103, 104, 0xFF, 106, 107, 0xFF, 109, 110, 0xFF, 112, 113, 0xFF, 115, 116, 0xFF, 118, 119, 0xFF, 121, 122, 0xFF, 124, 125, 0xFF, 127, 128, 0xFF, 130, 131, 0xFF, 133, 134, 0xFF, 136, 137, 0xFF, 139, 140, 0xFF};
f32 girlSkirtMax[48] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
f32 girlSkirtMaxEvt[48] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f};
f32 girlSkirtWindS[48] = {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.2f, 1.2f, 1.2f, 1.5f, 1.5f, 1.5f, 1.8f, 1.8f, 1.8f, 2.0f, 2.0f, 2.0f, 2.2f, 2.2f, 2.2f, 2.5f, 2.5f, 2.5f, 2.7f, 2.7f, 2.7f, 3.0f, 3.0f, 3.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.5f, 2.0f, 2.0f, 2.0f};
f32 girlSkirtWindR[48] = {0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f, 0.3f, 0.6f, 1.0f};
CLOTH_AT_SET girlSkirtAt[11] = {
    {0x0000, 0x12, 0x12, 1.0f, 80.0f, {10.0f, -50.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 80.0f, {10.0f, -100.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 80.0f, {10.0f, -150.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 80.0f, {10.0f, -200.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 80.0f, {-10.0f, -50.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 80.0f, {-10.0f, -100.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 80.0f, {-10.0f, -150.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 80.0f, {-10.0f, -200.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 80.0f, {0.0f, -50.0f, -20.0f}, {0.0f, -50.0f, -20.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 80.0f, {0.0f, -100.0f, -20.0f}, {0.0f, -100.0f, -20.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 85.0f, {0.0f, -150.0f, -20.0f}, {0.0f, -150.0f, -20.0f}},
};
CLOTH_AT_SET girlSkirtAtEvt[11] = {
    {0x0000, 0x12, 0x12, 1.0f, 90.0f, {10.0f, -50.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 90.0f, {10.0f, -100.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 90.0f, {10.0f, -150.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 90.0f, {10.0f, -200.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 90.0f, {-10.0f, -50.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 90.0f, {-10.0f, -100.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 90.0f, {-10.0f, -150.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 90.0f, {-10.0f, -200.0f, -20.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 80.0f, {0.0f, -50.0f, -20.0f}, {0.0f, -50.0f, -20.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 80.0f, {0.0f, -100.0f, -20.0f}, {0.0f, -100.0f, -20.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 85.0f, {0.0f, -150.0f, -20.0f}, {0.0f, -150.0f, -20.0f}},
};

// girlSweater
u8 girlSweaterP[8] = {85, 86, 87, 88, 89, 90, 91, 92};
u8 girlSweaterLp[8] = {87, 88, 89, 90, 91, 92, 0xFF, 0xFF};
u8 girlSweaterUp[8] = {0xFF, 85, 0xFF, 87, 0xFF, 89, 0xFF, 91};
u8 girlSweaterDp[8] = {86, 0xFF, 88, 0xFF, 90, 0xFF, 92, 0xFF};
f32 girlSweaterMax[8] = {0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f};
f32 girlSweaterWindS[8] = {0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f};
f32 girlSweaterWindR[8] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
CLOTH_AT_SET girlSweaterAt[6] = {
    {0x0000, 0x02, 0x02, 1.0f, 100.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 100.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.75f, 100.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.75f, 100.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.5f, 100.0f, {-70.0f, 0.0f, 0.0f}, {-70.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x03, 0.5f, 100.0f, {70.0f, 0.0f, 0.0f}, {70.0f, 0.0f, 0.0f}},
};

// girlRibbon
u8 girlRibbonP[12] = {144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155};
u8 girlRibbonUp[12] = {0xFF, 144, 0xFF, 146, 0xFF, 148, 0xFF, 150, 0xFF, 152, 0xFF, 154};
u8 girlRibbonDp[12] = {145, 0xFF, 147, 0xFF, 149, 0xFF, 151, 0xFF, 153, 0xFF, 155, 0xFF};
f32 girlRibbonMax[12] = {0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f, 0.3f, 0.6f};
f32 girlRibbonWindS[12] = {0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f, 1.5f, 1.5f, 2.0f, 2.0f, 2.5f, 2.5f};
f32 girlRibbonWindR[12] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
CLOTH_AT_SET girlRibbonAt[8] = {
    {0x0000, 0x02, 0x02, 1.0f, 110.0f, {-30.0f, 0.0f, 0.0f}, {-30.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 110.0f, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.75f, 110.0f, {-30.0f, 0.0f, 0.0f}, {-30.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.75f, 110.0f, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.5f, 110.0f, {-30.0f, 0.0f, 0.0f}, {-30.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x02, 0.5f, 110.0f, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.2f, 140.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x16, 0.2f, 140.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
};

// luisHair
u8 luisHairP[26] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89};
u8 luisHairUp[26] = {0xFF, 64, 0xFF, 66, 67, 0xFF, 69, 70, 0xFF, 72, 0xFF, 74, 0xFF, 76, 0xFF, 78, 0xFF, 80, 0xFF, 82, 83, 0xFF, 85, 86, 0xFF, 88};
u8 luisHairDp[26] = {65, 0xFF, 67, 68, 0xFF, 70, 71, 0xFF, 73, 0xFF, 75, 0xFF, 77, 0xFF, 79, 0xFF, 81, 0xFF, 83, 84, 0xFF, 86, 87, 0xFF, 89, 0xFF};
f32 luisHairMax[26] = {0.2f, 0.3f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f, 0.4f, 0.2f, 0.3f};
f32 luisHairWindS[26] = {0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.5f, 2.0f, 2.0f, 2.5f, 2.5f, 3.0f, 3.0f, -2.5f, -2.5f, -2.0f, -2.0f, -1.5f, -1.5f, -1.5f, -1.0f, -1.0f, -1.0f, -0.5f, -0.5f};
f32 luisHairWindR[26] = {0.5f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f, 1.0f};
CLOTH_AT_SET luisHairAt[7] = {
    {0x0000, 0x04, 0x04, 1.0f, 75.0f, {0.0f, 50.0f, 8.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 90.0f, 75.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 75.0f, {0.0f, 70.0f, 35.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 65.0f, 80.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 35.0f, 80.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 20.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 90.0f, {0.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
};

// adaDress
u8 adaDressP[146] = {73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218};
u8 adaDressUp[146] = {0xFF, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 0xFF, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 0xFF, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 0xFF, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 0xFF, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 0xFF, 128, 129, 130, 131, 132, 133, 134, 135, 136, 0xFF, 138, 139, 140, 141, 142, 143, 144, 145, 146, 0xFF, 148, 149, 150, 151, 152, 153, 154, 155, 156, 0xFF, 158, 159, 160, 161, 162, 163, 164, 165, 166, 0xFF, 168, 169, 170, 171, 172, 173, 174, 175, 176, 0xFF, 178, 179, 180, 181, 182, 183, 184, 185, 186, 0xFF, 188, 189, 190, 191, 192, 193, 194, 195, 196, 0xFF, 198, 199, 200, 201, 202, 203, 204, 205, 206, 0xFF, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217};
u8 adaDressDp[146] = {74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 0xFF, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 0xFF, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 0xFF, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 0xFF, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 0xFF, 129, 130, 131, 132, 133, 134, 135, 136, 137, 0xFF, 139, 140, 141, 142, 143, 144, 145, 146, 147, 0xFF, 149, 150, 151, 152, 153, 154, 155, 156, 157, 0xFF, 159, 160, 161, 162, 163, 164, 165, 166, 167, 0xFF, 169, 170, 171, 172, 173, 174, 175, 176, 177, 0xFF, 179, 180, 181, 182, 183, 184, 185, 186, 187, 0xFF, 189, 190, 191, 192, 193, 194, 195, 196, 197, 0xFF, 199, 200, 201, 202, 203, 204, 205, 206, 207, 0xFF, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 0xFF};
u8 adaDressLp[146] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207};
u8 adaDressULp[146] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 0xFF, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 0xFF, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 0xFF, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 0xFF, 118, 119, 120, 121, 122, 123, 124, 125, 126, 0xFF, 128, 129, 130, 131, 132, 133, 134, 135, 136, 0xFF, 138, 139, 140, 141, 142, 143, 144, 145, 146, 0xFF, 148, 149, 150, 151, 152, 153, 154, 155, 156, 0xFF, 158, 159, 160, 161, 162, 163, 164, 165, 166, 0xFF, 168, 169, 170, 171, 172, 173, 174, 175, 176, 0xFF, 178, 179, 180, 181, 182, 183, 184, 185, 186, 0xFF, 188, 189, 190, 191, 192, 193, 194, 195, 196, 0xFF, 198, 198, 199, 200, 201, 202, 203, 204, 205, 206};
f32 adaDressMax[146] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 adaDressWindS[146] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.5f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -2.0f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.5f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
f32 adaDressWindR[146] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
CLOTH_AT_SET adaDressAt[35] = {
    {0x0000, 0x12, 0x12, 1.0f, 85.0f, {20.0f, 100.0f, 0.0f}, {20.0f, 100.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 85.0f, {20.0f, 50.0f, 0.0f}, {20.0f, 50.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 85.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.8f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.6f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.4f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x13, 0.2f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x13, 1.0f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.8f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.6f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.4f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x14, 0.2f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x14, 0x14, 1.0f, 95.0f, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 85.0f, {-20.0f, 50.0f, 0.0f}, {-20.0f, 50.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 85.0f, {-20.0f, 100.0f, 0.0f}, {-20.0f, 100.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 85.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.8f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.6f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.4f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x17, 0.2f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x17, 1.0f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.8f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.6f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.4f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x17, 0x18, 0.2f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x18, 0x18, 1.0f, 95.0f, {-20.0f, 0.0f, 0.0f}, {-20.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -50.0f, 0.0f}, {0.0f, -50.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -100.0f, 0.0f}, {0.0f, -100.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -150.0f, 0.0f}, {0.0f, -150.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -200.0f, 0.0f}, {0.0f, -200.0f, 0.0f}},
    {0x0000, 0x12, 0x16, 0.5f, 95.0f, {0.0f, -250.0f, 0.0f}, {0.0f, -250.0f, 0.0f}},
    {0x0000, 0x13, 0x17, 0.5f, 95.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x13, 0x17, 0.5f, 95.0f, {0.0f, -50.0f, 0.0f}, {0.0f, -50.0f, 0.0f}},
    {0x0000, 0x13, 0x17, 0.5f, 95.0f, {0.0f, -100.0f, 0.0f}, {0.0f, -100.0f, 0.0f}},
};

// adaHair
u8 adaHairP[14] = {65, 66, 67, 68, 69, 70, 71, 72, 219, 220, 221, 222, 223, 224};
u8 adaHairUp[14] = {0xFF, 65, 0xFF, 67, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 219, 0xFF, 221, 0xFF, 223};
u8 adaHairDp[14] = {66, 0xFF, 68, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 220, 0xFF, 222, 0xFF, 224, 0xFF};
f32 adaHairMax[14] = {0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f};
f32 adaHairWindS[14] = {0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.0f, -2.5f, -2.5f, -2.0f, -2.0f};
f32 adaHairWindR[14] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
CLOTH_AT_SET adaHairAt[6] = {
    {0x0000, 0x04, 0x04, 1.0f, 65.0f, {0.0f, 50.0f, -10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x03, 0x04, 0.5f, 70.0f, {0.0f, 50.0f, 0.0f}, {0.0f, 50.0f, 0.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 60.0f, {0.0f, 30.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {-5.0f, 100.0f, 30.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {-3.0f, 70.0f, 30.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x04, 0x04, 1.0f, 70.0f, {0.0f, 50.0f, 30.0f}, {0.0f, 0.0f, 0.0f}},
};

// adaRibbon
u8 adaRibbonP[22] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
u8 adaRibbonUp[22] = {0xFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0xFF, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
u8 adaRibbonDp[22] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0xFF, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 0xFF};
f32 adaRibbonMax[22] = {0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 adaRibbonWindS[22] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
f32 adaRibbonWindR[22] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
CLOTH_AT_SET adaRibbonAt[10] = {
    {0x0000, 0x04, 0x04, 1.0f, 90.0f, {0.0f, 0.0f, 20.0f}, {0.0f, 0.0f, 20.0f}},
    {0x0000, 0x03, 0x03, 1.0f, 70.0f, {0.0f, 0.0f, 20.0f}, {0.0f, 0.0f, 20.0f}},
    {0x0000, 0x03, 0x02, 0.666f, 130.0f, {0.0f, 0.0f, 30.0f}, {0.0f, 0.0f, 30.0f}},
    {0x0000, 0x03, 0x02, 0.333f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x02, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x01, 0.666f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x02, 0x01, 0.333f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x01, 0x01, 1.0f, 130.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x06, 0x06, 1.0f, 130.0f, {0.0f, 0.0f, 30.0f}, {0.0f, 0.0f, 30.0f}},
    {0x0000, 0x0C, 0x0C, 1.0f, 130.0f, {0.0f, 0.0f, 30.0f}, {0.0f, 0.0f, 30.0f}},
};

// The callers pass (&leonHair, &leonJacket, &leonHolster); the works are used as jacket, holster, hair.
void PlClothSetLeon(cModel* pl, PlCloth* pCloth1, PlCloth* pCloth2, PlCloth* pCloth3)
{
    testJacketSetLeon(pl, pCloth1);
    testHolsterSetLeon(pl, pCloth2);
    testHairSetLeon(pl, pCloth3);
}

// Per frame (cPlLeon::moveCloth): simulates the jacket, holster and hair, then clears the model's
// warp / no-cloth flags (be_flag 0x00E00000) that reset the chains this frame.
void PlClothMoveLeon(cModel* pl, PlCloth* pCloth1, PlCloth* pCloth2, PlCloth* pCloth3)
{
    testJacketMoveLeon(pl, pCloth1);
    testHolsterMoveLeon(pl, pCloth2);
    testHairMoveLeon(pl, pCloth3);
    pl->be_flag &= ~0x00E00000;
}

// Likewise (&girlHair, &girlSkirt, &girlSweater) are used as skirt, hair, sweater (or ribbon).
void PlClothSetGirl(cModel* pl, PlCloth* pCloth1, PlCloth* pCloth2, PlCloth* pCloth3, int mode)
{
    testHairSetGirl(pl, pCloth2, mode);
    if (pG->game_costume == 1) {
        testRibbonSetGirl(pl, pCloth3);
    } else {
        testSkirtSetGirl(pl, pCloth1, mode);
        testSweaterSetGirl(pl, pCloth3);
    }
}

// Per frame (cPlAshley::moveCloth / the Ashley NPC): hair, then skirt + sweater, or ribbon + lapels
// for the alternate costume (game_costume 1); clears be_flag 0x00E00000.
void PlClothMoveGirl(cModel* pl, PlCloth* pCloth1, PlCloth* pCloth2, PlCloth* pCloth3)
{
    testHairMoveGirl(pl, pCloth2);
    if (pG->game_costume == 1) {
        testRibbonMoveGirl(pl, pCloth3);
        girlLapelMove(pl);
    } else {
        testSkirtMoveGirl(pl, pCloth1);
        testSweaterMoveGirl(pl, pCloth3);
    }
    pl->be_flag &= ~0x00E00000;
}

// Luis: hair chain only.
void PlClothSetLuis(cModel* pl, PlCloth* pCloth1)
{
    testHairSetLuis(pl, pCloth1);
}

// Luis per frame: hair; clears be_flag 0x00E00000.
void PlClothMoveLuis(cModel* pl, PlCloth* pCloth1)
{
    testHairMoveLuis(pl, pCloth1);
    pl->be_flag &= ~0x00E00000;
}

// Ada: dress (fewer bundles in events) and hair; the ribbon is a separate chain object (AdaRibbonSet).
void PlClothSetAda(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair, int evt)
{
    testDressSetAda(pl, dress, evt);
    testHairSetAda(pl, hair);
}

// Ada per frame: dress and hair; clears be_flag 0x00E00000.
void PlClothMoveAda(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair)
{
    testDressMoveAda(pl, dress);
    testHairMoveAda(pl, hair);
    pl->be_flag &= ~0x00E00000;
}

// Leon's hair: 21 links, 4 collision spheres, gravity 15, Flag 0x302 (no floor, border collision).
void testHairSetLeon(cModel* pl, PlCloth* pCloth)
{
    pCloth->Num = 21;
    pCloth->pCloth = leonHairP;
    pCloth->pLeft = 0;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = leonHairUp;
    pCloth->pChild = leonHairDp;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->pMax = leonHairMax;
    pCloth->pWindSin = leonHairWindS;
    pCloth->pWindRate = leonHairWindR;
    pCloth->pAtset = leonHairAt;
    pCloth->At_num = 4;
    pCloth->Gravity = 15.0f;
    pCloth->Rate = 0.75f;
    pCloth->Bundle_num = 4;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 1.0f;
    pCloth->Move_rate = 0.5f;
    pCloth->pModel = 0;
    pCloth->Flag = 0x302;
    pCloth->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) pCloth, 100.0f);
}

// Hair: the plain simulation.
void testHairMoveLeon(cModel* pl, PlCloth* pCloth)
{
    PenClothMove(pl, (PenCloth*) pCloth);
}

// Leon's jacket (costume 0 only): 24 links, 6 volumes, gravity 25, soft constraints (Stretchy 0.1).
void testJacketSetLeon(cModel* pl, PlCloth* pCloth)
{
    if (pG->pl_costume != 0) {
        return;
    }
    pCloth->Num = 24;
    pCloth->pCloth = leonJacketP;
    pCloth->pLeft = leonJacketLp;
    pCloth->pRight = leonJacketRp;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = leonJacketUp;
    pCloth->pChild = leonJacketDp;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->pMax = leonJacketMax;
    pCloth->pWindSin = leonJacketWindS;
    pCloth->pWindRate = leonJacketWindR;
    pCloth->pAtset = leonJacketAt;
    pCloth->At_num = 6;
    pCloth->Gravity = 25.0f;
    pCloth->pModel = 0;
    pCloth->Rate = 0.5f;
    pCloth->Bundle_num = 4;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 0.1f;
    pCloth->Move_rate = 0.5f;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) pCloth, 100.0f);
}

// Jacket (costume 0): the Move3 variant with parallel collision.
void testJacketMoveLeon(cModel* pl, PlCloth* pCloth)
{
    if (pG->pl_costume == 0) {
        PenClothMove3(pl, (PenCloth*) pCloth);
    }
}

// Leon's holster strap (costume 1, no jacket): a 2-link chain with one volume.
void testHolsterSetLeon(cModel* pl, PlCloth* pCloth)
{
    if (pG->pl_costume != 1) {
        return;
    }
    pCloth->Num = 2;
    pCloth->pCloth = leonHolsterP;
    pCloth->pLeft = leonHolsterLp;
    pCloth->pRight = leonHolsterRp;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = leonHolsterUp;
    pCloth->pChild = leonHolsterDp;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->pMax = leonHolsterMax;
    pCloth->pWindSin = leonHolsterWindS;
    pCloth->pWindRate = leonHolsterWindR;
    pCloth->pAtset = leonHolsterAt;
    pCloth->At_num = 1;
    pCloth->Gravity = 25.0f;
    pCloth->Rate = 0.7f;
    pCloth->Bundle_num = 4;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 1.0f;
    pCloth->Move_rate = 0.5f;
    pCloth->pModel = 0;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) pCloth, 100.0f);
}

// Holster (costume 1): Move3.
void testHolsterMoveLeon(cModel* pl, PlCloth* pCloth)
{
    if (pG->pl_costume == 1) {
        PenClothMove3(pl, (PenCloth*) pCloth);
    }
}

// Ashley's hair: 21 links, 7 collision spheres in events (evt) else 5, Flag 0x302.
void testHairSetGirl(cModel* pl, PlCloth* pCloth, int mode)
{
    pCloth->Num = 21;
    pCloth->pCloth = girlHairP;
    pCloth->pLeft = 0;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = girlHairUp;
    pCloth->pChild = girlHairDp;
    pCloth->pWindSin = girlHairWindS;
    pCloth->pWindRate = girlHairWindR;
    if (mode) {
        pCloth->pMax = girlHairMaxEvt;
        pCloth->pAtset = girlHairAtEvt;
        pCloth->At_num = 7;
    } else {
        pCloth->pMax = girlHairMax;
        pCloth->pAtset = girlHairAt;
        pCloth->At_num = 5;
    }
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->Gravity = 15.0f;
    pCloth->Rate = 0.75f;
    pCloth->Bundle_num = 4;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 1.0f;
    pCloth->Move_rate = 0.5f;
    pCloth->pModel = 0;
    pCloth->Flag = 0x302;
    pCloth->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) pCloth, 100.0f);
}

// Hair: the plain simulation.
void testHairMoveGirl(cModel* pl, PlCloth* pCloth)
{
    PenClothMove(pl, (PenCloth*) pCloth);
}

// Ashley's skirt: 48 links (a ring of 4 x 12) with 11 volumes (a different set in events).
void testSkirtSetGirl(cModel* pl, PlCloth* pCloth, int mode)
{
    pCloth->Num = 48;
    pCloth->pCloth = girlSkirtP;
    pCloth->pLeft = girlSkirtLp;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = girlSkirtUp;
    pCloth->pChild = girlSkirtDp;
    if (mode) {
        pCloth->pMax = girlSkirtMaxEvt;
        pCloth->pAtset = girlSkirtAtEvt;
        pCloth->At_num = 11;
    } else {
        pCloth->pMax = girlSkirtMax;
        pCloth->pAtset = girlSkirtAt;
        pCloth->At_num = 11;
    }
    pCloth->pWindSin = girlSkirtWindS;
    pCloth->pWindRate = girlSkirtWindR;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->Gravity = 10.0f;
    pCloth->Rate = 0.9f;
    pCloth->Bundle_num = 4;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 1.0f;
    pCloth->Move_rate = 0.5f;
    pCloth->pModel = 0;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) pCloth, 100.0f);
}

// Skirt: Move3, stiffer while Status_flg[1] 0x200000 (Ashley carried / on the ladder).
void testSkirtMoveGirl(cModel* pl, PlCloth* pCloth)
{
    if (StaFlagChk(pG, STA_PL_BOAT)) {
        pCloth->Move_rate = 0.9f;
    } else {
        pCloth->Move_rate = 0.5f;
    }
    PenClothMove3(pl, (PenCloth*) pCloth);
}

// Ashley's sweater hem: 8 links, 6 volumes.
void testSweaterSetGirl(cModel* pl, PlCloth* c)
{
    c->Num = 8;
    c->pCloth = girlSweaterP;
    c->pLeft = girlSweaterLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->pUpRight = 0;
    c->pParent = girlSweaterUp;
    c->pChild = girlSweaterDp;
    c->pGravity = 0;
    c->pRate = 0;
    c->pMax = girlSweaterMax;
    c->pWindSin = girlSweaterWindS;
    c->pWindRate = girlSweaterWindR;
    c->pAtset = girlSweaterAt;
    c->At_num = 6;
    c->Gravity = 10.0f;
    c->Rate = 0.7f;
    c->Bundle_num = 4;
    c->WindSin = 0.0f;
    c->Stretchy = 0.1f;
    c->Move_rate = 0.5f;
    c->pModel = 0;
    c->Flag = 0x100;
    c->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

// Sweater: the plain simulation.
void testSweaterMoveGirl(cModel* pl, PlCloth* c)
{
    PenClothMove(pl, (PenCloth*) c);
}

// Ashley's alternate costume ribbon: 12 links, 8 volumes, gravity 10, soft constraints.
void testRibbonSetGirl(cModel* pl, PlCloth* pCloth)
{
    pCloth->Num = 12;
    pCloth->pCloth = girlRibbonP;
    pCloth->pLeft = 0;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = girlRibbonUp;
    pCloth->pChild = girlRibbonDp;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->pMax = girlRibbonMax;
    pCloth->pWindSin = girlRibbonWindS;
    pCloth->pWindRate = girlRibbonWindR;
    pCloth->pAtset = girlRibbonAt;
    pCloth->At_num = 8;
    pCloth->Gravity = 10.0f;
    pCloth->Rate = 0.7f;
    pCloth->Bundle_num = 4;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 0.1f;
    pCloth->Move_rate = 0.5f;
    pCloth->pModel = 0;
    pCloth->Flag = 0x100;
    pCloth->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) pCloth, 100.0f);
}

// Ribbon: the plain simulation.
void testRibbonMoveGirl(cModel* pl, PlCloth* pCloth)
{
    PenClothMove(pl, (PenCloth*) pCloth);
}

// One lapel of Ashley's alternate costume: the part is rotated away from the body by the angle the
// chest (two parts, weighted) rises above it, then bent around the local Y and Z axes. The two
// lapels are written out (the second copy's pointer locals become gcse copies of the first's).
// A plain block, not do/while(0): a loop note makes haifa treat the next insn as a full barrier,
// which would pin the PRE copies behind the second lapel's first call. The inner block re-derives
// `pm1`, so the first lapel's later uses go through the PRE copy and `pm1` itself dies in bb 0.
#define LAPEL_MOVE(no, pa, pb, lift, sy, sz)                                        \
    {                                                                               \
        f32 ang;                                                                    \
        p = pl->getPartsPtr(no);                                                    \
        PSMTXIdentity(p->l_mat);                                                 \
        TransMatrix(p->l_mat, &p->pos);                                          \
        PSMTXConcat(p->pParent->mat, p->l_mat, p->mat);                          \
        p->world.x = p->mat[0][3];                                               \
        p->world.y = p->mat[1][3];                                               \
        p->world.z = p->mat[2][3];                                               \
        PSMTXConcat(p->mat, rotm, pm1);                                             \
        PSMTXInverse(pm1, pinv);                                                    \
        a = pl->getPartsPtr(pa);                                                    \
        b = pl->getPartsPtr(pb);                                                    \
        PosToPos(&a->world, &b->world, pv, lapel_weight);                     \
        PSMTXMultVec(pinv, pv, pv);                                                 \
        v.y += lift;                                                                \
        if (v.y > 0.0f) {                                                           \
            ang = asinf(v.y / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z));            \
        } else {                                                                    \
            ang = 0.0f;                                                             \
        }                                                                           \
        {                                                                           \
            f32 (*pm1)[4] = m1;                                                     \
            f32 (*pm3)[4] = m3;                                                      \
            Vec* pax = &axis;                                                       \
            rot.x = -ang * lapel_rate_x;                                            \
            rot.y = sy ang * lapel_rate_y;                                          \
            rot.z = sz ang * lapel_rate_z;                                          \
            PSMTXRotRad(p->l_mat, 'x', rot.x);                                   \
            TransMatrix(p->l_mat, &p->pos);                                      \
            PSMTXConcat(p->pParent->mat, p->l_mat, p->mat);                      \
            PSMTXConcat(p->mat, rotm, pm1);                                         \
            axis.x = 0.0f;                                                          \
            axis.y = 1.0f;                                                          \
            axis.z = 0.0f;                                                          \
            PSMTXMultVecSR(pm1, pax, pax);                                          \
            PSMTXRotAxisRad(pm3, pax, rot.y);                                       \
            PSMTXConcat(pm3, p->mat, p->mat);                                       \
            TransMatrix(p->mat, &p->world);                                      \
            PSMTXConcat(p->mat, rotm, pm1);                                         \
            axis.x = 0.0f;                                                          \
            axis.y = 0.0f;                                                          \
            axis.z = 1.0f;                                                          \
            PSMTXMultVecSR(pm1, pax, pax);                                          \
            PSMTXRotAxisRad(pm3, pax, rot.z);                                       \
            PSMTXConcat(pm3, p->mat, p->mat);                                       \
            TransMatrix(p->mat, &p->world);                                      \
        }                                                                           \
    }

// Ashley's alternate-costume lapels (parts 0x8D / 0x8E): each is tilted away from the chest by the
// angle the chest (weighted parts 7/8 and 0xD/0xE) rises above it, then bent about its Y and Z axes.
void girlLapelMove(cModel* pl)
{
    static f32 lapel_rr = 50.0f;
    static f32 lapel_rl = 50.0f;
    static char lapel_name[3] = "CH";
    static f32 lapel_rate_x = 1.3f;
    static f32 lapel_rate_y = 0.3f;
    static f32 lapel_rate_z = 0.6f;
    static f32 lapel_weight = 0.32f;
    Mtx rotm;
    Mtx m1;
    Mtx inv;
    Mtx m3;
    Vec v;
    Vec axis;
    Vec rot;
    cModel* p;
    cModel* a;
    cModel* b;
    f32 (*pm1)[4] = m1;
    f32 (*pinv)[4] = inv;
    Vec* pv = &v;

    PSMTXRotRad(rotm, 'x', 0.75049156f);
    pl->getPartsPtr(4);
    LAPEL_MOVE(0x8D, 7, 8, lapel_rr, -, -);
    {
        f32 (*pm1)[4] = m1;
        f32 (*pinv)[4] = inv;
        Vec* pv = &v;

        LAPEL_MOVE(0x8E, 0xD, 0xE, lapel_rl, +, +);
    }
}

// Luis's hair: 26 links.
void testHairSetLuis(cModel* pl, PlCloth* pCloth)
{
    pCloth->Num = 26;
    pCloth->pCloth = luisHairP;
    pCloth->pLeft = 0;
    pCloth->pRight = 0;
    pCloth->pUpLeft = 0;
    pCloth->pUpRight = 0;
    pCloth->pParent = luisHairUp;
    pCloth->pChild = luisHairDp;
    pCloth->pGravity = 0;
    pCloth->pRate = 0;
    pCloth->pMax = luisHairMax;
    pCloth->pWindSin = luisHairWindS;
    pCloth->pWindRate = luisHairWindR;
    pCloth->pAtset = luisHairAt;
    pCloth->At_num = 7;
    pCloth->Gravity = 15.0f;
    pCloth->Rate = 0.75f;
    pCloth->Bundle_num = 4;
    pCloth->WindSin = 0.0f;
    pCloth->Stretchy = 1.0f;
    pCloth->Move_rate = 0.5f;
    pCloth->pModel = 0;
    pCloth->Flag = 0x302;
    pCloth->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) pCloth, 100.0f);
}

// Hair: the plain simulation.
void testHairMoveLuis(cModel* pl, PlCloth* pCloth)
{
    PenClothMove(pl, (PenCloth*) pCloth);
}

// The zero stores come out in source order because none of them is the zero's last use: `x54 = 0`
// is the last one (issued first). x44 is stored directly in both arms (jump2 folds the two `li`s
// into `li 10; beq; li 2` after reload, so nothing is hoisted above the zero stores).
void testDressSetAda(cModel* pl, PlCloth* c, int evt)
{
    f32 rate;

    c->Num = 146;
    c->pCloth = adaDressP;
    c->pLeft = adaDressLp;
    c->pUpLeft = adaDressULp;
    c->pParent = adaDressUp;
    c->pChild = adaDressDp;
    c->pMax = adaDressMax;
    c->pWindSin = adaDressWindS;
    c->pWindRate = adaDressWindR;
    c->pAtset = adaDressAt;
    c->At_num = 35;
    c->Gravity = 20.0f;
    rate = 0.7f;
    c->WindSin = 0.0f;
    c->Stretchy = 1.0f;
    c->pRight = 0;
    c->pUpRight = 0;
    c->pGravity = 0;
    c->pRate = 0;
    c->pModel = 0;
    c->Rate = rate;
    c->Flag = 0;
    c->pPtbl = 0;
    if (evt) {
        c->Move_rate = rate;
        c->Bundle_num = 2;
    } else {
        c->Move_rate = rate;
        c->Bundle_num = 10;
    }
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

// Dress: Move3.
void testDressMoveAda(cModel* pl, PlCloth* c)
{
    PenClothMove3(pl, (PenCloth*) c);
}

// Ada's hair: 14 links, 6 volumes, Flag 0x302.
void testHairSetAda(cModel* pl, PlCloth* c)
{
    c->Num = 14;
    c->pCloth = adaHairP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->pUpRight = 0;
    c->pParent = adaHairUp;
    c->pChild = adaHairDp;
    c->pGravity = 0;
    c->pRate = 0;
    c->pWindSin = adaHairWindS;
    c->pWindRate = adaHairWindR;
    c->pMax = adaHairMax;
    c->pAtset = adaHairAt;
    c->At_num = 6;
    c->Gravity = 15.0f;
    c->Rate = 0.75f;
    c->Bundle_num = 4;
    c->WindSin = 0.0f;
    c->Stretchy = 1.0f;
    c->Move_rate = 0.5f;
    c->pModel = 0;
    c->Flag = 0x302;
    c->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

// Hair: the plain simulation.
void testHairMoveAda(cModel* pl, PlCloth* c)
{
    PenClothMove(pl, (PenCloth*) c);
}

// Ada's dress ribbon as a cObjChain (its own model bin/tpl, 22 links, 10 volumes, Flag 0x200) hung
// on parts 0x40 of the player. Returns 0 when the model data is missing.
cObjChain* AdaRibbonSet(cModel* pl, PlCloth* c, void* bin, void* tpl)
{
    cObjChain* chain;
    Vec pos;
    Vec rot;
    f32 zero;

    if (bin == 0 || tpl == 0) {
        return 0;
    }
    c->Num = 22;
    c->pCloth = adaRibbonP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->pUpRight = 0;
    c->pParent = adaRibbonUp;
    c->pChild = adaRibbonDp;
    c->pGravity = 0;
    c->pRate = 0;
    c->pMax = adaRibbonMax;
    c->Gravity = 10.0f;
    c->Rate = 0.8f;
    c->Bundle_num = 0;
    zero = 0.0f;
    c->Move_rate = 0.5f;
    c->Stretchy = 1.0f;
    c->pPtbl = 0;
    c->pWindSin = adaRibbonWindS;
    c->pWindRate = adaRibbonWindR;
    c->pAtset = adaRibbonAt;
    c->At_num = 10;
    c->Flag = 0x200;
    c->pModel = pl;
    c->WindSin = zero;
    pos.x = zero;
    pos.y = zero;
    pos.z = zero;
    rot.x = zero;
    rot.y = zero;
    rot.z = zero;
    chain = SetChain(bin, tpl, &pos, &rot);
    chain->setChain((PenCloth*) c);
    pos.x = zero;
    pos.y = zero;
    pos.z = zero;
    chain->setParent(pl, 0x40, &pos, 0);
    return chain;
}
