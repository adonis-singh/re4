#ifndef ESP_H
#define ESP_H

#include "types.h"
#include "vec.h"
#include "gx.h"
#include "model.h"
#include "trans_ot.h"

struct EspGenPrmW {
    u32 xCC;           // 0xCC
    u32 xD0;           // 0xD0
};
struct EspGenPrmH {
    u16 xCC;           // 0xCC
    u16 xCE;           // 0xCE
    u16 xD0;           // 0xD0
    u16 xD2;           // 0xD2
};
struct EspGenPrmB {
    u8 xCC, xCD, xCE, xCF;  // 0xCC
    u8 xD0, xD1, xD2, xD3;  // 0xD0
};
union EspGenPrm {
    EspGenPrmW w;
    EspGenPrmH h;
    EspGenPrmB b;
};

// Effect generator record (game/eff_sys.cpp, game/espgen*.cpp): one 0x12C byte entry of an
// EspSeqData (PS2 cEspSeqTbl, 1:1). GC types kept where the PS2 byte is signed.
struct EspGenWork {
    u8 Be_flg;         // 0x00 (PS2 Be_flg)
    u8 Id;             // 0x01 esp id / generator sub type (PS2 Id)
    u8 Tex_id;         // 0x02 (PS2 Tex_id)
    u8 Type;           // 0x03 (PS2 Type) -> cEsp::m_Type
    u16 Set_time;      // 0x04 sequence time (espgen10 compares it with the frame counter) (PS2 Set_time)
    u8 Parent_no;      // 0x06 event model index (EspEvModList) (PS2 Parent_no)
    u8 Parts_no;       // 0x07 (PS2 Parts_no)
    u32 Tool_flg;      // 0x08 (PS2 Tool_flg)
    Vec Pos;           // 0x0C generator position (PS2 Pos)
    Vec R_pos;         // 0x18 position random range (esp1a: .x/.y min / max distance factor) (PS2 R_pos)
    Vec Speed;         // 0x24 (PS2 Speed)
    f32 D_speed;       // 0x30 (PS2 D_speed)
    Vec R_speed;       // 0x34 (PS2 R_speed)
    Vec Speed_plus;    // 0x40 acceleration (PS2 Speed_plus)
    Vec R_speed_plus;  // 0x4C acceleration random range (PS2 R_speed_plus)
    Vec Ang;           // 0x58 (PS2 Ang)
    Vec R_ang;         // 0x64 rotation random range (PS2 R_ang)
    Vec Ang_plus;      // 0x70 rotation speed (PS2 Ang_plus)
    Vec R_ang_plus;    // 0x7C rotation speed random range (PS2 R_ang_plus)
    f32 Size_base_x;   // 0x88 (PS2 Size_base_x)
    f32 Size_base_y;   // 0x8C (PS2 Size_base_y)
    f32 R_size_base;   // 0x90 (PS2 R_size_base)
    f32 Size_plus;     // 0x94 (Espgen43: extra z scale, +1) (PS2 Size_plus)
    f32 D_size_plus;   // 0x98 (PS2 D_size_plus)
    u8 Col_start_r;    // 0x9C colour r (PS2 Col_start_r)
    u8 Col_start_g;    // 0x9D colour g
    u8 Col_start_b;    // 0x9E colour b
    u8 Col_start_a;    // 0x9F colour a
    f32 Col_d_r;       // 0xA0 colour step per frame as 0..1 floats (PS2 Col_d_r)
    f32 Col_d_g;       // 0xA4
    f32 Col_d_b;       // 0xA8
    f32 Col_d_a;       // 0xAC
    u16 Col_max_cnt;   // 0xB0 (esp_efm: fade start frame) (PS2 Col_max_cnt)
    u16 Col_start_cnt; // 0xB2 (esp_efm: fade length) (PS2 Col_start_cnt)
    u16 Pos_start_cnt; // 0xB4 (esp_efm: move start frame) (PS2 Pos_start_cnt)
    u16 Size_start_cnt; // 0xB6 (esp_efm: scale start frame) (PS2 Size_start_cnt)
    u16 Life_max;      // 0xB8 (esp_efm: life) (PS2 Life_max)
    u16 Life_time;     // 0xBA (esp_efm: start frame) (PS2 Life_time)
    u8 Ptn_no;         // 0xBC (esp_sub: start animation pattern) (PS2 Ptn_no)
    u8 Anm_rate;       // 0xBD (esp_sub: animation speed - 0x20) (PS2 sint8 Anm_rate)
    u16 Anm_cnt;       // 0xBE (esp_sub: animation counter) (PS2 Anm_cnt)
    u8 Release_time;   // 0xC0 (esp_efm: parent release frame) (PS2 Release_time)
    u8 Groupe_no;      // 0xC1 (PS2 Groupe_no)
    u8 Blend_type;     // 0xC2 (esp_sub: blend type, bl[] index) (PS2 Blend_type)
    u8 Shimmer_type;   // 0xC3 (esp_sub: cEsp m_Shimmer_type) (PS2 Shimmer_type)
    u8 Shimmer_pow;    // 0xC4 (esp_sub: cEsp m_Shimmer_pow) (PS2 Shimmer_pow)
    u8 MaskTex_id;     // 0xC5 (esp_sub: mask texture id) (PS2 MaskTex_id)
    u8 Del_far;        // 0xC6 (esp_sub: cEsp m_Del_far / 10) (PS2 Del_far)
    u8 Del_near;       // 0xC7 (esp_sub: cEsp m_Del_near / 10) (PS2 Del_near)
    u8 Work8[4];       // 0xC8 per-effect byte parameters (SE number, area number, type, ...) (PS2 signed char Work8[4])
    EspGenPrm prm;     // 0xCC .. 0xD4: per-effect integer parameters (word or halfword view) (PS2 int Work32[0..1])
    u32 xD4;           // 0xD4 (PS2 Work32[2])
    Vec Vec0;          // 0xD8 per-effect float parameters (esp_efm: obj05 burst centre / obj09 size) (PS2 Vec0)
    Vec Vec1;          // 0xE4 (esp_efm: bounce) (PS2 Vec1)
    Vec Vec2;          // 0xF0 (esp_efm: burst centre random range; esp0e .z: visible cone angle in degrees) (PS2 Vec2)
    u8 WorkSp8[4];     // 0xFC ([3]: esp_efm obj04 motion type) (PS2 WorkSp8[4])
    // 0x100..0x12C: sequence record tail (records of an EspSeqData are 0x12C bytes)
    u8 pad_100[0x104 - 0x100];
    u8 Espgen_work8_4[4]; // 0x104 (espgen02: path id, path number, path position offset, its random range) (PS2 Espgen_work8_4)
    u8 Kind;           // 0x108 0 = esp, 1 = espgen (PS2 Kind)
    u8 Espgen_id;      // 0x109 generator id (0xFF = loop marker) (PS2 Espgen_id)
    u8 Espgen_type;    // 0x10A (PS2 Espgen_type)
    u8 Espgen_flg;     // 0x10B (PS2 Espgen_flg)
    s8 Espgen_work8[4]; // 0x10C (espgen00/02: [0] wait, [1] count, [2] random seed offset) (PS2 signed char Espgen_work8[4])
    s16 Espgen_work16[4]; // 0x110 (PS2 Espgen_work16[4])
    Vec Espgen_vec0;   // 0x118 (espgen02: scale - 1 in 10ths) (PS2 Espgen_vec0)
    s8 Espgen_work8_2[4]; // 0x124 (espgen00/02: per-frame D of scale, speed, colour, wait) (PS2 signed char Espgen_work8_2[4])
    u8 Espgen_work8_3[4]; // 0x128 ([1]: espgen02 rotation x in 1/256 turns, [2]: rotation y, [3]: path orientation mode bits) (PS2 Espgen_work8_3)
};

