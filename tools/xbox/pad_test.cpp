// Host unit test for src/xbox/pad_xbox.cpp (the Xbox controls layer). Run it with tools/xbox/run_pad_test.sh.
//
// The layer talks to the platform only through XPadBackend_*, so this file provides a scripted fake
// backend and drives the same PAD calls the game makes. It checks:
//   * the PADStatus layout the layer fills is the one the game's pad.h declares
//   * button / stick / trigger translation, including the stick Y direction
//   * disconnected ports, hot-plug, and a backend that fails to start
//   * rumble commands reach the backend only when they change
//   * the layer's PADClamp equals the original src/lib/Padclamp.c for every possible input
#include "pad.h"
#include "xbox/xpad_backend.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#undef memcpy  // host_compat.h renamed it only to get past joy.h; the test itself wants the real one

// Original Dolphin implementation, built from src/lib/Padclamp.c with its entry point renamed.
extern "C" void PADClamp_orig(PADStatus* status);

static int g_fail = 0;
static int g_checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_fail++;                                                        \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
        }                                                                    \
    } while (0)

// ---- scripted backend ----
static XPadRaw g_raw[XPAD_MAX];
static int g_connected[XPAD_MAX];
static int g_initResult = 1;
static int g_initCalls, g_rescanCalls, g_rumbleCalls;
static unsigned short g_lastLow[XPAD_MAX], g_lastHigh[XPAD_MAX];

extern "C" {
int XPadBackend_Init(void)
{
    g_initCalls++;
    return g_initResult;
}
void XPadBackend_Rescan(void) { g_rescanCalls++; }
int XPadBackend_Read(int port, XPadRaw* out)
{
    if (!g_connected[port]) {
        return 0;
    }
    *out = g_raw[port];
    return 1;
}
void XPadBackend_Rumble(int port, unsigned short low, unsigned short high)
{
    g_rumbleCalls++;
    g_lastLow[port] = low;
    g_lastHigh[port] = high;
}
}

static void ResetFake()
{
    memset(g_raw, 0, sizeof(g_raw));
    memset(g_connected, 0, sizeof(g_connected));
    memset(g_lastLow, 0, sizeof(g_lastLow));
    memset(g_lastHigh, 0, sizeof(g_lastHigh));
    g_initResult = 1;
    g_initCalls = g_rescanCalls = g_rumbleCalls = 0;
}

static void TestLayout()
{
    // The game and the Dolphin header both lay PADStatus out as 12 bytes with err at 0x0A.
    CHECK(sizeof(PADStatus) == 12);
    CHECK(offsetof(PADStatus, err) == 0x0A);
    CHECK(offsetof(PADStatus, triggerLeft) == 0x06);
}

static void TestInitAndDisconnected()
{
    PADStatus st[4];
    int i;

    ResetFake();
    PADInit();
    CHECK(g_initCalls == 1);
    CHECK(g_rescanCalls == 1);
    PADSetAnalogMode(3);

    memset(st, 0xAA, sizeof(st));  // garbage must be overwritten
    CHECK(PADRead(st) == 0);
    for (i = 0; i < 4; i++) {
        CHECK(st[i].err == PAD_ERR_NO_CONTROLLER);
        CHECK(st[i].button == 0 && st[i].stickX == 0 && st[i].stickY == 0);
        CHECK(st[i].triggerLeft == 0 && st[i].triggerRight == 0);
    }

    // The game calls PADReset for every port without a controller; it must report success and rescan.
    CHECK(PADReset(0xF0000000) == 1);
    CHECK(g_rescanCalls == 2);
    PADRecalibrate(0xF0000000);  // no-op, must simply not crash
}

