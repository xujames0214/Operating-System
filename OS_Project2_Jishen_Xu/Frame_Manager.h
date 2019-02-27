#ifndef FRAME_MANAGER_H
#define FRAME_MANAGER_H
#include "global.h"
#include "stdio.h"

typedef struct{
	int PID;
	int pageNumber;
	int isUsed;
	int isShared;
}Frame;

typedef struct{
	int isInDisk;
	int diskID;
	int sector;
	int isSharedMemory;
}Page_Status;

typedef struct{
	INT32 PagesInSharedArea;
	char AreaTag[32];
	INT32 SharedID;
	INT32 StartingPageNumber;
}Shared_Memory_Info;

typedef struct node_PageTable{
	void *PageTable;
	Page_Status *PageStatusTable;
	struct node_PageTable *next;
	int startingPageNumber;
	int pagesInSharedArea;
	int sharedID;
}PageTable_node;

typedef struct{
	PageTable_node *head;
	int size;
}PageTable_queue;

Frame Frame_Table[NUMBER_PHYSICAL_PAGES];
int Frame_Table_point;
PageTable_queue PageTable_Queue;
void init_Frame_Manager();
void init_PageTable_Queue();
int get_new_frame(int PID, int pageNumber,int isShared);
void set_Frame_Table(int frameNumber,int PID,int pageNumber,int isUsed);
int get_new_memory_shared_ID();
void free_Frame_Table(int pid);
int is_page_number_in_shared_memory(int pageNumber); 
void add_node_to_PageTable_Queue(void *PageTable,Page_Status *PageStatusTable,int startingPageNumber, int pagesInSharedArea,int sharedID);
void update_PageTable_Queue(int offsetInSharedArea, unsigned short entry);
#endif
