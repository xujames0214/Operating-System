#include "File_Manager.h"
#include "Process_Manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**************************************************************************
To initialize file manager including Inode status array, the bitmap and OpenedInode Queue
***************************************************************************/
void init_File_Manager(){
	init_Inode_status();
	init_BITMAP();
	init_OpenedInode_Queue();
}

/**************************************************************************
To initialize Inode status array.
***************************************************************************/
void init_Inode_status(){
	int i;
	for(i = 0;i < MAXIMUM_INODE_NUMBER;i++){
		Inode_status[i] = 0;
	}
}

/**************************************************************************
To initialize bitmap stored in memory.
***************************************************************************/
void init_BITMAP(){
	int i,j,k;
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		for(j = 0;j < NUMBER_OF_SECTORS_IN_BITMAP;j++){
			for(k = 0;k < PGSIZE;k++){
				BITMAP[i][j][k] = 0;
			}
		}
	}
}

/**************************************************************************
To initialize OpenedInode Queue which will store Inodes of opened files.
***************************************************************************/
void init_OpenedInode_Queue(){
	OpenedInode_Queue.head = (OpenedInode_type*)malloc(sizeof(OpenedInode_type));
	OpenedInode_Queue.head->next = NULL;
}

/**************************************************************************
To generate a new Indoe.
***************************************************************************/
int generate_a_new_Inode(){
	int i;
	for(i = 0;i < MAXIMUM_INODE_NUMBER;i++){
		if(Inode_status[i] == 0){
			Inode_status[i] = 1;
			return i;
		}
	}
	return -1;
}

/**************************************************************************
To set a given directory as current directory in current PCB.
Input: the directory
***************************************************************************/
void set_currentDir(Directory_File dir){
	current_PCB.currentDir = dir;
}

/**************************************************************************
To check if current director is a root directory. 
***************************************************************************/
int is_currentDir_root(){
	return current_PCB.currentDir.parentInode == PARENT_INODE_FOR_ROOT;
}

/**************************************************************************
To get information from a header of directory or file and make a directory or file.
Input: the header, the header location, the diskID in which the header is stored.
***************************************************************************/
Directory_File get_dir_file_from_header(unsigned char header[],int headerLoc,int diskID){
	Directory_File df;
	int i;
	int description;
	df.Inode = header[0];
	description = header[8];
	df.creationTime = header[9] + header[10] * 256 + header[11] * 256 * 256;
	df.indexLocation = header[12] + header[13] * 256;
	df.size = header[14] + header[15] * 256;
	df.Dir_or_File = description % 2;
	df.indexLevel = (description / 2) % 4;
	df.parentInode = description / 8;
	df.headerLocation = headerLoc;
	df.diskID = diskID;
	return df;
}
/**************************************************************************
To check if the given header is one of a directory or not. It may be a file.
Input: the header.
***************************************************************************/
int is_header_of_dir(unsigned char header[]){
	int description;
	description = header[8];
	return description % 2 == 1;
}

/**************************************************************************
To construct a header of a directory or file with given information.
Input: Inode, name, Dir_or_File_Flag, parentInode, current time(to record creation time), index location, size.
***************************************************************************/
void set_df_header(unsigned char header[],int Inode,char name[],int Dir_File,int parent_Inode,long currentTime,int indexLocation,int size){
	int i;
	int name_length;
	header[0] = Inode; //'The Inode of Root Directory
	name_length = strlen(name);
	//The rootDir name
	for(i = 1;i <= name_length;i++){
		header[i] = name[i-1];
	}
	for(i = name_length + 1;i <= 7;i++){
		header[i] = '\0';
	}

	header[8] = parent_Inode * 8 + 4 + (Dir_File == DIR_FLAG ? 1 : 0); //File Description Field  XXXXX0X
	//creation time
	header[9] = currentTime % 256;
	header[10] = (currentTime / 256) % 256;
	header[11] = currentTime / 256 / 256;

	header[12] = indexLocation % 256; //Index Location: 256th block 
	header[13] = indexLocation / 256; 

	header[14] = size % 256;
	header[15] = size / 256;
}
void push_dirHeaderLocation(int loc){
	DirHeaderLoc *p = (DirHeaderLoc*)malloc(sizeof(DirHeaderLoc));
	p->dirHeaderLoc = loc;
	p->next = current_PCB.dirHeaderLocStack_head->next;
	current_PCB.dirHeaderLocStack_head->next = p;
}

