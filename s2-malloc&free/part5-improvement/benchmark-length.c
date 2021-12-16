#include <stdio.h>
#include <stdlib.h>
#include "dlmall.h"

/*
  This benchmark measures the length of the freelist as a function of the number of operations
  (dalloc & dfree) performed.
*/

/* Usage:
   argv[1] - min size of a block
   argv[2] - max size of a block
   argv[3] - dalloc limit, stop when this is reached
   argv[4] - max amount of blocks at a given time
/*

/* Description:
   There is a loop that runs until we have allocated memory argv[3] times. 
   Each iteration a block is either allocated or freed.
*/

int main(int argc, char* argv[]) {

  int minsize = atoi(argv[1]);
  int maxsize = atoi(argv[2]);
  int dalloclimit = atoi(argv[3]);
  int maxblocks = atoi(argv[4]);

  if(minsize < 1) {
    printf("error: Minsize too small!\n");
    return -1;
  }
  if(minsize > maxsize) {
    printf("error: minsize > maxsize!\n");
    return -1;
  }

  int* dallocated[maxblocks];
  int no_dallocations = 0;
  int allocated_blocks = 0;
  
  while(no_dallocations != dalloclimit) {
    // 30% chance to free, 70% chance to dallocate
    int chance = rand() % 10;

    if(chance < 3) {
    dfree:
      // No blocks were allocated
      if(allocated_blocks == 0) {
	goto dalloc;
      }
      // choose a random block between 0 and allocated_blocks - 1
      dfree(dallocated[allocated_blocks - 1]);
      allocated_blocks--;
    } else {
    dalloc:
      if(allocated_blocks == maxblocks) {
	goto dfree;
      }
      
      int size = (rand() % (maxsize - minsize)) + minsize;
      
      dallocated[allocated_blocks] = dalloc(size);
      
      no_dallocations++;
      allocated_blocks++;
    }
    printf("allocated blocks: %d\tno ops: %d\n", allocated_blocks, no_dallocations);
  }

  // int* a = dalloc(20);
  // sanity(1);
  return 0;
}
