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
	if(!fs || !disk) return NULL;

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
	FirstDirectoryBlock* root = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
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
	if(!fs) return;

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
	if(!d || !filename) return NULL;
	
	// security check on free blocks
	if(d->sfs->disk->header->free_blocks <= 2){
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
		
		//~ printf("\n\nThere's free space in first directory block\n\n");

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
				db->file_blocks[0] = ffb->fcb.block_in_disk;	
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
	if(!names || !d) return -1;

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
	if(!d || !filename) return NULL;

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
	if(!f) return 0;
	free(f->fcb);
	free(f);

	return 0;
}


// checks if a directory already exists and returns the block index
int SimpleFS_findDir(DirectoryHandle* d, const char* dirname){

	// security check on input args
	if(!d || !dirname) return 0;
	
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
			int block = ffb->fcb.block_in_disk;
			free(ffb);
			return block;
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
	if(!d || !dirname) return -1;

	// security check on free blocks
	if(d->sfs->disk->header->free_blocks <= 1){
		printf("\nThe disk is full\n");
		return -1; 
	}

	// checks if directory dirname already exists
	if(SimpleFS_findDir(d,dirname) != -1) return -1;

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


// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
int SimpleFS_changeDir(DirectoryHandle* d, char* dirname) {

	// security check on input args
	if(!d || !dirname) return -1;

	// back to the parent directory
	if(strcmp(dirname,"..") == 0){
		
		//checks if parent is not the root
		if(!d->directory){
			
			printf("you are in the root, ");
			return -1;
		}
		else{
			
			free(d->dcb);
			
			//updates directory handle
			d->dcb = d->directory;
			d->current_block = &(d->dcb->header);
			d->pos_in_dir = 0;
			d->pos_in_block = d->dcb->fcb.block_in_disk;
			
			if(strcmp(d->dcb->fcb.name, "/") != 0){
				
				FirstDirectoryBlock* parent = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
				DiskDriver_readBlock(d->sfs->disk, parent, d->dcb->fcb.directory_block);
				d->directory = parent;
			}
			else{
				d->directory = NULL;
			}
			
			return 0;
		}
	}else{

		int index = SimpleFS_findDir(d, dirname);

		// checks if dirname exists
	 	if(index != -1){
			
			FirstDirectoryBlock* child = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
			DiskDriver_readBlock(d->sfs->disk, child, index);
			
			free(d->directory);
			 
			//updates directory handle
			d->directory = d->dcb;
			d->dcb = child;
			d->current_block = &(d->dcb->header);
			d->pos_in_dir = 0;
			d->pos_in_block = index;
			return 0;
		}else{
			
			return -1;			
		}	
	}
}


// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success
// -1 on error (file too short)
int SimpleFS_seek(FileHandle* f, int pos) {

	// security check on input args
	if(!f || pos < 0 || pos > f->fcb->fcb.size_in_bytes) return -1;

	// scans each block of the file
	int weight = 0;
	FirstFileBlock * ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
	DiskDriver_readBlock(f->sfs->disk, ffb, f->fcb->fcb.block_in_disk);
	
	// computes the weight of the file
	weight += sizeof(ffb->data);
	if(ffb->header.next_block != -1) {
		
		FileBlock * file = (FileBlock*) malloc(sizeof(FileBlock));
		DiskDriver_readBlock(f->sfs->disk, file, ffb->header.next_block);
		
		weight += sizeof(file->data);
		while(file->header.next_block != -1) {
			
			DiskDriver_readBlock(f->sfs->disk, file, file->header.next_block);
			weight += sizeof(file->data);
		}
		
		free(file);
	}
	free(ffb);
	// file is too short
	if(pos > weight){
		
		return -1;
	}else{
		
		// updates the file current pointer
		f->pos_in_file = pos;
		return pos;
	}

}


// reads in the file, at current position size bytes stored in data
// returns the number of bytes read
int SimpleFS_read(FileHandle* f, void* info, int size) {

	// security check on input args
	if(!f || !info || f->pos_in_file > f->fcb->fcb.size_in_bytes || size < 0) return -1;

	// initializes data
	char* data = (char*) info;
	memset(data, '\0', size);

	// Memorizzo il primo blocco del file da leggere
	FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
	DiskDriver_readBlock(f->sfs->disk, ffb, f->fcb->fcb.block_in_disk);
	
	// reads only the first block
	if(size <= strlen(ffb->data)) {
		
		strcpy(data, ffb->data);
	}
	else{
		strcpy(data, ffb->data);
		
		// scans each file block until data's size reaches size
		FileBlock* file = (FileBlock*) malloc(sizeof(FileBlock));

		int next_block = f->fcb->header.next_block;
		
		while(next_block != -1) {
			DiskDriver_readBlock(f->sfs->disk, file, next_block);
			
			// data += file->data
			sprintf(data, "%s%s", data, file->data);
			
			next_block = file->header.next_block;
		}
		
		free(file);
	}
	
	free(ffb);
	// returns using strlen because data is char*
	return strlen(data);
}


// adds a new FileBlock
int SimpleFS_addFileBlock(DiskDriver* disk, FileBlock* new_file_block, FileBlock* parent){
	
	// security check on input args
	if(!disk || !new_file_block || !parent || disk->header->free_blocks <= 1){
		return -1;
	}
	
	// initializes new_file_block
	new_file_block->header.block_in_file = parent->header.block_in_file+1;
	new_file_block->header.next_block = -1;
	
	// retrieves and insert the previous block index
	DiskDriver_readBlock(disk, parent, new_file_block->header.previous_block);
	new_file_block->header.previous_block = parent->header.next_block;
	
	parent->header.next_block = disk->header->first_free_block;
	
	// writes the new block and updates the previous on disk
	DiskDriver_writeBlock(disk, parent, new_file_block->header.previous_block);
	DiskDriver_writeBlock(disk, new_file_block, disk->header->first_free_block);
	DiskDriver_flush(disk);
}


// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int SimpleFS_write(FileHandle* f, void* data, int size) {

	// security check on input args
	if(!f || !data || f->pos_in_file > f->fcb->fcb.size_in_bytes || size < 0 ) return -1;

	// bytes written to be returned and blocks written counter
	int bytes_w = 0, blocks_w = 0;

	// cursor in file
	int pos = f->pos_in_file;
	
	// updates the current position in data
	f->pos_in_file = (int) (f->pos_in_file + size);
	
	// counter of memory read from the first file block
	int ctr = sizeof(f->fcb->data);
	
	// if fits in the first file block
	if(size + pos < ctr){
		
		// writes all data starting from pos
		memcpy(f->fcb->data+pos, data, size);
		bytes_w = size;
		blocks_w = 1;
	}
	//if starts from ffb
	else if(pos < ctr){
		
		// fills the ffb 
		memcpy(f->fcb->data+pos, data, ctr-pos);
		bytes_w += ctr-pos;
		blocks_w = 1;
		
		FileBlock* file_block = (FileBlock*) malloc(sizeof(FileBlock));
		
		// if next block exists reads it
		if(DiskDriver_readBlock(f->sfs->disk, file_block, f->current_block->next_block) != -1){}
		// if not creates it
		else{
			
			// adds a new file block linked to the first one
			f->current_block->next_block = f->sfs->disk->header->first_free_block;
			file_block->header.next_block = -1;
			file_block->header.previous_block = f->fcb->fcb.block_in_disk;
			file_block->header.block_in_file = f->current_block->block_in_file+1;
			
			// updates the first one on disk
			DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->fcb.block_in_disk);
		}
		
		// if fits entirely in the block writes all data
		if((size - bytes_w) < sizeof(file_block->data)){
					
			memcpy(file_block->data, data+bytes_w, (size - bytes_w));
			bytes_w += (size - bytes_w);
		}
		// or fills the current block
		else{
					
			memcpy(file_block->data, data+bytes_w, sizeof(file_block->data) - (size - bytes_w));
			bytes_w += sizeof(file_block->data) - (size - bytes_w);
		}
				
		blocks_w++;
		// writes the new block if just created on disk, or updates it
		DiskDriver_writeBlock(f->sfs->disk, file_block, f->current_block->next_block);
		
		FileBlock * prev_block = (FileBlock*) malloc(sizeof(FileBlock));
		DiskDriver_readBlock(f->sfs->disk, prev_block, f->fcb->fcb.block_in_disk);
		
		// until writes all data
		while(bytes_w < size){
					
			// if next block exists reads it				
			if(DiskDriver_readBlock(f->sfs->disk, file_block, file_block->header.next_block) != -1){}
			// if not creates it
			else{
					
				file_block = (FileBlock*) malloc(sizeof(FileBlock));
				if(SimpleFS_addFileBlock(f->sfs->disk, file_block, prev_block) == -1)	return-1;
				
			}
					
			DiskDriver_readBlock(f->sfs->disk, prev_block, prev_block->header.next_block);
					
			// if fits entirely in the block writes all data
			if((size - bytes_w) < sizeof(file_block->data)){
					
				memcpy(file_block->data, data+bytes_w, (size - bytes_w));
				bytes_w += (size - bytes_w);
			}
			// or fills the current block
			else{
					
				memcpy(file_block->data, data+bytes_w, sizeof(file_block->data) - (size - bytes_w));
				bytes_w += sizeof(file_block->data) - (size - bytes_w);
			}
				
			blocks_w++;
			DiskDriver_writeBlock(f->sfs->disk, file_block, prev_block->header.next_block);	
		}
		
		free(prev_block);
		free(file_block);
	}
	// if doesn't start from ffb
	else{
		
		FileBlock* file_block = (FileBlock*) malloc(sizeof(FileBlock));
		DiskDriver_readBlock(f->sfs->disk, file_block, f->current_block->next_block);
		ctr += sizeof(file_block->data);

		FileBlock * prev_block = (FileBlock*) malloc(sizeof(FileBlock));
		DiskDriver_readBlock(f->sfs->disk, prev_block, f->fcb->fcb.block_in_disk);
			
		// scans blocks until finds block containing pos
		while(pos > ctr){
			DiskDriver_readBlock(f->sfs->disk, prev_block, prev_block->header.next_block);
				
			DiskDriver_readBlock(f->sfs->disk, file_block, file_block->header.next_block);
			ctr += sizeof(file_block->data);		
		}
		
		// computes the offset of the cursor in the current block
		int offset = pos-(ctr-sizeof(file_block->data));
				
		// if fits entirely in the current block		
		if(size + offset < sizeof(file_block->data)){
		
			// writes all data starting from offset
			memcpy(file_block->data+offset, data, size);
			bytes_w = size;
			blocks_w = 1;
			
			DiskDriver_writeBlock(f->sfs->disk, file_block, prev_block->header.next_block);	
		}
		else{
				
			// fills the current block	
			memcpy(file_block->data+offset, file_block, sizeof(file_block->data)-offset);
			
			bytes_w = sizeof(file_block->data)-offset;
			blocks_w++;
			DiskDriver_writeBlock(f->sfs->disk, file_block, prev_block->header.next_block);	
			
			// until writes all data
			while(bytes_w < size){
					
				// if next block exists reads it				
				if(DiskDriver_readBlock(f->sfs->disk, file_block, file_block->header.next_block) != -1){}
				// if not creates it
				else{
					
					file_block = (FileBlock*) malloc(sizeof(FileBlock));
					if(SimpleFS_addFileBlock(f->sfs->disk, file_block, prev_block) == -1)	return-1;
				}
					
				DiskDriver_readBlock(f->sfs->disk, prev_block, prev_block->header.next_block);
					
				// if fits entirely in the block writes all data
				if((size - bytes_w) < sizeof(file_block->data)){
					
					memcpy(file_block->data, data+bytes_w, (size - bytes_w));
					bytes_w += (size - bytes_w);
				}
				// or fills the current block
				else{
					
					memcpy(file_block->data, data+bytes_w, sizeof(file_block->data) - (size - bytes_w));
					bytes_w += sizeof(file_block->data) - (size - bytes_w);
				}
				
				blocks_w++;
				DiskDriver_writeBlock(f->sfs->disk, file_block, prev_block->header.next_block);	
			}
			
			free(prev_block);
			free(file_block);
		}
	}
	
	// updates fields and writes in disk
	f->fcb->fcb.size_in_bytes = pos + size > f->fcb->fcb.size_in_bytes ? pos + size : f->fcb->fcb.size_in_bytes;
	f->fcb->fcb.size_in_blocks = blocks_w;
	
	DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->fcb.block_in_disk);
	DiskDriver_flush(f->sfs->disk);
	
	return bytes_w;
}


