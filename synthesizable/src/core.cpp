/* vim: set ts=4 nu ai: */
#include "riscvISA.h"
#include "core.h"
#include "cache.h"
#ifndef __SYNTHESIS__
#include "syscall.h"
#endif

void memorySet(unsigned int memory[N], ac_int<32, false> address, ac_int<32, true> value, ac_int<2, false> op)
{
    ac_int<32, false> wrapped_address = address;// % N;
    ac_int<32, true> byte0 = value.slc<8>(0);
    ac_int<32, true> byte1 = value.slc<8>(8);
    ac_int<32, true> byte2 = value.slc<8>(16);
    ac_int<32, true> byte3 = value.slc<8>(24);
    ac_int<2, false> offset = wrapped_address & 0x3;
    ac_int<32, false> location = wrapped_address >> 2;
    /*this wont synthesize in catapult */
#ifdef __SIMULATOR__
    //first read existing value
    ac_int<32, true> memory_val = memory[location];
    ac_int<32, true> shifted_byte;
    ac_int<32, true> mask[4];
    mask[0] = 0xFFFFFF00;
    mask[1] = 0xFFFF00FF;
    mask[2] = 0xFF00FFFF;
    mask[3] = 0x00FFFFFF;
    ac_int<32, true> val_to_be_written;
    val_to_be_written = (memory_val & mask[offset]) | (byte0 << (offset*8));
    if(op & 1)
    {
        val_to_be_written = (val_to_be_written & mask[offset+1]) | (byte1 << ((offset+1)*8));
    }
    if(op & 2)
    {
        val_to_be_written = value;
    }
    memory[location] = val_to_be_written;
    assert(false);
#endif
    /*this will synthesize in catapult */
#ifdef __CATAPULT__
#ifndef __SYNTHESIS__
    ac_int<32, false> memory_val = memory[location];
    formatwrite(address, op, memory_val, value);
    // data                                     size-1        @address         what was there before  what we want to write  what is actually written
    coredebug("dW%d  @%06x   %08x   %08x   %08x\n", op.to_int(), wrapped_address.to_int(), memory[location], value.to_int(), memory_val.to_int());
    memory[location] = memory_val;
#else
    memory[location] = value;
#endif
#endif
}

ac_int<32, true> memoryGet(unsigned int memory[N], ac_int<32, false> address, ac_int<2, false> op, bool sign)
{
    ac_int<32, false> wrapped_address = address;// % N;
    ac_int<2, false> offset = wrapped_address & 0x3;
    ac_int<32, false> location = wrapped_address >> 2;
    ac_int<32, true> result;
    result = sign ? -1 : 0;
    ac_int<32, false> mem_read = memory[location];
    /*ac_int<8, true> byte0 = mem_read.slc<8>(0);
    ac_int<8, true> byte1 = mem_read.slc<8>(8);
    ac_int<8, true> byte2 = mem_read.slc<8>(16);
    ac_int<8, true> byte3 = mem_read.slc<8>(24);

    switch(offset)
    {
    case 0:
        break;
    case 1:
        byte0 = byte1;
        break;
    case 2:
        byte0 = byte1;
        byte1 = byte3;
        break;
    case 3:
        byte0 = byte3;
        break;
    }
    result.set_slc(0,byte0);
    if(op & 1)
    {
        result.set_slc(8,byte1);
    }
    if(op & 2)
    {
        result.set_slc(16,byte2);
        result.set_slc(24,byte3);
    }
    simul(
    switch(address.slc<2>(0))   // address & offmask
    {
    case 1:
        assert(op == 0 && "address misalignment");
        break;
    case 2:
        assert(op <= 1 && "address misalignment");
        break;
    case 3:
        assert(op == 0 && "address misalignment");
        break;
    })*/
    formatread(address, op, sign, mem_read);
    // data                                   size-1        @address               what is in memory   what is actually read    sign extension
    coredebug("dR%d  @%06x   %08x   %08x   %s\n", op.to_int(), wrapped_address.to_int(), memory[location], mem_read.to_int(), sign?"true":"false");
    return mem_read;
}

