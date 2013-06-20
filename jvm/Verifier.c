/* Verifier.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ClassFileFormat.h"
#include "OpcodeSignatures.h"
#include "TraceOptions.h"
#include "MyAlloc.h"
#include "VerifierUtils.h"
#include "Verifier.h"

// Output an array of the verifier's type descriptors
/*
static void printTypeCodesArray( char **vstate, method_info *m, char *name ) {
  int i;
  if (name != NULL)
    fprintf(stdout, "\nMethod %s:\n", name);
  else
    fputc('\n', stdout);
  for( i = 0;  i < m->max_locals; i++ )
    fprintf(stdout, "  V%d:  %s\n", i, *vstate++);
  for( i = 0;  i < m->max_stack; i++ )
    fprintf(stdout, "  S%d:  %s\n", i, *vstate++);
}
*/

static void printConstantPool(ClassFile *cf) {
  char *cptypes[] = {"","Asciz","","","","","","class","","","Method","","NameAndType"};
  int i;
  fprintf(stdout, "Constant Pool:\n");
  for (i = 1; i < cf->constant_pool_count; i++) {
    fprintf(stdout, "  #%d: %s\t%s\n", i, cptypes[cf->cp_tag[i]], GetCPItemAsString(cf, i));
  }
}


static void printAllInstructions(method_info *m, char *name) {
  int i;
  if (name == NULL)
    name = "";
  fprintf(stdout, "All Instructions for Method %s:\n", name);
  for (i = 0; i < m->code_length; i++) {
    if (m->code[i] <= LASTOPCODE)
      fprintf(stdout, "  %d : %s\t%d\n", i, opcodes[m->code[i]].opcodeName, m->code[i]);
    else
      fprintf(stdout, "  %d : %s\t%d\n", i, "(data)", m->code[i]);
  }
}


static void printInstructionInfo(InstructionInfo *iinfo, int localslen, int stacklen) {
  int j;
  char **vstate = iinfo->state;
  fprintf(stdout, "Instruction Info:\n");
  fprintf(stdout, "  cbit:    %d\n", iinfo->cbit);
  fprintf(stdout, "  stksize: %d\n", iinfo->stksize);
  fprintf(stdout, "  Locals:  (");
  for( j = 0;  j < localslen; j++ )
    fprintf(stdout, "%s ", *vstate++);
  fprintf(stdout, ")\n  Stack:   (");
  for( j = 0;  j < stacklen; j++ )
    fprintf(stdout, "%s ", *vstate++);
  fprintf(stdout, ")\n");
}

/*
static char **createInitialState(int localslen, int stacklen) {
  int i, size = localslen + stacklen;
  char **s2 = (char **) SafeMalloc( (stacklen + localslen) * sizeof(char **) );

  for( i = 0;  i < localslen;  i++ )
    s2[i] = "U";
  while(i < size)
    s2[i++] = "-";
  return s2;
}
*/

static InstructionInfo *createInstructionTable(method_info *m, char** initState) {
  InstructionInfo *itable = (InstructionInfo*) SafeCalloc(m->code_length, sizeof(InstructionInfo));
  //int i;
  itable[0].cbit = 1;
  itable[0].state = initState;
  //for (i = 1; i < m->code_length; i++) {
  //  itable[i].state = (m->code[i] > LASTOPCODE || m->code[i] == 0) ? 
  //    NULL : createInitialState(m->max_locals, m->max_stack);
  //}
  return itable;
}


static char **copyState(char **s1, int len) {
  int i;
  char **s2 = (char **) SafeMalloc(len * sizeof(char **));
  for (i = 0; i < len; i++)
    s2[i] = SafeStrdup(s1[i]);
  return s2;
}


static void dupInstructionInfo(InstructionInfo *oldi, InstructionInfo *newi, int typeArrayLen) {
  newi->cbit = 1;
  newi->state = copyState(oldi->state, typeArrayLen);
  newi->stksize = oldi->stksize;
}


// here we use 1 to say yes we are a simple type, 
//  and 0 to say no
static int isSimpleType(char *t) {
  if (*t == 'A')
    return 0; // ref type
  return 1; // simple
}