// frees each block after the first file block(if file) or first directory block(if directory)
void SimpleFS_free_file_dir(DiskDriver* disk, DirectoryHandle* d, FileHandle* f, int isDir){
	
	// if deletes a directory
	if(isDir){
		
		// first directory block pointer
		FirstDirectoryBlock * db = d->directory;
		
		int curr_block = db->fcb.block_in_disk;
		
		int i, dim_array = 0;
		for(i = 0; i < d->directory->num_entries; i++, dim_array++) {

			// if the array is finished
			if(dim_array >= sizeof(db->file_blocks)) {
				DirectoryBlock * db;

				// updates relative index
				dim_array = dim_array - sizeof(db->file_blocks);

				curr_block = db->header.next_block;

				// reads the next directory block
				DiskDriver_readBlock(d->sfs->disk, db, db->header.next_block);
			}
			
			if(db->file_blocks[dim_array] == d->dcb->fcb.block_in_disk){
				db->file_blocks[dim_array] = 0;
				
				while(db->file_blocks[dim_array+1]){
					db->file_blocks[dim_array] = db->file_blocks[dim_array+1];
					dim_array++;
				}
				db->file_blocks[dim_array] = 0;
				
				DiskDriver_writeBlock(disk, db, curr_block);
				break;
			}
		
		}
		
		d->directory->num_entries--;
		
		// updates d->directory->num_entries in disk
		DiskDriver_writeBlock(disk, d->directory, d->directory->fcb.block_in_disk);
		
		DirectoryBlock* dir = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));

		int next_block = d->dcb->header.next_block;
		
		// frees every block of the directory
		while(next_block != -1) {
			DiskDriver_readBlock(d->sfs->disk, dir, next_block);
			
			DiskDriver_freeBlock(disk, next_block);
			
			next_block = dir->header.next_block;
		}
		
		
		DiskDriver_freeBlock(disk, d->dcb->fcb.block_in_disk);
		DiskDriver_flush(disk);
		free(dir);
		
	}
	// if deletes a file
	else{
		
		// first directory block pointer
		FirstDirectoryBlock * db = f->directory;
		
		int curr_block = db->fcb.block_in_disk;
		
		int i, dim_array = 0;
		for(i = 0; i < f->directory->num_entries; i++, dim_array++) {

			// if the array is finished
			if(dim_array >= sizeof(db->file_blocks)) {
				DirectoryBlock * db;

				// updates relative index
				dim_array = dim_array - sizeof(db->file_blocks);

				curr_block = db->header.next_block;

				// reads the next directory block
				DiskDriver_readBlock(d->sfs->disk, db, db->header.next_block);
			}
			
			if(db->file_blocks[dim_array] == f->fcb->fcb.block_in_disk){
				db->file_blocks[dim_array] = 0;
				
				
				while(db->file_blocks[dim_array+1]){
					db->file_blocks[dim_array] = db->file_blocks[dim_array+1];
					dim_array++;
				}
				db->file_blocks[dim_array] = 0;
				
				DiskDriver_writeBlock(disk, db, curr_block);
				break;
			}
		}
				
		// updates d->dcb->num_entries in disk
		f->directory->num_entries--;
		DiskDriver_writeBlock(disk, f->directory, f->directory->fcb.block_in_disk);
		
		FileBlock* file = (FileBlock*) malloc(sizeof(FileBlock));

		int next_block = f->fcb->header.next_block;
		
		// frees every block of the file
		while(next_block != -1) {
			DiskDriver_readBlock(f->sfs->disk, file, next_block);
			
			DiskDriver_freeBlock(disk, next_block);
			
			next_block = file->header.next_block;
		}
		
		DiskDriver_freeBlock(disk, f->fcb->fcb.block_in_disk);
		DiskDriver_flush(disk);
		free(file);
	}
	
}


