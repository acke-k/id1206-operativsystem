#include <stdio.h>
#include <stdlib.h>
#include "dlmall.h"

/*
  arg[1] - number of blocks
  arg[2] - Max size
  arg[3] - Min size
*/

/* How it works:
   Allocate memory arg[1] times using dalloc.
   Then, there is a 50% chance that each allocation is freed
*/
int main(int argc, char* argv[]) {
  
  int no_blocks = atoi(argv[1]);
  int max_size = atoi(argv[2]);
  int min_size = atoi(argv[3]);
  // Blocks cannot have a size of 0
  if(min_size == 0) { 
    min_size = 1;
  }

  int no_operations = 0;
  int* dallocated[no_blocks];

  // allocate memory on all blocks
  for(int i = 0; i < no_blocks; i++) {
    dallocated[i]  = dalloc(min_size + (rand() % (max_size-min_size)));
    no_operations++;
    benchmark();
    printf("\t%d\n", no_operations);
    // Chance to deallocate
    if(rand() % 2 == 0) {
      no_operations++;
      dfree(dallocated[i]);
      benchmark();
      printf("\t%d\n", no_operations);
    }
  }
  sanity(1);
}

