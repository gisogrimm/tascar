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
#include <lo/lo.h>

class onsetdetector_t : public TASCAR::audioplugin_base_t {
public:
  onsetdetector_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  ~onsetdetector_t();

private:
  lo_address lo_addr;
  double tau;
  double taumin;
  double threshold;
  std::string url;
  std::string path;
  std::string side;
  const std::string side_l;
  const std::string side_r;
  bool b_autoside;
  double lpc1;
  double lpc11;
  double env;
  double ons;
  int8_t crit;
  int8_t crit1;
  int8_t detect;
  double time_since_last;
};

onsetdetector_t::onsetdetector_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), tau(1), taumin(0.05), threshold(0.01),
      url("osc.udp://localhost:9999/"), side_l("L"), side_r("R"),
      b_autoside(false), lpc1(0.0), lpc11(1.0), env(0.0), ons(0.0), crit(0),
      crit1(0), detect(0), time_since_last(0)
{
  GET_ATTRIBUTE(tau, "s", "Level estimator time constant");
  GET_ATTRIBUTE(taumin, "s", "Trigger blocking time");
  GET_ATTRIBUTE(side, "", "");
  GET_ATTRIBUTE_DBSPL(threshold, "Detection threshold");
  GET_ATTRIBUTE(url, "", "Destination OSC URL");
  GET_ATTRIBUTE(path, "", "Destination OSC path");
  if(url.empty())
    url = "osc.udp://localhost:9999/";
  lo_addr = lo_address_new_from_url(url.c_str());
}

void onsetdetector_t::configure()
{
  audioplugin_base_t::configure();
  lpc1 = exp(-1.0 / (tau * f_sample));
  lpc11 = 1.0 - lpc1;
}

onsetdetector_t::~onsetdetector_t()
{
  lo_address_free(lo_addr);
}

void onsetdetector_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                                 const TASCAR::pos_t& pos,
                                 const TASCAR::rotmat_t&,
                                 const TASCAR::transport_t&)
{
  const char* this_side(side.c_str());
  uint32_t N(chunk[0].size());
  float v2threshold(threshold * threshold);
  for(uint32_t k = 0; k < N; ++k) {
    time_since_last += t_sample;
    float v2(chunk[0][k]);
    v2 *= v2;
    env = lpc1 * env + lpc11 * std::max(v2, v2threshold);
    if(v2 > ons)
      ons = v2;
    else
      ons = lpc1 * ons + lpc11 * v2;
    crit1 = crit;
    crit = (ons > env);
    detect = (crit > crit1) && (time_since_last > taumin);
    if(detect) {
      if(side.empty()) {
        if(b_autoside) {
          this_side = side_l.c_str();
          b_autoside = false;
        } else {
          this_side = side_r.c_str();
          b_autoside = true;
        }
      }
      lo_send(lo_addr, path.c_str(), "ssffff", "/hitAt", this_side, pos.x,
              pos.y, pos.z, sqrt(ons));
      time_since_last = 0;
    }
  }
}

REGISTER_AUDIOPLUGIN(onsetdetector_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
