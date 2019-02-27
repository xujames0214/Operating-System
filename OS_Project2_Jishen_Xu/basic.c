/************************************************************************

This code forms the base of the operating system you will
build.  It has only the barest rudiments of what you will
eventually construct; yet it contains the interfaces that
allow test.c and z502.c to be successfully built together.

Revision History:
1.0 August 1990
1.1 December 1990: Portability attempted.
1.3 July     1992: More Portability enhancements.
Add call to SampleCode.
1.4 December 1992: Limit (temporarily) printout in
interrupt handler.  More portability.
2.0 January  2000: A number of small changes.
2.1 May      2001: Bug fixes and clear STAT_VECTOR
2.2 July     2002: Make code appropriate for undergrads.
Default program start is in test0.
3.0 August   2004: Modified to support memory mapped IO
3.1 August   2004: hardware interrupt runs on separate thread
3.11 August  2004: Support for OS level locking
4.0  July    2013: Major portions rewritten to support multiple threads
4.20 Jan     2015: Thread safe code - prepare for multiprocessors
************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include			 "Process_Manager.h"
#include			 "Timer_Manager.h"
#include			 "Disk_Manager.h"
#include			 "File_Manager.h"
#include			 "Frame_Manager.h"
#include			 "Message_Manager.h"
#include			 "others.h"
#include             <stdlib.h>
#include             <ctype.h>

#define				TEST_PROCESS_PRIORITY 10

//  Allows the OS and the hardware to agree on where faults occur
extern void *TO_VECTOR[];

char *call_names[] = { "mem_read ", "mem_write", "read_mod ", "get_time ",
"sleep    ", "get_pid  ", "create   ", "term_proc", "suspend  ",
"resume   ", "ch_prior ", "send     ", "receive  ", "PhyDskRd ",
"PhyDskWrt", "def_sh_ar", "Format   ", "CheckDisk", "Open_Dir ",
"OpenFile ", "Crea_Dir ", "Crea_File", "ReadFile ", "WriteFile",
"CloseFile", "DirContnt", "Del_Dir  ", "Del_File " };

int is_test24_running;
int is_test28_running;

void create_process_and_run(char name[],long identifier,int priority);
void create_process(char name[],long identifier, int priority, long* pid, long* ErrorReturned);
void terminate_process(long PID,long* ErrorReturned);
void terminate_machine();

void sleep(long time);
void get_PID(char processName[],long *PID, long* ErrorReturned);

void switch_process();
void waste_time();
long get_current_time();

void check_disk(long diskID,long* ErrorReturned);
void format_disk(long diskID,long* ErrorReturned);
void open_dir(long diskID,char name[], long* ErrorReturned);
void load_BITMAP_of_disk(int diskID);

void write_or_read_disk(long diskID,long sector,unsigned char* context,int FLAG);
void write_to_disk(long diskID,long sector,unsigned char* context);
void read_from_disk(long diskID,long sector,unsigned char* context);

void write_to_disk_no_switch(long diskID,long sector,unsigned char* context);
void read_from_disk_no_switch(long diskID,long sector,unsigned char* context);
void write_or_read_disk_no_switch(long diskID,long sector,unsigned char* context,int FLAG);
int get_empty_block_from_disk(int diskID);
int get_empty_block_from_swap_area(int diskID);
void open_or_create_dir_file(char name[],long *Inode,long* ErrorReturned,int FLAG);
void write_or_read_file(long Inode,long fileLogicalBlock,unsigned char* buffer,long* ErrorReturned,int FLAG);
void close_file(long Inode,long* ErrorReturned);

void define_shared_memory(int StartingAddressOfSharedArea, int PagesInSharedArea, char AreaTag[], int* SharedID, long *ErrorReturned);
void send_message(int targetPID, char messageBuffer[], int messageSendLength, long *ErrorReturned);
void receive_message(int sourcePID, char messageBuffer[], int messageReceiveLength, int* messageSendLength, int* messageSendPID, long *ErrorReturned);
void start_test_process(int argc, char *argv[]);
//for debugging
void print_bitmap(int diskID);

/************************************************************************
INTERRUPT_HANDLER
When the Z502 gets a hardware interrupt, it transfers control to
this routine in the OS.
************************************************************************/
void InterruptHandler(void) {
	INT32 DeviceID;
	INT32 Status;

	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

	static BOOL remove_this_in_your_code = TRUE; /** TEMP **/
	static INT32 how_many_interrupt_entries = 0; /** TEMP **/

	Timer timer;
	Process_Control_Block PCB;
	long tmp;
	Disk_node diskNode;
	int diskID;
	// Get cause of interrupt
	mmio.Mode = Z502GetInterruptInfo;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;
	Status = mmio.Field2;
	if (mmio.Field4 != ERR_SUCCESS) {
		printf(
			"The InterruptDevice call in the InterruptHandler has failed.\n");
		printf("The DeviceId and Status that were returned are not valid.\n");
	}

	/** REMOVE THE NEXT SIX LINES **/
//	how_many_interrupt_entries++; /** TEMP **/
//	if (remove_this_in_your_code && (how_many_interrupt_entries < 10)) {
//		printf("Interrupt_handler: Found device ID %d with status %d\n",
//			(int)mmio.Field1, (int)mmio.Field2);
//	}
	
	if(mmio.Field4 == ERR_SUCCESS){
		DeviceID = mmio.Field1;
		if(DeviceID == TIMER_INTERRUPT){
			//get the current time
			tmp = get_current_time();
			while(!is_Timer_Queue_Empty() && get_first_timer_end_time() <= tmp){
				PCB = pop_out_first_PCB_in_Timer_Queue();
				add_PCB_to_Ready_Queue(PCB);
			}

			//check if other timer exists in timer queue
			if(!is_Timer_Queue_Empty()){
				//start the timer again
				mmio.Mode = Z502Start;
				mmio.Field1 = get_first_timer_end_time() - tmp; 
				mmio.Field2 = mmio.Field3 = 0;
				MEM_WRITE(Z502Timer, &mmio);
			}
		}else if(DeviceID >= DISK_INTERRUPT && DeviceID <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS - 1){
			if(is_test28_running) return;
			diskID = DeviceID - DISK_INTERRUPT;
			diskNode = pop_out_first_node_in_Disk_Queue(diskID);
			add_PCB_to_Ready_Queue(diskNode.PCB);
			if(!is_Disk_Queue_empty(diskID)){
				diskNode = get_first_node_in_Disk_Queue(diskID);
				if(diskNode.IOtype == READ_DISK_FLAG || diskNode.IOtype == WRITE_DISK_FLAG){
					mmio.Mode = (diskNode.IOtype == READ_DISK_FLAG) ? Z502DiskRead : Z502DiskWrite;
					mmio.Field1 = diskNode.diskID;
					mmio.Field2 = diskNode.sector;
					mmio.Field3 = (long)(diskNode.context);
					mmio.Field4 = 0;
					MEM_WRITE(Z502Disk,&mmio);
				}else if(diskNode.IOtype == CHECK_DISK_FLAG){
					mmio.Mode = Z502CheckDisk;
					mmio.Field1 = diskNode.diskID;
					mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
					MEM_WRITE(Z502Disk, &mmio);
				}
			}
		}
	}
	

}           // End of InterruptHandler

			/************************************************************************
			FAULT_HANDLER
			The beginning of the OS502.  Used to receive hardware faults.
			************************************************************************/

