#ifndef OTHERS_H
#define OTHERS_H
#include "syscalls.h"

int SPPRINTLINE_ENABLED;
int MPPRINTLINE_ENABLED;
int DISPLAY_SP_EACH_TIME;
int DISPLAY_MP_EACH_TIME;
SP_INPUT_DATA SP;
MP_INPUT_DATA MP;
int SP_Counter;
int MP_Counter;

void init_SP_INPUT_DATA();

void set_SP_INPUT_DATA(char targetAction[],int CurrentlyRunningPID,int targetPID);
void copy_Ready_queue_to_SP();
void copy_Timer_queue_to_SP();
void copy_Disk_queue_to_SP();

void display_SP_INPUT_DATA(char targetAction[],int CurrentlyRunningPID,int targetPID);

void init_MP_INPUT_DATA();
void set_MP_INPUT_DATA();
void display_MP_INPUT_DATA();
#endif