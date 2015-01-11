//
// Geo.cpp
//
//
// Cf par exemple :
//   http://www.ign.fr/telechargement/FAQ
//
// Et http://williams.best.vwh.net/avform.htm :
// The great circle distance d between two points with coordinates {lat1,lon1}
// and {lat2,lon2} is given by:
//      d=acos(sin(lat1)*sin(lat2)+cos(lat1)*cos(lat2)*cos(lon1-lon2))
// A mathematically equivalent formula, which is less subject to rounding error
// for short distances is:
//      d=  2*asin(sqrt(  (sin((lat1-lat2)/2))^2
//                      + cos(lat1)*cos(lat2)*(sin((lon1-lon2)/2))^2))
//

#include "Geo.h"

//=============================================================================
// Commun


// Changement de repere
void Geo::Transform (const XYZ &old, const TransformXYZ &t, XYZ *out)
{
  XYZ const n = old - t.O;

  out->x = t.I * n;
  out->y = t.J * n;
  out->z = t.K * n;
}


// Distance (m) en ligne droite entre deux points
// + Il peut y avoir une implementation plus rapide, et plus precise pour des
//   points proches, suivant le modele : a surcharger par chacun.
double Geo::Distance (const LLA &a, const LLA &b) const
{
  // Trouver le XYZ 
  XYZ xyza, xyzb;
  toXYZ (a, &xyza);
  toXYZ (b, &xyzb);

  // Distance cartesienne
  const XYZ d = xyza - xyzb;
  return d.Length ();
}

// Azimut geo entre deux points (leurs projections au sol)
// Approche pour une sphere
// Indefini si from est proche d'un pole (au pole Nord,
// toutes les directions menent au Sud)
// TODO: Commun Distance, GroundDistance, ... A voir
double Geo::Azimuth (const LL &from, const LL &to) const
{
#if 0
  // Trouver le XYZ 
  XYZ xyza, xyzb;
  toXYZ (from, &xyza);
  toXYZ (to,   &xyzb);

  // Ecart cartesien
  const XYZ d = xyzb - xyza;
  return M_PI/2 - d.Argument ();
#else
  double dl = from.lon - to.lon;
  double y = sin(dl) * cos(to.lat);
  double x = cos(from.lat)*sin(to.lat) - sin(from.lat)*cos(to.lat)*cos(dl);
  return atan2 (y, x);
#endif
}


//=============================================================================
// Le modele WGS84

#define WGS84_A         6378137.0                       // Demi grand axe
#define WGS84_F         (1.0 / 298.257223563)           // Aplatissement

#define WGS84_B         (WGS84_A * (1.0 - WGS84_F))     // Demi petit axe
#define WGS84_A2        (WGS84_A * WGS84_A)
#define WGS84_B2        (WGS84_B * WGS84_B)
#define WGS84_E2        ((WGS84_A2 - WGS84_B2) / WGS84_A2)
#define WGS84_1E2       (1.0 - WGS84_E2)


GeoWGS84::GeoWGS84 ()
{
  m_R0 = WGS84_A * sqrt(WGS84_1E2);
}

void GeoWGS84::toXYZ
  (double alt,
   const Geo::sincoslatlon &sc,
   Geo::XYZ *out) const
{
  double const n    = WGS84_A / sqrt (1.0 - WGS84_E2 * sc.slon*sc.slon);
  double const nhc  = (alt + n) * sc.clat;

  out->x = nhc * sc.clon;
  out->y = nhc * sc.slon;
  out->z = (n * WGS84_1E2 + alt) * sc.slat;
}

void GeoWGS84::toXYZ (const Geo::LLA &in, Geo::XYZ *out) const
{
  // Cette syntaxe n'existe qu'en C !
//struct sincoslatlon sc = { .clat = cos(in.lat), .slat = sin(in.lat),
//                           .clon = cos(in.lon), .slon = sin(in.lon) };
  Geo::sincoslatlon const sc =
  { cos(in.lat), sin(in.lat), cos(in.lon), sin(in.lon) };

  toXYZ (in.alt, sc, out);
}



// Transformation pour passer dans le repere local au point "here"
// Ce repere est X = Est  Y = Nord, Z = Zenith ou presque (X^Y exactement)
// ! Restons loin des poles, c'est mieux.

void GeoWGS84::toLocal (const Geo::LLA &here, Geo::TransformXYZ *out) const
{
  Geo::sincoslatlon const sc =
  { cos(here.lat), sin(here.lat), cos(here.lon), sin(here.lon) };

  toXYZ (here.alt, sc, &out->O);

  out->I.x = -sc.slon;
  out->I.y =  sc.clon;
  out->I.z =  0.0;

  out->J.x = -sc.clon * sc.slat;
  out->J.y = -sc.slon * sc.slat;
  out->J.z =  sc.clat;

  out->K.x =  sc.clon * sc.clat;
  out->K.y =  sc.slon * sc.clat;
  out->K.z =  sc.slat;
}


// Distance (Unit=m) curviligne entre deux points au sol
// + C'est une approximation par la longueur de l'arc de grand cercle de la
//   sphere tangente en a avec l'ellipsoide WGS84
//   Cf http://www.ign.fr/telechargement/FAQ/FAQ11.doc

double GeoWGS84::GroundDistance (const LL &a, const LL &b) const
{
  // Abscisse curviligne AB sur la sphere unite
  double const SLatA = sin (a.lat);
  double const SLatB = sin (b.lat);
  double const CLatA = cos (a.lat);
  double const CLatB = cos (b.lat);
  double const CosD  = cos (b.lon - a.lon);
  double const CosAB = SLatA*SLatB + CLatA*CLatB*CosD;

  // Gerer quelques cas de debordements (pour lesquels l'approximation n'est
  // de toutes facons plus valide)
  double const AB = (CosAB >= 1.0) ? 0.0 : (CosAB <= -1.0) ? M_PI :
                    acos(CosAB);

  // Le rayon de la meilleure sphere locale a A
  double const S2Lat = SLatA * SLatA;
  double R = m_R0 / (1.0 - WGS84_E2 * S2Lat);
  // On peut aussi prendre globalement (2*A + B) / 3 si les points sont
  // eloignes

  return AB * R;
}



const GeoWGS84 geoWGS84;