void FaultHandler(void) {
	INT32 DeviceID;
	INT32 Status;
	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware
	int currPageNumber;
	void* currPageTable, *PageTableOfVictimFrame;
	int is_current_page_in_disk;
	int is_page_of_victim_frame_in_disk;
	int newFrameNumber;
	UINT16 page;
	int pageNumberOfVictim;
	int victimFrameNumber;
	unsigned char context[PGSIZE];
	unsigned char victimFrameContext[PGSIZE];
	int diskID,sector;
	Process_Control_Block* PCB;
	Page_Status* currPageStatusTable;
	Page_Status* PageSatusTableOfVictimFrame;
	int offsetInSharedArea, isShared;
	// Get cause of interrupt
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	mmio.Mode = Z502GetInterruptInfo;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;

	Status = mmio.Field2;
	printf("Fault_handler: Found vector type %d with value %d\n", DeviceID,
			Status);

	if(DeviceID == INVALID_MEMORY){
		currPageNumber = Status;
		if(currPageNumber >= NUMBER_VIRTUAL_PAGES){
			terminate_machine();
			return;
		}
		if(is_page_number_in_shared_memory(currPageNumber)){
			currPageTable = current_PCB.PageTable;
			currPageStatusTable = current_PCB.PageStatusTable;
			offsetInSharedArea = currPageNumber - current_PCB.sharedMemoryInfo->StartingPageNumber;
			is_current_page_in_disk = currPageStatusTable[currPageNumber].isInDisk;
			if(is_current_page_in_disk){
				diskID = current_PCB.PageStatusTable[currPageNumber].diskID;
				sector = current_PCB.PageStatusTable[currPageNumber].sector;
				read_from_disk_no_switch(diskID,sector,context);
			}
			newFrameNumber = get_new_frame(current_PCB.PID,currPageNumber,TRUE);
			if(newFrameNumber >= 0){
				if(is_current_page_in_disk)	Z502WritePhysicalMemory(newFrameNumber, context);
				update_PageTable_Queue(offsetInSharedArea,(unsigned short)(PTBL_VALID_BIT | newFrameNumber));
				display_MP_INPUT_DATA();
			}else{
				PCB = get_PCB_of_victim_frame(&pageNumberOfVictim,&isShared);
				PageTableOfVictimFrame = PCB->PageTable;
				PageSatusTableOfVictimFrame = PCB->PageStatusTable;
				page = ((UINT16*)PageTableOfVictimFrame)[pageNumberOfVictim];
				if(isShared){
					update_PageTable_Queue(offsetInSharedArea,0);
					display_MP_INPUT_DATA();
				}else{
					((UINT16*)PageTableOfVictimFrame)[pageNumberOfVictim] = 0;
					display_MP_INPUT_DATA();
				}
				victimFrameNumber = page & PTBL_PHYS_PG_NO;
				if((page & PTBL_MODIFIED_BIT) != 0){
					Z502ReadPhysicalMemory(victimFrameNumber, victimFrameContext);
					is_page_of_victim_frame_in_disk = PageSatusTableOfVictimFrame[pageNumberOfVictim].isInDisk;
					if(is_page_of_victim_frame_in_disk){
						diskID = PageSatusTableOfVictimFrame[pageNumberOfVictim].diskID;
						sector = PageSatusTableOfVictimFrame[pageNumberOfVictim].sector;
					}else{
						diskID = 1;
						sector = get_empty_block_from_disk_test24(diskID);
					
						PageSatusTableOfVictimFrame[pageNumberOfVictim].diskID = diskID;
						PageSatusTableOfVictimFrame[pageNumberOfVictim].sector = sector;
						PageSatusTableOfVictimFrame[pageNumberOfVictim].isInDisk = 1;
						PageSatusTableOfVictimFrame[pageNumberOfVictim].isSharedMemory = isShared;
						write_to_disk_no_switch(diskID,sector,victimFrameContext);	
					}
				}
				if(is_current_page_in_disk)	Z502WritePhysicalMemory(victimFrameNumber, context);
				update_PageTable_Queue(offsetInSharedArea, (unsigned short)(PTBL_VALID_BIT | victimFrameNumber));
				display_MP_INPUT_DATA();
				set_Frame_Table(victimFrameNumber,current_PCB.PID,currPageNumber,TRUE);
				Frame_Table[victimFrameNumber].isShared = TRUE;
			}
		}else{
			currPageTable = current_PCB.PageTable;
			is_current_page_in_disk = current_PCB.PageStatusTable[currPageNumber].isInDisk;
			if(is_current_page_in_disk){
				diskID = current_PCB.PageStatusTable[currPageNumber].diskID;
				sector = current_PCB.PageStatusTable[currPageNumber].sector;
				read_from_disk(diskID,sector,context);
			}
			newFrameNumber = get_new_frame(current_PCB.PID,currPageNumber,FALSE);
			
			if(newFrameNumber >= 0){
				if(is_current_page_in_disk)	Z502WritePhysicalMemory(newFrameNumber, context);
				((UINT16*)currPageTable)[currPageNumber] = (unsigned short)(PTBL_VALID_BIT | newFrameNumber);
				display_MP_INPUT_DATA();
			}else{
				PCB = get_PCB_of_victim_frame(&pageNumberOfVictim,&isShared);
				PageTableOfVictimFrame = PCB->PageTable;
				PageSatusTableOfVictimFrame = PCB->PageStatusTable;
				page = ((UINT16*)PageTableOfVictimFrame)[pageNumberOfVictim];
				((UINT16*)PageTableOfVictimFrame)[pageNumberOfVictim] = 0;
				display_MP_INPUT_DATA();
				victimFrameNumber = page & PTBL_PHYS_PG_NO;
				if(((page & PTBL_MODIFIED_BIT) != 0) || PageSatusTableOfVictimFrame[pageNumberOfVictim].isInDisk == 0){                                                                                        
					Z502ReadPhysicalMemory(victimFrameNumber, victimFrameContext);
					is_page_of_victim_frame_in_disk = PageSatusTableOfVictimFrame[pageNumberOfVictim].isInDisk;
					if(is_page_of_victim_frame_in_disk){
						diskID = PageSatusTableOfVictimFrame[pageNumberOfVictim].diskID;
						sector = PageSatusTableOfVictimFrame[pageNumberOfVictim].sector;
					}else{
						if(is_test24_running){
							diskID = 1;
							sector = get_empty_block_from_disk_test24(diskID);
						}else{
							diskID = 1;
							sector = get_empty_block_from_swap_area(diskID);
						}
						PageSatusTableOfVictimFrame[pageNumberOfVictim].diskID = diskID;
						PageSatusTableOfVictimFrame[pageNumberOfVictim].sector = sector;
						PageSatusTableOfVictimFrame[pageNumberOfVictim].isInDisk = 1;
					}
					write_to_disk(diskID,sector,victimFrameContext);	
				}
				if(is_current_page_in_disk)	Z502WritePhysicalMemory(victimFrameNumber, context);
				((UINT16*)currPageTable)[currPageNumber] = (unsigned short)(PTBL_VALID_BIT | victimFrameNumber);
				display_MP_INPUT_DATA();
				set_Frame_Table(victimFrameNumber,current_PCB.PID,currPageNumber,TRUE);
			}
		}
	}
} // End of FaultHandler




  /************************************************************************
  SVC
  The beginning of the OS502.  Used to receive software interrupts.
  All system calls come to this point in the code and are to be
  handled by the student written code here.
  The variable do_print is designed to print out the data for the
  incoming calls, but does so only for the first ten calls.  This
  allows the user to see what's happening, but doesn't overwhelm
  with the amount of data.
  ************************************************************************/