// Effect sequence data block: 0x30 byte header followed by 0x12C byte records (PS2 cEspSeqHead:
// data_num[4], Flg, Null_parts_no, Offset, Ang, Ver_no, Core_flg, SeqTbl[]).
struct EspSeqData {
    u16 num;           // 0x00 number of records
    u8 pad_2[6];
    u16 flags;         // 0x08
    u8 parts;          // 0x0A default parts number (EstSet with no = -1)
    u8 pad_B;
    Vec pos;           // 0x0C default position (EstSet with pos = NULL)
    Vec rot;           // 0x18 default rotation in degrees (EstSet with rot = NULL)
    u8 Ver_no;         // 0x24 file version (t_esp writes 0x10) (PS2 Ver_no)
    u8 pad0;           // 0x25 (PS2 pad0)
    u16 Core_flg;      // 0x26 (PS2 Core_flg)
    u32 pad1[2];       // 0x28 (PS2 pad1)
    EspGenWork rec[1]; // 0x30
};

// Texture animation data returned by EspGetAnmAddr (eff_sys.cpp). Partial layout.
struct EspAnmData {
    u16 Width;         // 0x00 texture width (PS2 cAnm::Width)
    u16 Height;        // 0x02 texture height (PS2 cAnm::Height)
    s16 Cx;            // 0x04 sprite width / centre x (PS2 cAnm::Cx)
    s16 Cy;            // 0x06 sprite height / centre y (PS2 cAnm::Cy)
    union {
        u16 Frames;    // 0x08 number of patterns (PS2 cAnm::Frames)
        struct {
            u8 x8;
            u8 x9;     // 0x09 low byte of Frames
        };
    };
    u8 Xn;             // 0x0A (PS2 cAnm::Xn)
    u8 Loop;           // 0x0B bits 0-1: loop mode (PS2 cAnm::Loop)
    u8 Data_num;       // 0x0C 0 = fixed pattern time (PS2 cAnm::Data_num)
    u8 pad_0D[3];
    u8 Frame_cnt[1];   // 0x10 pattern table: Frames entries, then the per-pattern display times (PS2 cAnm::Frame_cnt)
};

