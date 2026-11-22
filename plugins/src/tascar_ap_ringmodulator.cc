/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2024 Giso Grimm
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

class ringmod_t : public TASCAR::audioplugin_base_t {
public:
  ringmod_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~ringmod_t();

private:
  std::vector<float> frange = {300.0f, 2000.0f};
  bool active = true;
  float depth = 1.0f;
  float vco = 0.5f;
  double phi = 0.0;
  double dphi = 0.0;
};

ringmod_t::ringmod_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(frange, "Hz", "Frequency range (VCO controlled)");
  if(frange.size() != 2)
    throw TASCAR::ErrMsg("frange needs two entries");
  GET_ATTRIBUTE(depth, "", "Modulation depth amount");
  GET_ATTRIBUTE(vco, "", "Voltage control input, start value");
  GET_ATTRIBUTE_BOOL(active, "Start activated");
}

ringmod_t::~ringmod_t() {}

void ringmod_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_vector_float("/frange", &frange, "]0,20000]",
                        "Frequency range (VCO controlled), in Hz");
  srv->add_float("/depth", &depth, "[0,1]", "Modulation depth amount");
  srv->add_float("/vco", &vco, "[0,1]", "Voltage control input");
  srv->add_bool("/active", &active, "Activate ring modulator");
  srv->unset_variable_owner();
}

void ringmod_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                           const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                           const TASCAR::transport_t&)
{
  if(!active)
    return;
  float f = expf(logf(frange[0]) * (1.0f - vco) + logf(frange[1]) * vco);
  dphi = TASCAR_2PI * f * t_sample;
  for(uint32_t k = 0; k < chunk[0].n; ++k) {
    chunk[0].d[k] *= depth * sin(phi) + 1.0f - depth;
    phi += dphi;
  }
  phi = fmod(phi, TASCAR_2PI);
}

REGISTER_AUDIOPLUGIN(ringmod_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
