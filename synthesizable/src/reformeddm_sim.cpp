/* vim: set ts=4 ai nu: */
#include "elfFile.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <map>
#include <iostream>
#include <bitset>
#include <string.h>
#include "cache.h"
#include "core.h"
#include "portability.h"
#include "mc_scverify.h"

//#include "sds_lib.h"

#ifdef __VIVADO__
#include "DataMemory.h"
#else
#include "DataMemoryCatapult.h"
#endif

using namespace std;

class Simulator
{

private:
    //counters
    map<ac_int<32, false>, ac_int<8, false> > ins_memorymap;
    map<ac_int<32, false>, ac_int<8, false> > data_memorymap;
    ac_int<32, false> nbcycle;
    ac_int<32, false> ins_addr;

    ac_int<32, false> pc;

public:
    ac_int<32, true>* ins_memory;
    ac_int<32, true>* data_memory;

    Simulator()
    {
        ins_memory = (ac_int<32, true> *)malloc(N * sizeof(ac_int<32, true>));
        data_memory = (ac_int<32, true> *)malloc(N * sizeof(ac_int<32, true>));
        for(int i =0; i<N; i++)
        {
            ins_memory[i]=0;
            data_memory[i]=0;
        }
    }

    ~Simulator()
    {
        free(ins_memory);
        free(data_memory);
    }

    void fillMemory()
    {
        //Check whether data memory and instruction memory from program will actually fit in processor.
        //cout << ins_memorymap.size()<<endl;

        if(ins_memorymap.size() / 4 > N)
        {
            printf("Error! Instruction memory size exceeded");
            exit(-1);
        }
        if(data_memorymap.size() / 4 > N)
        {
            printf("Error! Data memory size exceeded");
            exit(-1);
        }

        //fill instruction memory
        for(map<ac_int<32, false>, ac_int<8, false> >::iterator it = ins_memorymap.begin(); it!=ins_memorymap.end(); ++it)
        {
            ins_memory[(it->first/4) % N].set_slc(((it->first)%4)*8,it->second);
            //debug("@%06x    @%06x    %d    %02x\n", it->first, (it->first/4) % N, ((it->first)%4)*8, it->second);
        }

        //fill data memory
        for(map<ac_int<32, false>, ac_int<8, false> >::iterator it = data_memorymap.begin(); it!=data_memorymap.end(); ++it)
        {
            //data_memory.set_byte((it->first/4)%N,it->second,it->first%4);
            data_memory[(it->first%N)/4].set_slc(((it->first%N)%4)*8,it->second);
        }
    }

    void setInstructionMemory(ac_int<32, false> addr, ac_int<8, true> value)
    {
        ins_memorymap[addr] = value;
    }

    void setDataMemory(ac_int<32, false> addr, ac_int<8, true> value)
    {
        if((addr % N )/4 == 0)
        {
            //cout << addr << " " << value << endl;
        }
        data_memorymap[addr] = value;
    }

    ac_int<32, true>* getInstructionMemory()
    {
        return ins_memory;
    }


    ac_int<32, true>* getDataMemory()
    {
        return data_memory;
    }

    void setPC(ac_int<32, false> pc)
    {
        this->pc = pc;
    }

    ac_int<32, false> getPC()
    {
        return pc;
    }

};


