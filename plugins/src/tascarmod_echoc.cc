/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2022 Giso Grimm
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
 * For reference, see:
 *
 * Spriet, A., Proudler, I., Moonen, M. & Wouters, J. (2005). Adaptive
 * feedback cancellation in hearing aids with linear prediction of the
 * desired signal. IEEE Transactions on Signal Processing, 53(10),
 * 3749–3763.
 */

#include "jackclient.h"
#include "jackiowav.h"
#include "mutex"
#include "ola.h"
#include "session.h"

// helper function to find index of sample with largest absolute value
// of an impulse response 'ir'
uint32_t get_idxmaxabs(const TASCAR::wave_t& ir)
{
  uint32_t imax = 0u;
  float vmax = 0.0f;
  float tmp = 0.0f;
  for(uint32_t k = 0; k < ir.n; ++k)
    if((tmp = fabsf(ir.d[k])) > vmax) {
      vmax = tmp;
      imax = k;
    }
  return imax;
}

class blms_proc_t : public TASCAR::overlap_save_t {
public:
  blms_proc_t(uint32_t irslen, uint32_t chunksize, uint32_t delay);
  void adapt(const TASCAR::wave_t&, const TASCAR::wave_t&,
             const TASCAR::wave_t&);
  TASCAR::static_delay_t delayline;

private:
  // container for error signal:
  TASCAR::wave_t w_e;
  // container for full length error and u signal:
  TASCAR::wave_t w_e_long;
  TASCAR::wave_t w_u_long;
};

blms_proc_t::blms_proc_t(uint32_t irslen, uint32_t chunksize, uint32_t delay)
    : TASCAR::overlap_save_t(irslen, chunksize), delayline(delay),
      w_e(chunksize), w_e_long(get_fftlen()), w_u_long(get_fftlen())
{
}

void blms_proc_t::adapt(const TASCAR::wave_t& w_u, const TASCAR::wave_t& w_out,
                        const TASCAR::wave_t& w_adapt)
{
  w_e.copy(w_out);
  w_e += w_adapt;
  // apply whitening filter on w_e and w_u
  // Fourier-transform of w_e and w_u
  // update filter based on E{s_e * conj(s_u)}
}

class echoc_var_t : public TASCAR::module_base_t {
public:
  echoc_var_t(const TASCAR::module_cfg_t& cfg);
  std::string name = "echoc";
  std::string path = "";
  std::vector<std::string> micports = {"system:capture_1"};
  std::vector<std::string> loudspeakerports = {"system:playback_1",
                                               "system:playback_2"};
  float maxdist = 2.0f;
  uint32_t nrep = 16;
  float level = 70;
  uint32_t filterlen = 65;
  uint32_t premax = 8;
  bool measureatstart = false;
  bool autoreconnect = false;
  bool bypass = false;
  bool adaptive = false;
};

echoc_var_t::echoc_var_t(const TASCAR::module_cfg_t& cfg) : module_base_t(cfg)
{
  GET_ATTRIBUTE(name, "", "Client name, used for jack and IR file name");
  GET_ATTRIBUTE(micports, "", "Microphone ports");
  GET_ATTRIBUTE(loudspeakerports, "", "Loudspeaker ports");
  GET_ATTRIBUTE(maxdist, "m",
                "Maximum distance between microphone and loudspeaker");
  GET_ATTRIBUTE(level, "dB SPL", "Playback level");
  GET_ATTRIBUTE(nrep, "", "Number of measurement repetitions");
  GET_ATTRIBUTE(filterlen, "samples", "Minimal length of filters");
  GET_ATTRIBUTE(premax, "samples", "Time before IR maximum to add to filter");
  GET_ATTRIBUTE_BOOL(measureatstart,
                     "Perform a measurement when the plugin is loaded");
  GET_ATTRIBUTE_BOOL(autoreconnect,
                     "Automatically re-connect ports after jack port change");
  GET_ATTRIBUTE_BOOL(bypass, "Bypass filter stage");
  GET_ATTRIBUTE_BOOL(adaptive, "Use adaptive filtering");
  if(micports.empty())
    throw TASCAR::ErrMsg(
        "At least one microphone (filter output) port must be given");
  if(loudspeakerports.empty())
    throw TASCAR::ErrMsg(
        "At least one loudspeaker (filter input) port must be given");
}

