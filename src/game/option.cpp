// game/option: the option menu (pause menu in game, and from the title) — OptionScreen drives the
// top menu (retry / load, controller, brightness, audio) and its sub menus on the option id
// archive (pG->pOption), writing the settings into pSys->Config_flg / brightness / sound_mode; also
// the GameResult (game clear / omake) and ChapterEnd result screens on the result id data.
// (D:/Bio4/Prog/option.cpp)
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "id_sys.h"
#include "mes.h"
#include "main.h"
#include "main_sub.h"
#include "pad.h"
#include "snd.h"
#include "cockpit.h"
#include "sscrn.h"
#include "sce.h"
#include "game.h"
#include "math_sub.h"
#include "option.h"

#define OPT_PTR(ofs) ((void*) (*(u32*) ((u8*) pG->pOption + (ofs)) + (u32) pG->pOption))
#define DATA_PTR(d, ofs) ((void*) (*(u32*) ((u8*) (d) + (ofs)) + (u32) (d)))

#define KEY_START 0x2000

extern "C" {
int top_menu(OptionScreen* o);
void back_to_top_menu(OptionScreen* o);
int retry_load_menu(OptionScreen* o);
int controller_menu(OptionScreen* o);
int brightness_menu(OptionScreen* o);
int audio_menu(OptionScreen* o);
void num(int val, int n, int mode, int base, u8 type, int reverse);
}

OptionScreen OptScrn;

// Writes the three-letter language suffix ("jpn", "eng", "ger", "fra", "esp", "ita") into
// name[0..2] for the language-dependent data file names (title / omake / chapter-end archives).
void setLangExt3(char* name)
{
    u8 lang = pSys->language;

    if (lang == 0) {
        name[0] = 'j';
        name[1] = 'p';
        name[2] = 'n';
    } else if (lang == 1) {
        name[0] = 'e';
        name[1] = 'n';
        name[2] = 'g';
    } else if (isLang(lang, 2)) {
        name[0] = 'e';
        name[1] = 'n';
        name[2] = 'g';
    } else if (isLang(lang, 3)) {
        name[0] = 'g';
        name[1] = 'e';
        name[2] = 'r';
    } else if (isLang(lang, 4)) {
        name[0] = 'f';
        name[1] = 'r';
        name[2] = 'a';
    } else if (isLang(lang, 5)) {
        name[0] = 'e';
        name[1] = 's';
        name[2] = 'p';
    } else if (isLang(lang, 6)) {
        name[0] = 'i';
        name[1] = 't';
        name[2] = 'a';
    } else if (lang == 7) {
        name[0] = 'e';
        name[1] = 'n';
        name[2] = 'g';
    } else if (isEurope(lang)) {
        name[0] = 'e';
        name[1] = 'n';
        name[2] = 'g';
    }
}

// May START open the pause option menu now? Not while the sub screen is fading (SubScreenWk.wait)
// and not while Status_flg[0] 0x400 (menus locked) or 0x40 (event running) are set.
int OptionOpenCheck()
{
    if (SubScreenWk.wait > 0) {
        return 0;
    }
    u32 f = pG->Status_flg[0];
    if (f & 0x400) {
        return 0;
    }
    u32 t = f & 0x40;
    return t == 0;
}

// Opens the option menu (`title` = 1 from the title screen, 0 = pause menu in game): hides the
// cockpit ids, loads the option id archive (pOption) with the background (IDC_OPTION_BG, fully faded in
// when from the title) and the top menu (IDC_OPTION); the cursor starts on "controller" when the
// retry entry is disabled (Status_flg[2] 0x8000).
void OptionScreen::init(int type)
{
    _type = type;
    if (type != 0) {
        _msg_attr = 0x94;
    } else {
        _msg_attr = 0x91;
    }
    IdSys.dispSw(IDC_LIFE_METER, 0);
    IdSys.dispSw(IDC_ACT_BUTTON, 0);
    IdSys.dispSw(IDC_COUNT_DOWN, 0);
    IdTexDataLoad(OPT_PTR(0x20), TEX_OWNER_ID_DEAD);
    IdSys.set(OPT_PTR(0x24), 0xFF, IDC_OPTION_BG, 0x13, 4, 0);
    if (_type != 0) {
        IdUnit* u = IdSys.unitPtr(0, IDC_OPTION_BG);
        Hermite1* h = u->curve[2];
        IdSys.setTime(u, (s16) (int) h->key[h->num - 1].t);
    }
    IdSys.set(OPT_PTR(0x28), 0xFF, IDC_OPTION, 0x13, 3, 0);
    _rno0 = 0;
    _rno1 = 0;
    _rno2 = 0;
    _rno3 = 0;
    if (StaFlagChk(pG, STA_TITLE)) {
        _rno1 = 1;
    }
    SndCall(0, 0x33, 0, 0, 0, 0);
}

// One frame of the menu: _rno0 0 = top menu, 1 = the sub menu chosen by _rno1 (0 retry/load, 1
// controller, 2 brightness, 3 audio); returns 1 when the menu should close (4 = back, or START).
int OptionScreen::move()
{
    int ret = 0;

    switch (_rno0) {
    case 0:
        ret = top_menu(this);
        break;
    case 1:
        switch (_rno1) {
        case 0:
            retry_load_menu(this);
            break;
        case 1:
            controller_menu(this);
            break;
        case 2:
            brightness_menu(this);
            break;
        case 3:
            audio_menu(this);
            break;
        case 4:
            ret = 1;
            break;
        }
        break;
    }
    return ret;
}

