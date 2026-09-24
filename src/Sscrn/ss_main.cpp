// Sscrn/ss_main: sub screen DLL entry (D:/Bio4/Prog/ss_main.cpp): _prolog/_epilog/_unresolved, the
// SubScreenTask that runs the widget chains, the common id/model/light helpers, the exit and item
// examine widgets and the DLL's own model managers.
#include "types.h"
#include "global.h"
#include "light.h"
#include "atari.h"
#include "map_obj.h"
#include "widget.h"
#include "dbg_button.h"
#include "item.h"
#include "cockpit.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main.h"
#include "snd.h"
#include "db_log.h"
#include "view.h"
#include "trans.h"
#include "model.h"
#include "camera.h"
#include "scheduler.h"
#include "gx.h"
#include "motion.h"
#include "esp.h"
#include "sscrn.h"
#include "espgen.h"

// `inline`, defined BEFORE ss_main.h: a deferred inline whose address SubScreenTask takes is output
// at the end of the file (after __static_initialization_and_destruction_0), in the order the deferred
// functions were queued. The original queued the synthesized widget destructors when they were
// synthesized (end of file); ours queues them at the class definition, so the definition must
// precede the widget classes to come out first (0xD5B4 before ~Widget and the three destructors).
extern "C" inline void LightSetModel2(cModel* m)
{
    LightMgr.setModel2(m);
}

#include "ss_main.h"
#include <stdio.h>
#include <dolphin/os.h>


#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// _ctors/_dtors: the linker script's labels on the .ctors/.dtors lists (null terminated)
extern void (*_ctors[])(void);
extern void (*_dtors[])(void);

// The widget classes (SsExitInit / SsExitMain / SsItemExamine) are declared in ss_main.h.

extern "C" {
void SubScreenTask();
void clearZbuffer();
void sscrnCameraInit(SUB_SCREEN* wk, CAMERA* cam);
int sscrnKey2Game(SUB_SCREEN* wk);
void dispScrollBar(u32 top, u32 n, u32 num, IdUnit* bar, IdUnit* up, IdUnit* down);
void generalModelAlloc(SUB_SCREEN* wk);
int sscrnMainMenu(SUB_SCREEN* wk);
void idMainMenu(SUB_SCREEN* wk, int sw);
void idMainMenuFade(SUB_SCREEN* wk, int sw);
void sscrnModelFree(SUB_SCREEN* wk);
void weaponChangeRequest(u16 no, u16 type);
int weaponChangeReadCheck();
int weaponChangeMoveCheck();
}

static void weaponChangeTask();
static void sscrnModelTrans(cModel* m);

// REL entry: runs the module's static constructors (.ctors), then the sub screen main loop
// (SubScreenTask) in the calling task; the DOL links the module from SubScreenCall.
extern "C" void _prolog()
{
    void (**p)(void);

    for (p = _ctors; *p; p++) {
        (*p)();
    }
    OSReport("prolog...\n");
    SubScreenTask();
}

// REL exit: runs the static destructors (.dtors) before the DOL unlinks the module.
extern "C" void _epilog()
{
    void (**p)(void);

    for (p = _dtors; *p; p++) {
        (*p)();
    }
    OSReport("epilog...\n");
}

// Trap for calls through unresolved module imports: reports and HALTs (line 139 of the original).
extern "C" void _unresolved()
{
    OSReport("unresolved...\n");
#line 139 "D:/Bio4/Prog/ss_main.cpp"
    HALT();
}

// Sets the sub screen's fixed camera: eye at (0, 0, 5000) looking at the origin, y up, 20 degree fov,
// 4:3 aspect; rebuilds its projection and view matrices. Every screen's Init widget calls it.
void sscrnCameraInit(SUB_SCREEN* wk, CAMERA* cam)
{
    const f32 zero = 0.0f;  // pool order: 0.0 first

    // Store order (sched1 weight rule): up.z is the last zero store in the source, so it carries the
    // zero register's death and is issued before the other zero stores; fovy is written last and
    // its late pool load lets up.z slip in front of it.
    cam->param.pos.z = 5000.0f;
    cam->Up.y = 1.0f;
    cam->param.at.x = 0.0f;
    cam->param.at.y = 0.0f;
    cam->param.at.z = 0.0f;
    cam->param.pos.x = 0.0f;
    cam->param.pos.y = 0.0f;
    cam->Up.x = 0.0f;
    cam->Up.z = 0.0f;
    cam->param.fovy = 20.0f;
    CameraSetOrientationUp(cam);
    C_MTXPerspective(cam->ProjMat, cam->param.fovy, 1.3333334f, ZNEAR, ZFAR);
    cam->Distance = PSVECDistance(&cam->param.pos, &cam->param.at);
    C_MTXLookAt(cam->v_mat, &cam->param.pos, &cam->Up, &cam->param.at);
}

