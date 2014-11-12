// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

extern void StartProcess (char*);

void
ForkStartFunction (int dummy)
{
   currentThread->Startup();
   machine->Run();
}

static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp;
    unsigned printvalus;	// Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);
    int exitcode;		// Used in syscall_Exit
    unsigned i;
    char buffer[1024];		// Used in syscall_Exec
    int waitpid;		// Used in syscall_Join
    int whichChild;		// Used in syscall_Join
    Thread *child;		// Used by syscall_Fork
    unsigned sleeptime;		// Used by syscall_Sleep
    int size;			// Used in syscall_ShmAllocate	
    int semKey; 		// Used in syscall_SemGet
    int semId; 			// Used in syscall_SemGet
    int adjustment_value; 	// Used in syscall_SemOp
    int PhyAddr; 		// Used in syscall_SemCtl
    int condId;			// Used in syscall_CondGet
    int condKey;		// Used in syscall_CondGet


    if ((which == SyscallException) && (type == syscall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == syscall_Exit)) {
       exitcode = machine->ReadRegister(4);
       printf("[pid %d]: Exit called. Code: %d\n", currentThread->GetPID(), exitcode);
       // We do not wait for the children to finish.
       // The children will continue to run.
       // We will worry about this when and if we implement signals.
       exitThreadArray[currentThread->GetPID()] = true;

       // Find out if all threads have called exit
       for (i=0; i<thread_index; i++) {
          if (!exitThreadArray[i]) break;
       }
       currentThread->Exit(i==thread_index, exitcode);
    }
    else if ((which == SyscallException) && (type == syscall_Exec)) {
       // Copy the executable name into kernel space
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       i = 0;
       while ((*(char*)&memval) != '\0') {
          buffer[i] = (*(char*)&memval);
          i++;
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       buffer[i] = (*(char*)&memval);
       StartProcess(buffer);
    }
    else if ((which == SyscallException) && (type == syscall_Join)) {
       waitpid = machine->ReadRegister(4);
       // Check if this is my child. If not, return -1.
       whichChild = currentThread->CheckIfChild (waitpid);
       if (whichChild == -1) {
          printf("[pid %d] Cannot join with non-existent child [pid %d].\n", currentThread->GetPID(), waitpid);
          machine->WriteRegister(2, -1);
          // Advance program counters.
          machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
          machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
          machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       }
       else {
          exitcode = currentThread->JoinWithChild (whichChild);
          machine->WriteRegister(2, exitcode);
          // Advance program counters.
          machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
          machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
          machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       }
    }
    else if ((which == SyscallException) && (type == syscall_Fork)) {
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       
       child = new Thread("Forked thread", GET_NICE_FROM_PARENT);
    	//printf("creating new thread for child\n");
       child->space = new AddrSpace (currentThread->space);  // Duplicates the address space
       	//printf("address space for child created\n");
	child->SaveUserState ();		     		      // Duplicate the register set
       	//printf("address space for child created 1111111\n");
       child->ResetReturnValue ();			     // Sets the return register to zero
       	//printf("address space for child created 2222222\n");
       child->StackAllocate (ForkStartFunction, 0);	// Make it ready for a later context switch
       	//printf("address space for child created 333333333\n");
       child->Schedule ();
       	//printf("address space for child created 44444\n");
       machine->WriteRegister(2, child->GetPID());		// Return value for parent
       	//printf("address space for child created 5555555\n");
    }
    else if ((which == SyscallException) && (type == syscall_Yield)) {
       currentThread->Yield();
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
             writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
             writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintChar)) {
        writeDone->P() ;        // wait for previous write to finish
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
          writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetReg)) {
       machine->WriteRegister(2, machine->ReadRegister(machine->ReadRegister(4))); // Return value
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPA)) {
       vaddr = machine->ReadRegister(4);
       machine->WriteRegister(2, machine->GetPA(vaddr));  // Return value
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPID)) {
       machine->WriteRegister(2, currentThread->GetPID());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPPID)) {
       machine->WriteRegister(2, currentThread->GetPPID());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Sleep)) {
       sleeptime = machine->ReadRegister(4);
       if (sleeptime == 0) {
          // emulate a yield
          currentThread->Yield();
       }
       else {
          currentThread->SortedInsertInWaitQueue (sleeptime+stats->totalTicks);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Time)) {
       machine->WriteRegister(2, stats->totalTicks);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_NumInstr)) {
       machine->WriteRegister(2, currentThread->GetInstructionCount());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
///////////////////////// STARTING CHANGES FOR ASSIGNMENT3 ///////////////////////
    else if((which == SyscallException) && (type == syscall_ShmAllocate)) {
	size = machine->ReadRegister(4);    

	vaddr = currentThread->space->AllocateSharedMemory(size);
       
	machine->WriteRegister(2, vaddr);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }

    else if ((which == SyscallException) && (type == syscall_SemGet))
    {
	semKey = machine->ReadRegister(4);
	for(i=0; i<Sem_size; i++){
		if(semaphoreKey[i] == semKey){
			semId = semaphoreId[i];
			break;
		}
	}
	if(Sem_size >= MAX_SEMAPHORES){
		printf("ERROR: The total number of semaphores has exceeded the allowed limit.\n Semaphore could not be created, function returns -1\n");
		semId = -1; //The return value corresponding to return
	}
	if(( i == Sem_size) && (Sem_size < MAX_SEMAPHORES)) {
		IntStatus oldLevel = interrupt->SetLevel(IntOff); //disable interrupts
	
		semaphoreKey[Sem_size] = semKey;
		semaphoreId[Sem_size] = Id_counter;
		semaphores[Sem_size] = new Semaphore("Sem_name", 1);
		semId = semaphoreId[Sem_size];
		Sem_size++;
		Id_counter++;
	
		(void) interrupt->SetLevel(oldLevel); //enable interrupts
    	}
	// Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	
	// Return the Semaphore ID
	machine->WriteRegister(2, semId);
    }
	
    else if ((which == SyscallException) && (type == syscall_SemOp))
    {
	semId = machine->ReadRegister(4);
	adjustment_value = machine->ReadRegister(5);
	for(i =0; i<Sem_size; i++){
		if(semaphoreId[i] == semId){
			semId = i;
			break;
		}
	}
	if(i == Sem_size){
		printf("ERROR: The semaphore id entered is not a valid id\n");
	}
	else {
		if(adjustment_value == -1){
			semaphores[semId]->P();
		}
		else if(adjustment_value == 1){
			semaphores[semId]->V();
		}
		else {
			printf("ERROR: Invalid Operation id in syscall_SemOp\n");
		}
	}
	// Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	
	// Return the Semaphore ID
	//machine->WriteRegister(2, semId);
    }
	
    else if ((which == SyscallException) && (type == syscall_SemCtl))
    {
	//printf("qqqqqqqqqqqqqqqqqqqqqq entered\n");
	semId = machine->ReadRegister(4);
	adjustment_value = machine->ReadRegister(5);
	vaddr = machine->ReadRegister(6);
	
	for (i=0; i<Sem_size; i++){
		if(semaphoreId[i] == semId){
			semId = i;
			break;
		}
	}
	if (i == Sem_size) {
		exitcode = -1;
	}
	else if(adjustment_value == SYNCH_REMOVE){
		delete semaphores[semId];
		for(i=semId; i<Sem_size-1; i++){
			semaphores[i] = semaphores[i+1]; //deleting semaphore
			semaphoreKey[i] = semaphoreKey[i+1]; //removing from mapping
			semaphoreId[i] = semaphoreId[i+1];
		}
		Sem_size--;
		exitcode = 0;
	}
	else if(adjustment_value == SYNCH_GET) {
		PhyAddr = machine->GetPA(vaddr);
		if(PhyAddr == -1){
			exitcode = -1;
		}
		else {
			machine->mainMemory[PhyAddr] = semaphores[semId]->getValue();
			exitcode = 0;
		}
	}
	else if(adjustment_value == SYNCH_SET) {
		PhyAddr = machine->GetPA(vaddr);
		//printf("reached a correct destination before segmentation fault\n");
		if(PhyAddr == -1){
			exitcode = -1;
		}
		else {
			//printf("intPointer is also not null, baby we are going to rock the world");
			semaphores[semId]->setValue(machine->mainMemory[PhyAddr]);
			exitcode = 0;
		}
	}
	else {
		//printf("The adjustment value was not a valid value");
		exitcode = -1;
	}
	//printf("aaaaaaaaaaaaaaaaaaaaaaaaaaaa successful completion\n");
	// Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	
	// Return the Semaphore ID
	machine->WriteRegister(2, exitcode);
    }
 
    else if ((which == SyscallException) && (type == syscall_CondGet))
    {
	condKey = machine->ReadRegister(4);
	for(i=0; i<Cond_size; i++){
		if(conditionKey[i] == condKey){
			condId = conditionId[i];
			break;
		}
	}
	if(Cond_size >= MAX_CONDITIONS){
		printf("ERROR: The total number of conditions has exceeded the allowed limit.\n Condition Variable could not be created, function returns -1\n");
		condId = -1; //The return value corresponding to return
	}
	if(( i == Cond_size) && (Cond_size < MAX_CONDITIONS)) {
		IntStatus oldLevel = interrupt->SetLevel(IntOff); //disable interrupts
	
		conditionKey[Cond_size] = condKey;
		conditionId[Cond_size] = CondId_counter;
		conditions[Cond_size] = new Condition("Cond_name");
		condId = conditionId[Cond_size];
		Cond_size++;
		CondId_counter++;
	
		(void) interrupt->SetLevel(oldLevel); //enable interrupts
    	}
	// Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	
	// Return the Semaphore ID
	machine->WriteRegister(2, condId);
    }

    else if ((which == SyscallException) && (type == syscall_CondOp))
    {
	condId = machine->ReadRegister(4);
	adjustment_value = machine->ReadRegister(5);
	semId = machine->ReadRegister(6);
	
	for(i =0; i<Cond_size; i++){
		if(conditionId[i] == condId){
			condId = i;
			break;
		}
	}
	if(i == Cond_size){
		printf("ERROR: The condition variable id entered is not a valid id\n");
	}
	else {
		for(i =0; i<Sem_size; i++){
			if(semaphoreId[i] == semId){
				semId = i;
				break;
			}
		}
		if(i == Sem_size){
			printf("ERROR: The semaphore id entered is not a valid id\n");
		}
		else {
			if(adjustment_value == COND_OP_WAIT){
				//printf("Before calling internal function\n");
				conditions[condId]->Wait(semaphores[semId]);
				//printf("after the same\n");
			}
			else if(adjustment_value == COND_OP_SIGNAL){
				conditions[condId]->Signal();
			}
			else if(adjustment_value == COND_OP_BROADCAST){
				conditions[condId]->Broadcast();
			}
			else {
				printf("ERROR: Invalid operation in syscall_CondOp");
			}
		}
	}
	// Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	
	// Return the Semaphore ID
	//machine->WriteRegister(2, semId);
    }

    else if ((which == SyscallException) && (type == syscall_CondRemove))
    {
	//printf("qqqqqqqqqqqqqqqqqqqqqq entered\n");
	condId = machine->ReadRegister(4);
//	adjustment_value = machine->ReadRegister(5);
//	vaddr = machine->ReadRegister(6);
	
	for (i=0; i<Cond_size; i++){
		if(conditionId[i] == condId){
			condId = i;
			break;
		}
	}
	if (i == Cond_size) {
		exitcode = -1;
	}
	else {
		delete conditions[condId];
		for(i=condId; i<Cond_size-1; i++){
			conditions[i] = conditions[i+1]; //deleting condiion variable
			conditionKey[i] = conditionKey[i+1]; //removing from mapping
			conditionId[i] = conditionId[i+1];
		}
		Cond_size--;
		exitcode = 0;
	}
	//printf("aaaaaaaaaaaaaaaaaaaaaaaaaaaa successful completion\n");
	// Advance program counters.
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
	
	// Return the Semaphore ID
	machine->WriteRegister(2, exitcode);
    }
 
    else if (which == PageFaultException)
    {
        vpn = machine->ReadRegister(BadVAddrReg);
        stats->numPageFaults++;
        while (PhyPageIsAllocated[i]) 
            i++;
        if (i >= NumPhysPages)
            ASSERT(FALSE);
//  currentThread->SortedInsertInWaitQueue(stats->totalTicks+1000);
  //entry = &pageTable[vpn];
    pageTable[vpn].physicalPage = i;
    pageTable[vpn].valid = TRUE;  
    PhyPageIsAllocated[i] = TRUE;

    bzero(&machine->mainMemory[numPagesAllocated*PageSize], PageSize);
  
    numPagesAllocated++;
    printf("HERE AM I");
    currentThread->space->CopyContent(vpn);
    printf("I CAN'T REACH HERE");

	       currentThread->SortedInsertInWaitQueue(stats->totalTicks + 1000);	
    }
 
//////////////////////////// DONE CHANGES IN ASSIGNMENT 3 /////////////////////////////////////

   else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
   }
}