void Ft(ac_int<32, false>& pc, bool freeze_fetch, ExtoMem extoMem, FtoDC& ftoDC, ac_int<3, false> mem_lock, // cpu internal logic
        ac_int<32, false>& iaddress,                                                // to cache
        ac_int<32, false> cachepc, unsigned int instruction, bool insvalid,         // from cache
        unsigned int ins_memory[N])
{
    ac_int<32, false> next_pc;
    if(freeze_fetch)
    {
        next_pc = pc;               // read after load dependency, stall 1 cycle
    }
    else
    {
        next_pc = pc + 4;
    }
    ac_int<32, false> jump_pc;
    if(mem_lock > 1)
    {
        jump_pc = next_pc;          // this means that we already had a jump before, so prevent double jumping when 2 jumps or branch in a row
    }
    else
    {
        jump_pc = extoMem.memValue; // forward the value from ex stage for jump & branch
    }
    bool control = 0;
    switch(extoMem.opCode)
    {
    case RISCV_BR:
        control = extoMem.result > 0 ? 1 : 0;   // result is the result of the comparison so either 1 or 0, memvalue is the jump address
        break;
    case RISCV_JAL:
        control = 1;
        break;
    case RISCV_JALR:
        control = 1;
        break;
    default:
        control = 0;
        break;
    }

#ifndef nocache
    if(!freeze_fetch)
    {
        if(insvalid && cachepc == pc)
        {
            ftoDC.instruction = instruction;
            ftoDC.pc = pc;
            if(control)
            {
                pc = jump_pc;
                iaddress = pc;
            }
            else
            {
                iaddress = pc;
                pc = next_pc;
            }
            debug("%06x\n", pc.to_int());
        }
        else
        {
            static bool init = false;
            ftoDC.instruction = 0x13;  // insert bubble (0x13 is NOP instruction and corresponds to ADDI r0, r0, 0)
            ftoDC.pc = 0;

            if(!init)
            {
                iaddress = pc;
                init = true;
            }
            else if(mem_lock > 1)
            {
                debug("Had a jump, not moving & ");
            }
            // if there's a jump at the same time of a miss, update to jump_pc
            else if(control)
            {
                pc = jump_pc;
                debug("Jumping & ");
            }
            iaddress = pc;
            debug("Requesting @%06x\n", pc.to_int());
        }
    }
    else      // we cannot overwrite because it would not execute the frozen instruction
    {
        ftoDC.instruction = ftoDC.instruction;
        ftoDC.pc = ftoDC.pc;
        debug("Fetch frozen, what to do?");
    }

    simul(if(ftoDC.pc)
    {
        coredebug("Ft   @%06x   %08x\n", ftoDC.pc.to_int(), ftoDC.instruction.to_int());
    }
    else
    {
        coredebug("Ft   \n");
    })
#else
    if(!freeze_fetch)
    {
        ftoDC.instruction = ins_memory[pc/4];
        ftoDC.pc = pc;
        //debug("i @%06x (%06x)   %08x    S:3\n", pc.to_int(), pc.to_int()/4, ins_memory[pc/4]);
    }
    else      // we cannot overwrite because it would not execute the frozen instruction
    {
        ftoDC.instruction = ftoDC.instruction;
        ftoDC.pc = ftoDC.pc;
        debug("Fetch frozen, what to do?");
    }
    simul(if(ftoDC.pc)
    {
        coredebug("Ft   @%06x   %08x\n", ftoDC.pc.to_int(), ftoDC.instruction.to_int());
    }
    else
    {
        coredebug("Ft   \n");
    })
    pc = control ? jump_pc : next_pc;
#endif
}

