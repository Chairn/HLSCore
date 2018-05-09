#include "simplecore.h"

#ifndef __SYNTHESIS__
#include <cstdio>

#define debug(...)   printf(__VA_ARGS__)
#else
#define debug(...)
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

struct SetControl
{
    ac_int<32-tagshift, false> tag[Associativity];
    bool dirty[Associativity];
    bool valid[Associativity];
#if Associativity == 1
    //ac_int<1, false> policy;
#else
  #if Policy == FIFO
    ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #elif Policy == LRU
    ac_int<Associativity * (Associativity+1) / 2 + 1, false> policy;
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
    ac_int<32, true> workAddress;
    bool dirty[Sets][Associativity];
    bool valid[Sets][Associativity];
    bool sens;
    bool enable;
    bool storecontrol;
    ac_int<ac::log2_ceil<Blocksize>::val, false> i;
    int valuetowrite;
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

bool find(CacheControl& ctrl, ac_int<32, true> address)
{
    bool found = false;
    bool valid = false;

    #pragma hls_unroll yes
    findloop:for(int i = 0; i < Associativity; ++i)
    {
        if((ctrl.setctrl.tag[i] == (address.slc<32-tagshift>(tagshift))) && ctrl.setctrl.valid[i])
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

void cache(CacheControl& ctrl, int dmem[N], int data[Sets][Associativity][Blocksize], ac_int<32, true> address, bool cacheenable, bool writeenable, int writevalue, int& read, bool& datavalid
#ifndef __SYNTHESIS__
           , int cycles
#endif
           )
{
    if(ctrl.storecontrol)
    {
        update_policy(ctrl);    // more time here?

        #pragma hls_unroll yes
        storeset:for(int i = 0; i < Associativity; ++i)
        {
            ctrl.tag[ctrl.currentset][i] = ctrl.setctrl.tag[i];
            ctrl.dirty[ctrl.currentset][i] = ctrl.setctrl.dirty[i];
            ctrl.valid[ctrl.currentset][i] = ctrl.setctrl.valid[i];
        #if Policy == FIFO || Policy == LRU
            ctrl.policy[ctrl.currentset] = ctrl.setctrl.policy;
        #endif
        }
        ctrl.storecontrol = false;
    }
    else if(ctrl.enable)
    {
        if(ctrl.sens)   //cachetomem = writeback
        {
            ac_int<32, true> ad;
            ad.set_slc(tagshift, ctrl.workAddress.slc<32-tagshift>(tagshift));
            ad.set_slc(setshift, ctrl.currentset);
            ad.set_slc(0, ctrl.i);
            dmem[ad] = ctrl.valuetowrite;

            debug("Writing back %04x to %04x \n", ctrl.valuetowrite, ad.to_int());

            if(++ctrl.i)
                ctrl.valuetowrite = data[ctrl.currentset][ctrl.currentway][ctrl.i];
            else
            {
                ctrl.enable = false;
                ctrl.setctrl.dirty[ctrl.currentway] = false;
                ctrl.storecontrol = true;
                debug("end of writeback\n");
            }

            datavalid = false;
        }
        else            //memtocache = fetch
        {
            if(ctrl.i == (ctrl.workAddress & blockmask))
            {
                if(writeenable)
                {
                    data[ctrl.currentset][ctrl.currentway][ctrl.i] = writevalue;
                    ctrl.setctrl.dirty[ctrl.currentway] = true;
                }
                else
                {
                    data[ctrl.currentset][ctrl.currentway][ctrl.i] = ctrl.valuetowrite;
                    read = ctrl.valuetowrite;
                    ctrl.setctrl.dirty[ctrl.currentway] = false;
                }
                datavalid = true;
            }
            else
            {
                data[ctrl.currentset][ctrl.currentway][ctrl.i] = ctrl.valuetowrite;
                datavalid = false;
            }

            if(++ctrl.i)
            {
                ac_int<32, true> ad;
                ad.set_slc(tagshift, ctrl.workAddress.slc<32-tagshift>(tagshift));
                ad.set_slc(setshift, ctrl.currentset);
                ad.set_slc(0, ctrl.i);
                ctrl.valuetowrite = dmem[ad];
                debug("fetching %04x from %04x \n", ctrl.valuetowrite, ad.to_int());
            }
            else
            {
                ctrl.enable = false;
                ctrl.setctrl.valid[ctrl.currentway] = true;
                ctrl.storecontrol = true;
                debug("end of fetch\n");
            }
        }
    }
    else if(cacheenable)
    {
        debug("%5d   cacheenable     ", cycles);
        ctrl.currentset = (address & setmask) >> setshift;
        #pragma hls_unroll yes
        loadset:for(int i = 0; i < Associativity; ++i)
        {
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
                data[ctrl.currentset][ctrl.currentway][address & blockmask] = writevalue;
                ctrl.setctrl.dirty[ctrl.currentway] = true;
                debug("W:%04x   ", writevalue);
            }
            else
            {
                read = data[ctrl.currentset][ctrl.currentway][address & blockmask];
                debug("R:%04x   ", read);
            }
            datavalid = true;
            debug("@%04x\n", address.to_int());

            update_policy(ctrl);
            ctrl.storecontrol = true;
        }
        else    // not found or invalid
        {
            select(ctrl);
            debug("data not found or invalid   ");
            if(ctrl.setctrl.dirty[ctrl.currentway] && ctrl.setctrl.valid[ctrl.currentway])
            {
                ctrl.enable = true;
                ctrl.sens = CachetoMem;
                ctrl.i = 0;
                ctrl.workAddress = ctrl.setctrl.tag[ctrl.currentway];
                ctrl.valuetowrite = data[ctrl.currentset][ctrl.currentway][0];
                debug("starting writeback \n");
            }
            else
            {
                ctrl.setctrl.tag[ctrl.currentway].set_slc(0, address.slc<32-tagshift>(tagshift));
                ctrl.workAddress = address;
                ctrl.enable = true;
                ctrl.sens = MemtoCache;
                ctrl.setctrl.valid[ctrl.currentway] = false;
                ctrl.i = 0;
                ac_int<32, true> ad;
                ad.set_slc(tagshift, ctrl.setctrl.tag[ctrl.currentway]);
                ad.set_slc(setshift, ctrl.currentset);
                ad.set_slc(0, (ac_int<setshift, true>)0);
                ctrl.valuetowrite = dmem[ad];
                debug("starting fetching for %s from %04x to %04x (%04x)\n", writeenable?"W":"R", ad.to_int(), ad.to_int()+Blocksize, address.to_int());
            }
            datavalid = false;
        }
    }
    else
    {
        datavalid = false;
    }
}

void Ft(int imem[N], int& pc, int& instruction, int lock)
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

void Dc(int instruction, DCtoEx& dte, int& lock, int reg[16])
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
            dctoex.dataa = instruction >> 8;
            break;
        case 3:
            dctoex.opcode = LOADMEM;
            dctoex.dest = (instruction & 0xF0) >> 4;
            dctoex.dataa = reg[(instruction & 0xF00) >> 8];
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
            break;
        }
    }
    dte = dctoex;
}