// Frees the option textures and kills both id groups.
void OptionScreen::quit()
{
    IdTexRelease(TEX_OWNER_ID_DEAD);
    IdSys.kill(0xFF, IDC_OPTION_BG);
    IdSys.kill(0xFF, IDC_OPTION);
}

// Copies the highlight colour of the cursor unit into a menu item.
static inline void setColor(IdUnit* u, IdUnit* base)
{
    u->col0[0] = (u8) base->col[0];
    u->col0[1] = (u8) base->col[1];
    u->col0[2] = (u8) base->col[2];
    u->col0[3] = (u8) base->col[3];
}

// Top menu (_rno0 == 0): up/down move _rno1 over the five entries (retry/load greyed out when
// Status_flg[2] 0x8000), A loads the chosen sub menu's id layout (OPT_PTR 0x2C..0x38) and copies
// the current pSys settings into the screen (m_reverse / m_vibration / m_knife_key / sound), B
// jumps to "back" or (on it) closes. Returns 1 to close (START or back).
int top_menu(OptionScreen* pOpt)
{
    static int x0 = 100;
    static int y0 = 245;
    s8 old = pOpt->_rno1;
    IdUnit* base;
    IdUnit* u;
    int i;

    if (Key.trg & KEY_START) {
        return 1;
    }
    if (Key.trg & KEY_B) {
        if (old == 4) {
            pOpt->_rno0 = 1;
            pOpt->_rno2 = 0;
            pOpt->_rno3 = 0;
            return 0;
        }
        pOpt->_rno1 = 4;
        SndCall(0, 0x39, 0, 0, 0, 0);
    } else if (Key.trg & KEY_A) {
        void* data = 0;

        switch (old) {
        case 0:
            data = OPT_PTR(0x2C);
            break;
        case 1:
            data = OPT_PTR(0x30);
            break;
        case 2:
            data = OPT_PTR(0x34);
            break;
        case 3:
            data = OPT_PTR(0x38);
            break;
        }
        if (pOpt->_rno1 != 4) {
            IdSys.kill(0xFF, IDC_OPTION);
            IdSys.set(data, 0xFF, IDC_OPTION, 0x13, 3, 0);
            SndCall(0, 0x36, 0, 0, 0, 0);
        }
        if (pOpt->_rno1 == 2) {
            IdSys.unitPtr(0, IDC_OPTION_BG)->rev_flag |= 0xF;
        }
        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
            pOpt->m_reverse = 1;
        } else {
            pOpt->m_reverse = 0;
        }
        if (CfgFlagChk(pSys, CFG_VIBRATION)) {
            pOpt->m_vibration = 1;
        } else {
            pOpt->m_vibration = 0;
        }
        if (CfgFlagChk(pSys, CFG_KNIFE_MODE)) {
            pOpt->m_knife_key = 1;
        } else {
            pOpt->m_knife_key = 0;
        }
        switch (pSys->SndMode) {
        case 0:
            pOpt->m_snd_mode = 1;
            break;
        case 1:
            pOpt->m_snd_mode = 0;
            break;
        case 2:
            pOpt->m_snd_mode = 2;
            break;
        default:
            pOpt->m_snd_mode = 0;
            break;
        }
        pOpt->_rno0 = 1;
        pOpt->_rno2 = 0;
        pOpt->_rno3 = 0;
        if (pOpt->_rno1 == 3) {
            pOpt->_rno2 = pOpt->m_snd_mode;
        }
        return 0;
    } else {
        if (Key.trg & KEY_UP) {
            pOpt->_rno1--;
        }
        if (Key.trg & KEY_DOWN) {
            pOpt->_rno1++;
        }
        pOpt->_rno1 = pOpt->_rno1 < 0 ? 0 : (pOpt->_rno1 > 4 ? 4 : pOpt->_rno1);
        if (StaFlagChk(pG, STA_TITLE) && pOpt->_rno1 == 0 && (Key.trg & KEY_UP)) {
            pOpt->_rno1 = 1;
        }
        if (old != pOpt->_rno1) {
            SndCall(0, 0x34, 0, 0, 0, 0);
        }
    }
    base = IdSys.unitPtr(8, IDC_OPTION);
    if (old != pOpt->_rno1) {
        IdSys.setTime(base, 0);
    }
    for (i = 0; i < 5; i++) {
        u = IdSys.unitPtr((u8) (i + 2), IDC_OPTION);
        if (pOpt->_rno1 == i) {
            setColor(u, base);
        } else {
            u->col0[3] = u->col0[2] = u->col0[1] = u->col0[0] = 0xFF;
        }
        if (StaFlagChk(pG, STA_TITLE) && i == 0) {
            u->col0[0] = 0x40;
            u->col0[1] = 0x40;
            u->col0[2] = 0x40;
            u->col0[3] = 0xFF;
        }
    }
    {
        u8 mes[5] = {0x81, 0x82, 0x83, 0x84, 0x85};

        cMes.MesSet(mes[pOpt->_rno1], x0, y0, pOpt->_msg_attr, 0, 0, 4);
    }
    return 0;
}

