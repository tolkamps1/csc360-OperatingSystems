#include <stdio.h>
#include <stdlib.h>

void readBlock(FILE * disk, int blockNum, char* buffer);

void writeBlock(FILE* disk, int blockNum, char* data);