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

/*int* inode_map1; //two ints per inode (block #, pos in block) stored in block 2 (inodes 1-64)
int* inode_map2; // stored in block 3 (inodes 65-128)*/
unsigned char* inode_map;
unsigned char* segment; // buffer of segment_size*BLOCK_SIZE
int segment_size; // usually 5 blocks
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
        //print_buffer(free_map, 512);
        

        // Create root directory
        // Init iNode  for root v
        //printf("Create INode for root.\n");
        segment_size = 5;
        segment = (unsigned char*) malloc(BLOCK_SIZE*segment_size);
        memset(segment, 0, BLOCK_SIZE*segment_size);
        segment_block = 10;
        unsigned char* inode_buffer;
        inode_buffer = segment;
        t_inode root;
        root.size = 32;
        root.flags = 1; // directory is 1
        memset(root.block_nums, 0, 10*sizeof(int));
        root.block_nums[0] = 11; // root dir is first free block after it's inode
        memcpy(inode_buffer, &root.size, sizeof(root.size));
        memcpy(inode_buffer + sizeof(int) * 1, &root.flags, sizeof(int));
        memcpy(inode_buffer + sizeof(int) * 2, &root.block_nums, sizeof(root.block_nums));

        //printf("Update inode mapping.\n");
        // Update iNode map
        inode_map = (unsigned char*) malloc(BLOCK_SIZE);
        memset(inode_map, 0, BLOCK_SIZE);
        memset(inode_map, 10, sizeof(char));


        //printf("Init root directory.\n");
        // Init root directory with itself '.'
        //unsigned char* dir_buffer = InitDir();
        unsigned char* dir_buffer;
        dir_buffer = segment+BLOCK_SIZE;
        t_directory_entry entry;
        memset(entry.filename, '\0', 31*sizeof(char));
        entry.inodeID = 1; // this inode
        backlog[1] = 1;
        entry.filename[0] = '.';
        //printf("root file name %s\n", entry.filename);
        memcpy(dir_buffer, &entry.inodeID, 1);
        memcpy(dir_buffer + 1, &entry.filename, 31);

        //printf("Write seg to log.\n");
        writeSegmentToLog();
        return 1;
    }else{
        printf("LLFS: Failed to open/create vdisk.\n");
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
        for(int k = 0; k < segment_size; k++){
            writeBlock(disk, segment_block+k, (char*) segment+(BLOCK_SIZE*k));
            //printf("wrote seg %d\n", k);
        }
        free(segment);

        // update inode mapping
        writeBlock(disk, 2, (char*) inode_map);
        free(inode_map);
        //printf("wrote inode map.\n");

        // update free list
        writeBlock(disk, 1, (char*) free_map);
        free(free_map);
        //printf("wrote free list\n");

        //clear backlog list
        memset(&backlog, 0, sizeof(int)*(NUM_INODES+1));
        //printf("cleared backlog.\n");

        fclose(disk);
    }else{
        printf("LLFS: Failed to write to disk, could not open %s", vdisk_path);
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
    //printf("Filename to read: %s\n", filename);
    FILE* disk = fopen(vdisk_path, "rb"); //Open file for reading.
    if(disk){
        unsigned char* buffer;
        buffer = (unsigned char*) malloc(BLOCK_SIZE);
        //get root dir block
        getInodeMap(disk);
        int root_inode = inode_map[0];
        unsigned char* i_buffer;
        i_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        readBlock(disk, root_inode, (char *) i_buffer);
        t_inode* rooti = tokenize_inode(i_buffer);
        free(i_buffer);
        int root_block = rooti->block_nums[0];


        //printf("Block num for root %d\n", root_block);
        readBlock(disk, root_block, (char *) buffer);
        //print_buffer(buffer, BLOCK_SIZE);
        //Get inode id from directory
        t_directory* direct = tokenize_directory(buffer);
        free(buffer);
        int inodeID = 0;
        for(int k = 0; k < 16; k++){
            unsigned char* fname = direct->directories[k].filename;
            if(strcmp((char*)fname, filename) == 0){
                //printf("found entry-> %s\n", fname);
                inodeID = direct->directories[k].inodeID;
                //printf("inodeID: %d\n", inodeID);
                break;
            }
        }
        if(inodeID == 0){
            printf("LLFS: File name %s not found.\n", filename);
            free(direct);
            fclose(disk);
            return;

        }
        // Check inode ID first in backlog to see if writeSegToLog is necessary first
        if(backlog[inodeID] != 0){
            //printf("FOUnd in backlog!!!!\n");
            writeSegmentToLog();
            getInodeMap(disk);
        }

        // Look up inode block from inode map
        //print_buffer(inode_map, BLOCK_SIZE);
        int inode_block = inode_map[inodeID-1];

        //Get file block nums from inode
        unsigned char* inode_buffer;
        inode_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        readBlock(disk, inode_block, (char*) inode_buffer);
        //printf("inode buffer:\n");
        //print_buffer(inode_buffer, BLOCK_SIZE);
        t_inode* inode = tokenize_inode(inode_buffer);
        free(inode_buffer);
        /*printf("INODE size %d\n", inode->size);
        printf("INODE flags %d\n", inode->flags);
        printf("INODE block_nums %d\n", inode->block_nums[0]);
        for(int k = 0; k < 10; k++){
            printf("%d ", inode->block_nums[k]);
        }*/
        int dir_flag = inode->flags;
        if(dir_flag == 1){ // if dir
            printf("LLFS: Directory %s\n", filename);
            printf("LLFS: INodeID     Filename\n");
        }else{
            printf("LLFS: File %s\n", filename);
        }
        // read each block
        for(int k = 0; k < 10; k++){
            int fileblockID = 0;
            fileblockID = inode->block_nums[k];
            if(fileblockID != 0){
                unsigned char* file_buffer;
                file_buffer = (unsigned char*) malloc(BLOCK_SIZE);
                readBlock(disk, fileblockID, (char *) file_buffer);
                //Print it out to stdout
                if(dir_flag == 1){ // if dir
                    for (int i = 1; i <= (inode->size); i++) {
                        if(i%32 == 1){
                            printf("      %-12d", file_buffer[i-1]);
                        }else{
                            printf("%c", file_buffer[i-1]);
                            if(i % 32 == 0){
                                printf("\n");
                            }
                        }
                    }
                    printf("\n");
                }
                else{ // if file
                    for (int i = 1; i <= BLOCK_SIZE; i++) {
                        printf("%c", file_buffer[i-1]);
                    }
                    printf("\n");
                }
                //printf("file buffer in block %d\n", fileblockID);
                //print_buffer(file_buffer, BLOCK_SIZE);
                free(file_buffer);
            }
        }
        free(inode_map);
        fclose(disk);
    }else{
        printf("LLFS: Failed to open vdisk.\n");
    }
}