// Leaves a sub menu: reloads the top menu id layout, _rno0 = 0, cancel SE.
void back_to_top_menu(OptionScreen* pOpt)
{
    IdSys.kill(0xFF, IDC_OPTION);
    IdSys.set(OPT_PTR(0x28), 0xFF, IDC_OPTION, 0x13, 3, 0);
    pOpt->_rno0 = 0;
    SndCall(0, 0x39, 0, 0, 0, 0);
}

// Retry / load sub menu: _rno3 0 cursor over retry / load / quit-to-title / back (load greyed when
// System_flg bit31 / 0x40000000: no card), 1 yes/no confirm (mes 0x98 retry, 0x8A quit), 2 loads
// from the card (CardLoad; on failure rebuilds the menu), 3 waits for the SE then System_flg
// 0x04000000 = return to the title. Retry calls GameContinue(1).
int retry_load_menu(OptionScreen* pOpt)
{
    static int yes = 0;
    static u32 snd_id = 0;
    int old = pOpt->_rno2;
    int confirm = 0;
    IdUnit* base;
    IdUnit* u;
    int i;

    switch (pOpt->_rno3) {
    case 0:
        if (Key.trg & KEY_B) {
            if (old == 3) {
                back_to_top_menu(pOpt);
                return 0;
            }
            pOpt->_rno2 = 3;
            SndCall(0, 0x39, 0, 0, 0, 0);
        } else if (Key.trg & KEY_A) {
            switch (old) {
            case 0:
            case 2: {
                static int x0 = 100;
                static int y0 = 245;
                int no;

                if (pOpt->_rno2 == 0) {
                    no = 0x98;
                } else {
                    no = 0x8A;
                }
                cMes.MesSet(no, x0, y0, (pOpt->_msg_attr | 0x40) & ~0x80, 0, 0, 4);
                cMes.m_Msg[0].m_cur = 1;
                pOpt->_rno3 = 1;
                confirm = 1;
                yes = 1;
                SndCall(0, 0x37, 0, 0, 0, 0);
                break;
            }
            case 1:
                pOpt->_rno3 = 2;
                SndCall(0, 0x36, 0, 0, 0, 0);
                break;
            case 3:
                back_to_top_menu(pOpt);
                return 0;
            }
        } else {
            if (Key.trg & KEY_UP) {
                pOpt->_rno2--;
            }
            if (Key.trg & KEY_DOWN) {
                pOpt->_rno2++;
            }
            pOpt->_rno2 = pOpt->_rno2 < 0 ? 0 : (pOpt->_rno2 > 3 ? 3 : pOpt->_rno2);
            if (FlagChkSignW(pG->System_flg, SYS_OMAKE_ADA_GAME) || (SysFlagChk(pG, SYS_OMAKE_ETC_GAME))) {
                if (pOpt->_rno2 == 1) {
                    if (Key.trg & KEY_UP) {
                        pOpt->_rno2 = 0;
                    }
                    if (Key.trg & KEY_DOWN) {
                        pOpt->_rno2 = 2;
                    }
                }
            }
            if (old != pOpt->_rno2) {
                SndCall(0, 0x34, 0, 0, 0, 0);
            }
        }
        base = IdSys.unitPtr(8, IDC_OPTION);
        if (old != pOpt->_rno2) {
            IdSys.setTime(base, 0);
        }
        for (i = 0; i < 4; i++) {
            u = IdSys.unitPtr((u8) (i + 2), IDC_OPTION);
            if (pOpt->_rno2 == i) {
                setColor(u, base);
            } else {
                u->col0[3] = u->col0[2] = u->col0[1] = u->col0[0] = 0xFF;
            }
            if ((FlagChkSignW(pG->System_flg, SYS_OMAKE_ADA_GAME) || (SysFlagChk(pG, SYS_OMAKE_ETC_GAME))) && i == 1) {
                u->col0[0] = 0x40;
                u->col0[1] = 0x40;
                u->col0[2] = 0x40;
                u->col0[3] = 0xFF;
            }
        }
        if (confirm == 0) {
            static int x0 = 100;
            static int y0 = 245;
            u8 mes[4] = {0x87, 0x88, 0x89, 0x86};

            cMes.MesSet(mes[pOpt->_rno2], x0, y0, pOpt->_msg_attr, 0, 0, 4);
        }
        break;
    case 1:
        if (Key.trg & KEY_B) {
            pOpt->_rno3 = 0;
            cMes.Delete(0);
            SndCall(0, 0x39, 0, 0, 0, 0);
        } else {
            s8 res = cMes.GetSelectMessage(0);

            if (res != 0) {
                cMes.Delete(0);
                if (res == 1) {
                    switch (pOpt->_rno2) {
                    case 0:
                        GameContinue(1);
                        SndCall(0, 0x38, 0, 0, 0, 0);
                        break;
                    case 2:
                        snd_id = SndCall(0, 0x38, 0, 0, 0, 0);
                        pOpt->_rno3 = 3;
                        break;
                    }
                } else {
                    pOpt->_rno3 = 0;
                    SndCall(0, 0x39, 0, 0, 0, 0);
                }
            } else {
                if (Key.trg & KEY_LEFT) {
                    yes = 1;
                } else if (Key.trg & KEY_RIGHT) {
                    yes = 0;
                }
                if (Key.trg & (KEY_LEFT | KEY_RIGHT)) {
                    SndCall(0, 0x34, 0, 0, 0, 0);
                }
            }
        }
        break;
    case 2:
        if (CardLoad() == 1) {
            ScreenReSize(0x200, 0x1C0);
            GameContinue(1);
            GameLoad();
        } else {
            ScreenReSize(0x200, 0x1C0);
            if (pOpt->_type != 1) {
                Cckpt.roomInit();
                Cckpt.move();
                Cockpit* ck = &Cckpt;
                ck->lifeMeterFix(1);
                ck->lifeMeterDisp(0);
            }
            IdTexDataLoad(OPT_PTR(0x20), TEX_OWNER_ID_DEAD);
            IdSys.set(OPT_PTR(0x24), 0xFF, IDC_OPTION_BG, 0x13, 4, 0);
            {
                IdUnit* bg = IdSys.unitPtr(0, IDC_OPTION_BG);
                Hermite1* h = bg->curve[2];
                IdSys.setTime(bg, (s16) (int) h->key[h->num - 1].t);
            }
            IdSys.kill(0xFF, IDC_OPTION);
            IdSys.set(OPT_PTR(0x2C), 0xFF, IDC_OPTION, 0x13, 3, 0);
            pOpt->_rno3 = 0;
        }
        break;
    case 3:
        if (SndEndCheck(snd_id)) {
            SysFlagOn(pG, SYS_SOFT_RESET);
        }
        break;
    }
    return 0;
}

