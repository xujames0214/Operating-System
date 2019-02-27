#include "Message_Manager.h"
#include "Process_Manager.h"
#include "others.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/******************************************************************
To initialize the message queue
******************************************************************/
void init_Message_Queue(){
	Message_Queue.head = (Message_node *)malloc(sizeof(Message_node));
	Message_Queue.head->next = NULL;
	Message_Queue.size = 0;
}

/******************************************************************
To create a message node.
Input: sourcePID, PID of source process
targetPID, PID of targer process
length, the length of the message,
context, array which stores the message's context
Output: pointer of a new message node
******************************************************************/
Message_node *create_a_message_node(int sourcePID, int targetPID, int length, char context[]){
	Message_node* node = (Message_node *)malloc(sizeof(Message_node));
	if(targetPID == -1)	node->broadcast = 1;
	else node->broadcast = 0;
	node->sourcePID = sourcePID;
	node->targetPID = targetPID;
	node->length = length;
	node->context = (char *)malloc(length * sizeof(char));
	strcpy(node->context,context);
	node->next = NULL;
	return node;
}

/******************************************************************
To add a message node in the Message Queue
Input: sourcePID, PID of source process
targetPID, PID of targer process
length, the length of the message,
context, array which stores the message's context
******************************************************************/
void add_message_to_Message_Queue(int sourcePID, int targetPID, int length, char context[]){
	Message_node* newNode, *p;
	newNode = create_a_message_node(sourcePID, targetPID, length, context);
	p = Message_Queue.head->next;
	Message_Queue.head->next = newNode;
	newNode->next = p;
	Message_Queue.size++;
}


/******************************************************************
To get out a node from Message Queue when a process tries to receive a message from another process.
Input: sourcePID, PID of source process
targetPID, PID of targer process
Output: the pointer of the message node whose source PID and targetPID are given
******************************************************************/
Message_node *get_out_a_node_from_Message_Queue(int sourcePID,int targetPID){
	Message_node* p = Message_Queue.head;
	Message_node* q = NULL;
	if(sourcePID == -1){
		while(p->next != NULL){
			if(p->next->broadcast || p->next->targetPID == targetPID){
				q = p->next;
				p->next = q->next;
				Message_Queue.size--;
				return q;
			}else{
				p = p->next;
			}
		}
	}else{
		while(p->next != NULL){
			if(p->next->sourcePID = sourcePID && p->next->targetPID == targetPID){
				q = p->next;
				p->next = q->next;
				Message_Queue.size--;
				return q;
			}else{
				p = p->next;
			}
		}
	}

	return NULL;
}