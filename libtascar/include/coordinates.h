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

#include <math.h>
#include <string>
#include <libxml++/libxml++.h>
#include <map>
#include <limits>
#include "defs.h"
#include <stdint.h>

#include "windows_port.h"

/// Avoid de-normals by flipping to zero
template<class T> void make_friendly_number(T& x)
{
  if( (-std::numeric_limits<T>::max() <= x) && (x <= std::numeric_limits<T>::max() ) ){
    if( (0 < x) && (x < std::numeric_limits<T>::min()) )
      x = 0;
    if( (0 > x) && (x > -std::numeric_limits<T>::min()) )
      x = 0;
    return;
  }
  x = 0;
}

/// Avoid de-normals and huge values by flipping to zero
template<class T> void make_friendly_number_limited(T& x)
{
  if( (-1000000 <= x) && (x <= 1000000 ) ){
    if( (0 < x) && (x < std::numeric_limits<T>::min()) )
      x = 0;
    if( (0 > x) && (x > -std::numeric_limits<T>::min()) )
      x = 0;
    return;
  }
  x = 0;
}


namespace TASCAR {

  /// Generate random number between 0 and 1
  double drand();

  /**
     \brief Linear interpolation table
   */
  class table1_t : public std::map<double,double> {
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
    void set_cart(double nx,double ny,double nz){ x=nx;y=ny;z=nz;};
    /**
       \brief Set point from spherical coordinates
       \param r radius from center
       \param phi azimuth
       \param theta elevation
    */
    void set_sphere(double r,double phi,double theta){ x=r*cos(phi)*cos(theta);y = r*sin(phi)*cos(theta); z=r*sin(theta);};
    /// squared norm of vector
    inline double norm2() const {return std::max(1e-10,x*x + y*y + z*z);};
    /// Eucledian norm
    inline double norm() const {return sqrt(norm2());};
    /// Eucledian norm of projection to x-y plane
    inline double norm_xy() const {return sqrt(x*x+y*y);};
    /// Azimuth in radians
    inline double azim() const {return atan2(y,x);};
    /// Elevation in radians
    inline double elev() const {return atan2(z,norm_xy());};
    /// Test if zero all dimensions
    inline bool is_null() const {return (x==0) && (y==0) && (z==0);};
    /// Test if larger than zero in all dimension
    inline bool has_volume() const {return (x>0) && (y>0) && (z>0);};
    /// Return normalized vector
    inline pos_t normal() const {
      pos_t r(*this);
      double n(1.0/norm());
      r.x *= n;
      r.y *= n;
      r.z *= n;
      return r;
    };
    /// Box volume:
    double boxvolume() const { return x*y*z; };
    /// Box area:
    double boxarea() const { return 2.0*(x*y + x*z + y*z);};
      /// Normalize vector
    void normalize();
    /**
       \brief Rotate around z-axis
    */
    inline pos_t& rot_z(double a) { 
      if( a != 0){
        // cos -sin  0
        // sin  cos  0
        //  0    0   1
        double xn = cos(a)*x - sin(a)*y;
        double yn = cos(a)*y + sin(a)*x;
        x = xn;
        y = yn;}
      return *this;
    };
    /**
       \brief Rotate around x-axis
    */
    inline pos_t& rot_x(double a) { 
      if( a != 0){
        // 1   0    0
        // 0  cos -sin
        // 0  sin  cos
        double zn = cos(a)*z + sin(a)*y;
        double yn = cos(a)*y - sin(a)*z;
        z = zn;
        y = yn;
      }
      return *this;
    };
    /**
       \brief Rotate around y-axis
    */
    inline pos_t& rot_y(double a) {
      if( a != 0){
        // cos 0 sin
        //  0  1   0
        // -sin 0  cos
        double xn = cos(a)*x + sin(a)*z;
        double zn = cos(a)*z - sin(a)*x;
        z = zn;
        x = xn;
      }
      return *this;
    };
    /**
       \brief Default constructor, initialize to origin
    */
    pos_t() : x(0),y(0),z(0) {};
    /**
       \brief Initialize to cartesian coordinates
    */
    pos_t(double nx, double ny, double nz) : x(nx),y(ny),z(nz) {};
    /**
       \brief Format as string in cartesian coordinates
    */
    std::string print_cart(const std::string& delim=", ") const;
    /**
       \brief Format as string in spherical coordinates
    */
    std::string print_sphere(const std::string& delim=", ") const;
    /**
       \brief Check for infinity in any of the elements
    */
    bool has_infinity() const;
  };

