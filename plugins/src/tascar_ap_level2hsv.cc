/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2024 Giso Grimm
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
#include "levelmeter.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <lo/lo.h>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

enum levelmode_t { dbspl, rms, max };

class level2hsv_t : public TASCAR::audioplugin_base_t {
public:
  level2hsv_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  void release();
  ~level2hsv_t();

private:
  uint32_t skip = 0;
  float tau = 0;
  bool use_leq = true;
  std::string url = "osc.udp://localhost:9999/";
  std::vector<std::string> path = {"/hsv"};
  float hue = 0;
  float saturation = 1;
  double decay = 0.0;
  TASCAR::levelmeter::weight_t weight = TASCAR::levelmeter::Z;
  std::vector<float> frange = {62.5f, 4000.0f};
  std::vector<float> lrange = {40.0f, 90.0f};
  bool active = true;
  // float value = 0;
  // derived variables:
  levelmode_t imode = dbspl;
  lo_address lo_addr;
  uint32_t skipcnt = 0;
  lo_message msg;
  std::thread thread;
  std::atomic_bool run_thread = true;
  void sendthread();
  std::mutex mtx;
  std::condition_variable cond;
  std::atomic_bool has_data = false;
  TASCAR::levelmeter_t* lmeter = NULL;
  float* p_value = NULL;
  float* p_sat = NULL;
  float* p_hue = NULL;
  double lstate = 0.0;
};

level2hsv_t::level2hsv_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(skip, "", "Skip frames");
  GET_ATTRIBUTE(url, "", "Target URL");
  GET_ATTRIBUTE(path, "", "Target path");
  GET_ATTRIBUTE(tau, "s", "Leq duration, or 0 to use block size");
  GET_ATTRIBUTE(hue, "degree", "Hue component (0-360)");
  GET_ATTRIBUTE(saturation, "", "Saturation component (0-1)");
  GET_ATTRIBUTE_BOOL(active, "start activated");
  GET_ATTRIBUTE(decay, "", "decay filter coefficient");
  std::string mode("dbspl");
  GET_ATTRIBUTE(mode, "", "Level mode [dbspl|rms|max]");
  if(mode == "dbspl")
    imode = dbspl;
  else if(mode == "rms")
    imode = rms;
  else if(mode == "max")
    imode = max;
  else
    throw TASCAR::ErrMsg("Invalid level mode: " + mode);
  GET_ATTRIBUTE_NOUNIT(weight, "Level meter weight");
  GET_ATTRIBUTE(frange, "Hz", "Frequency range in bandpass mode");
  if(weight == TASCAR::levelmeter::bandpass)
    if(frange.size() != 2)
      throw TASCAR::ErrMsg(
          "Frequency range requires exactly two entries (min max)");
  GET_ATTRIBUTE_BOOL(
      use_leq, "Use Leq in level metering (true) or exponential decay (false)");
  GET_ATTRIBUTE(lrange, "dB", "Level range");
  if((lrange.size() != 2) || (lrange[0] == lrange[1]))
    throw TASCAR::ErrMsg(
        "Level range requires exactly two different entries (min max)");
  lo_addr = lo_address_new_from_url(url.c_str());
  msg = lo_message_new();
  lo_message_add_float(msg, hue);
  lo_message_add_float(msg, saturation);
  lo_message_add_float(msg, 0);
  lo_message_add_float(msg, 0.01);
  auto oscmsgargv = lo_message_get_argv(msg);
  p_value = &(oscmsgargv[2]->f);
  p_hue = &(oscmsgargv[0]->f);
  p_sat = &(oscmsgargv[1]->f);
  thread = std::thread(&level2hsv_t::sendthread, this);
}

void level2hsv_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_bool("/active", &active);
  srv->add_float("/hue", &hue, "[0,360]", "Hue component (0-360 degree)");
  srv->add_float("/saturation", &saturation, "[0,1]",
                 "Saturation component (0-1)");
  srv->add_vector_float("/lrange", &lrange, "", "Level range in dB");
  srv->add_double("/decay", &decay, "[0,1[", "decay coeficient");
  srv->unset_variable_owner();
}

void level2hsv_t::sendthread()
{
  std::unique_lock<std::mutex> lk(mtx);
  while(run_thread) {
    cond.wait_for(lk, 100ms);
    if(has_data) {
      float l = 0.0f;
      switch(imode) {
      case dbspl:
        l = lmeter->get_rms_db();
        break;
      case rms:
        l = lmeter->get_rms();
        break;
      case max:
        l = lmeter->get_peak_db();
        break;
      }
      l = std::min(1.0f,
                   std::max(0.0f, (l - lrange[0]) / (lrange[1] - lrange[0])));
      if(l > lstate)
        lstate = l;
      else
        lstate *= decay;
      *p_value = lstate;
      *p_hue = hue;
      *p_sat = saturation;
      if(active)
        for(auto& p : path)
          lo_send_message(lo_addr, p.c_str(), msg);
      has_data = false;
    }
  }
}

void level2hsv_t::configure()
{
  audioplugin_base_t::configure();
  float tau_ = tau;
  if(tau_ == 0)
    tau_ = t_fragment;
  if(lmeter)
    delete lmeter;
  lmeter = NULL;
  lmeter = new TASCAR::levelmeter_t(f_sample, tau_, weight, use_leq);
  if(weight == TASCAR::levelmeter::bandpass)
    lmeter->bp.set_range(frange[0], frange[1]);
}

void level2hsv_t::release()
{
  audioplugin_base_t::release();
}

level2hsv_t::~level2hsv_t()
{
  run_thread = false;
  thread.join();
  lo_address_free(lo_addr);
  if(lmeter)
    delete lmeter;
  lo_message_free(msg);
}

void level2hsv_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                             const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                             const TASCAR::transport_t&)
{
  if(chunk.size() != n_channels)
    throw TASCAR::ErrMsg(
        "Programming error (invalid channel number, expected " +
        TASCAR::to_string(n_channels) + ", got " +
        std::to_string(chunk.size()) + ").");
  if(chunk.size() == 0)
    return;
  lmeter->update(chunk[0]);
  if(skipcnt) {
    skipcnt--;
  } else {
    // data will be sent in extra thread:
    if(mtx.try_lock()) {
      has_data = true;
      mtx.unlock();
      cond.notify_one();
    }
    skipcnt = skip;
  }
}

REGISTER_AUDIOPLUGIN(level2hsv_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
