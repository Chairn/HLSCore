#ifndef CACHE_H
#define CACHE_H

#include "portability.h"

//namespace __hls_cache {

#define sets                (Size/Blocksize/Associativity)
#define waybits             (ac::log2_ceil<Associativity>::val)
#define tagbits             (32 - ac::log2_ceil<Blocksize>::val - ac::log2_ceil<sets>::val)
#define indexbits           (ac::log2_ceil<sets>::val)
#define offsetbits          (ac::log2_ceil<Blocksize>::val)

#define getTag(address)     (address.SLC(tagbits, 32-tagbits))
#define getIndex(address)   (address.SLC(indexbits, 32-tagbits-indexbits))
#define getOffset(address)  (address.SLC(offsetbits, 0))

enum CacheReplacementPolicy
{
    LRU,
    LFU,
    RANDOM,
    FIFO
};

struct BaseCacheRequest
{
    CORE_UINT(32) address;
};

typedef BaseCacheRequest ICacheRequest;

struct DCacheRequest : BaseCacheRequest
{
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

template<int Size, int Blocksize, int Associativity>
struct BaseCacheControl
{
    CORE_UINT(tagbits) tag[Associativity];
    CORE_UINT(1) valid[Associativity];
    // Policy replacement bits here because they apply to the whole set
    // not just only a way (depends on the policy actually)
    // but makes more sense to put it here
};

template<int Size, int Blocksize, int Associativity>
struct DCacheControl : BaseCacheControl<Size, Blocksize, Associativity>
{
    CORE_UINT(1) dirty[Associativity];
};

#define ICacheControl BaseCacheControl
/*template<int Size, int Blocksize, int Associativity>
using ICacheControl = BaseCacheControl<Size, Blocksize, Associativity>;*/


/**
 * are address aligned? is cache write allocate or not or make it a template parameter ?
 * does hls support if constexpr (c++17) to discard branch based on template parameter ? NO
 * is hls smart enough to discard those untaken branch ? Probably
 */


template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
CORE_UINT(waybits) select(CORE_UINT(indexbits) index)
{   // à spécialiser autant de fois que nécessaire
    CORE_UINT(waybits) way;
    if(Associativity == 1)
        way = 0;

    return 0;
}

template<int Size, int Blocksize, int Associativity, template<int, int, int > class Control>
class BaseBase
{
protected:
    CORE_UINT(32) data[sets][Associativity][Blocksize/4];
    Control<Size, Blocksize, Associativity> control[sets];
};

template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
class BaseCache
{
public:
    explicit BaseCache()
    {
        assert(Blocksize % 4 == 0);
        assert(Blocksize > 0);
        assert(Size % 4 == 0);
        assert(Size > 0);
        assert(Associativity > 0);
        assert(tagbits + indexbits + offsetbits == 32);
        assert(Associativity == 1 && "Only direct mapped cache supported for the moment");
    }

    // Cache is non copyable object
    BaseCache(const BaseCache&) = delete;
    void operator=(const BaseCache&) = delete;

protected:
//#pragma map to sram
    CORE_UINT(32) data[sets][Associativity][Blocksize/4];

    CORE_UINT(32) read(CORE_UINT(indexbits) index, CORE_UINT(waybits) way, CORE_UINT(offsetbits) offset)
    {
        return data[index][way][offset >> 2];
    }

    CORE_UINT(32) read(CORE_UINT(indexbits) index, CORE_UINT(waybits) way, CORE_UINT(offsetbits) offset, CORE_UINT(2) dataSize)
    {
        // data is considered aligned
        CORE_UINT(32) value = data[index][way][offset >> 2];

        switch(offset.SLC(2,0))
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

        return value;
    }

