#include "cache.h"

bool find(ICacheControl& ictrl, ac_int<32, false> address)
{
    bool found = false;
    bool valid = false;

    #pragma hls_unroll yes
    findiloop:for(int i = 0; i < Associativity; ++i)
    {
        if((ictrl.setctrl.tag[i] == getTag(address)) && ictrl.setctrl.valid[i])
        {
            found = true;
            valid = true;
            ictrl.currentway = i;
        }
    }

    return found && valid;
}

bool find(DCacheControl& dctrl, ac_int<32, false> address)
{
    bool found = false;
    bool valid = false;

    #pragma hls_unroll yes
    finddloop:for(int i = 0; i < Associativity; ++i)
    {
        if((dctrl.setctrl.tag[i] == getTag(address)) && dctrl.setctrl.valid[i])
        {
            found = true;
            valid = true;
            dctrl.currentway = i;
        }
    }

    return found && valid;
}

void select(ICacheControl& ictrl)
{
#if Policy == FIFO
    ictrl.currentway = ictrl.setctrl.policy++;
#elif Policy == LRU
    if(ictrl.setctrl.policy.slc<3>(3) == 0)
    {
        ictrl.currentway = 3;
    }
    else if(ictrl.setctrl.policy.slc<2>(1) == 0)
    {
        ictrl.currentway = 2;
    }
    else if(ictrl.setctrl.policy.slc<1>(0) == 0)
    {
        ictrl.currentway = 1;
    }
    else
    {
        ictrl.currentway = 0;
    }
#elif Policy == RANDOM
    ictrl.currentway = ictrl.policy.slc<ac::log2_ceil<Associativity>::val>(0);     // ictrl.policy & (Associativity - 1)
    ictrl.policy = (ictrl.policy.slc<1>(31) ^ ictrl.policy.slc<1>(21) ^ ictrl.policy.slc<1>(1) ^ ictrl.policy.slc<1>(0)) | (ictrl.policy << 1);
#else   // None
    ictrl.currentway = 0;
#endif
}

void select(DCacheControl& dctrl)
{
#if Policy == FIFO
    dctrl.currentway = dctrl.setctrl.policy++;
#elif Policy == LRU
    if(dctrl.setctrl.policy.slc<3>(3) == 0)
    {
        dctrl.currentway = 3;
    }
    else if(dctrl.setctrl.policy.slc<2>(1) == 0)
    {
        dctrl.currentway = 2;
    }
    else if(dctrl.setctrl.policy.slc<1>(0) == 0)
    {
        dctrl.currentway = 1;
    }
    else
    {
        dctrl.currentway = 0;
    }
#elif Policy == RANDOM
    dctrl.currentway = dctrl.policy.slc<ac::log2_ceil<Associativity>::val>(0);     // dctrl.policy & (Associativity - 1)
    dctrl.policy = (dctrl.policy.slc<1>(31) ^ dctrl.policy.slc<1>(21) ^ dctrl.policy.slc<1>(1) ^ dctrl.policy.slc<1>(0)) | (dctrl.policy << 1);
#else   // None
    dctrl.currentway = 0;
#endif
}

void update_policy(ICacheControl& ictrl)
{
#if Policy == FIFO
    // no promotion
#elif Policy == LRU
    switch (ictrl.currentway) {
    case 0:
        ictrl.setctrl.policy.set_slc(0, (ac_int<1, false>)0);
        ictrl.setctrl.policy.set_slc(1, (ac_int<1, false>)0);
        ictrl.setctrl.policy.set_slc(3, (ac_int<1, false>)0);
        break;
    case 1:
        ictrl.setctrl.policy.set_slc(0, (ac_int<1, false>)1);
        ictrl.setctrl.policy.set_slc(2, (ac_int<1, false>)0);
        ictrl.setctrl.policy.set_slc(4, (ac_int<1, false>)0);
        break;
    case 2:
        ictrl.setctrl.policy.set_slc(1, (ac_int<1, false>)1);
        ictrl.setctrl.policy.set_slc(2, (ac_int<1, false>)1);
        ictrl.setctrl.policy.set_slc(5, (ac_int<1, false>)0);
        break;
    case 3:
        ictrl.setctrl.policy.set_slc(3, (ac_int<1, false>)1);
        ictrl.setctrl.policy.set_slc(4, (ac_int<1, false>)1);
        ictrl.setctrl.policy.set_slc(5, (ac_int<1, false>)1);
        break;
    default:
        break;
    }
#elif Policy == RANDOM
    // no promotion
#else   // None

#endif
}