class echoc_mod_t : public echoc_var_t, public jackc_t {
public:
  echoc_mod_t(const TASCAR::module_cfg_t& cfg);
  void configure();
  void ir_measure();
  void ir_update_from_file_and_truncate();
  void ports_connect();
  virtual ~echoc_mod_t();
  int process(jack_nframes_t nframes, const std::vector<float*>& inBuffer,
              const std::vector<float*>& outBuffer);
  void add_variables(TASCAR::osc_server_t* srv);
  static int osc_measure(const char*, const char*, lo_arg**, int, lo_message,
                         void* user_data)
  {
    ((echoc_mod_t*)user_data)->ir_measure();
    ((echoc_mod_t*)user_data)->ir_update_from_file_and_truncate();
    return 0;
  }
  static int osc_connect(const char*, const char*, lo_arg**, int, lo_message,
                         void* user_data)
  {
    ((echoc_mod_t*)user_data)->ports_connect();
    return 0;
  }
  static void jack_port_connect_cb(jack_port_id_t a, jack_port_id_t b,
                                   int connect, void* arg);
  void jack_port_connect_cb();

private:
  void port_service();
  bool run_port_service = true;
  std::thread port_thread;
  std::mutex lock;
  std::vector<blms_proc_t*> flt_hat_H;
  //  temporary storage for delayed signal:
  TASCAR::wave_t w_u_delayed;
  std::atomic_bool connecting_ports = false;
  std::atomic_bool reconnect = false;
  std::atomic_bool measuring = false;
  const size_t N_in = 0;
  const size_t N_out = 0;
  const size_t N_flt = 0;
};

echoc_mod_t::echoc_mod_t(const TASCAR::module_cfg_t& cfg)
    : echoc_var_t(cfg), jackc_t(name), N_in(loudspeakerports.size()),
      N_out(micports.size()), N_flt(N_in * N_out)
{
  // create output ports which will send the phase-inverted signal, to
  // be added to the microphone signals by jack:
  for(size_t ch = 0; ch < micports.size(); ++ch)
    add_output_port("out." + std::to_string(ch));
  // create input ports which receive a copy of the loudspeaker
  // signal:
  for(size_t ch = 0; ch < loudspeakerports.size(); ++ch)
    add_input_port("in." + std::to_string(ch));
  // for adaptation, add a copy of the microphone signals before
  // addition of the phase inverted copy:
  for(size_t ch = 0; ch < micports.size(); ++ch)
    add_input_port("adapt." + std::to_string(ch));
  if(autoreconnect) {
    // update port connections upon any jack port change:
    jack_set_port_connect_callback(jc, &echoc_mod_t::jack_port_connect_cb,
                                   this);
  }
  activate();
  ports_connect();
  add_variables(session);
  if(autoreconnect) {
    port_thread = std::thread(&echoc_mod_t::port_service, this);
  }
}

void echoc_mod_t::jack_port_connect_cb(jack_port_id_t, jack_port_id_t, int,
                                       void* arg)
{
  ((echoc_mod_t*)arg)->jack_port_connect_cb();
}

void echoc_mod_t::jack_port_connect_cb()
{
  if(!connecting_ports)
    reconnect = true;
}

void echoc_mod_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  std::string prefix_(srv->get_prefix());
  srv->set_prefix(std::string("/") + name);
  srv->add_method("/measure", "", &echoc_mod_t::osc_measure, this);
  srv->add_method("/connect", "", &echoc_mod_t::osc_connect, this);
  srv->add_bool("/bypass", &bypass);
  srv->set_prefix(prefix_);
  srv->unset_variable_owner();
}

void echoc_mod_t::ports_connect()
{
  connecting_ports = true;
  for(size_t ch = 0; ch < micports.size(); ++ch)
    disconnect_out(ch);
  for(size_t ch = 0; ch < loudspeakerports.size() + micports.size(); ++ch)
    disconnect_in(ch);
  for(size_t ch = 0; ch < micports.size(); ++ch)
    connect_out(ch, micports[ch], true, true, true);
  for(size_t ch = 0; ch < loudspeakerports.size(); ++ch)
    connect_in(ch, loudspeakerports[ch], true, true, true);
  for(size_t ch = loudspeakerports.size();
      ch < loudspeakerports.size() + micports.size(); ++ch)
    connect_in(ch, micports[ch - loudspeakerports.size()], true, false, true);
  connecting_ports = false;
}