/*
 * Gets block from inode map in disk
 * */
void getInodeMap(FILE *disk){
    inode_map = (unsigned char*) malloc(BLOCK_SIZE);
    readBlock(disk, 2, (char *) inode_map);
}


/*
 * Gets updated free map from disk
 */
void getFreeMap(FILE *disk){
    free_map = (unsigned char*) malloc(BLOCK_SIZE);
    readBlock(disk, 1, (char *) free_map);
}


/*
 * Gets updated free map from disk
 */
int getNextFreeBlock(FILE *disk){
    for(int k = 0; k < 512; k++){
        char x = getBlockBit(free_map, k);
        //printf("Block bit found is %d\n", x);
        if(!(x & 1)){
            segment_block = k;
            return segment_block;
        }
    }
    segment_block = 0;
    return 0;
}

char getBlockBit(unsigned char buffer[], int index){
    return 1 & (buffer[index / 8] >> (index % 8));
}


/*
 * Gets next nodeid in inodemap
 */
int getNextINodeID(FILE *disk){
    for(int k = 0; k < NUM_INODES; k++){
        if(inode_map[k] == 0){
            return (k+1);
        }
    }
    return 0;
}



/**
 * Find filename and INodeid in root directory, create if does not exist
 * Find Inode in INode blocks to find block number for file
 * Append write to block and create new block to add to log
*/
void writeFile(char* filename, char* update){
    //printf("Filename to write to: %s\n", filename);
    FILE* disk = fopen(vdisk_path, "rb+"); //Open file for update (reading and writing).
    if(disk){
        unsigned char* buffer;
        buffer = (unsigned char*) malloc(BLOCK_SIZE);
        getInodeMap(disk);
        int root_inode = inode_map[0];
        //printf("Root inode: %d\n", root_inode);
        unsigned char* i_buffer;
        i_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        readBlock(disk, root_inode, (char *) i_buffer);
        t_inode* rooti = tokenize_inode(i_buffer);
        free(i_buffer);
        int root_block = rooti->block_nums[0];
        //printf("Root block: %d\n", root_block);

        readBlock(disk, root_block, (char*) buffer);
        //print_buffer(buffer, BLOCK_SIZE);
        // look up file
        t_directory* direct = tokenize_directory(buffer);
        free(buffer);
        int inodeID = 0;
        for(int k = 0; k <16; k++){
            unsigned char* fname = direct->directories[k].filename;
            if(strcmp((char*)fname, filename) == 0){
                //printf("found entry\n");
                //printf("Directory %s\n", fname);
                inodeID = direct->directories[k].inodeID;
                //printf("inodeID: %d\n", inodeID);
                break;
            }
        }
        // File does not exist
        if(inodeID == 0){
            //printf("File does not yet exist.\n");
            // create file
            fclose(disk);
            createFile(filename, update, direct, 0);
        }

        else{
            /**
             * Create a new file in directory (currently must be root)
             * 1. create inode
             * 2. update inode map with new location and new blocks locations and free map
             * 3. create file blocks with contents
            */
            // look up file
            getInodeMap(disk);
            int inode_block = inode_map[inodeID-1];
            //printf("INode of file to write to %d\n", inode_block);
            i_buffer = (unsigned char*) malloc(BLOCK_SIZE);
            readBlock(disk, inode_block, (char *) i_buffer);
            t_inode* filei = tokenize_inode(i_buffer);
            free(i_buffer);

            // find last block 
            int full_blocks = 0;
            int file_size = filei->size;
            //printf("file size %d\n", file_size);
            full_blocks = file_size / BLOCK_SIZE;
            //printf("!!!!! Number of full blocks: %d\n", full_blocks);
            int file_block = filei->block_nums[full_blocks];
            //printf("last file block %d\n", file_block);

            // compute # blocks needed
            int update_size = strlen(update);
            //printf("Update size: %d\n", update_size);
            int extra = BLOCK_SIZE - (file_size%BLOCK_SIZE);
            //printf("extra room in file block: %d\n", extra);

            if((update_size+file_size) > 10*BLOCK_SIZE){
                printf("LLFS: File update too big remain in <= 10 blocks.\n");
                return;
            }
            int blocks_needed = 0;
            for(int remaining = (update_size-extra); remaining > 0; remaining -= BLOCK_SIZE){
                blocks_needed ++;
            }
            //get files last blocks current contents
            unsigned char* file_buffer = malloc(BLOCK_SIZE*(blocks_needed+1));
            memset(file_buffer, 0, BLOCK_SIZE*(blocks_needed+1));
            //get files last blocks current contents
            readBlock(disk, file_block, (char*) file_buffer);
            // append new contents to file buuffer
            memcpy(&file_buffer[file_size%BLOCK_SIZE], update, strlen(update));
            //printf("new file buffer with all blocks: \n");
            //print_buffer(file_buffer, BLOCK_SIZE*(blocks_needed+1));


            ////printf("Additional blocks needed for this update %d\n", blocks_needed);
            segment_size = blocks_needed+2; // additional blocks + 1 for new inode and +1 for block appended
            //printf("Segment size for this update %d\n", segment_size);

            segment = (unsigned char*) malloc(BLOCK_SIZE*segment_size);
            memset(segment, 0, BLOCK_SIZE*segment_size);
            
            //get next free block from free map
            getFreeMap(disk);
            int free_block = getNextFreeBlock(disk);
            //printf("Free block of New Inode for updated file: %d\n", free_block);

            //create new inode for file
            unsigned char* inode_buffer;
            inode_buffer = segment;
            t_inode* newfile = filei;
            newfile->size = update_size+file_size;
            // update inode file blocks
            int file_blocks[10];
            memset(&file_blocks, 0, sizeof(int)*10);
            /*printf("New block list: \n");
            for(int k = 0; k < 10; k++){
                printf("%d ", file_blocks[k]);
            }
            printf("\n");*/
            int count = 1;
            for(int k = 0; k <= (blocks_needed+full_blocks); k++){
                if(k < full_blocks)
                memcpy(&file_blocks[k], &filei->block_nums[k], sizeof(int));
                else {
                    /*printf("k: %d\n", k);
                    printf("free_block: %d\n", free_block);
                    printf("count: %d\n", count);*/

                    file_blocks[k] = free_block+count;
                    count ++;
                }
            }
            /*printf("New block list: \n");
            for(int k = 0; k < 10; k++){
                printf("%d ", file_blocks[k]);
            }
            printf("\n");*/
            memcpy(inode_buffer, &newfile->size, sizeof(int));
            memcpy(inode_buffer + sizeof(int) * 1, &newfile->flags, sizeof(int));
            memcpy(inode_buffer + sizeof(int) * 2, &file_blocks, sizeof(int)*10);

            // Add file blocks to seg
            memcpy(segment+BLOCK_SIZE, file_buffer, BLOCK_SIZE*(blocks_needed+1));

            // Update inode map and free map
            inode_map[inodeID-1] = free_block;
            for(int k = 1; k <= (blocks_needed+1); k++){
                set_block(free_map, free_block+k); // for files blocks
            }
            set_block(free_map, free_block); // for file's inode

            fclose(disk);

            writeSegmentToLog();
        }

        free(direct);
    }else{
        printf("LLFS: Failed to open vdisk.\n");
    }
}



