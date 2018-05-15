#!/usr/bin/python3

import argparse, os
from math import log2
import subprocess

genMem = """flow package require MemGen

flow run /MemGen/MemoryGenerator_BuildLib {
VENDOR           STMicroelectronics
RTLTOOL          DesignCompiler
TECHNOLOGY       {28nm FDSOI}
LIBRARY          ST_singleport_$setsx$bits
MODULE           ST_SPHD_BB_8192x32m16_aTdol_wrapper
OUTPUT_DIR       /udd/vegloff/Documents/comet/simplecore/memories/
FILES {
  { FILENAME /opt/DesignKit/cmos28fdsoi_29/memcut_28nm/C28SOI_SPHD_BB_170612/4.3-00.00/behaviour/verilog/SPHD_BB_170612.v         FILETYPE Verilog MODELTYPE generic PARSE 1 PATHTYPE abs STATICFILE 1 VHDL_LIB_MAPS work }
  { FILENAME /opt/DesignKit/cmos28fdsoi_29/memcut_28nm/C28SOI_SPHD_BB_170612/4.3-00.00/behaviour/verilog/SPHD_BB_170612_wrapper.v FILETYPE Verilog MODELTYPE generic PARSE 1 PATHTYPE abs STATICFILE 1 VHDL_LIB_MAPS work }
}
VHDLARRAYPATH    {}
WRITEDELAY       0.194
INITDELAY        1
READDELAY        0.940
VERILOGARRAYPATH {}
INPUTDELAY       0.146
WIDTH            $bits
AREA             $area
WRITELATENCY     1
RDWRRESOLUTION   UNKNOWN
READLATENCY      1
DEPTH            $sets
PARAMETERS {
  { PARAMETER Words                TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER Bits                 TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER mux                  TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER Bits_Func            TYPE hdl IGNORE 0 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER mask_bits            TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER repair_address_width TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER Addr                 TYPE hdl IGNORE 0 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER read_margin_size     TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER write_margin_size    TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
}
PORTS {
  { NAME port_0 MODE ReadWrite }
}
PINMAPS {
  { PHYPIN A     LOGPIN ADDRESS      DIRECTION in  WIDTH Addr      PHASE {} DEFAULT {} PORTS port_0 }
  { PHYPIN CK    LOGPIN CLOCK        DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {} PORTS port_0 }
  { PHYPIN CSN   LOGPIN CHIP_SELECT  DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {} PORTS {}     }
  { PHYPIN D     LOGPIN DATA_IN      DIRECTION in  WIDTH Bits_Func PHASE {} DEFAULT {} PORTS port_0 }
  { PHYPIN INITN LOGPIN UNCONNECTED  DIRECTION in  WIDTH 1.0       PHASE {} DEFAULT 1  PORTS {}     }
  { PHYPIN Q     LOGPIN DATA_OUT     DIRECTION out WIDTH Bits_Func PHASE {} DEFAULT {} PORTS port_0 }
  { PHYPIN WEN   LOGPIN WRITE_ENABLE DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {} PORTS port_0 }
}

}"""

