#include "cache.h"

void format(ac_int<32, false> address, ac_int<2, false> datasize, ac_int<32, false>& read)
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
        break;
    case 1:
        read &= 0x0000FFFF;
        break;
    case 2:
        read &= 0xFFFFFFFF;
        break;
    case 3:
        read &= 0xFFFFFFFF;
        break;
    }
}

void format(ac_int<32, false> address, ac_int<2, false> datasize, ac_int<32, false>& mem, ac_int<32, false> write)
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

bool find(CacheControl& ctrl, ac_int<32, false> address)
{
    bool found = false;
    bool valid = false;

    #pragma hls_unroll yes
    findloop:for(int i = 0; i < Associativity; ++i)
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

void select(CacheControl& ctrl)
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

void update_policy(CacheControl& ctrl)
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

void cache(CacheControl& ctrl, unsigned int dmem[N], unsigned int data[Sets][Blocksize][Associativity],      // control, memory and cachedata
           ac_int<32, false> address, ac_int<2, false> datasize, bool cacheenable, bool writeenable, int writevalue,    // from cpu
           int& read, bool& datavalid                                                       // to cpu
#ifndef __SYNTHESIS__
           , uint64_t cycles
#endif
           )
{
    switch(ctrl.state)
    {
    case Idle:
        if(cacheenable)
        {
            //debug("%5d  ", cycles);
            //debug("%5d   cacheenable     ", cycles);
            ctrl.currentset = getSet(address);//(address & setmask) >> setshift;
            ctrl.i = getOffset(address);
            #pragma hls_unroll yes
            loadset:for(int i = 0; i < Associativity; ++i)
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
                    format(address, datasize, ctrl.valuetowrite, writevalue);
                    ctrl.workAddress = address;
                    ctrl.setctrl.dirty[ctrl.currentway] = true;
                    /*simul(
                    switch(datasize)
                    {
                    case 0:
                        debug("W:      %02x   ", writevalue);
                        break;
                    case 1:
                        debug("W:    %04x   ", writevalue);
                        break;
                    case 2:
                    case 3:
                        debug("W:%08x   ", writevalue);
                        break;
                    }
                    );*/

                    ctrl.state = StoreData;
                    debug("memory enable    W:%08x  @%04x   S:%d    %5s %d  %d\n", writevalue, ((address % 8192) >> 2).to_int(), datasize.to_int(), " ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                }
                else
                {                                                //address.slc<ac::log2_ceil<Blocksize>::val>(2) // doesnt work, why?
                    ac_int<32, false> r = ctrl.setctrl.data[ctrl.currentway];//data[ctrl.currentset][ctrl.currentway][(address & (ac_int<32, true>)blockmask) >> 2];
                    //debug("%08x (%08x)", r.to_int(), data[ctrl.currentset][ctrl.currentway][ctrl.i]);
                    format(address, datasize, r);

                    read = r;
                    /*simul(
                    switch(datasize)
                    {
                    case 0:
                        debug("R:      %02x   ", read);
                        break;
                    case 1:
                        debug("R:    %04x   ", read);
                        break;
                    case 2:
                    case 3:
                        debug("R:%08x   ", read);
                        break;
                    }
                    );*/

                    ctrl.state = StoreControl;
                    debug("memory enable    R:%08x  @%04x   S:%d    %5s %d  %d\n", read, ((address % 8192) >> 2).to_int(), datasize.to_int(), 1?"true":"false", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                }
                datavalid = true;

                /*debug("@%04x    ", address.to_int());
                debug("(@%04x)\n", address.to_int() >> 2);*/
            }
            else    // not found or invalid
            {
                select(ctrl);
                debug("data @%04x (%04x) not found or invalid   ", address.to_int(), address.to_int() >> 2);
                if(ctrl.setctrl.dirty[ctrl.currentway] && ctrl.setctrl.valid[ctrl.currentway])
                {
                    if(ctrl.currentset == 5 && ctrl.currentway == 2)
                        debug("\n");
                    ctrl.state = FirstWriteBack;
                    ctrl.i = 0;
                    ctrl.workAddress = 0;//(ctrl.setctrl.tag[ctrl.currentway] << tagshift) | (ctrl.currentset << setshift);
                    setTag(ctrl.workAddress, ctrl.setctrl.tag[ctrl.currentway]);
                    setSet(ctrl.workAddress, ctrl.currentset);
                    //ctrl.valuetowrite = ctrl.setctrl.data[ctrl.currentway];
                    datavalid = false;
                    debug("starting writeback from %d %d from %04x to %04x\n", ctrl.currentset.to_int(), ctrl.currentway.to_int(), ctrl.workAddress.to_int() >> 2, (ctrl.workAddress.to_int() >> 2) + Blocksize-1);
                    //debug("%08x \n", ctrl.valuetowrite.to_int());
                }
                else
                {
                    ctrl.setctrl.tag[ctrl.currentway] = getTag(address);
                    ctrl.workAddress = address;
                    ctrl.state = Fetch;
                    ctrl.setctrl.valid[ctrl.currentway] = false;
                    ctrl.i = getOffset(address);
                    ac_int<32, false> wordad;
                    wordad.set_slc(0, address.slc<30>(2));
                    wordad.set_slc(30, (ac_int<2, false>)0);
                    debug("starting fetching to %d %d for %s from %04x to %04x (%04x to %04x)\n", ctrl.currentset.to_int(), ctrl.currentway.to_int(), writeenable?"W":"R", (wordad.to_int() << 2)&(tagmask+setmask),
                          (((int)(wordad.to_int()+Blocksize) << 2)&(tagmask+setmask))-1, ((address % 8192) >> 2).to_int() & (~(blockmask >> 2)), ((((address % 8192) >> 2).to_int() + Blocksize) & (~(blockmask >> 2)))-1 );
                    ctrl.valuetowrite = dmem[wordad];
                    // critical word first
                    if(writeenable)
                    {
                        format(address, datasize, ctrl.valuetowrite, writevalue);
                        ctrl.setctrl.dirty[ctrl.currentway] = true;
                        //debug("%5d  Storing data %08x @%04x\n", cycles, ctrl.valuetowrite.to_int(), wordad.to_int());
                        debug("memory enable    W:%08x  @%04x   S:%d    %5s %d  %d\n", writevalue, ((address % 8192) >> 2).to_int(), datasize.to_int(), " ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                    }
                    else
                    {
                        ac_int<32, false> r = ctrl.valuetowrite;
                        ctrl.setctrl.dirty[ctrl.currentway] = false;
                        format(address, datasize, r);
                        read = r;
                        debug("memory enable    R:%08x  @%04x   S:%d    %5s %d  %d\n", read, ((address % 8192) >> 2).to_int(), datasize.to_int(), 1?"true":"false", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                    }

                    datavalid = true;
                }
            }

            if(ctrl.state != WriteBack && ctrl.state != FirstWriteBack)
                update_policy(ctrl);
        }
        else
            datavalid = false;
        break;
    case StoreControl:
        #pragma hls_unroll yes
        storecontrol:for(int i = 0; i < Associativity; ++i)
        {
            ctrl.tag[ctrl.currentset][i] = ctrl.setctrl.tag[i];
            ctrl.dirty[ctrl.currentset][i] = ctrl.setctrl.dirty[i];
            ctrl.valid[ctrl.currentset][i] = ctrl.setctrl.valid[i];
        #if Policy == FIFO || Policy == LRU
            ctrl.policy[ctrl.currentset] = ctrl.setctrl.policy;
        #endif
        }

        ctrl.state = Idle;
        break;
    case StoreData:
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

        ctrl.state = Idle;
        break;
    case FirstWriteBack:
    {   //bracket for scope and allow compilation
        ctrl.i == 0;
        ac_int<32, false> bytead = 0;
        setTag(bytead, ctrl.setctrl.tag[ctrl.currentway]);
        setSet(bytead, ctrl.currentset);
        setOffset(bytead, ctrl.i);

        simul(
        for(unsigned int i(0); i < sizeof(int); ++i)
        {
            //debug("writing back %02x to %04x (%04x)\n", (dmem[bytead >> 2] >> (i*8))&0xFF, bytead.to_int() + i, (bytead.to_int()+i) >> 2);
        });

        ctrl.valuetowrite = data[ctrl.currentset][ctrl.i][ctrl.currentway];
        ctrl.state = WriteBack;
        datavalid = false;
        break;
    }
    case WriteBack:
    {   //bracket for scope and allow compilation
        ac_int<32, false> bytead = 0;
        setTag(bytead, ctrl.setctrl.tag[ctrl.currentway]);
        setSet(bytead, ctrl.currentset);
        setOffset(bytead, ctrl.i);
        dmem[bytead >> 2] = ctrl.valuetowrite;

        simul(
        for(unsigned int i(0); i < sizeof(int); ++i)
        {
            //debug("writing back %02x to %04x (%04x)\n", (dmem[bytead >> 2] >> (i*8))&0xFF, bytead.to_int() + i, (bytead.to_int()+i) >> 2);
        });

        if(++ctrl.i)
            ctrl.valuetowrite = data[ctrl.currentset][ctrl.i][ctrl.currentway];
        else
        {
            ctrl.state = StoreControl;
            ctrl.setctrl.dirty[ctrl.currentway] = false;
            //debug("end of writeback\n");
        }

        datavalid = false;
        break;
    }
    case Fetch:
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
                //debug("fetching %02x from %04x (%04x)\n", (dmem[bytead >> 2] >> (i*8))&0xFF, (bytead.to_int()) + i, bytead.to_int() >> 2);
            });

            ctrl.valuetowrite = dmem[bytead >> 2];
        }
        else
        {
            ctrl.state = StoreControl;
            ctrl.setctrl.valid[ctrl.currentway] = true;
            //debug("end of fetch to %d %d\n", ctrl.currentset.to_int(), ctrl.currentway.to_int());
        }
        break;
    default:
        datavalid = false;
        ctrl.state = Idle;
        break;
    }
}

#if false
#ifndef __SYNTHESIS__
bool
#else
void
#endif
simplecachedcore(int imem[N], int dmem[N], int& res)
{
    static int iaddress = 0;
#ifndef __SYNTHESIS__
    static int cycles = 0;
    if(iaddress > 8192)
    {
        debug("Out of instruction memory\n");
        return true;
    }
#endif

    static int reg[16] = {0};
    static int cachedata[Sets][Associativity][Blocksize] = {0};
    static bool dummy = ac::init_array<AC_VAL_DC>((int*)cachedata, Sets*Associativity*Blocksize);
    (void)dummy;
    static MemtoWB memtowb = {0};
    static DCtoEx dctoex = {0};
    static int ldaddress = 0;
    static int readvalue = 0;
    static ac_int<2, false> datasize;
    static int writevalue = 0;
    static int instruction = 0;
    static int branchjumplock = 0;
    static bool memlock = false;
    static bool cacheenable = false;
    static bool writeenable = false;
    static bool datavalid = false;
    static CacheControl ctrl = {0};
    static bool taginit = ac::init_array<AC_VAL_DC>((ac_int<32-tagshift, false>*)ctrl.tag, Sets*Associativity);
    (void)taginit;
    static bool dirinit = ac::init_array<AC_VAL_DC>((bool*)ctrl.dirty, Sets*Associativity);
    (void)dirinit;
    static bool valinit = ac::init_array<AC_VAL_0>((bool*)ctrl.valid, Sets*Associativity);
    (void)valinit;
#if Policy == FIFO
    static bool polinit = ac::init_array<AC_VAL_DC>((ac_int<ac::log2_ceil<Associativity>::val, false>*)ctrl.policy, Sets);
    (void)polinit;
#elif Policy == LRU
    static bool polinit = ac::init_array<AC_VAL_DC>((ac_int<Associativity * (Associativity-1) / 2, false>*)ctrl.policy, Sets);
    (void)polinit;
#elif Policy == RANDOM
    static bool rndinit = false;
    if(!rndinit)
    {
        rndinit = true;
        ctrl.policy = 0xF2D4B698;
    }
#endif

    cache(ctrl, dmem, cachedata, ldaddress, datasize, cacheenable, writeenable, writevalue, readvalue, datavalid
#ifndef __SYNTHESIS__
          , cycles++
#endif
          );
    Mem(reg, memtowb, ldaddress, datasize, cacheenable, writeenable, writevalue, readvalue, datavalid, memlock, res);
    if(!memlock)
    {
        Ex(dctoex, memtowb, iaddress);
        Dc(instruction, dctoex, branchjumplock, reg);
        Ft(imem, iaddress, instruction, branchjumplock);
    }

#ifndef __SYNTHESIS__
    //cache write back for simulation
    for(unsigned int i  = 0; i < Sets; ++i)
        for(unsigned int j = 0; j < Associativity; ++j)
            if(ctrl.dirty[i][j] && ctrl.valid[i][j])
                for(unsigned int k = 0; k < Blocksize; ++k)
                    dmem[(ctrl.tag[i][j] << (tagshift-2)) | (i << (setshift-2)) | k] = cachedata[i][j][k];

    if(cycles > 50000)
    {
        /*for(unsigned int i  = 0; i < Sets; ++i)
            for(unsigned int j = 0; j < Associativity; ++j)
                if(ctrl.dirty[i][j] && ctrl.valid[i][j])
                {
                    debug("%d   sync for simul  ", cycles);
                    for(unsigned int k = 0; k < Blocksize; ++k)
                    {
                        ac_int<32, true> ad;
                        ad.set_slc(30, (ac_int<2>)0);
                        ad.set_slc(tagshift-2, ctrl.workAddress.slc<32-tagshift>(tagshift));
                        ad.set_slc(setshift-2, ctrl.currentset);
                        ad.set_slc(0, ctrl.i);
                        dmem[(ctrl.tag[i][j] << (tagshift-2)) | (i << (setshift-2)) | k] = cachedata[i][j][k];
                        debug("%04x     %04x\n", ((ctrl.tag[i][j] << (tagshift-2)) | (i << (setshift-2)) | k).to_int(), ad.to_int());
                    }
                    debug("\n");
                }
        ac_int<32, true> ad;
        ad.set_slc(30, (ac_int<2>)0);
        ad.set_slc(tagshift-2, ctrl.tag[1][3]);
        ad.set_slc(setshift-2, (ac_int<ac::log2_ceil<Sets>::val, false>)3);
        ad.set_slc(0, (ac_int<ac::log2_ceil<Blocksize>::val, false>)0);
        debug("%5d   :   %08x   %08x\n", cycles, ((ctrl.tag[0][2] << (tagshift-2)) | (2 << (setshift-2) | 0)).to_int(), ad.to_int());*/
    }

    /*debug("%6d: ", cycles);
    if(instruction)
    {
        const char* ins;
        switch (instruction & 0xF)
        {
        case 0:
            ins = "nop";
            break;
        case 1:
            ins = "loadimm";
            break;
        case 2:
        case 3:
            ins = "loadmem";
            break;
        case 4:
            ins = "plus";
            break;
        case 5:
            ins = "compare";
            break;
        case 6:
            ins = "branch";
            break;
        case 7:
            ins = "out";
            break;
        default:
            ins = "???";
            break;
        }
        debug("%04x    %8s    %s:%04x(%d)    @%04x(%d)     ",
               iaddress-1, ins, writeenable?"W":"R", writeenable?writevalue:readvalue, datavalid, ldaddress, cacheenable);
        for(int i(0); i<16;++i)
            debug("%08x ", reg[i]);
        debug("\n");
    }
    else
    {
        debug("%04x    %8s    %s:%04x(%d)    @%04x(%d)     \n",
               iaddress-1, "nop", writeenable?"W":"R", writeenable?writevalue:readvalue, datavalid, ldaddress, cacheenable);
    }*/
    return false;
#endif
}
#endif
