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
#include "tascar_os.h"
#include <fstream>
#include <thread>

std::set<float> vu_thresholds = {-100.0f, -60.0f, -50.0f, -40.0f, -30.0f,
                                 -20.0f,  -14.0f, -10.0f, -8.0f,  -6.0f,
                                 -4.0f,   -2.0f,  -0.0f};

float gain_to_gui(float dbGain)
{
  return powf(((dbGain + 200.0f) / 210.0f), 6.0f);
}

float gui_to_gain(float guiGain)
{
  return 210.0f * (powf(guiGain, 1.0f / 6.0f)) - 200.0f;
}

int db_gain_to_midi14(float dbGain)
{
  return std::max(-8192.0f,
                  std::min(8191.0f, 16384.0f * gain_to_gui(dbGain) - 8192.0f));
}

class midictl_vars_t : public TASCAR::module_base_t {
public:
  midictl_vars_t(const TASCAR::module_cfg_t& cfg);

protected:
  bool dumpmsg = false;
  std::string name;
  std::vector<std::string> pattern;
  std::string mainctl;
  int32_t banksize = 8;
  int32_t minwait = 0;
  bool is_icon = false;
};

midictl_vars_t::midictl_vars_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg)
{
  GET_ATTRIBUTE_BOOL(dumpmsg, "Show unused messages in concole");
  GET_ATTRIBUTE(name, "",
                "Controller name used for MIDI port and OSC interface");
  // GET_ATTRIBUTE(connect, "", "ALSA midi port connection, e.g., BCF2000:0");
  GET_ATTRIBUTE(pattern, "", "TASCAR controllers");
  GET_ATTRIBUTE(mainctl, "", "TASCAR main controller");
  GET_ATTRIBUTE(banksize, "", "Number of faders per bank");
  GET_ATTRIBUTE(minwait, "ms", "Wait after each MIDI send");
  GET_ATTRIBUTE_BOOL(is_icon, "Is iCON V1-M or compatible?");
}

class controllable_t {
public:
  controllable_t(){};
  controllable_t(TASCAR::Scene::audio_port_t* p);
  float get_gain_db() const;
  bool get_mute() const;
  int get_gain_midi() const;
  int get_level_midi() const;
  void set_mute(bool m);
  void set_gain_db(float g);
  void set_gain_midi(int m);
  std::string get_name();
  TASCAR::Scene::rgb_color_t get_color() const;

  int fader_state = -8193;
  int fader_is_moving = 0;
  int meter_state = -2000;
  int mute_state = -1;

private:
  TASCAR::Scene::audio_port_t* p_port = NULL;
  TASCAR::Scene::route_t* p_route = NULL;
  TASCAR::Scene::sound_t* p_sound = NULL;
};

int controllable_t::get_level_midi() const
{
  int mval = 0;
  if(p_port) {
    float lrms = -200;
    if(p_sound)
      lrms = p_sound->read_meter_maxval();
    else if(p_route)
      lrms = p_route->read_meter_maxval();
    lrms = lrms - TASCAR::lin2dbspl(p_port->caliblevel);
    for(const auto& th : vu_thresholds) {
      if(th < lrms)
        ++mval;
      else
        break;
    }
  }
  return mval;
}

std::string controllable_t::get_name()
{
  if(p_port)
    return p_port->get_ctlname();
  return "";
}

float controllable_t::get_gain_db() const
{
  if(p_port)
    return p_port->get_gain_db();
  return 0.0f;
}

int controllable_t::get_gain_midi() const
{
  return db_gain_to_midi14(get_gain_db());
}

bool controllable_t::get_mute() const
{
  if(p_sound)
    return p_sound->get_mute();
  if(p_route)
    return p_route->get_mute();
  return false;
}

TASCAR::Scene::rgb_color_t controllable_t::get_color() const
{
  if(p_route)
    return p_route->color;
  if(p_sound)
    return p_sound->get_color();
  return TASCAR::Scene::rgb_color_t(0, 0, 0);
}

void controllable_t::set_gain_db(float g)
{
  if(p_port)
    p_port->set_gain_db(g);
}

void controllable_t::set_gain_midi(int m)
{
  set_gain_db(gui_to_gain((m + 8192.0f) / 16384.0f));
}

void controllable_t::set_mute(bool m)
{
  if(p_sound) {
    p_sound->set_mute(m);
    return;
  }
  if(p_route)
    p_route->set_mute(m);
}

