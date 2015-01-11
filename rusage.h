
#ifdef __cplusplus
extern "C" {
#endif

struct rusage
{
  long ru_maxrss;
  long ru_minrss;
};

  
void print_rusage (void);

#ifdef __cplusplus
}       /* extern "C" */
#endif

