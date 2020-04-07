## Little Log Filesystem
Shaelyn Tolkamp - V00875259


### Resources
Some code for binary operations adapted from Tutorials provided for CSC 360. Makefile adapted from Alex McRaes post to the forum. Discussion of concepts with Reed McIlwain.

### Testing
While creating this file system, most of my initial testing to get things working was done manually using hexdump and hexdump -C for vdisk, and printing the buffer in binary. I automated the calls to my functions in test01.c and eventually formatted print statements in my readFile() to print files and directories to stdout.

### Features/Functionality
Basic operations on a Log Filesystem: reading, writing, and creating files, creating directories. For full functionality and restrictions please see report.txt.
* Initialize the log file system with a superblock, free block map, and node map.
* Initialize log file with an empty root directory
* Create a file in root directory
* Create a subdirectory
* Write to a file in root directory
* Read directories and files in root directory
* Delete a file from the root directory
* Delete a subdirectory (and it’s contents) from the root directory

