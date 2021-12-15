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

static sigset_t block;

void timer_handler(int);

/* Dessa variabler definierar den context som körs */

// Skapa ny context (vad är det exakt?)
static ucontext_t main_cntx = {0};

// Initiera en ny green_t struktur, som pekar på den tidigare contexten och resten nullpekare
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, NULL, FALSE};

// Sätt running till den nya contexten
static green_t* running = &main_green;

/* green_create */
/*
Tar in en green_t struct, en funktion den ska utföra och args för den funktionen. Kopplar ihop green_t med dessa, fäster även en ny context till den och slutligen lägger den i "ready queue".
*/

/* Kön */
/*
ready kön är en enkellänkadlista med header och next* i green_t strutcten
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

/* green_thread */
/*
Denna funktion kör den kontext som är running, sparar argument, join
*/

void green_thread() {

  // Hittar den kontext som är "running"
  green_t* this = running;

  // Kör funktionen och spara resultatet
  void* result = (*this->fun)(this->arg);

  
  // Place waiting (joining) thread in ready queue
  if(this->join != NULL) {
    enqueue_ready(this->join);
  }

  // save result of exec
  this->retval = result;
  
  // Set it to zombie (?)
  this->zombie = TRUE;

  // Find the next thread to run
  green_t* next = dequeue_ready();

  running = next;
  //sigprocmask(SIG_UNBLOCK, &block, NULL);

}



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

  // add new to the ready queue..
  enqueue_ready(new);

  //sigprocmask(SIG_UNBLOCK, &block, NULL);
  
  return 0;
}

/* Green yield */
/*
Denna funktion "väjer", alltså slutar exekvera den nuvarande kontexten tråden och byter till nästa i kön
*/

int green_yield() {
  //sigprocmask(SIG_BLOCK, &block, NULL);
  green_t* susp = running;
  // Lägg till susp i ready kön
  enqueue_ready(susp);

  // Välj nästa tråd
  green_t* next = dequeue_ready();

  running = next;

  //sigprocmask(SIG_UNBLOCK, &block, NULL);

  
  swapcontext(susp->context, next->context);
  
  
  return 0;
}

/* Green join */
/*
Pausa den nuvarande tråden och vänta på att den i argumentet exekveras
*/
int green_join(green_t* thread, void** res) {

  //sigprocmask(SIG_BLOCK, &block, NULL);

  if(!thread->zombie) {
    green_t* susp = running;
    // add as joining thread
    thread->join = susp;
    // select next thread
    green_t* next = dequeue_ready();

    running = next;
    printf("running: %p\n", running);

    //sigprocmask(SIG_UNBLOCK, &block, NULL);
    
    swapcontext(susp->context, next->context);

   
  }
  // Collect result
  // Vi kommer hit då "thred" har exekverat (kallat green_thread på den).
  // Då har alltså "thread" exekverat sin funktion, som ska skickas tillbaka till susp
  res = thread->retval;
  
  // Free context
  // Ta bort thread kontexten, den är donzo
  // Vet inte riktigt om det är rätt saker att göra free på, använde de som malloc kallades på.
  free(thread->context->uc_stack.ss_sp);
  free(thread->context);

  //   sigprocmask(SIG_UNBLOCK, &block, NULL);

  return 0;
}

/* Conditional variables */
/*
green_cond_t* är en länkad lista

green_cond_init() skapar en länkad lista

green_cond_wait() lägger till element

green_cond_signal() poppar från listan
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
  
  // Hitta running och lägg till den i listan
  green_t* susp = running;
  
  green_t* index = cond->head;
  // Om listan är tom
  if(index == NULL) {
    cond->head = susp;
    susp->next = NULL;
  } else {
    // Hitta sista elementet
    while(index->next != NULL) {
      index = index->next;
    }
    index->next = susp;
    susp->next = NULL;
  }
  // Kör nästa tråd
  running = dequeue_ready();

  swapcontext(susp->context, running->context);

  //sigprocmask(SIG_UNBLOCK, &block, NULL);
}
// Tar ut ur listan
void green_cond_signal(green_cond_t* cond) {
  
  // sigprocmask(SIG_BLOCK, &block, NULL);

  green_t* index = cond->head;
  if(index == NULL) {
    // Borde skriva write() här om det ska vara kvar
    // printf("Attempted to signal an empty queue\n");
    return;
  }
  // Ta ut element ur länkad lista
  green_t* retval = cond->head;
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
