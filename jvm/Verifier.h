/* Verify.h */

#ifndef VERIFYH
#define VERIFYH

#include "ClassFileFormat.h"  // for ClassFile

typedef struct {
  short cbit;
  char **state;
  int stksize;
} InstructionInfo;



extern void Verify( ClassFile *cf );
extern void InitVerifier(void);

#endif
