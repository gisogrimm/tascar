/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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
#include "fft.h"

class dump_levels_t : public TASCAR::audioplugin_base_t {
public:
  dump_levels_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
};

dump_levels_t::dump_levels_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
}

void dump_levels_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                               const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                               const TASCAR::transport_t&)
{
  std::cout << this;
  for(auto it = chunk.begin(); it != chunk.end(); ++it)
    std::cout << " " << it->spldb() << " (" << it->maxabs() << ")";
  std::cout << std::endl;
}

REGISTER_AUDIOPLUGIN(dump_levels_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
