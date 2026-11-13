// xbox/xpad_backend.h: the only thing the Xbox controls layer needs from the platform.
//
// src/xbox/pad_xbox.cpp implements the Dolphin PAD calls the game makes (PADInit, PADRead, ...) on top
// of these four functions. A backend turns "whatever the Xbox gives us" into the neutral XPadRaw below
// and knows nothing about the game. Two backends exist:
//   src/xbox/xpad_backend_sdl.cpp   SDL2 game controller API (nxdk's SDL2 port)
//   tools/xbox/pad_test.cpp         scripted fake used by the host unit test
// Adding another one (nxdk's own USB/XID stack, say) means implementing these functions and nothing else.
//
// Plain C types only: this header must not pull in the game's types.h, which clashes with Windows-style
// headers on the Xbox toolchain.
#ifndef XBOX_XPAD_BACKEND_H
#define XBOX_XPAD_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#define XPAD_MAX 4  // the console has four controller ports, same as the GameCube

// XPadRaw.buttons. White and Black are the original Duke's two shoulder buttons.
#define XPAD_DPAD_UP    0x0001
#define XPAD_DPAD_DOWN  0x0002
#define XPAD_DPAD_LEFT  0x0004
#define XPAD_DPAD_RIGHT 0x0008
#define XPAD_START      0x0010
#define XPAD_BACK       0x0020
#define XPAD_LTHUMB     0x0040  // left stick click
#define XPAD_RTHUMB     0x0080  // right stick click
#define XPAD_A          0x0100
#define XPAD_B          0x0200
#define XPAD_X          0x0400
#define XPAD_Y          0x0800
#define XPAD_BLACK      0x1000
#define XPAD_WHITE      0x2000

typedef struct XPadRaw {
    unsigned short buttons;  // XPAD_* bits
    short lx, ly;            // left stick,  -32768..32767, +y = UP (the backend flips SDL's axis)
    short rx, ry;            // right stick, same convention
    unsigned char lt, rt;    // analog triggers, 0..255
} XPadRaw;

// One-time start of the input system. Returns 1 on success, 0 if no input system is available
// (the layer then reports "no controller" on every port instead of failing).
int XPadBackend_Init(void);

// Re-checks which controllers are plugged in: opens new ones, drops unplugged ones. Cheap enough to call
// every frame; the game itself triggers it through PADReset for any port that reports no controller.
void XPadBackend_Rescan(void);

// Samples `port` (0..XPAD_MAX-1). Returns 1 and fills *out when a controller is attached, 0 otherwise.
// Called once per frame per port, so this is also the place to pump the input system.
int XPadBackend_Read(int port, XPadRaw* out);

// Sets both rumble motors of `port` (0 = off, 65535 = full). The game already pulse-width-modulates the
// motor itself (pad.cpp VibControl), so the layer only ever asks for full or off.
void XPadBackend_Rumble(int port, unsigned short low, unsigned short high);

#ifdef __cplusplus
}
#endif

#endif
