// xbox/xpad_backend_sdl: XPadBackend_* on top of the SDL2 game controller API (nxdk's SDL2 port).
//
// Polling design: the game calls PADRead once per frame and PADReset for every port without a controller,
// so hot-plug is handled by rescanning (XPadBackend_Rescan) instead of consuming SDL device events.
// Event delivery is switched off for the same reason: nobody reads those events, and an unread queue
// fills up and starts dropping the events other parts of the game do care about.
//
// Assumptions about the SDL port, to check on hardware: SDL_CONTROLLER_BUTTON_LEFTSHOULDER is White and
// RIGHTSHOULDER is Black (swap the two lines in ReadButtons if the port maps them the other way), and
// SDL_GameControllerRumble (SDL >= 2.0.9) drives the Duke's motors.
#ifdef RE4_XBOX

#include "xbox/xpad_backend.h"

#include <SDL.h>

static SDL_GameController* s_ctrl[XPAD_MAX];
static unsigned short s_rumbleLow[XPAD_MAX];
static unsigned short s_rumbleHigh[XPAD_MAX];
static int s_inited = 0;

// A rumble request expires on its own after this long. The layer refreshes a running request every frame
// (XPadBackend_Read), so this only matters if the game stops calling us with the motor on.
#define RUMBLE_TIMEOUT_MS 250

static int IsOpen(SDL_JoystickID id)
{
    int i;
    for (i = 0; i < XPAD_MAX; i++) {
        if (s_ctrl[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s_ctrl[i])) == id) {
            return 1;
        }
    }
    return 0;
}

static short FlipAxis(short v) { return v == -32768 ? 32767 : (short) -v; }  // SDL: up is negative

static unsigned short ReadButtons(SDL_GameController* c)
{
    unsigned short b = 0;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) b |= XPAD_DPAD_UP;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) b |= XPAD_DPAD_DOWN;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) b |= XPAD_DPAD_LEFT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) b |= XPAD_DPAD_RIGHT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START)) b |= XPAD_START;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK)) b |= XPAD_BACK;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSTICK)) b |= XPAD_LTHUMB;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) b |= XPAD_RTHUMB;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A)) b |= XPAD_A;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B)) b |= XPAD_B;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X)) b |= XPAD_X;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y)) b |= XPAD_Y;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) b |= XPAD_WHITE;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) b |= XPAD_BLACK;
    return b;
}

extern "C" {

int XPadBackend_Init(void)
{
    if (s_inited) {
        return 1;
    }
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        return 0;
    }
    SDL_GameControllerEventState(SDL_IGNORE);
    SDL_JoystickEventState(SDL_IGNORE);
    s_inited = 1;
    return 1;
}

void XPadBackend_Rescan(void)
{
    int i;
    int n;
    int j;

    if (!s_inited) {
        return;
    }
    SDL_PumpEvents();  // refreshes SDL's device list

    for (i = 0; i < XPAD_MAX; i++) {
        if (s_ctrl[i] && !SDL_GameControllerGetAttached(s_ctrl[i])) {
            SDL_GameControllerClose(s_ctrl[i]);
            s_ctrl[i] = NULL;
            s_rumbleLow[i] = s_rumbleHigh[i] = 0;
        }
    }

    n = SDL_NumJoysticks();
    for (j = 0; j < n; j++) {
        SDL_JoystickID id;
        if (!SDL_IsGameController(j)) {
            continue;
        }
        id = SDL_JoystickGetDeviceInstanceID(j);
        if (id < 0 || IsOpen(id)) {
            continue;
        }
        for (i = 0; i < XPAD_MAX; i++) {
            if (!s_ctrl[i]) {
                s_ctrl[i] = SDL_GameControllerOpen(j);
                break;
            }
        }
        if (i == XPAD_MAX) {
            break;  // all four ports taken
        }
    }
}

int XPadBackend_Read(int port, XPadRaw* out)
{
    SDL_GameController* c;

    if (!s_inited || port < 0 || port >= XPAD_MAX) {
        return 0;
    }
    if (port == 0) {
        SDL_PumpEvents();
        SDL_GameControllerUpdate();  // once per frame is enough for all ports
    }
    c = s_ctrl[port];
    if (!c || !SDL_GameControllerGetAttached(c)) {
        return 0;
    }

    out->buttons = ReadButtons(c);
    out->lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    out->ly = FlipAxis(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
    out->rx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX);
    out->ry = FlipAxis(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY));
    // SDL reports triggers as 0..32767
    out->lt = (unsigned char) (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
    out->rt = (unsigned char) (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);

#if SDL_VERSION_ATLEAST(2, 0, 9)
    if (s_rumbleLow[port] || s_rumbleHigh[port]) {
        SDL_GameControllerRumble(c, s_rumbleLow[port], s_rumbleHigh[port], RUMBLE_TIMEOUT_MS);
    }
#endif
    return 1;
}

void XPadBackend_Rumble(int port, unsigned short low, unsigned short high)
{
    if (port < 0 || port >= XPAD_MAX) {
        return;
    }
    s_rumbleLow[port] = low;
    s_rumbleHigh[port] = high;
#if SDL_VERSION_ATLEAST(2, 0, 9)
    if (s_ctrl[port]) {
        // off must take effect immediately, not wait for the next Read
        SDL_GameControllerRumble(s_ctrl[port], low, high, (low || high) ? RUMBLE_TIMEOUT_MS : 0);
    }
#endif
}

}  // extern "C"

#endif  // RE4_XBOX