// Effect data owner: EspDataLoad/EspDataRelease/EspGetEstAddr/SstSet `owner`, EspInfo::owner. The names are
// the PS2 ESP_OWNER enumerators; the values are the GC ones, i.e. the index of the same name in eff_sys.cpp
// owner_name_tbl (the GC table has no EM3F/EM4E/EM4B/WEP51, so everything from WEP00 on sits 3 or 4 below
// the PS2 value). SST/ITM/NONE/MAX follow from est.cpp (EstSet owner 0xD0, 0xD2 = free, tables of 0xD3).
enum ESP_OWNER {
    EFF_CORE = 0,
    EFF_ROOM = 1,
    EFF_EM3A = 2,
    EFF_PL00 = 3,
    EFF_PL01 = 4,
    EFF_PL02 = 5,
    EFF_PL03 = 6,
    EFF_PL04 = 7,
    EFF_PL05 = 8,
    EFF_PL06 = 9,
    EFF_PL07 = 10,
    EFF_PL0A = 11,
    EFF_PL0B = 12,
    EFF_PL0D = 13,
    EFF_PL0E = 14,
    EFF_PL0F = 15,
    EFF_EM10 = 16,
    EFF_EM12 = 17,
    EFF_EM15 = 18,
    EFF_EM16 = 19,
    EFF_EM17 = 20,
    EFF_EM18 = 21,
    EFF_EM19 = 22,
    EFF_EM1A = 23,
    EFF_EM1B = 24,
    EFF_EM20 = 25,
    EFF_EM22 = 26,
    EFF_EM23 = 27,
    EFF_EM24 = 28,
    EFF_EM25 = 29,
    EFF_EM26 = 30,
    EFF_EM27 = 31,
    EFF_EM28 = 32,
    EFF_EM29 = 33,
    EFF_EM2A = 34,
    EFF_EM2B = 35,
    EFF_EM2C = 36,
    EFF_EM2D = 37,
    EFF_EM2E = 38,
    EFF_EM2F = 39,
    EFF_EM30 = 40,
    EFF_EM31 = 41,
    EFF_EM32 = 42,
    EFF_EM34 = 43,
    EFF_EM35 = 44,
    EFF_EM36 = 45,
    EFF_EM38 = 46,
    EFF_EM39 = 47,
    EFF_EM3B = 48,
    EFF_EM3C = 49,
    EFF_EM3D = 50,
    EFF_EM3E = 51,
    EFF_WEP00 = 52,
    EFF_WEP01 = 53,
    EFF_WEP02 = 54,
    EFF_WEP03 = 55,
    EFF_WEP04 = 56,
    EFF_WEP05 = 57,
    EFF_WEP06 = 58,
    EFF_WEP07 = 59,
    EFF_WEP08 = 60,
    EFF_WEP09 = 61,
    EFF_WEP0A = 62,
    EFF_WEP0B = 63,
    EFF_WEP0C = 64,
    EFF_WEP0D = 65,
    EFF_WEP0E = 66,
    EFF_WEP0F = 67,
    EFF_WEP10 = 68,
    EFF_WEP11 = 69,
    EFF_WEP12 = 70,
    EFF_WEP13 = 71,
    EFF_WEP14 = 72,
    EFF_WEP15 = 73,
    EFF_WEP16 = 74,
    EFF_WEP17 = 75,
    EFF_WEP18 = 76,
    EFF_WEP19 = 77,
    EFF_WEP26 = 78,
    EFF_WEP27 = 79,
    EFF_WEP28 = 80,
    EFF_WEP29 = 81,
    EFF_WEP30 = 82,
    EFF_WEP33 = 83,
    EFF_ET00 = 84,
    EFF_ET01 = 85,
    EFF_ET02 = 86,
    EFF_ET03 = 87,
    EFF_ET04 = 88,
    EFF_ET05 = 89,
    EFF_ET06 = 90,
    EFF_ET07 = 91,
    EFF_ET08 = 92,
    EFF_ET09 = 93,
    EFF_ET0A = 94,
    EFF_ET0B = 95,
    EFF_ET0C = 96,
    EFF_ET0D = 97,
    EFF_ET0E = 98,
    EFF_ET0F = 99,
    EFF_ET10 = 100,
    EFF_ET11 = 101,
    EFF_ET12 = 102,
    EFF_ET13 = 103,
    EFF_ET14 = 104,
    EFF_ET15 = 105,
    EFF_ET16 = 106,
    EFF_ET17 = 107,
    EFF_ET18 = 108,
    EFF_ET19 = 109,
    EFF_ET1A = 110,
    EFF_ET1B = 111,
    EFF_ET1C = 112,
    EFF_ET1D = 113,
    EFF_ET1E = 114,
    EFF_ET1F = 115,
    EFF_ET20 = 116,
    EFF_ET21 = 117,
    EFF_ET22 = 118,
    EFF_ET23 = 119,
    EFF_ET24 = 120,
    EFF_ET25 = 121,
    EFF_ET26 = 122,
    EFF_ET27 = 123,
    EFF_ET28 = 124,
    EFF_ET29 = 125,
    EFF_ET2A = 126,
    EFF_ET2B = 127,
    EFF_ET2C = 128,
    EFF_ET2D = 129,
    EFF_ET2E = 130,
    EFF_ET2F = 131,
    EFF_ET30 = 132,
    EFF_ET31 = 133,
    EFF_ET32 = 134,
    EFF_ET33 = 135,
    EFF_ET34 = 136,
    EFF_ET35 = 137,
    EFF_ET36 = 138,
    EFF_ET37 = 139,
    EFF_ET38 = 140,
    EFF_ET39 = 141,
    EFF_ET3A = 142,
    EFF_ET3B = 143,
    EFF_ET3C = 144,
    EFF_ET3D = 145,
    EFF_ET3E = 146,
    EFF_ET3F = 147,
    EFF_ET40 = 148,
    EFF_ET41 = 149,
    EFF_ET42 = 150,
    EFF_ET43 = 151,
    EFF_ET44 = 152,
    EFF_ET45 = 153,
    EFF_ET46 = 154,
    EFF_ET47 = 155,
    EFF_ET48 = 156,
    EFF_ET49 = 157,
    EFF_ET4A = 158,
    EFF_ET4B = 159,
    EFF_ET4C = 160,
    EFF_ET4D = 161,
    EFF_ET4E = 162,
    EFF_ET4F = 163,
    EFF_ET50 = 164,
    EFF_ET51 = 165,
    EFF_ET52 = 166,
    EFF_ET53 = 167,
    EFF_ET54 = 168,
    EFF_ET55 = 169,
    EFF_ET56 = 170,
    EFF_ET57 = 171,
    EFF_ET58 = 172,
    EFF_ET59 = 173,
    EFF_ET5A = 174,
    EFF_ET5B = 175,
    EFF_ET5C = 176,
    EFF_ET5D = 177,
    EFF_ET5E = 178,
    EFF_ET5F = 179,
    EFF_ET60 = 180,
    EFF_ET61 = 181,
    EFF_ET62 = 182,
    EFF_ET63 = 183,
    EFF_ET64 = 184,
    EFF_ET65 = 185,
    EFF_ET66 = 186,
    EFF_ET67 = 187,
    EFF_ET68 = 188,
    EFF_ET69 = 189,
    EFF_ET6A = 190,
    EFF_ET6B = 191,
    EFF_ET6C = 192,
    EFF_ET6D = 193,
    EFF_ET6E = 194,
    EFF_ET6F = 195,
    EFF_EV00 = 196,
    EFF_EV01 = 197,
    EFF_EV02 = 198,
    EFF_EV03 = 199,
    EFF_OBM1F = 200,
    EFF_OBM2B = 201,
    EFF_OBM34 = 202,
    EFF_OBM4C = 203,
    EFF_OBM66 = 204,
    EFF_OBM83 = 205,
    EFF_SUBSCR = 206,
    EFF_DEBUG = 207,
    EFF_SST = 208,
    EFF_ITM = 209,
    EFF_NONE = 210,
    EFF_MAX = 211,
    EFF_PL_IDSTART = 3,
    EFF_PL_IDEND = 15,
    EFF_EM_IDSTART = 16,
    EFF_EM_IDEND = 51,
    EFF_WEP_IDSTART = 52,
    EFF_WEP_IDEND = 83,
    EFF_ET_IDSTART = 84,
    EFF_ET_IDEND = 195
};

