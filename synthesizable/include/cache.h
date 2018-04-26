#ifndef CACHE_H
#define CACHE_H

#include "portability.h"

//namespace __hls_cache {

enum CacheReplacementPolicy
{
    LRU,
    LFU,
    RANDOM,
    FIFO
};

struct ICacheRequest
{
    CORE_UINT(32) address;
};

struct DCacheRequest    // : ICacheRequest
{
    CORE_UINT(32) address;
    CORE_UINT(1) RnW;       //Read noWrite : 1 for read, 0 for write
    CORE_UINT(2) dataSize;  //0 for 1 byte, 2 for 4 bytes
    CORE_UINT(32) datum;

    enum {
        WRITE   = 0,
        READ    = 1
    };

    enum {
        OneByte     = 0,
        TwoBytes    = 1,
        FourBytes   = 2,
        EighBytes   = 3
    };
};

template<int tagbits, int Associativity>
struct ICacheControl
{
    /*CORE_UINT(tagbits) tag[Associativity];
    CORE_UINT(1) valid[Associativity];*/

    CORE_UINT(tagbits + 1) tagvalid[Associativity]; // have to group manually to minimize control and memory instantiation...

    // Policy replacement bits here because they apply to the whole set
    // not just only a way (depends on the policy actually)
    // but makes more sense to put it here
};

template<int tagbits, int Associativity>
struct DCacheControl
{
    CORE_UINT(tagbits + 1 + 1) vdt[Associativity];  // v d ttttttttttt


    // Policy replacement bits here because they apply to the whole set
    // not just only a way (depends on the policy actually)
    // but makes more sense to put it here
};

#define sets                (Size/Blocksize/Associativity)
#define waybits             (ac::log2_ceil<Associativity>::val)
#define tagbits             (32 - ac::log2_ceil<Blocksize>::val - ac::log2_ceil<sets>::val)
#define indexbits           (ac::log2_ceil<sets>::val)
#define offsetbits          (ac::log2_ceil<Blocksize>::val)

#define getTag(address)     (address.template SLC(tagbits, 32-tagbits))
#define getIndex(address)   (address.template SLC(indexbits, 32-tagbits-indexbits))
#define getOffset(address)  (address.template SLC(offsetbits, 0))

template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
CORE_UINT(waybits) select(CORE_UINT(indexbits) index)
{   // à spécialiser autant de fois que nécessaire
    CORE_UINT(waybits) way;
    if(Associativity == 1)
        way = 0;

    return 0;
}

template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
class ICache
{
public:
    explicit ICache()
    {
        assert(Blocksize % 4 == 0);
        assert(Blocksize > 0);
        assert(Size % 4 == 0);
        assert(Size > 0);
        assert(Associativity > 0);
        assert(tagbits + indexbits + offsetbits == 32);
        assert(Associativity == 1 && "Only direct mapped cache supported for the moment");

        for(int i = 0; i < sets; ++i)
            for(int j = 0; j < Associativity; ++j)
                control[i].tagvalid[j] = 0;
    }

    HLS_DESIGN(interface)
    void run(FIFO(ICacheRequest)& fromCore, FIFO(CORE_UINT(32))& toCore, CORE_UINT(32) memory[DRAM_SIZE])
    {
        #ifndef __SYNTHESIS__
        while ( fromCore.available(1) )
        #endif
        {
            ICacheRequest request = fromCore.read();     // blocking read, replace with non blocking ?
            CORE_UINT(tagbits) tag = getTag(request.address);
            CORE_UINT(indexbits) index = getIndex(request.address);
            CORE_UINT(offsetbits) offset = getOffset(request.address);

            CORE_UINT(waybits) way;
            CORE_UINT(32) value;
            CORE_UINT(1) valid;

            CORE_UINT(1+tagbits) controlwriteback;
            controlwriteback.SET_SLC(0, tag);
            controlwriteback.SET_SLC(tagbits, (CORE_UINT(1))1);   // always valid

            if(find(tag, index, offset, way, valid))
            {
                value = data[index][way][offset >> 2];
                toCore.write(value);
            }
            else    // not found or invalid data
            {
                way = select<Size, Blocksize, Associativity, Policy>(index);    //select way to replace

                fetch(tag, index, offset, way, value, memory);
                toCore.write(value);
            }
            // update policy bits
            control[index].tagvalid[way] = controlwriteback;
        }
    }

private:
//#pragma map to sram
    CORE_UINT(32) data[sets][Associativity][Blocksize/4];
    ICacheControl<tagbits, Associativity> control[sets];