// 1 when the player asks to return to the game: Y (Key bit 20) on a type 1 (inventory) screen, Y or B
// otherwise. Main widgets test it before their own input.
int sscrnKey2Game(SUB_SCREEN* wk)
{
    if (wk->open_flag == 1) {
        if (Key.trg & 0x100000) {
            return 1;
        }
    } else {
        if (Key.trg & 0x40100000) {
            return 1;
        }
    }
    return 0;
}

// Sizes and places a list scroll bar: `bar` spans the fraction n/num of the height between the `up`
// and `down` arrow units, offset by top/num from the top; hidden (be_flag bit 3 off) when the list fits.
void dispScrollBar(u32 top, u32 n, u32 num, IdUnit* bar, IdUnit* up, IdUnit* down)
{
    if (n < num) {
        f32 h = up->pos0.y - down->pos0.y;
        f32 rate = (f32) n / (f32) num;
        bar->size_H = rate * h;
        rate = (f32) top / (f32) num;
        bar->pos0.y = up->pos.y - rate * (up->pos0.y - down->pos0.y);
        bar->be_flag |= 8;
    } else {
        bar->be_flag &= ~8;
    }
}

// Switches cModel to the DLL's own parts/model-info managers (0x100 parts, 0xA0 infos) and creates
// the 0xA0 MapMgr model works the screens use; attr_flag bit 0 records it for sscrnModelFree.
void generalModelAlloc(SUB_SCREEN* wk)
{
    int i;

    wk->attr_flag |= 1;
    ssModInfoMgr.roomInit();
    ssModInfoMgr.arrayAlloc(0xA0);
    ssPartsMgr.roomInit();
    ssPartsMgr.arrayAlloc(0x100);
    cModel::mm = &ssModInfoMgr;
    cModel::pm = &ssPartsMgr;
    MapMgr.roomInit();
    MapMgr.arrayAlloc(0xA0);
    for (i = 0; i < 0xA0; i++) {
        MapMgr.create(0, i);
    }
}

// Widget chain of the sub screen: every screen's Init/Main pair with its links (link 0 = the next
// widget of the screen's own chain; the main widgets link to the other screens' Init widgets and to
// the exit widget). Runs the current widget every frame until the screen closes.