// Controller sub menu: entries reverse camera (pSys->Config_flg bit31), vibration (0x08000000, with a
// test rumble), knife key (0x04000000), back; left/right toggle, A applies to pSys->Config_flg.
int controller_menu(OptionScreen* pOpt)
{
    static int vib_time = 10;
    static int vib_level = 0xFF;
    static int x0 = 100;
    static int y0 = 245;
    int old = pOpt->_rno2;
    IdUnit* base;
    IdUnit* u;
    IdUnit* off;
    int i;
    int j;

    if (Key.trg & KEY_B) {
        if (old == 3) {
            back_to_top_menu(pOpt);
            return 0;
        }
        pOpt->_rno2 = 3;
        SndCall(0, 0x39, 0, 0, 0, 0);
    } else if (Key.trg & KEY_A) {
        switch (old) {
        case 0:
            if (pOpt->m_reverse) {
                CfgFlagOn(pSys, CFG_AIM_REVERSE);
            } else {
                CfgFlagOff(pSys, CFG_AIM_REVERSE);
            }
            break;
        case 1:
            if (pOpt->m_vibration) {
                CfgFlagOn(pSys, CFG_VIBRATION);
                VibSet(vib_time, vib_level, 0, 4);
            } else {
                CfgFlagOff(pSys, CFG_VIBRATION);
            }
            break;
        case 2:
            if (pOpt->m_knife_key) {
                CfgFlagOn(pSys, CFG_KNIFE_MODE);
            } else {
                CfgFlagOff(pSys, CFG_KNIFE_MODE);
            }
            break;
        case 3:
            back_to_top_menu(pOpt);
            return 0;
        }
        SndCall(0, 0x3A, 0, 0, 0, 0);
    } else {
        int no = old;
        s8 now;

        switch (no) {
        case 0:
            old = pOpt->m_reverse;
            if (Key.trg & KEY_LEFT) {
                pOpt->m_reverse = 1;
            }
            if (Key.trg & KEY_RIGHT) {
                pOpt->m_reverse = 0;
            }
            now = pOpt->m_reverse;
            break;
        case 1:
            old = pOpt->m_vibration;
            if (Key.trg & KEY_LEFT) {
                pOpt->m_vibration = 1;
            }
            if (Key.trg & KEY_RIGHT) {
                pOpt->m_vibration = 0;
            }
            now = pOpt->m_vibration;
            break;
        case 2:
            old = pOpt->m_knife_key;
            if (Key.trg & KEY_LEFT) {
                pOpt->m_knife_key = 0;
            }
            if (Key.trg & KEY_RIGHT) {
                pOpt->m_knife_key = 1;
            }
            now = pOpt->m_knife_key;
            break;
        default:
            goto updown;
        }
        if (old != now) {
            SndCall(0, 0x35, 0, 0, 0, 0);
        } else {
        updown:
            old = pOpt->_rno2;
            if (Key.trg & KEY_UP) {
                pOpt->_rno2--;
            }
            if (Key.trg & KEY_DOWN) {
                pOpt->_rno2++;
            }
            if (pG->pl_type != 0 && pG->pl_type != 4 && pOpt->_rno2 == 2) {
                if (Key.trg & KEY_UP) {
                    pOpt->_rno2 = 1;
                }
                if (Key.trg & KEY_DOWN) {
                    pOpt->_rno2 = 3;
                }
            }
            pOpt->_rno2 = pOpt->_rno2 < 0 ? 0 : (pOpt->_rno2 > 3 ? 3 : pOpt->_rno2);
            if (old != pOpt->_rno2) {
                SndCall(0, 0x34, 0, 0, 0, 0);
            }
        }
    }
    base = IdSys.unitPtr(8, IDC_OPTION);
    IdUnit* sel = 0;
    IdUnit* uns = 0;
    if (old != pOpt->_rno2) {
        IdSys.setTime(base, 0);
    }
    for (i = 0; i < 4; i++) {
        u8 id = 0;

        switch (i) {
        case 0:
            id = 2;
            break;
        case 1:
            id = 5;
            break;
        case 2:
            id = 0x11;
            break;
        case 3:
            id = 9;
            break;
        }
        u = IdSys.unitPtr(id, IDC_OPTION);
        if (pOpt->_rno2 == i) {
            setColor(u, base);
        } else {
            u->col0[3] = u->col0[2] = u->col0[1] = u->col0[0] = 0xFF;
        }
        if (pG->pl_type != 0 && pG->pl_type != 4 && i == 2) {
            u->col0[0] = 0x40;
            u->col0[1] = 0x40;
            u->col0[2] = 0x40;
            u->col0[3] = 0xFF;
        }
    }
    off = IdSys.unitPtr(0xE, IDC_OPTION);
    for (j = 0; j < 3; j++) {
        switch (j) {
        case 0:
            if (pOpt->m_reverse) {
                sel = IdSys.unitPtr(3, IDC_OPTION);
                uns = IdSys.unitPtr(4, IDC_OPTION);
            } else {
                uns = IdSys.unitPtr(3, IDC_OPTION);
                sel = IdSys.unitPtr(4, IDC_OPTION);
            }
            break;
        case 1:
            if (pOpt->m_vibration) {
                sel = IdSys.unitPtr(6, IDC_OPTION);
                uns = IdSys.unitPtr(7, IDC_OPTION);
            } else {
                uns = IdSys.unitPtr(6, IDC_OPTION);
                sel = IdSys.unitPtr(7, IDC_OPTION);
            }
            break;
        case 2:
            if (pOpt->m_knife_key) {
                uns = IdSys.unitPtr(0x12, IDC_OPTION);
                sel = IdSys.unitPtr(0x13, IDC_OPTION);
            } else {
                sel = IdSys.unitPtr(0x12, IDC_OPTION);
                uns = IdSys.unitPtr(0x13, IDC_OPTION);
            }
            break;
        }
        if (j == pOpt->_rno2) {
            setColor(sel, base);
        } else {
            sel->col0[3] = sel->col0[2] = sel->col0[1] = sel->col0[0] = 0xFF;
        }
        uns->col0[0] = off->col0[0];
        uns->col0[1] = off->col0[1];
        uns->col0[2] = off->col0[2];
        uns->col0[3] = off->col0[3];
    }
    asm("" : : "r"(sel));  // COMPILER-DIFF: candidate (global.c allocno order: sel 29/338 must outrank o 42/600 for r31)
    if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
        IdSys.unitPtr(0xA, IDC_OPTION)->be_flag |= 8;
        IdSys.unitPtr(0xB, IDC_OPTION)->be_flag &= ~8;
    } else {
        IdSys.unitPtr(0xA, IDC_OPTION)->be_flag &= ~8;
        IdSys.unitPtr(0xB, IDC_OPTION)->be_flag |= 8;
    }
    if (CfgFlagChk(pSys, CFG_VIBRATION)) {
        IdSys.unitPtr(0xC, IDC_OPTION)->be_flag |= 8;
        IdSys.unitPtr(0xD, IDC_OPTION)->be_flag &= ~8;
    } else {
        IdSys.unitPtr(0xC, IDC_OPTION)->be_flag &= ~8;
        IdSys.unitPtr(0xD, IDC_OPTION)->be_flag |= 8;
    }
    if (CfgFlagChk(pSys, CFG_KNIFE_MODE)) {
        IdSys.unitPtr(0x14, IDC_OPTION)->be_flag &= ~8;
        IdSys.unitPtr(0x15, IDC_OPTION)->be_flag |= 8;
    } else {
        IdSys.unitPtr(0x14, IDC_OPTION)->be_flag |= 8;
        IdSys.unitPtr(0x15, IDC_OPTION)->be_flag &= ~8;
    }
    {
        u8 mes[4] = {0x8B, 0x8C, 0x92, 0x86};

        cMes.MesSet(mes[pOpt->_rno2], x0, y0, pOpt->_msg_attr, 0, 0, 4);
    }
    return 0;
}

