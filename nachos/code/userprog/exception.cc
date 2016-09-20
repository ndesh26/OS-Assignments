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
StackInitialize(int which)
{
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }

    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
        currentThread->space->RestoreStateOnSwitch();
    }

    machine->Run();
}
void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp, i;
    unsigned printvalus;        // Used for printing in hex
    char path[1000];            // To store path of the file for exec
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == SYScall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SYScall_PrintInt)) {
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
    else if ((which == SyscallException) && (type == SYScall_PrintChar)) {
	writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_PrintString)) {
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
    else if ((which == SyscallException) && (type == SYScall_PrintIntHex)) {
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
    else if ((which == SyscallException) && (type == SYScall_GetReg)) {
       machine->WriteRegister(2, machine->ReadRegister(machine->ReadRegister(4)));
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_GetPA)) {
        int i, physAddr, virtAddr;
        unsigned int vpn, offset;
        TranslationEntry *entry;
        unsigned int pageFrame;

        virtAddr = machine->ReadRegister(4);
    
        // we must have either a TLB or a page table, but not both!
        ASSERT(machine->tlb == NULL || machine->NachOSpageTable == NULL);	
        ASSERT(machine->tlb != NULL || machine->NachOSpageTable != NULL);	

        // calculate the virtual page number, and offset within the page,
        // from the virtual address
        vpn = (unsigned) virtAddr / PageSize;
        offset = (unsigned) virtAddr % PageSize;
    
        if (machine->tlb == NULL) {		// => page table => vpn is index into table
	    if (vpn >= machine->pageTableSize) {
	        machine->WriteRegister(2, -1); 
	    } else if (!machine->NachOSpageTable[vpn].valid) {
		machine->WriteRegister(2, -1); 
	    }
	    entry = &(machine->NachOSpageTable[vpn]);
        } else {
            for (entry = NULL, i = 0; i < TLBSize; i++)
    	        if (machine->tlb[i].valid && (machine->tlb[i].virtualPage == vpn)) {
		    entry = &(machine->tlb[i]);			// FOUND!
		    break;
	        }
	    if (entry == NULL) {				// not found
                machine->WriteRegister(2, -1);    // really, this is a TLB fault,
					          // the page may be in memory,
						  // but not in the TLB
	    }
        }

        pageFrame = entry->physicalPage;
        // if the pageFrame is too big, there is something really wrong! 
        // An invalid translation was loaded into the page table or TLB. 
        if (pageFrame >= NumPhysPages) { 
	    machine->WriteRegister(2, -1);
        }
        physAddr = pageFrame * PageSize + offset;
        machine->WriteRegister(2, physAddr);
       
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
   
    }
    else if ((which == SyscallException) && (type == SYScall_Time)) {
        //Statistics stat;
        machine->WriteRegister(2, stats->totalTicks);

        // Advance program counter
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_Yield)) {
        // Advance program counter
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

        // Yield the current Thread
        currentThread->YieldCPU();
    }
    else if ((which == SyscallException) && (type == SYScall_GetPID)) {
        machine->WriteRegister(2, currentThread->getPid());

        // Advance program counter
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_GetPPID)) {
        machine->WriteRegister(2, currentThread->getPpid());

        // Advance program counter
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_NumInstr)) {
        machine->WriteRegister(2, currentThread->getInstrNum());

        // Advance program counter
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SYScall_Exec)) {
        vaddr = machine->ReadRegister(4);
        machine->ReadMem(vaddr, 1, &memval);
        i = 0;

        while ((*(char*)&memval) != '\0') {     //put the file address in string array
            path[i] = *(char *)&memval;
            vaddr++;
            i++;
            machine->ReadMem(vaddr, 1, &memval);
        }

        path[i] = *(char *)&memval;             //put '\0' in the end if the string
        OpenFile *executable = fileSystem->Open(path);
        ProcessAddrSpace *space;

        if (executable == NULL) {
	    printf("Unable to open file %s\n", path);
	    return;
        }

        space = new ProcessAddrSpace(executable);
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
        currentThread->space = space;

        delete executable;			// close file

        space->InitUserCPURegisters();		// set the initial register values
        space->RestoreStateOnSwitch();		// load page table register

        machine->Run();			        // jump to the progam
        ASSERT(false);
    }
    else if ((which == SyscallException) && (type == SYScall_Sleep)) {
        // Advance program counter
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

        i = (int) machine->ReadRegister(4);

        if (i == 0) {
            currentThread->YieldCPU();
        }
        else {
               threadQueue->SortedInsert(currentThread, i + (stats->totalTicks));
               IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
               currentThread->PutThreadToSleep();
               (void) interrupt->SetLevel(oldLevel);	                // re-enable interrupts
        }
    }
    else if ((which == SyscallException) && (type == SYScall_Exit)) {
	int exitStatus = machine->ReadRegister(4);
        int ppid = currentThread->getPpid();
        int pid = currentThread->getPid();
        NachOSThread* parent = NULL;

        while (i < 1000 && processTable[i] != NULL && processTable[i]->getPid() != ppid) i++;
        if(i < 1000)
            parent = processTable[i];

        if (parent != NULL) {
            printf("%d", currentThread->getPid());
            int index = parent->getChildIndex(currentThread->getPid());
            if (index == -1) {
                machine->WriteRegister(2, -1);
            }
            else {
                int childStatus = parent->getChildStatus(index);
                parent->setChildStatus(index, CHILD_FINISHED);
                if (childStatus == PARENT_WAITING) {      // Child is running
                    scheduler->ThreadIsReadyToRun(parent);
                }
            }
        }
 
        while (i < 1000 && processTable[i] != NULL && processTable[i]->getPid() != pid) i++;
        if(i < 1000)
            processTable[i] = NULL;

        if(scheduler->IsEmpty())
            interrupt->Halt();
        currentThread->FinishThread();

    }
    else if((which == SyscallException) && (type == SYScall_Fork)) {
        NachOSThread *childThread = new NachOSThread("child thread");
        ProcessAddrSpace *space;

        space = new ProcessAddrSpace(machine->pageTableSize,
                                     machine->NachOSpageTable[0].physicalPage);
        childThread->space = space;

        // Advance program counter
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

        machine->WriteRegister(2, 0); // So that the saved user state for the child
        childThread->SaveUserState(); // has 0 as return value and incresed PCs
        machine->WriteRegister(2, childThread->getPid()); // setting return for parent process
        childThread->ThreadFork(StackInitialize, 0);
    }
    else if((which == SyscallException) && (type == SYScall_Join)) {
        i = machine->ReadRegister(4);
   
        // Advance program counter
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
        
        int index = currentThread->getChildIndex(i);
        if(index == -1) {
            machine->WriteRegister(2, -1);
        }
        else {
            ChildStatus childStatus = currentThread->getChildStatus(index); 
            if(childStatus == CHILD_LIVE) {      // Child is running
                currentThread->setChildStatus(index, PARENT_WAITING); 
                IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
                currentThread->PutThreadToSleep();
                (void) interrupt->SetLevel(oldLevel);	                // re-enable interrupts

            }
            else if(childStatus == 1) { // Child is Terminated
                machine->WriteRegister(2, currentThread->getChildExitCode(index));
            }
        }
    }
    else {
	printf("Unexpected user mode exception %d %d %d\n", which, type, which == SyscallException);
	ASSERT(FALSE);
    }
}