/**
 * Create a new file in directory (currently must be root)
 * 1. create inode
 * 2. update inode map and free map
 * 3. create file with contents
 * 4. add new file to current directory
 * 5. update current dir block to new block and append to segment
*/
void createFile(char* fname, char* contents, t_directory* direct, int dir_flag){
    FILE* disk = fopen(vdisk_path, "rb+"); //Open file for update (reading and writing).
    if(disk){

        // compute # blocks needed
        int update_size = strlen(contents);
        //printf("Update size: %d\n", update_size);

        if((update_size) > 10*BLOCK_SIZE){
            printf("LLFS: File update too big remain in <= 10 blocks. Must be < %d bytes.\n", (10*BLOCK_SIZE));
            return;
        }
        int blocks_needed = 0;
        for(int remaining = (update_size-BLOCK_SIZE); remaining > 0; remaining -= BLOCK_SIZE){
            blocks_needed ++;
        }
        //printf("Additional blocks needed for this update %d\n", blocks_needed);

        segment_size = blocks_needed+4; // file inode, dir, dir inode, file first block
        segment = (unsigned char*) malloc(BLOCK_SIZE*segment_size);
        memset(segment, 0, BLOCK_SIZE*segment_size);
        // Get iNode map
        int inodeID = getNextINodeID(disk);
        //printf("New Inode ID: %d\n", inodeID);
        //get next free block from free map
        getFreeMap(disk);
        int free_block = getNextFreeBlock(disk);
        //printf("Free block: %d\n", free_block);
        //create inode for file
        // update inode file blocks
        int file_blocks[10];
        memset(&file_blocks, 0, sizeof(int)*10);
        /*printf("New block list: \n");
        for(int k = 0; k < 10; k++){
            printf("%d ", file_blocks[k]);
        }
        printf("\n");*/
        int count = 1;
        for(int k = 0; k <= (blocks_needed); k++){
            file_blocks[k] = free_block+count;
            count ++;
        }
        /*printf("New block list: \n");
        for(int k = 0; k < 10; k++){
            printf("%d ", file_blocks[k]);
        }
        printf("\n");*/

        unsigned char* inode_buffer;
        inode_buffer = segment;
        t_inode filei;
        filei.size = strlen(contents);
        filei.flags = dir_flag;
        memcpy(inode_buffer, &filei.size, sizeof(int));
        memcpy(inode_buffer + sizeof(int) * 1, &filei.flags, sizeof(int));
        memcpy(inode_buffer + sizeof(int) * 2, &file_blocks, sizeof(int)*10);
        
        // Create file blocks
        unsigned char* file_buffer;
        file_buffer = segment+(BLOCK_SIZE*(blocks_needed+1));
        memset(file_buffer, 0, BLOCK_SIZE*(blocks_needed+1));
        memcpy(file_buffer, contents, strlen(contents));
        
        // Update inode map and free map
        inode_map[inodeID-1] = free_block;
        for(int k = 1; k <= (blocks_needed+1); k++){
            set_block(free_map, free_block+k); // for files blocks
        }
        set_block(free_map, free_block); // for file's inode


        int dirInode;
        dirInode = direct->directories[0].inodeID;
        // inode to update for dir
        // Create new entry in current directory
        int fn_length = 30;
        if(strlen(fname) < fn_length) fn_length = strlen(fname);
        int k;
        for(k = 0; k <16; k++){
            if(direct->directories[k].inodeID == 0){
                direct->directories[k].inodeID = inodeID;
                memset(&direct->directories[k].filename, '\0', 31);
                memcpy(direct->directories[k].filename, fname, fn_length);
                break;
            }else if(k == 15){
                printf("LLFS: Directory full. Cannot create new entry.\n");
                return;
            }
        }
        // add new dir to seg
        unsigned char* dir_buffer;
        dir_buffer = segment+(BLOCK_SIZE*(blocks_needed+3));
        memset(dir_buffer, 0, BLOCK_SIZE);
        memcpy(dir_buffer, &direct->directories, BLOCK_SIZE);
        //printf("directory buffer\n");
        //print_buffer(dir_buffer, BLOCK_SIZE);
        // update dir inode
        unsigned char* dirinode_buffer;
        dirinode_buffer = segment+(BLOCK_SIZE*(blocks_needed+2));
        t_inode rooti;
        rooti.size = 32*(k+1);
        rooti.flags = 1; // directory is 1
        memset(rooti.block_nums, 0, 10*sizeof(int));
        rooti.block_nums[0] = (free_block+3);
        memcpy(dirinode_buffer, &rooti.size, sizeof(rooti.size));
        memcpy(dirinode_buffer + sizeof(int) * 1, &rooti.flags, sizeof(int));
        memcpy(dirinode_buffer + sizeof(int) * 2, &rooti.block_nums, 10*sizeof(int));

        // Update inode map and free map with new dir info
        //printf("*** %d\n", dirInode);
        inode_map[dirInode-1] = (free_block+blocks_needed+2);
        set_block(free_map, free_block+blocks_needed+2); // for dir's inode
        set_block(free_map, free_block+blocks_needed+3); // for dir

        fclose(disk);
        writeSegmentToLog();
    }
    else{
        printf("LLFS: Failed to open vdisk.\n");
    }
}


