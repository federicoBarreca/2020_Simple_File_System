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

	// bitmap reset
	for(int i = 0; i < fs->disk->map->num_bits; i++) {
		BitMap_set(fs->disk->map, i, 0);
	}
	
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
	free(root);
	return;
}


// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename) {

	// security check on input args
	if(d == NULL || filename == NULL) return NULL;
	
	// security check on free blocks
	if(d->sfs->disk->header->free_blocks <= 1){
		printf("\nThe disk is full\n");
		return NULL; 
	}
	
	// checks if already exists a file named filename
	if(SimpleFS_openFile(d, filename) != NULL) return NULL;

	// file handle allocation
	FileHandle * file_handle = malloc(sizeof(FileHandle));
	file_handle->sfs = d->sfs;
	
	// first file block allocation and initialization
	FirstFileBlock * ffb = malloc(sizeof(FirstFileBlock)); 
	ffb->header.previous_block = -1;
	ffb->header.next_block = -1;
	ffb->header.block_in_file = 0;
	ffb->fcb.directory_block = d->dcb->fcb.block_in_disk;
	ffb->fcb.block_in_disk = DiskDriver_getFreeBlock(d->sfs->disk, 0);
	strcpy(ffb->fcb.name, filename);
	ffb->fcb.size_in_bytes = 0;
	ffb->fcb.size_in_blocks = ffb->fcb.size_in_bytes;
	ffb->fcb.is_dir = 0;
	
	file_handle->directory = d->dcb;
	file_handle->current_block = &(ffb->header);
	file_handle->pos_in_file = 0;
	file_handle->fcb = ffb;

	// resets file data with "end of line"
	memset(ffb->data, '\0', sizeof(ffb->data));

	// writes ffb in disk
	DiskDriver_writeBlock(d->sfs->disk, ffb, ffb->fcb.block_in_disk);

	// checks if the directory is full
	if(d->dcb->file_blocks[sizeof(d->dcb->file_blocks)-1] == 0){
		
		printf("\n\nThere's free space in first directory block\n\n");

		// finds the first free index in file_blocks
		int i;
		for(i = 0; d->dcb->file_blocks[i] != 0; i++) {}

		// updates directory control block info
		d->dcb->file_blocks[i] = ffb->fcb.block_in_disk;	
		d->dcb->num_entries++;

		// update the new info in disk
		DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
		
	}else{
		
		DirectoryBlock* db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));
		
		// first directory block has a next directory block
		if(d->dcb->header.next_block != -1){
			
			// looks for a free directory block or a to-be-created directory block
			DiskDriver_readBlock(d->sfs->disk, db, d->dcb->header.next_block);
			int curr_block = d->dcb->header.next_block;
			while(db->file_blocks[sizeof(db->file_blocks)-1] != 0 && db->header.next_block != -1){
				
				curr_block = db->header.next_block;
				DiskDriver_readBlock(d->sfs->disk, db, db->header.next_block);
			}
			
			// directory block is free
			if(db->file_blocks[sizeof(db->file_blocks)-1] == 0){
				//~ printf("\n\nThere's free space in directory block %d\n\n", curr_block);

				// finds the first free index in file_blocks
				int i;
				for(i = 0; db->file_blocks[i] != 0; i++) {}

				// updates directory control block info
				db->file_blocks[i] = ffb->fcb.block_in_disk;	
				d->dcb->num_entries++;

				// update the new info in disk
				DiskDriver_writeBlock(d->sfs->disk, db, curr_block);
			}
			
			// next directory block doesn't exist
			else if(db->header.next_block == -1){
				printf("\n\nCreating a new directory block because %d is full and has no next block, next block position = %d\n\n", curr_block, d->sfs->disk->header->first_free_block);

				// creates new directory block
				int free_block = d->sfs->disk->header->first_free_block;
				DirectoryBlock* new_db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));
				new_db->header.block_in_file = db->header.block_in_file+1;
				new_db->header.next_block = -1;
				new_db->header.previous_block = curr_block;
				
				db->header.next_block = free_block;
				DiskDriver_writeBlock(d->sfs->disk, db, curr_block);
				
				// updates directory control block info
				db->file_blocks[0] = ffb->fcb.block_in_disk;	
				d->dcb->num_entries++;

				// update the new info in disk
				DiskDriver_writeBlock(d->sfs->disk, new_db, free_block);
				free(new_db);				
			}
		}
		else{
			
			printf("\n\nCreating a new directory block because the first one is full and has no next block, next block position = %d\n\n", d->sfs->disk->header->first_free_block);
			
			// creates new directory block
			int free_block = d->sfs->disk->header->first_free_block;
			DirectoryBlock* new_db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));
			new_db->header.block_in_file = db->header.block_in_file+1;
			new_db->header.next_block = -1;
			new_db->header.previous_block = d->dcb->fcb.block_in_disk;
				
			d->dcb->header.next_block = free_block;
			DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
			
			// updates directory control block info
			db->file_blocks[0] = ffb->fcb.block_in_disk;	
			d->dcb->num_entries++;

			// update the new info in disk
			DiskDriver_writeBlock(d->sfs->disk, new_db, free_block);
			free(new_db);
		}
		free(db);	
	}
	
	
	DiskDriver_flush(d->sfs->disk);
	return file_handle;
}


