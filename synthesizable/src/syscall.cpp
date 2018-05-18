#include "riscvISA.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <map>
#include "portability.h"

std::map<ac_int<16, true>, FILE*> fileMap;
FILE **inStreams, **outStreams;
int nbInStreams, nbOutStreams;

void stb(ac_int<32, false> addr, ac_int<8, true> value)
{
}

void sth(ac_int<32, false> addr, ac_int<16, true> value)
{
}

void stw(ac_int<32, false> addr, ac_int<32, true> value)
{
    stb(addr+3, value.slc<8>(24));
    stb(addr+2, value.slc<8>(16));
    stb(addr+1, value.slc<8>(8));
    stb(addr+0, value.slc<8>(0));
}

void _std(ac_int<32, false> addr, ac_int<32, true> value)
{
    stb(addr+7, value.slc<8>(56));
    stb(addr+6, value.slc<8>(48));
    stb(addr+5, value.slc<8>(40));
    stb(addr+4, value.slc<8>(32));
    stb(addr+3, value.slc<8>(24));
    stb(addr+2, value.slc<8>(16));
    stb(addr+1, value.slc<8>(8));
    stb(addr+0, value.slc<8>(0));
}

ac_int<8, true> ldb(ac_int<32, false> addr)
{
    ac_int<8, true> result = 0;
    return result;
}

ac_int<16, true> ldh(ac_int<32, false> addr)
{
    ac_int<16, true> result = 0;
    result.set_slc(8, ldb(addr+1));
    result.set_slc(0, ldb(addr));
    return result;
}

ac_int<32, true> ldw(ac_int<32, false> addr)
{
    ac_int<32, true> result = 0;
    result.set_slc(24, ldb(addr+3));
    result.set_slc(16, ldb(addr+2));
    result.set_slc(8, ldb(addr+1));
    result.set_slc(0, ldb(addr));
    return result;
}

ac_int<32, true> ldd(ac_int<32, false> addr)
{
    ac_int<32, true> result = 0;
    result.set_slc(56, ldb(addr+7));
    result.set_slc(48, ldb(addr+6));
    result.set_slc(40, ldb(addr+5));
    result.set_slc(32, ldb(addr+4));
    result.set_slc(24, ldb(addr+3));
    result.set_slc(16, ldb(addr+2));
    result.set_slc(8, ldb(addr+1));
    result.set_slc(0, ldb(addr));
    return result;
}

ac_int<32, false> doRead(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size)
{
    //printf("Doign read on file %x\n", file);
    int localSize = size.slc<32>(0);
    char* localBuffer = (char*) malloc(localSize*sizeof(char));
    ac_int<32, false> result;
    if (file == 0)
    {
        if (nbInStreams == 1)
            result = fread(localBuffer, 1, size, inStreams[0]);
        else
            result = fread(localBuffer, 1, size, stdin);
    }
    else
    {
        FILE* localFile = fileMap[file.slc<16>(0)];
        result = fread(localBuffer, 1, size, localFile);
        if (localFile == 0)
            return -1;
    }
    for (int i=0; i<result; i++)
    {
        stb(bufferAddr + i, localBuffer[i]);
    }
    return result;
}

ac_int<32, false> doWrite(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size)
{
    int localSize = size.slc<32>(0);
    char* localBuffer = (char*) malloc(localSize*sizeof(char));
    for (int i=0; i<size; i++)
        localBuffer[i] = ldb(bufferAddr + i);
    if (file < 5)
    {
        ac_int<32, false> result = 0;
        int streamNB = (int) file-nbInStreams;
        if (nbOutStreams + nbInStreams > file)
            result = fwrite(localBuffer, 1, size, outStreams[streamNB]);
        else
            result = fwrite(localBuffer, 1, size, stdout);
        return result;
    }
    else
    {
        FILE* localFile = fileMap[file.slc<16>(0)];
        if (localFile == 0)
            return -1;
        ac_int<32, false> result = fwrite(localBuffer, 1, size, localFile);
        return result;
    }
}

