
#include "mGL.h"
#include "FTGL/ftgl.h"
#include "OSM.h"
#include "Geo.h"

class osmRender : public mgl::Renderable
{
public:
  osmRender ();

  void Bind (osm::OSMData *osm);

  void Render (void);

  // Stats
  unsigned mVertices;

private:
  osm::OSMData *mOSM;           // OSM rendu
  std::vector<bool> mWdone;     // Pour ne pas tracer un Way deux fois
  const Geo *mGeo;              // mOSM est mappe sur ce Geoide
  Geo::TransformXYZ mTrep;      // La transformation vers le repere local
  FTFont *mFont;

  void RenderNode (unsigned index);
  void RenderWay (unsigned index);
  void RenderRelation (unsigned index, unsigned guard = 0);
  void Project (double degLat, double degLon, mgl::Vec3 *vec3);

  void RenderWayLine (const osm::OSMData::Way &way);
  void RenderWayArea (const osm::OSMData::Way &way);
  void RenderWayExtruded (const osm::OSMData::Way &way, GLdouble height);
  void RenderWayStrip (const osm::OSMData::Way &way, GLdouble width);
  void RenderName (const mgl::Vec3 here, const osm::Tags &tags, int size);
};

