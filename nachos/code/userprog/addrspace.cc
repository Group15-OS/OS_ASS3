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
#include "noff.h"

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

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
// Group15:
//  The translation will not be one to one anymore, since we are implementing
//  demand paging in this Operating System
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

   //G-15 ASSERT(numPages+numPagesAllocated <= NumPhysPages);		// check we're not trying
										// to run anything too big --
										// at least until we have
										// virtual memory

	DEBUG('a', "Initializing address space, num pages %d, size %d\n", numPages, size);
// first, set up the translation 
	pageTable = new TranslationEntry[numPages];
	for (i = 0; i < numPages; i++) {
		pageTable[i].virtualPage = i;
		// pageTable[i].physicalPage = i+numPagesAllocated;
		pageTable[i].physicalPage = -1;
		pageTable[i].valid = FALSE;
		pageTable[i].use = FALSE;
		pageTable[i].dirty = FALSE;
		pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
										// a separate page, we could set its 
										// pages to be read-only
		pageTable[i].shared = FALSE;
	}
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    // bzero(&machine->mainMemory[numPagesAllocated*PageSize], size);
 
    // numPagesAllocated += numPages;

// then, copy in the code and data segments into memory
   /* if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        vpn = noffH.code.virtualAddr/PageSize;
        offset = noffH.code.virtualAddr%PageSize;
        entry = &pageTable[vpn];
        pageFrame = entry->physicalPage;
        executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
			noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        vpn = noffH.initData.virtualAddr/PageSize;
        offset = noffH.initData.virtualAddr%PageSize;
        entry = &pageTable[vpn];
        pageFrame = entry->physicalPage;
        executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
			noffH.initData.size, noffH.initData.inFileAddr);
    }*/

}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace (AddrSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

AddrSpace::AddrSpace(AddrSpace *parentSpace)
{
    numPages = parentSpace->GetNumPages();
    unsigned i, j, k=0, size = numPages * PageSize;

    ASSERT(numPages+numPagesAllocated <= NumPhysPages);                // check we're not trying
                                                                                // to run anything too big --
                                                                                // at least until we have
                                                                                // virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", numPages, size);
    // first, set up the translation
    TranslationEntry* parentPageTable = parentSpace->GetPageTable();
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
	    if (parentPageTable[i].shared) {
        	pageTable[i].physicalPage = parentPageTable[i].physicalPage;
        }
        else
        {	
    		//pageTable[i].physicalPage = numPagesAllocated;
            if (parentPageTable[i].valid)
            {
                while (PhyPageIsAllocated[k]) k++;
                if (k == NumPhysPages)
                {
                    ASSERT(FALSE);
                }   
                pageTable[i].physicalPage = k;
                PhyPageIsAllocated[k] = TRUE;
                for (j=0 ; j< PageSize; j++){
    	      		machine->mainMemory[(pageTable[i].physicalPage*PageSize)+j] = machine->mainMemory[(parentPageTable[i].physicalPage*PageSize)+j];
                }
            }
    		numPagesAllocated++;
        }
        pageTable[i].valid = parentPageTable[i].valid;
        pageTable[i].use = parentPageTable[i].use;
        pageTable[i].dirty = parentPageTable[i].dirty;
        pageTable[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
        pageTable[i].shared = parentPageTable[i].shared;
    }

    // Copy the contents
   /* unsigned startAddrParent = parentPageTable[0].physicalPage*PageSize;
    unsigned startAddrChild = numPagesAllocated*PageSize;
    for (i=0; i<size; i++) {
	for (j=0; j<numPages; j++){
	if(parentPageTable[j].shared == FALSE)
      		machine->mainMemory[startAddrChild+j] = machine->mainMemory[startAddrParent+j];
	}
    }*/

    //numPagesAllocated += numPages;
//	numPagesAllocated += numPages - j;
}

//----------------------------------------------------------------------------
//CHANGES FOR ASSIGNMENT 3
//AllocateSharedMemory - allocates shared memory when ShmAllocate is called
//-----------------------------------------------------------------------------
unsigned
AddrSpace::AllocateSharedMemory(int size )
{
	unsigned i , k=0;  
	unsigned TotalPages;					//Number of current pages + pages needed to cover the shared memory
	unsigned CurrentPages = GetNumPages();
	unsigned SharedPages = divRoundUp(size, PageSize);
	TotalPages = CurrentPages+SharedPages;
	
	TranslationEntry* oldPageTable = GetPageTable();
	pageTable = new TranslationEntry[TotalPages];
	for (i=0; i<CurrentPages; i++) {
            pageTable[i].virtualPage = i;
            pageTable[i].physicalPage = oldPageTable[i].physicalPage;
            pageTable[i].valid = oldPageTable[i].valid;
            pageTable[i].use = oldPageTable[i].use;
            pageTable[i].dirty = oldPageTable[i].dirty;
            pageTable[i].readOnly = oldPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
	    pageTable[i].shared = oldPageTable[i].shared;

	}
	for (i=CurrentPages; i<TotalPages; i++) {
           pageTable[i].virtualPage = i;

            while (PhyPageIsAllocated[k]) k++;
            if (k == NumPhysPages)
            {
                ASSERT(FALSE);
            }   
            pageTable[i].physicalPage = k;
            PhyPageIsAllocated[k] = TRUE;

           //pageTable[i].physicalPage = i+numPagesAllocated - CurrentPages;
           pageTable[i].valid = TRUE;
           pageTable[i].use = FALSE;
           pageTable[i].dirty = FALSE;
           pageTable[i].readOnly = FALSE;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
            pageTable[i].shared = TRUE;
            stats->numPageFaults++;

	}
	numPages = TotalPages;
	numPagesAllocated += SharedPages;

	machine->pageTable = pageTable;
	machine->pageTableSize = TotalPages;

	delete oldPageTable;
	return CurrentPages*PageSize;
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
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
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

unsigned
AddrSpace::GetNumPages()
{
   return numPages;
}

TranslationEntry*
AddrSpace::GetPageTable()
{
   return pageTable;
}

//---------------------------------------------------------------------
//AddrSpace::CopyContent(OpenFile *executable, int vpn)
//To copy the contents at the time of pageframe allocation
//---------------------------------------------------------------------
void
AddrSpace::CopyContent(unsigned int pageFrame, unsigned vpn)
{
    unsigned overlap_start, overlap_end;
    unsigned copy_vpn, copy_offset;
    unsigned size;
    TranslationEntry *copy_entry;
    NoffHeader noffH;
    // entry = &pageTable[vpn];
    OpenFile *executable = fileSystem->Open(currentFile);
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);
   
   /* size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
            + UserStackSize;    // we need to increase the size
                        // to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    char* temp_array;
    temp_array = new char[size];


    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
            noffH.code.virtualAddr, noffH.code.size);
        copy_vpn = noffH.code.virtualAddr/PageSize;
        copy_offset = noffH.code.virtualAddr%PageSize;
        copy_entry = &pageTable[copy_vpn];
        //pageFrame = entry->physicalPage;
        executable->ReadAt(&(temp_array[pageFrame * PageSize + copy_offset]),
            noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
            noffH.initData.virtualAddr, noffH.initData.size);
        copy_vpn = noffH.initData.virtualAddr/PageSize;
        copy_offset = noffH.initData.virtualAddr%PageSize;
        copy_entry = &pageTable[copy_vpn];
        // pageFrame = entry->physicalPage;
        executable->ReadAt(&(temp_array[pageFrame * PageSize + copy_offset]),
            noffH.initData.size, noffH.initData.inFileAddr);
    }

    for (int j=0 ; j< PageSize; j++){
                machine->mainMemory[(pageFrame*PageSize)+j] = temp_array[j];
    }*/
   if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", noffH.code.virtualAddr, noffH.code.size);
        copy_vpn = noffH.code.virtualAddr/PageSize;
        copy_offset = noffH.code.virtualAddr%PageSize;
        copy_entry = &pageTable[copy_vpn];
        // pageFrame = entry->physicalPage;
        unsigned code_start = noffH.code.virtualAddr;
        unsigned code_end = (noffH.code.size + noffH.code.virtualAddr) - 1;
        unsigned page_start = vpn * PageSize;
        unsigned page_end = (page_start + PageSize)-1;
        if (code_start > page_start)
        {
            overlap_start = code_start;
        }
        else
        {
            overlap_start = page_start;
        }
        if (code_end < page_end)
        {
            overlap_end = code_end;
        }
        else
        {
            overlap_end = page_end;
        }

        executable->ReadAt(&(machine->mainMemory[pageFrame*PageSize + copy_offset]), (overlap_end - overlap_start) + 1, noffH.code.inFileAddr + overlap_start - noffH.code.virtualAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", noffH.initData.virtualAddr, noffH.initData.size);
        copy_vpn = noffH.initData.virtualAddr/PageSize;
        copy_offset = noffH.initData.virtualAddr%PageSize;
        copy_entry = &pageTable[copy_vpn];
        // pageFrame = entry->physicalPage;
        unsigned data_end = (noffH.initData.size + noffH.initData.virtualAddr) - 1;
        unsigned page_start = vpn * PageSize;
        unsigned data_start = noffH.initData.virtualAddr;
        unsigned page_end = (page_start + PageSize)-1;
        
        if (data_start > page_start)
        {
            overlap_start = data_start;
        }
        else
        {
            overlap_start = page_start;
        }
        if (data_end < page_end)
        {
            overlap_end = data_end;
        }
        else
        {
            overlap_end = page_end;
        }

        executable->ReadAt(&(machine->mainMemory[pageFrame*PageSize + copy_offset]), (overlap_end - overlap_start) + 1, noffH.initData.inFileAddr + overlap_start - noffH.initData.virtualAddr);
    } 

    
}
