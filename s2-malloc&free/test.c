#include <stddef.h>
#include <stdio.h>
#include "dlmall.h"

int main() {
  int *a = dalloc(8);
  int *b = dalloc(9);
  int *c = dalloc(10);
  int *d = dalloc(11);
  dfree(a);
  dfree(b);
  dfree(c);
  dfree(d);
  sanity(1);
  benchmark(1, 1);
  return 0;
}
  
