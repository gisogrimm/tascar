/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 * Copyright (c) 2025 Giso Grimm
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
  std::string connect;
  std::vector<std::string> pattern;
  std::string mainctl;
  int32_t banksize = 8;
};

midictl_vars_t::midictl_vars_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg)
{
  GET_ATTRIBUTE_BOOL(dumpmsg, "Show unused messages in concole");
  GET_ATTRIBUTE(name, "",
                "Controller name used for MIDI port and OSC interface");
  GET_ATTRIBUTE(connect, "", "ALSA midi port connection, e.g., BCF2000:0");
  GET_ATTRIBUTE(pattern, "", "TASCAR controllers");
  GET_ATTRIBUTE(mainctl, "", "TASCAR main controller");
  GET_ATTRIBUTE(banksize, "", "Number of faders per bank");
}

class mcu_ctl_t : public midictl_vars_t, public TASCAR::midi_ctl_t {
public:
  mcu_ctl_t(const TASCAR::module_cfg_t& cfg);
  ~mcu_ctl_t();
  void configure();
  void release();
  virtual void emit_event(int channel, int param, int value);
  virtual void emit_event_note(int, int, int);
  virtual void emit_event_mmc(uint8_t, uint8_t);

private:
  void send_service();
  std::vector<int> fader_state;
  int mainfader_state = -8193;
  std::vector<int> fader_is_moving;
  std::vector<int> meter_state;
  std::vector<int> mute_state;
  std::vector<TASCAR::Scene::audio_port_t*> ports;
  std::vector<TASCAR::Scene::route_t*> routes;
  std::vector<TASCAR::Scene::sound_t*> sounds;
  std::vector<TASCAR::Scene::audio_port_t*> mainport;
  std::thread srv;
  bool run_service = true;
  bool upload = true;
  std::mutex mtx;
  int32_t bank_ofs = 0;
  std::set<float> vu_thresholds = {-100.0f, -60.0f, -50.0f, -40.0f, -30.0f,
                                   -20.0f,  -14.0f, -10.0f, -8.0f,  -6.0f,
                                   -4.0f,   -2.0f,  -0.0f};
  char msg_sysex[1024];
};

mcu_ctl_t::mcu_ctl_t(const TASCAR::module_cfg_t& cfg)
    : midictl_vars_t(cfg), TASCAR::midi_ctl_t(name)
{
  set_nonblock(true);
  if(!connect.empty()) {
    connect_input(connect, true);
    connect_output(connect, true);
  }
  session->add_bool_true(std::string("/") + name + "/upload", &upload);
  srv = std::thread(&mcu_ctl_t::send_service, this);
}

void mcu_ctl_t::configure()
{
  std::lock_guard<std::mutex> lock{mtx};
  ports.clear();
  routes.clear();
  sounds.clear();
  if(session) {
    auto aports = session->find_audio_ports(pattern);
    // for(const auto& p : aports) {
    //   DEBUG(p->get_ctlname());
    // }
  }
  if(session) {
    ports = session->find_audio_ports(pattern);
    mainport = session->find_audio_ports({mainctl});
    DEBUG(mainport.size());
  }
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
  // for(const auto& r : routes) {
  //   if(r)
  //     DEBUG(r->get_name());
  //   else
  //     DEBUG("");
  // }
  // for(const auto& r : sounds) {
  //   if(r)
  //     DEBUG(r->get_ctlname());
  //   else
  //     DEBUG("");
  // }
  fader_state.resize(routes.size());
  fader_is_moving.resize(routes.size());
  meter_state.resize(routes.size());
  mute_state.resize(routes.size());
  for(auto& state : fader_state)
    state = -8193;
  for(auto& state : mute_state)
    state = false;
  for(auto& ismov : fader_is_moving)
    ismov = false;
  for(auto& mstat : meter_state)
    mstat = -2000;
  start_service();
  TASCAR::module_base_t::configure();
}

void mcu_ctl_t::release()
{
  stop_service();
  TASCAR::module_base_t::release();
}

mcu_ctl_t::~mcu_ctl_t()
{
  run_service = false;
  srv.join();
}

