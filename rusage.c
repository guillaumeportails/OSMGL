// Helper to have memory usage of the OSM reader, as MinGW does not have getrusage()
//
// NB: GetSystemInfo -> dwNumberOfProcessors : pour File.h
//     GlobalMemoryStatus

#include <stdio.h>
#include "rusage.h"

#ifdef WIN32
#include <windows.h>
#include <psapi.h>              // needs -lpsapi
#endif


void print_rusage (void)
{
#if defined(WIN32)
//int i;
  static SIZE_T previous = 0;
  DWORD minWSS, maxWSS;
  GetProcessWorkingSetSize (GetCurrentProcess(), &minWSS, &maxWSS);

#if 0
  const DWORD maxH = 100;
  HANDLE heaps[maxH], nbrh;
  nbrh = GetProcessHeaps (maxH, heaps);
  if (nbrh > maxH) nbrh = maxH;
  unsigned long hsize = 0;
  for (i = 0; i < nbrh; ++i)
    hsize += HeapSize (heaps[i], 0, heaps[i]);

  MEMORY_BASIC_INFORMATION mbi;
  mbi.RegionSize = 1234;
  if (VirtualQuery (NULL, &mbi, sizeof(mbi)) != sizeof(mbi))
  {
  }

  printf ("RUSAGE: VQ=%ld minWSS=%ld maxWSS=%ld\n", mbi.RegionSize, minWSS, maxWSS);
#endif

  PROCESS_MEMORY_COUNTERS PMC;
  memset(&PMC, 0, sizeof(PMC));

  // Set size of structure
  PMC.cb = sizeof(PMC);

  // Get memory usage
  GetProcessMemoryInfo(GetCurrentProcess(), &PMC, sizeof(PMC));

  printf ("RUSAGE: PFU += %6ld kb      peakPFU=%3ld.%03ld  peakWSS=%3ld.%03ld (Mbytes)\n",
         (PMC.PeakPagefileUsage-previous)/1024,
         PMC.PeakPagefileUsage/(1024*1024), (PMC.PeakPagefileUsage%(1024*1024))/1000,
         PMC.PeakWorkingSetSize/(1024*1024), (PMC.PeakWorkingSetSize%(1024*1024))/1000);

  previous = PMC.PeakPagefileUsage;

#endif
}


#if 0
// gcc -o rusage rusage.c -lpsapi
int main(int argc, char **argv)
{
  int const n = atoi(argv[1])*1024*1024;
  int i;
  print_rusage();
  char *a = malloc(n);
  print_rusage();
  for (i = 0; i < n; ++i) a[i] = i;
  print_rusage();
  free(a);
  print_rusage();
}
#endif