void SubScreenTask()
{
    SUB_SCREEN* wk = &SubScreenWk;
    SsTermMain* termMain = 0;
    SsShopInit* shopInit = 0;
    SsShopMain* shopMain = 0;
    SsTermInit* termInit = 0;
    SsExitInit* exitInit = new SsExitInit;
    SsExitMain* exitMain;
    SsPzzlInit* pzzlInit;
    SsPzzlMain* pzzlMain;
    SsItemInit* itemInit;
    SsItemMain* itemMain;
    SsMapInit* mapInit;
    SsMapMain* mapMain;
    SsFileInit* fileInit;
    SsFileMain* fileMain;
    SsCapInit* capInit;
    SsCapMain* capMain;
    Widget<SUB_SCREEN>* cur;

    exitMain = new SsExitMain;
    exitInit->connect(0, exitMain);
    cur = 0;
    if (wk->open_flag & 0x10) {
        shopInit = new SsShopInit;
        shopMain = new SsShopMain;
        shopInit->connect(0, shopMain);
        shopMain->connect(0, exitInit);
        cur = shopInit;
        cur->init(wk);
    } else if (wk->open_flag & 0x20) {
        termInit = new SsTermInit;
        termMain = new SsTermMain;
        termInit->connect(0, termMain);
        termMain->connect(0, exitInit);
        cur = termInit;
        cur->init(wk);
    } else {
        pzzlInit = new SsPzzlInit;
        pzzlMain = new SsPzzlMain;
        itemInit = new SsItemInit;
        itemMain = new SsItemMain;
        mapInit = new SsMapInit;
        mapMain = new SsMapMain;
        fileInit = new SsFileInit;
        fileMain = new SsFileMain;
        capInit = new SsCapInit;
        capMain = new SsCapMain;
        pzzlInit->connect(0, pzzlMain);
        pzzlMain->connect(0, itemInit);
        pzzlMain->connect(2, mapInit);
        pzzlMain->connect(3, fileInit);
        pzzlMain->connect(4, exitInit);
        itemInit->connect(0, itemMain);
        itemMain->connect(0, pzzlInit);
        itemMain->connect(2, mapInit);
        itemMain->connect(3, fileInit);
        itemMain->connect(5, exitInit);
        itemMain->connect(4, capInit);
        mapInit->connect(0, mapMain);
        mapMain->connect(0, pzzlInit);
        mapMain->connect(1, itemInit);
        mapMain->connect(3, fileInit);
        mapMain->connect(4, exitInit);
        fileInit->connect(0, fileMain);
        fileMain->connect(0, pzzlInit);
        fileMain->connect(1, itemInit);
        fileMain->connect(3, mapInit);
        fileMain->connect(4, exitInit);
        capInit->connect(0, capMain);
        capMain->connect(0, itemInit);
        capMain->connect(1, exitInit);
        wk->pWepDat = (u8*) wk->pBuf + 0x2E5E00;
        if (pG->pl_type != 1) {
            char name[0x40];
            int req;
            weaponFilename(name, WeaponId2WeaponNo(ItemMgr.m_wep_id));
#line 412 "D:/Bio4/Prog/ss_main.cpp"
            req = DVD_READ_N(name, wk->pWepDat, 0, 0, 0, 0x11);
            Dvd.ReadCheck(req, 0, 0, 0);
        }
        generalModelAlloc(wk);
        playerModelInit();
        if (wk->open_flag & 2) {
            cur = mapInit;
            cur->init(wk);
        } else if (wk->open_flag & 4) {
            wk->pPzzlDat = wk->pSwitchDat;
            cur = pzzlMain;
            cur->init(wk);
        } else if (wk->open_flag & 0x80) {
            cur = itemInit;
            cur->init(wk);
        } else if (wk->open_flag & 0x40) {
            cur = fileInit;
            cur->init(wk);
        } else {
            wk->pPzzlDat = wk->pSwitchDat;
            cur = pzzlMain;
            cur->init(wk);
        }
    }
    wk->wep_rno = 0;
    wk->wep_idx = 0;
    TaskExec(2, (TaskFunc) weaponChangeTask, 0);
    while (wk->Loop) {
        // `&MapMgr` for the two model loops at the top of the body: loop.c hoists it (lis + addi
        // into r23) as one two-use invariant; declared inside the `if (wk->x44 == 0)` block below,
        // the set is `maybe_never` and used in two blocks, so it is not movable.
        cMapMgr* mgr = &MapMgr;
        if (exitInit != cur && exitMain != cur && shopInit != cur && shopMain != cur && termInit != cur &&
            termMain != cur && weaponChangeMoveCheck()) {
            f32 rate = (f32) (s16) wk->alpha_cnt / 10.0f;
            if (wk->alpha_flag == 0) {
                wk->alpha_cnt--;
                if ((s16) wk->alpha_cnt < 0) {
                    wk->alpha_cnt = 0;
                }
            } else {
                wk->alpha_cnt++;
                if ((s16) wk->alpha_cnt > 10) {
                    wk->alpha_cnt = 10;
                }
            }
            if (ssPlModel) {
                ssPlModel->invisible_factor2 = 1.0f - rate;
            }
            if (ssWepModel) {
                ssWepModel->invisible_factor2 = 1.0f - rate;
            }
            if (ssPlMotion) {
                MotionMove(ssPlModel, 0);
            }
            // Dead test (never-read store): its `high pG` is set in this block and survives as the
            // register of the 0x19/0x1F/0x20 arm (cse1 canon_reg: the arm's own high dies inside the
            // extended block, so the earlier one stays canonical); the 0x1C arm is reached in a fresh
            // cse block, keeps its high, and gcse PRE makes it redundant with the reaching register
            // inserted at this block's end -> the target's two `lis pG@ha` (r29/r30) before the
            // ssWepModel2 test, and no combine_movables hoist (both highs are used in other blocks
            // inside the maybe_never region). The store is trivially dead, so jump folds the branch
            // before gcse and sched2 sees one block.
            {
                int dmy;
                if (pG->pl_type == 7) {
                    dmy = 0;
                }
            }
            if (ssWepModel2 && ssWepModel) {
                switch (WeaponId2WeaponNo(ItemMgr.m_wep_id)) {
                case 0x19:
                case 0x1F:
                case 0x20:
                    if (pG->pl_type == 0) {
                        MotionMove(ssWepModel, 0);
                    } else {
                        ssWepModel->matUpdate();
                    }
                    break;
                case 0x1C:
                    if (pG->pl_type == 4) {
                        MotionMove(ssWepModel, 0);
                    } else {
                        ssWepModel->matUpdate();
                    }
                    break;
                default:
                    ssWepModel->matUpdate();
                    break;
                }
            }
        }
        cur->move(wk);
        cur = cur->cur;
        SscrnDebugMenu(wk);
        LightMgr.move();
        if (IdSub.setCk(IDC_SSCRN_PESETA)) {
            int d[8];
            int v = pG->peseta;
            int i;
            for (i = 0; i < 8; i++) {
                d[i] = v % 10;
                v /= 10;
            }
            for (i = 0; i < 8; i++) {
                IdUnit* u;
                u = IdSub.unitPtr(i + 1, IDC_SSCRN_PESETA);
                u->be_flag |= 8;
                u->tex_flag |= 2;
                u->texNo = d[i];
            }
            for (i = 7; i > 0 && d[i] == 0; i--) {
                IdSub.unitPtr(i + 1, IDC_SSCRN_PESETA)->be_flag &= ~8;
            }
        }
        Cckpt.move();
        IdSub.move();
        IdNum.move();
        CameraMove();
        EffClearToolState();
        EspgenMove();
        EspMove();
        EspGenLoopMove();
        IdSub.trans();
        IdNum.trans();
        if (wk->wait_cnt == 0) {
            cModel* m;
            void (*func)(cModel*);
            // `m->next` read before the call (`lwz r30, 4(r30)` above the `blrl`).
            func = sscrnModelTrans;
            m = mgr->getActiveWork();
            while (m) {
                cModel* p = m;
                m = (cModel*) m->getNext();
                func(p);
            }
            func = LightSetModel2;
            m = mgr->getActiveWork();
            while (m) {
                cModel* p = m;
                m = (cModel*) m->getNext();
                func(p);
            }
        }
        TaskSleep(1);
    }
    TaskKill(2);
    TaskChain((TaskFunc) SubScreenExit, 0);
}