  /// Spherical coordinates
  class sphere_t {
  public:
    sphere_t(double r_,double az_, double el_):r(r_),az(az_),el(el_){};
    sphere_t():r(0),az(0),el(0){};
    /// Convert from cartesian coordinates
    sphere_t(pos_t c){
      double xy2 = c.x*c.x+c.y*c.y;
      r = sqrt(xy2+c.z*c.z);
      az = atan2(c.y,c.x);
      el = atan2(c.z,sqrt(xy2));
    };
    /// Convert to cartesian coordinates
    pos_t cart(){
      double cel(cos(el));
      return pos_t(r*cos(az)*cel,r*sin(az)*cel,r*sin(el));
    };
    double r,az,el;
  };

  /**
     \brief Scale relative to origin
     \param self Input data
     \param d ratio
  */
  inline TASCAR::sphere_t& operator*=(TASCAR::sphere_t& self,double d) {
    self.r*=d;
    self.az*=d;
    self.el*=d;
    return self;
  };

  /// ZYX Euler angles
  class zyx_euler_t {
  public:
    std::string print(const std::string& delim=", ");
    zyx_euler_t(double z_,double y_,double x_):z(z_),y(y_),x(x_){};
    zyx_euler_t():z(0),y(0),x(0){};
    /// rotation around z-axis in radians
    double z;
    /// rotation around y-axis in radians
    double y;
    /// rotation around x-axis in radians
    double x;
  };

  inline TASCAR::zyx_euler_t& operator*=(TASCAR::zyx_euler_t& self,const double& scale){
    self.x*=scale;
    self.y*=scale;
    self.z*=scale;
    return self;
  };

  inline TASCAR::zyx_euler_t& operator+=(TASCAR::zyx_euler_t& self,const TASCAR::zyx_euler_t& other){
    // \todo this is not correct; it only works for single-axis rotations. 
    self.x+=other.x;
    self.y+=other.y;
    self.z+=other.z;
    return self;
  };

  inline TASCAR::zyx_euler_t& operator-=(TASCAR::zyx_euler_t& self,const TASCAR::zyx_euler_t& other){
    // \todo this is not correct; it only works for single-axis rotations. 
    self.x-=other.x;
    self.y-=other.y;
    self.z-=other.z;
    return self;
  };

  /**
     \brief Apply Euler rotation
     \param r Euler rotation
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator*=(TASCAR::pos_t& self,const TASCAR::zyx_euler_t& r){
    self.rot_z(r.z);
    self.rot_y(r.y);
    self.rot_x(r.x);
    return self;
  };

  /**
     \brief Apply inverse Euler rotation
     \param r Euler rotation
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator/=(TASCAR::pos_t& self,const TASCAR::zyx_euler_t& r){
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
  inline TASCAR::pos_t& operator+=(TASCAR::pos_t& self,const TASCAR::pos_t& p) {
    self.x+=p.x;
    self.y+=p.y;
    self.z+=p.z;
    return self;
  };
  /**
     \brief Return sum of vectors
     \param a Vector
     \param b Vector
     \return Sum
  */
  inline TASCAR::pos_t operator+(const TASCAR::pos_t& a,const TASCAR::pos_t& b) {
    pos_t tmp(a);
    tmp+=b;
    return tmp;
  };
  /**
     \brief Translate a point
     \param p Inverse offset
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator-=(TASCAR::pos_t& self,const TASCAR::pos_t& p) {
    self.x-=p.x;
    self.y-=p.y;
    self.z-=p.z;
    return self;
  };
  /**
     \brief Return difference between vectors
     \param a minuend
     \param b subtrahend
     \return Difference
  */
  inline TASCAR::pos_t operator-(const TASCAR::pos_t& a,const TASCAR::pos_t& b) {
    pos_t tmp(a);
    tmp-=b;
    return tmp;
  };
  /**
     \brief Scale relative to origin
     \param d inverse ratio
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator/=(TASCAR::pos_t& self,double d) {
    self.x/=d;
    self.y/=d;
    self.z/=d;
    return self;
  };
  /**
     \brief Scale relative to origin
     \param d ratio
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator*=(TASCAR::pos_t& self,double d) {
    self.x*=d;
    self.y*=d;
    self.z*=d;
    return self;
  };

  /**
     \brief Scale relative to origin, each axis separately
     \param d ratio
     \param self modified Cartesian coordinates
  */
  inline TASCAR::pos_t& operator*=(TASCAR::pos_t& self,const TASCAR::pos_t& d) {
    self.x*=d.x;
    self.y*=d.y;
    self.z*=d.z;
    return self;
  };

  /**
     \brief Return distance between two points
  */
  inline double distance(const TASCAR::pos_t& p1, const TASCAR::pos_t& p2)
  {
    return sqrt((p1.x-p2.x)*(p1.x-p2.x) + 
                (p1.y-p2.y)*(p1.y-p2.y) + 
                (p1.z-p2.z)*(p1.z-p2.z));
  }

