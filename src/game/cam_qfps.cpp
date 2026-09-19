// game/cam_qfps.cpp: the over-the-shoulder ("quasi FPS") camera, the default camera of the game.
// Per player character / weapon / state a table of QfpsOfs offsets (camera position, a close
// point the camera may not pass, target, roll, fov; left / right x up / mid / down sites) is
// applied in the player's frame, blended between tables on a type change, tilted by the floor
// slope, aimed by the C-stick, and pulled in front of the scenery / characters by hitCheck. Rooms
// may override the tables through a camera area cut (setAreaData).

#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_qfps.h"
#include "atari.h"
#include "light.h"
#include "db_log.h"
#include "math_sub.h"
#include "main.h"
#include "main_mem.h"
#include "model.h"
#include "player.h"
#include "pl_sub.h"
#include "dbmodule.h"
#include "view.h"

u32 SubCharGetStatus();

extern "C" {
void offsetCorrection(QfpsOfs* o);
static void offsetArrayCorrection(QfpsOfs (*o)[3]);
}

#define PI 3.1415927f

static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void S16Set(s16& d, s16 v) { d = v; }

f32 g_crouch_cam_z_back = 600.0f;
static f32 g_crouch_cam_y_down = 400.0f;

QfpsOfs g_readyOfs[16][2][3] = {
    {
        {
            {{-527.0f, 600.0f, -680.0f}, {-265.0f, 1280.0f, -350.0f}, {-220.0f, 4080.0f, 1100.0f}, 0.0f, 45.0f},
            {{-530.0f, 1765.0f, -590.0f}, {-260.0f, 1630.0f, -130.0f}, {-65.0f, 1340.0f, 1480.0f}, 0.0f, 45.0f},
            {{-393.0f, 2058.0f, -5.0f}, {-250.0f, 1860.0f, -65.0f}, {-179.0f, 365.0f, 943.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-470.0f, 250.0f, -700.0f}, {-265.0f, 1280.0f, -350.0f}, {-385.0f, 3500.0f, 800.0f}, 0.0f, 45.0f},
            {{-560.0f, 1618.0f, -1140.0f}, {-260.0f, 1630.0f, -130.0f}, {-370.0f, 1338.0f, 1285.0f}, 0.0f, 45.0f},
            {{-563.0f, 2339.0f, -254.0f}, {-250.0f, 1860.0f, -65.0f}, {-205.0f, 381.0f, 1010.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-400.0f, 600.0f, -2600.0f}, {-265.0f, 1280.0f, -350.0f}, {0.0f, 2770.0f, 1380.0f}, 0.0f, 45.0f},
            {{-400.0f, 2300.0f, -2760.0f}, {-260.0f, 1630.0f, -130.0f}, {0.0f, 1210.0f, 1410.0f}, 0.0f, 45.0f},
            {{-400.0f, 2800.0f, -1300.0f}, {-250.0f, 1860.0f, -65.0f}, {0.0f, 500.0f, 1500.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-527.0f, 867.0f, -681.0f}, {-265.0f, 1280.0f, -350.0f}, {-163.0f, 3084.0f, 940.0f}, 0.0f, 45.0f},
            {{-700.0f, 1800.0f, -1000.0f}, {-260.0f, 1630.0f, -130.0f}, {-65.0f, 1440.0f, 1480.0f}, 0.0f, 45.0f},
            {{-393.0f, 2058.0f, -5.0f}, {-250.0f, 1860.0f, -65.0f}, {-179.0f, 365.0f, 943.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-800.0f, 330.0f, -1185.0f}, {-265.0f, 1280.0f, -350.0f}, {-275.0f, 3290.0f, 1140.0f}, 0.0f, 45.0f},
            {{-635.0f, 1710.0f, -1455.0f}, {-260.0f, 1630.0f, -130.0f}, {-155.0f, 1510.0f, 1530.0f}, 0.0f, 45.0f},
            {{-648.0f, 2360.0f, -905.0f}, {-250.0f, 1860.0f, -65.0f}, {-189.0f, 495.0f, 1160.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-408.0f, 660.0f, -538.0f}, {-292.0f, 1171.0f, -257.0f}, {-187.0f, 3952.0f, 1323.0f}, 0.0f, 45.0f},
            {{-460.0f, 1450.0f, -767.0f}, {-216.0f, 1478.0f, -148.0f}, {-150.0f, 1600.0f, 1500.0f}, 0.0f, 45.0f},
            {{-231.0f, 1991.0f, -107.0f}, {-174.0f, 1456.0f, 197.0f}, {-392.0f, 322.0f, 867.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-408.0f, 660.0f, -538.0f}, {-292.0f, 1171.0f, -257.0f}, {-187.0f, 3952.0f, 1323.0f}, 0.0f, 45.0f},
            {{-460.0f, 1450.0f, -767.0f}, {-216.0f, 1478.0f, -148.0f}, {-150.0f, 1600.0f, 1500.0f}, 0.0f, 45.0f},
            {{-231.0f, 1991.0f, -107.0f}, {-174.0f, 1456.0f, 197.0f}, {-392.0f, 322.0f, 867.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-408.0f, 660.0f, -538.0f}, {-292.0f, 1171.0f, -257.0f}, {-187.0f, 3952.0f, 1323.0f}, 0.0f, 45.0f},
            {{-460.0f, 1450.0f, -767.0f}, {-216.0f, 1478.0f, -148.0f}, {-150.0f, 1600.0f, 1500.0f}, 0.0f, 45.0f},
            {{-231.0f, 1991.0f, -107.0f}, {-174.0f, 1456.0f, 197.0f}, {-392.0f, 322.0f, 867.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-595.0f, 428.0f, -1021.0f}, {-26.0f, 1562.0f, -320.0f}, {-223.0f, 4065.0f, 1122.0f}, 0.0f, 45.0f},
            {{-630.0f, 1850.0f, -1018.0f}, {-260.0f, 1630.0f, -128.0f}, {-67.0f, 1340.0f, 1483.0f}, 0.0f, 45.0f},
            {{-547.0f, 2757.0f, -637.0f}, {-401.0f, 1557.0f, -320.0f}, {-184.0f, 370.0f, 966.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-353.0f, 1045.0f, -918.0f}, {-129.0f, 1425.0f, -312.0f}, {264.0f, 2917.0f, 2420.0f}, 0.0f, 45.0f},
            {{-348.0f, 1590.0f, -1400.0f}, {-221.0f, 1612.0f, -320.0f}, {-13.0f, 1061.0f, 1514.0f}, 0.0f, 45.0f},
            {{-391.0f, 2467.0f, -732.0f}, {-80.0f, 1720.0f, -50.0f}, {-158.0f, 450.0f, 1065.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{228.0f, 990.0f, -1000.0f}, {136.0f, 1302.0f, -248.0f}, {364.0f, 2027.0f, 2722.0f}, 0.0f, 50.0f},
            {{240.0f, 1605.0f, -1152.0f}, {166.0f, 1718.0f, -320.0f}, {290.0f, 943.0f, 1458.0f}, 0.0f, 50.0f},
            {{205.0f, 2443.0f, -632.0f}, {235.0f, 1720.0f, -17.0f}, {350.0f, 427.0f, 1175.0f}, 0.0f, 50.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-527.0f, 600.0f, -680.0f}, {-265.0f, 1280.0f, -350.0f}, {-220.0f, 4080.0f, 1100.0f}, 0.0f, 45.0f},
            {{-530.0f, 1765.0f, -590.0f}, {-260.0f, 1630.0f, -130.0f}, {-65.0f, 1340.0f, 1480.0f}, 0.0f, 45.0f},
            {{-393.0f, 2058.0f, -5.0f}, {-250.0f, 1860.0f, -65.0f}, {-179.0f, 365.0f, 943.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-527.0f, 600.0f, -680.0f}, {-265.0f, 1280.0f, -350.0f}, {-220.0f, 4080.0f, 1100.0f}, 0.0f, 45.0f},
            {{-530.0f, 1765.0f, -590.0f}, {-260.0f, 1630.0f, -130.0f}, {-65.0f, 1340.0f, 1480.0f}, 0.0f, 45.0f},
            {{-393.0f, 2058.0f, -5.0f}, {-250.0f, 1860.0f, -65.0f}, {-179.0f, 365.0f, 943.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-527.0f, 600.0f, -680.0f}, {-265.0f, 1280.0f, -350.0f}, {-220.0f, 4080.0f, 1100.0f}, 0.0f, 45.0f},
            {{-530.0f, 1765.0f, -590.0f}, {-260.0f, 1630.0f, -130.0f}, {-65.0f, 1340.0f, 1480.0f}, 0.0f, 45.0f},
            {{-393.0f, 2058.0f, -5.0f}, {-250.0f, 1860.0f, -65.0f}, {-179.0f, 365.0f, 943.0f}, 0.0f, 45.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
};

QfpsOfs g_transOfs[7][2][3] = {
    {
        {
            {{-500.0f, 885.0f, -1050.0f}, {-240.0f, 1550.0f, -150.0f}, {0.0f, 2585.0f, 1390.0f}, 0.0f, 50.0f},
            {{-500.0f, 1765.0f, -1190.0f}, {-180.0f, 1700.0f, -110.0f}, {0.0f, 1340.0f, 1480.0f}, 0.0f, 50.0f},
            {{-500.0f, 2420.0f, -680.0f}, {-200.0f, 1870.0f, -185.0f}, {0.0f, 780.0f, 1250.0f}, 0.0f, 50.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-493.0f, 1270.0f, -1197.0f}, {-240.0f, 1550.0f, -150.0f}, {15.0f, 2092.0f, 1467.0f}, 0.0f, 50.0f},
            {{-785.0f, 1690.0f, -2685.0f}, {-180.0f, 1700.0f, -110.0f}, {0.0f, 1310.0f, 1530.0f}, 0.0f, 50.0f},
            {{-480.0f, 2123.0f, -975.0f}, {-200.0f, 1870.0f, -185.0f}, {36.0f, 944.0f, 1426.0f}, 0.0f, 50.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-500.0f, 885.0f, -1050.0f}, {-240.0f, 1550.0f, -150.0f}, {0.0f, 2585.0f, 1390.0f}, 0.0f, 50.0f},
            {{-565.0f, 1413.0f, -1483.0f}, {-180.0f, 1700.0f, -110.0f}, {-25.0f, 1242.0f, 1401.0f}, 0.0f, 50.0f},
            {{-500.0f, 2420.0f, -680.0f}, {-200.0f, 1870.0f, -185.0f}, {0.0f, 780.0f, 1250.0f}, 0.0f, 50.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-500.0f, 885.0f, -1050.0f}, {-240.0f, 1550.0f, -150.0f}, {0.0f, 2585.0f, 1390.0f}, 0.0f, 50.0f},
            {{-643.0f, 1215.0f, -1703.0f}, {-180.0f, 1700.0f, -110.0f}, {-46.0f, 1345.0f, 1482.0f}, 0.0f, 50.0f},
            {{-500.0f, 2420.0f, -680.0f}, {-200.0f, 1870.0f, -185.0f}, {0.0f, 780.0f, 1250.0f}, 0.0f, 50.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{-500.0f, 885.0f, -1050.0f}, {-240.0f, 1550.0f, -150.0f}, {0.0f, 2585.0f, 1390.0f}, 0.0f, 50.0f},
            {{-624.0f, 1679.0f, -1588.0f}, {-180.0f, 1700.0f, -110.0f}, {-49.0f, 1340.0f, 1485.0f}, 0.0f, 50.0f},
            {{-500.0f, 2420.0f, -680.0f}, {-200.0f, 1870.0f, -185.0f}, {0.0f, 780.0f, 1250.0f}, 0.0f, 50.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
    {
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
        {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f},
        },
    },
};

// ---------------------------------------------------------------------------

void CameraQuasiFPS::LRinfo(void* p)
{
    lr_info = p;
}

// Left / right shoulder check placeholder: always 0 (right shoulder).
int CameraQuasiFPS::LRcheck()
{
    return 0;
}

// Aim angles while ready: with a weapon the pitch ratio comes from the weapon's pitch, otherwise
// from the C-stick Y (clamped -1..1) and the yaw from the C-stick X within the left / right
// limits; both decay by 0.9 per frame when not aiming.
void CameraQuasiFPS::calcDepressionRatio()
{
    static f32 ANGLE_LEFT_LIMIT = 1.0471976f;
    static f32 ANGLE_RIGHT_LIMIT = 1.0471976f;
    static f32 C_RANGE = 59.0f;
    cPlayer* pl = pPL;

    if (pl->isKamae()) {
        angle_y = pl->Wep->getPitch();
        angle_x = 0.0f;
    } else if ((f32) Key.substickX != 0.0f || (f32) Key.substickY != 0.0f) {
        f32 t;

        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
            angle_y = -((f32) Key.substickY / C_RANGE);
        } else {
            angle_y = (f32) Key.substickY / C_RANGE;
        }
        // reference store: keeps the C_RANGE load below it (issued after the Key byte load)
        FSet(angle_y, angle_y < -1.0f ? -1.0f : (angle_y > 1.0f ? 1.0f : angle_y));
        t = -(f32) Key.substickX / C_RANGE;
        if (t < 0.0f) {
            angle_x = ANGLE_LEFT_LIMIT * t;
        } else {
            angle_x = ANGLE_RIGHT_LIMIT * t;
        }
    } else {
        angle_y *= 0.9f;
        angle_x *= 0.9f;
    }
}

// Stores the player matrix and floor normal pointer the base matrix is built from.
void CameraQuasiFPS::setPlayerLocation(Mtx m, Vec* nrm)
{
    PSMTXCopy(m, m_pl_mat);
    m_p_floor_norm = nrm;
}

// Column `c` of a matrix as a vector.
static inline void getColumn(Mtx m, int c, Vec* v)
{
    v->x = m[0][c];
    v->y = m[1][c];
    v->z = m[2][c];
}

// Builds a matrix from four column vectors.
static inline void setColumns(Mtx m, Vec* c0, Vec* c1, Vec* c2, Vec* c3)
{
    m[0][0] = c0->x; m[1][0] = c0->y; m[2][0] = c0->z;
    m[0][1] = c1->x; m[1][1] = c1->y; m[2][1] = c1->z;
    m[0][2] = c2->x; m[1][2] = c2->y; m[2][2] = c2->z;
    m[0][3] = c3->x; m[1][3] = c3->y; m[2][3] = c3->z;
}

// The player-space frame the shoulder offsets are applied in: the player's matrix (or the
// stored one after the search delay), one-shot translation / look-direction overrides, the up
// axis tilted toward the floor normal by the floor ratio while not aiming, and the crouch drop
// (g_crouch_cam_y_down / z_back) when the player is crouching.
void CameraQuasiFPS::calcBaseMatrix(Mtx m)
{
    static f32 s_ratio = 0.33333334f;
    static f32 f = 0.9f;
    static f32 y_down = 800.0f;
    cPlayer* pl = pPL;
    Vec v0;
    Vec v1;
    Vec v2;
    Vec v3;
    Vec v4;
    Vec v5;

    m_search_cnt++;
    if (m_search_cnt > m_search_frame) {
        m_search_cnt = m_search_frame;
        m_state |= 1;
    } else {
        getColumn(m_pl_mat, 3, &v0);
        PSVECSubtract(&m_Aim, &v0, &v1);
        v2.x = 0.0f;
        v2.y = atan2f(v1.x, v1.z);
        v2.z = 0.0f;
        PSMTXIdentity(m);
        RotMatrix(m, &v2);
        TransMatrix(m, &v0);
    }
    if (m_state & 1) {
        PSMTXCopy(m_pl_mat, m);
    }
    if (m_pl_ofs.x != 0.0f || m_pl_ofs.y != 0.0f || m_pl_ofs.z != 0.0f) {
        PSMTXMultVec(m, &m_pl_ofs, &v0);
        TransMatrix(m, &v0);
        memclr_asm(&m_pl_ofs, sizeof(Vec));
    }
    if (m_pl_dir.x != 0.0f || m_pl_dir.y != 0.0f || m_pl_dir.z != 0.0f) {
        getColumn(m, 1, &v1);
        getColumn(m, 3, &v4);
        PSVECCrossProduct(&v1, &m_pl_dir, &v0);
#line 650 "D:/Bio4/Prog/cam_qfps.cpp"
        VECNormalize(&v0, &v0);
        PSVECCrossProduct(&v0, &v1, &v3);
        setColumns(m, &v0, &v1, &v3, &v4);
        memclr_asm(&m_pl_dir, sizeof(Vec));
    }
    if (!pl->isKamae() && m_p_floor_norm != NULL && !StaFlagChk(pG, STA_SUB_SCRN)) {
        Vec up = {0.0f, 1.0f, 0.0f};

        v5 = *m_p_floor_norm;
        s_ratio = f * s_ratio + (1.0f - f) * m_floor_ratio;
        VecInternalDivisionAngle(&up, &v5, s_ratio, &v1, 1.0f - s_ratio);
        getColumn(m, 0, &v0);
        getColumn(m, 3, &v3);
        PSVECCrossProduct(&v0, &v1, &v2);
        PSVECCrossProduct(&v1, &v2, &v0);
        switch (PlGetStatus()) {
        case 0x4000:
            v3.y -= y_down;
            break;
        case 0x8000: {
            Vec t;
            f32 yd = g_crouch_cam_y_down;

            PSVECScale(&v2, &t, g_crouch_cam_z_back);
            PSVECSubtract(&v3, &t, &v3);
            v3.y -= yd;
            break;
        }
        }
        setColumns(m, &v0, &v1, &v2, &v3);
    }
}

// Which shoulder site the stick asks for: 0 default, 1 left, 2 right far, 3 left far (from the
// player's move direction relative to the camera); 0 while not moving.
int CameraQuasiFPS::checkFBLR()
{
    cPlayer* pl = pPL;

    if (StaFlagChk(pG, STA_KLAUSER_TRANSFORM)) {
        return 0;
    }
    if (pl->isKamae()) {
        if (LRcheck() == 0) {
            return 0;
        }
        return 1;
    } else {
        if (LRcheck() == 0) {
            return 2;
        }
        return 3;
    }
}

// Sets the offset blend ratio directly (1 = old offsets, 0 = current).
void CameraQuasiFPS::setBlendRatio(f32 r)
{
    blend_ratio = r;
}

// Starts a blend from the old offsets to the current ones over `n` frames (m_state bit2).
void CameraQuasiFPS::setBlendCount(int n)
{
    m_blend_count = n;
    m_blend_frame = n;
    m_state |= 4;
}

// How much the camera up axis follows the floor slope (0..1).
f32 CameraQuasiFPS::getFloorRatio()
{
    return m_floor_ratio;
}

// Sets the floor-follow ratio.
void CameraQuasiFPS::setFloorRatio(f32 ratio)
{
    m_floor_ratio = ratio;
}

// Picks the offset tables for this frame: the transition type from the player character
// (Leon / Ashley / Ada / mercenaries, or the partner state), then the ready type from the
// weapon in hand (none, handgun / shotgun by weapon_type, rifle, grenade, knife / special,
// mine thrower...) and the special states (Status_flg[3] 0x800000 -> 0xA). A type change starts
// an offset blend from the previous table (setBlendData) unless blending is frozen.
void CameraQuasiFPS::checkCameraType()
{
    if (SubCharGetStatus() & 0x20000000) {
        m_trans_type = 1;
    } else {
        switch (pG->pl_type) {
        case 0:
            m_trans_type = 0;
            break;
        case 1:
            m_trans_type = 2;
            break;
        case 2:
            m_trans_type = 3;
            break;
        case 4:
            m_trans_type = 4;
            break;
        case 5:
            m_trans_type = 5;
            break;
        default:
            m_trans_type = 0;
            break;
        }
    }
    blend_dst = trans_tbl[m_trans_type];
    if (DbgFlagChk(pGS, DBG_ADJUST_CAM)) {
        blend_dst = g_transOfs[5];
    }
    switch (m_trans_type) {
    case 0:
    case 2:
        switch (PlGetWeaponNo()) {
        default:
            m_ready_type = 0;
            break;
        case 8:
        case 0xB:
            switch (pG->weapon_type) {
            case 0:
            case 1:
                m_ready_type = 1;
                break;
            default:
                m_ready_type = 0;
                break;
            }
            break;
        case 0x13:
        case 0x16:
        case 0x17:
            m_ready_type = 2;
            break;
        case 0xE:
        case 0xF:
            m_ready_type = 3;
            break;
        case 0xD:
        case 0x10:
            m_ready_type = 1;
            break;
        }
        break;
    case 1:
        m_ready_type = 4;
        break;
    case 3:
        switch (PlGetWeaponNo()) {
        default:
            m_ready_type = 5;
            break;
        case 0xB:
            switch (pG->weapon_type) {
            case 0:
            case 1:
                m_ready_type = 6;
                break;
            default:
                m_ready_type = 5;
                break;
            }
            break;
        case 0x13:
        case 0x16:
        case 0x17:
            m_ready_type = 7;
            break;
        }
        break;
    case 4:
        if (StaFlagChk(pG, STA_KLAUSER_TRANSFORM)) {
            m_ready_type = 0xA;
        } else if (PlGetWeaponNo() != 0x10) {
            m_ready_type = 8;
        } else {
            m_ready_type = 9;
        }
        break;
    case 5:
        switch (PlGetWeaponNo()) {
        default:
            m_ready_type = 0xC;
            break;
        }
        break;
    }
    blend_src = ready_tbl[m_ready_type];
    if (DbgFlagChk(pGS, DBG_ADJUST_CAM)) {
        blend_src = g_readyOfs[14];
    }
}

// The frame's shoulder offset: blends old -> current tables by blend_ratio (counting the blend
// down), picks the up / mid / down site by the pitch ratio angle_y (interpolating toward the
// up or down entry), copies roll / fov, and rotates the result about y by the yaw angle_x.
void CameraQuasiFPS::calcOffset(QfpsOfs* out)
{
    Vec a;
    Vec b;
    Vec d;
    Vec c;
    QfpsOfs o[3];
    Mtx m;

    if (!(m_state & 8)) {
        m_blend_count--;
        if (m_blend_count > 0) {
            blend_ratio = (f32) m_blend_count / (f32) m_blend_frame;
        } else {
            m_state &= ~4;
            blend_ratio = 0.0f;
        }
    }
    if (m_state & 4) {
        f32 r = blend_ratio;
        f32 r1 = 1.0f - r;
        int i;

        for (i = 0; i < 3; i++) {
            VecLinearCombination(&old[i].Campos, &cur[i].Campos, r, r1, &o[i].Campos);
            VecLinearCombination(&old[i].target, &cur[i].target, r, r1, &o[i].target);
            VecLinearCombination(&old[i].campos2, &cur[i].campos2, r, r1, &o[i].campos2);
            o[i].Roll = r * old[i].Roll + r1 * cur[i].Roll;
            o[i].Fovy = r * old[i].Fovy + r1 * cur[i].Fovy;
        }
    } else {
        QfpsOfs* p = cur;
        int i;

        for (i = 0; i < 3; i++) {
            o[i] = p[i];
        }
    }
    switch (PlGetStatus()) {
    case 0x4000:
        break;
    case 0x8000: {
        int i;

        for (i = 0; i < 3; i++) {
            o[i].campos2.y -= g_crouch_cam_y_down * 0.5f;
            o[i].campos2.z += g_crouch_cam_z_back;
        }
        break;
    }
    }
    {
        f32 ay = angle_y;

        if (ay == 0.0f) {
            a = o[1].Campos;
            b = o[1].campos2;
            c = o[1].target;
            d = a;
        } else if (ay > 0.0f) {
            f32 r1 = 1.0f - ay;

            VecLinearCombination(&o[0].Campos, &o[1].Campos, ay, r1, &a);
            VecLinearCombination(&o[0].target, &o[1].target, ay, r1, &c);
            VecLinearCombination(&o[0].campos2, &o[1].campos2, ay, r1, &b);
            d = a;
        } else if (ay < 0.0f) {
            f32 r1;

            ay = -ay;
            r1 = 1.0f - ay;
            VecLinearCombination(&o[2].Campos, &o[1].Campos, ay, r1, &a);
            VecLinearCombination(&o[2].target, &o[1].target, ay, r1, &c);
            VecLinearCombination(&o[2].campos2, &o[1].campos2, ay, r1, &b);
            d = a;
        }
    }
    out->Campos = d;
    out->campos2 = b;
    out->target = c;
    out->Roll = o[1].Roll;
    out->Fovy = o[1].Fovy;
    if (angle_x != 0.0f) {
        PSMTXRotRad(m, 'y', angle_x);
        PSMTXMultVecSR(m, &out->Campos, &out->Campos);
        PSMTXMultVecSR(m, &out->campos2, &out->campos2);
        PSMTXMultVecSR(m, &out->target, &out->target);
    }
}

// Places the camera in the world from the offset: transforms Campos / campos2 / target by the
// base matrix, casts from the close point toward the camera position against the scenery,
// characters and objects (cameraHitCheck / EmHitCheck / ObjHitCheck) and pulls the camera in to
// the nearest hit (never closer than the close point); also probes the frustum edges so walls
// do not clip the view. Fills the camera parameters.
void CameraQuasiFPS::hitCheck(Mtx m, QfpsOfs* ofs, CameraParam* out)
{
    static f32 OFFSET_GAIN = 1.0f;
    Vec nrm;
    Vec wa;
    Vec wb;
    Mtx inv;
    Vec hitA;
    Vec hitB;
    Vec hitC;
    Vec vv[2];
    Vec diff;
    Vec sc;
    Vec dir;
    Vec near;
    Vec l0;
    Vec l1;
    int fA = 0;
    int fB = 0;
    int fC = 0;
    f32 w;
    f32 t;
    f32 d;

    PSMTXMultVec(m, &ofs->campos2, &wa);
    PSMTXMultVec(m, &ofs->Campos, &wb);
    l0 = wa;
    l1 = wb;
    if (pGS->debug_mode == 0xF) {  // struct view: the pG load stays below the copies' stores
        Draw_line3d(&l0, &l1, 0xFFFF0000, 0);
    }
    t = sinf(ofs->Fovy * PI / 360.0f) / cosf(ofs->Fovy * PI / 360.0f);
    w = ZNEAR * t * 1.3333334f * OFFSET_GAIN;
    {
        Vec up = {0.0f, 1.0f, 0.0f};
        Vec dd;
        Vec to;

        PSVECSubtract(&ofs->campos2, &ofs->target, &dd);
        PSVECCrossProduct(&dd, &up, &vv[1]);
#line 1140 "D:/Bio4/Prog/cam_qfps.cpp"
        VECNormalize(&vv[1], &vv[1]);
        PSVECScale(&vv[1], &vv[1], w);
        PSVECSubtract(&ofs->Campos, &ofs->target, &dd);
        PSVECCrossProduct(&dd, &up, &vv[0]);
#line 1148 "D:/Bio4/Prog/cam_qfps.cpp"
        VECNormalize(&vv[0], &vv[0]);
        PSVECScale(&vv[0], &vv[0], w);
        PSVECSubtract(&vv[1], &vv[0], &diff);
        PSVECSubtract(&ofs->campos2, &ofs->Campos, &dir);
#line 1157 "D:/Bio4/Prog/cam_qfps.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &near, ZNEAR);

        up = wa;
        to = wb;
        if (cameraHitCheck(&hitA, &nrm, &up, &to)) {
            l0 = wa;
            l1 = hitA;
            if (pG->debug_mode == 0xF) {
                Draw_line3d(&l0, &l1, 0xFFFF00FF, 0);
            }
            fA = 1;
            PSMTXInverse(m, inv);
            PSMTXMultVec(inv, &hitA, &hitA);
        }

        PSVECSubtract(&ofs->campos2, &vv[1], &wa);
        PSVECSubtract(&ofs->Campos, &vv[0], &wb);
        PSMTXMultVec(m, &wa, &wa);
        PSMTXMultVec(m, &wb, &wb);
        l0 = wa;
        l1 = wb;
        if (pG->debug_mode == 0xF) {
            Draw_line3d(&l0, &l1, 0xFF0000FF, 0);
        }
        up = wa;
        dd = wb;
        if (cameraHitCheck(&hitB, &nrm, &up, &dd)) {
            if (pG->debug_mode == 0xF) {
                l0 = hitB;
            }
            d = PSVECDistance(&hitB, &wb) / PSVECDistance(&wa, &wb);
            fB = 1;
            PSVECScale(&diff, &sc, d);
            PSVECAdd(&sc, &vv[0], &sc);
            PSMTXInverse(m, inv);
            PSMTXMultVec(inv, &hitB, &hitB);
            PSVECAdd(&hitB, &sc, &hitB);
            PSVECAdd(&hitB, &near, &hitB);
            if (pG->debug_mode == 0xF) {
                PSMTXMultVec(m, &hitB, &l1);
                Draw_line3d(&l0, &l1, 0xFFFFFF00, 0);
            }
        }

        PSVECAdd(&ofs->campos2, &vv[1], &wa);
        PSVECAdd(&ofs->Campos, &vv[0], &wb);
        PSMTXMultVec(m, &wa, &wa);
        PSMTXMultVec(m, &wb, &wb);
        l0 = wa;
        l1 = wb;
        if (pG->debug_mode == 0xF) {
            Draw_line3d(&l0, &l1, 0xFF00FF00, 0);
        }
        up = wa;
        dd = wb;
        if (cameraHitCheck(&hitC, &nrm, &up, &dd)) {
            l0 = wa;
            if (pG->debug_mode == 0xF) {
                l0 = hitC;
            }
            d = PSVECDistance(&hitC, &wb) / PSVECDistance(&wa, &wb);
            fC = 1;
            PSVECScale(&diff, &sc, d);
            PSVECAdd(&sc, &vv[0], &sc);
            PSMTXInverse(m, inv);
            PSMTXMultVec(inv, &hitC, &hitC);
            PSVECSubtract(&hitC, &sc, &hitC);
            PSVECAdd(&hitC, &near, &hitC);
            if (pG->debug_mode == 0xF) {
                PSMTXMultVec(m, &hitC, &l1);
                Draw_line3d(&l0, &l1, 0xFF00FFFF, 0);
            }
        }

        out->pos = ofs->Campos;
        out->at = ofs->target;
        out->roll = ofs->Roll;
        out->fovy = ofs->Fovy;
        if (fA || fB || fC) {
            f32 dmin = PSVECDistance(&out->pos, &ofs->campos2);

            if (fA) {
                d = PSVECDistance(&hitA, &ofs->campos2);
                if (d < dmin) {
                    dmin = d;
                    out->pos = hitA;
                }
            }
            if (fC) {
                d = PSVECDistance(&hitC, &ofs->campos2);
                if (d < dmin) {
                    dmin = d;
                    out->pos = hitC;
                }
            }
            if (fB) {
                d = PSVECDistance(&hitB, &ofs->campos2);
                if (d < dmin) {
                    dmin = d;
                    out->pos = hitB;
                }
            }
        } else {
            wb = ofs->Campos;
            fB = 0;
            PSVECSubtract(&ofs->Campos, &vv[0], &wa);
            up = wb;
            dd = wa;
            if (cameraHitCheck(&hitC, &nrm, &up, &dd)) {
                fB = 1;
            } else {
                PSVECAdd(&ofs->Campos, &vv[0], &wa);
                up = wb;
                dd = wa;
                if (cameraHitCheck(&hitB, &nrm, &up, &dd)) {
                    fC = 1;
                }
            }
            if (fB || fC) {
                PSVECAdd(&ofs->Campos, &near, &out->pos);
            }
        }
    }
}

// Copies the outgoing ready / transition tables into the blend-from slots (g_readyOfs[15],
// g_transOfs[6]).
void CameraQuasiFPS::setBlendData(void* src, void* dst)
{
    QfpsOfs (*s)[3] = (QfpsOfs (*)[3]) src;
    QfpsOfs (*d)[3] = (QfpsOfs (*)[3]) dst;
    int i;
    int j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            g_readyOfs[15][i][j] = s[i][j];
            g_transOfs[6][i][j] = d[i][j];
        }
    }
}

