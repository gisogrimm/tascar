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
#include "delayline.h"

class feedbackdelay_t : public TASCAR::audioplugin_base_t {
public:
  feedbackdelay_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~feedbackdelay_t();

private:
  uint64_t maxdelay = 44100;
  float f = 1000.0f;
  float feedback = 0.5f;
  float wet = 1.0f;
  float dry = 1.0f;
  TASCAR::varidelay_t* dl;
};

feedbackdelay_t::feedbackdelay_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), dl(NULL)
{
  GET_ATTRIBUTE(maxdelay, "samples", "Maximum delay line length");
  GET_ATTRIBUTE(f, "Hz", "Resonance frequency");
  GET_ATTRIBUTE(feedback, "", "Linear feedback gain");
  GET_ATTRIBUTE(wet, "", "Linear gain of input to delayline");
  GET_ATTRIBUTE(dry, "", "Linear gain of direct input");
  dl = new TASCAR::varidelay_t(maxdelay, 1.0, 1.0, 0, 1);
}

feedbackdelay_t::~feedbackdelay_t()
{
  delete dl;
}

void feedbackdelay_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/f", &f, "]0,8000]", "Resonance frequency");
  srv->add_float("/feedback", &feedback, "]-1,1[", "Linear feedback gain");
  srv->add_float("/wet", &wet, "[0,1]", "Linear gain of input to delayline");
  srv->add_float("/dry", &dry, "[0,1]", "Linear gain of direct input");
  srv->unset_variable_owner();
}

void feedbackdelay_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                                 const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                                 const TASCAR::transport_t&)
{
  // sample delay; subtract 1 to account for order of read/write:
  double d(f_sample / f - 1.0);
  // operate only on first channel:
  float* vsigbegin(chunk[0].d);
  float* vsigend(vsigbegin + chunk[0].n);
  for(float* v = vsigbegin; v != vsigend; ++v) {
    float v_out(dl->get(d));
    dl->push((v_out + wet * *v) * feedback);
    *v = dry * *v + v_out;
  }
}

REGISTER_AUDIOPLUGIN(feedbackdelay_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
