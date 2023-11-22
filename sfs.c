/**
 * File System Implementation
 * root directory starts at the first block of the data block region
 * the inode table, root directory and free byte map are brought into memory when first initialized
 */
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "disk_emu.h"
// #include "sfs_api.h"

#define MAX_FILE_NAME 16
#define BLOCK_SIZE 1024
#define NO_FBM_BLOCKS 4  // Free byte map blocks
#define MAX_BLOCK 1024 * NO_FBM_BLOCKS
#define INODE_SIZE 56
#define INODE_TABLE_SIZE 6  // 6 blocks for inode table
#define MAX_FILE_NO 100
#define DATA_BLOCK_START INODE_TABLE_SIZE + 1

typedef struct inode {  // inode size : 56 bytes
  int size;             // file size in bytes
  int direct[12];       // a data block number
  int indirect;         // an index block number
} Inode;

typedef struct superblock {
  int magic;
  int block_size;       // 1024 bytes
  int fs_size;          // MAX_BLOCK
  int inode_table_len;  // number of inode blocks
  int root_inode;       // inode number of root directory
} Superblock;

typedef struct file {
  int inode_num;
  int offset;
} File;

typedef struct directory_entry {
  bool used;
  char file_name[MAX_FILE_NAME];
  int inode_num;
} DirectoryEntry;

Superblock superblock;
File FDT[MAX_FILE_NO] = {0};
Inode inode_table[MAX_FILE_NO] = {0};
int FBM[MAX_BLOCK] = {0};
DirectoryEntry root_dir[MAX_FILE_NO] = {0};

int find_free_block() {
  for (int i = 0; i < MAX_BLOCK; i++) {
    if (FBM[i] == 0) {
      FBM[i] = 1;
      return i;
    }
  }
  return -1;
}

int find_no_file_blocks(int size) {
  int no_blocks = size / BLOCK_SIZE;
  if (size % BLOCK_SIZE != 0) {
    no_blocks++;
  }
  return no_blocks;
}

void mksfs(int fresh) {
  if (fresh) {
    init_fresh_disk("my_sfs", BLOCK_SIZE, MAX_BLOCK);
    // create superblock
    superblock.magic = 0xACBD0005;
    superblock.block_size = BLOCK_SIZE;
    superblock.fs_size = MAX_BLOCK;
    superblock.inode_table_len = INODE_TABLE_SIZE;
    superblock.root_inode = 0;
    // write superblock to disk
    char *buffer = (char *)&superblock;
    write_blocks(0, 1, buffer);

    // initialize free byte map
    FBM[0] = 1;  // superblock
    // 4 free byte map blocks located at the end of the disk
    for (int i = 1; i <= NO_FBM_BLOCKS; i++) {
      FBM[MAX_BLOCK - i] = 1;
    }
    // allocate 6 blocks for inode table
    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
      FBM[i + 1] = 1;
    }
    // allocate 1 block for root directory
    FBM[DATA_BLOCK_START] = 1;
    // write free byte map to disk
    buffer = (char *)&FBM;
    // free byte map blocks are located at the end of the disk
    write_blocks(MAX_BLOCK - NO_FBM_BLOCKS, NO_FBM_BLOCKS, buffer);

    // initialize inode table for root directory
    inode_table[0].size = 0;
    for (int i = 0; i < 12; i++) {
      inode_table[0].direct[i] = -1;
    }
    inode_table[0].indirect = -1;
    // point the root inode to the root directory
    inode_table[0].direct[0] = DATA_BLOCK_START;
    // write root inode to disk
    buffer = (char *)&inode_table;
    write_blocks(1, INODE_TABLE_SIZE, buffer);

    // write an empty root directory to disk
    buffer = (char *)&root_dir;
    write_blocks(DATA_BLOCK_START, 1, buffer);

    // initialize the root directory array
    for (int i = 0; i < MAX_FILE_NO; i++) {
      root_dir[i].used = false;
    }

  } else {
    init_disk("sfs", BLOCK_SIZE, MAX_BLOCK);
    // read superblock from disk to memory and store it in superblock
    char *buffer = (char *)&superblock;
    read_blocks(0, 1, buffer);
    int root_inode = superblock.root_inode;
    // read free byte map from disk to memory and store it in FBM
    buffer = (char *)&FBM;
    // free byte map blocks are located at the end of the disk
    read_blocks(MAX_BLOCK - NO_FBM_BLOCKS, NO_FBM_BLOCKS, buffer);

    // read inode table from disk to memory and store it in inode_table
    buffer = (char *)&inode_table;
    read_blocks(1, INODE_TABLE_SIZE, buffer);

    // find the root directory inode in the inode table
    Inode root_dir_inode = inode_table[root_inode];

    // use it's size to determine how many blocks it occupies
    int root_dir_blocks = find_no_file_blocks(root_dir_inode.size);

    // read the root directory from disk to memory and store it in root_dir
    char root_dir_buffer[root_dir_blocks * BLOCK_SIZE];
    if (root_dir_blocks <= 12) {
      for (int i = 0; i < root_dir_blocks; i++) {
        read_blocks(root_dir_inode.direct[i], 1,
                    root_dir_buffer + i * BLOCK_SIZE);
      }
    } else {
      // read the first 12 blocks
      for (int i = 0; i < 12; i++) {
        read_blocks(root_dir_inode.direct[i], 1,
                    root_dir_buffer + i * BLOCK_SIZE);
      }
      // read the indirect block
      int indirect_block_buffer[BLOCK_SIZE / sizeof(int)];
      read_blocks(root_dir_inode.indirect, 1, (char *)&indirect_block_buffer);
      // read the data blocks pointed by the indirect block
      for (int i = 12; i < root_dir_blocks; i++) {
        read_blocks(indirect_block_buffer[i - 12], 1,
                    root_dir_buffer + i * BLOCK_SIZE);
      }
    }

    // read the root directory blocks into root_dir
    memcpy(&root_dir, root_dir_buffer, root_dir_blocks * BLOCK_SIZE);
  }

  // initialize file descriptor table
  for (int i = 0; i < MAX_FILE_NO; i++) {
    FDT[i].inode_num = -1;
    FDT[i].offset = 0;
  }
}

