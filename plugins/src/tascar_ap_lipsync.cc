/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

/*
 * (C) 2016 Giso Grimm
 * (C) 2015-2016 Gerard Llorach to
 *
 */

#include "audioplugin.h"
#include "errorhandling.h"
#include "stft.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <lo/lo.h>
#include <mutex>
#include <string.h>
#include <thread>

using namespace std::chrono_literals;

class lipsync_t : public TASCAR::audioplugin_base_t {
public:
  lipsync_t(const TASCAR::audioplugin_cfg_t& cfg);
  void ap_process(std::vector<TASCAR::wave_t>& chunk, const TASCAR::pos_t& pos,
                  const TASCAR::rotmat_t&, const TASCAR::transport_t& tp);
  void configure();
  void release();
  void add_variables(TASCAR::osc_server_t* srv);
  ~lipsync_t();

private:
  // configuration variables:
  bool threaded = true;
  float smoothing = 0.02f;
  std::string url = "osc.udp://localhost:9999/";
  TASCAR::posf_t scale = TASCAR::posf_t(1, 1, 1);
  float vocalTract = 1.0f;
  float threshold = 0.5f;
  float maxspeechlevel = 48.0f;
  float dynamicrange = 165.0f;
  std::string energypath;
  // internal variables:
  lo_address lo_addr;
  std::string path_;
  TASCAR::stft_t* stft = NULL;
  float* sSmoothedMag = NULL;
  float* sLogMag = NULL;
  uint32_t* formantEdges = NULL;
  uint32_t numFormants = 4;
  bool active = true;
  bool was_active = true;
  enum { always, transport, onchange } send_mode = always;
  float prev_kissBS = HUGE_VAL;
  float prev_jawB = HUGE_VAL;
  float prev_lipsclosedBS = HUGE_VAL;
  uint32_t onchangecount = 3;
  uint32_t onchangecounter = 0;
  std::string strmsg = "/lipsync";
  // OSC messages:
  lo_message msg_blendshapes;
  lo_message msg_energy;
  std::atomic_bool has_msg_blendshapes = false;
  std::atomic_bool has_msg_energy = false;
  float* p_kissBS = NULL;
  float* p_jawB = NULL;
  float* p_lipsclosedBS = NULL;
  float* p_E1 = NULL;
  float* p_E2 = NULL;
  float* p_E3 = NULL;
  float* p_E4 = NULL;
  float* p_E5 = NULL;
  std::mutex mtx;
  std::thread thread;
  std::atomic_bool run_thread = true;
  void sendthread();
  std::condition_variable cond;
};

lipsync_t::lipsync_t(const TASCAR::audioplugin_cfg_t& cfg)
    : audioplugin_base_t(cfg), path_(std::string("/") + cfg.parentname)
{
  GET_ATTRIBUTE(smoothing, "s", "Smoothing time constant");
  GET_ATTRIBUTE(url, "", "Target OSC URL");
  GET_ATTRIBUTE(
      scale, "",
      "Scaling factor of blend shapes; 3 values: kiss, jaw, lipsclosed");
  GET_ATTRIBUTE(vocalTract, "", "Vocal tract scaling factor");
  GET_ATTRIBUTE(threshold, "", "Noise threshold, range 0-1");
  GET_ATTRIBUTE(maxspeechlevel, "dB", "Level normalization");
  GET_ATTRIBUTE(dynamicrange, "dB", "Mapped dynamic range");
  GET_ATTRIBUTE(energypath, "",
                "OSC destination for sending format energies, or empty for no "
                "energy messages");
  GET_ATTRIBUTE_BOOL(threaded, "Use additional thread for sending data");
  std::string path;
  GET_ATTRIBUTE(
      path, "",
      "OSC destination of blendshape messages (empty: use parent name)");
  if(!path.empty())
    path_ = path;
  GET_ATTRIBUTE(
      strmsg, "",
      "Message string to be added to OSC messages before blend shapes");
  std::string sendmode("always");
  GET_ATTRIBUTE(
      sendmode, "",
      "Sending mode, one of ``always'', ``transport'', or ``onchange''");
  if(sendmode == "always")
    send_mode = always;
  else if(sendmode == "transport")
    send_mode = transport;
  else if(sendmode == "onchange")
    send_mode = onchange;
  else
    throw TASCAR::ErrMsg("Invalid send mode " + sendmode +
                         " (possible values: always, transport, onchange)");
  GET_ATTRIBUTE(
      onchangecount, "",
      "Maximum number of repetitions of equal messages in ``onchange'' mode");
  if(url.empty())
    url = "osc.udp://localhost:9999/";
  lo_addr = lo_address_new_from_url(url.c_str());
  msg_blendshapes = lo_message_new();
  if(strmsg.empty()) {
    lo_message_add_float(msg_blendshapes, 0.0f);
    lo_message_add_float(msg_blendshapes, 0.0f);
    lo_message_add_float(msg_blendshapes, 0.0f);
    auto oscmsgargv = lo_message_get_argv(msg_blendshapes);
    p_kissBS = &(oscmsgargv[0]->f);
    p_jawB = &(oscmsgargv[1]->f);
    p_lipsclosedBS = &(oscmsgargv[2]->f);
  } else {
    lo_message_add_string(msg_blendshapes, strmsg.c_str());
    lo_message_add_float(msg_blendshapes, 0.0f);
    lo_message_add_float(msg_blendshapes, 0.0f);
    lo_message_add_float(msg_blendshapes, 0.0f);
    auto oscmsgargv = lo_message_get_argv(msg_blendshapes);
    p_kissBS = &(oscmsgargv[1]->f);
    p_jawB = &(oscmsgargv[2]->f);
    p_lipsclosedBS = &(oscmsgargv[3]->f);
  }
  msg_energy = lo_message_new();
  lo_message_add_float(msg_energy, 0.0f);
  lo_message_add_float(msg_energy, 0.0f);
  lo_message_add_float(msg_energy, 0.0f);
  lo_message_add_float(msg_energy, 0.0f);
  lo_message_add_float(msg_energy, 0.0f);
  auto oscmsgargv = lo_message_get_argv(msg_energy);
  p_E1 = &(oscmsgargv[0]->f);
  p_E2 = &(oscmsgargv[1]->f);
  p_E3 = &(oscmsgargv[2]->f);
  p_E4 = &(oscmsgargv[3]->f);
  p_E5 = &(oscmsgargv[4]->f);
  if(threaded)
    thread = std::thread(&lipsync_t::sendthread, this);
}