genCore = """project set -incr_directory $setsx$bitscachedcore
solution options set Project/ProjectNamePrefix SimpleCore
solution new -state initial
solution options set /Input/CompilerFlags {-D __CATAPULT__=1}
//solution options set /Input/SearchPath ../include
flow package require /SCVerify
solution file add simplecore.cpp -type C++
solution file add simplecore_tb.cpp -type C++ -exclude true
go new
directive set -DESIGN_GOAL area
directive set -OLD_SCHED false
directive set -SPECULATE true
directive set -MERGEABLE true
directive set -REGISTER_THRESHOLD 4096 
directive set -MEM_MAP_THRESHOLD 32
directive set -FSM_ENCODING none
directive set -REG_MAX_FANOUT 0
directive set -NO_X_ASSIGNMENTS true
directive set -SAFE_FSM false
directive set -ASSIGN_OVERHEAD 0
directive set -UNROLL no
directive set -IO_MODE super
directive set -REGISTER_IDLE_SIGNAL false
directive set -IDLE_SIGNAL {}
directive set -STALL_FLAG false
directive set -TRANSACTION_DONE_SIGNAL true
directive set -DONE_FLAG {}
directive set -READY_FLAG {}
directive set -START_FLAG {}
directive set -BLOCK_SYNC none
directive set -TRANSACTION_SYNC ready
directive set -DATA_SYNC none
directive set -RESET_CLEARS_ALL_REGS true
directive set -CLOCK_OVERHEAD 20.000000
directive set -OPT_CONST_MULTS use_library
directive set -CHARACTERIZE_ROM false
directive set -PROTOTYPE_ROM true
directive set -ROM_THRESHOLD 64
directive set -CLUSTER_ADDTREE_IN_COUNT_THRESHOLD 0
directive set -CLUSTER_OPT_CONSTANT_INPUTS true
directive set -CLUSTER_RTL_SYN false
directive set -CLUSTER_FAST_MODE false
directive set -CLUSTER_TYPE combinational
directive set -COMPGRADE fast
directive set -DESIGN_HIERARCHY simplecachedcore
go analyze
solution options set ComponentLibs/SearchPath /udd/vegloff/Documents/comet/simplecore/memories/ -append
solution library add C28SOI_SC_12_CORE_LL_ccs -file {$MGC_HOME/pkgs/siflibs/designcompiler/CORE65LPHVT_ccs.lib} -- -rtlsyntool DesignCompiler -vendor STMicroelectronics -technology {28nm FDSOI}
solution library add ST_singleport_8192x32
solution library add ST_singleport_$cachesizex32
solution library add ST_singleport_$setsx$bits
go libraries
directive set -CLOCKS {clk {-CLOCK_PERIOD $period -CLOCK_EDGE rising -CLOCK_HIGH_TIME $halfperiod -CLOCK_OFFSET 0.000000 -CLOCK_UNCERTAINTY 0.0 -RESET_KIND sync -RESET_SYNC_NAME rst -RESET_SYNC_ACTIVE high -RESET_ASYNC_NAME arst_n -RESET_ASYNC_ACTIVE low -ENABLE_NAME {} -ENABLE_ACTIVE high}}
go assembly
directive set /simplecachedcore/core/main -PIPELINE_INIT_INTERVAL 1
directive set /simplecachedcore/core/reg:rsc -MAP_TO_MODULE {[Register]}
directive set /simplecachedcore/core -CLOCK_OVERHEAD 1.0
directive set /simplecachedcore/core/ctrl.tag:rsc -PACKING_MODE sidebyside
directive set /simplecachedcore/core/ctrl.tag:rsc -MAP_TO_MODULE ST_singleport_$setsx$bits.ST_SPHD_BB_8192x32m16_aTdol_wrapper
directive set /simplecachedcore/core/ctrl.tag -WORD_WIDTH $tags 
directive set /simplecachedcore/core/ctrl.dirty -RESOURCE ctrl.tag:rsc
directive set /simplecachedcore/core/ctrl.dirty -WORD_WIDTH $assoc
directive set /simplecachedcore/core/ctrl.valid -RESOURCE ctrl.tag:rsc
directive set /simplecachedcore/core/ctrl.valid -WORD_WIDTH $assoc
directive set /simplecachedcore/core/ctrl.policy -RESOURCE ctrl.tag:rsc
go architect
//ignore_memory_precedences -from read_mem() -to ...    // use this? dangerous if real dependency
go schedule
go extract
project save $setsx$bitscore.ccs"""

os.chdir("/udd/vegloff/Documents/comet/simplecore/")

def is_powerof2(num):
	return (num & (num - 1)) == 0 and num > 0

def nextpowerof2(num):
	assert(num > 0)
	return 2**(num-1).bit_length()

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--cache-size", help="Cache size in bytes", type=int)
parser.add_argument("-a", "--associativity", help="Cache associativity", type=int)
parser.add_argument("-b", "--blocksize", help="Cache blocksize in bytes", type=int)
parser.add_argument("-p", "--policy", help="Replacement policy")

args = parser.parse_args()
try:
	cachesize = args.cache_size
	assert is_powerof2(cachesize), "cachesize is not a power of 2"