/**
 * Find root block in INode blocks
 * create new direcotry with initializer of '.' (itself)
 * call create file with contents of new directory
*/
void writeDirectory(char* dir_name){
    //printf("Subdirectory to create: %s\n", dir_name);
    FILE* disk = fopen(vdisk_path, "rb+"); //Open file for update (reading and writing).
    if(disk){
        unsigned char* buffer;
        buffer = (unsigned char*) malloc(BLOCK_SIZE);
        getInodeMap(disk);
        int root_inode = inode_map[0];
        //printf("Root inode: %d\n", root_inode);
        unsigned char* i_buffer;
        i_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        readBlock(disk, root_inode, (char *) i_buffer);
        t_inode* rooti = tokenize_inode(i_buffer);
        free(i_buffer);
        int root_block = rooti->block_nums[0];
        //printf("Root block: %d\n", root_block);

        readBlock(disk, root_block, (char*) buffer);
        //print_buffer(buffer, BLOCK_SIZE);
        // look up file
        t_directory* direct = tokenize_directory(buffer);
        free(buffer);
        int entry_line = 0;
        for(int k = 0; k <16; k++){
            if(direct->directories[k].inodeID == 0){
                //printf("found next blank\n");
                entry_line = k;
                break;
            }
        }
        // File does not exist
        if(entry_line == 0){
            printf("LLFS: Directory Full. Cannot add new entry.\n");
            fclose(disk);
            return;
        }
        else{
            // Init directory file with itself '.'
            unsigned char* dir_buffer;
            dir_buffer = malloc(BLOCK_SIZE);
            t_directory_entry entry;
            memset(entry.filename, '\0', 31*sizeof(char));
            entry.inodeID = getNextINodeID(disk);
            entry.filename[0] = '.';
            memcpy(dir_buffer, &entry.inodeID, 1);
            memcpy(dir_buffer + 1, &entry.filename, 31);
            createFile(dir_name, (char*) dir_buffer, direct, 1);
        }

        free(direct);
    }else{
        printf("LLFS: Failed to open vdisk.\n");
    }
}