// Effect core kind (PS2 ESP_CORE_KIND): EspInfo::Core_kind, SstSet/SetEspCore/PullEspEspgen `kind`.
enum ESP_CORE_KIND {
    ESP_CORE_KIND_NONE = 0,
    ESP_CORE_KIND_SST = 1,
    ESP_CORE_KIND_ROOM00 = 2,
    ESP_CORE_KIND_ROOM01 = 3,
    ESP_CORE_KIND_ROOM02 = 4,
    ESP_CORE_KIND_ROOM03 = 5,
    ESP_CORE_KIND_ROOM04 = 6,
    ESP_CORE_KIND_ROOM05 = 7,
    ESP_CORE_KIND_ROOM06 = 8,
    ESP_CORE_KIND_ROOM07 = 9,
    ESP_CORE_KIND_PL_WEP = 10,
    ESP_CORE_KIND_THERMO = 11,
    ESP_CORE_KIND_ROOM_AREA00 = 12,
    ESP_CORE_KIND_ROOM_AREA01 = 13,
    ESP_CORE_KIND_ROOM_AREA02 = 14,
    ESP_CORE_KIND_ROOM_AREA03 = 15,
    ESP_CORE_KIND_ROOM_AREA04 = 16,
    ESP_CORE_KIND_ROOM_AREA05 = 17,
    ESP_CORE_KIND_ROOM_AREA06 = 18,
    ESP_CORE_KIND_ROOM_AREA07 = 19,
    ESP_CORE_KIND_ROOM_AREA08 = 20,
    ESP_CORE_KIND_ROOM_AREA09 = 21,
    ESP_CORE_KIND_ROOM_AREA0A = 22,
    ESP_CORE_KIND_ROOM_AREA0B = 23,
    ESP_CORE_KIND_ROOM_AREA0C = 24,
    ESP_CORE_KIND_ROOM_AREA0D = 25,
    ESP_CORE_KIND_ROOM_AREA0E = 26,
    ESP_CORE_KIND_ROOM_AREA0F = 27,
    ESP_CORE_KIND_ROOM_AREA10 = 28,
    ESP_CORE_KIND_ROOM_AREA11 = 29,
    ESP_CORE_KIND_ROOM_AREA12 = 30,
    ESP_CORE_KIND_ROOM_AREA13 = 31,
    ESP_CORE_KIND_ROOM_AREA14 = 32,
    ESP_CORE_KIND_ROOM_AREA15 = 33,
    ESP_CORE_KIND_ROOM_AREA16 = 34,
    ESP_CORE_KIND_ROOM_AREA17 = 35,
    ESP_CORE_KIND_ROOM_AREA18 = 36,
    ESP_CORE_KIND_ROOM_AREA19 = 37,
    ESP_CORE_KIND_ROOM_AREA1A = 38,
    ESP_CORE_KIND_ROOM_AREA1B = 39,
    ESP_CORE_KIND_ROOM_AREA1C = 40,
    ESP_CORE_KIND_ROOM_AREA1D = 41,
    ESP_CORE_KIND_ROOM_AREA1E = 42,
    ESP_CORE_KIND_ROOM_AREA1F = 43,
    ESP_CORE_KIND_EM10_00 = 44,
    ESP_CORE_KIND_EM10_01 = 45,
    ESP_CORE_KIND_EM10_02 = 46,
    ESP_CORE_KIND_EM10_03 = 47,
    ESP_CORE_KIND_EM10_04 = 48,
    ESP_CORE_KIND_EMWINDOW00 = 49,
    ESP_CORE_KIND_EMTORCH = 50,
    ESP_CORE_KIND_EMWEP = 51,
    ESP_CORE_KIND_WATER = 52,
    ESP_CORE_KIND_BOAT = 53,
    ESP_CORE_KIND_PL0F_CURSOR = 54,
    ESP_CORE_KIND_EV00 = 55,
    ESP_CORE_KIND_EV01 = 56,
    ESP_CORE_KIND_EV02 = 57,
    ESP_CORE_KIND_EV03 = 58,
    ESP_CORE_KIND_ITEM = 59,
    ESP_CORE_KIND_LUIS_ITEM = 60,
    ESP_CORE_KIND_OBJ16_A = 61,
    ESP_CORE_KIND_OBJ16_B = 62,
    ESP_CORE_KIND_MARK = 63,
    ESP_CORE_KIND_OBJPILLAR = 64,
    ESP_CORE_KIND_EM36_00 = 65,
    ESP_CORE_KIND_EM36_01 = 66,
    ESP_CORE_KIND_EM36_02 = 67,
    ESP_CORE_KIND_EM36_03 = 68,
    ESP_CORE_KIND_EM36_04 = 69,
    ESP_CORE_KIND_MINE = 70,
    ESP_CORE_KIND_EM25 = 71,
    ESP_CORE_KIND_BARREL = 72,
    ESP_CORE_KIND_EM22 = 73,
    ESP_CORE_KIND_EM2A = 74,
    ESP_CORE_KIND_EM2D_00 = 75,
    ESP_CORE_KIND_EM2D_01 = 76,
    ESP_CORE_KIND_EM2D_02 = 77,
    ESP_CORE_KIND_EMROCK = 78,
    ESP_CORE_KIND_ITEM_AT = 79,
    ESP_CORE_KIND_ROOM08 = 80,
    ESP_CORE_KIND_ROOM09 = 81,
    ESP_CORE_KIND_ROOM0A = 82,
    ESP_CORE_KIND_ROOM0B = 83,
    ESP_CORE_KIND_ROOM0C = 84,
    ESP_CORE_KIND_ROOM0D = 85,
    ESP_CORE_KIND_ROOM0E = 86,
    ESP_CORE_KIND_ROOM0F = 87,
    ESP_CORE_KIND_ROOM10 = 88,
    ESP_CORE_KIND_ROOM11 = 89,
    ESP_CORE_KIND_ROOM12 = 90,
    ESP_CORE_KIND_ROOM13 = 91,
    ESP_CORE_KIND_ROOM14 = 92,
    ESP_CORE_KIND_ROOM15 = 93,
    ESP_CORE_KIND_ROOM16 = 94,
    ESP_CORE_KIND_GET_TOP = 95,
    ESP_CORE_KIND_END = 255
};