void update_policy(DCacheControl& dctrl)
{
#if Policy == FIFO
    // no promotion
#elif Policy == LRU
    switch (dctrl.currentway) {
    case 0:
        dctrl.setctrl.policy.set_slc(0, (ac_int<1, false>)0);
        dctrl.setctrl.policy.set_slc(1, (ac_int<1, false>)0);
        dctrl.setctrl.policy.set_slc(3, (ac_int<1, false>)0);
        break;
    case 1:
        dctrl.setctrl.policy.set_slc(0, (ac_int<1, false>)1);
        dctrl.setctrl.policy.set_slc(2, (ac_int<1, false>)0);
        dctrl.setctrl.policy.set_slc(4, (ac_int<1, false>)0);
        break;
    case 2:
        dctrl.setctrl.policy.set_slc(1, (ac_int<1, false>)1);
        dctrl.setctrl.policy.set_slc(2, (ac_int<1, false>)1);
        dctrl.setctrl.policy.set_slc(5, (ac_int<1, false>)0);
        break;
    case 3:
        dctrl.setctrl.policy.set_slc(3, (ac_int<1, false>)1);
        dctrl.setctrl.policy.set_slc(4, (ac_int<1, false>)1);
        dctrl.setctrl.policy.set_slc(5, (ac_int<1, false>)1);
        break;
    default:
        break;
    }
#elif Policy == RANDOM
    // no promotion
#else   // None

#endif
}