/**
 * Find root block in INode blocks
 * find file in direcotry
 * edit dir to remove entry
 * update dir inode
 * update inode map 
*/
void DeleteFile(char* fname){
    //printf("File to delete : %s\n", fname);
    if(strcmp(fname, ".") == 0){
        printf("LLFS: Cannot delete the root dir: %s", fname);
        return;
    }
    FILE* disk = fopen(vdisk_path, "rb+"); //Open file for update (reading and writing).
    if(disk){
        unsigned char* buffer;
        buffer = (unsigned char*) malloc(BLOCK_SIZE);
        getInodeMap(disk);
        int root_inode = inode_map[0];
        //printf("Root inode: %d\n", root_inode);
        unsigned char* i_buffer;
        i_buffer = (unsigned char*) malloc(BLOCK_SIZE);
        readBlock(disk, root_inode, (char *) i_buffer);
        t_inode* rooti = tokenize_inode(i_buffer);
        free(i_buffer);
        int root_block = rooti->block_nums[0];
        //printf("Root block: %d\n", root_block);

        readBlock(disk, root_block, (char*) buffer);
        // look up file
        t_directory* direct = tokenize_directory(buffer);
        free(buffer);
        int inodeID = 0;
        int k;
        for(k = 0; k <16; k++){
            unsigned char* filename = direct->directories[k].filename;
            if(strcmp(fname, (char*)filename) == 0){
                //printf("found entry\n");
                //printf("Directory %s\n", filename);
                inodeID = direct->directories[k].inodeID;
                //printf("inodeID: %d\n", inodeID);
                break;
            }
        }
        // File does not exist
        if(inodeID == 0){
            printf("LLFS: Cannot delete file. %s does not exist.\n", fname);
            free(direct);
            fclose(disk);
            return;
        }
        else{
            // Remove entry from root directory
            
            segment_size = 2; // new root inode new root file block
            //printf("Segment size for this update %d\n", segment_size);

            segment = (unsigned char*) malloc(BLOCK_SIZE*segment_size);
            memset(segment, 0, BLOCK_SIZE*segment_size);
            
            //get next free block from free map
            getFreeMap(disk);
            int free_block = getNextFreeBlock(disk);
            //printf("Free block of New Inode for updated file: %d\n", free_block);

            // Update inode map to remove file
            inode_map[inodeID-1] = 0;

            int dirInode;
            dirInode = direct->directories[0].inodeID;
            // inode to update for dir
            // Remove entry in current directory
            direct->directories[k].inodeID = 0;
            memset(&direct->directories[k].filename, '\0', 31*sizeof(char));
            
            // add new dir to seg
            unsigned char* dir_buffer;
            dir_buffer = segment+(BLOCK_SIZE);
            memset(dir_buffer, 0, BLOCK_SIZE);
            memcpy(dir_buffer, &direct->directories, BLOCK_SIZE);
            //printf("directory buffer\n");
            //print_buffer(dir_buffer, BLOCK_SIZE);
            // update dir inode
            unsigned char* dirinode_buffer;
            dirinode_buffer = segment;
            t_inode rooti;
            rooti.size = 512;
            rooti.flags = 1; // directory is 1
            memset(rooti.block_nums, 0, 10*sizeof(int));
            rooti.block_nums[0] = (free_block+1);
            memcpy(dirinode_buffer, &rooti.size, sizeof(rooti.size));
            memcpy(dirinode_buffer + sizeof(int) * 1, &rooti.flags, sizeof(int));
            memcpy(dirinode_buffer + sizeof(int) * 2, &rooti.block_nums, 10*sizeof(int));

            // Update inode map and free map with new dir info
            //printf("*** %d\n", dirInode);
            inode_map[dirInode-1] = (free_block);
            set_block(free_map, free_block); // for dir's inode
            set_block(free_map, free_block+1); // for dir

            fclose(disk);
            writeSegmentToLog();
        }

        free(direct);
    }else{
        printf("LLFS: Failed to open vdisk.\n");
    }
}




