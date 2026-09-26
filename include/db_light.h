#ifndef DB_LIGHT_H
#define DB_LIGHT_H

#include "types.h"

// Client view of the light editor object (tools/db_light.cpp owns the real class definition; the tool
// entry units t_light/t_camera/t_event/t_esp only construct it with `new`, poll move() and delete it).
// Only the members other units call are declared; the size is the original's (Debug heap `new 0x10CC`).
class cLightTool {
public:
    u8 pad[0x10CC];

    cLightTool();
    ~cLightTool();
    // 1 = running, 2 = player mode (the caller moves the player and camera), 0 = quit
    int move();
    void setLogMode(bool on);  // t_esp's build only
};

#endif
