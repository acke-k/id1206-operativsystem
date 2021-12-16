#include <stdio.h>

#define ALIGN 8
#define MIN(size) (((size) > (8))?(size):(8))

int adjust(size_t request) {
  int minsize = MIN(request);
  if(minsize % ALIGN == 0) {
    return request;
  } else {
    int adj = minsize % ALIGN;
    return minsize + ALIGN - adj;
  }
}

int main(void) {
  for(int i = 0; i < 30; i++) {
    printf("i: %d   adjusted: %d\n", i, adjust(i));
  }
  return 0;
}
