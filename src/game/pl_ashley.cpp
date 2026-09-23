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
#include <dolphin/os.h>
#include "esp.h"
#include "pl_mod.h"
#include "read.h"


// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.

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
    EspDataLoad((u32) PL_ARC_PTR(pG->pPlayer, 0x1A), EFF_PL00, 0);
    startUp();
    pFsdTbl = pl_fs_tbl;
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
    Body->m_pFace = info;
    Body->pHeadData = PL_ARC_PTR(pG->pPlayer, 7);
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
    setTevScaleGroup(1);
    setFace(0);
    setRightHand(0);
    setLeftHand(0);
}

// Ashley's motion table (m_MotTbl, 0x6D entries) from the player archive: a separate set for room
// 20E (the crate-carrying / cabin section), the normal set elsewhere.
void pl01weaponSet(cPlayer* pEm)
{
    int i;

    for (i = 0; i < 0x6D; i++) {
        pEm->m_MotTbl[i] = 0;
    }
    if (pG->stage_no == 2 && pG->room_no == 0xE) {
        PLA_MOT(pEm, 0x00, 0x80);
        PLA_MOT(pEm, 0x02, 0x81);
        PLA_MOT(pEm, 0x03, 0x9A);
        PLA_MOT(pEm, 0x06, 0x83);
        PLA_MOT(pEm, 0x07, 0x9C);
        PLA_MOT(pEm, 0x08, 0x82);
        PLA_MOT(pEm, 0x09, 0x9B);
        PLA_MOT(pEm, 0x0B, 0x84);
        PLA_MOT(pEm, 0x0C, 0x9D);
        PLA_MOT(pEm, 0x0D, 0x86);
        PLA_MOT(pEm, 0x0E, 0x9F);
        PLA_MOT(pEm, 0x0F, 0x85);
        PLA_MOT(pEm, 0x10, 0x9E);
        PLA_MOT(pEm, 0x3F, 0x8E);
        PLA_MOT(pEm, 0x40, 0x8F);
        PLA_MOT(pEm, 0x39, 0x90);
        PLA_MOT(pEm, 0x3A, 0x91);
        PLA_MOT(pEm, 0x41, 0x92);
        PLA_MOT(pEm, 0x42, 0x93);
        PLA_MOT(pEm, 0x5F, 0x87);
        PLA_MOT(pEm, 0x60, 0xA0);
        PLA_MOT(pEm, 0x61, 0x88);
        PLA_MOT(pEm, 0x62, 0xA1);
        PLA_MOT(pEm, 0x63, 0x89);
        PLA_MOT(pEm, 0x64, 0xA2);
        PLA_MOT(pEm, 0x65, 0x8A);
        PLA_MOT(pEm, 0x66, 0xA3);
        PLA_MOT(pEm, 0x6B, 0x8B);
        PLA_MOT(pEm, 0x6C, 0xA4);
        PLA_MOT(pEm, 0x67, 0x8D);
        PLA_MOT(pEm, 0x68, 0xA6);
        PLA_MOT(pEm, 0x69, 0x8C);
        PLA_MOT(pEm, 0x6A, 0xA5);
    } else {
        PLA_MOT(pEm, 0x00, 0x6A);
        PLA_MOT(pEm, 0x02, 0x6B);
        PLA_MOT(pEm, 0x03, 0x6C);
        PLA_MOT(pEm, 0x06, 0x6F);
        PLA_MOT(pEm, 0x07, 0x70);
        PLA_MOT(pEm, 0x08, 0x6D);
        PLA_MOT(pEm, 0x09, 0x6E);
        PLA_MOT(pEm, 0x0B, 0x71);
        PLA_MOT(pEm, 0x0C, 0x72);
        PLA_MOT(pEm, 0x0D, 0x73);
        PLA_MOT(pEm, 0x0E, 0x78);
        PLA_MOT(pEm, 0x0F, 0x74);
        PLA_MOT(pEm, 0x10, 0x79);
        PLA_MOT(pEm, 0x3F, 0x7A);
        PLA_MOT(pEm, 0x40, 0x7B);
        PLA_MOT(pEm, 0x39, 0x7C);
        PLA_MOT(pEm, 0x3A, 0x7D);
        PLA_MOT(pEm, 0x41, 0x7E);
        PLA_MOT(pEm, 0x42, 0x7F);
        PLA_MOT(pEm, 0x5F, 0x32);
        PLA_MOT(pEm, 0x60, 0x33);
        PLA_MOT(pEm, 0x61, 0x34);
        PLA_MOT(pEm, 0x62, 0x35);
        PLA_MOT(pEm, 0x63, 0x36);
        PLA_MOT(pEm, 0x64, 0x37);
        PLA_MOT(pEm, 0x65, 0x38);
        PLA_MOT(pEm, 0x66, 0x39);
        PLA_MOT(pEm, 0x6B, 0x3A);
        PLA_MOT(pEm, 0x6C, 0x3B);
        PLA_MOT(pEm, 0x67, 0x3C);
        PLA_MOT(pEm, 0x68, 0x3D);
        PLA_MOT(pEm, 0x69, 0x3E);
        PLA_MOT(pEm, 0x6A, 0x3F);
    }
}

// Right hand model: 0 = empty hand (room 20E variant 0xA7/0xA8, else 0x11/5), 1 = the weapon hand
// (Body->pWepHand). A create failure HALTs.
void cPlAshley::setRightHand(int type)
{
    cModelInfo* info;
    cModelInfo* data;
    void* tpl;

    if (Body->m_pHandR) {
        deleteModelInfo(Body->m_pHandR);
        data = Body->m_pHandR;
        ModInfoMgr.destroy(data);
        Body->m_pHandR = 0;
        Body->pRightData = 0;
    }
    switch (type) {
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
        Body->m_pHandR = info;
        Body->pRightData = data;
    }
    if (!info) {
#line 390 "D:/Bio4/Prog/pl_ashley.cpp"
        HALT();
    }
}

// Left hand model: 0 open (0x14), 1 closed (0x15); 0x63 = restore the previous hand.
void cPlAshley::setLeftHand(u32 type)
{
    cModelInfo* info;
    void* data;

    if (Body->m_pHandL) {
        deleteModelInfo(Body->m_pHandL);
        ModInfoMgr.destroy(Body->m_pHandL);
        Body->m_pHandL = 0;
        Body->pLeftData = 0;
    }
    if (type == 0x63) {
        type = Body->oldLhandNo;
    }
    switch (type) {
    case 0:
    default:
        data = PL_ARC_PTR(pG->pPlayer, 0x14);
        break;
    case 1:
        data = PL_ARC_PTR(pG->pPlayer, 0x15);
        break;
    }
    Body->oldLhandNo = Body->nowLhandNo;
    Body->nowLhandNo = type;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pG->pPlayer, 5));
    if (info == 0) {
        pLog->err(0, 0, "cLeon::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        Body->m_pHandL = info;
        Body->pLeftData = data;
    }
}

// Ashley has no face variants.
void cPlAshley::setFace(int type)
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
    cParts* parts;
    cParts* body = getPartsPtr(0);

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
