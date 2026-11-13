// xbox/pad_xbox: Xbox replacement for the Dolphin SDK PAD library (src/lib/Pad.c, Padclamp.c).
//
// Provides exactly the PAD calls the game makes and nothing else:
//   game/pad.cpp   PADInit PADReset PADRead PADClamp PADSetAnalogMode PADControlMotor
//   game/main.cpp  PADRecalibrate (soft reset path)
//   game/dvd.cpp   PADControlMotor (stops the motor when the drive reports an error)
// Link this file INSTEAD of src/lib/Pad.c, SIBios.c, SISamplingRate.c and Padclamp.c.
//
// What the game expects back is a GameCube controller: PADStatus with GC button bits, signed 8-bit sticks
// around 0 that PADClamp later shapes, and 8-bit triggers. So the work here is a translation:
//   * buttons   Xbox -> GC bit layout (kButtonMap, edit it to change the default binding)
//   * sticks    16-bit -> the raw range a GC stick reports before PADClamp (see kStickRaw / kSubstickRaw)
//   * triggers  0..255 -> the raw range GC triggers report (kTriggerRaw), plus the digital click bit
//   * rumble    GC "motor on / off" commands -> both Xbox motors full / off
// PADClamp is the same math as src/lib/Padclamp.c (same ClampRegion, same octagon gate), so a stick at
// full deflection gives the game the same numbers the GameCube did.
//
// Only compiled for the Xbox build (RE4_XBOX); it has no meaning on the GameCube target.
#ifdef RE4_XBOX

#include "pad.h"  // game header: PADStatus, PAD_CHAN0_BIT, PAD_ERR_*, JOY_* bit values
#include "xbox/xpad_backend.h"

#include <string.h>

// GameCube command values for PADControlMotor (Dolphin pad.h PAD_MOTOR_*)
#define MOTOR_STOP 0
#define MOTOR_RUMBLE 1
#define MOTOR_STOP_HARD 2

// Raw GC value that an Xbox axis at full deflection is scaled to. PADClamp then removes the dead zone
// (15) and gates to the octagon (max 72 / 59), which is why these sit above the clamped maximum.
static const int kStickRaw = 100;
static const int kSubstickRaw = 75;
// GC triggers report ~0..180 before PADClamp (min 30, max 180); a full Xbox pull maps to 180.
static const int kTriggerRaw = 180;
// Raw trigger value at which the GC also sets the digital L/R click bit.
static const int kTriggerClick = 160;

struct ButtonMap {
    u16 xpad;  // XPAD_* bit
    u16 gc;    // JOY_* bit (identical to the GC PAD_BUTTON_* layout)
};

// Default binding. White and Back are unmapped on purpose: the game has no use for a button that has no
// GC equivalent, and the original pad layout is a matter of taste, so keep it in one table.
static const ButtonMap kButtonMap[] = {
    {XPAD_DPAD_LEFT, JOY_LEFT},
    {XPAD_DPAD_RIGHT, JOY_RIGHT},
    {XPAD_DPAD_DOWN, JOY_DOWN},
    {XPAD_DPAD_UP, JOY_UP},
    {XPAD_A, JOY_A},
    {XPAD_B, JOY_B},
    {XPAD_X, JOY_X},
    {XPAD_Y, JOY_Y},
    {XPAD_START, JOY_START},
    {XPAD_BLACK, JOY_Z},
};

static int s_backendOk = 0;
static u32 s_analogMode = 0;
static u8 s_motorCmd[XPAD_MAX];  // last command sent per port, so the backend is only touched on change

static inline int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// 16-bit Xbox axis -> signed 8-bit GC raw value of magnitude `raw` at full deflection.
static s8 ScaleStick(short v, int raw) { return (s8) Clamp((int) v * raw / 32767, -128, 127); }

static u8 ScaleTrigger(unsigned char v) { return (u8) (v * kTriggerRaw / 255); }

static void SetNoController(PADStatus* st, int err)
{
    memset(st, 0, sizeof(PADStatus));
    st->err = (s8) err;
}

