/// @file  mGL.cpp
/// @brief mini GL wrapper (for displaying OSM / SRTM data)

#ifndef _H_MGL
#define _H_MGL

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

namespace mgl {

/// Maths
/// + Pas si facile de chganer GLdouble par GLfloat : faudrait remplacer aussi les
///   suffixes 'd' par 'f', ex glTranslated() -> glTranslatef(), etc.

//typedef GLfloat Real;
typedef GLdouble Real;

template<class R>
class TVec3
{
public:
  TVec3 () {}
  TVec3 (R x, R y, R z)
  { vec[0] = x; vec[1] = y; vec[2] = z; }

  R vec[3];

  inline TVec3 operator+ (const TVec3 &op) const
  { return TVec3 (vec[0]+op.vec[0], vec[1]+op.vec[1], vec[2]+op.vec[2]); }

};

typedef TVec3<Real> Vec3;


/// "la forme / geometrie d'une chose dessinable"
/// Une telle chose est envoyee avec ses coordonnees propres,
/// une transformation en censee etre active pour placer /orienter cette
/// forme dans la scene

class Renderable
{
public:
  Renderable ();

  void Compile (void);
  void RenderCompiled (void);

  virtual void Render (void) = 0;

private:
  GLuint mList;
  bool mCompiled;
};


class Sphere : public Renderable
{
public:
  Sphere (const Vec3& center, Real radius);
  ~Sphere ();

  void Render (void);

private:
  GLUquadricObj *mQuadobj;
  Vec3 mCenter;
  Real mRadius;
};


/// "un ensemble de choses dessinables"
/// organise au mieux pour la vitesse de rendu (KdTree, etc)
/// 

class Scene
{
  Scene ();

  void Render ();
};





}  // namespace mgl


#endif