  /// Dot product of two vectors
  inline double dot_prod(const TASCAR::pos_t& p1, const TASCAR::pos_t& p2)
  {
    return p1.x*p2.x+p1.y*p2.y+p1.z*p2.z;
  }

  /// Vector multiplication of two vectors
  inline TASCAR::pos_t cross_prod(const TASCAR::pos_t& a,const TASCAR::pos_t& b)
  {
    return pos_t(a.y*b.z - a.z*b.y,
                 a.z*b.x - a.x*b.z,
                 a.x*b.y - a.y*b.x);
  }


  /**
     \ingroup tascar
     \brief Trajectory (list of points connected with a time)
  */
  class track_t : public std::map<double,TASCAR::pos_t> {
  public:
    /// Interpolation mode
    enum interp_t {
      cartesian, spherical
    };
    track_t();
    /**
       \brief Return the center of a track.
    */
    TASCAR::pos_t center();
    /**
       \brief Return length of a track.
     */
    double length();
    /**
       \brief minimum time
     */
    double t_min(){if( size() ) return begin()->first; else return 0; };
    double t_max(){if( size() ) return rbegin()->first; else return 0; };
    double duration(){return t_max()-t_min();};
    /**
       \brief Return the interpolated position for a given time.
    */
    TASCAR::pos_t interp(double x) const;
    /**
       \brief Shift the time by a constant value
    */
    void shift_time(double dt);
    TASCAR::track_t& operator+=(const TASCAR::pos_t&);
    TASCAR::track_t& operator-=(const TASCAR::pos_t&);
    TASCAR::track_t& operator*=(const TASCAR::pos_t&);
    /**
       \brief Format as string, return velocity
    */
    std::string print_velocity(const std::string& delim=", ");
    /**
       \brief Format as string in cartesian coordinates
    */
    std::string print_cart(const std::string& delim=", ");
    /**
       \brief Format as string in spherical coordinates
    */
    std::string print_sphere(const std::string& delim=", ");
    /**
       \brief Tangent projection, transform origin to given point
    */
    void project_tangent(TASCAR::pos_t p);
    /**
       \brief Tangent projection, transform origin to center
    */
    void project_tangent();
    /**
       \brief Rotate around z-axis
    */
    void rot_z(double a);
    /**
       \brief Rotate around x-axis
    */
    void rot_x(double a);
    /**
       \brief Rotate around y-axis
    */
    void rot_y(double a);
    /**
       \brief Smooth a track by convolution with a Hann-window
    */
    void smooth(unsigned int n);
    /**
       \brief Resample trajectory with equal time sampling
       \param dt New period time
     */
    void resample(double dt);
    /**
       \brief load a track from a gpx file
    */
    void load_from_gpx(const std::string& fname);
    /**
       \brief load a track from a csv file
    */
    void load_from_csv(const std::string& fname);
    /**
       \brief manipulate track based on a set of XML entries
    */
    void edit( xmlpp::Element* m );
    /**
       \brief manipulate track based on a set of XML entries
    */
    void edit( xmlpp::Node::NodeList cmds );
    /**
       \brief set constant velocity
    */
    void set_velocity_const( double vel );
    /**
       \brief set velocity from CSV file
    */
    void set_velocity_csvfile( const std::string& fname, double offset );
    /**
       \brief Export to xml element
    */
    void write_xml( xmlpp::Element* );
    /// Read trajectroy from XML element, using "creator" features
    void read_xml( xmlpp::Element* );
    /// Set interpolation type
    void set_interpt(interp_t p){interpt=p;};
    /// Convert time to travel length
    double get_dist( double time ) const;
    /// Convert travel length to time
    double get_time( double dist ) const;
    /// Update internal data
    void prepare();
    void fill_gaps( double dt );
    /// Loop time
    double loop;
  private:
    interp_t interpt;
    table1_t time_dist;
    table1_t dist_time;
  };

  /**
     \brief Read a single track point from an XML trkpt element
  */
  pos_t xml_get_trkpt( xmlpp::Element* pt, time_t& tme );

