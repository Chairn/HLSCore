#ifndef PORTABILITY_H_
#define PORTABILITY_H_

#define STRINGIFY(a) #a

#ifdef __VIVADO__
#include <ap_int.h>
#define CORE_UINT(param) ap_uint<param>
#define CORE_INT(param) ap_int<param>
#define SLC(size,low) range(size + low -1, low)
#define SET_SLC(low, value) range(low + value.length()-1,low) = value

#define HLS_PIPELINE(param) _Pragma(STRINGIFY(HLS PIPELINE II=param))
#define HLS_UNROLL(param)   _Pragma(STRINGIFY(HLS UNROLL factor=param))
#define HLS_TOP(param)      _Pragma(STRINGIFY(HLS top name=param))
#define HLS_DESIGN

#else
#include <ac_int.h>
#include <ac_channel.h>
#define CORE_UINT(param) ac_int<param, false>
#define CORE_INT(param) ac_int<param, true>
#define SLC(size,low) slc<size>(low)
#define SET_SLC(low, value) set_slc(low, value)

#define HLS_PIPELINE(param) _Pragma(STRINGIFY(hls_pipeline_init_interval param))
#define HLS_UNROLL(param)   _Pragma(STRINGIFY(hls_unroll param))
#define HLS_TOP(param)      _Pragma(STRINGIFY(hls_design top))
#define HLS_DESIGN          _Pragma(STRINGIFY(hls_design))

#endif

#define DRAM_WIDTH 32
#define DRAM_SIZE 100000000

#endif /* For PORTABILITY_H_ */
