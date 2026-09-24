// Sscrn/ss_model: the sub screen's character and weapon models (D:/Bio4/Prog/ss_model.cpp, no
// range check: the real file name is unknown). Every *ModelInit builds the player model (MapMgr
// work 0, ssPlModel) from the player archive and the weapon model (work 1, ssWepModel) from the
// SS/cmn/ss_wepNN.dat data at SUB_SCREEN::x210.
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "item.h"
#include "id_sys.h"
#include "main_mem.h"
#include "model.h"
#include "motion.h"
#include "sscrn.h"
#include "ss_main.h"
#include <stdio.h>


extern "C" {
void wep00Init(int no);
void wep01Init(int no);
void wep02Init(int no);
void wep03Init(int no);
void wep04Init(int no);
void wep05Init(int no);
void wep06Init(int no);
void wep07Init(int no);
void wep08Init(int no);
void wep09Init(int no);
void wep10Init(int no);
void wep11Init(int no);
void wep12Init(int no);
void wep13Init(int no);
void wep14Init(int no);
void wep15Init(int no);
void wep16Init(int no);
void wep17Init(int no);
void wep19Init(int no, int type);
void wep28Init(int no);
void wep29Init(int no);
void wep30Init(int no, int type);
void wep33Init(int no);
void wep34Init(int no);
void wep35Init(int no);
void wep36Init(int no);
void wep37Init(int no);
void wep38Init(int no);
void wep39Init(int no);
void wep40Init(int no);
void wep41Init(int no, int type);
void wep42Init(int no, int type);
void wep43Init(int no);
void wep44Init(int no);
void wep45Init(int no, int type);
void wep47Init(int no);
}

// The player archive (pG->pPlArc) and the weapon data (SUB_SCREEN::x210): both are offset tables.
#define WEP_ARC(wk, no) SS_ARC_PTR((SsArc*) (wk)->pWepDat, no)

// Sub screen weapon data file for weapon number `no` of the current character (pG->pl_type: 0 Leon,
// 2 Ada, 3 HUNK, 4 Krauser, 5 Wesker): SS/cmn/ss_wepNN.dat, NN = the wepNN module that owns the
// weapon (grenades 0x13/0x16/0x17/0x19/0x1F/0x20 share one file per character; Leon's 0x1D uses
// wep10's, unknown numbers wep02's).
void weaponFilename(char* name, int no)
{
    switch (pG->pl_type) {
    case 0:
        switch (no) {
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0E: case 0x0F:
        case 0x10: case 0x11: case 0x21:
            sprintf(name, "SS/cmn/ss_wep%02ld.dat", no);
            break;
        case 0x1D:
            sprintf(name, "SS/cmn/ss_wep10.dat");
            break;
        case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
            sprintf(name, "SS/cmn/ss_wep19.dat");
            break;
        default:
            sprintf(name, "SS/cmn/ss_wep02.dat");
            break;
        }
        break;
    case 2:
        switch (no) {
        case 0x00:
            sprintf(name, "SS/cmn/ss_wep34.dat");
            break;
        case 0x01:
            sprintf(name, "SS/cmn/ss_wep38.dat");
            break;
        case 0x0B:
            sprintf(name, "SS/cmn/ss_wep39.dat");
            break;
        case 0x0A:
            sprintf(name, "SS/cmn/ss_wep40.dat");
            break;
        case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
            sprintf(name, "SS/cmn/ss_wep30.dat");
            break;
        }
        break;
    case 4:
        switch (no) {
        case 0x00:
            sprintf(name, "SS/cmn/ss_wep36.dat");
            break;
        case 0x1C:
            sprintf(name, "SS/cmn/ss_wep28.dat");
            break;
        case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
            sprintf(name, "SS/cmn/ss_wep42.dat");
            break;
        }
        break;
    case 3:
        switch (no) {
        case 0x00:
            sprintf(name, "SS/cmn/ss_wep35.dat");
            break;
        case 0x0B:
            sprintf(name, "SS/cmn/ss_wep29.dat");
            break;
        case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
            sprintf(name, "SS/cmn/ss_wep41.dat");
            break;
        }
        break;
    case 5:
        switch (no) {
        case 0x00:
            sprintf(name, "SS/cmn/ss_wep37.dat");
            break;
        case 0x02:
            sprintf(name, "SS/cmn/ss_wep43.dat");
            break;
        case 0x06:
            sprintf(name, "SS/cmn/ss_wep44.dat");
            break;
        case 0x0A:
            sprintf(name, "SS/cmn/ss_wep47.dat");
            break;
        case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
            sprintf(name, "SS/cmn/ss_wep45.dat");
            break;
        }
        break;
    }
}

// Every model gets the same light set (the two Vecs are the one .rodata copy of this inline).
static inline void ssModelLight(cModel* m)
{
    static const Vec light_ofs = {0.0f, 0.0f, 0.0f};
    static const Vec light_size = {1000.0f, 1000.0f, 0.0f};
    m->LightInfo.init2(0, 1, &light_ofs, &light_size, 1);
}