// Effect owner info at the head of every cEsp (copied as a block by esp3f).
struct EspInfo {
    u16 Core_flg;            // 0x00
    u8 Core_kind;             // 0x02 ESP_CORE_KIND
    u8 owner;             // 0x03 ESP_OWNER
    union {
        u32 Call_no;        // 0x04
        struct {
            u8 x4;     // 0x04
            u8 x5;     // 0x05
            u8 x6;     // 0x06
            u8 x7;     // 0x07
        } b;
    };
    void* Core_pEm;          // 0x08
};

// Special cEsp::m_Parts_no values (PS2 ESP_PARTS_NO): 248..253 are the screen layers (esp.cpp AddOtDirect
// ot slots), 254 the world, 255 none.
enum ESP_PARTS_NO {
    ESP_PARTS_NULL = 255,
    ESP_PARTS_WORLD = 254,
    ESP_PARTS_SCR_NO_START = 253,
    ESP_PARTS_SCREEN = 253,
    ESP_PARTS_SCREEN_AFTER1 = 252,
    ESP_PARTS_SCREEN_AFTER2 = 251,
    ESP_PARTS_SCREEN_PRE1 = 250,
    ESP_PARTS_SCREEN_PRE2 = 249,
    ESP_PARTS_SCREEN_FIRST = 248,
    ESP_PARTS_SCR_NO_END = 248,
    ESP_PARTS_NOPARTS = 248
};

