// game/merchant: the merchant's stock, weapon tune tables and shop prices (D:/Bio4/Prog/merchant.cpp).
#include "types.h"
#include "global.h"
#include "main.h"
#include "atari.h"
#include "item.h"
#include "room_data.h"
#include "merchant.h"

#define MERCHANT_NUM 1
#define STOCK_MAX 64
#define LEVEL_MAX 32
#define LIST_MAX 0xFF

// ItemWork::x6 tune levels (the exclusive nibble is read as a byte)

MerchantInfo merchant_info_A = {0, -10, -10, -10, -10, 10000, 5, 10, 10, 10, 20, 30, 70, 30, 10};

LevelPrice level_price[] = {
    {0x23, {700, 1000, 1500, 2000, 3000, 4000, 0}, {500, 1200, 0}, {400, 1000, 0}, {400, 600, 1000, 1500, 2000, 0, 0}},
    {0x25, {1500, 2000, 2400, 2800, 4500, 8000, 0}, {1000, 1500, 0}, {600, 1000, 0}, {600, 800, 1200, 1600, 2200, 0, 0}},
    {0x03, {1500, 1700, 2000, 2500, 3500, 0, 0}, {0, 0, 0}, {600, 1500, 0}, {700, 1000, 1200, 1600, 2000, 3500, 0}},
    {0x21, {1000, 1500, 2000, 2500, 3500, 4000, 0}, {1000, 2000, 0}, {800, 1800, 0}, {800, 1000, 1500, 1800, 2400, 0, 0}},
    {0x27, {1500, 1800, 2400, 3000, 4000, 8000, 0}, {1000, 2000, 0}, {800, 1500, 0}, {800, 1000, 1500, 2000, 2500, 0, 0}},
    {0x29, {2500, 3000, 3500, 5000, 7000, 15000, 0}, {0, 0, 0}, {1500, 2000, 0}, {1500, 2000, 2500, 0, 0, 0, 0}},
    {0x2C, {1500, 2000, 2500, 3000, 4500, 9000, 0}, {0, 0, 0}, {700, 1500, 0}, {800, 1000, 1200, 1500, 2000, 0, 0}},
    {0x2D, {2500, 2800, 3200, 4000, 6000, 0, 0}, {0, 0, 0}, {800, 1500, 0}, {1000, 1200, 1600, 1800, 2500, 6000, 0}},
    {0x94, {2000, 2400, 2800, 3200, 5000, 12000, 0}, {0, 0, 0}, {700, 2000, 0}, {1000, 1200, 1500, 2000, 2500, 0, 0}},
    {0x2E, {1000, 1200, 2000, 2500, 3500, 8000, 0}, {0, 0, 0}, {800, 1800, 0}, {600, 800, 1200, 1800, 2500, 0, 0}},
    {0x2F, {1500, 1800, 2400, 3000, 4000, 0, 0}, {8000, 0, 0}, {900, 1800, 0}, {1000, 1200, 1500, 2000, 2500, 0, 0}},
    {0x30, {700, 1400, 1800, 2400, 3500, 10000, 0}, {0, 0, 0}, {500, 1500, 0}, {700, 1500, 2000, 2500, 3500, 0, 0}},
    {0x36, {2500, 4500, 3000, 0, 0, 0, 0}, {0, 0, 0}, {1800, 0, 0}, {2500, 4000, 0, 0, 0, 0, 0}},
    {0x34, {2500, 2500, 3000, 3000, 3500, 5000, 0}, {0, 0, 0}, {1500, 2000, 0}, {1500, 1800, 2000, 2500, 3000, 0, 0}},
    {0x2A, {6000, 8000, 0, 0, 0, 0, 0}, {0, 0, 0}, {2000, 3000, 0}, {3000, 4000, 0, 0, 0, 0, 0}},
    {0x37, {4000, 5000, 7000, 9000, 12000, 20000, 0}, {0, 0, 0}, {2500, 5000, 0}, {1500, 2000, 2500, 3500, 5000, 0, 0}},
};

LevelEntry level_first[] = {
    {0x23, {2, 2, 2, 2}},
    {0x2C, {2, 1, 2, 2}},
    {0x2E, {2, 1, 2, 2}},
    {0xFFFF},
};

LevelEntry level_styer[] = {
    {0x30, {2, 1, 2, 2}},
    {0xFFFF},
};

static LevelEntry level_r10e_day[] = {
    {0x21, {2, 2, 2, 2}},
    {0xFFFF},
};

LevelEntry level_1st_night[] = {
    {0x23, {3, 2, 2, 3}},
    {0x2C, {3, 1, 2, 3}},
    {0x2E, {3, 1, 2, 3}},
    {0xFFFF},
};

LevelEntry level_r112[] = {
    {0x25, {2, 2, 2, 2}},
    {0x21, {3, 2, 2, 3}},
    {0x30, {3, 1, 2, 3}},
    {0xFFFF},
};

LevelEntry level_r200[] = {
    {0x23, {4, 3, 3, 4}},
    {0x25, {3, 2, 2, 3}},
    {0x27, {2, 2, 2, 2}},
    {0x29, {2, 1, 1, 2}},
    {0x2C, {4, 1, 3, 4}},
    {0x2E, {4, 1, 3, 4}},
    {0x2F, {2, 1, 2, 2}},
    {0x30, {4, 1, 2, 3}},
    {0x94, {2, 1, 1, 1}},
    {0x36, {1, 1, 1, 2}},
    {0xFFFF},
};

LevelEntry level_r202[] = {
    {0x94, {3, 1, 1, 1}},
    {0xFFFF},
};

LevelEntry level_r204[] = {
    {0x23, {5, 3, 3, 5}},
    {0x25, {4, 3, 3, 4}},
    {0x21, {4, 3, 3, 4}},
    {0x2E, {5, 1, 3, 5}},
    {0x2F, {3, 1, 2, 3}},
    {0x30, {5, 1, 3, 3}},
    {0x94, {3, 1, 2, 2}},
    {0xFFFF},
};

LevelEntry level_r20b[] = {
    {0x27, {3, 2, 2, 3}},
    {0x29, {3, 1, 2, 2}},
    {0x94, {4, 1, 2, 3}},
    {0x36, {2, 1, 1, 2}},
    {0xFFFF},
};

LevelEntry level_r211[] = {
    {0x2C, {5, 1, 3, 5}},
    {0x2F, {4, 1, 3, 4}},
    {0x30, {6, 1, 3, 4}},
    {0xFFFF},
};

static LevelEntry level_r214[] = {
    {0x23, {6, 3, 3, 6}},
    {0x25, {5, 3, 3, 5}},
    {0x21, {5, 3, 3, 5}},
    {0x27, {4, 3, 3, 4}},
    {0x29, {4, 1, 2, 3}},
    {0x2E, {6, 1, 3, 6}},
    {0x94, {5, 1, 2, 4}},
    {0x36, {2, 1, 2, 2}},
    {0xFFFF},
};

LevelEntry level_r229[] = {
    {0x2D, {2, 1, 2, 2}},
    {0xFFFF},
};

static LevelEntry level_r220[] = {
    {0x94, {5, 1, 2, 5}},
    {0x36, {3, 1, 2, 2}},
    {0xFFFF},
};

LevelEntry level_r225[] = {
    {0x25, {6, 3, 3, 5}},
    {0x21, {6, 3, 3, 6}},
    {0x27, {5, 3, 3, 5}},
    {0x29, {5, 1, 3, 3}},
    {0x2C, {6, 1, 3, 6}},
    {0x2D, {3, 1, 2, 3}},
    {0x2F, {5, 1, 3, 5}},
    {0x30, {6, 1, 3, 5}},
    {0x23, {7, 3, 3, 6}},
    {0x2E, {7, 1, 3, 6}},
    {0xFFFF},
};

static LevelEntry level_r227[] = {
    {0x25, {6, 3, 3, 6}},
    {0xFFFF},
};

