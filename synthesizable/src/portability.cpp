#include "portability.h"

void formatread(ac_int<32, false> address, ac_int<2, false> datasize, bool sign, ac_int<32, false>& read)
{
    switch(address.slc<2>(0))   // address & offmask
    {
    case 0:
        read >>= 0;
        break;
    case 1:
        read >>= 8;
        assert(datasize == 0 && "address misalignment");
        break;
    case 2:
        read >>= 16;
        assert(datasize <= 1 && "address misalignment");
        break;
    case 3:
        read >>= 24;
        assert(datasize == 0 && "address misalignment");
        break;
    }
    switch(datasize)
    {
    case 0:
        read &= 0x000000FF;
        if(sign && read.slc<1>(7))
            read |= 0xFFFFFF00;
        break;
    case 1:
        read &= 0x0000FFFF;
        if(sign && read.slc<1>(15))
            read |= 0xFFFF0000;
        break;
    case 2:
        read &= 0xFFFFFFFF;
        assert(false && "Size 2 (3 bytes) not supported");
        break;
    case 3:
        read &= 0xFFFFFFFF;
        if(sign && read.slc<1>(31))
            read |= 0x00000000;
        break;
    }
}

void formatwrite(ac_int<32, false> address, ac_int<2, false> datasize, ac_int<32, false>& mem, ac_int<32, false> write)
{
    switch(address.slc<2>(0))
    {
    case 0:
        switch(datasize)
        {
        case 0:
            mem.set_slc(0, write.slc<8>(0));
            break;
        case 1:
            mem.set_slc(8, write.slc<16>(0));
            break;
        case 2:
            mem = write;
            break;
        case 3:
            mem = write;
            break;
        }
        break;
    case 1:
        mem.set_slc(8, write.slc<8>(0));
        assert(datasize == 0 && "address misalignment");
        break;
    case 2:
        if(datasize)
            mem.set_slc(16, write.slc<16>(0));
        else
            mem.set_slc(16, write.slc<8>(0));
        assert(datasize <= 1 && "address misalignment");
        break;
    case 3:
        mem.set_slc(24, write.slc<8>(0));
        assert(datasize == 0 && "address misalignment");
        break;
    }
}