    CORE_UINT(1) find(CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(waybits)& way, CORE_UINT(1)& valid)
    {
        CORE_UINT(1) found = 0;
        HLS_UNROLL(yes)
        findloop:for(int i = 0; i < Associativity; ++i)
        {
            CORE_UINT(tagbits+1) _tag = control[index].tagvalid[i];
            if((_tag.template SLC(tagbits, 0)) == tag)
            {
                way = i;
                valid.SET_SLC(tagbits, _tag.template SLC(1, tagbits));
                found = 1;
                //break;
            }
        }

        return found && valid;
    }

    void fetch(CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(waybits) way, CORE_UINT(32)& value, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(32) baseAddress = (tag << (indexbits+offsetbits)) | (index << offsetbits);
        CORE_UINT(32) address = (tag << (indexbits+offsetbits)) | (index << offsetbits) | offset;

        HLS_PIPELINE(1)
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            CORE_UINT(32) tmp = memory[baseAddress | i];
            data[index][way][i] = tmp;
            if(baseAddress | i == address)
                value = tmp;
        }

        control[index].tagvalid[way] = (1 << tagbits) | tag;
    }
};


template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
class DCache
{
public:
    explicit DCache()
    {
        assert(Blocksize % 4 == 0);
        assert(Blocksize > 0);
        assert(Size % 4 == 0);
        assert(Size > 0);
        assert(Associativity > 0);
        assert(tagbits + indexbits + offsetbits == 32);
        assert(Associativity == 1 && "Only direct mapped cache supported for the moment");

        for(int i = 0; i < sets; ++i)
            for(int j = 0; j < Associativity; ++j)
                control[i].vdt[j] = 0;
    }

    HLS_DESIGN(interface)
    void run(FIFO(DCacheRequest)& fromCore, FIFO(CORE_UINT(32))& toCore, CORE_UINT(32) memory[DRAM_SIZE])
    {
        #ifndef __SYNTHESIS__
        while ( fromCore.available(1) )
        #endif
        {
            DCacheRequest request = fromCore.read();     // blocking read, replace with non blocking
            CORE_UINT(tagbits) tag = getTag(request.address);
            CORE_UINT(indexbits) index = getIndex(request.address);
            CORE_UINT(offsetbits) offset = getOffset(request.address);

            CORE_UINT(tagbits) replacedtag;
            CORE_UINT(waybits) way;
            CORE_UINT(32) value;
            CORE_UINT(1) valid;
            CORE_UINT(1) dirty;

            CORE_UINT(1+1+tagbits) controlwriteback;
            controlwriteback.SET_SLC(0, tag);
            controlwriteback.SET_SLC(1+tagbits, (CORE_UINT(1))1);   // always valid

            if(find(tag, index, offset, way, valid, dirty))
            {
                value = data[index][way][offset >> 2];
                if(request.RnW) //read
                {
                    format(value, offset, request.dataSize);
                    toCore.write(value);
                    controlwriteback.SET_SLC(tagbits, dirty);
                }
                else            //write
                {
                    write(index, way, offset, request.dataSize, value, request.datum);
                    controlwriteback.SET_SLC(tagbits, (CORE_UINT(1))1);
                }
            }
            else    // not found or invalid data
            {
                way = select<Size, Blocksize, Associativity, Policy>(index);    //select invalid data first
                replacedtag = control[index].vdt[way].template SLC(tagbits, 0);

                if(valid && dirty)
                    writeBack(replacedtag, index, way, memory);

                fetch(tag, index, offset, way, value, memory);
                if(request.RnW) //read
                {
                    format(value, offset, request.dataSize);
                    toCore.write(value);
                }
                else            //write
                {
                    write(index, way, offset, request.dataSize, value, request.datum);
                }
                controlwriteback.SET_SLC(tagbits, (CORE_UINT(1))!request.RnW);
            }
            // update policy bits with controlwriteback
            control[index].vdt[way] = controlwriteback;
        }
    }

private:
    CORE_UINT(32) data[sets][Associativity][Blocksize/4];
//#pragma map to register? to sram? if sram, maybe merge with data(and make an array of struct)
    DCacheControl<tagbits, Associativity> control[sets];

