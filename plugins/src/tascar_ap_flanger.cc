/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2022 Giso Grimm
 * Copyright (c) 2024 Giso Grimm
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
#include "errorhandling.h"
#include <complex>

class flanger_t : public TASCAR::audioplugin_base_t {
public:
  flanger_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~flanger_t();

private:
  uint64_t maxdelay = 44100;
  float wet = 1.0f;
  float modf = 1.0f;
  float dmin = 0.0f;
  float dmax = 0.01f;
  float feedback = 0.0f;
  TASCAR::varidelay_t* dl;
  std::complex<float> phase = 1.0f;
  std::complex<float> i = {0.0f, 1.0f};
};

flanger_t::flanger_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), dl(NULL)
{
  GET_ATTRIBUTE(maxdelay, "samples", "Maximum delay line length");
  GET_ATTRIBUTE(wet, "", "Linear gain of input to delayline");
  GET_ATTRIBUTE(modf, "Hz", "Modulation frequency");
  GET_ATTRIBUTE(dmin, "s", "Lower bound of delay");
  GET_ATTRIBUTE(dmax, "s", "Upper bound of delay");
  GET_ATTRIBUTE(feedback, "", "Feedback, must be between 0 and 0.999");
  if((feedback < 0.0f) || (feedback > 0.999))
    throw TASCAR::ErrMsg("Feedback parameter must be between 0 and 0.999");
  dl = new TASCAR::varidelay_t(maxdelay, 1.0, 1.0, 0, 1);
}

flanger_t::~flanger_t()
{
  delete dl;
}

void flanger_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/wet", &wet, "[0,1]", "Linear gain of input to delayline");
  srv->add_float("/modf", &modf, "[0,100]", "Modulation frequency");
  srv->add_float("/dmin", &dmin, "[0,1]", "Lower bound of delay, in s");
  srv->add_float("/dmax", &dmax, "[0,1]", "Upper bound of delay, in s");
  srv->add_float("/feedback", &feedback, "[0,0.999]", "Feedback");
  srv->unset_variable_owner();
}

void flanger_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                           const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                           const TASCAR::transport_t&)
{
  std::complex<float> dphase =
      std::exp(i * TASCAR_2PIf * modf * (float)t_sample);
  // operate only on first channel:
  float* vsigbegin(chunk[0].d);
  float* vsigend(vsigbegin + chunk[0].n);
  for(float* v = vsigbegin; v != vsigend; ++v) {
    phase *= dphase;
    if(TASCAR::is_denormal(std::real(phase))) {
      DEBUG(std::real(phase));
      phase = 1.0f;
    }
    float delay_s = dmin + (0.5f + 0.5f * std::real(phase)) * (dmax - dmin);
    // sample delay; subtract 1 to account for order of read/write:
    double d = std::max(0.0, f_sample * delay_s);
    float v_out(dl->get(d));
    dl->push((1.0f - feedback) * wet * *v + feedback * v_out);
    *v += v_out;
  }
  phase /= std::abs(phase);
}

REGISTER_AUDIOPLUGIN(flanger_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
