#include <ap_int.h>
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

struct CacheControl
{
    int tag;
    int wantedAddress;
    bool dirty;
    bool valid;
    bool sens;
    bool enable;
    ap_uint<5> i;
    int valuetowrite;
};

void cache(CacheControl& ctrl, int dmem[N], int data[32], int address, bool cacheenable, bool writeenable, int writevalue, int& read, bool& datavalid)
{
    if(ctrl.enable)
    {
        if(ctrl.sens)   //cachetomem = writeback
        {
            dmem[ctrl.tag | ctrl.i] = ctrl.valuetowrite;
            if(++ctrl.i)
                ctrl.valuetowrite = data[ctrl.i];
            else
            {
                ctrl.enable = false;
                ctrl.dirty = false;
                debug("end of writeback\n");
            }

            datavalid = false;
        }
        else            //memtocache = fetch
        {
            if(ctrl.i == (ctrl.wantedAddress & 0x1F))
            {
                if(writeenable)
                {
                    data[ctrl.i] = writevalue;
                    ctrl.dirty = true;
                }
                else
                {
                    data[ctrl.i] = ctrl.valuetowrite;
                    read = ctrl.valuetowrite;
                    ctrl.dirty = false;
                }
                datavalid = true;
            }
            else
            {
                data[ctrl.i] = ctrl.valuetowrite;
                datavalid = false;
            }

            if(++ctrl.i)
                ctrl.valuetowrite = dmem[ctrl.tag | ctrl.i];
            else
            {
                ctrl.enable = false;
                ctrl.dirty = false;
                ctrl.valid = true;
                debug("end of fetch\n");
            }
        }
    }
    else if(cacheenable)
    {
        debug("cacheenable     ");
        if(((unsigned)ctrl.tag == (address & 0xFFFFFFE0)) && ctrl.valid)
        {
            debug("valid data   ");
            if(writeenable)
            {
                data[address & 0x1F] = writevalue;
                ctrl.dirty = true;
                debug("W:%04x\n", writevalue);
            }
            else
            {
                read = data[address & 0x1F];
                debug("R:%04x\n", read);
            }
            datavalid = true;
        }
        else    // not found or invalid
        {
            debug("data not found or invalid   ");
            if(ctrl.dirty)
            {
                ctrl.enable = true;
                ctrl.sens = CachetoMem;
                ctrl.i = 0;
                ctrl.valuetowrite = data[0];
                debug("starting writeback \n");
            }
            else
            {
                ctrl.tag = address & 0xFFFFFFE0;
                ctrl.wantedAddress = address;
                ctrl.enable = true;
                ctrl.sens = MemtoCache;
                ctrl.valid = false;
                ctrl.i = 0;
                ctrl.valuetowrite = dmem[ctrl.tag];
                debug("starting fetching\n");
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
    static int iaddress = 0;
    static int reg[16] = {0};
    static int cachedata[32] = {0};
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

    cache(ctrl, dmem, cachedata, ldaddress, cacheenable, writeenable, writevalue, readvalue, datavalid);
    Mem(reg, memtowb, ldaddress, cacheenable, writeenable, writevalue, readvalue, datavalid, memlock, res);
    if(!memlock)
    {
        Ex(dctoex, memtowb, iaddress);
        Dc(instruction, dctoex, branchjumplock, reg);
        Ft(imem, iaddress, instruction, branchjumplock);
    }
#ifndef __SYNTHESIS__
    //cache write back for simulation
    if(ctrl.dirty)
        for(int i  = 0; i < 32; ++i)
            dmem[ctrl.tag | i] = cachedata[i];

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
        debug("%04x    %8s    %04x(%d)    R:%04x W:%04x(%d)    ",
               iaddress-1, ins, ldaddress, cacheenable, readvalue, writevalue, datavalid);
        for(int i(0); i<16;++i)
            debug("%08x ", reg[i]);
        debug("\n");
    }
    else
    {
        debug("%04x    %8s    %04x(%d)    R:%04x W:%04x(%d)\n",
               iaddress-1, "nop", ldaddress, cacheenable, readvalue, writevalue, datavalid);
    }
#endif
}
