#ifndef CMANAGER_H
#define CMANAGER_H

#include "types.h"

// Placement new for the managers' construct(): the work is constructed in place.
#ifndef PLACEMENT_NEW_DEFINED
#define PLACEMENT_NEW_DEFINED
inline void* operator new(unsigned int, void* p) { return p; }
#endif

// Base of every managed work object (cLight, cEsp, cObj, cEm, ...).
// GCC 2.95 places the vptr after the fields of the class that introduces it.
class cUnit {
public:
    u32 be_flag;  // 0x0  bit0: alive, bit9/10: reserved-alive bits (0x601 = in use)
    cUnit* pNext;  // 0x4  active list link
    // 0x8 vptr

    cUnit() {}
    // Event::Event stores be_flag before its vptr and member constructors: only a base-class
    // initializer runs there.
    cUnit(u32 flag) { be_flag = flag; }
    virtual ~cUnit() { be_flag &= ~0x601; }
    virtual void beginEvent(u32 mode) {}
    virtual void endEvent(u32 mode) {}
    // Works are pool-managed: `delete work` only runs the destructor (be_flag cleared).
    // size_t is `unsigned int` for this compiler; with u32 (unsigned long) GCC 2.95 would not
    // treat this as the usual deallocation function.
    void operator delete(void*, unsigned int) {}
    // PS2 declares setAlive, isDieRequest and isDieRequest2 but no PS2 code calls them, so these
    // three masks are unconfirmed
    int setAlive() { return be_flag |= 1; }
    int isEmpty() { return !(be_flag & 0x601); }
    // cManager::dieCheck deletes a work with 0x400 set and turns 0x200 into 0x400 for the next pass
    int setDieRequest() { return be_flag |= 0x600; }
    int setDieRequest2() { return be_flag |= 0x200; }
    int isDieRequest() { return be_flag & 0x200; }
    int isDieRequest2() { return be_flag & 0x400; }
    cUnit* getNext() { return pNext; }
    // addListBack's `p->next = 0` goes through this: the argument copy gives the zero register a
    // lifetime of 2 luids, which is what makes loop.c hoist `li rN, 0` out of createBack's loop
    void setNext(cUnit* n) { pNext = n; }
    // deleteList / destroy test the work through this (a derived class may hide it with its own
    // test: cSat adds its active flag, which is why cManager<cSat>::destroy's check differs)
    int isAlive() { return (be_flag & 0x201) == 1; }
};

// Fixed array work manager. Element stride is the runtime field `size`
// (derived work classes share the manager of their base type).
template <class T>
class cManager {
public:
    T* pArray;         // 0x00 work array
    u32 nArray;        // 0x04 number of works
    u32 size;          // 0x08 sizeof one work
    u8 flag;           // 0x0C
    T* pAlive;         // 0x10 head of active list (cUnit::next)
    u32 pAlivePush;     // 0x14  arrayPush (light.cpp keeps its own inSscrn globals): the room's pAlive while a debug tool works on its own array
    u32 pArrayPush;     // 0x18  arrayPush: the room's pArray (0 = not pushed)
    u32 nArrayPush;     // 0x1C  arrayPush: the room's nArray
    u32 maxAlive;      // 0x20 peak active count
    const char* name;  // 0x24
    u32 warnDiv;       // 0x28 countActiveWork() warns when free works < nArray / warnDiv
    void (**funcTbl)(T*);  // 0x2C per-type move handlers, set by init() (cLight::move calls funcTbl[type])
    // 0x30 vptr

    cManager(u32 size, u8 flag);
    // in-class: an out-of-class template definition is instantiated by the derived constructors'
    // base cleanup and lands before setName in light.cpp; the DOL has it after the destructors
    virtual ~cManager() {}
    virtual void* memAlloc(u32 size) = 0;
    virtual void memFree(void* p) = 0;
    virtual void memClear(T* p, u32 size) = 0;
    virtual void log(const char* fmt, ...);
    virtual void destroy(T* pEm);
    virtual int construct(T* pSat, u32 room_no) = 0;  // the id switch trees compare unsigned (cLightMgr, cCtrlMgr)

