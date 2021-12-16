#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "green.h"

int flag = 0;
volatile int counter;

volatile int def_counter;

int loops = 10000000;

green_cond_t cond;
green_cond_t cond2;

green_mutex_t mutex;
green_mutex_t mutex2;


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
    green_mutex_lock(&mutex);
    def_counter += 1;
    green_mutex_unlock(&mutex);
  }
  return 0;
}

void* last_test(void* arg) {
  int id = *(int*)arg;
  int loop = 4;
  while(loop > 0) {
    green_mutex_lock(&mutex);
    while(flag != id) {
      green_mutex_unlock(&mutex);
      green_cond_wait(&cond, NULL);
      green_mutex_lock(&mutex);
    }
    flag = (id + 1) % 2;
    
    printf("id: %d  func 1\n", id);
 
    green_cond_signal(&cond);;
    
    green_mutex_unlock(&mutex);
    
    loop--;
  }
}

void* last_test_2(void* arg) {
  int id = *(int*)arg;
  int loop = 100000;
  while(loop > 0) {
    green_mutex_lock(&mutex2);
    while(flag != id) {
      green_cond_wait(&cond2, &mutex2);
    }
    flag = (id + 1) % 2;

    counter++;
 
    green_cond_signal(&cond2);
    
    green_mutex_unlock(&mutex2);
    
    loop--;
  }
}


int main() {
  
  green_cond_init(&cond);
    green_cond_init(&cond2);
  
  green_mutex_init(&mutex);
  green_mutex_init(&mutex2);
  

  
  green_t g0, g1, g2, g3;
  
  int a0 = 0;
  int a1 = 1;
  int a2 = 2;
  int a3 = 3;
  
  //green_create(&g0, last_test_2, &a0);
  // green_create(&g1, last_test, &a1);
  
  green_create(&g2, last_test_2, &a0);
  green_create(&g3, last_test_2, &a1);

  //green_join(&g0, NULL);
  //green_join(&g1, NULL);

  double tstart = clock();
  
  green_join(&g2, NULL);
  green_join(&g3, NULL);

  double timeend = clock();

  printf("time: %f\n", timeend - tstart);

    printf("join was a success\n");
  
  printf("counter: %d\n", counter);
  printf("defined counter: %d\n", def_counter);
  
  return 0;
  
}
