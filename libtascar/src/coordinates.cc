/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 */
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
#include "errorhandling.h"
#include "tscconfig.h"
#include <cmath>
#include <numeric>
#include <sstream>
#include <cstdlib>
#include <vector>

#include "quickhull/QuickHull.cpp"
#include "quickhull/QuickHull.hpp"

using namespace quickhull;

using namespace TASCAR;

double TASCAR::drand()
{
  return (double)rand() / ((double)RAND_MAX + 1.0);
}

float TASCAR::frand()
{
  return (float)rand() / ((float)RAND_MAX + 1.0f);
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
  tmp << norm() << delim << RAD2DEG * azim() << delim << RAD2DEG * elev();
  return tmp.str();
}

std::string posf_t::print_cart(const std::string& delim) const
{
  std::ostringstream tmp("");
  tmp.precision(9);
  tmp << x << delim << y << delim << z;
  return tmp.str();
}

std::string posf_t::print_sphere(const std::string& delim) const
{
  std::ostringstream tmp("");
  tmp.precision(9);
  tmp << norm() << delim << RAD2DEGf * azim() << delim << RAD2DEGf * elev();
  return tmp.str();
}

table1_t::table1_t() {}

double table1_t::interp(double x) const
{
  if(begin() == end())
    return 0;
  const_iterator lim2 = lower_bound(x);
  if(lim2 == end())
    return rbegin()->second;
  if(lim2 == begin())
    return begin()->second;
  if(lim2->first == x)
    return lim2->second;
  const_iterator lim1 = lim2;
  --lim1;
  // cartesian interpolation:
  double p1(lim1->second);
  double p2(lim2->second);
  double w = (x - lim1->first) / (lim2->first - lim1->first);
  make_friendly_number(w);
  p1 *= (1.0 - w);
  p2 *= w;
  p1 += p2;
  return p1;
}

void pos_t::normalize()
{
  *this /= norm();
}

void posf_t::normalize()
{
  *this /= norm();
}

bool pos_t::has_infinity() const
{
  return std::isinf(x) || std::isinf(y) || std::isinf(z);
}

bool posf_t::has_infinity() const
{
  return std::isinf(x) || std::isinf(y) || std::isinf(z);
}

shoebox_t::shoebox_t() {}

shoebox_t::shoebox_t(const pos_t& center_, const pos_t& size_,
                     const zyx_euler_t& orientation_)
    : center(center_), size(size_), orientation(orientation_)
{
}

