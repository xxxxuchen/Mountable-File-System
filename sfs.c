/**
 * File System Implementation
 */
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "disk_emu.h"
#include "sfs_api.h"

#define MAX_FILE_NAME 16
#define BLOCK_SIZE 1024
#define NO_FBM_BLOCKS 4  // Free byte map blocks
#define MAX_BLOCK 1024 * NO_FBM_BLOCKS
#define INODE_SIZE 56
#define INODE_TABLE_SIZE 6 // 6 blocks for inode table



typedef struct inode { // inode size : 56 bytes
  int size;
  int direct[12]; // a data block number
  int indirect; // an index block number
} Inode;

typedef struct superblock {
  int magic;
  int block_size; // 1024 bytes
  int fs_size; // MAX_BLOCK
  int inode_table_len; // number of inode blocks
  int root_inode; // inode number of root directory
} Superblock;


typedef struct file {
  int inode_num;
  int offset;
} File;

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
  char *buffer = (char *) &superblock;
  write_blocks(0, 1, buffer);

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
