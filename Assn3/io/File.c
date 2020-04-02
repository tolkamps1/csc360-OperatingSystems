#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "File.h"
#include "../disk/drivers.h"

#define vdisk_path "../disk/vdisk"
#define BLOCK_SIZE 512
#define NUM_BLOCKS 4096
#define NUM_INODES 128 // Blocks 2-6
#define ROOT_BLOCK 10 // Root directory starts block 10
#define TOKEN_DELIM '\0'


// Func declarations
int InitLLFS();
t_directory* tokenize_directory(unsigned char *buffer);
t_inode* tokenize_inode(unsigned char *buffer, int inode_num);
void writeFile(char* filename, char* update);
void readFile(char* filename);
void print_buffer(unsigned char* buffer, int size);
void init_buffer(unsigned char* buffer, int size);
void set_block(unsigned char* buffer, int block_num);
void unset_block(unsigned char* buffer, int block_num);
void print_binary(unsigned char byte);
unsigned char bitwise_or(unsigned char byte1, unsigned char byte2);




/**
 * Initialize virtual disk with file system info.
 * Create root directory
 */
int InitLLFS(){
    FILE* disk = fopen(vdisk_path, "w");
    if(disk){
        char* init = calloc(BLOCK_SIZE * NUM_BLOCKS, 1);
        fwrite(init, BLOCK_SIZE * NUM_BLOCKS, 1, disk);
        free(init);
        fclose(disk);

        disk = fopen(vdisk_path, "rb+"); //Open file for update (reading and writing).

        // Init block 0 - superblock
        unsigned char* buffer;
        buffer = (unsigned char*) malloc(BLOCK_SIZE);
        int magic = 42;
        int blocks = NUM_BLOCKS;
        int inodes = NUM_INODES;
        memcpy(buffer, &magic, sizeof(magic));
        memcpy(buffer + sizeof(int) * 1, &blocks, sizeof(int));
        memcpy(buffer + sizeof(int) * 2, &inodes, sizeof(int));
        //print_buffer(buffer, BLOCK_SIZE);
        writeBlock(disk, 0, (char*) buffer);
        free(buffer);

        // Init block 1 - free buffer
        // 0 is free, 1 is taken
        // set first 10 blocks (0-9) as taken
        // also set block 10 as taken for root directory
        unsigned char* free_buffer;
        free_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        init_buffer(free_buffer, BLOCK_SIZE);
        for(int k = 0; k < 11; k++){
            set_block(free_buffer, k);
        }
        //print_buffer(free_buffer, 512);
        writeBlock(disk, 1, (char*) free_buffer);
        free(free_buffer);


        // Init block 2 - First INode block 
        // first inode block for root directory
        unsigned char* inode_buffer;
        inode_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        //init_buffer(inode_buffer, BLOCK_SIZE);
        t_inode root;
        root.size = 512;
        root.flags = 1; // directory is 1
        memset(root.block_nums, 0, 10*sizeof(int));
        root.block_nums[0] = ROOT_BLOCK; // root dir is block 10
        printf("root size %d\n", root.size);
        printf("%ld\n",sizeof(root.size));
        memcpy(inode_buffer, &root.size, sizeof(root.size));
        memcpy(inode_buffer + sizeof(int) * 1, &root.flags, sizeof(int));
        memcpy(inode_buffer + sizeof(int) * 2, &root.block_nums, sizeof(int)*(10));

        printf("INODE BUFFER:\n");
        //print_buffer(inode_buffer, BLOCK_SIZE);
        writeBlock(disk, 2, (char*) inode_buffer);
        free(inode_buffer);

        // Init root directory with itself '.'
        unsigned char* dir_buffer;
        dir_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        t_directory_entry entry;
        memset(entry.filename, '\0', 31*sizeof(char));
        entry.inodeID = 1; // this inode
        entry.filename[0] = '.';
        entry.filename[1] = '\0';
        printf("root file name %s\n", entry.filename);
        memcpy(dir_buffer, &entry.inodeID, 1);
        memcpy(dir_buffer + 1, &entry.filename, 31);

        printf("ROOT DIR BUFFER:\n");
        //print_buffer(dir_buffer, BLOCK_SIZE);
        writeBlock(disk, 10, (char*) dir_buffer);
        free(dir_buffer);

        fclose(disk);
        return 1;
    }else{
        printf("Failed to open/create vdisk.\n");
        return -1;
    }
}


