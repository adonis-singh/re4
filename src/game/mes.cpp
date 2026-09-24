// game/mes: in-game message system (D:/Bio4/Prog/mes.cpp).
#include "types.h"
#include "vec.h"
#include "model.h"
// Declared before global.h/mes.h name MesData: uninitialised objects are emitted in first-declaration
// order and the original .bss is cMes, MesFont, MesData, MsgQueue.
class MessageFont;
extern MessageFont MesFont[4];
#include "gx.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "light.h"
#include "mes.h"
#include "dvd.h"
#include "db_log.h"
#include "id_sys.h"
#include "cockpit.h"
#include "pad.h"
#include "trans_ot.h"
#include "view.h"
#include "snd.h"
#include "ref_access.h"
#include <stdio.h>

extern "C" {
u16 getCharCode(u16 code);
int isCtrlCode(u16 code);
void setAttribute(FONT_TEX* t);
void draw(MesQue* q);
void messageCamera();
void messageTrans(MesQue* q);
}

// Dolphin OS ROM font header (only the fields RomFont reads).
struct OSFontHeader {
    u16 fontType;     // 0x00
    u16 firstChar;    // 0x02
    u16 lastChar;     // 0x04
    u16 invalChar;    // 0x06
    u16 ascent;       // 0x08
    u16 descent;      // 0x0A
    u16 width;        // 0x0C
    u16 leading;      // 0x0E
    u16 cellWidth;    // 0x10
    u16 cellHeight;   // 0x12
    u32 sheetSize;    // 0x14
    u16 sheetFormat;  // 0x18
    u16 sheetColumn;  // 0x1A
    u16 sheetRow;     // 0x1C
    u16 sheetWidth;   // 0x1E
    u16 sheetHeight;  // 0x20
    u16 widthTable;   // 0x22
    u32 sheetImage;   // 0x24
    u32 sheetFullSize;  // 0x28
};

// Message slot address as an expression (not an inline call): the multiply lands in the same pseudo
// as the sum, which is what the original codegen shows.
#define MES(no) ((Message*) ((no) * sizeof(Message) + (u32) this + sizeof(u32)))

// Font file: offsets to the TPL and to the width table.
struct MesFontFile {
    u32 tplOfs;    // 0x00
    u32 widthOfs;  // 0x04
};

u32 mes_col_tbl[10] = {
    0xE5D9CFF0, 0x87CFA5FF, 0xCD7D5FFF, 0x87AFFFFF, 0xA55FFFFF,
    0x707070FF, 0x707070FF, 0x52DF73FF, 0x00000000, 0x00000000,
};

MessageControl cMes;
MessageFont MesFont[4];
MessageData MesData;
static MesQue MsgQueue[3][0x100];

// Message text word -> font glyph index (codes 0x80.. are glyphs).
u16 getCharCode(u16 data)
{
    return data - 0x80;
}

// 1 when a message word is a control code (< 0x80).
int isCtrlCode(u16 code)
{
    return code < 0x80;
}

