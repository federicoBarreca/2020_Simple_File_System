#pragma once
#include "disk_driver.h"
#include <fcntl.h>
#include <stdio.h>

// opens the file (creating it if necessary)
// allocates the necessary space on the disk
// calculates how big the bitmap should be
// if the file was new
// compiles a disk header, and fills in the bitmap of appropriate size
// with all 0 (to denote the free space);
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks){
	
	int file_descriptor;
	
	if(!access(filename, F_OK)) { // file already exists
		
		file_descriptor = open(filename, O_RDWR, 0666);
		
		if(!file) {
			printf("File opening error. Program stopped");
			return;
		}

		// DiskHeader allocation
		int ret = posix_fallocate(file, 0, sizeof(DiskHeader)+num_blocks+num_blocks*BLOCK_SIZE);
		
		if(!ret){
			printf("DiskHeader allocation error");
			return;
		}
		disk->fd = file_descriptor;
		//DiskHeader mmapped
		disk->header = (DiskHeader*) mmap(0, sizeof(DiskHeader)+num_blocks+num_blocks*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
		
	}else{ //file doesn't exists
		//file creation and opening
		file_descriptor = open(filename, O_CREAT | O_RDWR, 0666);

		
		if(!file) {
			printf("File opening error. Program stopped");
			return;
		}

		disk->fd=file_descriptor;

		int ret = posix_fallocate(file, 0, sizeof(DiskHeader)+num_blocks+num_blocks*BLOCK_SIZE);

		disk->header = (DiskHeader*) mmap(0, sizeof(DiskHeader)+num_blocks+num_blocks*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
		disk->header->num_blocks = num_blocks;
		disk->header->free_blocks = num_blocks;
		
	}
	
	
	lseek(file, 0, SEEK_SET);
	disk->header->first_free_block = DiskDriver_getFreeBlock(disk,0);
	
	// bitmap initialization
	disk->map = (BitMap*) malloc(sizeof(BitMap)); 
	disk->map->entries = (char*) malloc(size*sizeof(char));
	disk->map->num_bits = num_blocks;
	
	return;
}


// reads the block in position block_num and returns -1 if the block is free according to the bitmap, 0 otherwise
int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num){
	
	// security check on disk size
	if(block_num >= disk->header->num_blocks) return -1;
	
	// check in the bitmap if block_num is free
	if(BitMap_get(disk->map, block_num, 0) == block_num) return -1;
	
	// inserts in dest the block block_num
	memcpy(dest, disk->map->entries + disk->header->num_blocks + (block_num * BLOCK_SIZE), BLOCK_SIZE);

	// function ends returning 0
	return 0;
}

// writes a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num){
	
	// security check on disk size
	if(block_num >= disk->header->num_blocks) return -1;
	
	//security check on source size
	if(strlen(src) * 8 > BLOCK_SIZE) return -1;
	
	// decreases the number of free blocks in the disk header
	if(BitMap_get(disk->map,block_num,0) == block_num) disk->header->free_blocks--;
	
	BitMap_set(disk->map, block_num, 1);

	// inserts or overwites src in the block block_num
	memcpy(disk->map->entries + disk->header->num_blocks + (block_num * BLOCK_SIZE), src, BLOCK_SIZE);

	// synchronizes mmapped memory
	if(DiskDriver_flush(disk) == -1) return -1;

	// updates the first free block position in the disk header if changed
	if(disk->header->first_free_block != block_num)
		disk->header->first_free_block = DiskDriver_getFreeBlock(disk,0);		

  return 0;	
}

// frees a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
int DiskDriver_freeBlock(DiskDriver* disk, int block_num){}

// returns the first free block in the disk from position (checking the bitmap)
int DiskDriver_getFreeBlock(DiskDriver* disk, int start){}

// writes the data (flushing the mmaps)
int DiskDriver_flush(DiskDriver* disk){
	// size of the memory to be synchronized
	int mem_size = sizeof(DiskHeader) + (disk->header->num_blocks) + (disk->header->num_blocks*BLOCK_SIZE) ;

	// synchronizes to mmap memory
	return msync(disk->header, mem_size, MS_SYNC);
}