// One effect sprite (game/esp.cpp, game/esp_sub.cpp). sizeof 0xF8; the vptr sits at 0xF4.
class cEsp {
public:
    EspInfo info;      // 0x00
    u8 m_Be_flg;           // 0x0C bit0: in use
    u8 m_Id;             // 0x0D effect id
    u8 m_Tex_id;       // 0x0E texture animation id (EspGetAnmAddr; EspGenWork Tex_id) (PS2 m_Tex_id)
    u8 m_Type;         // 0x0F EspGenWork Type (PS2 m_Type)
    u8 m_Rno0;         // 0x10 routine numbers (PS2 m_Rno0..3)
    u8 m_Rno1;         // 0x11
    u8 m_Rno2;         // 0x12 (PS2 m_Rno2)
    u8 m_Rno3;         // 0x13 (PS2 m_Rno3)
    u16 m_Del_near;    // 0x14 near delete distance (EspGenWork Del_near * 10) (PS2 m_Del_near)
    u16 m_Del_far;           // 0x16
    u32 m_Tool_flg;         // 0x18 effect option bits
    cModel* m_pMod;    // 0x1C model the effect is attached to
    u32 m_Guid_pMod;           // 0x20
    cCoord* parent;    // 0x24 parent coordinate (pEffParentWorld = world)
    u8 m_Parts_no;        // 0x28 parts of pModel the effect follows, or an ESP_PARTS_NO sentinel
    u8 m_Release_time;      // 0x29 frames to stay attached to parent (0xFF = forever)
    u16 m_Flg;      // 0x2A bit1: sizeY is a world-space length (beam sprites)
    Vec m_Pos;           // 0x2C
    Vec m_Speed;           // 0x38
    f32 m_D_speed;      // 0x44
    Vec m_Speed_plus;           // 0x48
    Vec m_Ang;           // 0x54
    Vec m_Ang_plus;        // 0x60
    f32 m_Size_base_x;         // 0x6C
    f32 m_Size_base_y;         // 0x70
    f32 m_Size_mul;         // 0x74
    f32 m_Size_plus;      // 0x78
    f32 m_D_size_plus;    // 0x7C
    u8 m_Col_start_r;            // 0x80 (esp0c: copied into the est work colour bytes)
    u8 m_Col_start_g;            // 0x81
    u8 m_Col_start_b;            // 0x82
    u8 m_Col_start_a;            // 0x83
    f32 m_Col_r;          // 0x84
    f32 m_Col_g;          // 0x88
    f32 m_Col_b;          // 0x8C
    f32 m_Col_a;          // 0x90
    f32 m_Col_d_r;       // 0x94
    f32 m_Col_d_g;       // 0x98
    f32 m_Col_d_b;       // 0x9C
    f32 m_Col_d_a;       // 0xA0
    u8 m_Blend_mode;   // 0xA4 GXSetBlendMode arguments (esp_sub bl[Blend_type], esp42/esp_app/objWep set them by hand; GC only, no PS2 field)
    u8 m_Src_factor;   // 0xA5
    u8 m_Dst_factor;   // 0xA6
    u8 m_Logic_op;     // 0xA7
    u16 m_Col_max_cnt;           // 0xA8
    u16 m_Col_start_cnt;           // 0xAA
    u16 m_Pos_start_cnt;        // 0xAC frames the speed is applied (0 = always)
    u16 m_Size_start_cnt;      // 0xAE frames the scale speed is applied (0 = always)
    u16 m_Life_max;          // 0xB0 life time in frames (0 = infinite)
    u16 m_Life_time;           // 0xB2 frame counter
    u8 m_Ptn_no;         // 0xB4 current animation pattern
    u8 m_Anm_rate;         // 0xB5
    u16 m_Anm_cnt;        // 0xB6
    f32 m_Radius;           // 0xB8
    Mtx m_Mat;           // 0xBC model matrix built by the Trans functions
    union {
        u8 pad_EC[0xF4 - 0xEC];
        struct {
            u8 m_Shimmer_type;        // 0xEC  (EspGenWork xC3; esp.cpp: 0 = plain EspCommonTrans)
            u8 m_Shimmer_pow;        // 0xED  (EspGenWork xC4)
            u16 m_MaskAnm_cnt;   // 0xEE  mask texture animation counter
            u8 m_MaskPtn_no;    // 0xF0  mask texture animation pattern
            u8 m_MaskTex_id;     // 0xF1  mask texture animation id (EspGenWork xC5)
            u8 m_Blend_type;  // 0xF2  EspGenWork xC2 (3: colour bytes scaled by the fade)
            u8 xF3;
        };
    };
    // 0xF4 vptr

