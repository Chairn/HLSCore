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

    cout << setshift << "        " << tagshift << endl;
    cout << Size << "        " << Blocksize << "        " << Associativity << "        " << Sets << endl;

    assert(Associativity == 1 ? Policy == NONE : Policy != NONE);
    assert((int)ac::log2_ceil<Blocksize>::val == (int)ac::log2_floor<Blocksize>::val);
    assert((int)ac::log2_ceil<Associativity>::val == (int)ac::log2_floor<Associativity>::val);
    assert((int)ac::log2_ceil<Sets>::val == (int)ac::log2_floor<Sets>::val);
    assert((unsigned)(tagmask + setmask + blockmask + offmask) == 0xFFFFFFFF);

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
    imem[0x07] = 0x00000471;    // r[7] = 4
    imem[0x08] = 0x00400081;    // r[8] = 0x4000 (16384)
    imem[0x10] = 0x00002463;    // load word @reg[4] --> reg[6]
    imem[0x11] = 0x00007444;    // plus reg[4] reg[7] --> reg[4]
    imem[0x12] = 0x00006554;    // plus reg[6] reg[5] --> reg[5]
    imem[0x13] = 0x00008405;    // compare reg[4] < reg[8]
    imem[0x14] = 0x00002507;    // out word r[5] --> mem[0]
    imem[0x15] = 0x000010F6;    // branch 0x10
    imem[0x16] = 0x00000000;
    imem[0x17] = 0x00000000;
    imem[0x18] = 0x00000000;
    imem[0x19] = 0x10042507;    // out word r[5] --> mem[0x1004]
    imem[0x20] = 0x10202507;    // out word r[5] --> mem[0x1020]

    i = 0;
#define CHECK_INT(address, value)  ((*reinterpret_cast<int*>(&reinterpret_cast<char*>(dmem)[address])) != (value/4))
    while(true)
    {
        if(CCS_DESIGN(simplecachedcore)(imem, dmem, res))
        {
            cout << "Simulation stopped unexpectedly at cycle " << i << endl;
            break;
        }

        if(CHECK_INT(0x1020, 0x1020))
            break;
        i++;
        if(i > 1e5)
            break;
    }

    cout << "Result : " << res << " in " << i << " cycles" << endl;
    cout << "Result in memory : " << dmem[0] << "   " << dmem[0x401] << "  " << dmem[0x408] << endl;
    cout << "Expected result : " << 4095*4096/2 << endl;

    cout << Size << "        " << Blocksize << "        " << Associativity << "        " << Sets << endl;

    for(i = 0; i < 8191; ++i)
        if(dmem[i] != dmem[i+1]-1)
            cout << i << "  :    " << dmem[i] << endl;

    CCS_RETURN(0);
}
