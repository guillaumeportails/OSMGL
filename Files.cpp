//
// Temps de chargement de "rhone-alpes.osm.bz2" (GCC 4.5, multi-core)
//  maxSize  -DPTHREAD   -UPTHREAD
//    16 Ko      94s        111s
//   256 Ko      61s        110s
// Idem avec "rhone-alpes.osm" :
//    16 Ko      68s         66s
//   256 Ko      60s         67s
// Avec 512 Ko, le temps "bz2" ne bassie guere, donc on peut rester a 256 Ko.

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>

#include "Files.h"
#include "zlib.h"
#include "bzlib.h"

void IByteFileReader::Feed (void)
{
  fill = Feed (buffer);
}

#ifndef HAS_PTHREAD
void IByteFileReader::Async_Feed (void)
{
  Feed();
}

#else
// Sommet "C" du thread
extern "C" void *thread_entry (void *arg)
{
  // Execute the entry() method in this thread
  ((IByteFileReader *) arg)->entry();
  return NULL;
}

// Idem que Feed(), mais fait dans un thread dedie : pendant que l'appelant
// consomme les donnees recues, ce thread charge les suivantes
void IByteFileReader::Async_Feed (void)
{
  // Create thread and synchonizers (first or previously failed)
  if (! pinit)
  {
    sem_init (&sem_feed, 0, 0);
    sem_init (&sem_filled, 0, 0);
    pinit = (pthread_create (&pt, NULL, thread_entry, this) == 0);
    if (!pinit) perror ("pthread_create");
  }

  if (! pinit)
    Feed();       // Back to synchronous way
  else
  {
    // Demander un Feed d'avance
    sem_post (&sem_feed);
//  printf ("m Wait\n");
    sem_wait (&sem_filled);
//  printf ("m Obtained\n");
  }
}

void IByteFileReader::kill (void)
{
  if (! pinit) return;
  m_kill = true;
//printf ("m Kill\n");
  sem_post (&sem_feed);
  // Au retour de ceci, *this n'existe plus !
}

// Le thread de Feed asycnhrone :
// + fait une lecture en avance, puis attend une demande de
//   lecture. Fourni alors ce qui a ete lu puis relance la lecture
//   qui sera demande plus tard
void IByteFileReader::entry (void)
{
  while (! m_kill)
  {
    // Effectuer la prochaine lecture
    // + backbuffer aussi pourrait etre dans la pile de ce thread,
    //   mais il pourrait alors y avoir une limite a sa taille ?
//  printf ("t Filling\n");
    size_t backfill = Feed(backbuffer);
//  printf ("t Filled %u\n", backfill);

    // Attendre une demande de feed
    // Cela signale que le thread principal attend le buffer donc on peut y toucher
    sem_wait (&sem_feed);

    // Publier la lecture deja effectuee
    memcpy (buffer, backbuffer, backfill);
    fill = backfill;

    // Signaler que le tampon est dispo
    sem_post (&sem_filled);
    // A partir d'ici ce thread ne doit plus toucher le buffer principal, tant que
    // le thread principal n'est pas revenu en attente de feed
  }
  // !! Si m_kill, *this n'existe plus. Cela peut-il gener le retour de cette methode ?
//printf ("t Killed\n");
}
#endif



class PlainReader : public IByteFileReader
{
public:
  PlainReader (const char *filename)
  {
    if ((fd = open (filename, O_RDONLY)) < 0)
      perror(filename);
  }

  ~PlainReader ()
  {
    if (fd >= 0) close (fd);
  }

  size_t Feed (char *b)
  {
//  printf ("p Filling\n");
    if (fd < 0) return 0;
    int n = read (fd, b, maxSize);
//  printf ("p Filled %d\n", n);
    if (n < 0) n = 0;
    return n;
  }

private:
  int fd;
};


class Bz2Reader : public IByteFileReader
{
public:
  Bz2Reader (const char *filename)
  {
    if ((fp = fopen (filename, "rb")) == NULL)
      perror(filename);
    else
    {
      fb = BZ2_bzReadOpen (&bzerror, fp, 0, 0, NULL, 0);
      if (fb == NULL)
      {
        perror (filename);
        fclose(fp);
      }
    }
  }

  ~Bz2Reader ()
  {
    if (fb != NULL)
    {
      BZ2_bzReadClose (&bzerror, fb);
      fclose (fp);
    }
  }

  size_t Feed (char *b)
  {
//  printf ("z Filling\n");
    if (fb == NULL) return 0;
    int n = BZ2_bzRead (&bzerror, fb, b, maxSize);
//  printf ("z Filled %d\n", n);
    if (n < 0) n = 0;
    return n;
  }

private:
  int bzerror;
  FILE *fp;
  BZFILE *fb;
};



IByteFileReader *NewByteFileReader (const char *filename)
{
  int n = strlen(filename);     // ! Ne marcherait pas en UTF-8

  while ((n >= 0) && (filename[n] != '.')) --n;

  if (! strcmp(filename+n, ".bz2")) return new Bz2Reader (filename);

  return new PlainReader (filename);
}