    void* operator new(unsigned int size);
    cEsp();
    virtual ~cEsp();
    virtual void move();
    virtual int SetFreeWork(EspGenWork* pSeq, u32* pRand_seed);
    virtual void Destruct();

    int CommonMove();
    int AnmMove();
    int ColorUpdate();
    void ApplyMatrix(Mtx pMat);
    void CommonStateSet();
    int ChannelSet();   // col.a != 0 (esp18 tests it)
    int IsActive();
};

// game/esp3f.cpp: vector buffer owned by an effect (see esp3f.cpp for the class)
class cEsp3f;
int Esp3f_Alloc(u32 WorkSize, u32 Num, cEsp3f** ppEsp, EspInfo* pEff_core);
Vec* Esp3f_GetVecPtr(cEsp3f* pEsp, u32 idx);

// game/esp.cpp
typedef cEsp* (*EspCreateFunc)();
typedef void (*EspTransFunc)(cEsp*);
void PushEsp(cEsp* pEsp);
extern "C" {
void EspFuncTblSet(int id, EspCreateFunc create, EspTransFunc trans);
int PullEsp(cEsp** ppEsp, int id);
cEsp* EspGetDmyPtr();
void EspAddOtAfterRender(cEsp* esp, void (*func)(cEsp*));
void EspArrayClear();
// game/esp_app.cpp
void EffSetId();
void EspFreeSizeCheckAll();
void EffCrearRoomSeFunc();
void EffAreaUpdate();
void EffEm2d_setTexRender(cModel* pMod);
void EspDrawLaserLine2(Vec* from, Vec* to, u8 r, u8 g, u8 b, u8 a);
void EspSetGatling(Vec pos, Vec dir);   // Vec by value (obj15)
void setPlWaterOtType();
// game/eff_sys.cpp
void EffSetAreaState(int no, int flg);
// game/esp_efm.cpp
void EfmDelete(int a, int b, void* c);
void EfmDeleteEvent();
void EfmArrayClear();
// game/esp_app.cpp
int EffAreaCheckInRoom(Vec* pos);
// game/esp01.cpp
void EspStrip_draw_poly(cEsp* esp, int no, Vec* v, u8 texRepeat, int flag);
// game/trans_ot.cpp: AddOtWorldPos & co. are declared in trans_ot.h (void* data / u16 kind).
// game/esp_sub.cpp
void EspCommonTrans(cEsp* esp);
int EspEstSetSelect(int owner, int id, int no, cEsp** ppEsp, int bNoSuspend);   // objWep drawPoint: (0, 0x50, 0, &esp, 1)
// game/esp_app.cpp: laser sight line (objWep drawLaserSight), Vec by value
void EspDrawLaserLine(Vec lpos, Vec lcross, f32 rate);
// game/eff_sys.cpp
int EspGetAnmAddr(int no, EspAnmData** ppAnm);
void EspTexSet(int anmNo, int ptn);
void* EspGetPathAddr(u32 owner, int id);
struct EspSeqData* EspGetEstAddr(u32 owner, int id, int quiet);
void EspGenSetMoveLoop(int loop);
void EspGenLoopMove();
// game/path.cpp
int PathHasWeight(void* pPdat);
f32 PathGetLength(void* pPdat);
int PathGetPos(void* pPdat, f32 dist, u16* pPntNo, Vec* pPos);  // f32 second: callee copies f1 right after r3
int PathGetPosEm(void* pPdat, cModel* pMod, f32 dist, u16* pPntNo, Vec* pPos);
int EspGetTplAddr(int no, void** pTpl_addr);
// game/est.cpp. void: no caller reads r3 after the call, and with an `int` result the call's
// set of r3 changes the haifa depend counts, moving `li r3,0` to the end of the arg setup
// (obj01/obj10 move00, obj10AddSpeed).
void EstSet(cModel* a, int b, Vec* pos, Vec* rot, int c, u8 d, u16 e, u8 f, void* g, void* h);
}
// game/est.cpp: the C++ overload the plain EstSet forwards to, with the est data block resolved.
void EstSet(cModel* model, int no, Vec* pos, Vec* rot, EspSeqData* head, u16 e, u8 f, void* g, u32 owner, void* h);
// game/eff_sys.cpp
int EspGenGetMoveLoop();
extern cCoord* pEffParentWorld;
extern char* owner_name_tbl[0xD1];   // effect owner names 0..0xD0 (debug display; eff_sys.cpp)
// game/esp_app.cpp
extern "C" void EspCallSeType(int type, Vec* pos);
void EffCallRoomSeFunc(int no, Vec* pPos);
int EffAreaCheckNo(Vec* pos, u8 areaNo);
void EspFootCall(int type, int no, Vec* pPos);
int EspPlWaterCall(int type, Vec* pPos);
// game/Espgen42.cpp
int GetWaterHeight(Vec* pos, f32* Ret);
extern "C" void AddWaterPower(Vec& pos, f32 power);
extern "C" {
void EspWaterInit();
void Espgen42SetNoWater(int flg);
int GetWaterCrossPos(Vec* pos, Vec* dir, Vec* Ret);
}
// game/Espgen43.cpp
extern "C" {
int GetSandHeight(Vec* pos, f32* Ret);
void AddSandPower(Vec& pos, f32 power);
// game/eff_sys.cpp
int EspChkTexId(int no);   // 1 when texture `no` has an object
GXTexObj* EspGetTexObj(int no, int ptn_no);
GXTlutObj* EspGetTlutObj(int no);
struct EspTexWk* EspGetTexWk(int id, int quiet);   // NULL (and an error unless quiet) when the id has no texture
int EspGetTexOwner(int id, u32* pOwner);
int EspGetEfmAddr(int id, void** ppBin, void** ppTpl);
int EspGetEfmMotAddr(int id, u32 no, void** ppMot);
u8 EspPullCoreKind();
int EffAreaDataLoad(struct SstArea* area);
int EffIsSetFinalCol();
void EffGetFinalCol(GXColor* Ret_col);
void EffSetFinalCol(u8 r, u8 g, u8 b, u8 a);
int EffGetAreaState(int no);
void EffSetToolState(int state);
u8 EffGetToolState();
void EffClearToolState();
void EffSetToolStateCallBack(int no, void (*on)(), void (*off)());
void EffCallToolStateCallBack();
// Loads the effect data at `addr` under `owner` (the rooms load their EFF sub-files)
int EspDataLoad(u32 eff_addr, u32 owner, int MultipleOK);
}
// game/eff_sys.cpp (C++ linkage): the TPL of effect model `id`; 0 when not registered
int EspGetEfmTplAddr(int id, void** ppTpl);
// game/eff_sys.cpp: quad display list shared by the sprite effects (esp_sub)
extern u8 g_EspCommonDisplayList[0x60];
// game/trans.cpp: fallback texture used when an effect texture id has no object
extern GXTexObj Specular;

