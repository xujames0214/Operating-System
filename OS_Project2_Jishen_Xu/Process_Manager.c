#include "Process_Manager.h"
#include "Timer_Manager.h"
#include "Disk_Manager.h"
#include "File_Manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/******************************************************************
INIT_PCB_QUEUE
This function is used to initialize the PCB_Queue(or Ready_Queue).
******************************************************************/
void init_Ready_Queue(){
	int i = 0;
	Ready_Queue.head = (PCB_node*)malloc(sizeof(PCB_node));
	Ready_Queue.head->next = NULL;
	Ready_Queue.number = 0;
	current_PCB.PID = -1;
	for(i = 0;i < MAXIMUM_PROCESS_NUMBER;i++){
		PCB_status[i] = 0;
	}
}
/****************************************************************
ADD_PCB_to_Queue
This fucntion is used to add the given PCB into the Ready_Queue.
The PCBs is sorted according to their each priority.
*****************************************************************/
void add_PCB_to_Ready_Queue(Process_Control_Block PCB){
	PCB_node *p = Ready_Queue.head;
	PCB_node *node = (PCB_node *)malloc(sizeof(PCB_node));
	node->PCB = PCB;
	while(p->next != NULL){
		if(PCB.priority > p->next->PCB.priority){
			break;
		}else{
			p = p->next;
		}
	}
	node->next = p->next;
	p->next = node;
	Ready_Queue.number++;
//	PCB_status[PCB.PID] = 1;
}

/***************************************************************
To get current PCB's PID.
****************************************************************/
int get_current_PID(){
	return current_PCB.PID;
}

/***************************************************************
Let one PCB to be the currently-running one.
****************************************************************/
void set_current_PCB(Process_Control_Block PCB){
	current_PCB = PCB;
//	PCB_status[current_PCB.PID] = 1;
}

/***************************************************************
To generate a new PID. Used when a new process is created.
****************************************************************/
int generate_a_new_PID(){
	int i;
	for(i = 0;i < MAXIMUM_PROCESS_NUMBER;i++){
		if(PCB_status[i] == 0){
			PCB_status[i] = 1;	
			return i;
		}
	}
	return -1;
}

/***************************************************************
To check whether the given name is used by one process in 
Ready_Queue, Timer_Queue or Disk_Queue. Used when a new process is created.
****************************************************************/
int is_process_name_used(char name[]){
	PCB_node *node;
	Timer_node *t;
	Disk_node *d;
	int i;
	//check current PCB
	if(get_PCB_number() == 0) return 0;
	if(current_PCB.PID >= 0 && strcmp(current_PCB.name,name) == 0){
		return 1;
	}
	
	//check Ready_Queue
	node = Ready_Queue.head->next;
	while(node != NULL){
		if(strcmp(node->PCB.name,name) == 0){
			return 1;
		}
		node = node->next;
	}
	
	//check Timer_Queue
	t = Timer_Queue.head->next;
	while(t != NULL){
		if(!t->timer.PCB.terminated && strcmp(t->timer.PCB.name,name) == 0){
			return 1;
		}
		t = t->next;
	}

	//check Disk_Queue
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		d = Disk_Queue[i].head->next;
		while(d != NULL){
			if(!d->PCB.terminated && strcmp(d->PCB.name,name) == 0){
				return 1;
			}
			d = d->next;
		}
	}
	
	return 0;
}

/***************************************************************
Delete the PCB with the given PID. It can be the current one or
one in the Ready_Queue.
****************************************************************/
int del_PCB_in_Ready_Queue_with_PID(int pid){
	PCB_node* node = Ready_Queue.head;
	PCB_node* p;
	if(get_PCB_number() == 0) return 0;
	if(pid >= MAXIMUM_PROCESS_NUMBER || pid < 0) return 0;
	if(pid == current_PCB.PID){
		PCB_status[pid] = 0;
		return 1;
	}

	while(node->next != NULL){
		if(node->next->PCB.PID == pid){
			p = node->next;
			node->next = p->next;
			free(p);
			Ready_Queue.number--;
			PCB_status[pid] = 0;
			return 1; //successfully delete
		}
		node = node->next;
	}
	return 0; //cannot find such pid
	
}
/***************************************************************
To check whether the Ready_Queue is empty.
****************************************************************/
int is_Ready_Queue_empty(){
	PCB_node *node = Ready_Queue.head->next;
	return node == NULL;
}
/***************************************************************
To get PID with the given name.
****************************************************************/
int get_PID_in_Ready_Queue_with_name(char name[]){
	PCB_node *node;
	if(get_PCB_number() == 0) return -1;
	if(current_PCB.PID >= 0 && strcmp(current_PCB.name,name) == 0) return current_PCB.PID;

	node = Ready_Queue.head->next;
	while(node != NULL){
		if(strcmp(node->PCB.name,name) == 0){
			return node->PCB.PID;
		}
		node = node->next;
	}
	return -1;
}
/***************************************************************
To get how many PCBs are in the system. Some of them may be in
Timer_Queue, Disk_Queue or Ready_Queue
****************************************************************/
int get_PCB_number(){
	int i;
	int num = 0;
	for(i = 0;i < MAXIMUM_PROCESS_NUMBER;i++){
		if(PCB_status[i] == 1) num++;
	}
	return num;
}

