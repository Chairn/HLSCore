#ifndef SRC_DATAMEMORYCATAPULT_H_
#define SRC_DATAMEMORYCATAPULT_H_

#include "portability.h"

class DataMemory
{

public:

    ac_int<32, true> memory[N];

    void set(ac_int<32, false> address, ac_int<32, true> value, ac_int<2, false> op)
    {
        ac_int<13, false> wrapped_address = address % N;
        memory[wrapped_address >> 2] = value;
    }

    ac_int<32, true> get(ac_int<32, false> address, ac_int<2, false> op,
                     ac_int<1, false> sign)
    {
        ac_int<13, false> wrapped_address = address % N;
        ac_int<2, false> offset = wrapped_address & 0x3;
        ac_int<13, false> location = wrapped_address >> 2;
        ac_int<32, true> result;
        result = sign ? -1 : 0;
        ac_int<32, true> mem_read = memory[location];
        result=mem_read;
        //ac_int<8, true> byte0 = mem_read.slc<8>(0);
        //ac_int<8, true> byte1 = mem_read.slc<8>(8);
        //ac_int<8, true> byte2 = mem_read.slc<8>(16);
        //ac_int<8, true> byte3 = mem_read.slc<8>(24);
        /*switch(offset){
        	case 0:
        		break;
        	case 1:
        		byte0 = byte1;
        		break;
        	case 2:
        		byte0 = byte1;
        		byte1 = byte3;
        	case 3:
        		byte0 = byte3;
        		break;
        }
        result.set_slc(0,byte0);
        if(op & 1){
        	result.set_slc(8,byte1);
        }
        if(op & 2){
        	result.set_slc(16,byte2);
        	result.set_slc(24,byte3);
        }*/
        return result;
    }

};

#endif /* SRC_DATAMEMORYCATAPULT_H_ */