void mcu_ctl_t::send_service()
{
  while(run_service) {
    // wait for 10 ms:
    for(uint32_t k = 0; k < 10; ++k)
      if(run_service)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(run_service) {
      std::lock_guard<std::mutex> lock{mtx};
      if(mainport.size()) {
        // main:
        // fader states:
        float g(mainport[0]->get_gain_db());
        g = std::max(-8191.0f,
                     std::min(8191.0f, 16384.0f * gain_to_gui(g) - 8192.0f));
        int v = g;
        if((v != mainfader_state) || upload) {
          mainfader_state = v;
          send_midi_pitchbend(8, 0, v);
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
          send_midi_pitchbend(8, 0, v);
        }
      }
      for(int32_t k = 0; k < (int)(ports.size()); ++k) {
        // gain:
        if((k - bank_ofs < banksize) && (k >= bank_ofs)) {
          // fader states:
          float g(ports[k]->get_gain_db());
          g = std::max(-8191.0f,
                       std::min(8191.0f, 16384.0f * gain_to_gui(g) - 8192.0f));
          int v = g;
          if((v != fader_state[k]) || upload) {
            fader_state[k] = v;
            send_midi_pitchbend(k - bank_ofs, 0, v);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            send_midi_pitchbend(k - bank_ofs, 0, v);
          }
          // mute state:
          bool mutestate = false;
          if(sounds[k])
            mutestate = sounds[k]->get_mute();
          else
            mutestate = routes[k]->get_mute();
          if((mutestate != mute_state[k]) || upload) {
            mute_state[k] = mutestate;
            if(mutestate)
              send_midi_note(0, 16 + k - bank_ofs, 127);
            else
              send_midi_note(0, 16 + k - bank_ofs, 0);
          }
          // level display:
          if(routes[k]) {
            float lrms = -200;
            if(sounds[k])
              lrms = sounds[k]->read_meter_maxval();
            else
              lrms = routes[k]->read_meter_maxval();
            lrms = lrms - TASCAR::lin2dbspl(ports[k]->caliblevel);
            int mval = 0;
            for(const auto& th : vu_thresholds) {
              if(th < lrms)
                ++mval;
              else
                break;
            }
            // if(mval != meter_state[k]) {
            {
              meter_state[k] = mval;
              send_midi_channel_pressure(0, 0, mval + 16 * (k - bank_ofs));
            }
          }

          // names:
          // if(upload) {
          //  msg_sysex[0] = 0xf0;
          //  msg_sysex[1] = 0;
          //  msg_sysex[2] = 0;
          //  msg_sysex[3] = 0x66;
          //  msg_sysex[4] = 0x14;
          //  msg_sysex[5] = 0x12;
          //  msg_sysex[6] = 6 * (k - bank_ofs);
          //  msg_sysex[7] = 'x';
          //  msg_sysex[8] = 'B';
          //  msg_sysex[9] = 'C';
          //  msg_sysex[10] = 'D';
          //  msg_sysex[11] = 0xf7;
          //  send_midi_sysex(12, msg_sysex);
          //  DEBUG(k);
          //}
        }
      }
      upload = false;
    }
  }
  // reset all faders to zero:
  for(int32_t k = 0; k < banksize; ++k) {
    send_midi_pitchbend(k, 0, -8192);
  }
}

void mcu_ctl_t::emit_event_note(int channel, int note, int vel)
{
  bool known = false;
  if((channel == 0) && (note == 48) && (vel > 0)) {
    if(bank_ofs > 0)
      --bank_ofs;
    upload = true;
    known = true;
  }
  if((channel == 0) && (note == 49) && (vel > 0)) {
    if(bank_ofs + banksize < (int)ports.size())
      ++bank_ofs;
    upload = true;
    known = true;
  }
  if((channel == 0) && (note == 46) && (vel > 0)) {
    if(bank_ofs > 0)
      bank_ofs -= std::min(bank_ofs, banksize);
    upload = true;
    known = true;
  }
  if((channel == 0) && (note == 47) && (vel > 0)) {
    DEBUG(bank_ofs);
    DEBUG(banksize);
    DEBUG(ports.size());
    DEBUG(std::min((int)ports.size() - bank_ofs - banksize, banksize));
    if(bank_ofs + banksize < (int)ports.size())
      bank_ofs += std::min((int)ports.size() - bank_ofs - banksize, banksize);
    DEBUG(bank_ofs);
    DEBUG(std::min((int)ports.size() - bank_ofs - banksize, banksize));
    upload = true;
    known = true;
  }
  if((channel == 0) && (note >= 16) && (note < 24)) {
    known = true;
    auto idx = note - 16 + bank_ofs;
    if(((size_t)idx < ports.size()) && (vel > 0)) {
      bool mutestate = false;
      if(sounds[idx])
        mutestate = sounds[idx]->get_mute();
      else
        mutestate = routes[idx]->get_mute();
      mutestate = !mutestate;
      if(sounds[idx])
        sounds[idx]->set_mute(mutestate);
      else
        routes[idx]->set_mute(mutestate);
      if(mutestate)
        send_midi_note(channel, note, 127);
      else
        send_midi_note(channel, note, 0);
    }
  }
  if(!known && dumpmsg) {
    std::cout << "note " << channel << "/" << note << "/" << vel << std::endl;
  }
}

void mcu_ctl_t::emit_event_mmc(uint8_t a, uint8_t b)
{
  if(dumpmsg) {
    std::cout << "MMC " << a << "/" << b << std::endl;
  }
}

/*
 * This function will be called when a MIDI event is sent by the MCU
 * device.
 */
void mcu_ctl_t::emit_event(int channel, int param, int value)
{
  // uint32_t ctl(256 * channel + param);
  bool known = false;
  // gain faders:
  if((param == 0) && (channel < banksize) &&
     (channel + bank_ofs < (int)ports.size())) {
    known = true;
    auto gain = gui_to_gain((value + 8192.0f) / 16384.0f);
    ports[channel + bank_ofs]->set_gain_db(gain);
  }
  // main gain faders:
  if((param == 0) && (channel == 8) && mainport.size()) {
    known = true;
    auto gain = gui_to_gain((value + 8192.0f) / 16384.0f);
    mainport[0]->set_gain_db(gain);
  }
  // for(uint32_t k = 0; k < controllers_.size(); ++k) {
  //  if(controllers_[k] == ctl) {
  //    if(k < ports.size()) {
  //      values[k] = value;
  //      ports[k]->set_gain_db(gui_to_gain(value / 127.0f));
  //    } else {
  //      uint32_t k1(k - ports.size());
  //      if(k1 < routes.size()) {
  //        if(sounds[k1]) {
  //          values[k] = value;
  //          sounds[k1]->set_mute(value > 0);
  //        } else {
  //          if(routes[k1]) {
  //            values[k] = value;
  //            routes[k1]->set_mute(value > 0);
  //          }
  //        }
  //      }
  //    }
  //    known = true;
  //  }
  //}

  if((!known) && dumpmsg) {
    char ctmp[256];
    snprintf(ctmp, 256, "%d/%d: %d", channel, param, value);
    ctmp[255] = 0;
    std::cout << ctmp << std::endl;
  } else {
    //
  }
}

REGISTER_MODULE(mcu_ctl_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
