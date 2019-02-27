#include "others.h"
#include "Process_Manager.h"
#include "Timer_Manager.h"
#include "Disk_Manager.h"
#include "Frame_Manager.h"
#include "protos.h"
#include "string.h"

/********************************************
To initialzie SP_INPUT_DATA
*********************************************/
void init_SP_INPUT_DATA(){
	SP.NumberOfDiskSuspendedProcesses = 0;
	SP.NumberOfMessageSuspendedProcesses = 0;
	SP.NumberOfProcSuspendedProcesses = 0;
	SP.NumberOfReadyProcesses = 0;
	SP.NumberOfRunningProcesses = 0;
	SP.NumberOfTerminatedProcesses = 0;
	SP.NumberOfTimerSuspendedProcesses = 0;
	SP_Counter = 0;
	SPPRINTLINE_ENABLED = 1;
	DISPLAY_SP_EACH_TIME = 0;
}

/********************************************
To set SP_INPUT_DATA
Input: targetAction, currently running process's PID, target process's PID
*********************************************/
void set_SP_INPUT_DATA(char targetAction[],int CurrentlyRunningPID,int targetPID){
	strcpy(SP.TargetAction,targetAction);
	SP.CurrentlyRunningPID = CurrentlyRunningPID;
	SP.TargetPID = targetPID;
	copy_Ready_queue_to_SP();
	copy_Timer_queue_to_SP();
	copy_Disk_queue_to_SP();
}
/********************************************
To copy Ready Queue's information to SP_INPUT_DATA
*********************************************/
void copy_Ready_queue_to_SP(){
	PCB_node *p = Ready_Queue.head->next;
	SP.NumberOfReadyProcesses = 0;
	while(p != NULL){
		SP.ReadyProcessPIDs[SP.NumberOfReadyProcesses] = p->PCB.PID;
		p = p->next;
		SP.NumberOfReadyProcesses++;
	}
}

/********************************************
To copy Timer Queue's information to SP_INPUT_DATA
*********************************************/
void copy_Timer_queue_to_SP(){
	Timer_node *p = Timer_Queue.head->next;
	SP.NumberOfTimerSuspendedProcesses = 0;
	while(p != NULL){
		SP.TimerSuspendedProcessPIDs[SP.NumberOfTimerSuspendedProcesses] = p->timer.PCB.PID;
		p = p->next;
		SP.NumberOfTimerSuspendedProcesses++;
	}
}
/********************************************
To copy Disk Queue's information to SP_INPUT_DATA
*********************************************/
void copy_Disk_queue_to_SP(){
	int i;
	Disk_node *p;
	SP.NumberOfDiskSuspendedProcesses = 0;
	for(i = 0;i < MAX_NUMBER_OF_DISKS;i++){
		p = Disk_Queue[i].head->next;
		while(p != NULL){
			SP.DiskSuspendedProcessPIDs[SP.NumberOfDiskSuspendedProcesses] = p->PCB.PID;
			p = p->next;
			SP.NumberOfDiskSuspendedProcesses++;
		}
	}
}
/********************************************
To display SP_INPUT_DATA
*********************************************/
void display_SP_INPUT_DATA(char targetAction[],int CurrentlyRunningPID,int targetPID){
	if(SPPRINTLINE_ENABLED){
		if(DISPLAY_SP_EACH_TIME){
			set_SP_INPUT_DATA(targetAction,CurrentlyRunningPID,targetPID);
			SPPrintLine(&SP);
		}else{
			if(SP_Counter == 0){
				set_SP_INPUT_DATA(targetAction,CurrentlyRunningPID,targetPID);
				SPPrintLine(&SP);
			}
			SP_Counter = (SP_Counter + 1) % 20;
		}
	}
}

void init_MP_INPUT_DATA(){
	int i;
	for(i = 0;i < NUMBER_PHYSICAL_PAGES;i++){
		MP.frames[i].InUse = FALSE;
		MP.frames[i].LogicalPage = 0;
		MP.frames[i].Pid = 0;
		MP.frames[i].State = 0;
	}
	MP_Counter = 0;
	MPPRINTLINE_ENABLED = 1;
	DISPLAY_MP_EACH_TIME = 0;
	
}

void set_MP_INPUT_DATA(){
	int i,pid,pageNumber,isUsed;
	void *PageTable = NULL;
	Process_Control_Block *PCB;
	UINT16 page;
	for(i = 0;i < NUMBER_PHYSICAL_PAGES;i++){
		isUsed = Frame_Table[i].isUsed;
		MP.frames[i].InUse = isUsed;
		if(!isUsed) continue;
		MP.frames[i].LogicalPage = Frame_Table[i].pageNumber;
		MP.frames[i].Pid = Frame_Table[i].PID;
		pageNumber = MP.frames[i].LogicalPage;
		pid = MP.frames[i].Pid;
		PCB = get_PCB_with_PID(pid);
		PageTable = PCB->PageTable;
		page = ((UINT16*)PageTable)[pageNumber];
		MP.frames[i].State = page / 8192;//(8192 = 2^13)
	}
}

void display_MP_INPUT_DATA(){
	if(MPPRINTLINE_ENABLED){
		if(DISPLAY_MP_EACH_TIME){
			set_MP_INPUT_DATA();
			MPPrintLine(&MP);
		}else{
			if(MP_Counter == 0){
				set_MP_INPUT_DATA();
				MPPrintLine(&MP);
			}
			MP_Counter = (MP_Counter + 1) % 20;
		}
	}
}