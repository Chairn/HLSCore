#!/usr/bin/python3

import subprocess, os, sys, argparse

class Boundaries:
	"""Class to represents discontinuous set.
	Example: [0;12] U [125;256]"""
	def __init__(self, m=None, M=None):
		if m is not None and M is not None:
			self.bounds = [(m, M)]
		else:
			self.bounds = []

	def __repr__(self):
		if len(self.bounds):
			return " U ".join("[{};{}]".format(m, M) for m, M in b.bounds)
		else:
			return "Empty"
		
	def __contains__(self, item):
		for m, M in self.bounds:
			if item >= m and item <= M:
				return True
		return False
	
	def __iter__(self):
		for m, M in self.bounds:
			yield m
			yield M
		
	def append(self, m, M):
		self.bounds.append( (m, M) )
		
class mylist(list):
	"""Standard list with preallocate method."""
	def __init__(self, *args, **kwargs):
		list.__init__(self, *args, **kwargs)
		self.idx = list.__len__(self)		# current index used for append
		self.size = list.__len__(self)		# real size
	
	def __len__(self):
		return self.idx
	
	def append(self, item):
		if self.idx == self.size:
			list.append(self, item)
			self.idx += 1
			self.size += 1
		else:
			self[self.idx] = item
			self.idx += 1
			
	def clear(self):
		list.clear(self)
		self.idx = 0
		self.size = 0
		
	def reserve(self, size, item=None):
		if size <= self.size:
			return
		
		list.extend(self, [item] * (size - self.size))
		self.size = size
		assert self.size == list.__len__(self)
		
	
def lru(pol, way):
	ind = pol.index(way)
	for i in range(ind,0,-1):
		pol[i] = pol[i-1]
	pol[0] = way
	
def formatread(dmem, ad, size):
	read = 0
	for i, j in enumerate(range(ad, ad+size+1)):
		read |= (dmem[j][0] << (8*i))
	
	return read
	
def formatwrite(dmem, ad, size, val):
	for i, j in enumerate(range(ad, ad+size+1)):
		foo = (val & (0xFF << (8*i))) >> (8*i)
		dmem[j] = (foo, True)
		
	return formatread(dmem, ad & 0xFFFFFFFC, 3)
	
def readpreambule(fichier, prog):
	fichier.seek(0)			# go to start of file
	i = 2
	assert fichier.readline().split('/')[-1][:-1] == prog, "Wrong matching with "\
	"program's name : {}".format(prog)
	line = fichier.readline()
	ibound = Boundaries()
	dbound = Boundaries()
	while line.startswith("filling"):
		l = line.split()
		if l[1] == "instruction":
			ibound.append(int(l[-3], 16), int(l[-1], 16))
		elif l[1] == "data":
			dbound.append(int(l[-3], 16), int(l[-1], 16))
		else:
			assert False, "{} line {} : {} Neither data or instruction found in preamble"\
			.format(prog, i, line)
		line = fichier.readline()
		i += 1
	
	assert line == "instruction memory :\n", "{} line {} : {} Expected instruction memory"\
			.format(prog, i, line)
	
	line = fichier.readline()
	i += 1
	imem = [0] * 2**24
	while not line.startswith("data"):
		l = line.split()
		ad = int(l[0], 16)
		val = int(l[2], 16)
		imem[ad] = val
		line = fichier.readline()
		i += 1
	
	assert line == "data memory :\n", "{} line {} : {} Expected data memory"\
			.format(prog, i, line)
			
	line = fichier.readline()
	i += 1
	dmem = [(0, False)] * 2**24
	while not line.startswith("end"):
		l = line.split()
		ad = int(l[0], 16)
		val = int(l[2], 16)
		dmem[ad] = (val, False)
		line = fichier.readline()
		i += 1
	
	return imem, dmem, ibound, dbound, i

