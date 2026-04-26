/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 * Copyright (c) 2026 Giso Grimm
 */
/*
 * TASCAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * TASCAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ola.h"
#include "session.h"
#include <stdlib.h>

// const std::complex<float> i_f(0.0, 1.0);
const std::complex<double> i_d(0.0, 1.0);

class sustain_vars_t : public TASCAR::module_base_t {
public:
  sustain_vars_t(const TASCAR::module_cfg_t& cfg);
  ~sustain_vars_t() {};

protected:
  std::string id = "sustain";
  std::string oscprefix = "";
  float tau_sustain = 20.0f;
  float tau_envelope = 1.0f;
  float bass = 0.0f;
  float bassratio = 2.0f;
  float wet = 1.0f;
  uint32_t wlen = 8192;
  float fcut = 40.0f;
  double gain = 1.0;
  bool delayenvelope = false;
};

sustain_vars_t::sustain_vars_t(const TASCAR::module_cfg_t& cfg)
    : TASCAR::module_base_t(cfg)
{
  GET_ATTRIBUTE(id, "", "ID used for jack and OSC");
  GET_ATTRIBUTE(oscprefix, "", "Prefix used in OSC");
  GET_ATTRIBUTE(tau_sustain, "s", "Clustering time constant");
  GET_ATTRIBUTE(tau_envelope, "s", "Envelope tracking time constant");
  GET_ATTRIBUTE(wet, "", "Wet-dry ratio");
  GET_ATTRIBUTE(wlen, "samples", "Window length");
  GET_ATTRIBUTE(bass, "", "Linear gain of subsonic component");
  GET_ATTRIBUTE(bassratio, "", "Frequency ratio of subsonic component");
  GET_ATTRIBUTE(fcut, "Hz", "Low-cut edge frequency");
  GET_ATTRIBUTE_DB(gain, "Gain");
  GET_ATTRIBUTE_BOOL(delayenvelope, "Delay envelope to match processed signal");
  // test for current jack fragment size and adapt wlen to be a multiple of it:
  jackc_portless_t jc(id + "_hossustain__fragsizetest");
  auto frs = jc.get_fragsize();
  uint32_t wlen_final = ceilf((float)wlen / (float)frs) * frs;
  if(wlen != wlen_final)
    TASCAR::add_warning(
        "Inner fragment size was adjusted from " + std::to_string(wlen) +
        " samples to " + std::to_string(wlen_final) +
        " samples in order to match jack fragment size of " +
        std::to_string(frs) + " samples per block (hossustain)");
  wlen = wlen_final;
}

class sustain_t : public sustain_vars_t, public jackc_db_t {
public:
  sustain_t(const TASCAR::module_cfg_t& cfg);
  ~sustain_t();
  virtual int inner_process(jack_nframes_t nframes,
                            const std::vector<float*>& inBuffer,
                            const std::vector<float*>& outBuffer);
  virtual int process(jack_nframes_t, const std::vector<float*>&,
                      const std::vector<float*>&);
  static int osc_apply(const char* path, const char* types, lo_arg** argv,
                       int argc, lo_message msg, void* user_data);
  void set_apply(float t);

protected:
  TASCAR::ola_t ola;
  TASCAR::wave_t absspec;
  double Lin;
  double Lout;
  uint32_t t_apply;
  float deltaw;
  float currentw;
  uint32_t fcut_int;
};

int sustain_t::osc_apply(const char*, const char*, lo_arg** argv, int,
                         lo_message, void* user_data)
{
  ((sustain_t*)user_data)->set_apply(argv[0]->f);
  return 0;
}

void sustain_t::set_apply(float t)
{
  deltaw = 0;
  t_apply = 0;
  if(t >= 0) {
    uint32_t tau(std::max(1, (int32_t)(srate * t)));
    deltaw = (wet - currentw) / (float)tau;
    t_apply = tau;
  }
}

int sustain_t::process(jack_nframes_t n, const std::vector<float*>& vIn,
                       const std::vector<float*>& vOut)
{
  jackc_db_t::process(n, vIn, vOut);
  TASCAR::wave_t w_in(n, vIn[0]);
  TASCAR::wave_t w_out(n, vOut[0]);
  if(!delayenvelope) {
    float env_c1(0);
    if(tau_envelope > 0)
      env_c1 = exp(-1.0 / (tau_envelope * (double)srate));
    float env_c2(1.0f - env_c1);
    // envelope reconstruction:
    for(uint32_t k = 0; k < w_in.size(); ++k) {
      Lin *= env_c1;
      Lin += env_c2 * w_in[k] * w_in[k];
      Lout *= env_c1;
      Lout += env_c2 * w_out[k] * w_out[k];
      if(TASCAR::is_denormal(Lin))
        Lin = 0.0;
      if(TASCAR::is_denormal(Lout))
        Lout = 0.0;
      if(Lout > 0)
        w_out[k] *= sqrt(Lin / Lout);
      if(TASCAR::is_denormal(w_out[k]))
        w_out[k] = 0.0f;
    }
  }
  for(uint32_t k = 0; k < w_in.size(); ++k) {
    if(t_apply) {
      t_apply--;
      currentw += deltaw;
    }
    w_out[k] *= gain * currentw;
    w_out[k] += (1.0f - std::max(0.0f, currentw)) * w_in[k];
  }
  return 0;
}

int sustain_t::inner_process(jack_nframes_t n, const std::vector<float*>& vIn,
                             const std::vector<float*>& vOut)
{
  fcut_int = (fcut / (0.5 * f_sample)) * ola.s.size();
  TASCAR::wave_t w_in(n, vIn[0]);
  TASCAR::wave_t w_out(n, vOut[0]);
  ola.process(w_in);
  float sus_c1(0);
  if(tau_sustain > 0)
    sus_c1 = exp(-1.0 / (tau_sustain * (double)srate / (double)(w_in.size())));
  float sus_c2(1.0f - sus_c1);
  ola.s *= sus_c2;
  absspec *= sus_c1;
  float br(1.0f / std::max(1.0f, bassratio));
  for(uint32_t k = ola.s.size(); k--;) {
    absspec[k] += std::abs(ola.s[k]);
    if((bass > 0))
      absspec[k * br] += bass * std::abs(ola.s[k]);
    ola.s[k] =
        (double)(absspec[k]) * std::exp(i_d * (TASCAR::drand() * TASCAR_2PI));
    if(k < fcut_int)
      ola.s[k] = 0;
  }
  ola.ifft(w_out);
  if(delayenvelope) {
    TASCAR::wave_t w_in(n, vIn[0]);
    TASCAR::wave_t w_out(n, vOut[0]);
    float env_c1(0);
    if(tau_envelope > 0)
      env_c1 = exp(-1.0 / (tau_envelope * (double)srate));
    float env_c2(1.0f - env_c1);
    // envelope reconstruction:
    for(uint32_t k = 0; k < w_in.size(); ++k) {
      Lin *= env_c1;
      Lin += env_c2 * w_in[k] * w_in[k];
      Lout *= env_c1;
      Lout += env_c2 * w_out[k] * w_out[k];
      if(Lout > 0)
        w_out[k] *= sqrt(Lin / Lout);
    }
  }
  return 0;
}

sustain_t::sustain_t(const TASCAR::module_cfg_t& cfg)
    : sustain_vars_t(cfg), jackc_db_t(id, wlen),
      ola(2 * wlen, 2 * wlen, wlen, TASCAR::stft_t::WND_HANNING,
          TASCAR::stft_t::WND_RECT, 0.5, TASCAR::stft_t::WND_SQRTHANN),
      absspec(ola.s.size()), Lin(0), Lout(0), t_apply(0), deltaw(0),
      currentw(0), fcut_int(0)
{
  add_input_port("in");
  add_output_port("out");
  session->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  session->add_float("/" + oscprefix + id + "/tau_sus", &tau_sustain, "[0,]",
                     "Sets the decay time for the frozen spectral texture.");
  session->add_float("/" + oscprefix + id + "/tau_env", &tau_envelope, "[0,]",
                     "Adjusts the speed of the envelope follower matching the "
                     "output to the input.");
  session->add_float(
      "/" + oscprefix + id + "/bass", &bass, "",
      "Controls the gain of the added subsonic low-frequency component.");
  session->add_float("/" + oscprefix + id + "/bassratio", &bassratio, "[1,]",
                     "Sets the frequency ratio for the added bass component.");
  session->add_float("/" + oscprefix + id + "/fcut", &fcut, "",
                     "Defines the low-cut edge frequency to remove high "
                     "frequencies from the processed signal.");
  session->add_double_db("/" + oscprefix + id + "/gain", &gain, "",
                         "Sets the overall output amplification.");
  session->add_float("/" + oscprefix + id + "/wet", &wet, "",
                     "Adjusts the balance between the processed effect and the "
                     "dry input signal.");
  session->add_method("/" + oscprefix + id + "/wetapply", "f",
                      &sustain_t::osc_apply, this,
                      "Smoothly transitions the wet/dry mix to a new value "
                      "over a specified time.");
  session->add_bool("/" + oscprefix + id + "/delayenvelope", &delayenvelope,
                    "Toggles whether the envelope follower is delayed to match "
                    "the processed signal latency.");
  session->unset_variable_owner();
  activate();
}

sustain_t::~sustain_t()
{
  deactivate();
}

REGISTER_MODULE(sustain_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * compile-command: "make -C .."
 * End:
 */
