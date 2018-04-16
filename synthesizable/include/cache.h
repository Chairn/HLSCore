#ifndef CACHE_H
#define CACHE_H

#include "portability.h"

enum CACHE_REPLACEMENT_POLICY
{
    LRU,
    LFU,
    RANDOM,
    FIFO
};

/**
 * are address aligned? is cache write allocate or not or make it a template parameter ?
 * does hls support if constexpr (c++17) to discard branch based on template parameter ? NO
 * is hls smart enough to discard those untaken branch ? Probably
 */

template<int Size, int Blocksize, int Associativity, CACHE_REPLACEMENT_POLICY Policy>
class Cache
{
    #define sets (Size/Blocksize/Associativity)
    #define waybits (ac::log2_ceil<Associativity>::val)
    #define tagbits (ac::log2_ceil< 32 - ac::log2_ceil<Blocksize>::val - ac::log2_ceil<sets>::val >::val)
    #define indexbits (ac::log2_ceil<sets>::val)
    #define offsetbits (ac::log2_ceil<Blocksize>::val)

    #define getTag(address) (address.SLC(tagbits, 32-tagbits))
    #define getIndex(address) (indexbits, 32-tagbits-indexbits)
    #define getOffset(address) (address.SLC(offsetbits, 0))
public:
    struct CacheRequest
    {
        CORE_UINT(32) address;
        CORE_UINT(1) RnW;       //Read noWrite : 1 for read, 0 for write
        CORE_UINT(2) dataSize;  //0 for 1 byte, 3 for 4 bytes
        CORE_UINT(32) datum;
    };

    Cache(ac_channel<CORE_UINT(32)>& tc, ac_channel<CacheRequest>& fc)
    {
        assert(tagbits+indexbits+offsetbits == 32);
        //assert(Blocksize*ways == Size);
        assert(Blocksize*Associativity*sets == Size);
        assert(Associativity == 1 && "Only direct mapped cache supported for the moment");

        toCore = tc;
        fromCore = fc;
    }

    void cacheLoop()
    {
        #ifndef __SYNTHESIS__
        while ( fromCore.available(1) )
        #endif
        {
            CacheRequest request = fromCore.read();
            CORE_UINT(tagbits) tag = getTag(request.address);
            CORE_UINT(indexbits) index = getIndex(request.address);
            CORE_UINT(offsetbits) offset = getOffset(request.address);
            CORE_UINT(32) baseAddress = (request.address.SLC(tagbits+indexbits, offsetbits) << offsetbits) | 0;
            CORE_UINT(waybits) way;
            if(find(request.address, way))
            {
                if(request.RnW) //read
                {
                    CORE_UINT(32) value = read(index, way, offset, request.dataSize);
                    toCore.write(value);
                }
                else            //write
                {
                    write(request.address, request.datum);
                    control[index][way].dirty = 1;
                }
            }
            else    // not found or invalid data
            {
                way = select(index);
                if(control[index][way].valid && control[index][way].dirty)
                    writeBack(index, way);

                fetch(request.address, way);
                if(request.RnW) //read
                {
                    CORE_UINT(32) value = read(request.address, way);
                    toCore.write(value);
                    control[index][way].dirty = 0;
                }
                else            //write
                {
                    write(request.address, request.datum);
                    control[index][way].dirty = 1;
                }
            }
        }
    }

private:
    struct Cache_Control
    {
        CORE_UINT(tagbits) tag;
        CORE_UINT(1) valid;
        CORE_UINT(1) dirty;
    };

//#pragma map to sram
    CORE_UINT(32) data[sets][Associativity][Blocksize/4];
//#pragma map to register? to sram? if sram, maybe merge with data(and make an array of struct)
    Cache_Control control[sets][Associativity];

    ac_channel<CORE_UINT(32)>& toCore;
    ac_channel<CacheRequest>& fromCore;

    CORE_UINT(32)* dram;

    CORE_UINT(1) find(CORE_UINT(32) address, CORE_UINT(waybits)& way)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);

        CORE_UINT(1) valid;
        CORE_UINT(1) dirty;
        CORE_UINT(1) found = 0;

        #pragma hls_unroll yes
        findloop:for(int i = 0; i < Associativity; ++i)
        {
            if(control[index][i].tag == tag)
            {
                way = i;
                valid = control[index][i].valid;
                dirty = control[index][i].dirty;
                found = 1;
                //break;
            }
        }

        return found;
    }

    void fetch(CORE_UINT(32) address, CORE_UINT(waybits) way)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);
        CORE_UINT(offsetbits) offset = getOffset(address);
        CORE_UINT(32) baseAddress = (address.SLC(tagbits+indexbits, offsetbits) << offsetbits) | 0;

        #pragma hls_pipeline_init_interval 1
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            /*if(i == offset)
                continue;*/
            data[index][way][i] = dram[baseAddress | i];
        }

        control[index][way].tag = tag;
        control[index][way].valid = 1;
        //caller is responsible to mark dirty or not(caller is either read or write)
        //control[index][way].dirty = 0;
    }

    CORE_UINT(waybits) select(CORE_UINT(indexbits) index)
    {
        CORE_UINT(waybits) way;
        if(Associativity == 1)
            way = 0;

        return 0;
    }

    void writeBack(CORE_UINT(indexbits) index, CORE_UINT(waybits) way)
    {
        CORE_UINT(tagbits) tag = control[index][way].tag;
        CORE_UINT(32) baseAddress = (tag << (indexbits+offsetbits)) | (index << offsetbits) | 0;

        #pragma hls_pipeline_init_interval 1
        writeBackloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            dram[baseAddress | i] = data[index][way][i];
        }

        //control[index][way].valid = 0;
    }

    CORE_UINT(32) read(CORE_UINT(indexbits) index, CORE_UINT(waybits) way, CORE_UINT(offsetbits) offset, CORE_UINT(2) dataSize)
    {
        CORE_UINT(32) value = data[index][way][offset];
        switch(dataSize)
        {
        case 0:
            value &= 0x000000FF;
            break;
        case 1:
            value &= 0x0000FFFF;
            break;
        case 2:
            value &= 0x00FFFFFF;
            break;
        }

        return value;
    }

    void write(CORE_UINT(indexbits) index, CORE_UINT(waybits) way, CORE_UINT(offsetbits) offset, CORE_UINT(32) datum)
    {
        data[index][way][offset] = datum;
    }
};

/** Specialisation
 * A<x, y>
 *
 * B<y> : public A<int, y>
 * a essayer
 */

#undef getTag
#undef getIndex
#undef getOffset
#undef offsetbits
#undef indexbits
#undef tagbits
#undef ways
#undef sets
#endif // CACHE_H
