/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

#include "audioplugin.h"
#include "errorhandling.h"
#include "filterclass.h"
#include "ringbuffer.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

class reclevelanalyzer_t : public TASCAR::audioplugin_base_t {
public:
  reclevelanalyzer_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t&,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t&);
  void add_variables(TASCAR::osc_server_t* srv);
  ~reclevelanalyzer_t();
  void configure();
  void release();

private:
  static int osc_trigger(const char* path, const char* types, lo_arg** argv,
                         int argc, lo_message msg, void* user_data);
  void osc_trigger();
  void analysis_and_send_data();

  void updatethread();
  float tau_segment = 0.125;
  float tau_analysis = 30.0;
  float update_interval = 5.0;
  std::vector<float> p = {0.0f, 0.5f, 0.65f, 0.95f, 1.0f};
  std::string path = "/reclevelanalyzer";
  bool triggered = false;
  //
  TASCAR::ringbuffer_t* rb = NULL;
  TASCAR::wave_t writebuffer;
  TASCAR::wave_t readbuffer;
  // threads and synchronization:
  std::thread thread;
  std::atomic_bool run_thread = true;
  void sendthread();
  std::mutex mtx;
  std::mutex mtxanalysis;
  std::condition_variable cond;
  // OSC server copy:
  TASCAR::osc_server_t* srv_ = NULL;
  std::atomic_bool can_send = false;
  lo_message msg = NULL;
  TASCAR::wave_t sort_buffer;
  std::vector<TASCAR::wave_t> v_peaks;
  std::vector<TASCAR::wave_t> v_ms;
  std::vector<uint32_t> idx_percentile;
};

reclevelanalyzer_t::reclevelanalyzer_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg)
{
  GET_ATTRIBUTE(tau_segment, "s", "Period time of one level segment");
  GET_ATTRIBUTE(tau_analysis, "s", "Length of analysis window");
  GET_ATTRIBUTE(
      update_interval, "s",
      "Update interval of analysis (each update will analyse full window)");
  GET_ATTRIBUTE(p, "", "Percentile values");
  GET_ATTRIBUTE(
      path, "",
      "OSC path. It will contain 2*sizeof(p)+1 values, the first is channel, "
      "then sizeof(p) peak values, then sizeof(p) RMS values.");
  GET_ATTRIBUTE_BOOL(triggered,
                     "Update analysis only when triggered via OSC message.");
}

int reclevelanalyzer_t::osc_trigger(const char*, const char*, lo_arg**, int,
                                    lo_message, void* h)
{
  ((reclevelanalyzer_t*)h)->osc_trigger();
  return 0;
}

void reclevelanalyzer_t::osc_trigger()
{
  if(triggered)
    analysis_and_send_data();
}

void reclevelanalyzer_t::analysis_and_send_data()
{
  if(!can_send)
    return;
  auto oscmsgargv = lo_message_get_argv(msg);
  for(uint32_t ch = 0; ch < n_channels; ++ch) {
    oscmsgargv[0]->i = ch;
    // analyse peak_
    {
      std::lock_guard<std::mutex> lock(mtxanalysis);
      sort_buffer.copy(v_peaks[ch]);
    }
    std::sort(sort_buffer.begin(), sort_buffer.end());
    uint32_t k = 1;
    for(auto idx : idx_percentile) {
      oscmsgargv[k]->f = 20.0f * log10f(sort_buffer.d[idx]);
      ++k;
    }
    // analyse ms:
    {
      std::lock_guard<std::mutex> lock(mtxanalysis);
      sort_buffer.copy(v_ms[ch]);
    }
    std::sort(sort_buffer.begin(), sort_buffer.end());
    for(auto idx : idx_percentile) {
      oscmsgargv[k]->f = 10.0f * log10f(sort_buffer.d[idx]);
      ++k;
    }
    srv_->dispatch_data_message(path.c_str(), msg);
  }
}

void reclevelanalyzer_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv_ = srv;
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_method("/trigger", "", &osc_trigger, this);
  // srv->add_float_db("/gain", &gain);
  // srv->add_float("/lingain", &gain);
  // srv->add_method("/fade", "ff", osc_set_fade, this);
  srv->unset_variable_owner();
}