// Exit widget: starts its step counter (_rno) at the fade-out.
void SsExitInit::init(SUB_SCREEN* wk)
{
    _rno = 0;
}

// Sub screen close sequence: _rno 0 fade out (FadeSetW 3 frames), 1 wait for the fade and a pending
// weapon change read, 2 free the models, lights and every IdSub/IdNum unit, 3 transit to SsExitMain.
void SsExitInit::move(SUB_SCREEN* wk)
{
    switch (_rno) {
    case 0:
        FadeSetW(0, 3, 0, 0);
        _rno++;
    case 1:
        if (Fade[0].flags & 1) {
            break;
        }
        if (weaponChangeReadCheck() == 0) {
            break;
        }
        _rno++;
    case 2:
        sscrnModelFree(wk);
        sscrnLightClear(wk);
        IdTexRelease(TEX_OWNER_ID_SHARE);
        IdSubErase();
        IdNumErase();
        IdFreeBuffer();
        IdSub.roomInit();
        IdNum.roomInit();
        _rno++;
    case 3:
        transit(0, wk);
        break;
    }
}

// Last widget: clears wk->Loop so SubScreenTask leaves its loop and chains to SubScreenExit.
void SsExitMain::move(SUB_SCREEN* wk)
{
    wk->Loop = 0;
}

// Item examine widget: restarts at the model read step (SsCapMain/SsItemMain/SsPzzlMain set
// p_exam_item/p_exam_model before transiting here).
void SsItemExamine::init(SUB_SCREEN* wk)
{
    _rno = 0;
}

// `&local` arguments through an inlined helper are recomputed at every call (`addi r4, r1, 0x208`)
// instead of being kept in a callee-saved register (integrate.c substitutes the frame address into
// the hard-register argument set).
static inline void ssItemInfo(u16 id, ItemInfo* info)
{
    itemInfo(id, info);
}