void DC(ac_int<32, true> REG[32], FtoDC ftoDC, ExtoMem extoMem, MemtoWB memtoWB, DCtoEx& dctoEx, ac_int<7, false>& prev_opCode,
        ac_int<32, false>& prev_pc, ac_int<3, false> mem_lock, bool& freeze_fetch, bool& ex_bubble)
{
    ac_int<5, false> rs1 = ftoDC.instruction.slc<5>(15);       // Decoding the instruction, in the DC stage
    ac_int<5, false> rs2 = ftoDC.instruction.slc<5>(20);
    ac_int<5, false> rd = ftoDC.instruction.slc<5>(7);
    ac_int<7, false> opcode = ftoDC.instruction.slc<7>(0);
    ac_int<7, false> funct7 = ftoDC.instruction.slc<7>(25);
    ac_int<7, false> funct3 = ftoDC.instruction.slc<3>(12);
    ac_int<7, false> funct7_smaller = 0;
    funct7_smaller.set_slc(1, ftoDC.instruction.slc<6>(26));
    ac_int<6, false> shamt = ftoDC.instruction.slc<6>(20);
    ac_int<13, false> imm13 = 0;
    imm13[12] = ftoDC.instruction[31];
    imm13.set_slc(5, ftoDC.instruction.slc<6>(25));
    imm13.set_slc(1, ftoDC.instruction.slc<4>(8));
    imm13[11] = ftoDC.instruction[7];
    ac_int<13, true> imm13_signed = 0;
    imm13_signed.set_slc(0, imm13);
    ac_int<12, false> imm12_I = ftoDC.instruction.slc<12>(20);
    ac_int<12, true> imm12_I_signed = ftoDC.instruction.slc<12>(20);
    ac_int<21, false> imm21_1 = 0;
    imm21_1.set_slc(12, ftoDC.instruction.slc<8>(12));
    imm21_1[11] = ftoDC.instruction[20];
    imm21_1.set_slc(1, ftoDC.instruction.slc<10>(21));
    imm21_1[20] = ftoDC.instruction[31];
    ac_int<21, true> imm21_1_signed = 0;
    imm21_1_signed.set_slc(0, imm21_1);
    ac_int<32, true> imm31_12 = 0;
    imm31_12.set_slc(12, ftoDC.instruction.slc<20>(12));
    bool forward_rs1;
    bool forward_rs2;
    bool forward_ex_or_mem_rs1;
    bool forward_ex_or_mem_rs2;
    bool datab_fwd = 0;
    ac_int<12, true> store_imm = 0;
    store_imm.set_slc(0,ftoDC.instruction.slc<5>(7));
    store_imm.set_slc(5,ftoDC.instruction.slc<7>(25));

    ac_int<32, true> reg_rs1 = REG[rs1.slc<5>(0)];
    ac_int<32, true> reg_rs2 = REG[rs2.slc<5>(0)];

    dctoEx.opCode = opcode;
    dctoEx.funct3 = funct3;
    dctoEx.funct7_smaller = funct7_smaller;
    dctoEx.funct7 = funct7;
    dctoEx.shamt = shamt;
    dctoEx.rs1 = rs1;
    dctoEx.rs2 = 0;
    dctoEx.pc = ftoDC.pc;
    dctoEx.instruction = ftoDC.instruction;
    freeze_fetch = 0;
    switch (opcode)
    {
    case RISCV_LUI:
        dctoEx.dest = rd;
        dctoEx.datab = imm31_12;
        break;
    case RISCV_AUIPC:
        dctoEx.dest = rd;
        dctoEx.datab = imm31_12;
        break;
    case RISCV_JAL:
        dctoEx.dest = rd;
        dctoEx.datab = imm21_1_signed;
        break;
    case RISCV_JALR:
        dctoEx.dest = rd;
        dctoEx.datab = imm12_I_signed;
        break;
    case RISCV_BR:
        dctoEx.rs2 = rs2;
        datab_fwd = 1;
        dctoEx.datac = imm13_signed;
        dctoEx.dest = 0;
        break;
    case RISCV_LD:
        dctoEx.dest = rd;
        dctoEx.memValue = imm12_I_signed;
        break;
    case RISCV_ST:
        dctoEx.datad = rs2;
        dctoEx.memValue = store_imm;
        dctoEx.dest = 0;
        break;
    case RISCV_OPI:
        dctoEx.dest = rd;
        dctoEx.memValue = imm12_I_signed;
        dctoEx.datab = imm12_I;
        break;
    case RISCV_OP:
        dctoEx.rs2 = rs2;
        datab_fwd = 1;
        dctoEx.dest = rd;
        break;
#ifndef __SYNTHESIS__
    case RISCV_SYSTEM:
        dctoEx.dest = 10;
        rs1 = 17;
        rs2 = 10;
        dctoEx.datac = (extoMem.dest == 11 && mem_lock < 2) ? extoMem.result : ((memtoWB.dest == 11 && mem_lock == 0) ? memtoWB.result : REG[11]);
        dctoEx.datad = (extoMem.dest == 12 && mem_lock < 2) ? extoMem.result : ((memtoWB.dest == 12 && mem_lock == 0) ? memtoWB.result : REG[12]);
        dctoEx.datae = (extoMem.dest == 13 && mem_lock < 2) ? extoMem.result : ((memtoWB.dest == 13 && mem_lock == 0) ? memtoWB.result : REG[13]);
        break;
#endif
    }

    forward_rs1 = ((extoMem.dest == rs1 && mem_lock < 2) || (memtoWB.dest == rs1 && mem_lock == 0)) ? 1 : 0;
    forward_rs2 = ((extoMem.dest == rs2 && mem_lock < 2) || (memtoWB.dest == rs2 && mem_lock == 0)) ? 1 : 0;
    forward_ex_or_mem_rs1 = (extoMem.dest == rs1) ? 1 : 0;
    forward_ex_or_mem_rs2 = (extoMem.dest == rs2) ? 1 : 0;

    dctoEx.dataa = (forward_rs1 && rs1 != 0) ? (forward_ex_or_mem_rs1 ? extoMem.result : memtoWB.result) : reg_rs1;
    if(opcode == RISCV_ST)
    {
        dctoEx.datac = (forward_rs2 && rs2 != 0) ? (forward_ex_or_mem_rs2 ? extoMem.result : memtoWB.result) : reg_rs2;
    }
    if(datab_fwd)
    {
        dctoEx.datab = (forward_rs2 && rs2 != 0) ? (forward_ex_or_mem_rs2 ? extoMem.result : memtoWB.result) : reg_rs2;
    }

    // stall 1 cycle if load to one of the register we use, e.g lw a5, someaddress followed by any operation that use a5
    if(prev_opCode == RISCV_LD && (extoMem.dest == rs1 || (opcode != RISCV_LD && extoMem.dest == rs2)) && mem_lock < 2 && prev_pc != ftoDC.pc)
    {
        freeze_fetch = 1;
        ex_bubble = 1;
    }
    prev_opCode = opcode;
    prev_pc = ftoDC.pc;

    simul(if(dctoEx.pc)
    {
        coredebug("Dc   @%06x   %08x\n", dctoEx.pc.to_int(), dctoEx.instruction.to_int());
    }
    else
    {
        coredebug("Dc   \n");
    })

}

