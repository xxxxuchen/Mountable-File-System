# Mountable File System

## Design Decisions of On-Disk Structure
The overall on disk structure follows the typical linux file system design with some small modifications. 
- The block size is 1024 bytes.
- There is only one root directory, and no subdirectories.
- I decided to use free **byte** map (FBM) to keep track of free blocks in the disk instead of free bit map.
- Four blocks are allocated for FBM, which determines the max number of blocks in the disk to be 1024 * 4 = 4096 blocks. So, the capacity of the **whole file system** is 4096 * 1024 = 4MB.
- 1 block is allocated for super block.
- 6 block are allocated for inode table.
- Among the total 4096 blocks, 1 + 4 + 6 = 11 blocks are used for metadata, so the total number of data blocks is 4096 - 11 = 4085 blocks.
- The inode contains 12 direct block pointers, and only 1 indirect block pointer, which points to 1024 / 4 = 256 blocks. So, the max file size of a single file is (12 + 256) * 1024 = 274432 bytes = 256 KB.
- see source code sfs.c for more details.

## Design Decisions of In-Memory Structure
- There are four main data structures in memory: file descriptor table, root directory, inode table, and free block map.
- The inode table, root directory and free byte map are cached in memory when non fresh disk first initialized.
- Since only one process is allowed to access the file system at a time, The FDT combines the open file descriptor tables (the per-process one and system-wide one) in a UNIX-like operating system.
- see source code sfs.c for more details.

## Implementation Details
The source code is well commented, see sfs.c for more details.

## How to Run
- There is only one source file sfs.c, and one header file sfs.h.
- `make` to compile the program.

