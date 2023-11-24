/**
 * Author: Xu Chen
 * File System Implementation
 * root directory starts at the first block of the data block region
 * the inode table, root directory and free byte map are brought into memory
 * when first initialized
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

int min(int x, int y) { return x < y ? x : y; }

int allocate_free_block() {
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

void fill_last_block(int block_num, char *write_buffer, int offset,
                     int remaining, char *writing_contents) {
  char *buffer = (char *)malloc(BLOCK_SIZE);
  read_blocks(block_num, 1, buffer);
  memcpy(write_buffer, buffer, offset);
  memcpy(write_buffer + offset, writing_contents, remaining);
  free(buffer);
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

    // initialize inode table
    for (int i = 0; i < MAX_FILE_NO; i++) {
      inode_table[i].size = -1;
      for (int j = 0; j < 12; j++) {
        inode_table[i].direct[j] = -1;
      }
      inode_table[i].indirect = -1;
    }

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
    init_disk("my_sfs", BLOCK_SIZE, MAX_BLOCK);
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
    // check if the file is already open
    for (int i = 0; i < MAX_FILE_NO; i++) {
      if (FDT[i].inode_num == file_inode_num) {
        // the file is already open
        return i;
      }
    }

    // find the file inode in the inode table
    Inode file_inode = inode_table[file_inode_num];
    // find the number of blocks the file occupies
    // int file_blocks = find_no_file_blocks(file_inode.size);
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
  file_inode_num = -1;
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
  inode_table[file_inode_num].direct[0] = allocate_free_block();
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

/*The sfs_fclose() closes a file, i.e., removes the entry (used = 0) from the
 * file descriptor table (note that the file remains on disk – it is just the
 * association between the process and the file is terminated). On success,
 * sfs_fclose() should return 0 and a negative value otherwise.*/
int sfs_fclose(int fileID) {
  // check if the file descriptor table entry is in use
  if (FDT[fileID].inode_num == -1) {
    return -1;
  }
  // set the file descriptor table entry
  FDT[fileID].inode_num = -1;
  FDT[fileID].offset = 0;
  return 0;
}

/*The sfs_fwrite() writes the given number of bytes of data in buf into the open
 * file, starting from the current file pointer. This in effect could increase
 * the size of the file by at most the given number of bytes (it may not
 * increase the file size by the number of bytes written if the write pointer is
 * located at a location other than the end of the file). The sfs_fwrite()
 * should return the number of bytes written.*/
int sfs_fwrite(int fileID, char *buf, int length) {
  // check if the file descriptor table entry is in use
  if (FDT[fileID].inode_num == -1) {
    return -1;
  }

  // Get the file inode from the inode table
  int file_inode_num = FDT[fileID].inode_num;
  Inode file_inode = inode_table[file_inode_num];

  // Calculate the current block and offset within the block
  int current_block = FDT[fileID].offset / BLOCK_SIZE;
  int offset_within_block = FDT[fileID].offset % BLOCK_SIZE;

  // Calculate the remaining space in the current block
  int remaining_space_in_block = BLOCK_SIZE - offset_within_block;

  // Initialize variables to keep track of the bytes written and the buffer
  // pointer
  int bytes_written = 0;
  char *current_buffer = buf;

  while (bytes_written < length) {
    // Check if the current block is within the direct blocks of the inode
    if (current_block < 12) {
      // If the current block is not allocated, allocate a new block
      if (file_inode.direct[current_block] == -1) {
        file_inode.direct[current_block] = allocate_free_block();
        if (file_inode.direct[current_block] == -1) {
          printf("error for direct block allocation\n");
          return bytes_written;
        }
      }

      // Calculate the amount of data to write in the current block
      int write_size = min(remaining_space_in_block, length - bytes_written);

      if (bytes_written == 0 && write_size <= remaining_space_in_block) {
        // fill the remaining space in current block
        char write_buffer[BLOCK_SIZE];
        fill_last_block(file_inode.direct[current_block], write_buffer,
                        offset_within_block, write_size, current_buffer);
        write_blocks(file_inode.direct[current_block], 1, write_buffer);
      } else {
        // write the data to the current block
        char write_buffer[BLOCK_SIZE];
        memcpy(write_buffer, current_buffer, write_size);
        write_blocks(file_inode.direct[current_block], 1, write_buffer);
      }

      // Update file pointer and counters
      FDT[fileID].offset += write_size;
      bytes_written += write_size;
      current_block++;
      remaining_space_in_block = BLOCK_SIZE;
      current_buffer += write_size;
      // need to update the offset because we may fill the remaining space in
      // the indirect block later
      offset_within_block = FDT[fileID].offset % BLOCK_SIZE;
    } else {
      // If the current block is in the indirect block
      if (file_inode.indirect == -1) {
        // If the indirect block is not allocated, allocate a new block
        file_inode.indirect = allocate_free_block();
        if (file_inode.indirect == -1) {
          printf("error for index block allocation\n");
          return bytes_written;
        }

        // Initialize the indirect block with -1 (unallocated)
        int indirect_block_buffer[BLOCK_SIZE / sizeof(int)];
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
          indirect_block_buffer[i] = -1;
        }
        // Write the initialized indirect block to disk
        write_blocks(file_inode.indirect, 1, (char *)&indirect_block_buffer);
      }

      // Read the indirect block into memory
      int index_block[BLOCK_SIZE / sizeof(int)];
      read_blocks(file_inode.indirect, 1, (char *)&index_block);

      // If the current block in the index block is not allocated, allocate a
      // new block
      if (index_block[current_block - 12] == -1) {
        index_block[current_block - 12] = allocate_free_block();
        if (index_block[current_block - 12] == -1) {
          printf("error for indirect data block allocation\n");
          return bytes_written;
        }
        // Write the updated index block to disk
        write_blocks(file_inode.indirect, 1, (char *)&index_block);
      }

      // Calculate the amount of data to write in the current block
      int write_size = min(remaining_space_in_block, length - bytes_written);

      if (bytes_written == 0 && write_size <= remaining_space_in_block) {
        // fill the remaining space in current block
        char write_buffer[BLOCK_SIZE];
        fill_last_block(index_block[current_block - 12], write_buffer,
                        offset_within_block, write_size, current_buffer);
        write_blocks(index_block[current_block - 12], 1, write_buffer);
      } else {
        // write the data to the current block
        char write_buffer[BLOCK_SIZE];
        memcpy(write_buffer, current_buffer, write_size);
        write_blocks(index_block[current_block - 12], 1, write_buffer);
      }

      // Update file pointer and counters
      FDT[fileID].offset += write_size;
      bytes_written += write_size;
      current_block++;
      remaining_space_in_block = BLOCK_SIZE;
      current_buffer += write_size;
    }
  }

  // Update the size of the file in the inode table
  if (FDT[fileID].offset > file_inode.size) {
    file_inode.size = FDT[fileID].offset;
    inode_table[file_inode_num] = file_inode;
  }

  // Write the updated inode table and free byte map to disk
  char *buffer = (char *)&inode_table;
  write_blocks(1, INODE_TABLE_SIZE, buffer);

  buffer = (char *)&FBM;
  write_blocks(MAX_BLOCK - NO_FBM_BLOCKS, NO_FBM_BLOCKS, buffer);

  return bytes_written;
}

