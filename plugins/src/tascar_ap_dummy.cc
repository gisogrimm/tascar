/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
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

class dummy_t : public TASCAR::audioplugin_base_t {
public:
  dummy_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  void add_variables(TASCAR::osc_server_t* srv);
  ~dummy_t();

private:
};

dummy_t::dummy_t(const TASCAR::audioplugin_cfg_t& cfg) : audioplugin_base_t(cfg)
{
  DEBUG("--constructor--");
  DEBUG(f_sample);
  DEBUG(f_fragment);
  DEBUG(t_sample);
  DEBUG(t_fragment);
  DEBUG(n_fragment);
  DEBUG(n_channels);
  DEBUG(name);
  DEBUG(modname);
}

void dummy_t::add_variables(TASCAR::osc_server_t* srv)
{
  DEBUG(srv);
}

void dummy_t::configure()
{
  audioplugin_base_t::configure();
  DEBUG("--prepare--");
  DEBUG(f_sample);
  DEBUG(f_fragment);
  DEBUG(t_sample);
  DEBUG(t_fragment);
  DEBUG(n_fragment);
  DEBUG(n_channels);
  DEBUG(name);
  DEBUG(modname);
}

void dummy_t::release()
{
  audioplugin_base_t::release();
  DEBUG("--release--");
}

dummy_t::~dummy_t()
{
  DEBUG("--destruct--");
}

void dummy_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                         const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                         const TASCAR::transport_t& tp)
{
  DEBUG(chunk.size());
  DEBUG(chunk[0].n);
  DEBUG(tp.object_time_samples);
}

REGISTER_AUDIOPLUGIN(dummy_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
