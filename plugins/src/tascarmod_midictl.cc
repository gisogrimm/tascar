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

#include "alsamidicc.h"
#include "session.h"
#include <thread>

float gain_to_gui(float dbGain)
{
  return powf(((dbGain + 200.0f) / 210.0f), 6.0f);
}

float gui_to_gain(float guiGain)
{
  return 210.0f * (powf(guiGain, 1.0f / 6.0f)) - 200.0f;
}

class midictl_vars_t : public TASCAR::module_base_t {
public:
  midictl_vars_t(const TASCAR::module_cfg_t& cfg);

protected:
  bool dumpmsg = false;
  std::string name;
  std::vector<std::string> controllers;
  std::vector<std::string> pattern;
  double min = 0.0;
  double max = 1.0;
  bool mutesounds = true;
  bool nonlinscale = false;
};

midictl_vars_t::midictl_vars_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg)
{
  GET_ATTRIBUTE_BOOL(dumpmsg, "Show unused messages in concole");
  GET_ATTRIBUTE_(name);
  GET_ATTRIBUTE(
      controllers, "",
      "MIDI CC controllers in form channel/param, both starting at zero");
  GET_ATTRIBUTE(pattern, "", "TASCAR controllers");
  GET_ATTRIBUTE(min, "dB", "scale minimum");
  GET_ATTRIBUTE(max, "dB", "scale maximum");
  GET_ATTRIBUTE_BOOL(mutesounds,
                     "Mute individual sounds instead of parent sources");
  GET_ATTRIBUTE_BOOL(
      nonlinscale,
      "Use non-linear mapping [-200,10] instead linear mapping of min-max");
}

class midictl_t : public midictl_vars_t, public TASCAR::midi_ctl_t {
public:
  midictl_t(const TASCAR::module_cfg_t& cfg);
  ~midictl_t();
  void configure();
  void release();
  virtual void emit_event(int channel, int param, int value);

private:
  void send_service();
  std::vector<uint16_t> controllers_;
  std::vector<uint8_t> values;
  std::vector<TASCAR::Scene::audio_port_t*> ports;
  std::vector<TASCAR::Scene::route_t*> routes;
  std::vector<TASCAR::Scene::sound_t*> sounds;
  std::thread srv;
  bool run_service;
  bool upload;
};

midictl_t::midictl_t(const TASCAR::module_cfg_t& cfg)
    : midictl_vars_t(cfg), TASCAR::midi_ctl_t(name), run_service(true),
      upload(false)
{
  for(uint32_t k = 0; k < controllers.size(); ++k) {
    size_t cpos(controllers[k].find("/"));
    if(cpos != std::string::npos) {
      uint32_t channel(atoi(controllers[k].substr(0, cpos).c_str()));
      uint32_t param(
          atoi(controllers[k]
                   .substr(cpos + 1, controllers[k].size() - cpos - 1)
                   .c_str()));
      controllers_.push_back(256 * channel + param);
      values.push_back(255);
    } else {
      throw TASCAR::ErrMsg("Invalid controller name " + controllers[k]);
    }
  }
  if(controllers_.size() == 0)
    throw TASCAR::ErrMsg("No controllers defined.");
  parse_xml_connections(e);
  session->add_bool_true(std::string("/") + name + "/upload", &upload);
  srv = std::thread(&midictl_t::send_service, this);
}

void midictl_t::configure()
{
  ports.clear();
  routes.clear();
  sounds.clear();
  if(session)
    ports = session->find_audio_ports(pattern);
  for(auto& it : ports) {
    TASCAR::Scene::route_t* r(dynamic_cast<TASCAR::Scene::route_t*>(it));
    TASCAR::Scene::sound_t* s = NULL;
    if(!r) {
      s = dynamic_cast<TASCAR::Scene::sound_t*>(it);
      if(s)
        r = dynamic_cast<TASCAR::Scene::route_t*>(s->parent);
    }
    routes.push_back(r);
    sounds.push_back(s);
  }
  start_service();
}

void midictl_t::release()
{
  stop_service();
  TASCAR::module_base_t::release();
}

midictl_t::~midictl_t()
{
  run_service = false;
  srv.join();
}

void midictl_t::send_service()
{
  while(run_service) {
    // wait for 100 ms:
    for(uint32_t k = 0; k < 100; ++k)
      if(run_service)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // usleep(1000);
    if(run_service) {
      for(uint32_t k = 0; k < ports.size(); ++k) {
        // gain:
        if(k < controllers_.size()) {
          float g(ports[k]->get_gain_db());
          if(nonlinscale) {
            g = std::max(0.0f, std::min(127.0f, 127.0f * gain_to_gui(g)));
          } else {
            g -= min;
            g *= 127 / (max - min);
            g = std::max(0.0f, std::min(127.0f, g));
          }
          uint8_t v(g);
          if((v != values[k]) || upload) {
            values[k] = v;
            int channel(controllers_[k] >> 8);
            int param(controllers_[k] & 0xff);
            send_midi(channel, param, v);
          }
        }
        // mute:
        uint32_t k1 = k + ports.size();
        if((k1 < controllers_.size())) {
          uint8_t v = 0;
          if(mutesounds && sounds[k]) {
            v = 127 * sounds[k]->get_mute();
          } else {
            if(routes[k])
              v = 127 * routes[k]->get_mute();
          }
          if((v != values[k1]) || upload) {
            values[k1] = v;
            int channel(controllers_[k1] >> 8);
            int param(controllers_[k1] & 0xff);
            send_midi(channel, param, v);
          }
        }
      }
      upload = false;
    }
  }
  for(uint32_t k = 0; k < controllers_.size(); ++k) {
    int channel(controllers_[k] >> 8);
    int param(controllers_[k] & 0xff);
    send_midi(channel, param, 0);
  }
}

void midictl_t::emit_event(int channel, int param, int value)
{
  uint32_t ctl(256 * channel + param);
  bool known = false;
  for(uint32_t k = 0; k < controllers_.size(); ++k) {
    if(controllers_[k] == ctl) {
      if(k < ports.size()) {
        values[k] = value;
        if(nonlinscale)
          ports[k]->set_gain_db(gui_to_gain(value / 127.0f));
        else
          ports[k]->set_gain_db(min + value * (max - min) / 127);
      } else {
        uint32_t k1(k - ports.size());
        if(k1 < routes.size()) {
          if(mutesounds && sounds[k1]) {
            values[k] = value;
            sounds[k1]->set_mute(value > 0);
          } else {
            if(routes[k1]) {
              values[k] = value;
              routes[k1]->set_mute(value > 0);
            }
          }
        }
      }
      known = true;
    }
  }
  if((!known) && dumpmsg) {
    char ctmp[256];
    snprintf(ctmp, 256, "%d/%d: %d", channel, param, value);
    ctmp[255] = 0;
    std::cout << ctmp << std::endl;
  } else {
    //
  }
}

REGISTER_MODULE(midictl_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
