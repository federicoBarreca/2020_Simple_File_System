#include "bitmap.c"
#include "disk_driver.c"
#include "simplefs.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <stdlib.h>
#define BLOCKS 999

int main(int argc, char** argv) {
	
	if(argc < 2){
		printf("\033[0;31m"); 
		printf("\nUsage: ./simplefs_test.c <code>\n");
		printf("\033[0m"); 
		printf("\nIf you want to test the bitmap module's functions: code = bitmap\n");
		printf("\nIf you want to test the disk driver module's functions: code = diskdriver\n");
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
	
		printf("\nFreeing bitmap\n");
		free(bitmap->entries);
		free(bitmap);
		
	}
	//DISK DRIVER TEST
	else if(strcmp(test, "diskdriver") == 0){
		printf("DISK DRIVER FUNCTIONS TEST\n");
		
	}
	//FILE SYSTEM TESTs
	else if(strcmp(test, "simplefs") == 0){
		printf("SIMPLE FILE SYSTEM TEST\n");
		
	}
	else{
		printf("\033[0;31m"); 
		printf("\nUsage: ./simplefs_test.c <code>\n\n");
		printf("\033[0m"); 
		printf("If you want to test the bitmap module's functions: code = bitmap\n\n");
		printf("If you want to test the disk driver module's functions: code = diskdriver\n\n");
		printf("If you want to test the file system: code = simplefs\n\n");
		return 0;
	}
	
	
	
	
	
  //~ printf("FirstBlock size %ld\n", sizeof(FirstFileBlock));
  //~ printf("DataBlock size %ld\n", sizeof(FileBlock));
  //~ printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  //~ printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));
  
}
