/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

#include "acousticmodel.h"
#include "audioplugin.h"
#include "errorhandling.h"

class gainramp_t : public TASCAR::audioplugin_base_t {
public:
  gainramp_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  ~gainramp_t();
  void set_fade(float targetgain, float duration, float start);

private:
  float gain = 1.0f;
  // fade timer, is > 0 during fade:
  int32_t fade_timer = 0;
  // time constant for fade:
  float fade_rate = 1.0f;
  // target gain at end of fade:
  float next_fade_gain = 1.0f;
  // current fade gain at time of fade update:
  float previous_fade_gain = 1.0f;
  // preliminary values to have atomic operations (correct?):
  float prelim_next_fade_gain = 1.0f;
  float prelim_previous_fade_gain = 1.0f;
  // current fade gain:
  // float fade_gain = 1.0f;
  // start sample of next fade, or FADE_START_NOW
  uint64_t fade_startsample = 0u;
};

int osc_set_fade(const char*, const char* types, lo_arg** argv, int argc,
                 lo_message, void* user_data)
{
  gainramp_t* h((gainramp_t*)user_data);
  if(h && (argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
    h->set_fade(argv[0]->f, argv[1]->f, -1.0f);
    return 0;
  }
  return 1;
}

gainramp_t::gainramp_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), gain(1.0)
{
  bool has_gain = has_attribute("gain");
  GET_ATTRIBUTE_DB(gain, "gain");
  bool has_lingain = has_attribute("lingain");
  float lingain = 1.0f;
  GET_ATTRIBUTE(lingain, "", "lingain");
  if(has_lingain) {
    gain = lingain;
    if(has_gain) {
      TASCAR::add_warning("gain plugin was configured with \"gain\" and "
                          "\"lingain\" attribute, using \"lingain\".");
    }
  }
}

void gainramp_t::set_fade(float targetgain, float duration, float start)
{
  fade_timer = 0;
  duration = std::max((float)t_sample, duration);
  if(start < 0)
    fade_startsample = FADE_START_NOW;
  else
    fade_startsample = (uint64_t)(f_sample * start);
  prelim_previous_fade_gain = gain;
  prelim_next_fade_gain = targetgain;
  fade_rate = TASCAR_PIf * (float)t_sample / duration;
  fade_timer = std::max(1u, (uint32_t)(f_sample * duration));
}

void gainramp_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float_db("/gain", &gain);
  srv->add_float("/lingain", &gain);
  srv->add_method("/fade", "ff", osc_set_fade, this);
  srv->unset_variable_owner();
}

gainramp_t::~gainramp_t() {}

void gainramp_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                            const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                            const TASCAR::transport_t& ltp)
{
  if(!chunk.empty()) {
    uint32_t nch(chunk.size());
    uint32_t N(chunk[0].n);
    for(uint32_t k = 0; k < N; ++k) {
      if((fade_timer > 0) &&
         ((fade_startsample == FADE_START_NOW) ||
          ((fade_startsample <= ltp.session_time_samples + k) &&
           ltp.rolling))) {
        --fade_timer;
        previous_fade_gain = prelim_previous_fade_gain;
        next_fade_gain = prelim_next_fade_gain;
        gain = previous_fade_gain +
               (next_fade_gain - previous_fade_gain) *
                   (0.5f + 0.5f * cosf((float)fade_timer * fade_rate));
      }
      for(uint32_t ch = 0; ch < nch; ++ch)
        chunk[ch].d[k] *= gain;
    }
  }
}

REGISTER_AUDIOPLUGIN(gainramp_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