    void write(CORE_UINT(indexbits) index, CORE_UINT(waybits) way, CORE_UINT(offsetbits) offset, CORE_UINT(2) dataSize, CORE_UINT(32) datum)
    {
        CORE_UINT(32) mem = data[index][way][offset >> 2];

        switch(dataSize)
        {
        case 0:
            switch(offset & 3)
            {
            case 0:
                mem.SET_SLC(0, datum.SLC(8, 0));
                //mem = (mem & 0xFFFFFF00) | (datum & 0x000000FF);
                break;
            case 1:
                mem.SET_SLC(8, datum.SLC(8,8));
                //mem = (mem & 0xFFFF00FF) | ((datum & 0x000000FF) << 8);
                break;
            case 2:
                mem.SET_SLC(16, datum.SLC(8,16));
                //mem = (mem & 0xFF00FFFF) | ((datum & 0x000000FF) << 16);
                break;
            case 3:
                mem.SET_SLC(24, datum.SLC(8,24));
                //mem = (mem & 0x00FFFFFF) | ((datum & 0x000000FF) << 24);
                break;
            }
            break;
        case 1:
            if(offset & 2)
                mem.SET_SLC(16, datum.SLC(16,16));
                //mem = (mem & 0x0000FFFF) | ((datum & 0x0000FFFF) << 16);
            else
                mem.SET_SLC(0, datum.SLC(16,0));
                //mem = (mem & 0xFFFF0000) | (datum & 0x0000FFFF);
            break;
        case 2:
            mem = datum;
            break;
        case 3:
            mem = datum;
            break;
        }
        data[index][way][offset >> 2] = mem;
    }
};

template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
class ICache : public BaseCache<Size, Blocksize, Associativity, Policy>
{
    using BaseCache<Size, Blocksize, Associativity, Policy>::data;
    using BaseCache<Size, Blocksize, Associativity, Policy>::read;
    using BaseCache<Size, Blocksize, Associativity, Policy>::write;
public:
    explicit ICache() : BaseCache<Size, Blocksize, Associativity, Policy>()
    {
        for(int i = 0; i < sets; ++i)
            for(int j = 0; j < Associativity; ++j)
                control[i].valid[j] = 0;
    }

    HLS_DESIGN(interface)
    void run(ac_channel<ICacheRequest>& fromCore, ac_channel<CORE_UINT(32)>& toCore, ac_channel<ICacheRequest>& toMemory, ac_channel<CORE_UINT(32)>& fromMemory)
    {
        #ifndef __SYNTHESIS__
        while ( fromCore.available(1) )
        #endif
        {
            ICacheRequest request = fromCore.read();     // blocking read, replace with non blocking ?
            CORE_UINT(tagbits) tag = getTag(request.address);
            CORE_UINT(indexbits) index = getIndex(request.address);
            CORE_UINT(offsetbits) offset = getOffset(request.address);
            CORE_UINT(32) baseAddress = (request.address.SLC(tagbits+indexbits, offsetbits) << offsetbits) | 0;
            CORE_UINT(waybits) way;
            if(find(request.address, way))
            {
                CORE_UINT(32) value = read(index, way, offset);
                toCore.write(value);
            }
            else    // not found or invalid data
            {
                way = select<Size, Blocksize, Associativity, Policy>(index);    //select way to replace

                fetch(toMemory, fromMemory, request.address, way);
                CORE_UINT(32) value = read(index, way, offset);
                toCore.write(value);
            }
            // update policy bits
        }
    }

protected:
//#pragma map to register? to sram? if sram, maybe merge with data(and make an array of struct)
    ICacheControl<Size, Blocksize, Associativity> control[sets];

    CORE_UINT(1) find(CORE_UINT(32) address, CORE_UINT(waybits)& way)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);

        CORE_UINT(1) valid = 0;
        CORE_UINT(1) found = 0;

        HLS_UNROLL(yes)
        findloop:for(int i = 0; i < Associativity; ++i)
        {
            if(control[index].tag[i] == tag)
            {
                way = i;
                valid = control[index].valid[i];
                found = 1;
                //break;
            }
        }

