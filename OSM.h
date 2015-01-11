/// @file  OSM.h
/// @brief Open Street Map  geo data tools
///
/// TODO:
/// + Support a PBF reader

#ifndef _H_OSM
#define _H_OSM

#include <stdint.h>
#include <math.h>
#include <vector>
#include <map>

// private only
#include "expat.h"

//-------------------------------------------------------------------------------
// A) Liste de quelques choix d'organisation de la representation d'un OSM

// Choix : les id XML contiennent au moins [0..2^63-1] car un id=""
//         est un unsignedLong XML d'apres le schema OSM XML
//              http://wiki.openstreetmap.org/wiki/API_v0.6/XSD
//              http://www.w3.org/TR/xmlschema-2/#unsignedLong
//              http://www.w3.org/TR/xmlschema-2/#built-in-datatypes
//              http://wiki.openstreetmap.org/wiki/Xml_schema#Common_attributes
// Mais dans un OSM, en 2010 en tout cas, 32bits suffisent ...
// Est-il rentable de limiter a 32 ? Cf temps de LoadText("rhone-alpes.osm") (2.4Go) :
//   32b = 75.2s  64b = 77.3s  (GCC 4.5 -O2 / core-i5)
// Ce choix influe donc essentiellement sur la RAM, assez peu sur le CPU
#define OSM_ID32                // Limit IDs to 32 bits
#undef  OSM_ID32                // IDs use 64bits (OSM/XML spec)

// Choix : un element OSM (Node,Way,Relation) en memoire doit-il memoriser son
//         propre id XML ?
// Une fois en memoire, les liens entre elements (Relation->Way par exemple) sont
// representes par des index directs, les ID ne servent plus. On peut donc s'en
// passer ... Mais pour debugger du rendu ou autre, il pourrait etre utile de
// savoir afficher l'ID d'un objet dessine, pour savoir aller l'examiner dans
// le source OSM.
// It is clear that ID's are unique per - "OSM and ElementType" :
//      http://wiki.openstreetmap.org/wiki/Xml_schema
// It is unclear if ID's are "planet.osm wide" or if two OSM files may use the
// same ID value for differents elements. Seems true for OSM files comming
// directly from the "server".
#undef  OSM_ID_STORED           // ID's are forgotten
#define OSM_ID_STORED           // Every Element stores it's ID

// Choix : un Node peut toujours etre tagge, mais cela occupe beaucoup de
//         RAM pour rien, car la grande majorite des Node d'un OSM n'a pas
//         de tag.
//         On peut donc choisir de ne pas memoriser que les Node possedant un
//         tag, et d'enregistrer dans les Way directement la latlon des
//         Node qu'il reference
// Supporter deux types de Node, PoorNode et RichNode est plus difficile a gerer

#undef  OSM_NODE_TAGGED         // All nodes are stored
#define OSM_NODE_TAGGED         // Only tagged nodes are stored


//-------------------------------------------------------------------------------
// B) namespace osm : definition des types OSM en memoire

namespace osm {


// Dans la spec OSM, les lat/lon sont representees en degres avec 7 digits
//      http://wiki.openstreetmap.org/wiki/Node
// Or [-180°,180°]/1.0e-7 tient dans 32 bits. On represente donc partout les
// lat/lon en virgule fixe (entier) avec un LSB de 10^-7, ce qui occupe 32bits
// au lieu de 64bits pour un double (32b pour un float, mais imprecis a 1.0e-7)
// L'intervalle canonique de latitude est  [ -90, 90]
// L'intervalle canonique de longitude est [-180,180]
typedef int32_t latlon_t;

static double const latlon_lsb = 1.0e-7;

inline double degree (latlon_t g) { return (double) g * latlon_lsb; }
inline double latlon (double g)   { return (double) g * latlon_lsb; }

// Normalisation dans les intervalles transformables en virgule fixe
inline double canonicalDeg (double g)
{
  g = fmod (g, 360.0);
  if (g >= 180.0) g -=360; else if (g < -180.0) g +=360;
  return g;
}

struct LatLon
{
  latlon_t lat, lon;
  inline double degLat(void) { return degree (lat); }
  inline double degLon(void) { return degree (lon); }
};


// Un pave rectangulaire de lat/lon
class LatLonBox
{
public:
  LatLon min, max;

