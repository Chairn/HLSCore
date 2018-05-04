AC=/opt/Catapult-10.0b/Mgc_home/shared/include
INC_PARAMS=$(foreach d, $(AC), -I$d)
VARS_CAT=-D__DEBUG__=1 -D__CATAPULT__=1
FILES=simplecore.cpp simplecore_tb.cpp
S_FILES=$(foreach f, $(FILES), $f)
HEADER=simplecore.h
I_HEADER=$(foreach f, $(HEADER), $f)
CFLAGS=-Wall -Wno-unknown-pragmas -Wno-unused-label

testbench: $(S_FILES) $(I_HEADER)
	g++ -o testbench.sim $(CFLAGS) $(INC_PARAMS) $(S_FILES) $(VARS_CAT)

debug: $(S_FILES) $(I_HEADER)
	g++ -g -O0 -o testbench.sim $(CFLAGS) $(INC_PARAMS) $(S_FILES) $(VARS_CAT)

clean:
	rm -rf *.o testbench