/*The sfs_fopen() opens a file and returns the index that corresponds to the
newly opened file in the file descriptor table. If the file does not exist, it
creates a new file and sets its size to 0 (as part of this you create the
directory entry and also allocate the i-Node for the file). No disk data block
allocated. File size is set to 0. If the file exists, the file is opened in
append mode (i.e., set the file pointer to the end of the file). Both case, we
should update our inode table and root directly in memory*/
int sfs_fopen(char *name) {
  // check if the file already exists by searching the root directory
  int file_inode_num = -1;
  for (int i = 0; i < MAX_FILE_NO; i++) {
    if (root_dir[i].used && strcmp(root_dir[i].file_name, name) == 0) {
      // the file already exists
      file_inode_num = root_dir[i].inode_num;
      break;
    }
  }
  // if the file exists, open it
  if (file_inode_num != -1) {
    // find the file inode in the inode table
    Inode file_inode = inode_table[file_inode_num];
    // find the number of blocks the file occupies
    int file_blocks = find_no_file_blocks(file_inode.size);
    // find the file descriptor table entry that is not in use
    int fdt_index = -1;
    for (int i = 0; i < MAX_FILE_NO; i++) {
      if (FDT[i].inode_num == -1) {
        fdt_index = i;
        break;
      }
    }

    if (fdt_index == -1) {
      return -1;
    }
    // set the file descriptor table entry
    FDT[fdt_index].inode_num = file_inode_num;
    FDT[fdt_index].offset = file_inode.size;
    // return the index of the file descriptor table entry
    return fdt_index;
  }

  // if the file does not exist, create a new file
  // find the file descriptor table entry that is not in use
  int fdt_index = -1;
  for (int i = 0; i < MAX_FILE_NO; i++) {
    if (FDT[i].inode_num == -1) {
      fdt_index = i;
      break;
    }
  }

  if (fdt_index == -1) {
    return -1;
  }
  // find the first available inode in the inode table
  int file_inode_num = -1;
  for (int i = 0; i < MAX_FILE_NO; i++) {
    if (inode_table[i].size == -1) {
      file_inode_num = i;
      break;
    }
  }

  if (file_inode_num == -1) {
    return -1;
  }
  // set the file descriptor table entry
  FDT[fdt_index].inode_num = file_inode_num;
  FDT[fdt_index].offset = 0;
  // set the inode table entry
  inode_table[file_inode_num].size = 0;
  inode_table[file_inode_num].direct[0] = find_available_block();
  // set the root directory entry by finding the first available entry
  int root_dir_index = -1;
  for (int i = 0; i < MAX_FILE_NO; i++) {
    if (!root_dir[i].used) {
      root_dir_index = i;
      break;
    }
  }
  // if there is no available entry in the root directory, return -1
  if (root_dir_index == -1) {
    return -1;
  }
  // set the root directory entry
  root_dir[root_dir_index].inode_num = file_inode_num;
  strcpy(root_dir[root_dir_index].file_name, name);
  root_dir[root_dir_index].used = 1;
  // return the index of the file descriptor table entry
  return fdt_index;

}

/*The sfs_fclose() closes a file, i.e., removes the entry (used = 0) from the file descriptor table (note that the file remains on disk – it is just the association between the process and the file is terminated). On success, sfs_fclose() should return 0 and a negative value otherwise.*/
int sfs_fclose(int fileID) {
  // check if the file descriptor table entry is in use
  if (FDT[fileID].inode_num == -1) {
    return -1;
  }
  // set the file descriptor table entry
  FDT[fileID].inode_num = -1;
  FDT[fileID].offset = 0;
  // return 0 on success
  return 0;
}

int sfs_fwrite(int, const char *, int);

int sfs_fread(int, char *, int);

/*The sfs_fseek() moves the read/write pointer (a single pointer in SFS) to the given location. It returns 0 on success and a negative integer value otherwise. No disk operation*/
int sfs_fseek(int fileID, int loc) {
  // check if the file descriptor table entry is in use
  if (FDT[fileID].inode_num == -1) {
    return -1;
  }
  // check if the location is valid
  if (loc < 0 || loc > inode_table[FDT[fileID].inode_num].size) {
    return -1;
  }
  // set the file descriptor table entry
  FDT[fileID].offset = loc;
  // return 0 on success
  return 0;
}

/* The sfs_remove() removes the file from the directory entry, releases the i-Node and releases the data blocks used by the file (i.e., the data blocks are added to the free block list/map), so that they can be used by new files in the future.*/
int sfs_remove( ){

}


// the following is for testing and usage of the functions

// int main() {
//   mksfs(1);
//   int f = sfs_fopen("some_name.txt");
//   char my_data[] = "The quick brown fox jumps over the lazy dog";
//   char out_data[1024];
//   sfs_fwrite(f, my_data, sizeof(my_data) + 1);
//   sfs_fseek(f, 0);
//   sfs_fread(f, out_data, sizeof(out_data) + 1);
//   printf("%s\n", out_data);
//   sfs_fclose(f);
//   // sfs_remove("some_name.txt");
// }
