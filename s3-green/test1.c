#include <stdio.h>
#include <stdlib.h>
#include "green.h"

int flag = 0;
volatile int counter;

green_cond_t cond;

green_mutex_t* a_mutex;

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
      green_cond_wait(&cond);
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

  for(int i = 0; i < 100000; i++) {
      
    counter += 1;
    printf("id %d\n", id);
  }
  green_yield();
  return 0;
}



int main() {
  
  green_cond_init(&cond);

  
  green_t g0, g1;
  printf("g0: %p\tg1: %p\n", &g0, &g1);
  
  int a0 = 0;
  int a1 = 1;
  int a3 = 100000;

  //  a_mutex = malloc(sizeof(green_mutex_t));
  
  //green_mutex_init(a_mutex);
  
  green_create(&g0, test, &a0);
  green_create(&g1, test, &a1);
  printf("thread is running\n");
  
  printf("test\n");
  green_join(&g0, NULL);
  printf("test2\n");
  green_join(&g1, NULL);
  
  return 0;
  
}
