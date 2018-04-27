#include <iostream>

#include "simplecore.h"
#include "mc_scverify.h"



using namespace std;

CCS_MAIN(int argc, char **argv)
{
    int imem[N];
    int dmem[N];
    int res = 0;
    for(int i = 0; i < N; ++i)
    {
        dmem[i] = i;
        imem[i] = 0;
    }

    for(int i = 0; i < 16; ++i)
        imem[i] = (i << 4) | 1;
    imem[0x07] = 0x00000171;    // r[7] = 1
    imem[0x08] = 0x00100081;    // r[8] = 0x1000 (4096)
    imem[0x10] = 0x00000463;    // loadmem @reg[4] --> reg[6]
    imem[0x11] = 0x00007444;    // plus reg[4] reg[7] --> reg[4]
    imem[0x12] = 0x00006554;    // plus reg[6] reg[5] --> reg[5]
    imem[0x13] = 0x00008405;    // compare reg[4] < reg[8]
    imem[0x14] = 0x00000000;    // nop
    imem[0x15] = 0x000010F6;    // branch 0x10
    imem[0x16] = 0x00000000;
    imem[0x17] = 0x00000000;
    imem[0x18] = 0x00000000;
    imem[0x19] = 0x00000507;    // out r[5]

    int i = 0;
    while(true)
    {
        CCS_DESIGN(simplecachedcore)(imem, dmem, res);

        if(res != 0)
            break;
        i++;
        if(i > 1000)
            break;
    }

    cout << res << endl;

    CCS_RETURN(0);
}