/**
 * Tokenize directory buffer
*/
t_directory* tokenize_directory(unsigned char *buffer){
    t_directory* directory = malloc(sizeof(t_directory));
    if (!directory) {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }
    for(int k = 0; k < 16; k++){
        memcpy(&(directory->directories[k].inodeID), &buffer[k*32], 1);
        memcpy(&(directory->directories[k].filename), &buffer[(k*32)+1], 31);
    }
    return directory;
}


/**
 * Tokenize inode buffer
*/
t_inode* tokenize_inode(unsigned char *buffer){
    t_inode* inode = malloc(sizeof(t_inode));
    if (!inode) {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }
    memset(&inode->block_nums, 0, 10*sizeof(int));
    memcpy(&inode->size, &buffer[0], sizeof(int));
    memcpy(&inode->flags, &buffer[4], sizeof(int));
    memcpy(&inode->block_nums, &buffer[8], 10*sizeof(int));
    memcpy(&inode->sindirect, &buffer[48], sizeof(char));
    memcpy(&inode->dindirect, &buffer[50], sizeof(char));

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
 * print a buffer
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


/* Checks to see if bit in byte is set
*/
int check_set_block(unsigned char byte){
    int k;
    for (int i = 7; i >= 0; i--){
        k = byte >> i;
        if (k & 1)
            return i;
    }
    return 0;
    
}


/**
 * Set block in buffer as not free
 * Taken from tutorial 10
*/
//https://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit
void set_block(unsigned char buffer[], int block_num){
    int index = block_num / 8;
    int bit_index = (block_num % 8);
    buffer[index] |= 1UL << bit_index;
}

/**
 * Set block as free
 * Taken from tutorial 10
 */
void unset_block(unsigned char buffer[], int block_num){
    int index = block_num / 8;
    int bit_index = (block_num % 8);
    buffer[index] &= ~(1UL << bit_index);
}