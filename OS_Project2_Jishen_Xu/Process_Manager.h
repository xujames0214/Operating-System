#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H
#include "File_Manager.h"
#include "Frame_Manager.h"
#define MAXIMUM_PROCESS_NUMBER 15  //the OS can only contain 15 processes at maximum
/*******************************************************
Here we define the structure of PCB and Ready_Queue, storing the PCBs
which is ready to run.
********************************************************/
typedef struct{
	int PID;
	long ContextID;
	void *PageTable;
	Page_Status *PageStatusTable;
	int priority;
	char name[100];
	int parent_PID;
	int terminated;	//if this attribute is 1, it means that this PCB has been terminated. Drop this PCB if possible.
	Directory_File currentDir;
	DirHeaderLoc *dirHeaderLocStack_head;
	Shared_Memory_Info *sharedMemoryInfo;
}Process_Control_Block;     //Process Control Block


typedef struct node_PCB{
	Process_Control_Block PCB;
	struct node_PCB* next;
}PCB_node;

typedef struct{
	PCB_node* head;
	int number;
}Ready_queue;

Ready_queue Ready_Queue;  //the Ready_Queue
Process_Control_Block current_PCB;  //the currently-running PCB
int PCB_status[MAXIMUM_PROCESS_NUMBER]; //used to identify whether a PID(0 ~ MAXIMUM_PROCESS_NUMBER - 1) is used

//initialize PCB queue
void init_Ready_Queue();

void add_PCB_to_Ready_Queue(Process_Control_Block PCB);//Priority should be considered!! Later finish it

int get_current_PID();
void set_current_PCB(Process_Control_Block PCB);
int generate_a_new_PID();
int is_process_name_used(char name[]);
int del_PCB_in_Ready_Queue_with_PID(int pid);
int is_Ready_Queue_empty();
int get_PID_in_Ready_Queue_with_name(char name[]);
int get_PCB_number();
Process_Control_Block get_out_current_PCB();
int no_PCB_running();
Process_Control_Block get_next_ready_PCB();
int get_head_PID_in_Ready_Queue();
int is_PCB_empty();
int is_PCB_full();
void del_PCB_with_parent_PID_in_Ready_Queue(int parent_PID);
void set_currPCB_Page_Status_Table(int pageNumber, int isInDisk,int diskID,int sector);
int is_PID_alive(int pid);
Process_Control_Block* get_PCB_with_PID(int pid);
Process_Control_Block* get_PCB_of_victim_frame(int *pageNumber, int *isShared);
//used for debugging
void print_Ready_Queue();
int count_PCB_in_Ready_Queue();
#endif