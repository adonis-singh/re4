// game/map_obj: the cMap manager (D:/Bio4/Prog/map_obj.cpp). cMap units (kindid 2) are the room
// map/scroll model holders managed by cMapMgr (MapMgr); the retail build leaves cMap::move as a
// stub, the manager only ages them and shows the free count.
#include "types.h"
#include "cManager.h"
#include "map_obj.h"
#include "eprintf.h"

cMapMgr MapMgr;

// Manager of cMap units.
cMapMgr::cMapMgr() : cManager<cMap>(sizeof(cMap), 0)
{
    setName("cMapMgr");
}

// Unit construction: placement-news the cMap with `id` and index = number of alive maps sharing that id.
int cMapMgr::construct(cMap* pMap, u32 room_no)
{
    u8 n = 0;
    u32 i;

    i = 0;
    if (i < nArray) {
        do {
            cMap* q = getWork(i);
            if (q->isAlive() && (int)room_no == q->id) {
                n++;
            }
        } while (++i < nArray);
    }
    pMap = new (pMap) cMap;
    pMap->id = room_no;
    pMap->part = n;
    return 1;
}

// Finds the alive map with id `id` and index `no`.
cMap* cMapMgr::room(int id, int no)
{
    u32 i;

    i = 0;
    if (i < nArray) {
        do {
            cMap* p = getWork(i);
            if (p->isAlive() && id == p->id && no == p->part) {
                return p;
            }
        } while (++i < nArray);
    }
    return 0;
}

// Per-frame: moves every alive, active (be_flag 0x20) map and records its old position; shows the free count.
void cMapMgr::move()
{
    u32 i;

    i = 0;
    if (i < nArray) {
        do {
            cMap* p = getWork(i);
            if (p->isAlive()) {
                if (p->be_flag & 0x20) {
                    dieCheck();
                    p->move();
                    p->updateOldPos();
                }
            }
        } while (++i < nArray);
    }
    dispInfo();
}

// Debug: prints the number of free map slots at (472, 42).
int cMapMgr::dispInfo()
{
    u32 i;
    u32 n;

    if (pArray == 0) {
        return 0;
    }
    n = 0;
    for (i = 0; i < nArray; i++) {
        cMap* p = fastAt(i);
        if (p->isAlive()) {
            n++;
        }
    }
    eprintf(0x1D8, 0x2A, 0, 1, "%3d", nArray - n);
    return 1;
}

// New map unit: kindid 2, alive/visible flags.
cMap::cMap()
{
    kindid = 2;
    be_flag |= 0x23;
    setNoClip(1);
}

// Stub: only advances r_no_0 once.
void cMap::move()
{
    static int timer = 0;

    if (r_no_0 != 0) {
        return;
    }
    timer = 0;
    r_no_0++;
}

// unreferenced (the second .sdata word of the unit; the map lost its name)
static int MapObjTimer = 0;
