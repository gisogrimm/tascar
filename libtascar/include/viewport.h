/* License (GPL)
 *
 * Copyright (C) 2014,2015,2016,2017,2018  Giso Grimm
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
#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "coordinates.h"

using namespace TASCAR;

class viewport_t {
public:
  viewport_t();
  pos_t operator()(pos_t);
  pos_t inverse(pos_t);
  void set_perspective(bool p);
  void set_fov(double f);
  void set_scale(double s);
  void set_ref(const pos_t& r);
  void set_rot(const rotmat_t& o);
  bool get_perspective() const;
  double get_scale() const;
  double get_fov() const;
  pos_t get_ref() const;
  // private:
  rotmat_t orientation;
  pos_t ref;
  bool perspective;
  double fov;
  double scale;
};

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