// reads in the (preallocated) blocks array, the name of all files in a directory
int SimpleFS_readDir(char** names, DirectoryHandle* d) {

	// security check on input args
	if(names == NULL || d == NULL) return -1;

	FirstFileBlock * ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));

	// first directory block pointer
	FirstDirectoryBlock * db = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
	db = d->dcb;

	int i, dim_array = 0;
	for(i = 0; i < d->dcb->num_entries; i++, dim_array++) {

		// if the array is finished
		if(dim_array >= sizeof(db->file_blocks)) {
			DirectoryBlock * db;

			// updates relative index
			dim_array = dim_array - sizeof(db->file_blocks);

			// reads the next directory block
			DiskDriver_readBlock(d->sfs->disk, db, db->header.next_block);
		}

		// retrieves the first file block in db->file_blocks[dim_array]
		ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
		DiskDriver_readBlock(d->sfs->disk, ffb, db->file_blocks[dim_array]);

		names[dim_array] = ffb->fcb.name;
		
	}
	free(ffb);
	return 0;
}


// opens a file in the directory d. The file should be exisiting
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename){

	// security check on input args
	if(d == NULL || filename == NULL) return NULL;

	// file handle allocation
	FileHandle * file_handle = (FileHandle*) malloc(sizeof(FileHandle));
	
	// first directory block pointer
	FirstDirectoryBlock * db = d->dcb;

	int i, dim_array = 0; 
	for(i = 0; i < d->dcb->num_entries; i++, dim_array++){
		
		// if the array is finished
		if(dim_array >= sizeof(db->file_blocks)){
			
			DirectoryBlock* db;
			
			// updates relative index
			dim_array = dim_array - sizeof(db->file_blocks);
			
			// reads the next directory block
			DiskDriver_readBlock(d->sfs->disk, db, db->header.next_block);
		}
		
		// retrieves the first file block in db->file_blocks[dim_array]
		FirstFileBlock * ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
		DiskDriver_readBlock(d->sfs->disk, ffb, db->file_blocks[dim_array]);
		
		// compares the names of the files and checks if is not a directory
		if(strcmp(ffb->fcb.name, filename) == 0 && ffb->fcb.is_dir == 0){

			// initializes file_handle
			file_handle->sfs = d->sfs;
			file_handle->fcb = ffb;
			file_handle->directory = d->dcb;
			file_handle->current_block = &(ffb->header);
			file_handle->pos_in_file = 0;

			
			return file_handle;
		}
	}

	return NULL;
}


// closes a file handle (destroyes it)
int SimpleFS_close(FileHandle* f) {

	// security check
	if(f == NULL) return -1;

	free(f->directory);
	free(f);

	return 0;
}