  // Tout le domaine
  inline void open (void)
  { min.lat = min.lon = -LATLONMAX;
    max.lat = max.lon =  LATLONMAX;
  }

  // Vide
  inline void close (void)
  { min.lat = min.lon =  LATLONMAX;
    max.lat = max.lon = -LATLONMAX;
  }

  // Etendre le pave pour qui'il contienne le point donne
  // + close() is the required initial state
  inline void extend (LatLon &ll)
  {
    if (ll.lat < min.lat) min.lat = ll.lat;
    if (ll.lat > max.lat) max.lat = ll.lat;
    if (ll.lon < min.lon) min.lon = ll.lon;
    if (ll.lon > max.lon) max.lon = ll.lon;
  }

  // C# put/get is better ...
  inline double degMinLat(void) { return degree (min.lat); }
  inline double degMaxLat(void) { return degree (max.lat); }
  inline double degMinLon(void) { return degree (min.lon); }
  inline double degMaxLon(void) { return degree (max.lon); }

private:
  static const latlon_t LATLONMAX = 180*10000000;
};


// Un niveau de zoom : cela va de 0 a 17
typedef unsigned zoom_t;


// Un identifiant d'element de fichier OSM / XML
// Cela n'est pas propre a OSM mais est defini par XML comme occupant au plus 0..2^63-1
// + Compile sur GCC 4.5 32bits, a la lecture de "rhone-alpes.osm" (2.4Go) gprof affirme
//   que strtoull occupe 15% du temps de lecture
//   Les std::map::find de id_t sont le premier poste de cout pour gprof
//   Donc il peut y avoir un interet de perfo a rester sur 32 bits ... qui semble suffisant
//   dans les fichiers OSM vus (en 2011, mais la croissance est exponentielle)

#ifndef OSM_ID32
  typedef uint64_t id_t;        // unsignedLong XML, "API v0.6/XSD" conformant
#else
  typedef uint32_t id_t;        // RAM is expensive ...
#endif


// Les trois types elements d'un OSM : Node, Way, Relation
enum eltType { eltNode, eltWay, eltRelation };

// Noms de proprietes usuelles
// + Le format OSM permet de stocker n'importe quel tags "key=value", neanmoins
//   l'usage prevoit certains "key" :
//      http://wiki.openstreetmap.org/wiki/Map_Features
//   Quelques uns potentiellement utiles au rendu sont regroupes ici
// Bof: Cela pourrait etre fastidieux. La methode osmarender est preferable :
//   Un fichier lu au demarrage contient une association entre "key=value" et
//   une caracteristique de rendu, son LoD minimal, etc.
//   Mais cela melangerait l'objet et son rendu ... donc on se contente ici
//   de memoriser des chaines de caracteres usuelles, pour perfos (memoire,
//   int==int au lieu de strcmp, etc ?)
//
enum commonTag
{
  tagNil = 0,
  tagHighway,
      tagMotorway, tagMotorwayLink, tagTrunk, tagTrunkLink, tagPrimary,
      tagPrimaryLink,
  tagTrafficCalming,
  tagService,
  tagBoundary,
      tagAdministrative, tagCivil, tagPolitical, tagMaritime,

  // Nombre de chaines predefinies
  tagCommonCount
};

//extern XML_Char *commonTagStr[(int) tagCommonCount];

union tagString         // UTF-8  (char compatible)
{
  enum commonTag index;        // Valid only if < tagStringCount
  const char    *pntr;

