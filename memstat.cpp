
#include <stdlib.h>

size_t memstat_sumnew = 0;
size_t memstat_sumdel = 0;

void * operator new (size_t size)
{
  memstat_sumnew += size;
  size_t *p = (size_t *) malloc (size);
  p[0] = size;
  return p+1;
}

void operator delete (void *p)
{
  size_t *h = (size_t *)p;
  memstat_sumdel += h[0];
  free (h+1);
}