void svc(SYSTEM_CALL_DATA *SystemCallData) {
	short call_type;
	static short do_print = 10;
	short i;
	
	call_type = (short)SystemCallData->SystemCallNumber;
	if (do_print > 0) {
		printf("SVC handler: %s\n", call_names[call_type]);
		for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
			//Value = (long)*SystemCallData->Argument[i];
			printf("Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
				(unsigned long)SystemCallData->Argument[i],
				(unsigned long)SystemCallData->Argument[i]);
		}
		do_print--;
	}

	switch(call_type){
	case SYSNUM_GET_TIME_OF_DAY:
		*(long *)SystemCallData->Argument[0] = get_current_time(); 
		break;
	case SYSNUM_TERMINATE_PROCESS:
		terminate_process((long)(SystemCallData->Argument[0]),SystemCallData->Argument[1]);
		break;
	case SYSNUM_SLEEP:
		sleep((long)(SystemCallData->Argument[0]));
		break;
	case SYSNUM_GET_PROCESS_ID:
		get_PID((char*)(SystemCallData->Argument[0]),SystemCallData->Argument[1],SystemCallData->Argument[2]);
		break;
	case SYSNUM_PHYSICAL_DISK_WRITE:
		write_to_disk((long)(SystemCallData->Argument[0]),(long)(SystemCallData->Argument[1]),(char*)(SystemCallData->Argument[2]));
		break;
	case SYSNUM_PHYSICAL_DISK_READ:
		read_from_disk((long)(SystemCallData->Argument[0]),(long)(SystemCallData->Argument[1]),(char*)(SystemCallData->Argument[2]));
		break;
	case SYSNUM_CHECK_DISK:
		check_disk((long)(SystemCallData->Argument[0]),SystemCallData->Argument[1]);
		break;
	case SYSNUM_CREATE_PROCESS:
		create_process((char*)SystemCallData->Argument[0],(long)SystemCallData->Argument[1],(int)SystemCallData->Argument[2],SystemCallData->Argument[3], SystemCallData->Argument[4]);
		break;
	case SYSNUM_FORMAT:
		format_disk((long)SystemCallData->Argument[0],SystemCallData->Argument[1]);
		break;
	case SYSNUM_OPEN_DIR:
		open_dir((long)SystemCallData->Argument[0],(char*)SystemCallData->Argument[1],SystemCallData->Argument[2]);
		break;
	case SYSNUM_CREATE_DIR:
		open_or_create_dir_file((char*)SystemCallData->Argument[0],NULL,SystemCallData->Argument[1],CREATE_DIR_FLAG);
		break;
	case SYSNUM_CREATE_FILE:
		open_or_create_dir_file((char*)SystemCallData->Argument[0],NULL,SystemCallData->Argument[1],CREATE_FILE_FLAG);
		break;
	case SYSNUM_OPEN_FILE:
		open_or_create_dir_file((char*)SystemCallData->Argument[0],SystemCallData->Argument[1],SystemCallData->Argument[2],OPEN_FILE_FLAG);
		break;
	case SYSNUM_WRITE_FILE:
		write_or_read_file((long)SystemCallData->Argument[0],(long)SystemCallData->Argument[1],(char *)SystemCallData->Argument[2],SystemCallData->Argument[3],WRITE_FILE_FLAG);
		break;
	case SYSNUM_READ_FILE:
		write_or_read_file((long)SystemCallData->Argument[0],(long)SystemCallData->Argument[1],(char *)SystemCallData->Argument[2],SystemCallData->Argument[3],READ_FILE_FLAG);
		break;
	case SYSNUM_CLOSE_FILE:
		close_file((long)SystemCallData->Argument[0],SystemCallData->Argument[1]);
		break;
	case SYSNUM_DIR_CONTENTS:
		open_or_create_dir_file("",NULL,SystemCallData->Argument[0],DIR_CONTENTS_FLAG);
		break;
	case SYSNUM_DEFINE_SHARED_AREA:
		define_shared_memory((int)SystemCallData->Argument[0],(int)SystemCallData->Argument[1],(char*)SystemCallData->Argument[2],(int *)SystemCallData->Argument[3],SystemCallData->Argument[4]);
		break;
	case SYSNUM_SEND_MESSAGE:
		send_message((int)SystemCallData->Argument[0],(char *)SystemCallData->Argument[1],(int)SystemCallData->Argument[2],SystemCallData->Argument[3]);
		break;
	case SYSNUM_RECEIVE_MESSAGE:
		receive_message((int)SystemCallData->Argument[0],(char *)SystemCallData->Argument[1],(int)SystemCallData->Argument[2],(int *)SystemCallData->Argument[3],(int *)SystemCallData->Argument[4],SystemCallData->Argument[5]);
		break;
	default:
		printf("ERROR!  call_type not recognized!\n");
		printf("Call_type is - %i\n", call_type);
	}


}                                               // End of svc

												/************************************************************************
												osInit
												This is the first routine called after the simulation begins.  This
												is equivalent to boot code.  All the initial OS components can be
												defined and initialized here.
												************************************************************************/
void osInit(int argc, char *argv[]) {
	
	INT32 i;
	MEMORY_MAPPED_IO mmio;

	//Do some initialisazions
	init_Ready_Queue();
	init_Timer_Queue();
	init_all_Disk_Queue();
	init_File_Manager();
	init_Frame_Manager();
	init_Message_Queue();
	init_SP_INPUT_DATA();
	init_MP_INPUT_DATA();
	is_test24_running = 0;
    is_test28_running = 0;
	// Demonstrates how calling arguments are passed thru to here

	printf("Program called with %d arguments:", argc);
	for (i = 0; i < argc; i++)
		printf(" %s", argv[i]);
	printf("\n");
	printf("Calling with argument 'sample' executes the sample program.\n");

	// Here we check if a second argument is present on the command line.
	// If so, run in multiprocessor mode.  Note - sometimes people change
	// around where the "M" should go.  Allow for both possibilities
	if (argc > 2) {
		if ((strcmp(argv[1], "M") == 0) || (strcmp(argv[1], "m") == 0)) {
			strcpy(argv[1], argv[2]);
			strcpy(argv[2], "M\0");
		}
		if ((strcmp(argv[2], "M") == 0) || (strcmp(argv[2], "m") == 0)) {
			printf("Simulation is running as a MultProcessor\n\n");
			mmio.Mode = Z502SetProcessorNumber;
			mmio.Field1 = MAX_NUMBER_OF_PROCESSORS;
			mmio.Field2 = (long)0;
			mmio.Field3 = (long)0;
			mmio.Field4 = (long)0;
			MEM_WRITE(Z502Processor, &mmio);   // Set the number of processors
		}
	}
	else {
		printf("Simulation is running as a UniProcessor\n");
		printf(
			"Add an 'M' to the command line to invoke multiprocessor operation.\n\n");
	}

	//          Setup so handlers will come to code in base.c

	TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR] = (void *)InterruptHandler;
	TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)FaultHandler;
	TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR] = (void *)svc;

	//  Determine if the switch was set, and if so go to demo routine.

	
	if ((argc > 1) && (strcmp(argv[1], "sample") == 0)) {
		create_process_and_run("sample",(long)SampleCode,TEST_PROCESS_PRIORITY);
	} // End of handler for sample code - This routine should never return here
	  //  By default test0 runs if no arguments are given on the command line
	  //  Creation and Switching of contexts should be done in a separate routine.
	  //  This should be done by a "OsMakeProcess" routine, so sthat
	  //  test0 runs on a process recognized by the operating system.

	start_test_process(argc,argv);
}	// End of osInit

/***************************************************************
CREATE_PROCESS_AND_RUN
This function is used to create each test process and run them.
Input: process name, function address, process priority.
****************************************************************/
void create_process_and_run(char name[],long identifier,int priority){
	Process_Control_Block PCB;
	MEMORY_MAPPED_IO mmio;
	void *PageTable;
	
	PageTable = (void *)calloc(2, NUMBER_VIRTUAL_PAGES);
	PCB.PageTable = PageTable;
	PCB.PageStatusTable = (Page_Status *)calloc(sizeof(Page_Status), NUMBER_VIRTUAL_PAGES);
	strcpy(PCB.name,name);
	
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = (long)identifier;
	mmio.Field3 = (long)PageTable;

	MEM_WRITE(Z502Context, &mmio);   // Start this new Context Sequence

	PCB.ContextID = mmio.Field1;
	PCB.PID = generate_a_new_PID();
	PCB.parent_PID = -1;
	PCB.priority = priority;
	PCB.terminated = 0;
	PCB.dirHeaderLocStack_head = (DirHeaderLoc *)malloc(sizeof(DirHeaderLoc));
	PCB.dirHeaderLocStack_head->next = NULL;
	PCB.sharedMemoryInfo = (Shared_Memory_Info *)calloc(sizeof(Shared_Memory_Info),1);
	display_SP_INPUT_DATA("Create",get_current_PID(),PCB.PID);
	set_current_PCB(PCB);
	mmio.Mode = Z502StartContext;
	// Field1 contains the value of the context returned in the last call
	// Suspends this current thread
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	MEM_WRITE(Z502Context, &mmio);     // Start up the context
}

