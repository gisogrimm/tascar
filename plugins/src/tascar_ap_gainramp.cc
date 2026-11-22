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
#include "errorhandling.h"

class gainramp_t : public TASCAR::audioplugin_base_t {
public:
  gainramp_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~gainramp_t();

private:
  double gain;
  double slope;
  double maxgain;
};

gainramp_t::gainramp_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), gain(1.0), slope(0.0), maxgain(1.0)
{
  GET_ATTRIBUTE_DB(gain, "Set current gain");
  GET_ATTRIBUTE_DB(slope, "Set gain slope in dB/s");
  GET_ATTRIBUTE_DB(maxgain, "Set maximal gain");
}

void gainramp_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_double_db("/gain", &gain);
  srv->add_double_db("/slope", &slope);
  srv->add_double_db("/maxgain", &maxgain);
  srv->unset_variable_owner();
}

gainramp_t::~gainramp_t() {}

void gainramp_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                            const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                            const TASCAR::transport_t&)
{
  if(!chunk.empty()) {
    uint32_t nch(chunk.size());
    uint32_t N(chunk[0].n);
    double dg(1.0);
    if(slope != 1.0)
      dg = pow(slope, t_sample);
    for(uint32_t k = 0; k < N; ++k) {
      gain *= dg;
      if(gain > maxgain)
        gain = maxgain;
      if(gain < std::numeric_limits<float>::min())
        gain = 0.0;
      for(uint32_t ch = 0; ch < nch; ++ch)
        chunk[ch].d[k] *= gain;
    }
  }
}

REGISTER_AUDIOPLUGIN(gainramp_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
