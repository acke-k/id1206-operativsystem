#include <stdio.h>
#include <unistd.h>

struct chunk {
  int size;
  struct chunk *next;
};

struct chunk* flist = NULL;

void *malloc(size_t size) {
  // Om vi allokerar 0 bytes
  if(size == 0) {
    return NULL;
  }
  // "länka in" i freelist
  struct chunk *next = flist;
  struct chunk *prev = NULL;

  while(next != NULL) {      // Iterara genom hela listan
    if(next->size >= size) { // Om nästa chunk är tillräckligt stort
      if(prev != NULL) {     // Speciallfall: Om freelist är tom
	prev->next = next->next; 
    } else {
      flist = next->next;
    }
    return (void*)(next + 1); // Spara ett "hemligt" byte
    } else {   // Om nästa chunk ej är tillräckligt stort, fortsätt iterera
    prev = next;
    next = next->next;
    }
  }
  void *memory = sbrk(size + sizeof(struct chunk*));
  if(memory == (void *)-1) {
    return NULL;
  } else {
    struct chunk *cnk = (struct chunk*)memory;
    cnk->size = size;
    return (void*)(cnk + 1);
  }		      
}

// Lägg till memory i den länkade listan, observera "-1"
void free(void *memory) {
  if(memory != NULL) {
    struct chunk *cnk = (struct chunk*)((struct chunk*)memory - 1);
    cnk->next = flist;
    flist = cnk;
  }
  return;
}
