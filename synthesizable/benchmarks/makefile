objects = matmul4_4.o matmul8_8.o matmul16_16.o matmul32_32.o
executables = $(OUT_DIR)/matmul4_4.out $(OUT_DIR)/matmul8_8.out $(OUT_DIR)/matmul16_16.out $(OUT_DIR)/matmul32_32.out $(OUT_DIR)/qsort_10.out $(OUT_DIR)/qsort_50.out $(OUT_DIR)/qsort_100.out $(OUT_DIR)/qsort_1000.out
dependancies = src/matmul.c include/matmul.h src/qsort.c include/qsort.h
CCX = riscv32-unknown-elf-gcc
INCLUDE = ./include
OUT_DIR = ./build
TYPES = schar uchar short ushort int uint int64 uint64 

benchmarks = matmul qsort
msize = 4 8 16 32
qsize = 10 50 100 1000

all: $(dependancies)
	$(foreach size, $(msize), $(foreach type, $(TYPES), $(CCX) -I $(INCLUDE) src/matmul.c -Wno-overflow -o $(OUT_DIR)/matmul_$(type)_$(size).riscv -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(qsize), $(foreach type, $(TYPES), $(CCX) -I $(INCLUDE) src/qsort.c -Wno-overflow -o $(OUT_DIR)/qsort_$(type)_$(size).riscv -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(msize), $(foreach type, $(TYPES), g++ -I $(INCLUDE) src/matmul.c -Wno-overflow -o build/matmul_$(type)_$(size) -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(qsize), $(foreach type, $(TYPES), g++ -I $(INCLUDE) src/qsort.c -Wno-overflow -o build/qsort_$(type)_$(size) -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(msize), $(foreach type, $(TYPES), build/matmul_$(type)_$(size) > res/matmul_$(type)_$(size);))
	$(foreach size, $(qsize), $(foreach type, $(TYPES), build/qsort_$(type)_$(size) > res/qsort_$(type)_$(size);))
	$(foreach size, $(msize), $(foreach type, $(TYPES), riscv32-unknown-elf-objdump -D build/matmul_$(type)_$(size).riscv > build/matmul_$(type)_$(size).txt;))
	$(foreach size, $(qsize), $(foreach type, $(TYPES), riscv32-unknown-elf-objdump -D build/qsort_$(type)_$(size).riscv > build/qsort_$(type)_$(size).txt;))
	 

clean:
	$(foreach benchmark, $(benchmarks), rm build/$(benchmark)*;)
	$(foreach benchmark, $(benchmarks), rm res/$(benchmark)*;)

.PHONY: all clean directories test
