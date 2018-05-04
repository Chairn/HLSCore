#ifndef SIMPLECORE_H
#define SIMPLECORE_H

#ifdef QT_WA
#include "/mnt/vegloff/Documents/comet/synthesizable/include/Catapult/ac_int.h"
#else
#include <ac_int.h>
#endif

#define N 8192
#define Size            512
#define Blocksize       8
#define Associativity   4
#define Sets            (Size/Blocksize/Associativity)

#define blockmask       (Blocksize - 1)
#define setmask         ((Sets - 1) << ac::log2_ceil<Blocksize>::val)
#define tagmask         (~(blockmask + setmask))

#define setshift        (ac::log2_ceil<Blocksize>::val)
#define tagshift        (ac::log2_ceil<Sets>::val + setshift)

void simplecachedcore(int imem[N], int dmem[N], int& res);

#endif // SIMPLECORE_H
