#!/usr/bin/python3

import os, subprocess, sys
from check import *

if __name__ == "__main__":
	progs = os.listdir("benchmarks/build")
	progs[:] = [p for p in progs if p.endswith(".out")]
	
	rcycles = list()
	ref = list()
	for i, p in enumerate(progs):
		print("\nLoading reference for", p)
		with open("traces/ipath/ref_"+p+".log", "r") as r:
			ref.append(parsefile(r, p))	# ipath, dpath, imem, dmem, endmem, cycles, cpi
			rcycles.append((ref[i][5], ref[i][6]))

	assoc = 4
	for blocksize in (4,8,16):
		for cachesize in (1024, 2048, 4096, 8192, 16384, 32768, 65536):
			policy = "lru"
			policy = 0 if policy == "none" else 1 if policy == "fifo" else 2 if policy == "lru" else 3 if policy == "random" else 0
			makeparameters = "-DSize={cachesize} -DBlocksize={blocksize} -DAssociativity={assoc} -DPolicy={policy}".format(**globals())
			subprocess.check_call(["make", "DEFINES="+makeparameters])
			ccycles = list()
			for j, p in enumerate(progs):
				with subprocess.Popen(["./catapult.sim", "benchmarks/build/"+p], stdout=subprocess.PIPE,\
				universal_newlines=True) as output:
					print("")
					current = "cache"
					try:
						cipath, cdpath, cimem, cdmem, cendmem, cycles, cpi = parsefile(output, p, True)
						ccycles.append((cycles, cpi))
					except BaseException as e:
						print(str(type(e)).split("'")[1], ":", e)
						with open("traces/cache_"+p+".log", "w") as dump:
							print("Dumping to", "traces/cache_"+p+".log")
							dump.write("".join(line for line in check.lues))
						cipath = ipath
						cdpath = dpath
						
						for i, el in enumerate(ripath):	# pc, registre modifié, nouvelle valeur
							assert el == cipath[i], "{} : ipath in {} is different after {} instructions, expected "\
					"(@{:06x} {} {:08x}) got (@{:06x} {} {:08x})".format(p, current, i, *(el+cipath[i]))
					
					
					ripath, rdpath, rimem, rdmem, rendmem, cycles, cpi = ref[j]
					if ripath != cipath:				# != and == operator are much more faster than a loop
						# if different, then look which elements are different
						for i, el in enumerate(ripath):	# pc, registre modifié, nouvelle valeur
							assert el == cipath[i], "{} : ipath in {} is different after {} instructions, expected "\
							"(@{:06x} {} {:08x}) got (@{:06x} {} {:08x})".format(p, current, i, *(el+cipath[i]))
					if rdpath != cdpath:
						for i, el in enumerate(rdpath):	# address, valeur, (0:read, 1:write), datasize, sign extension
							assert el == cdpath[i], "{} : dpath in {} is different after {} memory access, expected "\
						"({}{} @{:06x} {:02x} {}) got ({}{} @{:06x} {:02x} {})".format(p, current, i, el[2], el[3], el[0],\
						el[1], el[4], cdpath[i][2], cdpath[i][3], cdpath[i][0], cdpath[i][1], cdpath[i][4])
					
					if rimem != cimem:
						for i, el in enumerate(rimem):
							assert el == cimem[i], "{} : imem in {} is different @{:06x}, expected "\
						"{:02x} got {:02x}".format(p, current, i, el, cimem[i])
						
					if rdmem != cdmem:
						for i, el in enumerate(rdmem):
							assert el == cdmem[i], "{} : dmem in {} is different @{:06x}, expected "\
						"({:02x}, {}) got ({:02x}, {})".format(p, current, i, el[0], el[1], cdmem[i][0], cdmem[i][1])
					
					if rendmem != cendmem:
						addresses = rendmem.keys() & cendmem.keys()
						assert len(addresses) == len(rendmem) and len(addresses) == len(cendmem)
						for ad in addresses:
							assert rendmem[ad] == cendmem[ad], "{} : endmem in {} is different @{:06x}"\
							", expected {:02x} got {:x02}".format(p, current, ad, rendmem[ad], cendmem[ad])

			print("\n\n{:<20s} {:^21s} {:^21s}".format("Timings", "Reference", "Cache"))
			print("{:<20s} {:>7s} {:>10s} {:>10s} {:>10s} {:>10s}".format("Benchmark", "Cycles",\
			"CPI", "Cycles", "CPI", "Variation"))
			for i in range(len(rcycles)):
				print("{:<20s} {:7d} {:>10.2f} {:10d} {:>10.2f} {:>+10.0%}".format(progs[i], rcycles[i][0],\
				rcycles[i][1], ccycles[i][0], ccycles[i][1], ccycles[i][0]/rcycles[i][0]-1))

			with open("res.txt", "a") as res:
				print("\nCachesize : {:<6d}octets Associativity : {} Blocksize : {:>2d} octets".format(\
				cachesize, assoc, 4*blocksize), file=res)
				print("{:<20s} {:^21s} {:^21s}".format("Timings", "Reference", "Cache"), file=res)
				print("{:<20s} {:>7s} {:>10s} {:>10s} {:>10s} {:>10s}".format("Benchmark", "Cycles",\
			"CPI", "Cycles", "CPI", "Variation"), file=res)
				for i in range(len(rcycles)):
					print("{:<20s} {:7d} {:>10.2f} {:10d} {:>10.2f} {:>+10.0%}".format(progs[i], rcycles[i][0],\
					rcycles[i][1], ccycles[i][0], ccycles[i][1], ccycles[i][0]/rcycles[i][0]-1), file=res)
