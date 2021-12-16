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

// Allockera på första lediga plats
// freea på random plats som har isdallocated = 1

#define MAXBLOCKS 1000
#define ROUNDS 5000
#define MAXSIZE 160
#define MINSIZE 8


// choose a random number in range of the array
int rand_index() {
  return rand() % MAXBLOCKS;
}

int main() {
  int operations = 0;
  int allocated_blocks = 0;
  int* allocated[MAXBLOCKS];
  
  for(int i = 0; i < MAXBLOCKS; i++) {
    allocated[i] = NULL;
  }
  while(operations < ROUNDS) {
    int choice = rand() % 10;

    // dalloc
    if(choice < 7) {
      
      if(allocated_blocks == MAXBLOCKS - 1) {
	continue;
      }
      /* Iterate through the list and allocate to the first empty index */
      int index = 0;
      while(allocated[index] != NULL) {
	index++;
      }
      int size = (rand() % MAXSIZE - MINSIZE) + MINSIZE;
      allocated[index] = dalloc(size);
      allocated_blocks++;
    } else {
      
      if(allocated_blocks == 0) {
	continue;
      }
      // Find a index which isn't empty
      int index = rand_index();
      while(allocated[index] == NULL) {
	index = rand_index();
      }
      dfree(allocated[index]);
      allocated[index] = NULL;
      allocated_blocks--;
    }
    /*
    for(int i = 0; i < MAXBLOCKS; i++) {
      printf("i: %d\tcontains: %p\n", i, allocated[i]);
    }
    printf("\n\n\n");
    sanity(1);
    */
    printf("%3d\t%f\t%d\n", (operations + 1), flist_blocksize(), flist_size());
    operations++;
  }
}
 