static char *mergeTypes(char *t1, char*t2) {

  if ( strcmp(t1, t2) == 0 ) // t & t => t
    return t2;

  // now we know t1 != t2 so do some fun stuff
  if ( strcmp(t1, "X") == 0 || // t & X => X
       strcmp(t2, "X") == 0 )
    return "X";
  if ( strcmp(t1, "U") == 0 || // t & U => U
       strcmp(t2, "U") == 0 )
    return "U";
  if ( isSimpleType(t1) || // s1 & s2 or s & a => x
       isSimpleType(t2) )
    return "X";

  // now we only have ref types
  if ( strcmp(t1, "N") == 0 )
    return t2;
  if ( strcmp(t2, "N") == 0 )
    return t1;
  return LUB(t1, t2);
}



static int mergeState(InstructionInfo* oldi, InstructionInfo* newi, int typeArrLen) {
  
  if ( oldi->stksize != newi->stksize ) {
    fprintf(stdout, "Stack heights don't match: %d =/= %d\n", oldi->stksize, newi->stksize);
    return -1;
  }
  int i;
  for (i = 0; *(oldi->state[i]) != '-' && i < typeArrLen; i++) {
    newi->state[i] = mergeTypes(oldi->state[i], newi->state[i]);
  }
  newi->cbit = 1;
  return 0;
}


static int findChangedInstruction(InstructionInfo *itable, int ipos, int imax) {
  int numchecked;
  while (numchecked < imax) {
    if (itable[ipos].state && itable[ipos].cbit) {
      if (tracingExecution & TRACE_VERIFY)
	fprintf(stdout, "Found instruction to verify at: %d\n", ipos);
      return ipos;
    }
    ipos = (ipos + 1) % imax;
    numchecked++;
  }
  if (tracingExecution & TRACE_VERIFY)
    fprintf(stdout, "Failed to find more instructions to verify\n");
  return -1;
}


static int checkStackOverflow(int stksize, int toPush,  int max) {
  if (stksize + toPush > max) {
    fprintf(stdout, "Stack Overflow!\n");
    return -1;
  }
  return 0;
}


static int checkStackUnderflow(int stksize, int toPop) {
  if (stksize - toPop < 0) {
    fprintf(stdout, "Stack Underflow!\n");
    return -1;
  }
  return 0;
}


static void updateInstruction(InstructionInfo *icurr, InstructionInfo *inext, int typeArrSize) {
    if (inext->state) {
      // merge the states, stacks, and bears. Oh my!
      mergeState(icurr, inext, typeArrSize);
    } else {
      // make a new instruction entry
      dupInstructionInfo(icurr, inext, typeArrSize);
    }
}


static int compareSimpleTypes(char* t1, char *t2) {
  if ( strcmp(t1, t2) ) {
    fprintf(stdout, "Type mismatch:  %s =/= %s\n", t1, t2);
    return -1;
  }
  return 0;
}


static int compareReferenceTypes(char *t1, char *t2) {
  int i;
  for (i = 0; t1[i] != '\0' && t2[i] != '\0'; i++) {
    if (t1[i] != t2[i]) {
      fprintf(stdout, "Type mismatch:  %s =/= %s\n", t1, t2);
      return -1;
    }
  }
  return 0;
}


static int checkInLocalsRange(int varnum, int max) {
  if (varnum >= max) {
    fprintf(stdout, "Local var out of range, tried: %d max:%d\n", varnum, max-1);
    return -1;
  }
  return 0;
}


static int checkJumpPosition(int pos, InstructionInfo *itable, int imax) {
  if (pos >= imax) {
    fprintf(stdout, "Code position out of range, tried: %d max:%d\n", pos, imax-1);
    return -1;
  }
  // check pos has valid bit set
  return 0;
}