extern "C" {

void PADInit()
{
    int i;

    s_backendOk = XPadBackend_Init();
    s_analogMode = 0;
    for (i = 0; i < XPAD_MAX; i++) {
        s_motorCmd[i] = MOTOR_STOP;
    }
    if (s_backendOk) {
        XPadBackend_Rescan();
    }
}

// The GameCube version reconfigures the SI channels. Here "reset" is a hot-plug rescan, which the game
// reaches every frame for a port that reports PAD_ERR_NO_CONTROLLER. Returns 1 = done, like the original.
BOOL PADReset(u32 mask)
{
    (void) mask;
    if (s_backendOk) {
        XPadBackend_Rescan();
    }
    return 1;
}

// GC: re-reads the stick origin from the controller. An Xbox stick reports 0 at rest and the clamp
// already removes the dead zone, so there is nothing to recalibrate.
void PADRecalibrate(u32 mask) { (void) mask; }

// Mode 3 is what the game asks for: full 8-bit axes. analogA/analogB (GC pressure-sensitive A and B) have
// no Xbox backend source, so they stay 0; the game zeroes Key.analogA/B itself anyway.
void PADSetAnalogMode(u32 mode) { s_analogMode = mode; }

// Fills status[0..3] for the four ports. Returns the ports that have a rumble motor (every Xbox pad does).
u32 PADRead(PADStatus* status)
{
    u32 motor = 0;
    int chan;

    for (chan = 0; chan < XPAD_MAX; chan++, status++) {
        XPadRaw raw;
        u16 buttons = 0;
        size_t i;

        if (!s_backendOk || !XPadBackend_Read(chan, &raw)) {
            SetNoController(status, PAD_ERR_NO_CONTROLLER);
            continue;
        }

        for (i = 0; i < sizeof(kButtonMap) / sizeof(kButtonMap[0]); i++) {
            if (raw.buttons & kButtonMap[i].xpad) {
                buttons |= kButtonMap[i].gc;
            }
        }

        status->stickX = ScaleStick(raw.lx, kStickRaw);
        status->stickY = ScaleStick(raw.ly, kStickRaw);
        status->substickX = ScaleStick(raw.rx, kSubstickRaw);
        status->substickY = ScaleStick(raw.ry, kSubstickRaw);
        status->triggerLeft = ScaleTrigger(raw.lt);
        status->triggerRight = ScaleTrigger(raw.rt);
        if (status->triggerLeft >= kTriggerClick) {
            buttons |= JOY_L;
        }
        if (status->triggerRight >= kTriggerClick) {
            buttons |= JOY_R;
        }
        status->button = buttons;
        status->analogA = 0;
        status->analogB = 0;
        status->pad_B = 0;
        status->err = PAD_ERR_NONE;
        motor |= PAD_CHAN0_BIT >> chan;
    }
    return motor;
}

// ---- PADClamp: same math and region as src/lib/Padclamp.c (PADClamp only; the game never calls
// PADClampCircle). Kept bit-for-bit so analog feel matches the GameCube. tools/xbox/pad_test.cpp checks
// it against the original over every input.
static const struct {
    u8 minTrigger, maxTrigger;
    s8 minStick, maxStick, xyStick;
    s8 minSubstick, maxSubstick, xySubstick;
} ClampRegion = {30, 180, 15, 72, 40, 15, 59, 31};

static void ClampStick(s8* px, s8* py, s8 max, s8 xy, s8 min)
{
    int x = *px;
    int y = *py;
    int signX;
    int signY;
    int d;

    if (0 <= x) {
        signX = 1;
    } else {
        signX = -1;
        x = -x;
    }
    if (0 <= y) {
        signY = 1;
    } else {
        signY = -1;
        y = -y;
    }

    if (x <= min) {
        x = 0;
    } else {
        x -= min;
    }
    if (y <= min) {
        y = 0;
    } else {
        y -= min;
    }

    if (x == 0 && y == 0) {
        *px = *py = 0;
        return;
    }

    if (xy * y <= xy * x) {
        d = xy * x + (max - xy) * y;
        if (xy * max < d) {
            x = (s8) (xy * max * x / d);
            y = (s8) (xy * max * y / d);
        }
    } else {
        d = xy * y + (max - xy) * x;
        if (xy * max < d) {
            x = (s8) (xy * max * x / d);
            y = (s8) (xy * max * y / d);
        }
    }

    *px = (s8) (signX * x);
    *py = (s8) (signY * y);
}

static void ClampTrigger(u8* trigger, u8 min, u8 max)
{
    if (*trigger <= min) {
        *trigger = 0;
    } else {
        if (max < *trigger) {
            *trigger = max;
        }
        *trigger -= min;
    }
}

void PADClamp(PADStatus* status)
{
    int i;

    for (i = 0; i < 4; i++, status++) {
        if (status->err == PAD_ERR_NONE) {
            ClampStick(&status->stickX, &status->stickY, ClampRegion.maxStick, ClampRegion.xyStick, ClampRegion.minStick);
            ClampStick(&status->substickX, &status->substickY, ClampRegion.maxSubstick, ClampRegion.xySubstick,
                       ClampRegion.minSubstick);
            ClampTrigger(&status->triggerLeft, ClampRegion.minTrigger, ClampRegion.maxTrigger);
            ClampTrigger(&status->triggerRight, ClampRegion.minTrigger, ClampRegion.maxTrigger);
        }
    }
}

// GC: command 1 = motor on, 0 = off, 2 = hard stop. The game pulses it every frame (pad.cpp VibControl),
// so "on" maps to both Xbox motors at full and the duty cycle gives the intensity, as on the GameCube.
void PADControlMotor(int chan, u32 cmd)
{
    if (chan < 0 || chan >= XPAD_MAX) {
        return;
    }
    if (cmd > MOTOR_STOP_HARD) {
        cmd = MOTOR_STOP;
    }
    if (s_motorCmd[chan] == cmd) {
        return;
    }
    s_motorCmd[chan] = (u8) cmd;
    if (s_backendOk) {
        unsigned short level = (cmd == MOTOR_RUMBLE) ? 0xFFFF : 0;
        XPadBackend_Rumble(chan, level, level);
    }
}

}  // extern "C"

#endif  // RE4_XBOX