// Codec screen: the player's radio pose model from the player archive plus the radio/hand parts of
// ss_term.dat (sub-files 10..13).
void tel00ModelInit(cModel* m, SsArc* arc)
{
    SUB_SCREEN* wk = &SubScreenWk;

    m->modelInit(PL_ARC(4), PL_ARC(5));
    m->addModel(ssModInfoMgr.create(PL_ARC(10), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(SS_ARC_PTR(wk->pTermDat, 10), SS_ARC_PTR(wk->pTermDat, 11)));
    m->addModel(ssModInfoMgr.create(SS_ARC_PTR(wk->pTermDat, 12), SS_ARC_PTR(wk->pTermDat, 13)));
    m->addModel(ssModInfoMgr.create(PL_ARC(9), PL_ARC(7)));
    ssModelLight(m);
}

// Codec screen partner from SS/cmn/ss_ocNNN.dat (sub-files 5..11): base body + four parts, and the
// sixth part (11) for partnerType 0/2 (Hunnigan); type 1 leaves it out.
void hunniganModelInit(cModel* m, void* data, u32 type)
{
    SsArc* d = (SsArc*) data;

    m->modelInit(SS_ARC_PTR(d, 6), SS_ARC_PTR(d, 5));
    m->addModel(ssModInfoMgr.create(SS_ARC_PTR(d, 7), SS_ARC_PTR(d, 5)));
    m->addModel(ssModInfoMgr.create(SS_ARC_PTR(d, 8), SS_ARC_PTR(d, 5)));
    m->addModel(ssModInfoMgr.create(SS_ARC_PTR(d, 9), SS_ARC_PTR(d, 5)));
    m->addModel(ssModInfoMgr.create(SS_ARC_PTR(d, 10), SS_ARC_PTR(d, 5)));
    switch (type) {
    case 0:
    case 2:
        m->addModel(ssModInfoMgr.create(SS_ARC_PTR(d, 11), SS_ARC_PTR(d, 5)));
        break;
    case 1:
        break;
    }
    ssModelLight(m);
}

// Builds the sub screen's character (MapMgr work 0) and weapon (work 1) models for the equipped
// weapon (ItemMgr.weaponId() -> number / type) by pl_type; called at screen entry and by
// weaponChangeTask after a re-equip.
void playerModelInit()
{
    cItemMgr* im = &ItemMgr;
    u16 no = WeaponId2WeaponNo(im->weaponId());
    u16 type = WeaponId2WeaponType(im->weaponId());

    ssPlModel = MapMgr.getWork(0);
    ssWepModel = MapMgr.getWork(1);
    switch (pG->pl_type) {
    case 0:
        leonModelInit(no, type);
        break;
    case 1:
        ashleyModelInit();
        break;
    case 2:
        adaModelInit(no, type);
        break;
    case 4:
        klauserModelInit(no, type);
        break;
    case 3:
        hunkModelInit(no, type);
        break;
    case 5:
        weskerModelInit(no, type);
        break;
    }
}

// Place the character model (its static position / rotation / scale follow each function). The
// scale static is a one-element array read once into a local after the pos/rot word copies: the
// in-struct load stays below the `m->rot` stores (a fixed scalar would float above them) and its
// `lis` goes early into a callee-saved register, as in the target.
#define SS_MODEL_PLACE(m, p, r, s) \
    (m)->pos = p;                  \
    (m)->ang = r;                  \
    {                              \
        f32 sc_ = (s)[0];          \
        (m)->scale.z = sc_;        \
        (m)->scale.y = sc_;        \
        (m)->scale.x = sc_;        \
    }

// Bullets left in the equipped weapon decide whether the magazine model (ssWepModel) is shown.
static inline void ssWepMagazine(cModel* wep, int no, int type)
{
    if (ItemMgr.bulletNum(WeaponNo2WeaponId(no, type)) != 0) {
        cModel* one = (cModel*) 1;
        wep->be_flag |= 2;
        ssWepModel2 = one;
    } else {
        wep->be_flag &= ~2;
        ssWepModel2 = 0;
    }
}

// Ashley's sub screen model: body + six costume parts from her archive, placed at (850, -1300),
// idle motion from ss_cmmn.dat 22; no weapon.
void ashleyModelInit()
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    static Vec ashley_pos = {850.0f, -1300.0f, 0.0f};
    static Vec ashley_rot = {0.0f, -0.5f, 0.0f};
    static f32 ashley_scale[1] = {1.0f};

    m->modelInit(PL_ARC(4), PL_ARC(5));
    m->addModel(ssModInfoMgr.create(PL_ARC(7), PL_ARC(9)));
    m->addModel(ssModInfoMgr.create(PL_ARC(6), PL_ARC(11)));
    m->addModel(ssModInfoMgr.create(PL_ARC(8), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(10), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(17), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(5)));
    ssModelLight(m);
    SS_MODEL_PLACE(m, ashley_pos, ashley_rot, ashley_scale);
    MotionSetCore(m, &((cMotModel*) m)->Motion, SS_ARC_PTR(wk->pCmmn, 22), 0, 0, 4, 0);
    ssPlMotion = (cModel*) 1;
}