  std::string xml_get_text( xmlpp::Node* n, const std::string& child );

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
    pos_t nearest_on_edge(const pos_t& p0,uint32_t* pk0=NULL) const;
    /**
       \brief Return nearest point on polygon
    */
    pos_t nearest(const pos_t& p0, bool* is_outside=NULL, pos_t* on_edge_=NULL) const;
    /**
       \brief Return intersection point of connection line p0-p1 with infinite plane.

       \param p0 Starting point of intersecting edge
       \param p1 End point of intersecting edge
       \param p_is Intersection point
       \param w Optional pointer on intersecting weight, or NULL. w=0 means that the intersection is at p0, w=1 means that intersection is at p1.
       
       \return True if the line p0-p1 is intersecting with the plane, and false otherwise.
    */
    bool intersection( const pos_t& p0, const pos_t& p1, pos_t& p_is, double* w=NULL) const;
    const std::vector<pos_t>& get_verts() const { return verts_;};
    const std::vector<pos_t>& get_edges() const { return edges_;};
    const std::vector<pos_t>& get_vert_normals() const { return vert_normals_;};
    const std::vector<pos_t>& get_edge_normals() const { return edge_normals_;};
    const pos_t& get_normal() const { return normal;};
    double get_area() const { return area;};
    double get_aperture() const { return aperture; };
    std::string print(const std::string& delim=", ") const;
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
  pos_t edge_nearest(const pos_t& v,const pos_t& d,const pos_t& p0);
  
  /**
     \ingroup tascar
     \brief List of Euler rotations connected with a time line.
  */
  class euler_track_t : public std::map<double,zyx_euler_t> {
  public:
    /**
       \brief Return the interpolated orientation for a given time.
    */
    euler_track_t();
    zyx_euler_t interp(double x) const;
    void write_xml( xmlpp::Element* );
    void read_xml( xmlpp::Element* );
    std::string print(const std::string& delim=", ");
    double loop;
  };

  class shoebox_t {
  public:
    shoebox_t();
    shoebox_t(const pos_t& center_,const pos_t& size_,const zyx_euler_t& orientation_);
    pos_t nextpoint(pos_t p);
    double volume() const { return size.boxvolume();};
    double area() const { return size.boxarea();};
    pos_t center;
    pos_t size;
    zyx_euler_t orientation;
  };

  class c6dof_t {
  public:
    c6dof_t() {};
    c6dof_t(const pos_t& psrc,const zyx_euler_t& osrc) : position(psrc), orientation(osrc) {};
    pos_t position;
    zyx_euler_t orientation;
  };

  class quickhull_t {
  public:
    struct simplex_t {
      size_t c1, c2, c3;
    };
    quickhull_t( const std::vector<pos_t>& pos );
    std::vector<simplex_t> faces;
  };

  std::vector<pos_t> generate_icosahedron();

  std::vector<pos_t> subdivide_and_normalize_mesh( std::vector<pos_t> mesh, uint32_t iterations );

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
      x = axis.x;
      y = axis.y;
      z = axis.z;
    };
    inline void set_euler(const zyx_euler_t& eul)
    {
      // Abbreviations for the various angular functions
      float cy = cosf(eul.z * 0.5);
      float sy = sinf(eul.z * 0.5);
      float cp = cosf(eul.y * 0.5);
      float sp = sinf(eul.y * 0.5);
      float cr = cosf(eul.x * 0.5);
      float sr = sinf(eul.x * 0.5);
      w = cr * cp * cy + sr * sp * sy;
      x = sr * cp * cy - cr * sp * sy;
      y = cr * sp * cy + sr * cp * sy;
      z = cr * cp * sy - sr * sp * cy;
    };
    inline quaternion_t inverse() const
    {
      return conjugate().scale(1.0 / norm());
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
    inline quaternion_t& operator*=(const quaternion_t& q)
    {
      quaternion_t tmp(*this * q);
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
    inline zyx_euler_t to_euler() const
    {
      zyx_euler_t eul;
      // x-axis rotation
      float sinr_cosp(2.0f * (w * x + y * z));
      float cosr_cosp(1.0f - 2.0f * (x * x + y * y));
      eul.x = atan2f(sinr_cosp, cosr_cosp);
      // y-axis rotation
      float sinp(2.0f * (w * y - z * x));
      if(fabsf(sinp) >= 1.0f)
        eul.y = copysignf(0.5 * M_PI, sinp); // use 90 degrees if out of range
      else
        eul.y = asinf(sinp);
      // yaw (z-axis rotation)
      float siny_cosp(2.0f * (w * z + x * y));
      float cosy_cosp(1.0f - 2.0f * (y * y + z * z));
      eul.z = atan2f(siny_cosp, cosy_cosp);
      return eul;
    };
  };
} // namespace TASCAR

std::ostream& operator<<(std::ostream& out, const TASCAR::pos_t& p);
std::ostream& operator<<(std::ostream& out, const TASCAR::ngon_t& n);

bool operator==(const TASCAR::quickhull_t& h1,const TASCAR::quickhull_t& h2);
bool operator==(const TASCAR::quickhull_t::simplex_t& s1,const TASCAR::quickhull_t::simplex_t& s2);



#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