static void TestButtons()
{
    PADStatus st[4];

    ResetFake();
    PADInit();
    g_connected[1] = 1;
    g_raw[1].buttons = XPAD_A | XPAD_B | XPAD_X | XPAD_Y | XPAD_START | XPAD_BLACK | XPAD_DPAD_UP | XPAD_DPAD_LEFT;
    PADRead(st);
    CHECK(st[0].err == PAD_ERR_NO_CONTROLLER);
    CHECK(st[1].err == PAD_ERR_NONE);
    CHECK(st[1].button == (JOY_A | JOY_B | JOY_X | JOY_Y | JOY_START | JOY_Z | JOY_UP | JOY_LEFT));

    // White, Back and the stick clicks have no GC equivalent and must not leak into the button word.
    g_raw[1].buttons = XPAD_WHITE | XPAD_BACK | XPAD_LTHUMB | XPAD_RTHUMB;
    PADRead(st);
    CHECK(st[1].button == 0);

    g_raw[1].buttons = XPAD_DPAD_DOWN | XPAD_DPAD_RIGHT;
    PADRead(st);
    CHECK(st[1].button == (JOY_DOWN | JOY_RIGHT));
}

static void TestSticksAndTriggers()
{
    PADStatus st[4];

    ResetFake();
    PADInit();
    g_connected[0] = 1;

    // full deflection: +y is UP on both sides of the layer
    g_raw[0].lx = 32767;
    g_raw[0].ly = 32767;
    g_raw[0].rx = -32768;
    g_raw[0].ry = 0;
    PADRead(st);
    CHECK(st[0].stickX == 100 && st[0].stickY == 100);
    CHECK(st[0].substickX == -75 && st[0].substickY == 0);

    g_raw[0].ly = -32768;
    PADRead(st);
    CHECK(st[0].stickY == -100);

    // triggers: 0 -> 0, half pull -> no click, full pull -> 180 and the digital bit
    g_raw[0].lt = 0;
    g_raw[0].rt = 100;
    PADRead(st);
    CHECK(st[0].triggerLeft == 0 && st[0].triggerRight == 70);
    CHECK((st[0].button & (JOY_L | JOY_R)) == 0);

    g_raw[0].lt = 255;
    g_raw[0].rt = 255;
    PADRead(st);
    CHECK(st[0].triggerLeft == 180 && st[0].triggerRight == 180);
    CHECK((st[0].button & JOY_L) && (st[0].button & JOY_R));

    // after the game's PADClamp, full deflection equals what a GameCube pad gave: 72 on the main stick,
    // 59 on the C stick (clamped octagon maxima) and 150 on a fully pulled trigger
    ResetFake();
    PADInit();
    g_connected[0] = 1;
    g_raw[0].lx = 32767;
    g_raw[0].rx = 32767;
    g_raw[0].lt = 255;
    PADRead(st);
    PADClamp(st);
    CHECK(st[0].stickX == 72 && st[0].stickY == 0);
    CHECK(st[0].substickX == 59);
    CHECK(st[0].triggerLeft == 150);

    // a stick resting inside the dead zone reads exactly zero
    g_raw[0].lx = 3000;  // ~9 raw, under the 15 dead zone
    g_raw[0].lt = 20;    // ~14 raw, under the trigger minimum of 30
    PADRead(st);
    PADClamp(st);
    CHECK(st[0].stickX == 0 && st[0].triggerLeft == 0);
}

static void TestHotplug()
{
    PADStatus st[4];

    ResetFake();
    PADInit();
    PADRead(st);
    CHECK(st[2].err == PAD_ERR_NO_CONTROLLER);

    g_connected[2] = 1;
    PADRead(st);
    CHECK(st[2].err == PAD_ERR_NONE);
    CHECK(PADRead(st) == (PAD_CHAN0_BIT >> 2));  // returns the ports with a motor (== connected)

    g_connected[2] = 0;
    PADRead(st);
    CHECK(st[2].err == PAD_ERR_NO_CONTROLLER);
}

static void TestBackendFailure()
{
    PADStatus st[4];
    int i;

    ResetFake();
    g_initResult = 0;
    g_connected[0] = 1;  // would be readable, but the backend said it is not running
    PADInit();
    CHECK(g_rescanCalls == 0);
    PADRead(st);
    for (i = 0; i < 4; i++) {
        CHECK(st[i].err == PAD_ERR_NO_CONTROLLER);
    }
    PADControlMotor(0, 1);
    CHECK(g_rumbleCalls == 0);
    PADReset(0xF0000000);
    CHECK(g_rescanCalls == 0);
}