controllable_t::controllable_t(TASCAR::Scene::audio_port_t* p)
{
  p_port = p;
  p_route = dynamic_cast<TASCAR::Scene::route_t*>(p_port);
  if(!p_route) {
    p_sound = dynamic_cast<TASCAR::Scene::sound_t*>(p_port);
    if(p_sound)
      p_route = dynamic_cast<TASCAR::Scene::route_t*>(p_sound->parent);
  }
}

class mcu_ctl_t : public midictl_vars_t, public TASCAR::midi_ctl_t {
public:
  mcu_ctl_t(const TASCAR::module_cfg_t& cfg);
  ~mcu_ctl_t();
  virtual void update(uint32_t tp_frame, bool running);
  void configure();
  void release();
  virtual void emit_event(int channel, int param, int value);
  virtual void emit_event_note(int, int, int);
  virtual void emit_event_mmc(uint8_t, uint8_t);
  void upload_text(int channel, int row, std::string msg);

private:
  void send_service();
  std::vector<controllable_t> controllers;
  controllable_t main;
  std::thread srv;
  bool run_service = true;
  int upload = 2;
  std::mutex mtx;
  int32_t bank_ofs = 0;
  char msg_sysex[1024];
  uint32_t sessiontime = -1;
  uint32_t lastsessiontime = -1;
  uint32_t transportstate = -1;
  uint32_t lasttransportstate = -1;
};

void mcu_ctl_t::update(uint32_t tp_frame, bool running)
{
  sessiontime = tp_frame;
  transportstate = running;
}

mcu_ctl_t::mcu_ctl_t(const TASCAR::module_cfg_t& cfg)
    : midictl_vars_t(cfg), TASCAR::midi_ctl_t(name)
{
  TASCAR::midi_ctl_t::minwait = midictl_vars_t::minwait;
  set_nonblock(true);
  parse_xml_connections(e);
  session->add_int(std::string("/") + name + "/upload", &upload);
}

void mcu_ctl_t::configure()
{
  std::lock_guard<std::mutex> lock{mtx};
  controllers.clear();
  main = controllable_t();
  if(session) {
    auto ports = session->find_audio_ports(pattern);
    for(const auto& p : ports)
      controllers.push_back(p);
    auto mainport = session->find_audio_ports({mainctl});
    if(!mainport.empty())
      main = controllable_t(mainport[0]);
  }
  start_service();
  TASCAR::module_base_t::configure();
  run_service = true;
  srv = std::thread(&mcu_ctl_t::send_service, this);
}

void mcu_ctl_t::release()
{
  run_service = false;
  if(srv.joinable())
    srv.join();
  stop_service();
  TASCAR::module_base_t::release();
}

mcu_ctl_t::~mcu_ctl_t() {}

void mcu_ctl_t::upload_text(int channel, int row, std::string msg)
{
  if((0 <= row) && (row < 2)) {
    msg += "          ";
    msg.resize(7);
    std::string msg_sysex_;
    msg_sysex_.resize(7);
    msg_sysex_[0] = 0xf0;
    msg_sysex_[1] = 0;
    msg_sysex_[2] = 0;
    msg_sysex_[3] = 0x66;
    msg_sysex_[4] = 0x14;
    msg_sysex_[5] = 0x12;
    msg_sysex_[6] = 7 * channel + row * 0x38;
    msg_sysex_ += msg;
    msg_sysex_ += 0xf7;
    send_midi_sysex(msg_sysex_.size(), msg_sysex_.data());
  }
  if(is_icon && (2 <= row) && (row < 4)) {
    msg += "          ";
    msg.resize(7);
    std::string msg_sysex_;
    msg_sysex_.resize(7);
    msg_sysex_[0] = 0xf0;
    msg_sysex_[1] = 0;
    msg_sysex_[2] = 0x02;
    msg_sysex_[3] = 0x4e;
    msg_sysex_[4] = 0x15;
    msg_sysex_[5] = 0x13;
    msg_sysex_[6] = 7 * channel + (row - 2) * 0x38;
    msg_sysex_ += msg;
    msg_sysex_ += 0xf7;
    send_midi_sysex(msg_sysex_.size(), msg_sysex_.data());
  }
}

#include <cmath>
#include <cstdint>

struct TimeComponents {
  uint32_t hours;
  uint32_t minutes;
  uint32_t seconds;
  uint32_t milliseconds;
};