    void setName(const char* n);
    int roomInit();
    void init(void (**tbl)(T*));
    u32 countActiveWork();
    T* create(int id);
    T* create();
    T* create(int id, u32 no);
    T* createBack(int id);
    void destroyNow(T* p);   // objRocket.cpp instantiates it (pl_wep weaponRelease)
    T* getPrevWork(T* p);
    int dieCheck();
    int arrayAlloc(u32 workNum);   // memFree + memAlloc(size * n) + memClear (em.cpp, game.cpp instantiate them)
    int arrayFree();         // 1 when there was an array
    // Debug print "alive/peak/total" (each minus `sub`) at (x, y) in colour `col`; returns the alive
    // count, 0 with an invalid array (game.cpp gameMainLoop's manager table).
    int dispWorkNum(int x, int y, int col, int sub);
    // Event brackets of every alive work (defined in sce_com.cpp, the only unit instantiating them).
    void beginEvent(int flag);
    void endEvent(int flag);
    // destroy() every alive work (debug tools: db_light LitLoadWork, Sscrn ss_main)
    void destroyAll();
    // Debug tools (tools.cpp ToolArrayPush/ToolWorkPop): park the room's array in pArrayPush/pAlivePush/nArrayPush and work
    // on a fresh Debug_alloc'd one of `n` works; arrayPop frees it and restores the room's. Both
    // return 1 when they did something.
    int arrayPush(int n);
    int arrayPop();

    // Work `no`: at() gives NULL when out of range, fastAt() does not check.
    T* at(u32 no) {
        if (no >= nArray) {
            return 0;
        }
        return (T*)((u8*)pArray + size * no);
    }
    T* fastAt(u32 no) { return (T*)((u8*)pArray + size * no); }
    u32 getArrayNum() { return nArray; }
    // Alive list: the first active work and the one after `p`.
    T* getActiveWork() { return pAlive; }
    T* getNext(T* p) { return (T*)p->pNext; }
    int deleteList(T* p) {
        T* q;
        if (!p->isAlive()) {
            log("%s::deleteList() WORK IS ALREADY DEAD 0x%08X", name, p);
            return 0;
        }
        q = pAlive;
        if (q == p) {
            pAlive = (T*)p->pNext;
            p->pNext = 0;
            return 1;
        }
        for (; q->pNext; q = (T*)q->pNext) {
            if (q->pNext == p) {
                q->pNext = p->pNext;
                p->pNext = 0;
                return 1;
            }
        }
        log("%s::deleteList() WORK CANT FOUND 0x%08X", name, p);
        return 0;
    }
    void addListFront(T* p) {
        T* q;
        for (q = pAlive; q; q = (T*)q->pNext) {
            if (q == p) {
                log("%s::addListFront() ERROR SET x2 0x%08X", name, p);
                return;
            }
        }
        p->pNext = pAlive;
        pAlive = p;
    }
    void addListBack(T* p) {
        T* q;
        for (q = pAlive; q; q = (T*)q->pNext) {
            if (q == p) {
                log("%s::addListBack() ERROR 0x%08X", name, p);
                return;
            }
        }
        q = pAlive;
        if (q == 0) {
            pAlive = p;
            return;
        }
        while (q->pNext) {
            q = (T*)q->pNext;
        }
        q->pNext = p;
        p->setNext(0);
    }
};

// Member initializer list, in this order: the stores come out in this order (body assignments
// would be rescheduled: the last use of the zero register first).
template <class T>
cManager<T>::cManager(u32 size, u8 flag)
    : size(size), flag(flag), maxAlive(0), name("Mgr"), warnDiv(10), pArray(0), nArray(0), pAlive(0), pAlivePush(0), pArrayPush(0), nArrayPush(0)
{
}

template <class T>
void cManager<T>::log(const char* fmt, ...)
{
}

template <class T>
void cManager<T>::setName(const char* n)
{
    name = n;
}

template <class T>
int cManager<T>::roomInit()
{
    pArray = 0;
    nArray = 0;
    pAlive = 0;
    pAlivePush = 0;
    pArrayPush = 0;
    nArrayPush = 0;
    maxAlive = 0;
    return 1;
}

template <class T>
void cManager<T>::init(void (**tbl)(T*))
{
    funcTbl = tbl;
    roomInit();
}