        return found && valid;
    }

    void fetch(ac_channel<ICacheRequest>& toMemory, ac_channel<CORE_UINT(32)>& fromMemory, CORE_UINT(32) address, CORE_UINT(waybits) way)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);
        CORE_UINT(offsetbits) offset = getOffset(address);
        CORE_UINT(32) baseAddress = (address.SLC(tagbits+indexbits, offsetbits) << offsetbits) | 0;

        ICacheRequest request = {address};
        toMemory.write(request);
        HLS_PIPELINE(1)
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            data[index][way][i] = fromMemory.read();    //dram[baseAddress | i];
        }

        control[index].tag[way] = tag;
        control[index].valid[way] = 1;
    }
};

template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
class DCache : public BaseCache<Size, Blocksize, Associativity, Policy>
{
    using BaseCache<Size, Blocksize, Associativity, Policy>::data;
    using BaseCache<Size, Blocksize, Associativity, Policy>::read;
    using BaseCache<Size, Blocksize, Associativity, Policy>::write;
public:
    explicit DCache() : BaseCache<Size, Blocksize, Associativity, Policy>()
    {
        for(int i = 0; i < sets; ++i)
            for(int j = 0; j < Associativity; ++j)
                control[i].valid[j] = 0;
    }

    HLS_DESIGN(interface)
    void run(ac_channel<DCacheRequest>& fromCore, ac_channel<CORE_UINT(32)>& toCore, ac_channel<DCacheRequest>& toMemory, ac_channel<CORE_UINT(32)>& fromMemory)
    {
        #ifndef __SYNTHESIS__
        while ( fromCore.available(1) )
        #endif
        {
            DCacheRequest request = fromCore.read();     // blocking read, replace with non blocking
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
                    write(index, way, offset, request.dataSize, request.datum);
                    control[index].dirty[way] = 1;
                }
            }
            else    // not found or invalid data
            {
                way = select<Size, Blocksize, Associativity, Policy>(index);    //select invalid data first
                if(control[index].valid[way] && control[index].dirty[way])
                    writeBack(toMemory, tag, index, way);

                this->fetch(toMemory, fromMemory, request.address, way);
                if(request.RnW) //read
                {
                    CORE_UINT(32) value = read(index, way, offset, request.dataSize);
                    toCore.write(value);
                    control[index].dirty[way] = 0;
                }
                else            //write
                {
                    write(index, way, offset, request.dataSize, request.datum);
                    control[index].dirty[way] = 1;
                }
            }
        }
    }

