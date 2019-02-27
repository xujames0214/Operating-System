#ifndef MESSAGE_MANAGER_H
#define MESSAGE_MANAGER_H

typedef struct node_Message{
	int sourcePID;
	int targetPID;
	int length;
	char* context;
	struct node_Message *next;
	int broadcast;
}Message_node;

typedef struct{
	Message_node* head;
	int size;
}Message_queue;

Message_queue Message_Queue;

void init_Message_Queue();
Message_node *create_a_message_node(int sourcePID, int targetPID, int length, char context[]);
void add_message_to_Message_Queue(int sourcePID, int targetPID, int length, char context[]);
Message_node *get_out_a_node_from_Message_Queue(int sourcePID,int targetPID);
#endif