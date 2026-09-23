// game/obj03: object id 3, path-following chain (D:/Bio4/Prog/obj03.cpp): a model whose parts are
// laid 40 units apart along an effect path (PathGetMatEm) and slide along it at `speed` per
// frame (conveyor / chain links); flag bit 0 draws the path for debugging.
#include "obj.h"
#include "obj03.h"
#include "global.h"
#include "db_log.h"
#include "path.h"
#include "dbmodule.h"

// New chain: work cleared, small light volume.
cObj03::cObj03()
{
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 500.0f, 0.0f, 0.0f };

    r3 = 0;
    r2 = 0;
    r1 = 0;
    r0 = 0;
    pathLen = 0.0f;
    pathPos = 0.0f;
    speed = 0.0f;
    flag = 0;
    atari.throughOn();
    LightInfo.init2(1, 1, &p0, &p1, 1);
}

// Model-less init (parts only); resets the path parameter.
// Nobody calls this: the original linker dead-stripped it from .text (unit in STRIP_UNUSED)
// but left its message string and its 4-byte pool (one SF 0.0 at 0x8023F51C) behind, right
// before move's pool; so it is defined out of line here, between the ctor and move.
int cObj03::init()
{
    if (modelInit(NULL, NULL) == 0) {
        pLog->err(0, 0, "cObj03::init() modelInit() was failed.");
        return 0;
    }
    pathPos = 0.0f;
    return 1;
}

// Places each parts at t, t+40, ... along the path (wrapping at the path length), advances t by speed.
void cObj03::move()
{
    static Vec bp = { 0.0f, 0.0f, 0.0f };
    static u16 hist = 0;
    static Vec pos0;
    static Vec pos1;
    u16 h;
    f32 t = pathPos;
    int i;

    for (i = nParts - 1; i >= 0; i--) {
        cParts* parts = getPartsPtr(i);
        h = 0;
        PathGetMatEm(pPath, pPathParent, t, &h, parts->mat);
        if (flag & 1) {
            Mtx m;
            PSMTXConcat(pG->Camera.v_mat, parts->mat, m);
            Draw_local_pos(&bp, 10, m);
        }
        t += 40.0f;
        if (t > pathLen) {
            t -= pathLen;
        }
    }
    if (flag != 0) {
        Mtx m;
        int j;
        for (j = 0; j < 500; j++) {
            PathGetMatEm(pPath, pPathParent, pathLen * (f32)j / 500.0f, &hist, m);
            pos1.x = m[0][3];
            pos1.y = m[1][3];
            pos1.z = m[2][3];
            if (j != 0) {
                Draw_line3d(&pos0, &pos1, -1, 0);
            }
            pos0 = pos1;
        }
    }
    pathPos += speed;
    if (pathPos > pathLen) {
        pathPos -= pathLen;
    }
}

// The split object's .sdata is 8 bytes (hist + padding to the 8-aligned next unit).
asm(".section .sdata; .balign 8");