/***************************************************************
CREATE_PROCESS
This function is used to create process by the currently-running
process but we do not run the created one. The newly created process
is added to Ready_Queue.
Input: process name, function address.
Output: PID of new process, success or not.
****************************************************************/

void create_process(char name[],long identifier, int priority, long* pid, long* ErrorReturned){
	Process_Control_Block PCB;
	MEMORY_MAPPED_IO mmio;
	void *PageTable;
	
	//check if priority is legal, process name is used before or PCB queue is full
	if(priority < 0 || is_process_name_used(name) || is_PCB_full()){
		*ErrorReturned = 1;
		return;
	}
	
	PageTable = (void *)calloc(2, NUMBER_VIRTUAL_PAGES);
	PCB.PageTable = PageTable;
	PCB.PageStatusTable = (Page_Status *)calloc(sizeof(Page_Status), NUMBER_VIRTUAL_PAGES);
	PCB.priority = priority;
	PCB.terminated = 0;
	strcpy(PCB.name,name);
	
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = (long)identifier;
	mmio.Field3 = (long)PageTable;
	MEM_WRITE(Z502Context, &mmio);   // Start this new Context Sequence

	PCB.ContextID = mmio.Field1;
	PCB.PID = generate_a_new_PID();
	PCB.parent_PID = get_current_PID();
	PCB.dirHeaderLocStack_head = (DirHeaderLoc *)malloc(sizeof(DirHeaderLoc));
	PCB.dirHeaderLocStack_head->next = NULL;
	PCB.sharedMemoryInfo = (Shared_Memory_Info *)calloc(sizeof(Shared_Memory_Info),1);
	display_SP_INPUT_DATA("Create",get_current_PID(),PCB.PID);
	add_PCB_to_Ready_Queue(PCB);
	
	*pid = PCB.PID;
	*ErrorReturned = ERR_SUCCESS;
}
/***************************************************
Terminate Process
This function is used to terminate one process. 
Input: the PID of process to be terminated.
Output: success or not.
****************************************************/
void terminate_process(long PID,long* ErrorReturned){
	MEMORY_MAPPED_IO mmio;
	int currentPID = get_current_PID();
	if(PID == -1 || PID == currentPID || PID == -2){
		if(PID == -2){ 
			terminate_machine();
		}
		free_Frame_Table(currentPID);
		del_PCB_in_Ready_Queue_with_PID(currentPID);
		if(!is_Ready_Queue_empty()){
			switch_process();
		}else if(!is_Timer_Queue_Empty() || !is_all_Disk_Queue_Empty()){
			while(is_Ready_Queue_empty()){
				CALL(waste_time);
			}
			if(!is_Ready_Queue_empty()){
				switch_process();
			}else{
				mmio.Mode = Z502Action;
				mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
				MEM_WRITE(Z502Halt, &mmio);
			}
		}else{ //if no PCB is ready or waiting, there is no process left that can be executed. Terminate the machine;
			terminate_machine();
		}
	}else{
		if(del_PCB_in_Ready_Queue_with_PID(PID) || del_PCB_in_Timer_Queue_with_PID(PID) || del_PCB_in_Disk_Queue_with_PID(PID)){
			*ErrorReturned = ERR_SUCCESS;
		}else{
			*ErrorReturned = 1;
		}
	}
}

void terminate_machine(){
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502Action;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_WRITE(Z502Halt, &mmio);
}
/***************************************************
Sleep
This function is used to make currently running process sleep.
It will put current process in Timer Queue and do switch process.
Input: how long the process will sleep.
****************************************************/
void sleep(long time){
	MEMORY_MAPPED_IO mmio;
	Timer timer;
	timer.end_time = get_current_time() + time;
	timer.PCB = get_out_current_PCB();
	if(is_Timer_Queue_Empty() || timer.end_time < get_first_timer_end_time()){ 
		add_Timer_to_Queue(timer);
		//start the timer
		mmio.Mode = Z502Start;
		mmio.Field1 = time; 
		mmio.Field2 = mmio.Field3 = 0;
		MEM_WRITE(Z502Timer, &mmio);
	}else{
		add_Timer_to_Queue(timer);
	}
	while(is_Ready_Queue_empty()){
		CALL(waste_time);
	}
	switch_process();
}

/***************************************************
Get Pid
This function is used to get PID of target Process.
Input: the name pf process.
Output: the PID, success or not.
****************************************************/
void get_PID(char processName[],long *PID, long* ErrorReturned){
	char process_name[50];
	strcpy(process_name,processName);
	if(strcmp(process_name,"") == 0){ //get_current_PID;
		*PID = get_current_PID();
	}else{
		*PID = get_PID_in_Ready_Queue_with_name(process_name);
		if(*PID < 0){
			*PID = get_PID_in_Timer_Queue_with_name(process_name);
		}
		if(*PID < 0){
			*PID = get_PID_in_Disk_Queue_with_name(process_name);
		}
	}	
	if(*PID < 0){
		*ErrorReturned = 1;
	}else{
		*ErrorReturned = ERR_SUCCESS;
	}
}

/***************************************************************
SWITCH_PROCESS
This function is used for switching process. When the currently 
running process is waiting(sleeping or waiting for an I/O response)
or terminated, we pick another process from Ready_Queue and run it.
****************************************************************/
void switch_process(){
	Process_Control_Block PCB;
	MEMORY_MAPPED_IO mmio;
	display_SP_INPUT_DATA("Dispatch",get_current_PID(),get_head_PID_in_Ready_Queue());
	PCB = get_next_ready_PCB();
	set_current_PCB(PCB);
	mmio.Mode = Z502StartContext;
	mmio.Field1 = PCB.ContextID;
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	mmio.Field3 = mmio.Field4 = 0;
	MEM_WRITE(Z502Context, &mmio);     // Start up the context
}

