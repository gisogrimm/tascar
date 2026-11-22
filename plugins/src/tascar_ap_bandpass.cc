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
#include "filterclass.h"

class bandpassplugin_t : public TASCAR::audioplugin_base_t {
public:
  bandpassplugin_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  void add_variables(TASCAR::osc_server_t* srv);
  ~bandpassplugin_t();
  static int osc_fminfade(const char*, const char* fmt, lo_arg** argv, int,
                          lo_message, void* user_data)
  {
    if((fmt[0] == 'f') && (fmt[1] == 'f'))
      ((bandpassplugin_t*)user_data)->fminfade(argv[0]->f, argv[1]->f);
    return 0;
  }
  static int osc_fmaxfade(const char*, const char* fmt, lo_arg** argv, int,
                          lo_message, void* user_data)
  {
    if((fmt[0] == 'f') && (fmt[1] == 'f'))
      ((bandpassplugin_t*)user_data)->fmaxfade(argv[0]->f, argv[1]->f);
    return 0;
  }
  void fminfade(float newval, float duration)
  {
    fmin_fade = fmin;
    uint32_t wtimer = f_fragment * duration;
    fmin_final = newval;
    dfmin_fade = (newval - fmin_fade) / wtimer;
    fmin_timer = wtimer;
  }
  void fmaxfade(float newval, float duration)
  {
    fmax_fade = fmax;
    uint32_t wtimer = f_fragment * duration;
    fmax_final = newval;
    dfmax_fade = (newval - fmax_fade) / wtimer;
    fmax_timer = wtimer;
  }

private:
  float fmin = 100.0f;
  float fmax = 20000.0f;
  float fmin_fade = 100.0f;
  float fmax_fade = 20000.0f;
  float fmin_final = 100.0f;
  float fmax_final = 20000.0f;
  uint32_t fmin_timer = 0u;
  uint32_t fmax_timer = 0u;
  float dfmin_fade = 0.0f;
  float dfmax_fade = 0.0f;
  bool bypass = false;
  std::vector<TASCAR::bandpassf_t*> bp;
};

bandpassplugin_t::bandpassplugin_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(fmin, "Hz", "Minimum frequency");
  GET_ATTRIBUTE(fmax, "Hz", "Maximum frequency");
  GET_ATTRIBUTE_BOOL(bypass, "bypass plugin");
}

void bandpassplugin_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/fmin", &fmin, "]0,20000]", "Lower cutoff frequency in Hz");
  srv->add_float("/fmax", &fmax, "]0,20000]", "Upper cutoff frequency in Hz");
  srv->add_method("/fmin", "ff", &bandpassplugin_t::osc_fminfade, this, true,
                  false, "",
                  "Fade the lower cutoff frequency, first parameter is new "
                  "frequency in Hz, second parameter is fade duration in s");
  srv->add_method("/fmax", "ff", &bandpassplugin_t::osc_fmaxfade, this, true,
                  false, "",
                  "Fade the upper cutoff frequency, first parameter is new "
                  "frequency in Hz, second parameter is fade duration in s");
  srv->add_bool("/bypass", &bypass);
  srv->unset_variable_owner();
}

void bandpassplugin_t::configure()
{
  audioplugin_base_t::configure();
  for(uint32_t ch = 0; ch < n_channels; ++ch)
    bp.push_back(new TASCAR::bandpassf_t(fmin, fmax, (float)f_sample));
}

void bandpassplugin_t::release()
{
  audioplugin_base_t::release();
  for(auto it = bp.begin(); it != bp.end(); ++it)
    delete(*it);
  bp.clear();
}

bandpassplugin_t::~bandpassplugin_t() {}

void bandpassplugin_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                                  const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                                  const TASCAR::transport_t&)
{
  if(bypass)
    return;
  if(fmin_timer) {
    fmin_fade += dfmin_fade;
    fmin = fmin_fade;
    --fmin_timer;
    if(!fmin_timer)
      fmin = fmin_final;
  }
  if(fmax_timer) {
    fmax_fade += dfmax_fade;
    fmax = fmax_fade;
    --fmax_timer;
    if(!fmax_timer)
      fmax = fmax_final;
  }
  for(size_t k = 0; k < chunk.size(); ++k) {
    bp[k]->set_range(fmin, fmax);
    bp[k]->filter(chunk[k]);
  }
}

REGISTER_AUDIOPLUGIN(bandpassplugin_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
