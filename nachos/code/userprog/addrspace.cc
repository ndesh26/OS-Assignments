// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

void
CheckDirtyAndBackup(int page)
{
    char *backup = NULL;
    TranslationEntry *entry;
    int i, thread_id;

    entry = machine->physicalPageMap[page].entry;
    thread_id = machine->physicalPageMap[page].thread_id;
    ASSERT(page == entry->physicalPage);

    if (entry->dirty && !exitThreadArray[thread_id]) {
        backup = threadArray[thread_id]->space->GetBackup();
        for (i = 0; i < PageSize; i++)
            backup[entry->virtualPage * PageSize + i] = machine->mainMemory[page * PageSize + i];
        entry->backup = TRUE;
    }
    entry->physicalPage = -1;
    entry->valid = FALSE;
}

unsigned
getNewPage (int avoid)
{
    int page = avoid;
 
    stats->numPageFaults++;
    switch (pageReplacementAlgo) {
        case NONE:
            ASSERT(numPagesAllocated <= NumPhysPages)
            numPagesAllocated++;
            return numPagesAllocated - 1;
        case RANDOM:
            if (numPagesAllocated == NumPhysPages) {
                while(page == avoid || machine->physicalPageMap[page].entry->shared == TRUE)
                    page = Random() % NumPhysPages;
                DEBUG('k', "Replacing page %d\n", page);
                CheckDirtyAndBackup(page);
                return page;
            }
            numPagesAllocated++;
            return numPagesAllocated - 1;
        case FIFO:
        case LRU:
        case LRU_CLOCK:
        default:
            ASSERT(0);
    }
}

void
SetPhysicalMap(int page, int pid, TranslationEntry *entry)
{
    machine->physicalPageMap[page].thread_id = pid;
    machine->physicalPageMap[page].entry = entry;
}
//----------------------------------------------------------------------
// ProcessAddrSpace::ProcessAddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

ProcessAddrSpace::ProcessAddrSpace(OpenFile *Executable, char *Filename)
{
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;

    executable = Executable;
    filename = Filename;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPagesInVM = divRoundUp(size, PageSize);
    size = numPagesInVM * PageSize;

    backup = new char[size];
    if (pageReplacementAlgo == NONE)
        ASSERT(numPagesInVM+numPagesAllocated <= NumPhysPages);		// check we're not trying
										// to run anything too big --
										// at least until we have
										// virtual memory

    DEBUG('k', "Initializing address space, num pages %d, size %d\n", 
					numPagesInVM, size);
// first, set up the translation 
    NachOSpageTable = new TranslationEntry[numPagesInVM];
    for (i = 0; i < numPagesInVM; i++) {
	NachOSpageTable[i].virtualPage = i;
	NachOSpageTable[i].physicalPage = -1;
	NachOSpageTable[i].valid = FALSE;
	NachOSpageTable[i].use = FALSE;
	NachOSpageTable[i].dirty = FALSE;
        NachOSpageTable[i].shared = FALSE;
        NachOSpageTable[i].backup = FALSE;
	NachOSpageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
}

//----------------------------------------------------------------------
// ProcessAddrSpace::ProcessAddrSpace (ProcessAddrSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

ProcessAddrSpace::ProcessAddrSpace(ProcessAddrSpace *parentSpace, int pid)
{
    numPagesInVM = parentSpace->GetNumPages();
    unsigned i, k, size = numPagesInVM * PageSize;
    unsigned startAddrParent, startAddrChild, unallocated;
    char *parentBackup = parentSpace->GetBackup();

    filename = parentSpace->filename;
    executable = fileSystem->Open(filename);
    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    noffH = parentSpace->GetNoffHeader();
    backup = new char[size];

    if (pageReplacementAlgo == NONE)
        ASSERT(numPagesInVM+numPagesAllocated <= NumPhysPages);                // check we're not trying
                                                                                // to run anything too big --
                                                                                // at least until we have
                                                                                // virtual memory

    DEBUG('k', "Initializing address space, num pages %d, size %d\n",
                                        numPagesInVM, size);
    // first, set up the translation
    TranslationEntry* parentPageTable = parentSpace->GetPageTable();
    NachOSpageTable = new TranslationEntry[numPagesInVM];
    for (i = 0; i < numPagesInVM; i++) {
        NachOSpageTable[i].virtualPage = i;
        if (parentPageTable[i].shared || !parentPageTable[i].valid)
            NachOSpageTable[i].physicalPage = parentPageTable[i].physicalPage;
        else if (parentPageTable[i].valid) {
            unallocated = getNewPage(parentPageTable[i].physicalPage);
            DEBUG('k', "Copying virtual page %d to physical page %d\n", i, unallocated);
            bzero(&machine->mainMemory[unallocated * PageSize], PageSize);
            startAddrChild = unallocated * PageSize;
            startAddrParent = parentPageTable[i].physicalPage * PageSize;
            for (k = 0; k < PageSize; k++)
               machine->mainMemory[startAddrChild + k] = machine->mainMemory[startAddrParent + k];
            NachOSpageTable[i].physicalPage = unallocated;
            SetPhysicalMap(unallocated, pid, &NachOSpageTable[i]);
            //machine->physicalPageMap[unallocated].thread_id = pid; 
              //machine->physicalPageMap[unallocated].entry = &NachOSpageTable[i];
        }
        NachOSpageTable[i].valid = parentPageTable[i].valid;
        NachOSpageTable[i].use = parentPageTable[i].use;
        NachOSpageTable[i].dirty = parentPageTable[i].dirty;
        NachOSpageTable[i].shared = parentPageTable[i].shared;
        NachOSpageTable[i].backup = parentPageTable[i].backup;
        NachOSpageTable[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
    }
    for (i = 0; i < size; i++)
        backup[i] = parentBackup[i];
}

//----------------------------------------------------------------------
// ProcessAddrSpace::~ProcessAddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

ProcessAddrSpace::~ProcessAddrSpace()
{
   delete NachOSpageTable;
}

//----------------------------------------------------------------------
// ProcessAddrSpace::InitUserCPURegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
ProcessAddrSpace::InitUserCPURegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPagesInVM * PageSize - 16);
    DEBUG('k', "Initializing stack register to %d\n", numPagesInVM * PageSize - 16);
}

