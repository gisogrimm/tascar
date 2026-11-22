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
#include <lo/lo.h>

class ap_pos2osc_t : public TASCAR::audioplugin_base_t {
public:
  ap_pos2osc_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  ~ap_pos2osc_t();

private:
  bool sendwhilestopped;
  uint32_t skip;
  std::string url;
  std::string path;
  std::string label;
  // derived variables:
  lo_address lo_addr;
  uint32_t skipcnt;
  lo_message msg;
  lo_arg** oscmsgargv;
  uint32_t has_label;
};

ap_pos2osc_t::ap_pos2osc_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), sendwhilestopped(false), skip(0),
      url("osc.udp://localhost:9999/"), path("/tascarpos"), skipcnt(0),
      has_label(0)
{
  GET_ATTRIBUTE_BOOL_(sendwhilestopped);
  GET_ATTRIBUTE_(skip);
  GET_ATTRIBUTE_(url);
  GET_ATTRIBUTE_(path);
  GET_ATTRIBUTE_(label);
  // rot *= DEG2RAD;
  lo_addr = lo_address_new_from_url(url.c_str());
}

void ap_pos2osc_t::configure()
{
  audioplugin_base_t::configure();
  msg = lo_message_new();
  has_label = !label.empty();
  // time:
  if(has_label)
    lo_message_add_string(msg, label.c_str());
  // coordinates:
  for(uint32_t k = 0; k < 6; ++k)
    lo_message_add_float(msg, 0);
  oscmsgargv = lo_message_get_argv(msg);
}

void ap_pos2osc_t::release()
{
  lo_message_free(msg);
  audioplugin_base_t::release();
}

ap_pos2osc_t::~ap_pos2osc_t()
{
  lo_address_free(lo_addr);
}

void ap_pos2osc_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                              const TASCAR::pos_t& pos,
                              const TASCAR::rotmat_t& rot,
                              const TASCAR::transport_t& tp)
{
  if(chunk.size() != n_channels)
    throw TASCAR::ErrMsg(
        "Programming error (invalid channel number, expected " +
        TASCAR::to_string(n_channels) + ", got " +
        std::to_string(chunk.size()) + ").");
  if(tp.rolling || sendwhilestopped) {
    if(skipcnt) {
      skipcnt--;
    } else {
      // pack data:
      oscmsgargv[has_label]->f = pos.x;
      oscmsgargv[has_label + 1]->f = pos.y;
      oscmsgargv[has_label + 2]->f = pos.z;
      auto eul = rot.to_euler();
      oscmsgargv[has_label + 3]->f = eul.z * RAD2DEG;
      oscmsgargv[has_label + 4]->f = eul.y * RAD2DEG;
      oscmsgargv[has_label + 5]->f = eul.x * RAD2DEG;
      // send message:
      lo_send_message(lo_addr, path.c_str(), msg);
      skipcnt = skip;
    }
  }
}

REGISTER_AUDIOPLUGIN(ap_pos2osc_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
