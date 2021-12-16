#include <stdio.h>
#include <unistd.h>

void *malloc(size_t size)
{
  if (size == 0)
    {
      return NULL;
    }
  void *memory = sbrk(size);  // Flytta fram break pekare size bytes
  if (memory == (void *)-1)
    {
      return NULL;  // Om sbrk är utanför minne(?)
    } else
    {
      return memory;
    }
}

void free(void *memory)
{
  return;
}