TimeComponents sampleToTime(uint32_t sampleNumber, double sampleRate)
{
  TimeComponents time;
  if(sampleRate == 0.0) {
    // Handle division by zero if needed
    time.hours = 0;
    time.minutes = 0;
    time.seconds = 0;
    time.milliseconds = 0;
    return time;
  }

  double totalSeconds = static_cast<double>(sampleNumber) / sampleRate;

  // Extract hours
  time.hours = static_cast<uint32_t>(totalSeconds / 3600.0);
  double remainingSeconds = totalSeconds - (time.hours * 3600.0);

  // Extract minutes
  time.minutes = static_cast<uint32_t>(remainingSeconds / 60.0);
  remainingSeconds -= time.minutes * 60.0;

  // Extract seconds
  time.seconds = static_cast<uint32_t>(remainingSeconds);

  // Extract milliseconds
  double fractionalSeconds = totalSeconds - floor(totalSeconds);
  time.milliseconds = static_cast<uint32_t>(fractionalSeconds * 1000.0 +
                                            0.5); // +0.5 for rounding

  return time;
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
      // session time:
      if(sessiontime != lastsessiontime) {
        lastsessiontime = sessiontime;
        // transform into h/m/s/ms
        auto tm = sampleToTime(sessiontime, f_sample);
        send_midi(15, 0x40, (tm.milliseconds % 10) | 0x30);
        send_midi(15, 0x41, ((tm.milliseconds / 10) % 10) | 0x30);
        send_midi(15, 0x42, ((tm.milliseconds / 100) % 10) | 0x30);
        send_midi(15, 0x43, (tm.seconds % 10) | 0x70);
        send_midi(15, 0x44, ((tm.seconds / 10) % 10) | 0x30);
        send_midi(15, 0x45, (tm.minutes % 10) | 0x70);
        send_midi(15, 0x46, ((tm.minutes / 10) % 10) | 0x30);
        send_midi(15, 0x47, (tm.hours % 10) | 0x70);
        send_midi(15, 0x48, ((tm.hours / 10) % 10) | 0x30);
        send_midi(15, 0x49, 0x20);
      }
      // main gain:
      if(!main.fader_is_moving) {
        int main_midigain = main.get_gain_midi();
        if((main_midigain != main.fader_state) || upload) {
          main.fader_state = main_midigain;
          send_midi_pitchbend(8, 0, main_midigain);
        }
      }
      for(int32_t k = 0; k < (int32_t)controllers.size(); ++k) {
        // channel gain:
        if((k - bank_ofs < banksize) && (k >= bank_ofs)) {
          // fader states:
          if((!controllers[k].fader_is_moving) || upload) {
            int midigain = controllers[k].get_gain_midi();
            if((midigain != controllers[k].fader_state) || upload) {
              controllers[k].fader_state = midigain;
              send_midi_pitchbend(k - bank_ofs, 0, midigain);
            }
          }
          // mute state:
          bool mute = controllers[k].get_mute();
          if((mute != controllers[k].mute_state) || upload) {
            controllers[k].mute_state = mute;
            send_midi_note(0, 16 + k - bank_ofs, 127 * mute);
          }
          // level display:
          if(!upload) {
            int mval = controllers[k].get_level_midi();
            send_midi_channel_pressure(0, 0, mval + 16 * (k - bank_ofs));
          }

          // names:
          if(upload) {
            auto vstr = TASCAR::str2vecstr(controllers[k].get_name(), "/");
            std::string onemsg;
            for(size_t row = 0; row < 2u; ++row) {
              std::string msg;
              if(row < vstr.size())
                msg = vstr[vstr.size() - row - 1u];
              if(onemsg.empty())
                onemsg = msg;
              else
                onemsg = msg + std::string(".") + onemsg;
              upload_text(k - bank_ofs, row, msg);
            }
            if(onemsg[0] == '.')
              onemsg.erase(0, 1);
            upload_text(k - bank_ofs, 3, onemsg);
          }
        }
      }
      if(upload) {
        // colors:
        if(is_icon) {
          std::string msg_sysex_;
          msg_sysex_.resize(6);
          msg_sysex_[0] = 0xf0;
          msg_sysex_[1] = 0;
          msg_sysex_[2] = 2;
          msg_sysex_[3] = 0x4e;
          msg_sysex_[4] = 0x16;
          msg_sysex_[5] = 0x14;
          for(int32_t k = 0; k < (int32_t)controllers.size(); ++k) {
            if((k - bank_ofs < banksize) && (k >= bank_ofs)) {
              auto col = controllers[k].get_color();
              msg_sysex_ += uint8_t(col.r * 127);
              msg_sysex_ += uint8_t(col.g * 127);
              msg_sysex_ += uint8_t(col.b * 127);
            }
          }
          msg_sysex_ += 0xf7;
          send_midi_sysex(msg_sysex_.size(), msg_sysex_.data());
        }
      }
      if(upload)
        --upload;
    }
  }
  // on exit, reset all faders to zero:
  for(int32_t k = 0; k < banksize; ++k) {
    if(is_icon)
      send_midi_pitchbend(8, 0, -8192);
    send_midi_pitchbend(k, 0, -8192);
    send_midi_channel_pressure(0, 0, 16 * k);
    upload_text(k, 0, "");
    upload_text(k, 1, "");
    upload_text(k, 3, "");
  }
  {
    // reset colors:
    std::string msg_sysex_;
    msg_sysex_.resize(6);
    msg_sysex_[0] = 0xf0;
    msg_sysex_[1] = 0;
    msg_sysex_[2] = 2;
    msg_sysex_[3] = 0x4e;
    msg_sysex_[4] = 0x16;
    msg_sysex_[5] = 0x14;
    for(int32_t k = 0; k < banksize; ++k) {
      msg_sysex_ += uint8_t(0);
      msg_sysex_ += uint8_t(0);
      msg_sysex_ += uint8_t(0);
    }
    msg_sysex_ += 0xf7;
    send_midi_sysex(msg_sysex_.size(), msg_sysex_.data());
  }
  {
    // reset time display:
    send_midi(15, 0x40, 0x20);
    send_midi(15, 0x41, 0x20);
    send_midi(15, 0x42, 0x20);
    send_midi(15, 0x43, 0x20);
    send_midi(15, 0x44, 0x20);
    send_midi(15, 0x45, 0x20);
    send_midi(15, 0x46, 0x20);
    send_midi(15, 0x47, 0x20);
    send_midi(15, 0x48, 0x20);
    send_midi(15, 0x49, 0x20);
  }
  upload_text(0, 0, "Thanks");
  upload_text(1, 0, "for");
  upload_text(2, 0, "using");
  upload_text(3, 0, "TASCAR!");
}

