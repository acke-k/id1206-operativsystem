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

// Skapar "default" tråden & dess kontext
static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, NULL, FALSE};
static green_t* running = &main_green;

// Header-elementet för ready kön
green_t* readyq = NULL;

// Förhindra implicit declaration error
void queue_add(green_t* element, green_t** queue);
green_t* queue_rem(green_t** queue);
void printqueue();

/* Implementation av trådar */

/* Green_thread
Kör funktionen i "running" tråden och terminerar sedan tråden.
Om denna tråden vill "joina" (har en tråd som väntar på den) så läggs den väntade tråden på ready kön.
*/
void green_thread() {
  sigprocmask(SIG_UNBLOCK, &block, NULL); // Behöver skyddar p.g.a. listor ändras
  
  green_t* this = running;  // Hittar den kontext som är "running"
  void* result = (*this->fun)(this->arg);   // Kör funktionen och spara resultatet
  
  // Lägg till tråden som väntar i ready-kön
  if(this->join != NULL) {
    queue_add(this->join, &readyq);
  }
  
  this->retval = result;  // Spara resultatet av this->fun i this->retval
  this->zombie = TRUE;  // Sätt den i zombie state

  green_t* next = queue_rem(&readyq); // Kör nästa tråd i ready-kön
  running = next;
  setcontext(next->context);
}

/* green_create
Tar in en green_t struct, en funktion den ska utföra och args för den funktionen.
Kopplar ihop green_t med dessa, fäster även en ny context till den och slutligen lägger den i "ready queue".
Kort sagt skapar den nya trådar.
*/
int green_create(green_t* new, void* (*fun)(void*), void* arg) {
  sigprocmask(SIG_BLOCK, &block, NULL);

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

  queue_add(new, &readyq);   // Lägg till i ready-kön
  //printqueue(readyq);
  
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}
/* green_yield
Slutar exekvera den nuvarande tråden, lägger den längst bak på ready-kön och börjar exekvera nästa i kön.
*/

int green_yield() {
  sigprocmask(SIG_BLOCK, &block, NULL); // Vi börjar med att ändra ready-kön så allt borde skyddas
  
  green_t* susp = running;
  queue_add(susp, &readyq);  // Lägg till susp i ready kön
  green_t* next = queue_rem(&readyq);  // Välj nästa tråd

  running = next;  // Kör nästa tråd
  swapcontext(susp->context, next->context); 
 
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

/* green join
"running" tråden suspenderas tills tråden "thread" har exekverat.
När thread har exekverat kommer running att väckas och fortsätta exekvera i denna funktion.
Resultatet av thread kommer då vara redo i retval.
*/
int green_join(green_t* thread, void** res) {
  sigprocmask(SIG_BLOCK, &block, NULL); // Skydda allt
  
  if(!thread->zombie) {
    green_t* susp = running;
    thread->join = susp; // Lägg till som joining tråd i "thread"
    green_t* next = queue_rem(&readyq); // Kör nästa tråd
    running = next;
    
    swapcontext(susp->context, next->context);
    sigprocmask(SIG_UNBLOCK, &block, NULL);
  }
  // Collect result
  res = thread->retval;  // Spara resultatet av join tråden i res-pekaren

  free(thread->context->uc_stack.ss_sp); // Deallokera allt minne för thread
  free(thread->context);

  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

/* Conditional variables */
/*
Funktionerna nedan implementerar conditional variables.
En conditional variable har en länkad lista.
Wait lägger till en tråd i listan och signal poppar en till ready-kön.
*/

// Initierar den länkade listan
void green_cond_init(green_cond_t* new) {
  new->susp_list = NULL;
}
// Lägg till running tråden i en lista
void green_cond_wait(green_cond_t* cond, green_mutex_t *mutex) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  green_t* susp = running;    // Hitta running och lägg till den i listan
  printf("queue before add\n");
  printqueue(cond->susp_list);
  queue_add(susp, &(cond->susp_list));
  printf("aftwewards:\n");
  printqueue(cond->susp_list);
  /*
  // Om mutex existerar
  // Alltså: Om vi angav mutex i argumentet kommer vi släppa en suspendad i den mutexen
  // och ge den låset. Detta görs tills mutexen är tom
  if(mutex != NULL) {
    if(mutex->head != NULL) {
      green_t* desusp = mutex->head;
      mutex->head = mutex->head->next;
      desusp->next = NULL;
      enqueue_ready(desusp);

      mutex->taken = FALSE;
    }
  }
  */
  running = queue_rem(&readyq);
  swapcontext(susp->context, running->context);
  /*
  // Nu är vi tillbaka till den tråden som suspenderades
  if(mutex != NULL) {
    // try o take the lock
    if(mutex->taken) {
      
    } else {
      // take the lock
      mutex->taken = TRUE;
    }
  }
  */
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return;
}
// Tar ut en tråd ur listan och lägger den i ready-kön
void green_cond_signal(green_cond_t* cond) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  
  green_t* desusp = queue_rem(&(cond->susp_list));
  queue_add(desusp, &readyq);

  sigprocmask(SIG_UNBLOCK, &block, NULL);
}

