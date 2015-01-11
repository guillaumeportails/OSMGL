#include <stdio.h>
#include <fcntl.h>
#include <string.h>             // Works for UTF-8 (strdup, strcpy, etc)
//#include <assert.h>

#include "OSM.h"

#include "Files.h"

// On peut ne pas verifier la syntaxe, ce qui permet de gagner du temps d'exec
// dans une lecture de fichier. Par contre s'il contient des erreurs, le donnees
// chargees seront sans doute abimees, sans que cela soit su.
#ifdef OSM_CHECKSYNTAX
#define checkSyntax(truth)     { if (! (truth)) throw "syntaxError"; }
#else
#define checkSyntax(truth)     {}
#endif


namespace osm {


//-----------------------------
// Donnees globales a tous les objets OSM
//
// TODO: A mettre static dans la classe OSMData ?

Tags nilTags;   // Always empty

#if 0
char *commonTagStr[(int) tagCommonCount] =
{
  "",                           // tagNil = 0,
  "highway",                    // tagHighway,
  "motorway",                   // tagMotorway,
  "motorway-link",              // tagMotorwayLink,
  "trunk",                      // tagTrunk,
  "trunk-link",                 // tagTrunkLink,
  "primary",                    // tagPrimary,
  "primary-link",               // tagPrimaryLink,
  "traffic-calming",            // tagTrafficCalming,
  "service",                    // tagService,
  "boundary",                   // tagBoundary,
  "administrative",             // tagAdministrative,
  "civil",                      // tagCivil,
  "political",                  // tagPolitical,
  "maritime",                   // tagMaritime,
};
#endif

// Liste des chaines decouvertes
// Associe une map et un vector pour memoriser les chaines vues dans les tags "key=value"
// afin de ce memoriser chaque chaine qu'une seule fois (partage)
//
// + Avec ceci au lieu de strdup() systematiques, la RAM du process lors
//   du chargement de "rhone-alpes.osm" (2.4Go) economise 320Mo.
//   (1.092Go au lieu de 1.413Go)
//   Le temps de chargement passe de 61s a 65s
//
// TODO: Allouer des blocs de RAM et inserer les chaines dedans plutot que strdup() ou new string ?

class StringStock
{
public:
  inline const char *FindOrAdd (const char *str)
  {
     const char *n;
     map_t::const_iterator i = mMap.find(str);
     if (i == mMap.end())
     {
       n = strdup (str);                // TODO: comment tracer free ?
       mVector.push_back (n);
       mMap[n] = mVector.size()-1;
     }
     else
       n = (*i).first;
     return n;
  }

private:
  struct less_str : public std::binary_function<char *, char *, bool>
  {
    bool operator() (const char * x, const char * y) const
    { return strcmp (x, y) < 0; }
  };

