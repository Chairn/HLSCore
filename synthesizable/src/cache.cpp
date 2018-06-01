#include "cache.h"

bool find(ICacheControl& ctrl, ac_int<32, false> address)
{
    bool found = false;
    bool valid = false;

    #pragma hls_unroll yes
    findiloop:for(int i = 0; i < Associativity; ++i)
    {
        if((ctrl.setctrl.tag[i] == getTag(address)) && ctrl.setctrl.valid[i])
        {
            found = true;
            valid = true;
            ctrl.currentway = i;
        }
    }

    return found && valid;
}

bool find(DCacheControl& ctrl, ac_int<32, false> address)
{
    bool found = false;
    bool valid = false;

    #pragma hls_unroll yes
    finddloop:for(int i = 0; i < Associativity; ++i)
    {
        if((ctrl.setctrl.tag[i] == getTag(address)) && ctrl.setctrl.valid[i])
        {
            found = true;
            valid = true;
            ctrl.currentway = i;
        }
    }

    return found && valid;
}

void select(ICacheControl& ctrl)
{
#if Policy == FIFO
    ctrl.currentway = ctrl.setctrl.policy++;
#elif Policy == LRU
    if(ctrl.setctrl.policy.slc<3>(3) == 0)
    {
        ctrl.currentway = 3;
    }
    else if(ctrl.setctrl.policy.slc<2>(1) == 0)
    {
        ctrl.currentway = 2;
    }
    else if(ctrl.setctrl.policy.slc<1>(0) == 0)
    {
        ctrl.currentway = 1;
    }
    else
    {
        ctrl.currentway = 0;
    }
#elif Policy == RANDOM
    ctrl.currentway = ctrl.policy.slc<ac::log2_ceil<Associativity>::val>(0);     // ctrl.policy & (Associativity - 1)
    ctrl.policy = (ctrl.policy.slc<1>(31) ^ ctrl.policy.slc<1>(21) ^ ctrl.policy.slc<1>(1) ^ ctrl.policy.slc<1>(0)) | (ctrl.policy << 1);
#else   // None
    ctrl.currentway = 0;
#endif
}

void select(DCacheControl& ctrl)
{
#if Policy == FIFO
    ctrl.currentway = ctrl.setctrl.policy++;
#elif Policy == LRU
    if(ctrl.setctrl.policy.slc<3>(3) == 0)
    {
        ctrl.currentway = 3;
    }
    else if(ctrl.setctrl.policy.slc<2>(1) == 0)
    {
        ctrl.currentway = 2;
    }
    else if(ctrl.setctrl.policy.slc<1>(0) == 0)
    {
        ctrl.currentway = 1;
    }
    else
    {
        ctrl.currentway = 0;
    }
#elif Policy == RANDOM
    ctrl.currentway = ctrl.policy.slc<ac::log2_ceil<Associativity>::val>(0);     // ctrl.policy & (Associativity - 1)
    ctrl.policy = (ctrl.policy.slc<1>(31) ^ ctrl.policy.slc<1>(21) ^ ctrl.policy.slc<1>(1) ^ ctrl.policy.slc<1>(0)) | (ctrl.policy << 1);
#else   // None
    ctrl.currentway = 0;
#endif
}