LevelEntry level_r22a[] = {
    {0x27, {6, 3, 3, 6}},
    {0x2D, {4, 1, 3, 4}},
    {0x94, {6, 1, 3, 6}},
    {0x30, {6, 1, 3, 6}},
    {0x36, {3, 1, 2, 3}},
    {0x25, {7, 3, 3, 6}},
    {0x21, {7, 3, 3, 6}},
    {0x27, {7, 3, 3, 6}},
    {0x2C, {7, 1, 3, 6}},
    {0x94, {7, 1, 3, 6}},
    {0x30, {7, 1, 3, 6}},
    {0xFFFF},
};

LevelEntry level_r301[] = {
    {0x29, {5, 1, 3, 4}},
    {0x2D, {5, 1, 3, 5}},
    {0x2F, {6, 1, 3, 6}},
    {0x2F, {6, 2, 3, 6}},
    {0x36, {4, 1, 2, 3}},
    {0xFFFF},
};

static LevelEntry level_r305[] = {
    {0x2D, {6, 1, 3, 6}},
    {0x2A, {2, 1, 2, 2}},
    {0x2D, {6, 1, 3, 7}},
    {0xFFFF},
};

LevelEntry level_r31a[] = {
    {0x29, {6, 1, 3, 4}},
    {0x2A, {3, 1, 3, 3}},
    {0xFFFF},
};

LevelEntry level_r31d[] = {
    {0x29, {7, 1, 3, 4}},
    {0xFFFF},
};

// declared before its definition so that its size is unknown at the uses (full address, not @sda21)
LevelEntry level_null[] = {
    {0xFFFF},
};

static LevelEntry level_r329[] = {
    {0xFFFF},
};

LevelEntry level_ext_normal[] = {
    {0x03, {6, 1, 3, 7}},
    {0xFFFF},
};

static LevelEntry level_ext_sw500[] = {
    {0x37, {7, 1, 3, 6}},
    {0xFFFF},
};

LevelEntry level_ext_tompson[] = {
    {0x34, {7, 1, 3, 6}},
    {0xFFFF},
};

PriceEntry exer_price_1st[] = {
    {0x01, 400, 1}, {0x02, 200, 1}, {0x04, 10, 1}, {0x05, 1000, 1},
    {0x06, 100, 1}, {0x95, 60, 1}, {0x97, 240, 1}, {0x07, 30, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x0E, 100, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x15, 2000, 1},
    {0x16, 900, 1}, {0x18, 30, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x20, 6, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0xA8, 1000, 1},
    {0x57, 200, 1}, {0x77, 1000, 1}, {0x58, 1000, 1}, {0x59, 1000, 1},
    {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1}, {0x5D, 1000, 1},
    {0x5E, 300, 1}, {0x5F, 300, 1}, {0x60, 300, 1}, {0x61, 300, 1},
    {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1}, {0x65, 1500, 1},
    {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1}, {0xC6, 300, 1},
    {0xC7, 300, 1}, {0xC8, 300, 1}, {0xC9, 300, 1}, {0xCA, 1000, 1},
    {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1}, {0xCE, 1500, 1},
    {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x89, 100, 1}, {0x8A, 100, 1},
    {0xFFFF},
};