void lipsync_t::sendthread()
{
  std::unique_lock<std::mutex> lk(mtx);
  while(run_thread) {
    cond.wait_for(lk, 100ms);
    if(has_msg_blendshapes) {
      lo_send_message(lo_addr, path_.c_str(), msg_blendshapes);
      has_msg_blendshapes = false;
    }
    if(has_msg_energy) {
      lo_send_message(lo_addr, energypath.c_str(), msg_energy);
      has_msg_energy = false;
    }
  }
}

void lipsync_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  srv->add_float("/smoothing", &smoothing);
  srv->add_float("/vocalTract", &vocalTract);
  srv->add_float("/threshold", &threshold);
  srv->add_float("/maxspeechlevel", &maxspeechlevel);
  srv->add_float("/dynamicrange", &dynamicrange);
  srv->add_bool("/active", &active);
  srv->unset_variable_owner();
}

void lipsync_t::configure()
{
  audioplugin_base_t::configure();
  // allocate FFT buffers:
  stft = new TASCAR::stft_t(2 * n_fragment, 2 * n_fragment, n_fragment,
                            TASCAR::stft_t::WND_BLACKMAN, 0);
  uint32_t num_bins(stft->s.n_);
  // allocate buffer for processed smoothed log values:
  sSmoothedMag = new float[num_bins];
  memset(sSmoothedMag, 0, num_bins * sizeof(float));
  sLogMag = new float[num_bins];
  memset(sLogMag, 0, num_bins * sizeof(float));
  // Edge frequencies for format energies:
  float freqBins[numFormants + 1];
  if(numFormants + 1 != 5)
    throw TASCAR::ErrMsg("Programming error");
  freqBins[0] = 0;
  freqBins[1] = 500 * vocalTract;
  freqBins[2] = 700 * vocalTract;
  freqBins[3] = 3000 * vocalTract;
  freqBins[4] = 6000 * vocalTract;
  formantEdges = new uint32_t[numFormants + 1];
  for(uint32_t k = 0; k < numFormants + 1; ++k)
    formantEdges[k] = std::min(
        num_bins, (uint32_t)(round(2 * freqBins[k] * (float)n_fragment /
                                   (float)f_sample)));
}

void lipsync_t::release()
{
  audioplugin_base_t::release();
  delete stft;
  delete[] sSmoothedMag;
  delete[] sLogMag;
}

lipsync_t::~lipsync_t()
{
  run_thread = false;
  if(threaded)
    thread.join();
  lo_address_free(lo_addr);
  lo_message_free(msg_blendshapes);
  lo_message_free(msg_energy);
}

