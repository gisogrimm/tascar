/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

#include "jackclient.h"
#include "pluginprocessor.h"
#include "scene.h"
#include "session.h"

#define SQRT12 0.70710678118654757274f

class fader_t {
public:
  fader_t();
  void set_fs(double fs) { f_sample = fs; };
  void set_fade(double targetgain, double duration);
  void apply_gain(std::vector<TASCAR::wave_t>& sig);

private:
  double f_sample;
  // fade timer, is > 0 during fade:
  int32_t fade_timer;
  // time constant for fade:
  float fade_rate;
  // target gain at end of fade:
  float next_fade_gain;
  // current fade gain at time of fade update:
  float previous_fade_gain;
  // preliminary values to have atomic operations (correct?):
  float prelim_next_fade_gain;
  float prelim_previous_fade_gain;
  // current fade gain:
  float fade_gain;
};

fader_t::fader_t()
    : f_sample(1.0), fade_timer(0), fade_rate(1), next_fade_gain(1),
      previous_fade_gain(1), prelim_next_fade_gain(1),
      prelim_previous_fade_gain(1), fade_gain(1)
{
}

void fader_t::apply_gain(std::vector<TASCAR::wave_t>& sig)
{
  size_t n_channels(sig.size());
  if(n_channels > 0) {
    uint32_t psize(sig[0].size());
    for(uint32_t k = 0; k < psize; k++) {
      double g(1.0);
      if(fade_timer > 0) {
        --fade_timer;
        previous_fade_gain = prelim_previous_fade_gain;
        next_fade_gain = prelim_next_fade_gain;
        fade_gain =
            previous_fade_gain + (next_fade_gain - previous_fade_gain) *
                                     (0.5 + 0.5 * cos(fade_timer * fade_rate));
      }
      g *= fade_gain;
      for(uint32_t c = 0; c < n_channels; c++) {
        sig[c][k] *= g;
      }
    }
  }
}

void fader_t::set_fade(double targetgain, double duration)
{
  fade_timer = 0;
  prelim_previous_fade_gain = fade_gain;
  prelim_next_fade_gain = targetgain;
  fade_rate = TASCAR_PI / (duration * f_sample);
  fade_timer = std::max(1u, (uint32_t)(f_sample * duration));
}

class routemod_t : public TASCAR::module_base_t,
                   public TASCAR::Scene::route_t,
                   public jackc_transport_t,
                   public TASCAR::Scene::audio_port_t {
public:
  routemod_t(const TASCAR::module_cfg_t& cfg);
  ~routemod_t();
  virtual int process(jack_nframes_t, const std::vector<float*>&,
                      const std::vector<float*>&, uint32_t tp_frame,
                      bool tp_rolling);
  void configure();
  void release();
  void validate_attributes(std::string&) const;
  uint32_t get_num_input_ports();
  uint32_t get_num_output_ports();

private:
  uint32_t channels;
  std::vector<std::string> connect_out;
  double levelmeter_tc;
  TASCAR::levelmeter::weight_t levelmeter_weight;
  std::vector<float> caliblevel_in;
  TASCAR::plugin_processor_t plugins;
  TASCAR::pos_t nullpos;
  TASCAR::zyx_euler_t nullrot;
  std::vector<TASCAR::wave_t> sIn_tsc;
  bool bypass;
  pthread_mutex_t mtx_;
  fader_t fader;
};

void routemod_t::validate_attributes(std::string& msg) const
{
  TASCAR::module_base_t::validate_attributes(msg);
  TASCAR::Scene::route_t::validate_attributes(msg);
  TASCAR::Scene::audio_port_t::validate_attributes(msg);
  plugins.validate_attributes(msg);
}

int osc_routemod_mute(const char*, const char* types, lo_arg** argv, int argc,
                      lo_message, void* user_data)
{
  routemod_t* h((routemod_t*)user_data);
  if(h && (argc == 1) && (types[0] == 'i')) {
    h->set_mute(argv[0]->i);
    return 0;
  }
  return 1;
}

