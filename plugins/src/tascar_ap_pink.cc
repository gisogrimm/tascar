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
#include "fft.h"
#include <random>

const std::complex<double> i(0.0, 1.0);

class pink_t : public TASCAR::audioplugin_base_t {
public:
  pink_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  void release();
  ~pink_t();

private:
  double fmin = 62.5;
  double fmax = 4000.0;
  double level = 0.001;
  double period = 4.0;
  double alpha = 2.0;
  bool use_transport = false;
  bool mute = false;
  std::vector<TASCAR::looped_wave_t> pink;
};

pink_t::pink_t(const TASCAR::audioplugin_cfg_t& cfg) : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(fmin, "Hz", "Minimum frequency");
  GET_ATTRIBUTE(fmax, "Hz", "Maximum frequency");
  GET_ATTRIBUTE_DBSPL(level, "RMS level");
  GET_ATTRIBUTE(period, "s", "Period time of frozen noise");
  GET_ATTRIBUTE_BOOL(use_transport, "Play only if transport is running");
  GET_ATTRIBUTE_BOOL(mute, "load muted");
  GET_ATTRIBUTE(alpha, "", "Frequency exponent alpha, 1 = pink");
}

void pink_t::configure()
{
  TASCAR::audioplugin_base_t::configure();
  uint32_t fftlen(period * f_sample);
  // create 4 buffers, wxyz
  pink.clear();
  pink.emplace_back(fftlen);
  pink.emplace_back(fftlen);
  pink.emplace_back(fftlen);
  pink.emplace_back(fftlen);
  TASCAR::fft_t fft(fftlen);
  TASCAR::spec_t sw(fft.s.n_);
  TASCAR::spec_t sx(fft.s.n_);
  TASCAR::spec_t sy(fft.s.n_);
  TASCAR::spec_t sz(fft.s.n_);
  std::mt19937 gen(1);
  std::uniform_real_distribution<double> dis(0.0, TASCAR_2PI);
  std::uniform_real_distribution<double> disx(-1.0, 1.0);
  for(uint32_t kf = 0; kf < sw.n_; ++kf) {
    double f((double)kf * f_sample / (double)fftlen);
    if((f >= fmin) && (f <= fmax))
      sw.b[kf] = 1.0 / pow(f, 0.5 * alpha) * std::exp(i * dis(gen));
    else
      sw.b[kf] = 0;
    TASCAR::pos_t p;
    p.z = disx(gen);
    p.y = disx(gen);
    p.x = disx(gen);
    p.normalize();
    // FuMa normalization:
    p *= sqrt(2.0);
    sx.b[kf] = (float)(p.x) * sw.b[kf];
    sy.b[kf] = (float)(p.y) * sw.b[kf];
    sz.b[kf] = (float)(p.z) * sw.b[kf];
  }
  // w channel:
  fft.s.copy(sw);
  fft.ifft();
  pink[0].copy(fft.w);
  // x channel:
  fft.s.copy(sx);
  fft.ifft();
  pink[1].copy(fft.w);
  // y channel:
  fft.s.copy(sy);
  fft.ifft();
  pink[2].copy(fft.w);
  // z channel:
  fft.s.copy(sz);
  fft.ifft();
  pink[3].copy(fft.w);
  // level normalization:
  double gain(1.0 / pink[0].rms());
  TASCAR::wave_t tmp(n_fragment);
  for(auto it = pink.begin(); it != pink.end(); ++it) {
    it->set_loop(0);
    (*it) *= gain;
    it->add_chunk_looped(level, tmp);
  }
}

void pink_t::release()
{
  TASCAR::audioplugin_base_t::release();
}

pink_t::~pink_t() {}

void pink_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_double_dbspl("/level", &level);
  srv->add_bool("/use_transport", &use_transport);
  srv->add_bool("/mute", &mute);
  srv->unset_variable_owner();
}

void pink_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                        const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                        const TASCAR::transport_t& tp)
{
  if(mute)
    return;
  if((!use_transport) || tp.rolling) {
    for(uint32_t k = 0; k < std::min(chunk.size(), pink.size()); ++k) {
      if(use_transport)
        pink[k].set_iposition(tp.object_time_samples);
      pink[k].add_chunk_looped(level, chunk[k]);
    }
  }
}

REGISTER_AUDIOPLUGIN(pink_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