void icache(ICacheControl& ictrl, unsigned int imem[N], unsigned int data[Sets][Blocksize][Associativity],      // control, memory and cachedata
           ac_int<32, false> address,                                                               // from cpu
           ac_int<32, false>& cachepc, int& instruction, bool& insvalid                             // to cpu
#ifndef __SYNTHESIS__
           , uint64_t cycles
#endif
           )
{
#ifdef Debug
    unsigned int cacheaddress[Sets][Associativity];

    for(int i(0); i < Sets; ++i)
    {
        for(int j(0); j < Associativity; ++j)
        {
            cacheaddress[i][j] = ((ictrl.tag[i][j] << (tagshift-2)) | (i << (setshift-2))) << 2;
        }
    }
#endif
    if(address == 0x100b4)
        debug("\n");

    if(cycles == 373)
        debug("\n");

    switch(ictrl.state)
    {
    case IState::Start:
        insvalid = false;
        ictrl.state = IState::Idle;
        ictrl.currentset = getSet(address);
        break;
    case IState::Idle:
        if(ictrl.currentset != getSet(address))  // different way but same set keeps same control
        {
            ictrl.state = IState::StoreControl;
            debug("address %06x storecontrol\n", (int)address);
        }
        else
        {
            //debug("%5d  ", cycles);
            //debug("%5d   cacheenable     ", cycles);

            ictrl.currentset = getSet(address);//(address & setmask) >> setshift;
            ictrl.i = getOffset(address);
            #pragma hls_unroll yes
            loadiset:for(int i = 0; i < Associativity; ++i)
            {
                ictrl.setctrl.data[i] = data[ictrl.currentset][ictrl.i][i];
                ictrl.setctrl.tag[i] = ictrl.tag[ictrl.currentset][i];
                ictrl.setctrl.valid[i] = ictrl.valid[ictrl.currentset][i];
            #if Policy == FIFO || Policy == LRU
                ictrl.setctrl.policy = ictrl.policy[ictrl.currentset];
            #endif
            }

            if(find(ictrl, address))
            {
                /*debug("%2d %2d    ", ictrl.currentset.to_int(), ictrl.currentway.to_int());
                debug("valid data   ");*/

                instruction = ictrl.setctrl.data[ictrl.currentway];

                ictrl.state = IState::Idle;
                //debug("i @%06x (%06x)   %08x    S:3  %5s   %d  %d\n", address.to_int(), address.to_int() >> 2, instruction, " ", ictrl.currentset.to_int(), ictrl.currentway.to_int());
            }
            else    // not found or invalid
            {
                select(ictrl);
                debug("cim  @%06x   not found or invalid   ", address.to_int());
                ictrl.setctrl.tag[ictrl.currentway] = getTag(address);
                ictrl.workAddress = address;
                ictrl.state = IState::Fetch;
                ictrl.setctrl.valid[ictrl.currentway] = false;
                ictrl.i = getOffset(address);
                ac_int<32, false> wordad;
                wordad.set_slc(0, address.slc<30>(2));
                wordad.set_slc(30, (ac_int<2, false>)0);
                debug("starting fetching to %d %d from %06x to %06x (%06x to %06x)\n", ictrl.currentset.to_int(), ictrl.currentway.to_int(), (wordad.to_int() << 2)&(tagmask+setmask),
                      (((int)(wordad.to_int()+Blocksize) << 2)&(tagmask+setmask))-1, (address >> 2).to_int() & (~(blockmask >> 2)), (((address >> 2).to_int() + Blocksize) & (~(blockmask >> 2)))-1 );
                ictrl.valuetowrite = imem[wordad];
                // critical word first
                instruction = ictrl.valuetowrite;

                //debug("i @%06x (%06x)   %08x    S:3  %5s   %d  %d\n", address.to_int(), address.to_int() >> 2, instruction, " ", ictrl.currentset.to_int(), ictrl.currentway.to_int());
            }

            update_policy(ictrl);
            insvalid = true;
            cachepc = address;
        }
        break;
    case IState::StoreControl:
        #pragma hls_unroll yes
        storeicontrol:for(int i = 0; i < Associativity; ++i)
        {
            ictrl.tag[ictrl.currentset][i] = ictrl.setctrl.tag[i];
            ictrl.valid[ictrl.currentset][i] = ictrl.setctrl.valid[i];
        #if Policy == FIFO || Policy == LRU
            ictrl.policy[ictrl.currentset] = ictrl.setctrl.policy;
        #endif
        }

        ictrl.state = IState::Idle;
        ictrl.currentset = getSet(address);  //use workaddress?
        break;
    case IState::Fetch:
        data[ictrl.currentset][ictrl.i][ictrl.currentway] = ictrl.valuetowrite;
        insvalid = false;

        if(++ictrl.i != getOffset(ictrl.workAddress))
        {
            ac_int<32, false> bytead;
            setTag(bytead, ictrl.setctrl.tag[ictrl.currentway]);
            setSet(bytead, ictrl.currentset);
            setOffset(bytead, ictrl.i);

            simul(
            for(unsigned int i(0); i < sizeof(int); ++i)
            {
                //debug("fetching %02x from %06x (%06x)\n", (imem[bytead >> 2] >> (i*8))&0xFF, (bytead.to_int()) + i, bytead.to_int() >> 2);
            });

            ictrl.valuetowrite = imem[bytead >> 2];
            instruction = ictrl.valuetowrite;
            cachepc = ictrl.workAddress;
            cachepc.set_slc(2, ictrl.i);
            insvalid = true;
        }
        else
        {
            ictrl.state = IState::StoreControl;
            ictrl.setctrl.valid[ictrl.currentway] = true;
            //debug("end of fetch to %d %d\n", ictrl.currentset.to_int(), ictrl.currentway.to_int());
            insvalid = false;
        }
        break;
    default:
        insvalid = false;
        ictrl.state = IState::Start;
        break;
    }

    simul(if(insvalid)
    {
        cachedebug("i    @%06x   %08x\n", cachepc.to_int(), instruction);
    })

}

