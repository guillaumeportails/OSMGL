
//
// En "demontant" dans InkScape un SVG produit par osmarender (XML/OSM -> SVG par XSLT) on y voit que :
// + Un Way est fait de segments de droite larges a bout rond (sans doute une paire de Node successifs).
//   Un segment sombre est en arriere-plan, immediatement surmonte d'un segment clair de taille
//   legerement inferieure : ce qui produit des routes claires bordees de lignes sombres, et construit
//   des intersections meme complexes correctes.
//   => A reproduire avec des QUAD textures ?
//
// Rappel :
//   QUAD_STRIP : 2468...   QUAD :  23 67 ...  TRIANGLE_STRIP : 24  ...
//                1357...           14 58 ...                   135 ...

#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "osmRender.h"


static void Material (GLfloat r, GLfloat g, GLfloat b, GLfloat s)
{
  const GLfloat p = 0.5;
  const GLfloat q = 1.0 - p;
  { GLfloat f[4] = { r*q, r*q, r*q, 1.0 }; glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, f); }
  { GLfloat f[4] = { 1.0, 1.0, 1.0, 1.0 }; glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, f); }
  { GLfloat f[4] = { r*p, r*p, r*p, 1.0 }; glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, f); }
  glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, s);
  glColor3f (r, g, b);         // Au cas ou LIGHTING est Disabled
}


osmRender::osmRender ()
{
  // Toujours en WGS84
  mGeo = &geoWGS84;

  mFont = NULL;
//mFont = new FTExtrudeFont ("C:\\Windows\\Fonts\\arial.ttf");
//mFont = new FTHaloFont ("C:\\Windows\\Fonts\\arial.ttf");
//mFont = new FTBufferFont ("C:\\Windows\\Fonts\\arial.ttf");
  if (mFont && mFont->Error())
  {
    printf ("Failed to load font\n");
    delete mFont;
    mFont = NULL;
  }
}


void osmRender::Bind (osm::OSMData *osm)
{
  // Noter une reference ce que doit etre dessine
  mOSM = osm;

  // Initialiser la table des indicateurs "ce Way est deja reference par un Relation"
  mWdone.resize (mOSM->m_ways.size());
  for (unsigned i = 0; i < mWdone.size(); ++i) mWdone[i] = false;

  // Placer le repere au centre de l'OSM mappe sur le geoide
  double lat = (  mOSM->m_loadbound.min.degLat()
                + mOSM->m_loadbound.max.degLat()) / 2.0;
  double lon = (  mOSM->m_loadbound.min.degLon()
                + mOSM->m_loadbound.max.degLon()) / 2.0;
  printf ("Centre : %10.7f %10.7f   %u rels\n",
      lat, lon,
      mOSM->m_relations.size());

  Geo::LLA here;
  here.lat = lat / 180.0 * M_PI;
  here.lon = lon / 180.0 * M_PI;
  here.alt = 0.0;
  mGeo->toLocal (here, &mTrep);
}

void osmRender::Project (double degLat, double degLon, mgl::Vec3 *vec3) // double alt = 0.0)
{
  Geo::LLA lla;
  lla.lat = degLat / 180.0 * M_PI;
  lla.lon = degLon / 180.0 * M_PI;
  lla.alt = 0.0;        // SRTM !

  Geo::XYZ xyz, vec;
  mGeo->toXYZ (lla, &xyz);
  Geo::Transform (xyz, mTrep, &vec);

  vec3->vec[0] = (mgl::Real) vec.x;
  vec3->vec[1] = (mgl::Real) vec.y;
  vec3->vec[2] = (mgl::Real) vec.z;

//printf ("Node %10.7f %10.7f    %6.1f %6.1f %6.1f\n", degLat, degLon, vec.x, vec.y, vec.z);
}


void osmRender::Render (void)
{
  mVertices = 0;        // Stats : nombre de sommets

  // Ground
  // A discretiser suivant l taille de l'OSM : faire des QUADS dans lequel le Z local
  // est constant a 1m pres.
  mgl::Vec3 v;
  Material (0.8, 0.8, 0.8, 10.0);
  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  glNormal3d (0.0, 0.0, 1.0);
  glBegin (GL_QUADS);
  {
    const GLdouble margin = 50.0;
    Project (mOSM->m_loadbound.min.degLat(), mOSM->m_loadbound.min.degLon(), &v);
    glVertex3d (v.vec[0], v.vec[1], v.vec[2]-margin);
    Project (mOSM->m_loadbound.min.degLat(), mOSM->m_loadbound.max.degLon(), &v);
    glVertex3d (v.vec[0], v.vec[1], v.vec[2]-margin);
    Project (mOSM->m_loadbound.max.degLat(), mOSM->m_loadbound.max.degLon(), &v);
    glVertex3d (v.vec[0], v.vec[1], v.vec[2]-margin);
    Project (mOSM->m_loadbound.max.degLat(), mOSM->m_loadbound.min.degLon(), &v);
    glVertex3d (v.vec[0], v.vec[1], v.vec[2]-margin);
  }
  glEnd();

//return;

  // Relations
  for (unsigned n = 0; n < mOSM->m_relations.size(); ++n)
    RenderRelation (n);

  // Ways
  // Des Way ne sont pas references par les relations, donc il faut les
  // tracer et seulement ceux-ci : mWdone permet de ne pas dessiner les Way
  // deja vu par RenderRelation
//glColor3f (0.0f, 1.0f, 1.0f);         // Pour ditinguer Relation et Way
  for (unsigned n = 0; n < mOSM->m_ways.size(); ++n)
    RenderWay (n);
}