protected:
//#pragma map to register? to sram? if sram, maybe merge with data(and make an array of struct)
    DCacheControl<Size, Blocksize, Associativity> control[sets];     // control exists twice, but is discarded by the HLS tool

    void fetch(ac_channel<DCacheRequest>& toMemory, ac_channel<CORE_UINT(32)>& fromMemory, CORE_UINT(32) address, CORE_UINT(waybits) way)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);
        CORE_UINT(offsetbits) offset = getOffset(address);
        CORE_UINT(32) baseAddress = (address.SLC(tagbits+indexbits, offsetbits) << offsetbits) | 0;

        HLS_PIPELINE(1)
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            DCacheRequest request;
            request.address = baseAddress | i;
            request.RnW = DCacheRequest::READ;
            request.dataSize = DCacheRequest::FourBytes;
            request.datum = 0;
            toMemory.write(request);
            toMemory.write(request);
            data[index][way][i] = fromMemory.read();    //dram[baseAddress | i];
        }

        control[index].tag[way] = tag;
        control[index].valid[way] = 1;
    }

    CORE_UINT(1) find(CORE_UINT(32) address, CORE_UINT(waybits)& way)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);

        CORE_UINT(1) valid = 0;
        CORE_UINT(1) found = 0;

        HLS_UNROLL(yes)
        findloop:for(int i = 0; i < Associativity; ++i)
        {
            if(control[index].tag[i] == tag)
            {
                way = i;
                valid = control[index].valid[i];
                found = 1;
                //break;
            }
        }

        return found && valid;
    }

    void writeBack(ac_channel<DCacheRequest>& toMemory, CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index, CORE_UINT(waybits) way)
    {
        CORE_UINT(32) baseAddress = (tag << (indexbits+offsetbits)) | (index << offsetbits) | 0;

        HLS_PIPELINE(1)
        writeBackloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            DCacheRequest request;
            request.address = baseAddress | i;
            request.RnW = DCacheRequest::WRITE;
            request.dataSize = DCacheRequest::FourBytes;
            request.datum = data[index][way][i];
            toMemory.write(request);
            //dram[baseAddress | i] = data[index][way][i];
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

#define getTag(address)     (address.SLC(tagbits, 32-tagbits))
#define getIndex(address)   (address.SLC(indexbits, 32-tagbits-indexbits))
#define getOffset(address)  (address.SLC(offsetbits, 0))

template<int Size, int Blocksize, CacheReplacementPolicy Policy>
class BaseCache<Size, Blocksize, 1, Policy>
{
public:
    explicit BaseCache()
    {
        assert(Blocksize % 4 == 0);
        assert(Blocksize > 0);
        assert(Size % 4 == 0);
        assert(Size > 0);
        assert(tagbits + indexbits + offsetbits == 32);
    }

    // Cache is non copyable object
    BaseCache(const BaseCache&) = delete;
    void operator=(const BaseCache&) = delete;

protected:
//#pragma map to sram
    CORE_UINT(32) data[sets][Blocksize/4];

    CORE_UINT(32) read(CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset)
    {
        return data[index][offset >> 2];
    }

    CORE_UINT(32) read(CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(2) dataSize)
    {
        // data is considered aligned
        CORE_UINT(32) value = data[index][offset >> 2];

        switch(offset & 3)  //offset.SLC(2,0) interprété comme opérateur < ....
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

        return value;
    }

    void write(CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(2) dataSize, CORE_UINT(32) datum, CORE_UINT(32) mem)
    {
        //CORE_UINT(32) mem = data[index][offset >> 2];

        switch(dataSize)
        {
        case 0:
            switch(offset & 3)
            {
            case 0:
                mem.SET_SLC(0, datum.SLC(8, 0));
                //mem = (mem & 0xFFFFFF00) | (datum & 0x000000FF);
                break;
            case 1:
                mem.SET_SLC(8, datum.SLC(8,8));
                //mem = (mem & 0xFFFF00FF) | ((datum & 0x000000FF) << 8);
                break;
            case 2:
                mem.SET_SLC(16, datum.SLC(8,16));
                //mem = (mem & 0xFF00FFFF) | ((datum & 0x000000FF) << 16);
                break;
            case 3:
                mem.SET_SLC(24, datum.SLC(8,24));
                //mem = (mem & 0x00FFFFFF) | ((datum & 0x000000FF) << 24);
                break;
            }
            break;
        case 1:
            if(offset & 2)
                mem.SET_SLC(16, datum.SLC(16,16));
                //mem = (mem & 0x0000FFFF) | ((datum & 0x0000FFFF) << 16);
            else
                mem.SET_SLC(0, datum.SLC(16,0));
                //mem = (mem & 0xFFFF0000) | (datum & 0x0000FFFF);
            break;
        case 2:
            mem = datum;
            break;
        case 3:
            mem = datum;
            break;
        }
        data[index][offset >> 2] = mem;
    }
};

template<int Size, int Blocksize, CacheReplacementPolicy Policy>
class ICache<Size, Blocksize, 1, Policy> : public BaseCache<Size, Blocksize, 1, Policy>
{
    using BaseCache<Size, Blocksize, 1, Policy>::data;
    using BaseCache<Size, Blocksize, 1, Policy>::read;
    using BaseCache<Size, Blocksize, 1, Policy>::write;
public:
    explicit ICache() : BaseCache<Size, Blocksize, 1, Policy>()
    {
        for(int i = 0; i < sets; ++i)
            control[i].valid[0] = 0;
    }

    HLS_DESIGN(interface)
    void run(ac_channel<ICacheRequest>& fromCore, ac_channel<CORE_UINT(32)>& toCore, ac_channel<ICacheRequest>& toMemory, ac_channel<CORE_UINT(32)>& fromMemory)
    {
        #ifndef __SYNTHESIS__
        while ( fromCore.available(1) )
        #endif
        {
            ICacheRequest request = fromCore.read();     // blocking read, replace with non blocking ?
            CORE_UINT(tagbits) tag = getTag(request.address);
            CORE_UINT(indexbits) index = getIndex(request.address);
            CORE_UINT(offsetbits) offset = getOffset(request.address);
            CORE_UINT(32) baseAddress = (request.address.SLC(tagbits+indexbits, offsetbits) << offsetbits) | 0;
            if(find(request.address))
            {
                CORE_UINT(32) value = read(index, offset);
                toCore.write(value);
            }
            else    // not found or invalid data
            {
                fetch(toMemory, fromMemory, request.address);
                CORE_UINT(32) value = read(index, offset);
                toCore.write(value);
            }
            // update policy bits
        }
    }

protected:
//#pragma map to register? to sram? if sram, maybe merge with data(and make an array of struct)
    ICacheControl<Size, Blocksize, 1> control[sets];

    CORE_UINT(1) find(CORE_UINT(32) address)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);

        CORE_UINT(1) valid = 0;
        CORE_UINT(1) found = 0;

        if(control[index].tag[0] == tag)
        {
            valid = control[index].valid[0];
            found = 1;
            //break;
        }


        return found && valid;
    }

    void fetch(ac_channel<ICacheRequest>& toMemory, ac_channel<CORE_UINT(32)>& fromMemory, CORE_UINT(32) address)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);
        CORE_UINT(offsetbits) offset = getOffset(address);
        CORE_UINT(32) baseAddress = (address.SLC(tagbits+indexbits, offsetbits) << offsetbits) | 0;

        ICacheRequest request = {address};
        toMemory.write(request);
        HLS_PIPELINE(1)
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            data[index][i] = fromMemory.read();    //dram[baseAddress | i];
        }

        control[index].tag[0] = tag;
        control[index].valid[0] = 1;
    }
};

