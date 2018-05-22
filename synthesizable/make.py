#!/usr/bin/python3

import subprocess, os

def lru(pol, way):
	ind = pol.index(way)
	for i in range(ind,0,-1):
		pol[i] = pol[i-1]
	pol[0] = way
	
progs = os.listdir("benchmarks/build")

# ~ subprocess.check_call(["make", "DEFINES=-Dnocache"])
# ~ for p in progs:
	# ~ with open("traces/nocache_"+p+".log", "w") as output:
		# ~ subprocess.call(["./catapult.sim", "benchmarks/build/"+p], stdout=output)
		
# ~ subprocess.check_call(["make"])
# ~ for p in progs:
	# ~ with open("traces/cache_"+p+".log", "w") as output:
		# ~ subprocess.call(["./catapult.sim", "benchmarks/build/"+p], stdout=output)
	
assoc = 4
sets = 8
os.chdir("traces")
for p in progs:
	with open("cache_"+p+".log", "r") as cache:
		policy = [list(range(assoc-1, -1, -1)) for i in range(sets)]
		cachemem = [(None, False)] * 8192
		print("Comparing", p)
		i = 0
		for c in cache:
			i = i+1
			if c.startswith("m"):
				l = c.split()
				we = l[2][0] == "W"
				val = int(l[2][2:], 16)
				ad = int(l[3][1:], 16)
				size = int(l[4][2])
				sett = int(l[-2])
				way = int(l[-1])
				
				if we:
					cachemem[ad] = (val, True)
				elif cachemem[ad][0] is not None:
					assert cachemem[ad][0] == val, "error line {} : {} value is incorrect, expected {:08x} got {:08x} @{:04x}".format(i, c, cachemem[ad][0], val, ad)
				else:
					cachemem[ad] = (val, False)
					
				lru(policy[sett], way)
				
			elif c.startswith("dm"):
				break
			elif c.startswith("da"):
				if c.find("write", 50) != -1:
					l = c[65:71].split()
					sett = int(l[0])
					way = int(l[1])
					assert way == policy[sett][-1], "error line {} : {} Wrong way replaced, expected {} got {}".format(i, c, policy[sett][-1], way)
		
		cacheendmem = [0] * 8192
		for c in cache:
			i = i+1
			l = c.split()
			ad = int(l[0], 16)
			val = int(l[2], 16)
			cacheendmem[ad] = val

		for i in range(8192):
			if cachemem[i][0] is not None:
				assert cachemem[i][0] == cacheendmem[i], "In cache log, memory not consistent @{:04x} : expected {:08x} got {:08x}".format(i, cachemem[i][0], cacheendmem[i])

	with open("nocache_"+p+".log", "r") as nocache:
		mem = [(None, False)] * 8192
		i = 0
		for n in nocache:
			i = i+1
			if n.startswith("m"):
				l = n.split()
				we = l[2][0] == "W"
				val = int(l[2][2:], 16)
				ad = int(l[3][1:], 16)
				size = int(l[4][2])	
				
				if we:
					mem[ad] = (val, True)
				elif mem[ad][0] is not None:
					assert mem[ad][0] == val, "error line {} : {} Read value is incorrect, expected {:08x} got {:08x} @{:04x}".format(i, n, mem[ad][0], val, ad)
				else:
					mem[ad] = (val, False)
			elif n.startswith("dm"):
				break
		
		endmem = [0] * 8192
		for n in nocache:
			i = i+1
			l = n.split()
			ad = int(l[0], 16)
			val = int(l[2], 16)
			endmem[ad] = val

		for i in range(8192):
			if mem[i][0] is not None:
				assert mem[i][0] == endmem[i], "In uncached, memory not consistent @{:04x} : expected {:08x} got {:08x}".format(i, mem[i][0], endmem[i])

	assert cachemem == mem, "Memory simulations are different in {}".format(p)
	assert cacheendmem == endmem, "End memory are different in {}".format(p)
