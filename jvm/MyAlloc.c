// Stephen Tredger, V00185745
// Josh Erickson, V00218296


/* MyAlloc.c */

/*
   All memory allocation and deallocation is performed in this module.
   
   There are two families of functions, one just for managing the Java heap
   and a second for other memory requests.  This second family of functions
   provides wrappers for standard C library functions but checks that memory
   is not exhausted and zeroes out any returned memory.
   
   Java Heap Management Functions:
   * InitMyAlloc  -- initializes the Java heap before execution starts
   * MyHeapAlloc  -- returns a block of memory from the Java heap
   * gc           -- the System.gc garbage collector
   * MyHeapFree   -- to be called only by gc()!!
   * PrintHeapUsageStatistics  -- does as the name suggests

   General Storage Functions:
   * SafeMalloc  -- used like malloc
   * SafeCalloc  -- used like calloc
   * SafeStrdup  -- used like strdup
   * SafeFree    -- used like free
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ClassFileFormat.h"
#include "ClassResolver.h"
#include "TraceOptions.h"
#include "MyAlloc.h"
#include "jvm.h"

/* we will never allocate a block smaller than this */
#define MINBLOCKSIZE 12

/* or larger than this! */
#define MAXBLOCKSIZE (((uint32_t)(~0x80000000)) - 4)

/* this pattern will appear in blocks in the freelist  */
#define FREELISTBITPATTERN 0x0BADA550

/* this is the 32 bit bitmask for the mark bit */
#define MARKBIT 0x80000000


typedef struct FreeStorageBlock {
    uint32_t size;  /* size in bytes of this block of storage */
    int32_t  offsetToNextBlock;
    uint8_t  restOfBlock[1];   /* the actual size has to be determined from the size field */
} FreeStorageBlock;

/* these three variables are externally visible */
uint8_t *HeapStart, *HeapEnd;
HeapPointer MaxHeapPtr;

static int offsetToFirstBlock = -1;
static long totalBytesRequested = 0;
static int numAllocations = 0;
static int gcCount = 0;
static long totalBytesRecovered = 0;
static int totalBlocksRecovered = 0;
static int searchCount = 0;

static void *maxAddr = NULL;    // used by SafeMalloc, etc
static void *minAddr = NULL;



/* prints a byte in readable format, ie. xxxx xxxx */
static void printByte(char byte) {
	int i;
	uint8_t mask = 0x01 << 7;
	for (i = 7; i >= 0; i--, mask=mask>>1) {
		if (i == 3)
			printf(" ");
		if (byte & mask)
			printf("1");
		else
			printf("0");
	}
}

/* Prints the bits of a given value in a readable format.
   Expects 'numBytes' to be the number of bytes pointed to by 'val'.
   Note: bits will be printed as they are stored in mem, watch
   for endianness */
static void printBits(void *val, int numBytes) {
	printf("Printing %d bytes starting at address %p\n",
		   numBytes, val);
	int i;
	char *bytePtr = (char*) val;
	for (i = 0; i < numBytes; i++) {
		if ( i % 2 )
			printf("   ");
		else if (i == 0)
			printf("\t");
		else
			printf("\t\n");
		printByte(*bytePtr++);
	}
}

static void printBlock(void *p) {
    printf("Address: %p\n", p);
    
    char *bytePtr = (char*) p;
    int i;
    for(i = 0; i < 4; i++) {
        printf("    ");
        printByte(*bytePtr++);
    }
    printf("\tSize (=%d)\n", ~MARKBIT & *(uint32_t *)p);
    
    for(i = 0; i < 4; i++) {
        printf("    ");
        printByte(*bytePtr++);
    }

    if((MARKBIT & *(uint32_t *)p)) {
         printf("\tContent\n");
    }
    else {
        printf("\tContent (if garbage) / Reference to next free block (if free)\n");
    }

    for(i = 0; i < 4; i++) {
        printf("    ");
        printByte(*bytePtr++);
    }
    printf("\tContent (if active or garbage) / FreePattern (if free)\n");
    
    printf("    ...");
    for(i=0; i<49; i++)
        printf(" ");
    printf("Possibly more content\n");
    
}