/* timer_handler
Denna funktion bestämmer vad som händer då timern varvar
*/
void timer_handler(int sig) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  green_t* susp = running;

  // Add the running to the ready queue
  queue_add(susp, &readyq);

  // find the next thread for execution
  green_t* next = queue_rem(&readyq);
  running = next;
  
  swapcontext(susp->context, next->context);
  sigprocmask(SIG_UNBLOCK, &block, NULL);
}
/* Mutex */
int green_mutex_init(green_mutex_t* mutex) {
  mutex->taken = FALSE;
  mutex->susplist = NULL;
}
/* green_mutex_lock
Försöker ta låset. 
Om det är låst suspendas tråden och väcks när låset är ledigt.
*/
int green_mutex_lock(green_mutex_t* mutex) {
  sigprocmask(SIG_BLOCK, &block, NULL);

  green_t* susp = running;
  // Om mutex är låst, suspenda tråden
  if(mutex->taken) {
    queue_add(susp, &(mutex->susplist));   

    green_t* next = queue_rem(&readyq);     // Starta nästa tråd i ready-kön
    running = next;
    swapcontext(susp->context, next->context);
    } else {
    mutex->taken = TRUE; // Om Låset är ledigt, ta det
  }
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}
/* green_mutex_unlock
Låser upp ett lås och ger det till en tråd som väntar på det (om sådan finns)
*/
int green_mutex_unlock(green_mutex_t* mutex) {
  sigprocmask(SIG_BLOCK, &block, NULL);

  // Om det finns en tråd som väntar på låset
  if(mutex->susplist != NULL) {
    green_t* desusp = queue_rem(&(mutex->susplist)); // Ta första tråden i kön
    queue_add(desusp, &readyq);  // Lägg den i readyq
    // effekt av detta: Den tråd vi lade i ready-kön kommer få låset när den exekveras
  } else {
    mutex->taken = FALSE;  // Om ingen tråd väntar
  }
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

/* Sanity */
/*
Printar diverse information om current state av trådar
*/
void sanity() {
  printf("\nsanity start\n");
  printf("Running process: %p\n", running);
  printf("ready q\n");
  green_t* index = readyq;
  while(index != NULL) {
    printf("%p\n", index);
    index = index->next;
  }
  printf("done\n");
}

/* Denna funktion initierar vår första tråd vid startup (automatiskt) */
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

/* Queue
Hjälpfunktioner som skapar green_t köer.
De utnyttjar next pekaren i green_t structen för att skapa länkade listor.
Godtycklig green_t* pekare kan användas som huvud. (Den bör == NULL från början)
*/
// Lägg till element längst bak i kön
void queue_add(green_t* element, green_t** queue) {
  if(*queue == NULL) {
    *queue = element;
    element->next = NULL;
  } else {
    green_t* index = *queue;
    while(index->next != NULL) {
      index = index->next;
    }
    index->next = element;
     element->next = NULL;
  }
  return;
}
// Poppa första elementet i kön
green_t* queue_rem(green_t** queue) {
  green_t* retval = *queue;
  if(retval == NULL) {
    printf("attempted to pop an empty queue\n");
  }
  *queue = retval->next;
  retval->next = NULL;

  return retval;
}
// Skriv adresser för alla element i kön
void printqueue(green_t* queue) {
  green_t* index = queue;
  printf("\nprint queue\n");
  while(index != NULL) {
    printf("%p\n", index);
    index = index->next;
  }
  printf("print done\n");
}
/*
int main() {
  // queue test
  
  green_t* queue = NULL;
  
  green_t a;
  queue_add(&a, &queue);
  printf("a: %p\n", &a);
  
  printqueue(queue);

  queue_rem(queue);
  printqueue(queue);
  
  green_cond_t acond;
  green_cond_t* apoint = &acond;
  apoint->susp_list = NULL;
  green_t bgreen;

  queue_add(&bgreen, &(apoint->susp_list));
  printf("acond: %p\n", apoint);
  printf("acond suslist: %p\n", apoint->susp_list);
  
  
  // queue_add(susp, &(cond->susp_list));
}
*/