static int verifyOpcode(InstructionInfo *itable, ClassFile *cf, method_info *m, int ipos) {

  int op = m->code[ipos];
  char **localsbase = itable[ipos].state;
  char **stackbase = &itable[ipos].state[m->max_locals];
  char *tmpStr;
  int *stkSizePtr = &itable[ipos].stksize;
  int typeArrSize = m->max_locals + m->max_stack;
  int varnum;

  // say that we verified the instruction before we actually do it
  itable[ipos].cbit = 0;

  // could have a big goto table eventually
  switch (op) {

  case 0x00: // nop
    // just move to the next instruction
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x01: // aconst_null
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 )
      return -1; // verification failed
    stackbase[(*stkSizePtr)++] = "N"; // push N onto stack
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x02: // iconst_m1
  case 0x03: // iconst_0
  case 0x04: // iconst_1
  case 0x05: // iconst_2
  case 0x06: // iconst_3
  case 0x07: // iconst_4
  case 0x08: // iconst_5
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "I"; // push I onto stack
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;
    
  case 0x09: // lconst_0
  case 0x0a: // lconst_1
    if ( checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 )
      return -1;  
    stackbase[(*stkSizePtr)++] = "l"; // push l onto stack
    stackbase[(*stkSizePtr)++] = "L"; // push L onto stack
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x0b: // fconst_0
  case 0x0c: // fconst_1
  case 0x0d: // fconst_2
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "F"; // push F onto stack
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x0e: // dconst_0
  case 0x0f: // dconst_1
    if ( checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 )
      return -1;  
    stackbase[(*stkSizePtr)++] = "d"; // push d onto stack
    stackbase[(*stkSizePtr)++] = "D"; // push D onto stack
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x15: // iload
    varnum = m->code[ipos+1];
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(localsbase[varnum], "I") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "I"; // push I onto stack
    // the next instruction is the current instruction position plus two
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;
    
  case 0x16: // lload
    varnum = m->code[ipos+1];
    if ( checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(localsbase[varnum], "Ll") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "l";
    stackbase[(*stkSizePtr)++] = "L";
    // the next instruction is the current instruction position plus two (plus 1 is the inline!)
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;

  case 0x17: // fload
    varnum = m->code[ipos+1];
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(localsbase[varnum], "F") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "F";
    // the next instruction is the current instruction position plus two
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;

  case 0x18: // dload
    varnum = m->code[ipos+1];
    if ( checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(localsbase[varnum], "Dd") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "d";
    stackbase[(*stkSizePtr)++] = "D";
    // the next instruction is the current instruction position plus two
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;

  case 0x19: // aload
    varnum = m->code[ipos+1];
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareReferenceTypes(localsbase[varnum], "A") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = localsbase[varnum];
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;
  
  case 0x1a: // iload_0
  case 0x1b: // iload_1
  case 0x1c: // iload_2
  case 0x1d: // iload_3
    varnum = op - 0x1a; // eg. iload_0 op = 0x1a, so varnum = 0
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(localsbase[varnum], "I") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "I";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x1e: // lload_0
  case 0x1f: // lload_1
  case 0x20: // lload_2
  case 0x21: // lload_3
    varnum = op - 0x1e;
    if ( checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(localsbase[varnum], "Ll") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "l";
    stackbase[(*stkSizePtr)++] = "L";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x22: // fload_0
  case 0x23: // fload_1
  case 0x24: // fload_2
  case 0x25: // fload_3
    varnum = op - 0x22;
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(localsbase[varnum], "F") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "F";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x26: // lload_0
  case 0x27: // lload_1
  case 0x28: // lload_2
  case 0x29: // lload_3
    varnum = op - 0x26;
    if ( checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(localsbase[varnum], "Dd") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = "d";
    stackbase[(*stkSizePtr)++] = "D";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x2a: // aload_0
  case 0x2b: // aload_1
  case 0x2c: // aload_2
  case 0x2d: // aload_3
    varnum = op - 0x2a;
    if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareReferenceTypes(localsbase[varnum], "A") == -1 )
      return -1;
    stackbase[(*stkSizePtr)++] = localsbase[varnum];
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x2e: // iaload
  case 0x33: // baload
  case 0x34: // caload
  case 0x35: // saload
    // im fairly certain the array type for all of these is A[I
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[I") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 )
	 return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the A, keep the I and we are golden
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x2f: // laload
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[Ll") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 )
	 return -1;
    stackbase[*stkSizePtr - 1] = "L";
    stackbase[*stkSizePtr - 2] = "l";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x30: // faload
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[F") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 )
	 return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[*stkSizePtr - 1] = "F";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x31: // daload
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[Dd") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 )
	 return -1;
    stackbase[*stkSizePtr - 1] = "D";
    stackbase[*stkSizePtr - 2] = "d";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x32: // aaload
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[A") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 )
	 return -1;
    // chop the A[ off the type, should really do a strdup here.. and everywhere actually
    stackbase[*stkSizePtr - 2] = &stackbase[*stkSizePtr - 1][2];
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x36: // istore
    varnum = m->code[ipos+1];
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // basically pop the I
    localsbase[varnum] = "I";
    // next instruction is at ipos + 2
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;

  case 0x37: // lstore
    varnum = m->code[ipos+1];
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // basically pop the L
    stackbase[--(*stkSizePtr)] = "-"; // and l
    localsbase[varnum] = "Ll";
    // next instruction is at ipos + 2
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;

 case 0x38: // fstore
    varnum = m->code[ipos+1];
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "F") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the F
    localsbase[varnum] = "F";
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;

  case 0x39: // dstore
    varnum = m->code[ipos+1];
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the D
    stackbase[--(*stkSizePtr)] = "-"; // pop the d
    localsbase[varnum] = "Dd";
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;

 case 0x3a: // astore
    varnum = m->code[ipos+1];
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A") == -1 )
      return -1;
    localsbase[varnum] = stackbase[*stkSizePtr - 1];
    stackbase[--(*stkSizePtr)] = "-"; // pop the A..
    updateInstruction(&itable[ipos], &itable[ipos+2], typeArrSize);
    break;

  case 0x3b: // istore_0
  case 0x3c: // istore_1
  case 0x3d: // istore_2
  case 0x3e: // istore_3
    varnum = op - 0x3b;
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 checkInLocalsRange(varnum, m->max_locals) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the I
    localsbase[varnum] = "I"; // just smash over whatever was here before
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x3f: // lstore_0
  case 0x40: // lstore_1
  case 0x41: // lstore_2
  case 0x42: // lstore_3
    varnum = op - 0x3f;
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the L
    stackbase[--(*stkSizePtr)] = "-"; // pop the l
    localsbase[varnum] = "Ll";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x43: // fstore_0
  case 0x44: // fstore_1
  case 0x45: // fstore_2
  case 0x46: // fstore_3
    varnum = op - 0x43;
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "F") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    localsbase[varnum] = "F";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x47: // dstore_0
  case 0x48: // dstore_1
  case 0x49: // dstore_2
  case 0x4a: // dstore_3
    varnum = op - 0x47;
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the D
    stackbase[--(*stkSizePtr)] = "-"; // pop the d
    localsbase[varnum] = "Dd";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x4b: // astore_0
  case 0x4c: // astore_1
  case 0x4d: // astore_2
  case 0x4e: // astore_3
    varnum = op - 0x4b;
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A") == -1 )
      return -1;
    // could be 1 line if we had a pop fn yo!
    localsbase[varnum] = stackbase[*stkSizePtr - 1];
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x4f: // iastore
  case 0x54: // bastore
  case 0x55: // castore
  case 0x56: // sastore
    if ( checkStackUnderflow(*stkSizePtr, 3) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[I") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 3], "I") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x50: // lastore
    if ( checkStackUnderflow(*stkSizePtr, 4) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[Ll") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 3], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 4], "l") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x51: // fastore
    if ( checkStackUnderflow(*stkSizePtr, 3) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[F") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 3], "F") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x52: // dastore
    if ( checkStackUnderflow(*stkSizePtr, 4) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[Dd") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 3], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 4], "d") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

    /*
  case 0x53: // aastore
    if ( checkStackUnderflow(*stkSizePtr, 3) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A[A") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 3], // should use lub function here
			       &stackbase[*stkSizePtr - 3][2]) == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;
    */

  case 0x57: // pop
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the val
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x58: // pop2
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the first val
    stackbase[--(*stkSizePtr)] = "-"; // and the second
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x59: // dup
    // check underflow to see if we have a val, then overflow to see if we can push
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 || 
	 checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 )
      return -1;
    varnum = (*stkSizePtr)++; // dont want to screw up the incrementing so just do it here
    stackbase[varnum] = stackbase[varnum - 1];
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x5a: // dup_x1
    // check underflow to see if we have a 2 vals, then overflow to see if we can push
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 || 
	 checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 )
      return -1;
    varnum = (*stkSizePtr)++; // dont want to screw up the incrementing so just do it here
    stackbase[varnum] = stackbase[varnum - 2];
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x5b: // dup_x2
    // check underflow to see if we have a 3 vals, then overflow to see if we can push
    if ( checkStackUnderflow(*stkSizePtr, 3) == -1 || 
	 checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 )
      return -1;
    varnum = (*stkSizePtr)++; // dont want to screw up the incrementing so just do it here
    stackbase[varnum] = stackbase[varnum - 3];
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x5c: // dup2
    // check underflow to see if we have a 2 vals, then overflow to see if we can push 2
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 || 
	 checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 )
      return -1;
    varnum = (*stkSizePtr)++; // dont want to screw up the incrementing so just do it here
    stackbase[varnum] = stackbase[varnum - 2];
    varnum = (*stkSizePtr)++; // and repeat for the second val
    stackbase[varnum] = stackbase[varnum - 2];
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x5d: // dup2_x1
    // check underflow to see if we have a 3 vals, then overflow to see if we can push 2
    if ( checkStackUnderflow(*stkSizePtr, 3) == -1 || 
	 checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 )
      return -1;
    varnum = (*stkSizePtr)++; // dont want to screw up the incrementing so just do it here
    stackbase[varnum] = stackbase[varnum - 3];
    varnum = (*stkSizePtr)++; // and repeat for the second val
    stackbase[varnum] = stackbase[varnum - 3];
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x5e: // dup2_x2
    // check underflow to see if we have a 4 vals, then overflow to see if we can push 2
    if ( checkStackUnderflow(*stkSizePtr, 4) == -1 || 
	 checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 )
      return -1;
    varnum = (*stkSizePtr)++; // dont want to screw up the incrementing so just do it here
    stackbase[varnum] = stackbase[varnum - 4];
    varnum = (*stkSizePtr)++; // and repeat for the second val
    stackbase[varnum] = stackbase[varnum - 4];
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x5f: // swap
    // check that we have 2 vals to swap
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 )
      return -1;
    varnum = (*stkSizePtr) - 1;
    tmpStr = stackbase[varnum];
    stackbase[varnum] = stackbase[varnum - 1];
    stackbase[varnum - 1] = tmpStr;
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x60: // iadd
  case 0x64: // isub
  case 0x68: // imul
  case 0x6c: // idiv
  case 0x70: // irem
  case 0x78: // ishl
  case 0x7a: // ishr
  case 0x7c: // iushr
  case 0x7e: // iand
  case 0x80: // ior
  case 0x82: // ixor
    // do we have 2 vals, that are both I's?
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // just pop an I (pop 2, push 1)
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;
    
  case 0x61: // ladd
  case 0x65: // lsub
  case 0x69: // lmul
  case 0x6d: // ldiv
  case 0x71: // lrem
  case 0x79: // lshl
  case 0x7b: // lshr
  case 0x7d: // lushr
  case 0x7f: // land
  case 0x81: // lor
  case 0x83: // lxor
    // do we have 4 vals
    if ( checkStackUnderflow(*stkSizePtr, 4) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 3], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 4], "l") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // just pop an L
    stackbase[--(*stkSizePtr)] = "-"; // and the l
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;   

  case 0x62: // fadd
  case 0x66: // fsub
  case 0x6a: // fmul
  case 0x6e: // fdiv
  case 0x72: // frem
    // do we have 2 vals
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "F") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "F") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x63: // dadd
  case 0x67: // dsub
  case 0x6b: // dmul
  case 0x6f: // ddiv
  case 0x73: // drem
    // do we have 4 vals
    if ( checkStackUnderflow(*stkSizePtr, 4) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 3], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 4], "d") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;   

  case 0x74: // ineg
  case 0x91: // i2b
  case 0x92: // i2c
  case 0x93: // i2s
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 )
      return -1;
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x75: // lneg
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 )
      return -1;
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x76: // fneg
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "F") == -1 )
      return -1;
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x77: // dneg
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 )
      return -1;
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x84: // iinc
    varnum = m->code[ipos+1];
    if ( checkInLocalsRange(varnum, m->max_locals) == -1 )
      return -1;
    // dont need to do anything with the second inline, and no types change. Done!
    updateInstruction(&itable[ipos], &itable[ipos+3], typeArrSize); // +3 as 2 inline
    break;

  case 0x85: // i2l
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 )
      return -1;
    stackbase[(*stkSizePtr) - 1] = "l"; // overwrite the old stack ele
    stackbase[(*stkSizePtr)++] = "L"; // and add a new one
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x86: // i2f
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareSimpleTypes(stackbase[(*stkSizePtr) - 1], "I") == -1 )
      return -1;
    stackbase[(*stkSizePtr) - 1] = "F"; // just overwrite the old ele
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x87: // i2d
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 )
      return -1;
    stackbase[(*stkSizePtr) - 1] = "d";
    stackbase[(*stkSizePtr)++] = "D";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x88: // l2i
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the L
    stackbase[(*stkSizePtr) - 1] = "I"; // change the l to an I

  case 0x89: // l2f
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[(*stkSizePtr) - 1] = "F"; // change the l to an F

  case 0x8a: // l2d
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 )
      return -1;
    stackbase[(*stkSizePtr) - 1] = "D"; // change L to D
    stackbase[(*stkSizePtr) - 2] = "d"; // and l to d

  case 0x8b: // f2i
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareSimpleTypes(stackbase[(*stkSizePtr) - 1], "F") == -1 )
      return -1;
    stackbase[(*stkSizePtr) - 1] = "I"; // just overwrite the old ele
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x8c: // f2l
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "F") == -1 )
      return -1;
    stackbase[(*stkSizePtr) - 1] = "l"; // overwrite the old stack ele
    stackbase[(*stkSizePtr)++] = "L"; // and add a new one
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x8d: // f2d
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 checkStackOverflow(*stkSizePtr, 2, m->max_stack) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "F") == -1 )
      return -1;
    stackbase[(*stkSizePtr) - 1] = "d"; // overwrite the old stack ele
    stackbase[(*stkSizePtr)++] = "D"; // and add a new one
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x8e: // d2i
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the D
    stackbase[(*stkSizePtr) - 1] = "I"; // change the d to an I
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x8f: // d2l
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 )
      return -1;
    stackbase[(*stkSizePtr) - 1] = "L"; // change D to L
    stackbase[(*stkSizePtr) - 2] = "l"; // and d to l
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x90: // d2f
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the L
    stackbase[(*stkSizePtr) - 1] = "F"; // change the l to an F
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x94: // lcmp
    if ( checkStackUnderflow(*stkSizePtr, 4) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 3], "L") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 4], "l") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop the L
    stackbase[--(*stkSizePtr)] = "-"; // l
    stackbase[--(*stkSizePtr)] = "-"; // L
    stackbase[(*stkSizePtr) - 1] = "I"; // convert the l to an I
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x95: // fcmpl
  case 0x96: // fcmpg
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "F") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "F") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-"; // pop 
    stackbase[(*stkSizePtr) - 1] = "I"; // convert the F to an I
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x97: // dcmpl
  case 0x98: // dcmpl
    if ( checkStackUnderflow(*stkSizePtr, 4) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 3], "D") == -1 || 
	 compareSimpleTypes(stackbase[*stkSizePtr - 4], "d") == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[(*stkSizePtr) - 1] = "I";
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x99: // ifeq
  case 0x9a: // ifne
  case 0x9b: // iflt
  case 0x9c: // ifge
  case 0x9d: // ifgt
  case 0x9e: // ifle
    varnum = (m->code[ipos+1] << 8) + m->code[ipos+2];
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 ||
	 checkJumpPosition(varnum, itable, m->code_length) == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    // uprade branch point and next instruction
    updateInstruction(&itable[ipos], &itable[varnum], typeArrSize);
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0x9f: // if_icmpeq  
  case 0xa0: // if_icmpne
  case 0xa1: // if_icmplt
  case 0xa2: // if_icmpge
  case 0xa3: // if_icmpgt
  case 0xa4: // if_icmple
    varnum = (m->code[ipos+1] << 8) + m->code[ipos+2];
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "I") == -1 ||
	 checkJumpPosition(varnum, itable, m->code_length) == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    // uprade branch point and next instruction
    updateInstruction(&itable[ipos], &itable[varnum], typeArrSize);
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0xa5: // if_acmpeq
  case 0xa6: // if_acmpne
    varnum = (m->code[ipos+1] << 8) + m->code[ipos+2];
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A") == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 2], "A") == -1 ||
	 checkJumpPosition(varnum, itable, m->code_length) == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[varnum], typeArrSize);
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;

  case 0xa7: // goto
    varnum = (m->code[ipos+1] << 8) & m->code[ipos+2];
    if ( checkJumpPosition(varnum, itable, m->code_length) == -1 )
      return -1;
    updateInstruction(&itable[ipos], &itable[varnum], typeArrSize);
    break;