int SimpleFS_remove_aux(DirectoryHandle* d, FileHandle* f, char* filename, int control){
	
	// tries to open filename
	f = SimpleFS_openFile(d, filename);
	
	// tries to change directory
	int ret = SimpleFS_changeDir(d, filename);
	
	// if filename is a directory
	if(!f && ret != -1){

		control++;
		
		int i;
		char** names = (char**) malloc(sizeof(char*)*d->dcb->num_entries);
		for(i = 0; i < d->dcb->num_entries; i++) {
			names[i] = (char*) malloc(125);
		}
		
		SimpleFS_readDir(names, d);
		
		int h = d->dcb->num_entries;
		
		// recursively remove the files contained in names
		for(i = 0; i < h; i++){
			control += SimpleFS_remove_aux(d, f, names[i], control);
		}
		
		free(names);
		
		// remove the directory once empty
		SimpleFS_free_file_dir(d->sfs->disk, d, f, 1);
		
		// returns to the parent directory
		SimpleFS_changeDir(d, "..");
		
	}
	// if filename is a file
	else if(f){
		
		control++;
		
		// remove filename
		SimpleFS_free_file_dir(f->sfs->disk, d, f, 0);	
	}
	
	return control;
	
}


// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(DirectoryHandle* d, char* filename) {

	// security check on input args
	if(!d || !filename) return -1;
	
	FileHandle* f = NULL;
		
	// remove auxiliary function
	int ret = SimpleFS_remove_aux(d, f, filename, 0);
	
	SimpleFS_close(f);
	
	if(ret)	return 0;
	return -1;
}
