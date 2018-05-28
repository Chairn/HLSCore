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
        if(oneSection->address != 0 && oneSection->getName().compare(".text"))
        {
            //If the address is not null we place its content into memory
            unsigned char* sectionContent = oneSection->getSectionCode();
            for (unsigned int byteNumber = 0; byteNumber<oneSection->size; byteNumber++)
            {
                counter++;
                sim.setDataMemory(oneSection->address + byteNumber, sectionContent[byteNumber]);
            }
            debug("filling data from %06x to %06x\n", oneSection->address, oneSection->address + oneSection->size -1);
        }

        if (!oneSection->getName().compare(".text"))
        {
            unsigned char* sectionContent = oneSection->getSectionCode();
            for (unsigned int byteNumber = 0; byteNumber<oneSection->size; byteNumber++)
            {
                sim.setInstructionMemory((oneSection->address + byteNumber) /*& 0x0FFFF*/, sectionContent[byteNumber]);
            }
            debug("filling instruction from %06x to %06x\n", oneSection->address, oneSection->address + oneSection->size -1);
        }
    }

    for (int oneSymbol = 0; oneSymbol < elfFile.symbols->size(); oneSymbol++)
    {
        ElfSymbol *symbol = elfFile.symbols->at(oneSymbol);
        const char* name = (const char*) &(elfFile.sectionTable->at(elfFile.indexOfSymbolNameSection)->getSectionCode()[symbol->name]);
        if (strcmp(name, "_start") == 0)
        {
            fprintf(stderr, "%s\n", name);
            sim.setPC(symbol->offset);
        }
    }


    sim.fillMemory();

    unsigned int* dm = new unsigned int[N];
    unsigned int* im = new unsigned int[N];
    for(int i = 0; i<N; i++)
    {
        dm[i] = sim.getDataMemory()[i];
        im[i] = sim.getInstructionMemory()[i];
    }

    uint64_t cycles = 1;
    bool exit = false;
    while(!exit)
    {
        CCS_DESIGN(doStep(sim.getPC(), im, dm, exit
                  #ifndef __SYNTHESIS__
                          , cycles++
                  #endif
                          ));
        if(cycles > (uint64_t)1e7)
            //fprintf(stderr, "%lld\n", cycles);
            break;//return 1;
    }
    debug("Successfully executed all instructions in %d cycles\n", cycles);

    std::cout << "memory :" <<std::endl;
    for(int i = 0; i<N; i++)
    {
        if(dm[i])
            printf("%4x : %08x (%d)\n", i, dm[i], dm[i]);
    }

    CCS_RETURN(0);
}
