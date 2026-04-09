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
#include "speakerarray.h"

class ap_spkcalib_t : public TASCAR::audioplugin_base_t {
public:
  ap_spkcalib_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::zyx_euler_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  void release();
  virtual ~ap_spkcalib_t();

private:
  TASCAR::spk_array_diff_render_t spk;
};

// default constructor, called while loading the plugin
ap_spkcalib_t::ap_spkcalib_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), spk(e, false)
{
  // register variable for XML access:
}

void ap_spkcalib_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_bool("/enable_eq", &(spk.enable_eq));
  srv->unset_variable_owner();
}

void ap_spkcalib_t::configure()
{
  if(n_channels != spk.num_output_channels())
    throw TASCAR::ErrMsg(std::string("Speaker layout has ") +
                         std::to_string(spk.num_output_channels()) +
                         " channels (" + std::to_string(spk.size()) +
                         " main speaker, " + std::to_string(spk.subs.size()) +
                         " subwoofer, " + std::to_string(spk.conv_channels) +
                         " convolution channels), but plugin has " +
                         std::to_string(n_channels) + " channels.");
  TASCAR::audioplugin_base_t::configure();
  spk.prepare(cfg());
}

void ap_spkcalib_t::release()
{
  TASCAR::audioplugin_base_t::release();
  spk.release();
}

ap_spkcalib_t::~ap_spkcalib_t() {}

void ap_spkcalib_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                               const TASCAR::pos_t&, const TASCAR::zyx_euler_t&,
                               const TASCAR::transport_t&)
{
  // implement the algrithm:
  spk.postproc(chunk);
  for(auto& ch : chunk)
    ch *= 1.0f / (float)spk.caliblevel;
}

// create the plugin interface:
REGISTER_AUDIOPLUGIN(ap_spkcalib_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