// Item examine: reads the item's model (.bin) and texture (.tpl) into the examine buffer, shows it
// with the examine camera and returns to the caller widget on cancel.
void SsItemExamine::move(SUB_SCREEN* wk)
{
    static int exam_read_req;
    static u16 exam_id;
    char name[0x100];
    char name2[0x100];
    ItemInfo info;
    int size;

    switch (_rno) {
    case 0: {
        ssItemInfo(wk->p_exam_item->id, &info);
        switch (info.type) {
        case 1:
            exam_id = ItemMgr.weaponId(wk->p_exam_item);
            break;
        case 9:
            if (wk->p_exam_item->lv == 1) {
                exam_id = ItemMgr.weaponId(ItemMgr.at(wk->p_exam_item->bullet));
            } else {
                exam_id = wk->p_exam_item->id;
            }
            break;
        default:
            exam_id = wk->p_exam_item->id;
            break;
        }
        wk->pItemBin = wk->pExamDat;
        ssItemInfo(exam_id, &info);
        if (info.type == 0xD) {
            sprintf(name, "SS/item/cap%02d.bin", exam_id - 0xDB);
        } else {
            sprintf(name, "SS/item/idm%03x.bin", exam_id);
        }
#line 718 "D:/Bio4/Prog/ss_main.cpp"
        exam_read_req = DVD_READ_N(name, wk->pItemBin, 0, 0, 0, 0x10);
        _rno++;
    }
    case 1: {
        int ret = Dvd.ReadCheck(exam_read_req, &size, 0, 0);
        if (ret == 0) {
            break;
        }
        if (ret == 1) {
            int s = size;
            if (s & 0x1F) {
                size = s + (u8) (0x20 - (s & 0x1F));
            }
            wk->pItemTpl = (u8*) wk->pExamDat + size;
            _rno++;
        } else {
            transit(0, wk);
        }
        break;
    }
    case 2: {
        ssItemInfo(exam_id, &info);
        if (info.type == 0xD) {
            sprintf(name2, "SS/item/cap%02d.tpl", exam_id - 0xDB);
        } else {
            sprintf(name2, "SS/item/idm%03x.tpl", exam_id);
        }
#line 751 "D:/Bio4/Prog/ss_main.cpp"
        exam_read_req = DVD_READ_N(name2, wk->pItemTpl, 0, 0, 0, 0x10);
        _rno++;
    }
    case 3: {
        int ret = Dvd.ReadCheck(exam_read_req, &size, 0, 0);
        if (ret == 0) {
            break;
        }
        if (ret == 1) {
            if ((u32) wk->pItemTpl + size > (u32) wk->pExamDat + 0x3E800) {
                pLog->err(0, 0, "Item Examine: model is too large.");
            }
            _rno++;
        } else {
            transit(0, wk);
        }
        break;
    }
    case 4: {
        // light info origin / size (emitted into .rodata here, before this function's pool)
        static const Vec exam_light_ofs = {0.0f, 0.0f, 0.0f};
        static const Vec exam_light_size = {10000.0f, 10000.0f, 0.0f};
        cMap* m = wk->p_exam_model;
        m->modelInit(wk->pItemBin, wk->pItemTpl);
        m->be_flag |= 0x4000;
        m->LightInfo.init2(0, 1, &exam_light_ofs, &exam_light_size, 0x20);
        m->partsMatCalc();
        m->partsWorldCalc();
        ssItemInfo(exam_id, &info);
        if (info.type == 1) {
            ItemWork* w = 0;
            ssItemInfo(wk->p_exam_item->id, &info);
            switch (info.type) {
            case 1:
                w = wk->p_exam_item;
                break;
            case 9:
                w = ItemMgr.at(wk->p_exam_item->bullet);
                break;
            }
            if (w) {
                _itemExam.level((w->lv >> 12) + 1, ((w->lv >> 8) & 0xF) + 1, ((w->lv >> 4) & 0xF) + 1, (w->lv8[1] & 0xF) + 1);
            }
        }
        ssItemInfo(exam_id, &info);
        if (info.type == 0xD) {
            _itemExam.init(exam_id, m, 2);
        } else {
            _itemExam.init(exam_id, m, 1);
        }
        _rno++;
    }
    case 5: {
        IdUnit* pos;
        _itemExam.move();
        _itemExam.trans();
        pos = IdSub.unitPtr(0xFE, IDC_DATA);
        cMes.setLayout(7, LAYOUT_SUBSCRN);
        cMes.MesSet(exam_id, (int) ((pos->pos0.x + 320.0f) * 0.8f), (int) ((240.0f - pos->pos0.y) * 0.8f), 0x20084, 7, 0, 4);
        if (Key.trg & 0x20000) {
            ssItemInfo(exam_id, &info);
            if (info.type == 0xD) {
                SndStrReq(1, exam_id - 0x14, 0x80000003, 0, 0, 0.0f);
            }
        }
        if (Key.trg & 0xC0000000) {
            _itemExam.quit();
            LightMgr.offKind(0x7F);
            wk->p_exam_model->be_flag &= ~2;
            transit(0, wk);
            SndCall(0, 5, 0, 0, 0, 0);
        }
        break;
    }
    }
}

// Per-frame model draw callback of SubScreenTask: draws every alive MapMgr model with be_flag bit 1.
static void sscrnModelTrans(cModel* m)
{
    if (m->be_flag & 2) {
        ModelTrans(m);
    }
}

// Shows the main menu (top tab row) highlight on menu_next; `no` != 0 shows the cursor unit.
void sscrnMainMenuInit(SUB_SCREEN* wk, int no)
{
    idMainMenu(wk, no);
}

