// game/atariInfo.cpp: cAtariInfo, the collision body every character / object carries (cEm::atari):
// a vertical cylinder (radius m_radius2, half height m_height) or, with m_flag bit1, a box
// (m_radius x m_radius2) offset from the model or one of its parts. m_flag 0x100 = collides with
// the scenario, 0x200 = collides with other characters, bits 3-4 the push priority. The size can
// be interpolated toward a new one over m_hokan frames (crouching / dying characters).

#include "atariInfo.h"
#include "atari.h"
#include "model.h"
#include "global.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "main_mem.h"

// Zeroed info (no collision until init).
cAtariInfo::cAtariInfo()
{
    memclr_asm(this, sizeof(cAtariInfo));
}

// Full setup: offset x / y / z, size rx (x radius) / rz (z radius) / w (radius3) / hh (half
// height), the parts it follows (0 = the model), interpolation frames and flags | 0x300 (scenery
// and character collision on).
void cAtariInfo::init0(f32 ox, f32 oy, f32 oz, f32 rs, f32 ro, f32 ra, f32 h, int pno, int hokan, int f)
{
    m_offset.x = ox;
    m_offset.y = oy;
    m_offset.z = oz;
    m_radius_n = rs;
    m_radius2_n = ro;
    m_radius = rs;
    m_radius2 = ro;
    m_radius3 = ra;
    m_height = h;
    m_parts_no = pno;
    this->m_hokan = hokan;
    m_flag = f | 0x300;
    x48 = 0;
    m_stat = 1;
}

// init0 with the argument order most callers use, plus m_flag bit0.
void cAtariInfo::init(f32 ox, f32 oy, f32 oz, f32 rs, f32 ro, f32 ra, f32 h, int pno, int f, int hokan)
{
    init0(ox, oy, oz, rs, ro, ra, h, pno, hokan, f);
    m_flag |= 1;
}

// Changes the size: mode 0 sets radius / radius2 to a / b at once; mode > 0 interpolates toward
// a / b over `mode` frames; mode < 0 snaps to 100 / 100 first and interpolates over -mode frames.
void cAtariInfo::set(int mode, f32 a, f32 b)
{
    if (mode < 0) {
        m_radius_n = a;
        m_radius2_n = b;
        m_radius2 = 100.0f;
        m_radius = 100.0f;
        mode = -mode;
    } else {
        if (mode == 0) {
            m_radius = a;
            m_radius2 = b;
        }
        m_radius_n = a;
        m_radius2_n = b;
    }
    m_hokan = mode;
}

// Dead-stripped by the original linker (only its constant pool survives in .rodata).
static f32 atariInfoRange(f32 v)
{
    if (v < 250.0f) {
        return 600.0f;
    }
    return 100000.0f;
}

// World positions of the body's centre this frame and last frame (parts world / world_old, or
// the model pos / pos_old), at model height + radius; the sweep the scenery test uses.
void cAtariInfo::getSpeedVector(cModel* m, Vec* oldPos, Vec* newPos)
{
    Vec v;

    if (m_parts_no != 0) {
        cParts* p = m->getPartsPtr(m_parts_no - 1);
        v.x = m_offset.x;
        v.y = m_offset.y;
        v.z = m_offset.z;
        RotVector(&v, &m->ang, &v);
        PSVECAdd(&p->world, &v, newPos);
        PSVECAdd(&p->world_old, &v, oldPos);
        newPos->y = m->pos.y + m_radius;
        oldPos->y = m->pos_old.y + m_radius;
    } else {
        v.x = m_offset.x;
        v.y = m_offset.y + m_radius;
        v.z = m_offset.z;
        RotVector(&v, &m->ang, &v);
        PSVECAdd(&m->pos, &v, newPos);
        PSVECAdd(&m->pos_old, &v, oldPos);
    }
}

