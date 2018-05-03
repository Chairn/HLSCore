#ifndef SIMPLECORE_H
#define SIMPLECORE_H

#ifdef QT_WA
#include "/mnt/vegloff/Documents/comet/synthesizable/include/Catapult/ac_int.h"
#else
#include <ac_int.h>
#endif

#define N 8192
#define Size            8192
#define Blocksize       8
#define Associativity   1
#define Sets            (Size/Blocksize/Associativity)

#define blockmask       (Blocksize - 1)
#define setmask         (((1 << ac::log2_ceil<Sets>::val)-1) << (ac::log2_ceil<Blocksize>::val + ac::log2_ceil<Associativity>::val))
#define tagmask         (~((1 << (ac::log2_ceil<Sets>::val + ac::log2_ceil<Associativity>::val + ac::log2_ceil<Blocksize>::val)) - 1))

void simplecachedcore(int imem[N], int dmem[N], int& res);

#endif // SIMPLECORE_H
