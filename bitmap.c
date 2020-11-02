#include "bitmap.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

// "num" represents a block's position in memory
// "num" will be converted in 2 values: bitmap entry index and offset inside the array
BitMapEntryKey BitMap_blockToIndex(int num) {
	BitMapEntryKey block_info;
	// computes the bitmap entry index
	block_info.entry_num = num / 8 ;
	// 
	// computes the actual position offset inside the array
	block_info.bit_num = num % 8;
	return block_info;
}

// the bitmap entry key will be converted to an integer representing the block's position in memory
int BitMap_indexToBlock(BitMapEntryKey entry) {
	return (entry.entry_num * 8) + entry.bit_num;
}


// sets the bit at index pos in bitmap to status
 int BitMap_set(BitMap* bitmap, int pos, int status) {
	
	// security check on the bitmap's size
	if(pos > bitmap->num_bits) return -1;

	// declares the bitmap entry key and bit mask
    BitMapEntryKey block_info = BitMap_blockToIndex(pos);
	uint8_t mask = 1 << (7 - block_info.bit_num);

	if(status){
		bitmap->entries[block_info.entry_num] |= mask;
	}else{
		bitmap->entries[block_info.entry_num] &= ~(mask);
	}
	
    return status;
 }

// returns the index of the first bit having status "status" in the bitmap, and starts looking from position start
int BitMap_get(BitMap* bitmap, int start, int status) {

	// security check on the bitmap's size
	if(start > bitmap->num_bits) return -1;

	int pos, idx, res;

	// for each bit from start
	// security check guaranteed by the cycle
	for(idx = start; idx < bitmap->num_bits; idx++) {

		//~ if(idx == bitmap->num_bits) return -1;
		
		BitMapEntryKey block_info = BitMap_blockToIndex(idx);
	 	res = bitmap->entries[block_info.entry_num] & (1 << (7 - block_info.bit_num)); 

		// if status is 1, "res" must be greater than 0
		if(status) {
			if(res > 0) return idx;
		}else{
			if(res) return idx;
		}
	}
	
	// error: index out of range bitmap->num_bits
	return -1;
}