int pop_dirHeaderLocation(){
	DirHeaderLoc *p = current_PCB.dirHeaderLocStack_head->next;
	int loc;
	if(p != NULL){
		current_PCB.dirHeaderLocStack_head->next = p->next;
		loc = p->dirHeaderLoc;
		free(p);
		return loc;
	}
	return 0;
}

/**************************************************************************
To copy name of a directory or file from a header to an array.
Input: the header.
***************************************************************************/
void copy_name_from_header(char name[],unsigned char header[]){
	int i;
	for(i = 1;i <= 7;i++){
		name[i - 1] = header[i];
	}
	name[7] = '\0';
}

/**************************************************************************
To check if the given name is valid for a directory or file.
Input: the name.
***************************************************************************/
int is_valid_name_for_dir_file(char name[]){
	int len = strlen(name);
	if(len > LONGEST_NAME_LENGTH_FOR_FILE_DIR) return 0;
	return 1;
}

/**************************************************************************
To add Inode along with other information about the opened file to Opened Inode Queue,
which indicates this file is opened.
Input: Inode, header location of the file, diskID of the file.
***************************************************************************/
void add_Inode_to_OpenedInode_Queue(int Inode,int headerLoc,int diskID){
	OpenedInode_type *p = OpenedInode_Queue.head->next;
	while(p != NULL){
		if(p->Inode == Inode) return;  //the Inode is already in the OpenedInode Queue
		p = p->next;
	}
	p = (OpenedInode_type*)malloc(sizeof(OpenedInode_type));
	p->Inode = Inode;
	p->headerLoc = headerLoc;
	p->diskID = diskID;
	p->next = OpenedInode_Queue.head->next;
	OpenedInode_Queue.head->next = p;
}

/**************************************************************************
To get header location and diskID of an opened file with given Inode from OpenedInode Queue.
This helps to load header of the target file which will be read or written later.
Input: Inode of target file.
Output: the location of the file's header, the file's diskID.
***************************************************************************/
void get_headerLoc_from_OpenedInode_Queue(int Inode, int *headerLoc, int *diskID){
	OpenedInode_type *p = OpenedInode_Queue.head->next;
	while(p != NULL){
		if(p->Inode == Inode){
			*headerLoc = p->headerLoc;
			*diskID = p->diskID;
			return;
		}
		p = p->next;
	}
	*headerLoc = -1;
	*diskID = -1;
}

/**************************************************************************
To delete Inode from OpenedIndoe Queue, which means the target file is closed.
Input: the Inode of target file.
***************************************************************************/
int del_Indoe_from_OpenedInode_Queue(int Inode){
	OpenedInode_type *p = OpenedInode_Queue.head,*q;
	while(p->next != NULL){
		if(p->next->Inode == Inode){
			q = p->next;
			p->next = q->next;
			free(q);
			return 1;
		}
		p = p->next;
	}
	return 0;
}

/**************************************************************************
To change the header of a file to increase the file size by one sector.
***************************************************************************/
void increase_fileSize_by_one_sector(unsigned char header[]){
	int size = header[14] + header[15] * 256;
	size += PGSIZE;
	header[14] = size % 256;
	header[15] = size / 256;
}

//for debugging
void print_array(unsigned char _array[],int size){
	int i;
	for(i = 0;i < size;i++){
		printf("%d ",_array[i]);
	}
	printf("\n");
}

void print_OpenedInode_Queue(){
	OpenedInode_type *p = OpenedInode_Queue.head->next;
	printf("OpenedInode_Queue: ");
	while(p != NULL){
		printf("%d ",p->Inode);
		p = p->next;
	}
	printf("\n");
	
}

