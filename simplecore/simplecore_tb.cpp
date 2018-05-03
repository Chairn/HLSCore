#include <iostream>
#include <bitset>

#include "simplecore.h"
#include "mc_scverify.h"



using namespace std;

CCS_MAIN(int argc, char **argv)
{
    cout << bitset<32>(blockmask) << hex << "          " << blockmask << endl;
    cout << bitset<32>(setmask) << hex << "          " << setmask << endl;
    cout << bitset<32>(tagmask) << hex << "          " << tagmask << dec << endl;

    assert((int)ac::log2_ceil<Blocksize>::val == (int)ac::log2_floor<Blocksize>::val);
    assert((int)ac::log2_ceil<Associativity>::val == (int)ac::log2_floor<Associativity>::val);
    assert((int)ac::log2_ceil<Sets>::val == (int)ac::log2_floor<Sets>::val);
    assert((unsigned)(tagmask + setmask + blockmask) == 0xFFFFFFFF);

    int imem[N];
    int dmem[N];
    int res = 0;
    int i;
    for(i = 0; i < N; ++i)
    {
        dmem[i] = i;
        imem[i] = 0;
    }

    for(i = 0; i < 16; ++i)
        imem[i] = (i << 4) | 1;
    imem[0x07] = 0x00000171;    // r[7] = 1
    imem[0x08] = 0x00100081;    // r[8] = 0x1000 (4096)
    imem[0x10] = 0x00000463;    // loadmem @reg[4] --> reg[6]
    imem[0x11] = 0x00007444;    // plus reg[4] reg[7] --> reg[4]
    imem[0x12] = 0x00006554;    // plus reg[6] reg[5] --> reg[5]
    imem[0x13] = 0x00008405;    // compare reg[4] < reg[8]
    imem[0x14] = 0x00000507;    // out r[5] --> mem[0]
    imem[0x15] = 0x000010F6;    // branch 0x10
    imem[0x16] = 0x00000000;
    imem[0x17] = 0x00000000;
    imem[0x18] = 0x00000000;
    imem[0x19] = 0x10010507;    // out r[5] --> mem[0x1001]
    imem[0x20] = 0x10200507;

    i = 0;
    while(true)
    {
        CCS_DESIGN(simplecachedcore)(imem, dmem, res);

        if(dmem[0x1020] != 0x1020)
            break;
        i++;
        if(i > 1e6)
            break;
    }

    cout << "Result : " << res << " in " << i << " cycles" << endl;
    cout << "Result in memory : " << dmem[0] << "   " << dmem[0x1001] << "  " << dmem[0x1020] << endl;
    cout << "Expected result : " << 4095*4096/2 << endl;

    CCS_RETURN(0);
}