CCS_MAIN(int argc, char** argv)
{
    const char* binaryFile;
    if(argc > 1)
        binaryFile = argv[1];
    else
#ifdef __SYNTHESIS__
        binaryFile = "../benchmarks/build/matmul4_4.out";
#else
        binaryFile = "benchmarks/build/matmul4_4.out";
#endif
    fprintf(stderr, "Simulating %s\n", binaryFile);
    std::cout << "Simulating " << binaryFile << std::endl;
    ElfFile elfFile(binaryFile);
    Simulator sim;
    int counter = 0;
    for (unsigned int sectionCounter = 0; sectionCounter<elfFile.sectionTable->size(); sectionCounter++)
    {
        ElfSection *oneSection = elfFile.sectionTable->at(sectionCounter);
        if(oneSection->address != 0 && strncmp(oneSection->getName().c_str(), ".text", 5))
        {
            //If the address is not null we place its content into memory
            unsigned char* sectionContent = oneSection->getSectionCode();
            for (unsigned int byteNumber = 0; byteNumber<oneSection->size; byteNumber++)
            {
                counter++;
                sim.setDataMemory(oneSection->address + byteNumber, sectionContent[byteNumber]);
            }
            coredebug("filling data from %06x to %06x\n", oneSection->address, oneSection->address + oneSection->size -1);
        }

        if(!strncmp(oneSection->getName().c_str(), ".text", 5))
        {
            unsigned char* sectionContent = oneSection->getSectionCode();
            for (unsigned int byteNumber = 0; byteNumber<oneSection->size; byteNumber++)
            {
                sim.setInstructionMemory((oneSection->address + byteNumber) /*& 0x0FFFF*/, sectionContent[byteNumber]);
            }
            coredebug("filling instruction from %06x to %06x\n", oneSection->address, oneSection->address + oneSection->size -1);
        }
    }

    for (int oneSymbol = 0; oneSymbol < elfFile.symbols->size(); oneSymbol++)
    {
        ElfSymbol *symbol = elfFile.symbols->at(oneSymbol);
        const char* name = (const char*) &(elfFile.sectionTable->at(elfFile.indexOfSymbolNameSection)->getSectionCode()[symbol->name]);
        if (strcmp(name, "_start") == 0)
        {
            fprintf(stderr, "%s     @%06x\n", name, symbol->offset);
            sim.setPC(symbol->offset);
        }
    }

    sim.fillMemory();

    /*for(int s(0); s < 2; ++s)
    {
        debug("%s\n", s?"Sign extension":"No sign extension");
        for(int size(0); size < 4; ++size)
        {
            if(size == 2)
                continue;
            debug("Size %d\n", size);
            for(ac_int<32, false> i(0); i < 256; ++i)
            {
                ac_int<32, false> r = (i << 24) + (i << 16) + (i <<8) + i;
                formatread(0, size, s, r);
                debug("%08x ", r.to_int());
            }
            debug("\n");
        }
        for(int ad(0); ad < 4; ++ad)
        {
            debug("Address %d\n", ad);
            for(ac_int<32, false> i(0); i < 256; ++i)
            {
                ac_int<32, false> r = (i << 24) + (i << 16) + (i <<8) + i;
                formatread(ad, 0, s, r);
                debug("%08x ", r.to_int());
            }
            debug("\n");
        }
    }

    return 0;*/


    unsigned int* dm = new unsigned int[N];
    unsigned int* im = new unsigned int[N];
    for(int i = 0; i < N; i++)
    {
        dm[i] = sim.getDataMemory()[i];
        im[i] = sim.getInstructionMemory()[i];
    }

    coredebug("instruction memory :\n");
    for(int i = 0; i < N; i++)
    {
        if(im[i])
            printf("%06x : %08x (%d)\n", 4*i, im[i], im[i]);
    }
    coredebug("data memory :\n");
    for(int i = 0; i < N; i++)
    {
        for(int j(0); j < 4; ++j)
        {
            if(dm[i] & (0xFF << (8*j)))
            {
                printf("%06x : %02x (%d)\n", 4*i+j, (dm[i] & (0xFF << (8*j))) >> (8*j), (dm[i] & (0xFF << (8*j))) >> (8*j));
            }
        }
    }
    coredebug("end of preambule\n");

    uint64_t cycles = 1;
    bool exit = false;
    while(!exit)
    {
        CCS_DESIGN(doStep(sim.getPC(), im, dm, exit
                  #ifndef __SYNTHESIS__
                          , cycles++
                  #endif
                          ));
        if(cycles >
        #ifndef nocache
                (uint64_t)1e7
        #else
                (uint64_t)2e6
        #endif
                )
            break;
    }
    coredebug("Successfully executed all instructions in %d cycles\n", cycles);

    std::cout << "memory :" <<std::endl;
    for(int i = 0; i < N; i++)
    {
        for(int j(0); j < 4; ++j)
        {
            if(dm[i] & (0xFF << (8*j)))
            {
                printf("%06x : %02x (%d)\n", 4*i+j, (dm[i] & (0xFF << (8*j))) >> (8*j), (dm[i] & (0xFF << (8*j))) >> (8*j));
            }
        }
    }

    CCS_RETURN(0);
}