  typedef std::map<const char *, int, less_str> map_t;
  std::vector<const char *> mVector;
  map_t mMap;
};

static StringStock globalStringStock;



// NB: Les exceptions passent-elles a travers ceci ?

static void XMLCALL C_startElementHandler
    (void *userData,
     const XML_Char *name,
     const XML_Char **atts)
{
  ((OSMData *) userData)->startElementHandler (name, atts);
}

static void XMLCALL C_endElementHandler
    (void *userData,
     const XML_Char *name)
{
  ((OSMData *) userData)->endElementHandler (name);
}



OSMData::OSMData()
{
  m_parser  = NULL;

  // Optimiser les petits fichiers ... ce qui n'est pas la vocation ici
  m_nodes.reserve (10000);
  m_ways.reserve (2000);
  m_relations.reserve (1000);
  m_filebound.close();
}

OSMData::~OSMData()
{
  if (m_parser != NULL) XML_ParserFree(m_parser);
}

void OSMData::LoadText (const char *filename)
{
  LatLonBox clip;
  clip.open();                  // Pas de restriction
  LoadText (filename, clip);
}

void OSMData::LoadText (const char *filename, LatLonBox &clip)
{
  IByteFileReader *f = NewByteFileReader (filename);
  if (f == NULL) throw "nofile";

  m_parser = XML_ParserCreate (NULL);   // NB: Defaut OSM encoding is UTF-8

  XML_SetUserData (m_parser, this);
  XML_SetElementHandler (m_parser, C_startElementHandler, C_endElementHandler);

  // Les compteurs de ne pas rinces : ce LoadText est en fait un Append,
  // possible car l'espace des ID est commun a tous les OSM
  m_inway = false;
  m_inrel = false;
  m_badrefwn = 0;
  m_badrefr  = 0;
  m_loadbound.close();  // So that extend() works

  for (;;)
  {
    XML_Status status;

    // Obtenir un bloc a parser
    f->Async_Feed();

    try
    {
      status = XML_Parse(m_parser, f->buffer, f->fill, f->fill == 0);
    }
    catch (...)
    {
      printf ("EXC line %d  node=%d way=%d relations=%d\n",
            (int) XML_GetCurrentLineNumber (m_parser),
            m_nodes.size(), m_ways.size(), m_relations.size());
      delete f;
      throw;
    }
    if (! status)
    {                           // Parse error
      delete f;
      throw "syntaxError";
    }

    if (f->fill == 0) break;    // EOF
  }

  delete f;
}

int OSMData::findNodeIx (id_t id)
{
  std::map<id_t,unsigned>::const_iterator i = m_idnodes.find(id);
  if (i == m_idnodes.end()) return -1;
  return i->second;

//for (unsigned i = 0; i < m_nodes.size(); ++i)
//  if (m_nodes[i].id == id) return i;
//return -1;
}

int OSMData::findWayIx (id_t id)
{
  std::map<id_t,unsigned>::const_iterator i = m_idways.find(id);
  if (i == m_idways.end()) return -1;
  return i->second;

//for (unsigned i = 0; i < m_ways.size(); ++i)
//  if (m_ways[i].id == id) return i;
//return -1;
}

int OSMData::findRelationIx (id_t id)
{
  std::map<id_t,unsigned>::const_iterator i = m_idrelations.find(id);
  if (i == m_idrelations.end()) return -1;
  return i->second;

//for (unsigned i = 0; i < m_ways.size(); ++i)
//  if (m_relations[i].id == id) return i;
//return -1;
}

int OSMData::findIx (eltType elt, id_t id)
{
  switch (elt)
  {
    case eltNode     : return findNodeIx (id);
    case eltWay      : return findWayIx (id);
    case eltRelation : return findRelationIx (id);
  }
  throw "ProgramError";
}

static const XML_Char *value (const XML_Char **atts, const XML_Char *name)
{
  int guard = 20;
  while ((atts != NULL) && (*atts != NULL) && (--guard > 0))
  {
    if (!strcmp(atts[0], name)) return atts[1];
    atts += 2;
  }
  return "";
}

static inline id_t idvalue (const XML_Char *str)
{
#ifndef OSM_ID32
  // strtoull() occupe 15% du temps de lecture de "rhone-alpes.osm"
  return strtoull (str, NULL, 10);
#else
  return strtoul (str, NULL, 10);
#endif
}

// Verifier un nom d'element XML : si le fichier OSM est bien fait, on peut
// se contenter de comparer le 1er chr ... sauf pour nd/node
// => La lecture de rhone-alpes.osm (2.4Go) passe de (GCC 4.5.0 MinGW) :
//      strcmp : 80.8s    [0&1] : 79.2s      "-g -O3"
//      strcmp : 80.0s    [0&1] : 78.6s      "-g -O2"
//    nb: la charge CPU est a 100%, donc pas ralenti par les I/O disque ?
static inline bool sameEltName (const char *name, const char *check)
{
//return ! strcmp (name, check);
  return (name[0] == check[0]) && (name[1] == check[1]);
}

static inline enum eltType eltvalue (const XML_Char *str)
{
  if (sameEltName(str, "node"))     return eltNode;
  if (sameEltName(str, "way"))      return eltWay;
  if (sameEltName(str, "relation")) return eltRelation;
  checkSyntax(false);
  throw "syntaxError";  // on ne peut pas laisser faire
}

void OSMData::startElementHandler (const XML_Char *name, const XML_Char **atts)
{
  // Les differents element XML d'un OSM sont :
  //   osm, bounds, node, way, relation, nd, member, tag
  // On peut donc, presque, se contenter de tester le 1er chr ce qui va mieux
  // qu'un strcmp
  // Mesure sur rhone-alpes.osm (2.4G) GCC 4.5.0 MinGW en "-O2 -DNDEBUG -g" :
  //    strcmp = 80.0s,  switch = 78.7s  + newND fixe = 78.1s
  switch (name[0])
  {
    case 'o' :  // XML "osm" element : ignored
    {
      checkSyntax (!strcmp (name, "osm"));
    }
    break;

    case 'b' :  // XML "bounds" element
    {
      checkSyntax (!strcmp (name, "bounds"));
      m_filebound.min.lat = fixedlatlon (value (atts, "minlat"));
      m_filebound.max.lat = fixedlatlon (value (atts, "maxlat"));
      m_filebound.min.lon = fixedlatlon (value (atts, "minlon"));
      m_filebound.max.lon = fixedlatlon (value (atts, "maxlon"));
    }
    break;

    case 'n' :  // XML "node" or "nd" element
    {
      if (name[1] == 'o')
      {
        checkSyntax (!strcmp (name, "node"));
        newNode (atts);
      }
      else
      {
        checkSyntax (!strcmp (name, "nd"));
#if 1
        // Pas la peine de chercher, "ref" est toujours atts[0] ?
        checkSyntax (! strcmp (atts[0], "ref"));
        newND (idvalue(atts[1]));
#else
        newND (idvalue(value(atts, "ref")));
#endif
      }
    }
    break;

    case 'w' :  // XML "way" element
    {
      checkSyntax (!strcmp (name, "way"));
      newWay (atts);
    }
    break;

    case 'r' :  // XML "relation" element
    {
      checkSyntax (!strcmp (name, "relation"));
      newRelation (atts);
    }
    break;

    case 'm' :  // XML "member" element
    {
      checkSyntax (!strcmp (name, "member"));
      newMember (idvalue(value(atts, "ref")),
                 eltvalue(value(atts, "type")),
                 value(atts, "role"));
    }
    break;

    case 't' :  // XML "tag" element
    {
      checkSyntax (!strcmp (name, "tag"));
      // Pas la peine de chercher, c'est toujours 0="k" et 2="v" ... ?
      addTag (value(atts, "k"), value(atts, "v"));
    }
    break;
  }
}

void OSMData::endElementHandler (const XML_Char *name)
{
  switch (name[0])
  {
    case 'n' :  // XML "node/" element (or "nd/")
      if (name[1] == 'o')
      {
        checkSyntax (!strcmp (name, "node"));
        endNode ();
      }
    break;

    case 'w' :  // XML "way/" element
      checkSyntax (!strcmp (name, "way"));
      endWay ();
    break;
  }
}


inline void OSMData::newNode (const XML_Char **atts)
{
  unsigned const cur = m_nodes.size();
// Ceci a l'air bien pour la RAM ... mais est catastrophique en temps car
// on fait alors enormement de copies, sur un gros OSM
//if (cur+10 > m_nodes.capacity()) m_nodes.reserve (cur + 1000);
  m_nodes.resize (cur + 1);
  Node &p = m_nodes.back();
  p.Init();
  const id_t id = idvalue (value (atts, "id"));
#ifdef OSM_ID_STORED
  p.mID = id;
#endif
  m_idnodes[id] = cur;
  p.set (value (atts, "lat"), value (atts, "lon"));
  m_loadbound.extend (p.pos);   // Meme si ce node n'est pas reference
  m_curelt = &p;                // Si Node supporte les tags
//m_curelt = NULL;              // Node ne peut pas avoir de tag
}

inline void OSMData::endNode (void)
{
  m_curelt = NULL;
}

inline void OSMData::newWay (const XML_Char **atts)
{
  unsigned const cur = m_ways.size();
  m_ways.resize (cur + 1);
  Way &p = m_ways.back();
  p.Init();
  const id_t id = idvalue (value (atts, "id"));
#ifdef OSM_ID_STORED
  p.mID = id;
#endif
  m_idways[id] = cur;
  m_curelt = &p;
  m_inway = true;
}

inline void OSMData::newND (id_t id)
{
  if (! m_inway) return;          // Possible en saturation, et en cas de OSM faux
  int idx = findNodeIx (id);      // Les node sont avant les way dans un OSM ?
  if (idx < 0)                    // Seuls les Node existant sont enregistres dans les Way
    ++m_badrefwn;
  else
    m_ways.back().nodesIx.push_back (idx);
}

inline void OSMData::endWay (void)
{
  m_curelt = NULL;
  m_inway  = false;
}


inline void OSMData::newRelation (const XML_Char **atts)
{
  unsigned const cur = m_relations.size();
  m_relations.resize (cur + 1);
  Relation &p = m_relations.back();
  p.Init();
  const id_t id = idvalue (value (atts, "id"));
#ifdef OSM_ID_STORED
  p.mID = id;
#endif
  m_idrelations[id] = cur;
  m_curelt = &p;
  m_inrel = true;
}

inline void OSMData::newMember (id_t id, enum eltType elt, const XML_Char *role)
{
  if (! m_inrel) return;          // Possible en saturation, et en cas de OSM faux
  Relation::Member m;
  m.elt = elt;
  m.ix  = findIx (elt, id);  // Pas de reference en avant dans un OSM ?
  if (m.ix < 0)
    ++m_badrefr;                // Seuls les references existants sont memorises
  else
    m_relations.back().eltIx.push_back (m);
}

inline void OSMData::endRelation (void)
{
  m_curelt = NULL;
  m_inrel  = false;
}

inline void OSMData::addTag (const XML_Char *key, const XML_Char *value)
{
  if (m_curelt == NULL) return;
//if (m_curelt->tagCapable()) return;

  // S'il n'y a pas encore de Tags, il est temps d'en allouer
  if (m_curelt->mTags == &nilTags)
    m_curelt->mTags = new Tags;

  Tags * const tags = m_curelt->mTags;

  (tags->count)++;     // Stats pour aider la conception

  // Tags usuel
  if (! strcmp (key, "name"))
    // Il n'y a pas de raison qu'un nom d'Element soit partage ? strdup suffit
    // plutot que globalStringStock ?
    tags->name = strdup (value);
  else if (! strcmp (key, "layer"))
    // la valeur de layer est censee entre dans [-5,5], on en verifie pas
    tags->layer = atoi (value);
  else
  { // discovered tags
    tagPair pair;
    pair.key.pntr   = globalStringStock.FindOrAdd ((const char *) key);
    pair.value.pntr = globalStringStock.FindOrAdd ((const char *) value);
    tags->pairs.push_back(pair);

    // pre-decodage
    // + Des places sont parfois notees highway=footway en plus de area=yes,
    //   on prend donc ce cas en premier
    // TODO : A generaliser/communaliser dans globalStringStock avec commonTagStr
//  if      (! strcmp (key, "area"))     tags->kind = Tags::area;
         if (! strcmp (key, "building")) tags->kind = Tags::building;
    else if (! strcmp (key, "highway"))  tags->kind = Tags::highway;
    else if (! strcmp (key, "railway"))  tags->kind = Tags::railway;
    else if (! strcmp (key, "waterway")) tags->kind = Tags::waterway;
  }
}

// "[-]NNN.NNNNNNN"
// Bien plus rapide que passer par atof(). Sur rhone-alpes.osm GCC 4.5.0 -O2 :
//    atof = 75s   fixedlatlon = 67s
int OSMData::fixedlatlon (const char *str)
{
  bool const negative = (*str == '-');
  int lsb = 10000000;
  int val = 0;
  char c;

  if (negative) ++str;

  // Partie entiere
  while (((c = *str++) != '\0') && (c != '.'))
  {
    checkSyntax ((c >= '0') && (c <= '9'));
    val = val*10 + (int) (c - '0');
  }

  // Partie fractionnaire
  // + Un peu plus rapide a executer que de traiter les deux dans la meme boucle
  //   (mais plus long a coder) ... on gagne 0.5s sur rhones-alpes.osm (66s)
  if (c == '.')
  {
    while (((c = *str++) != '\0'))
    {
      checkSyntax ((c >= '0') && (c <= '9'));
      val = val*10 + (int) (c - '0');
      lsb /= 10;
    }
  }

  val *= lsb;
  return (negative) ? -val : val;
}

}  // namespace osm

