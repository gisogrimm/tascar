/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

class delay_t : public TASCAR::audioplugin_base_t {
public:
  delay_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::zyx_euler_t& rot,
                  const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  void release();
  ~delay_t();

private:
  std::vector<double> delay;
  std::vector<TASCAR::wave_t*> dline;
  std::vector<uint32_t> pos;
  bool bypass = false;
};

delay_t::delay_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), delay({1.0}), pos(0)
{
  GET_ATTRIBUTE(delay, "s", "Delays in seconds");
  GET_ATTRIBUTE_BOOL(bypass, "Bypass delay function");
  if(delay.empty())
    delay.push_back(0.0);
}

void delay_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_bool("/bypass", &bypass);
  srv->unset_variable_owner();
}

void delay_t::configure()
{
  audioplugin_base_t::configure();
  dline.clear();
  for(size_t k = 0; k < n_channels; ++k) {
    if(delay[k % delay.size()] > 0.0)
      dline.push_back(new TASCAR::wave_t(
          (uint32_t)(std::max(1.0, f_sample * delay[k % delay.size()]))));
    else
      dline.push_back(NULL);
  }
  pos = std::vector<uint32_t>(n_channels, 0);
}

void delay_t::release()
{
  audioplugin_base_t::release();
  for(auto d : dline)
    delete d;
  dline.clear();
}

delay_t::~delay_t() {}

void delay_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                         const TASCAR::pos_t&, const TASCAR::zyx_euler_t&,
                         const TASCAR::transport_t&)
{
  if(bypass)
    return;
  // first iterate over samples:
  for(uint32_t k = 0; k < chunk[0].n; ++k) {
    // now iterate over channels:
    for(uint32_t c = 0; c < n_channels; ++c) {
      if(dline[c]) {
        float v(dline[c]->d[pos[c]]);
        dline[c]->d[pos[c]] = chunk[c][k];
        chunk[c][k] = v;
        if(pos[c])
          pos[c]--;
        else
          pos[c] = dline[c]->n - 1;
      }
    }
  }
}

REGISTER_AUDIOPLUGIN(delay_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