template<int Size, int Blocksize, CacheReplacementPolicy Policy>
class DCache<Size, Blocksize, 1, Policy> : public BaseCache<Size, Blocksize, 1, Policy>
{
    using BaseCache<Size, Blocksize, 1, Policy>::data;
    using BaseCache<Size, Blocksize, 1, Policy>::read;
    using BaseCache<Size, Blocksize, 1, Policy>::write;
public:
    explicit DCache() : BaseCache<Size, Blocksize, 1, Policy>()
    {
        for(int i = 0; i < sets; ++i)
            control[i].valid[0] = 0;
    }

    HLS_DESIGN(interface)
    void run(ac_channel<DCacheRequest>& fromCore, ac_channel<CORE_UINT(32)>& toCore, ac_channel<DCacheRequest>& toMemory, ac_channel<CORE_UINT(32)>& fromMemory)
    {
        #ifndef __SYNTHESIS__
        while ( fromCore.available(1) )
        #endif
        {
            DCacheRequest request = fromCore.read();     // blocking read, replace with non blocking
            CORE_UINT(tagbits) tag = getTag(request.address);
            CORE_UINT(indexbits) index = getIndex(request.address);
            CORE_UINT(offsetbits) offset = getOffset(request.address);
            CORE_UINT(32) baseAddress = 0;
            baseAddress.SET_SLC(offsetbits, request.address.SLC(tagbits+indexbits, offsetbits));

            CORE_UINT(32) value;// = read(index, offset, request.dataSize);
            CORE_UINT(tagbits) memtag;
            bool valid, dirty;
            load(index, offset, value, memtag, valid, dirty);

            if(memtag == tag && valid)
            {
                if(request.RnW) //read
                {
                    read(value, offset, request.dataSize);
                    toCore.write(value);
                }
                else            //write
                {
                    write(index, offset, request.dataSize, request.datum, value);
                    control[index].dirty[0] = 1;
                }
            }
            else   // not found or invalid data
            {
                if(valid && dirty);
                    writeBack(toMemory, tag, index);

                fetch(toMemory, fromMemory, request.address, value);
                if(request.RnW) //read
                {
                    toCore.write(value);
                    control[index].dirty[0] = 0;
                }
                else            //write
                {
                    write(index, offset, request.dataSize, request.datum, value);
                    control[index].dirty[0] = 1;
                }
            }
        }
    }

protected:
//#pragma map to register? to sram? if sram, maybe merge with data(and make an array of struct)
    DCacheControl<Size, Blocksize, 1> control[sets];

