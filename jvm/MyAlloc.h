// Stephen Tredger, V00185745
// Josh Erickson, V00218296

/* MyAlloc.h */

#ifndef MYALLOCH

#define MYALLOCH

#include <stdint.h>

/* All pointers into the JVM Heap are implemented as
   offsets from the base of the heap area.
   This allows us to use 4 bytes for a heap pointer,
   even on a machine with 64-bit words.  */
typedef uint32_t HeapPointer;

extern uint8_t *HeapStart, *HeapEnd;
extern HeapPointer MaxHeapPtr;

extern void InitMyAlloc( int HeapSize );
extern void *MyHeapAlloc( int size );
extern void gc();
extern void PrintHeapUsageStatistics();
int isProbablePointer(void *real_heap_pointer);
void mark();
void sweep();

extern char *SafeStrdup( char *s );
extern char *SafeStrcat( char *a, char *b );
extern void *SafeMalloc( int size );
extern void *SafeCalloc( int ncopies, int size );
extern void SafeFree( void *p );

#endif
