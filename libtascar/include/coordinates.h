/**
 * @file coordinates.h
 * @brief Simple numerical geometry library
 * @ingroup libtascar
 * @author Giso Grimm
 * @date 2012
 */
/* License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
 *
 * This program is free software; you can redistribute it and/ or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef COORDINATES_H
#define COORDINATES_H

#include "defs.h"
#include <algorithm>
#include <limits>
#include <map>
#include <math.h>
#include <stdint.h>
#include <string>
#include <vector>

/// Avoid de-normals by flipping to zero
template <class T> void make_friendly_number(T& x)
{
  if((-std::numeric_limits<T>::max() <= x) &&
     (x <= std::numeric_limits<T>::max())) {
    if((0 < x) && (x < std::numeric_limits<T>::min()))
      x = 0;
    if((0 > x) && (x > -std::numeric_limits<T>::min()))
      x = 0;
    return;
  }
  x = 0;
}

/// Avoid de-normals and huge values by flipping to zero
template <class T> void make_friendly_number_limited(T& x)
{
  if((-1000000 <= x) && (x <= 1000000)) {
    if((0 < x) && (x < std::numeric_limits<T>::min()))
      x = 0;
    if((0 > x) && (x > -std::numeric_limits<T>::min()))
      x = 0;
    return;
  }
  x = 0;
}

namespace TASCAR {

  inline bool is_denormal(const double& a)
  {
    return !((a < 1.0) || (a > 0.0));
  }

  inline bool is_denormal(const float& a)
  {
    return !((a < 1.0f) || (a > 0.0f));
  }

  /// Generate random number between 0 and 1
  double drand();

  /// Generate random number between 0 and 1
  float frand();

  /**
     \brief Linear interpolation table
   */
  class table1_t : public std::map<double, double> {
  public:
    table1_t();
    double interp(double) const;
  };

  /**
     \brief Cartesian coordinate vector
  */
  class pos_t {
  public:
    /// x-axis, to the front
    double x;
    /// y-axis, to the left
    double y;
    /// z-axis, to the top
    double z;
    /**
       \brief Set point from cartesian coordinates
       \param nx new x value
       \param ny new y value
       \param nz new z value
    */
    void set_cart(double nx, double ny, double nz)
    {
      x = nx;
      y = ny;
      z = nz;
    };
    /**
       \brief Set point from spherical coordinates
       \param r radius from center
       \param phi azimuth
       \param theta elevation
    */
    void set_sphere(double r, double phi, double theta)
    {
      x = r * cos(phi) * cos(theta);
      y = r * sin(phi) * cos(theta);
      z = r * sin(theta);
    };
    /// squared norm of vector
    inline double norm2() const
    {
      return std::max(1e-10, x * x + y * y + z * z);
    };
    inline float norm2f() const
    {
      return std::max(1e-10f, (float)x * (float)x + (float)y * (float)y +
                                  (float)z * (float)z);
    };
    /// Eucledian norm
    inline double norm() const { return sqrt(norm2()); };
    inline float normf() const { return sqrtf(norm2f()); };
    /// Eucledian norm of projection to x-y plane
    inline double norm_xy() const { return sqrt(x * x + y * y); };
    inline float norm_xyf() const
    {
      return sqrtf((float)x * (float)x + (float)y * (float)y);
    };
    /// Azimuth in radians
    inline double azim() const { return atan2(y, x); };
    /// Elevation in radians
    inline double elev() const { return atan2(z, norm_xy()); };
    inline float elevf() const { return atan2f((float)z, norm_xyf()); };
    /// Test if zero all dimensions
    inline bool is_null() const { return (x == 0) && (y == 0) && (z == 0); };
    /// Test if larger than zero in all dimension
    inline bool has_volume() const { return (x > 0) && (y > 0) && (z > 0); };
    /// Return normalized vector
    inline pos_t normal() const
    {
      pos_t r(*this);
      double n(1.0 / norm());
      r.x *= n;
      r.y *= n;
      r.z *= n;
      return r;
    };
    /// Box volume:
    double boxvolume() const { return x * y * z; };
    float boxvolumef() const { return (float)x * (float)y * (float)z; };
    /// Box area:
    double boxarea() const { return 2.0 * (x * y + x * z + y * z); };
    float boxareaf() const
    {
      return 2.0f *
             ((float)x * (float)y + (float)x * (float)z + (float)y * (float)z);
    };
    /// Normalize vector
    void normalize();
    /**
       \brief Rotate around z-axis
    */
    inline pos_t& rot_z(double a)
    {
      if(a != 0) {
        // cos -sin  0
        // sin  cos  0
        //  0    0   1
        double xn = cos(a) * x - sin(a) * y;
        double yn = cos(a) * y + sin(a) * x;
        x = xn;
        y = yn;
      }
      return *this;
    };
    /**
       \brief Rotate around x-axis
    */
    inline pos_t& rot_x(double a)
    {
      if(a != 0) {
        // 1   0    0
        // 0  cos -sin
        // 0  sin  cos
        double zn = cos(a) * z + sin(a) * y;
        double yn = cos(a) * y - sin(a) * z;
        z = zn;
        y = yn;
      }
      return *this;
    };
    /**
       \brief Rotate around y-axis
    */
    inline pos_t& rot_y(double a)
    {
      if(a != 0) {
        // cos 0 sin
        //  0  1   0
        // -sin 0  cos
        double xn = cos(a) * x + sin(a) * z;
        double zn = cos(a) * z - sin(a) * x;
        z = zn;
        x = xn;
      }
      return *this;
    };
    /**
       \brief Default constructor, initialize to origin
    */
    pos_t() : x(0), y(0), z(0){};
    /**
       \brief Initialize to cartesian coordinates
    */
    pos_t(double nx, double ny, double nz) : x(nx), y(ny), z(nz){};
    /**
       \brief Format as string in cartesian coordinates
    */
    std::string print_cart(const std::string& delim = ", ") const;
    /**
       \brief Format as string in spherical coordinates
    */
    std::string print_sphere(const std::string& delim = ", ") const;
    /**
       \brief Check for infinity in any of the elements
    */
    bool has_infinity() const;
  };

  /**
     \brief Cartesian coordinate vector with single precision resolution
  */
  class posf_t {
  public:
    /// x-axis, to the front
    float x;
    /// y-axis, to the left
    float y;
    /// z-axis, to the top
    float z;
    /**
       \brief Set point from cartesian coordinates
       \param nx new x value
       \param ny new y value
       \param nz new z value
    */
    void set_cart(float nx, float ny, float nz)
    {
      x = nx;
      y = ny;
      z = nz;
    };
    /**
       \brief Set point from spherical coordinates
       \param r radius from center
       \param phi azimuth
       \param theta elevation
    */
    void set_sphere(float r, float phi, float theta)
    {
      x = r * cosf(phi) * cosf(theta);
      y = r * sinf(phi) * cosf(theta);
      z = r * sinf(theta);
    };
    /// squared norm of vector
    inline float norm2() const
    {
      return std::max(1e-10f, x * x + y * y + z * z);
    };
    /// Eucledian norm
    inline float norm() const { return sqrtf(norm2()); };
    /// Eucledian norm of projection to x-y plane
    inline float norm_xy() const { return sqrtf(x * x + y * y); };
    /// Azimuth in radians
    inline float azim() const { return atan2f(y, x); };
    /// Elevation in radians
    inline float elev() const { return atan2f(z, norm_xy()); };
    /// Test if zero all dimensions
    inline bool is_null() const { return (x == 0) && (y == 0) && (z == 0); };
    /// Test if larger than zero in all dimension
    inline bool has_volume() const { return (x > 0) && (y > 0) && (z > 0); };
    /// Return normalized vector
    inline posf_t normal() const
    {
      posf_t r(*this);
      float n(1.0f / norm());
      r.x *= n;
      r.y *= n;
      r.z *= n;
      return r;
    };
    /// Box volume:
    float boxvolume() const { return x * y * z; };
    /// Box area:
    float boxarea() const { return 2.0f * (x * y + x * z + y * z); };
    /// Normalize vector
    void normalize();
    /**
       \brief Rotate around z-axis
    */
    inline posf_t& rot_z(float a)
    {
      if(a != 0) {
        // cos -sin  0
        // sin  cos  0
        //  0    0   1
        float xn = cosf(a) * x - sinf(a) * y;
        float yn = cosf(a) * y + sinf(a) * x;
        x = xn;
        y = yn;
      }
      return *this;
    };
    /**
       \brief Rotate around x-axis
    */
    inline posf_t& rot_x(float a)
    {
      if(a != 0) {
        // 1   0    0
        // 0  cos -sin
        // 0  sin  cos
        float zn = cosf(a) * z + sinf(a) * y;
        float yn = cosf(a) * y - sinf(a) * z;
        z = zn;
        y = yn;
      }
      return *this;
    };
    /**
       \brief Rotate around y-axis
    */
    inline posf_t& rot_y(float a)
    {
      if(a != 0) {
        // cos 0 sin
        //  0  1   0
        // -sin 0  cos
        float xn = cosf(a) * x + sinf(a) * z;
        float zn = cosf(a) * z - sinf(a) * x;
        z = zn;
        x = xn;
      }
      return *this;
    };
    /**
       \brief Default constructor, initialize to origin
    */
    posf_t() : x(0), y(0), z(0){};
    /**
       \brief Initialize to cartesian coordinates
    */
    posf_t(float nx, float ny, float nz) : x(nx), y(ny), z(nz){};
    /**
       \brief Format as string in cartesian coordinates
    */
    std::string print_cart(const std::string& delim = ", ") const;
    /**
       \brief Format as string in spherical coordinates
    */
    std::string print_sphere(const std::string& delim = ", ") const;
    /**
       \brief Check for infinity in any of the elements
    */
    bool has_infinity() const;
  };

  /// Spherical coordinates
  class sphere_t {
  public:
    sphere_t(double r_, double az_, double el_) : r(r_), az(az_), el(el_){};
    sphere_t() : r(0), az(0), el(0){};
    /// Convert from cartesian coordinates
    sphere_t(pos_t c)
    {
      double xy2 = c.x * c.x + c.y * c.y;
      r = sqrt(xy2 + c.z * c.z);
      az = atan2(c.y, c.x);
      el = atan2(c.z, sqrt(xy2));
    };
    /// Convert to cartesian coordinates
    pos_t cart()
    {
      double cel(cos(el));
      return pos_t(r * cos(az) * cel, r * sin(az) * cel, r * sin(el));
    };
    double r, az, el;
  };

  /**
     \brief Scale relative to origin
     \param self Input data
     \param d ratio
  */
  inline TASCAR::sphere_t& operator*=(TASCAR::sphere_t& self, double d)
  {
    self.r *= d;
    self.az *= d;
    self.el *= d;
    return self;
  };

  /// ZYX Euler angles
  class zyx_euler_t {
  public:
    std::string print(const std::string& delim = ", ") const;
    zyx_euler_t(double z_, double y_, double x_) : z(z_), y(y_), x(x_){};
    zyx_euler_t() : z(0), y(0), x(0){};
    /// Return Euler transformation from position vector
    inline void set_from_pos(TASCAR::pos_t pos)
    {
      x = 0.0;
      y = -atan2(pos.z, pos.x);
      pos.rot_y(-y);
      z = atan2(pos.y, pos.x);
    };

    /// rotation around z-axis in radians
    double z;
    /// rotation around y-axis in radians
    double y;
    /// rotation around x-axis in radians
    double x;
  };

  class rotmat_t {
  public:
    rotmat_t(){};
    inline void set_from_euler(const zyx_euler_t& eul)
    {
      const auto cx = cos(eul.x), sx = sin(eul.x);
      const auto cy = cos(eul.y), sy = sin(eul.y);
      const auto cz = cos(eul.z), sz = sin(eul.z);
      const auto cx_cz = cx * cz, cx_sz = cx * sz, sx_cz = sx * cz,
                 sx_sz = sx * sz;
      m11 = cy * cz;
      m12 = -cy * sz;
      m13 = sy;
      m21 = cx_sz + sx * sy * cz;
      m22 = cx_cz - sx * sy * sz;
      m23 = -sx * cy;
      m31 = sx_sz - cx * sy * cz;
      m32 = sx_cz + cx * sy * sz;
      m33 = cx * cy;
    };
    inline zyx_euler_t to_euler() const
    {
      zyx_euler_t eul;
      eul.x = atan2(-m23, m33);
      auto m132 = m13 * m13;
      if(m132 < 0.9999999)
        eul.y = atan2(m13, sqrt(1.0 - m132));
      else
        eul.y = copysign(TASCAR_PI2, m13);
      eul.z = atan2(-m12, m11);
      return eul;
    };
    // Compound assignment operator for matrix multiplication (this *= rhs)
    inline rotmat_t& operator*=(const rotmat_t& rhs)
    {
      // Store current values in temporary variables to ensure correct
      // calculation when writing back to member variables.
      const double t11 = m11 * rhs.m11 + m12 * rhs.m21 + m13 * rhs.m31;
      const double t12 = m11 * rhs.m12 + m12 * rhs.m22 + m13 * rhs.m32;
      const double t13 = m11 * rhs.m13 + m12 * rhs.m23 + m13 * rhs.m33;

      const double t21 = m21 * rhs.m11 + m22 * rhs.m21 + m23 * rhs.m31;
      const double t22 = m21 * rhs.m12 + m22 * rhs.m22 + m23 * rhs.m32;
      const double t23 = m21 * rhs.m13 + m22 * rhs.m23 + m23 * rhs.m33;

      const double t31 = m31 * rhs.m11 + m32 * rhs.m21 + m33 * rhs.m31;
      const double t32 = m31 * rhs.m12 + m32 * rhs.m22 + m33 * rhs.m32;
      const double t33 = m31 * rhs.m13 + m32 * rhs.m23 + m33 * rhs.m33;

      // Assign computed values back to members
      m11 = t11;
      m12 = t12;
      m13 = t13;
      m21 = t21;
      m22 = t22;
      m23 = t23;
      m31 = t31;
      m32 = t32;
      m33 = t33;

      return *this;
    };

    double m11 = 1;
    double m12 = 0;
    double m13 = 0;
    double m21 = 0;
    double m22 = 1;
    double m23 = 0;
    double m31 = 0;
    double m32 = 0;
    double m33 = 1;
  };

  std::string to_string(const rotmat_t& m);

  inline bool operator!=(const TASCAR::pos_t& p1, const TASCAR::pos_t& p2)
  {
    return (p1.x != p2.x) || (p1.y != p2.y) || (p1.z != p2.z);
  }

  inline TASCAR::zyx_euler_t& operator*=(TASCAR::zyx_euler_t& self,
                                         const double& scale)
  {
    self.x *= scale;
    self.y *= scale;
    self.z *= scale;
    return self;
  };

  inline TASCAR::zyx_euler_t& operator+=(TASCAR::zyx_euler_t& self,
                                         const TASCAR::zyx_euler_t& other)
  {
    // \todo this is not correct; it only works for single-axis rotations.
    self.x += other.x;
    self.y += other.y;
    self.z += other.z;
    return self;
  };

  inline TASCAR::zyx_euler_t& operator-=(TASCAR::zyx_euler_t& self,
                                         const TASCAR::zyx_euler_t& other)
  {
    // \todo this is not correct; it only works for single-axis rotations.
    self.x -= other.x;
    self.y -= other.y;
    self.z -= other.z;
    return self;
  };

  /**
     \brief Apply Euler rotation
     \param r Euler rotation
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator*=(TASCAR::pos_t& self,
                                   const TASCAR::zyx_euler_t& r)
  {
    self.rot_z(r.z);
    self.rot_y(r.y);
    self.rot_x(r.x);
    return self;
  };

  inline TASCAR::pos_t& operator*=(TASCAR::pos_t& self,
                                   const TASCAR::rotmat_t& r)
  {
    auto temp = self;
    self.x = temp.x * r.m11 + temp.y * r.m12 + temp.z * r.m13;
    self.y = temp.x * r.m21 + temp.y * r.m22 + temp.z * r.m23;
    self.z = temp.x * r.m31 + temp.y * r.m32 + temp.z * r.m33;
    return self;
  };

  /**
     \brief Apply inverse Euler rotation
     \param r Euler rotation
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator/=(TASCAR::pos_t& self,
                                   const TASCAR::zyx_euler_t& r)
  {
    self.rot_x(-r.x);
    self.rot_y(-r.y);
    self.rot_z(-r.z);
    return self;
  };

  /**
     \brief Translate a point
     \param p Offset
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator+=(TASCAR::pos_t& self, const TASCAR::pos_t& p)
  {
    self.x += p.x;
    self.y += p.y;
    self.z += p.z;
    return self;
  };
  /**
     \brief Return sum of vectors
     \param a Vector
     \param b Vector
     \return Sum
  */
  inline TASCAR::pos_t operator+(const TASCAR::pos_t& a, const TASCAR::pos_t& b)
  {
    pos_t tmp(a);
    tmp += b;
    return tmp;
  };
  /**
     \brief Translate a point
     \param p Inverse offset
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator-=(TASCAR::pos_t& self, const TASCAR::pos_t& p)
  {
    self.x -= p.x;
    self.y -= p.y;
    self.z -= p.z;
    return self;
  };
  /**
     \brief Return difference between vectors
     \param a minuend
     \param b subtrahend
     \return Difference
  */
  inline TASCAR::pos_t operator-(const TASCAR::pos_t& a, const TASCAR::pos_t& b)
  {
    pos_t tmp(a);
    tmp -= b;
    return tmp;
  };
  /**
     \brief Scale relative to origin
     \param d inverse ratio
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator/=(TASCAR::pos_t& self, double d)
  {
    self.x /= d;
    self.y /= d;
    self.z /= d;
    return self;
  };
  /**
     \brief Scale relative to origin
     \param d inverse ratio
     \param self modified Cartesian coordinates
  */
  inline TASCAR::posf_t& operator/=(TASCAR::posf_t& self, float d)
  {
    self.x /= d;
    self.y /= d;
    self.z /= d;
    return self;
  };
  /**
     \brief Scale relative to origin
     \param d ratio
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator*=(TASCAR::pos_t& self, double d)
  {
    self.x *= d;
    self.y *= d;
    self.z *= d;
    return self;
  };
  /**
     \brief Scale relative to origin
     \param d ratio
     \param self modified Cartesian coordinates
  */
  inline TASCAR::posf_t& operator*=(TASCAR::posf_t& self, float d)
  {
    self.x *= d;
    self.y *= d;
    self.z *= d;
    return self;
  };

  /**
     \brief Scale relative to origin, each axis separately
     \param d ratio
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator*=(TASCAR::pos_t& self, const TASCAR::pos_t& d)
  {
    self.x *= d.x;
    self.y *= d.y;
    self.z *= d.z;
    return self;
  };

  /**
     \brief Return distance between two points
  */
  inline double distance(const TASCAR::pos_t& p1, const TASCAR::pos_t& p2)
  {
    return sqrt((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y) +
                (p1.z - p2.z) * (p1.z - p2.z));
  }
  inline float distancef(const TASCAR::pos_t& p1, const TASCAR::pos_t& p2)
  {
    return sqrtf(((float)p1.x - (float)p2.x) * ((float)p1.x - (float)p2.x) +
                 ((float)p1.y - (float)p2.y) * ((float)p1.y - (float)p2.y) +
                 ((float)p1.z - (float)p2.z) * ((float)p1.z - (float)p2.z));
  }

  /// Dot product of two vectors
  inline double dot_prod(const TASCAR::pos_t& p1, const TASCAR::pos_t& p2)
  {
    return p1.x * p2.x + p1.y * p2.y + p1.z * p2.z;
  }

  /// Dot product of two vectors
  inline float dot_prodf(const TASCAR::pos_t& p1, const TASCAR::pos_t& p2)
  {
    return (float)p1.x * (float)p2.x + (float)p1.y * (float)p2.y +
           (float)p1.z * (float)p2.z;
  }

  /// Vector multiplication of two vectors
  inline TASCAR::pos_t cross_prod(const TASCAR::pos_t& a,
                                  const TASCAR::pos_t& b)
  {
    return pos_t(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x);
  }

  /**
     \brief Polygon class for reflectors and obstacles
   */
  class ngon_t {
  public:
    /** \brief Default constructor, initialize to 1x2m rectangle
     */
    ngon_t();
    /**
       \brief Create a polygon from a list of vertices
    */
    void nonrt_set(const std::vector<pos_t>& verts);
    /**
       \brief Create a rectangle

       The vertices are at (0,0,0), (0,w,0), (0,w,h), (0,0,h). The
       face normal is pointing in positive x-axis.
    */
    void nonrt_set_rect(double width, double height);
    void apply_rot_loc(const pos_t& p0, const zyx_euler_t& o);
    bool is_infront(const pos_t& p0) const;
    bool is_behind(const pos_t& p0) const;
    /**
       \brief Return nearest point on infinite plane
    */
    pos_t nearest_on_plane(const pos_t& p0) const;
    /**
       \brief Return nearest point on face boundary

       If the test point is in the middle of the surface, then center
       of the first edge is returned.

       \param p0 Point to be tested
       \param pk0 Edge number
       \return Nearest point on boundary
    */
    pos_t nearest_on_edge(const pos_t& p0, uint32_t* pk0 = NULL) const;
    /**
       \brief Return nearest point on polygon
    */
    pos_t nearest(const pos_t& p0, bool* is_outside = NULL,
                  pos_t* on_edge_ = NULL) const;
    /**
       \brief Return intersection point of connection line p0-p1 with infinite
       plane.

       \param p0 Starting point of intersecting edge
       \param p1 End point of intersecting edge
       \param p_is Intersection point
       \param w Optional pointer on intersecting weight, or NULL. w=0 means that
       the intersection is at p0, w=1 means that intersection is at p1.

       \return True if the line p0-p1 is intersecting with the plane, and false
       otherwise.
    */
    bool intersection(const pos_t& p0, const pos_t& p1, pos_t& p_is,
                      double* w = NULL) const;
    const std::vector<pos_t>& get_verts() const { return verts_; };
    const std::vector<pos_t>& get_edges() const { return edges_; };
    const std::vector<pos_t>& get_vert_normals() const
    {
      return vert_normals_;
    };
    const std::vector<pos_t>& get_edge_normals() const
    {
      return edge_normals_;
    };
    const pos_t& get_normal() const { return normal; };
    double get_area() const { return area; };
    double get_aperture() const { return aperture; };
    std::string print(const std::string& delim = ", ") const;
    ngon_t& operator+=(const pos_t& p);
    ngon_t& operator+=(double p);

  protected:
    /**
       \brief Transform local to global coordinates and update normals.
    */
    void update();
    uint32_t N;
    std::vector<pos_t> local_verts_;
    std::vector<pos_t> verts_;
    std::vector<pos_t> edges_;
    std::vector<pos_t> vert_normals_;
    std::vector<pos_t> edge_normals_;
    zyx_euler_t orientation;
    pos_t delta;
    pos_t normal;
    pos_t local_normal;
    double area;
    double aperture;
  };

  /**
     \brief Find the nearest point between an edge vector from v to d and p0
     \param v Origin of the edge
     \param d Direction of the edge
     \param p0 Test point
     \return Position of the nearest point on the edge
  */
  pos_t edge_nearest(const pos_t& v, const pos_t& d, const pos_t& p0);

  class shoebox_t {
  public:
    shoebox_t();
    shoebox_t(const pos_t& center_, const pos_t& size_,
              const zyx_euler_t& orientation_);
    pos_t nextpoint(pos_t p);
    double volume() const { return size.boxvolume(); };
    double area() const { return size.boxarea(); };
    pos_t center;
    pos_t size;
    zyx_euler_t orientation;
  };

  class c6dof_t {
  public:
    c6dof_t(){};
    c6dof_t(const pos_t& psrc, const zyx_euler_t& osrc)
        : position(psrc), orientation(osrc){};
    pos_t position;
    zyx_euler_t orientation;
  };

  class quickhull_t {
  public:
    struct simplex_t {
      size_t c1, c2, c3;
    };
    quickhull_t(const std::vector<pos_t>& pos);
    std::vector<simplex_t> faces;
  };

  std::vector<pos_t> generate_icosahedron();

  std::vector<pos_t> subdivide_and_normalize_mesh(std::vector<pos_t> mesh,
                                                  uint32_t iterations);

  class quaternion_t {
  public:
    quaternion_t() : w(0), x(0), y(0), z(0){};
    quaternion_t(float w_, float x_, float y_, float z_)
        : w(w_), x(x_), y(y_), z(z_){};
    float w;
    float x;
    float y;
    float z;
    void clear()
    {
      w = 0.0;
      x = 0.0;
      y = 0.0;
      z = 0.0;
    };
    inline quaternion_t& operator*=(float b)
    {
      w *= b;
      x *= b;
      y *= b;
      z *= b;
      return *this;
    };
    inline void set_rotation(float angle, TASCAR::pos_t axis)
    {
      axis *= sinf(0.5f * angle);
      w = cosf(0.5f * angle);
      x = (float)(axis.x);
      y = (float)(axis.y);
      z = (float)(axis.z);
    };
    inline void set_rotation(float angle, TASCAR::posf_t axis)
    {
      axis *= sinf(0.5f * angle);
      w = cosf(0.5f * angle);
      x = axis.x;
      y = axis.y;
      z = axis.z;
    };
    inline void set_euler_xyz(const zyx_euler_t& eul)
    {
      // Abbreviations for the various angular functions
      float cy = cosf((float)(eul.z) * 0.5f);
      float sy = sinf((float)(eul.z) * 0.5f);
      float cp = cosf((float)(eul.y) * 0.5f);
      float sp = sinf((float)(eul.y) * 0.5f);
      float cr = cosf((float)(eul.x) * 0.5f);
      float sr = sinf((float)(eul.x) * 0.5f);
      w = cr * cp * cy + sr * sp * sy;
      x = sr * cp * cy - cr * sp * sy;
      y = cr * sp * cy + sr * cp * sy;
      z = cr * cp * sy - sr * sp * cy;
    };
    inline void set_euler_zyx(const zyx_euler_t& eul)
    {
      set_rotation((float)(eul.x), TASCAR::posf_t(1.0f, 0.0f, 0.0f));
      quaternion_t q;
      q.set_rotation((float)(eul.y), TASCAR::posf_t(0.0f, 1.0f, 0.0f));
      rmul(q);
      q.set_rotation((float)(eul.z), TASCAR::posf_t(0.0f, 0.0f, 1.0f));
      rmul(q);
    };
    // inline void set_euler_xyz(const zyx_euler_t& eul)
    //{
    //   set_rotation(eul.z, TASCAR::posf_t(0.0f, 0.0f, 1.0f));
    //   quaternion_t q;
    //   q.set_rotation(eul.y, TASCAR::posf_t(0.0f, 1.0f, 0.0f));
    //   rmul(q);
    //   q.set_rotation(eul.x, TASCAR::posf_t(1.0f, 0.0f, 0.0f));
    //   rmul(q);
    // };
    inline quaternion_t inverse() const
    {
      return conjugate().scale(1.0f / norm());
    };
    inline float norm() const { return w * w + x * x + y * y + z * z; };
    inline quaternion_t conjugate() const
    {
      return quaternion_t(w, -x, -y, -z);
    };
    inline quaternion_t scale(float s) const
    {
      return quaternion_t(w * s, x * s, y * s, z * s);
    };
    inline void rotate(TASCAR::pos_t& p)
    {
      quaternion_t qv(0.0f, (float)(p.x), (float)(p.y), (float)(p.z));
      qv = (*this) * qv * inverse();
      p.x = qv.x;
      p.y = qv.y;
      p.z = qv.z;
    };
    inline void rotate(TASCAR::posf_t& p)
    {
      quaternion_t qv(0.0f, p.x, p.y, p.z);
      qv = (*this) * qv * inverse();
      p.x = qv.x;
      p.y = qv.y;
      p.z = qv.z;
    };
    inline quaternion_t operator*(const quaternion_t& q) const
    {
      return quaternion_t(w * q.w - x * q.x - y * q.y - z * q.z,
                          w * q.x + x * q.w + y * q.z - z * q.y,
                          w * q.y + y * q.w + z * q.x - x * q.z,
                          w * q.z + z * q.w + x * q.y - y * q.x);
    };
    inline quaternion_t& rmul(const quaternion_t& q)
    {
      quaternion_t tmp(*this * q);
      *this = tmp;
      return *this;
    };
    inline quaternion_t& lmul(const quaternion_t& q)
    {
      quaternion_t tmp(q * *this);
      *this = tmp;
      return *this;
    };
    inline quaternion_t operator+(const quaternion_t& q) const
    {
      return quaternion_t(w + q.w, x + q.x, y + q.y, z + q.z);
    };
    inline quaternion_t& operator+=(const quaternion_t& q)
    {
      quaternion_t tmp(*this + q);
      *this = tmp;
      return *this;
    };
    inline zyx_euler_t to_euler_xyz() const
    {
      zyx_euler_t eul;
      // x-axis rotation
      float sinr_cosp(2.0f * (w * x + y * z));
      float cosr_cosp(1.0f - 2.0f * (x * x + y * y));
      eul.x = atan2f(sinr_cosp, cosr_cosp);
      // y-axis rotation
      float sinp(2.0f * (w * y - z * x));
      if(fabsf(sinp) >= 1.0f)
        eul.y = copysignf(TASCAR_PI2f, sinp); // use 90 degrees if out of range
      else
        eul.y = asinf(sinp);
      // yaw (z-axis rotation)
      float siny_cosp(2.0f * (w * z + x * y));
      float cosy_cosp(1.0f - 2.0f * (y * y + z * z));
      eul.z = atan2f(siny_cosp, cosy_cosp);
      return eul;
    };
    inline zyx_euler_t to_euler_zyx() const { return to_rotmat().to_euler(); };
    inline rotmat_t to_rotmat() const
    {
      rotmat_t rm;
      rm.m11 = 2.0 * (w * w + x * x) - 1.0;
      rm.m12 = 2.0 * (x * y - w * z);
      rm.m13 = 2.0 * (x * z + w * y);
      rm.m21 = 2.0 * (x * y + w * z);
      rm.m22 = 2.0 * (w * w + y * y) - 1.0;
      rm.m23 = 2.0 * (y * z - w * x);
      rm.m31 = 2.0 * (x * z - w * y);
      rm.m32 = 2.0 * (y * z + w * x);
      rm.m33 = 2.0 * (w * w + z * z) - 1.0;
      return rm;
    };
  };

  template <class RandAccessIter>
  double median(RandAccessIter begin, RandAccessIter end, double q = 0.5)
  {
    if(begin == end)
      return 0.0;
    q = std::max(q, 0.0);
    std::size_t size = end - begin;
    std::size_t middleIdx = (std::size_t)((double)size * q);
    if(middleIdx >= size)
      middleIdx = size - 1u;
    RandAccessIter target = begin + middleIdx;
    std::nth_element(begin, target, end);
    if(((size & 1) != 0) || (q != 0.5)) { // Odd number of elements
      return *target;
    } else { // Even number of elements
      double a = *target;
      RandAccessIter targetNeighbor = target - 1;
      std::nth_element(begin, targetNeighbor, end);
      return (a + *targetNeighbor) / 2.0;
    }
  }

  void vector_get_mean_std(const std::vector<double>& v, double& mean,
                           double& stdev);
} // namespace TASCAR

std::ostream& operator<<(std::ostream& out, const TASCAR::pos_t& p);
std::ostream& operator<<(std::ostream& out, const TASCAR::ngon_t& n);

inline bool operator==(const TASCAR::pos_t& a, const TASCAR::pos_t& b)
{
  return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
};

bool operator==(const TASCAR::quickhull_t& h1, const TASCAR::quickhull_t& h2);
bool operator==(const TASCAR::quickhull_t::simplex_t& s1,
                const TASCAR::quickhull_t::simplex_t& s2);

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
