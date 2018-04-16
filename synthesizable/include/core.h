#ifndef CORE_H
#define CORE_H

#include "portability.h"

struct FtoDC
{
    ac_int<32, false> pc;
    ac_int<32, false> instruction; //Instruction to execute
};

struct DCtoEx
{
    ac_int<32, false> pc;
    ac_int<32, true> dataa; //First data from register file
    ac_int<32, true> datab; //Second data, from register file or immediate value
    ac_int<32, true> datac;
    ac_int<32, true> datad; //Third data used only for store instruction and corresponding to rb
    ac_int<32, true> datae;
    ac_int<5, false> dest; //Register to be written
    ac_int<7, false> opCode;//OpCode of the instruction
    ac_int<32, true> memValue; //Second data, from register file or immediate value
    ac_int<7, false> funct3 ;
    ac_int<7, false> funct7;
    ac_int<7, false> funct7_smaller ;
    ac_int<6, false> shamt;
    ac_int<5, false> rs1;
    ac_int<5, false> rs2;
};

struct ExtoMem
{
    ac_int<32, false> pc;
    ac_int<32, true> result; //Result of the EX stage
    ac_int<32, true> datad;
    ac_int<32, true> datac; //Data to be stored in memory (if needed)
    ac_int<5, false> dest; //Register to be written at WB stage
    ac_int<1, false> WBena; //Is a WB is needed ?
    ac_int<7, false> opCode; //OpCode of the operation
    ac_int<32, true> memValue; //Second data, from register file or immediate value
    ac_int<5, false> rs2;
    ac_int<7, false> funct3;
    ac_int<2, false> sys_status;
};

struct MemtoWB
{
    ac_int<32, true> result; //Result to be written back
    ac_int<5, false> dest; //Register to be written at WB stage
    ac_int<1, false> WBena; //Is a WB is needed ?
    ac_int<7, false> opCode;
    ac_int<2, false> sys_status;
};


void doStep(ac_int<32, false> pc, ac_int<32, true> ins_memory[8192], ac_int<32, true> dm[8192], bool &exit
#ifndef __SYNTHESIS__
, int &cycles
#endif
);


#endif  // CORE_H

