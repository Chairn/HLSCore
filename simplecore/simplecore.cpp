#include "simplecore.h"

#ifndef __SYNTHESIS__
#include <cstdio>
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


void cache(int dmem[N],  int data[32], int address, bool cacheenable, int& read, bool& datavalid)
{
    static int tag = 0;
    static int wantedAddress = 0;
    static bool valid = 0;
    static bool fetch = 0;
    static int i;
    int tmp;

    if(fetch)
    {
#ifndef __SYNTHESIS__
        //printf("fetching %04x\n", tag|i);
#endif
        tmp = dmem[tag | i];
        data[i] = tmp;
        if(i == wantedAddress & 0x1F)
        {
            read = tmp;
            datavalid = true;
        }
        else
        {
            datavalid = false;
        }

        if(++i == 32)
        {
            fetch = false;
            valid = true;
        }
    }
    else if(cacheenable)
    {
#ifndef __SYNTHESIS__
        printf("cacheenable     ");
#endif
        if((tag == (address & 0xFFFFFFE0)) && valid)
        {
            read = data[address & 0x1F];
            datavalid = true;
#ifndef __SYNTHESIS__
        printf("valid data\n");
#endif
        }
        else    // not found or invalid
        {
#ifndef __SYNTHESIS__
        printf("data not found or invalid   ");
#endif
            tag = address & 0xFFFFFFE0;
            wantedAddress = address;
            fetch = true;
            valid = false;
            tmp = dmem[tag];
            data[0] = tmp;
            i = 1;
            if((wantedAddress & 0x1F) == 0)
            {
                read = tmp;
                datavalid = true;
#ifndef __SYNTHESIS__
        printf("address low bytes are null\n");
#endif
            }
            else
            {
                datavalid = false;
#ifndef __SYNTHESIS__
        printf("starting fetching\n");
#endif
            }
        }
    }
    else
    {
        datavalid = false;
        /*datavalid = true;
        read = data[address & 0x1F];*/
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
        tomem.result = dctoex.dataa;
        break;
    }
    memtowb = tomem;
}

void Mem(int reg[16], MemtoWB memtowb,      // from core
         int& ldaddress, bool& read,        // to cache
         int datum, bool valid,             // from cache
         bool& memlock, int& res)
{
    if(memlock)
    {
        if(valid)
        {
            reg[memtowb.dest] = datum;
            memlock = false;
        }
    }
    else
    {
        if(memtowb.ldmem)
        {
            ldaddress = memtowb.result;
            read = true;
            memlock = true;
        }
        else if(memtowb.out)
        {
            res = memtowb.result;
            read = false;
        }
        else if(memtowb.dest)
        {
            reg[memtowb.dest] = memtowb.result;
            read = false;
        }
    }
}

void simplecachedcore(int imem[N], int dmem[N], int& res)
{
    static int iaddress = 0;
    static int reg[16] = {0};
    static int cachedata[32] = {0};
    static MemtoWB memtowb = {0,0,0,0};
    static DCtoEx dctoex = {0,0,0,0};
    static int ldaddress = 0;
    static int readvalue = 0;
    static int instruction = 0;
    static int branchjumplock = 0;
    static bool memlock = false;
    static bool cacheenable = false;
    static bool datavalid = false;

    cache(dmem, cachedata, ldaddress, cacheenable, readvalue, datavalid);
    Mem(reg, memtowb, ldaddress, cacheenable, readvalue, datavalid, memlock, res);
    if(!memlock)
    {
        Ex(dctoex, memtowb, iaddress);
        Dc(instruction, dctoex, branchjumplock, reg);
        Ft(imem, iaddress, instruction, branchjumplock);
    }
#ifndef __SYNTHESIS__
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
    printf("%04x    % 8s    %04x(%d)    %04x(%d)    ",
           iaddress-1, ins, ldaddress, cacheenable, readvalue, datavalid);
    for(int i(0); i<16;++i)
        printf("%08x ", reg[i]);
    printf("\n");
#endif
}
