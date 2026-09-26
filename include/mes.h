#ifndef MES_H
#define MES_H

#include "types.h"
#include "vec.h"
#include "gx.h"
#include "tpl.h"

// game/mes.cpp: in-game message system (fonts, message queues, control codes).

// Language table (MesData language): 0 JP, 1 EN, 2 DE, 3 FR, 4 ES, 5 IT.
struct MessageData {
private:
    u32 m_language;  // 0x00

public:
    // 0x04  message tables (type 0..4), each: u32 x0, u32 ofs[lang]. Public only for
    // cDvd::ErrCheck, which holds a pointer to the array across its wait loop.
    u8* m_Data[5];

    void setLanguage(u32 lang) { m_language = lang; }
    u32 getLanguage() { return m_language; }
    void registData(int type, u8* p) { m_Data[type] = p; }
    u16* getAddr(int no, int data_type);
    int getMesNum(int data_type);
    int getSpaceWidth();
};

// One queued glyph (MsgQueue entries, 0x10 bytes).
class MessageFont;
struct MesQue {
    u16 x;              // 0x00
    u16 y;              // 0x02
    u16 code;           // 0x04
    u8 w;               // 0x06
    u8 h;               // 0x07
    u32 color;          // 0x08
    MessageFont* font;  // 0x0C
};

// One texture sheet of a font (0x64 bytes).
struct FONT_TEX {
    GXTexObj tex;       // 0x00
    GXTlutObj tlut;     // 0x20
    Mtx mtx;            // 0x2C
    TEXHeader* pTex;    // 0x5C
    TEXPalette* pTpl;   // 0x60
};

// Font (MesFont[4], 0xDC bytes each): a TPL with up to two sheets and a glyph width table.
class MessageFont {
public:
    u32 be_flag;          // 0x00  bit 0 = loaded
    TEXPalette* m_tpl;   // 0x04
    FONT_TEX m_mTex[2];  // 0x08
    u8* pWidth;         // 0xD0  per glyph: left, right (s8 pairs)
    u16 m_tex_w;           // 0xD4  sheet 0 width
    u16 m_tex_h;           // 0xD6  sheet 0 height
    u8 m_char_w;           // 0xD8
    u8 m_char_h;           // 0xD9
    u8 pad_DA[2];

    s16 getSize(s16 mes, s8* L, s8* R);
    void create(int char_w, int char_h, TEXPalette* addr, u8* size);
    void destroy();
    int chkFlag(u32 b) { return (be_flag & b) ? 1 : 0; }
};

// One message slot (MessageControl::mes[16], 0xEC bytes).
class Message {
public:
    u32 stop_bak;       // 0x00  pG->flags_170 saved while the message stops the game
private:
    u32 be_flag;          // 0x04  bit 0 = active, bit 1 = first frame
    u8 r_no_0;              // 0x08  code01 step
    u8 r_no_1;
    u8 r_no_2;
    u8 r_no_3;
public:
    u32 m_state;         // 0x0C  bit 0 = active, bit 1 = finished, bit 3 = width check pass
    f32 m_scale_w;         // 0x10
    f32 m_scale_h;         // 0x14
    s8 m_font_w;           // 0x18
    s8 m_font_h;           // 0x19
    u16 m_item_no;            // 0x1A  message number for code10 (type 3 table)
    u16 m_ot_type;             // 0x1C  ordering table
    u16 m_ot_no;           // 0x1E
private:
    MessageFont* m_pFont;  // 0x20
    u16 m_pos_x;              // 0x24  cursor
    u16 m_pos_y;              // 0x26
    u16 m_pos0_x[16];      // 0x28
    u16 m_pos0_y;          // 0x48
    u16 m_width[16];      // 0x4A
    s16 m_width_max;           // 0x6A
    u16 m_height;
    u16 m_bttn_wait;        // 0x6E
    u8 m_btn;             // 0x70  code08 started
    s8 m_evt_no;             // 0x71  code0d
    s8 m_lines;            // 0x72
    u8 x73;
    u16 m_number_width;           // 0x74  width added by numbers/tables (code0a)
public:
    // PS2 has m_line_gap private; public here because r20e, r224 and r307 read the low byte (lineSpace) through getWork().
    union {
        u16 m_line_gap;      // 0x76
        struct {
            u8 lineH_hi;    // 0x76
            s8 lineSpace;   // 0x77  (embox emBoxAction: prompt y = 336 - fontH - lineSpace - 1)
        };
    };
private:
    u16 m_char_gap;      // 0x78
    u16 x7A;
    u32 m_col;          // 0x7C
    u32 m_attr;           // 0x80
    s16 m_spd;          // 0x84
    s16 m_spd_cnt;       // 0x86
    s16 m_spd_old;      // 0x88
    s16 m_spd_flag;           // 0x8A
    u16 x8C;
    s16 m_wait_cnt;        // 0x8E
    u16 m_jump_mes[3];     // 0x90
    s8 m_jump_idx;         // 0x96
    s8 m_jump_max;         // 0x97
    u16* m_pMes;          // 0x98
    u16* m_pRetAddr;       // 0x9C
    MessageFont* m_pRetFont;  // 0xA0
    u32 m_number;         // 0xA4
    u16 digit;          // 0xA8
    u8 pad_AA[2];
    u32 numberSave;     // 0xAC
    u16 digitSave;      // 0xB0
    u8 pad_B2[6];
    MesQue* m_queue;    // 0xB8
    MesQue* m_pMque;    // 0xBC
    MesQue* m_selTbl[8];  // 0xC0  glyphs of the selection cursors
    s8 m_selTbl_size;          // 0xE0
    s8 m_sel;          // 0xE1  menu selection (0 = none yet)
    s8 m_cur;          // 0xE2
    s8 m_cursol_time;      // 0xE3
    u8 m_who;             // 0xE4  code12
    u8 pad_E5[3];

public:
    virtual ~Message() {}