// Ada's sub screen model with her weapon `no`: 0 hands (wep34), 1 handgun (wep38), 0xB machine gun
// (wep39), 0xA rifle (wep40), grenades (wep30, with the ammo check); ssWepModel2 marks a weapon
// model to fade/animate.
void adaModelInit(u16 no, u16 type)
{
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    static Vec ada_pos = {850.0f, -1300.0f, 0.0f};
    static Vec ada_rot = {0.0f, -0.5f, 0.0f};
    static f32 ada_scale[1] = {1.0f};

    m->modelInit(PL_ARC(4), PL_ARC(5));
    m->addModel(ssModInfoMgr.create(PL_ARC(6), PL_ARC(7)));
    m->addModel(ssModInfoMgr.create(PL_ARC(8), PL_ARC(7)));
    m->addModel(ssModInfoMgr.create(PL_ARC(9), PL_ARC(10)));
    ssModelLight(m);
    SS_MODEL_PLACE(m, ada_pos, ada_rot, ada_scale);
    MotionClear(wep, 0);
    switch (no) {
    case 0x00:
        wep34Init(0);
        break;
    case 0x01:
        wep38Init(1);
        break;
    case 0x0B:
        wep39Init(0xB);
        break;
    case 0x0A:
        wep40Init(0xA);
        break;
    case 0x13:
        wep30Init(0x13, type);
        break;
    case 0x16:
        wep30Init(0x16, type);
        break;
    case 0x17:
        wep30Init(0x17, type);
        break;
    case 0x19:
        wep30Init(0x19, type);
        break;
    case 0x1F:
        wep30Init(0x1F, type);
        break;
    case 0x20:
        wep30Init(0x20, type);
        break;
    default:
        wep34Init(no);
        break;
    }
    ssPlMotion = (cModel*) 1;
    ssWepModel2 = (cModel*) 1;
    switch (no) {
    case 0x00:
        ssWepModel2 = 0;
        break;
    case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
        ssWepMagazine(wep, no, type);
        break;
    }
}

// Ada: the weapon-less pose (wep34: knife?) hands and the weapon data motion.
void wep34Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(PL_ARC(17), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(18), PL_ARC(5)));
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 4), 0, 0, 4, 0);
    wep->be_flag &= ~2;
}

// Weapon held in the right hand (parts 10): the weapon model hangs off the hand with an offset.
#define SS_WEP_HANG(m, wep, parts, px, py, pz, s)   \
    (wep)->pList->pParent = (m)->getPartsPtr(parts); \
    (wep)->pList->pos.x = px;                        \
    (wep)->pList->pos.y = py;                        \
    (wep)->pList->pos.z = pz;                        \
    (wep)->scale.z = s;                               \
    (wep)->scale.y = s;                               \
    (wep)->scale.x = s

// Ada's handgun (ss_wep38.dat): hand parts, weapon hung off the right hand with an offset, pose
// motion 7.
void wep38Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(18), PL_ARC(5)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    SS_WEP_HANG(m, wep, 10, 22.0f, 0.0f, -5.0f, 1.0f);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// Ada's machine gun (ss_wep39.dat): as wep38 with its own hand offset.
void wep39Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(19), PL_ARC(5)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    SS_WEP_HANG(m, wep, 10, 35.0f, -25.0f, 4.0f, 1.0f);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// Ada's semi-auto rifle (ss_wep40.dat): weapon parented to the right hand, pose motion 7.
void wep40Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(5)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    wep->pList->pParent = m->getPartsPtr(10);
    wep->scale.z = 1.0f;
    wep->scale.y = 1.0f;
    wep->scale.x = 1.0f;
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}


// Ada's grenades (ss_wep30.dat): grenade model from the player archive (0x13 hand / 0x16 incendiary
// / 0x17 flash share a mesh, 0x19/0x1F/0x20 the eggs at half scale) held in the right hand; the
// empty-hand pose (5) when no ammo, else the grenade (6) or egg (7 + 8) poses.
void wep30Init(int no, int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    void* bin;
    void* tpl;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 4), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(18), PL_ARC(5)));
    switch (no) {
    case 0x13:
    default:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6B);
        break;
    case 0x16:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6D);
        break;
    case 0x17:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6F);
        break;
    case 0x19:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7E);
        break;
    case 0x1F:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7F);
        break;
    case 0x20:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x80);
        break;
    }
    wep->modelInit(bin, tpl);
    wep->pList->pParent = m->getPartsPtr(10);
    wep->pList->pos.x = -70.0f;
    wep->pList->pos.y = -6.0f;
    wep->pList->pos.z = 0.0f;
    wep->pList->ang.x = 1.5707964f;
    wep->pList->ang.y = 0.0f;
    wep->pList->ang.z = 0.0f;
    switch (no) {
    case 0x13:
    case 0x16:
    case 0x17:
        break;
    case 0x19:
    case 0x1F:
    case 0x20: {
        cParts* p = wep->pList;
        p->scale.z = 0.5f;
        p->scale.y = 0.5f;
        p->scale.x = 0.5f;
        break;
    }
    }
    ssModelLight(wep);
    if (ItemMgr.bulletNum(WeaponNo2WeaponId(no, type)) == 0) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 5), 0, 0, 4, 0);
        wep->be_flag &= ~2;
    } else {
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 6), 0, 0, 4, 0);
            break;
        case 0x19:
        case 0x1F:
        case 0x20:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
            MotionSetCore(wep, &((cMotModel*) wep)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
            break;
        }
        wep->be_flag |= 2;
    }
}

