README

Stephen Tredger - v00185745
Josh Erickson 	- v00218296


Deviations:

We do not handle the athrow opcode.
We also do not handle inheritance in some cases (eg. aastore)


Memory Leaks:
Unfortunately we have a large number of memory leaks
We were focused on the verification so we use a mix of static strings and stuff on the heap
This made tracking when and when not to free quite complicated, so we decided instead of 
aborting when trying to free memory not on the heap, that we would just print a warning.



JVM State Woes:
Our JVM state is handled incorrectly is some cases,
We keep the state associated with the opcode, unfortunately when the opcode changes the state
it keeps the changed state instead of reverting to the state it had before the opcodes execution.
ie. iload will have the I on the stack for a state, even though it didnt before. 

This makes some brnaching statements fail verification.
We could resolve this by manipulating a temporary state and leacving 
the opcodes initial state alone, but unfortunately never impelented this solution.



Issues:
  - Array Syntax:
      - There are different formats everywhere 
            LL vs. L vs. J
            DD vs. D
            A[ALMyClass vs. A[LMyClass
            java.lang.String vs. java/lang/String;
  
      - In the assignment specs, it states the use of 
            A[I
            A[L
            A[D
            A[A[Ljava.lang.String
      - In the values taken from the constant pool (multi-dimensional arrays), we get 
            [[I                     which we turn into    A[A[I
            [[[J                    which we turn into    A[A[A[J
            [[[D                    which we turn into    A[A[A[D
            [[Ljava/lang/String;    which we turn into    A[A[Ljava/lang/String;
      - In the initialization (i.e. arguments passed into a method), we get
            A[I
            A[Ll
            A[Dd
            A[A[ALjava/lang/String



- Double/long Issue:

From testing it looks like doubles and longs take up TWO local variable spots


public static void ds(double a, double b) {
 a = b;
}


javap -c:

public static void ds(double, double);
  Code:
   Stack=2, Locals=4, Args_size=2
   0:	dload_2
   1:	dstore_0
   2:	return
  LineNumberTable: 
   line 45: 0
   line 46: 2



Unfortunately we discovered this late is testing, and the function that
returns the inital state places doubles and longs into one local.
We had expected it was working like this prevously as when a function uses a double/long as 
the last local, this does not become a problem

Inital state:

Method ds:
  V0:  Dd
  V1:  Dd
  V2:  U
  V3:  U
  S0:  -
  S1:  -

Instruction 0 dload_2:
Pre Instruction Instruction Info:
  cbit:    1
  stksize: 0
  Locals:  (Dd Dd U U )
  Stack:   (- - )
Type mismatch:  U =/= Dd
Verification Failed at instruction 0 : dload_2



- Stack Issue:

We had originally placed arguments on the stack from right to left, (ie. A was on the top below)
making the top of the stack on the right. However java places them the other way (I is on the top)
so we had to swap the way we pushed halfway through our implementation.
{ 0X4f, "iastore",      "", "AII>" },