    CORE_UINT(1) find(CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(waybits)& way, CORE_UINT(1)& valid, CORE_UINT(1)& dirty)
    {
        CORE_UINT(1) found = 0;

        HLS_UNROLL(yes)
        findloop:for(int i = 0; i < Associativity; ++i)
        {
            CORE_UINT(1+1+tagbits) _tag = control[index].vdt[i];
            if((_tag.template SLC(tagbits, 0)) == tag)
            {
                way = i;
                valid = _tag.template SLC(1, 1+tagbits);
                dirty = _tag.template SLC(1, tagbits);
                found = 1;
                //break;
            }
        }

        return found && valid;
    }

    void fetch(CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(waybits) way, CORE_UINT(32)& value, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(32) baseAddress = (tag << (indexbits+offsetbits)) | (index << offsetbits);
        CORE_UINT(32) address = (tag << (indexbits+offsetbits)) | (index << offsetbits) | offset;

        HLS_PIPELINE(1)
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            CORE_UINT(32) tmp = memory[baseAddress | i];
            data[index][way][i] = tmp;
            if(baseAddress | i == address)
                value = tmp;
        }

        control[index].vdt[way] = (1 << (tagbits+1)) | tag;
    }

    void format(CORE_UINT(32)& value, CORE_UINT(offsetbits) offset, CORE_UINT(2) dataSize)
    {
        switch(offset.template SLC(2,0))
        {
        case 0:
            value >>= 0;
            break;
        case 1:
            value >>= 8;
            break;
        case 2:
            value >>= 16;
            break;
        case 3:
            value >>= 24;
            break;
        }
        switch(dataSize)
        {
        case 0:
            value &= 0x000000FF;
            break;
        case 1:
            value &= 0x0000FFFF;
            break;
        case 2:
            value &= 0xFFFFFFFF;
            break;
        case 3:
            value &= 0xFFFFFFFF;
            break;
        }
    }

    void write(CORE_UINT(indexbits) index, CORE_UINT(waybits) way, CORE_UINT(offsetbits) offset, CORE_UINT(2) dataSize, CORE_UINT(32) mem, CORE_UINT(32) datum)
    {
        switch(dataSize)
        {
        case 0:
            switch(offset & 3)
            {
            case 0:
                mem.SET_SLC(0, datum.template SLC(8, 0));
                //mem = (mem & 0xFFFFFF00) | (datum & 0x000000FF);
                break;
            case 1:
                mem.SET_SLC(8, datum.template SLC(8,8));
                //mem = (mem & 0xFFFF00FF) | ((datum & 0x000000FF) << 8);
                break;
            case 2:
                mem.SET_SLC(16, datum.template SLC(8,16));
                //mem = (mem & 0xFF00FFFF) | ((datum & 0x000000FF) << 16);
                break;
            case 3:
                mem.SET_SLC(24, datum.template SLC(8,24));
                //mem = (mem & 0x00FFFFFF) | ((datum & 0x000000FF) << 24);
                break;
            }
            break;
        case 1:
            if(offset & 2)
                mem.SET_SLC(16, datum.template SLC(16,16));
            //mem = (mem & 0x0000FFFF) | ((datum & 0x0000FFFF) << 16);
            else
                mem.SET_SLC(0, datum.template SLC(16,0));
            //mem = (mem & 0xFFFF0000) | (datum & 0x0000FFFF);
            break;
        case 2:
            mem = datum;
            break;
        case 3:
            mem = datum;
            break;
        }
        data[index][way][offset >> 2] = mem;        // à sortir et à mettre à la fin de run? à tester
    }

    void writeBack(CORE_UINT(tagbits) replacedtag, CORE_UINT(indexbits) index, CORE_UINT(waybits) way, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(32) baseAddress = (replacedtag << (indexbits+offsetbits)) | (index << offsetbits) | 0;

        HLS_PIPELINE(1)
        writeBackloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            memory[baseAddress | i] = data[index][way][i];
        }

        // data is still valid, no need to invalidate although it is likely to be replaced
        //control[index][way].valid = 0;
    }
};

#undef getTag
#undef getIndex
#undef getOffset
#undef offsetbits
#undef indexbits
#undef tagbits
#undef waybits
#undef sets

