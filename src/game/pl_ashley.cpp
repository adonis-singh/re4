// game/pl_ashley: cPlAshley, the player class for the Ashley chapter (pl_type 1): builds her
// model set (body, face, hair, skirt) from the player archive, the room-dependent motion table
// (pl01weaponSet), the two hand models, and adds the bust bounce (moveBust) on top of cPlayer.
// She has no weapon; the cloth (hair / skirt / sweater) runs through pl_cloth.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "joy.h"
#include "pl_cloth.h"
#include "math_sub.h"

extern "C" {
void OSReport(const char* fmt, ...);
void EspDataLoad(void* data, int a, int b);     // game/eff_sys.cpp
void ReleaseWepData();                          // game/read.cpp
f32 sinf(f32 x);
}

extern u8 pl_fs_tbl[];   // game/foot_shadow_tbl.cpp (incomplete type: full address, not @sda21)

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(cModelInfo*& d, cModelInfo* v) { d = v; }

// Builds the Ashley player (pl_type 1 / the "Ashley chapter"): common init, model set, bust rest
// positions (parts 0x1D / 0x1E / 0x1A), motion table, her effect data (archive 0x1A), foot shadows.
cPlAshley::cPlAshley()
{
    init0();
    setModel();
    matUpdate();
    bustBase[0] = getPartsPtr(0x1D)->pos;
    bustBase[1] = getPartsPtr(0x1E)->pos;
    bustBase[2] = getPartsPtr(0x1A)->pos;
    pl01weaponSet(this);
    ReleaseWepData();
    init1();
    EspDataLoad(PL_ARC_PTR(pG->pPlayer, 0x1A), 3, 0);
    startUp();
    pFootShadowTbl = pl_fs_tbl;
}

// cPlayer::move plus the bust bounce.
void cPlAshley::move()
{
    cPlayer::move();
    moveBust();
}

// Nothing to fix up before the matrix pass (Leon uses it for the head).
void cPlAshley::moveMatCalcBefore()
{
}

// Loads the body (archive 4/5) and adds the face (7/9, also Body->pShape for the face morphs), hair
// (6/0xB), the skirt (8, be_flag 0x40) and 0xA, then the default face and empty hands.
void cPlAshley::setModel()
{
    cModelInfo* info;

    info = (cModelInfo*) modelInit(PL_ARC_PTR(pG->pPlayer, 4), PL_ARC_PTR(pG->pPlayer, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 7), PL_ARC_PTR(pG->pPlayer, 9));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(Body->pShape, info);
    PSet(Body->pHeadData, PL_ARC_PTR(pG->pPlayer, 7));
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 6), PL_ARC_PTR(pG->pPlayer, 0xB));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    addModel(info);
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 8), PL_ARC_PTR(pG->pPlayer, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    info->be_flag |= 0x40;
    addModel(info);
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 0xA), PL_ARC_PTR(pG->pPlayer, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlAshley::setModel() failed.");
        return;
    }
    addModel(info);
    TevScaleGroup = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(0);
}