int osc_setfade(const char*, const char* types, lo_arg** argv, int argc,
                lo_message, void* user_data)
{
  fader_t* h((fader_t*)user_data);
  if(h && (argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
    h->set_fade(argv[0]->f, argv[1]->f);
    return 0;
  }
  return 1;
}

uint32_t routemod_t::get_num_input_ports()
{
  return jackc_t::get_num_input_ports();
}

uint32_t routemod_t::get_num_output_ports()
{
  return jackc_t::get_num_output_ports();
}

const std::string& fail_on_empty_name(const std::string& name,
                                      const tsccfg::node_t& e)
{
  if(name.empty())
    throw TASCAR::ErrMsg("The route name is empty (" +
                         tsccfg::node_get_path(e) + ")");
  return name;
}

routemod_t::routemod_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), TASCAR::Scene::route_t(TASCAR::module_base_t::e),
      jackc_transport_t(
          fail_on_empty_name(get_name(), TASCAR::module_base_t::e)),
      TASCAR::Scene::audio_port_t(TASCAR::module_base_t::e, false), channels(1),
      levelmeter_tc(2.0), levelmeter_weight(TASCAR::levelmeter::Z),
      plugins(TASCAR::module_base_t::e, get_name(), ""), bypass(true)
{
  pthread_mutex_init(&mtx_, NULL);
  TASCAR::module_base_t::GET_ATTRIBUTE(channels, "", "Number of channels");
  TASCAR::module_base_t::GET_ATTRIBUTE(
      connect_out, "", "Regular expressions of output port names");
  TASCAR::module_base_t::get_attribute("lingain", gain, "", "linear gain");
  TASCAR::module_base_t::GET_ATTRIBUTE(levelmeter_tc, "s",
                                       "Leq level metering time constant");
  TASCAR::module_base_t::GET_ATTRIBUTE_NOUNIT(levelmeter_weight,
                                              "level meter weighting");
  TASCAR::module_base_t::GET_ATTRIBUTE_DBSPL(caliblevel_in,
                                             "Input calibration levels");
  session->add_float_db("/" + get_name() + "/gain", &gain);
  session->add_float("/" + get_name() + "/lingain", &gain);
  session->add_method("/" + get_name() + "/mute", "i", osc_routemod_mute, this);
  session->add_method("/" + get_name() + "/fade", "ff", osc_setfade, &fader);
  configure_meter(levelmeter_tc, levelmeter_weight);
  set_ctlname("/" + get_name());
  for(uint32_t k = 0; k < channels; ++k) {
    char ctmp[1024];
    ctmp[1023] = 0;
    snprintf(ctmp, 1023, "in.%d", k);
    add_input_port(ctmp);
    snprintf(ctmp, 1023, "out.%d", k);
    add_output_port(ctmp);
    addmeter(get_srate());
  }
  for(auto c = caliblevel_in.size(); c < channels; ++c)
    caliblevel_in.push_back(1.0f);
  caliblevel_in.resize(channels);
  session->add_vector_float_dbspl("/" + get_name() + "/caliblevel_in",
                                  &caliblevel_in);
  std::string pref(session->get_prefix());
  session->set_prefix("/" + get_name());
  plugins.add_variables(session);
  plugins.add_licenses(session);
  session->set_prefix(pref);
  activate();
}

void routemod_t::configure()
{
  pthread_mutex_lock(&mtx_);
  fader.set_fs(f_sample);
  n_channels = channels;
  sIn_tsc.clear();
  for(uint32_t ch = 0; ch < channels; ++ch)
    sIn_tsc.push_back(TASCAR::wave_t(fragsize));
  plugins.prepare(cfg());
  TASCAR::module_base_t::configure();
  std::vector<std::string> con(get_connect());
  for(auto it = con.begin(); it != con.end(); ++it)
    *it = TASCAR::env_expand(*it);
  if(!con.empty()) {
    std::vector<std::string> ports(get_port_names_regexp(con));
    if(ports.empty())
      TASCAR::add_warning("No port \"" + TASCAR::vecstr2str(con) + "\" found.");
    uint32_t ip(0);
    for(auto it = ports.begin(); it != ports.end(); ++it) {
      if(ip < jackc_t::get_num_input_ports()) {
        connect_in(ip, *it, true, true);
        ++ip;
      }
    }
  }
  for(auto it = connect_out.begin(); it != connect_out.end(); ++it)
    *it = TASCAR::env_expand(*it);
  if(!connect_out.empty()) {
    std::vector<std::string> ports(
        get_port_names_regexp(connect_out, JackPortIsInput));
    if(ports.empty())
      TASCAR::add_warning("No input port matches \"" +
                          TASCAR::vecstr2str(connect_out) + "\".");
    uint32_t ip(0);
    for(auto it = ports.begin(); it != ports.end(); ++it) {
      if(ip < jackc_t::get_num_output_ports()) {
        jackc_t::connect_out(ip, *it, true);
        ++ip;
      }
    }
  }
  bypass = false;
  pthread_mutex_unlock(&mtx_);
}

void routemod_t::release()
{
  pthread_mutex_lock(&mtx_);
  bypass = true;
  TASCAR::module_base_t::release();
  // TASCAR::Scene::route_t::release();
  plugins.release();
  sIn_tsc.clear();
  pthread_mutex_unlock(&mtx_);
}

routemod_t::~routemod_t()
{
  deactivate();
  pthread_mutex_destroy(&mtx_);
}

int routemod_t::process(jack_nframes_t n, const std::vector<float*>& sIn,
                        const std::vector<float*>& sOut, uint32_t tp_frame,
                        bool tp_rolling)
{
  if(bypass)
    return 0;
  if(pthread_mutex_trylock(&mtx_) == 0) {
    TASCAR::transport_t tp;
    tp.rolling = tp_rolling;
    tp.session_time_samples = tp_frame;
    tp.session_time_seconds = (double)tp_frame / (double)srate;
    tp.object_time_samples = tp_frame;
    tp.object_time_seconds = (double)tp_frame / (double)srate;
    bool active(is_active(0));
    for(uint32_t ch = 0; ch < std::min(sIn.size(), sIn_tsc.size()); ++ch) {
      sIn_tsc[ch].copy(sIn[ch], n, caliblevel_in[ch]);
    }
    if(active) {
      plugins.process_plugins(sIn_tsc, nullpos, nullrot, tp);
      fader.apply_gain(sIn_tsc);
    }
    for(uint32_t ch = 0; ch < std::min(sIn.size(), sOut.size()); ++ch) {
      if(active) {
        for(uint32_t k = 0; k < n; ++k) {
          sOut[ch][k] = gain * sIn_tsc[ch].d[k];
        }
      } else {
        memset(sOut[ch], 0, n * sizeof(float));
      }
      rmsmeter[ch]->update(TASCAR::wave_t(n, sOut[ch]));
    }
    pthread_mutex_unlock(&mtx_);
  }
  return 0;
}

REGISTER_MODULE(routemod_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