// Reads the per-area override tables (g_readyOfs[14], g_transOfs[5]) for the debug camera editor.
void CameraQuasiFPS::getAreaData(QfpsOfs (*ready)[3], QfpsOfs (*trans)[3])
{
    int i;
    int j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            ready[i][j] = g_readyOfs[14][i][j];
            trans[i][j] = g_transOfs[5][i][j];
        }
    }
}

// Writes the per-area override tables.
void CameraQuasiFPS::setAreaData(QfpsOfs (*ready)[3], QfpsOfs (*trans)[3])
{
    int i;
    int j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            g_readyOfs[14][i][j] = ready[i][j];
            g_transOfs[5][i][j] = trans[i][j];
        }
    }
}

#define OFS_COPY(src, dst)                 \
    {                                      \
        QfpsOfs (*d_)[3] = (dst);          \
        QfpsOfs (*s_)[3] = (src);          \
        int i_ = 2;                        \
        int j_;                            \
        QfpsOfs* sp_;                      \
        QfpsOfs* dp_;                      \
        while (i_--) {                     \
            dp_ = *d_;                     \
            sp_ = *s_;                     \
            j_ = 3;                        \
            while (j_--) {                 \
                *dp_ = *sp_;               \
                dp_++;                     \
                sp_++;                     \
            }                              \
            d_++;                          \
            s_++;                          \
        }                                  \
    }