void echoc_mod_t::configure()
{
  {
    std::lock_guard<std::mutex> lockguard(lock);
    // temporary storage for delayed signal:
    w_u_delayed.resize(n_fragment);
  }
  //
  if(measureatstart)
    ir_measure();
  ir_update_from_file_and_truncate();
  ports_connect();
  reconnect = true;
}

// periodically reconnect ports:
void echoc_mod_t::port_service()
{
  size_t pcnt = 100;
  while(run_port_service) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if(!measuring) {
      if(reconnect) {
        if(pcnt)
          pcnt--;
        else {
          pcnt = 100;
          reconnect = false;
          ports_connect();
        }
      }
    }
  }
}

echoc_mod_t::~echoc_mod_t()
{
  run_port_service = false;
  deactivate();
  // clear all filters and delays:
  for(auto& obj : flt_hat_H)
    delete obj;
  flt_hat_H.clear();
  if(port_thread.joinable())
    port_thread.join();
}

void echoc_mod_t::ir_update_from_file_and_truncate()
{
  std::lock_guard<std::mutex> lockguard(lock);
  // clear all filters and delays:
  for(auto& obj : flt_hat_H)
    delete obj;
  flt_hat_H.clear();
  if(n_fragment == 0)
    return;
  auto fftlen = pow(2.0, ceil(log2(n_fragment + filterlen - 1)));
  auto filterlen_final = fftlen - n_fragment + 1;
  // load recorded IR:
  float fs = 0;
  try {
    auto all_ir =
        TASCAR::audioread(TASCAR::env_expand(path + name + ".wav"), fs);
    if(fs != f_sample)
      TASCAR::add_warning(
          "Invalid sampling rate of impulse response (expected " +
          TASCAR::to_string(f_sample) + " Hz, got " + TASCAR::to_string(fs) +
          " Hz).");
    if(all_ir.size() != N_in * N_out)
      TASCAR::add_warning(
          "Not the same number of channels in pre-stored impulse response as "
          "number of inputs * number of outputs");
    // find maxima and measurement quality:
    std::vector<uint32_t> idxmax;
    std::vector<float> aratio;
    size_t ch = 0;
    TASCAR::wave_t filterir(filterlen_final);
    for(const auto& ir : all_ir) {
      idxmax.push_back(get_idxmaxabs(ir));
      float r = 0.0f;
      for(uint32_t k = 0; k <= idxmax.back(); ++k)
        r += ir.d[k] * ir.d[k];
      r = sqrtf(r);
      aratio.push_back(fabsf(ir.d[idxmax.back()]) / r);
      if(aratio.back() < 0.5)
        TASCAR::add_warning(
            "echoc: Poor IR measurement quality in channel " +
            std::to_string(ch) +
            TASCAR::to_string(100.0f * aratio.back(), " (%1.0f%%)"));
      ++ch;
      uint32_t predelay = std::max(idxmax.back(), premax) - premax;
      DEBUG(predelay);
      filterir.clear();
      for(uint32_t k = 0; k < filterir.n; ++k)
        filterir.d[k] = -ir.d[k + predelay];
      flt_hat_H.push_back(
          new blms_proc_t(filterlen_final, n_fragment, predelay));
      flt_hat_H.back()->set_irs(filterir);
    }
  }
  catch(const std::exception& ex) {
    TASCAR::add_warning(std::string("In plugin echoc (") +
                        tsccfg::node_get_path(e) + "): " + ex.what());
  }
}

