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

class pulse_t : public TASCAR::audioplugin_base_t {
public:
  pulse_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~pulse_t();

private:
  double f;
  double a;
  uint32_t period;
};

pulse_t::pulse_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), f(1000), a(0.001), period(0)
{
  GET_ATTRIBUTE(f, "Hz", "Pulse frequency");
  GET_ATTRIBUTE(a, "Pa", "Pulse amplitude");
}

pulse_t::~pulse_t() {}

void pulse_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_double("/f", &f);
  srv->add_double_dbspl("/a", &a);
  srv->unset_variable_owner();
}

void pulse_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                         const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                         const TASCAR::transport_t&)
{
  uint32_t p(f_sample / f);
  size_t channels(chunk.size());
  for(uint32_t k = 0; k < chunk[0].n; ++k) {
    if(!period) {
      period = std::max(1u, p);
      for(size_t ch = 0; ch < channels; ++ch)
        chunk[ch].d[k] += a;
    }
    --period;
  }
}

REGISTER_AUDIOPLUGIN(pulse_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