/***************************************************************
WASTE_TIME
This function does nothing. We use it when all processes are
waiting and none can be run. We put this function in a WHILE loop
to wait until some process can be run.
****************************************************************/
void waste_time(){
	//do nothing.
}
/***************************************************************
GET_CURRENT_TIME
This function is used to get the current machine time.
****************************************************************/
long get_current_time(){
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502Clock, &mmio);
	return mmio.Field1;
}
/***************************************************
Check_Disk
This function is used to write out the contests of the disk into a file named ¡°CheckDiskData.¡±
Input: the ID of target disk.
Output: success or not.
****************************************************/
void check_disk(long diskID,long* ErrorReturned){
	MEMORY_MAPPED_IO mmio;
	*ErrorReturned = ERR_SUCCESS;
	mmio.Mode = Z502CheckDisk;
	mmio.Field1 = diskID;
	mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_WRITE(Z502Disk, &mmio);	
}
/************************************************************
Write_or_Read Disk
This function is used to write or read a sector to the disk.
It will put current process in Disk Queue and do switch process.
Input: diskID, sector number, context address, FLAG indicating write or read.
*************************************************************/
void write_or_read_disk(long diskID,long sector,unsigned char* context,int FLAG){
	Disk_node diskNode;
	MEMORY_MAPPED_IO mmio;
	diskNode.PCB = get_out_current_PCB();
	diskNode.diskID = diskID;
	diskNode.sector = sector;
	diskNode.IOtype = FLAG;
	diskNode.context = context;
	display_SP_INPUT_DATA("Disk",get_current_PID(),diskNode.PCB.PID);
	if(is_Disk_Queue_empty(diskID)){
		add_Disk_Node_to_Queue(diskNode);
		mmio.Mode = (FLAG == WRITE_DISK_FLAG ? Z502DiskWrite : Z502DiskRead);
		mmio.Field1 = diskID;
		mmio.Field2 = sector;
		mmio.Field3 = (long)context;
		mmio.Field4 = 0;
		MEM_WRITE(Z502Disk, &mmio);
	}else{
		add_Disk_Node_to_Queue(diskNode);
	}
	while(is_Ready_Queue_empty()){
		CALL(waste_time);
	}
	switch_process();
}
/************************************************************
Write_to_Disk
This function is used to write a sector to the disk.
It calls the function 'write_or_read_disk'.
Input: diskID, sector number, write buffer address.
*************************************************************/
void write_to_disk(long diskID,long sector,unsigned char* context){
	write_or_read_disk(diskID,sector,context,WRITE_DISK_FLAG);
}
/************************************************************
Read_from_Disk
This function is used to read a sector from the disk.
It calls the function 'write_or_read_disk'.
Input: diskID, sector number, read buffer address.
*************************************************************/
void read_from_disk(long diskID,long sector,unsigned char* context){
	write_or_read_disk(diskID,sector,context,READ_DISK_FLAG);
}
/************************************************************
FORMAT_DISK
This function is used to format a disk.
Input: diskID.
Output: success or not.
*************************************************************/
void format_disk(long diskID,long* ErrorReturned){
	MEMORY_MAPPED_IO mmio;
	int i,j;
	unsigned char sector0[PGSIZE];
	unsigned char bitmap[NUMBER_OF_SECTORS_IN_BITMAP][PGSIZE];
	unsigned char rootDirHeader[PGSIZE];
	unsigned char rootDirIndexLevel1[PGSIZE];
	unsigned char rootDirIndexLevel2[PGSIZE];
	long currentTime;
	int rootInode;
	if(diskID < 0 || diskID >= MAX_NUMBER_OF_DISKS){
		*ErrorReturned = 1;
		return;
	}
	//Initializ sector0
	sector0[0] = diskID; //DiskId
	sector0[1] = NUMBER_OF_SECTORS_IN_BITMAP / 4; //Bitmap Size, 16 blocks assigned to bitmap
	sector0[2] = NUMBER_OF_SECTORS_IN_ROOTDIR_HEADER; //RootDir Header Size, 1 block assigned to RootDir Header
	sector0[3] = (unsigned char)NUMBER_OF_SECTORS_IN_SWAP / 4; //Swap Size, 1024 blocks assigned to Swap Size	
	sector0[4] = NUMBER_LOGICAL_SECTORS % 256; //Disk Length, byte4 is LSB, byte5 is MSB, total length is 2048 blocks
	sector0[5] = NUMBER_LOGICAL_SECTORS / 256;	
	sector0[6] = BITMAP_START_LOCATION % 256; //Bitmap Location, starts from the 8th block;
	sector0[7] = BITMAP_START_LOCATION / 256;	
	sector0[8] = ROOTDIR_HEADER_START_LOCATION % 256; //Root Dir location, starts from the 24th block
	sector0[9] = ROOTDIR_HEADER_START_LOCATION / 256;	
	sector0[10] = SWAP_START_LOCATION % 256; //Swap Location, starts from the 1024th block
	sector0[11] = SWAP_START_LOCATION / 256;	
	sector0[12] = sector0[13] = sector0[14] = sector0[15] = 0; //Reserved
	
	//Initialize bitmap
	for(i = 0;i < NUMBER_OF_SECTORS_IN_BITMAP;i++){
		for(j = 0;j < PGSIZE;j++){
			bitmap[i][j] = 0;
		}
	}

	bitmap[0][0] = 1;	//sector0
	bitmap[0][1] = 255;	//bitmap, the first 8 bits
	bitmap[0][2] = 255; //bitmap, the second 8 bits
	bitmap[0][3] = 1;	//rootDir header
	//bitmap[1][0] = 15;	//swap
	bitmap[2][0] = 3;	//rootDir Index level1 & level2
	
	//write sector0 and bitmap to Disk
	write_to_disk(diskID,0,sector0);
	for(i = 0;i < NUMBER_OF_SECTORS_IN_BITMAP;i++){
		write_to_disk(diskID,BITMAP_START_LOCATION + i,bitmap[i]);
	}

	//Creat the root dir.
	rootInode = generate_a_new_Inode(); //'The Inode of Root Directory
	currentTime = get_current_time();
	set_df_header(rootDirHeader,rootInode,"root",DIR_FLAG,PARENT_INODE_FOR_ROOT,currentTime,ROOTDIR_INDEXLEVEL1_LOCATION,0);
	
	write_to_disk(diskID,ROOTDIR_HEADER_START_LOCATION,rootDirHeader);

	for(i = 0;i < PGSIZE;i++){
		rootDirIndexLevel1[i] = 0;
		rootDirIndexLevel2[i] = 0;
	}
	rootDirIndexLevel1[0] = ROOTDIR_INDEXLEVEL2_LOCATION % 256; // rootDirIndexLevel2_loc = 257
	rootDirIndexLevel1[1] = ROOTDIR_INDEXLEVEL2_LOCATION / 256;
	write_to_disk(diskID,ROOTDIR_INDEXLEVEL1_LOCATION,rootDirIndexLevel1);
	write_to_disk(diskID,ROOTDIR_INDEXLEVEL2_LOCATION,rootDirIndexLevel2);
	load_BITMAP_of_disk(diskID);
}
/************************************************************
OPEN_DIR
This function is used to open a directory.
Input: diskID, directory name.
Output: success or not.
*************************************************************/
void open_dir(long diskID,char name[], long* ErrorReturned){
	unsigned char sector0[PGSIZE];
	int rootDirHeader_loc;
	unsigned char rootDirHeader[PGSIZE];
	
	Directory_File rootDir;

	int parentDir_header_loc;
	unsigned char parentDir_header[PGSIZE];
	Directory_File parentDir;
	
	int currentDiskID;
	*ErrorReturned = ERR_SUCCESS;
	if(diskID < -1 || diskID >= MAX_NUMBER_OF_DISKS){
		*ErrorReturned = 1;
		return;
	}
	//open root dir
	if(diskID >=0 && diskID < MAX_NUMBER_OF_DISKS && strcmp(name,"root") == 0){
		read_from_disk(diskID,0,sector0);
		rootDirHeader_loc = sector0[8] + sector0[9] * 256;
		read_from_disk(diskID,rootDirHeader_loc,rootDirHeader);

		rootDir = get_dir_file_from_header(rootDirHeader,rootDirHeader_loc,diskID);
		set_currentDir(rootDir);
		return;
	}

	//Open a directory in current directory
	else if(diskID == -1 && strcmp(name,"..") != 0){
		open_or_create_dir_file(name,NULL,ErrorReturned,OPEN_DIR_FLAG);
		return;
	}
	
	else if(diskID == -1 && strcmp(name,"..") == 0){
		currentDiskID = current_PCB.currentDir.diskID;
		if(is_currentDir_root()){
			*ErrorReturned = 1;
			return;
		}
		parentDir_header_loc = pop_dirHeaderLocation();
		read_from_disk(currentDiskID,parentDir_header_loc,parentDir_header);
		parentDir = get_dir_file_from_header(parentDir_header,parentDir_header_loc,currentDiskID);
		set_currentDir(parentDir);
	}
}
/************************************************************
LOAD_BITMAP_OF_DISK
This function is used to load the bitmap to memory, 
which helps to allocate disk space more conviniently.
Input: diskID
*************************************************************/
void load_BITMAP_of_disk(int diskID){
	int i,j;
	for(i = 0;i < NUMBER_OF_SECTORS_IN_BITMAP;i++){
		read_from_disk(diskID,BITMAP_START_LOCATION + i,BITMAP[diskID][i]);
	}
}
/************************************************************
GET_EMPTY_BLOCK_FROM_DISK
This function is used to allocate a free block to someone who calls it.
Input: diskID
Output: sector number of the new free block
*************************************************************/
int get_empty_block_from_disk(int diskID){
	int i,j,k,m,n;
	int num;
	int bit;
	int flag;
	int binary[BYTE_SIZE];
	for(i = 0;i < NUMBER_OF_SECTORS_IN_BITMAP;i++){
		for(j = 0;j < PGSIZE;j++){
			num = BITMAP[diskID][i][j];
			for(k = 0;k < BYTE_SIZE;k++){
				binary[k] = num % 2;
				num /= 2;
			}
			for(k = 0;k < BYTE_SIZE;k++){
				if(binary[k] == 0){
					binary[k] = 1;
					num = 0;
					n = 1;
					for(m = 0;m < BYTE_SIZE;m++){
						num += binary[m] * n;
						n *= 2;
					}
					BITMAP[diskID][i][j] = num;
					write_to_disk(diskID,BITMAP_START_LOCATION + i,BITMAP[diskID][i]);
					return (i * PGSIZE * BYTE_SIZE + j * BYTE_SIZE + k);
				}
			}
		}
	}
}
/************************************************************
GET_EMPTY_BLOCK_FROM_SWAP_AREA
This function is used to allocate a free block from swap area of a disk to someone who calls it.
Input: diskID
Output: sector number of the new free block
*************************************************************/
int get_empty_block_from_swap_area(int diskID){
	int i,j,k,m,n;
	int num;
	int bit;
	int flag;
	int binary[BYTE_SIZE];
	for(i = SWAP_START_LOCATION / 128;i < NUMBER_OF_SECTORS_IN_BITMAP;i++){
		for(j = 0;j < PGSIZE;j++){
			num = BITMAP[diskID][i][j];
			for(k = 0;k < BYTE_SIZE;k++){
				binary[k] = num % 2;
				num /= 2;
			}
			for(k = 0;k < BYTE_SIZE;k++){
				if(binary[k] == 0){
					binary[k] = 1;
					num = 0;
					n = 1;
					for(m = 0;m < BYTE_SIZE;m++){
						num += binary[m] * n;
						n *= 2;
					}
					BITMAP[diskID][i][j] = num;
					write_to_disk(diskID,BITMAP_START_LOCATION + i,BITMAP[diskID][i]);
					return (i * PGSIZE * BYTE_SIZE + j * BYTE_SIZE + k);
				}
			}
		}
	}
}

