#ifndef CFLAG_H
#define CFLAG_H

#include "types.h"
#include "db_log.h"

// Flag word of type T addressed by the bit numbers of the enum E (bit n is 1 << n), with
// range-checked access.
template <class T, class E> class cFlag {
    T m_Flag;

public:
    cFlag() { m_Flag = 0; }
    void reset() { m_Flag = 0; }
    cFlag& on(E stat) {
        if (stat > sizeof(T) * 8 - 1) {
            pLog->err(0, 0, "cFlag.set() arg stat OVER FLOW %d", stat);
            return *this;
        }
        m_Flag |= 1 << stat;
        return *this;
    }
    cFlag& off(E stat) {
        if (stat > sizeof(T) * 8 - 1) {
            pLog->err(0, 0, "cFlag.set() arg stat OVER FLOW %d", stat);
            return *this;
        }
        m_Flag &= ~(1 << stat);
        return *this;
    }
    int check(E stat) {
        if (stat > sizeof(T) * 8 - 1) {
            pLog->err(0, 0, "cFlag.set() arg stat OVER FLOW %d", stat);
            return 0;
        }
        return m_Flag & (1 << stat);
    }
    T get() { return m_Flag; }
    void set(T flag) { m_Flag = flag; }
};

#endif
