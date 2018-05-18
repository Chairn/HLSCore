#include "simplecore.h"

#ifndef __SYNTHESIS__
#include <cstdio>

#define debug(...)   printf(__VA_ARGS__)
#define simul(...)   __VA_ARGS__
#else
#define debug(...)
#define simul(...)
#undef assert
#define assert(a)
#endif

struct FtoDC
{
    int pc;
    int instruction; //Instruction to execute
};

struct DCtoEx
{
    int opcode;
    int dataa;
    int datab;
    int dest;
};

struct ExtoMem
{
    int pc;
    int result; //Result of the EX stage
    int datad;
    int datac; //Data to be stored in memory (if needed)
    int dest; //Register to be written at WB stage
    int WBena; //Is a WB is needed ?
    int opCode; //OpCode of the operation
    int memValue; //Second data, from register file or immediate value
    int rs2;
    int funct3;
    int sys_status;
};

struct MemtoWB
{
    int result; //Result to be written back
    int dest; //Register to be written at WB stage
    ac_int<2, false> datasize;
    bool ldmem;
    bool out;
};

enum {
    NOP,
    LOADIMM,
    LOADMEM,
    PLUS,
    COMPARE,
    BRANCH,
    OUT
};

enum {
    MemtoCache = 0,
    CachetoMem = 1
};

enum {
    Idle         = 0,
    StoreControl = 1,
    StoreData    = 2,
    WriteBack    = 3,
    Fetch        = 4,
    States       = 5
};

struct SetControl
{
    int data[Associativity];
    ac_int<32-tagshift, false> tag[Associativity];
    bool dirty[Associativity];
    bool valid[Associativity];
#if Associativity == 1
    //ac_int<1, false> policy;
#else
  #if Policy == FIFO
    ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #elif Policy == LRU
    ac_int<Associativity * (Associativity+1) / 2, false> policy;
  #elif Policy == RANDOM
    //ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #else   // None
    //ac_int<1, false> policy;
  #endif
#endif
};

struct CacheControl
{
    ac_int<32-tagshift, false> tag[Sets][Associativity];
    ac_int<32, false> workAddress;
    bool dirty[Sets][Associativity];
    bool valid[Sets][Associativity];
    ac_int<ac::log2_ceil<States>::val, false> state;
    ac_int<ac::log2_ceil<Blocksize>::val, false> i;
    ac_int<32, false> valuetowrite;
    ac_int<ac::log2_ceil<Sets>::val, false> currentset;
#if Associativity == 1
    ac_int<1, false> currentway;
    //ac_int<1, false> policy[Sets];
#else
    ac_int<ac::log2_ceil<Associativity>::val, false> currentway;
  #if Policy == FIFO
    ac_int<ac::log2_ceil<Associativity>::val, false> policy[Sets];
  #elif Policy == LRU
    ac_int<Associativity * (Associativity-1) / 2, false> policy[Sets];
  #elif Policy == RANDOM
    ac_int<32, false> policy;   //32 bits for the whole cache
  #else   // None alias direct mapped
    //ac_int<1, false> policy[Sets];
  #endif
#endif

    SetControl setctrl;
};

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
           , int cycles
