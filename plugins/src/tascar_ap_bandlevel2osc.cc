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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <lo/lo.h>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

enum levelmode_t { dbspl, rms, max };

class level2osc_t : public TASCAR::audioplugin_base_t {
public:
  level2osc_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  ~level2osc_t();

private:
  bool threaded = true;
  bool sendwhilestopped = false;
  uint32_t skip = 0;
  std::string url = "osc.udp://localhost:9999/";
  std::string path = "/level";
  std::vector<float> f = {250.0f, 500.0f, 1000.0f, 2000.0f};
  float bandwidth = 1; // in octaves
  // derived variables:
  levelmode_t imode = dbspl;
  lo_address lo_addr;
  uint32_t skipcnt = 0;
  lo_message msg;
  lo_arg** oscmsgargv;
  std::thread thread;
  std::atomic_bool run_thread = true;
  void sendthread();
  std::mutex mtx;
  std::condition_variable cond;
  std::atomic_bool has_data = false;
  std::vector<TASCAR::wave_t> filterbankout;
  std::vector<TASCAR::bandpass_t> bp1;
  std::vector<TASCAR::bandpass_t> bp2;
  size_t n_bands;
};

level2osc_t::level2osc_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE_BOOL(sendwhilestopped, "Send also when transport is stopped");
  GET_ATTRIBUTE(skip, "", "Skip frames");
  GET_ATTRIBUTE(url, "", "Target URL");
  GET_ATTRIBUTE(path, "", "Target path");
  GET_ATTRIBUTE_BOOL(threaded, "Use additional thread for sending data");
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
  GET_ATTRIBUTE(f, "Hz", "Center frequencies");
  if(f.size() == 0)
    throw TASCAR::ErrMsg("Invalid number of frequency bands");
  n_bands = f.size();
  GET_ATTRIBUTE(bandwidth, "octaves", "band width");
  if(!(bandwidth > 0.0f))
    throw TASCAR::ErrMsg("Invalid bandwidth, needs to be a positive number.");
  lo_addr = lo_address_new_from_url(url.c_str());
  if(threaded)
    thread = std::thread(&level2osc_t::sendthread, this);
}

void level2osc_t::sendthread()
{
  std::unique_lock<std::mutex> lk(mtx);
  while(run_thread) {
    cond.wait_for(lk, 100ms);
    if(has_data) {
      lo_send_message(lo_addr, path.c_str(), msg);
      has_data = false;
    }
  }
}

void level2osc_t::configure()
{
  audioplugin_base_t::configure();
  msg = lo_message_new();
  // time:
  lo_message_add_float(msg, 0);
  // levels:
  for(uint32_t k = 0; k < n_channels * n_bands; ++k)
    lo_message_add_float(msg, 0);
  oscmsgargv = lo_message_get_argv(msg);
  filterbankout.resize(n_channels * n_bands);
  for(auto& w : filterbankout)
    w.resize(n_fragment);
  bp1.resize(n_channels * n_bands);
  bp2.resize(n_channels * n_bands);
  float f_inc = pow(2.0, 0.5 * bandwidth);
  for(size_t k = 0; k < n_bands; ++k) {
    float fc = f[k];
    for(size_t c = 0; c < n_channels; ++c) {
      bp1[k + n_bands * c].set_range(fc / f_inc, fc * f_inc, f_sample);
      bp2[k + n_bands * c].set_range(fc / f_inc, fc * f_inc, f_sample);
    }
  }
}

void level2osc_t::release()
{
  lo_message_free(msg);
  audioplugin_base_t::release();
}

level2osc_t::~level2osc_t()
{
  run_thread = false;
  if(threaded)
    thread.join();
  lo_address_free(lo_addr);
}

void level2osc_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                             const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                             const TASCAR::transport_t& tp)
{
  if(chunk.size() != n_channels)
    throw TASCAR::ErrMsg(
        "Programming error (invalid channel number, expected " +
        TASCAR::to_string(n_channels) + ", got " +
        std::to_string(chunk.size()) + ").");
  // update filters:
  for(size_t k = 0; k < n_bands; ++k) {
    for(size_t c = 0; c < n_channels; ++c) {
      filterbankout[k + n_bands * c].copy(chunk[c]);
      bp1[k + n_bands * c].filter(filterbankout[k + n_bands * c]);
      bp2[k + n_bands * c].filter(filterbankout[k + n_bands * c]);
    }
  }
  if(tp.rolling || sendwhilestopped) {
    if(skipcnt) {
      skipcnt--;
    } else {
      // data will be sent in extra thread:
      if(mtx.try_lock()) {
        // pack data:
        oscmsgargv[0]->f = tp.object_time_seconds;
        for(uint32_t ch = 0; ch < n_channels * n_bands; ++ch) {
          switch(imode) {
          case dbspl:
            oscmsgargv[ch + 1]->f = filterbankout[ch].spldb();
            break;
          case rms:
            oscmsgargv[ch + 1]->f = filterbankout[ch].rms();
            break;
          case max:
            oscmsgargv[ch + 1]->f = filterbankout[ch].maxabs();
            break;
          }
        }
        has_data = true;
        mtx.unlock();
        if(threaded)
          cond.notify_one();
        else
          lo_send_message(lo_addr, path.c_str(), msg);
      }
      skipcnt = skip;
    }
  }
}

REGISTER_AUDIOPLUGIN(level2osc_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