// Brightness sub menu: left/right change pSys->brightness (and pRK->base_brightness) around DEFAULT
// within MIN_OFS..MAX_OFS, shows the signed level as digits and slides the marker; cursor 1 = back.
int brightness_menu(OptionScreen* pOpt)
{
    static int DEFAULT = 0x40;
    static int MIN_OFS = -30;
    static int MAX_OFS = 50;
    static int x0 = 100;
    static int y0 = 245;
    s8 old = pOpt->_rno2;
    IdUnit* base;
    IdUnit* u;
    int level;
    int digits;
    int i;
    int j;
    Vec a;
    Vec b;
    Vec c;
    f32 rate;

    if (Key.trg & KEY_B) {
        if (old == 1) {
            back_to_top_menu(pOpt);
            IdSys.unitPtr(0, IDC_OPTION_BG)->rev_flag &= ~0xF;
            return 0;
        }
        pOpt->_rno2 = 1;
        SndCall(0, 0x39, 0, 0, 0, 0);
    } else if (Key.trg & KEY_A) {
        switch (old) {
        case 0:
            break;
        case 1:
            back_to_top_menu(pOpt);
            IdSys.unitPtr(0, IDC_OPTION_BG)->rev_flag &= ~0xF;
            return 0;
        }
    } else {
        if (old == 0) {
            u8 bright = pSys->brightness;
            int n;

            if (Key.rep2 & KEY_LEFT) {
                pSys->brightness--;
            }
            if (Key.rep2 & KEY_RIGHT) {
                pSys->brightness++;
            }
            {
                // COMPILER-DIFF: candidate (global alloc order): pSys must be allocated after DEFAULT
                // (target r10/r11; ours has pSys 4 refs/18 = 0.444 > DEFAULT 3/9 = 0.333).
                register SYSTEM_SAVE_WORK* s asm("r10") = pSys;

                if (s->brightness < DEFAULT + MIN_OFS) {
                    // COMPILER-DIFF: candidate (local-alloc qty order): the byte-narrowed DEFAULT must take
                    // r9 (D dies into the sum) so the two `stb r9,0xa(r10)` tails cross-jump.
                    register u8 d asm("r9") = DEFAULT;
                    s->brightness = d + MIN_OFS;
                } else {
                    n = s->brightness;
                    if (n > DEFAULT + MAX_OFS) {
                        n = DEFAULT + MAX_OFS;
                    }
                    s->brightness = n;
                }
            }
            pRK->base_brightness = pSys->brightness;
            if (bright != pSys->brightness) {
                SndCall(0, 0x3B, 0, 0, 0, 0);
            }
        }
        {
            old = pOpt->_rno2;
            if (Key.trg & KEY_UP) {
                pOpt->_rno2--;
            }
            if (Key.trg & KEY_DOWN) {
                pOpt->_rno2++;
            }
            pOpt->_rno2 = pOpt->_rno2 < 0 ? 0 : (pOpt->_rno2 > 1 ? 1 : pOpt->_rno2);
            if (old != pOpt->_rno2) {
                SndCall(0, 0x34, 0, 0, 0, 0);
            }
        }
    }
    level = pSys->brightness - DEFAULT;
    base = IdSys.unitPtr(8, IDC_OPTION);
    if (old != pOpt->_rno2) {
        IdSys.setTime(base, 0);
    }
    digits = (int) fabsf((f32) level);
    if (level == 0) {
        IdSys.unitPtr(4, IDC_OPTION)->be_flag &= ~8;
        IdSys.unitPtr(5, IDC_OPTION)->be_flag &= ~8;
    } else if (level > 0) {
        IdSys.unitPtr(4, IDC_OPTION)->be_flag &= ~8;
        IdSys.unitPtr(5, IDC_OPTION)->be_flag |= 8;
    } else if (level < 0) {
        IdSys.unitPtr(4, IDC_OPTION)->be_flag |= 8;
        IdSys.unitPtr(5, IDC_OPTION)->be_flag &= ~8;
    }
    for (i = 0; i < 2; i++) {
        u = IdSys.unitPtr((u8) (i + 4), IDC_OPTION);
        if (pOpt->_rno2 == 0) {
            setColor(u, base);
        } else {
            u->col0[0] = 0xFF;
            u->col0[1] = 0xFF;
            u->col0[2] = 0xFF;
            u->col0[3] = 0xFF;
        }
    }
    for (j = 0; j < 2; j++) {  // its own counter: a shared `i` outranks o/digits for r29
        int d = digits % 10;

        digits /= 10;
        u = IdSys.unitPtr((u8) (3 - j), IDC_OPTION);
        u->texNo = d;
        u->tex_flag |= 2;
        if (pOpt->_rno2 == 0) {
            setColor(u, base);
        } else {
            u->col0[0] = 0xFF;
            u->col0[1] = 0xFF;
            u->col0[2] = 0xFF;
            u->col0[3] = 0xFF;
        }
    }
    a = IdSys.unitPtr(9, IDC_OPTION)->pos0;
    b = IdSys.unitPtr(0x10, IDC_OPTION)->pos0;
    rate = (f32) (pSys->brightness - (DEFAULT + MIN_OFS)) / (f32) (MAX_OFS - MIN_OFS);
    PSVECSubtract(&b, &a, &c);
    PSVECScale(&c, &c, rate);
    PSVECAdd(&a, &c, &IdSys.unitPtr(1, IDC_OPTION)->pos0);
    u = IdSys.unitPtr(6, IDC_OPTION);
    if (pOpt->_rno2 == 1) {
        setColor(u, base);
    } else {
        u->col0[0] = 0xFF;
        u->col0[1] = 0xFF;
        u->col0[2] = 0xFF;
        u->col0[3] = 0xFF;
    }
    {
        u8 mes[2] = {0x8D, 0x86};

        cMes.MesSet(mes[pOpt->_rno2], x0, y0, pOpt->_msg_attr, 0, 0, 4);
    }
    return 0;
}