int sfs_fread(int fileID, char *buf, int length) {
  // Check if the file descriptor table entry is in use
  if (FDT[fileID].inode_num == -1) {
    return -1;
  }

  // Get the file inode from the inode table
  int file_inode_num = FDT[fileID].inode_num;
  Inode file_inode = inode_table[file_inode_num];

  // Calculate the current block and offset within the block
  int current_block = FDT[fileID].offset / BLOCK_SIZE;
  int offset_within_block = FDT[fileID].offset % BLOCK_SIZE;

  int following_bytes = BLOCK_SIZE - offset_within_block;

  // Initialize variables to keep track of the bytes read and the buffer pointer
  int bytes_read = 0;
  char *current_buffer = buf;

  while (bytes_read < length) {
    // Check if the current block is within the direct blocks of the inode
    if (current_block < 12) {
      // If the current block is not allocated, break (reached end of file)
      if (file_inode.direct[current_block] == -1) {
        break;
      }

      // Calculate the amount of data to read from the current block
      int read_size = min(following_bytes, length - bytes_read);

      // TODO: --------------------------------------

      // Read data from the current block
      read_blocks(file_inode.direct[current_block], 1,
                  current_buffer + bytes_read);

      // Update file pointer and counters
      FDT[fileID].offset += read_size;
      bytes_read += read_size;
      current_block++;
      current_buffer += read_size;
    } else {
      // If the current block is in the indirect block
      if (file_inode.indirect == -1) {
        break;  // reached end of file
      }

      // Read the indirect block into memory
      int indirect_block_buffer[BLOCK_SIZE / sizeof(int)];
      read_blocks(file_inode.indirect, 1, (char *)&indirect_block_buffer);

      // If the current block in the indirect block is not allocated, break
      // (reached end of file)
      if (indirect_block_buffer[current_block - 12] == -1) {
        break;
      }

      // Calculate the amount of data to read from the current block
      int read_size =
          (file_inode.size - FDT[fileID].offset < length - bytes_read)
              ? file_inode.size - FDT[fileID].offset
              : length - bytes_read;

      // Read data from the current block in the indirect block
      read_blocks(indirect_block_buffer[current_block - 12], 1,
                  current_buffer + bytes_read);

      // Update file pointer and counters
      FDT[fileID].offset += read_size;
      bytes_read += read_size;
      current_block++;
      current_buffer += read_size;
    }
  }

  return bytes_read;
}

/*The sfs_fseek() moves the read/write pointer (a single pointer in SFS) to the
 * given location. It returns 0 on success and a negative integer value
 * otherwise. No disk operation*/
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

/* The sfs_remove() removes the file from the directory entry, releases the
 * i-Node and releases the data blocks used by the file (i.e., the data blocks
 * are added to the free block list/map), so that they can be used by new files
 * in the future.*/

// int sfs_remove() {}

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
