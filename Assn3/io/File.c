#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "File.h"
#include "../disk/drivers.h"

#define vdisk_path "../disk/vdisk"
#define BLOCK_SIZE 512
#define NUM_BLOCKS 4096
#define NUM_INODES 128
#define TOKEN_DELIM '\0'

int* inode_map1; //two ints per inode (block #, pos in block) stored in block 2 (inodes 1-64)
int* inode_map2; // stored in block 3 (inodes 65-128)
unsigned char* segment; // 3 blocks
int segment_block; // block that segment will start on (first free block on vdisk)
unsigned char* free_map; // bit map
int backlog[NUM_INODES+1];// keeps track of what blocks have been changed but are not yet written to vdisk


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
        memset(buffer, 0, BLOCK_SIZE);
        int magic = 42;
        int blocks = NUM_BLOCKS;
        int inodes = NUM_INODES;
        memcpy(buffer, &magic, sizeof(magic));
        memcpy(buffer + sizeof(int) * 1, &blocks, sizeof(int));
        memcpy(buffer + sizeof(int) * 2, &inodes, sizeof(int));
        //print_buffer(buffer, BLOCK_SIZE);
        writeBlock(disk, 0, (char*) buffer);
        free(buffer);
        fclose(disk);


        // Init block 1 - free buffer
        // 0 is free, 1 is taken
        // set first 10 blocks (0-9) as taken and 10 and 11 for root dir
        free_map = (unsigned char*) malloc(BLOCK_SIZE);
        init_buffer(free_map, BLOCK_SIZE);
        for(int k = 0; k < 12; k++){
            set_block(free_map, k);
        }

        print_buffer(free_map, 512);
        

        // Create root directory
        // Init iNode  for root v
        printf("Create INode for root.\n");
        segment = (unsigned char*) malloc(BLOCK_SIZE*3);
        memset(segment, 0, BLOCK_SIZE*3);
        segment_block = 10;
        unsigned char* inode_buffer;
        inode_buffer = segment;
        t_inode root;
        root.size = 512;
        root.flags = 1; // directory is 1
        memset(root.block_nums, 0, 10*sizeof(int));
        root.block_nums[0] = 11; // root dir is first free block after it's inode
        memcpy(inode_buffer, &root.size, sizeof(root.size));
        memcpy(inode_buffer + sizeof(int) * 1, &root.flags, sizeof(int));
        memcpy(inode_buffer + sizeof(int) * 2, &root.block_nums, sizeof(root.block_nums));

        printf("Update inode mapping.\n");
        // Update iNode map
        inode_map1 = (int*) malloc(BLOCK_SIZE);
        memset(inode_map1, 0, BLOCK_SIZE);
        memset(inode_map1, 10, 1);
        memset(inode_map1+1, 1, 1);  //block number, num in block
        inode_map2 = (int*) malloc(BLOCK_SIZE);
        memset(inode_map2, 0, BLOCK_SIZE);



        //printf("INODE BUFFER:\n");
        //print_buffer(inode_buffer, BLOCK_SIZE);
        //writeBlock(disk, 2, (char*) inode_buffer);

        printf("Init root directory.\n");
        // Init root directory with itself '.'
        unsigned char* dir_buffer;
        dir_buffer = segment+BLOCK_SIZE;
        t_directory_entry entry;
        memset(entry.filename, '\0', 31*sizeof(char));
        entry.inodeID = 1; // this inode
        backlog[1] = 1;
        entry.filename[0] = '.';
        entry.filename[1] = '\0';
        printf("root file name %s\n", entry.filename);
        memcpy(dir_buffer, &entry.inodeID, 1);
        memcpy(dir_buffer + 1, &entry.filename, 31);
        //print_buffer(dir_buffer, BLOCK_SIZE);
        //writeBlock(disk, 10, (char*) dir_buffer);

        printf("Write seg to log.\n");
        writeSegmentToLog();
        return 1;
    }else{
        printf("Failed to open/create vdisk.\n");
        return -1;
    }
}


/**
 * Write segment, and update inode map and free map
*/
void writeSegmentToLog(){
    FILE * disk = fopen(vdisk_path, "rb+");
    if(disk){
        // append segment to log
        for(int k = 0; k < 3; k++){
            writeBlock(disk, segment_block+k, (char*) segment+(BLOCK_SIZE*k));
            printf("wrtoe seg %d\n", k);
        }
        free(segment);

        // update inode mapping
        writeBlock(disk, 2, (char*) inode_map1);
        printf("wrtoe inode map 1\n");

        writeBlock(disk, 3, (char*) inode_map2);
        printf("wrote indoe map 2\n");


        // update free list
        writeBlock(disk, 1, (char*) free_map);
        printf("wrote free list\n");

        //clear backlog list
        memset(&backlog, 0, sizeof(int)*(NUM_INODES+1));
        printf("cleared backlog.\n");

        fclose(disk);
    }else{
        printf("Failed to write to disk, could not open %s", vdisk_path);
    }

}


