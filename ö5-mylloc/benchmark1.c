#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "rand.h"

#define BUFFER 100
#define ROUNDS 100 
#define LOOPS 100000

int main()
{
  void *buffer[BUFFER];
  for(int i = 0; i < BUFFER; i++) {
    buffer[i] = NULL;
  }
  void *init = sbrk(0);
  void *current;

  printf("Ursprungliga toppen av heapen: %p\n", init);

  for(int j = 0; j < ROUNDS; j++)
    {
      for(int i = 0; i < LOOPS; i++)
	{
	  int index = rand() % BUFFER;
	  if(buffer[index] != NULL) {
	      free(buffer[index]);
	      buffer[index] = NULL;
	    } else {
	      size_t size = (size_t)request();
	      int *memory;
	      memory = malloc(size);

	      if(memory == NULL) {
	      fprintf(stderr, "malloc failed\n");
	      return(1);
	      }
	      
	      buffer[index] = memory;
	      *memory = 123;  // Vad händer här????
	      free(memory);
	}
      current = sbrk(0);
      int allocated = (int)((current - init) / 1024);
      printf("%d\n", j);
      printf("Den nuvarande toppen av heapen: %p\n", current);
      printf("  increased by %d Kbyte\n", allocated);
    }
  return 0;
}
}
