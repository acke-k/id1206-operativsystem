#include <stdio.h>
#include <stdlib.h>
#include "green.h"

int flag = 0;
volatile int counter;

volatile int def_counter;

int loops = 10000000;

green_cond_t cond;

green_mutex_t mutte;
  green_mutex_t* a_mutex = &mutte;

void* test(void* arg) {
  int i = *(int*)arg;
  int loop = 4;
  while(loop > 0) {
    printf("thread %d: %d\n", i, loop);
    loop--;
    green_yield();
  }
}

void* test_cond(void* arg) {
  int id = *(int*)arg;
  int loop = 4;

  while(loop > 0) {
    if(flag == id) {
      printf("thread %d: %d\n", id, loop);
      loop--;
      flag = (id + 1) % 2;
      green_cond_signal(&cond);
    } else {
      green_cond_wait(&cond, NULL);
    }
  }
}


void* timer_test(void* arg) {
    int id = *(int*)arg;
    int i;
    while(1) {
      printf("%d is running\n", id);
      i = 0;
      while(i++ < 2<<(28 - id)) {}
      printf("done!\n");
    }
}

// Ett test där vår implementation ej fungerar
void* undefined_test(void* arg) {
  int id = *(int*)arg;

  for(int i = 0; i < loops; i++) {
      
    counter += 1;
  }
  return 0;
}

void* defined_test(void* arg) {
  int id = *(int*)arg;

  for(int i = 0; i < loops; i++) {
    green_mutex_lock(a_mutex);
    def_counter += 1;
    green_mutex_unlock(a_mutex);
  }
  return 0;
}



int main() {
  
  green_cond_init(&cond);
  
  green_mutex_init(a_mutex);
  

  
  green_t g0, g1, g2, g3;
  
  int a0 = 0;
  int a1 = 1;
  int a2 = 2;
  int a3 = 3;
  
  green_create(&g0, defined_test, &a0);
  green_create(&g1, defined_test, &a1);

  green_create(&g2, undefined_test, &a2);
  green_create(&g3, undefined_test, &a3);

  
  green_join(&g0, NULL);
  green_join(&g1, NULL);
  green_join(&g2, NULL);
  green_join(&g3, NULL);

    printf("join was a success\n");
  
  printf("counter: %d\n", counter);
  printf("defined counter: %d\n", def_counter);
  
  return 0;
  
}
