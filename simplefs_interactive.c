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

	SimpleFS* fs = (SimpleFS*) malloc(sizeof(SimpleFS));
	DiskDriver* disk = (DiskDriver*) malloc(sizeof(DiskDriver));
	FileHandle* fl = NULL;
	int i, ret;
	
	DiskDriver_init(disk, TEST_PATH, BLOCKS);
	
	DirectoryHandle * directory_handle = SimpleFS_init(fs, disk);
	if(directory_handle) {
		printf("\nFile System created and initlialized successfully\n");
	}else{
		printf("\nFile System creation error\n");
		return 0;
	}
	
	char** list = (char**) malloc(sizeof(char*)*directory_handle->dcb->num_entries);
	for(i = 0; i < directory_handle->dcb->num_entries; i++) {
		list[i] = (char*) malloc(125);
	}

	char quest[125] = " ";
	
	while(strcmp(quest, "quit") != 0){
		
		
		printf("\nWhat do you want to do? (type help for help) (quit to exit):\n");
		scanf("%s", quest);
		
		if(strcmp(quest, "help") == 0){
			printf("\nType ls to now the content of your directory\n");	
			printf("\nType cd to change directory\n");
			printf("\nType rm to remove a file or a directory inside your directory\n");	
			printf("\nType mkDir to create a directory inside your directory\n");
			printf("\nType mkFile to create a file inside your directory\n");
			printf("\nType write to write on a file\n");
			printf("\nType read a file\n");
			printf("\nType seek on a file\n");
		}
		else if(strcmp(quest, "ls") == 0){
			list = (char**) malloc(sizeof(char*)*directory_handle->dcb->num_entries);
			for(i = 0; i < directory_handle->dcb->num_entries; i++) {
				list[i] = (char*) malloc(125);
			}
			SimpleFS_readDir(list, directory_handle);
			for(i = 0; i < directory_handle->dcb->num_entries; i++) {
				printf(" - > %s\n", list[i]);
			}
		}
		else if(strcmp(quest, "cd") == 0){
			printf("\nWhere do you want to move?\n");
			scanf("%s", quest);
			printf("\nCurrently in dir %s, ", directory_handle->dcb->fcb.name);
			printf("changing to %s", quest);
			ret = SimpleFS_changeDir(directory_handle, quest);
			if(ret == 0)
				printf(", now in %s \n", directory_handle->dcb->fcb.name);
			else
				printf(", directory not found\n");
		
		}
		else if(strcmp(quest, "rm") == 0){
			printf("\nWhat do you want to remove?\n");
			scanf("%s", quest);
			printf("\nTrying to remove dir %s from dir %s\n", quest, directory_handle->dcb->fcb.name);
			ret = SimpleFS_remove(directory_handle, quest);
			if(ret >= 0)
				printf("\nFile deleted successfully\n");
			else
				printf("\nFile deletion error\n");
			
		}
		else if(strcmp(quest, "mkDir") == 0){
			printf("\nHow do you want to call the new directory?\n");
			scanf("%s", quest);
			printf("Trying to make dir pluto in dir %s\n", directory_handle->dcb->fcb.name);
			ret = SimpleFS_mkDir(directory_handle, quest);
			if(ret == 0)
				printf("Directory created successfully\n");
			else
				printf("Directory creation error\n");
			
		}
		else if(strcmp(quest, "mkFile") == 0){
			printf("\nHow do you want to call the new file?\n");
			scanf("%s", quest);
			fl = SimpleFS_createFile(directory_handle, quest);
			if(fl)
				printf("%s created successfully in dir %s\n", quest, directory_handle->dcb->fcb.name);
			else
				printf("%s creation error\n", quest);
			
		}
		else if(strcmp(quest, "write") == 0){
			printf("\nWhere do you want to write?\n");
			scanf("%s", quest);
			fl = SimpleFS_openFile(directory_handle, quest);
			if(!fl){
				printf("\nFile not found\n");
			}
			else{
				char* content = "";
				printf("\nWhat do you want to write?\n");
				scanf("%s", content);
				strcpy(content, content);
				ret = SimpleFS_write(fl, content, strlen(content));
				if(ret != -1)
					printf("File written successfully\n");
				else
					printf("File write error\n");
			}
			
		}
		else if(strcmp(quest, "read") == 0){
			printf("\nWhat do you want to read?\n");
			scanf("%s", quest);
			fl = SimpleFS_openFile(directory_handle, quest);
			char* content = " ";
			int size = fl->fcb->fcb.size_in_bytes;
			ret = SimpleFS_read(fl, (void*)content, size);
			if(ret != -1)
				printf("File read successfully:\n%s\n", content);
			else
				printf("File write error\n");
			
		}
		else if(strcmp(quest, "seek") == 0){
			printf("\nWhich file do you want to open?\n");
			scanf("%s", quest);
			fl = SimpleFS_openFile(directory_handle, quest);
			if(fl){
				char* content = " ";
				printf("\nWhere do you want to move the cursor?\n");
				scanf("%s", content);
				int pos = atoi(content);
				ret = SimpleFS_seek(fl, pos);
				printf("Cursor moved successfully\n");
			}
			else
				printf("%s opening error\n", quest);
			
		}
	}
	printf("\nClosing file if opened\n");
	printf("Closing %s\n", directory_handle->dcb->fcb.name);
	printf("Closing disk driver\n");
	printf("Closing simple file system\n");
	free(list);
	SimpleFS_close(fl);
	free(directory_handle);
	DiskDriver_destroy(disk);
	free(fs);
 
}