static void printStack() {
    DataItem *Stack_Iterator = JVM_Top;
    
    printf("\n================\n");
    printf("Stack -- Top\n");
    printf("================\n");
    while(Stack_Iterator >= JVM_Stack) {
        printf("ival: %d \tuval: %d \tfval: %f \tpval: %p\n", Stack_Iterator->ival, Stack_Iterator->uval, Stack_Iterator->fval, REAL_HEAP_POINTER(Stack_Iterator->pval));
        
        Stack_Iterator--;
    }
    printf("================\n");
    printf("Stack -- Bottom\n");
    printf("================\n\n");
}

static void printHeap() {
    printf("\n=========================\n");
    printf("Heap -- Start - %p\n", HeapStart);
    printf("=========================\n");
    
    HeapPointer Heap_Iterator = 0;
    while(Heap_Iterator < MaxHeapPtr) {
        printBlock(REAL_HEAP_POINTER(Heap_Iterator));
        Heap_Iterator += ~MARKBIT & *(uint32_t *)REAL_HEAP_POINTER(Heap_Iterator);
    }

    printf("=======================\n");
    printf("Heap -- End - %p\n", HeapEnd);
    printf("=======================\n\n");
}


/* Allocate the Java heap and initialize the free list */
void InitMyAlloc( int HeapSize ) {
    FreeStorageBlock *FreeBlock;

    HeapSize &= 0xfffffffc;   /* force to a multiple of 4 */
    HeapStart = calloc(1,HeapSize);
    if (HeapStart == NULL) {
        fprintf(stderr, "unable to allocate %d bytes for heap\n", HeapSize);
        exit(1);
    }
    HeapEnd = HeapStart + HeapSize;
    MaxHeapPtr = (HeapPointer)HeapSize;
    
    FreeBlock = (FreeStorageBlock*)HeapStart;
    FreeBlock->size = HeapSize;
    FreeBlock->offsetToNextBlock = -1;  /* marks end of list */
    *(uint32_t *)FreeBlock->restOfBlock = FREELISTBITPATTERN;
    offsetToFirstBlock = 0;
    
    // Used bu SafeMalloc, SafeCalloc, SafeFree below
    maxAddr = minAddr = malloc(4);  // minimal small request to get things started
}

