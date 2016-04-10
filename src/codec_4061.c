#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codec.h"
#include "alloc.h"

#define ENCODE_FLAG ((char) 0x65)
#define DECODE_FLAG ((char) 0x64)

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Illegal input\n");
    return -1;
  }


  string inputDir = MALLOC(sizeof(string) + 1, char);
  string outputDir = MALLOC(sizeof(string) + 1, char);
  char flagOpt;

  inputDir = argv[2];
  outputDir = argv[3];
  /*extract arguments from argumnet array*/
  while((++argv)[0] && argv[0][0] == '-') {
    flagOpt = *++argv[0];
  }

  makeDir(NULL, outputDir);

  switch (flagOpt) {
  case ENCODE_FLAG: {
    /* call encodeDir function */
    encodeDir(inputDir, outputDir);
    break;
  }
  case DECODE_FLAG: {
    /* call decodeDir function */
    decodeDir(inputDir, outputDir);
    break;
  }
  }
  /*after finish to write temp file call sort system call
   *to sort file and then we call rm system call for deleting
   the temperatory report file
   */
  sort( inputDir, outputDir );
  delete( inputDir, outputDir );
  return 0;
}