// Ashley's motion table (m_MotTbl, 0x6D entries) from the player archive: a separate set for room
// 20E (the crate-carrying / cabin section), the normal set elsewhere.
void pl01weaponSet(cPlayer* pl)
{
    int i;

    for (i = 0; i < 0x6D; i++) {
        pl->m_MotTbl[i] = 0;
    }
    if (pG->stage_no == 2 && pG->room_no == 0xE) {
        PSet(pl->m_MotTbl[0x00], PL_ARC_PTR(pG->pPlayer, 0x80));
        PSet(pl->m_MotTbl[0x02], PL_ARC_PTR(pG->pPlayer, 0x81));
        PSet(pl->m_MotTbl[0x03], PL_ARC_PTR(pG->pPlayer, 0x9A));
        PSet(pl->m_MotTbl[0x06], PL_ARC_PTR(pG->pPlayer, 0x83));
        PSet(pl->m_MotTbl[0x07], PL_ARC_PTR(pG->pPlayer, 0x9C));
        PSet(pl->m_MotTbl[0x08], PL_ARC_PTR(pG->pPlayer, 0x82));
        PSet(pl->m_MotTbl[0x09], PL_ARC_PTR(pG->pPlayer, 0x9B));
        PSet(pl->m_MotTbl[0x0B], PL_ARC_PTR(pG->pPlayer, 0x84));
        PSet(pl->m_MotTbl[0x0C], PL_ARC_PTR(pG->pPlayer, 0x9D));
        PSet(pl->m_MotTbl[0x0D], PL_ARC_PTR(pG->pPlayer, 0x86));
        PSet(pl->m_MotTbl[0x0E], PL_ARC_PTR(pG->pPlayer, 0x9F));
        PSet(pl->m_MotTbl[0x0F], PL_ARC_PTR(pG->pPlayer, 0x85));
        PSet(pl->m_MotTbl[0x10], PL_ARC_PTR(pG->pPlayer, 0x9E));
        PSet(pl->m_MotTbl[0x3F], PL_ARC_PTR(pG->pPlayer, 0x8E));
        PSet(pl->m_MotTbl[0x40], PL_ARC_PTR(pG->pPlayer, 0x8F));
        PSet(pl->m_MotTbl[0x39], PL_ARC_PTR(pG->pPlayer, 0x90));
        PSet(pl->m_MotTbl[0x3A], PL_ARC_PTR(pG->pPlayer, 0x91));
        PSet(pl->m_MotTbl[0x41], PL_ARC_PTR(pG->pPlayer, 0x92));
        PSet(pl->m_MotTbl[0x42], PL_ARC_PTR(pG->pPlayer, 0x93));
        PSet(pl->m_MotTbl[0x5F], PL_ARC_PTR(pG->pPlayer, 0x87));
        PSet(pl->m_MotTbl[0x60], PL_ARC_PTR(pG->pPlayer, 0xA0));
        PSet(pl->m_MotTbl[0x61], PL_ARC_PTR(pG->pPlayer, 0x88));
        PSet(pl->m_MotTbl[0x62], PL_ARC_PTR(pG->pPlayer, 0xA1));
        PSet(pl->m_MotTbl[0x63], PL_ARC_PTR(pG->pPlayer, 0x89));
        PSet(pl->m_MotTbl[0x64], PL_ARC_PTR(pG->pPlayer, 0xA2));
        PSet(pl->m_MotTbl[0x65], PL_ARC_PTR(pG->pPlayer, 0x8A));
        PSet(pl->m_MotTbl[0x66], PL_ARC_PTR(pG->pPlayer, 0xA3));
        PSet(pl->m_MotTbl[0x6B], PL_ARC_PTR(pG->pPlayer, 0x8B));
        PSet(pl->m_MotTbl[0x6C], PL_ARC_PTR(pG->pPlayer, 0xA4));
        PSet(pl->m_MotTbl[0x67], PL_ARC_PTR(pG->pPlayer, 0x8D));
        PSet(pl->m_MotTbl[0x68], PL_ARC_PTR(pG->pPlayer, 0xA6));
        PSet(pl->m_MotTbl[0x69], PL_ARC_PTR(pG->pPlayer, 0x8C));
        PSet(pl->m_MotTbl[0x6A], PL_ARC_PTR(pG->pPlayer, 0xA5));
    } else {
        PSet(pl->m_MotTbl[0x00], PL_ARC_PTR(pG->pPlayer, 0x6A));
        PSet(pl->m_MotTbl[0x02], PL_ARC_PTR(pG->pPlayer, 0x6B));
        PSet(pl->m_MotTbl[0x03], PL_ARC_PTR(pG->pPlayer, 0x6C));
        PSet(pl->m_MotTbl[0x06], PL_ARC_PTR(pG->pPlayer, 0x6F));
        PSet(pl->m_MotTbl[0x07], PL_ARC_PTR(pG->pPlayer, 0x70));
        PSet(pl->m_MotTbl[0x08], PL_ARC_PTR(pG->pPlayer, 0x6D));
        PSet(pl->m_MotTbl[0x09], PL_ARC_PTR(pG->pPlayer, 0x6E));
        PSet(pl->m_MotTbl[0x0B], PL_ARC_PTR(pG->pPlayer, 0x71));
        PSet(pl->m_MotTbl[0x0C], PL_ARC_PTR(pG->pPlayer, 0x72));
        PSet(pl->m_MotTbl[0x0D], PL_ARC_PTR(pG->pPlayer, 0x73));
        PSet(pl->m_MotTbl[0x0E], PL_ARC_PTR(pG->pPlayer, 0x78));
        PSet(pl->m_MotTbl[0x0F], PL_ARC_PTR(pG->pPlayer, 0x74));
        PSet(pl->m_MotTbl[0x10], PL_ARC_PTR(pG->pPlayer, 0x79));
        PSet(pl->m_MotTbl[0x3F], PL_ARC_PTR(pG->pPlayer, 0x7A));
        PSet(pl->m_MotTbl[0x40], PL_ARC_PTR(pG->pPlayer, 0x7B));
        PSet(pl->m_MotTbl[0x39], PL_ARC_PTR(pG->pPlayer, 0x7C));
        PSet(pl->m_MotTbl[0x3A], PL_ARC_PTR(pG->pPlayer, 0x7D));
        PSet(pl->m_MotTbl[0x41], PL_ARC_PTR(pG->pPlayer, 0x7E));
        PSet(pl->m_MotTbl[0x42], PL_ARC_PTR(pG->pPlayer, 0x7F));
        PSet(pl->m_MotTbl[0x5F], PL_ARC_PTR(pG->pPlayer, 0x32));
        PSet(pl->m_MotTbl[0x60], PL_ARC_PTR(pG->pPlayer, 0x33));
        PSet(pl->m_MotTbl[0x61], PL_ARC_PTR(pG->pPlayer, 0x34));
        PSet(pl->m_MotTbl[0x62], PL_ARC_PTR(pG->pPlayer, 0x35));
        PSet(pl->m_MotTbl[0x63], PL_ARC_PTR(pG->pPlayer, 0x36));
        PSet(pl->m_MotTbl[0x64], PL_ARC_PTR(pG->pPlayer, 0x37));
        PSet(pl->m_MotTbl[0x65], PL_ARC_PTR(pG->pPlayer, 0x38));
        PSet(pl->m_MotTbl[0x66], PL_ARC_PTR(pG->pPlayer, 0x39));
        PSet(pl->m_MotTbl[0x6B], PL_ARC_PTR(pG->pPlayer, 0x3A));
        PSet(pl->m_MotTbl[0x6C], PL_ARC_PTR(pG->pPlayer, 0x3B));
        PSet(pl->m_MotTbl[0x67], PL_ARC_PTR(pG->pPlayer, 0x3C));
        PSet(pl->m_MotTbl[0x68], PL_ARC_PTR(pG->pPlayer, 0x3D));
        PSet(pl->m_MotTbl[0x69], PL_ARC_PTR(pG->pPlayer, 0x3E));
        PSet(pl->m_MotTbl[0x6A], PL_ARC_PTR(pG->pPlayer, 0x3F));
    }
}

