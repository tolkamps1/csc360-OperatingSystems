#include <stdio.h>
#include <stdlib.h>


typedef struct INODE{ //52 bytes, 1 per block
    int size; // size of file in bytes; an integer
    int flags; // flags – i.e., type of file (flat or directory); an integer
    int block_nums[10]; // block numbers for file’s first ten blocks
    unsigned char sindirect; // single-indirect block
    unsigned char dindirect; // double-indirect block
}t_inode;


typedef struct DIRECTORY_ENTRY{
    unsigned char inodeID; // indicates the inode (value of 0 means no entry) (1-128)
    unsigned char filename[31]; // the filename, terminated with a “null” character.
}t_directory_entry;


typedef struct DIRECTORY{
    t_directory_entry directories[16]; //each contains 16 entries max.
}t_directory;


int InitLLFS();
void writeSegmentToLog();
void getInodeMap(FILE *disk);
void getFreeMap(FILE *disk);
int getNextFreeBlock(FILE *disk);
char getBlockBit(unsigned char buffer[], int index);
t_directory* tokenize_directory(unsigned char *buffer);
t_inode* tokenize_inode(unsigned char *buffer);
void writeFile(char* filename, char* update);
void readFile(char* filename);
void createFile(char* filename, char* contents, t_directory* direct, int dir_flag);
void writeDirectory(char* dir_name);
void DeleteFile(char* fname);
int check_set_block(unsigned char byte);

// Functions taken/adapted from Tutorial 10
void print_buffer(unsigned char* buffer, int size);
void init_buffer(unsigned char* buffer, int size);
void set_block(unsigned char* buffer, int block_num);
void unset_block(unsigned char* buffer, int block_num);