/**
 * Read file specified by name.
 * 1. Find filename and INodeid in root directory
 * 2. Check if file was updated in backlog - force write before continuing
 * 3. Find Inode in INode blocks to find block numbers for file
 * 4. Read blocks
 * Asumption is that this is called in root directory
*/
void readFile(char* filename){
    printf("Filename to read: %s\n", filename);
    FILE* disk = fopen(vdisk_path, "rb"); //Open file for reading.
    if(disk){
        unsigned char* buffer;
        buffer = (unsigned char*) malloc(BLOCK_SIZE);
        readBlock(disk, 11, (char *) buffer);
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
        // Check inode ID first in backlog to see if writeToLog is necessary first
        if(backlog[inodeID] != 0){
            writeSegmentToLog();
        }
        printf("sleep 1.\n");
        sleep(1);
        // Look up inode block from inode map
        int inode_block;
        int pos_in_block;
        if(inodeID <= 64){
            inode_block = inode_map1[(inodeID-1)*2];
            pos_in_block = inode_map1[1+((inodeID-1)*2)];
        }else{
            inode_block = inode_map2[(inodeID-1)*2];
            pos_in_block = inode_map2[1+((inodeID-1)*2)];
        }
        printf("sleep 2.\n");
        sleep(1);

        //Get file block nums from inode
        unsigned char* inode_buffer;
        inode_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        printf("Inode %d in block %d\n", (pos_in_block), inode_block);
        readBlock(disk, inode_block, (char*)inode_buffer);
        printf("inode buffer\n");
        print_buffer(inode_buffer, BLOCK_SIZE);
        t_inode* inode = tokenize_inode(inode_buffer, (pos_in_block-1));
        free(inode_buffer);
        printf("INODE size %d\n", inode->size);
        printf("INODE flags %d\n", inode->flags);
        printf("INODE block_nums %d\n", inode->block_nums[0]);
        for(int k = 0; k < 10; k++){
            printf("%d ", inode->block_nums[k]);
        }

        printf("sleep 3.\n");
        sleep(1);
        for(int k = 0; k < 10; k++){
            int fileblockID = 0;
            fileblockID = inode->block_nums[k];
            // read each block
            if(fileblockID != 0){
                unsigned char* file_buffer;
                file_buffer = (unsigned char*) malloc(BLOCK_SIZE);
                readBlock(disk, fileblockID, (char *) file_buffer);
                printf("file buffer\n");
                print_buffer(file_buffer, BLOCK_SIZE);
                free(file_buffer);
            }
        }
        fclose(disk);
        printf("sleep 4\n");
        sleep(1);
    }else{
        printf("Failed to open vdisk.\n");
    }
}


/*
 * Gets block from inode map in disk
 * */
void getInodeMap(FILE *disk){
    readBlock(disk, 2, (char *) inode_map1);
    readBlock(disk, 3, (char *) inode_map2);

}


/*
 * Gets updated free map from disk
 * */
void getFreeMap(FILE *disk){
    readBlock(disk, 1, (char *) free_map);
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
        readBlock(disk, 11, (char*) buffer);
        print_buffer(buffer, BLOCK_SIZE);
        
        t_directory* direct = tokenize_directory(buffer);
        free(buffer);
        int inodeID = 0;
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
        // File does not exist
        if(inodeID == 0){
            printf("File does not yet exist.\n");
            // create file
            createFile(filename, direct);
        }
        else{
            // look up file and write to it.
        }

        free(direct);
        fclose(disk);
    }else{
        printf("Failed to open vdisk.\n");
    }
}



/**
 * Create a new file in current directory (root)
*/
void createFile(char* filename, t_directory* direct){
    FILE* disk = fopen(vdisk_path, "rb+"); //Open file for update (reading and writing).
    if(disk){
        //create inode
        unsigned char* inode_buffer;
        inode_buffer = segment;
        t_inode newfile;
        newfile.size = 512;
        newfile.flags = 0; // not a directory
        memset(newfile.block_nums, 0, 10*sizeof(int));
        memcpy(inode_buffer, &newfile.size, sizeof(newfile.size));
        memcpy(inode_buffer + sizeof(int) * 1, &newfile.flags, sizeof(int));
        memcpy(inode_buffer + sizeof(int) * 2, &newfile.block_nums, sizeof(int)*(10));

        // Update iNode map
        inode_map1 = (int*) malloc(BLOCK_SIZE);
        getInodeMap(disk);
        //get next free block from free map

        //inode_map[1] = 10;


        for(int k = 0; k <16; k++){
            if(direct->directories[k].inodeID == 0);
            // put new entry here

            break;
        }
        // create first file block

    }
    else{
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
    memset(&inode->block_nums, 0, 10*sizeof(int));
    memcpy(&inode->size, &buffer[inode_num*32], 4);
    memcpy(&inode->flags, &buffer[(inode_num*32)+4], 4);
    memcpy(&inode->block_nums, &buffer[(inode_num*32)+8], 20);
    memcpy(&inode->sindirect, &buffer[(inode_num*32)+10], 2);
    memcpy(&inode->dindirect, &buffer[(inode_num*32)+12], 2);

    return inode;
}


/**
 * Initialize a buffer to zeros
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
 * print binary
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
 * bitwise or
 * Taken from tutorial 10
 */
unsigned char bitwise_or(unsigned char byte1, unsigned char byte2){
    unsigned char byte3 = byte1 | byte2;
    return byte3;
}