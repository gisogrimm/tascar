/**
 * @file coordinates.cc
 * @brief "coordinates" provide classes for coordinate handling
 * @ingroup libtascar
 * @author Giso Grimm
 * @date 2012
 *
 * @section license License (GPL)
 *
 * This program is free software; you can redistribute it and/or
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
#include "coordinates.h"
#include <sstream>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include "errorhandling.h"
#include "xmlconfig.h"
#include <unistd.h>

#include "windows_port.h"

#define QUICKHULL_1

#ifdef QUICKHULL_1
#include "quickhull/QuickHull.hpp"
#include "quickhull/QuickHull.cpp"
using namespace quickhull;
#else
#define CONVHULL_3D_ENABLE
#include "convhull_3d/convhull_3d.h"
#endif

using namespace TASCAR;

double TASCAR::drand()
{
  return (double)random()/(double)(RAND_MAX+1.0);
}

std::string pos_t::print_cart(const std::string& delim) const
{
  std::ostringstream tmp("");
  tmp.precision(12);
  tmp << x << delim << y << delim << z;
  return tmp.str();

}

std::string pos_t::print_sphere(const std::string& delim) const
{
  std::ostringstream tmp("");
  tmp.precision(12);
  tmp << norm() << delim << RAD2DEG*azim() << delim << RAD2DEG*elev();
  return tmp.str();

}

pos_t track_t::center()
{
  pos_t c;
  for(iterator i=begin();i!=end();++i){
    c += i->second;
  }
  if( size() )
    c /= size();
  return c;
}

track_t& track_t::operator+=(const pos_t& x)
{
  for(iterator i=begin();i!=end();++i){
    i->second += x;
  }
  return *this;
}

track_t& track_t::operator-=(const pos_t& x)
{
  for(iterator i=begin();i!=end();++i){
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
  for(iterator i=begin();i!=end();++i){
    if( i!=begin() )
      tmp << i->first << delim << distance(i->second,d)/(i->first-t) << "\n";
    t = i->first;
    d = i->second;
  }
  return tmp.str();
}
  
std::string track_t::print_cart(const std::string& delim)
{
  std::ostringstream tmp("");
  tmp.precision(12);
  for(iterator i=begin();i!=end();++i){
    tmp << i->first << delim << i->second.print_cart(delim) << "\n";
  }
  return tmp.str();
}
  
std::string track_t::print_sphere(const std::string& delim)
{
  std::ostringstream tmp("");
  tmp.precision(12);
  for(iterator i=begin();i!=end();++i){
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
  rot_z( -c.azim() );
  // warning: the sign of next rotation is a work-around for a
  // potential bug in TASCAR rotation:
  rot_y( -(PI_2 - c.elev()) );
  rot_z( -PI_2 );
  c.set_cart(0,0,-c.norm());
  operator+=(c);
  //operator*=(pos_t(1,-1,1));
}

void track_t::rot_z(double a)
{
  for(iterator i=begin();i!=end();++i)
    i->second.rot_z(a);
}

void track_t::rot_x(double a)
{
  for(iterator i=begin();i!=end();++i)
    i->second.rot_x(a);
}

void track_t::rot_y(double a)
{
  for(iterator i=begin();i!=end();++i)
    i->second.rot_y(a);
}


track_t& track_t::operator*=(const pos_t& x)
{
  for(iterator i=begin();i!=end();++i){
    i->second *= x;
  }
  return *this;
}

void track_t::shift_time(double dt)
{
  track_t nt;
  for(iterator i=begin();i!=end();++i){
    nt[i->first+dt] = i->second;
  }
  *this = nt;
  prepare();
}

pos_t track_t::interp(double x) const
{
  if( begin() == end() )
    return pos_t();
  if( (loop > 0) && (x >= loop) )
    x = fmod(x,loop);
  const_iterator lim2 = lower_bound(x);
  if( lim2 == end() )
    return rbegin()->second;
  if( lim2 == begin() )
    return begin()->second;
  if( lim2->first == x )
    return lim2->second;
  const_iterator lim1 = lim2;
  --lim1;
  if( interpt == track_t::cartesian ){
    // cartesian interpolation:
    pos_t p1(lim1->second);
    pos_t p2(lim2->second);
    double w = (x-lim1->first)/(lim2->first-lim1->first);
    make_friendly_number(w);
    p1 *= (1.0-w);
    p2 *= w;
    p1 += p2;
    return p1;
  }else{
    // spherical interpolation:
    sphere_t p1(lim1->second);
    sphere_t p2(lim2->second);
    double w = (x-lim1->first)/(lim2->first-lim1->first);
    make_friendly_number(w);
    p1 *= (1.0-w);
    p2 *= w;
    p1.r += p2.r;
    p1.az += p2.az;
    p1.el += p2.el;
    return p1.cart();
  }
}

void track_t::smooth( unsigned int n )
{
  uint32_t n_in(size());
  track_t sm;
  std::vector<pos_t> vx;
  std::vector<double> vt;
  vx.resize(n_in);
  vt.resize(n_in);
  int32_t n2(n/2);
  int32_t k(0);
  for(iterator i=begin();i!=end();++i){
    vt[k] = i->first;
    vx[k] = i->second;
    ++k;
  }
  std::vector<double> wnd;
  wnd.resize(n);
  double wsum(0);
  for(k = 0;k<(int32_t)n;k++){
    wnd[k] = 0.5 - 0.5*cos(PI2*(k+1)/(n+1));
    wsum+=wnd[k];
  }
  make_friendly_number(wsum);
  for(k = 0;k<(int32_t)n;++k){
    wnd[k]/=wsum;
  }
  for( k=0;k<(int32_t)n_in;k++ ){
    pos_t ps;
    for( int32_t kw=0;kw<(int32_t)n;++kw){
      pos_t p = vx[std::min(std::max(k+kw,n2)-n2,(int32_t)n_in-1)];
      p *= wnd[kw];
      ps += p;
    }
    sm[vt[k]] = ps;
  }
  *this = sm;
  prepare();
}  

std::string TASCAR::xml_get_text( xmlpp::Node* n, const std::string& child )
{
  if( n ){
    if( child.size() ){
      xmlpp::Node::NodeList ch = n->get_children( child );
      if( ch.size() ){
        xmlpp::NodeSet stxt = (*ch.begin())->find("text()");
        xmlpp::TextNode* txt = dynamic_cast<xmlpp::TextNode*>(*(stxt.begin()));
        if( txt ){
          return txt->get_content();
        }
      }
    }else{
      xmlpp::NodeSet stxt = n->find("text()");
      if( stxt.begin() != stxt.end() ){
        xmlpp::TextNode* txt = dynamic_cast<xmlpp::TextNode*>(*(stxt.begin()));
        if( txt ){
          return txt->get_content();
        }
      }
    }
  }
  return "";
}


pos_t TASCAR::xml_get_trkpt( xmlpp::Element* pt, time_t& tme )
{
  double lat = atof(pt->get_attribute_value("lat").c_str());
  double lon = atof(pt->get_attribute_value("lon").c_str());
  std::string stm = xml_get_text( pt, "time" );
  struct tm bdtm;
  tme = 0;
  memset(&bdtm,0,sizeof(bdtm));
  if( strptime(stm.c_str(),"%Y-%m-%dT%T",&bdtm) ){
    tme = mktime( &bdtm );
  }
  std::string selev = xml_get_text( pt, "ele" );
  double elev = 0;
  if( selev.size() )
    elev = atof(selev.c_str());
  TASCAR::pos_t p;
  p.set_sphere(R_EARTH+elev,DEG2RAD*lon,DEG2RAD*lat);
  return p;
}

void track_t::load_from_gpx( const std::string& fname )
{
  double ttinc(0);
  track_t track;
  xmlpp::DomParser parser(TASCAR::env_expand(fname));
  xmlpp::Element* root = parser.get_document()->get_root_node();
  if( root ){
    xmlpp::Node::NodeList xmltrack = root->get_children("trk");
    if( xmltrack.size() ){
      xmlpp::Node::NodeList trackseg = (*(xmltrack.begin()))->get_children("trkseg");
      if( trackseg.size() ){
        xmlpp::Node::NodeList trackpt = (*(trackseg.begin()))->get_children("trkpt");
        for(xmlpp::Node::NodeList::iterator itrackpt=trackpt.begin();itrackpt!=trackpt.end();++itrackpt){
          xmlpp::Element* loc = dynamic_cast<xmlpp::Element*>(*itrackpt);
          if( loc ){
            time_t tm;
            pos_t p = xml_get_trkpt( loc, tm );
            double ltm(tm);
            if( ltm == 0 )
              ltm = ttinc;
            track[ltm] = p;
            ttinc += 1.0;
          }
        }
      }
    }
  }
  *this = track;
  prepare();
}

void track_t::load_from_csv( const std::string& fname )
{
  std::string lfname(TASCAR::env_expand(fname));
  track_t track;
  std::ifstream fh(lfname.c_str());
  if( fh.fail() )
    throw TASCAR::ErrMsg("Unable to open track csv file \""+lfname+"\".");
  std::string v_tm, v_x, v_y, v_z;
  while( !fh.eof() ){
    getline(fh,v_tm,',');
    getline(fh,v_x,',');
    getline(fh,v_y,',');
    getline(fh,v_z);
    if( v_tm.size() && v_x.size() && v_y.size() && v_z.size() ){
      double tm = atof(v_tm.c_str());
      double x = atof(v_x.c_str());
      double y = atof(v_y.c_str());
      double z = atof(v_z.c_str());
      track[tm] = pos_t(x,y,z);
    }
  }
  fh.close();
  *this = track;
  prepare();
}

void track_t::edit( xmlpp::Element* cmd )
{
  if( cmd ){
    std::string scmd(cmd->get_name());
    if( scmd == "load" ){
      std::string filename = TASCAR::env_expand(cmd->get_attribute_value("name"));
      std::string filefmt = cmd->get_attribute_value("format");
      if( filefmt == "gpx" ){
        load_from_gpx(filename);
      }else if( filefmt == "csv" ){
        load_from_csv(filename);
      }else{
        DEBUG("invalid file format");
        DEBUG(filefmt);
      }
    }else if( scmd == "save" ){
      std::string filename = TASCAR::env_expand(cmd->get_attribute_value("name"));
      std::ofstream ofs(filename.c_str());
      ofs << print_cart(",");
    }else if( scmd == "origin" ){
      std::string normtype = cmd->get_attribute_value("src");
      std::string normmode = cmd->get_attribute_value("mode");
      TASCAR::pos_t orig;
      if( normtype == "center" ){
        orig = center();
      }else if( normtype == "trkpt" ){
        xmlpp::Node::NodeList trackpt = cmd->get_children("trkpt");
        xmlpp::Element* loc = dynamic_cast<xmlpp::Element*>(*(trackpt.begin()));
        if( loc ){
          time_t tm;
          orig = xml_get_trkpt( loc, tm );
        }
      }
      if( normmode == "tangent" ){
        project_tangent( orig );
      }else if( normmode == "translate" ){
        *this -= orig;
      }
    }else if( scmd == "addpoints" ){
      std::string fmt = cmd->get_attribute_value("format");
      //TASCAR::pos_t orig;
      if( fmt == "trkpt" ){
        double ttinc(0);
        if( rbegin() != rend() )
          ttinc = rbegin()->first;
        xmlpp::Node::NodeList trackpt = cmd->get_children("trkpt");
        for(xmlpp::Node::NodeList::iterator itrackpt=trackpt.begin();itrackpt!=trackpt.end();++itrackpt){
          xmlpp::Element* loc = dynamic_cast<xmlpp::Element*>(*itrackpt);
          if( loc ){
            time_t tm;
            pos_t p = xml_get_trkpt( loc, tm );
            double ltm(tm);
            if( ltm == 0 )
              ltm = ttinc;
            (*this)[ltm] = p;
            ttinc += 1.0;
          }
        }
      }
    }else if( scmd == "velocity" ){
      std::string vel(cmd->get_attribute_value("const"));
      if( vel.size() ){
        double v(atof(vel.c_str()));
        set_velocity_const( v );
      }
      std::string vel_fname(TASCAR::env_expand(cmd->get_attribute_value("csvfile")));
      std::string s_offset(cmd->get_attribute_value("start"));
      if( vel_fname.size() ){
        double v_offset(0);
        if( s_offset.size() )
          v_offset = atof(s_offset.c_str());
        set_velocity_csvfile( vel_fname, v_offset );
      }
    }else if( scmd == "rotate" ){
      rot_z(DEG2RAD*atof(cmd->get_attribute_value("angle").c_str()));
    }else if( scmd == "scale" ){
      TASCAR::pos_t scale(atof(cmd->get_attribute_value("x").c_str()),
                          atof(cmd->get_attribute_value("y").c_str()),
                          atof(cmd->get_attribute_value("z").c_str()));
      *this *= scale;
    }else if( scmd == "translate" ){
      TASCAR::pos_t dx(atof(cmd->get_attribute_value("x").c_str()),
                       atof(cmd->get_attribute_value("y").c_str()),
                       atof(cmd->get_attribute_value("z").c_str()));
      *this += dx;
    }else if( scmd == "smooth" ){
      unsigned int n = atoi(cmd->get_attribute_value("n").c_str());
      if( n )
        smooth( n );
    }else if( scmd == "resample" ){
      double dt = atof(cmd->get_attribute_value("dt").c_str());
      resample(dt);
    }else if( scmd == "trim" ){
      prepare();
      double d_start = atof(cmd->get_attribute_value("start").c_str());
      double d_end = atof(cmd->get_attribute_value("end").c_str());
      double t_start(get_time(d_start));
      double t_end(get_time(length()-d_end));
      TASCAR::track_t ntrack;
      for( TASCAR::track_t::iterator it=begin();it != end(); ++it){
        if( (t_start < it->first) && (t_end > it->first) ){
          ntrack[it->first] = it->second;
        }
      }
      ntrack[t_start] = interp(t_start);
      ntrack[t_end] = interp(t_end);
      *this = ntrack;
      prepare();
    }else if( scmd == "time" ){
      // scale...
      std::string att_start(cmd->get_attribute_value("start"));
      if( att_start.size() ){
        double starttime = atof(att_start.c_str());
        shift_time(starttime - begin()->first);
      }
      std::string att_scale(cmd->get_attribute_value("scale"));
      if( att_scale.size() ){
        double scaletime = atof(att_scale.c_str());
        TASCAR::track_t ntrack;
        for( TASCAR::track_t::iterator it=begin();it != end(); ++it){
          ntrack[scaletime*it->first] = it->second;
        }
        *this = ntrack;
        prepare();
      }
    }else{
      DEBUG(cmd->get_name());
    }
  }
  prepare();
}

void track_t::resample( double dt )
{
  if( dt > 0 ){
    TASCAR::track_t ntrack;
    double t_begin = begin()->first;
    double t_end = rbegin()->first;
    for( double t = t_begin; t <= t_end; t+=dt )
      ntrack[t] = interp(t);
    *this = ntrack;
  }
  prepare();
}

void track_t::edit( xmlpp::Node::NodeList cmds )
{
  for( xmlpp::Node::NodeList::iterator icmd=cmds.begin();icmd != cmds.end();++icmd){
    xmlpp::Element* cmd = dynamic_cast<xmlpp::Element*>(*icmd);
    edit( cmd );
  }
  prepare();
}

void track_t::set_velocity_const( double v )
{
  if( v != 0 ){
    double t0(0);
    TASCAR::pos_t p0;
    if( begin() != end() ){
      t0 = begin()->first;
      p0 = begin()->second;
    }
    TASCAR::track_t ntrack;
    for( TASCAR::track_t::iterator it=begin();it != end(); ++it){
      double d = distance(p0,it->second);
      p0 = it->second;
      t0 += d/v;
      ntrack[t0] = p0;
    }
    *this = ntrack;
  }
  prepare();
}

void track_t::set_velocity_csvfile( const std::string& fname_, double offset )
{
  std::string fname(TASCAR::env_expand(fname_));
  std::ifstream fh(fname.c_str());
  if( fh.fail() )
    throw TASCAR::ErrMsg("Unable to open velocity csv file \""+fname+"\".");
  std::string v_tm, v_x;
  track_t vmap;
  while( !fh.eof() ){
    getline(fh,v_tm,',');
    getline(fh,v_x);
    if( v_tm.size() && v_x.size() ){
      double tm = atof(v_tm.c_str());
      double x = atof(v_x.c_str());
      vmap[tm-offset] = pos_t(x,0,0);
    }
  }
  fh.close();
  if( vmap.begin() != vmap.end() ){
    set_velocity_const( 1.0 );
    double d = 0;
    double dt = 0.5;
    track_t ntrack;
    for( double tm=std::max(0.0,vmap.begin()->first); tm <= vmap.rbegin()->first; tm += dt){
      pos_t pv = vmap.interp( tm );
      d += dt*pv.x;
      ntrack[tm] = interp( d );
    }
    *this = ntrack;
  }
  prepare();
}

double track_t::length()
{
  if( !size() )
    return 0;
  double l(0);
  pos_t p = begin()->second;
  for(iterator i=begin();i!=end();++i){
    l+=distance(p,i->second);
    p = i->second;
  }
  return l;
}

track_t::track_t()
  : loop(0),
    interpt(cartesian)
{
}

double track_t::get_dist( double time ) const
{
  if( (loop > 0) && (time > loop) )
    time = fmod(time,loop);
  return time_dist.interp(time);
}

double track_t::get_time( double dist ) const
{
  return dist_time.interp(dist);
}

void track_t::write_xml( xmlpp::Element* a)
{
  switch( interpt ){
  case TASCAR::track_t::cartesian:
    //a->set_attribute("interpolation","cartesian");
    break;
  case TASCAR::track_t::spherical:
    a->set_attribute("interpolation","spherical");
    break;
  }
  xmlpp::Node::NodeList ch = a->get_children();
  for(xmlpp::Node::NodeList::reverse_iterator sn=ch.rbegin();sn!=ch.rend();++sn)
    a->remove_child(*sn);
  a->add_child_text(print_cart(" "));
}

void track_t::read_xml( xmlpp::Element* a )
{
  get_attribute_value(a,"loop",loop);
  track_t ntrack;
  ntrack.loop = loop;
  if( a->get_attribute_value("interpolation") == "spherical" )
    ntrack.set_interpt(TASCAR::track_t::spherical);
  std::string importcsv(a->get_attribute_value("importcsv"));
  if( !importcsv.empty() ){
    // load track from CSV file:
    std::ifstream fh(importcsv.c_str());
    if( fh.fail() || (!fh.good()) ){
      throw TASCAR::ErrMsg("Unable to open track csv file \""+importcsv+"\".");
    }
    std::string v_tm, v_x, v_y, v_z;
    while( !fh.eof() ){
      getline(fh,v_tm,',');
      getline(fh,v_x,',');
      getline(fh,v_y,',');
      getline(fh,v_z);
      if( v_tm.size() && v_x.size() && v_y.size() && v_z.size() ){
        double tm = atof(v_tm.c_str());
        double x = atof(v_x.c_str());
        double y = atof(v_y.c_str());
        double z = atof(v_z.c_str());
        ntrack[tm] = pos_t(x,y,z);
      }
    }
    fh.close();
  }
  std::stringstream ptxt(xml_get_text(a,""));
  while( !ptxt.eof() ){
    double t(-1);
    pos_t p;
    ptxt >> t;
    if( !ptxt.eof() ){
      ptxt >> p.x >> p.y >> p.z;
      ntrack[t] = p;
    }
  }
  *this = ntrack;
}

table1_t::table1_t()
{
}

double table1_t::interp( double x ) const
{
  if( begin() == end() )
    return 0;
  const_iterator lim2 = lower_bound(x);
  if( lim2 == end() )
    return rbegin()->second;
  if( lim2 == begin() )
    return begin()->second;
  if( lim2->first == x )
    return lim2->second;
  const_iterator lim1 = lim2;
  --lim1;
  // cartesian interpolation:
  double p1(lim1->second);
  double p2(lim2->second);
  double w = (x-lim1->first)/(lim2->first-lim1->first);
  make_friendly_number(w);
  p1 *= (1.0-w);
  p2 *= w;
  p1 += p2;
  return p1;
}

void track_t::prepare()
{
  time_dist.clear();
  dist_time.clear();
  if( size() ){
    double l(0);
    pos_t p0(begin()->second);
    for(const_iterator it=begin();it!=end();++it){
      l+=distance(it->second,p0);
      time_dist[it->first] = l;
      dist_time[l] = it->first;
      p0 = it->second;
    }
  }
}

euler_track_t::euler_track_t()
  : loop(0)
{
}

zyx_euler_t euler_track_t::interp(double x) const
{
  if( begin() == end() ){
    return zyx_euler_t();
  }
  if( (loop > 0) && (x >= loop ) )
    x = fmod(x,loop);
  const_iterator lim2 = lower_bound(x);
  if( lim2 == end() )
    return rbegin()->second;
  if( lim2 == begin() )
    return begin()->second;
  if( lim2->first == x )
    return lim2->second;
  const_iterator lim1 = lim2;
  --lim1;
  zyx_euler_t p1(lim1->second);
  zyx_euler_t p2(lim2->second);
  double w = (x-lim1->first)/(lim2->first-lim1->first);
  make_friendly_number(w);
  p1 *= (1.0-w);
  p2 *= w;
  p1 += p2;
  return p1;
}

void euler_track_t::write_xml( xmlpp::Element* a)
{
  xmlpp::Node::NodeList ch = a->get_children();
  for(xmlpp::Node::NodeList::reverse_iterator sn=ch.rbegin();sn!=ch.rend();++sn)
    a->remove_child(*sn);
  a->add_child_text(print(" "));
}

void euler_track_t::read_xml( xmlpp::Element* a )
{
  get_attribute_value(a,"loop",loop);
  euler_track_t ntrack;
  ntrack.loop = loop;
  std::string importcsv(a->get_attribute_value("importcsv"));
  if( !importcsv.empty() ){
    // load track from CSV file:
    std::ifstream fh(importcsv.c_str());
    if( fh.fail() || (!fh.good()) )
      throw TASCAR::ErrMsg("Unable to open Euler track csv file \""+importcsv+"\".");
    std::string v_tm, v_x, v_y, v_z;
    while( !fh.eof() ){
      getline(fh,v_tm,',');
      getline(fh,v_z,',');
      getline(fh,v_y,',');
      getline(fh,v_x);
      if( v_tm.size() && v_x.size() && v_y.size() && v_z.size() ){
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
  std::stringstream ptxt(xml_get_text(a,""));
  while( !ptxt.eof() ){
    double t(-1);
    zyx_euler_t p;
    ptxt >> t;
    if( !ptxt.eof() ){
      ptxt >> p.z >> p.y >> p.x;
      p *= DEG2RAD;
      ntrack[t] = p;
    }
  }
  *this = ntrack;
}

std::string euler_track_t::print(const std::string& delim)
{
  std::ostringstream tmp("");
  tmp.precision(12);
  for(iterator i=begin();i!=end();++i){
    tmp << i->first << delim << i->second.print(delim) << "\n";
  }
  return tmp.str();
}

std::string zyx_euler_t::print(const std::string& delim)
{
  std::ostringstream tmp("");
  tmp.precision(12);
  tmp << RAD2DEG*z << delim << RAD2DEG*y << delim << RAD2DEG*x;
  return tmp.str();

}

void track_t::fill_gaps(double dt)
{
  if( empty() )
    return;
  track_t nt;
  double lt(begin()->first);
  for(iterator i=begin();i!=end();++i){
    nt[i->first] = i->second;
    double tdt(i->first-lt);
    unsigned int n(tdt/dt);
    if( n > 0 ){
      double ldt = tdt/n;
      for( double t=lt+ldt;t<i->first;t+=ldt )
        nt[t] = interp(t);
    }
    lt = i->first;
  }
  *this = nt;
  prepare();
}

void pos_t::normalize()
{
  *this /= norm();
}

bool pos_t::has_infinity() const
{
  if( x == std::numeric_limits<double>::infinity() )
    return true;
  if( y == std::numeric_limits<double>::infinity() )
    return true;
  if( z == std::numeric_limits<double>::infinity() )
    return true;
  return false;
}

shoebox_t::shoebox_t()
{
}

shoebox_t::shoebox_t(const pos_t& center_,const pos_t& size_,const zyx_euler_t& orientation_)
  : center(center_),
    size(size_),
    orientation(orientation_)
{
}

pos_t shoebox_t::nextpoint(pos_t p)
{
  p -= center;
  p /= orientation;
  pos_t prel;
  if( p.x > 0 )
    prel.x = std::max(0.0,p.x-0.5*size.x);
  else
    prel.x = std::min(0.0,p.x+0.5*size.x);
  if( p.y > 0 )
    prel.y = std::max(0.0,p.y-0.5*size.y);
  else
    prel.y = std::min(0.0,p.y+0.5*size.y);
  if( p.z > 0 )
    prel.z = std::max(0.0,p.z-0.5*size.z);
  else
    prel.z = std::min(0.0,p.z+0.5*size.z);
  return prel;
}

ngon_t& ngon_t::operator+=(const pos_t& p)
{
  delta += p;
  update();
  return *this;
}

ngon_t& ngon_t::operator+=(double p)
{
  pos_t n(normal);
  n *= p;
  return (*this += n);
}

ngon_t::ngon_t()
  : N(4)
{
  nonrt_set_rect(1,2);
}

void ngon_t::nonrt_set_rect(double width, double height)
{
  std::vector<pos_t> nverts;
  nverts.push_back(pos_t(0,0,0));
  nverts.push_back(pos_t(0,width,0));
  nverts.push_back(pos_t(0,width,height));
  nverts.push_back(pos_t(0,0,height));
  nonrt_set(nverts);
}

void ngon_t::nonrt_set(const std::vector<pos_t>& verts)
{
  if( verts.size() < 3 )
    throw TASCAR::ErrMsg("A polygon needs at least three vertices.");
  local_verts_ = verts;
  N = verts.size();
  verts_.resize(N);
  edges_.resize(N);
  vert_normals_.resize(N);
  edge_normals_.resize(N);
  // calculate area and aperture size:
  pos_t rot;
  std::vector<pos_t>::iterator i_prev_vert(local_verts_.end()-1);
  for(std::vector<pos_t>::iterator i_vert=local_verts_.begin();i_vert!=local_verts_.end();++i_vert){
    rot += cross_prod(*i_prev_vert,*i_vert);
    i_prev_vert = i_vert;
  }
  local_normal = rot;
  local_normal /= local_normal.norm();
  area = 0.5*rot.norm();
  aperture = 2.0*sqrt(area/M_PI);
  // update global coordinates:
  update();
}

void ngon_t::apply_rot_loc(const pos_t& p0,const zyx_euler_t& o)
{
  orientation = o;
  delta = p0;
  update();
}

void ngon_t::update()
{
  // firtst calculate vertices in global coordinate system:
  std::vector<pos_t>::iterator i_local_vert(local_verts_.begin());
  for(std::vector<pos_t>::iterator i_vert=verts_.begin();i_vert!=verts_.end();++i_vert){
    *i_vert = *i_local_vert;
    *i_vert *= orientation;
    *i_vert += delta;
    ++i_local_vert;
  }
  std::vector<pos_t>::iterator i_vert(verts_.begin());
  std::vector<pos_t>::iterator i_next_vert(i_vert+1);
  for(std::vector<pos_t>::iterator i_edge=edges_.begin();i_edge!=edges_.end();++i_edge){
    *i_edge = *i_next_vert;
    *i_edge -= *i_vert;
    ++i_next_vert;
    if( i_next_vert == verts_.end() )
      i_next_vert = verts_.begin();
    ++i_vert;
  }
  normal = local_normal;
  normal *= orientation;
  // vertex normals, used to calculate inside/outside:
  std::vector<pos_t>::iterator i_prev_edge(edges_.end()-1);
  std::vector<pos_t>::iterator i_edge(edges_.begin());
  for(std::vector<pos_t>::iterator i_vert_normal=vert_normals_.begin();i_vert_normal!=vert_normals_.end();++i_vert_normal){
    *i_vert_normal = cross_prod(i_edge->normal() + i_prev_edge->normal(),normal).normal();
    i_prev_edge = i_edge;
    ++i_edge;
  }
  for(uint32_t k=0;k<N;++k){
    edge_normals_[k] = cross_prod( edges_[k].normal(), normal );
  }
}

pos_t ngon_t::nearest_on_plane( const pos_t& p0 ) const
{
  double plane_dist = dot_prod(normal,verts_[0]-p0);
  pos_t p0d = normal;
  p0d *= plane_dist;
  p0d += p0;
  return p0d;
}

pos_t TASCAR::edge_nearest(const pos_t& v,const pos_t& d,const pos_t& p0)
{
  pos_t p0p1(p0-v);
  double l(d.norm());
  pos_t n(d);
  n /= l;
  double r(0.0);
  if( !p0p1.is_null() )
    r = dot_prod(n,p0p1.normal())*p0p1.norm();
  if( r < 0 )
    return v;
  if( r > l )
    return v+d;
  pos_t p0d(n);
  p0d *= r;
  p0d += v;
  return p0d;
}

pos_t ngon_t::nearest_on_edge(const pos_t& p0,uint32_t* pk0) const
{
  pos_t ne(edge_nearest(verts_[0],edges_[0],p0));
  double d(distance(ne,p0));
  uint32_t k0(0);
  for( uint32_t k=1;k<N;k++){
    pos_t ln(edge_nearest(verts_[k],edges_[k],p0));
    double ld;
    if( (ld = distance(ln,p0)) < d ){
      ne = ln;
      d = ld;
      k0 = k;
    }
  }
  if( pk0 )
    *pk0 = k0;
  return ne;
}

pos_t ngon_t::nearest( const pos_t& p0, bool* is_outside_, pos_t* on_edge_ ) const
{
  uint32_t k0(0);
  pos_t ne(nearest_on_edge(p0,&k0));
  if( on_edge_ )
    *on_edge_ = ne;
  // is inside?
  bool is_outside(false);
  pos_t dp0(ne-p0);
  if( dp0.is_null() )
    // point is exactly on the edge
    is_outside = true;
  else
    // caclulate edge normal:
    is_outside = (dot_prod(edge_normals_[k0],dp0) < 0);
  if( is_outside_ )
    *is_outside_ = is_outside;
  if( is_outside )
    return ne;
  return nearest_on_plane(p0);
}

bool ngon_t::is_infront(const pos_t& p0) const
{
  pos_t p_cut(nearest_on_plane(p0));
  return (dot_prod( p0-p_cut, normal ) > 0);
}

bool ngon_t::is_behind(const pos_t& p0) const
{
  pos_t p_cut(nearest_on_plane(p0));
  return (dot_prod( p0-p_cut, normal ) < 0);
}

bool ngon_t::intersection( const pos_t& p0, const pos_t& p1, pos_t& p_is, double* w) const
{
  pos_t np(nearest_on_plane(p0));
  pos_t dpn(p1-p0);
  double dpl(dpn.norm());
  dpn.normalize();
  double d(distance(np,p0));
  if( d == 0 ){
    // first point is intersecting:
    if( w )
      *w = 0;
    p_is = p0;
    return true;
  }
  double r(dot_prod(dpn,(np-p0).normal()));
  if( r == 0 ){
    // the edge is parallel to the plane; no intersection.
    return false;
  }
  r = d/r;
  dpn *= r;
  r /= dpl;
  if( w )
    *w = r;
  dpn += p0;
  p_is = dpn;
  return true;
}

std::string ngon_t::print(const std::string& delim) const
{
  std::ostringstream tmp("");
  tmp.precision(12);
  for(std::vector<pos_t>::const_iterator i_vert=verts_.begin();i_vert!=verts_.end();++i_vert){
    if( i_vert != verts_.begin() )
      tmp << delim;
    tmp << i_vert->print_cart(delim);
  }
  return tmp.str();
}

std::ostream& operator<<(std::ostream& out, const TASCAR::pos_t& p)
{
  out << p.print_cart();
  return out;
}

std::ostream& operator<<(std::ostream& out, const TASCAR::ngon_t& n)
{
  out << n.print();
  return out;
}

std::vector<pos_t> TASCAR::generate_icosahedron()
{
  std::vector<pos_t> m;
  double phi((1.0+sqrt(5.0))/2.0);
  m.push_back(pos_t(0,1,phi));
  m.push_back(pos_t(0,-1,-phi));
  m.push_back(pos_t(0,1,-phi));
  m.push_back(pos_t(0,-1,phi));
  m.push_back(pos_t(1,phi,0));
  m.push_back(pos_t(1,-phi,0));
  m.push_back(pos_t(-1,-phi,0));
  m.push_back(pos_t(-1,phi,0));
  m.push_back(pos_t(phi,0,1));
  m.push_back(pos_t(-phi,0,1));
  m.push_back(pos_t(phi,0,-1));
  m.push_back(pos_t(-phi,0,-1));
  return m;
}

#ifdef QUICKHULL_1
uint32_t findindex2( const std::vector<Vector3<double>>& spklist, const Vector3<double>& vertex )
{
  for(uint32_t k=0;k<spklist.size();++k)
    if( (spklist[k].x == vertex.x) &&
        (spklist[k].y == vertex.y) &&
        (spklist[k].z == vertex.z)) 
      return k;
  throw TASCAR::ErrMsg("Simplex index not found in list");
  return spklist.size();
}

TASCAR::quickhull_t::quickhull_t( const std::vector<pos_t>& mesh )
{
  std::vector<Vector3<double>> spklist;
  for( auto it=mesh.begin();it!=mesh.end();++it)
    spklist.push_back( Vector3<double>(it->x,it->y,it->z));
  QuickHull<double> qh;
  auto hull = qh.getConvexHull( spklist, true, true );
  auto indexBuffer = hull.getIndexBuffer();
  if( indexBuffer.size() < 12 )
    throw TASCAR::ErrMsg("Invalid convex hull.");
  auto vertexBuffer = hull.getVertexBuffer();
  for( uint32_t khull=0;khull<indexBuffer.size();khull+=3){
    simplex_t sim;
    sim.c1 = findindex2(spklist,vertexBuffer[indexBuffer[khull]]);
    sim.c2 = findindex2(spklist,vertexBuffer[indexBuffer[khull+1]]);
    sim.c3 = findindex2(spklist,vertexBuffer[indexBuffer[khull+2]]);
    faces.push_back(sim);
  }
}
#else

uint32_t findindex2( const std::vector<TASCAR::pos_t>& spklist, const ch_vertex& vertex )
{
  for(uint32_t k=0;k<spklist.size();++k)
    if( (spklist[k].x == vertex.x) &&
        (spklist[k].y == vertex.y) &&
        (spklist[k].z == vertex.z)) 
      return k;
  throw TASCAR::ErrMsg("Simplex index not found in list");
  return spklist.size();
}

TASCAR::quickhull_t::quickhull_t( const std::vector<pos_t>& mesh )
{
  ch_vertex* vertices;
  vertices = (ch_vertex*)malloc(mesh.size()*sizeof(ch_vertex));
  size_t k(0);
  for( auto it=mesh.begin();it!=mesh.end();++it){
    vertices[k].x = it->x;
    vertices[k].y = it->y;
    vertices[k].z = it->z;
    ++k;
  }
  int* faceIndices(NULL);
  int nFaces(0);
  convhull_3d_build(vertices, mesh.size(), &faceIndices, &nFaces);
  for( int khull=0;khull<nFaces;++khull){
    simplex_t sim;
    //sim.c1 = faceIndices[khull];
    //sim.c2 = faceIndices[khull+nFaces];
    //sim.c3 = faceIndices[khull+2*nFaces];
    sim.c1 = faceIndices[3*khull];
    sim.c2 = faceIndices[3*khull+1];
    sim.c3 = faceIndices[3*khull+2];
    sim.c1 = findindex2(mesh,vertices[faceIndices[3*khull]]);
    sim.c2 = findindex2(mesh,vertices[faceIndices[3*khull+1]]);
    sim.c3 = findindex2(mesh,vertices[faceIndices[3*khull+2]]);
    faces.push_back(sim);
  }
  free(vertices);
  free(faceIndices);
}
#endif

std::vector<pos_t> TASCAR::subdivide_and_normalize_mesh( std::vector<pos_t> mesh, uint32_t iterations )
{
  for( auto it=mesh.begin();it!=mesh.end();++it)
    it->normalize();
  for( uint32_t i=0;i<iterations;++i){
    TASCAR::quickhull_t qh( mesh );
    for( auto it=qh.faces.begin(); it!=qh.faces.end(); ++it ){
      pos_t pn(mesh[it->c1]);
      pn += mesh[it->c2];
      pn += mesh[it->c3];
      pn *= 1.0/3.0;
      mesh.push_back(pn);
    }
    for( auto it=mesh.begin();it!=mesh.end();++it)
      it->normalize();
  }
  return mesh;
}

bool operator==(const TASCAR::quickhull_t& h1,const TASCAR::quickhull_t& h2)
{
  if( h1.faces.size() != h2.faces.size() )
    return false;
  // see if all faces are also available in other mesh:
  uint32_t k=0;
  for( auto it=h1.faces.begin();it!=h1.faces.end();++it){
    bool found(false);
    for( auto it2=h2.faces.begin();it2!=h2.faces.end();++it2)
      if( *it == *it2 )
        found = true;
    if( !found ){
      return false;
    }
    ++k;
  }
  return true;
}

bool operator==(const TASCAR::quickhull_t::simplex_t& s1,const TASCAR::quickhull_t::simplex_t& s2)
{
  return (s1.c1==s2.c1) && (s1.c2==s2.c2) && (s1.c3==s2.c3);
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