pos_t shoebox_t::nextpoint(pos_t p)
{
  p -= center;
  p /= orientation;
  pos_t prel;
  if(p.x > 0)
    prel.x = std::max(0.0, p.x - 0.5 * size.x);
  else
    prel.x = std::min(0.0, p.x + 0.5 * size.x);
  if(p.y > 0)
    prel.y = std::max(0.0, p.y - 0.5 * size.y);
  else
    prel.y = std::min(0.0, p.y + 0.5 * size.y);
  if(p.z > 0)
    prel.z = std::max(0.0, p.z - 0.5 * size.z);
  else
    prel.z = std::min(0.0, p.z + 0.5 * size.z);
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

ngon_t::ngon_t() : N(4)
{
  nonrt_set_rect(1, 2);
}

void ngon_t::nonrt_set_rect(double width, double height)
{
  std::vector<pos_t> nverts;
  nverts.push_back(pos_t(0, 0, 0));
  nverts.push_back(pos_t(0, width, 0));
  nverts.push_back(pos_t(0, width, height));
  nverts.push_back(pos_t(0, 0, height));
  nonrt_set(nverts);
}

void ngon_t::nonrt_set(const std::vector<pos_t>& verts)
{
  if(verts.size() < 3)
    throw TASCAR::ErrMsg("A polygon needs at least three vertices.");
  if(verts.size() > (size_t)1 << 31)
    throw TASCAR::ErrMsg("Too many vertices.");
  local_verts_ = verts;
  N = (uint32_t)(verts.size());
  verts_.resize(N);
  edges_.resize(N);
  vert_normals_.resize(N);
  edge_normals_.resize(N);
  // calculate area and aperture size:
  pos_t rot;
  std::vector<pos_t>::iterator i_prev_vert(local_verts_.end() - 1);
  for(std::vector<pos_t>::iterator i_vert = local_verts_.begin();
      i_vert != local_verts_.end(); ++i_vert) {
    rot += cross_prod(*i_prev_vert, *i_vert);
    i_prev_vert = i_vert;
  }
  local_normal = rot;
  local_normal /= local_normal.norm();
  area = 0.5 * rot.norm();
  aperture = 2.0 * sqrt(area / TASCAR_PI);
  // update global coordinates:
  update();
}

void ngon_t::apply_rot_loc(const pos_t& p0, const zyx_euler_t& o)
{
  orientation = o;
  delta = p0;
  update();
}

void ngon_t::update()
{
  // first calculate vertices in global coordinate system:
  std::vector<pos_t>::iterator i_local_vert(local_verts_.begin());
  for(std::vector<pos_t>::iterator i_vert = verts_.begin();
      i_vert != verts_.end(); ++i_vert) {
    *i_vert = *i_local_vert;
    *i_vert *= orientation;
    *i_vert += delta;
    ++i_local_vert;
  }
  std::vector<pos_t>::iterator i_vert(verts_.begin());
  std::vector<pos_t>::iterator i_next_vert(i_vert + 1);
  for(std::vector<pos_t>::iterator i_edge = edges_.begin();
      i_edge != edges_.end(); ++i_edge) {
    *i_edge = *i_next_vert;
    *i_edge -= *i_vert;
    ++i_next_vert;
    if(i_next_vert == verts_.end())
      i_next_vert = verts_.begin();
    ++i_vert;
  }
  normal = local_normal;
  normal *= orientation;
  // vertex normals, used to calculate inside/outside:
  std::vector<pos_t>::iterator i_prev_edge(edges_.end() - 1);
  std::vector<pos_t>::iterator i_edge(edges_.begin());
  for(std::vector<pos_t>::iterator i_vert_normal = vert_normals_.begin();
      i_vert_normal != vert_normals_.end(); ++i_vert_normal) {
    *i_vert_normal =
        cross_prod(i_edge->normal() + i_prev_edge->normal(), normal).normal();
    i_prev_edge = i_edge;
    ++i_edge;
  }
  for(uint32_t k = 0; k < N; ++k) {
    edge_normals_[k] = cross_prod(edges_[k].normal(), normal);
  }
}

pos_t ngon_t::nearest_on_plane(const pos_t& p0) const
{
  double plane_dist = dot_prod(normal, verts_[0] - p0);
  pos_t p0d = normal;
  p0d *= plane_dist;
  p0d += p0;
  return p0d;
}

pos_t TASCAR::edge_nearest(const pos_t& v, const pos_t& d, const pos_t& p0)
{
  pos_t p0p1(p0 - v);
  double l(d.norm());
  pos_t n(d);
  n /= l;
  double r(0.0);
  if(!p0p1.is_null())
    r = dot_prod(n, p0p1.normal()) * p0p1.norm();
  if(r < 0)
    return v;
  if(r > l)
    return v + d;
  pos_t p0d(n);
  p0d *= r;
  p0d += v;
  return p0d;
}

pos_t ngon_t::nearest_on_edge(const pos_t& p0, uint32_t* pk0) const
{
  pos_t ne(edge_nearest(verts_[0], edges_[0], p0));
  double d(distance(ne, p0));
  uint32_t k0(0);
  for(uint32_t k = 1; k < N; k++) {
    pos_t ln(edge_nearest(verts_[k], edges_[k], p0));
    double ld;
    if((ld = distance(ln, p0)) < d) {
      ne = ln;
      d = ld;
      k0 = k;
    }
  }
  if(pk0)
    *pk0 = k0;
  return ne;
}

pos_t ngon_t::nearest(const pos_t& p0, bool* is_outside_, pos_t* on_edge_) const
{
  uint32_t k0(0);
  pos_t ne(nearest_on_edge(p0, &k0));
  if(on_edge_)
    *on_edge_ = ne;
  // is inside?
  bool is_outside(false);
  pos_t dp0(ne - p0);
  if(dp0.is_null())
    // point is exactly on the edge
    is_outside = true;
  else
    // caclulate edge normal:
    is_outside = (dot_prod(edge_normals_[k0], dp0) < 0);
  if(is_outside_)
    *is_outside_ = is_outside;
  if(is_outside)
    return ne;
  return nearest_on_plane(p0);
}

bool ngon_t::is_infront(const pos_t& p0) const
{
  pos_t p_cut(nearest_on_plane(p0));
  return (dot_prod(p0 - p_cut, normal) > 0);
}

bool ngon_t::is_behind(const pos_t& p0) const
{
  pos_t p_cut(nearest_on_plane(p0));
  return (dot_prod(p0 - p_cut, normal) < 0);
}

bool ngon_t::intersection(const pos_t& p0, const pos_t& p1, pos_t& p_is,
                          double* w) const
{
  // 1. Find the nearest point on the plane to p0
  pos_t np(nearest_on_plane(p0));

  // 2. Calculate the vector from p0 to p1
  pos_t dir(p1 - p0);
  double dpl(dir.norm()); // Length of the segment

  // Handle degenerate case where p0 and p1 are the same point
  if(dpl == 0) {
    // If p0 is on the plane, it intersects, otherwise it doesn't
    double d = distance(np, p0);
    if(d == 0) {
      if(w)
        *w = 0;
      p_is = p0;
      return true;
    }
    return false;
  }

  dir.normalize(); // dpn in original code

  // 3. Calculate distance from p0 to the plane
  // Note: distance(np, p0) is the absolute distance.
  // We need the signed distance to determine direction.
  // Assuming distance() returns absolute value, we calculate sign manually.
  // Or, if distance() is signed, use it directly.
  // Here we calculate the signed distance using the plane normal.
  // (Assuming get_normal() returns the plane's normal vector)
  // pos_t plane_normal = get_normal();
  double d = dot_prod(normal, np - p0);

  if(std::abs(d) < 1e-9) { // Check if p0 is effectively on the plane
    if(w)
      *w = 0;
    p_is = p0;
    return true;
  }

  // 4. Calculate the projection of the line onto the plane normal
  // This replaces: dot_prod(dpn, (np - p0).normal())
  double denom = dot_prod(dir, normal);

  if(std::abs(denom) < 1e-9) {
    // The line is parallel to the plane; no intersection (unless on plane,
    // handled above)
    return false;
  }

  // 5. Calculate the ratio 'r' (distance along line / total segment length)
  // r = d / denom
  // Since 'd' is distance to plane and 'denom' is the component of dir along
  // normal, this gives us the scalar distance along the line.
  auto r = d / denom;

  // 6. Check if the intersection is within the segment [0, 1]
  // (Optional: usually desired for segment intersection)
  // if (r < 0 || r > 1) return false;

  if(w)
    *w = r;

  // 7. Calculate the intersection point
  // p_is = p0 + dir * r
  // Note: We must multiply by the scalar distance 'r', not the normalized 'r'
  // from original code logic. However, the original code calculated r /= dpl at
  // the end. Let's stick to standard vector math: Point = Origin + Direction *
  // ScalarDistance
  dir *= r;
  p_is = p0 + dir;

  return true;
}

std::string ngon_t::print(const std::string& delim) const
{
  std::ostringstream tmp("");
  tmp.precision(12);
  for(std::vector<pos_t>::const_iterator i_vert = verts_.begin();
      i_vert != verts_.end(); ++i_vert) {
    if(i_vert != verts_.begin())
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
  double phi((1.0 + sqrt(5.0)) / 2.0);
  m.push_back(pos_t(0, 1, phi));
  m.push_back(pos_t(0, -1, -phi));
  m.push_back(pos_t(0, 1, -phi));
  m.push_back(pos_t(0, -1, phi));
  m.push_back(pos_t(1, phi, 0));
  m.push_back(pos_t(1, -phi, 0));
  m.push_back(pos_t(-1, -phi, 0));
  m.push_back(pos_t(-1, phi, 0));
  m.push_back(pos_t(phi, 0, 1));
  m.push_back(pos_t(-phi, 0, 1));
  m.push_back(pos_t(phi, 0, -1));
  m.push_back(pos_t(-phi, 0, -1));
  return m;
}

TASCAR::quickhull_t::quickhull_t(const std::vector<pos_t>& vertices)
{
  std::vector<Vector3<double>> spklist;
  for(const auto& vert : vertices)
    spklist.push_back(Vector3<double>(vert.x, vert.y, vert.z));
  QuickHull<double> qh;
  auto hull = qh.getConvexHull(spklist, true, true);
  auto indexBuffer = hull.getIndexBuffer();
  if(indexBuffer.size() < 12)
    throw TASCAR::ErrMsg("Invalid convex hull.");
  for(uint32_t khull = 0; khull < indexBuffer.size(); khull += 3) {
    simplex_t sim;
    sim.c1 = indexBuffer[khull];
    sim.c2 = indexBuffer[khull + 1];
    sim.c3 = indexBuffer[khull + 2];
    if((sim.c2 < sim.c1) && (sim.c2 < sim.c3)) {
      // c2 is smallest, turn backwards:
      simplex_t sim2;
      sim2.c1 = sim.c2;
      sim2.c2 = sim.c3;
      sim2.c3 = sim.c1;
      sim = sim2;
    } else {
      if((sim.c3 < sim.c1) && (sim.c3 < sim.c2)) {
        // c3 is smallest, turn forward:
        simplex_t sim2;
        sim2.c1 = sim.c3;
        sim2.c2 = sim.c1;
        sim2.c3 = sim.c2;
        sim = sim2;
      }
    }
    faces.push_back(sim);
  }
  std::sort(faces.begin(), faces.end(), [](const simplex_t& a, const simplex_t& b) {
    if(a.c1 < b.c1)
      return true;
    if(a.c1 == b.c1) {
      if(a.c2 < b.c2)
        return true;
      if(a.c2 == b.c2)
        if(a.c3 < b.c3)
          return true;
    }
    return false;
  });
}

std::vector<pos_t> TASCAR::subdivide_and_normalize_mesh(std::vector<pos_t> mesh,
                                                        uint32_t iterations)
{
  for(auto it = mesh.begin(); it != mesh.end(); ++it)
    it->normalize();
  for(uint32_t i = 0; i < iterations; ++i) {
    TASCAR::quickhull_t qh(mesh);
    for(auto it = qh.faces.begin(); it != qh.faces.end(); ++it) {
      pos_t pn(mesh[it->c1]);
      pn += mesh[it->c2];
      pn += mesh[it->c3];
      pn *= 1.0 / 3.0;
      mesh.push_back(pn);
    }
    for(auto it = mesh.begin(); it != mesh.end(); ++it)
      it->normalize();
  }
  return mesh;
}

bool operator==(const TASCAR::quickhull_t& h1, const TASCAR::quickhull_t& h2)
{
  if(h1.faces.size() != h2.faces.size())
    return false;
  // Both face lists are sorted by the quickhull_t constructor, so
  // element-wise equality is equivalent to multiset equality
  // (duplicate faces are now handled correctly).
  for(size_t k = 0; k < h1.faces.size(); ++k) {
    if(!(h1.faces[k] == h2.faces[k]))
      return false;
  }
  return true;
}

bool operator==(const TASCAR::quickhull_t::simplex_t& s1,
                const TASCAR::quickhull_t::simplex_t& s2)
{
  return (s1.c1 == s2.c1) && (s1.c2 == s2.c2) && (s1.c3 == s2.c3);
}

std::string TASCAR::to_string(const rotmat_t& m)
{
  std::string s =
      "\n[" + to_string(m.m11, "%1.4g") + " " + to_string(m.m12, "%1.4g") +
      " " + to_string(m.m13, "%1.4g") + "]\n[" + to_string(m.m21, "%1.4g") +
      " " + to_string(m.m22, "%1.4g") + " " + to_string(m.m23, "%1.4g") +
      "]\n[" + to_string(m.m31, "%1.4g") + " " + to_string(m.m32, "%1.4g") +
      " " + to_string(m.m33, "%1.4g") + "]\n";
  return s;
}

void TASCAR::vector_get_mean_std(const std::vector<double>& v, double& mean,
                                 double& stdev)
{
  mean = std::numeric_limits<double>::quiet_NaN();
  stdev = std::numeric_limits<double>::quiet_NaN();
  if(v.empty())
    return;
  double sum = std::accumulate(v.begin(), v.end(), 0.0);
  mean = sum / (double)(v.size());
  if(v.size() == 1)
    return;
  std::vector<double> diff(v.size());
  std::transform(v.begin(), v.end(), diff.begin(),
                 [mean](double x) { return x - mean; });
  double sq_sum =
      std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
  stdev = std::sqrt(sq_sum / ((double)(v.size()) - 1.0));
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
