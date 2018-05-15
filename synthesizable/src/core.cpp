/* vim: set ts=4 nu ai: */
#include "riscvISA.h"
#include "core.h"
#include "cache.h"
#if defined(__SIMULATOR__) || defined(__DEBUG__)
#include "syscall.h"
#endif

void memorySet(ac_int<32, true> memory[8192], ac_int<32, false> address, ac_int<32, true> value, ac_int<2, false> op)
{
    ac_int<13, false> wrapped_address = address % 8192;
    ac_int<32, true> byte0 = value.slc<8>(0);
    ac_int<32, true> byte1 = value.slc<8>(8);
    ac_int<32, true> byte2 = value.slc<8>(16);
    ac_int<32, true> byte3 = value.slc<8>(24);
    ac_int<2, false> offset = wrapped_address & 0x3;
    ac_int<13, false> location = wrapped_address >> 2;
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
#endif
    /*this will synthesize in catapult */
#ifdef __CATAPULT__
    memory[location] = value;
#endif
}

ac_int<32, true> memoryGet(ac_int<32, true> memory[8192], ac_int<32, false> address, ac_int<2, false> op, bool sign)
{
    ac_int<13, false> wrapped_address = address % 8192;
    ac_int<2, false> offset = wrapped_address & 0x3;
    ac_int<13, false> location = wrapped_address >> 2;
    ac_int<32, true> result;
    result = sign ? -1 : 0;
    ac_int<32, true> mem_read = memory[location];
    ac_int<8, true> byte0 = mem_read.slc<8>(0);
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
    return result;
}

void Ft(ac_int<32, false>& pc, bool freeze_fetch, ExtoMem extoMem, ac_int<32, true> ins_memory[8192], FtoDC& ftoDC, ac_int<3, false> mem_lock)
{

    ac_int<32, false> next_pc;
    if(freeze_fetch)
    {
        next_pc = pc;
    }
    else
    {
        next_pc = pc + 4;
    }
    ac_int<32, false> jump_pc;
    if(mem_lock > 1)
    {
        jump_pc = next_pc;
    }
    else
    {
        jump_pc = extoMem.memValue;
    }
    bool control = 0;
    switch(extoMem.opCode)
    {
    case RISCV_BR:
        control = extoMem.result > 0 ? 1 : 0;
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
    if(!freeze_fetch)
    {
        (ftoDC.instruction).set_slc(0, ins_memory[(pc & 0x0FFFF)/4]);
        ftoDC.pc = pc;
    }
    pc = control ? jump_pc : next_pc;
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
    freeze_fetch = 0;
    switch (opcode)
    {
    case RISCV_LUI:
        dctoEx.dest = rd;
        dctoEx.datab = imm31_12;
        break;
    case RISCV_AUIPC:
        dctoEx.dest=rd;
        dctoEx.datab = imm31_12;
        break;
    case RISCV_JAL:
        dctoEx.dest=rd;
        dctoEx.datab = imm21_1_signed;
        break;
    case RISCV_JALR:
        dctoEx.dest=rd;
        dctoEx.datab = imm12_I_signed;
        break;
    case RISCV_BR:
        dctoEx.rs2=rs2;
        datab_fwd = 1;
        dctoEx.datac = imm13_signed;
        dctoEx.dest = 0;
        break;
    case RISCV_LD:
        dctoEx.dest=rd;
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
        dctoEx.rs2=rs2;
        datab_fwd = 1;
        dctoEx.dest=rd;
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

    if(prev_opCode == RISCV_LD && (extoMem.dest == rs1 || (opcode != RISCV_LD && extoMem.dest == rs2)) && mem_lock < 2 && prev_pc != ftoDC.pc)
    {
        freeze_fetch = 1;
        ex_bubble = 1;
    }
    prev_opCode = opcode;
    prev_pc = ftoDC.pc;
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
            extoMem.result = (dctoEx.dataa== dctoEx.datab);
            break;
        case RISCV_BR_BNE:
            extoMem.result = (dctoEx.dataa!= dctoEx.datab);
            break;
        case RISCV_BR_BLT:
            extoMem.result = (dctoEx.dataa< dctoEx.datab);
            break;
        case RISCV_BR_BGE:
            extoMem.result = (dctoEx.dataa>= dctoEx.datab);
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
            extoMem.result=  dctoEx.dataa << dctoEx.shamt;
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
}

void do_Mem(ac_int<32, true> data_memory[8192], ExtoMem extoMem, MemtoWB& memtoWB, ac_int<3, false>& mem_lock, bool& mem_bubble, bool& wb_bubble)
{

    if(mem_bubble)
    {
        mem_bubble = 0;
        //wb_bubble = 1;
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
        }

        memtoWB.sys_status = extoMem.sys_status;
        memtoWB.opCode = extoMem.opCode;
        memtoWB.result = extoMem.result;

        ac_int<2, false> st_op = 0;
        ac_int<2, false> ld_op = 0;
        bool sign = 0;

        if(mem_lock == 0)
        {
            memtoWB.WBena = extoMem.WBena;
            memtoWB.dest = extoMem.dest; // Memory operation in do_Mem stage
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
                    ld_op = 3;
                    sign = 1;
                    break;
                case RISCV_LD_LH:
                    ld_op = 1;
                    sign = 1;
                    break;
                case RISCV_LD_LHU:
                    ld_op = 1;
                    sign = 0;
                    break;
                case RISCV_LD_LB:
                    ld_op = 0;
                    sign = 1;
                    break;
                case RISCV_LD_LBU:
                    ld_op = 1;
                    sign = 0;
                    break;
                }
                memtoWB.result = memoryGet(data_memory, memtoWB.result, ld_op, sign);
                break;
            case RISCV_ST:
                switch(extoMem.funct3)
                {
                case RISCV_ST_STW:
                    st_op = 3;
                    break;
                case RISCV_ST_STH:
                    st_op = 1;
                    break;
                case RISCV_ST_STB:
                    st_op = 0;
                    break;
                }
                memorySet(data_memory, memtoWB.result, extoMem.datac, st_op);
                //data_memory[(memtoWB.result/4)%8192] = extoMem.datac;
                break;
            }
        }
    }
}