void Ex(DCtoEx dctoex, MemtoWB& memtowb, int& pc)
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
        break;
    }
    memtowb = tomem;
}

void Mem(int reg[16], MemtoWB memtowb,      // from core
         int& address, bool& cacheenable, bool& writeenable, int& writevalue,       // to cache
         int datum, bool valid,             // from cache
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
            cacheenable = true;
            memlock = true;
            writeenable = false;
        }
        else if(memtowb.out)
        {
            res = memtowb.result;
            address = memtowb.dest;
            writevalue = memtowb.result;
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

void simplecachedcore(int imem[N], int dmem[N], int& res)
{
#ifndef __SYNTHESIS__
    static int cycles = 0;
#endif
    static int iaddress = 0;
    static int reg[16] = {0};
    static int cachedata[Sets][Associativity][Blocksize] = {0};
    static bool dummy = ac::init_array<AC_VAL_DC>((int*)cachedata, Sets*Associativity*Blocksize);
    (void)dummy;
    static MemtoWB memtowb = {0};
    static DCtoEx dctoex = {0};
    static int ldaddress = 0;
    static int readvalue = 0;
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
    static bool fifinit = ac::init_array<AC_VAL_DC>((ac_int<ac::log2_ceil<Associativity>::val, false>*)ctrl.policy, Sets);
    (void)fifinit;
#elif Policy == LRU
#elif Policy == RANDOM
    static bool rndinit = false;
    if(!rndinit)
    {
        rndinit = true;
        ctrl.policy = 0xF2D4B698;
    }
#endif

    cache(ctrl, dmem, cachedata, ldaddress, cacheenable, writeenable, writevalue, readvalue, datavalid
#ifndef __SYNTHESIS__
          , cycles++
#endif
          );
    Mem(reg, memtowb, ldaddress, cacheenable, writeenable, writevalue, readvalue, datavalid, memlock, res);
    if(!memlock)
    {
        Ex(dctoex, memtowb, iaddress);
        Dc(instruction, dctoex, branchjumplock, reg);
        Ft(imem, iaddress, instruction, branchjumplock);
    }
#ifndef __SYNTHESIS__
    //cache write back for simulation
    for(int i  = 0; i < Sets; ++i)
        for(int j = 0; j < Associativity; ++j)
            if(ctrl.dirty[i][j] && ctrl.valid[i][j])
                for(int k = 0; k < Blocksize; ++k)
                    dmem[(ctrl.tag[i][j] << tagshift) | (i << setshift) | k] = cachedata[i][j][k];

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
#endif
}
