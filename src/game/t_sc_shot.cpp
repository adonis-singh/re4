#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "t_util.h"

extern int ScreenShotTriggerType;

// Debug screenshot settings task: the A-button trigger mode (held / press / press-to-press) and
// whether the debug display is kept in shots; B leaves.
void ToolScreenShot()
{
    TOOL_MENU menu[2] = {
        {1, "SCREEN SHOT TRIGGER", NULL},
        {1, "DEBUG DISP", NULL},
    };
    static const char* trg_str[3] = {"A PUSHING", "A PUSH", "A PUSH_TO_PUSH"};
    u32 stop_bak;
    s8 cursor;
    int type;
    int n;

    stop_bak = pG->Stop_flg;
    BitOn(pG->Stop_flg, ~0x4000);
    DbgFlagOn(pG, DBG_TEST_MODE);
    cursor = 0;
    while (1) {
        ToolMenuDisp_cur(60, 80, 0, &cursor, menu, sizeof(menu), Joy);
        switch (cursor) {
        case 0:
            type = ScreenShotTriggerType;
            if (Joy[0].rep2 & 0x20002) {
                type++;
            }
            if (Joy[0].rep2 & 0x10001) {
                type--;
            }
            if (type >= 0) {
                n = type;
                if (n > 2) {
                    n = 2;
                }
            } else {
                n = 0;
            }
            ScreenShotTriggerType = n;
            break;
        case 1:
            if (Joy[0].rep2 & 0x30003) {
                if (pG->debug_disp == 0) {
                    pG->debug_disp = 1;
                } else {
                    pG->debug_disp = 0;
                }
            }
            break;
        }
        eprintf(228, 80, 0, 0, "%s", trg_str[ScreenShotTriggerType]);
        eprintf(228, 96, 0, 0, "%s", pG->debug_disp == 0 ? "OFF" : "ON");
        if (Joy[0].trg & JOY_B) {
            break;
        }
        TaskSleep(1);
    }
    BitSet(pG->Stop_flg, stop_bak);
    DbgFlagOff(pG, DBG_TEST_MODE);
    TaskExit();
}