// Per-frame size interpolation toward m_radius_n / m_radius2_n over the remaining m_hokan frames.
void cAtariInfo::move()
{
    if (m_hokan != 0) {
        f32 c = (f32) m_hokan;
        m_hokan--;
        m_radius += (m_radius_n - m_radius) / c;
        m_radius2 += (m_radius2_n - m_radius2) / c;
    }
}

// World position of the body: the rotated offset added to the parts (or model) position.
void cAtariInfo::getPos(cModel* m, Vec* out)
{
    Vec v;

    RotVector(&m_offset, &m->ang, &v);
    if (m_parts_no != 0) {
        PSVECAdd(&m->getPartsPtr(m_parts_no - 1)->world, &v, out);
    } else {
        PSVECAdd(&m->pos, &v, out);
    }
}

// Push priority 0..3 (m_flag bits 3-4): the lower priority body gets pushed when two characters
// overlap (PRI_LV3 objects never move).
void cAtariInfo::setPriority(int pri)
{
    m_flag &= ~0x18;
    switch (pri) {
    case 0:
        break;
    case 1:
        m_flag |= 0x8;
        break;
    case 2:
        m_flag |= 0x10;
        break;
    case 3:
        m_flag |= 0x18;
        break;
    }
}

// Debug draw: the cylinder or the box.
void cAtariInfo::disp(cModel* pMod)
{
    if (m_flag & 2) {
        dispRect(pMod);
    } else {
        Vec p;
        getPos(pMod, &p);
        p.y -= m_height;
        Draw_cylinder(&p, m_radius2, m_height * 2.0f, 0xFFFFFFFF);
    }
}

// Debug draw of the box form: 12 triangles of the offset box in the parts / model yaw frame.
void cAtariInfo::dispRect(cModel* pMod)
{
    static u8 ptbl[36] = {
        0, 2, 1, 2, 3, 1, 4, 5, 6, 5, 7, 6, 2, 6, 3, 6, 7, 3,
        0, 1, 4, 1, 5, 4, 1, 3, 5, 3, 7, 5, 2, 0, 6, 0, 4, 6,
    };
    Vec v[8];
    Vec w[8];
    Vec size;
    Mtx mat;
    int i;

    size.x = m_radius;
    size.y = m_height;
    size.z = m_radius2;
    v[0].x = -size.x;
    v[0].y = -size.y;
    v[0].z = -size.z;
    v[1].x = size.x;
    v[1].y = -size.y;
    v[1].z = -size.z;
    v[2].x = -size.x;
    v[2].y = -size.y;
    v[2].z = size.z;
    v[3].x = size.x;
    v[3].y = -size.y;
    v[3].z = size.z;
    v[4].x = -size.x;
    v[4].y = size.y;
    v[4].z = -size.z;
    v[5].x = size.x;
    v[5].y = size.y;
    v[5].z = -size.z;
    v[6].x = -size.x;
    v[6].y = size.y;
    v[6].z = size.z;
    v[7].x = size.x;
    v[7].y = size.y;
    v[7].z = size.z;
    for (i = 0; i < 8; i++) {
        PSVECAdd(&v[i], &m_offset, &v[i]);
    }
    if (m_parts_no != 0) {
        cParts* p = pMod->getPartsPtr(m_parts_no - 1);
        PSMTXRotRad(mat, 'y', p->ang.y);
        TransMatrix(mat, &p->world);
        PSMTXConcat(pG->Camera.v_mat, mat, mat);
    } else {
        PSMTXRotRad(mat, 'y', pMod->ang.y);
        TransMatrix(mat, &pMod->pos);
        PSMTXConcat(pG->Camera.v_mat, mat, mat);
    }
    for (i = 0; i < 12; i++) {
        u8* t = &ptbl[i * 3];
        Draw_line3d_local(&v[t[0]], &v[t[1]], mat, 0xFFFFFFFF, 0);
        Draw_line3d_local(&v[t[1]], &v[t[2]], mat, 0xFFFFFFFF, 0);
        Draw_line3d_local(&v[t[2]], &v[t[0]], mat, 0xFFFFFFFF, 0);
    }
}
