/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

#include "audioplugin.h"

/*
  This example implements an audio plugin which returns the session
  time in seconds as output.

  Audio plugins inherit from TASCAR::audioplugin_base_t and need to
  implement the method ap_process(), and optionally add_variables().
 */
class sessiontime_t : public TASCAR::audioplugin_base_t {
public:
  sessiontime_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  virtual ~sessiontime_t();

private:
  std::vector<float> a;
};

// default constructor, called while loading the plugin
sessiontime_t::sessiontime_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), a(1, 1)
{
}

sessiontime_t::~sessiontime_t() {}

void sessiontime_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                               const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                               const TASCAR::transport_t& tp)
{
  float dt = 0.0f;
  float inct = 0.0f;
  if(tp.rolling)
    inct = t_sample;
  size_t channels(std::min(chunk.size(), a.size()));
  for(size_t ch = 0; ch < channels; ++ch)
    for(uint32_t k = 0; k < chunk[ch].n; ++k) {
      chunk[ch].d[k] = tp.session_time_seconds + dt;
      dt += inct;
    }
}

// create the plugin interface:
REGISTER_AUDIOPLUGIN(sessiontime_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