/**
 * Read file specified by name.
 * 1. Find filename and INodeid in root directory
 * 2. Find Inode in INode blocks to find block numbers for file
 * 3. Read blocks
*/
void readFile(char* filename){
    printf("Filename to read: %s\n", filename);
    FILE* disk = fopen(vdisk_path, "rb"); //Open file for reading.
    if(disk){
        unsigned char* buffer;
        buffer = (unsigned char*) malloc(BLOCK_SIZE);
        readBlock(disk, ROOT_BLOCK, (char *) buffer);
        //print_buffer(buffer, BLOCK_SIZE);
        //Get inode id from directory
        t_directory* direct = tokenize_directory(buffer);
        free(buffer);
        int inodeID;
        for(int k = 0; k <16; k++){
            unsigned char* fname = direct->directories[k].filename;
            if(strcmp((char*)fname, filename) == 0){
                printf("found entry\n");
                printf("Directory %s\n", fname);
                inodeID = direct->directories[k].inodeID;
                printf("inodeID: %d\n", inodeID);
                break;
            }
        }
        //Get file block nums from inode
        unsigned char* inode_buffer;
        inode_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        printf("Inode %d in block %d\n", (inodeID % 32), (inodeID / 32)+2);
        readBlock(disk,(inodeID / 32)+2, (char*)inode_buffer);
        printf("inode buffer\n");
        //print_buffer(inode_buffer, BLOCK_SIZE);
        t_inode* inode = tokenize_inode(inode_buffer, ((inodeID-1) % 32));
        free(inode_buffer);
        for(int k = 0; k < 10; k++){
            int blockID = inode->block_nums[k];
            // read each block
            if(blockID != 0){
                unsigned char* file_buffer;
                file_buffer = (unsigned char*) malloc(BLOCK_SIZE);
                readBlock(disk, blockID, (char *) buffer);
                printf("file buffer\n");
                print_buffer(file_buffer, BLOCK_SIZE);
                free(file_buffer);
            }
        }
        fclose(disk);
    }else{
        printf("Failed to open vdisk.\n");
    }
}

/**
 * Find filename and INodeid in root directory, create if does not exist
 * Find Inode in INode blocks to find block number for file
 * Append write to block
*/
void writeFile(char* filename, char* update){
    printf("Filename to write to: %s\n", filename);
    FILE* disk = fopen(vdisk_path, "rb+"); //Open file for update (reading and writing).
    if(disk){
        unsigned char* buffer;
        buffer = (unsigned char*) malloc(BLOCK_SIZE);
        readBlock(disk, ROOT_BLOCK, (char*) buffer);
        print_buffer(buffer, BLOCK_SIZE);
        
        t_directory* direct = tokenize_directory(buffer);
        printf("Directory %s\n", direct->directories[0].filename);

    
        free(direct);
        free(buffer);
        fclose(disk);
    }else{
        printf("Failed to open vdisk.\n");
    }
}

/**
 * Tokenize directory buffer
*/
t_directory* tokenize_directory(unsigned char *buffer){
    /*sunsigned char inode;
    unsigned char *fname;*/
    t_directory* directory = malloc(sizeof(t_directory));
    if (!directory) {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }
    for(int k = 0; k < 16; k++){
        /*
        inode = buffer[k * 32];
        fname = (unsigned char *)buffer[(k * 32)+1];
        printf("inode: %c", inode);
        printf("fname: %c", fname);
        */
        memcpy(&(directory->directories[k].inodeID), &buffer[k*32], 1);
        memcpy(&(directory->directories[k].filename), &buffer[(k*32)+1], 31);
    }
    return directory;
}


/**
 * Tokenize inode buffer
*/
t_inode* tokenize_inode(unsigned char *buffer, int inode_num){
    t_inode* inode = malloc(sizeof(t_inode));
    if (!inode) {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }
    memcpy(&inode->size, &buffer[inode_num*32], 4);
    memcpy(&inode->flags, &buffer[(inode_num*32)+4], 4);
    memcpy(&inode->block_nums, &buffer[(inode_num*32)+8], 2);
    memcpy(&inode->sindirect, &buffer[(inode_num*32)+10], 2);
    memcpy(&inode->dindirect, &buffer[(inode_num*32)+12], 2);

    return inode;
}


/**
 * Initialize free buffer
 */
void init_buffer(unsigned char buffer[], int size){
    for (int i = 0; i < size; i++) {
        buffer[i] = 0x0;
    }
}


/**
 * print buffer
 */
void print_buffer(unsigned char buffer[], int size){
    printf("%04x: ", 0);
    for (int i = 1; i <= size; i++) {
        printf("%02x ", buffer[i-1]);
        if(i % 8 == 0)
        {
            printf("\n");
            printf("%04x: ", i);
        }
    }
    printf("\n");
}


/**
 * Set block in buffer as not free
*/
//https://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit
void set_block(unsigned char buffer[], int block_num){
    int index = block_num / 8;
    int bit_index = (block_num % 8);
    
    printf("SET\n");
    printf("Index: %d\n", index);
    printf("Bit Index: %d\n", bit_index);
    buffer[index] |= 1UL << bit_index;
}

/**
 * Set block as free
 */
void unset_block(unsigned char buffer[], int block_num){
    int index = block_num / 8;
    int bit_index = (block_num % 8);
    
    printf("UNSET\n");
    printf("Index: %d\n", index);
    printf("Bit Index: %d\n", bit_index);
    buffer[index] &= ~(1UL << bit_index);
}



/**
 * Taken from tutorial 10
 */
void print_binary(unsigned char byte){
    int k;
    for (int i = 7; i >= 0; i--){
        k = byte >> i;
        if (k & 1)
            printf("1");
        else
            printf("0");
    }
    printf("\n");
}

/**
 * Taken from tutorial 10
 */
unsigned char bitwise_or(unsigned char byte1, unsigned char byte2){
    unsigned char byte3 = byte1 | byte2;
    return byte3;
}