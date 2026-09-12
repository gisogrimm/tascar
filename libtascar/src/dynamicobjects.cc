/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 */
/*
 * TASCAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * TASCAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHATABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "dynamicobjects.h"
#include "errorhandling.h"
#include "tascar_os.h"
#include <fstream>
#include <sstream>
#include <string.h>

using namespace TASCAR;

TASCAR::navmesh_t::navmesh_t(tsccfg::node_t xmlsrc)
    : xml_element_t(xmlsrc), maxstep(0.5), zshift(0)
{
  std::string importraw;
  GET_ATTRIBUTE(maxstep, "m", "maximum step height of object");
  GET_ATTRIBUTE(importraw, "", "file name of vertex list");
  GET_ATTRIBUTE(zshift, "m", "shift object vertically");
  if(!importraw.empty()) {
    std::ifstream rawmesh(TASCAR::env_expand(importraw).c_str());
    if(!rawmesh.good())
      throw TASCAR::ErrMsg("Unable to open mesh file \"" +
                           TASCAR::env_expand(importraw) + "\".");
    while(!rawmesh.eof()) {
      std::string meshline;
      getline(rawmesh, meshline, '\n');
      if(!meshline.empty()) {
        TASCAR::ngon_t* p_face(new TASCAR::ngon_t());
        p_face->nonrt_set(TASCAR::str2vecpos(meshline));
        mesh.push_back(p_face);
      }
    }
  }
  std::stringstream txtmesh(tsccfg::node_get_text(xmlsrc, "faces"));
  while(!txtmesh.eof()) {
    std::string meshline;
    getline(txtmesh, meshline, '\n');
    if(!meshline.empty()) {
      TASCAR::ngon_t* p_face(new TASCAR::ngon_t());
      p_face->nonrt_set(TASCAR::str2vecpos(meshline));
      mesh.push_back(p_face);
    }
  }
  for(auto& mp : mesh)
    *mp += TASCAR::pos_t(0, 0, zshift);
}

void TASCAR::navmesh_t::update_pos(TASCAR::pos_t& p)
{
  if(mesh.empty())
    return;
  TASCAR::pos_t pnearest(mesh[0]->nearest(p));
  double dist((p.x - pnearest.x) * (p.x - pnearest.x) +
              (p.y - pnearest.y) * (p.y - pnearest.y) +
              1e-3 * (p.z - pnearest.z) * (p.z - pnearest.z));
  for(auto& mp : mesh) {
    TASCAR::pos_t pnl(mp->nearest(p));
    double ld((p.x - pnl.x) * (p.x - pnl.x) + (p.y - pnl.y) * (p.y - pnl.y) +
              1e-3 * (p.z - pnl.z) * (p.z - pnl.z));
    if((ld < dist) && (pnl.z - p.z <= maxstep)) {
      pnearest = pnl;
      dist = ld;
    }
  }
  p = pnearest;
}

TASCAR::navmesh_t::~navmesh_t()
{
  for(auto& mp : mesh)
    delete mp;
}

TASCAR::dynobject_t::dynobject_t(tsccfg::node_t xmlsrc)
    : xml_element_t(xmlsrc), starttime(0), sampledorientation(0), c6dof(c6dof_),
      c6dof_nodelta(c6dof_nodelta_), xml_location(NULL), xml_orientation(NULL),
      navmesh(NULL)
{
  get_attribute("start", starttime, "s",
                "time when rendering of object starts");
  GET_ATTRIBUTE(sampledorientation, "m",
                "sample orientation by line fit into curve");
  GET_ATTRIBUTE(localpos, "m", "local position");
  GET_ATTRIBUTE(dlocation, "m", "delta location");
  GET_ATTRIBUTE_NOUNIT(dorientation, "delta orientation");
  GET_ATTRIBUTE(parent, "", "Name of parent object from same scene");
  for(auto& sne : tsccfg::node_get_children(e)) {
    if(tsccfg::node_get_name(sne) == "position") {
      xml_location = sne;
      location.read_xml(sne);
    }
    if(tsccfg::node_get_name(sne) == "orientation") {
      xml_orientation = sne;
      orientation.read_xml(sne);
    }
    if(tsccfg::node_get_name(sne) == "creator") {
      for(auto& node : tsccfg::node_get_children(sne))
        location.edit(node);
      TASCAR::track_t::iterator it_old = location.end();
      double old_azim(0);
      double new_azim(0);
      for(TASCAR::track_t::iterator it = location.begin(); it != location.end();
          ++it) {
        if(it_old != location.end()) {
          pos_t p = it->second;
          p -= it_old->second;
          new_azim = p.azim();
          while(new_azim - old_azim > TASCAR_PI)
            new_azim -= TASCAR_2PI;
          while(new_azim - old_azim < -TASCAR_PI)
            new_azim += TASCAR_2PI;
          orientation[it_old->first] = zyx_euler_t(new_azim, 0, 0);
          old_azim = new_azim;
        }

        if((it_old == location.end()) ||
           (TASCAR::distance(it->second, it_old->second) > 0))
          it_old = it;
      }
      double loop(0);
      get_attribute_value(sne, "loop", loop);
      if(loop > 0) {
        location.loop = loop;
        orientation.loop = loop;
      }
    }
    if(tsccfg::node_get_name(sne) == "navmesh") {
      navmesh = new TASCAR::navmesh_t(sne);
    }
  }
  location.prepare();
  geometry_update(0);
  c6dof_prev = c6dof_;
}

void TASCAR::dynobject_t::geometry_update(double time)
{
  // at the end of this function, c_6dof_ is the final position and
  // orientation of this object.
  c6dof_prev = c6dof_;
  // the interpolation time is the 'object time':
  double ltime(time - starttime);
  // get interpolated position from trajectory:
  c6dof_.position = location.interp(ltime);
  // temporary position, used for navigation mesh:
  TASCAR::pos_t ptmp(c6dof_.position);
  // store trajectory in case some methods need position without delta
  // position:
  c6dof_nodelta_.position = c6dof_.position;
  // add delta position:
  c6dof_.position += dlocation;
  // next step is the orientation:
  if(sampledorientation == 0)
    // interpolated orientation from orientation table:
    c6dof_.orientation = orientation.interp(ltime);
  else {
    // alternatively, extract orientation from position trajectory:
    double tp(location.get_time(location.get_dist(ltime) - sampledorientation));
    TASCAR::pos_t pdt(c6dof_nodelta_.position);
    pdt -= location.interp(tp);
    if(sampledorientation < 0)
      pdt *= -1.0;
    c6dof_.orientation.z = pdt.azim();
    c6dof_.orientation.y = pdt.elev();
    c6dof_.orientation.x = 0.0;
  }
  // store orientation without delta orientation:
  c6dof_nodelta_.orientation = c6dof_.orientation;
  // add delta orientation (here: addition of Euler angles!):
  c6dof_.orientation += dorientation;
  if(navmesh) {
    // the object is restricted to a navigation mesh:
    navmesh->update_pos(c6dof_.position);
    dlocation = c6dof_.position;
    dlocation -= ptmp;
  }
  // add local position offset:
  ptmp = localpos;
  ptmp *= c6dof_.orientation;
  c6dof_.position += ptmp;
  if(oparent) {
    c6dof_.position *= oparent->c6dof.orientation;
    c6dof_.position += oparent->c6dof.position;
    c6dof_.orientation += oparent->c6dof.orientation;
  }
}

TASCAR::pos_t TASCAR::dynobject_t::get_location() const
{
  return c6dof_.position;
}

TASCAR::zyx_euler_t TASCAR::dynobject_t::get_orientation() const
{
  return c6dof_.orientation;
}

void TASCAR::dynobject_t::get_6dof(pos_t& p, zyx_euler_t& o) const
{
  p = c6dof_.position;
  o = c6dof_.orientation;
}

void TASCAR::dynobject_t::get_6dof_prev(pos_t& p, zyx_euler_t& o) const
{
  p = c6dof_prev.position;
  o = c6dof_prev.orientation;
}

TASCAR::dynobject_t::~dynobject_t()
{
  if(navmesh)
    delete navmesh;
}

pos_t track_t::center()
{
  pos_t c;
  for(iterator i = begin(); i != end(); ++i) {
    c += i->second;
  }
  if(size())
    c /= size();
  return c;
}

track_t& track_t::operator+=(const pos_t& x)
{
  for(iterator i = begin(); i != end(); ++i) {
    i->second += x;
  }
  return *this;
}

track_t& track_t::operator-=(const pos_t& x)
{
  for(iterator i = begin(); i != end(); ++i) {
    i->second -= x;
  }
  return *this;
}

std::string track_t::print_velocity(const std::string& delim)
{
  std::ostringstream tmp("");
  tmp.precision(12);
  pos_t d;
  double t(0);
  for(iterator i = begin(); i != end(); ++i) {
    if(i != begin())
      tmp << i->first << delim << distance(i->second, d) / (i->first - t)
          << "\n";
    t = i->first;
    d = i->second;
  }
  return tmp.str();
}

std::string track_t::print_cart(const std::string& delim)
{
  std::ostringstream tmp("");
  tmp.precision(12);
  for(iterator i = begin(); i != end(); ++i) {
    tmp << i->first << delim << i->second.print_cart(delim) << "\n";
  }
  return tmp.str();
}

std::string track_t::print_sphere(const std::string& delim)
{
  std::ostringstream tmp("");
  tmp.precision(12);
  for(iterator i = begin(); i != end(); ++i) {
    tmp << i->first << delim << i->second.print_sphere(delim) << "\n";
  }
  return tmp.str();
}

void track_t::project_tangent()
{
  project_tangent(center());
}

void track_t::project_tangent(pos_t c)
{
  rot_z(-c.azim());
  // warning: the sign of next rotation is a work-around for a
  // potential bug in TASCAR rotation:
  rot_y(-(TASCAR_PI2 - c.elev()));
  rot_z(-TASCAR_PI2);
  c.set_cart(0, 0, -c.norm());
  operator+=(c);
  // operator*=(pos_t(1,-1,1));
}

void track_t::rot_z(double a)
{
  for(iterator i = begin(); i != end(); ++i)
    i->second.rot_z(a);
}

void track_t::rot_x(double a)
{
  for(iterator i = begin(); i != end(); ++i)
    i->second.rot_x(a);
}

void track_t::rot_y(double a)
{
  for(iterator i = begin(); i != end(); ++i)
    i->second.rot_y(a);
}

track_t& track_t::operator*=(const pos_t& x)
{
  for(iterator i = begin(); i != end(); ++i) {
    i->second *= x;
  }
  return *this;
}

void track_t::shift_time(double dt)
{
  track_t nt;
  for(iterator i = begin(); i != end(); ++i) {
    nt[i->first + dt] = i->second;
  }
  *this = nt;
  prepare();
}

pos_t track_t::interp(double x) const
{
  if(begin() == end())
    return pos_t();
  if((loop > 0) && (x >= loop))
    x = fmod(x, loop);
  const_iterator lim2 = lower_bound(x);
  if(lim2 == end())
    return rbegin()->second;
  if(lim2 == begin())
    return begin()->second;
  if(lim2->first == x)
    return lim2->second;
  const_iterator lim1 = lim2;
  --lim1;
  if(interpt == track_t::cartesian) {
    // cartesian interpolation:
    pos_t p1(lim1->second);
    pos_t p2(lim2->second);
    double w = (x - lim1->first) / (lim2->first - lim1->first);
    make_friendly_number(w);
    p1 *= (1.0 - w);
    p2 *= w;
    p1 += p2;
    return p1;
  } else {
    // spherical interpolation:
    sphere_t p1(lim1->second);
    sphere_t p2(lim2->second);
    double w = (x - lim1->first) / (lim2->first - lim1->first);
    make_friendly_number(w);
    p1 *= (1.0 - w);
    p2 *= w;
    p1.r += p2.r;
    p1.az += p2.az;
    p1.el += p2.el;
    return p1.cart();
  }
}

void track_t::smooth(unsigned int n)
{
  uint32_t n_in(size());
  track_t sm;
  std::vector<pos_t> vx;
  std::vector<double> vt;
  vx.resize(n_in);
  vt.resize(n_in);
  int32_t n2(n / 2);
  int32_t k(0);
  for(iterator i = begin(); i != end(); ++i) {
    vt[k] = i->first;
    vx[k] = i->second;
    ++k;
  }
  std::vector<double> wnd;
  wnd.resize(n);
  double wsum(0);
  for(k = 0; k < (int32_t)n; k++) {
    wnd[k] = 0.5 - 0.5 * cos(TASCAR_2PI * (k + 1) / (n + 1));
    wsum += wnd[k];
  }
  make_friendly_number(wsum);
  for(k = 0; k < (int32_t)n; ++k) {
    wnd[k] /= wsum;
  }
  for(k = 0; k < (int32_t)n_in; k++) {
    pos_t ps;
    for(int32_t kw = 0; kw < (int32_t)n; ++kw) {
      pos_t p = vx[std::min(std::max(k + kw, n2) - n2, (int32_t)n_in - 1)];
      p *= wnd[kw];
      ps += p;
    }
    sm[vt[k]] = ps;
  }
  *this = sm;
  prepare();
}

pos_t TASCAR::xml_get_trkpt(tsccfg::node_t pt, time_t& tme)
{
  double lat(0);
  get_attribute_value(pt, "lat", lat);
  double lon(0);
  get_attribute_value(pt, "lon", lon);
  std::string stm(tsccfg::node_get_text(pt, "time"));
  struct tm bdtm;
  tme = 0;
  memset(&bdtm, 0, sizeof(bdtm));
  if(TASCAR::strptime(stm.c_str(), "%Y-%m-%dT%T", &bdtm)) {
    tme = mktime(&bdtm);
  }
  std::string selev = tsccfg::node_get_text(pt, "ele");
  double elev = 0;
  if(selev.size())
    elev = atof(selev.c_str());
  TASCAR::pos_t p;
  p.set_sphere(R_EARTH + elev, DEG2RAD * lon, DEG2RAD * lat);
  return p;
}

void track_t::load_from_gpx(const std::string& fname)
{
  double ttinc(0);
  track_t track;
  TASCAR::xml_doc_t doc(TASCAR::env_expand(fname),
                        TASCAR::xml_doc_t::LOAD_FILE);
  for(auto& node : doc.root.get_children("trk"))
    for(auto& segment : tsccfg::node_get_children(node, "trkseg"))
      for(auto& point : tsccfg::node_get_children(segment, "trkpt")) {
        time_t tm;
        pos_t p = xml_get_trkpt(point, tm);
        double ltm(tm);
        if(ltm == 0)
          ltm = ttinc;
        track[ltm] = p;
        ttinc += 1.0;
      }
  *this = track;
  prepare();
}

void track_t::load_from_csv(const std::string& fname)
{
  std::string lfname(TASCAR::env_expand(fname));
  track_t track;
  std::ifstream fh(lfname.c_str());
  if(fh.fail())
    throw TASCAR::ErrMsg("Unable to open track csv file \"" + lfname + "\".");
  std::string v_tm, v_x, v_y, v_z;
  while(!fh.eof()) {
    getline(fh, v_tm, ',');
    getline(fh, v_x, ',');
    getline(fh, v_y, ',');
    getline(fh, v_z);
    if(v_tm.size() && v_x.size() && v_y.size() && v_z.size()) {
      double tm = atof(v_tm.c_str());
      double x = atof(v_x.c_str());
      double y = atof(v_y.c_str());
      double z = atof(v_z.c_str());
      track[tm] = pos_t(x, y, z);
    }
  }
  fh.close();
  *this = track;
  prepare();
}

void track_t::edit(tsccfg::node_t cmd)
{
  if(cmd) {
    std::string scmd(tsccfg::node_get_name(cmd));
    if(scmd == "load") {
      std::string filename(
          TASCAR::env_expand(tsccfg::node_get_attribute_value(cmd, "name")));
      std::string filefmt(tsccfg::node_get_attribute_value(cmd, "format"));
      if(filefmt == "gpx") {
        load_from_gpx(filename);
      } else if(filefmt == "csv") {
        load_from_csv(filename);
      } else {
        DEBUG("invalid file format");
        DEBUG(filefmt);
      }
    } else if(scmd == "save") {
      std::string filename(
          TASCAR::env_expand(tsccfg::node_get_attribute_value(cmd, "name")));
      std::ofstream ofs(filename.c_str());
      ofs << print_cart(",");
    } else if(scmd == "origin") {
      std::string normtype(tsccfg::node_get_attribute_value(cmd, "src"));
      std::string normmode(tsccfg::node_get_attribute_value(cmd, "mode"));
      TASCAR::pos_t orig;
      if(normtype == "center") {
        orig = center();
      } else if(normtype == "trkpt") {
        auto trackptlist(tsccfg::node_get_children(cmd, "trkpt"));
        if(trackptlist.size()) {
          time_t tm;
          orig = xml_get_trkpt(trackptlist[0], tm);
        }
      }
      if(normmode == "tangent") {
        project_tangent(orig);
      } else if(normmode == "translate") {
        *this -= orig;
      }
    } else if(scmd == "addpoints") {
      std::string fmt(tsccfg::node_get_attribute_value(cmd, "format"));
      // TASCAR::pos_t orig;
      if(fmt == "trkpt") {
        double ttinc(0);
        if(rbegin() != rend())
          ttinc = rbegin()->first;
        for(auto& loc : tsccfg::node_get_children(cmd, "trkpt")) {
          time_t tm;
          pos_t p = xml_get_trkpt(loc, tm);
          double ltm(tm);
          if(ltm == 0)
            ltm = ttinc;
          (*this)[ltm] = p;
          ttinc += 1.0;
        }
      }
    } else if(scmd == "velocity") {
      std::string vel(tsccfg::node_get_attribute_value(cmd, "const"));
      if(vel.size()) {
        double v(atof(vel.c_str()));
        set_velocity_const(v);
      }
      std::string vel_fname(
          TASCAR::env_expand(tsccfg::node_get_attribute_value(cmd, "csvfile")));
      std::string s_offset(tsccfg::node_get_attribute_value(cmd, "start"));
      if(vel_fname.size()) {
        double v_offset(0);
        if(s_offset.size())
          v_offset = atof(s_offset.c_str());
        set_velocity_csvfile(vel_fname, v_offset);
      }
    } else if(scmd == "rotate") {
      rot_z(DEG2RAD *
            atof(tsccfg::node_get_attribute_value(cmd, "angle").c_str()));
    } else if(scmd == "scale") {
      TASCAR::pos_t scale(
          atof(tsccfg::node_get_attribute_value(cmd, "x").c_str()),
          atof(tsccfg::node_get_attribute_value(cmd, "y").c_str()),
          atof(tsccfg::node_get_attribute_value(cmd, "z").c_str()));
      *this *= scale;
    } else if(scmd == "translate") {
      TASCAR::pos_t dx(
          atof(tsccfg::node_get_attribute_value(cmd, "x").c_str()),
          atof(tsccfg::node_get_attribute_value(cmd, "y").c_str()),
          atof(tsccfg::node_get_attribute_value(cmd, "z").c_str()));
      *this += dx;
    } else if(scmd == "smooth") {
      unsigned int n = atoi(tsccfg::node_get_attribute_value(cmd, "n").c_str());
      if(n)
        smooth(n);
    } else if(scmd == "resample") {
      double dt = atof(tsccfg::node_get_attribute_value(cmd, "dt").c_str());
      resample(dt);
    } else if(scmd == "trim") {
      prepare();
      double d_start =
          atof(tsccfg::node_get_attribute_value(cmd, "start").c_str());
      double d_end = atof(tsccfg::node_get_attribute_value(cmd, "end").c_str());
      double t_start(get_time(d_start));
      double t_end(get_time(length() - d_end));
      TASCAR::track_t ntrack;
      for(TASCAR::track_t::iterator it = begin(); it != end(); ++it) {
        if((t_start < it->first) && (t_end > it->first)) {
          ntrack[it->first] = it->second;
        }
      }
      ntrack[t_start] = interp(t_start);
      ntrack[t_end] = interp(t_end);
      *this = ntrack;
      prepare();
    } else if(scmd == "time") {
      // scale...
      std::string att_start(tsccfg::node_get_attribute_value(cmd, "start"));
      if(att_start.size()) {
        double starttime = atof(att_start.c_str());
        shift_time(starttime - begin()->first);
      }
      std::string att_scale(tsccfg::node_get_attribute_value(cmd, "scale"));
      if(att_scale.size()) {
        double scaletime = atof(att_scale.c_str());
        TASCAR::track_t ntrack;
        for(TASCAR::track_t::iterator it = begin(); it != end(); ++it) {
          ntrack[scaletime * it->first] = it->second;
        }
        *this = ntrack;
        prepare();
      }
    } else {
      DEBUG(tsccfg::node_get_name(cmd));
    }
  }
  prepare();
}

void track_t::resample(double dt)
{
  if(dt > 0) {
    TASCAR::track_t ntrack;
    double t_begin = begin()->first;
    double t_end = rbegin()->first;
    for(double t = t_begin; t <= t_end; t += dt)
      ntrack[t] = interp(t);
    *this = ntrack;
  }
  prepare();
}

void track_t::set_velocity_const(double v)
{
  if(v != 0) {
    double t0(0);
    TASCAR::pos_t p0;
    if(begin() != end()) {
      t0 = begin()->first;
      p0 = begin()->second;
    }
    TASCAR::track_t ntrack;
    for(TASCAR::track_t::iterator it = begin(); it != end(); ++it) {
      double d = distance(p0, it->second);
      p0 = it->second;
      t0 += d / v;
      ntrack[t0] = p0;
    }
    *this = ntrack;
  }
  prepare();
}

void track_t::set_velocity_csvfile(const std::string& fname_, double offset)
{
  std::string fname(TASCAR::env_expand(fname_));
  std::ifstream fh(fname.c_str());
  if(fh.fail())
    throw TASCAR::ErrMsg("Unable to open velocity csv file \"" + fname + "\".");
  std::string v_tm, v_x;
  track_t vmap;
  while(!fh.eof()) {
    getline(fh, v_tm, ',');
    getline(fh, v_x);
    if(v_tm.size() && v_x.size()) {
      double tm = atof(v_tm.c_str());
      double x = atof(v_x.c_str());
      vmap[tm - offset] = pos_t(x, 0, 0);
    }
  }
  fh.close();
  if(vmap.begin() != vmap.end()) {
    set_velocity_const(1.0);
    double d = 0;
    double dt = 0.5;
    track_t ntrack;
    for(double tm = std::max(0.0, vmap.begin()->first);
        tm <= vmap.rbegin()->first; tm += dt) {
      pos_t pv = vmap.interp(tm);
      d += dt * pv.x;
      ntrack[tm] = interp(d);
    }
    *this = ntrack;
  }
  prepare();
}

double track_t::length()
{
  if(!size())
    return 0;
  double l(0);
  pos_t p = begin()->second;
  for(iterator i = begin(); i != end(); ++i) {
    l += distance(p, i->second);
    p = i->second;
  }
  return l;
}

track_t::track_t() : loop(0), interpt(cartesian) {}

double track_t::get_dist(double time) const
{
  if((loop > 0) && (time > loop))
    time = fmod(time, loop);
  return time_dist.interp(time);
}

double track_t::get_time(double dist) const
{
  return dist_time.interp(dist);
}

void track_t::write_xml(tsccfg::node_t a)
{
  switch(interpt) {
  case TASCAR::track_t::cartesian:
    // a->set_attribute("interpolation","cartesian");
    break;
  case TASCAR::track_t::spherical:
    tsccfg::node_set_attribute(a, "interpolation", "spherical");
    break;
  }
  tsccfg::node_set_text(a, print_cart(" "));
}

void track_t::read_xml(tsccfg::node_t a)
{
  TASCAR::xml_element_t te(a);
  te.GET_ATTRIBUTE(loop, "s",
                   "The value, if greater than 0, specifies the time when this "
                   "position track is repeated from 0");
  // get_attribute_value(a,"loop",loop);
  track_t ntrack;
  ntrack.loop = loop;
  std::string interpolation("cartesian");
  te.GET_ATTRIBUTE(
      interpolation, "",
      "Coordinate system in which positions are linearly interpolated between "
      "given positions. Possible values are cartesian and spherical.");
  if(interpolation == "spherical")
    ntrack.set_interpt(TASCAR::track_t::spherical);
  else if(interpolation != "cartesian")
    throw TASCAR::ErrMsg(
        "Invalid interpolation type, must be either spherical or cartesian.");
  std::string importcsv;
  te.GET_ATTRIBUTE(
      importcsv, "",
      "Read position track from the {\\tt .csv}-file as comma-separated "
      "values.  The file name can contain absolute or relative path.  Relative "
      "paths are relative to the session's {\\tt .tsc}-file. Default: "
      "position track is contained as space-separated text between opening and "
      "closing \\refelem{position} tags.");
  if(!importcsv.empty()) {
    // load track from CSV file:
    std::ifstream fh(importcsv.c_str());
    if(fh.fail() || (!fh.good())) {
      throw TASCAR::ErrMsg("Unable to open track csv file \"" + importcsv +
                           "\".");
    }
    std::string v_tm, v_x, v_y, v_z;
    while(!fh.eof()) {
      getline(fh, v_tm, ',');
      getline(fh, v_x, ',');
      getline(fh, v_y, ',');
      getline(fh, v_z);
      if(v_tm.size() && v_x.size() && v_y.size() && v_z.size()) {
        double tm = atof(v_tm.c_str());
        double x = atof(v_x.c_str());
        double y = atof(v_y.c_str());
        double z = atof(v_z.c_str());
        ntrack[tm] = pos_t(x, y, z);
      }
    }
    fh.close();
  }
  std::stringstream ptxt1(tsccfg::node_get_text(a, ""));
  while(!ptxt1.eof()) {
    std::string meshline;
    getline(ptxt1, meshline, '\n');
    if(!meshline.empty()) {
      double t(-1);
      pos_t p;
      bool notgood(false);
      std::stringstream ptxt(meshline);
      while(ptxt.good()) {
        ptxt >> t;
        if(ptxt.good()) {
          notgood = true;
          ptxt >> p.x;
          if(ptxt.good()) {
            ptxt >> p.y;
            if(ptxt.good()) {
              ptxt >> p.z;
              ntrack[t] = p;
              notgood = false;
            }
          }
        }
      }
      if(notgood)
        TASCAR::add_warning("Invalid characters or unexpected end of line in "
                            "track definition: " +
                            meshline);
    }
  }
  *this = ntrack;
}

void track_t::prepare()
{
  time_dist.clear();
  dist_time.clear();
  if(size()) {
    double l(0);
    pos_t p0(begin()->second);
    for(const_iterator it = begin(); it != end(); ++it) {
      l += distance(it->second, p0);
      time_dist[it->first] = l;
      dist_time[l] = it->first;
      p0 = it->second;
    }
  }
}

euler_track_t::euler_track_t() : loop(0) {}

zyx_euler_t euler_track_t::interp(double x) const
{
  if(begin() == end()) {
    return zyx_euler_t();
  }
  if((loop > 0) && (x >= loop))
    x = fmod(x, loop);
  const_iterator lim2 = lower_bound(x);
  if(lim2 == end())
    return rbegin()->second;
  if(lim2 == begin())
    return begin()->second;
  if(lim2->first == x)
    return lim2->second;
  const_iterator lim1 = lim2;
  --lim1;
  zyx_euler_t p1(lim1->second);
  zyx_euler_t p2(lim2->second);
  double w = (x - lim1->first) / (lim2->first - lim1->first);
  make_friendly_number(w);
  p1 *= (1.0 - w);
  p2 *= w;
  p1 += p2;
  return p1;
}

void euler_track_t::write_xml(tsccfg::node_t a)
{
  tsccfg::node_set_text(a, print(" "));
}

void euler_track_t::read_xml(tsccfg::node_t a)
{
  get_attribute_value(a, "loop", loop);
  euler_track_t ntrack;
  ntrack.loop = loop;
  std::string importcsv(tsccfg::node_get_attribute_value(a, "importcsv"));
  if(!importcsv.empty()) {
    // load track from CSV file:
    std::ifstream fh(importcsv.c_str());
    if(fh.fail() || (!fh.good()))
      throw TASCAR::ErrMsg("Unable to open Euler track csv file \"" +
                           importcsv + "\".");
    std::string v_tm, v_x, v_y, v_z;
    while(!fh.eof()) {
      getline(fh, v_tm, ',');
      getline(fh, v_z, ',');
      getline(fh, v_y, ',');
      getline(fh, v_x);
      if(v_tm.size() && v_x.size() && v_y.size() && v_z.size()) {
        double tm = atof(v_tm.c_str());
        zyx_euler_t p;
        p.x = atof(v_x.c_str());
        p.y = atof(v_y.c_str());
        p.z = atof(v_z.c_str());
        p *= DEG2RAD;
        ntrack[tm] = p;
      }
    }
    fh.close();
  }
  std::stringstream ptxt1(tsccfg::node_get_text(a, ""));
  while(!ptxt1.eof()) {
    std::string meshline;
    getline(ptxt1, meshline, '\n');
    if(!meshline.empty()) {
      double t(-1);
      zyx_euler_t p;
      bool notgood(false);
      std::stringstream ptxt(meshline);
      while(ptxt.good()) {
        ptxt >> t;
        if(ptxt.good()) {
          notgood = true;
          ptxt >> p.z;
          if(ptxt.good()) {
            ptxt >> p.y;
            if(ptxt.good()) {
              ptxt >> p.x;
              p *= DEG2RAD;
              ntrack[t] = p;
              notgood = false;
            }
          }
        }
      }
      if(notgood)
        TASCAR::add_warning("Invalid characters or unexpected end of line in "
                            "orientation definition: " +
                            meshline);
    }
  }
  *this = ntrack;
}

std::string euler_track_t::print(const std::string& delim)
{
  std::ostringstream tmp("");
  tmp.precision(12);
  for(iterator i = begin(); i != end(); ++i) {
    tmp << i->first << delim << i->second.print(delim) << "\n";
  }
  return tmp.str();
}

std::string zyx_euler_t::print(const std::string& delim) const
{
  std::ostringstream tmp("");
  tmp.precision(12);
  tmp << RAD2DEG * z << delim << RAD2DEG * y << delim << RAD2DEG * x;
  return tmp.str();
}

void track_t::fill_gaps(double dt)
{
  if(empty())
    return;
  track_t nt;
  double lt(begin()->first);
  for(iterator i = begin(); i != end(); ++i) {
    nt[i->first] = i->second;
    double tdt(i->first - lt);
    unsigned int n(tdt / dt);
    if(n > 0) {
      double ldt = tdt / n;
      for(double t = lt + ldt; t < i->first; t += ldt)
        nt[t] = interp(t);
    }
    lt = i->first;
  }
  *this = nt;
  prepare();
}

void dynobject_t::set_parent(dynobject_t* p)
{
  if(p == this)
    throw TASCAR::ErrMsg("Cannot set parent to self.");
  // if(p->oparent)
  //  throw TASCAR::ErrMsg("Higher order parenting is currently not
  //  supported.");
  if(p) {
    oparent = p;
    bool found = false;
    for(auto pp : p->children)
      if(pp == this)
        found = true;
    if(!found)
      p->children.push_back(this);
  }
}

size_t dynobject_t::get_num_descendants() const
{
  size_t ret = children.size();
  for(auto ch : children)
    ret += ch->get_num_descendants();
  return ret;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
