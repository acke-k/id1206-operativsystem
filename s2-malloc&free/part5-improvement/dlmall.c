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
#define NO_LISTS 17

int flist_length;

/* BLOCK LOGIC */

// Takes in a pointer to a block and returns a pointer to the next/previous block
struct head *after(struct head *block) {
  return (struct head*)((char*)block + (block->size) + HEAD);
}
struct head *before(struct head *block) {
    return (struct head*)((char*)block - (block->bsize) - HEAD);
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
    flist->prev = block;
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
/*
// Checks if the given block is in the general freelist
boolean is_genfree(struct head* block) {
  struct head* index = flists[NO_BLOCK - 1];
  while(index != block && index->free != FALSE) {
    index = index->next;
  }
  if(index == block) {
    return true;
  } else {
    return false;
  }
}
*/
struct head* merge(struct head* block) {
    // If the block after is free
  if(after(block) != NULL && after(block)->free && after(block)->size > 128 ) {
    struct head* aft = after(block);
    
    // Calculate the total size of the merged blocks
    int size = HEAD + block->size + aft->size;
    // Update the block after the merged block
    after(block)->bfree = TRUE;
    after(block)->bsize = size;
    block->size = size;
  }

  
  // If the block before is free
  if(after(block) != NULL && block->bfree && before(block)->size > 128) {
    struct head* bef = before(block);
    // Unlink the block before  

    // Calculate and set the total size of the soon to be merged blocks
    bef->size = HEAD + block->size + bef->size;
    
    // update the block after the merged blocks
    after(block)->bsize = bef->size;
    after(block)->bfree = block->free;
    
    // Continue with the merged block
    block = bef;
  }
  
  return block;
}

/* new api that implements a list of different sized blocks */

/* Helper functions which add block to the different indices */

// An array of lists
struct head* flists[17];

// Creates blocks for the different arrays in flists
// Each index will have 4KB for itself
// Each index contains a linked list of blocks
void init_lists() {
  flist = new();
  // Do this for each index
  for(int i = 0; i < NO_LISTS - 1;  i++) {
    //int no_blocks = (4096 - HEAD) / (HEAD + 8  + (i * 8)); //  How many blocks can fit in 4KB?
    flists[i] = NULL;
    // Allocate free blocks in each index
    for(int j = 0; j < 40; j++) {
      struct head* new_block = split(arena, (i + 1) * 8);

      after(new_block)->bfree = TRUE;
      after(new_block)->bsize = new_block->size;
      new_block->next = flists[i];
      
      if(flists[i] != NULL) {
	flists[i]->prev = new_block;
      }
      flists[i] = new_block;
    }
  }
  // The general index, i.e. where blocks > 128 byte are taken from
  flists[NO_LISTS - 1] = arena;
}

struct head* find_in_lists(int size) {
  // check if memory is allocated
  if(arena == NULL) {
    init_lists();
  }
  struct head* new_block;
  if(size > 128) {
  found128:
    // Take memory from the general index
    size = adjust(size);
    new_block = split(flists[16], size);
    new_block->free = FALSE;
    new_block->size = size;
    if(after(new_block) != NULL) {
      after(new_block)->bfree = FALSE;
    }
  } else {
    // take from the appropriate block
    int index = (adjust(size) / 8) - 1; // Take from this index in flists
    // check if index is empty & if so, try to find a larger one
    if(flists[index] == NULL) {
      while(index <= 16) {
	index++;
	if(flists[index] != NULL) {
	  if(index == 16) {
	    size = 129; // So that this memory will be returned to the proper index later
	    goto found128;
	  }
	  goto found;
	}
      }
      printf("Out of memory in a block!\n");
      return NULL;
    }
  found:
    // Pointer stuff
    new_block = flists[index];
    new_block->free = FALSE;
    after(new_block)->bfree = FALSE;
    
    if(flists[index]->next != NULL) {
      struct head* new_first = flists[index]->next;
      new_first->prev = NULL;
      flists[index] = new_first;
    } else {
      flists[index] = NULL;
    }
    return new_block;
  }
}

void return2lists(struct head* block) {

  block = before(block);
  //sanity(1);
  if(block->size > 128) {   
    // Try to merge with other blocks in flists[16]
    // If it is not possible, add it to the list
    // Eventually they will hopefully merge
    block->free = TRUE;
    before(block)->bfree = TRUE;
    block = merge(block);
    
    
  } else {
    int index = (block->size / 8) - 1;
    // Link it into the list
    struct head* oldfirst = flists[index];
    
    block->next = oldfirst;
    oldfirst->prev = block;

    block->prev = NULL;
    flists[index] = block;
    
    // Manage the header of this & before blocks
    after(block)->bfree = TRUE;
    block->free = TRUE;
  }
  return;
}

// My own version of malloc
void *dalloc(size_t request) {
  if(request <= 0) {
    return NULL;
  }
  struct head *taken = find_in_lists(request);
  if (taken == NULL) {
    return NULL;
  } else {
    return HIDE(taken);
  }
}
// My own version of free
void dfree(void *memory) {
  if(memory != NULL) {
    // Find the start of the header
    struct head *block = MAGIC(memory);
    return2lists(block);
  }
  return;
}
// Prints flist & all blocks
void sanity_print() {
  /* print all blocks */
  struct head* index = flist;
  int i = 0;
  printf("ALL BLOCKS\n");
  if(index != NULL) {
    while(!(index->free == FALSE && index->size == 0)) {
      printf("%3d:  size: %10d   address: %20p  free: %d\tbfree: %d\n", i, index->size, index, index->free, index->bfree);
      
      index = after(index);
      i++;
  }
  }
  
  /* print list in each index */
  
  for(int i = 0; i < NO_LISTS - 1; i++) {
    if(i == NO_LISTS - 1) {
      printf("General index\n");
    } else {
      printf("index: %d  size: %d\n", i, (i + 1) * 8);
    }
    if(flists[i] != NULL) {
      struct head* index = flists[i];

      while(index != NULL) {
	printf("\texpected size: %2d\taddress: %p\tfree: %d\tactual size: %d\n", index->size, index, index->free, index->size);
	index = index->next;
      }
    }
  }
}
void sanity(int arg) {
  printf("Sanitycheck\n");

  if(arg == 1) {
    sanity_print();
  }
}

int flist_size() {
  return 1;
}
// Average blocksize. Iterate all blocks so that we easily can ignore the large block at the first position
int flist_blocksize() {
  
  return 1;
}
/*
int main() {
  init_lists();
  printf("init success\n");
  sanity(1);
  int *a = dalloc(20);
  sanity(1);
  dfree(a);
  sanity(1);

  int *b = dalloc(129);
  sanity(1);
  dfree(b);
  sanity(1);
  
  return 0;
}
  
*/
  


    


