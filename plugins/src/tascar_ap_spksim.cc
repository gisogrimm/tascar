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
#include <complex>

const std::complex<double> i(0.0, 1.0);

class spksim_t : public TASCAR::audioplugin_base_t {
public:
  spksim_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  ~spksim_t();

private:
  double scale = 0.5;
  double fres = 1200.0;
  double q = 0.8;
  double gain = 0.0;
  bool bypass = false;
  float wet = 1.0f;
  double b1;
  double b2;
  TASCAR::wave_t statex;
  TASCAR::wave_t statey1;
  TASCAR::wave_t statey2;
};

spksim_t::spksim_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), scale(0.5), fres(1200), q(0.8), gain(0),
      statex(0), statey1(0), statey2(0)
{
  GET_ATTRIBUTE(fres, "Hz", "Resonance frequency");
  GET_ATTRIBUTE(scale, "", "Distortion factor $s$");
  GET_ATTRIBUTE(q, "", "$q$-factor of the resonance filter");
  GET_ATTRIBUTE(gain, "dB", "Post-gain $g$");
  GET_ATTRIBUTE_BOOL(bypass, "Bypass plugin");
  GET_ATTRIBUTE(wet, "", "Wet (1) - dry (0) mixture gain");
}

void spksim_t::configure()
{
  audioplugin_base_t::configure();
  statex.resize(n_channels);
  statey1.resize(n_channels);
  statey2.resize(n_channels);
}

void spksim_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_double("/fres", &fres, "[1,10000]", "Resonance frequency in Hz");
  srv->add_double("/scale", &scale);
  srv->add_double("/q", &q, "]0,1[", "q-factor of the resonance filter");
  srv->add_double("/gain", &gain, "[-40,40]", "Post-gain in dB");
  srv->add_bool("/bypass", &bypass);
  srv->add_float("/wet", &wet, "[0,1]");
  srv->unset_variable_owner();
}

spksim_t::~spksim_t() {}

void spksim_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                          const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                          const TASCAR::transport_t&)
{
  if(bypass)
    return;
  const double farg(TASCAR_2PI * fres / f_sample);
  b1 = 2.0 * q * cos(farg);
  b2 = -q * q;
  std::complex<double> z(std::exp(i * farg));
  std::complex<double> z0(q * std::exp(-i * farg));
  double a1((1.0 - q) * (std::abs(z - z0)));
  double og(pow(10.0, 0.05 * gain));
  for(size_t ch = 0; ch < chunk.size(); ++ch) {
    TASCAR::wave_t& aud(chunk[0]);
    for(uint32_t k = 0; k < aud.n; ++k) {
      // input resonance filter:
      make_friendly_number_limited(aud.d[k]);
      double y(a1 * aud.d[k] + b1 * statey1.d[ch] + b2 * statey2.d[ch]);
      make_friendly_number_limited(y);
      statey2.d[ch] = statey1.d[ch];
      statey1.d[ch] = y;
      // non-linearity:
      y *= scale / (scale + fabs(y));
      // air coupling to velocity:
      aud.d[k] = wet * og * (y - statex.d[ch]) + (1.0f - wet) * aud.d[k];
      statex.d[ch] = y;
    }
  }
}

REGISTER_AUDIOPLUGIN(spksim_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
