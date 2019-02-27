#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H
#include "global.h"
#include <string.h>
#define BYTE_SIZE 8
#define NUMBER_OF_SECTORS_IN_BITMAP 16
#define NUMBER_OF_SECTORS_IN_SWAP 1024
#define NUMBER_OF_SECTORS_IN_ROOTDIR_HEADER 1
#define MAXIMUM_INODE_NUMBER 256
#define PARENT_INODE_FOR_ROOT 31
#define MXIMUM_DEPTH_OF_DIR 32
#define NUMBER_OF_ADDRESS_IN_INDEXLEVEL1 8
#define NUMBER_OF_ADDRESS_IN_INDEXLEVEL2 8
#define BITMAP_START_LOCATION 8
#define ROOTDIR_HEADER_START_LOCATION 24
#define SWAP_START_LOCATION 1024
#define ROOTDIR_INDEXLEVEL1_LOCATION 256
#define ROOTDIR_INDEXLEVEL2_LOCATION 257

#define LONGEST_NAME_LENGTH_FOR_FILE_DIR 7

#define OPEN_DIR_FLAG 0
#define CREATE_DIR_FLAG 1
#define OPEN_FILE_FLAG 2
#define CREATE_FILE_FLAG 3
#define DIR_CONTENTS_FLAG 4

#define WRITE_FILE_FLAG 0
#define READ_FILE_FLAG 1

#define DIR_FLAG 1
#define FILE_FLAG 0

typedef struct{
	int Inode;
	int Dir_or_File;
	long creationTime;
	int indexLocation;
	int indexLevel;
	int parentInode;
	int headerLocation;
	int diskID;
	int size;
}Directory_File;

typedef struct _DirHeaderLoc{
	int dirHeaderLoc;
	struct _DirHeaderLoc* next;
}DirHeaderLoc;

typedef struct _OpenedInode_type{
	int Inode;
	int headerLoc;
	int diskID;
	struct _OpenedInode_type* next;
}OpenedInode_type;


typedef struct{
	OpenedInode_type* head;
}OpenedInode_queue;

unsigned char BITMAP[MAX_NUMBER_OF_DISKS][NUMBER_OF_SECTORS_IN_BITMAP][PGSIZE];

int Inode_status[MAXIMUM_INODE_NUMBER];
OpenedInode_queue OpenedInode_Queue;

void init_File_Manager();
void init_Inode_status();
void init_BITMAP();

void init_OpenedInode_Queue();
int generate_a_new_Inode();
void set_currentDir(Directory_File dir);
int is_currentDir_root();
Directory_File get_dir_file_from_header(unsigned char header[],int headerLoc,int diskID);

int is_header_of_dir(unsigned char header[]);
void set_df_header(unsigned char header[],int Inode,char name[],int Dir_File,int parent_Inode,long currentTime,int indexLocation,int siz);

void push_dirHeaderLocation(int loc);
int pop_dirHeaderLocation();

void copy_name_from_header(char name[], unsigned char header[]);
int is_valid_name_for_dir_file(char name[]);

void add_Inode_to_OpenedInode_Queue(int Inode,int headerLoc,int diskID);
void get_headerLoc_from_OpenedInode_Queue(int Inode, int *headerLoc, int *diskID);
int del_Indoe_from_OpenedInode_Queue(int Inode);
void increase_fileSize_by_one_sector(unsigned char header[]);
//for debugging
void print_array(unsigned char _array[],int size);
void print_OpenedInode_Queue();
#endif