void Ex(DCtoEx dctoEx, ExtoMem& extoMem, bool& ex_bubble, bool& mem_bubble, ac_int<2, false>& sys_status)
{
    ac_int<32, false> unsignedReg1;
    unsignedReg1.set_slc(0,(dctoEx.dataa).slc<32>(0));
    ac_int<32, false> unsignedReg2;
    unsignedReg2.set_slc(0,(dctoEx.datab).slc<32>(0));
    ac_int<33, true> mul_reg_a;
    ac_int<33, true> mul_reg_b;
    ac_int<66, true> longResult;
    ac_int<33, true> srli_reg;
    ac_int<33, true> srli_result;                   // Execution of the Instruction in EX stage
    extoMem.pc = dctoEx.pc;
    extoMem.instruction = dctoEx.instruction;
    extoMem.opCode = dctoEx.opCode;
    extoMem.dest = dctoEx.dest;
    extoMem.datac = dctoEx.datac;
    extoMem.funct3 = dctoEx.funct3;
    extoMem.datad = dctoEx.datad;
    extoMem.sys_status = 0;
    if ((extoMem.opCode != RISCV_BR) && (extoMem.opCode != RISCV_ST))
    {
        extoMem.WBena = 1;
    }
    else
    {
        extoMem.WBena = 0;
    }

    switch (dctoEx.opCode)
    {
    case RISCV_LUI:
        extoMem.result = dctoEx.datab;
        break;
    case RISCV_AUIPC:
        extoMem.result = dctoEx.pc + dctoEx.datab;
        break;
    case RISCV_JAL:
        extoMem.result = dctoEx.pc + 4;
        extoMem.memValue = dctoEx.pc + dctoEx.datab;
        break;
    case RISCV_JALR:
        extoMem.result = dctoEx.pc + 4;
        extoMem.memValue = (dctoEx.dataa + dctoEx.datab) & 0xfffffffe;
        break;
    case RISCV_BR: // Switch case for branch instructions
        switch(dctoEx.funct3)
        {
        case RISCV_BR_BEQ:
            extoMem.result = (dctoEx.dataa == dctoEx.datab);
            break;
        case RISCV_BR_BNE:
            extoMem.result = (dctoEx.dataa != dctoEx.datab);
            break;
        case RISCV_BR_BLT:
            extoMem.result = (dctoEx.dataa < dctoEx.datab);
            break;
        case RISCV_BR_BGE:
            extoMem.result = (dctoEx.dataa >= dctoEx.datab);
            break;
        case RISCV_BR_BLTU:
            extoMem.result = (unsignedReg1 < unsignedReg2);
            break;
        case RISCV_BR_BGEU:
            extoMem.result = (unsignedReg1 >= unsignedReg2);
            break;
        }
        extoMem.memValue = dctoEx.pc + dctoEx.datac;
        break;
    case RISCV_LD:
        extoMem.result = (dctoEx.dataa + dctoEx.memValue);
        break;
    case RISCV_ST:
        extoMem.result = (dctoEx.dataa + dctoEx.memValue);
        extoMem.datac = dctoEx.datac;
        extoMem.rs2 = dctoEx.rs2;
        break;
    case RISCV_OPI:
        switch(dctoEx.funct3)
        {
        case RISCV_OPI_ADDI:
            extoMem.result = dctoEx.dataa + dctoEx.memValue;
            break;
        case RISCV_OPI_SLTI:
            extoMem.result = (dctoEx.dataa < dctoEx.memValue) ? 1 : 0;
            break;
        case RISCV_OPI_SLTIU:
            extoMem.result = (unsignedReg1 < dctoEx.datab) ? 1 : 0;
            break;
        case RISCV_OPI_XORI:
            extoMem.result = dctoEx.dataa ^ dctoEx.memValue;
            break;
        case RISCV_OPI_ORI:
            extoMem.result =  dctoEx.dataa | dctoEx.memValue;
            break;
        case RISCV_OPI_ANDI:
            extoMem.result = dctoEx.dataa & dctoEx.memValue;
            break;
        case RISCV_OPI_SLLI:
            extoMem.result = dctoEx.dataa << dctoEx.shamt;
            break;
        case RISCV_OPI_SRI:
            if (dctoEx.funct7_smaller == RISCV_OPI_SRI_SRLI)
            {
                srli_reg.set_slc(0,dctoEx.dataa);
                srli_result = srli_reg >> dctoEx.shamt;
                extoMem.result = srli_result.slc<32>(0);
            }
            else //SRAI
                extoMem.result = dctoEx.dataa >> dctoEx.shamt;
            break;
        }
        break;
    case RISCV_OP:
        if (dctoEx.funct7 == 1)
        {
            mul_reg_a = dctoEx.dataa;
            mul_reg_b = dctoEx.datab;
            mul_reg_a[32] = dctoEx.dataa[31];
            mul_reg_b[32] = dctoEx.datab[31];
            switch (dctoEx.funct3)  //Switch case for multiplication operations (RV32M)
            {
            case RISCV_OP_M_MULHSU:
                mul_reg_b[32] = 0;
                break;
            case RISCV_OP_M_MULHU:
                mul_reg_a[32] = 0;
                mul_reg_b[32] = 0;
                break;
            }
            longResult = mul_reg_a * mul_reg_b;
            if(dctoEx.funct3 == RISCV_OP_M_MUL)
            {
                extoMem.result = longResult.slc<32>(0);
            }
            else
            {
                extoMem.result = longResult.slc<32>(32);
            }
        }
        else
        {
            switch(dctoEx.funct3)
            {
            case RISCV_OP_ADD:
                if (dctoEx.funct7 == RISCV_OP_ADD_ADD)
                    extoMem.result = dctoEx.dataa + dctoEx.datab;
                else
                    extoMem.result = dctoEx.dataa - dctoEx.datab;
                break;
            case RISCV_OP_SLL:
                extoMem.result = dctoEx.dataa << (dctoEx.datab & 0x3f);
                break;
            case RISCV_OP_SLT:
                extoMem.result = (dctoEx.dataa < dctoEx.datab) ? 1 : 0;
                break;
            case RISCV_OP_SLTU:
                extoMem.result = (unsignedReg1 < unsignedReg2) ? 1 : 0;
                break;
            case RISCV_OP_XOR:
                extoMem.result = dctoEx.dataa ^ dctoEx.datab;
                break;
            case RISCV_OP_OR:
                extoMem.result = dctoEx.dataa | dctoEx.datab;
                break;
            case RISCV_OP_AND:
                extoMem.result =  dctoEx.dataa & dctoEx.datab;
                break;
            }
        }
        break;
#ifndef __SYNTHESIS__
    case RISCV_SYSTEM:
        extoMem.result = solveSysCall(dctoEx.dataa, dctoEx.datab, dctoEx.datac, dctoEx.datad, dctoEx.datae, &extoMem.sys_status);
        break;
    default:
        fprintf(stderr, "Error : Unknown operation in Ex stage : @%06x	%08x\n", extoMem.pc.to_int(), extoMem.instruction.to_int());
        debug("Error : Unknown operation in Ex stage : @%06x	%08x\n", extoMem.pc.to_int(), extoMem.instruction.to_int());
        assert(false && "Unknown operation in Ex stage");
        break;
#endif
    }

    if(ex_bubble)
    {
        mem_bubble = 1;
        extoMem.pc = 0;
        extoMem.result = 0; //Result of the EX stage
        extoMem.datad = 0;
        extoMem.datac = 0;
        extoMem.dest = 0;
        extoMem.WBena = 0;
        extoMem.opCode = 0;
        extoMem.memValue = 0;
        extoMem.rs2 = 0;
        extoMem.funct3 = 0;
    }
    ex_bubble = 0;

    simul(if(extoMem.pc)
    {
        coredebug("Ex   @%06x   %08x\n", extoMem.pc.to_int(), extoMem.instruction.to_int());
    }
    else
    {
        coredebug("Ex   \n");
    })

}

