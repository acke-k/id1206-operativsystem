#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>

#include <unistd.h>

/* Maybe change the size of these variables in the future to reduce overhead or
   increase performance */
struct head {
  uint16_t bfree; // Is the block before free?
  uint16_t bsize; // The size of the block before
  uint16_t free;  // Is this block free?
  uint16_t size;  // The size of this block
  struct head *prev;  // The previous block in the list
  struct head *next;  // The next block in the list
  struct arena* home; // The arena this block belongs to
};
// This is pointed to in struct head, it is stored in the first block of each arena
struct arena {
  struct head* arena;
  struct head* flist;
  struct arena* next;
};

#define TRUE 1
#define FALSE 0

#define HEAD (sizeof(struct head)) // For now, 24 bytes
#define ARENA_HEAD (sizeof(struct arena))
#define MIN(size) (((size) > (8))?(size):(8)) // Smallest size that a block can be, 8 bytes for now
#define LIMIT(size) (MIN(0) + HEAD + size)  // A block cannot be split if it is smaller than this
#define MAGIC(memory) ((struct head*)memory - 1)  // Retrive the head of a block
#define HIDE(block) (void*)((struct head*)block + 1) // Hide the head of a block

#define ALIGN 8 // Since we are using a 64 bit processor
#define ARENA (64*1024)  // The block we start out with, i.e. the size of the heap

int flist_length;
struct arena* arena_head; // This is the head of out list of arenas

int firsttime = 0; // wtf

/* BLOCK LOGIC */

// Takes in a pointer to a block and returns a pointer to the next/previous block
struct head *after(struct head *block) {
  return (struct head*)((char*)block + (block->size) + HEAD);
}
struct head *before(struct head *block) {
    return (struct head*)((char*)block - (block->bsize) - HEAD);
}
   
// returns the split block
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