/* Returns a pointer to a block with at least size bytes available,
   and initialized to hold zeros.
   Notes:
   1. The result will always be a word-aligned address.
   2. The word of memory preceding the result address holds the
      size in bytes of the block of storage returned (including
      this size field).
   3. A block larger than that requested may be returned if the
      leftover portion would be too small to be useful.
   4. The size of the returned block is always a multiple of 4.
   5. The implementation of MyAlloc contains redundant tests to
      verify that the free list blocks contain plausible info.
*/
void *MyHeapAlloc( int size ) {
    /* we need size bytes plus more for the size field that precedes
       the block in memory, and we round up to a multiple of 4 */
    int offset, diff, blocksize;
    FreeStorageBlock *blockPtr, *prevBlockPtr, *newBlockPtr;
    int minSizeNeeded = (size + sizeof(blockPtr->size) + 3) & 0xfffffffc;

    // we use the top bit for marking and therefore our size 
    //  is bound to be between 0 and 2^31-1
    if (size < MINBLOCKSIZE || size > MAXBLOCKSIZE) {
      fprintf(stderr, 
	      "request for invalid amount of heap - req: %d, max: %d, min: %d\n",
	      size, MAXBLOCKSIZE, MINBLOCKSIZE);
      exit(1);
    }

    if (tracingExecution & TRACE_HEAP)
        fprintf(stdout, "* heap allocation request of size %d (augmented to %d)\n",
            size, minSizeNeeded);
    blockPtr = prevBlockPtr = NULL;
    offset = offsetToFirstBlock;
    while(offset >= 0) {
        searchCount++;
        blockPtr = (FreeStorageBlock*)(HeapStart + offset);
        /* the following check should be quite unnecessary, but is
           a good idea to have while debugging */
        if ((offset&3) != 0 || (uint8_t*)blockPtr >= HeapEnd) {
            fprintf(stderr,
                "corrupted block in the free list -- bad next offset pointer\n");
            exit(1);
        }
        blocksize = blockPtr->size;
        /* the following check should be quite unnecessary, but is
           a good idea to have while debugging */
        if (blocksize < MINBLOCKSIZE || (blocksize&3) != 0) {
            fprintf(stderr,
                "corrupted block in the free list -- bad size field\n");
            exit(1);
        }
        diff = blocksize - minSizeNeeded;
        if (diff >= 0) break;
        offset = blockPtr->offsetToNextBlock;
        prevBlockPtr = blockPtr;
    }
    if (offset < 0) {
        static int gcAlreadyPerformed = 0;
        void *result;
        if (gcAlreadyPerformed) {
            /* we are in a recursive call to MyAlloc after a gc */
            fprintf(stderr,
                "\nHeap exhausted! Unable to allocate %d bytes\n", size);
            exit(1);
        }
        gc();
        gcAlreadyPerformed = 1;
        result = MyHeapAlloc(size);
        /* control never returns from the preceding call if the gc
           did not obtain enough storage */
        gcAlreadyPerformed = 0;
        return result;
    }
    /* we have a sufficiently large block of free storage, now determine
       if we will have a significant amount of storage left over after
       taking what we need */
    if (diff < MINBLOCKSIZE) {
        /* we will return the entire free block that we found, so
           remove the block from the free list  */
        if (prevBlockPtr == NULL)
            offsetToFirstBlock = blockPtr->offsetToNextBlock;
        else
            prevBlockPtr->offsetToNextBlock = blockPtr->offsetToNextBlock;
        if (tracingExecution & TRACE_HEAP)
            fprintf(stdout, "* free list block of size %d used\n", blocksize);
    } else {
        /* we split the free block that we found into two pieces;
           blockPtr refers to the piece we will return;
           newBlockPtr will refer to the remaining piece */
        blockPtr->size = minSizeNeeded;
        newBlockPtr = (FreeStorageBlock*)((uint8_t*)blockPtr + minSizeNeeded);
        /* replace the block in the free list with the leftover piece */
        if (prevBlockPtr == NULL)
            offsetToFirstBlock += minSizeNeeded;
        else
            prevBlockPtr->offsetToNextBlock += minSizeNeeded;
        newBlockPtr->size = diff;
        newBlockPtr->offsetToNextBlock = blockPtr->offsetToNextBlock;
        *(uint32_t *)newBlockPtr->restOfBlock = FREELISTBITPATTERN;
        printBits(newBlockPtr->restOfBlock, 4);
        
        if (tracingExecution & TRACE_HEAP)
            fprintf(stdout, "* free list block of size %d split into %d + %d\n",
		    diff+minSizeNeeded, minSizeNeeded, diff);
    }
    blockPtr->offsetToNextBlock = 0;  /* remove this info from the returned block */
    totalBytesRequested += minSizeNeeded;
    numAllocations++;
    
    // Remove the FREELISTBITPATTERN
    *(uint32_t *)blockPtr->restOfBlock = 0;

    return (uint8_t*)blockPtr + sizeof(blockPtr->size);
}


/* When garbage collection is implemented, this function should never
   be called from outside the current file.
   This implementation checks that p is plausible and that the block of
   memory referenced by p holds a plausible size field.
*/
static void MyHeapFree(void *p) {
    uint8_t *p1 = (uint8_t*)p;
    int blockSize;
    FreeStorageBlock *blockPtr, *freelistBlock;
	
    if (p1 < HeapStart || p1 >= HeapEnd || ((p1-HeapStart) & 3) != 0) {
        fprintf(stderr, "bad call to MyHeapFree -- bad pointer\n");
        exit(1);
    }
    /* step back over the size field */
    p1 -= sizeof(blockPtr->size);
    /* now check the size field for validity */
    blockSize = *(uint32_t*)p1;
    
    if (blockSize < MINBLOCKSIZE || (p1 + blockSize) > HeapEnd || (blockSize & 3) != 0) {
        fprintf(stderr, "bad call to MyHeapFree -- invalid block\n");
        exit(1);
    }
	
	blockPtr = (FreeStorageBlock*)p1;

	// there is already something in the freelist, so see if we can combine
	if (offsetToFirstBlock > -1) {
		freelistBlock = (FreeStorageBlock*) REAL_HEAP_POINTER(offsetToFirstBlock);

		if (freelistBlock->size + (unsigned long) freelistBlock == blockPtr) {

			printf("Combining free blocks %p and %p\n", freelistBlock, blockPtr);
			// p1 is the next block so combine sizes and we are done
			freelistBlock->size += blockPtr->size;
			return;
		}
	}
   
    // TEMP
    printf("Adding Block to Freelist - Block size = %d Pointer = %p Heap end = %p\n",
		   blockSize, p1, HeapEnd);

    /* link the block into the free list at the front */
    blockPtr->offsetToNextBlock = offsetToFirstBlock;

    // add bit pattern for stuff in freelist
    *(uint32_t*)blockPtr->restOfBlock = FREELISTBITPATTERN;
    
    offsetToFirstBlock = p1 - HeapStart;
}


