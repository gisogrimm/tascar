/**
 * @file   dynamicobjects.h
 * @author Giso Grimm
 *
 * @brief  Base class for objects with trajectories
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
#ifndef DYNAMICOBJECTS_H
#define DYNAMICOBJECTS_H

#include "tscconfig.h"

namespace TASCAR {

  class navmesh_t : public xml_element_t {
  public:
    navmesh_t(tsccfg::node_t);
    ~navmesh_t();
    void update_pos(TASCAR::pos_t& p);

  private:
    std::vector<TASCAR::ngon_t*> mesh;
    double maxstep;
    double zshift;
  };

  /**
     \ingroup tascar
     \brief Trajectory (list of points connected with a time)
  */
  class track_t : public std::map<double, TASCAR::pos_t> {
  public:
    /// Interpolation mode
    enum interp_t { cartesian, spherical };
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
    double t_min()
    {
      if(size())
        return begin()->first;
      else
        return 0;
    };
    double t_max()
    {
      if(size())
        return rbegin()->first;
      else
        return 0;
    };
    double duration() { return t_max() - t_min(); };
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
    std::string print_velocity(const std::string& delim = ", ");
    /**
       \brief Format as string in cartesian coordinates
    */
    std::string print_cart(const std::string& delim = ", ");
    /**
       \brief Format as string in spherical coordinates
    */
    std::string print_sphere(const std::string& delim = ", ");
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
    void edit(tsccfg::node_t m);
    /**
       \brief set constant velocity
    */
    void set_velocity_const(double vel);
    /**
       \brief set velocity from CSV file
    */
    void set_velocity_csvfile(const std::string& fname, double offset);
    /**
       \brief Export to xml element
    */
    void write_xml(tsccfg::node_t);
    /// Read trajectroy from XML element, using "creator" features
    void read_xml(tsccfg::node_t);
    /// Set interpolation type
    void set_interpt(interp_t p) { interpt = p; };
    /// Convert time to travel length
    double get_dist(double time) const;
    /// Convert travel length to time
    double get_time(double dist) const;
    /// Update internal data
    void prepare();
    void fill_gaps(double dt);
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
  pos_t xml_get_trkpt(tsccfg::node_t pt, time_t& tme);

  /**
     \ingroup tascar
     \brief List of Euler rotations connected with a time line.
  */
  class euler_track_t : public std::map<double, zyx_euler_t> {
  public:
    /**
       \brief Return the interpolated orientation for a given time.
    */
    euler_track_t();
    zyx_euler_t interp(double x) const;
    void write_xml(tsccfg::node_t);
    void read_xml(tsccfg::node_t);
    std::string print(const std::string& delim = ", ");
    double loop;
  };

  class dynobject_t : public xml_element_t {
  public:
    dynobject_t(tsccfg::node_t);
    ~dynobject_t();
    virtual void geometry_update(double t);
    pos_t get_location() const;
    rotmat_t get_orientation() const;
    void get_6dof(pos_t&, rotmat_t&) const;
    void get_6dof_prev(pos_t&, rotmat_t&) const;
    void set_parent(dynobject_t* p);
    size_t get_num_descendants() const;
    double starttime;
    double sampledorientation;
    track_t location;
    euler_track_t orientation;
    pos_t dlocation;
    rotmat_t dorientation;
    const c6dof_t& c6dof;
    const c6dof_t& c6dof_nodelta;
    std::string parent;
    dynobject_t* oparent = NULL;
    std::vector<dynobject_t*> children;

  private:
    dynobject_t(const dynobject_t&);
    c6dof_t c6dof_nodelta_;
    c6dof_t c6dof_;
    c6dof_t c6dof_prev;
    tsccfg::node_t xml_location;
    tsccfg::node_t xml_orientation;
    navmesh_t* navmesh;
    pos_t localpos;
  };

} // namespace TASCAR

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