/****************************************************************************************************************
 *                                              Specialization                                                  *
 ****************************************************************************************************************/

#define sets                (Size/Blocksize)
#define tagbits             (32 - ac::log2_ceil<Blocksize>::val - ac::log2_ceil<sets>::val)
#define indexbits           (ac::log2_ceil<sets>::val)
#define offsetbits          (ac::log2_ceil<Blocksize>::val)

#define getTag(address)     (address.template SLC(tagbits, 32-tagbits))
#define getIndex(address)   (address.template SLC(indexbits, 32-tagbits-indexbits))
#define getOffset(address)  (address.template SLC(offsetbits, 0))

template<int Size, int Blocksize, CacheReplacementPolicy Policy>
class ICache<Size, Blocksize, 1, Policy>
{
public:
    explicit ICache()
    {
        assert(Blocksize % 4 == 0);
        assert(Blocksize > 0);
        assert(Size % 4 == 0);
        assert(Size > 0);
        assert(tagbits + indexbits + offsetbits == 32);

        for(int i = 0; i < sets; ++i)
            control[i].tagvalid[0] = 0;
    }

    CORE_UINT(32) read(CORE_UINT(32) address, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);
        CORE_UINT(offsetbits) offset = getOffset(address);

        CORE_UINT(32) value;
        CORE_UINT(1) valid;
        CORE_UINT(1+tagbits) controlwriteback;
        controlwriteback.SET_SLC(0, tag);
        controlwriteback.SET_SLC(tagbits, (CORE_UINT(1))1); // valid

        if(find(tag, index, offset, valid))
        {
            value = data[index][0][offset >> 2];
        }
        else    // not found or invalid data
        {
            fetch(tag, index, offset, value, memory);
        }
        // update policy bits
        control[index].tagvalid[0] = controlwriteback;
        return value;
    }

private:
//#pragma map to sram
    CORE_UINT(32) data[sets][1][Blocksize/4];
    ICacheControl<tagbits, 1> control[sets];

    CORE_UINT(1) find(CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(1)& valid)
    {
        CORE_UINT(1) found = 0;

        CORE_UINT(tagbits+1) _tag = control[index].tagvalid[0];
        if((_tag.template SLC(tagbits, 0)) == tag)
        {
            valid = _tag.template SLC(1, tagbits);
            found = 1;
            //break;
        }

        return found && valid;
    }

    void fetch(CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(32)& value, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(32) baseAddress = (tag << (indexbits+offsetbits)) | (index << offsetbits);
        CORE_UINT(32) address = (tag << (indexbits+offsetbits)) | (index << offsetbits) | offset;

        HLS_PIPELINE(1)
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            CORE_UINT(32) tmp = memory[baseAddress | i];
            data[index][0][i] = tmp;
            if(baseAddress | i == address)
                value = tmp;
        }
    }
};


template<int Size, int Blocksize, CacheReplacementPolicy Policy>
class DCache<Size, Blocksize, 1, Policy>
{
public:
    explicit DCache()
    {
        assert(Blocksize % 4 == 0);
        assert(Blocksize > 0);
        assert(Size % 4 == 0);
        assert(Size > 0);
        assert(tagbits + indexbits + offsetbits == 32);

        for(int i = 0; i < sets; ++i)
            control[i].vdt[0] = 0;
    }

    CORE_UINT(32) read(CORE_UINT(32) address, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);
        CORE_UINT(offsetbits) offset = getOffset(address);

        CORE_UINT(tagbits) replacedtag;
        CORE_UINT(32) value;
        CORE_UINT(1) valid;
        CORE_UINT(1) dirty;

        CORE_UINT(1+1+tagbits) controlwriteback;
        controlwriteback.SET_SLC(0, tag);

        if(find(tag, index, offset, valid, dirty, replacedtag))
        {
            value = data[index][0][offset >> 2];
            controlwriteback.SET_SLC(tagbits, dirty);
            controlwriteback.SET_SLC(1+tagbits, (CORE_UINT(1))1);
        }
        else    // not found or invalid data
        {
            if(valid && dirty)
                writeBack(replacedtag, index, memory);

            fetch(tag, index, offset, value, memory);
            controlwriteback.SET_SLC(tagbits, (CORE_UINT(2))2); // valid & non dirty
        }
        // update policy bits with controlwriteback
        control[index].vdt[0] = controlwriteback;