// Krauser's sub screen model with weapon `no`: 0 hands (wep36), 0x1C bow (wep28), grenades (wep42).
void klauserModelInit(u16 no, u16 type)
{
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    static Vec klauser_pos = {850.0f, -1300.0f, 0.0f};
    static Vec klauser_rot = {0.0f, -0.5f, 0.0f};
    static f32 klauser_scale[1] = {1.0f};

    m->modelInit(PL_ARC(4), PL_ARC(5));
    m->addModel(ssModInfoMgr.create(PL_ARC(6), PL_ARC(7)));
    m->addModel(ssModInfoMgr.create(PL_ARC(8), PL_ARC(9)));
    m->addModel(ssModInfoMgr.create(PL_ARC(15), PL_ARC(16)));
    m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(17)));
    ssModelLight(m);
    SS_MODEL_PLACE(m, klauser_pos, klauser_rot, klauser_scale);
    MotionClear(wep, 0);
    switch (no) {
    case 0x00:
        wep36Init(0);
        break;
    case 0x1C:
        wep28Init(0x1C);
        break;
    case 0x13:
        wep42Init(0x13, type);
        break;
    case 0x16:
        wep42Init(0x16, type);
        break;
    case 0x17:
        wep42Init(0x17, type);
        break;
    case 0x19:
        wep42Init(0x19, type);
        break;
    case 0x1F:
        wep42Init(0x1F, type);
        break;
    case 0x20:
        wep42Init(0x20, type);
        break;
    default:
        wep36Init(no);
        break;
    }
    ssPlMotion = (cModel*) 1;
    ssWepModel2 = (cModel*) 1;
    switch (no) {
    case 0x00:
        ssWepModel2 = 0;
        break;
    case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
        ssWepMagazine(wep, no, type);
        break;
    }
}

// Krauser unarmed (ss_wep36.dat): hand parts and the idle pose; no weapon model.
void wep36Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(PL_ARC(14), PL_ARC(9)));
    m->addModel(ssModInfoMgr.create(PL_ARC(18), PL_ARC(17)));
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 4), 0, 0, 4, 0);
    wep->be_flag &= ~2;
}

// Krauser's bow (ss_wep28.dat): bow on parts 16 (left hand) with its own motion (8), pose 7.
void wep28Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 4), WEP_ARC(wk, 5));
    wep->pList->pParent = m->getPartsPtr(16);
    wep->scale.z = 1.0f;
    wep->scale.y = 1.0f;
    wep->scale.x = 1.0f;
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
    MotionSetCore(wep, &((cMotModel*) wep)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
}

// Krauser's grenades (ss_wep42.dat): like wep30 with his hand parts.
void wep42Init(int no, int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    void* bin;
    void* tpl;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 4), PL_ARC(17)));
    switch (no) {
    case 0x13:
    default:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6B);
        break;
    case 0x16:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6D);
        break;
    case 0x17:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6F);
        break;
    case 0x19:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7E);
        break;
    case 0x1F:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7F);
        break;
    case 0x20:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x80);
        break;
    }
    wep->modelInit(bin, tpl);
    wep->pList->pParent = m->getPartsPtr(10);
    wep->pList->pos.x = -95.0f;
    wep->pList->pos.y = -30.0f;
    wep->pList->pos.z = -5.0f;
    wep->pList->ang.x = 1.5707964f;
    wep->pList->ang.y = 0.0f;
    wep->pList->ang.z = 0.0f;
    switch (no) {
    case 0x13:
    case 0x16:
    case 0x17:
        break;
    case 0x19:
    case 0x1F:
    case 0x20: {
        cParts* p = wep->pList;
        p->scale.z = 0.5f;
        p->scale.y = 0.5f;
        p->scale.x = 0.5f;
        break;
    }
    }
    ssModelLight(wep);
    if (ItemMgr.bulletNum(WeaponNo2WeaponId(no, type)) == 0) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 5), 0, 0, 4, 0);
        wep->be_flag &= ~2;
    } else {
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 6), 0, 0, 4, 0);
            break;
        case 0x19:
        case 0x1F:
        case 0x20:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
            break;
        }
        wep->be_flag |= 2;
    }
}

// HUNK's sub screen model with weapon `no`: 0 hands (wep35), 0xB machine gun (wep29), grenades
// (wep41).
void hunkModelInit(u16 no, u16 type)
{
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    static Vec hunk_pos = {850.0f, -1300.0f, 0.0f};
    static Vec hunk_rot = {0.0f, -0.5f, 0.0f};
    static f32 hunk_scale[1] = {1.0f};

    m->modelInit(PL_ARC(4), PL_ARC(5));
    m->addModel(ssModInfoMgr.create(PL_ARC(6), PL_ARC(7)));
    m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(17)));
    ssModelLight(m);
    SS_MODEL_PLACE(m, hunk_pos, hunk_rot, hunk_scale);
    switch (no) {
    case 0x00:
        wep35Init(0);
        break;
    case 0x0B:
        wep29Init(0xB);
        break;
    case 0x13:
        wep41Init(0x13, type);
        break;
    case 0x16:
        wep41Init(0x16, type);
        break;
    case 0x17:
        wep41Init(0x17, type);
        break;
    case 0x19:
        wep41Init(0x19, type);
        break;
    case 0x1F:
        wep41Init(0x1F, type);
        break;
    case 0x20:
        wep41Init(0x20, type);
        break;
    default:
        wep35Init(no);
        break;
    }
    ssPlMotion = (cModel*) 1;
    ssWepModel2 = (cModel*) 1;
    switch (no) {
    case 0x00:
        ssWepModel2 = 0;
        break;
    case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
        ssWepMagazine(wep, no, type);
        break;
    }
}