// Top menu tab row input (menu 0 key items/treasures, 1 attache case, 2 map, 3 files, 4 exit): left/right (Key bits 26/27,
// repeat) move menu_next, A picks it into menu_no (clearing close_flag), Y or B on the exit tab
// selects exit, B elsewhere moves to the exit tab. Returns 1 when menu_no was chosen; the caller
// then transits to the widget linked under menu_no.
int sscrnMainMenu(SUB_SCREEN* wk)
{
    int ret = 0;
    s8 old = wk->menu_next;

    if (Key.trg & 0x100000) {
        wk->menu_next = 4;
        wk->menu_no = 4;
        ret = 1;
    } else if (Key.trg & 0x40000000) {
        if (old == 4) {
            wk->menu_next = old;
            wk->menu_no = old;
            ret = 1;
        } else {
            wk->menu_next = 4;
        }
    } else if (Key.rep & 0x08000000) {
        wk->menu_next--;
    } else if (Key.rep & 0x04000000) {
        wk->menu_next++;
    } else if (Key.trg & 0x80000000) {
        wk->menu_old = wk->menu_no;
        wk->menu_no = wk->menu_next;
        ret = 1;
        if (old != 4) {
            wk->close_flag = 0;
            SndCall(0, 4, 0, 0, 0, 0);
        }
    }
    wk->menu_next = (s8) wk->menu_next < 0 ? 4 : ((s8) wk->menu_next > 4 ? 0 : wk->menu_next);
    if (old != (s8) wk->menu_next) {
        idMainMenu(wk, 1);
        SndCall(0, 0xA, 0, 0, 0, 0);
    }
    return ret;
}

// Hides the five tab highlight units (IdSub group 0) and, when sw, shows the one under menu_next
// with its animation restarted.
void idMainMenu(SUB_SCREEN* wk, int sw)
{
    IdUnit* u;
    int i;

    for (i = 0; i < 5; i++) {
        u = IdSub.unitPtr(i, IDC_SSCRN_MAIN_MENU);
        u->be_flag &= ~8;
    }
    if (sw) {
        u = IdSub.unitPtr(wk->menu_next, IDC_SSCRN_MAIN_MENU);
        u->be_flag |= 8;
        IdSub.setTime(u, 0);
    }
}

// Fades the menu background unit (IdSub 7/0) in (sw) or out; skipped on the shop screen (type 0x10).
void idMainMenuFade(SUB_SCREEN* wk, int sw)
{
    if (wk->open_flag != 0x10) {
        IdUnit* u = IdSub.unitPtr(7, IDC_SSCRN_MAIN_MENU);
        if (sw) {
            u->rev_flag &= ~0xF;
        } else {
            u->rev_flag |= 0xF;
        }
    }
}

// Kills every screen-owned IdSub unit group (item, map, file, puzzle, shop groups 0x10..0x1F,
// 0x80..0x84) and releases the sub screen id textures; SsExitInit step 2 and screen switches.
void IdSubErase()
{
    IdSub.kill(0xFF, IDC_SSCRN_CKPT_0);
    IdSub.kill(0xFF, IDC_SSCRN_CKPT_1);
    IdSub.kill(0xFF, IDC_SSCRN_CKPT_2);
    IdSub.kill(0xFF, IDC_SSCRN_CKPT_3);
    IdSub.kill(0xFF, IDC_SSCRN_0);
    IdSub.kill(0xFF, IDC_SSCRN_1);
    IdSub.kill(0xFF, IDC_SSCRN_2);
    IdSub.kill(0xFF, IDC_SSCRN_NEAR_0);
    IdSub.kill(0xFF, IDC_SSCRN_NEAR_1);
    IdSub.kill(0xFF, IDC_SSCRN_NEAR_2);
    IdSub.kill(0xFF, IDC_SSCRN_FAR_0);
    IdSub.kill(0xFF, IDC_SSCRN_FAR_1);
    IdSub.kill(0xFF, IDC_SSCRN_FAR_2);
    IdSub.kill(0xFF, IDC_PRICE_00);
    IdSub.kill(0xFF, IDC_PRICE_01);
    IdSub.kill(0xFF, IDC_PRICE_02);
    IdSub.kill(0xFF, IDC_PRICE_03);
    IdSub.kill(0xFF, IDC_PRICE_04);
    IdTexRelease(TEX_OWNER_ID_SSCRN);
}

// Kills the number display units (IdNum groups 0x40..0x7D individually, 0x10..0x16 whole).
void IdNumErase()
{
    int i;

    for (i = 0; i < 0x3E; i++) {
        IdNum.kill(0xFF, 0x40 + i);
    }
    IdNum.kill(0xFF, IDC_SSCRN_NEAR_0);
    IdNum.kill(0xFF, IDC_SSCRN_NEAR_1);
    IdNum.kill(0xFF, IDC_SSCRN_NEAR_2);
    IdNum.kill(0xFF, IDC_SSCRN_0);
    IdNum.kill(0xFF, IDC_SSCRN_1);
    IdNum.kill(0xFF, IDC_SSCRN_2);
}