void doWB(ac_int<32, true> REG[32], MemtoWB memtoWB, bool& wb_bubble, bool& early_exit)
{
    if (memtoWB.WBena == 1 && memtoWB.dest != 0)
    {
        REG[memtoWB.dest.slc<5>(0)] = memtoWB.result;
    }
#ifndef __SYNTHESIS__
    if(memtoWB.sys_status == 1)
    {
        debug("Exit system call received, Exiting... ");
        early_exit = 1;
    }
    else if(memtoWB.sys_status == 2)
    {
        debug("Unknown system call received, Exiting... ");
        early_exit = 1;
    }
#endif
}

void doStep(ac_int<32, false> startpc, ac_int<32, true> ins_memory[8192], ac_int<32, true> dm[8192], bool& exit
#ifndef __SYNTHESIS__
    , int& cycles
#endif
)
{
    static MemtoWB memtoWB = {0};
    static ExtoMem extoMem = {0};
    static DCtoEx dctoEx = {0};
    static FtoDC ftoDC = {0};

    static ac_int<32, true> REG[32] = {0,0,0xf00000,0,0,0,0,0,
                                       0,0,0,0,0,0,0,0,
                                       0,0,0,0,0,0,0,0,
                                       0,0,0,0,0,0,0,0}; // Register file
    static ac_int<2, false> sys_status = 0;

    static ac_int<7, false> prev_opCode = 0;
    static ac_int<32, false> prev_pc = 0;

    static ac_int<3, false> mem_lock = 0;
    static bool early_exit = 0;

    static bool freeze_fetch = 0;
    static bool ex_bubble = 0;
    static bool mem_bubble = 0;
    static bool wb_bubble = 0;
    static ac_int<32, false> pc;


    static bool init = false;
    if(!init)
    {
        init = true;
        pc = startpc;
        /*memtoWB.WBena = 0;
        memtoWB.dest = 0;
        memtoWB.opCode = 0;
        extoMem.opCode = 0;
        extoMem.sys_status = 0;
        dctoEx.opCode = 0;

        dctoEx.dataa = 0; //First data from register file
        dctoEx.datab = 0; //Second data, from register file or immediate value
        dctoEx.datac = 0;
        dctoEx.datad = 0; //Third data used only for store instruction and corresponding to rb
        dctoEx.dest = 0; //Register to be written

        for(int i = 0; i<32; i++)
        {
            REG[i] = 0;
        }

        REG[2] = 0xf00000;
        sys_status = 0;*/
    }

    debug("%d;%x;%08x ", cycles++, (int)pc, (int)ins_memory[(pc & 0x0FFFF)/4]);

    doWB(REG, memtoWB, wb_bubble, early_exit);
    do_Mem(dm, extoMem, memtoWB, mem_lock, mem_bubble, wb_bubble);
    Ex(dctoEx, extoMem, ex_bubble, mem_bubble, sys_status);
    DC(REG, ftoDC, extoMem, memtoWB, dctoEx, prev_opCode, prev_pc, mem_lock, freeze_fetch, ex_bubble);
    Ft(pc,freeze_fetch, extoMem, ins_memory, ftoDC, mem_lock);

    /*simul(
    for(int i=0; i<32; i++)
    {
        debug(";%08x",(int)REG[i]);
    }
    )*/
    debug("\n");

    if(early_exit)
        exit = true;
}