void osmRender::RenderName (const mgl::Vec3 here, const osm::Tags &tags, int size)
{
  if ((tags.name != NULL) && (mFont != NULL))
  {
    mFont->FaceSize(size);
    glPushMatrix();
    glTranslated (here.vec[0], here.vec[1], here.vec[2] + 60.0);
    mFont->Render(tags.name);
    glPopMatrix();
  }
}


void osmRender::RenderNode (unsigned index)
{
  osm::OSMData::Node &node = mOSM->m_nodes[index];
  mgl::Vec3 v;
  Project (node.pos.degLat(), node.pos.degLon(), &v);

  glLineWidth (1.5);
  glBegin (GL_LINES);                           // Trait vertical, pour visibilite
  glVertex3d (v.vec[0], v.vec[1], v.vec[2]);
  glVertex3d (v.vec[0], v.vec[1], v.vec[2] + 30.0);
  glEnd();

  glPointSize (2.5);
  glBegin (GL_POINTS);
  glVertex3d (v.vec[0], v.vec[1], v.vec[2] + 30.0);
  glEnd();

  RenderName (v, node.tags(), 8);

  mVertices += 3;
}


void osmRender::RenderWay (unsigned index)
{
  // Si ce Way a deja ete trace depuis une Relation, ne pas recommencer
  if (mWdone[index]) return;

  const osm::OSMData::Way &way = mOSM->m_ways[index];
  if (way.nodesIx.size() == 0) return;

  switch (way.tags().kind)
  {
    case osm::Tags::unknown :
      if (! way.isLoop())
      {
        Material (1.0, 0.0, 0.0, 25.0);
        RenderWayLine (way);
      }
      else
      {
        Material (0.76, 0.80, 0.76, 25.0);         // A peu pres la couleur de fond, verdatre
        RenderWayArea (way);
      }
    break;

    case osm::Tags::building :          // En principe on a tags().isLoop
      Material (0.6f, 0.6f, 0.6f, 25.0);
      RenderWayExtruded (way, 15.0);    // TODO: comment connaitre sa hauteur ?
    break;

    case osm::Tags::highway :
      if (way.isLoop())       // Une place peut taggee highway=footway
      {
        Material (0.9f, 0.5f, 0.0f, 25.0);
        RenderWayArea (way);
      }
      else
      {
        Material (1.0f, 0.5f, 0.1f, 25.0);
        RenderWayStrip (way, 5.0);
      }
    break;

    case osm::Tags::waterway :
      Material (0.0f, 0.0f, 1.0f, 25.0);     // Blue (but the Seine is brown ...)
      RenderWayStrip (way, 10.0);
    break;

    case osm::Tags::railway :
      Material (0.5f, 0.1f, 0.7f, 25.0);
//    RenderWayStrip (way, 3.0);
      glEnable(GL_LINE_STIPPLE);
      glLineStipple (1, 0xF0F0);
      glLineWidth (2.0);
      RenderWayLine (way);
      glDisable(GL_LINE_STIPPLE);
    break;

    default: assert(false);
  }

  if (true)
  {
    osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[0]];
    mgl::Vec3 v;
    Project (p.pos.degLat(), p.pos.degLon(), &v);
    RenderName (v, way.tags(), 12);
  }
}

void osmRender::RenderWayLine (const osm::OSMData::Way &way)
{
  if (way.nodesIx.size() <= 1) return;
  const GLdouble layer = way.tags().layer;

  // Une simple ligne brisee : pour les LoD faibles
  glLineWidth (1.0);
  glBegin (GL_LINE_STRIP);
  for (unsigned n = 0; n < way.nodesIx.size(); ++n)
  {
    osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[n]];
    mgl::Vec3 v;
    Project (p.pos.degLat(), p.pos.degLon(), &v);
    glVertex3d (v.vec[0], v.vec[1], v.vec[2]+layer);
    ++mVertices;
  }
  glEnd();
}

