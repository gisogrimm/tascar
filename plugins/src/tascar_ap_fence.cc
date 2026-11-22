/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

class apfence_t : public TASCAR::audioplugin_base_t {
public:
  apfence_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~apfence_t();

private:
  TASCAR::pos_t origin;
  float r = 1.0f;
  // double gain;
  float alpha = 1.0f;
  float range = 0.1f;
};

apfence_t::apfence_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(origin, "m", "origin");
  GET_ATTRIBUTE(r, "m", "r");
  GET_ATTRIBUTE(alpha, "", "alpha");
  GET_ATTRIBUTE(range, "m", "range");
}

void apfence_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/r", &r);
  srv->add_float("/alpha", &alpha);
  srv->add_float("/range", &range);
  srv->unset_variable_owner();
}

apfence_t::~apfence_t() {}

void apfence_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                           const TASCAR::pos_t& p, const TASCAR::rotmat_t&,
                           const TASCAR::transport_t&)
{
  float gain =
      powf(std::max(0.0f, TASCAR::distancef(p, origin) - r) / range, alpha);
  if(!chunk.empty()) {
    uint32_t nch(chunk.size());
    uint32_t N(chunk[0].n);
    for(uint32_t k = 0; k < N; ++k)
      for(uint32_t ch = 0; ch < nch; ++ch)
        chunk[ch].d[k] *= gain;
  }
}

REGISTER_AUDIOPLUGIN(apfence_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
