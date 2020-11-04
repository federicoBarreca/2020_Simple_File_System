#pragma once
#include "simplefs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> 
#include <stdlib.h>

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk){

	// security check on fs and disk correct initialization
	if(fs == NULL || disk == NULL) return NULL;

	// sets "disk" as the first file system's disk
	fs->disk = disk;
	
	DirectoryHandle* directory_handle = (DirectoryHandle*) malloc(sizeof(DirectoryHandle));	
	
	// the root directory should be in the first block	
	if(fs->disk->header->first_free_block == 0){
		
		//if file system doesn't exist
		SimpleFS_format(fs);
	}
	else{
		
		printf("\nFile System already formatted\n");
	}
	
	directory_handle->sfs = fs;
	
	// retrieves fdb info from disk
	FirstDirectoryBlock * root = malloc(sizeof(FirstDirectoryBlock));
	DiskDriver_readBlock(disk, root, 0);
	
	directory_handle->dcb = root;
	directory_handle->directory = NULL;
	directory_handle->current_block = &(directory_handle->dcb->header);
	directory_handle->pos_in_dir = 0;
	directory_handle->pos_in_block = root->fcb.block_in_disk;
	
	return directory_handle;
}

// creates the initial structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
void SimpleFS_format(SimpleFS* fs) {

	// security check on fs correct initialization
	if(fs == NULL) return;

	//~ BitMap* bmap = fs->disk->map;
	
	//~ bitmap.num_bits = fs->disk->header->bitmap_entries * 8;
	//~ bitmap.entries = fs->disk->bitmap_data;

	// bitmap reset
	for(int i = 0; i < fs->disk->map->num_bits; i++) {
		BitMap_set(fs->disk->map, i, 0);
	}

	// Memorizzo le entries della bitmap nel disk
	//~ fs->disk->bitmap_data = bitmap.entries;
	
	// first block of the root directory allocation
	FirstDirectoryBlock * root = malloc(sizeof(FirstDirectoryBlock));

	// initializes fdb's header
	root->header.previous_block = -1;
	root->header.next_block = -1;
	root->header.block_in_file = 0; 

	// initializes fdb's file control block
	root->fcb.directory_block = -1;
	root->fcb.block_in_disk = fs->disk->header->first_free_block;
	strcpy(root->fcb.name,"/");
	root->fcb.size_in_bytes = sizeof(FirstDirectoryBlock);
	root->fcb.size_in_blocks = root->fcb.size_in_bytes;
	root->fcb.is_dir = 1;
	root->num_entries = 0;

	// resets file_blocks
	memset(root->file_blocks, 0, sizeof(root->file_blocks));

	// writes first_directory_block in disk
	DiskDriver_writeBlock(fs->disk, root, fs->disk->header->first_free_block);
	DiskDriver_flush(fs->disk);	

	return;
}