    void load(CORE_UINT(indexbits) index, CORE_UINT(offsetbits) offset, CORE_UINT(32)& value, CORE_UINT(tagbits)& tag, bool& valid, bool& dirty)
    {
        value = data[index][offset >> 2];
        tag = control[index].tag[0];
        valid = control[index].valid[0];
        dirty = control[index].dirty[0];
    }

    void fetch(ac_channel<DCacheRequest>& toMemory, ac_channel<CORE_UINT(32)>& fromMemory, CORE_UINT(32) address, CORE_UINT(32)& value)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);
        CORE_UINT(offsetbits) offset = getOffset(address);
        CORE_UINT(32) baseAddress = (address.SLC(tagbits+indexbits, offsetbits) << offsetbits) | 0;

        HLS_PIPELINE(1)
        fetchloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            DCacheRequest request;
            request.address = baseAddress | i;
            request.RnW = DCacheRequest::READ;
            request.dataSize = DCacheRequest::FourBytes;
            request.datum = 0;
            toMemory.write(request);
            CORE_UINT(32) tmp = fromMemory.read();
            data[index][i] = tmp;    //dram[baseAddress | i];
            if(i == offset)
                value = tmp;
        }

        control[index].tag[0] = tag;
        control[index].valid[0] = 1;
    }

    CORE_UINT(1) find(CORE_UINT(32) address)
    {
        CORE_UINT(tagbits) tag = getTag(address);
        CORE_UINT(indexbits) index = getIndex(address);

        CORE_UINT(1) valid = 0;
        CORE_UINT(1) found = 0;

        if(control[index].tag[0] == tag)
        {
            valid = control[index].valid[0];
            found = 1;
            //break;
        }


        return found && valid;
    }

    void read(CORE_UINT(32)& value, CORE_UINT(offsetbits) offset, CORE_UINT(2) dataSize)
    {
        switch(offset & 3)  //offset.SLC(2,0) interprété comme opérateur < ....
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

    void writeBack(ac_channel<DCacheRequest>& toMemory, CORE_UINT(tagbits) tag, CORE_UINT(indexbits) index)
    {
        CORE_UINT(32) baseAddress = (tag << (indexbits+offsetbits)) | (index << offsetbits) | 0;

        HLS_PIPELINE(1)
        writeBackloop:for(int i = 0; i < Blocksize/4; ++i)
        {
            DCacheRequest request;
            request.address = baseAddress | i;
            request.RnW = DCacheRequest::WRITE;
            request.dataSize = DCacheRequest::FourBytes;
            request.datum = data[index][i];
            toMemory.write(request);
            //dram[baseAddress | i] = data[index][way][i];
        }

        // data is still valid, no need to invalidate although it is likely to be replaced
        //control[index][way].valid = 0;
    }
};



/*template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
using ICache =              BaseCache<Size, Blocksize, Associativity, Policy>;
template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
using DCache = Cache<Size, Blocksize, Associativity, Policy>;
typedef CacheRequest        DCacheRequest;*/

/** Specialisation
 * A<x, y>
 *
 * B<y> : public A<int, y>
 * a essayer
 */

//} // __hls_cache namespace

#endif // CACHE_H
