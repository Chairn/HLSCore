#ifndef __CORE_H__
#define __CORE_H__

#include "portability.h"
#include "cache.h"

#ifdef __VIVADO__
#include "DataMemory.h"
#define DO_MEM_PARAMETER DataMemory* data_memory
#define MEM_SET(memory,address,value,op) memory->set(address,value,op)
#define MEM_GET(memory,address,op,sign) memory->get(address,op,sign)
#endif

#ifdef __CATAPULT__
#define DO_MEM_PARAMETER CORE_INT(32) data_memory[DRAM_SIZE]
#define MEM_SET(memory,address,value,op) memorySet(memory,address,value,op)
#define MEM_GET(memory,address,op,sign) memoryGet(memory,address,op,sign)
#endif

class Core
{
public:
    Core();

    HLS_DESIGN(interface)
    void doStep(CORE_UINT(32) pc, CORE_UINT(32) nbcycle, CORE_INT(32) ins_memory[8192],CORE_INT(32) dm[8192], CORE_INT(32) dm_out[8192]);

private:
    CORE_INT(32) REG[32]; // Register file
    CORE_UINT(2) sys_status;


    struct FtoDC
    {
        CORE_UINT(32) pc;
        CORE_UINT(32) instruction; //Instruction to execute
    } ftoDC;

    struct DCtoEx
    {
        CORE_UINT(32) pc;
        CORE_INT(32) dataa; //First data from register file
        CORE_INT(32) datab; //Second data, from register file or immediate value
        CORE_INT(32) datac;
        CORE_INT(32) datad; //Third data used only for store instruction and corresponding to rb
        CORE_INT(32) datae;
        CORE_UINT(5) dest; //Register to be written
        CORE_UINT(7) opCode;//OpCode of the instruction
        CORE_INT(32) memValue; //Second data, from register file or immediate value
        CORE_UINT(7) funct3 ;
        CORE_UINT(7) funct7;
        CORE_UINT(7) funct7_smaller ;
        CORE_UINT(6) shamt;
        CORE_UINT(5) rs1;
        CORE_UINT(5) rs2;
    } dctoEx;

    struct ExtoMem
    {
        CORE_UINT(32) pc;
        CORE_INT(32) result; //Result of the EX stage
        CORE_INT(32) datad;
        CORE_INT(32) datac; //Data to be stored in memory (if needed)
        CORE_UINT(5) dest; //Register to be written at WB stage
        CORE_UINT(1) WBena; //Is a WB is needed ?
        CORE_UINT(7) opCode; //OpCode of the operation
        CORE_INT(32) memValue; //Second data, from register file or immediate value
        CORE_UINT(5) rs2;
        CORE_UINT(7) funct3;
        CORE_UINT(2) sys_status;
    } extoMem;

    struct MemtoWB
    {
        CORE_INT(32) result; //Result to be written back
        CORE_UINT(5) dest; //Register to be written at WB stage
        CORE_UINT(1) WBena; //Is a WB is needed ?
        CORE_UINT(7) opCode;
        CORE_UINT(2) sys_status;
    } memtoWB;

    CORE_INT(32) reg_controller(CORE_UINT(32) address, CORE_UINT(1) op, CORE_INT(32) val);

    void doWB(CORE_UINT(1) *wb_bubble, CORE_UINT(1) *early_exit);
    void do_Mem(DO_MEM_PARAMETER, CORE_UINT(3) *mem_lock, CORE_UINT(1) *mem_bubble, CORE_UINT(1) *wb_bubble);
    void Ex(CORE_UINT(1) *ex_bubble, CORE_UINT(1) *mem_bubble, CORE_UINT(2) *sys_status);
    void DC(CORE_UINT(7) *prev_opCode, CORE_UINT(32) *prev_pc, CORE_UINT(3) mem_lock, CORE_UINT(1) *freeze_fetch, CORE_UINT(1) *ex_bubble);
    void Ft(CORE_UINT(32) *pc, CORE_UINT(1) freeze_fetch, CORE_INT(32) ins_memory[8192], CORE_UINT(3) mem_lock);
};

void doCore(CORE_UINT(32) pc, CORE_UINT(32) nbcycle, CORE_INT(32) ins_memory[8192], CORE_INT(32) dm[8192], CORE_INT(32) dm_out[8192]);
void doCache(ac_channel<DCacheRequest>& a, ac_channel<CORE_UINT(32)>& b, ac_channel<DCacheRequest>& c, ac_channel<CORE_UINT(32)>& d);

#endif  // __CORE_H__
