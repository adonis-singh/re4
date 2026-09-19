#include "types.h"
#include "light.h"
#include "dbg_tool.h"
#include "area.h"
#include "db_cam.h"
#include "block.h"
#include "scheduler.h"
#include "dbmodule.h"
#include "t_util.h"

// Effect area editor (Tools/t_esp_area.cpp): a cDbgToolMain<ESP_AREA> over the room's 32 effect
// trigger areas (.ear files), edited with the area editor of game/area.cpp.

extern "C" {
void* memset(void* dst, int c, unsigned int n);
// game/sub2.cpp; really takes Vec*, declared by value here (same ABI) so the caller copies its Vec.
int GetScreenPos(Vec pos, Vec* scr);
}

class cPlayer;
extern cPlayer* pPL;
// cModel::pos without em.h (the unit's .rodata has no atari.h string)
static inline Vec* PlPos() { return (Vec*) ((u8*) pPL + 0x94); }

struct ESP_AREA {
    u8 no;          // 0x00
    u8 be_flag;       // 0x01  bit 0: in use
    u8 area_no;      // 0x02
    u8 x3;
    AreaData area;  // 0x04
    u32 flags34;    // 0x34  bit 0: in room
    u32 x38[0x18];
};

#define ESP_AREA_MAX 32

static ESP_AREA esp_area_work[ESP_AREA_MAX];

// cDbgToolMain hook: slot in use (be_flag bit 0).
int IsWorkAlive(ESP_AREA* w)
{
    if (w->be_flag & 1) {
        return 1;
    }
    return 0;
}

// cDbgToolMain hook: sets / clears the in-use bit.
void SetWorkAlive(ESP_AREA* w, int alive)
{
    if (alive == 1) {
        w->be_flag |= 1;
    } else {
        w->be_flag &= ~1;
    }
}

// cDbgToolMain hook: slot number.
int GetWorkNo(ESP_AREA* w)
{
    return w->no;
}

// cDbgToolMain hook: slot number.
void SetWorkNo(ESP_AREA* w, int no)
{
    w->no = no;
}

// New slot: a 7000 x 5000 square area at the player's position.
void InitWork(ESP_AREA* w, int no)
{
    memclr_asm(w, sizeof(ESP_AREA));
    w->no = no;
    AreaDataInit(&w->area, PlPos(), 1, 7000.0f, 5000.0f);
}

// Position column pressed: runs the shared AreaDataEdit editor on the slot's area (info / help
// panels); returns 0 on B (column done).
int PosExec_callback(int no, ESP_AREA* w, cDbgButtonTemplate<ESP_AREA>* b)
{
    AreaData* a = &w->area;

    AreaDataEdit(a, 0xA0FF8080, 1, 0, 2.3f);
    AreaDataInfoDisp(a, 0x28, 0x18);
    AreaDataHelpDisp(a, 0x108, 8);
    {
        u32 t = Joy[0].trg & 0x200;
        return t == 0;
    }
}

// Position column text: area centre (x y z in metres) and height.
void PosUpdate_callback(int no, ESP_AREA* w, cDbgButtonTemplate<ESP_AREA>* b)
{
    char buf[64];
    Vec pos = {0.0f, 0.0f, 0.0f};
    f32 h = 0.0f;

    if (IsWorkAlive(w)) {
        AreaGetCenterPos(&pos, &w->area);
        PSVECScale(&pos, &pos, 0.001f);
        h = w->area.u.xz4.height / 1000.0f;
    }
    sprintf(buf, "%6.1f %6.1f %6.1f %6.1f", pos.x, pos.y, pos.z, h);
    DbgButtonSetName(b, buf);
}

