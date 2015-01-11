#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

#include "mGL.h"
#include "OSM.h"
#include "osmRender.h"

#include "rusage.h"

static osm::OSMData OSM;
static osmRender OSMgeom;


static float w, h;
static float fov = 45.0;
static float eye[3] = { -900.0, -900.0,  900.0 };
static float ctr[3] = { 0.0, 0.0, 0.0 };
static bool methode = true;
static unsigned moving = 0;
//static osm::eltType quoi = osm::eltNode;



static void init (void)
{
  glClearColor(0.4, 0.4, 0.9, 0.0);     // Nice blue sky today
  glClearColor(0.0, 0.0, 0.0, 0.0);     // At night

#if 0
  glShadeModel (GL_SMOOTH); //FLAT);

  { GLfloat f[]  = { 0.5, 0.5, 0.5, 1.0};
    glLightfv(GL_LIGHT0, GL_AMBIENT, f); }
  { GLfloat f[]  = { 0.5, 0.5, 0.5, 1.0};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, f); }
  { GLfloat f[]  = { 0.9, 0.9, 0.9, 1.0};
    glLightfv(GL_LIGHT0, GL_SPECULAR, f); }
  { GLfloat f[]  = { 0.0, 0.0, 50000.0, 0.0};
    glLightfv(GL_LIGHT0, GL_POSITION, f); }
  glEnable(GL_LIGHT0);

  { GLfloat f[]  = { 0.5, 0.5, 0.5, 1.0};
    glLightfv(GL_LIGHT1, GL_AMBIENT, f); }
  { GLfloat f[]  = { 0.5, 0.5, 0.5, 1.0};
    glLightfv(GL_LIGHT1, GL_DIFFUSE, f); }
  { GLfloat f[]  = { 1800.0,  800.0, 130.0, 0.0};
    glLightfv(GL_LIGHT1, GL_POSITION, f); }
  glEnable(GL_LIGHT1);

  glEnable (GL_LIGHTING);
  glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, 1);
  glEnable (GL_RESCALE_NORMAL);
  glEnable (GL_NORMALIZE);
//glDisable (VERTEX_PROGRAM_TWO_SIDE);

//glColorMaterial (GL_FRONT_AND_BACK, GL_DIFFUSE);
//glEnable (GL_COLOR_MATERIAL);
#endif

#if 0
  glEnable(GL_FOG);
  { GLfloat f[] = { 0.8, 0.8, 0.8, 1.0 }; glFogfv(GL_FOG_COLOR, f); }
  glFogi(GL_FOG_MODE, GL_LINEAR);
  glFogf(GL_FOG_START,  800.0);
  glFogf(GL_FOG_END, 4000.0);
#endif

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LINE_SMOOTH);

//glEnable (GL_BLEND);
//glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//glHint (GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
}

static void setcam (void)
{
//glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (fov,                  // field of view in degree
                   1.0,                 // aspect ratio
                  10.0,                 // Z near
               25000.0);                // Z far
  gluLookAt (eye[0], eye[1], eye[2],    // eye
             ctr[0], ctr[1], ctr[2],    // center
              0.0, 0.0, 1.0);           // up
}

void reshape (int nw, int nh)
{
//printf ("reshape %d %d\n", nw, nh);
  w = nw;
  h = nh;
}

void keyboard (unsigned char k, int x, int y)
{
//printf ("keyboard %c %d %d\n", k,x,y);
  static GLdouble s = 10.0;
  switch (k)
  {
    case 27  : exit(1);         // Il n'y a rien pour sortir de GLUT ...

    case 'X' : s *= 1.01; break;
    case 'x' : s /= 1.01; break;

    case 'k' : eye[0] -= s; ctr[0] -= s; break;
    case 'K' : eye[0] += s; ctr[0] += s; break;
    case 'l' : eye[1] -= s; ctr[1] -= s; break;
    case 'L' : eye[1] += s; ctr[1] += s; break;
    case 'm' : eye[2] -= s; ctr[2] -= s; break;
    case 'M' : eye[2] += s; ctr[2] += s; break;

    case 'i' : ctr[0] -= s; break;
    case 'I' : ctr[0] += s; break;
    case 'o' : ctr[1] -= s; break;
    case 'O' : ctr[1] += s; break;
    case 'p' : ctr[2] -= s; break;
    case 'P' : ctr[2] += s; break;

    case 'w' : fov *= 1.01; break;
    case 'W' : fov /= 1.01; break;

//  case 'q' : quoi = osm::eltNode; break;
//  case 's' : quoi = osm::eltWay; break;
//  case 'd' : quoi = osm::eltRelation; break;

    case 'a' : methode = ! methode; break;
    case 'z' : moving = (moving) ? 0 : 1; break;
    case 'Z' : moving = (moving) ? 0 : 2; break;
  }
  setcam();
  glutPostRedisplay();
}