template <class T>
int cManager<T>::arrayFree()
{
    int ret;

    if (pArray) {
        memFree(pArray);
        pArray = 0;
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

template <class T>
int cManager<T>::arrayAlloc(u32 n)
{
    arrayFree();
    pArray = (T*) memAlloc(size * n);
    nArray = n;
    if (n) {
        memClear(pArray, size * n);
    }
    return 1;
}

// Works marked for deletion (flag 1 / 2 destroy): bit 0x400 deletes this frame, 0x200 arms 0x400.
template <class T>
int cManager<T>::dieCheck()
{
    u32 i;
    for (i = 0; i < nArray; i++) {
        T* p = (T*)((u8*)pArray + size * i);
        if (!p->isEmpty()) {
            if (p->be_flag & 0x400) {
                delete p;
                p->be_flag = 0;
            } else if (p->be_flag & 0x200) {
                p->be_flag |= 0x400;
            }
        }
    }
    return 1;
}

template <class T>
u32 cManager<T>::countActiveWork()
{
    u32 n = 0;
    u32 i;
    for (i = 0; i < nArray; i++) {
        T* p = (T*)((u8*)pArray + size * i);
        if (!p->isEmpty()) {
            n++;
        }
    }
    if (n > maxAlive) {
        maxAlive = n;
    }
    if (nArray - n < nArray / warnDiv) {
        log("countActiveWork() warning, %s work remain under 1/%d", name, warnDiv);
    }
    return n;
}

template <class T>
T* cManager<T>::create(int id)
{
    u32 i;
    for (i = 0; i < nArray; i++) {
        T* p = (T*)((u8*)pArray + size * i);
        if (p->isEmpty()) {
            memClear(p, size);
            if (construct(p, id) == 0) {
                log("create()->construct() failed. %s id:%d", name, id);
                return 0;
            }
            addListFront(p);
            countActiveWork();
            return p;
        }
    }
    log("create() failed. %s work is full id:%d", name, id);
    return 0;
}

template <class T>
T* cManager<T>::create()
{
    return create(0);
}

template <class T>
T* cManager<T>::create(int id, u32 no)
{
    if (no >= nArray) {
        return 0;
    }
    T* p = (T*)((u8*)pArray + size * no);
    if (!p->isEmpty()) {
        log("create() failed %s id:%d", name, id);
        return 0;
    }
    memClear(p, size);
    construct(p, id);
    addListFront(p);
    countActiveWork();
    return p;
}

template <class T>
int cManager<T>::arrayPush(int n)
{
    // `if (busy) return 0;` first: the `li r3,0` stays out of line after the body (an `if (free) {..;
    // return 1;} return 0;` gets it hoisted above the branch)
    if (pArrayPush != 0) {
        return 0;
    }
    pArrayPush = (u32) pArray;
    pArray = (T*) Debug_alloc(size * n, 1);
    nArrayPush = nArray;
    nArray = n;
    pAlivePush = (u32) pAlive;
    pAlive = 0;
    return 1;
}

template <class T>
int cManager<T>::arrayPop()
{
    if (pArrayPush == 0) {
        return 0;
    }
    Debug_free(pArray);
    // statement order found by brute force (zero stores last in the schedule, pAlive restored last)
    pArray = (T*) pArrayPush;
    nArray = nArrayPush;
    pArrayPush = 0;
    nArrayPush = 0;
    pAlive = (T*) pAlivePush;
    return 1;
}

template <class T>
void cManager<T>::destroyAll()
{
    u32 i;
    for (i = 0; i < nArray; i++) {
        T* p = (T*)((u8*)pArray + size * i);
        if (p->isAlive()) {
            destroy(p);
        }
    }
}

// Release a work: unlink it from the active list, then run its destructor (flag 0) or only
// mark it (flag 1/2). Called on the manager object itself, so GCC inlines it.
template <class T>
inline void cManager<T>::destroy(T* p)
{
    if ((u32)p < 0x80000000 || (u32)p > 0x82FFFFFF) {
        if (p != 0) {
            log("%s::destroy() ERROR, INVALID  PTR %08X", name, p);
        }
        return;
    }
    deleteList(p);
    switch (flag) {
    case 0:
        if (!p->isAlive()) {
            log("%s::destroy() delete but not alive", name);
            return;
        }
        delete p;
        p->be_flag = 0;
        break;
    case 1:
        p->setDieRequest();
        break;
    case 2:
        p->setDieRequest2();
        break;
    default:
        log("%s::destroy() INVALID ID %d", name, flag);
        break;
    }
}

// The work in front of `p` in the array, or 0 at the front (scroll groups chain works this way).
template <class T>
T* cManager<T>::getPrevWork(T* p)
{
    p = (T*)((u8*)p - size);
    if ((u32)p < (u32)pArray || (u32)p >= (u32)pArray + size * (nArray - 1)) {
        return 0;
    }
    return p;
}

template <class T>
T* cManager<T>::createBack(int id)
{
    int i;
    for (i = nArray - 1; i >= 0; i--) {
        T* p = (T*)((u8*)pArray + size * i);
        if (p->isEmpty()) {
            memClear(p, size);
            if (construct(p, id) == 0) {
                log("create()->construct() failed. %s id:%d", name, id);
                return 0;
            }
            addListBack(p);
            countActiveWork();
            return p;
        }
    }
    log("create() failed. %s work is full id:%d", name, id);
    return 0;
}

#endif