// Sprite texture-corner selection (esp_sub/esp0f/esp12/esp16/esp18 Trans; esp08 has its own leaf
// shapes): flags bit1 flips s, bit2 flips t, screen sprites are drawn upside down. One combined
// condition and corners built from a `zero` variable: each leaf is a jump target where cse knows
// neither operand of `zero + z`, which keeps the `fadds` (nested ifs with literals fold 0 + z).
#define ESP_SPRITE_SCREEN(esp) ((s8) (esp)->m_Parts_no >= -8 && (s8) (esp)->m_Parts_no <= -3)
#define ESP_SPRITE_FLIP_T(esp)                                                                    \
    ((ESP_SPRITE_SCREEN(esp) && !((esp)->m_Tool_flg & 4)) || (!ESP_SPRITE_SCREEN(esp) && ((esp)->m_Tool_flg & 4)))
#define ESP_SPRITE_CORNERS(esp, zero, z, s0, s1, t0, t1)                                          \
    if ((esp)->m_Tool_flg & 2) {                                                                  \
        if (ESP_SPRITE_FLIP_T(esp)) {                                                             \
            s0 = zero + z;                                                                        \
            s1 = zero;                                                                            \
            t0 = s0;                                                                              \
            t1 = s1;                                                                              \
        } else {                                                                                  \
            s0 = zero + z;                                                                        \
            t0 = zero;                                                                            \
            s1 = zero;                                                                            \
            t1 = s0;                                                                              \
        }                                                                                         \
    } else {                                                                                      \
        if (ESP_SPRITE_FLIP_T(esp)) {                                                             \
            s0 = zero;                                                                            \
            s1 = s0 + z;                                                                          \
            t1 = s0;                                                                              \
            t0 = s1;                                                                              \
        } else {                                                                                  \
            s0 = zero;                                                                            \
            s1 = s0 + z;                                                                          \
            t0 = s0;                                                                              \
            t1 = s1;                                                                              \
        }                                                                                         \
    }

// game/emdata.cpp: swap enemy module `id`'s effect data in / out around a room event.
void EspEmDataSwapPush(int em_id);
void EspEmDataSwapPop(int em_id);
// game/eff_sys.cpp: registers a scroll model's texture palette for the room's effect models.
void RoomEfmRegist(cModel* pMod, u8 no);
// Same for a model / texture palette pair that is not a scroll model yet (raw addresses).
void RoomEfmRegist(void* model, void* tpl, u8 id);
// game/eff_sys.cpp: releases the effect data of owner `id` (C linkage).
extern "C" int EspDataRelease(u32 owner, int flag, int warn);
// Debug tools (tools.cpp ToolArrayPush/ToolWorkPop): swap the esp work pool for a Debug_alloc'd one of
// `num` works and back; 1 when done, 0 when a pool is already pushed / none is.
extern "C" int EspArrayPush(u32 num);
extern "C" int EspArrayPop();

// game/esp.cpp: the effect pool's frame update / draw / allocation and its debug view (C linkage).
extern "C" {
void EspFuncTblInit();
int EspMove();
int EspTrans();
int EspArrayAlloc(u32 workNum);
int EspDispInfo();
// Camera pan angles the billboard effects face (esp_sub.cpp, esp08.cpp).
f32 EspGetCameraPan();
f32 EspGetCameraPan2();
}
extern u32 tubo_amb;   // ambient colour the breakable pots add (embox.cpp)

#endif
