#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H
#include "Process_Manager.h"
/****************************************************
Here we define the structure of timer and Timer_Queue
*****************************************************/
typedef struct{
	Process_Control_Block PCB;
	long end_time;
}Timer;

typedef struct node_Timer{
	Timer timer;
	struct node_Timer* next;
}Timer_node;

typedef struct{
	Timer_node *head;
	int number;
}Timer_queue;

Timer_queue Timer_Queue;

void init_Timer_Queue();
void add_Timer_to_Queue(Timer timer);
void remove_First_Timer_from_Queue();
int is_Timer_Queue_Empty();
Timer get_First_Timer_in_Queue();
Process_Control_Block pop_out_first_PCB_in_Timer_Queue();
long get_first_timer_end_time();
int del_PCB_in_Timer_Queue_with_PID(int pid);
int get_PID_in_Timer_Queue_with_name(char name[]);
void del_PCB_with_parent_PID_in_Timer_Queue(int parent_PID);
#endif