static void TestRumble()
{
    ResetFake();
    PADInit();

    PADControlMotor(0, 1);  // on
    CHECK(g_rumbleCalls == 1 && g_lastLow[0] == 0xFFFF && g_lastHigh[0] == 0xFFFF);
    PADControlMotor(0, 1);  // unchanged: not sent again
    CHECK(g_rumbleCalls == 1);
    PADControlMotor(0, 2);  // hard stop
    CHECK(g_rumbleCalls == 2 && g_lastLow[0] == 0 && g_lastHigh[0] == 0);
    PADControlMotor(0, 0);  // plain stop after a hard stop is a different command, still ends at off
    CHECK(g_lastLow[0] == 0 && g_lastHigh[0] == 0);

    PADControlMotor(3, 1);
    CHECK(g_lastLow[3] == 0xFFFF);
    PADControlMotor(4, 1);   // out-of-range channels are ignored
    PADControlMotor(-1, 1);
    CHECK(g_lastLow[0] == 0 && g_lastLow[3] == 0xFFFF);

    PADControlMotor(3, 7);  // unknown command is treated as stop
    CHECK(g_lastLow[3] == 0 && g_lastHigh[3] == 0);

    // PADInit resets the layer's command state, so the next "on" is sent even if it was on before
    PADControlMotor(1, 1);
    PADInit();
    g_rumbleCalls = 0;
    PADControlMotor(1, 1);
    CHECK(g_rumbleCalls == 1);
}

// PADClamp over every possible input against the original Padclamp.c.
static void TestClampMatchesOriginal()
{
    PADStatus mine[4];
    PADStatus orig[4];
    int a, b;
    long compared = 0;

    // sticks: every (x, y) pair on both sticks
    for (a = -128; a <= 127; a++) {
        for (b = -128; b <= 127; b++) {
            memset(mine, 0, sizeof(mine));
            mine[0].err = PAD_ERR_NONE;
            mine[0].stickX = (s8) a;
            mine[0].stickY = (s8) b;
            mine[0].substickX = (s8) b;
            mine[0].substickY = (s8) a;
            memcpy(orig, mine, sizeof(orig));
            PADClamp(mine);
            PADClamp_orig(orig);
            compared++;
            if (memcmp(mine, orig, sizeof(mine)) != 0) {
                CHECK(!"stick clamp differs from Padclamp.c");
                return;
            }
        }
    }
    // triggers: every value on both
    for (a = 0; a <= 255; a++) {
        memset(mine, 0, sizeof(mine));
        mine[0].triggerLeft = (u8) a;
        mine[0].triggerRight = (u8) (255 - a);
        memcpy(orig, mine, sizeof(orig));
        PADClamp(mine);
        PADClamp_orig(orig);
        compared++;
        if (memcmp(mine, orig, sizeof(mine)) != 0) {
            CHECK(!"trigger clamp differs from Padclamp.c");
            return;
        }
    }
    // a port in error state is left untouched by both
    memset(mine, 0, sizeof(mine));
    mine[1].err = PAD_ERR_NO_CONTROLLER;
    mine[1].stickX = 99;
    memcpy(orig, mine, sizeof(orig));
    PADClamp(mine);
    PADClamp_orig(orig);
    CHECK(memcmp(mine, orig, sizeof(mine)) == 0 && mine[1].stickX == 99);
    CHECK(compared == 65536 + 256);
    CHECK(1);  // reached the end without a mismatch
    printf("PADClamp compared against Padclamp.c on %ld input sets\n", compared);
}

int main()
{
    TestLayout();
    TestInitAndDisconnected();
    TestButtons();
    TestSticksAndTriggers();
    TestHotplug();
    TestBackendFailure();
    TestRumble();
    TestClampMatchesOriginal();

    printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