// HUNK unarmed (ss_wep35.dat): hand parts and the idle pose; no weapon model.
void wep35Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(PL_ARC(18), PL_ARC(17)));
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 4), 0, 0, 4, 0);
    wep->be_flag &= ~2;
}

// HUNK's machine gun (ss_wep29.dat): weapon on the right hand, pose 7.
void wep29Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    wep->pList->pParent = m->getPartsPtr(10);
    wep->scale.z = 1.0f;
    wep->scale.y = 1.0f;
    wep->scale.x = 1.0f;
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// HUNK's grenades (ss_wep41.dat): like wep30 with his hand parts.
void wep41Init(int no, int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    void* bin;
    void* tpl;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 4), PL_ARC(17)));
    switch (no) {
    case 0x13:
    default:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6B);
        break;
    case 0x16:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6D);
        break;
    case 0x17:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6F);
        break;
    case 0x19:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7E);
        break;
    case 0x1F:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7F);
        break;
    case 0x20:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x80);
        break;
    }
    wep->modelInit(bin, tpl);
    wep->pList->pParent = m->getPartsPtr(10);
    wep->pList->pos.x = -95.0f;
    wep->pList->pos.y = -30.0f;
    wep->pList->pos.z = -5.0f;
    wep->pList->ang.x = 1.5707964f;
    wep->pList->ang.y = 0.0f;
    wep->pList->ang.z = 0.0f;
    switch (no) {
    case 0x13:
    case 0x16:
    case 0x17:
        break;
    case 0x19:
    case 0x1F:
    case 0x20: {
        cParts* p = wep->pList;
        p->scale.z = 0.5f;
        p->scale.y = 0.5f;
        p->scale.x = 0.5f;
        break;
    }
    }
    ssModelLight(wep);
    if (ItemMgr.bulletNum(WeaponNo2WeaponId(no, type)) == 0) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 5), 0, 0, 4, 0);
        wep->be_flag &= ~2;
    } else {
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 6), 0, 0, 4, 0);
            break;
        case 0x19:
        case 0x1F:
        case 0x20:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 6), 0, 0, 4, 0);
            break;
        }
        wep->be_flag |= 2;
    }
}

// Wesker's sub screen model with weapon `no`: 0 hands (wep37), 2 handgun (wep43), 6 Matilda-class
// handgun (wep44), 0xA rifle (wep47), grenades (wep45).
void weskerModelInit(u16 no, u16 type)
{
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    static Vec wesker_pos = {850.0f, -1300.0f, 0.0f};
    static Vec wesker_rot = {0.0f, -0.5f, 0.0f};
    static f32 wesker_scale[1] = {1.0f};

    m->modelInit(PL_ARC(4), PL_ARC(5));
    m->addModel(ssModInfoMgr.create(PL_ARC(10), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(8), PL_ARC(7)));
    m->addModel(ssModInfoMgr.create(PL_ARC(6), PL_ARC(7)));
    ssModelLight(m);
    SS_MODEL_PLACE(m, wesker_pos, wesker_rot, wesker_scale);
    switch (no) {
    case 0x00:
        wep37Init(0);
        break;
    case 0x02:
        wep43Init(2);
        break;
    case 0x06:
        wep44Init(6);
        break;
    case 0x0A:
        wep47Init(0xA);
        break;
    case 0x13:
        wep45Init(0x13, type);
        break;
    case 0x16:
        wep45Init(0x16, type);
        break;
    case 0x17:
        wep45Init(0x17, type);
        break;
    case 0x19:
        wep45Init(0x19, type);
        break;
    case 0x1F:
        wep45Init(0x1F, type);
        break;
    case 0x20:
        wep45Init(0x20, type);
        break;
    default:
        wep37Init(no);
        break;
    }
    ssPlMotion = (cModel*) 1;
    ssWepModel2 = (cModel*) 1;
    switch (no) {
    case 0x00:
        ssWepModel2 = 0;
        break;
    case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
        ssWepMagazine(wep, no, type);
        break;
    }
}

// Wesker unarmed (ss_wep37.dat): hand parts and the idle pose; no weapon model.
void wep37Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(PL_ARC(18), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(17)));
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 4), 0, 0, 4, 0);
    wep->be_flag &= ~2;
}

// Wesker's handgun (ss_wep43.dat): weapon on the right hand, pose 7.
void wep43Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(22), PL_ARC(17)));
    if (no == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    }
    wep->pList->pParent = m->getPartsPtr(10);
    wep->scale.z = 1.0f;
    wep->scale.y = 1.0f;
    wep->scale.x = 1.0f;
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
}