void update_policy(ICacheControl& ctrl)
{
#if Policy == FIFO
    // no promotion
#elif Policy == LRU
    switch (ctrl.currentway) {
    case 0:
        ctrl.setctrl.policy.set_slc(0, (ac_int<1, false>)0);
        ctrl.setctrl.policy.set_slc(1, (ac_int<1, false>)0);
        ctrl.setctrl.policy.set_slc(3, (ac_int<1, false>)0);
        break;
    case 1:
        ctrl.setctrl.policy.set_slc(0, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(2, (ac_int<1, false>)0);
        ctrl.setctrl.policy.set_slc(4, (ac_int<1, false>)0);
        break;
    case 2:
        ctrl.setctrl.policy.set_slc(1, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(2, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(5, (ac_int<1, false>)0);
        break;
    case 3:
        ctrl.setctrl.policy.set_slc(3, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(4, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(5, (ac_int<1, false>)1);
        break;
    default:
        break;
    }
#elif Policy == RANDOM
    // no promotion
#else   // None

#endif
}

void update_policy(DCacheControl& ctrl)
{
#if Policy == FIFO
    // no promotion
#elif Policy == LRU
    switch (ctrl.currentway) {
    case 0:
        ctrl.setctrl.policy.set_slc(0, (ac_int<1, false>)0);
        ctrl.setctrl.policy.set_slc(1, (ac_int<1, false>)0);
        ctrl.setctrl.policy.set_slc(3, (ac_int<1, false>)0);
        break;
    case 1:
        ctrl.setctrl.policy.set_slc(0, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(2, (ac_int<1, false>)0);
        ctrl.setctrl.policy.set_slc(4, (ac_int<1, false>)0);
        break;
    case 2:
        ctrl.setctrl.policy.set_slc(1, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(2, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(5, (ac_int<1, false>)0);
        break;
    case 3:
        ctrl.setctrl.policy.set_slc(3, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(4, (ac_int<1, false>)1);
        ctrl.setctrl.policy.set_slc(5, (ac_int<1, false>)1);
        break;
    default:
        break;
    }
#elif Policy == RANDOM
    // no promotion
#else   // None

#endif
}

void icache(ICacheControl& ctrl, unsigned int imem[N], unsigned int data[Sets][Blocksize][Associativity],      // control, memory and cachedata
           ac_int<32, false> address,                                                               // from cpu
           ac_int<32, false>& cachepc, int& instruction, bool& insvalid                             // to cpu
#ifndef __SYNTHESIS__
           , uint64_t cycles
#endif
           )
{
    switch(ctrl.state)
    {
    case IState::Start:
        insvalid = false;
        ctrl.state = IState::Idle;
        ctrl.currentset = getSet(address);
        break;
    case IState::Idle:
        if(ctrl.currentset != getSet(address))  // different way but same set keeps same control
        {
            ctrl.state = IState::StoreControl;
            debug("address %06x storecontrol\n", (int)address);
        }
        else
        {
            //debug("%5d  ", cycles);
            //debug("%5d   cacheenable     ", cycles);

            ctrl.currentset = getSet(address);//(address & setmask) >> setshift;
            ctrl.i = getOffset(address);
            #pragma hls_unroll yes
            loadiset:for(int i = 0; i < Associativity; ++i)
            {
                ctrl.setctrl.data[i] = data[ctrl.currentset][ctrl.i][i];
                ctrl.setctrl.tag[i] = ctrl.tag[ctrl.currentset][i];
                ctrl.setctrl.valid[i] = ctrl.valid[ctrl.currentset][i];
            #if Policy == FIFO || Policy == LRU
                ctrl.setctrl.policy = ctrl.policy[ctrl.currentset];
            #endif
            }

            if(find(ctrl, address))
            {
                /*debug("%2d %2d    ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                debug("valid data   ");*/

                instruction = ctrl.setctrl.data[ctrl.currentway];

                ctrl.state = IState::Idle;
                //debug("i @%06x (%06x)   %08x    S:3  %5s   %d  %d\n", address.to_int(), address.to_int() >> 2, instruction, " ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
            }
            else    // not found or invalid
            {
                select(ctrl);
                debug("cim  @%06x   not found or invalid   ", address.to_int());
                ctrl.setctrl.tag[ctrl.currentway] = getTag(address);
                ctrl.workAddress = address;
                ctrl.state = IState::Fetch;
                ctrl.setctrl.valid[ctrl.currentway] = false;
                ctrl.i = getOffset(address);
                ac_int<32, false> wordad;
                wordad.set_slc(0, address.slc<30>(2));
                wordad.set_slc(30, (ac_int<2, false>)0);
                debug("starting fetching to %d %d from %06x to %06x (%06x to %06x)\n", ctrl.currentset.to_int(), ctrl.currentway.to_int(), (wordad.to_int() << 2)&(tagmask+setmask),
                      (((int)(wordad.to_int()+Blocksize) << 2)&(tagmask+setmask))-1, ((address % N) >> 2).to_int() & (~(blockmask >> 2)), ((((address % N) >> 2).to_int() + Blocksize) & (~(blockmask >> 2)))-1 );
                ctrl.valuetowrite = imem[wordad];
                // critical word first
                instruction = ctrl.valuetowrite;

                //debug("i @%06x (%06x)   %08x    S:3  %5s   %d  %d\n", address.to_int(), address.to_int() >> 2, instruction, " ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
            }

            update_policy(ctrl);
            insvalid = true;
            cachepc = address;
        }
        break;
    case IState::StoreControl:
        #pragma hls_unroll yes
        storeicontrol:for(int i = 0; i < Associativity; ++i)
        {
            ctrl.tag[ctrl.currentset][i] = ctrl.setctrl.tag[i];
            ctrl.valid[ctrl.currentset][i] = ctrl.setctrl.valid[i];
        #if Policy == FIFO || Policy == LRU
            ctrl.policy[ctrl.currentset] = ctrl.setctrl.policy;
        #endif
        }

        ctrl.state = IState::Idle;
        ctrl.currentset = getSet(address);  //use workaddress?
        break;
    case IState::Fetch:
        data[ctrl.currentset][ctrl.i][ctrl.currentway] = ctrl.valuetowrite;
        insvalid = false;

        if(++ctrl.i != getOffset(ctrl.workAddress))
        {
            ac_int<32, false> bytead;
            setTag(bytead, ctrl.setctrl.tag[ctrl.currentway]);
            setSet(bytead, ctrl.currentset);
            setOffset(bytead, ctrl.i);

            simul(
            for(unsigned int i(0); i < sizeof(int); ++i)
            {
                //debug("fetching %02x from %06x (%06x)\n", (imem[bytead >> 2] >> (i*8))&0xFF, (bytead.to_int()) + i, bytead.to_int() >> 2);
            });

            ctrl.valuetowrite = imem[bytead >> 2];
            instruction = ctrl.valuetowrite;
            cachepc = ctrl.workAddress;
            cachepc.set_slc(2, ctrl.i);
            insvalid = true;
        }
        else
        {
            ctrl.state = IState::StoreControl;
            ctrl.setctrl.valid[ctrl.currentway] = true;
            //debug("end of fetch to %d %d\n", ctrl.currentset.to_int(), ctrl.currentway.to_int());
            insvalid = false;
        }
        break;
    default:
        insvalid = false;
        ctrl.state = IState::Start;
        break;
    }

    simul(if(insvalid)
    {
        cachedebug("ci   @%06x   %08x   R   S:3\n", cachepc.to_int(), instruction);
    })

}

void dcache(DCacheControl& ctrl, unsigned int dmem[N], unsigned int data[Sets][Blocksize][Associativity],      // control, memory and cachedata
           ac_int<32, false> address, ac_int<2, false> datasize, bool signenable, bool cacheenable, bool writeenable, int writevalue,    // from cpu
           int& read, bool& datavalid                                                       // to cpu
#ifndef __SYNTHESIS__
           , uint64_t cycles
#endif
           )
{
    switch(ctrl.state)
    {
    case DState::Idle:
        if(cacheenable)
        {
            //debug("%5d  ", cycles);
            //debug("%5d   cacheenable     ", cycles);
            ctrl.currentset = getSet(address);//(address & setmask) >> setshift;
            ctrl.i = getOffset(address);
            #pragma hls_unroll yes
            loaddset:for(int i = 0; i < Associativity; ++i)
            {
                ctrl.setctrl.data[i] = data[ctrl.currentset][ctrl.i][i];
                ctrl.setctrl.tag[i] = ctrl.tag[ctrl.currentset][i];
                ctrl.setctrl.dirty[i] = ctrl.dirty[ctrl.currentset][i];
                ctrl.setctrl.valid[i] = ctrl.valid[ctrl.currentset][i];
            #if Policy == FIFO || Policy == LRU
                ctrl.setctrl.policy = ctrl.policy[ctrl.currentset];
            #endif
            }

            if(find(ctrl, address))
            {
                /*debug("%2d %2d    ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                debug("valid data   ");*/
                if(writeenable)
                {
                    ctrl.valuetowrite = ctrl.setctrl.data[ctrl.currentway];//data[ctrl.currentset][ctrl.currentway][(address & (ac_int<32, true>)blockmask) >> 2];
                    formatwrite(address, datasize, ctrl.valuetowrite, writevalue);
                    ctrl.workAddress = address;
                    ctrl.setctrl.dirty[ctrl.currentway] = true;
                    /*simul(
                    switch(datasize)
                    {
                    case 0:
                        debug("W:      %02x   ", writevalue);
                        break;
                    case 1:
                        debug("W:    %06x   ", writevalue);
                        break;
                    case 2:
                    case 3:
                        debug("W:%08x   ", writevalue);
                        break;
                    }
                    );*/

                    ctrl.state = DState::StoreData;
                    //debug("d @%06x (%06x) W %08x    S:%d  %5s   %d  %d\n", address.to_int(), address.to_int() >> 2, writevalue, datasize.to_int(), " ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                }
                else
                {                                                //address.slc<ac::log2_ceil<Blocksize>::val>(2) // doesnt work, why?
                    ac_int<32, false> r = ctrl.setctrl.data[ctrl.currentway];//data[ctrl.currentset][ctrl.currentway][(address & (ac_int<32, true>)blockmask) >> 2];
                    //debug("%08x (%08x)", r.to_int(), data[ctrl.currentset][ctrl.currentway][ctrl.i]);
                    formatread(address, datasize, signenable, r);

                    read = r;
                    /*simul(
                    switch(datasize)
                    {
                    case 0:
                        debug("R:      %02x   ", read);
                        break;
                    case 1:
                        debug("R:    %06x   ", read);
                        break;
                    case 2:
                    case 3:
                        debug("R:%08x   ", read);
                        break;
                    }
                    );*/

                    ctrl.state = DState::StoreControl;
                    //debug("d @%06x (%06x) R %08x    S:%d  %5s   %d  %d\n", address.to_int(), address.to_int() >> 2, read, datasize.to_int(), 1?"true":"false", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                }
                datavalid = true;

                /*debug("@%06x    ", address.to_int());
                debug("(@%06x)\n", address.to_int() >> 2);*/
            }
            else    // not found or invalid
            {
                select(ctrl);
                cachedebug("cdm  @%06x   not found or invalid   ", address.to_int());
                if(ctrl.setctrl.dirty[ctrl.currentway] && ctrl.setctrl.valid[ctrl.currentway])
                {
                    ctrl.state = DState::FirstWriteBack;
                    ctrl.i = 0;
                    ctrl.workAddress = 0;//(ctrl.setctrl.tag[ctrl.currentway] << tagshift) | (ctrl.currentset << setshift);
                    setTag(ctrl.workAddress, ctrl.setctrl.tag[ctrl.currentway]);
                    setSet(ctrl.workAddress, ctrl.currentset);
                    //ctrl.valuetowrite = ctrl.setctrl.data[ctrl.currentway];
                    datavalid = false;
                    cachedebug("starting writeback from %d %d from %06x to %06x\n", ctrl.currentset.to_int(), ctrl.currentway.to_int(), ctrl.workAddress.to_int() >> 2, (ctrl.workAddress.to_int() >> 2) + Blocksize-1);
                    //debug("%08x \n", ctrl.valuetowrite.to_int());
                }
                else
                {
                    ctrl.setctrl.tag[ctrl.currentway] = getTag(address);
                    ctrl.workAddress = address;
                    ctrl.state = DState::Fetch;
                    ctrl.setctrl.valid[ctrl.currentway] = false;
                    ctrl.i = getOffset(address);
                    ac_int<32, false> wordad;
                    wordad.set_slc(0, address.slc<30>(2));
                    wordad.set_slc(30, (ac_int<2, false>)0);
                    cachedebug("starting fetching to %d %d for %s from %06x to %06x (%06x to %06x)\n", ctrl.currentset.to_int(), ctrl.currentway.to_int(), writeenable?"W":"R", (wordad.to_int() << 2)&(tagmask+setmask),
                          (((int)(wordad.to_int()+Blocksize) << 2)&(tagmask+setmask))-1, ((address % N) >> 2).to_int() & (~(blockmask >> 2)), ((((address % N) >> 2).to_int() + Blocksize) & (~(blockmask >> 2)))-1 );
                    ctrl.valuetowrite = dmem[wordad];
                    // critical word first
                    if(writeenable)
                    {
                        formatwrite(address, datasize, ctrl.valuetowrite, writevalue);
                        ctrl.setctrl.dirty[ctrl.currentway] = true;
                        //debug("d @%06x (%06x) W %08x    S:%d  %5s   %d  %d\n", address.to_int(), address.to_int() >> 2, writevalue, datasize.to_int(), " ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                    }
                    else
                    {
                        ac_int<32, false> r = ctrl.valuetowrite;
                        ctrl.setctrl.dirty[ctrl.currentway] = false;
                        formatread(address, datasize, signenable, r);
                        read = r;
                        //debug("d @%06x (%06x) R %08x    S:%d  %5s   %d  %d\n", address.to_int(), address.to_int() >> 2, read, datasize.to_int(), 1?"true":"false", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                    }

                    datavalid = true;
                }
            }

            if(ctrl.state != DState::WriteBack && ctrl.state != DState::FirstWriteBack)
                update_policy(ctrl);
        }
        else
            datavalid = false;
        break;
    case DState::StoreControl:
        #pragma hls_unroll yes
        storedcontrol:for(int i = 0; i < Associativity; ++i)
        {
            ctrl.tag[ctrl.currentset][i] = ctrl.setctrl.tag[i];
            ctrl.dirty[ctrl.currentset][i] = ctrl.setctrl.dirty[i];
            ctrl.valid[ctrl.currentset][i] = ctrl.setctrl.valid[i];
        #if Policy == FIFO || Policy == LRU
            ctrl.policy[ctrl.currentset] = ctrl.setctrl.policy;
        #endif
        }

        ctrl.state = DState::Idle;
        datavalid = false;
        break;
    case DState::StoreData:
        #pragma hls_unroll yes
        storedata:for(int i = 0; i < Associativity; ++i)
        {
            ctrl.tag[ctrl.currentset][i] = ctrl.setctrl.tag[i];
            ctrl.dirty[ctrl.currentset][i] = ctrl.setctrl.dirty[i];
            ctrl.valid[ctrl.currentset][i] = ctrl.setctrl.valid[i];
        #if Policy == FIFO || Policy == LRU
            ctrl.policy[ctrl.currentset] = ctrl.setctrl.policy;
        #endif
        }                         //(ctrl.workAddress & (ac_int<32, true>)blockmask) >> 2
        data[ctrl.currentset][getOffset(ctrl.workAddress)][ctrl.currentway] = ctrl.valuetowrite;

        ctrl.state = DState::Idle;
        datavalid = false;
        break;
    case DState::FirstWriteBack:
    {   //bracket for scope and allow compilation
        ctrl.i = 0;
        ac_int<32, false> bytead = 0;
        setTag(bytead, ctrl.setctrl.tag[ctrl.currentway]);
        setSet(bytead, ctrl.currentset);
        setOffset(bytead, ctrl.i);

        simul(
        for(unsigned int i(0); i < sizeof(int); ++i)
        {
            //debug("writing back %02x to %06x (%06x)\n", (dmem[bytead >> 2] >> (i*8))&0xFF, bytead.to_int() + i, (bytead.to_int()+i) >> 2);
        });

        ctrl.valuetowrite = data[ctrl.currentset][ctrl.i][ctrl.currentway];
        ctrl.state = DState::WriteBack;
        datavalid = false;
        break;
    }
    case DState::WriteBack:
    {   //bracket for scope and allow compilation
        ac_int<32, false> bytead = 0;
        setTag(bytead, ctrl.setctrl.tag[ctrl.currentway]);
        setSet(bytead, ctrl.currentset);
        setOffset(bytead, ctrl.i);
        dmem[bytead >> 2] = ctrl.valuetowrite;

        simul(
        for(unsigned int i(0); i < sizeof(int); ++i)
        {
            //debug("writing back %02x to %06x (%06x)\n", (dmem[bytead >> 2] >> (i*8))&0xFF, bytead.to_int() + i, (bytead.to_int()+i) >> 2);
        });

        if(++ctrl.i)
            ctrl.valuetowrite = data[ctrl.currentset][ctrl.i][ctrl.currentway];
        else
        {
            ctrl.state = DState::StoreControl;
            ctrl.setctrl.dirty[ctrl.currentway] = false;
            //debug("end of writeback\n");
        }

        datavalid = false;
        break;
    }
    case DState::Fetch:
        data[ctrl.currentset][ctrl.i][ctrl.currentway] = ctrl.valuetowrite;
        datavalid = false;

        if(++ctrl.i != getOffset(ctrl.workAddress))
        {
            ac_int<32, false> bytead;
            setTag(bytead, ctrl.setctrl.tag[ctrl.currentway]);
            setSet(bytead, ctrl.currentset);
            setOffset(bytead, ctrl.i);

            simul(
            for(unsigned int i(0); i < sizeof(int); ++i)
            {
                //debug("fetching %02x from %06x (%06x)\n", (dmem[bytead >> 2] >> (i*8))&0xFF, (bytead.to_int()) + i, bytead.to_int() >> 2);
            });

            ctrl.valuetowrite = dmem[bytead >> 2];
        }
        else
        {
            ctrl.state = DState::StoreControl;
            ctrl.setctrl.valid[ctrl.currentway] = true;
            //debug("end of fetch to %d %d\n", ctrl.currentset.to_int(), ctrl.currentway.to_int());
        }
        break;
    default:
        datavalid = false;
        ctrl.state = DState::Idle;
        break;
    }

    simul(if(datavalid)
    {
        if(writeenable)
            cachedebug("dW%d  @%06x   %08x   %08x   %08x   %d %d\n", datasize.to_int(), address.to_int(), ctrl.state == DState::Fetch?dmem[address/4]:data[ctrl.currentset][ctrl.i][ctrl.currentway],
                                                                      writevalue, ctrl.valuetowrite.to_int(), ctrl.currentset.to_int(), ctrl.currentway.to_int());
        else
            cachedebug("dR%d  @%06x   %08x   %08x   %5s   %d %d\n", datasize.to_int(), address.to_int(), ctrl.state == DState::Fetch?dmem[address/4]:data[ctrl.currentset][ctrl.i][ctrl.currentway],
                                                                    read, signenable?"true":"false", ctrl.currentset.to_int(), ctrl.currentway.to_int());
    })
}


