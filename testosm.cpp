#include <sys/time.h>
#include <stdio.h>
#include <getopt.h>
//#ifdef __GNUC__
//#include <mcheck.h>
//#endif
#include "OSM.h"

#include "rusage.h"

int main (int argc, char **argv)
{
//uint64_t id = 0;      // Can always hold a id_t whatever OSM_ID32

  // CLI options
  int c;
  bool opt_nodes = false;      // Show all nodes
  bool opt_ways  = false;      // Show all ways
  bool opt_relations = false;  // Show all relations
  bool opt_manyrefs = false;   // Show Node having >10 references
  bool opt_reftaged = false;   // Show Node having tags and references
  bool opt_rnnotag = false;    // Show Node having R-ref and no tag

  while ((c = getopt(argc, argv, "nwrmts")) > 0)
    switch (c)
    {
      case 'n' : opt_nodes     = true; break;
      case 'w' : opt_ways      = true; break;
      case 'r' : opt_relations = true; break;
      case 'm' : opt_manyrefs  = true; break;
      case 't' : opt_reftaged  = true; break;
      case 's' : opt_rnnotag   = true; break;
    }
  if (optind != argc-1) return -1;

  osm::OSMData OSM;

  // Restriction a une zone.
  osm::LatLonBox clip;
  clip.open();          // Par defaut, tout prendre


  // Lecture de fichier, chronometree pour test des perfs
  {
    struct timeval prev, curr;
    print_rusage();
    gettimeofday(&prev, NULL);
    OSM.LoadText (argv[optind], clip);
    gettimeofday(&curr, NULL);
    double dur =   (double) (curr.tv_sec - prev.tv_sec)
                 + (double) (curr.tv_usec - prev.tv_usec)/1.0e6;
    printf ("Loaded OSM file in %.3fs\n", dur);
    print_rusage();
  }

  // Liste des noeuds
  if (opt_nodes)
  {
    for (unsigned i = 0; i < OSM.m_nodes.size(); ++i)
    {
      osm::OSMData::Node &p = OSM.m_nodes[i];
      printf ("%10.7f %10.7f  %10I64u  T=%d %s\n",
          p.pos.degLat(), p.pos.degLon(), (uint64_t) p.id(),
          p.tags().count,
          (p.tags().name) ? p.tags().name : "");
    }
    printf ("\n\n");
  }

  // Liste des chemins
  if (opt_ways)
  {
    for (unsigned i = 0; i < OSM.m_ways.size(); ++i)
    {
      osm::OSMData::Way &p = OSM.m_ways[i];
      printf ("%5d %s\n",
          p.nodesIx.size(),
          (p.tags().name) ? p.tags().name : "");
    }
    printf ("\n\n");
  }

  // Liste des relations ayant un nom
  if (opt_relations)
  {
    for (unsigned i = 0; i < OSM.m_relations.size(); ++i)
    {
      osm::OSMData::Relation &p = OSM.m_relations[i];
      if (p.tags().name == NULL) continue;
      printf ("%5d %s\n",
          p.eltIx.size(),
          (p.tags().name) ? p.tags().name : "");
    }
    printf ("\n\n");
  }

  // Bounds
  printf ("#\n");
  printf ("# Lat : %10.7f %10.7f\n", OSM.m_filebound.degMinLat(), OSM.m_filebound.degMaxLat());
  printf ("# Lon : %10.7f %10.7f\n", OSM.m_filebound.degMinLon(), OSM.m_filebound.degMaxLon());


  // Compte compare des elements tagges et non taggees
  unsigned tagN[2] = { 0, 0 };
  for (unsigned i = 0; i < OSM.m_nodes.size(); ++i)
    (tagN[(OSM.m_nodes[i].hasTag()) ? 1 : 0])++;
  unsigned tagW[2] = { 0, 0 };
  for (unsigned i = 0; i < OSM.m_ways.size(); ++i)
    (tagW[(OSM.m_ways[i].hasTag()) ? 1 : 0])++;
  unsigned tagR[2] = { 0, 0 };
  for (unsigned i = 0; i < OSM.m_relations.size(); ++i)
    (tagR[(OSM.m_relations[i].hasTag()) ? 1 : 0])++;

  // Recherche du nombre de noeuds non references
  // + Sert a voir s'il y a frequement ces pertes dans les OSM, pour savoir s'il
  //   est rentable de les supprimer du lecteur
  unsigned *refsN = new unsigned[OSM.m_nodes.size()];
  unsigned *refsW = new unsigned[OSM.m_ways.size()];
  unsigned *refsR = new unsigned[OSM.m_relations.size()];
  unsigned bad1=0, bad2=0, bad3=0, bad4=0, bad5=0, bad6=0, bad7=0;
  for (unsigned i = 0; i < OSM.m_nodes.size();     refsN[i++] = 0);
  for (unsigned i = 0; i < OSM.m_ways.size();      refsW[i++] = 0);
  for (unsigned i = 0; i < OSM.m_relations.size(); refsR[i++] = 0);
  printf ("#\n");
  for (unsigned w = 0; w < OSM.m_ways.size(); w++)      // Node references par les Way
  {
    osm::OSMData::Way &p = OSM.m_ways[w];
    for (unsigned i = 0; i < p.nodesIx.size(); ++i)
    {
      if (p.nodesIx[i] < 0)
        ++bad1;
      else if (p.nodesIx[i] >= (int) OSM.m_nodes.size())
        ++bad2;
      else
      {
        ++(refsN[p.nodesIx[i]]);
      }
    }
  }
  printf ("# Node refs from Way : bad1=%u  bad2=%u\n", bad1, bad2);

  // Nbre de Node references et ayant neanmoins des tags
  unsigned NWTagged = 0;
  for (unsigned n = 0; n < OSM.m_nodes.size(); n++)
  {
    if ((refsN[n] > 0) && (OSM.m_nodes[n].hasTag()))
    {
      ++NWTagged;
      if (opt_reftaged)
        printf ("Node %10I64u has %d refs and %d tags\n",
            (uint64_t) OSM.m_nodes[n].id(), refsN[n],
            OSM.m_nodes[n].tags().count);
    }
  }

  // Histogramme des nombres de references des Node par les Way ... zut.
//std::map<unsigned,unsigned> histoN;

  // Nombre de reference a un Node par les Relation en direct
  unsigned *refRN = new unsigned[OSM.m_nodes.size()];
  for (unsigned i = 0; i < OSM.m_nodes.size(); refRN[i++] = 0);
  for (unsigned r = 0; r < OSM.m_relations.size(); r++)      // Node references par les Relation
  {
    osm::OSMData::Relation &p = OSM.m_relations[r];
    for (unsigned i = 0; i < p.eltIx.size(); ++i)
    {
      int k = p.eltIx[i].ix;
      switch (p.eltIx[i].elt)
      {
        case osm::eltNode :
          if (k < 0) ++bad1; else if (k >= (int) OSM.m_nodes.size()) ++bad2;
          else { ++(refsN[k]); ++refRN[k]; }
          break;
        case osm::eltWay :
          if (k < 0) ++bad3; else if (k >= (int) OSM.m_ways.size()) ++bad4;
          else ++(refsW[k]);
          break;
        case osm::eltRelation :
          if (k < 0) ++bad5; else if (k >= (int) OSM.m_ways.size()) ++bad6;
          else ++(refsR[k]);
          break;
        default : ++bad7;
      }
    }
  }
  printf ("# refs from Rel : bad's %u %u %u %u %u %u %u\n",
      bad1, bad2, bad3, bad4, bad5, bad6, bad7);

  // Node references par un Relation et n'ayant poutant pas de tag (=> quel rendu ?)
  unsigned RNnotag = 0;
  for (unsigned n = 0; n < OSM.m_nodes.size(); n++)
  {
    if ((refRN[n] > 0) && (! OSM.m_nodes[n].hasTag()))
    {
      ++RNnotag;
      if (opt_rnnotag)
        printf ("Node %10I64u has %d R-refs and no tags\n",
            (uint64_t) OSM.m_nodes[n].id(), refRN[n]);
    }
  }

  // Histogramme des nombres de references des Node par les Way ... zut.

  unsigned refN0=0, refN1=0, refN2=0, refN3=0, refN4=0,
           refN5=0, refN6=0, refN7=0, refN8=0, refN9=0;
  for (unsigned i = 0; i < OSM.m_nodes.size(); i++)
  {
//  if (OSM.m_nodes[i].tags ... compter les cas de Node sans tag
    switch (refsN[i])
    {
      case 0 : ++refN0; break; case 1 : ++refN1; break;
      case 2 : ++refN2; break; case 3 : ++refN3; break;
      case 4 : ++refN4; break; case 5 : ++refN5; break;
      case 6 : ++refN6; break; case 7 : ++refN7; break;
      case 8 : ++refN6; break; case 9 : ++refN9; break;
    }
  }
  printf("# Node refs never=%u 1=%u 2=%u 3=%u 4=%u 5=%u 6=%u 7=%u 8=%u 9=%u\n",
      refN0, refN1, refN2, refN3, refN4, refN5, refN6, refN7, refN8, refN9);

  // Stats d'allocation
  printf ("#\n");  //     1234567890 1234567890 1234567890 1234567890 123456 123456
  printf ("#           sz     in-OSM   capacity     no-tag     tagged reftag RNnotg\n");
  printf ("# Node     %3d %10u %10u %10u %10u %6u %6u\n", 
      sizeof(osm::OSMData::Node),
      OSM.m_nodes.size(), OSM.m_nodes.capacity(), tagN[0], tagN[1], NWTagged, RNnotag);
  printf ("# Way      %3d %10u %10u %10u %10u\n",
      sizeof(osm::OSMData::Way),
      OSM.m_ways.size(), OSM.m_ways.capacity(), tagW[0], tagW[1]);
  printf ("# Relation %3d %10u %10u %10u %10u\n",
      sizeof(osm::OSMData::Relation),
      OSM.m_relations.size(), OSM.m_relations.capacity(), tagR[0], tagR[1]);
  printf ("# Tags     %3d\n",
      sizeof(osm::Tags));


  if (opt_manyrefs)
  {
    printf ("\n\n");
    for (unsigned i = 0; i < OSM.m_nodes.size(); i++)
      if (refsN[i] >= 10)
        printf ("Node %I64u has %d refs\n", (uint64_t) OSM.m_nodes[i].id(), refsN[i]);
  }
}

