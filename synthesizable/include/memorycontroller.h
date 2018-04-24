#ifndef MEMORYCONTROLLER_H
#define MEMORYCONTROLLER_H

#include "portability.h"
#include "cache.h"

template<int Blocksize>
class MemoryController
{
public:
    MemoryController()
    {
        assert(Blocksize % 2 == 0);
        //assert(ac::log2_ceil<Blocksize>::val);
    }

    HLS_DESIGN(interface)
    void run(ac_channel<ICacheRequest>& fromICache, ac_channel<CORE_UINT(32)>& toICache, ac_channel<DCacheRequest>& fromDCache, ac_channel<CORE_UINT(32)>& toDCache)//, CORE_UINT(32) mem[1000000])
    {
        ICacheRequest ireq;
        DCacheRequest dreq;
        bool ir, dr;
        ir = fromICache.nb_read(ireq);
        dr = fromDCache.nb_read(dreq);
        if(ir)   // prioritize instructions because no instruction means stalling
        {
            CORE_UINT(32) baseAddress = 0;
            baseAddress.SET_SLC(32-offsetbits, ireq.address.SLC(32-offsetbits, offsetbits));
            for(int i = 0; i < Blocksize/4; ++i)
            {
                CORE_UINT(32) ins = mem[baseAddress | i];
                toICache.write(ins);
            }
        }
        else if(dr)
        {
            CORE_UINT(32) baseAddress = 0;
            baseAddress.SET_SLC(32-offsetbits, dreq.address.SLC(32-offsetbits, offsetbits));
            for(int i = 0; i < Blocksize/4; ++i)
            {
                if(dreq.RnW == DCacheRequest::READ)
                {
                    CORE_UINT(32) data = mem[baseAddress | i];
                    toDCache.write(data);
                }
                else
                {
                    // todo : change interface for writing
                    // send blocksize/4 data and address?
                    //
                }
            }
        }
    }
private:
    CORE_UINT(32) mem[1000000];
};

#endif // MEMORYCONTROLLER_H
