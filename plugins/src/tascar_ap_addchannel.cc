/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

using namespace TASCAR;

class addchannel_t : public audioplugin_base_t {
public:
  addchannel_t( const audioplugin_cfg_t& cfg) : audioplugin_base_t( cfg ), first(true) {};
  void ap_process( std::vector<wave_t>& chunk, const pos_t& pos, const TASCAR::zyx_euler_t&, const transport_t& tp);
  void configure();
  bool first;
};

void addchannel_t::ap_process(std::vector<wave_t>& chunk, const pos_t&,
                              const TASCAR::zyx_euler_t&, const transport_t&)
{
  if(first)
    DEBUG(chunk.size());
  first = false;
}

void addchannel_t::configure()
{
  audioplugin_base_t::configure();
  DEBUG(n_channels);
  n_channels++;
  DEBUG(n_channels);
}

REGISTER_AUDIOPLUGIN( addchannel_t );

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
