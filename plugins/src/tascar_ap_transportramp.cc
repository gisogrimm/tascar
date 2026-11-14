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

class transportramp_t : public TASCAR::audioplugin_base_t {
public:
  transportramp_t(const TASCAR::audioplugin_cfg_t& cfg);
  ~transportramp_t();
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::zyx_euler_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();

private:
  float startduration = 0.025f;
  float endduration = 0.025f;
  float startgain = 0.0f;
  float endgain = 0.0f;
  TASCAR::transport_t tpprev;
  float gain = 0.0f;
  uint32_t rampcount = 0;
  uint32_t ramplen = 1u;
  TASCAR::wave_t startramp = TASCAR::wave_t(1);
  TASCAR::wave_t endramp = TASCAR::wave_t(1);
  TASCAR::wave_t* ramp = &startramp;
  bool precalc = true;
};

transportramp_t::transportramp_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(startduration, "s",
                "Duration of ramp when transport is switched from ``stopped'' "
                "to ``rolling''");
  GET_ATTRIBUTE(endduration, "s",
                "Duration of ramp when transport is switched from ``rolling'' "
                "to ``stopped''");
  GET_ATTRIBUTE_BOOL(precalc,
                     "Operation mode, to switch between precalculated and "
                     "online-generated ramps");
}

transportramp_t::~transportramp_t() {}

void transportramp_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  if(!precalc) {
    srv->add_float("/startduration", &startduration, "]0,10]",
                   "Ramp duration for stopped-to-rolling transition in s");
    srv->add_float("/endduration", &endduration, "]0,10]",
                   "Ramp duration for rolling-to-stopped transition in s");
  }
  srv->unset_variable_owner();
}

void transportramp_t::configure()
{
  audioplugin_base_t::configure();
  // create start ramp - starting with 1 because the iterator is going from
  // end to beginning:
  startramp.resize(
      std::max(1u, (uint32_t)(f_sample * std::max(0.0f, startduration))));
  for(uint32_t k = 0; k < startramp.n; ++k)
    startramp.d[k] = 0.5f + 0.5f * cosf(k * TASCAR_PIf / startramp.n);
  // same with second ramp:
  endramp.resize(
      std::max(1u, (uint32_t)(f_sample * std::max(0.0f, endduration))));
  for(uint32_t k = 0; k < endramp.n; ++k)
    endramp.d[k] = 0.5f + 0.5f * cosf(k * TASCAR_PIf / endramp.n);
}

void transportramp_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                                 const TASCAR::pos_t&,
                                 const TASCAR::zyx_euler_t&,
                                 const TASCAR::transport_t& tp)
{
  if(tp.rolling != tpprev.rolling) {
    startgain = gain;
    if(tp.rolling) {
      ramplen = (uint32_t)std::max(1.0, startduration * f_sample);
      if(precalc)
        rampcount = startramp.n - 1u;
      else
        rampcount = ramplen - 1u;
      endgain = 1.0f;
      ramp = &startramp;
    } else {
      ramplen = (uint32_t)std::max(1.0, endduration * f_sample);
      if(precalc)
        rampcount = endramp.n - 1u;
      else
        rampcount = ramplen - 1u;
      endgain = 0.0f;
      ramp = &endramp;
    }
  }
  tpprev = tp;
  if(!chunk.empty()) {
    uint32_t nch(chunk.size());
    uint32_t N(chunk[0].n);
    for(uint32_t k = 0; k < N; ++k) {
      if(precalc)
        gain = (endgain - startgain) * ramp->d[rampcount] + startgain;
      else
        gain = (endgain - startgain) *
                   (0.5f + 0.5f * cosf(rampcount * TASCAR_PIf / ramplen)) +
               startgain;
      if(rampcount)
        rampcount--;
      for(uint32_t ch = 0; ch < nch; ++ch)
        chunk[ch].d[k] *= gain;
    }
  }
}

REGISTER_AUDIOPLUGIN(transportramp_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