// checks if a directory already exists
int SimpleFS_findDir(DirectoryHandle* d, const char* dirname){

	// security check on input args
	if(d == NULL || dirname == NULL) return 0;
	
	// first directory block pointer
	FirstDirectoryBlock * db = d->dcb;

	FirstFileBlock * ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
	
	int i, dim_array = 0; 
	for(i = 0; i < d->dcb->num_entries; i++, dim_array++){
		
		// if the array is finished
		if(dim_array >= sizeof(db->file_blocks)){
			
			DirectoryBlock* db;
			
			// updates relative index
			dim_array = dim_array - sizeof(db->file_blocks);
			
			// reads the next directory block
			DiskDriver_readBlock(d->sfs->disk, db, db->header.next_block);
		}
		
		// retrieves the first file block in db->file_blocks[dim_array]
		ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
		DiskDriver_readBlock(d->sfs->disk, ffb, db->file_blocks[dim_array]);
		
		// compares the names of the files and checks if is not a directory
		if(strcmp(ffb->fcb.name, dirname) == 0 && ffb->fcb.is_dir == 1){
			free(ffb);
			return 0;
		}
	}
	
	free(ffb);
	return -1;
}

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success
// -1 on error
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname){

	// security check on input args
	if(d == NULL || dirname == NULL) return -1;

	// security check on free blocks
	if(d->sfs->disk->header->free_blocks <= 1){
		printf("\nThe disk is full\n");
		return -1; 
	}

	// checks if directory dirname already exists
	if(SimpleFS_findDir(d,dirname) == 0) return -1;

	// first directory block allocation
	FirstDirectoryBlock * fdb = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
	fdb->header.previous_block = -1;
	fdb->header.next_block = -1;
	fdb->header.block_in_file = 0;
	fdb->fcb.directory_block = d->dcb->fcb.block_in_disk;
	fdb->fcb.block_in_disk = DiskDriver_getFreeBlock(d->sfs->disk, 0);
	strcpy(fdb->fcb.name, dirname);
	fdb->fcb.size_in_bytes = 0;
	fdb->fcb.size_in_blocks = 0;
	fdb->fcb.is_dir = 1;
	fdb->num_entries = 0;
	memset(fdb->file_blocks, 0, sizeof(fdb->file_blocks));
	
	DiskDriver_writeBlock(d->sfs->disk, fdb, fdb->fcb.block_in_disk);
	
	// checks if the directory is full
	if(d->dcb->file_blocks[sizeof(d->dcb->file_blocks)-1] == 0){
		
		//~ printf("\n\nThere's free space in first directory block\n\n");

		// finds the first free index in file_blocks
		int i;
		for(i = 0; d->dcb->file_blocks[i] != 0; i++) {}

		// updates directory control block info
		d->dcb->file_blocks[i] = fdb->fcb.block_in_disk;	
		d->dcb->num_entries++;

		// update the new info in disk
		DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
		
	}else{
		
		DirectoryBlock* db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));
		
		// first directory block has a next directory block
		if(d->dcb->header.next_block != -1){
			
			// looks for a free directory block or a to-be-created directory block
			DiskDriver_readBlock(d->sfs->disk, db, d->dcb->header.next_block);
			int curr_block = d->dcb->header.next_block;
			while(db->file_blocks[sizeof(db->file_blocks)-1] != 0 && db->header.next_block != -1){
				
				curr_block = db->header.next_block;
				DiskDriver_readBlock(d->sfs->disk, db, db->header.next_block);
			}
			
			// directory block is free
			if(db->file_blocks[sizeof(db->file_blocks)-1] == 0){
				//~ printf("\n\nThere's free space in directory block %d\n\n", curr_block);

				// finds the first free index in file_blocks
				int i;
				for(i = 0; db->file_blocks[i] != 0; i++) {}

				// updates directory control block info
				db->file_blocks[i] = fdb->fcb.block_in_disk;	
				d->dcb->num_entries++;

				// update the new info in disk
				DiskDriver_writeBlock(d->sfs->disk, db, curr_block);
			}
			
			// next directory block doesn't exist
			else if(db->header.next_block == -1){
				//~ printf("\n\nCreating a new directory block because %d is full and has no next block, next block position = %d\n\n", curr_block, d->sfs->disk->header->first_free_block);

				// creates new directory block
				int free_block = d->sfs->disk->header->first_free_block;
				DirectoryBlock* new_db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));
				new_db->header.block_in_file = db->header.block_in_file+1;
				new_db->header.next_block = -1;
				new_db->header.previous_block = curr_block;
				
				db->header.next_block = free_block;
				DiskDriver_writeBlock(d->sfs->disk, db, curr_block);
				
				// updates directory control block info
				db->file_blocks[0] = fdb->fcb.block_in_disk;	
				d->dcb->num_entries++;

				// update the new info in disk
				DiskDriver_writeBlock(d->sfs->disk, new_db, free_block);
				free(new_db);				
			}
		}
		else{
			
			//~ printf("\n\nCreating a new directory block because the first one is full and has no next block, next block position = %d\n\n", d->sfs->disk->header->first_free_block);
			
			// creates new directory block
			int free_block = d->sfs->disk->header->first_free_block;
			DirectoryBlock* new_db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));
			new_db->header.block_in_file = db->header.block_in_file+1;
			new_db->header.next_block = -1;
			new_db->header.previous_block = d->dcb->fcb.block_in_disk;
				
			d->dcb->header.next_block = free_block;
			DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
			
			// updates directory control block info
			db->file_blocks[0] = fdb->fcb.block_in_disk;	
			d->dcb->num_entries++;

			// update the new info in disk
			DiskDriver_writeBlock(d->sfs->disk, new_db, free_block);
			free(new_db);
		}
		free(db);	
	}

	free(fdb);
	DiskDriver_flush(d->sfs->disk);

	return 0;
}

