#ifndef SIMPLECORE_H
#define SIMPLECORE_H

#ifdef QT_WA
#include "/mnt/vegloff/Documents/comet/synthesizable/include/Catapult/ac_int.h"
#else
#include <ac_int.h>
#endif

#define NONE            0
#define FIFO            1
#define LRU             2
#define RANDOM          3

#ifndef Policy
#define Policy          LRU
#endif

#define N 8192

#ifndef Size
#define Size            512
#endif

#ifndef Blocksize
#define Blocksize       8
#endif

#ifndef Associativity
#define Associativity   4
#endif

#define Sets            (Size/Blocksize/Associativity)

#define blockmask       (Blocksize - 1)
#define setmask         ((Sets - 1) << ac::log2_ceil<Blocksize>::val)
#define tagmask         (~(blockmask + setmask))

#define setshift        (ac::log2_ceil<Blocksize>::val)
#define tagshift        (ac::log2_ceil<Sets>::val + setshift)

void simplecachedcore(int imem[N], int dmem[N], int& res);

#endif // SIMPLECORE_H