ac_int<32, false> doOpen(ac_int<32, false> path, ac_int<32, false> flags, ac_int<32, false> mode)
{
    int oneStringElement = ldb(path);
    int index = 0;
    while (oneStringElement != 0)
    {
        index++;
        oneStringElement = ldb(path+index);
    }
    int pathSize = index+1;
    char* localPath = (char*) malloc(pathSize*sizeof(char));
    for (int i=0; i<pathSize; i++)
        localPath[i] = ldb(path + i);
    const char* localMode;
    if (flags==0)
        localMode = "r";
    else if (flags == 577)
        localMode = "w";
    else if (flags == 1089)
        localMode = "a";
    else if (flags == O_WRONLY|O_CREAT|O_EXCL)
        localMode = "wx";
    else
    {
        fprintf(stderr, "Trying to open files with unknown flags... \n");
        exit(-1);
    }
    FILE* test = fopen(localPath, localMode);
    int result; //= (int) test;
    ac_int<32, true> result_ac = result;
    //For some reasons, newlib only store last 16 bits of this pointer, we will then compute a hash and return that.
    //The real pointer is stored here in a hashmap
    ac_int<32, true> returnedResult = 0;
    returnedResult.set_slc(0, result_ac.slc<15>(0) ^ result_ac.slc<15>(16));
    returnedResult[15] = 0;
    fileMap[returnedResult.slc<16>(0)] = test;
    return returnedResult;
}

ac_int<32, false> doOpenat(ac_int<32, false> dir, ac_int<32, false> path, ac_int<32, false> flags, ac_int<32, false> mode)
{
    fprintf(stderr, "Syscall openat not implemented yet...\n");
    exit(-1);
}

ac_int<32, false> doClose(ac_int<32, false> file)
{
    if (file > 2 )
    {
        FILE* localFile = fileMap[file.slc<16>(0)];
        int result = fclose(localFile);
        return result;
    }
    else
        return 0;
}

ac_int<32, true> doLseek(ac_int<32, false> file, ac_int<32, false> ptr, ac_int<32, false> dir)
{
    if (file>2)
    {
        FILE* localFile = fileMap[file.slc<16>(0)];
        if (localFile == 0)
            return -1;
        int result = fseek(localFile, ptr, dir);
        return result;
    }
    else
        return 0;
}

ac_int<32, false> doStat(ac_int<32, false> filename, ac_int<32, false> ptr)
{
    int oneStringElement = ldb(filename);
    int index = 0;
    while (oneStringElement != 0)
    {
        index++;
        oneStringElement = ldb(filename+index);
    }
    int pathSize = index+1;
    char* localPath = (char*) malloc(pathSize*sizeof(char));
    for (int i=0; i<pathSize; i++)
        localPath[i] = ldb(filename + i);
    struct stat fileStat;
    int result = stat(localPath, &fileStat);
    //We copy the result in simulator memory
    for (int oneChar = 0; oneChar<sizeof(struct stat); oneChar++)
        stb(ptr+oneChar, ((char*)(&stat))[oneChar]);
    return result;
}

ac_int<32, false> doSbrk(ac_int<32, false> value)
{
    return 0;
}

ac_int<32, false> doGettimeofday(ac_int<32, false> timeValPtr)
{
    timeval* oneTimeVal;
    struct timezone* oneTimeZone;
    int result = gettimeofday(oneTimeVal, oneTimeZone);
    return result;
}

ac_int<32, false> doUnlink(ac_int<32, false> path)
{
    int oneStringElement = ldb(path);
    int index = 0;
    while (oneStringElement != 0)
    {
        index++;
        oneStringElement = ldb(path+index);
    }
    int pathSize = index+1;
    char* localPath = (char*) malloc(pathSize*sizeof(char));
    for (int i=0; i<pathSize; i++)
        localPath[i] = ldb(path + i);
    int result = unlink(localPath);
    return result;
}

ac_int<32, false> solveSysCall(ac_int<32, false> syscallId, ac_int<32, false> arg1, ac_int<32, false> arg2,
                           ac_int<32, false> arg3, ac_int<32, false> arg4, ac_int<2, false> *sys_status)
{
    ac_int<32, false> result = 0;
    switch (syscallId)
    {
    case SYS_exit:
        *sys_status = 1; //Currently we break on ECALL
        break;
    case SYS_read:
        result = doRead(arg1, arg2, arg3);
        break;
    case SYS_write:
        result = doWrite(arg1, arg2, arg3);
        break;
    case SYS_brk:
        result = doSbrk(arg1);
        break;
    case SYS_open:
        result = doOpen(arg1, arg2, arg3);
        break;
    case SYS_openat:
        result = doOpenat(arg1, arg2, arg3, arg4);
        break;
    case SYS_lseek:
        result = doLseek(arg1, arg2, arg3);
        break;
    case SYS_close:
        result = doClose(arg1);
        break;
    case SYS_fstat:
        result = 0;
        break;
    case SYS_stat:
        result = doStat(arg1, arg2);
        break;
    case SYS_gettimeofday:
        result = doGettimeofday(arg1);
        break;
    case SYS_unlink:
        result = doUnlink(arg1);
        break;
    default:
        *sys_status = 2;
        break;
    }
    return result;
}
