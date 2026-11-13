// Minimal copy of the parts of include/dolphin/pad.h that src/lib/Padclamp.c uses (types and one
// constant, layout identical to the real header). Test-only.
#ifndef STUB_DOLPHIN_PAD_H
#define STUB_DOLPHIN_PAD_H

typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;

#define PAD_ERR_NONE 0

typedef struct PADStatus {
    u16 button;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;
    u8 triggerLeft;
    u8 triggerRight;
    u8 analogA;
    u8 analogB;
    s8 err;
} PADStatus;

typedef struct PADClampRegion {
    u8 minTrigger;
    u8 maxTrigger;
    s8 minStick;
    s8 maxStick;
    s8 xyStick;
    s8 minSubstick;
    s8 maxSubstick;
    s8 xySubstick;
    s8 radStick;
    s8 radSubstick;
} PADClampRegion;

void PADClamp(PADStatus* status);
void PADClampCircle(PADStatus* status);

#endif