// Create a new arena header and places it in the arena list
struct arena* new_arena() {
  struct arena* new_arena = mmap(NULL, ARENA_HEAD, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(new_arena == MAP_FAILED) {
    printf("mmap failed: error %d\n", errno);
    return NULL;
  }
  
  if(arena_head == NULL) {
    // If we don't have an arena yet
    arena_head = new_arena;
    arena_head->next = NULL;
  } else {
    // Put it at the first place in the arena list
    struct arena* oldfirst = arena_head;
    new_arena->next = oldfirst;
    new_arena = arena_head;
  }
  return new_arena; 
}

// This function "initializes" an arena struct
void* init_arena(struct arena* arena) {
  struct head* new = arena->arena;
  // Reserve memory for the arena
  new = mmap(NULL, ARENA, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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
  
  new->home = arena;
  arena->flist = new;
  arena->arena = new;
}


// Detach a block, i.e. remove it(?) Should it be returned somewhere?
void detach(struct head *block) {
  struct arena* home  = block->home;
  // If the block is not the last element in the list
  if(block->next != NULL) {
    (block->next)->prev= block->prev;
  }
  // If the block isn't the first element in the list
  if (block->prev != NULL) {
    (block->prev)->next = block->next;
  }
  // If the block is the first element in the list
  else {
    home->flist = (home->flist->next);
  }
  return;
}

// Insert a new block into the front of the list
void insert(struct head *block) {
  struct arena* home = block->home;
  flist_length++;
  block->next = home->flist;
  block->prev = NULL;
  if (home->flist != NULL) {  // If the list isn't empty
    home->flist->prev = block;
  }
  home->flist = block;
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

  // If this is the first call of find
  if(arena_head == NULL) {
    struct arena* arena = new_arena();
    init_arena(arena);
    // Now, there exists an arena at arena_head
  }

  // Now, go thorugh all the freelists and search for a block
  // Iterate through the arenas
  struct arena* arena_index = arena_head;
  while(arena_index != NULL) {
    // Iterate through the flists (Same logic as before
    struct head* flist_index = arena_index->flist;
    while(flist_index != NULL) {
      if(flist_index->size >= size) {
	struct head* new_block;
	detach(flist_index);
	if(flist_index->size > LIMIT(size)) {
	  new_block = split(flist_index, size);
	  insert(flist_index);
	} else {
	  new_block = flist_index;
	}

	new_block->free = FALSE;
	after(new_block)->bfree = FALSE;
	new_block->home = arena_index;

	return new_block;
      }
      flist_index = flist_index->next;
    }
      arena_index = arena_index->next;
  }
  // If we couldn't find a suitable block, create another arena
  //printf("NEW ARENA CREATED\n\n");
  sleep(1);
  struct arena* arena = new_arena();
  init_arena(arena);
  find(size); // Shouldn't be recursive since we create a new arena, but maybe dangerous
}
 

struct head* merge(struct head* block) {

  struct head* aft = after(block);
  // If the block before is free
  if(block->bfree) {
    
    struct head* bef = before(block);
    
    // Unlink the block before  
    detach(bef);

    // Calculate and set the total size of the soon to be merged blocks
    bef->size = HEAD + block->size + bef->size;
    
    // update the block after the merged blocks
    aft->bsize = bef->size;
    after(block)->bsize = bef->size;
    
    // Continue with the merged block
    block = bef;
  }
  // If the block after is free
  if(aft->free) {
    // Unlink the block
    detach(aft);
    // Calculate the total size of the merged blocks
    int size = HEAD + block->size + aft->size;
    // Update the block after the merged block
    after(block)->bfree = TRUE;
    after(block)->bsize = size;
    block->size = size;
  }
  return block;
}
// My own version of malloc
void *dalloc(size_t request) {
  /*
  if(firsttime == 0) {
    dalloc(20);
    printf("firsttime\n");
    firsttime = 1;
  }
  */
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
    // Find the start of the header
    struct head *block = MAGIC(memory);
    // Try to merge block with another one
    block = merge(block);
    
    struct head *aft = after(block);
    block->free = TRUE;
    aft->bfree = TRUE;
    aft->bsize = block->size;
    insert(block);
  }
  return;
}

void print_arenas() {
  if(arena_head == NULL) {
    printf("No arenas availible\n");
    return;
  }
  printf("ALL ARENAS: \n");
  struct arena* a_index = arena_head;
  int i = 0;
  while(a_index != NULL) {
    printf("index: %2d address: %p\n", i, a_index);
    i++;
    a_index = a_index->next;
  }
  printf("ARENAS DONE\n\n");
}
// Prints every block in every arena
void print_blocks() {
  if(arena_head == NULL) {
    printf("No arenas availible\n");
    return;
  }
  // Iterate arenas
  printf("ALL BLOCKS\n");
  struct arena* a_index = arena_head;
  int i = 0;
  while(a_index != NULL) {
    printf("arena: %p\n", a_index);
    struct head* h_index = a_index->arena;
    while(!(after(h_index)->free == FALSE && after(h_index)->size == 0)) {
      printf("%3d:  size: %7d  address: %14p  after: %14p  before: %14p  free: %d\n", i, h_index->size, h_index, after(h_index), before(h_index), h_index->free);
      h_index = after(h_index);
    }
    a_index = a_index->next;
  }
  printf("BLOCKS DONE\n\n");
}

void print_flists() {
    if(arena_head == NULL) {
    printf("No arenas availible\n");
    return;
  }
  // Iterate arenas
  printf("ALL FLISTS\n");
  struct arena* a_index = arena_head;
  int i = 0;
  while(a_index != NULL) {
    printf("arena: %p\n", a_index);
    struct head* f_index = a_index->flist;
    while(f_index != NULL) {
      printf("%3d:  size: %10d   address: %20p  free: %d bfree: %d bsize: %d\n", i, f_index->size, f_index, f_index->free, f_index->bfree, f_index->bsize);
      f_index = f_index->next;
    }
    a_index = a_index->next;
  }
  printf("FLISTS DONE\n\n");
}
  
// Prints flist & all blocks
void sanity_print() {

  /* print all arenas */
  print_arenas();
  /* print all blocks */
  print_blocks();
  /* print all flists */
  print_flists();
}
void sanity(int arg) {
  if(arg == 1) {
    sanity_print();
    return;
  }
  printf("Sanitycheck\n");
  struct arena* a_index = arena_head;
  struct head* prev;
  int i = 0;
  while(a_index != NULL) {
    printf("arena: %p\n", a_index);
    struct head* h_index = a_index->arena;
    prev = before(h_index);
    while(!(after(h_index)->free == FALSE && after(h_index)->size == 0)) {
      if(before(h_index) != prev) {
	printf("INSANITY\n\n");
	printf("HERE: %p\n\n", h_index);
	sleep(10);
      }
      prev = h_index;
      h_index = after(h_index);
    }
    a_index = a_index->next;
  }
  /* Go through all blocks & check before and after */
}

int flist_size() {
  int len = 0;
  struct arena* a_index = arena_head;
  while(a_index != NULL) {
    //printf("arena: %p\n", a_index);
    struct head* f_index = a_index->flist;
    while(f_index != NULL) {
      len++;
      f_index = f_index->next;
    }
    a_index = a_index->next;
  }
  return len;
}
// Average blocksize. Iterate all blocks so that we easily can ignore the large block at the first position
float flist_blocksize() {
  
  float avrg = 0.0;
  struct head* index = arena_head->arena;

  // Iterate arenas
  //printf("ALL FLISTS\n");
  struct arena* a_index = arena_head;
  int i = 0;
  while(a_index != NULL) {
    struct head* f_index = a_index->flist;
    while(f_index != NULL) {
      avrg += f_index->size;
      f_index = f_index->next;
    }
    a_index = a_index->next;
  }
  
  return avrg / flist_size();
}
/*
int main() {
  
  struct arena* new = new_arena();
  printf("new arena: %p\n", &new);
  printf("new->flist %p\n", new->flist);
  init_arena(new);
  printf("new->flist %p\n", new->flist);
  
  // Nu finns alltså flist och arena för new
  // Nästa steg, gör så att vi kan kalla find
  //struct head* test = find(20);

  // Nu, försök lösa dalloc & dfree
  int* a = dalloc(50000);
  sanity_print();
  dfree(a);
  
  //int* b = dalloc(300);
  //int* c = dalloc(1);
  sanity_print();
  sanity();
  return 0;
}
*/
  
   
    
    