// Audio sub menu: mono / stereo / surround, A applies SndSetOutputMode (pSys->SndMode) and
// keeps `sound` as the checked entry; cursor 3 = back.
int audio_menu(OptionScreen* pOpt)
{
    static int x0 = 100;
    static int y0 = 245;
    s8 old = pOpt->_rno2;
    IdUnit* base;
    IdUnit* cur;
    IdUnit* u;
    int i;

    if (Key.trg & KEY_B) {
        if (old == 3) {
            back_to_top_menu(pOpt);
            return 0;
        }
        pOpt->_rno2 = 3;
        SndCall(0, 0x39, 0, 0, 0, 0);
    } else if (Key.trg & KEY_A) {
        switch (old) {
        case 0:
            SndSetOutputMode(1, 0);
            break;
        case 1:
            SndSetOutputMode(0, 0);
            break;
        case 2:
            SndSetOutputMode(2, 0);
            break;
        case 3:
            back_to_top_menu(pOpt);
            return 0;
        }
        if (pOpt->_rno2 != 3) {
            pOpt->m_snd_mode = pOpt->_rno2;
        }
        SndCall(0, 0x3A, 0, 0, 0, 0);
    } else {
        old = pOpt->_rno2;
        if (Key.trg & KEY_UP) {
            pOpt->_rno2--;
        }
        if (Key.trg & KEY_DOWN) {
            pOpt->_rno2++;
        }
        pOpt->_rno2 = pOpt->_rno2 < 0 ? 0 : (pOpt->_rno2 > 3 ? 3 : pOpt->_rno2);
        if (old != pOpt->_rno2) {
            SndCall(0, 0x34, 0, 0, 0, 0);
        }
    }
    base = IdSys.unitPtr(8, IDC_OPTION);
    cur = IdSys.unitPtr(0xE, IDC_OPTION);
    if (old != pOpt->_rno2) {
        IdSys.setTime(base, 0);
    }
    for (i = 0; i < 4; i++) {
        u = IdSys.unitPtr((u8) (i + 2), IDC_OPTION);
        if (pOpt->_rno2 == i) {
            setColor(u, base);
        } else if (i == 3 || i == pOpt->m_snd_mode) {
            u->col0[0] = 0xFF;
            u->col0[1] = 0xFF;
            u->col0[2] = 0xFF;
            u->col0[3] = 0xFF;
        } else {
            u->col0[0] = cur->col0[0];
            u->col0[1] = cur->col0[1];
            u->col0[2] = cur->col0[2];
            u->col0[3] = cur->col0[3];
        }
    }
    IdSys.unitPtr(0xA, IDC_OPTION)->be_flag &= ~8;
    IdSys.unitPtr(0xB, IDC_OPTION)->be_flag &= ~8;
    IdSys.unitPtr(0xC, IDC_OPTION)->be_flag &= ~8;
    switch (pSys->SndMode) {
    case 1:
        u = IdSys.unitPtr(0xA, IDC_OPTION);
        break;
    case 0:
        u = IdSys.unitPtr(0xB, IDC_OPTION);
        break;
    case 2:
        u = IdSys.unitPtr(0xC, IDC_OPTION);
        break;
    default:
        u = IdSys.unitPtr(0xA, IDC_OPTION);
        break;
    }
    u->be_flag |= 8;
    {
        u8 mes[4] = {0x8E, 0x8E, 0x8E, 0x86};

        cMes.MesSet(mes[pOpt->_rno2], x0, y0, pOpt->_msg_attr, 0, 0, 4);
    }
    return 0;
}

