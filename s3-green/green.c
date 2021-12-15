#include <stdlib.h>
#include <ucontext.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

#define PERIOD 100
// Globala variabler för timer interrupt
static sigset_t block;
void timer_handler(int);

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, NULL, FALSE};

static green_t* running = &main_green;

/*
ready-kön är en enkellänkadlista med ready_head som head-element.
Den är länkad m.h.a. "*next" pekaren i green_t structen.
Det finns två hjälpfunktioner enqueue & dequeue
*/
green_t* ready_head = NULL;

// Lägg till ett element i ready-kön
void enqueue_ready(green_t* element) {
    if(ready_head == NULL) {
    ready_head = element;
    return;
  } else {
    green_t* index = ready_head;
    while(index->next != NULL) {
      index = index->next;
    }
    index->next = element;
    return;
  }
}
// Ta ut ett element ur ready-kön
green_t* dequeue_ready() {
  if(ready_head == NULL) {
    printf("Attempt to dequeue when queue empty\n");
    return NULL;
  } else {
    green_t* retval = ready_head;
    ready_head = ready_head->next;
    retval->next = NULL;
    return retval;
  }
}
/*
Denna funktion kör funktionen i den tråd som running pekar på och sparar dess argument.
Om denna tråd kommer "joina" en annan tråd läggs den andra tråden på ready-kön.
*/

void green_thread() {
  green_t* this = running;  // Hittar den kontext som är "running"
  void* result = (*this->fun)(this->arg);   // Kör funktionen och spara resultatet
  // Place waiting (joining) thread in ready queue
  if(this->join != NULL) {
    enqueue_ready(this->join);
  }
  
  this->retval = result;  // Spara resultatet av this->fun i this->retval
  this->zombie = TRUE;  // Sätt den i zombie state

  green_t* next = dequeue_ready(); // Kör nästa tråd i kön
  running = next;
  //sigprocmask(SIG_UNBLOCK, &block, NULL);
}

/*
Tar in en green_t struct, en funktion den ska utföra och args för den funktionen.
Kopplar ihop green_t med dessa, fäster även en ny context till den och slutligen lägger den i "ready queue".
I princip hela denna funktion är given i instruktionerna
*/
int green_create(green_t* new, void* (*fun)(void*), void* arg) {
  //sigprocmask(SIG_BLOCK, &block, NULL);
  ucontext_t* cntx = (ucontext_t*)malloc(sizeof(ucontext_t));
  getcontext(cntx);

  void* stack = malloc(STACK_SIZE);
  
  cntx->uc_stack.ss_sp = stack;
  cntx->uc_stack.ss_size = STACK_SIZE;
  
  makecontext(cntx, green_thread, 0);
  
  new->context = cntx;
  new->fun = fun;
  new->arg = arg;
  new->next = NULL;
  new->join = NULL;
  new->retval = NULL;
  new->zombie = FALSE;

  enqueue_ready(new);   // add new to the ready queue..

  //sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}
/*
Slutar exekvera den nuvarande tråden, lägger den på ready-kön och börjar exekvera nästa i kön
*/

int green_yield() {
  //sigprocmask(SIG_BLOCK, &block, NULL);
  
  green_t* susp = running;
  enqueue_ready(susp);  // Lägg till susp i ready kön
  green_t* next = dequeue_ready();   // Välj nästa tråd

  running = next;  // Kör nästa tråd
  swapcontext(susp->context, next->context); 
 
  //sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}
/*
Sluta köra den tråd som är running och kör den i argumentet (thread).
Den som är running blir "join" till thread, d.v.s. när thread har exekverat sin funktion kommer 
den nuvarande läggas på ready-kön (se green_thread).
När thread har exekverat och joinat kommer den börja exekvera där den avslutade, i collect result delen nedan.
*/
int green_join(green_t* thread, void** res) {
  //sigprocmask(SIG_BLOCK, &block, NULL);

  if(!thread->zombie) {
    green_t* susp = running;
    thread->join = susp; // Lägg till som joining thread i "thread"
    
    green_t* next = dequeue_ready(); // Kör nästa tråd
    running = next;
    
    swapcontext(susp->context, next->context);
    //sigprocmask(SIG_UNBLOCK, &block, NULL);  
  }
  // Collect result
  res = thread->retval;  // Spara resultatet av join tråden i res-pekaren
  
  free(thread->context->uc_stack.ss_sp); // Deallokera allt minne thread har använt, den kommmer inte köras mer
  free(thread->context);

  //   sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

/* Conditional variables */
/*
Funktionerna nedan implementerar conditional variables.
I grund och botten är det en länkad lista som vi kan köa trådar i med wait, och ta ut dem ur kön med signal
*/

// Initierar den länkade listan
void green_cond_init(green_cond_t* new) {
  //sigprocmask(SIG_BLOCK, &block, NULL);
  new->head = NULL;
  //sigprocmask(SIG_UNBLOCK, &block, NULL);
}
// Lägg till running tråden i en lista
void green_cond_wait(green_cond_t* cond) {
  // sigprocmask(SIG_BLOCK, &block, NULL);
  green_t* susp = running;    // Hitta running och lägg till den i listan
  
  green_t* index = cond->head; // Lägg til susp i "cond" listan
  if(index == NULL) {
    cond->head = susp;
    susp->next = NULL;
  } else {
    while(index->next != NULL) {
      index = index->next;
    }
    index->next = susp;
    susp->next = NULL;
  }
  
  running = dequeue_ready();   // Kör nästa tråd
  swapcontext(susp->context, running->context);

  //sigprocmask(SIG_UNBLOCK, &block, NULL);
}
// Tar ut en tråd ur listan och lägger den i ready-kön
void green_cond_signal(green_cond_t* cond) {
  // sigprocmask(SIG_BLOCK, &block, NULL);

  green_t* index = cond->head;
  if(index == NULL) {
    // Borde skriva write() här om det ska vara kvar
    // printf("Attempted to signal an empty queue\n");
    return;
  }

  green_t* retval = cond->head;   // Ta ut element ur länkad lista
  cond->head = cond->head->next;
  retval->next = NULL;
  enqueue_ready(retval);

  //sigprocmask(SIG_UNBLOCK, &block, NULL);
}

/* Timer handler */
/*
Denna funktion bestämmer vad som händer då timern varvar
*/
void timer_handler(int sig) {
  //printf("time\n");
  green_t* susp = running;

  // Add the running to the ready queue
  enqueue_ready(susp);

  // find the next thread for execution
  green_t* next = dequeue_ready();
  running = next;

  //sigprocmask(SIG_BLOCK, &block, NULL);
  swapcontext(susp->context, next->context);
  //sigprocmask(SIG_UNBLOCK, &block, NULL);
}

/* Sanity */

void sanity() {
  printf("Running process: %p\n", running);
  printf("ready q\n");
  green_t* index = ready_head;
  while(index != NULL) {
    printf("i: %p\n", index);
    index = index->next;
  }
  printf("done\n");
}

/* Denna funktion initierar vår första context vid startup (automatiskt) */
static void init() __attribute__((constructor));

void init() {
  getcontext(&main_cntx);
  
  sigemptyset(&block);
  sigaddset(&block, SIGVTALRM);

  struct sigaction act = {0};
  struct timeval interval;
  struct itimerval period;

  act.sa_handler = timer_handler;
  assert(sigaction(SIGVTALRM, &act, NULL) == 0);

  interval.tv_sec = 0;
  interval.tv_usec = PERIOD;
  period.it_interval = interval;
  period.it_value = interval;
  setitimer(ITIMER_VIRTUAL, &period, NULL);
}
