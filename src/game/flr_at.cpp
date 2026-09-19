// game/flr_at: floor attribute areas (D:/Bio4/Prog/flr_at.cpp). The room's "FSE" block lists
// FlrAt areas (type, group, area polygon, per-type data) used for footstep sounds/effects (type 0/1),
// sound situations (type 3) and puddles; FlrAtCheck finds the enabled area of a type containing a
// point, FlrAtSetDefVal sets the default footstep SE / effect per group.
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flr_at.h"
#include "area.h"
#include "global.h"
#include "db_log.h"
#include "main_mem.h"

void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp
extern "C" int strcmp(const char* a, const char* b);

FlrSys FlrAt_sys;
FlrSys* pFlrSys;

// Reference store: a scalar (non-struct) MEM, so the following pFlrSys load is not hoisted
// above it and is reloaded (light.cpp PSet).
static inline void PSet(void*& d, void* v) { d = v; }

// Room init: binds the room archive's "FSE" block (version 0x103) as the floor attribute list.
void FlrAtInit()
{
    FlrAtHead* p;

    pFlrSys = &FlrAt_sys;
    memclr_asm(&FlrAt_sys, sizeof(FlrAt_sys));
    p = (FlrAtHead*) GetDataExt(pG->pRoom, "FSE", 0);
    if (p == 0) {
        return;
    }
    if (strcmp((char*) p, "FSE") != 0) {
        return;
    }
    if (p->version != 0x103) {
        pLog->warn(0, 0, "FlrAt DATA IS OLD VERSION");
        return;
    }
    PSet(pFlrSys->pData, p);
    pFlrSys->pList = (FlrAt*) (p + 1);
}

// Returns the enabled floor attribute of `type` whose area contains pos (+300 y) and whose group
// matches the current group (0xFF = any); NULL outside the main game step (Rno0 != 3), during
// Status_flg[0] 0x10000000, or when none hits.
FlrAt* FlrAtCheck(int type, Vec* pos, int flag)
{
    Vec p;
    FlrAt* at;
    u32 i;
    int hit = 0;

    if (pG->Rno0 != 3) {
        return 0;
    }
    if (StaFlagChk(pG, STA_MOVIE_ON)) {
        return 0;
    }
    if (pFlrSys == 0) {
        return 0;
    }
    if (pFlrSys->pData == 0) {
        return 0;
    }
    p.x = pos->x;
    p.y = pos->y + 300.0f;
    p.z = pos->z;
    for (i = 0; i < ((FlrAtHead*) pFlrSys->pData)->num; i++) {
        at = &pFlrSys->pList[i];
        if ((at->flag & 1) == 0) {
            continue;
        }
        if (at->group != pFlrSys->group && pFlrSys->group != 0xFF) {
            continue;
        }
        if (at->type != type) {
            continue;
        }
        if (AreaHitCheck(at->area, &p) != 1) {
            continue;
        }
        if (type != 0) {
            hit = 1;
        } else if (at->x46[1] & flag) {
            hit = 1;
        }
        if (hit == 1) {
            return at;
        }
    }
    return 0;
}

// Enables floor attribute entry `no` (flag bit 0).
// Dead-stripped by the original linker (only their strings survive in .rodata).
static int FlrAtSetEnable(int no)
{
    if (pFlrSys->pData == 0) {
        pLog->err(0, 0, "FlrAtSetEnable() : AT DATA NOT FOUND");
        return 0;
    }
    pFlrSys->pList[no].flag |= 1;
    return 1;
}

// Disables floor attribute entry `no`.
static int FlrAtSetDisable(int no)
{
    if (pFlrSys->pData == 0) {
        pLog->err(0, 0, "FlrAtSetDisable() : AT DATA NOT FOUND");
        return 0;
    }
    pFlrSys->pList[no].flag &= ~1;
    return 1;
}

// Sets the default footstep SE set and effect number for group `no` (0..0x3F; 0xFF = the default slot 0x40).
int FlrAtSetDefVal(u32 no, u8 foot_se_set, u8 eff_no)
{
    if (no != 0xFF && no > 0x3F) {
        pLog->err(0, 0, "FlrAt : group %d Illegal No.", no);
        return 0;
    }
    no = no == 0xFF ? 0x40 : no;
    pFlrSys->foot_se[no] = foot_se_set;
    pFlrSys->foot_esp[no] = eff_no;
    return 1;
}