// check the return type of the method
/*
  case 0xac: // ireturn
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "I") == -1 )
      // return doesn't go anywhere so we don't update any more instructions
    break;

  case 0xad: // lreturn
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "L") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "l") == -1 )
      // return doesn't go anywhere so we don't update any more instructions
    break;

  case 0xae: // freturn
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "F") == -1 )
      // return doesn't go anywhere so we don't update any more instructions
    break;

  case 0xaf: // dreturn
    if ( checkStackUnderflow(*stkSizePtr, 2) == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 1], "D") == -1 ||
	 compareSimpleTypes(stackbase[*stkSizePtr - 2], "d") == -1 )
      // return doesn't go anywhere so we don't update any more instructions
    break;

  case 0xb0: // areturn
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A") == -1 )
      // return doesn't go anywhere so we don't update any more instructions
    break;

  case 0xb1: // return
    break;
*/
  case 0xbb: // new
	varnum = (m->code[ipos+1] << 8) + m->code[ipos+2];
	if ( checkStackOverflow(*stkSizePtr, 1, m->max_stack) == -1 )
      return -1; // verification failed
    tmpStr = GetCPItemAsString(cf, varnum);
    stackbase[(*stkSizePtr)++] = SafeStrcat("A", tmpStr);
	SafeFree(tmpStr);
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
	break;

