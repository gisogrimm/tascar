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

class timestamp_t : public TASCAR::audioplugin_base_t {
public:
  timestamp_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~timestamp_t();

private:
  TASCAR::tictoc_t tictoc;
  std::string path = "/timestamp";
  lo_message msg;
  double* ptime;
  double time1 = 0.0;
  double time2 = 0.0;
  TASCAR::osc_server_t* osrv = NULL;
};

timestamp_t::timestamp_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(path, "", "OSC path where time stamps are dispatched");
  msg = lo_message_new();
  lo_message_add_double(msg, 0.0);
  auto argv = lo_message_get_argv(msg);
  ptime = &(argv[0]->d);
  tictoc.tic();
}

void timestamp_t::add_variables(TASCAR::osc_server_t* srv)
{
  osrv = srv;
}

timestamp_t::~timestamp_t()
{
  lo_message_free(msg);
}

void timestamp_t::ap_process(std::vector<TASCAR::wave_t>&, const TASCAR::pos_t&,
                             const TASCAR::rotmat_t&,
                             const TASCAR::transport_t&)
{
  time1 = tictoc.toc();
  *ptime = time1 - time2;
  time2 = time1;
  if(osrv)
    osrv->dispatch_data_message(path.c_str(), msg);
}

REGISTER_AUDIOPLUGIN(timestamp_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
