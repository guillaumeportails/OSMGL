// Helper class for text file reading, either plain or compressed

#ifdef HAS_PTHREAD
#include <pthread.h>
#include <semaphore.h>  // POSIX 1003.1b <= pthread.h ?
#endif

class IByteFileReader
{
public:
  // Taille de lecture. Plus c'est grand, plus -DHAS_THREAD est rentable
  // Alloue sur le tas et pas la pile, et temporaire, donc peut etre "assez" grand
  static const size_t maxSize = 256*1024;
  char buffer[maxSize];                 // Tampon de lecture
  size_t fill;                          // Nombre d'octets presents dans buffer

  // Lire au plus maxSize octets
  // + Si fill==0 suite a cela, c'est fini
  void Feed (void);

  // Idem que Feed(), mais fait dans un thread dedie : pendant que l'appelant
  // consomme les donnees recues, ce thread charge les suivantes
  // Si les pthreads sont indospo, alors est idem que Feed
  void Async_Feed (void);

#ifdef HAS_PTHREAD
  IByteFileReader() { pinit = false; } 
  ~IByteFileReader() { kill(); }
private:
  pthread_t pt;
  sem_t sem_feed, sem_filled;
  bool pinit, m_kill;
  char backbuffer[maxSize];          // Tampon de lecture ping/pong
  void kill (void);
public:
  void entry (void);    // private
#endif
private:
  virtual size_t Feed (char *b) = 0;
};

IByteFileReader *NewByteFileReader (const char *filename);