// Wesker's second handgun (ss_wep44.dat): weapon on the right hand, pose 7.
void wep44Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(PL_ARC(18), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(24), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    wep->pList->pParent = m->getPartsPtr(10);
    wep->scale.z = 1.0f;
    wep->scale.y = 1.0f;
    wep->scale.x = 1.0f;
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
}

// Wesker's grenades (ss_wep45.dat): like wep30 with his hand parts.
void wep45Init(int no, int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    void* bin;
    void* tpl;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 4), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(17)));
    switch (no) {
    case 0x13:
    default:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6B);
        break;
    case 0x16:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6D);
        break;
    case 0x17:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6F);
        break;
    case 0x19:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7E);
        break;
    case 0x1F:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7F);
        break;
    case 0x20:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x80);
        break;
    }
    wep->modelInit(bin, tpl);
    wep->pList->pParent = m->getPartsPtr(10);
    wep->pList->pos.x = -95.0f;
    wep->pList->pos.y = -30.0f;
    wep->pList->pos.z = -5.0f;
    wep->pList->ang.x = 1.5707964f;
    wep->pList->ang.y = 0.0f;
    wep->pList->ang.z = 0.0f;
    switch (no) {
    case 0x13:
    case 0x16:
    case 0x17:
        break;
    case 0x19:
    case 0x1F:
    case 0x20: {
        cParts* p = wep->pList;
        p->scale.z = 0.5f;
        p->scale.y = 0.5f;
        p->scale.x = 0.5f;
        break;
    }
    }
    ssModelLight(wep);
    if (ItemMgr.bulletNum(WeaponNo2WeaponId(no, type)) == 0) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 5), 0, 0, 4, 0);
        wep->be_flag &= ~2;
    } else {
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 6), 0, 0, 4, 0);
            break;
        case 0x19:
        case 0x1F:
        case 0x20:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
            break;
        }
        wep->be_flag |= 2;
    }
}

// Wesker's semi-auto rifle (ss_wep47.dat): weapon on the right hand, pose 7.
void wep47Init(int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(22), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    wep->pList->pParent = m->getPartsPtr(10);
    wep->scale.z = 1.0f;
    wep->scale.y = 1.0f;
    wep->scale.x = 1.0f;
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// Leon's weapons take the weapon *type* (tune / costume variant), his rifles the number too.
void leonModelInit(u16 no, u16 type)
{
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    static Vec leon_pos = {850.0f, -1370.0f, 0.0f};
    static Vec leon_rot = {0.0f, -0.3f, 0.0f};
    static f32 leon_scale[1] = {1.0f};

    m->modelInit(PL_ARC(4), PL_ARC(5));
    m->addModel(ssModInfoMgr.create(PL_ARC(10), PL_ARC(5)));
    m->addModel(ssModInfoMgr.create(PL_ARC(8), PL_ARC(7)));
    m->addModel(ssModInfoMgr.create(PL_ARC(6), PL_ARC(7)));
    m->addModel(ssModInfoMgr.create(PL_ARC(9), PL_ARC(7)));
    ssModelLight(m);
    SS_MODEL_PLACE(m, leon_pos, leon_rot, leon_scale);
    MotionClear(wep, 0);
    switch (no) {
    case 0x00:
        wep00Init(type);
        break;
    case 0x01:
        wep01Init(type);
        break;
    case 0x02:
        wep02Init(type);
        break;
    case 0x03:
        wep03Init(type);
        break;
    case 0x04:
        wep04Init(type);
        break;
    case 0x05:
        wep05Init(type);
        break;
    case 0x06:
        wep06Init(type);
        break;
    case 0x07:
        wep07Init(type);
        break;
    case 0x08:
        wep08Init(type);
        break;
    case 0x09:
        wep09Init(type);
        break;
    case 0x0A:
        wep10Init(type);
        break;
    case 0x0B:
        wep11Init(type);
        break;
    case 0x0C:
        wep12Init(type);
        break;
    case 0x0D:
        wep13Init(type);
        break;
    case 0x0E:
        wep14Init(type);
        break;
    case 0x0F:
        wep15Init(type);
        break;
    case 0x10:
        wep16Init(type);
        break;
    case 0x11:
        wep17Init(type);
        break;
    case 0x21:
        wep33Init(type);
        break;
    case 0x13:
        wep19Init(0x13, type);
        break;
    case 0x16:
        wep19Init(0x16, type);
        break;
    case 0x17:
        wep19Init(0x17, type);
        break;
    case 0x19:
        wep19Init(0x19, type);
        break;
    case 0x1F:
        wep19Init(0x1F, type);
        break;
    case 0x20:
        wep19Init(0x20, type);
        break;
    default:
        wep02Init(type);
        break;
    }
    ssPlMotion = (cModel*) 1;
    ssWepModel2 = (cModel*) 1;
    switch (no) {
    case 0x00:
        ssWepModel2 = 0;
        break;
    case 0x13: case 0x16: case 0x17: case 0x19: case 0x1F: case 0x20:
        ssWepMagazine(wep, no, type);
        break;
    }
}

// The weapon model hangs off the right hand (parts 10), unscaled.
#define SS_WEP_HAND(m, wep)                          \
    (wep)->pList->pParent = (m)->getPartsPtr(10);   \
    (wep)->scale.z = 1.0f;                           \
    (wep)->scale.y = 1.0f;                           \
    (wep)->scale.x = 1.0f

// Leon unarmed (ss_wep00.dat): hand parts and idle pose; the weapon model is hidden.
void wep00Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(PL_ARC(18), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    wep->pList->pParent = m->getPartsPtr(10);
    wep->be_flag &= ~2;
    wep->scale.z = 0.0f;
    wep->scale.y = 0.0f;
    wep->scale.x = 0.0f;
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
    wep->be_flag &= ~2;
}

// Leon's handgun (ss_wep01.dat): model 5 (type 0) or 6 (tuned), in the right hand, pose 8.
void wep01Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(24), PL_ARC(17)));
    if (type == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else if (type == 1) {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    }
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
}

