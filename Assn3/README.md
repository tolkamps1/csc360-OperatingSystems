### Report on Little Log Filesystem
Shaelyn Tolkamp - V00875259


## Resources
Code adapted from Tutorials provided for CSC 360. Makefile adapted from Alex McRaes post to the forum. 

## Testing
While creating this file system, most of my initial testing to get things working was done manually using hexdump for vdisk, and printing the buffer in binary.

## Features/Functionality
Each directory can only have 16 entries. I added this limit to make this part simpler becuase then a directory does not an ID and to cover multiple blocks.