// Loads a camera area cut's shoulder offsets into the override tables: starts from the defaults,
// takes the cut's floor ratio, then per left / right x up / mid / down entry the ready (flags
// 0x30) and transition (not 0x20) camera / target / roll / fov and close points.
void CameraQuasiFPS::setAreaData(CameraCut* cut)
{
    int i;
    int j;
    int k = 0;
    QfpsOfs* p;

    if (cut == NULL) {
        return;
    }
    OFS_COPY(g_readyOfs[0], g_readyOfs[14]);
    OFS_COPY(g_transOfs[0], g_transOfs[5]);
    m_floor_ratio = cut->floor_ratio;
    if (cut->num == 0) {
        return;
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++, k++) {
            if (i <= 1) {
                p = &g_readyOfs[14][i][j];
                if (cut->flags & 0x30) {
                    p->Campos = cut->pos[k];
                    p->target = cut->at[k];
                    p->Roll = cut->roll[k];
                    p->Fovy = cut->fovy[k];
                }
            } else {
                p = &g_transOfs[5][i - 2][j];
                if (!(cut->flags & 0x20)) {
                    p->Campos = cut->pos[k];
                    p->target = cut->at[k];
                    p->Roll = cut->roll[k];
                    p->Fovy = cut->fovy[k];
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++, k++) {
            if (i <= 1) {
                p = &g_readyOfs[14][i][j];
                if (cut->flags & 0x30) {
                    p->campos2 = cut->pos[k];
                }
            } else {
                p = &g_transOfs[5][i - 2][j];
                if (!(cut->flags & 0x20)) {
                    p->campos2 = cut->pos[k];
                }
            }
        }
    }
}

// Keeps the close point at least 320 units behind the player along the camera direction so the
// shoulder camera cannot start inside the model.
void offsetCorrection(QfpsOfs* o)
{
    static f32 GAIN = 0.8f;
    Vec d;
    f32 len;
    f32 t;

    PSVECSubtract(&o->campos2, &o->Campos, &d);
    len = PSVECMag(&d);
#line 1518 "D:/Bio4/Prog/cam_qfps.cpp"
    VECNormalize(&d, &d);
    t = (-GAIN * 400.0f - o->Campos.z) / d.z;
    if (t > len) {
        PSVECScale(&d, &d, t);
        PSVECAdd(&o->Campos, &d, &o->campos2);
    }
}

// offsetCorrection on the three sites of one side.
static void offsetArrayCorrection(QfpsOfs (*o)[3])
{
    int i;

    for (i = 0; i < 3; i++) {
        offsetCorrection(&o[0][i]);
    }
}

// Corrects every default ready (0..13) and transition (0..4) table at init.
void CameraQuasiFPS::offsetCorrection()
{
    int i;

    for (i = 0; i < 14; i++) {
        offsetArrayCorrection(g_readyOfs[i]);
    }
    for (i = 0; i < 5; i++) {
        offsetArrayCorrection(g_transOfs[i]);
    }
}

// Points the type -> table slots at the built-in default tables.
void CameraQuasiFPS::bindDefaultCamera()
{
    ready_tbl[0] = g_readyOfs[0];
    ready_tbl[1] = g_readyOfs[1];
    ready_tbl[2] = g_readyOfs[2];
    ready_tbl[3] = g_readyOfs[3];
    ready_tbl[4] = g_readyOfs[4];
    ready_tbl[5] = g_readyOfs[5];
    ready_tbl[6] = g_readyOfs[6];
    ready_tbl[7] = g_readyOfs[7];
    ready_tbl[8] = g_readyOfs[8];
    ready_tbl[9] = g_readyOfs[9];
    ready_tbl[10] = g_readyOfs[10];
    ready_tbl[11] = g_readyOfs[11];
    ready_tbl[12] = g_readyOfs[12];
    ready_tbl[13] = g_readyOfs[13];
    trans_tbl[0] = g_transOfs[0];
    trans_tbl[1] = g_transOfs[1];
    trans_tbl[2] = g_transOfs[0];
    trans_tbl[3] = g_transOfs[0];
    trans_tbl[4] = g_transOfs[3];
    trans_tbl[5] = g_transOfs[4];
}

// Points the type slots at the area override table (g_readyOfs[14]) for the types the area cut
// overrides (its flags decide the normal and the partner-carry type separately), the defaults
// for the rest.
void CameraQuasiFPS::bindAreaCamera(CameraAreaRec* rec)
{
    CameraCut* cut;
    CameraAreaInfo* area;

    if (rec == NULL) {
        return;
    }
    cut = rec->cut;
    if (cut != NULL && cut->num == 0) {
        return;
    }
    area = rec->area;
    if (cut->flags & 0x30) {
        offsetArrayCorrection(g_readyOfs[14]);
        if (area->attr2 & 0x5D) {
            ready_tbl[0] = g_readyOfs[14];
            ready_tbl[1] = g_readyOfs[14];
            ready_tbl[2] = g_readyOfs[14];
            ready_tbl[3] = g_readyOfs[14];
            ready_tbl[5] = g_readyOfs[14];
            ready_tbl[6] = g_readyOfs[14];
            ready_tbl[7] = g_readyOfs[14];
            ready_tbl[8] = g_readyOfs[14];
            ready_tbl[9] = g_readyOfs[14];
            ready_tbl[10] = g_readyOfs[14];
            ready_tbl[11] = g_readyOfs[14];
            ready_tbl[12] = g_readyOfs[14];
            ready_tbl[13] = g_readyOfs[14];
        } else {
            ready_tbl[0] = g_readyOfs[0];
            ready_tbl[1] = g_readyOfs[1];
            ready_tbl[2] = g_readyOfs[2];
            ready_tbl[3] = g_readyOfs[3];
            ready_tbl[5] = g_readyOfs[5];
            ready_tbl[6] = g_readyOfs[6];
            ready_tbl[7] = g_readyOfs[7];
            ready_tbl[8] = g_readyOfs[8];
            ready_tbl[9] = g_readyOfs[9];
            ready_tbl[10] = g_readyOfs[10];
            ready_tbl[11] = g_readyOfs[11];
            ready_tbl[12] = g_readyOfs[12];
            ready_tbl[13] = g_readyOfs[13];
        }
        if (area->attr2 & 2) {
            ready_tbl[4] = g_readyOfs[14];
        } else {
            ready_tbl[4] = g_readyOfs[4];
        }
    }
    if (!(cut->flags & 0x20)) {
        offsetArrayCorrection(g_transOfs[5]);
        if (area->attr2 & 0x5D) {
            trans_tbl[0] = g_transOfs[5];
            trans_tbl[2] = g_transOfs[5];
            trans_tbl[3] = g_transOfs[5];
            trans_tbl[4] = g_transOfs[5];
            trans_tbl[5] = g_transOfs[5];
        } else {
            trans_tbl[0] = g_transOfs[0];
            trans_tbl[2] = g_transOfs[0];
            trans_tbl[3] = g_transOfs[2];
            trans_tbl[4] = g_transOfs[3];
            trans_tbl[5] = g_transOfs[4];
        }
        if (area->attr2 & 2) {
            trans_tbl[1] = g_transOfs[5];
        } else {
            trans_tbl[1] = g_transOfs[1];
        }
    }
}

// OPEN (12 words): the original issues the eleven init stores in pure source order (ours: the
// dying-source stores first) — the dying-store family of emrock SetRock / obj1b SetSpear.
void CameraQuasiFPS::init()
{
    // The original issues the eleven reference stores in pure source order with the constants
    // in reload's spill registers (r10/r8/r7, the flags temp r0, both pool floats through f0):
    // nothing dies at a store there. Pinned constants stored through plain references (the
    // u8&/s16& setters would copy a hard register into a pseudo) plus one codeless keep-alive
    // at the block end so no store has a dying source.
    int one;
    register int two asm("r8");    // COMPILER-DIFF: #13 (value pin)
    register int zero asm("r7");   // COMPILER-DIFF: #13 (value pin)
    f32 fz;
    u32 fl;
    one = 1;
    two = 2;
    zero = 0;
    FSet(m_walk_ratio, 0.8f);
    FSet(CamSmth.m_ratio, 0.8f);
    fz = 0.0f;  // after the 0.8 stores: pool order 0.8, 0.0
    FSet(m_zoom_ratio, fz);
    { u8& r_ = reset; r_ = one; }
    { s16& r_ = m_search_frame; r_ = zero; }
    { u8& r_ = m_site; r_ = two; }
    fl = m_state & ~7;
    BitSet(m_state, fl);
    FSet(angle_y, fz);
    FSet(angle_x, fz);
    { s16& r_ = m_search_cnt; r_ = zero; }
    asm("" : "=m"(m_floor_ratio) : "r"(one), "r"(two), "r"(zero), "f"(fz), "r"(fl));  // COMPILER-DIFF: #13 (keep-alive)
    if (pPL) {
        setPlayerLocation(pPL->mat, pPL->pFloor_norm);
    }
    checkCameraType();
    setBlendCount(0);
}

// Per-frame shoulder camera: table selection, base matrix, shoulder site from the stick (kept
// while the stick is held), aim angles, the blended offset, the collision-corrected camera, and
// the smoothing (CamSmth: ratio m_walk_ratio while walking, snapped on the first frame after a
// reset); the result lands in cam.param for CameraControl.
void CameraQuasiFPS::move()
{
    static Vec campos_aim_eff;
    static Vec target_aim_eff;
    static f32 lr_rate = 0.6f;
    static ViewFrustum view_box[16];
    static int cnt = 0;
    Camera c;
    Mtx m;
    CameraParam p2;
    QfpsOfs ofs;
    CameraParam prm;

    checkCameraType();
    calcBaseMatrix(m);
    if (!DbgFlagChk(pG, DBG_ADJUST_CAM)) {
        m_site = checkFBLR();
    }
    switch (m_site) {
    case 0:
        cur = blend_src[0];
        old = g_readyOfs[15][0];
        break;
    case 1:
        cur = blend_src[2];
        old = g_readyOfs[15][1];
        break;
    case 2:
        cur = blend_dst[0];
        old = g_transOfs[6][0];
        break;
    case 3:
        cur = blend_dst[2];
        old = g_transOfs[6][1];
        break;
    }
    if (!DbgFlagChk(pG, DBG_ADJUST_CAM)) {
        calcDepressionRatio();
    }
    calcOffset(&ofs);
    hitCheck(m, &ofs, &prm);
    p2 = prm;
    if (reset) {
        campos_aim_eff = p2.pos;
        target_aim_eff = p2.at;
    } else {
        p2.pos.x *= 1.0f - lr_rate;
        p2.at.x *= 1.0f - lr_rate;
        campos_aim_eff.x = campos_aim_eff.x * lr_rate + p2.pos.x;
        campos_aim_eff.y = p2.pos.y;
        campos_aim_eff.z = p2.pos.z;
        target_aim_eff.x = target_aim_eff.x * lr_rate + p2.at.x;
        target_aim_eff.y = p2.at.y;
        target_aim_eff.z = p2.at.z;
    }
    PSMTXMultVec(m, &campos_aim_eff, &c.param.pos);
    PSMTXMultVec(m, &target_aim_eff, &c.param.at);
    c.param.roll = 0.0f;
    c.param.fovy = p2.fovy;
    switch (PlGetStatus()) {
    case 2:
    case 4:
    case 8:
        CamSmth.m_ratio = m_walk_ratio;
        break;
    }
    if (reset) {
        CamSmth.m_flag |= 1;
    }
    if (pG->debug_mode == 0xF) {
        Vec poly[3];
        int i;
        int j;

        {
            ViewFrustum* vf = &View.localFull;
            Vec* src;

            CameraSetOrientationRoll(&c);
            src = vf->point;
            for (j = 0; j < 8; j++, src++) {
                PSMTXMultVec(c.mat, src, &view_box[cnt].point[j]);
                PSMTXMultVec(c.mat, src, &view_box[cnt].point[j]);
            }
        }
        for (i = 0; i < 16; i++) {
            for (j = 0; j < 4; j++) {
                u32 col;

                if ((cnt + 6) % 7 == i) {
                    col = 0xFFFF0000;
                } else if (cnt == i) {
                    col = 0xFFFFFFFF;
                } else {
                    col = 0xFF606060;
                }
                Draw_line3d(&view_box[i].point[j], &view_box[i].point[(j + 1) % 4], col, 0);
            }
        }
        Draw_line3d(&view_box[cnt].point[0], &view_box[cnt].point[3], 0xFFFFFFFF, 0);
        Draw_line3d(&view_box[cnt].point[3], &view_box[cnt].point[7], 0xFFFFFFFF, 0);
        Draw_line3d(&view_box[cnt].point[7], &view_box[cnt].point[4], 0xFFFFFFFF, 0);
        Draw_line3d(&view_box[cnt].point[4], &view_box[cnt].point[0], 0xFFFFFFFF, 0);
        poly[0] = view_box[cnt].point[0];
        poly[1] = view_box[cnt].point[3];
        poly[2] = view_box[cnt].point[4];
        Draw_poly(poly, 0x40FFFFFF, 1);
        poly[0] = view_box[cnt].point[3];
        poly[1] = view_box[cnt].point[7];
        poly[2] = view_box[cnt].point[4];
        Draw_poly(poly, 0x40FFFFFF, 1);
        cnt++;
        cnt %= 16;
    }
    cam.param.pos = c.param.pos;
    cam.param.at = c.param.at;
    cam.param.roll = c.param.roll;
    cam.param.fovy = c.param.fovy;
    reset = 0;
}
