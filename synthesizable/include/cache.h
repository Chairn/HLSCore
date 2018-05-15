#ifndef CACHE_H
#define CACHE_H

#include "portability.h"

#define NONE            0
#define FIFO            1
#define LRU             2
#define RANDOM          3

#ifndef Policy
#define Policy          LRU
#endif

#define N 8192

#ifndef Size
#define Size            1024    // bytes
#endif

#ifndef Blocksize
#define Blocksize       (32/sizeof(int))  // 32 bytes
#endif

#ifndef Associativity
#define Associativity   4
#endif

#define Sets            (Size/(Blocksize*sizeof(int))/Associativity)

#define setshift        (ac::log2_ceil<Blocksize>::val + 2)
#define tagshift        (ac::log2_ceil<Sets>::val + setshift)

#define offmask         (0x3)
#define blockmask       ((Blocksize - 1) << 2)
#define setmask         ((Sets - 1) << setshift)
#define tagmask         (~(blockmask + setmask + offmask))

enum {
    MemtoCache = 0,
    CachetoMem = 1
};

struct SetControl
{
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
    ac_int<32, true> workAddress;
    bool dirty[Sets][Associativity];
    bool valid[Sets][Associativity];
    bool sens;
    bool enable;
    bool storecontrol;
    bool storedata;
    ac_int<ac::log2_ceil<Blocksize>::val, false> i;
    ac_int<32> valuetowrite;
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


void cache(CacheControl& ctrl, int dmem[N], int data[Sets][Associativity][Blocksize],      // control, memory and cachedata
           ac_int<32, true> address, ac_int<2, false> datasize, bool cacheenable, bool writeenable, int writevalue,    // from cpu
           int& read, bool& datavalid                                                       // to cpu
#ifndef __SYNTHESIS__
           , int cycles
#endif
           );


#endif // CACHE_H