/* This implements garbage collection.
   It should be called when
   (a) MyAlloc cannot satisfy a request for a block of memory, or
   (b) when invoked by the call System.gc() in the Java program.
*/
void gc() {
    gcCount++;
    
    // TEMP
    printf("\nStarting Garbage Collection...\n");
    printf("\nCLASSFILES\n=====\n");
    // END TEMP

	// We must mark this fake file descriptor
	mark(Fake_System_Out);

	// The class list is a linked list so mark, being recursive,
	//  should get all the class files on the heap. However, to
	//  be really safe call mark on all the classfiles manually
	ClassType *ct = FirstLoadedClass;
	while (ct) {
		mark(ct);
		printf("class: %p, nextclass: %p\n", ct, ct->nextClass);
		ct = ct->nextClass;
	}
    
    printStack();
    
    DataItem *Stack_Iterator = JVM_Top;
    while(Stack_Iterator >= JVM_Stack) {
        if(isProbablePointer(REAL_HEAP_POINTER(Stack_Iterator->pval))) {
              mark(REAL_HEAP_POINTER(Stack_Iterator->pval));
        }
        Stack_Iterator--;
    }
      
    // TEMP
    printf("\n");
    // END TEMP
    
    sweep();
    
}

/* Returns 1 if the pointer p is a valid pointer into the 
   java heap, 0 otherwise */
int isProbablePointer(void *p) {
    
	// check the pointer is valid
	if ( (uint8_t) p % 4 || // must be 4 byte alligned
		 // the first valid pointer is HeapStart + 4
		 p < (void*)(HeapStart + 4) ||
		 // and the last is HeapEnd - MINBLOCKSIZE
		 p > ( (void*)(HeapEnd - MINBLOCKSIZE) )) {
		return 0;
	}
	
	// check the block has a valid size
	uint32_t blockSize = *( ((uint32_t*) p) - 1 );
	if (blockSize > MAXBLOCKSIZE || 
		blockSize < MINBLOCKSIZE || 
		blockSize > (HeapEnd - HeapStart) ||
		blockSize % 4) { // block sizes are always a multiple of 4
		return 0;
	}
	return 1;
}


void mark(uint32_t *block) {
	uint32_t size, i;
	
	printf("mark(): Checking ptr %p\n", block);
	
	// back up 4 bytes to get at the size field of the block
	uint32_t *blockMetadata = block - 1;
	//printBits(blockMetadata, 4);
	
	if ( !(*blockMetadata & MARKBIT) ) {
		printf("mark(): Marking ptr %p\n", block);
		size = (*blockMetadata - 4) / sizeof(uint32_t); // get the number of remaining 32bit spots
		//printf("size: %d, numEntires: %d\n", size*4 + 4, size);
		*blockMetadata |= MARKBIT;
		//printBits(blockMetadata, 4);
		for (i = 0; i < size; i++) {
			//printf("pos: %d, size: %d, block[i]: %d, ptr: %p\n", i, size, block[i], REAL_HEAP_POINTER(block[i]));
			if ( isProbablePointer((uint32_t*) REAL_HEAP_POINTER(block[i])) ) {
				mark((uint32_t*) REAL_HEAP_POINTER(block[i]));
			}
		}
	} else {
		printf("mark(): Ptr %p already marked\n", block);
	}
}


