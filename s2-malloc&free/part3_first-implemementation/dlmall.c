#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>

/* Maybe change the size of these variables in the future to reduce overhead or
   increase performance */
struct head {
  uint16_t bfree; // Is the block before free?
  uint16_t bsize; // The size of the block before
  uint16_t free;  // Is this block free?
  uint16_t size;  // The size of this block
  struct head *prev;  // The previous block in the list
  struct head *next;  // The next block in the list
};

#define TRUE 1
#define FALSE 0

#define HEAD (sizeof(struct head)) // For now, 24 bytes
#define MIN(size) (((size) > (8))?(size):(8)) // Smallest size that a block can be, 8 bytes for now
#define LIMIT(size) (MIN(0) + HEAD + size)  // A block cannot be split if it is smaller than this
#define MAGIC(memory) ((struct head*)memory - 1)  // Retrive the head of a block
#define HIDE(block) (void*)((struct head*)block + 1) // Hide the head of a block

#define ALIGN 8 // Since we are using a 64 bit processor
#define ARENA (64*1024)  // The block we start out with, i.e. the size of the heap

int flist_length;

/* BLOCK LOGIC */

// Takes in a pointer to a block and returns a pointer to the next/previous block
struct head *after(struct head *block) {
  return (struct head*)((char*)block + (block->size) + HEAD);
}
struct head *before(struct head *block) {
    return (struct head*)((char*)block - (block->size) - HEAD);
}
   

struct head *split(struct head *block, int size) {
  int rsize = block->size - HEAD - size;  // calculate how much space will remain in the given block
  block->size = rsize;

  struct head *splt = HIDE(block) + rsize; // Find where the new block will begin
  splt->bsize = rsize;
  splt->bfree = block->free;
  splt->size = size;
  splt->free = TRUE;  // Should this be initialized as true or false after splitting?

  struct head *aft = after(splt);
  aft->bsize = size;

  return splt;

  // NOTE: why aren't we typing block->next = splt ?
  // We'll do it later, just return the split block for now
}

// Creating the initial memory which we can use
  
struct head *arena = NULL;

struct head *new() {
  if(arena != NULL) {
    printf("one arena already allocated\n");
    return NULL;
  }
  struct head *new = mmap(NULL, ARENA, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(new == MAP_FAILED) {
    printf("mmap failed: error %d\n", errno);
    return NULL;
  }

  uint size = ARENA - (2*HEAD); 
  // Make it so that we can not merge out of the arena through the beginning
  new->bfree = FALSE;
  new->bsize = 0;
  new->free = TRUE;
  new->size = size;
  
  struct head *sentinel = after(new);

  // Make it so that we cannot merge out of the arena through the end
  sentinel->bfree = TRUE;
  sentinel->bsize = new->size;
  sentinel->free = FALSE;
  sentinel->size = 0;

  flist_length = 1;

  arena = (struct head*)new;
  return new;
}


// Freelist is a double linked list using flist as a head element.
struct head* flist;

// Detach a block, i.e. remove it(?) Should it be returned somewhere?
void detach(struct head *block) {
  // If the block is not the last element in the list
  if (block->next != NULL) {
    (block->next)->prev= block->prev;
  }
  // If the block isn't the first element in the list
  if (block->prev != NULL) {
    (block->prev)->next = block->next;
  }
  // If the block is the first element in the list
  else {
    flist = (flist->next);
  }
  flist_length--;
  return;
}

// Insert a new block into the front of the list
void insert(struct head *block) {
  flist_length++;
  block->next = flist;
  block->prev = NULL;
  if (flist != NULL) {  // If the list isn't empty
    (block->next)->prev = block;
  }
  flist = block;
}

/* Malloc & free implementation  */

// Take an integer and return the closest multiple of ALIGN, rounded upwards
int adjust(size_t request) {
  int minsize = MIN(request);
  if(minsize % ALIGN == 0) {
    return minsize; 
  } else {
    int adj = minsize % ALIGN;
    return minsize + ALIGN - adj;
  }
}
// Find an appropiate block in freelist using the take first algorithm
struct head* find(size_t size) {
  // If we do not have an arena, create one
  if(flist == NULL) {
    flist = new();
  }
  struct head* index = flist; 
  // Take first algorithm
  while(index != NULL) { // Iterate through flist
    // Can we split the current block? If yes, choose this block
    if(index->size >= size) {
      struct head* new_block;
      // Detach the block, split it, and insert the leftover
      detach(index);
      if(index->size > LIMIT(size)) {
	new_block = split(index, size);
	insert(index);
      } else {
	new_block = index; 
      }
      // Make sure that the header of the new block is correct
      new_block->free = FALSE;
      after(new_block)->bfree = FALSE;
      return new_block;
    }
    index = index->next;
  }
  // If we cannot find a large enough block
  return NULL;
}
// My own version of malloc
void *dalloc(size_t request) {
  if(request <= 0) {
    return NULL;
  }
  int size = adjust(request);
  struct head *taken = find(size);
  if (taken == NULL) {
    return NULL;
  } else {
    return HIDE(taken);
  }
}
// My own version of free
void dfree(void *memory) {
  if(memory != NULL) {
    struct head *block = MAGIC(memory);
    struct head *aft = after(block);
    block->free = TRUE;
    aft->bfree = TRUE;
    insert(block);
  }
  return;
}
// Prints flist & all blocks
void sanity_print() {
  /* print all blocks */
  struct head* index = arena;
  int i = 0;
  printf("ALL BLOCKS\n");
  while(!(index->free == FALSE && index->size == 0)) {
    printf("%3d:  size: %10d   address: %20p  free: %d\n", i, index->size, index, index->free);
    index = after(index);
    i++;
  }

  /* print freelist */
  index = flist;
  i = 0;
  printf("FREELIST\n");
  while(index != NULL) {
    printf("%3d:  size: %10d   address: %20p\n", i, index->size, index);
      index = index->next;
      i++;
  }
}
void sanity(int arg) {
  printf("Sanitycheck\n");
  
  /* Iterate through the freelist */
  struct head* index = flist;
  struct head* prev = flist->prev;
  while(index != NULL) {
    if(prev != index->prev) {
      printf("Error: Prev isnt prev!\n");
    }
    if(index->free == FALSE) {
      printf("Error: Isnt marked as free!\n");
    }
    if(index->size % 8 != 0) {
      printf("Error: Size isnt alligned!!\n");
    }
    prev = index;
    index = index->next;
  }
  /* Iterate blocks */
  index = arena;
  while(!(after(index)->free == FALSE && after(index)->size == 0)) {
    index = after(index);
  }
  if(arg == 1) {
    sanity_print();
  }
}

int flist_size() {
  return flist_length;
}
// Average blocksize. Iterate all blocks so that we easily can ignore the large block at the first position
float flist_blocksize() {
  
  float avrg = 0.0;
  struct head* index = arena;
  
  while(index != NULL) {
    if(index->free) {
      avrg += index->size;
    }
    index = index->next;
  }
  return avrg / flist_size();
}
  
  

  
   
    
    