// Data column pressed: rows AREA NO (left/right +-1, x10 with A) and IN ROOM (toggle, flags34
// bit 0); B done.
int AreaNoExec_callback(int no, ESP_AREA* w, cDbgButtonTemplate<ESP_AREA>* b)
{
    static int cursor = 0;
    int step = 0;
    u32 rep;

    eprintf(0xAA, 0xA0, 4, 0, "AREA NO : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->area_no);
    eprintf(0xAA, 0xB0, 4, 0, "IN ROOM : ");
    if (w->flags34 & 1) {
        eprintf(0xAA, 0xB0, 0, 0, "          ON");
    } else {
        eprintf(0xAA, 0xB0, 0, 0, "          OFF");
    }
    if (pG->Frame_cnt & 7) {
        eprintf(0x9A, (cursor + 10) * 16, 0, 0, cDbgStr::cursor());
    }
    rep = Joy[0].rep;
    if (rep & 0x80008) {
        cursor--;
    }
    if (rep & 0x40004) {
        cursor++;
    }
    if (cursor < 0) {
        cursor = 2;
    }
    if (cursor > 1) {
        cursor = 0;
    }
    switch (cursor) {
    case 0:
        if (rep & 0x10001) {
            step = -1;
        }
        if (rep & 0x20002) {
            step = 1;
        }
        if (Joy[0].on & 0x100) {
            step *= 10;
        }
        w->area_no += step;
        if ((Joy[0].on & 0x800) && (Joy[0].trg & 0x100)) {
            w->area_no = 0;
        }
        break;
    case 1:
        if ((rep & 0x30003) || (Joy[0].trg & 0x100)) {
            w->flags34 ^= 1;
        }
        break;
    }
    {
        u32 t = Joy[0].trg & 0x200;
        return t == 0;
    }
}

// Data column text: the area number digits and the in-room mark.
void AreaNoUpdate_callback(int no, ESP_AREA* w, cDbgButtonTemplate<ESP_AREA>* b)
{
    static char digits[] = "0123456789";
    u32 n = w->area_no;
    char buf[4];

    if (n <= 99) {
        buf[0] = ' ';
    } else {
        buf[0] = digits[n / 100];
        n %= 100;
    }
    buf[3] = 0;
    buf[1] = digits[n / 10];
    buf[2] = digits[n % 10];
    DbgButtonSetName(b, buf);
}

// OPTION window: FOG on/off (Disp_flg 0x4000); B closes.
void OptionExec()
{
    static int cursor = 0;
    u32 rep;

    eprintf(0xAA, 0xA0, 4, 0, "FOG : ");
    if (DpfFlagChk(pG, DPF_FOG)) {
        eprintf(0xAA, 0xA0, 0, 0, "       ON");
    } else {
        eprintf(0xAA, 0xA0, 0, 0, "       OFF");
    }
    if (pG->Frame_cnt & 7) {
        eprintf(0x9A, (cursor + 10) * 16, 0, 0, cDbgStr::cursor());
    }
    rep = Joy[0].rep;
    if (rep & 0x80008) {
        cursor--;
    }
    if (rep & 0x40004) {
        cursor++;
    }
    if (cursor < 0) {
        cursor = 1;
    }
    if (cursor > 0) {
        cursor = 0;
    }
    switch (cursor) {
    case 0:
        if ((rep & 0x30003) || (Joy[0].trg & 0x100)) {
            if (DpfFlagChk(pG, DPF_FOG)) {
                DpfFlagOff(pG, DPF_FOG);
            } else {
                DpfFlagOn(pG, DPF_FOG);
            }
        }
        break;
    }
}

void tEspAreaInit();
void tEspAreaExit();

// Effect area editor entry (debug menu 32): builds the cDbgToolMain windows (menu, file windows on
// X:\Soft\Room\st<n>\r<room>\ *.ear, edit table with Position / Data columns), loads r<room>00.ear,
// then loops: START toggles the debug camera, otherwise the tool runs and every area is drawn
// with its number (in-room ones highlighted); exits when the tool quits.
void ToolEspArea()
{
    u8 wait = 0;
    cDbgToolMain<ESP_AREA> tool;
    char path1[64];
    char path2[32];
    char buf[128];
    u8 cnt = 0;
    int cam = 0;
    u32 i;

    TutilInitDefault();
    tEspAreaInit();
    tool.CreateMenuWindow();
    sprintf(path1, "X:\\Soft\\Room\\st%d\\r%03x\\", pG->stage_no, pG->room_id);
    sprintf(path2, "r%03x", pG->room_id);
    tool.CreateFileWindows(0x16, 0xA, path1, path2, ".ear");
    tool.CreateEditWindow(4, 0x19, esp_area_work, "No ========Position=========== =Data=", 5, ESP_AREA_MAX);
    tool.AddEditColumn(3, "                          ", 1, PosExec_callback, PosUpdate_callback);
    tool.AddEditColumn(0x20, "    ", 2, AreaNoExec_callback, AreaNoUpdate_callback);
    tool.SetIsWorkAliveFunc(IsWorkAlive);
    tool.SetSetWorkAliveFunc(SetWorkAlive);
    tool.SetGetWorkNoFunc(GetWorkNo);
    tool.SetSetWorkNoFunc(SetWorkNo);
    tool.SetInitWorkFunc(InitWork);
    // The gcse PRE copies of &path1 / &path2 (refs 3 each, live from the sprintfs to the default-file
    // sprintf below) tie at global priority 29 only when the tool loop's blocks hold exactly the
    // original insn count; the ctor anchor in dbg_tool.h adds one, so a fourth (codeless) ref on
    // &path1 ranks it at 77 instead and it keeps r18 (&path2 stays 29 -> r17). This slot before the
    // InitAllWork loop has a free issue cycle at sched2; elsewhere the asm displaces an insn.
    asm("" : "=m"(buf[0]) : "r"(path1)); // COMPILER-DIFF: #17 (global.c refs, the r18/r17 pair)
    tool.InitAllWork();

    // the room's default file
    sprintf(buf, "%s%s00.ear", path1, path2);
    tool.LoadData(buf, esp_area_work, ESP_AREA_MAX);

    for (;;) {
        ESP_AREA* w;
        Vec pos;
        Vec scr;

        if (Joy[0].trg & 0x1000) {
            cam ^= 1;
        }
        w = esp_area_work;
        for (i = 0; i < ESP_AREA_MAX; i++, w++) {
            if (IsWorkAlive(w)) {
                AreaGetCenterPos(&pos, &w->area);
                pos.y = (pos.y + w->area.u.xz4.height) * 0.5f;
                if (GetScreenPos(pos, &scr) == 1) {
                    u32 col1;
                    u32 col2;

                    if (w->flags34 & 1) {
                        col1 = 0xA06060FF;
                        col2 = 0x608080C0;
                    } else {
                        col1 = 0xA0FF8080;
                        col2 = 0x60808080;
                    }
                    if (i == tool.GetEdit()->GetCurrentNo()) {
                        AreaDataDisp(&w->area, col1, 1, 0);
                        eprintf2(8, 12, (int) scr.x + 8, (int) scr.y + 0x10, 6, 0, "%d", w->area_no);
                    } else {
                        AreaDataDisp(&w->area, col2, 1, 0);
                        eprintf2(8, 12, (int) scr.x + 8, (int) scr.y + 0x10, 0, 0, "%d", w->area_no);
                    }
                }
            }
        }
        Draw_sphere(PlPos(), 600.0f, 0xFF404080, 1, 1);
        if (cam) {
            int c = cnt;

            CamDbg.move(&pG->Cam, &Joy[0], 1);
            cnt = (u8) (c + 1);
            if (c & 8) {
                eprintf2(0xE, 0x12, 0xAA, 0x18, 6, 0, "CAMERA MODE");
            }
        } else {
            if (tool.Update() == 0) {
                break;
            }
            if (tool.GetMode() == 4) {
                if (wait == 0) {
                    OptionExec();
                } else {
                    wait--;
                }
            } else if (tool.GetMode() == 2) {
                // separate compares: `!= 2 && != 3` would fold into a subi/cmplwi range test
            } else if (tool.GetMode() == 3) {
            } else {
                wait = 1;
            }
            tool.Disp();
        }
        TaskSleep(1);
    }
    DbgFlagOff(pG, DBG_DBG_CAM);
    tEspAreaExit();
    TutilQuitDefault();
    TaskExit();
}

// Pauses the game and turns on the debug displays (Stop / Disp flag bits, Debug_flg[0] bit 28),
// camera target type 4, all blocks visible.
void tEspAreaInit()
{
    SpfFlagOn(pG, SPF_EM);
    SpfFlagOn(pG, SPF_PL);
    SpfFlagOn(pG, SPF_ESP);
    SpfFlagOn(pG, SPF_SCE);
    SpfFlagOn(pG, SPF_SCE_AT);
    SpfFlagOn(pG, SPF_EARTHQUAKE);
    SpfFlagOn(pG, SPF_MIST);
    DpfFlagOn(pG, DPF_SUBCHAR);
    DpfFlagOn(pG, DPF_PL);
    DpfFlagOn(pG, DPF_ESP);
    DpfFlagOn(pG, DPF_SHADOW);
    DpfFlagOn(pG, DPF_FILTER);
    DbgFlagOn(pG, DBG_DBG_CAM);
    CamDbg.m_target_type = 4;
    Block.dispAllBlock(1);
}

// Undoes tEspAreaInit (fog display off too).
void tEspAreaExit()
{
    SpfFlagOff(pG, SPF_EM);
    SpfFlagOff(pG, SPF_PL);
    SpfFlagOff(pG, SPF_ESP);
    SpfFlagOff(pG, SPF_SCE);
    SpfFlagOff(pG, SPF_SCE_AT);
    SpfFlagOff(pG, SPF_EARTHQUAKE);
    SpfFlagOff(pG, SPF_MIST);
    DpfFlagOff(pG, DPF_SUBCHAR);
    DpfFlagOff(pG, DPF_PL);
    DpfFlagOff(pG, DPF_ESP);
    DpfFlagOff(pG, DPF_SHADOW);
    DpfFlagOff(pG, DPF_FILTER);
    DpfFlagOff(pG, DPF_FOG);
    DbgFlagOff(pG, DBG_DBG_CAM);
    {
        // through a volatile pointer: the store keeps `&CamDbg` in a register (`stb 0xf(rX)`)
        volatile debugCamera* c = &CamDbg;
        c->m_target_type = 0;
    }
    Block.dispAllBlock(0);
}
