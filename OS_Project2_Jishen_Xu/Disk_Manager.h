#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H
#include "global.h"
#include "Process_Manager.h"
#define READ_DISK_FLAG 0
#define WRITE_DISK_FLAG 1
#define CHECK_DISK_FLAG 2

typedef struct node_Disk{
	Process_Control_Block PCB;
	long diskID;
	long sector;
	struct node_Disk *next;
	int IOtype;
	char *context;
}Disk_node;

typedef struct{
	Disk_node *head;
	int number;
}Disk_queue;

Disk_queue Disk_Queue[MAX_NUMBER_OF_DISKS];
int Disk_Map[MAX_NUMBER_OF_DISKS][NUMBER_LOGICAL_SECTORS]; //for debugging

void init_all_Disk_Queue();
int is_Disk_Queue_empty(int diskID);
void add_Disk_Node_to_Queue(Disk_node diskNode);
Disk_node pop_out_first_node_in_Disk_Queue(int diskID);
Disk_node get_first_node_in_Disk_Queue(int diskID);
void del_PCB_with_parent_PID_in_Disk_Queue(int PID);
void del_PCB_with_parent_PID_in_subDisk_Queue(int diskID,int parent_PID);
int is_all_Disk_Queue_Empty();
int del_PCB_in_Disk_Queue_with_PID(int PID);
int del_PCB_in_subDisk_Queue_with_PID(int diskID,int PID);
int get_PID_in_Disk_Queue_with_name(char process_name[]);
int get_PID_in_subDisk_Queue_with_name(int diskID, char process_name[]);
int get_empty_block_from_disk_test24(int diskID);
//for debugging
void init_Disk_Map();
int isDiskFull(int diskID);

#endif