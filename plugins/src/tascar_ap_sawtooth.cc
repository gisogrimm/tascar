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

class sawtooth_t : public TASCAR::audioplugin_base_t {
public:
  sawtooth_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::zyx_euler_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~sawtooth_t();
  static int osc_update(const char*, const char* types, lo_arg** argv, int argc,
                        lo_message, void* user_data)
  {
    if(user_data && (argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
      ((sawtooth_t*)user_data)->a = TASCAR::dbspl2lin(argv[0]->f);
      ((sawtooth_t*)user_data)->f = argv[1]->f;
    }
    return 0;
  }

  float f = 1000.0f;
  float a = 0.05f;
  float fscale = 1.0f;

private:
  float phi = 0.0f;
  float dphi = 0.0f;
};

sawtooth_t::sawtooth_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(f, "Hz", "Frequency");
  GET_ATTRIBUTE_DBSPL(a, "Amplitude");
  GET_ATTRIBUTE(fscale, "", "Frequency scaler");
}

sawtooth_t::~sawtooth_t() {}

void sawtooth_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/f", &f, "]0,20000]", "Frequency in Hz");
  srv->add_float("/fscale", &fscale, "", "Frequency scaling factor");
  srv->add_float_dbspl("/a", &a, "[0,100]", "Amplitude in dB");
  srv->add_method("/af", "ff", &sawtooth_t::osc_update, this);
  srv->unset_variable_owner();
}

void sawtooth_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                            const TASCAR::pos_t&, const TASCAR::zyx_euler_t&,
                            const TASCAR::transport_t&)
{
  dphi = fscale * f / f_sample;
  for(uint32_t k = 0; k < chunk[0].n; ++k) {
    chunk[0].d[k] += 2.0f * a * phi;
    phi += dphi;
    while(phi > 0.5f)
      phi -= 1.0f;
  }
}

REGISTER_AUDIOPLUGIN(sawtooth_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
