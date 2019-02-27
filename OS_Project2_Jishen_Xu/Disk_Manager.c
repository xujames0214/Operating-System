#include "global.h"
#include "Disk_Manager.h"
#include "Process_Manager.h"
#include "others.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


/******************************************************************
To initialize all Disk_Queue
******************************************************************/
void init_all_Disk_Queue(){
	int i;
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		Disk_Queue[i].head = (Disk_node*)malloc(sizeof(Disk_node));
		Disk_Queue[i].head->next = NULL;
		Disk_Queue[i].number = 0;
	}
	init_Disk_Map();
}
/************************************************************
To check if the target disk is empty.
Input: diskID
*************************************************************/
int is_Disk_Queue_empty(int diskID){
	return Disk_Queue[diskID].head->next == NULL;
}
/************************************************************
To add one diskNode(an IO request) into DiskQueue
input: a diskNode
*************************************************************/
void add_Disk_Node_to_Queue(Disk_node diskNode){
	Disk_node *node = (Disk_node*)malloc(sizeof(Disk_node));
	Disk_node *p = Disk_Queue[diskNode.diskID].head;
	
	node->PCB = diskNode.PCB;
	node->diskID = diskNode.diskID;
	node->sector = diskNode.sector;
	node->IOtype = diskNode.IOtype;
	node->context = diskNode.context;
	node->next = NULL;

	while(p->next != NULL){
		p = p->next;
	}
	p->next = node;
	Disk_Queue[diskNode.diskID].number++;
}
/************************************************************
To pop out first diskNode(IO request) in Disk Queue.
Input: diskID of target disk.
*************************************************************/
Disk_node pop_out_first_node_in_Disk_Queue(int diskID){
	Disk_node result;
	Disk_node *node = Disk_Queue[diskID].head->next;
	Disk_Queue[diskID].head->next = node->next;
	result.context = node->context;
	result.diskID = node->diskID;
	result.IOtype = node->IOtype;
	result.PCB = node->PCB;
	result.sector = node->sector;
	result.next = NULL;
	free(node);
	Disk_Queue[diskID].number--;
	return result;
}

/************************************************************
To get the first diskNode in Disk Queue(not pop out it, just want its information).
Input: diskID of target disk.
*************************************************************/
Disk_node get_first_node_in_Disk_Queue(int diskID){
	Disk_node *node = Disk_Queue[diskID].head->next;
	Disk_node result;
	result.context = node->context;
	result.diskID = node->diskID;
	result.IOtype = node->IOtype;
	result.PCB = node->PCB;
	result.sector = node->sector;
	result.next = NULL;
	return result;
}

/**************************************************************************
To delete PCBs whose parent PID is given in all Disk Queue.
Input: the parent PID.
***************************************************************************/
void del_PCB_with_parent_PID_in_Disk_Queue(int parent_PID){
	int i;
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		del_PCB_with_parent_PID_in_subDisk_Queue(i,parent_PID);
	}
}

/**************************************************************************
To delete PCBs whose parent PID is given in a single Disk Queue
Input: the parent PID, diskID of target disk.
***************************************************************************/
void del_PCB_with_parent_PID_in_subDisk_Queue(int diskID,int parent_PID){
	Disk_node* node = Disk_Queue[diskID].head;
	Disk_node* p;
	while(node->next != NULL){
		if(!node->next->PCB.terminated && node->next->PCB.parent_PID == parent_PID){
			p = node->next;
			node->next = p->next;
			PCB_status[p->PCB.PID] = 0;
			Disk_Queue[diskID].number--;
			free(p);	
		}else{
			node = node->next;
		}
	}
}

/**************************************************************************
To check if all Disk Queues are empty.
***************************************************************************/
int is_all_Disk_Queue_Empty(){
	int i;
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		if(!is_Disk_Queue_empty(i)) return 0;
	}
	return 1;
}
/**************************************************************************
To delete PCBs with given PID in all Disk Queues.
Input: the PID.
***************************************************************************/
int del_PCB_in_Disk_Queue_with_PID(int PID){
	int i;
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		if(del_PCB_in_subDisk_Queue_with_PID(i,PID)) return 1;
	}
	return 0;
}

/**************************************************************************
To delet PCB with given PID in a sigle Disk Queue.
Input: diskID, PID.
***************************************************************************/
int del_PCB_in_subDisk_Queue_with_PID(int diskID,int PID){
	Disk_node *node = Disk_Queue[diskID].head->next;
	while(node != NULL){
		if(!node->PCB.terminated && node->PCB.PID == PID){
			node->PCB.terminated = 1;
			PCB_status[PID] = 0;
			Disk_Queue[diskID].number--;
			return 1;
		}else{
			node = node->next;
		}
	}
	return 0;
}

/**************************************************************************
To get PID of a process with given name in all Disk Queues.
Input: the process name.
***************************************************************************/
int get_PID_in_Disk_Queue_with_name(char process_name[]){
	int i,pid;
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		pid = get_PID_in_subDisk_Queue_with_name(i,process_name);
		if(pid >= 0) return pid;
	}
	return -1;
}

/**************************************************************************
To get PID of a process with given name in a single disk.
Input: diskID, process name.
***************************************************************************/
int get_PID_in_subDisk_Queue_with_name(int diskID, char process_name[]){
	Disk_node *node = Disk_Queue[diskID].head->next;
	while(node != NULL){
		if(!node->PCB.terminated && strcmp(node->PCB.name,process_name) == 0){
			return node->PCB.PID;
		}else{
			node = node->next;
		}
	}
	return -1;
}
/****************************************************************************
To get a free block from disk. This function is used only by process test24 which requires a block for a victim frame to be swapped out.
Input: diskID, in which disk the block will be given
Output: sector number that is allocated.
*****************************************************************************/
int get_empty_block_from_disk_test24(int diskID){
	int j;
	for(j = 0;j < NUMBER_LOGICAL_SECTORS;j++){
		if(Disk_Map[diskID][j] == 0){
			Disk_Map[diskID][j] = 1;
			return j;
		}
	}
}
//for debugging
void init_Disk_Map(){
	int i,j;
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		for(j = 0;j < NUMBER_LOGICAL_SECTORS;j++){
			Disk_Map[i][j] = 0;
		}
	}
}

int isDiskFull(int diskID){
	int i;
	for(i = 0;i < NUMBER_LOGICAL_SECTORS;i++){
		if(Disk_Map[diskID][i] == 0) return 0;
	}
	return 1;
}