  inline const char *str(void)
//{ return ((unsigned) index < tagCommonCount) ? commonTagStr[index] : pntr; }
  { return pntr; }
};

struct tagPair
{
  union tagString key, value;
};


// Ensemble de proprietes d'un element
// + C'est assez cher en RAM, alors il ne faut le mettre que dans les elements qui
//   en ont besoin et non pas dans tous (IElement).
//   Compte des Elements sans nom ou avec nom, dans quelques OSM :
//     "rhone-alpes.osm" :    poorNode= 11M  richNode= 17k
//                            poorWay =1.4M  richWay = 68k
//                            poorRel =2.9k  richRel = 10k
//     "ile-de-france.osm" :  poorNode=8.2M  richNode= 19k
//                            poorWay =1.1M  richWay =101k
//                            poorRel = 12k  richRel =4.7k
//   Le resultat change radicalement si on compte avec/sans tag (nom ou autre)
//     "rhone-alpes.osm" :    poorNode= 11M  richNode=846k
//                            poorWay =3.4k  richWay =1.4k
//                            poorRel =   0  richRel = 13k
//     "ile-de-france.osm" :  poorNode=7.8M  richNode=472k
//                            poorWay =3.1k  richWay =1.2M
//                            poorRel =   1  richRel = 17k
//   Mais la proportion de Node sans tag justifie que l'on specialise la classe Node
//   en "avec" ou "sans" tag.
class Tags
{
public:
  // May have a ctor : is not un IElement, no ctor will be called by large containers
  Tags() { Init(); }

  // Usual tags
  char *name;         // Not so usual ?  (UTF-8)
  char layer;         // -5 .. 5,  0 == au sol, -1 == tunnel, etc

  enum Kind
  {
    unknown,
    building,
    highway,
    waterway,
    railway
  } kind;

  unsigned short count;     // Stats : nombre de tags, nom compris

  // Discovered tags
  // + L'examen de quelques OSM montre que de nombreux tags non documentes existent ...
  // TODO: liste de chaines "Atom" : faire une map string->index et in vector index->string
  //       des chaines trouvees.
  //       initialiser cette map/vector avec les chaines predefinies par la "spec" d'emploi
  //       d'OSM ("highway", etc), de sorte qu'elles soient reconnues par le code par un enum
  //       connu a la compilation.
  //       Pour les chaines "nom de propriete" : ok
  //       Pour les chaines "valeur de propriete" : la valeur est a stocker dans sa traduction
  //       binaire plutot que comme une chaine
  std::vector<struct tagPair> pairs;    // ou *pairs pour eviter un ctor systematique ?

  inline void Init (void)
  { name = NULL; layer = 0; kind = unknown; count = 0; }

  inline bool isEmpty (void)
  { return (name == NULL) && (pairs.size() == 0); }

private:
};

extern Tags nilTags;   // Always empty


// Classe racine des elements OSM Node, Way, Relation
// + Aucun IElement et derives n'a pas de constructeur pour eviter des
//   inits volumineuses inutiles quand un Element est dans un conteneur
//   comme std::vector
// + Au sujet de la presence ou non de tags, qui sont cher a enregistrer
//   donc pour lesquels il vaudrait mieux avoir une race sans tag et une race
//   avec, au moins les Node qui sont majoritairement sans tag.
//   Cela compliquerait la taxonomie des Elements, imposerait peut etre
//   un double heritage pour ne pas dupliquer la classe Node, ajouterait
//   une memoire des Node avec et sans, un cas de plus a eltType, etc.
//   Alternative: n'enregistrer que les Node possedant un tag, et stocker
//   dans un Way directement la latlon de ses Node.
//   Un Node tagge reference par un Way est a la fois dans la table des Node
//   et sa latlon est copiee dans le Way (ou les Way).
//   On suppose en effet que les tags d'un Node n'ont pas d'utilite pour le Way,
//   qui possede les siens propres.
//
// Exemples vus :
//   + un Way qui est un building, reference des Node dont l'un possede
//     un tag : celui qui represente la porte d'entree avec le numero d'adresse
//     du batiment a le tag "addr:housenumber", et le Way lui-meme n'a pas cette
//     info ...
//     Pour supporter ces cas, il faut que les Way referencent les Node. Et pour
//     ne pas gacher la RAM, il faut que deux classes Node existent, Poor et Rich.
//     Ou alors il faut copier les tags des Node d'un Way dans le Way lui-meme.
//   + Idem avec le Way building "Musee d'Orsay" dont un Node represente l'horloge
//     murale qu'il porte.
//   + Un Relation reduite a un Node. La Relation possede des tags (permettant le
//     rendu), de sorte que le Node n'a pas besoin de tag (et n'en a pas)
//
// Hierarchies possibles :
//   IElement                    IElement
//     PoorNode-->Node             ITaggedElement
//     ITaggedElement                Node  (*have* tags)
//       RichNode-->Node             Way  (holds latlon's)
//       Way (holds indexes)         Relation
//       Relation
//   Node
//
//   Bof: il ne sert a rien de memoriser un PoorNode, en tout cas pour un Way. Autant
//        memoriser les latlon dans le Way. Quid des Relation qui detiennent des PoorNode ?
//   => Il semble complique d'eloigner les IElement des capacites effectives du format OSM

class IElement
{
public:
  // unique ID value of this OSM Element
  // + Using an inline function has no cost and enables user code syntatic
  //   independance of ODM_ID_STORED
#ifdef OSM_ID_STORED
  inline id_t id (void) const { return mID; };
#else
  inline id_t id (void) const { return 0; };   // 0 is not a valid OSM ID
#endif