void do_Mem(ExtoMem extoMem, MemtoWB& memtoWB, ac_int<3, false>& mem_lock, bool& mem_bubble, bool& wb_bubble, bool& cachelock,  // internal core control
            ac_int<32, false>& address, ac_int<2, false>& datasize, bool& signenable, bool& cacheenable, bool& writeenable, int& writevalue,      // control & data to cache
            int readvalue, bool datavalid, unsigned int data_memory[N]           // data & acknowledgment from cache
            #ifndef __SYNTHESIS__
            , int cycles
            #endif
            )
{
    if(cachelock)
    {
        if(datavalid)
        {
            memtoWB.WBena = extoMem.WBena;
            memtoWB.pc = extoMem.pc;
            if(!writeenable)
                memtoWB.result = readvalue;
            cachelock = false;
            cacheenable = false;
            writeenable = false;
        }
        else
        {
            memtoWB.pc = 0;
        }
    }
    else if(mem_bubble)
    {
        mem_bubble = 0;
        //wb_bubble = 1;
        memtoWB.pc = 0;
        memtoWB.instruction = 0;
        memtoWB.result = 0; //Result to be written back
        memtoWB.dest = 0; //Register to be written at WB stage
        memtoWB.WBena = 0; //Is a WB is needed ?
        memtoWB.opCode = 0;
        memtoWB.sys_status = 0;
    }
    else
    {
        if(mem_lock > 0)
        {
            mem_lock = mem_lock - 1;
            memtoWB.WBena = 0;
            simul(if(mem_lock)
                debug("I    @%06x\n", extoMem.pc.to_int());
            )
        }

        memtoWB.sys_status = extoMem.sys_status;
        memtoWB.opCode = extoMem.opCode;
        memtoWB.result = extoMem.result;

        bool sign = 0;

        if(mem_lock == 0)
        {
            memtoWB.pc = extoMem.pc;
            memtoWB.instruction = extoMem.instruction;
            memtoWB.WBena = extoMem.WBena;
            memtoWB.dest = extoMem.dest;
            switch(extoMem.opCode)
            {
            case RISCV_BR:
                if (extoMem.result)
                {
                    mem_lock = 3;
                }
                memtoWB.WBena = 0;
                memtoWB.dest = 0;
                break;
            case RISCV_JAL:
                mem_lock = 3;
                break;
            case RISCV_JALR:
                mem_lock = 3;
                break;
            case RISCV_LD:
                switch(extoMem.funct3)
                {
                case RISCV_LD_LW:
                    datasize = 3;
                    sign = 1;
                    break;
                case RISCV_LD_LH:
                    datasize = 1;
                    sign = 1;
                    break;
                case RISCV_LD_LHU:
                    datasize = 1;
                    sign = 0;
                    break;
                case RISCV_LD_LB:
                    datasize = 0;
                    sign = 1;
                    break;
                case RISCV_LD_LBU:
                    datasize = 1;
                    sign = 0;
                    break;
                }
#if 0//ndef nocache
                address = memtoWB.result;// % N;
                signenable = sign;
                cacheenable = true;
                writeenable = false;
                cachelock = true;
                memtoWB.WBena = false;
                memtoWB.pc = 0;
#else
                //debug("%5d  ", cycles);
                memtoWB.result = memoryGet(data_memory, memtoWB.result, datasize, sign);
#endif
                break;
            case RISCV_ST:
                switch(extoMem.funct3)
                {
                case RISCV_ST_STW:
                    datasize = 3;
                    break;
                case RISCV_ST_STH:
                    datasize = 1;
                    break;
                case RISCV_ST_STB:
                    datasize = 0;
                    break;
                }
#if 0//ndef nocache
                address = memtoWB.result;// % N;
                signenable = false;
                cacheenable = true;
                writeenable = true;
                writevalue = extoMem.datac;
                cachelock = true;
                memtoWB.WBena = false;
                memtoWB.pc = 0;
#else
                //debug("%5d  ", cycles);
                memorySet(data_memory, memtoWB.result, extoMem.datac, datasize);
#endif
                //data_memory[(memtoWB.result/4)%N] = extoMem.datac;
                break;
            }
        }
    }
    simul(if(memtoWB.pc)
    {
        coredebug("Mem  @%06x   %08x\n", memtoWB.pc.to_int(), memtoWB.instruction.to_int());
    }
    else
    {
        coredebug("Mem  \n");
    })
}

