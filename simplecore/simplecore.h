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

#ifndef __SYNTHESIS__
bool
#else
void
#endif
simplecachedcore(int imem[N], int dmem[N], int& res);

#endif // SIMPLECORE_H