def parsefile(fichier, prog, cache=False):
	imem, dmem, ibound, dbound, i = readpreambule(fichier, prog)
	
	reg = [0] * 32
	reg[2] = 0x00f00000
	ipath = mylist()
	ipath.append( (0,0,0,0) )		# pc, instruction, registre modifié, nouvelle valeur
	ipath.reserve(10**7, (0,0,0,0))
	dpath = mylist()				# address, valeur, (0:read, 1:write), datasize, sign extension
	dpath.reserve(10**6, (0,0,False,0,False))
	#dmem = {}	
	#imem = {}
	endmem = {}
	
	if cache:
		dpolicy = [[i for i in range(assoc)] for i in range(sets)]
		#ipolicy = [[i for i in range(assoc-1,-1,-1)] for i in range(sets)]
	
	for line in fichier:
		i += 1
		if line.startswith("W"):		# WriteBack
			l = line.split()
			if len(l) > 1:
				ad = int(l[1][1:], 16)
				ins = int(l[2], 16)
				assert ad in ibound, "{} line {} : {} Instruction "\
				"out of bound @{:06x}".format(prog, i, line, ad)
				
				assert ins == imem[ad], "{} line {} : {} Instruction is incorrect, "\
				"expected {:08x} got {:08x} @{:06x}".format(prog, i, line, imem[ad], ins, ad)
					
				if ipath[-1][0] != ad:
					line = fichier.readline()
					i += 1
					l = line.split()
					changed = False
					for tmp in l[1:]:
						foo = tmp.split(':')
						idx = int(foo[0])
						val = int(foo[1], 16)
						if reg[idx] != val:
							assert changed == False, "{} line {}: {} Multiple registers "\
							"changed @{:06x}".format(prog, i, line, ad)
							ipath.append((ad, idx, val))
							reg[idx] = val
							changed = True
					if not changed:
						ipath.append((ad, 0, 0))
		
		elif line.startswith("d"):		# data access
			#dR{Size}	@{ad}	{memory}	{formattedread}	{signextension}
			#dW{Size}	@{ad}	{memory}	{writevalue}	{formattedwrite}
			l = line.split()
			we = l[0][1] == "W"
			size = int(l[0][2])
			ad = int(l[1][1:], 16)
			mem = int(l[2], 16)
			
			if we:
				writevalue = int(l[3], 16)
				fwrite = int(l[4], 16)
				sign = False
			else:
				fread = int(l[3], 16)
				sign = l[4] == "true"
				
			if cache:
				sett = int(l[-2])
				way = int(l[-1])
				
				lru(dpolicy[sett], way)
				
			if ad & 1:
				assert size == 0, "{} line {} : {} address misalignment @{:06x}"\
				.format(prog, i, line, ad)
			elif ad & 2:
				assert size <= 1, "{} line {} : {} address misalignment @{:06x}"\
				.format(prog, i, line, ad)
			
				
			if we:
				check = formatwrite(dmem, ad, size, writevalue)
				assert check == fwrite, "{} line {} : {} Formatted write failed @{:06x},"\
				" expected {:02x} got {:02x}".format(prog, i, line, ad, check, fwrite)
				
				for n, a in enumerate(range(ad, ad+size+1)):
					val = (writevalue & (0xFF << (8*n))) >> (8*n)
					dpath.append( (a, val, we, size, sign) )
			else:
				check = formatread(dmem, ad, size)
				assert check == fread, "{} line {} : {} Formatted read failed @{:06x},"\
				" expected {:02x} got {:02x}".format(prog, i, line, ad, check, fread)
				
				readuninit = False
				for n, a in enumerate(range(ad, ad+size+1)):
					# ~ print(n, hex(a), hex(dmem[a][0]), hex((fread >> (8*n)) & 0xFF))
					val = (fread >> (8*n)) & 0xFF
					assert dmem[a][0] == val, "{} line {} : {} Read value is incorrect"\
					" @{:06x}, expected {:02x} got {:02x}".format(prog, i, line, a, dmem[a][0], val)
				
					if not dmem[a][1]:
						if not a in dbound:
							if not readuninit:
								print("{} line {} : {} Reading unitialized memory @{:06x}"\
								.format(prog, i, line, ad))
								readuninit = True
							else:
								print(" Reading unitialized memory @{:06x}".format(a))
								
					dpath.append( (a, val, we, size, sign) )
		
		elif line.startswith("c"):			# Cache
			l = line.split()
			ad = int(l[1][1:], 16)
			sett = int(l[9])
			way = int(l[10])
			
			if l[0][1] == "d":				# data cache only
				assert way == dpolicy[sett][-1], "{} line {} : {} Replacing wrong way @{:06x},"\
				" expected {} got {} in set {}".format(prog, i, line, ad, dpolicy[sett][-1], way, sett)
			elif l[0][1] == "i":
				pass
			else:
				assert False
			
			
		# ~ elif line.startswith("M"):		# Memorization
			# ~ pass
	
		# ~ elif line.startswith("E"):		# Execute
			# ~ pass
			
		# ~ elif line.startswith("D"):		# Decode
			# ~ pass
			
		# ~ elif line.startswith("F"):		# Fetch
			# ~ pass
			
		elif line.startswith("S"):
			l = line.split()
			cycles = " ".join(l[-2:])
			break
		
	assert fichier.readline() == "memory :\n"
	for line in fichier:					# read end memory
		i += 1
		l = line.split()
		ad = int(l[0], 16)
		val = int(l[2], 16)
		endmem[ad] = val
	
	for ad in endmem:
		assert dmem[ad][0] == endmem[ad], "{}\n Memory not consistent @{:06x} " \
		": expected {:02x} got {:02x}".format(prog, ad, dmem[ad][0], endmem[ad])
	
	# rebuilds dmem from dpath
	bis = {}
	for data in dpath:						# address, valeur, (0:read, 1:write), datasize, sign extension
		if data[2]:
			bis[data[0]] = (data[1], True)
		elif data[0] in bis:
			assert bis[data[0]][0] == data[1], "{}\n Read value is incorrect, "\
			"expected {:02x} got {:02x} @{:06x}".format(prog, data[1], bis[data[0]][0], data[0])
		else:
			bis[data[0]] = (data[1], False)
		
	for ad in bis:
		assert dmem[ad] == bis[ad], "{}\n Memory not consistent @{:06x} " \
		": expected {} got {}".format(prog, ad, dmem[ad], bis[ad])
	return ipath, dpath, imem, dmem, endmem


