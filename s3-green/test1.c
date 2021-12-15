#include <stdio.h>
#include "green.h"

int flag = 0;

green_cond_t cond;

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
  int loop = 4 + id;
  printf("loop: %d\n", loop);
  
  int i = 0;

  while(loop > 0) {
    printf("\niteration: %d\n", i++);
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


int main() {
  
  green_cond_init(&cond);

  
  green_t g0, g1;
  printf("g0: %p\tg1: %p\n", &g0, &g1);
  int a0 = 0;
  
  int a1 = 1;
  green_create(&g0, timer_test, &a0);
  green_create(&g1, timer_test, &a1);

  green_join(&g0, NULL);
  green_join(&g1, NULL);
  
  return 0;
  
}