void lipsync_t::ap_process(std::vector<TASCAR::wave_t>& chunk,
                           const TASCAR::pos_t&, const TASCAR::rotmat_t&,
                           const TASCAR::transport_t& tp)
{
  // Following web api doc:
  // https://webaudio.github.io/web-audio-api/#fft-windowing-and-smoothing-over-time
  // Blackman window
  // FFT
  // Smooth over time
  // Conversion to dB
  if(!chunk.size())
    return;
  stft->process(chunk[0]);
  float vmin(1e20f);
  float vmax(-1e20f);
  uint32_t num_bins(stft->s.n_);
  // calculate smooth st-PSD:
  float smoothing_c1(expf(-1.0f / (smoothing * (float)f_fragment)));
  float smoothing_c2(1.0f - smoothing_c1);
  for(uint32_t i = 0; i < num_bins; ++i) {
    // smoothing:
    sSmoothedMag[i] *= smoothing_c1;
    sSmoothedMag[i] += smoothing_c2 * std::abs(stft->s.b[i]);
    vmin = std::min(vmin, sSmoothedMag[i]);
    vmax = std::max(vmax, sSmoothedMag[i]);
  }
  float energy[numFormants];
  for(uint32_t k = 0; k < numFormants; ++k) {
    // Sum of freq values inside bin
    energy[k] = 0;
    // Sum intensity:
    for(uint32_t i = formantEdges[k]; i < formantEdges[k + 1]; ++i)
      energy[k] += sSmoothedMag[i] * sSmoothedMag[i];
    // Divide by number of sumples
    energy[k] /= (float)(formantEdges[k + 1] - formantEdges[k]);
    energy[k] =
        std::max(0.0f, (10.0f * log10f(energy[k] + 1e-6f) - maxspeechlevel) /
                               dynamicrange +
                           1.0f - threshold);
  }

  // Lipsync - Blend shape values
  float value = 0;

  // Kiss blend shape
  // When there is energy in the 3 and 4 bin, blend shape is 0
  float kissBS = 0;
  value = (0.5f - energy[2]) * 2.0f;
  if(energy[1] < 0.2f)
    value *= energy[1] * 5.0f;
  value *= scale.x;
  value = value > 1.0f ? 1.0f : value; // Clip
  value = value < 0.0f ? 0.0f : value; // Clip
  make_friendly_number(value);
  kissBS = value;

  // Jaw blend shape
  float jawB = 0;
  value = energy[1] * 0.8 - energy[3] * 0.8;
  value *= scale.y;
  value = value > 1.0 ? 1.0 : value; // Clip
  value = value < 0.0 ? 0.0 : value; // Clip
  make_friendly_number(value);
  jawB = value;

  // Lips closed blend shape
  float lipsclosedBS = 0;
  value = energy[3] * 3;
  value *= scale.z;
  value = value > 1.0 ? 1.0 : value; // Clip
  value = value < 0.0 ? 0.0 : value; // Clip
  make_friendly_number(value);
  lipsclosedBS = value;

  bool lactive(active);
  if((send_mode == transport) && (tp.rolling == false))
    lactive = false;

  // handle sending of data, forward data to sender thread:
  if(mtx.try_lock()) {
    if(lactive) {
      // if "onchange" mode then send max "onchangecount" equal messages:
      if((send_mode == onchange) && (kissBS == prev_kissBS) &&
         (jawB == prev_jawB) && (lipsclosedBS == prev_lipsclosedBS)) {
        if(onchangecounter)
          onchangecounter--;
      } else {
        onchangecounter = onchangecount;
      }
      if((send_mode != onchange) || (onchangecounter > 0)) {
        // send lipsync values to osc target:
        *p_kissBS = kissBS;
        *p_jawB = jawB;
        *p_lipsclosedBS = lipsclosedBS;
        has_msg_blendshapes = true;
        if(!energypath.empty()) {
          *p_E1 = energy[1];
          *p_E2 = energy[2];
          *p_E3 = energy[3];
          *p_E4 = 20.0f * log10f(vmin + 1e-6f);
          *p_E5 = 20.0f * log10f(vmax + 1e-6f);
          has_msg_energy = true;
        }
      }
    } else {
      if(was_active) {
        *p_kissBS = 0.0f;
        *p_jawB = 0.0f;
        *p_lipsclosedBS = 0.0f;
        has_msg_blendshapes = true;
      }
    }
    was_active = lactive;
    prev_kissBS = kissBS;
    prev_jawB = jawB;
    prev_lipsclosedBS = lipsclosedBS;
    mtx.unlock();
  }
  if(threaded) {
    cond.notify_one();
  } else {
    if(has_msg_blendshapes) {
      lo_send_message(lo_addr, path_.c_str(), msg_blendshapes);
      has_msg_blendshapes = false;
    }
    if(has_msg_energy) {
      lo_send_message(lo_addr, energypath.c_str(), msg_energy);
      has_msg_energy = false;
    }
  }
}

REGISTER_AUDIOPLUGIN(lipsync_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
