// game/lightInfo: per-model lighting info (D:/Bio4/Prog/lightInfo.cpp). cLightInfo (embedded in
// cModel) holds the model's light bounding volume (Offset/Size relative to a parts, Flag = shape
// type), the light enable/select masks and the up to 8 lights cLightMgr picked for it.
#include "model.h"
#include "db_log.h"
#include "math_sub.h"

// pointer to game memory (0x80000000 .. 0x82FFFFFF)

// Empty info: no lights, no masks, zero volume.
cLightInfo::cLightInfo()
{
    int i;

    for (i = 0; i < 8; i++) {
        pLight[i] = 0;
    }
    EnableMask = 0;
    Flag = 0;
    PartsNo = 0;
    x53 = 0;
    SelectMask = 0;
    Offset.x = Offset.y = Offset.z = 0.0f;
    Size.x = Size.y = Size.z = 0.0f;
}

// Sets the lighting volume: type (getType(): 0 capsule-like Size.x+Size.y, 1 sphere Size.x, else box
// diagonal) attached to parts partsNo (0 = the model origin), centre offset, size and the light
// kind enable mask. Returns 0 on invalid pointers.
int cLightInfo::init2(int type, int partsNo, const Vec* pOffset, const Vec* pSize, int mask)
{
    int i;

    if (!VALID_PTR(pOffset) || !VALID_PTR(pSize)) {
        pLog->err(0, 0, "cLightInfo::init2() PTR ERROR %08X %08X", pOffset, pSize);
        return 0;
    }
    for (i = 0; i < 8; i++) {
        pLight[i] = 0;
    }
    Flag = type;
    EnableMask = mask;
    PartsNo = partsNo;
    SelectMask = 0xFFFFFFFF;
    Offset = *pOffset;
    Size = *pSize;
    if (getType() == 0) {
        Radius = Size.x + Size.y;
    } else if (getType() != 2) {
        Radius = Size.x;
    } else {
        Radius = SQRTF(Size.x * Size.x + Size.y * Size.y + Size.z * Size.z);
    }
    return 1;
}

// Number of lights currently assigned to the model.
u32 cLightInfo::getLightNum()
{
    u8 n = 0;
    u8 i;

    for (i = 0; i < 8; i++) {
        if (pLight[i] != 0) {
            n++;
        }
    }
    return n;
}

// Rebuilds imat, the inverse of the volume's world matrix (offset scaled and rotated by the model,
// at the model position or the parts' world position), for the lighting tests.
void cLightInfo::updateMatrix(cModel* pMod)
{
    Vec v;
    Mtx tmp;

    v.x = Offset.x * pMod->scale.x;
    v.y = Offset.y * pMod->scale.y;
    v.z = Offset.z * pMod->scale.z;
    RotVector(&v, &pMod->ang, &v);
    if (PartsNo == 0) {
        PSVECAdd(&v, &pMod->pos, &v);
    } else {
        if (pMod->pList == 0) {
            return;
        }
        PSVECAdd(&v, &pMod->getPartsPtr(PartsNo - 1)->world, &v);
    }
    RotMatrix(tmp, &pMod->ang);
    TransMatrix(tmp, &v);
    PSMTXInverse(tmp, imat);
}

// World-space offset of the volume centre (rotated by the parts' matrix); returns the parts used.
cCoord* cLightInfo::getPos(cModel* m, Vec* out)
{
    cCoord* c;

    if (PartsNo > 0) {
        c = m->getPartsPtr(PartsNo - 1);
        if (!VALID_PTR(c)) {
            pLog->err(0, 0, "litHitCk PNo%d %d %d %x %x", PartsNo - 1, m->kindid, m->id, Flag, EnableMask);
            c = m;
        }
        PSMTXMultVecSR(c->mat, &Offset, out);
        PSVECAdd(out, &c->world, out);
    } else {
        c = m;
        PSMTXMultVecSR(c->mat, &Offset, out);
        PSVECAdd(out, &c->pos, out);
    }
    return c;
}