// Right hand model: 0 = empty hand (room 20E variant 0xA7/0xA8, else 0x11/5), 1 = the weapon hand
// (Body->pWepHand). A create failure HALTs.
void cPlAshley::setRightHand(int no)
{
    cModelInfo* info;
    cModelInfo* data;
    void* tpl;

    if (Body->pRight) {
        deleteModelInfo(Body->pRight);
        data = Body->pRight;
        ModInfoMgr.destroy(data);
        Body->pRight = 0;
        Body->pRightData = 0;
    }
    switch (no) {
    case 0:
    default:
        if (pG->stage_no == 2 && pG->room_no == 0xE) {
            data = (cModelInfo*) PL_ARC_PTR(pG->pPlayer, 0xA7);
            tpl = PL_ARC_PTR(pG->pPlayer, 0xA8);
        } else {
            data = (cModelInfo*) PL_ARC_PTR(pG->pPlayer, 0x11);
            tpl = PL_ARC_PTR(pG->pPlayer, 5);
        }
        break;
    case 1:
        data = (cModelInfo*) Body->pWepHand;
        tpl = PL_ARC_PTR(pG->pPlayer, 5);
        break;
    }
    if ((info = ModInfoMgr.create(data, tpl)) != 0) {
        addModel(info);
        Body->pRight = info;
        Body->pRightData = data;
    }
    if (!info) {
#line 390 "D:/Bio4/Prog/pl_ashley.cpp"
        HALT();
    }
}