// Leon's Red9 (ss_wep02.dat): model by type, right hand, pose 8.
void wep02Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(24), PL_ARC(17)));
    if (type == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else if (type == 1) {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    }
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
}

// Leon's weapon 3 (ss_wep03.dat; the wep03 REL is a copy of wep02): model by type, right hand, pose 8.
void wep03Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(24), PL_ARC(17)));
    if (type == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else if (type == 2) {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    }
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    if (type == 0) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
    } else if (type == 2) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 9), 0, 0, 4, 0);
    }
}

// Leon's Blacktail (ss_wep04.dat): model by type, right hand, pose 8.
void wep04Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(24), PL_ARC(17)));
    if (type == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else if (type == 1) {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    }
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
}

// Leon's weapon 5 (ss_wep05.dat, the objCivilian revolver): one model, right hand.
void wep05Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
}

// Leon's Matilda (ss_wep06.dat): model by type, right hand, pose 8.
void wep06Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 8), PL_ARC(17)));
    if (type == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else if (type == 1) {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    }
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 9), 0, 0, 4, 0);
}

// Leon's shotgun (ss_wep07.dat): one model, right hand.
void wep07Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(22), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 4), WEP_ARC(wk, 5));
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// Leon's Striker (ss_wep08.dat): one model, right hand.
void wep08Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(25), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 4), WEP_ARC(wk, 5));
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// Leon's rifle (ss_wep09.dat): model 5/6/7 by scope type, right hand.
void wep09Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 8), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(22), PL_ARC(17)));
    if (type == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else if (type == 1) {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    } else if (type == 2) {
        wep->modelInit(WEP_ARC(wk, 7), WEP_ARC(wk, 4));
    }
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 9), 0, 0, 4, 0);
}

// Leon's semi-auto rifle (ss_wep10.dat): model 5/6/7 by scope type, right hand.
void wep10Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 8), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(22), PL_ARC(17)));
    if (type == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else if (type == 1) {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    } else {
        wep->modelInit(WEP_ARC(wk, 7), WEP_ARC(wk, 4));
    }
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 9), 0, 0, 4, 0);
}

// Leon's TMP (ss_wep11.dat): model 5..8 by type (stock variants), pose 11 without / 12 with the
// stock.
void wep11Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 9), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(21), PL_ARC(17)));
    switch (type) {
    case 0:
    default:
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
        break;
    case 1:
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
        break;
    case 2:
        wep->modelInit(WEP_ARC(wk, 7), WEP_ARC(wk, 4));
        break;
    case 3:
        wep->modelInit(WEP_ARC(wk, 8), WEP_ARC(wk, 4));
        break;
    }
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    switch (type) {
    case 0:
    case 1:
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 11), 0, 0, 4, 0);
        break;
    case 2:
    case 3:
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 12), 0, 0, 4, 0);
        break;
    }
}

// Leon's Chicago Typewriter (ss_wep12.dat): one model, right hand.
void wep12Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(22), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// The Mine Thrower: the sight (a second model info) is rotated onto the weapon; the tuned variant
// (type 1) recolours both.
void wep13Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    cModelInfo* info;
    static Vec wep13_pos = {-136.0f, -30.72f, 118.85f};
    static Vec wep13_rot = {1.5707964f, 0.0f, -1.5707964f};

    info = ssModInfoMgr.create(WEP_ARC(wk, 4), PL_ARC(17));
    m->addModel(info);
    info = ssModInfoMgr.create(PL_ARC(24), PL_ARC(17));
    m->addModel(info);
    info = (cModelInfo*) wep->modelInit(PL_ARC(0x76), PL_ARC(0x75));
    if (type == 1) {
        info->color[0] = 0xA0;
        info->color[1] = 0xD0;
        info->color[2] = 0xE0;
        info->color[3] = 0xFF;
    }
    wep->pList->pParent = m->getPartsPtr(10);
    info = ssModInfoMgr.create(PL_ARC(0x70), PL_ARC(0x71));
    {
        f32(*mat)[4] = info->mat;
        RotMatrix(mat, &wep13_rot);
        TransMatrix(mat, &wep13_pos);
    }
    if (type == 1) {
        info->be_flag |= 0x20;
        info->color[0] = 0xFF;
        info->color[1] = 0x78;
        info->color[2] = 0x80;
        info->color[3] = 0xFF;
    } else {
        info->be_flag &= ~0x20;
    }
    wep->addModel(info);
    wep->scale.z = 1.0f;
    wep->scale.y = 1.0f;
    wep->scale.x = 1.0f;
    ssModelLight(wep);
    if (ItemMgr.bulletNum(WeaponNo2WeaponId(0xD, type)) == 0) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 6), 0, 0, 4, 0);
        wep->be_flag &= ~2;
    } else {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 5), 0, 0, 4, 0);
        wep->be_flag |= 2;
    }
}

