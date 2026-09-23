// game/dmg.cpp: damage volumes (DmgMgr). Fire, explosions and traps register a cylinder or an
// XZ quad with a damage kind and a lifetime; the objects (boxes, doors, items...) and characters
// poll DmgMgr.hitCheck with their position each frame and react to the kind (1 / 4 / 5 / 7 break
// the breakable objects, 5 is fire). Volumes expire on their own.
// Original source: D:/Bio4/Prog/dmg.cpp.
#include "types.h"
#include "global.h"
#include "dmg.h"
#include "dbmodule.h"
#include "math_sub.h"

// A cManager<cDmg> pool of 0x118 byte works.
cDmgMgr::cDmgMgr() : cManager<cDmg>(0x118, 2)
{
    setName("cDmgMgr");
}

// Places a cylinder (id 0) or quad (id 1) volume into the fresh work.
int cDmgMgr::construct(cDmg* pDmg, int id)
{
    switch (id) {
    case ID_CYLINDER:
    default:
        new (pDmg) cDmgCyl;
        pDmg->be_flag = 1;
        break;
    case ID_POINT4:
        new (pDmg) cDmgP4;
        pDmg->be_flag = 1;
        break;
    }
    pDmg->m_Id = id;
    return 1;
}

// cManager hook (unsigned id).
int cDmgMgr::construct(cDmg* pDmg, u32 id)
{
    return construct(pDmg, (int) id);
}

// Per-frame: counts every live volume's lifetime down and destroys it at 0.
void cDmgMgr::move()
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        cDmg* p = fastAt(i);
        dieCheck();
        if (p->isAlive()) {
            if (--p->m_Time == 0) {
                destroy(p);
            }
        }
    }
}

// Registers a cylinder volume (centre, radius, half height) of `kind` for `time` frames; 1 when
// a work was free.
int cDmgMgr::set(int type, int time, Vec* pPos, f32 radius, f32 height)
{
    cDmgCyl* p = (cDmgCyl*) create(ID_CYLINDER);

    if (p == 0) {
        return 0;
    }
    p->m_Type = type;
    p->m_Time = time;
    p->m_Pos = *pPos;
    p->m_Radius = radius;
    p->m_Height = height;
    return 1;
}

// Registers an XZ quad volume (4 corners, half height) of `kind` for `time` frames.
int cDmgMgr::set(int type, int time, Vec* pPos4, f32 height)
{
    cDmgP4* p = (cDmgP4*) create(ID_POINT4);

    if (p == 0) {
        return 0;
    }
    p->m_Type = type;
    p->m_Time = time;
    p->m_Pos[0] = pPos4[0];
    p->m_Pos[1] = pPos4[1];
    p->m_Pos[2] = pPos4[2];
    p->m_Pos[3] = pPos4[3];
    p->m_Height = height;
    return 1;
}

// The kind of the first live volume containing `pos` (its centre in *out); 0 when none.
int cDmgMgr::hitCheck(Vec* pPos, Vec* pFrom)
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        cDmg* p = fastAt(i);
        if (p->isAlive()) {
            if (p->hitCheck(pPos, pFrom)) {
                return p->m_Type;
            }
        }
    }
    return 0;
}

// Point in cylinder (height band +-m_Height, XZ radius); *out = centre. Debug_flg[2] 0x10000000
// draws the volume.
int cDmgCyl::hitCheck(Vec* pPos, Vec* pFrom)
{
    if (DbgFlagChk(pG, DBG_OBA_VIEW)) {
        Draw_cylinder(&m_Pos, m_Radius, m_Height, 0xFFFFFFFF);
    }
    if (pPos->y > m_Pos.y + m_Height) {
        return 0;
    }
    if (pPos->y < m_Pos.y - m_Height) {
        return 0;
    }
    if ((pPos->x - m_Pos.x) * (pPos->x - m_Pos.x) + (pPos->z - m_Pos.z) * (pPos->z - m_Pos.z) > m_Radius * m_Radius) {
        return 0;
    }
    if (pFrom) {
        *pFrom = m_Pos;
    }
    return m_Type;
}

// Point in the XZ quad; *out = the corners' mean.
int cDmgP4::hitCheck(Vec* pPos, Vec* pFrom)
{
    u32 i;

    if (HitCheckPoint4(pPos, m_Pos)) {
        if (pFrom) {
            pFrom->x = 0.0f;
            pFrom->y = 0.0f;
            pFrom->z = 0.0f;
            for (i = 0; i < 4; i++) {
                PSVECAdd(pFrom, &m_Pos[i], pFrom);
            }
            PSVECScale(pFrom, pFrom, 0.25f);
        }
        return m_Type;
    }
    return 0;
}

// Event start: damage volumes are removed.
void cDmg::beginEvent(u32 flag)
{
    DmgMgr.destroy(this);
}

cDmgMgr DmgMgr;

// The split object carries 16 unnamed zero bytes after DmgMgr (0x802DA150): dvd.cpp's .bss follows
// at the next 32-byte boundary and ngcld does not pad for it. A zero-initialised static referenced
// only by a never-called inline is emitted after DmgMgr (first-declaration order) without a body.
static u8 dmg_pad[16];