// Shows `val` as `n` decimal digits on the id units base.. (reverse: base - i); mode 1 hides
// leading zeros.
void num(int no, int digit_num, int flag, int mark_bottom, u8 id_class, int reverse)
{
    u8 d[8];
    int show;
    int i;

    for (int j = 0; j < digit_num; j++) {
        d[j] = no % 10;
        no /= 10;
    }
    show = 1;
    if (flag == 1) {
        show = 0;
    }
    for (i = digit_num - 1; i >= 0; i--) {
        IdUnit* u;

        if (reverse == 0) {
            u = IdSys.unitPtr((u8) (mark_bottom + i), id_class);
        } else {
            u = IdSys.unitPtr((u8) (mark_bottom - i), id_class);
        }
        if (show == 0 && d[i] == 0 && i != 0) {
            u->be_flag &= ~8;
        } else {
            u->be_flag |= 8;
            show = 1;
            u->tex_flag |= 2;
            u->texNo = d[i];
        }
    }
}

// Game clear result screen: replaces the cockpit ids with the result id archive `d` (IDC_TITLE).
void GameResult::init(void* d)
{
    _addr = d;
    IdTexRelease(TEX_OWNER_ID_COCKPIT);
    IdSys.roomInit();
    IdTexDataLoad(DATA_PTR(_addr, 0x10), TEX_OWNER_ID_TITLE);
    IdSys.set(DATA_PTR(_addr, 0x14), 0xFF, IDC_TITLE, 0x13, 6, 0);
    _rno0 = 0;
    _rno1 = 0;
    _rno2 = 0;
    _rno3 = 0;
}