        return value;
    }

    void write(CORE_UINT(32) address, CORE_UINT(32) datum, CORE_UINT(2) dataSize, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);
        CORE_UINT(offsetbits) offset = getOffset(address);

        CORE_UINT(tagbits) replacedtag;
        CORE_UINT(32) value;
        CORE_UINT(1) valid;
        CORE_UINT(1) dirty;

        CORE_UINT(1+1+tagbits) controlwriteback;
        controlwriteback.SET_SLC(0, tag);

        if(find(tag, index, offset, valid, dirty, replacedtag))
        {
            value = data[index][0][offset >> 2];
            controlwriteback.SET_SLC(tagbits, (CORE_UINT(2))3); // valid & dirty
        }
        else    // not found or invalid data
        {
            if(valid && dirty)
                writeBack(replacedtag, index, memory);

            fetch(tag, index, offset, value, memory);
            controlwriteback.SET_SLC(tagbits, (CORE_UINT(2))3); // valid & dirty
        }

        switch(dataSize)
        {
        case 0:
            switch(offset & 3)
            {
            case 0:
                value.SET_SLC(0, datum.SLC(8, 0));
                //mem = (mem & 0xFFFFFF00) | (datum & 0x000000FF);
                break;
            case 1:
                value.SET_SLC(8, datum.SLC(8, 0));
                //mem = (mem & 0xFFFF00FF) | ((datum & 0x000000FF) << 8);
                break;
            case 2:
                value.SET_SLC(16, datum.SLC(8, 0));
                //mem = (mem & 0xFF00FFFF) | ((datum & 0x000000FF) << 16);
                break;
            case 3:
                value.SET_SLC(24, datum.SLC(8, 0));
                //mem = (mem & 0x00FFFFFF) | ((datum & 0x000000FF) << 24);
                break;
            }
            break;
        case 1:
            if(offset & 2)
                value.SET_SLC(16, datum.SLC(16, 0));
            //mem = (mem & 0x0000FFFF) | ((datum & 0x0000FFFF) << 16);
            else
                value.SET_SLC(0, datum.SLC(16,0));
            //mem = (mem & 0xFFFF0000) | (datum & 0x0000FFFF);
            break;
        case 2:
            value = datum;
            break;
        case 3:
            value = datum;
            break;
        }
        data[index][0][offset >> 2] = value;        // à sortir et à mettre à la fin de run? à tester
        // update policy bits with controlwriteback
        control[index].vdt[0] = controlwriteback;
    }

private:
    CORE_UINT(32) data[sets][1][Blocksize/4];
//#pragma map to register? to sram? if sram, maybe merge with data(and make an array of struct)
    DCacheControl<tagbits, 1> control[sets];

    CORE_UINT(1) find(CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(1)& valid, CORE_UINT(1)& dirty, CORE_UINT(tagbits)& replacedtag)
    {
        CORE_UINT(1) found = 0;
        CORE_UINT(1+1+tagbits) _tag = control[index].vdt[0];
        replacedtag = _tag.template SLC(tagbits, 0);

        if((_tag.template SLC(tagbits, 0)) == tag)
        {
            valid = _tag.template SLC(1, 1+tagbits);
            dirty = _tag.template SLC(1, tagbits);
            found = 1;
            //break;
        }

        return found && valid;
    }

    void fetch(CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(32)& value, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(32) baseAddress = (tag << (indexbits+offsetbits)) | (index << offsetbits);
        CORE_UINT(32) address = (tag << (indexbits+offsetbits)) | (index << offsetbits) | offset;

        HLS_PIPELINE(1)
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            CORE_UINT(32) tmp = memory[baseAddress | i];
            data[index][0][i] = tmp;
            if(baseAddress | i == address)
                value = tmp;
        }
    }

    void writeBack(CORE_UINT(tagbits) replacedtag, CORE_UINT(indexbits) index, CORE_UINT(32) memory[DRAM_SIZE])
    {
        CORE_UINT(32) baseAddress = (replacedtag << (indexbits+offsetbits)) | (index << offsetbits) | 0;

        HLS_PIPELINE(1)
        writeBackloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            memory[baseAddress | i] = data[index][0][i];
        }

        // data is still valid, no need to invalidate although it is likely to be replaced
        //control[index][way].valid = 0;
    }
};


//} // __hls_cache namespace

#endif // CACHE_H
