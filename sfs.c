/**
 * File System Implementation
 * root directory starts at the first block of the data block region
 */
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "disk_emu.h"
#include "sfs_api.h"

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

void mksfs(int fresh) {
  if (fresh) {
    init_fresh_disk("sfs", BLOCK_SIZE, MAX_BLOCK);
  } else {
    init_disk("sfs", BLOCK_SIZE, MAX_BLOCK);
  }
  // create superblock
  Superblock superblock;
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

  // initialize file descriptor table
  for (int i = 0; i < MAX_FILE_NO; i++) {
    FDT[i].inode_num = -1;
    FDT[i].offset = 0;
  }

}

int sfs_fopen(char *);

int sfs_fclose(int);

int sfs_fwrite(int, const char *, int);

int sfs_fread(int, char *, int);

int sfs_fseek(int, int);

int sfs_remove(char *);

int main(int argc, char const *argv[]) {
  // char byte_array[sizeof(int)];

  // int i = 2353;
  // memcpy(byte_array, &i, sizeof(int));

  // // convert the byte array to original int
  // int j;
  // memcpy(&j, byte_array, sizeof(int));
  // printf("%d\n", j);

  // int i = 2536;
  // void *buffer = &i;
  // char *byte_array = (char *) buffer;

  // int *abc = (int *) byte_array;
  // printf("%d\n", *abc);

  // // print the size of inode
  // printf("Size of inode: %lu\n", sizeof(Inode));
}