void osmRender::RenderWayArea (const osm::OSMData::Way &way)
{
  if (way.nodesIx.size() <= 2) return;
  if (! way.isLoop()) return;
  const GLdouble layer = way.tags().layer - 500.0;

//return;       // En fait assez nuisible ... regler layer

  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_POLYGON_STIPPLE);
//glPolygonStipple(pattern);
  glBegin (GL_POLYGON);
  for (unsigned n = 0; n < way.nodesIx.size(); ++n)
  {
    osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[n]];
    mgl::Vec3 v;
    Project (p.pos.degLat(), p.pos.degLon(), &v);
    glVertex3d (v.vec[0], v.vec[1], v.vec[2]+layer);
    ++mVertices;
  }
  glEnd();
  glDisable(GL_POLYGON_STIPPLE);
}


//
//
void osmRender::RenderWayStrip (const osm::OSMData::Way &way, GLdouble width)
{
  if (way.nodesIx.size() <= 1) return;
  const GLdouble layer = way.tags().layer;

#if 0
  // La voie simple : une LINE_STRIP, mais de taille large
  // + Mais alors le taille de la route sur l'ecran est fixe en nombre de pixels, quelle que
  //   soit la distance d'observation.
  glLineWidth (width);
  glBegin (GL_LINE_STRIP);
  for (unsigned n = 0; n < way.nodesIx.size(); ++n)
  {
    osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[n]];
    mgl::Vec3 v;
    Project (p.pos.degLat(), p.pos.degLon(), &v);
    glVertex3d (v.vec[0], v.vec[1], v.vec[2]+layer);
    ++mVertices;
  }
  glEnd();
#elif 0
  // La voie fatigante comme un ensemble de QUADS
  // + Il faut les faire un peut deborder de part et d'autre de leurs extremites pour assurer
  //   de la continuite, mais ce faisant lors des coudes serres il y a des pointes qui depassent

  mgl::Vec3 curr, prev, v;
  v.vec[0] = v.vec[1] = 0.0; // Ceci fait plaisir au compilateur sur le risque de non-init
 
  osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[0]];
  Project (p.pos.degLat(), p.pos.degLon(), &prev);

  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  glBegin (GL_QUADS);
//glNormal3d (0.0, 0.0, 1.0);
  for (unsigned n = 1; n < way.nodesIx.size(); ++n)
  {
    osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[n]];
    Project (p.pos.degLat(), p.pos.degLon(), &curr);

    // OSM est une carte 2D ... pour trouver la direction en largeur du Way
    // une rotation dans l'horizontale. Ce sera faux avec SRTM ?
    // v est la rotation de +90° du vecteur prev->curr, de longueur width/2 :
    v.vec[0] = curr.vec[1] - prev.vec[1];
    v.vec[1] = prev.vec[0] - curr.vec[0];
    double length = 2.0 * sqrt (v.vec[0]*v.vec[0] + v.vec[1]*v.vec[1]) / width;
    v.vec[0] /= length;
    v.vec[1] /= length;

    // Extend avec rot(v,+90°)
    glVertex3d (prev.vec[0]-v.vec[0]+v.vec[1], prev.vec[1]-v.vec[1]-v.vec[0], prev.vec[2]+layer);
    glVertex3d (prev.vec[0]+v.vec[0]+v.vec[1], prev.vec[1]+v.vec[1]-v.vec[0], prev.vec[2]+layer);

    // Extend avec rot(v,-90°)
    glVertex3d (curr.vec[0]+v.vec[0]-v.vec[1], curr.vec[1]+v.vec[1]+v.vec[0], curr.vec[2]+layer);
    glVertex3d (curr.vec[0]-v.vec[0]-v.vec[1], curr.vec[1]-v.vec[1]+v.vec[0], curr.vec[2]+layer);

    prev = curr;
  }
  mVertices += 2 * way.nodesIx.size();
#else
  // La voie fatigante comme un ensemble de TRIANGLE
  // + Pour faire comme des QUAD mais un une petite excroissance triangulaire, approchant un
  //   "bout rond"

  mgl::Vec3 curr, prev;
  GLdouble x,y;
 
  osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[0]];
  Project (p.pos.degLat(), p.pos.degLon(), &prev);

  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);   // normal=FILL debug=LINE

