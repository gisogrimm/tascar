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

class tubesim_t : public TASCAR::audioplugin_base_t {
public:
  tubesim_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~tubesim_t();
  void wetfade_fun(float newwet, float duration)
  {
    wetfade = wet;
    uint32_t wftimer = f_sample * duration;
    dwetfade = (newwet - wetfade) / wftimer;
    targetwet = newwet;
    wetfade_timer = wftimer;
  }
  inline float tubeval(float x)
  {
    x += offset;
    if(x < 0.0f)
      x = 0.0f;
    x *= sqrtf(x);
    if(-x != saturation)
      x /= x + saturation;
    return x;
  }
  static int osc_wetfade(const char*, const char* fmt, lo_arg** argv, int,
                         lo_message, void* user_data)
  {
    if((fmt[0] == 'f') && (fmt[1] == 'f'))
      ((tubesim_t*)user_data)->wetfade_fun(argv[0]->f, argv[1]->f);
    return 0;
  }

private:
  float pregain = 1.0f;
  float offset = 0.5f;
  float saturation = 0.5f;
  float postgain = 1.0f;
  bool bypass = false;
  float wet = 1.0f;
  float wetfade = 1.0f;
  float targetwet = 1.0f;
  float dwetfade = 0.0f;
  uint32_t wetfade_timer = 0u;
};

tubesim_t::tubesim_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE_DB(pregain, "Pre-gain $g_i$");
  GET_ATTRIBUTE(offset, "", "Input offset $x_0$");
  GET_ATTRIBUTE_DB(saturation, "Saturation parameter $s$");
  GET_ATTRIBUTE_DB(postgain, "Post-gain $g_o$");
  GET_ATTRIBUTE_BOOL(bypass, "Bypass plugin");
  GET_ATTRIBUTE(wet, "", "Wet (1) - dry (0) mixture gain");
}

void tubesim_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float_db("/pregain", &pregain, "[-10,50]", "Input gain in dB");
  srv->add_float_db("/postgain", &postgain, "[-50,10]", "Output gain in dB");
  srv->add_float_db("/saturation", &saturation, "[-40,0]",
                    "Saturation threshold in dB");
  srv->add_float("/offset", &offset, "[0,1]", "Input offset");
  srv->add_bool("/bypass", &bypass);
  srv->add_float("/wet", &wet, "[0,1]");
  srv->add_method("/wet", "ff", &tubesim_t::osc_wetfade, this);
  srv->unset_variable_owner();
}

tubesim_t::~tubesim_t() {}

void tubesim_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                           const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                           const TASCAR::transport_t&)
{
  if(bypass)
    return;
  float oshift = tubeval(0.0f);
  auto channels = chunk.size();
  if(channels) {
    auto numframes = chunk[0].n;
    for(uint32_t k = 0; k < numframes; ++k) {
      if(wetfade_timer) {
        wetfade += dwetfade;
        wet = wetfade;
        --wetfade_timer;
        if(!wetfade_timer) {
          wet = targetwet;
        }
      }
      for(auto& aud : chunk) {
        aud.d[k] = wet * (tubeval(aud.d[k] * pregain) - oshift) * postgain +
                   (1.0f - wet) * aud.d[k];
        if(TASCAR::is_denormal(aud.d[k]))
          aud.d[k] = 0.0f;
      }
    }
  }
}

REGISTER_AUDIOPLUGIN(tubesim_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
