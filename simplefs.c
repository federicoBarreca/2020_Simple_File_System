#pragma once
#include "simplefs.h"
#include <stdio.h>


// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk){
	
	//security ctrl, if anyone of the arguments is NULL return NULL
	if(!fs || !disk)	return NULL;
	
}