void idle (void)
{
  static GLdouble s = 1.0;
  if (moving == 1)
  {
    eye[0] += s; ctr[0] += s;
    eye[1] += s; ctr[1] += s;
    setcam();
    glutPostRedisplay();
  }
  else if (moving == 2)
  {
    eye[0] -= s; ctr[0] -= s;
    eye[1] -= s; ctr[1] -= s;
    setcam();
    glutPostRedisplay();
  }
}


void render_ground (void)
{
  glColor3f (0.1f, 0.6f, 0.1f);
  glBegin(GL_LINES);
  GLdouble q = 1000.0;
  for (int i = -10; i <= 10; ++i)
  {
    GLdouble w = (GLdouble) i * 100.0;
    glVertex3d (-q, w, -1.0); glVertex3d ( q, w, -1.0);
    glVertex3d (w, -q, -1.0); glVertex3d (w,  q, -1.0);
  }
  glEnd();
}


void display ()
{
//printf ("display index=%u\n", index);

  static struct timeval prev;
  static int frames = 0;
  struct timeval curr;
  gettimeofday(&curr, NULL);
  double dur =   (double) (curr.tv_sec  - prev.tv_sec)
               + (double) (curr.tv_usec - prev.tv_usec)/1.0e6;
  ++frames;
  if (dur > 2.0)
  {
    printf ("%.1f FPS\n", (double) frames / dur);
    frames = 0;
    prev = curr;
  }

  glViewport(0, 0, w, h);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//render_ground();

  if (methode)
    OSMgeom.RenderCompiled();
  else
    OSMgeom.Render();

#if 0
  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
  {
    { GLfloat f[4] = { 0.9, 0.0, 0.0, 1.0 }; glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, f); }
    { GLfloat f[4] = { 0.0, 0.9, 0.0, 1.0 }; glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, f); }
    { GLfloat f[4] = { 0.0, 0.9, 0.0, 1.0 }; glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, f); }
    glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 25.0);
    glPushMatrix();
    glTranslatef(1000.0, 2000.0, 30.0);
    glutSolidTorus (30.0, 100.0, 8, 8);
    glPopMatrix();
  }
  {
    { GLfloat f[4] = { 0.0, 0.0, 0.9, 1.0 }; glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, f); }
    { GLfloat f[4] = { 0.0, 0.9, 0.0, 1.0 }; glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, f); }
    glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 75.0);
    glPushMatrix();
    glTranslatef(2000.0, 1000.0, 60.0);
    glutSolidSphere (30.0, 8, 8);
    glPopMatrix();
  }
#endif

  glutSwapBuffers();
}


static void onexit (void)
{
  print_rusage();
}


int main(int argc, char **argv)
{
  atexit (onexit);
  glutInit(&argc, argv);

  glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
  glutCreateWindow("OSM rendering test");
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboard);
  glutIdleFunc(idle);
  init();
  setcam();

  print_rusage();
  OSM.LoadText (argv[1]);  //("/c/GIS/Aravis.OSM");
  print_rusage();

  OSMgeom.Bind (&OSM);
  printf ("%u node, %u way, %u relation\n",
      OSM.m_nodes.size(), OSM.m_ways.size(), OSM.m_relations.size());
  OSMgeom.Compile();
  printf ("%u vertices\n", OSMgeom.mVertices);

  glutMainLoop();
  print_rusage();
  return 0;
}