  inline virtual void Init()
  {
#ifdef OSM_ID_STORED
    mID = 0;
#endif
  }

  // Type d'element Node / Way / Relation
  inline virtual eltType type (void) const = 0;

  // "Est capable de stocker des tags"
  inline virtual bool tagCapable (void) const
  { return false; }

  inline virtual const Tags& tags (void) const
  { return nilTags; }

  // "Possede au moins un tag" (qui peut etre par exemple "name")
  // + Always false if !tagCapable()
  inline virtual bool hasTag (void) const
  { return false; }

//protected:
#ifdef OSM_ID_STORED
  id_t mID;           // Identifiant unique (dans un fichier ou pour tous ?)
#endif
};

// OSM Element that can store Tags
class ITaggedElement : public IElement
{
public:
  inline virtual const Tags& tags (void) const
  { return *mTags; }

  inline virtual void Init()
  {
#ifdef OSM_ID_STORED
    mID = 0;
#endif
    mTags = &nilTags;
  }

  // "Est capable de stocker des tags"
  inline virtual bool tagCapable (void) const
  { return true; }

  inline virtual bool hasTag (void) const
  { return ! mTags->isEmpty(); }

//protected:
  // A Tags object (set of tags) is much larger than a (void *), so we store
  // it only then at least one tag exists
  Tags *mTags;
};


// Image en memoire d'un fichier OSM, ou de plusieurs accumules
// + Les liens qu'entretiennent entre eux Node, Way et Relation sont designes
//   par les IDs XML dans le fichier OSM. Ici en memoire, on compile ces liens
//   par des index directs. Donc la representation de ces elements est liee a
//   leur presence au sein de OSMData, raison pour laquelles ces classes sont
//   inclues dans OSMData et pas externe.
//   Par exemple, un index de Node n'a de sens que parce que les nodes sont
//   contenus dans un std::vector de OSMData
//   Pourquoi pas un pointeur plutot qu'un index ? Parce que ces conteneurs
//   dynamiquemlent agrandis lors de la lecture ne garantissent pas que l'adresse
//   des objets contenus est inchangee (ils contiennent des valeurs, pas des
//   "objets").

class OSMData
{
public:
  OSMData();
  ~OSMData();

  // Lire un fichier OSM (XML)
  // + Les fichiers OSM comprimes en ".bz2" ou ".gz" sont admis
  // + Les donnees sont ajoutees a l'existant, il est permis d'appeler LoadText
  //   plusieurs fois.
  //   Si un element Node/Way/Relation est deja charge, le second est ignore
  // + Un fichier OSM "naturel" contient la totalite des infos de sa region, a
  //   l'echelle la plus fone (zoom 17). Ils peuvent donc etre tres grand.
  //   Par exemple, "rhone-alpes.osm" en 2011/01 pese 2.4Go (163Mo en bz2)
  //   Il y a donc des version de LoadText pour ne charger qu'une partie filtree
  //   sur un domaine geographique, ou pour un niveau de zoom defini
  //
  // TODO: facteur de zoom, pour ne pas charger des details inutiles si
  //       cet OSM est destine a produire une vue de zoom faible, etc.
  // TODO: PBF reader
  void LoadText (const char *filename);
  void LoadText (const char *filename, LatLonBox &clip);