except TypeError:
	cachesize = 1024
	print("Setting default cachesize to {}".format(cachesize))

try:
	associativity = args.associativity
	assert is_powerof2(associativity), "associativity is not a power of 2"
except TypeError:
	associativity = 4
	print("Setting default associativity to {}".format(associativity))

try:
	blocksize = int(args.blocksize/4)
	assert is_powerof2(blocksize), "Error : blocksize not a power of 2"
except TypeError:
	blocksize = int(32/4)
	print("Setting default blocksize to {}".format(blocksize))

try:
	policy = args.policy + ""
except TypeError:
	if associativity == 1:
		policy = "NONE"
	else:
		policy = "LRU"
	print("Setting default policy to {}".format(policy))

policy = policy.upper()
assert (associativity == 1 and policy == "NONE") or (associativity != 1 and policy != "NONE")
assert policy in ["LRU", "RANDOM", "NONE", "FIFO"]

defines = "-D{}={} "
defines = defines.format("Size", cachesize) + defines.format("Associativity", associativity) + defines.format("Blocksize", blocksize) + defines.format("Policy", policy)
subprocess.check_call(["make", "DEFINES="+defines])
with open("output.log", "w") as output:
	subprocess.check_call(["./testbench.sim"], stdout=output)

#define Sets            (Size/(Blocksize*sizeof(int))/Associativity)

#define setshift        (ac::log2_ceil<Blocksize>::val + 2)
#define tagshift        (ac::log2_ceil<Sets>::val + setshift)

sets = int(cachesize/(blocksize*4)/associativity)
tagbits = int(32 - log2(blocksize) - log2(sets) - 2)
bits = int(associativity*(tagbits+1+1)+associativity*(associativity-1)/2)

print("Generating cached core with cachesize ", cachesize, " bytes and associativity ", associativity, " and block size of ", blocksize, " bytes.")
print("Tagbits {}\nBitwidth of control {}({})\nSets {}".format(tagbits, bits, nextpowerof2(bits), sets))
bits = nextpowerof2(bits)
sets = nextpowerof2(sets)
area = int(1.4624*sets*bits+4915)

params = [("$sets", str(sets)), ("$bits", str(bits)), ("$area", str(area)), ("$tags", str(associativity*tagbits)), ("$assoc", str(associativity)), ("$cachesize", str(int(cachesize/4))), ("$period", "{:.2f}".format(1.5)), ("$halfperiod", "{:.2f}".format(0.75))]

memorypath = "memories/ST_singleport_{}x{}.lib".format(sets, bits)
if not os.path.isfile(memorypath):
	mem = genMem
	for k, v in params:
		mem = mem.replace(k, v)
	print("Generating memory {}x{} with area {}".format(sets, bits, area))
	with open("memories/generateCacheMemories.tcl", "w") as f:
		f.write(mem)
	print(os.getcwd())
	subprocess.check_call(["./catapult.sh", "-product lb -shell -f memories/generateCacheMemories.tcl"])

memorypath = "memories/ST_singleport_{}x{}.lib".format(int(cachesize/4), 32)
if not os.path.isfile(memorypath):
	mem = genMem
	mem = mem.replace("$sets", str(int(cachesize/4)))
	mem = mem.replace("$bits", "32")
	mem = mem.replace("$area", str(int(1.4624*cachesize*4+4915)))
	print("Generating memory {}x{} with area {}".format(int(cachesize/4), 32, int(1.4624*cachesize*4+4915)))
	with open("memories/generateCacheMemories.tcl", "w") as f:
		f.write(mem)
	print(os.getcwd())
	subprocess.check_call(["./catapult.sh", "-product lb -shell -f memories/generateCacheMemories.tcl"])

core = genCore	
for k, v in params:
	core = core.replace(k, v)
with open("genCore.tcl", "w") as f:
	f.write(core)

subprocess.check_call(["./catapult.sh", "-f genCore.tcl"])

#or add // pragma dofile( noerrors : true ) in file to continue even if errors
#then new tcl file
#subprocess.check_call(["./catapult.sh", "-shell -f mytcl.tcl myproject.ccs"])
# project load ...
# new iteration
# with -shell, error in synthesis is detected(at least for schedule error)