// Shows hit rate (g_hit_cnt / g_shot_cnt), kills, continues, play time (h:m:s) and the "2nd run"
// mark when game_cnt > 1; returns 1 when A is pressed.
int GameResult::move()
{
    u32 h;
    u32 m;
    u32 s;
    IdUnit* u;
    int hit;

    if (pG->g_shot_cnt != 0) {
        hit = (int) ((f32) pG->g_hit_cnt * 100.0f / (f32) pG->g_shot_cnt + 0.5f);
    } else {
        hit = 0;
    }
    num(hit, 3, 1, 1, IDC_TITLE, 0);
    num(pG->g_kill_cnt, 4, 1, 0x11, IDC_TITLE, 0);
    num(pG->g_continue_cnt, 3, 1, 0x21, IDC_TITLE, 0);
    SecToTime(pG->play_time, &h, &m, &s);
    num(h, 2, 0, 0x35, IDC_TITLE, 0);
    num(m, 2, 0, 0x33, IDC_TITLE, 0);
    num(s, 2, 0, 0x31, IDC_TITLE, 0);
    u = IdSys.unitPtr(0x30, IDC_TITLE);
    u->texNo = 0xB;
    u->be_flag |= 8;
    u->tex_flag |= 2;
    u = IdSys.unitPtr(0, IDC_TITLE);
    if (pG->game_cnt > 1) {
        u->be_flag |= 8;
    } else {
        u->be_flag &= ~8;
    }
    if (Key.trg & KEY_A) {
        return 1;
    }
    return 0;
}

// Restores the cockpit ids.
void GameResult::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}

// Omake (bonus unlocked) screen: the second id layout of the result archive.
void GameResult::omake_init(void* d)
{
    _addr = d;
    IdTexRelease(TEX_OWNER_ID_COCKPIT);
    IdSys.roomInit();
    IdTexDataLoad(DATA_PTR(_addr, 0x10), TEX_OWNER_ID_TITLE);
    IdSys.set(DATA_PTR(_addr, 0x18), 0xFF, IDC_TITLE, 0x13, 6, 0);
}

// Waits for A; returns 1 to leave.
int GameResult::omake_move()
{
    if (Key.trg & KEY_A) {
        return 1;
    }
    return 0;
}

// Chapter end screen for chapter `ch` (SceChapterEnd): the chapter result id layout of archive `d`.
void ChapterEnd::init(void* d, u8 no)
{
    _addr = d;
    IdTexRelease(TEX_OWNER_ID_COCKPIT);
    IdSys.roomInit();
    IdTexDataLoad(DATA_PTR(_addr, 0x10), TEX_OWNER_ID_TITLE);
    IdSys.set(DATA_PTR(_addr, 0x14), 0xFF, IDC_TITLE, 0x13, 6, 0);
    _chapter = no;
}

// Fills the chapter result: this chapter / next chapter numbers ("chap-sec"), chapter and total hit
// rates, kills and continues (c_* chapter counters, g_* game counters). Never returns 1: the
// scenario task ends the screen.
int ChapterEnd::move()
{
    static u8 char_per = 0xA;
    static u8 char_bar = 0xB;
    // 16 unused frame bytes before the address-taken ints (0x18..0x28 in the original): an
    // aggregate local that is never referenced still takes its slot.
    Vec unused;
    int chap;
    int sec;
    int chap2;
    int sec2;
    IdUnit* u;
    int hit;

    getChapterSection(_chapter, &chap, &sec);
    u = IdSys.unitPtr(0, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = chap;
    u = IdSys.unitPtr(1, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = char_bar;
    u = IdSys.unitPtr(2, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = sec;
    u = IdSys.unitPtr(0x1A, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = sec - 1;
    getChapterSection(_chapter + 1, &chap2, &sec2);
    u = IdSys.unitPtr(3, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = chap2;
    u = IdSys.unitPtr(4, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = char_bar;
    u = IdSys.unitPtr(5, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = sec2;
    if (pG->c_shot_cnt != 0) {
        hit = (int) ((f32) pG->c_hit_cnt * 100.0f / (f32) pG->c_shot_cnt + 0.5f);
    } else {
        hit = 0;
    }
    num(hit, 3, 1, 8, IDC_TITLE, 1);
    u = IdSys.unitPtr(9, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = char_per;
    if (pG->g_shot_cnt != 0) {
        hit = (int) ((f32) pG->g_hit_cnt * 100.0f / (f32) pG->g_shot_cnt + 0.5f);
    } else {
        hit = 0;
    }
    num(hit, 3, 1, 0xC, IDC_TITLE, 1);
    u = IdSys.unitPtr(0xD, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = char_per;
    num(pG->c_kill_cnt, 3, 1, 0x10, IDC_TITLE, 1);
    num(pG->g_kill_cnt, 3, 1, 0x13, IDC_TITLE, 1);
    num(pG->c_continue_cnt, 3, 1, 0x16, IDC_TITLE, 1);
    num(pG->g_continue_cnt, 3, 1, 0x19, IDC_TITLE, 1);
    return 0;
}

// Restores the cockpit ids.
void ChapterEnd::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}