// Clears the Z buffer with a full screen quad at the far plane (the model screens draw over the 2D
// background).
void clearZbuffer()
{
    static f32 clear_z = -0.99999f;
    Mtx44 proj;
    Mtx mtx;

    GXSetColorUpdate(0);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 1);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, 1.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetNumTexGens(0);
    GXSetChanCtrl(0, 0, 0, 0, 0, 2, 2);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
    GXSetTevOrder(0, 0xFF, 0xFF, 0xFF);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 7);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    {
        GXColor col = {0x08, 0x08, 0x80, 0x1C};
        GXSetChanMatColor(4, col);
    }
    GXSetBlendMode(1, 4, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(0.0f, 0.0f, clear_z);
    GXPosition3f32(512.0f, 0.0f, clear_z);
    GXPosition3f32(512.0f, 448.0f, clear_z);
    GXPosition3f32(0.0f, 448.0f, clear_z);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
}

// Hides every MapMgr model work above the three reserved ones (player, weapon, examine) by clearing
// be_flag bit 1; screens call it when they take over the model area.
void sscrnModelClear(SUB_SCREEN* wk)
{
    u32 i;

    for (i = 3; i < 0xA0; i++) {
        MapMgr.getWork(i)->be_flag &= ~2;
    }
}

// Undoes generalModelAlloc: restores the DOL parts/model-info managers, destroys the MapMgr models
// and frees the three arrays. No-op unless attr_flag bit 0 is set.
void sscrnModelFree(SUB_SCREEN* wk)
{
    int off = !(wk->attr_flag & 1);

    if (off) {
        return;
    }
    {
        cModel::mm = &ModInfoMgr;
        cModel::pm = &PartsMgr;
        MapMgr.destroyAll();
        ssModInfoMgr.arrayFree();
        ssPartsMgr.arrayFree();
        MapMgr.arrayFree();
        wk->attr_flag &= ~1;
    }
}

// Destroys the eight screen lights (p_light[]).
void sscrnLightClear(SUB_SCREEN* wk)
{
    int i;

    for (i = 0; i < 8; i++) {
        LightMgr.destroy(wk->p_light[i]);
        wk->p_light[i] = 0;
    }
}

// Creates the screen lights 0..2 from the archive's cLit table when not yet alive.
void sscrnLightCreate(SUB_SCREEN* wk, cLit* lit)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (wk->p_light[i] == 0) {
            wk->p_light[i] = LightMgr.create(lit, 0, i, 0);
        }
    }
}

// Three digit number display with the IdNum table `id`: unit 0 is the frame (placed at `pos`), units
// 1..3 the digits (colour of IdSub 0x14/0xFD or 0xFE when flags bit 1 is set), 0x11..0x13 their
// shadows; flags bit 0 hides the leading zeros.
void numDisp(int id, int num, Vec* pos, u32 flags)
{
    IdUnit* col0 = IdSub.unitPtr(0xFD, IDC_SSCRN_0);
    IdUnit* col1 = IdSub.unitPtr(0xFE, IDC_SSCRN_0);
    IdUnit* u;
    int i;

    u = IdNum.unitPtr(0, id);
    u->be_flag &= ~8;
    for (i = 1; i <= 3; i++) {
        u = IdNum.unitPtr(i, id);
        u->be_flag &= ~8;
        if (flags & 2) {
            u->col0[0] = col1->col0[0];
            u->col0[1] = col1->col0[1];
            u->col0[2] = col1->col0[2];
            u->col0[3] = col1->col0[3];
        } else {
            u->col0[0] = col0->col0[0];
            u->col0[1] = col0->col0[1];
            u->col0[2] = col0->col0[2];
            u->col0[3] = col0->col0[3];
        }
    }
    for (i = 0x11; i <= 0x13; i++) {
        u = IdNum.unitPtr(i, id);
        u->be_flag &= ~8;
    }
    if (pos) {
        u8 d[3];
        int on;
        for (i = 0; i < 3; i++) {
            d[i] = num % 10;
            num /= 10;
        }
        on = 0;
        for (i = 2; i >= 0; i--) {
            if ((flags & 1) && on == 0) {
                if (d[i] == 0 && i != 0) {
                    continue;
                }
                on = 1;
            }
            u = IdNum.unitPtr(i + 1, id);
            u->be_flag |= 8;
            u->tex_flag |= 2;
            u->texNo = d[i];
            u = IdNum.unitPtr(i + 0x11, id);
            u->be_flag |= 8;
        }
        u = IdNum.unitPtr(0, id);
        u->be_flag |= 8;
        u->pos0 = *pos;
    }
}

// Queues a character weapon model change (weapon number / type) into the current wepChange slot for
// weaponChangeTask; ignored for Ashley (pl_type 1) who has no weapon model.
void weaponChangeRequest(u16 no, u16 type)
{
    SUB_SCREEN* wk = &SubScreenWk;

    if (pG->pl_type == 1) {
        return;
    }
    switch (pG->pl_type) {
    case 0:
    case 2:
    case 3:
    case 4:
    case 5:
        wk->wepChange[wk->wep_idx].req = 1;
        wk->wepChange[wk->wep_idx].no = no;
        wk->wepChange[wk->wep_idx].type = type;
        break;
    }
}

