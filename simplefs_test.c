#include "bitmap.c"
#include "disk_driver.c"
#include "simplefs.c"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <stdlib.h>

#define BLOCKS 1000
#define TEST_PATH "mydisk.txt"


int main(int argc, char** argv) {
	
	if(argc < 2){
		printf("\033[0;31m"); 
		printf("\nUsage: ./simplefs_test.c <code>\n");
		printf("\033[0m"); 
		printf("\nIf you want to test the bitmap module's functions: code = bitmap\n");
		printf("\nIf you want to test the disk driver module's functions: code = disk_driver\n");
		printf("\nIf you want to test the file system: code = simplefs\n");
		return 0;
	}
	
	char* test = argv[1];
	
	//BITMAP TEST
	if(strcmp(test, "bitmap") == 0){
		printf("BITMAP FUNCTIONS TEST\n");
		
		// BitMap_blockToIndex(int num)
		int pos = BLOCKS;
		printf("\n*** Testing BitMap_blockToIndex(%d) ***\n", pos);   
		BitMapEntryKey block_info = BitMap_blockToIndex(pos);
		printf("\nBlock position = %d has been converted to entry number = %d, bit offset = %d\n", pos, block_info.entry_num, block_info.bit_num);
		printf("{Expected: 124, 7}\n");
	 
		// BitMap_indexToBlock(int entry, uint8_t bit_num)
		printf("\n*** Testing BitMap_indexToBlock(%d, %d) ***\n", block_info.entry_num, block_info.bit_num);
		pos = BitMap_indexToBlock(block_info.entry_num, block_info.bit_num); 
		printf("\nBitmap entry key (%d, %d) has been converted to block position = %d\n", block_info.entry_num, block_info.bit_num, pos);
		printf("{Expected: 999}\n");

		// BitMap_get(BitMap* bmap, int start, int status)
		// BitMap_set(BitMap* bmap, int pos, int status)
		printf("\n*** Testing BitMap_get(BitMap* bmap, int start, int status) ***\n");
		printf("*** Testing BitMap_set(BitMap* bmap, int pos, int status) ***\n");
		
		BitMap* bitmap = (BitMap*) malloc(sizeof(BitMap));
		bitmap->entries = (char*) calloc(BLOCKS/8,sizeof(char));
		bitmap->num_bits = BLOCKS;
		printf("\nNumber of blocks = %d, number of bitmap entries = %d\n", BLOCKS, BLOCKS/8); 
		
		printf("\nBitMap_get returns the index of the first block in a status, -1 otherwise\n");
		printf("\nBitMap_set returns the new status of the block, -1 otherwise\n");
		
		printf("\nBitMap_get(bitmap, 0, 0) returns -> %d    {Expected: 0}\n", BitMap_get(bitmap, 0, 0));
		printf("BitMap_get(bitmap, 50, 0) returns -> %d    {Expected: 50}\n", BitMap_get(bitmap, 50, 0));
		printf("BitMap_get(bitmap, 998, 0) returns -> %d    {Expected: 998}\n", BitMap_get(bitmap, 998, 0));
		printf("BitMap_get(bitmap, 8000, 0) returns -> %d    {Expected: -1}\n", BitMap_get(bitmap, 8000, 0));

		printf("BitMap_get(bitmap, 0, 1) returns -> %d    {Expected: -1}\n", BitMap_get(bitmap, 0, 1));
		
		printf("BitMap_set(bitmap, 0, 1) returns -> %d    {Expected: 1}\n", BitMap_set(bitmap, 0, 1));
		printf("BitMap_set(bitmap, 998, 1) returns -> %d    {Expected: 1}\n", BitMap_set(bitmap, 998, 1));
		printf("BitMap_set(bitmap, 8000, 0) returns -> %d    {Expected: -1}\n", BitMap_set(bitmap, 8000, 0));
		
		printf("BitMap_get(bitmap, 50, 1) returns -> %d    {Expected: 998}\n", BitMap_get(bitmap, 50, 1));
		
		printf("BitMap_set(bitmap, 50, 1) returns -> %d    {Expected: 1}\n", BitMap_set(bitmap, 50, 1));
		
		printf("BitMap_get(bitmap, 0, 1) returns -> %d    {Expected: 0}\n", BitMap_get(bitmap, 0, 1));
		
		printf("BitMap_set(bitmap, 0, 0) returns -> %d    {Expected: 0}\n", BitMap_set(bitmap, 0, 0));
		
		printf("BitMap_get(bitmap, 0, 0) returns -> %d    {Expected: 0}\n", BitMap_get(bitmap, 0, 0));
	
		printf("\nDestroying bitmap\n");
		BitMap_destroy(bitmap);
		
	}
	//DISK DRIVER TEST
	else if(strcmp(test, "disk_driver") == 0){
		printf("DISK DRIVER FUNCTIONS TEST\n");
		
		// DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks)
		printf("\n*** Testing DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks) ***\n");
		DiskDriver* disk = (DiskDriver*) malloc(sizeof(DiskDriver));
		DiskDriver_init(disk, TEST_PATH, BLOCKS);
		
		int block_num = disk->header->first_free_block;
		
		if(block_num == -1){
			printf("Disk driver initialization error\n");
			return 0;
		}
		
		printf("\nDisk driver and bitmap correctly initialized\n");
		printf("First free block index = %d\n", block_num);
		
		// DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num)
		// DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num)
		printf("\n*** Testing DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num) ***\n");
		printf("\n*** Testing DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num) ***\n");
		
		char* in = "pippo";
		void* src = (void*) in;

		printf("\nWriting data = %s in block %d\n", in, block_num);
		int ret = DiskDriver_writeBlock(disk, src, block_num);
		printf("Function returned %d, data successfully written in block %d\n", ret, block_num);

		void* dest = (void*) malloc(BLOCK_SIZE);
		printf("\nRetrieving data from block %d\n", block_num);
		ret = DiskDriver_readBlock(disk, dest, block_num);
		printf("Function returned %d, data = %s\n", ret, (char*)dest);

		printf("\nWriting data in block %d\n", -1);
		ret = DiskDriver_writeBlock(disk, src, -1);
		printf("Function returned %d, data not written in block %d\n", ret, -1);

		printf("\nWriting data = paperino in block %d\n", block_num+3);
		ret = DiskDriver_writeBlock(disk, "paperino", block_num+3);
		printf("Function returned %d, data successfully written in block %d\n", ret, block_num+3);
		
		printf("\nRetrieving data from block %d\n", block_num+3);
		ret = DiskDriver_readBlock(disk, dest, block_num+3);
		printf("Function returned %d, data = %s\n", ret, (char*)dest);
  
		// DiskDriver_getFreeBlock(DiskDriver* disk, int start)
		// DiskDriver_freeBlock(DiskDriver* disk, int block_num)
		printf("\n*** Testing DiskDriver_getFreeBlock(DiskDriver* disk, int start) ***\n");
		printf("\n*** Testing DiskDriver_freeBlock(DiskDriver* disk, int block_num) ***\n");
		
		block_num = DiskDriver_getFreeBlock(disk, 0);
		printf("\nCurrent first free block = %d\n", block_num);
		
		ret = DiskDriver_freeBlock(disk, 0);
		printf("\nBlock %d freed with return value = %d\n", 0, ret);
		
		ret = DiskDriver_freeBlock(disk, block_num+3);
		printf("\nBlock %d freed with return value = %d\n", block_num, ret);
		
		block_num = DiskDriver_getFreeBlock(disk, 0);
		printf("\nCurrent first free block = %d\n", block_num);
			
		printf("\nMajor error checking\n");
		ret = 0;
		ret += DiskDriver_freeBlock(disk, block_num);                          //Tries freeing an empty block
		ret += DiskDriver_freeBlock(disk, block_num+BLOCKS);                   //Tries freeing an out of range block
		ret += DiskDriver_readBlock(disk, dest, block_num-1);                  //Tries reading an empty block
		ret += DiskDriver_readBlock(disk, dest, block_num+BLOCKS);             //Tries reading from an out of range block
		ret += DiskDriver_getFreeBlock(disk, block_num+BLOCKS);                //Tries retrieving a free block from out of range
		ret += DiskDriver_writeBlock(disk, src, block_num);                    //Writing on a block for next test
		ret += DiskDriver_writeBlock(disk, src, block_num);                    //Tries writing in a non free block
		ret += DiskDriver_writeBlock(disk, src, block_num+BLOCKS);             //Tries writing in an out of range block
		
		printf("\nClosing disk driver\n");
		free(dest);
		DiskDriver_destroy(disk);
		
	}
	//FILE SYSTEM TEST
	else if(strcmp(test, "simplefs") == 0){
		printf("SIMPLE FILE SYSTEM TEST\n");
		
		// SimpleFS_init(SimpleFS* fs, DiskDriver* disk)
		// SimpleFS_format(SimpleFS* fs)
		printf("\n*** Testing SimpleFS_init(SimpleFS* fs, DiskDriver* disk)) ***\n");
		printf("*** Testing SimpleFS_format(SimpleFS* fs) ***\n");
		SimpleFS* fs = (SimpleFS*) malloc(sizeof(SimpleFS));
		DiskDriver* disk = (DiskDriver*) malloc(sizeof(DiskDriver));
		
		DiskDriver_init(disk, TEST_PATH, BLOCKS);
	
		DirectoryHandle * directory_handle = SimpleFS_init(fs, disk);
		if(directory_handle) {
			printf("\nFile System created and initlialized successfully\n");
		}else{
			printf("\nFile System creation error\n");
		}
		
		// SimpleFS_createFile(DirectoryHandle* d, const char* filename)
		printf("\n*** Testing SimpleFS_createFile(DirectoryHandle* d, const char* filename) ***\n");
		int i, ret, num_file = 5;
		FileHandle* fl = (FileHandle*) malloc(sizeof(FileHandle));
		char filename[125];
		for(i = 0; i < num_file; i++) {
			sprintf(filename, "file_%d.txt", directory_handle->dcb->num_entries);
			fl = SimpleFS_createFile(directory_handle,filename);
			if(fl) {
			printf("%s created successfully in dir %s\n", filename, directory_handle->dcb->fcb.name);
			
			}
			else{
				printf("%s creation error\n", filename);
			}
		}
			
		printf("\nTrying to create a file that already exixts\n");
		fl = SimpleFS_createFile(directory_handle,"file_4.txt");
		if(fl != NULL) {
			printf("%s created successfully in dir %s\n", "file_4.txt", directory_handle->dcb->fcb.name);
			
		}else{
			printf("%s creation error: file already created in dir %s\n", "file_4.txt", directory_handle->dcb->fcb.name);
		}
	
		// SimpleFS_openFile(DirectoryHandle* d, const char* filename)
		printf("\n*** Testing SimpleFS_openFile(DirectoryHandle* d, const char* filename) ***\n");
		printf("Trying to open file_2.txt\n");
		strcpy(filename,"file_2.txt");
		fl = SimpleFS_openFile(directory_handle, filename);
		if(fl) {
			printf("%s opened successfully\n", fl->fcb->fcb.name);
		}else{
			printf("Opening %s error\n", fl->fcb->fcb.name);
			
		}		

	 	// SimpleFS_mkDir(DirectoryHandle* d, char* dirname)
		printf("\n*** Testing SimpleFS_mkDir(DirectoryHandle* d, char* dirname) ***\n");
		printf("Trying to make dir pluto in dir %s\n", directory_handle->dcb->fcb.name);
		ret = SimpleFS_mkDir(directory_handle, "pluto");
		if(ret == 0) {
			printf("Directory created successfully\n");
		}else{
			printf("Directory creation error\n");
		}

	 	// SimpleFS_readDir(char** names, DirectoryHandle* d)
		printf("\n*** Testing SimpleFS_readDir(char** names, DirectoryHandle* d) ***\n");
		printf("Number elements in dir %s = %d\n" ,  directory_handle->dcb->fcb.name, directory_handle->dcb->num_entries);
		char ** list = (char**) malloc(sizeof(char*)*directory_handle->dcb->num_entries);
		for(i = 0; i < directory_handle->dcb->num_entries; i++) {
			list[i] = (char*) malloc(125);
		}
		SimpleFS_readDir(list, directory_handle);
		for(i = 0; i < directory_handle->dcb->num_entries; i++) {
			printf(" - > %s\n", list[i]);
		}
		
		// SimpleFS_changeDir(DirectoryHandle* d, char* dirname)
		printf("\n*** Testing SimpleFS_changeDir(DirectoryHandle* d, char* dirname) ***\n");
		printf("Currently in dir %s, ", directory_handle->dcb->fcb.name);
		printf("changing to \"..\" ret=> %d", SimpleFS_changeDir(directory_handle, ".."));
		printf(", now in %s \n", directory_handle->dcb->fcb.name);
		
		printf("Currently in dir %s, ", directory_handle->dcb->fcb.name);
		printf("changing to \"pluto\" ret=> %d", SimpleFS_changeDir(directory_handle, "pluto"));
		printf(", now in %s \n", directory_handle->dcb->fcb.name);
		
		printf("Currently in dir %s, ", directory_handle->dcb->fcb.name);
		printf("changing to \"..\" ret=> %d", SimpleFS_changeDir(directory_handle, ".."));
		printf(", now in %s \n", directory_handle->dcb->fcb.name);
		
		printf("Currently in dir %s, ", directory_handle->dcb->fcb.name);
		printf("changing to \"..\" ret=> %d", SimpleFS_changeDir(directory_handle, ".."));
		printf(", now in %s \n", directory_handle->dcb->fcb.name);
		
		// SimpleFS_write(FileHandle* f, void* data, int size)
		printf("\n*** Testing SimpleFS_write(FileHandle* f, void* data, int size) ***\n");
		char* string = "Sora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofySora, Donald and GoofyAO";
		printf("Writing in %s:\n%s\n", fl->fcb->fcb.name, string);
		ret = SimpleFS_write(fl, string, strlen(string));
		printf("\nLength of the string = %ld\n", strlen(string));
		printf("%d bytes written\n", ret);
		printf("%d size in bytes\n", fl->fcb->fcb.size_in_bytes);
		printf("%d blocks written\n", fl->fcb->fcb.size_in_blocks);
		
		// SimpleFS_read(FileHandle* f, void* data, int size)
		// SimpleFS_seek(FileHandle* f, int pos)
		printf("\n*** Testing SimpleFS_read(FileHandle* f, void* data, int size) ***\n");
		printf("*** Testing SimpleFS_seek(FileHandle* f, int pos) ***\n");
		int size = fl->fcb->fcb.size_in_bytes;
		char data[size];
		SimpleFS_read(fl, (void*)data, size);
		printf("%s now contains:\n%s\n", fl->fcb->fcb.name, data);
		printf("\nChanging the file cursor to 80 and writing INSERT in the file\n");
		ret = SimpleFS_seek(fl, 80);
		ret = SimpleFS_write(fl, " INSERT ", strlen(" INSERT "));
		size = fl->fcb->fcb.size_in_bytes;
		SimpleFS_read(fl, (void*)data, size);
		printf("%s now contains:\n%s\n", fl->fcb->fcb.name, data);

		// SimpleFS_remove(DirectoryHandle* d, char* filename)
		printf("\n*** Testing SimpleFS_remove(DirectoryHandle* d, char* filename) ***\n");
		printf("\nCurrently in dir %s, ", directory_handle->dcb->fcb.name);
		printf("changing to \"pluto\" ret=> %d", SimpleFS_changeDir(directory_handle, "pluto"));
		printf(", now in %s \n", directory_handle->dcb->fcb.name);
		
		printf("\nPopulating dir pluto with a directory and a file\n");
		printf("Trying to make dir sora in dir %s\n", directory_handle->dcb->fcb.name);
		ret = SimpleFS_mkDir(directory_handle, "sora");
		if(ret == 0) {
			printf("Directory created successfully\n");
		}else{
			printf("Directory creation error\n");
		}
		strcpy(filename, "prova_x.txt");
		fl = SimpleFS_createFile(directory_handle,filename);
		if(fl){
			printf("%s created successfully in dir %s\n", filename, directory_handle->dcb->fcb.name);
		}
		else{
			printf("%s creation error\n", filename);
		}
		
		printf("\nCurrently in dir %s, ", directory_handle->dcb->fcb.name);
		printf("changing to \"..\" ret=> %d", SimpleFS_changeDir(directory_handle, ".."));
		printf(", now in %s \n", directory_handle->dcb->fcb.name);
		
		printf("\nInside %s:\n", directory_handle->dcb->fcb.name);
		list = (char**) malloc(sizeof(char*)*directory_handle->dcb->num_entries);
		for(i = 0; i < directory_handle->dcb->num_entries; i++) {
			list[i] = (char*) malloc(125);
		}
		SimpleFS_readDir(list, directory_handle);
		for(i = 0; i < directory_handle->dcb->num_entries; i++) {
			printf(" - > %s\n", list[i]);
		}
		
		strcpy(filename, "pluto");
		printf("\nTrying to remove dir %s from dir %s\n", filename, directory_handle->dcb->fcb.name);
		ret = SimpleFS_remove(directory_handle, filename);
		if(ret >= 0) {
			printf("\nFile deleted successfully\n");
		}else{
			printf("\nFile deletion error\n");
		}
		
		printf("\nAfter deletion inside %s:\n", directory_handle->dcb->fcb.name);
		list = (char**) malloc(sizeof(char*)*directory_handle->dcb->num_entries);
		for(i = 0; i < directory_handle->dcb->num_entries; i++) {
			list[i] = (char*) malloc(125);
		}
		SimpleFS_readDir(list, directory_handle);
		for(i = 0; i < directory_handle->dcb->num_entries; i++) {
			printf(" - > %s\n", list[i]);
		}
		
		printf("\nClosing %s\n", fl->fcb->fcb.name);
		printf("Closing %s\n", directory_handle->dcb->fcb.name);
		printf("Closing disk driver\n");
		printf("Closing simple file system\n");
		free(list);
		SimpleFS_close(fl);
		free(directory_handle);
		DiskDriver_destroy(disk);
		free(fs);
	}
	else{
		printf("\033[0;31m"); 
		printf("\nUsage: ./simplefs_test.c <code>\n\n");
		printf("\033[0m"); 
		printf("If you want to test the bitmap module's functions: code = bitmap\n\n");
		printf("If you want to test the disk driver module's functions: code = disk_driver\n\n");
		printf("If you want to test the file system: code = simplefs\n\n");
		return 0;
	}
  
}
