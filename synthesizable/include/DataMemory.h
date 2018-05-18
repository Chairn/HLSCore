/*
 * DataMemory.h
 *
 *  Created on: Sep 15, 2017
 *      Author: emascare
 */

#ifndef SRC_DATAMEMORY_H_
#define SRC_DATAMEMORY_H_

#include "portability.h"

//NOTE: Only supports aligned accesses
class DataMemory
{

public:

    ac_int<8, true> memory[8192][4];

    void set(ac_int<32, false> address, ac_int<32, true> value, ac_int<2, false> op)
    {
        // For store byte, op = 0
        // For store half word, op = 1
        // For store word, op = 3

        ac_int<13, false> wrapped_address = address % 8192;
        ac_int<8, true> byte0 = value.slc<8>(0);
        ac_int<8, true> byte1 = value.slc<8>(8);
        ac_int<8, true> byte2 = value.slc<8>(16);
        ac_int<8, true> byte3 = value.slc<8>(24);
        ac_int<2, false> offset = wrapped_address & 0x3;
        ac_int<13, false> location = wrapped_address >> 2;
        memory[location][offset] = byte0;
        if(op & 1)
        {
            memory[location][offset+1] = byte1;
        }
        if(op & 2)
        {
            memory[location][2] = byte2;
            memory[location][3] = byte3;
        }
    }

    ac_int<32, true> get(ac_int<32, false> address, ac_int<2, false> op, ac_int<1, false> sign)
    {
        // For load byte, op = 0
        // For load half word, op = 1
        // For load word, op = 3
        ac_int<13, false> wrapped_address = address % 8192;
        ac_int<2, false> offset = wrapped_address & 0x3;
        ac_int<13, false> location = wrapped_address >> 2;
        ac_int<32, true> result;
        result = sign ? -1 : 0;
        ac_int<8, true> byte0 = memory[location][0];
        ac_int<8, true> byte1 = memory[location][1];
        ac_int<8, true> byte2 = memory[location][2];
        ac_int<8, true> byte3 = memory[location][3];
        switch(offset)
        {
        case 0:
            break;
        case 1:
            byte0 = byte1;
            break;
        case 2:
            byte0 = byte2;
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

};

#endif /* SRC_DATAMEMORY_H_ */