/************************************************************
OPEN_OR_CREATE_DIR_FILE
This function is used to open or create a directory or file.
Input: name of target dir or file, Inode of target dir or file, 
FLAG indication what kind of operation(OPEN_DIR, OPEN_FILE, CREATE_DIR, CREATE_FILE, DIR_CONTENS).
Output: success or not
*************************************************************/
void open_or_create_dir_file(char name[],long *Inode,long* ErrorReturned,int FLAG){
	unsigned char currentDir_header[PGSIZE];
	int currentDir_header_loc;
	char currentDir_name[10];

	unsigned char header[PGSIZE];
	unsigned char indexLevel1[PGSIZE];
	unsigned char indexLevel2[PGSIZE];
	Directory_File df, newDf;
	int i,j,k;
	
	int indexLevel1_loc;
	int indexLevel2_loc;
	int headerLoc;
	int vacant_space_in_indexLevel1 = -1;
	int vacant_space_in_indexLevel2 = -1;
	int vacant_indexLevel2_loc;
	unsigned char vacant_indexLevel2[PGSIZE];
	
	int new_df_header_loc;
	unsigned char new_df_header[PGSIZE];
	int new_df_Inode;
	int new_df_indexLevel1_loc;
	int new_df_indexLevel2_loc;
	unsigned char new_df_indexLevel1[PGSIZE];
	unsigned char new_df_indexLevel2[PGSIZE];
	
	int new_indexLevel2_loc;
	unsigned char new_indexLevel2[PGSIZE];
	
	char dfName[10];
	int currentDiskID;

	char dirContents[1000];
	char buffer[20];

	if(!is_valid_name_for_dir_file(name)){
		*ErrorReturned = 1;
		return;
	}
	
	currentDiskID = current_PCB.currentDir.diskID;
	indexLevel1_loc = current_PCB.currentDir.indexLocation;
	read_from_disk(currentDiskID,indexLevel1_loc,indexLevel1);
	
	if(FLAG == DIR_CONTENTS_FLAG){
		currentDir_header_loc = current_PCB.currentDir.headerLocation;
		read_from_disk(currentDiskID,currentDir_header_loc,currentDir_header);
		copy_name_from_header(currentDir_name,currentDir_header);
		sprintf(dirContents,"Contents of Directory %s:\n",currentDir_name);
		strcat(dirContents,"Inode,\tFilename,\tD/F,\tCreation Time,\tFile Size\n");
	}

	for(i = 0;i < NUMBER_OF_ADDRESS_IN_INDEXLEVEL1 * 2;i+=2){
		indexLevel2_loc = indexLevel1[i] + indexLevel1[i+1] * 256;	
		if(indexLevel2_loc != 0){
			read_from_disk(currentDiskID,indexLevel2_loc,indexLevel2);
			for(j = 0;j < NUMBER_OF_ADDRESS_IN_INDEXLEVEL2 * 2;j+=2){
				headerLoc = indexLevel2[j] + indexLevel2[j+1] * 256;
				if(headerLoc != 0){
					read_from_disk(currentDiskID,headerLoc,header);
					df = get_dir_file_from_header(header,headerLoc,currentDiskID);
					copy_name_from_header(dfName,header);

					if(FLAG == DIR_CONTENTS_FLAG){				
						sprintf(buffer,"%d\t%s\t\t%c\t%d\t\t",df.Inode,dfName,(df.Dir_or_File == DIR_FLAG ? 'D' : 'F'),df.creationTime);
						strcat(dirContents,buffer);
						if(df.Dir_or_File == DIR_FLAG) strcat(dirContents,"--");
						else{
							sprintf(buffer,"%d",df.size);
							strcat(dirContents,buffer);
						}
						strcat(dirContents,"\n");
					}

					if(strcmp(dfName,name) == 0){
						if(FLAG == OPEN_DIR_FLAG && df.Dir_or_File == DIR_FLAG){
							push_dirHeaderLocation(current_PCB.currentDir.headerLocation);
							set_currentDir(df);
							return;
						}else if(FLAG == CREATE_DIR_FLAG && df.Dir_or_File == DIR_FLAG){
							*ErrorReturned = 1;
							return;
						}else if(FLAG == CREATE_FILE_FLAG && df.Dir_or_File == FILE_FLAG){
							*ErrorReturned = 1;
							return;
						}else if(FLAG == OPEN_FILE_FLAG && df.Dir_or_File == FILE_FLAG){
							add_Inode_to_OpenedInode_Queue(df.Inode,df.headerLocation,df.diskID);
							*Inode = df.Inode;
							return;
						}
					}				
				}else{
					if(vacant_space_in_indexLevel2 < 0){
						vacant_space_in_indexLevel2 = j;
						for(k = 0;k < PGSIZE;k++){
							vacant_indexLevel2[k] = indexLevel2[k];
						}
						vacant_indexLevel2_loc = indexLevel2_loc;
					}
				}
			}
		}else{
			if(vacant_space_in_indexLevel2 < 0) vacant_space_in_indexLevel2 = i;
		}
	}

	if(FLAG == DIR_CONTENTS_FLAG){
		printf("%s",dirContents);
		return;
	}

	//creat new dir or file when the target is not found
	new_df_header_loc = get_empty_block_from_disk(currentDiskID);
	new_df_Inode = generate_a_new_Inode();
	new_df_indexLevel1_loc = get_empty_block_from_disk(currentDiskID);
	new_df_indexLevel2_loc = get_empty_block_from_disk(currentDiskID);
	set_df_header(new_df_header,new_df_Inode,name,(FLAG == CREATE_DIR_FLAG ? DIR_FLAG : FILE_FLAG),current_PCB.currentDir.Inode,get_current_time(),new_df_indexLevel1_loc,0);
	write_to_disk(currentDiskID,new_df_header_loc,new_df_header);
	newDf = get_dir_file_from_header(new_df_header,new_df_header_loc,currentDiskID);
	for(i = 0;i < PGSIZE;i++){
		new_df_indexLevel1[i] = 0;
		new_df_indexLevel2[i] = 0;
	}		
	new_df_indexLevel1[0] = new_df_indexLevel2_loc % 256;
	new_df_indexLevel1[1] = new_df_indexLevel2_loc / 256;
	write_to_disk(currentDiskID,new_df_indexLevel1_loc,new_df_indexLevel1);
	write_to_disk(currentDiskID,new_df_indexLevel2_loc,new_df_indexLevel2);

	if(vacant_space_in_indexLevel2 >= 0){
		vacant_indexLevel2[vacant_space_in_indexLevel2] = new_df_header_loc % 256;
		vacant_indexLevel2[vacant_space_in_indexLevel2 + 1] = new_df_header_loc / 256;
		write_to_disk(currentDiskID,vacant_indexLevel2_loc,vacant_indexLevel2);
	}else if(vacant_space_in_indexLevel1 >= 0){
		new_indexLevel2_loc = get_empty_block_from_disk(currentDiskID);
		indexLevel1[vacant_space_in_indexLevel1] = new_indexLevel2_loc % 256;
		indexLevel1[vacant_space_in_indexLevel1 + 1] = new_indexLevel2_loc / 256;
		write_to_disk(currentDiskID,indexLevel1_loc,indexLevel1);
		for(i = 0;i < PGSIZE;i++){
			new_indexLevel2[i] = 0;
		}
		new_indexLevel2[0] = new_df_header_loc % 256;
		new_indexLevel2[1] = new_df_header_loc / 256;
		write_to_disk(currentDiskID,new_indexLevel2_loc,new_indexLevel2);
	}
	if(FLAG == OPEN_DIR_FLAG){
		push_dirHeaderLocation(current_PCB.currentDir.headerLocation);
		set_currentDir(newDf);
	}else if(FLAG == OPEN_FILE_FLAG){
		add_Inode_to_OpenedInode_Queue(newDf.Inode,newDf.headerLocation,newDf.diskID);
		*Inode = newDf.Inode;
	}
}

