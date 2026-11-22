/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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
#include <lo/lo.h>

class lookatme_t : public TASCAR::audioplugin_base_t {
public:
  lookatme_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void add_variables(TASCAR::osc_server_t* srv);
  ~lookatme_t();

private:
  lo_address lo_addr;
  double tau;
  double fadelen;
  double threshold;
  std::string animation;
  std::string url;
  std::vector<std::string> paths;
  std::string thresholdpath;
  std::string levelpath;
  TASCAR::pos_t pos_onset;
  TASCAR::pos_t pos_offset;
  std::string self_;

  double lpc1;
  double rms;
  bool waslooking;
  bool active;
  bool discordantLS;
};

lookatme_t::lookatme_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), tau(1), fadelen(1), threshold(0.01),
      url("osc.udp://localhost:9999/"), self_(cfg.parentname), lpc1(0.0),
      rms(0.0), waslooking(false), active(true), discordantLS(false)
{
  GET_ATTRIBUTE(tau, "s", "Time constant of level estimation");
  GET_ATTRIBUTE(fadelen, "s", "Motion duration after threshold");
  GET_ATTRIBUTE_DBSPL(threshold, "Level threshold");
  GET_ATTRIBUTE(url, "", "Target OSC URL");
  GET_ATTRIBUTE(paths, "", "Space-separated list of target paths");
  GET_ATTRIBUTE(animation, "", "Animation name (or empty for no animation)");
  GET_ATTRIBUTE(thresholdpath, "",
                "Destination path of threshold criterion (or empty)");
  GET_ATTRIBUTE(levelpath, "", "Destination path of level logging (or empty)");
  GET_ATTRIBUTE(
      pos_onset, "m",
      "Position to look at on onset (or empty to look at vertex position)");
  GET_ATTRIBUTE(pos_offset, "m",
                "Position to look at on offset (or empty for no change of look "
                "direction)");
  if(url.empty())
    url = "osc.udp://localhost:9999/";
  lo_addr = lo_address_new_from_url(url.c_str());
}

void lookatme_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_bool("/active", &active);
  srv->add_bool("/discordantLS", &discordantLS);
  srv->add_double_dbspl("/threshold", &threshold);
  srv->unset_variable_owner();
}

void lookatme_t::configure()
{
  audioplugin_base_t::configure();
  lpc1 = exp(-1.0 / (tau * f_fragment));
  rms = 0;
  waslooking = false;
}

lookatme_t::~lookatme_t()
{
  lo_address_free(lo_addr);
}

void lookatme_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                            const TASCAR::pos_t& pos, const TASCAR::rotmat_t&,
                            const TASCAR::transport_t&)
{
  rms = lpc1 * rms + (1.0 - lpc1) * chunk[0].rms();
  if(!levelpath.empty())
    lo_send(lo_addr, levelpath.c_str(), "f", TASCAR::lin2dbspl(rms));
  if(rms > threshold) {
    if(!waslooking) {
      // send lookatme values to osc target:
      if(active) {
        if(!pos_onset.is_null()) {
          for(std::vector<std::string>::iterator s = paths.begin();
              s != paths.end(); ++s)
            lo_send(lo_addr, s->c_str(), "sffff", "/lookAt", pos_onset.x,
                    pos_onset.y, pos_onset.z, fadelen);
        } else {
          for(std::vector<std::string>::iterator s = paths.begin();
              s != paths.end(); ++s)
            lo_send(lo_addr, s->c_str(), "sffff", "/lookAt", pos.x, pos.y,
                    pos.z, fadelen);
        }
        if(!animation.empty())
          lo_send(lo_addr, self_.c_str(), "ss", "/animation",
                  animation.c_str());
      }
      if(!thresholdpath.empty())
        lo_send(lo_addr, thresholdpath.c_str(), "f", 1.0f);
      if(discordantLS)
        lo_send(lo_addr, self_.c_str(), "sf", "/discordantLS", 1.0);
      waslooking = true;
    }
  } else {
    if(waslooking) {
      if(active) {
        if(!pos_offset.is_null()) {
          for(std::vector<std::string>::iterator s = paths.begin();
              s != paths.end(); ++s)
            lo_send(lo_addr, s->c_str(), "sffff", "/lookAt", pos_offset.x,
                    pos_offset.y, pos_offset.z, fadelen);
        }
      }
      if(!thresholdpath.empty())
        lo_send(lo_addr, thresholdpath.c_str(), "f", 0.0f);
      // if( discordantLS )
      lo_send(lo_addr, self_.c_str(), "sf", "/discordantLS", 0.0);
    }
    // below threshold, release:
    waslooking = false;
  }
}

REGISTER_AUDIOPLUGIN(lookatme_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