//glNormal3d (0.0, 0.0, 1.0);
  for (unsigned n = 1; n < way.nodesIx.size(); ++n)
  {
    osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[n]];
    Project (p.pos.degLat(), p.pos.degLon(), &curr);

    // (x,y) est le vecteur 2D parallele a l'axe du segment courant, de longueur width/2 :
    x = curr.vec[0] - prev.vec[0];
    y = curr.vec[1] - prev.vec[1];
    const double length = 2.0 * sqrt (x*x + y*y) / width;
    x /= length;
    y /= length;

    glBegin (GL_TRIANGLE_STRIP);        // Un QUAD avec un triangle a chaque extremete
    {
      // Sur l'axe central, un peu en avant le point de depart
      glVertex3d (prev.vec[0]-x, prev.vec[1]-y, prev.vec[2]+layer);
      // A gauche du point de depart
      glVertex3d (prev.vec[0]-y, prev.vec[1]+x, prev.vec[2]+layer);
      // A droite du point de depart
      glVertex3d (prev.vec[0]+y, prev.vec[1]-x, prev.vec[2]+layer);
      // A gauche du point d'arrivee
      glVertex3d (curr.vec[0]-y, curr.vec[1]+x, curr.vec[2]+layer);
      // A droite du point d'arrivee
      glVertex3d (curr.vec[0]+y, curr.vec[1]-x, curr.vec[2]+layer);
      // Sur l'axe central, un peu en apres le point d'arrivee
      glVertex3d (curr.vec[0]+x, curr.vec[1]+y, curr.vec[2]+layer);
    }
    glEnd();

    prev = curr;
  }
#endif
  glEnd();
}


// Juste les murs et le toit, pas de plancher
//
void osmRender::RenderWayExtruded (const osm::OSMData::Way &way, GLdouble height)
{
  if (way.nodesIx.size() <= 3) return;
  const GLdouble layer = way.tags().layer;

  mgl::Vec3 grnd[way.nodesIx.size()];             // Should be small
 
  // Walls
  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  glBegin (GL_QUAD_STRIP);
  for (unsigned n = 0; n < way.nodesIx.size(); ++n)
  {
    osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[n]];
    Project (p.pos.degLat(), p.pos.degLon(), &grnd[n]);
    grnd[n].vec[2] += layer;
    glVertex3d (grnd[n].vec[0], grnd[n].vec[1], grnd[n].vec[2]);
    glVertex3d (grnd[n].vec[0], grnd[n].vec[1], grnd[n].vec[2]+height);
  }
  glVertex3d (grnd[0].vec[0], grnd[0].vec[1], grnd[0].vec[2]);
  glVertex3d (grnd[0].vec[0], grnd[0].vec[1], grnd[0].vec[2]+height);
  glEnd();

  // Roof
  glBegin (GL_POLYGON);
//glNormal3d (0.0, 0.0, 1.0);
  for (unsigned n = 0; n < way.nodesIx.size(); ++n)
    glVertex3d (grnd[n].vec[0], grnd[n].vec[1], grnd[n].vec[2]+height);
  glVertex3d (grnd[0].vec[0], grnd[0].vec[1], grnd[0].vec[2]+height);
  glEnd();

  // Edges
  glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
  glLineWidth (1.5);
  glColor3f (0.0, 0.0, 0.0);
  glBegin (GL_QUAD_STRIP);
  for (unsigned n = 0; n < way.nodesIx.size(); ++n)
  {
    glVertex3d (grnd[n].vec[0], grnd[n].vec[1], grnd[n].vec[2]);
    glVertex3d (grnd[n].vec[0], grnd[n].vec[1], grnd[n].vec[2]+height);
  }
  glVertex3d (grnd[0].vec[0], grnd[0].vec[1], grnd[0].vec[2]);
  glVertex3d (grnd[0].vec[0], grnd[0].vec[1], grnd[0].vec[2]+height);
  glEnd();
 
  mVertices += 4 * (way.nodesIx.size()+1);
}


void osmRender::RenderRelation (unsigned index, unsigned guard)
{
  // Not too deep please
  if (++guard > 10) return;

  osm::OSMData::Relation &relation = mOSM->m_relations[index];

  // Render each member of relation
  for (unsigned m = 0; m < relation.eltIx.size(); ++m)
  {
    switch (relation.eltIx[m].elt)
    {
      case osm::eltNode :
      {
        RenderNode (relation.eltIx[m].ix);
      }
      break;

      case osm::eltWay :
      {
        RenderWay (relation.eltIx[m].ix);

        // Noter que ce Way est deja trace comme element d'un Relation
        mWdone[relation.eltIx[m].ix] = true;
      }
      break;

      case osm::eltRelation :
      {
        RenderRelation (relation.eltIx[m].ix, guard); 
      }
      break;
    }
  }

//if (guard == 0)
//{
//  osm::OSMData::Node &p = mOSM->m_nodes[way.nodesIx[0]];
//  mgl::Vec3 v;
//  Project (p.pos.degLat(), p.pos.degLon(), &v);
//  RenderName (v, node.tags(), 72);
//}
}