/************************************************************
WRITE_OR_READ_FILE
This function is used to write or read a block in a given file.
Input: Inode of target file, the logical address of target block, IO buffer address, FLAG indicting what kind of opeation(WRITE or READ).
Output: success or not
*************************************************************/
void write_or_read_file(long Inode,long fileLogicalBlock,unsigned char* buffer,long* ErrorReturned,int FLAG){
	int i;
	int headerLoc;
	int diskID;
	unsigned char header[PGSIZE];
	Directory_File file;
	int indexLevel1_loc;
	int indexLevel2_loc;
	unsigned char indexLevel1[PGSIZE];
	unsigned char indexLevel2[PGSIZE];
	int dataLoc_in_indexLeve1;
	int dataLoc_in_indexLeve2;
	int dataBlock_loc;
	*ErrorReturned = ERR_SUCCESS;

	get_headerLoc_from_OpenedInode_Queue(Inode,&headerLoc,&diskID);
	if(headerLoc < 0 || diskID < 0){
		*ErrorReturned = 1;
		return;
	}
	read_from_disk(diskID,headerLoc,header);
	file = get_dir_file_from_header(header,headerLoc,diskID);
	
	if(file.Dir_or_File == DIR_FLAG){
		*ErrorReturned = 1;
		return;
	}

	indexLevel1_loc = file.indexLocation;
	read_from_disk(diskID,indexLevel1_loc,indexLevel1);
	dataLoc_in_indexLeve1 = (fileLogicalBlock / NUMBER_OF_ADDRESS_IN_INDEXLEVEL2) * 2;
	dataLoc_in_indexLeve2 = (fileLogicalBlock % NUMBER_OF_ADDRESS_IN_INDEXLEVEL2) * 2;
	indexLevel2_loc = indexLevel1[dataLoc_in_indexLeve1] + indexLevel1[dataLoc_in_indexLeve1 + 1] * 256;

	if(indexLevel2_loc == 0){	
		if(FLAG == READ_FILE_FLAG){
			for(i = 0;i <PGSIZE;i++){
				buffer[i] = '\0';
				return;
			}
		}else if(FLAG == WRITE_FILE_FLAG){
			indexLevel2_loc = get_empty_block_from_disk(diskID);
			dataBlock_loc = get_empty_block_from_disk(diskID);
			write_to_disk(diskID,dataBlock_loc,buffer);
			for(i = 0;i < PGSIZE;i++){
				indexLevel2[i] = 0;
			}
			indexLevel2[dataLoc_in_indexLeve2] = dataBlock_loc % 256;
			indexLevel2[dataLoc_in_indexLeve2 + 1] = dataBlock_loc / 256;
			write_to_disk(diskID,indexLevel2_loc,indexLevel2);

			indexLevel1[dataLoc_in_indexLeve1] = indexLevel2_loc % 256;
			indexLevel1[dataLoc_in_indexLeve1 + 1] = indexLevel2_loc / 256;
			write_to_disk(diskID,indexLevel1_loc,indexLevel1);
		}
	}else{
		read_from_disk(diskID,indexLevel2_loc,indexLevel2);
		dataBlock_loc = indexLevel2[dataLoc_in_indexLeve2] + indexLevel2[dataLoc_in_indexLeve2 + 1] * 256;
		if(dataBlock_loc == 0){
			if(FLAG == READ_FILE_FLAG){
				for(i = 0;i <PGSIZE;i++){
					buffer[i] = '\0';
					return;
				}
			}else if(FLAG == WRITE_FILE_FLAG){	
				dataBlock_loc = get_empty_block_from_disk(diskID);
				write_to_disk(diskID,dataBlock_loc,buffer);
				indexLevel2[dataLoc_in_indexLeve2] = dataBlock_loc % 256;
				indexLevel2[dataLoc_in_indexLeve2 + 1] = dataBlock_loc / 256;
				write_to_disk(diskID,indexLevel2_loc,indexLevel2);	
			}
		}else{
			if(FLAG == READ_FILE_FLAG){
				read_from_disk(diskID,dataBlock_loc,buffer);
			}else if(FLAG == WRITE_FILE_FLAG){
				write_to_disk(diskID,dataBlock_loc,buffer);			
			}
		}
	}
	if(FLAG == WRITE_DISK_FLAG){
		increase_fileSize_by_one_sector(header);
		write_to_disk(diskID,headerLoc,header);
	}
}
/************************************************************
CLOSE_FILE
This function is used to close a file which is opened before.
Input: Inode of target file.
Output: success or not.
*************************************************************/
void close_file(long Inode,long* ErrorReturned){
	if(!del_Indoe_from_OpenedInode_Queue(Inode)){
		*ErrorReturned = 1;
	}
	*ErrorReturned = ERR_SUCCESS;
}
/************************************************************
DEFINE SHARED MEMORY
This function is used to define a shared area in memory for different processes.
Input: 
StartingAddressOfSharedArea, the virtual memory address at which the shared area should begin.
PagesInSharedArea, the number of pages that are to be in the shared area
AreaTag, a label used to enumerate which shared area is being asked for
output:
NumberPreviousSharers, the number of other processes that currently have this area_tag shared area DEFINED
ErrorReturned, success or not
*************************************************************/
void define_shared_memory(int StartingAddressOfSharedArea, int PagesInSharedArea, char AreaTag[], int* SharedID, long* ErrorReturned){
	int sharedID, i;
	int startingPageNumber = StartingAddressOfSharedArea / PGSIZE;
	if(startingPageNumber < 0 || startingPageNumber >= NUMBER_VIRTUAL_PAGES || startingPageNumber + PagesInSharedArea >= NUMBER_VIRTUAL_PAGES){
		*ErrorReturned = 1;
		return;
	}
	current_PCB.sharedMemoryInfo->StartingPageNumber = startingPageNumber;
	current_PCB.sharedMemoryInfo->PagesInSharedArea = PagesInSharedArea;
	strcpy(current_PCB.sharedMemoryInfo->AreaTag, AreaTag);
	current_PCB.sharedMemoryInfo->SharedID = sharedID;
	for(i = 0; i < PagesInSharedArea;i++){
		current_PCB.PageStatusTable[startingPageNumber + i].isSharedMemory = 1;
	}
	add_node_to_PageTable_Queue(current_PCB.PageTable,current_PCB.PageStatusTable,startingPageNumber,PagesInSharedArea,sharedID);
	*SharedID = PageTable_Queue.size - 1;
	*ErrorReturned = 0;
}
/************************************************************
SEND MESSAGE
This function is used to send a message to the target process.
Input: targetPID, PID of target process,
messageBuffer, the array where the message is stored,
messageSendLength, length of the message.
Output: ErrorReturned, success or not
*************************************************************/
void send_message(int targetPID, char messageBuffer[], int messageSendLength, long *ErrorReturned){
	if((targetPID != -1 && !is_PID_alive(targetPID)) || messageSendLength <= 0){
		*ErrorReturned = 1;
		return;
	}
	add_message_to_Message_Queue(get_current_PID(),targetPID,messageSendLength,messageBuffer);
	*ErrorReturned = 0;
}
/************************************************************
RECEIVE MESSAGE
This function is used to receive a message from a source process.
Input: sourcePID, PID of source process,
messageBuffer, the array where the message is to be stored,
messageReceiveLength, max length of the message that can b received.
Output: 
messageSendLength, the length of this message,
messageSendPID, PID of source process if the message is received from any process,
ErrorReturned, success or not
*************************************************************/
void receive_message(int sourcePID, char messageBuffer[], int messageReceiveLength, int* messageSendLength, int* messageSendPID, long *ErrorReturned){
	Message_node *p;
	if(sourcePID < -1 || sourcePID >= MAXIMUM_PROCESS_NUMBER){
		*ErrorReturned = 1;
		return;
	}
	while(TRUE){
		p = get_out_a_node_from_Message_Queue(sourcePID,get_current_PID());
		if(p != NULL){
			if(p->length <= messageReceiveLength){
				strcpy(messageBuffer,p->context);
			}else{
				strncpy(messageBuffer,p->context,messageReceiveLength);
			}
			*messageSendLength = p->length;
			*messageSendPID = p->sourcePID;
			*ErrorReturned = 0;
			return;
		}else{
			add_PCB_to_Ready_Queue(current_PCB);
			switch_process();
		}
	}
}

