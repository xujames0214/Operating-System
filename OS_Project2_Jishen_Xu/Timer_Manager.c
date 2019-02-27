#include "Timer_Manager.h"
#include "Process_Manager.h"
#include "others.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/***************************************************************
To initialize the Timer_Queue
****************************************************************/
void init_Timer_Queue(){
	Timer_Queue.head = (Timer_node *)malloc(sizeof(Timer_node));
	Timer_Queue.head->next = NULL;
	Timer_Queue.number = 0;
}

/***************************************************************
To add one timer into the Timer_Queue
****************************************************************/
void add_Timer_to_Queue(Timer timer){
	Timer_node *p;
	Timer_node *node;
	p = Timer_Queue.head;
	display_SP_INPUT_DATA("Sleep",get_current_PID(),timer.PCB.PID);
	while(p->next != NULL){
		if(timer.end_time < p->next->timer.end_time){
			break;
		}else{
			p = p->next;
		}
	}

	node = (Timer_node *)malloc(sizeof(Timer_node));
	node->timer = timer;
	node->next = p->next;
	p->next = node;

	Timer_Queue.number++;
}

/***************************************************************
To remove the first timer in Timer_Queue which is also the 
soonest-awake one.
****************************************************************/
void remove_First_Timer_from_Queue(){
	Timer_node *node;
	if(Timer_Queue.head->next == NULL) return;
	node = Timer_Queue.head->next;
	Timer_Queue.head->next = node->next;
	free(node);
	Timer_Queue.number--;
}

/***************************************************************
To chech if the Timer_Queue is empty.
****************************************************************/
int is_Timer_Queue_Empty(){
	return Timer_Queue.head->next == NULL;
}

Timer get_First_Timer_in_Queue(){
	return Timer_Queue.head->next->timer;
}

/***************************************************************
To pick the first-awake timer in the Timer_Queue
****************************************************************/
Process_Control_Block pop_out_first_PCB_in_Timer_Queue(){
	Timer_node *node = Timer_Queue.head->next;
	Process_Control_Block result;
	Timer_Queue.head->next = node->next;
	Timer_Queue.number--;
	result = node->timer.PCB;
	free(node);
	return result;
}

/***************************************************************
To get the timer's end_time (when it wakes up).
****************************************************************/
long get_first_timer_end_time(){
	Timer_node *node = Timer_Queue.head->next;
	if(node == NULL) return 0;
	return node->timer.end_time;
}

/***************************************************************
To delete the PCB with the given PID in the Timer_Queue.
****************************************************************/
int del_PCB_in_Timer_Queue_with_PID(int pid){
	Timer_node *node = Timer_Queue.head, *p;
	while(node->next != NULL){
		if(node->next->timer.PCB.PID == pid){
			p = node->next;
			node->next = p->next;
			PCB_status[pid] = 0;
			free(p);
			Timer_Queue.number--;
			return 1;
		}else{
			node = node->next;
		}
	}
	return 0;
}

/***************************************************************
To get PID with the given name in Timer_Queue.
****************************************************************/
int get_PID_in_Timer_Queue_with_name(char name[]){
	Timer_node *node = Timer_Queue.head->next;
	while(node != NULL){
		if(strcmp(node->timer.PCB.name,name) == 0){
			return node->timer.PCB.PID;
		}else{
			node = node->next;
		}
	}
	return -1;
}

/***************************************************************
To remove PCB with a given paren PID in Timer_Queue
****************************************************************/
void del_PCB_with_parent_PID_in_Timer_Queue(int parent_PID){
	Timer_node *node = Timer_Queue.head, *p;
	while(node->next != NULL){
		if(node->next->timer.PCB.parent_PID == parent_PID){
			p = node->next;
			node->next = p->next;
			PCB_status[p->timer.PCB.PID] = 0;
			Timer_Queue.number--;
			free(p);
		}else{
			node = node->next;
		}
	}
}