PriceEntry sell_price_r104[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x2C, 1820, 1},
    {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1}, {0x44, 700, 1},
    {0x7D, 3000, 1}, {0xA9, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r102_r10d_r10e[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x2C, 1820, 1},
    {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1}, {0x43, 400, 1},
    {0x44, 700, 1}, {0x7D, 3000, 1}, {0xA9, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r112[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r11c[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r10f[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1},
    {0xFFFF},
};

StockEntry stock_r104[] = {
    {0x05, 1},
    {0x23, 1},
    {0x2C, 1},
    {0x2E, 1},
    {0x30, 1},
    {0x44, 1},
    {0x35, 1},
    {0x7D, 1},
    {0xA9, 1},
    {0xFFFF},
};

static StockEntry stock_r102[] = {
    {0x43, 1},
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r10e_day[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r10d[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r10e_night[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r112[] = {
    {0x05, 1},
    {0x25, 1},
    {0x42, 1},
    {0xFFFF},
};

StockEntry stock_r11c[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r11c_after_event[] = {
    {0x21, 1},
    {0xFFFF},
};

StockEntry stock_r10f[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_1st_mission[] = {
    {0x40, 1},
    {0xFFFF},
};

PriceEntry exer_price_2st[] = {
    {0x01, 400, 1}, {0x02, 200, 1}, {0x04, 10, 1}, {0x05, 1000, 1},
    {0x06, 100, 1}, {0x95, 60, 1}, {0x97, 240, 1}, {0x07, 30, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x0E, 100, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x15, 2000, 1},
    {0x16, 900, 1}, {0x18, 30, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x20, 6, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0xA8, 1000, 1},
    {0xAA, 800, 1}, {0x57, 200, 1}, {0x77, 1000, 1}, {0x58, 1000, 1},
    {0x59, 1000, 1}, {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1},
    {0x5D, 1000, 1}, {0x5E, 300, 1}, {0x5F, 300, 1}, {0x60, 300, 1},
    {0x61, 300, 1}, {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1},
    {0x65, 1500, 1}, {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1},
    {0xC6, 300, 1}, {0xC7, 300, 1}, {0xC8, 300, 1}, {0xC9, 300, 1},
    {0xCA, 1000, 1}, {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1},
    {0xCE, 1500, 1}, {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x89, 100, 1},
    {0x8A, 100, 1}, {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1},
    {0x2D, 3240, 1}, {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x46, 100, 1},
    {0x00, 100, 1}, {0x45, 1000, 1}, {0x8F, 850, 1}, {0x98, 1200, 1},
    {0x70, 2000, 1}, {0x93, 1300, 1}, {0x90, 1000, 1}, {0x91, 1200, 1},
    {0x96, 1200, 1}, {0x9A, 900, 1}, {0x9B, 1100, 1}, {0x9C, 1300, 1},
    {0x9D, 2500, 1}, {0x9E, 2700, 1}, {0x9F, 4800, 1}, {0xB8, 450, 1},
    {0xB9, 100, 1}, {0xBA, 150, 1}, {0xBB, 300, 1}, {0xBC, 650, 1},
    {0xBD, 700, 1}, {0xBE, 850, 1}, {0xBF, 1100, 1}, {0xC0, 1300, 1},
    {0xC1, 1500, 1}, {0xC2, 3200, 1}, {0x56, 250, 1},
    {0xFFFF},
};

static PriceEntry sell_price_2st_first[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x36, 2300, 1},
    {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2F, 3200, 1}, {0x94, 2990, 1},
    {0x45, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r20f[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2F, 3200, 1},
    {0x94, 2990, 1}, {0x45, 1000, 1},
    {0xFFFF},
};

static PriceEntry sell_price_r229[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2D, 3240, 1},
    {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x45, 1000, 1},
    {0xFFFF},
};

StockEntry stock_2st_first[] = {
    {0x05, 1},
    {0x54, 1},
    {0x7E, 1},
    {0x27, 1},
    {0x29, 1},
    {0x2F, 1},
    {0x36, 1},
    {0x94, 1},
    {0x45, 1},
    {0xAA, 1},
    {0x21, 1},
    {0x23, 1},
    {0x25, 1},
    {0x2C, 1},
    {0x2E, 1},
    {0x30, 1},
    {0x42, 1},
    {0x43, 1},
    {0x44, 1},
    {0xFFFF},
};

StockEntry stock_2st_general[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r20f[] = {
    {0x7F, 1},
    {0xFFFF},
};

static StockEntry stock_r229[] = {
    {0x2D, 1},
    {0xFFFF},
};

static PriceEntry exer_price_3st[] = {
    {0x01, 400, 1}, {0x02, 200, 1}, {0x04, 10, 1}, {0x05, 1000, 1},
    {0x06, 100, 1}, {0x95, 60, 1}, {0x97, 240, 1}, {0x07, 30, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x0E, 100, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x15, 2000, 1},
    {0x16, 900, 1}, {0x18, 30, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x20, 6, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0xA8, 1000, 1},
    {0xAA, 800, 1}, {0x57, 200, 1}, {0x77, 1000, 1}, {0x58, 1000, 1},
    {0x59, 1000, 1}, {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1},
    {0x5D, 1000, 1}, {0x5E, 300, 1}, {0x5F, 300, 1}, {0x60, 300, 1},
    {0x61, 300, 1}, {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1},
    {0x65, 1500, 1}, {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1},
    {0xC6, 300, 1}, {0xC7, 300, 1}, {0xC8, 300, 1}, {0xC9, 300, 1},
    {0xCA, 1000, 1}, {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1},
    {0xCE, 1500, 1}, {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x89, 100, 1},
    {0x8A, 100, 1}, {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1},
    {0x2D, 3240, 1}, {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x46, 100, 1},
    {0x00, 100, 1}, {0x45, 1000, 1}, {0x8F, 850, 1}, {0x98, 1200, 1},
    {0x70, 2000, 1}, {0x93, 1300, 1}, {0x90, 1000, 1}, {0x91, 1200, 1},
    {0x96, 1200, 1}, {0x9A, 900, 1}, {0x9B, 1100, 1}, {0x9C, 1300, 1},
    {0x9D, 2500, 1}, {0x9E, 2700, 1}, {0x9F, 4800, 1}, {0xB8, 450, 1},
    {0xB9, 100, 1}, {0xBA, 150, 1}, {0xBB, 300, 1}, {0xBC, 650, 1},
    {0xBD, 700, 1}, {0xBE, 850, 1}, {0xBF, 1100, 1}, {0xC0, 1300, 1},
    {0xC1, 1500, 1}, {0xC2, 3200, 1}, {0x56, 250, 1}, {0x6A, 10, 1},
    {0x2A, 6300, 1}, {0xFE, 6000, 1}, {0xA1, 300, 1}, {0xD1, 1500, 1},
    {0xD2, 350, 1}, {0xD3, 350, 1}, {0xD4, 350, 1}, {0xD5, 2000, 1},
    {0xD6, 2000, 1}, {0xD7, 2000, 1}, {0xD8, 2500, 1}, {0xD9, 2500, 1},
    {0xDA, 2500, 1}, {0xDB, 3000, 1},
    {0xFFFF},
};

static PriceEntry sell_price_r301[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2D, 3240, 1},
    {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x45, 1000, 1}, {0x2A, 6300, 1},
    {0xFFFF},
};

PriceEntry sell_price_r305[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2D, 3240, 1},
    {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x45, 1000, 1}, {0x2A, 6300, 1},
    {0xFE, 6000, 1},
    {0xFFFF},
};

StockEntry stock_3st_general[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r301[] = {
    {0x55, 1},
    {0x05, 1},
    {0x7E, 1},
    {0x27, 1},
    {0x29, 1},
    {0x2F, 1},
    {0x36, 1},
    {0x94, 1},
    {0x45, 1},
    {0xAA, 1},
    {0x21, 1},
    {0x23, 1},
    {0x25, 1},
    {0x2C, 1},
    {0x2E, 1},
    {0x30, 1},
    {0x42, 1},
    {0x43, 1},
    {0x44, 1},
    {0x2D, 1},
    {0x2A, 1},
    {0xFFFF},
};

StockEntry stock_r305[] = {
    {0x05, 1},
    {0xFE, 1},
    {0xFFFF},
};

static PriceEntry exer_price_ext[] = {
    {0x01, 400, 1}, {0x02, 200, 1}, {0x04, 10, 1}, {0x05, 1000, 1},
    {0x06, 100, 1}, {0x95, 60, 1}, {0x97, 240, 1}, {0x07, 30, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x0E, 100, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x15, 2000, 1},
    {0x16, 900, 1}, {0x18, 30, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x20, 6, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0xA8, 1000, 1},
    {0xAA, 800, 1}, {0x57, 200, 1}, {0x77, 1000, 1}, {0x58, 1000, 1},
    {0x59, 1000, 1}, {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1},
    {0x5D, 1000, 1}, {0x5E, 300, 1}, {0x5F, 300, 1}, {0x60, 300, 1},
    {0x61, 300, 1}, {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1},
    {0x65, 1500, 1}, {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1},
    {0xC6, 300, 1}, {0xC7, 300, 1}, {0xC8, 300, 1}, {0xC9, 300, 1},
    {0xCA, 1000, 1}, {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1},
    {0xCE, 1500, 1}, {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x89, 100, 1},
    {0x8A, 100, 1}, {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1},
    {0x2D, 3240, 1}, {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x46, 100, 1},
    {0x00, 100, 1}, {0x45, 1000, 1}, {0x8F, 850, 1}, {0x98, 1200, 1},
    {0x70, 2000, 1}, {0x93, 1300, 1}, {0x90, 1000, 1}, {0x91, 1200, 1},
    {0x96, 1200, 1}, {0x9A, 900, 1}, {0x9B, 1100, 1}, {0x9C, 1300, 1},
    {0x9D, 2500, 1}, {0x9E, 2700, 1}, {0x9F, 4800, 1}, {0xB8, 450, 1},
    {0xB9, 100, 1}, {0xBA, 150, 1}, {0xBB, 300, 1}, {0xBC, 650, 1},
    {0xBD, 700, 1}, {0xBE, 850, 1}, {0xBF, 1100, 1}, {0xC0, 1300, 1},
    {0xC1, 1500, 1}, {0xC2, 3200, 1}, {0x56, 250, 1}, {0x6A, 10, 1},
    {0x2A, 6300, 1}, {0xFE, 6000, 1}, {0xA1, 300, 1}, {0xD1, 1500, 1},
    {0xD2, 350, 1}, {0xD3, 350, 1}, {0xD4, 350, 1}, {0xD5, 2000, 1},
    {0xD6, 2000, 1}, {0xD7, 2000, 1}, {0xD8, 2500, 1}, {0xD9, 2500, 1},
    {0xDA, 2500, 1}, {0xDB, 3000, 1}, {0x03, 7000, 1}, {0x6D, 65535, 1},
    {0x37, 30000, 1}, {0x34, 20000, 1}, {0x6A, 10, 1}, {0x1A, 120, 1},
    {0xFFFF},
};

PriceEntry sell_price_ext[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2D, 3240, 1},
    {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x45, 1000, 1}, {0x2A, 6300, 1},
    {0xFE, 6000, 1}, {0x03, 7000, 1}, {0x6D, 65535, 1}, {0x37, 30000, 1},
    {0x34, 20000, 1},
    {0xFFFF},
};

StockEntry stock_ext_normal[] = {
    {0x03, 1},
    {0x6D, 1},
    {0xFFFF},
};

StockEntry stock_ext_sw500[] = {
    {0x37, 1},
    {0xFFFF},
};

StockEntry stock_ext_tompson[] = {
    {0x34, 1},
    {0xFFFF},
};

PriceEntry g_item_price_tbl[] = {
    {0x7F, 6000, 1}, {0x7E, 4000, 1}, {0x7D, 3000, 1}, {0xFE, 6000, 1},
    {0xA9, 1000, 1}, {0x54, 1000, 1}, {0x55, 1000, 1}, {0x04, 10, 1},
    {0x18, 30, 1}, {0x00, 100, 1}, {0x07, 30, 1}, {0x20, 6, 1},
    {0x46, 100, 1}, {0x6A, 10, 1}, {0x1A, 120, 1}, {0x0E, 100, 1},
    {0x02, 200, 1}, {0x01, 400, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x40, 0, 1}, {0x21, 1900, 1}, {0x27, 2250, 1}, {0x29, 3200, 1},
    {0x2A, 6300, 1}, {0x03, 6850, 1}, {0x37, 0, 1}, {0x2C, 1820, 1},
    {0x94, 2990, 1}, {0x2D, 3240, 1}, {0x2E, 1050, 1}, {0x2F, 3200, 1},
    {0x30, 1320, 1}, {0x36, 2300, 1}, {0x34, 65535, 1}, {0x35, 3000, 1},
    {0x17, 6000, 1}, {0x6D, 65535, 1}, {0x42, 400, 1}, {0x43, 400, 1},
    {0x44, 700, 1}, {0x45, 1000, 1}, {0xAA, 800, 1}, {0xC5, 2000, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x95, 150, 1},
    {0x97, 460, 1}, {0x06, 100, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x16, 900, 1},
    {0xA8, 1000, 1}, {0x15, 2000, 1}, {0x05, 1000, 1}, {0x57, 200, 1},
    {0x77, 1000, 1}, {0x58, 1000, 1}, {0x89, 100, 1}, {0x59, 1000, 1},
    {0x8A, 100, 1}, {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1},
    {0x5D, 1000, 1}, {0x5F, 300, 1}, {0x60, 300, 1}, {0x61, 300, 1},
    {0x5E, 300, 1}, {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1},
    {0x65, 1500, 1}, {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1},
    {0xB9, 100, 1}, {0xBA, 150, 1}, {0xBB, 300, 1}, {0xB8, 450, 1},
    {0xBC, 650, 1}, {0xBD, 700, 1}, {0xBE, 850, 1}, {0xBF, 1100, 1},
    {0xC0, 1300, 1}, {0xC1, 1500, 1}, {0xC2, 3200, 1}, {0xC7, 300, 1},
    {0xC8, 300, 1}, {0xC9, 300, 1}, {0xC6, 300, 1}, {0xCA, 1000, 1},
    {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1}, {0xCE, 1500, 1},
    {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x56, 250, 1}, {0x8F, 850, 1},
    {0x98, 1200, 1}, {0x70, 2000, 1}, {0x93, 1300, 1}, {0x90, 1000, 1},
    {0x91, 1200, 1}, {0x96, 1200, 1}, {0x9B, 1100, 1}, {0x9C, 1300, 1},
    {0x9A, 900, 1}, {0x9D, 2500, 1}, {0x9E, 2700, 1}, {0x9F, 4800, 1},
    {0xA1, 300, 1}, {0xD1, 1500, 1}, {0xD2, 350, 1}, {0xD3, 350, 1},
    {0xD4, 350, 1}, {0xD5, 2000, 1}, {0xD6, 2000, 1}, {0xD7, 2000, 1},
    {0xD8, 2500, 1}, {0xD9, 2500, 1}, {0xDA, 2500, 1}, {0xDB, 3500, 1},
    {0xFFFF},
};


static int g_item_price_tbl_num = sizeof(g_item_price_tbl) / sizeof(g_item_price_tbl[0]);

MerchantCharacter merchantChar;
MerchantData merchantData[MERCHANT_NUM];

// Debug_flg[3] bit test.
// the tests below are kept apart (fold would merge two masks of one lvalue into a single andis.)
static inline u32 chkFlag6C(u32 b)
{
    return pG->Debug_flg[3] & b;
}

// Scenario_flg[1] bit test.
static inline u32 chkFlag51C0(u32 b)
{
    return pG->Scenario_flg[1] & b;
}

// Debug: adds every stage 1 stock and tune table at once.
void merchant_stage1_full()
{
    stockDataAdd(merchantData, stock_r104);
    stockDataAdd(merchantData, stock_r102);
    stockDataAdd(merchantData, stock_r10e_day);
    stockDataAdd(merchantData, stock_r10d);
    stockDataAdd(merchantData, stock_r10e_night);
    stockDataAdd(merchantData, stock_r112);
    stockDataAdd(merchantData, stock_r11c);
    stockDataAdd(merchantData, stock_r11c_after_event);
    stockDataAdd(merchantData, stock_r10f);
    stockDataAdd(merchantData, stock_1st_mission);
    levelDataAdd(merchantData, level_first);
    levelDataAdd(merchantData, level_styer);
    levelDataAdd(merchantData, level_r112);
    levelDataAdd(merchantData, level_r10e_day);
    levelDataAdd(merchantData, level_1st_night);
}

// Debug: adds every stage 2 stock and tune table.
void merchant_stage2_full()
{
    stockDataAdd(merchantData, stock_2st_first);
    stockDataAdd(merchantData, stock_r20f);
    stockDataAdd(merchantData, stock_r229);
    levelDataAdd(merchantData, level_r200);
    levelDataAdd(merchantData, level_r202);
    levelDataAdd(merchantData, level_r204);
    levelDataAdd(merchantData, level_r20b);
    levelDataAdd(merchantData, level_r211);
    levelDataAdd(merchantData, level_r214);
    levelDataAdd(merchantData, level_r220);
    levelDataAdd(merchantData, level_r225);
    levelDataAdd(merchantData, level_r22a);
}

// Debug: adds every stage 3 stock and tune table.
void merchant_stage3_full()
{
    stockDataAdd(merchantData, stock_r301);
    stockDataAdd(merchantData, stock_r305);
    stockDataAdd(merchantData, stock_3st_general);
    levelDataAdd(merchantData, level_r301);
    levelDataAdd(merchantData, level_r305);
    levelDataAdd(merchantData, level_r31a);
    levelDataAdd(merchantData, level_r31d);
    levelDataAdd(merchantData, level_r329);
}

// New game: favor 50, no discount, empty stock and tune tables.
void MerchantGameInit()
{
    int i;

    for (i = 0; i < MERCHANT_NUM; i++) {
        MerchantData* d = &merchantData[i];
        d->friendship = 50;
        d->study_num = 0;
        d->reduction_ratio = 0;
        d->bonus_flag = 0;
    }
    stockDataInit(merchantData);
    levelDataInit(merchantData);
    merchantChar.setChar(0, 0, 0, 0, 0);
    if (chkFlag6C(0x00800000) || chkFlag6C(0x00040000)) {
        if (pG->stage_no > 1) {
            merchant_stage1_full();
        }
        if (pG->stage_no > 2) {
            merchant_stage2_full();
        }
    }
}

// Second round (cleared game): adds the extra weapons/tunes (level_ext_normal / stock_ext_normal)
// and the extra price tables.
void Merchant2ndRoundInit()
{
    levelDataAdd(merchantData, level_ext_normal);
    stockDataAdd(merchantData, stock_ext_normal);
    merchantChar.setChar(&merchant_info_A, merchantData, sell_price_ext, exer_price_ext, level_price);
}

// Room entry: builds the merchant's stock for the current room. On a cleared game (game_cnt != 0)
// only the unlock extras are added (Handcannon with unlock_flg 0x20000000, Chicago Typewriter with
// 0x10000000). Otherwise the stage's tables are added room by room as the scenario progresses
// (each stock/level table the first time its room is passed, RoomData.checkPassed), plus the
// debug "everything" mode (Debug_flg[3] bit 4). Finally selects merchant_info_A and the price tables.
void MerchantRoomInit()
{
    if (pG->game_cnt != 0) {
        if (ExtFlagChk(pSys, EXT_GET_SW500)) {
            levelDataAdd(merchantData, level_ext_sw500);
            stockDataAdd(merchantData, stock_ext_sw500);
        }
        if (ExtFlagChk(pSys, EXT_GET_TOMPSON)) {
            levelDataAdd(merchantData, level_ext_tompson);
            stockDataAdd(merchantData, stock_ext_tompson);
        }
        merchantChar.setChar(&merchant_info_A, merchantData, g_item_price_tbl, g_item_price_tbl, level_price);
        return;
    }
    merchantChar.setChar(0, 0, 0, 0, 0);
    switch (pG->room_id) {
    case 0x104:
        if (!RoomData.checkPassed(0x104, 0)) {
            levelDataAdd(merchantData, level_first);
            stockDataAdd(merchantData, stock_r104);
        }
        break;
    case 0x102:
        if (!RoomData.checkPassed(0x102, 0)) {
            levelDataAdd(merchantData, level_styer);
            stockDataAdd(merchantData, stock_r102);
        }
        break;
    case 0x10D:
        if (!RoomData.checkPassed(0x10D, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_r10d);
        }
        break;
    case 0x10F:
        if (!RoomData.checkPassed(0x10F, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_r10f);
        }
        break;
    case 0x112:
        if (!RoomData.checkPassed(0x112, 0)) {
            levelDataAdd(merchantData, level_r112);
            stockDataAdd(merchantData, stock_r112);
        }
        break;
    case 0x11C:
        break;
    }
    if (pG->room_id == 0x10E) {
        if (ScfFlagChk(pG, SCF_ST1_NIGHT)) {
            if (!ScfFlagChk(pG, SCF_R10E_STOCK_NIGHT)) {
                levelDataAdd(merchantData, level_null);
                stockDataAdd(merchantData, stock_r10e_night);
                ScfFlagOn(pG, SCF_R10E_STOCK_NIGHT);
            }
        } else {
            if (!ScfFlagChk(pG, SCF_R10E_STOCK_DAY)) {
                levelDataAdd(merchantData, level_r10e_day);
                stockDataAdd(merchantData, stock_r10e_day);
                ScfFlagOn(pG, SCF_R10E_STOCK_DAY);
            }
        }
    }
    if (chkFlag51C0(0x01000000) && !chkFlag51C0(0x2000)) {
        levelDataAdd(merchantData, level_1st_night);
        ScfFlagOn(pG, SCF_ST1_NIGHT_LV_ADD);
    }
    switch (pG->room_id) {
    case 0x200:
        break;
    case 0x202:
        if (!RoomData.checkPassed(0x202, 0)) {
            levelDataAdd(merchantData, level_r202);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x204:
        if (!RoomData.checkPassed(0x204, 0)) {
            levelDataAdd(merchantData, level_r204);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x20B:
        if (!RoomData.checkPassed(0x20B, 0)) {
            levelDataAdd(merchantData, level_r20b);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x20F:
        if (!RoomData.checkPassed(0x20F, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_r20f);
        }
        break;
    case 0x211:
        if (!RoomData.checkPassed(0x211, 0)) {
            levelDataAdd(merchantData, level_r211);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x214:
        if (!RoomData.checkPassed(0x214, 0)) {
            levelDataAdd(merchantData, level_r214);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x229:
        if (!RoomData.checkPassed(0x229, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_r229);
        }
        break;
    case 0x220:
        if (!RoomData.checkPassed(0x220, 0)) {
            levelDataAdd(merchantData, level_r220);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x225:
        if (!RoomData.checkPassed(0x225, 0)) {
            levelDataAdd(merchantData, level_r225);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x227:
        if (!RoomData.checkPassed(0x227, 0)) {
            levelDataAdd(merchantData, level_r227);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x22A:
        if (!RoomData.checkPassed(0x22A, 0)) {
            levelDataAdd(merchantData, level_r22a);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    }
    switch (pG->room_id) {
    case 0x301:
        if (!RoomData.checkPassed(0x301, 0)) {
            levelDataAdd(merchantData, level_r301);
            stockDataAdd(merchantData, stock_r301);
        }
        break;
    case 0x305:
        if (!RoomData.checkPassed(0x305, 0)) {
            levelDataAdd(merchantData, level_r305);
            stockDataAdd(merchantData, stock_r305);
        }
        break;
    case 0x30A:
        if (!RoomData.checkPassed(0x30A, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x31A:
        if (!RoomData.checkPassed(0x31A, 0)) {
            levelDataAdd(merchantData, level_r31a);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x30F:
    case 0x312:
        if (!RoomData.checkPassed(pG->room_id, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x315:
        if (!RoomData.checkPassed(0x315, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x31D:
        if (!RoomData.checkPassed(0x31D, 0)) {
            levelDataAdd(merchantData, level_r31d);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x329:
        if (!RoomData.checkPassed(0x329, 0)) {
            levelDataAdd(merchantData, level_r329);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x331:
        if (!RoomData.checkPassed(0x331, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    }
    if (DbgFlagChk(pG, DBG_SHOP_FULL)) {
        stockDataInit(merchantData);
        levelDataInit(merchantData);
        merchantChar.setChar(0, 0, 0, 0, 0);
        merchant_stage1_full();
        merchant_stage2_full();
        merchant_stage3_full();
    }
    merchantChar.setChar(&merchant_info_A, merchantData, g_item_price_tbl, g_item_price_tbl, level_price);
}

// Size of the merchant save block.
int MerchantDataSize()
{
    return sizeof(MerchantData) * MERCHANT_NUM;
}

// Copies the merchant data (stock, tunes, favor, discount) into the save block.
void MerchantDataSave(void* dst)
{
    MerchantData* p = (MerchantData*) dst;
    int i;

    for (i = 0; i < MERCHANT_NUM; i++) {
        *p++ = merchantData[i];
    }
}

// Restores the merchant data from the save block.
void MerchantDataLoad(void* src)
{
    MerchantData* p = (MerchantData*) src;
    int i;

    for (i = 0; i < MERCHANT_NUM; i++) {
        merchantData[i] = *p++;
    }
}

// Empties the stock table (all ids 0xFFFF).
void stockDataInit(MerchantData* p_data)
{
    StockEntry* s = p_data->stock.e;
    int i;

    memclr_asm(p_data->stock.e, sizeof(STOCK_INFO));
    for (i = 0; i < STOCK_MAX; i++, s++) {
        s->id = 0xFFFF;
    }
}

// Adds a table entry's count to a stock slot: weapons/parts are single (1), stackables add num;
// the 0x35 (Rocket Launcher special) stays single outside Japan.
void add_stock(StockEntry* dst, StockEntry* src)
{
    ItemInfo info;

    itemInfo(src->id, &info);
    switch (info.type) {
    case 1:
    case 9:
        dst->num = 1;
        break;
    case 3:
        if (src->id == 0x35 && pSys->language != 0) {
            dst->num = 1;
        } else {
            dst->num += src->num;
        }
        break;
    default:
        dst->num += src->num;
        break;
    }
}

// Merges a stock table into the merchant's stock: clears the "new" marks, then adds to existing
// slots or appends new ones (marked new). Logs when the 64-slot table is full.
void stockDataAdd(MerchantData* d, StockEntry* tbl)
{
    StockEntry* s;
    int i;

    for (s = d->stock.e, i = 0; i < STOCK_MAX && s->id != 0xFFFF; i++, s++) {
        s->isNew = 0;
    }
    for (; tbl->id != 0xFFFF; tbl++) {
        int found;
        int i;

        s = d->stock.e;
        found = 0;
        for (i = 0; i < STOCK_MAX; i++, s++) {
            if (s->id == 0xFFFF) {
                break;
            }
            if (s->id == tbl->id) {
                add_stock(s, tbl);
                found = 1;
                break;
            }
        }
        if (!found) {
            int added;
            int i;

            s = d->stock.e;
            added = 0;
            for (i = 0; i < STOCK_MAX; i++, s++) {
                if (s->id == 0xFFFF) {
                    s->id = tbl->id;
                    add_stock(s, tbl);
                    s->isNew = 1;
                    added = 1;
                    break;
                }
            }
            if (!added) {
                pLog->err(0, 0, "stockDataAdd(): lack of stock table");
            }
        }
    }
}

// Empties the tune (level) table.
void levelDataInit(MerchantData* p_data)
{
    LevelEntry* l = p_data->level.e;
    int i;

    memclr_asm(p_data->level.e, sizeof(LEVEL_INFO));
    for (i = 0; i < LEVEL_MAX; i++, l++) {
        l->id = 0xFFFF;
    }
}

// Merges a tune table: raises the max level per type of existing weapons (marking them new when
// something rose) or appends new weapons. Logs when the 32-slot table is full.
void levelDataAdd(MerchantData* d, LevelEntry* tbl)
{
    LevelEntry* l;
    int j;
    int i;

    for (l = d->level.e, i = 0; i < LEVEL_MAX && l->id != 0xFFFF; i++, l++) {
        l->isNew = 0;
    }
    for (; tbl->id != 0xFFFF; tbl++) {
        int found;
        int i;

        l = d->level.e;
        found = 0;
        for (i = 0; i < LEVEL_MAX; i++, l++) {
            if (l->id == 0xFFFF) {
                break;
            }
            if (l->id == tbl->id) {
                for (j = 0; j < 4; j++) {
                    if (l->lv[j] < tbl->lv[j]) {
                        l->isNew = 1;
                        l->lv[j] = tbl->lv[j];
                    }
                }
                found = 1;
                break;
            }
        }
        if (!found) {
            int added;
            int i;

            l = d->level.e;
            added = 0;
            for (i = 0; i < LEVEL_MAX; i++, l++) {
                if (l->id == 0xFFFF) {
                    l->id = tbl->id;
                    for (j = 0; j < 4; j++) {
                        l->lv[j] = tbl->lv[j];
                    }
                    l->isNew = 1;
                    added = 1;
                    break;
                }
            }
            if (!added) {
                pLog->err(0, 0, "levelDataAdd(): lack of level table");
            }
        }
    }
}

// Binds the merchant personality, data block, selling and buying (exercise) price tables and the
// tune price table.
void MerchantCharacter::setChar(MerchantInfo* info, MerchantData* data, PriceEntry* sell, PriceEntry* exer, LevelPrice* lvup)
{
    m_p_info = info;
    m_p_data = data;
    m_p_sell = sell;
    m_p_exer = exer;
    m_p_lvup = lvup;
}

// Shop session object (sub screen): copies the character's tables and loads its data.
Merchant::Merchant(MerchantCharacter* c)
{
    m_p_info = c->m_p_info;
    m_p_sell = c->m_p_sell;
    m_p_exer = c->m_p_exer;
    m_p_lvup = c->m_p_lvup;
    memclr_asm(&m_stock, sizeof(STOCK_INFO));
    memclr_asm(&level, sizeof(LEVEL_INFO));
    load(c->m_p_data);
}

// Writes the session's stock/tune/favor/discount back into the merchant data.
void Merchant::save(MerchantData* p_data)
{
    p_data->stock = m_stock;
    p_data->level = level;
    p_data->friendship = m_friendship;
    p_data->study_num = m_study_num;
    p_data->reduction_ratio = m_reduction_ratio;
    p_data->bonus_flag = m_bonus_flag;
}

// Loads the session from the merchant data.
void Merchant::load(MerchantData* p_data)
{
    if (p_data == 0) {
        pLog->err(0, 0, "Merchant::load() Data is empty.");
        return;
    }
    m_stock = p_data->stock;
    level = p_data->level;
    m_friendship = p_data->friendship;
    m_study_num = p_data->study_num;
    m_reduction_ratio = p_data->reduction_ratio;
    m_bonus_flag = p_data->bonus_flag;
}

// Stock slot of item `id` (0 when not stocked).
StockEntry* Merchant::stockPtr(u16 id)
{
    StockEntry* s;

    for (s = m_stock.e;; s++) {
        if (s->id == 0xFFFF) {
            return 0;
        }
        if (id == s->id) {
            break;
        }
    }
    return s;
}

// Adds num to the stock of `id` (not for unlimited/-1 or unavailable/-2 entries).
void Merchant::stockAdd(u16 id, int num)
{
    StockEntry* s = stockPtr(id);

    if (s && s->num != -2 && s->num != -1) {
        s->num += num;
    }
}

// Removes num from the stock when enough is there.
void Merchant::stockSub(u16 id, int num)
{
    StockEntry* s = stockPtr(id);

    if (s && s->num != -2 && s->num != -1 && s->num >= num) {
        s->num -= num;
    }
}

// Pieces for sale of `id`: weapons/parts and the special guns 0x38/0x35 are 1 while the player does
// not own one, the attache cases 0x7D..0x7F/0xFE depend on the case already owned (and costume
// for 0xFE), otherwise the stock count (-1 = 1000, -2/absent = 0).
int Merchant::stockNum(u16 id)
{
    ItemInfo info;
    StockEntry* s = stockPtr(id);

    itemInfo(id, &info);
    if (info.type == 1) {
        return ItemMgr.search(id) == 0;
    }
    itemInfo(id, &info);
    if (info.type == 9) {
        return ItemMgr.search(id) == 0;
    }
    if (id == 0x38 || id == 0x35) {
        return ItemMgr.search(id) == 0;
    }
    switch (id) {
    case 0xFE:
        if (ItemMgr.num(0xFE) != 0) {
            return 0;
        }
        return pG->pl_costume != 2 && pG->pl_costume != 3;
    case 0x7D:
        if (ItemMgr.num(0x7F) != 0 || ItemMgr.num(0x7E) != 0 || ItemMgr.num(0x7D) != 0) {
            return 0;
        }
        return 1;
    case 0x7E:
        if (ItemMgr.num(0x7F) != 0 || ItemMgr.num(0x7E) != 0) {
            return 0;
        }
        return 1;
    case 0x7F:
        return ItemMgr.num(0x7F) == 0;
    }
    if (s == 0 || s->num == -2) {
        return 0;
    }
    if (s->num == -1) {
        return 1000;
    }
    return s->num;
}

// 1 when item `id` was added to the stock since the last visit.
int Merchant::stockNew(u16 id)
{
    StockEntry* s = stockPtr(id);

    if (s && s->isNew) {
        return 1;
    }
    return 0;
}

// 1 when any stock item is new (the "new" mark on the Buy menu).
int Merchant::stockNew()
{
    StockEntry* s;

    for (s = m_stock.e; s->id != 0xFFFF; s++) {
        if (s->isNew) {
            return 1;
        }
    }
    return 0;
}

// Tune entry of weapon `id`, only while the weapon (or for 0x21 the Punisher/0x40) is in stock.
LevelEntry* Merchant::levelPtr(u16 id)
{
    LevelEntry* l = level.e;

    if (id == 0x21) {
        if (stockPtr(0x21) == 0 && stockPtr(0x40) == 0) {
            return 0;
        }
    } else {
        if (stockPtr(id) == 0) {
            return 0;
        }
    }
    for (;; l++) {
        if (l->id == 0xFFFF) {
            return 0;
        }
        if (id == l->id) {
            break;
        }
    }
    return l;
}

// 1 when weapon `id` got a new tune level since the last visit.
int Merchant::levelNew(u16 id)
{
    LevelEntry* l = levelPtr(id);

    if (l && l->isNew) {
        return 1;
    }
    return 0;
}

// 1 when any tune is new (the "new" mark on the Tune-up menu).
int Merchant::levelNew()
{
    LevelEntry* l;

    for (l = level.e; l->id != 0xFFFF; l++) {
        if (l->isNew) {
            if (l->id == 0x21) {
                if (stockPtr(0x21) || stockPtr(0x40)) {
                    return 1;
                }
            } else if (stockPtr(l->id)) {
                return 1;
            }
        }
    }
    return 0;
}

// Max tune level currently offered for weapon `id` and stat `type` (1 when not offered).
s8 Merchant::levelMax(u16 id, int type)
{
    LevelEntry* l = levelPtr(id);

    if (l != 0) {
        return l->lv[type];
    }
    return 1;
}

// 1 when the merchant offers the exclusive (special) upgrade: a level above the weapon's normal max.
int Merchant::stockSpecial(ITEM_ID id)
{
    if (levelMax(id, 0) > WeaponId2MaxLevel(id, 0) || levelMax(id, 1) > WeaponId2MaxLevel(id, 1) ||
        levelMax(id, 2) > WeaponId2MaxLevel(id, 2) || levelMax(id, 3) > WeaponId2MaxLevel(id, 3)) {
        return 1;
    }
    return 0;
}

// 1 when the weapon is at every normal max level and the exclusive upgrade is offered.
int Merchant::specialTunable(ItemWork* p_item)
{
    if (stockSpecial(p_item->id) != 0 && p_item->getPowerLevel() + 1 == WeaponId2MaxLevel(p_item->id, 0) &&
        p_item->getSpeedLevel() + 1 == WeaponId2MaxLevel(p_item->id, 1) && p_item->getReloadLevel() + 1 == WeaponId2MaxLevel(p_item->id, 2) &&
        p_item->getBulletLevel() + 1 == WeaponId2MaxLevel(p_item->id, 3)) {
        return 1;
    }
    return 0;
}

// 1 when the weapon already has its exclusive upgrade.
int Merchant::specialTuned(ItemWork* p_item)
{
    if (p_item->getPowerLevel() + 1 > WeaponId2MaxLevel(p_item->id, 0) || p_item->getSpeedLevel() + 1 > WeaponId2MaxLevel(p_item->id, 1) ||
        p_item->getReloadLevel() + 1 > WeaponId2MaxLevel(p_item->id, 2) || p_item->getBulletLevel() + 1 > WeaponId2MaxLevel(p_item->id, 3)) {
        return 1;
    }
    return 0;
}

// 1 when the weapon can still be tuned here (below an offered max, or the exclusive is available).
int Merchant::tunable(ItemWork* p_item)
{
    if (p_item == 0) {
        return 0;
    }
    if (stockSpecial(p_item->id) == 0) {
        if (p_item->getPowerLevel() + 1 >= levelMax(p_item->id, 0) && p_item->getSpeedLevel() + 1 >= levelMax(p_item->id, 1) &&
            p_item->getReloadLevel() + 1 >= levelMax(p_item->id, 2) && p_item->getBulletLevel() + 1 >= levelMax(p_item->id, 3)) {
            return 0;
        }
    } else {
        if (specialTuned(p_item) == 1) {
            return 0;
        }
    }
    return 1;
}

// Rebuilds the Buy and Sell lists.
void Merchant::makeList()
{
    m_sell_tbl_num = makeSellingList();
    m_exer_tbl_num = makeExerciseList();
}

// Special availability of Buy items: the Infinite Launcher 0x40 only after the game is cleared and
// not yet bought (Item_flg[0] 0x10000000); a few ids never.
int checkSellingItem(ITEM_ID id)
{
    int ret = 1;

    switch (id) {
    case 0x40:
        if (ScfFlagChk(pG, SCF_ST1_SUB_MISSION)) {
            u32 sold = ItfFlagChk(pG, ITF_FN57);
            ret = sold == 0;
        } else {
            ret = 0;
        }
        break;
    case 0xC5:
        ret = 0;
        break;
    }
    return ret;
}

// Fills sellingList with the indices of price table entries that are stocked and allowed.
int Merchant::makeSellingList()
{
    PriceEntry* p = m_p_sell;
    int n;
    int i;

    for (i = 0; i < LIST_MAX; i++) {
        sellingList[i] = 0;
    }
    n = 0;
    for (i = 0; i < g_item_price_tbl_num; i++, p++) {
        if (checkSellingItem(p->id) && stockPtr(p->id)) {
            sellingList[n] = i;
            n++;
        }
    }
    return n;
}

// Number of items on the Buy list.
u8 Merchant::sellingItemNum()
{
    return m_sell_tbl_num;
}

// Price entry of Buy list row `no`.
PriceEntry* Merchant::sellingItemNo(int no)
{
    return &m_p_sell[sellingList[no]];
}

// Selling price entry of item `id`.
PriceEntry* Merchant::sellingItemId(u16 id)
{
    PriceEntry* p = m_p_sell;

    if (p->id != 0xFFFF) {
        do {
            if (p->id == id) {
                return p;
            }
            p++;
        } while (p->id != 0xFFFF);
    }
    return 0;
}

// Special availability of Sell items (a few ids cannot be sold).
int checkExerciseItem(ITEM_ID id)
{
    int ret = 1;

    switch (id) {
    case 0x54:
    case 0x55:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
    case 0xA9:
        ret = 0;
        break;
    }
    return ret;
}

// Fills exerciseList with the player's inventory slots that have a buying price (weapons listed
// per slot by descending count); returns the count.
int Merchant::makeExerciseList()
{
    PriceEntry* p = m_p_exer;
    int n;
    int j;
    ItemInfo info;

    for (int i = 0; i < LIST_MAX; i++) {
        exerciseList[i] = 0;
    }
    n = 0;
    for (int i = 0; i < g_item_price_tbl_num; i++, p++) {
        ItemWork* item = ItemMgr.search(p->id);

        if (item == 0) {
            continue;
        }
        if (checkExerciseItem(p->id) == 0) {
            continue;
        }
        itemInfo(p->id, &info);
        if (info.type == 1) {
            ItemMgr.ordering(p->id);
            if (ItemMgr.m_order_tbl_num > 0) {
                for (j = 0; j < ItemMgr.m_order_tbl_num; j++) {
                    exerciseList[n] = ItemMgr.searchAt(ItemMgr.m_p_order_tbl[j].p_item);
                    n++;
                }
            }
        } else {
            exerciseList[n] = ItemMgr.searchAt(item);
            n++;
        }
    }
    return n;
}

// Number of items on the Sell list.
u8 Merchant::exerciseItemNum()
{
    return m_exer_tbl_num;
}

// Inventory slot of Sell list row `no`.
ItemWork* Merchant::exerciseItemPtr(int no)
{
    return ItemMgr.at(exerciseList[no]);
}

// Buying price entry of Sell list row `no`.
PriceEntry* Merchant::exerciseItemNo(int no)
{
    return exerciseItemId(ItemMgr.at(exerciseList[no])->id);
}

// Buying price entry of item `id`.
PriceEntry* Merchant::exerciseItemId(u16 id)
{
    PriceEntry* p = m_p_exer;

    if (p->id != 0xFFFF) {
        do {
            if (p->id == id) {
                return p;
            }
            p++;
        } while (p->id != 0xFFFF);
    }
    return 0;
}

// What the merchant pays for num of `id`: table price x10 per piece, full for treasures, half for
// weapons/ammo/grenades/the case, 90% otherwise.
int Merchant::buyupPrice(u16 id, int num)
{
    ItemInfo info;
    PriceEntry* p;
    int price;
    int n;
    int type;

    // The 0.5 arm is written three times (one per test group) and the last test as
    // `id == 0xFE`: jump1 swaps that arm ahead of the 0.9 arm, so all three 0.5 loads sit
    // on a cse1 path from block 0 and share the `half` declaration's high (4 refs: no
    // update_equiv_regs move, global gives it r29); jump2 cross-jumps the three arms into
    // one. Declaring `half`/`nine` first keeps the pool order [string][0.5][0.9][0x4330].
    const f32 half = 0.5f;
    const f32 nine = 0.9f;
    p = exerciseItemId(id);
    if (p == 0) {
        pLog->err(0, 0, "buyupPriece() : 0x%02x not found", id);
        return 0;
    }
    n = num * 10;
    price = p->price * n;
    itemInfo(id, &info);
    type = info.type;
    if (type == 5 || type == 0xC) {
        return (int) ((f32) price * 1.0f);
    }
    if (type == 1 || type == 2) {
        return (int) ((f32) price * half);
    }
    if (type == 3 || type == 6) {
        return (int) ((f32) price * half);
    }
    if (id == 0xFE) {
        return (int) ((f32) price * half);
    }
    return (int) ((f32) price * nine);
}

// Buying price of an inventory slot: the item plus, for a weapon, its loaded ammo and half of every
// tune level bought.
int Merchant::buyupPrice(ItemWork* item, int num)
{
    ItemInfo info;
    int price = buyupPrice(item->id, num);
    const f32 rate = 0.5f; // pool entry before the 0x4330 magic; the literal is folded at every use
    int type;
    int lv;

    itemInfo(item->id, &info);
    if (info.type == 1 && num == 1) {
        price += buyupPrice(WeaponId2BulletId(item->id, item->getBulletType()), item->bullet & 0x1FFF);
        for (type = 0; type <= 3; type++) {
            int lvMax = 0;

            switch (type) {
            case 0:
                lvMax = item->getPowerLevel() + 1;
                break;
            case 1:
                lvMax = item->getSpeedLevel() + 1;
                break;
            case 2:
                lvMax = item->getReloadLevel() + 1;
                break;
            case 3:
                lvMax = item->getBulletLevel() + 1;
                break;
            }
            for (lv = 2; lv <= lvMax; lv++) {
                price += (int) ((f32) levelupPrice(item, type, lv) * rate);
            }
            // COMPILER-DIFF: candidate (loop.c pass-2 insn_count). The original's outer loop had
            // >= 72 real insns at the second loop pass, so the two pool `lis` hoisted into the
            // lv-loop preheader by pass 1 stay there (threshold 71 * 1 * 1 < insn_count); ours has
            // 65 and hoists them to the function top. The two dead tests (num is dead after the
            // `num == 1` test) add the missing insns and vanish in flow/jump2.
            if (price == 0) {
                num = 0;
            }
            if (item->bullet == 0) {
                num = 1;
            }
        }
    }
    return price;
}

// Sells a slot to the merchant: adds the price to *money, returns the item (and a weapon's ammo)
// to the stock and raises favor by shift_Buyup.
int Merchant::buyup(ItemWork* p_item, int num, int* pocket)
{
    ItemInfo ii;

    *pocket += buyupPrice(p_item, num);
    stockAdd(p_item->id, num);
    itemInfo(p_item->id, &ii);
    if (ii.type == 1 && num == 1) {
        stockAdd(WeaponId2BulletId(p_item->id, p_item->getBulletType()), p_item->bullet & 0x1FFF);
    }
    m_friendship += m_p_info->shift_Buyup;
    m_friendship = m_friendship < 0 ? 0 : (m_friendship > 100 ? 100 : m_friendship);
    return 1;
}

// Price the player pays for num of `id`: table price x10 per piece minus the discount; a weapon
// includes one magazine of ammo; 0x40/0x37 cost 1,000,000.
int Merchant::sellPrice(u16 id, int num)
{
    ItemInfo info;
    PriceEntry* p = sellingItemId(id);
    f32 rate = 1.0f - (f32) m_reduction_ratio / 100.0f;
    int price;
    int n;

    if (p == 0) {
        pLog->err(0, 0, "sellPriece() : 0x%02x not found", id);
        return 0;
    }
    n = num * 10;
    price = p->price * n;
    if (id == 0x40 || id == 0x37) {
    } else if (id == 0x6D || id == 0x34) {
        price = 1000000;
    } else {
        itemInfo(id, &info);
        if (info.type == 1 && num == 1) {
            u16 bid = WeaponId2BulletId(id, 0);
            int m = WeaponId2ChargeNum(id, 1);
            p = exerciseItemId(bid);

            if (p) {
                int n2 = m * 10;
                price += p->price * n2;
            } else {
                pLog->err(0, 0, "sellPrice() : 0x%02x not found", id);
            }
        }
    }
    return (int) ((f32) price * rate);
}

// Pieces per purchase of `id`.
int Merchant::sellUnit(u16 id)
{
    PriceEntry* p = sellingItemId(id);

    if (p == 0) {
        pLog->err(0, 0, "sellUnit() : 0x%02x not found", id);
        return 0;
    }
    return p->unit;
}

// Buys num of `id` when *money suffices: takes the price, decrements the stock (and a weapon's
// magazine of ammo), marks the Infinite Launcher as bought, raises favor (sellFavorBig above the
// threshold) and clears the discount. Returns 1 on success.
int Merchant::sell(u16 id, int num, int* pocket)
{
    ItemInfo ii;
    int price = sellPrice(id, num);
    int point = (int) ((f32) price * 0.02f);

    if (*pocket >= price) {
        if (id == 0x40) {
            ItfFlagOn(pG, ITF_FN57);
        }
        *pocket -= price;
        stockSub(id, num);
        itemInfo(id, &ii);
        if (ii.type == 1 && num != 0) {
            u16 bid = WeaponId2BulletId(id, 0);
            stockSub(bid, WeaponId2ChargeNum(id, 1));
        }
        if (point >= m_p_info->threshold) {
            m_friendship += m_p_info->sellFavorBig;
        } else {
            m_friendship += m_p_info->sellFavor;
        }
        m_friendship = m_friendship < 0 ? 0 : (m_friendship > 100 ? 100 : m_friendship);
        m_reduction_ratio = 0;
        return 1;
    }
    return 0;
}

// Percent -> fraction (unused helper).
// Dead-stripped in the original (STRIP_UNUSED): only its constant pool (0.0f, 0.01f) survives after sell's.
static f32 merchant_dead_rate(f32 rate)
{
    if (rate != 0.0f) {
        rate = rate * 0.01f;
    }
    return rate;
}

// Number of rows of the Tune-up list (one per owned slot of each tunable weapon).
int Merchant::levelupItemNum()
{
    LevelEntry* l = level.e;
    int n = 0;

    while (l->id != 0xFFFF) {
        if (levelPtr(l->id) == 0) {
            l++;
            continue;
        }
        ItemMgr.ordering(l->id);
        if (ItemMgr.m_order_tbl_num > 0) {
            n += ItemMgr.m_order_tbl_num;
        } else {
            n++;
        }
        l++;
    }
    return n;
}

// Tune entry of Tune-up row `no`.
LevelEntry* Merchant::levelupItemNo(int no)
{
    LevelEntry* l = level.e;
    int cnt = 0;
    int j;

    while (l->id != 0xFFFF) {
        if (levelPtr(l->id) == 0) {
            l++;
            continue;
        }
        ItemMgr.ordering(l->id);
        if (ItemMgr.m_order_tbl_num > 0) {
            for (j = 0; j < ItemMgr.m_order_tbl_num; j++) {
                if (cnt == no) {
                    return l;
                }
                cnt++;
            }
        } else {
            if (cnt == no) {
                return l;
            }
            cnt++;
        }
        l++;
    }
    return 0;
}

// Inventory slot of Tune-up row `no` (0 when the weapon is offered but not owned).
ItemWork* Merchant::levelupItemPtr(int no)
{
    LevelEntry* l = level.e;
    int cnt = 0;
    int j;

    while (l->id != 0xFFFF) {
        if (levelPtr(l->id) == 0) {
            l++;
            continue;
        }
        ItemMgr.ordering(l->id);
        if (ItemMgr.m_order_tbl_num > 0) {
            for (j = 0; j < ItemMgr.m_order_tbl_num; j++) {
                if (cnt == no) {
                    return ItemMgr.m_p_order_tbl[j].p_item;
                }
                cnt++;
            }
        } else {
            if (cnt == no) {
                return 0;
            }
            cnt++;
        }
        l++;
    }
    return 0;
}

// Tune price table entry of weapon `id`.
LevelPrice* Merchant::levelupItemPrice(u16 id)
{
    LevelPrice* p = m_p_lvup;

    if (p->id != 0xFFFF) {
        do {
            if (p->id == id) {
                return p;
            }
            p++;
        } while (p->id != 0xFFFF);
    }
    return 0;
}

// Price of raising stat `type` of weapon `id` to level lv (2..): table value x10; 0 when not offered.
int Merchant::levelupPrice(u16 id, int type, int lv)
{
    LevelEntry* l = levelPtr(id);
    LevelPrice* p = levelupItemPrice(id);
    int price = 0;

    if (l && lv <= l->lv[type] && p) {
        switch (type) {
        case 0:
            price = p->power[lv - 2];
            break;
        case 1:
            price = p->speed[lv - 2];
            break;
        case 2:
            price = p->reload[lv - 2];
            break;
        case 3:
            price = p->bullet[lv - 2];
            break;
        }
    }
    return price * 10;
}

// levelupPrice for an inventory slot.
int Merchant::levelupPrice(ItemWork* item, int type, int lv)
{
    return levelupPrice(item->id, type, lv);
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