    int isAlive() { return be_flag & 1; }
    void setBorn() { be_flag |= 3; }
    void setDie() { be_flag &= ~1; }
    u32 attrCk(u32 attr) { return m_attr & attr; }
    s8 getSel() { return m_sel; }
    s8 getCursor() { return m_cur; }
    void setFontSize(s16 w, s16 h) {
        m_font_w = w;
        m_font_h = h;
    }
    void setLineGap(u16 gap) { m_line_gap = gap; }
    void setFontGap(u16 gap) { m_char_gap = gap; }
    s16 getLineGap() { return m_line_gap; }
    s16 getFontHeight() { return m_font_h; }
    u32 getColor() { return m_col; }
    void setColor(u32 col) { m_col = col; }
    void setCursor(int cur) { m_cur = cur; }
    void setBttnWait(s16 wait) { m_bttn_wait = wait; }
    void registQueue(MesQue* q) { m_queue = q; }
    void init(int no, int px, int py, u32 attr, int col, MessageFont* font);
    void move();
    void WidthCk();
    void QueSet(int code, MessageFont* font);
    void setNumber(u32 num, u16 digits);
    void putSelCursol();
    void putNextCursol(int flag);
    void setJump(u16 mes);
    void trans();
    int CommandExec();
    int CommandArg();
    void WaitEnd();
    int code00();
    int code01();
    int code02();
    int code03();
    int code04();
    int code05();
    int code06();
    int code07();
    int code08();
    int code09();
    int code0a();
    int code0b();
    int code0c();
    int code0d();
    int code0e();
    int code0f();
    int code10();
    int code11();
    int code12();
};

typedef Message MesWork;

enum LAYOUT_TYPE {
    LAYOUT_CAPTION = 0,
    LAYOUT_ACT_BTN = 1,
    LAYOUT_SUBSCRN = 2,
    LAYOUT_MEMCARD = 3,
    LAYOUT_OPERATOR = 4,
    LAYOUT_SYSTEM = 5,
    LAYOUT_SHOP_LIST = 6,
    LAYOUT_FILE = 7,
    LAYOUT_MANUAL = 8,
    LAYOUT_NUM = 9
};

// game/mes.cpp
class MessageControl {
private:
    u32 m_sel_sav;            // 0x00
    Message m_Msg[16];        // 0x04
    void* m_font_addr[4];     // 0xEC4
    u32 m_state;              // 0xED4
public:
    FONT_TEX m_mTex[4][2];    // 0xED8
private:
    u32 m_stop;               // 0x11F8
public:

    virtual ~MessageControl() {}

    MesWork* getWork() { return &m_Msg[0]; }

    void setLayout(int no, int type);
    void setLanguage(int lang);
    void setupFont(int char_w, int char_h, TEXPalette* addr, int no);
    void releaseFont(int no);
    int loadFont(int w, int h, const char* name, int no);
    void init();
    void gameInit();
    void roomInit();
    void loadCommonFont();
    void loadSystemFont();
    void stageInit();
    void loadStageFont();
    void loadEventFont();
    void setState(u32 b);
    void unsetState(u32 b);
    int checkState(u32 b);
    void Move();
    void Trans();
    void setFontSize(int no, s16 font_w, s16 font_h);
    void MesSet(int no, int px, int py, u32 attr, int wk, int col, int font_no);
    void Delete(int no);
    void WaitEnd(int no);
    void MesSetOt(int no, u16 type, u16 ot_no) {
        m_Msg[no].m_ot_type = type;
        m_Msg[no].m_ot_no = ot_no;
    }
    void MesSetJump(int no, u16 mes) { m_Msg[no].setJump(mes); }
    void Clear() {
        for (int i = 0; i < 16; i++) {
            Delete(i);
        }
    }
    void MesSetNumber(int no, u32 num, u16 digits) { m_Msg[no].setNumber(num, digits); }
    s8 GetSelectMessage(int no) { return m_Msg[no].getSel(); }
    s8 GetSelectCursor(int no) { return m_Msg[no].getCursor(); }
    u32 GetMesStatus(int no) { return m_Msg[no].m_state; }
    void setLineGap(int no, u16 gap) { m_Msg[no].setLineGap(gap); }
    void setFontGap(int no, u16 gap) { m_Msg[no].setFontGap(gap); }
    s8 getLineGap(int no) { return m_Msg[no].getLineGap(); }
    s8 getFontHeight(int no) { return m_Msg[no].getFontHeight(); }
    void SetColor(int no, u32 col) { m_Msg[no].setColor(col); }
    u32 GetColor(int no) { return m_Msg[no].getColor(); }
    void SetCursor(int no, s8 cur) { m_Msg[no].setCursor(cur); }
    void SetItemName(int no, u16 id) { m_Msg[no].m_item_no = id; }
    void SetBttnWait(int no, s16 wait) { m_Msg[no].setBttnWait(wait); }
    void MesRegistQueue(int no, MesQue* q) { m_Msg[no].registQueue(q); }
    void MesReleaseQueue(int no) { m_Msg[no].registQueue(0); }
};

// ROM font glyph renderer (game/mes.cpp), used by the dvd error screen before the message
// system is up.
class RomFont {
public:
    void* m_FontData;  // 0x00  OSFontHeader

    RomFont(void* font);
    void setup(void* image);
    void draw(int x, int y, int xChar, int yChar);
};

extern MessageControl cMes;
extern MessageData MesData;
extern u32 mes_col_tbl[10];

#endif