void reclevelanalyzer_t::configure()
{
  audioplugin_base_t::configure();
  uint32_t ringbuffersize =
      (uint32_t)(std::max(update_interval, 1.0f) * f_sample);
  rb = new TASCAR::ringbuffer_t(ringbuffersize, n_channels);
  writebuffer.resize(n_fragment * n_channels);
  thread = std::thread(&reclevelanalyzer_t::updatethread, this);
}

void reclevelanalyzer_t::release()
{
  run_thread = false;
  if(thread.joinable())
    thread.join();
  if(rb)
    delete rb;
  rb = NULL;
  audioplugin_base_t::release();
}

reclevelanalyzer_t::~reclevelanalyzer_t() {}

void reclevelanalyzer_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                                    const TASCAR::pos_t&,
                                    const TASCAR::rotmat_t&,
                                    const TASCAR::transport_t&)
{
  if(!rb)
    return;
  if((n_channels > 0) && ((size_t)n_channels == chunk.size())) {
    uint32_t N = chunk[0].n;
    for(size_t ch = 0; ch < n_channels; ++ch)
      chunk[ch].copy_to_stride(writebuffer.d + ch, N, n_channels);
    if(rb->write_space() >= N) {
      rb->write(writebuffer.d, N);
      cond.notify_one();
    }
  }
}

void reclevelanalyzer_t::updatethread()
{
  // number of audio samples in one analysis segment:
  uint32_t segment_len = (uint32_t)(std::max(1.0, tau_segment * f_sample));
  // number of segments in the analysis window:
  uint32_t num_segments = std::max(3u, (uint32_t)(tau_analysis / tau_segment));
  // audio buffer for one channel of a segment:
  TASCAR::wave_t segment(segment_len);
  // read buffer for one segment (all channels):
  TASCAR::wave_t readbuffer(segment_len * n_channels);
  sort_buffer.resize(num_segments);
  // indices of percentile values:
  idx_percentile.clear();
  for(const auto& p_ : p)
    idx_percentile.push_back(std::max(
        0u, std::min(num_segments, uint32_t(p_ * (float)(num_segments - 1u)))));
  v_peaks.resize(n_channels);
  v_ms.resize(n_channels);
  for(auto& w : v_peaks)
    w.resize(num_segments);
  for(auto& w : v_ms)
    w.resize(num_segments);
  std::unique_lock<std::mutex> lk(mtx);
  uint32_t cnt_segment = 0;
  uint32_t analysis_period =
      std::max(1u, (uint32_t)(update_interval / tau_segment));
  // A weighting filters:
  std::vector<TASCAR::aweighting_t> a_flt(n_channels,
                                          TASCAR::aweighting_t(f_sample));
  // OSC message for sending:
  msg = lo_message_new();
  lo_message_add_int32(msg, 0);
  for(size_t k = 0u; k < p.size(); ++k) {
    lo_message_add_float(msg, 0.0f);
    lo_message_add_float(msg, 0.0f);
  }
  can_send = true;
  while(run_thread) {
    cond.wait_for(lk, 100ms);
    while(rb->read_space() >= segment_len) {
      // process one audio segment:
      rb->read(readbuffer.begin(), segment_len);
      {
        std::lock_guard<std::mutex> lock(mtxanalysis);
        for(uint32_t ch = 0; ch < n_channels; ++ch) {
          segment.copy_stride(readbuffer.d + ch, segment_len, n_channels);
          // analyse and filter one segment, store to analysis window buffer:
          float peak = segment.maxabs();
          v_peaks[ch].append_sample(peak);
          // now filter...
          a_flt[ch].filter(segment);
          v_ms[ch].append_sample(segment.ms());
        }
      }
      // increase segment counter and test if analysis is needed:
      cnt_segment++;
      if(cnt_segment >= analysis_period) {
        cnt_segment = 0;
        if(!triggered)
          analysis_and_send_data();
      }
    }
  }
  can_send = false;
  lo_message_free(msg);
}

REGISTER_AUDIOPLUGIN(reclevelanalyzer_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
