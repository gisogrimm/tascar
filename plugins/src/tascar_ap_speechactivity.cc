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
#include <lo/lo.h>
#include <lsl_cpp.h>

class speechactivity_t : public TASCAR::audioplugin_base_t {
public:
  speechactivity_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  ~speechactivity_t();

private:
  lo_address lo_addr;
  lsl::stream_outlet* lsl_outlet;
  double tauenv;
  double tauonset;
  double threshold;
  std::string url;
  std::string path;
  bool transitionsonly;
  std::vector<double> intensity;
  std::vector<int32_t> active;
  std::vector<int32_t> prevactive;
  std::vector<double> dactive;
  std::vector<int32_t> onset;
  std::vector<int32_t> oldonset;
};

speechactivity_t::speechactivity_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), tauenv(1), tauonset(1), threshold(0.0056234),
      url("osc.udp://localhost:9999/"), path("/" + get_fullname()),
      transitionsonly(false)
{
  GET_ATTRIBUTE(tauenv, "s", "Envelope tracking time constant");
  GET_ATTRIBUTE(tauonset, "s", "Onset detection time constant");
  GET_ATTRIBUTE_DBSPL(threshold, "Envelope threshold");
  GET_ATTRIBUTE(url, "", "OSC destination URL");
  GET_ATTRIBUTE(path, "", "OSC destination path");
  GET_ATTRIBUTE_BOOL(transitionsonly, "Send only when a transition occurs");
  if(url.empty())
    url = "osc.udp://localhost:9999/";
}

void speechactivity_t::configure()
{
  audioplugin_base_t::configure();
  lo_addr = lo_address_new_from_url(url.c_str());
  lsl_outlet = new lsl::stream_outlet(
      lsl::stream_info(get_fullname(), "level", n_channels, f_fragment,
                       lsl::cf_int32, tsccfg::node_get_path(e)));
  intensity = std::vector<double>(n_channels, 0.0);
  active = std::vector<int32_t>(n_channels, 0);
  prevactive = std::vector<int32_t>(n_channels, 0);
  dactive = std::vector<double>(n_channels, 0);
  onset = std::vector<int32_t>(n_channels, 0);
  oldonset = std::vector<int32_t>(n_channels, 0);
}

void speechactivity_t::release()
{
  audioplugin_base_t::release();
  delete lsl_outlet;
  lo_address_free(lo_addr);
}

speechactivity_t::~speechactivity_t() {}

void speechactivity_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                                  const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                                  const TASCAR::transport_t&)
{
  TASCAR_ASSERT(chunk.size() == intensity.size());
  const double lpc1(exp(-TASCAR_2PI / (tauenv * f_fragment)));
  const double lpc2(pow(2.0, -1.0 / (tauonset * f_fragment)));
  const float v2threshold(threshold * threshold);
  bool transition(false);
  for(uint32_t ch = 0; ch < chunk.size(); ++ch) {
    // first get signal intensity:
    intensity[ch] = (1.0 - lpc1) * chunk[ch].ms() + lpc1 * intensity[ch];
    // speech activity is given if the intensity is above the given
    // threshold:
    active[ch] = (intensity[ch] > v2threshold);
    // calculate low-pass filtered temporal derivative of activity, to
    // get the onsets:
    dactive[ch] = (active[ch] - prevactive[ch]) + lpc2 * dactive[ch];
    prevactive[ch] = active[ch];
    onset[ch] = (dactive[ch] > 0.5) * active[ch] + active[ch];
    if(onset[ch] != oldonset[ch])
      transition = true;
    oldonset[ch] = onset[ch];
  }
  if((!transitionsonly) || transition) {
    for(uint32_t ch = 0; ch < chunk.size(); ++ch) {
      char ctmp[1024];
      ctmp[1023] = 0;
      snprintf(ctmp, 1023, "%s%d", path.c_str(), ch);
      lo_send(lo_addr, ctmp, "i", onset[ch]);
    }
    lsl_outlet->push_sample(onset);
  }
}

REGISTER_AUDIOPLUGIN(speechactivity_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