void dcache(DCacheControl& dctrl, unsigned int dmem[N], unsigned int data[Sets][Blocksize][Associativity],      // control, memory and cachedata
           ac_int<32, false> address, ac_int<2, false> datasize, bool signenable, bool dcacheenable, bool writeenable, int writevalue,    // from cpu
           int& read, bool& datavalid                                                       // to cpu
#ifndef __SYNTHESIS__
           , uint64_t cycles
#endif
           )
{
    /*if(dcacheenable && datavalid)   // we can avoid storing control if we hit same set multiple times in a row
    {
        if(dctrl.currentset != getSet(address))
        {
            dctrl.state == DState::StoreControl;
        }
        else
        {
            dctrl.state == DState::Idle;
        }
    }
    else if(!dcacheenable && datavalid)
    {
        dctrl.state == DState::StoreControl;
    }*/

    switch(dctrl.state)
    {
    case DState::Idle:
        if(dcacheenable)
        {
            dctrl.currentset = getSet(address);
            dctrl.i = getOffset(address);
            #pragma hls_unroll yes
            loaddset:for(int i = 0; i < Associativity; ++i)
            {
                dctrl.setctrl.data[i] = data[dctrl.currentset][dctrl.i][i];
                dctrl.setctrl.tag[i] = dctrl.tag[dctrl.currentset][i];
                dctrl.setctrl.dirty[i] = dctrl.dirty[dctrl.currentset][i];
                dctrl.setctrl.valid[i] = dctrl.valid[dctrl.currentset][i];
            #if Policy == FIFO || Policy == LRU
                dctrl.setctrl.policy = dctrl.policy[dctrl.currentset];
            #endif
            }

            if(find(dctrl, address))
            {
                if(writeenable)
                {
                    dctrl.valuetowrite = dctrl.setctrl.data[dctrl.currentway];
                    formatwrite(address, datasize, dctrl.valuetowrite, writevalue);
                    dctrl.workAddress = address;
                    dctrl.setctrl.dirty[dctrl.currentway] = true;

                    dctrl.state = DState::StoreData;
                }
                else
                {
                    ac_int<32, false> r = dctrl.setctrl.data[dctrl.currentway];
                    formatread(address, datasize, signenable, r);

                    read = r;

                    dctrl.state = DState::StoreControl;
                }
                update_policy(dctrl);
                datavalid = true;
            }
            else    // not found or invalid
            {
                select(dctrl);
                cachedebug("cdm  @%06x   not found or invalid   ", address.to_int());
                if(dctrl.setctrl.dirty[dctrl.currentway] && dctrl.setctrl.valid[dctrl.currentway])
                {
                    dctrl.state = DState::FirstWriteBack;
                    dctrl.i = 0;
                    dctrl.workAddress = 0;
                    setTag(dctrl.workAddress, dctrl.setctrl.tag[dctrl.currentway]);
                    setSet(dctrl.workAddress, dctrl.currentset);
                    //dctrl.valuetowrite = dctrl.setctrl.data[dctrl.currentway];    // only if same offset than requested address
                    datavalid = false;
                    cachedebug("starting writeback from %d %d from %06x to %06x\n", dctrl.currentset.to_int(), dctrl.currentway.to_int(), dctrl.workAddress.to_int() >> 2, (dctrl.workAddress.to_int() >> 2) + Blocksize-1);
                }
                else
                {
                    dctrl.setctrl.tag[dctrl.currentway] = getTag(address);
                    dctrl.workAddress = address;
                    dctrl.state = DState::Fetch;
                    dctrl.setctrl.valid[dctrl.currentway] = false;
                    dctrl.i = getOffset(address);
                    ac_int<32, false> wordad;
                    wordad.set_slc(0, address.slc<30>(2));
                    wordad.set_slc(30, (ac_int<2, false>)0);
                    cachedebug("starting fetching to %d %d for %s from %06x to %06x (%06x to %06x)\n", dctrl.currentset.to_int(), dctrl.currentway.to_int(), writeenable?"W":"R", (wordad.to_int() << 2)&(tagmask+setmask),
                          (((int)(wordad.to_int()+Blocksize) << 2)&(tagmask+setmask))-1, (address >> 2).to_int() & (~(blockmask >> 2)), (((address >> 2).to_int() + Blocksize) & (~(blockmask >> 2)))-1 );
                    dctrl.valuetowrite = dmem[wordad];
                    // critical word first
                    if(writeenable)
                    {
                        formatwrite(address, datasize, dctrl.valuetowrite, writevalue);
                        dctrl.setctrl.dirty[dctrl.currentway] = true;
                    }
                    else
                    {
                        ac_int<32, false> r = dctrl.valuetowrite;
                        dctrl.setctrl.dirty[dctrl.currentway] = false;
                        formatread(address, datasize, signenable, r);
                        read = r;
                    }

                    datavalid = true;
                }
            }
        }
        else
            datavalid = false;
        break;
    case DState::StoreControl:
        #pragma hls_unroll yes
        storedcontrol:for(int i = 0; i < Associativity; ++i)
        {
            dctrl.tag[dctrl.currentset][i] = dctrl.setctrl.tag[i];
            dctrl.dirty[dctrl.currentset][i] = dctrl.setctrl.dirty[i];
            dctrl.valid[dctrl.currentset][i] = dctrl.setctrl.valid[i];
        #if Policy == FIFO || Policy == LRU
            dctrl.policy[dctrl.currentset] = dctrl.setctrl.policy;
        #endif
        }

        dctrl.state = DState::Idle;
        datavalid = false;
        break;
    case DState::StoreData:
        #pragma hls_unroll yes
        storedata:for(int i = 0; i < Associativity; ++i)
        {
            dctrl.tag[dctrl.currentset][i] = dctrl.setctrl.tag[i];
            dctrl.dirty[dctrl.currentset][i] = dctrl.setctrl.dirty[i];
            dctrl.valid[dctrl.currentset][i] = dctrl.setctrl.valid[i];
        #if Policy == FIFO || Policy == LRU
            dctrl.policy[dctrl.currentset] = dctrl.setctrl.policy;
        #endif
        }
        data[dctrl.currentset][getOffset(dctrl.workAddress)][dctrl.currentway] = dctrl.valuetowrite;

        dctrl.state = DState::Idle;
        datavalid = false;
        break;
    case DState::FirstWriteBack:
    {   //bracket for scope and allow compilation
        dctrl.i = 0;
        ac_int<32, false> bytead = 0;
        setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
        setSet(bytead, dctrl.currentset);
        setOffset(bytead, dctrl.i);

        dctrl.valuetowrite = data[dctrl.currentset][dctrl.i][dctrl.currentway];
        dctrl.state = DState::WriteBack;
        datavalid = false;
        break;
    }
    case DState::WriteBack:
    {   //bracket for scope and allow compilation
        ac_int<32, false> bytead = 0;
        setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
        setSet(bytead, dctrl.currentset);
        setOffset(bytead, dctrl.i);
        dmem[bytead >> 2] = dctrl.valuetowrite;

        if(++dctrl.i)
            dctrl.valuetowrite = data[dctrl.currentset][dctrl.i][dctrl.currentway];
        else
        {
            dctrl.state = DState::StoreControl;
            dctrl.setctrl.dirty[dctrl.currentway] = false;
            //debug("end of writeback\n");
        }

        datavalid = false;
        break;
    }
    case DState::Fetch:
        data[dctrl.currentset][dctrl.i][dctrl.currentway] = dctrl.valuetowrite;

        if(++dctrl.i != getOffset(dctrl.workAddress))
        {
            ac_int<32, false> bytead;
            setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
            setSet(bytead, dctrl.currentset);
            setOffset(bytead, dctrl.i);

            dctrl.valuetowrite = dmem[bytead >> 2];
        }
        else
        {
            dctrl.state = DState::StoreControl;
            dctrl.setctrl.valid[dctrl.currentway] = true;
            update_policy(dctrl);
            //debug("end of fetch to %d %d\n", dctrl.currentset.to_int(), dctrl.currentway.to_int());
        }

        datavalid = false;
        break;
    default:
        datavalid = false;
        dctrl.state = DState::Idle;
        break;
    }

    simul(if(datavalid)
    {
        if(writeenable)
            cachedebug("dW%d  @%06x   %08x   %08x   %08x   %d %d\n", datasize.to_int(), address.to_int(), dctrl.state == DState::Fetch?dmem[address/4]:data[dctrl.currentset][dctrl.i][dctrl.currentway],
                                                                      writevalue, dctrl.valuetowrite.to_int(), dctrl.currentset.to_int(), dctrl.currentway.to_int());
        else
            cachedebug("dR%d  @%06x   %08x   %08x   %5s   %d %d\n", datasize.to_int(), address.to_int(), dctrl.state == DState::Fetch?dmem[address/4]:data[dctrl.currentset][dctrl.i][dctrl.currentway],
                                                                    read, signenable?"true":"false", dctrl.currentset.to_int(), dctrl.currentway.to_int());
    })
}