/************************************************************
START_TEST_PROCESS
This function is used to start the test process.
*************************************************************/
void start_test_process(int argc, char *argv[]){
	long identifier = -1;
	if(argc > 1){
		if(strcmp(argv[1], "test0") == 0) identifier = (long)test0;
		if(strcmp(argv[1], "test1") == 0) identifier = (long)test1;
		if(strcmp(argv[1], "test2") == 0) identifier = (long)test2;
		if(strcmp(argv[1], "test3") == 0) identifier = (long)test3;
		if(strcmp(argv[1], "test4") == 0) identifier = (long)test4;
		if(strcmp(argv[1], "test5") == 0) identifier = (long)test5;
		if(strcmp(argv[1], "test6") == 0) identifier = (long)test6;
		if(strcmp(argv[1], "test7") == 0) identifier = (long)test7;
		if(strcmp(argv[1], "test8") == 0) identifier = (long)test8;
		if(strcmp(argv[1], "test9") == 0) identifier = (long)test9;
		if(strcmp(argv[1], "test10") == 0) identifier = (long)test10;
		if(strcmp(argv[1], "test11") == 0) identifier = (long)test11;
		if(strcmp(argv[1], "test12") == 0) identifier = (long)test12;
		if(strcmp(argv[1], "test13") == 0) identifier = (long)test13;
		if(strcmp(argv[1], "test14") == 0) identifier = (long)test14;
		if(strcmp(argv[1], "test15") == 0) identifier = (long)test15;
		if(strcmp(argv[1], "test16") == 0) identifier = (long)test16;
		
		if(strcmp(argv[1], "test21") == 0){
			identifier = (long)test21;
			SPPRINTLINE_ENABLED = 0;
			MPPRINTLINE_ENABLED = 1;
			DISPLAY_SP_EACH_TIME = 0;
			DISPLAY_MP_EACH_TIME = 1;
		}
		if(strcmp(argv[1], "test22") == 0){
			identifier = (long)test22;
			SPPRINTLINE_ENABLED = 0;
			MPPRINTLINE_ENABLED = 1;
			DISPLAY_SP_EACH_TIME = 0;
			DISPLAY_MP_EACH_TIME = 1;
		}
		if(strcmp(argv[1], "test23") == 0){
			identifier = (long)test23;
			SPPRINTLINE_ENABLED = 1;
			MPPRINTLINE_ENABLED = 1;
			DISPLAY_SP_EACH_TIME = 1;
			DISPLAY_MP_EACH_TIME = 1;
		}
		if(strcmp(argv[1], "test24") == 0){
			identifier = (long)test24;
			is_test24_running = 1;
			SPPRINTLINE_ENABLED = 1;
			MPPRINTLINE_ENABLED = 1;
			DISPLAY_SP_EACH_TIME = 0;
			DISPLAY_MP_EACH_TIME = 1;
		}
		if(strcmp(argv[1], "test25") == 0){
			identifier = (long)test25;
			SPPRINTLINE_ENABLED = 1;
			MPPRINTLINE_ENABLED = 1;
			DISPLAY_SP_EACH_TIME = 0;
			DISPLAY_MP_EACH_TIME = 0;
		}
		if(strcmp(argv[1], "test26") == 0){
			identifier = (long)test26;
			SPPRINTLINE_ENABLED = 1;
			MPPRINTLINE_ENABLED = 1;
			DISPLAY_SP_EACH_TIME = 1;
			DISPLAY_MP_EACH_TIME = 0;
		}
		if(strcmp(argv[1], "test27") == 0) identifier = (long)test27;
		if(strcmp(argv[1], "test28") == 0){
			identifier = (long)test28;
			is_test28_running = 1;
			SPPRINTLINE_ENABLED = 0;
			MPPRINTLINE_ENABLED = 1;
			DISPLAY_SP_EACH_TIME = 0;
			DISPLAY_MP_EACH_TIME = 0;
		}
	}
	if(identifier >= 0){
		create_process_and_run(argv[1],identifier,TEST_PROCESS_PRIORITY);
	}
}


//for debugging
void print_bitmap(int diskID){
	unsigned char bitmap[PGSIZE];
	int i,j;
	printf("diskID:%d\n",diskID);
	for(i = 0;i < NUMBER_OF_SECTORS_IN_BITMAP;i++){
		read_from_disk(diskID,i + BITMAP_START_LOCATION,bitmap);
		for(j = 0;j < PGSIZE;j++){
			printf("%d ",bitmap[j]);
		}
		printf("\n");
	}
}

void write_to_disk_no_switch(long diskID,long sector,unsigned char* context){
	write_or_read_disk_no_switch(diskID,sector,context,WRITE_DISK_FLAG);
}
void read_from_disk_no_switch(long diskID,long sector,unsigned char* context){
	write_or_read_disk_no_switch(diskID,sector,context,READ_DISK_FLAG);
}

void write_or_read_disk_no_switch(long diskID,long sector,unsigned char* context,int FLAG){
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = (FLAG == WRITE_DISK_FLAG ? Z502DiskWrite : Z502DiskRead);
	mmio.Field1 = diskID;
	mmio.Field2 = sector;
	mmio.Field3 = (long)context;
	mmio.Field4 = 0;
	MEM_WRITE(Z502Disk, &mmio);

	mmio.Mode = Z502Action;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_WRITE(Z502Idle, &mmio);
}