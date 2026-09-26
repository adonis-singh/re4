#ifndef FLAG_H
#define FLAG_H

#include "types.h"

// Bit flags packed into u32 words, numbered from the top bit: flag `no` is bit 31 - (no & 31) of word no >> 5.

// The per-field tests go through a real inline rather than the FlagChk macro: the flag number
// arrives as a parameter, which changes how sched1 orders the constants that follow the test
// (objWep drawPoint and esp_app EspDrawLaserLine only match this way).
static inline u32 FlagBitChk(u32* flg, u32 no) { return flg[no >> 5] & (0x80000000 >> (no & 31)); }

// Set, clear and toggle, against the same base and index as FlagBitChk.
// TODO: PS2's flag.h had these as inline functions taking the word's address, like FlagBitChk.
#define FlagOn(base, no) (*(u32*) ((((no) >> 5) << 2) + (u32) (base)) |= (0x80000000 >> ((no) & 31)))
#define FlagOff(base, no) (*(u32*) ((((no) >> 5) << 2) + (u32) (base)) &= ~(0x80000000 >> ((no) & 31)))
#define FlagXor(base, no) (*(u32*) ((((no) >> 5) << 2) + (u32) (base)) ^= (0x80000000 >> ((no) & 31)))

// The same four, for a call site whose flag number is a variable or a struct field rather than an
// enumerator.  Both arguments are copied into locals: substituted twice the field would be loaded
// twice, where the original loads it once.  The number keeps the type the call site gives it, so an
// index cast to u32 folds its shift into a single rlwinm where a signed one takes two instructions.
#define FLAG_WORD_VAR(base, no, op) ({ u32 flagBase_ = (u32) (base); __typeof__(no) flagNo_ = (no); \
                                       *(u32*) (((flagNo_ >> 5) << 2) + flagBase_) op; })
#define FlagChkVar(base, no) FLAG_WORD_VAR(base, no, & (0x80000000 >> (flagNo_ & 31)))
#define FlagOnVar(base, no) FLAG_WORD_VAR(base, no, |= (0x80000000 >> (flagNo_ & 31)))
#define FlagOffVar(base, no) FLAG_WORD_VAR(base, no, &= ~(0x80000000 >> (flagNo_ & 31)))
#define FlagXorVar(base, no) FLAG_WORD_VAR(base, no, ^= (0x80000000 >> (flagNo_ & 31)))

#endif
