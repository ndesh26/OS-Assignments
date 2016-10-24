// progtest.cc 
//	Test routines for demonstrating that Nachos can load
//	a user program and execute it.  
//
//	Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "console.h"
#include "addrspace.h"
#include "synch.h"

void
InitializeStack(int which)
{
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->space->InitUserCPURegisters();     // to restore, do it.
        currentThread->space->RestoreStateOnSwitch();
    }
#endif
    machine->Run();
}

//----------------------------------------------------------------------
// StartUserProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------

void
StartUserProcess(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename);
    ProcessAddrSpace *space;

    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    space = new ProcessAddrSpace(executable);    
    currentThread->space = space;

    delete executable;			// close file

    space->InitUserCPURegisters();		// set the initial register values
    space->RestoreStateOnSwitch();		// load page table register

    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

void
StartBatchProcess(char *filename)
{
    OpenFile *executable_list = fileSystem->Open(filename);
    char c, path[1000];
    int priority = 100, i = 0;
    bool num = false;
    OpenFile *executable;
    NachOSThread *batchThread;

    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    /* Ignore the first line as it contains scheduler type */
    while (executable_list->Read(&c,1) != 0 && c != '\n'); 

    while (executable_list->Read(&c,1) != 0) {
        switch (c) {
            case '\n': {
                path[i] = 0;
                executable = fileSystem->Open(path);

                if (executable == NULL) {
                    printf("Unable to open file %s\n", path);
                    i = 0;
                } else {
                    batchThread = new NachOSThread("batch thread");
                    i = 0;

                    while(processTable[i] != NULL && i < 1000) i++;
                    if (i < 1000) {
                        processTable[i] = batchThread;
                        DEBUG('f', "Process with pid %d added to processTable\n",batchThread->getPid());
                    }

                    batchThread->space = new ProcessAddrSpace(executable);
                    batchThread->setPriority(50 + priority);
                    batchThread->setBasePriority(50 + priority);
                    batchThread->ThreadFork(InitializeStack, 0);

                    delete executable;			// close file

                    priority = 100;
                    i = 0;
                    num = false;
                }
                break;
            }
            case ' ': {
                priority = 0;
                num = true;
                break;
            }
            default: {
                if (num)
                    priority = (priority * 10) + c - '0';
                else
                    path[i++] = c;
            }
        }
    }
    delete executable_list;
    (void) interrupt->SetLevel(oldLevel);
}

// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
// 	Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

//----------------------------------------------------------------------
// ConsoleTest
// 	Test the console by echoing characters typed at the input onto
//	the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------

void 
ConsoleTest (char *in, char *out)
{
    char ch;

    console = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    
    for (;;) {
	readAvail->P();		// wait for character to arrive
	ch = console->GetChar();
	console->PutChar(ch);	// echo it!
	writeDone->P() ;        // wait for write to finish
	if (ch == 'q') return;  // if q, quit
    }
}
