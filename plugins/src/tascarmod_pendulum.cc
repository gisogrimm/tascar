/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

#include "session.h"

class pendulum_t : public TASCAR::actor_module_t {
public:
  pendulum_t(const TASCAR::module_cfg_t& cfg);
  ~pendulum_t();
  void update(uint32_t frame, bool running);

private:
  double amplitude;
  double frequency;
  double decaytime;
  double starttime;
  TASCAR::pos_t distance;
};

pendulum_t::pendulum_t(const TASCAR::module_cfg_t& cfg)
    : actor_module_t(cfg), amplitude(45), frequency(0.5), decaytime(40),
      starttime(10), distance(0, 0, -2)
{
  GET_ATTRIBUTE_(amplitude);
  GET_ATTRIBUTE_(frequency);
  GET_ATTRIBUTE_(decaytime);
  GET_ATTRIBUTE_(starttime);
  GET_ATTRIBUTE_(distance);
}

pendulum_t::~pendulum_t() {}

void pendulum_t::update(uint32_t frame, bool)
{
  double time((double)frame * t_sample);
  double rx(amplitude * DEG2RAD);
  time -= starttime;
  if(time > 0)
    rx = amplitude * DEG2RAD * cos(TASCAR_2PI * frequency * (time)) *
         exp(-0.70711 * time / decaytime);
  for(std::vector<TASCAR::named_object_t>::iterator iobj = obj.begin();
      iobj != obj.end(); ++iobj) {
    TASCAR::zyx_euler_t dr(0, 0, rx);
    TASCAR::pos_t dp(distance);
    dp *= TASCAR::zyx_euler_t(0, 0, rx);
    auto zyeul = iobj->obj->c6dof.orientation.to_euler();
    zyeul.x = 0.0;
    dp *= zyeul;
    iobj->obj->dorientation = dr;
    iobj->obj->dlocation = dp;
  }
}

REGISTER_MODULE(pendulum_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