#endif
           )
{
    ac_int<32, false> bytead;
    switch(ctrl.state)
    {
    case Idle:
        if(cacheenable)
        {
            debug("%5d   cacheenable     ", cycles);
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
                debug("%2d %2d    ", ctrl.currentset.to_int(), ctrl.currentway.to_int());
                debug("valid data   ");
                if(writeenable)
                {
                    ctrl.valuetowrite = ctrl.setctrl.data[ctrl.currentway];//data[ctrl.currentset][ctrl.currentway][(address & (ac_int<32, true>)blockmask) >> 2];
                    format(address, datasize, ctrl.valuetowrite, writevalue);
                    ctrl.workAddress = address;
                    ctrl.setctrl.dirty[ctrl.currentway] = true;
                    simul(
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
                    );

                    ctrl.state = StoreData;
                }
                else
                {                                                //address.slc<ac::log2_ceil<Blocksize>::val>(2) // doesnt work, why?
                    ac_int<32, false> r = ctrl.setctrl.data[ctrl.currentway];//data[ctrl.currentset][ctrl.currentway][(address & (ac_int<32, true>)blockmask) >> 2];
                    //debug("%08x (%08x)", r.to_int(), data[ctrl.currentset][ctrl.currentway][ctrl.i]);
                    format(address, datasize, r);

                    read = r;
                    simul(
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
                    );

                    ctrl.state = StoreControl;
                }
                datavalid = true;

                debug("@%04x    ", address.to_int());
                debug("(@%04x)\n", address.to_int() >> 2);

                update_policy(ctrl);
            }
            else    // not found or invalid
            {
                select(ctrl);
                debug("data not found or invalid   ");
                if(ctrl.setctrl.dirty[ctrl.currentway] && ctrl.setctrl.valid[ctrl.currentway])
                {
                    ctrl.state = WriteBack;
                    ctrl.i = 0;
                    ctrl.workAddress = ctrl.setctrl.tag[ctrl.currentway];
                    ctrl.valuetowrite = ctrl.setctrl.data[ctrl.currentway];
                    datavalid = false;
                    debug("starting writeback \n");
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
                    debug("starting fetching for %s from %04x to %04x (%04x)\n", writeenable?"W":"R", wordad.to_int() << 2, (int)(wordad.to_int()+Blocksize) << 2, address.to_int());
                    ctrl.valuetowrite = dmem[wordad];
                    // critical word first
                    if(writeenable)
                    {
                        format(address, datasize, ctrl.valuetowrite, writevalue);
                        ctrl.setctrl.dirty[ctrl.currentway] = true;
                        debug("%5d  Storing data %08x @%04x\n", cycles, ctrl.valuetowrite.to_int(), wordad.to_int());
                    }
                    else
                    {
                        ac_int<32, false> r = ctrl.valuetowrite;
                        format(address, datasize, r);
                        read = r;
                    }

                    datavalid = true;
                }
            }
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
    case WriteBack:
        //ac_int<32, false> bytead;
        setTag(bytead, ctrl.setctrl.tag[ctrl.currentway]);
        setSet(bytead, ctrl.currentset);
        setOffset(bytead, ctrl.i);
        dmem[bytead >> 2] = ctrl.valuetowrite;

        simul(
        for(unsigned int i(0); i < sizeof(int); ++i)
        {
            debug("writing back %02x to %04x (%04x)\n", (dmem[bytead >> 2] >> (i*8))&0xFF, (bytead.to_int() << 2) + i, bytead.to_int());
        });

        if(++ctrl.i)
            ctrl.valuetowrite = data[ctrl.currentset][ctrl.i][ctrl.currentway];
        else
        {
            ctrl.state = StoreControl;
            ctrl.setctrl.dirty[ctrl.currentway] = false;
            debug("end of writeback\n");
        }

        datavalid = false;
        break;
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
                debug("fetching %02x from %04x (%04x)\n", (dmem[bytead >> 2] >> (i*8))&0xFF, (bytead.to_int()) + i, bytead.to_int() >> 2);
            });

            ctrl.valuetowrite = dmem[bytead >> 2];
        }
        else
        {
            ctrl.state = StoreControl;
            ctrl.setctrl.valid[ctrl.currentway] = true;
            debug("end of fetch to %d %d\n", ctrl.currentset.to_int(), ctrl.currentway.to_int());
        }
        break;
    default:
        datavalid = false;
        ctrl.state = Idle;
        break;
    }
}

void Ft(int imem[N], unsigned int& pc, int& instruction, int lock)
{
    int tmp;
    if(lock)
    {
        tmp = 0;
    }
    else
        tmp = imem[pc++];

    instruction = tmp;
}