//----------------------------------------------------------------------
// ProcessAddrSpace::SaveStateOnSwitch
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void ProcessAddrSpace::SaveStateOnSwitch() 
{}

//----------------------------------------------------------------------
// ProcessAddrSpace::RestoreStateOnSwitch
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void ProcessAddrSpace::RestoreStateOnSwitch() 
{
    machine->NachOSpageTable = NachOSpageTable;
    machine->NachOSpageTableSize = numPagesInVM;
}

unsigned
ProcessAddrSpace::GetNumPages()
{
   return numPagesInVM;
}

TranslationEntry*
ProcessAddrSpace::GetPageTable()
{
   return NachOSpageTable;
}

OpenFile*
ProcessAddrSpace::GetExecutable()
{
    return executable;
}

NoffHeader
ProcessAddrSpace::GetNoffHeader()
{
    return noffH;
}

char*
ProcessAddrSpace::GetFilename()
{
    return filename;
}

char*
ProcessAddrSpace::GetBackup()
{
    return backup;
}

int
ProcessAddrSpace::AddSharedMemory(unsigned size)
{
    int i, startSharedAddr;
    unsigned numSharedPages, unallocated;
    TranslationEntry* pageTable;
    char *oldBackup;

    numSharedPages =  divRoundUp(size, PageSize);
    size = numSharedPages * PageSize;
    pageTable = NachOSpageTable;
    NachOSpageTable = new TranslationEntry[numPagesInVM + numSharedPages];
    oldBackup = backup;
    backup = new char[(numPagesInVM + numSharedPages) * PageSize];
     
    for (i = 0; i < numPagesInVM; i++) {
        NachOSpageTable[i].virtualPage = i;
        NachOSpageTable[i].physicalPage = pageTable[i].physicalPage;
        NachOSpageTable[i].valid = pageTable[i].valid;
        NachOSpageTable[i].use = pageTable[i].use;
        NachOSpageTable[i].dirty = pageTable[i].dirty;
        NachOSpageTable[i].shared = pageTable[i].shared;
        NachOSpageTable[i].readOnly = pageTable[i].readOnly;
        NachOSpageTable[i].backup = pageTable[i].backup;

        if (pageTable[i].valid)
            SetPhysicalMap(pageTable[i].physicalPage, currentThread->GetPID(), &NachOSpageTable[i]);
    }

    for (i = numPagesInVM; i < numPagesInVM + numSharedPages; i++) {
        unallocated = getNewPage(-1);
        DEBUG('k', "allocating shared page %d to physical page %d\n", i, unallocated);
        bzero(&machine->mainMemory[unallocated * PageSize], PageSize);
        NachOSpageTable[i].virtualPage = i;
        NachOSpageTable[i].physicalPage = unallocated;
        NachOSpageTable[i].valid = TRUE;
        NachOSpageTable[i].use = FALSE;
        NachOSpageTable[i].dirty = FALSE;
        NachOSpageTable[i].shared = TRUE;
        NachOSpageTable[i].readOnly = FALSE;
        NachOSpageTable[i].backup = FALSE;
        SetPhysicalMap(unallocated, currentThread->GetPID(), &NachOSpageTable[i]);
        //machine->physicalPageMap[unallocated].thread_id = currentThread->GetPID();
        //machine->physicalPageMap[unallocated].entry = &NachOSpageTable[i];
    }

    for (i = 0; i < numPagesInVM * PageSize; i++)
        backup[i] = oldBackup[i];

    DEBUG('k', "Initializing shared address space, num pages %d, size %d\n",
                                        numSharedPages, size);
    delete pageTable;
    delete oldBackup;
    startSharedAddr = numPagesInVM * PageSize;
    numPagesInVM += numSharedPages;
    machine->NachOSpageTable = NachOSpageTable;
    machine->NachOSpageTableSize = numPagesInVM;
    return startSharedAddr;
}

void
ProcessAddrSpace::HandlePageFault(int vaddr)
{
    unsigned vpn, i;
    unsigned unallocated = getNewPage(-1);

    vpn = vaddr/PageSize;
    DEBUG('k', "Copying virtual page %d to physical page %d ", vpn, unallocated);
    bzero(&machine->mainMemory[unallocated * PageSize], PageSize);
    if (NachOSpageTable[vpn].backup) {
        DEBUG('k',"from backup\n");
        for (i = 0; i < PageSize; i++)
            machine->mainMemory[i + unallocated * PageSize] = backup[i + vpn * PageSize];
    } 
    else {
        DEBUG('k',"from executable\n");
        executable->ReadAt(&(machine->mainMemory[unallocated * PageSize]),
                        PageSize, noffH.code.inFileAddr + vpn * PageSize);
    }
    NachOSpageTable[vpn].physicalPage = unallocated;
    NachOSpageTable[vpn].valid = TRUE;
    SetPhysicalMap(unallocated, currentThread->GetPID(), &NachOSpageTable[vpn]);
    //machine->physicalPageMap[unallocated].thread_id = currentThread->GetPID();
    //machine->physicalPageMap[unallocated].entry = &NachOSpageTable[vpn];
}
