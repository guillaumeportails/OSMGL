/// @file  mGL.h
/// @brief mini GL wrapper (for displaying OSM / SRTM data)

#include <stdio.h>

#include <GL/gl.h>
#include <GL/glu.h>     // Private

#include "mGL.h"


namespace mgl {


/// Maths


/// "la forme / geometrie d'une chose dessinable"
/// Une telle chose est envoyee avec ses coordonnees propres,
/// une transformation en censee etre active pour placer /orienter cette
/// forme dans la scene

Renderable::Renderable ()
{
  mCompiled = false;
}

void Renderable::Compile (void)
{
  mList = glGenLists (1);
  glNewList (mList, GL_COMPILE);
  Render();
  glEndList();
  mCompiled = true;
  printf ("List %d compiled\n", mList);
}

void Renderable::RenderCompiled (void)
{
  if (mCompiled)
    glCallList (mList);
  else
    Render();
}


Sphere::Sphere (const Vec3 &center, Real radius)
{
  mQuadobj = gluNewQuadric();
  mCenter = center;
  mRadius = radius;
}

Sphere::~Sphere ()
{
  gluDeleteQuadric (mQuadobj);
}

void Sphere::Render (void)
{
  glPushMatrix();
  glTranslated (mCenter.vec[0], mCenter.vec[1], mCenter.vec[2]);
  gluSphere (mQuadobj, (GLdouble) mRadius, 10, 10);
  glPopMatrix();
};


/// "un ensemble de choses dessinables"
/// organise au mieux pour la vitesse de rendu (KdTree, etc)

Scene::Scene()
{
};



}  // namespace mgl