// 1 when no weapon change request is pending in either slot (SsExitInit waits for it).
int weaponChangeReadCheck()
{
    SUB_SCREEN* wk = &SubScreenWk;

    if (wk->wepChange[0].req == 0 && wk->wepChange[1].req == 0) {
        return 1;
    }
    return 0;
}

// 1 while the weapon change task is not reading (wep_rno 3/4): the character model may be animated.
int weaponChangeMoveCheck()
{
    return SubScreenWk.wep_rno != 3 && SubScreenWk.wep_rno != 4;
}

// Weapon change task (TaskExec priority 2): fades the character and weapon models out, reads the
// requested weapon's model into the weapon buffer, rebuilds the character model and fades back in.
static void weaponChangeTask()
{
    SUB_SCREEN* wk = &SubScreenWk;
    static u16 wep_no;
    static u16 wep_type;
    static s8 wep_slot;
    static int wep_read_req;
    static int fade_out_frame = 5;
    static int fade_in_frame = 5;
    char name[0x40];
    int stat;
    int size;

    for (;;) {
        switch ((s8) wk->wep_rno) {
        case 0:
            if (wk->wepChange[wk->wep_idx].req) {
                wep_slot = wk->wep_idx;
                wk->wep_idx = wk->wep_idx == 0;
                wk->wep_rno++;
                wk->wep_cnt = fade_out_frame;
            }
            break;
        case 1: {
            f32 rate;
            wk->wep_cnt--;
            rate = (f32) wk->wep_cnt / (f32) fade_out_frame;
            if (ssPlModel) {
                ssPlModel->invisible_factor = rate;
            }
            if (ssWepModel2) {
                ssWepModel->invisible_factor = rate;
            }
            if (wk->wep_cnt <= 0) {
                if (ssPlModel) {
                    ssPlModel->be_flag &= ~2;
                }
                if (ssWepModel2) {
                    ssWepModel->be_flag &= ~2;
                }
                wk->wep_rno++;
            }
            break;
        }
        case 2:
            SndBlkStop(2);
            wk->wep_rno++;
        case 3:
            wep_no = wk->wepChange[wep_slot].no;
            wep_type = wk->wepChange[wep_slot].type;
            weaponFilename(name, wep_no);
#line 1439 "D:/Bio4/Prog/ss_main.cpp"
            wep_read_req = DVD_READ_N(name, wk->pWepDat, 0, 0, 0, 0x10);
            if (wep_read_req <= 0) {
                break;
            }
            wk->wep_rno++;
            break;
        case 4:
            if (Dvd.ReadCheck(wep_read_req, &stat, &size, 0) != 1) {
                break;
            }
            wk->wep_rno++;
        case 5:
            switch (pG->pl_type) {
            case 0:
                leonModelInit(wep_no, wep_type);
                break;
            case 1:
                ashleyModelInit();
                break;
            case 2:
                adaModelInit(wep_no, wep_type);
                break;
            case 4:
                klauserModelInit(wep_no, wep_type);
                break;
            case 3:
                hunkModelInit(wep_no, wep_type);
                break;
            case 5:
                weskerModelInit(wep_no, wep_type);
                break;
            }
            if (ssPlModel) {
                ssPlModel->be_flag |= 2;
                ssPlModel->invisible_factor = 0.0f;
            }
            if (ssWepModel2) {
                ssWepModel->be_flag |= 2;
                ssWepModel->invisible_factor = 0.0f;
            }
            wk->wepChange[wep_slot].req = 0;
            wk->wep_cnt = 0;
            wk->wep_rno++;
            break;
        case 6: {
            f32 rate;
            wk->wep_cnt++;
            rate = (f32) wk->wep_cnt / (f32) fade_in_frame;
            if (ssPlModel) {
                ssPlModel->invisible_factor = rate;
            }
            if (ssWepModel2) {
                ssWepModel->invisible_factor = rate;
            }
            if (wk->wep_cnt >= fade_in_frame) {
                if (ssPlModel) {
                    ssPlModel->invisible_factor = 1.0f;
                }
                if (ssWepModel2) {
                    ssWepModel->invisible_factor = 1.0f;
                }
                wk->wep_rno = 0;
            }
            break;
        }
        }
        TaskSleep(1);
    }
}

// Defined after the function-local statics above: objects with constructors are emitted at their
// definition, the statics at their declaration (.bss 0x27C..0x28C, then the managers).
cPartsMgr ssPartsMgr;
cModInfoMgr ssModInfoMgr;


// The split object's .data is 4 bytes longer than the variables (the next unit's .data starts
// 8-aligned in the REL), like ss_file.
asm(".section .data; .balign 8; .section .text");