// Leon's mine thrower (ss_wep14.dat): model by type, its parts 1 parented to the right hand at the
// origin; pose 8 / 9 by type.
void wep14Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    cParts* p;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(24), PL_ARC(17)));
    if (type == 0) {
        wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    } else {
        wep->modelInit(WEP_ARC(wk, 6), WEP_ARC(wk, 4));
    }
    wep->getPartsPtr(1)->pParent = m->getPartsPtr(10);
    p = wep->getPartsPtr(1);
    p->pos.x = 0.0f;
    p->pos.y = 0.0f;
    p->pos.z = 0.0f;
    p->ang.x = 0.0f;
    p->ang.y = 0.0f;
    p->ang.z = 0.0f;
    wep->scale.z = 1.0f;
    wep->scale.y = 1.0f;
    wep->scale.x = 1.0f;
    ssModelLight(wep);
    if (type == 0) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
    } else {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 9), 0, 0, 4, 0);
    }
}

// Leon's Broken Butterfly (ss_wep15.dat): one model, right hand.
void wep15Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 7), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
}

// Leon's knife (ss_wep16.dat): one model, right hand.
void wep16Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// Leon's Killer7 (ss_wep17.dat): one model, right hand.
void wep17Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(24), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 5), WEP_ARC(wk, 4));
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// Leon's Riot Gun (ss_wep33.dat, weapon 0x21): one model, right hand.
void wep33Init(int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 6), PL_ARC(17)));
    m->addModel(ssModInfoMgr.create(PL_ARC(22), PL_ARC(17)));
    wep->modelInit(WEP_ARC(wk, 4), WEP_ARC(wk, 5));
    SS_WEP_HAND(m, wep);
    ssModelLight(wep);
    MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
}

// Leon's grenades (ss_wep19.dat): grenade / egg model from the player archive by `no` (see wep30),
// held in the right hand; empty-hand pose when no ammo, else the grenade or egg poses.
void wep19Init(int no, int type)
{
    SUB_SCREEN* wk = &SubScreenWk;
    cModel* m = ssPlModel;
    cModel* wep = ssWepModel;
    void* bin;
    void* tpl;

    m->addModel(ssModInfoMgr.create(WEP_ARC(wk, 4), PL_ARC(17)));
    switch (no) {
    case 0x13:
    case 0x16:
    case 0x17:
        m->addModel(ssModInfoMgr.create(PL_ARC(20), PL_ARC(17)));
        break;
    case 0x19:
    case 0x1F:
    case 0x20:
        m->addModel(ssModInfoMgr.create(PL_ARC(21), PL_ARC(17)));
        break;
    }
    switch (no) {
    case 0x13:
    default:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6B);
        break;
    case 0x16:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6D);
        break;
    case 0x17:
        bin = PL_ARC(0x6A);
        tpl = PL_ARC(0x6F);
        break;
    case 0x19:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7E);
        break;
    case 0x1F:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x7F);
        break;
    case 0x20:
        bin = PL_ARC(0x7D);
        tpl = PL_ARC(0x80);
        break;
    }
    wep->modelInit(bin, tpl);
    wep->pList->pParent = m->getPartsPtr(10);
    wep->pList->pos.x = -95.0f;
    wep->pList->pos.y = -30.0f;
    wep->pList->pos.z = -5.0f;
    wep->pList->ang.x = 1.5707964f;
    wep->pList->ang.y = 0.0f;
    wep->pList->ang.z = 0.0f;
    switch (no) {
    case 0x13:
    case 0x16:
    case 0x17:
        break;
    case 0x19:
    case 0x1F:
    case 0x20: {
        cParts* p = wep->pList;
        p->scale.z = 0.5f;
        p->scale.y = 0.5f;
        p->scale.x = 0.5f;
        break;
    }
    }
    ssModelLight(wep);
    if (ItemMgr.bulletNum(WeaponNo2WeaponId(no, type)) == 0) {
        MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 8), 0, 0, 4, 0);
        wep->be_flag &= ~2;
    } else {
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 5), 0, 0, 4, 0);
            break;
        case 0x19:
        case 0x1F:
        case 0x20:
            MotionSetCore(m, &((cMotModel*) m)->Motion, WEP_ARC(wk, 6), 0, 0, 4, 0);
            MotionSetCore(wep, &((cMotModel*) wep)->Motion, WEP_ARC(wk, 7), 0, 0, 4, 0);
            break;
        }
        wep->be_flag |= 2;
    }
}

// An unreferenced zero-initialised static closes the unit's .data (the split object has a zero
// word after wep13's rotation; nothing addresses it).
static int wep_model_unused = 0;
