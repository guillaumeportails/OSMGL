/// @file  Geo.h
/// @brief Definitions communes de geodesie.
///
/// Coordonnees spheriques et cartesiennes d'un point du globe.
/// Definition d'un repere (par rapport a un repere canonique).
/// Conversion entre coordonnees, changement de repere.


#ifndef _GEO
#define _GEO

#include "math.h"


/// @brief modele abstrait de reperes/coordonnees.
///
/// La classe Geo represente tout modele de terre.
/// En ce sens c'est une "interface"
/// Chaque classe derivee implemente un modele particulier,
/// comme le standard WGS84 du GPS.

class Geo
{
public:
  //Geo::Geo() { }

  /// Coordonnees spheriques d'un point en surface : latitude,longitude
  struct LL
  {
    double lat, lon;      // Latitude et Longitude      Unit=rad
  };

  /// Coordonnees spheriques d'un point : latitude,longitude,altitude
  struct LLA
  {
    double lat, lon;      // Latitude et Longitude      Unit=rad
    double alt;           // Une reference de hauteur   Unit=m
  };

  /// Coordonnees cartesiennes d'un point, d'un vecteur
  struct XYZ
  {
    double x, y, z;      //                                     Unit=m

    inline double Length (void) const  // Longueur du vecteur   Unit = m
    { const double n = x*x + y*y + z*z; return sqrt (n); }

    inline double Argument (void) const // Angle trigo dans xy  Unit=rad
    { return atan2 (y, x); }            // x=0° y=90°
  };
//static XYZ const xyz_zero = { 0.0, 0.0, 0.0 };

  /// Un changement de repere
  struct TransformXYZ
  {
    struct XYZ O, I,J,K;  // Origine, et trois axes orthonormes
  };

  /// Changement de repere (methode de classe car independant du modele)
  static void Transform (const XYZ &old, const TransformXYZ &t, XYZ *out);


  /// Methodes propres a chaque modele


  /// Conversion Lat,Lon,Alt vers X,Y,Z
  virtual void toXYZ (const LLA &in, XYZ *out) const = 0;

  inline XYZ toXYZ (const LLA &in) const        // La meme
  { XYZ out; toXYZ (in, &out); return out; }

  /// Transformation pour passer dans le repere local au point "here".
  // Ce repere est X = Est  Y = Nord, Z = Zenith ou presque (X^Y exactement)
  // ! Restons loin des poles, c'est mieux.
  virtual void toLocal (const LLA &here, TransformXYZ *out) const = 0;

  /// Distance curviligne entre deux points au sol              Unit=m
  virtual double GroundDistance (const LL &a, const LL &b) const = 0;

  /// Distance curviligne entre deux points au sol, LLA.Alt est ignore
  inline double GroundDistance (const LLA &a, const LLA &b) const
  { LL a2; a2.lat = a.lat; a2.lon = a.lon;
    LL b2; b2.lat = b.lat; b2.lon = b.lon;
    return GroundDistance (a2, b2);
  }

  /// Distance (m) en ligne droite entre deux points
  //  + L'implementation par defaut utilise toXYZ
  double Distance (const LLA &a, const LLA &b) const;

  // Azimut de to par rapport a from
  double Azimuth (const LL &from, const LL &to) const;

  inline double Azimuth (const LLA &from, const LLA &to) const
  { LL a2; a2.lat = from.lat; a2.lon = from.lon;
    LL b2; b2.lat = to.lat;   b2.lon = to.lon;
    return Azimuth (a2, b2);
  }


protected:
  // Pour l'implementation de classes derivees : memoire des sinus et cosinus
  // de latitude et longitude
  struct sincoslatlon { double clat, slat, clon, slon; };
};


// Algebre


// Difference de vecteurs
inline Geo::XYZ operator- (Geo::XYZ l, Geo::XYZ r)
{
  Geo::XYZ const t = { l.x - r.x, l.y - r.y, l.z - r.z };
  return t;
}

// Produit scalaire de vecteurs
inline double operator* (Geo::XYZ l, Geo::XYZ r)
{
  return l.x * r.x + l.y * r.y + l.z * r.z;
}




/// Le modele WGS84 utilise par le GPS

class GeoWGS84 : public Geo
{
public:
  GeoWGS84 ();

  /// Conversion Lat,Lon,Alt vers X,Y,Z
  void toXYZ (const Geo::LLA &in, Geo::XYZ *out) const;

  /// Transformation pour passer dans le repere local au point "here".
  void toLocal (const Geo::LLA &here, Geo::TransformXYZ *out) const;

  /// Distance (Unit=m) curviligne entre deux points au sol
  //  + C'est une approximation par la longueur de l'arc de grand cercle de la
  //    sphére tangente en a avec l'ellipsoide WGS84
  double GroundDistance (const LL &a, const LL &b) const;


private:
  double m_R0;
  void toXYZ (double alt, const Geo::sincoslatlon &sc, Geo::XYZ *out) const;
};


/// Un objet WGS84
extern const GeoWGS84 geoWGS84;


#endif
