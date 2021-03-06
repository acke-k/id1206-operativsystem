#include <ucontext.h>

typedef struct green_t {
  ucontext_t *context;
  void* (*fun)(void*);
  void* arg;
  struct green_t* next;
  struct green_t* join;
  void* retval;
  int zombie;
} green_t;

// Kanske göra dummy här
typedef struct green_cond_t {
  green_t* head;
} green_cond_t;

int green_create(green_t* thread, void* (*fun)(void*), void* arg);
int green_yield();
int green_join(green_t* thread, void** val);

void green_cond_init(green_cond_t* new);
void green_cond_wait(green_cond_t* thread);
void green_cond_signal(green_cond_t* thread);

void sanity();
  
