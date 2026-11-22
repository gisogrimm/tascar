/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

#include "viewport.h"

viewport_t::viewport_t() : perspective(false), fov(110.0 * DEG2RAD), scale(10.0)
{
  set_fov(120);
}

void viewport_t::set_perspective(bool p)
{
  perspective = p;
}

void viewport_t::set_fov(double f)
{
  fov = 0.5 * f * DEG2RAD;
}

void viewport_t::set_scale(double s)
{
  scale = 0.5 * s;
}

void viewport_t::set_ref(const pos_t& r)
{
  ref = r;
}

void viewport_t::set_rot(const rotmat_t& e)
{
  orientation = e;
}

bool viewport_t::get_perspective() const
{
  return perspective;
}

double viewport_t::get_scale() const
{
  return 2 * scale;
}

TASCAR::pos_t viewport_t::get_ref() const
{
  return ref;
}

double viewport_t::get_fov() const
{
  return 2 * fov * RAD2DEG;
}

pos_t viewport_t::operator()(pos_t x)
{
  x -= ref;
  x *= orientation;
  if(perspective) {
    pos_t t;
    t.x = -x.y;
    t.y = x.z;
    t.z = x.x;
    if(x.x < 0)
      t.z = std::numeric_limits<double>::infinity();
    x = t;
    double d(5.0 / x.norm());
    x.x *= d;
    x.y *= d;
  } else {
    x /= scale;
  }
  return x;
}

pos_t viewport_t::inverse(pos_t x)
{
  if(perspective) {
    x.x *= scale;
    x.y *= scale;
  } else {
    x.x *= scale;
    x.y *= scale;
  }
  x /= orientation;
  x += ref;
  return x;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
