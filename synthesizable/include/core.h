#ifndef __CORE_H__
#define __CORE_H__

#include "portability.h"
#include "cache.h"

class Core
{
public:
    Core();

    HLS_DESIGN(interface)
    void doCachedStep(CORE_UINT(32) pc, CORE_UINT(32) nbcycle, FIFO(ICacheRequest)& toICache, FIFO(CORE_UINT(32))& fromICache, FIFO(DCacheRequest)& toDCache, FIFO(CORE_UINT(32))& fromDCache);

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
    void do_Mem(FIFO(DCacheRequest)& toDCache, FIFO(CORE_UINT(32))& fromDCache, CORE_UINT(3) *mem_lock, CORE_UINT(1) *mem_bubble, CORE_UINT(1) *wb_bubble);
    void Ex(CORE_UINT(1) *ex_bubble, CORE_UINT(1) *mem_bubble, CORE_UINT(2) *sys_status);
    void DC(CORE_UINT(7) *prev_opCode, CORE_UINT(32) *prev_pc, CORE_UINT(3) mem_lock, CORE_UINT(1) *freeze_fetch, CORE_UINT(1) *ex_bubble);
    void Ft(CORE_UINT(32) *pc, CORE_UINT(1) freeze_fetch, FIFO(ICacheRequest)& toICache, FIFO(CORE_UINT(32))& fromICache, CORE_UINT(3) mem_lock);
};

template<int Size, int Blocksize, int Associativity, CacheReplacementPolicy Policy>
class CachedCore
{
public:
    CachedCore()
    {}

    HLS_DESIGN(interface)
    void run(CORE_UINT(32) pc, CORE_UINT(32) nbcycle, CORE_UINT(32) ins_memory[DRAM_SIZE], CORE_UINT(32) dm[DRAM_SIZE])
    {
        core.doCachedStep(pc, nbcycle, fromCoretoICache, fromICachetoCore, fromCoretoDCache, fromDCachetoCore);
        dcache.run(fromCoretoDCache, fromDCachetoCore, dm);
        icache.run(fromCoretoICache, fromICachetoCore, ins_memory);
    }
private:
    Core core;
    DCache<Size, Blocksize, Associativity, Policy> dcache;
    ICache<Size, Blocksize, Associativity, Policy> icache;
    FIFO(DCacheRequest) fromCoretoDCache;
    FIFO(ICacheRequest) fromCoretoICache;
    FIFO(CORE_UINT(32)) fromDCachetoCore;
    FIFO(CORE_UINT(32)) fromICachetoCore;
};

void doCore(CORE_UINT(32) pc, CORE_UINT(32) nbcycle, FIFO(ICacheRequest)& toICache, FIFO(CORE_UINT(32))& fromICache, FIFO(DCacheRequest)& toDCache, FIFO(CORE_UINT(32))& fromDCache);
HLS_TOP()
void doCachedCore(CORE_UINT(32) pc, CORE_UINT(32) nbcycle, CORE_UINT(32) ins_memory[8192], CORE_UINT(32) dm[8192]);
void doCache(ac_channel<DCacheRequest>& a, ac_channel<CORE_UINT(32)>& b, CORE_UINT(32) memory[1000000]);

#endif  // __CORE_H__