void Dc(int instruction, DCtoEx& dte, int& lock, unsigned int reg[16])
{
    DCtoEx dctoex = {0,0,0,0};

    if(lock)
    {
        dctoex.opcode = NOP;
        --lock;
    }
    else
    {
        switch(instruction & 0x0F)
        {
        case 0:
            dctoex.opcode = NOP;
            break;
        case 1:
            dctoex.opcode = LOADIMM;
            dctoex.dest = (instruction & 0xF0) >> 4;
            dctoex.dataa = instruction >> 8;
            break;
        case 2:
            dctoex.opcode = LOADMEM;
            dctoex.dest = (instruction & 0xF0) >> 4;
            dctoex.dataa = (instruction & 0xF00) >> 8;
            dctoex.datab = (instruction & 0x3000) >> 12;
            break;
        case 3:
            dctoex.opcode = LOADMEM;
            dctoex.dest = (instruction & 0xF0) >> 4;
            dctoex.dataa = reg[(instruction & 0xF00) >> 8];
            dctoex.datab = (instruction & 0x3000) >> 12;
            break;
        case 4:
            dctoex.opcode = PLUS;
            dctoex.dest = (instruction & 0xF0) >> 4;
            dctoex.dataa = reg[(instruction & 0xF00) >> 8];
            dctoex.datab = reg[(instruction & 0xF000) >> 12];
            break;
        case 5:
            dctoex.opcode = COMPARE;
            dctoex.dataa = reg[(instruction & 0xF00) >> 8];
            dctoex.datab = reg[(instruction & 0xF000) >> 12];
            dctoex.dest = 0x0F;
            break;
        case 6:
            dctoex.opcode = BRANCH;
            dctoex.dest = instruction >> 8;
            dctoex.dataa = reg[0x0F];
            lock = 2;
            break;
        case 7:
            dctoex.opcode = OUT;
            dctoex.dataa = reg[(instruction & 0xF00) >> 8];
            dctoex.dest = instruction >> 16;
            dctoex.datab = (instruction & 0x3000) >> 12;
            break;
        }
    }
    dte = dctoex;
}

void Ex(DCtoEx dctoex, MemtoWB& memtowb, unsigned int& pc)
{
    MemtoWB tomem = {0,0,0,0};
    switch(dctoex.opcode)
    {
    case NOP:
        break;
    case LOADIMM:
        tomem.dest = dctoex.dest;
        tomem.result = dctoex.dataa;
        break;
    case LOADMEM:
        tomem.dest = dctoex.dest;
        tomem.result = dctoex.dataa;
        tomem.datasize = dctoex.datab;
        tomem.ldmem = true;
        break;
    case PLUS:
        tomem.dest = dctoex.dest;
        tomem.result = dctoex.dataa + dctoex.datab;
        break;
    case COMPARE:
        tomem.dest = dctoex.dest;
        tomem.result = dctoex.dataa < dctoex.datab;
        break;
    case BRANCH:
        if(dctoex.dataa)
            pc = dctoex.dest;
        break;
    case OUT:
        tomem.out = true;
        tomem.dest = dctoex.dest;
        tomem.result = dctoex.dataa;
        tomem.datasize = dctoex.datab;
        break;
    }
    memtowb = tomem;
}

void Mem(unsigned int reg[16], MemtoWB memtowb,      // from core
         unsigned int& address, ac_int<2,false>& datasize, bool& cacheenable, bool& writeenable, unsigned int& writevalue,       // to cache
         unsigned int datum, bool valid,             // from cache
         bool& memlock, int& res)
{
    if(memlock)
    {
        if(valid)
        {
            if(!writeenable)
                reg[memtowb.dest] = datum;
            memlock = false;
            writeenable = false;
            cacheenable = false;
        }
    }
    else
    {
        if(memtowb.ldmem)
        {
            address = memtowb.result;
            datasize = memtowb.datasize;
            cacheenable = true;
            memlock = true;
            writeenable = false;
        }
        else if(memtowb.out)
        {
            res = memtowb.result;
            address = memtowb.dest;
            writevalue = memtowb.result;
            datasize = memtowb.datasize;
            cacheenable = true;
            memlock = true;
            writeenable = true;
        }
        else if(memtowb.dest)
        {
            reg[memtowb.dest] = memtowb.result;
            cacheenable = false;
            memlock = false;
            writeenable = false;
        }
    }
}

#ifndef __SYNTHESIS__
bool
#else
void
#endif
simplecachedcore(int imem[N], unsigned int dmem[N], int& res)
{
    static unsigned int iaddress = 0;
#ifndef __SYNTHESIS__
    static int cycles = 0;
    if(iaddress > 8192)
    {
        debug("Out of instruction memory\n");
        return true;
    }
#endif

    static unsigned int reg[16] = {0};
    static unsigned int cachedata[Sets][Blocksize][Associativity] = {0};
    static bool dummy = ac::init_array<AC_VAL_DC>((unsigned int*)cachedata, Sets*Associativity*Blocksize);
    (void)dummy;
    static MemtoWB memtowb = {0};
    static DCtoEx dctoex = {0};
    static unsigned int ldaddress = 0;
    static int readvalue = 0;
    static ac_int<2, false> datasize;
    static unsigned int writevalue = 0;
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
                    dmem[(ctrl.tag[i][j] << (tagshift-2)) | (i << (setshift-2)) | k] = cachedata[i][k][j];

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
