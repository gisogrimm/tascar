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
  This example implements an audio plugin which is a white noise
  generator.

  Audio plugins inherit from TASCAR::audioplugin_base_t and need to
  implement the method ap_process(), and optionally add_variables().
 */
class noise_t : public TASCAR::audioplugin_base_t {
public:
  noise_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  virtual ~noise_t();

private:
  double a;
};

// default constructor, called while loading the plugin
noise_t::noise_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), a(0.001)
{
  // register variable for XML access:
  GET_ATTRIBUTE_DBSPL(a, "Noise level");
}

noise_t::~noise_t() {}

void noise_t::add_variables(TASCAR::osc_server_t* srv)
{
  // register variables for interactive OSC access:
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_double_dbspl("/a", &a);
  srv->unset_variable_owner();
}

void noise_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                         const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                         const TASCAR::transport_t&)
{
  // implement the algrithm:
  for(uint32_t k = 0; k < chunk[0].n; ++k)
    chunk[0].d[k] += 2.0 * a * (TASCAR::drand() - 0.5);
}

// create the plugin interface:
REGISTER_AUDIOPLUGIN(noise_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
