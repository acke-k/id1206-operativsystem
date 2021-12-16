/*
Benchmark för att jämföra min implementation av green-threads med pthreads
*/

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "green.h"

int loops = 0;
#define FIB 10

double t1 = 0;
double t2 = 0;;

int counter1 = 0;
int counter2 = 0;

int flag1 = 0;
int flag2 = 0;

pthread_mutex_t lock1;
green_mutex_t lock2;

pthread_cond_t cond1;
green_cond_t cond2;

/*
Short function which adds some computing time in each loop
*/

int factorial(int n) {
   if(n == 0) {
      return 1;
   } else {
      return n * factorial(n-1);
   }
}

int fib(int n) {
   if(n == 0){
      return 0;
   } else if(n == 1) {
      return 1;
   } else {
      return (fib(n-1) + fib(n-2));
   }
}

/* Yield() benchmarks 
Measure the time it takes
*/
void* bmark_pthread_yield(void* arg) {

  volatile int lim =  *(int*)arg;;
  while(lim > 0) {
    fib(FIB);
    lim--;
    pthread_yield();
  }
}

void* bmark_greent_yield(void* arg) {
  
  volatile int lim =  *(int*)arg;
  while(lim > 0) {
    fib(FIB);
    lim--;
    green_yield();
  }
}
/* No yield (timer interrupts?)
Same test but no yield
*/
void* bmark_pthread_timer(void* arg) {

  volatile int lim =  *(int*)arg;
  while(lim > 0) {
    fib(FIB);
    lim--;
  }
}

void* bmark_greent_timer(void* arg) {
  
  volatile int lim =  *(int*)arg;
  while(lim > 0) {
    fib(FIB);
    lim--;
  }
}
/* Mutex
Increment a global counter which is protected by a mutex.
*/

void* bmark_pthread_mutex(void *arg) {
  int id = *(int*)arg;
  volatile int lim = 0;
  lim += loops;
  
  while(lim > 0) {
    pthread_mutex_lock(&lock1);
    while(flag1 != id) {
      pthread_cond_wait(&cond1, &lock1);
    }
    flag1 = (id + 1) % 2;
    counter1++;
    fib(FIB);
    pthread_cond_signal(&cond1);
    pthread_mutex_unlock(&lock1);
    lim--;
  }
}

void* bmark_greent_mutex(void *arg) {
  int id = *(int*)arg;
  volatile int lim = 0;
  lim += loops;
  
  while(lim > 0) {
    green_mutex_lock(&lock2);
    while(flag1 != id) {
      green_cond_wait(&cond2, &lock2);
    }
    flag1 = (id + 1) % 2;
    counter2++;
    fib(FIB);
    green_cond_signal(&cond2);
    green_mutex_unlock(&lock2);
    lim--;
  }
}

/*
argv[1] - no loops 
*/
int main(int arc, char* argv[]) {
  
  double tstart;
  double tend;

  int arg1 = atoi(argv[1]);
  int arg2 = atoi(argv[2]);

  int* parg1 = &arg1;
  int* parg2 = &arg2;

  int id1 = 0;
  int id2 = 1;

  int* pid1 = &id1;
  int* pid2 = &id2;

  loops = arg2;
 
  printf("%d\t", arg2);
  
  /* pthread loop benchmark */
  /* 
  pthread_t p1, p2;

  pthread_create(&p1, NULL, bmark_pthread_yield, parg1);
  pthread_create(&p2, NULL, bmark_pthread_yield, parg2);

  tstart = clock();
  
  pthread_join(p1, NULL);
  pthread_join(p2, NULL);

  tend = clock();

  //printf("time p_yield: %f ms\n", tend - tstart);
  printf("%f\t", tend - tstart);

  /* greenthread loop benchmark */
  /*
  green_t g1, g2;

  green_create(&g1, bmark_greent_yield, parg1);
  green_create(&g2, bmark_greent_yield, parg1);

  tstart = clock();
  
  green_join(&g1, NULL);
  green_join(&g2, NULL);

  tend = clock();

  //printf("time g_yield: %f ms\n", tend - tstart);
    printf("%f\n", tend - tstart);

  /* pthread timer benchmark */
  /*
  pthread_t p3, p4;

  pthread_create(&p3, NULL, bmark_pthread_timer, parg2);
  pthread_create(&p4, NULL, bmark_pthread_timer, parg2);

  tstart = clock();
  
  pthread_join(p3, NULL);
  pthread_join(p4, NULL);
  
  
  tend = clock();

  //printf("time p_timer: %f ms\n", tend - tstart);
  printf("%f\t", tend - tstart);
  
  /* greenthread timer benchmark */
  /*
  green_t g3, g4;

  green_create(&g3, bmark_greent_timer, parg2);
  green_create(&g4, bmark_greent_timer, parg2);

  tstart = clock();
  
  green_join(&g3, NULL);
  green_join(&g4, NULL);

  tend = clock();

  //printf("time g_timer: %f ms\n", tend - tstart);
  printf("%f\n", tend - tstart);
    
  /* pthread mutex benchmark */
  
  pthread_t p5, p6;

  pthread_create(&p5, NULL, bmark_pthread_mutex, pid1);
  pthread_create(&p6, NULL, bmark_pthread_mutex, pid2);

  pthread_mutex_init(&lock1, NULL);

  pthread_cond_init(&cond1, NULL);
  
  tstart = clock();

  pthread_join(p5, NULL);
  pthread_join(p6, NULL);

  tend = clock();

  //printf("counter1: %d\n", counter1);

  //printf("time p_mutex: %f ms\n", tend - tstart);
  printf("%f\t", tend - tstart);
  
  /* greenthread mutex benchmark */
  
  green_t g5, g6;
  
  green_create(&g5, bmark_greent_mutex, pid1);
  green_create(&g6, bmark_greent_mutex, pid2);

  green_mutex_init(&lock2);
  green_cond_init(&cond2);

  tstart = clock();

  green_join(&g5, NULL);
  green_join(&g6, NULL);

  tend = clock();

  //printf("counter2: %d\n", counter2);

  //printf("time g_mutex: %f ms\n", tend - tstart);
  printf("%f\n", tend - tstart);
  
  /**/

  return 0;
}


  
