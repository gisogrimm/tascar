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
#include "errorhandling.h"

class loopmachine_t : public TASCAR::audioplugin_base_t {
public:
  loopmachine_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  void release();
  ~loopmachine_t();

private:
  bool muteinput = false;
  double bpm = 120.0;
  double durationbeats = 4.0;
  double ramplen = 0.01;
  bool bypass = false;
  bool clear = false;
  bool record = false;
  float gain = 1.0f;
  double delaycomp = 0.0;
  // uint32_t loopcnt = 0;
  TASCAR::looped_wave_t* loop = NULL;
  TASCAR::wave_t* ramp = NULL;
  size_t rec_counter = 0;
  size_t ramp_counter = 0;
  size_t t_rec_counter = 0;
  size_t t_ramp_counter = 0;
  TASCAR::static_delay_t* delay = NULL;
  TASCAR::wave_t* delayed = NULL;
};

loopmachine_t::loopmachine_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(bpm, "", "Beats per minute");
  GET_ATTRIBUTE(durationbeats, "beats", "Record duration");
  GET_ATTRIBUTE(ramplen, "s", "Ramp length");
  GET_ATTRIBUTE_DB(gain, "Playback gain");
  GET_ATTRIBUTE_BOOL(bypass, "Start in bypass mode");
  GET_ATTRIBUTE(delaycomp, "s", "Delay compensation");
  GET_ATTRIBUTE_BOOL(muteinput, "Mute input while not recording");
}

void loopmachine_t::configure()
{
  if(n_channels != 1)
    TASCAR::add_warning("loopmachine will process only the first channel");
  if(n_channels == 0)
    throw TASCAR::ErrMsg("loopmachine requires at least one audio channel");
  if(delaycomp < 0)
    throw TASCAR::ErrMsg("Invalid delay compensation time: " +
                         TASCAR::to_string(delaycomp));
  audioplugin_base_t::configure();
  uint32_t period = (60 * (int64_t)f_sample) / (int64_t)bpm;
  loop = new TASCAR::looped_wave_t(durationbeats * period);
  loop->set_loop(0);
  ramp = new TASCAR::wave_t(f_sample * ramplen);
  for(size_t k = 0; k < ramp->n; ++k)
    ramp->d[k] = 0.5f + 0.5f * cosf(k * t_sample * TASCAR_PI / ramplen);
  delay = new TASCAR::static_delay_t(f_sample * delaycomp);
  delayed = new TASCAR::wave_t(n_fragment);
}

void loopmachine_t::release()
{
  audioplugin_base_t::release();
  delete loop;
  delete ramp;
  delete delay;
  delete delayed;
}

void loopmachine_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_bool_true("/clear", &clear, "clear current recording");
  srv->add_bool_true("/record", &record, "start recording");
  srv->add_bool("/bypass", &bypass, "bypass, 0 means loop is added to output");
  srv->add_float("/gain", &gain, "", "linear gain applied to loop");
  srv->add_float_db("/gaindb", &gain, "", "dB gain applied to loop");
  srv->add_bool("/muteinput", &muteinput, "mute the input (play only loop)");
  // srv->add_uint("/loopcnt", &loopcnt);
  srv->unset_variable_owner();
}

loopmachine_t::~loopmachine_t() {}

void loopmachine_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                               const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                               const TASCAR::transport_t&)
{
  if(chunk.size() == 0)
    return;
  auto& chunk_ = chunk[0];
  if(record) {
    record = false;
    rec_counter = loop->n;
    ramp_counter = ramp->n;
    t_rec_counter = 0;
    t_ramp_counter = 0;
    // loop->set_loop(loopcnt);
    loop->restart();
  }
  if(clear) {
    clear = false;
    loop->clear();
  }
  delayed->copy(chunk_);
  delay->operator()(*delayed);
  for(size_t k = 0; k < n_fragment; ++k) {
    if(rec_counter) {
      loop->d[t_rec_counter] = delayed->d[k];
      if(t_rec_counter < ramp->n)
        loop->d[t_rec_counter] *= 1.0f - ramp->d[t_rec_counter];
      --rec_counter;
      ++t_rec_counter;
    } else {
      if(muteinput)
        chunk_.d[k] = 0.0f;
      if(ramp_counter) {
        loop->d[t_ramp_counter] += (ramp->d[t_ramp_counter] * delayed->d[k]);
        --ramp_counter;
        ++t_ramp_counter;
      }
    }
  }
  if(bypass || (rec_counter > 0)) {
    loop->add_chunk_looped(0.0f, chunk_);
  } else {
    loop->add_chunk_looped(gain, chunk_);
  }
}

REGISTER_AUDIOPLUGIN(loopmachine_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