// Debug ROM font drawer over an OSFontHeader (ortho 544x408 projection set up).
RomFont::RomFont(void* font)
{
    Mtx44 proj;
    Mtx m;

    C_MTXOrtho(proj, 0.0f, 408.0f, 0.0f, 544.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXSetZMode(1, 7, 1);
    GXSetNumChans(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 3);
    GXSetTevOrder(0, 0, 0, 0xFF);
    GXSetBlendMode(1, 1, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 3, 0);
    m_FontData = font;
}

#define FONT_HDR ((OSFontHeader*) m_FontData)

// Loads a font sheet as the texture with a 1/sheet-size texture matrix.
void RomFont::setup(void* image)
{
    Mtx m;
    GXTexObj tex;

    GXInitTexObj(&tex, image, FONT_HDR->sheetWidth, FONT_HDR->sheetHeight, FONT_HDR->sheetFormat, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    PSMTXScale(m, 1.0f / (f32) (int) FONT_HDR->sheetWidth, 1.0f / (f32) (int) FONT_HDR->sheetHeight, 1.0f);
    GXLoadTexMtxImm(m, 0x1E, 1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
}

#define WGFIFO_S16(v) (GXWGFifo->s16 = (v))

// Draws one cell of the font sheet (cell at cx, cy) at screen x, y.
void RomFont::draw(int x, int y, int xChar, int yChar)
{
    s16 x0 = x;
    s16 y0 = y;
    s16 u0 = xChar;
    s16 v0 = yChar;
    s16 x1 = x0 + ((OSFontHeader*) m_FontData)->cellWidth;
    s16 y1 = y0 + ((OSFontHeader*) m_FontData)->cellHeight;
    s16 u1 = xChar + ((OSFontHeader*) m_FontData)->cellWidth;
    s16 v1 = yChar + ((OSFontHeader*) m_FontData)->cellHeight;

    GXBegin(0x80, 0, 4);
    WGFIFO_S16(x0);
    WGFIFO_S16(y0);
    WGFIFO_S16(0);
    WGFIFO_S16(u0);
    WGFIFO_S16(v0);
    WGFIFO_S16(x1);
    WGFIFO_S16(y0);
    WGFIFO_S16(0);
    WGFIFO_S16(u1);
    WGFIFO_S16(v0);
    WGFIFO_S16(x1);
    WGFIFO_S16(y1);
    WGFIFO_S16(0);
    WGFIFO_S16(u1);
    WGFIFO_S16(v1);
    WGFIFO_S16(x0);
    WGFIFO_S16(y1);
    WGFIFO_S16(0);
    WGFIFO_S16(u0);
    WGFIFO_S16(v1);
}

// Glyph advance of `code` from the width table (left/right bearings); glyph 0 uses entry 1.
s16 MessageFont::getSize(s16 mes, s8* L, s8* R)
{
    int idx = mes * 2;

    if (mes == 0) {
        idx = 2;
    }
    *L = pWidth[idx];
    *R = pWidth[idx + 1];
    return *R - *L;
}

// Builds a message font from a .fnt file: relocates the TPL once, records each texture sheet
// (FONT_TEX: texture + TLUT), the width table and the cell size w x h.
void MessageFont::create(int char_w, int char_h, TEXPalette* addr, u8* size)
{
    u32 i;
    TEXDescriptor* d;
    FONT_TEX* t;

    m_tpl = addr;
    if ((s32) addr->descriptorArray >= 0) {
        addr->descriptorArray = (TEXDescriptor*) ((u32) addr->descriptorArray + (u32) addr);
        d = addr->descriptorArray;
        for (i = 0; i < addr->numDescriptors; i++, d++) {
            d->textureHeader = (TEXHeader*) ((u8*) addr + (u32) d->textureHeader);
            d->CLUTHeader = (CLUTHeader*) ((u8*) addr + (u32) d->CLUTHeader);
            if (d->textureHeader->unpacked == 0) {
                d->textureHeader->data = (u8*) addr + (u32) d->textureHeader->data;
                d->textureHeader->unpacked = 1;
            }
            if (d->CLUTHeader->unpacked == 0) {
                d->CLUTHeader->data = (u8*) addr + (u32) d->CLUTHeader->data;
                d->CLUTHeader->unpacked = 1;
            }
        }
    }
    d = m_tpl->descriptorArray;
    t = m_mTex;
    for (i = 0; i < m_tpl->numDescriptors; i++, d++, t++) {
        TEXHeader* th = d->textureHeader;
        t->pTpl = addr;
        t->pTex = th;
        if (d->textureHeader->format - 8 <= 1) {
            if (d->textureHeader->unpacked) {
                GXInitTexObjCI(&t->tex, th->data, th->width, th->height, th->format, 0, 0, 0, 0);
            }
            if (d->CLUTHeader->unpacked == 1) {
                CLUTHeader* c = d->CLUTHeader;
                GXInitTlutObj(&t->tlut, c->data, c->format, c->numEntries);
            }
        } else {
            GXInitTexObj(&t->tex, th->data, th->width, th->height, th->format, 0, 0, 0);
        }
        PSMTXScale(t->mtx, 1.0f, 1.0f, 1.0f);
    }
    pWidth = size;
    be_flag = 1;
    t = m_mTex;
    m_char_w = char_w;
    m_char_h = char_h;
    m_tex_w = t->pTex->width;
    m_tex_h = t->pTex->height;
}

// Releases the font (TPL offsets restored so it can be reloaded).
void MessageFont::destroy()
{
    calcTplOffset(m_tpl);
    be_flag = 0;
}

// Message table: u32 header, then per language an offset to a block of message offsets.
struct MesTblBlock {
    u32 x0;
    u32 count;    // 0x04
    u32 ofs[1];   // 0x08
};

// Text of message `no` in message file `type` (0 core, 1 room/event mdt, 2 core, 3 item names,
// 4 ...) for the current language; NULL when out of range.
u16* MessageData::getAddr(int no, int data_type)
{
    u32* tbl = (u32*) ptr[data_type];
    MesTblBlock* blk = (MesTblBlock*) ((u8*) tbl + tbl[lang + 1]);

    if (no > (int) blk->count - 1) {
        return NULL;
    }
    return (u16*) ((u8*) blk + blk->ofs[no]);
}

// Number of messages in file `type` for the current language.
int MessageData::getMesNum(int data_type)
{
    u32* tbl = (u32*) ptr[data_type];
    MesTblBlock* blk = (MesTblBlock*) ((u8*) tbl + tbl[lang + 1]);

    return blk->count;
}

// Width of a space glyph (13 px Japanese, 8 px otherwise).
int MessageData::getSpaceWidth()
{
    if (lang == 0) {
        return 13;
    }
    return 8;
}

// Applies layout preset `layout` (font size, char spacing, line gap; per language) to slot `no`.
void MessageControl::setLayout(int no, int type)
{
    static s8 layout_tbl[2][9][6] = {
        {
            { 0x19, 0x1B, 0x00, 0x01, 0x00, 0x1B },
            { 0x1A, 0x1C, 0x00, 0x01, 0x00, 0x16 },
            { 0x16, 0x18, 0x00, 0x01, 0x00, 0x18 },
            { 0x16, 0x16, 0x00, 0x00, 0x00, 0x16 },
            { 0x18, 0x15, 0x00, 0x00, 0x00, 0x19 },
            { 0x17, 0x17, 0x00, 0x00, 0x00, 0x1C },
            { 0x11, 0x13, 0x00, 0x00, 0x00, 0x00 },
            { 0x16, 0x17, 0x00, 0x01, 0x00, 0x16 },
            { 0x16, 0x15, 0x00, 0x01, 0x00, 0x13 },
        },
        {
            { 0x16, 0x20, -1, -1, 0x00, 0x19 },
            { 0x17, 0x20, 0x00, 0x00, 0x00, 0x19 },
            { 0x12, 0x18, -1, -1, 0x00, 0x13 },
            { 0x15, 0x19, 0x00, 0x00, 0x00, 0x14 },
            { 0x15, 0x1C, 0x00, 0x00, 0x00, 0x19 },
            { 0x17, 0x17, 0x00, 0x00, 0x00, 0x1C },
            { 0x10, 0x15, -1, -1, 0x00, 0x00 },
            { 0x14, 0x19, -1, -1, 0x00, 0x15 },
            { 0x14, 0x19, -1, -1, 0x00, 0x15 },
        },
    };
    static s8* p_layout;

    p_layout = layout_tbl[MesData.lang][type];
    setFontSize(no, p_layout[0], p_layout[1]);
    U16Set(MES(no)->m_char_gap, p_layout[3]);
    MES(no)->m_line_gap = p_layout[5];
}

// Selects the language block of the message files (0 Japanese, 1 English, 2..5 French/German/
// Spanish/Italian; unknown -> 1).
void MessageControl::setLanguage(int lang)
{
    switch (lang) {
    case 0:
        MesData.lang = lang;
        break;
    case 1:
        MesData.lang = lang;
        break;
    case 3:
        MesData.lang = 2;
        break;
    case 4:
        MesData.lang = 3;
        break;
    case 5:
        MesData.lang = 4;
        break;
    case 6:
        MesData.lang = 5;
        break;
    case 2:
        MesData.lang = 1;
        break;
    default:
        pLog->err(0, 0, "MesCtrl::setLanguage() Invalid LANG_TYPE");
        MesData.lang = 1;
        break;
    }
}

// Creates font slot `no` from a loaded .fnt buffer (TPL and width table offsets).
void MessageControl::setupFont(int char_w, int char_h, TEXPalette* addr, int no)
{
    MesFontFile* f = (MesFontFile*) addr;

    m_font_addr[no] = addr;
    MesFont[no].create(char_w, char_h, (TEXPalette*) ((u8*) f + f->tplOfs), (u8*) f + f->widthOfs);
}

// Destroys font slot `no`.
void MessageControl::releaseFont(int no)
{
    MesFont[no].destroy();
}

// Loads a .fnt from disc into m_font_addr[no] and creates the font; 0 on read failure.
#line 703 "D:/Bio4/Prog/mes.cpp"
int MessageControl::loadFont(int w, int h, const char* name, int no)
{
    int req = DvdReadN(name, m_font_addr[no], 0, 0, 0, 0x11, __FILE__, __LINE__);

    if (Dvd.ReadCheck(req, 0, 0, 0) != 1) {
        pLog->err(0, 0, "MesCtrl::fontLoad() Font load failed");
        return 0;
    }
    setupFont(w, h, (TEXPalette*) m_font_addr[no], no);
    return 1;
}

// Boot: allocates the common and system font buffers by file size, loads them, sets the language,
// binds the three 0x100-entry glyph queues to slots 0..2 and clears the state.
void MessageControl::init()
{
    u32 size;
    u32 sz = 0;
    int i;
    Message* m;

    if (Dvd.FileExistCheck("Font/common_j.fnt", &size) != -1) {
        sz = size;
    }
    if (sz != 0) {
#line 734 "D:/Bio4/Prog/mes.cpp"
        m_font_addr[0] = mem_alloc(sz, __FILE__, __LINE__, 1, 0xD);
    } else {
        pLog->err(0, 0, "MesCtrl::init() Font file not found.");
    }
    if (Dvd.FileExistCheck("Font/system_j.fnt", &size) != -1) {
        sz = size;
    }
    if (sz != 0) {
#line 748 "D:/Bio4/Prog/mes.cpp"
        m_font_addr[1] = mem_alloc(sz, __FILE__, __LINE__, 1, 0xD);
    } else {
        pLog->err(0, 0, "MesCtrl::init() Font file not found.");
    }
    loadCommonFont();
    loadSystemFont();
    setLanguage(pSys->language);
    x11F8 = 0;
    m_state = 0;
    m = m_Msg;
    for (i = 0; i < 16; m++, i++) {
        if (i <= 2) {
            m->qbase = MsgQueue[i];
        } else {
            m->qbase = NULL;
        }
    }
}

// Game start: binds the message files (core text for types 0..2, item names for 3), reloads the
// common font, resets the state.
void MessageControl::gameInit()
{
    MesData.setPtr(0, (u8*) (pG->pCore->ofs_28 + (u32) pG->pCore));
    MesData.setPtr(1, (u8*) (pG->pCore->ofs_28 + (u32) pG->pCore));
    MesData.setPtr(2, (u8*) (pG->pCore->ofs_28 + (u32) pG->pCore));
    MesData.setPtr(3, (u8*) (pG->pCore->ofs_54 + (u32) pG->pCore));
    pG->IsMessageInit = 1;
    loadCommonFont();
    setLanguage(pSys->language);
    x11F8 = 0;
    m_state = 0;
}

// Room init: deletes all 16 slots, binds the room text (RoomMes) as type 1, default layout, and
// reloads the stage font when the event font was loaded.
void MessageControl::roomInit()
{
    int i;

    for (i = 0; i < 16; i++) {
        Delete(i);
    }
    MesData.setPtr(0, (u8*) (pG->pCore->ofs_28 + (u32) pG->pCore));
    MesData.setPtr(1, (u8*) pG->RoomMes);
    setLayout(0, 0);
    if (checkState(1)) {
        loadStageFont();
    }
}

// Loads Font/common_j.fnt (28 px) or common_p.fnt (32 px) into slot 0.
void MessageControl::loadCommonFont()
{
    if (pSys->language == 0) {
        loadFont(0x1C, 0x1C, "Font/common_j.fnt", 0);
    } else {
        loadFont(0x20, 0x20, "Font/common_p.fnt", 0);
    }
}

// Loads Font/system_j.fnt (20 px) into slot 1 (Japanese); other languages reuse the common font.
void MessageControl::loadSystemFont()
{
    if (pSys->language == 0) {
        loadFont(0x14, 0x14, "Font/system_j.fnt", 1);
    } else {
        setupFont(0x20, 0x20, (TEXPalette*) m_font_addr[0], 1);
    }
}

// Stage start: allocates the stage font buffer (largest of stageN/eventN/stage1 .fnt) and loads
// the stage font.
void MessageControl::stageInit()
{
    char name[0x100];
    u32 size;
    u32 sz = 0;

    if (pG->stage_no != 0) {
        sprintf(name, "Font/stage%1d_j.fnt", pG->stage_no);
        if (Dvd.FileExistCheck(name, &size) != -1 && size > sz) {
            sz = size;
        }
        sprintf(name, "Font/event%1d_j.fnt", pG->stage_no);
        if (Dvd.FileExistCheck(name, &size) != -1 && size > sz) {
            sz = size;
        }
    } else {
        if (Dvd.FileExistCheck("Font/stage1_j.fnt", &size) != -1 && size > sz) {
            sz = size;
        }
    }
    if (sz != 0) {
#line 874 "D:/Bio4/Prog/mes.cpp"
        (pG->pStFnt = mem_alloc(sz, __FILE__, __LINE__, 1, 0xD));
        m_font_addr[2] = pG->pStFnt;
    } else {
        pLog->err(0, 0, "MesCtrl::init() Font file not found.");
    }
    loadStageFont();
}

// Japanese: loads Font/stageN_j.fnt into slot 2 and clears state bit 0 (event font loaded).
void MessageControl::loadStageFont()
{
    char name[0x100];

    if (pSys->language == 0 && pG->stage_no != 0) {
        sprintf(name, "Font/stage%1d_j.fnt", pG->stage_no);
        loadFont(0x1C, 0x1C, name, 2);
        unsetState(1);
    }
}

// Japanese: loads Font/eventN_j.fnt into slot 2 and sets state bit 0.
void MessageControl::loadEventFont()
{
    char name[0x100];

    if (pSys->language == 0 && pG->stage_no != 0) {
        sprintf(name, "Font/event%1d_j.fnt", pG->stage_no);
        loadFont(0x1C, 0x1C, name, 2);
        setState(1);
    }
}

// Sets state bits.
void MessageControl::setState(u32 b)
{
    m_state |= b;
}

// Clears state bits.
void MessageControl::unsetState(u32 b)
{
    m_state &= ~b;
}

// Tests state bits.
int MessageControl::checkState(u32 b)
{
    return (m_state & b) ? 1 : 0;
}

// Per-frame: runs slot 15 (the system/pause message) first, then every active slot 0..15.
void MessageControl::Move()
{
    Message* m = &m_Msg[15];
    Message* p = &m_Msg[0];
    int act = 0;
    int i;

    if (m->isAlive()) {
        act = 1;
    }
    if (act) {
        m->move();
    } else {
        for (i = 0; i < 16; i++, p++) {
            int a = 0;
            if (p->isAlive()) {
                a = 1;
            }
            if (a) {
                p->move();
            }
        }
    }
}

// Per-frame draw of every active slot (slot 15 first) while the HUD is on (Disp_flg 0x800).
void MessageControl::Trans()
{
    Message* p = &m_Msg[0];
    Message* m;
    int act;
    int i;

    if (DpfFlagChk(pG, DPF_MESSAGE)) {
        return;
    }
    m = &m_Msg[15];
    act = 0;
    if (m->isAlive()) {
        act = 1;
    }
    if (act) {
        m->trans();
    } else {
        for (i = 0; i < 16; i++, p++) {
            int a = 0;
            if (p->isAlive()) {
                a = 1;
            }
            if (a) {
                p->trans();
            }
        }
    }
}

// Sets the glyph draw size of slot `no`.
void MessageControl::setFontSize(int no, s16 font_w, s16 font_h)
{
    Message* m = getMes(no);
    m->m_font_w = font_w;
    m->m_font_h = font_h;
}

// Starts message `no` in slot `slot` at (x, y): the font by `type` (0 common, 2 stage/event, 3
// item names; attr bit 0 = core text; slot 15 uses the system font), colour mes_col_tbl[col]
// and attribute bits (0x80 = no wait/stop, 0x10 = do not stop the game, 0x40 = instant, 0x20 = OT
// draw, alignment bits 0x20000/0x80000/0x10000, ...). Unless attr 0x80, saves Stop_flg and
// stops the game and input while the message runs.
void MessageControl::MesSet(int no, int px, int py, u32 attr, int wk, int col, int font_no)
{
    MessageFont* font;
    Message* m;
    int ok;

    if (pSys->language == 0) {
        if (font_no == 4) {
            if (attr & 1) {
                font = &MesFont[0];
            } else if (attr & 2) {
                font = &MesFont[2];
            } else if (attr & 4) {
                font = &MesFont[3];
            } else {
                font = &MesFont[0];
            }
        } else {
            font = &MesFont[font_no];
        }
    } else {
        font = &MesFont[0];
    }
    if (wk == 15) {
        font = &MesFont[1];
    }
    ok = 0;
    if (font->be_flag & 1) {
        ok = 1;
    }
    if (!ok) {
        pLog->err(0, 0, "MesSet(): Font not found", 0);
        return;
    }
    m = &m_Msg[wk];
    m->init(no, px, py, attr, col, font);
    m->m_scale_w = (f32) m->m_font_w / (f32) font->m_char_w;
    m->m_scale_h = (f32) m->m_font_h / (f32) font->m_char_h;
    if (!(attr & 0x80)) {
        m->stop_bak = pG->Stop_flg;
        if (!(attr & 0x10)) {
            pG->Stop_flg = 0xFFFFFFFF;
            SpfFlagOff(pG, SPF_ID_SYSTEM);
            KeyStop(0xEFCF0000);
        }
    }
}

// Kills slot `no`.
void MessageControl::Delete(int no)
{
    if (no > 15) {
        return;
    }
    MES(no)->setDie();
    m_Msg[no].m_state &= ~1;
}

// Requests slot `no` to finish its current wait.
void MessageControl::WaitEnd(int no)
{
    if (no > 15) {
        return;
    }
    m_Msg[no].WaitEnd();
}

// Resets a slot for message `no`: routine numbers, position, colour, speed (attr 0x40 = 0 frames
// per char), text pointer from the file selected by attr bits 0/1/2/3 (falls back to message 0
// with an error), OT type 0x15.
void Message::init(int no, int px, int py, u32 attr, int col, MessageFont* pFont)
{
    int i;

    m_pFont = pFont;
    setBorn();
    m_state = (m_state & ~2) | 1;
    r_no_3 = 0;
    r_no_2 = 0;
    r_no_1 = 0;
    r_no_0 = 0;
    m_cur = 0;
    m_sel = 0;
    m_selTbl_size = 0;
    for (i = 15; i >= 0; i--) {
        m_pos0_x[i] = px;
    }
    this->m_pos_x = px;
    this->m_pos_y = py;
    m_pos0_y = py;
    m_col = mes_col_tbl[col];
    qp = qbase;
    this->m_attr = attr;
    m_wait_cnt = 0;
    m_bttn_wait = 0;
    m_sel = 0;
    m_pRetAddr = NULL;
    m_pRetFont = NULL;
    m_jump_max = 0;
    m_scale_w = 1.0f;
    m_jump_idx = 0;
    m_evt_no = -1;
    m_scale_h = 1.0f;
    if (attr & 0x40) {
        m_spd = 0;
    } else {
        m_spd = 1;
    }
    m_spd_cnt = 0;
    m_spd_flag = 0;
    if (attr & 1) {
        m_pMes = MesData.getAddr(no, 0);
    } else if (attr & 2) {
        m_pMes = MesData.getAddr(no, 1);
    } else if (attr & 4) {
        m_pMes = MesData.getAddr(no, 2);
    } else if (attr & 8) {
        m_pMes = MesData.getAddr(no, 3);
    } else if (attr & 0x10) {
        m_pMes = MesData.getAddr(no, 4);
    }
    if (m_pMes == NULL) {
        m_pMes = MesData.getAddr(0, 0);
        pLog->err(0, 0, "Message::init() Msg[%02d] Address Error", no);
    }
    m_ot_type = 0x15;
    m_ot_no = 1;
}

// Per-frame text engine: A/B speeds the text up; executes control codes (CommandExec: 2 = wait
// this frame) and queues one glyph per m_spd frames into the glyph queue (space = getSpaceWidth).
void Message::move()
{
    int ret;
    int code;

    if (!(be_flag & 2) && attrCk(0x80)) {
        be_flag &= ~1;
        m_state &= ~1;
    }
    be_flag &= ~2;
    if (Key.trg & 0xC0000000) {
        m_spd_flag = 1;
    }
    for (;;) {
        if (isCtrlCode(*m_pMes)) {
            ret = CommandExec();
            if (ret == 2 && *m_pMes != 0xA) {
                break;
            }
        } else {
            if (qp != NULL && qp >= qbase + 0x100) {
                pLog->err(0, 0, "Message [%d]: Overflow!", 0);
                return;
            }
            if (!attrCk(0xC0) && m_spd_flag == 0) {
                if (m_spd_cnt++ < m_spd) {
                    return;
                }
            }
            code = getCharCode(*m_pMes);
            asm("" : : "r"(code));  // COMPILER-DIFF: candidate (combine: the original keeps the call-result copy and `cmpwi` apart, ours fuses them into `mr.`)
            if (code == 0) {
                m_pos_x += (s16) ((f32) MesData.getSpaceWidth() * m_scale_w);
            } else {
                QueSet(code, NULL);
            }
            m_spd_cnt = 0;
        }
        m_pMes++;
    }
}

// Pre-pass over the message (m_state bit 3): measures each line's width (glyphs, numbers,
// spaces), then computes the line start x positions from the alignment attributes (0x20000 left,
// 0x80000 per-line centre, else centre on the widest; right-aligned when set) and the vertical
// centring (0x10000), and rewinds.
void Message::WidthCk()
{
    int n = 0;
    int i;
    s8 l, r;
    u16* save;
    u16 code;

    qp = qbase;
    m_pos_y = m_pos0_y;
    m_state |= 8;
    save = m_pMes;
    m_width_max = 0;
    m_number_width = 0;
    for (i = 15; i >= 0; i--) {
        m_width[i] = 0;
    }
    while (m_state & 8) {
        if (isCtrlCode(*m_pMes)) {
            switch (*m_pMes) {
            case 3:
                n++;
                break;
            case 1:
            case 4:
                if ((s16) m_width[n] > 0) {
                    n++;
                }
                m_state &= ~8;
                break;
            case 2:
                CommandExec();
                break;
            case 7:
                CommandExec();
                m_spd = m_spd_old;
                break;
            case 0xA:
                if (CommandExec() != 0) {
                    m_width[n] += m_number_width;
                }
                break;
            case 0xE:
                if (CommandExec() != 2) {
                    break;
                }
                m_state &= ~8;
                // falls through into case 0xF (the original has no break here)
            case 0xF:
                CommandExec();
                break;
            case 0x10:
                CommandExec();
                break;
            case 0x11:
                CommandExec();
                break;
            default:
                m_pMes += CommandArg();
                break;
            }
            if (n > 15) {
                pLog->err(0, 0, "Message [%d]: line overflow!!", 0);
            }
        } else {
            if (qp != NULL && qp >= qbase + 0x100) {
                pLog->err(0, 0, "Message [%d]: queue overflow!!", 0);
                break;
            }
            code = getCharCode(*m_pMes);
            asm("" : "+r"(code));  // COMPILER-DIFF: candidate (combine: the original keeps the call-result copy and `cmpwi` apart, ours fuses them into `mr.`)
            if (code != 0) {
                s16 w = m_pFont->getSize(code, &l, &r);
                int a = (s16) ((f32) w * m_scale_w) + m_char_gap;
                m_width[n] += a;
            } else {
                m_width[n] += (s16) ((f32) MesData.getSpaceWidth() * m_scale_w);
            }
        }
        m_pMes++;
    }
    m_width_max = 0;
    for (i = 0; i < n; i++) {
        if ((s16) m_width[i] > m_width_max) {
            m_width_max = m_width[i];
        }
    }
    if (!(m_attr & 0x20000)) {
        for (i = 0; i < n; i++) {
            if (m_attr & 0x80000) {
                m_pos0_x[i] = (0x200 - (s16) m_width[i]) >> 1;
            } else if (m_attr & 0x40000) {
                int w = 0x200 - m_width[i];
                m_pos0_x[i] = w - (0x200 - m_width_max) / 2;
            } else {
                m_pos0_x[i] = (0x200 - m_width_max) >> 1;
            }
        }
    } else if (m_attr & 0x40000) {
        for (i = 0; i < n; i++) {
            m_pos0_x[i] -= m_width[i];
        }
    }
    if (pSys->language == 0 && (m_attr & 0x1000) && n == 1) {
        m_pos_y += m_line_gap;
    }
    if (m_attr & 0x10000) {
        m_pos_y = (0x180 - n * (s16) m_line_gap) >> 1;
    }
    m_pMes = save;
    qp = qbase;
    m_jump_idx = 0;
}

// Emits glyph `code` at the pen position: appended to the glyph queue (drawn by trans), or drawn
// immediately when the slot has no queue; advances the pen by the glyph width + m_char_gap.
void Message::QueSet(int code, MessageFont* p_font)
{
    s8 l, r;
    s16 w, h;

    if (p_font == NULL) {
        p_font = m_pFont;
    }
    w = (s16) ((f32) p_font->getSize(code, &l, &r) * m_scale_w);
    h = (s16) ((f32) (int) p_font->m_char_h * m_scale_h);
    if (!(m_state & 8)) {
        if (qp != NULL) {
            qp->x = m_pos_x;
            qp->y = m_pos_y;
            qp->color = m_col;
            qp->code = code;
            qp->w = w;
            qp->h = h;
            qp->font = p_font;
            qp++;
        } else {
            MesQue q;
            q.x = m_pos_x;
            q.y = m_pos_y;
            q.color = m_col;
            q.code = code;
            q.h = h;
            q.font = p_font;
            q.w = w;
            messageTrans(&q);
        }
    }
    m_pos_x += w + (s16) m_char_gap;
}

// Sets the number shown by control code 0x0A (`digits` 0 = as many as needed).
void Message::setNumber(u32 num, u16 digits)
{
    s16 i;
    int d;
    int t;

    numberSave = num;
    m_number = num;
    if (digits == 0 && num != 0) {
        do {
            num /= 10;
            digits++;
        } while (num != 0);
    }
    digit = 1;
    i = 1;
    if (i < digits) {
        d = 1;
        do {
            t = (u16) d * 10;
            d = t;
            i++;
        } while (i < digits);
        digit = t;
    }
    digitSave = digit;
}

// Shows the cursor glyph at the selected choice (selCur queue entries, code 1 = on).
void Message::putSelCursol()
{
    int i;

    for (i = 0; i < m_selTbl_size; i++) {
        selCur[i]->code = (m_cur == i);
    }
}

// Blinks the "next page" cursor (timer 1..30; reset restarts at 20).
void Message::putNextCursol(int flag)
{
    if (flag == 0) {
        m_cursol_time = 0x14;
    }
    m_cursol_time++;
    if (m_cursol_time > 0x1E) {
        m_cursol_time = 1;
    }
}

// Adds a message number to the jump table used by control codes 02/0F/11 with argument 0xFFFF (max 3).
void Message::setJump(u16 mes)
{
    if (m_jump_max <= 2) {
        m_jump_mes[m_jump_max] = mes;
        m_jump_max++;
    } else {
        pLog->warn(0, 0, "JumpTbl is Max!");
    }
}

// Loads a font sheet texture (with TLUT for CI formats) into texture map 0.
void setAttribute(FONT_TEX* t)
{
    GXSetCullMode(0);
    GXSetZMode(0, 3, 1);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetTevOp(0, 0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetChanCtrl(4, 0, 1, 1, 0, 0, 2);
    if (t->pTex->format - 8 <= 1) {
        GXLoadTlut(&t->tlut, 0);
    }
    GXLoadTexObj(&t->tex, 0);
    GXLoadTexMtxImm(t->mtx, 0x1E, 1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
    GXSetBlendMode(1, 4, 5, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 0, 3, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
}

// Draws one queued glyph: cell (code % cols, code / cols) of the font sheet, colour from the queue,
// at (x, y) with size w x h, then restores the fog.
void draw(MesQue* q)
{
    MessageFont* font = q->font;
    s8 l, r;
    GXColor fog;
    f32 texH = (f32) font->m_tex_h;
    f32 texW = (f32) font->m_tex_w;
    u32 col = q->color;
    u8 ca = col & 0xFF;
    u8 cb = (col >> 8) & 0xFF;
    u8 cg = (col >> 16) & 0xFF;
    u8 cr = col >> 24;
    s16 x = q->x;
    s16 y = q->y;
    u8 w = q->w;
    u8 h = q->h;
    FONT_TEX* t = &font->m_mTex[0];
    s16 cw;
    int cols, rows;
    s16 u, v;
    s16 x1, y1;
    u8 cellW, cellH;

    font->getSize(q->code, &l, &r);
    cellW = font->m_char_w;
    cols = font->m_tex_w / cellW;
    cw = r - l;
    cellH = font->m_char_h;
    rows = font->m_tex_h / cellH;
    u = (q->code % cols) * cellW;
    v = (q->code / cols) * cellH;
    while (v >= rows * cellW) {
        v -= rows * cellH;
    }
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    setAttribute(t);
    GXBegin(0x80, 0, 4);
    u += l;
    x1 = x + w;
    y1 = y + h;
    GXWGFifo->s16 = x;
    GXWGFifo->s16 = y;
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2f32((f32) u / texW, (f32) v / texH);
    GXWGFifo->s16 = x1;
    GXWGFifo->s16 = y;
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2f32((f32) (u + cw) / texW, (f32) v / texH);
    GXWGFifo->s16 = x1;
    GXWGFifo->s16 = y1;
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2f32((f32) (u + cw) / texW, (f32) (v + cellH) / texH);
    GXWGFifo->s16 = x;
    GXWGFifo->s16 = y1;
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2f32((f32) u / texW, (f32) (v + cellH) / texH);
    LightMgr.setFog();
}

// Screen-space ortho projection (512 x 384) for message glyphs.
void messageCamera()
{
    Mtx44 proj;
    Mtx m;

    C_MTXOrtho(proj, 0.0f, 384.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
}

// OT callback / direct draw of one glyph.
void messageTrans(MesQue* p_que)
{
    messageCamera();
    draw(p_que);
}

// Draws the slot's glyph queue, through the OT (attr 0x20) or directly.
void Message::trans()
{
    MesQue* q;

    for (q = qbase; q < qp; q++) {
        if (attrCk(0x20)) {
            AddOtDirect(m_ot_type, q, (void (*)()) messageTrans, m_ot_no, 0x1000, NULL, 0.0f);
        } else {
            messageTrans(q);
        }
    }
}

// Dispatches the control code at the text pointer to code00..code12; returns 0 continue, 2 wait,
// 3 unknown.
int Message::CommandExec()
{
    switch (*m_pMes) {
    case 0x00:
        return code00();
    case 0x01:
        return code01();
    case 0x02:
        return code02();
    case 0x03:
        return code03();
    case 0x04:
        return code04();
    case 0x05:
        return code05();
    case 0x06:
        return code06();
    case 0x07:
        return code07();
    case 0x08:
        return code08();
    case 0x09:
        return code09();
    case 0x0A:
        return code0a();
    case 0x0B:
        return code0b();
    case 0x0C:
        return code0c();
    case 0x0D:
        return code0d();
    case 0x0E:
        return code0e();
    case 0x0F:
        return code0f();
    case 0x10:
        return code10();
    case 0x11:
        return code11();
    case 0x12:
        return code12();
    }
    return 3;
}

// Number of argument words following the current control code.
int Message::CommandArg()
{
    static const s16 arg[19] = { 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1 };

    return arg[*m_pMes];
}

// Ends the current 0x09 wait.
void Message::WaitEnd()
{
    m_wait_cnt = 1;
}

// Code 00 (page start): clears the glyph queue, measures the page (WidthCk) and resets the pen,
// line, speed, choice and cursor state.
int Message::code00()
{
    if (qbase != NULL) {
        qp = qbase;
        memclr_asm(qbase, 0x1000);
    } else {
        qp = NULL;
    }
    WidthCk();
    m_pos_x = m_pos0_x[0];
    m_lines = 0;
    m_spd_cnt = 0;
    m_wait_cnt = 0;
    m_selTbl_size = 0;
    m_spd_old = 0;
    m_btn = 0;
    m_cursol_time = 0;
    m_spd_flag = 0;
    return 0;
}

// Code 01 (end): marks the message finished (m_state bit 1); unless attr 0x01000000 (keep
// displayed) frees the slot, restores Stop_flg (unless attr 0x10) and the life meter.
int Message::code01()
{
    switch (r_no_0) {
    case 0:
        r_no_0++;
        break;
    case 1:
        m_state |= 2;
        if (!attrCk(0x01000000)) {
            m_state &= ~1;
            setDie();
            if (!attrCk(0x10)) {
                pG->Stop_flg = stop_bak;
            }
        }
        if (IdSys.setCk(IDC_LIFE_METER) && !StaFlagChk(pG, STA_SUB_SCRN)) {
            Cckpt.lifeMeterDisp(1);
        }
        break;
    }
    return 2;
}

// Code 02 (call): saves the return address and jumps to message arg (0xFFFF = next jump table
// entry) in the same file type.
int Message::code02()
{
    u16 no;

    m_pMes++;
    no = *m_pMes;
    if (no == 0xFFFF) {
        no = m_jump_mes[m_jump_idx++];
        if (no == 0xFFFF) {
            return 0;
        }
    }
    m_pRetAddr = m_pMes;
    m_pRetFont = m_pFont;
    if (m_attr & 1) {
        m_pMes = MesData.getAddr(no, 0);
    } else if (m_attr & 2) {
        m_pMes = MesData.getAddr(no, 1);
    } else if (m_attr & 4) {
        m_pMes = MesData.getAddr(no, 2);
    } else if (m_attr & 8) {
        m_pMes = MesData.getAddr(no, 3);
    }
    return 0;
}

// Code 03 (new line): next line start.
int Message::code03()
{
    m_pos_y += m_line_gap;
    m_lines++;
    m_pos_x = m_pos0_x[m_lines];
    return 0;
}

// Code 04 (new page): restarts the page (code00); ignored in no-wait mode.
int Message::code04()
{
    if (attrCk(0x80)) {
        return 0;
    }
    m_pMes++;
    code00();
    return 2;
}

// Code 05 (speed): frames per glyph = arg.
int Message::code05()
{
    if (attrCk(0x80)) {
        return 0;
    }
    m_pMes++;
    m_spd = *m_pMes;
    m_spd_cnt = 0;
    return 0;
}

// Code 06 (colour): colour = mes_col_tbl[arg].
int Message::code06()
{
    m_pMes++;
    m_col = mes_col_tbl[*m_pMes];
    return 0;
}

// Code 07 (choice): records a cursor slot for a selectable line (text speed forced instant).
int Message::code07()
{
    if (m_spd_old == 0) {
        m_spd_old = m_spd;
    }
    m_spd = 0;
    selCur[m_selTbl_size++] = qp;
    QueSet(0, NULL);
    return 0;
}

// Code 08 (wait for input): with choices, up/down (or left/right with attr 0x00100000) move the
// cursor with sounds, A confirms (m_sel = choice + 1), B cancels (m_sel = -1 or the last choice
// with 0x00200000); without choices A/B continues; the blinking next cursor is shown.
int Message::code08()
{
    int ret = 2;

    if (attrCk(0x02000000)) {
        return 0;
    }
    if (m_btn == 0) {
        if (attrCk(0x00400000)) {
            m_cur = m_selTbl_size - 1;
            putSelCursol();
        }
        m_btn = 1;
        return 2;
    }
    if (m_bttn_wait != 0) {
        m_bttn_wait--;
        return 2;
    }
    if (m_selTbl_size != 0) {
        if (Key.trg & 0x80000000) {
            ret = 0;
            m_sel = m_cur + 1;
            if (m_sel == 1) {
                if (attrCk(0x100)) {
                    SndCall(0, 0xF, NULL, 0, 0, NULL);
                }
                if (attrCk(0x200)) {
                    SndCall(0, 0x11, NULL, 0, 0, NULL);
                }
                if (attrCk(0x400)) {
                    SndCall(0, 0x13, NULL, 0, 0, NULL);
                }
            } else if (m_sel == 3) {
                if (attrCk(0x400)) {
                    SndCall(0, 0x12, NULL, 0, 0, NULL);
                }
            }
        } else if (Key.trg & 0x40000000) {
            if (attrCk(0x00100000)) {
                if (attrCk(0x00200000)) {
                    m_sel = m_selTbl_size;
                    ret = 0;
                } else {
                    m_sel = -1;
                    ret = 0;
                }
            } else if (attrCk(0x00200000)) {
                m_cur = m_selTbl_size - 1;
            }
        } else if (attrCk(0x00800000)) {
            s8 old = m_cur;
            if (Key.trg & 0x01000000) {
                m_cur--;
                if (m_cur < 0) {
                    m_cur = m_selTbl_size - 1;
                }
            } else if (Key.trg & 0x02000000) {
                m_cur++;
                if (m_cur >= m_selTbl_size) {
                    m_cur = 0;
                }
            }
            if (old != m_cur) {
                if (attrCk(0x100)) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else if (attrCk(0x200)) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else if (attrCk(0x400)) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else {
                    SndCall(0, 0xA, NULL, 0, 0, NULL);
                }
            }
        } else {
            s8 old = m_cur;
            if (Key.trg & 0x08000000) {
                m_cur--;
                if (m_cur < 0) {
                    m_cur = m_selTbl_size - 1;
                }
            } else if (Key.trg & 0x04000000) {
                m_cur++;
                if (m_cur >= m_selTbl_size) {
                    m_cur = 0;
                }
            }
            if (old != m_cur) {
                if (attrCk(0x100)) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else if (attrCk(0x200)) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else if (attrCk(0x400)) {
                    SndCall(0, 0xE, NULL, 0, 0, NULL);
                } else {
                    SndCall(0, 0xA, NULL, 0, 0, NULL);
                }
            }
        }
        putSelCursol();
    } else {
        putNextCursol(1);
        if (Key.trg & 0xC0000000) {
            ret = 0;
            putNextCursol(0);
        }
    }
    return ret;
}

// Code 09 (timed wait): waits arg frames (skipped in no-wait mode).
int Message::code09()
{
    int ret = 2;

    if (attrCk(0x80)) {
        return 0;
    }
    if (m_wait_cnt == 0) {
        m_wait_cnt = m_pMes[1];
    } else if (m_wait_cnt == -1) {
        return 2;
    } else {
        m_wait_cnt--;
        if (m_wait_cnt == 0) {
            m_pMes++;
            ret = 0;
        }
    }
    return ret;
}

// Code 0A (number): emits the digits of m_number (set by setNumber) one per frame with the digit
// glyphs of the current font (Japanese: system-font digits, optional "%" glyph with attr 0x10000000).
int Message::code0a()
{
    u32 d;
    u16 code;
    MessageFont* fnt;
    s8 l, r;
    s16 w;
    int a;

    if (qp >= qbase + 0x100) {
        return 2;
    }
    d = m_number / digit;
    if (d > 9) {
        d = 0;
    }
    if (MesData.lang == 0) {
        if (attrCk(0x10000000)) {
            code = d + 0xE;
        } else {
            code = d + 3;
        }
        if (m_pFont == &MesFont[1]) {
            fnt = &MesFont[1];
        } else {
            fnt = &MesFont[0];
        }
    } else {
        code = d + 3;
        fnt = m_pFont;
    }
    w = fnt->getSize(code, &l, &r);
    a = (s16) ((f32) w * m_scale_w) + m_char_gap;
    m_number_width += a;
    QueSet(code, fnt);
    m_number -= d * digit;
    digit /= 10;
    if (digit == 0) {
        m_number = numberSave;
        digit = digitSave;
        if (attrCk(0x10000000)) {
            if (MesData.lang == 0) {
                code = 0xD;
            } else {
                code = 0xAC;
            }
            w = fnt->getSize(code, &l, &r);
            a = (s16) ((f32) w * m_scale_w) + m_char_gap;
            m_number_width += a;
            QueSet(code, fnt);
        }
        return 2;
    }
    m_pMes--;
    return 0;
}

// Code 0B: pen x = arg.
int Message::code0b()
{
    m_pMes++;
    m_pos_x = *m_pMes;
    return 0;
}

// Code 0C: pen y = arg.
int Message::code0c()
{
    m_pMes++;
    m_pos_y = *m_pMes;
    return 0;
}

// Code 0D: sets m_evt_no (event/scenario hook number read by the caller).
int Message::code0d()
{
    m_pMes++;
    m_evt_no = *m_pMes;
    return 0;
}

// Code 0E (return): back to the caller of code02/0F/10/11.
int Message::code0e()
{
    if (m_pRetAddr != NULL) {
        m_pMes = m_pRetAddr;
        m_pFont = m_pRetFont;
        m_pRetAddr = NULL;
        m_pRetFont = NULL;
        return 0;
    }
    return 2;
}

// Code 0F (call core text): saves the return and jumps to core message arg with the common font.
int Message::code0f()
{
    u16 no;

    m_pMes++;
    no = *m_pMes;
    if (no != 0xFFFF || (no = m_jump_mes[m_jump_idx++]) != 0xFFFF) {
        m_pRetAddr = m_pMes;
        m_pRetFont = m_pFont;
        m_pMes = MesData.getAddr(no, 0);
        m_pFont = &MesFont[0];
    }
    return 0;
}

// Code 10 (item name): inserts the item-name message m_item_no (file type 3).
int Message::code10()
{
    m_pRetAddr = m_pMes;
    m_pRetFont = m_pFont;
    m_pMes = MesData.getAddr(m_item_no, 3);
    return 0;
}

// Code 11 (call item text): jumps to item-name message arg (0xFFFF = jump table) with the common
// font in Japanese.
int Message::code11()
{
    u16 no;

    m_pMes++;
    no = *m_pMes;
    if (no == 0xFFFF) {
        no = m_jump_mes[m_jump_idx++];
        if (no == 0xFFFF) {
            return 0;
        }
    }
    m_pRetAddr = m_pMes;
    m_pRetFont = m_pFont;
    m_pMes = MesData.getAddr(no, 3);
    if (MesData.lang == 0) {
        m_pFont = &MesFont[0];
    }
    return 0;
}

// Code 12 (speaker): sets m_who (speaker id for the caller).
int Message::code12()
{
    m_pMes++;
    m_who = *m_pMes;
    return 0;
}
