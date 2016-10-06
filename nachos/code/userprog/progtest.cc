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

//----------------------------------------------------------------------
// StartUserProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------

void
StartUserProcess(char *filename)
{
    for(int h=0;h<=strlen(filename);h++){
        printf("here:%d\n",filename[h]);
    }
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
    char c;
    char path[1000];
    int priority = 100,i=0;
    bool Num=false;
    char filename_i[28];
    while(executable_list->Read(&c,1)!=0){
        //printf("C: %c\n",c);
        //printf("PATh: %s\n",path);
        if(c == '\n'){
            printf("filename_i goes here:%s\n",filename_i);
            for(int h=0;h<=strlen(filename_i);h++){
                    printf("here:%d\n",filename_i[h]);
                }

            NachOSThread *threadToPut = new NachOSThread("Thread to be put in ready queue"); 
            OpenFile *executable = fileSystem->Open(filename_i);
            ProcessAddrSpace *space;

            if (executable == NULL) {
                printf("Unable to open file asasd%s\n", filename_i);
                return;
            }
            space = new ProcessAddrSpace(executable);    
            threadToPut->space = space;
            threadToPut->setPriority(priority);

            delete executable;			// close file

            space->InitUserCPURegisters();		// set the initial register values
            space->RestoreStateOnSwitch();		// load page table register

            //machine->Run();			// jump to the user progam
            scheduler->ThreadIsReadyToRun(threadToPut);
            //ASSERT(FALSE);			// machine->Run never returns;
                                                // the address space exits
                                                // by doing the syscall "exit"
            priority = 100;
            i = 0;
            Num = false;
        }
        if(c == ' '){
            priority = 0;
            Num=true;
            //filename_i=new char[i+1];
            for(int j=0;j<i;j++){
                filename_i[j] = path[j];
            }
            filename_i[i]='\0';
            printf("filename_i:%s\n",filename_i);
        }
        if(Num){
            priority = priority*10+c-'0';
        }
        else{
            path[i]=c;
            i++;
        }
    }
    delete executable_list;
    machine->Run();
    ASSERT(false);
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
