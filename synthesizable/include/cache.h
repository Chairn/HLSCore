#ifndef CACHE_H
#define CACHE_H

#include "portability.h"

#define NONE                        0
#define FIFO                        1
#define LRU                         2
#define RANDOM                      3

#ifndef Policy
#define Policy                      LRU
#endif

#define N 8192

#ifndef Size
#define Size                        1024    // bytes
#endif

#ifndef Blocksize
#define Blocksize                   (32/sizeof(int))  // 32 bytes
#endif

#ifndef Associativity
#define Associativity               4
#endif

#define Sets                        (Size/(Blocksize*sizeof(int))/Associativity)

#define blockshift                  2
#define setshift                    (ac::log2_ceil<Blocksize>::val + 2)
#define tagshift                    (ac::log2_ceil<Sets>::val + setshift)

#define offmask                     (0x3)
#define blockmask                   ((Blocksize - 1) << 2)
#define setmask                     ((Sets - 1) << setshift)
#define tagmask                     (~(blockmask + setmask + offmask))

#if 1
#define getTag(address)             (address.slc<32-tagshift>(tagshift))
#define getSet(address)             (address.slc<ac::log2_ceil<Sets>::val>(setshift))
#define getOffset(address)          (address.slc<ac::log2_ceil<Blocksize>::val>(2))

#define setTag(address, tag)        (address.set_slc(tagshift, (ac_int<32-tagshift, false>)tag))
#define setSet(address, set)        (address.set_slc(setshift, (ac_int<ac::log2_ceil<Sets>::val, false>)set))
#define setOffset(address, offset)  (address.set_slc(blockshift, (ac_int<ac::log2_ceil<Blocksize>::val, false>)offset))
#else
#define getTag(address)             (address  >> tagshift)
#define getSet(address)             ((address & setmask) >> setshift)
#define getOffset(address)          ((address & blockmask) >> 2)

#define setTag(address, tag)        (address = ((tag) << tagshift) | (address & ~tagmask))
#define setSet(address, set)        (address = ((set) << setshift) | (address & ~setmask))
#define setOffset(address, offset)  (address = ((offset) << blockshift) | (address & ~blockmask))
#endif

enum {
    MemtoCache = 0,
    CachetoMem = 1
};

enum {
    Idle         = 0,
    StoreControl = 1,
    StoreData    = 2,
    WriteBack    = 3,
    Fetch        = 4,
    States       = 5
};

struct SetControl
{
    int data[Associativity];
    ac_int<32-tagshift, false> tag[Associativity];
    bool dirty[Associativity];
    bool valid[Associativity];
#if Associativity == 1
    //ac_int<1, false> policy;
#else
  #if Policy == FIFO
    ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #elif Policy == LRU
    ac_int<Associativity * (Associativity+1) / 2, false> policy;
  #elif Policy == RANDOM
    //ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #else   // None
    //ac_int<1, false> policy;
  #endif
#endif
};

struct CacheControl
{
    ac_int<32-tagshift, false> tag[Sets][Associativity];
    ac_int<32, false> workAddress;
    bool dirty[Sets][Associativity];
    bool valid[Sets][Associativity];
    ac_int<ac::log2_ceil<States>::val, false> state;
    ac_int<ac::log2_ceil<Blocksize>::val, false> i;
    ac_int<32, false> valuetowrite;
    ac_int<ac::log2_ceil<Sets>::val, false> currentset;
#if Associativity == 1
    ac_int<1, false> currentway;
    //ac_int<1, false> policy[Sets];
#else
    ac_int<ac::log2_ceil<Associativity>::val, false> currentway;
  #if Policy == FIFO
    ac_int<ac::log2_ceil<Associativity>::val, false> policy[Sets];
  #elif Policy == LRU
    ac_int<Associativity * (Associativity-1) / 2, false> policy[Sets];
  #elif Policy == RANDOM
    ac_int<32, false> policy;   //32 bits for the whole cache
  #else   // None alias direct mapped
    //ac_int<1, false> policy[Sets];
  #endif
#endif

    SetControl setctrl;
};

void cache(CacheControl& ctrl, unsigned int dmem[N], unsigned int data[Sets][Blocksize][Associativity],      // control, memory and cachedata
           ac_int<32, false> address, ac_int<2, false> datasize, bool cacheenable, bool writeenable, int writevalue,    // from cpu
           int& read, bool& datavalid                                                       // to cpu
#ifndef __SYNTHESIS__
           , int cycles
#endif
           );


#endif // CACHE_H