void mcu_ctl_t::emit_event_note(int channel, int note, int vel)
{
  bool known = false;
  if((channel == 0) && (note == 48) && (vel > 0)) {
    if(bank_ofs > 0)
      --bank_ofs;
    upload = 2;
    known = true;
  }
  if((channel == 0) && (note == 49) && (vel > 0)) {
    if(bank_ofs + banksize < (int)controllers.size())
      ++bank_ofs;
    upload = 2;
    known = true;
  }
  if((channel == 0) && (note == 46) && (vel > 0)) {
    if(bank_ofs > 0)
      bank_ofs -= std::min(bank_ofs, banksize);
    upload = 2;
    known = true;
  }
  if((channel == 0) && (note == 47) && (vel > 0)) {
    if(bank_ofs + banksize < (int)controllers.size())
      bank_ofs +=
          std::min((int)controllers.size() - bank_ofs - banksize, banksize);
    upload = 2;
    known = true;
  }
  if((channel == 0) && (note >= 16) && (note < 16 + banksize)) {
    known = true;
    auto idx = note - 16 + bank_ofs;
    if(((size_t)idx < controllers.size()) && (vel > 0)) {
      bool mute = controllers[idx].get_mute();
      mute = !mute;
      controllers[idx].set_mute(mute);
      send_midi_note(channel, note, mute * 127);
    }
  }
  if((channel == 0) && (note >= 104) && (note < 104 + banksize)) {
    known = true;
    auto idx = note - 104 + bank_ofs;
    if((size_t)idx < controllers.size()) {
      controllers[idx].fader_is_moving = vel > 0;
    }
  }
  if((channel == 0) && (note == 104 + banksize)) {
    known = true;
    main.fader_is_moving = vel > 0;
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
     (channel + bank_ofs < (int)controllers.size())) {
    known = true;
    controllers[channel + bank_ofs].set_gain_midi(value);
  }
  // main gain faders:
  if((param == 0) && (channel == 8)) {
    known = true;
    main.set_gain_midi(value);
  }
  if((!known) && dumpmsg) {
    char ctmp[256];
    snprintf(ctmp, 256, "%d/%d: %d", channel, param, value);
    ctmp[255] = 0;
    std::cout << ctmp << std::endl;
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
