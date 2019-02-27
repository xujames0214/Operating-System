#include "Frame_Manager.h"
#include "Process_Manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/******************************************************************
To initialize the frame manager
******************************************************************/
void init_Frame_Manager(){
	int i;
	for(i = 0;i < NUMBER_PHYSICAL_PAGES;i++){
		Frame_Table[i].isUsed = 0;
		Frame_Table[i].pageNumber = -1;
		Frame_Table[i].PID = -1;
		Frame_Table[i].isShared = 0;
	}
	Frame_Table_point = 0;
	init_PageTable_Queue();
}

/******************************************************************
To initialize the PageTableQueue which stores all PageTables of processes having shared area
******************************************************************/
void init_PageTable_Queue(){
	PageTable_Queue.head = (PageTable_node*)malloc(sizeof(PageTable_node));
	PageTable_Queue.head->next = NULL;
	PageTable_Queue.size = 0;
}

/******************************************************************
To get a free frame from physical memory
Input: PID, the PID of process which requires a frame
pageNumber, page number of the page that is accessed and needs a frame space
isShared, whether the page is in a shared area
Output: frame number of the free frame
******************************************************************/
int get_new_frame(int PID, int pageNumber,int isShared){
	int i;
	for(i = 0;i < NUMBER_PHYSICAL_PAGES;i++){
		if(Frame_Table[i].isUsed == 0){
			Frame_Table[i].isUsed = 1;
			Frame_Table[i].PID = PID;
			Frame_Table[i].pageNumber = pageNumber;
			Frame_Table[i].isShared = isShared;
			return i;
		}
	}
	return -1;
}

/******************************************************************
To set frame tabel which stores information about the frames in physical memory
Input: frameNumber, which frame's information will be updated
PID of process that owns this frame,
pageNumber of the frame in page table,
isUsed, whether this frame is used by a process.
******************************************************************/
void set_Frame_Table(int frameNumber,int PID,int pageNumber,int isUsed){
	Frame_Table[frameNumber].PID = PID;
	Frame_Table[frameNumber].pageNumber = pageNumber;
	Frame_Table[frameNumber].isUsed = isUsed;
}


/******************************************************************
To free frames in physical memory that a process owns when it is terminated
Input: PID of the process wgich has been terminated
******************************************************************/
void free_Frame_Table(int pid){
	int i;
	for(i = 0;i < NUMBER_PHYSICAL_PAGES;i++){
		if(Frame_Table[i].PID == pid){
			Frame_Table[i].isUsed = 0;
			Frame_Table[i].PID = -1;
		}
	}
}


/******************************************************************
To check whether the page number of page fault is in a shared area.
Input: page number of the page fault
Output: 1, if the page in in a shared area; 0, if not
******************************************************************/
int is_page_number_in_shared_memory(int pageNumber){
	int startingPageNumber = current_PCB.sharedMemoryInfo->StartingPageNumber;
	int endingPageNumber = startingPageNumber + current_PCB.sharedMemoryInfo->PagesInSharedArea - 1;
	return pageNumber >= startingPageNumber && pageNumber <= endingPageNumber;
}


/******************************************************************
To add a page table of a process which defines a shared area to the PageTableQueue 
Input: page table to be added,
page status table to be added,
starting page number of the shared area,
number of pages in the shared area
******************************************************************/
void add_node_to_PageTable_Queue(void *PageTable,Page_Status *PageStatusTable,int startingPageNumber, int pagesInSharedArea,int sharedID){
	PageTable_node* node = (PageTable_node*)malloc(sizeof(PageTable_node));
	node->PageTable = PageTable;
	node->PageStatusTable = PageStatusTable;
	node->startingPageNumber = startingPageNumber;
	node->pagesInSharedArea = pagesInSharedArea;
	node->sharedID = sharedID;
	node->next = PageTable_Queue.head->next;
	PageTable_Queue.head->next = node;
	PageTable_Queue.size++;
}

/******************************************************************
To update all page tables in PageTableQueue when a page fault happening in shared area is handled
Input: the offset of the shared page, the new page table entry context of the page
******************************************************************/
void update_PageTable_Queue(int offsetInSharedArea, unsigned short entry){
	PageTable_node* p = PageTable_Queue.head->next;
	while(p != NULL){
		((UINT16*)(p->PageTable))[p->startingPageNumber + offsetInSharedArea] = entry;
		p = p->next;
	}
}