void doWB(ac_int<32, true> REG[32], MemtoWB memtoWB, bool& wb_bubble, bool& early_exit
#ifndef __SYNTHESIS__
    , unsigned int& numins
#endif
    )
{
    if (memtoWB.WBena == 1 && memtoWB.dest != 0)
    {
        REG[memtoWB.dest.slc<5>(0)] = memtoWB.result;
    }
#ifndef __SYNTHESIS__
    if(memtoWB.sys_status == 1)
    {
        debug("\nExit system call received, Exiting...");
        early_exit = 1;
    }
    else if(memtoWB.sys_status == 2)
    {
        debug("\nUnknown system call received, Exiting...");
        early_exit = 1;
    }

    if(memtoWB.pc)
    {
        static int lastpc = 0;
        if(memtoWB.pc != lastpc)
        {
            ++numins;
            lastpc = memtoWB.pc;
        }
        coredebug("\nWB   @%06x   %08x   (%d)\n", memtoWB.pc.to_int(), memtoWB.instruction.to_int(), numins);
    }
    else
    {
        coredebug("\nWB   \n");
    }
#endif
}

void doStep(ac_int<32, false> startpc, unsigned int ins_memory[N], unsigned int dm[N], bool& exit
#ifndef __SYNTHESIS__
    , uint64_t cycles
#endif
)
{
    static ICacheControl ictrl = {0};
    static unsigned int idata[Sets][Blocksize][Associativity] = {0};
    static bool idummy = ac::init_array<AC_VAL_DC>((unsigned int*)idata, Sets*Associativity*Blocksize);
    (void)idummy;
    static bool itaginit = ac::init_array<AC_VAL_DC>((ac_int<32-tagshift, false>*)ictrl.tag, Sets*Associativity);
    (void)itaginit;
    static bool ivalinit = ac::init_array<AC_VAL_0>((bool*)ictrl.valid, Sets*Associativity);
    (void)ivalinit;
    static DCacheControl dctrl = {0};
    static unsigned int ddata[Sets][Blocksize][Associativity] = {0};
    static bool dummy = ac::init_array<AC_VAL_DC>((unsigned int*)ddata, Sets*Associativity*Blocksize);
    (void)dummy;
    static bool taginit = ac::init_array<AC_VAL_DC>((ac_int<32-tagshift, false>*)dctrl.tag, Sets*Associativity);
    (void)taginit;
    static bool dirinit = ac::init_array<AC_VAL_DC>((bool*)dctrl.dirty, Sets*Associativity);
    (void)dirinit;
    static bool valinit = ac::init_array<AC_VAL_0>((bool*)dctrl.valid, Sets*Associativity);
    (void)valinit;
#if Policy == FIFO
    static bool dpolinit = ac::init_array<AC_VAL_DC>((ac_int<ac::log2_ceil<Associativity>::val, false>*)dctrl.policy, Sets);
    (void)dpolinit;
    static bool ipolinit = ac::init_array<AC_VAL_DC>((ac_int<ac::log2_ceil<Associativity>::val, false>*)ictrl.policy, Sets);
    (void)ipolinit;
#elif Policy == LRU
    static bool dpolinit = ac::init_array<AC_VAL_DC>((ac_int<Associativity * (Associativity-1) / 2, false>*)dctrl.policy, Sets);
    (void)dpolinit;
    static bool ipolinit = ac::init_array<AC_VAL_DC>((ac_int<Associativity * (Associativity-1) / 2, false>*)ictrl.policy, Sets);
    (void)ipolinit;
#elif Policy == RANDOM
    static bool rndinit = false;
    if(!rndinit)
    {
        rndinit = true;
        dctrl.policy = 0xF2D4B698;
        ictrl.policy = 0xF2D4B698;
    }
#endif
    static MemtoWB memtoWB = {0,0x13,0,0,0,0x13,0};
    static ExtoMem extoMem = {0,0x13,0,0,0,0,0,0x13,0};
    static DCtoEx dctoEx = {0,0x13,0,0,0,0,0,0,0x13,0}; // opCode = 0x13
    static FtoDC ftoDC = {0,0x13};

    static ac_int<32, true> REG[32] = {0,0,0xf00000,0,0,0,0,0,  //0xf00000 is stack address and so is the highest reachable address
                                       0,0,0,0,0,0,0,0,
                                       0,0,0,0,0,0,0,0,
                                       0,0,0,0,0,0,0,0}; // Register file
    static ac_int<2, false> sys_status = 0;

    static ac_int<7, false> prev_opCode = 0;
    static ac_int<32, false> prev_pc = 0;

    static ac_int<3, false> mem_lock = 0;
    static bool early_exit = false;

    static bool freeze_fetch = false;
    static bool ex_bubble = false;
    static bool mem_bubble = false;
    static bool wb_bubble = false;
    static bool cachelock = false;
    static ac_int<32, false> pc;
    static bool init = false;
    if(!init)
    {
        init = true;
        pc = startpc;   // startpc - 4 ?
    }

    /// Instruction cache
    static ac_int<32, false> iaddress = 0;
    static ac_int<32, false> cachepc = 0;
    static int instruction = 0;
    static bool insvalid = false;

    /// Data cache
    static ac_int<32, false> daddress = 0;
    static ac_int<2, false> datasize = 0;
    static bool signenable = false;
    static bool dcacheenable = false;
    static bool writeenable = false;
    static int writevalue = 0;
    static int readvalue = 0;
    static bool datavalid = false;


#ifndef __SYNTHESIS__
    static unsigned int numins = 0;
#endif

    if(cycles == 1)
        init = true;

    doWB(REG, memtoWB, wb_bubble, early_exit
     #ifndef __SYNTHESIS__
         , numins
     #endif
         );
    simul(coredebug("%d ", cycles);
    for(int i=0; i<32; i++)
    {
        if(REG[i])
            coredebug("%d:%08x ", i, (int)REG[i]);
    }
    )
    coredebug("\n");

    do_Mem(extoMem, memtoWB, mem_lock, mem_bubble, wb_bubble, cachelock,                // internal core control
           daddress, datasize, signenable, dcacheenable, writeenable, writevalue,       // control & data to cache
           readvalue, datavalid, dm                                                     // data & acknowledgment from cache
       #ifndef __SYNTHESIS__
           , cycles
       #endif
           );
#ifndef nocache
    dcache(dctrl, dm, ddata, daddress, datasize, signenable, dcacheenable, writeenable, writevalue, readvalue, datavalid
      #ifndef __SYNTHESIS__
          , cycles
      #endif
          );
#endif

    if(!cachelock)
    {
        Ex(dctoEx, extoMem, ex_bubble, mem_bubble, sys_status);
        DC(REG, ftoDC, extoMem, memtoWB, dctoEx, prev_opCode, prev_pc, mem_lock, freeze_fetch, ex_bubble);
        Ft(pc,freeze_fetch, extoMem, ftoDC, mem_lock, iaddress, cachepc, instruction, insvalid, ins_memory);
#ifndef nocache
        icache(ictrl, ins_memory, idata, iaddress, cachepc, instruction, insvalid
       #ifndef __SYNTHESIS__
               , cycles
       #endif
               );
#endif
        /*if(cycles > 1e1)
            exit = true;*/
    }


    if(early_exit)
    {
        exit = true;
     simul(
        //cache write back for simulation
        for(unsigned int i  = 0; i < Sets; ++i)
            for(unsigned int j = 0; j < Associativity; ++j)
                if(dctrl.dirty[i][j] && dctrl.valid[i][j])
                    for(unsigned int k = 0; k < Blocksize; ++k)
                        dm[(dctrl.tag[i][j] << (tagshift-2)) | (i << (setshift-2)) | k] = ddata[i][k][j];
    )
    }
}