/***************************************************************
To get the current process's PCB.
****************************************************************/
Process_Control_Block get_out_current_PCB(){
	Process_Control_Block result = current_PCB;
	return result;
}

/***************************************************************
To check whether there is a process which is running
****************************************************************/
int no_PCB_running(){
	return current_PCB.PID < 0;
}

/***************************************************************
To pick a PCB in Ready_Queue to run.
****************************************************************/
Process_Control_Block get_next_ready_PCB(){
	PCB_node *node = Ready_Queue.head->next;
	Process_Control_Block result;
	Ready_Queue.head->next = node->next;
	result = node->PCB;
	free(node);
	Ready_Queue.number--;
	return result;

}

int get_head_PID_in_Ready_Queue(){
	return Ready_Queue.head->next->PCB.PID;
}
/***************************************************************
To check if no process is alive.
****************************************************************/
int is_PCB_empty(){
	return get_PCB_number() == 0;
}

/***************************************************************
To check if the number of processes is at its maximum.
****************************************************************/
int is_PCB_full(){
	return get_PCB_number() == MAXIMUM_PROCESS_NUMBER;
}

/***********************************************************************
To delete all PCBs whose parent PID is the given one in the Ready_Queue.
************************************************************************/
void del_PCB_with_parent_PID_in_Ready_Queue(int parent_PID){
	PCB_node *node = Ready_Queue.head, *p;
	if(current_PCB.parent_PID == parent_PID){
		PCB_status[current_PCB.PID] = 0;
	}
	while(node->next != NULL){
		if(node->next->PCB.parent_PID == parent_PID){
			p = node->next;
			node->next = p->next;
			PCB_status[p->PCB.PID] = 0;
			Ready_Queue.number--;
			free(p);
		}else{
			node = node->next;
		}
	}
}

/******************************************************************
To set PageTable in current PCB
Input: page number of the page,
isInDisk, whether the page has a copy in disk,
diskID, which disk stores this page,
sector, sector number of the page in disk
******************************************************************/
void set_currPCB_Page_Status_Table(int pageNumber, int isInDisk,int diskID,int sector){
	current_PCB.PageStatusTable[pageNumber].isInDisk = isInDisk;
	current_PCB.PageStatusTable[pageNumber].diskID = diskID;
	current_PCB.PageStatusTable[pageNumber].sector = sector;
}


/******************************************************************
To check the pid of a process is alive
******************************************************************/
int is_PID_alive(int pid){
	if(pid < 0 || pid >= MAXIMUM_PROCESS_NUMBER) return 0;
	return PCB_status[pid];
}


/******************************************************************
To get the address of PCB with its PID
Input: the PID of the process
Output: the pointer of PCB of the process
******************************************************************/
Process_Control_Block* get_PCB_with_PID(int pid){
	PCB_node *node;
	Timer_node *t;
	Disk_node *d;
	int i;
	if(pid == current_PCB.PID) return &current_PCB;
	node = Ready_Queue.head->next;
	while(node != NULL){
		if(node->PCB.PID == pid) return &(node->PCB);
		node = node->next;
	}
	t = Timer_Queue.head->next;
	while(t != NULL){
		if(t->timer.PCB.PID == pid) return &(t->timer.PCB);
		t = t->next;
	}
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		d = Disk_Queue[i].head->next;
		while(d != NULL){
			if(d->PCB.PID == pid) return &(d->PCB);
			d = d->next;
		}
	}
	return NULL;
}

/******************************************************************
To find a victim frame for page replacement, get the address of PCB of the process which owns the victime frame
Output: the PCB of process that owns the victim,
the page number of the victim in its page table,
whether it is a shared page.
******************************************************************/
Process_Control_Block* get_PCB_of_victim_frame(int *pageNumber, int *isShared){
	UINT16 page;
	int PN;
	Process_Control_Block *pcb;
	void *pageTable;
	int pid;
	while(TRUE){
		if(!Frame_Table[Frame_Table_point].isUsed) continue;
		PN = Frame_Table[Frame_Table_point].pageNumber;
		pid = Frame_Table[Frame_Table_point].PID;
		pcb = get_PCB_with_PID(pid);
		pageTable = pcb->PageTable;
		page = ((UINT16 *)pageTable)[PN];
		if((page & PTBL_VALID_BIT) != 0){
			if((page & PTBL_REFERENCED_BIT) != 0){
				page = page & (~PTBL_REFERENCED_BIT);
				((UINT16 *)pageTable)[PN] = page;
			}else{
				*pageNumber = PN;
				*isShared = Frame_Table[Frame_Table_point].isShared;
				return pcb;
			}
		}
		Frame_Table_point = (Frame_Table_point + 1) % NUMBER_PHYSICAL_PAGES;
	}
}

//used for debugging
void print_Ready_Queue(){
	PCB_node* node = Ready_Queue.head->next;
	printf("Current PCB Name:%s ",current_PCB.ContextID);
	while(node != NULL){
		printf("Name:%s ", node->PCB.name);
		node = node->next;
	}
	printf("Finish\n");
}

int count_PCB_in_Ready_Queue(){
	int num = 0;
	PCB_node* node = Ready_Queue.head->next;
	while(node != NULL){
		num++;
		node = node->next;
	}
	return num;
}