void sweep() {
	
    printHeap();

	// we rebuild the freelist at each gc, so reset it!
    offsetToFirstBlock = -1;

    HeapPointer Heap_Iterator = 0;
    while(Heap_Iterator < MaxHeapPtr) {
   
        //printBlock(REAL_HEAP_POINTER(Heap_Iterator));
        
        if( !(MARKBIT & *(uint32_t *)REAL_HEAP_POINTER(Heap_Iterator)) ) {
			// we are not marked, if we were not in the 
			//  previous freelist we are garbage, so lets 
			//  collect some stats!
            if (FREELISTBITPATTERN != 
				*(uint32_t *)REAL_HEAP_POINTER(Heap_Iterator + 8)) {
				
                printf("sweep(): Found garbage at %p\n", 
					   REAL_HEAP_POINTER(Heap_Iterator)); 
                
                // Statistics tracking
                totalBytesRecovered += 
					*(uint32_t *)REAL_HEAP_POINTER(Heap_Iterator);
                totalBlocksRecovered++;
            }
			MyHeapFree(REAL_HEAP_POINTER(Heap_Iterator + 4));
 
        } else { 
            // Unmark
            *(uint32_t *)REAL_HEAP_POINTER(Heap_Iterator) &= ~MARKBIT;
        }

        // Move 'size' bytes to next block (ignoring the Mark Bit when determining size)
        Heap_Iterator += ~MARKBIT & *(uint32_t *)REAL_HEAP_POINTER(Heap_Iterator);
        
        //printBits((char *)REAL_HEAP_POINTER(Heap_Iterator), sizeof(HeapPointer));
    }
}


/* Report on heap memory usage */
void PrintHeapUsageStatistics() {
    printf("\nHeap Usage Statistics\n=====================\n\n");
    printf("  Number of blocks allocated = %d\n", numAllocations);
    if (numAllocations > 0) {
        float avgBlockSize = (float)totalBytesRequested / numAllocations;
        float avgSearch = (float)searchCount / numAllocations;
        printf("  Average size of allocated blocks = %.2f\n", avgBlockSize);
        printf("  Average number of blocks checked = %.2f\n", avgSearch);
    }
    printf("  Number of garbage collections = %d\n", gcCount);
    if (gcCount > 0) {
        float avgRecovery = (float)totalBytesRecovered / gcCount;
        printf("  Total storage reclaimed = %ld\n", totalBytesRecovered);
        printf("  Total number of blocks reclaimed = %d\n", totalBlocksRecovered);
        printf("  Average bytes recovered per gc = %.2f\n", avgRecovery);
    }
}

static void *trackHeapArea( void *p ) {
    if (p > maxAddr)
        maxAddr = p;
    if (p < minAddr)
        minAddr = p;
    return p;
}


void *SafeMalloc( int size ) {
    return SafeCalloc(1,size);
}


void *SafeCalloc( int ncopies, int size ) {
    void *result;
    result = calloc(ncopies,size);
    if (result == NULL) {
        fprintf(stderr, "Fatal error: memory request cannot be satisfied\n");
        exit(1);
    }
    trackHeapArea(result);
    return result;    
}


char *SafeStrdup( char *s ) {
    char *r;
    int len;

    len = (s == NULL)? 0 : strlen(s);
    r = SafeMalloc(len+1);
    if (len > 0)
        strcpy(r,s);
    return r;
}

char *SafeStrcat( char *a, char *b) {
	char *s;
	int len;
	
	len = ((a == NULL) || (b == NULL))? 0 : strlen(a) + strlen(b);
	s = SafeMalloc(len+1);
	if (len > 0) {
		strcpy(s,a);
		strcat(s,b);
	}
	return s;
}


void SafeFree( void *p ) {
    if (p == NULL || ((int)p & 0x7) != 0) {
        fprintf(stderr, "Fatal error: invalid parameter passed to SafeFree\n");
        fprintf(stderr, "    The address was NULL or misaligned\n");
	abort();
    }
    if (p >= minAddr && p <= maxAddr)
        free(p);
    else {
        fprintf(stderr, "Fatal error: invalid parameter passed to SafeFree\n");
        fprintf(stderr, "     The memory was not allocated by SafeMalloc\n");
        abort();
    }
}