// Left hand model: 0 open (0x14), 1 closed (0x15); 0x63 = restore the previous hand.
void cPlAshley::setLeftHand(u32 no)
{
    cModelInfo* info;
    void* data;

    if (Body->pLeft) {
        deleteModelInfo(Body->pLeft);
        ModInfoMgr.destroy(Body->pLeft);
        Body->pLeft = 0;
        Body->pLeftData = 0;
    }
    if (no == 0x63) {
        no = Body->oldLhandNo;
    }
    switch (no) {
    case 0:
    default:
        data = PL_ARC_PTR(pG->pPlayer, 0x14);
        break;
    case 1:
        data = PL_ARC_PTR(pG->pPlayer, 0x15);
        break;
    }
    Body->oldLhandNo = Body->nowLhandNo;
    Body->nowLhandNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pGS->pPlayer, 5));
    if (info == 0) {
        pLog->err(0, 0, "cLeon::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        Body->pLeft = info;
        Body->pLeftData = data;
    }
}

// Ashley has no face variants.
void cPlAshley::setFace(int no)
{
}

// Bust bounce: a sine offset (period 256/15 frames) on parts 0x1D / 0x1E / 0x1A that is pumped to 6
// units while she moves faster than 5 units/frame (or pad 2 X is held) and decays otherwise.
void cPlAshley::moveBust()
{
    static u8 bbx = 0;
    static f32 bul = 0.0f;
    f32 max = 6.0f;
    f32 div = 11.0f;
    Vec ofs;
    cModel* parts;
    cModel* body = getPartsPtr(0);

    if (GetDistance3(&body->world, &body->world_old2) > 5.0f) {
        bul = max;
    }
    if (Joy[1].on & JOY_X) {
        bul = max;
    } else {
        if (bul > max / div) {
            bul -= max / div;
        } else {
            bul = 0.0f;
        }
    }
    ofs.x = 0.0f;
    ofs.y = sinf((f32) bbx * (PI * 2.0f) * (1.0f / 256.0f)) * bul;
    ofs.z = 0.0f;
    parts = getPartsPtr(0x1D);
    PSVECAdd(&bustBase[0], &ofs, &parts->pos);
    parts->matUpdate();
    PSMTXConcat(parts->pParent->mat, parts->mat, parts->mat);
    parts->world.x = parts->mat[0][3];
    parts->world.y = parts->mat[1][3];
    parts->world.z = parts->mat[2][3];
    parts = getPartsPtr(0x1E);
    PSVECAdd(&bustBase[1], &ofs, &parts->pos);
    parts = getPartsPtr(0x1A);
    PSVECAdd(&bustBase[2], &ofs, &parts->pos);
    bbx += 15;
    parts->matUpdate();
    PSMTXConcat(parts->pParent->mat, parts->mat, parts->mat);
    parts->world.x = parts->mat[0][3];
    parts->world.y = parts->mat[1][3];
    parts->world.z = parts->mat[2][3];
}
