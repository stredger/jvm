README

Stephen Tredger - v00185745
Josh Erickson 	- v00218296


Tracing:
    - Setting tracingExecution = TRACE_HEAP; in TraceOptions.c will show the heap 
      allocation, garbage collection process and garbage collection statistics.
    - Setting tracingExecution = TRACE_HEAP | TRACE_GC; will also show the state
      of the stack and heap during garbage collection.
      
Marking:
    - We use the most-significant bit of the size field as the "mark" bit. As a
      result, we limit the maximimum size of our blocks to 2^31 bytes.
      
Free List:
    - We use a "Free List" bit code in the free blocks to distinguish them from
      garbage (as both are unmarked during the marking phase). During the sweep
      phase, we "rebuild" the free list as we move through the heap, adding both
      garbage and free blocks as we encounter them. We found this to be the most
      efficient method of adding garbage blocks (and combining them) into the 
      free list.
    
Deviations:
    - We did not use the 'kind' codes to determine how to handle each type on the 
      heap. Instead, we used a general approach for every type, checking each
      four byte word of the block to determine if it is a potential pointer. We
      felt this was the best way to do it and have had good success with our
      approach. The 'kind' codes are used as one of the checks for potential
      pointers - the check is to ensure that the pointer is pointing to a valid
      object on the heap.