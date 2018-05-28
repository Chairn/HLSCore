#!/usr/bin/python3

import subprocess, os, sys, argparse

def lru(pol, way):
	ind = pol.index(way)
	for i in range(ind,0,-1):
		pol[i] = pol[i-1]
	pol[0] = way
	
progs = os.listdir("benchmarks/build")
progs[:] = [p for p in progs if p.endswith(".out")]

parser = argparse.ArgumentParser()
parser.add_argument("-m", "--make", help="Build before comparing", action="store_true")

args = parser.parse_args()
if args.make :
	subprocess.check_call(["make", "DEFINES=-Dnocache"])
	for p in progs:
		with open("traces/nocache_"+p+".log", "w") as output:
			subprocess.call(["./catapult.sim", "benchmarks/build/"+p], stdout=output)
			
	subprocess.check_call(["make"])
	for p in progs:
		with open("traces/cache_"+p+".log", "w") as output:
			subprocess.call(["./catapult.sim", "benchmarks/build/"+p], stdout=output)
	
N = 2**24
assoc = 4
sets = 8
os.chdir("traces")
for p in progs:
	print("\nComparing", p)
	with open("cache_"+p+".log", "r") as cache:
		dpolicy = [list(range(assoc-1, -1, -1)) for i in range(sets)]
		ipolicy = [list(range(assoc-1, -1, -1)) for i in range(sets)]
		cachemem = {}
		cacheimem = {}
		cachepcs = [0] * 10**7;
		pc = 0;
		i = 0
		maxad = 0
		for c in cache:
			i = i+1
			if c.startswith("i"):
				l = c.split()
				ad = int(l[1][1:], 16)
				ins = int(l[3], 16)
				cachepcs[pc] = ad
				pc = pc+1
		
				if ad in cacheimem:
					assert ins == cacheimem[ad], "error line {} : {} instruction is incorrect, expected {:08x} got {:08x} @{:06x}".format(i, n, cacheimem[ad], ins, ad)
				else:
					cacheimem[ad] = ins
			elif c.startswith("d"):
				l = c.split()
				ad = int(l[1][1:], 16)
				we = l[3][0] == "W"
				val = int(l[4], 16)
				size = int(l[5][2])
				sett = int(l[-2])
				way = int(l[-1])
				
				lru(dpolicy[sett], way)
				
				if we:		# add formatted write case
					cachemem[ad] = (val, True)
				elif ad in cachemem:
					assert cachemem[ad][0] == val, "error line {} : {} Read value is incorrect, expected {:08x} got {:08x} @{:06x}".format(i, c, cachemem[ad][0], val, ad)
				else:
					cachemem[ad] = (val, False)
					
				if ad > maxad:
					maxad = ad
			elif c.startswith("I"):
				pass
				
			elif c.startswith("da"):
				if c.find("write", 50) != -1:
					l = c[69:75].split()
					sett = int(l[0])
					way = int(l[1])
					assert way == policy[sett][-1], "error line {} : {} Wrong way replaced, expected {} got {}".format(i, c, policy[sett][-1], way)
			elif c.startswith("Su"):
				l = c.split()
				cycles = " ".join(l[-2:])
			elif c.startswith("mem"):
				break
				
		print(p, "with cache done in {}\nmaxad = {}({:08x})".format(cycles, maxad, maxad))
		
		cacheendmem = {}
		for c in cache:
			i = i+1
			l = c.split()
			ad = int(l[0], 16)
			val = int(l[2], 16)
			cacheendmem[ad] = val

		addresses = cachemem.keys() & cacheendmem.keys()
		for ad in addresses:
			assert cachemem[ad][0] == cacheendmem[ad], "In cache log, memory not consistent @{:06x} : expected {:08x} got {:08x}".format(i, cachemem[ad][0], cacheendmem[ad])

	with open("nocache_"+p+".log", "r") as nocache:
		mem = {}
		imem = {}
		pcs = [0] * 10**7
		pc = 0
		i = 0
		maxad = 0
		for n in nocache:
			i = i+1
			if n.startswith("i"):
				l = n.split()
				ad = int(l[1][1:], 16)
				ins = int(l[3], 16)
				pcs[pc] = ad
				pc = pc+1
				
				if ad in imem:
					assert ins == imem[ad], "error line {} : {} instruction is incorrect, expected {:08x} got {:08x} @{:06x}".format(i, n, imem[ad], ins, ad)
				else:
					imem[ad] = ins
			elif n.startswith("I"):
				pass
			elif n.startswith("d"):
				l = n.split()
				ad = int(l[1][1:], 16)
				we = l[3][0] == "W"
				val = int(l[4], 16)
				size = int(l[5][2])
				
				if we:		# add formatted write case
					mem[ad] = (val, True)
				elif ad in mem:
					assert mem[ad][0] == val, "error line {} : {} Read value is incorrect, expected {:08x} got {:08x} @{:06x}".format(i, n, mem[ad][0], val, ad)
				else:
					mem[ad] = (val, False)
					
				if ad > maxad:
					maxad = ad
				
			elif n.startswith("Su"):
				l = n.split()
				cycles = " ".join(l[-2:])
			elif n.startswith("memory :"):
				break
				
		print(p, "done in {}\nmaxad = {}({:08x})".format(cycles, maxad, maxad))
		
		endmem = {}
		for n in nocache:
			i = i+1
			l = n.split()
			ad = int(l[0], 16)
			val = int(l[2], 16)
			endmem[ad] = val

		addresses = mem.keys() & endmem.keys()
		for ad in addresses:
			assert mem[ad][0] == endmem[ad], "In uncached, memory not consistent @{:06x} : expected {:08x} got {:08x}".format(ad, mem[ad][0], endmem[ad])

	assert cachepcs == pcs, "Instructions paths are different in {}".format(p)
	assert cachemem == mem, "Memory simulations are different in {}".format(p)
	assert cacheendmem == endmem, "End memory are different in {}".format(p)