// measure impulse responses using a sweep:
void echoc_mod_t::ir_measure()
{
  measuring = true;
  std::lock_guard<std::mutex> lockguard(lock);
  // IR length magic:
  // 10 ms for AD/DA and aliasing filters
  // 4 fragment sizes for block processing and poor sound card design
  // maxdist at typical speed of sound
  // power of 2 for efficiency
  size_t irlen = pow(2.0, ceil(log2(0.01 * f_sample + 4.0 * n_fragment +
                                    maxdist / 340 * f_sample + filterlen)));
  // create jack client with number of micports inputs and one output:
  std::vector<TASCAR::wave_t> isig = {TASCAR::wave_t(irlen * (nrep + 1))};
  TASCAR::fft_t fft(irlen);
  TASCAR::fft_t fft_y(irlen);
  // generate exponential sweep:
  const std::complex<float> If = {0.0f, 1.0f};
  for(size_t k = 0; k < fft.s.n_; k++)
    fft.s[k] = std::exp(-If * TASCAR_2PIf * (float)irlen *
                        std::pow((float)k / (float)(fft.s.n_), 2.0f)) *
               1.0f / sqrtf(irlen);
  fft.ifft();
  // scale to desired signal level:
  fft.w *= TASCAR::db2lin(level - fft.w.spldb());
  // repeat stimulus for averaging:
  for(size_t k = 0; k < nrep + 1; ++k)
    isig[0].append(fft.w);
  fft.fft();
  // create output signals:
  std::vector<TASCAR::wave_t> osig;
  for(size_t ch = 0; ch < micports.size(); ++ch)
    osig.push_back(TASCAR::wave_t(irlen * (nrep + 1)));
  // storage for impulse responses:
  std::vector<TASCAR::wave_t> all_ir;
  float fs = f_sample;
  for(auto& port : loudspeakerports) {
    std::vector<TASCAR::wave_t> ir(osig.size(), TASCAR::wave_t(irlen));
    std::vector<std::string> ports = {port};
    ports.insert(ports.end(), micports.begin(), micports.end());
    // create an audio recorder instance, and record signals:
    jackio_t jio(isig, osig, ports, name + "irrecorder", 0, false);
    jio.run();
    // process recordings:
    for(size_t ch = 0; ch < osig.size(); ++ch) {
      fft_y.w.clear();
      // take average of response over repetitions:
      for(size_t rep = 1; rep < nrep; ++rep)
        for(size_t k = 0; k < irlen; ++k)
          fft_y.w.d[k] += osig[ch].d[irlen * rep + k];
      fft_y.w *= 1.0f / nrep;
      // apply Fourier transform:
      fft_y.fft();
      // calculate impulse response by element-wise division by
      // reference signal:
      for(size_t bin = 0; bin < fft_y.s.n_; ++bin)
        fft_y.s.b[bin] /= fft.s.b[bin];
      fft_y.ifft();
      // copy to impulse response container:
      ir[ch].copy(fft_y.w);
    }
    all_ir.insert(all_ir.end(), ir.begin(), ir.end());
  }
  // store all impulse responses on disk:
  TASCAR::audiowrite(TASCAR::env_expand(path + name + ".wav"), all_ir, fs,
                     SF_FORMAT_WAV | SF_FORMAT_FLOAT | SF_ENDIAN_FILE);
  measuring = false;
}

int echoc_mod_t::process(jack_nframes_t nframes,
                         const std::vector<float*>& inBuffer,
                         const std::vector<float*>& outBuffer)
{
  // clear output samples:
  for(auto pOut : outBuffer)
    memset(pOut, 0, sizeof(float) * nframes);
  if(!bypass) {
    if(lock.try_lock()) {
      size_t idx = 0;
      uint32_t cin = 0;
      for(auto pIn : inBuffer) {
        if(cin < loudspeakerports.size()) {
          // input port, not a filter update port.
          uint32_t cout = 0;
          for(auto pOut : outBuffer) {
            if(idx < flt_hat_H.size()) {
              // create a delayed copy of the input signal:
              for(uint32_t k = 0; k < nframes; ++k)
                w_u_delayed.d[k] = flt_hat_H[idx]->delayline(pIn[k]);
              TASCAR::wave_t w_out(nframes, pOut);
              // filter signal and store in pOut (referenced by w_out):
              flt_hat_H[idx]->process(w_u_delayed, w_out);
              if(adaptive) {
                TASCAR::wave_t w_adapt(nframes, pIn + loudspeakerports.size());
                flt_hat_H[idx]->adapt(w_u_delayed, w_out, w_adapt);
              }
            }
            ++idx;
            ++cout;
          }
        }
        ++cin;
      }
      lock.unlock();
    }
  }
  return 0;
}

REGISTER_MODULE(echoc_mod_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