/* ^^ create new object of type identified by class reference in constant pool index (i1 << 8 + i2) */
    
  
  case 0xc6: // ifnull
  case 0xc7: // ifnonnull
    varnum = (m->code[ipos+1] << 8) + m->code[ipos+2];
    if ( checkStackUnderflow(*stkSizePtr, 1) == -1 ||
	 compareReferenceTypes(stackbase[*stkSizePtr - 1], "A") == -1 ||
	 checkJumpPosition(varnum, itable, m->code_length) == -1 )
      return -1;
    stackbase[--(*stkSizePtr)] = "-";
    updateInstruction(&itable[ipos], &itable[varnum], typeArrSize);
    updateInstruction(&itable[ipos], &itable[ipos+1], typeArrSize);
    break;


  default:
    fprintf(stdout, "Verification for opcode %d %s not implemented!\n", op, opcodes[op].opcodeName);
  }
  
  return 0;
}





// Verify the bytecode of one method m from class file cf
static void verifyMethod( ClassFile *cf, method_info *m ) {
    char *name = GetCPItemAsString(cf, m->name_index);
    char *retType;
    int numSlots = m->max_locals + m->max_stack;
    int ipos = 0;

    // initState is an array of strings, it has numSlots elements
    // retType describes the result type of this method
    char **initState = MapSigToInitState(cf, m, &retType);

    InstructionInfo *itable = createInstructionTable(m, initState);

    if (tracingExecution & TRACE_VERIFY)
      printConstantPool(cf);


    if (tracingExecution & TRACE_VERIFY)
      printAllInstructions(m, name);

    //if (tracingExecution & TRACE_VERIFY)
    // printTypeCodesArray(initState, m, name);

    do {
      if (tracingExecution & TRACE_VERIFY) {
	fprintf(stdout, "Pre Instruction ");
	printInstructionInfo(&itable[ipos], m->max_locals, m->max_stack);
      }

      if ( verifyOpcode(itable, cf, m, ipos) == -1 ) {
	fprintf(stdout, "Verification Failed at instruction %d : %s\n", ipos, opcodes[m->code[ipos]].opcodeName);
	// fail verification
      }
      if (tracingExecution & TRACE_VERIFY) {
	fprintf(stdout, "Post Instruction ");
	printInstructionInfo(&itable[ipos], m->max_locals, m->max_stack);
      }

    } while ( (ipos = findChangedInstruction(itable, ipos, m->code_length)) != -1 );

    FreeTypeDescriptorArray(initState, numSlots);
    SafeFree(name);
}


// Verify the bytecode of all methods in class file cf
void Verify( ClassFile *cf ) {
    int i;

    for( i = 0;  i < cf->methods_count;  i++ ) {
        method_info *m = &(cf->methods[i]);
	    verifyMethod(cf, m);
    }
    if (tracingExecution & TRACE_VERIFY)
    	fprintf(stdout, "Verification of class %s completed\n\n", cf->cname);
}


// Initialize this module
void InitVerifier(void) {
#ifndef NDEBUG
    // perform integrity check on the opcode table
    CheckOpcodeTable();
#endif
    // any initialization of local data structures can go here
}