  // Quelques statistiques de dernier appel a LoadText
  // - Nombre d'elements Node/Way/Relation designes mais absents
  //   (l'absence peut etre due au filtrage demande)
  //   Ceci peut arriver si un OSM contient des "references en avant",
  //   i.e. un Way dont les Node seraient decrits apres le Way.
  //   L'exament de quelques fichiers montre que ceci est negligeable
  //   et le lecteur fonctionne donc en une seule passe
  //   TODO: sinon ecrire un LoadText qui recompile ensuite au format
  //         canonique
  unsigned m_badrefwn,       // Node inconnu reference par un Way
           m_badrefr;        // Element inconnu reference par un Relation
  LatLonBox m_filebound;     // Limites annoncees par le fichier
  LatLonBox m_loadbound;     // Limites du domaine effectivement charge

  // Evaluer les limites effectives de ce qui est charge
  // Bof: systematique lors du Load(), toute appli en aura besoin inutile
  //      d'economiser cela
//void ComputeBound ();


  // Un Node
  // + C'est un simple point sur la carte, qui peut faire partir d'un autre element
  // + Il peut etre tag-capable, auquel cas dans cette version 'riche', un Way memorise
  //   un index de Node, ce qui lui permet d'acceder aux eventuels tags de son Node qui
  //   pourraient affecter son rendu. Par contre c'est assez cher en RAM alors que la
  //   majorite des Node n'ont pas de tag. Donc :
  // + Il peut etre tag-incapable, auquel cas un Way memorise 

#ifdef OSM_NODE_TAGGED
  class Node : public ITaggedElement
#else
  class Node : public IElement
#endif
  {
  public:
    inline eltType type (void) const { return eltNode; }

    // A Node is only a Lat/Lon and zero or some tags.
    LatLon pos;

    // Definition a partir des chaines lat="" et lon="" du OSM/XML
    // + gprof prouve que ceci est un des principaux consomateurs de CPU au
    //   chargement aussi est-il inline avec un "atoi()" dedie
    //   Le temps de LoadText passe ainsi de 75s a 67s sur "rhone-alpes.osm"
    inline void set (const char *latimg, const char *lonimg)
    { pos.lat = fixedlatlon (latimg);
      pos.lon = fixedlatlon (lonimg);
    }

//  inline void set (double latdeg, double londeg)
//  { pos.lat = latlon (canonicalDeg(latdeg));
//    pos.lon = latlon (canonicalDeg(londeg));
//  }
  };

  // NB: Le cout memoire de ce double heritage est supportable
  //     Le sizeof() est juste la somme des deux classes meres
  //     Ce sont juste les VTable qui doivent se compliquer ?

//class PoorNode : public Node, public IElement
//{
//  inline eltType type (void) { return eltNode; }
//};

//class RichNode : public Node, public ITaggedElement
//{
//  inline eltType type (void) { return eltNode; }
//};


  // Un Way
  // + C'est un ensemble ordonne de Node, formant un ligne ou delimitant une aire

  class Way : public ITaggedElement
  {
  public:
    inline eltType type (void) const { return eltWay; }

    // "the same node is at first and last"
    // + So it is an area, event if no tag tells it ?
    inline bool isLoop(void) const
    { return nodesIx.front() == nodesIx.back(); }