progs = os.listdir("benchmarks/build")
progs[:] = [p for p in progs if p.endswith(".out")]

parser = argparse.ArgumentParser()
parser.add_argument("-m", "--make", help="Build before comparing", action="store_true")
parser.add_argument("-n", "--nocache", help="Performs nocache comparison and build nocache if -m is set", action="store_true")
parser.add_argument("-s", "--shell", help="Launch catapult in shell", action="store_true")
parser.add_argument("-c", "--cache-size", help="Cache size in bytes", type=int)
parser.add_argument("-a", "--associativity", help="Cache associativity", type=int)
parser.add_argument("-b", "--blocksize", help="Cache blocksize in bytes", type=int)
parser.add_argument("-p", "--policy", help="Replacement policy")

args = parser.parse_args()
if args.make:
	if args.nocache:
		subprocess.check_call(["make", "DEFINES=-Dnocache"])
		for p in progs:
			with open("traces/ref_"+p+".log", "w") as output:
				subprocess.call(["./catapult.sim", "benchmarks/build/"+p], stdout=output)
			
	subprocess.check_call(["make"])
	# ~ for p in progs:
		# ~ with open("traces/cache_"+p+".log", "w") as output:
			# ~ subprocess.call(["./catapult.sim", "benchmarks/build/"+p], stdout=output)

assoc = 4
sets = 8
for p in progs:
	if args.make:
		with open("traces/cache_"+p+".log", "w") as output:
			subprocess.call(["./catapult.sim", "benchmarks/build/"+p], stdout=output)
	
	print("\nLoading cache for", p)
	with open("traces/cache_"+p+".log", "r") as c:
		current = "cache"
		#make parsecache function...
		cache = parsefile(c, p, True)
	
	print("\nLoading reference for", p)
	with open("traces/ipath/ref_"+p+".log", "r") as r:
		current = "ref"
		ref = parsefile(r, p)	# ipath, dpath, imem, dmem, endmem
	
	assert ref[0] == cache[0]
	assert ref[1] == cache[1]
	assert ref[2] == cache[2]
	assert ref[3] == cache[3]
	assert ref[4] == cache[4]
		
	if args.nocache:
		print("Loading uncached for", p)
		with open("traces/nocache_"+p+".log", "r") as n:
			current = "nocache"
			nocache = parsefile(n, p)
		
		assert ref[0] == nocache[0]
		assert ref[1] == nocache[1]
		assert ref[2] == nocache[2]	
	
	
	
	# ~ for idx, pc in enumerate(ipath):
		# ~ assert pc == pcs[idx], "In uncached, instruction path incorrect on {}th instruction : expected @{:06x} got @{:06x}".format(idx, pc, pcs[idx])
	# ~ assert cachemem == mem, "Memory simulations are different in {}".format(p)
	# ~ assert cacheendmem == endmem, "End memory are different in {}".format(p)
