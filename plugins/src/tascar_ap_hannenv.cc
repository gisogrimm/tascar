/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

#include "audioplugin.h"

class hannenv_t : public TASCAR::audioplugin_base_t {
public:
  hannenv_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  ~hannenv_t();

private:
  double t0;
  double ramp1;
  double steady;
  double ramp2;
  double period;
};

hannenv_t::hannenv_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), t0(0), ramp1(0.25), steady(0.5), ramp2(0.25),
      period(2)
{
  GET_ATTRIBUTE(t0, "s", "Start time");
  GET_ATTRIBUTE(ramp1, "s", "First ramp length");
  GET_ATTRIBUTE(steady, "s", "Duration of steady state");
  GET_ATTRIBUTE(ramp2, "s", "Second ramp length");
  GET_ATTRIBUTE(period, "s", "Period time");
}

hannenv_t::~hannenv_t() {}

void hannenv_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                           const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                           const TASCAR::transport_t& tp)
{
  const double t1(ramp1);
  const double t2(t1 + steady);
  const double t3(t2 + ramp2);
  const double p1(TASCAR_PI / ramp1);
  const double p2(TASCAR_PI / ramp2);
  double t(tp.object_time_seconds - t0);
  double dt(t_sample);
  if(!tp.rolling)
    dt = 0;
  for(uint32_t k = 0; k < chunk[0].n; ++k) {
    t = fmod(t, period);
    if(t <= 0)
      chunk[0].d[k] = 0.0f;
    else if(t < t1)
      chunk[0].d[k] *= 0.5f * (1.0f - (float)(cos(p1 * t)));
    else if(t > t2) {
      if(t < t3)
        chunk[0].d[k] *= 0.5f * (1.0f + (float)(cos(p2 * (t - t2))));
      else
        chunk[0].d[k] = 0.0f;
    }
    t += dt;
  }
}

REGISTER_AUDIOPLUGIN(hannenv_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