    // Index des Node constituant le Way
    // + nodesIx[i] == -1 si le Node i n'a pas pu etre trouve a partir
    //   de son ID.
    //   Ceci est une provision, en l'etat actuel un tel Node n'est plus
    //   conserve dans le Way. En effet, le cas ne se presente pas dans les
    //   fichiers OSM testes
    // + On a couramment plus de 2^16 Nodes dans un fichier, donc pas possible
    //   de stocker ces index sur un short
    std::vector<int> nodesIx;      // Indexes in m_nodes (-1 if unknown)

    // TODO: est-il utile de garder un pointeur vers les Node et leurs Tags ?
    //       reproduire ici la lat/lon ne suffit-il pas ? Un Node est rarement
    //       partage par plusieurs Way, et les Nodes d'un Way possedent-ils des
    //       Tags propres a heriter vers le Way ?
    //       Un node = 2 x mot-32, un ptr = 1 mot-32
  };


  // Un Relation
  // + C'est un ensemble de Node, de Way, et de Relation  (resursif)

  class Relation : public ITaggedElement
  {
  public:
    inline eltType type (void) const { return eltRelation; }

    struct Member       // Un pointeur + RTTI suffirait prendrait moins de place ?
    {
      eltType elt;
      int     ix;
    };
    std::vector<Member> eltIx;       // Indexes (-1 if unknown)
  };

#if 0
  template<class E>
    class SetOf
    {
      std::vector<E>    m_elts;         // Liste des objets E
      std::map<id_t,unsigned> m_idmap;  // Trouver l'indice dans m_elts suivant E.id
      void registerID (id_t);
    };
  SetOf<Node>        m_toto;
#endif

  // Les ensembles d'objets lus
  // + Il y a un vector pour chacun des 3 types d'elements,
  //   plus une map pour convertir "id XML" -> "index dans le vector"
  //   Seuls les elements entres dans le vector ont une entree dans la map
  //   donc l'index enregistre en map est toujours defini ([0..zize-1])
  //   L'ordre des elements dans un vector n'est pas particulier (c'est celui
  //   du fichier OSM)
  // + Pour resoudre des references en avant, il faudrait noter dans la map
  //   un index=-1, et lorsque l'element est enfin defini et qu'on voit qu'il
  //   est deja en map avec -1, alors il faudrait parcourir les hotes possibles
  //   (Way et Relation) et corriger, ce qui imposerait de stocker aussi les
  //   IDs (64bits) des membres des Way/Relation.
  //   => On fait l'impasse. Il apparait sur les OSM testes qu'il n'existe pas
  //      de reference en avant.
  //      Par ailleurs le nombre de Node non reference, bien que non nul, est
  //      negligeable face a la totalite.
  // + TODO: template pour cette paire vector/map ?

  std::vector<Node>       m_nodes;         // Liste des Node
  std::map<id_t,unsigned> m_idnodes;       // Map id -> index dans m_nodes

  std::vector<Way>        m_ways;          // Liste des Way
  std::map<id_t,unsigned> m_idways;        // Map id -> index dans m_nodes

  std::vector<Relation>   m_relations;     // Liste des Relation
  std::map<id_t,unsigned> m_idrelations;   // Map id -> index dans m_nodes


public:
  int findNodeIx (id_t id);
  int findWayIx (id_t id);
  int findRelationIx (id_t id);
  int findIx (eltType elt, id_t id);

private:
//struct ParserContext          // Si ceci s'avere volumineux
//{
    XML_Parser m_parser;
    ITaggedElement *m_curelt;
    bool m_inway, m_inrel;
//};
//ParserContext *m_ctx;
//

  inline void newNode (const XML_Char **atts);
  inline void endNode (void);
  inline void newWay  (const XML_Char **atts);
  inline void newND   (id_t id);
  inline void endWay  (void);
  inline void newRelation (const XML_Char **atts);
  inline void newMember (id_t id, enum eltType elt, const XML_Char *role);
  inline void endRelation (void);
  inline void addTag (const XML_Char *key, const XML_Char *value);
public: // really private
  void startElementHandler (const XML_Char *name, const XML_Char **atts);
  void endElementHandler (const XML_Char *name);

private:
  static latlon_t fixedlatlon (const char *str);      // strtoul() "optimise"

};

}  // namespace osm

#endif
